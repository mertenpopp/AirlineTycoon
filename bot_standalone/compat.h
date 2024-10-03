#ifndef COMPAT_H_
#define COMPAT_H_

#include "compat_album.h"
#include "defines.h"

class CPlane;

class CEinheit             // 0 = km
{                          // 1 = km/h
  public:                  // 2 = m
    CString Name{"%s km"}; // 3 = kN
    DOUBLE Faktor{};       // 4 = l
                           // 5 = l/h
  public:                  // 6 = DM
    char *bString(SLONG Value) const;
};

//--------------------------------------------------------------------------------------------
// Die gemieteten Gates: (in Schedule.cpp)
//--------------------------------------------------------------------------------------------
class /**/ CGate {
  public:
    SLONG Nummer{}; // Nummer auf Flughafen (0 bis n)
    SLONG Miete{};  // Monatsmiete in DM
};

class /**/ CGates {
  public:
    BUFFER_V<CGate> Gates;
    BUFFER_V<UBYTE> Auslastung{24 * 7}; // Für 2x alle 24 Stunden der Uhr: Wie viele sind belegt?
    SLONG NumRented{};
};

class CPlaneType {
  public:
    CString Name;              // Der Name des Flugzeuges
    __int64 NotizblockPhoto{}; // Photos für den Notizblock
    SLONG AnzPhotos{};         // Zahl der Photos
    SLONG FirstMissions{};     // Ist erst verfügbar ab Mission x
    SLONG FirstDay{};          //...und auch dort erst ab Tag y

    // Technische Beschreibung
  public:
    CString Hersteller;      // Textstring, z.B. "Boing"
    SLONG Erstbaujahr{};     // Zahl, z.B. 1980
    SLONG Passagiere{};      // Maximale Zahl der Passagiere (ein erste Klasse Passagier verbraucht 2 Plätze)
    SLONG Reichweite{};      // Reichweite in km
    SLONG Geschwindigkeit{}; // in km/h
    SLONG Spannweite{};      // in m
    SLONG Laenge{};          // in m
    SLONG Hoehe{};           // in m
    SLONG Startgewicht{};    // maximales Startgewicht
    CString Triebwerke;      // Als Textstring
    SLONG Schub{};           // in lb
    SLONG AnzPiloten{};      // Piloten und Co-Piloten
    SLONG AnzBegleiter{};    // Zahl der Stewardessen
    SLONG Tankgroesse{};     // Kerosin in l
    SLONG Verbrauch{};       // Kerosin in l/h
    SLONG Preis{};           // Der Neupreis in DM
    FLOAT Wartungsfaktor{};  // Faktor für die Wartungskosten
    CString Kommentar;       // Ggf. allgemeines über diese Maschine
};

class CPlaneTypes : public ALBUM_V<CPlaneType> {
  public:
    CPlaneTypes() : ALBUM_V<CPlaneType>("PlaneTypes") {}
    CPlaneTypes(const CString &TabFilename);
    void ReInit(const CString &TabFilename);
    ULONG GetRandomExistingType(TEAKRAND *pRand);
};

class CAuftrag {
  public:
    ULONG VonCity{0xDEADBEEF};  // bezeichnet eine Stadt
    ULONG NachCity{0xDEADBEEF}; // bezeichnet eine Stadt
    ULONG Personen{};           // So viele müssen in die Maschine passen
    UWORD Date{};               // Vertragsbeginn
    UWORD BisDate{};            // Vertragsende
    SBYTE InPlan{};             // 0=Nix, -1=Durchgeführt, 1=1x im Plan
    SBYTE Okay{};               // 0=Nix, -1=Durchgeführt, 1=1x im Plan
    SLONG Praemie{};            // Prämie bei Erfüllung
    SLONG Strafe{};             // Strafe bei Versagen
    BOOL bUhrigFlight{};        // Von Uhrig in Auftrag gegeben?
    /* types */
    SLONG jobType{-1};
    SLONG jobSizeType{-1};

    CAuftrag() = default;
    CAuftrag(ULONG VonCity, ULONG NachCity, ULONG Personen, UWORD Date, SLONG Praemie, SLONG Strafe);
    CAuftrag(ULONG VonCity, ULONG NachCity, ULONG Personen, UWORD Date);
    CAuftrag(char *VonCity, char *NachCity, ULONG Personen, UWORD Date, SLONG Praemie, SLONG Strafe);
    CAuftrag(char *VonCity, char *NachCity, ULONG Personen, UWORD Date);

