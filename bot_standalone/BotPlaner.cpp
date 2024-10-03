#include "BotPlaner.h"

#include "compat_global.h"
#include "compat_misc.h"
#include "GameMechanic.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <unordered_map>

// #define PRINT_DETAIL 1
#define PRINT_OVERALL 1

int currentPass = 0;

const int kAvailTimeExtra = 2;
const int kDurationExtra = 1;
const int kScheduleForNextDays = 4;
const int64_t timeBudgetMS = 100;
const int kFreightMaxFlights = 4;

inline constexpr int ceil_div(int a, int b) {
    assert(b != 0);
    return a / b + (a % b != 0);
}

inline bool canFlyThisJob(const CPlane &qPlane, int passengers, int distance, int duration) {
    if (passengers > qPlane.ptPassagiere) {
        return false;
    }
    if (distance > qPlane.ptReichweite * 1000) {
        return false;
    }
    if (duration >= 24) {
        return false;
    }
    return true;
}

inline void calcCostAndDuration(int startCity, int destCity, const CPlane &qPlane, bool emptyFlight, int &cost, int &duration, int &distance) {
    assert(startCity >= 0 && startCity < Cities.AnzEntries());
    assert(destCity >= 0 && destCity < Cities.AnzEntries());
    /* needs to match CITIES::CalcFlugdauer() */
    distance = Cities.CalcDistance(startCity, destCity);
    duration = (distance / qPlane.ptGeschwindigkeit + 999) / 1000 + 1 + 2 - 2;
    if (duration < 2) {
        duration = 2;
    }

    /* needs to match CalculateFlightKerosin() */
    SLONG kerosene = distance / 1000            // weil Distanz in m übergeben wird
                     * qPlane.ptVerbrauch / 160 // Liter pro Barrel
                     / qPlane.ptGeschwindigkeit;

    /* needs to match CalculateFlightCostNoTank() */
    cost = kerosene * Sim.Kerosin;
    if (cost < 1000) {
        cost = 1000;
    }

    if (emptyFlight) {
        cost -= (qPlane.ptPassagiere * distance / 1000 / 40);
    }
}

BotPlaner::FlightJob::FlightJob(int i, int j, CAuftrag a, JobOwner o) : id(i), sourceId(j), owner(o), auftrag(a) {
    assert(i >= 0x1000000);
    startCity = Cities.find(auftrag.VonCity);
    destCity = Cities.find(auftrag.NachCity);
}

BotPlaner::FlightJob::FlightJob(int i, int j, CFracht a, JobOwner o) : id(i), sourceId(j), owner(o), fracht(a) {
    assert(i >= 0x1000000);
    startCity = Cities.find(fracht.VonCity);
    destCity = Cities.find(fracht.NachCity);
}

BotPlaner::BotPlaner(PLAYER &player, const CPlanes &planes) : qPlayer(player), qPlanes(planes) {}

void BotPlaner::addJobSource(JobOwner jobOwner, std::vector<int> intJobSource) {
    if (jobOwner == JobOwner::International) {
        for (const auto &i : intJobSource) {
            if (i < 0 || i >= AuslandsAuftraege.size()) {
                redprintf("BotPlaner::addJobSource(): Invalid intJobSource given: %d", i);
                return;
            }
        }
        if (intJobSource.empty()) {
            redprintf("BotPlaner::addJobSource(): No intJobSource given.");
            return;
        }
    } else if (jobOwner == JobOwner::InternationalFreight) {
        for (const auto &i : intJobSource) {
            if (i < 0 || i >= AuslandsFrachten.size()) {
                redprintf("BotPlaner::addJobSource(): Invalid intJobSource given.");
                return;
            }
        }
        if (intJobSource.empty()) {
            redprintf("BotPlaner::addJobSource(): No intJobSource given.");
            return;
        }
    } else {
        if (!intJobSource.empty()) {
            redprintf("BotPlaner::addJobSource(): intJobSource does not need to be given for this job source.");
            return;
        }
    }

    if (jobOwner == JobOwner::TravelAgency) {
        mJobSources.emplace_back(&ReisebueroAuftraege, jobOwner);
    } else if (jobOwner == JobOwner::LastMinute) {
        mJobSources.emplace_back(&LastMinuteAuftraege, jobOwner);
    } else if (jobOwner == JobOwner::Freight) {
        mJobSources.emplace_back(&gFrachten, jobOwner);
    } else if (jobOwner == JobOwner::International) {
        for (const auto &i : intJobSource) {
            mJobSources.emplace_back(&AuslandsAuftraege[i], jobOwner);
            mJobSources.back().sourceId = i;
        }
    } else if (jobOwner == JobOwner::InternationalFreight) {
        for (const auto &i : intJobSource) {
            mJobSources.emplace_back(&AuslandsFrachten[i], jobOwner);
            mJobSources.back().sourceId = i;
        }
    }
}

