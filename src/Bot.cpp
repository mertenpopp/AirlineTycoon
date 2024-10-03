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
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

template <class... Types> void AT_Error(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_ERROR, "Bot", args...); }
template <class... Types> void AT_Warn(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_WARN, "Bot", args...); }
template <class... Types> void AT_Info(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_INFO, "Bot", args...); }
template <class... Types> void AT_Log(Types... args) { AT_Log_I("Bot", args...); }

const bool kAlwaysReplan = true;
float kSchedulingMinScoreRatio = 140 * 1000.0F;
float kSchedulingMinScoreRatioLastMinute = 10 * 1000.0F;
SLONG kSwitchToRoutesNumPlanesMin = 2;
SLONG kSwitchToRoutesNumPlanesMax = 4;
const SLONG kSmallestAdCampaign = 4;
const SLONG kMinimumImage = -4;
const SLONG kRouteAvgDays = 3;
const SLONG kMinimumOwnRouteUtilization = 0;
SLONG kMaximumRouteUtilization = 90;
const SLONG kMaximumPlaneUtilization = 70;
DOUBLE kMaxTicketPriceFactor = 3.0;
const DOUBLE kDefaultTicketPriceFactor = 3.5;
const SLONG kTargetEmployeeHappiness = 90;
const SLONG kMinimumEmployeeSkill = 70;
SLONG kPlaneMinimumZustand = 90;
const SLONG kPlaneTargetZustand = 100;
const SLONG kUsedPlaneMinimumScore = 40;
DOUBLE kMaxKerosinQualiZiel = 1.2;
SLONG kNumRoutesStartBuyingTanks = 3;
SLONG kOwnStockPosessionRatio = 51;
const SLONG kStockEmissionMode = 2;
const bool kReduceDividend = false;
const SLONG kMaxSabotageHints = 99;

const SLONG kMoneyEmergencyFund = 100000;
const SLONG kMoneyReserveRepairs = 0;
const SLONG kMoneyReservePlaneUpgrades = 2500 * 1000;
const SLONG kMoneyReserveBuyTanks = 200 * 1000;
const SLONG kMoneyReserveIncreaseDividend = 100 * 1000;
const SLONG kMoneyReservePaybackCredit = 1500 * 1000;
const SLONG kMoneyReserveBuyOwnShares = 2000 * 1000;
const SLONG kMoneyReserveBuyNemesisShares = 80 * 1e6;
const SLONG kMoneyReserveBossOffice = 0;
const SLONG kMoneyReserveExpandAirport = 1000 * 1000;
const SLONG kMoneyReserveSabotage = 200 * 1000;

SLONG kPlaneScoreForceBest = -1;

const char *Bot::getPrioName(Bot::Prio prio) {
    switch (prio) {
    case Bot::Prio::Top:
        return "Top";
    case Bot::Prio::Higher:
        return "Higher";
    case Bot::Prio::High:
        return "High";
    case Bot::Prio::Medium:
        return "Medium";
    case Bot::Prio::Low:
        return "Low";
    case Bot::Prio::Lowest:
        return "Lowest";
    case Bot::Prio::None:
        return "None";
    default:
        AT_Error("Bot.cpp: Default case should not be reached.");
        DebugBreak();
        return "INVALID";
    }
    return "INVALID";
}
const char *Bot::getPrioName(SLONG prio) { return getPrioName(static_cast<Bot::Prio>(prio)); }

Bot::Bot(PLAYER &player) : qPlayer(player) {}

