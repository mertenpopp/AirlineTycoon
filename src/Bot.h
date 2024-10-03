#ifndef BOT_H_
#define BOT_H_

#include "BotPlaner.h"
#include "class.h"
#include "defines.h"

#include <deque>
#include <unordered_map>
#include <vector>

class CRentRoute;
class CRoute;
class PLAYER;

extern const bool kAlwaysReplan;
extern float kSchedulingMinScoreRatio;
extern float kSchedulingMinScoreRatioLastMinute;
extern SLONG kSwitchToRoutesNumPlanesMin;
extern SLONG kSwitchToRoutesNumPlanesMax;
extern const SLONG kSmallestAdCampaign;
extern const SLONG kMinimumImage;
extern const SLONG kRouteAvgDays;
extern const SLONG kMinimumOwnRouteUtilization;
extern SLONG kMaximumRouteUtilization;
extern const SLONG kMaximumPlaneUtilization;
extern DOUBLE kMaxTicketPriceFactor;
extern const DOUBLE kDefaultTicketPriceFactor;
extern const SLONG kTargetEmployeeHappiness;
extern const SLONG kMinimumEmployeeSkill;
extern SLONG kPlaneMinimumZustand;
extern const SLONG kPlaneTargetZustand;
extern const SLONG kUsedPlaneMinimumScore;
extern DOUBLE kMaxKerosinQualiZiel;
extern SLONG kNumRoutesStartBuyingTanks;
extern SLONG kOwnStockPosessionRatio;
extern const SLONG kStockEmissionMode;
extern const bool kReduceDividend;
extern const SLONG kMaxSabotageHints;

extern const SLONG kMoneyEmergencyFund;
extern const SLONG kMoneyReserveRepairs;
extern const SLONG kMoneyReservePlaneUpgrades;
extern const SLONG kMoneyReserveBuyTanks;
extern const SLONG kMoneyReserveIncreaseDividend;
extern const SLONG kMoneyReservePaybackCredit;
extern const SLONG kMoneyReserveBuyOwnShares;
extern const SLONG kMoneyReserveBuyNemesisShares;
extern const SLONG kMoneyReserveBossOffice;
extern const SLONG kMoneyReserveExpandAirport;
extern const SLONG kMoneyReserveSabotage;

extern SLONG kPlaneScoreForceBest;

class Bot {
  public:
    explicit Bot(PLAYER &player);

    void printStatisticsLine(const CString &prefix, bool printHeader);

    void RobotInit();
    void RobotPlan();
    void RobotExecuteAction();

    void setNoticedSickness() { mIsSickToday = true; }

    /* anim state */
    bool getOnThePhone() const { return mOnThePhone > 0; }
    void decOnThePhone() { mOnThePhone--; }

    friend TEAKFILE &operator<<(TEAKFILE &File, const Bot &bot);
    friend TEAKFILE &operator>>(TEAKFILE &File, Bot &bot);

