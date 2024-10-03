#include "Bot.h"

#include "BotHelper.h"
#include "GameMechanic.h"
#include "Proto.h"
#include "TeakLibW.h"
#include "class.h"
#include "defines.h"
#include "global.h"

#include <SDL_log.h>

#include <algorithm>
#include <array>
#include <deque>
#include <unordered_map>
#include <utility>
#include <vector>

template <class... Types> void AT_Error(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_ERROR, "Bot", args...); }
template <class... Types> void AT_Warn(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_WARN, "Bot", args...); }
template <class... Types> void AT_Info(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_INFO, "Bot", args...); }
template <class... Types> void AT_Log(Types... args) { AT_Log_I("Bot", args...); }

void Bot::printRobotFlags() const {
    const std::array<std::pair<SLONG, bool>, 28> list = {
        {{ROBOT_USE_FRACHT, true},         {ROBOT_USE_WERBUNG, true},           {ROBOT_USE_NASA, false},
         {ROBOT_USE_ROUTES, true},         {ROBOT_USE_FORCEROUTES, false},      {ROBOT_USE_ROUTEMISSION, false},
         {ROBOT_USE_MUCHWERBUNG, false},   {ROBOT_USE_ROUTEBOX, true},          {ROBOT_USE_ABROAD, true},
         {ROBOT_USE_MUCH_FRACHT, false},   {ROBOT_USE_FREE_FRACHT, false},      {ROBOT_USE_LUXERY, false},
         {ROBOT_USE_HIGHSHAREPRICE, true}, {ROBOT_USE_WORKQUICK, false},        {ROBOT_USE_GROSSESKONTO, false},
         {ROBOT_USE_WORKVERYQUICK, false}, {ROBOT_USE_DONTBUYANYSHARES, false}, {ROBOT_USE_NOCHITCHAT, false},
         {ROBOT_USE_SHORTFLIGHTS, false},  {ROBOT_USE_EXTREME_SABOTAGE, false}, {ROBOT_USE_SECURTY_OFFICE, false},
         {ROBOT_USE_MAKLER, true},         {ROBOT_USE_PETROLAIR, true},         {ROBOT_USE_MAX20PERCENT, false},
         {ROBOT_USE_TANKS, true},          {ROBOT_USE_DESIGNER, true},          {ROBOT_USE_DESIGNER_BUY, false},
         {ROBOT_USE_WORKQUICK_2, true}}};

    for (const auto &i : list) {
        bool robotUses = qPlayer.RobotUse(i.first);
        if (robotUses == i.second) {
            AT_Info("Bot::printRobotFlags(): %s is %s (default)", Translate_ROBOT_USE(i.first), (robotUses ? "SET" : "UNSET"));
        } else {
            AT_Warn("Bot::printRobotFlags(): %s is %s (mission specialization)", Translate_ROBOT_USE(i.first), (robotUses ? "SET" : "UNSET"));
        }
    }
}

SLONG Bot::numPlanes() const { return mPlanesForJobs.size() + mPlanesForJobsUnassigned.size() + mPlanesForRoutes.size() + mPlanesForRoutesUnassigned.size(); }

std::vector<SLONG> Bot::getAllPlanes() const {
    std::vector<SLONG> planes = mPlanesForRoutes;
    for (const auto &i : mPlanesForRoutesUnassigned) {
        planes.push_back(i);
    }
    for (const auto &i : mPlanesForJobs) {
        planes.push_back(i);
    }
    for (const auto &i : mPlanesForJobsUnassigned) {
        planes.push_back(i);
    }
    return planes;
}

bool Bot::isOfficeUsable() const { return (qPlayer.OfficeState != 2); }

bool Bot::hoursPassed(SLONG room, SLONG hours) const {
    const auto it = mLastTimeInRoom.find(room);
    if (it == mLastTimeInRoom.end()) {
        return true;
    }
    return (Sim.Time - it->second > hours * 60000);
}