void Bot::printStatisticsLine(const CString &prefix, bool printHeader) {
    if (printHeader) {
        printf("%s: Tag, Geld, Kredit, Available, ", prefix.c_str());
        printf("SaldoGesamt, Saldo, Gewinn, Verlust, ");
        printf("Routen, Auftraege, Fracht, ");
        printf("KerosinFlug, KerosinVorrat, Essen, ");
        printf("Strafe, Wartung, Umbau, ");
        printf("Personal, Gatemiete, Citymiete, Routenmiete, ");
        printf("HabenZinsen, HabenRendite, KreditNeu, ");
        printf("SollZinsen, SollRendite, KreditTilgung, Steuer, ");
        printf("Aktienverkauf, AktienEmission, AktienEmissionKompErh, ");
        printf("AktienEmissionKompGez, Aktienkauf, AktienEmissionFee, ");
        printf("FlugzeugVerkauf, Takeovers, FlugzeugKauf, FlugzeugUpgrades, ");
        printf("ExpansionCity, ExpansionRouten, ExpansionGates, ExpansionTanks, ");
        printf("SabotageGeklaut, SabotageKomp, ");
        printf("Sabotage, SabotageStrafe, SabotageSchaden, ");
        printf("BodyguardRabatt, GeldErhalten, SonstigeEinnahmen, ");
        printf("PanneSchaden, SecurityKosten, WerbeKosten, ");
        printf("GeldGeschickt, SonstigeAusgaben, KerosinGespart, ");
        printf("Image, ");

        printf("Flugzeuge, Passagiere, PassagiereHome, Aktienkurs, ");
        printf("Flüge, Aufträge, LastMinute, ");
        printf("Firmenwert, PassZufrieden, PassUnzufrieden, PersonalZufrieden, ");
        printf("Verspätung, Unfälle, Sabotiert, ");
        printf("Mitarbeiter, Niederlassungen, AnzRouten, Gehalt, ");
        printf("AktienGesamt, AktienSA, AktienFL, AktienPT, AktienHA, ");
        printf("Frachtaufträge, Frachttonnen, ");
        printf("ZielSA, ZielFL, ZielPT, ZielHA, ");
        printf("Planetype\n");
    }

    std::vector<__int64> values;
    auto balanceAvg = qPlayer.BilanzWoche.Hole();
    auto balance = qPlayer.BilanzGesamt;
    values.insert(values.end(), {Sim.Date, qPlayer.Money, qPlayer.Credit, getMoneyAvailable()});
    values.insert(values.end(), {balance.GetOpSaldo(), balanceAvg.GetOpSaldo(), balanceAvg.GetOpGewinn(), balanceAvg.GetOpVerlust()});
    values.insert(values.end(), {balance.Tickets, balance.Auftraege, balance.FrachtAuftraege});
    values.insert(values.end(), {balance.KerosinFlug, balance.KerosinVorrat, balance.Essen});
    values.insert(values.end(), {balance.Vertragsstrafen, balance.Wartung, balance.FlugzeugUmbau});
    values.insert(values.end(), {balance.Personal, balance.Gatemiete, balance.Citymiete, balance.Routenmiete});
    values.insert(values.end(), {balance.HabenZinsen, balance.HabenRendite, balance.KreditNeu});
    values.insert(values.end(), {balance.SollZinsen, balance.SollRendite, balance.KreditTilgung, balance.Steuer});
    values.insert(values.end(), {balance.Aktienverkauf, balance.AktienEmission, balance.AktienEmissionKompErh});
    values.insert(values.end(), {balance.AktienEmissionKompGez, balance.Aktienkauf, balance.AktienEmissionFee});
    values.insert(values.end(), {balance.FlugzeugVerkauf, balance.Takeovers, balance.FlugzeugKauf, balance.FlugzeugUpgrades});
    values.insert(values.end(), {balance.ExpansionCity, balance.ExpansionRouten, balance.ExpansionGates, balance.ExpansionTanks});
    values.insert(values.end(), {balance.SabotageGeklaut, balance.SabotageKomp});
    values.insert(values.end(), {balance.Sabotage, balance.SabotageStrafe, balance.SabotageSchaden});
    values.insert(values.end(), {balance.BodyguardRabatt, balance.GeldErhalten, balance.SonstigeEinnahmen});
    values.insert(values.end(), {balance.PanneSchaden, balance.SecurityKosten, balance.WerbeKosten});
    values.insert(values.end(), {balance.GeldGeschickt, balance.SonstigeAusgaben, balance.KerosinGespart});
    values.insert(values.end(), {qPlayer.Image});

    std::vector<SLONG> valuesStat;
    valuesStat.insert(valuesStat.end(), {STAT_FLUGZEUGE, STAT_PASSAGIERE, STAT_PASSAGIERE_HOME, STAT_AKTIENKURS});
    valuesStat.insert(valuesStat.end(), {STAT_FLUEGE, STAT_AUFTRAEGE, STAT_LMAUFTRAEGE});
    valuesStat.insert(valuesStat.end(), {STAT_FIRMENWERT, STAT_ZUFR_PASSAGIERE, STAT_UNZUFR_PASSAGIERE, STAT_ZUFR_PERSONAL});
    valuesStat.insert(valuesStat.end(), {STAT_VERSPAETUNG, STAT_UNFAELLE, STAT_SABOTIERT});
    valuesStat.insert(valuesStat.end(), {STAT_MITARBEITER, STAT_NIEDERLASSUNGEN, STAT_ROUTEN, STAT_GEHALT});
    valuesStat.insert(valuesStat.end(), {STAT_AKTIEN_ANZAHL, STAT_AKTIEN_SA, STAT_AKTIEN_FL, STAT_AKTIEN_PT, STAT_AKTIEN_HA});
    valuesStat.insert(valuesStat.end(), {STAT_FRACHTEN, STAT_TONS});

    std::cout << prefix.c_str() << ": ";
    for (auto i : values) {
        std::cout << i << ", ";
    }
    for (auto i : valuesStat) {
        std::cout << qPlayer.Statistiken[i].GetAtPastDay(1) << ", ";
    }

    for (SLONG i = 0; i < 4; i++) {
        std::cout << Sim.Players.Players[i].Statistiken[STAT_MISSIONSZIEL].GetAtPastDay(1) << ", ";
    }

    SLONG count = 0;
    for (SLONG i = 0; i < qPlayer.Planes.AnzEntries(); i++) {
        if (qPlayer.Planes.IsInAlbum(i)) {
            if (kPlaneScoreForceBest + 0x10000000 == qPlayer.Planes[i].TypeId) {
                count++;
            }
        }
    }
    std::cout << count << std::endl;
}

