#include "Bot.h"

#include "BotHelper.h"
#include "BotPlaner.h"
#include "GameMechanic.h"
#include "Proto.h"
#include "TeakLibW.h"
#include "class.h"
#include "defines.h"
#include "global.h"
#include "helper.h"

#include <SDL_log.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <utility>
#include <vector>

template <class... Types> void AT_Error(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_ERROR, "Bot", args...); }
template <class... Types> void AT_Warn(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_WARN, "Bot", args...); }
template <class... Types> void AT_Info(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_INFO, "Bot", args...); }
template <class... Types> void AT_Log(Types... args) { AT_Log_I("Bot", args...); }

// #define PRINT_ROUTE_DETAILS 1

// Preise verstehen sich pro Sitzplatz:
extern SLONG SeatCosts[];
extern SLONG FoodCosts[];
extern SLONG TrayCosts[];
extern SLONG DecoCosts[];

// Preise pro Flugzeuge:
extern SLONG TriebwerkCosts[];
extern SLONG ReifenCosts[];
extern SLONG ElektronikCosts[];
extern SLONG SicherheitCosts[];

template <typename T> inline bool eraseFirst(T &l, SLONG val) {
    auto it = l.begin();
    while (it != l.end() && *it != val) {
        it++;
    }
    if (it != l.end()) {
        std::iter_swap(it, l.end() - 1);
        l.resize(l.size() - 1);
        return true;
    }
    return false;
}

void Bot::actionStartDay(__int64 moneyAvailable) {
    actionStartDayLaptop(moneyAvailable);

    /* always use tanks: We get discount from advisor and by using cheap kerosine */
    GameMechanic::setKerosinTankOpen(qPlayer, true);
}

void Bot::actionStartDayLaptop(__int64 moneyAvailable) {
    mDayStarted = true;

    /*  invalidate cached info */
    mRoutesUpdated = false;
    mRoutesUtilizationUpdated = false;
    mRoutesNextStep = RoutesNextStep::None;
    mExtraPilots = -1;
    mExtraBegleiter = -1;

    mArabHintsTracker -= std::min(3, mArabHintsTracker);

    /* check lists of planes, check which planes are available for service and which are not */
    checkPlaneLists();

    /* check routes */
    if (mDoRoutes) {
        checkLostRoutes();
        updateRouteInfoOffice();
        requestPlanRoutes(true);
    } else if (qPlayer.RobotUse(ROBOT_USE_ROUTES) && (getNumRentedRoutes() == 0)) {
        /* logic for switching to routes. Before switching, make sure any initially rented routes have been cancelled */
        if (qPlayer.RobotUse(ROBOT_USE_FORCEROUTES)) {
            mDoRoutes = true;
            AT_Log("Bot::RobotInit(): Switching to routes (forced).");
        } else if (mBestPlaneTypeId != -1) {
            const auto &bestPlaneType = PlaneTypes[mBestPlaneTypeId];
            SLONG costRouteAd = gWerbePrice[1 * 6 + 5];
            __int64 moneyNeeded = 2 * costRouteAd + bestPlaneType.Preis;
            SLONG numPlanes = mPlanesForJobs.size() + mPlanesForJobsUnassigned.size();
            if ((numPlanes >= mOptions.kSwitchToRoutesNumPlanesMin && moneyAvailable >= moneyNeeded) || (numPlanes >= mOptions.kSwitchToRoutesNumPlanesMax)) {
                mDoRoutes = true;
                AT_Log("Bot::actionStartDay(): Switching to routes. Reserving 2*%d + %d for ads and plane.", costRouteAd, bestPlaneType.Preis);
            }
        }
    }
    if (mDoRoutes && !mLongTermStrategy) {
        mLongTermStrategy = true;
        AT_Log("Bot::actionStartDay(): Switching to longterm strategy.");
    }

    /* logic deciding when to switch to final target run */
    determineNemesis();
    switchToFinalTarget();

    /* update how much kerosine was used */
    assert(mKerosineLevelLastChecked >= qPlayer.TankInhalt);
    mKerosineUsedTodaySoFar += (mKerosineLevelLastChecked - qPlayer.TankInhalt);
    mKerosineLevelLastChecked = qPlayer.TankInhalt;
    mTankRatioEmptiedYesterday = 1.0 * mKerosineUsedTodaySoFar / qPlayer.Tank;
    mKerosineUsedTodaySoFar = 0;

    /* starting jobs (check every day because of mission DIFF_ADDON09) */
    SLONG numToPlan = 0;
    for (int i = 0; i < qPlayer.Auftraege.AnzEntries(); i++) {
        if (qPlayer.Auftraege.IsInAlbum(i) == 0) {
            continue;
        }
        if (qPlayer.Auftraege[i].InPlan == 0) {
            numToPlan++;
        }
    }
    if (numToPlan > 0) {
        AT_Log("Bot::RobotInit(): Have %d jobs to plan", numToPlan);
        BotPlaner planer(qPlayer, qPlayer.Planes);
        grabFlights(planer, true);
    }

    /* some conditions might have changed (plane availability) */
    forceReplanning();
}

void Bot::actionBuero() {
    if (mNeedToPlanJobs) {
        planFlights();
    }
    if (mDoRoutes) {
        updateRouteInfoOffice();
        assignPlanesToRoutes(true);
        if (mNeedToPlanRoutes) {
            planRoutes();
        }
    }
}

void Bot::actionCallInternational(bool areWeInOffice) {
    std::vector<SLONG> cities;
    for (SLONG n = 0; n < Cities.AnzEntries(); n++) {
        if (!GameMechanic::canCallInternational(qPlayer, n)) {
            continue;
        }

        GameMechanic::refillFlightJobs(n);
        cities.push_back(n);
    }

    AT_Log("Bot::actionCallInternational(): There are %u cities we can call.", cities.size());
    if (!cities.empty()) {
        BotPlaner planer(qPlayer, qPlayer.Planes);
        planer.addJobSource(BotPlaner::JobOwner::International, cities);
        planer.addJobSource(BotPlaner::JobOwner::InternationalFreight, cities);
        grabFlights(planer, areWeInOffice);

        SLONG cost = cities.size();
        qPlayer.History.AddCallCost(cost);
        qPlayer.Money -= cost;
        qPlayer.Statistiken[STAT_A_SONSTIGES].AddAtPastDay(-cost);
        qPlayer.Bilanz.SonstigeAusgaben -= cost;
    }
}

void Bot::actionCheckLastMinute() {
    LastMinuteAuftraege.RefillForLastMinute();

    BotPlaner planer(qPlayer, qPlayer.Planes);
    planer.addJobSource(BotPlaner::JobOwner::LastMinute, {});
    grabFlights(planer, false);

    LastMinuteAuftraege.RefillForLastMinute();
}

void Bot::actionCheckTravelAgency() {
    if (mItemAntiVirus == 0) {
        if (GameMechanic::PickUpItemResult::PickedUp == GameMechanic::pickUpItem(qPlayer, ITEM_SPINNE)) {
            AT_Log("Bot::actionCheckTravelAgency(): Picked up item tarantula");
            mItemAntiVirus = 1;
        }
        if (HowToPlan::None == howToPlanFlights()) {
            return; /* avoid warning in grabFlights(). We only came here for the item */
        }
    }

    ReisebueroAuftraege.RefillForReisebuero();

    BotPlaner planer(qPlayer, qPlayer.Planes);
    planer.addJobSource(BotPlaner::JobOwner::TravelAgency, {});
    grabFlights(planer, false);

    ReisebueroAuftraege.RefillForReisebuero();
}