void BotPlaner::printGraph(const Graph &g) {
    std::cout << "digraph G {" << std::endl;
    for (int i = 0; i < g.nNodes; i++) {
        auto &curInfo = g.nodeInfo[i];

        std::cout << "node" << i << " [";
        if (i >= g.nPlanes) {
            auto &job = mJobList[curInfo.jobIdx];
            std::cout << "label=\"" << (LPCTSTR)Cities[job.getStartCity()].Kuerzel << " => " << (LPCTSTR)Cities[job.getDestCity()].Kuerzel << "\\n";
            // std::cout << "(" << job.Personen  << ", " << CalcDistance(job.VonCity, job.NachCity);
            // std::cout << ", P: " << job.Praemie << " $, F: " << job.Strafe << " $)\\n";
            std::cout << "earning " << curInfo.score << " $, " << curInfo.duration << " hours\\n";
            std::cout << "earliest: " << Helper::getWeekday(curInfo.earliest) << ", latest " << Helper::getWeekday(curInfo.latest) << "\\n";
            std::cout << "\"";
        } else {
            const auto &planeState = mPlaneStates[i];
            const auto &qPlane = qPlanes[planeState.planeId];
            std::cout << "label=\"start for " << qPlane.Name << "\",shape=Mdiamond";
        }
        std::cout << "];" << std::endl;
    }
    for (int i = 0; i < g.nNodes; i++) {
        auto &curInfo = g.nodeInfo[i];
        for (int j = 0; j < g.nNodes; j++) {
            if (g.adjMatrix[i][j].cost < 0) {
                continue;
            }
            std::cout << "node" << i << " -> "
                      << "node" << j << " [";
            std::cout << "label=\"" << g.adjMatrix[i][j].cost << " $, " << g.adjMatrix[i][j].duration << " h\"";
            for (int n = 0; n < 3 && n < curInfo.bestNeighbors.size(); n++) {
                if (curInfo.bestNeighbors[n] == j) {
                    std::cout << ", penwidth=" << (8 - 2 * n);
                    break;
                }
            }
            std::cout << "];" << std::endl;
        }
    }
    std::cout << "}" << std::endl;
}

