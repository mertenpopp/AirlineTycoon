//============================================================================================
// PNet.cpp : Routinen zum verwalten der Spieler im Netzwerk
//============================================================================================
#include "AtNet.h"
#include "class.h"
#include "global.h"

#define AT_Log(...) AT_Log_I("Player", __VA_ARGS__)

extern bool bgIsLoadingSavegame;

inline bool needToSyncPlayer(const PLAYER &qPlayer, bool onlyBots) {
    if (qPlayer.IsOut != 0) {
        return false;
    }
    if (qPlayer.Owner == 0 && !onlyBots) {
        return true;
    }
    if ((Sim.bIsHost != 0) && qPlayer.Owner == 1) {
        return true;
    }
    return false;
}

//--------------------------------------------------------------------------------------------
// Returns the number of players on which this computer will send information:
//--------------------------------------------------------------------------------------------
inline SLONG NetSynchronizeGetNum(bool onlyBots) {
    SLONG n = 0;
    for (SLONG c = 0; c < 4; c++) {
        PLAYER &qPlayer = Sim.Players.Players[c];
        if (needToSyncPlayer(qPlayer, onlyBots)) {
            n++;
        }
    }
    return (n);
}

//--------------------------------------------------------------------------------------------
// Sends the data concerning image to other players:
//--------------------------------------------------------------------------------------------
void PLAYER::NetSynchronizeImage() {
    TEAKFILE Message;
    SLONG c = 0;

    Message.Announce(1024);

    Message << ATNET_SYNC_IMAGE << NetSynchronizeGetNum(false);

    // Für den lokalen Spieler und (wenn dies der Server ist) auch für Computerspieler:
    for (c = 0; c < 4; c++) {
        PLAYER &qPlayer = Sim.Players.Players[c];

        if (needToSyncPlayer(qPlayer, false)) {
            SLONG d = 0;

            Message << c << qPlayer.Image << qPlayer.ImageGotWorse;

            for (d = 0; d < 4; d++) {
                Message << qPlayer.Sympathie[d];
            }

            for (d = Routen.AnzEntries() - 1; d >= 0; d--) {
                Message << qPlayer.RentRouten.RentRouten[d].Image;
            }
            for (d = Cities.AnzEntries() - 1; d >= 0; d--) {
                Message << qPlayer.RentCities.RentCities[d].Image;
            }
        }
    }

    SIM::SendMemFile(Message);
}

//--------------------------------------------------------------------------------------------
// Sends the data concerning money to other players:
//--------------------------------------------------------------------------------------------
void PLAYER::NetSynchronizeMoney() {
    TEAKFILE Message;
    SLONG c = 0;

    Message.Announce(256);

    Message << ATNET_SYNC_MONEY << NetSynchronizeGetNum(false);

    // Für den lokalen Spieler und (wenn dies der Server ist) auch für Computerspieler:
    for (c = 0; c < 4; c++) {
        PLAYER &qPlayer = Sim.Players.Players[c];

        if (needToSyncPlayer(qPlayer, false)) {
            SLONG d = 0;

            Message << c;

            Message << qPlayer.Money << qPlayer.Credit << qPlayer.Bonus << qPlayer.AnzAktien << qPlayer.MaxAktien << qPlayer.TrustedDividende
                    << qPlayer.Dividende;

            for (d = 0; d < 4; d++) {
                Message << qPlayer.OwnsAktien[d] << qPlayer.AktienWert[d];
            }
            for (d = 0; d < 10; d++) {
                Message << qPlayer.Kurse[d];
            }
        }
    }

    SIM::SendMemFile(Message);
}

//--------------------------------------------------------------------------------------------
// Sends the data concerning routes to other players:
//--------------------------------------------------------------------------------------------
void PLAYER::NetSynchronizeRoutes() {
    TEAKFILE Message;
    SLONG c = 0;

    Message.Announce(1024);

    Message << ATNET_SYNC_ROUTES << NetSynchronizeGetNum(false);

    // Für den lokalen Spieler und (wenn dies der Server ist) auch für Computerspieler:
    for (c = 0; c < 4; c++) {
        PLAYER &qPlayer = Sim.Players.Players[c];

        if (needToSyncPlayer(qPlayer, false)) {
            Message << c;

            for (SLONG d = Routen.AnzEntries() - 1; d >= 0; d--) {
                Message << qPlayer.RentRouten.RentRouten[d].Rang << qPlayer.RentRouten.RentRouten[d].LastFlown << qPlayer.RentRouten.RentRouten[d].Image
                        << qPlayer.RentRouten.RentRouten[d].Miete << qPlayer.RentRouten.RentRouten[d].Ticketpreis
                        << qPlayer.RentRouten.RentRouten[d].TicketpreisFC << qPlayer.RentRouten.RentRouten[d].TageMitVerlust
                        << qPlayer.RentRouten.RentRouten[d].TageMitGering;
            }
        }
    }

    SIM::SendMemFile(Message);
}

