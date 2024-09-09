#include "Bot.h"

#include "BotHelper.h"
#include "GameMechanic.h"
#include "global.h"
#include "TeakLibW.h"

#define AT_Error(...) Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_ERROR, "Bot", __VA_ARGS__)
#define AT_Warn(...) Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_WARN, "Bot",__VA_ARGS__)
#define AT_Info(...) Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_INFO, "Bot",__VA_ARGS__)
#define AT_Log(...) AT_Log_I("Bot", __VA_ARGS__)

// Preise verstehen sich pro Sitzplatz:
extern SLONG SeatCosts[];

Bot::Prio Bot::condAll(SLONG actionId) {
    __int64 moneyAvailable = 0;
    switch (actionId) {
    case ACTION_RAISEMONEY:
        return condTakeOutLoan();
    case ACTION_DROPMONEY:
        return condDropMoney(moneyAvailable);
    case ACTION_EMITSHARES:
        return condEmitShares();
    case ACTION_CHECKAGENT1:
        return condCheckLastMinute();
    case ACTION_CHECKAGENT2:
        return condCheckTravelAgency();
    case ACTION_CHECKAGENT3:
        return condCheckFreight();
    case ACTION_STARTDAY:
        return condStartDay();
    case ACTION_BUERO:
        return condBuero();
    case ACTION_PERSONAL:
        return condVisitHR();
    case ACTION_VISITKIOSK:
        return condVisitMisc();
    case ACTION_VISITMECH:
        return condVisitMech();
    case ACTION_VISITMUSEUM:
        return condVisitMuseum();
    case ACTION_VISITDUTYFREE:
        return condVisitDutyFree(moneyAvailable);
    case ACTION_VISITAUFSICHT:
        return condVisitBoss(moneyAvailable);
    case ACTION_VISITNASA:
        return condVisitNasa(moneyAvailable);
    case ACTION_VISITTELESCOPE:
        return condVisitMisc();
    case ACTION_VISITMAKLER:
        return condVisitMakler();
    case ACTION_VISITARAB:
        return condVisitArab();
    case ACTION_VISITRICK:
        return condVisitRick();
    case ACTION_VISITROUTEBOX:
        return condVisitRouteBoxPlanning();
    case ACTION_VISITSECURITY:
        return condVisitSecurity(moneyAvailable);
    case ACTION_VISITDESIGNER:
        return condVisitDesigner(moneyAvailable);
    case ACTION_VISITSECURITY2:
        return condSabotageSecurity();
    case ACTION_BUYUSEDPLANE:
        return condBuyUsedPlane(moneyAvailable);
    case ACTION_BUYNEWPLANE:
        return condBuyNewPlane(moneyAvailable);
    case ACTION_WERBUNG:
        return condBuyAds(moneyAvailable);
    case ACTION_SABOTAGE:
        return condSabotage(moneyAvailable);
    case ACTION_UPGRADE_PLANES:
        return condUpgradePlanes();
    case ACTION_BUY_KEROSIN:
        return condBuyKerosine(moneyAvailable);
    case ACTION_BUY_KEROSIN_TANKS:
        return condBuyKerosineTank(moneyAvailable);
    case ACTION_SET_DIVIDEND:
        return condIncreaseDividend(moneyAvailable);
    case ACTION_BUYSHARES:
        return condBuyShares(moneyAvailable);
    case ACTION_SELLSHARES:
        return condSellShares(moneyAvailable);
    case ACTION_WERBUNG_ROUTES:
        return condBuyAdsForRoutes(moneyAvailable);
    case ACTION_CALL_INTERNATIONAL:
        return condCallInternational();
    case ACTION_VISITROUTEBOX2:
        return condVisitRouteBoxRenting(moneyAvailable);
    case ACTION_EXPANDAIRPORT:
        return condExpandAirport(moneyAvailable);
    case ACTION_CALL_INTER_HANDY:
        return condCallInternationalHandy();
    case ACTION_STARTDAY_LAPTOP:
        return condStartDayLaptop();
    case ACTION_VISITADS:
        return condVisitAds();
    case ACTION_OVERTAKE_AIRLINE:
        return condOvertakeAirline();
    default:
        AT_Error("Bot::condAll(): Default case should not be reached.");
        return Prio::None;
    }
    return Prio::None;
}

/** How to write condXYZ() function
 * If action costs money: Initialize moneyAvailable
 * Check if enough hours passed since last time action was executed.
 * Check if all other conditions for this actions are met (e.g. RobotUse(), is action even possible right now?)
 * If actions costs money:
 *      Reduce moneyAvailable by amount that should not be spent for this action
 *      Check if moneyAvailable is still larger/equal than minimum amount necessary for action (don't modify moneyAvailable again!)
 * Return priority of this action.
 */

Bot::Prio Bot::condStartDay() {
    /* not necessary to check hoursPassed() */
    if (mDayStarted || !isOfficeUsable()) {
        return Prio::None;
    }
    return Prio::Top;
}

