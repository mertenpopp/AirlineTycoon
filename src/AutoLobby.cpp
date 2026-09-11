//============================================================================================
// AutoLobby.cpp : Unattended multiplayer setup for the test harness.
//============================================================================================
// Link: "AutoLobby.h"
//============================================================================================
#include "AutoLobby.h"

#include "AtNet.h"
#include "class.h"
#include "GameMechanic.h"
#include "global.h"
#include "helper.h"
#include "NetTrace.h"
#include "Proto.h"
#include "StdRaum.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#define AT_Log(...) AT_Log_I("AutoLobby", __VA_ARGS__)

AutoLobbyRole gAutoLobbyRole = AutoLobbyRole::NONE;
SLONG gAutoLobbySlot = 0;
SLONG gAutoLobbyHumans = 2;
SLONG gAutoLobbyBots = 0;
CString gAutoLobbyHostIP = "127.0.0.1";
SLONG gAutoLobbyTimeout = 120;
SLONG gAutoLobbyGoHome = 0;
SLONG gAutoLobbyCutSalaries = 0;

namespace {
DWORD gStartedAt = 0;
DWORD gQuitAt = 0;
} // namespace

void AutoLobbyApplyOptions() {
    if (!AutoLobbyActive()) {
        return;
    }

    Limit(SLONG(0), gAutoLobbySlot, SLONG(3));
    Limit(SLONG(2), gAutoLobbyHumans, SLONG(4));

    /* Claim our airline deterministically. Both the host ("this is me") and the client (the
       WantedIndex in ATNET_WANNAJOIN) read the slot from here. */
    Sim.Options.OptionLastPlayer = gAutoLobbySlot;

    /* A networked game autosaves ~12 MB into slot 11 at every day boundary
       (AtNet.cpp, ATNET_DAYFINISHALL). Four peers doing that is pointless I/O at best and a
       corrupted file when they share a directory. */
    Sim.Options.OptionAutosave = 0;

    CheatAutoSkip = 1;
    Sim.Difficulty = DIFF_FREEGAME;

    /* The single player harness gets this via gQuickTestRun, which we cannot use here: it
       makes NewGamePopup short-circuit into a local game and never open the lobby at all. */
    Sim.Options.OptionFullscreen = 1;
    if (gAutoQuitOnDay < 0) {
        gAutoQuitOnDay = 99;
    }

    /* Comparing peers after the fact is the whole point of the harness. */
    if (gNetTraceLevel <= 0) {
        gNetTraceLevel = 1;
    }

    AT_Log("Role=%s slot=%ld humans=%ld bots=%ld host=%s timeout=%lds gohome=%ld cutsalaries=%ld trace=%ld",
           gAutoLobbyRole == AutoLobbyRole::HOST ? "HOST" : "JOIN", static_cast<long>(gAutoLobbySlot), static_cast<long>(gAutoLobbyHumans),
           static_cast<long>(gAutoLobbyBots), gAutoLobbyHostIP.c_str(), static_cast<long>(gAutoLobbyTimeout), static_cast<long>(gAutoLobbyGoHome),
           static_cast<long>(gAutoLobbyCutSalaries), static_cast<long>(gNetTraceLevel));
}

void AutoLobbyStartClock() {
    if (gStartedAt == 0) {
        gStartedAt = AtGetTime();
    }
}

bool AutoLobbyTimedOut() {
    if (!AutoLobbyActive() || gStartedAt == 0 || gAutoLobbyTimeout <= 0) {
        return false;
    }
    return (AtGetTime() - gStartedAt) > DWORD(gAutoLobbyTimeout) * 1000;
}

void AutoLobbyAbort(const char *Reason, ...) {
    char Text[512];
    va_list Args;
    va_start(Args, Reason);
    vsnprintf(Text, sizeof(Text), Reason, Args);
    va_end(Args);

    AT_Log("ABORT: %s", Text);
    NetTraceEvent("LOBBYABORT %s", Text);

    /* Flush whatever the trace has buffered before we go. */
    fflush(stdout);
    fflush(stderr);

    /* _exit rather than exit: we are called from a timer callback deep inside the room code,
       and running the static destructors from here crashes on the way out, which would turn a
       clean "harness gave up" into a segfault the harness has to interpret. */
    _exit(1);
}

