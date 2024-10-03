#ifndef BOT_PLANER_H_
#define BOT_PLANER_H_

#include "BotHelper.h"

#include <cassert>
#include <climits>
#include <deque>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

extern const int kAvailTimeExtra;
extern const int kDurationExtra;
extern const int kScheduleForNextDays;

extern int kNumToAdd;
extern int kNumBestToAdd;
extern int kNumToRemove;
extern int kTempStart;
extern int kTempStep;
extern int kJobSelectRandomization;

class Graph {
  public:
    struct Node {
        int jobIdx{-1};
        int score{0};
        float scoreRatio{0.0F};
        int duration{0};
        int earliest{0};
        int latest{INT_MAX};
        std::vector<int> bestNeighbors{};
    };

    struct Edge {
        int cost{-1};
        int duration{-1};
    };

    struct NodeState {
        PlaneTime startTime{};
        int cameFrom{-1};
        int nextNode{-1};
    };

    Graph(int numPlanes, int ptPass) : nPlanes(numPlanes), nNodes(numPlanes), planeTypePassengers(ptPass) {}

    int addNode(int jobId) {
        if (mapJobIdToNode.find(jobId) == mapJobIdToNode.end()) {
            mapJobIdToNode[jobId] = nNodes;
        }
        nNodes++;
        nodeInfo.resize(nNodes);
        return (nNodes - 1);
    }

    void resizeAll() {
        nodeInfo.resize(nNodes);
        nodeState.resize(nNodes);
        adjMatrix.resize(nNodes);
        for (int i = 0; i < nNodes; i++) {
            adjMatrix[i].resize(nNodes);
        }
    }

    void resetNodes() {
        for (int i = 0; i < nNodes; i++) {
            nodeState[i] = {};
        }
    }

    inline int getNode(int jobId) const {
        auto it = mapJobIdToNode.find(jobId);
        return (it != mapJobIdToNode.end()) ? it->second : -1;
    }

    int nPlanes{0};
    int nNodes{0};
    int planeTypePassengers{0};
    std::vector<Node> nodeInfo{};
    std::vector<NodeState> nodeState{};
    std::vector<std::vector<Edge>> adjMatrix{};

    std::unordered_map<int, int> mapJobIdToNode{};
};

class BotPlaner {
  public:
    enum class JobOwner { Planned, PlannedFreight, Backlog, BacklogFreight, TravelAgency, LastMinute, Freight, International, InternationalFreight };
    struct JobScheduled {
        JobScheduled() = default;
        JobScheduled(int idx, PlaneTime a, PlaneTime b) : jobIdx(idx), start(a), end(b) {}
        int jobIdx{-1};
        int objectId{-1};
        bool bIsFreight{false};
        float scoreRatio{0.0F};
        PlaneTime start{};
        PlaneTime end{};
    };
    struct JobToTake {
        JobToTake() = default;
        JobToTake(int i, int o, int s, JobOwner j) : jobIdx{i}, objectId(o), sourceId(s), owner(j) {}
        int jobIdx{-1};
        int objectId{-1};
        int sourceId{-1};
        JobOwner owner{};
    };
    struct Solution {
        Solution() = default;
        explicit Solution(int p) : totalPremium(p) {}
        std::deque<JobScheduled> jobs{};
        int totalPremium{0};
        int planeId{-1};
        PlaneTime scheduleFromTime{};
        inline bool empty() const { return jobs.empty(); }
    };
    struct SolutionList {
        SolutionList() = default;
        std::vector<JobToTake> toTake{};
        std::vector<Solution> list{};
        inline bool empty() const { return list.empty(); }
    };

    BotPlaner() = delete;
    BotPlaner(PLAYER &player, const CPlanes &planes);

    void addJobSource(JobOwner jobOwner, const std::vector<int> &intJobSource);

    /* score weighting factors */
    void setDistanceFactor(int val) { mFactors.distanceFactor = val; }
    void setPassengerFactor(int val) { mFactors.passengerFactor = val; }
    void setUhrigBonus(int val) { mFactors.uhrigBonus = val; }
    void setConstBonus(int val) { mFactors.constBonus = val; }
    void setFreightBonus(int val) { mFactors.freightBonus = val; }
    void setFreeFreightBonus(int val) { mFactors.freeFreightBonus = val; }
    void setMinScoreRatio(float val) { mMinScoreRatio = val; }
    void setMinScoreRatioLastMinute(float val) { mMinScoreRatioLastMinute = val; }
    void setMinSpeedRatio(float val) { mMinSpeedRatio = val; }

