#include "BotHelper.h"

#include "Proto.h"
#include "TeakLibW.h"
#include "class.h"
#include "defines.h"
#include "global.h"
#include "helper.h"

#include <SDL_log.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

template <class... Types> void AT_Error(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_ERROR, "Bot", args...); }
template <class... Types> void AT_Warn(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_WARN, "Bot", args...); }
template <class... Types> void AT_Info(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_INFO, "Bot", args...); }
template <class... Types> void AT_Log(Types... args) { AT_Log_I("Bot", args...); }

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

TEAKFILE &operator<<(TEAKFILE &File, const PlaneTime &planeTime) {
    File << static_cast<SLONG>(planeTime.getDate());
    File << static_cast<SLONG>(planeTime.getHour());
    return (File);
}

TEAKFILE &operator>>(TEAKFILE &File, PlaneTime &planeTime) {
    SLONG date{};
    SLONG time{};
    File >> date;
    File >> time;
    planeTime = {date, time};

    return (File);
}

inline void setColorForFlightJob(const PLAYER &qPlayer, const CFlugplanEintrag &qFPE) {
    std::cout << "\033[1;";
    if (qFPE.ObjectType == 1) {
        std::cout << "34";
    } else if (qFPE.ObjectType == 2) {
        if (qPlayer.Auftraege[qFPE.ObjectId].bUhrigFlight) {
            std::cout << "31";
        } else {
            std::cout << "32";
        }
    } else if (qFPE.ObjectType == 4) {
        std::cout << "33";
    } else {
        std::cout << "35";
    }
    std::cout << "m";
}

inline void resetColor() { std::cout << "\033[m"; }