    void RandomCities(SLONG AreaType, SLONG HomeCity, TEAKRAND *pRandom);
    void RefillForLastMinute(SLONG AreaType, TEAKRAND *pRandom);
    void RefillForReisebuero(SLONG AreaType, TEAKRAND *pRandom);
    void RefillForBegin(SLONG AreaType, TEAKRAND *pRandom);
    void RefillForAusland(SLONG AreaType, SLONG CityNum, TEAKRAND *pRandom);
    void RefillForUhrig(SLONG AreaType, TEAKRAND *pRandom);
    BOOL FitsInPlane(const CPlane &Plane) const;
};

class CAuftraege : public ALBUM_V<CAuftrag> {
  public:
    TEAKRAND Random;

  public:
    CAuftraege() : ALBUM_V<CAuftrag>("Auftraege") {}
    void FillForLastMinute(void);
    void RefillForLastMinute(SLONG Minimum = 0);
    void FillForReisebuero(void);
    void RefillForReisebuero(SLONG Minimum = 0);
    void FillForAusland(SLONG CityNum);
    void RefillForAusland(SLONG CityNum, SLONG Minimum = 0);
    SLONG GetNumOpen(void);
    SLONG GetNumDueToday(void);
};

class CFracht {
  public:
    ULONG VonCity{};  // bezeichnet eine Stadt
    ULONG NachCity{}; // bezeichnet eine Stadt
    SLONG Tons{};     // So viele Tonnen müssen insgesamt geflogen werden
    SLONG TonsOpen{}; // So viele Tonnen sind weder geflogen, noch im Flugplan verplant
    SLONG TonsLeft{}; // So viele Tonnen müssen noch geflogen werden
    UWORD Date{};     // Vertragsbeginn
    UWORD BisDate{};  // Vertragsende
    SBYTE InPlan{};   // 0=Nix, -1=Durchgeführt, 1=1x im Plan
    SBYTE Okay{};     // 0=Nix, -1=Durchgeführt, 1=1x im Plan
    SLONG Praemie{};  // Prämie bei Erfüllung
    SLONG Strafe{};   // Strafe bei Versagen
    /* types */
    SLONG jobType{-1};
    SLONG jobSizeType{-1};

    CFracht() = default;
    CFracht(ULONG VonCity, ULONG NachCity, SLONG Tons, ULONG Personen, UWORD Date, SLONG Praemie, SLONG Strafe);
    CFracht(ULONG VonCity, ULONG NachCity, SLONG Tons, ULONG Personen, UWORD Date);
    CFracht(char *VonCity, char *NachCity, SLONG Tons, ULONG Personen, UWORD Date, SLONG Praemie, SLONG Strafe);
    CFracht(char *VonCity, char *NachCity, SLONG Tons, ULONG Personen, UWORD Date);

    void RandomCities(SLONG AreaType, SLONG HomeCity, TEAKRAND *pRand);
    void RefillForBegin(SLONG AreaType, TEAKRAND *pRand);
    void Refill(SLONG AreaType, TEAKRAND *pRnd);
    void RefillForAusland(SLONG AreaType, SLONG CityNum, TEAKRAND *pRandom);
    BOOL FitsInPlane(const CPlane &Plane) const;
    BOOL IsValid() const { return Praemie > 0; };
};

class CFrachten : public ALBUM_V<CFracht> {
  public:
    TEAKRAND Random;

  public:
    CFrachten() : ALBUM_V<CFracht>("Fracht") {}

    void Fill(void);
    void Refill(SLONG Minimum = 0);
    SLONG GetNumOpen(void);
    void RefillForAusland(SLONG CityNum, SLONG Minimum = 0);
    void FillForAusland(SLONG CityNum);
    SLONG GetNumDueToday(void);
};

class CRoute {
  public:
    BOOL bNewInDeluxe{}; // wird nicht serialisiert
    SLONG Ebene{};       // 1=fein; 2=grob
    ULONG VonCity{};     // bezeichnet eine Stadt
    ULONG NachCity{};    // bezeichnet eine Stadt
    SLONG Miete{};
    DOUBLE Faktor{}; // Attraktivität der Route
    SLONG Bedarf{};  // Soviele Leute wollen heute fliegen
};

class CRouten : public ALBUM_V<CRoute> {
  public:
    CRouten() : ALBUM_V<CRoute>("Routen") {}
};

