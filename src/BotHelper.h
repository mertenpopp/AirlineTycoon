#ifndef BOT_HELPER_H_
#define BOT_HELPER_H_

#include "class.h"
#include "defines.h"
#include "GameMechanic.h"

#include <array>
#include <cassert>
#include <climits>
#include <iostream>
#include <optional>
#include <vector>

inline constexpr int ceil_div(int a, int b) {
    assert(b != 0);
    return a / b + (a % b != 0);
}

class PlaneTime {
  public:
    PlaneTime() = default;
    PlaneTime(int date, int time) : mDate(date), mTime(time) { normalize(); }

    int getDate() const { return mDate; }
    int getHour() const { return mTime; }
    int convertToHours() const { return 24 * mDate + mTime; }

    PlaneTime &operator+=(int delta) {
        mTime += delta;
        normalize();
        return *this;
    }

    PlaneTime &operator-=(int delta) {
        mTime -= delta;
        normalize();
        return *this;
    }
    PlaneTime operator+(int delta) const {
        PlaneTime t = *this;
        t += delta;
        return t;
    }
    PlaneTime operator-(int delta) const {
        PlaneTime t = *this;
        t -= delta;
        return t;
    }
    int operator-(const PlaneTime &time) const {
        PlaneTime t = *this - time.convertToHours();
        return t.convertToHours();
    }
    bool operator==(const PlaneTime &other) const { return (mDate == other.mDate && mTime == other.mTime); }
    bool operator!=(const PlaneTime &other) const { return (mDate != other.mDate || mTime != other.mTime); }
    bool operator>(const PlaneTime &other) const {
        if (mDate == other.mDate) {
            return (mTime > other.mTime);
        }
        return (mDate > other.mDate);
    }
    bool operator>=(const PlaneTime &other) const {
        if (mDate == other.mDate) {
            return (mTime >= other.mTime);
        }
        return (mDate > other.mDate);
    }
    bool operator<(const PlaneTime &other) const { return other > *this; }
    bool operator<=(const PlaneTime &other) const { return other >= *this; }
    void setDate(int date) {
        mDate = date;
        mTime = 0;
    }

  private:
    void normalize() {
        while (mTime >= 24) {
            mTime -= 24;
            mDate++;
        }
        while (mTime < 0) {
            mTime += 24;
            mDate--;
        }
    }
    int mDate{0};
    int mTime{0};
};

TEAKFILE &operator<<(TEAKFILE &File, const PlaneTime &planeTime);
TEAKFILE &operator>>(TEAKFILE &File, PlaneTime &planeTime);

class SabotageMode {
  public:
    enum class SabotageCategory { None = -1, Plane = 0, Personal = 1, Special = 2 };

    enum class Plane {
        SaltedFood = 1,        // Heavily salted food onboard ($1,000)
        MovieTheatreBreak = 2, // Breakdown of the on-board movie theatre ($5,000)
        FlatTire = 3,          // Delay due to flat tire ($10,000)
        EngineBreakdown = 4,   // Engine breakdown ($50,000)
        PlaneCrash = 5         // A plane crash ($100,000)
    };

    enum class Personal {
        CoffeeBacteria = 1, // Bacteria in the coffee ($10,000) - sends rival to restroom between Rick’s cafe and Newspaper Stand
        NotebookVirus = 2,  // Virus on the notebook ($25,000) - ruins notebook via computer virus
        OfficeBomb = 3,     // Bomb in the office ($50,000) - blows up opponent office
        ProvokeStrike = 4   // Provoke a strike ($250,000) - suspends flights by disconnecting employees
    };

    enum class Special {
        AircraftBrochures = 1, // Place brochures in competitor's aircraft ($100,000)
        CutTelephones = 2,     // Cut off telephones ($500,000)
        FalsePressRelease = 3, // Publish a false press-release ($1,000,000) - cancels all flights for a full day
        BankHack = 4,          // Hack bank account ($2,000,000) - opponent steals $1M
        GroundAircraft = 5,    // Ground a competitor's aircraft ($5,000,000)
        RouteTheft = 6         // Steal one route from opponent (existing behavior)
    };

