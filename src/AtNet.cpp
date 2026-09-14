//============================================================================================
// AtNet.cpp : Handling the Network things:
//============================================================================================
// Link: "AtNet.h"
//============================================================================================
#include "AtNet.h"
#include "NetTrace.h"

#include "Bot.h"
#include "Buero.h"
#include "GameMechanic.h"
#include "global.h"
#include "helper.h"
#include "network.h"
#include "Proto.h"
#include "SbLib.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <vector>

#define AT_Log(...) AT_Log_I("AtNet", __VA_ARGS__)

extern SBNetwork gNetwork;

#define GFX_MENU (0x00000000554e454d)

SLONG nPlayerOptionsOpen[4] = {0, 0, 0, 0};  // Fummelt gerade wer an den Options?
SLONG nPlayerAppsDisabled[4] = {0, 0, 0, 0}; // Ist ein anderer Spieler gerade in einer anderen Anwendung?
SLONG nPlayerWaiting[4] = {0, 0, 0, 0};      // Hinkt jemand hinterher?

extern SLONG gTimerCorrection; // Is it necessary to adapt the local clock to the server clock?

// Zum Debuggen:
SLONG rChkTime = 0;
ULONG rChkPersonRandCreate = 0, rChkPersonRandMisc = 0, rChkHeadlineRand = 0;
ULONG rChkLMA = 0, rChkRBA = 0, rChkAA[MAX_CITIES], rChkFrachen = 0;
SLONG rChkGeneric, CheckGeneric = 0;
SLONG rChkActionId[5 * 4];
SLONG rChkRobotSyncAge[4];
SLONG gRobotSyncSlice[4] = {-1, -1, -1, -1}; // Sim.TimeSlice when a bot's action queue was last handed over (host) or received (client)

SLONG GenericSyncIds[4] = {0, 0, 0, 0};
static std::deque<SLONG> GenericSyncReceived[4]; // ATNET_GENERICSYNC ids per player, not yet waited for
SLONG GenericSyncIdPars[4] = {0, 0, 0, 0};
SLONG GenericAsyncIds[4 * 100] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
SLONG GenericAsyncIdPars[4 * 100] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

//--------------------------------------------------------------------------------------------
// Player numbers arrive straight from the network and are used to index both
// Sim.Players.Players (a BUFFER_V, whose operator[] is unchecked in release builds) and
// several fixed 4-element globals. A desynced or corrupt message could therefore read and
// write outside those arrays. Validate before use: the exception is caught by
// PumpNetwork(), which logs and drops the offending message.
//--------------------------------------------------------------------------------------------
static SLONG NetCheckPlayerNum(SLONG PlayerNum, ULONG MessageType) {
    if (PlayerNum < 0 || PlayerNum >= 4) {
        TeakLibW_Exception(FNL, "Invalid player number %ld in message %s", static_cast<long>(PlayerNum), Translate_ATNET(MessageType));
    }
    return PlayerNum;
}

//--------------------------------------------------------------------------------------------
// Sets the bitmap for the network to display: (0=none; 1=player in options, 2=player in windows; 3=waiting for player)
//--------------------------------------------------------------------------------------------
void SetNetworkBitmap(SLONG Number, SLONG WaitingType) {
    static SLONG CurrentNumber = -1;

    if (CurrentNumber != Number || gNetworkBmsType != WaitingType) {
        GfxLib *pGLib = nullptr;

        if (Number == 0) {
            gNetworkBms.Destroy();
            if (pGLib != nullptr) {
                pGfxMain->ReleaseLib(pGLib);
            }
        }

        try {
            if (Number == 1) {
                pGfxMain->LoadLib(const_cast<char *>((LPCTSTR)FullFilename("network1.gli", GliPath)), &pGLib, L_LOCMEM);
            }
            if (Number == 2) {
                pGfxMain->LoadLib(const_cast<char *>((LPCTSTR)FullFilename("network2.gli", GliPath)), &pGLib, L_LOCMEM);
            }
            if (Number == 3) {
                pGfxMain->LoadLib(const_cast<char *>((LPCTSTR)FullFilename("network3.gli", GliPath)), &pGLib, L_LOCMEM);
            }
        } catch (TeakLibException &e) {
            AT_Log_I("NET", "Did not find gli file, trying to continue", e.what());
            e.caught();
        }

        if (pGLib != nullptr) {
            if (Number == 3) {
                gNetworkBms.ReSize(pGLib, "MENU PL0 PL1 PL2 PL3 PL4 PL5 PL6 PL7");
            } else {
                gNetworkBms.ReSize(pGLib, "MENU");
            }
        }

        CurrentNumber = Number;
        gNetworkBmsType = WaitingType;
    }
}

//--------------------------------------------------------------------------------------------
// Look for new messages:
//--------------------------------------------------------------------------------------------
void DisplayBroadcastMessage(CString str, SLONG FromPlayer) {
    SBBM TempBm(gBroadcastBm.Size);
    SLONG sy = 0;
    SLONG oldy = 0;
    SLONG offy = 0;

    // if (!Sim.bNetwork)
    //    return;

    if (FromPlayer != Sim.localPlayer) {
        static SLONG LastTime = 0;

        if (AtGetTime() - LastTime > 500) {
            PlayUniversalFx("netmsg.raw", Sim.Options.OptionEffekte);
        }

        LastTime = AtGetTime();
    }

    if (FromPlayer >= 0 && FromPlayer < 4) {
        str = Sim.Players.Players[FromPlayer].NameX + ": " + str;
    }

    sy = gBroadcastBm.TryPrintAt(str.c_str(), FontSmallBlack, TEC_FONT_LEFT, XY(10, 10), XY(320, 1000));
    oldy = gBroadcastBm.Size.y;
    offy = gBroadcastBm.Size.y;

    if (oldy < 10) {
        oldy = 10;
    }
    if (offy < 10) {
        offy = 10;
    }

    TempBm.BlitFrom(gBroadcastBm);
    gBroadcastBm.ReSize(320, oldy + sy + 10);
    if (gBroadcastBm.Size.y != TempBm.Size.y) {
        SB_CBitmapKey Key(*XBubbleBms[9].pBitmap);
        gBroadcastBm.FillWith(*static_cast<UWORD *>(Key.Bitmap));
    }
    gBroadcastBm.BlitFrom(TempBm);

    TempBm.ReSize(320, sy + 5);
    {
        SB_CBitmapKey Key(*XBubbleBms[9].pBitmap);
        TempBm.FillWith(*static_cast<UWORD *>(Key.Bitmap));
    }
    TempBm.PrintAt(str, FontSmallBlack, TEC_FONT_LEFT, XY(10, 0), XY(320, 1000));
    gBroadcastBm.BlitFrom(TempBm, 0, offy);

    if (gBroadcastBm.Size.y > 220) {
        SBBM TempBm(gBroadcastBm.Size);

        TempBm.BlitFrom(gBroadcastBm);
        gBroadcastBm.ReSize(gBroadcastBm.Size.x, 220);
        gBroadcastBm.BlitFrom(TempBm, 0, -(TempBm.Size.y - 220));
    }

    gBroadcastTimeout = 600;
}

//--------------------------------------------------------------------------------------------
// Updates the Broadcast Bitmap:
//  bJustForEmergency : if true, then this call will only be used it this function hasn't been
//                      called normally for half a second
//--------------------------------------------------------------------------------------------
void PumpBroadcastBitmap(bool bJustForEmergency) {
    if (gBroadcastBm.Size.y == 0) {
        return;
    }

    static SLONG LastTimeCalled = 0;

    if (bJustForEmergency) {
        if (AtGetTime() - LastTimeCalled < 500) {
            return;
        }
    } else {
        LastTimeCalled = AtGetTime();
    }

    if (gBroadcastTimeout > 0 && gBroadcastBm.Size.y < 200) {
        gBroadcastTimeout--;
    } else if (gBroadcastBm.Size.y != 0) {
        static SLONG p = 0;

        gBroadcastTimeout = 0;

        if (p++ > 2 || gBroadcastBm.Size.y <= 10 || gBroadcastBm.Size.y >= 150) {
            p = 0;

            if (gBroadcastBm.Size.y - 1 <= 0) {
                gBroadcastBm.Destroy();
            } else {
                SBBM TempBm(gBroadcastBm.Size);

                TempBm.BlitFrom(gBroadcastBm);
                gBroadcastBm.ReSize(gBroadcastBm.Size.x, gBroadcastBm.Size.y - 1);
                gBroadcastBm.BlitFrom(TempBm, 0, -1);
            }
        }
    }
}

//--------------------------------------------------------------------------------------------
// Look for new messages:
//--------------------------------------------------------------------------------------------
/* Each of these counts how many peers are currently keeping the game from running: somebody in
   the options, somebody whose window lost focus, somebody busy saving. The clock runs only while
   they are all exactly zero (CTakeOffApp::GameLoop), so a count below zero stops the game as
   surely as one above it - and the peers send only the change, never the state. A peer that
   was not yet in the session when somebody opened the options receives only the "closed again"
   half of the pair. Hold them at zero from below; the peer that is really blocked keeps its own
   count. */
static void NetClampBlockers() {
    if (nOptionsOpen < 0) {
        nOptionsOpen = 0;
    }
    if (nAppsDisabled < 0) {
        nAppsDisabled = 0;
    }
    for (SLONG c = 0; c < 4; c++) {
        if (nPlayerOptionsOpen[c] < 0) {
            nPlayerOptionsOpen[c] = 0;
        }
        if (nPlayerAppsDisabled[c] < 0) {
            nPlayerAppsDisabled[c] = 0;
        }
    }
}

/* A frozen multiplayer game is the hardest thing to tell from a bug report, because nothing in
   the log says what the game is waiting for. The clock only runs while nobody is holding it up
   (CTakeOffApp::GameLoop), so while somebody is, say so - and say who - every half minute.
   Players send us their debug.txt, and this turns "it froze" into a name. Sim.bPause is left
   out: that one is the local player's own doing. */
void NetWaitWatchdog() {
    static DWORD WaitingSince = 0;
    static DWORD LastReported = 0;

    if (Sim.bNetwork == 0 || Sim.Gamestate != (GAMESTATE_PLAYING | GAMESTATE_WORKING) ||
        (nWaitingForPlayer == 0 && nOptionsOpen == 0 && nAppsDisabled == 0)) {
        WaitingSince = 0;
        return;
    }

    const DWORD Now = AtGetTime();
    if (WaitingSince == 0) {
        WaitingSince = Now;
        LastReported = Now;
        return;
    }

    if (Now - LastReported < 30000) {
        return;
    }
    LastReported = Now;

    AT_Log("The clock has been standing for %lu s at day %ld %02ld:%02ld: waiting=%ld (%ld/%ld/%ld/%ld) options=%ld (%ld/%ld/%ld/%ld) apps=%ld",
           static_cast<unsigned long>((Now - WaitingSince) / 1000), static_cast<long>(Sim.Date), static_cast<long>(Sim.GetHour()),
           static_cast<long>(Sim.GetMinute()), static_cast<long>(nWaitingForPlayer), static_cast<long>(nPlayerWaiting[0]),
           static_cast<long>(nPlayerWaiting[1]), static_cast<long>(nPlayerWaiting[2]), static_cast<long>(nPlayerWaiting[3]),
           static_cast<long>(nOptionsOpen), static_cast<long>(nPlayerOptionsOpen[0]), static_cast<long>(nPlayerOptionsOpen[1]),
           static_cast<long>(nPlayerOptionsOpen[2]), static_cast<long>(nPlayerOptionsOpen[3]), static_cast<long>(nAppsDisabled));
    NetTraceEvent("STILLWAITING seconds=%lu waiting=%ld p0=%ld p1=%ld p2=%ld p3=%ld options=%ld apps=%ld", static_cast<unsigned long>((Now - WaitingSince) / 1000),
                  static_cast<long>(nWaitingForPlayer), static_cast<long>(nPlayerWaiting[0]), static_cast<long>(nPlayerWaiting[1]),
                  static_cast<long>(nPlayerWaiting[2]), static_cast<long>(nPlayerWaiting[3]), static_cast<long>(nOptionsOpen),
                  static_cast<long>(nAppsDisabled));
}