class CFlugplanEintrag {
  public:
    UBYTE Okay{};             // 0=Auftrag Okay 1=falscher Tag, 2=schon durchgeführt, 3=Passagiere passen nicht
    UBYTE HoursBefore{};      // So viele Stunden vor dem Start wurde der Flug festgelegt
    UWORD Passagiere{};       // Zahl der belegten Sitzplätze (Normal)
    UWORD PassagiereFC{};     // Zahl der belegten Sitzplätze (in der ersten Klasse)
    UWORD PArrived{};         // Zahl Passagiere, die schon im Flughafen sind
    SLONG Gate{-1};           //-1 = kein Gate frei ==> Flugfeld; -2=externer Hafen
    UBYTE GateWarning{FALSE}; // Warnung, daß ein anderer Flug zu dieser Zeit Probleme macht
    ULONG VonCity{};          // bezeichnet eine Stadt
    ULONG NachCity{};         // bezeichnet eine Stadt
    SLONG Startzeit{};        // Zu diesem Zeitpunkt (0-24h) beginnt dieser Eintrag
    SLONG Landezeit{};        // Zu diesem Zeitpunkt (0-24h) landet das Flugzeug
    SLONG Startdate{};        // Referenz auf Sim.Date
    SLONG Landedate{};        // Referenz auf Sim.Date
    SLONG ObjectType{};       // 0=Nix 1=Route 2=Auftrag 3=Automatik 4=Fracht
    SLONG ObjectId{-1};       // Bezeichnet Auftrag oder -1
    SLONG Ticketpreis{};      // Ticketpreis für Routen
    SLONG TicketpreisFC{};    // Ticketpreis für Routen (Erste Klasse)
    BOOL FlightBooked{FALSE}; // Added: Ensure flight is only "booked" once

    CFlugplanEintrag() = default;
    void CalcPassengers(SLONG PlayerNum, CPlane &qPlane);
    void FlightChanged(void);
    SLONG GetEinnahmen(SLONG PlayerNum, const CPlane &qPlane) const;
};

class CFlugplan {
  public:
    SLONG StartCity{};               // Hier beginnt der Flugplan
    BUFFER_V<CFlugplanEintrag> Flug; // Eine Zeile des Flugplans
    SLONG NextFlight{-1};            // Aktueller; sonst nächster Flug
    SLONG NextStart{-1};             // Verweis auf den nächsten, startenden Flug

  public:
    CFlugplan() {
        Flug.ReSize(10 * 6); // Maximal 10 Flüge pro Tag
        for (SLONG c = Flug.AnzEntries() - 1; c >= 0; c--) {
            Flug[c].ObjectType = 0;
        }
    }
    void UpdateNextFlight(void);
    void UpdateNextStart(void);
};

class CPlane {
  public:
    CString Name{"noname"};                   // Der Name des Flugzeuges
    SLONG Ort{};                              //-1=Landend; -2=Startend; -5 in der Luft; sonst Stadt
    ULONG TypeId{};                           // referenziert CPlaneType oder ist -1
    CFlugplan Flugplan;                       // Der Flugplan
    SLONG Baujahr{2000};                      // Das Baujahr dieses Flugzeuges
    SLONG MaxPassagiere{}, MaxPassagiereFC{}; // Soviele Leute passen bei der derzeiten Konfiguration rein
    SLONG Problem{};                          // 0 oder Anzahl der Stunden bis das Flugzeug kein Problem mehr hat
    SLONG ptPassagiere{};                     // Maximale Zahl der Passagiere (ein erste Klasse Passagier verbraucht 2 Plätze)
    SLONG ptReichweite{};                     // Reichweite in km
    SLONG ptGeschwindigkeit{};                // in km/h
    SLONG ptVerbrauch{};                      // Kerosin in l/h

    CPlane() = default;
    CPlane(const CString &Name, ULONG TypeId, UBYTE Zustand, SLONG Baujahr);
    void CheckFlugplaene(SLONG PlayerNum, BOOL Sort = TRUE, BOOL PlanGates = TRUE);

    BOOL operator>(const CPlane &p) const { return (Name > p.Name); }
    BOOL operator<(const CPlane &p) const { return (Name < p.Name); }
};

class CPlanes : public ALBUM_V<CPlane> {
  public:
    CPlanes() : ALBUM_V<CPlane>("Planes") {}
    BOOL IsPlaneNameInUse(const CString &PlaneName);
};

class CPlaneNames {
  private:
    BUFFER_V<CString> NameBuffer1;
    BUFFER_V<CString> NameBuffer2;

  public:
    CPlaneNames() = default;
    CPlaneNames(const CString &TabFilename);
    void ReInit(const CString &TabFilename);
    CString GetRandom(TEAKRAND *pRnd);
    CString GetUnused(TEAKRAND *pRnd);
};

