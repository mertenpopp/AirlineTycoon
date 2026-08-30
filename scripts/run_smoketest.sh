#!/bin/bash

./scripts/run_build.sh
cd "/media/LINUX/GOG Games/Airline Tycoon Deluxe/game/"
rm -f GameLog.txt
rm -f ClaudeBot.csv
./AT /quicker -1 2>&1 | tee GameLog.txt | grep 'BotStatistics/HA' > ClaudeBot.csv
cd -