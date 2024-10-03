//============================================================================================
// Editor.cpp : Der Flugzeugeditor
//============================================================================================
#include "AtNet.h"
#include "ColorFx.h"
#include "Editor.h"
#include "gleditor.h"
#include "global.h"
#include "helper.h"
#include "Proto.h"

#include <fstream>
#include <sstream>
#include <string>

#if __cplusplus < 201703L // If the version of C++ is less than 17
#include <experimental/filesystem>
// It was still in the experimental:: namespace
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
#endif

#include <algorithm>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern SB_CColorFX ColorFX;

#define DSBVOLUME_MIN (-10000)
#define DSBVOLUME_MAX 0

void ParseTokens(char *String, char *tokens[], SLONG nTokens);

std::vector<CPlaneBuild> gPlaneBuilds;
std::unordered_map<std::string, SLONG> gPlaneBuildsHash;

#undef _C2

std::vector<CPlanePartRelation> gPlanePartRelations;

void CEditor::generateStaticData() {
    CString str;

    // Planebuilds (static data):
    {
        // Id    Shortname Cost  Weight  Power, Noise, Wartung, Passag., Verbrauch, BmIdx zPos
        gPlaneBuilds.emplace_back(1000, "B1", 10000000, 40000, 0, 0, -15, 140, 0, 0, 10000);
        gPlaneBuilds.emplace_back(1001, "B2", 40000000, 90000, 0, -5, 0, 600, 0, 1, 10000);
        gPlaneBuilds.emplace_back(1002, "B3", 40000000, 30000, 0, -40, -15, 130, 0, 2, 10000);
        gPlaneBuilds.emplace_back(1003, "B4", 25000000, 70000, 0, -20, -15, 320, 0, 3, 10000);
        gPlaneBuilds.emplace_back(1004, "B5", 20000000, 60000, 0, -10, -15, 280, 0, 4, 10000);

        gPlaneBuilds.emplace_back(2000, "C1", 100000, 2000, 0, 5, 10, 0, 0, 5, 10010);
        gPlaneBuilds.emplace_back(2001, "C2", 1100000, 2500, 0, 2, 5, 0, 0, 6, 10010);
        gPlaneBuilds.emplace_back(2002, "C3", 1200000, 4000, 0, 0, 0, 0, 0, 7, 10010);
        gPlaneBuilds.emplace_back(2003, "C4", 200000, 500, 0, 5, 20, 0, 0, 8, 10010);
        gPlaneBuilds.emplace_back(2004, "C5", 4000000, 5000, 0, -15, 0, 0, 0, 9, 10010);

        gPlaneBuilds.emplace_back(3000, "H1", 400000, 7000, 0, 10, 0, 0, 0, 10, 9000);
        gPlaneBuilds.emplace_back(3001, "H2", 800000, 5000, 0, 0, 0, 0, 0, 11, 9000);
        gPlaneBuilds.emplace_back(3002, "H3", 1200000, 3000, 0, 0, 0, 0, 0, 12, 9000);
        gPlaneBuilds.emplace_back(3003, "H4", 4500000, 4000, 0, 0, 0, 0, 0, 13, 9000);
        gPlaneBuilds.emplace_back(3003, "H5", 2000000, 4000, -4000, 20, -20, 0, 0, 14, 9000);
        gPlaneBuilds.emplace_back(3003, "H6", 6000000, 9000, 0, -20, 10, 0, 0, 15, 9000);
        gPlaneBuilds.emplace_back(3003, "H7", 5000000, 4000, 0, -10, 0, 0, 0, 16, 9000);

        gPlaneBuilds.emplace_back(4000, "R1", 7000000, 21000, 0, -10, 0, 0, -50, 17, 12000);
        gPlaneBuilds.emplace_back(4001, "R2", 3000000, 11000, 0, 0, 0, 0, 0, 18, 12000);
        gPlaneBuilds.emplace_back(4002, "R3", 1700000, 7000, 0, 2, 0, 0, 0, 19, 12000);
        gPlaneBuilds.emplace_back(4003, "R4", 1200000, 5000, 0, 5, 0, 0, 0, 20, 12000);
        gPlaneBuilds.emplace_back(4004, "R5", 500000, 2000, 0, 0, 0, 0, 0, 21, 12000);
        gPlaneBuilds.emplace_back(4004, "R6", 2000000, 30000, 0, 0, 0, 0, 0, 22, 12000);

        gPlaneBuilds.emplace_back(5000, "M1", 2300000, 1100, 6000, 3, 0, 0, 2000, 23, 7500);
        gPlaneBuilds.emplace_back(5001, "M2", 100000, 500, 4000, 20, 10, 0, 500, 24, 7500);
        gPlaneBuilds.emplace_back(5002, "M3", 250000, 600, 5000, 20, 10, 0, 600, 25, 7500);
        gPlaneBuilds.emplace_back(5003, "M4", 4300000, 1600, 7000, 3, 0, 0, 3000, 26, 7500);
        gPlaneBuilds.emplace_back(5003, "M5", 2400000, 900, 5000, 0, 0, 0, 4000, 27, 7500);
        gPlaneBuilds.emplace_back(5003, "M6", 5000000, 2000, 10000, 14, 5, 0, 12000, 28, 7500);
        gPlaneBuilds.emplace_back(5003, "M7", 5500000, 2500, 18000, 20, 10, 0, 8000, 29, 7500);
        gPlaneBuilds.emplace_back(5003, "M8", 3000000, 3400, 14000, 15, 12, 0, 10000, 30, 7500);

        gPlaneBuilds.emplace_back(6000, "L1", 7000000, 21000, 0, -10, 0, 0, -50, 31, 8000);
        gPlaneBuilds.emplace_back(6001, "L2", 3000000, 11000, 0, 0, 0, 0, 0, 32, 8000);
        gPlaneBuilds.emplace_back(6002, "L3", 1700000, 7000, 0, 2, 0, 0, 0, 33, 8000);
        gPlaneBuilds.emplace_back(6003, "L4", 1200000, 5000, 0, 5, 10, 0, 0, 34, 8000);
        gPlaneBuilds.emplace_back(6004, "L5", 500000, 2000, 0, 0, 10, 0, 0, 35, 8000);
        gPlaneBuilds.emplace_back(6004, "L6", 2000000, 30000, 0, 0, 0, 0, 0, 36, 8000);
        gPlaneBuilds.resize(37);
    }

    for (SLONG i = 0; i < gPlaneBuilds.size(); i++) {
        gPlaneBuildsHash[gPlaneBuilds[i].Shortname] = i;
    }

    // Relations (static data):
    {
        SLONG B1 = GetPlaneBuildIndex("B1");
        SLONG B2 = GetPlaneBuildIndex("B2");
        SLONG B3 = GetPlaneBuildIndex("B3");
        SLONG B4 = GetPlaneBuildIndex("B4");
        SLONG B5 = GetPlaneBuildIndex("B5");
        SLONG C1 = GetPlaneBuildIndex("C1");
        SLONG C2 = GetPlaneBuildIndex("C2");
        SLONG C3 = GetPlaneBuildIndex("C3");
        SLONG C4 = GetPlaneBuildIndex("C4");
        SLONG C5 = GetPlaneBuildIndex("C5");
        SLONG H1 = GetPlaneBuildIndex("H1");
        SLONG H2 = GetPlaneBuildIndex("H2");
        SLONG H3 = GetPlaneBuildIndex("H3");
        SLONG H4 = GetPlaneBuildIndex("H4");
        SLONG H5 = GetPlaneBuildIndex("H5");
        SLONG H6 = GetPlaneBuildIndex("H6");
        SLONG H7 = GetPlaneBuildIndex("H7");
        SLONG L1 = GetPlaneBuildIndex("L1");
        SLONG L2 = GetPlaneBuildIndex("L2");
        SLONG L3 = GetPlaneBuildIndex("L3");
        SLONG L4 = GetPlaneBuildIndex("L4");
        SLONG L5 = GetPlaneBuildIndex("L5");
        SLONG L6 = GetPlaneBuildIndex("L6");
        SLONG M1 = GetPlaneBuildIndex("M1");
        SLONG M2 = GetPlaneBuildIndex("M2");
        SLONG M3 = GetPlaneBuildIndex("M3");
        SLONG M4 = GetPlaneBuildIndex("M4");
        SLONG M5 = GetPlaneBuildIndex("M5");
        SLONG M6 = GetPlaneBuildIndex("M6");
        SLONG M7 = GetPlaneBuildIndex("M7");
        SLONG M8 = GetPlaneBuildIndex("M8");
        SLONG R1 = GetPlaneBuildIndex("R1");
        SLONG R2 = GetPlaneBuildIndex("R2");
        SLONG R3 = GetPlaneBuildIndex("R3");
        SLONG R4 = GetPlaneBuildIndex("R4");
        SLONG R5 = GetPlaneBuildIndex("R5");
        SLONG R6 = GetPlaneBuildIndex("R6");

        // Für das Anhängen der Tragflächen:
        XY rbody2a(66, 79);
        XY rbody2b(118, 91);
        XY rbody2c(169, 105);
        XY rbody2d(139, 138);
        XY lbody2a(66 + 29, 79 - 30);
        XY lbody2b(118 + 29, 91 - 30);
        XY lbody2c(169 + 29, 105 - 30);
        XY lbody2d(139 + 29, 138 - 30);

        XY rbody3(82, 77);
        XY lbody3(118, 44);

        XY rbody4(114, 113);
        XY lbody4(160, 42);

        XY rbody5(107, 94);
        XY lbody5(120 + 36, 33 + 15);

        XY rwing1(182, 14);
        XY lwing1(80, 166);

        XY rwing2(206, 11);
        XY lwing2(68, 141);

        XY rwing3(202, 20);
        XY lwing3(110, 129);

        XY rwing4(202, 20);
        XY lwing4(57, 135);

        XY rwing5(121, 10);
        XY lwing5(62, 86);

        XY rwing6(200, 21);
        XY lwing6(111, 100);

        // Für das Anhängen der Triebwerke:
        XY motor1(45, 6);
        XY motor2(33, 4);
        XY motor3(44, 13);
        XY motor4(42, 4);
        XY motor5(49, 12);
        XY motor6(79, 30);
        XY motor7(87, 31);
        XY motor8(91, 4);

        XY m_rwing1a(224, 41);
        XY m_rwing1b(150, 58);
        XY m_rwing1c(80, 81);
        XY m_lwing1a(138, 163);
        XY m_lwing1b(146, 112);
        XY m_lwing1c(154, 60);

        XY m_rwing2a(221, 29);
        XY m_rwing2b(156, 45);
        XY m_rwing2c(57, 61);
        XY m_lwing2a(106, 129);
        XY m_lwing2b(108, 90);
        XY m_lwing2c(98, 23);

        XY m_rwing3a(235, 61);
        XY m_rwing3b(181, 76);
        XY m_rwing3c(81, 97);
        XY m_lwing3a(195, 118);
        XY m_lwing3b(190, 88);
        XY m_lwing3c(185, 34);

        XY m_rwing4a(166, 43);
        XY m_rwing4b(103, 83);
        XY m_lwing4a(111, 92);
        XY m_lwing4b(151, 45);

        XY m_rwing5(100, 54);
        XY m_lwing5(128, 42);

        XY m_rwing6a(217, 64);
        XY m_rwing6b(117, 105);
        XY m_lwing6a(178, 87);
        XY m_lwing6b(177, 43);

        // 2d: Zum anhängen des Cockpits:
        XY _2d_cbody1(0, 0);
        XY _2d_cbody2(0, 36);
        XY _2d_cbody3(0, 0);
        XY _2d_cbody4(0, 10);
        XY _2d_cbody5(0, 10);

        XY _2d_cpit1(48, 1);
        XY _2d_cpit2(62, 0);
        XY _2d_cpit3(76, 0);
        XY _2d_cpit4(37, 0);
        XY _2d_cpit5(99, 0);

        // 2d: Zum anhängen des Hecks:
        XY _2d_hbody1(89, 0);
        XY _2d_hbody2(214, 11);
        XY _2d_hbody3(134, 0);
        XY _2d_hbody4(178, 8);
        XY _2d_hbody5(178, 9);

        XY _2d_heck1(0, 71);
        XY _2d_heck2(0, 47);
        XY _2d_heck3(0, 43);
        XY _2d_heck4(0, 46);
        XY _2d_heck5(0, 47);
        XY _2d_heck6(0, 29);
        XY _2d_heck7(0, 59);

        // 2d: Die Tragflächen anhängen:
        XY _2d_tbody1(44, 39);
        XY _2d_tbody2a(150, 48);
        XY _2d_tbody2b(110, 48);
        XY _2d_tbody2c(75, 48);
        XY _2d_tbody2d(96, 81);
        XY _2d_tbody3(66, 39);
        XY _2d_tbody4(91, 50);
        XY _2d_tbody5(91, 50);

        XY _2d_rght1(59, 7);
        XY _2d_rght2(52, 5);
        XY _2d_rght3(93, 6);
        XY _2d_rght4(40, 5);
        XY _2d_rght5(43, 4);
        XY _2d_rght6(93, 6);

        XY _2d_left1(59, 78);
        XY _2d_left2(52, 64);
        XY _2d_left3(93, 58);
        XY _2d_left4(40, 69);
        XY _2d_left5(90, 44);
        XY _2d_left6(93, 31);

        // Für das Anhängen der Triebwerke:
        XY _2d_motor1(28, 0);
        XY _2d_motor2(42, 6);
        XY _2d_motor3(34, 14);
        XY _2d_motor4(32, 0);
        XY _2d_motor5(27, 0);
        XY _2d_motor6(40, 0);
        XY _2d_motor7(42, 0);
        XY _2d_motor8(56, 0);

        XY _2d_m_rwing1a(27, 19);
        XY _2d_m_rwing1b(47, 27);
        XY _2d_m_rwing1c(66, 39);
        XY _2d_m_lwing1a(24, 61);
        XY _2d_m_lwing1b(39, 49);
        XY _2d_m_lwing1c(53, 38);

        XY _2d_m_rwing2a(24, 11);
        XY _2d_m_rwing2b(45, 19);
        XY _2d_m_rwing2c(87, 32);
        XY _2d_m_lwing2a(36, 42);
        XY _2d_m_lwing2b(57, 29);
        XY _2d_m_lwing2c(93, 8);

        XY _2d_m_rwing3a(45, 21);
        XY _2d_m_rwing3b(72, 34);
        XY _2d_m_rwing3c(105, 49);
        XY _2d_m_lwing3a(47, 39);
        XY _2d_m_lwing3b(79, 22);
        XY _2d_m_lwing3c(105, 10);

        XY _2d_m_rwing4a(18, 20);
        XY _2d_m_rwing4b(29, 43);
        XY _2d_m_lwing4a(18, 49);
        XY _2d_m_lwing4b(29, 21);

        XY _2d_m_rwing5(19, 18);
        XY _2d_m_lwing5(18, 27);

        XY _2d_m_rwing6a(43, 21);
        XY _2d_m_rwing6b(82, 48);
        XY _2d_m_lwing6a(68, 17);
        XY _2d_m_lwing6b(97, 8);

        // Bug             // Id,  From, To, Offset2d,     Offset3d,     Note1,            Note1,           Note1,    zAdd, Slot, RulesOutSlots
        gPlanePartRelations.emplace_back(100, -1, B1, XY(-44, -75), XY(320, 220 - 30), NOTE_BEGLEITER4, NOTE_STD, NOTE_STD, 0, 0, "B*", "B*");
        gPlanePartRelations.emplace_back(101, -1, B2, XY(-107, -120), XY(320, 220 - 30), NOTE_BEGLEITER8, NOTE_BEGLEITER4, NOTE_STD, 0, 0, "B*", "B*");
        gPlanePartRelations.emplace_back(102, -1, B3, XY(-67, -76), XY(320, 220 - 30), NOTE_BEGLEITER6, NOTE_STD, NOTE_STD, 0, 0, "B*", "B*");
        gPlanePartRelations.emplace_back(103, -1, B4, XY(-80, -99), XY(320, 220 - 30), NOTE_BEGLEITER6, NOTE_BEGLEITER4, NOTE_STD, 0, 0, "B*", "B*");
        gPlanePartRelations.emplace_back(104, -1, B5, XY(-80, -94), XY(320, 220 - 30), NOTE_BEGLEITER8, NOTE_STD, NOTE_STD, 0, 0, "B*", "B*");

        // Bug->Cockpit    // Id,  From, To, Offset2d,             Offset3d,     Note1,       Note1,       Note1,         zAdd, Noise, Slot, RulesOutSlots
        gPlanePartRelations.emplace_back(200, B1, C1, _2d_cbody1 - _2d_cpit1, XY(103, 30), NOTE_PILOT3, NOTE_STD, NOTE_SPEED400, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(201, B1, C2, _2d_cbody1 - _2d_cpit2, XY(104, 30), NOTE_PILOT3, NOTE_STD, NOTE_SPEED500, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(202, B1, C3, _2d_cbody1 - _2d_cpit3, XY(104, 30), NOTE_PILOT4, NOTE_STD, NOTE_SPEED800, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(203, B1, C4, _2d_cbody1 - _2d_cpit4, XY(104, 31), NOTE_PILOT2, NOTE_STD, NOTE_SPEED300, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(204, B1, C5, _2d_cbody1 - _2d_cpit5, XY(104, 31), NOTE_PILOT4, NOTE_PILOT3, NOTE_STD, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(210, B2, C1, _2d_cbody2 - _2d_cpit1, XY(249, 105), NOTE_PILOT3, NOTE_STD, NOTE_SPEED400, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(211, B2, C2, _2d_cbody2 - _2d_cpit2, XY(250, 105), NOTE_PILOT3, NOTE_STD, NOTE_SPEED500, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(212, B2, C3, _2d_cbody2 - _2d_cpit3, XY(250, 105), NOTE_PILOT4, NOTE_STD, NOTE_SPEED800, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(213, B2, C4, _2d_cbody2 - _2d_cpit4, XY(250, 106), NOTE_PILOT2, NOTE_STD, NOTE_SPEED300, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(214, B2, C5, _2d_cbody2 - _2d_cpit5, XY(250, 106), NOTE_PILOT4, NOTE_PILOT3, NOTE_STD, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(220, B3, C1, _2d_cbody3 - _2d_cpit1, XY(157, 46), NOTE_PILOT3, NOTE_STD, NOTE_SPEED400, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(221, B3, C2, _2d_cbody3 - _2d_cpit2, XY(157, 46), NOTE_PILOT3, NOTE_STD, NOTE_SPEED500, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(222, B3, C3, _2d_cbody3 - _2d_cpit3, XY(158, 46), NOTE_PILOT4, NOTE_STD, NOTE_SPEED800, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(223, B3, C4, _2d_cbody3 - _2d_cpit4, XY(158, 47), NOTE_PILOT2, NOTE_STD, NOTE_SPEED300, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(224, B3, C5, _2d_cbody3 - _2d_cpit5, XY(158, 47), NOTE_PILOT4, NOTE_PILOT3, NOTE_STD, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(230, B4, C1, _2d_cbody4 - _2d_cpit1, XY(208, 60), NOTE_PILOT3, NOTE_STD, NOTE_SPEED400, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(231, B4, C2, _2d_cbody4 - _2d_cpit2, XY(209, 60), NOTE_PILOT3, NOTE_STD, NOTE_SPEED500, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(232, B4, C3, _2d_cbody4 - _2d_cpit3, XY(209, 60), NOTE_PILOT4, NOTE_STD, NOTE_SPEED800, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(233, B4, C4, _2d_cbody4 - _2d_cpit4, XY(209, 61), NOTE_PILOT2, NOTE_STD, NOTE_SPEED300, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(234, B4, C5, _2d_cbody4 - _2d_cpit5, XY(209, 61), NOTE_PILOT4, NOTE_PILOT3, NOTE_STD, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(240, B5, C1, _2d_cbody5 - _2d_cpit1, XY(207, 60), NOTE_PILOT3, NOTE_STD, NOTE_SPEED400, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(241, B5, C2, _2d_cbody5 - _2d_cpit2, XY(208, 60), NOTE_PILOT3, NOTE_STD, NOTE_SPEED500, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(242, B5, C3, _2d_cbody5 - _2d_cpit3, XY(208, 60), NOTE_PILOT4, NOTE_STD, NOTE_SPEED800, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(243, B5, C4, _2d_cbody5 - _2d_cpit4, XY(208, 61), NOTE_PILOT2, NOTE_STD, NOTE_SPEED300, 0, 0, "C0", "C0");
        gPlanePartRelations.emplace_back(244, B5, C5, _2d_cbody5 - _2d_cpit5, XY(208, 61), NOTE_PILOT4, NOTE_PILOT3, NOTE_STD, 0, 0, "C0", "C0");

        // Bug->Heck       // Id,  From, To, Offset2d,     Offset3d,        Note1,    Note1,    Note1,         zAdd, Noise, Slot, RulesOutSlots
        gPlanePartRelations.emplace_back(300, B1, H1, _2d_hbody1 - _2d_heck1, XY(-130, -121), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(301, B1, H2, _2d_hbody1 - _2d_heck2, XY(-126, -91), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(302, B1, H3, _2d_hbody1 - _2d_heck3, XY(-110, -90), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(303, B1, H4, _2d_hbody1 - _2d_heck4, XY(-78, -85), NOTE_STD, NOTE_STD, NOTE_SPEED500, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(304, B1, H5, _2d_hbody1 - _2d_heck5, XY(-111, -87), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(305, B1, H6, _2d_hbody1 - _2d_heck6, XY(-98, -59), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(306, B1, H7, _2d_hbody1 - _2d_heck7, XY(-136, -113), NOTE_STD, NOTE_STD, NOTE_SPEED800, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(310, B2, H1, _2d_hbody2 - _2d_heck1, XY(-130, -121), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(311, B2, H2, _2d_hbody2 - _2d_heck2, XY(-126, -91), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(312, B2, H3, _2d_hbody2 - _2d_heck3, XY(-110, -90), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(313, B2, H4, _2d_hbody2 - _2d_heck4, XY(-78, -85), NOTE_STD, NOTE_KAPUTTXL, NOTE_SPEED500, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(314, B2, H5, _2d_hbody2 - _2d_heck5, XY(-111, -87), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(315, B2, H6, _2d_hbody2 - _2d_heck6, XY(-98, -59), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(316, B2, H7, _2d_hbody2 - _2d_heck7, XY(-136, -113), NOTE_STD, NOTE_STD, NOTE_SPEED800, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(320, B3, H1, _2d_hbody3 - _2d_heck1, XY(-130, -121), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(321, B3, H2, _2d_hbody3 - _2d_heck2, XY(-126, -91), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(322, B3, H3, _2d_hbody3 - _2d_heck3, XY(-110, -90), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(323, B3, H4, _2d_hbody3 - _2d_heck4, XY(-78, -85), NOTE_STD, NOTE_STD, NOTE_SPEED500, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(324, B3, H5, _2d_hbody3 - _2d_heck5, XY(-111, -87), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(325, B3, H6, _2d_hbody3 - _2d_heck6, XY(-98, -59), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(326, B3, H7, _2d_hbody3 - _2d_heck7, XY(-136, -113), NOTE_STD, NOTE_STD, NOTE_SPEED800, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(330, B4, H1, _2d_hbody4 - _2d_heck1, XY(-130, -121), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(331, B4, H2, _2d_hbody4 - _2d_heck2, XY(-126, -91), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(332, B4, H3, _2d_hbody4 - _2d_heck3, XY(-110, -90), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(333, B4, H4, _2d_hbody4 - _2d_heck4, XY(-78, -85), NOTE_STD, NOTE_STD, NOTE_SPEED500, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(334, B4, H5, _2d_hbody4 - _2d_heck5, XY(-111, -87), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(335, B4, H6, _2d_hbody4 - _2d_heck6, XY(-98, -59), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(336, B4, H7, _2d_hbody4 - _2d_heck7, XY(-136, -113), NOTE_STD, NOTE_STD, NOTE_SPEED800, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(340, B5, H1, _2d_hbody5 - _2d_heck1, XY(-130, -121), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(341, B5, H2, _2d_hbody5 - _2d_heck2, XY(-126, -91), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(342, B5, H3, _2d_hbody5 - _2d_heck3, XY(-110, -90), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(343, B5, H4, _2d_hbody5 - _2d_heck4, XY(-78, -85), NOTE_STD, NOTE_STD, NOTE_SPEED500, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(344, B5, H5, _2d_hbody5 - _2d_heck5, XY(-111, -87), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(345, B5, H6, _2d_hbody5 - _2d_heck6, XY(-98, -59), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "H0", "H0");
        gPlanePartRelations.emplace_back(346, B5, H7, _2d_hbody5 - _2d_heck7, XY(-136, -113), NOTE_STD, NOTE_STD, NOTE_SPEED800, 0, 0, "H0", "H0");

        // Bug->Flügel     // Id,  From, To, Offset2d,     Offset3d,        Note1,    Note1,    Note1,    zAdd, Noise, Slot, RulesOutSlots
        gPlanePartRelations.emplace_back(400, B1, R4, _2d_tbody1 - _2d_rght4, XY(-128, 59), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(600, B1, L4, _2d_tbody1 - _2d_left4, XY(42, -93), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");
        gPlanePartRelations.emplace_back(401, B1, R5, _2d_tbody1 - _2d_rght5, XY(-66, 58), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(601, B1, L5, _2d_tbody1 - _2d_left5, XY(35, -46), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");

        gPlanePartRelations.emplace_back(411, B2, R1, _2d_tbody2b - _2d_rght1, rbody2b - rwing1, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(611, B2, L1, _2d_tbody2b - _2d_left1, lbody2b - lwing1, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");
        gPlanePartRelations.emplace_back(413, B2, R1, _2d_tbody2d - _2d_rght1, rbody2d - rwing1, NOTE_STD, NOTE_STD, NOTE_STD, -400, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(613, B2, L1, _2d_tbody2d - _2d_left1, lbody2d - lwing1, NOTE_STD, NOTE_STD, NOTE_STD, -400, 0, "Lx", "Lx");

        gPlanePartRelations.emplace_back(420, B2, R2, _2d_tbody2a - _2d_rght2, rbody2a - rwing2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(620, B2, L2, _2d_tbody2a - _2d_left2, lbody2a - lwing2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");
        gPlanePartRelations.emplace_back(421, B2, R2, _2d_tbody2b - _2d_rght2, rbody2b - rwing2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(621, B2, L2, _2d_tbody2b - _2d_left2, lbody2b - lwing2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");
        gPlanePartRelations.emplace_back(422, B2, R2, _2d_tbody2c - _2d_rght2, rbody2c - rwing2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(622, B2, L2, _2d_tbody2c - _2d_left2, lbody2c - lwing2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");
        gPlanePartRelations.emplace_back(423, B2, R2, _2d_tbody2d - _2d_rght2, rbody2d - rwing2, NOTE_STD, NOTE_STD, NOTE_STD, -400, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(623, B2, L2, _2d_tbody2d - _2d_left2, lbody2d - lwing2, NOTE_STD, NOTE_STD, NOTE_STD, -400, 0, "Lx", "Lx");

        gPlanePartRelations.emplace_back(424, B2, R3, _2d_tbody2b - _2d_rght3, rbody2b - rwing3, NOTE_STD, NOTE_STD, NOTE_STD, -400, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(624, B2, L3, _2d_tbody2b - _2d_left3, lbody2b - lwing3, NOTE_STD, NOTE_STD, NOTE_STD, -400, 0, "Lx", "Lx");

        gPlanePartRelations.emplace_back(430, B2, R4, _2d_tbody2a - _2d_rght4, rbody2a - rwing4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(630, B2, L4, _2d_tbody2a - _2d_left4, lbody2a - lwing4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");
        gPlanePartRelations.emplace_back(431, B2, R4, _2d_tbody2b - _2d_rght4, rbody2b - rwing4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(631, B2, L4, _2d_tbody2b - _2d_left4, lbody2b - lwing4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");
        gPlanePartRelations.emplace_back(432, B2, R4, _2d_tbody2c - _2d_rght4, rbody2c - rwing4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(632, B2, L4, _2d_tbody2c - _2d_left2, lbody2c - lwing4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");
        gPlanePartRelations.emplace_back(433, B2, R4, _2d_tbody2d - _2d_rght4, rbody2d - rwing4, NOTE_STD, NOTE_STD, NOTE_STD, -400, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(633, B2, L4, _2d_tbody2d - _2d_left4, lbody2d - lwing4, NOTE_STD, NOTE_STD, NOTE_STD, -400, 0, "Lx", "Lx");

        gPlanePartRelations.emplace_back(444, B2, R6, _2d_tbody2b - _2d_rght6, rbody2b - rwing6, NOTE_STD, NOTE_STD, NOTE_STD, -400, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(644, B2, L6, _2d_tbody2b - _2d_left6, lbody2b - lwing6, NOTE_STD, NOTE_STD, NOTE_STD, -400, 0, "Lx", "Lx");

        gPlanePartRelations.emplace_back(450, B3, R1, _2d_tbody3 - _2d_rght1, rbody3 - rwing1, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(650, B3, L1, _2d_tbody3 - _2d_left1, lbody3 - lwing1, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");
        gPlanePartRelations.emplace_back(451, B3, R2, _2d_tbody3 - _2d_rght2, rbody3 - rwing2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(651, B3, L2, _2d_tbody3 - _2d_left2, lbody3 - lwing2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");
        gPlanePartRelations.emplace_back(452, B3, R4, _2d_tbody3 - _2d_rght4, rbody3 - rwing4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(652, B3, L4, _2d_tbody3 - _2d_left4, lbody3 - lwing4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");
        gPlanePartRelations.emplace_back(453, B3, R5, _2d_tbody3 - _2d_rght5, rbody3 - rwing5, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(653, B3, L5, _2d_tbody3 - _2d_left5, lbody3 - lwing5, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");

        gPlanePartRelations.emplace_back(460, B4, R2, _2d_tbody4 - _2d_rght2, rbody4 - rwing2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(660, B4, L2, _2d_tbody4 - _2d_left2, lbody4 - lwing2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");
        gPlanePartRelations.emplace_back(461, B4, R4, _2d_tbody4 - _2d_rght4, rbody4 - rwing4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(661, B4, L4, _2d_tbody4 - _2d_left4, lbody4 - lwing4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");
        gPlanePartRelations.emplace_back(462, B4, R5, _2d_tbody4 - _2d_rght5, rbody4 - rwing5 - XY(14, 5), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(662, B4, L5, _2d_tbody4 - _2d_left5, lbody4 - lwing5, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");

        gPlanePartRelations.emplace_back(470, B5, R1, _2d_tbody5 - _2d_rght1, rbody5 - rwing1, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(670, B5, L1, _2d_tbody5 - _2d_left1, lbody5 - lwing1, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");
        gPlanePartRelations.emplace_back(471, B5, R2, _2d_tbody5 - _2d_rght2, rbody5 - rwing2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(671, B5, L2, _2d_tbody5 - _2d_left2, lbody5 - lwing2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");
        gPlanePartRelations.emplace_back(472, B5, R3, _2d_tbody5 - _2d_rght3, rbody5 - rwing3, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(672, B5, L3, _2d_tbody5 - _2d_left3, lbody5 - lwing3, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");
        gPlanePartRelations.emplace_back(473, B5, R4, _2d_tbody5 - _2d_rght4, rbody5 - rwing4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(673, B5, L4, _2d_tbody5 - _2d_left4, lbody5 - lwing4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");
        gPlanePartRelations.emplace_back(474, B5, R5, _2d_tbody5 - _2d_rght5, rbody5 - rwing5, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Rx", "Rx");
        gPlanePartRelations.emplace_back(674, B5, L5, _2d_tbody5 - _2d_left5, lbody5 - lwing5 + XY(45, 15), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "Lx", "Lx");

        // Flügel->Motor   // Id,  From, To, Offset2d,                  Offset3d,            Note1,    Note1,    Note1,    zAdd, Noise, Slot, RulesOutSlots
        gPlanePartRelations.emplace_back(700, R1, M1, _2d_m_rwing1a - _2d_motor1, m_rwing1a - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 2, "M1", "M1");
        gPlanePartRelations.emplace_back(710, L1, M1, _2d_m_lwing1a - _2d_motor1, m_lwing1a - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 0, 2, "M4", "M4");
        gPlanePartRelations.emplace_back(701, R1, M2, _2d_m_rwing1a - _2d_motor2, m_rwing1a - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 15, "M1", "M1");
        gPlanePartRelations.emplace_back(711, L1, M2, _2d_m_lwing1a - _2d_motor2, m_lwing1a - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 15, "M4", "M4");
        gPlanePartRelations.emplace_back(704, R1, M5, XY(90, 22), XY(-33, 40), NOTE_STD, NOTE_STD, NOTE_STD, 5000, 0, "MR", "MR");
        gPlanePartRelations.emplace_back(714, L1, M5, XY(64, -14), XY(102, -27), NOTE_STD, NOTE_STD, NOTE_STD, 1000, 0, "ML", "ML");
        gPlanePartRelations.emplace_back(707, R1, M8, _2d_m_rwing1a - _2d_motor8, m_rwing1a - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 8, "M1", "M1M2");
        gPlanePartRelations.emplace_back(717, L1, M8, _2d_m_lwing1a - _2d_motor8, m_lwing1a - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 0, 8, "M4", "M4M5");

        gPlanePartRelations.emplace_back(720, R1, M1, _2d_m_rwing1b - _2d_motor1, m_rwing1b - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(730, L1, M1, _2d_m_lwing1b - _2d_motor1, m_lwing1b - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(721, R1, M2, _2d_m_rwing1b - _2d_motor2, m_rwing1b - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(731, L1, M2, _2d_m_lwing1b - _2d_motor2, m_lwing1b - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(722, R1, M3, _2d_m_rwing1b - _2d_motor3, m_rwing1b - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(732, L1, M3, _2d_m_lwing1b - _2d_motor3, m_lwing1b - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(723, R1, M4, _2d_m_rwing1b - _2d_motor4, m_rwing1b - motor4, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(733, L1, M4, _2d_m_lwing1b - _2d_motor4, m_lwing1b - motor4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(725, R1, M6, _2d_m_rwing1b - _2d_motor6, m_rwing1b - motor6, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M1M2M3");
        gPlanePartRelations.emplace_back(735, L1, M6, _2d_m_lwing1b - _2d_motor6, m_lwing1b - motor6, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M4M5M6");
        gPlanePartRelations.emplace_back(726, R1, M7, _2d_m_rwing1b - _2d_motor7, m_rwing1b - motor7, NOTE_STD, NOTE_PILOT1, NOTE_STD, 3000, 0, "M2", "M1M2M3");
        gPlanePartRelations.emplace_back(736, L1, M7, _2d_m_lwing1b - _2d_motor7, m_lwing1b - motor7, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M4M5M6");
        gPlanePartRelations.emplace_back(727, R1, M8, _2d_m_rwing1b - _2d_motor8, m_rwing1b - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M1M2M3");
        gPlanePartRelations.emplace_back(737, L1, M8, _2d_m_lwing1b - _2d_motor8, m_lwing1b - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M4M5M6");

        gPlanePartRelations.emplace_back(740, R1, M1, _2d_m_rwing1c - _2d_motor1, m_rwing1c - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M3", "M3");
        gPlanePartRelations.emplace_back(750, L1, M1, _2d_m_lwing1c - _2d_motor1, m_lwing1c - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M6", "M6");
        gPlanePartRelations.emplace_back(741, R1, M2, _2d_m_rwing1c - _2d_motor2, m_rwing1c - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M3", "M3");
        gPlanePartRelations.emplace_back(751, L1, M2, _2d_m_lwing1c - _2d_motor2, m_lwing1c - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M6", "M6");
        gPlanePartRelations.emplace_back(742, R1, M3, _2d_m_rwing1c - _2d_motor3, m_rwing1c - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M3", "M3");
        gPlanePartRelations.emplace_back(752, L1, M3, _2d_m_lwing1c - _2d_motor3, m_lwing1c - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M6", "M6");
        gPlanePartRelations.emplace_back(743, R1, M4, _2d_m_rwing1c - _2d_motor4, m_rwing1c - motor4, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M3", "M3");
        gPlanePartRelations.emplace_back(753, L1, M4, _2d_m_lwing1c - _2d_motor4, m_lwing1c - motor4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M6", "M6");
        gPlanePartRelations.emplace_back(745, R1, M6, _2d_m_rwing1c - _2d_motor6, m_rwing1c - motor6, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M3", "M2M3");
        gPlanePartRelations.emplace_back(755, L1, M6, _2d_m_lwing1c - _2d_motor6, m_lwing1c - motor6, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M6", "M5M6");
        gPlanePartRelations.emplace_back(746, R1, M7, _2d_m_rwing1c - _2d_motor7, m_rwing1c - motor7, NOTE_STD, NOTE_PILOT1, NOTE_STD, 3000, 0, "M3", "M2M3");
        gPlanePartRelations.emplace_back(756, L1, M7, _2d_m_lwing1c - _2d_motor7, m_lwing1c - motor7, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M6", "M5M6");
        gPlanePartRelations.emplace_back(747, R1, M8, _2d_m_rwing1c - _2d_motor8, m_rwing1c - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M3", "M2M3");
        gPlanePartRelations.emplace_back(757, L1, M8, _2d_m_lwing1c - _2d_motor8, m_lwing1c - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M6", "M5M6");

        // Flügel2->Motor  // Id,  From, To, Offset2d,                 Offset3d,            Note1,    Note1,    Note1,    zAdd, Slot, RulesOutSlots
        gPlanePartRelations.emplace_back(800, R2, M1, _2d_m_rwing2a - _2d_motor1, m_rwing2a - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 2, "M1", "M1");
        gPlanePartRelations.emplace_back(810, L2, M1, _2d_m_lwing2a - _2d_motor1, m_lwing2a - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 0, 2, "M4", "M4");
        gPlanePartRelations.emplace_back(801, R2, M2, _2d_m_rwing2a - _2d_motor2, m_rwing2a - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 15, "M1", "M1");
        gPlanePartRelations.emplace_back(811, L2, M2, _2d_m_lwing2a - _2d_motor2, m_lwing2a - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 15, "M4", "M4");
        gPlanePartRelations.emplace_back(807, R2, M8, _2d_m_rwing2a - _2d_motor8, m_rwing2a - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 8, "M1", "M1M2");
        gPlanePartRelations.emplace_back(817, L2, M8, _2d_m_lwing2a - _2d_motor8, m_lwing2a - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 0, 8, "M4", "M4M5");

        gPlanePartRelations.emplace_back(820, R2, M1, _2d_m_rwing2b - _2d_motor1, m_rwing2b - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(830, L2, M1, _2d_m_lwing2b - _2d_motor1, m_lwing2b - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(821, R2, M2, _2d_m_rwing2b - _2d_motor2, m_rwing2b - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(831, L2, M2, _2d_m_lwing2b - _2d_motor2, m_lwing2b - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(822, R2, M3, _2d_m_rwing2b - _2d_motor3, m_rwing2b - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(832, L2, M3, _2d_m_lwing2b - _2d_motor3, m_lwing2b - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(823, R2, M4, _2d_m_rwing2b - _2d_motor4, m_rwing2b - motor4, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(833, L2, M4, _2d_m_lwing2b - _2d_motor4, m_lwing2b - motor4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(824, R2, M5, XY(87, 33), XY(-37, 43), NOTE_STD, NOTE_STD, NOTE_STD, 5000, 0, "MR", "MR");
        gPlanePartRelations.emplace_back(834, L2, M5, XY(86, -14), XY(52, -23), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "ML", "ML");
        gPlanePartRelations.emplace_back(825, R2, M6, _2d_m_rwing2b - _2d_motor6, m_rwing2b - motor6, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M1M2M3");
        gPlanePartRelations.emplace_back(835, L2, M6, _2d_m_lwing2b - _2d_motor6, m_lwing2b - motor6, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M4M5M6");
        gPlanePartRelations.emplace_back(826, R2, M7, _2d_m_rwing2b - _2d_motor7, m_rwing2b - motor7, NOTE_STD, NOTE_PILOT1, NOTE_STD, 3000, 0, "M2", "M1M2M3");
        gPlanePartRelations.emplace_back(836, L2, M7, _2d_m_lwing2b - _2d_motor7, m_lwing2b - motor7, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M4M5M6");
        gPlanePartRelations.emplace_back(827, R2, M8, _2d_m_rwing2b - _2d_motor8, m_rwing2b - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M1M2M3");
        gPlanePartRelations.emplace_back(837, L2, M8, _2d_m_lwing2b - _2d_motor8, m_lwing2b - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M4M5M6");

        gPlanePartRelations.emplace_back(840, R2, M1, _2d_m_rwing2c - _2d_motor1, m_rwing2c - motor1, NOTE_KAPUTT, NOTE_STD, NOTE_STD, 3000, 0, "M3", "M3");
        gPlanePartRelations.emplace_back(850, L2, M1, _2d_m_lwing2c - _2d_motor1, m_lwing2c - motor1, NOTE_KAPUTT, NOTE_STD, NOTE_STD, 0, 0, "M6", "M6");
        gPlanePartRelations.emplace_back(841, R2, M2, _2d_m_rwing2c - _2d_motor2, m_rwing2c - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M3", "M3");
        gPlanePartRelations.emplace_back(851, L2, M2, _2d_m_lwing2c - _2d_motor2, m_lwing2c - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M6", "M6");
        gPlanePartRelations.emplace_back(842, R2, M3, _2d_m_rwing2c - _2d_motor3, m_rwing2c - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M3", "M3");
        gPlanePartRelations.emplace_back(852, L2, M3, _2d_m_lwing2c - _2d_motor3, m_lwing2c - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M6", "M6");
        gPlanePartRelations.emplace_back(843, R2, M4, _2d_m_rwing2c - _2d_motor4, m_rwing2c - motor4, NOTE_KAPUTT, NOTE_STD, NOTE_STD, 3000, 0, "M3", "M3");
        gPlanePartRelations.emplace_back(853, L2, M4, _2d_m_lwing2c - _2d_motor4, m_lwing2c - motor4, NOTE_KAPUTT, NOTE_STD, NOTE_STD, 0, 0, "M6", "M6");
        gPlanePartRelations.emplace_back(845, R2, M6, _2d_m_rwing2c - _2d_motor6, m_rwing2c - motor6, NOTE_KAPUTTXL, NOTE_STD, NOTE_STD, 3000, 0, "M3", "M2M3");
        gPlanePartRelations.emplace_back(855, L2, M6, _2d_m_lwing2c - _2d_motor6, m_lwing2c - motor6, NOTE_KAPUTTXL, NOTE_STD, NOTE_STD, 0, 0, "M6", "M5M6");
        gPlanePartRelations.emplace_back(846, R2, M7, _2d_m_rwing2c - _2d_motor7, m_rwing2c - motor7, NOTE_KAPUTTXL, NOTE_PILOT1, NOTE_STD, 3000, 0, "M3",
                                         "M2M3");
        gPlanePartRelations.emplace_back(856, L2, M7, _2d_m_lwing2c - _2d_motor7, m_lwing2c - motor7, NOTE_KAPUTTXL, NOTE_STD, NOTE_STD, 0, 0, "M6", "M5M6");
        gPlanePartRelations.emplace_back(847, R2, M8, _2d_m_rwing2c - _2d_motor8, m_rwing2c - motor8, NOTE_KAPUTTXL, NOTE_STD, NOTE_STD, 3000, 0, "M3", "M2M3");
        gPlanePartRelations.emplace_back(857, L2, M8, _2d_m_lwing2c - _2d_motor8, m_lwing2c - motor8, NOTE_KAPUTTXL, NOTE_STD, NOTE_STD, 0, 0, "M6", "M5M6");

        // Flügel2->Motor  // Id,  From, To, Offset2d,                 Offset3d,            Note1,    Note1,    Note1,    zAdd, Noise, Slot, RulesOutSlots
        gPlanePartRelations.emplace_back(900, R3, M1, _2d_m_rwing3a - _2d_motor1, m_rwing3a - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 2, "M1", "M1");
        gPlanePartRelations.emplace_back(910, L3, M1, _2d_m_lwing3a - _2d_motor1, m_lwing3a - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 0, 2, "M4", "M4");
        gPlanePartRelations.emplace_back(901, R3, M2, _2d_m_rwing3a - _2d_motor2, m_rwing3a - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 20, "M1", "M1");
        gPlanePartRelations.emplace_back(911, L3, M2, _2d_m_lwing3a - _2d_motor2, m_lwing3a - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 20, "M4", "M4");
        gPlanePartRelations.emplace_back(904, R3, M5, XY(115, 54), XY(-19, 81), NOTE_STD, NOTE_STD, NOTE_STD, 5000, 0, "MR", "MR");
        gPlanePartRelations.emplace_back(914, L3, M5, XY(111, -13), XY(126, -18), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "ML", "ML");
        gPlanePartRelations.emplace_back(907, R3, M8, _2d_m_rwing3a - _2d_motor8, m_rwing3a - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 15, "M1", "M1M2");
        gPlanePartRelations.emplace_back(917, L3, M8, _2d_m_lwing3a - _2d_motor8, m_lwing3a - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 0, 15, "M4", "M4M5");

        gPlanePartRelations.emplace_back(920, R3, M1, _2d_m_rwing3b - _2d_motor1, m_rwing3b - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(930, L3, M1, _2d_m_lwing3b - _2d_motor1, m_lwing3b - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(921, R3, M2, _2d_m_rwing3b - _2d_motor2, m_rwing3b - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(931, L3, M2, _2d_m_lwing3b - _2d_motor2, m_lwing3b - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(922, R3, M3, _2d_m_rwing3b - _2d_motor3, m_rwing3b - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(932, L3, M3, _2d_m_lwing3b - _2d_motor3, m_lwing3b - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(923, R3, M4, _2d_m_rwing3b - _2d_motor4, m_rwing3b - motor4, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(933, L3, M4, _2d_m_lwing3b - _2d_motor4, m_lwing3b - motor4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(925, R3, M6, _2d_m_rwing3b - _2d_motor6, m_rwing3b - motor6, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M1M2M3");
        gPlanePartRelations.emplace_back(935, L3, M6, _2d_m_lwing3b - _2d_motor6, m_lwing3b - motor6, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M4M5M6");
        gPlanePartRelations.emplace_back(926, R3, M7, _2d_m_rwing3b - _2d_motor7, m_rwing3b - motor7, NOTE_STD, NOTE_PILOT1, NOTE_STD, 3000, 0, "M2", "M1M2M3");
        gPlanePartRelations.emplace_back(936, L3, M7, _2d_m_lwing3b - _2d_motor7, m_lwing3b - motor7, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M4M5M6");
        gPlanePartRelations.emplace_back(927, R3, M8, _2d_m_rwing3b - _2d_motor8, m_rwing3b - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M1M2M3");
        gPlanePartRelations.emplace_back(937, L3, M8, _2d_m_lwing3b - _2d_motor8, m_lwing3b - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M4M5M6");

        gPlanePartRelations.emplace_back(940, R3, M1, _2d_m_rwing3c - _2d_motor1, m_rwing3c - motor1, NOTE_KAPUTT, NOTE_STD, NOTE_STD, 3000, 0, "M3", "M3");
        gPlanePartRelations.emplace_back(950, L3, M1, _2d_m_lwing3c - _2d_motor1, m_lwing3c - motor1, NOTE_KAPUTT, NOTE_STD, NOTE_STD, 0, 0, "M6", "M6");
        gPlanePartRelations.emplace_back(941, R3, M2, _2d_m_rwing3c - _2d_motor2, m_rwing3c - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M3", "M3");
        gPlanePartRelations.emplace_back(951, L3, M2, _2d_m_lwing3c - _2d_motor2, m_lwing3c - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M6", "M6");
        gPlanePartRelations.emplace_back(942, R3, M3, _2d_m_rwing3c - _2d_motor3, m_rwing3c - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M3", "M3");
        gPlanePartRelations.emplace_back(952, L3, M3, _2d_m_lwing3c - _2d_motor3, m_lwing3c - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M6", "M6");
        gPlanePartRelations.emplace_back(943, R3, M4, _2d_m_rwing3c - _2d_motor4, m_rwing3c - motor4, NOTE_KAPUTT, NOTE_STD, NOTE_STD, 3000, 0, "M3", "M3");
        gPlanePartRelations.emplace_back(953, L3, M4, _2d_m_lwing3c - _2d_motor4, m_lwing3c - motor4, NOTE_KAPUTT, NOTE_STD, NOTE_STD, 0, 0, "M6", "M6");
        gPlanePartRelations.emplace_back(945, R3, M6, _2d_m_rwing3c - _2d_motor6, m_rwing3c - motor6, NOTE_KAPUTTXL, NOTE_STD, NOTE_STD, 3000, 0, "M3", "M2M3");
        gPlanePartRelations.emplace_back(955, L3, M6, _2d_m_lwing3c - _2d_motor6, m_lwing3c - motor6, NOTE_KAPUTTXL, NOTE_STD, NOTE_STD, 0, 0, "M6", "M5M6");
        gPlanePartRelations.emplace_back(946, R3, M7, _2d_m_rwing3c - _2d_motor7, m_rwing3c - motor7, NOTE_KAPUTTXL, NOTE_PILOT1, NOTE_STD, 3000, 0, "M3",
                                         "M2M3");
        gPlanePartRelations.emplace_back(956, L3, M7, _2d_m_lwing3c - _2d_motor7, m_lwing3c - motor7, NOTE_KAPUTTXL, NOTE_STD, NOTE_STD, 0, 0, "M6", "M5M6");
        gPlanePartRelations.emplace_back(947, R3, M8, _2d_m_rwing3c - _2d_motor8, m_rwing3c - motor8, NOTE_KAPUTTXL, NOTE_STD, NOTE_STD, 3000, 0, "M3", "M2M3");
        gPlanePartRelations.emplace_back(957, L3, M8, _2d_m_lwing3c - _2d_motor8, m_lwing3c - motor8, NOTE_KAPUTTXL, NOTE_STD, NOTE_STD, 0, 0, "M6", "M5M6");

        // Flügel4->Motor  // Id,  From, To, Offset2d,                 Offset3d,            Note1,    Note1,    Note1,    zAdd, Slot, RulesOutSlots
        gPlanePartRelations.emplace_back(1000, R4, M1, _2d_m_rwing4a - _2d_motor1, m_rwing4a - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 2, "M3", "M3");
        gPlanePartRelations.emplace_back(1010, L4, M1, _2d_m_lwing4a - _2d_motor1, m_lwing4a - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 0, 2, "M6", "M6");
        gPlanePartRelations.emplace_back(1001, R4, M2, _2d_m_rwing4a - _2d_motor2, m_rwing4a - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 20, "M3", "M3");
        gPlanePartRelations.emplace_back(1011, L4, M2, _2d_m_lwing4a - _2d_motor2, m_lwing4a - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 20, "M6", "M6");
        gPlanePartRelations.emplace_back(1002, R4, M3, _2d_m_rwing4a - _2d_motor3, m_rwing4a - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 20, "M3", "M3");
        gPlanePartRelations.emplace_back(1012, L4, M3, _2d_m_lwing4a - _2d_motor3, m_lwing4a - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 0, 20, "M6", "M6");
        gPlanePartRelations.emplace_back(1003, R4, M4, _2d_m_rwing4a - _2d_motor4, m_rwing4a - motor4, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 6, "M3", "M3");
        gPlanePartRelations.emplace_back(1013, L4, M4, _2d_m_lwing4a - _2d_motor4, m_lwing4a - motor4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 6, "M6", "M6");
        gPlanePartRelations.emplace_back(1004, R4, M5, XY(22, 61), XY(-24, 107), NOTE_STD, NOTE_STD, NOTE_STD, 5000, 0, "MR", "MR");
        gPlanePartRelations.emplace_back(1014, L4, M5, XY(18, -13), XY(137, -24), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "ML", "ML");
        gPlanePartRelations.emplace_back(1005, R4, M6, _2d_m_rwing4a - _2d_motor6, m_rwing4a - motor6, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 14, "ML", "M3");
        gPlanePartRelations.emplace_back(1015, L4, M6, _2d_m_lwing4a - _2d_motor6, m_lwing4a - motor6, NOTE_STD, NOTE_STD, NOTE_STD, 0, 14, "M6", "M6");
        gPlanePartRelations.emplace_back(1006, R4, M7, _2d_m_rwing4a - _2d_motor7, m_rwing4a - motor7, NOTE_STD, NOTE_PILOT1, NOTE_STD, 3000, 20, "M3", "M3");
        gPlanePartRelations.emplace_back(1016, L4, M7, _2d_m_lwing4a - _2d_motor7, m_lwing4a - motor7, NOTE_STD, NOTE_STD, NOTE_STD, 0, 20, "M6", "M6");
        gPlanePartRelations.emplace_back(1007, R4, M8, _2d_m_rwing4a - _2d_motor8, m_rwing4a - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 15, "M3", "M3");
        gPlanePartRelations.emplace_back(1017, L4, M8, _2d_m_lwing4a - _2d_motor8, m_lwing4a - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 0, 15, "M6", "M6");

        gPlanePartRelations.emplace_back(1020, R4, M1, _2d_m_rwing4b - _2d_motor1, m_rwing4b - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(1030, L4, M1, _2d_m_lwing4b - _2d_motor1, m_lwing4b - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(1021, R4, M2, _2d_m_rwing4b - _2d_motor2, m_rwing4b - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(1031, L4, M2, _2d_m_lwing4b - _2d_motor2, m_lwing4b - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(1022, R4, M3, _2d_m_rwing4b - _2d_motor3, m_rwing4b - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(1032, L4, M3, _2d_m_lwing4b - _2d_motor3, m_lwing4b - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(1023, R4, M4, _2d_m_rwing4b - _2d_motor4, m_rwing4b - motor4, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(1033, L4, M4, _2d_m_lwing4b - _2d_motor4, m_lwing4b - motor4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(1025, R4, M6, _2d_m_rwing4b - _2d_motor6, m_rwing4b - motor6, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(1035, L4, M6, _2d_m_lwing4b - _2d_motor6, m_lwing4b - motor6, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(1026, R4, M7, _2d_m_rwing4b - _2d_motor7, m_rwing4b - motor7, NOTE_STD, NOTE_PILOT1, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(1036, L4, M7, _2d_m_lwing4b - _2d_motor7, m_lwing4b - motor7, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(1027, R4, M8, _2d_m_rwing4b - _2d_motor8, m_rwing4b - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(1037, L4, M8, _2d_m_lwing4b - _2d_motor8, m_lwing4b - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");

        // Flügel4->Motor  // Id,  From, To, Offset2d,                 Offset3d,            Note1,    Note1,    Note1,    zAdd, Slot, RulesOutSlots
        gPlanePartRelations.emplace_back(1100, R5, M1, _2d_m_rwing5 - _2d_motor1, m_rwing5 - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 2, "M3", "M3");
        gPlanePartRelations.emplace_back(1110, L5, M1, _2d_m_lwing5 - _2d_motor1, m_lwing5 - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 0, 2, "M6", "M6");
        gPlanePartRelations.emplace_back(1101, R5, M2, _2d_m_rwing5 - _2d_motor2, m_rwing5 - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 20, "M3", "M3");
        gPlanePartRelations.emplace_back(1111, L5, M2, _2d_m_lwing5 - _2d_motor2, m_lwing5 - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 20, "M6", "M6");
        gPlanePartRelations.emplace_back(1102, R5, M3, _2d_m_rwing5 - _2d_motor3, m_rwing5 - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 20, "M3", "M3");
        gPlanePartRelations.emplace_back(1112, L5, M3, _2d_m_lwing5 - _2d_motor3, m_lwing5 - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 0, 20, "M6", "M6");
        gPlanePartRelations.emplace_back(1103, R5, M4, _2d_m_rwing5 - _2d_motor4, m_rwing5 - motor4, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 6, "M3", "M3");
        gPlanePartRelations.emplace_back(1113, L5, M4, _2d_m_lwing5 - _2d_motor4, m_lwing5 - motor4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 6, "M6", "M6");
        gPlanePartRelations.emplace_back(1104, R5, M5, XY(13, 37), XY(-16, 67), NOTE_STD, NOTE_STD, NOTE_STD, 5000, 0, "MR", "MR");
        gPlanePartRelations.emplace_back(1114, L5, M5, XY(13, -13), XY(103, -22), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "ML", "ML");
        gPlanePartRelations.emplace_back(1105, R5, M6, _2d_m_rwing5 - _2d_motor6, m_rwing5 - motor6, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 14, "M3", "M2M3");
        gPlanePartRelations.emplace_back(1115, L5, M6, _2d_m_lwing5 - _2d_motor6, m_lwing5 - motor6, NOTE_STD, NOTE_STD, NOTE_STD, 0, 14, "M6", "M5M6");
        gPlanePartRelations.emplace_back(1106, R5, M7, _2d_m_rwing5 - _2d_motor7, m_rwing5 - motor7, NOTE_STD, NOTE_PILOT1, NOTE_STD, 3000, 20, "M3", "M2M3");
        gPlanePartRelations.emplace_back(1116, L5, M7, _2d_m_lwing5 - _2d_motor7, m_lwing5 - motor7, NOTE_STD, NOTE_STD, NOTE_STD, 0, 20, "M6", "M5M6");
        gPlanePartRelations.emplace_back(1107, R5, M8, _2d_m_rwing5 - _2d_motor8, m_rwing5 - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 15, "M3", "M2M3");
        gPlanePartRelations.emplace_back(1117, L5, M8, _2d_m_lwing5 - _2d_motor8, m_lwing5 - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 0, 15, "M6", "M5M6");

        // Flügel6->Motor  // Id,  From, To, Offset2d,                 Offset3d,            Note1,    Note1,    Note1,    zAdd, Noise, Slot, RulesOutSlots
        gPlanePartRelations.emplace_back(1200, R6, M1, _2d_m_rwing6a - _2d_motor1, m_rwing6a - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 2, "M3", "M3");
        gPlanePartRelations.emplace_back(1210, L6, M1, _2d_m_lwing6a - _2d_motor1, m_lwing6a - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 0, 2, "M6", "M6");
        gPlanePartRelations.emplace_back(1201, R6, M2, _2d_m_rwing6a - _2d_motor2, m_rwing6a - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 20, "M3", "M3");
        gPlanePartRelations.emplace_back(1211, L6, M2, _2d_m_lwing6a - _2d_motor2, m_lwing6a - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 20, "M6", "M6");
        gPlanePartRelations.emplace_back(1202, R6, M3, _2d_m_rwing6a - _2d_motor3, m_rwing6a - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 20, "M3", "M3");
        gPlanePartRelations.emplace_back(1212, L6, M3, _2d_m_lwing6a - _2d_motor3, m_lwing6a - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 0, 20, "M6", "M6");
        gPlanePartRelations.emplace_back(1203, R6, M4, _2d_m_rwing6a - _2d_motor4, m_rwing6a - motor4, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 6, "M3", "M3");
        gPlanePartRelations.emplace_back(1213, L6, M4, _2d_m_lwing6a - _2d_motor4, m_lwing6a - motor4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 6, "M6", "M6");
        gPlanePartRelations.emplace_back(1204, R6, M5, XY(116, 76), XY(-19, 112), NOTE_STD, NOTE_STD, NOTE_STD, 5000, 0, "MR", "MR");
        gPlanePartRelations.emplace_back(1214, L6, M5, XY(104, -15), XY(124, -19), NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "ML", "ML");
        gPlanePartRelations.emplace_back(1205, R6, M6, _2d_m_rwing6a - _2d_motor6, m_rwing6a - motor6, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 14, "M3", "M3");
        gPlanePartRelations.emplace_back(1215, L6, M6, _2d_m_lwing6a - _2d_motor6, m_lwing6a - motor6, NOTE_STD, NOTE_STD, NOTE_STD, 0, 14, "M6", "M6");
        gPlanePartRelations.emplace_back(1206, R6, M7, _2d_m_rwing6a - _2d_motor7, m_rwing6a - motor7, NOTE_STD, NOTE_PILOT1, NOTE_STD, 3000, 20, "M3", "M3");
        gPlanePartRelations.emplace_back(1216, L6, M7, _2d_m_lwing6a - _2d_motor7, m_lwing6a - motor7, NOTE_STD, NOTE_STD, NOTE_STD, 0, 20, "M6", "M6");
        gPlanePartRelations.emplace_back(1207, R6, M8, _2d_m_rwing6a - _2d_motor8, m_rwing6a - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 15, "M3", "M3");
        gPlanePartRelations.emplace_back(1217, L6, M8, _2d_m_lwing6a - _2d_motor8, m_lwing6a - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 0, 15, "M6", "M6");

        gPlanePartRelations.emplace_back(1220, R6, M1, _2d_m_rwing6b - _2d_motor1, m_rwing6b - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(1230, L6, M1, _2d_m_lwing6b - _2d_motor1, m_lwing6b - motor1, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(1221, R6, M2, _2d_m_rwing6b - _2d_motor2, m_rwing6b - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(1231, L6, M2, _2d_m_lwing6b - _2d_motor2, m_lwing6b - motor2, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(1222, R6, M3, _2d_m_rwing6b - _2d_motor3, m_rwing6b - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(1232, L6, M3, _2d_m_lwing6b - _2d_motor3, m_lwing6b - motor3, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(1223, R6, M4, _2d_m_rwing6b - _2d_motor4, m_rwing6b - motor4, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(1233, L6, M4, _2d_m_lwing6b - _2d_motor4, m_lwing6b - motor4, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(1225, R6, M6, _2d_m_rwing6b - _2d_motor6, m_rwing6b - motor6, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(1235, L6, M6, _2d_m_lwing6b - _2d_motor6, m_lwing6b - motor6, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(1226, R6, M7, _2d_m_rwing6b - _2d_motor7, m_rwing6b - motor7, NOTE_STD, NOTE_PILOT1, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(1236, L6, M7, _2d_m_lwing6b - _2d_motor7, m_lwing6b - motor7, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");
        gPlanePartRelations.emplace_back(1227, R6, M8, _2d_m_rwing6b - _2d_motor8, m_rwing6b - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 3000, 0, "M2", "M2");
        gPlanePartRelations.emplace_back(1237, L6, M8, _2d_m_lwing6b - _2d_motor8, m_lwing6b - motor8, NOTE_STD, NOTE_STD, NOTE_STD, 0, 0, "M5", "M5");

        // Heck => Triebwerk  Id,  From, To, Offset2d,                 Offset3d,            Note1,       Note1,    Note1,    zAdd, Noise, Slot, RulesOutSlots
        gPlanePartRelations.emplace_back(1304, H2, M5, XY(40, 68), XY(-47, 99), NOTE_PILOT1, NOTE_STD, NOTE_STD, 1550, 0, "Mr", "Mr");
        gPlanePartRelations.emplace_back(1314, H2, M5, XY(40, 25), XY(69, 11), NOTE_STD, NOTE_STD, NOTE_STD, 1450, 0, "Ml", "Ml");
        gPlanePartRelations.emplace_back(1305, H5, M5, XY(24, 71), XY(-34, 106), NOTE_PILOT1, NOTE_STD, NOTE_STD, 1550, 0, "Mr", "Mr");
        gPlanePartRelations.emplace_back(1315, H5, M5, XY(24, 40), XY(76, 13), NOTE_STD, NOTE_STD, NOTE_STD, 1450, 0, "Ml", "Ml");
        gPlanePartRelations.emplace_back(1306, H7, M5, XY(72, 21), XY(-33, 33), NOTE_PILOT1, NOTE_STD, NOTE_STD, 1550, 0, "Mr", "Mr");
        gPlanePartRelations.emplace_back(1316, H7, M5, XY(72, -11), XY(34, -24), NOTE_STD, NOTE_STD, NOTE_STD, 1450, 0, "Ml", "Ml");

        gPlanePartRelations.resize(307);
    }

    // Relations:
    {
        BUFFER_V<BYTE> tempBuf = LoadCompleteFile(FullFilename("relation.csv", ExcelPath));
        std::string tempStr((const char *)(tempBuf.getData()), tempBuf.AnzEntries());
        std::istringstream file(tempStr, std::ios_base::in);

        file >> str;

        for (auto &gPlanePartRelation : gPlanePartRelations) {
            file >> str;
            SLONG id = atol(str);

            if (gPlanePartRelation.Id != id) {
                hprintf(0, "Id mismatch: %li vs %li!", gPlanePartRelation.Id, id);
            }
            gPlanePartRelation.FromString(str);
        }
    }

    // Planebuilds:
    {
        BUFFER_V<BYTE> tempBuf = LoadCompleteFile(FullFilename("builds.csv", ExcelPath));
        std::string tempStr((const char *)(tempBuf.getData()), tempBuf.AnzEntries());
        std::istringstream file(tempStr, std::ios_base::in);

        file >> str;

        for (auto &gPlaneBuild : gPlaneBuilds) {
            file >> str;
            SLONG id = atol(str);

            if (gPlaneBuild.Id != id) {
                hprintf(0, "Id mismatch: %li vs %li!", gPlaneBuild.Id, id);
            }
            gPlaneBuild.FromString(str);
        }
    }
}

//--------------------------------------------------------------------------------------------
// Die Schalter wird eröffnet:
//--------------------------------------------------------------------------------------------
CEditor::CEditor(BOOL bHandy, ULONG PlayerNum) : CStdRaum(bHandy, PlayerNum, "editor.gli", GFX_EDITOR) {
    SetRoomVisited(PlayerNum, ROOM_EDITOR);
    HandyOffset = 320;
    Sim.FocusPerson = -1;

    // Tabellen exportieren:
    if (false) {
        std::string str;

        // Relations:
        {
            std::ofstream file(FullFilename("relation.csv", ExcelPath), std::ios_base::trunc | std::ios_base::out);

            str = "Id;From;To;Offset2dX;Offset2dY;Offset3dX;Offset3dY;Note1;Note2;Note3;Noise";
            file << str << std::endl;

            for (const auto &gPlanePartRelation : gPlanePartRelations) {
                str = gPlanePartRelation.ToString();
                file << str << std::endl;
            }
        }

        // Planebuilds:
        {
            std::ofstream file(FullFilename("builds.csv", ExcelPath), std::ios_base::trunc | std::ios_base::out);

            str = "Id;ShortName;Cost;Weight;Power;Noise;Wartung;Passagiere;Verbrauch";
            file << str << std::endl;

            for (const auto &gPlaneBuild : gPlaneBuilds) {
                str = gPlaneBuild.ToString();
                file << str << std::endl;
            }
        }
    }

    // Tabellen importieren:
    if (true) {
        std::string str;

        // Relations:
        {
            BUFFER_V<BYTE> tempBuf = LoadCompleteFile(FullFilename("relation.csv", ExcelPath));
            std::string tempStr((const char *)(tempBuf.getData()), tempBuf.AnzEntries());
            std::istringstream file(tempStr, std::ios_base::in);

            file >> str;

            for (auto &gPlanePartRelation : gPlanePartRelations) {
                file >> str;
                SLONG id = atol(str.c_str());

                if (gPlanePartRelation.Id != id) {
                    hprintf(0, "Id mismatch: %li vs %li!", gPlanePartRelation.Id, id);
                }
                gPlanePartRelation.FromString(str);
            }
        }

        // Planebuilds:
        {
            BUFFER_V<BYTE> tempBuf = LoadCompleteFile(FullFilename("builds.csv", ExcelPath));
            std::string tempStr((const char *)(tempBuf.getData()), tempBuf.AnzEntries());
            std::istringstream file(tempStr, std::ios_base::in);

            file >> str;

            for (auto &gPlaneBuild : gPlaneBuilds) {
                file >> str;
                SLONG id = atol(str.c_str());

                if (gPlaneBuild.Id != id) {
                    hprintf(0, "Id mismatch: %li vs %li!", gPlaneBuild.Id, id);
                }
                gPlaneBuild.FromString(str);
            }
        }
    }

    bAllowB = bAllowC = bAllowH = bAllowW = bAllowM = false;

    GripRelation = -1;
    Plane.Parts.ReSize(100);

    if (bHandy == 0) {
        AmbientManager.SetGlobalVolume(60);
    }

    GripRelation = -1;
    PartUnderCursor = "";
    sel_b = sel_c = sel_h = sel_w = sel_m = 0;
    DragDropMode = 0;

    Plane.Name = StandardTexte.GetS(TOKEN_MISC, 8210);

    PlaneFilename = FullFilename("data.plane", MyPlanePath);
    if (DoesFileExist(PlaneFilename) != 0) {
        Plane.Load(PlaneFilename);
    }

    UpdateButtonState();

    PartBms.ReSize(pRoomLib, "BODY_A01 BODY_A02 BODY_A03 BODY_A04 BODY_A05 "
                             "CPIT_A01 CPIT_A02 CPIT_A03 CPIT_A04 CPIT_A05 "
                             "HECK_A01 HECK_A02 HECK_A03 HECK_A04 HECK_A05 HECK_A06 HECK_A07 "
                             "RWNG_A01 RWNG_A02 RWNG_A03 RWNG_A04 RWNG_A05 RWNG_A06 "
                             "MOT_A01  MOT_A02  MOT_A03  MOT_A04  MOT_A05  MOT_A06  MOT_A07  MOT_A08 "
                             "LWNG_A01 LWNG_A02 LWNG_A03 LWNG_A04 LWNG_A05 LWNG_A06 ");

    SelPartBms.ReSize(pRoomLib, "S_B_01 S_B_02 S_B_03 S_B_04 S_B_05 "
                                "S_C_01 S_C_02 S_C_03 S_C_04 S_C_05 "
                                "S_H_01 S_H_02 S_H_03 S_H_04 S_H_05 S_H_06 S_H_07 "
                                "S_W_01 S_W_02 S_W_03 S_W_04 S_W_05 S_W_06 "
                                "S_M_01 S_M_02 S_M_03 S_M_04 S_M_05 S_M_06 S_M_07  S_M_08 ");

    ButtonPartLRBms.ReSize(pRoomLib, "BUT_TL0 BUT_TL1 BUT_TL2 BUT_TR0 BUT_TR1 BUT_TR2");
    ButtonPlaneLRBms.ReSize(pRoomLib, "BUT_DL0 BUT_DL1 BUT_DL2 BUT_DR0 BUT_DR1 BUT_DR2");
    OtherButtonBms.ReSize(pRoomLib, "CANCEL0 CANCEL1 CANCEL2 DELETE0 DELETE1 DELETE2 NEW0 NEW1 NEW2 OK0 OK1 OK2");
    MaskenBms.ReSize(pRoomLib, "MASKE_O MASKE_U");

    CheckUnusablePart(1);

    hprintf("stat_3.mcf");
    FontNormalRed.Load(lpDD, const_cast<char *>((LPCTSTR)FullFilename("stat_3.mcf", MiscPath)));
    FontYellow.Load(lpDD, const_cast<char *>((LPCTSTR)FullFilename("stat_4.mcf", MiscPath)));

    // Hintergrundsounds:
    if (Sim.Options.OptionEffekte != 0) {
        BackFx.ReInit("secure.raw");
        BackFx.Play(DSBPLAY_NOSTOP | DSBPLAY_LOOPING, Sim.Options.OptionEffekte * 100 / 7);
    }
}

//--------------------------------------------------------------------------------------------
// Sehr destruktiv!
//--------------------------------------------------------------------------------------------
CEditor::~CEditor() {
    BackFx.SetVolume(DSBVOLUME_MIN);
    BackFx.Stop();
}

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
void CEditor::UpdateButtonState() {
    SLONG d = 0;

    bAllowB = true;
    bAllowC = bAllowH = bAllowW = bAllowM = false;

    for (d = 0; d < Plane.Parts.AnzEntries(); d++) {
        if (Plane.Parts.IsInAlbum(d) != 0) {
            if (Plane.Parts[d].Shortname[0] == 'B') {
                bAllowC = bAllowH = bAllowW = true;
            }
            if (Plane.Parts[d].Shortname[0] == 'L' || Plane.Parts[d].Shortname[0] == 'H') {
                bAllowM = true;
            }
        }
    }

    for (d = 0; d < Plane.Parts.AnzEntries(); d++) {
        if (Plane.Parts.IsInAlbum(d) != 0) {
            if (Plane.Parts[d].Shortname[0] == 'B') {
                bAllowB = false;
            }
            if (Plane.Parts[d].Shortname[0] == 'C') {
                bAllowC = false;
            }
            if (Plane.Parts[d].Shortname[0] == 'H') {
                bAllowH = false;
            }
            if (Plane.Parts[d].Shortname[0] == 'L') {
                bAllowW = false;
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////
// CEditor message handlers
//////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------
// void CEditor::OnPaint()
//--------------------------------------------------------------------------------------------
void CEditor::OnPaint() {
    SLONG c = 0;
    SLONG d = 0;

    if (bHandy == 0) {
        SetMouseLook(CURSOR_NORMAL, 0, ROOM_EDITOR, 0);
    }

    // Die Standard Paint-Sachen kann der Basisraum erledigen
    CStdRaum::OnPaint();

    GripRelation = -1;
    bool bAlsoOutline = false;
    if (!PartUnderCursor.empty()) {
        DOUBLE BestDist = 100;

        GripRelation = -1;
        GripRelationB = -1;
        GripRelationPart = -1;
        GripAtPos = gMousePosition - PartBms[GetPlaneBuild(PartUnderCursor).BitmapIndex].Size / SLONG(2);

        if (!PartUnderCursorB.empty()) {
            GripAtPosB = GripAtPos +
                         XY(PartBms[GetPlaneBuild(PartUnderCursor).BitmapIndex].Size.x * 3 / 4, -PartBms[GetPlaneBuild(PartUnderCursorB).BitmapIndex].Size.y);
        }

        // Für alle Relations:
        for (c = 0; c < gPlanePartRelations.size(); c++) {
            if (gPlanePartRelations[c].ToBuildIndex != GetPlaneBuildIndex(PartUnderCursor)) {
                continue;
            }
            if (!Plane.Parts.IsSlotFree(gPlanePartRelations[c].Slot)) {
                continue;
            }

            XY GripToSpot(-9999, -9999);
            XY GripToSpot2d(-9999, -9999);

            // Ist die Von-Seite ein Part oder der Desktop?
            if (gPlanePartRelations[c].FromBuildIndex == -1) {
                GripToSpot = gPlanePartRelations[c].Offset3d - PartBms[GetPlaneBuild(PartUnderCursor).BitmapIndex].Size / SLONG(2);
                GripToSpot2d = gPlanePartRelations[c].Offset2d;

                if ((gMousePosition - GripToSpot).abs() < BestDist * 2) {
                    BestDist = (gMousePosition - GripToSpot).abs();
                    GripAtPos = GripToSpot;
                    GripAtPos2d = GripToSpot2d;
                    GripRelation = c;
                    GripRelationPart = -1;

                    bAlsoOutline = true;
                }
            } else {
                // Für alle eingebauten Planeparts:
                for (d = 0; d < Plane.Parts.AnzEntries(); d++) {
                    if (Plane.Parts.IsInAlbum(d) == 0) {
                        continue;
                    }

                    const auto &qPart = Plane.Parts[d];
                    if (gPlanePartRelations[c].FromBuildIndex != GetPlaneBuildIndex(qPart.Shortname)) {
                        continue;
                    }

                    GripToSpot = qPart.Pos3d + gPlanePartRelations[c].Offset3d;
                    GripToSpot2d = qPart.Pos2d + gPlanePartRelations[c].Offset2d;

                    if ((gMousePosition - GripToSpot - PartBms[GetPlaneBuild(PartUnderCursor).BitmapIndex].Size / SLONG(2)).abs() < BestDist) {
                        BestDist = (gMousePosition - GripToSpot - PartBms[GetPlaneBuild(PartUnderCursor).BitmapIndex].Size / SLONG(2)).abs();
                        GripAtPos = GripToSpot;
                        GripAtPos2d = GripToSpot2d;
                        GripRelation = c;
                        GripRelationPart = d;

                        if (!PartUnderCursorB.empty()) {
                            SLONG e = d;
                            SLONG add = 1;
                            if (PartUnderCursor[0] == 'L') {
                                /* attach wing on other side of body as well. Relation is before current relation */
                                add = -1;
                                assert(gPlanePartRelations[c + add].FromBuildIndex == gPlanePartRelations[c].FromBuildIndex);
                            } else if (PartUnderCursor[0] == 'R') {
                                /* attach wing on other side of body as well. Relation is after current relation */
                                assert(gPlanePartRelations[c + add].FromBuildIndex == gPlanePartRelations[c].FromBuildIndex);
                            } else if (PartUnderCursor[0] == 'M' && qPart.Shortname[0] == 'H') {
                                /* attaches engine to other side of tail. Relation is either before or after current
                                 * relation */
                                if (c > 0 && gPlanePartRelations[c - 1].FromBuildIndex == gPlanePartRelations[c].FromBuildIndex) {
                                    add = -1;
                                }
                                assert(gPlanePartRelations[c + add].FromBuildIndex == gPlanePartRelations[c].FromBuildIndex);
                            } else if (PartUnderCursor[0] == 'M') {
                                /* searching for other wing to attach second copy of engine. Relation is either before or after current relation
                                 */
                                for (e = 0; e < Plane.Parts.AnzEntries(); e++) {
                                    if (e == d || Plane.Parts.IsInAlbum(e) == 0) {
                                        continue;
                                    }
                                    if (Plane.Parts[e].Shortname[0] == 'L' || Plane.Parts[e].Shortname[0] == 'R') {
                                        break;
                                    }
                                }
                                assert(e < Plane.Parts.AnzEntries());
                                if (c > 0 && gPlanePartRelations[c - 1].ToBuildIndex == gPlanePartRelations[c].ToBuildIndex) {
                                    add = -1;
                                }
                                assert(gPlanePartRelations[c + add].ToBuildIndex == gPlanePartRelations[c].ToBuildIndex);
                                assert(gPlanePartRelations[c + add].FromBuildIndex == GetPlaneBuildIndex(Plane.Parts[e].Shortname));
                            }

                            GripAtPosB = Plane.Parts[e].Pos3d + gPlanePartRelations[c + add].Offset3d;
                            GripAtPosB2d = Plane.Parts[e].Pos2d + gPlanePartRelations[c + add].Offset2d;
                            GripRelationB = c + add;
                        }

                        bAlsoOutline = true;
                    }
                }
            }
        }
    }

    // Die Online-Statistik:
    SLONG weight = Plane.CalcWeight();
    SLONG passa = Plane.CalcPassagiere();
    SLONG verbrauch = Plane.CalcVerbrauch();
    SLONG noise = Plane.CalcNoise();
    SLONG wartung = Plane.CalcWartung();
    SLONG cost = Plane.CalcCost();
    SLONG speed = Plane.CalcSpeed();
    SLONG tank = Plane.CalcTank(true);
    SLONG reichw = Plane.CalcReichweite();

    /* NUR TEMPORÄR:
    SLONG verbrauch2 = 0;
    if ((verbrauch != 0) && (speed != 0) && (passa != 0)) {
        verbrauch2 = verbrauch * 100 / speed * 100 / passa;
    } else {
        verbrauch2 = 0;
    }*/

    SLONG index_b = GetPlaneBuild(bprintf("B%li", 1 + sel_b)).BitmapIndex;
    SLONG index_c = GetPlaneBuild(bprintf("C%li", 1 + sel_c)).BitmapIndex;
    SLONG index_h = GetPlaneBuild(bprintf("H%li", 1 + sel_h)).BitmapIndex;
    SLONG index_w = GetPlaneBuild(bprintf("R%li", 1 + sel_w)).BitmapIndex;
    SLONG index_m = GetPlaneBuild(bprintf("M%li", 1 + sel_m)).BitmapIndex;

    // Ggf. Onscreen-Texte einbauen:
    CStdRaum::InitToolTips();

    if ((IsDialogOpen() == 0) && (MenuIsOpen() == 0)) {
        // Part tips:
        for (c = 0; c < 5; c++) {
            if (gMousePosition.IfIsWithin(4 + c * 127, 363, 124 + c * 127, 436)) {
                CString part;

                if ((c == 0 && !bAllowB) || (c == 1 && !bAllowC) || (c == 2 && !bAllowH) || (c == 3 && !bAllowW) || (c == 4 && !bAllowM)) {
                    continue;
                }

                if (c == 0) {
                    part = bprintf("B%li", 1 + sel_b);
                }
                if (c == 1) {
                    part = bprintf("C%li", 1 + sel_c);
                }
                if (c == 2) {
                    part = bprintf("H%li", 1 + sel_h);
                }
                if (c == 3) {
                    part = bprintf("R%li", 1 + sel_w);
                }
                if (c == 4) {
                    part = bprintf("M%li", 1 + sel_m);
                }

                CPlaneBuild &qb = GetPlaneBuild(part);
                weight = qb.Weight;
                passa = qb.Passagiere;
                verbrauch = 0;
                noise = qb.Noise;
                wartung = qb.Wartung;
                cost = qb.Cost;
                speed = 0;
                tank = 0;
                reichw = 0;
            }
        }
    }

    CString loudnesstext = StandardTexte.GetS(TOKEN_MISC, 8400); // sehr leise
    if (noise > 0) {
        loudnesstext = StandardTexte.GetS(TOKEN_MISC, 8401); // leise
    }
    if (noise > 20) {
        loudnesstext = StandardTexte.GetS(TOKEN_MISC, 8402); // okay
    }
    if (noise > 40) {
        loudnesstext = StandardTexte.GetS(TOKEN_MISC, 8403); // noch okay
    }
    if (noise > 60) {
        loudnesstext = StandardTexte.GetS(TOKEN_MISC, 8404); // halbwegs laut
    }
    if (noise > 80) {
        loudnesstext = StandardTexte.GetS(TOKEN_MISC, 8405); // laut
    }
    if (noise > 100) {
        loudnesstext = StandardTexte.GetS(TOKEN_MISC, 8406); // sehr laut
    }

    CString wartungtext = StandardTexte.GetS(TOKEN_MISC, 8508); // sehr gut
    if (wartung > -30) {
        wartungtext = StandardTexte.GetS(TOKEN_MISC, 8507); // recht gut
    }
    if (wartung > -20) {
        wartungtext = StandardTexte.GetS(TOKEN_MISC, 8506); // gut
    }
    if (wartung > -10) {
        wartungtext = StandardTexte.GetS(TOKEN_MISC, 8505); // über normal
    }
    if (wartung >= 0) {
        wartungtext = StandardTexte.GetS(TOKEN_MISC, 8504); // normal
    }
    if (wartung > 20) {
        wartungtext = StandardTexte.GetS(TOKEN_MISC, 8503); // mäßig
    }
    if (wartung > 50) {
        wartungtext = StandardTexte.GetS(TOKEN_MISC, 8502); // schlecht
    }
    if (wartung > 80) {
        wartungtext = StandardTexte.GetS(TOKEN_MISC, 8501); // sehr schlecht
    }
    if (wartung > 110) {
        wartungtext = StandardTexte.GetS(TOKEN_MISC, 8500); // katastrophal
    }

    /*   //Die Online-Statistik:
         RoomBm.PrintAt (bprintf(StandardTexte.GetS (TOKEN_MISC, 8200), weight), FontSmallBlack, TEC_FONT_LEFT, 480, 20, 640, 200);
         if (passa>0)     RoomBm.PrintAt (bprintf(StandardTexte.GetS (TOKEN_MISC, 8201), passa, passa/10), FontSmallBlack, TEC_FONT_LEFT, 480, 20+15, 640, 200);
         if (verbrauch>0) RoomBm.PrintAt (bprintf(StandardTexte.GetS (TOKEN_MISC, 8202), verbrauch), FontSmallBlack, TEC_FONT_LEFT, 480, 20+30, 640, 200);
         if (noise!=0)    RoomBm.PrintAt (bprintf(CString(StandardTexte.GetS (TOKEN_MISC, 8204)), loudnesstext, abs(noise)), FontSmallBlack, TEC_FONT_LEFT, 480,
    20+45, 640, 200); RoomBm.PrintAt (bprintf(StandardTexte.GetS (TOKEN_MISC, 8206), wartungtext), FontSmallBlack, TEC_FONT_LEFT, 480, 20+60, 640, 200); if
    (speed>0)     RoomBm.PrintAt (bprintf(StandardTexte.GetS (TOKEN_MISC, 8207), (CString)Einheiten[EINH_KMH].bString (speed)), FontSmallBlack, TEC_FONT_LEFT,
    480, 20+75, 640, 200); if (tank>0)      RoomBm.PrintAt (CString(StandardTexte.GetS (TOKEN_PLANE, 1008))+": "+(CString)Einheiten[EINH_L].bString (tank),
    FontSmallBlack, TEC_FONT_LEFT, 480, 20+75+15, 640, 200); if (reichw>0)    RoomBm.PrintAt (CString(StandardTexte.GetS (TOKEN_PLANE, 1001))+":
    "+(CString)Einheiten[EINH_KM].bString (reichw), FontSmallBlack, TEC_FONT_LEFT, 480, 20+75+30, 640, 200);
    //if (verbrauch2>0) RoomBm.PrintAt (bprintf("Verbrauch: %li l/100/100", verbrauch2), FontSmallBlack, TEC_FONT_LEFT, 480, 20+75+15, 640, 200);
    RoomBm.PrintAt (bprintf(StandardTexte.GetS (TOKEN_MISC, 8203), (CString)Einheiten[EINH_DM].bString (cost)), FontSmallBlack, TEC_FONT_LEFT, 480,
    20+100+15+15, 640, 200);

    CString error = Plane.GetError();
    if (error!="") RoomBm.PrintAt (error, FontNormalRed, TEC_FONT_LEFT, 480, 20+125+15+15, 580, 300);
    */
    // Das Flugzeug blitten:
    bool bCursorBlitted = false;
    // bool bCursorBlittedB = false;
    for (d = 0; d < Plane.Parts.AnzEntries(); d++) {
        if (Plane.Parts.IsInAlbum(d) != 0) {
            BOOL bShift = 0; //(AtGetAsyncKeyState (ATKEY_SHIFT)/256)!=0;

            SBBM &qBm = bShift != 0 ? (gEditorPlane2dBms[GetPlaneBuild(Plane.Parts[d].Shortname).BitmapIndex])
                                    : (PartBms[GetPlaneBuild(Plane.Parts[d].Shortname).BitmapIndex]);
            XY p = bShift != 0 ? (Plane.Parts[d].Pos2d + XY(320, 200)) : (Plane.Parts[d].Pos3d);

            if (!PartUnderCursor.empty() && !bCursorBlitted &&
                GetPlaneBuild(PartUnderCursor).zPos + GripAtPos.y + gPlanePartRelations[GripRelation].zAdd <=
                    GetPlaneBuild(Plane.Parts[d].Shortname).zPos + Plane.Parts[d].Pos3d.y) {
                RoomBm.BlitFromT(PartBms[GetPlaneBuild(PartUnderCursor).BitmapIndex], GripAtPos);
                bCursorBlitted = true;
            }
            /*if (PartUnderCursorB!="" && bCursorBlittedB==false &&
              GetPlaneBuild(PartUnderCursorB).zPos+GripAtPosB.y+gPlanePartRelations[GripRelation].zAdd<=GetPlaneBuild(Plane.Parts[d].Shortname).zPos+Plane.Parts[d].Pos3d.y)
              {
              RoomBm.BlitFromT (PartBms[GetPlaneBuild(PartUnderCursorB).BitmapIndex], GripAtPosB);
              bCursorBlittedB=true;
              }*/

            RoomBm.BlitFromT(qBm, p);
        }
    }

    if (Plane.Parts.GetNumUsed() > 0) {
        for (SLONG cx = -1; cx <= 1; cx++) {
            for (SLONG cy = -1; cy <= 1; cy++) {
                // Die Online-Statistik:
                RoomBm.PrintAt(bprintf(StandardTexte.GetS(TOKEN_MISC, 8200), weight), FontSmallBlack, TEC_FONT_LEFT, 480 + cx, 20 + cy, 640 + cx, 200 + cy);
                if (passa > 0) {
                    RoomBm.PrintAt(bprintf(StandardTexte.GetS(TOKEN_MISC, 8201), passa, passa / 10), FontSmallBlack, TEC_FONT_LEFT, 480 + cx, 20 + 15 + cy,
                                   640 + cx, 200 + cy);
                }
                if (verbrauch > 0) {
                    RoomBm.PrintAt(bprintf(StandardTexte.GetS(TOKEN_MISC, 8202), verbrauch), FontSmallBlack, TEC_FONT_LEFT, 480 + cx, 20 + 30 + cy, 640 + cx,
                                   200 + cy);
                }
                if (noise != 0) {
                    RoomBm.PrintAt(bprintf(CString(StandardTexte.GetS(TOKEN_MISC, 8204)), loudnesstext.c_str(), abs(noise)), FontSmallBlack, TEC_FONT_LEFT,
                                   480 + cx, 20 + 45 + cy, 640 + cx, 200 + cy);
                }
                RoomBm.PrintAt(bprintf(StandardTexte.GetS(TOKEN_MISC, 8206), wartungtext.c_str()), FontSmallBlack, TEC_FONT_LEFT, 480 + cx, 20 + 60 + cy,
                               640 + cx, 200 + cy);
                if (speed > 0) {
                    RoomBm.PrintAt(bprintf(StandardTexte.GetS(TOKEN_MISC, 8207), CString(Einheiten[EINH_KMH].bString(speed)).c_str()), FontSmallBlack,
                                   TEC_FONT_LEFT, 480 + cx, 20 + 75 + cy, 640 + cx, 200 + cy);
                }
                if (tank > 0) {
                    RoomBm.PrintAt(CString(StandardTexte.GetS(TOKEN_PLANE, 1008)) + ": " + CString(Einheiten[EINH_L].bString(tank)).c_str(), FontSmallBlack,
                                   TEC_FONT_LEFT, 480 + cx, 20 + 75 + 15 + cy, 640 + cx, 200 + cy);
                }
                if (reichw > 0) {
                    RoomBm.PrintAt(CString(StandardTexte.GetS(TOKEN_PLANE, 1001)) + ": " + CString(Einheiten[EINH_KM].bString(reichw)).c_str(), FontSmallBlack,
                                   TEC_FONT_LEFT, 480 + cx, 20 + 75 + 30 + cy, 640 + cx, 200 + cy);
                }
                // if (verbrauch2>0) RoomBm.PrintAt (bprintf("Verbrauch: %li l/100/100", verbrauch2), FontSmallBlack, TEC_FONT_LEFT, 480, 20+75+15, 640, 200);
                RoomBm.PrintAt(bprintf(StandardTexte.GetS(TOKEN_MISC, 8203), CString(Einheiten[EINH_DM].bString(cost)).c_str()), FontSmallBlack, TEC_FONT_LEFT,
                               480 + cx, 20 + 100 + 15 + 15 + cy, 640 + cx, 200 + cy);
            }
        }
    }
#define FontSmallBlack FontYellow

    // Die Online-Statistik:
    if (Plane.Parts.GetNumUsed() > 0) {
        RoomBm.PrintAt(bprintf(StandardTexte.GetS(TOKEN_MISC, 8200), weight), FontSmallBlack, TEC_FONT_LEFT, 480, 20, 640, 200);
        if (passa > 0) {
            RoomBm.PrintAt(bprintf(StandardTexte.GetS(TOKEN_MISC, 8201), passa, passa / 10), FontSmallBlack, TEC_FONT_LEFT, 480, 20 + 15, 640, 200);
        }
        if (verbrauch > 0) {
            RoomBm.PrintAt(bprintf(StandardTexte.GetS(TOKEN_MISC, 8202), verbrauch), FontSmallBlack, TEC_FONT_LEFT, 480, 20 + 30, 640, 200);
        }
        if (noise != 0) {
            RoomBm.PrintAt(bprintf(CString(StandardTexte.GetS(TOKEN_MISC, 8204)), loudnesstext.c_str(), abs(noise)), FontSmallBlack, TEC_FONT_LEFT, 480,
                           20 + 45, 640, 200);
        }
        RoomBm.PrintAt(bprintf(StandardTexte.GetS(TOKEN_MISC, 8206), wartungtext.c_str()), FontSmallBlack, TEC_FONT_LEFT, 480, 20 + 60, 640, 200);
        if (speed > 0) {
            RoomBm.PrintAt(bprintf(StandardTexte.GetS(TOKEN_MISC, 8207), CString(Einheiten[EINH_KMH].bString(speed)).c_str()), FontSmallBlack, TEC_FONT_LEFT,
                           480, 20 + 75, 640, 200);
        }
        if (tank > 0) {
            RoomBm.PrintAt(CString(StandardTexte.GetS(TOKEN_PLANE, 1008)) + ": " + CString(Einheiten[EINH_L].bString(tank)).c_str(), FontSmallBlack,
                           TEC_FONT_LEFT, 480, 20 + 75 + 15, 640, 200);
        }
        if (reichw > 0) {
            RoomBm.PrintAt(CString(StandardTexte.GetS(TOKEN_PLANE, 1001)) + ": " + CString(Einheiten[EINH_KM].bString(reichw)).c_str(), FontSmallBlack,
                           TEC_FONT_LEFT, 480, 20 + 75 + 30, 640, 200);
        }
        // if (verbrauch2>0) RoomBm.PrintAt (bprintf("Verbrauch: %li l/100/100", verbrauch2), FontSmallBlack, TEC_FONT_LEFT, 480, 20+75+15, 640, 200);
        RoomBm.PrintAt(bprintf(StandardTexte.GetS(TOKEN_MISC, 8203), CString(Einheiten[EINH_DM].bString(cost)).c_str()), FontSmallBlack, TEC_FONT_LEFT, 480,
                       20 + 100 + 15 + 15, 640, 200);

        CString error = Plane.GetError();
        if (!error.empty()) {
            RoomBm.PrintAt(error, FontNormalRed, TEC_FONT_LEFT, 480, 20 + 125 + 15 + 15, 580, 300);
        }
    }

    // BROKEN:
    if (!PartUnderCursor.empty() && !bCursorBlitted) {
        RoomBm.BlitFromT(PartBms[GetPlaneBuild(PartUnderCursor).BitmapIndex], GripAtPos);
    }
    // if (PartUnderCursorB!="" && bCursorBlittedB==false) RoomBm.BlitFromT (PartBms[GetPlaneBuild(PartUnderCursorB).BitmapIndex], GripAtPosB);

    if (bAlsoOutline && (IsDialogOpen() == 0) && (MenuIsOpen() == 0)) {
        SB_CColorFX::BlitOutline(PartBms[GetPlaneBuild(PartUnderCursor).BitmapIndex].pBitmap, RoomBm.pBitmap, GripAtPos, 0xffffff);
        if (!PartUnderCursorB.empty()) {
            SB_CColorFX::BlitOutline(PartBms[GetPlaneBuild(PartUnderCursorB).BitmapIndex].pBitmap, RoomBm.pBitmap, GripAtPosB, 0xffffff);
        }
    }

    // Die Maske um Überhänge zu verdecken:
    RoomBm.BlitFromT(MaskenBms[0], 0, 0);
    RoomBm.BlitFromT(MaskenBms[1], 0, 343);

    // Flugzeugname:
    RoomBm.PrintAt(Plane.Name, FontNormalGreen, TEC_FONT_CENTERED, 193, 4 + 3, 471, 25 + 3);

    // Die aktuell gewählten Parts:
    if (bAllowB) {
        RoomBm.BlitFromT(SelPartBms[index_b], 66 - SelPartBms[index_b].Size.x / 2, 399 - SelPartBms[index_b].Size.y / 2);
    } else {
        ColorFX.BlitTrans(SelPartBms[index_b].pBitmap, RoomBm.pBitmap, XY(66 - SelPartBms[index_b].Size.x / 2, 399 - SelPartBms[index_b].Size.y / 2), nullptr,
                          5);
    }
    if (bAllowC) {
        RoomBm.BlitFromT(SelPartBms[index_c], 193 - SelPartBms[index_c].Size.x / 2, 399 - SelPartBms[index_c].Size.y / 2);
    } else {
        ColorFX.BlitTrans(SelPartBms[index_c].pBitmap, RoomBm.pBitmap, XY(193 - SelPartBms[index_c].Size.x / 2, 399 - SelPartBms[index_c].Size.y / 2), nullptr,
                          5);
    }
    if (bAllowH) {
        RoomBm.BlitFromT(SelPartBms[index_h], 320 - SelPartBms[index_h].Size.x / 2, 399 - SelPartBms[index_h].Size.y / 2);
    } else {
        ColorFX.BlitTrans(SelPartBms[index_h].pBitmap, RoomBm.pBitmap, XY(320 - SelPartBms[index_h].Size.x / 2, 399 - SelPartBms[index_h].Size.y / 2), nullptr,
                          5);
    }
    if (bAllowW) {
        RoomBm.BlitFromT(SelPartBms[index_w], 447 - SelPartBms[index_w].Size.x / 2, 399 - SelPartBms[index_w].Size.y / 2);
    } else {
        ColorFX.BlitTrans(SelPartBms[index_w].pBitmap, RoomBm.pBitmap, XY(447 - SelPartBms[index_w].Size.x / 2, 399 - SelPartBms[index_w].Size.y / 2), nullptr,
                          5);
    }
    if (bAllowM) {
        RoomBm.BlitFromT(SelPartBms[index_m], 574 - SelPartBms[index_m].Size.x / 2, 399 - SelPartBms[index_m].Size.y / 2);
    } else {
        ColorFX.BlitTrans(SelPartBms[index_m].pBitmap, RoomBm.pBitmap, XY(574 - SelPartBms[index_m].Size.x / 2, 399 - SelPartBms[index_m].Size.y / 2), nullptr,
                          5);
    }

    if ((IsDialogOpen() == 0) && (MenuIsOpen() == 0)) {
        // Ok, Cancel:
        if (gMousePosition.IfIsWithin(602, 192, 640, 224)) {
            SetMouseLook(CURSOR_EXIT, 0, ROOM_EDITOR, 998);
        }
        if (gMousePosition.IfIsWithin(602, 158, 640, 188)) {
            SetMouseLook(CURSOR_EXIT, 0, ROOM_EDITOR, 999);
        }

        // Delete, new:
        if (gMousePosition.IfIsWithin(0, 192, 38, 224)) {
            SetMouseLook(CURSOR_HOT, 0, ROOM_EDITOR, 900);
        }
        if (gMousePosition.IfIsWithin(0, 158, 38, 188)) {
            SetMouseLook(CURSOR_HOT, 0, ROOM_EDITOR, 901);
        }

        // Prev, Next:
        if (gMousePosition.IfIsWithin(185, 0, 212, 23)) {
            SetMouseLook(CURSOR_HOT, 0, ROOM_EDITOR, 800);
        }
        if (gMousePosition.IfIsWithin(438, 0, 465, 23)) {
            SetMouseLook(CURSOR_HOT, 0, ROOM_EDITOR, 801);
        }

        // Rename:
        if (gMousePosition.IfIsWithin(212, 0, 438, 15)) {
            SetMouseLook(CURSOR_HOT, 0, ROOM_EDITOR, 10);
        }

        // Select Parts:
        for (c = 0; c < 5; c++) {
            if ((c == 0 && !bAllowB) || (c == 1 && !bAllowC) || (c == 2 && !bAllowH) || (c == 3 && !bAllowW) || (c == 4 && !bAllowM)) {
                continue;
            }

            if (gMousePosition.IfIsWithin(4 + c * 127, 370, 27 + c * 127, 426)) {
                SetMouseLook(CURSOR_HOT, 0, ROOM_EDITOR, c * 100 + 100);
            }
            if (gMousePosition.IfIsWithin(101 + c * 127, 370, 124 + c * 127, 426)) {
                SetMouseLook(CURSOR_HOT, 0, ROOM_EDITOR, c * 100 + 101);
            }
            if (gMousePosition.IfIsWithin(27 + c * 127, 363, 101 + c * 127, 436)) {
                SetMouseLook(CURSOR_HOT, 0, ROOM_EDITOR, c * 100 + 105);
            }

            if (gMousePosition.IfIsWithin(4 + c * 127, 363, 124 + c * 127, 436)) {
                CString part;

                if (c == 0) {
                    part = bprintf("B%li", 1 + sel_b);
                }
                if (c == 1) {
                    part = bprintf("C%li", 1 + sel_c);
                }
                if (c == 2) {
                    part = bprintf("H%li", 1 + sel_h);
                }
                if (c == 3) {
                    part = bprintf("R%li", 1 + sel_w);
                }
                if (c == 4) {
                    part = bprintf("M%li", 1 + sel_m);
                }

                CPlaneBuild &qb = GetPlaneBuild(part);
                weight = qb.Weight;
                passa = qb.Passagiere;
                verbrauch = 0;
                noise = qb.Noise;
                wartung = qb.Wartung;
                cost = qb.Cost;
                speed = 0;
            }
        }
    }

    // Select Part Buttons:
    for (c = 0; c < 5; c++) {
        if ((c == 0 && !bAllowB) || (c == 1 && !bAllowC) || (c == 2 && !bAllowH) || (c == 3 && !bAllowW) || (c == 4 && !bAllowM)) {
            continue;
        }

        RoomBm.BlitFromT(
            ButtonPartLRBms[0 + static_cast<SLONG>(MouseClickId == c * 100 + 100) + static_cast<SLONG>(MouseClickId == c * 100 + 100 && (gMouseLButton != 0))],
            4 + c * 127, 370);
        RoomBm.BlitFromT(
            ButtonPartLRBms[3 + static_cast<SLONG>(MouseClickId == c * 100 + 101) + static_cast<SLONG>(MouseClickId == c * 100 + 101 && (gMouseLButton != 0))],
            101 + c * 127, 370);
    }

    // Cancel, Delete, New, Ok:
    RoomBm.BlitFromT(OtherButtonBms[0 + static_cast<SLONG>(MouseClickId == 998) + static_cast<SLONG>(MouseClickId == 998 && (gMouseLButton != 0))], 602, 192);
    RoomBm.BlitFromT(OtherButtonBms[3 + static_cast<SLONG>(MouseClickId == 900) + static_cast<SLONG>(MouseClickId == 900 && (gMouseLButton != 0))], 0, 192);
    RoomBm.BlitFromT(OtherButtonBms[6 + static_cast<SLONG>(MouseClickId == 901) + static_cast<SLONG>(MouseClickId == 901 && (gMouseLButton != 0))], 0, 158);
    RoomBm.BlitFromT(OtherButtonBms[9 + static_cast<SLONG>(MouseClickId == 999) + static_cast<SLONG>(MouseClickId == 999 && (gMouseLButton != 0))], 602, 158);

    // Prev, Next:
    RoomBm.BlitFromT(ButtonPlaneLRBms[0 + static_cast<SLONG>(MouseClickId == 800) + static_cast<SLONG>(MouseClickId == 800 && (gMouseLButton != 0))], 185, 0);
    RoomBm.BlitFromT(ButtonPlaneLRBms[3 + static_cast<SLONG>(MouseClickId == 801) + static_cast<SLONG>(MouseClickId == 801 && (gMouseLButton != 0))], 438, 0);

    if ((IsDialogOpen() == 0) && (MenuIsOpen() == 0)) {
        bool bHotPartFound = false;
        for (SLONG pass = 1; pass <= 2; pass++) {
            for (d = Plane.Parts.AnzEntries() - 1; d >= 0; d--) {
                if ((Plane.Parts.IsInAlbum(d) != 0) && (Plane.Parts[d].Shortname[0] == 'M') == (pass == 1)) {
                    SBBM &qBm = PartBms[GetPlaneBuild(Plane.Parts[d].Shortname).BitmapIndex];
                    XY p = Plane.Parts[d].Pos3d;

                    if (PartUnderCursor.empty() && !bHotPartFound) {
                        if (gMousePosition.x >= p.x && gMousePosition.y >= p.y && gMousePosition.x < p.x + qBm.Size.x && gMousePosition.y < p.y + qBm.Size.y) {
                            if (qBm.GetPixel(gMousePosition.x - p.x, gMousePosition.y - p.y) != 0) {
                                SB_CColorFX::BlitOutline(qBm.pBitmap, RoomBm.pBitmap, p, 0xffffff);
                                SetMouseLook(CURSOR_HOT, 0, ROOM_EDITOR, 10000 + d);
                                bHotPartFound = true;
                            }
                        }
                    }
                }
            }
        }
    }

    CStdRaum::PostPaint();
    CStdRaum::PumpToolTips();
}

//--------------------------------------------------------------------------------------------
// void CEditor::OnLButtonDown(UINT nFlags, CPoint point)
//--------------------------------------------------------------------------------------------
void CEditor::OnLButtonDown(UINT nFlags, CPoint point) {
    XY RoomPos;

    DefaultOnLButtonDown();

    if (ConvertMousePosition(point, &RoomPos) == 0) {
        CStdRaum::OnLButtonDown(nFlags, point);
        return;
    }

    if (PreLButtonDown(point) == 0) {
        if (MouseClickArea == ROOM_EDITOR) {
            DoLButtonWork(nFlags, point);
        } else {
            CStdRaum::OnLButtonDown(nFlags, point);
        }
    }

    PartUnderCursorB = "";
    if (!PartUnderCursor.empty()) {
        if (PartUnderCursor[0] == 'L') {
            PartUnderCursorB = CString("R") + PartUnderCursor[1];
        } else if (PartUnderCursor[0] == 'R') {
            PartUnderCursorB = CString("L") + PartUnderCursor[1];
        } else if (PartUnderCursor[0] == 'M') {
            PartUnderCursorB = PartUnderCursor;
        }
    }

    UpdateButtonState();
}

//--------------------------------------------------------------------------------------------
// void CEditor::OnLButtonDblClk(UINT nFlags, CPoint point)
//--------------------------------------------------------------------------------------------
void CEditor::OnLButtonDblClk(UINT nFlags, CPoint point) {
    XY RoomPos;

    DefaultOnLButtonDown();

    if (ConvertMousePosition(point, &RoomPos) == 0) {
        CStdRaum::OnLButtonDblClk(nFlags, point);
        return;
    }

    if (PreLButtonDown(point) == 0) {
        if (MouseClickArea == ROOM_EDITOR) {
            DoLButtonWork(nFlags, point);
        } else {
            CStdRaum::OnLButtonDblClk(nFlags, point);
        }
    }

    if (!PartUnderCursor.empty() && PartUnderCursor[0] == 'R') {
        PartUnderCursorB = CString("L") + PartUnderCursor[1];
    } else if (!PartUnderCursor.empty() && PartUnderCursor[0] == 'M') {
        PartUnderCursorB = PartUnderCursor;
    } else {
        PartUnderCursorB = "";
    }

    UpdateButtonState();
}

//--------------------------------------------------------------------------------------------
// Erledigt die eigentliche Arbeit bei einem L-Click
//--------------------------------------------------------------------------------------------
void CEditor::DoLButtonWork(UINT /*nFlags*/, const CPoint & /*point*/) {
    if (MouseClickId == 998) {
        Sim.Players.Players[PlayerNum].LeaveRoom();
    }
    if (MouseClickId == 999) {
        // Plane.Save (FullFilename (Plane.Name+".plane", MyPlanePath));
        Plane.Save(PlaneFilename);

        Sim.Players.Players[PlayerNum].LeaveRoom();
    }
    if (MouseClickId == 10) {
        MenuStart(MENU_RENAMEEDITPLANE);
    }

    if (MouseClickId == 100) {
        sel_b = (sel_b - 1 + NUM_PLANE_BODY) % NUM_PLANE_BODY;
    }
    if (MouseClickId == 101) {
        sel_b = (sel_b + 1 + NUM_PLANE_BODY) % NUM_PLANE_BODY;
    }
    if (MouseClickId == 105) {
        PartUnderCursor = bprintf("B%li", 1 + sel_b);
    }

    if (MouseClickId == 200) {
        sel_c = (sel_c - 1 + NUM_PLANE_COCKPIT) % NUM_PLANE_COCKPIT;
    }
    if (MouseClickId == 201) {
        sel_c = (sel_c + 1 + NUM_PLANE_COCKPIT) % NUM_PLANE_COCKPIT;
    }
    if (MouseClickId == 205) {
        PartUnderCursor = bprintf("C%li", 1 + sel_c);
    }

    if (MouseClickId == 300) {
        sel_h = (sel_h - 1 + NUM_PLANE_HECK) % NUM_PLANE_HECK;
    }
    if (MouseClickId == 301) {
        sel_h = (sel_h + 1 + NUM_PLANE_HECK) % NUM_PLANE_HECK;
    }
    if (MouseClickId == 305) {
        PartUnderCursor = bprintf("H%li", 1 + sel_h);
    }

    if (MouseClickId == 400) {
        sel_w = (sel_w - 1 + NUM_PLANE_LWING) % NUM_PLANE_LWING;
    }
    if (MouseClickId == 401) {
        sel_w = (sel_w + 1 + NUM_PLANE_LWING) % NUM_PLANE_LWING;
    }
    if (MouseClickId == 405) {
        PartUnderCursor = bprintf("R%li", 1 + sel_w);
    }

    if (MouseClickId == 500) {
        sel_m = (sel_m - 1 + NUM_PLANE_MOT) % NUM_PLANE_MOT;
    }
    if (MouseClickId == 501) {
        sel_m = (sel_m + 1 + NUM_PLANE_MOT) % NUM_PLANE_MOT;
    }
    if (MouseClickId == 505) {
        PartUnderCursor = bprintf("M%li", 1 + sel_m);
    }

    if (MouseClickId == 105 || MouseClickId == 205 || MouseClickId == 305 || MouseClickId == 405 || MouseClickId == 505) {
        DragDropMode = 1;
    }

    // Delete, new:
    if (MouseClickId == 900) {
        if (Plane.Parts.GetNumUsed() > 1) {
            PLAYER &qPlayer = Sim.Players.Players[PlayerNum];
            (*qPlayer.LocationWin).MenuStart(MENU_REQUEST, MENU_REQUEST_KILLPLANE, 77);
            MouseClickId = 0;
        } else {
            DeleteCurrent();
        }
    }
    if (MouseClickId == 901) {
        // Plane.Save (FullFilename (Plane.Name+".plane", MyPlanePath));
        Plane.Save(PlaneFilename);

        Plane.Clear();
        PlaneFilename = CreateNumeratedFreeFilename(FullFilename("data%s.plane", MyPlanePath));
        Plane.Name = GetFilenameFromFullFilename(PlaneFilename);
        Plane.Name = CString(StandardTexte.GetS(TOKEN_MISC, 8210)) + Plane.Name.Mid(5, Plane.Name.GetLength() - 6 - 5);
    }

    // Prev, Next:
    if (MouseClickId == 800) {
        Plane.Save(PlaneFilename);
        // Plane.Save (FullFilename (Plane.Name+".plane", MyPlanePath));
        Plane.Clear();

        // PlaneFilename = FullFilename (GetMatchingNext (FullFilename ("*.plane", MyPlanePath), Plane.Name+".plane", -1), MyPlanePath);
        PlaneFilename = FullFilename(GetMatchingNext(FullFilename("*.plane", MyPlanePath), GetFilenameFromFullFilename(PlaneFilename), -1), MyPlanePath);
        Plane.Load(PlaneFilename);
    }
    if (MouseClickId == 801) {
        Plane.Save(PlaneFilename);
        // Plane.Save (FullFilename (Plane.Name+".plane", MyPlanePath));
        Plane.Clear();

        // PlaneFilename = FullFilename (GetMatchingNext (FullFilename ("*.plane", MyPlanePath), Plane.Name+".plane", 1), MyPlanePath);
        PlaneFilename = FullFilename(GetMatchingNext(FullFilename("*.plane", MyPlanePath), GetFilenameFromFullFilename(PlaneFilename), 1), MyPlanePath);
        Plane.Load(PlaneFilename);
    }

    if (MouseClickId >= 10000) {
        SLONG relnr = Plane.Parts[SLONG(MouseClickId - 10000)].ParentRelationId;
        SLONG rel = gPlanePartRelations[relnr].Id;
        PartUnderCursor = Plane.Parts[SLONG(MouseClickId - 10000)].Shortname;
        if (PartUnderCursor[0] == 'L') {
            PartUnderCursor.SetAt(0, 'R');
        }

        DragDropMode = -1;

        if (PartUnderCursor[0] == 'B' && Plane.Parts.GetNumUsed() > 1) {
            PLAYER &qPlayer = Sim.Players.Players[PlayerNum];
            (*qPlayer.LocationWin).MenuStart(MENU_REQUEST, MENU_REQUEST_KILLPLANE);
            PartUnderCursor = "";
        } else {
            Plane.Parts -= (MouseClickId - 10000);

            while (true) {
                SLONG c = 0;
                for (c = 0; c < Plane.Parts.AnzEntries(); c++) {
                    if (Plane.Parts.IsInAlbum(c) != 0) {
                        // if ((Plane.Parts[c].ParentShortname!="" && !Plane.Parts.IsShortnameInAlbum(Plane.Parts[c].ParentShortname)) ||
                        // (Plane.Parts[c].ParentShortname!="" && PartUnderCursor[0]=='R' && ((gPlanePartRelations[Plane.Parts[c].ParentRelationId].Id==rel+200
                        // && rel>=400 && rel<600) || (gPlanePartRelations[Plane.Parts[c].ParentRelationId].Id==rel-200 && rel>=600 && rel<800))))
                        if ((!Plane.Parts[c].ParentShortname.empty() && !Plane.Parts.IsShortnameInAlbum(Plane.Parts[c].ParentShortname)) ||
                            ((!Plane.Parts[c].ParentShortname.empty() && PartUnderCursor[0] == 'R' &&
                              (gPlanePartRelations[Plane.Parts[c].ParentRelationId].Id == rel + 200 && rel >= 400 && rel < 600)) ||
                             (gPlanePartRelations[Plane.Parts[c].ParentRelationId].Id == rel - 200 && rel >= 600 && rel < 800) ||
                             (PartUnderCursor[0] == 'M' && rel >= 700 && rel <= 1400 &&
                              abs(gPlanePartRelations[Plane.Parts[c].ParentRelationId].Id - rel) == 10 && abs(relnr - Plane.Parts[c].ParentRelationId) == 1))) {
                            Plane.Parts -= c;
                            break;
                        }
                    }
                }

                if (c >= Plane.Parts.AnzEntries()) {
                    break;
                }
            }
        }
    }

    CheckUnusablePart((MouseClickId % 100) == 0 ? (-1) : (1));

    if ((MouseClickId == 100 || MouseClickId == 101) && PartUnderCursor.Left(1) == "B") {
        PartUnderCursor = bprintf("B%li", 1 + sel_b);
    }
    if ((MouseClickId == 200 || MouseClickId == 201) && PartUnderCursor.Left(1) == "C") {
        PartUnderCursor = bprintf("C%li", 1 + sel_c);
    }
    if ((MouseClickId == 300 || MouseClickId == 301) && PartUnderCursor.Left(1) == "H") {
        PartUnderCursor = bprintf("H%li", 1 + sel_h);
    }
    if ((MouseClickId == 400 || MouseClickId == 401) && PartUnderCursor.Left(1) == "R") {
        PartUnderCursor = bprintf("R%li", 1 + sel_w);
    }
    if ((MouseClickId == 500 || MouseClickId == 501) && PartUnderCursor.Left(1) == "M") {
        PartUnderCursor = bprintf("M%li", 1 + sel_m);
    }
}

//--------------------------------------------------------------------------------------------
// OnLButtonUp(UINT nFlags, CPoint point)
//--------------------------------------------------------------------------------------------
void CEditor::OnLButtonUp(UINT /*nFlags*/, CPoint /*point*/) {
    if ((IsDialogOpen() == 0) && (MenuIsOpen() == 0)) {
        if (GripRelation != -1 && DragDropMode != -1) {
            CPlanePart part;
            part.Pos2d = GripAtPos2d;
            part.Pos3d = GripAtPos;
            part.Shortname = PartUnderCursor;
            part.ParentShortname = (GripRelationPart == -1) ? "" : Plane.Parts[GripRelationPart].Shortname;
            part.ParentRelationId = GripRelation;
            Plane.Parts += std::move(part);

            if (PartUnderCursor.Left(1) == "B" || PartUnderCursor.Left(1) == "C" || PartUnderCursor.Left(1) == "H" || PartUnderCursor.Left(1) == "L" ||
                PartUnderCursor.Left(1) == "R" || PartUnderCursor.Left(1) == "W") {
                PartUnderCursor = "";
            }

            if (GripRelationB != -1) {
                CPlanePart part;
                part.Pos2d = GripAtPosB2d;
                part.Pos3d = GripAtPosB;
                part.Shortname = PartUnderCursorB;
                part.ParentShortname = Plane.Parts[GripRelationPart].Shortname;
                part.ParentRelationId = GripRelationB;
                Plane.Parts += std::move(part);

                Plane.Parts.Sort();
            }

            Plane.Parts.Sort();
            UpdateButtonState();

            if (DragDropMode != 0) {
                PartUnderCursor = PartUnderCursorB = "";
            }
        }
    }

    DragDropMode = 0;

    CheckUnusablePart(1);

    CStdRaum::OnLButtonUp(1, CPoint(1, 1));
}

//--------------------------------------------------------------------------------------------
// Testet ob ein aktuell gewähltes Teil da gar nicht dran paßt:
//--------------------------------------------------------------------------------------------
void CEditor::CheckUnusablePart(SLONG iDirection) {
    if (bAllowW) {
    again_w:
        bool bad = false;

        if (Plane.IstPartVorhanden("B1") && (sel_w == 0 || sel_w == 1 || sel_w == 2 || sel_w == 5)) {
            bad = true;
        }
        if (Plane.IstPartVorhanden("B2") && (sel_w == 4)) {
            bad = true;
        }
        if (Plane.IstPartVorhanden("B3") && (sel_w == 2 || sel_w == 5)) {
            bad = true;
        }
        if (Plane.IstPartVorhanden("B4") && (sel_w == 0 || sel_w == 2 || sel_w == 5)) {
            bad = true;
        }
        if (Plane.IstPartVorhanden("B5") && (sel_w == 5)) {
            bad = true;
        }

        if (bad) {
            sel_w = (sel_w + iDirection + NUM_PLANE_LWING) % NUM_PLANE_LWING;
            goto again_w;
        }
    }
}

//--------------------------------------------------------------------------------------------
// Löscht das aktuelle Flugzeug:
//--------------------------------------------------------------------------------------------
void CEditor::DeleteCurrent() {
    // try { std::remove (FullFilename (Plane.Name+".plane", MyPlanePath)); }
    try {
        std::remove(PlaneFilename);
    } catch (...) {
    }

    Plane.Clear();
    CString fn = FullFilename(GetMatchingNext(FullFilename("*.plane", MyPlanePath), GetFilenameFromFullFilename(PlaneFilename), -1), MyPlanePath);
    if (!fn.empty() && fn.Right(1)[0] != std::filesystem::path::preferred_separator) {
        Plane.Load(fn);
        PlaneFilename = fn;
    } else {
        Plane.Name = StandardTexte.GetS(TOKEN_MISC, 8210);
    }
}

//--------------------------------------------------------------------------------------------
// void CEditor::OnRButtonDown(UINT nFlags, CPoint point)
//--------------------------------------------------------------------------------------------
void CEditor::OnRButtonDown(UINT nFlags, CPoint point) {
    DefaultOnRButtonDown();

    // Außerhalb geklickt? Dann Default-Handler!
    if (point.x < WinP1.x || point.y < WinP1.y || point.x > WinP2.x || point.y > WinP2.y) {
        return;
    }

    if (MenuIsOpen() != 0) {
        MenuRightClick(point);
    } else {
        if (!PartUnderCursor.empty()) {
            PartUnderCursor = "";
            PartUnderCursorB = "";
            return;
        }
        if (MouseClickId >= 10000) {
            SLONG relnr = Plane.Parts[SLONG(MouseClickId - 10000)].ParentRelationId;
            SLONG rel = gPlanePartRelations[relnr].Id;
            PartUnderCursor = Plane.Parts[SLONG(MouseClickId - 10000)].Shortname;
            if (PartUnderCursor[0] == 'L') {
                PartUnderCursor.SetAt(0, 'R');
            }

            if (PartUnderCursor[0] == 'B' && Plane.Parts.GetNumUsed() > 1) {
                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];
                (*qPlayer.LocationWin).MenuStart(MENU_REQUEST, MENU_REQUEST_KILLPLANE);
                PartUnderCursor = "";
                return;
            }

            Plane.Parts -= (MouseClickId - 10000);

            while (true) {
                SLONG c = 0;
                for (c = 0; c < Plane.Parts.AnzEntries(); c++) {
                    if (Plane.Parts.IsInAlbum(c) != 0) {
                        if ((!Plane.Parts[c].ParentShortname.empty() && !Plane.Parts.IsShortnameInAlbum(Plane.Parts[c].ParentShortname)) ||
                            ((!Plane.Parts[c].ParentShortname.empty() && PartUnderCursor[0] == 'R' &&
                              (gPlanePartRelations[Plane.Parts[c].ParentRelationId].Id == rel + 200 && rel >= 400 && rel < 600)) ||
                             (gPlanePartRelations[Plane.Parts[c].ParentRelationId].Id == rel - 200 && rel >= 600 && rel < 800) ||
                             (PartUnderCursor[0] == 'M' && rel >= 700 && rel <= 1400 &&
                              abs(gPlanePartRelations[Plane.Parts[c].ParentRelationId].Id - rel) == 10 && abs(relnr - Plane.Parts[c].ParentRelationId) == 1))) {
                            Plane.Parts -= c;
                            break;
                        }
                    }
                }

                if (c >= Plane.Parts.AnzEntries()) {
                    break;
                }
            }

            PartUnderCursor = "";
            UpdateButtonState();
        } else {
            SLONG c = 0;
            SLONG MouseClickId = 0;

            for (c = 0; c < 5; c++) {
                if (gMousePosition.IfIsWithin(27 + c * 127, 363, 101 + c * 127, 436)) {
                    MouseClickId = c * 100 + 105;
                }
            }

            if (MouseClickId == 105 && Plane.Parts.GetNumUsed() > 1) {
                PLAYER &qPlayer = Sim.Players.Players[PlayerNum];
                (*qPlayer.LocationWin).MenuStart(MENU_REQUEST, MENU_REQUEST_KILLPLANE);
                return;
            }

            while (true) {
                SLONG c = 0;
                for (c = 0; c < Plane.Parts.AnzEntries(); c++) {
                    if (Plane.Parts.IsInAlbum(c) != 0) {
                        if (!Plane.Parts[c].Shortname.empty()) {
                            char typ = Plane.Parts[c].Shortname[0];

                            if ((MouseClickId == 105 && typ == 'B') || (MouseClickId == 205 && typ == 'C') || (MouseClickId == 305 && typ == 'H') ||
                                (MouseClickId == 405 && (typ == 'R' || typ == 'L')) || (MouseClickId == 505 && typ == 'M')) {
                                Plane.Parts -= c;
                                break;
                            }
                            if (!Plane.Parts[c].ParentShortname.empty() && !Plane.Parts.IsShortnameInAlbum(Plane.Parts[c].ParentShortname)) {
                                Plane.Parts -= c;
                                break;
                            }
                        }
                    }
                }

                if (c >= Plane.Parts.AnzEntries()) {
                    break;
                }
            }

            UpdateButtonState();
        }

        /*if (!IsDialogOpen() && point.y<440)
          Sim.Players.Players[(SLONG)PlayerNum].LeaveRoom();*/

        CStdRaum::OnRButtonDown(nFlags, point);
    }
}

//--------------------------------------------------------------------------------------------
// CPlaneParts
//--------------------------------------------------------------------------------------------
// Sucht einen Id eines Planeparts anhand seines Shortnames raus:
//--------------------------------------------------------------------------------------------
ULONG CPlaneParts::IdFrom(const CString &ShortName) {
    SLONG c = 0;

    for (c = 0; c < AnzEntries(); c++) {
        if ((IsInAlbum(c) != 0) && stricmp(ShortName, at(c).Shortname) == 0) {
            return (GetIdFromIndex(c));
        }
    }

    TeakLibW_Exception(FNL, ExcNever);
    return (0);
}

//--------------------------------------------------------------------------------------------
// Gibt an, ob dieses Part im Flugzeug ist:
//--------------------------------------------------------------------------------------------
bool CPlaneParts::IsShortnameInAlbum(const CString &ShortName) {
    SLONG c = 0;

    for (c = 0; c < AnzEntries(); c++) {
        if ((IsInAlbum(c) != 0) && stricmp(ShortName, at(c).Shortname) == 0) {
            return (true);
        }
    }

    return (false);
}

//--------------------------------------------------------------------------------------------
// Gibt true zurück, falls der Slot noch von keinem Part belegt ist:
//--------------------------------------------------------------------------------------------
bool CPlaneParts::IsSlotFree(const CString &Slotname) {
    SLONG c = 0;
    SLONG d = 0;

    for (c = 0; c < AnzEntries(); c++) {
        if (IsInAlbum(c) != 0) {
            CString SlotsUsed = gPlanePartRelations[(*this)[c].ParentRelationId].RulesOutSlots;

            for (d = 0; d < SlotsUsed.GetLength(); d += 2) {
                if (*(const UWORD *)(((LPCTSTR)SlotsUsed) + d) == *(const UWORD *)(LPCTSTR)Slotname) {
                    return (false);
                }
            }
        }
    }

    return (true);
}

//--------------------------------------------------------------------------------------------
// Sortiert die Parts nach der Z-Position
//--------------------------------------------------------------------------------------------
void CPlaneParts::Sort() {
    SLONG c = 0;

    for (c = 0; c < AnzEntries(); c++) {
        if (IsInAlbum(c) != 0) {
            at(c).SortIndex = GetPlaneBuild((*this)[c].Shortname).zPos + (*this)[c].Pos3d.y + gPlanePartRelations[(*this)[c].ParentRelationId].zAdd;
        }
    }
    ALBUM_V<CPlanePart>::Sort();
}

//--------------------------------------------------------------------------------------------
// Speichert ein CPlaneParts-Objekt:
//--------------------------------------------------------------------------------------------
TEAKFILE &operator<<(TEAKFILE &File, const CPlaneParts &pp) {
    File << *((const ALBUM_V<CPlanePart> *)&pp);

    return (File);
}

//--------------------------------------------------------------------------------------------
// Lädt ein CPlaneParts-Objekt:
//--------------------------------------------------------------------------------------------
TEAKFILE &operator>>(TEAKFILE &File, CPlaneParts &pp) {
    File >> *((ALBUM_V<CPlanePart> *)&pp);

    return (File);
}

//--------------------------------------------------------------------------------------------
// CPlanePart
//--------------------------------------------------------------------------------------------
// Gibt die Bitmap zurück (via das PlaneBuild Array) was dieses Part repräsentiert
//--------------------------------------------------------------------------------------------
SBBM &CPlanePart::GetBm(SBBMS &PartBms) const {
    for (auto &gPlaneBuild : gPlaneBuilds) {
        if (gPlaneBuild.Shortname == Shortname) {
            return (PartBms[gPlaneBuild.BitmapIndex]);
        }
    }

    TeakLibW_Exception(FNL, ExcNever);
    return PartBms[0];
}

//--------------------------------------------------------------------------------------------
// Speichert ein CPlanePart-Objekt:
//--------------------------------------------------------------------------------------------
TEAKFILE &operator<<(TEAKFILE &File, const CPlanePart &pp) {
    File << pp.Pos2d << pp.Pos3d << pp.Shortname << pp.ParentShortname << pp.ParentRelationId;

    return (File);
}

//--------------------------------------------------------------------------------------------
// Lädt ein CPlanePart-Objekt:
//--------------------------------------------------------------------------------------------
TEAKFILE &operator>>(TEAKFILE &File, CPlanePart &pp) {
    File >> pp.Pos2d >> pp.Pos3d >> pp.Shortname >> pp.ParentShortname >> pp.ParentRelationId;

    return (File);
}

//--------------------------------------------------------------------------------------------
// CXPlane::
//--------------------------------------------------------------------------------------------
// Löscht ein altes Flugzeug
//--------------------------------------------------------------------------------------------
void CXPlane::Clear() { Parts.ReSize(100); }

//--------------------------------------------------------------------------------------------
// Berechnet die Kosten für ein Flugzeug:
//--------------------------------------------------------------------------------------------
SLONG CXPlane::CalcCost() {
    SLONG cost = 0;

    for (SLONG c = 0; c < Parts.AnzEntries(); c++) {
        if ((Parts.IsInAlbum(c) != 0) && !Parts[c].Shortname.empty()) {
            cost += GetPlaneBuild(Parts[c].Shortname).Cost;
        }
    }

    return (cost);
}

//--------------------------------------------------------------------------------------------
// Berechnet die Anzahl der Passagiere die reinpassen:
//--------------------------------------------------------------------------------------------
SLONG CXPlane::CalcPassagiere() {
    SLONG passagiere = 0;

    for (SLONG c = 0; c < Parts.AnzEntries(); c++) {
        if (Parts.IsInAlbum(c) != 0) {
            passagiere += GetPlaneBuild(Parts[c].Shortname).Passagiere;
        }
    }

    return (passagiere);
}

//--------------------------------------------------------------------------------------------
// Berechnet die Reichweite des Flugzeuges:
//--------------------------------------------------------------------------------------------
SLONG CXPlane::CalcReichweite() {
    SLONG Reichweite = 0;

    if (CalcVerbrauch() > 0) {
        Reichweite = CalcTank() / CalcVerbrauch() * CalcSpeed();
    }

    return (Reichweite);
}

//--------------------------------------------------------------------------------------------
// Berechnet das benötigte Flugpersonal:
//--------------------------------------------------------------------------------------------
SLONG CXPlane::CalcPiloten() {
    SLONG c = 0;
    SLONG piloten = 0;

    for (c = 0; c < Parts.AnzEntries(); c++) {
        if (Parts.IsInAlbum(c) != 0) {
            for (SLONG pass = 1; pass <= 3; pass++) {
                SLONG note = 0;
                if (pass == 1) {
                    note = gPlanePartRelations[Parts[c].ParentRelationId].Note1;
                }
                if (pass == 2) {
                    note = gPlanePartRelations[Parts[c].ParentRelationId].Note2;
                }
                if (pass == 3) {
                    note = gPlanePartRelations[Parts[c].ParentRelationId].Note3;
                }

                switch (note) {
                case NOTE_PILOT1:
                    piloten += 1;
                    break;
                case NOTE_PILOT2:
                    piloten += 2;
                    break;
                case NOTE_PILOT3:
                    piloten += 3;
                    break;
                case NOTE_PILOT4:
                    piloten += 4;
                    break;
                default:
                    break;
                }
            }
        }
    }

    return (std::max((SLONG)1, piloten));
}

//--------------------------------------------------------------------------------------------
// Berechnet das benötigte Flugpersonal:
//--------------------------------------------------------------------------------------------
SLONG CXPlane::CalcBegleiter() {
    SLONG c = 0;
    SLONG begleiter = 0;

    for (c = 0; c < Parts.AnzEntries(); c++) {
        if (Parts.IsInAlbum(c) != 0) {
            for (SLONG pass = 1; pass <= 3; pass++) {
                SLONG note = 0;
                if (pass == 1) {
                    note = gPlanePartRelations[Parts[c].ParentRelationId].Note1;
                }
                if (pass == 2) {
                    note = gPlanePartRelations[Parts[c].ParentRelationId].Note2;
                }
                if (pass == 3) {
                    note = gPlanePartRelations[Parts[c].ParentRelationId].Note3;
                }

                switch (note) {
                case NOTE_BEGLEITER4:
                    begleiter += 4;
                    break;
                case NOTE_BEGLEITER6:
                    begleiter += 6;
                    break;
                case NOTE_BEGLEITER8:
                    begleiter += 8;
                    break;
                case NOTE_BEGLEITER10:
                    begleiter += 10;
                    break;
                default:
                    break;
                }
            }
        }
    }

    return (std::max((SLONG)1, begleiter));
}

//--------------------------------------------------------------------------------------------
// Berechnet die Tankgröße des Flugzeuges:
//--------------------------------------------------------------------------------------------
SLONG CXPlane::CalcTank(bool bFaked) {
    SLONG tank = 0;

    for (SLONG c = 0; c < Parts.AnzEntries(); c++) {
        if ((Parts.IsInAlbum(c) != 0) && (Parts[c].Shortname[0] == 'L' || Parts[c].Shortname[0] == 'R')) {
            tank += GetPlaneBuild(Parts[c].Shortname).Weight;
        }
    }
    tank *= 7;

    if (bFaked) {
        return (tank);
    }

    // Länger als 22 Stunden unterwegs?
    SLONG Verbrauch = CalcVerbrauch();
    if (Verbrauch > 0 && tank > 0 && tank / Verbrauch > 22) {
        tank = 22 * Verbrauch;
    }

    return (tank);
}

//--------------------------------------------------------------------------------------------
// Berechnet den Verbrauch des Flugzeugs:
//--------------------------------------------------------------------------------------------
SLONG CXPlane::CalcVerbrauch() {
    SLONG c = 0;
    SLONG verbrauch = 0;

    for (c = 0; c < Parts.AnzEntries(); c++) {
        if (Parts.IsInAlbum(c) != 0) {
            verbrauch += GetPlaneBuild(Parts[c].Shortname).Verbrauch;
        }
    }

    for (c = 0; c < Parts.AnzEntries(); c++) {
        if (Parts.IsInAlbum(c) != 0) {
            for (SLONG pass = 1; pass <= 3; pass++) {
                SLONG note = 0;
                if (pass == 1) {
                    note = gPlanePartRelations[Parts[c].ParentRelationId].Note1;
                }
                if (pass == 2) {
                    note = gPlanePartRelations[Parts[c].ParentRelationId].Note2;
                }
                if (pass == 3) {
                    note = gPlanePartRelations[Parts[c].ParentRelationId].Note3;
                }

                switch (note) {
                case NOTE_VERBRAUCH:
                    verbrauch += 2500;
                    break;
                case NOTE_VERBRAUCHXL:
                    verbrauch += 5000;
                    break;
                default:
                    break;
                }
            }
        }
    }

    /*if (verbrauch>0)
      {
    //Länger als 22 Stunden unterwegs?
    if (CalcTank()/verbrauch>22)
    verbrauch=CalcTank()/22;
    }*/

    return (verbrauch);
}

//--------------------------------------------------------------------------------------------
// Berechnet das Gewicht für ein Flugzeug:
//--------------------------------------------------------------------------------------------
SLONG CXPlane::CalcWeight() {
    SLONG weight = 0;

    for (SLONG c = 0; c < Parts.AnzEntries(); c++) {
        if (Parts.IsInAlbum(c) != 0) {
            weight += GetPlaneBuild(Parts[c].Shortname).Weight;
        }
    }

    return (weight);
}

//--------------------------------------------------------------------------------------------
// Berechnet die Kraft der Triebwerke für ein Flugzeug:
//--------------------------------------------------------------------------------------------
SLONG CXPlane::CalcPower() {
    SLONG power = 0;

    for (SLONG c = 0; c < Parts.AnzEntries(); c++) {
        if (Parts.IsInAlbum(c) != 0) {
            power += GetPlaneBuild(Parts[c].Shortname).Power;
        }
    }

    return (power);
}

//--------------------------------------------------------------------------------------------
// Berechnet die Kraft der Triebwerke für ein Flugzeug:
//--------------------------------------------------------------------------------------------
SLONG CXPlane::CalcNoise() {
    SLONG c = 0;
    SLONG noise = 0;

    for (c = 0; c < Parts.AnzEntries(); c++) {
        if (Parts.IsInAlbum(c) != 0) {
            noise += GetPlaneBuild(Parts[c].Shortname).Noise;
        }
    }

    for (c = 0; c < Parts.AnzEntries(); c++) {
        if (Parts.IsInAlbum(c) != 0) {
            noise += gPlanePartRelations[Parts[c].ParentRelationId].Noise;
        }
    }

    return (noise);
}

//--------------------------------------------------------------------------------------------
// Berechnet die Wartungsintensität:
//--------------------------------------------------------------------------------------------
SLONG CXPlane::CalcWartung() {
    SLONG c = 0;
    SLONG wartung = 0;

    for (c = 0; c < Parts.AnzEntries(); c++) {
        if (Parts.IsInAlbum(c) != 0) {
            wartung += GetPlaneBuild(Parts[c].Shortname).Wartung;
        }
    }

    for (c = 0; c < Parts.AnzEntries(); c++) {
        if (Parts.IsInAlbum(c) != 0) {
            for (SLONG pass = 1; pass <= 3; pass++) {
                SLONG note = 0;
                if (pass == 1) {
                    note = gPlanePartRelations[Parts[c].ParentRelationId].Note1;
                }
                if (pass == 2) {
                    note = gPlanePartRelations[Parts[c].ParentRelationId].Note2;
                }
                if (pass == 3) {
                    note = gPlanePartRelations[Parts[c].ParentRelationId].Note3;
                }

                switch (note) {
                case NOTE_KAPUTT:
                    wartung += 10;
                    break;
                case NOTE_KAPUTTXL:
                    wartung += 20;
                    break;
                default:
                    break;
                }
            }
        }
    }

    return (wartung);
}

//--------------------------------------------------------------------------------------------
// Berechnet die Wartungsintensität:
//--------------------------------------------------------------------------------------------
SLONG CXPlane::CalcSpeed() {
    SLONG c = 0;
    SLONG speed = 0;

    // Power 6000...50000 wird zu kmh 270..1000
    speed = (CalcPower() - 6000) * (1000 - 270) / (50000 - 6000) + 270;
    if (CalcPower() == 0) {
        speed = 0;
    }

    for (c = 0; c < Parts.AnzEntries(); c++) {
        if (Parts.IsInAlbum(c) != 0) {
            for (SLONG pass = 1; pass <= 3; pass++) {
                SLONG note = 0;
                if (pass == 1) {
                    note = gPlanePartRelations[Parts[c].ParentRelationId].Note1;
                }
                if (pass == 2) {
                    note = gPlanePartRelations[Parts[c].ParentRelationId].Note2;
                }
                if (pass == 3) {
                    note = gPlanePartRelations[Parts[c].ParentRelationId].Note3;
                }

                switch (note) {
                case NOTE_SPEED300:
                    if (speed > 300) {
                        speed = (speed * 2 + 300) / 3;
                    }
                    break;
                case NOTE_SPEED400:
                    if (speed > 400) {
                        speed = (speed * 2 + 400) / 3;
                    }
                    break;
                case NOTE_SPEED500:
                    if (speed > 500) {
                        speed = (speed * 2 + 500) / 3;
                    }
                    break;
                case NOTE_SPEED600:
                    if (speed > 600) {
                        speed = (speed * 2 + 600) / 3;
                    }
                    break;
                case NOTE_SPEED700:
                    if (speed > 700) {
                        speed = (speed * 2 + 700) / 3;
                    }
                    break;
                case NOTE_SPEED800:
                    if (speed > 800) {
                        speed = (speed * 2 + 800) / 3;
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }

    return (speed);
}

//--------------------------------------------------------------------------------------------
// Gibtr ggf. true zurück
//--------------------------------------------------------------------------------------------
bool CXPlane::IstPartVorhanden(CString Shortname, bool bOnlyThisType) {
    SLONG c = 0;

    if (!bOnlyThisType) {
        for (c = 0; c < Parts.AnzEntries(); c++) {
            if ((Parts.IsInAlbum(c) != 0) && (Parts[c].Shortname == Shortname || (Parts[c].Shortname[0] == Shortname[0] && Shortname[1] == '*'))) {
                return (true);
            }
        }

        return (false);
    }

    for (c = 0; c < Parts.AnzEntries(); c++) {
        if ((Parts.IsInAlbum(c) != 0) && Parts[c].Shortname != Shortname && Parts[c].Shortname[0] == Shortname[0]) {
            return (false);
        }
    }

    return (IstPartVorhanden(Shortname));
}

//--------------------------------------------------------------------------------------------
// Kann dieses Flugzeug gebaut werden?
//--------------------------------------------------------------------------------------------
bool CXPlane::IsBuildable() {
    if (!IstPartVorhanden("B*")) {
        return (false);
    }
    if (!IstPartVorhanden("C*")) {
        return (false);
    }
    if (!IstPartVorhanden("H*")) {
        return (false);
    }
    if (!IstPartVorhanden("R*")) {
        return (false);
    }
    if (!IstPartVorhanden("M*")) {
        return (false);
    }

    if (!GetError().empty()) {
        return (false);
    }

    return (true);
}

//--------------------------------------------------------------------------------------------
// Gibt eine Fehlerbeschreibung zurück:
//--------------------------------------------------------------------------------------------
CString CXPlane::GetError() {
    SLONG c = 0;
    SLONG d = 0;

    if (!IstPartVorhanden("B*")) {
        return ("");
    }
    if (!IstPartVorhanden("C*")) {
        return ("");
    }
    if (!IstPartVorhanden("H*")) {
        return ("");
    }
    if (!IstPartVorhanden("R*")) {
        return ("");
    }
    if (!IstPartVorhanden("M*")) {
        return ("");
    }

    // Symetrisch 1/2?
    for (c = 0; c < Parts.AnzEntries(); c++) {
        if ((Parts.IsInAlbum(c) != 0) && Parts[c].Shortname[0] == 'M' &&
            gPlaneBuilds[gPlanePartRelations[Parts[c].ParentRelationId].FromBuildIndex].Shortname[0] == 'R') {
            for (d = 0; d < Parts.AnzEntries(); d++) {
                if ((Parts.IsInAlbum(d) != 0) && Parts[d].Shortname[0] == 'M' &&
                    gPlaneBuilds[gPlanePartRelations[Parts[d].ParentRelationId].FromBuildIndex].Shortname[0] == 'L') {
                    if (gPlanePartRelations[Parts[c].ParentRelationId].Id + 10 == gPlanePartRelations[Parts[d].ParentRelationId].Id) {
                        break;
                    }
                }
            }

            if (d == Parts.AnzEntries()) {
                return (StandardTexte.GetS(TOKEN_MISC, 8300));
            }
        }
    }

    // Symetrisch 2/2?
    for (c = 0; c < Parts.AnzEntries(); c++) {
        if ((Parts.IsInAlbum(c) != 0) && Parts[c].Shortname[0] == 'M' &&
            gPlaneBuilds[gPlanePartRelations[Parts[c].ParentRelationId].FromBuildIndex].Shortname[0] == 'L') {
            for (d = 0; d < Parts.AnzEntries(); d++) {
                if ((Parts.IsInAlbum(d) != 0) && Parts[d].Shortname[0] == 'M' &&
                    gPlaneBuilds[gPlanePartRelations[Parts[d].ParentRelationId].FromBuildIndex].Shortname[0] == 'R') {
                    if (gPlanePartRelations[Parts[c].ParentRelationId].Id - 10 == gPlanePartRelations[Parts[d].ParentRelationId].Id) {
                        break;
                    }
                }
            }

            if (d == Parts.AnzEntries()) {
                return (StandardTexte.GetS(TOKEN_MISC, 8300));
            }
        }
    }

    // Triebwerke kräftig genug?
    if (CalcPower() * 4 < CalcWeight()) {
        return (StandardTexte.GetS(TOKEN_MISC, 8301));
    }

    // Tragflächen groß genug?
    /*if (IstPartVorhanden ("R5"))
      if (IstPartVorhanden ("B2") || IstPartVorhanden ("B4") || IstPartVorhanden ("B5"))
      return (StandardTexte.GetS (TOKEN_MISC, 8302));

      if (IstPartVorhanden ("R4") && IstPartVorhanden ("B2"))
      return (StandardTexte.GetS (TOKEN_MISC, 8302));*/

    return ("");
}

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
void CXPlane::Load(const CString &Filename) {
    TEAKFILE InputFile(Filename, TEAKFILE_READ);

    if (InputFile.GetFileLength() == 0) {
        Clear();
    } else {
        InputFile >> (*this);
    }
}

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
void CXPlane::Save(const CString &Filename) {
    TEAKFILE OutputFile(Filename, TEAKFILE_WRITE);

    OutputFile << (*this);
}

//--------------------------------------------------------------------------------------------
// Speichert ein CXPlane-Objekt:
//--------------------------------------------------------------------------------------------
TEAKFILE &operator<<(TEAKFILE &File, const CXPlane &p) {
    auto *pp = const_cast<CXPlane *>(&p);

    DWORD dwSize = sizeof(DWORD) + sizeof(SLONG) * 6 + strlen(p.Name);
    SLONG lCost = pp->CalcCost();
    SLONG lWeight = pp->CalcWeight();
    SLONG lConsumption = pp->CalcVerbrauch();
    SLONG lNoise = pp->CalcNoise();
    SLONG lReliability = pp->CalcVerbrauch();
    SLONG lSpeed = pp->CalcSpeed();
    SLONG lReichweite = pp->CalcReichweite();

    File << dwSize << lCost << lWeight << lConsumption << lNoise << lReliability << lSpeed << lReichweite;
    File.Write((const UBYTE *)(LPCTSTR)(p.Name), strlen(p.Name) + 1);

    File << p.Name << p.Cost << p.Parts;

    return (File);
}

//--------------------------------------------------------------------------------------------
// Lädt ein CXPlane-Objekt:
//--------------------------------------------------------------------------------------------
TEAKFILE &operator>>(TEAKFILE &File, CXPlane &p) {
    DWORD dwSize = 0;
    SLONG lCost = 0;
    SLONG lWeight = 0;
    SLONG lConsumption = 0;
    SLONG lNoise = 0;
    SLONG lReliability = 0;
    SLONG lSpeed = 0;

    char Dummy[8192];

    File >> dwSize >> lCost >> lWeight >> lConsumption >> lNoise >> lReliability >> lSpeed;
    dwSize -= sizeof(DWORD) + sizeof(SLONG) * 5;
    File.Read(reinterpret_cast<UBYTE *>(Dummy), dwSize + 1);

    File >> p.Name;

    File >> p.Cost >> p.Parts;

    return (File);
}

//--------------------------------------------------------------------------------------------
// ::
//--------------------------------------------------------------------------------------------
// Gibt das passende Build zum Shortname zurück:
//--------------------------------------------------------------------------------------------
SLONG GetPlaneBuildIndex(const std::string &Shortname) {
    auto it = gPlaneBuildsHash.find(Shortname);
    if (it != gPlaneBuildsHash.end()) {
        return it->second;
    }

    TeakLibW_Exception(FNL, ExcNever);
    return (-1);
}

//--------------------------------------------------------------------------------------------
// Gibt das passende Build zum Shortname zurück:
//--------------------------------------------------------------------------------------------
CPlaneBuild &GetPlaneBuild(const std::string &Shortname) {
    auto it = gPlaneBuildsHash.find(Shortname);
    if (it != gPlaneBuildsHash.end()) {
        return gPlaneBuilds[it->second];
    }

    TeakLibW_Exception(FNL, ExcNever);
    return gPlaneBuilds[0];
}

//--------------------------------------------------------------------------------------------
// Blittet das Flugzeug (Footpos):
//--------------------------------------------------------------------------------------------
void CXPlane::BlitPlaneAt(SBPRIMARYBM &TargetBm, SLONG Size, XY Pos, SLONG OwningPlayer) {

    switch (Size) {
    // Auf Runway:
    case 1:
        TargetBm.BlitFromT(gUniversalPlaneBms[OwningPlayer], Pos.x - gUniversalPlaneBms[OwningPlayer].Size.x / 2,
                           Pos.y - gUniversalPlaneBms[OwningPlayer].Size.y);
        break;

        // Hinter Glas:
    case 2: {
        Parts.IsInAlbum(SLONG(0x01000000));
        for (SLONG d = 0; d < Parts.AnzEntries(); d++) {
            if (Parts.IsInAlbum(d) != 0) {
                SBBM &qBm = gEditorPlane2dBms[GetPlaneBuild(Parts[d].Shortname).BitmapIndex];
                XY p = Parts[d].Pos2d;

                TargetBm.BlitFromT(qBm, Pos + p);
            }
        }
    } break;
    default:
        hprintf("Editor.cpp: Default case should not be reached.");
        DebugBreak();
    }
}

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
void CXPlane::operator=(const CXPlane &plane) {
    TEAKFILE f;

    if (plane.Parts.AnzEntries() > 0) {
        f.Announce(30000);

        f << plane;
        f.MemPointer = 0;

        if (this->Cost != -1) {
            Clear();
        }

        Parts.ReSize(100);
        f >> (*this);
    }
}

//--------------------------------------------------------------------------------------------
// Zerlegt das Ganze anhand von Semikolons:
//--------------------------------------------------------------------------------------------
void ParseTokens(char *String, char *tokens[], SLONG nTokens) {
    SLONG c = 0;
    SLONG n = 0;

    for (c = 0; c < nTokens; c++) {
        tokens[c] = nullptr;
    }

    tokens[n] = String;
    while (*String != 0) {
        if (*String == ';' && n < nTokens - 1) {
            *String = 0;
            n++;

            tokens[n] = String + 1;
        }

        String++;
    }
}

//--------------------------------------------------------------------------------------------
// Konvertiert die Daten aus dem String
//--------------------------------------------------------------------------------------------
void CPlaneBuild::FromString(const CString &str) {
    char *tokens[9];

    ParseTokens(const_cast<char *>((LPCTSTR)str), tokens, 9);

    Cost = atol(tokens[2]);
    Weight = atol(tokens[3]);
    Power = atol(tokens[4]);
    Noise = atol(tokens[5]);
    Wartung = atol(tokens[6]);
    Passagiere = atol(tokens[7]);
    Verbrauch = atol(tokens[8]);
}

//--------------------------------------------------------------------------------------------
// Konvertiert die Daten in einen String
//--------------------------------------------------------------------------------------------
CString CPlaneBuild::ToString() const {
    return (bprintf("%li;%s;%li;%li;%li;%li;%li;%li;%li", Id, Shortname, Cost, Weight, Power, Noise, Wartung, Passagiere, Verbrauch));
}

//--------------------------------------------------------------------------------------------
// Konvertiert die Daten aus dem String
//--------------------------------------------------------------------------------------------
void CPlanePartRelation::FromString(const CString &str) {
    char *tokens[11];

    ParseTokens(const_cast<char *>((LPCTSTR)str), tokens, 11);

    Offset2d.x = atol(tokens[3]);
    Offset2d.y = atol(tokens[4]);
    Offset3d.x = atol(tokens[5]);
    Offset3d.y = atol(tokens[6]);
    Note1 = atol(tokens[7]);
    Note2 = atol(tokens[8]);
    Note3 = atol(tokens[9]);
    Noise = atol(tokens[10]);
}

//--------------------------------------------------------------------------------------------
// Konvertiert die Daten in einen String
//--------------------------------------------------------------------------------------------
CString CPlanePartRelation::ToString() const {
    LPCTSTR n1 = "-";
    LPCTSTR n2 = "-";

    if (FromBuildIndex != -1) {
        n1 = gPlaneBuilds[FromBuildIndex].Shortname;
    }
    if (FromBuildIndex != -1) {
        n2 = gPlaneBuilds[ToBuildIndex].Shortname;
    }

    return (bprintf("%li;%s;%s;%li;%li;%li;%li;%li;%li;%li;%li", Id, n1, n2, Offset2d.x, Offset2d.y, Offset3d.x, Offset3d.y, Note1, Note2, Note3, Noise));
}
