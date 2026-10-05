#include "BotWalk.h"

#include "AtNet.h"
#include "Proto.h"
#include "global.h"
#include "GameMechanic.h"

#include <SDL_log.h>

template <class... Types> void AT_Error(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_ERROR, "Bot", args...); }
template <class... Types> void AT_Warn(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_WARN, "Bot", args...); }
template <class... Types> void AT_Info(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_INFO, "Bot", args...); }
template <class... Types> void AT_Log(Types... args) { AT_Log_I("Bot", args...); }

/* Shortest hold, so that a walk of a few plates is not abandoned before it starts. */
static const SLONG kMinWalkHoldTicks = 20;
/* Longest hold. RobotPump() ticks are 20 to the second at normal speed, so this is a minute of
 * wall clock standing about - far more than crossing the airport takes, and a bound on the
 * damage an unreachable target can do to the day. */
static const SLONG kMaxWalkHoldTicks = 20 * 60;

/* How long a drop walk may take in all before it is given up, in RobotPump() ticks. */
static const SLONG kMaxDropTicks = 2 * 20 * 60;
/* Ticks spent at the spot trying to use the item, or standing still short of it. */
static const SLONG kMaxDropStandTicks = 20;
/* Tries at a drop per day, so that a layout we cannot place an item in does not eat the day. */
static const SLONG kMaxDropTriesPerDay = 4;
/* No new drop from this hour on: the victim still has to walk into it before 18:00. */
static const SLONG kLastDropHour = 16;

/* Plate coordinates from position coordinates. Mirrors PLAYER::WalkToRoom() */
static XY plateFromPosition(XY position) {
    XY plate;
    plate.x = position.x / 44;
    if (position.y < 4000) {
        plate.y = position.y / 22 + 5; /* lower floor: plate rows 5..14 */
    } else {
        plate.y = (position.y - 5000 + 2200) / 22 - 100; /* upper floor: rows 0..4 */
    }
    return plate;
}

/* The inverse, the way PERSON::DoOnePlayerStep() turns TertiaryTarget back into a position */
static XY positionFromPlate(XY plate) {
    XY position;
    position.x = plate.x * 44 + 22;
    if (plate.y < 5) {
        position.y = plate.y * 22 + 5000 + 11;
    } else {
        position.y = (plate.y - 5) * 22 + 11;
    }
    return position;
}

/* Bit 2 of a plate says players may be on it at all, the top nibble which directions it can be
 * left in. Both have to be set, which is the test PLAYER::WalkToMouseClick() makes on the plate. */
static bool plateIsWalkable(XY plate) {
    if (plate.x < 0 || plate.x >= Airport.PlateDimension.x) {
        return false;
    }
    if (plate.y < 0 || plate.y >= Airport.PlateDimension.y) {
        return false;
    }
    const SLONG index = plate.y + (plate.x << 4);
    if (index < 0 || index >= Airport.iPlate.AnzEntries()) {
        return false;
    }
    const UBYTE flags = Airport.iPlate[index];
    return (flags & 4) != 0 && (flags & 0xf0) != 0;
}

/* Rows 3 and 15 are the two wall rows, one per floor: nothing stands there and nothing south of
 * them belongs to the same floor. Everything else follows PLAYER::WalkToMouseClick(). */
static bool resolvePlate(XY &plate) {
    if (plate.y < 0 || plate.y > 15) {
        return false;
    }

    /* Blocked? Then try south of it, like a click that lands on scenery. */
    while (plate.y != 3 && plate.y != 15) {
        if (plateIsWalkable(plate)) {
            break;
        }
        plate.y++;
        if (plate.y > 15) {
            return false;
        }
    }
    if (plate.y == 3 || plate.y == 15) {
        return false;
    }

    /* Upper floor: a plate with open floor all the way down to the wall row is a spot in mid
     * air over the hall rather than a place to stand (Player.cpp:2817-2829). */
    if (plate.y < 3) {
        SLONG y = plate.y;
        while (y < 3 && (Airport.iPlate[y + (plate.x << 4)] & 32) != 0) {
            y++;
        }
        if (y == 3) {
            return false;
        }
    }

    return true;
}

