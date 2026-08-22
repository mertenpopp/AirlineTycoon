#ifndef CLAUDE_BOT_H_
#define CLAUDE_BOT_H_

#include "BotHelper.h"
#include "class.h"
#include "defines.h"

#include <vector>

class PLAYER;
class CFracht;

extern const SLONG kRouteAvgDays;

class ClaudeBot {
  public:
    explicit ClaudeBot(PLAYER &player);

    void RobotInit();
    void RobotPlan();
    void RobotExecuteAction();

    void setNoticedSickness() { mIsSickToday = true; }
    SLONG getNextMood();

    __int64 getMoneyAvailable() const { return qPlayer.Money; }

    /* anim state */
    bool getOnThePhone() const { return mOnThePhone > 0; }
    void decOnThePhone() { mOnThePhone--; }

    friend TEAKFILE &operator<<(TEAKFILE &File, const ClaudeBot &bot);
    friend TEAKFILE &operator>>(TEAKFILE &File, ClaudeBot &bot);

  private:
    /* A window in which a plane sits idle at a known city, bounded on both sides by a
     * flight that is already planned. Route legs fill the plan to the horizon, so these
     * are the overnight hours between the last leg of one day and the first of the next.
     *
     * Jobs are only ever placed inside such a window, and only if the return leg fits too:
     * the game shifts every following flight later when an entry overruns
     * (CPlane::UpdateFlightPlan, Planetyp.cpp:753-780), which would drag route legs into
     * the night penalty. A window bounded by a following flight also guarantees the plane
     * is flown back - the game inserts the empty return itself (Planetyp.cpp:708-750). */
    struct PlaneGap {
        PlaneTime start{};
        PlaneTime end{}; /* departure of the next planned flight */
        SLONG city{-1};  /* where the plane waits, i.e. where a job must depart */
    };

    /* One leg of a freight job placed into an idle window. `slot` indexes the parallel
     * arrays the placement was computed on, not qPlayer.Planes. */
    struct FreightLeg {
        SLONG slot{-1};
        SLONG gap{-1};
        PlaneTime start{};
        PlaneTime back{};
    };

    /* Cached view of one plane. Only the fields derived from the flight plan need
     * caching; everything copied from CPlaneType may be read in any room. */
    struct PlaneState {
        SLONG id{-1};
        PlaneTime avail{};
        SLONG city{-1};
        std::vector<PlaneGap> gaps;
    };

    /* A route we rent, cached at the route box: the global Routen array may only be read
     * there, but the ticket prices and the flight plans are set in the office. */
    struct RouteState {
        SLONG id{-1};        /* route id of the outbound direction */
        SLONG reverseId{-1}; /* route id of the return direction */
        ULONG vonCity{0};
        ULONG nachCity{0};
        SLONG distance{0};
        SLONG ticketPrice{0};
        SLONG ticketPriceFC{0};
        SLONG bedarf{0}; /* passengers still waiting in the route's pool */
        /* What the pair is worth per plane hour to the best aeroplane we own. Below
         * kMinRouteValuePerHour the pair is a castaway route: rented only because some
         * plane can reach nothing else, and reserved for exactly those planes. */
        SLONG valuePerHour{0};
        SLONG anzPax{0};  /* passengers the route wants per day, CRoute::AnzPassagiere() */
        bool pricesSet{false};
    };

    /* --- planning --- */
    void collectActions(std::vector<SLONG> &out) const;
    SLONG pickFillerAction();
    bool canUseAction(SLONG actionId) const;
    bool haveOffice() const;
    void startNewDay();

    /* --- action implementations --- */
    void executePersonal();
    void hireAdvisors();
    void executeCheckAgent2();
    void executeCheckAgent3();
    void executeOffice();
    void executeMech();
    void executeBank();
    void executeStock();
    void executeRouteBox();
    void executeAds();
    void executeUpgrades();
    void executeBuyPlane();
    void executeMuseum();
    void executeKerosinTanks();
    void executeBuyKerosin();
    /* The end-game fuel manoeuvre: how many units the tank should hold today, and whether
     * we are still inside the window in which buying them is free of the score. */
    bool inFuelPrepayWindow() const;
    SLONG fuelPrepayTarget() const;