void Bot::RobotInit() {
    if (gQuickTestRun > 0) {
        printStatisticsLine("BotStatistics", (Sim.Date == 0));
    } else {
        auto balance = qPlayer.BilanzWoche.Hole();
        AT_Info("Bot.cpp: Enter RobotInit(): Current day: %d, money: %s $ (op saldo %s = %s %s)", Sim.Date, Insert1000erDots64(qPlayer.Money).c_str(),
                Insert1000erDots64(balance.GetOpSaldo()).c_str(), Insert1000erDots64(balance.GetOpGewinn()).c_str(),
                Insert1000erDots64(balance.GetOpVerlust()).c_str());
    }

    /* print inventory */
    printf("Inventory: ");
    for (SLONG d = 0; d < 6; d++) {
        if (qPlayer.Items[d] != 0xff) {
            printf("%s, ", Helper::getItemName(qPlayer.Items[d]));
        }
    }
    printf("\n");

    if (mFirstRun) {
        AT_Log("Bot::RobotInit(): First run.");

        /* random source */
        LocalRandom.SRand(qPlayer.WaitWorkTill);

        /* starting planes */
        for (SLONG i = 0; i < qPlayer.Planes.AnzEntries(); i++) {
            if (qPlayer.Planes.IsInAlbum(i)) {
                mPlanesForJobsUnassigned.push_back(qPlayer.Planes.GetIdFromIndex(i));
            }
        }

        /* mission specialization */
        if (Sim.Difficulty == DIFF_TUTORIAL) {
            mItemPills = -1;      /* item not available */
            mItemAntiStrike = -1; /* item not available */
        }
        if (Sim.Difficulty <= DIFF_FIRST) {
            mItemAntiVirus = -1; /* item not available */
        }

        if (qPlayer.RobotUse(ROBOT_USE_ROUTEMISSION)) {
            kMaximumRouteUtilization = 20;
        }

        if (qPlayer.RobotUse(ROBOT_USE_FORCEROUTES)) {
            std::swap(mPlanesForJobsUnassigned, mPlanesForRoutesUnassigned);
        }

        if (qPlayer.RobotUse(ROBOT_USE_GROSSESKONTO)) {
            /* immediately start saving money */
            mRunToFinalObjective = FinalPhase::SaveMoney;
        }

        if (qPlayer.RobotUse(ROBOT_USE_DESIGNER_BUY)) {
            // Path will be "%AppPath%/myplanes/"
            CString path{AppPath + MyPlanePath.Left(MyPlanePath.GetLength() - 3)};
            fs::create_directory(path.c_str());

            if (Sim.Difficulty == DIFF_ATFS05) {
                kSwitchToRoutesNumPlanesMin = std::max(3, kSwitchToRoutesNumPlanesMin);
                kSwitchToRoutesNumPlanesMax = std::max(3, kSwitchToRoutesNumPlanesMax);
                setHardcodedDesignerPlaneLarge();
                mDesignerPlaneFile = FullFilename("botplane_atfs05_1.plane", MyPlanePath);
            } else if (Sim.Difficulty == DIFF_ATFS08) {
                kSwitchToRoutesNumPlanesMin = std::max(5, kSwitchToRoutesNumPlanesMin);
                kSwitchToRoutesNumPlanesMax = std::max(5, kSwitchToRoutesNumPlanesMax);
                setHardcodedDesignerPlaneEco();
                mDesignerPlaneFile = FullFilename("botplane_atfs08_1.plane", MyPlanePath);
            }
            if (!mDesignerPlaneFile.empty()) {
                if (DoesFileExist(mDesignerPlaneFile) == 1) {
                    mDesignerPlane.Load(mDesignerPlaneFile);
                } else {
                    mDesignerPlane.Save(mDesignerPlaneFile);
                }
            }
        }

        if (qPlayer.RobotUse(ROBOT_USE_MAX20PERCENT)) {
            kOwnStockPosessionRatio = 20;
        }

        /* bot level */
        AT_Log("Bot::RobotInit(): We are player %d with bot level = %s.", qPlayer.PlayerNum, StandardTexte.GetS(TOKEN_NEWGAME, 5001 + qPlayer.BotLevel));
        if (qPlayer.BotLevel <= 2) {
            kMaxTicketPriceFactor = std::min(2.0, kMaxTicketPriceFactor);
            kSchedulingMinScoreRatio = std::min(5.0F, kSchedulingMinScoreRatio);
            kSchedulingMinScoreRatioLastMinute = std::min(5.0F, kSchedulingMinScoreRatioLastMinute);
            kMaxKerosinQualiZiel = std::min(1.0, kMaxKerosinQualiZiel);
        }

        printRobotFlags();

        mFirstRun = false;
    }

    for (auto &i : qPlayer.RobotActions) {
        i = {};
    }

    /* action economy */
    mLastTimeInRoom.clear();
    mNumActionsToday = 0;

    /* strategy state */
    mBestUsedPlaneIdx = -1;
    mDayStarted = false;
    mNeedToShutdownSecurity = false;

    /* status boss office */
    mBossNumCitiesAvailable = -1;
    mBossGateAvailable = false;

    /* items */
    mIsSickToday = false;

    RobotPlan();
    AT_Log("Bot.cpp: Leaving RobotInit()");
}

