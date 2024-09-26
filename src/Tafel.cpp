//============================================================================================
// Tafel.cpp : Das schwarze Brett der Flugaufsicht
//============================================================================================
// Link: "Tafel.h"
//============================================================================================
#include "Tafel.h" //Die Tafel bei der Flughafenaufsicht

#include "AtNet.h"
#include "global.h"
#include "gltafel.h"
#include "Proto.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define AT_Error(...) Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_ERROR, "Tafel", __VA_ARGS__)
#define AT_Warn(...) Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_WARN, "Tafel", __VA_ARGS__)
#define AT_Info(...) Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_INFO, "Tafel", __VA_ARGS__)
#define AT_Log(...) AT_Log_I("Tafel", __VA_ARGS__)

std::vector<XY> AvailablePositions = {XY(005, 7),   XY(100, 25),  XY(200, 10),  XY(300, 36),  XY(400, 25),  XY(500, 31),  // First line
                                      XY(000, 115), XY(95, 131),  XY(195, 124), XY(285, 146), XY(385, 140), XY(490, 168), // Second line
                                      XY(10, 245),  XY(120, 234), XY(224, 261), XY(327, 255), XY(440, 290)};

// Daten des aktuellen Savegames beim laden:
extern SLONG SaveVersion;
extern SLONG SaveVersionSub;

//--------------------------------------------------------------------------------------------
// Konstruktor
//--------------------------------------------------------------------------------------------
CTafel::CTafel(BOOL bHandy, ULONG PlayerNum) : CStdRaum(bHandy, PlayerNum, "tafel.gli", GFX_TAFEL) {
    ReloadBitmaps();

    KommVar = -1;

    // Make a copy of pointed objects
    for (CTafelZettel *ptr : TafelData.ByPositions) {
        LastTafelData.push_back(*ptr);
    }

    LeereZettelBms.ReSize(pRoomLib, "ZETTEL04", 3);
    PostcardBm.ReSize(pRoomLib, "NOCARD");

    for (SLONG c = 0; c < TafelData.ByPositions.size(); c++) {
        RepaintZettel(c);
    }

    SP_Fliege.ReSize(5);
    SP_Fliege.Clips[0].ReSize(0, "flyw01.smk", "", XY(554, 253), SPM_IDLE, CRepeat(5, 9), CPostWait(0, 0), SMACKER_CLIP_CANCANCEL, nullptr, SMACKER_CLIP_SET, 0,
                              &KommVar, "A9A9", 0, 1);
    SP_Fliege.Clips[1].ReSize(1, "flyw02.smk", "", XY(554, 253), SPM_IDLE, CRepeat(6, 9), CPostWait(0, 0), SMACKER_CLIP_CANCANCEL, nullptr, SMACKER_CLIP_SET, 0,
                              &KommVar, "A9A9", 0, 1);
    SP_Fliege.Clips[2].ReSize(2, "fly.smk", "fly.raw", XY(554, 253), SPM_IDLE, CRepeat(1, 1), CPostWait(0, 0), SMACKER_CLIP_DONTCANCEL, nullptr,
                              SMACKER_CLIP_SET, 0, nullptr, "A9", 3);
    SP_Fliege.Clips[3].ReSize(3, "flyout.smk", "", XY(554, 253), SPM_IDLE, CRepeat(9, 9), CPostWait(200, 600), SMACKER_CLIP_DONTCANCEL, nullptr,
                              SMACKER_CLIP_SET, 0, nullptr, "A9", 4);
    SP_Fliege.Clips[4].ReSize(4, "flyb.smk", "flyb.raw", XY(554, 253), SPM_IDLE, CRepeat(1, 1), CPostWait(0, 0), SMACKER_CLIP_DONTCANCEL, &KommVar,
                              SMACKER_CLIP_SET | SMACKER_CLIP_PRE, -1, nullptr, "A9", 0);
}

