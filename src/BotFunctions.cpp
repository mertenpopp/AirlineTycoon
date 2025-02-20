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
#include <cassert>
#include <climits>
#include <cmath>
#include <string>
#include <tuple>
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

struct RouteScore {
    DOUBLE score{};
    SLONG routeId{-1};
    SLONG planeTypeId{-1};
    std::vector<SLONG> planeId{};
    SLONG numPlanesToBuy{-1};

    bool operator<(const RouteScore &other) const {
        if (planeId.size() == other.planeId.size()) {
            return score > other.score;
        }
        return (planeId.size() > other.planeId.size());
    }
};

void Bot::grabNewFlights() {
    /* this will cause the planning algo to assume that we have not checked these today */
    mLastTimeInRoom.erase(ACTION_CALL_INTERNATIONAL);
    mLastTimeInRoom.erase(ACTION_CALL_INTER_HANDY);
    mLastTimeInRoom.erase(ACTION_CHECKAGENT1);
    mLastTimeInRoom.erase(ACTION_CHECKAGENT2);
    mLastTimeInRoom.erase(ACTION_CHECKAGENT3);
}

void Bot::determineNemesis() {
    auto nemesisOld = mNemesis;
    mNemesis = -1;
    mNemesisScore = INT_MIN;
    auto nemesisSabotaged = std::exchange(mNemesisSabotaged, -1);
    if (qPlayer.HasBerater(BERATERTYP_INFO) < 50) {
        AT_Log("Bot::determineNemesis(): Need to hire spy first");
        return;
    }

    for (SLONG p = 0; p < 4; p++) {
        auto &qTarget = Sim.Players.Players[p];
        if (p == qPlayer.PlayerNum || qTarget.IsOut != 0) {
            continue;
        }

        __int64 score = 0;
        if (Sim.Difficulty == DIFF_FREEGAME) {
            score = qTarget.BilanzWoche.Hole().GetOpSaldo();
        } else {
            /* for missions */
            if (Sim.Difficulty == DIFF_FINAL || Sim.Difficulty == DIFF_ADDON10) {
                /* better than GetMissionRating(): Calculate sum of part cost instead of just number of parts */
                const auto &qPrices = (Sim.Difficulty == DIFF_FINAL) ? RocketPrices : StationPrices;
                auto nParts = qPrices.size();
                for (SLONG i = 0; i < nParts; i++) {
                    if ((qTarget.RocketFlags & (1 << i)) != 0) {
                        score += qPrices[i];
                    }
                }
            } else if (Sim.Difficulty == DIFF_ADDON01) {
                /* negative: lower score is better! */
                score = -qTarget.GetMissionRating();
            } else {
                score = qTarget.GetMissionRating();
            }
        }
        if (score > mNemesisScore && p != nemesisSabotaged) {
            mNemesis = p;
            mNemesisScore = score;
        }
    }
    if (-1 != mNemesis) {
        if (nemesisOld != mNemesis) {
            AT_Log("Bot::determineNemesis(): Our nemesis now is %s with a score of %s", Sim.Players.Players[mNemesis].AirlineX.c_str(),
                   Insert1000erDots64(mNemesisScore).c_str());
        } else {
            AT_Log("Bot::determineNemesis(): Our nemesis is still %s with a score of %s", Sim.Players.Players[mNemesis].AirlineX.c_str(),
                   Insert1000erDots64(mNemesisScore).c_str());
        }
    }
}