void BotPlaner::collectAllFlightJobs(const std::vector<int> &planeIds) {
    mJobList.clear();

    /* jobs already in planer */
    mJobSources.emplace_back(&qPlayer.Auftraege, JobOwner::Backlog);

    /* freight jobs already in planer */
    mJobSources.emplace_back(&qPlayer.Frachten, JobOwner::BacklogFreight);

    /* create list of open jobs */
    for (const auto &source : mJobSources) {
        /* job source B: Jobs */
        for (int i = 0; source.jobs && i < source.jobs->AnzEntries(); i++) {
            if (source.jobs->IsInAlbum(i) == 0) {
                continue;
            }
            const auto &job = source.jobs->at(i);
            if ((job.VonCity == job.NachCity) || (job.InPlan != 0) || (job.Praemie < 0)) {
                continue;
            }
            if (job.Date <= Sim.Date + kScheduleForNextDays) {
                mJobList.emplace_back(source.jobs->GetIdFromIndex(i), source.sourceId, job, source.owner);
                int score = job.Praemie;
                score += mDistanceFactor * Cities.CalcDistance(job.VonCity, job.NachCity);
                score += mPassengerFactor * job.Personen;
                score += mUhrigBonus * job.bUhrigFlight;
                score += mConstBonus;

                mJobList.back().score = score;
                mJobList.back().scoreRatio = 1.0f * score / CalculateFlightCostNoTank(job.VonCity, job.NachCity, 8000, 700);
            }
        }
        /* job source B: Freight */
        for (int i = 0; source.freight && i < source.freight->AnzEntries(); i++) {
            if (source.freight->IsInAlbum(i) == 0) {
                continue;
            }
            const auto &job = source.freight->at(i);
            if ((job.VonCity == job.NachCity) || (job.InPlan != 0) || (job.Praemie < 0)) {
                continue;
            }
            if (job.Date <= Sim.Date + kScheduleForNextDays) {
                mJobList.emplace_back(source.freight->GetIdFromIndex(i), source.sourceId, job, source.owner);
                int score = job.Praemie;
                score += mDistanceFactor * Cities.CalcDistance(job.VonCity, job.NachCity);
                score += mConstBonus;
                score += mFreightBonus;
                if (job.Praemie == 0) {
                    score += mFreeFreightBonus;
                }

                mJobList.back().score = score;
                mJobList.back().scoreRatio = 1.0f * score / CalculateFlightCostNoTank(job.VonCity, job.NachCity, 8000, 700);
                mJobList.back().setNumToTransport(job.TonsLeft);
                assert(job.TonsLeft <= job.Tons);
            }
        }
    }

    mJobSources.clear();

    /* add jobs that will be re-planned */
    std::unordered_map<SLONG, int> jobs; /* to only count freight jobs once */
    for (auto i : planeIds) {
        const auto &qFlightPlan = qPlanes[i].Flugplan.Flug;
        for (int c = qFlightPlan.AnzEntries() - 1; c >= 0; c--) {
            const auto &qFPE = qFlightPlan[c];
            if (PlaneTime{qFPE.Startdate, qFPE.Startzeit} < mScheduleFromTime) {
                continue;
            }
            if (qFPE.ObjectType == 2) {
                const auto &job = qPlayer.Auftraege[qFPE.ObjectId];
                mJobList.emplace_back(qFPE.ObjectId, -1, job, JobOwner::Planned);
                mJobList.back().score = job.Praemie;
                mJobList.back().scoreRatio = 1.0f * job.Praemie / CalculateFlightCostNoTank(job.VonCity, job.NachCity, 8000, 700);
            } else if (qFPE.ObjectType == 4) {
                if (jobs.find(qFPE.ObjectId) == jobs.end()) {
                    jobs[qFPE.ObjectId] = mJobList.size();

                    const auto &job = qPlayer.Frachten[qFPE.ObjectId];
                    mJobList.emplace_back(qFPE.ObjectId, -1, job, JobOwner::PlannedFreight);
                    mJobList.back().score = job.Praemie;
                    mJobList.back().scoreRatio = 1.0f * job.Praemie / CalculateFlightCostNoTank(job.VonCity, job.NachCity, 8000, 700);
                    mJobList.back().setNumToTransport(qPlanes[i].ptPassagiere / 10);
                } else {
                    mJobList[jobs[qFPE.ObjectId]].addNumToTransport(qPlanes[i].ptPassagiere / 10);
                }
            }
        }
    }

    /* sort list of jobs */
    std::sort(mJobList.begin(), mJobList.end(), [](const FlightJob &a, const FlightJob &b) {
        if (a.wasTaken() != b.wasTaken()) {
            return a.wasTaken();
        }
        return a.scoreRatio > b.scoreRatio;
    });

    for (int i = 0; i < mJobList.size(); i++) {
        auto &job = mJobList[i];
        if (job.owner == JobOwner::Planned) {
            mExistingJobsById[job.id] = i;
        }
        if (job.owner == JobOwner::PlannedFreight) {
            mExistingFreightJobsById[job.id] = i;

            /* if there are tons open it means that the existing schedule already did not have enough instances of this freight job */
            if (job.getTonsOpen() > 0) {
                redprintf("BotPlaner::collectAllFlightJobs(): Existing plane schedules did not have enough instances for job %s: %ld open, %ld left.",
                          job.getName().c_str(), job.getTonsOpen(), job.getTonsLeft());
                job.addNumToTransport(job.getTonsOpen());
                job.setIncompleteFreight();
            }
            /* limit numToTransport to total amount of tons left */
            if (job.getNumStillNeeded() > job.getTonsLeft()) {
                job.setNumToTransport(job.getTonsLeft());
            }
        }
    }
}

