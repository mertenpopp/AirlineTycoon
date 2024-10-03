#include "GameMechanic.h"

#include "compat_global.h"
#include "compat_misc.h"

bool GameMechanic::takeFlightJob(PLAYER &qPlayer, SLONG jobId, SLONG &outObjectId) {
    outObjectId = -1;
    if (!ReisebueroAuftraege.IsInAlbum(jobId)) {
        redprintf("GameMechanic::takeFlightJob: Invalid jobId (%ld).", jobId);
        return false;
    }

    auto &qAuftrag = ReisebueroAuftraege[jobId];
    if (qAuftrag.Praemie <= 0) {
        return false;
    }
    if (qPlayer.Auftraege.GetNumFree() < 3) {
        qPlayer.Auftraege.ReSize(qPlayer.Auftraege.AnzEntries() + 10);
    }

    outObjectId = (qPlayer.Auftraege += qAuftrag);

    qAuftrag.Praemie = -1000;

    return true;
}

bool GameMechanic::takeLastMinuteJob(PLAYER &qPlayer, SLONG jobId, SLONG &outObjectId) {
    outObjectId = -1;
    if (!LastMinuteAuftraege.IsInAlbum(jobId)) {
        redprintf("GameMechanic::takeLastMinuteJob: Invalid jobId (%ld).", jobId);
        return false;
    }

    auto &qAuftrag = LastMinuteAuftraege[jobId];
    if (qAuftrag.Praemie <= 0) {
        return false;
    }
    if (qPlayer.Auftraege.GetNumFree() < 3) {
        qPlayer.Auftraege.ReSize(qPlayer.Auftraege.AnzEntries() + 10);
    }

    outObjectId = (qPlayer.Auftraege += qAuftrag);

    qAuftrag.Praemie = -1000;

    return true;
}

bool GameMechanic::takeFreightJob(PLAYER &qPlayer, SLONG jobId, SLONG &outObjectId) {
    outObjectId = -1;
    if (!gFrachten.IsInAlbum(jobId)) {
        redprintf("GameMechanic::takeFreightJob: Invalid jobId (%ld).", jobId);
        return false;
    }

    auto &qAuftrag = gFrachten[jobId];
    if (qAuftrag.Praemie < 0) {
        return false;
    }
    if (qPlayer.Frachten.GetNumFree() < 3) {
        qPlayer.Frachten.ReSize(qPlayer.Auftraege.AnzEntries() + 10);
    }

    outObjectId = (qPlayer.Frachten += qAuftrag);

    qAuftrag.Praemie = -1000;

    return true;
}

bool GameMechanic::takeInternationalFlightJob(PLAYER &qPlayer, SLONG cityId, SLONG jobId, SLONG &outObjectId) {
    outObjectId = -1;
    if (cityId < 0 || cityId >= AuslandsAuftraege.size()) {
        redprintf("GameMechanic::takeInternationalFlightJob: Invalid cityId (%ld).", cityId);
        return false;
    }
    if (!AuslandsAuftraege[cityId].IsInAlbum(jobId)) {
        redprintf("GameMechanic::takeInternationalFlightJob: Invalid jobId (%ld).", jobId);
        return false;
    }

    auto &qAuftrag = AuslandsAuftraege[cityId][jobId];
    if (qAuftrag.Praemie <= 0) {
        return false;
    }
    if (qPlayer.Auftraege.GetNumFree() < 3) {
        qPlayer.Auftraege.ReSize(qPlayer.Auftraege.AnzEntries() + 10);
    }

    outObjectId = (qPlayer.Auftraege += qAuftrag);

    qAuftrag.Praemie = -1000;

    return true;
}