//--------------------------------------------------------------------------------------------
// Destruktor:
//--------------------------------------------------------------------------------------------
CTafel::~CTafel() {
    LeereZettelBms.Destroy();
    PostcardBm.Destroy();
    Sim.Players.Players[PlayerNum].Messages.AddMessage(BERATERTYP_ROUTE, "", MESSAGE_COMMENT);

    TEAKFILE Message;

    Message.Announce(1024);

    Message << ATNET_TAKE_CITY;

    for (SLONG c = 0; c < 7; c++) {
        Message << TafelData.City[c].Player << TafelData.City[c].Preis;
        Message << TafelData.Gate[c].Player << TafelData.Gate[c].Preis;
    }

    SIM::SendMemFile(Message);
}

//--------------------------------------------------------------------------------------------
// Bitmaps reloaden:
//--------------------------------------------------------------------------------------------
void CTafel::ReloadBitmaps() {}

//////////////////////////////////////////////////////////////////////////////////////////////
// CTafel message handlers
//////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------
// void CTafel::OnPaint()
//--------------------------------------------------------------------------------------------
void CTafel::OnPaint() {
    SLONG c = 0;
    BOOL OnTip = FALSE;
    PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

    if (bHandy == 0) {
        SetMouseLook(CURSOR_NORMAL, 0, ROOM_ARAB_AIR, 0);
    }

    // Die Standard Paint-Sachen kann der Basisraum erledigen
    CStdRaum::OnPaint();

    if (!((Sim.ItemPostcard != 0) && qPlayer.SeligTrust == 0 && Sim.Difficulty != DIFF_TUTORIAL)) {
        RoomBm.BlitFromT(PostcardBm, 16, 290);
    }

    for (c = 0; c < TafelData.ByPositions.size(); c++) {
        CTafelZettel *entry = TafelData.ByPositions[c];
        if (entry->Type == CTafelZettel::Type::ROUTE) {
            continue;
        }

        if (RoomBm.BlitFromT(ZettelBms[c], entry->Position.x, entry->Position.y) == 0) {
            RepaintZettel(c);
            RoomBm.BlitFromT(ZettelBms[c], entry->Position.x, entry->Position.y);
        }
    }

    if (OnTip == FALSE) {
        LastTip = -1;
    }

    // Ggf. Onscreen-Texte einbauen:
    CStdRaum::InitToolTips();

    SP_Fliege.Pump();
    SP_Fliege.BlitAtT(RoomBm);

    if ((IsDialogOpen() == 0) && (MenuIsOpen() == 0)) {
        if (gMousePosition.IfIsWithin(580, 369, 640, 423)) {
            SetMouseLook(CURSOR_EXIT, 0, ROOM_TAFEL, 999);
        }
        if (gMousePosition.IfIsWithin(500, 280, 640, 424)) {
            KommVar = 2;
        }

        if ((Sim.ItemPostcard != 0) && qPlayer.SeligTrust == 0 && Sim.Difficulty != DIFF_TUTORIAL) {
            if (gMousePosition.IfIsWithin(25, 317, 188, 410) || gMousePosition.IfIsWithin(116, 299, 182, 334) || gMousePosition.IfIsWithin(37, 385, 116, 425)) {
                SetMouseLook(CURSOR_HOT, 0, ROOM_TAFEL, 800);
            }
        }

        if (!(MouseClickArea == ROOM_TAFEL && MouseClickId == 999)) {
            for (c = 0; c < TafelData.ByPositions.size(); c++) {
                if (gMousePosition.y > 440) {
                    continue;
                }
                CTafelZettel *entry = TafelData.ByPositions[c];
                if (XY(gMousePosition)
                        .IfIsWithin(entry->Position.x, entry->Position.y, entry->Position.x + LeereZettelBms[c % 3].Size.x,
                                    entry->Position.y + LeereZettelBms[c % 3].Size.y)) {
                    if (entry->Type == CTafelZettel::Type::CITY && entry->Player != PlayerNum &&
                        Sim.Players.Players[Sim.localPlayer].RentCities.RentCities[entry->ZettelId].Rang == 0) {
                        SetMouseLook(CURSOR_HOT, 0, ROOM_TAFEL, 0);
                    } else if (entry->Type == CTafelZettel::Type::GATE && entry->Player != PlayerNum) {
                        SetMouseLook(CURSOR_HOT, 0, ROOM_TAFEL, 0);
                    }
                }
            }
        }
    }

    CStdRaum::PostPaint();
    CStdRaum::PumpToolTips();
}