void BotPlaner::findPlaneTypes() {
    int nPlaneTypes = 0;
    std::unordered_map<int, int> mapOldType2NewType;
    for (auto &planeState : mPlaneStates) {
        const auto &qPlane = qPlanes[planeState.planeId];
        if (qPlane.TypeId == -1) {
            planeState.planeTypeId = nPlaneTypes++;
            mPlaneTypeToPlane.push_back(&qPlane);
            continue;
        }
        if (mapOldType2NewType.end() == mapOldType2NewType.find(qPlane.TypeId)) {
            mapOldType2NewType[qPlane.TypeId] = nPlaneTypes++;
            mPlaneTypeToPlane.push_back(&qPlane);
        }
        planeState.planeTypeId = mapOldType2NewType[qPlane.TypeId];
    }
    assert(mPlaneTypeToPlane.size() == nPlaneTypes);

#ifdef PRINT_DETAIL
    std::cout << "There are " << mPlaneStates.size() << " planes of " << mPlaneTypeToPlane.size() << " types" << std::endl;
#endif
}

std::vector<Graph> BotPlaner::prepareGraph() {
    int nPlanes = mPlaneStates.size();
    int nPlaneTypes = mPlaneTypeToPlane.size();

    /* prepare graph */
    std::vector<Graph> graphs;
    graphs.reserve(nPlaneTypes);
    for (int pt = 0; pt < nPlaneTypes; pt++) {
        const auto *plane = mPlaneTypeToPlane[pt];
        graphs.emplace_back(nPlanes, plane->ptPassagiere);
        auto &g = graphs.back();

        /* nodes for jobs */
        for (int jobIdx = 0; jobIdx < mJobList.size(); jobIdx++) {
            const auto &job = mJobList[jobIdx];

            /* freight job way too big for plane? */
            int numRequired = 1;
            if (job.isFreight()) {
                numRequired = ceil_div(job.getNumStillNeeded(), plane->ptPassagiere / 10);
            }
            if (numRequired > kFreightMaxFlights) {
                continue;
            }

            /* calculate cost, duration and distance */
            int cost = 0;
            int duration = 0;
            int distance = 0;
            calcCostAndDuration(job.getStartCity(), job.getDestCity(), *plane, false, cost, duration, distance);

            /* check job score */
            int score = job.score - cost;
            int scoreRatio = 1.0f * job.score / (cost * numRequired);
            auto minScore = (job.getBisDate() == Sim.Date) ? mMinScoreRatioLastMinute : mMinScoreRatio;
            if (!job.wasTaken() && scoreRatio < minScore) {
                continue; /* job was not taken yet and does not have required minimum score */
            }

            if (canFlyThisJob(*plane, job.getPersonen(), distance, duration)) {
                auto n = numRequired;
                while (n-- > 0) {
                    int node = g.addNode(jobIdx);
                    auto &qNodeInfo = g.nodeInfo[node];

                    qNodeInfo.jobIdx = jobIdx;
                    qNodeInfo.earliest = job.getDate();
                    qNodeInfo.latest = job.getBisDate();
                    qNodeInfo.score = score;
                    qNodeInfo.scoreRatio = scoreRatio;
                    qNodeInfo.duration = duration + kDurationExtra;
                }
            }
        }

        g.resizeAll();

        /* edges */
        for (int i = 0; i < g.nNodes; i++) {
            std::vector<std::pair<int, int>> neighborList;
            if (kNumToAdd > 0) {
                neighborList.reserve(g.nNodes);
            }
            for (int j = nPlanes; j < g.nNodes; j++) {
                if (i == j) {
                    continue; /* self edge not allowed */
                }

                const auto &destJob = mJobList[g.nodeInfo[j].jobIdx];
                int startCity = (i >= nPlanes) ? mJobList[g.nodeInfo[i].jobIdx].getDestCity() : mPlaneStates[i].startCity;
                int destCity = destJob.getStartCity();
                if (startCity != destCity) {
                    int cost = 0;
                    int duration = 0;
                    int distance = 0;
                    calcCostAndDuration(startCity, destCity, *plane, true, cost, duration, distance);
                    g.adjMatrix[i][j].cost = cost;
                    g.adjMatrix[i][j].duration = duration + kDurationExtra;
                } else {
                    g.adjMatrix[i][j].cost = 0;
                    g.adjMatrix[i][j].duration = 0;
                }

                if (kNumToAdd == 0 || destJob.isFreight()) {
                    continue; /* we are not using the algo that utilizes bestNeighbors */
                }

                if (g.nodeInfo[i].earliest > g.nodeInfo[j].latest) {
                    continue; /* not possible because date constraints */
                }

                neighborList.emplace_back(j, g.nodeInfo[j].score - g.adjMatrix[i][j].cost);
            }

            std::sort(neighborList.begin(), neighborList.end(), [](const std::pair<int, int> &a, const std::pair<int, int> &b) { return a.second > b.second; });
            for (const auto &n : neighborList) {
                g.nodeInfo[i].bestNeighbors.push_back(n.first);
            }
        }
#ifdef PRINT_DETAIL
        printGraph(graphs[pt]);
#endif
    }
    return graphs;
}