void Bot::switchToFinalTarget() {
    if (mRunToFinalObjective == FinalPhase::TargetRun) {
        AT_Log("Bot::switchToFinalTarget(): We are in final target run.");
        return;
    }

    __int64 requiredMoney = 0;
    bool forceSwitch = false;
    DOUBLE nemesisRatio = 0;
    if (qPlayer.RobotUse(ROBOT_USE_NASA) && (Sim.Difficulty == DIFF_FINAL || Sim.Difficulty == DIFF_ADDON10)) {
        const auto &qPrices = (Sim.Difficulty == DIFF_FINAL) ? RocketPrices : StationPrices;
        auto nParts = qPrices.size();
        SLONG numRequired = 0;
        for (SLONG i = 0; i < nParts; i++) {
            if ((qPlayer.RocketFlags & (1 << i)) == 0) {
                requiredMoney += qPrices[i];
                numRequired++;
            }
        }
        AT_Log("Bot::switchToFinalTarget(): Need %lld to buy %d missing pieces.", requiredMoney, numRequired);

        nemesisRatio = 1.0 * mNemesisScore / requiredMoney;
        if (nemesisRatio > 0.5) {
            forceSwitch = true;
        }
    } else if (qPlayer.RobotUse(ROBOT_USE_MUCHWERBUNG) && Sim.Difficulty == DIFF_HARD) {
        /* formula that calculates image gain from largest campaign */
        SLONG adCampaignSize = 5;
        SLONG numCampaignsRequired = ceil_div((TARGET_IMAGE - getImage()) * 55, (adCampaignSize + 6) * (gWerbePrice[adCampaignSize] / 10000));
        requiredMoney = numCampaignsRequired * static_cast<__int64>(gWerbePrice[adCampaignSize]);
        AT_Log("Bot::switchToFinalTarget(): Need %lld to buy %d ad campaigns.", requiredMoney, numCampaignsRequired);

        nemesisRatio = 1.0 * mNemesisScore / TARGET_IMAGE;
        if (nemesisRatio > 0.7) {
            forceSwitch = true;
        }
    } else if (qPlayer.RobotUse(ROBOT_USE_LUXERY) && Sim.Difficulty == DIFF_ADDON05) {
        /* how many service points can we get by upgrading? */
        SLONG servicePointsStart = qPlayer.GetMissionRating();
        SLONG servicePoints = servicePointsStart;
        for (SLONG upgradeWhat = 0; upgradeWhat < 7; upgradeWhat++) {
            if (servicePoints > TARGET_SERVICE) {
                break;
            }

            auto planes = getAllPlanes();
            for (auto planeId : planes) {
                const CPlane &qPlane = qPlayer.Planes[planeId];
                __int64 ptPassagiere = qPlane.ptPassagiere;

                if (servicePoints > TARGET_SERVICE) {
                    break;
                }

                switch (upgradeWhat) {
                case 0:
                    if (qPlane.SitzeTarget < 2) {
                        requiredMoney += ptPassagiere * (SeatCosts[2] - SeatCosts[qPlane.Sitze] / 2);
                        servicePoints += (2 - qPlane.Sitze);
                    }
                    break;
                case 1:
                    if (qPlane.TablettsTarget < 2) {
                        requiredMoney += ptPassagiere * (TrayCosts[2] - TrayCosts[qPlane.Tabletts] / 2);
                        servicePoints += (2 - qPlane.Tabletts);
                    }
                    break;
                case 2:
                    if (qPlane.DecoTarget < 2) {
                        requiredMoney += ptPassagiere * (DecoCosts[2] - DecoCosts[qPlane.Deco] / 2);
                        servicePoints += (2 - qPlane.Deco);
                    }
                    break;
                case 3:
                    if (qPlane.ReifenTarget < 2) {
                        requiredMoney += (ReifenCosts[2] - ReifenCosts[qPlane.Reifen] / 2);
                        servicePoints += (2 - qPlane.Reifen);
                    }
                    break;
                case 4:
                    if (qPlane.TriebwerkTarget < 2) {
                        requiredMoney += (TriebwerkCosts[2] - TriebwerkCosts[qPlane.Triebwerk] / 2);
                        servicePoints += (2 - qPlane.Triebwerk);
                    }
                    break;
                case 5:
                    if (qPlane.SicherheitTarget < 2) {
                        requiredMoney += (SicherheitCosts[2] - SicherheitCosts[qPlane.Sicherheit] / 2);
                        servicePoints += (2 - qPlane.Sicherheit);
                    }
                    break;
                case 6:
                    if (qPlane.ElektronikTarget < 2) {
                        requiredMoney += (ElektronikCosts[2] - ElektronikCosts[qPlane.Elektronik] / 2);
                        servicePoints += (2 - qPlane.Elektronik);
                    }
                    break;
                default:
                    AT_Error("Bot::switchToFinalTarget(): Default case should not be reached.");
                    DebugBreak();
                }
            }
        }

        nemesisRatio = 1.0 * mNemesisScore / TARGET_SERVICE;
        if (servicePoints <= TARGET_SERVICE) {
            AT_Log("Bot::switchToFinalTarget(): Can only get %d service points in total (+ %d) by upgrading existing planes.", servicePoints,
                   servicePoints - servicePointsStart);
            requiredMoney = LLONG_MAX; /* we cannot reach target yet */
        } else {
            AT_Log("Bot::switchToFinalTarget(): Need %lld to upgrade existing planes.", requiredMoney);
            if (nemesisRatio > 0.7) {
                forceSwitch = true;
            }
        }
    } else if (qPlayer.RobotUse(ROBOT_USE_GROSSESKONTO)) {
        requiredMoney = BTARGET_KONTO;
    } else if (qPlayer.RobotUse(ROBOT_USE_LUXERY) && Sim.Difficulty == DIFF_ATFS02) {
        /* how much money do we need to upgrade everything safety-related? */
        auto planes = getAllPlanes();
        auto numPlanes = std::min(5, static_cast<SLONG>(planes.size()));
        for (SLONG c = 0; c < numPlanes; c++) {
            const CPlane &qPlane = qPlayer.Planes[planes[c]];
            if (qPlane.ReifenTarget < 2) {
                requiredMoney += (ReifenCosts[2] - ReifenCosts[qPlane.Reifen] / 2);
            }
            if (qPlane.TriebwerkTarget < 2) {
                requiredMoney += (TriebwerkCosts[2] - TriebwerkCosts[qPlane.Triebwerk] / 2);
            }
            if (qPlane.SicherheitTarget < 2) {
                requiredMoney += (SicherheitCosts[2] - SicherheitCosts[qPlane.Sicherheit] / 2);
            }
            if (qPlane.ElektronikTarget < 2) {
                requiredMoney += (ElektronikCosts[2] - ElektronikCosts[qPlane.Elektronik] / 2);
            }
        }

        if (qPlayer.Planes.GetNumUsed() < 5) {
            AT_Log("Bot::switchToFinalTarget(): We do not have enough planes yet: %d (need 5)", qPlayer.Planes.GetNumUsed());
            requiredMoney = LLONG_MAX; /* we cannot reach target yet */
        } else {
            AT_Log("Bot::switchToFinalTarget(): Need %lld to upgrade existing planes.", requiredMoney);
        }
    } else {
        /* no race to finish for this mission */
        return;
    }

    if (nemesisRatio > 0) {
        AT_Log("Bot::switchToFinalTarget(): Most dangerous competitor is %s with %.1f %% of goal achieved.", Sim.Players.Players[mNemesis].AirlineX.c_str(),
               nemesisRatio * 100);
    }
    if (forceSwitch) {
        AT_Log("Bot::switchToFinalTarget(): Competitor too close, forcing switch.");
    }

    /* is our target in reach at all? */
    if (requiredMoney == LLONG_MAX) {
        AT_Log("Bot::switchToFinalTarget(): Cannot switch to final target run, target not in reach.");
        if (mRunToFinalObjective == FinalPhase::SaveMoney) {
            AT_Error("Bot::switchToFinalTarget(): We switched to 'save money' phase too early!");
        } else if (mRunToFinalObjective == FinalPhase::TargetRun) {
            AT_Error("Bot::switchToFinalTarget(): We switched to 'target run' phase too early!");
        }
        return;
    }

    /* switch to final phase if we have enough money */
    auto availableMoney = howMuchMoneyCanWeGet(true);
    auto cash = qPlayer.Money - kMoneyEmergencyFund;
    if (mRunToFinalObjective < FinalPhase::TargetRun) {
        if ((availableMoney > requiredMoney) || forceSwitch) {
            mRunToFinalObjective = FinalPhase::TargetRun;
            mMoneyForFinalObjective = requiredMoney;
            AT_Log("Bot::switchToFinalTarget(): Switching to final target run. Need %s $, got %s $ (+ %s $).", Insert1000erDots64(requiredMoney).c_str(),
                   Insert1000erDots64(cash).c_str(), Insert1000erDots64(availableMoney - cash).c_str());
            return;
        }
    }

    /* switch to money saving phase if we have already 80% */
    if (mRunToFinalObjective < FinalPhase::SaveMoney) {
        if (1.0 * availableMoney / requiredMoney >= 0.8) {
            mRunToFinalObjective = FinalPhase::SaveMoney;
            mMoneyForFinalObjective = requiredMoney;
            AT_Log("Bot::switchToFinalTarget(): Switching to money saving phase. Need %s $, got %s $ (+ %s $).", Insert1000erDots64(requiredMoney).c_str(),
                   Insert1000erDots64(cash).c_str(), Insert1000erDots64(availableMoney - cash).c_str());
            return;
        }
    }

    AT_Log("Bot::switchToFinalTarget(): Cannot switch to final target run. Need %s $, got %s $ (+ %s $).", Insert1000erDots64(requiredMoney).c_str(),
           Insert1000erDots64(cash).c_str(), Insert1000erDots64(availableMoney - cash).c_str());
}

std::vector<SLONG> Bot::findBestAvailablePlaneType(bool forRoutes, bool canRefresh) {
    if (canRefresh) {
        mKnownPlaneTypes = GameMechanic::getAvailablePlaneTypes();
        AT_Log("Bot::findBestAvailablePlaneType(): Checking available plane types: %d available", mKnownPlaneTypes.size());
    }

    if (mKnownPlaneTypes.empty()) {
        AT_Warn("Bot::findBestAvailablePlaneType(): No plane types known yet.");
        return {};
    }

    std::vector<std::pair<SLONG, DOUBLE>> scores;
    for (const auto &i : mKnownPlaneTypes) {
        if (!PlaneTypes.IsInAlbum(i)) {
            continue;
        }
        const auto &planeType = PlaneTypes[i];

        DOUBLE score = 1.0; /* multiplication (geometric mean) because values have wildly different ranges */
        score = 1.0 * planeType.Passagiere;
        if (!forRoutes) {
            score *= planeType.Reichweite;
        }
        score /= planeType.Verbrauch;

        scores.emplace_back(i, score);
    }
    std::sort(scores.begin(), scores.end(), [](const std::pair<SLONG, DOUBLE> &a, const std::pair<SLONG, DOUBLE> &b) { return a.second > b.second; });

    /* build list */
    std::vector<SLONG> bestList;
    bestList.reserve(scores.size());

    /* exception: Force specified plane type to be best */
    if (kPlaneScoreForceBest != -1) {
        SLONG bestType = kPlaneScoreForceBest + 0x10000000;
        AT_Log("Bot::findBestAvailablePlaneType(): Forcing best plane type to be %s", PlaneTypes[bestType].Name.c_str());
        bestList.push_back(bestType);
    }

    /* exception: Have atleast one gulfstream for jobs */
    SLONG numGulfstream = 0;
    SLONG gulfstreamType = 119 + 0x10000000;
    for (SLONG i = 0; i < qPlayer.Planes.AnzEntries(); i++) {
        if (qPlayer.Planes.IsInAlbum(i) && qPlayer.Planes[i].TypeId == gulfstreamType) {
            numGulfstream++;
        }
    }
    if (numGulfstream == 0 && !mDoRoutes) {
        AT_Log("Bot::findBestAvailablePlaneType(): Forcing best plane type to be %s", PlaneTypes[gulfstreamType].Name.c_str());
        bestList.push_back(gulfstreamType);
    }

    for (const auto &i : scores) {
        AT_Log("Bot::findBestAvailablePlaneType(): Plane type %s has score %.2e", PlaneTypes[i.first].Name.c_str(), i.second);
        bestList.push_back(i.first);
    }

    if (bestList.empty()) {
        AT_Warn("Bot::findBestAvailablePlaneType(): No plane types selected!");
    } else {
        AT_Log("Bot::findBestAvailablePlaneType(): Best plane type is %s", PlaneTypes[bestList[0]].Name.c_str());
    }
    return bestList;
}