//--------------------------------------------------------------------------------------------
// Läßt einen Spieler die Ticketpreis verändern:
//--------------------------------------------------------------------------------------------
void PLAYER::NetRouteUpdateTicketpreise(SLONG RouteId, SLONG Ticketpreis, SLONG TicketpreisFC) const {
    SIM::SendSimpleMessage(ATNET_SYNCROUTECHANGE, 0, PlayerNum, RouteId, Ticketpreis, TicketpreisFC);
}

//--------------------------------------------------------------------------------------------
// Sends the data concerning flags to other players:
//--------------------------------------------------------------------------------------------
void PLAYER::NetSynchronizeFlags() {
    TEAKFILE Message;
    SLONG c = 0;

    Message.Announce(64);

    Message << ATNET_SYNC_FLAGS << NetSynchronizeGetNum(false);

    // Für den lokalen Spieler und (wenn dies der Server ist) auch für Computerspieler:
    for (c = 0; c < 4; c++) {
        PLAYER &qPlayer = Sim.Players.Players[c];

        if (needToSyncPlayer(qPlayer, false)) {
            Message << c;

            Message << qPlayer.SickTokay << qPlayer.RunningToToilet << qPlayer.PlayerSmoking << qPlayer.Stunned << qPlayer.OfficeState << qPlayer.Koffein
                    << qPlayer.NumFlights << qPlayer.WalkSpeed << qPlayer.WerbeBroschuere << qPlayer.TelephoneDown << qPlayer.Presseerklaerung
                    << qPlayer.SecurityFlags << qPlayer.PlayerStinking << qPlayer.RocketFlags << qPlayer.LastRocketFlags;
        }
    }

    SIM::SendMemFile(Message);
}

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
void PLAYER::NetSynchronizeItems() {
    TEAKFILE Message;
    SLONG c = 0;

    Message.Announce(64);

    Message << ATNET_SYNC_ITEMS << NetSynchronizeGetNum(false);

    // Für den lokalen Spieler und (wenn dies der Server ist) auch für Computerspieler:
    for (c = 0; c < 4; c++) {
        PLAYER &qPlayer = Sim.Players.Players[c];

        if (needToSyncPlayer(qPlayer, false)) {
            Message << c;

            for (SLONG d = 0; d < 6; d++) {
                Message << qPlayer.Items[d];
            }
        }
    }

    SIM::SendMemFile(Message);
}

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
void PLAYER::NetSynchronizePlanes() {
    if (Sim.bIsHost != 0) {
        TEAKFILE Message;
        SLONG c = 0;

        Message.Announce(1024);

        Message << ATNET_SYNC_PLANES << NetSynchronizeGetNum(true);

        // Wenn dies der Server ist für alle Computerspieler:
        for (c = 0; c < 4; c++) {
            PLAYER &qPlayer = Sim.Players.Players[c];

            if (needToSyncPlayer(qPlayer, true)) {
                Message << c;
                Message << qPlayer.Planes << qPlayer.Auftraege << qPlayer.Frachten << qPlayer.RentCities;
            }
        }

        SIM::SendMemFile(Message);
    }
}

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
void PLAYER::NetSynchronizeMeeting() {
    TEAKFILE Message;
    SLONG c = 0;

    Message.Announce(64);

    Message << ATNET_SYNC_MEETING << NetSynchronizeGetNum(false);

    // Wenn dies der Server ist für alle Computerspieler:
    for (c = 0; c < 4; c++) {
        PLAYER &qPlayer = Sim.Players.Players[c];

        if (needToSyncPlayer(qPlayer, false)) {
            Message << c;
            Message << qPlayer.ArabTrust << qPlayer.ArabMode << qPlayer.ArabMode2 << qPlayer.ArabMode3 << qPlayer.ArabActive;
            Message << qPlayer.ArabOpfer << qPlayer.ArabOpfer2 << qPlayer.ArabOpfer3 << qPlayer.ArabPlane << qPlayer.ArabHints;
            Message << qPlayer.ArabTimeout << qPlayer.NumPassengers << qPlayer.NumFracht;
        }
    }

    Message << Sim.bIsHost;
    if (Sim.bIsHost != 0) {
        Message << Sim.SabotageActs;
    }

    SIM::SendMemFile(Message);
}

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
void PLAYER::NetSynchronizeSabotage() const {
    TEAKFILE Message;

    Message.Announce(128);

    Message << ATNET_SABOTAGE_ARAB << PlayerNum;

    Message << ArabOpfer << ArabMode << ArabActive << ArabPlane << ArabOpfer2 << ArabMode2 << ArabOpfer3 << ArabMode3 << ArabTimeout;

    SIM::SendMemFile(Message);
}