bool BotPlaner::takeJobs(Solution &currentSolution) {
    bool ok = true;
    for (auto &jobScheduled : currentSolution.jobs) {
        auto &job = mJobList[jobScheduled.jobIdx];
        if (!job.isScheduled()) {
            continue;
        }

        /* take jobs that have not been taken yet */
        if (!job.wasTaken()) {
            int outAuftragsId = -1;
            switch (job.owner) {
            case JobOwner::TravelAgency:
                GameMechanic::takeFlightJob(qPlayer, job.id, outAuftragsId);
                break;
            case JobOwner::LastMinute:
                GameMechanic::takeLastMinuteJob(qPlayer, job.id, outAuftragsId);
                break;
            case JobOwner::Freight:
                GameMechanic::takeFreightJob(qPlayer, job.id, outAuftragsId);
                break;
            case JobOwner::International:
                assert(job.sourceId != -1);
                GameMechanic::takeInternationalFlightJob(qPlayer, job.sourceId, job.id, outAuftragsId);
                break;
            case JobOwner::InternationalFreight:
                assert(job.sourceId != -1);
                GameMechanic::takeInternationalFreightJob(qPlayer, job.sourceId, job.id, outAuftragsId);
                break;
            default:
                redprintf("BotPlaner::takeJobs(): Unknown job source: %d", job.owner);
                return false;
            }

            if (outAuftragsId < 0) {
                redprintf("BotPlaner::takeJobs(): GameMechanic returned error when trying to take job!");
                ok = false;
                continue;
            }
            job.id = outAuftragsId;
            if (job.isFreight()) {
                job.owner = JobOwner::BacklogFreight;
            } else {
                job.owner = JobOwner::Backlog;
            }
        }
        jobScheduled.objectId = job.id;
    }
    return ok;
}