XY BotWalk::getPosition() const { return Sim.Persons[Sim.Persons.GetPlayerIndex(qPlayer.PlayerNum)].Position; }

XY BotWalk::getPlate() const { return plateFromPosition(getPosition()); }

/* PERSON::DoOnePlayerStep() looks for a room announcement one step ahead of the character and
 * walks it into whatever it finds, with the tolerance below. Room entrances are therefore not
 * places to stand: the character walks in instead. */
SLONG BotWalk::roomAtPosition(XY position) const { return Airport.GetRuneParNear(position, XY(qPlayer.WalkSpeed * 2, qPlayer.WalkSpeed * 2), RUNE_2SHOP); }

SLONG BotWalk::roomAtPlate(XY plate) const { return roomAtPosition(positionFromPlate(plate)); }

/* A free walk is one the airport machinery is running with no room at the end of it. */
bool BotWalk::isWalking() const { return (qPlayer.iWalkActive != 0) && qPlayer.DirectToRoom == 0 && qPlayer.GetRoom() == ROOM_AIRPORT; }

void BotWalk::stopWalking() {
    /* WalkStopEx() plants every target on the character's own plate, so nothing walks it on.
     * It refuses while the character is entering or leaving a room, which is also the one
     * moment where there is no free walk to stop. */
    qPlayer.WalkStopEx();
    qPlayer.WorkCountdown = 0;
}

SLONG BotWalk::estimateWalkTicks(XY plate, bool run) const {
    const XY here = getPosition();
    const XY there = positionFromPlate(plate);

    /* PLAYER::WalkToRoom()'s own estimate of a walk (Player.cpp:2656-2666): the Manhattan
     * distance in position units over the walking speed, with the 5000 units between the two
     * floors taken back out because the staircase is not walked in a straight line. Running
     * takes two steps a tick (Person.cpp:3370-3384). */
    SLONG distance = std::abs(here.x - there.x) + std::abs(here.y - there.y);
    if (std::abs(here.y - there.y) > 4600) {
        distance -= 4600;
    }
    SLONG ticks = std::max<SLONG>(1, distance / std::max<SLONG>(1, qPlayer.WalkSpeed));
    if (run) {
        ticks /= 2;
    }

    /* Half again on top: the estimate is a straight line, while the character walks around
     * whatever stands in the way and up and down a staircase. The hold has to outlast the
     * walk, or RobotPump() takes over halfway there and carries on to the next room. */
    ticks = ticks * 3 / 2 + kMinWalkHoldTicks;
    return std::min(kMaxWalkHoldTicks, std::max(kMinWalkHoldTicks, ticks));
}

/* Leaves every real room we are in, so that the walk below has the airport to walk in. */
void BotWalk::leaveRoomsForWalk() {
    bool bLeft = false;
    for (SLONG c = 9; c >= 0; c--) {
        const UWORD room = UWORD(qPlayer.Locations[c] & ~(ROOM_ENTERING | ROOM_LEAVING));
        if (room != 0 && room != ROOM_AIRPORT) {
            qPlayer.Locations[c] |= ROOM_LEAVING;
            bLeft = true;
        }
    }

    if (bLeft) {
        qPlayer.BroadcastRooms(ATNET_LEAVEROOM);
    }
}

