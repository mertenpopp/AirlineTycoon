#ifndef BOT_PLANER_H_
#define BOT_PLANER_H_

#include "BotHelper.h"
#include "compat.h"

#include <cassert>
#include <climits>
#include <deque>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

extern int currentPass;

extern const int kAvailTimeExtra;
extern const int kDurationExtra;
extern const int kScheduleForNextDays;

extern int kNumToAdd;
extern int kNumBestToAdd;
extern int kNumToRemove;
extern int kTempStart;
extern int kTempStep;

class Graph {
  public:
    struct Node {
        int jobIdx{-1};
        int score{0};
        float scoreRatio{0};
        int duration{0};
        int earliest{0};
        int latest{INT_MAX};
        std::vector<int> bestNeighbors;
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
    std::vector<Node> nodeInfo;
    std::vector<NodeState> nodeState;
    std::vector<std::vector<Edge>> adjMatrix;

    std::unordered_map<int, int> mapJobIdToNode;
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
        PlaneTime start;
        PlaneTime end;
    };
    struct Solution {
        Solution() = default;
        Solution(int p) : totalPremium(p) {}
        std::deque<JobScheduled> jobs;
        int totalPremium{0};
        int planeId{-1};
        PlaneTime scheduleFromTime;
    };
    using SolutionList = std::vector<BotPlaner::Solution>;

    BotPlaner() = default;
    BotPlaner(PLAYER &player, const CPlanes &planes);

    void addJobSource(JobOwner jobOwner, std::vector<int> intJobSource);

    /* score weighting factors */
    void setDistanceFactor(int val) { mDistanceFactor = val; }
    void setPassengerFactor(int val) { mPassengerFactor = val; }
    void setUhrigBonus(int val) { mUhrigBonus = val; }
    void setConstBonus(int val) { mConstBonus = val; }
    void setFreightBonus(int val) { mFreightBonus = val; }
    void setFreeFreightBonus(int val) { mFreeFreightBonus = val; }
    void setMinScoreRatio(float val) { mMinScoreRatio = val; }
    void setMinScoreRatioLastMinute(float val) { mMinScoreRatioLastMinute = val; }

    SolutionList planFlights(const std::vector<int> &planeIdsInput, int extraBufferTime);
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

    class FlightJob {
      public:
        FlightJob(int i, int j, CAuftrag a, JobOwner o);
        FlightJob(int i, int j, CFracht a, JobOwner o);
        int getNumStillNeeded() const { return numStillNeeded; }
        void setNumToTransport(int val) { numToTransport = numStillNeeded = val; }
        void addNumToTransport(int val) {
            numToTransport += val;
            numStillNeeded = numToTransport;
        }
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

        int id{};
        int sourceId{-1};
        JobOwner owner;
        int score{0};
        float scoreRatio{0};

      private:
        CAuftrag auftrag;
        CFracht fracht;
        int numStillNeeded{1};
        int numToTransport{1};
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
    void findPlaneTypes();
    void collectAllFlightJobs(const std::vector<int> &planeIds);
    std::vector<Graph> prepareGraph();

    /* in BotPlanerAlgo.cpp */
    inline int allPlaneGain() {
        int iterGain = 0;
        for (int planeIdx = 0; planeIdx < mPlaneStates.size(); planeIdx++) {
            iterGain += mPlaneStates[planeIdx].currentPremium - mPlaneStates[planeIdx].currentCost;
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
    void applySolutionToGraph();
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
    bool takeJobs(Solution &currentSolution);
    static bool applySolutionForPlane(PLAYER &qPlayer, int planeId, const BotPlaner::Solution &solution);

    /* randomness */
    inline int getRandInt(int min, int max) {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(mMT);
    }

    PLAYER &qPlayer;
    std::vector<JobSource> mJobSources;
    const CPlanes &qPlanes;
    PlaneTime mScheduleFromTime;

    /* score weighting factors */
    int mDistanceFactor{0};
    int mPassengerFactor{0};
    int mUhrigBonus{0};
    int mConstBonus{0};
    float mMinScoreRatio{1.0f};
    float mMinScoreRatioLastMinute{1.0f};
    int mFreightBonus{0};
    int mFreeFreightBonus{0};

    /* state */
    std::vector<const CPlane *> mPlaneTypeToPlane;
    std::vector<FlightJob> mJobList;
    std::unordered_map<int, int> mExistingJobsById;
    std::unordered_map<int, int> mExistingFreightJobsById;
    std::vector<PlaneState> mPlaneStates;
    std::vector<int> mPlaneIdxSortedBySize;
    std::vector<Graph> mGraphs;

    /* randomness source */
    std::random_device mRD;
    std::mt19937 mMT{mRD()};
};

#endif // BOT_PLANER_H_
