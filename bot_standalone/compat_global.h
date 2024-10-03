#ifndef GLOBAL_H_
#define GLOBAL_H_

#include "compat.h"
#include "defines.h"

//--------------------------------------------------------------------------------------------
// Zu erst die Ausnahmen:
//--------------------------------------------------------------------------------------------
extern const char ExcNever[];        // Interne Dinge; Sicherheitshalber abgefragt
extern const char ExcCreateWindow[]; // Fenster konnte nicht erzeugt werden

//--------------------------------------------------------------------------------------------
// Einfache globale Variablen:
//--------------------------------------------------------------------------------------------
extern const char TabSeparator[]; // Zum Import der Excel-Tabellen

//--------------------------------------------------------------------------------------------
// Die Pfade der einzelnen Dateigruppen:
//--------------------------------------------------------------------------------------------
extern CString ExcelPath; // Hier sind die csv-Tabellen

//--------------------------------------------------------------------------------------------
// Die Simulationswelt mit ihren Parameter (Zeit, Spieler, Schwierigkeit, ..)
//--------------------------------------------------------------------------------------------
extern SIM Sim;

//--------------------------------------------------------------------------------------------
// Globale Daten und Tabellen:
//--------------------------------------------------------------------------------------------
extern CPlaneTypes PlaneTypes;
extern CITIES Cities;
extern CPlaneNames PlaneNames;
extern CRouten Routen;
extern BUFFER_V<CEinheit> Einheiten;

//--------------------------------------------------------------------------------------------
// Das Inventar und die Zettel vom schwarzen Brett:
//--------------------------------------------------------------------------------------------
extern CAuftraege LastMinuteAuftraege;            // Die hängen gerade aus
extern CAuftraege ReisebueroAuftraege;            // Die hängen gerade aus
extern CFrachten gFrachten;                       // Die Frachtaufträge
extern std::vector<CAuftraege> AuslandsAuftraege; // Aus dem Ausland
extern std::vector<SLONG> AuslandsRefill;         // Aus dem Ausland
extern std::vector<CFrachten> AuslandsFrachten;   // Aus dem Ausland
extern std::vector<SLONG> AuslandsFRefill;        // Aus dem Ausland

//--------------------------------------------------------------------------------------------
// Globals from other AT files
//--------------------------------------------------------------------------------------------

// Öffnungszeiten:
extern SLONG timeDutyOpen;
extern SLONG timeDutyClose;
extern SLONG timeArabOpen;
extern SLONG timeLastClose;
extern SLONG timeMuseOpen;
extern SLONG timeReisClose;
extern SLONG timeMaklClose;
extern SLONG timeWerbOpen;

extern const RES StandardTexte;

void Init(SLONG date);

#endif // GLOBAL_H_
