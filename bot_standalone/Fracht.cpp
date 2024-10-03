//============================================================================================
// Fracht.cpp : Der Fracht Schalter (neu im Add-On)
//============================================================================================
#include "compat.h"
#include "compat_global.h"
#include "compat_misc.h"

extern SLONG PlayerMaxPassagiere;
extern SLONG PlayerMaxLength;
extern SLONG PlayerMinPassagiere;
extern SLONG PlayerMinLength;

void CalcPlayerMaximums(bool bForce = false);

//--------------------------------------------------------------------------------------------
// Legt Städte fest: (AreaType: 0=Europa-Europa, 1=Region-gleicher Region, 2=alles
//--------------------------------------------------------------------------------------------
void CFracht::RandomCities(SLONG AreaType, SLONG HomeCity, TEAKRAND *pRand) {
    SLONG TimeOut = 0;

    do {
        switch (AreaType) {
        // Region-Gleiche Region:
        case 0: {
            SLONG Area = 1 + pRand->Rand(4);
            VonCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(Area, pRand));
            NachCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(Area, pRand));
        } break;

            // Europa:
        case 1:
            VonCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(Cities[Sim.HomeAirportId].Areacode, pRand));
            NachCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(Cities[Sim.HomeAirportId].Areacode, pRand));
            switch (pRand->Rand(2)) {
            case 0:
                VonCity = HomeCity;
                break;
            case 1:
                NachCity = HomeCity;
                break;
            default:
                hprintf("Fracht.cpp: Default case should not be reached.");
                DebugBreak();
            }
            break;

            // Irgendwas:
        case 2:
            VonCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(pRand));
            NachCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(pRand));
            break;

            // Die ersten Tage:
        case 4:
            VonCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(Cities[Sim.HomeAirportId].Areacode, pRand));
            NachCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(Cities[Sim.HomeAirportId].Areacode, pRand));
            switch (pRand->Rand(2)) {
            case 0:
                VonCity = HomeCity;
                break;
            case 1:
                NachCity = HomeCity;
                break;
            default:
                hprintf("Fracht.cpp: Default case should not be reached.");
                DebugBreak();
            }
            break;
        default:
            hprintf("Fracht.cpp: Default case should not be reached.");
            DebugBreak();
        }

        TimeOut++;

        if (VonCity < 0x1000000) {
            VonCity = Cities.GetIdFromIndex(VonCity);
        }
        if (NachCity < 0x1000000) {
            NachCity = Cities.GetIdFromIndex(NachCity);
        }

        if (TimeOut > 300 && VonCity != NachCity) {
            break;
        }
    } while (VonCity == NachCity || (AreaType == 4 && Cities.CalcDistance(VonCity, NachCity) > 10000000));
}