void Bot::RobotPlan() {
    if (mFirstRun) {
        AT_Error("Bot::RobotPlan(): Bot was not initialized!");
        RobotInit();
        AT_Log("Bot.cpp: Leaving RobotPlan() (not initialized)\n");
        return;
    }

    if (mIsSickToday && qPlayer.HasItem(ITEM_TABLETTEN)) {
        GameMechanic::useItem(qPlayer, ITEM_TABLETTEN);
        AT_Log("Bot::RobotPlan(): Cured sickness using item pills");
        mIsSickToday = false;
    }

    auto &qRobotActions = qPlayer.RobotActions;

    std::array<SLONG, 42> actions = {
        ACTION_STARTDAY, ACTION_STARTDAY_LAPTOP,
        /* repeated actions */
        ACTION_BUERO, ACTION_CALL_INTERNATIONAL, ACTION_CALL_INTER_HANDY, ACTION_CHECKAGENT1, ACTION_CHECKAGENT2, ACTION_CHECKAGENT3, ACTION_UPGRADE_PLANES,
        ACTION_BUYNEWPLANE, ACTION_BUYUSEDPLANE, ACTION_VISITMUSEUM, ACTION_PERSONAL, ACTION_BUY_KEROSIN, ACTION_BUY_KEROSIN_TANKS, ACTION_SABOTAGE,
        ACTION_SET_DIVIDEND, ACTION_RAISEMONEY, ACTION_DROPMONEY, ACTION_EMITSHARES, ACTION_SELLSHARES, ACTION_BUYSHARES, ACTION_VISITMECH, ACTION_VISITNASA,
        ACTION_VISITTELESCOPE, ACTION_VISITMAKLER, ACTION_VISITARAB, ACTION_VISITRICK, ACTION_VISITKIOSK, ACTION_VISITDUTYFREE, ACTION_VISITAUFSICHT,
        ACTION_EXPANDAIRPORT, ACTION_VISITROUTEBOX, ACTION_VISITROUTEBOX2, ACTION_VISITSECURITY, ACTION_VISITSECURITY2, ACTION_VISITDESIGNER,
        ACTION_WERBUNG_ROUTES, ACTION_WERBUNG, ACTION_VISITADS, ACTION_OVERTAKE_AIRLINE, ACTION_VISITSABOTEUR};

    if (qRobotActions[0].ActionId != ACTION_NONE || qRobotActions[1].ActionId != ACTION_NONE) {
        AT_Log("Bot.cpp: Leaving RobotPlan() (actions already planned)\n");
        return;
    }

    auto &qFirstAction = qRobotActions[1];
    auto &qSecondAction = qRobotActions[2];
    qFirstAction.ActionId = ACTION_NONE;
    qSecondAction.ActionId = ACTION_NONE;

    /* populate prio list. Will be sorted by priority (1st order) and then by score (2nd order) */
    struct PrioListItem {
        SLONG actionId{-1};
        Prio prio{Prio::None};
        SLONG rnd{0};
        SLONG walkingDistance{0};
    };
    std::vector<PrioListItem> prioList;
    for (auto &action : actions) {
        if (!Helper::checkRoomOpen(action)) {
            continue;
        }
        auto prio = condAll(action);
        if (prio == Prio::None) {
            continue;
        }

        SLONG room = Helper::getRoomFromAction(qPlayer.PlayerNum, action);
        prioList.emplace_back(PrioListItem{action, prio, qPlayer.PlayerWalkRandom.Rand(0, 100)});

        if (prio >= Prio::Medium && room > 0 && (Sim.Time > 540000)) {
            /* factor in walking distance for more important actions */
            prioList.back().walkingDistance = Helper::getWalkDistance(qPlayer.PlayerNum, room);
        }
    }

    if (prioList.size() < 2) {
        prioList.emplace_back(PrioListItem{ACTION_CHECKAGENT2, Prio::Medium, 10000});
    }
    if (prioList.size() < 2) {
        prioList.emplace_back(PrioListItem{ACTION_CHECKAGENT1, Prio::Medium, 10000});
    }

    /* sort by priority */
    std::sort(prioList.begin(), prioList.end(), [](const PrioListItem &a, const PrioListItem &b) {
        if (a.prio == b.prio) {
            return ((a.rnd + a.walkingDistance) < (b.rnd + b.walkingDistance));
        }
        return (a.prio > b.prio);
    });

    /*for (const auto &qAction : prioList) {
        AT_Log("Bot::RobotPlan(): %s with prio %s (%d+%d)", Translate_ACTION(qAction.actionId), getPrioName(qAction.prio), qAction.rnd,
                qAction.walkingDistance);
    }*/

    auto threshNoRun = (qPlayer.BotLevel >= 3 ? Prio::Low : (qPlayer.BotLevel >= 2 ? Prio::Medium : Prio::Top));

    qFirstAction.ActionId = prioList[0].actionId;
    qFirstAction.Running = (prioList[0].prio > threshNoRun);
    qFirstAction.Prio = static_cast<SLONG>(prioList[0].prio);
    qSecondAction.ActionId = prioList[1].actionId;
    qSecondAction.Running = (prioList[1].prio > threshNoRun);
    qSecondAction.Prio = static_cast<SLONG>(prioList[1].prio);

    AT_Log("Bot::RobotPlan(): Current: %s, planned: %s, %s", Translate_ACTION(qRobotActions[0].ActionId), Translate_ACTION(qFirstAction.ActionId),
           Translate_ACTION(qSecondAction.ActionId));

    if (qFirstAction.ActionId == ACTION_NONE) {
        AT_Error("Did not plan action for slot #1");
    }
    if (qSecondAction.ActionId == ACTION_NONE) {
        AT_Error("Did not plan action for slot #2");
    }
}