Bot::Prio Bot::condStartDayLaptop() {
    /* not necessary to check hoursPassed() */
    if (mDayStarted || isOfficeUsable()) {
        return Prio::None;
    }
    if (qPlayer.HasItem(ITEM_LAPTOP) && qPlayer.LaptopVirus == 0) {
        return Prio::Top;
    }
    return Prio::None;
}

Bot::Prio Bot::condBuero() {
    /* not necessary to check hoursPassed() */
    if (!isOfficeUsable()) {
        return Prio::None; /* office is destroyed */
    }
    if (mNeedToPlanJobs) {
        return Prio::Top;
    }
    if (mNeedToPlanRoutes) {
        return Prio::Top;
    }
    if (mDoRoutes) {
        if (!mRoutesUpdated) {
            return Prio::Medium; /* update cached route info */
        }
        if (mRoutesUtilizationUpdated && mRoutesNextStep == RoutesNextStep::None) {
            return Prio::Medium; /* generate route strategy if other info is already updated */
        }
    }
    return Prio::None;
}

Bot::Prio Bot::condCallInternational() {
    if (qPlayer.HasItem(ITEM_HANDY)) {
        return Prio::None; /* we rather use mobile to call */
    }
    if (!isOfficeUsable()) {
        return Prio::None; /* office is destroyed */
    }
    if (!canWeCallInternational()) {
        return Prio::None;
    }
    if (hoursPassed(ACTION_CALL_INTERNATIONAL, 2)) {
        return Prio::High;
    }
    return qPlayer.RobotUse(ROBOT_USE_NOCHITCHAT) ? Prio::Low : Prio::None;
}

Bot::Prio Bot::condCallInternationalHandy() {
    if (!qPlayer.HasItem(ITEM_HANDY) || qPlayer.IsStuck != 0) {
        return Prio::None; /* cannot use mobile */
    }
    if (!canWeCallInternational()) {
        return Prio::None;
    }
    if (hoursPassed(ACTION_CALL_INTER_HANDY, 2)) {
        return Prio::High;
    }
    return qPlayer.RobotUse(ROBOT_USE_NOCHITCHAT) ? Prio::Low : Prio::None;
}

Bot::Prio Bot::condCheckLastMinute() {
    if (mPlanesForJobs.empty()) {
        return Prio::None; /* no planes */
    }
    if (!mPlanerSolution.empty()) {
        return Prio::None; /* previously grabbed flights still not scheduled */
    }

    auto res = canWePlanFlights();
    if (HowToPlan::Office == res && Sim.GetHour() >= 17) {
        return Prio::None; /* might be too late to reach office */
    }
    if (HowToPlan::None == res) {
        return Prio::None;
    }

    if (hoursPassed(ACTION_CHECKAGENT1, 2)) {
        return Prio::High;
    }
    return qPlayer.RobotUse(ROBOT_USE_NOCHITCHAT) ? Prio::Low : Prio::None;
}

Bot::Prio Bot::condCheckTravelAgency() {
    if (mPlanesForJobs.empty()) {
        return Prio::None; /* no planes */
    }
    if (!mPlanerSolution.empty()) {
        return Prio::None; /* previously grabbed flights still not scheduled */
    }

    auto res = canWePlanFlights();
    if (HowToPlan::Office == res && Sim.GetHour() >= 17) {
        return Prio::None; /* might be too late to reach office */
    }
    if (HowToPlan::None == res) {
        return Prio::None;
    }

    if (hoursPassed(ACTION_CHECKAGENT2, 2)) {
        return qPlayer.RobotUse(ROBOT_USE_MUCH_FRACHT) ? Prio::High : Prio::Higher;
    }
    if (mItemAntiVirus == 0) {
        return Prio::Low; /* collect tarantula */
    }
    return qPlayer.RobotUse(ROBOT_USE_NOCHITCHAT) ? Prio::Low : Prio::None;
}

Bot::Prio Bot::condCheckFreight() {
    if (!qPlayer.RobotUse(ROBOT_USE_FRACHT)) {
        return Prio::None;
    }

    if (mPlanesForJobs.empty()) {
        return Prio::None; /* no planes */
    }
    if (!mPlanerSolution.empty()) {
        return Prio::None; /* previously grabbed flights still not scheduled */
    }

    auto res = canWePlanFlights();
    if (HowToPlan::Office == res && Sim.GetHour() >= 17) {
        return Prio::None; /* might be too late to reach office */
    }
    if (HowToPlan::None == res) {
        return Prio::None;
    }

    if (hoursPassed(ACTION_CHECKAGENT3, 2)) {
        return qPlayer.RobotUse(ROBOT_USE_MUCH_FRACHT) ? Prio::Higher : Prio::High;
    }
    return qPlayer.RobotUse(ROBOT_USE_NOCHITCHAT) ? Prio::Low : Prio::None;
}

