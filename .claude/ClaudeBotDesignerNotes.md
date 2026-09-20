ClaudeBot notes: how to build a plane design
============================================

Reference implementation: `src/BotDesigner.cpp` (MertenBot's offline search, bound to F7 in
`GameFrame.cpp:2126`). These are my own working notes, distilled from reading that file plus
`CXPlane` / `CPlaneParts` in `src/Editor.cpp`. Everything here is verified against the code, not
assumed.

What is actually needed at runtime
----------------------------------

- `gPlanePartRelations` — the only global that has to be read directly. 307 entries, indices
  0..306. No header declares it; `extern std::vector<CPlanePartRelation> gPlanePartRelations;`
  goes into my own .cpp (the type comes from `Editor.h`).
- `GetPlaneBuildIndex(shortname)` and `GetPlaneBuild(shortname)` from `Editor.h` — name/index
  mapping and the per-part record. Declared helpers, so no direct `gPlaneBuilds` access.
- `CXPlane::Calc*()` for every number that matters. **Part stats never have to be summed by
  hand** — that was my mistake in the earlier assessment. `gPlaneBuilds` is not needed: the only
  field BotDesigner touches on a `CPlaneBuild` is `BitmapIndex`, and only for cosmetics (below).

The part stats are data, not code
---------------------------------

**Trap.** The `gPlaneBuilds` / `gPlanePartRelations` literals in `CEditor::generateStaticData()`
are only defaults: the same function then overwrites them from `builds.csv` and `relation.csv` in
`ExcelPath` (= the active `<lang>/data` dir), and `CEditor`'s constructor re-imports them again.
Reading the C++ literals gives wrong numbers — I did exactly that and got a Beluga cost of 57.3M
against the real 60.3M, plus a bogus "fails the power check" conclusion.

What comes from where:

- **CSV `builds.csv`** — `Cost`, `Weight`, `Power`, `Noise`, `Wartung`, `Passagiere`, `Verbrauch`.
  All of it. Hull `Power` is negative in the live data (drag); `B2` is −10725.
- **CSV `relation.csv`** — `Offset2d`, `Offset3d`, `Note1..3`, `Noise`.
- **C++ only** — `Id`, `FromBuildIndex`, `ToBuildIndex`, `Slot`, `RulesOutSlots`, `zAdd`, and the
  row order. `resize(307)` pins the count, and the loader warns on an `Id` mismatch.

So the **topology and the relation indices are stable across data sets, the economics are not.**
A hardcoded `ParentRelationId` stays valid; a hardcoded *choice of parts* does not — the
`data_polish_20260903` table prices H2 at 160k instead of 800k and B1 at 900k instead of 10M,
which moves the optimum. Anything that picks parts must read the live table.

Ground truth for the two reference designs under `de/data` (measured in-game, not computed):

| | cost | pax | weight | power | speed | verbrauch | reichweite | buildable |
|---|---|---|---|---|---|---|---|---|
| Beluga (ATFS05) | 60,300,000 | 600 | 117,100 | 29,595 | 607 | 6,300 | 11,533 | yes |
| Ecomaster (ATFS08) | 17,200,000 | 95 | 40,600 | 10,200 | 339 | 200 | 7,458 | yes |

The header block at the front of a saved `.plane` file carries these same numbers, but `Load()`
reads and discards it — only `Name`, `Cost` and `Parts` are restored, and the stored `Cost` is 0.
Never trust that header; recompute with `Calc*()`.

Data model
----------

`CPlanePartRelation` (`Editor.h:21`) = one legal "part X may be glued onto part Y here":

