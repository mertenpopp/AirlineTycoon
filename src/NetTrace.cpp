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

        /* One hash per part as well as the whole, so that the trace diff names the part that
           differs rather than just the player. */
        Fingerprint Fp;
        Fingerprint FpCity;
        Fingerprint FpRoute;
        Fingerprint FpStock;
        Fingerprint FpPlane;
        Fingerprint FpOrder;
        Fingerprint FpMisc;
        Fingerprint FpStaff;
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
            FpCity.Add(qCity.Rang);
            FpCity.Add(qCity.Image);
            FpCity.Add(qCity.Miete);
            if (qCity.Rang > 0) {
                Branches++;
            }
        }

        SLONG Routes = 0;
        for (SLONG d = 0; d < qPlayer.RentRouten.RentRouten.AnzEntries(); d++) {
            const CRentRoute &qRoute = qPlayer.RentRouten.RentRouten[d];
            FpRoute.Add(qRoute.Rang);
            FpRoute.Add(qRoute.Auslastung);
            FpRoute.Add(qRoute.RoutenAuslastung);
            FpRoute.Add(qRoute.Ticketpreis);
            FpRoute.Add(qRoute.TicketpreisFC);
            if (qRoute.Rang > 0) {
                Routes++;
            }
        }

        for (SLONG d = 0; d < 4; d++) {
            FpStock.Add(qPlayer.OwnsAktien[d]);
            FpStock.Add(qPlayer.Sympathie[d]);
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
            FpPlane.Add(d);
            FpPlane.Add(qPlane.Sitze);
            FpPlane.Add(qPlane.SitzeTarget);
            FpPlane.Add(qPlane.Essen);
            FpPlane.Add(qPlane.EssenTarget);
            FpPlane.Add(qPlane.Tabletts);
            FpPlane.Add(qPlane.TablettsTarget);
            FpPlane.Add(qPlane.Deco);
            FpPlane.Add(qPlane.DecoTarget);
            FpPlane.Add(qPlane.Triebwerk);
            FpPlane.Add(qPlane.TriebwerkTarget);
            FpPlane.Add(qPlane.Reifen);
            FpPlane.Add(qPlane.ReifenTarget);
            FpPlane.Add(qPlane.Elektronik);
            FpPlane.Add(qPlane.ElektronikTarget);
            FpPlane.Add(qPlane.Sicherheit);
            FpPlane.Add(qPlane.SicherheitTarget);
            FpPlane.Add(qPlane.MaxPassagiere);
            FpPlane.Add(qPlane.MaxPassagiereFC);
            FpPlane.Add(qPlane.MaxPassagiereTarget);
            FpPlane.Add(qPlane.MaxPassagiereTargetFC);
            FpPlane.Add(qPlane.TargetZustand);
            FpPlane.Add(qPlane.WorstZustand);

            /* The flight plan and the gate each flight got: a flight that finds no gate at the
               home airport costs image, so peers that planned gates differently diverge. */
            const CFlugplan &qPlan = qPlane.Flugplan;
            for (SLONG e = 0; e < qPlan.Flug.AnzEntries(); e++) {
                const CFlugplanEintrag &qFlight = qPlan.Flug[e];
                if (qFlight.ObjectType == 0) {
                    continue;
                }
                FpPlane.Add(e);
                FpPlane.Add(qFlight.ObjectType);
                FpPlane.Add(qFlight.ObjectId);
                FpPlane.Add(qFlight.Startdate);
                FpPlane.Add(qFlight.Startzeit);
                FpPlane.Add(qFlight.Gate);
            }
        }

        /* The player's own contracts: which orders and freight it holds, how far each is
           planned and done. The daily penalty and bonus bookings follow from these. */
        for (SLONG d = 0; d < qPlayer.Auftraege.AnzEntries(); d++) {
            if (qPlayer.Auftraege.IsInAlbum(d) == 0) {
                continue;
            }
            const CAuftrag &qOrder = qPlayer.Auftraege[d];
            FpOrder.Add(d);
            FpOrder.Add(qOrder.VonCity);
            FpOrder.Add(qOrder.NachCity);
            FpOrder.Add(qOrder.Date);
            FpOrder.Add(qOrder.BisDate);
            FpOrder.Add(qOrder.InPlan);
            FpOrder.Add(qOrder.Okay);
            FpOrder.Add(qOrder.Praemie);
            FpOrder.Add(qOrder.Strafe);
        }
        for (SLONG d = 0; d < qPlayer.Frachten.AnzEntries(); d++) {
            if (qPlayer.Frachten.IsInAlbum(d) == 0) {
                continue;
            }
            const CFracht &qFreight = qPlayer.Frachten[d];
            FpOrder.Add(d);
            FpOrder.Add(qFreight.VonCity);
            FpOrder.Add(qFreight.NachCity);
            FpOrder.Add(qFreight.Tons);
            FpOrder.Add(qFreight.TonsOpen);
            FpOrder.Add(qFreight.TonsLeft);
            FpOrder.Add(qFreight.Praemie);
        }

        FpMisc.Add(qPlayer.Tank);
        FpMisc.Add(qPlayer.TankInhalt);
        FpMisc.Add(qPlayer.TankOpen);
        /* KerosinKind is not hashed: it only remembers which kind the buy menu offers first.
           Kerosin bought outside the tank counts as quality 1 either way, so it decides
           nothing - but a human opening the dealer's dialog would look like a desync. */

        /* Sabotage state: a sabotaged office and the security measures decide what the next
           saboteur gets away with. */
        FpMisc.Add(qPlayer.OfficeState);
        FpMisc.Add(qPlayer.SecurityFlags);

        /* A strike delays the player's departures on every peer. How it ended is left out: its
           owner's peer clears that once the player has been told. */
        FpMisc.Add(qPlayer.StrikeHours);
        FpMisc.Add(qPlayer.StrikePlanned);
        FpMisc.Add(qPlayer.StrikeEndCountdown);

        FpMisc.Add(qPlayer.Gates.NumRented);
        for (SLONG d = 0; d < qPlayer.Gates.Gates.AnzEntries(); d++) {
            FpMisc.Add(qPlayer.Gates.Gates[d].Nummer);
        }

        /* Staff drives the salary booked every night, so a disagreement about who works for
           whom, or for how much, surfaces as money a day later - better to see it directly.
           Happiness decides who quits during the night. */
        SLONG Staff = 0;
        for (SLONG d = 0; d < SLONG(Workers.Workers.AnzEntries()); d++) {
            if (Workers.Workers[d].Employer == c) {
                FpStaff.Add(d);
                FpStaff.Add(Workers.Workers[d].Gehalt);
                FpStaff.Add(Workers.Workers[d].Happyness);
                Staff++;
            }
        }

        for (const Fingerprint *Part : {&FpCity, &FpRoute, &FpStock, &FpPlane, &FpOrder, &FpMisc, &FpStaff}) {
            Fp.Add(Part->Get());
        }

        AT_Log("FP  %s day=%ld t=%ld p=%ld owner=%ld out=%ld money=%lld credit=%lld image=%ld planes=%ld branches=%ld routes=%ld orders=%ld gates=%ld "
               "shares=%ld staff=%ld hcity=%08lx hroute=%08lx hstock=%08lx hplane=%08lx horder=%08lx hmisc=%08lx hstaff=%08lx hash=%08lx",
               When, static_cast<long>(Sim.Date), static_cast<long>(Sim.Time), static_cast<long>(c), static_cast<long>(qPlayer.Owner),
               static_cast<long>(qPlayer.IsOut), static_cast<long long>(qPlayer.Money), static_cast<long long>(qPlayer.Credit),
               static_cast<long>(qPlayer.Image), static_cast<long>(qPlayer.Planes.AnzEntries()), static_cast<long>(Branches), static_cast<long>(Routes),
               static_cast<long>(qPlayer.Auftraege.AnzEntries()), static_cast<long>(qPlayer.Gates.Gates.AnzEntries()), static_cast<long>(qPlayer.AnzAktien),
               static_cast<long>(Staff), static_cast<unsigned long>(FpCity.Get()), static_cast<unsigned long>(FpRoute.Get()),
               static_cast<unsigned long>(FpStock.Get()), static_cast<unsigned long>(FpPlane.Get()), static_cast<unsigned long>(FpOrder.Get()),
               static_cast<unsigned long>(FpMisc.Get()), static_cast<unsigned long>(FpStaff.Get()), static_cast<unsigned long>(Fp.Get()));
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

    /* The weekday decides which rooms are closed. */
    Pool.Add(Sim.Weekday);

    /* Today's kerosine price: it goes into the cost of every flight. */
    Pool.Add(Sim.Kerosin);

    /* The applicants waiting for work: the pool is shared, and a hire names a worker by his
       place in it, so the peers must agree on who stands where - names included, or two
       players cannot talk about the same person. Each employer's own staff is hashed with the
       player above. */
    SLONG Applicants = 0;
    for (SLONG d = 0; d < SLONG(Workers.Workers.AnzEntries()); d++) {
        const CWorker &qWorker = Workers.Workers[d];
        Pool.Add(d);
        Pool.Add(qWorker.Employer);
        Pool.Add(qWorker.Typ);
        Pool.Add(qWorker.Talent);
        Pool.Add(qWorker.OriginalGehalt);
        for (const char *Name = qWorker.Name.c_str(); *Name != 0; Name++) {
            Pool.Add(*Name);
        }
        if (qWorker.Employer == WORKER_RESERVE || qWorker.Employer == WORKER_JOBLESS) {
            Applicants++;
        }
    }

    /* The items lying around the airport: each is there once a day for whoever finds it first,
       so every peer must agree on what is still there. */
    Pool.Add(Sim.ItemGlove);
    Pool.Add(Sim.ItemClips);
    Pool.Add(Sim.ItemGlue);
    Pool.Add(Sim.ItemPostcard);
    Pool.Add(Sim.ItemKohle);
    Pool.Add(Sim.ItemParfuem);
    Pool.Add(Sim.ItemZange);

    AT_Log("FP  %s day=%ld t=%ld pool lma=%ld rba=%ld fracht=%ld ausland=%ld usedplanes=%ld applicants=%ld expand=%ld hash=%08lx", When, static_cast<long>(Sim.Date),
           static_cast<long>(Sim.Time), static_cast<long>(LastMinuteAuftraege.GetNumUsed()), static_cast<long>(ReisebueroAuftraege.GetNumUsed()),
           static_cast<long>(gFrachten.GetNumUsed()), static_cast<long>(AuslandsAuftraege.size()), static_cast<long>(UsedPlanes), static_cast<long>(Applicants),
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
