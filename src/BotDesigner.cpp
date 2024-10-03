#include "BotDesigner.h"

#include "Editor.h"
#include "Proto.h"
#include "TeakLibW.h"
#include "class.h"
#include "defines.h"
#include "global.h"
#include "sbl.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

template <class... Types> void AT_Log(Types... args) { AT_Log_I("Bot", args...); }

extern std::vector<CPlanePartRelation> gPlanePartRelations;

const SLONG kMaxBest = 5;
enum class ScoreType { Standard, FastPassengers, FastFast, VIP, Miss05, Miss08 };

class PlaneCandidate {
  public:
    PlaneCandidate() = default;
    PlaneCandidate(const CXPlane &plane, const std::vector<PartIter> &config, ScoreType type) : mPlane(plane), mPartIter(config), mScoreType(type) {
        calculatePlaneScore();
    }

    void printPlaneInfo() {
        AT_Log("*** CXPlane %s ***", mPlane.Name.c_str());
        std::stringstream ss;
        for (const auto &iter : mPartIter) {
            ss << iter.formatPrefix() << "@" << iter.position << ", ";
        }
        AT_Log("Configuration: %s", ss.str().c_str());
        AT_Log("Score = %f (BaseScore = %f)", mScore, mBaseScore);
        AT_Log("Passagiere = %d", mPlane.CalcPassagiere());
        AT_Log("Reichweite = %d", mPlane.CalcReichweite());
        AT_Log("Verbrauch = %d", mPlane.CalcVerbrauch());
        AT_Log("Geschwindigkeit = %d", mPlane.CalcSpeed());
        AT_Log("Lärm = %d", mPlane.CalcNoise());
        AT_Log("Wartung = %d", mPlane.CalcWartung());
        AT_Log("Preis = %s", Insert1000erDots(mPlane.CalcCost()).c_str());
    }

    void setPlaneName(SLONG number) {
        number += 1;
        switch (mScoreType) {
        case ScoreType::Standard:
            mPlane.Name = bprintf("Bot Standard %d", number);
            break;
        case ScoreType::FastPassengers:
            mPlane.Name = bprintf("Bot Jet %d", number);
            break;
        case ScoreType::FastFast:
            mPlane.Name = bprintf("Bot Speedmaster %d", number);
            break;
        case ScoreType::VIP:
            mPlane.Name = bprintf("Bot VIP %d", number);
            break;
        case ScoreType::Miss05:
            mPlane.Name = bprintf("Bot Beluga %d", number);
            break;
        case ScoreType::Miss08:
            mPlane.Name = bprintf("Bot Ecomaster %d", number);
            break;
        default:
            mPlane.Name = bprintf("Bot %d", number);
            break;
        }
    }

    void save(SLONG number) {
        CString fileName;
        number += 1;
        switch (mScoreType) {
        case ScoreType::Standard:
            fileName = FullFilename(bprintf("botplane_std_%d.plane", number), MyPlanePath);
            break;
        case ScoreType::FastPassengers:
            fileName = FullFilename(bprintf("botplane_jet_%d.plane", number), MyPlanePath);
            break;
        case ScoreType::FastFast:
            fileName = FullFilename(bprintf("botplane_speed_%d.plane", number), MyPlanePath);
            break;
        case ScoreType::VIP:
            fileName = FullFilename(bprintf("botplane_vip_%d.plane", number), MyPlanePath);
            break;
        case ScoreType::Miss05:
            fileName = FullFilename(bprintf("botplane_atfs05_%d.plane", number), MyPlanePath);
            break;
        case ScoreType::Miss08:
            fileName = FullFilename(bprintf("botplane_atfs08_%d.plane", number), MyPlanePath);
            break;
        default:
            fileName = FullFilename(bprintf("botplane_unknown_%d.plane", number), MyPlanePath);
            break;
        }
        mPlane.Save(fileName);
    }

    bool operator<(const PlaneCandidate &other) const { return mScore > other.mScore; }

    bool operator==(const PlaneCandidate &other) const {
        if (mScoreType != other.mScoreType) {
            return false;
        }
        if (reichweite != other.reichweite) {
            return false;
        }
        if (speed != other.speed) {
            return false;
        }
        if (passagiere != other.passagiere) {
            return false;
        }
        if (verbrauch != other.verbrauch) {
            return false;
        }
        if (preis != other.preis) {
            return false;
        }
        if (noise != other.noise) {
            return false;
        }
        if (std::abs(wartung - other.wartung) > 0.01) {
            return false;
        }
        assert(std::abs(mScore - other.mScore) < 0.01);
        return true;
    }