bool Bot::haveDiscount() const {
    /* wait until we have some discount */
    return (qPlayer.HasBerater(BERATERTYP_SICHERHEIT) >= 50) || (Sim.Date > 7);
}

bool Bot::checkLaptop() {
    if (qPlayer.HasItem(ITEM_LAPTOP)) {
        if ((qPlayer.LaptopVirus == 1) && (qPlayer.HasItem(ITEM_DISKETTE) == 1)) {
            GameMechanic::useItem(qPlayer, ITEM_DISKETTE);
        }
        if (qPlayer.LaptopVirus == 0) {
            return true;
        }
    }
    return false;
}

Bot::HowToPlan Bot::howToPlanFlights() {
    if (!mDayStarted) {
        return HowToPlan::None;
    }
    if (checkLaptop()) {
        return HowToPlan::Laptop;
    }
    if (isOfficeUsable()) {
        return HowToPlan::Office;
    }
    return HowToPlan::None;
}

__int64 Bot::getMoneyAvailable() const {
    __int64 m = qPlayer.Money;
    m -= mMoneyReservedForRepairs;
    m -= mMoneyReservedForUpgrades;
    m -= mMoneyReservedForAuctions;
    m -= mMoneyReservedForFines;
    m -= kMoneyEmergencyFund;
    return m;
}

Bot::AreWeBroke Bot::areWeBroke() const {
    if (mRunToFinalObjective == FinalPhase::TargetRun) {
        return AreWeBroke::Desperate;
    }

    auto moneyAvailable = getMoneyAvailable();
    if (moneyAvailable < DEBT_WARNLIMIT3) {
        return AreWeBroke::Desperate;
    }
    if (moneyAvailable < 0) {
        return AreWeBroke::Yes;
    }

    /* no reason to get as much money as possible right now */
    if (moneyAvailable < qPlayer.Credit) {
        return AreWeBroke::Somewhat;
    }
    return AreWeBroke::No;
}

Bot::HowToGetMoney Bot::howToGetMoney() {
    auto broke = areWeBroke();
    if (broke == AreWeBroke::No) {
        return HowToGetMoney::None;
    }

    SLONG numShares = 0;
    SLONG numOwnShares = 0;
    for (SLONG c = 0; c < Sim.Players.AnzPlayers; c++) {
        if (c == qPlayer.PlayerNum) {
            numOwnShares += qPlayer.OwnsAktien[c];
        } else {
            numShares += qPlayer.OwnsAktien[c];
        }
    }

    if (broke < AreWeBroke::Desperate) {
        /* do not sell all shares to prevent hostile takeover */
        numOwnShares = std::max(0, numOwnShares - qPlayer.AnzAktien / 2 - 1);
    }

    /* Step 1: Lower repair targets */
    if (mMoneyReservedForRepairs > 0) {
        return HowToGetMoney::LowerRepairTargets;
    }

    /* Step 2: Cancel plane upgrades */
    if (mMoneyReservedForUpgrades > 0) {
        return HowToGetMoney::CancelPlaneUpgrades;
    }

    /* Step 3: Emit shares */
    if (GameMechanic::canEmitStock(qPlayer) == GameMechanic::EmitStockResult::Ok) {
        return HowToGetMoney::EmitShares;
    }

    /* Step 4: Sell shares */
    if (numShares > 0) {
        return HowToGetMoney::SellShares;
    }
    if (broke == AreWeBroke::Somewhat) {
        return HowToGetMoney::None;
    }
    if (numOwnShares > 0) {
        return (broke == AreWeBroke::Desperate) ? HowToGetMoney::SellAllOwnShares : HowToGetMoney::SellOwnShares;
    }

    /* Step 5: Take out loan */
    if (qPlayer.CalcCreditLimit() >= 1000) {
        return HowToGetMoney::IncreaseCredit;
    }
    return HowToGetMoney::None;
}

