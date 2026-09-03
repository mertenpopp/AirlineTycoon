#pragma once
//============================================================================================
// NetTrace.h - Structured tracing of the multiplayer protocol.
//============================================================================================
// Every line is machine parseable and carries the in-game clock, so traces captured on two
// peers can be joined on (day, type, name) and diffed to find where they stop agreeing.
// Enable with the "/nettrace [level]" command line switch:
//
//   0  off (default, no runtime cost beyond one comparison per message)
//   1  protocol events, drops and per-day state fingerprints
//   2  additionally the per-frame chatter (player positions, time pings, keepalives)
//
// Line formats written to the game log:
//
//   NetTrace || MSG n=<ordinal> dir=SEND|RECV lp=<localPlayer> peer=<targetOrSource>
//              type=<hex> name=<ATNET_*> bytes=<n> day=<d> t=<Sim.Time> ts=<Sim.TimeSlice>
//              wall=<ms> tail=<unreadBytesAfterDispatch>
//   NetTrace || FP  day=<d> t=<Sim.Time> p=<player> ... hash=<hex>
//   NetTrace || EVT day=<d> t=<Sim.Time> <free text>
//
// "tail" is the number of bytes of the message the handler did not consume. Anything other
// than 0 means sender and receiver disagree about the layout of that message, which is the
// single most likely cause of a silent divergence.
//============================================================================================

#include "defines.h"

/* 0 = off, 1 = normal, 2 = verbose. Set from the command line, see CTakeOffApp::CLI. */
extern SLONG gNetTraceLevel;

/* True when this message type should be traced at the current level. Cheap; call before
   doing any formatting work. */
bool NetTraceWants(ULONG MessageType);

/* One line per sent/received message. "peer" is the target network id for SEND and the
   local id for RECV (the transport does not tell us the origin). "tail" is only known
   after dispatch, so receivers pass -1 here and call NetTraceTail() afterwards. */
void NetTraceMessage(const char *Direction, ULONG MessageType, ULONG Peer, SLONG Bytes, SLONG Tail);

/* Records how many bytes of the last traced RECV were left unread. */
void NetTraceTail(ULONG MessageType, SLONG Tail);

/* Free-form protocol event (connect, drop, migration, dropped message, ...). */
void NetTraceEvent(const char *Format, ...);

/* Compact fingerprint of everything the peers must agree on, one line per player plus one
   for the shared pools. Diff two peers' FP lines to locate a divergence. */
void NetTraceFingerprint(const char *When);