Bot::Prio Bot::condUpgradePlanes() {
    if (!hoursPassed(ACTION_UPGRADE_PLANES, 1)) { /* When broke: Cancel ugprades ASAP */
        return Prio::None;
    }
    if (howToGetMoney() == HowToGetMoney::CancelPlaneUpgrades) {
        return Prio::Top;
    }

    if (!hoursPassed(ACTION_UPGRADE_PLANES, 24)) {
        return Prio::None;
    }
    if (!mDayStarted) {
        return Prio::None; /* plane list not updated */
    }
    if (!isOfficeUsable()) {
        return Prio::None; /* office is destroyed */
    }

    if (mRunToFinalObjective == FinalPhase::SaveMoney) {
        return Prio::None;
    }

    if (mRunToFinalObjective == FinalPhase::No) {
        if (!haveDiscount()) {
            return Prio::None; /* wait until we have some discount */
        }
        if (RoutesNextStep::UpgradePlanes != mRoutesNextStep) {
            return Prio::None;
        }
    } else if (mRunToFinalObjective == FinalPhase::TargetRun) {
        if (!qPlayer.RobotUse(ROBOT_USE_LUXERY)) { /* unless mission is plane upgrades */
            return Prio::None;
        }
    } else {
        assert(false);
    }

    /* Plane upgrades happen asynchronously. Therefore, we earmark money in the variable mMoneyReservedForUpgrades.
     * We need to execute this action regularly even if we have no money since we
     * might cancel previously planned upgrades to have more available money. */
    SLONG minCost = 550 * (SeatCosts[2] - SeatCosts[0] / 2); /* assuming 550 seats (777) */
    if (getMoneyAvailable() < minCost && mMoneyReservedForUpgrades == 0) {
        return Prio::None;
    }
    if ((mRunToFinalObjective == FinalPhase::TargetRun) && qPlayer.RobotUse(ROBOT_USE_LUXERY)) {
        return Prio::High;
    }
    if (!mPlanesForRoutes.empty()) {
        return Prio::Medium;
    }
    return Prio::None;
}

Bot::Prio Bot::condBuyNewPlane(__int64 &moneyAvailable) {
    moneyAvailable = getMoneyAvailable();
    if (!hoursPassed(ACTION_BUYNEWPLANE, 2)) {
        return Prio::None;
    }
    if (!qPlayer.RobotUse(ROBOT_USE_MAKLER) || qPlayer.RobotUse(ROBOT_USE_DESIGNER_BUY)) {
        return Prio::None;
    }
    if (!mLongTermStrategy) {
        return Prio::None; /* we will only buy used planes */
    }
    if (mRunToFinalObjective > FinalPhase::No) {
        return Prio::None;
    }
    if (HowToPlan::None == canWePlanFlights()) {
        return Prio::None;
    }
    if (!haveDiscount()) {
        return Prio::None; /* wait until we have some discount */
    }

    SLONG bestPlaneTypeId = mDoRoutes ? mBuyPlaneForRouteId : mBestPlaneTypeId;
    if (bestPlaneTypeId < 0) {
        return Prio::None; /* no plane purchase planned */
    }

    if (mDoRoutes && RoutesNextStep::BuyMorePlanes != mRoutesNextStep) {
        return Prio::None;
    }

    for (auto planeId : mPlanesForRoutesUnassigned) {
        const auto &qPlane = qPlayer.Planes[planeId];
        if (qPlane.TypeId == bestPlaneTypeId) {
            return Prio::None; /* we already have an unused plane of desired type */
        }
    }
    if ((mExtraPilots < PlaneTypes[bestPlaneTypeId].AnzPiloten) || (mExtraBegleiter < PlaneTypes[bestPlaneTypeId].AnzBegleiter)) {
        return Prio::None; /* not enough crew */
    }
    if (moneyAvailable >= PlaneTypes[bestPlaneTypeId].Preis) {
        return Prio::High; /* buy the plane (e.g. for a new route) before spending it on something else */
    }

    return Prio::None;
}

Bot::Prio Bot::condBuyUsedPlane(__int64 &moneyAvailable) {
    moneyAvailable = getMoneyAvailable();
    if (!hoursPassed(ACTION_BUYUSEDPLANE, 2)) {
        return Prio::None;
    }
    if (mLongTermStrategy) {
        return Prio::None; /* we will only buy new planes */
    }
    if (mRunToFinalObjective > FinalPhase::No) {
        return Prio::None;
    }
    if (HowToPlan::None == canWePlanFlights()) {
        return Prio::None;
    }
    if (!haveDiscount()) {
        return Prio::None; /* wait until we have some discount */
    }
    if (mBestUsedPlaneIdx < 0) {
        return Prio::None; /* no plane selected (ACTION_VISITMUSEUM) */
    }
    if ((mExtraPilots < Sim.UsedPlanes[mBestUsedPlaneIdx].ptAnzPiloten) || (mExtraBegleiter < Sim.UsedPlanes[mBestUsedPlaneIdx].ptAnzBegleiter)) {
        return Prio::None; /* not enough crew */
    }
    if (moneyAvailable >= Sim.UsedPlanes[mBestUsedPlaneIdx].CalculatePrice()) {
        return Prio::High; /* buy the plane (e.g. for a new route) before spending it on something else */
    }

    return Prio::None;
}