void Bot::actionCheckFreightDepot() {
    gFrachten.Refill();

    BotPlaner planer(qPlayer, qPlayer.Planes);
    planer.addJobSource(BotPlaner::JobOwner::Freight, {});
    grabFlights(planer, false);

    gFrachten.Refill();
}

void Bot::actionUpgradePlanes() {
    /* which planes to upgrade */
    std::vector<SLONG> planes = mPlanesForRoutes;
    if ((mRunToFinalObjective == FinalPhase::TargetRun) && qPlayer.RobotUse(ROBOT_USE_LUXERY)) {
        planes = getAllPlanes();
    }

    bool onlySecurity = (Sim.Difficulty == DIFF_ATFS02) && (mRunToFinalObjective == FinalPhase::TargetRun);

    /* cancel all currently planned plane ugprades */
    mMoneyReservedForUpgrades = 0;
    for (auto planeId : planes) {
        auto &qPlane = qPlayer.Planes[planeId];

        qPlane.SitzeTarget = qPlane.Sitze;
        qPlane.TablettsTarget = qPlane.Tabletts;
        qPlane.DecoTarget = qPlane.Deco;
        qPlane.ReifenTarget = qPlane.Reifen;
        qPlane.TriebwerkTarget = qPlane.Triebwerk;
        qPlane.SicherheitTarget = qPlane.Sicherheit;
        qPlane.ElektronikTarget = qPlane.Elektronik;
    }

    if (planes.empty()) {
        return;
    }

    /* plan new plane ugprades until we run out of money */
    auto randOffset = LocalRandom.Rand(planes.size());
    for (SLONG upgradeWhat = 0; upgradeWhat < 7; upgradeWhat++) {
        auto moneyAvailable = getMoneyAvailable() - kMoneyReservePlaneUpgrades;
        if (moneyAvailable < 0) {
            break;
        }

        for (SLONG c = 0; c < planes.size(); c++) {
            SLONG idx = (c + randOffset) % planes.size();
            CPlane &qPlane = qPlayer.Planes[planes[idx]];
            auto ptPassagiere = qPlane.ptPassagiere;

            moneyAvailable = getMoneyAvailable() - kMoneyReservePlaneUpgrades;
            if (moneyAvailable < 0) {
                break;
            }

            SLONG cost = 0;
            switch (upgradeWhat) {
            case 0:
                cost = ptPassagiere * (SeatCosts[2] - SeatCosts[qPlane.Sitze] / 2);
                if (qPlane.SitzeTarget < 2 && cost <= moneyAvailable && !onlySecurity) {
                    qPlane.SitzeTarget = 2;
                    mMoneyReservedForUpgrades += cost;
                    AT_Log("Bot::actionUpgradePlanes(): Upgrading seats in %s.", Helper::getPlaneName(qPlane).c_str());
                }
                break;
            case 1:
                cost = ptPassagiere * (TrayCosts[2] - TrayCosts[qPlane.Tabletts] / 2);
                if (qPlane.TablettsTarget < 2 && cost <= moneyAvailable && !onlySecurity) {
                    qPlane.TablettsTarget = 2;
                    mMoneyReservedForUpgrades += cost;
                    AT_Log("Bot::actionUpgradePlanes(): Upgrading tabletts in %s.", Helper::getPlaneName(qPlane).c_str());
                }
                break;
            case 2:
                cost = ptPassagiere * (DecoCosts[2] - DecoCosts[qPlane.Deco] / 2);
                if (qPlane.DecoTarget < 2 && cost <= moneyAvailable && !onlySecurity) {
                    qPlane.DecoTarget = 2;
                    mMoneyReservedForUpgrades += cost;
                    AT_Log("Bot::actionUpgradePlanes(): Upgrading deco in %s.", Helper::getPlaneName(qPlane).c_str());
                }
                break;
            case 3:
                cost = (ReifenCosts[2] - ReifenCosts[qPlane.Reifen] / 2);
                if (qPlane.ReifenTarget < 2 && cost <= moneyAvailable) {
                    qPlane.ReifenTarget = 2;
                    mMoneyReservedForUpgrades += cost;
                    AT_Log("Bot::actionUpgradePlanes(): Upgrading tires in %s.", Helper::getPlaneName(qPlane).c_str());
                }
                break;
            case 4:
                cost = (TriebwerkCosts[2] - TriebwerkCosts[qPlane.Triebwerk] / 2);
                if (qPlane.TriebwerkTarget < 2 && cost <= moneyAvailable) {
                    qPlane.TriebwerkTarget = 2;
                    mMoneyReservedForUpgrades += cost;
                    AT_Log("Bot::actionUpgradePlanes(): Upgrading engines in %s.", Helper::getPlaneName(qPlane).c_str());
                }
                break;
            case 5:
                cost = (SicherheitCosts[2] - SicherheitCosts[qPlane.Sicherheit] / 2);
                if (qPlane.SicherheitTarget < 2 && cost <= moneyAvailable) {
                    qPlane.SicherheitTarget = 2;
                    mMoneyReservedForUpgrades += cost;
                    AT_Log("Bot::actionUpgradePlanes(): Upgrading safety in %s.", Helper::getPlaneName(qPlane).c_str());
                }
                break;
            case 6:
                cost = (ElektronikCosts[2] - ElektronikCosts[qPlane.Elektronik] / 2);
                if (qPlane.ElektronikTarget < 2 && cost <= moneyAvailable) {
                    qPlane.ElektronikTarget = 2;
                    mMoneyReservedForUpgrades += cost;
                    AT_Log("Bot::actionUpgradePlanes(): Upgrading electronics in %s.", Helper::getPlaneName(qPlane).c_str());
                }
                break;
            default:
                AT_Error("Bot::actionUpgradePlanes(): Default case should not be reached.");
                DebugBreak();
            }
        }
    }
    AT_Log("Bot::actionUpgradePlanes(): We are reserving %s $ for plane upgrades, available money: %s $", Insert1000erDots64(mMoneyReservedForUpgrades).c_str(),
           Insert1000erDots64(getMoneyAvailable()).c_str());

    mRoutesNextStep = RoutesNextStep::None;
}

void Bot::updateExtraWorkers() {
    if (qPlayer.HasBerater(BERATERTYP_PERSONAL) > 0) {
        mExtraPilots = qPlayer.xPiloten;
        mExtraBegleiter = qPlayer.xBegleiter;
    } else {
        mExtraPilots = -1;
        mExtraBegleiter = -1;
    }
    AT_Log("Bot::updateExtraWorkers(): We have %d extra pilots and %d extra attendants", mExtraPilots, mExtraBegleiter);
}

