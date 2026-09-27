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
#include <map>
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

inline SLONG getRouteBaseCost(const CRoute &qRoute) { return CalculateFlightCost(qRoute.VonCity, qRoute.NachCity, 800, 800, -1) * 3 / 180 * 2; }

void Bot::grabNewFlights() {
    /* this will cause the planning algo to assume that we have not checked these today */
    mLastTimeInRoom.erase(ACTION_CALL_INTERNATIONAL);
    mLastTimeInRoom.erase(ACTION_CALL_INTER_HANDY);
    mLastTimeInRoom.erase(ACTION_CHECKAGENT1);
    mLastTimeInRoom.erase(ACTION_CHECKAGENT2);
    mLastTimeInRoom.erase(ACTION_CHECKAGENT3);
}

__int64 Bot::getNemesisScore(SLONG p) const {
    __int64 score = 0;
    auto &qTarget = Sim.Players.Players[p];
    if (Sim.Difficulty == DIFF_FREEGAME) {
        score = qTarget.BilanzWoche.Hole().GetOpSaldo();
    } else {
        /* for missions */
        if (Sim.Difficulty == DIFF_FINAL || Sim.Difficulty == DIFF_ADDON10) {
            /* better than GetMissionRating(): Calculate sum of part cost instead of just number of parts */
            const auto &qPrices = (Sim.Difficulty == DIFF_FINAL) ? RocketPrices : StationPrices;
            auto nParts = qPrices.size();
            for (SLONG i = 0; i < nParts; i++) {
                if (qTarget.CheckRocketPart(i)) {
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

    return score;
}

void Bot::determineNemesis() {
    auto nemesisOld = mNemesis;

    mNemesis = -1;
    mNemesisScore = 0;
    auto nemesisSabotaged = std::exchange(mNemesisSabotaged, -1);

    bool mayCalculateNemesisScore = true;
    if (Sim.Difficulty == DIFF_FREEGAME) {
        if (qPlayer.HasBerater(BERATERTYP_GELD) < 50) {
            AT_Log("Bot::determineNemesis(): Need to hire financial advisor first");
            mayCalculateNemesisScore = false;
        }
        if (qPlayer.HasBerater(BERATERTYP_INFO) < 50) {
            AT_Log("Bot::determineNemesis(): Need to hire spy first");
            mayCalculateNemesisScore = false;
        }
    }

    /* nemesis mode: Pick host if we cannot calculate scores */
    if (qPlayer.RobotUse(ROBOT_USE_EXTRA_SABOTAGE) && Sim.Players.Players[Sim.localPlayer].IsOut == 0) {
        mNemesis = Sim.localPlayer;
        if (mayCalculateNemesisScore) {
            mNemesisScore = getNemesisScore(mNemesis);
        }
        AT_Log("Bot::determineNemesis(): Nemesis mode: Targeting human player (default).");
    }

    if (!mayCalculateNemesisScore) {
        return;
    }

    /* check scores */
    SLONG enemiesBetterThanMe = 0;
    __int64 myScore = getNemesisScore(qPlayer.PlayerNum);
    std::vector<std::pair<SLONG, __int64>> scores;
    for (SLONG p = 0; p < 4; p++) {
        auto &qTarget = Sim.Players.Players[p];
        if (p == qPlayer.PlayerNum || qTarget.IsOut != 0) {
            continue;
        }
        if (qPlayer.RobotUse(ROBOT_USE_EXTRA_SABOTAGE) && (qTarget.Owner != 0) && (qTarget.Owner != 2)) {
            continue;
        }

        __int64 score = getNemesisScore(p);
        scores.push_back({p, score});
        if (score > myScore) {
            enemiesBetterThanMe++;
        }
    }
    mMood = enemiesBetterThanMe;
    AT_Log("Bot::determineNemesis(): Our score is %s, this puts us on place %d", Insert1000erDots64(myScore).c_str(), enemiesBetterThanMe + 1);
    if (scores.empty()) {
        AT_Log("Bot::determineNemesis(): No enemies found.");
        return;
    }

    /* find best enemy */
    std::sort(scores.begin(), scores.end(), [](const auto &a, const auto &b) { return std::get<1>(a) > std::get<1>(b); });

    /* target best enemy (nemesis mode: best human enemy) */
    mNemesis = scores.front().first;
    mNemesisScore = scores.front().second;
    if (mNemesis == nemesisSabotaged && scores.size() > 1) {
        mNemesis = scores[1].first;
        mNemesisScore = scores[1].second;
    }
    if (mNemesis < 0) {
        return;
    }

    for (const auto &[p, score] : scores) {
        if (p == mNemesis && p == nemesisOld) {
            AT_Log("Bot::determineNemesis(): %s has score %s [remains our nemesis]", Sim.Players.Players[p].AirlineX.c_str(),
                   Insert1000erDots64(score).c_str());
        } else if (p == mNemesis) {
            AT_Log("Bot::determineNemesis(): %s has score %s [new nemesis]", Sim.Players.Players[p].AirlineX.c_str(), Insert1000erDots64(score).c_str());
        } else if (p == nemesisOld) {
            AT_Log("Bot::determineNemesis(): %s has score %s [old nemesis]", Sim.Players.Players[p].AirlineX.c_str(), Insert1000erDots64(score).c_str());
        } else {
            AT_Log("Bot::determineNemesis(): %s has score %s", Sim.Players.Players[p].AirlineX.c_str(), Insert1000erDots64(score).c_str());
        }
    }

    if (nemesisOld != mNemesis) {
        AT_Log("Bot::determineNemesis(): Our nemesis now is %s with a score of %s", Sim.Players.Players[mNemesis].AirlineX.c_str(),
               Insert1000erDots64(mNemesisScore).c_str());
    } else {
        AT_Log("Bot::determineNemesis(): Our nemesis is still %s with a score of %s", Sim.Players.Players[mNemesis].AirlineX.c_str(),
               Insert1000erDots64(mNemesisScore).c_str());
    }
}

void Bot::switchToFinalTarget(bool areWeInOffice) {
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
            if (!qPlayer.CheckRocketPart(i)) {
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
    } else if (qPlayer.RobotUse(ROBOT_USE_LUXERY) && Sim.Difficulty == DIFF_ADDON05 && areWeInOffice) {
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
    } else if (qPlayer.RobotUse(ROBOT_USE_LUXERY) && Sim.Difficulty == DIFF_ATFS02 && areWeInOffice) {
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

std::vector<SLONG> Bot::findBestAvailablePlaneType() {
    auto list = GameMechanic::getAvailablePlaneTypes();
    if (list.empty()) {
        AT_Warn("Bot::findBestAvailablePlaneType(): No plane types known yet.");
        return {};
    }
    if (list == mKnownPlaneTypes) {
        AT_Log("Bot::findBestAvailablePlaneType(): %d available (no new types)", mKnownPlaneTypes.size());
        return {};
    }

    mKnownPlaneTypes = list;
    AT_Log("Bot::findBestAvailablePlaneType(): Checking available plane types: %d available", mKnownPlaneTypes.size());

    std::vector<std::pair<SLONG, DOUBLE>> scores;
    for (const auto &i : mKnownPlaneTypes) {
        if (!PlaneTypes.IsInAlbum(i)) {
            continue;
        }
        const auto &planeType = PlaneTypes[i];

        DOUBLE score = 1.0; /* multiplication (geometric mean) because values have wildly different ranges */
        score = 1.0 * planeType.Passagiere;
        score *= planeType.Reichweite;
        score /= planeType.Verbrauch;

        scores.emplace_back(i, score);
    }
    std::sort(scores.begin(), scores.end(), [](const std::pair<SLONG, DOUBLE> &a, const std::pair<SLONG, DOUBLE> &b) { return a.second > b.second; });

    /* build list */
    std::vector<SLONG> bestList;
    bestList.reserve(scores.size());

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
    auto res = howToPlanFlightsLaptopFix();
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
        planer.setConstBonus(-1000 * 1000);
        planer.setFreeFreightBonus(500 * 1000);
    } else if (qPlayer.RobotUse(ROBOT_USE_MUCH_FRACHT)) {
        planer.setConstBonus(-1000 * 1000);
        planer.setFreightBonus(200 * 1000);
    } else if (qPlayer.RobotUse(ROBOT_USE_RUN_FRACHT)) {
        planer.setFreightBonus(10 * 1000);
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

    if (mPlanerSolution.gain > 1e6) {
        mMood = 0;
    } else if (mPlanerSolution.gain > 1e5) {
        mMood = 1;
    } else if (mPlanerSolution.gain > 1e4) {
        mMood = 2;
    } else {
        mMood = 3;
    }
}

void Bot::requestPlanFlights(bool areWeInOffice) {
    auto res = howToPlanFlightsLaptopFix();
    if (res == HowToPlan::Laptop) {
        AT_Log("Bot::requestPlanFlights(): Planning using laptop");
        planFlights();
        mNeedToPlanJobs = false;
    } else if (res == HowToPlan::Office && areWeInOffice) {
        AT_Log("Bot::requestPlanFlights(): Already in office, planning right now");
        planFlights();
        mNeedToPlanJobs = false;
    } else {
        AT_Log("Bot::requestPlanFlights(): No laptop, need to go to office");
        mNeedToPlanJobs = true;
        forceReplanning();
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
    if (count > 0) {
        AT_Log("Bot::planFlights(): Replaced %d automatic flights with routes", count);
    }
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
                if (iter.planeTypeId == -1) {
                    continue; /* route will be removed */
                }
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

SabotageMode Bot::determineSabotageMode(__int64 moneyAvailable, bool print) {
    std::map<SabotageMode, int> candidates;

    if (qPlayer.RobotUse(ROBOT_USE_EXTREME_SABOTAGE)) {
        /* special mode for specific missions */
        bool stockPriceSabotage = (Sim.Difficulty == DIFF_ADDON08 || Sim.Difficulty == DIFF_ATFS07);
        bool delaySabotage = (Sim.Difficulty == DIFF_ADDON04);
        if (stockPriceSabotage) {
            /* sabotage planes to damage enemy stock price in stock price competitions */
            candidates[SabotageMode::Plane::EngineBreakdown] = 10;
        } else if (delaySabotage) {
            /* sabotage plane tire to delay next start in miles&more mission */
            candidates[SabotageMode::Plane::FlatTire] = 10;
        } else {
            /* use sabotage with low hint count to sabotage as often as possible: weight = 10 - hints */
            candidates[SabotageMode::Plane::SaltedFood] = 8;
            candidates[SabotageMode::Plane::MovieTheatreBreak] = 6;
            candidates[SabotageMode::Personal::CoffeeBacteria] = 2;
            candidates[SabotageMode::Personal::NotebookVirus] = 10;
            candidates[SabotageMode::Special::AircraftBrochures] = 2;
        }
    } else { /* regular mode */
        /* check some conditions for what could be a good sabotage */
        bool earlyGame = (Sim.Date < 15);
        bool nemesisBroke = false;
        if ((mNemesis != -1) && (qPlayer.HasBerater(BERATERTYP_INFO) > 0)) {
            nemesisBroke = (Sim.Players.Players[mNemesis].Money < 1e6);
        }
        bool nemesisManyOffices = false;
        if ((mNemesis != -1) && (qPlayer.HasBerater(BERATERTYP_INFO) >= 50)) {
            SLONG numOffices = Sim.Players.Players[mNemesis].Statistiken[STAT_NIEDERLASSUNGEN].GetAtPastDay(0);
            nemesisManyOffices = (numOffices >= 5);
        }
        bool nemesisHasRoutes = false;
        if ((mNemesis != -1) && (qPlayer.HasBerater(BERATERTYP_INFO) >= 40)) {
            SLONG numRoutes = Sim.Players.Players[mNemesis].Statistiken[STAT_ROUTEN].GetAtPastDay(0);
            nemesisHasRoutes = (numRoutes >= 4);
        }
        bool routeTheftPossible = (!earlyGame && (mRouteToSteal != -1));

        // Plane sabotage candidates
        candidates[SabotageMode::Plane::SaltedFood] = 1;
        candidates[SabotageMode::Plane::MovieTheatreBreak] = 1;
        candidates[SabotageMode::Plane::FlatTire] = 5;
        candidates[SabotageMode::Plane::EngineBreakdown] = 10;
        candidates[SabotageMode::Plane::PlaneCrash] = 0;
        // Personal sabotage candidates
        candidates[SabotageMode::Personal::CoffeeBacteria] = (earlyGame ? 10 : 1);
        candidates[SabotageMode::Personal::NotebookVirus] = (earlyGame ? 10 : 1);
        candidates[SabotageMode::Personal::OfficeBomb] = (earlyGame ? 10 : 1);
        candidates[SabotageMode::Personal::ProvokeStrike] = 5;
        // Special candidates
        candidates[SabotageMode::Special::AircraftBrochures] = (nemesisHasRoutes ? 1 : 0);
        candidates[SabotageMode::Special::CutTelephones] = (nemesisManyOffices ? 10 : 0);
        candidates[SabotageMode::Special::FalsePressRelease] = (nemesisHasRoutes ? 10 : 0);
        candidates[SabotageMode::Special::BankHack] = (nemesisBroke ? 50 : 5);
        candidates[SabotageMode::Special::GroundAircraft] = 10;
        candidates[SabotageMode::Special::RouteTheft] = (routeTheftPossible ? 10 : 0);
    }

    /* get highest trust level needed */
    SLONG removedWeights = 0;
    for (auto &c : candidates) {
        /* remove candidates that cannot be used because ArabTrust is too low */
        if (c.first.getJobNumber() > qPlayer.ArabTrust) {
            removedWeights += c.second;
            c.second = 0; /* cannot take this candidate because we do not have enough trust */
        }
    }

    if (removedWeights > 0) {
        /* add jobs to build up trust. we pick the cheapest unless it would get us caught (plane crash) */
        switch (qPlayer.ArabTrust) {
        case 1:
            candidates[SabotageMode::Plane::SaltedFood] += removedWeights;
            break;
        case 2:
            candidates[SabotageMode::Plane::MovieTheatreBreak] += removedWeights / 2;
            candidates[SabotageMode::Personal::NotebookVirus] += (removedWeights + 1) / 2; /* more expensive, but no hints */
            break;
        case 3:
            candidates[SabotageMode::Plane::FlatTire] += removedWeights;
            break;
        case 4:
            candidates[SabotageMode::Plane::EngineBreakdown] += removedWeights;
            break;
        case 5:
            candidates[SabotageMode::Special::GroundAircraft] += removedWeights;
            break;
        default:
            AT_Error("Bot::determineSabotageMode(): Should not reach default case!");
            break;
        }
    }

    SLONG summedWeights = 0;
    for (auto &c : candidates) {
        summedWeights += c.second;
    }
    if (summedWeights == 0) {
        AT_Error("Bot::determineSabotageMode(): No sabotage jobs enabled!");
        return {};
    }

    /* select a candidate based on weights */
    TEAKRAND rnd{mSabotageSeed};
    SLONG idx = rnd.getRandInt(1, summedWeights);
    SabotageMode sabotageMode;
    SLONG weight = 0;
    for (const auto &c : candidates) {
        idx -= c.second;
        if (idx <= 0) {
            sabotageMode = c.first;
            weight = c.second;
            break;
        }
    }

    if (print) {
        AT_Log("Bot::determineSabotageMode(): Selected sabotage mode '%s' with weight %d (chance: %.2f%%, trust needed: %d/%d, job hints: %d, job cost: %lld)",
               sabotageMode.getName().c_str(), weight, 100.0 * weight / summedWeights, sabotageMode.getJobNumber(), qPlayer.ArabTrust,
               sabotageMode.getJobHints(), sabotageMode.getJobCost());
    }

    /* check preconditions */
    if (mArabHintsTracker + sabotageMode.getJobHints() > kMaxSabotageHints) {
        return {}; /* wait until we won't be caught */
    }
    if (sabotageMode.getJobCost() > moneyAvailable) {
        return {}; /* wait until we have enough money */
    }

    return sabotageMode;
}

SpecialSabotage Bot::determineSpecialSabotage() const {
    /* return "no" to not start chain again */
    if (qPlayer.HasItem(ITEM_ZANGE)) {
        return SpecialSabotage::No;
    }
    if (qPlayer.HasItem(ITEM_GLOVE) || qPlayer.HasItem(ITEM_REDBULL) || qPlayer.HasItem(ITEM_STINKBOMBE)) {
        return SpecialSabotage::No;
    }
    if (qPlayer.HasItem(ITEM_PAPERCLIP) || qPlayer.HasItem(ITEM_GLUE)) {
        return SpecialSabotage::No;
    }

    /* in missions where we use the security office: Prioritize getting wire cutters */
    if (qPlayer.RobotUse(ROBOT_USE_SECURTY_OFFICE) && !mPliersWereTaken) {
        return SpecialSabotage::CutWires;
    }

    /* other sabotage not used when not in nemesis mode */
    if (!qPlayer.RobotUse(ROBOT_USE_EXTRA_SABOTAGE)) {
        return SpecialSabotage::No;
    }

    /* cut phones, glue and stink bombing only make sense during regular play */
    if (Sim.CallItADay != 0) {
        return SpecialSabotage::No;
    }
    return SpecialSabotage::Any;
}

SLONG Bot::getNumRentedRoutes() const {
    SLONG numRented = 0;
    const auto &qRRouten = qPlayer.RentRouten.RentRouten;
    for (const auto &rentRoute : qRRouten) {
        if (rentRoute.Rang != 0) {
            numRented++;
        }
    }
    assert(numRented % 2 == 0);
    return (numRented / 2);
}

void Bot::checkRentedRoutes() {
    /* check for additional routes */
    const auto &qRRouten = qPlayer.RentRouten.RentRouten;
    for (SLONG routeId = 0; routeId < qRRouten.AnzEntries(); routeId++) {
        const auto &rentRoute = qRRouten[routeId];
        if (rentRoute.Rang == 0) {
            continue;
        }
        bool found = false;
        for (const auto &route : mRoutes) {
            if (route.routeId == routeId || route.routeReverseId == routeId) {
                found = true;
                break;
            }
        }
        if (!found) {
            AT_Log("Bot::checkRentedRoutes(): We found a new rented route: %s", Helper::getRouteName(Routen[routeId]).c_str());
            addNewRoute(routeId, -1);
            mRoutesToRemove = true;
        } else {
            AT_Log("Bot::checkRentedRoutes(): Route %s is still there.", Helper::getRouteName(Routen[routeId]).c_str());
        }
    }

    if (!mDoRoutes) {
        return; /* we do not care about routes, so we do not need to check whether some got lost */
    }

    auto numRented = getNumRentedRoutes();
    assert(numRented <= mRoutes.size());
    if (numRented >= mRoutes.size()) {
        return; /* alles ok */
    }

    AT_Error("We lost %d routes!", mRoutes.size() - numRented);

    std::vector<RouteInfo> routesNew;
    std::vector<SLONG> planesForRoutesNew;
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
                AT_Log("Bot::checkRentedRoutes(): Plane %s does not have a route anymore.", Helper::getPlaneName(qPlayer.Planes[planeId]).c_str());
            }
        }
    }
    std::swap(mRoutes, routesNew);
    std::swap(mPlanesForRoutes, planesForRoutesNew);
}

void Bot::updateRoutesSortedList() {
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
        AT_Log("Bot::updateRoutesSortedList(): Route %s has lowest image: %d", Helper::getRouteName(getRoute(mRoutes[lowImage])).c_str(),
               mRoutes[lowImage].image);
        AT_Log("Bot::updateRoutesSortedList(): Route %s has lowest utilization: %d/%d", Helper::getRouteName(getRoute(mRoutes[lowUtil])).c_str(),
               mRoutes[lowUtil].routeOwnUtilization, mRoutes[lowUtil].routeUtilization);
    }
}

void Bot::updateRouteInfoOffice(bool areWeInOffice) {
    /* copy most import information from routes
     * updates: image, routeOwnUtilization, planeUtilization(FC), canUpgrade, mPlanesForRoutesUnassigned
     * does not update: routeUtilization, mRouteToSteal */
    std::unordered_map<SLONG, std::vector<SLONG>> tmpList;
    for (auto &route : mRoutes) {
        route.image = std::min(getRentRoute(route).Image, getReverseRentRoute(route).Image);
        route.routeOwnUtilization = (getRentRoute(route).RoutenAuslastungBot + getReverseRentRoute(route).RoutenAuslastungBot) / 2;
        route.planeUtilization = getRentRoute(route).AuslastungBot;
        route.planeUtilizationFC = getRentRoute(route).AuslastungFirstClassBot;

        if (route.planeIds.empty()) {
            continue;
        }

        DOUBLE luxusSumme = 0;
        if (areWeInOffice) {
            route.canUpgrade = false;
            SLONG luxusTarget = 3 * (checkVeryLateGame() ? kPlaneLuxuryTargetLateGame : kPlaneLuxuryTarget) + kPlaneFoodTarget;
            for (auto i : route.planeIds) {
                const auto &qPlane = qPlayer.Planes[i];

                SLONG luxusForImage = qPlane.SitzeTarget + qPlane.EssenTarget + qPlane.TablettsTarget + qPlane.DecoTarget;
                SLONG luxusForFirstClass = qPlane.TriebwerkTarget + qPlane.ReifenTarget + qPlane.ElektronikTarget + qPlane.SicherheitTarget;
                luxusSumme += (luxusForImage + luxusForFirstClass);

                /* target: upgrade image-relevant */
                route.canUpgrade = route.canUpgrade || (qPlane.MaxPassagiereTargetFC > 0) || (luxusForImage < luxusTarget);
            }
            luxusSumme /= route.planeIds.size();
        }

        __int64 currentWeeklyRevenue = 0;
        for (auto i : route.planeIds) {
            const auto &qPlane = qPlayer.Planes[i];
            currentWeeklyRevenue += qPlane.GetSaldo();
        }

        AT_Log("Bot::updateRouteInfoOffice(): Route %s has image=%d and utilization=%d/%d (%d/%d planes with average utilization=%d/%d and luxus=%.2f)",
               Helper::getRouteName(getRoute(route)).c_str(), route.image, route.routeOwnUtilization, route.routeUtilization, route.planeIds.size(),
               route.numberOfPlanesTarget, route.planeUtilization, route.planeUtilizationFC, luxusSumme);

        if (route.planeTypeId != -1) {
            __int64 estimatedWeeklyRevenue = calcRouteScore(route.routeId, route.planeTypeId, tmpList).score;
            AT_Log("Bot::updateRouteInfoOffice(): Route %s has estimated weekly revenue=%s $ (current=%s $)", Helper::getRouteName(getRoute(route)).c_str(),
                   Insert1000erDots64(estimatedWeeklyRevenue).c_str(), Insert1000erDots64(currentWeeklyRevenue).c_str());
        }
    }

    updateRoutesSortedList();

    /* idle planes? */
    if (!mPlanesForRoutesUnassigned.empty()) {
        AT_Log("Bot::updateRouteInfoOffice(): There are %lu unassigned planes with no route ", mPlanesForRoutesUnassigned.size());
    }

    /* generate strategy for routes */
    mRoutesUpdated = true;
}

void Bot::updateRouteInfoBoard() {
    /* copy most import information from routes
     * updates: image, routeOwnUtilization, routeUtilization, mRouteToSteal
     * does not update: planeUtilization(FC), canUpgrade, mPlanesForRoutesUnassigned*/
    mRouteToSteal = -1;
    SLONG routeToStealUtil = 0;
    for (auto &route : mRoutes) {
        route.image = std::min(getRentRoute(route).Image, getReverseRentRoute(route).Image);
        route.routeOwnUtilization = (getRentRoute(route).RoutenAuslastungBot + getReverseRentRoute(route).RoutenAuslastungBot) / 2;
        route.routeUtilization = 0;
        for (SLONG i = 0; i < Sim.Players.Players.AnzEntries(); i++) {
            const auto &qqPlayer = Sim.Players.Players[i];
            if (qqPlayer.IsOut != 0) {
                continue;
            }
            if ((i != qPlayer.PlayerNum) && (qPlayer.HasBerater(BERATERTYP_INFO) == 0)) {
                continue; /* we do not know the route utilization by competitor */
            }

            const auto &qRentRoute = qqPlayer.RentRouten.RentRouten[route.routeId];
            const auto &qReverseRentRoute = qqPlayer.RentRouten.RentRouten[route.routeReverseId];
            route.routeUtilization += (qRentRoute.RoutenAuslastungBot + qReverseRentRoute.RoutenAuslastungBot) / 2;

            if (qRentRoute.RoutenAuslastungBot > 0 && i != qPlayer.PlayerNum && qRentRoute.Rang != 0) {
                AT_Log("Bot::updateRouteInfoBoard(): Route %s: We (%d utilization) are competing with %s (%d utilization)",
                       Helper::getRouteName(getRoute(route)).c_str(), route.routeOwnUtilization, qqPlayer.AirlineX.c_str(), qRentRoute.RoutenAuslastungBot);

                if ((mRouteToSteal == -1) || (qRentRoute.RoutenAuslastungBot > routeToStealUtil)) {
                    mRouteToSteal = route.routeId;
                    mRouteToStealFrom = i;
                    routeToStealUtil = qRentRoute.RoutenAuslastungBot;
                }
            }
        }
        AT_Log("Bot::updateRouteInfoBoard(): Route %s has image=%d and utilization=%d/%d (%d/%d planes with average utilization=%d/%d)",
               Helper::getRouteName(getRoute(route)).c_str(), route.image, route.routeOwnUtilization, route.routeUtilization, route.planeIds.size(),
               route.numberOfPlanesTarget, route.planeUtilization, route.planeUtilizationFC);
    }

    updateRoutesSortedList();

    /* find a route to steal even if we have none yet */
    if ((mRouteToSteal == -1) && (qPlayer.HasBerater(BERATERTYP_INFO) > 0)) {
        for (SLONG c = 0; c < Routen.AnzEntries(); c++) {
            for (SLONG i = 0; i < Sim.Players.Players.AnzEntries(); i++) {
                const auto &qqPlayer = Sim.Players.Players[i];
                if ((i == qPlayer.PlayerNum) || (qqPlayer.IsOut != 0)) {
                    continue;
                }
                const auto &qRentRoute = qqPlayer.RentRouten.RentRouten[c];
                if (qRentRoute.Rang == 0) {
                    continue; /* competitor does not rent this route, stealing it would only waste money and hints */
                }

                if (qRentRoute.RoutenAuslastungBot > routeToStealUtil) {
                    mRouteToSteal = c;
                    mRouteToStealFrom = i;
                    routeToStealUtil = qRentRoute.RoutenAuslastungBot;
                }
            }
        }
    }

    if (mRouteToSteal != -1) {
        AT_Log("Bot::updateRouteInfoBoard(): Best route to steal is %s from %s: %d max. utilization", Helper::getRouteName(Routen[mRouteToSteal]).c_str(),
               Sim.Players.Players[mRouteToStealFrom].AirlineX.c_str(), routeToStealUtil);
    }

    /* generate strategy for routes */
    mRoutesUtilizationUpdated = true;
}

SLONG Bot::calcRequiredImageForAirline() {
    bool targetRunStarted = (mRunToFinalObjective > FinalPhase::No);
    SLONG targetImage = kMinimumImage;
    if (qPlayer.RobotUse(ROBOT_USE_MUCHWERBUNG) && targetRunStarted) { /* mission where we need to buy ads */
        if (mRunToFinalObjective == FinalPhase::TargetRun) {
            targetImage = 1000;
        }
    } else if (!targetRunStarted && haveDiscount() && mDoRoutes && !mRoutes.empty()) {
        if (kImagePaybackDays > 0) {
            targetImage = calcAirlineImageTarget();
        } else {
            SLONG lowestRouteImage = 100;
            for (const auto &qRoute : mRoutes) {
                lowestRouteImage = std::min(lowestRouteImage, qRoute.image);
            }
            targetImage = Helper::getRequiredImageBasedOnLowestRoute(lowestRouteImage);
        }
    }
    return targetImage;
}

SLONG Bot::calcAirlineImageTarget() const {
    SLONG lowestRouteImage = 100;
    for (const auto &qRoute : mRoutes) {
        lowestRouteImage = std::min(lowestRouteImage, qRoute.image);
    }
    /* image beyond this does not add passengers: ImageTotal is capped at 1000 */
    __int64 saturation = Helper::getRequiredImageBasedOnLowestRoute(lowestRouteImage);

    /* One airline image point costs ~50,000 and lifts every route passenger by 1 / (400 + ImageTotal),
     * so it is worth yesterday's tickets / (400 + ImageTotal) a day. Buy only as far as the last point
     * pays back within kImagePaybackDays. */
    __int64 baseTotal = 400 + 4 * lowestRouteImage + 200;
    __int64 worthwhile = mTicketsYesterday * kImagePaybackDays / 50000 - baseTotal;
    saturation = std::max(0LL, std::min(saturation, worthwhile));
    if (saturation <= 0LL) {
        return kMinimumImage;
    }

    /* the agency is closed on Saturday and Sunday: cover the erosion until it opens again */
    SLONG target = static_cast<SLONG>(saturation);
    SLONG daysToCover = 1;
    while (daysToCover < 7 && ((Sim.Weekday + daysToCover) % 7 == 5 || (Sim.Weekday + daysToCover) % 7 == 6)) {
        daysToCover++;
    }
    return std::min(1000, target + mImageDecayPerDay * daysToCover);
}

void Bot::routesRecalcNextStep() {
    mRoutesNextStep = RoutesNextStep::None;
    if (!mRoutesUpdated || !mRoutesUtilizationUpdated) {
        AT_Log("Bot::routesRecalcNextStep(): Route information not updated, cannot generate route strategy yet");
        return;
    }

    std::tie(mRoutesNextStep, mImproveRouteId) = routesFindNextStep();

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
        AT_Log("Bot::routesRecalcNextStep(): Need to buy another %s for route %s", getPlaneType(mBuyPlaneForRouteId).Name.c_str(), routeName.c_str());
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

    /* find route with not enough planes */
    SLONG routeToBuyPlanes = -1;
    for (auto i : mRoutesSortedByOwnUtilization) {
        if (mRoutes[i].planeIds.size() < mRoutes[i].numberOfPlanesTarget) {
            if (mRoutes[i].routeUtilization < 90 && mRoutes[i].routeOwnUtilization < mOptions.kMaximumRouteUtilization) {
                routeToBuyPlanes = i;
                break;
            }
        }
    }

    /* find route with low image */
    SLONG routeWithLowImage = -1;
    SLONG lowestImage = 9999;
    for (auto i : mRoutesSortedByOwnUtilization) {
        if (mRoutes[i].image < lowestImage) {
            routeWithLowImage = i;
            lowestImage = mRoutes[i].image;
        }
    }

    /* find route with pending plane upgrades */
    SLONG routeWithPendingPlaneUpgrades = -1;
    for (auto i : mRoutesSortedByOwnUtilization) {
        if (mRoutes[i].canUpgrade) {
            routeWithPendingPlaneUpgrades = i;
            break;
        }
    }

    bool canBuyAdsToday = qPlayer.RobotUse(ROBOT_USE_WERBUNG) && (Sim.Weekday != 5 && Sim.Weekday != 6);

    /* Step 1: Is the default, at the bottom */

    /* Step 2: Buy additional plane when we have the money */
    if (routeToBuyPlanes != -1) {
        __int64 moneyAvailable = getMoneyAvailable();
        const auto &qRoute = mRoutes[routeToBuyPlanes];
        const auto &qPlaneType = getPlaneType(qRoute.planeTypeId);
        bool haveMoney = (moneyAvailable >= qPlaneType.Preis);
        bool haveCrew = (mExtraPilots >= qPlaneType.AnzPiloten) && (mExtraBegleiter >= qPlaneType.AnzBegleiter);
        if (haveMoney && haveCrew) {
            return {RoutesNextStep::BuyMorePlanes, routeToBuyPlanes};
        }
    }

    /* Step 3: Buy first plane for underutilized route */
    if (routeToBuyPlanes != -1) {
        const auto &qRoute = mRoutes[routeToBuyPlanes];
        if (qRoute.planeIds.empty()) {
            return {RoutesNextStep::BuyMorePlanes, routeToBuyPlanes};
        }
    }

    /* Step 4: Increase route image if planes underutilized */
    if (canBuyAdsToday && (routeWithLowImage != -1) && (mRoutes[routeWithLowImage].image < kRouteMaxImage)) {
        return {RoutesNextStep::BuyAdsForRoute, routeWithLowImage};
    }

    /* Step 5: Now we can upgrade the plane for first class passengers */
    if (routeWithPendingPlaneUpgrades != -1) {
        const auto &qRoute = mRoutes[routeWithPendingPlaneUpgrades];
        (void)qRoute;
        assert(qRoute.canUpgrade);
        return {RoutesNextStep::UpgradePlanes, routeWithPendingPlaneUpgrades};
    }

    /* Step 6: Planes are all upgraded, buy next one */
    if (routeToBuyPlanes != -1) {
        return {RoutesNextStep::BuyMorePlanes, routeToBuyPlanes};
    }

    /* We do not return RoutesNextStep::ImproveAirlineImage anymore, this is handled in parallel */

    /* Step 1: No routes underutilized, rent new route */
    return {RoutesNextStep::RentNewRoute, -1};
}

void Bot::requestPlanRoutes(bool areWeInOffice) {
    auto res = howToPlanFlightsLaptopFix();
    if (res == HowToPlan::Laptop) {
        AT_Log("Bot::requestPlanRoutes(): Planning using laptop");
        planRoutes();
        mNeedToPlanRoutes = false;
    } else if (res == HowToPlan::Office && areWeInOffice) {
        AT_Log("Bot::requestPlanRoutes(): Already in office, planning right now");
        planRoutes();
        mNeedToPlanRoutes = false;
    } else {
        AT_Log("Bot::requestPlanRoutes(): No laptop, need to go to office");
        mNeedToPlanRoutes = true;
        forceReplanning();
    }
}

Bot::RouteScore Bot::calcRouteScore(SLONG routeId, SLONG planeTypeId, std::unordered_map<SLONG, std::vector<SLONG>> &existingPlaneIds) {
    const auto &qRoute = Routen[routeId];
    const auto &qPlaneType = getPlaneType(planeTypeId);

    int cost = 0;
    int duration = 0;
    int dist = 0;
    Helper::calcCostAndDuration(Cities.find(qRoute.VonCity), Cities.find(qRoute.NachCity), qPlaneType, false, cost, duration, dist);
    duration += kDurationExtra;

    /* check if plane type is suitable for route */
    SLONG distance = Cities.CalcDistance(qRoute.VonCity, qRoute.NachCity);
    if (distance > qPlaneType.Reichweite * 1000 || duration >= 24) {
        return {};
    }
    if (planeTypeId == kDesignerPlaneTypeId && !designerRoutePays(qRoute)) {
        return {};
    }

    if (qPlayer.RobotUse(ROBOT_USE_SHORTFLIGHTS)) {
        const auto target = BTARGET_PASSAVG * 6 / 5; /* need to transport X passengers each day (plus margin) */
        if ((24 / duration) * qPlaneType.Passagiere < target) {
            return {};
        }
    }

    /* estimate our target share, considering current utilization of route */
    SLONG routeUtilization = 0;
    SLONG targetSharePercent = mOptions.kMaximumRouteUtilization;
    if (qPlayer.HasBerater(BERATERTYP_INFO) > 0) {
        for (SLONG i = 0; i < Sim.Players.Players.AnzEntries(); i++) {
            const auto &qqPlayer = Sim.Players.Players[i];
            if (i == qPlayer.PlayerNum || qqPlayer.IsOut != 0) {
                continue;
            }
            routeUtilization += qqPlayer.RentRouten.RentRouten[routeId].RoutenAuslastungBot;
        }
    } else {
        /* no spy: one estimate for all competitors together (50 each rejected every route) */
        routeUtilization = kUnknownCompetitorUtilization;
    }
    if (routeUtilization > 0) {
        routeUtilization = std::min(100, routeUtilization + 20); /* offset, we can expect the enemy to increase their share */
        targetSharePercent = std::max(0, targetSharePercent - routeUtilization);
    }
    if (targetSharePercent <= 10) {
        return {}; /* route is already fully utilized */
    }

    /* calculate how many planes would be need to get desired route utilization */
    SLONG numPlanesMin = Helper::getNumberOfPlanesNeededForRoute(qRoute, qPlaneType, 10);
    SLONG numPlanesTarget = Helper::getNumberOfPlanesNeededForRoute(qRoute, qPlaneType, targetSharePercent);
    numPlanesTarget *= 2; /* for each route leg */
    /* numPlanesMin stays since each flight is booked for both directions for required minimum utilization */

    /* estimate revenue */
    __int64 baseCost = getRouteBaseCost(qRoute);
    __int64 revenue = qPlaneType.Passagiere * baseCost * 3.0 * mOptions.kMaxTicketPriceFactor.target;
    SLONG numTripsPerWeek = 24 * 7 / duration;
    __int64 profitPerWeek = (revenue - cost) * numTripsPerWeek * numPlanesTarget - (qRoute.Miete / 30 * 2 * 7);

    /* account for the fact that we already have suitable planes */
    SLONG planesToBuy = std::max(0, numPlanesMin - static_cast<SLONG>(existingPlaneIds[planeTypeId].size()));

    /* is this route important for our mission */
    if (qPlayer.RobotUse(ROBOT_USE_ROUTEMISSION)) {
        auto homeAirport = static_cast<ULONG>(Sim.HomeAirportId);
        for (SLONG d = 0; d < 6; d++) {
            auto missionCity = static_cast<ULONG>(Sim.MissionCities[d]);
            if ((qRoute.VonCity == homeAirport && qRoute.NachCity == missionCity) || (qRoute.NachCity == homeAirport && qRoute.VonCity == missionCity)) {

                AT_Log("Bot::calcRouteScore(): Route %s is important for mission, increasing score.", Helper::getRouteName(qRoute).c_str());
                profitPerWeek *= 10;
            }
        }
    }
    return {profitPerWeek, routeId, planeTypeId, existingPlaneIds[planeTypeId], planesToBuy};
}

const CPlaneType &Bot::getPlaneType(SLONG planeTypeId) const {
    if (planeTypeId == kDesignerPlaneTypeId) {
        assert(mDesignerPlaneType.Passagiere > 0);
        return mDesignerPlaneType;
    }
    return PlaneTypes[planeTypeId];
}

void Bot::updateDesignerPlaneType() {
    mDesignerPlaneType = {};
    if (mDesignerPlane.Name.empty() || !mDesignerPlane.IsBuildable()) {
        return;
    }
    /* same values as PLAYER::BuyPlane() gives the plane */
    mDesignerPlaneType.Name = mDesignerPlane.Name;
    mDesignerPlaneType.Passagiere = mDesignerPlane.CalcPassagiere();
    mDesignerPlaneType.Reichweite = mDesignerPlane.CalcReichweite();
    mDesignerPlaneType.Geschwindigkeit = mDesignerPlane.CalcSpeed();
    mDesignerPlaneType.AnzPiloten = mDesignerPlane.CalcPiloten();
    mDesignerPlaneType.AnzBegleiter = mDesignerPlane.CalcBegleiter();
    mDesignerPlaneType.Tankgroesse = mDesignerPlane.CalcTank();
    mDesignerPlaneType.Verbrauch = mDesignerPlane.CalcVerbrauch();
    mDesignerPlaneType.Preis = mDesignerPlane.CalcCost();

    mDesignerRoutesPay = false;
    for (SLONG c = 0; c < Routen.AnzEntries() && !mDesignerRoutesPay; c++) {
        if ((Routen.IsInAlbum(c) != 0) && Routen[c].VonCity < Routen[c].NachCity) {
            mDesignerRoutesPay = designerRoutePays(Routen[c]);
        }
    }
    AT_Log("Bot::updateDesignerPlaneType(): %s: %d seats, %d km, %d km/h, %d l/h, %s $, pays on routes: %s", mDesignerPlaneType.Name.c_str(),
           mDesignerPlaneType.Passagiere, mDesignerPlaneType.Reichweite, mDesignerPlaneType.Geschwindigkeit, mDesignerPlaneType.Verbrauch,
           Insert1000erDots64(mDesignerPlaneType.Preis).c_str(), mDesignerRoutesPay ? "yes" : "no");
}

bool Bot::designerRoutePays(const CRoute &qRoute) const {
    const auto &qPlaneType = mDesignerPlaneType;
    if (qPlaneType.Passagiere <= 0 || qPlaneType.Preis <= 0) {
        return false;
    }
    int cost = 0;
    int duration = 0;
    int dist = 0;
    Helper::calcCostAndDuration(Cities.find(qRoute.VonCity), Cities.find(qRoute.NachCity), qPlaneType, false, cost, duration, dist);
    duration += kDurationExtra;
    if (dist > qPlaneType.Reichweite * 1000 || duration >= 24) {
        return false;
    }
    DOUBLE ticketPrice = getRouteBaseCost(qRoute) * 3.0 * mOptions.kMaxTicketPriceFactor.target;
    DOUBLE profitPerFlight = qPlaneType.Passagiere * kDesignerRouteExpectedLoad * ticketPrice - cost;
    DOUBLE profitPerWeek = profitPerFlight * (24 * 7 / duration);
    return profitPerWeek >= kDesignerRouteMinWeeklyReturn * qPlaneType.Preis;
}

void Bot::findBestRoute() {
    mWantToRentRouteId = -1;
    mPlaneTypeForNewRoute = -1;
    mPlanesForNewRoute.clear();

    /* check existing planes */
    std::unordered_map<SLONG, std::vector<SLONG>> existingPlaneIds;
    if (mRoutes.empty()) {
        for (const auto id : mPlanesForRoutesUnassigned) {
            auto &qPlane = qPlayer.Planes[id];
            existingPlaneIds[getRoutePlaneTypeId(qPlane)].emplace_back(id);
        }
    }

    /* the designer plane is the only plane we buy in designer missions, so plan routes for it */
    std::vector<SLONG> planeTypeIds = mKnownPlaneTypes;
    if (qPlayer.RobotUse(ROBOT_USE_DESIGNER_BUY) && mDesignerPlaneType.Passagiere > 0) {
        planeTypeIds = {kDesignerPlaneTypeId};
    }

    std::vector<RouteScore> bestRoutes;
    auto isBuyable = GameMechanic::getBuyableRoutes(qPlayer);
    for (SLONG c = 0; c < Routen.AnzEntries(); c++) {
        if (isBuyable[c] == 0) {
            continue;
        }
        if (Routen[c].VonCity > Routen[c].NachCity) {
            continue; /* we only need to check one of each pair */
        }
        for (const auto &planeTypeId : planeTypeIds) {
            if (planeTypeId != kDesignerPlaneTypeId && !PlaneTypes.IsInAlbum(planeTypeId)) {
                continue;
            }

            RouteScore score = calcRouteScore(c, planeTypeId, existingPlaneIds);
            if (score.score > 0) {
                bestRoutes.emplace_back(std::move(score));
            }
        }
    }

    /* sort routes by score, limit to 5 best */
    std::sort(bestRoutes.begin(), bestRoutes.end());
    bestRoutes.resize(std::min(bestRoutes.size(), static_cast<size_t>(5)));

    for (const auto &candidate : bestRoutes) {
        if (!candidate.planeId.empty()) {
            AT_Log("Bot::findBestRoute(): Estimated weekly revenue of route %s (using %d existing planes, need %d) is: %s $",
                   Helper::getRouteName(Routen[candidate.routeId]).c_str(), candidate.planeId.size(), candidate.numPlanesToBuy,
                   Insert1000erDots64(candidate.score).c_str());
        } else {
            AT_Log("Bot::findBestRoute(): Estimated weekly revenue of route %s (using plane type %s, need %d) is: %s $",
                   Helper::getRouteName(Routen[candidate.routeId]).c_str(), getPlaneType(candidate.planeTypeId).Name.c_str(), candidate.numPlanesToBuy,
                   Insert1000erDots64(candidate.score).c_str());
        }
    }

    /* pick best route we can afford */
    __int64 moneyAvailable = getMoneyAvailable();
    for (const auto &candidate : bestRoutes) {
        __int64 planeCost = getPlaneType(candidate.planeTypeId).Preis;
        if (candidate.numPlanesToBuy * planeCost > moneyAvailable) {
            AT_Log("Bot::findBestRoute(): We cannot afford route %s (plane costs %lld, need %d), our available money is %lld",
                   Helper::getRouteName(Routen[candidate.routeId]).c_str(), planeCost, candidate.numPlanesToBuy, moneyAvailable);
            continue;
        }
        AT_Log("Bot::findBestRoute(): Best route (using plane type %s) is: ", getPlaneType(candidate.planeTypeId).Name.c_str());
        Helper::printRoute(Routen[candidate.routeId]);

        mWantToRentRouteId = candidate.routeId;
        mPlaneTypeForNewRoute = candidate.planeTypeId; /* buy new plane */
        mPlanesForNewRoute = candidate.planeId;        /* use existing planes */
        return;
    }

    AT_Log("Bot::findBestRoute(): No routes match criteria.");
}

bool Bot::addNewRoute(SLONG routeA, SLONG planeTypeForNewRoute) {
    /* find route in reverse direction */
    SLONG routeB = -1;
    for (SLONG c = 0; c < Routen.AnzEntries(); c++) {
        if ((Routen.IsInAlbum(c) != 0) && Routen[c].VonCity == Routen[routeA].NachCity && Routen[c].NachCity == Routen[routeA].VonCity) {
            routeB = c;
            break;
        }
    }
    if (-1 == routeB) {
        AT_Error("Bot::addNewRoute(): Unable to find route in reverse direction.");
        return false;
    }

    SLONG numberOfPlanesTarget = 0;
    if (planeTypeForNewRoute != -1) {
        numberOfPlanesTarget = Helper::getNumberOfPlanesNeededForRoute(Routen[routeA], getPlaneType(planeTypeForNewRoute), mOptions.kMaximumRouteUtilization);
        numberOfPlanesTarget *= 2; /* for each route leg */
    }
    mRoutes.emplace_back(routeA, routeB, planeTypeForNewRoute, numberOfPlanesTarget);
    if (planeTypeForNewRoute != -1) {
        AT_Log("Bot::addNewRoute(): Renting route %s (using plane type %s): ", Helper::getRouteName(getRoute(mRoutes.back())).c_str(),
               getPlaneType(planeTypeForNewRoute).Name.c_str());
    }

    /* update sorted list */
    assert(mRoutes.size() > 0);
    mRoutesSortedByOwnUtilization.resize(mRoutes.size());
    for (SLONG i = mRoutesSortedByOwnUtilization.size() - 1; i >= 1; i--) {
        mRoutesSortedByOwnUtilization[i] = mRoutesSortedByOwnUtilization[i - 1];
    }
    mRoutesSortedByOwnUtilization[0] = mRoutes.size() - 1;

    return true;
}

std::vector<Bot::RouteInfo>::iterator Bot::removeRoute(std::vector<RouteInfo>::iterator it) {
    SLONG routeIdx = std::distance(mRoutes.begin(), it);
    if (routeIdx < 0 || routeIdx >= mRoutes.size()) {
        AT_Error("Bot::removeRoute(): Invalid route index %d", routeIdx);
        return it;
    }

    for (auto planeId : it->planeIds) {
        auto planeIt = std::find(mPlanesForRoutes.begin(), mPlanesForRoutes.end(), planeId);
        if (planeIt != mPlanesForRoutes.end()) {
            mPlanesForRoutes.erase(planeIt);
        }
        mPlanesForRoutesUnassigned.push_back(planeId);
        AT_Log("Bot::removeRoute(): Plane %s does not have a route anymore.", Helper::getPlaneName(qPlayer.Planes[planeId]).c_str());
    }

    it = mRoutes.erase(it);

    /* update sorted list */
    mRoutesSortedByOwnUtilization.erase(std::remove(mRoutesSortedByOwnUtilization.begin(), mRoutesSortedByOwnUtilization.end(), routeIdx),
                                        mRoutesSortedByOwnUtilization.end());
    for (auto &idx : mRoutesSortedByOwnUtilization) {
        if (idx > routeIdx) {
            idx--;
        }
    }

    return it;
}

void Bot::planRoutes() {
    mNeedToPlanRoutes = false;

    for (auto planeId : mPlanesForRoutesUnassigned) {
        GameMechanic::clearFlightPlan(qPlayer, planeId);
    }

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

            /* plane is not on the route yet: the first leg departs from fromCity (route A) or toCity (route B) */
            SLONG departCity = useRouteA ? fromCity : toCity;
            if (numCorrectlyScheduled == 0 && availCity != departCity) {
                SLONG otherCity = useRouteA ? toCity : fromCity;
                if (availCity == otherCity) {
                    /* plane already stands at the other end: the next time slot departs from there */
                    startTime += duration;
                    useRouteA = !useRouteA;
                    AT_Log("Bot::planRoutes(): Plane %s: Starting one slot later, in %s", Helper::getPlaneName(qPlane).c_str(),
                           Cities[availCity].Kuerzel.c_str());
                } else {
                    /* leave room for auto flight */
                    SLONG autoFlightDuration = kDurationExtra + Cities.CalcFlugdauer(availCity, departCity, qPlane.ptGeschwindigkeit);
                    availTime += autoFlightDuration;
                    while (startTime < availTime) {
                        startTime += 2 * duration;
                    }
                    AT_Log("Bot::planRoutes(): Plane %s: Adding buffer of %d hours for auto flight from %s to %s", Helper::getPlaneName(qPlane).c_str(),
                           autoFlightDuration, Cities[availCity].Kuerzel.c_str(), Cities[departCity].Kuerzel.c_str());
                }
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

    if (mImageDecayPerDay >= 500 || getImage() < -500) {
        /* preserve image for one week; if that week would end on a weekend, extend it to Monday when the ad agency is open again */
        mImagePreservationMode = Sim.Date + 7 + (Sim.Weekday == 5 ? 2 : 0) + (Sim.Weekday == 6 ? 1 : 0);
        AT_Log("Bot::planRoutes(): Activate image preserving mode until day %d (current image: %d, daily decay: %d)", mImagePreservationMode, getImage(),
               mImageDecayPerDay);
    } else if (checkVeryLateGame()) {
        AT_Log("Bot::planRoutes(): No image preserving mode necessary (current image: %d, daily decay: %d)", getImage(), mImageDecayPerDay);
    }

    /* adjust ticket prices */
    for (auto &qRoute : mRoutes) {
        if (qRoute.planeIds.empty()) {
            continue;
        }

        SLONG priceOld = getRentRoute(qRoute).Ticketpreis;
        SLONG cost = getRouteBaseCost(getRoute(qRoute));
        SLONG highCost = 3 * cost;

        const RoutePriceLevels &factors = (Sim.Date >= mImagePreservationMode) ? mOptions.kMaxTicketPriceFactor : mOptions.kMaxTicketPriceFactorLowImage;
        SLONG priceNew = static_cast<SLONG>(std::round(factors.target * highCost)) / 10 * 10;
        SLONG priceNewFC = static_cast<SLONG>(std::round(mOptions.kFirstClassTicketSurcharge * factors.target * highCost)) / 10 * 10;

        /* only touch the price when the old one actually costs us revenue or image */
        if ((priceOld >= factors.lowerLimit * highCost) && (priceOld <= factors.upperLimit * highCost)) {
            continue;
        }

        AT_Log("Bot::planRoutes(): Changing ticket price for route %s: %d (%.2f %%) => %d (%.2f %%), first class: %d => %d",
               Helper::getRouteName(getRoute(qRoute)).c_str(), priceOld, 100.0f * priceOld / highCost, priceNew, 100.0f * priceNew / highCost,
               getRentRoute(qRoute).TicketpreisFC, priceNewFC);
        GameMechanic::setRouteTicketPriceBoth(qPlayer, qRoute.routeId, priceNew, priceNewFC);
    }
}

void Bot::assignPlanesToRoutes(bool areWeInOffice) {
    if (mRoutes.empty()) {
        return;
    }

    /* assign planes to routes */
    SLONG numUnassigned = mPlanesForRoutesUnassigned.size();
    for (SLONG i = 0; i < numUnassigned; i++) {
        SLONG planeId = mPlanesForRoutesUnassigned.front();
        const auto &qPlane = qPlayer.Planes[planeId];
        mPlanesForRoutesUnassigned.pop_front();

        if (!checkPlaneAvailable(planeId, true, areWeInOffice) || stillNeedsRepairs(qPlane)) {
            mPlanesForRoutesUnassigned.push_back(planeId);
            continue;
        }

        SLONG targetRouteIdx = -1;
        for (SLONG routeIdx : mRoutesSortedByOwnUtilization) {
            auto &qRoute = mRoutes[routeIdx];
            if (qRoute.routeUtilization >= mOptions.kMaximumRouteUtilization) {
                continue;
            }
            if (qRoute.planeTypeId == -1) {
                continue; /* route will be removed */
            }
            if (qRoute.planeTypeId != getRoutePlaneTypeId(qPlane)) {
                continue; /* a designer plane only flies routes planned for it */
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