bool BotWalk::walkToPlate(XY plate, SLONG holdTicks, bool run) {
    const XY requested = plate;

    if (qPlayer.IsOut != 0) {
        return false;
    }
    /* The end-of-day fast forward does not walk anybody anywhere: PERSONS::DoOneStep() skips
     * straight to the next action while Sim.CallItADay is set, and the main loop stops calling
     * UpdateWaypointWalkingDirection() altogether. */
    if (Sim.CallItADay != 0) {
        AT_Log("BotWalk::walkToPlate(): Not walking to %ld/%ld, the day is being fast forwarded", (long)plate.x, (long)plate.y);
        return false;
    }
    if (qPlayer.RunningToToilet != 0 || qPlayer.IsStuck != 0) {
        AT_Log("BotWalk::walkToPlate(): Not walking to %ld/%ld, the character is not ours to steer right now", (long)plate.x, (long)plate.y);
        return false;
    }

    if (!resolvePlate(plate)) {
        AT_Warn("BotWalk::walkToPlate(): There is nowhere to stand at plate %ld/%ld", (long)requested.x, (long)requested.y);
        return false;
    }

    const SLONG announced = roomAtPlate(plate);
    if (announced != 0) {
        AT_Warn("BotWalk::walkToPlate(): Plate %ld/%ld announces room %ld - the character will walk into it rather than stand there", (long)plate.x,
                (long)plate.y, (long)announced);
    }

    /* The target is out in the airport, so whatever room we are in has to be left first. The
     * character walks out by itself; the target below just waits for it. */
    leaveRoomsForWalk();

    qPlayer.WaitForRoom = 0;
    qPlayer.DirectToRoom = 0; /* nothing is entered on arrival */
    qPlayer.WalkToPlate(plate);

    PERSON &qPerson = Sim.Persons[Sim.Persons.GetPlayerIndex(qPlayer.PlayerNum)];
    qPerson.SetIsRunning(run ? TRUE : FALSE);

    SLONG hold = (holdTicks >= 0) ? holdTicks : estimateWalkTicks(plate, run);
    if (mInExecuteAction) {
        /* PLAYER::RobotExecuteAction() divides WorkCountdown down again once this callback
         * returns (Player.cpp:4234-4242), so what is set here is not what the bot gets. The
         * free game only has ROBOT_USE_WORKQUICK_2, which halves it; the missions can have the
         * other two on top. Multiplying up first leaves the hold that was asked for, and keeps
         * it clear of the "> 2" and "> 4" guards those divisions carry. */
        if (qPlayer.RobotUse(ROBOT_USE_WORKQUICK_2)) {
            hold *= 2;
        }
        if (qPlayer.RobotUse(ROBOT_USE_WORKVERYQUICK)) {
            hold *= 4;
        } else if (qPlayer.RobotUse(ROBOT_USE_WORKQUICK)) {
            hold *= 2;
        }
    }

    /* See the banner: the countdown is only counted down while slot 0 is empty. */
    qPlayer.RobotActions[0].ActionId = ACTION_NONE;
    qPlayer.StandStillSince = 0;
    qPlayer.WorkCountdown = hold;
    /* Only read during the end-of-day fast forward, which this never runs in, but a zero there
     * means "re-plan now" and there is no reason to leave one behind. */
    qPlayer.SpeedCount = std::max<SLONG>(1, hold);

    AT_Log("BotWalk::walkToPlate(): Walking from plate %ld/%ld to %ld/%ld (asked for %ld/%ld), holding for %ld ticks", (long)getPlate().x, (long)getPlate().y,
           (long)plate.x, (long)plate.y, (long)requested.x, (long)requested.y, (long)hold);
    return true;
}

/* A rune of a type and parameter, in position coordinates. AIRPORT::GetRandomTypedRune() draws
 * from rand() unless it is handed a generator, so it gets a local one: the answer stays the
 * same for the same rune and nothing else's random sequence moves. */
static bool findRune(ULONG brickId, UBYTE par, XY &outPosition) {
    TEAKRAND rand(par);
    const XY position = Airport.GetRandomTypedRune(brickId, par, true, &rand);
    if (position.x == -9999 && position.y == -9999) {
        return false;
    }
    outPosition = position;
    return true;
}