void Bot::RobotExecuteAction() {
    if (mFirstRun) {
        AT_Error("Bot::RobotExecuteAction(): Bot was not initialized!");
        RobotInit();
        AT_Log("Bot.cpp: Leaving RobotExecuteAction() (not initialized)\n");
        return;
    }

    /* refuse to work outside working hours (game sometimes calls this too early) */
    if (Sim.Time <= 540000) { /* check if it is precisely 09:00 or earlier */
        AT_Log("Bot.cpp: Leaving RobotExecuteAction() (too early)\n");
        forceReplanning();
        return;
    }
    if (Sim.GetHour() >= 18) {
        AT_Log("Bot.cpp: Leaving RobotExecuteAction() (too late)\n");
        forceReplanning();
        return;
    }

    if (kAlwaysReplan) {
        forceReplanning();
    }

    auto &qAction = qPlayer.RobotActions[0];
    LocalRandom.Rand(2); // Sicherheitshalber, damit wir immer genau ein Random ausführen

    mNumActionsToday += 1;
    AT_Info("Bot::RobotExecuteAction(): Executing %s (#%d, %s), current time: %02ld:%02ld, money: %s $ (available: %s $)", Translate_ACTION(qAction.ActionId),
            mNumActionsToday, getPrioName(qAction.Prio), Sim.GetHour(), Sim.GetMinute(), Insert1000erDots64(qPlayer.Money).c_str(),
            Insert1000erDots64(getMoneyAvailable()).c_str());

    mOnThePhone = 0;

    __int64 moneyAvailable = getMoneyAvailable();
    if (condAll(qAction.ActionId) == Prio::None) {
        AT_Warn("Bot::RobotExecuteAction(): Conditions not met anymore.");
        qAction.ActionId = ACTION_NONE;
    }

    qPlayer.WorkCountdown = 20 * 5;

    switch (qAction.ActionId) {
    case ACTION_NONE:
        qPlayer.WorkCountdown = 2;
        break;

    case ACTION_STARTDAY:
        actionStartDay(moneyAvailable);
        break;

    case ACTION_STARTDAY_LAPTOP:
        actionStartDayLaptop(moneyAvailable);
        break;

    case ACTION_BUERO:
        actionBuero();
        break;

    case ACTION_CALL_INTERNATIONAL:
        actionCallInternational(true);
        break;

    case ACTION_CALL_INTER_HANDY:
        AT_Log("Bot::RobotExecuteAction(): Calling international using mobile phone.");
        actionCallInternational(false);
        mOnThePhone = 30;
        break;

    case ACTION_CHECKAGENT1:
        actionCheckLastMinute();
        qPlayer.WorkCountdown = 20 * 7;
        break;

    case ACTION_CHECKAGENT2:
        actionCheckTravelAgency();
        qPlayer.WorkCountdown = 20 * 7;
        break;

    case ACTION_CHECKAGENT3:
        actionCheckFreightDepot();
        qPlayer.WorkCountdown = 20 * 7;
        break;

    case ACTION_UPGRADE_PLANES:
        actionUpgradePlanes();
        break;

    case ACTION_BUYNEWPLANE:
        actionBuyNewPlane(moneyAvailable);
        break;

    case ACTION_BUYUSEDPLANE:
        actionBuyUsedPlane(moneyAvailable);
        break;

    case ACTION_VISITMUSEUM:
        actionMuseumCheckPlanes();
        break;

    case ACTION_PERSONAL:
        actionVisitHR();
        break;

    case ACTION_BUY_KEROSIN:
        actionBuyKerosine(moneyAvailable);
        break;

    case ACTION_BUY_KEROSIN_TANKS:
        actionBuyKerosineTank(moneyAvailable);
        break;

    case ACTION_SABOTAGE:
        actionSabotage(moneyAvailable);
        break;

    case ACTION_VISITSABOTEUR:
        actionVisitSaboteur();
        break;

    case ACTION_SET_DIVIDEND: {
        SLONG targetDividend = qPlayer.Dividende;
        SLONG maxToEmit = 0;
        GameMechanic::canEmitStock(qPlayer, &maxToEmit);
        if (kReduceDividend && maxToEmit < 10000) {
            /* we cannot emit any shares anymore. We do not care about stock prices now. */
            targetDividend = 0;
        } else if (qPlayer.RobotUse(ROBOT_USE_HIGHSHAREPRICE)) {
            targetDividend = 25;
        } else if (LocalRandom.Rand(10) == 0) {
            targetDividend++;
        }

        Limit(5, targetDividend, 25);
        if (targetDividend != qPlayer.Dividende) {
            AT_Log("Bot::RobotExecuteAction(): Setting dividend to %d", targetDividend);
            GameMechanic::setDividend(qPlayer, targetDividend);
        }
    } break;

    case ACTION_RAISEMONEY: {
        __int64 limit = qPlayer.CalcCreditLimit();
        __int64 moneyRequired = -getMoneyAvailable();
        __int64 m = std::min(limit, moneyRequired);
        m = std::max(m, 1000LL);
        if (mRunToFinalObjective == FinalPhase::TargetRun) {
            m = limit;
        }
        if (m > 0) {
            AT_Log("Bot::RobotExecuteAction(): Taking loan: %s $", Insert1000erDots64(m).c_str());
            GameMechanic::takeOutCredit(qPlayer, m);
            moneyAvailable = getMoneyAvailable();
        }
    } break;

    case ACTION_DROPMONEY: {
        __int64 m = std::min({qPlayer.Credit, moneyAvailable, getWeeklyOpSaldo()});
        AT_Log("Bot::RobotExecuteAction(): Paying back loan: %s $", Insert1000erDots64(m).c_str());
        GameMechanic::payBackCredit(qPlayer, m);
        moneyAvailable = getMoneyAvailable();
    } break;

    case ACTION_EMITSHARES:
        actionEmitShares();
        qPlayer.WorkCountdown = 20 * 6;
        break;

    case ACTION_SELLSHARES:
        actionSellShares(moneyAvailable);
        qPlayer.WorkCountdown = 20 * 6;
        break;

    case ACTION_BUYSHARES: {
        if (condBuyOwnShares(moneyAvailable) != Prio::None) {
            actionBuyOwnShares(moneyAvailable);
        }
        if (condBuyNemesisShares(moneyAvailable) != Prio::None) {
            actionBuyNemesisShares(moneyAvailable);
        }
    } break;

    case ACTION_OVERTAKE_AIRLINE:
        actionOvertakeAirline();
        qPlayer.WorkCountdown = 20 * 6;
        break;

    case ACTION_VISITMECH:
        actionVisitMech();
        break;

    case ACTION_VISITNASA: {
        const auto &qPrices = (Sim.Difficulty == DIFF_FINAL) ? RocketPrices : StationPrices;
        for (SLONG i = 0; i < qPrices.size(); i++) {
            if ((qPlayer.RocketFlags & (1 << i)) == 0 && moneyAvailable >= qPrices[i]) {
                qPlayer.ChangeMoney(-qPrices[i], 3400, "");
                PlayFanfare();
                qPlayer.RocketFlags |= (1 << i);
            }
            moneyAvailable = getMoneyAvailable();
        }
    } break;

    case ACTION_VISITTELESCOPE:
        break;

    case ACTION_VISITKIOSK:
        break;

    case ACTION_VISITMAKLER: {
        auto list = findBestAvailablePlaneType(false, true);
        mBestPlaneTypeId = list.empty() ? -1 : list[0];

        if (mItemAntiStrike == 0) {
            if (GameMechanic::PickUpItemResult::PickedUp == GameMechanic::pickUpItem(qPlayer, ITEM_BH)) {
                AT_Log("Bot::RobotExecuteAction(): Picked up item BH");
                mItemAntiStrike = 1;
            }
        }
    } break;

    case ACTION_VISITARAB:
        if (mItemArabTrust == 1) {
            if (GameMechanic::useItem(qPlayer, ITEM_MG)) {
                AT_Log("Bot::RobotExecuteAction(): Used item MG");
                mItemArabTrust = 2;
            }
        }
        break;

    case ACTION_VISITRICK:
        if (mItemAntiStrike == 3) {
            if (GameMechanic::useItem(qPlayer, ITEM_HUFEISEN)) {
                AT_Log("Bot::RobotExecuteAction(): Used item horse shoe");
                mItemAntiStrike = 4;
            }
        }
        if (qPlayer.StrikeHours > 0 && qPlayer.StrikeEndType == 0 && mItemAntiStrike == 4) {
            AT_Log("Bot::RobotExecuteAction(): Ended strike using drunk guy");
            GameMechanic::endStrike(qPlayer, GameMechanic::EndStrikeMode::Drunk);
        }
        break;

    case ACTION_VISITDUTYFREE:
        actionVisitDutyFree(moneyAvailable);
        break;

    case ACTION_VISITAUFSICHT:
        actionVisitBoss();
        break;

    case ACTION_EXPANDAIRPORT:
        AT_Log("Bot::RobotExecuteAction(): Expanding Airport");
        GameMechanic::expandAirport(qPlayer);
        qPlayer.WorkCountdown = 20 * 7;
        break;

    case ACTION_VISITROUTEBOX:
        actionVisitRouteBox();
        break;

    case ACTION_VISITROUTEBOX2:
        actionRentRoute();
        qPlayer.WorkCountdown = 20 * 7;
        break;

    case ACTION_VISITSECURITY:
        actionVisitSecurity(moneyAvailable);
        qPlayer.WorkCountdown = 20 * 7;
        break;

    case ACTION_VISITSECURITY2:
        if (GameMechanic::sabotageSecurityOffice(qPlayer)) {
            AT_Log("Bot::RobotExecuteAction(): Successfully sabotaged security office.");
        } else {
            AT_Error("Bot::RobotExecuteAction(): Failed to sabotage security office!");
        }
        mNeedToShutdownSecurity = false;
        mLastTimeInRoom.erase(ACTION_SABOTAGE); /* allow sabotage again */
        qPlayer.WorkCountdown = 20 * 1;
        break;

    case ACTION_VISITDESIGNER:
        actionBuyDesignerPlane(moneyAvailable);
        qPlayer.WorkCountdown = 20 * 7;
        break;

    case ACTION_WERBUNG_ROUTES:
        actionBuyAdsForRoutes(moneyAvailable);
        qPlayer.WorkCountdown = 20 * 7;
        break;

    case ACTION_WERBUNG:
        actionBuyAds(moneyAvailable);
        qPlayer.WorkCountdown = 20 * 7;
        break;

    case ACTION_VISITADS:
        actionVisitAds();
        qPlayer.WorkCountdown = 20 * 7;
        break;

    default:
        AT_Error("Bot::RobotExecuteAction(): Trying to execute invalid action: %s", Translate_ACTION(qAction.ActionId));
        DebugBreak();
    }

    mLastTimeInRoom[qAction.ActionId] = Sim.Time;

    if (qPlayer.RobotUse(ROBOT_USE_WORKQUICK_2) && qPlayer.WorkCountdown > 2) {
        qPlayer.WorkCountdown /= 2;
    }

    if (qPlayer.RobotUse(ROBOT_USE_WORKVERYQUICK) && qPlayer.WorkCountdown > 4) {
        qPlayer.WorkCountdown /= 4;
    } else if (qPlayer.RobotUse(ROBOT_USE_WORKQUICK) && qPlayer.WorkCountdown > 2) {
        qPlayer.WorkCountdown /= 2;
    }

    AT_Log("");
}