__int64 Bot::howMuchMoneyCanWeGet(bool extremeMeasures) {
    __int64 valueCompetitorShares = 0;
    SLONG numOwnShares = 0;
    for (SLONG c = 0; c < Sim.Players.AnzPlayers; c++) {
        if (c == qPlayer.PlayerNum) {
            numOwnShares += qPlayer.OwnsAktien[c];
        } else {
            auto stockPrice = static_cast<__int64>(Sim.Players.Players[c].Kurse[0]);
            valueCompetitorShares += stockPrice * qPlayer.OwnsAktien[c];
        }
    }

    if (!extremeMeasures) {
        numOwnShares = std::max(0, numOwnShares - qPlayer.AnzAktien / 2 - 1);
    }

    __int64 moneyEmit = 0;
    __int64 moneyStock = 0;
    __int64 moneyStockOwn = 0;
    auto stockPrice = static_cast<__int64>(qPlayer.Kurse[0]);
    if (GameMechanic::canEmitStock(qPlayer) == GameMechanic::EmitStockResult::Ok) {
        __int64 newStock = 100 * (qPlayer.MaxAktien - qPlayer.AnzAktien) / 100;
        __int64 emissionsKurs = 0;
        __int64 marktAktien = 0;
        if (kStockEmissionMode == 0) {
            emissionsKurs = stockPrice - 5;
            marktAktien = newStock;
        } else if (kStockEmissionMode == 1) {
            emissionsKurs = stockPrice - 3;
            marktAktien = newStock * 8 / 10;
        } else if (kStockEmissionMode == 2) {
            emissionsKurs = stockPrice - 1;
            marktAktien = newStock * 6 / 10;
        }
        moneyEmit = marktAktien * emissionsKurs - newStock * emissionsKurs / 10 / 100 * 100;
        AT_Log("Bot::howMuchMoneyCanWeGet(): Can get %s $ by emitting stock", Insert1000erDots(moneyEmit).c_str());
    }

    if (valueCompetitorShares > 0) {
        moneyStock = valueCompetitorShares - valueCompetitorShares / 10 - 100;
        AT_Log("Bot::howMuchMoneyCanWeGet(): Can get %s $ by selling stock", Insert1000erDots(moneyStock).c_str());
    }

    if (numOwnShares > 0) {
        auto value = stockPrice * numOwnShares;
        moneyStockOwn = value - value / 10 - 100;
        AT_Log("Bot::howMuchMoneyCanWeGet(): Can get %s $ by selling our own stock", Insert1000erDots(moneyStockOwn).c_str());
    }

    __int64 moneyForecast = qPlayer.Money + moneyEmit + moneyStock + moneyStockOwn - kMoneyEmergencyFund;
    __int64 credit = qPlayer.CalcCreditLimit(moneyForecast, qPlayer.Credit);
    if (credit >= 1000) {
        moneyForecast += credit;
        AT_Log("Bot::howMuchMoneyCanWeGet(): Can get %s $ by taking a loan", Insert1000erDots(credit).c_str());
    }

    AT_Log("Bot::howMuchMoneyCanWeGet(): We can get %s $ in total!", Insert1000erDots(moneyForecast).c_str());
    return moneyForecast;
}

bool Bot::canWeCallInternational() {
    if (!qPlayer.RobotUse(ROBOT_USE_ABROAD)) {
        return false;
    }

    if (qPlayer.TelephoneDown != 0) {
        return false;
    }

    if (mPlanesForJobs.empty()) {
        return false; /* no planes */
    }
    if (!mPlanerSolution.empty()) {
        return false; /* previously grabbed flights still not scheduled */
    }

    auto res = howToPlanFlights();
    if (HowToPlan::None == res) {
        return false;
    }
    if (HowToPlan::Office == res && Sim.GetHour() >= 17) {
        return false; /* might be too late to reach office */
    }

    for (SLONG c = 0; c < 4; c++) {
        if ((c != qPlayer.PlayerNum) && (Sim.Players.Players[c].IsOut == 0) && (qPlayer.Kooperation[c] != 0)) {
            return true; /* we have a cooperation partner, we can check if they have a branch office */
        }
    }
    for (SLONG n = 0; n < Cities.AnzEntries(); n++) {
        if (n == Cities.find(Sim.HomeAirportId)) {
            continue;
        }
        if (qPlayer.RentCities.RentCities[n].Rang != 0U) {
            return true; /* we have our own branch office */
        }
    }
    return false;
}