    SolutionList generateSolution(const std::vector<int> &planeIdsInput, const std::deque<int> &planeIdsExtraInput, int extraBufferTime);
    static bool takeAllJobs(PLAYER &qPlayer, SolutionList &solutions);
    static bool applySolution(PLAYER &qPlayer, const SolutionList &solutions);

  private:
    struct PlaneState {
        /* static data */
        int planeId{-1};
        int planeTypeId{-1};
        int startCity{-1};
        PlaneTime availTime{};
        Solution currentSolution{};
        int numNodes{0};
        int currentPremium{0};
        int currentCost{0};
    };

    struct Factors {
        int distanceFactor{0};
        int passengerFactor{0};
        int uhrigBonus{0};
        int constBonus{0};
        int freightBonus{0};
        int freeFreightBonus{0};
    };

    class FlightJob {
      public:
        FlightJob(int i, int j, CAuftrag a, JobOwner o);
        FlightJob(int i, int j, CFracht a, JobOwner o);

        inline int getId() const { return id; }
        inline int getSourceId() const { return sourceId; }
        inline JobOwner getOwner() const { return owner; }
        inline void setOwner(JobOwner o) { owner = o; }
        inline float getScoreRatio() const { return scoreRatio; }

        inline int getNumStillNeeded() const { return numStillNeeded; }
        inline int getNumToTransport() const { return numToTransport; }
        inline int getNumLocked() const { return numLocked; }
        inline int getNumNotLocked() const { return numNotLocked; }
        inline void setNumToTransport(int val) { numToTransport = numStillNeeded = val; }
        inline void addNumToTransport(int val) {
            numToTransport += val;
            numStillNeeded = numToTransport;
        }
        inline void reduceNumToTransport(int val) {
            numToTransport -= val;
            numStillNeeded = numToTransport;
        }
        inline void addNumLocked(int val) { numLocked += val; }
        inline void addNumNotLocked(int val) { numNotLocked += val; }
        inline bool wasTaken() const {
            return owner == JobOwner::Planned || owner == JobOwner::PlannedFreight || owner == JobOwner::Backlog || owner == JobOwner::BacklogFreight;
        }
        inline bool isFreight() const {
            return owner == JobOwner::PlannedFreight || owner == JobOwner::Freight || owner == JobOwner::InternationalFreight ||
                   owner == JobOwner::BacklogFreight;
        }
        inline int getStartCity() const { return startCity; }
        inline int getDestCity() const { return destCity; }
        inline int getDate() const { return isFreight() ? fracht.Date : auftrag.Date; }
        inline int getBisDate() const { return isFreight() ? fracht.BisDate : auftrag.BisDate; }
        inline int getPremium() const { return isFreight() ? fracht.Praemie : auftrag.Praemie; }
        inline int getPenalty() const { return isFreight() ? fracht.Strafe : auftrag.Strafe; }
        inline int getPersonen() const { return isFreight() ? 0 : auftrag.Personen; }
        inline int getTonsOpen() const { return isFreight() ? fracht.TonsOpen : 0; }
        inline int getTonsLeft() const { return isFreight() ? fracht.TonsLeft : 0; }
        inline int getOverscheduledTons() const { return (numStillNeeded < 0) ? (-numStillNeeded) : 0; }
        inline bool isScheduled() const { return (numStillNeeded < numToTransport); }
        inline bool isFullyScheduled() const { return (numStillNeeded <= 0); }
        inline void setIncompleteFreight() {
            assert(isFreight());
            incompleteFreight = true;
        }
        inline bool scheduledOK() const { return (numStillNeeded <= 0) || incompleteFreight; }
        inline void schedule(int passenger) {
            if (isFreight()) {
                numStillNeeded -= passenger / 10;
            } else {
                assert(numStillNeeded == 1);
                numStillNeeded = 0;
            }
        }
        inline void unschedule(int passenger) {
            if (isFreight()) {
                numStillNeeded += passenger / 10;
            } else {
                assert(numStillNeeded == 0);
                numStillNeeded = 1;
            }
        }

        std::string getName() const { return isFreight() ? Helper::getFreightName(fracht) : Helper::getJobName(auftrag); }
        void printInfo() const;

        int calculateDistance() const;
        std::pair<int, float> calculateScore(const Factors &f, int hours, int cost, int numRequired);