  private:
    void calculatePlaneScore() {
        speed = mPlane.CalcSpeed();
        passagiere = mPlane.CalcPassagiere();
        verbrauch = mPlane.CalcVerbrauch();
        /* optimization: Replicate formula in mPlane.CalcReichweite() */
        reichweite = mPlane.CalcTank() / verbrauch * speed;
        preis = mPlane.CalcCost();
        noise = std::max(0, std::min(mPlane.CalcNoise(), 111) - 60); /* can't get worse than 111, no impact when 60 or smaller */
        wartung = (mPlane.CalcWartung() + 100.0) / 100.0;

        mScore = 1.0; /* multiplication (geometric mean) because values have wildly different ranges */

        /* basis score */
        mScore *= reichweite;
        mScore /= sqrt(1 + noise / 10);
        assert(!std::isinf(mScore));
        mScore /= sqrt(1 + wartung);
        assert(!std::isinf(mScore));

        mBaseScore = mScore;

        /* more passengers or more speed? */
        if (mScoreType == ScoreType::VIP) {
            /* we only need to transport 1-2 people (rare jobs with a high premium) */
            mScore *= speed;
        } else {
            mScore *= passagiere;
        }

        /* even more speed? */
        if (mScoreType == ScoreType::FastFast) {
            mScore *= speed;
            mScore *= speed;
        }

        /* optimize for routes */
        if (mScoreType == ScoreType::FastPassengers) {
            mScore *= passagiere;
            mScore *= speed;
        }

        /* economical considerations */
        if (mScoreType != ScoreType::FastFast) {
            mScore /= sqrt(preis); /* one time cost not that important */
            mScore /= verbrauch;
        }

        if (mScoreType == ScoreType::Miss05) {
            if (passagiere < BTARGET_PLANESIZE) {
                mScore = 0;
            }
        }

        if (mScoreType == ScoreType::Miss08) {
            if (verbrauch * 100 / speed > BTARGET_VERBRAUCH) {
                mScore = 0;
            }
        }
    }

    CXPlane mPlane;
    std::vector<PartIter> mPartIter;

    ScoreType mScoreType{ScoreType::Standard};
    DOUBLE mScore{-1};
    DOUBLE mBaseScore{-1};

    SLONG reichweite{};
    SLONG speed{};
    SLONG passagiere{};
    SLONG verbrauch{};
    SLONG preis{};
    SLONG noise{};
    DOUBLE wartung{};
};

