#include "Bot.h"

#include "BotHelper.h"
#include "Proto.h"
#include "global.h"
#include "helper.h"

#include <SDL_log.h>

#include <cstdio>
#include <iostream>

template <class... Types> void AT_Error(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_ERROR, "Bot", args...); }
template <class... Types> void AT_Warn(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_WARN, "Bot", args...); }
template <class... Types> void AT_Info(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_INFO, "Bot", args...); }
template <class... Types> void AT_Log(Types... args) { AT_Log_I("Bot", args...); }

const SLONG kRouteAvgDays = 3;

Bot::Bot(PLAYER &player) : qPlayer(player) {}

void Bot::RobotInit() {
    auto balance = qPlayer.BilanzWoche.Hole();
    AT_Info("Bot.cpp: Enter RobotInit() for %s: Current day: %d, money: %s $ (op saldo %s = %s %s)", qPlayer.Abk.c_str(), Sim.Date,
            Insert1000erDots64(qPlayer.Money).c_str(), Insert1000erDots64(balance.GetOpSaldo()).c_str(), Insert1000erDots64(balance.GetOpGewinn()).c_str(),
            Insert1000erDots64(balance.GetOpVerlust()).c_str());

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

        /* bot level */
        AT_Log("Bot::RobotInit(): We are player %d with bot level = %s.", qPlayer.PlayerNum, StandardTexte.GetS(TOKEN_NEWGAME, 5001 + qPlayer.BotLevel));

        mFirstRun = false;
    }

    for (auto &i : qPlayer.RobotActions) {
        i = {};
    }

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

    auto &qRobotActions = qPlayer.RobotActions;

    if (qRobotActions[0].ActionId != ACTION_NONE || qRobotActions[1].ActionId != ACTION_NONE) {
        AT_Log("Bot.cpp: Leaving RobotPlan() (actions already planned)\n");
        return;
    }

    auto &qFirstAction = qRobotActions[1];
    auto &qSecondAction = qRobotActions[2];
    qFirstAction.ActionId = ACTION_NONE;
    qSecondAction.ActionId = ACTION_NONE;

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
        return;
    }
    if (Sim.GetHour() >= 18) {
        AT_Log("Bot.cpp: Leaving RobotExecuteAction() (too late)\n");
        return;
    }

    auto &qAction = qPlayer.RobotActions[0];
    LocalRandom.Rand(2); // Sicherheitshalber, damit wir immer genau ein Random ausführen

    AT_Info("Bot::RobotExecuteAction() for %s: Executing %s, current time: %02ld:%02ld, money: %s $ (available: %s $)", qPlayer.Abk.c_str(),
            Translate_ACTION(qAction.ActionId), Sim.GetHour(), Sim.GetMinute(), Insert1000erDots64(qPlayer.Money).c_str(),
            Insert1000erDots64(getMoneyAvailable()).c_str());

    mOnThePhone = 0;

    qPlayer.WorkCountdown = 20 * 5;

    switch (qAction.ActionId) {
    case ACTION_NONE:
        qPlayer.WorkCountdown = 2;
        break;

    default:
        AT_Error("Bot::RobotExecuteAction(): Trying to execute invalid action: %s", Translate_ACTION(qAction.ActionId));
        DebugBreak();
    }

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

SLONG Bot::getNextMood() {
    SLONG mood = mMood;
    mMood = mMoodNext;
    mMoodNext = -1;
    return mood;
}

TEAKFILE &operator<<(TEAKFILE &File, const Bot &bot) {
    SLONG savegameVersion = 102;
    File << savegameVersion;

    File << bot.mFirstRun;
    File << bot.mIsSickToday;

    File << bot.mOnThePhone;

    SLONG magicnumber = 0x42;
    File << magicnumber;

    return (File);
}

TEAKFILE &operator>>(TEAKFILE &File, Bot &bot) {
    SLONG savegameVersion;
    File >> savegameVersion;

    File >> bot.mFirstRun;
    File >> bot.mIsSickToday;

    File >> bot.mOnThePhone;

    SLONG magicnumber = 0;
    File >> magicnumber;
    assert(magicnumber == 0x42);

    return (File);
}