void Bot::actionBuyNewPlane(__int64 /*moneyAvailable*/) {
    if (mItemAntiStrike == 0 && (LocalRandom.Rand() % 2 == 0)) { /* rand() because human player has same chance of item appearing */
        if (GameMechanic::PickUpItemResult::PickedUp == GameMechanic::pickUpItem(qPlayer, ITEM_BH)) {
            AT_Log("Bot::actionBuyNewPlane(): Picked up item BH");
            mItemAntiStrike = 1;
        }
    }

    SLONG bestPlaneTypeId = mDoRoutes ? mBuyPlaneForRouteId : mBestPlaneTypeId;
    if (bestPlaneTypeId < 0) {
        AT_Error("Bot::actionBuyNewPlane(): No plane was selected!");
        return;
    }
    if (qPlayer.xPiloten < PlaneTypes[bestPlaneTypeId].AnzPiloten || qPlayer.xBegleiter < PlaneTypes[bestPlaneTypeId].AnzBegleiter) {
        AT_Error("Bot::actionBuyNewPlane(): Not enough crew for selected plane!");
    }

    auto list = GameMechanic::buyPlane(qPlayer, bestPlaneTypeId, 1);
    if (list.empty()) {
        AT_Error("Bot::actionBuyNewPlane(): Gamemechanic returned error!");
        return;
    }
    assert(list.size() == 1);
    assert(list[0] >= 0x1000000);

    auto planeId = list[0];
    auto &qPlane = qPlayer.Planes[planeId];
    qPlane.MaxBegleiter = qPlane.ptAnzBegleiter;
    AT_Log("Bot::actionBuyNewPlane(): Bought plane %s", Helper::getPlaneName(qPlane).c_str());
    if (mDoRoutes) {
        if (mRoutesNextStep == RoutesNextStep::BuyMorePlanes) {
            assert(mImproveRouteId != -1);
            auto &qRoute = mRoutes[mImproveRouteId];
            qRoute.planeIds.push_back(planeId);
            mPlanesForRoutes.push_back(planeId);
            AT_Log("Bot::actionBuyNewPlane(): Assigning new plane %s to route %s", Helper::getPlaneName(qPlane).c_str(),
                   Helper::getRouteName(getRoute(qRoute)).c_str());
            mRoutesNextStep = RoutesNextStep::None;
        } else {
            mPlanesForRoutesUnassigned.push_back(planeId);
        }
        requestPlanRoutes(false);
        mBuyPlaneForRouteId = -1;
    } else {
        if (checkPlaneAvailable(planeId, true, false)) {
            mPlanesForJobs.push_back(planeId);
            grabNewFlights();
        } else {
            mPlanesForJobsUnassigned.push_back(planeId);
        }
        mBestPlaneTypeId = -1;
    }

    updateExtraWorkers();
}

void Bot::actionBuyUsedPlane(__int64 /*moneyAvailable*/) {
    if (mBestUsedPlaneIdx < 0) {
        AT_Error("Bot::actionBuyUsedPlane(): We have not yet checked which plane to buy!");
        return;
    }

    if (qPlayer.xPiloten < Sim.UsedPlanes[mBestUsedPlaneIdx].ptAnzPiloten || qPlayer.xBegleiter < Sim.UsedPlanes[mBestUsedPlaneIdx].ptAnzBegleiter) {
        AT_Error("Bot::actionBuyUsedPlane(): Not enough crew for selected plane!");
    }

    SLONG planeId = GameMechanic::buyUsedPlane(qPlayer, mBestUsedPlaneIdx);
    assert(planeId >= 0x1000000);

    auto &qPlane = qPlayer.Planes[planeId];
    qPlane.MaxBegleiter = qPlane.ptAnzBegleiter;
    AT_Log("Bot::actionBuyUsedPlane(): Bought used plane %s", Helper::getPlaneName(qPlane).c_str());
    AT_Log("Bot::actionBuyUsedPlane(): Passengers = %d, fuel = %d, year = %d", qPlane.ptPassagiere, qPlane.ptVerbrauch, qPlane.Baujahr);
    SLONG improvementNeeded = std::max(0, 80 - qPlane.WorstZustand);
    SLONG repairCost = improvementNeeded * (qPlane.ptPreis / 110);
    AT_Log("Bot::actionBuyUsedPlane(): WorstZustand = %u, cost = %d", qPlane.WorstZustand, repairCost);

    /* we always repair used planes first */
    mPlanesForJobsUnassigned.push_back(planeId);

    updateExtraWorkers();
}

void Bot::actionMuseumCheckPlanes() {
    SLONG bestIdx = -1;
    DOUBLE bestScore = kUsedPlaneMinimumScore;
    for (SLONG c = 0; c < 3; c++) {
        const auto &qPlane = Sim.UsedPlanes[0x1000000 + c];
        if (qPlane.Name.GetLength() <= 0) {
            continue;
        }

        DOUBLE score = 1.0 * 1e9; /* multiplication (geometric mean) because values have wildly different ranges */
        score *= qPlane.ptPassagiere * qPlane.ptPassagiere;
        score /= qPlane.ptVerbrauch;
        score /= (2015 - qPlane.Baujahr);

        auto worstZustand = qPlane.Zustand - 20;
        SLONG improvementNeeded = std::max(0, 80 - worstZustand);
        SLONG repairCost = improvementNeeded * (qPlane.ptPreis / 110);
        if (qPlayer.HasBerater(BERATERTYP_FLUGZEUG) > 0) {
            score /= repairCost;
        }

        AT_Log("Bot::actionMuseumCheckPlanes(): Used plane %s has score %.2f", Helper::getPlaneName(qPlane).c_str(), score);
        AT_Log("\t\tPassengers = %d, fuel = %d, year = %d", qPlane.ptPassagiere, qPlane.ptVerbrauch, qPlane.Baujahr);
        if (qPlayer.HasBerater(BERATERTYP_FLUGZEUG) > 0) {
            AT_Log("\t\tWorstZustand = %u, cost = %d", worstZustand, repairCost);
        }

        if (score > bestScore) {
            bestScore = score;
            bestIdx = c;
        }
    }
    mBestUsedPlaneIdx = bestIdx;
}

void Bot::actionBuyDesignerPlane(__int64 /*moneyAvailable*/) {
    if (mDesignerPlane.Name.empty() || !mDesignerPlane.IsBuildable()) {
        AT_Error("Bot::actionBuyDesignerPlane(): No designer plane!");
        return;
    }

    if (qPlayer.xPiloten < mDesignerPlane.CalcPiloten() || qPlayer.xBegleiter < mDesignerPlane.CalcBegleiter()) {
        AT_Error("Bot::actionBuyDesignerPlane(): Not enough crew for selected plane!");
    }

    auto list = GameMechanic::buyXPlane(qPlayer, mDesignerPlaneFile, 1);
    if (list.empty()) {
        AT_Error("Bot::actionBuyDesignerPlane(): Gamemechanic returned error!");
        return;
    }
    assert(list.size() == 1);
    assert(list[0] >= 0x1000000);

    auto planeId = list[0];
    auto &qPlane = qPlayer.Planes[planeId];
    qPlane.MaxBegleiter = qPlane.ptAnzBegleiter;
    AT_Log("Bot::actionBuyDesignerPlane(): Bought plane %s", Helper::getPlaneName(qPlane).c_str());
    if (mDoRoutes) {
        if (mRoutesNextStep == RoutesNextStep::BuyMorePlanes) {
            assert(mImproveRouteId != -1);
            auto &qRoute = mRoutes[mImproveRouteId];
            qRoute.planeIds.push_back(planeId);
            mPlanesForRoutes.push_back(planeId);
            AT_Log("Bot::actionBuyDesignerPlane(): Assigning new plane %s to route %s", Helper::getPlaneName(qPlane).c_str(),
                   Helper::getRouteName(getRoute(qRoute)).c_str());
            mRoutesNextStep = RoutesNextStep::None;
        } else {
            mPlanesForRoutesUnassigned.push_back(planeId);
        }
        requestPlanRoutes(false);
    } else {
        if (checkPlaneAvailable(planeId, true, false)) {
            mPlanesForJobs.push_back(planeId);
            grabNewFlights();
        } else {
            mPlanesForJobsUnassigned.push_back(planeId);
        }
    }

    updateExtraWorkers();
}