SLONG Bot::calcCurrentGainFromJobs() const {
    SLONG gain = 0;
    for (auto planeId : mPlanesForJobs) {
        gain += Helper::calculateScheduleInfo(qPlayer, planeId).gain;
    }
    return gain;
}

SLONG Bot::calcRouteImageNeeded(const Bot::RouteInfo &routeInfo) const {
    auto routeImageTarget = std::min(100, (800 - getImage()) / 4);
    return (routeImageTarget - routeInfo.image);
}

void Bot::removePlaneFromRoute(SLONG planeId) {
    for (auto &route : mRoutes) {
        auto it = route.planeIds.begin();
        while (it != route.planeIds.end() && *it != planeId) {
            it++;
        }
        if (it != route.planeIds.end()) {
            route.planeIds.erase(it);
        }
    }
}

bool Bot::checkPlaneAvailable(SLONG planeId, bool printIfAvailable, bool areWeInOffice) {
    const auto &qPlane = qPlayer.Planes[planeId];
    if (!areWeInOffice && !checkLaptop()) {
        AT_Warn("Bot::checkPlaneAvailable: Cannot check availability of plane %s.", Helper::getPlaneName(qPlane).c_str());
        return false;
    }
    if (qPlane.AnzBegleiter < qPlane.ptAnzBegleiter) {
        AT_Error("Bot::checkPlaneAvailable: Plane %s does not have enough crew members (%d, need %d).", Helper::getPlaneName(qPlane).c_str(),
                 qPlane.AnzBegleiter, qPlane.ptAnzBegleiter);
        return false;
    }
    if (qPlane.AnzPiloten < qPlane.ptAnzPiloten) {
        AT_Error("Bot::checkPlaneAvailable: Plane %s does not have enough pilots (%d, need %d).", Helper::getPlaneName(qPlane).c_str(), qPlane.AnzPiloten,
                 qPlane.ptAnzPiloten);
        return false;
    }
    if (qPlane.Problem != 0) {
        AT_Error("Bot::checkPlaneAvailable: Plane %s has a problem for the next %d hours", Helper::getPlaneName(qPlane).c_str(), qPlane.Problem);
        return false;
    }
    if (printIfAvailable) {
        AT_Log("Bot::checkPlaneAvailable: Plane %s is available for service.", Helper::getPlaneName(qPlane).c_str());
    }
    return true;
}

