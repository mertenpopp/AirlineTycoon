//============================================================================================//============================================================================================
// Auftrag.cpp : Routinen für die Aufträge die die Spieler haben
//============================================================================================
#include "compat.h"
#include "compat_global.h"
#include "compat_misc.h"

SLONG PlayerMaxPassagiere = 90;
SLONG PlayerMaxLength = 2000000;
SLONG PlayerMinPassagiere = 90;
SLONG PlayerMinLength = 2000000;

void CalcPlayerMaximums(bool bForce = false);

//--------------------------------------------------------------------------------------------
// Berechnet, was für Aufträge der Spieler maximal annehmen kann:
//--------------------------------------------------------------------------------------------
void CalcPlayerMaximums(bool bForce) {
    static SLONG LastHour = -1;

    if (LastHour == Sim.GetHour() && !bForce) {
        return;
    }

    LastHour = Sim.GetHour();

    PlayerMaxPassagiere = 90;
    PlayerMaxLength = 2000;
    PlayerMinPassagiere = 2147483647;
    PlayerMinLength = 2147483647;

    for (SLONG c = 0; c < Sim.Players.AnzPlayers; c++) {
        if ((Sim.Players.Players[c].IsOut == 0) && (Sim.Players.Players[c].Owner == 0 || Sim.Players.Players[c].Owner == 2)) {
            PLAYER &qPlayer = Sim.Players.Players[c];

            for (SLONG d = 0; d < qPlayer.Planes.AnzEntries(); d++) {
                if (qPlayer.Planes.IsInAlbum(d) != 0) {
                    if (qPlayer.Planes[d].MaxPassagiere + qPlayer.Planes[d].MaxPassagiereFC > PlayerMaxPassagiere) {
                        PlayerMaxPassagiere = qPlayer.Planes[d].ptPassagiere;
                    }

                    if (qPlayer.Planes[d].ptReichweite > PlayerMaxLength) {
                        PlayerMaxLength = qPlayer.Planes[d].ptReichweite;
                    }

                    if (qPlayer.Planes[d].MaxPassagiere + qPlayer.Planes[d].MaxPassagiereFC < PlayerMinPassagiere) {
                        PlayerMinPassagiere = qPlayer.Planes[d].ptPassagiere;
                    }

                    if (qPlayer.Planes[d].ptReichweite < PlayerMinLength) {
                        PlayerMinLength = qPlayer.Planes[d].ptReichweite;
                    }
                }
            }
        }
    }

    PlayerMaxLength *= 1000;
    PlayerMinLength *= 1000;
}

//============================================================================================
// CAuftrag::
//============================================================================================
// Konstruktor:
//============================================================================================
CAuftrag::CAuftrag(ULONG VonCity, ULONG NachCity, ULONG Personen, UWORD Date, SLONG Praemie, SLONG Strafe) {
    CAuftrag::VonCity = VonCity;
    CAuftrag::NachCity = NachCity;
    CAuftrag::Personen = Personen;
    CAuftrag::Date = Date;
    CAuftrag::InPlan = 0;
    CAuftrag::Praemie = Praemie;
    CAuftrag::Strafe = Strafe;
    CAuftrag::bUhrigFlight = FALSE;
}

//--------------------------------------------------------------------------------------------
// Konstruktor:
//--------------------------------------------------------------------------------------------
CAuftrag::CAuftrag(ULONG VonCity, ULONG NachCity, ULONG Personen, UWORD Date) {
    CAuftrag::VonCity = VonCity;
    CAuftrag::NachCity = NachCity;
    CAuftrag::Personen = Personen;
    CAuftrag::Date = Date;
    CAuftrag::InPlan = 0;
    CAuftrag::Praemie = 100; //==>+<==
    CAuftrag::Strafe = 100;
    CAuftrag::bUhrigFlight = FALSE;
}