BotDesigner::BotDesigner() {
    mPartIter = {{"B%d", 1, NUM_PLANE_BODY, false},
                 {"R%d", 1, NUM_PLANE_LWING, false},
                 {"C%d", 1, NUM_PLANE_COCKPIT, false},
                 {"H%d", 1, NUM_PLANE_HECK, false},
                 {"M%d", 1, NUM_PLANE_MOT, true},
                 {"M%d", 1, NUM_PLANE_MOT, false},
                 {"M%d", 5, 5, true},
                 {"M%d", 5, 5, true}};
    mSecondaryEnginesIdx = 4;
    mPrimaryEnginesIdx = 5;
    mSecondaryAuxEnginesIdx = 6;
    mPrimaryAuxEnginesIdx = 7;

    for (auto &iter : mPartIter) {
        iter.resetPosition();
        iter.variant = iter.minVariant;
    }

    CString GfxLibName = "editor.gli";
    GfxLib *pRoomLib = nullptr;
    pGfxMain->LoadLib(FullFilename(GfxLibName, RoomPath).c_str(), &pRoomLib, L_LOCMEM);
    mPartBms.ReSize(pRoomLib, "BODY_A01 BODY_A02 BODY_A03 BODY_A04 BODY_A05 "
                              "CPIT_A01 CPIT_A02 CPIT_A03 CPIT_A04 CPIT_A05 "
                              "HECK_A01 HECK_A02 HECK_A03 HECK_A04 HECK_A05 HECK_A06 HECK_A07 "
                              "RWNG_A01 RWNG_A02 RWNG_A03 RWNG_A04 RWNG_A05 RWNG_A06 "
                              "MOT_A01  MOT_A02  MOT_A03  MOT_A04  MOT_A05  MOT_A06  MOT_A07  MOT_A08 "
                              "LWNG_A01 LWNG_A02 LWNG_A03 LWNG_A04 LWNG_A05 LWNG_A06 ");

    for (SLONG c = 0; c < gPlanePartRelations.size(); c++) {
        const auto &qRelation = gPlanePartRelations[c];
        auto key = qRelation.ToBuildIndex;

        /* we drop the slot Ml (add to right wing instead, will be symetrically matched anyhow) */
        if (strcmp(qRelation.Slot, "Ml") == 0) {
            continue;
        }

        /* we drop any relations to the left wing (add to right wing instead, will be symetrically matched anyhow) */
        auto attachTo = qRelation.FromBuildIndex;
        if (attachTo >= GetPlaneBuildIndex("L1") && attachTo <= GetPlaneBuildIndex("L6")) {
            continue;
        }

        auto it = mPlaneRelations.find(key);
        if (it == mPlaneRelations.end()) {
            mPlaneRelations[key] = {c};
            continue;
        }

        /* only add relation with same 'to' part if it is a different part or results in different effects */
        bool canAdd = true;
        for (auto &idx : it->second) {
            const auto &existing = gPlanePartRelations[idx];
            if (qRelation.FromBuildIndex != existing.FromBuildIndex) {
                continue;
            }
            if (qRelation.Note1 != existing.Note1 || qRelation.Note2 != existing.Note2 || qRelation.Note3 != existing.Note3) {
                continue;
            }
            if (strcmp(qRelation.Slot, existing.Slot) != 0) {
                continue;
            }
            if (strcmp(qRelation.RulesOutSlots, existing.RulesOutSlots) != 0) {
                continue;
            }
            if (qRelation.Noise < existing.Noise) {
                idx = c; /* replace with lower noise relation, no need to add */
            }
            canAdd = false;
            break;
        }
        if (canAdd) {
            it->second.push_back(c);
        } else {
            AT_Log("Dropping relation %d", qRelation.Id);
        }
    }

    SLONG count = 0;
    for (const auto &iter : mPlaneRelations) {
        count += iter.second.size();
    }
    AT_Log("Reduced from %d to %d relations.", gPlanePartRelations.size(), count);
}