void Bot::grabFlights(BotPlaner &planer, bool areWeInOffice) {
    auto res = howToPlanFlights();
    if (HowToPlan::None == res) {
        AT_Error("Bot::grabFlights(): Tried to grab plans without ability to plan them");
        return;
    }

    planer.setMinScoreRatio(mOptions.kSchedulingMinScoreRatio);
    planer.setMinScoreRatioLastMinute(mOptions.kSchedulingMinScoreRatioLastMinute);

    /* configure weighting for special missions */
    switch (Sim.Difficulty) {
    case DIFF_TUTORIAL:
        planer.setConstBonus(1000 * 1000); /* need to schedule 10 jobs ASAP, so premium does not really matter */
        break;
    case DIFF_FIRST:
        planer.setPassengerFactor(10 * 1000); /* need to fly as many passengers as possible */
        break;
    case DIFF_ADDON04:
        // planer.setDistanceFactor(1);
        // planer.setMinSpeedRatio(0.6F);
        break;
    case DIFF_ADDON09:
        planer.setUhrigBonus(1000 * 1000);
        break;
    default:
        break;
    }

    if (qPlayer.RobotUse(ROBOT_USE_FREE_FRACHT)) {
        planer.setFreeFreightBonus(500 * 1000);
    } else if (qPlayer.RobotUse(ROBOT_USE_MUCH_FRACHT)) {
        planer.setFreightBonus(500 * 1000);
    }

    int extraBufferTime = kAvailTimeExtra;
    if (!areWeInOffice && HowToPlan::Office == res) {
        extraBufferTime += 1;
        if (Sim.GetMinute() >= 30) {
            extraBufferTime += 1;
        }
        AT_Log("Bot::grabFlights(): Extra time planned for walking to office: %d", extraBufferTime);
    }

    mPlanerSolution = planer.generateSolution(mPlanesForJobs, mPlanesForJobsUnassigned, extraBufferTime);
    if (!mPlanerSolution.empty()) {
        BotPlaner::takeAllJobs(qPlayer, mPlanerSolution);
        requestPlanFlights(areWeInOffice);
    }
}

void Bot::requestPlanFlights(bool areWeInOffice) {
    auto res = howToPlanFlights();
    if (res == HowToPlan::Laptop) {
        AT_Log("Bot::requestPlanFlights(): Planning using laptop");
        planFlights();
    } else {
        AT_Log("Bot::requestPlanFlights(): No laptop, need to go to office");
        mNeedToPlanJobs = true;
        forceReplanning();
    }

    if (res == HowToPlan::Office && areWeInOffice) {
        AT_Log("Bot::requestPlanFlights(): Already in office, planning right now");
        planFlights();
        mNeedToPlanJobs = false;
    }
}

void Bot::planFlights() {
    mNeedToPlanJobs = false;

    SLONG oldGain = calcCurrentGainFromJobs();
    if (!BotPlaner::applySolution(qPlayer, mPlanerSolution)) {
        AT_Error("Bot::planFlights(): Solution does not apply! Need to re-plan.");

        BotPlaner planer(qPlayer, qPlayer.Planes);
        mPlanerSolution = planer.generateSolution(mPlanesForJobs, mPlanesForJobsUnassigned, kAvailTimeExtra);
        if (!mPlanerSolution.empty()) {
            BotPlaner::takeAllJobs(qPlayer, mPlanerSolution);
            BotPlaner::applySolution(qPlayer, mPlanerSolution);
        }
    }
    mPlanerSolution = {};

    SLONG newGain = calcCurrentGainFromJobs();
    SLONG diff = newGain - oldGain;
    if (diff > 0) {
        AT_Log("Bot::planFlights(): Total gain improved: %s $ (+%s $)", Insert1000erDots(newGain).c_str(), Insert1000erDots(diff).c_str());
    } else if (diff == 0) {
        AT_Log("Bot::planFlights(): Total gain did not change: %s $", Insert1000erDots(newGain).c_str());
    } else {
        AT_Log("Bot::planFlights(): Total gain got worse: %s $ (%s $)", Insert1000erDots(newGain).c_str(), Insert1000erDots(diff).c_str());
    }

    /* replace automatic flights with routes */
    SLONG count = 0;
    for (const auto &id : mPlanesForJobs) {
        count += replaceAutomaticFlights(id);
    }
    AT_Log("Bot::planFlights(): Replaced %d automatic flights with routes", count);
    Helper::checkFlightJobs(qPlayer, false, true);

    /* check whether we will incur any fines */
    SLONG num = 0;
    mMoneyReservedForFines = 0;
    for (SLONG i = 0; i < qPlayer.Auftraege.AnzEntries(); i++) {
        if (qPlayer.Auftraege.IsInAlbum(i) == 0) {
            continue;
        }
        const auto &job = qPlayer.Auftraege[i];
        if ((job.VonCity == job.NachCity) || (job.InPlan != 0) || (job.Praemie < 0)) {
            continue;
        }
        mMoneyReservedForFines += job.Strafe;
        num++;
        AT_Info("Job %s not planned, fine +%s", Helper::getJobName(job).c_str(), Insert1000erDots64(job.Strafe).c_str());
    }
    for (SLONG i = 0; i < qPlayer.Frachten.AnzEntries(); i++) {
        if (qPlayer.Frachten.IsInAlbum(i) == 0) {
            continue;
        }
        const auto &job = qPlayer.Frachten[i];
        if ((job.VonCity == job.NachCity) || (job.InPlan != 0) || (job.Praemie < 0)) {
            continue;
        }
        mMoneyReservedForFines += job.Strafe;
        num++;
        AT_Info("Freight job %s not planned, fine +%s", Helper::getFreightName(job).c_str(), Insert1000erDots64(job.Strafe).c_str());
    }
    if (mMoneyReservedForFines > 0) {
        AT_Warn("Bot::planFlights(): %d jobs not planned, need to reserve %s $ for future fines, available money: %s $", num,
                Insert1000erDots64(mMoneyReservedForFines).c_str(), Insert1000erDots64(getMoneyAvailable()).c_str());
    }

    forceReplanning();
}