Bot::Prio Bot::condVisitMuseum() {
    if (!hoursPassed(ACTION_VISITMUSEUM, 2)) {
        return Prio::None;
    }
    if (mLongTermStrategy) {
        return Prio::None; /* we will only buy new planes */
    }
    if (mRunToFinalObjective > FinalPhase::No) {
        return Prio::None;
    }
    if (HowToPlan::None == canWePlanFlights()) {
        return Prio::None;
    }
    if (!haveDiscount()) {
        return Prio::None; /* wait until we have some discount */
    }
    if (mBestUsedPlaneIdx < 0) {
        return Prio::High;
    }
    return Prio::None;
}

Bot::Prio Bot::condVisitHR() {
    if (!hoursPassed(ACTION_PERSONAL, 4)) {
        return Prio::None;
    }
    if (hoursPassed(ACTION_PERSONAL, 24)) {
        return Prio::Medium; /* hire new crew every day */
    }
    if (mItemPills >= 1 && qPlayer.HasItem(ITEM_TABLETTEN) == 0) {
        return Prio::Low; /* we need new pills */
    }
    return Prio::None;
}

Bot::Prio Bot::condBuyKerosine(__int64 &moneyAvailable) {
    moneyAvailable = getMoneyAvailable();
    if (!hoursPassed(ACTION_BUY_KEROSIN, 4)) {
        return Prio::None;
    }
    if (!qPlayer.RobotUse(ROBOT_USE_PETROLAIR)) {
        return Prio::None;
    }
    if (qPlayer.HasBerater(BERATERTYP_KEROSIN) < 30 || !checkLaptop()) {
        return Prio::None; /* no access to advisor report */
    }
    if (!haveDiscount()) {
        return Prio::None; /* wait until we have some discount */
    }
    if ((qPlayer.TankInhalt * 100) / qPlayer.Tank > 90) {
        return Prio::None;
    }
    if (moneyAvailable >= 0) {
        return Prio::Medium;
    }
    return Prio::None;
}

Bot::Prio Bot::condBuyKerosineTank(__int64 &moneyAvailable) {
    moneyAvailable = getMoneyAvailable() - kMoneyReserveBuyTanks;
    if (!hoursPassed(ACTION_BUY_KEROSIN_TANKS, 24)) {
        return Prio::None;
    }
    if (!qPlayer.RobotUse(ROBOT_USE_PETROLAIR) || !qPlayer.RobotUse(ROBOT_USE_TANKS)) {
        return Prio::None;
    }
    if (qPlayer.HasBerater(BERATERTYP_KEROSIN) < 30 || !checkLaptop()) {
        return Prio::None; /* no access to advisor report */
    }
    if (!haveDiscount()) {
        return Prio::None; /* wait until we have some discount */
    }
    if (!mDoRoutes || mRoutes.size() < kNumRoutesStartBuyingTanks) {
        return Prio::None; /* wait until we started using routes */
    }
    if (mRunToFinalObjective > FinalPhase::No) {
        return Prio::None;
    }
    if (mTankRatioEmptiedYesterday < 0.5) {
        return Prio::None;
    }
    auto nTankTypes = sizeof(TankSize) / sizeof(TankSize[0]);
    if (moneyAvailable > TankPrice[nTankTypes - 1]) {
        moneyAvailable = TankPrice[nTankTypes - 1]; /* do not spend more than 1x largest tank at once*/
    }
    if (moneyAvailable >= TankPrice[1]) {
        return Prio::Medium;
    }
    return Prio::None;
}

Bot::Prio Bot::condSabotage(__int64 &moneyAvailable) {
    moneyAvailable = getMoneyAvailable();
    if (!hoursPassed(ACTION_SABOTAGE, 24)) {
        return Prio::None;
    }
    if (qPlayer.ArabTrust == 0) {
        return Prio::None;
    }

    /* check if we want to prevent competitors from shutting down security office */
    if (qPlayer.RobotUse(ROBOT_USE_SECURTY_OFFICE)) {
        return mUsingSecurity ? Prio::High : Prio::Low;
    }

    /* check if we want to do sabotage */
    moneyAvailable -= kMoneyReserveSabotage;
    if (qPlayer.RobotUse(ROBOT_USE_EXTREME_SABOTAGE)) {
        if (!mNeedToShutdownSecurity && (mNemesis != -1)) {
            bool wantToSabotage = true;

            /* spiking coffee */
            auto minCost = SabotagePrice2[0];
            SLONG hints = 2;

            /* damage stock price */
            if (Sim.Difficulty == DIFF_ADDON08 || Sim.Difficulty == DIFF_ATFS07) {
                minCost = SabotagePrice[0];
                hints = 4;

                if (mNemesisScore * 100 / TARGET_SHARES < 50) {
                    wantToSabotage = false;
                }
            }

            if (mArabHintsTracker + hints > kMaxSabotageHints) {
                wantToSabotage = false;
            }

            if (wantToSabotage && (minCost <= moneyAvailable)) {
                return Prio::Medium;
            }
        }
        /* we might need them to shut down security office */
        if (qPlayer.HasItem(ITEM_ZANGE) == 0) {
            return mNeedToShutdownSecurity ? Prio::Medium : Prio::Low;
        }
    }

    if (mItemAntiVirus == 1 || mItemAntiVirus == 2) {
        return Prio::Low; /* collect darts */
    }
    return Prio::None;
}