namespace Helper {
void ScheduleInfo::printGain() const {
    printf("Schedule (%s %d - %s %d) with %d planes gains %s $.\n", getWeekday(scheduleStart).c_str(), scheduleStart.getHour(), getWeekday(scheduleEnd).c_str(),
           scheduleEnd.getHour(), numPlanes, Insert1000erDots(gain).c_str());
}

void ScheduleInfo::printDetails() const {
    printf("vvvvvvvvvvvvvvvvvvvv\n");
    printGain();

    if (uhrigFlights > 0) {
        printf("Flying %d jobs (%d from Uhrig, %d passengers), %d freight jobs (%d tons) and %s miles.\n", jobs, uhrigFlights, passengers, freightJobs, tons,
               Insert1000erDots(miles).c_str());
    } else {
        printf("Flying %d jobs (%d passengers), %d freight jobs (%d tons) and %s miles.\n", jobs, passengers, freightJobs, tons,
               Insert1000erDots(miles).c_str());
    }
    printf("%.1f %% of plane schedule are regular flights, %.1f %% are automatic flights (%.1f %% useful kerosene).\n", getRatioFlights(),
           getRatioAutoFlights(), getKeroseneRatio());

    std::array<CString, 5> typeStr{"normal", "later", "highriskreward", "scam", "no fine"};
    std::array<CString, 6> sizeStr{"VIP", "S", "M", "L", "XL", "XXL"};
    printf("Job types: ");
    for (SLONG i = 0; i < jobTypes.size(); i++) {
        printf("%.0f %% %s", 100.0 * jobTypes[i] / (jobs + freightJobs), typeStr[i].c_str());
        if (i < jobTypes.size() - 1) {
            printf(", ");
        }
    }
    printf("\nJob sizes: ");
    for (SLONG i = 0; i < jobSizeTypes.size(); i++) {
        printf("%.0f %% %s", 100.0 * jobSizeTypes[i] / (jobs + freightJobs), sizeStr[i].c_str());
        if (i < jobSizeTypes.size() - 1) {
            printf(", ");
        }
    }
    printf("\n");
    printf("^^^^^^^^^^^^^^^^^^^^\n");
}

CString getWeekday(UWORD date) { return StandardTexte.GetS(TOKEN_SCHED, 3010 + (date + Sim.StartWeekday) % 7); }
CString getWeekday(const PlaneTime &time) { return StandardTexte.GetS(TOKEN_SCHED, 3010 + (time.getDate() + Sim.StartWeekday) % 7); }

void printJob(const CAuftrag &qAuftrag) {
    CString strDate = (qAuftrag.Date == qAuftrag.BisDate) ? getWeekday(qAuftrag.Date) : (bprintf("until %s", getWeekday(qAuftrag.BisDate).c_str()));
    CString strDist(Einheiten[EINH_KM].bString(Cities.CalcDistance(qAuftrag.VonCity, qAuftrag.NachCity) / 1000));
    CString strPraemie(Insert1000erDots(qAuftrag.Praemie));
    CString strStrafe(Insert1000erDots(qAuftrag.Strafe));
    printf("%s -> %s (%u) (%s, %s, %s $ / %s $)\n", Cities[qAuftrag.VonCity].Name.c_str(), Cities[qAuftrag.NachCity].Name.c_str(), qAuftrag.Personen,
           strDist.c_str(), strDate.c_str(), strPraemie.c_str(), strStrafe.c_str());
}

void printRoute(const CRoute &qRoute) {
    CString strDist(Einheiten[EINH_KM].bString(Cities.CalcDistance(qRoute.VonCity, qRoute.NachCity) / 1000));
    printf("%s -> %s (Bedarf: %d, Faktor: %f, %s)\n", Cities[qRoute.VonCity].Name.c_str(), Cities[qRoute.NachCity].Name.c_str(), qRoute.Bedarf, qRoute.Faktor,
           strDist.c_str());
}

void printFreight(const CFracht &qAuftrag) {
    CString strDate = (qAuftrag.Date == qAuftrag.BisDate) ? getWeekday(qAuftrag.Date) : (bprintf("until %s", getWeekday(qAuftrag.BisDate).c_str()));
    CString strDist(Einheiten[EINH_KM].bString(Cities.CalcDistance(qAuftrag.VonCity, qAuftrag.NachCity) / 1000));
    CString strPraemie(Insert1000erDots(qAuftrag.Praemie));
    CString strStrafe(Insert1000erDots(qAuftrag.Strafe));
    printf("%s -> %s (%d tons total, %d left, %d open) (%s, %s, %s $ / %s $)\n", Cities[qAuftrag.VonCity].Name.c_str(), Cities[qAuftrag.NachCity].Name.c_str(),
           qAuftrag.Tons, qAuftrag.TonsLeft, qAuftrag.TonsOpen, strDist.c_str(), strDate.c_str(), strPraemie.c_str(), strStrafe.c_str());
}

std::string getRouteName(const CRoute &qRoute) { return {bprintf("%s -> %s", Cities[qRoute.VonCity].Name.c_str(), Cities[qRoute.NachCity].Name.c_str())}; }

std::string getJobName(const CAuftrag &qAuftrag) {
    return {bprintf("%s -> %s", Cities[qAuftrag.VonCity].Name.c_str(), Cities[qAuftrag.NachCity].Name.c_str())};
}

std::string getFreightName(const CFracht &qAuftrag) {
    return {bprintf("%s -> %s", Cities[qAuftrag.VonCity].Name.c_str(), Cities[qAuftrag.NachCity].Name.c_str())};
}

std::string getPlaneName(const CPlane &qPlane, int mode) {
    CString typeName = (qPlane.TypeId == -1) ? "Designer" : PlaneTypes[qPlane.TypeId].Name;
    if (mode == 1) {
        return {bprintf("%s (%s, Bj: %d)", qPlane.Name.c_str(), typeName.c_str(), qPlane.Baujahr)};
    }
    return {bprintf("%s (%s)", qPlane.Name.c_str(), typeName.c_str())};
}

void printFPE(const CFlugplanEintrag &qFPE) {
    CString strInfo = bprintf("%s -> %s (%s %d -> %s %d)", Cities[qFPE.VonCity].Kuerzel.c_str(), Cities[qFPE.NachCity].Kuerzel.c_str(),
                              getWeekday(qFPE.Startdate).c_str(), qFPE.Startzeit, getWeekday(qFPE.Landedate).c_str(), qFPE.Landezeit);

    CString strType = "Auto   ";
    if (qFPE.ObjectType == 1) {
        strType = "Route  ";
    } else if (qFPE.ObjectType == 2) {
        strType = "Auftrag";
    } else if (qFPE.ObjectType == 4) {
        strType = "Fracht ";
    }
    CString strDist(Einheiten[EINH_KM].bString(Cities.CalcDistance(qFPE.VonCity, qFPE.NachCity) / 1000));
    printf("%s: %s (%u, %s)\n", strType.c_str(), strInfo.c_str(), qFPE.Passagiere, strDist.c_str());
}

const CFlugplanEintrag *getLastFlight(const CPlane &qPlane) {
    const auto &qFlightPlan = qPlane.Flugplan.Flug;
    for (SLONG c = qFlightPlan.AnzEntries() - 1; c >= 0; c--) {
        const auto *qFPE = &qFlightPlan[c];
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
        const auto *qFPE = &qFlightPlan[c];
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
        res.first = {qFPE->Landedate, qFPE->Landezeit + 1};
        res.second = qFPE->NachCity;
    }
    PlaneTime currentTime{Sim.Date, Sim.GetHour() + 1};
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
    const auto &qPlane = qPlayer.Planes[planeId];
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

    const auto &qFlightPlan = qPlane.Flugplan.Flug;
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
            AT_Error("Helper::checkPlaneSchedule(): CFlugplanEintrag has invalid landing time: %s %d, should be %s %d", getWeekday(qFPE.Landedate).c_str(),
                     qFPE.Landezeit, getWeekday(time.getDate()).c_str(), time.getHour());
            printFPE(qFPE);
        }

