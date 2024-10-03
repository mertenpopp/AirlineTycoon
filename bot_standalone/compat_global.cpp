#include "compat_global.h"

//--------------------------------------------------------------------------------------------
// Zu erst die Ausnahmen:
//--------------------------------------------------------------------------------------------
const char ExcNever[] = "ExcNever";
const char ExcCreateWindow[] = "CreateWindow failed!";

//--------------------------------------------------------------------------------------------
// Einfache globale Variablen:
//--------------------------------------------------------------------------------------------
const char TabSeparator[] = ";\x8"; // Zum Import der Excel-Tabellen

//--------------------------------------------------------------------------------------------
// Die Pfade der einzelnen Dateigruppen:
//--------------------------------------------------------------------------------------------
CString ExcelPath = ".\\%s"; // Hier sind die csv-Tabellen

//--------------------------------------------------------------------------------------------
// Die Simulationswelt mit ihren Parameter (Zeit, Spieler, Schwierigkeit, ..)
//--------------------------------------------------------------------------------------------
SIM Sim;

//--------------------------------------------------------------------------------------------
// Globale Daten und Tabellen:
//--------------------------------------------------------------------------------------------
CPlaneTypes PlaneTypes;
CITIES Cities;
CPlaneNames PlaneNames;
CRouten Routen;
BUFFER_V<CEinheit> Einheiten(1);

//--------------------------------------------------------------------------------------------
// Das Inventar und das schwarze Brett:
//--------------------------------------------------------------------------------------------
CAuftraege LastMinuteAuftraege;            // Die hängen gerade aus
CAuftraege ReisebueroAuftraege;            // Die hängen gerade aus
CFrachten gFrachten;                       // Die Frachtaufträge
std::vector<CAuftraege> AuslandsAuftraege; // Aus dem Ausland
std::vector<SLONG> AuslandsRefill;         // Aus dem Ausland
std::vector<CFrachten> AuslandsFrachten;   // Aus dem Ausland
std::vector<SLONG> AuslandsFRefill;        // Aus dem Ausland

//--------------------------------------------------------------------------------------------
// Globals from other AT files
//--------------------------------------------------------------------------------------------

// Öfnungszeiten:
SLONG timeDutyOpen = 10 * 60000;
SLONG timeDutyClose = 16 * 60000; // Nur Sa, So
SLONG timeArabOpen = 10 * 60000;
SLONG timeLastClose = 16 * 60000;
SLONG timeMuseOpen = 11 * 60000;
SLONG timeReisClose = 17 * 60000;
SLONG timeMaklClose = 16 * 60000;
SLONG timeWerbOpen = 12 * 60000;

const RES StandardTexte{};

void Init(SLONG date) {
    Sim = {};

    Sim.Date = date;

    PlaneTypes = {};
    Cities = {};
    PlaneNames = {};
    Routen = {};

    LastMinuteAuftraege = {};
    ReisebueroAuftraege = {};
    gFrachten = {};
    AuslandsAuftraege = {};
    AuslandsRefill = {};
    AuslandsFrachten = {};
    AuslandsFRefill = {};

    Cities.ReInit("city.csv");
    AuslandsAuftraege.resize(MAX_CITIES);
    AuslandsRefill.resize(MAX_CITIES);
    AuslandsFrachten.resize(MAX_CITIES);
    AuslandsFRefill.resize(MAX_CITIES);

    PlaneTypes.ReInit("planetyp.csv");
    PlaneNames.ReInit("pnames.csv");

    gFrachten.Fill();
    LastMinuteAuftraege.FillForLastMinute();
    ReisebueroAuftraege.FillForReisebuero();

    for (SLONG c = 0; c < SLONG(Cities.AnzEntries()); c++) {
        AuslandsRefill[c] = 6;
        AuslandsAuftraege[c].Random.SRand(Sim.Date + c + 3);
        AuslandsAuftraege[c].FillForAusland(c);
        AuslandsRefill[c] = 6;

        AuslandsFRefill[c] = 6;
        AuslandsFrachten[c].Random.SRand(Sim.Date + c + 3);
        AuslandsFrachten[c].FillForAusland(c);
        AuslandsFRefill[c] = 6;
    }
}