  private:
    enum class Prio {
        None,   /* conditions not met: Forbidden to perform this action! */
        Lowest, /* unimportant actions (mostly wasting time at bar, telescope, shops, ...) */
        Low,    /* not completely unimportant (for example collecting items) */
        Medium, /* most main tasks that do not need higher priority */
        High,   /* important to do soon (grab flights, important investments) */
        Higher, /* important to do very soon (grab flights from travel agency) */
        Top     /* survival at stake */
    };
    enum class AreWeBroke {
        No,
        Somewhat, /* we can use some more, but don't generate cost (loan interest) or problems (sell all stock)*/
        Yes,      /* we need money, but do not go scorched earth (e.g. sell stock, but not that enemy can take over) */
        Desperate /* sell everything (TODO: Not fully implemented) */
    };
    enum class HowToGetMoney {
        None,
        SellShares,         /* sell shares from other airlines */
        SellOwnShares,      /* sell own shares if > 50% */
        SellAllOwnShares,   /* sell all own shares */
        IncreaseCredit,     /* take new loan */
        EmitShares,         /* emit new shares */
        LowerRepairTargets, /* reduce amount pre-allocated for repairs */
        CancelPlaneUpgrades /* reduce amount pre-allocated for upgrades */
    };
    enum class RoutesNextStep {
        None, /* not doing anything specific to routes */
        RentNewRoute,
        BuyMorePlanes,
        BuyAdsForRoute,
        UpgradePlanes,
        ImproveAirlineImage
    };
    enum class FinalPhase {
        No,
        SaveMoney, /* stop investing towards other targets */
        TargetRun  /* invest towards target, liquidize all other assets */
    };
    struct RouteInfo {
        RouteInfo() = default;
        RouteInfo(SLONG id, SLONG id2, SLONG typeId) : routeId(id), routeReverseId(id2), planeTypeId(typeId) {}
        SLONG routeId{-1};
        SLONG routeReverseId{-1};
        SLONG planeTypeId{-1};
        SLONG routeUtilization{};
        SLONG routeOwnUtilization{};
        SLONG image{};
        SLONG planeUtilization{};
        SLONG planeUtilizationFC{};
        DOUBLE ticketCostFactor{2}; /* 0.5: discount / 1: flight cost / 2: normal price / 4: deluxe price */
        std::vector<SLONG> planeIds{};
        bool canUpgrade{false};
    };

    const char *getPrioName(Prio prio);
    const char *getPrioName(SLONG prio);

    /* in BotConditions.cpp */
    Prio condAll(SLONG actionId);
    Prio condStartDay();
    Prio condStartDayLaptop();
    Prio condBuero();
    Prio condCallInternational();
    Prio condCallInternationalHandy();
    Prio condCheckLastMinute();
    Prio condCheckTravelAgency();
    Prio condCheckFreight();
    Prio condUpgradePlanes();
    Prio condBuyNewPlane(__int64 &moneyAvailable);
    Prio condBuyUsedPlane(__int64 &moneyAvailable);
    Prio condVisitMuseum();
    Prio condVisitHR();
    Prio condBuyKerosine(__int64 &moneyAvailable);
    Prio condBuyKerosineTank(__int64 &moneyAvailable);
    Prio condSabotage(__int64 &moneyAvailable);
    Prio condVisitSaboteur();
    Prio condIncreaseDividend(__int64 &moneyAvailable);
    Prio condTakeOutLoan();
    Prio condDropMoney(__int64 &moneyAvailable);
    Prio condEmitShares();
    Prio condBuyShares(__int64 &moneyAvailable);
    Prio condBuyNemesisShares(__int64 &moneyAvailable);
    Prio condBuyOwnShares(__int64 &moneyAvailable);
    Prio condOvertakeAirline();
    Prio condSellShares(__int64 &moneyAvailable);
    Prio condVisitMech();
    Prio condVisitNasa(__int64 &moneyAvailable);
    Prio condVisitMisc();
    Prio condVisitMakler();
    Prio condVisitArab();
    Prio condVisitRick();
    Prio condVisitDutyFree(__int64 &moneyAvailable);
    Prio condVisitBoss(__int64 &moneyAvailable);
    Prio condExpandAirport(__int64 &moneyAvailable);
    Prio condVisitRouteBoxPlanning();
    Prio condVisitRouteBoxRenting(__int64 &moneyAvailable);
    Prio condVisitSecurity(__int64 &moneyAvailable);
    Prio condSabotageSecurity();
    Prio condVisitDesigner(__int64 &moneyAvailable);
    Prio condBuyAdsForRoutes(__int64 &moneyAvailable);
    Prio condBuyAds(__int64 &moneyAvailable);
    Prio condVisitAds();