class CITY {
  public:
    CString Name;          // z.B. "Rio de Janeiro"
    CString Lage;          // z.B. "Südamerika"
    SLONG Areacode{};      // 1=Europa, 2=Amerika, 3=Afrika-Indien, 4=Asien&Ozeanien
    CString Kuerzel;       // aktuelles kuerzel
    CString KuerzelGood;   // z.B. "MOS" f. Moskau
    CString KuerzelReal;   // z.B. "SVO" f. Moskau
    CString Wave;          // Die Wave-Datei
    SLONG TextRes{};       // Base-Ressource Id für Texte
    SLONG AnzTexts{};      // Anzahl der Seiten mit Text
    CString PhotoName;     // Name des Photos auf die Stadt
    SLONG AnzPhotos{};     // Anzahl der Photos (%li muß dann im Namen vorkommen)
    SLONG Einwohner{};     // Die Zahl der Einwohner
    CPoint GlobusPosition; // Die Position auf dem Globus
    CPoint MapPosition;    // Die Position auf der flachen Karte
    SLONG BuroRent{};      // Die Monatsmiete für eine Niederlassung
    SLONG bNewInAddOn{};   // Ist im Add-On neu hinzugekommen?
                           // Vorraussetzung für Anflug
};

class CITIES : public ALBUM_V<CITY> {
  public:
    BUFFER_V<SLONG> HashTable;

  public:
    CITIES() : ALBUM_V<CITY>("Cities") {}
    void ReInit(const CString &TabFilename);
    SLONG CalcDistance(SLONG CityId1, SLONG CityId2);
    SLONG CalcFlugdauer(SLONG CityId1, SLONG CityId2, SLONG Speed);
    SLONG GetRandomUsedIndex(TEAKRAND *pRand = NULL);
    SLONG GetRandomUsedIndex(SLONG AreaCode, TEAKRAND *pRand = NULL);
    ULONG GetIdFromName(const char *Name);
    ULONG GetIdFromNames(const char *Name, ...);
    void UseRealKuerzel(BOOL Real);

  private:
    SLONG HashTableSize{};
};

class PLAYER {
  public:
    SLONG PlayerNum{}; // Seine Nummer
    BOOL IsOut;        // Ist der Spieler aus dem Spiel?
    CString AirlineX;  // Name der Fluglinie; ohne Leerstellen am Ende
    UBYTE Owner{};     // Spieler=0, Computergegner=1, Netzwerkgegner=2

    CPlanes Planes;       // Flugzeuge, die der Spieler besitzt
    CAuftraege Auftraege; // Verträge die er für Flüge abgeschlossen hat
    CFrachten Frachten;   // Verträge die er für Flüge abgeschlossen hat
    CGates Gates;         // Die Gates (immer CheckIn + Abflug) die gemietet wurden

    ULONG BuyPlane(ULONG PlaneTypeId, TEAKRAND *pRnd);
    void UpdateAuftragsUsage(void);
    void UpdateFrachtauftragsUsage(void);
    void PlanGates(void);
};

class PLAYERS {
  public:
    SLONG AnzPlayers;
    BUFFER_V<PLAYER> Players;

    PLAYERS() {
        AnzPlayers = 4;
        Players.ReSize(AnzPlayers);

        for (SLONG c = 0; c < AnzPlayers; c++) {
            Players[c].PlayerNum = c;
        }
        Players[0].AirlineX = "Sunshine Airways";
        Players[1].AirlineX = "Falcon Lines";
        Players[2].AirlineX = "Phoenix Travel";
        Players[3].AirlineX = "Honey Airlines";
    }
    BOOL IsPlaneNameInUse(const CString &PlaneName);
};

struct SIM {
    PLAYERS Players;                 // Die agierenden Personen
    SLONG Date{0};                   // Tage seit Spielbeginn
    ULONG Time{9 * 60000};           // Die Simulationszeit
    SLONG StartWeekday{};            // An diesem Wochentag haben wir das Spiel begonnen
    SLONG Weekday{};                 // 0-6 für Mo-So
    SLONG Kerosin{500};              // Preis pro Liter
    SLONG nSecOutDays{};             // 0 oder n=So viele Tage fällt das Security Office noch aus
    SBYTE Difficulty{DIFF_FREEGAME}; // Schwierigkeitsgrad
    SLONG HomeAirportId{0x1000072};  // Id der Heimatstadt
    SLONG KrisenCity{-1};            // Id der Stadt, wo das Erdbeben ist

    SLONG GetHour() const { return (Time / 60000); }
};

#endif // COMPAT_H_
