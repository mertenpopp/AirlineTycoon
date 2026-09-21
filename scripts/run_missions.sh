#!/bin/bash
# Plays every mission once per bot-level combination and reports how each game ended: the exit
# code, the day reached and the mission result line. A release check for crashes and hangs in the
# missions, which the free-game measurements never visit.
#
#   ./scripts/run_missions.sh [botlevels...]
#
# Each botlevels value is what /setbotlevel gets: hundreds = FL, tens = PT, ones = HA (the idle
# human is SA). Default: every MertenBot level for all three computer players, and the classic bot.
# MISSIONS and TIMEOUT (seconds per game, default 900) can be set to run a subset, SEEDS (default 1)
# to play each game on several seeds.
# The game is started exactly as threadpool.rb starts it ("./AT /quick <mission> /seed <n> ..."),
# at most as many games at once as it runs.

MISSIONS="${MISSIONS:-0 1 2 3 4 5 11 12 13 14 15 16 17 18 19 20 41 42 43 44 45 46 47 48 49 50}"
LEVELS="${*:-000 111 222 333 444 555}"
SEEDS="${SEEDS:-1}"
TIMEOUT="${TIMEOUT:-900}"

./scripts/run_build.sh
cd "/media/LINUX/GOG Games/Airline Tycoon Deluxe/game/" || exit 1

OUT=missions
rm -rf "$OUT"
mkdir -p "$OUT"

WORKERS=$(nproc)
if [ "$WORKERS" -gt 24 ]; then
    WORKERS=24
fi

for level in $LEVELS; do
    for mission in $MISSIONS; do
        for seed in $SEEDS; do
            echo "$mission $level $seed"
        done
    done
done | xargs -P "$WORKERS" -n 3 sh -c '
    log="'"$OUT"'/m$0_b$1_s$2.txt"
    timeout -k 30 '"$TIMEOUT"' ./AT /quick "$0" /seed "$2" /setbotlevel "$1" > "$log" 2>&1
    echo "EXITCODE=$?" >> "$log"
'

echo
printf "%-8s %-6s %-5s %-5s %-5s %s\n" mission bots seed exit day result
for level in $LEVELS; do
    for mission in $MISSIONS; do
      for seed in $SEEDS; do
        log="$OUT/m${mission}_b${level}_s${seed}.txt"
        code=$(grep -a -o 'EXITCODE=[0-9]*' "$log" | tail -1 | cut -d= -f2)
        day=$(grep -a -o 'BotStatistics/[A-Z]*: [0-9]*' "$log" | tail -1 | awk '{print $2}')
        result=$(grep -a 'BotMission: [0-9-]' "$log" | tail -1 | sed 's/.*BotMission: //')
        printf "%-8s %-6s %-5s %-5s %-5s %s\n" "$mission" "$level" "$seed" "$code" "${day:--}" "${result:-no result}"
      done
    done
done
cd - > /dev/null
