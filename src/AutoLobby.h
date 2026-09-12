#pragma once
//============================================================================================
// AutoLobby.h - Unattended multiplayer setup for the test harness.
//============================================================================================
// The multiplayer lobby normally needs a human: pick a provider, create or join a session,
// wait for the other players, claim an airline, then start. "/quick" cannot help here
// because it short-circuits NewGamePopup entirely, which only works for a game that needs
// no agreement with anybody.
//
// Instead the driver below replaces the mouse: once per timer tick it looks at the page the
// lobby is on and performs exactly what the corresponding click handler would, gated on
// exactly the same preconditions. It deliberately does not bypass those checks - bypassing
// them is what would produce a half-joined session that then deadlocks.
//
// Command line:
//
//   /mphost <slot> <humans> [botlevels]   host the session, e.g. /mphost 0 2 44
//   /mpjoin <ip> <slot>                   join one,          e.g. /mpjoin 127.0.0.1 1
//
//   <slot>       0..3, which airline this instance plays. Passing it explicitly is what
//                makes runs reproducible: without it every instance asks for the same
//                Sim.Options.OptionLastPlayer and the host shuffles the collisions to
//                whatever slot happens to be free, so airlines depend on join order.
//   <humans>     total human peers including the host. The host waits for exactly this
//                many before starting, rather than for "more than one".
//   [botlevels]  bot levels for the remaining slots, same digit encoding as /setbotlevel.
//
//   /mptimeout <seconds>  give up if the game has not started by then (default 120)
//   /mpgohome <hour>      call it a day at that hour, like a player clicking "go home";
//                         0 (default) stays until the game closes the day at 18:00
//   /mpcutsalaries <n>    before going home the first time, cut all salaries n times, as the
//                         personnel dialog does (needs /mpgohome); 4 provokes a strike
//   /mpactions 1          the idle human also does what a player does in the rooms: bid for a
//                         branch and a gate, refit a plane, give up a branch, buy a used plane
//                         and be nice to a competitor - one action per game hour on days 1 and 2
//
// Both roles imply CheatAutoSkip, a free game, no autosave (a networked game autosaves 12 MB
// per in-game day, and peers sharing a directory would fight over the file), and protocol
// tracing at level 1.
//============================================================================================

#include "defines.h"
#include "TeakLibW.h"

enum class AutoLobbyRole { NONE, HOST, JOIN };

extern AutoLobbyRole gAutoLobbyRole;
extern SLONG gAutoLobbySlot;     // 0..3: which airline this instance claims
extern SLONG gAutoLobbyHumans;   // host: number of human peers to wait for, including itself
extern SLONG gAutoLobbyBots;     // host: bot level digits for the remaining slots
extern CString gAutoLobbyHostIP; // client: address of the host
extern SLONG gAutoLobbyTimeout;  // seconds before the run is declared stuck
extern SLONG gAutoLobbyGoHome;   // hour at which the idle human calls it a day, 0 = never
extern SLONG gAutoLobbyCutSalaries; // times the idle human cuts all salaries once, before first going home
extern SLONG gAutoLobbyActions;     // 1 = the idle human also bids, gives up a branch, refits and buys

inline bool AutoLobbyActive() { return gAutoLobbyRole != AutoLobbyRole::NONE; }

/* Applies the settings both roles need. Called once, before the lobby opens. */
void AutoLobbyApplyOptions();

/* Milliseconds since the lobby driver started, for the watchdog. */
void AutoLobbyStartClock();
bool AutoLobbyTimedOut();

/* Ends a harness run cleanly once the last day's morning briefing barrier has passed. The quit
   is deferred by DelayMs so that RakNet - which sends from its own thread - gets this peer's
   last messages out; AutoLobbyPollQuit() from the main loop then terminates. */
void AutoLobbyScheduleQuit(DWORD DelayMs);

/* Lets the idle human end the day at /mpgohome's hour. Called from the main loop. */
void AutoLobbyPumpDay();
void AutoLobbyPumpActions();
void AutoLobbyPollQuit();

/* Logs why we are stuck and terminates with a non-zero exit code. There is nobody to read a
   message box, so a harness run must fail loudly rather than hang. */
void AutoLobbyAbort(const char *Reason, ...);
