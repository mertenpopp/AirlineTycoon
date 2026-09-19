#!/bin/bash
# Loads savegames headlessly and lets each game run on for a while: a release check that saves of
# older versions still load and play. Reports the save version, the exit code, and the days the
# game got through after loading.
#
#   ./scripts/run_loadtest.sh [slots...]
#
# slots are the N of savegame/gameN.dat (default: every one there). Each game runs for SECONDS of
# wall time; a timeout (exit 124) is the normal end unless the free game reached its last day. The
# savegame folder is copied before and restored afterwards, because the game autosaves into slot 11.

SECONDS_PER_GAME=${SECONDS_PER_GAME:-90}

./scripts/run_build.sh
cd "/media/LINUX/GOG Games/Airline Tycoon Deluxe/game/" || exit 1

SLOTS="$*"
if [ -z "$SLOTS" ]; then
    SLOTS=$(ls savegame/game*.dat | sed 's/.*game\([0-9]*\)\.dat/\1/' | sort -n)
fi

OUT=loadtest
rm -rf "$OUT"
mkdir -p "$OUT"
rm -rf savegame.loadtest-backup
cp -a savegame savegame.loadtest-backup

# One at a time: every game autosaves into slot 11, which may be one of the saves under test.
for slot in $SLOTS; do
    rm -rf savegame
    cp -a savegame.loadtest-backup savegame
    timeout -k 10 "$SECONDS_PER_GAME" ./AT /quick -1 /load $((slot + 1)) > "$OUT/slot$slot.txt" 2>&1
    echo "EXITCODE=$?" >> "$OUT/slot$slot.txt"
done

rm -rf savegame
mv savegame.loadtest-backup savegame

echo
printf "%-5s %-8s %-5s %-10s %s\n" slot version exit days errors
for slot in $SLOTS; do
    log="$OUT/slot$slot.txt"
    version=$(python3 -c "
import struct
d = open('savegame/game$slot.dat', 'rb').read(64)
n = struct.unpack('<I', d[0:4])[0]
print('%d.%d' % struct.unpack('<ii', d[4 + n:12 + n]))" 2>/dev/null)
    code=$(grep -a -o 'EXITCODE=[0-9]*' "$log" | tail -1 | cut -d= -f2)
    days=$(grep -a -o 'BotStatistics/[A-Z]*: [0-9]*' "$log" | awk '{print $2}' | sort -n | uniq | sed -n '1p;$p' | tr '\n' ' ')
    errors=$(grep -a -c -i 'exception\|error' "$log")
    printf "%-5s %-8s %-5s %-10s %s\n" "$slot" "${version:-?}" "$code" "${days:--}" "$errors"
done
cd - > /dev/null
