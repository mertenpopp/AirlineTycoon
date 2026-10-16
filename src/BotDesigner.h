#ifndef BOT_DESIGNER_H_
#define BOT_DESIGNER_H_

#include "Sbbm.h"
#include "TeakLibW.h"
#include "class.h"

#include <queue>
#include <unordered_map>
#include <vector>

/* What a design is being optimised for. The first six are MertenBot's; the two Claude* goals are
 * ClaudeBot's own, kept separate so that tuning one never moves the other. */
enum class ScoreType { Standard, FastPassengers, FastFast, VIP, Miss05, Miss08, ClaudeMiss05, ClaudeMiss08 };

struct PartIter {
    LPCTSTR formatPrefix() const { return bprintf(prefix.c_str(), variant); }
    void resetPosition() { position = canBeOmitted ? -1 : 0; }
    void resetVariant() { variant = minVariant; }

    /* static */
    std::string prefix; /* e.g. 'M%d' for engines */
    SLONG minVariant{};
    SLONG maxVariant{};
    bool canBeOmitted{};

    /* iterators */
    SLONG variant{};  /* the number behind the prefix */
    SLONG position{}; /* which position */
};

class BotDesigner {
  public:
    BotDesigner();

    /* F7: searches for all goals, saves every winner to myplanes/ and logs it. */
    SLONG findBestDesignerPlane();

    /* One goal, no files and no logging: returns the single best design. Used by ClaudeBot, which
     * needs one plane for the mission it is playing rather than a catalogue. */
    bool findBestPlaneForGoal(ScoreType goal, CXPlane &outPlane);

  private:
    enum class FailedWhy { NoMorePositions, InvalidPosition, DidNotFail };

    /* Runs the enumeration once, scoring every buildable design against each goal. Returns the
     * winner per goal, index-aligned with `goals`; an entry is left empty if nothing qualified. */
    std::vector<CXPlane> runSearch(const std::vector<ScoreType> &goals, bool saveAndPrint);

    bool buildPart(CXPlane &plane, const PartIter &part) const;
    std::pair<FailedWhy, SLONG> buildPlane(CXPlane &plane) const;

    std::vector<PartIter> mPartIter;
    SLONG mPrimaryEnginesIdx{-1};
    SLONG mSecondaryEnginesIdx{-1};
    SLONG mPrimaryAuxEnginesIdx{-1};
    SLONG mSecondaryAuxEnginesIdx{-1};
    SBBMS mPartBms;

    std::unordered_map<SLONG, std::vector<int>> mPlaneRelations;

    /* Passenger floor for the current search, 0 when the goals do not impose one. Only hulls
     * carry passengers in the shipped table, but mMaxNonHullPax is read off the live data so the
     * prune stays exact if another builds.csv puts seats elsewhere. */
    SLONG mMinPassagiere{0};
    SLONG mMaxNonHullPax{0};

    /* Scratch buffer for buildPart(): the build index of each slot in the plane's parts album.
     * mutable because buildPart() is const and this is a cache, not state. */
    mutable std::vector<SLONG> mPlacedBuildIndex;
};

#endif // BOT_DESIGNER_H_
