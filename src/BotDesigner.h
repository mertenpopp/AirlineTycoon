#ifndef BOT_DESIGNER_H_
#define BOT_DESIGNER_H_

#include "Sbbm.h"
#include "TeakLibW.h"
#include "class.h"

#include <queue>
#include <unordered_map>
#include <vector>

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

    SLONG findBestDesignerPlane();

  private:
    enum class FailedWhy { NoMorePositions, InvalidPosition, DidNotFail };

    bool buildPart(CXPlane &plane, const PartIter &part) const;
    std::pair<FailedWhy, SLONG> buildPlane(CXPlane &plane) const;

    std::vector<PartIter> mPartIter;
    SLONG mPrimaryEnginesIdx{-1};
    SLONG mSecondaryEnginesIdx{-1};
    SLONG mPrimaryAuxEnginesIdx{-1};
    SLONG mSecondaryAuxEnginesIdx{-1};
    SBBMS mPartBms;

    std::unordered_map<SLONG, std::vector<int>> mPlaneRelations;

    /* Scratch buffer for buildPart(): the build index of each slot in the plane's parts album.
     * mutable because buildPart() is const and this is a cache, not state. */
    mutable std::vector<SLONG> mPlacedBuildIndex;
};

#endif // BOT_DESIGNER_H_
