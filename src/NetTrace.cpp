//============================================================================================
// NetTrace.cpp : Structured tracing of the multiplayer protocol.
//============================================================================================
// Link: "NetTrace.h"
//============================================================================================
#include "NetTrace.h"

#include "AtNet.h"
#include "class.h"
#include "global.h"
#include "helper.h"
#include "Proto.h"
#include "TeakLibW.h"

#include <cstdarg>
#include <cstdio>

#define AT_Log(...) AT_Log_I("NetTrace", __VA_ARGS__)

SLONG gNetTraceLevel = 0;

namespace {

/* Ordinal of the next traced message. Lets a reader tell "the message never arrived" from
   "the messages arrived in a different order" when comparing two logs. */
SLONG gTraceOrdinal = 0;

/* These fire many times per second and would bury everything else, so they only show up at
   level 2. */
bool IsHighFrequency(ULONG MessageType) {
    return MessageType == ATNET_PLAYERPOS || MessageType == ATNET_TIMEPING || MessageType == ATNET_ALIVE || MessageType == ATNET_SETSPEED;
}

/* FNV-1a. Only needs to be stable within one build, never persisted. */
class Fingerprint {
  public:
    void Add(__int64 Value) {
        for (SLONG i = 0; i < 8; i++) {
            Hash ^= static_cast<ULONG>((Value >> (i * 8)) & 0xFF);
            Hash *= 16777619U;
        }
    }
    ULONG Get() const { return Hash; }

  private:
    ULONG Hash{2166136261U};
};

} // namespace

bool NetTraceWants(ULONG MessageType) {
    if (gNetTraceLevel <= 0) {
        return false;
    }
    if (gNetTraceLevel == 1 && IsHighFrequency(MessageType)) {
        return false;
    }
    return true;
}

void NetTraceMessage(const char *Direction, ULONG MessageType, ULONG Peer, SLONG Bytes, SLONG Tail) {
    if (!NetTraceWants(MessageType)) {
        return;
    }

    char TailText[24] = "";
    if (Tail >= 0) {
        snprintf(TailText, sizeof(TailText), " tail=%ld", static_cast<long>(Tail));
    }

    AT_Log("MSG n=%ld dir=%s lp=%ld peer=%lx type=%lx name=%s bytes=%ld day=%ld t=%ld ts=%ld wall=%lu%s", static_cast<long>(gTraceOrdinal++), Direction,
           static_cast<long>(Sim.localPlayer), static_cast<unsigned long>(Peer), static_cast<unsigned long>(MessageType), Translate_ATNET(MessageType),
           static_cast<long>(Bytes), static_cast<long>(Sim.Date), static_cast<long>(Sim.Time), static_cast<long>(Sim.TimeSlice),
           static_cast<unsigned long>(AtGetTime()), TailText);
}

void NetTraceTail(ULONG MessageType, SLONG Tail) {
    if (gNetTraceLevel <= 0) {
        return;
    }

    /* Unread or over-read bytes mean the sender and this receiver disagree about the layout
       of this message. Always report that, even at level 1 and even for the high frequency
       messages that are otherwise filtered out - it is never normal. */
    if (Tail != 0) {
        AT_Log("MISMATCH name=%s tail=%ld day=%ld t=%ld - sender and receiver disagree about this message's layout", Translate_ATNET(MessageType),
               static_cast<long>(Tail), static_cast<long>(Sim.Date), static_cast<long>(Sim.Time));
    }
}

void NetTraceEvent(const char *Format, ...) {
    if (gNetTraceLevel <= 0) {
        return;
    }

    char Text[512];
    va_list Args;
    va_start(Args, Format);
    vsnprintf(Text, sizeof(Text), Format, Args);
    va_end(Args);

    AT_Log("EVT day=%ld t=%ld lp=%ld %s", static_cast<long>(Sim.Date), static_cast<long>(Sim.Time), static_cast<long>(Sim.localPlayer), Text);
}

