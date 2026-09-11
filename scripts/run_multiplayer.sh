#!/bin/bash
# Runs an unattended multiplayer session with N peers on this machine and diffs their traces.
#
#   ./scripts/run_multiplayer.sh [--debug] [humans] [days] [botlevels] [gohome] [cutsalaries]
#
# --debug builds the same optimised Release configuration with debug symbols added (-g) into
# build-debug/ and runs the peers on that binary, so that a crash leaves a core with source
# lines. It deliberately does not use CMAKE_BUILD_TYPE=Debug: that defines _DEBUG, which
# switches on extra code paths and changes timing, and can make a race disappear. The installed
# game binary is not touched.
#
# gohome is the hour at which the idle humans call it a day, the way a player clicking "go
# home" does; the rest of the day then runs fast-forwarded. 0 keeps them in until the game
# closes the day at 18:00, which takes about 16 minutes per day.
#
# cutsalaries makes each idle human cut all salaries that many times before going home the
# first time, as the personnel dialog does; 4 makes their staff strike the next morning. Needs
# gohome.
#
# days=0 quits as soon as the players reach the boss's office on the first morning - enough to
# exercise the lobby and the start of the game in well under a minute.
#
# Each peer gets its own directory so that they cannot fight over AT.json, debug.txt or the
# savegame slot. The bulk of the game data is symlinked, so a peer directory costs a few MB.
# The AT binary itself must be a real copy: AppPath comes from SDL_GetBasePath(), which
# resolves /proc/self/exe, so a symlinked binary would resolve back to the shared directory
# and share its AT.json after all.

set -u

DEBUG=0
if [ "${1:-}" = "--debug" ]; then
    DEBUG=1
    shift
fi

HUMANS=${1:-2}
DAYS=${2:-10}
BOTS=${3:-44}
GOHOME=${4:-0}
CUTSALARIES=${5:-0}

GAME="/media/LINUX/GOG Games/Airline Tycoon Deluxe/game"
RUN="$GAME/mprun"
TIMEOUT=120

if [ "$DEBUG" = 1 ]; then
    cmake -B build-debug -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-g -fno-omit-frame-pointer" \
        -DPROJECT_INSTALL_DIR="$GAME" >/dev/null || { echo "cmake failed"; exit 1; }
    ninja -C build-debug AT >/dev/null || { echo "build failed"; exit 1; }
    BINARY="$PWD/build-debug/Release/AT"
else
    ./scripts/run_build.sh >/dev/null || { echo "build failed"; exit 1; }
    BINARY="$GAME/AT"
fi

rm -rf "$RUN"
mkdir -p "$RUN"

for ((i = 0; i < HUMANS; i++)); do
    peer="$RUN/peer$i"
    mkdir -p "$peer/savegame"

    # Symlink everything read-only, copy only what a peer writes to.
    for entry in "$GAME"/*; do
        name=$(basename "$entry")
        case "$name" in
            AT | AT.json | savegame | mprun | debug.txt | GameLog.txt | data* | *.csv | *.txt) continue ;;
        esac
        ln -sfn "$entry" "$peer/$name"
    done

    cp "$BINARY" "$peer/AT"
    [ -f "$GAME/AT.json" ] && cp "$GAME/AT.json" "$peer/AT.json"
done

echo "Starting $HUMANS peers for $DAYS days (bots=$BOTS, gohome=$GOHOME, cutsalaries=$CUTSALARIES)..."

pids=()
# Host first, so that the clients have something to connect to. They retry anyway.
(
    cd "$RUN/peer0" || exit 1
    ./AT /mphost 0 "$HUMANS" "$BOTS" /mpdays "$DAYS" /mpgohome "$GOHOME" /mpcutsalaries "$CUTSALARIES" /mptimeout "$TIMEOUT" /nettrace 1 \
        >"$RUN/peer0.log" 2>&1
) &
pids+=($!)

for ((i = 1; i < HUMANS; i++)); do
    (
        cd "$RUN/peer$i" || exit 1
        ./AT /mpjoin 127.0.0.1 "$i" /mpdays "$DAYS" /mpgohome "$GOHOME" /mpcutsalaries "$CUTSALARIES" /mptimeout "$TIMEOUT" /nettrace 1 \
            >"$RUN/peer$i.log" 2>&1
    ) &
    pids+=($!)
done

fail=0
for pid in "${pids[@]}"; do
    wait "$pid" || fail=1
done

echo
for ((i = 0; i < HUMANS; i++)); do
    started=$(grep -c "LOBBY starting game\|BEGINGAME" "$RUN/peer$i.log" 2>/dev/null)
    days=$(grep -c "NetTrace || FP .*dayend" "$RUN/peer$i.log" 2>/dev/null)
    echo "peer$i: lobby=$started fingerprints=$days  ($RUN/peer$i.log)"
done

echo
python3 scripts/nettrace_diff.py "$RUN"/peer*.log

exit $fail