void Bot::actionVisitHR() {
    if (mItemPills == 1) {
        if (GameMechanic::useItem(qPlayer, ITEM_POSTKARTE)) {
            AT_Log("Bot::actionVisitHR(): Used item card");
            mItemPills = 2;
        }
    }
    if (mItemPills == 2) {
        if (qPlayer.HasItem(ITEM_TABLETTEN) == 0) {
            GameMechanic::pickUpItem(qPlayer, ITEM_TABLETTEN);
            AT_Log("Bot::actionVisitHR(): Picked up item pills");
        }
    }

    /* create list sorted by salary */
    std::vector<SLONG> workersSorted;
    workersSorted.reserve(Workers.Workers.AnzEntries());
    for (SLONG c = 0; c < Workers.Workers.AnzEntries(); c++) {
        workersSorted.push_back(c);
    }
    std::sort(workersSorted.begin(), workersSorted.end(), [&](SLONG a, SLONG b) { return Workers.Workers[a].Gehalt < Workers.Workers[b].Gehalt; });

    /* advisors */
    std::vector<SLONG> wantedAdvisors = {BERATERTYP_PERSONAL, BERATERTYP_KEROSIN, BERATERTYP_GELD,      BERATERTYP_INFO,
                                         BERATERTYP_FLUGZEUG, BERATERTYP_FITNESS, BERATERTYP_SICHERHEIT};
    for (auto advisorType : wantedAdvisors) {
        SLONG bestCandidateId = -1;
        SLONG bestCandidateSkill = 0;
        for (SLONG c : workersSorted) {
            const auto &qWorker = Workers.Workers[c];
            if (qWorker.Typ != advisorType) {
                continue;
            }
            if (bestCandidateSkill < qWorker.Talent) {
                if (qWorker.Employer == qPlayer.PlayerNum) {
                    /* best advisor that is currently employed */
                    bestCandidateSkill = qWorker.Talent;
                    bestCandidateId = -1;
                } else if (qWorker.Employer == WORKER_JOBLESS) {
                    /* better candidates has applied */
                    bestCandidateSkill = qWorker.Talent;
                    bestCandidateId = c;
                }
            }
        }
        /* hire best candidate and fire everyone of same type */
        if (bestCandidateId != -1) {
            CString typeStr = StandardTexte.GetS(TOKEN_JOBS, 2000 + advisorType);
            CString skillStr = StandardTexte.GetS(TOKEN_JOBS, 5020 + bestCandidateSkill / 10);
            AT_Log("Bot::actionVisitHR(): Upgrading advisor %s to '%s' (%d)", typeStr.c_str(), skillStr.c_str(), bestCandidateSkill);
            /* fire existing adivsors */
            for (SLONG c = 0; c < Workers.Workers.AnzEntries(); c++) {
                const auto &qWorker = Workers.Workers[c];
                if (qWorker.Employer == qPlayer.PlayerNum && qWorker.Typ == advisorType) {
                    if (GameMechanic::fireWorker(qPlayer, c)) {
                        mNumEmployees--;
                    }
                }
            }
            /* hire new advisor */
            if (GameMechanic::hireWorker(qPlayer, bestCandidateId)) {
                mNumEmployees++;
            }
        }
    }

    /* crew */
    SLONG pilotsTarget = 3;     /* sensible default */
    SLONG stewardessTarget = 6; /* sensible default */
    SLONG numPilotsHired = 0;
    SLONG numStewardessHired = 0;
    if (mLongTermStrategy) {
        SLONG bestPlaneTypeId = mDoRoutes ? mBuyPlaneForRouteId : mBestPlaneTypeId;
        if (bestPlaneTypeId >= 0) {
            const auto &bestPlaneType = PlaneTypes[bestPlaneTypeId];
            pilotsTarget = bestPlaneType.AnzPiloten;
            stewardessTarget = bestPlaneType.AnzBegleiter;
        }
    } else {
        if (mBestUsedPlaneIdx != -1) {
            const auto &bestPlane = Sim.UsedPlanes[mBestUsedPlaneIdx];
            pilotsTarget = bestPlane.ptAnzPiloten;
            stewardessTarget = bestPlane.ptAnzBegleiter;
        }
    }
    if (!mDesignerPlane.Name.empty()) {
        pilotsTarget = std::max(pilotsTarget, mDesignerPlane.CalcPiloten());
        stewardessTarget = std::max(stewardessTarget, mDesignerPlane.CalcBegleiter());
    }

    for (SLONG c : workersSorted) {
        const auto &qWorker = Workers.Workers[c];
        if (qWorker.Employer != WORKER_JOBLESS) {
            continue;
        }
        if (qWorker.Talent < kMinimumEmployeeSkill) {
            continue;
        }
        if (qWorker.Typ == WORKER_PILOT && qPlayer.xPiloten < pilotsTarget) {
            if (GameMechanic::hireWorker(qPlayer, c)) {
                mNumEmployees++;
                numPilotsHired++;
            }
        } else if (qWorker.Typ == WORKER_STEWARDESS && qPlayer.xBegleiter < stewardessTarget) {
            if (GameMechanic::hireWorker(qPlayer, c)) {
                mNumEmployees++;
                numStewardessHired++;
            }
        }
    }
    if (numPilotsHired > 0 || numStewardessHired > 0) {
        AT_Log("Bot::actionVisitHR(): Hiring %d pilots and %d attendants", numPilotsHired, numStewardessHired);
    }

    /* check whether we lost employees / increase salary for unhappy employees once */
    SLONG nWorkers = 0;
    SLONG salaryIncreases = 0;
    for (SLONG c = 0; c < Workers.Workers.AnzEntries(); c++) {
        auto &qWorker = Workers.Workers[c];
        if (qWorker.Employer != qPlayer.PlayerNum) {
            continue;
        }

        nWorkers++;

        /* unhappy? */
        if (qWorker.Happyness < kTargetEmployeeHappiness && qWorker.Gehalt <= qWorker.OriginalGehalt) {
            qWorker.Gehaltsaenderung(true);
            salaryIncreases++;
        }
    }
    if (mNumEmployees > nWorkers) {
        AT_Log("Bot::actionVisitHR(): We lost %d employees", mNumEmployees - nWorkers);
    }
    if (salaryIncreases > 0) {
        AT_Log("Bot::actionVisitHR(): Increased salary of %d employees", salaryIncreases);
    }

    mNumEmployees = nWorkers;
    mExtraPilots = qPlayer.xPiloten;
    mExtraBegleiter = qPlayer.xBegleiter;
    AT_Log("Bot::actionVisitHR(): We have %d extra pilots and %d extra attendants", mExtraPilots, mExtraBegleiter);
}

void Bot::actionBuyKerosine(__int64 moneyAvailable) {
    if (mItemArabTrust == 1) {
        if (GameMechanic::useItem(qPlayer, ITEM_MG)) {
            AT_Log("Bot::actionBuyKerosine(): Used item MG");
            mItemArabTrust = 2;
        }
    }

    auto Preis = Sim.HoleKerosinPreis(1); /* range: 300 - 700 */
    __int64 moneyToSpend = (moneyAvailable - 2500 * 1000LL);
    DOUBLE targetFillRatio = 0.5;
    if (Preis < 500) {
        moneyToSpend = (moneyAvailable - 1500 * 1000LL);
        targetFillRatio = 0.7;
    }
    if (Preis < 450) {
        moneyToSpend = (moneyAvailable - 1000 * 1000LL);
        targetFillRatio = 0.8;
    }
    if (Preis < 400) {
        moneyToSpend = (moneyAvailable - 500 * 1000LL);
        targetFillRatio = 0.9;
    }
    if (Preis < 350) {
        moneyToSpend = moneyAvailable;
        targetFillRatio = 1.0;
    }

    /* update how much kerosine was used */
    assert(mKerosineLevelLastChecked >= qPlayer.TankInhalt);
    mKerosineUsedTodaySoFar += (mKerosineLevelLastChecked - qPlayer.TankInhalt);
    mKerosineLevelLastChecked = qPlayer.TankInhalt;

    if (moneyToSpend > 0) {
        auto res = kerosineQualiOptimization(moneyToSpend, targetFillRatio);
        AT_Log("Bot::actionBuyKerosine(): Buying %lld good and %lld bad kerosine", res.first, res.second);

        auto qualiOld = qPlayer.KerosinQuali;
        auto amountOld = qPlayer.TankInhalt;
        GameMechanic::buyKerosin(qPlayer, 1, res.first);
        GameMechanic::buyKerosin(qPlayer, 2, res.second);
        mKerosineLevelLastChecked = qPlayer.TankInhalt;

        AT_Log("Bot::actionBuyKerosine(): Kerosine quantity: %d => %d", amountOld, qPlayer.TankInhalt);
        AT_Log("Bot::actionBuyKerosine(): Kerosine quality: %.2f => %.2f", qualiOld, qPlayer.KerosinQuali);
    }
}