//--------------------------------------------------------------------------------------------
// void CTafel::OnPaint()
//--------------------------------------------------------------------------------------------
void CTafel::RepaintZettel(SLONG n) {
    CTafelZettel *entry = TafelData.ByPositions[n];
    SBBM &LeereZettel = LeereZettelBms[n % 3];

    if (entry->Type == CTafelZettel::Type::ROUTE) {
        if (entry->ZettelId <= 0) {
            ZettelBms[n].Destroy();
        } else {
            ZettelBms[n].ReSize(LeereZettel.Size);
            ZettelBms[n].BlitFrom(LeereZettel);

            ZettelBms[n].PrintAt(
                bprintf("%s-%s", (LPCTSTR)Cities[Routen[entry->ZettelId].VonCity].Kuerzel, (LPCTSTR)Cities[Routen[entry->ZettelId].NachCity].Kuerzel),
                FontSmallBlack, TEC_FONT_CENTERED, XY(3, 30), XY(ZettelBms[n].Size.x - 3, 202));

            ZettelBms[n].PrintAt(Cities[Routen[entry->ZettelId].VonCity].Name, FontSmallBlack, TEC_FONT_CENTERED, XY(3, 34 + 15),
                                 XY(ZettelBms[n].Size.x - 3, 202));
            ZettelBms[n].PrintAt("-", FontSmallBlack, TEC_FONT_CENTERED, XY(3, 45 + 10), XY(ZettelBms[n].Size.x - 3, 202));
            ZettelBms[n].PrintAt(Cities[Routen[entry->ZettelId].NachCity].Name, FontSmallBlack, TEC_FONT_CENTERED, XY(3, 56 + 6),
                                 XY(ZettelBms[n].Size.x - 3, 202));

            // Bisheriger Höchstbieter & Gebot:
            if (entry->Player != -1) {
                ZettelBms[n].PrintAt(Sim.Players.Players[entry->Player].Airline, FontSmallBlack, TEC_FONT_CENTERED, XY(3, 72 + 30),
                                     XY(ZettelBms[n].Size.x - 3, 202));
            }
            ZettelBms[n].PrintAt(Einheiten[EINH_DM].bString(entry->Preis), FontSmallBlack, TEC_FONT_CENTERED, XY(3, 95 + 20), XY(ZettelBms[n].Size.x - 3, 202));
        }
    } else if (entry->Type == CTafelZettel::Type::CITY) {
        if (entry->ZettelId <= -1) {
            ZettelBms[n].Destroy();
        } else {
            ZettelBms[n].ReSize(LeereZettel.Size);
            ZettelBms[n].BlitFrom(LeereZettel);

            ZettelBms[n].PrintAt(StandardTexte.GetS(TOKEN_MISC, 2501), FontSmallBlack, TEC_FONT_CENTERED, XY(3, 28 + 8), XY(ZettelBms[n].Size.x - 3, 132));
            ZettelBms[n].PrintAt(Cities[entry->ZettelId].Name, FontSmallBlack, TEC_FONT_CENTERED, XY(3, 28 + 30), XY(ZettelBms[n].Size.x - 3, 132));

            // Bisheriger Höchstbieter & Gebot:
            if (entry->Player != -1) {
                ZettelBms[n].PrintAt(Sim.Players.Players[entry->Player].Airline, FontSmallBlack, TEC_FONT_LEFT, XY(13, 55 + 30),
                                     XY(ZettelBms[n].Size.x - 3, 132));
            }
            ZettelBms[n].PrintAt(Einheiten[EINH_DM].bString(entry->Preis), FontSmallBlack, TEC_FONT_LEFT, XY(13, 70 + 30),
                                      XY(ZettelBms[n].Size.x - 3, 132));
        }
    } else if (entry->Type == CTafelZettel::Type::GATE) {
        if (entry->ZettelId <= -1) {
            ZettelBms[n].Destroy();
        } else {
            ZettelBms[n].ReSize(LeereZettel.Size);
            ZettelBms[n].BlitFrom(LeereZettel);

            ZettelBms[n].PrintAt(bprintf(StandardTexte.GetS(TOKEN_MISC, 2500), entry->ZettelId + 1), FontSmallBlack, TEC_FONT_CENTERED, XY(3, 18 + 15),
                                 XY(ZettelBms[n].Size.x - 3, 102));

            // Bisheriger Höchstbieter & Gebot:
            if (entry->Player != -1) {
                ZettelBms[n].PrintAt(Sim.Players.Players[entry->Player].Airline, FontSmallBlack, TEC_FONT_LEFT, XY(13, 45 + 30),
                                     XY(ZettelBms[n].Size.x - 3, 202));
            }
            ZettelBms[n].PrintAt(Einheiten[EINH_DM].bString(entry->Preis), FontSmallBlack, TEC_FONT_LEFT, XY(13, 60 + 30), XY(ZettelBms[n].Size.x - 3, 202));
        }
    }
}

