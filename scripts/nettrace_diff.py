#!/usr/bin/env python3
"""Compare NetTrace logs captured on two or more peers of the same multiplayer session.

Usage:
    scripts/nettrace_diff.py hostlog.txt clientlog.txt [morelogs...]

Reports, in this order:

  1. MISMATCH lines - a receiver did not consume its message exactly. These mean sender
     and receiver disagree about that message's layout, and are almost always the cause
     rather than a symptom. Fix these first.
  2. The first state fingerprint on which the peers disagree, with the differing fields.
     This is the tick where the session actually diverged; everything after it is noise.
  3. Messages one peer sent that no other peer logged receiving (and vice versa).
  4. Dropped messages and other protocol events.

Enable the traces with "/nettrace 1" (or 2 for per-frame chatter) on every peer.
"""

import re
import sys
from collections import defaultdict

LINE = re.compile(r"NetTrace \|\| (?P<kind>MSG|FP|EVT|MISMATCH)\s+(?P<rest>.*)$")
KV = re.compile(r"(\w+)=(\S+)")


def parse(path):
    msgs, fps, evts, mismatches = [], [], [], []
    with open(path, encoding="utf-8", errors="replace") as handle:
        for lineno, line in enumerate(handle, 1):
            found = LINE.search(line)
            if not found:
                continue
            kind = found.group("kind")
            rest = found.group("rest")
            fields = dict(KV.findall(rest))
            fields["_line"] = lineno
            fields["_raw"] = rest.strip()
            if kind == "MSG":
                msgs.append(fields)
            elif kind == "FP":
                # "FP daystart day=.. p=.." or the pool line, which has no p=
                fields["_when"] = rest.split()[0]
                fields["_who"] = fields.get("p", "pool")
                fps.append(fields)
            elif kind == "EVT":
                evts.append(fields)
            else:
                mismatches.append(fields)
    return {"msgs": msgs, "fps": fps, "evts": evts, "mismatch": mismatches, "path": path}


def fp_key(entry):
    return (entry.get("day"), entry["_when"], entry["_who"])


def report_mismatches(peers):
    hits = [(p["path"], m) for p in peers for m in p["mismatch"]]
    print("== 1. Message layout mismatches ==")
    if not hits:
        print("   none\n")
        return
    print("   These mean a handler read a different number of bytes than the sender wrote.")
    for path, m in hits[:40]:
        print(f"   {path}: {m['_raw']}")
    if len(hits) > 40:
        print(f"   ... and {len(hits) - 40} more")
    print()


def report_divergence(peers):
    print("== 2. First diverging state fingerprint ==")
    if len(peers) < 2:
        print("   need at least two logs\n")
        return

    tables = [{fp_key(f): f for f in p["fps"]} for p in peers]
    common = set(tables[0])
    for table in tables[1:]:
        common &= set(table)
    if not common:
        print("   no fingerprints in common (did every peer run with /nettrace?)\n")
        return

    def order(key):
        day = key[0]
        return (int(day) if day and day.lstrip("-").isdigit() else 0, key[1], str(key[2]))

    # Each peer reports its own human as owner 0 and the other humans as 2, so only the
    # human/computer split is comparable - and it must agree, or the peers are playing
    # different games. It is not part of the hash.
    def role(row):
        owner = row.get("owner")
        return "human" if owner in ("0", "2") else ("computer" if owner == "1" else owner)

    for peer, table in zip(peers, tables):
        groups = defaultdict(list)
        for (day, when, who), entry in table.items():
            if who != "pool":
                groups[(day, when)].append(entry)
        for (day, when) in sorted(groups, key=lambda k: order((k[0], k[1], ""))):
            local = sum(1 for entry in groups[(day, when)] if entry.get("owner") == "0")
            if local != 1:
                print(f"   {peer['path']} controls {local} players at day={day} {when} (expected exactly 1)")
                break

    for key in sorted(common, key=order):
        rows = [table[key] for table in tables]
        if len({r.get("hash") for r in rows}) == 1 and len({role(r) for r in rows}) == 1:
            continue
        day, when, who = key
        print(f"   day={day} {when} {'pool' if who == 'pool' else 'player ' + str(who)}")
        if len({role(r) for r in rows}) > 1:
            for peer, r in zip(peers, rows):
                print(f"      {'role':<10} {role(r):<24} {peer['path']}")
        # "owner" is relative to the peer doing the reporting (itself 0, the others 2), so it
        # differs by design and is not evidence of anything. A human's money and credit are only
        # authoritative on that human's own peer and resynchronised every morning, so they are
        # not hashed either; listing them would bury the field that actually caused the mismatch.
        local_only = {"owner"}
        if when != "briefing" and any(r.get("owner") in ("0", "2") for r in rows):
            local_only |= {"money", "credit"}
        keys = [k for k in rows[0] if not k.startswith("_") and k not in local_only]
        for field in keys:
            values = [r.get(field) for r in rows]
            if len(set(values)) > 1:
                for peer, value in zip(peers, values):
                    print(f"      {field:<10} {value:<24} {peer['path']}")
        print("\n   (everything after this point is downstream of this divergence)\n")
        return

    print("   peers agree on every shared fingerprint\n")