        /* check overlap */
        if (d > 0) {
            if (qFPE.Startdate < qFlightPlan[d - 1].Landedate ||
                (qFPE.Startdate == qFlightPlan[d - 1].Landedate && qFPE.Startzeit < qFlightPlan[d - 1].Landezeit)) {
                nIncorrect++;
                AT_Error("Helper::checkPlaneSchedule(): Job (%s -> %s) on plane %s overlaps with previous job (%s -> %s)", Cities[qFPE.VonCity].Kuerzel.c_str(),
                         Cities[qFPE.NachCity].Kuerzel.c_str(), qPlane.Name.c_str(), Cities[qFlightPlan[d - 1].VonCity].Kuerzel.c_str(),
                         Cities[qFlightPlan[d - 1].NachCity].Kuerzel.c_str());
            }
            if (qFPE.VonCity != qFlightPlan[d - 1].NachCity) {
                nIncorrect++;
                AT_Error(
                    "Helper::checkPlaneSchedule(): Start location of job (%s -> %s) on plane %s does not matching landing location of previous job (%s -> %s)",
                    Cities[qFPE.VonCity].Kuerzel.c_str(), Cities[qFPE.NachCity].Kuerzel.c_str(), qPlane.Name.c_str(),
                    Cities[qFlightPlan[d - 1].VonCity].Kuerzel.c_str(), Cities[qFlightPlan[d - 1].NachCity].Kuerzel.c_str());
            }
        }

