#include "BotHelper.h"

#include "BotPlaner.h"
#include "compat_global.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_map>

// Öffnungszeiten:
extern SLONG timeDutyOpen;
extern SLONG timeDutyClose;
extern SLONG timeArabOpen;
extern SLONG timeLastClose;
extern SLONG timeMuseOpen;
extern SLONG timeReisClose;
extern SLONG timeMaklClose;
extern SLONG timeWerbOpen;

// #define PRINT_OVERALL 1

static void setColorForFlightJob(SLONG objectType) {
    std::cout << "\e[1;";
    if (objectType == 1) {
        std::cout << "34";
    } else if (objectType == 2) {
        std::cout << "32";
    } else if (objectType == 4) {
        std::cout << "33";
    } else {
        std::cout << "35";
    }
    std::cout << "m";
}

static void resetColor() { std::cout << "\e[m"; }

namespace Helper {

CString getWeekday(UWORD date) { return StandardTexte.GetS(TOKEN_SCHED, 3010 + (date + Sim.StartWeekday) % 7); }
CString getWeekday(const PlaneTime &time) { return StandardTexte.GetS(TOKEN_SCHED, 3010 + (time.getDate() + Sim.StartWeekday) % 7); }

void printJob(const CAuftrag &qAuftrag) {
    CString strDate = (qAuftrag.Date == qAuftrag.BisDate) ? getWeekday(qAuftrag.Date) : (bprintf("until %s", (LPCTSTR)getWeekday(qAuftrag.BisDate)));
    CString strDist(Einheiten[EINH_KM].bString(Cities.CalcDistance(qAuftrag.VonCity, qAuftrag.NachCity) / 1000));
    CString strPraemie(Insert1000erDots(qAuftrag.Praemie));
    CString strStrafe(Insert1000erDots(qAuftrag.Strafe));
    hprintf("%s -> %s (%u) (%s, %s, %s $ / %s $)", (LPCTSTR)Cities[qAuftrag.VonCity].Name, (LPCTSTR)Cities[qAuftrag.NachCity].Name, qAuftrag.Personen,
            (LPCTSTR)strDist, (LPCTSTR)strDate, (LPCTSTR)strPraemie, (LPCTSTR)strStrafe);
}

void printRoute(const CRoute &qRoute) {
    CString strDist(Einheiten[EINH_KM].bString(Cities.CalcDistance(qRoute.VonCity, qRoute.NachCity) / 1000));
    hprintf("%s -> %s (Bedarf: %ld, Faktor: %f, %s)", (LPCTSTR)Cities[qRoute.VonCity].Name, (LPCTSTR)Cities[qRoute.NachCity].Name, qRoute.Bedarf, qRoute.Faktor,
            (LPCTSTR)strDist);
}

void printFreight(const CFracht &qAuftrag) {
    CString strDate = (qAuftrag.Date == qAuftrag.BisDate) ? getWeekday(qAuftrag.Date) : (bprintf("until %s", (LPCTSTR)getWeekday(qAuftrag.BisDate)));
    CString strDist(Einheiten[EINH_KM].bString(Cities.CalcDistance(qAuftrag.VonCity, qAuftrag.NachCity) / 1000));
    CString strPraemie(Insert1000erDots(qAuftrag.Praemie));
    CString strStrafe(Insert1000erDots(qAuftrag.Strafe));
    hprintf("%s -> %s (%ld tons total, %ld left, %ld open) (%s, %s, %s $ / %s $)", (LPCTSTR)Cities[qAuftrag.VonCity].Name,
            (LPCTSTR)Cities[qAuftrag.NachCity].Name, qAuftrag.Tons, qAuftrag.TonsLeft, qAuftrag.TonsOpen, (LPCTSTR)strDist, (LPCTSTR)strDate,
            (LPCTSTR)strPraemie, (LPCTSTR)strStrafe);
}

std::string getRouteName(const CRoute &qRoute) { return {bprintf("%s -> %s", (LPCTSTR)Cities[qRoute.VonCity].Name, (LPCTSTR)Cities[qRoute.NachCity].Name)}; }

std::string getJobName(const CAuftrag &qAuftrag) {
    return {bprintf("%s -> %s", (LPCTSTR)Cities[qAuftrag.VonCity].Name, (LPCTSTR)Cities[qAuftrag.NachCity].Name)};
}

std::string getFreightName(const CFracht &qAuftrag) {
    return {bprintf("%s -> %s", (LPCTSTR)Cities[qAuftrag.VonCity].Name, (LPCTSTR)Cities[qAuftrag.NachCity].Name)};
}

std::string getPlaneName(const CPlane &qPlane, int mode) {
    if (mode == 1) {
        return {bprintf("%s (%s, Bj: %ld)", (LPCTSTR)qPlane.Name, (LPCTSTR)PlaneTypes[qPlane.TypeId].Name, qPlane.Baujahr)};
    }
    return {bprintf("%s (%s)", (LPCTSTR)qPlane.Name, (LPCTSTR)PlaneTypes[qPlane.TypeId].Name)};
}

void printFPE(const CFlugplanEintrag &qFPE) {
    CString strInfo = bprintf("%s -> %s (%s %ld -> %s %ld)", (LPCTSTR)Cities[qFPE.VonCity].Kuerzel, (LPCTSTR)Cities[qFPE.NachCity].Kuerzel,
                              (LPCTSTR)getWeekday(qFPE.Startdate), qFPE.Startzeit, (LPCTSTR)getWeekday(qFPE.Landedate), qFPE.Landezeit);

    CString strType = "Auto   ";
    if (qFPE.ObjectType == 1) {
        strType = "Route  ";
    } else if (qFPE.ObjectType == 2) {
        strType = "Auftrag";
    } else if (qFPE.ObjectType == 4) {
        strType = "Fracht ";
    }
    CString strDist(Einheiten[EINH_KM].bString(Cities.CalcDistance(qFPE.VonCity, qFPE.NachCity) / 1000));
    hprintf("%s: %s (%u, %s)", (LPCTSTR)strType, (LPCTSTR)strInfo, qFPE.Passagiere, (LPCTSTR)strDist);
}

const CFlugplanEintrag *getLastFlight(const CPlane &qPlane) {
    const auto &qFlightPlan = qPlane.Flugplan.Flug;
    for (SLONG c = qFlightPlan.AnzEntries() - 1; c >= 0; c--) {
        auto *qFPE = &qFlightPlan[c];
        if (qFPE->ObjectType <= 0) {
            continue;
        }
        return qFPE;
    }
    return nullptr;
}

const CFlugplanEintrag *getLastFlightNotAfter(const CPlane &qPlane, PlaneTime ignoreFrom) {
    const auto &qFlightPlan = qPlane.Flugplan.Flug;
    for (SLONG c = qFlightPlan.AnzEntries() - 1; c >= 0; c--) {
        auto *qFPE = &qFlightPlan[c];
        if (qFPE->ObjectType <= 0) {
            continue;
        }
        if (PlaneTime{qFPE->Startdate, qFPE->Startzeit} >= ignoreFrom) {
            continue;
        }
        return qFPE;
    }
    return nullptr;
}

std::pair<PlaneTime, int> getPlaneAvailableTimeLoc(const CPlane &qPlane, std::optional<PlaneTime> ignoreFrom, std::optional<PlaneTime> earliest) {
    std::pair<PlaneTime, int> res{};
    res.second = qPlane.Flugplan.StartCity;

    const CFlugplanEintrag *qFPE = ignoreFrom.has_value() ? getLastFlightNotAfter(qPlane, ignoreFrom.value()) : getLastFlight(qPlane);
    if (qFPE != nullptr) {
        res.first = {qFPE->Landedate, qFPE->Landezeit + kDurationExtra};
        res.second = qFPE->NachCity;
    }
    PlaneTime currentTime{Sim.Date, Sim.GetHour() + kAvailTimeExtra};
    if (earliest.has_value() && earliest.value() > currentTime) {
        currentTime = earliest.value();
    }
    if (currentTime > res.first) {
        res.first = currentTime;
    }
    return res;
}

SLONG checkPlaneSchedule(const PLAYER &qPlayer, SLONG planeId, bool alwaysPrint) {
    if (qPlayer.Planes.IsInAlbum(planeId) == 0) {
        return 0;
    }
    auto &qPlane = qPlayer.Planes[planeId];
    std::unordered_map<SLONG, CString> assignedJobs;
    std::unordered_map<SLONG, FreightInfo> freightTons;
    return _checkPlaneSchedule(qPlayer, qPlane, assignedJobs, freightTons, alwaysPrint);
}

SLONG checkPlaneSchedule(const PLAYER &qPlayer, const CPlane &qPlane, bool alwaysPrint) {
    std::unordered_map<SLONG, CString> assignedJobs;
    std::unordered_map<SLONG, FreightInfo> freightTons;
    return _checkPlaneSchedule(qPlayer, qPlane, assignedJobs, freightTons, alwaysPrint);
}

SLONG _checkPlaneSchedule(const PLAYER &qPlayer, const CPlane &qPlane, std::unordered_map<SLONG, CString> &assignedJobs,
                          std::unordered_map<SLONG, FreightInfo> &freightTons, bool alwaysPrint) {
    SLONG nIncorrect = 0;
    auto nIncorredOld = nIncorrect;

    auto &qFlightPlan = qPlane.Flugplan.Flug;
    for (SLONG d = 0; d < qFlightPlan.AnzEntries(); d++) {
        const auto &qFPE = qFlightPlan[d];
        if (qFPE.ObjectType == 0) {
            continue;
        }

        auto id = qFPE.ObjectId;

        /* check duration */
        PlaneTime startTime{qFPE.Startdate, qFPE.Startzeit};
        auto time = startTime + Cities.CalcFlugdauer(qFPE.VonCity, qFPE.NachCity, qPlane.ptGeschwindigkeit);
        if ((qFPE.Landedate != time.getDate()) || (qFPE.Landezeit != time.getHour())) {
            redprintf("Helper::checkPlaneSchedule(): CFlugplanEintrag has invalid landing time: %s %ld, should be %s %ld", (LPCTSTR)getWeekday(qFPE.Landedate),
                      qFPE.Landezeit, (LPCTSTR)getWeekday(time.getDate()), time.getHour());
            printFPE(qFPE);
        }

        /* check overlap */
        if (d > 0) {
            if (qFPE.Startdate < qFlightPlan[d - 1].Landedate ||
                (qFPE.Startdate == qFlightPlan[d - 1].Landedate && qFPE.Startzeit < qFlightPlan[d - 1].Landezeit)) {
                nIncorrect++;
                redprintf("Helper::checkPlaneSchedule(): Job (%s -> %s) on plane %s overlaps with previous job (%s -> %s)",
                          (LPCTSTR)Cities[qFPE.VonCity].Kuerzel, (LPCTSTR)Cities[qFPE.NachCity].Kuerzel, (LPCTSTR)qPlane.Name,
                          (LPCTSTR)Cities[qFlightPlan[d - 1].VonCity].Kuerzel, (LPCTSTR)Cities[qFlightPlan[d - 1].NachCity].Kuerzel);
            }
            if (qFPE.VonCity != qFlightPlan[d - 1].NachCity) {
                nIncorrect++;
                redprintf(
                    "Helper::checkPlaneSchedule(): Start location of job (%s -> %s) on plane %s does not matching landing location of previous job (%s -> %s)",
                    (LPCTSTR)Cities[qFPE.VonCity].Kuerzel, (LPCTSTR)Cities[qFPE.NachCity].Kuerzel, (LPCTSTR)qPlane.Name,
                    (LPCTSTR)Cities[qFlightPlan[d - 1].VonCity].Kuerzel, (LPCTSTR)Cities[qFlightPlan[d - 1].NachCity].Kuerzel);
            }
        }

        /* check route jobs */
        if (qFPE.ObjectType == 1) {
            auto &qAuftrag = Routen[id];

            if (qFPE.VonCity != qAuftrag.VonCity || qFPE.NachCity != qAuftrag.NachCity) {
                nIncorrect++;
                redprintf("Helper::checkPlaneSchedule(): CFlugplanEintrag does not match CRoute for job (%s)", getRouteName(qAuftrag).c_str());
                printRoute(qAuftrag);
                printFPE(qFPE);
            }

            if (qFPE.Okay != 0) {
                nIncorrect++;
                redprintf("Helper::checkPlaneSchedule(): Route (%s) for plane %s is not scheduled correctly", getRouteName(qAuftrag).c_str(),
                          (LPCTSTR)qPlane.Name);
            }

            if (qPlane.ptReichweite * 1000 < Cities.CalcDistance(qAuftrag.VonCity, qAuftrag.NachCity)) {
                nIncorrect++;
                redprintf("Helper::checkPlaneSchedule(): Route (%s) exceeds range for plane %s", getRouteName(qAuftrag).c_str(), (LPCTSTR)qPlane.Name);
            }
        }

        /* check flight jobs */
        if (qFPE.ObjectType == 2) {
            auto &qAuftrag = qPlayer.Auftraege[id];

            if (assignedJobs.find(id) != assignedJobs.end()) {
                nIncorrect++;
                redprintf("Helper::checkPlaneSchedule(): Job (%s) for plane %s is also scheduled for plane %s", getJobName(qAuftrag).c_str(),
                          (LPCTSTR)qPlane.Name, (LPCTSTR)assignedJobs[id]);
                printJob(qAuftrag);
                printFPE(qFPE);
            }
            assignedJobs[id] = qPlane.Name;

            if (qFPE.VonCity != qAuftrag.VonCity || qFPE.NachCity != qAuftrag.NachCity) {
                nIncorrect++;
                redprintf("Helper::checkPlaneSchedule(): CFlugplanEintrag start/destination does not match CAuftrag for job (%s)",
                          getJobName(qAuftrag).c_str());
                printJob(qAuftrag);
                printFPE(qFPE);
            }

            if ((qFPE.Passagiere + qFPE.PassagiereFC) != qAuftrag.Personen) {
                nIncorrect++;
                redprintf("Helper::checkPlaneSchedule(): CFlugplanEintrag passenger count does not match CAuftrag for job (%s)", getJobName(qAuftrag).c_str());
                printJob(qAuftrag);
                printFPE(qFPE);
            }

            if (qFPE.Okay != 0) {
                nIncorrect++;
                redprintf("Helper::checkPlaneSchedule(): Job (%s) for plane %s is not scheduled correctly", getJobName(qAuftrag).c_str(), (LPCTSTR)qPlane.Name);
            }

            if (qPlane.ptReichweite * 1000 < Cities.CalcDistance(qAuftrag.VonCity, qAuftrag.NachCity)) {
                nIncorrect++;
                redprintf("Helper::checkPlaneSchedule(): Job (%s) exceeds range for plane %s", getJobName(qAuftrag).c_str(), (LPCTSTR)qPlane.Name);
            }

            if (qFPE.Startdate < qAuftrag.Date) {
                nIncorrect++;
                redprintf("Helper::checkPlaneSchedule(): Job (%s) starts too early (%s instead of %s) for plane %s", getJobName(qAuftrag).c_str(),
                          (LPCTSTR)getWeekday(qFPE.Startdate), (LPCTSTR)getWeekday(qAuftrag.Date), (LPCTSTR)qPlane.Name);
            }

            if (qFPE.Startdate > qAuftrag.BisDate) {
                nIncorrect++;
                redprintf("Helper::checkPlaneSchedule(): Job (%s) starts too late (%s instead of %s) for plane %s", getJobName(qAuftrag).c_str(),
                          (LPCTSTR)getWeekday(qFPE.Startdate), (LPCTSTR)getWeekday(qAuftrag.BisDate), (LPCTSTR)qPlane.Name);
            }

            if (qPlane.ptPassagiere < SLONG(qAuftrag.Personen)) {
                nIncorrect++;
                redprintf("Helper::checkPlaneSchedule(): Job (%s) exceeds number of seats for plane %s", getJobName(qAuftrag).c_str(), (LPCTSTR)qPlane.Name);
            }
        }

        /* check freight jobs */
        if (qFPE.ObjectType == 4) {
            auto &qAuftrag = qPlayer.Frachten[id];

            if (!qFPE.FlightBooked) {
                if (freightTons.find(id) == freightTons.end()) {
                    freightTons[id].tonsOpen = qAuftrag.TonsLeft;
                }
                SLONG tons = qPlane.ptPassagiere / 10;
                freightTons[id].planeNames.push_back(qPlane.Name);
                freightTons[id].tonsPerPlane.push_back(tons);
                freightTons[id].tonsOpen -= tons;
                freightTons[id].smallestDecrement = std::min(freightTons[id].smallestDecrement, tons);
            }

            if (qFPE.VonCity != qAuftrag.VonCity || qFPE.NachCity != qAuftrag.NachCity) {
                nIncorrect++;
                redprintf("Helper::checkPlaneSchedule(): CFlugplanEintrag does not match CFracht for job (%s)", getFreightName(qAuftrag).c_str());
                printFreight(qAuftrag);
                printFPE(qFPE);
            }

            if (qFPE.Okay != 0) {
                nIncorrect++;
                redprintf("Helper::checkPlaneSchedule(): Freight job (%s) for plane %s is not scheduled correctly", getFreightName(qAuftrag).c_str(),
                          (LPCTSTR)qPlane.Name);
            }

            if (qPlane.ptReichweite * 1000 < Cities.CalcDistance(qAuftrag.VonCity, qAuftrag.NachCity)) {
                nIncorrect++;
                redprintf("Helper::checkPlaneSchedule(): Freight job (%s) exceeds range for plane %s", getFreightName(qAuftrag).c_str(), (LPCTSTR)qPlane.Name);
            }

            if (qFPE.Startdate < qAuftrag.Date) {
                nIncorrect++;
                redprintf("Helper::checkPlaneSchedule(): Freight job (%s) starts too early (%s instead of %s) for plane %s", getFreightName(qAuftrag).c_str(),
                          (LPCTSTR)getWeekday(qFPE.Startdate), (LPCTSTR)getWeekday(qAuftrag.Date), (LPCTSTR)qPlane.Name);
            }

            if (qFPE.Startdate > qAuftrag.BisDate) {
                nIncorrect++;
                redprintf("Helper::checkPlaneSchedule(): Freight job (%s) starts too late (%s instead of %s) for plane %s", getFreightName(qAuftrag).c_str(),
                          (LPCTSTR)getWeekday(qFPE.Startdate), (LPCTSTR)getWeekday(qAuftrag.BisDate), (LPCTSTR)qPlane.Name);
            }
        }
    }

    if ((nIncorrect > nIncorredOld) || alwaysPrint) {
        printFlightJobs(qPlayer, qPlane);
    }
    return nIncorrect;
}

void printFlightJobs(const PLAYER &qPlayer, SLONG planeId) {
    if (qPlayer.Planes.IsInAlbum(planeId) == 0) {
        return;
    }
    auto &qPlane = qPlayer.Planes[planeId];
    printFlightJobs(qPlayer, qPlane);
}

void printFlightJobs(const PLAYER &qPlayer, const CPlane &qPlane) {
    auto &qFlightPlan = qPlane.Flugplan.Flug;

    hprintf("Helper::printFlightJobs(): Schedule for plane %s:", Helper::getPlaneName(qPlane).c_str());

    /* print job list */
    for (SLONG d = 0; d < qFlightPlan.AnzEntries(); d++) {
        auto objectType = qFlightPlan[d].ObjectType;
        if (objectType == 0) {
            continue;
        }
        setColorForFlightJob(objectType);
        printf("%c> ", 'A' + (d % 26));
        printFPE(qFlightPlan[d]);
        if (objectType == 1) {
            printf("%c>          ", 'A' + (d % 26));
            printRoute(Routen[qFlightPlan[d].ObjectId]);
        } else if (objectType == 2) {
            printf("%c>          ", 'A' + (d % 26));
            printJob(qPlayer.Auftraege[qFlightPlan[d].ObjectId]);
        } else if (objectType == 4) {
            printf("%c>          ", 'A' + (d % 26));
            printFreight(qPlayer.Frachten[qFlightPlan[d].ObjectId]);
        }
        resetColor();
    }

    /* render schedule to ASCII */
    SLONG idx = 0;
    for (SLONG day = Sim.Date; day < Sim.Date + 7; day++) {
        std::cout << std::setw(10) << getWeekday(day) << std::setw(0);
        for (SLONG i = 0; i < 24; i++) {
            while ((idx < qFlightPlan.AnzEntries()) && (qFlightPlan[idx].ObjectType == 0 || qFlightPlan[idx].Landedate < day ||
                                                        (qFlightPlan[idx].Landedate == day && qFlightPlan[idx].Landezeit < i))) {
                idx++;
            }

            if (idx == qFlightPlan.AnzEntries()) {
                std::cout << ".";
                continue;
            }

            if (qFlightPlan[idx].Startdate < day || (qFlightPlan[idx].Startdate == day && qFlightPlan[idx].Startzeit <= i)) {
                setColorForFlightJob(qFlightPlan[idx].ObjectType);
                std::cout << static_cast<char>('A' + (idx % 26));
                resetColor();
            } else {
                std::cout << ".";
            }
        }
        std::cout << std::endl;
    }
}

ScheduleInfo calculateScheduleInfo(const PLAYER &qPlayer, SLONG planeId) {
    if (qPlayer.Planes.IsInAlbum(planeId) == 0) {
        return {};
    }

    ScheduleInfo info;
    info.numPlanes = 1;

    bool first = true;
    auto &qPlane = qPlayer.Planes[planeId];
    auto &qFlightPlan = qPlane.Flugplan.Flug;
    std::unordered_map<SLONG, bool> jobs; /* to only count freight jobs once */
    for (SLONG c = 0; c < qFlightPlan.AnzEntries(); c++) {
        const auto &qFPE = qFlightPlan[c];
        if (qFPE.ObjectType <= 0) {
            continue;
        }

        if (first) {
            first = false;
            info.scheduleStart = {qFPE.Startdate, qFPE.Startzeit};
        }
        info.scheduleEnd = {qFPE.Landedate, qFPE.Landezeit + 1};

        if (qFPE.ObjectType != 3) {
            info.hoursFlights += 24 * (qFPE.Landedate - qFPE.Startdate) + (qFPE.Landezeit + 1 - qFPE.Startzeit);
            info.keroseneFlights = CalculateFlightKerosin(qFPE.VonCity, qFPE.NachCity, qPlane.ptVerbrauch, qPlane.ptGeschwindigkeit);
        } else {
            info.hoursAutoFlights += 24 * (qFPE.Landedate - qFPE.Startdate) + (qFPE.Landezeit + 1 - qFPE.Startzeit);
            info.keroseneAutoFlights = CalculateFlightKerosin(qFPE.VonCity, qFPE.NachCity, qPlane.ptVerbrauch, qPlane.ptGeschwindigkeit);
        }

        info.miles += Cities.CalcDistance(qFPE.VonCity, qFPE.NachCity) / 1609;

        if (qFPE.ObjectType == 4) {
            /* give premium only once */
            if (jobs.find(qFPE.ObjectId) == jobs.end()) {
                jobs[qFPE.ObjectId] = true;

                info.gain += qFlightPlan[c].GetEinnahmen(qPlayer.PlayerNum, qPlane);
                info.freightJobs += 1;

                const auto &job = qPlayer.Frachten[qFPE.ObjectId];
                assert(job.jobType >= 0 && job.jobType < info.jobTypes.size());
                assert(job.jobSizeType >= 0 && job.jobSizeType < info.jobSizeTypes.size());
                info.jobTypes[job.jobType]++;
                info.jobSizeTypes[job.jobSizeType]++;
            }
        } else if (qFPE.ObjectType == 2) {
            info.gain += qFlightPlan[c].GetEinnahmen(qPlayer.PlayerNum, qPlane);
            info.jobs += 1;

            const auto &job = qPlayer.Auftraege[qFPE.ObjectId];
            assert(job.jobType >= 0 && job.jobType < info.jobTypes.size());
            assert(job.jobSizeType >= 0 && job.jobSizeType < info.jobSizeTypes.size());
            info.jobTypes[job.jobType]++;
            info.jobSizeTypes[job.jobSizeType]++;

            if (job.bUhrigFlight != 0) {
                info.uhrigFlights += 1;
            }
        } else {
            info.gain += qFlightPlan[c].GetEinnahmen(qPlayer.PlayerNum, qPlane);
        }
        info.gain -= CalculateFlightCostNoTank(qFPE.VonCity, qFPE.NachCity, qPlane.ptVerbrauch, qPlane.ptGeschwindigkeit);

        if (qFPE.ObjectType == 1 || qFPE.ObjectType == 2) {
            info.passengers += qFPE.Passagiere + qFPE.PassagiereFC;
        } else if (qFPE.ObjectType == 4) {
            info.tons += qPlane.ptPassagiere / 10;
        }
    }
    return info;
}

SLONG checkFlightJobs(const PLAYER &qPlayer, bool alwaysPrint, bool verboseInfo) {
    SLONG nIncorrect = 0;
    SLONG nPlanes = 0;
    std::unordered_map<SLONG, CString> assignedJobs;
    std::unordered_map<SLONG, FreightInfo> freightTons;
    Helper::ScheduleInfo overallInfo;
    for (SLONG c = 0; c < qPlayer.Planes.AnzEntries(); c++) {
        if (qPlayer.Planes.IsInAlbum(c) == 0) {
            continue;
        }
        nIncorrect += _checkPlaneSchedule(qPlayer, qPlayer.Planes[c], assignedJobs, freightTons, alwaysPrint);
        overallInfo += calculateScheduleInfo(qPlayer, c);
        nPlanes++;
    }

    /* check whether all freight jobs are fully scheduled */
    for (const auto &iter : freightTons) {
        const auto &qAuftrag = qPlayer.Frachten[iter.first];
        const auto &info = iter.second;
        std::stringstream ss;
        for (SLONG i = 0; i < info.planeNames.size(); i++) {
            ss << info.tonsPerPlane[i] << "t on " << info.planeNames[i].c_str();
            if (i < info.planeNames.size() - 1) {

                ss << ", ";
            }
        }
        if (info.tonsOpen > 0) {
            redprintf("Helper::checkPlaneSchedule(): There are still %d tons open for job %s (%s)", info.tonsOpen, getFreightName(qAuftrag).c_str(),
                      ss.str().c_str());
        }
        if ((-info.tonsOpen) >= info.smallestDecrement) {
            redprintf("Helper::checkPlaneSchedule(): At least one instance of job %s (%s) can be removed (overscheduled: %ld, smallest: %ld)",
                      getFreightName(qAuftrag).c_str(), ss.str().c_str(), (-info.tonsOpen), info.smallestDecrement);
        }
    }

    hprintf("Helper::checkFlightJobs(): %s: Found %ld problems for %ld planes.", (LPCTSTR)qPlayer.AirlineX, nIncorrect, nPlanes);
    if (verboseInfo) {
        overallInfo.printDetails();
    } else {
        overallInfo.printGain();
    }
    return nIncorrect;
}

void printAllSchedules(bool infoOnly) {
    for (SLONG d = 0; d < 4; d++) {
        const auto &qPlayer = Sim.Players.Players[d];
        hprintf("========== %s ==========", (LPCTSTR)qPlayer.AirlineX);
        checkFlightJobs(qPlayer, !infoOnly, !infoOnly);
        hprintf("==============================");
    }
}

bool checkRoomOpen(SLONG roomId) {
    SLONG time = Sim.Time;
    switch (roomId) {
    case ACTION_VISITDUTYFREE:
        return (time >= timeDutyOpen && ((Sim.Weekday != 5 && Sim.Weekday != 6) || time <= timeDutyClose - 60000));
    case ACTION_BUY_KEROSIN:
        [[fallthrough]];
    case ACTION_BUY_KEROSIN_TANKS:
        [[fallthrough]];
    case ACTION_VISITARAB:
        return (Sim.Difficulty >= DIFF_EASY || Sim.Difficulty == DIFF_FREEGAME) && (time >= timeArabOpen && Sim.Weekday != 6);
    case ACTION_CHECKAGENT1:
        return (time <= timeLastClose - 60000 && Sim.Weekday != 5 && GlobalUse(USE_TRAVELHOLDING));
    case ACTION_BUYUSEDPLANE:
        [[fallthrough]];
    case ACTION_VISITMUSEUM:
        return (time >= timeMuseOpen && Sim.Weekday != 5 && Sim.Weekday != 6);
    case ACTION_CHECKAGENT2:
        return (time <= timeReisClose - 60000 && GlobalUse(USE_TRAVELHOLDING));
    case ACTION_BUYNEWPLANE:
        return (time <= timeMaklClose - 60000);
    case ACTION_WERBUNG:
        [[fallthrough]];
    case ACTION_WERBUNG_ROUTES:
        [[fallthrough]];
    case ACTION_VISITADS:
        return (Sim.Difficulty >= DIFF_NORMAL || Sim.Difficulty == DIFF_FREEGAME) && (time >= timeWerbOpen && Sim.Weekday != 5 && Sim.Weekday != 6);
    case ACTION_VISITSECURITY:
        [[fallthrough]];
    case ACTION_VISITSECURITY2:
        return (Sim.nSecOutDays <= 0);
    case ACTION_VISITROUTEBOX:
        [[fallthrough]];
    case ACTION_VISITROUTEBOX2:
        return (Sim.Difficulty >= DIFF_EASY || Sim.Difficulty == DIFF_FREEGAME);
    case ACTION_CHECKAGENT3:
        return (Sim.Difficulty >= DIFF_ADDON01 || Sim.Difficulty == DIFF_FREEGAME);
    case ACTION_VISITNASA:
        return (Sim.Difficulty == DIFF_FINAL || Sim.Difficulty == DIFF_ADDON10);
    default:
        return true;
    }
    return true;
}

SLONG getRoomFromAction(SLONG PlayerNum, SLONG actionId) {
    switch (actionId) {
    case ACTION_WAIT:
        return -1;
    case ACTION_RAISEMONEY:
        [[fallthrough]];
    case ACTION_DROPMONEY:
        [[fallthrough]];
    case ACTION_VISITBANK:
        [[fallthrough]];
    case ACTION_EMITSHARES:
        [[fallthrough]];
    case ACTION_SET_DIVIDEND:
        [[fallthrough]];
    case ACTION_BUYSHARES:
        [[fallthrough]];
    case ACTION_SELLSHARES:
        return ROOM_BANK;
    case ACTION_CHECKAGENT1:
        return ROOM_LAST_MINUTE;
    case ACTION_CHECKAGENT2:
        return ROOM_REISEBUERO;
    case ACTION_CHECKAGENT3:
        return ROOM_FRACHT;
    case ACTION_STARTDAY:
        [[fallthrough]];
    case ACTION_BUERO:
        [[fallthrough]];
    case ACTION_UPGRADE_PLANES:
        [[fallthrough]];
    case ACTION_CALL_INTERNATIONAL:
        return UBYTE(PlayerNum * 10 + 10);
    case ACTION_PERSONAL:
        return UBYTE(PlayerNum * 10 + 11);
    case ACTION_VISITARAB:
        [[fallthrough]];
    case ACTION_BUY_KEROSIN:
        [[fallthrough]];
    case ACTION_BUY_KEROSIN_TANKS:
        return ROOM_ARAB_AIR;
    case ACTION_VISITKIOSK:
        return ROOM_KIOSK;
    case ACTION_VISITMECH:
        return ROOM_WERKSTATT;
    case ACTION_VISITMUSEUM:
        return ROOM_MUSEUM;
    case ACTION_VISITDUTYFREE:
        return ROOM_SHOP1;
    case ACTION_VISITAUFSICHT:
        [[fallthrough]];
    case ACTION_EXPANDAIRPORT:
        return ROOM_AUFSICHT;
    case ACTION_VISITNASA:
        return ROOM_NASA;
    case ACTION_VISITTELESCOPE:
        if (Sim.Difficulty == DIFF_FINAL) {
            return ROOM_INSEL;
        } else if (Sim.Difficulty == DIFF_ADDON10) {
            return ROOM_WELTALL;
        } else {
            return ROOM_RUSHMORE;
        }
    case ACTION_VISITMAKLER:
        return ROOM_MAKLER;
    case ACTION_VISITRICK:
        return ROOM_RICKS;
    case ACTION_VISITROUTEBOX:
        [[fallthrough]];
    case ACTION_VISITROUTEBOX2:
        return ROOM_ROUTEBOX;
    case ACTION_VISITSECURITY:
        return ROOM_SECURITY;
    case ACTION_VISITDESIGNER:
        return ROOM_DESIGNER;
    case ACTION_VISITSECURITY2:
        return ROOM_SECURITY;
    case ACTION_SABOTAGE:
        return ROOM_SABOTAGE;
    case ACTION_BUYUSEDPLANE:
        return ROOM_MUSEUM;
    case ACTION_BUYNEWPLANE:
        return ROOM_MAKLER;
    case ACTION_WERBUNG:
        [[fallthrough]];
    case ACTION_WERBUNG_ROUTES:
        [[fallthrough]];
    case ACTION_VISITADS:
        return ROOM_WERBUNG;
    case ACTION_CALL_INTER_HANDY:
        [[fallthrough]];
    case ACTION_STARTDAY_LAPTOP:
        return 0; /* no need to walk anywhere */
    default:
        DebugBreak();
    }
    return -1;
}

const char *getItemName(SLONG item) {
    switch (item) {
    case ITEM_LAPTOP:
        return "Laptop";
    case ITEM_HANDY:
        return "Handy";
    case ITEM_NOTEBOOK:
        return "Filofax";
    case ITEM_MG:
        return "MG";
    case ITEM_BIER:
        return "Beer";
    case ITEM_ZIGARRE:
        return "Cigarette";
    case ITEM_OEL:
        return "Oilcan";
    case ITEM_POSTKARTE:
        return "Postcard";
    case ITEM_TABLETTEN:
        return "Pills";
    case ITEM_SPINNE:
        return "Tarantula";
    case ITEM_DART:
        return "Dart";
    case ITEM_DISKETTE:
        return "Floppy disk";
    case ITEM_BH:
        return "BH";
    case ITEM_HUFEISEN:
        return "Horseshoe";
    case ITEM_PRALINEN:
        return "Pralines";
    case ITEM_PRALINEN_A:
        return "Pralines (spiked)";
    case ITEM_PAPERCLIP:
        return "Paperclip";
    case ITEM_GLUE:
        return "Glue";
    case ITEM_GLOVE:
        return "Gloves";
    case ITEM_REDBULL:
        return "Redbull";
    case ITEM_STINKBOMBE:
        return "Stinkbomb";
    case ITEM_GLKOHLE:
        return "Ember";
    case ITEM_KOHLE:
        return "Ember (cold)";
    case ITEM_ZANGE:
        return "Pliers";
    case ITEM_PARFUEM:
        return "Perfume";
    case ITEM_XPARFUEM:
        return "Stinking perfume";
    default:
        redprintf("BotHelper.cpp: Default case should not be reached.");
        DebugBreak();
        return "INVALID";
    }
    return "INVALID";
}

} // namespace Helper