//--------------------------------------------------------------------------------------------
// void CTafel::OnLButtonDown(UINT nFlags, CPoint point)
//--------------------------------------------------------------------------------------------
void CTafel::OnLButtonDown(UINT nFlags, CPoint point) {
    SLONG c = 0;
    XY RoomPos;
    PLAYER &qPlayer = Sim.Players.Players[PlayerNum];

    DefaultOnLButtonDown();

    if (ConvertMousePosition(point, &RoomPos) == 0) {
        CStdRaum::OnLButtonDown(nFlags, point);
        return;
    }

    if (PreLButtonDown(point) == 0) {
        if (MouseClickArea == ROOM_TAFEL && MouseClickId == 999) {
            qPlayer.LeaveRoom();
        } else if (MouseClickArea == ROOM_TAFEL && MouseClickId == 800 && (Sim.ItemPostcard != 0)) {
            if (qPlayer.HasSpaceForItem() != 0) {
                qPlayer.BuyItem(ITEM_POSTKARTE);

                if (qPlayer.HasItem(ITEM_POSTKARTE) != 0) {
                    Sim.ItemPostcard = 0;
                    SIM::SendSimpleMessage(ATNET_TAKETHING, 0, ITEM_POSTKARTE);
                }
            }
        } else {
            // Auf einen der Zettel geklickt?
            // We reverse the array to alway pick the lastest drawn post-it in case of overlap
            if (point.y < 440) {
                for (c = TafelData.ByPositions.size() - 1; c >= 0; c--) {
                    CTafelZettel *entry = TafelData.ByPositions[c];
                    if (XY(point).IfIsWithin(entry->Position.x, entry->Position.y, entry->Position.x + LeereZettelBms[c % 3].Size.x,
                                             entry->Position.y + LeereZettelBms[c % 3].Size.y)) {

                        if (entry->Player != LastTafelData[c].Player) {
                            // Undo auction
                            entry->Player = LastTafelData[c].Player;
                            entry->Preis = LastTafelData[c].Preis;
                            entry->WasInterested = LastTafelData[c].WasInterested;
                        } else if (entry->Type == CTafelZettel::Type::CITY && entry->Player != PlayerNum &&
                                   Sim.Players.Players[Sim.localPlayer].RentCities.RentCities[entry->ZettelId].Rang == 0) {
                            if ((Sim.bNetwork != 0) && entry->Player != -1 && Sim.Players.Players[entry->Player].Owner == 2) {
                                SIM::SendSimpleMessage(ATNET_ADVISOR, Sim.Players.Players[entry->Player].NetworkID, 0, PlayerNum, c);
                            }

                            entry->Preis += entry->Preis / 10;
                            entry->Player = PlayerNum;
                            entry->WasInterested = TRUE;
                        } else if (entry->Type == CTafelZettel::Type::GATE && entry->Player != PlayerNum) {
                            if ((Sim.bNetwork != 0) && entry->Player != -1 && Sim.Players.Players[entry->Player].Owner == 2) {
                                SIM::SendSimpleMessage(ATNET_ADVISOR, Sim.Players.Players[entry->Player].NetworkID, 0, PlayerNum, c);
                            }

                            entry->Preis += entry->Preis / 10;
                            entry->Player = PlayerNum;
                            entry->WasInterested = TRUE;
                        }
                        RepaintZettel(c);
                        return;
                    }
                }
            }
            CStdRaum::OnLButtonDown(nFlags, point);
        }
    }
}