bool Bot::checkPlaneLists() {
    auto &qPlanes = qPlayer.Planes;
    bool foundProblem = false;
    bool planesGoneMissing = false;
    std::unordered_map<SLONG, SLONG> uniquePlaneIds;

    std::vector<SLONG> newPlanesForJobs;
    for (const auto &i : mPlanesForJobs) {
        if (!qPlayer.Planes.IsInAlbum(i)) {
            AT_Error("Bot::checkPlaneLists(): We lost the plane with ID = %d", i);
            planesGoneMissing = true;
            continue;
        }
        if (uniquePlaneIds.find(i) != uniquePlaneIds.end()) {
            AT_Error("Bot::checkPlaneLists(): Plane with ID = %d appears in multiple lists: 1 and %d.", i, uniquePlaneIds[i]);
            foundProblem = true;
            continue;
        }
        uniquePlaneIds[i] = 1;
        newPlanesForJobs.push_back(i);
        AT_Log("Bot::checkPlaneLists(): Jobs: Plane %s gains %s $", Helper::getPlaneName(qPlanes[i]).c_str(),
               Insert1000erDots64(qPlanes[i].GetSaldo()).c_str());
    }
    std::swap(mPlanesForJobs, newPlanesForJobs);

    std::vector<SLONG> newPlanesForRoutes;
    for (const auto &i : mPlanesForRoutes) {
        if (!qPlayer.Planes.IsInAlbum(i)) {
            AT_Error("Bot::checkPlaneLists(): We lost the plane with ID = %d", i);
            removePlaneFromRoute(i);
            planesGoneMissing = true;
            continue;
        }
        if (uniquePlaneIds.find(i) != uniquePlaneIds.end()) {
            AT_Error("Bot::checkPlaneLists(): Plane with ID = %d appears in multiple lists: 2 and %d.", i, uniquePlaneIds[i]);
            foundProblem = true;
            continue;
        }
        uniquePlaneIds[i] = 2;
        newPlanesForRoutes.push_back(i);
        AT_Log("Bot::checkPlaneLists(): Routes: Plane %s gains %s $", Helper::getPlaneName(qPlanes[i]).c_str(),
               Insert1000erDots64(qPlanes[i].GetSaldo()).c_str());
    }
    std::swap(mPlanesForRoutes, newPlanesForRoutes);

    std::deque<SLONG> newPlanesJobsUnassigned;
    for (const auto &i : mPlanesForJobsUnassigned) {
        if (!qPlayer.Planes.IsInAlbum(i)) {
            AT_Error("Bot::checkPlaneLists(): We lost the plane with ID = %d", i);
            planesGoneMissing = true;
            continue;
        }
        if (uniquePlaneIds.find(i) != uniquePlaneIds.end()) {
            AT_Error("Bot::checkPlaneLists(): Plane with ID = %d appears in multiple lists: 3 and %d.", i, uniquePlaneIds[i]);
            foundProblem = true;
            continue;
        }
        uniquePlaneIds[i] = 3;
        newPlanesJobsUnassigned.push_back(i);
        AT_Log("Bot::checkPlaneLists(): Jobs unassigned: Plane %s gains %s $", Helper::getPlaneName(qPlanes[i]).c_str(),
               Insert1000erDots64(qPlanes[i].GetSaldo()).c_str());
    }
    std::swap(mPlanesForJobsUnassigned, newPlanesJobsUnassigned);

    std::deque<SLONG> newPlanesRoutesUnassigned;
    for (const auto &i : mPlanesForRoutesUnassigned) {
        if (!qPlayer.Planes.IsInAlbum(i)) {
            AT_Error("Bot::checkPlaneLists(): We lost the plane with ID = %d", i);
            planesGoneMissing = true;
            continue;
        }
        if (uniquePlaneIds.find(i) != uniquePlaneIds.end()) {
            AT_Error("Bot::checkPlaneLists(): Plane with ID = %d appears in multiple lists: 4 and %d.", i, uniquePlaneIds[i]);
            foundProblem = true;
            continue;
        }
        uniquePlaneIds[i] = 4;
        newPlanesRoutesUnassigned.push_back(i);
        AT_Log("Bot::checkPlaneLists(): Routes unassigned: Plane %s gains %s $", Helper::getPlaneName(qPlanes[i]).c_str(),
               Insert1000erDots64(qPlanes[i].GetSaldo()).c_str());
    }
    std::swap(mPlanesForRoutesUnassigned, newPlanesRoutesUnassigned);

    for (SLONG i = 0; i < qPlayer.Planes.AnzEntries(); i++) {
        SLONG id = qPlayer.Planes.GetIdFromIndex(i);
        if (qPlayer.Planes.IsInAlbum(i) && uniquePlaneIds.find(id) == uniquePlaneIds.end()) {
            const auto &qPlane = qPlayer.Planes[i];
            AT_Error("Bot::checkPlaneLists(): Found new plane: %s (%lx).", Helper::getPlaneName(qPlane).c_str(), id);
            mPlanesForJobsUnassigned.push_back(id);
            foundProblem = true;
        }
    }

    /* check if we still have enough personal */
    findPlanesNotAvailableForService(mPlanesForJobs, mPlanesForJobsUnassigned);
    findPlanesNotAvailableForService(mPlanesForRoutes, mPlanesForRoutesUnassigned);

    /* maybe some planes now have crew? planes for routes will be checked in planRoutes() */
    findPlanesAvailableForService(mPlanesForJobsUnassigned, mPlanesForJobs);

    AT_Log("Bot::checkPlaneLists(): Planes for jobs: %d / %d are available.", mPlanesForJobs.size(), mPlanesForJobs.size() + mPlanesForJobsUnassigned.size());
    AT_Log("Bot::checkPlaneLists(): Planes for routes: %d / %d are available.", mPlanesForRoutes.size(),
           mPlanesForRoutes.size() + mPlanesForRoutesUnassigned.size());

    return foundProblem || planesGoneMissing;
}