void PumpNetwork() {
    SLONG c = 0;
    SLONG e = 0; // Universell, können von jedem case verwendet werden.

    if (Sim.bNetwork == 0) {
        return;
    }
    NetTraceCheckThread("PumpNetwork");

    if (Sim.bThisIsSessionMaster && Sim.Time > 9 * 60000 && Sim.Time < 18 * 60000 && (Sim.CallItADay == 0) && (Sim.CallItADayAt == 0)) {
        static DWORD LastTime = 0;

        if (AtGetTime() - LastTime > 1000) {
            SIM::SendSimpleMessage(ATNET_TIMEPING, 0, Sim.TimeSlice);
            LastTime = AtGetTime();
        }
    }

    bool bReturnAfterThisMessage = false;
    /* GetMessageCount() reports every packet queued in the transport, including ones
       Receive() puts back (lobby traffic) or drops. Without a bound this loop spins
       forever on such a packet, which looks like a hard freeze with no log output. */
    SLONG MessageBudget = 256;
    while ((gNetwork.GetMessageCount() != 0) && !bReturnAfterThisMessage) {
        if (--MessageBudget < 0) {
            AT_Log("PumpNetwork: message budget exhausted, %ld packets still queued", static_cast<long>(gNetwork.GetMessageCount()));
            NetTraceEvent("BUDGET queued=%ld", static_cast<long>(gNetwork.GetMessageCount()));
            break;
        }
        TEAKFILE Message;

        if (SIM::ReceiveMemFile(Message)) {
            ULONG MessageType = 0;
            SLONG Par1 = 0;
            SLONG Par2 = 0;

            /* Any malformed message now throws out of the TEAKFILE reader instead of
               silently producing garbage. Drop that one message and keep the session
               alive rather than taking the whole game down. */
            try {

            Message >> MessageType;
            // AT_Log_I("Net", "Received net event: %s", Translate_ATNET(MessageType));

            NetTraceMessage("RECV", MessageType, gNetwork.GetLocalPlayerID(), static_cast<SLONG>(Message.MemBufferUsed), -1);

            switch (MessageType) {
            case ATNET_SETSPEED:
                Message >> Par1 >> Par2;
                Sim.Players.Players[Par1].GameSpeed = Par2;
                if (Sim.Players.Players[Sim.localPlayer].LocationWin != nullptr) {
                    (Sim.Players.Players[Sim.localPlayer].LocationWin)->StatusCount = 3;
                }
                break;

            case ATNET_FORCESPEED:
                Message >> Par1;
                for (c = 0; c < 4; c++) {
                    Sim.Players.Players[c].GameSpeed = Par1;
                }
                if (Sim.Players.Players[Sim.localPlayer].LocationWin != nullptr) {
                    (Sim.Players.Players[Sim.localPlayer].LocationWin)->StatusCount = 3;
                }
                break;

            case ATNET_READYFORMORNING:
                Message >> Par1;
                Sim.Players.Players[Par1].bReadyForMorning = 1;
                break;

            case ATNET_READYFORBRIEFING:
                Message >> Par1;
                Sim.Players.Players[Par1].bReadyForBriefing = 1;
                break;

            case ATNET_PAUSE:
                GameFrame::Pause(Sim.bPause == 0);
                break;

            case ATNET_WANNAJOIN:
                if (Sim.bIsHost != 0) {
                    SIM::SendSimpleMessage(ATNET_SORRYFULL, 0);
                }
                break;

            case ATNET_CHATBROADCAST: {
                CString str;
                SLONG from = 0;

                Message >> from >> str;

                DisplayBroadcastMessage(str, from);
            } break;

            case DPSYS_HOST:
                NetTraceEvent("HOSTMIGRATION we are now host");
                Sim.bIsHost = TRUE;
                DisplayBroadcastMessage(StandardTexte.GetS(TOKEN_MISC, 7000));
                break;

            case DPSYS_SESSIONLOST:
                NetTraceEvent("SESSIONLOST humans=%ld", static_cast<long>(Sim.Players.GetAnzHumanPlayers()));
                DisplayBroadcastMessage(StandardTexte.GetS(TOKEN_MISC, 7001));
                for (c = 0; c < 4; c++) {
                    if (Sim.Players.Players[c].Owner == 2) {
                        Sim.Players.Players[c].Owner = 1;
                        Sim.Players.Players[c].GameSpeed = 3;
                        nPlayerOptionsOpen[c] = 0;
                        nPlayerAppsDisabled[c] = 0;
                        nPlayerWaiting[c] = 0;
                    }
                }

                if (Sim.Players.GetAnzHumanPlayers() == 1) {
                    gNetwork.DisConnect();
                    Sim.bNetwork = 0;
                    return;
                }

                nOptionsOpen = 0;
                nAppsDisabled = 0;

                if (nOptionsOpen == 0 && nAppsDisabled == 0 && Sim.bPause == 0) {
                    SetNetworkBitmap(0);
                }
                break;

            case DPSYS_DESTROYPLAYERORGROUP: {
                DWORD dwPlayerType = 0;
                DPID dpId = 0;

                Message >> dwPlayerType >> dpId;

                if (dwPlayerType == DPPLAYERTYPE_PLAYER) {
                    for (c = 0; c < 4; c++) {
                        if (Sim.Players.Players[c].NetworkID == dpId) {
                            nOptionsOpen -= nPlayerOptionsOpen[c];
                            nAppsDisabled -= nPlayerAppsDisabled[c];
                            nWaitingForPlayer -= nPlayerWaiting[c];

                            /* These per-player tallies have now been taken out of the global
                               counters. They must be cleared, otherwise a second disconnect
                               message for the same slot subtracts them again. */
                            nPlayerOptionsOpen[c] = 0;
                            nPlayerAppsDisabled[c] = 0;
                            nPlayerWaiting[c] = 0;

                            /* The two lower clamps used to reset nOptionsOpen as well, so
                               nAppsDisabled and nWaitingForPlayer could stay negative. Both
                               gate the main loop, and a stuck value freezes the game. */
                            if (nOptionsOpen < 0) {
                                nOptionsOpen = 0;
                            }
                            if (nAppsDisabled < 0) {
                                nAppsDisabled = 0;
                            }
                            if (nWaitingForPlayer < 0) {
                                nWaitingForPlayer = 0;
                            }

                            if (nOptionsOpen == 0 && nAppsDisabled == 0 && Sim.bPause == 0) {
                                SetNetworkBitmap(0);
                            }

                            NetTraceEvent("PLAYERDROP p=%ld options=%ld apps=%ld waiting=%ld", static_cast<long>(c), static_cast<long>(nOptionsOpen),
                                          static_cast<long>(nAppsDisabled), static_cast<long>(nWaitingForPlayer));

                            Sim.Players.Players[c].Owner = 1;
                            Sim.Players.Players[c].NetworkID = 0;
                            Sim.Players.Players[c].GameSpeed = 3;
                            DisplayBroadcastMessage(bprintf(StandardTexte.GetS(TOKEN_MISC, 7002), (LPCTSTR)Sim.Players.Players[c].NameX));
                        }
                    }

                    if (Sim.Players.GetAnzHumanPlayers() == 1) {
                        gNetwork.DisConnect();
                        nOptionsOpen = 0;
                        nAppsDisabled = 0;
                        nWaitingForPlayer = 0;
                        SetNetworkBitmap(0);
                        Sim.bNetwork = 0;
                        return;
                    }
                }
            } break;

            case ATNET_OPTIONS:
                Message >> Par1 >> Par2;
                Par2 = NetCheckPlayerNum(Par2, MessageType);
                nOptionsOpen += Par1;
                nPlayerOptionsOpen[Par2] += Par1;
                NetClampBlockers();
                SetNetworkBitmap(static_cast<SLONG>(nOptionsOpen > 0) * 1);
                break;

            case ATNET_ACTIVATEAPP:
                Message >> Par1 >> Par2;
                Par2 = NetCheckPlayerNum(Par2, MessageType);
                nAppsDisabled += Par1;
                nOptionsOpen += Par1;
                nPlayerOptionsOpen[Par2] += Par1;
                nPlayerAppsDisabled[Par2] += Par1;
                NetClampBlockers();
                SetNetworkBitmap(static_cast<SLONG>(nOptionsOpen > 0) * 2);
                break;

            case ATNET_TIMEPING:
                Message >> Par1;
                gTimerCorrection = Par1 - Sim.TimeSlice;
                break;

            case ATNET_SETGAMESPEED: {
                SLONG PlayerNum = 0;
                SLONG SyncedServerGameSpeed = 0;
                Message >> SyncedServerGameSpeed >> PlayerNum;

                if (Sim.ServerGameSpeed != SyncedServerGameSpeed) {
                    Sim.ServerGameSpeed = SyncedServerGameSpeed;
                    DisplayBroadcastMessage(bprintf("GameSpeed changed to %i / 7\n", static_cast<int>(std::floor(Sim.ServerGameSpeed / 5)) + 1), PlayerNum);
                }
            } break;

            case ATNET_PLAYERPOS: {
                SLONG PlayerNum = 0;
                SLONG MessageTime = 0;
                SLONG LocalTime = 0;

                Message >> PlayerNum;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);
                // if (Sim.Players.Players[PlayerNum].Owner!=1) hprintf ("Received Message ATNET_PLAYERPOS (%li)", PlayerNum);

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];
                PERSON &qPerson = Sim.Persons[Sim.Persons.GetPlayerIndex(PlayerNum)];

                // A position sent before the player got on the phone must not take the phone away:
                bool bTelefoning = (qPlayer.IsTalking != 0) && qPerson.LookDir == 8 && qPerson.Phase >= 4;
                UBYTE TelefoningPhase = qPerson.Phase;

                // Read the message data:
                Message >> qPlayer.PrimaryTarget.x >> qPlayer.PrimaryTarget.y;
                Message >> qPlayer.SecondaryTarget.x >> qPlayer.SecondaryTarget.y;
                Message >> qPlayer.TertiaryTarget.x >> qPlayer.TertiaryTarget.y;
                Message >> qPlayer.DirectToRoom >> qPlayer.iWalkActive;
                Message >> qPlayer.TopLocation >> qPlayer.ExRoom;
                Message >> qPlayer.NewDir >> qPlayer.WalkSpeed;
                Message >> qPerson.Target.x >> qPerson.Target.y;
                Message >> qPerson.Position.x >> qPerson.Position.y;
                Message >> qPerson.ScreenPos.x >> qPerson.ScreenPos.y;
                Message >> qPerson.StatePar >> qPerson.Running;
                Message >> qPerson.Dir >> qPerson.LookDir;
                Message >> qPerson.Phase;

                if (bTelefoning) {
                    qPerson.Dir = 8;
                    qPerson.LookDir = 8;
                    qPerson.Phase = TelefoningPhase;
                }

                qPlayer.UpdateWaypointWalkingDirection();

                /* XY OldPosition = qPerson.Position;

                (Sim.Players.Players[PlayerNum].Owner==1 && PlayerNum==2)
                   {
                   hprintf ("Received Message ATNET_PLAYERPOS (%li) (%li,%li)->(%li,%li) [%li]", PlayerNum, qPerson.Position.x, qPerson.Position.y,
                   qPerson.Target.x, qPerson.Target.y, SLONG(qPlayer.NewDir));

                   if ((OldPosition-qPerson.Position).abs()>100) hprintf ("!!Big Pos Delta. OldPos: (%li,%li) NewPos: (%li,%li)", OldPosition.x, OldPosition.y,
                   qPerson.Position.x, qPerson.Position.y);
                   }*/

                // Message time is different from local time. Adapt data:
                Message >> MessageTime;
                LocalTime = Sim.TimeSlice;

                Sim.TimeSlice = MessageTime;
                /*hprintf ("Diff=%li", LocalTime-Sim.TimeSlice);*/
                if (qPlayer.GetRoom() == ROOM_AIRPORT && (qPlayer.IsTalking == 0) &&
                    (qPlayer.LocationWin == nullptr || ((*qPlayer.LocationWin).CurrentMenu != MENU_WC_F && (*qPlayer.LocationWin).CurrentMenu != MENU_WC_M))) {
                    while (Sim.TimeSlice < LocalTime) {
                        qPerson.DoOnePlayerStep();
                        qPlayer.UpdateWaypointWalkingDirection();
                        Sim.TimeSlice++;
                    }
                }

                Sim.TimeSlice = LocalTime;
            } break;

            case ATNET_ADD_EXPLOSION: {
                SLONG PlayerNum = 0;

                Message >> PlayerNum;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                qPlayer.OfficeState = 2;
                qPlayer.pSmack = new CSmack16;
                qPlayer.pSmack->Open("expl.smk");
                PLAYER::NetSynchronizeFlags();

                gUniversalFx.Stop();
                gUniversalFx.ReInit("explode.raw");
                gUniversalFx.Play(DSBPLAY_NOSTOP, Sim.Options.OptionEffekte * 100 / 7);

                Airport.SetConditionBlock(20 + PlayerNum, 1);
            } break;

            case ATNET_CAFFEINE: {
                SLONG PlayerNum = 0;

                Message >> PlayerNum;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                Message >> Sim.Players.Players[PlayerNum].Koffein;
            } break;

            case ATNET_GIMMICK: {
                SLONG PlayerNum = 0;
                SLONG Mode = 0;

                Message >> PlayerNum >> Mode;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                PERSON &qPerson = Sim.Persons[static_cast<SLONG>(Sim.Persons.GetPlayerIndex(PlayerNum))];

                if (Mode == 1) {
                    qPerson.State = qPerson.State & ~PERSON_WAITFLAG;
                    qPerson.LookDir = 9; // Gimmick starten
                    qPerson.Phase = 0;
                    qPerson.Target = qPerson.Position;

                    Sim.Players.Players[PlayerNum].NewDir = 8;
                    Sim.Players.Players[PlayerNum].StandCount = 0;
                } else {
                    Sim.Players.Players[PlayerNum].StandCount = -100;
                    qPerson.Dir = 8;
                    qPerson.LookDir = 2;
                    qPerson.Phase = 0;
                }
            } break;

            case ATNET_PLAYERLOOK: {
                SLONG PlayerNum = 0;
                SLONG Dir = 0;

                Message >> PlayerNum >> Dir;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                Sim.Persons[static_cast<SLONG>(Sim.Persons.GetPlayerIndex(PlayerNum))].LookAt(Dir);
            } break;

            case ATNET_PLAYERSTOP: {
                SLONG PlayerNum = 0;

                Message >> PlayerNum;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                Sim.Players.Players[PlayerNum].WalkStopEx();
            } break;

            case ATNET_ENTERROOM: {
                SLONG PlayerNum = 0;
                SLONG RoomEntered = 0;

                Message >> PlayerNum;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                Message >> qPlayer.DirectToRoom >> RoomEntered;

                if (RoomEntered != -1) {
                    CTalker *pTalker = nullptr;
                    CTalker *pTalker2 = nullptr;

                    switch (RoomEntered) {
                    case ROOM_AUFSICHT:
                        pTalker = &Talkers.Talkers[TALKER_BOSS];
                        break;
                    case ROOM_ARAB_AIR:
                        pTalker = &Talkers.Talkers[TALKER_ARAB];
                        break;
                    case ROOM_SABOTAGE:
                        pTalker = &Talkers.Talkers[TALKER_SABOTAGE];
                        break;
                    case ROOM_BANK:
                        pTalker = &Talkers.Talkers[TALKER_BANKER1];
                        pTalker2 = &Talkers.Talkers[TALKER_BANKER2];
                        break;
                    case ROOM_MUSEUM:
                        pTalker = &Talkers.Talkers[TALKER_MUSEUM];
                        break;
                    case ROOM_MAKLER:
                        pTalker = &Talkers.Talkers[TALKER_MAKLER];
                        break;
                    case ROOM_WERKSTATT:
                        pTalker = &Talkers.Talkers[TALKER_MECHANIKER];
                        break;
                    case ROOM_WERBUNG:
                        pTalker = &Talkers.Talkers[TALKER_WERBUNG];
                        break;
                    default:
                        break;
                    }

                    if ((pTalker != nullptr) && (pTalker->IsBusy() == 0)) {
                        pTalker->IncreaseLocking();
                    }
                    if ((pTalker2 != nullptr) && (pTalker2->IsBusy() == 0)) {
                        pTalker2->IncreaseLocking();
                    }

                    if (Sim.RoomBusy[RoomEntered] == 0) {
                        Sim.RoomBusy[RoomEntered]++;
                    }
                }

                // if (qPlayer.Owner!=1) hprintf ("Received Message ATNET_ENTERROOM (%li)", PlayerNum);

                for (c = 9; c >= 0; c--) {
                    Message >> qPlayer.Locations[c];
                    // if (qPlayer.Owner!=1) hprintf ("qPlayer.Locations[%li]=%li", c, qPlayer.Locations[c]);
                }

                qPlayer.CalcRoom();
                Sim.UpdateRoomUsage();

                // Bei menschlichen nicht-lokalen Mitspielern eine Fehlerbehandlung:
                if (qPlayer.Owner == 2 && Sim.Time > 9 * 60000) {
                    for (c = 9; c >= 0; c--) {
                        if (qPlayer.GetRoom() == Sim.Players.Players[Sim.localPlayer].Locations[c]) {
                            if (qPlayer.GetRoom() != ROOM_STATISTICS && qPlayer.GetRoom() != ROOM_GLOBE && qPlayer.GetRoom() != ROOM_LAPTOP &&
                                qPlayer.GetRoom() != ROOM_PLANEPROPS && qPlayer.GetRoom() != ROOM_WC_F && qPlayer.GetRoom() != ROOM_WC_M) {
                                SIM::SendSimpleMessage(ATNET_ENTERROOMBAD, qPlayer.NetworkID);
                            }
                        }
                    }
                }
            } break;

            case ATNET_ENTERROOMBAD:
                Sim.Players.Players[Sim.localPlayer].LeaveAllRooms();
                break;

            case ATNET_LEAVEROOM: {
                SLONG PlayerNum = 0;
                SLONG RoomLeft = 0;

                Message >> PlayerNum;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                Message >> qPlayer.DirectToRoom >> RoomLeft;

                if (RoomLeft != -1) {
                    switch (RoomLeft) {
                    case ROOM_AUFSICHT:
                        Talkers.Talkers[TALKER_BOSS].DecreaseLocking();
                        break;
                    case ROOM_ARAB_AIR:
                        Talkers.Talkers[TALKER_ARAB].DecreaseLocking();
                        break;
                    case ROOM_SABOTAGE:
                        Talkers.Talkers[TALKER_SABOTAGE].DecreaseLocking();
                        break;
                    case ROOM_BANK:
                        Talkers.Talkers[TALKER_BANKER1].DecreaseLocking();
                        Talkers.Talkers[TALKER_BANKER2].DecreaseLocking();
                        break;
                    case ROOM_MUSEUM:
                        Talkers.Talkers[TALKER_MUSEUM].DecreaseLocking();
                        break;
                    case ROOM_MAKLER:
                        Talkers.Talkers[TALKER_MAKLER].DecreaseLocking();
                        break;
                    case ROOM_WERKSTATT:
                        Talkers.Talkers[TALKER_MECHANIKER].DecreaseLocking();
                        break;
                    case ROOM_WERBUNG:
                        Talkers.Talkers[TALKER_WERBUNG].DecreaseLocking();
                        break;
                    default:
                        break;
                    }

                    if (Sim.RoomBusy[RoomLeft] != 0U) {
                        Sim.RoomBusy[RoomLeft]--;
                    }
                }
                // if (qPlayer.Owner!=1) hprintf ("Received Message ATNET_LEAVEROOM (%li)", PlayerNum);

                for (c = 9; c >= 0; c--) {
                    Message >> qPlayer.Locations[c];
                    // if (qPlayer.Owner!=1) hprintf ("qPlayer.Locations[%li]=%li", c, qPlayer.Locations[c]);
                }

                qPlayer.CalcRoom();
                Sim.UpdateRoomUsage();
            } break;

            case ATNET_CHEAT: {
                SLONG PlayerNum = 0;
                SLONG Cheat = 0;

                Message >> PlayerNum >> Cheat;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                switch (Cheat) {
                case 0:
                    qPlayer.Money += 10000000;
                    break;
                case 1:
                    qPlayer.Credit = 0;
                    break;
                case 2:
                    qPlayer.Image = 1000;
                    break;
                default:
                    break;
                }
            } break;

            case ATNET_SYNC_IMAGE: {
                SLONG Anz = 0;
                SLONG PlayerNum = 0;

                Message >> Anz;

                while (Anz > 0) {
                    Message >> PlayerNum;
                    PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                    PLAYER &qPlayer = Sim.Players.Players[PlayerNum];
                    SLONG d = 0;

                    Message >> qPlayer.Image >> qPlayer.ImageGotWorse;

                    for (d = 0; d < 4; d++) {
                        Message >> qPlayer.Sympathie[d];
                    }

                    for (d = Routen.AnzEntries() - 1; d >= 0; d--) {
                        Message >> qPlayer.RentRouten.RentRouten[d].Image;
                    }
                    for (d = Cities.AnzEntries() - 1; d >= 0; d--) {
                        Message >> qPlayer.RentCities.RentCities[d].Image;
                    }

                    Anz--;
                }
            } break;

            case ATNET_SYNC_MONEY: {
                SLONG Anz = 0;
                SLONG PlayerNum = 0;

                Message >> Anz;

                while (Anz > 0) {
                    Message >> PlayerNum;
                    PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                    PLAYER &qPlayer = Sim.Players.Players[PlayerNum];
                    SLONG d = 0;

                    Message >> qPlayer.Money >> qPlayer.Credit >> qPlayer.Bonus >> qPlayer.AnzAktien >> qPlayer.MaxAktien >> qPlayer.TrustedDividende >>
                        qPlayer.Dividende;

                    for (d = 0; d < 4; d++) {
                        Message >> qPlayer.OwnsAktien[d] >> qPlayer.AktienWert[d];
                    }
                    for (d = 0; d < 10; d++) {
                        Message >> qPlayer.Kurse[d];
                    }

                    Anz--;
                }
            } break;

            case ATNET_SYNC_ROUTES: {
                SLONG Anz = 0;
                SLONG PlayerNum = 0;

                Message >> Anz;

                while (Anz > 0) {
                    Message >> PlayerNum;
                    PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                    PLAYER &qPlayer = Sim.Players.Players[PlayerNum];
                    SLONG d = 0;

                    for (d = Routen.AnzEntries() - 1; d >= 0; d--) {
                        Message >> qPlayer.RentRouten.RentRouten[d].Rang >> qPlayer.RentRouten.RentRouten[d].LastFlown >>
                            qPlayer.RentRouten.RentRouten[d].Image >> qPlayer.RentRouten.RentRouten[d].Miete >> qPlayer.RentRouten.RentRouten[d].Ticketpreis >>
                            qPlayer.RentRouten.RentRouten[d].TicketpreisFC >> qPlayer.RentRouten.RentRouten[d].TageMitVerlust >>
                            qPlayer.RentRouten.RentRouten[d].TageMitGering;
                    }

                    SLONG Rented = 0;
                    Message >> Rented;

                    while (Rented > 0) {
                        SLONG RouteId = 0;

                        Message >> RouteId;
                        /* RouteId indexes a plain array below, where out of range is not an error
                           but a write into whatever happens to lie there. */
                        if (RouteId < 0 || RouteId >= Routen.AnzEntries()) {
                            NetTraceEvent("DROP name=%s reason=route %ld out of range", Translate_ATNET(MessageType), static_cast<long>(RouteId));
                            Anz = 0; // The rest of the message cannot be read any more either
                            break;
                        }

                        CRentRoute &qRoute = qPlayer.RentRouten.RentRouten[RouteId];
                        Message >> qRoute.Auslastung >> qRoute.AuslastungFC >> qRoute.RoutenAuslastung >> qRoute.HeuteBefoerdert;
                        for (d = 0; d < 7; d++) {
                            Message >> qRoute.WocheBefoerdert[d];
                        }

                        Rented--;
                    }

                    Anz--;
                }
            } break;

            case ATNET_SYNC_FLAGS: {
                SLONG Anz = 0;
                SLONG PlayerNum = 0;

                Message >> Anz;

                while (Anz > 0) {
                    Message >> PlayerNum;
                    PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                    PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                    Message >> qPlayer.SickTokay >> qPlayer.RunningToToilet >> qPlayer.PlayerSmoking >> qPlayer.Stunned >> qPlayer.OfficeState >>
                        qPlayer.Koffein >> qPlayer.NumFlights >> qPlayer.WalkSpeed >> qPlayer.WerbeBroschuere >> qPlayer.TelephoneDown >>
                        qPlayer.Presseerklaerung >> qPlayer.SecurityFlags >> qPlayer.PlayerStinking >> qPlayer.RocketFlags >> qPlayer.LastRocketFlags;
                    /* Whether the laptop has a virus: the floppy disk cures it on its owner's peer
                       only, and the other peers kept the virus for good. */
                    Message >> qPlayer.LaptopVirus;

                    Anz--;
                }
            } break;

            case ATNET_SYNC_OFFICEFLAG: {
                SLONG PlayerNum = 0;

                Message >> PlayerNum;

                /* 55 is not a player: it is how sabotageSecurityOffice() says "the security
                   office is out", so it has to be recognised before the range check drops it.
                   The sabotage also switches off everybody's security measures, but the
                   saboteur's peer can only broadcast the flags of the players it owns - every
                   other peer has to clear its own, or its next flag sync turns them back on. */
                if (PlayerNum == 55) {
                    Message >> Sim.nSecOutDays;

                    for (c = 0; c < 4; c++) {
                        if (Sim.Players.Players[c].IsOut == 0) {
                            Sim.Players.Players[c].SecurityFlags = 0;
                        }
                    }
                } else {
                    PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);
                    PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                    Message >> qPlayer.OfficeState;
                }
            } break;

            case ATNET_SYNC_ITEMS: {
                SLONG Anz = 0;
                SLONG PlayerNum = 0;

                Message >> Anz;

                while (Anz > 0) {
                    Message >> PlayerNum;
                    PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                    PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                    for (SLONG c = 0; c < 6; c++) {
                        Message >> qPlayer.Items[c];
                    }

                    Anz--;
                }
            } break;

            case ATNET_SYNC_PLANES: {
                SLONG Anz = 0;
                SLONG PlayerNum = 0;

                Message >> Anz;

                while (Anz > 0) {
                    Message >> PlayerNum;
                    PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                    PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                    Message >> qPlayer.Planes >> qPlayer.Auftraege >> qPlayer.Frachten >> qPlayer.RentCities;

                    Anz--;
                }
            } break;

            case ATNET_SYNC_MEETING: {
                SLONG Anz = 0;
                SLONG PlayerNum = 0;

                Message >> Anz;

                while (Anz > 0) {
                    Message >> PlayerNum;
                    PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                    PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                    Message >> qPlayer.ArabTrust >> qPlayer.ArabMode >> qPlayer.ArabMode2 >> qPlayer.ArabMode3 >> qPlayer.ArabActive;
                    Message >> qPlayer.ArabOpfer >> qPlayer.ArabOpfer2 >> qPlayer.ArabOpfer3 >> qPlayer.ArabPlane >> qPlayer.ArabHints;
                    Message >> qPlayer.ArabTimeout >> qPlayer.NumPassengers >> qPlayer.NumFracht;

                    Anz--;
                }

                BOOL SentFromHost = 0;
                Message >> SentFromHost;

                if (SentFromHost != 0) {
                    Message >> Sim.SabotageActs;
                }
            } break;

            case ATNET_ADD_SYMPATHIE: {
                SLONG Anz = 0;
                SLONG PlayerNum = 0;
                SLONG SympathieTarget = 0;

                Message >> PlayerNum >> SympathieTarget >> Anz;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);
                SympathieTarget = NetCheckPlayerNum(SympathieTarget, MessageType);

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                qPlayer.Sympathie[SympathieTarget] += Anz;
                Limit(static_cast<SLONG>(-1000), qPlayer.Sympathie[SympathieTarget], static_cast<SLONG>(1000));
            } break;

            case ATNET_SYNCROUTECHANGE: {
                SLONG PlayerNum = 0;
                SLONG RouteId = 0;
                SLONG Ticketpreis = 0;
                SLONG TicketpreisFC = 0;

                Message >> PlayerNum >> RouteId >> Ticketpreis >> TicketpreisFC;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                /* Routen() looks the route up and throws when it does not know it. */
                if (Routen.IsInAlbum(RouteId) == 0) {
                    NetTraceEvent("DROP name=%s reason=route %ld unknown", Translate_ATNET(MessageType), static_cast<long>(RouteId));
                    break;
                }

                if (qPlayer.RentRouten.RentRouten[Routen(RouteId)].Ticketpreis != Ticketpreis) {
                    DebugBreak();
                }
                if (qPlayer.RentRouten.RentRouten[Routen(RouteId)].TicketpreisFC != TicketpreisFC) {
                    DebugBreak();
                }

                qPlayer.UpdateTicketpreise(RouteId, Ticketpreis, TicketpreisFC);
            } break;

                //--------------------------------------------------------------------------------------------
                // Robot:
                //--------------------------------------------------------------------------------------------
            case ATNET_ROBOT_EXECUTE: {
                SLONG c = 0;
                SLONG PlayerNum = 0;

                Message >> PlayerNum;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];
                SLONG WaitWorkTill = 0;

                Message >> WaitWorkTill >> qPlayer.WaitWorkTill2;

                /* The host sends this also after planning, merely to hand over the new action
                   queue, and then its own WaitWorkTill is -1 because it has already executed the
                   action. That must not cancel the execution still pending here: after the day is
                   called off the host waits for our ATNET_READYFORMORNING, which we only send when
                   we execute it, and we wait for the host - the game froze during fast-forward. */
                if (WaitWorkTill != -1 || qPlayer.WaitWorkTill == -1) {
                    qPlayer.WaitWorkTill = WaitWorkTill;
                }

                for (c = 0; c < 4; c++) {
                    Message >> qPlayer.Sympathie[c];
                }
                for (c = 0; c < qPlayer.RobotActions.AnzEntries(); c++) {
                    Message >> qPlayer.RobotActions[c];
                }
                gRobotSyncSlice[PlayerNum] = Sim.TimeSlice;
            } break;

            case ATNET_ROBOT_PHONE: {
                SLONG PlayerNum = 0;
                SLONG Steps = 0;

                Message >> PlayerNum >> Steps;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];
                if (qPlayer.IsMertenBot() && qPlayer.mBot != nullptr && Steps >= 0 && Steps <= 100) {
                    qPlayer.mBot->setOnThePhone(Steps);
                }
            } break;

                //--------------------------------------------------------------------------------------------
                // Player:
                //--------------------------------------------------------------------------------------------
            case ATNET_PLAYER_REFILL: {
                SLONG Type = 0;
                SLONG City = 0;
                SLONG Delta = 0;
                SLONG Time = 0;

                Message >> Type >> City >> Delta >> Time;

                /* City indexes plain vectors below, where out of range is not an error but a
                   write into whatever happens to lie there. */
                if ((Type == 4 || Type == 5) && (City < 0 || City >= SLONG(AuslandsAuftraege.size()) || City >= SLONG(AuslandsRefill.size()))) {
                    NetTraceEvent("DROP name=%s reason=city %ld out of range", Translate_ATNET(MessageType), static_cast<long>(City));
                    break;
                }

                switch (Type) {
                case 1:
                    Sim.TickLastMinuteRefill = Delta;
                    LastMinuteAuftraege.RefillForLastMinute();
                    break;
                case 2:
                    Sim.TickReisebueroRefill = Delta;
                    ReisebueroAuftraege.RefillForReisebuero();
                    break;
                case 3:
                    Sim.TickFrachtRefill = Delta;
                    gFrachten.Refill();
                    break;
                case 4:
                    AuslandsRefill[City] = Delta;
                    AuslandsAuftraege[City].RefillForAusland(City);
                    break;
                case 5:
                    AuslandsFRefill[City] = Delta;
                    AuslandsFrachten[City].RefillForAusland(City);
                    break;
                default:
                    hprintf("AtNet.cpp: Default case should not be reached.");
                    DebugBreak();
                }
            } break;

            case ATNET_PLAYER_TOOK: {
                SLONG Type = 0;
                SLONG City = 0;
                SLONG Index = 0;
                SLONG PlayerNum = 0;

                Message >> PlayerNum >> Type >> Index >> City;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                /* Which order somebody took, by its place in a shared list. If the lists had
                   drifted apart the lookup throws (and ends a release build) or, for the foreign
                   cities, indexes a vector out of range. */
                if ((Type == 4 || Type == 5) && (City < 0 || City >= SLONG(AuslandsAuftraege.size()) || City >= SLONG(AuslandsFrachten.size()))) {
                    NetTraceEvent("DROP name=%s reason=city %ld out of range", Translate_ATNET(MessageType), static_cast<long>(City));
                    break;
                }

                {
                    bool bKnown = false;
                    switch (Type) {
                    case 1:
                        bKnown = (LastMinuteAuftraege.IsInAlbum(Index) != 0);
                        break;
                    case 2:
                        bKnown = (ReisebueroAuftraege.IsInAlbum(Index) != 0);
                        break;
                    case 3:
                        bKnown = (gFrachten.IsInAlbum(Index) != 0);
                        break;
                    case 4:
                        bKnown = (AuslandsAuftraege[City].IsInAlbum(Index) != 0);
                        break;
                    case 5:
                        bKnown = (AuslandsFrachten[City].IsInAlbum(Index) != 0);
                        break;
                    default:
                        break;
                    }

                    if (!bKnown) {
                        NetTraceEvent("DROP name=%s reason=order %ld of type %ld unknown", Translate_ATNET(MessageType), static_cast<long>(Index),
                                      static_cast<long>(Type));
                        break;
                    }
                }

                switch (Type) {
                case 1:
                    LastMinuteAuftraege[Index].Praemie = -1;
                    break;
                case 2:
                    ReisebueroAuftraege[Index].Praemie = -1;
                    break;
                case 3:
                    gFrachten[Index].Praemie = -1;
                    break;
                case 4:
                    AuslandsAuftraege[City][Index].Praemie = -1;
                    break;
                case 5:
                    AuslandsFrachten[City][Index].Praemie = -1;
                    break;
                default:
                    hprintf("AtNet.cpp: Default case should not be reached.");
                    DebugBreak();
                }
            } break;

            case ATNET_PLAYER_18UHR:
                Sim.b18Uhr = TRUE;

                for (c = 0; c < 4; c++) {
                    Sim.Players.Players[c].bReadyForMorning = 0;
                }

                break;

                //--------------------------------------------------------------------------------------------
                // Flugplan:
                //--------------------------------------------------------------------------------------------
            case ATNET_FP_UPDATE: {
                SLONG PlaneId = 0;
                SLONG PlayerNum = 0;

                Message >> PlaneId >> PlayerNum;

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];
                if (qPlayer.Planes.IsInAlbum(PlaneId) == 0) {
                    hprintf("Plane not in Album: %li, %li", PlayerNum, PlaneId);
                }

                CPlane &qPlane = qPlayer.Planes[PlaneId];

                /* Whether a flight has been booked is this peer's own bookkeeping: every peer books
                   the flight itself when the plane takes off on its own clock. The sender's flag
                   says where the sender's clock is. A plan from a peer a minute ahead came with the
                   departure of that minute already booked, and this peer then refused to book it -
                   the premium, the kerosine and the wear of that flight were lost here. From a peer
                   behind, a flight already booked here would be booked a second time. So a flight
                   keeps the flag it has here, and one this peer does not know yet is not booked. */
                std::vector<CFlugplanEintrag> BookedHere;
                for (e = 0; e < qPlane.Flugplan.Flug.AnzEntries(); e++) {
                    if (qPlane.Flugplan.Flug[e].ObjectType != 0 && (qPlane.Flugplan.Flug[e].FlightBooked != 0)) {
                        BookedHere.push_back(qPlane.Flugplan.Flug[e]);
                    }
                }

                Message >> qPlane.Flugplan;
                SLONG PlanDate = 0;
                SLONG PlanHour = 0;
                Message >> PlanDate >> PlanHour;

                for (e = 0; e < qPlane.Flugplan.Flug.AnzEntries(); e++) {
                    CFlugplanEintrag &qFlight = qPlane.Flugplan.Flug[e];
                    if (qFlight.ObjectType == 0) {
                        continue;
                    }
                    const bool bSame = std::any_of(BookedHere.begin(), BookedHere.end(), [&qFlight](const CFlugplanEintrag &qOld) {
                        return qOld.ObjectType == qFlight.ObjectType && qOld.ObjectId == qFlight.ObjectId && qOld.VonCity == qFlight.VonCity &&
                               qOld.NachCity == qFlight.NachCity && qOld.Startdate == qFlight.Startdate && qOld.Startzeit == qFlight.Startzeit;
                    });
                    if ((qFlight.FlightBooked != 0) != bSame) {
                        NetTraceEvent("FLIGHTBOOKED p=%ld plane=%ld flight=%ld theirs=%ld mine=%ld", static_cast<long>(PlayerNum), static_cast<long>(PlaneId),
                                      static_cast<long>(e), static_cast<long>(qFlight.FlightBooked), static_cast<long>(bSame));
                    }
                    qFlight.FlightBooked = bSame ? TRUE : FALSE;
                }

                // Daten aktualisieren
                qPlane.Flugplan.UpdateNextFlight();
                qPlane.Flugplan.UpdateNextStart();

                // Alle Aufträge überprüfen:
                CFlugplan &qPlan = qPlane.Flugplan;

                for (e = qPlan.Flug.AnzEntries() - 1; e >= 0; e--) {
                    if (qPlan.Flug[e].ObjectType == 2) {
                        if (qPlayer.Auftraege.IsInAlbum(qPlan.Flug[e].ObjectId) == 0) {
                            hprintf("Err: Flight %li, %lx", qPlan.Flug[e].ObjectType, qPlan.Flug[e].ObjectId);
                            qPlan.Flug[e].ObjectType = 0;
                        }
                    }
                    if (qPlan.Flug[e].ObjectType == 4) {
                        if (qPlayer.Frachten.IsInAlbum(qPlan.Flug[e].ObjectId) == 0) {
                            hprintf("Err: Flight %li, %lx", qPlan.Flug[e].ObjectType, qPlan.Flug[e].ObjectId);
                            qPlan.Flug[e].ObjectType = 0;
                        }
                    }

                    if (qPlan.Flug[e].ObjectType != 0) {
                        if (Cities.IsInAlbum(qPlan.Flug[e].VonCity) == 0) {
                            hprintf("Err: Flight %li, VonCity %lx", qPlan.Flug[e].ObjectType, qPlan.Flug[e].VonCity);
                            qPlan.Flug[e].ObjectId = 0;
                        }
                        if (Cities.IsInAlbum(qPlan.Flug[e].NachCity) == 0) {
                            hprintf("Err: Flight %li, NachCity %lx", qPlan.Flug[e].ObjectType, qPlan.Flug[e].NachCity);
                            qPlan.Flug[e].ObjectId = 0;
                        }
                    }
                }

                qPlayer.UpdateAuftragsUsage();
                /* How much freight is still open leaves out the flights that have already taken off,
                   judged by the hour. On this peer's clock a plan that arrived just after the change
                   of hour counted one flight less than on the sender's, and the freight contract's
                   open tons and whether it was fully planned differed until the next recount - in a
                   played session for the rest of the day. */
                qPlayer.UpdateFrachtauftragsUsage(PlanDate, PlanHour);

                /* The sender has already checked the plan (CPlane::CheckFlugplaene) and sends the
                   result; checking it again here did it on this peer's clock. That is not a no-op:
                   automatic flights are placed relative to the current hour. A bot's plan the
                   host sent unchanged because only a ticket price had changed was rearranged an
                   hour later on the client, and the two peers flew different plans for the rest of
                   the day. Only the gates are worked out again, since the sender may have moved
                   other planes' gates without sending those planes. */
                qPlayer.PlanGates();
            } break;

            case ATNET_TAKE_ORDER: {
                SLONG PlayerNum = 0;
                CAuftrag a;

                Message >> PlayerNum >> a;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                if (qPlayer.Auftraege.GetNumFree() < 3) {
                    qPlayer.Auftraege.ReSize(qPlayer.Auftraege.AnzEntries() + 10);
                }

                qPlayer.Auftraege += a;
            } break;

            case ATNET_TAKE_FREIGHT: {
                SLONG PlayerNum = 0;
                CFracht a;

                Message >> PlayerNum >> a;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                if (qPlayer.Frachten.GetNumFree() < 3) {
                    qPlayer.Frachten.ReSize(qPlayer.Frachten.AnzEntries() + 10);
                }

                qPlayer.Frachten += a;
            } break;

            case ATNET_BID: {
                SLONG Type = 0;
                SLONG Slot = 0;
                SLONG PlayerNum = 0;
                SLONG Preis = 0;

                Message >> Type >> Slot >> PlayerNum >> Preis;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                if ((Type != CTafelZettel::Type::CITY && Type != CTafelZettel::Type::GATE) || Slot < 0 || Slot >= 7) {
                    NetTraceEvent("BID out of range type=%ld slot=%ld", static_cast<long>(Type), static_cast<long>(Slot));
                    break;
                }

                CTafelZettel &qNote = (Type == CTafelZettel::Type::CITY) ? TafelData.City[Slot] : TafelData.Gate[Slot];

                /* Every bid raises the note's price by a tenth, so the higher price is the later
                   bid, and two peers that bid in the same moment send the same price. Taking the
                   higher price, and on equal prices the lower player number, leaves every peer
                   with the same holder whichever order the bids arrive in - which a whole-board
                   snapshot did not. */
                if (Preis > qNote.Preis || (Preis == qNote.Preis && PlayerNum < qNote.Player)) {
                    qNote.Preis = Preis;
                    qNote.Player = PlayerNum;
                }
            } break;

            case ATNET_KILL_CITY: {
                SLONG PlayerNum = 0;
                SLONG CityId = 0;

                Message >> PlayerNum >> CityId;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                GameMechanic::killCity(Sim.Players.Players[PlayerNum], CityId, true);
            } break;

            case ATNET_TAKE_ROUTE: {
                SLONG PlayerNum = 0;
                SLONG Route1Id = 0;
                SLONG Route2Id = 0;

                Message >> PlayerNum >> Route1Id >> Route2Id;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                /* Both index a plain vector, where out of range writes over whatever lies
                   there - and a whole CRentRoute is written, not a single field. */
                if (Route1Id < 0 || Route1Id >= qPlayer.RentRouten.RentRouten.AnzEntries() || Route2Id < 0 ||
                    Route2Id >= qPlayer.RentRouten.RentRouten.AnzEntries()) {
                    NetTraceEvent("DROP name=%s reason=routes %ld and %ld out of range", Translate_ATNET(MessageType), static_cast<long>(Route1Id),
                                  static_cast<long>(Route2Id));
                    break;
                }

                Message >> qPlayer.RentRouten.RentRouten[Route1Id];
                Message >> qPlayer.RentRouten.RentRouten[Route2Id];
            } break;

            case ATNET_ADVISOR: {
                SLONG Art = 0;
                SLONG From = 0;
                SLONG Generic1 = 0;
                PLAYER &qPlayer = Sim.Players.Players[Sim.localPlayer];
                TEAKRAND rnd;

                Message >> Art >> From >> Generic1;
                From = NetCheckPlayerNum(From, MessageType);

                PLAYER &qFromPlayer = Sim.Players.Players[From];

                switch (Art) {
                // Tafel: Jemand hat einen überboten
                case 0:
                    /* The note on the board, by its place on it: a board that had drifted apart
                       would index past the end here, and the lookup throws rather than returns. */
                    if (Generic1 < 0 || Generic1 >= SLONG(TafelData.ByPositions.size())) {
                        NetTraceEvent("ADVISOR out of range art=%ld note=%ld", static_cast<long>(Art), static_cast<long>(Generic1));
                        break;
                    }
                    if (qPlayer.HasBerater(BERATERTYP_INFO) >= rnd.Rand(100)) {
                        auto &qEntry = *TafelData.ByPositions[Generic1];
                        if (qEntry.Type == CTafelZettel::Type::GATE) {
                            qPlayer.Messages.AddMessage(
                                BERATERTYP_INFO, bprintf(StandardTexte.GetS(TOKEN_ADVICE, 9001), (LPCTSTR)qFromPlayer.NameX, (LPCTSTR)qFromPlayer.AirlineX));
                        } else if (qEntry.Type == CTafelZettel::Type::CITY) {
                            qPlayer.Messages.AddMessage(BERATERTYP_INFO, bprintf(StandardTexte.GetS(TOKEN_ADVICE, 9002), (LPCTSTR)qFromPlayer.NameX,
                                                                                 (LPCTSTR)qFromPlayer.AirlineX, (LPCTSTR)Cities[qEntry.ZettelId].Name));
                        }
                    }
                    break;

                    // Jemand kauft gebrauchtes Flugzeug:
                case 1:
                    /* The used plane somebody bought, by its place in the museum's list. */
                    if (Sim.UsedPlanes.IsInAlbum(Generic1) == 0) {
                        NetTraceEvent("ADVISOR out of range art=%ld usedplane=%ld", static_cast<long>(Art), static_cast<long>(Generic1));
                        break;
                    }
                    if (qPlayer.HasBerater(BERATERTYP_INFO) >= rnd.Rand(100)) {
                        qPlayer.Messages.AddMessage(BERATERTYP_INFO, bprintf(StandardTexte.GetS(TOKEN_ADVICE, 9000), (LPCTSTR)qFromPlayer.NameX,
                                                                             (LPCTSTR)qFromPlayer.AirlineX, Sim.UsedPlanes[Generic1].CalculatePrice()));
                    }
                    break;

                    // Jemand gibt Aktien aus:
                case 3:
                    if (qPlayer.HasBerater(BERATERTYP_INFO) >= rnd.Rand(100)) {
                        qPlayer.Messages.AddMessage(BERATERTYP_INFO, bprintf(StandardTexte.GetS(TOKEN_ADVICE, 9004), (LPCTSTR)qFromPlayer.NameX,
                                                                             (LPCTSTR)qFromPlayer.AirlineX, Generic1));
                    }
                    break;

                    // Jemand kauft Aktien vom localPlayer:
                case 4:
                    if (qPlayer.HasBerater(BERATERTYP_INFO) >= rnd.Rand(100)) {
                        qPlayer.Messages.AddMessage(BERATERTYP_INFO, bprintf(StandardTexte.GetS(TOKEN_ADVICE, 9005), (LPCTSTR)qFromPlayer.NameX,
                                                                             (LPCTSTR)qFromPlayer.AirlineX, Generic1));
                    }
                    break;
                default:
                    hprintf("AtNet.cpp: Default case should not be reached.");
                    DebugBreak();
                }
            } break;

            case ATNET_BUY_USED: {
                SLONG PlayerNum = 0;
                SLONG PlaneIndex = 0;
                SLONG Time = 0;

                Message >> PlayerNum >> PlaneIndex >> Time;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                PLAYER &qFromPlayer = Sim.Players.Players[PlayerNum];

                /* Which plane in the museum, by its place in the list: the lookup throws when
                   this peer does not have it. */
                if (Sim.UsedPlanes.IsInAlbum(PlaneIndex) == 0) {
                    NetTraceEvent("DROP name=%s reason=used plane %ld unknown", Translate_ATNET(MessageType), static_cast<long>(PlaneIndex));
                    break;
                }

                if (qFromPlayer.Planes.GetNumFree() < 2) {
                    qFromPlayer.Planes.ReSize(qFromPlayer.Planes.AnzEntries() + 10);
                }

                /* Prepare the plane exactly as the buyer did (GameMechanic::buyUsedPlane) before
                   taking it over. WorstZustand decides the nightly repair bill, so a copy that
                   skipped this charged the new owner differently on every other peer. */
                Sim.UsedPlanes[PlaneIndex].WorstZustand = static_cast<UBYTE>(Sim.UsedPlanes[PlaneIndex].Zustand - 20);
                Sim.UsedPlanes[PlaneIndex].GlobeAngle = 0;
                qFromPlayer.Planes += Sim.UsedPlanes[PlaneIndex];

                Sim.UsedPlanes[PlaneIndex].Name.Empty();
                Sim.TickMuseumRefill = 0;
            } break;

            case ATNET_SELL_USED: {
                SLONG PlayerNum = 0;
                SLONG PlaneId = 0;

                Message >> PlayerNum >> PlaneId;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                if (qPlayer.Planes.IsInAlbum(PlaneId) == 0) {
                    NetTraceEvent("DROP name=%s reason=plane %ld unknown", Translate_ATNET(MessageType), static_cast<long>(PlaneId));
                    break;
                }

                qPlayer.Planes -= PlaneId;
            } break;

            case ATNET_BUY_NEW: {
                SLONG PlayerNum = 0;
                SLONG Anzahl = 0;
                SLONG Type = 0;
                TEAKRAND rnd;

                Message >> PlayerNum >> Anzahl >> Type;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                /* PLAYER::BuyPlane() looks the type up in an album, and an unknown key throws
                   instead of returning - which ends a release build through the handler in
                   main(). Peers with different plane data would take each other down. */
                if (PlaneTypes.IsInAlbum(Type + 0x10000000) == 0) {
                    NetTraceEvent("DROP name=%s reason=unknown plane type %ld", Translate_ATNET(MessageType), static_cast<long>(Type));
                    break;
                }

                rnd.SRand(Sim.Date);

                for (c = 0; c < Anzahl; c++) {
                    qPlayer.BuyPlane(Type, &rnd);
                }
            } break;

            case ATNET_BUY_NEWX: {
                SLONG PlayerNum = 0;
                SLONG Anzahl = 0;
                CXPlane plane;
                TEAKRAND rnd;

                Message >> PlayerNum >> Anzahl >> plane;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                rnd.SRand(Sim.Date);

                for (c = 0; c < Anzahl; c++) {
                    qPlayer.BuyPlane(plane, &rnd);
                }
            } break;

            case ATNET_PERSONNEL: {
                SLONG PlayerNum = 0;
                SLONG m = 0;
                SLONG n = 0;

                Message >> PlayerNum >> m >> n;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                if (PlayerNum >= 0 && PlayerNum < Sim.Players.Players.AnzEntries()) {
                    PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                    qPlayer.Statistiken[STAT_ZUFR_PERSONAL].SetAtPastDay(m);
                    qPlayer.Statistiken[STAT_MITARBEITER].SetAtPastDay(n);

                    SLONG c = 0;
                    while (true) {
                        Message >> c;

                        if (c == -1) {
                            break;
                        }
                        if (qPlayer.Planes.IsInAlbum(c) != 0) {
                            Message >> qPlayer.Planes[c].AnzPiloten;
                            Message >> qPlayer.Planes[c].AnzBegleiter;
                            Message >> qPlayer.Planes[c].PersonalQuality;
                        } else {
                            SLONG dummy = 0;

                            Message >> dummy >> dummy >> dummy;
                        }
                    }
                }
            } break;

            case ATNET_PLANEPROPS: {
                SLONG PlayerNum = 0;
                SLONG PlaneId = 0;

                Message >> PlayerNum >> PlaneId;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);
                if (PlayerNum > 4) {
                    break;
                }
                Message >> Sim.Players.Players[PlayerNum].MechMode;

                if (PlaneId != -1 && (Sim.Players.Players[PlayerNum].Planes.IsInAlbum(PlaneId) != 0)) {
                    CPlane &qPlane = Sim.Players.Players[PlayerNum].Planes[PlaneId];

                    Message >> qPlane.Sitze >> qPlane.SitzeTarget >> qPlane.Essen >> qPlane.EssenTarget >> qPlane.Tabletts >> qPlane.TablettsTarget >>
                        qPlane.Deco >> qPlane.DecoTarget >> qPlane.Triebwerk >> qPlane.TriebwerkTarget >> qPlane.Reifen >> qPlane.ReifenTarget >>
                        qPlane.Elektronik >> qPlane.ElektronikTarget >> qPlane.Sicherheit >> qPlane.SicherheitTarget >> qPlane.MaxPassagiereTarget >>
                        qPlane.MaxPassagiereTargetFC;

                    Message >> qPlane.WorstZustand >> qPlane.Zustand >> qPlane.TargetZustand;
                    Message >> qPlane.AnzBegleiter >> qPlane.MaxBegleiter;
                }
            } break;

                //--------------------------------------------------------------------------------------------
                // Dialog:
                //--------------------------------------------------------------------------------------------
            case ATNET_DIALOG_REQUEST: {
                PLAYER &qPlayer = Sim.Players.Players[Sim.localPlayer];
                auto *pRaum = qPlayer.LocationWin;

                SLONG RequestingPlayer = 0;

                // Ist Spieler bereit, einen Dialog zu beginnen?
                if (qPlayer.GetRoom() == ROOM_AIRPORT && (qPlayer.IsStuck == 0) && (pRaum != nullptr) && pRaum->MenuIsOpen() == FALSE &&
                    pRaum->IsDialogOpen() == FALSE) {
                    PERSON &qPerson = Sim.Persons[static_cast<SLONG>(Sim.Persons.GetPlayerIndex(Sim.localPlayer))];

                    qPlayer.WalkStopEx();
                    qPlayer.IsTalking = TRUE;

                    Message >> qPerson.Phase >> RequestingPlayer >> qPerson.Position.x >> qPerson.Position.y;
                    RequestingPlayer = NetCheckPlayerNum(RequestingPlayer, MessageType);

                    qPerson.Dir = 8;
                    qPerson.LookDir = 8;

                    SIM::SendSimpleMessage(ATNET_PLAYERLOOK, Sim.Players.Players[RequestingPlayer].NetworkID, qPerson.State, qPerson.Phase);
                    SIM::SendSimpleMessage(ATNET_DIALOG_YES, Sim.Players.Players[RequestingPlayer].NetworkID, Sim.localPlayer, qPerson.Phase);
                } else {
                    UBYTE Dummy = 0;
                    XY Dummy2;

                    Message >> Dummy >> RequestingPlayer >> Dummy2.x >> Dummy2.y;
                    RequestingPlayer = NetCheckPlayerNum(RequestingPlayer, MessageType);

                    // Nein! Keine Interviews!
                    SIM::SendSimpleMessage(ATNET_DIALOG_NO, Sim.Players.Players[RequestingPlayer].NetworkID);
                }
            } break;

            case ATNET_DIALOG_YES: {
                PLAYER &qPlayer = Sim.Players.Players[Sim.localPlayer];
                SLONG TargetPlayer = 0;
                SLONG Phase = 0;

                Message >> TargetPlayer >> Phase;
                TargetPlayer = NetCheckPlayerNum(TargetPlayer, MessageType);

                Sim.Persons[static_cast<SLONG>(Sim.Persons.GetPlayerIndex(TargetPlayer))].Phase = UBYTE(Phase);

                if (!qPlayer.bDialogStartSent) {
                    qPlayer.IsWalking2Player = TargetPlayer;
                }
            } break;

            case ATNET_DIALOG_START: {
                PLAYER &qPlayer = Sim.Players.Players[Sim.localPlayer];
                SLONG OtherPlayerNum = 0;

                auto *pRaum = qPlayer.LocationWin;

                Message >> OtherPlayerNum;
                OtherPlayerNum = NetCheckPlayerNum(OtherPlayerNum, MessageType);

                // Erneute Abfrage: Ist Spieler bereit, einen Dialog zu beginnen?
                if (qPlayer.GetRoom() == ROOM_AIRPORT && (qPlayer.IsStuck == 0) && (pRaum != nullptr) && pRaum->MenuIsOpen() == FALSE &&
                    pRaum->IsDialogOpen() == FALSE) {
                    // JA!
                    PERSON &qPerson = Sim.Persons[Sim.Persons.GetPlayerIndex(OtherPlayerNum)];

                    Message >> qPerson.Position.x >> qPerson.Position.y >> qPerson.Phase >> qPerson.LookDir;

                    if (qPlayer.LocationWin != nullptr) {
                        (qPlayer.LocationWin)->StartDialog(TALKER_COMPETITOR, MEDIUM_AIR, OtherPlayerNum, 1);
                    }

                    qPlayer.PlayerDialogState = -1;
                } else {
                    UBYTE DummyPhase = 0;
                    UBYTE DummyLookDir = 0;
                    SLONG DummyX = 0;
                    SLONG DummyY = 0;

                    Message >> DummyX >> DummyY >> DummyPhase >> DummyLookDir;

                    // Nein! Keine Interviews!
                    SIM::SendSimpleMessage(ATNET_DIALOG_NO, Sim.Players.Players[OtherPlayerNum].NetworkID);
                }
            } break;

            case ATNET_DIALOG_NO: {
                PLAYER &qPlayer = Sim.Players.Players[Sim.localPlayer];

                qPlayer.IsWalking2Player = -1;
                qPlayer.IsTalking = 0;

                qPlayer.PlayerDialogState = -1;
                qPlayer.WalkStop();
                qPlayer.NewDir = 8;

                if (qPlayer.LocationWin != nullptr) {
                    (qPlayer.LocationWin)->StopDialog();
                }
            } break;

            case ATNET_DIALOG_SAY: {
                SLONG id = 0;
                PLAYER &qPlayer = Sim.Players.Players[Sim.localPlayer];

                Message >> id;

                MouseClickArea = -102;
                MouseClickId = 1;
                MouseClickPar1 = id;

                if (qPlayer.LocationWin != nullptr) {
                    (qPlayer.LocationWin)->PreLButtonDown(CPoint(0, 0));
                }
            } break;

            case ATNET_DIALOG_TEXT: {
                PLAYER &qPlayer = Sim.Players.Players[Sim.localPlayer];
                CString Answer;
                SLONG id = 0;
                BOOL TextAlign = 0;

                Message >> TextAlign >> id >> Answer;

                if (qPlayer.LocationWin != nullptr) {
                    (qPlayer.LocationWin)->MakeSayWindow(static_cast<BOOL>(TextAlign == 0), id, Answer, (qPlayer.LocationWin)->pFontNormal);
                }
            } break;

            case ATNET_DIALOG_NEXT: {
                PLAYER &qPlayer = Sim.Players.Players[Sim.localPlayer];

                if (qPlayer.LocationWin != nullptr) {
                    (qPlayer.LocationWin)->PreLButtonDown(CPoint(0, 0));
                }
            } break;

            case ATNET_DIALOG_DRUNK:
                Sim.Players.Players[Sim.localPlayer].IsDrunk += 400;
                break;

            case ATNET_DIALOG_LOCK: {
                SLONG PlayerNum = 0;

                Message >> PlayerNum;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];
                qPlayer.IsTalking = TRUE;
            } break;

            case ATNET_DIALOG_UNLOCK: {
                SLONG PlayerNum = 0;

                Message >> PlayerNum;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];
                qPlayer.IsTalking = FALSE;

                // Aus irgendeinem Grund fängt die Spielfigur sonst an zu laufen:
                if (qPlayer.Owner == 2) {
                    qPlayer.WalkStopEx();
                }
            } break;

            case ATNET_DIALOG_END: {
                PLAYER &qPlayer = Sim.Players.Players[Sim.localPlayer];

                if (qPlayer.LocationWin != nullptr) {
                    (qPlayer.LocationWin)->StopDialog();
                }

                qPlayer.PlayerDialogState = -1;
            } break;

            case ATNET_DIALOG_KOOP: {
                SLONG PlayerNum = 0;

                Message >> PlayerNum;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                Message >> qPlayer.Kooperation;
            } break;

            case ATNET_DIALOG_DROPITEM: {
                SLONG PlayerNum = 0;
                SLONG Item = 0;

                Message >> PlayerNum >> Item;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                Sim.Players.Players[PlayerNum].DropItem(UBYTE(Item));
            } break;

                //--------------------------------------------------------------------------------------------
                // Dialogaufbau per Telefon:
                //--------------------------------------------------------------------------------------------
            case ATNET_PHONE_DIAL: {
                PLAYER &qPlayer = Sim.Players.Players[Sim.localPlayer];
                SLONG OtherPlayerNum = 0;
                SLONG bHandy = 0;

                Message >> OtherPlayerNum >> bHandy;
                OtherPlayerNum = NetCheckPlayerNum(OtherPlayerNum, MessageType);

                if (qPlayer.LocationWin != nullptr) {
                    CStdRaum &qRoom = *(qPlayer.LocationWin);

                    bool bImpossible = false; // Kein Telefonat annehmen, wenn wir gerade den Höhrer in die Hand nehmen:
                    if (qPlayer.GetRoom() == ROOM_BURO_A + Sim.localPlayer * 10 && ((dynamic_cast<CBuero *>(qPlayer.LocationWin))->KommVarTelefon != 0)) {
                        bImpossible = true;
                    }

                    if (qRoom.IsDialogOpen() == 0 && qRoom.MenuIsOpen() == 0 && !bImpossible &&
                        Sim.Persons[Sim.Persons.GetPlayerIndex(Sim.localPlayer)].StatePar == 0 && qPlayer.TelephoneDown == FALSE) {
                        if (bHandy == 0 && qPlayer.GetRoom() != ROOM_BURO_A + Sim.localPlayer * 10) {
                            SIM::SendSimpleMessage(ATNET_PHONE_NOTHOME, Sim.Players.Players[OtherPlayerNum].NetworkID);
                        } else {
                            SIM::SendSimpleMessage(ATNET_PHONE_ACCEPT, Sim.Players.Players[OtherPlayerNum].NetworkID, Sim.localPlayer, bHandy);

                            gUniversalFx.Stop();
                            gUniversalFx.ReInit("phone.raw");
                            gUniversalFx.Play(DSBPLAY_NOSTOP, Sim.Options.OptionEffekte * 100 / 7);

                            qPlayer.GameSpeed = 0;
                            SIM::SendSimpleMessage(ATNET_SETSPEED, 0, Sim.localPlayer, qPlayer.GameSpeed);

                            qRoom.StartDialog(TALKER_COMPETITOR, MEDIUM_HANDY, OtherPlayerNum, 1);
                            qRoom.PayingForCall = FALSE;

                            qPlayer.DisplayAsTelefoning();
                            Sim.Players.Players[OtherPlayerNum].DisplayAsTelefoning();
                        }
                    } else {
                        SIM::SendSimpleMessage(ATNET_PHONE_BUSY, Sim.Players.Players[OtherPlayerNum].NetworkID);
                    }
                } else {
                    SIM::SendSimpleMessage(ATNET_PHONE_BUSY, Sim.Players.Players[OtherPlayerNum].NetworkID);
                }
            } break;

            case ATNET_PHONE_ACCEPT: {
                PLAYER &qPlayer = Sim.Players.Players[Sim.localPlayer];
                SLONG OtherPlayerNum = 0;
                SLONG bHandy = 0;

                Message >> OtherPlayerNum >> bHandy;
                OtherPlayerNum = NetCheckPlayerNum(OtherPlayerNum, MessageType);

                if (qPlayer.LocationWin != nullptr) {
                    (qPlayer.LocationWin)->StartDialog(TALKER_COMPETITOR, MEDIUM_HANDY, OtherPlayerNum, 0);
                }

                qPlayer.GameSpeed = 0;
                SIM::SendSimpleMessage(ATNET_SETSPEED, 0, Sim.localPlayer, qPlayer.GameSpeed);

                qPlayer.DisplayAsTelefoning();
                Sim.Players.Players[OtherPlayerNum].DisplayAsTelefoning();
            } break;

            case ATNET_PHONE_BUSY: {
                PLAYER &qPlayer = Sim.Players.Players[Sim.localPlayer];

                if (qPlayer.LocationWin != nullptr) {
                    CStdRaum &qRoom = *(qPlayer.LocationWin);

                    qRoom.DialBusyFX.ReInit("busypure.raw"); // busy pure (without dialing first)
                    qRoom.DialBusyFX.Play(0, Sim.Options.OptionEffekte * 100 / 7);

                    if (qPlayer.GetRoom() == ROOM_BURO_A + Sim.localPlayer * 10) {
                        (dynamic_cast<CBuero *>(&qRoom))->SP_Player.SetDesiredMood(SPM_IDLE);
                    }
                }
            } break;

            case ATNET_PHONE_NOTHOME: {
                PLAYER &qPlayer = Sim.Players.Players[Sim.localPlayer];

                if (qPlayer.LocationWin != nullptr) {
                    CStdRaum &qRoom = *(qPlayer.LocationWin);

                    qRoom.DialBusyFX.ReInit("noanpure.raw"); // No Answer pure (without dialing first)
                    qRoom.DialBusyFX.Play(0, Sim.Options.OptionEffekte * 100 / 7);

                    if (qPlayer.GetRoom() == ROOM_BURO_A + Sim.localPlayer * 10) {
                        (dynamic_cast<CBuero *>(&qRoom))->SP_Player.SetDesiredMood(SPM_IDLE);
                    }
                }
            } break;

                // case ATNET_CHATSTART: //Veraltet, wird nicht mehr gebraucht
                //   break;

                //--------------------------------------------------------------------------------------------
                // Chatten:
                //--------------------------------------------------------------------------------------------
            case ATNET_CHATSTOP: {
                PLAYER &qPlayer = Sim.Players.Players[Sim.localPlayer];

                if (qPlayer.LocationWin != nullptr) {
                    (qPlayer.LocationWin)->MenuStop();
                }
            } break;

            case ATNET_CHATMESSAGE: {
                CString str;
                PLAYER &qPlayer = Sim.Players.Players[Sim.localPlayer];

                Message >> str;

                if (qPlayer.LocationWin != nullptr) {
                    (qPlayer.LocationWin)->MenuBms[1].ShiftUp(10);
                    (qPlayer.LocationWin)->MenuBms[1].PrintAt(str, FontSmallRed, TEC_FONT_LEFT, 6, 119, 279, 147);
                    (qPlayer.LocationWin)->MenuRepaint();
                }
            } break;

            case ATNET_CHATMONEY: {
                SLONG Money = 0;
                SLONG OtherPlayer = 0;
                PLAYER &qPlayer = Sim.Players.Players[Sim.localPlayer];

                Message >> Money >> OtherPlayer;

                Sim.Players.Players[Sim.localPlayer].ChangeMoney(Money, 3700, Sim.Players.Players[OtherPlayer].NameX);
                Sim.Players.Players[OtherPlayer].ChangeMoney(-Money, 3701, Sim.Players.Players[Sim.localPlayer].NameX);

                if (qPlayer.LocationWin != nullptr) {
                    (qPlayer.LocationWin)->MenuBms[1].ShiftUp(10);
                    (qPlayer.LocationWin)
                        ->MenuBms[1]
                        .PrintAt(bprintf(StandardTexte.GetS(TOKEN_MISC, 3010), Money), FontNormalGrey, TEC_FONT_LEFT, 6, 119, 279, 147);
                    (qPlayer.LocationWin)->MenuRepaint();
                }
            } break;

                //--------------------------------------------------------------------------------------------
                // Sabotage:
                //--------------------------------------------------------------------------------------------
            case ATNET_SABOTAGE_DIRECT: {
                SLONG Type = 0;
                XY Position;

                Message >> Type;
                Message >> Position.x >> Position.y;

                if (Type == ITEM_STINKBOMBE) // Stinkbombe
                {
                    Sim.AddStenchSabotage(XY(Position.x, Position.y));
                } else if (Type == ITEM_GLUE) // Klebstoff
                {
                    UBYTE Dir = 0;
                    UBYTE NewDir = 0;
                    UBYTE Phase = 0;

                    Message >> Dir >> NewDir >> Phase;

                    Sim.AddGlueSabotage(Position, Dir, NewDir, Phase);
                }
            } break;

            case ATNET_SABOTAGE_ARAB: {
                SLONG PlayerNum = 0;

                Message >> PlayerNum;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                Message >> qPlayer.ArabOpfer >> qPlayer.ArabMode >> qPlayer.ArabActive >> qPlayer.ArabPlane >> qPlayer.ArabOpfer2 >> qPlayer.ArabMode2 >>
                    qPlayer.ArabOpfer3 >> qPlayer.ArabMode3 >> qPlayer.ArabTimeout;
            } break;

            case ATNET_WAITFORPLAYER:
                Message >> Par1 >> Par2;
                Par2 = NetCheckPlayerNum(Par2, MessageType);
                nWaitingForPlayer += Par1;
                /* The clock only runs while this is exactly zero (CTakeOffApp::GameLoop), so a
                   negative count stops the game just as a positive one does. A peer that
                   reports "I am done waiting" twice, or once without this peer having counted
                   its wait, used to leave everybody at -1 with the clock stopped for good. */
                if (nWaitingForPlayer < 0) {
                    nWaitingForPlayer = 0;
                }
                NetTraceEvent("WAITFORPLAYER p=%ld delta=%ld waiting=%ld", static_cast<long>(Par2), static_cast<long>(Par1),
                              static_cast<long>(nWaitingForPlayer));
                nPlayerWaiting[Par2] += Par1;
                if (nPlayerWaiting[Par2] < 0) {
                    nPlayerWaiting[Par2] = 0;
                }
                SetNetworkBitmap(static_cast<SLONG>(nWaitingForPlayer > 0) * 3);
                break;

            case ATNET_TAKETHING: {
                SLONG Item = 0;

                Message >> Item;

                switch (Item) {
                case ITEM_POSTKARTE:
                    Sim.ItemPostcard = 0;
                    break;
                case ITEM_PAPERCLIP:
                    Sim.ItemClips = 0;
                    break;
                case ITEM_GLUE:
                    Sim.ItemGlue = 2;
                    break;
                case ITEM_GLOVE:
                    Sim.ItemGlove = 0;
                    break;
                case ITEM_LAPTOP:
                    Message >> Sim.LaptopSoldTo;
                    break;
                case ITEM_NONE:
                    Sim.MoneyInBankTrash = 0;
                    break;
                case ITEM_KOHLE:
                    Sim.ItemKohle = 0;
                    break;
                case ITEM_PARFUEM:
                    Sim.ItemParfuem = 0;
                    break;
                case ITEM_ZANGE:
                    Sim.ItemZange = 0;
                    break;
                default:
                    break; /* some items do not require network sync */
                }
            } break;

                //--------------------------------------------------------------------------------------------
                // Day control:
                //--------------------------------------------------------------------------------------------
            case ATNET_DAYFINISH: {
                SLONG FromPlayer = 0;

                Message >> FromPlayer;
                FromPlayer = NetCheckPlayerNum(FromPlayer, MessageType);
                NetTraceEvent("DAYFINISH from=%ld", static_cast<long>(FromPlayer));

                Sim.Players.Players[FromPlayer].CallItADay = TRUE;
            } break;
            case ATNET_DAYBACK:
                if (Sim.CallItADayAt == 0) {
                    SLONG FromPlayer = 0;

                    Message >> FromPlayer;
                    FromPlayer = NetCheckPlayerNum(FromPlayer, MessageType);

                    Sim.Players.Players[FromPlayer].CallItADay = FALSE;
                }
                break;
            case ATNET_DAYFINISHALL: {
                PLAYER &qPlayer = Sim.Players.Players[Sim.localPlayer];
                NetTraceEvent("DAYFINISHALL");

                if ((Sim.Options.OptionAutosave != 0) && (Sim.bNetwork != 0)) {
                    Sim.SaveGame(11, StandardTexte.GetS(TOKEN_MISC, 5000));
                }

                Message >> Sim.CallItADayAt;

                for (SLONG c = 0; c < 4; c++) {
                    if (Sim.Players.Players[c].Owner == 2) {
                        Sim.Players.Players[c].CallItADay = TRUE;
                    }
                }

                if (qPlayer.CallItADay == 0) {
                    SIM::SendSimpleMessage(ATNET_DAYFINISH, 0, Sim.localPlayer);
                    SIM::SendSimpleMessage(ATNET_DAYFINISH, qPlayer.NetworkID, Sim.localPlayer);
                }
            } break;

                //--------------------------------------------------------------------------------------------
                // Miscellaneous:
                //--------------------------------------------------------------------------------------------
            case ATNET_EXPAND_AIRPORT:
                Sim.ExpandAirport = 1;
                break;

            case ATNET_OVERTAKE:
                Message >> Sim.OvertakenAirline >> Sim.OvertakerAirline >> Sim.Overtake;
                break;

                //--------------------------------------------------------------------------------------------
                // Savegames:
                //--------------------------------------------------------------------------------------------
            case ATNET_IO_SAVE: {
                SLONG CursorY = 0;
                CString Name;

                Message >> Sim.UniqueGameId >> CursorY >> Name;

                Sim.SaveGame(CursorY, Name);
            } break;

            case ATNET_IO_LOADREQUEST: {
                SLONG Index = 0;
                SLONG FromPlayer = 0;
                DWORD UniqueGameId = 0;

                Message >> FromPlayer >> Index >> UniqueGameId;
                FromPlayer = NetCheckPlayerNum(FromPlayer, MessageType);

                if (Sim.GetSavegameUniqueGameId(Index, true) == UniqueGameId) {
                    SIM::SendSimpleMessage(ATNET_IO_LOADREQUEST_OK, Sim.Players.Players[FromPlayer].NetworkID, Sim.localPlayer, Index);
                } else {
                    SIM::SendSimpleMessage(ATNET_IO_LOADREQUEST_BAD, Sim.Players.Players[FromPlayer].NetworkID, Sim.localPlayer);

                    if (Sim.Players.Players[Sim.localPlayer].LocationWin != nullptr) {
                        (Sim.Players.Players[Sim.localPlayer].LocationWin)->MenuStart(MENU_REQUEST, MENU_REQUEST_NET_LOADTHIS);
                    }
                }
            } break;

            case ATNET_IO_LOADREQUEST_OK: {
                SLONG c = 0;
                SLONG FromPlayer = 0;
                SLONG Index = 0;

                Message >> FromPlayer >> Index;
                FromPlayer = NetCheckPlayerNum(FromPlayer, MessageType);

                Sim.Players.Players[FromPlayer].bReadyForBriefing = 1;

                // Haben alle ihr okay gegeben?
                for (c = 0; c < 4; c++) {
                    if (!static_cast<bool>(Sim.Players.Players[c].bReadyForBriefing) && (Sim.Players.Players[c].IsOut == 0) &&
                        Sim.Players.Players[c].Owner == 2) {
                        break;
                    }
                }

                if (c == 4) {
                    nOptionsOpen--;
                    SIM::SendSimpleMessage(ATNET_OPTIONS, 0, -1, Sim.localPlayer);
                    SIM::SendSimpleMessage(ATNET_IO_LOADREQUEST_DOIT, 0, Index);
                    Sim.LoadGame(Index);
                }
            } break;

            case ATNET_IO_LOADREQUEST_BAD:
                if (!static_cast<bool>(Sim.Players.Players[Sim.localPlayer].bReadyForBriefing)) {
                    nOptionsOpen--;
                    SIM::SendSimpleMessage(ATNET_OPTIONS, 0, -1, Sim.localPlayer);
                    Sim.Players.Players[Sim.localPlayer].bReadyForBriefing = 1;

                    if (Sim.Players.Players[Sim.localPlayer].LocationWin != nullptr) {
                        (Sim.Players.Players[Sim.localPlayer].LocationWin)->MenuStart(MENU_REQUEST, MENU_REQUEST_NET_LOADONE);
                    }
                }
                break;

            case ATNET_IO_LOADREQUEST_DOIT: {
                SLONG Index = 0;

                Message >> Index;

                Sim.LoadGame(Index);
            } break;

                //--------------------------------------------------------------------------------------------
                // Testing & Debugging:
                //--------------------------------------------------------------------------------------------
            case ATNET_CHECKRANDS: {
                SLONG rTime = 0;
                SLONG rGeneric = 0;
                ULONG rPersonRandCreate = 0;
                ULONG rPersonRandMisc = 0;
                ULONG rHeadlineRand = 0;
                ULONG rLMA = 0;
                ULONG rRBA = 0;
                ULONG rAA[MAX_CITIES];
                ULONG rFrachen = 0;
                SLONG rActionId[5 * 4];

                Message >> rTime;
                Message >> rPersonRandCreate >> rPersonRandMisc >> rHeadlineRand;
                Message >> rLMA >> rRBA >> rFrachen >> rGeneric;

                for (auto &c : rAA) {
                    Message >> c;
                }
                for (c = 0; c < 20; c++) {
                    Message >> rActionId[c];
                }
                SLONG rRobotSyncAge[4];
                for (c = 0; c < 4; c++) {
                    Message >> rRobotSyncAge[c];
                }

                /* The comparison below is the game's own desync detector, but it only exists in
                   debug builds. Report the same comparison through the trace, so that a release
                   build under the multiplayer harness sees it too. Both sides capture at the same
                   game minute; if the minutes differ the peers are several minutes apart, which
                   is worth knowing but makes the seeds incomparable. */
                if (gNetTraceLevel > 0) {
                    if (rTime != rChkTime) {
                        NetTraceEvent("RANDSKEW theirs_minute=%ld mine_minute=%ld", static_cast<long>(rTime), static_cast<long>(rChkTime));
                    } else {
                        auto Report = [&](const char *What, ULONG Theirs, ULONG Mine) {
                            if (Theirs != Mine) {
                                NetTraceEvent("DESYNC what=%s theirs=%lu mine=%lu", What, static_cast<unsigned long>(Theirs), static_cast<unsigned long>(Mine));
                            }
                        };
                        Report("PersonRandCreate", rPersonRandCreate, rChkPersonRandCreate);
                        Report("PersonRandMisc", rPersonRandMisc, rChkPersonRandMisc);
                        Report("HeadlineRand", rHeadlineRand, rChkHeadlineRand);
                        Report("LastMinute", rLMA, rChkLMA);
                        Report("Reisebuero", rRBA, rChkRBA);
                        Report("Fracht", rFrachen, rChkFrachen);

                        SLONG CitiesOff = 0;
                        SLONG FirstCity = -1;
                        for (c = 0; c < MAX_CITIES; c++) {
                            if (rAA[c] != rChkAA[c]) {
                                if (FirstCity < 0) {
                                    FirstCity = c;
                                }
                                CitiesOff++;
                            }
                        }
                        if (CitiesOff > 0) {
                            NetTraceEvent("DESYNC what=Ausland cities=%ld first=%ld theirs=%lu mine=%lu", static_cast<long>(CitiesOff), static_cast<long>(FirstCity),
                                          static_cast<unsigned long>(rAA[FirstCity]), static_cast<unsigned long>(rChkAA[FirstCity]));
                        }

                        /* Bot action queues are not comparable slot by slot. Bots run on the host
                           only: it broadcasts a freshly planned queue, then shifts it locally as
                           each action starts, while clients keep the broadcast copy and only catch
                           up when the action executes (see the "Manchmal kommen wir als Client hier
                           an" shift in PLAYER::RobotExecuteAction). So between two broadcasts the
                           client legitimately still holds actions the host has already consumed.
                           Also only on the host, a bot that finds its room occupied puts the
                           current action behind the secondary one and fills an empty secondary slot
                           with ACTION_BUERO or ACTION_PERSONAL (PERSON::DoOnePlayerStep, "Raum schon
                           besetzt?"). NetSyncRobot() hands the clients the host's whole queue again
                           before the next action executes, so neither is a disagreement: accept
                           when every pending host action, apart from at most one such filler, is
                           still pending on the client, in any order. Anything else is a real
                           disagreement about what a bot is going to do. */
                        for (SLONG p = 0; p < 4; p++) {
                            /* Both sides take the snapshot on the first step of the minute, each on its
                               own clock, and the peers are a few steps apart. A queue handed over
                               just before one snapshot and received just after the other is not a
                               disagreement - in a played session the host planned 40 ticks before
                               the minute and the client got the queue 20 ticks into it. Compare
                               only queues that have been left alone for a while on both sides. */
                            const SLONG kSettleSlices = 20;
                            if (rRobotSyncAge[p] < kSettleSlices || rChkRobotSyncAge[p] < kSettleSlices) {
                                continue;
                            }

                            const SLONG *Host = (Sim.bIsHost != 0) ? &rChkActionId[p * 5] : &rActionId[p * 5];
                            const SLONG *Client = (Sim.bIsHost != 0) ? &rActionId[p * 5] : &rChkActionId[p * 5];

                            std::vector<SLONG> HostPending;
                            std::vector<SLONG> ClientPending;
                            for (SLONG d = 0; d < 5; d++) {
                                if (Host[d] != ACTION_NONE) {
                                    HostPending.push_back(Host[d]);
                                }
                                if (Client[d] != ACTION_NONE) {
                                    ClientPending.push_back(Client[d]);
                                }
                            }

                            bool bLagOnly = true;
                            bool bFillerSeen = false;
                            for (const SLONG ActionId : HostPending) {
                                const auto Match = std::find(ClientPending.begin(), ClientPending.end(), ActionId);
                                if (Match != ClientPending.end()) {
                                    ClientPending.erase(Match);
                                } else if (!bFillerSeen && (ActionId == ACTION_BUERO || ActionId == ACTION_PERSONAL)) {
                                    bFillerSeen = true;
                                } else {
                                    bLagOnly = false;
                                    break;
                                }
                            }
                            if (bLagOnly) {
                                continue;
                            }

                            CString HostText;
                            CString ClientText;
                            for (SLONG d = 0; d < 5; d++) {
                                HostText += CString(d > 0 ? "," : "") + (Host[d] == ACTION_NONE ? "-" : Translate_ACTION(Host[d]));
                                ClientText += CString(d > 0 ? "," : "") + (Client[d] == ACTION_NONE ? "-" : Translate_ACTION(Client[d]));
                            }
                            NetTraceEvent("DESYNC what=RobotQueue player=%ld host=%s client=%s", static_cast<long>(p), HostText.c_str(), ClientText.c_str());
                        }
                    }
                }

#ifdef _DEBUG
                if (rTime != rChkTime)
                    DisplayBroadcastMessage(bprintf("rTime: %li vs %li\n", rTime, rChkTime));
                if (rPersonRandCreate != rChkPersonRandCreate)
                    DisplayBroadcastMessage(bprintf("rPersonRandCreate: %li vs %li\n", rPersonRandCreate, rChkPersonRandCreate));
                if (rPersonRandMisc != rChkPersonRandMisc)
                    DisplayBroadcastMessage(bprintf("rPersonRandMisc: %li vs %li\n", rPersonRandMisc, rChkPersonRandMisc));
                if (rHeadlineRand != rChkHeadlineRand)
                    DisplayBroadcastMessage(bprintf("rHeadlineRand: %li vs %li\n", rHeadlineRand, rChkHeadlineRand));
                if (rLMA != rChkLMA)
                    DisplayBroadcastMessage(bprintf("rLMA: %li vs %li\n", rLMA, rChkLMA));
                if (rRBA != rChkRBA)
                    DisplayBroadcastMessage(bprintf("rRBA: %li vs %li\n", rRBA, rChkRBA));
                // if (rChkGeneric!=rGeneric)                   DisplayBroadcastMessage (bprintf("rChkGeneric: %li vs %li\n", rChkGeneric, rGeneric));
                if (rFrachen != rChkFrachen)
                    DisplayBroadcastMessage(bprintf("rFrachen: %li vs %li\n", rFrachen, rChkFrachen));

                for (c = 0; c < MAX_CITIES; c++)
                    if (rAA[c] != rChkAA[c])
                        DisplayBroadcastMessage(bprintf("rAA[%li]: %li vs %li\n", c, rAA[c], rChkAA[c]));

                for (c = 0; c < 20; c++)
                    if (rActionId[c] != rChkActionId[c]) {
                        DisplayBroadcastMessage(
                            bprintf("Desync AI Action[%li]: %s vs %s\n", c, Translate_ACTION(rActionId[c]), Translate_ACTION(rChkActionId[c])));
                        AT_Log_I("AtNet", "Desync AI Action[%li]: %s vs %s\n", c, Translate_ACTION(rActionId[c]), Translate_ACTION(rChkActionId[c]));
                    }
#endif
            } break;

            case ATNET_GENERICSYNC: {
                SLONG localPlayer = 0;

                Message >> localPlayer;
                localPlayer = NetCheckPlayerNum(localPlayer, MessageType);
                Message >> GenericSyncIds[localPlayer];
                GenericSyncReceived[localPlayer].push_back(GenericSyncIds[localPlayer]);

                bReturnAfterThisMessage = true;
            } break;

            case ATNET_GENERICSYNCX: {
                SLONG localPlayer = 0;

                Message >> localPlayer;
                localPlayer = NetCheckPlayerNum(localPlayer, MessageType);
                Message >> GenericSyncIds[localPlayer] >> GenericSyncIdPars[localPlayer];

                bReturnAfterThisMessage = true;
            } break;

            case ATNET_GENERICASYNC: {
                SLONG SyncId = 0;
                SLONG Par = 0;
                SLONG player = 0;

                Message >> player;
                player = NetCheckPlayerNum(player, MessageType);
                Message >> SyncId >> Par;

                bReturnAfterThisMessage = true;

                NetGenericAsync(SyncId, Par, player);
            } break;

                //--------------------------------------------------------------------------------------------
                // Weitere Synchronisierungen:
                //--------------------------------------------------------------------------------------------
            case ATNET_BODYGUARD: {
                SLONG localPlayer = 0;
                SLONG delta = 0;

                Message >> localPlayer >> delta;
                localPlayer = NetCheckPlayerNum(localPlayer, MessageType);

                if (localPlayer != Sim.localPlayer) {
                    Sim.Players.Players[localPlayer].ChangeMoney(delta, 3130, "");
                }
            } break;

            case ATNET_CHANGEMONEY: {
                __int64 Par1 = 0;
                __int64 Par2 = 0;
                __int64 Par3 = 0;

                Message >> Par1 >> Par2 >> Par3;

                /* Par1 travels as 64 bit: range check before narrowing, or 0x100000001 would
                   narrow to a perfectly valid-looking 1. */
                SLONG playerId = NetCheckPlayerNum((Par1 < 0 || Par1 > 3) ? SLONG(-1) : static_cast<SLONG>(Par1), MessageType);
                SLONG statistikid = static_cast<SLONG>(Par3);

                Sim.Players.Players[playerId].ChangeMoney(Par2, statistikid, "");
            } break;

            case ATNET_SYNCKEROSIN: {
                SLONG playerId = 0;
                PLAYER::NetTankState State;

                Message >> playerId >> State.Tank >> State.TankOpen >> State.TankInhalt >> State.KerosinQuali >> State.KerosinKind >> State.TankPreis >>
                    State.Stamp;
                playerId = NetCheckPlayerNum(playerId, MessageType);

                if (playerId != Sim.localPlayer && State.Stamp >= 0) {
                    Sim.Players.Players[playerId].NetReceiveKerosin(State);
                }
            } break;

            case ATNET_WORKER_HIRE:
            case ATNET_WORKER_FIRE: {
                SLONG PlayerNum = 0;
                SLONG WorkerId = 0;

                Message >> PlayerNum >> WorkerId;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                /* Same validation as a local hire or fire: if this peer's worker pool already
                   disagreed, it logs an error instead of silently hiring the wrong person. */
                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];
                const bool ok = (MessageType == ATNET_WORKER_HIRE) ? GameMechanic::hireWorker(qPlayer, WorkerId, true)
                                                                   : GameMechanic::fireWorker(qPlayer, WorkerId, true);
                if (!ok) {
                    NetTraceEvent("WORKERSYNC failed name=%s p=%ld worker=%ld", Translate_ATNET(MessageType), static_cast<long>(PlayerNum),
                                  static_cast<long>(WorkerId));
                }
            } break;

            case ATNET_WORKER_SALARY: {
                SLONG PlayerNum = 0;
                SLONG WorkerId = 0;
                SLONG Art = 0;

                Message >> PlayerNum >> WorkerId >> Art;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                /* Repeat the owner's change rather than copying its result, so that a worker
                   the cut drives out of the job leaves on this peer too. */
                if (WorkerId == -1) {
                    if (Art == 2) {
                        GameMechanic::increaseAllSalaries(Sim.Players.Players[PlayerNum], true);
                    } else {
                        Workers.Gehaltsaenderung(Art, PlayerNum, true);
                    }
                } else if (WorkerId >= 0 && WorkerId < Workers.Workers.AnzEntries() && Workers.Workers[WorkerId].Employer == PlayerNum) {
                    Workers.Workers[WorkerId].Gehaltsaenderung(Art, true);
                } else {
                    NetTraceEvent("WORKERSYNC failed name=%s p=%ld worker=%ld", Translate_ATNET(MessageType), static_cast<long>(PlayerNum),
                                  static_cast<long>(WorkerId));
                }
            } break;

            case ATNET_STRIKE: {
                SLONG PlayerNum = 0;
                SLONG Event = 0;
                SLONG Par = 0;
                SLONG SenderHour = 0;

                Message >> PlayerNum >> Event >> Par >> SenderHour;
                PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);
                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

                if (Event == 0 && Par >= static_cast<SLONG>(GameMechanic::EndStrikeMode::Salary) &&
                    Par <= static_cast<SLONG>(GameMechanic::EndStrikeMode::Drunk)) {
                    const bool bWasOn = (qPlayer.StrikeEndType == 0);
                    GameMechanic::endStrike(qPlayer, static_cast<GameMechanic::EndStrikeMode>(Par), true);

                    /* The countdown ends the strike after that many changes of hour. Count them
                       from the owner's hour rather than from this peer's: a message that crosses
                       the change of hour would otherwise keep the departures of that hour
                       grounded on one peer only - and a delayed flight stays delayed. */
                    if (bWasOn && qPlayer.StrikeEndType != 0 && qPlayer.StrikeEndCountdown > 0) {
                        const SLONG Behind = SenderHour - (Sim.Date * 24 + Sim.GetHour());
                        if (Behind >= -24 && Behind <= 24) {
                            qPlayer.StrikeEndCountdown = max(1, qPlayer.StrikeEndCountdown + Behind);
                        } else {
                            NetTraceEvent("STRIKE end out of range p=%ld sender_hour=%ld", static_cast<long>(PlayerNum), static_cast<long>(SenderHour));
                        }
                    }
                } else if (Event == 1) {
                    qPlayer.StrikeHours = Par;
                } else if (Event == 2) {
                    /* The owner's peer decided that these people are striking until this hour
                       (counted from the start of the game). Turning it back into hours against
                       this peer's own clock lets the strike end in the same hour everywhere, even
                       though the message arrives just before or just after the change of hour. */
                    /* The longest strike the game hands out is 72 hours, and a peer that has not
                       reached the sender's hour yet arrives at one more than that - which is why
                       the bound is not 72. */
                    const SLONG Hours = Par - (Sim.Date * 24 + Sim.GetHour());
                    if (Hours > 0 && Hours <= 96) {
                        GameMechanic::startStrike(qPlayer, Hours, true);
                    } else {
                        NetTraceEvent("STRIKE out of range p=%ld until=%ld hours=%ld", static_cast<long>(PlayerNum), static_cast<long>(Par),
                                      static_cast<long>(Hours));
                    }
                }
            } break;

            case ATNET_SYNC_STAFF: {
                SLONG Anz = 0;

                Message >> Anz;

                while (Anz > 0) {
                    SLONG PlayerNum = 0;
                    SLONG Workers2 = 0;

                    Message >> PlayerNum >> Workers2;
                    PlayerNum = NetCheckPlayerNum(PlayerNum, MessageType);

                    for (SLONG d = 0; d < Workers2; d++) {
                        SLONG WorkerId = 0;
                        SLONG Gehalt = 0;
                        SLONG Happyness = 0;

                        Message >> WorkerId >> Gehalt >> Happyness;

                        /* Only for people this player really employs here: if the pools had drifted
                           apart, writing by index alone would scramble somebody else's staff. */
                        if (WorkerId >= 0 && WorkerId < Workers.Workers.AnzEntries() && Workers.Workers[WorkerId].Employer == PlayerNum) {
                            Workers.Workers[WorkerId].Gehalt = Gehalt;
                            Workers.Workers[WorkerId].Happyness = Happyness;
                        }
                    }

                    Anz--;
                }
            } break;

            case ATNET_SYNCGEHALT: {
                SLONG playerId = 0;
                SLONG gehalt = 0;

                Message >> playerId >> gehalt;
                playerId = NetCheckPlayerNum(playerId, MessageType);

                Sim.Players.Players[playerId].Statistiken[STAT_GEHALT].SetAtPastDay(gehalt);
            } break;

            case ATNET_SYNCNUMFLUEGE: {
                SLONG playerId = 0;
                SLONG auftrag = 0;
                SLONG lm = 0;
                SLONG fracht = 0;

                Message >> playerId >> auftrag >> lm >> fracht;
                playerId = NetCheckPlayerNum(playerId, MessageType);

                Sim.Players.Players[playerId].Statistiken[STAT_AUFTRAEGE].SetAtPastDay(auftrag);
                Sim.Players.Players[playerId].Statistiken[STAT_LMAUFTRAEGE].SetAtPastDay(lm);
                Sim.Players.Players[playerId].Statistiken[STAT_FRACHTEN].SetAtPastDay(fracht);
            } break;

                //--------------------------------------------------------------------------------------------
                // Microsoft and SBLib internal codes:
                //--------------------------------------------------------------------------------------------
            case 0x0003:
            case 0x0007:
            case 0x0021:
            case 0x0102:
            case 0x0103:
            case 0x0104:
            case 0x0105:
            case 0x0106:
            case 0x0107:
            case 0x0108:
            case 0x0109:
            case 0x010A:
            case 0x010D:
            case 0xDEADBEEF:
                break;

            default:
                // Something is wrong
                hprintf("Unknown Message %lx", MessageType);
                break;
            }

            /* The original code wanted to break into the debugger when a handler did not
               consume its message exactly; report it in the trace instead. A non-zero tail
               means this receiver disagrees with the sender about the message layout, which
               is how a session silently drifts apart. */
            NetTraceTail(MessageType, static_cast<SLONG>(Message.MemBufferUsed) - Message.MemPointer);

            } catch (TeakLibException &ex) {
                AT_Log("PumpNetwork: dropping malformed message %s: %s", Translate_ATNET(MessageType), ex.what());
                NetTraceEvent("DROP name=%s reason=%s", Translate_ATNET(MessageType), ex.what());
                ex.caught();
            }
        }
    }
}