//--------------------------------------------------------------------------------------------
// void CTafel::OnRButtonDown(UINT nFlags, CPoint point)
//--------------------------------------------------------------------------------------------
void CTafel::OnRButtonDown(UINT nFlags, CPoint point) {
    DefaultOnRButtonDown();

    // Außerhalb geklickt? Dann Default-Handler!
    if (point.x < WinP1.x || point.y < WinP1.y || point.x > WinP2.x || point.y > WinP2.y) {
        return;
    }

    if (MenuIsOpen() != 0) {
        MenuRightClick(point);
    } else {
        if ((IsDialogOpen() == 0) && point.y < 440) {
            Sim.Players.Players[PlayerNum].LeaveRoom();
        }
        CStdRaum::OnRButtonDown(nFlags, point);
    }
}

//--------------------------------------------------------------------------------------------
// CTafelData::
//--------------------------------------------------------------------------------------------
// Daten alle löschen ==> keine Zettel
//--------------------------------------------------------------------------------------------
void CTafelData::Clear() {
    SLONG c = 0;

    for (c = 0; c < 7; c++) {
        Route[c].ZettelId = 0;
        City[c].ZettelId = -1;
        Gate[c].ZettelId = -1;
        Route[c].WasInterested = FALSE;
        City[c].WasInterested = FALSE;
        Gate[c].WasInterested = FALSE;
    }
}