void Bot::findPlanesNotAvailableForService(std::vector<SLONG> &listAvailable, std::deque<SLONG> &listUnassigned) {
    std::vector<SLONG> newAvailable;
    for (const auto id : listAvailable) {
        auto &qPlane = qPlayer.Planes[id];
        auto worstZustand = std::min(qPlane.WorstZustand, qPlane.Zustand);
        AT_Log("Bot::findPlanesNotAvailableForService(): Plane %s: Zustand = %u, WorstZustand = %u, Baujahr = %d", Helper::getPlaneName(qPlane).c_str(),
               qPlane.Zustand, worstZustand, qPlane.Baujahr);

        SLONG mode = 0; /* 0: keep plane in service */
        if (qPlane.Zustand <= kPlaneMinimumZustand) {
            AT_Log("Bot::findPlanesNotAvailableForService(): Plane %s not available for service: Needs repairs.", Helper::getPlaneName(qPlane).c_str());
            mode = 1; /* 1: phase plane out */
        }
        if (!checkPlaneAvailable(id, false, true)) {
            AT_Error("Bot::findPlanesNotAvailableForService(): Plane %s not available for service: No crew.", Helper::getPlaneName(qPlane).c_str());
            mode = 2; /* 2: remove plane immediately from service */
        }
        if (mode > 0) {
            listUnassigned.push_back(id);
            if (mode == 2) {
                GameMechanic::clearFlightPlan(qPlayer, id);
            }

            /* remove plane from route */
            removePlaneFromRoute(id);
        } else {
            newAvailable.push_back(id);
        }
    }
    std::swap(listAvailable, newAvailable);
}

void Bot::findPlanesAvailableForService(std::deque<SLONG> &listUnassigned, std::vector<SLONG> &listAvailable) {
    std::deque<SLONG> newUnassigned;
    for (const auto id : listUnassigned) {
        auto &qPlane = qPlayer.Planes[id];
        auto worstZustand = std::min(qPlane.WorstZustand, qPlane.Zustand);
        AT_Log("Bot::findPlanesAvailableForService(): Plane %s: Zustand = %u, WorstZustand = %u, Baujahr = %d", Helper::getPlaneName(qPlane).c_str(),
               qPlane.Zustand, worstZustand, qPlane.Baujahr);

        if (qPlane.Zustand < 100 && (qPlane.Zustand < qPlane.TargetZustand)) {
            AT_Log("Bot::findPlanesAvailableForService(): Plane %s still not available for service: Needs repairs.", Helper::getPlaneName(qPlane).c_str());
            newUnassigned.push_back(id);
        } else if (!checkPlaneAvailable(id, false, true)) {
            AT_Error("Bot::findPlanesAvailableForService(): Plane %s still not available for service: No crew.", Helper::getPlaneName(qPlane).c_str());
            newUnassigned.push_back(id);
        } else {
            AT_Log("Bot::findPlanesAvailableForService(): Putting plane %s back into service.", Helper::getPlaneName(qPlane).c_str());
            listAvailable.push_back(id);
        }
    }
    std::swap(listUnassigned, newUnassigned);
}

const CRentRoute &Bot::getRentRoute(const Bot::RouteInfo &routeInfo) const { return qPlayer.RentRouten.RentRouten[routeInfo.routeId]; }

const CRoute &Bot::getRoute(const Bot::RouteInfo &routeInfo) const { return Routen[routeInfo.routeId]; }