SLONG Bot::replaceAutomaticFlights(SLONG planeId) {
    auto &qPlane = qPlayer.Planes[planeId];
    auto &qFlightPlan = qPlane.Flugplan.Flug;

    bool changedFlightPlan = true;
    SLONG count = 0;
    while (changedFlightPlan) {
        changedFlightPlan = false;
        for (SLONG d = 0; !changedFlightPlan && d < qFlightPlan.AnzEntries(); d++) {
            const auto &qFPE = qFlightPlan[d];
            if (qFPE.ObjectType != 3) {
                continue;
            }
            if (qFPE.Startdate == Sim.Date && qFPE.Startzeit <= Sim.GetHour() + 1) {
                continue;
            }

            SLONG from = Cities.find(qFPE.VonCity);
            SLONG to = Cities.find(qFPE.NachCity);
            PlaneTime startTime{qFPE.Startdate, qFPE.Startzeit};
            PlaneTime endTime{qFPE.Landedate, qFPE.Landezeit};

            for (const auto &iter : mRoutes) {
                const auto &qRoute = getRoute(iter);
                SLONG fromCity = Cities.find(qRoute.VonCity);
                SLONG toCity = Cities.find(qRoute.NachCity);
                if (from != fromCity || to != toCity) {
                    continue;
                }
                if (qPlane.ptReichweite * 1000 < Cities.CalcDistance(fromCity, toCity)) {
                    continue;
                }

                if (!GameMechanic::removeFromFlightPlan(qPlayer, planeId, d)) {
                    AT_Error("Bot::replaceAutomaticFlights(): GameMechanic::removeFromFlightPlan returned error!");
                    return count;
                }
                if (!GameMechanic::planRouteJob(qPlayer, planeId, iter.routeId, startTime.getDate(), startTime.getHour())) {
                    AT_Error("Bot::replaceAutomaticFlights(): GameMechanic::planRouteJob returned error!");
                    return count;
                }
                changedFlightPlan = true;
                count++;
                break;
            }
        }
    }
    return count;
}

std::pair<SLONG, SLONG> Bot::kerosineQualiOptimization(__int64 moneyAvailable, DOUBLE targetFillRatio) const {
    AT_Log("Bot::kerosineQualiOptimization(): Buying kerosine for no more than %lld $ and %.2f %% of capacity", moneyAvailable, targetFillRatio * 100);

    std::pair<SLONG, SLONG> res{};
    DOUBLE priceGood = Sim.HoleKerosinPreis(1);
    DOUBLE priceBad = Sim.HoleKerosinPreis(2);
    DOUBLE tankContent = qPlayer.TankInhalt;
    DOUBLE tankMax = qPlayer.Tank * targetFillRatio;

    DOUBLE qualiZiel = mOptions.kMaxKerosinQualiZiel;
    DOUBLE qualiStart = qPlayer.KerosinQuali;
    DOUBLE amountGood = 0;
    DOUBLE amountBad = 0;

    if (tankContent >= tankMax) {
        return res;
    }

    // Definitions:
    // aG := amountGood, aB := amountBad, mA := moneyAvailable, pG := priceGood, pB := priceBaD,
    // T := tankMax, Ti := tankContent, qS := qualiStart, qZ := qualiZiel
    // Given are the following two equations:
    // I: aG*pG + aB*pB = mA    (spend all money for either good are bad kerosine)
    // II: qZ = (Ti*qS + aB*2 + aG) / (Ti + aB + aG)   (new quality qZ depends on amounts bought)
    // Solve I for aG:
    // aG = (mA - aB*pB) / pG
    // Solve II for aB:
    // qZ*(Ti + aB + aG) = Ti*qS + aB*2 + aG;
    // qZ*Ti + qZ*(aB + aG) = Ti*qS + aB*2 + aG;
    // qZ*Ti - Ti*qS = aB*2 + aG - qZ*(aB + aG);
    // Ti*(qZ - qS) = aB*(2 - qZ) + aG*(1 - qZ);
    // Insert I in II to eliminate aG
    // Ti*(qZ - qS) = aB*(2 - qZ) + ((mA - aB*pB) / pG)*(1 - qZ);
    // Ti*(qZ - qS) = aB*(2 - qZ) + mA*(1 - qZ) / pG - aB*pB*(1 - qZ) / pG;
    // Ti*(qZ - qS) - mA*(1 - qZ) / pG = aB*((2 - qZ) - pB*(1 - qZ) / pG);
    // (Ti*(qZ - qS) - mA*(1 - qZ) / pG) / ((2 - qZ) - pB*(1 - qZ) / pG) = aB;
    DOUBLE nominator = (tankContent * (qualiZiel - qualiStart) - moneyAvailable * (1 - qualiZiel) / priceGood);
    DOUBLE denominator = ((2 - qualiZiel) - priceBad * (1 - qualiZiel) / priceGood);

    // Limit:
    amountBad = std::max(0.0, nominator / denominator);               // equation II
    amountGood = (moneyAvailable - amountBad * priceBad) / priceGood; // equation I
    if (amountGood < 0) {
        amountGood = 0;
        amountBad = moneyAvailable / priceBad;
    }

    // Round:
    if (amountGood > INT_MAX) {
        amountGood = INT_MAX;
    }
    if (amountBad > INT_MAX) {
        amountBad = INT_MAX;
    }
    res.first = static_cast<SLONG>(std::floor(amountGood));
    res.second = static_cast<SLONG>(std::floor(amountBad));

    // we have more than enough money to fill the tank, calculate again using this for equation I:
    // I: aG = (T - Ti - aB)  (cannot exceed tank capacity)
    // Insert I in II
    // Ti*(qZ - qS) = aB*(2 - qZ) + (T - Ti - aB)*(1 - qZ);
    // Ti*(qZ - qS) = aB*(2 - qZ) + (T - Ti)*(1 - qZ) - aB*(1 - qZ);
    // Ti*(qZ - qS) - (T - Ti)*(1 - qZ) = aB*(2 - qZ) - aB*(1 - qZ);
    // Ti*(qZ - qS) - (T - Ti)*(1 - qZ) = aB*((2 - qZ) - (1 - qZ));
    // (Ti*(qZ - qS) - (T - Ti)*(1 - qZ)) / ((2 - qZ) - (1 - qZ)) = aB;
    if (res.first + res.second + tankContent > tankMax) {
        DOUBLE nominator = (tankContent * (qualiZiel - qualiStart) - (tankMax - tankContent) * (1 - qualiZiel));
        DOUBLE denominator = ((2 - qualiZiel) - (1 - qualiZiel));

        // Limit:
        amountBad = std::min(tankMax - tankContent, std::max(0.0, nominator / denominator));
        amountGood = (tankMax - tankContent - amountBad);

        // Round:
        if (amountGood > INT_MAX) {
            amountGood = INT_MAX;
        }
        if (amountBad > INT_MAX) {
            amountBad = INT_MAX;
        }
        res.first = static_cast<SLONG>(std::floor(amountGood));
        res.second = static_cast<SLONG>(std::floor(amountBad));
    }

    return res;
}