    SabotageMode() = default;
    SabotageMode(Plane t, SLONG maxTrust = INT_MAX) {
        mCategory = SabotageCategory::Plane;
        mJobNumber = std::min(static_cast<SLONG>(t), maxTrust);
        mJobHints = hintArray1[mJobNumber - 1];
        mJobCost = SabotagePrice[mJobNumber - 1];
    }
    SabotageMode(Personal t, SLONG maxTrust = INT_MAX) {
        mCategory = SabotageCategory::Personal;
        mJobNumber = std::min(static_cast<SLONG>(t), maxTrust);
        mJobHints = hintArray2[mJobNumber - 1];
        mJobCost = SabotagePrice2[mJobNumber - 1];
    }
    SabotageMode(Special t, SLONG maxTrust = INT_MAX) {
        mCategory = SabotageCategory::Special;
        mJobNumber = std::min(static_cast<SLONG>(t), maxTrust);
        mJobHints = hintArray3[mJobNumber - 1];
        mJobCost = SabotagePrice3[mJobNumber - 1];
    }
    SabotageMode(SLONG category, SLONG jobNumber) {
        if (category == static_cast<SLONG>(SabotageCategory::Plane)) {
            *this = SabotageMode(static_cast<Plane>(jobNumber));
        } else if (category == static_cast<SLONG>(SabotageCategory::Personal)) {
            *this = SabotageMode(static_cast<Personal>(jobNumber));
        } else if (category == static_cast<SLONG>(SabotageCategory::Special)) {
            *this = SabotageMode(static_cast<Special>(jobNumber));
        } else {
            mCategory = SabotageCategory::None;
            mJobNumber = 0;
            mJobHints = 0;
            mJobCost = 0;
        }
    }

    SLONG getCategory() const { return static_cast<SLONG>(mCategory); }
    bool isValid() const { return mCategory != SabotageCategory::None; }
    SLONG getJobNumber() const { return mJobNumber; }
    SLONG getJobHints() const { return mJobHints; }
    SLONG getJobCost() const { return mJobCost; }

    bool needPlane() const {
        if (mCategory == SabotageCategory::Plane) {
            return true;
        }
        if (mCategory == SabotageCategory::Special && mJobNumber == static_cast<SLONG>(Special::GroundAircraft)) {
            return true;
        }
        return false;
    }
    bool needRoute() const {
        if (mCategory == SabotageCategory::Special && mJobNumber == static_cast<SLONG>(Special::RouteTheft)) {
            return true;
        }
        return false;
    }

    std::string getName() const;

  private:
    static constexpr std::array<SLONG, 5> hintArray1{2, 4, 10, 20, 100};
    static constexpr std::array<SLONG, 4> hintArray2{8, 0, 25, 40};
    static constexpr std::array<SLONG, 6> hintArray3{8, 15, 25, 30, 50, 70};
    SabotageCategory mCategory{SabotageCategory::None};
    SLONG mJobNumber{0};
    SLONG mJobHints{0};
    SLONG mJobCost{0};
};

namespace Helper {

CString getWeekday(UWORD date);
CString getWeekday(const PlaneTime &time);

struct FreightInfo {
    std::vector<CString> planeNames{};
    std::vector<SLONG> tonsPerPlane{};
    std::vector<CFlugplanEintrag> FPEs{};
    SLONG tonsOpen{0};
    SLONG smallestDecrement{INT_MAX};
};

struct ScheduleInfo {
    SLONG jobs{0};
    SLONG freightJobs{0};
    SLONG gain{0};
    SLONG passengers{0};
    SLONG tons{0};
    SLONG miles{0};
    SLONG uhrigFlights{0};
    SLONG hoursFlights{0};
    SLONG hoursAutoFlights{0};
    SLONG keroseneFlights{0};
    SLONG keroseneAutoFlights{0};
    SLONG numPlanes{0};
    PlaneTime scheduleStart{INT_MAX, 0};
    PlaneTime scheduleEnd{0, 0};
    /* for job statistics */
    std::array<SLONG, 5> jobTypes{};
    std::array<SLONG, 6> jobSizeTypes{};