__int64 Bot::getDailyOpSaldo() const { return qPlayer.BilanzGestern.GetOpSaldo(); }

__int64 Bot::getWeeklyOpSaldo() const { return qPlayer.BilanzWoche.Hole().GetOpSaldo(); }

bool Bot::isLateGame() const { return (getWeeklyOpSaldo() > 1e8); }

SLONG Bot::getImage() const { return (qPlayer.HasBerater(BERATERTYP_GELD) < 50) ? mCurrentImage : qPlayer.Image; }

void Bot::forceReplanning() { qPlayer.RobotActions[1].ActionId = ACTION_NONE; }

void Bot::setHardcodedDesignerPlaneLarge() {
    mDesignerPlane.Name = "Bot Beluga";
    mDesignerPlane.Parts.ReSize(7);
    mDesignerPlane.Parts.FillAlbum();
    mDesignerPlane.Parts[0].Pos2d.x = -35;
    mDesignerPlane.Parts[0].Pos2d.y = -95;
    mDesignerPlane.Parts[0].Pos3d.x = 308;
    mDesignerPlane.Parts[0].Pos3d.y = 69;
    mDesignerPlane.Parts[0].Shortname = "M7";
    mDesignerPlane.Parts[0].ParentShortname = "R6";
    mDesignerPlane.Parts[0].ParentRelationId = 298;
    mDesignerPlane.Parts[1].Pos2d.x = -90;
    mDesignerPlane.Parts[1].Pos2d.y = -103;
    mDesignerPlane.Parts[1].Pos3d.x = 216;
    mDesignerPlane.Parts[1].Pos3d.y = 55;
    mDesignerPlane.Parts[1].Shortname = "L6";
    mDesignerPlane.Parts[1].ParentShortname = "B2";
    mDesignerPlane.Parts[1].ParentRelationId = 92;
    mDesignerPlane.Parts[2].Pos2d.x = 107;
    mDesignerPlane.Parts[2].Pos2d.y = -152;
    mDesignerPlane.Parts[2].Pos3d.x = 62;
    mDesignerPlane.Parts[2].Pos3d.y = 9;
    mDesignerPlane.Parts[2].Shortname = "H3";
    mDesignerPlane.Parts[2].ParentShortname = "B2";
    mDesignerPlane.Parts[2].ParentRelationId = 39;
    mDesignerPlane.Parts[3].Pos2d.x = -107;
    mDesignerPlane.Parts[3].Pos2d.y = -120;
    mDesignerPlane.Parts[3].Pos3d.x = 174;
    mDesignerPlane.Parts[3].Pos3d.y = 99;
    mDesignerPlane.Parts[3].Shortname = "B2";
    mDesignerPlane.Parts[3].ParentShortname.clear();
    mDesignerPlane.Parts[3].ParentRelationId = 1;
    mDesignerPlane.Parts[4].Pos2d.x = -171;
    mDesignerPlane.Parts[4].Pos2d.y = -83;
    mDesignerPlane.Parts[4].Pos3d.x = 424;
    mDesignerPlane.Parts[4].Pos3d.y = 204;
    mDesignerPlane.Parts[4].Shortname = "C2";
    mDesignerPlane.Parts[4].ParentShortname = "B2";
    mDesignerPlane.Parts[4].ParentRelationId = 11;
    mDesignerPlane.Parts[5].Pos2d.x = -50;
    mDesignerPlane.Parts[5].Pos2d.y = -30;
    mDesignerPlane.Parts[5].Pos3d.x = 126;
    mDesignerPlane.Parts[5].Pos3d.y = 243;
    mDesignerPlane.Parts[5].Shortname = "M7";
    mDesignerPlane.Parts[5].ParentShortname = "R6";
    mDesignerPlane.Parts[5].ParentRelationId = 297;
    mDesignerPlane.Parts[6].Pos2d.x = -90;
    mDesignerPlane.Parts[6].Pos2d.y = -78;
    mDesignerPlane.Parts[6].Pos3d.x = 92;
    mDesignerPlane.Parts[6].Pos3d.y = 169;
    mDesignerPlane.Parts[6].Shortname = "R6";
    mDesignerPlane.Parts[6].ParentShortname = "B2";
    mDesignerPlane.Parts[6].ParentRelationId = 91;
}

