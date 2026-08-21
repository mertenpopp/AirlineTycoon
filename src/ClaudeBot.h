#ifndef CLAUDE_BOT_H_
#define CLAUDE_BOT_H_

#include "class.h"
#include "defines.h"

class PLAYER;

extern const SLONG kRouteAvgDays;

class ClaudeBot {
  public:
    explicit ClaudeBot(PLAYER &player);

    void RobotInit();
    void RobotPlan();
    void RobotExecuteAction();

    void setNoticedSickness() { mIsSickToday = true; }
    SLONG getNextMood();

    __int64 getMoneyAvailable() const { return qPlayer.Money; }

    /* anim state */
    bool getOnThePhone() const { return mOnThePhone > 0; }
    void decOnThePhone() { mOnThePhone--; }

    friend TEAKFILE &operator<<(TEAKFILE &File, const ClaudeBot &bot);
    friend TEAKFILE &operator>>(TEAKFILE &File, ClaudeBot &bot);

  private:
    TEAKRAND LocalRandom{};
    PLAYER &qPlayer;

    bool mFirstRun{true};
    bool mIsSickToday{false};

    /* anim state and mood bubbles */
    SLONG mOnThePhone{0};
    SLONG mMood{-1};
    SLONG mMoodNext{-1};
};

TEAKFILE &operator<<(TEAKFILE &File, const ClaudeBot &bot);
TEAKFILE &operator>>(TEAKFILE &File, ClaudeBot &bot);

#endif // CLAUDE_BOT_H_