//--------------------------------------------------------------------------------------------
// Zettel für n Tage auslosen:
//--------------------------------------------------------------------------------------------
void CTafelData::Randomize(SLONG Day) {
    SLONG c = 0;
    SLONG d = 0;
    SLONG e = 0;
    SLONG f = 0;
    SLONG Anz = 0;

    std::vector<ULONG> citiesToExclude;
    SLONG NumGates = 0;

    for (c = 0; c < 7; c++) {
        City[c].ZettelId = -1;
        Gate[c].ZettelId = -1;
        Route[c].Player = -1;
        City[c].Player = -1;
        Gate[c].Player = -1;
        Route[c].WasInterested = FALSE;
        City[c].WasInterested = FALSE;
        Gate[c].WasInterested = FALSE;
    }

    if (Sim.Difficulty == DIFF_NORMAL) {
        citiesToExclude.resize(7);
        citiesToExclude[0] = Cities(Sim.HomeAirportId);
        citiesToExclude[1] = Cities(Sim.MissionCities[0]);
        citiesToExclude[2] = Cities(Sim.MissionCities[1]);
        citiesToExclude[3] = Cities(Sim.MissionCities[2]);
        citiesToExclude[4] = Cities(Sim.MissionCities[3]);
        citiesToExclude[5] = Cities(Sim.MissionCities[4]);
        citiesToExclude[6] = Cities(Sim.MissionCities[5]);
    }

    TEAKRAND localRand(Sim.Date + Sim.StartTime);

    if (GlobalUse(USE_TRAVELHOLDING)) {
        ULONG maxCities = Sim.Options.OptionRentOfficeMaxAvailable;
        if (Sim.Difficulty == DIFF_NORMAL && maxCities < 4) {
            maxCities = 4;
        }

        SLONG numTries = min(Day, 10);
        if (Sim.Difficulty == DIFF_NORMAL && numTries < 10) {
            numTries = 10;
        }

        std::vector<CTafelZettel> availableCities;
        GetAvailableCities(availableCities, citiesToExclude);

        ULONG citiesToPick = 0;
        for (c = 0; c < numTries; c++) {
            BOOL shouldRun = localRand.Rand(100) < Sim.Options.OptionRentOfficeTriggerPercent;
            // Should we consider adding a city to the board through pseudo-RNG ?
            if (!shouldRun || citiesToPick >= availableCities.size() || citiesToPick >= maxCities) {
                continue;
            }

            citiesToPick += 1;
        }

        // Players might set a min value
        citiesToPick = std::max(citiesToPick, Sim.Options.OptionRentOfficeMinAvailable);
        // Also, we don't want to set more than the availableCities or more than 7 (because UI doesn't manage it yet).
        citiesToPick = std::min({citiesToPick, (ULONG)availableCities.size(), (ULONG)7U});

        AT_Log("citiesToPick = %d (numTries = %d), availableCities = %d", citiesToPick, numTries, availableCities.size());

        for (SLONG i = 0; i < citiesToPick; i++) {
            // Take and remove the first city
            // availableCities is randomized so it should work just fine.
            if (availableCities.empty()) {
                break;
            }
            auto city = availableCities.front();
            availableCities.erase(availableCities.begin());

            // Add to our city array
            City[i].ZettelId = city.ZettelId;
            City[i].Player = city.Player;
            City[i].Preis = city.Preis;
            City[i].Rang = city.Rang;
        }
    }

    // Und zu letzt die Gates:
    Anz = Airport.GetNumberOfShops(RUNE_2WAIT);
    if (Sim.Date == 0) {
        Anz = 4;
    }

    for (c = f = 0; c < Anz && f < 7; c++) {
        BOOL Found = FALSE;

        for (d = 0; d < Sim.Players.Players.AnzEntries(); d++) {
            if (Sim.Players.Players[d].IsOut == 0) {
                for (e = 0; e < Sim.Players.Players[d].Gates.Gates.AnzEntries(); e++) {
                    if (Sim.Players.Players[d].Gates.Gates[e].Miete != -1 && Sim.Players.Players[d].Gates.Gates[e].Nummer == c) {
                        Found = TRUE;
                    }
                }
            }
        }

        if (Found == FALSE) {
            Gate[NumGates].ZettelId = c;
            Gate[NumGates].Player = -1;
            Gate[NumGates].Preis = 3000 - c * 100;
            Gate[NumGates].Rang = 0;
            f++;

            NumGates++;
        }
    }

    AssignPositions();
}

void CTafelData::GetAvailableCities(std::vector<CTafelZettel> &results, const std::vector<ULONG> &excluded) {
    SLONG cityIndex = 0;
    results.clear();

    for (cityIndex = 0; cityIndex < Cities.AnzEntries(); cityIndex++) {
        SLONG PlayerIndex = 0;
        SLONG PlayerUsed = 0;
        for (PlayerIndex = 0; PlayerIndex < Sim.Players.AnzPlayers; PlayerIndex++) {
            if (Sim.Players.Players[PlayerIndex].RentCities.RentCities[cityIndex].Rang != 0U) {
                PlayerUsed++;
            }
        }

        // Do not include if already used by more than 2 players
        if (PlayerUsed >= 3) {
            continue;
        }

        // Exclude the city if the ID is in the excluded array
        if (std::find(excluded.begin(), excluded.end(), cityIndex) != excluded.end()) {
            continue;
        }

        CTafelZettel taffel;
        taffel.ZettelId = cityIndex;
        taffel.Preis = ::Cities[cityIndex].BuroRent;
        taffel.Player = -1;
        taffel.Rang = PlayerUsed + 1;
        results.push_back(taffel);
    }

    TeakURBG rnd(Sim.Date + Sim.StartTime);
    std::shuffle(results.begin(), results.end(), rnd);
}

