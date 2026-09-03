#!/bin/bash
# Runs an unattended multiplayer session with N peers on this machine and diffs their traces.
#
#   ./scripts/run_multiplayer.sh [humans] [days] [botlevels]
#
# Each peer gets its own directory so that they cannot fight over AT.json, debug.txt or the
# savegame slot. The bulk of the game data is symlinked, so a peer directory costs a few MB.
# The AT binary itself must be a real copy: AppPath comes from SDL_GetBasePath(), which
# resolves /proc/self/exe, so a symlinked binary would resolve back to the shared directory
# and share its AT.json after all.

set -u

HUMANS=${1:-2}
DAYS=${2:-10}
BOTS=${3:-44}

GAME="/media/LINUX/GOG Games/Airline Tycoon Deluxe/game"
RUN="$GAME/mprun"
TIMEOUT=120

./scripts/run_build.sh >/dev/null || { echo "build failed"; exit 1; }

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

    cp "$GAME/AT" "$peer/AT"
    [ -f "$GAME/AT.json" ] && cp "$GAME/AT.json" "$peer/AT.json"
done

echo "Starting $HUMANS peers for $DAYS days (bots=$BOTS)..."

pids=()
# Host first, so that the clients have something to connect to. They retry anyway.
(
    cd "$RUN/peer0" || exit 1
    ./AT /mphost 0 "$HUMANS" "$BOTS" /mpdays "$DAYS" /mptimeout "$TIMEOUT" /nettrace 1 \
        >"$RUN/peer0.log" 2>&1
) &
pids+=($!)

for ((i = 1; i < HUMANS; i++)); do
    (
        cd "$RUN/peer$i" || exit 1
        ./AT /mpjoin 127.0.0.1 "$i" /mpdays "$DAYS" /mptimeout "$TIMEOUT" /nettrace 1 \
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