//--------------------------------------------------------------------------------------------
// Fügt einen neuen Auftrag für den Spielbeginn ein:
//--------------------------------------------------------------------------------------------
void CFracht::RefillForBegin(SLONG AreaType, TEAKRAND *pRand) {
    SLONG TimeOut = 0;

    InPlan = 0;
    Okay = 1;

too_large:
    if (Sim.Difficulty == DIFF_ADDON03 && pRand->Rand(100) > 25) {
        NachCity = Sim.KrisenCity;

        do {
            VonCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(pRand));
        } while (VonCity == ULONG(Sim.KrisenCity));
    } else {
        do {
            RandomCities(AreaType, Sim.HomeAirportId, pRand);

            VonCity = Sim.HomeAirportId;

            if (Sim.Difficulty == DIFF_ADDON03 && pRand->Rand(100) > 25) {
                NachCity = Sim.KrisenCity;
            }
            if (Sim.Difficulty == DIFF_ADDON03 && VonCity == ULONG(Sim.KrisenCity)) {
                continue;
            }
        } while (VonCity == NachCity || Cities.CalcDistance(VonCity, NachCity) > 5000000);
    }

    Date = UWORD(Sim.Date);
    BisDate = UWORD(Sim.Date);
    Tons = 30;
    InPlan = 0;
    Praemie = ((CalculateFlightCostNoTank(VonCity, NachCity, 8000, 700)) + 99) / 100 * 130;
    Strafe = Praemie / 2 * 100 / 100;

    SLONG Type = pRand->Rand(100);

    // Typ A = Normal, Gewinn möglich, etwas Strafe
    if (Type >= 0 && Type < 50) {
        jobType = 0;
    }
    // Typ B = Hoffmann, Gewinn möglich, keine Strafe
    else if (Type >= 50 && Type < 65) {
        BisDate = UWORD(Sim.Date + 6);
        jobType = 1;
    }
    // Typ C = Zeit knapp, viel Gewinn, viel Strafe
    else if (Type >= 65 && Type < 95) {
        Praemie *= 2;
        Strafe = Praemie * 2;
        BisDate = UWORD(Sim.Date + 1);
        jobType = 2;
    }
    // Typ E = Glücksfall, viel Gewinn, keine Strafe
    else if (Type >= 95 && Type < 100) {
        Praemie *= 2;
        Strafe = 0;
        jobType = 4;
    }

    jobSizeType = 2;

    if (AreaType == 1) {
        Praemie = Praemie * 3 / 2;
    }
    if (AreaType == 2) {
        Praemie = Praemie * 8 / 5;
    }
    if (Date != BisDate) {
        Praemie = Praemie * 4 / 5;
    }

    if (NachCity == ULONG(Sim.KrisenCity) && Sim.Difficulty == DIFF_ADDON03) {
        Praemie = 0;
        Strafe /= 2;
    }

    TimeOut++;

    if ((Cities.CalcDistance(VonCity, NachCity) > PlayerMaxLength) && TimeOut < 80) {
        goto too_large;
    }

    if ((Cities.CalcDistance(VonCity, NachCity) > PlayerMinLength) && TimeOut < 60) {
        goto too_large;
    }

    if ((Cities.CalcDistance(VonCity, NachCity) > PlayerMaxLength)) {
        Strafe = 0;
        Praemie = Praemie * 2;
        Date = static_cast<UWORD>(Sim.Date);
        BisDate = Date + 5;
    }

    TonsOpen = TonsLeft = Tons;
}