void Bot::setHardcodedDesignerPlaneEco() {
    mDesignerPlane.Name = "Bot Ecomaster";
    mDesignerPlane.Parts.ReSize(7);
    mDesignerPlane.Parts.FillAlbum();
    mDesignerPlane.Parts[0].Pos2d.x = -53;
    mDesignerPlane.Parts[0].Pos2d.y = -93;
    mDesignerPlane.Parts[0].Pos3d.x = 408;
    mDesignerPlane.Parts[0].Pos3d.y = 82;
    mDesignerPlane.Parts[0].Shortname = "M2";
    mDesignerPlane.Parts[0].ParentShortname = "R4";
    mDesignerPlane.Parts[0].ParentRelationId = 244;
    mDesignerPlane.Parts[1].Pos2d.x = -40;
    mDesignerPlane.Parts[1].Pos2d.y = -108;
    mDesignerPlane.Parts[1].Pos3d.x = 290;
    mDesignerPlane.Parts[1].Pos3d.y = 41;
    mDesignerPlane.Parts[1].Shortname = "L4";
    mDesignerPlane.Parts[1].ParentShortname = "B1";
    mDesignerPlane.Parts[1].ParentRelationId = 66;
    mDesignerPlane.Parts[2].Pos2d.x = 45;
    mDesignerPlane.Parts[2].Pos2d.y = -121;
    mDesignerPlane.Parts[2].Pos3d.x = 168;
    mDesignerPlane.Parts[2].Pos3d.y = 52;
    mDesignerPlane.Parts[2].Shortname = "H4";
    mDesignerPlane.Parts[2].ParentShortname = "B1";
    mDesignerPlane.Parts[2].ParentRelationId = 33;
    mDesignerPlane.Parts[3].Pos2d.x = -44;
    mDesignerPlane.Parts[3].Pos2d.y = -75;
    mDesignerPlane.Parts[3].Pos3d.x = 247;
    mDesignerPlane.Parts[3].Pos3d.y = 137;
    mDesignerPlane.Parts[3].Shortname = "B1";
    mDesignerPlane.Parts[3].ParentShortname.clear();
    mDesignerPlane.Parts[3].ParentRelationId = 0;
    mDesignerPlane.Parts[4].Pos2d.x = -92;
    mDesignerPlane.Parts[4].Pos2d.y = -76;
    mDesignerPlane.Parts[4].Pos3d.x = 350;
    mDesignerPlane.Parts[4].Pos3d.y = 167;
    mDesignerPlane.Parts[4].Shortname = "C1";
    mDesignerPlane.Parts[4].ParentShortname = "B1";
    mDesignerPlane.Parts[4].ParentRelationId = 5;
    mDesignerPlane.Parts[5].Pos2d.x = -53;
    mDesignerPlane.Parts[5].Pos2d.y = -4;
    mDesignerPlane.Parts[5].Pos3d.x = 189;
    mDesignerPlane.Parts[5].Pos3d.y = 275;
    mDesignerPlane.Parts[5].Shortname = "M2";
    mDesignerPlane.Parts[5].ParentShortname = "R4";
    mDesignerPlane.Parts[5].ParentRelationId = 243;
    mDesignerPlane.Parts[6].Pos2d.x = -40;
    mDesignerPlane.Parts[6].Pos2d.y = -41;
    mDesignerPlane.Parts[6].Pos3d.x = 119;
    mDesignerPlane.Parts[6].Pos3d.y = 196;
    mDesignerPlane.Parts[6].Shortname = "R4";
    mDesignerPlane.Parts[6].ParentShortname = "B1";
    mDesignerPlane.Parts[6].ParentRelationId = 65;
}