bool Bot::determineSabotageMode(__int64 moneyAvailable, SLONG &jobType, SLONG &jobNumber, SLONG &jobHints) {
    std::array<SLONG, 5> hintArray1{2, 4, 10, 20, 100};
    std::array<SLONG, 4> hintArray2{8, 0, 25, 40};
    /* std::array<SLONG, 6> hintArray3{8, 15, 25, 30, 50, 70}; */

    /* decide which sabotage to use. Default: Spiked coffee */
    jobType = 1;
    jobNumber = 1;
    jobHints = hintArray2[jobNumber - 1];
    SLONG jobCost = SabotagePrice2[jobNumber - 1];

    /* sabotage planes to damage enemy stock price in stock price competitions */
    bool stockPriceSabotage = (Sim.Difficulty == DIFF_ADDON08 || Sim.Difficulty == DIFF_ATFS07);
    /* sabotage plane tire to delay next start in miles&more mission */
    bool delaySabotage = (Sim.Difficulty == DIFF_ADDON04);
    if (stockPriceSabotage || delaySabotage) {
        jobType = 0;
        jobNumber = std::min((stockPriceSabotage ? 4 : 3), qPlayer.ArabTrust);
        jobHints = hintArray1[jobNumber - 1];
        jobCost = SabotagePrice[jobNumber - 1];
    }

    /* check preconditions */
    if (mArabHintsTracker + jobHints > kMaxSabotageHints) {
        return false; /* wait until we won't be caught */
    }
    if (jobCost > moneyAvailable) {
        return false; /* wait until we have enough money */
    }
    return true;
}

SLONG Bot::getNumRentedRoutes() const {
    SLONG numRented = 0;
    const auto &qRRouten = qPlayer.RentRouten.RentRouten;
    for (const auto &rentRoute : qRRouten) {
        if (rentRoute.Rang != 0) {
            numRented++;
        }
    }
    return numRented;
}

void Bot::checkLostRoutes() {
    if (mRoutes.empty()) {
        return;
    }

    auto numRented = getNumRentedRoutes();
    assert(numRented % 2 == 0);
    assert(numRented / 2 <= mRoutes.size());
    if (numRented / 2 >= mRoutes.size()) {
        return; /* alles ok */
    }

    AT_Error("We lost %d routes!", mRoutes.size() - numRented / 2);

    std::vector<RouteInfo> routesNew;
    std::vector<SLONG> planesForRoutesNew;
    const auto &qRRouten = qPlayer.RentRouten.RentRouten;
    for (const auto &route : mRoutes) {
        if (qRRouten[route.routeId].Rang != 0) {
            /* route still exists */
            routesNew.emplace_back(route);
            for (auto planeId : route.planeIds) {
                planesForRoutesNew.push_back(planeId);
            }
        } else {
            /* route is gone! move planes from route back into the "unassigned" pile */
            for (auto planeId : route.planeIds) {
                mPlanesForRoutesUnassigned.push_back(planeId);
                GameMechanic::clearFlightPlan(qPlayer, planeId);
                AT_Log("Bot::checkLostRoutes(): Plane %s does not have a route anymore.", Helper::getPlaneName(qPlayer.Planes[planeId]).c_str());
            }
        }
    }
    std::swap(mRoutes, routesNew);
    std::swap(mPlanesForRoutes, planesForRoutesNew);

    mRoutesUtilizationUpdated = false;
    mRoutesNextStep = RoutesNextStep::None;
}

void Bot::updateRouteInfoOffice() {
    /* copy most import information from routes
     * copy information that is available when in office */
    for (auto &route : mRoutes) {
        route.image = getRentRoute(route).Image;
        route.routeOwnUtilization = getRentRoute(route).RoutenAuslastungBot;
        route.planeUtilization = getRentRoute(route).AuslastungBot;
        route.planeUtilizationFC = getRentRoute(route).AuslastungFirstClassBot;

        DOUBLE luxusSumme = 0;
        route.canUpgrade = false;
        for (auto i : route.planeIds) {
            const auto &qPlane = qPlayer.Planes[i];

            SLONG luxusThisPlane = qPlane.Sitze + qPlane.Essen + qPlane.Tabletts + qPlane.Deco;
            luxusThisPlane += qPlane.Triebwerk + qPlane.Reifen + qPlane.Elektronik + qPlane.Sicherheit;

            luxusSumme += luxusThisPlane;
            route.canUpgrade = (luxusThisPlane < 7 * 2);
        }
        luxusSumme /= route.planeIds.size();

        AT_Log("Bot::updateRouteInfoOffice(): Route %s has image=%d and utilization=%d/%d (%d planes with average utilization=%d/%d and luxus=%.2f)",
               Helper::getRouteName(getRoute(route)).c_str(), route.image, route.routeOwnUtilization, route.routeUtilization, route.planeIds.size(),
               route.planeUtilization, route.planeUtilizationFC, luxusSumme);
    }

    mRoutesSortedByOwnUtilization.resize(mRoutes.size());
    if (!mRoutes.empty()) {
        /* sort routes by utilization and find route with lowest image */
        SLONG lowImage = 0;
        for (SLONG i = 0; i < mRoutes.size(); i++) {
            mRoutesSortedByOwnUtilization[i] = i;

            if (mRoutes[i].image < mRoutes[lowImage].image) {
                lowImage = i;
            }
        }
        std::sort(mRoutesSortedByOwnUtilization.begin(), mRoutesSortedByOwnUtilization.end(),
                  [&](SLONG a, SLONG b) { return mRoutes[a].routeOwnUtilization < mRoutes[b].routeOwnUtilization; });

        auto lowUtil = mRoutesSortedByOwnUtilization[0];
        AT_Log("Bot::updateRouteInfoOffice(): Route %s has lowest image: %d", Helper::getRouteName(getRoute(mRoutes[lowImage])).c_str(),
               mRoutes[lowImage].image);
        AT_Log("Bot::updateRouteInfoOffice(): Route %s has lowest utilization: %d/%d", Helper::getRouteName(getRoute(mRoutes[lowUtil])).c_str(),
               mRoutes[lowUtil].routeOwnUtilization, mRoutes[lowUtil].routeUtilization);
    }

    /* idle planes? */
    if (!mPlanesForRoutesUnassigned.empty()) {
        AT_Log("Bot::updateRouteInfoOffice(): There are %lu unassigned planes with no route ", mPlanesForRoutesUnassigned.size());
    }

    /* generate strategy for routes */
    mRoutesUpdated = true;
    routesRecalcNextStep();
}

void Bot::updateRouteInfoBoard() {
    /* copy most import information from routes
     * copy information that is available when visiting the route board */
    for (auto &route : mRoutes) {
        route.image = getRentRoute(route).Image;
        route.routeOwnUtilization = getRentRoute(route).RoutenAuslastungBot;
        route.routeUtilization = 0;
        for (SLONG i = 0; i < Sim.Players.Players.AnzEntries(); i++) {
            const auto &qqPlayer = Sim.Players.Players[i];
            if (qqPlayer.IsOut != 0) {
                continue;
            }
            const auto &qRentRoute = qqPlayer.RentRouten.RentRouten[route.routeId];
            route.routeUtilization += qRentRoute.RoutenAuslastungBot;

            if (qRentRoute.RoutenAuslastungBot > 0 && i != qPlayer.PlayerNum) {
                AT_Log("Bot::updateRouteInfoBoard(): Route %s: We (%d utilization) are competing with %s (%d utilization)",
                       Helper::getRouteName(getRoute(route)).c_str(), route.routeOwnUtilization, qqPlayer.AirlineX.c_str(), qRentRoute.RoutenAuslastungBot);
            }
        }
        AT_Log("Bot::updateRouteInfoBoard(): Route %s has utilization=%d/%d (%d planes with average utilization=%d/%d)",
               Helper::getRouteName(getRoute(route)).c_str(), route.routeOwnUtilization, route.routeUtilization, route.planeIds.size(), route.planeUtilization,
               route.planeUtilizationFC);
    }

    /* generate strategy for routes */
    mRoutesUtilizationUpdated = true;
    routesRecalcNextStep();
}