bool BotWalk::findStenchPlate(SLONG victim, XY &outPlate) const {
    std::vector<SLONG> gates;
    const auto &qGates = Sim.Players.Players[victim].Gates.Gates;
    for (SLONG e = 0; e < qGates.AnzEntries(); e++) {
        if (qGates[e].Miete != -1) {
            gates.push_back(qGates[e].Nummer);
        }
    }
    if (gates.empty()) {
        return false;
    }

    /* The plate of the entrance itself if we may stand there, else one next to it: the stench
     * covers the eight plates around the bomb as well. */
    static const XY kOffsets[] = {{0, 0}, {0, 1}, {0, -1}, {1, 0}, {-1, 0}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
    const SLONG n = static_cast<SLONG>(gates.size());
    for (SLONG i = 0; i < n; i++) {
        const SLONG gate = gates[(Sim.Date + mStinkBombDrops + i) % n];
        XY position;
        if (gate < 0 || gate > 255 || !findRune(RUNE_2WAIT, static_cast<UBYTE>(gate), position) || position.y >= 4000) {
            continue;
        }
        const XY center = plateFromPosition(position);
        for (const auto &qOffset : kOffsets) {
            const XY plate(center.x + qOffset.x, center.y + qOffset.y);
            if (plate.y < 5 || plate.y > 14 || !plateIsWalkable(plate) || roomAtPlate(plate) != 0) {
                continue;
            }
            AT_Log("BotWalk::findStenchPlate(): Gate %ld of %s, entrance at plate %ld/%ld, dropping at %ld/%ld.", gate,
                   Sim.Players.Players[victim].AirlineX.c_str(), (long)center.x, (long)center.y, (long)plate.x, (long)plate.y);
            outPlate = plate;
            return true;
        }
    }
    return false;
}

bool BotWalk::findGluePlates(SLONG victim, XY &outApproach, XY &outDrop) const {
    XY position;
    if (!findRune(RUNE_2SHOP, static_cast<UBYTE>(ROOM_BURO_A + victim * 10), position) || position.y < 4000) {
        return false;
    }
    const XY door = plateFromPosition(position);

    /* The corridor plate in front of the door first: the victim crosses it on the way in, and
     * the corridor is where we can walk to. The door plate sits in a nook of its own, and the
     * first walking day never reached the plates beside it - the walk stopped at the top of
     * the stairs. Approached along the row, from the west (facing east, Phase 1, which
     * SIM::AddGlueSabotage lets through where the plate has exit bit 64) or from the east
     * (facing west, Phase 3, bit 16). Every failed try today moves on to the next candidate. */
    std::vector<std::pair<XY, XY>> candidates;
    const XY targets[] = {XY(door.x, door.y + 1), door};
    for (const auto &qTarget : targets) {
        if (qTarget.y < 0 || qTarget.y > 2) {
            continue;
        }
        for (SLONG dir : {1, -1}) {
            const XY drop(qTarget.x - dir, qTarget.y);
            const XY approach(qTarget.x - 2 * dir, qTarget.y);
            if (!plateIsWalkable(drop) || !plateIsWalkable(approach) || roomAtPlate(drop) != 0 || roomAtPlate(approach) != 0) {
                continue;
            }
            const UBYTE exitBit = (dir == 1) ? 64 : 16;
            if ((Airport.iPlate[drop.y + (drop.x << 4)] & exitBit) == 0) {
                continue;
            }
            candidates.emplace_back(approach, drop);
        }
    }
    if (candidates.empty()) {
        return false;
    }
    const auto &qPick = candidates[mGlueTriesToday % static_cast<SLONG>(candidates.size())];
    outApproach = qPick.first;
    outDrop = qPick.second;
    AT_Log("BotWalk::findGluePlates(): Office of %s at plate %ld/%ld, dropping from %ld/%ld (candidate %ld of %ld).",
           Sim.Players.Players[victim].AirlineX.c_str(), (long)door.x, (long)door.y, (long)outDrop.x, (long)outDrop.y,
           mGlueTriesToday % static_cast<SLONG>(candidates.size()) + 1, static_cast<SLONG>(candidates.size()));
    return true;
}

bool BotWalk::wantItemDrop(SLONG item) const {
    if (Sim.CallItADay != 0 || mDropStage != DropStage::None) {
        return false;
    }
    if ((Sim.bNetwork != 0) && (Sim.bIsHost == 0)) {
        return false; /* the host plays the computer players */
    }
    if (Sim.GetHour() >= kLastDropHour || qPlayer.HasItem(item) == 0) {
        return false;
    }
    if (item == ITEM_STINKBOMBE) {
        return !mStinkBombDroppedToday && mBombTriesToday < kMaxDropTriesPerDay;
    }
    return !mGlueDroppedToday && mGlueTriesToday < kMaxDropTriesPerDay;
}

bool BotWalk::startItemDrop(const std::vector<SLONG> &victims, SLONG item) {
    if (!wantItemDrop(item)) {
        return false;
    }
    const bool isBomb = (item == ITEM_STINKBOMBE);
    SLONG &qTries = isBomb ? mBombTriesToday : mGlueTriesToday;

    for (SLONG victim : victims) {
        if (victim < 0 || victim >= 4 || victim == qPlayer.PlayerNum || Sim.Players.Players[victim].IsOut != 0) {
            continue;
        }
        XY plate;
        XY drop(-1, -1);
        if (isBomb ? !findStenchPlate(victim, plate) : !findGluePlates(victim, plate, drop)) {
            continue;
        }
        qTries++;
        if (!walkToPlate(plate)) {
            return false;
        }
        mDropStage = isBomb ? DropStage::StinkBomb : DropStage::GlueApproach;
        mDropPlate = plate;
        mDropFinal = drop;
        mDropVictim = victim;
        mDropStart = Sim.TimeSlice;
        mDropUseTries = 0;
        AT_Log("BotWalk::startItemDrop(): Walking to plate %ld/%ld to drop %s against %s (try %ld today).", (long)mDropPlate.x, (long)mDropPlate.y,
               isBomb ? "a stink bomb" : "glue", Sim.Players.Players[victim].AirlineX.c_str(), qTries);
        return true;
    }

    /* Nowhere to put it. Counted as a try all the same, or the caller would keep coming back
     * for a drop this layout does not allow. */
    qTries++;
    AT_Log("BotWalk::startItemDrop(): No spot for %s against any victim (try %ld today).", isBomb ? "a stink bomb" : "glue", qTries);
    return false;
}

void BotWalk::abortItemDrop(const char *why) {
    const PERSON &qPerson = Sim.Persons[Sim.Persons.GetPlayerIndex(qPlayer.PlayerNum)];
    AT_Log("BotWalk::abortItemDrop(): Giving up the %s drop at plate %ld/%ld after %ld ticks: %s. Now at plate %ld/%ld, room %ld, StatePar %ld, "
           "walking %ld, Dir %ld, NewDir %ld, primary target %ld/%ld.",
           mDropStage == DropStage::StinkBomb ? "stink bomb" : "glue", (long)mDropPlate.x, (long)mDropPlate.y, (long)(Sim.TimeSlice - mDropStart), why,
           (long)getPlate().x, (long)getPlate().y, static_cast<SLONG>(qPlayer.GetRoom()), static_cast<SLONG>(qPerson.GetStatePar()),
           static_cast<SLONG>(qPlayer.iWalkActive), static_cast<SLONG>(qPerson.GetDir()), static_cast<SLONG>(qPlayer.NewDir), (long)qPlayer.PrimaryTarget.x,
           (long)qPlayer.PrimaryTarget.y);
    /* Hand the character back to the bot, unless it has already moved on by itself. */
    if (qPlayer.GetRoom() == ROOM_AIRPORT && qPlayer.DirectToRoom == 0 && qPlayer.WorkCountdown > 0) {
        stopWalking();
    }
    mDropStage = DropStage::None;
}

void BotWalk::tickItemDrop(bool isOnThePhone) {
    if (mDropStage == DropStage::None) {
        return;
    }
    if (Sim.CallItADay != 0) {
        abortItemDrop("the day is being fast forwarded");
        return;
    }
    if (Sim.TimeSlice - mDropStart > kMaxDropTicks) {
        abortItemDrop("took too long");
        return;
    }
    if (qPlayer.IsStuck != 0) {
        abortItemDrop("stuck in glue");
        return;
    }
    /* Still stepping out of the room the walk started in. */
    if (qPlayer.GetRoom() != ROOM_AIRPORT) {
        return;
    }
    /* The hold ran out and RobotPump() has sent the character on to its next room. */
    if (qPlayer.DirectToRoom != 0) {
        abortItemDrop("the bot moved on to its next room");
        return;
    }
    /* Keep the hold up until the item is down: walkToPlate()'s estimate is a straight line,
     * and the walk upstairs to an office took twice as long as it. kMaxDropTicks bounds it. */
    qPlayer.WorkCountdown = std::max<SLONG>(qPlayer.WorkCountdown, 20);

    const PERSON &qPerson = Sim.Persons[Sim.Persons.GetPlayerIndex(qPlayer.PlayerNum)];
    const bool stopped = qPlayer.iWalkActive == 0 && qPerson.GetDir() == 8 && qPerson.GetStatePar() == 0;
    if (!stopped) {
        return;
    }
    if (getPlate() != mDropPlate || qPlayer.NewDir != 8) {
        /* Blocked short of the spot, or the walk ended somewhere else: the first glue walks
         * stood at the top of the stairs with the walk switched off until the timeout. */
        if (++mDropUseTries > kMaxDropStandTicks) {
            abortItemDrop("stopped short of the spot");
        }
        return;
    }

    if (mDropStage == DropStage::GlueApproach) {
        /* One plate towards the target, so that the character ends up facing it. */
        if (!walkToPlate(mDropFinal)) {
            abortItemDrop("could not take the last step");
            return;
        }
        mDropStage = DropStage::GlueFinal;
        mDropPlate = mDropFinal;
        mDropUseTries = 0;
        return;
    }

    /* The phone animation puts 6 into Phase, which is the facing the glue goes by. */
    if (isOnThePhone) {
        return;
    }
    const SLONG item = (mDropStage == DropStage::StinkBomb) ? ITEM_STINKBOMBE : ITEM_GLUE;
    GameMechanic::useItem(qPlayer, item);
    if (qPlayer.HasItem(item) != 0) {
        if (++mDropUseTries > kMaxDropStandTicks) {
            abortItemDrop("the item could not be used here");
        }
        return;
    }

    const char *victimName = (mDropVictim >= 0) ? Sim.Players.Players[mDropVictim].AirlineX.c_str() : "?";
    if (item == ITEM_STINKBOMBE) {
        mStinkBombDroppedToday = true;
        mStinkBombDrops++;
        AT_Log("BotWalk::tickItemDrop(): Dropped a stink bomb at plate %ld/%ld against %s after %ld ticks (#%ld).", (long)mDropPlate.x, (long)mDropPlate.y,
               victimName, (long)(Sim.TimeSlice - mDropStart), mStinkBombDrops);
    } else {
        mGlueDroppedToday = true;
        mGlueDrops++;
        AT_Log("BotWalk::tickItemDrop(): Dropped glue from plate %ld/%ld (facing %ld) against %s after %ld ticks (#%ld).", (long)mDropPlate.x,
               (long)mDropPlate.y, static_cast<SLONG>(qPerson.GetPhase()), victimName, (long)(Sim.TimeSlice - mDropStart), mGlueDrops);
    }
    mDropStage = DropStage::None;
    stopWalking();
}

TEAKFILE &operator<<(TEAKFILE &File, const BotWalk &bot) {
    SLONG savegameVersion = 100;
    File << savegameVersion;

    File << bot.mGlueTriesToday << bot.mBombTriesToday;
    File << bot.mGlueDrops << bot.mStinkBombDrops;
    File << bot.mStinkBombDroppedToday << bot.mGlueDroppedToday;

    SLONG magicnumber = 0x23;
    File << magicnumber;

    return (File);
}

TEAKFILE &operator>>(TEAKFILE &File, BotWalk &bot) {
    SLONG savegameVersion;
    File >> savegameVersion;

    bot.mInExecuteAction = false;
    bot.mDropStage = BotWalk::DropStage::None;
    bot.mDropVictim = -1;
    bot.mDropUseTries = 0;

    File >> bot.mGlueTriesToday >> bot.mBombTriesToday;
    File >> bot.mGlueDrops >> bot.mStinkBombDrops;
    File >> bot.mStinkBombDroppedToday >> bot.mGlueDroppedToday;

    SLONG magicnumber = 0;
    File >> magicnumber;
    assert(magicnumber == 0x23);

    return (File);
}