Bot::Prio Bot::condIncreaseDividend(__int64 &moneyAvailable) {
    moneyAvailable = getMoneyAvailable();
    if (!hoursPassed(ACTION_SET_DIVIDEND, 24)) {
        return Prio::None;
    }
    if (mRunToFinalObjective > FinalPhase::No) {
        return Prio::None;
    }

    SLONG maxToEmit = (2500000 - qPlayer.MaxAktien) / 100 * 100;
    if (kReduceDividend && maxToEmit < 10000) {
        /* we cannot emit any shares anymore. We do not care about stock prices now. */
        return (qPlayer.Dividende > 0) ? Prio::Medium : Prio::None;
    }
    if (qPlayer.Dividende >= 25) {
        return Prio::None;
    }
    moneyAvailable -= kMoneyReserveIncreaseDividend;
    if (moneyAvailable >= 0) {
        return Prio::Medium;
    }
    return Prio::None;
}

Bot::Prio Bot::condTakeOutLoan() {
    if (!hoursPassed(ACTION_RAISEMONEY, 1)) {
        return Prio::None;
    }
    if (howToGetMoney() == HowToGetMoney::IncreaseCredit) {
        return Prio::Top;
    }
    return Prio::None;
}

Bot::Prio Bot::condDropMoney(__int64 &moneyAvailable) {
    moneyAvailable = getMoneyAvailable();
    if (!hoursPassed(ACTION_DROPMONEY, 24)) {
        return Prio::None;
    }
    if (mRunToFinalObjective > FinalPhase::No) {
        return Prio::None;
    }

    moneyAvailable -= kMoneyReservePaybackCredit;
    if (moneyAvailable >= 1000 && qPlayer.Credit > 0 && getWeeklyOpSaldo() > 1000 * 1000) {
        return Prio::Medium;
    }
    return Prio::None;
}

Bot::Prio Bot::condEmitShares() {
    if (!hoursPassed(ACTION_EMITSHARES, 1)) {
        return Prio::None;
    }
    if (howToGetMoney() == HowToGetMoney::EmitShares) {
        return Prio::Top;
    }

    if (!hoursPassed(ACTION_EMITSHARES, 24)) {
        return Prio::None;
    }
    if (GameMechanic::canEmitStock(qPlayer) == GameMechanic::EmitStockResult::Ok) {
        return Prio::Medium;
    }
    return Prio::None;
}

Bot::Prio Bot::condBuyShares(__int64 &moneyAvailable) {
    if (!hoursPassed(ACTION_BUYSHARES, 24)) {
        return Prio::None;
    }
    return std::max(condBuyNemesisShares(moneyAvailable), condBuyOwnShares(moneyAvailable));
}

Bot::Prio Bot::condBuyNemesisShares(__int64 &moneyAvailable) {
    moneyAvailable = getMoneyAvailable() - kMoneyReserveBuyNemesisShares;
    if (mRunToFinalObjective > FinalPhase::No) {
        return Prio::None;
    }
    if (qPlayer.RobotUse(ROBOT_USE_DONTBUYANYSHARES)) {
        return Prio::None;
    }
    if ((moneyAvailable < 0) || (qPlayer.Credit != 0)) {
        return Prio::None;
    }
    if (qPlayer.HasBerater(BERATERTYP_INFO) < 50) {
        return Prio::None; /* we don't know the number of enemy stock */
    }
    for (SLONG dislike = 0; dislike < 4; dislike++) {
        auto &qTarget = Sim.Players.Players[dislike];
        if (dislike == qPlayer.PlayerNum || qTarget.IsOut != 0) {
            continue;
        }
        if (qPlayer.OwnsAktien[dislike] < (qTarget.AnzAktien / 2)) {
            return Prio::Low; /* we own less than 50% of enemy stock */
        }
    }
    return Prio::None;
}

Bot::Prio Bot::condBuyOwnShares(__int64 &moneyAvailable) {
    moneyAvailable = getMoneyAvailable() - kMoneyReserveBuyOwnShares;
    if (mRunToFinalObjective > FinalPhase::No) {
        return Prio::None;
    }
    if (qPlayer.RobotUse(ROBOT_USE_DONTBUYANYSHARES)) {
        return Prio::None;
    }
    if (qPlayer.OwnsAktien[qPlayer.PlayerNum] >= (qPlayer.AnzAktien * kOwnStockPosessionRatio / 100)) {
        return Prio::None;
    }
    if ((moneyAvailable >= 0) && (qPlayer.Credit == 0)) {
        return Prio::Low;
    }
    return Prio::None;
}

