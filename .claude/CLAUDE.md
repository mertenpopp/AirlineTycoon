General information
===================

The game is called "Airline Tycoon Deluxe".

Full source code available in local directory: /home/merten/repositories/AirlineTycoon
Source code builds only the main executable.
The full game (main executable + assets) is installed here: /media/LINUX/GOG Games/Airline Tycoon Deluxe

Goal
----

Build a new computer player called 'ClaudeBot' for this game that follows all the rules and can beat even experienced human players. I want to maximize one specific metric: Weekly averaged operative saldo after 100 days.

Work independently. Only stop and ask me if you can't get a passing build after 2 attempts, found a rule that's genuinely ambiguous in code or performance regresses and you can't determine why after 2 tries.

Constraints
-----------

- Only make changes in Bot.cpp, Bot.h or any new files you created (they have to start with "Bot")
- Follow all the game rules in RULES.md
- You can analyze the entire source code to better understand the game.
- Ignore rendering, audio, animation, networking/multiplayer sync, save-game serialization, launcher code, and localization files — these don't affect game rules or valid moves.
- Beware that there is an existing computer player who is cheating and does not follow the rules
- There is existing scaffolding in Bot.cpp which shall be used for Claude bot
- Action IDs define to which room ClaudeBot will walk and what he does there. Use only the action IDs defined in defines.h (starting with "ACTION_"). If you need more action IDs to better structure ClaudeBot, tell me what it does and which room is associated with it and I will add it.

The game is a real-time not turn-based. ClaudeBot is called by the game simulation via the following callbacks:
- RobotInit(): Called once per in-game day. Can be used for initialization and check what has changed since evening.
- RobotPlan(): Called when the game wants you to plan what to do next. Needs to set a primary and secondary action ID. Player character will walk to the appropriate place for the primary action or for the secondary if the room for the first is already occupied.
- RobotExecuteAction(): Called when it is now possible to execute the primary action. Note that the game can shift actions and that the primary action now might have been planned as secondary action.


How to build
------------

In top-level directory, run the following commands:
- cmake -B build -S . -G Ninja
- ninja -C build install

Output should contain the following line if there was a change in source code:
Installing: /media/LINUX/GOG Games/Airline Tycoon Deluxe/game/AT

Output should contain the following line if no change was made:
Up-to-date: /media/LINUX/GOG Games/Airline Tycoon Deluxe/game/AT

How to test
-----------

In the installation directory, run the command:
./AT /quick -1 2>&1 | tee GameLog.txt | grep 'BotStatistics/HA' > ClaudeBot.csv"

This runs the game in a mode which requires no human input. Note:
- Players "Sunshine Airways" and "Falcon Lines" will be controlled by the regular CPU player
- Player "Phoenix Travel" will be the human player and remain idle the entire time
- Player "Honey Airlines" will be controlled by ClaudeBot
- Game ends automatically after 100 in-game days
- A detailled log is printed to GameLog.txt
- ClaudeBot.csv contains important stats with one line of data per in-game day. Very first filtered line are column headers
- You can also filter for the other airlines by adapting the grep command above: Search for "BotStatistics/<abbreviation>" instead

How to measure performance of bot
---------------------------------

To get the performance score of the bot, run these commands in the installation directory:

- ruby threadpool.rb 
- python concat.py 'dataCLAUDE_*.csv'

First commands runs 50 game instances in parallel and waits until all have terminated.
Second commands parses the CSV data from the runs and computes our performance score: Weekly averaged operative saldo after 100 days.

Last line of output shall look like this: Day 99 / Airline HA:  -49091.0

Persistent progress log
-----------------------

Append to the file DECISIONS.md after each session — what you tried, what the performance indicator was, what you are trying next.

Development loop
----------------

- First get a legal-move-playing AI working (even a dumb one) so the harness (build → run → get score) is proven end-to-end
- Then treat the performance score as a fitness function: implement a change, run it, log the score, keep or revert based on result
- Commit after every improvement so you can git diff/revert cleanly when something regresses