void Bot::routesRecalcNextStep() {
    mRoutesNextStep = RoutesNextStep::None;
    if (!mRoutesUpdated || !mRoutesUtilizationUpdated) {
        AT_Log("Bot::routesRecalcNextStep(): Route information not updated, cannot generate route strategy yet");
        return;
    }

    std::tie(mRoutesNextStep, mImproveRouteId) = routesFindNextStep();
    mWantToRentRouteId = (mRoutesNextStep == RoutesNextStep::RentNewRoute) ? mImproveRouteId : -1;

    std::string routeName;
    if (mImproveRouteId != -1) {
        routeName = Helper::getRouteName(getRoute(mRoutes[mImproveRouteId]));
    }

    switch (mRoutesNextStep) {
    case RoutesNextStep::None:
        AT_Error("Bot::routesRecalcNextStep(): No strategy!");
        break;
    case RoutesNextStep::RentNewRoute:
        AT_Log("Bot::routesRecalcNextStep(): We will rent a new route");
        break;
    case RoutesNextStep::BuyMorePlanes:
        mBuyPlaneForRouteId = mRoutes[mImproveRouteId].planeTypeId;
        /* if RoutesNextStep changes to something else, we won't reset
         * mBuyPlaneForRouteId so that we keep hiring new employees. */
        AT_Log("Bot::routesRecalcNextStep(): Need to buy another %s for route %s", PlaneTypes[mBuyPlaneForRouteId].Name.c_str(), routeName.c_str());
        break;
    case RoutesNextStep::BuyAdsForRoute:
        AT_Log("Bot::routesRecalcNextStep(): Need to buy ads for route %s with image %d", routeName.c_str(), mRoutes[mImproveRouteId].image);
        break;
    case RoutesNextStep::UpgradePlanes:
        AT_Log("Bot::routesRecalcNextStep(): Need to upgrade planes of route %s", routeName.c_str());
        break;
    case RoutesNextStep::ImproveAirlineImage:
        AT_Log("Bot::routesRecalcNextStep(): Need to improve airline image");
        break;
    default:
        AT_Error("Bot::routesRecalcNextStep(): Default case should not be reached.");
        DebugBreak();
    }
}

std::pair<Bot::RoutesNextStep, SLONG> Bot::routesFindNextStep() const {
    assert(mDoRoutes);
    assert(mRoutesUpdated && mRoutesUtilizationUpdated);

    /* find route with lowest utilization that can be improved */
    SLONG routeToImprove = -1;
    for (auto i : mRoutesSortedByOwnUtilization) {
        if (mRoutes[i].routeUtilization < 90 && mRoutes[i].routeOwnUtilization < mOptions.kMaximumRouteUtilization) {
            routeToImprove = i;
            break;
        }
    }

    /* Step 1: Is the default, at the bottom */

    if (routeToImprove != -1) {
        const auto &qRoute = mRoutes[routeToImprove];

        /* Step 2: Buy first plane for underutilized route */
        if (qRoute.planeIds.empty()) {
            return {RoutesNextStep::BuyMorePlanes, routeToImprove};
        }

        /* Step 3: Increase route image if planes underutilized */
        if (calcRouteImageNeeded(qRoute) > 0 && qRoute.planeUtilization < kMaximumPlaneUtilization) {
            return {RoutesNextStep::BuyAdsForRoute, routeToImprove};
        }

        /* Step 4: Buy enough planes to not loose route */
        if (qRoute.routeOwnUtilization < kMinimumOwnRouteUtilization) {
            return {RoutesNextStep::BuyMorePlanes, routeToImprove};
        }

        /* Step 5: Now we can upgrade the plane for first class passengers */
        if (qRoute.canUpgrade) {
            return {RoutesNextStep::UpgradePlanes, routeToImprove};
        }

        /* Step 6: Planes are all upgraded, buy next one */
        return {RoutesNextStep::BuyMorePlanes, routeToImprove};
    }

    /* Step 7: Improve airline image when we have one fully utilized route */
    if (!mRoutes.empty() && getImage() < 800) {
        return {RoutesNextStep::ImproveAirlineImage, -1};
    }

    /* Step 1: No routes underutilized, rent new route */
    return {RoutesNextStep::RentNewRoute, -1};
}

void Bot::requestPlanRoutes(bool areWeInOffice) {
    auto res = howToPlanFlights();
    if (res == HowToPlan::Laptop) {
        AT_Log("Bot::requestPlanRoutes(): Planning using laptop");
        planRoutes();
    } else {
        AT_Log("Bot::requestPlanRoutes(): No laptop, need to go to office");
        mNeedToPlanRoutes = true;
        forceReplanning();
    }

    if (res == HowToPlan::Office && areWeInOffice) {
        AT_Log("Bot::requestPlanRoutes(): Already in office, planning right now");
        planRoutes();
        mNeedToPlanRoutes = false;
    }
}