Bot::Prio Bot::condOvertakeAirline() {
    if (!hoursPassed(ACTION_OVERTAKE_AIRLINE, 24)) {
        return Prio::None;
    }
    if ((qPlayer.HasBerater(BERATERTYP_INFO) < 50) || (qPlayer.HasBerater(BERATERTYP_GELD) < 50)) {
        return Prio::None; /* we don't know the number of enemy stock */
    }
    for (SLONG p = 0; p < 4; p++) {
        auto &qTarget = Sim.Players.Players[p];
        if (p == qPlayer.PlayerNum || qTarget.IsOut != 0) {
            continue;
        }
        if (GameMechanic::OvertakeAirlineResult::Ok != GameMechanic::canOvertakeAirline(qPlayer, p)) {
            continue;
        }
        return Prio::High;
    }
    return Prio::None;
}

Bot::Prio Bot::condSellShares(__int64 &moneyAvailable) {
    moneyAvailable = getMoneyAvailable();
    if (!hoursPassed(ACTION_SELLSHARES, 1)) {
        return Prio::None;
    }
    auto res = howToGetMoney();
    if (res == HowToGetMoney::SellShares || res == HowToGetMoney::SellOwnShares || res == HowToGetMoney::SellAllOwnShares) {
        return Prio::Top;
    }

    if (!hoursPassed(ACTION_SELLSHARES, 24)) {
        return Prio::None;
    }
    if (qPlayer.RobotUse(ROBOT_USE_MAX20PERCENT)) {
        SLONG c = qPlayer.PlayerNum;
        SLONG sells = (qPlayer.OwnsAktien[c] - qPlayer.AnzAktien * kOwnStockPosessionRatio / 100);
        if (sells > 0) {
            return Prio::Medium;
        }
    }
    return Prio::None;
}

Bot::Prio Bot::condVisitMech() {
    /* Plane repairs happen asynchronously. Therefore, we earmark money in the variable mMoneyReservedForRepairs.
     * We need to execute this action regularly even if we have no money since we
     * might cancel previously planned repairs to have more available money. */
    if (!hoursPassed(ACTION_VISITMECH, 1)) { /* When broke: Lower repair targets ASAP */
        return Prio::None;
    }
    if (howToGetMoney() == HowToGetMoney::LowerRepairTargets) {
        return Prio::Top;
    }
    if (!hoursPassed(ACTION_VISITMECH, 4)) { /* Not broke: Do not need to visit too often */
        return Prio::None;
    }
    if (getMoneyAvailable() < 0 && mMoneyReservedForRepairs == 0) {
        return Prio::None;
    }
    return Prio::Medium;
}

Bot::Prio Bot::condVisitNasa(__int64 &moneyAvailable) {
    moneyAvailable = getMoneyAvailable();
    if (!hoursPassed(ACTION_VISITNASA, 4)) {
        return Prio::None;
    }
    if (!qPlayer.RobotUse(ROBOT_USE_NASA)) {
        return Prio::None;
    }
    if (mRunToFinalObjective != FinalPhase::TargetRun) {
        return Prio::None;
    }

    const auto &qPrices = (Sim.Difficulty == DIFF_FINAL) ? RocketPrices : StationPrices;
    auto nRocketParts = sizeof(qPrices) / sizeof(qPrices[0]);
    for (SLONG i = 0; i < nRocketParts; i++) {
        if ((qPlayer.RocketFlags & (1 << i)) == 0 && moneyAvailable >= qPrices[i]) {
            return Prio::High;
        }
    }

    return Prio::None;
}

Bot::Prio Bot::condVisitMisc() {
    /* misc action, can do as often as the bot likes */
    return Prio::Lowest;
}

Bot::Prio Bot::condVisitMakler() {
    if (!hoursPassed(ACTION_VISITMAKLER, 4)) {
        return Prio::None;
    }
    if (mLongTermStrategy) {
        if (hoursPassed(ACTION_VISITMAKLER, 24) || (mBestPlaneTypeId == -1)) {
            return Prio::Low; /* check available plane types at least once per day */
        }
    }
    if (mItemAntiStrike == 0) {
        return Prio::Low; /* take BH */
    }
    return condVisitMisc();
}

Bot::Prio Bot::condVisitArab() {
    /* misc action, can do as often as the bot likes */
    if (!qPlayer.RobotUse(ROBOT_USE_PETROLAIR)) {
        return Prio::None;
    }
    if (qPlayer.HasItem(ITEM_MG)) {
        return Prio::Low;
    }
    return condVisitMisc();
}

Bot::Prio Bot::condVisitRick() {
    /* misc action, can do as often as the bot likes */
    if (qPlayer.StrikeHours > 0 && qPlayer.StrikeEndType == 0 && mItemAntiStrike >= 3) {
        return Prio::Top;
    }
    if (mItemAntiStrike == 3) {
        return Prio::Low; /* give horse shoe */
    }
    return condVisitMisc();
}