//--------------------------------------------------------------------------------------------
// Returns a (short) String describing the Medium
//--------------------------------------------------------------------------------------------
CString GetMediumName(SLONG Medium) { return (StandardTexte.GetS(TOKEN_MISC, 7100 + Medium)); }

//--------------------------------------------------------------------------------------------
// Kehrt erst zurück, wenn die anderen Spieler hier auch waren:
//--------------------------------------------------------------------------------------------
void NetGenericSync(SLONG SyncId) {
    if (Sim.bNetwork == 0) {
        return;
    }
    if (Sim.localPlayer < 0 || Sim.localPlayer > 3) {
        return;
    }

    SIM::SendSimpleMessage(ATNET_GENERICSYNC, 0, Sim.localPlayer, SyncId); // Requesting Sync

    GenericSyncIds[Sim.localPlayer] = SyncId;

    /* Wait for SyncId in what every other human has sent us, not merely in the last thing
       they sent. The morning briefing syncs twice in a row (0x4211014, then 0x4211015), and
       with three or more humans a peer that got through the first one can send the second
       before another peer has received the last human's first: that peer then saw
       0x4211015 where it waited for 0x4211014 and waited forever - and everyone else with it
       at the second sync. A resent packet on the internet is enough to open that window. */
    /* This loop neither draws nor takes input, so a peer that never sends its half looks to the
       player like a hung game - and left nothing in the log to say so. Name the players still
       missing every five seconds. */
    const DWORD WaitingSince = AtGetTime();
    DWORD LastReported = WaitingSince;

    while (true) {
        bool bAllThere = true;
        SLONG Missing = 0;
        for (SLONG c = 0; c < 4; c++) {
            if (c == Sim.localPlayer || Sim.Players.Players[c].Owner == 1 || (Sim.Players.Players[c].IsOut != 0)) {
                continue;
            }
            const auto &Received = GenericSyncReceived[c];
            if (std::find(Received.begin(), Received.end(), SyncId) == Received.end()) {
                bAllThere = false;
                Missing |= (1 << c);
            }
        }

        if (!bAllThere && AtGetTime() - LastReported >= 5000) {
            LastReported = AtGetTime();
            AT_Log("Still waiting for sync %lx from players 0x%lx after %lu s (day %ld %02ld:%02ld)", static_cast<unsigned long>(SyncId),
                   static_cast<unsigned long>(Missing), static_cast<unsigned long>((AtGetTime() - WaitingSince) / 1000), static_cast<long>(Sim.Date),
                   static_cast<long>(Sim.GetHour()), static_cast<long>(Sim.GetMinute()));
            NetTraceEvent("STILLSYNCING id=%lx missing=0x%lx seconds=%lu", static_cast<unsigned long>(SyncId), static_cast<unsigned long>(Missing),
                          static_cast<unsigned long>((AtGetTime() - WaitingSince) / 1000));
        }

        if (bAllThere) {
            for (auto &Received : GenericSyncReceived) {
                const auto Hit = std::find(Received.begin(), Received.end(), SyncId);
                if (Hit != Received.end()) {
                    Received.erase(Received.begin(), Hit + 1);
                }
            }
            return;
        }

        PumpNetwork();
    }
}

