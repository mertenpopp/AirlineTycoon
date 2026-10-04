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
#include <map>
#include <utility>
#include <vector>

template <class... Types> void AT_Error(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_ERROR, "Bot", args...); }
template <class... Types> void AT_Warn(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_WARN, "Bot", args...); }
template <class... Types> void AT_Info(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_INFO, "Bot", args...); }
template <class... Types> void AT_Log(Types... args) { AT_Log_I("Bot", args...); }

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

/* Room: any */
void Bot::grabNewFlights() {
    /* this will cause the planning algo to assume that we have not checked these today */
    mLastTimeInRoom.erase(ACTION_CALL_INTERNATIONAL);
    mLastTimeInRoom.erase(ACTION_CALL_INTER_HANDY);
    mLastTimeInRoom.erase(ACTION_CHECKAGENT1);
    mLastTimeInRoom.erase(ACTION_CHECKAGENT2);
    mLastTimeInRoom.erase(ACTION_CHECKAGENT3);
}

/* Room: office or laptop, financial advisor >= 0 for BilanzWoche, spy >= 50 for any competitor */
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

/* Room: office or laptop, has advisor check */
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

/* Room: any; office only if areWeInOffice == true (reads plane upgrade levels) */
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
                    AtDebugBreak();
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
        mOptions.kSchedulingMinScoreRatio = mOptions.kSchedulingMinScoreRatioLastMinute; /* planes have to fly for upgrades to apply */
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
        mOptions.kSchedulingMinScoreRatio = mOptions.kSchedulingMinScoreRatioLastMinute; /* planes have to fly for upgrades to apply */
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

/* Room: plane broker, because of getAvailablePlaneTypes() */
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

/* Room: room of the planer's job source (ACTION_CHECKAGENT1/2/3, office or ACTION_CALL_INTER_HANDY); areWeInOffice must be truthful */
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

/* Room: any (plans right away in office or with laptop, otherwise defers to the office) */
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

/* Room: office or laptop */
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

/* Room: office or laptop */
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

/* Room: Arab (ACTION_BUY_KEROSIN) or office (kerosine price, TankInhalt) */
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

/* Room: any (competitor data gated by spy checks); used at the saboteur */
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

/* Room: any */
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

/* Room: any (cached data only) */
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

/* Room: any (cached data only) */
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

/* Room: any */
const CPlaneType &Bot::getPlaneType(SLONG planeTypeId) const {
    if (planeTypeId == kDesignerPlaneTypeId) {
        assert(mDesignerPlaneType.Passagiere > 0);
        return mDesignerPlaneType;
    }
    return PlaneTypes[planeTypeId];
}

/* Room: any */
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