//--------------------------------------------------------------------------------------------
// Transfers a flightplan
//--------------------------------------------------------------------------------------------
void PLAYER::NetUpdateFlightplan(SLONG PlaneId) {
    TEAKFILE Message;

    Message.Announce(1024);

    Message << ATNET_FP_UPDATE;
    Message << PlaneId << PlayerNum;
    Message << Planes[PlaneId].Flugplan;

    SIM::SendMemFile(Message);
}

//--------------------------------------------------------------------------------------------
// Player took an order flight ==> Tell the others:
// Type: 1 - LastMinute
// Type: 2 - Reisebüro
// Type: 3 - Fracht
// Type: 4 - Ausland, City = CityIndex
// This function only handle the blackboard in the room; It does not update the player's data
//--------------------------------------------------------------------------------------------
void PLAYER::NetUpdateTook(SLONG Type, SLONG Index, SLONG City) const {
    TEAKFILE Message;

    Message.Announce(128);

    Message << ATNET_PLAYER_TOOK << PlayerNum << Type << Index << City;

    SIM::SendMemFile(Message);
}

//--------------------------------------------------------------------------------------------
// Let's the player take the order on other computers, too:
//--------------------------------------------------------------------------------------------
void PLAYER::NetUpdateOrder(const CAuftrag &auftrag) const {
    TEAKFILE Message;

    Message.Announce(128);

    Message << ATNET_TAKE_ORDER;
    Message << PlayerNum << auftrag;

    SIM::SendMemFile(Message);
}

//--------------------------------------------------------------------------------------------
// Let's the player take the order on other computers, too:
//--------------------------------------------------------------------------------------------
void PLAYER::NetUpdateFreightOrder(const CFracht &auftrag) const {
    TEAKFILE Message;

    Message.Announce(128);

    Message << ATNET_TAKE_FREIGHT;
    Message << PlayerNum << auftrag;

    SIM::SendMemFile(Message);
}

//--------------------------------------------------------------------------------------------
// Updates the rental data of one route and back:
//--------------------------------------------------------------------------------------------
void PLAYER::NetUpdateRentRoute(SLONG Route1Id, SLONG Route2Id) {
    TEAKFILE Message;

    Message.Announce(128);

    Message << ATNET_TAKE_ROUTE;

    Message << PlayerNum << Route1Id << Route2Id;
    Message << RentRouten.RentRouten[Route1Id];
    Message << RentRouten.RentRouten[Route2Id];

    SIM::SendMemFile(Message);
}

//--------------------------------------------------------------------------------------------
// Updates the Kooperation status:
//--------------------------------------------------------------------------------------------
void PLAYER::NetSynchronizeKooperation() const {
    TEAKFILE Message;

    Message.Announce(128);

    Message << ATNET_DIALOG_KOOP;

    Message << PlayerNum << Kooperation;

    SIM::SendMemFile(Message);
}

//--------------------------------------------------------------------------------------------
// Sends what this peer's people earn and how they feel:
//--------------------------------------------------------------------------------------------
/* Salaries and happiness are changed by every peer alike - at night, and when a salary change or
   the end of a strike is announced - so they should never drift. But nothing repaired them when
   they did: a strike that started an hour apart once left a whole staff 10 happiness apart for
   the rest of the game, which then decides who quits and when the next strike comes. The owner
   resends them every hour, as it does money, image and routes. */