//--------------------------------------------------------------------------------------------
// Fügt einen neuen Auftrag ein:
//--------------------------------------------------------------------------------------------
void CFracht::Refill(SLONG AreaType, TEAKRAND *pRnd) {
    SLONG TimeOut = 0;

    InPlan = 0;
    Okay = 1;

too_large:
    if (Sim.Difficulty == DIFF_ADDON03 && pRnd->Rand(100) > 25) {
        NachCity = Sim.KrisenCity;

        do {
            VonCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(pRnd));
        } while (VonCity == ULONG(Sim.KrisenCity));
    } else {
        RandomCities(AreaType, Sim.HomeAirportId, pRnd);
    }

    Tons = 30;
    Date = UWORD(Sim.Date + 1 + pRnd->Rand(3));
    BisDate = Date;
    InPlan = 0;

    // Kopie dieser Formel auch bei Last-Minute
    Praemie = ((CalculateFlightCostNoTank(VonCity, NachCity, 8000, 700)) + 99) / 100 * 105;
    Strafe = Praemie / 2 * 100 / 100;

    if (pRnd->Rand(5) == 4) {
        BisDate += ((pRnd->Rand(5)));
    }

    SLONG Type = pRnd->Rand(100);

    // Typ A = Normal, Gewinn möglich, etwas Strafe
    if (Type >= 0 && Type < 50) {
        jobType = 0;
    }
    // Typ B = Hoffmann, Gewinn möglich
    else if (Type >= 50 && Type < 65) {
        Date = UWORD(Sim.Date);
        BisDate = UWORD(Sim.Date + 4 + pRnd->Rand(3));
        jobType = 1;
    }
    // Typ C = Zeit knapp, viel Gewinn, viel Strafe
    else if (Type >= 65 && Type < 80) {
        Praemie *= 2;
        Strafe = Praemie * 2;
        BisDate = Date = UWORD(Sim.Date + 1);
        jobType = 2;
    }
    // Typ D = Betrug, kein Gewinn möglich, etwas Strafe
    else if (Type >= 80 && Type < 95) {
        Praemie /= 2;
        jobType = 3;
    }
    // Typ E = Glücksfall, viel Gewinn, keine Strafe
    else if (Type >= 95 && Type < 100) {
        Praemie *= 2;
        Strafe = 0;
        jobType = 4;
    }

    Type = pRnd->Rand(100);

    if (Type == 0) {
        Tons = 1;
        Praemie = Praemie * 4;
        Strafe = Praemie * 4;
        jobSizeType = 0;
    } else if (Type < 15 || (Sim.Date < 4 && Type < 30) || (Sim.Date < 8 && Type < 20) || (Sim.Difficulty == DIFF_TUTORIAL && Type < 70)) {
        Tons = 15;
        Praemie = Praemie * 3 / 4;
        jobSizeType = 1;
    } else if (Type < 40 || Sim.Difficulty == DIFF_TUTORIAL) {
        Tons = 30;
        /* Praemie bleibt gleich */
        jobSizeType = 2;
    } else if (Type < 70) {
        Tons = 40;
        Praemie = Praemie * 5 / 4;
        jobSizeType = 3;
    } else if (Type < 90) {
        Tons = 60;
        Praemie = Praemie * 6 / 4;
        jobSizeType = 4;
    } else {
        Tons = 100;
        Praemie = Praemie * 7 / 4;
        jobSizeType = 5;
    }

    if (AreaType == 1) {
        Praemie = Praemie * 3 / 2;
    }
    if (AreaType == 2) {
        Praemie = Praemie * 8 / 5;
    }
    if (Date != BisDate) {
        Date = static_cast<UWORD>(Sim.Date);
        Praemie = Praemie * 4 / 5;
    }

    if (NachCity == ULONG(Sim.KrisenCity) && Sim.Difficulty == DIFF_ADDON03) {
        Praemie = 0;
        Strafe /= 2;
    }

    if (Tons > 80) {
        Praemie *= 5;
    } else if (Tons > 60) {
        Praemie *= 4;
    } else if (Tons > 30) {
        Praemie *= 3;
    }

    if ((Cities.CalcDistance(VonCity, NachCity) > PlayerMaxLength) && TimeOut++ < 100) {
        goto too_large;
    }

    TonsOpen = TonsLeft = Tons;
}

//--------------------------------------------------------------------------------------------
// Passt der Auftrag in ein Flugzeug von der Sorte?
//--------------------------------------------------------------------------------------------
BOOL CFracht::FitsInPlane(const CPlane &Plane) const {
    if (Cities.CalcDistance(VonCity, NachCity) > Plane.ptReichweite * 1000) {
        return (FALSE);
    }
    if (Cities.CalcFlugdauer(VonCity, NachCity, Plane.ptGeschwindigkeit) >= 24) {
        return (FALSE);
    }

    return (TRUE);
}

//============================================================================================
// CFrachten::
//============================================================================================
// Fügt eine Reihe von neuen Aufträgen ein:
//============================================================================================
void CFrachten::Fill() {

    CalcPlayerMaximums();

    ReSize(6);
    FillAlbum();

    SLONG c = 0;
    for (auto &f : *this) {
        f.Refill(c / 2, &Random);
        c++;
    }

    if (Sim.Difficulty == DIFF_ATFS10 && Sim.Date >= 25 && Sim.Date <= 35) {
        for (auto &f : *this) {
            f.Praemie = -1;
        }
    }
}