    /* in BotActions.cpp */
    void actionStartDay(__int64 moneyAvailable);
    void actionStartDayLaptop(__int64 moneyAvailable);
    void actionBuero();
    void actionCallInternational(bool areWeInOffice);
    void actionCheckLastMinute();
    void actionCheckTravelAgency();
    void actionCheckFreightDepot();
    void actionUpgradePlanes();
    void updateExtraWorkers();
    void actionBuyNewPlane(__int64 moneyAvailable);
    void actionBuyUsedPlane(__int64 moneyAvailable);
    void actionMuseumCheckPlanes();
    void actionBuyDesignerPlane(__int64 moneyAvailable);
    void actionVisitHR();
    void actionBuyKerosine(__int64 moneyAvailable);
    void actionBuyKerosineTank(__int64 moneyAvailable);
    void actionSabotage(__int64 moneyAvailable);
    void actionVisitSaboteur();
    static __int64 calcBuyShares(__int64 moneyAvailable, DOUBLE kurs);
    static __int64 calcSellShares(__int64 moneyToGet, DOUBLE kurs);
    static __int64 calcNumOfFreeShares(SLONG playerId);
    __int64 calcAmountToBuy(SLONG buyFromPlayerId, SLONG desiredRatio, __int64 moneyAvailable) const;
    void actionEmitShares();
    void actionBuyNemesisShares(__int64 moneyAvailable);
    void actionBuyOwnShares(__int64 moneyAvailable);
    void actionOvertakeAirline();
    void actionSellShares(__int64 moneyAvailable);
    void actionVisitMech();
    void actionVisitDutyFree(__int64 moneyAvailable);
    void actionVisitBoss();
    void actionVisitRouteBox();
    void actionRentRoute();
    void actionBuyAdsForRoutes(__int64 moneyAvailable);
    void actionBuyAds(__int64 moneyAvailable);
    void actionVisitAds();
    void actionVisitSecurity(__int64 moneyAvailable);

    /* in BotFunctions.cpp */
    void grabNewFlights();
    void determineNemesis();
    void switchToFinalTarget();
    std::vector<SLONG> findBestAvailablePlaneType(bool forRoutes, bool canRefresh);
    void grabFlights(BotPlaner &planer, bool areWeInOffice);
    void requestPlanFlights(bool areWeInOffice);
    void planFlights();
    SLONG replaceAutomaticFlights(SLONG planeId);
    std::pair<SLONG, SLONG> kerosineQualiOptimization(__int64 moneyAvailable, DOUBLE targetFillRatio) const;
    bool determineSabotageMode(__int64 moneyAvailable, SLONG &jobType, SLONG &jobNumber, SLONG &jobHints);
    /* routes */
    SLONG getNumRentedRoutes() const;
    void checkLostRoutes();
    void updateRouteInfoOffice();
    void updateRouteInfoBoard();
    void routesRecalcNextStep();
    std::pair<Bot::RoutesNextStep, SLONG> routesFindNextStep() const;
    void requestPlanRoutes(bool areWeInOffice);
    void findBestRoute();
    void planRoutes();
    void assignPlanesToRoutes(bool areWeInOffice);

    /* misc (in BotMisc.cpp) */
    void printRobotFlags() const;
    SLONG numPlanes() const;
    std::vector<SLONG> getAllPlanes() const;
    bool isOfficeUsable() const;
    bool hoursPassed(SLONG room, SLONG hours) const;
    bool haveDiscount() const;
    bool checkLaptop();
    enum class HowToPlan { None, Laptop, Office };
    HowToPlan howToPlanFlights();
    __int64 getMoneyAvailable() const;
    AreWeBroke areWeBroke() const;
    HowToGetMoney howToGetMoney();
    __int64 howMuchMoneyCanWeGet(bool extremeMeasures);
    bool canWeCallInternational();
    SLONG calcCurrentGainFromJobs() const;
    SLONG calcRouteImageNeeded(const RouteInfo &routeInfo) const;
    void removePlaneFromRoute(SLONG planeId);
    bool checkPlaneLists();
    void findPlanesNotAvailableForService(std::vector<SLONG> &listAvailable, std::deque<SLONG> &listUnassigned);
    void findPlanesAvailableForService(std::deque<SLONG> &listUnassigned, std::vector<SLONG> &listAvailable);
    bool checkPlaneAvailable(SLONG planeId, bool printIfAvailable, bool areWeInOffice);
    const CRentRoute &getRentRoute(const RouteInfo &routeInfo) const;
    const CRoute &getRoute(const RouteInfo &routeInfo) const;
    __int64 getDailyOpSaldo() const;
    __int64 getWeeklyOpSaldo() const;
    bool isLateGame() const;
    SLONG getImage() const;
    void forceReplanning();
    void setHardcodedDesignerPlaneLarge();
    void setHardcodedDesignerPlaneEco();