        /* check route jobs */
        if (qFPE.ObjectType == 1) {
            const auto &qAuftrag = Routen[id];

            if (qFPE.VonCity != qAuftrag.VonCity || qFPE.NachCity != qAuftrag.NachCity) {
                nIncorrect++;
                AT_Error("Helper::checkPlaneSchedule(): CFlugplanEintrag does not match CRoute for job (%s)", getRouteName(qAuftrag).c_str());
                printRoute(qAuftrag);
                printFPE(qFPE);
            }

            if (qFPE.Okay != 0) {
                nIncorrect++;
                AT_Warn("Helper::checkPlaneSchedule(): Route (%s) for plane %s is not scheduled correctly", getRouteName(qAuftrag).c_str(),
                        qPlane.Name.c_str());
            }

            if (qPlane.ptReichweite * 1000 < Cities.CalcDistance(qAuftrag.VonCity, qAuftrag.NachCity)) {
                nIncorrect++;
                AT_Error("Helper::checkPlaneSchedule(): Route (%s) exceeds range for plane %s", getRouteName(qAuftrag).c_str(), qPlane.Name.c_str());
            }
        }

        /* check flight jobs */
        if (qFPE.ObjectType == 2) {
            const auto &qAuftrag = qPlayer.Auftraege[id];

            if (assignedJobs.find(id) != assignedJobs.end()) {
                nIncorrect++;
                AT_Error("Helper::checkPlaneSchedule(): Job (%s) for plane %s is also scheduled for plane %s", getJobName(qAuftrag).c_str(),
                         qPlane.Name.c_str(), assignedJobs[id].c_str());
                printJob(qAuftrag);
                printFPE(qFPE);
            }
            assignedJobs[id] = qPlane.Name;

            if (qFPE.VonCity != qAuftrag.VonCity || qFPE.NachCity != qAuftrag.NachCity) {
                nIncorrect++;
                AT_Error("Helper::checkPlaneSchedule(): CFlugplanEintrag start/destination does not match CAuftrag for job (%s)", getJobName(qAuftrag).c_str());
                printJob(qAuftrag);
                printFPE(qFPE);
            }

            if ((qFPE.Passagiere + qFPE.PassagiereFC) != qAuftrag.Personen) {
                nIncorrect++;
                AT_Error("Helper::checkPlaneSchedule(): CFlugplanEintrag passenger count does not match CAuftrag for job (%s)", getJobName(qAuftrag).c_str());
                printJob(qAuftrag);
                printFPE(qFPE);
            }

            if (qFPE.Okay != 0) {
                nIncorrect++;
                AT_Warn("Helper::checkPlaneSchedule(): Job (%s) for plane %s is not scheduled correctly", getJobName(qAuftrag).c_str(), qPlane.Name.c_str());
            }

            if (qPlane.ptReichweite * 1000 < Cities.CalcDistance(qAuftrag.VonCity, qAuftrag.NachCity)) {
                nIncorrect++;
                AT_Error("Helper::checkPlaneSchedule(): Job (%s) exceeds range for plane %s", getJobName(qAuftrag).c_str(), qPlane.Name.c_str());
            }

            if (qFPE.Startdate < qAuftrag.Date) {
                nIncorrect++;
                AT_Error("Helper::checkPlaneSchedule(): Job (%s) starts too early (%s instead of %s) for plane %s", getJobName(qAuftrag).c_str(),
                         getWeekday(qFPE.Startdate).c_str(), getWeekday(qAuftrag.Date).c_str(), qPlane.Name.c_str());
            }

            if (qFPE.Startdate > qAuftrag.BisDate) {
                nIncorrect++;
                AT_Warn("Helper::checkPlaneSchedule(): Job (%s) starts too late (%s instead of %s) for plane %s", getJobName(qAuftrag).c_str(),
                        getWeekday(qFPE.Startdate).c_str(), getWeekday(qAuftrag.BisDate).c_str(), qPlane.Name.c_str());
            }

            if (qPlane.ptPassagiere < static_cast<SLONG>(qAuftrag.Personen)) {
                nIncorrect++;
                AT_Error("Helper::checkPlaneSchedule(): Job (%s) exceeds number of seats for plane %s", getJobName(qAuftrag).c_str(), qPlane.Name.c_str());
            }
        }

