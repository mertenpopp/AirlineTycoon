#include "Bot.h"

#include "BotHelper.h"
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
#include <unordered_map>
#include <vector>

template <class... Types> void AT_Error(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_ERROR, "Bot", args...); }
template <class... Types> void AT_Warn(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_WARN, "Bot", args...); }
template <class... Types> void AT_Info(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_INFO, "Bot", args...); }
template <class... Types> void AT_Log(Types... args) { AT_Log_I("Bot", args...); }

/* Room: any (only reads VonCity/NachCity) */
inline SLONG getRouteBaseCost(const CRoute &qRoute) { return CalculateFlightCost(qRoute.VonCity, qRoute.NachCity, 800, 800, -1) * 3 / 180 * 2; }

/* Room: any (only checks Rang != 0) */
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

/* Room: office or laptop (clearFlightPlan()) */
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

/* Room: any (cached data only) */
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

/* Room: office or laptop; office only if areWeInOffice == true (reads plane upgrade targets) */
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
            __int64 estimatedWeeklyRevenue = calcRouteScore(route.routeId, route.planeTypeId, tmpList, true).score;
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

/* Room: route box (ACTION_VISITROUTEBOX), because of competitor Rang */
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

/* Room: any (plans right away in office or with laptop, otherwise defers to the office) */
void Bot::requestPlanRoutes(bool areWeInOffice) {
    if (mRoutes.empty() || (mRoutesToRemove && !mDoRoutes)) {
        return; /* no route yet or only starter route */
    }
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

/* Room: any (cached data only, also called from RobotPlan()) */
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
        if (mDoRoutes) {
            AT_Error("Bot::routesRecalcNextStep(): No strategy!");
        } else {
            AT_Log("Bot::routesRecalcNextStep(): None, because routes are not our primary strategy yet");
        }
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

/* Room: any (cached data only) */
std::pair<Bot::RoutesNextStep, SLONG> Bot::routesFindNextStep() const {
    assert(mRoutesUpdated && mRoutesUtilizationUpdated);

    /* find route with not enough planes */
    SLONG routeToBuyPlanes = -1;
    for (auto i : mRoutesSortedByOwnUtilization) {
        if (mRoutes[i].planeTypeId == -1) {
            continue;
        }
        const auto &qRoute = getRoute(mRoutes[i]);
        const auto numPlanes = static_cast<SLONG>(mRoutes[i].planeIds.size());
        if (isMissionRoute(qRoute)) {
            /* the goal needs our own share above the threshold, whoever else flies the route: keep adding planes while
             * we are short of it, up to twice the planned number */
            if (mRoutes[i].routeOwnUtilization < routeUtilizationTarget(qRoute) && numPlanes < 2 * std::max(1, mRoutes[i].numberOfPlanesTarget)) {
                routeToBuyPlanes = i;
                break;
            }
        } else if (numPlanes < mRoutes[i].numberOfPlanesTarget) {
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
        if (mRoutes[i].planeTypeId == -1) {
            continue;
        }
        if (mRoutes[i].image < lowestImage) {
            routeWithLowImage = i;
            lowestImage = mRoutes[i].image;
        }
    }

    /* find route with pending plane upgrades */
    SLONG routeWithPendingPlaneUpgrades = -1;
    for (auto i : mRoutesSortedByOwnUtilization) {
        if (mRoutes[i].planeTypeId == -1) {
            continue;
        }
        if (mRoutes[i].canUpgrade) {
            routeWithPendingPlaneUpgrades = i;
            break;
        }
    }

    bool canBuyAdsToday = qPlayer.RobotUse(ROBOT_USE_WERBUNG) && (Sim.Weekday != 5 && Sim.Weekday != 6);

    /* Step 1: Is the default, at the bottom */

    /* Step 2: Buy additional plane when we have the money */
    if (routeToBuyPlanes != -1 && mDoRoutes) {
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
    if (routeToBuyPlanes != -1 && mDoRoutes) {
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
    if (routeToBuyPlanes != -1 && mDoRoutes) {
        return {RoutesNextStep::BuyMorePlanes, routeToBuyPlanes};
    }

    /* We do not return RoutesNextStep::ImproveAirlineImage anymore, this is handled in parallel */

    if (mDoRoutes) { /* routes are our primary strategy */
        /* Step 1: No routes underutilized, rent new route */
        return {RoutesNextStep::RentNewRoute, -1};
    }
    if (mRoutes.empty() && !mPlanesForRoutesUnassigned.empty()) { /* routes not the primary strategy yet */
        return {RoutesNextStep::RentNewRoute, -1};
    }
    return {RoutesNextStep::None, -1};
}

/* Room: any */
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

/* Room: any (only reads VonCity/NachCity). True for a route between home and a mission city in a route mission. */
bool Bot::isMissionRoute(const CRoute &qRoute) const {
    if (!qPlayer.RobotUse(ROBOT_USE_ROUTEMISSION)) {
        return false;
    }
    auto homeAirport = static_cast<ULONG>(Sim.HomeAirportId);
    for (SLONG d = 0; d < 6; d++) {
        auto missionCity = static_cast<ULONG>(Sim.MissionCities[d]);
        if ((qRoute.VonCity == homeAirport && qRoute.NachCity == missionCity) || (qRoute.NachCity == homeAirport && qRoute.VonCity == missionCity)) {
            return true;
        }
    }
    return false;
}

/* Room: any. Own utilization in % of weekly demand we size a route for. */
/* Own route utilization aimed for on a mission route (DIFF_NORMAL). The goal counts a direction only above
 * 20% (Aufsicht.cpp), so aiming at exactly 20% left routes hovering around the threshold. */
SLONG Bot::routeUtilizationTarget(const CRoute &qRoute) const { return isMissionRoute(qRoute) ? 30 : mOptions.kMaximumRouteUtilization; }

/* Room: route box (CRoute::Miete, CRoute::AnzPassagiere()) */
Bot::RouteScore Bot::calcRouteScore(SLONG routeId, SLONG planeTypeId, std::unordered_map<SLONG, std::vector<SLONG>> &existingPlaneIds, bool canBuy) {
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
    const bool missionRoute = isMissionRoute(qRoute);
    SLONG targetSharePercent = routeUtilizationTarget(qRoute);
    if (missionRoute) {
        /* we need this route whoever else flies it: competitors do not reduce our target */
    } else if (qPlayer.HasBerater(BERATERTYP_INFO) > 0) {
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

    /* account for the fact that we already have suitable planes */
    SLONG numExistingPlanes = static_cast<SLONG>(existingPlaneIds[planeTypeId].size());
    SLONG planesToBuy = std::max(0, numPlanesMin - numExistingPlanes);

    /* base score on the following number of planes */
    SLONG numPlanes = canBuy ? numPlanesTarget : numExistingPlanes;

    /* estimate revenue */
    __int64 baseCost = getRouteBaseCost(qRoute);
    __int64 revenue = qPlaneType.Passagiere * baseCost * 3.0 * mOptions.kMaxTicketPriceFactor.target;
    SLONG numTripsPerWeek = 24 * 7 / duration;
    __int64 profitPerWeek = (revenue - cost) * numTripsPerWeek * numPlanes - (qRoute.Miete / 30 * 2 * 7);

    if (!canBuy && numPlanes < numPlanesMin) {
        profitPerWeek = 0; /* we initially won't buy planes for this route, so we need to meet minimum right away */
    }

    /* is this route important for our mission */
    if (missionRoute) {
        AT_Log("Bot::calcRouteScore(): Route %s is important for mission, increasing score.", Helper::getRouteName(qRoute).c_str());
        profitPerWeek *= 10;
    }
    return {profitPerWeek, routeId, planeTypeId, existingPlaneIds[planeTypeId], planesToBuy};
}

/* Room: route box (getBuyableRoutes()) */
void Bot::findBestRoute() {
    mWantToRentRouteId = -1;
    mPlaneTypeForNewRoute = -1;
    mPlanesForNewRoute.clear();

    std::vector<std::pair<SLONG, bool>> planeTypeIds;
    if (qPlayer.RobotUse(ROBOT_USE_DESIGNER_BUY) && mDesignerPlaneType.Passagiere > 0) {
        planeTypeIds.emplace_back(kDesignerPlaneTypeId, true);
    } else {
        for (const auto &typeId : mKnownPlaneTypes) {
            planeTypeIds.emplace_back(typeId, true);
        }
    }

    /* check existing planes */
    std::unordered_map<SLONG, std::vector<SLONG>> existingPlaneIds;
    if (mRoutes.empty()) {
        for (const auto id : mPlanesForRoutesUnassigned) {
            auto &qPlane = qPlayer.Planes[id];
            existingPlaneIds[getRoutePlaneTypeId(qPlane)].emplace_back(id);

            SLONG typeId = getRoutePlaneTypeId(qPlane);
            if (planeTypeIds.end() == std::find_if(planeTypeIds.begin(), planeTypeIds.end(), [typeId](const auto &p) { return p.first == typeId; })) {
                planeTypeIds.emplace_back(typeId, false);
            }
        }
    }

    /* the designer plane is the only plane we buy in designer missions, so plan routes for it */

    std::vector<RouteScore> bestRoutes;
    auto isBuyable = GameMechanic::getBuyableRoutes(qPlayer);
    for (SLONG c = 0; c < Routen.AnzEntries(); c++) {
        if (isBuyable[c] == 0) {
            continue;
        }
        if (Routen[c].VonCity > Routen[c].NachCity) {
            continue; /* we only need to check one of each pair */
        }
        for (const auto &i : planeTypeIds) {
            SLONG planeTypeId = i.first;
            bool canBuy = i.second && mDoRoutes;
            if (planeTypeId != kDesignerPlaneTypeId && !PlaneTypes.IsInAlbum(planeTypeId)) {
                continue;
            }

            RouteScore score = calcRouteScore(c, planeTypeId, existingPlaneIds, canBuy);
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

/* Room: route box if planeTypeForNewRoute != -1 (CRoute::AnzPassagiere()), any otherwise */
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
        numberOfPlanesTarget =
            Helper::getNumberOfPlanesNeededForRoute(Routen[routeA], getPlaneType(planeTypeForNewRoute), routeUtilizationTarget(Routen[routeA]));
        numberOfPlanesTarget *= 2; /* for each route leg */
    }
    /* a route for planes we already own (the starter plane) keeps that many planes,
     * so that the next route is planned for the best plane type we can buy */
    if (!mDoRoutes) {
        numberOfPlanesTarget = mPlanesForNewRoute.size();
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

/* Room: any (bookkeeping only) */
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

/* Room: route box, office or laptop (killRoute()) */
SLONG Bot::removeInvalidatedRoutes() {
    /* kill routes marked for deletion (no plane type id assigned) */
    SLONG numWaitForRemoval = 0;
    auto it = mRoutes.begin();
    while (it != mRoutes.end()) {
        if (it->planeTypeId == -1) {
            SLONG routeID = it->routeId;
            if (GameMechanic::getAnyPlaneOnRoute(qPlayer, routeID) != -1) {
                AT_Log("Bot::removeInvalidatedRoutes(): Cannot remove route %s, still in flightplans", Helper::getRouteName(Routen[routeID]).c_str());
                numWaitForRemoval++;
                ++it; /* only increase if not erased */
            } else {
                GameMechanic::killRoute(qPlayer, routeID);
                it = removeRoute(it);
                AT_Log("Bot::removeInvalidatedRoutes(): Removing route %s", Helper::getRouteName(Routen[routeID]).c_str());
            }
        } else {
            ++it; /* only increase if not erased */
        }
    }
    mRoutesToRemove = (numWaitForRemoval > 0);
    return numWaitForRemoval;
}

/* Room: office or laptop (clearFlightPlan()) */
void Bot::releaseStarterRoutes() {
    if (qPlayer.RobotUse(ROBOT_USE_FORCEROUTES)) {
        return;
    }
    for (auto &qRoute : mRoutes) {
        if (qRoute.planeTypeId == -1) {
            continue;
        }
        AT_Log("Bot::releaseStarterRoutes(): Giving up starter route %s", Helper::getRouteName(getRoute(qRoute)).c_str());
        for (auto planeId : qRoute.planeIds) {
            GameMechanic::clearFlightPlan(qPlayer, planeId);
            auto it = std::find(mPlanesForRoutes.begin(), mPlanesForRoutes.end(), planeId);
            if (it != mPlanesForRoutes.end()) {
                mPlanesForRoutes.erase(it);
            }
            mPlanesForJobsUnassigned.push_back(planeId);
            AT_Log("Bot::releaseStarterRoutes(): Plane %s flies jobs again", Helper::getPlaneName(qPlayer.Planes[planeId]).c_str());
        }
        qRoute.planeIds.clear();
        qRoute.planeTypeId = -1;
        mRoutesToRemove = true;
    }
    if (mRoutesToRemove) {
        removeInvalidatedRoutes();
    }
}

/* Room: office or laptop */
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

        /* a starter route (only starter planes) keeps its price low: every flight above 150% costs airline image
         * (Schedule.cpp, "Preispolitik bewerten"), which the first route for bought planes would inherit */
        const RoutePriceLevels &factors =
            (mDoRoutes && Sim.Date >= mImagePreservationMode) ? mOptions.kMaxTicketPriceFactor : mOptions.kMaxTicketPriceFactorLowImage;
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

/* Room: any; areWeInOffice must be truthful */
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
            const auto &qRouteData = getRoute(qRoute);
            if (isMissionRoute(qRouteData)) {
                if (qRoute.routeOwnUtilization >= routeUtilizationTarget(qRouteData)) {
                    continue; /* competitors do not count against a mission route, only our own share does */
                }
            } else if (qRoute.routeUtilization >= mOptions.kMaximumRouteUtilization) {
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