bool BotDesigner::buildPart(CXPlane &plane, const PartIter &partIter) const {
    CString PartUnderCursor = partIter.formatPrefix();
    CString PartUnderCursorB{};

    /* taken from Editor:: CEditor::OnLButtonDown() */

    if (!PartUnderCursor.empty()) {
        if (PartUnderCursor[0] == 'L') {
            PartUnderCursorB = CString("R") + PartUnderCursor[1];
        } else if (PartUnderCursor[0] == 'R') {
            PartUnderCursorB = CString("L") + PartUnderCursor[1];
        } else if (PartUnderCursor[0] == 'M') {
            PartUnderCursorB = PartUnderCursor;
        }
    }

    /* taken from Editor:: CEditor::OnPaint() */

    SLONG GripRelation = -1;
    SLONG GripRelationB = -1;
    SLONG GripRelationPart = -1;
    XY GripAtPos{};
    XY GripAtPosB{};
    XY GripAtPos2d{};
    XY GripAtPosB2d{};

    // Für alle Relations:
    SLONG position = partIter.position;
    auto it = mPlaneRelations.find(GetPlaneBuildIndex(PartUnderCursor));
    assert(it != mPlaneRelations.end());
    for (SLONG c : it->second) {
        const auto &qRelation = gPlanePartRelations[c];
        if (!plane.Parts.IsSlotFree(qRelation.Slot)) {
            continue;
        }

        XY GripToSpot(-9999, -9999);
        XY GripToSpot2d(-9999, -9999);

        // Ist die Von-Seite ein Part oder der Desktop?
        if (qRelation.FromBuildIndex == -1) {
            GripToSpot = qRelation.Offset3d - mPartBms[GetPlaneBuild(PartUnderCursor).BitmapIndex].Size / 2;
            GripToSpot2d = qRelation.Offset2d;

            if ((position--) == 0) {
                GripAtPos = GripToSpot;
                GripAtPos2d = GripToSpot2d;
                GripRelation = c;
                GripRelationPart = -1;
            }
        } else {
            // Für alle eingebauten Planeparts:
            for (SLONG d = 0; d < plane.Parts.AnzEntries(); d++) {
                if (plane.Parts.IsInAlbum(d) == 0) {
                    continue;
                }

                const auto &qPart = plane.Parts[d];
                if (qRelation.FromBuildIndex != GetPlaneBuildIndex(qPart.Shortname)) {
                    continue;
                }

                GripToSpot = qPart.Pos3d + qRelation.Offset3d;
                GripToSpot2d = qPart.Pos2d + qRelation.Offset2d;

                if ((position--) == 0) {
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
                            assert(gPlanePartRelations[c + add].FromBuildIndex == qRelation.FromBuildIndex);
                        } else if (PartUnderCursor[0] == 'R') {
                            /* attach wing on other side of body as well. Relation is after current relation */
                            assert(gPlanePartRelations[c + add].FromBuildIndex == qRelation.FromBuildIndex);
                        } else if (PartUnderCursor[0] == 'M' && qPart.Shortname[0] == 'H') {
                            /* attaches engine to other side of tail. Relation is either before or after current
                             * relation */
                            if (c > 0 && gPlanePartRelations[c - 1].FromBuildIndex == qRelation.FromBuildIndex) {
                                add = -1;
                            }
                            assert(gPlanePartRelations[c + add].FromBuildIndex == qRelation.FromBuildIndex);
                        } else if (PartUnderCursor[0] == 'M') {
                            /* searching for other wing to attach second copy of engine. Relation is either before or after current relation
                             */
                            for (e = 0; e < plane.Parts.AnzEntries(); e++) {
                                if (e == d || plane.Parts.IsInAlbum(e) == 0) {
                                    continue;
                                }
                                if (plane.Parts[e].Shortname[0] == 'L' || plane.Parts[e].Shortname[0] == 'R') {
                                    break;
                                }
                            }
                            assert(e < plane.Parts.AnzEntries());
                            if (c > 0 && gPlanePartRelations[c - 1].ToBuildIndex == qRelation.ToBuildIndex) {
                                add = -1;
                            }
                            assert(gPlanePartRelations[c + add].ToBuildIndex == qRelation.ToBuildIndex);
                            assert(gPlanePartRelations[c + add].FromBuildIndex == GetPlaneBuildIndex(plane.Parts[e].Shortname));
                        }

                        GripAtPosB = plane.Parts[e].Pos3d + gPlanePartRelations[c + add].Offset3d;
                        GripAtPosB2d = plane.Parts[e].Pos2d + gPlanePartRelations[c + add].Offset2d;
                        GripRelationB = c + add;
                    }
                }
            }
        }
    }

    /* new code: need to check if a position was found */
    if (GripRelation == -1) {
        return false;
    }

    /* taken from Editor:: CEditor::OnLButtonUp() */

    CPlanePart part;
    part.Pos2d = GripAtPos2d;
    part.Pos3d = GripAtPos;
    part.Shortname = PartUnderCursor;
    part.ParentShortname = (GripRelationPart == -1) ? "" : plane.Parts[GripRelationPart].Shortname;
    part.ParentRelationId = GripRelation;
    plane.Parts += std::move(part);

    if (PartUnderCursor.Left(1) == "B" || PartUnderCursor.Left(1) == "C" || PartUnderCursor.Left(1) == "H" || PartUnderCursor.Left(1) == "L" ||
        PartUnderCursor.Left(1) == "R" || PartUnderCursor.Left(1) == "W") {
        PartUnderCursor = "";
    }

    if (GripRelationB != -1) {
        CPlanePart part;
        part.Pos2d = GripAtPosB2d;
        part.Pos3d = GripAtPosB;
        part.Shortname = PartUnderCursorB;
        part.ParentShortname = plane.Parts[GripRelationPart].Shortname;
        part.ParentRelationId = GripRelationB;
        plane.Parts += std::move(part);

        plane.Parts.Sort();
    }

    plane.Parts.Sort();
    return true;
}