| field | meaning |
|---|---|
| `Id` | the table's own id (100, 101, 200, …), **not** the index. Left/right engine pairs are `Id` and `Id + 10` |
| `FromBuildIndex` | parent part index, or `-1` for "hull, placed on the empty desktop" |
| `ToBuildIndex` | the part being attached |
| `Offset2d`, `Offset3d` | added to the parent's `Pos2d` / `Pos3d` to place the child |
| `Note1..3` | the effect notes: `NOTE_PILOT*`, `NOTE_BEGLEITER*`, `NOTE_SPEED*`, `NOTE_VERBRAUCH*`, `NOTE_KAPUTT*` |
| `zAdd`, `Noise` | draw order; extra noise |
| `Slot` | the slot this attachment occupies (`B*`, `C0`, `H0`, `Rx`, `Lx`, `M1`..`M6`, `MR`/`ML`, `Mr`/`Ml`) |
| `RulesOutSlots` | concatenated 2-char slot names this attachment blocks |

`CPlanePart` (`class.h:762`) = one placed part: `Pos2d`, `Pos3d`, `Shortname` ("B2"),
`ParentShortname`, `ParentRelationId`.

**`ParentRelationId` is the index into `gPlanePartRelations`, not `Id`.** Confirmed at
`Editor.cpp` in `GetError()`, `CalcSpeed()`, `IsSlotFree()`, `Sort()` — all subscript the vector
with it directly. The hardcoded Ecomaster uses `ParentRelationId = 0` for its B1 hull, which is
`Id 100`.

Parts inventory: 5 hulls (B), 5 cockpits (C), 7 tails (H), 6 right wings (R), 6 left wings (L),
8 engines (M). See `NUM_PLANE_*` in `Editor.h`.

What the positions are worth
----------------------------

`Pos2d` / `Pos3d` feed only: the offset chain to children, the editor's own hit-testing, and
`SortIndex` (draw order) in `CPlaneParts::Sort()`. **No `Calc*()` and no game rule reads them.**
So they are cosmetic, and the bitmap dependency below can be avoided.

The hull is the only part attached to the desktop, and it is the only place BotDesigner needs a
bitmap: `Pos3d = rel.Offset3d - mPartBms[GetPlaneBuild(hull).BitmapIndex].Size / 2`, which is why
it loads `editor.gli`. `Pos2d` is just `rel.Offset2d`. Two half-sizes can be read straight off the
hardcoded designs instead of loading graphics:

- B1 (relation index 0, `Offset3d` 320,190) → `Pos3d` (247,137), `Pos2d` (-44,-75)
- B2 (relation index 1, `Offset3d` 320,190) → `Pos3d` (174, 99), `Pos2d` (-107,-120)

Attaching one part
------------------

`BotDesigner::buildPart()`, distilled. `position` selects *which* of the legal attachment points
to use — it is an ordinal, not a coordinate.

```
attach(plane, shortname, position):
    n = position
    chosen = -1
    for c in candidateRelations[GetPlaneBuildIndex(shortname)]:
        rel = gPlanePartRelations[c]
        if not plane.Parts.IsSlotFree(rel.Slot):      continue
        if rel.FromBuildIndex == -1:                  # hull onto the desktop
            if n-- == 0: chosen = c; parent = none
        else:
            for each placed part d:                   # every matching parent counts
                if GetPlaneBuildIndex(d.Shortname) != rel.FromBuildIndex: continue
                if n-- == 0: chosen = c; parent = d
    if chosen == -1: return NoMorePositions           # this variant is exhausted
    append CPlanePart{ Pos3d = parent.Pos3d + rel.Offset3d   (hull: rel.Offset3d - bmSize/2),
                       Pos2d = parent.Pos2d + rel.Offset2d   (hull: rel.Offset2d),
                       Shortname, ParentShortname = parent.Shortname,
                       ParentRelationId = chosen }
    append the mirror part if there is one (below)
    plane.Parts.Sort()
```

Note the asymmetry in `IsSlotFree(rel.Slot)` (`Editor.cpp:1914`): it tests the *new* relation's
`Slot` against the `RulesOutSlots` of every *already placed* relation. It does not test the new
relation's own `RulesOutSlots` against the existing slots. Replicate as-is; order of placement
therefore matters.

Symmetry
--------

Wings and engines are always placed in pairs, and the mirror relation sits at index `c ± 1`
because the table emplaces R and L adjacently. `GetError()` enforces the pairing (engine on an R
wing needs a matching engine on an L wing with `Id + 10`), so a plane built without the mirror is
never buildable.

