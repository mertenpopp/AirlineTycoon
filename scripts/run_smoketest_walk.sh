#!/bin/bash
# Like run_smoketest_tycoon.sh, but the idle human stays in until WALKUNTIL (default 18), so the
# computer players walk from room to room instead of the day being fast-forwarded.
#
#   ./scripts/run_smoketest_walk.sh [walkuntil] [seed]

WALKUNTIL=${1:-18}
SEED=${2:-1}

./scripts/run_build.sh
cd "/media/LINUX/GOG Games/Airline Tycoon Deluxe/game/"
rm -f GameLogWalk.txt ClaudeBotWalk.csv
time ./AT /quicker -1 /setbotlevel 6 /seed "$SEED" /walkuntil "$WALKUNTIL" 2>&1 | tee GameLogWalk.txt | grep 'BotStatistics/HA' > ClaudeBotWalk.csv
cd -
