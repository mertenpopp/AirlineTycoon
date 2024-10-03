#ifndef GAMEMECHANIC_H_
#define GAMEMECHANIC_H_

#include "compat.h"

class GameMechanic {
  public:
    /* Flights */
    static bool takeFlightJob(PLAYER &qPlayer, SLONG jobId, SLONG &outObjectId);
    static bool takeLastMinuteJob(PLAYER &qPlayer, SLONG jobId, SLONG &outObjectId);
    static bool takeFreightJob(PLAYER &qPlayer, SLONG jobId, SLONG &outObjectId);
    static bool canCallInternational(PLAYER &qPlayer, SLONG cityId);
    static bool takeInternationalFlightJob(PLAYER &qPlayer, SLONG cityId, SLONG jobId, SLONG &outObjectId);
    static bool takeInternationalFreightJob(PLAYER &qPlayer, SLONG cityId, SLONG jobId, SLONG &outObjectId);
    static bool killFlightJob(PLAYER &qPlayer, SLONG par1, bool payFine);
    static bool killFreightJob(PLAYER &qPlayer, SLONG par1, bool payFine);
    static bool killFlightPlan(PLAYER &qPlayer, SLONG par1);
    static bool killFlightPlanFrom(PLAYER &qPlayer, SLONG planeId, SLONG date, SLONG hours);
    static bool refillFlightJobs(SLONG cityNum, SLONG minimum = 0);
    static bool planFlightJob(PLAYER &qPlayer, SLONG planeID, SLONG objectID, SLONG date, SLONG time);
    static bool planFreightJob(PLAYER &qPlayer, SLONG planeID, SLONG objectID, SLONG date, SLONG time);
    static bool planRouteJob(PLAYER &qPlayer, SLONG planeID, SLONG objectID, SLONG date, SLONG time);

  private:
    static bool _planFlightJob(PLAYER &qPlayer, SLONG planeID, SLONG objectID, SLONG objectType, SLONG date, SLONG time);
};

#endif // GAMEMECHANIC_H_