void Bot::findBestRoute() {
    auto isBuyable = GameMechanic::getBuyableRoutes(qPlayer);
    auto bestPlanes = findBestAvailablePlaneType(true, false);

    mWantToRentRouteId = -1;
    mPlaneTypeForNewRoute = -1;
    mPlanesForNewRoute.clear();

    /* check existing planes */
    std::vector<std::pair<SLONG, __int64>> existingPlaneIds;
    if (mRoutes.empty()) {
        for (const auto id : mPlanesForRoutesUnassigned) {
            auto &qPlane = qPlayer.Planes[id];
            __int64 score = qPlane.ptPassagiere * qPlane.ptPassagiere / qPlane.ptVerbrauch;
            existingPlaneIds.emplace_back(id, score);
        }
        std::sort(existingPlaneIds.begin(), existingPlaneIds.end(),
                  [](const std::pair<SLONG, __int64> &a, const std::pair<SLONG, __int64> &b) { return a.second > b.second; });
    }

    std::vector<RouteScore> bestRoutes;
    for (SLONG c = 0; c < Routen.AnzEntries(); c++) {
        if (isBuyable[c] == 0) {
            continue;
        }

        SLONG distance = Cities.CalcDistance(Routen[c].VonCity, Routen[c].NachCity);

        SLONG planeTypeId = -1;
        for (SLONG i : bestPlanes) {
            SLONG duration = Cities.CalcFlugdauer(Routen[c].VonCity, Routen[c].NachCity, PlaneTypes[i].Geschwindigkeit);
            if (distance <= PlaneTypes[i].Reichweite * 1000 && duration < 24) {
                planeTypeId = i;
                break;
            }
        }

        /* also check existing planes if they can be used for routes */
        std::vector<SLONG> useExistingPlaneId;
        for (const auto &i : existingPlaneIds) {
            auto &qPlane = qPlayer.Planes[i.first];
            SLONG duration = Cities.CalcFlugdauer(Routen[c].VonCity, Routen[c].NachCity, qPlane.ptGeschwindigkeit);
            if (distance <= qPlane.ptReichweite * 1000 && duration < 24) {
                useExistingPlaneId.push_back(i.first);
            }
        }

        if (planeTypeId == -1) {
            continue;
        }

        /* calc score for route (more passengers always good, longer routes tend to be also more worth it) */
        DOUBLE score = Routen[c].AnzPassagiere();
        if (!qPlayer.RobotUse(ROBOT_USE_SHORTFLIGHTS)) {
            score *= (Cities.CalcDistance(Routen[c].VonCity, Routen[c].NachCity) / 1000.0);
        }
        score /= Routen[c].Miete;

        /* current utilization of route */
        SLONG routeUtilization = 0;
        for (SLONG i = 0; i < Sim.Players.Players.AnzEntries(); i++) {
            const auto &qqPlayer = Sim.Players.Players[i];
            if (qqPlayer.IsOut == 0) {
                const auto &qRentRoute = qqPlayer.RentRouten.RentRouten[c];
                routeUtilization += qRentRoute.RoutenAuslastungBot;
            }
        }
        if (routeUtilization > 0) {
            routeUtilization += 20; /* offset, we can expect the enemy to increase their share */
            routeUtilization = std::min(100, routeUtilization);
        }

        /* adjust score based on utilization */
        auto scoreOld = score;
        score = score * (100.0 - routeUtilization) / 100.0;
        if (std::abs(scoreOld - score) > 0.01) {
            AT_Log("Bot::actionFindBestRoute(): Route %s is already used, reducing score: %.2f => %.2f", Helper::getRouteName(Routen[c]).c_str(), scoreOld,
                   score);
        }

        /* is this route important for our mission */
        if (qPlayer.RobotUse(ROBOT_USE_ROUTEMISSION)) {
            auto homeAirport = static_cast<ULONG>(Sim.HomeAirportId);
            for (SLONG d = 0; d < 6; d++) {
                auto missionCity = static_cast<ULONG>(Sim.MissionCities[d]);
                if ((Routen[c].VonCity == homeAirport && Routen[c].NachCity == missionCity) ||
                    (Routen[c].NachCity == homeAirport && Routen[c].VonCity == missionCity)) {

                    AT_Log("Bot::actionFindBestRoute(): Route %s is important for mission, increasing score.", Helper::getRouteName(Routen[c]).c_str());
                    score *= 10;
                }
            }
        }

        /* calculate how many planes would be need to get desired route utilization */
        /* TODO: What about factor 4.27 */
        SLONG duration = kDurationExtra + Cities.CalcFlugdauer(Routen[c].VonCity, Routen[c].NachCity, PlaneTypes[planeTypeId].Geschwindigkeit);
        SLONG roundTripDuration = 2 * duration;
        SLONG numTripsPerWeek = 24 * 7 / roundTripDuration;
        SLONG passengersPerWeek = 7 * Routen[c].AnzPassagiere();
        SLONG minTarget = ceil_div(passengersPerWeek * 10, 100); /* to not loose the route */
        SLONG finalTarget = ceil_div(passengersPerWeek * mOptions.kMaximumRouteUtilization, 100);
        SLONG numPlanesTarget = ceil_div(minTarget, numTripsPerWeek * PlaneTypes[planeTypeId].Passagiere);
        SLONG numPlanesTotal = ceil_div(finalTarget, numTripsPerWeek * PlaneTypes[planeTypeId].Passagiere);

        if (qPlayer.RobotUse(ROBOT_USE_SHORTFLIGHTS)) {
            const auto target = BTARGET_PASSAVG * 6 / 5; /* need to transport X passengers each day (plus margin) */
            if ((24 / duration) * PlaneTypes[planeTypeId].Passagiere < target) {
                continue;
            }
        }

        /* account for the fact that we already have suitable planes */
        if (useExistingPlaneId.size() > numPlanesTotal) {
            /* only have to use the best n planes */
            useExistingPlaneId.resize(numPlanesTotal);
        }
        SLONG planesToBuy = std::max(0, numPlanesTarget - static_cast<SLONG>(useExistingPlaneId.size()));

        bestRoutes.emplace_back(RouteScore{score, c, planeTypeId, useExistingPlaneId, planesToBuy});
    }

    /* sort routes by score */
    std::sort(bestRoutes.begin(), bestRoutes.end());
    for (const auto &candidate : bestRoutes) {
        if (!candidate.planeId.empty()) {
            AT_Log("Bot::actionFindBestRoute(): Score of route %s (using %d existing planes, need %d) is: %.2f",
                   Helper::getRouteName(Routen[candidate.routeId]).c_str(), candidate.planeId.size(), candidate.numPlanesToBuy, candidate.score);
        } else {
            AT_Log("Bot::actionFindBestRoute(): Score of route %s (using plane type %s, need %d) is: %.2f",
                   Helper::getRouteName(Routen[candidate.routeId]).c_str(), PlaneTypes[candidate.planeTypeId].Name.c_str(), candidate.numPlanesToBuy,
                   candidate.score);
        }
    }

    /* pick best route we can afford */
    __int64 moneyAvailable = qPlayer.Money + getWeeklyOpSaldo();
    for (const auto &candidate : bestRoutes) {
        __int64 planeCost = PlaneTypes[candidate.planeTypeId].Preis;
        if (candidate.numPlanesToBuy * planeCost > moneyAvailable) {
            AT_Log("Bot::actionFindBestRoute(): We cannot afford route %s (plane costs %lld, need %d), our available money is %lld",
                   Helper::getRouteName(Routen[candidate.routeId]).c_str(), planeCost, candidate.numPlanesToBuy, moneyAvailable);
            continue;
        }
        AT_Log("Bot::actionFindBestRoute(): Best route (using plane type %s) is: ", PlaneTypes[candidate.planeTypeId].Name.c_str());
        Helper::printRoute(Routen[candidate.routeId]);

        mWantToRentRouteId = candidate.routeId;
        mPlaneTypeForNewRoute = candidate.planeTypeId; /* buy new plane */
        mPlanesForNewRoute = candidate.planeId;        /* use existing planes */
        return;
    }

    AT_Log("Bot::actionFindBestRoute(): No routes match criteria.");
}