void NetResetGenericSync() {
    for (auto &Received : GenericSyncReceived) {
        Received.clear();
    }
}

//--------------------------------------------------------------------------------------------
// Kehrt erst zurück, wenn die anderen Spieler hier auch waren:
// Gibt Warnung aus, falls die Parameter unterschiedlich waren.
//--------------------------------------------------------------------------------------------
#ifdef DEBUG_NET
void NetGenericSync(SLONG SyncId, SLONG Par) {
    static bool bReentrant = false;

    if (bReentrant) {
        return;
    }
    if (!Sim.bNetwork) {
        return;
    }
    if (Sim.localPlayer < 0 || Sim.localPlayer > 3) {
        return;
    }
    if (Sim.Players.Players[Sim.localPlayer].Owner == 1) {
        return;
    }
    if (Sim.Time == 9 * 60000) {
        return;
    }

    bReentrant = true;

    Sim.SendSimpleMessage(ATNET_GENERICSYNCX, 0, Sim.localPlayer, SyncId, Par); // Requesting Sync

    GenericSyncIds[Sim.localPlayer] = SyncId;
    GenericSyncIdPars[Sim.localPlayer] = Par;

    while (1) {
        SLONG c = 0;
        for (; c < 4; c++) {
            if (Sim.Players.Players[c].Owner != 1 && GenericSyncIds[c] != SyncId && !Sim.Players.Players[c].IsOut) {
                break;
            }
        }

        if (c == 4) {
            for (c = 0; c < 4; c++) {
                if (Sim.Players.Players[c].Owner != 1 && !Sim.Players.Players[c].IsOut && GenericSyncIdPars[c] != Par) {
                    DisplayBroadcastMessage(bprintf("NetGenericSync (%li): %li vs. %li\n", SyncId, Par, GenericSyncIdPars[c]));
                    AT_Log_I("AtNet", "Desync detected Id(%li): %li vs. %li\n", SyncId, Par, GenericSyncIdPars[c]);
                    // DebugBreak();
                }
            }

            for (c = 0; c < 4; c++) {
                GenericSyncIds[c] = 0;
            }

            bReentrant = false;
            return;
        }

        PumpNetwork();
    }
}
#else
void NetGenericSync(SLONG /*SyncId*/, SLONG /*Par*/) {}
#endif