//--------------------------------------------------------------------------------------------
// Konstruktor:
//--------------------------------------------------------------------------------------------
CAuftrag::CAuftrag(char *VonCity, char *NachCity, ULONG Personen, UWORD Date, SLONG Praemie, SLONG Strafe) {
    CAuftrag::VonCity = Cities.GetIdFromName(VonCity);
    CAuftrag::NachCity = Cities.GetIdFromName(NachCity);
    CAuftrag::Personen = Personen;
    CAuftrag::Date = Date;
    CAuftrag::InPlan = 0;
    CAuftrag::Praemie = Praemie;
    CAuftrag::Strafe = Strafe;
    CAuftrag::bUhrigFlight = FALSE;
}

//--------------------------------------------------------------------------------------------
// Konstruktor:
//--------------------------------------------------------------------------------------------
CAuftrag::CAuftrag(char *VonCity, char *NachCity, ULONG Personen, UWORD Date) {
    CAuftrag::VonCity = Cities.GetIdFromName(VonCity);
    CAuftrag::NachCity = Cities.GetIdFromName(NachCity);
    CAuftrag::Personen = Personen;
    CAuftrag::Date = Date;
    CAuftrag::InPlan = 0;
    CAuftrag::Praemie = 100; //==>+<==
    CAuftrag::Strafe = 100;
    CAuftrag::bUhrigFlight = FALSE;
}

//--------------------------------------------------------------------------------------------
// Legt Städte fest: (AreaType: 0=Europa-Europa, 1=Region-gleicher Region, 2=alles
//--------------------------------------------------------------------------------------------
void CAuftrag::RandomCities(SLONG AreaType, SLONG HomeCity, TEAKRAND *pRandom) {
    SLONG TimeOut = 0;

    do {
        switch (AreaType) {
        // Region-Gleiche Region:
        case 0: {
            SLONG Area = 1 + pRandom->Rand(4);

            VonCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(Area, pRandom));
            NachCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(Area, pRandom));
        } break;

            // Europa:
        case 1:
            VonCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(Cities[Sim.HomeAirportId].Areacode, pRandom));
            NachCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(Cities[Sim.HomeAirportId].Areacode, pRandom));
            switch (pRandom->Rand(2)) {
            case 0:
                VonCity = HomeCity;
                break;
            case 1:
                NachCity = HomeCity;
                break;
            default:
                hprintf("Auftrag.cpp: Default case should not be reached.");
                DebugBreak();
            }
            break;

            // Irgendwas:
        case 2:
            VonCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(pRandom));
            NachCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(pRandom));
            break;

            // Die ersten Tage:
        case 4:
            VonCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(Cities[Sim.HomeAirportId].Areacode, pRandom));
            NachCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(Cities[Sim.HomeAirportId].Areacode, pRandom));
            switch (pRandom->Rand(2)) {
            case 0:
                VonCity = HomeCity;
                break;
            case 1:
                NachCity = HomeCity;
                break;
            default:
                hprintf("Auftrag.cpp: Default case should not be reached.");
                DebugBreak();
            }
            break;
        default:
            hprintf("Auftrag.cpp: Default case should not be reached.");
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
// Fügt einen neuen Auftrag ein:
//--------------------------------------------------------------------------------------------
void CAuftrag::RefillForLastMinute(SLONG AreaType, TEAKRAND *pRandom) {
    SLONG TimeOut = 0;

    // NetGenericSync (2000, AreaType);
    // NetGenericSync (2001, pRandom->GetSeed());

too_large:
    RandomCities(AreaType, Sim.HomeAirportId, pRandom);

    Date = UWORD(Sim.Date);
    BisDate = Date;
    InPlan = 0;

    // Kopie dieser Formel auch bei Last-Minute
    Praemie = ((CalculateFlightCostNoTank(VonCity, NachCity, 8000, 700)) + 99) / 100 * 130;
    Strafe = Praemie / 2 * 100 / 100;

    Praemie = Praemie * 5 / 4; // Last-Minute Zuschlag

    SLONG Type = pRandom->Rand(100);

    // Typ A = Normal, Gewinn möglich, etwas Strafe
    if (Type >= 0 && Type < 50) {
        jobType = 0;
    }
    // Typ B = Hoffmann, Gewinn möglich, keine Strafe
    else if (Type >= 50 && Type < 60) {
        Date++;
        BisDate++;
        jobType = 1;
    }
    // Typ C = Zeit knapp, viel Gewinn, viel Strafe
    else if (Type >= 60 && Type < 80) {
        Praemie *= 2;
        Strafe = Praemie * 4;
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
        Personen = 1;
        Praemie = Praemie * 4;
        Strafe = Praemie * 4;
        jobSizeType = 0;
    } else if (Type < 15 || (Sim.Date < 4 && Type < 30) || (Sim.Date < 8 && Type < 20) || (Sim.Difficulty == DIFF_TUTORIAL && Type < 70)) {
        Personen = 90;
        Praemie = Praemie * 3 / 4;
        jobSizeType = 1;
    } else if (Type < 40 || Sim.Difficulty == DIFF_TUTORIAL) {
        Personen = 180;
        /* Praemie bleibt gleich */
        jobSizeType = 2;
    } else if (Type < 70) {
        Personen = 280;
        Praemie = Praemie * 5 / 4;
        jobSizeType = 3;
    } else if (Type < 90) {
        Personen = 340;
        Praemie = Praemie * 6 / 4;
        jobSizeType = 4;
    } else {
        Personen = 430;
        Praemie = Praemie * 7 / 4;
        jobSizeType = 5;
    }

    if (AreaType == 1) {
        Praemie = Praemie * 3 / 2;
    }
    if (AreaType == 2) {
        Praemie = Praemie * 8 / 5;
    }

    if ((SLONG(Personen) > PlayerMaxPassagiere || Cities.CalcDistance(VonCity, NachCity) > PlayerMaxLength) && TimeOut++ < 100) {
        goto too_large;
    }
}

