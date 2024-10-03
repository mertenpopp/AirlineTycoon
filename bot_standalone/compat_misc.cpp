#include "compat_misc.h"

#include "compat.h"
#include "compat_global.h"

#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <filesystem>
#include <locale>

void DebugBreak() { redprintf("DebugBreak() called!"); }

//--------------------------------------------------------------------------------------------
// Aus Debug.cpp
//--------------------------------------------------------------------------------------------

const char *ExcAssert = "Assert (called from %s:%li) failed!";
const char *ExcGuardian = "Con/Des guardian %lx failed!";
const char *ExcStrangeMem = "Strange new: %li bytes!";
const char *ExcOutOfMem = "Couldn't allocate %li bytes!";
const char *ExcNotImplemented = "Function not implemented!";
const char *ExcImpossible = "Impossible Event %s occured";

void HercPrintf(SLONG /*unused*/, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}

void HercPrintf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}

void HercPrintfRed(const char *format, ...) {
    static SLONG numErrors = 0;
    printf("\e[1;31mERR %d: ", numErrors++);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\e[m\n");
    fflush(stdout);
}

void HercPrintfGreen(const char *format, ...) {
    printf("\e[1;32m");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\e[m\n");
    fflush(stdout);
}

const char *ExcAlbumInsert = "Album: %s += failed!";
const char *ExcAlbumFind = "Album: %s [] failed!";
const char *ExcAlbumDelete = "Album: %s -= failed!";
const char *ExcXIDUnrecoverable = "XID-Access for %li (%s) failed (unrecoverable)!";
const char *ExcAlbumNotConsistent = "Album %s index is corrupted!";
const char *ExcAlbumInvalidArg = "Album: %s swap() called with invalid arg!";

SLONG TeakLibW_Exception(char *file, SLONG line, const char *format, ...) {
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    HercPrintf(1, "====================================================================");
    HercPrintf(0, "Exception in File %s, Line %li:", file, line);
    HercPrintf(0, buffer);
    HercPrintf(1, "====================================================================");
    HercPrintf(0, "C++ Exception thrown. Programm will probably be terminated.");
    throw std::runtime_error(buffer);
}

//--------------------------------------------------------------------------------------------
// Aus Misc.cpp
//--------------------------------------------------------------------------------------------

SLONG ReadLine(BUFFER_V<UBYTE> &Buffer, SLONG BufferStart, char *Line, SLONG LineLength) {
    SLONG c = 0;
    SLONG d = 0;

    for (c = BufferStart; c < Buffer.AnzEntries() && d < LineLength - 1; c++, d++) {
        Line[d] = Buffer[c];
        if (Line[d] == 13 || Line[d] == 10 || Line[d] == 26) {
            if (c + 1 >= Buffer.AnzEntries() || (Buffer[c + 1] != 13 && Buffer[c + 1] != 10 && Buffer[c + 1] != 26)) {
                Line[d + 1] = 0;
                return (c + 1);
            }
        }
    }

    return (c);
}

//--------------------------------------------------------------------------------------------
// Korrigiert die Umlaute wegen dem schönen Windows-Doppelsystem
//--------------------------------------------------------------------------------------------
CString KorrigiereUmlaute(CString &OriginalText) {
    CString rc;
    SLONG c = 0;

    for (c = 0; c < OriginalText.GetLength(); c++) {
        if (OriginalText[c] == '\x84') {
            rc += '\xE4'; // Nicht ändern - das macht so wie es ist schon Sinn!
        } else if (OriginalText[c] == '\x94') {
            rc += '\xF6';
        } else if (OriginalText[c] == '\x81') {
            rc += '\xFC';
        } else if (OriginalText[c] == '\x8E') {
            rc += '\xC4';
        } else if (OriginalText[c] == '\x99') {
            rc += '\xD6';
        } else if (OriginalText[c] == '\x9A') {
            rc += '\xDC';
        } else if (OriginalText[c] == '\xE1') {
            rc += '\xDF';
        } else {
            rc += OriginalText[c];
        }
    }

    return (rc);
}

__int64 StringToInt64(const CString &String) {
    __int64 rc = 0;

    for (SLONG d = 0; d < String.GetLength(); d++) {
        rc += static_cast<__int64>(String[SLONG(d)]) << (8 * d);
    }

    return (rc);
}

CString FullFilename(const CString &Filename, const CString &PathString) {
    CString path;
    CString rc;

    path = PathString;
    if (std::filesystem::path::preferred_separator != '\\') {
        std::replace(path.begin(), path.end(), static_cast<std::filesystem::path::value_type>('\\'), std::filesystem::path::preferred_separator);
    }
    // assert(path[1] == ':');
    return {bprintf(path, Filename.c_str())};
}

char *CEinheit::bString(SLONG Value) const { return (bprintf(Name, (LPCTSTR)Insert1000erDots(Value))); }

CString Insert1000erDots(SLONG Value) {
    short c = 0;
    short d = 0; // Position in neuen bzw. alten String
    short l = 0; // Stringlänge
    short CharsUntilPoint = 0;
    char String[80];
    char Tmp[80];

    static char Dot = 0;

    if (Dot == 0) {
        Dot = std::use_facet<std::moneypunct<char, true>>(std::locale("")).thousands_sep();
        if (Dot != '.' && Dot != ',' && Dot != ':' && Dot != '/' && Dot != '\'' && Dot != '`') {
            Dot = '.';
        }
    }

    sprintf(Tmp, "%i", Value);

    l = short(strlen(Tmp));

    String[0] = Tmp[0];
    c = d = 1;

    CharsUntilPoint = short((l - c) % 3);

    if (Value < 0) {
        CharsUntilPoint = short((l - 1 - c) % 3 + 1);
    }

    for (; d < l; c++, d++) {
        if (CharsUntilPoint == 0 && d < l - 1) {
            String[c] = Dot;
            c++;
            CharsUntilPoint = 3;
        }

        String[c] = Tmp[d];
        CharsUntilPoint--;
    }

    String[c] = '\0';

    return (String);
}

//--------------------------------------------------------------------------------------------
// Berechnet, wieviel ein Flug kostet:
//--------------------------------------------------------------------------------------------
SLONG CalculateFlightKerosin(SLONG VonCity, SLONG NachCity, SLONG Verbrauch, SLONG Geschwindigkeit) {
    SLONG Kerosin = Cities.CalcDistance(VonCity, NachCity) / 1000 // weil Distanz in m übergeben wird
                    * Verbrauch / 160                             // Liter pro Barrel
                    / Geschwindigkeit;

    return (Kerosin);
}

//--------------------------------------------------------------------------------------------
// Berechnet, wieviel ein Flug kostet (ignoriere Kerosin-Tanks)
//--------------------------------------------------------------------------------------------
SLONG CalculateFlightCostNoTank(SLONG VonCity, SLONG NachCity, SLONG Verbrauch, SLONG Geschwindigkeit) {
    SLONG Kerosin = CalculateFlightKerosin(VonCity, NachCity, Verbrauch, Geschwindigkeit);
    SLONG Kosten = Kerosin * Sim.Kerosin;

    if (Kosten < 1000) {
        Kosten = 1000;
    }

    return (Kosten);
}

//--------------------------------------------------------------------------------------------
// Aus Player.cpp
//--------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------
// Der Spieler kauft ein Flugzeug:
//--------------------------------------------------------------------------------------------
ULONG PLAYER::BuyPlane(ULONG PlaneTypeId, TEAKRAND *pRnd) {
    ULONG Id = 0;

    if (Planes.GetNumFree() == 0) {
        Planes.ReSize(Planes.AnzEntries() + 10);
    }
    Id = (Planes += CPlane(PlaneNames.GetUnused(pRnd), PlaneTypeId + 0x10000000, 100, 2002 + (Sim.Date / 365)));

    Planes.Sort();

    return Id;
}

//--------------------------------------------------------------------------------------------
// Welche Aufträge wurden wie oft verplant?:
//--------------------------------------------------------------------------------------------
void PLAYER::UpdateAuftragsUsage() {
    SLONG c = 0;
    SLONG d = 0;

    for (c = Auftraege.AnzEntries() - 1; c >= 0; c--) {
        if (Auftraege.IsInAlbum(c) != 0) {
            Auftraege[c].Okay = 1;

            if (Auftraege[c].InPlan != -1) {
                Auftraege[c].InPlan = 0;
            }
        }
    }

    for (c = 0; c < Planes.AnzEntries(); c++) {
        if (Planes.IsInAlbum(c) != 0) {
            CFlugplan *Plan = &Planes[c].Flugplan;

            for (d = Planes[c].Flugplan.Flug.AnzEntries() - 1; d >= 0; d--) {
                // Nur bei Aufträgen von menschlichen Spielern
                if (Plan->Flug[d].ObjectType == 2 && (Owner == 0)) {
                    // if ((PlaneTypes[Planes[c].TypeId].Passagiere>=SLONG(Auftraege[Plan->Flug[d].ObjectId].Personen) &&
                    // Plan->Flug[d].Startdate<=Auftraege[Plan->Flug[d].ObjectId].BisDate) ||
                    if ((Planes[c].ptPassagiere >= SLONG(Auftraege[Plan->Flug[d].ObjectId].Personen) &&
                         Plan->Flug[d].Startdate <= Auftraege[Plan->Flug[d].ObjectId].BisDate) ||
                        Plan->Flug[d].Startdate > Sim.Date || (Plan->Flug[d].Startdate == Sim.Date && Plan->Flug[d].Startzeit > Sim.GetHour())) {
                        if (Auftraege[Plan->Flug[d].ObjectId].InPlan == 0) {
                            Auftraege[Plan->Flug[d].ObjectId].InPlan = 1;
                            Plan->Flug[d].Okay = 0; // Alles klar
                        }
                    }

                    // if (PlaneTypes[Planes[c].TypeId].Passagiere<SLONG(Auftraege[Plan->Flug[d].ObjectId].Personen))
                    if (Planes[c].ptPassagiere < SLONG(Auftraege[Plan->Flug[d].ObjectId].Personen)) {
                        Auftraege[Plan->Flug[d].ObjectId].Okay = 0;
                        Plan->Flug[d].Okay = 3; // Passagierzahl!
                    }

                    if (Plan->Flug[d].Startdate < Auftraege[Plan->Flug[d].ObjectId].Date ||
                        Plan->Flug[d].Startdate > Auftraege[Plan->Flug[d].ObjectId].BisDate) {
                        Auftraege[Plan->Flug[d].ObjectId].Okay = 0;
                        Plan->Flug[d].Okay = 1; // Falscher Tag!
                    }
                }
                // Frachtaufträge werden hier nicht behandelt
                else if (Plan->Flug[d].ObjectType != 4) {
                    Plan->Flug[d].Okay = 0; // Alles klar
                }
            }
        }
    }
}

//--------------------------------------------------------------------------------------------
// Plant die Gate-Vergabe neu:
//--------------------------------------------------------------------------------------------
// Fliegt ein Flug um 15:00 (heute) ab, dann ist ein Gate von 14:00-15:59 belegt. Geht ein
// Flug (laut Flugplan) bis 15:00, dann kommt er so gegen 14:30 an. Das Gate ist von 14:00 bis
// 14:59 belegt
//--------------------------------------------------------------------------------------------
struct flug_info_t {
    flug_info_t(CFlugplanEintrag *p, SLONG i, bool f) : flug(p), zeit(i), istAbflug(f) {}
    CFlugplanEintrag *flug{nullptr};
    SLONG zeit{};
    bool istAbflug{};
};

void PLAYER::PlanGates() {
    // Alte Gate-Einteilung löschen:
    for (SLONG c = 0; c < 24 * 7; c++) {
        Gates.Auslastung[c] = 0;
    }

    // Alle Flugzeuge durchgehen:
    std::vector<flug_info_t> list;
    for (SLONG c = 0; c < Planes.AnzEntries(); c++) {
        if (Planes.IsInAlbum(c) == 0) {
            continue;
        }

        CFlugplan *Plan = &Planes[c].Flugplan;
        for (SLONG d = Planes[c].Flugplan.Flug.AnzEntries() - 1; d >= 0; d--) {
            Planes[c].Flugplan.Flug[d].GateWarning = FALSE;

            if (Plan->Flug[d].ObjectType != 1 && Plan->Flug[d].ObjectType != 2) {
                continue;
            }

            // Ggf. Gate nehmen:
            if (Plan->Flug[d].VonCity != static_cast<ULONG>(Sim.HomeAirportId) && Plan->Flug[d].NachCity != static_cast<ULONG>(Sim.HomeAirportId)) {
                // Flug hat nichts mit Heimatflughafen zu tun:
                Plan->Flug[d].Gate = -2; // Null Problemo
                continue;
            }

            // Erst einmal mit Default-Wert initialisieren:
            if (Plan->Flug[d].Startdate > Sim.Date || (Plan->Flug[d].Startdate == Sim.Date && Plan->Flug[d].Startzeit > Sim.GetHour() + 1)) {
                Plan->Flug[d].Gate = -1; // Kein Gate frei
            }

            SLONG tmp = 0;
            if (Plan->Flug[d].VonCity == static_cast<ULONG>(Sim.HomeAirportId)) {
                tmp = Plan->Flug[d].Startzeit + (Plan->Flug[d].Startdate - Sim.Date) * 24;
            } else {
                tmp = Plan->Flug[d].Landezeit - 1 + (Plan->Flug[d].Landedate - Sim.Date) * 24;
            }

            if (tmp <= 1 || tmp >= 24 * 7 - 1) {
                continue;
            }

            // Abflug oder Ankunft?
            if (Plan->Flug[d].VonCity == ULONG(Sim.HomeAirportId)) {
                list.emplace_back(&Plan->Flug[d], tmp - 1, true);
            } else // Ankunft!
            {
                list.emplace_back(&Plan->Flug[d], tmp, false);
            }
        }
    }
    std::sort(list.begin(), list.end(), [](const flug_info_t &a, const flug_info_t &b) { return a.zeit < b.zeit; });

    std::vector<std::vector<bool>> auslastungProGate;
    auslastungProGate.resize(Gates.NumRented);
    for (auto &i : auslastungProGate) {
        i.resize(24 * 7, false);
    }

    for (auto &i : list) {
        bool found = false;
        for (SLONG n = 0; n < Gates.NumRented && !found; n++) {
            if (auslastungProGate[n][i.zeit] || (auslastungProGate[n][i.zeit + 1] && i.istAbflug)) {
                continue;
            }

            // Gate wird für Flug veranschlagt:
            i.flug->Gate = Gates.Gates[n].Nummer;

            auslastungProGate[n][i.zeit] = true;
            ++Gates.Auslastung[i.zeit];

            if (i.istAbflug) {
                auslastungProGate[n][i.zeit + 1] = true;
                ++Gates.Auslastung[i.zeit + 1];
            }

            found = true;
        }

        if (!found) {
            // Kein Gate mehr frei:
            i.flug->Gate = -1;

            Gates.Auslastung[i.zeit] = static_cast<UBYTE>(Gates.NumRented + 1);

            if (i.istAbflug) {
                Gates.Auslastung[i.zeit + 1] = static_cast<UBYTE>(Gates.NumRented + 1);
            }

            // Andere Flüge warnen:
            if (Owner != 0) {
                continue;
            }
            for (SLONG e = 0; e < Planes.AnzEntries(); e++) {
                if (Planes.IsInAlbum(e) == 0) {
                    continue;
                }

                CFlugplan &qPlan = Planes[e].Flugplan;
                for (SLONG f = qPlan.Flug.AnzEntries() - 1; f >= 0; f--) {
                    CFlugplanEintrag &qEintrag = qPlan.Flug[f];
                    if (qEintrag.ObjectType != 1 && qEintrag.ObjectType != 2) {
                        continue;
                    }
                    SLONG tmp = i.zeit;
                    if (i.istAbflug) {
                        if (qEintrag.VonCity == ULONG(Sim.HomeAirportId) && abs(qEintrag.Startzeit + (qEintrag.Startdate - Sim.Date) * 24 - tmp) < 2) {
                            qEintrag.GateWarning = TRUE;
                        } else if (qEintrag.NachCity == ULONG(Sim.HomeAirportId) && (qEintrag.Landezeit + (qEintrag.Landedate - Sim.Date) * 24 == tmp ||
                                                                                     qEintrag.Landezeit + (qEintrag.Landedate - Sim.Date) * 24 == tmp + 1)) {
                            qEintrag.GateWarning = TRUE;
                        }
                    } else {
                        if (qEintrag.VonCity == ULONG(Sim.HomeAirportId) && (qEintrag.Startzeit + (qEintrag.Startdate - Sim.Date) * 24 == tmp ||
                                                                             qEintrag.Startzeit + (qEintrag.Startdate - Sim.Date) * 24 == tmp + 1)) {
                            qEintrag.GateWarning = TRUE;
                        } else if (qEintrag.NachCity == ULONG(Sim.HomeAirportId) && qEintrag.Landezeit - 1 + (qEintrag.Landedate - Sim.Date) * 24 == tmp) {
                            qEintrag.GateWarning = TRUE;
                        }
                    }
                }
            }
        }
    }
}

//--------------------------------------------------------------------------------------------
// Welche Frachtaufträge wurden wie oft verplant?:
//--------------------------------------------------------------------------------------------
void PLAYER::UpdateFrachtauftragsUsage() {
    SLONG c = 0;
    SLONG d = 0;

    // Nur bei Aufträgen von menschlichen Spielern
    if (Owner == 1) {
        return;
    }

    // TonsOpen bei allen Frachtaufträge resetten:
    for (c = Frachten.AnzEntries() - 1; c >= 0; c--) {
        if (Frachten.IsInAlbum(c) != 0) {
            Frachten[c].Okay = 1;

            if (Frachten[c].InPlan != -1 && Frachten[c].TonsLeft > 0) {
                Frachten[c].InPlan = 0;
                Frachten[c].TonsOpen = Frachten[c].TonsLeft;
            }
        }
    }

    // TonsOpen bei allen Frachtaufträge neu berechnen:
    for (c = 0; c < Planes.AnzEntries(); c++) {
        if (Planes.IsInAlbum(c) != 0) {
            CFlugplan *Plan = &Planes[c].Flugplan;

            for (d = 0; d < Planes[c].Flugplan.Flug.AnzEntries(); d++) {
                CFlugplanEintrag &qFPE = Plan->Flug[d];

                if (qFPE.ObjectType == 4) {
                    if (qFPE.Startdate < Frachten[qFPE.ObjectId].Date || qFPE.Startdate > Frachten[qFPE.ObjectId].BisDate) {
                        Frachten[qFPE.ObjectId].Okay = 0;
                        Frachten[qFPE.ObjectId].InPlan = 1; // New
                        qFPE.Okay = 1;                      // Falscher Tag!
                    } else if (qFPE.Startdate <= Frachten[qFPE.ObjectId].BisDate || qFPE.Startdate > Sim.Date ||
                               (qFPE.Startdate == Sim.Date && qFPE.Startzeit > Sim.GetHour())) {
                        CFracht &qFracht = Frachten[qFPE.ObjectId];

                        // Ist dieser Frachtflug überhaupt noch zu erledigen?
                        if (qFracht.TonsLeft != 0) {
                            // Wir misbrauchen bei Frachtflügen das Passagierfeld um zu speichern, wieviel Fracht hier mitfliegt
                            qFPE.Passagiere = Planes[c].ptPassagiere / 10;

                            // Flug nur beachten, wenn er noch nicht gestartet ist:
                            // Heute ist Tag 5 15:00
                            // Flug ging an Tag 4 16:00 los. Tag < 5
                            // Flug 2 geht an Tag 5 18:00 los

                            BOOL ignoreFlight = 0;
                            if (qFPE.Startdate < Sim.Date) {
                                ignoreFlight = 1;
                            } // (qFPE.Startzeit==Sim.GetHour() && (Sim.GetHour()<30 || Planes[c].Ort!=-5)))
                            if (qFPE.Startdate == Sim.Date && qFPE.Startzeit < Sim.GetHour()) {
                                ignoreFlight = 1;
                            }

                            if (!static_cast<bool>(ignoreFlight)) {
                                qFracht.TonsOpen -= Planes[c].ptPassagiere / 10;
                            }

                            if (qFracht.TonsOpen <= 0) {
                                qFPE.Passagiere -= UWORD(-qFracht.TonsOpen);

                                qFracht.TonsOpen = 0;
                                qFracht.InPlan = 1;
                                qFPE.Okay = 0; // Alles klar
                            }

                            // Bei Frachten warnen, wenn die Frachtmenge auf 0 schrumpft:
                            if (qFPE.Passagiere == 0) {
                                qFPE.GateWarning = TRUE;
                            }
                        }
                    }
                }
            }
        }
    }
}

//--------------------------------------------------------------------------------------------
// Unterstützen die Roboter im aktuellen die Level ein bestimmtes Feature?:
//--------------------------------------------------------------------------------------------
bool GlobalUse(SLONG FeatureId) {
    switch (FeatureId) {
    case USE_TRAVELHOLDING:
        return (Sim.Difficulty != DIFF_ADDON06);
    default:
        TeakLibW_Exception(FNL, ExcNever);
    }
    return false;
}

//--------------------------------------------------------------------------------------------
// Schaut nach, ob der Flugzeugname schon verwendet wird:
//--------------------------------------------------------------------------------------------
BOOL PLAYERS::IsPlaneNameInUse(const CString &PlaneName) {
    SLONG c = 0;

    for (c = 0; c < AnzPlayers; c++) {
        if (Players[c].Planes.IsPlaneNameInUse(PlaneName) != 0) {
            return (TRUE);
        }
    }

    return (FALSE);
}

//--------------------------------------------------------------------------------------------
// Aus Planetyp.cpp
//--------------------------------------------------------------------------------------------
CPlaneTypes::CPlaneTypes(const CString &TabFilename) : ALBUM_V<CPlaneType>("PlaneTypes") { ReInit(TabFilename); }

void CPlaneTypes::ReInit(const CString &TabFilename) {
    // CStdioFile    Tab;
    BUFFER_V<char> Line(800);
    SLONG Id = 0;

    // Load Table header:
    auto FileData = LoadCompleteFile(FullFilename(TabFilename, ExcelPath));
    SLONG FileP = 0;

    // Die erste Zeile einlesen
    FileP = ReadLine(FileData, FileP, Line.getData(), 800);

    PlaneTypes.ReSize(MAX_PLANETYPES);

    while (true) {
        if (FileP >= FileData.AnzEntries()) {
            break;
        }
        FileP = ReadLine(FileData, FileP, Line.getData(), 800);

        // if (!Tab.ReadString (Line.getData(), 800)) break;
        TeakStrRemoveEndingCodes(Line.getData(), "\xd\xa\x1a\r");

        // Tabellenzeile hinzufügen:
        Id = atol(strtok(Line.getData(), ";\x8\"")) + 0x10000000;

        // Hinzufügen (darf noch nicht existieren):
        if (IsInAlbum(Id) != 0) {
            TeakLibW_Exception(FNL, ExcNever);
        }

        CPlaneType planeType;
        planeType.Hersteller = strtok(nullptr, TabSeparator);
        planeType.Name = strtok(nullptr, TabSeparator);
        planeType.NotizblockPhoto = StringToInt64(strtok(nullptr, TabSeparator));
        planeType.AnzPhotos = atoi(strtok(nullptr, TabSeparator));
        planeType.FirstMissions = atoi(strtok(nullptr, TabSeparator));
        planeType.FirstDay = atoi(strtok(nullptr, TabSeparator));
        planeType.Erstbaujahr = atoi(strtok(nullptr, TabSeparator));
        planeType.Spannweite = atoi(strtok(nullptr, TabSeparator));
        planeType.Laenge = atoi(strtok(nullptr, TabSeparator));
        planeType.Hoehe = atoi(strtok(nullptr, TabSeparator));
        planeType.Startgewicht = atoi(strtok(nullptr, TabSeparator));
        planeType.Passagiere = atoi(strtok(nullptr, TabSeparator));
        planeType.Geschwindigkeit = atoi(strtok(nullptr, TabSeparator));
        planeType.Reichweite = atoi(strtok(nullptr, TabSeparator));
        planeType.Triebwerke = strtok(nullptr, TabSeparator);
        planeType.Schub = atoi(strtok(nullptr, TabSeparator));
        planeType.AnzPiloten = atoi(strtok(nullptr, TabSeparator));
        planeType.AnzBegleiter = atoi(strtok(nullptr, TabSeparator));
        planeType.Tankgroesse = atoi(strtok(nullptr, TabSeparator));
        planeType.Verbrauch = atoi(strtok(nullptr, TabSeparator));
        planeType.Preis = atoi(strtok(nullptr, TabSeparator));
        planeType.Wartungsfaktor = FLOAT(atof(strtok(nullptr, TabSeparator)));
        planeType.Kommentar = strtok(nullptr, TabSeparator);

        push_back(Id, std::move(planeType));
    }
}

CPlane::CPlane(const CString &Name, ULONG TypeId, UBYTE Zustand, SLONG Baujahr) {

    CPlane::Name = Name;
    CPlane::TypeId = TypeId;
    CPlane::Ort = Sim.HomeAirportId;
    CPlane::Problem = 0;

    if (TypeId != -1) {
        CPlane::MaxPassagiere = PlaneTypes[TypeId].Passagiere * 6 / 8;
        CPlane::MaxPassagiereFC = PlaneTypes[TypeId].Passagiere * 1 / 8;

        CPlaneType &qPlaneType = PlaneTypes[TypeId];
        ptReichweite = qPlaneType.Reichweite;
        ptGeschwindigkeit = qPlaneType.Geschwindigkeit;
        ptPassagiere = qPlaneType.Passagiere;
        ptVerbrauch = qPlaneType.Verbrauch;
    }

    CPlane::Flugplan.StartCity = CPlane::Ort;
}

//--------------------------------------------------------------------------------------------
// Überprüft, die Flugpläne für ein Flugzeug und streicht ggf. Flüge
//--------------------------------------------------------------------------------------------
void CPlane::CheckFlugplaene(SLONG PlayerNum, BOOL Sort, BOOL PlanGates) {
    SLONG c = 0;
    SLONG d = 0;

    // Zeitlich sortieren:
    for (c = Flugplan.Flug.AnzEntries() - 1; c > 0; c--) {
        if ((Flugplan.Flug[c - 1].ObjectType == 0 && Flugplan.Flug[c].ObjectType != 0) ||
            ((Sort != 0) && Flugplan.Flug[c - 1].ObjectType != 0 && Flugplan.Flug[c].ObjectType != 0 &&
             Flugplan.Flug[c - 1].Startdate > Flugplan.Flug[c].Startdate) ||
            ((Sort != 0) && Flugplan.Flug[c - 1].ObjectType != 0 && Flugplan.Flug[c].ObjectType != 0 &&
             Flugplan.Flug[c - 1].Startdate == Flugplan.Flug[c].Startdate && Flugplan.Flug[c - 1].Startzeit > Flugplan.Flug[c].Startzeit)) {
            CFlugplanEintrag tmpFPE;

            tmpFPE = Flugplan.Flug[c - 1];
            Flugplan.Flug[c - 1] = Flugplan.Flug[c];
            Flugplan.Flug[c] = tmpFPE;

#ifdef DEMO
            if (Flugplan.Flug[c - 1].Startdate > 109) {
                Flugplan.Flug[c - 1].ObjectId++;
            }
#endif

            c += 2;
            if (c > Flugplan.Flug.AnzEntries()) {
                c = Flugplan.Flug.AnzEntries();
            }
        }
    }

    // Automatikflüge löschen, andere ergänzen:
    for (c = Flugplan.Flug.AnzEntries() - 1; c >= 0; c--) {
        if (Flugplan.Flug[c].Startdate > Sim.Date || (Flugplan.Flug[c].Startdate == Sim.Date && Flugplan.Flug[c].Startzeit > Sim.GetHour() + 1)) {
            if (Flugplan.Flug[c].ObjectType == 3) {
                Flugplan.Flug[c].ObjectType = 0;
            } else if (Flugplan.Flug[c].ObjectType != 0) {
                if (Flugplan.Flug[c].ObjectType == 1) // Typ: Route
                {
                    Flugplan.Flug[c].VonCity = Routen[Flugplan.Flug[c].ObjectId].VonCity;
                    Flugplan.Flug[c].NachCity = Routen[Flugplan.Flug[c].ObjectId].NachCity;
                } else if (Flugplan.Flug[c].ObjectType == 2) // Typ: Auftrag
                {
                    Flugplan.Flug[c].VonCity = Sim.Players.Players[PlayerNum].Auftraege[Flugplan.Flug[c].ObjectId].VonCity;
                    Flugplan.Flug[c].NachCity = Sim.Players.Players[PlayerNum].Auftraege[Flugplan.Flug[c].ObjectId].NachCity;
                } else if (Flugplan.Flug[c].ObjectType == 4) // Typ: Fracht
                {
                    Flugplan.Flug[c].VonCity = Sim.Players.Players[PlayerNum].Frachten[Flugplan.Flug[c].ObjectId].VonCity;
                    Flugplan.Flug[c].NachCity = Sim.Players.Players[PlayerNum].Frachten[Flugplan.Flug[c].ObjectId].NachCity;
                }

                // SLONG Dauer = Cities.CalcFlugdauer (Flugplan.Flug[c].VonCity, Flugplan.Flug[c].NachCity, PlaneTypes[TypeId].Geschwindigkeit);
                SLONG Dauer = Cities.CalcFlugdauer(Flugplan.Flug[c].VonCity, Flugplan.Flug[c].NachCity, ptGeschwindigkeit);
                Flugplan.Flug[c].Landezeit = (Flugplan.Flug[c].Startzeit + Dauer) % 24;

                Flugplan.Flug[c].Landedate = Flugplan.Flug[c].Startdate;
                if (Flugplan.Flug[c].Landezeit < Flugplan.Flug[c].Startzeit) {
                    Flugplan.Flug[c].Landedate++;
                }

                if (Dauer >= 24) {
                    Flugplan.Flug[c].ObjectType = 0;
                    redprintf("Flight takes too long!");
                }
            }
        }
    }

    // Zeitlich sortieren:
    for (c = Flugplan.Flug.AnzEntries() - 1; c > 0; c--) {
        if ((Flugplan.Flug[c - 1].ObjectType == 0 && Flugplan.Flug[c].ObjectType != 0) ||
            ((Sort != 0) && Flugplan.Flug[c - 1].ObjectType != 0 && Flugplan.Flug[c].ObjectType != 0 &&
             Flugplan.Flug[c - 1].Startdate > Flugplan.Flug[c].Startdate) ||
            ((Sort != 0) && Flugplan.Flug[c - 1].ObjectType != 0 && Flugplan.Flug[c].ObjectType != 0 &&
             Flugplan.Flug[c - 1].Startdate == Flugplan.Flug[c].Startdate && Flugplan.Flug[c - 1].Startzeit > Flugplan.Flug[c].Startzeit)) {
            CFlugplanEintrag tmpFPE;

            tmpFPE = Flugplan.Flug[c - 1];
            Flugplan.Flug[c - 1] = Flugplan.Flug[c];
            Flugplan.Flug[c] = tmpFPE;

#ifdef DEMO
            if (Flugplan.Flug[c - 1].Startdate > 39) {
                Flugplan.Flug[c - 1].NachCity++;
            }
#endif

            c += 2;
            if (c > Flugplan.Flug.AnzEntries()) {
                c = Flugplan.Flug.AnzEntries();
            }
        }
    }

    // Nötigenfalls am Anfang automatische Flüge einbauen:
    if (Flugplan.Flug[0].ObjectType != 0 && Cities(Flugplan.Flug[0].VonCity) != static_cast<ULONG>(Cities(Flugplan.StartCity))) {
        // Automatik-Flug einfügen:
        for (d = Flugplan.Flug.AnzEntries() - 1; d > 0; d--) {
            Flugplan.Flug[d] = Flugplan.Flug[d - 1];
        }

        Flugplan.Flug[0].ObjectType = 3;
        Flugplan.Flug[0].VonCity = Flugplan.StartCity;
        Flugplan.Flug[0].NachCity = Flugplan.Flug[1].VonCity;
        Flugplan.Flug[0].Startdate = Sim.Date;
        Flugplan.Flug[0].Startzeit = Sim.GetHour() + 2;

        // SLONG Dauer = Cities.CalcFlugdauer (Flugplan.Flug[0].VonCity, Flugplan.Flug[0].NachCity, PlaneTypes[TypeId].Geschwindigkeit);
        SLONG Dauer = Cities.CalcFlugdauer(Flugplan.Flug[0].VonCity, Flugplan.Flug[0].NachCity, ptGeschwindigkeit);
        Flugplan.Flug[0].Landezeit = (Flugplan.Flug[0].Startzeit + Dauer) % 24;
        Flugplan.Flug[0].Landedate = Flugplan.Flug[0].Startdate + (Flugplan.Flug[0].Startzeit + Dauer) / 24;
    }

    // Nötigenfalls zwischendurch automatische Flüge einbauen:
    for (c = 0; c < Flugplan.Flug.AnzEntries() - 1; c++) {
        if (Flugplan.Flug[c + 1].ObjectType == 0) {
            break;
        }

        SLONG VonCity = Flugplan.Flug[c + 1].VonCity;
        SLONG NachCity = Flugplan.Flug[c].NachCity;

        if (VonCity > 0x01000000) {
            VonCity = Cities(VonCity);
        }
        if (NachCity > 0x01000000) {
            NachCity = Cities(NachCity);
        }

        if (VonCity != NachCity) {
            // Automatik-Flug einfügen:
            for (d = Flugplan.Flug.AnzEntries() - 1; d > c + 1; d--) {
                Flugplan.Flug[d] = Flugplan.Flug[d - 1];
            }

            Flugplan.Flug[c + 1].ObjectType = 3;
            Flugplan.Flug[c + 1].VonCity = Flugplan.Flug[c + 0].NachCity;
            Flugplan.Flug[c + 1].NachCity = Flugplan.Flug[c + 2].VonCity;
            Flugplan.Flug[c + 1].Startzeit = Flugplan.Flug[c + 0].Landezeit;
            Flugplan.Flug[c + 1].Startdate = Flugplan.Flug[c + 0].Landedate;

            // Prüfen, ob Startzeit- und Datum auch in der Zukunft liegen:
            if (Flugplan.Flug[c + 1].Startdate < Sim.Date) {
                Flugplan.Flug[c + 1].Startdate = Sim.Date;
            }
            if (Flugplan.Flug[c + 1].Startdate == Sim.Date && Flugplan.Flug[c + 1].Startzeit <= Sim.GetHour() + 1) {
                Flugplan.Flug[c + 1].Startzeit = Sim.GetHour() + 2;
            }

            // SLONG Dauer = Cities.CalcFlugdauer (Flugplan.Flug[c+1].VonCity, Flugplan.Flug[c+1].NachCity, PlaneTypes[TypeId].Geschwindigkeit);
            SLONG Dauer = Cities.CalcFlugdauer(Flugplan.Flug[c + 1].VonCity, Flugplan.Flug[c + 1].NachCity, ptGeschwindigkeit);
            Flugplan.Flug[c + 1].Landezeit = (Flugplan.Flug[c + 1].Startzeit + Dauer) % 24;
            Flugplan.Flug[c + 1].Landedate = Flugplan.Flug[c + 1].Startdate + (Flugplan.Flug[c + 1].Startzeit + Dauer) / 24;
        }
    }

    // Ggf. verschieben, damit die Zeiten passen:
    for (c = 0; c < Flugplan.Flug.AnzEntries() - 1; c++) {
        if (Flugplan.Flug[c + 1].ObjectType == 0) {
            break;
        }

        SLONG tTime = (Flugplan.Flug[c].Landezeit + 1) % 24;
        SLONG tDate = Flugplan.Flug[c].Landedate + (Flugplan.Flug[c].Landezeit + 1) / 24;

        if (Flugplan.Flug[c + 1].Startdate < tDate) {
            Flugplan.Flug[c + 1].Startdate = tDate;
            Flugplan.Flug[c + 1].Startzeit = tTime;

            // SLONG Dauer = Cities.CalcFlugdauer (Flugplan.Flug[c+1].VonCity, Flugplan.Flug[c+1].NachCity, PlaneTypes[TypeId].Geschwindigkeit);
            SLONG Dauer = Cities.CalcFlugdauer(Flugplan.Flug[c + 1].VonCity, Flugplan.Flug[c + 1].NachCity, ptGeschwindigkeit);
            Flugplan.Flug[c + 1].Landezeit = (Flugplan.Flug[c + 1].Startzeit + Dauer) % 24;
            Flugplan.Flug[c + 1].Landedate = Flugplan.Flug[c + 1].Startdate + (Flugplan.Flug[c + 1].Startzeit + Dauer) / 24;
        } else if (Flugplan.Flug[c + 1].Startdate == tDate && Flugplan.Flug[c + 1].Startzeit < tTime) {
            Flugplan.Flug[c + 1].Startzeit = tTime;

            // SLONG Dauer = Cities.CalcFlugdauer (Flugplan.Flug[c+1].VonCity, Flugplan.Flug[c+1].NachCity, PlaneTypes[TypeId].Geschwindigkeit);
            SLONG Dauer = Cities.CalcFlugdauer(Flugplan.Flug[c + 1].VonCity, Flugplan.Flug[c + 1].NachCity, ptGeschwindigkeit);
            Flugplan.Flug[c + 1].Landezeit = (Flugplan.Flug[c + 1].Startzeit + Dauer) % 24;
            Flugplan.Flug[c + 1].Landedate = Flugplan.Flug[c + 1].Startdate + (Flugplan.Flug[c + 1].Startzeit + Dauer) / 24;
        }
    }

    // Überschüssige Flüge abschneiden:
    for (c = 0; c < Flugplan.Flug.AnzEntries(); c++) {
        if (Flugplan.Flug[c].ObjectType == 0) {
            break;
        }

        if (Flugplan.Flug[c].Startdate >= Sim.Date + 7) {
            Flugplan.Flug[c].ObjectType = 0;
            if (c > 0 && Flugplan.Flug[c - 1].ObjectType == 3) {
                Flugplan.Flug[c - 1].ObjectType = 0;
            }
        }
    }

    // Automatik-Flüge möglichst spät ansetzen:
    for (c = 0; c < Flugplan.Flug.AnzEntries() - 1; c++) {
        if (Flugplan.Flug[c + 1].ObjectType == 0) {
            break;
        }

        if (Flugplan.Flug[c].ObjectType == 3 &&
            (Flugplan.Flug[c].Startdate > Sim.Date || (Flugplan.Flug[c].Startdate == Sim.Date && Flugplan.Flug[c].Startzeit > Sim.GetHour() + 1))) {
            SLONG tTime = (Flugplan.Flug[c + 1].Startzeit - 1 + 24) % 24;
            SLONG tDate = Flugplan.Flug[c + 1].Startdate - static_cast<SLONG>(Flugplan.Flug[c + 1].Startzeit == 0);

            if (Flugplan.Flug[c].Landedate < tDate) {
                Flugplan.Flug[c].Landedate = tDate;
                Flugplan.Flug[c].Landezeit = tTime;

                // SLONG Dauer = Cities.CalcFlugdauer (Flugplan.Flug[c].VonCity, Flugplan.Flug[c].NachCity, PlaneTypes[TypeId].Geschwindigkeit);
                SLONG Dauer = Cities.CalcFlugdauer(Flugplan.Flug[c].VonCity, Flugplan.Flug[c].NachCity, ptGeschwindigkeit);
                Flugplan.Flug[c].Startzeit = (Flugplan.Flug[c].Landezeit - Dauer + 480) % 24;
                Flugplan.Flug[c].Startdate = Flugplan.Flug[c].Landedate + (Flugplan.Flug[c].Landezeit - Dauer - 23) / 24;
            } else if (Flugplan.Flug[c].Landedate == tDate && Flugplan.Flug[c].Landezeit < tTime) {
                Flugplan.Flug[c].Landezeit = tTime;

                // SLONG Dauer = Cities.CalcFlugdauer (Flugplan.Flug[c].VonCity, Flugplan.Flug[c].NachCity, PlaneTypes[TypeId].Geschwindigkeit);
                SLONG Dauer = Cities.CalcFlugdauer(Flugplan.Flug[c].VonCity, Flugplan.Flug[c].NachCity, ptGeschwindigkeit);
                Flugplan.Flug[c].Startzeit = (Flugplan.Flug[c].Landezeit - Dauer + 480) % 24;
                Flugplan.Flug[c].Startdate = Flugplan.Flug[c].Landedate + (Flugplan.Flug[c].Landezeit - Dauer - 23) / 24;
            }
        }
    }

    if (PlanGates != 0) {
        Sim.Players.Players[PlayerNum].PlanGates();
    }
}

BOOL CPlanes::IsPlaneNameInUse(const CString &PlaneName) {
    SLONG c = 0;

    for (c = 0; c < AnzEntries(); c++) {
        if ((IsInAlbum(c) != 0) && at(c).Name == PlaneName) {
            return (TRUE);
        }
    }

    return (FALSE);
}

CPlaneNames::CPlaneNames(const CString &TabFilename) { ReInit(TabFilename); }

void CPlaneNames::ReInit(const CString &TabFilename) {
    // CStdioFile    Tab;
    CString str;
    BUFFER_V<char> Line(300);
    SLONG Anz1 = 0;
    SLONG Anz2 = 0;

    // Load Table header:
    auto FileData = LoadCompleteFile(FullFilename(TabFilename, ExcelPath));
    SLONG FileP = 0;

    // Die erste Zeile einlesen
    FileP = ReadLine(FileData, FileP, Line.getData(), 300);

    NameBuffer1.ReSize(MAX_PNAMES1);
    NameBuffer2.ReSize(MAX_PNAMES1);

    while (true) {
        if (FileP >= FileData.AnzEntries()) {
            break;
        }
        FileP = ReadLine(FileData, FileP, Line.getData(), 300);

        TeakStrRemoveEndingCodes(Line.getData(), "\xd\xa\x1a\r");

        // Tabellenzeile hinzufügen:
        str = strtok(Line.getData(), ";\x8\"");

        if (atoi(strtok(nullptr, TabSeparator)) == 1) {
            if (Anz1 >= MAX_PNAMES1) {
                TeakLibW_Exception(FNL, ExcImpossible, "");
            }
            NameBuffer1[Anz1++] = str;
        } else {
            if (Anz2 >= MAX_PNAMES2) {
                TeakLibW_Exception(FNL, ExcImpossible, "");
            }
            NameBuffer2[Anz2++] = str;
        }
    }

    // Keine freien Plätze offen lassen:
    NameBuffer1.ReSize(Anz1);
    NameBuffer2.ReSize(Anz2);
}

CString CPlaneNames::GetRandom(TEAKRAND *pRnd) {
    if (pRnd != nullptr) {
        return (NameBuffer1[1 + pRnd->Rand((NameBuffer1.AnzEntries() - 1))]);
    }
    return (NameBuffer1[1 + rand() % (NameBuffer1.AnzEntries() - 1)]);
}

CString CPlaneNames::GetUnused(TEAKRAND *pRnd) {
    SLONG c = 0;
    CString Name;

    // Namen erster Qualitätsstufe
    for (c = 0; c < NameBuffer1.AnzEntries(); c++) {
        Name = GetRandom(pRnd);
        if (Sim.Players.IsPlaneNameInUse(Name) == 0) {
            return (Name);
        }
    }

    for (c = 1; c < NameBuffer1.AnzEntries(); c++) {
        if (Sim.Players.IsPlaneNameInUse(NameBuffer1[c]) == 0) {
            return (NameBuffer1[c]);
        }
    }

    // Namen zweiter Qualitätsstufe
    for (c = 0; c < NameBuffer2.AnzEntries(); c++) {
        if (pRnd != nullptr) {
            Name = NameBuffer2[1 + pRnd->Rand((NameBuffer2.AnzEntries() - 1))];
        } else {
            Name = NameBuffer2[1 + rand() % (NameBuffer2.AnzEntries() - 1)];
        }

        if (Sim.Players.IsPlaneNameInUse(Name) == 0) {
            return (Name);
        }
    }

    for (c = 1; c < NameBuffer2.AnzEntries(); c++) {
        if (Sim.Players.IsPlaneNameInUse(NameBuffer2[c]) == 0) {
            return (NameBuffer2[c]);
        }
    }

    return ("ohne Name");
}

//--------------------------------------------------------------------------------------------
// Aus Schedule.cpp
//--------------------------------------------------------------------------------------------

void CFlugplan::UpdateNextFlight() {
    NextFlight = -1;
    for (SLONG e = 0; e < Flug.AnzEntries(); e++) {
        if (NextFlight == -1 && (Flug[e].ObjectType != 0) &&
            (Flug[e].Landedate > Sim.Date || (Flug[e].Landedate == Sim.Date && Flug[e].Landezeit >= Sim.GetHour()))) {
            NextFlight = e;
        }
    }
}

void CFlugplan::UpdateNextStart() {
    NextStart = -1;
    for (SLONG e = 0; e < Flug.AnzEntries(); e++) {
        if ((Flug[e].ObjectType != 0) && (Flug[e].Startdate > Sim.Date || (Flug[e].Startdate == Sim.Date && Flug[e].Startzeit + 1 >= Sim.GetHour()))) {
            NextStart = e;
            break;
        }
    }
}
//--------------------------------------------------------------------------------------------
// Berechnet die Zahl der Passagiere auf dem Flug, wenn der Flug in diesem Augenblick erstellt/
// geändert wird. Kurzfristige Flüge haben weniger passagiere
//--------------------------------------------------------------------------------------------
void CFlugplanEintrag::CalcPassengers(SLONG PlayerNum, CPlane &qPlane) {
    PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

    // Routen automatisch ergänzen:
    if (ObjectType == 1) {
        VonCity = Routen[ObjectId].VonCity;
        NachCity = Routen[ObjectId].NachCity;

        SLONG Dauer = Cities.CalcFlugdauer(VonCity, NachCity, qPlane.ptGeschwindigkeit);
        Landezeit = (Startzeit + Dauer) % 24;
        Landedate = Startdate;
        if (Landezeit < Startzeit) {
            Landedate++;
        }
    }

    if (ObjectType == 2) {
        // Auftrag:
        Passagiere = static_cast<UWORD>(min(qPlane.MaxPassagiere, SLONG(qPlayer.Auftraege[ObjectId].Personen)));

        PassagiereFC = static_cast<UWORD>(qPlayer.Auftraege[ObjectId].Personen - Passagiere);
    }
}

void CFlugplanEintrag::FlightChanged() { HoursBefore = UBYTE((Startdate - Sim.Date) * 24 + (Startzeit - Sim.GetHour())); }

SLONG CFlugplanEintrag::GetEinnahmen(SLONG PlayerNum, const CPlane &qPlane) const {
    switch (ObjectType) {
    // Route:
    case 1:
        return (Ticketpreis * Passagiere + TicketpreisFC * PassagiereFC);
        break;

        // Auftrag:
    case 2:
        return (Sim.Players.Players[PlayerNum].Auftraege[ObjectId].Praemie);
        break;

        // Leerflug:
    case 3:
        return (qPlane.ptPassagiere * Cities.CalcDistance(VonCity, NachCity) / 1000 / 40);
        break;

        // Frachtauftrag:
    case 4:
        return (Sim.Players.Players[PlayerNum].Frachten[ObjectId].Praemie);
        break;

    default: // Eigentlich unmöglich:
        return (0);
    }

    return (0);
}

//--------------------------------------------------------------------------------------------
// Aus City.cpp
//--------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------
// Initialisiert die Städte:
//--------------------------------------------------------------------------------------------
void CITIES::ReInit(const CString &TabFilename) {
    // CStdioFile    Tab;
    BUFFER_V<char> Line(300);
    SLONG Id = 0;
    SLONG Anz = 0;

    // Load Table header:
    auto FileData = LoadCompleteFile(FullFilename(TabFilename, ExcelPath));
    SLONG FileP = 0;

    // Die erste Zeile einlesen
    FileP = ReadLine(FileData, FileP, Line.getData(), 300);

    ReSize(MAX_CITIES);

    while (true) {
        if (FileP >= FileData.AnzEntries()) {
            break;
        }
        FileP = ReadLine(FileData, FileP, Line.getData(), 300);

        // if (!Tab.ReadString (Line.getData(), 300)) break;
        TeakStrRemoveEndingCodes(Line.getData(), "\xd\xa\x1a\r");

        // Tabellenzeile hinzufügen:
        Id = atol(strtok(Line.getData(), ";\x8\"")) + 0x1000000;

        // Hinzufügen (darf noch nicht existieren):
        if (IsInAlbum(Id) != 0) {
            TeakLibW_Exception(FNL, ExcNever);
        }

        CITY city;
        city.Name = strtok(nullptr, TabSeparator);
        city.Lage = strtok(nullptr, TabSeparator);
        city.Areacode = atoi(strtok(nullptr, TabSeparator));
        city.KuerzelGood = strtok(nullptr, TabSeparator);
        city.KuerzelReal = strtok(nullptr, TabSeparator);
        city.Wave = strtok(nullptr, TabSeparator);
        city.TextRes = atoi(strtok(nullptr, TabSeparator));
        city.AnzTexts = atoi(strtok(nullptr, TabSeparator));
        city.PhotoName = strtok(nullptr, TabSeparator);
        city.AnzPhotos = atoi(strtok(nullptr, TabSeparator));
        city.Einwohner = atoi(strtok(nullptr, TabSeparator));
        city.GlobusPosition.x = atoi(strtok(nullptr, TabSeparator));
        city.GlobusPosition.y = atoi(strtok(nullptr, TabSeparator));
        city.MapPosition.x = atoi(strtok(nullptr, TabSeparator));
        city.MapPosition.y = atoi(strtok(nullptr, TabSeparator));
        city.BuroRent = atoi(strtok(nullptr, TabSeparator));
        city.bNewInAddOn = atoi(strtok(nullptr, TabSeparator));

        city.Name = KorrigiereUmlaute(city.Name);

        push_front(Id, std::move(city));

#ifdef DEMO
        city.AnzPhotos = 0; // In der Demo keine Städtebilder
#endif

        Anz++;
    }

    ReSize(Anz);
    HashTable.ReSize(Anz * Anz);
    HashTable.FillWith(0);
    HashTableSize = Anz;

    UseRealKuerzel(true);
}

//--------------------------------------------------------------------------------------------
// Schaltet um zwischen reellen und praktischen Kuerzeln:
//--------------------------------------------------------------------------------------------
void CITIES::UseRealKuerzel(BOOL Real) {
    SLONG c = 0;

    for (c = AnzEntries() - 1; c >= 0; c--) {
        if (IsInAlbum(c) != 0) {
            if (Real != 0) {
                (*this)[c].Kuerzel = (*this)[c].KuerzelReal;
            } else {
                (*this)[c].Kuerzel = (*this)[c].KuerzelGood;
            }
        }
    }
}

//--------------------------------------------------------------------------------------------
// Berechnet die Entfernung zweier beliebiger Städte: (in Meter)
//--------------------------------------------------------------------------------------------
SLONG CITIES::CalcDistance(SLONG CityId1, SLONG CityId2) {
    if (CityId1 >= 0x1000000) {
        CityId1 = (*this)(CityId1);
    }
    if (CityId2 >= 0x1000000) {
        CityId2 = (*this)(CityId2);
    }

    SLONG idx = CityId1 + CityId2 * HashTableSize;
    SLONG rc = HashTable[idx];
    if (rc != 0) {
        return (min(rc, 16440000));
    }

    FXYZ Vector1;      // Vektor vom Erdmittelpunkt zu City1
    FXYZ Vector2;      // Vektor vom Erdmittelpunkt zu City2
    FLOAT Alpha = NAN; // Winkel in Grad zwischen den Vektoren (für Kreissegment)

    // Berechnung des ersten Vektors:
    Vector1.x = static_cast<FLOAT>(cos((*this)[CityId1].GlobusPosition.x * 3.14159 / 180.0));
    Vector1.z = static_cast<FLOAT>(sin((*this)[CityId1].GlobusPosition.x * 3.14159 / 180.0));

    Vector1.y = static_cast<FLOAT>(sin((*this)[CityId1].GlobusPosition.y * 3.14159 / 180.0));

    Vector1.x *= sqrt(1 - Vector1.y * Vector1.y);
    Vector1.z *= sqrt(1 - Vector1.y * Vector1.y);

    // Berechnung des zweiten Vektors:
    Vector2.x = static_cast<FLOAT>(cos((*this)[CityId2].GlobusPosition.x * 3.14159 / 180.0));
    Vector2.z = static_cast<FLOAT>(sin((*this)[CityId2].GlobusPosition.x * 3.14159 / 180.0));

    Vector2.y = static_cast<FLOAT>(sin((*this)[CityId2].GlobusPosition.y * 3.14159 / 180.0));

    Vector2.x *= sqrt(1 - Vector2.y * Vector2.y);
    Vector2.z *= sqrt(1 - Vector2.y * Vector2.y);

    // Berechnung des Winkels zwischen den Vektoren:
    Alpha = static_cast<FLOAT>(acos((Vector1.x * Vector2.x + Vector1.y * Vector2.y + Vector1.z * Vector2.z) / Vector1.abs() / Vector2.abs()) * 180.0 / 3.14159);

    // Berechnung der Länge des Kreissegments: (40040174 = Äquatorumfang)
    rc = SLONG(fabs(Alpha) * 40040174.0 / 360.0);

    HashTable[idx] = rc;

    return (min(rc, 16440000));
}

//--------------------------------------------------------------------------------------------
// Berechnet die Dauer eines Fluges:
//--------------------------------------------------------------------------------------------
SLONG CITIES::CalcFlugdauer(SLONG CityId1, SLONG CityId2, SLONG Speed) {
    SLONG d = (CalcDistance(CityId1, CityId2) / Speed + 999) / 1000 + 1 + 2 - 2;

    if (d < 2) {
        d = 2;
    }

    return (d);
}

//--------------------------------------------------------------------------------------------
// Gibt eine zufällige Stadt zurück:
//--------------------------------------------------------------------------------------------
SLONG CITIES::GetRandomUsedIndex(TEAKRAND *pRand) {
    SLONG c = 0;
    SLONG n = 0;

    for (c = AnzEntries() - 1; c >= 0; c--) {
        if (IsInAlbum(c) != 0) {
            n++;
        }
    }

    if (pRand != nullptr) {
        n = pRand->Rand(n) + 1;
    } else {
        n = (rand() % n) + 1;
    }

    for (c = AnzEntries() - 1; c >= 0; c--) {
        if (IsInAlbum(c) != 0) {
            n--;
            if (n == 0) {
                return (c);
            }
        }
    }

    DebugBreak();
    return (0);
}

//--------------------------------------------------------------------------------------------
// Gibt die Nummer der Stadt mit dem angegebnen Namen zurück:
//--------------------------------------------------------------------------------------------
ULONG CITIES::GetIdFromName(const char *Name) {
    SLONG c = 0;

    for (c = 0; c < AnzEntries(); c++) {
        if ((IsInAlbum(c) != 0) && stricmp(Name, (LPCTSTR)at(c).Name) == 0) {
            return (GetIdFromIndex(c));
        }
    }

    TeakLibW_Exception(FNL, ExcNever);
    return (0);
}

//--------------------------------------------------------------------------------------------
// Gibt die Nummer der Stadt mit dem angegebnen Namen zurück:
//--------------------------------------------------------------------------------------------
ULONG CITIES::GetIdFromNames(const char *Name, ...) {
    SLONG c = 0;

    va_list va_marker;
    LPCTSTR tmp = Name;
    for (va_start(va_marker, Name); tmp != (nullptr); tmp = va_arg(va_marker, LPCTSTR)) {
        for (c = 0; c < AnzEntries(); c++) {
            if ((IsInAlbum(c) != 0) && stricmp(tmp, (LPCTSTR)at(c).Name) == 0) {
                return (GetIdFromIndex(c));
            }
        }
    }

    TeakLibW_Exception(FNL, ExcNever);
    return (0);
}

//--------------------------------------------------------------------------------------------
// Gibt eine zufällige Stadt von diesem AreaCode zurück:
//--------------------------------------------------------------------------------------------
SLONG CITIES::GetRandomUsedIndex(SLONG AreaCode, TEAKRAND *pRand) {
    SLONG c = 0;
    SLONG n = 0;

    for (c = AnzEntries() - 1; c >= 0; c--) {
        if ((IsInAlbum(c) != 0) && (*this)[c].Areacode == AreaCode) {
            n++;
        }
    }

    if (pRand != nullptr) {
        n = pRand->Rand(n) + 1;
    } else {
        n = (rand() % n) + 1;
    }

    for (c = AnzEntries() - 1; c >= 0; c--) {
        if ((IsInAlbum(c) != 0) && (*this)[c].Areacode == AreaCode) {
            n--;
            if (n == 0) {
                return (c);
            }
        }
    }

    DebugBreak();
    return (0);
}