void PLAYER::NetSynchronizeStaff() {
    TEAKFILE Message;

    Message.Announce(1024);

    Message << ATNET_SYNC_STAFF << NetSynchronizeGetNum(false);

    for (SLONG c = 0; c < 4; c++) {
        PLAYER &qPlayer = Sim.Players.Players[c];

        if (needToSyncPlayer(qPlayer, false)) {
            SLONG Anz = 0;
            for (SLONG d = 0; d < Workers.Workers.AnzEntries(); d++) {
                if (Workers.Workers[d].Employer == c) {
                    Anz++;
                }
            }

            Message << c << Anz;

            for (SLONG d = 0; d < Workers.Workers.AnzEntries(); d++) {
                if (Workers.Workers[d].Employer == c) {
                    Message << d << Workers.Workers[d].Gehalt << Workers.Workers[d].Happyness;
                }
            }
        }
    }

    SIM::SendMemFile(Message);
}

//--------------------------------------------------------------------------------------------
// Changes how much this player likes another one, and tells the other peers:
//--------------------------------------------------------------------------------------------
/* How a player feels about the others belongs to that player, so a dialog may only change it
   where it happens - and the owner's next ATNET_SYNC_IMAGE then overwrote it. On a client,
   being nice to a bot therefore had no effect at all, while the host's human was heard.
   ATNET_ADD_SYMPATHIE exists for this and had no sender. */
void PLAYER::NetAddSympathie(SLONG Target, SLONG Delta) {
    if (Target < 0 || Target >= 4) {
        return;
    }

    Sympathie[Target] += Delta;
    Limit(static_cast<SLONG>(-1000), Sympathie[Target], static_cast<SLONG>(1000));

    if (Sim.bNetwork != 0) {
        SIM::SendSimpleMessage(ATNET_ADD_SYMPATHIE, 0, PlayerNum, Target, Delta);
    }
}

//--------------------------------------------------------------------------------------------
// Updates the total number of workers and which planes they work on:
//--------------------------------------------------------------------------------------------
void PLAYER::NetUpdateWorkers() {
    TEAKFILE Message;
    SLONG m = 0;
    SLONG n = 0;
    SLONG c = 0;

    if (bgIsLoadingSavegame) {
        return;
    }

    /* Callers rely on this side effect whether or not anything gets sent - also in single
       player - so it stays ahead of the ownership check. */
    UpdateStatistics();

    if (!NetIsAuthoritative()) {
        return;
    }

    Message.Announce(128);

    Message << ATNET_PERSONNEL;

    m = static_cast<SLONG>(Statistiken[STAT_ZUFR_PERSONAL].GetAtPastDay(0));
    n = static_cast<SLONG>(Statistiken[STAT_MITARBEITER].GetAtPastDay(0));

    Message << PlayerNum << m << n;

    for (c = 0; c < Planes.AnzEntries(); c++) {
        if (Planes.IsInAlbum(c) != 0) {
            Message << c;
            Message << Planes[c].AnzPiloten;
            Message << Planes[c].AnzBegleiter;
            Message << Planes[c].PersonalQuality;
        }
    }

    c = -1;
    Message << c;

    SIM::SendMemFile(Message);

    if (Owner == 0 || ((Owner == 1) && (Sim.bIsHost != 0) && !RobotUse(ROBOT_USE_FAKE_PERSONAL))) {
        /*
        SLONG c = 0;
        SLONG d = 0;

        for (c = d = 0; c < Workers.Workers.AnzEntries(); c++) {
            if (Workers.Workers[c].Employer == PlayerNum) {
                d += Workers.Workers[c].Gehalt;
            }
        }
        Statistiken[STAT_GEHALT].SetAtPastDay(-d);
        */

        SIM::SendSimpleMessage(ATNET_SYNCGEHALT, 0, PlayerNum, Statistiken[STAT_GEHALT].GetAtPastDay(0));
    }
}

//--------------------------------------------------------------------------------------------
// Organizes saving in the network:
//--------------------------------------------------------------------------------------------
void PLAYER::NetSave(DWORD UniqueGameId, SLONG CursorY, const CString &Name) {
    TEAKFILE Message;

    Message.Announce(128);

    Message << ATNET_IO_SAVE;

    Message << UniqueGameId << CursorY << Name;

    SIM::SendMemFile(Message);
}