//--------------------------------------------------------------------------------------------
// Fügt einen neuen Auftrag ein:
//--------------------------------------------------------------------------------------------
void CFrachten::Refill(SLONG Minimum) {
    SLONG Anz = AnzEntries();

    CalcPlayerMaximums();

    ReSize(6);
    FillAlbum();

    SLONG c = 0;
    for (auto &f : *this) {
        if (Anz <= 0) {
            break;
        }
        if (f.Praemie < 0) {
            if (Sim.Date < 5 && c < 5) {
                f.Refill(4, &Random);
            } else if (Sim.Date < 10 && c < 3) {
                f.Refill(4, &Random);
            } else {
                f.Refill(c / 2, &Random);
            }

            Anz--;
        }
        c++;
    }

    for (auto &f : *this) {
        if (Anz <= 0) {
            break;
        }
        if (f.Praemie != -1) {
            Minimum--;
        }
    }

    c = 0;
    for (auto &f : *this) {
        if (Anz <= 0) {
            break;
        }
        if (f.Praemie < 0 && Minimum > 0) {
            if (Sim.Date < 5 && c < 5) {
                f.Refill(4, &Random);
            } else if (Sim.Date < 10 && c < 3) {
                f.Refill(4, &Random);
            } else {
                f.Refill(c / 2, &Random);
            }

            Minimum--;
        }
        c++;
    }

    if (Sim.Difficulty == DIFF_ATFS10 && Sim.Date >= 25 && Sim.Date <= 35) {
        for (auto &f : *this) {
            f.Praemie = -1;
        }
    }
}

//--------------------------------------------------------------------------------------------
// Returns the number of open Order flights which are due today:
//--------------------------------------------------------------------------------------------
SLONG CFrachten::GetNumDueToday() {
    SLONG Anz = 0;

    for (auto &f : *this) {
        if (f.BisDate >= Sim.Date) {
            if (f.TonsOpen > 0 && f.BisDate == Sim.Date) {
                Anz++;
            }
        }
    }

    return (Anz);
}