//--------------------------------------------------------------------------------------------
// Fügt einen neuen Auftrag ein:
//--------------------------------------------------------------------------------------------
void CAuftrag::RefillForReisebuero(SLONG AreaType, TEAKRAND *pRandom) {
    SLONG TimeOut = 0;

    // NetGenericSync (3000, AreaType);
    // NetGenericSync (3001, pRandom->GetSeed());

too_large:
    RandomCities(AreaType, Sim.HomeAirportId, pRandom);

    Personen = 180;
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
        Personen = 2;
        Praemie = Praemie * 4;
        Strafe = Praemie * 4;
        jobSizeType = 0;
    } else if (Type < 15 || (Sim.Date < 4 && Type < 30) || (Sim.Date < 8 && Type < 20) || (Sim.Difficulty == DIFF_TUTORIAL && Type < 70)) {
        Personen = 90;
        Praemie = Praemie * 3 / 4;
        jobSizeType = 1;
    } else if (Type < 40 || Sim.Difficulty == DIFF_TUTORIAL) {
        Personen = 180;
        /* Praemie bleibt gleich */
        jobSizeType = 2;
    } else if (Type < 70) {
        Personen = 280;
        Praemie = Praemie * 5 / 4;
        jobSizeType = 3;
    } else if (Type < 90) {
        Personen = 340;
        Praemie = Praemie * 6 / 4;
        jobSizeType = 4;
    } else {
        Personen = 430;
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

    if ((SLONG(Personen) > PlayerMaxPassagiere || Cities.CalcDistance(VonCity, NachCity) > PlayerMaxLength) && TimeOut++ < 100) {
        goto too_large;
    }
}