//--------------------------------------------------------------------------------------------
// Broadcasts a plane's properties:
//--------------------------------------------------------------------------------------------
/* Only the peer that owns a player may broadcast its state: its own human's machine, or the
   host for the bots. MapWorkers() broadcasts plane properties and staff for all of a player's
   planes, and it also runs when a peer merely applies a hire it received from the network - so
   without this check a client echoed a bot's plane settings back to the host from its own,
   possibly older copy, and could revert a refit the bot had just ordered. */
bool PLAYER::NetIsAuthoritative() const { return (Sim.bNetwork != 0) && (Owner == 0 || (Owner == 1 && Sim.bIsHost != 0)); }

void PLAYER::NetUpdatePlaneProps(SLONG PlaneId) {
    TEAKFILE Message;

    Message.Announce(128);

    if (bgIsLoadingSavegame || !NetIsAuthoritative()) {
        return;
    }

    Message << ATNET_PLANEPROPS;

    Message << PlayerNum << PlaneId;
    Message << MechMode;

    if (PlaneId != -1) {
        CPlane &qPlane = Planes[PlaneId];

        Message << qPlane.Sitze << qPlane.SitzeTarget << qPlane.Essen << qPlane.EssenTarget << qPlane.Tabletts << qPlane.TablettsTarget << qPlane.Deco
                << qPlane.DecoTarget << qPlane.Triebwerk << qPlane.TriebwerkTarget << qPlane.Reifen << qPlane.ReifenTarget << qPlane.Elektronik
                << qPlane.ElektronikTarget << qPlane.Sicherheit << qPlane.SicherheitTarget << qPlane.MaxPassagiereTarget << qPlane.MaxPassagiereTargetFC;

        Message << qPlane.WorstZustand << qPlane.Zustand << qPlane.TargetZustand;
        Message << qPlane.AnzBegleiter << qPlane.MaxBegleiter;
    }

    SIM::SendMemFile(Message);
}

//--------------------------------------------------------------------------------------------
// Resends the kerosine state of this peer's players:
//--------------------------------------------------------------------------------------------
/* The tank drains with every flight, and every peer works that out for itself; the state is only
   broadcast when somebody buys a tank, buys kerosine or opens it. So a single flight that went
   differently for a moment left the tanks apart for the rest of the game - and what is in the
   tank decides what the next flight costs. The owner resends it every hour, as it does money,
   image, routes and staff. */
void PLAYER::NetSynchronizeKerosin() {
    for (SLONG c = 0; c < 4; c++) {
        PLAYER &qPlayer = Sim.Players.Players[c];

        if (needToSyncPlayer(qPlayer, false)) {
            qPlayer.NetUpdateKerosin();
        }
    }
}

//--------------------------------------------------------------------------------------------
// Broadcasts a players kerosine state:
//--------------------------------------------------------------------------------------------
void PLAYER::NetUpdateKerosin() const {
    TEAKFILE Message;

    Message.Announce(128);

    Message << ATNET_SYNCKEROSIN;
    Message << PlayerNum << Tank << TankOpen << TankInhalt << KerosinQuali << KerosinKind << TankPreis;

    SIM::SendMemFile(Message);
}

//--------------------------------------------------------------------------------------------
// Broadcasts a xplane buy:
//--------------------------------------------------------------------------------------------
void PLAYER::NetBuyXPlane(SLONG Anzahl, CXPlane &plane) const {
    TEAKFILE Message;

    Message.Announce(128);

    Message << ATNET_BUY_NEWX;
    Message << PlayerNum << Anzahl << plane;

    SIM::SendMemFile(Message);
}

//--------------------------------------------------------------------------------------------
// Broadcasts robot state:
//--------------------------------------------------------------------------------------------
void PLAYER::NetSyncRobot(SLONG Par1, SLONG Par2) const {
    // AT_Log("%s NetSyncRobot(): Par1 = %d, Par2 = %d", AirlineX.c_str(), Par1, Par2);

    TEAKFILE Message;

    Message.Announce(128);
    Message << ATNET_ROBOT_EXECUTE << PlayerNum << Par1 << Par2;

    for (SLONG c = 0; c < 4; c++) {
        Message << Sympathie[c];
    }
    for (SLONG c = 0; c < RobotActions.AnzEntries(); c++) {
        Message << RobotActions[c];
    }

    SIM::SendMemFile(Message);
}