| new part | mirror | `add` | mirror parent |
|---|---|---|---|
| `Rn` | `Ln` | +1 | same hull |
| `Ln` | `Rn` | −1 | same hull |
| `Mn` on a tail (`H*`) | `Mn` | −1 if `rel[c-1].FromBuildIndex == rel.FromBuildIndex` else +1 | the same tail part |
| `Mn` on a wing | `Mn` | −1 if `rel[c-1].ToBuildIndex == rel.ToBuildIndex` else +1 | the *other* wing part in the plane |

The mirror part gets `ParentRelationId = c + add` and — a quirk worth keeping, since the hardcoded
designs have it too — `ParentShortname` copied from the *original* parent, so the left-wing engine
claims `"R6"` as its parent. Harmless: `ParentShortname` is only read by the editor's delete
logic (`Editor.cpp:1632`).

Slot naming: `M1..M3` = right wing, `M4..M6` = left wing, `MR`/`ML` = wing-root M5,
`Mr`/`Ml` = tail-mounted M5 (only on H2, H5, H7).

Validity
--------

`CXPlane::IsBuildable()` = has at least one B, C, H, R and M part, and `GetError()` empty:

1. every engine on an R wing has a partner engine on an L wing with `Id + 10`, and vice versa;
2. `CalcPower() * 4 >= CalcWeight()`.

(The wing-area check is commented out in the shipped code.)

Shrinking the search space
--------------------------

BotDesigner pre-builds `map<ToBuildIndex, vector<relationIndex>>` and drops:

1. relations with `Slot == "Ml"` — the tail-left engine, which the mirror step adds anyway;
2. every relation whose `FromBuildIndex` is a left wing (`L1..L6`) — likewise;
3. duplicates: for the same `ToBuildIndex`, a relation with the same `FromBuildIndex`, the same
   `Note1/2/3`, the same `Slot` and the same `RulesOutSlots` as one already kept is redundant —
   it differs only in where the part sits on the parent. Keep whichever has the lower `Noise`.

Enumeration
-----------

Eight iterators, each `{prefix, minVariant, maxVariant, canBeOmitted}`, tried in this order:

| # | prefix | variants | optional | role |
|---|---|---|---|---|
| 0 | `B%d` | 1..5 | no | hull |
| 1 | `R%d` | 1..6 | no | right wing (left comes free) |
| 2 | `C%d` | 1..5 | no | cockpit |
| 3 | `H%d` | 1..7 | no | tail |
| 4 | `M%d` | 1..8 | yes | secondary engine pair |
| 5 | `M%d` | 1..8 | no | primary engine pair |
| 6 | `M%d` | 5..5 | yes | secondary aux engine (`MR`/`ML` or `Mr`/`Ml`) |
| 7 | `M%d` | 5..5 | yes | primary aux engine |

`resetPosition()` sets `position = canBeOmitted ? -1 : 0`; a negative position means "skip this
part", and the first `position++` brings it into play.

Canonical-ordering prune: if the primary and secondary engine iterators hold the same variant and
`primary.position < secondary.position`, reject immediately (`InvalidPosition`) — otherwise every
identical engine pair is enumerated twice. Same for the two aux iterators.

Backtracking after `buildPlane()`:

- success → `lastIterator.position++`, continue;
- `InvalidPosition` at `i` → `iter[i].position++`, reset all iterators after `i`, continue;
- `NoMorePositions` at `i` → reset `iter[i].position`, then `iter[i].variant++`; if that
  overflows `maxVariant`, reset the variant and do `iter[i-1].position++`; if `i == 0`, stop.

Then `IsBuildable()` gates scoring.

Scoring
-------

Geometric (multiplicative), because the terms have wildly different ranges:

```
noise    = clamp(CalcNoise(), .., 111) - 60, floored at 0     # ≤60 is free, >111 impossible
wartung  = (CalcWartung() + 100) / 100
base     = CalcReichweite() / sqrt(1 + noise/10) / sqrt(1 + wartung)
score    = base * passagiere / sqrt(preis) / verbrauch        # "Standard"
```