    TEAKRAND LocalRandom{};
    PLAYER &qPlayer;

    /* action economy */
    std::unordered_map<SLONG, SLONG> mLastTimeInRoom{};
    SLONG mNumActionsToday{0};

    /* planes used for what? */
    std::vector<SLONG> mPlanesForJobs{};
    std::deque<SLONG> mPlanesForJobsUnassigned{};
    std::vector<SLONG> mPlanesForRoutes{};
    std::deque<SLONG> mPlanesForRoutesUnassigned{};

    /* known plane types */
    std::vector<SLONG> mKnownPlaneTypes{};

    /* strategy state */
    bool mLongTermStrategy{true};
    SLONG mBestPlaneTypeId{-1};
    SLONG mBestUsedPlaneIdx{-1};
    SLONG mBuyPlaneForRouteId{-1};
    SLONG mPlaneTypeForNewRoute{-1};
    std::vector<SLONG> mPlanesForNewRoute{};
    SLONG mWantToRentRouteId{-1};
    bool mFirstRun{true};
    bool mDayStarted{false};
    bool mDoRoutes{false};
    FinalPhase mRunToFinalObjective{FinalPhase::No};
    __int64 mMoneyForFinalObjective{0};
    bool mOutOfGates{false};
    bool mNeedToPlanJobs{false};
    bool mNeedToPlanRoutes{false};
    __int64 mMoneyReservedForRepairs{0};
    __int64 mMoneyReservedForUpgrades{0};
    __int64 mMoneyReservedForAuctions{0};
    __int64 mMoneyReservedForFines{0};
    SLONG mNemesis{-1};
    __int64 mNemesisScore{0};
    bool mNeedToShutdownSecurity{false};
    bool mUsingSecurity{false};
    SLONG mNemesisSabotaged{-1};
    SLONG mArabHintsTracker{0};
    SLONG mCurrentImage{0};

    /* status boss office */
    SLONG mBossNumCitiesAvailable{-1};
    bool mBossGateAvailable{false};

    /* detect tanks being too small */
    DOUBLE mTankRatioEmptiedYesterday{0};
    SLONG mKerosineUsedTodaySoFar{0};
    SLONG mKerosineLevelLastChecked{0};

    /* routes */
    std::vector<RouteInfo> mRoutes{};
    std::vector<SLONG> mRoutesSortedByOwnUtilization{};
    bool mRoutesUpdated{false};
    bool mRoutesUtilizationUpdated{false};
    RoutesNextStep mRoutesNextStep{RoutesNextStep::None};
    SLONG mImproveRouteId{-1};

    /* crew */
    SLONG mNumEmployees{0};
    SLONG mExtraPilots{0};
    SLONG mExtraBegleiter{0};

    /* items */
    SLONG mItemPills{0};      /* 1: card taken, 2: card given */
    SLONG mItemAntiVirus{0};  /* 1: spider taken, 2: spider given, 3: darts taken, 4: darts given */
    SLONG mItemAntiStrike{0}; /* 1: BH taken, 2: BH given, 3: horseshoe taken, 4: horseshoe given */
    SLONG mItemArabTrust{0};  /* 1: MG bought, 2: MG given */
    bool mIsSickToday{false};

    /* current solution (not planned yet) */
    BotPlaner::SolutionList mPlanerSolution;

    /* anim state */
    SLONG mOnThePhone{0};

    /* designer plane */
    CXPlane mDesignerPlane{};
    CString mDesignerPlaneFile{};
};

TEAKFILE &operator<<(TEAKFILE &File, const Bot &bot);
TEAKFILE &operator>>(TEAKFILE &File, Bot &bot);

#endif // BOT_H_