bool GameMechanic::takeInternationalFreightJob(PLAYER &qPlayer, SLONG cityId, SLONG jobId, SLONG &outObjectId) {
    outObjectId = -1;
    if (cityId < 0 || cityId >= AuslandsFrachten.size()) {
        redprintf("GameMechanic::takeInternationalFreightJob: Invalid cityId (%ld).", cityId);
        return false;
    }
    if (!AuslandsFrachten[cityId].IsInAlbum(jobId)) {
        redprintf("GameMechanic::takeInternationalFreightJob: Invalid jobId (%ld).", jobId);
        return false;
    }

    auto &qFracht = AuslandsFrachten[cityId][jobId];
    if (qFracht.Praemie < 0) {
        return false;
    }
    if (qPlayer.Frachten.GetNumFree() < 3) {
        qPlayer.Frachten.ReSize(qPlayer.Frachten.AnzEntries() + 10);
    }

    outObjectId = (qPlayer.Frachten += qFracht);

    qFracht.Praemie = -1000;

    return true;
}

bool GameMechanic::killFlightJob(PLAYER &qPlayer, SLONG par1, bool payFine) {
    if (!qPlayer.Auftraege.IsInAlbum(par1)) {
        redprintf("GameMechanic::killFlightJob: Invalid par1 (%ld).", par1);
        return false;
    }

    qPlayer.Auftraege -= par1;

    return true;
}

bool GameMechanic::killFreightJob(PLAYER &qPlayer, SLONG par1, bool payFine) {
    if (!qPlayer.Frachten.IsInAlbum(par1)) {
        redprintf("GameMechanic::killFreightJob: Invalid par1 (%ld).", par1);
        return false;
    }

    // qPlayer.Frachten-= par1;
    qPlayer.Frachten[par1].Okay = -1;
    qPlayer.Frachten[par1].InPlan = -1;

    return true;
}

bool GameMechanic::killFlightPlan(PLAYER &qPlayer, SLONG planeId) { return killFlightPlanFrom(qPlayer, planeId, Sim.Date, Sim.GetHour() + 2); }

bool GameMechanic::killFlightPlanFrom(PLAYER &qPlayer, SLONG planeId, SLONG date, SLONG hours) {
    if (!qPlayer.Planes.IsInAlbum(planeId)) {
        redprintf("GameMechanic::killFlightPlanFrom: Invalid planeId (%ld).", planeId);
        return false;
    }
    if (date < Sim.Date) {
        redprintf("GameMechanic::killFlightPlanFrom: Date must not be in the past (%ld).", date);
        return false;
    }
    if (hours < 0) {
        redprintf("GameMechanic::killFlightPlanFrom: Offset must not be negative (%ld).", hours);
        return false;
    }
    if (date == Sim.Date && hours <= Sim.GetHour() + 1) {
        hprintf("GameMechanic::killFlightPlanFrom: Increasing hours to current time + 2 (was %ld).", hours);
        hours = Sim.GetHour() + 2;
    }

    CFlugplan &qPlan = qPlayer.Planes[planeId].Flugplan;

    for (SLONG c = qPlan.Flug.AnzEntries() - 1; c >= 0; c--) {
        auto &qFPE = qPlan.Flug[c];
        if (qFPE.ObjectType != 0) {
            if (qFPE.Startdate > date || (qFPE.Startdate == date && qFPE.Startzeit >= hours)) {
                qFPE.ObjectType = 0;
            }
        }
    }

    qPlan.UpdateNextFlight();
    qPlan.UpdateNextStart();

    qPlayer.UpdateAuftragsUsage();
    qPlayer.UpdateFrachtauftragsUsage();
    qPlayer.Planes[planeId].CheckFlugplaene(qPlayer.PlayerNum, FALSE);

    return true;
}

bool GameMechanic::refillFlightJobs(SLONG cityNum, SLONG minimum) {
    if (cityNum < 0 || cityNum >= AuslandsAuftraege.size()) {
        redprintf("GameMechanic::refillFlightJobs: Invalid cityNum (%ld).", cityNum);
        return false;
    }

    AuslandsAuftraege[cityNum].RefillForAusland(cityNum, minimum);
    AuslandsFrachten[cityNum].RefillForAusland(cityNum, minimum);

    return true;
}