void AutoLobbyPumpDay() {
    if (!AutoLobbyActive() || gAutoLobbyGoHome <= 0 || Sim.bNetwork == 0 || Sim.Gamestate != (GAMESTATE_PLAYING | GAMESTATE_WORKING)) {
        return;
    }

    /* Once per day, and not so close to 18:00 that the game closes the day on its own before
       the other peers have heard from us. */
    static SLONG WentHomeOnDay = -1;
    if (WentHomeOnDay == Sim.Date || Sim.GetHour() < gAutoLobbyGoHome || Sim.Time >= 17 * 60000 + 30 * 1000) {
        return;
    }

    /* Say once per game hour what is holding us back, so that a run which never goes home
       shows why instead of silently taking the slow path. */
    static SLONG LastReportedHour = -1;
    auto Blocked = [](const char *Why) {
        if (LastReportedHour != Sim.GetHour()) {
            LastReportedHour = Sim.GetHour();
            NetTraceEvent("GOHOME blocked by %s", Why);
        }
    };

    if (Sim.bNoTime != 0) {
        return Blocked("bNoTime");
    }
    if (Sim.CallItADay != 0 || Sim.CallItADayAt != 0) {
        return Blocked("CallItADay");
    }

    PLAYER &qPlayer = Sim.Players.Players[Sim.localPlayer];
    if (qPlayer.IsOut != 0) {
        return Blocked("IsOut");
    }
    /* Any room will do: a player can ask to go home from anywhere (TAB), and the auto-skip
       leaves our human sitting in the office. */
    if (qPlayer.LocationWin == nullptr) {
        return Blocked(bprintf("room %ld without window", static_cast<long>(qPlayer.GetRoom())));
    }
    CStdRaum &qWin = *qPlayer.LocationWin;
    if (qWin.IsDialogOpen() != 0 || qWin.MenuIsOpen() != 0) {
        return Blocked(bprintf("dialog %ld menu %ld", static_cast<long>(qWin.IsDialogOpen()), static_cast<long>(qWin.MenuIsOpen())));
    }

    WentHomeOnDay = Sim.Date;

    /* Once per game, before the first time going home: cut everybody's salary, as the
       personnel manager's dialog does. The staff's happiness drops by 25 per cut, which
       (at 4 cuts) makes them strike the next morning - to exercise the strike on every peer. */
    static bool bCutSalaries = false;
    if (!bCutSalaries && gAutoLobbyCutSalaries > 0) {
        bCutSalaries = true;
        NetTraceEvent("CUTSALARIES x%ld", static_cast<long>(gAutoLobbyCutSalaries));
        for (SLONG c = 0; c < gAutoLobbyCutSalaries; c++) {
            GameMechanic::decreaseAllSalaries(qPlayer);
        }
    }

    /* Exactly what answering "yes" to the call-it-a-day request does in a network game
       (CStdRaum::OnLButtonDown, MENU_REQUEST_CALLITADAY). The auto-skip already set the local
       CallItADay flag in the briefing room, but only locally: without ATNET_DAYFINISH the host
       never learns that this human is done, so the day always ran to 18:00 at normal pace. */
    AT_Log("Calling it a day at %02ld:%02ld", static_cast<long>(Sim.GetHour()), static_cast<long>(Sim.GetMinute()));
    NetTraceEvent("GOHOME");

    /* Open the request first, as the click does: MENU_CALLITADAY relies on the request's
       bitmaps still being loaded when network3.gli is missing, which it is in the GOG data. */
    qWin.MenuStart(MENU_REQUEST, MENU_REQUEST_CALLITADAY, 0);

    qPlayer.CallItADay = TRUE;
    Sim.DayState = 2;
    qWin.MenuStart(MENU_CALLITADAY);
    qPlayer.WalkStopEx();
    SIM::SendSimpleMessage(ATNET_DAYFINISH, 0, Sim.localPlayer);
    SIM::SendChatBroadcast(bprintf(StandardTexte.GetS(TOKEN_MISC, 7020), qPlayer.NameX.c_str()));
}

void AutoLobbyScheduleQuit(DWORD DelayMs) {
    if (AutoLobbyActive() && gQuitAt == 0) {
        gQuitAt = AtGetTime() + DelayMs;
        AT_Log("Run complete, quitting in %lu ms", static_cast<unsigned long>(DelayMs));
    }
}

void AutoLobbyPollQuit() {
    if (gQuitAt == 0 || AtGetTime() < gQuitAt) {
        return;
    }
    /* _exit for the same reason as in AutoLobbyAbort(): the static destructors crash when
       run from here, and the harness would read that as a failed run. */
    fflush(stdout);
    fflush(stderr);
    _exit(0);
}