Bot::Prio Bot::condVisitDutyFree(__int64 &moneyAvailable) {
    moneyAvailable = getMoneyAvailable();
    moneyAvailable -= 100 * 1000;

    if (hoursPassed(ACTION_VISITDUTYFREE, 24) && moneyAvailable >= 0) {
        /* emergency shopping: Laptop because office is destroyed */
        if (!mDayStarted && !isOfficeUsable() && qPlayer.LaptopQuality == 0) {
            return Prio::Higher;
        }

        if (qPlayer.LaptopQuality < 4) {
            return Prio::Low;
        }
    }

    /* misc action, can do as often as the bot likes */
    if (moneyAvailable >= 0 && !qPlayer.HasItem(ITEM_HANDY)) {
        return Prio::Low;
    }
    if (mItemAntiStrike >= 1 && mItemAntiStrike <= 2) {
        return Prio::Low; /* we still need to aquire the horse shoe */
    }
    if (mItemArabTrust == 0 && Sim.Date > 0 && qPlayer.ArabTrust == 0) {
        return Prio::Low; /* we still need to aquire the MG */
    }

    return condVisitMisc();
}

Bot::Prio Bot::condVisitBoss(__int64 &moneyAvailable) {
    moneyAvailable = getMoneyAvailable();
    if (!hoursPassed(ACTION_VISITAUFSICHT, 2)) {
        return Prio::None;
    }

    moneyAvailable -= kMoneyReserveBossOffice;
    if ((mRunToFinalObjective == FinalPhase::No) && (moneyAvailable >= 0)) {
        if (Sim.Time > (16 * 60000) && (mBossNumCitiesAvailable > 0 || mBossGateAvailable)) {
            return Prio::High; /* check again right before end of day */
        }
        if (mBossGateAvailable || (mBossNumCitiesAvailable == -1)) { /* there is gate available (or we don't know yet) */
            return mOutOfGates ? Prio::High : Prio::Medium;
        }
        if (mBossNumCitiesAvailable > 0 && hoursPassed(ACTION_VISITAUFSICHT, 4)) {
            return Prio::Low;
        }
    }

    if (mItemPills == 0) {
        return Prio::Low; /* we still need to take the card */
    }
    return Prio::None;
}

Bot::Prio Bot::condExpandAirport(__int64 &moneyAvailable) {
    moneyAvailable = getMoneyAvailable();
    if (!hoursPassed(ACTION_EXPANDAIRPORT, 24)) {
        return Prio::None;
    }
    if (GameMechanic::ExpandAirportResult::Ok != GameMechanic::canExpandAirport(qPlayer)) {
        return Prio::None;
    }
    if (!checkLaptop()) {
        return Prio::None; /* no access to gate utilization */
    }
    if (mRunToFinalObjective > FinalPhase::No) {
        return Prio::None;
    }

    DOUBLE gateUtilization = 0;
    for (auto util : qPlayer.Gates.Auslastung) {
        gateUtilization += util;
    }
    if (gateUtilization / (24 * 7) < (qPlayer.Gates.NumRented - 1)) {
        return Prio::None;
    }
    moneyAvailable -= kMoneyReserveExpandAirport;
    if (moneyAvailable >= 0) {
        return Prio::Medium;
    }

    return Prio::None;
}

Bot::Prio Bot::condVisitRouteBoxPlanning() {
    /* no hoursPassed(): Action frequency is controlled by mRoutesNextStep */
    if (!qPlayer.RobotUse(ROBOT_USE_ROUTEBOX) || !mDoRoutes) {
        return Prio::None;
    }
    if (mRunToFinalObjective > FinalPhase::No) {
        return Prio::None;
    }
    if ((mWantToRentRouteId == -1) && RoutesNextStep::RentNewRoute == mRoutesNextStep) {
        return Prio::Medium; /* execute route strategy */
    }
    if (!mRoutesUtilizationUpdated) {
        return Prio::Medium; /* update cached route info */
    }
    if (mRoutesUpdated && mRoutesNextStep == RoutesNextStep::None) {
        return Prio::Medium; /* generate route strategy if other info is already updated */
    }
    return Prio::None;
}

Bot::Prio Bot::condVisitRouteBoxRenting(__int64 &moneyAvailable) {
    moneyAvailable = getMoneyAvailable();
    if (!hoursPassed(ACTION_VISITROUTEBOX2, 4)) {
        return Prio::None;
    }
    if (!mDoRoutes) {
        return (getNumRentedRoutes() > 0) ? Prio::Low : Prio::None;
    }
    if (!qPlayer.RobotUse(ROBOT_USE_ROUTEBOX)) {
        return Prio::None;
    }
    if (mRunToFinalObjective > FinalPhase::No) {
        return Prio::None;
    }
    if (HowToPlan::None == canWePlanFlights()) {
        return Prio::None;
    }
    if (!Helper::checkRoomOpen(ACTION_WERBUNG_ROUTES)) {
        return Prio::None; /* let's wait until we are able to buy ads for the route */
    }
    if ((mWantToRentRouteId != -1) && RoutesNextStep::RentNewRoute == mRoutesNextStep) {
        return Prio::High;
    }
    return Prio::None;
}

