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

#include <SDL_thread.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#define AT_Log(...) AT_Log_I("NetTrace", __VA_ARGS__)

SLONG gNetTraceLevel = 0;

namespace {

SDL_threadID gMainThread = 0;
std::atomic<SLONG> gThreadViolations{0};

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

    AT_Log("EVT day=%ld t=%ld lp=%ld wall=%lu %s", static_cast<long>(Sim.Date), static_cast<long>(Sim.Time), static_cast<long>(Sim.localPlayer),
           static_cast<unsigned long>(AtGetTime()), Text);
}

void NetTraceFingerprint(const char *When) {
    if (gNetTraceLevel <= 0) {
        return;
    }

    /* At the start of the morning briefing every peer has just received every owner's money
       and must agree on it - that is what the bankruptcy verdict is computed from. */
    const bool bHashHumanMoney = (strcmp(When, "briefing") == 0);

    for (SLONG c = 0; c < Sim.Players.Players.AnzEntries() && c < 4; c++) {
        const PLAYER &qPlayer = Sim.Players.Players[c];

        Fingerprint Fp;
        /* A human's money is authoritative only on that human's own peer. The others do not book
           its salaries (PLAYER::BookSalary skips Owner 2) and get the real figure from
           ATNET_SYNC_MONEY when that human leaves the morning briefing. Between those syncs the
           copies differ by design, so hashing them would report a desync every single day.
           Bots are Owner 1 on every peer and booked identically everywhere, so their money is
           hashed. Both are still printed. */
        if (qPlayer.Owner == 1 || bHashHumanMoney) {
            Fp.Add(qPlayer.Money);
            Fp.Add(qPlayer.Credit);
        }
        Fp.Add(qPlayer.Image);
        Fp.Add(qPlayer.AnzAktien);
        Fp.Add(qPlayer.IsOut);
        /* Owner is deliberately not hashed: every peer sees itself as 0 and the others as 2,
           so including it would make every comparison report a divergence on day 0. It stays
           in the printed line because it is useful context. */

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
            Fp.Add(qRoute.Ticketpreis);
            Fp.Add(qRoute.TicketpreisFC);
            if (qRoute.Rang > 0) {
                Routes++;
            }
        }

        for (SLONG d = 0; d < 4; d++) {
            Fp.Add(qPlayer.OwnsAktien[d]);
            Fp.Add(qPlayer.Sympathie[d]);
        }

        /* Equipment and its targets decide each plane's refit and how many passengers it
           carries, and WorstZustand the nightly repair bill; a disagreement here only shows
           up as money a day later. The current condition is left out, it wears
           continuously. */
        for (SLONG d = 0; d < qPlayer.Planes.AnzEntries(); d++) {
            if (qPlayer.Planes.IsInAlbum(d) == 0) {
                continue;
            }
            const CPlane &qPlane = qPlayer.Planes[d];
            Fp.Add(d);
            Fp.Add(qPlane.Sitze);
            Fp.Add(qPlane.SitzeTarget);
            Fp.Add(qPlane.Essen);
            Fp.Add(qPlane.EssenTarget);
            Fp.Add(qPlane.Tabletts);
            Fp.Add(qPlane.TablettsTarget);
            Fp.Add(qPlane.Deco);
            Fp.Add(qPlane.DecoTarget);
            Fp.Add(qPlane.Triebwerk);
            Fp.Add(qPlane.TriebwerkTarget);
            Fp.Add(qPlane.Reifen);
            Fp.Add(qPlane.ReifenTarget);
            Fp.Add(qPlane.Elektronik);
            Fp.Add(qPlane.ElektronikTarget);
            Fp.Add(qPlane.Sicherheit);
            Fp.Add(qPlane.SicherheitTarget);
            Fp.Add(qPlane.MaxPassagiere);
            Fp.Add(qPlane.MaxPassagiereFC);
            Fp.Add(qPlane.MaxPassagiereTarget);
            Fp.Add(qPlane.MaxPassagiereTargetFC);
            Fp.Add(qPlane.TargetZustand);
            Fp.Add(qPlane.WorstZustand);

            /* The flight plan and the gate each flight got: a flight that finds no gate at the
               home airport costs image, so peers that planned gates differently diverge. */
            const CFlugplan &qPlan = qPlane.Flugplan;
            for (SLONG e = 0; e < qPlan.Flug.AnzEntries(); e++) {
                const CFlugplanEintrag &qFlight = qPlan.Flug[e];
                if (qFlight.ObjectType == 0) {
                    continue;
                }
                Fp.Add(e);
                Fp.Add(qFlight.ObjectType);
                Fp.Add(qFlight.ObjectId);
                Fp.Add(qFlight.Startdate);
                Fp.Add(qFlight.Startzeit);
                Fp.Add(qFlight.Gate);
            }
        }

        /* The player's own contracts: which orders and freight it holds, how far each is
           planned and done. The daily penalty and bonus bookings follow from these. */
        for (SLONG d = 0; d < qPlayer.Auftraege.AnzEntries(); d++) {
            if (qPlayer.Auftraege.IsInAlbum(d) == 0) {
                continue;
            }
            const CAuftrag &qOrder = qPlayer.Auftraege[d];
            Fp.Add(d);
            Fp.Add(qOrder.VonCity);
            Fp.Add(qOrder.NachCity);
            Fp.Add(qOrder.Date);
            Fp.Add(qOrder.BisDate);
            Fp.Add(qOrder.InPlan);
            Fp.Add(qOrder.Okay);
            Fp.Add(qOrder.Praemie);
            Fp.Add(qOrder.Strafe);
        }
        for (SLONG d = 0; d < qPlayer.Frachten.AnzEntries(); d++) {
            if (qPlayer.Frachten.IsInAlbum(d) == 0) {
                continue;
            }
            const CFracht &qFreight = qPlayer.Frachten[d];
            Fp.Add(d);
            Fp.Add(qFreight.VonCity);
            Fp.Add(qFreight.NachCity);
            Fp.Add(qFreight.Tons);
            Fp.Add(qFreight.TonsOpen);
            Fp.Add(qFreight.TonsLeft);
            Fp.Add(qFreight.Praemie);
        }

        Fp.Add(qPlayer.Tank);
        Fp.Add(qPlayer.TankInhalt);
        Fp.Add(qPlayer.TankOpen);
        Fp.Add(qPlayer.KerosinKind);

        /* Sabotage state: a sabotaged office and the security measures decide what the next
           saboteur gets away with. */
        Fp.Add(qPlayer.OfficeState);
        Fp.Add(qPlayer.SecurityFlags);

        /* A strike delays the player's departures on every peer. How it ended is left out: its
           owner's peer clears that once the player has been told. */
        Fp.Add(qPlayer.StrikeHours);
        Fp.Add(qPlayer.StrikePlanned);
        Fp.Add(qPlayer.StrikeEndCountdown);

        Fp.Add(qPlayer.Gates.NumRented);
        for (SLONG d = 0; d < qPlayer.Gates.Gates.AnzEntries(); d++) {
            Fp.Add(qPlayer.Gates.Gates[d].Nummer);
        }

        /* Staff drives the salary booked every night, so a disagreement about who works for
           whom, or for how much, surfaces as money a day later - better to see it directly.
           Happiness decides who quits during the night. */
        SLONG Staff = 0;
        for (SLONG d = 0; d < SLONG(Workers.Workers.AnzEntries()); d++) {
            if (Workers.Workers[d].Employer == c) {
                Fp.Add(d);
                Fp.Add(Workers.Workers[d].Gehalt);
                Fp.Add(Workers.Workers[d].Happyness);
                Staff++;
            }
        }

        AT_Log("FP  %s day=%ld t=%ld p=%ld owner=%ld out=%ld money=%lld credit=%lld image=%ld planes=%ld branches=%ld routes=%ld orders=%ld gates=%ld "
               "shares=%ld staff=%ld hash=%08lx",
               When, static_cast<long>(Sim.Date), static_cast<long>(Sim.Time), static_cast<long>(c), static_cast<long>(qPlayer.Owner),
               static_cast<long>(qPlayer.IsOut), static_cast<long long>(qPlayer.Money), static_cast<long long>(qPlayer.Credit),
               static_cast<long>(qPlayer.Image), static_cast<long>(qPlayer.Planes.AnzEntries()), static_cast<long>(Branches), static_cast<long>(Routes),
               static_cast<long>(qPlayer.Auftraege.AnzEntries()), static_cast<long>(qPlayer.Gates.Gates.AnzEntries()), static_cast<long>(qPlayer.AnzAktien),
               static_cast<long>(Staff), static_cast<unsigned long>(Fp.Get()));
    }

    /* The shared pools are the other thing every peer must agree on: a divergence here is what
       makes one peer index an order the others do not have, or hand an auction or a used
       plane to different players. Hash what is in them, not just how big they are. */
    Fingerprint Pool;
    auto AddOrders = [&Pool](CAuftraege &Orders) {
        Pool.Add(Orders.Random.GetSeed());
        for (SLONG d = 0; d < Orders.AnzEntries(); d++) {
            if (Orders.IsInAlbum(d) == 0) {
                continue;
            }
            const CAuftrag &qOrder = Orders[d];
            Pool.Add(d);
            Pool.Add(qOrder.VonCity);
            Pool.Add(qOrder.NachCity);
            Pool.Add(qOrder.Personen);
            Pool.Add(qOrder.Date);
            Pool.Add(qOrder.BisDate);
            Pool.Add(qOrder.Praemie);
            Pool.Add(qOrder.Strafe);
        }
    };
    AddOrders(LastMinuteAuftraege);
    AddOrders(ReisebueroAuftraege);
    for (auto &Orders : AuslandsAuftraege) {
        AddOrders(Orders);
    }

    Pool.Add(gFrachten.Random.GetSeed());
    for (SLONG d = 0; d < gFrachten.AnzEntries(); d++) {
        if (gFrachten.IsInAlbum(d) == 0) {
            continue;
        }
        const CFracht &qFreight = gFrachten[d];
        Pool.Add(d);
        Pool.Add(qFreight.VonCity);
        Pool.Add(qFreight.NachCity);
        Pool.Add(qFreight.Tons);
        Pool.Add(qFreight.Praemie);
        Pool.Add(qFreight.Strafe);
    }

    for (const auto *Board : {&TafelData.Route, &TafelData.City, &TafelData.Gate}) {
        for (const auto &qNote : *Board) {
            Pool.Add(qNote.ZettelId);
            Pool.Add(qNote.Player);
            Pool.Add(qNote.Preis);
            Pool.Add(qNote.Rang);
        }
    }

    SLONG UsedPlanes = 0;
    for (SLONG d = 0; d < Sim.UsedPlanes.AnzEntries(); d++) {
        if (Sim.UsedPlanes.IsInAlbum(d) == 0 || Sim.UsedPlanes[d].Name.empty()) {
            continue;
        }
        const CPlane &qPlane = Sim.UsedPlanes[d];
        Pool.Add(d);
        Pool.Add(qPlane.TypeId);
        Pool.Add(qPlane.Baujahr);
        Pool.Add(qPlane.Zustand);
        UsedPlanes++;
    }

    /* Days the security office stays switched off after a sabotage; while it is off nobody's
       security measures work. */
    Pool.Add(Sim.nSecOutDays);

    AT_Log("FP  %s day=%ld t=%ld pool lma=%ld rba=%ld fracht=%ld ausland=%ld usedplanes=%ld expand=%ld hash=%08lx", When, static_cast<long>(Sim.Date),
           static_cast<long>(Sim.Time), static_cast<long>(LastMinuteAuftraege.GetNumUsed()), static_cast<long>(ReisebueroAuftraege.GetNumUsed()),
           static_cast<long>(gFrachten.GetNumUsed()), static_cast<long>(AuslandsAuftraege.size()), static_cast<long>(UsedPlanes),
           static_cast<long>(Sim.ExpandAirport), static_cast<unsigned long>(Pool.Get()));
}

void NetTraceSetMainThread() { gMainThread = SDL_ThreadID(); }

void NetTraceCheckThread(const char *Where) {
    if (gNetTraceLevel <= 0 || gMainThread == 0) {
        return;
    }
    const SDL_threadID Current = SDL_ThreadID();
    if (Current == gMainThread) {
        return;
    }
    /* Capped: a path that runs off-thread usually does so on every timer tick. */
    if (gThreadViolations.fetch_add(1) < 200) {
        NetTraceEvent("THREAD where=%s thread=%lu main=%lu - network or game state touched off the main thread", Where,
                      static_cast<unsigned long>(Current), static_cast<unsigned long>(gMainThread));
    }
}
