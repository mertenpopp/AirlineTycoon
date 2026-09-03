//============================================================================================
// AutoLobby.cpp : Unattended multiplayer setup for the test harness.
//============================================================================================
// Link: "AutoLobby.h"
//============================================================================================
#include "AutoLobby.h"

#include "class.h"
#include "global.h"
#include "helper.h"
#include "NetTrace.h"
#include "Proto.h"

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

namespace {
DWORD gStartedAt = 0;
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

    AT_Log("Role=%s slot=%ld humans=%ld bots=%ld host=%s timeout=%lds trace=%ld",
           gAutoLobbyRole == AutoLobbyRole::HOST ? "HOST" : "JOIN", static_cast<long>(gAutoLobbySlot), static_cast<long>(gAutoLobbyHumans),
           static_cast<long>(gAutoLobbyBots), gAutoLobbyHostIP.c_str(), static_cast<long>(gAutoLobbyTimeout), static_cast<long>(gNetTraceLevel));
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