void Bot::actionBuyKerosineTank(__int64 moneyAvailable) {
    auto nTankTypes = TankSize.size();
    for (SLONG i = nTankTypes - 1; i >= 1; i--) // avoid cheapest tank (not economical)
    {
        if (moneyAvailable >= TankPrice[i]) {
            SLONG amount = std::min(3LL, moneyAvailable / TankPrice[i]);
            AT_Log("Bot::actionBuyKerosineTank(): Buying %d times tank type %d", amount, i);
            GameMechanic::buyKerosinTank(qPlayer, i, amount);
            moneyAvailable = getMoneyAvailable() - kMoneyReserveBuyTanks;
            break;
        }
    }
}

void Bot::actionSabotage(__int64 moneyAvailable) {
    actionVisitSaboteur();

    if (!qPlayer.RobotUse(ROBOT_USE_EXTREME_SABOTAGE) || mNemesis == -1) {
        AT_Error("Bot::actionSabotage(): Conditions not met.");
    }

    SLONG jobType{-1};
    SLONG jobNumber{-1};
    SLONG jobHints{-1};
    if (!determineSabotageMode(moneyAvailable, jobType, jobNumber, jobHints)) {
        return;
    }

    /* exceute sabotage */
    if (jobType == 0) {
        const auto &qNemesisPlanes = Sim.Players.Players[mNemesis].Planes;
        qPlayer.ArabPlaneSelection = qNemesisPlanes.GetRandomUsedIndex(&LocalRandom);
        AT_Log("Bot::actionSabotage(): Selecting plane %s", Helper::getPlaneName(qNemesisPlanes[qPlayer.ArabPlaneSelection]).c_str());
    }

    const auto &nemesisName = Sim.Players.Players[mNemesis].AirlineX;
    auto ret = GameMechanic::setSaboteurTarget(qPlayer, mNemesis);
    if (ret != mNemesis) {
        AT_Error("Bot::actionSabotage(): Cannot sabotage nemesis %s: Cannot set as target", nemesisName.c_str());
        return;
    }

    auto res = GameMechanic::checkPrerequisitesForSaboteurJob(qPlayer, jobType, jobNumber, FALSE).result;
    switch (res) {
    case GameMechanic::CheckSabotageResult::Ok:
        GameMechanic::activateSaboteurJob(qPlayer, FALSE);
        AT_Log("Bot::actionSabotage(): Sabotaging nemesis %s (jobType = %d, jobNumber = %d)", nemesisName.c_str(), jobType, jobNumber);
        mNemesisSabotaged = mNemesis; /* ensures that we do not sabotage him again tomorrow */
        mArabHintsTracker += jobHints;
        break;
    case GameMechanic::CheckSabotageResult::DeniedInvalidParam:
        AT_Error("Bot::actionSabotage(): Cannot sabotage nemesis %s: Invalid param", nemesisName.c_str());
        break;
    case GameMechanic::CheckSabotageResult::DeniedSaboteurBusy:
        AT_Log("Bot::actionSabotage(): Cannot sabotage nemesis %s: Saboteur busy", nemesisName.c_str());
        break;
    case GameMechanic::CheckSabotageResult::DeniedSecurity:
        mNeedToShutdownSecurity = true;
        AT_Log("Bot::actionSabotage(): Cannot sabotage nemesis %s: Blocked by security", nemesisName.c_str());
        break;
    case GameMechanic::CheckSabotageResult::DeniedNotEnoughMoney:
        AT_Error("Bot::actionSabotage(): Cannot sabotage nemesis %s: Not enough money", nemesisName.c_str());
        break;
    case GameMechanic::CheckSabotageResult::DeniedNoLaptop:
        AT_Error("Bot::actionSabotage(): Cannot sabotage nemesis %s: Enemy has no laptop", nemesisName.c_str());
        break;
    case GameMechanic::CheckSabotageResult::DeniedTrust:
        AT_Error("Bot::actionSabotage(): Cannot sabotage nemesis %s: Not enough trust", nemesisName.c_str());
        break;
    default:
        AT_Error("Bot::actionSabotage(): Cannot sabotage nemesis %s: Unknown error", nemesisName.c_str());
    }
}

void Bot::actionVisitSaboteur() {
    if (mItemAntiVirus == 1) {
        if (GameMechanic::useItem(qPlayer, ITEM_SPINNE)) {
            AT_Log("Bot::actionVisitSaboteur(): Used item tarantula");
            mItemAntiVirus = 2;
        }
    }
    if (mItemAntiVirus == 2) {
        if (GameMechanic::PickUpItemResult::PickedUp == GameMechanic::pickUpItem(qPlayer, ITEM_DART)) {
            AT_Log("Bot::actionVisitSaboteur(): Picked up item dart");
            mItemAntiVirus = 3;
        }
    }
    if (Sim.ItemZange == 1) {
        if (qPlayer.HasItem(ITEM_ZANGE) == 1) {
            GameMechanic::removeItem(qPlayer, ITEM_ZANGE);
        }
        if (GameMechanic::PickUpItemResult::PickedUp == GameMechanic::pickUpItem(qPlayer, ITEM_ZANGE)) {
            AT_Log("Bot::actionVisitSaboteur(): Picked up item pliers");
        }
    }
}

__int64 Bot::calcBuyShares(__int64 moneyAvailable, DOUBLE kurs) { return static_cast<__int64>(std::floor((moneyAvailable - 100) / (1.1 * kurs))); }
__int64 Bot::calcSellShares(__int64 moneyToGet, DOUBLE kurs) { return static_cast<__int64>(std::floor((moneyToGet + 100) / (0.9 * kurs))); }

__int64 Bot::calcNumOfFreeShares(SLONG playerId) {
    auto &player = Sim.Players.Players[playerId];
    __int64 amount = player.AnzAktien;
    for (SLONG c = 0; c < 4; c++) {
        amount -= Sim.Players.Players[c].OwnsAktien[playerId];
    }
    return amount;
}

__int64 Bot::calcAmountToBuy(SLONG buyFromPlayerId, SLONG desiredRatio, __int64 moneyAvailable) const {
    auto &player = Sim.Players.Players[buyFromPlayerId];
    __int64 targetAmount = player.AnzAktien * desiredRatio / 100;
    __int64 amountWanted = targetAmount - qPlayer.OwnsAktien[buyFromPlayerId];
    __int64 amountFree = calcNumOfFreeShares(buyFromPlayerId);
    __int64 amountCanAfford = calcBuyShares(moneyAvailable, player.Kurse[0]);
    return std::min({amountFree, amountWanted, amountCanAfford});
}