Variants: VIP swaps `passagiere` for `speed`; FastPassengers multiplies by `passagiere * speed`;
FastFast multiplies by `speed²` and drops the `preis`/`verbrauch` divisors. `reichweite` is
recomputed inline as `CalcTank() / verbrauch * speed` purely to save the repeated calls.

Mission filters just zero the score: ATFS05 needs `passagiere >= BTARGET_PLANESIZE` (600),
ATFS08 needs `verbrauch * 100 / speed <= BTARGET_VERBRAUCH` (500).

Top-5 per score type, deduplicated by the stat tuple (range, speed, passengers, consumption,
price, noise, maintenance) so near-identical layouts don't fill the list.

Practical consequences for ClaudeBot
------------------------------------

- The full sweep is an **offline** tool (F7), writing `myplanes/botplane_*.plane`; the two winners
  were then transcribed into `Helper::getHardcodedDesignerPlaneLarge/Eco()`. Cost of a full sweep
  is untested — do not assume it fits in a callback.
- For ATFS05/ATFS08 the hardcoded designs are enough, so the search is optional. If I ever want a
  free-game design, a targeted search (fix the hull, sweep engines/wings) is the cheap version.
- `GameMechanic::buyXPlane()` loads from a **file**, so a design must be `Save()`d first; the
  `myplanes` directory (`AppPath + MyPlanePath`) may not exist yet. It never calls
  `IsBuildable()` — validity is enforced by the editor UI, not by the purchase.
- Cost, weight, power and passengers are plain sums over the parts, so they are **independent of
  the attachment positions**. Only speed, consumption, pilots, crew, noise and maintenance depend
  on the relation notes. That splits any search into a cheap exact phase over part multisets and
  an expensive phase that only the survivors need.
- `PLAYER::BuyPlane()` copies the design's `Calc*()` results straight into `CPlane` with no
  randomisation, then calls `Planes.Sort()` — cached plane indices go stale.

See also `.claude/RULES.md` "Designer actions" and the CXPlane object section.

How ClaudeBot gets its designs (decided 2026-09-20)
---------------------------------------------------

**Generated at runtime by the exhaustive search, not hardcoded.** The part economics live in
`builds.csv` and differ between data dirs, so a hardcoded *choice of parts* is optimal for one
table only; relation indices are stable, so such a design stays valid, just not good.

- `BotDesigner::findBestPlaneForGoal(ScoreType, CXPlane &)` — one goal, no files, no logging.
  `findBestDesignerPlane()` (F7) is unchanged: all six goals, saves and prints.
- Goals `ScoreType::ClaudeMiss05` / `ClaudeMiss08` are ClaudeBot's own, so tuning them never
  moves MertenBot's designs.
- Score: `reichweite(cap 6000) / sqrt(1+wartung) * pax * noiseFactor / sqrt(verbrauch) / preis^2`,
  with the mission bar as a hard filter. **No speed term** - tried, and it bought a 65.0M plane
  over the 60.3M one, because `Schedule.cpp` caps route passengers at the player's share of
  `qRoute.Bedarf`, so seats and trips are already over-supplied on a 600-seater.
- Hull prune: a hull that cannot reach a goal's passenger floor, even with the largest possible
  non-hull contribution read off the live table, fails on the hull iterator so backtracking skips
  its whole subtree. ATFS05 8.8 s -> 2.77 s for an identical design. Applies only when every goal
  in the run wants the passengers, so the F7 run stays exhaustive (verified: 1,738,530 builds).

Measured on `de/data`, seed 1: ATFS05 **55.7M** (vs 60.3M reference), ATFS08 **13.6M** (vs 17.2M).
Runtime 2.8 s / 8.8 s, once per game, only on those two missions.

Open: the buying logic in ClaudeBot, and tuning `kClaudePriceExponent` / `kClaudeReichweiteCap`
against day-to-win. The price exponent barely moves the pick; the range cap does most of the work.