    /* --- scheduling --- */
    SLONG scheduleRouteFlights();
    SLONG schedulePendingJobs();
    SLONG schedulePendingFreight();
    void refreshPlaneState();
    bool planeCanFly(const CPlane &qPlane, const CAuftrag &qJob) const;
    /* Finds the idle window that can serve the job most profitably, working on the cached
     * state only. Returns the index into mPlanes, or -1. */
    SLONG findPlaneForJob(const CAuftrag &qJob, SLONG &outGap, PlaneTime &outStart, SLONG &outGain) const;
    /* The idle windows in a plane's flight plan. Only legal in the office. */
    std::vector<PlaneGap> collectGaps(const CPlane &qPlane) const;
    /* Fits a job into one idle window, return leg included. False if it does not fit. */
    static bool fitJobIntoGap(const PlaneGap &qGap, const CPlane &qPlane, const CAuftrag &qJob, PlaneTime &outStart, PlaneTime &outBack, SLONG &outGain);
    /* One out-and-back into one idle window, for any city pair and date range. The kerosene
     * of the empty return the game inserts itself is part of outCost. */
    static bool fitLegIntoGap(const PlaneGap &qGap, const CPlane &qPlane, ULONG vonCity, ULONG nachCity, SLONG fromDate, SLONG toDate, PlaneTime &outStart,
                              PlaneTime &outBack, SLONG &outCost);
    /* Spreads a freight job over the idle windows of the given planes, earliest window
     * first. Returns the tons that can be delivered before the deadline; outCost is the
     * kerosene of every leg plus one refit charge per window used. The windows passed in
     * are consumed, so the caller may keep planning against them. */
    SLONG fitFreightIntoGaps(const std::vector<SLONG> &planeIds, std::vector<std::vector<PlaneGap>> &gaps, const CFracht &qFreight, SLONG tons, SLONG &outCost,
                             std::vector<FreightLeg> &outLegs) const;

    TEAKRAND LocalRandom{};
    PLAYER &qPlayer;

    bool mFirstRun{true};
    bool mIsSickToday{false};

    /* anim state and mood bubbles */
    SLONG mOnThePhone{0};
    SLONG mMood{-1};
    SLONG mMoodNext{-1};

    /* --- bot state (scalars must stay in sync with operator<< / operator>>) --- */

    /* Availability of every plane, cached from the flight plans during the last office
     * visit. Used at the travel agency, where the flight plans may not be read, to decide
     * whether a job can actually be flown before accepting it. */
    std::vector<PlaneState> mPlanes;

    /* Routes we rent, cached at the route box. */
    std::vector<RouteState> mRoutes;

    /* per-day bookkeeping, reset in startNewDay() */
    SLONG mDay{-1};
    SLONG mJobsTakenToday{0};
    bool mVisitedPersonalToday{false};
    bool mVisitedMechToday{false};
    bool mVisitedRouteBoxToday{false};
    bool mVisitedAdsToday{false};
    bool mVisitedBrokerToday{false};
    bool mVisitedBankToday{false};
    bool mVisitedStockToday{false};
    bool mVisitedMuseumToday{false};
    /* the broker had nothing we could afford, so try the museum */
    bool mWantUsedPlane{false};
    bool mUpgradedToday{false};
    bool mAgencyEmptyToday{false};
    SLONG mAgencyVisitsToday{0};
    SLONG mFreightVisitsToday{0};
    bool mVisitedTanksToday{false};
    bool mVisitedKerosinToday{false};
    bool mVisitedFreightToday{false};
    bool mFreightEmptyToday{false};
    SLONG mFreightTakenToday{0};

    /* mPlanes is stale and has to be rebuilt in the office before it may be used */
    bool mPlaneStateStale{true};
    /* jobs were taken since the last office visit, so a scheduling run is due */
    bool mNeedSchedule{false};

    /* alternates the filler action so we do not stand in the same room twice */
    SLONG mFillerIdx{0};

    /* measured erosion of the airline image between two advertising visits, and the reading
     * the last visit left behind to measure the next one against */
    SLONG mImageDecayPerDay{0};
    SLONG mImageAfterAds{0};
    SLONG mImageAdsDay{-1};

};

TEAKFILE &operator<<(TEAKFILE &File, const ClaudeBot &bot);
TEAKFILE &operator>>(TEAKFILE &File, ClaudeBot &bot);

#endif // CLAUDE_BOT_H_