bool GameMechanic::planFlightJob(PLAYER &qPlayer, SLONG planeID, SLONG objectID, SLONG date, SLONG time) {
    if (!qPlayer.Auftraege.IsInAlbum(objectID)) {
        redprintf("GameMechanic::planFlightJob: Invalid job index (%ld).", objectID);
        return false;
    }
    if (qPlayer.Auftraege[objectID].InPlan != 0) {
        redprintf("GameMechanic::planFlightJob: Job was already planned (%ld).", objectID);
        return false;
    }
    if (objectID < 0x1000000) {
        objectID = qPlayer.Auftraege.GetIdFromIndex(objectID);
    }
    return _planFlightJob(qPlayer, planeID, objectID, 2, date, time);
}
bool GameMechanic::planFreightJob(PLAYER &qPlayer, SLONG planeID, SLONG objectID, SLONG date, SLONG time) {
    if (!qPlayer.Frachten.IsInAlbum(objectID)) {
        redprintf("GameMechanic::planFreightJob: Invalid job index (%ld).", objectID);
        return false;
    }
    if (objectID < 0x1000000) {
        objectID = qPlayer.Frachten.GetIdFromIndex(objectID);
    }
    return _planFlightJob(qPlayer, planeID, objectID, 4, date, time);
}
bool GameMechanic::planRouteJob(PLAYER &qPlayer, SLONG planeID, SLONG objectID, SLONG date, SLONG time) {
    if (!Routen.IsInAlbum(objectID)) {
        redprintf("GameMechanic::planRouteJob: Invalid job index (%ld).", objectID);
        return false;
    }
    if (objectID < 0x1000000) {
        objectID = Routen.GetIdFromIndex(objectID);
    }
    return _planFlightJob(qPlayer, planeID, objectID, 1, date, time);
}
bool GameMechanic::_planFlightJob(PLAYER &qPlayer, SLONG planeID, SLONG objectID, SLONG objectType, SLONG date, SLONG time) {
    if (!qPlayer.Planes.IsInAlbum(planeID)) {
        redprintf("GameMechanic::_planFlightJob: Invalid plane index (%ld).", objectID);
        return false;
    }
    if (date < Sim.Date) {
        redprintf("GameMechanic::_planFlightJob: Invalid day (too early, %ld).", date);
        return false;
    }
    if (date >= Sim.Date + 7) {
        redprintf("GameMechanic::_planFlightJob: Invalid day (too late, %ld).", date);
        return false;
    }
    if (time < 0 || (date == Sim.Date && time < Sim.GetHour() + 2)) {
        redprintf("GameMechanic::_planFlightJob: Invalid time (too early, %ld).", time);
        return false;
    }
    if (time >= 24) {
        redprintf("GameMechanic::_planFlightJob: Invalid time (%ld).", time);
        return false;
    }

    auto &qPlane = qPlayer.Planes[planeID];
    if (qPlane.Problem != 0) {
        redprintf("GameMechanic::_planFlightJob: Plane %s has a problem for the next %ld hours", (LPCTSTR)qPlane.Name, qPlane.Problem);
        return false;
    }

    auto lastIdx = qPlane.Flugplan.Flug.AnzEntries() - 1;
    CFlugplanEintrag &fpe = qPlane.Flugplan.Flug[lastIdx];

    fpe = {};
    fpe.ObjectType = objectType;
    fpe.ObjectId = objectID;
    fpe.Startdate = date;
    fpe.Startzeit = time;

    if (objectType == 1) {
        // fpe.Ticketpreis = qPlayer.RentRouten.RentRouten[Routen(objectID)].Ticketpreis;
        // fpe.TicketpreisFC = qPlayer.RentRouten.RentRouten[Routen(objectID)].TicketpreisFC;
    }

    fpe.FlightChanged();
    if (objectType != 4) {
        fpe.CalcPassengers(qPlayer.PlayerNum, qPlane);
    }
    fpe.PArrived = 0;

    qPlane.Flugplan.UpdateNextFlight();
    qPlane.Flugplan.UpdateNextStart();
    qPlane.CheckFlugplaene(qPlayer.PlayerNum);

    qPlayer.UpdateAuftragsUsage();
    qPlayer.UpdateFrachtauftragsUsage();

    return true;
}