    DOUBLE getRatioFlights() const { return 100.0 * hoursFlights / ((scheduleEnd - scheduleStart) * numPlanes); }
    DOUBLE getRatioAutoFlights() const { return 100.0 * hoursAutoFlights / ((scheduleEnd - scheduleStart) * numPlanes); }
    DOUBLE getKeroseneRatio() const { return 100.0 * keroseneFlights / (keroseneFlights + keroseneAutoFlights); }

    ScheduleInfo &operator+=(ScheduleInfo delta) {
        jobs += delta.jobs;
        freightJobs += delta.freightJobs;
        gain += delta.gain;
        passengers += delta.passengers;
        tons += delta.tons;
        miles += delta.miles;
        uhrigFlights += delta.uhrigFlights;
        hoursFlights += delta.hoursFlights;
        hoursAutoFlights += delta.hoursAutoFlights;
        keroseneFlights += delta.keroseneFlights;
        keroseneAutoFlights += delta.keroseneAutoFlights;
        numPlanes += delta.numPlanes;
        scheduleStart = std::min(scheduleStart, delta.scheduleStart);
        scheduleEnd = std::max(scheduleEnd, delta.scheduleEnd);
        /* for job statistics */
        for (SLONG i = 0; i < jobTypes.size(); i++) {
            jobTypes[i] += delta.jobTypes[i];
        }
        for (SLONG i = 0; i < jobSizeTypes.size(); i++) {
            jobSizeTypes[i] += delta.jobSizeTypes[i];
        }
        return *this;
    }

    void printGain() const;
    void printDetails() const;
};

void printJob(const CAuftrag &qAuftrag);
void printRoute(const CRoute &qRoute);
void printFreight(const CFracht &qAuftrag);

std::string getRouteName(const CRoute &qRoute);
std::string getJobName(const CAuftrag &qAuftrag);
std::string getFreightName(const CFracht &qAuftrag);
std::string getPlaneName(const CPlane &qPlane, int mode = 0);

void printFPE(const CFlugplanEintrag &qFPE);

const CFlugplanEintrag *getLastFlight(const CPlane &qPlane);
const CFlugplanEintrag *getLastFlightNotAfter(const CPlane &qPlane, PlaneTime ignoreFrom);
std::pair<PlaneTime, int> getPlaneAvailableTimeLoc(const CPlane &qPlane, std::optional<PlaneTime> ignoreFrom, std::optional<PlaneTime> earliest);

SLONG checkPlaneSchedule(const PLAYER &qPlayer, SLONG planeId, bool alwaysPrint);
SLONG checkPlaneSchedule(const PLAYER &qPlayer, const CPlane &qPlane, bool alwaysPrint);
SLONG _checkPlaneSchedule(const PLAYER &qPlayer, const CPlane &qPlane, std::unordered_map<SLONG, CString> &assignedJobs,
                          std::unordered_map<SLONG, FreightInfo> &freightTons, bool alwaysPrint);
SLONG checkFlightJobs(const PLAYER &qPlayer, bool alwaysPrint, bool verboseInfo);
void printFlightJobs(const PLAYER &qPlayer, SLONG planeId);
void printFlightJobs(const PLAYER &qPlayer, const CPlane &qPlane);

ScheduleInfo calculateScheduleInfo(const PLAYER &qPlayer, SLONG planeId);

void printAllSchedules(bool infoOnly);

bool checkRoomOpen(SLONG roomId);
SLONG getRoomFromAction(SLONG PlayerNum, SLONG actionId);
SLONG getWalkDistance(int playerNum, SLONG roomId);

const char *getItemName(SLONG item);

void printStatisticsLine(const PLAYER &qPlayer, const CString &prefix, bool printHeader);
void printStatisticsLineForAllPlayers(const CString &prefix, bool printHeader);

} // namespace Helper

#endif // BOT_HELPER_H_