//--------------------------------------------------------------------------------------------
// Fügt einen neuen Auftrag für den Spielbeginn ein:
//--------------------------------------------------------------------------------------------
void CAuftrag::RefillForBegin(SLONG AreaType, TEAKRAND *pRandom) {
    SLONG TimeOut = 0;

    // NetGenericSync (4000, AreaType);
    // NetGenericSync (4001, pRandom->GetSeed());

too_large:
    do {
        RandomCities(AreaType, Sim.HomeAirportId, pRandom);

        VonCity = Sim.HomeAirportId;
    } while (VonCity == NachCity || Cities.CalcDistance(VonCity, NachCity) > 5000000);

    Date = UWORD(Sim.Date);
    BisDate = UWORD(Sim.Date);
    Personen = 90;
    InPlan = 0;
    Praemie = ((CalculateFlightCostNoTank(VonCity, NachCity, 8000, 700)) + 99) / 100 * 100;
    Strafe = Praemie / 2 * 100 / 100;

    SLONG Type = pRandom->Rand(100);

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

    TimeOut++;

    if ((SLONG(Personen) > PlayerMaxPassagiere || Cities.CalcDistance(VonCity, NachCity) > PlayerMaxLength) && TimeOut < 80) {
        goto too_large;
    }

    if ((SLONG(Personen) > PlayerMinPassagiere || Cities.CalcDistance(VonCity, NachCity) > PlayerMinLength) && TimeOut < 60) {
        goto too_large;
    }

    if ((SLONG(Personen) > PlayerMaxPassagiere || Cities.CalcDistance(VonCity, NachCity) > PlayerMaxLength)) {
        Strafe = 0;
        Praemie = Praemie * 2;
        Date = static_cast<UWORD>(Sim.Date);
        BisDate = Date + 5;
    }
}

//--------------------------------------------------------------------------------------------
// Fügt einen neuen Auftrag für Uhrig ein:
//--------------------------------------------------------------------------------------------
void CAuftrag::RefillForUhrig(SLONG AreaType, TEAKRAND *pRandom) {
    SLONG TimeOut = 0;

    // NetGenericSync (5000, AreaType);
    // NetGenericSync (5001, pRandom->GetSeed());

too_large:
    RandomCities(AreaType, Sim.HomeAirportId, pRandom);

    Personen = 180;
    Date = UWORD(Sim.Date + 1 + pRandom->Rand(3));
    BisDate = Date;
    InPlan = 0;
    bUhrigFlight = TRUE;

    // Kopie dieser Formel auch bei Last-Minute
    Praemie = ((CalculateFlightCostNoTank(VonCity, NachCity, 8000, 700)) + 99) / 100 * 115;
    Strafe = Praemie / 2 * 100 / 100 * 2;

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
        Personen = 2;
        Praemie = Praemie * 4;
        Strafe = Praemie * 4;
        jobSizeType = 0;
    } else if (Type < 15 || (Sim.Date < 4 && Type < 30) || (Sim.Date < 8 && Type < 20) || (Sim.Difficulty == DIFF_TUTORIAL && Type < 70)) {
        Personen = 90;
        Praemie = Praemie * 3 / 4;
        jobSizeType = 1;
    } else if (Type < 40 || Sim.Difficulty == DIFF_TUTORIAL) {
        Personen = 180;
        /* Praemie bleibt gleich */
        jobSizeType = 2;
    } else if (Type < 70) {
        Personen = 280;
        Praemie = Praemie * 5 / 4;
        jobSizeType = 3;
    } else if (Type < 90) {
        Personen = 340;
        Praemie = Praemie * 6 / 4;
        jobSizeType = 4;
    } else {
        Personen = 430;
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

    if ((SLONG(Personen) > PlayerMaxPassagiere || Cities.CalcDistance(VonCity, NachCity) > PlayerMaxLength) && TimeOut++ < 100) {
        goto too_large;
    }
}