bool BotPlaner::applySolutionForPlane(PLAYER &qPlayer, int planeId, const BotPlaner::Solution &solution) {
    const auto &qPlanes = qPlayer.Planes;
    if (planeId < 0x1000000 || !qPlanes.IsInAlbum(planeId)) {
        redprintf("BotPlaner::applySolutionForPlane(): Skipping invalid plane: %d", planeId);
        return false;
    }

    std::unordered_map<int, JobScheduled> jobHash;
    std::unordered_map<int, std::vector<JobScheduled>> freightHash;
    for (auto &iter : solution.jobs) {
        const auto startTime = iter.start;

        /* plan taken jobs */
        if (!iter.bIsFreight) {
            if (!qPlayer.Auftraege.IsInAlbum(iter.objectId)) {
                redprintf("BotPlaner::applySolutionForPlane(): Skipping invalid job: %d", iter.objectId);
                continue;
            }
            jobHash[iter.objectId] = iter;
            const auto &auftrag = qPlayer.Auftraege[iter.objectId];
            if (!GameMechanic::planFlightJob(qPlayer, planeId, iter.objectId, startTime.getDate(), startTime.getHour())) {
                redprintf("BotPlaner::applySolutionForPlane(): GameMechanic::planFlightJob returned error!");
                redprintf("Tried to schedule %s at %s %d", Helper::getJobName(auftrag).c_str(), Helper::getWeekday(startTime.getDate()).c_str(),
                          startTime.getHour());
            }
        } else {
            if (!qPlayer.Frachten.IsInAlbum(iter.objectId)) {
                redprintf("BotPlaner::applySolutionForPlane(): Skipping invalid freight job: %d", iter.objectId);
                continue;
            }
            freightHash[iter.objectId].push_back(iter);
            const auto &auftrag = qPlayer.Frachten[iter.objectId];
            if (!GameMechanic::planFreightJob(qPlayer, planeId, iter.objectId, startTime.getDate(), startTime.getHour())) {
                redprintf("BotPlaner::applySolutionForPlane(): GameMechanic::planFreightJob returned error!");
                redprintf("Tried to schedule %s at %s %d", Helper::getFreightName(auftrag).c_str(), Helper::getWeekday(startTime.getDate()).c_str(),
                          startTime.getHour());
            }
        }
    }

    /* sort instances of freight jobs */
    for (auto &iter : freightHash) {
        std::sort(iter.second.begin(), iter.second.end(), [](const JobScheduled &a, const JobScheduled &b) { return a.start > b.start; });
    }

    /* check flight time */
    bool ok = true;
    const auto &qFlightPlan = qPlanes[planeId].Flugplan.Flug;
    for (int d = 0; d < qFlightPlan.AnzEntries(); d++) {
        const auto &qFPE = qFlightPlan[d];
        if (PlaneTime{qFPE.Startdate, qFPE.Startzeit} < solution.scheduleFromTime) {
            continue;
        }
        if (qFPE.ObjectType != 2 && qFPE.ObjectType != 4) {
            continue;
        }
        bool isFreight = (qFPE.ObjectType == 4);
        const auto strJob = isFreight ? Helper::getFreightName(qPlayer.Frachten[qFPE.ObjectId]) : Helper::getJobName(qPlayer.Auftraege[qFPE.ObjectId]);

        JobScheduled iter;
        if (isFreight) {
            if (freightHash.find(qFPE.ObjectId) == freightHash.end()) {
                redprintf("BotPlaner::applySolutionForPlane(): Plane %s, schedule entry %d: Unknown freight job scheduled: %s", (LPCTSTR)qPlanes[planeId].Name,
                          d, strJob.c_str());
                continue;
            }
            if (freightHash[qFPE.ObjectId].empty()) {
                redprintf("BotPlaner::applySolutionForPlane(): Plane %s, schedule entry %d: Too many instances of freight job scheduled: %s",
                          (LPCTSTR)qPlanes[planeId].Name, d, strJob.c_str());
                continue;
            }
            iter = freightHash[qFPE.ObjectId].back();
            freightHash[qFPE.ObjectId].pop_back();
        } else {
            if (jobHash.find(qFPE.ObjectId) == jobHash.end()) {
                continue;
            }
            iter = jobHash[qFPE.ObjectId];
            jobHash.erase(qFPE.ObjectId);
        }

        const auto startTime = iter.start;
        const auto endTime = iter.end - kDurationExtra;

        if (PlaneTime(qFPE.Startdate, qFPE.Startzeit) != startTime) {
            redprintf("BotPlaner::applySolutionForPlane(): Plane %s, schedule entry %d: GameMechanic scheduled job (%s) at different start time (%s %d "
                      "instead of %s %d)!",
                      (LPCTSTR)qPlanes[planeId].Name, d, strJob.c_str(), (LPCTSTR)Helper::getWeekday(qFPE.Startdate), qFPE.Startzeit,
                      (LPCTSTR)Helper::getWeekday(startTime.getDate()), startTime.getHour());
            ok = false;
        }
        if (PlaneTime(qFPE.Landedate, qFPE.Landezeit) != endTime) {
            redprintf("BotPlaner::applySolutionForPlane(): Plane %s, schedule entry %d: GameMechanic scheduled job (%s) with different landing time (%s %d "
                      "instead of %s %d)!",
                      (LPCTSTR)qPlanes[planeId].Name, d, strJob.c_str(), (LPCTSTR)Helper::getWeekday(qFPE.Landedate), qFPE.Landezeit,
                      (LPCTSTR)Helper::getWeekday(endTime.getDate()), endTime.getHour());
            ok = false;
        }
    }

    /* check for jobs that have not been scheduled */
    for (const auto &iter : jobHash) {
        const auto strJob = Helper::getJobName(qPlayer.Auftraege[iter.second.objectId]);
        redprintf("BotPlaner::applySolutionForPlane(): Did not find job %s in flight plan!", strJob.c_str());
        ok = false;
    }
    for (const auto &iter : freightHash) {
        if (!iter.second.empty()) {
            const auto strJob = Helper::getFreightName(qPlayer.Frachten[iter.second.front().objectId]);
            redprintf("BotPlaner::applySolutionForPlane(): Not enough instances of freight job %s in flight plan (%d missing)!", strJob.c_str(),
                      iter.second.size());
            ok = false;
        }
    }

    return ok;
}