      private:
        CAuftrag auftrag{};
        CFracht fracht{};
        int id{};
        int sourceId{-1};
        JobOwner owner{};
        float scoreRatio{0};

        int numStillNeeded{1}; /* used in heuristic: how much to still plan (after mScheduleFromTime) */
        int numToTransport{1}; /* used in heuristic: how much to plan in total (after mScheduleFromTime) */
        int numLocked{0};      /* used for checking: how much is planned before mScheduleFromTime */
        int numNotLocked{0};   /* used for checking: how much is planned after mScheduleFromTime (intially) */
        bool incompleteFreight{false};
        int startCity{-1};
        int destCity{-1};
    };

    struct JobSource {
        JobSource(CAuftraege *ptr, JobOwner o) : jobs(ptr), owner(o) {}
        JobSource(CFrachten *ptr, JobOwner o) : freight(ptr), owner(o) {}
        CAuftraege *jobs{nullptr};
        CFrachten *freight{nullptr};
        int sourceId{-1};
        JobOwner owner{JobOwner::Backlog};
    };

    void printGraph(const Graph &g);

    /* preparation */
    void collectAllFlightJobs(const std::vector<int> &planeIds, const std::vector<int> &planeIdsExtra);
    void findPlaneTypes();
    std::vector<Graph> prepareGraph();

    /* in BotPlanerAlgo.cpp */
    inline int allPlaneGain() {
        int iterGain = 0;
        for (const auto &qPlaneState : mPlaneStates) {
            iterGain += qPlaneState.currentPremium - qPlaneState.currentCost;
        }
        return iterGain;
    }
    void printForPlane(const char *txt, int planeIdx, bool printOnErrorOnly);
    void savePath(int planeIdx, std::vector<int> &path) const;
    void restorePath(int planeIdx, const std::vector<int> &path);
    void killPath(int planeIdx);
    void printNodeInfo(const Graph &g, int nodeIdx) const;
    bool shiftLeft(Graph &g, int nodeToShift, int shiftT, bool commit);
    bool shiftRight(Graph &g, int nodeToShift, int shiftT, bool commit);
    int makeRoom(Graph &g, int nodeToMoveLeft, int nodeToMoveRight);
    void genSolutionsFromGraph(int planeIdx);
    int applySolutionToGraph();
    bool canInsert(const Graph &g, int currentNode, int nextNode) const;
    int findBestNeighbor(const Graph &g, int currentNode, int choice) const;
    void insertNode(Graph &g, int planeIdx, int currentNode, int nextNode);
    void removeNode(Graph &g, int planeIdx, int currentNode);
    int removeAllJobInstances(int jobIdx);
    int runRemoveWorst(int planeIdx, int numToRemove);
    int runPruneFreightJobs();
    bool runAddBestNeighbor(int planeIdx, int choice);
    bool runAddNodeToBestPlaneInner(int jobIdxToInsert);
    bool runAddNodeToBestPlane(int jobIdxToInsert);
    bool algo(int64_t timeBudget);

    /* apply solution */
    static bool removeInvalidFlightsForPlane(PLAYER &qPlayer, int planeId);
    static bool applySolutionForPlane(PLAYER &qPlayer, int planeId, const BotPlaner::Solution &solution);

    /* randomness */
    inline int getRandInt(int min, int max) {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(mMT);
    }

    PLAYER &qPlayer;
    std::vector<JobSource> mJobSources{};
    const CPlanes &qPlanes;
    PlaneTime mScheduleFromTime{};
    int mScheduleLastDay{};

    /* score weighting factors */
    Factors mFactors{};

    /* score thresholds */
    float mMinScoreRatio{1.0F};
    float mMinScoreRatioLastMinute{1.0F};
    float mMinSpeedRatio{0.0F};

    /* state */
    std::vector<const CPlane *> mPlaneTypeToPlane{};
    std::vector<FlightJob> mJobList{};
    std::vector<int> mJobListSorted{};
    std::unordered_map<int, int> mExistingJobsById{};
    std::unordered_map<int, int> mExistingFreightJobsById{};
    std::vector<PlaneState> mPlaneStates{};
    std::vector<int> mPlaneIdxSortedBySize{};
    std::vector<Graph> mGraphs{};

    /* randomness source */
    std::random_device mRD;
    std::mt19937 mMT{mRD()};
};

#endif // BOT_PLANER_H_