def report_unmatched(peers):
    print("== 3. Messages sent but never received ==")
    if len(peers) < 2:
        print("   need at least two logs\n")
        return

    sent = defaultdict(int)
    recv = defaultdict(int)
    for peer in peers:
        for m in peer["msgs"]:
            key = (m.get("day"), m.get("name"))
            if m.get("dir") == "SEND":
                sent[key] += 1
            elif m.get("dir") == "RECV":
                recv[key] += 1

    rows = []
    for key in sorted(set(sent) | set(recv), key=lambda k: (int(k[0] or 0), str(k[1]))):
        want = sent[key] * (len(peers) - 1)
        got = recv[key]
        if want != got:
            rows.append((key, sent[key], got, want))

    if not rows:
        print("   every sent message was accounted for\n")
        return
    print("   day  message                        sent  received  expected")
    for (day, name), s, g, w in rows[:40]:
        print(f"   {str(day):<4} {str(name):<30} {s:<5} {g:<9} {w}")
    if len(rows) > 40:
        print(f"   ... and {len(rows) - 40} more")
    print("   (counts are approximate for targeted messages, which only one peer receives)\n")


# What every run does: going home, the scripted actions, the day's handover. Anything else is
# printed, so a new kind of trouble does not need this list to be extended first.
ROUTINE_EVENTS = {
    "LOBBY",
    "WINDOW",
    "GOHOME",
    "DAYFINISH",
    "DAYFINISHALL",
    "CUTSALARIES",
    "ACTION",
    "MENUAWAY",
    "WAITFORPLAYER",
    "NOGATE",
}


def report_events(peers):
    print("== 4. Protocol events ==")
    any_shown = False
    for peer in peers:
        for e in peer["evts"]:
            raw = e["_raw"]
            # the event keyword is the first token that is not a key=value field
            keyword = next((tok for tok in raw.split() if "=" not in tok), "")
            # Everything that is not part of a run's normal course is worth seeing - a message
            # refused as out of range says as much about a desync as a dropped one, and listing
            # only known keywords hid exactly that once.
            if keyword not in ROUTINE_EVENTS:
                print(f"   {peer['path']}: {raw}")
                any_shown = True
    if not any_shown:
        print("   nothing but the run's normal course")
    print()


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    peers = [parse(p) for p in sys.argv[1:]]
    for peer in peers:
        print(f"{peer['path']}: {len(peer['msgs'])} messages, {len(peer['fps'])} fingerprints, "
              f"{len(peer['evts'])} events, {len(peer['mismatch'])} mismatches")
    print()
    report_mismatches(peers)
    report_divergence(peers)
    report_unmatched(peers)
    report_events(peers)
    return 0


if __name__ == "__main__":
    sys.exit(main())