void Bot::actionEmitShares() {
    SLONG newStock = 0;
    GameMechanic::canEmitStock(qPlayer, &newStock);
    AT_Log("Bot::actionEmitShares(): Emitting stock: %lld", newStock);
    GameMechanic::emitStock(qPlayer, newStock, kStockEmissionMode);

    /* rebuy some to reach target ratio of own stock */
    if (mRunToFinalObjective < FinalPhase::TargetRun) {
        auto moneyAvailable = getMoneyAvailable() - kMoneyReserveBuyOwnShares;
        auto amount = calcAmountToBuy(qPlayer.PlayerNum, mOptions.kOwnStockPosessionRatio, moneyAvailable);
        if (amount > 0) {
            AT_Log("Bot::actionEmitShares(): Buying own stock: %lld", amount);
            GameMechanic::buyStock(qPlayer, qPlayer.PlayerNum, amount);
        }
    }
}

void Bot::actionBuyNemesisShares(__int64 moneyAvailable) {
    for (SLONG dislike = 0; dislike < 4; dislike++) {
        auto &qTarget = Sim.Players.Players[dislike];
        if (dislike == qPlayer.PlayerNum || qTarget.IsOut != 0) {
            continue;
        }
        auto amount = calcAmountToBuy(dislike, 50, moneyAvailable);
        if (amount > 0) {
            AT_Log("Bot::actionBuyNemesisShares(): Buying enemy stock from %s: %lld", qTarget.AirlineX.c_str(), amount);
            GameMechanic::buyStock(qPlayer, dislike, amount);
            moneyAvailable = getMoneyAvailable() - kMoneyReserveBuyOwnShares;
        }
    }
}

void Bot::actionBuyOwnShares(__int64 moneyAvailable) {
    auto amount = calcAmountToBuy(qPlayer.PlayerNum, mOptions.kOwnStockPosessionRatio, moneyAvailable);
    if (amount > 0) {
        AT_Log("Bot::actionBuyOwnShares(): Buying own stock: %lld", amount);
        GameMechanic::buyStock(qPlayer, qPlayer.PlayerNum, amount);
    }
}

void Bot::actionOvertakeAirline() {
    SLONG airline = -1;
    for (SLONG p = 0; p < 4; p++) {
        auto &qTarget = Sim.Players.Players[p];
        if (p == qPlayer.PlayerNum || qTarget.IsOut != 0) {
            continue;
        }
        if (GameMechanic::OvertakeAirlineResult::Ok != GameMechanic::canOvertakeAirline(qPlayer, p)) {
            continue;
        }
        if (airline == -1 || p == mNemesis) {
            airline = p;
        }
    }
    if (airline == -1) {
        AT_Error("Bot::actionOvertakeAirline(): There is no airline we can overtake");
        return;
    }
    AT_Log("Bot::actionOvertakeAirline(): Liquidating %s", Sim.Players.Players[airline].AirlineX.c_str());
    GameMechanic::overtakeAirline(qPlayer, airline, true);
}

void Bot::actionSellShares(__int64 moneyAvailable) {
    if (qPlayer.RobotUse(ROBOT_USE_MAX20PERCENT)) {
        /* do never own more than 20 % of own stock */
        SLONG c = qPlayer.PlayerNum;
        __int64 sells = (qPlayer.OwnsAktien[c] - qPlayer.AnzAktien * mOptions.kOwnStockPosessionRatio / 100);
        if (sells > 0) {
            AT_Log("Bot::actionSellShares(): Selling own stock to have no more than %d %%: %lld", mOptions.kOwnStockPosessionRatio, sells);
            GameMechanic::sellStock(qPlayer, c, sells);
            return;
        }
    }

    SLONG pass = 0;
    for (; pass < 10; pass++) {
        __int64 howMuchToRaise = -(moneyAvailable - qPlayer.Credit);
        if (mRunToFinalObjective == FinalPhase::TargetRun) {
            howMuchToRaise = std::max(howMuchToRaise, mMoneyForFinalObjective);
        }
        if (howMuchToRaise <= 0) {
            break;
        }

        auto res = howToGetMoney();
        if (res == HowToGetMoney::SellOwnShares) {
            SLONG c = qPlayer.PlayerNum;
            __int64 sellsNeeded = calcSellShares(howMuchToRaise, qPlayer.Kurse[0]);
            __int64 sellsMax = std::max(0, qPlayer.OwnsAktien[c] - qPlayer.AnzAktien / 2 - 1);
            auto sells = std::min(sellsMax, sellsNeeded);
            if (sells > 0) {
                AT_Log("Bot::actionSellShares(): Selling own stock: %lld", sells);
                GameMechanic::sellStock(qPlayer, c, sells);
            }
        } else if (res == HowToGetMoney::SellAllOwnShares) {
            SLONG c = qPlayer.PlayerNum;
            __int64 sellsNeeded = calcSellShares(howMuchToRaise, qPlayer.Kurse[0]);
            __int64 sellsMax = qPlayer.OwnsAktien[c];
            auto sells = std::min(sellsMax, sellsNeeded);
            if (sells > 0) {
                AT_Log("Bot::actionSellShares(): Selling all own stock: %lld", sells);
                GameMechanic::sellStock(qPlayer, c, sells);
            }
        } else if (res == HowToGetMoney::SellShares) {
            for (SLONG c = 0; c < Sim.Players.AnzPlayers; c++) {
                if (qPlayer.OwnsAktien[c] == 0 || c == qPlayer.PlayerNum) {
                    continue;
                }

                __int64 sellsNeeded = calcSellShares(howMuchToRaise, Sim.Players.Players[c].Kurse[0]);
                __int64 sellsMax = qPlayer.OwnsAktien[c];
                __int64 sells = std::min(sellsMax, sellsNeeded);
                AT_Log("Bot::actionSellShares(): Selling stock from player %d: %lld", c, sells);
                GameMechanic::sellStock(qPlayer, c, sells);
                break;
            }
        } else {
            break;
        }
    }
    if (pass == 0) {
        AT_Error("Bot::actionSellShares(): We do not actually need money");
    }
}