void Bot::planRoutes() {
    mNeedToPlanRoutes = false;
    if (mRoutes.empty()) {
        return;
    }

    /* plan route flights */
    for (auto &qRoute : mRoutes) {
        SLONG fromCity = Cities.find(getRoute(qRoute).VonCity);
        SLONG toCity = Cities.find(getRoute(qRoute).NachCity);
        auto routeIdA = Routen.GetIdFromIndex(qRoute.routeId);
        auto routeIdB = Routen.GetIdFromIndex(qRoute.routeReverseId);

        SLONG timeSlotIdx = 0;
        for (auto planeId : qRoute.planeIds) {
            const auto &qPlane = qPlayer.Planes[planeId];
            SLONG duration = kDurationExtra + Cities.CalcFlugdauer(fromCity, toCity, qPlane.ptGeschwindigkeit);
            assert(duration == kDurationExtra + Cities.CalcFlugdauer(toCity, fromCity, qPlane.ptGeschwindigkeit));
            SLONG maxNumTimeSlots = (2 * duration) / 3;

#ifdef PRINT_ROUTE_DETAILS
            AT_Log("Bot::planRoutes(): =================== Plane %s ===================", Helper::getPlaneName(qPlane).c_str());
            Helper::printFlightJobs(qPlayer, planeId);
#endif

            /* where is the plane right now and when can it be in the origin city? */
            PlaneTime scheduleFromTime = {Sim.Date, Sim.GetHour() + 2};
            PlaneTime availTime;
            SLONG availCity{};
            std::tie(availTime, availCity) = Helper::getPlaneAvailableTimeLoc(qPlane, scheduleFromTime, scheduleFromTime);
            availCity = Cities.find(availCity);
#ifdef PRINT_ROUTE_DETAILS
            AT_Log("Bot::planRoutes(): Plane %s is in %s @ %s %d", Helper::getPlaneName(qPlane).c_str(), Cities[availCity].Kuerzel.c_str(),
                   Helper::getWeekday(availTime.getDate()).c_str(), availTime.getHour());
#endif

            /* planes on same route fly with 3 hours inbetween */
            SLONG offset = 3 * ((timeSlotIdx++) % maxNumTimeSlots);
            SLONG hours = availTime.getDate() * 24 + availTime.getHour();
            SLONG timeSlot = ceil_div(hours - offset, duration);
            bool useRouteA = (0 == (timeSlot % 2));
            hours = timeSlot * duration + offset;
            PlaneTime startTime = {hours / 24, hours % 24};

            /* find insertion point */
            const auto &qFlightPlan = qPlane.Flugplan.Flug;
            SLONG numCorrectlyScheduled = 0;
            for (SLONG d = 0; d < qFlightPlan.AnzEntries(); d++) {
                const auto &qFPE = qFlightPlan[d];
                if (PlaneTime{qFPE.Startdate, qFPE.Startzeit} < startTime) {
                    continue;
                }
                /* check whether this FPE is correct */
                if (PlaneTime{qFPE.Startdate, qFPE.Startzeit} > startTime) {
                    break;
                }
                if (qFPE.ObjectType != 1) {
                    break;
                }
                if ((useRouteA && qFPE.ObjectId != routeIdA) || (!useRouteA && qFPE.ObjectId != routeIdB)) {
                    break;
                }
                numCorrectlyScheduled++;
                useRouteA = !useRouteA;
                startTime += duration;
            }
            AT_Log("Bot::planRoutes(): Plane %s: %d instances of route %s were correctly scheduled (until %s %d)", Helper::getPlaneName(qPlane).c_str(),
                   numCorrectlyScheduled, Helper::getRouteName(getRoute(qRoute)).c_str(), Helper::getWeekday(startTime.getDate()).c_str(), startTime.getHour());

            /* plane is not on the route yet */
            if (numCorrectlyScheduled == 0 && availCity != fromCity) {
                /* leave room for auto flight */
                SLONG autoFlightDuration = kDurationExtra + Cities.CalcFlugdauer(availCity, fromCity, qPlane.ptGeschwindigkeit);
                availTime += autoFlightDuration;
                while (startTime < availTime) {
                    startTime += 2 * duration;
                }
                AT_Log("Bot::planRoutes(): Plane %s: Adding buffer of %d hours for auto flight from %s to %s", Helper::getPlaneName(qPlane).c_str(),
                       autoFlightDuration, Cities[availCity].Kuerzel.c_str(), Cities[fromCity].Kuerzel.c_str());
            }

            if (startTime.getDate() >= Sim.Date + 6) {
                continue;
            }

            /* kill everyting after insertion point */
            GameMechanic::clearFlightPlanFrom(qPlayer, planeId, startTime.getDate(), startTime.getHour());

            /* schedule route jobs */
            SLONG numScheduled = 0;
            auto currentTime = startTime;
            while (currentTime.getDate() < Sim.Date + 6) {
                auto routeId = useRouteA ? qRoute.routeId : qRoute.routeReverseId;
                if (!GameMechanic::planRouteJob(qPlayer, planeId, routeId, currentTime.getDate(), currentTime.getHour())) {
                    AT_Error("Bot::planRoutes(): GameMechanic::planRouteJob returned error!");
                    return;
                }
                numScheduled++;
                useRouteA = !useRouteA;
                currentTime += duration;
            }
            AT_Log("Bot::planRoutes(): Scheduled route %s %d times for plane %s, starting at %s %d", Helper::getRouteName(getRoute(qRoute)).c_str(),
                   numScheduled, Helper::getPlaneName(qPlane).c_str(), Helper::getWeekday(currentTime.getDate()).c_str(), currentTime.getHour());
            Helper::checkPlaneSchedule(qPlayer, planeId, false);
        }
    }
    Helper::checkFlightJobs(qPlayer, false, false);

    /* adjust ticket prices */
    for (auto &qRoute : mRoutes) {
        if (qRoute.planeIds.empty()) {
            continue;
        }

        SLONG priceOld = getRentRoute(qRoute).Ticketpreis;
        DOUBLE factorOld = qRoute.ticketCostFactor;
        SLONG costs = CalculateFlightCost(getRoute(qRoute).VonCity, getRoute(qRoute).NachCity, 800, 800, -1) * 3 / 180 * 2;
        if (qRoute.planeUtilization > kMaximumPlaneUtilization) {
            qRoute.ticketCostFactor += 0.1;
        } else {
            /* planes are not fully utilized */
            assert(qRoute.routeUtilization >= 0);
            if (qRoute.routeUtilization < mOptions.kMaximumRouteUtilization) {
                /* decrease one time per each 25% missing */
                SLONG numDecreases = ceil_div(kMaximumPlaneUtilization - qRoute.planeUtilization, 25);
                qRoute.ticketCostFactor -= (0.1 * numDecreases);
            }
        }
        Limit(0.5, qRoute.ticketCostFactor, mOptions.kMaxTicketPriceFactor);

        SLONG priceNew = costs * qRoute.ticketCostFactor;
        priceNew = priceNew / 10 * 10;
        if (std::abs(factorOld - qRoute.ticketCostFactor) > 0.05) {
            GameMechanic::setRouteTicketPriceBoth(qPlayer, qRoute.routeId, priceNew, priceNew * 2);
            AT_Log("Bot::planRoutes(): Changing ticket price factor for route %s: %.2f => %.2f (%d => %d)", Helper::getRouteName(getRoute(qRoute)).c_str(),
                   factorOld, qRoute.ticketCostFactor, priceOld, priceNew);
        }
    }
}

void Bot::assignPlanesToRoutes(bool areWeInOffice) {
    if (mRoutes.empty()) {
        return;
    }

    /* assign planes to routes */
    SLONG numUnassigned = mPlanesForRoutesUnassigned.size();
    for (SLONG i = 0; i < numUnassigned; i++) {
        if (mRoutes[mRoutesSortedByOwnUtilization[0]].routeUtilization >= mOptions.kMaximumRouteUtilization) {
            break; /* No more underutilized routes */
        }

        SLONG planeId = mPlanesForRoutesUnassigned.front();
        const auto &qPlane = qPlayer.Planes[planeId];
        mPlanesForRoutesUnassigned.pop_front();

        if (!checkPlaneAvailable(planeId, true, areWeInOffice)) {
            mPlanesForRoutesUnassigned.push_back(planeId);
            continue;
        }

        SLONG targetRouteIdx = -1;
        for (SLONG routeIdx : mRoutesSortedByOwnUtilization) {
            auto &qRoute = mRoutes[routeIdx];
            if (qRoute.routeUtilization >= mOptions.kMaximumRouteUtilization) {
                break; /* No more underutilized routes */
            }
            if (qRoute.planeTypeId != qPlane.TypeId) {
                continue;
            }
            targetRouteIdx = routeIdx;
            break;
        }
        if (targetRouteIdx != -1) {
            auto &qRoute = mRoutes[targetRouteIdx];
            qRoute.planeIds.push_back(planeId);
            mPlanesForRoutes.push_back(planeId);
            AT_Log("Bot::assignPlanesToRoutes(): Assigning plane %s to route %s", Helper::getPlaneName(qPlane).c_str(),
                   Helper::getRouteName(getRoute(qRoute)).c_str());
        } else {
            mPlanesForRoutesUnassigned.push_back(planeId);
        }
    }
}