BotPlaner::SolutionList BotPlaner::planFlights(const std::vector<int> &planeIdsInput, int extraBufferTime) {
    auto t_begin = std::chrono::steady_clock::now();

    if (mDistanceFactor > 0) {
        hprintf("BotPlaner::planFlights(): Using mDistanceFactor = %d", mDistanceFactor);
    }
    if (mPassengerFactor > 0) {
        hprintf("BotPlaner::planFlights(): Using mPassengerFactor = %d", mPassengerFactor);
    }
    if (mUhrigBonus > 0) {
        hprintf("BotPlaner::planFlights(): Using mUhrigBonus = %d", mUhrigBonus);
    }
    if (mConstBonus > 0) {
        hprintf("BotPlaner::planFlights(): Using mConstBonus = %d", mConstBonus);
    }

#ifdef PRINT_DETAIL
    hprintf("BotPlaner::planFlights(): Current time: %d", Sim.GetHour());
#endif
    mScheduleFromTime = {Sim.Date, Sim.GetHour() + extraBufferTime};

    /* find valid plane IDs */
    std::vector<int> planeIds;
    for (auto i : planeIdsInput) {
        if (qPlanes.IsInAlbum(i) != 0) {
            planeIds.push_back(i);
        }
    }

    /* prepare list of jobs */
    collectAllFlightJobs(planeIds);

    /* statistics of existing jobs */
    int nPreviouslyOwned = 0;
    int nNewJobs = 0;
    for (const auto &job : mJobList) {
        if (job.wasTaken()) {
            nPreviouslyOwned++;
        } else {
            nNewJobs++;
        }
    }

    /* prepare list of planes */
    mPlaneStates.clear();
    mPlaneStates.reserve(planeIds.size());
    for (auto i : planeIds) {
        assert(i >= 0x1000000 && qPlanes.IsInAlbum(i));

        mPlaneStates.push_back({});
        auto &planeState = mPlaneStates.back();
        planeState.planeId = i;

        /* determine when and where the plane will be available */
        std::tie(planeState.availTime, planeState.startCity) = Helper::getPlaneAvailableTimeLoc(qPlanes[i], mScheduleFromTime, mScheduleFromTime);
        planeState.startCity = Cities.find(planeState.startCity);
        assert(planeState.startCity >= 0 && planeState.startCity < Cities.AnzEntries());
#ifdef PRINT_DETAIL
        Helper::checkPlaneSchedule(qPlayer, i, false);
        hprintf("BotPlaner::planFlights(): After %s %d: Plane %s is in %s @ %s %d", (LPCTSTR)Helper::getWeekday(mScheduleFromTime.getDate()),
                mScheduleFromTime.getHour(), (LPCTSTR)qPlanes[i].Name, (LPCTSTR)Cities[planeState.startCity].Kuerzel,
                (LPCTSTR)Helper::getWeekday(planeState.availTime.getDate()), planeState.availTime.getHour());
#endif
    }

    /* sort by carrying capacity, smallest first */
    mPlaneIdxSortedBySize.clear();
    mPlaneIdxSortedBySize.reserve(planeIds.size());
    for (int i = 0; i < mPlaneStates.size(); i++) {
        mPlaneIdxSortedBySize.push_back(i);
    }
    std::sort(mPlaneIdxSortedBySize.begin(), mPlaneIdxSortedBySize.end(),
              [&](int a, int b) { return qPlanes[mPlaneStates[a].planeId].ptPassagiere < qPlanes[mPlaneStates[b].planeId].ptPassagiere; });

    /* find number of unique plane types */
    findPlaneTypes();

    /* prepare graph */
    mGraphs = prepareGraph();

    /* start algo */
    auto t_current = std::chrono::steady_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(t_current - t_begin).count();
    bool improved = algo(timeBudgetMS - diff);

    /* check statistics */
    int nPreviouslyOwnedScheduled = 0;
    int nNewJobsScheduled = 0;
    for (const auto &job : mJobList) {
        if (!job.isScheduled()) {
            continue;
        }
        if (job.wasTaken()) {
            nPreviouslyOwnedScheduled++;
        } else {
            nNewJobsScheduled++;
        }
    }

#ifdef PRINT_OVERALL
    hprintf("Scheduled %d out of %d existing jobs.", nPreviouslyOwnedScheduled, nPreviouslyOwned);
    hprintf("Scheduled %d out of %d new jobs.", nNewJobsScheduled, nNewJobs);
#endif

#ifdef PRINT_OVERALL
    auto t_end = std::chrono::steady_clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_begin).count();
    hprintf("Elapsed time in total: %lld ms", delta);