void NetTraceFingerprint(const char *When) {
    if (gNetTraceLevel <= 0) {
        return;
    }

    for (SLONG c = 0; c < Sim.Players.Players.AnzEntries() && c < 4; c++) {
        const PLAYER &qPlayer = Sim.Players.Players[c];

        Fingerprint Fp;
        Fp.Add(qPlayer.Money);
        Fp.Add(qPlayer.Credit);
        Fp.Add(qPlayer.Image);
        Fp.Add(qPlayer.AnzAktien);
        Fp.Add(qPlayer.IsOut);
        Fp.Add(qPlayer.Owner);

        SLONG Branches = 0;
        for (SLONG d = 0; d < qPlayer.RentCities.RentCities.AnzEntries(); d++) {
            const CRentCity &qCity = qPlayer.RentCities.RentCities[d];
            Fp.Add(qCity.Rang);
            Fp.Add(qCity.Image);
            Fp.Add(qCity.Miete);
            if (qCity.Rang > 0) {
                Branches++;
            }
        }

        SLONG Routes = 0;
        for (SLONG d = 0; d < qPlayer.RentRouten.RentRouten.AnzEntries(); d++) {
            const CRentRoute &qRoute = qPlayer.RentRouten.RentRouten[d];
            Fp.Add(qRoute.Rang);
            Fp.Add(qRoute.Auslastung);
            Fp.Add(qRoute.RoutenAuslastung);
            if (qRoute.Rang > 0) {
                Routes++;
            }
        }

        for (SLONG d = 0; d < 4; d++) {
            Fp.Add(qPlayer.OwnsAktien[d]);
            Fp.Add(qPlayer.Sympathie[d]);
        }

        AT_Log("FP  %s day=%ld t=%ld p=%ld owner=%ld out=%ld money=%lld credit=%lld image=%ld planes=%ld branches=%ld routes=%ld orders=%ld gates=%ld "
               "shares=%ld hash=%08lx",
               When, static_cast<long>(Sim.Date), static_cast<long>(Sim.Time), static_cast<long>(c), static_cast<long>(qPlayer.Owner),
               static_cast<long>(qPlayer.IsOut), static_cast<long long>(qPlayer.Money), static_cast<long long>(qPlayer.Credit),
               static_cast<long>(qPlayer.Image), static_cast<long>(qPlayer.Planes.AnzEntries()), static_cast<long>(Branches), static_cast<long>(Routes),
               static_cast<long>(qPlayer.Auftraege.AnzEntries()), static_cast<long>(qPlayer.Gates.Gates.AnzEntries()), static_cast<long>(qPlayer.AnzAktien),
               static_cast<unsigned long>(Fp.Get()));
    }

    /* The shared order pools are the other thing every peer must agree on: a divergence here
       is what makes one peer index an order the others do not have. */
    Fingerprint Pool;
    Pool.Add(LastMinuteAuftraege.AnzEntries());
    Pool.Add(ReisebueroAuftraege.AnzEntries());
    Pool.Add(gFrachten.AnzEntries());
    for (SLONG c = 0; c < SLONG(AuslandsAuftraege.size()); c++) {
        Pool.Add(AuslandsAuftraege[c].AnzEntries());
    }

    AT_Log("FP  %s day=%ld t=%ld pool lma=%ld rba=%ld fracht=%ld ausland=%ld expand=%ld hash=%08lx", When, static_cast<long>(Sim.Date),
           static_cast<long>(Sim.Time), static_cast<long>(LastMinuteAuftraege.AnzEntries()), static_cast<long>(ReisebueroAuftraege.AnzEntries()),
           static_cast<long>(gFrachten.AnzEntries()), static_cast<long>(AuslandsAuftraege.size()), static_cast<long>(Sim.ExpandAirport),
           static_cast<unsigned long>(Pool.Get()));
}