std::pair<BotDesigner::FailedWhy, SLONG> BotDesigner::buildPlane(CXPlane &plane) const {
    /* check if build configuration makes sense */
    const auto &qFirstEngine = mPartIter[mPrimaryEnginesIdx];
    const auto &qSecondEngine = mPartIter[mSecondaryEnginesIdx];
    if (qFirstEngine.variant == qSecondEngine.variant) {
        if (qFirstEngine.position < qSecondEngine.position) {
            return {FailedWhy::InvalidPosition, mPrimaryEnginesIdx};
        }
    }
    const auto &qAuxEngine1 = mPartIter[mPrimaryAuxEnginesIdx];
    const auto &qAuxEngine2 = mPartIter[mSecondaryAuxEnginesIdx];
    if (qAuxEngine1.variant == qAuxEngine2.variant) {
        if (qAuxEngine1.position < qAuxEngine2.position) {
            return {FailedWhy::InvalidPosition, mPrimaryAuxEnginesIdx};
        }
    }

    for (SLONG i = 0; i < mPartIter.size(); i++) {
        const auto &iter = mPartIter[i];
        if (iter.position < 0) {
            continue;
        }
        CString prefix(iter.formatPrefix());
        plane.Name.append(bprintf("%s%d", prefix.c_str(), iter.position));
        if (!buildPart(plane, iter)) {
            return {FailedWhy::NoMorePositions, i};
        }
    }
    return {FailedWhy::DidNotFail, -1};
}

SLONG BotDesigner::findBestDesignerPlane() {
    std::vector<ScoreType> scoresToCheck{ScoreType::Standard, ScoreType::FastPassengers, ScoreType::FastFast,
                                         ScoreType::VIP,      ScoreType::Miss05,         ScoreType::Miss08};
    std::vector<std::vector<PlaneCandidate>> bestPlanes;
    bestPlanes.resize(scoresToCheck.size());
    for (auto &list : bestPlanes) {
        list.resize(kMaxBest + 1); /* last one is to store new candidates that are not confirmed yet */
    }

    bool endLoop = false;
    SLONG totalBuilds = 0;
    SLONG successfulBuilds = 0;
    while (!endLoop) {
        CXPlane plane{};
        plane.Parts.ReSize(mPartIter.size() * 2); /* times two because engines and wings come in pairs */

        FailedWhy failedWhy = FailedWhy::DidNotFail;
        SLONG failedOn = -1;
        std::tie(failedWhy, failedOn) = buildPlane(plane);
        totalBuilds++;

        if (failedWhy == FailedWhy::DidNotFail) {
            if (plane.IsBuildable()) {
                successfulBuilds++;
                if (successfulBuilds % 1000 == 0) {
                    AT_Log("Evaluated %d/%d builds so far!", successfulBuilds, totalBuilds);
                }

                for (SLONG i = 0; i < bestPlanes.size(); i++) {
                    bestPlanes[i][kMaxBest] = {plane, mPartIter, scoresToCheck[i]};

                    bool doSort = true;
                    for (int j = 0; j < kMaxBest; j++) {
                        if (bestPlanes[i][j] == bestPlanes[i][kMaxBest]) {
                            doSort = false;
                            break;
                        }
                    }
                    if (doSort) {
                        std::sort(bestPlanes[i].begin(), bestPlanes[i].end());
                        /* doSort == false:
                         * We leave the new candidate at last position where it will be overwritten by the next candidate */
                    }
                }
            }

            /* check next position for this variant of a part */
            mPartIter[mPartIter.size() - 1].position++;
            continue;
        }

        /* move to next position */
        mPartIter[failedOn].position++;
        for (int i = failedOn + 1; i < mPartIter.size(); i++) {
            mPartIter[i].resetVariant();
            mPartIter[i].resetPosition();
        }

        /* only this one position was forbidden, can continue */
        if (failedWhy == FailedWhy::InvalidPosition) {
            continue;
        }

        /* this was the last working position for this variant */
        assert(failedWhy == FailedWhy::NoMorePositions);
        mPartIter[failedOn].resetPosition();

        /* move on to next variant of part */
        mPartIter[failedOn].variant += 1;
        if (mPartIter[failedOn].variant <= mPartIter[failedOn].maxVariant) {
            continue;
        }

        mPartIter[failedOn].resetVariant(); /* reset to first variant and increment next part iterator */

        SLONG nextVariant = failedOn - 1;
        if (nextVariant < 0) {
            break;
        }
        mPartIter[nextVariant].position++;
    }

    for (auto &qPlanes : bestPlanes) {
        for (int j = 0; j < kMaxBest; j++) {
            auto &bestPlane = qPlanes[j];
            bestPlane.setPlaneName(j);
            bestPlane.save(j);
            bestPlane.printPlaneInfo();
        }
    }

    AT_Log("Evaluated %d/%d builds in total!", successfulBuilds, totalBuilds);

    return 0;
}
