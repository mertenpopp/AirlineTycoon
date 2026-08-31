General information
===================

The game is called "Airline Tycoon Deluxe".

The full source code is available in this repository. Source code builds only the main executable. The full game (main executable + assets) is installed here: `/media/LINUX/GOG Games/Airline Tycoon Deluxe`.

Goal
----

Build a new computer player called 'ClaudeBot' for this game that follows all the rules and can beat even experienced human players. We want to maximize one specific metric: Cumulative operative saldo after 99 full days.

Sometimes, a reference implementation called "MertenBot" is available in the game files matching `Bot*.*`. Analyze and compare against this implementation only when asked explicitly.

Work independently. Only stop and ask me if you can't get a passing build after 2 attempts, found a rule that's genuinely ambiguous in code or performance regresses and you can't determine why after 2 tries.

Constraints
-----------

- Only make changes in ClaudeBot.cpp, ClaudeBot.h or any new files you created (they have to start with "ClaudeBot")
- Follow all the game rules in `.claude/RULES.md`
- You can analyze the entire source code to better understand the game.
- Ignore rendering, audio, animation, networking/multiplayer sync, save-game serialization, launcher code, and localization files — these don't affect game rules or valid moves.
- Write ClaudeBot for the "free game" only, you can assume `Sim.Difficulty == -1`
- Beware that there is an existing computer player who is cheating and does not follow the rules
- There is existing scaffolding in ClaudeBot.cpp which shall be used for Claude bot
- Action IDs define to which room ClaudeBot will walk and what he does there. Use only the action IDs defined in defines.h (starting with "ACTION_"). If you need more action IDs to better structure ClaudeBot, tell me what it does and which room is associated with it and I will add it.

The game is real-time not turn-based. ClaudeBot is called by the game simulation via the following callbacks:
- `RobotInit()`: Called once at the start of each in-game day. Can be used for initialization and to prepare internal bot state for the new day. Note that the player character is not in any room yet. Thus, access to the game state is very limited. After `RobotInit()`, the game will call the callback `RobotExecuteAction()` with the action ID ACTION_STARTDAY. This is usually the better place to check what has changed in the game state since last evening.
- `RobotPlan()`: Called when the game wants you to plan what to do next. Needs to set a primary and secondary action ID. Player character will walk to the appropriate place for the primary action or for the secondary if the room for the first is already occupied. Note that this is called usually between rooms. Thus, the decision making usually needs to be based on cached data since access to the game state is very limited.
- `RobotExecuteAction()`: Called when it is now possible to execute the primary action. Note that if the room was full, the primary action now might have been planned as secondary action. This callback is where almost all of ClaudeBots actions will be executed and also most of the planning and thinking needs to happen because only here ClaudeBot will have access to certain parts of the game's state (depending on the current room).

Note that the game is single-threaded. Thus, during a callback, the game state does not advance. However, it is possible that between two callbacks quite a bit of time has passed. Recommendation: In RobotExecuteAction(), check again if the preconditions for the planned actions still apply.

How to build
------------

In top-level directory, run the command:
- ./scripts/run_build.sh

Output should contain the following line if there was a change in source code:
`Installing: /media/LINUX/GOG Games/Airline Tycoon Deluxe/game/AT`

Output should contain the following line if no change was made:
`Up-to-date: /media/LINUX/GOG Games/Airline Tycoon Deluxe/game/AT`

How to test
-----------

In top-level directory, run the command:
- `./scripts/run_test.sh`

This runs the game in a mode which requires no human input. Note:
- Players "Falcon Lines" and "Phoenix Travel" will be controlled by the regular CPU player
- Player "Sunshine Airways" will be the human player and remain idle the entire time
- Player "Honey Airlines" will be controlled by ClaudeBot
- Game ends automatically at start of in-game day 100
- A detailled log is printed to GameLog.txt
- ClaudeBot.csv contains important stats with one line of data per in-game day. Very first filtered line are column headers
- You can also filter for the other airlines by adapting the grep command above: Search for "BotStatistics/<abbreviation>" instead

For a quick smoke test, use `./scripts/run_smoketest.sh`. This ends the game automatically after 5 in-game days.

How to measure performance of bot
---------------------------------

In top-level directory, run the command:
- `./scripts/run_measurement_claudebot.sh`

This runs the game in a mode which requires no human input. Note:
- Players "Falcon Lines" and "Phoenix Travel" will be controlled by the regular CPU player
- Player "Sunshine Airways" will be the human player and remain idle the entire time
- Player "Honey Airlines" will be controlled by ClaudeBot
- Game ends automatically at start of in-game day 100
- 300 game instances in total are run using parallel threads. Script waits until all have terminated
- Detailled logs are printed for each instance to a log file matching `dataCLAUDE_*.txt`
- For each instance, CSV data is printed to a file matching `dataCLAUDE_*.csv`
- The CSV data from the runs is then combined and a performance score is computed: Cumulative operative saldo after 99 full days

Last line of output shall look like this:
```
Day 99 / SaldoGesamt / Airline HA:  7228674337.883333
Day 99 / Firmenwert / Airline HA:  6512003341.083333
```

To measure the performance of "MertenBot", run the following command. "Honey Airlines" will be controlled by MertenBot.
- `./scripts/run_measurement_bot.sh`

To have both bots compete against each other, run the following command. "Phoenix Travel" will be controlled by MertenBot. "Honey Airlines" will be controlled by ClaudeBot.
- `./scripts/run_competition.sh`

Persistent progress log
-----------------------

Append to the file `.claude/DECISIONS.md` after each session — what you tried, what the performance indicator was, what you are trying next.

Development loop
----------------

- First get a legal-move-playing AI working (even a dumb one) so the harness (build → run → get score) is proven end-to-end
- Then treat the performance score as a fitness function: implement a change, run it, log the score, keep or revert based on result
- Commit after every improvement so you can git diff/revert cleanly when something regresses