void CTafelData::AssignPositions() {
    ByPositions.clear();
    std::vector<XY> unusedPositions = AvailablePositions;

    TeakURBG rnd(Sim.Date + Sim.StartTime);
    std::shuffle(unusedPositions.begin(), unusedPositions.end(), rnd);

    SLONG posIndex = 0;
    for (SLONG i = 0; i < 7 && (posIndex < unusedPositions.size()); i++) {
        if (City[i].ZettelId != -1) {
            City[i].Type = CTafelZettel::CITY;
            City[i].Position = unusedPositions[posIndex++];

            // Add to the ByPosition array (ordered by position ASC)
            auto it = std::lower_bound(ByPositions.begin(), ByPositions.end(), &City[i], CTafelZettel::ComparePositions);
            ByPositions.insert(it, &City[i]);
        }
    }

    for (SLONG i = 0; i < 7 && (posIndex < unusedPositions.size()); i++) {
        if (Route[i].ZettelId != 0) {
            Route[i].Type = CTafelZettel::ROUTE;
            Route[i].Position = unusedPositions[posIndex++];

            // Add to the ByPosition array (ordered by position ASC)
            auto it = std::lower_bound(ByPositions.begin(), ByPositions.end(), &Route[i], CTafelZettel::ComparePositions);
            ByPositions.insert(it, &Route[i]);
        }
    }

    for (SLONG i = 0; i < 7 && (posIndex < unusedPositions.size()); i++) {
        if (Gate[i].ZettelId != -1) {
            Gate[i].Type = CTafelZettel::GATE;
            Gate[i].Position = unusedPositions[posIndex++];

            // Add to the ByPosition array (ordered by position ASC)
            auto it = std::lower_bound(ByPositions.begin(), ByPositions.end(), &Gate[i], CTafelZettel::ComparePositions);
            ByPositions.insert(it, &Gate[i]);
        }
    }
}

//--------------------------------------------------------------------------------------------
// Speichert ein TafelZettel-Objekt:
//--------------------------------------------------------------------------------------------
TEAKFILE &operator<<(TEAKFILE &File, const CTafelZettel &TafelZettel) {
    File << TafelZettel.ZettelId << TafelZettel.Player;
    File << TafelZettel.Preis << TafelZettel.Rang;
    File << TafelZettel.WasInterested;

    return (File);
}

//--------------------------------------------------------------------------------------------
// Lädt ein TafelZettel-Objekt:
//--------------------------------------------------------------------------------------------
TEAKFILE &operator>>(TEAKFILE &File, CTafelZettel &TafelZettel) {
    File >> TafelZettel.ZettelId >> TafelZettel.Player;
    File >> TafelZettel.Preis >> TafelZettel.Rang;
    File >> TafelZettel.WasInterested;

    return (File);
}

//--------------------------------------------------------------------------------------------
// Speichert ein TafelData-Objekt:
//--------------------------------------------------------------------------------------------
TEAKFILE &operator<<(TEAKFILE &File, const CTafelData &TafelData) {
    SLONG c = 0;

    for (c = 0; c < 7; c++) {
        File << TafelData.Route[c] << TafelData.City[c] << TafelData.Gate[c];
    }

    return (File);
}

//--------------------------------------------------------------------------------------------
// Lädt ein TafelData-Objekt:
//--------------------------------------------------------------------------------------------
TEAKFILE &operator>>(TEAKFILE &File, CTafelData &TafelData) {
    SLONG c = 0;

    for (c = 0; c < 7; c++) {
        File >> TafelData.Route[c] >> TafelData.City[c] >> TafelData.Gate[c];
        if (SaveVersion == 1 && SaveVersionSub < 102) {
            if (TafelData.City[c].ZettelId == 0) {
                TafelData.City[c].ZettelId = -1; // Amsterdam-Bugfix
            }
        }
    }

    TafelData.AssignPositions();
    return (File);
}