        /* check freight jobs */
        if (qFPE.ObjectType == 4) {
            const auto &qAuftrag = qPlayer.Frachten[id];

            if (!qFPE.FlightBooked && qFPE.Okay == 0) {
                if (freightTons.find(id) == freightTons.end()) {
                    freightTons[id].tonsOpen = qAuftrag.TonsLeft;
                }
                SLONG tons = qPlane.ptPassagiere / 10;
                freightTons[id].planeNames.push_back(qPlane.Name);
                freightTons[id].tonsPerPlane.push_back(tons);
                freightTons[id].FPEs.push_back(qFPE);
                freightTons[id].tonsOpen -= tons;
                freightTons[id].smallestDecrement = std::min(freightTons[id].smallestDecrement, tons);
            }

            if (qFPE.VonCity != qAuftrag.VonCity || qFPE.NachCity != qAuftrag.NachCity) {
                nIncorrect++;
                AT_Error("Helper::checkPlaneSchedule(): CFlugplanEintrag does not match CFracht for job (%s)", getFreightName(qAuftrag).c_str());
                printFreight(qAuftrag);
                printFPE(qFPE);
            }

            if (qFPE.Okay != 0) {
                nIncorrect++;
                AT_Warn("Helper::checkPlaneSchedule(): Freight job (%s) for plane %s is not scheduled correctly", getFreightName(qAuftrag).c_str(),
                        qPlane.Name.c_str());
            }

            if (qPlane.ptReichweite * 1000 < Cities.CalcDistance(qAuftrag.VonCity, qAuftrag.NachCity)) {
                nIncorrect++;
                AT_Error("Helper::checkPlaneSchedule(): Freight job (%s) exceeds range for plane %s", getFreightName(qAuftrag).c_str(), qPlane.Name.c_str());
            }

            if (qFPE.Startdate < qAuftrag.Date) {
                nIncorrect++;
                AT_Error("Helper::checkPlaneSchedule(): Freight job (%s) starts too early (%s instead of %s) for plane %s", getFreightName(qAuftrag).c_str(),
                         getWeekday(qFPE.Startdate).c_str(), getWeekday(qAuftrag.Date).c_str(), qPlane.Name.c_str());
            }

            if (qFPE.Startdate > qAuftrag.BisDate) {
                nIncorrect++;
                AT_Warn("Helper::checkPlaneSchedule(): Freight job (%s) starts too late (%s instead of %s) for plane %s", getFreightName(qAuftrag).c_str(),
                        getWeekday(qFPE.Startdate).c_str(), getWeekday(qAuftrag.BisDate).c_str(), qPlane.Name.c_str());
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
    const auto &qPlane = qPlayer.Planes[planeId];
    printFlightJobs(qPlayer, qPlane);
}

void printFlightJobs(const PLAYER &qPlayer, const CPlane &qPlane) {
    const auto &qFlightPlan = qPlane.Flugplan.Flug;

    printf("Helper::printFlightJobs(): Schedule for plane %s:\n", Helper::getPlaneName(qPlane).c_str());

    /* print job list */
    for (SLONG d = 0; d < qFlightPlan.AnzEntries(); d++) {
        auto objectType = qFlightPlan[d].ObjectType;
        if (objectType == 0) {
            continue;
        }
        setColorForFlightJob(qPlayer, qFlightPlan[d]);
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
                setColorForFlightJob(qPlayer, qFlightPlan[idx]);
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
    const auto &qPlane = qPlayer.Planes[planeId];
    const auto &qFlightPlan = qPlane.Flugplan.Flug;
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
    if (info.scheduleStart.getDate() == INT_MAX) {
        info.scheduleStart = {Sim.Date, 0};
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

        bool printInfo = false;
        if (info.tonsOpen > 0) {
            printInfo = true;
            AT_Warn("Helper::checkPlaneSchedule(): There are still %d tons open for job %s", info.tonsOpen, getFreightName(qAuftrag).c_str());
        }
        if ((-info.tonsOpen) >= info.smallestDecrement) {
            printInfo = true;
            AT_Error("Helper::checkPlaneSchedule(): At least one instance of job %s can be removed (overscheduled: %d, smallest: %d)",
                     getFreightName(qAuftrag).c_str(), (-info.tonsOpen), info.smallestDecrement);
        }

        if (printInfo) {
            assert(info.planeNames.size() == info.FPEs.size());
            for (SLONG i = 0; i < info.FPEs.size(); i++) {
                printf("+ %d t on %s\n", info.tonsPerPlane[i], info.planeNames[i].c_str());
                Helper::printFPE(info.FPEs[i]);
            }
        }
    }

    printf("Helper::checkFlightJobs(): %s: Found %d problems for %d planes.\n", qPlayer.AirlineX.c_str(), nIncorrect, nPlanes);
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
        printf("========== %s ==========\n", qPlayer.AirlineX.c_str());
        checkFlightJobs(qPlayer, !infoOnly, !infoOnly);
        printf("==============================\n");
    }
}

bool checkRoomOpen(SLONG roomId) {
    SLONG time = Sim.Time;
    switch (roomId) {
    case ACTION_VISITDUTYFREE:
        return (time >= timeDutyOpen && ((Sim.Weekday != 5 && Sim.Weekday != 6) || time < timeDutyClose));
    case ACTION_BUY_KEROSIN:
        [[fallthrough]];
    case ACTION_BUY_KEROSIN_TANKS:
        [[fallthrough]];
    case ACTION_VISITARAB:
        return (Sim.Difficulty >= DIFF_EASY || Sim.Difficulty == DIFF_FREEGAME) && (time >= timeArabOpen && Sim.Weekday != 6);
    case ACTION_CHECKAGENT1:
        return (time < timeLastClose && Sim.Weekday != 5 && GlobalUse(USE_TRAVELHOLDING));
    case ACTION_BUYUSEDPLANE:
        [[fallthrough]];
    case ACTION_VISITMUSEUM:
        return (time >= timeMuseOpen && Sim.Weekday != 5 && Sim.Weekday != 6);
    case ACTION_CHECKAGENT2:
        return (time < timeReisClose && GlobalUse(USE_TRAVELHOLDING));
    case ACTION_BUYNEWPLANE:
        return (time < timeMaklClose);
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
    case ACTION_SABOTAGE:
        [[fallthrough]];
    case ACTION_VISITSABOTEUR:
        return (Sim.Difficulty >= DIFF_FIRST || Sim.Difficulty == DIFF_FREEGAME);
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
        [[fallthrough]];
    case ACTION_OVERTAKE_AIRLINE:
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
        return static_cast<UBYTE>(PlayerNum * 10 + 10);
    case ACTION_PERSONAL:
        return static_cast<UBYTE>(PlayerNum * 10 + 11);
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
        [[fallthrough]];
    case ACTION_VISITSABOTEUR:
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

SLONG getWalkDistance(int playerNum, SLONG roomId) {
    auto primaryTarget = Airport.GetRandomTypedRune(RUNE_2SHOP, roomId);
    const PERSON &qPerson = Sim.Persons[Sim.Persons.GetPlayerIndex(playerNum)];

    SLONG speedCount = std::abs(qPerson.Position.x - primaryTarget.x);
    speedCount += std::abs(qPerson.Position.y - primaryTarget.y);

    if (std::abs(qPerson.Position.y - primaryTarget.y) > 4600) {
        speedCount -= 4600;
    }
    speedCount = std::max(1, speedCount);
    return speedCount;
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
        AT_Error("BotHelper.cpp: Default case should not be reached.");
        DebugBreak();
        return "INVALID";
    }
    return "INVALID";
}

} // namespace Helper