#endif

    if (!improved) {
        hprintf("Scheduling algo did not improve anything, returning empty solution.");
        return {};
    }

    /* now take all jobs */
    for (auto &planeState : mPlaneStates) {
        takeJobs(planeState.currentSolution);
    }

    SolutionList list;
    for (const auto &plane : mPlaneStates) {
        list.emplace_back(std::move(plane.currentSolution));
    }
    return list;
}

bool BotPlaner::applySolution(PLAYER &qPlayer, const SolutionList &solutions) {
    /* kill existing flight plans */
    for (const auto &solution : solutions) {
        int planeId = solution.planeId;
        auto time = solution.scheduleFromTime;
        if (!GameMechanic::killFlightPlanFrom(qPlayer, planeId, time.getDate(), time.getHour())) {
            return false;
        }
    }

    /* apply solution */
    Helper::ScheduleInfo overallInfo;
    SLONG totalDiff = 0;
    for (const auto &solution : solutions) {
        int planeId = solution.planeId;

        auto oldInfo = Helper::calculateScheduleInfo(qPlayer, planeId);
        applySolutionForPlane(qPlayer, planeId, solution);

        auto newInfo = Helper::calculateScheduleInfo(qPlayer, planeId);
        overallInfo += newInfo;
        SLONG diff = newInfo.gain - oldInfo.gain;
        totalDiff += diff;

#ifdef PRINT_DETAIL
        Helper::checkPlaneSchedule(qPlayer, planeId, false);
        if (diff > 0) {
            hprintf("%s: Improved gain: %d => %d (+%d)", (LPCTSTR)qPlayer.Planes[planeId].Name, oldInfo.gain, newInfo.gain, diff);
        } else {
            hprintf("%s: Gain did not improve: %d => %d (%d)", (LPCTSTR)qPlayer.Planes[planeId].Name, oldInfo.gain, newInfo.gain, diff);
        }
#endif
    }

#ifdef PRINT_DETAIL
    if (totalDiff > 0) {
        hprintf("Total gain improved: %s $ (+%s $)", Insert1000erDots(overallInfo.gain).c_str(), Insert1000erDots(totalDiff).c_str());
    } else if (totalDiff == 0) {
        hprintf("Total gain did not change: %s $", Insert1000erDots(overallInfo.gain).c_str());
    } else {
        hprintf("Total gain got worse: %s $ (%s $)", Insert1000erDots(overallInfo.gain).c_str(), Insert1000erDots(totalDiff).c_str());
    }
#endif

#ifdef PRINT_OVERALL
    overallInfo.printDetails();
#endif

    return true;
}
