#ifndef CLAUDE_BOT_H_
#define CLAUDE_BOT_H_

#include "BotHelper.h"
#include "class.h"
#include "defines.h"

#include <vector>

class PLAYER;
class CAuftraege;
class CFracht;

extern const SLONG kRouteAvgDays;

class ClaudeBot {
  public:
    explicit ClaudeBot(PLAYER &player);

    void RobotInit(SLONG randomSeed);
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
        /* The open end of a flight plan nothing follows, which only exists in a mission
         * without a route box - see collectGaps(). Nothing has to fit back into it, so a
         * leg placed here is not charged a return the game will never fly, and the plane
         * stays at the destination: the window's city moves with it. */
        bool openTail{false};
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
        /* The album's *unique id*, not its index.
         *
         * mPlanes is built in the office and read again at the travel agency and the freight
         * depot, and ALBUM_V indices do not survive in between: PLAYER::BuyPlane() ends with
         * `Planes.Sort()` (Player.cpp:167), and a sort preserves unique ids but not indices.
         * IsInAlbum() cannot catch that - the slot is still valid, it just holds a different
         * aeroplane - so the cached windows would be sold against the wrong flight plan.
         *
         * That is a latent hazard rather than a live bug: executeBuyPlane() sets
         * mPlaneStateStale, and collectActions() will not schedule the agency or the freight
         * depot while it is set, so today the cache is always rebuilt before it is reused.
         * Instrumenting the resolution found zero divergences over a full game, with and
         * without aeroplanes being destroyed. Keying on the unique id costs nothing and
         * removes the dependency on every future album mutation remembering to set that
         * flag. Resolve to today's index with planeIndex() at the point of use. */
        ULONG uid{0};
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
        /* Rented by the stranded-plane pass in executeRouteBox(), for an aeroplane that
         * could reach nothing else. scheduleRouteFlights() hides it from every aeroplane
         * that has a proper pair in range - see the comment there. */
        bool castaway{false};
        SLONG anzPax{0}; /* passengers the route wants per day, CRoute::AnzPassagiere() */
        bool pricesSet{false};
    };

    /* What the current mission changes about the game.
     *
     * Missions differ in two ways that matter: which rooms the airport has at all, and what
     * counts as winning. Everything the bot does differently in a mission goes through one
     * of these flags, so the free game keeps running through exactly the same code with
     * every flag false. Derived from Sim.Difficulty once a day, never serialised. */
    struct Mission {
        SLONG difficulty{DIFF_FREEGAME};
        bool isMission{false};
        /* Rent-a-route does not exist in this mission, so the flight plans stay empty
         * unless jobs alone fill them - see collectGaps(). */
        bool noRouteBox{false};
        /* Goals. At most one is set; several missions share a goal. */
        bool wantDebtFree{false};    /* ADDON01: Credit == 0 and Money >= 0 */
        bool wantImage{false};       /* HARD: Image >= TARGET_IMAGE */
        /* ADDON07, ATFS02: how many planes the goal wants at 90 or better, 0 for none.
         * Only that many are repaired: the extra repair charge is Improvement * ptPreis /
         * 110 a night, which is the largest bill in the game, and a mission fleet starts at
         * Zustand 35 with a million in the bank. */
        SLONG conditionPlanes{0};
        bool wantUpgrades{false};    /* ADDON05, ATFS02: service points / the four fittings */
        /* How many planes have to carry the fittings, or -1 for the whole fleet (ADDON05
         * counts every level on every plane). */
        SLONG upgradePlanes{0};
        bool wantFreight{false};     /* ADDON02, ADDON03: tonnage is the goal */
        bool wantFreeFreight{false}; /* ADDON03: only Praemie == 0 contracts count */
        bool wantMissionCities{false}; /* NORMAL: routes to Sim.MissionCities win */
        /* Routes are the free game's engine, but they are an investment: a pair has to be
         * rented, priced, advertised and flown for days before the route image makes it pay.
         * A mission that the legacy bot finishes in two or three weeks with two planes never
         * gets that back, and every route flight is an idle window the goal could have used.
         * Set for the short missions and for the ones scored on something routes do not
         * produce. */
        bool noRoutes{false};
        /* FINAL and ADDON10: won by buying ten parts from NASA, in order, for 204M and 238M
         * respectively. Routes stay on - that bill needs the full economy behind it. */
        bool wantRocket{false};
        /* ATFS05, ATFS08: how many self-designed planes clearing the mission's bar the goal
         * wants, 0 for every other mission. The only way to get one is the aeroplane designer,
         * so this is the whole game in those two missions. */
        SLONG designerPlanes{0};
        /* ADDON09: five jobs land in the backlog every morning and the goal counts only those.
         * The whole fleet is re-planned around them at every office visit - see
         * scheduleUhrigJobs() - and nothing else is taken that could compete for the time. */
        bool uhrigJobs{false};
        /* EASY: the goal counts PLAYER::Gewinn, which only sums each flight's saldo and the
         * fines. A flight's saldo is charged only for the kerosene bought at the gate
         * (Schedule.cpp, BookFlight) - fuel drawn from our own tank was paid for at the Arab,
         * and neither the tank nor its contents ever reach Gewinn. So every flight flown
         * from the tank counts its whole revenue towards the goal. */
        bool fuelFromTank{false};
        /* Missions won by capacity (jobs, passengers, tons, miles) or by the number of planes
         * owned: how many planes to grow the fleet to with used planes from the museum, 0 for
         * none. A mission starts with a million or three, and the cheapest new plane costs 9.9M,
         * so the museum is the only way to a third plane in the first weeks. */
        SLONG usedFleet{0};
        /* Least cabin a used plane must have. Seats are what the capacity missions buy it for;
         * ATFS04/06 only count planes and take any. */
        SLONG usedMinSeats{0};
        /* ATFS01: the goal is cash in hand, so nothing is bought that does not fly - no
         * aeroplanes, advertising, gates or fittings. */
        bool hoardCash{false};
        /* ATFS06: fifteen days without being sabotaged, against a third actor that picks a
         * random airline every day. Only the security office stops it. */
        bool wantNoSabotage{false};
        /* Route ticket price, in percent of the threshold CalcPassengers() calls extortionate. */
        SLONG ticketPercent{0};
        /* FIRST: first to 2500 passengers, so jobs are ranked by their cabin as well. */
        bool wantPassengers{false};
        /* ATFS09: take no freight contracts - see setupMission(). */
        bool noFreight{false};
        /* ADDON08: fly routes but never advertise - see setupMission(). */
        bool noAds{false};
    };

    /* --- the aeroplane designer --- */
    /* Builds the design this mission needs, once per game. Pure computation over the static
     * part tables, so it needs no room and can run before the character is anywhere. */
    void prepareDesignerPlane();
    /* How many planes we already own that clear the mission's bar. */
    SLONG countQualifyingPlanes() const;
    /* True while the mission still wants designer planes we cannot yet pay for - the broker has
     * to stop eating the cash they need. */
    bool savingForDesigner() const;
    /* True once the ordinary fleet is large enough that further catalogue planes only delay the
     * designer planes. Separate from savingForDesigner(): the fleet has to keep growing for a
     * while even after every other kind of spending has stopped. */
    bool designerFleetFull() const;
    void executeDesigner();

    /* --- planning --- */
    void setupMission();
    /* Whether this mission has a rent-a-route counter at all. */
    bool routesAvailable() const;
    /* Extra weight on a route the mission goal asks for, 0 otherwise. Route box only. */
    SLONG missionRouteBonus(const CRoute &qRoute) const;
    void collectActions(std::vector<SLONG> &out) const;
    SLONG pickFillerAction();
    bool canUseAction(SLONG actionId) const;
    bool haveOffice() const;
    SLONG planeIndex(ULONG uid) const;
    void startNewDay();

    /* --- action implementations --- */
    void executePersonal();
    void hireAdvisors();
    /* Signature shared by GameMechanic::takeLastMinuteJob() and takeFlightJob(). */
    using JobTaker = bool (*)(PLAYER &, SLONG, SLONG &);
    /* One greedy pass over a board of passenger jobs; `who` only names the caller in the log. */
    void takeJobsFromBoard(CAuftraege &board, JobTaker take, const char *who);
    void executeNasa();
    void executeCheckAgent1();
    void executeCheckAgent2();
    void executeCheckAgent3();
    void executeOffice();
    void executeMech();
    void executeBank();
    void executeStock();
    void executeRouteBox();
    void executeBoss();
    void executeAds();
    void executeUpgrades();
    void executeBuyPlane();
    void executeBuyUsedPlane();
    void executeProtection();
    SLONG countPlanes() const;
    void executeKerosinTanks();
    void executeBuyKerosin();
    /* Fuel arbitrage: how much capacity to hold, and whether today is cheap enough to fill it. */
    SLONG fuelTankTarget() const;
    bool fuelIsCheap() const;
    /* Forms the burn estimate the Arab and the broker work from. Office only: it reads
     * qPlayer.BilanzGestern, which RULES.md gates on the office and a financial advisor. */
    void cacheFuelBurn();

    /* --- Hurricane (BotLevel == BotDifficultyHurricane): the Tycoon economy plus sabotage --- */
    bool isHurricane() const;
    bool sabotageVisitWorthIt() const;
    void executeDutyFree();
    void executeArab();
    void executeSabotage();
    void executeSecurity();
    /* Saboteur room only: the competitor to sabotage, or -1. */
    SLONG pickSabotageVictim() const;
    /* Saboteur room only: album index of the victim's largest aeroplane, or -1. */
    SLONG pickVictimPlane(SLONG victim) const;
    /* Saboteur room only: the victim's route worth most to us, from mTheftValues, or -1. */
    SLONG pickRouteToSteal(SLONG victim) const;

    /* --- scheduling --- */
    SLONG scheduleRouteFlights();
    SLONG schedulePendingJobs();
    /* ADDON09 only: clears every plan that is not locked yet and re-plans all jobs we hold
     * across the whole fleet, repositioning legs included. Office only. */
    SLONG scheduleUhrigJobs();
    SLONG schedulePendingFreight();
    void refreshPlaneState();
    bool planeCanFly(const CPlane &qPlane, const CAuftrag &qJob) const;
    /* Finds the idle window that can serve the job most profitably, working on the cached
     * state only. Returns the index into mPlanes, or -1. */
    SLONG findPlaneForJob(const CAuftrag &qJob, SLONG &outGap, PlaneTime &outStart, SLONG &outGain) const;
    /* The idle windows in a plane's flight plan. Only legal in the office. */
    std::vector<PlaneGap> collectGaps(const CPlane &qPlane) const;
    /* Fits a job into one idle window, return leg included. False if it does not fit. */
    /* `alreadyOurs` changes what the gain means. Deciding whether to *take* a job off a board,
     * the alternative is not having it, so the gain is the premium less the flight. Deciding
     * which of the jobs we already hold gets a scarce window, the alternative is the fine, so
     * flying it is worth the premium *and* the fine avoided. Ignoring that let ADDON09 - which
     * pushes five jobs a day into the backlog, each with Strafe == Praemie - run up 11.7M of
     * fines in two days and hit the debt floor by day 10. */
    static bool fitJobIntoGap(const PlaneGap &qGap, const CPlane &qPlane, const CAuftrag &qJob, PlaneTime &outStart, PlaneTime &outBack, SLONG &outGain,
                              bool alreadyOurs = false);
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

    /* Derived from Sim.Difficulty, so it is rebuilt rather than loaded with a savegame. */
    Mission mMission{};

    /* The design for this mission, its file (buyXPlane() loads from disk) and its price, which
     * every affordability decision needs and which is only known once the design exists. */
    CXPlane mDesignerPlane{};
    CString mDesignerPlaneFile{};
    SLONG mDesignerPlaneCost{0};
    bool mDesignerPlaneReady{false};
    bool mDesignerPlaneSaved{false};
    bool mVisitedDesignerToday{false};

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
    /* Route ids of the pairs rented for a stranded aeroplane. mRoutes is rebuilt from
     * qPlayer.RentRouten every day, so the flag has to live somewhere that survives that -
     * and it has to be pruned to what we still hold, or a pair the game confiscates and the
     * ordinary loop later rents back on merit would stay marked a castaway for good. */
    std::vector<SLONG> mCastawayRoutes;

    /* per-day bookkeeping, reset in startNewDay() */
    SLONG mDay{-1};
    SLONG mJobsTakenToday{0};
    bool mVisitedPersonalToday{false};
    bool mVisitedMechToday{false};
    bool mVisitedRouteBoxToday{false};
    bool mVisitedAdsToday{false};
    bool mVisitedBossToday{false};
    bool mVisitedBrokerToday{false};
    bool mVisitedMuseumToday{false}; /* not serialised: reset every morning anyway */
    bool mVisitedBankToday{false};
    bool mVisitedStockToday{false};
    bool mUpgradedToday{false};
    bool mAgencyEmptyToday{false};
    SLONG mAgencyVisitsToday{0};
    SLONG mLastMinuteVisitsToday{0};
    bool mVisitedNasaToday{false};
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

    /* Units of kerosene the fleet burns in a day, measured in the office off yesterday's
     * balance. The Arab and the broker size the fuel manoeuvre from this, because neither
     * may read the balance itself. */
    SLONG mFuelUnitsPerDay{0};

    /* Yesterday's ticket revenue, cached in the office (BilanzGestern needs the office and a
     * financial advisor) for the advertising agency to size the airline image by. */
    __int64 mTicketsYesterday{0};

    /* Room-check bookkeeping: the day the last mismatch warning was printed, and how many
     * mismatches that day has seen. */
    SLONG mWrongRoomDay{-1};
    SLONG mWrongRoomCount{0};

    /* the day the weekly balance was last logged from the office */
    SLONG mBalanceLoggedDay{-1};

    /* --- Hurricane state --- */

    /* Our own count of the saboteur's hints. qPlayer.ArabHints may not be read, so this is
     * what a human would have to keep in mind: every ordered job adds its hints, every day
     * takes 3 off. The game only adds hints once a job is carried out, and a job may never
     * be, so this count is never below the real one - which is the safe side, because the
     * boss exposes the saboteur at 100. */
    SLONG mSabotageHints{0};
    SLONG mSabotageJobsOrdered{0};
    bool mVisitedDutyFreeToday{false};
    bool mVisitedArabToday{false};
    bool mVisitedSaboteurToday{false};
    bool mVisitedSecurityToday{false};
    /* A job was refused because the victim bought protection. Cleared once the pliers have
     * knocked out the security office. */
    bool mSecurityBlocked{false};
    /* Bit per entry of kSabotageJobs: refused by the victim's security, skip until the pliers
     * have been used or the week is over. Without this we would save hints for a job that
     * can never be ordered. */
    SLONG mBlockedJobs{0};
    SLONG mLastVirusDay{-100};
    /* Hints of the job we are saving for, 0 if none - see sabotageVisitWorthIt(). */
    SLONG mSavingForHints{0};
    /* Routes we could fly, with their value per plane hour to our largest aeroplane, cached at
     * the route box (Bedarf and Miete may only be read there). The saboteur steals from this. */
    std::vector<std::pair<SLONG, SLONG>> mTheftValues;
};

TEAKFILE &operator<<(TEAKFILE &File, const ClaudeBot &bot);
TEAKFILE &operator>>(TEAKFILE &File, ClaudeBot &bot);

#endif // CLAUDE_BOT_H_