Bot::Prio Bot::condVisitSecurity(__int64 &moneyAvailable) {
    moneyAvailable = getMoneyAvailable();
    if (!hoursPassed(ACTION_VISITSECURITY, 24)) {
        return Prio::None;
    }
    if (!qPlayer.RobotUse(ROBOT_USE_SECURTY_OFFICE)) {
        return Prio::None;
    }
    if (isLateGame()) {
        return Prio::Medium; /* enable security measures */
    } else if (mUsingSecurity) {
        return Prio::High; /* deactivate security measures */
    }
    return Prio::None;
}

Bot::Prio Bot::condSabotageSecurity() {
    if (!hoursPassed(ACTION_VISITSECURITY2, 24)) {
        return Prio::None;
    }
    if (!mNeedToShutdownSecurity) {
        return Prio::None;
    }
    if (qPlayer.HasItem(ITEM_ZANGE) == 0) {
        return Prio::None;
    }
    return Prio::Medium;
}

Bot::Prio Bot::condVisitDesigner(__int64 &moneyAvailable) {
    moneyAvailable = getMoneyAvailable();
    if (!hoursPassed(ACTION_VISITDESIGNER, 2)) {
        return Prio::None;
    }
    if (!qPlayer.RobotUse(ROBOT_USE_DESIGNER) || !qPlayer.RobotUse(ROBOT_USE_DESIGNER_BUY)) {
        return Prio::None;
    }
    if (HowToPlan::None == canWePlanFlights()) {
        return Prio::None;
    }
    if (!haveDiscount()) {
        return Prio::None; /* wait until we have some discount */
    }

    if (mDesignerPlane.Name.empty() || !mDesignerPlane.IsBuildable()) {
        return Prio::None; /* no designer plane available */
    }
    if ((mExtraPilots < mDesignerPlane.CalcPiloten()) || (mExtraBegleiter < mDesignerPlane.CalcBegleiter())) {
        return Prio::None; /* not enough crew */
    }
    if (moneyAvailable >= mDesignerPlane.CalcCost()) {
        return Prio::High; /* buy the plane (e.g. for a new route) before spending it on something else */
    }

    return Prio::None;
}

Bot::Prio Bot::condBuyAdsForRoutes(__int64 &moneyAvailable) {
    moneyAvailable = getMoneyAvailable();
    /* no hoursPassed(): Action frequency is controlled by mRoutesNextStep */

    if (!qPlayer.RobotUse(ROBOT_USE_WERBUNG)) {
        return Prio::None;
    }
    if (!haveDiscount()) {
        return Prio::None; /* wait until we have some discount */
    }

    if (mRunToFinalObjective > FinalPhase::No) {
        return Prio::None;
    }

    if (mRoutesNextStep == RoutesNextStep::BuyAdsForRoute) {
        return (mRoutes[mImproveRouteId].image < 80) ? Prio::High : Prio::Medium;
    }
    return Prio::None;
}

Bot::Prio Bot::condBuyAds(__int64 &moneyAvailable) {
    moneyAvailable = getMoneyAvailable();
    if (!hoursPassed(ACTION_WERBUNG, 4)) {
        return Prio::None;
    }

    if (!qPlayer.RobotUse(ROBOT_USE_WERBUNG)) {
        return Prio::None;
    }

    bool nearEnd = (mRunToFinalObjective > FinalPhase::No);
    if (qPlayer.RobotUse(ROBOT_USE_MUCHWERBUNG) && nearEnd) { /* mission where we need to buy ads */
        if (mRunToFinalObjective == FinalPhase::SaveMoney) {
            return Prio::None;
        }
    } else {
        if (nearEnd) {
            return Prio::None;
        }
        if (!haveDiscount()) {
            return Prio::None; /* wait until we have some discount */
        }
        if (mRoutesNextStep != RoutesNextStep::ImproveAirlineImage) {
            return Prio::None;
        }
    }

    auto minCost = gWerbePrice[0 * 6 + kSmallestAdCampaign];
    if (moneyAvailable >= minCost) {
        if ((mRunToFinalObjective == FinalPhase::TargetRun) && qPlayer.RobotUse(ROBOT_USE_MUCHWERBUNG)) {
            return Prio::High;
        }
        if (getImage() < kMinimumImage || (mDoRoutes && getImage() < 300)) {
            return Prio::Medium;
        }
        auto imageDelta = minCost / 10000 * (kSmallestAdCampaign + 6) / 55;
        if (mDoRoutes && getImage() < (1000 - imageDelta)) {
            return Prio::Low;
        }
    }
    return Prio::None;
}

Bot::Prio Bot::condVisitAds() {
    /* check company image unless available via advisor */
    if (hoursPassed(ACTION_VISITADS, 24) && qPlayer.HasBerater(BERATERTYP_GELD) < 50) {
        return Prio::Medium;
    }

    /* always collect items if necessary */
    if (mItemAntiVirus == 3) {
        return Prio::Low;
    }
    if (mItemAntiVirus == 4 && qPlayer.HasItem(ITEM_DISKETTE) == 0) {
        return Prio::Low;
    }

    return Prio::None;
}