void Bot::actionVisitMech() {
    if (qPlayer.MechMode != 3) {
        AT_Log("Bot::actionVisitMech(): Setting mech mode to 3");
        GameMechanic::setMechMode(qPlayer, 3);
    }

    const auto &qPlanes = qPlayer.Planes;

    /* save old repair targets */
    std::vector<std::pair<SLONG, UBYTE>> planeList;
    for (SLONG c = 0; c < qPlanes.AnzEntries(); c++) {
        if (qPlanes.IsInAlbum(c) == 0) {
            continue;
        }

        const auto &qPlane = qPlanes[c];
        auto oldTarget = qPlane.TargetZustand;

        /* WorstZustand more than 20 lower than repair target means extra cost! */
        auto worstZustand = std::min(qPlane.WorstZustand, qPlane.Zustand);
        auto worstZustandNoCost = std::min(100, worstZustand + 20);
        GameMechanic::setPlaneTargetZustand(qPlayer, c, worstZustandNoCost);

        if (qPlanes[c].TargetZustand == 100) {
            continue; /* this plane won't cost extra */
        }

        planeList.emplace_back(c, oldTarget);
    }

    /* distribute available money for repair extra costs */
    mMoneyReservedForRepairs = 0;
    auto moneyAvailable = getMoneyAvailable() - kMoneyReserveRepairs;
    bool keepGoing = true;
    while (keepGoing && moneyAvailable >= 0) {
        keepGoing = false;
        for (const auto &iter : planeList) {
            const auto &qPlane = qPlanes[iter.first];
            auto worstZustand = std::min(qPlane.WorstZustand, qPlane.Zustand);
            SLONG cost = (qPlane.TargetZustand + 1 > (worstZustand + 20)) ? (qPlane.ptPreis / 110) : 0;
            if (qPlane.TargetZustand < kPlaneTargetZustand && moneyAvailable >= cost) {
                GameMechanic::setPlaneTargetZustand(qPlayer, iter.first, qPlane.TargetZustand + 1);
                keepGoing = true;
                mMoneyReservedForRepairs += cost;
                moneyAvailable = getMoneyAvailable() - kMoneyReserveRepairs;
            }
            if (moneyAvailable < 0) {
                break;
            }
        }
    }

    for (const auto &iter : planeList) {
        const auto &qPlane = qPlanes[iter.first];
        auto worstZustand = std::min(qPlane.WorstZustand, qPlane.Zustand);
        if (qPlane.TargetZustand > iter.second) {
            AT_Log("Bot::actionVisitMech(): Increasing repair target of plane %s: %u => %u (Zustand = %u, WorstZustand = %u)",
                   Helper::getPlaneName(qPlane, 1).c_str(), iter.second, qPlane.TargetZustand, qPlane.Zustand, worstZustand);
        } else if (qPlane.TargetZustand < iter.second) {
            AT_Log("Bot::actionVisitMech(): Lowering repair target of plane %s: %u => %u (Zustand = %u, WorstZustand = %u)",
                   Helper::getPlaneName(qPlane, 1).c_str(), iter.second, qPlane.TargetZustand, qPlane.Zustand, worstZustand);
        }
    }
    AT_Log("Bot::actionVisitMech(): We are reserving %s $ for repairs, available money: %s $", Insert1000erDots64(mMoneyReservedForRepairs).c_str(),
           Insert1000erDots64(getMoneyAvailable()).c_str());
}

void Bot::actionVisitDutyFree(__int64 moneyAvailable) {
    if (mItemAntiStrike == 1) {
        if (GameMechanic::useItem(qPlayer, ITEM_BH)) {
            AT_Log("Bot::actionVisitDutyFree(): Used item BH");
            mItemAntiStrike = 2;
        }
    }
    if (mItemAntiStrike == 2) {
        if (GameMechanic::PickUpItemResult::PickedUp == GameMechanic::pickUpItem(qPlayer, ITEM_HUFEISEN)) {
            AT_Log("Bot::actionVisitDutyFree(): Picked up item horse shoe");
            mItemAntiStrike = 3;
        }
    }

    if (mItemArabTrust == 0 && Sim.Date > 0 && qPlayer.ArabTrust == 0) {
        if (GameMechanic::BuyItemResult::Ok == GameMechanic::buyDutyFreeItem(qPlayer, ITEM_MG)) {
            mItemArabTrust = 1;
        }
    }

    __int64 money = qPlayer.Money;
    if (qPlayer.LaptopQuality < 4) {
        auto quali = qPlayer.LaptopQuality;
        GameMechanic::buyDutyFreeItem(qPlayer, ITEM_LAPTOP);
        AT_Log("Bot::actionVisitDutyFree(): Buying laptop (%d => %d)", quali, qPlayer.LaptopQuality);
    }
    __int64 moneySpent = std::max(0LL, (money - qPlayer.Money));
    moneyAvailable -= moneySpent;

    if (moneyAvailable > 0 && !qPlayer.HasItem(ITEM_HANDY)) {
        AT_Log("Bot::actionVisitDutyFree(): Buying cell phone");
        GameMechanic::buyDutyFreeItem(qPlayer, ITEM_HANDY);
    }
}

void Bot::actionVisitBoss() {
    if (mItemPills == 0 && Sim.ItemPostcard == 1) {
        if (GameMechanic::PickUpItemResult::PickedUp == GameMechanic::pickUpItem(qPlayer, ITEM_POSTKARTE)) {
            AT_Log("Bot::actionVisitBoss(): Picked up item card");
            mItemPills = 1;
        }
    }

    /* what is available? how much money are we currently bidding in total? */
    mMoneyReservedForAuctions = 0;
    mBossGateAvailable = false;
    for (const auto &qZettel : TafelData.Gate) {
        if (qZettel.ZettelId < 0) {
            continue;
        }
        mBossGateAvailable = true;
        if (qZettel.Player == qPlayer.PlayerNum) {
            mMoneyReservedForAuctions += qZettel.Preis;
        }
    }
    mBossNumCitiesAvailable = 0;
    for (const auto &qZettel : TafelData.City) {
        if (qZettel.ZettelId < 0) {
            continue;
        }
        mBossNumCitiesAvailable++;
        if (qZettel.Player == qPlayer.PlayerNum) {
            mMoneyReservedForAuctions += qZettel.Preis;
        }
    }

    auto moneyAvailable = getMoneyAvailable() - kMoneyReserveBossOffice;

    /* auction for gates */
    for (SLONG c = 0; c < TafelData.ByPositions.size(); c++) {
        auto &qZettel = *TafelData.ByPositions[c];
        if (qZettel.Type != CTafelZettel::Type::GATE) {
            continue;
        }
        if (qZettel.ZettelId < 0 || qZettel.Player == qPlayer.PlayerNum) {
            continue;
        }
        if (qZettel.Preis > moneyAvailable) {
            continue;
        }

        if (GameMechanic::bidOnGate(qPlayer, c)) {
            mMoneyReservedForAuctions += qZettel.Preis;
            moneyAvailable = getMoneyAvailable() - kMoneyReserveBossOffice;
            AT_Log("Bot::actionVisitBoss(): Bidding on gate: %d $", qZettel.Preis);
        }
    }

    /* auction for subsidiaries */
    for (SLONG c = 0; c < TafelData.ByPositions.size(); c++) {
        auto &qZettel = *TafelData.ByPositions[c];
        if (qZettel.Type != CTafelZettel::Type::CITY) {
            continue;
        }
        if (qZettel.ZettelId < 0 || qZettel.Player == qPlayer.PlayerNum) {
            continue;
        }
        if (qPlayer.RentCities.RentCities[Cities(qZettel.ZettelId)].Rang != 0) {
            continue;
        }
        if (qZettel.Preis > moneyAvailable) {
            continue;
        }

        if (GameMechanic::bidOnCity(qPlayer, c)) {
            mMoneyReservedForAuctions += qZettel.Preis;
            moneyAvailable = getMoneyAvailable() - kMoneyReserveBossOffice;
            AT_Log("Bot::actionVisitBoss(): Bidding on city %s: %d $", Cities[qZettel.ZettelId].Name.c_str(), qZettel.Preis);
        }
    }
}

void Bot::actionVisitRouteBox() {
    updateRouteInfoBoard();
    assignPlanesToRoutes(false);
    findBestRoute();
}

