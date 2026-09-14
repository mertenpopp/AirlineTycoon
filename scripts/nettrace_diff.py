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

A log may hold several sessions: a new game starts one ("daystart"), and so does loading a
savegame ("loaded"). Fingerprints are only compared within the same session, which is named
after the day and hour it started at. A peer that rejoined in a new process may pass that
second log separately, or the two concatenated.
"""

import re
import sys
from collections import defaultdict

LINE = re.compile(r"NetTrace \|\| (?P<kind>MSG|FPDETAIL|FP|EVT|MISMATCH)\s+(?P<rest>.*)$")
KV = re.compile(r"(\w+)=(\S+)")


def parse(path):
    msgs, fps, evts, mismatches = [], [], [], []
    details = {}
    # The same game time comes round again after loading a savegame, so every fingerprint belongs
    # to the session the last "daystart" or "loaded" began. The name must come out the same on
    # every peer, which are a step or two apart: day and hour, and how often that was loaded.
    session = ""
    started = defaultdict(int)
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
                if fields["_when"] in ("daystart", "loaded") and fields.get("p") == "0":
                    t = fields.get("t", "0")
                    name = f"{fields['_when']} day={fields.get('day')} {(int(t) if t.isdigit() else 0) // 60000:02d}h"
                    started[name] += 1
                    session = name if started[name] == 1 else f"{name} #{started[name]}"
                fields["_session"] = session
                fps.append(fields)
            elif kind == "FPDETAIL":
                # "FPDETAIL hour13 day=.. p=.. planes=index:equipment/plan,... routes=index:hash,..."
                parts = {name: dict(item.split(":", 1) for item in fields.get(name, "").split(",") if ":" in item) for name in ("planes", "routes")}
                details[((session, fields.get("day")), rest.split()[0], fields.get("p"))] = parts
            elif kind == "EVT":
                evts.append(fields)
            else:
                mismatches.append(fields)
    return {"msgs": msgs, "fps": fps, "evts": evts, "mismatch": mismatches, "details": details, "path": path}


def fp_key(entry):
    return ((entry["_session"], entry.get("day")), entry["_when"], entry["_who"])


def show_day(day):
    """The (session, day) of a fingerprint key, as printed."""
    session, number = day
    return f"{number} (session {session})" if session else f"{number}"


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

    # In the order they are taken: "daystart" when the game starts, "dayend day=N" in the night
    # before day N (SIM::NewDay has already counted the day up), "hourHH" at every change of hour
    # at trace level 2 and "briefing day=N" once the peers meet at 9:00. Sorting the names
    # alphabetically put the briefing first, so a divergence the briefing sync repaired looked
    # lasting, and the one that really lasted was never reached. Within a day the game time
    # orders them; the briefing is taken after the change to 9:00 that shares its time.
    WHEN = {"daystart": 0, "dayend": 1, "briefing": 3}
    taken_at = {}
    for peer in peers:
        for f in peer["fps"]:
            t = f.get("t", "0")
            taken_at.setdefault((fp_key(f)[0], f["_when"]), int(t) if t.isdigit() else 0)

    # Sessions in the order the peers ran them.
    session_rank = {}
    for peer in peers:
        for f in peer["fps"]:
            session_rank.setdefault(f["_session"], len(session_rank))

    def order(key):
        session, day = key[0]
        rank = WHEN.get(key[1], 2)
        return (session_rank.get(session, 0), int(day) if day and day.lstrip("-").isdigit() else 0, taken_at.get((key[0], key[1]), 0), rank, key[1],
                str(key[2]))

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
                print(f"   {peer['path']} controls {local} players at day={show_day(day)} {when} (expected exactly 1)")
                break

    keys = sorted(common, key=order)

    def agrees(key):
        rows = [table[key] for table in tables]
        return len({r.get("hash") for r in rows}) == 1 and len({role(r) for r in rows}) == 1

    # "owner" is relative to the peer doing the reporting (itself 0, the others 2), so it
    # differs by design and is not evidence of anything. A human's money and credit are only
    # authoritative on that human's own peer and resynchronised every morning, so they are
    # not hashed either; listing them would bury the field that actually caused the mismatch.
    def comparable(key):
        rows = [table[key] for table in tables]
        local_only = {"owner", "t", "hash"}
        if key[1] != "briefing" and any(r.get("owner") in ("0", "2") for r in rows):
            local_only |= {"money", "credit"}
        return [k for k in rows[0] if not k.startswith("_") and k not in local_only]

    def differing(key):
        rows = [table[key] for table in tables]
        return [f for f in comparable(key) if len({r.get(f) for r in rows}) > 1]

    # A divergence the next sync repairs is a different animal from one that stays. The owner
    # resends money, image, routes and staff every hour and at the morning briefing, so a
    # message that crossed a nightly computation shows up once and is gone - worth listing,
    # but not worth stopping at while a lasting one may follow. Judged field by field: an
    # unrelated transient at the next fingerprint must not make this one look lasting.
    # With a fingerprint every hour a gap often spans several of them before the resend closes it,
    # so a field has healed once any later fingerprint agrees on it, not just the next one.
    position = {key: index for index, key in enumerate(keys)}
    by_player = defaultdict(list)
    for key in keys:
        by_player[key[2]].append(key)

    def heals_later(key):
        if len({role(table[key]) for table in tables}) != 1:
            return False
        later = [other for other in by_player[key[2]] if position[other] > position[key]]
        fields = differing(key)
        if not fields:
            # Only the hash differs - the pool line names no parts - so it has healed once a
            # later hash agrees.
            return any(agrees(other) for other in later)
        for field in fields:
            if not any(field in comparable(other) and len({table[other].get(field) for table in tables}) == 1 for other in later):
                return False
        return True

    # At trace level 2 each fingerprint comes with a hash per plane and per route: name the ones
    # that differ, so that "hplane" becomes "plane 3's flight plan".
    def detail_difference(key):
        rows = [peer["details"].get(key) for peer in peers]
        if any(row is None for row in rows):
            return ""
        found = []
        for name, label in (("planes", "plane"), ("routes", "route")):
            indices = set().union(*(row[name].keys() for row in rows))
            for index in sorted(indices, key=lambda value: int(value) if value.isdigit() else 0):
                values = [row[name].get(index) for row in rows]
                if len(set(values)) > 1:
                    if name == "planes" and all(value is not None for value in values):
                        what = "equipment" if len({v.split("/")[0] for v in values}) > 1 else "plan"
                        found.append(f"{label} {index} {what}")
                    else:
                        found.append(f"{label} {index}")
        return f" ({', '.join(found)})" if found else ""

    transient = [k for k in keys if not agrees(k) and heals_later(k)]
    if transient:
        print("   healed again later (the owner resends its state every hour):")
        transient_set = set(transient)
        for who, own in by_player.items():
            index = 0
            while index < len(own):
                if own[index] not in transient_set:
                    index += 1
                    continue
                start = index
                while index + 1 < len(own) and own[index + 1] in transient_set:
                    index += 1
                day, when, _ = own[start]
                fields = sorted({f for k in own[start:index + 1] for f in differing(k)}) or ["hash"]
                until = "" if index == start else f" until day={show_day(own[index][0])} {own[index][1]}"
                print(f"      day={show_day(day)} {when}{until} {'pool' if who == 'pool' else 'player ' + str(who)}: {' '.join(fields)}"
                      f"{detail_difference(own[start])}")
                index += 1
        print()

    for key in keys:
        if agrees(key) or key in transient:
            continue
        rows = [table[key] for table in tables]
        day, when, who = key
        print(f"   day={show_day(day)} {when} {'pool' if who == 'pool' else 'player ' + str(who)}{detail_difference(key)}")
        if len({role(r) for r in rows}) > 1:
            for peer, r in zip(peers, rows):
                print(f"      {'role':<10} {role(r):<24} {peer['path']}")
        for field in differing(key) + ["hash"]:
            values = [r.get(field) for r in rows]
            if len(set(values)) > 1:
                for peer, value in zip(peers, values):
                    print(f"      {field:<10} {value:<24} {peer['path']}")
        print("\n   (everything after this point is downstream of this divergence)\n")
        return

    if transient:
        print("   every divergence was healed again - none of them lasted\n")
    else:
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