TEAKFILE &operator<<(TEAKFILE &File, const Bot &bot) {
    File << static_cast<SLONG>(bot.mLastTimeInRoom.size());
    for (const auto &i : bot.mLastTimeInRoom) {
        File << i.first << i.second;
    }

    File << bot.mNumActionsToday;

    File << static_cast<SLONG>(bot.mPlanesForJobs.size());
    for (const auto &i : bot.mPlanesForJobs) {
        File << i;
    }

    File << static_cast<SLONG>(bot.mPlanesForJobsUnassigned.size());
    for (const auto &i : bot.mPlanesForJobsUnassigned) {
        File << i;
    }

    File << static_cast<SLONG>(bot.mPlanesForRoutes.size());
    for (const auto &i : bot.mPlanesForRoutes) {
        File << i;
    }

    File << static_cast<SLONG>(bot.mPlanesForRoutesUnassigned.size());
    for (const auto &i : bot.mPlanesForRoutesUnassigned) {
        File << i;
    }

    File << static_cast<SLONG>(bot.mKnownPlaneTypes.size());
    for (const auto &i : bot.mKnownPlaneTypes) {
        File << i;
    }

    File << bot.mLongTermStrategy;
    File << bot.mBestPlaneTypeId << bot.mBestUsedPlaneIdx;
    File << bot.mBuyPlaneForRouteId << bot.mPlaneTypeForNewRoute;

    File << static_cast<SLONG>(bot.mPlanesForNewRoute.size());
    for (const auto &i : bot.mPlanesForNewRoute) {
        File << i;
    }

    File << bot.mWantToRentRouteId;
    File << bot.mFirstRun << bot.mDayStarted << bot.mDoRoutes;
    File << static_cast<SLONG>(bot.mRunToFinalObjective) << bot.mMoneyForFinalObjective;
    File << bot.mOutOfGates;
    File << bot.mNeedToPlanJobs << bot.mNeedToPlanRoutes;
    File << bot.mMoneyReservedForRepairs << bot.mMoneyReservedForUpgrades;
    File << bot.mMoneyReservedForAuctions << bot.mMoneyReservedForFines;
    File << bot.mNemesis << bot.mNemesisScore << bot.mNeedToShutdownSecurity << bot.mUsingSecurity;
    File << bot.mNemesisSabotaged << bot.mArabHintsTracker << bot.mCurrentImage;

    File << bot.mBossNumCitiesAvailable;
    File << bot.mBossGateAvailable;

    File << bot.mTankRatioEmptiedYesterday;
    File << bot.mKerosineUsedTodaySoFar;
    File << bot.mKerosineLevelLastChecked;

    File << static_cast<SLONG>(bot.mRoutes.size());
    for (const auto &i : bot.mRoutes) {
        File << i.routeId << i.routeReverseId << i.planeTypeId;
        File << i.routeUtilization << i.routeOwnUtilization << i.image;
        File << i.planeUtilization << i.planeUtilizationFC;
        File << i.ticketCostFactor;

        File << static_cast<SLONG>(i.planeIds.size());
        for (const auto &j : i.planeIds) {
            File << j;
        }
        File << i.canUpgrade;
    }

    File << static_cast<SLONG>(bot.mRoutesSortedByOwnUtilization.size());
    for (const auto &i : bot.mRoutesSortedByOwnUtilization) {
        File << i;
    }

    File << bot.mRoutesUpdated << bot.mRoutesUtilizationUpdated;
    File << static_cast<SLONG>(bot.mRoutesNextStep) << bot.mImproveRouteId;

    File << bot.mNumEmployees << bot.mExtraPilots << bot.mExtraBegleiter;

    File << bot.mItemPills << bot.mItemAntiVirus << bot.mItemAntiStrike << bot.mItemArabTrust << bot.mIsSickToday;

    File << static_cast<SLONG>(bot.mPlanerSolution.list.size());
    for (const auto &solution : bot.mPlanerSolution.list) {
        File << static_cast<SLONG>(solution.jobs.size());
        for (const auto &i : solution.jobs) {
            File << i.jobIdx;
            File << i.objectId;
            File << i.bIsFreight;
            File << i.scoreRatio;
            File << i.start;
            File << i.end;
        }

        File << solution.totalPremium;
        File << solution.planeId;
        File << solution.scheduleFromTime;
    }
    File << static_cast<SLONG>(bot.mPlanerSolution.toTake.size());
    for (const auto &i : bot.mPlanerSolution.toTake) {
        File << i.jobIdx;
        File << i.objectId;
        File << i.sourceId;
        File << static_cast<SLONG>(i.owner);
    }

    File << bot.mOnThePhone;

    File << bot.mDesignerPlane;
    File << bot.mDesignerPlaneFile;

    SLONG magicnumber = 0x42;
    File << magicnumber;

    return (File);
}