void Bot::actionRentRoute() {
    if (!mDoRoutes) {
        /* kill routes rented at beginning of game */
        const auto &qRRouten = qPlayer.RentRouten.RentRouten;
        for (SLONG routeID = 0; routeID < qRRouten.AnzEntries(); routeID++) {
            if (qRRouten[routeID].Rang != 0) {
                GameMechanic::killRoute(qPlayer, routeID);
                AT_Log("Bot::RobotInit(): Removing initial route %s", Helper::getRouteName(Routen[routeID]).c_str());
            }
        }

        /* in route mission, do not loose any time! */
        if (qPlayer.RobotUse(ROBOT_USE_FORCEROUTES)) {
            mDoRoutes = true;
            AT_Log("Bot::actionRentRoute(): Switching to routes (forced).");
        }
        return;
    }

    auto routeA = mWantToRentRouteId;
    mWantToRentRouteId = -1;
    assert(routeA != -1);

    /* find route in reverse direction */
    SLONG routeB = -1;
    for (SLONG c = 0; c < Routen.AnzEntries(); c++) {
        if ((Routen.IsInAlbum(c) != 0) && Routen[c].VonCity == Routen[routeA].NachCity && Routen[c].NachCity == Routen[routeA].VonCity) {
            routeB = c;
            break;
        }
    }
    if (-1 == routeB) {
        AT_Error("Bot::actionRentRoute: Unable to find route in reverse direction.");
        return;
    }

    /* rent route */
    if (!GameMechanic::rentRoute(qPlayer, routeA)) {
        AT_Error("Bot::actionRentRoute: Failed to rent route.");
        return;
    }
    mRoutes.emplace_back(routeA, routeB, mPlaneTypeForNewRoute);
    mRoutes.back().ticketCostFactor = kDefaultTicketPriceFactor;
    AT_Log("Bot::actionRentRoute(): Renting route %s (using plane type %s): ", Helper::getRouteName(getRoute(mRoutes.back())).c_str(),
           PlaneTypes[mPlaneTypeForNewRoute].Name.c_str());
    mPlaneTypeForNewRoute = -1;

    /* update sorted list */
    assert(mRoutes.size() > 0);
    mRoutesSortedByOwnUtilization.resize(mRoutes.size());
    for (SLONG i = mRoutesSortedByOwnUtilization.size() - 1; i >= 1; i--) {
        mRoutesSortedByOwnUtilization[i] = mRoutesSortedByOwnUtilization[i - 1];
    }
    mRoutesSortedByOwnUtilization[0] = mRoutes.size() - 1;

    /* use existing planes */
    for (auto id : mPlanesForNewRoute) {
        const auto &qPlane = qPlayer.Planes[id];
        AT_Log("Bot::actionRentRoute(): Using existing plane: %s", Helper::getPlaneName(qPlane).c_str());

        mRoutes.back().planeIds.push_back(id);
        mPlanesForRoutes.push_back(id);
        bool erased = eraseFirst(mPlanesForRoutesUnassigned, id);
        if (!erased) {
            AT_Error("Bot::actionRentRoute(): Plane with ID = %d should have been in unassigned list", id);
        }
    }
    mPlanesForNewRoute.clear();

    mRoutesNextStep = RoutesNextStep::None;
    updateRouteInfoBoard();

    requestPlanRoutes(false);
}

void Bot::actionBuyAdsForRoutes(__int64 moneyAvailable) {
    actionVisitAds();

    if (mRoutesNextStep != RoutesNextStep::BuyAdsForRoute) {
        AT_Error("Bot::actionBuyAdsForRoutes(): Conditions not met.");
        return;
    }

    assert(mImproveRouteId != -1);
    auto &qRoute = mRoutes[mImproveRouteId];

    const SLONG largestAdCampaign = 5;

    SLONG cost = 0;
    SLONG adCampaignSize = kSmallestAdCampaign;
    for (; adCampaignSize <= largestAdCampaign; adCampaignSize++) {
        cost = gWerbePrice[1 * 6 + adCampaignSize];
        SLONG imageDelta = (cost / 30000);
        if (cost > moneyAvailable) {
            adCampaignSize -= 1;
            break;
        }
        if (getRentRoute(qRoute).Image + imageDelta > 100) {
            break;
        }
    }
    if (adCampaignSize < kSmallestAdCampaign) {
        return; /* not enough money */
    }
    adCampaignSize = std::min(adCampaignSize, largestAdCampaign);

    SLONG oldImage = getRentRoute(qRoute).Image;
    GameMechanic::buyAdvertisement(qPlayer, 1, adCampaignSize, qRoute.routeId);
    SLONG newImage = getRentRoute(qRoute).Image;
    AT_Log("Bot::actionBuyAdsForRoutes(): Buying advertisement for route %s for %d $ (image improved %d => %d)", Helper::getRouteName(getRoute(qRoute)).c_str(),
           cost, oldImage, newImage);

    qRoute.image = newImage;

    mRoutesNextStep = RoutesNextStep::None;
}

void Bot::actionBuyAds(__int64 moneyAvailable) {
    actionVisitAds();

    assert(kSmallestAdCampaign >= 1);
    for (SLONG adCampaignSize = 5; adCampaignSize >= kSmallestAdCampaign; adCampaignSize--) {
        SLONG cost = gWerbePrice[0 * 6 + adCampaignSize];
        SLONG cost2 = gWerbePrice[0 * 6 + (adCampaignSize - 1)];
        SLONG imageDelta2 = cost2 / 10000 * ((adCampaignSize - 1) + 6) / 55;

        while (moneyAvailable > cost && qPlayer.Image < 1000 && (qPlayer.Image + imageDelta2 < 1000)) {
            SLONG oldImage = qPlayer.Image;
            AT_Log("Bot::actionBuyAds(): Buying advertisement for airline for %d $", cost);
            GameMechanic::buyAdvertisement(qPlayer, 0, adCampaignSize);
            moneyAvailable = getMoneyAvailable();

            AT_Log("Bot::actionBuyAds(): Airline image improved (%d => %d)", oldImage, qPlayer.Image);
            if (!qPlayer.RobotUse(ROBOT_USE_WERBUNG)) {
                return;
            }
        }
    }

    if (mRoutesNextStep == RoutesNextStep::ImproveAirlineImage) {
        mRoutesNextStep = RoutesNextStep::None;
    }
}

void Bot::actionVisitAds() {
    if (mItemAntiVirus == 3) {
        if (GameMechanic::useItem(qPlayer, ITEM_DART)) {
            AT_Log("Bot::actionVisitAds(): Used item darts");
            mItemAntiVirus = 4;
        }
    }
    if (mItemAntiVirus == 4) {
        if (qPlayer.HasItem(ITEM_DISKETTE) == 0) {
            GameMechanic::pickUpItem(qPlayer, ITEM_DISKETTE);
            AT_Log("Bot::actionVisitAds(): Picked up item floppy disk");
        }
    }
    mCurrentImage = qPlayer.Image;
    AT_Log("Bot::actionVisitAds(): Checked company image: %d", mCurrentImage);
}

void Bot::actionVisitSecurity(__int64 /*moneyAvailable*/) {
    bool targetState = isLateGame();
    GameMechanic::setSecurity(qPlayer, 0, targetState); /* office: spiked coffee, bomb */
    GameMechanic::setSecurity(qPlayer, 1, targetState); /* laptop: virus */
    GameMechanic::setSecurity(qPlayer, 2, targetState); /* HR: strike */
    GameMechanic::setSecurity(qPlayer, 3, targetState); /* bank: hacking */
    GameMechanic::setSecurity(qPlayer, 4, targetState); /* routebox: route stealing */
    GameMechanic::setSecurity(qPlayer, 5, targetState); /* admin: cut phones, fake announcement */
    GameMechanic::setSecurity(qPlayer, 6, targetState); /* planes: food, tv, crash */
    GameMechanic::setSecurity(qPlayer, 7, targetState); /* planes: tire, engines */
    GameMechanic::setSecurity(qPlayer, 8, targetState); /* gates: fake ads, seize plane */
    mUsingSecurity = targetState;
    if (targetState) {
        AT_Log("Bot::actionVisitSecurity(): Activate security measures");
    } else {
        AT_Log("Bot::actionVisitSecurity(): Deactivate security measures");
    }
}