//--------------------------------------------------------------------------------------------
// Returns the number of open Order flights which still must be planned:
//--------------------------------------------------------------------------------------------
SLONG CFrachten::GetNumOpen() {
    SLONG Anz = 0;

    for (auto &f : *this) {
        if (f.BisDate >= Sim.Date) {
            if (f.TonsOpen > 0) {
                Anz++;
            }
        }
    }

    return (Anz);
}

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
void CFracht::RefillForAusland(SLONG AreaType, SLONG CityNum, TEAKRAND *pRandom) {
    SLONG TimeOut = 0;

    InPlan = 0;
    Okay = 1;

    if (CityNum < 0x1000000) {
        CityNum = Cities.GetIdFromIndex(CityNum);
    }

too_large:
    do {
        VonCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(pRandom));
        NachCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(pRandom));
        switch (pRandom->Rand(2)) {
        case 0:
            VonCity = CityNum;
            break;
        case 1:
            NachCity = CityNum;
            break;
        default:
            hprintf("Fracht.cpp: Default case should not be reached.");
            DebugBreak();
        }
    } while (VonCity == NachCity || (AreaType == 4 && Cities.CalcDistance(VonCity, NachCity) > 10000000));

    Tons = 30;
    Date = UWORD(Sim.Date + 1 + pRandom->Rand(3));
    BisDate = Date;
    InPlan = 0;

    // Kopie dieser Formel auch bei Last-Minute
    Praemie = ((CalculateFlightCostNoTank(VonCity, NachCity, 8000, 700)) + 99) / 100 * 105;
    Strafe = Praemie / 2 * 100 / 100;

    if (pRandom->Rand(5) == 4) {
        BisDate += ((pRandom->Rand(5)));
    }

    SLONG Type = pRandom->Rand(100);

    // Typ A = Normal, Gewinn möglich, etwas Strafe
    if (Type >= 0 && Type < 50) {
        jobType = 0;
    }
    // Typ B = Hoffmann, Gewinn möglich
    else if (Type >= 50 && Type < 65) {
        Date = UWORD(Sim.Date);
        BisDate = UWORD(Sim.Date + 4 + pRandom->Rand(3));
        jobType = 1;
    }
    // Typ C = Zeit knapp, viel Gewinn, viel Strafe
    else if (Type >= 65 && Type < 80) {
        Praemie *= 2;
        Strafe = Praemie * 2;
        BisDate = Date = UWORD(Sim.Date + 1);
        jobType = 2;
    }
    // Typ D = Betrug, kein Gewinn möglich, etwas Strafe
    else if (Type >= 80 && Type < 95) {
        Praemie /= 2;
        jobType = 3;
    }
    // Typ E = Glücksfall, viel Gewinn, keine Strafe
    else if (Type >= 95 && Type < 100) {
        Praemie *= 2;
        Strafe = 0;
        jobType = 4;
    }

    Type = pRandom->Rand(100);

    if (Type == 0) {
        Tons = 1;
        Praemie = Praemie * 4;
        Strafe = Praemie * 4;
        jobSizeType = 0;
    } else if (Type < 15 || (Sim.Date < 4 && Type < 30) || (Sim.Date < 8 && Type < 20) || (Sim.Difficulty == DIFF_TUTORIAL && Type < 70)) {
        Tons = 15;
        Praemie = Praemie * 3 / 4;
        jobSizeType = 1;
    } else if (Type < 40 || Sim.Difficulty == DIFF_TUTORIAL) {
        Tons = 30;
        /* Praemie bleibt gleich */
        jobSizeType = 2;
    } else if (Type < 70) {
        Tons = 40;
        Praemie = Praemie * 5 / 4;
        jobSizeType = 3;
    } else if (Type < 90) {
        Tons = 60;
        Praemie = Praemie * 6 / 4;
        jobSizeType = 4;
    } else {
        Tons = 100;
        Praemie = Praemie * 7 / 4;
        jobSizeType = 5;
    }

    if (AreaType == 1) {
        Praemie = Praemie * 3 / 2;
    }
    if (AreaType == 2) {
        Praemie = Praemie * 8 / 5;
    }
    if (Date != BisDate) {
        Date = static_cast<UWORD>(Sim.Date);
        Praemie = Praemie * 4 / 5;
    }

    if (NachCity == ULONG(Sim.KrisenCity) && Sim.Difficulty == DIFF_ADDON03) {
        Praemie = 0;
        Strafe /= 2;
    }

    if (Tons > 80) {
        Praemie *= 5;
    } else if (Tons > 60) {
        Praemie *= 4;
    } else if (Tons > 30) {
        Praemie *= 3;
    }

    if ((Cities.CalcDistance(VonCity, NachCity) > PlayerMaxLength) && TimeOut++ < 100) {
        goto too_large;
    }

    TonsOpen = TonsLeft = Tons;
}

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
void CFrachten::RefillForAusland(SLONG CityNum, SLONG Minimum) {
    SLONG Anz = AnzEntries();

    CalcPlayerMaximums();

    ReSize(6);
    FillAlbum();

    SLONG c = 0;
    for (auto &f : *this) {
        if (Anz <= 0) {
            break;
        }
        if (f.Praemie <= 0) {
            if (Sim.Date < 5 && c < 5) {
                f.RefillForAusland(4, CityNum, &Random);
            } else if (Sim.Date < 10 && c < 3) {
                f.RefillForAusland(4, CityNum, &Random);
            } else {
                f.RefillForAusland(c / 2, CityNum, &Random);
            }

            Anz--;
        }
        c++;
    }

    for (auto &f : *this) {
        if (Anz <= 0) {
            break;
        }
        if (f.Praemie != 0) {
            Minimum--;
        }
    }

    c = 0;
    for (auto &f : *this) {
        if (Anz <= 0) {
            break;
        }
        if (f.Praemie <= 0 && Minimum > 0) {
            if (Sim.Date < 5 && c < 5) {
                f.RefillForAusland(4, CityNum, &Random);
            } else if (Sim.Date < 10 && c < 3) {
                f.RefillForAusland(4, CityNum, &Random);
            } else {
                f.RefillForAusland(c / 2, CityNum, &Random);
            }

            Minimum--;
        }
        c++;
    }

    AuslandsFRefill[CityNum] = 0;
}

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
void CFrachten::FillForAusland(SLONG CityNum) {

    CalcPlayerMaximums();

    ReSize(6); // ex:10
    FillAlbum();

    SLONG c = 0;
    for (auto &f : *this) {
        if (Sim.Date < 5 && c < 5) {
            f.RefillForAusland(4, CityNum, &Random);
        } else if (Sim.Date < 10 && c < 3) {
            f.RefillForAusland(4, CityNum, &Random);
        } else {
            f.RefillForAusland(c / 2, CityNum, &Random);
        }
        c++;
    }
}
