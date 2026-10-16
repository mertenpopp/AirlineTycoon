#ifndef BOTWALK_H_
#define BOTWALK_H_

#include "class.h"
#include "defines.h"
#include "TeakLibW.h"

#include <vector>

class BotWalk {
  public:
    explicit BotWalk(PLAYER &player) : qPlayer(player) {}

    void startNewDay() {
        mGlueTriesToday = 0;
        mBombTriesToday = 0;
        mGlueDroppedToday = false;
        mStinkBombDroppedToday = false;
    }
    void setInExecuteAction(bool val) { mInExecuteAction = val; }

    /* Whether a drop of the item (ITEM_GLUE or ITEM_STINKBOMBE) can be started now. */
    bool wantItemDrop(SLONG item) const;
    /* Walks to a spot of the first victim that has one and drops the item there. False if no
     * walk was started; that still counts as one of the day's tries. Call it from
     * RobotExecuteAction() for ACTION_DROP_GLUE / ACTION_DROP_BOMB. */
    bool startItemDrop(const std::vector<SLONG> &victims, SLONG item);
    void tickItemDrop(bool isOnThePhone);

    friend TEAKFILE &operator<<(TEAKFILE &File, const BotWalk &bot);
    friend TEAKFILE &operator>>(TEAKFILE &File, BotWalk &bot);

  private:
    XY getPosition() const;
    XY getPlate() const;
    SLONG roomAtPosition(XY position) const;
    SLONG roomAtPlate(XY plate) const;
    bool isWalking() const;
    void stopWalking();
    SLONG estimateWalkTicks(XY plate, bool run) const;
    void leaveRoomsForWalk();
    bool walkToPlate(XY plate, SLONG holdTicks = -1, bool run = true);

    bool findStenchPlate(SLONG victim, XY &outPlate) const;
    bool findGluePlates(SLONG victim, XY &outApproach, XY &outDrop) const;

    void abortItemDrop(const char *why);

    PLAYER &qPlayer;
    bool mInExecuteAction{false};
    SLONG mGlueTriesToday{0};
    SLONG mBombTriesToday{0};
    SLONG mGlueDrops{0};
    SLONG mStinkBombDrops{0};
    bool mStinkBombDroppedToday{false};
    bool mGlueDroppedToday{false};

    enum class DropStage { None, GlueApproach, GlueFinal, StinkBomb };
    /* The walk in flight, none of it serialised: the loader drops it, and the day's tries and
     * drop flags, which are saved, decide whether to go again. */
    DropStage mDropStage{DropStage::None};
    XY mDropPlate{-1, -1}; /* the plate we are walking to now */
    XY mDropFinal{-1, -1}; /* glue: the plate to step onto for the drop */
    SLONG mDropStart{0};   /* Sim.TimeSlice the walk began */
    SLONG mDropVictim{-1};
    SLONG mDropUseTries{0}; /* ticks spent at the spot trying to use the item */
};

TEAKFILE &operator<<(TEAKFILE &File, const BotWalk &bot);
TEAKFILE &operator>>(TEAKFILE &File, BotWalk &bot);

#endif // BOTWALK_H_
