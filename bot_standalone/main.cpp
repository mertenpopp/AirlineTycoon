#include "BotHelper.h"
#include "BotPlaner.h"
#include "compat_global.h"
#include "compat_misc.h"
#include "GameMechanic.h"

#include <iostream>

static const int numAveraged = 5;
static const float kSchedulingMinScoreRatio = 15.0f;
static const float kSchedulingMinScoreRatioLastMinute = 5.0f;

void findBestPlaneTypes();
int64_t run(int numPasses, int freightMode);
int64_t _run(int numPasses, int freightMode, const std::vector<int> &planeIds);
int64_t benchmark(int numPasses, int freightMode);

void findBestPlaneTypes() {
    std::vector<int> bestPlaneTypes;
    for (int i = 0; i < 10; i++) {
        int64_t bestGain = 0;
        int bestType = -1;
        for (int newType = 100; newType <= 126; newType++) {
            TEAKRAND rand;
            rand.SRandTime();
            Init(rand.Rand(30, 100));

            if (!PlaneTypes.IsInAlbum(newType + 0x10000000)) {
                continue;
            }
            std::cout << "Checking plane type " << PlaneTypes[newType + 0x10000000].Name << std::endl;

            auto &qPlayer = Sim.Players.Players[0];

            std::vector<int> planeIds;
            for (const auto &type : bestPlaneTypes) {
                planeIds.push_back(qPlayer.BuyPlane(type, nullptr));
            }
            planeIds.push_back(qPlayer.BuyPlane(newType, nullptr));

            auto gain = _run(1, 0, planeIds);
            if (gain > bestGain) {
                bestGain = gain;
                bestType = newType;
            }
        }

        if (-1 == bestType) {
            break;
        }
        std::cout << "Best next plane type is " << PlaneTypes[bestType + 0x10000000].Name << std::endl;
        bestPlaneTypes.push_back(bestType);
    }

    for (const auto &i : bestPlaneTypes) {
        std::cout << "Final plane list: " << PlaneTypes[i + 0x10000000].Name << std::endl;
    }
}

int64_t run(int numPasses, int freightMode) {
    Init(50);
    auto &qPlayer = Sim.Players.Players[0];
    std::vector<int> planeIds;
    for (int i = 0; i < 10; i++) {
        planeIds.push_back(qPlayer.BuyPlane(106, nullptr));
        planeIds.push_back(qPlayer.BuyPlane(111, nullptr));
        planeIds.push_back(qPlayer.BuyPlane(119, nullptr));
    }
    return _run(numPasses, freightMode, planeIds);
}

int64_t _run(int numPasses, int freightMode, const std::vector<int> &planeIds) {
    auto &qPlayer = Sim.Players.Players[0];
    for (auto &plane : qPlayer.Planes) {
        plane.Flugplan.StartCity = plane.Ort = Sim.HomeAirportId;
        plane.Flugplan.UpdateNextFlight();
        plane.Flugplan.UpdateNextStart();
        plane.CheckFlugplaene(qPlayer.PlayerNum);
    }

    for (currentPass = 0; currentPass < numPasses; currentPass++) {
        std::cout << "********** " << currentPass << " **********" << std::endl;

        std::vector<int> cities;
        ReisebueroAuftraege.FillForReisebuero();
        LastMinuteAuftraege.FillForReisebuero();
        assert(AuslandsAuftraege.size() == AuslandsFrachten.size());
        for (int cityNum = 0; cityNum < AuslandsAuftraege.size(); cityNum++) {
            if (Cities.IsInAlbum(cityNum)) {
                GameMechanic::refillFlightJobs(cityNum);
                cities.push_back(cityNum);
            }
        }

        // std::cout << "====================" << std::endl;
        BotPlaner planer(qPlayer, qPlayer.Planes);
        planer.addJobSource(BotPlaner::JobOwner::TravelAgency, {});
        planer.addJobSource(BotPlaner::JobOwner::International, cities);
        planer.addJobSource(BotPlaner::JobOwner::InternationalFreight, cities);

        planer.setMinScoreRatio(kSchedulingMinScoreRatio);
        planer.setMinScoreRatioLastMinute(kSchedulingMinScoreRatioLastMinute);

        auto solutions = planer.planFlights(planeIds, 2);
        planer.applySolution(qPlayer, solutions);
        int nIncorrect = Helper::checkFlightJobs(qPlayer, false, false);
        if (nIncorrect > 0) {
            break;
        }
    }

    int64_t totalGain = 0;
    for (auto planeId : planeIds) {
        totalGain += Helper::calculateScheduleInfo(qPlayer, planeId).gain;
    }
    return totalGain;
}

int64_t benchmark(int numPasses, int freightMode) {
    std::cout << "kNumBestToAdd = " << kNumBestToAdd << std::endl;
    std::cout << "kNumToAdd = " << kNumToAdd << std::endl;
    std::cout << "kNumToRemove = " << kNumToRemove << std::endl;
    std::cout << "kTempStart = " << kTempStart << std::endl;
    std::cout << "kTempStep = " << kTempStep << std::endl;

    std::vector<int64_t> r;
    for (int i = 0; i < numAveraged; i++) {
        r.push_back(run(numPasses, freightMode));
    }
    int64_t mean = std::accumulate(r.begin(), r.end(), 0) / numAveraged;
    int64_t stddev = std::accumulate(r.begin(), r.end(), 0LL, [mean](int64_t acc, int64_t val) { return acc + (val - mean) * (val - mean); });
    stddev = std::round(std::sqrt(stddev / numAveraged));

    std::cout << "averaged benchmark result: " << Insert1000erDots(mean).c_str() << " (stddev = " << Insert1000erDots(stddev).c_str() << ")" << std::endl;
    return mean;
}

int main(int argc, char *argv[]) {

    int numPasses = 1;
    int freightMode = 0;
    if (argc > 1) {
        numPasses = atoi(argv[1]);
    }
    if (argc > 2) {
        freightMode = atoi(argv[2]) > 0;
    }
    if (argc > 3) {
        kNumBestToAdd = atoi(argv[3]);
    }
    if (argc > 4) {
        kNumToAdd = atoi(argv[4]);
    }
    if (argc > 5) {
        kNumToRemove = atoi(argv[5]);
    }
    if (argc > 6) {
        kTempStart = atoi(argv[6]);
    }
    if (argc > 6) {
        kTempStep = atoi(argv[6]);
    }

    // findBestPlaneTypes();
    benchmark(numPasses, freightMode);

    redprintf("END");

    return 0;
}