//--------------------------------------------------------------------------------------------
// Fügt einen neuen Auftrag für den Spielbeginn ein:
//--------------------------------------------------------------------------------------------
void CAuftrag::RefillForAusland(SLONG AreaType, SLONG CityNum, TEAKRAND *pRandom) {
    SLONG TimeOut = 0;

    TEAKRAND localRand;
    localRand.SRand(pRandom->Rand());

    if (CityNum < 0x1000000) {
        CityNum = Cities.GetIdFromIndex(CityNum);
    }

too_large:

    do {
        VonCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(&localRand));
        NachCity = Cities.GetIdFromIndex(Cities.GetRandomUsedIndex(&localRand));
        switch (localRand.Rand(2)) {
        case 0:
            VonCity = CityNum;
            break;
        case 1:
            NachCity = CityNum;
            break;
        default:
            hprintf("Auftrag.cpp: Default case should not be reached.");
            DebugBreak();
        }
    } while (VonCity == NachCity || (AreaType == 4 && Cities.CalcDistance(VonCity, NachCity) > 10000000));

    Personen = 180;
    Date = UWORD(Sim.Date + 1 + localRand.Rand(3));
    BisDate = Date;
    InPlan = 0;

    // Kopie dieser Formel auch bei Last-Minute
    Praemie = ((CalculateFlightCostNoTank(VonCity, NachCity, 8000, 700)) + 99) / 100 * 120;
    Strafe = Praemie / 2 * 100 / 100;

    if (localRand.Rand(5) == 4) {
        BisDate += ((localRand.Rand(5)));
    }

    SLONG Type = localRand.Rand(100);

    // Typ A = Normal, Gewinn möglich, etwas Strafe
    if (Type >= 0 && Type < 50) {
        jobType = 0;
    }
    // Typ B = Hoffmann, Gewinn möglich
    else if (Type >= 50 && Type < 65) {
        Date = UWORD(Sim.Date);
        BisDate = UWORD(Sim.Date + 4 + localRand.Rand(3));
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

    // Type = pRandom->Rand (100);
    Type = localRand.Rand(100);

    if (Type == 0) {
        Personen = 2;
        Praemie = Praemie * 4;
        Strafe = Praemie * 4;
        jobSizeType = 0;
    } else if (Type < 15 || (Sim.Date < 4 && Type < 30) || (Sim.Date < 8 && Type < 20) || (Sim.Difficulty == DIFF_TUTORIAL && Type < 70)) {
        Personen = 90;
        Praemie = Praemie * 3 / 4;
        jobSizeType = 1;
    } else if (Type < 40 || Sim.Difficulty == DIFF_TUTORIAL) {
        Personen = 180;
        /* Praemie bleibt gleich */
        jobSizeType = 2;
    } else if (Type < 70) {
        Personen = 280;
        Praemie = Praemie * 5 / 4;
        jobSizeType = 3;
    } else if (Type < 90) {
        Personen = 340;
        Praemie = Praemie * 6 / 4;
        jobSizeType = 4;
    } else {
        Personen = 430;
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

    if ((SLONG(Personen) > PlayerMaxPassagiere || Cities.CalcDistance(VonCity, NachCity) > PlayerMaxLength) && TimeOut++ < 100) {
        goto too_large;
    }
}

//--------------------------------------------------------------------------------------------
// Passt der Auftrag in ein Flugzeug von der Sorte?
//--------------------------------------------------------------------------------------------
BOOL CAuftrag::FitsInPlane(const CPlane &Plane) const {
    if (Cities.CalcDistance(VonCity, NachCity) > Plane.ptReichweite * 1000) {
        return (FALSE);
    }
    // if ((Cities.CalcDistance (VonCity, NachCity)/PlaneType.Geschwindigkeit+999)/1000+1+2>=24)
    // if (Cities.CalcFlugdauer (VonCity, NachCity, PlaneType.Geschwindigkeit)>=24)
    if (Cities.CalcFlugdauer(VonCity, NachCity, Plane.ptGeschwindigkeit) >= 24) {
        return (FALSE);
    }

    return (TRUE);
}

//============================================================================================
// CAuftraege::
//============================================================================================
// Fügt eine Reihe von neuen Aufträgen ein:
//============================================================================================
void CAuftraege::FillForLastMinute() {

    CalcPlayerMaximums();

    ReSize(6); // ex:10
    FillAlbum();

    SLONG c = 0;
    for (auto &a : *this) {
        a.RefillForLastMinute(c / 2, &Random);
        c++;
    }

    if (Sim.Difficulty == DIFF_ATFS10 && Sim.Date >= 20 && Sim.Date <= 30) {
        for (auto &a : *this) {
            a.Praemie = 0;
        }
    }
}

//--------------------------------------------------------------------------------------------
// Fügt einen neuen Auftrag ein:
//--------------------------------------------------------------------------------------------
void CAuftraege::RefillForLastMinute(SLONG Minimum) {
    SLONG Anz = AnzEntries();

    CalcPlayerMaximums();

    ReSize(6); // ex:10
    FillAlbum();

    SLONG c = 0;
    for (auto &a : *this) {
        if (Anz <= 0) {
            break;
        }
        if (a.Praemie == 0) {
            a.RefillForLastMinute(c / 2, &Random);
            Anz--;
        }
        c++;
    }

    for (auto &a : *this) {
        if (Anz <= 0) {
            break;
        }
        if (a.Praemie != 0) {
            Minimum--;
        }
    }

    c = 0;
    for (auto &a : *this) {
        if (Anz <= 0) {
            break;
        }
        if (a.Praemie == 0 && Minimum > 0) {
            a.RefillForLastMinute(c / 2, &Random);
            Minimum--;
        }
        c++;
    }

    if (Sim.Difficulty == DIFF_ATFS10 && Sim.Date >= 20 && Sim.Date <= 30) {
        for (auto &a : *this) {
            a.Praemie = 0;
        }
    }
}

//--------------------------------------------------------------------------------------------
// Fügt eine Reihe von neuen Aufträgen ein:
//--------------------------------------------------------------------------------------------
void CAuftraege::FillForReisebuero() {

    CalcPlayerMaximums();

    ReSize(6);
    FillAlbum();

    SLONG c = 0;
    for (auto &a : *this) {
        if (Sim.Date < 5 && c < 5) {
            a.RefillForAusland(4, Sim.HomeAirportId, &Random);
        } else if (Sim.Date < 10 && c < 3) {
            a.RefillForAusland(4, Sim.HomeAirportId, &Random);
        } else {
            a.RefillForAusland(c / 2, Sim.HomeAirportId, &Random);
        }
        c++;
    }

    if (Sim.Difficulty == DIFF_ATFS10 && Sim.Date >= 20 && Sim.Date <= 30) {
        for (auto &a : *this) {
            a.Praemie = 0;
        }
    }
}

//--------------------------------------------------------------------------------------------
// Fügt einen neuen Auftrag ein:
//--------------------------------------------------------------------------------------------
void CAuftraege::RefillForReisebuero(SLONG Minimum) {
    SLONG Anz = AnzEntries();

    // NetGenericSync (8000, Minimum);
    // NetGenericSync (8001, Random.GetSeed());

    CalcPlayerMaximums();

    ReSize(6);
    FillAlbum();

    SLONG c = 0;
    for (auto &a : *this) {
        if (Anz <= 0) {
            break;
        }
        if (a.Praemie == 0) {
            if (Sim.Date < 5 && c < 5) {
                a.RefillForAusland(4, Sim.HomeAirportId, &Random);
            } else if (Sim.Date < 10 && c < 3) {
                a.RefillForAusland(4, Sim.HomeAirportId, &Random);
            } else {
                a.RefillForAusland(c / 2, Sim.HomeAirportId, &Random);
            }

            Anz--;
        }
        c++;
    }

    for (auto &a : *this) {
        if (Anz <= 0) {
            break;
        }
        if (a.Praemie != 0) {
            Minimum--;
        }
    }

    c = 0;
    for (auto &a : *this) {
        if (Anz <= 0) {
            break;
        }
        if (a.Praemie == 0 && Minimum > 0) {
            if (Sim.Date < 5 && c < 5) {
                a.RefillForAusland(4, Sim.HomeAirportId, &Random);
            } else if (Sim.Date < 10 && c < 3) {
                a.RefillForAusland(4, Sim.HomeAirportId, &Random);
            } else {
                a.RefillForAusland(c / 2, Sim.HomeAirportId, &Random);
            }

            Minimum--;
        }
        c++;
    }

    if (Sim.Difficulty == DIFF_ATFS10 && Sim.Date >= 20 && Sim.Date <= 30) {
        for (auto &a : *this) {
            a.Praemie = 0;
        }
    }
}

//--------------------------------------------------------------------------------------------
// Fügt eine Reihe von neuen Aufträgen ein:
//--------------------------------------------------------------------------------------------
void CAuftraege::FillForAusland(SLONG CityNum) {

    CalcPlayerMaximums();

    ReSize(6); // ex:10
    FillAlbum();

    SLONG c = 0;
    for (auto &a : *this) {
        if (Sim.Date < 5 && c < 5) {
            a.RefillForAusland(4, CityNum, &Random);
        } else if (Sim.Date < 10 && c < 3) {
            a.RefillForAusland(4, CityNum, &Random);
        } else {
            a.RefillForAusland(c / 2, CityNum, &Random);
        }
        c++;
    }
}

//--------------------------------------------------------------------------------------------
// Fügt einen neuen Auftrag ein:
//--------------------------------------------------------------------------------------------
void CAuftraege::RefillForAusland(SLONG CityNum, SLONG Minimum) {
    SLONG Anz = AnzEntries();

    CalcPlayerMaximums();

    ReSize(6);
    FillAlbum();

    SLONG c = 0;
    for (auto &a : *this) {
        if (Anz <= 0) {
            break;
        }
        if (a.Praemie == 0) {
            if (Sim.Date < 5 && c < 5) {
                a.RefillForAusland(4, CityNum, &Random);
            } else if (Sim.Date < 10 && c < 3) {
                a.RefillForAusland(4, CityNum, &Random);
            } else {
                a.RefillForAusland(c / 2, CityNum, &Random);
            }

            Anz--;
        }
        c++;
    }

    for (auto &a : *this) {
        if (Anz <= 0) {
            break;
        }
        if (a.Praemie != 0) {
            Minimum--;
        }
    }

    c = 0;
    for (auto &a : *this) {
        if (Anz <= 0) {
            break;
        }
        if (a.Praemie == 0 && Minimum > 0) {
            if (Sim.Date < 5 && c < 5) {
                a.RefillForAusland(4, CityNum, &Random);
            } else if (Sim.Date < 10 && c < 3) {
                a.RefillForAusland(4, CityNum, &Random);
            } else {
                a.RefillForAusland(c / 2, CityNum, &Random);
            }

            Minimum--;
        }
        c++;
    }

    AuslandsRefill[CityNum] = 0;
}

//--------------------------------------------------------------------------------------------
// Returns the number of open Order flights which are due today:
//--------------------------------------------------------------------------------------------
SLONG CAuftraege::GetNumDueToday() {
    SLONG c = 0;
    SLONG Anz = 0;

    for (c = 0; c < AnzEntries(); c++) {
        if ((IsInAlbum(c) != 0) && at(c).BisDate >= Sim.Date) {
            if (at(c).InPlan == 0 && at(c).BisDate == Sim.Date) {
                Anz++;
            }
        }
    }

    return (Anz);
}

//--------------------------------------------------------------------------------------------
// Returns the number of open Order flights which still need to be done:
//--------------------------------------------------------------------------------------------
SLONG CAuftraege::GetNumOpen() {
    SLONG c = 0;
    SLONG Anz = 0;

    for (c = 0; c < AnzEntries(); c++) {
        if ((IsInAlbum(c) != 0) && at(c).BisDate >= Sim.Date) {
            if (at(c).InPlan == 0) {
                Anz++;
            }
        }
    }

    return (Anz);
}