//--------------------------------------------------------------------------------------------
// Kehrt erst zurück, wenn die anderen Spieler hier auch waren:
// Gibt Warnung aus, falls die Parameter unterschiedlich waren.
//--------------------------------------------------------------------------------------------
#ifdef DEBUG_NET
void NetGenericAsync(SLONG SyncId, SLONG Par, SLONG player) {
    if (!Sim.bNetwork) {
        return;
    }
    if (Sim.localPlayer < 0 || Sim.localPlayer > 3) {
        return;
    }
    if (Sim.Players.Players[Sim.localPlayer].Owner == 1) {
        return;
    }
    if (Sim.Time == 9 * 60000) {
        return;
    }

    if (player == -1) {
        Sim.SendSimpleMessage(ATNET_GENERICASYNC, 0, Sim.localPlayer, SyncId, Par); // Requesting Sync
        player = Sim.localPlayer;
    }

    SLONG d;

    // Gibt es den Eintrag schon?
    for (d = 0; d < 400; d++) {
        if (GenericAsyncIds[d] == SyncId) {
            break;
        }
    }

    // Eventuell müssen wir einen Leereintrag suchen:
    if (d == 400) {
        for (d = 0; d < 400; d += 4) {
            if (GenericAsyncIds[d] == 0 && GenericAsyncIds[d + 1] == 0 && GenericAsyncIds[d + 2] == 0 && GenericAsyncIds[d + 3] == 0) {
                break;
            }
        }

        if (d == 400) {
            DisplayBroadcastMessage("NetGenericAsync overflow\n");
            return;
        }
    }

    d = d / 4 * 4;
    GenericAsyncIds[d + player] = SyncId;
    GenericAsyncIdPars[d + player] = Par;

    SLONG c = 0;
    for (; c < 4; c++) {
        if (Sim.Players.Players[c].Owner != 1 && GenericAsyncIds[d + c] != SyncId && !Sim.Players.Players[c].IsOut) {
            break;
        }
    }

    if (c == 4) {
        for (c = 0; c < 4; c++) {
            if (Sim.Players.Players[c].Owner != 1 && !Sim.Players.Players[c].IsOut && GenericAsyncIdPars[d + c] != Par) {
                DisplayBroadcastMessage(bprintf("NetGenericAsync (%li): %li vs. %li\n", SyncId, Par, GenericAsyncIdPars[d + c]));
                AT_Log_I("AtNet", "Desync detected Id(%li): %li vs. %li\n", SyncId, Par, GenericSyncIdPars[d + c]);
                // DebugBreak();
            }
        }

        for (c = 0; c < 4; c++) {
            GenericAsyncIds[d + c] = 0;
        }
    }
}
#else
void NetGenericAsync(SLONG /*SyncId*/, SLONG /*Par*/, SLONG /*player*/) {}
#endif