TEAKFILE &operator>>(TEAKFILE &File, Bot &bot) {
    SLONG size{};

    File >> size;
    bot.mLastTimeInRoom.clear();
    for (SLONG i = 0; i < size; i++) {
        SLONG key{};
        SLONG value{};
        File >> key >> value;
        bot.mLastTimeInRoom[key] = value;
    }
    assert(bot.mLastTimeInRoom.size() == size);

    File >> bot.mNumActionsToday;

    File >> size;
    bot.mPlanesForJobs.resize(size);
    for (SLONG i = 0; i < size; i++) {
        File >> bot.mPlanesForJobs[i];
    }

    File >> size;
    bot.mPlanesForJobsUnassigned.resize(size);
    for (SLONG i = 0; i < size; i++) {
        File >> bot.mPlanesForJobsUnassigned[i];
    }

    File >> size;
    bot.mPlanesForRoutes.resize(size);
    for (SLONG i = 0; i < size; i++) {
        File >> bot.mPlanesForRoutes[i];
    }

    File >> size;
    bot.mPlanesForRoutesUnassigned.resize(size);
    for (SLONG i = 0; i < size; i++) {
        File >> bot.mPlanesForRoutesUnassigned[i];
    }

    File >> size;
    bot.mKnownPlaneTypes.resize(size);
    for (SLONG i = 0; i < size; i++) {
        File >> bot.mKnownPlaneTypes[i];
    }

    File >> bot.mLongTermStrategy;
    File >> bot.mBestPlaneTypeId >> bot.mBestUsedPlaneIdx;
    File >> bot.mBuyPlaneForRouteId >> bot.mPlaneTypeForNewRoute;

    File >> size;
    bot.mPlanesForNewRoute.resize(size);
    for (SLONG i = 0; i < size; i++) {
        File >> bot.mPlanesForNewRoute[i];
    }

    File >> bot.mWantToRentRouteId;
    File >> bot.mFirstRun >> bot.mDayStarted >> bot.mDoRoutes;
    SLONG runToFinalObjective = 0;
    File >> runToFinalObjective >> bot.mMoneyForFinalObjective;
    bot.mRunToFinalObjective = static_cast<Bot::FinalPhase>(runToFinalObjective);
    File >> bot.mOutOfGates;
    File >> bot.mNeedToPlanJobs >> bot.mNeedToPlanRoutes;
    File >> bot.mMoneyReservedForRepairs >> bot.mMoneyReservedForUpgrades;
    File >> bot.mMoneyReservedForAuctions >> bot.mMoneyReservedForFines;
    File >> bot.mNemesis >> bot.mNemesisScore >> bot.mNeedToShutdownSecurity >> bot.mUsingSecurity;
    File >> bot.mNemesisSabotaged >> bot.mArabHintsTracker >> bot.mCurrentImage;

    File >> bot.mBossNumCitiesAvailable;
    File >> bot.mBossGateAvailable;

    File >> bot.mTankRatioEmptiedYesterday;
    File >> bot.mKerosineUsedTodaySoFar;
    File >> bot.mKerosineLevelLastChecked;

    File >> size;
    bot.mRoutes.resize(size);
    for (auto &iter : bot.mRoutes) {
        Bot::RouteInfo info;
        File >> info.routeId >> info.routeReverseId >> info.planeTypeId;
        File >> info.routeUtilization >> info.routeOwnUtilization >> info.image;
        File >> info.planeUtilization >> info.planeUtilizationFC;
        File >> info.ticketCostFactor;

        File >> size;
        info.planeIds.resize(size);
        for (auto &iter2 : info.planeIds) {
            File >> iter2;
        }
        File >> info.canUpgrade;

        iter = info;
    }

    File >> size;
    bot.mRoutesSortedByOwnUtilization.resize(size);
    for (SLONG i = 0; i < size; i++) {
        File >> bot.mRoutesSortedByOwnUtilization[i];
    }

    File >> bot.mRoutesUpdated >> bot.mRoutesUtilizationUpdated;
    SLONG routesNextStep = 0;
    File >> routesNextStep >> bot.mImproveRouteId;
    bot.mRoutesNextStep = static_cast<Bot::RoutesNextStep>(routesNextStep);

    File >> bot.mNumEmployees >> bot.mExtraPilots >> bot.mExtraBegleiter;

    File >> bot.mItemPills >> bot.mItemAntiVirus >> bot.mItemAntiStrike >> bot.mItemArabTrust >> bot.mIsSickToday;

    File >> size;
    bot.mPlanerSolution.list.resize(size);
    for (auto &solution : bot.mPlanerSolution.list) {
        File >> size;
        solution.jobs.resize(size);
        for (auto &i : solution.jobs) {
            File >> i.jobIdx;
            File >> i.objectId;
            File >> i.bIsFreight;
            File >> i.scoreRatio;
            File >> i.start;
            File >> i.end;
        }

        File >> solution.totalPremium;
        File >> solution.planeId;
        File >> solution.scheduleFromTime;
    }
    File >> size;
    bot.mPlanerSolution.toTake.resize(size);
    for (auto &i : bot.mPlanerSolution.toTake) {
        File >> i.jobIdx;
        File >> i.objectId;
        File >> i.sourceId;
        SLONG owner{};
        File >> owner;
        i.owner = static_cast<BotPlaner::JobOwner>(owner);
    }

    File >> bot.mOnThePhone;

    File >> bot.mDesignerPlane;
    File >> bot.mDesignerPlaneFile;

    SLONG magicnumber = 0;
    File >> magicnumber;
    assert(magicnumber == 0x42);

    return (File);
}
