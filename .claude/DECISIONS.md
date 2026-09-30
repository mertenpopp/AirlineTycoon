## 2026-09-02 — Comparison session (no code changes)

Compared `Bot*` (MertenBot) against `ClaudeBot*` at HEAD 16d5116c. Three 300-game batches.

| Matchup | ClaudeBot (HA) | Bot |
|---|---|---|
| Solo (`run_measurement_claudebot.sh` / `run_measurement_bot.sh`) | 8.364e9 (med 8.545e9) | 11.032e9 (med 11.06e9) |
| Head-to-head (`run_competition.sh`, lvl 24) | 2.337e9 (med 1.802e9) | 2.879e9 (med 3.170e9), wins 240/300 |

Both bots survive to day 99 in 300/300 of every batch — the old "Bot liquidates ClaudeBot
by day 39" failure is gone.

Where the solo gap comes from (day-99 lifetime totals; the CSV columns other than
Saldo/Gewinn/Verlust are cumulative `BilanzGesamt`, not per-day):

- Operating revenue: Bot 12.115e9 vs ClaudeBot 9.312e9.
  - Tickets: 11.824e9 vs 9.257e9. Jobs: 0.198e9 vs 0.054e9. Freight: 0.093e9 vs 0.0005e9.
- ClaudeBot draws on 2 job sources (travel agency + domestic freight); Bot's `BotPlaner`
  draws on 5 (adds last-minute, international passenger, international freight) and solves
  with simulated annealing instead of greedy gap-filling.
- Efficiency: Bot 2.42M pax on 134 planes / 28 routes (272 pax per flight); ClaudeBot 1.64M
  on 200 planes / 130 routes (202 pax per flight). ClaudeBot spreads over ~5x the routes,
  pays 2.3x the route rent and 2.8x the ad spend, and ends with image 530 vs Bot's 721.
- ClaudeBot's fuel is *cheaper in scored terms* (-0.760e9 vs Bot's -0.914e9): `KerosinGespart`
  is not in `GetOpVerlust()`, so Bot's tank arbitrage saves cash the metric never credits.
  ClaudeBot's `kUseFuelArbitrage = false` is correct for this objective.

Head-to-head: ClaudeBot ends at the overdraft floor (Geld = -1e7) in 300/300 games, Bot in
0/300. ClaudeBot's outcome is bimodal — 213/300 plateau at ~1.8e9 with 50-80 planes, 25
break out to 120-200 planes and ~7.9e9. Its self-holding is still 8,000 shares
(`executeStock` emits max with mode 0; `buyStock` appears nowhere in ClaudeBot.cpp) while
Bot rebuys to 51% and books 1.71e9 of Takeovers.

Caveat: both scripts run Bot at BotLevel 2. Level 3 is the threshold for
`ROBOT_USE_MUCH_SABOTAGE` (Player.cpp:7915) and lowers `threshNoRun` so it runs between
rooms (Bot.cpp:309). ClaudeBot at level 4 always sets `Running = TRUE`. Bot wins with a
handicap.

Next candidates for ClaudeBot, in expected-value order:
1. Concentrate routes: cap `kRoutePairsPerHundredPlanes` far below 38 and push image/planes
   into fewer pairs — the 130-route spread is where revenue per flight is lost.
2. Add last-minute and international job sources (`ACTION_CHECKAGENT1`,
   `ACTION_CALL_INTERNATIONAL`) — Bot earns 3.7x on jobs and 174x on freight.
3. Buy back to 51% of own stock in `executeStock`; add a nemesis stake.

### Correction + A/B on the takeover (same session)

My first pass reported "ClaudeBot eliminated in 0/300". That was wrong: I tested for the
fleet dropping to zero. `GameMechanic::bankruptPlayer` (GameMechanic.cpp:38) does call
`Planes.ReSize(0)`, but the player's *statistics freeze at their last recorded values*, so
`Flugzeuge` never reads 0 in the CSV. The reliable signature is
`Geld == 2 * DEBT_GAMEOVER == -10,000,000` together with `SaldoGesamt` constant to day 99.

Corrected numbers, head-to-head (`run_competition.sh`, lvl 24):

| build | ClaudeBot taken over | median day | ClaudeBot saldo | Bot saldo |
|---|---|---|---|---|
| current (gate present) | **281/300** | 78 | 2.337e9 | 2.879e9 |
| A/B: `isLateGame()` gate removed | **300/300** | 39 (min 33, max 48) | 4.06e7 | 1.185e10 |

So ClaudeBot's 2.337e9 is a frozen day-78 snapshot, not a day-99 result. The 19 games it
survives are its good ones: median 8.09e9 on 200 planes vs Bot's 1.00e9. In the 281 it
loses: median 1.79e9 on 62 planes. Legacy player FL is taken over in 299/300 too.

Cause established by A/B, not inference: commit `e7590e17` (2026-09-02) added
`if (!isLateGame()) return Prio::None;` to `Bot::condBuyNemesisShares`. `isLateGame()` is
fleet >= 12, which Bot reaches at median day 78 in this matchup. Reverting that single line
and re-running 300 games reproduces the old day-39 liquidation exactly. ClaudeBot's own
share code is unchanged since it was first added (`git log -S executeStock`), so the gate
merely bought it 39 days - the blind spot itself is untouched.

This raises the priority of the 51% self-stake fix: it is worth ~6.3e9 of head-to-head
score (the survivors' 8.09e9 median vs the 1.79e9 of the games it loses), which dwarfs the
route-concentration and job-source items above.

(`src/BotConditions.cpp` was patched temporarily for the A/B and has been restored; build
re-installed from clean sources.)

### Head-to-head measurement of `c01acd1e` "Bot: Earlier late game condition"

`isLateGame()` (fleet >= 12) became `checkLateGame()` = `weeklyOpSaldo > 1e8 || fleet >= 8`.
The commit measured solo only (11.008e9, unchanged). Head-to-head, 300 games, lvl 24:

| gate on `condBuyNemesisShares` | trigger day (median) | ClaudeBot taken over | Bot h2h | ClaudeBot h2h |
|---|---|---|---|---|
| `fleet >= 8 \|\| saldo > 1e8` (c01acd1e) | **50** | **300/300** @ day 50 | **7.792e9** | 1.773e8 |
| `fleet >= 12` (16d5116c) | 78 | 281/300 @ day 78 | 2.879e9 | 2.337e9 |
| no gate (A/B) | ~30 | 300/300 @ day 39 | 1.185e10 | 4.06e7 |
| nemesis buying disabled (A/B) | never | 0/300 | 1.235e9 | 8.415e9 |

Bot solo for reference: 11.03e9 (11.008e9 with this commit).

**Yes, it triggers earlier and yes, it largely fixes the head-to-head deficit.** Bot goes
2.879e9 -> 7.792e9 (2.7x), closing 66% of the gap to its solo score. The mechanism is the
one established earlier: the gate delays Bot's **liquidation** of ClaudeBot, and until then
Bot is starved of crew (ClaudeBot hires every applicant) and cannot convert cash into
planes. Firing 28 days earlier gives Bot 28 more days of expansion - fleet 62 -> 108,
routes 11 -> 21, passengers 5.7e5 -> 1.52e6, flights 2424 -> 5728.

The relationship is monotone in trigger time: the earlier the switch, the better Bot does,
with no gate at all still the best (1.185e10, *above* its own solo score). Job and freight
revenue are flat across all four configurations (~2.0e8 / ~9.2e7); the entire difference is
ticket revenue, i.e. fleet size.

`SecurityKosten` is 0 in every configuration, so the extra `checkLateGame()` call sites
(`condVisitSecurity`, `actionVisitSecurity`) cost nothing here - `ROBOT_USE_SECURTY_OFFICE`
returns false for SuperBot outside DIFF_ATFS04/06 (Player.cpp:8007).

Caveat: measured against ClaudeBot, whose 8,000-share self-holding makes it trivially
takeable. Against an opponent that holds 51% of itself, an earlier `checkLateGame()` would
buy Bot far less, and could cost it by pulling spend forward into shares it cannot convert
into control.


### Terminology correction: Bot liquidates, it does not take over

`Bot::actionOvertakeAirline` calls `GameMechanic::overtakeAirline(qPlayer, airline, true)` -
the third argument is `liquidate`, so `Sim.Overtake = 2` (GameMechanic.cpp:966) and the
**Liquidieren** branch runs. Bot never uses the `Sim.Overtake == 1` (Uebernahme) branch.

Nothing is transferred to Bot. In the liquidation branch the target's planes are *sold*
(`sellPlane`), gates set to `Miete = -1`, routes and cities set to `Rang = 0`, its
shareholdings sold, and `Money - Credit` distributed pro rata to shareholders under category
**3181 "Liquidierung von %s"** (Player.cpp:530). `bankruptPlayer` then calls `SackWorkers()`,
which returns every employee to the pool as `WORKER_RESERVE` at `OriginalGehalt`
(Player.cpp:6566).

So the `Takeovers` CSV column is a cash payout on Bot's ~51% stake, not an asset transfer -
categories 3180 and 3181 both book to `Bilanz.Takeovers`, which makes the column name
misleading.

Confirmed in the data: aligning all 300 games on the liquidation day, Bot's resources climb
over about a fortnight instead of jumping - employees 51 -> 58 -> 90 -> 108 -> 167 and
routes 4 -> 6 -> 8 at offsets -1/0/+2/+4/+12. A transfer would show a step at offset 0.
Bot re-acquires the freed crew and routes through its normal HR and route-box actions, in
competition with FL. That is why the timing of `checkLateGame()` matters so much: every day
earlier adds a day to the re-acquisition runway.


### Route ticket pricing for Bot (2026-09-03)

Reworked `Bot::planRoutes()` ticket pricing. Measured with `run_measurement_bot.sh`
(300 games, mean cumulative operative saldo day 99, HA). Noise floor ~0.1e9: two runs of
*identical* code gave 11.674e9 and 11.636e9, and the runs are not seed-reproducible
(pairing run n against run n gives the same SEM as unpaired, wins 152/300).

| config | price target (x highCost) | keep rule | saldo |
|---|---|---|---|
| committed `0bc11a38` | ~1.33 / 1.67, effectively frozen | factor rounded to integer | 10.94e9 |
| recompute daily | 1.90 | none | 8.84e9 |
| wide band | 1.80 | [1.05, 3.00] | 11.09e9 |
| wide band, high | 1.90 (clamped) | [0.84, 3.95] | 11.08e9 |
| pinned to kerosene 700 | 1.90 @ 700, i.e. 2.7-4.4 live | never raises | 10.55e9 |
| **top of tier 2** | **1.90** | **[1.60, 1.98]** | **11.65e9 / 11.67e9** |
| top of tier 2, raise sooner | 1.90 | [1.75, 1.98] | 11.36e9 |
| top of tier 2, raise later | 1.90 | [1.45, 1.98] | 11.55e9 |
| unclamped target | 1.97 | [1.60, 1.98] | 11.72e9 (t=0.90, not significant) |

Three mechanics drive this, all in the base game:

1. **Raising a price is expensive, lowering is free.** `PLAYER::UpdateTicketpreise`
   (Player.cpp:6893) calls `FlightChanged()` on every *already scheduled* flight of the route
   when the new price is higher, which re-stamps `HoursBefore` to the hours left until
   departure. That flight then loses its advance-booking bonus `tmp * (HoursBefore + 48) / 96`
   (Schedule.cpp:330) - up to half its passengers. The committed code quantised the factor to an
   integer multiple of `cost`, so it changed the price 14 times per game; recomputing from the
   live kerosene price every day changes it 426 times (189 raises) and costs 19% of the score.
2. **Revenue is flat above ~1.9 * highCost.** Passengers are scaled by
   `(highCost - 10) / price` above `highCost` (Schedule.cpp:344), which cancels the price
   exactly. Below that the plane still fills, so revenue grows linearly. The `keepMin` sweep
   locates the fill point at ~1.6 * highCost. Also note `Gewichte[c] = 10000 / Ticketpreis`
   (Schedule.cpp:256) uses *our* price for all four players, so price does not affect our
   demand share at all.
3. **Image tiers are decided by truncation.** `Add / 10` (Schedule.cpp:992) truncates toward
   zero and a well kept plane earns `Add = 21` (condition +10, four upgrades +8, crew +3).
   So per flight: below 1.5 * highCost +1 image, below 2.0 nothing, above 2.0 **-1**. The third
   and second tier are *not* equal, which is why pricing high bleeds image and forces ad
   spending: image 725 / ads 0.96e8 at 1.90 vs image 265 / ads 3.08e8 when pinned high.

Optimum is therefore the top of tier 2: highest revenue at no image cost, held stable inside a
band so the kerosene price rarely forces a raise. Lower freely at >1.98 (free, avoids tier 3),
raise only below 1.60 (where revenue actually starts dropping).

`mOptions.kMaxTicketPriceFactor` (5.7 = 1.9 * highCost) clamps the target to exactly 1.90.
Raising it to 5.94 and targeting 1.97 measured +0.5 sigma - not significant - so it was left
alone, which also keeps the `BotLevel <= 1` handicap (Bot.cpp:189) intact.

Not pursued: first class pricing must stay below 3 * highCost, where the same scaling factor
kicks in for first class (Schedule.cpp:513) and cuts first class passengers to a third at once.
`kTicketPriceFactorFC = 2.85` respects this, but it is untested - Bot buys no first class seats
(one FC seat costs two regular ones), so `MaxPassagiereFC` is 0 on all its planes.
`RouteInfo::ticketCostFactor` is now unused but still serialised.

## 2026-09-16 — Comparison session (no code changes), day-59 objective

First comparison since the objective moved from day 99 to day 59 (`adbf4bd4`), so none of the
numbers above are comparable. Three fresh 300-game batches at HEAD `d44f757f`.

| Matchup | ClaudeBot (HA) | Bot |
|---|---|---|
| Solo (`run_measurement_claudebot.sh` / `run_measurement_bot.sh`) | **7.079e8** (med 7.594e8, sd 1.37e8, n=295+) | **1.760e9** (med 1.824e9, sd 3.28e8) |
| Head-to-head (`run_competition.sh`, lvl 24) | 3.215e8 | **1.943e9**, wins **298/298** |

ClaudeBot is liquidated in **296/300** head-to-head games, median day **52** (min 45, max 59) -
`Geld == -1e7` and SaldoGesamt frozen. It still emits with mode 0 and never buys back, so its
self-holding is 8,000 shares. Bot is taken over in 0/300 of either batch.

Where the solo gap comes from (day-59 lifetime totals, mean):

- Bot's early engine is jobs, ClaudeBot has none: Auftraege+Fracht 1.44e8 vs 3.9e6. Through
  day 20 that *is* Bot's income, and it is what funds the first route planes.
- Growth curve: fleet at day 30/40/50/59 - Bot 4.9 / 9.6 / 27.6 / 54.4, ClaudeBot 3.0 / 4.7 /
  10.4 / 26.1. ClaudeBot flies its two starting planes until day 22 (kMinFleetBeforeSaving)
  and is ~8 days behind for the rest of the game.
- Concentration: Bot 7.9 pairs for 54 planes, 203 pax/flight, 6,452 of ticket revenue per
  passenger; ClaudeBot 21.4 pairs for 26 planes, 179 pax/flight, 5,193 per passenger.
- ClaudeBot spends *more* on advertising with a third of the fleet: 1.25e8 vs 8.5e7.

Findings about Bot from reading its sources against ClaudeBot's:

1. **Bug in `d44f757f`** (`updateRouteInfoBoard`): for a competitor `i`, the reverse direction is
   read from `getReverseRentRoute(route)`, which is `qPlayer`, not `qqPlayer` - so every
   competitor contributes `(their outbound + OUR return) / 2` to `route.routeUtilization`.
   With BERATERTYP_INFO employed and three competitors, our own return-direction load is counted
   1.5 extra times, which pushes `routeUtilization` over the `< 90` gate in
   `routesFindNextStep()` and stops the route buying planes. Same expression, same bug, in the
   `routeOwnUtilization` line is correct (it is our own player there).
2. ~~Cabin fit-out and food are image-neutral at Bot's own price point.~~ **Corrected later the
   same day - wrong as written.** `BookFlight` (Schedule.cpp:925-998) gives Add = +10 (Zustand>98)
   +3 (crew) + cabin - 20 (price is between 1.5x and 2x Costs2, and Costs2 == Bot's `highCost`),
   and `Add / 10` truncates toward zero. The mistake was assuming a new plane's cabin starts at
   level 1: **all four items start at 0** (Planetyp.cpp:226-229, -1 each). So with Zustand > 98:
   full fit-out +1 -> 0, food 2 only -8 -> 0, **food 0 or 1 -11/-10 -> -1 image per flight**.
   `kPlaneFoodTarget = 2` is therefore load-bearing, not waste. The late-game luxury arm is only
   neutral while Zustand > 98; below that, full fit-out -9 -> 0 against food-only -18 -> -1, which
   is a plausible mechanism for c8f71c28's +6.1e7.
3. Airline image is negative until ~day 36 and falls 539 -> 442 over the last nine days while the
   fleet doubles. Image is step 7 of `routesFindNextStep()`, so it only gets cash when no route
   wants a plane or an ad.

Transferable from ClaudeBot, in expected-value order: the weekend/decay-aware image target
(`mImageDecayPerDay * daysToCover`, agency shut Sat+Sun), the marginal-payback rule for airline
image (`kImagePaybackDays`), folding `DoBodyguardRabatt` into the plane count and calling
`buyPlane()` repeatedly instead of once, `WorkCountdown = 2` on filler actions, and fitting
agency/freight jobs into the overnight idle windows of route planes (Bot's route planes never
visit the agency again).

### Same day, later: Bot-side experiments (idle actions, bodyguard discount, earlier routes)

All batches below are 300 games, mean day-59 `SaldoGesamt` for HA, per-run sem ~2.0e7 - so
nothing under ~5e7 is decidable from a single pair of runs.

| build | score | note |
|---|---|---|
| `0d4e8188` (reference) | 1.762e9 | measured here; commit `ee02d9e2` reported 1.748e9 |
| `c9d2df9f` / `12655bd8` "Shorter idle actions" | 1.715e9 / 1.741e9 | + `kFrequencyRouteStrategy` 2 -> 1 |
| `dc8f04ba` "Check discount when buying planes" | 1.751e9 | t=-0.36 against `12655bd8` |
| A/B: no truncation in `findBestRoute()` | **0.593e9** | t=-47 |
| A/B: rank routes by profit per dollar of capital | **0.634e9** | t=-46 |

**The `d44f757f` reverse-utilization bug is fixed** in `ee02d9e2`: `route.routeUtilization` now
reads `qqPlayer.RentRouten.RentRouten[route.routeReverseId]` instead of our own player's.

#### Shorter idle actions: works mechanically, worth nothing

`WorkCountdown = 2` on rooms where the bot did nothing (kiosk, telescope, and Arab / Rick /
duty-free / broker when they had no item business). Placement is correct - `PLAYER::RobotPump`
sets `WorkCountdown = 20 * 5` *before* calling `RobotExecuteAction()` (Player.cpp:4204) and the
`ROBOT_USE_WORKQUICK*` divisors only touch values `> 2`.

It buys slots that nothing wants. Actions per day over 30 games (1,740 days):

| build | Top | Higher | High | Medium | Low | Lowest |
|---|---|---|---|---|---|---|
| reference | 1.3 | 19.9 | 54.9 | 8.8 | 2.2 | 15.7 |
| idle actions | 1.3 | 20.0 | 55.6 | 8.8 | 2.2 | 20.8 |
| + `kFrequencyRouteStrategy` 1 | 1.3 | 20.0 | 56.1 | 9.6 | 2.1 | 25.2 |

Per-action counts per game are flat for everything useful (`CHECKAGENT2` 1170 -> 1176,
`CHECKAGENT3` 399 -> 398, `VISITROUTEBOX` 98 -> 98, `VISITMECH` 116 -> 116, `PERSONAL` 83 -> 82)
while the idle rooms absorb the entire gain (Rick 212 -> 269, telescope 191 -> 268, kiosk
203 -> 258). Every useful action is gated by its own `hoursPassed`/`minutesPassed` cadence or a
state precondition, so cheaper idle actions cannot make one due any earlier - and the baseline
was never slot-starved (it already idled ~16 actions a day). Halving `kFrequencyRouteStrategy`
raised route-box visits 98 -> 136 per game and changed nothing else.

Log volume is unaffected: mean 7.51 / 7.49 / 7.90 MB per game. (An earlier claim in this session
that logs grew 7.8 -> 13.2 MB was wrong - that compared two single games, and the per-game spread
dwarfs the effect.)

Two bugs found and fixed while reviewing it:
- `findBestAvailablePlaneType()` was rewritten to return `{}` when the catalogue is unchanged, and
  the caller did `mBestPlaneTypeId = list.empty() ? -1 : list[0]` - so the id was cleared on
  almost every broker visit (78 "no new types" against 6 real refreshes per game). Fixed by
  assigning only when non-empty; the post-purchase `mBestPlaneTypeId = -1` in the non-route branch
  was removed as well (it would have stuck at -1 forever under the new caching).
- `condVisitMakler()` floored at `condVisitMisc()`, making the broker a permanent filler room:
  633 visits per game (1,028 with `kFrequencyRouteStrategy` 1), each rebuilding a `CDataTable` via
  `GameMechanic::getAvailablePlaneTypes()`. Now ~39 per game.

#### Bodyguard discount

`PLAYER::DoBodyguardRabatt` (Player.cpp:6737) refunds `Money / 100 * (quality / 10)`, only when
quality > 20, capped at 10% by talent <= 100. It is called for planes (new / used / designer),
kerosene, tanks, advertising, duty-free items and sabotage - **not** for rocket or station parts
(`AddRocketPart` just books `ChangeMoney(-price, 3400)` and has no affordability check at all).

The refund is paid *after* the purchase, while `buyPlane`/`buyXPlane`/`buyUsedPlane`/
`buyAdvertisement` all validate at **list** price against `DEBT_LIMIT` (-1e6). So the discounted
price may size a batch but must never gate affordability: an early version that did gate on it
produced 330 refused purchases in 40 games, clustered at 22.75-24.0M against the 25M 767-300 ER
(= 0.91 x price up to price - DEBT_LIMIT). It also returned 0 below the talent threshold, which
would have divided by zero in `actionBuyKerosineTank` and could hang `actionBuyAds` (that loop
ignores `buyAdvertisement`'s return value, and the call refuses without spending).

The committed form (`dc8f04ba`) is correct: gate on `Preis`, size the batch with the discounted
price, then call `buyPlane(type, 1)` in a loop and stop when it refuses, so each refund funds the
next unit. Refused purchases: 0. Planes per broker visit 1.57 -> 1.66.

**It still does not move the score (t=-0.36), and cannot:** `BodyguardRabatt` at day 59 is
1.775e8 (reference) against 1.751e8 - the rebate was always being collected. The change only
moves the marginal plane earlier by at most one broker visit (~1 in-game hour, with the 1h
cadence), and the fleet curve is identical at every checkpoint (day 15/25/35/45/55/59:
2.26/3.70/7.04/15.40/45.29/55.89 against 2.23/3.63/6.99/15.36/44.96/55.64).

#### Why throughput and cash-timing changes cannot pay

The fleet is **cash-flow bound**. `routesRecalcNextStep()` logs "Need to buy another ..." ~3,200
times per game against ~56 purchases: the bot knows what it wants on essentially every planning
round and is waiting for money. Funding at day 59: `FlugzeugKauf` -1.70e9 against net equity of
**+0.9e7** (2.87e7 emitted - 1.53e7 own-share buy-back - 0.48e7 fee) and 6.5e7 of new credit, so
~96% of the fleet comes out of operating cash flow. Anything that neither adds capital nor raises
revenue per plane-hour lands inside the noise.

#### The early game is where the score is - and the Il 62 trap

First route and first plane arrive on the *same day*, median **day 14-15** (range 11-20): the
airline spends two weeks as a two-plane charter operation saving for a 25M wide-body. Fleet then
doubles every ~7-9 days, so moving that curve left is worth far more than any percent-level knob.

Two attempts to start earlier, both catastrophic, both for the same reason:

1. **No truncation in `findBestRoute()`** (0.593e9): the loop takes the first *affordable*
   candidate in score order, so without the top-5 cut it falls through to the cheapest combination
   - the **Ilyushin Il 62** at 9.9M. Fleet over 20 games: 508 Il 62 + 118 Boeing 720 + 158 767.
2. **Ranking by profit per dollar of capital** (0.634e9): now the Il 62 is the *top* candidate -
   988 of 1030 planes. With `profitPerWeek ∝ numPlanesTarget` and the denominator `∝ numPlanesMin`
   the plane count cancels and the score reduces to `const × (revenue/trip - fuel/trip) / Preis`:
   Il 62 ~0.072 per million against the 767-300 ER's ~0.065.

Both collapse the same way: kerosene 1.64e8 -> 3.8-5.1e8, passengers -26 to -36% on *more*
flights, ticket revenue roughly halved. The Il 62 carries 198 seats on 10,500 l/h (53 fuel/seat)
and the Boeing 720 165 on 11,000 (66.7), against the 767-300 ER's 290 on 2,800 (9.7).

Three mechanics make this a trap rather than a trade-off:
- **`FlugzeugKauf` is not in `GetOpVerlust`, kerosene is.** Optimising return on capital trades an
  unscored one-off for a scored recurring cost: the second experiment saved 1.17e9 of purchase
  price the metric ignores and bought 3.5e8 of fuel it counts.
- **The fare does not depend on your consumption.** `getRouteBaseCost` prices from a reference
  plane (`CalculateFlightCost(von, nach, 800, 800, -1)`), so a thirsty aircraft can never earn its
  fuel back - efficiency is pure margin.
- **The choice is permanent.** `RouteInfo::planeTypeId` is fixed in `addNewRoute()`;
  `condBuyNewPlane`/`actionBuyNewPlane` only ever buy that type for the route and
  `assignPlanesToRoutes()` only assigns matching types. Nothing re-types a route, so one cheap
  decision on day 8 sets the fleet for 50 days.

**The head start itself is real, though:** both arms rented their first route at ~day 8-10 and led
until the crossover - at day 35 they had 9.8-9.9 planes against 7.0 and ~26% more ticket revenue;
the fleet crossover only comes around day 50.

Also note in the capital-ranking patch: `planesToBuy` can be 0 (division by zero -> `inf` wins
every sort; reachable under `ROBOT_USE_FORCEROUTES`, where `mPlanesForRoutesUnassigned` is
non-empty for the first route), and `RouteScore::score` became `DOUBLE` while the log still prints
it through `Insert1000erDots64`, so the tuning quantity shows as "is: 7 $".

#### Next candidates

1. Keep absolute-profit ranking and the truncation, but **filter candidate plane types by
   efficiency** first - e.g. fuel per seat within ~1.5x of the best available type (Il 62 and 720
   out, 767 and A 310 in). That allows a cheap *and* efficient early start.
2. **Allow a route to be re-typed** once a better type is affordable. This is the only option that
   keeps the whole day-35 head start, because it makes an early cheap choice recoverable.
3. Still open from the earlier entry: the weekend/decay-aware image target, the marginal-payback
   rule for airline image, and jobs in the overnight windows of route planes. (The earlier
   suggestion to cut `kPlaneFoodTarget` is withdrawn - see the correction to finding 2 above.)
4. **Kerosene tanks look net-negative on this objective.** The tank program starts ~day 46 (needs
   3 routes) and spends 7.76e7 of `ExpansionTanks` by day 59 - about three 767s' worth, spent in
   exactly the days the fleet grows 17.5 -> 55.6. In scored terms it does not pay either: stock
   bought (`KerosinVorrat`) is 1.074e8 against 9.25e7 of fuel drawn from the tank
   (`KerosinGespart`), and 3.3e7 of stock is still being bought on days 58-59 against 2.6e7
   burned, so part of it is stranded when the game ends. ClaudeBot measured the same thing
   independently (`kUseFuelArbitrage = false`). A/B: no tanks at all, or no stock purchases in the
   last ~2 days.
5. **ClaudeBot re-prices every route every day.** `executeRouteBox()` rebuilds `mRoutes` from
   scratch (ClaudeBot.cpp:1501) with `pricesSet = false`, so `scheduleRouteFlights()` calls
   `setRouteTicketPriceBoth()` daily from that day's kerosene price. Whenever kerosene rose
   overnight that is a raise, and `PLAYER::UpdateTicketpreise` then calls `FlightChanged()` on
   every scheduled leg of the route, re-stamping `HoursBefore` and costing the legs inside 48 hours
   up to half their passengers - the "recompute daily" arm MertenBot measured at -24% on the old
   objective. Port Bot's keep-band (only change when outside [1.60, 1.98] x highCost).
6. Checked and rejected: selling the sponsored starting planes to fund an early 767 -
   `CPlane::CalculatePrice()` divides a sponsored plane's value by 10.

### Same day, later: seeded games for paired measurements (harness change)

`./AT /seed N` now replays exactly the same game for the same N, and `threadpool.rb` plays run j
with `/seed <base + j + 1>` (`--seed-base N` for a different set of games, `--unseeded` for the
old behaviour). `compare_paired.py` (installed next to `concat.py`) compares two measurements game
by game. Uncommitted at the time of writing.

**Nondeterminism found and removed**, each confirmed by diffing identical-seed replicas:
- `Sim.StartTime = time(nullptr)` (Sim.cpp) - most world randomness derives from it. Seeded:
  2026-01-05 12:00 UTC (a Monday) plus N days, so weekday and season still vary between seeds.
- Job/freight pools and the starting jobs seeded from `AtGetTime()` (GameMechanic.cpp,
  Sim.cpp) - now `AtGetSeedTime()`, one seed-derived value for all of them, like the millisecond
  they used to share.
- `BotPlaner`'s `std::mt19937` seeded from `std::random_device` - now from seed, date, game time
  and player. The annealing loop itself is iteration-bounded (`kTempStep = 100`), not time-bounded.
- **The main one:** the legacy CPU players reseed their per-action RNG with `time(nullptr)` in single
  player (Player.cpp, `PLAYER::RobotExecuteAction`) - every sabotage and auction decision depended on
  the wall-clock second. Now from seed, date, game time and player.
- libc `rand()` is used by game logic (e.g. the legacy sabotage mode) and by drawing, once per frame,
  so its stream shifted with frame timing. Reseeded at every simulation step from seed, date and
  game time - not from `TimeSlice`, which keeps counting while the clock stands at 9:00 waiting for
  the idle human to leave the boss office on a painted frame.

Verified: 8 replicas of one seed produce byte-identical traces of every robot action and every
`ChangeMoney` of all four players over 59 days (level 2), 4 replicas identical at levels 4 and 24,
and two full 300-game batches under 24-way load identical in every game both completed.

**A pre-existing harness crash is fixed too:** `SIM::LoadHighscores()` does
`atoll(strtok(nullptr, ";"))` on the shared `xmlmap.fla`, which other games rewrite on day
boundaries - a truncated read segfaults at game start (seen 5 and 4 times per 300; earlier unseeded
batches also lost 3-5 games at times). Highscore load/save is now skipped when `gQuickTestRun > 0`.
(The many SIGSEGV core dumps at game end are a separate, harmless Mesa crash in SDL teardown after
`exit()` on day 59; the CSV is complete by then.)

**What pairing buys, measured:** MertenBot HEAD vs a one-constant change (`kFrequencyRouteStrategy`
1 -> 2), 300 seeded games each: B - A = +9.9e6 (+0.54%), **paired se 8.0e6 (t = +1.23) against
unpaired se 9.34e7 (t = +0.11)** - 11.6x smaller, i.e. ~135x fewer games for the same precision;
correlation across games 0.993. Seeded baseline: mean 1.831e9, median 1.516e9 (not comparable to
unseeded numbers, see below).

**The start weekday is a massive confound:** seeded MertenBot mean by start weekday -
Sun 2.32e9, Sat 2.19e9, Tue 1.91e9, Thu 1.74e9, Fri 1.73e9, Mon 1.51e9, Wed 1.40e9 (43 games
each). An unseeded batch starts every game on the real date it is run, i.e. on one weekday, so
**unseeded measurements taken on different days of the week are not comparable** - a plausible
source of past "run-to-run noise". Why the weekday matters this much is unexplored (both bots
have weekend-dependent logic, e.g. the ad agency closes Sat/Sun) and a promising lead in itself.

## 2026-09-17 — Strengths/weaknesses review of both bots (no code changes, no new batches)

Code read in full for ClaudeBot (unchanged since `abc54748`) and the strategy layer of MertenBot. Numbers are
the 2026-09-16 batches above plus the single ClaudeBot game still in `game/ClaudeBot.csv` (2026-09-15 21:12,
possibly one commit before `abc54748`).

New findings, verified in code:
- **ClaudeBot re-prices every route every day and it is a raise-penalty.** `executeRouteBox()` rebuilds
  `mRoutes` with `pricesSet = false`; `scheduleRouteFlights()` then sets 5.7 x base from today's kerosene.
  `PLAYER::UpdateTicketpreise` (Player.cpp:7092) calls `FlightChanged()` on every future leg whenever the
  new price is higher. Same mechanism MertenBot measured at -19% ("recompute daily").
- **ClaudeBot's night-flight rationale is wrong.** `CalcPassengers` caps at 1.5x cabin *first*
  (Schedule.cpp:321), then applies price (~1/1.9), night (5/6 each) and image (<= 1.27). At our price and max
  image the chain is exactly one cabin, so each night departure/landing costs ~1/6 of the leg's
  passengers while fuel is charged in full. MertenBot flies around the clock too.
- **Starter-plane idle cash.** Single game: 2 planes until day ~23, cash 24.2M on day 22 waiting for the
  25M 767; jobs 0.72M and freight 0.15M by day 22 (route legs leave no >= 5h windows); route ads 12.2M;
  equity emission only 4.1M by day 22 and 23.0M by day 59 - small capital for the takeover exposure.
- **MertenBot `routesFindNextStep()` crew test is inverted** (BotFunctions.cpp:1104, since `045d1925`):
  `haveCrew` is true when crew is *missing*. With money and crew, a route that already has a plane skips
  steps 2-3 and on weekdays buys route ads to 97 (step 4) before planes (step 6). Possibly half-intended
  (the BuyMorePlanes state drives crew hiring), so A/B rather than assume.

Planned next, ranked by likelihood of a significant paired win:
- ClaudeBot: (1) port the [1.60, 1.98] price keep-band; (2) `kEmitStock = false` (keeps 80% own stake,
  measure solo *and* competition); (3) paired re-sweep of 99-day-era constants (`kRoutePairsPerHundredPlanes`,
  `kRankPlanesByCrew`, `kMinRouteValueShare`); (4) night-aware leg placement; (5) jobs-first starter planes;
  (6) last-minute agency.
- MertenBot: (1) fix/A-B the inverted crew test; (2) no kerosene tanks; (3) fuel-per-seat filtered early
  route start; (4) weekend-aware image target; (5) night-aware scheduling; (6) route re-typing.
- Both: explain the start-weekday effect (Sun 2.32e9 vs Wed 1.40e9) before tuning weekday-sensitive rules.

### Same day, later: first ClaudeBot improvement round (seeded, paired)

All numbers: 300 seeded games, mean day-59 `SaldoGesamt` for HA, paired against the previous build
with `compare_paired.py`. One batch takes ~4 minutes.

| commit | change | seed base 0 | seed base 1000 |
|---|---|---|---|
| `b291dc95` (start) | - | 6.36e8 | - |
| `acc9dd19` | keep ticket prices inside [160%, 198%] of the threshold | **1.770e9** (+178%, t=+89, 299/300) | 1.789e9 |
| `b6d6ab0d` | `kMinRouteValueShare` 90 -> 95 | **1.954e9** (+10.4%, t=+22, 280/300) | 1.957e9 (+9.4%, t=+23) |
| `8d5b6547` | hold legs back <= 4h to avoid night departure/landing | **2.024e9** (+3.6%, t=+8, 219/300) | 2.016e9 (+3.0%, t=+6) |

For reference, seeded MertenBot (unchanged since `dc8f04ba`) measured 1.831e9 on seed base 0.

Price band: through day 15 the fleet and flights are identical (75.2 flights, same kerosene) but
passengers are +21%; that compounds into 63 aeroplanes on day 59 against 23.

Rejected (paired against the build current at the time):
- `kEmitStock = false` (takeover defence): **-38.6%**. Setting the dividend without emitting: -38.7%,
  so it is the emission cash itself - a few million in weeks 1-3 bring the first 767 forward.
  Head-to-head with `acc9dd19`: MertenBot still liquidates ClaudeBot in 290/300 (median day 51),
  ClaudeBot wins 81/300 (was ~2). Defence needs a cheaper mechanism (late buy-back).
- `kRoutePairsPerHundredPlanes`: 25 -28%, 32 -17.8%, 44 -1.5%, 50 -3.8% -> 38 stays.
- `kMinRouteValueShare`: 80 -13.7%, 98 -40.9%, 100 -69.7% -> sharp peak at 95 (confirmed on
  seed base 1000). The cliff suggests the gate logic itself is fragile - worth a structural look.
- `kRankPlanesByCrew = false`: identical in 300/300 games (never changes the pick).
- `kImagePaybackDays` 20 / 40: +0.6% / +0.9% (t 1.6 / 2.6) - not taken.
- `kMaxNightShiftHours`: 2 -1.7%, 3 +3.6%, 5 -2.3%, 6 and 8 -11%.

Next candidates: takeover defence by late buy-back to 51% (measure solo and head-to-head);
rework the route gate so it is not a knife edge (value per hour per actual plane, not per the
largest plane); last-minute jobs; jobs-first starter planes; weekday effect.

### Same day, later: early-income round - nothing kept (all paired against `8d5b6547`, 2.024e9)

Early economics, seeded base 0, mean of 300 games:

| | MertenBot (`dc8f04ba`, 1.831e9) | ClaudeBot (`8d5b6547`, 2.024e9) |
|---|---|---|
| income days 1-14 | jobs 12.2M + freight 7.0M, ads 0.9M | tickets 24.9M, ads 10.2M |
| first bought aeroplane (quartiles) | day 15 / 17 / 22 | day 18 / 18 / 19 |
| fleet day 30 / 40 / 59 | 5.4 / 11.8 / 50.7 | 4.0 / 8.4 / 72.1 |
| ads by day 30 | 6.1M | 36.4M (airline image starts at the 3rd aeroplane) |

Rejected:
- **Job planes** (starting aeroplanes chain agency + last-minute jobs with repositioning, first route
  rented at 20M cash, no route ads before a route aeroplane): **-90%**. Jobs earned 7.5M by day 14 vs
  24.9M of tickets; first purchase slipped to day 29-35. Even MertenBot's planner (19M) would only
  match routes net of ads.
- Route image target while fleet <= 2: 0 -50.7%, 40 -49.3%, 60 -18.3%, 100 -8.6%.
  `kRouteImageTarget` 100: -4.9%. `kNoAirlineImageUpToPlanes` 4 -4.2%, 6 -14.0%, 0 identical.
  Early image is essential and already tuned.
- Second bank visit whenever the credit limit is >= 500k: -0.6% (t -2.2). The limit is
  `(Money - Credit)/2 - Credit`, so total credit is capped at half the net cash anyway.
- Two route pairs for the two starting aeroplanes: -50.6%.
- Efficient fallback purchase (buy an affordable type within 125% / 200% fuel per seat of the best one
  while the fleet is < 4 / 8 / always): -32% in every arm. Traced seed 5: A 310 on day 16 and 25 instead
  of 767 on day 19 and 27 - three days ahead, but 250 against 290 seats, and behind from day 34 on.
- `kExpectedPaxPerFlight` 280 / 300: identical (the A 300 is slower and needs 7 crew, the per-crew rank
  still picks the 767). With `kRankPlanesByCrew = false` as well it picks the A 300: -29.7%.

Plane catalogue (price, seats, fuel/seat): 767-300 ER 25M/290/9.7, A 300 28.1M/375/8.0,
A 310 22.5M/250/11.4 (from day 15), MD 81 20M/172/17.4, A 320 12M/149/20.3 (from day 25),
Il 86 17.1M/380/25.8, Il 62 9.9M/198/53.0.

Harness notes: bot log lines go to the game's stdout (`./AT /quick -1 /setbotlevel 4 /seed N | grep`),
not into GameLog.txt via run_test.sh; use `fprintf(stderr)` / `%d` for SLONG in temporary debug prints
(-Werror=format). `threadpool.rb --help` starts a real batch.

Conclusion: the early game is bound by the 25M first aeroplane and the route image it needs; income and
capital levers tested here are exhausted. Next: takeover defence (late buy-back), a less knife-edged
route gate (95 good, 98 -41%), and why the 767 fleet stops at ~72 aeroplanes on day 59.

### Same day, later: MertenBot inverted crew check - not committed

`routesFindNextStep()` step 2 fixed to `haveCrew = (mExtraPilots >= AnzPiloten) && (mExtraBegleiter >= AnzBegleiter)`
so a route aeroplane is bought eagerly when money and crew are there. Paired, 300 seeded games each:

| seed base | reference | fixed | difference |
|---|---|---|---|
| 0 | 1.8308e9 | 1.8532e9 | +1.23% (t +2.26, better in 154/300) |
| 1000 | 1.8300e9 | 1.8233e9 | -0.37% (t -0.69, better in 116/300) |

Not reproducible on the second game set, so not committed (reverted). Consistent with
[at-bot-throughput-knobs-dont-pay]: the fleet is cash-bound, `condBuyNewPlane()` checks crew itself, and
step 3/6 still return BuyMorePlanes, so the fix mostly reorders ads vs. purchase by a planning round.

#### 99-day check of the same fix (seed base 0, `/mpdays 99` appended after `/quick`, no code change)

| day 99, mean of 300 | reference | fixed | paired difference |
|---|---|---|---|
| SaldoGesamt | 1.3487e10 | 1.3615e10 | +0.95% (t +4.9, better in 185/300) |
| Flugzeuge | 122.3 | 122.8 | +0.5 (t +3.0, identical in 63) |
| Geld | 8.163e9 | 8.268e9 | +1.3% (t +4.3) |
| Kredit | 0 | 0 | - |

The CSV column `Available` is always 0, so cash is `Geld`. Over 99 days the fix is a small but clear gain;
over 59 days it is +1.2% / -0.4% on two seed bases. Still not committed. Note that MertenBot ends a 99-day
game with ~8.2e9 idle cash against 122 aeroplanes - the fleet stops being cash-bound late in a long game.

### Same day, later: MertenBot airline image rule ported from ClaudeBot (`Bot: Buy airline image ...`)

Target = min(saturation `1000 - 200 - 4 * lowest route image`, what pays back within `kImagePaybackDays` of
yesterday's `BilanzGestern.Tickets` at ~50,000 per point) + measured daily erosion x days until the ad agency
reopens. Refill only to that target. `kAirlineImageAnyStep`: buy whenever below target instead of only in
`routesFindNextStep()` step 7; step 7 uses the same target so it cannot stall route renting.

Paired against `5fe834ac` (crew fix), 300 seeded games, day 59:

| arm | seed base 0 | seed base 1000 |
|---|---|---|
| payback 0 (old rule) | identical 300/300 | - |
| step 7 only, payback 10 / 20 / 40 | -10.5% (all identical: cap never binds late) | - |
| any step, old target (saturation, refill 1000) | +0.3% (t +0.2) | - |
| any step, payback 3 / 6 | +6.4% / +14.9% | - |
| any step, payback 10 | +18.4% (t +27) | +19.5% (t +28) |
| **any step, payback 20 (committed)** | **+19.2% (t +27), 2.209e9** | **+20.4% (t +28), 2.194e9** |
| any step, payback 40 | +14.3% | - |

The gain needs both parts: buying whenever below target (step 7 is rarely reached) and a target that holds
image back early (payback cap) but keeps a weekend buffer later. Refill-to-1000 at any step is neutral.
MertenBot (2.21e9) is now above ClaudeBot `8d5b6547` (2.02e9) on seed base 0. Not in savegames:
`mTicketsYesterday` and the erosion tracking reset on load (no airline image until the next day starts).

### 2026-09-17: Why MertenBot's score depends on the start weekday (investigation, no code change)

`Sim.StartWeekday` (Sim.cpp:468) is `(tm_wday + 6) % 7`, Mon=0, derived from `StartTime`, so with
`/seed N` the start weekday is exactly `N % 7`. `SIM::NewDay()` recomputes `Weekday` from
`StartTime + Date * 86400` (Sim.cpp:2311) *before* `Date++` (Sim.cpp:2383), so despite the different
convention the sequence is consistent: `Weekday(d) = (StartWeekday + d) % 7`. No bug there.

Measured on the 300 seeded games of `run_measurement_bot.sh`, grouping by `(j + 1) % 7`:

| start | Mo | Tu | We | Th | Fr | Sa | Su |
|---|---|---|---|---|---|---|---|
| day-59 saldo, geo mean | 1.45e9 | 1.90e9 | 1.43e9 | 1.84e9 | 1.87e9 | 2.20e9 | 2.39e9 |

ANOVA on log score: F(6,293) = 3.79, weekday explains 7.2% of the variance. The only contrast that
resolves is **(Sa,Su) vs (Mo-Fr) = 1.36x, t = 4.0**; the ordering inside Mo-Fr is noise (se(log) ~0.1).

Cause: in-game **Saturday** (`Weekday == 5`) closes `ROOM_LAST_MINUTE` (Sim.cpp:1769,
`checkRoomOpen(ACTION_CHECKAGENT1)` BotHelper.cpp:714), and in week 1 MertenBot takes 2.43 of its
2.57 daily flight jobs from that office. Per-weekday means over days 0-6: flights 2.9 on any day,
0.62 on Saturday; flight-job income 0.70e6 vs 0.11e6. Saturday is a ~85% write-off. The travel
agency is open on Saturday but MertenBot books only ~0.14 jobs/day there at any time, so it does not
compensate.

Phase is what the start weekday sets: the first Saturday falls on day `(5 - StartWeekday) % 7`, i.e.
**day 1-5 for a Mon-Fri start** - inside the ramp, which compounds at ~+25%/day on days 3-14 - but on
day 0 (the stub day) for a Sat start and day 6 for a Sun start. After ~day 15 growth is ~+11%/day
regardless of weekday, so the head start is frozen, not amplified: the (Sa,Su)/(Mo-Fr) ratio is
1.29x at day 5, 1.30x at day 15, 1.41x at day 30, 1.36x at day 59.

Control: ClaudeBot `8d5b6547` on the same 300 seeds is flat across weekdays - **1.001x, t = 0.07**,
per-weekday se(log) 0.013-0.031 - because it flies routes from day 0 and takes *zero* last-minute
jobs (4.86 flights/day on every weekday incl. Saturday). Paired, ClaudeBot 2.024e9 vs MertenBot
2.209e9 overall = 1.093x for MertenBot, but the sign flips with the weekday: ClaudeBot is 1.42x
ahead on Mo and We starts and 0.83x behind on Su starts.

Next, if this is worth fixing in MertenBot: stock up on travel-agency jobs on Friday for the
Saturday schedule, or get onto routes earlier. Upper bound ~+25% overall (the 1.36x applies to the
5/7 of games that start Mon-Fri).

### 2026-09-17, later: is the last-minute preference a bug? (three measured arms, all reverted)

Follow-up to the Saturday finding. The preference comes from one line, [BotPlaner.cpp:473](src/BotPlaner.cpp):
`auto minScore = (job.getBisDate() == Sim.Date) ? mMinScoreRatioLastMinute : mMinScoreRatio;` with
`kSchedulingMinScoreRatio = 140000` and `kSchedulingMinScoreRatioLastMinute = 10000` (Bot.h:158-159,
clamped to 5.0 only for BotLevel <= 1). `scoreRatio` is `(premium - kerosene) / flight hours`; in a
free game no bonus factors are set, so it is plain profit per flight hour.

It is backed by real pricing: the game generates last-minute jobs at `flightCost * 130/100 * 5/4`
= 1.625x and travel-agency jobs (`RefillForAusland`) at `* 120/100` = 1.20x, and the agency pool is
AreaType 4 for days 0-9 (no `*3/2` / `*8/5` area bonus) while the last-minute pool draws 4 of 6 slots
from AreaType 1/2. The detour is real - 44-48% of flights are empty legs, longer on average than the
loaded ones - but costs ~17k against ~331k net per loaded leg, ~5% of revenue. The empty leg *is*
priced in the optimizer (adjMatrix edges, `emptyFlight = true`); only the admission filter ignores it.

Three arms, paired against `77db4b91` (`dataREF`, 2.209e9), 300 seeded games, seed base 0:

| arm | day 59 | vs baseline | t | better/worse |
|---|---|---|---|---|
| `getDate() <= Sim.Date` (predicate change) | 0.569e9 | **-74.3%** | -25.7 | 6 / 294 |
| `kSchedulingMinScoreRatio` 140k -> 40k | 1.575e9 | **-28.7%** | -10.6 | 85 / 215 |
| `kSchedulingMinScoreRatio` 140k -> 240k | 1.095e9 | **-50.4%** | -21.9 | 24 / 274 |

**The predicate is not a bug.** `BisDate == Sim.Date` means the job *must* be flown today (node
`latest == today`), so the cheap bar can only ever fill today's gap. `getDate() <= Sim.Date` means it
*may* be flown today, and the optimizer then places those multi-day jobs (agency type B and any
widened window, both carrying the `Praemie * 4/5` cut) on **future** days, pre-committing plane
capacity cheaply. Measured: agency jobs/day 0.38-0.47 -> 1.75-1.90, flights/day 2.65 -> 4.70, but job
income 1.06e6 -> 0.47e6 and last-minute jobs 1.20 -> 0.03 per day. The bot flies nearly twice as much
for less than half the revenue. It did cure the weekday spread - (Sa,Su)/(Mo-Fr) 1.36x -> ~1.16x,
agency uptake flat at 1.75 even on Saturday - but at a catastrophic price.

**140k is a tuned optimum, not a floor**: both 40k and 240k are large regressions. Early planes are
the scarce resource; reserving them for the 1.6x-priced today-jobs beats filling them. The Saturday
write-off is the cost of a policy that is still winning, not a defect - so the ~25% Saturday
opportunity is *not* reachable by touching this filter. If it is reachable at all it has to come
from somewhere else (pre-booking Friday for Saturday without giving up capacity on other days, or
reaching routes earlier).

Also confirmed: under the current rule agency uptake is flat across weekdays (0.35-0.47/day, 0.38 on
Saturday), so the filter binds, not competition from last-minute jobs in the optimizer.

### 2026-09-17, later still: agent-check priority and frequency sweep

Hypothesis (user): since most jobs come from last minute, its `Prio` should outrank the travel
agency's, and the check frequencies should follow. Baseline `77db4b91` (`dataREF`, 2.209e9),
300 seeded games per arm, seed base 0, paired. `kCheck*EveryXMinutes` live in Bot.cpp:34-36, the
priorities in `condCheckLastMinute` / `condCheckTravelAgency` / `condCheckFreight`
(BotConditions.cpp:220/246/280). All arms reverted; nothing committed.

| arm | LM | TA | Fr | prio | delta | t | better/worse |
|---|---|---|---|---|---|---|---|
| prioswap | 60 | 15 | 60 | LM Higher, TA High | -0.92% | -0.85 | 150/150 |
| lm15 | 15 | 15 | 60 | base | +0.75% | +1.07 | 145/155 |
| lm30 | 30 | 15 | 60 | base | -0.15% | -0.40 | 162/138 |
| lm120 | 120 | 15 | 60 | base | -0.19% | -0.48 | 145/155 |
| fr30 | 60 | 15 | 30 | base | -0.16% | -0.45 | 139/161 |
| fr120 | 60 | 15 | 120 | base | +0.61% | +2.13 | 168/132 |
| ta20 | 60 | 20 | 60 | base | +4.47% | +12.21 | 268/32 |
| **ta30** | 60 | **30** | 60 | base | **+4.74%** | **+9.55** | **269/31** |
| ta45 | 60 | 45 | 60 | base | +4.61% | +9.23 | 258/42 |
| ta60 | 60 | 60 | 60 | base | +4.63% | +10.42 | 264/36 |
| ta120 | 60 | 120 | 60 | base | +4.20% | +9.16 | 254/46 |
| ta45_fr120 | 60 | 45 | 120 | base | +4.41% | +10.32 | 264/36 |
| **ta45_fr120_lm15** | **15** | 45 | 120 | base | **+0.20%** | **+0.42** | 158/142 |

**Result: `kCheckTravelAgencyEveryXMinutes` 15 -> 30 is worth +4.7%.** Re-measured on a fresh 300
games (`--seed-base 1000`, own baseline): **+4.52%, t +9.42, 257/300** - not fitted to seed base 0.

**Mechanism.** The decisive arm is `ta45_fr120_lm15`: putting last minute on 15 minutes throws the
whole travel-agency gain away (+4.61% -> +0.20%). So the problem is not *which* counter is polled
often - it is that **any** agent check on a 15-minute timer is permanently due and starves every
lower-priority action. TA@15 was blocking the queue; moving last minute to 15 just re-blocks it.
That also explains the two nulls: `lm15` measured neutral in isolation because the queue was already
saturated by TA@15, and `prioswap` is neutral because ranking two permanently-due actions against
each other unblocks nothing. `Prio` only arbitrates simultaneous readiness.

The response is a step, not a gradient - 20/30/45/60 are all +4.2..4.7% and indistinguishable, only
15 is wrong - and the decay by 120 is mild, so anything in 20..60 works. Freight at 120 (+0.61%,
t +2.13) does not survive once TA is fixed (`ta45_fr120` +4.41% vs `ta45` +4.61%): the same slack
measured twice.

The gain is **not** better job acquisition - `Aufträge` 108.9 -> 107.8, `LastMinute` 54.5 -> 54.4,
job income 88.5e6 -> 87.6e6 at day 59, all flat. It is route throughput on an unchanged fleet:
passengers 351,992 -> 367,844 (+4.5%) with 56.1 -> 56.6 planes and 8.2 -> 8.2 routes, and image at
day 20 42.4 -> 45.3. The freed action slots go into running the routes, not into buying more jobs.

Next: commit `kCheckTravelAgencyEveryXMinutes = 30` if wanted (one constant). Then look for other
actions whose condition is near-permanently satisfied - the same starvation pattern may exist
elsewhere in the prio queue.

## 2026-09-18 - ClaudeBot: buy aeroplanes one per call, not in batches of ten

**Bug (spotted by the user).** `executeBuyPlane()` sized the purchase at the bodyguard-discounted
price, then called `GameMechanic::buyPlane()` in batches of up to ten. `buyPlane()` checks
`Money - Preis * amount >= DEBT_LIMIT` at *list* price and only refunds the discount afterwards. So
when ten fit the discounted budget but only nine fit at list price, the batch of ten was refused and
the loop broke off - **zero** aeroplanes that visit, not nine. Fix: one aeroplane per call, each step
guarded by `Money - Preis >= DEBT_LIMIT + kPlaneCashReserve`, so every refund lands before the next
charge and the reserve holds at list price. Per-call side effects are harmless (a random name picked
from the unused ones, MapWorkers, advisor update).

**Result:** paired on seed base 0, HEAD ae3f5614 vs fix: 2.0242e9 -> 2.1531e9, **+6.36%, t +30.33,
better in 299/300**. Committed.

Next: the same list-vs-discount mismatch may exist anywhere else ClaudeBot budgets with a discount
it only receives after paying.

## 2026-09-18 - Hurricane (BotLevel 7): first implementation of sabotage

New play style "Hurricane" = Tycoon economy plus sabotage, gated on `BotLevel == BotDifficultyHurricane`,
so Tycoon (level 6) is unchanged (smoke test: no Hurricane code runs).

- Trust: buy ITEM_MG at duty free, hand it over at the Arab Air counter (the game grants the trust
  in ROOM_ARAB_AIR, not in the saboteur's room).
- Victim: a human if there is one, else the competitor with the highest share price.
- Hints: own count (ArabHints may not be read), +job hints on order, -3 per day, ceiling 99 so the
  boss never exposes us. Cash reserve 1.5M after paying.
- Job order: route theft at trust 6 (saves hints for it), else the trust-raising job, else damage
  (strike, press release if the victim has routes, engine breakdown, ...).
- Route theft steals the victim route with the best value per hour among routes our scheduler can
  fly, cached at the route box.
- Pliers: after a job is refused by security, pick up ITEM_ZANGE and knock out the security office.
- Savegame version 108.

Not measured yet: no script runs level 7. Next: a level-7 smoke test, then check that sabotage
does not cost the economy too much (paired measurement against Tycoon).

## 2026-09-19 - Hurricane: first measurements and fixes

Paired against Tycoon (level 6), seed base 0, 300 games. Tycoon reference 2.1409e9.

| Hurricane version | vs Tycoon | t | worse in |
|---|---|---|---|
| first version (orders whatever fits under the hint ceiling) | not measured | | |
| save hints for the best affordable job, virus weekly | -29.40% | -57.7 | 299/300 |
| + no job above 5% of cash | -4.26% | -11.8 | 261/300 |
| + no walk to the saboteur while saving | -4.58% | -12.7 | 270/300 (neutral, kept) |
| + no job above 1% of cash | **-1.85%** | -6.5 | 224/300 |

- The -29% came from paying 5M for "ground aeroplane" (trust 5 -> 6) on day 9: it delayed the next
  25M aeroplane by a few days and the gap opened between days 15 and 20. Sabotage itself cost only
  5.3M. Early cash compounds; the cap on the share of cash fixes it.
- The first version kept its hint count pinned at 90-99 with flat tires and never had room for a
  strike; saving for the first affordable job fixes that (strike every ~13 days).
- At 1% what remains is mostly the 80,000 for ITEM_MG on day 1.
- With 1%, full trust needs ~500M cash, so route theft does not happen within 59 days. In the test
  setup the victim is the idle human, so none of the damage shows up in any number.

Open design question for the user: how much of its own score may Hurricane give up for earlier
and stronger sabotage (early sabotage hurts a human most).

## 2026-09-19 - Release check: Tycoon after the 1.9.1 fixes

Paired against `tycoon_ref` (Tycoon, seed base 0, 300 games, taken before 79dc91e3), HEAD dfadf990:
+0.01% (t +1.0), 2.1411e9 vs 2.1409e9. 199 of 300 games identical, the other 101 within
-0.03%..+0.2%, except game 111 at +3.1%, where the run went a different way after a small change.

- Food for first-class passengers (2d6ccdfa) costs Tycoon about 1,700 over 59 days (Essen
  -19.93M either way): it carries almost no first-class passengers.
- The ClaudeBot edits in 1b589497 only reformat code and rename Hurricane's level constant.

No regression. `tycoon_ref` stays a valid reference.

## 2026-09-19 - Release tests and fixes (branch dev)

Tycoon on the final build (2e30dc28), paired against `tycoon_ref`: +0.01% (t +1.0), identical game
by game to the run before the fixes - the classic bots' share purchase fix never triggers in a
59-day free game. 0 core dumps in 300 games (the exit crash fix; each batch left 15-50 before).

Tested: all 26 missions x bot levels 000/111/222/333/444/555 (scripts/run_missions.sh), savegame
formats 202/203/204 (scripts/run_loadtest.sh), five 3-day network games (harness) with Saboteur,
Nemesis, Tycoon and Hurricane, strikes and room actions. Fixed: harness exit crash, /load, classic
bot share purchases, stock dry-run logging, network version for preview peers, route usage /
route image / airline image syncs racing flights booked in the same minute.

## 2026-09-20 - Mission sweep with ClaudeBot (level 666)

All 26 missions x 7 bot levels (classic, the five MertenBot levels, Tycoon), seed 1,
`scripts/run_missions.sh 000 111 222 333 444 555 666`. Missions won, of 26:

| level | won | median day | games with no result |
|---|---|---|---|
| classic | 19 | 53 | 6 |
| LaidBack | 21 | 45 | 5 |
| Challenger | 22 | 34 | 4 |
| Saboteur | 22 | 33 | 4 |
| FreightBaron | **25** | 35 | 1 |
| Nemesis | 24 | 34 | 2 |
| Tycoon (ClaudeBot) | **12** | 184 | 13 |

MertenBot wins clearly, as expected. ClaudeBot has no mission logic, and half its games never
end: it plays on for hundreds or thousands of days without reaching the goal or going bankrupt.

Found and fixed (ClaudeBot.cpp):
- **Crash, mission 11.** ClaudeBot planned ACTION_CHECKAGENT3 because
  `Helper::checkRoomOpen()` only tests the mission number, but the freight depot is gated by
  `ROBOT_USE_FRACHT` (off for missions 0-5 and 11) and in Sanierung the hall is not in the
  airport at all. The walk target lookup then throws `ExcNever` out of
  `GetRandomTypedRune(RUNE_2SHOP, ROOM_FRACHT)` and aborts the process (SIGABRT, day 0).
  MertenBot is safe because `condCheckFreight()` checks the flag.
  `canUseAction()` now checks `ROBOT_USE_FRACHT` and, generally, `DoesRuneExist(RUNE_2SHOP,
  room)` - the same guard the classic bot uses for NASA and the telescope. The latent case was
  ACTION_VISITTELESCOPE, which maps to ROOM_RUSHMORE outside two missions.
- **`takeOutCredit(): Invalid amount`**, 11 times. `executeBank()` borrowed
  `CalcCreditLimit()` whenever it was `> 0`, but the game refuses anything below 1000.

Both are no-ops in the free game (freight is enabled, every room exists, the limit is never a
handful of dollars), and a smoke test confirms all 16 actions still execute. Mission 11 now
runs past day 327 with no exception.

Not fixed, reported only:
- **ClaudeBot cannot bootstrap without routes.** `collectGaps()` only yields a window bounded
  by a *following* flight, so a plane with an empty flight plan has no window and no job can
  ever be placed. In mission 0 (tutorial, 10 orders, the easiest mission in the game) the route
  box is closed, so ClaudeBot took 0 jobs in 76,590 agency visits over 2553 days and idled at
  the kiosk. The same hole would stop it recovering in a free game that lost all its routes.
- **Crew shortage churns the scheduler.** ~180,000 `_planFlightJob(): does not have enough crew
  members` across 10 games: HR hiring lags fleet growth (e.g. 194 attendants against 200
  needed), and the scheduler keeps offering jobs to under-crewed planes instead of skipping
  them. Starts as early as day 89 with a 50-plane fleet.
- ClaudeBot never visits the Last Minute counter (no ACTION_CHECKAGENT1 anywhere).

MertenBot warnings seen, for the record: `actionSabotage(): Cannot determine sabotage mode`
(9x, levels 333/555), `checkPlaneLists(): We lost the plane with ID` (mission 50, all levels),
`condAll(): Default case should not be reached` (1x, level 111), `_planFlightJob(): Invalid day
(too early, 174)` with `planRouteJob returned error` (level 222, missions 44/46, both past day
150). Classic bot only: `buyStock(): Player cannot afford` (30x) and `Tried to book flight
twice` (11x, mission 3).

Next: decide whether ClaudeBot should get a route-free bootstrap (open-tail window with a
forced return leg) - it is the one change that would also protect the free game.

## 2026-09-20 - ClaudeBot learns to play missions (first cut)

Two parts, as asked: stop missions breaking the bot, then aim it at the goal. Everything goes
through one `ClaudeBot::Mission` struct read from `Sim.Difficulty` in `startNewDay()`; every
flag in it is false in the free game, so the free game runs through the same code as before.
Which of the two sources a flag comes from follows what the game keys on: room availability is
a `PLAYER::RobotUse()` feature, because that is the table the airport is built from, and a win
condition has no feature flag at all, so those are read off `Sim.Difficulty` like `HasWon()`.

### Part 1: robustness

- `canUseAction()` refuses any action whose room has no `RUNE_2SHOP` entrance, and honours
  `ROBOT_USE_FRACHT`. This was the SIGABRT in mission 11 (committed separately) and would have
  hit `ACTION_VISITTELESCOPE` -> `ROOM_RUSHMORE` next.
- No loans below 1000, which `takeOutCredit()` refuses.
- **The route-free deadlock.** `collectGaps()` only ever returned a window bounded by a
  *following* flight, so with no route box the flight plans stayed empty, no window existed and
  no job could ever be accepted: 76,590 agency visits over 2553 days of the tutorial, 0 jobs.
  It now opens a tail window in those missions. Two things had to follow:
  - The tail is not charged the empty return, because the game only inserts one *between* two
    planned flights (Planetyp.cpp:700-760), and the plane stays where the job left it, so the
    window's city moves with it.
  - A plane may then depart from anywhere: the game repositions it before the first planned
    flight, so the leg is paid for and waited out instead of refused. Without that the airline
    seized up after one job each - every offer departs from the home airport, and a plane that
    had flown once was stranded for good. That one change took the tutorial from day 73 to day 2.

### Part 2: goals

`wantDebtFree` (ADDON01) repays instead of borrowing; `wantImage` (HARD) buys past the payback
saturation; `conditionPlanes` (ADDON07, ATFS02) and `wantUpgrades`/`upgradePlanes` (ADDON05,
ATFS02) fit out only as many planes as the goal names; `wantFreight`/`wantFreeFreight`
(ADDON02, ADDON03) rank contracts by tonnage, and ADDON03 now takes exactly the `Praemie == 0`
contracts the old `Praemie <= 0` filter threw away; `wantMissionCities` (NORMAL) outranks
everything at the route box.

One measured correction: repairing the whole fleet to 100 bankrupted all three ClaudeBots by
day 7 of ADDON07. A mission fleet starts at Zustand 35 with a million in the bank, and
`Improvement * ptPreis / 110` was 3.7M and 6.3M for two planes in a single night. The repair
target is now only raised while the last points are cheap (`WorstZustand + 20 >= 90`), and the
cheap way to own a plane at 90 is to buy one - they are delivered at 100.

### Result

26 missions at seed 1, `scripts/run_missions.sh 666`: **17 won, 0 crashes, 0 non-zero exits**,
against 12 won and one SIGABRT before. Missions that flipped: 0 (never -> day 2), 1 (never ->
day 6), 4 (123 -> 63), 11 (crash -> 46), 12 (never -> 350), 18, 41, 42 (never -> 71), 43, 44,
46, 47, 49.

Free game unchanged, checked paired on six seeds: five byte-identical to the pre-change build.
Seed 6 differed, but it is **not deterministic on its own** - two runs of the identical old
build gave 85,573,300 and 28,375,854, the first matching the new build exactly. Worth knowing
for the harness: a seed fixes the game, not the run, so `compare_paired.py` reduces the noise
rather than removing it.

### Still unwon, and why

- **45 (ATFS05) and 48 (ATFS08) are unreachable**: both count only `Planes[d].TypeId == -1`,
  a self-designed plane, and ClaudeBot never enters the designer.
- **5 and 20** need the NASA room, which ClaudeBot never enters either.
- **17 (ADDON07)** no longer goes bankrupt but stays poor: a 2.9M budget against a 9.9M
  cheapest airframe, so it never buys the two planes the goal wants.
- **3 (NORMAL)** still not won despite the mission-city bonus - the pairs are rented, so the
  utilisation or the ten-flag count is what is missing.
- **2, 19** and **16** (where the idle human wins on company value) are ordinary economy.

Next: the aeroplane designer, which closes two missions outright, then mission 17's capital
problem.

2026-09-20 - the aeroplane designer
-----------------------------------

**BotDesigner is 5.7x faster, same output.** Profiled with `eu-stack` sampling (no `perf` on this
box): 67% of the runtime sat in `CXPlane::operator=`, which serialises the whole plane through a
30KB `TEAKFILE` and re-runs seven `Calc*()` just to write the file header - once per element move
of `std::sort`, for every buildable plane, for each of six score types. Gating the best-list on
the score, collecting the stats once per plane instead of once per plane and goal, dropping a dead
`bprintf` name per part per attempt, and hoisting a string hash out of the relation x part loop
took 54.9s to 9.5s with all 30 designs bit-identical.

**ClaudeBot designs its own planes rather than reusing the hardcoded ones.** The part economics
come from `builds.csv`, which differs between data dirs and moves the optimum, so a hardcoded
choice of parts is valid everywhere but cheapest only where it was found - and price is what
decides these races. New `ScoreType::ClaudeMiss05/08` and `findBestPlaneForGoal()`. A hull prune
for passenger-floor goals cuts ATFS05 from 8.8s to 2.77s for an identical design.

Scoring findings, all measured: a speed term bought a 65.0M plane over the 60.3M one, because
`Schedule.cpp` caps route passengers at the player's share of `qRoute.Bedarf`; the price exponent
barely moves the pick (1.5 to 3.0 went 60.3M to 59.3M) while the **range cap does the work**
(11200 -> 6000 km gives 55.7M). Result: ATFS05 55.7M vs the 60.3M reference, ATFS08 13.6M vs 17.2M.

**Buying implemented.** ATFS05 went 0 -> 2 of 3 Belugas, ATFS08 0 -> 1 of 5. Cash discipline was
most of it: with advertising left on ATFS05 bought one instead of two and ATFS08 none instead of
one; letting the fleet grow to five bought none. An unguarded ATFS08 run spent 30.4M on ads by
day 34 for a 13.6M plane.

Next: neither mission is won yet, and the gap is **not** the designer - on the same two planes
ClaudeBot banks 14.3M by day 20 where MertenBot banks 47.7M. The earning rate in missions is the
thing to attack, and it is the same lever as the free-game score.

2026-09-20 (later) - winning missions, not the free game
---------------------------------------------------------

**The free game is already won.** At `/setbotlevel 6` against two legacy bots and the idle human,
ClaudeBot ends day 59 with a company value of 1.71e9 against their ~53M, and has the highest
company value in **80/80** games. Day-59 SaldoGesamt is 2.14e9. The place ClaudeBot loses is the
missions: **8 of 26 won**, 13 lost, 5 that nobody ever wins so they run to the harness timeout.

**Route ticket price is already at its optimum.** `kTicketPriceThresholdPercent` 190 was worth
testing because pricing above the threshold costs 2 image points per flight and image scales
route passengers hard (playerImage -247 gives a 0.364 multiplier against 0.545 at 0). But both
`CalcPassengers` and `BookFlight` use the same threshold, and the passenger count is *also* capped
by seats - so once the plane fills, revenue is linear in price and the high price wins outright.
Measured day-59 SaldoGesamt: 95% -> 0.33e9, **190% -> 2.14e9**, 250% -> 0.94e9. Left alone.

**Missions need a different game, not a better one.** Routes are an investment that a three-week
mission never pays back, and each route flight consumes an idle window the goal needs. New
`Mission::noRoutes` (EASY, ADDON01, ADDON02, ADDON03) routed through `routesAvailable()`, which
had been dead code. The trap: `collectGaps()` only reports a window bounded by a *following*
flight, with an open-tail fallback gated on `noRouteBox`; turning routes off without widening that
gate left every plane with zero windows and took ADDON02 from 15 tons to 0. ADDON03 is now a win,
ADDON02 went 15 -> 550 tons of 1000, ADDON01 finishes day 29 instead of 46. Free game byte-identical.

Next, in order of what the evidence shows:
1. **Throughput per plane, probably not used planes.** In EASY ClaudeBot sits on 6.8M with two
   planes and buys nothing. That part is explained: the cheapest new type in planetyp.csv is the
   Il 62 at **9,900,000**, and *no* type costs under 9.9M, so 6.8M buys nothing. ClaudeBot also has
   no museum path at all (`ACTION_BUYUSEDPLANE` appears zero times), which looked like the fix -
   but MertenBot gates used planes off under `mLongTermStrategy` and still wins these missions, so
   a third plane is probably not what decides EASY. The legacy bot's three planes are not evidence
   either: it cheats. The likelier lever is earning more with the two planes - in ATFS05 MertenBot
   banks 3.3x more than ClaudeBot on an identical fleet. **Unverified**: the MertenBot-vs-ClaudeBot
   EASY comparison could not be run (see below).
2. **International branch offices.** No `bidOnCity`, no `ACTION_CALL_INTERNATIONAL`. In ATFS05
   MertenBot holds 32 offices and earns 41.6M from freight by day 20; ClaudeBot holds 1 and earns 0.
3. ATFS05/ATFS08 still need a faster ramp - see the designer entry above; the designs are not the
   limit, the earning rate is.

**Harness note (2026-09-20 evening): the game needs a real display.** With the monitor switched
off, every run dies at `TeakLibW/Bitmap.cpp:22 CreatePrimarySurface failed`, then *hangs* instead
of exiting, so `timeout` kills it with exit code 124 and the mission table reads "no result". Any
measurement taken in that state is void. Do not reach for `SDL_VIDEODRIVER` - see the rule in
CLAUDE.md and the GPU-load incident behind it.

2026-09-20 (evening) - last minute jobs
----------------------------------------

**ClaudeBot had never entered the last minute counter.** `ACTION_CHECKAGENT1` appeared nowhere in
ClaudeBot.cpp, so its only passenger-job source was the travel agency, whose jobs all start at the
home airport. Last minute jobs run between any two cities and pay far more. The symptom was
visible on EASY: same seed, same two planes, neither bot buying used planes, ClaudeBot flew 33
flights and 49 jobs for a saldo of 604,185 while MertenBot flew 20 and 20 for 2,572,713 - its jobs
averaged 229k against ClaudeBot's 77k.

`takeJobsFromBoard()` now holds the greedy pass and both counters call it; they differ only in the
board and the GameMechanic call, whose signatures are identical. Last minute is planned ahead of
the agency (it shuts earlier and pays more).

  free game day-59 SaldoGesamt  2,141,078,215 -> 2,149,433,048
  mission wins                  9 -> 10 (ATFS09 L -> W)
  EASY ratio 464 -> 326, ATFS01 900 -> 509

**Two corrections to the previous entry.** Used planes are *not* the lever: MertenBot buys none
(gated off under `mLongTermStrategy`) and still wins these missions on two planes. And the free
game's job profit floor of 1,000 was badly wrong for missions - `kMissionJobGain` now uses 50,000
where no routes are flown. Swept on EASY, day-9 saldo: 1,000 -> 604,185 with 49 jobs; 50,000 ->
976,665 with 18; 150,000 -> 765,105 with 6; 400,000 -> the bot stops flying and ends at -111,104.

Still open, in order:
1. EASY is still lost at ratio 326, and ATFS01 at 509 - both are "first to a cash/profit figure",
   so the early ramp is still the weak point.
2. **International branch offices**: no `bidOnCity`, no `ACTION_CALL_INTERNATIONAL`. In ATFS05
   MertenBot holds 32 offices and earns 41.6M from freight by day 20; ClaudeBot holds 1 and earns 0.
3. ATFS05/ATFS08 designs are fine; the ramp is what is missing (see the designer entry).
4. ATFS09's harness ratio (508) disagrees with its win flag (ClaudeBot won). Unexplained; the win
   flag is what `PLAYER::HasWon()` reports, the ratio is a separate STAT_MISSIONSZIEL sample.

2026-09-20 (night) - NASA, Uhrig, the routes mission, and a batch cut-off
--------------------------------------------------------------------------

State at the end of the session: **12 of 26 missions won** at `/setbotlevel 006` (ClaudeBot as HA
against two legacy bots and the idle human), and **every mission now terminates** - there is no
"no result" row left anywhere.

  won      TUTORIAL FINAL ADDON01 ADDON03 ADDON05 ADDON08 ADDON10
           ATFS02 ATFS03 ATFS07 ATFS09 ATFS10
  not won  FIRST 154, EASY 326, NORMAL 100(cutoff), HARD 1244, ADDON02 122,
           ADDON04 305, ADDON06 104, ADDON07 119, ADDON09 1150, ATFS01 509,
           ATFS04 403, ATFS05 152, ATFS06 800, ATFS08 500      (ratio, lower is better)

Free game is unaffected by all of it: day-59 SaldoGesamt stayed at 2,149,433,048, and ClaudeBot
already has the highest company value in 80/80 free games.

### NASA closes two missions outright

FINAL and ADDON10 are won by buying ten parts, and ClaudeBot had never entered the room, so
neither could ever end. Parts must be bought **in order**, so only the first one not yet owned is
ever on offer (`Player.cpp`, ACTION_VISITNASA is the reference). Ten parts cost 204M (rocket,
`RocketPrices`) and 238M (station, `StationPrices`), which is why these two **keep their routes**
where the tonnage missions do not - that bill needs the whole economy.

  FINAL    never ended -> won on day 102
  ADDON10  never ended -> won on day  97

### ADDON09 may simply not be winnable on two planes

Not "cannot reach 200": ClaudeBot was **bankrupt by day 10**, pinned at -10,000,000, having flown
1 of 200. Fines went -818k on day 5 to **-11,667,894 on day 7**. Three things compound, all in
`CAuftrag::RefillForUhrig`:

- `Personen = 180` on every job, so it needs a large aeroplane.
- `Strafe == Praemie` (type C, 15% of them: premium doubled *and* fine twice that, due next day).
  `killFlightJob` pays the same fine, so there is no way to duck it - the only lever is flying.
- `AreaType` per day is `[0,0,1,1,2]`, and case 0 draws **both** endpoints from a random region
  with neither forced home: 2 of 5 jobs a day need empty legs at both ends. (Merten's own
  MertenBot analysis reached the same conclusion independently.)

The legacy bots cheat here and it is worth knowing how: with `ROBOT_UHRIG_FLIGHTS_AUTO` they never
receive Uhrig jobs at all (`Add5UhrigFlights()` is skipped in Aufsicht.cpp) and instead **every
ordinary job they fly** counts toward the goal, capped near 5/day. SuperBots are explicitly
excluded (`Player.cpp:8450`), so MertenBot and ClaudeBot must fly the real thing.

Two changes made, both right, both measured neutral: `Mission::noRoutes` for ADDON09 (4 jobs flown
instead of 1) and a **fix to how a job in the backlog is valued**. `fitJobIntoGap()` used
`Praemie - cost`, which is right when deciding whether to *take* a job off a board - the
alternative is not having it. For a job already held the alternative is the fine, so it is worth
`Praemie + Strafe - cost`; `schedulePendingJobs()` now passes `alreadyOurs`.

### NORMAL (mission 3): ClaudeBot is not stuck, its *rating* is

**Resolved** (see the mission-city scheduling bonus below, 2026-09-20). The three-part fix this
section asks for was never needed and was never built. Re-checked 2026-09-22 at `8826dbf8`: won on
20/20 seeds, day 42-58 (median 47), and at the other bot levels too - Hurricane 4/4 (day 46-53),
ClaudeBot on all three computer airlines 4/4 (day 60-64). MertenBot's median is day 75.

It reaches **310 billion and 200 planes by day 1000** while the flag count sits at 6 of 10 from
day 50 for ever. `NumMissionRoutes` (`Aufsicht.cpp:101-125`) counts a rented pair only when one end
is home, the other is a `Sim.MissionCities` entry, **and `RoutenAuslastung > 20`** - more than 20%
of that pair's weekly demand. Pairs count in both directions, so `TARGET_FLAGS = 10` is five
cities. Holding ~96 pairs spreads 200 aeroplanes so thin that only three clear 20%. **More flying
makes this worse.** It is a concentration problem, not a capacity one.

Tried and **reverted**: capping the mission to 8 pairs. The game did start terminating (day 135),
but the fleet still grew to 179 aeroplanes with only 8 pairs to fly, and it went **bankrupt by day
100**. The log also shows it renting pairs that can never score a flag (Johannesburg-Sydney,
New York-Tokyo - neither touches home). A real fix needs three parts together: cap the pairs, cap
the **fleet** alongside them, and refuse pairs that cannot earn a flag.

### Route ticket price is at its optimum - do not touch it again

Pricing above the threshold costs 2 image points per flight and image scales route passengers hard
(playerImage -247 gives a 0.364 multiplier against 0.545 at 0), so 190% looked wrong. It is not:
`CalcPassengers` caps passengers by **seats** as well, so once the plane fills, revenue is linear
in price and the high price wins. Day-59 SaldoGesamt: 95% -> 0.33e9, **190% -> 2.14e9**, 250% ->
0.94e9. Also note the free game's image is healthy (541); the -247 was mission-specific.

### Harness: the batch cut-off (Merten's commit a05afa56)

Runs now stop when every `Owner == 1` player is out, or at `gAutoQuitOnDay = 500` for batch mode,
and the cut-off calls `printPostGameInfo()` so a stopped run emits a real `BotMission:` row instead
of "no result" - a mission nobody can win is now visibly different from a run that died. Two traps
found while reviewing it, both fixed before it landed:

- The alive-check had `anyBotAlive = false` on the branch that proves a bot **is** alive, so
  `giveUp` was true whenever any CPU slot existed and **every** batch run quit on day 0. It fails
  silently: the scoreboard just prints `0` / `no result`, which looks exactly like the
  display-off failure. Only a mission that *should* take 60 days tells them apart.
- Without the `gQuickTestRun > 0` gate, a multiplayer table with `BOTS=0` has no `Owner == 1`
  player at all and would quit on day 0.

Still cosmetic: `printf("Triggering cutoff: ...")` has no `\n`, so it runs into the first
`BotStatistics2` header line.

### Next, in order of evidence

1. **International branch offices** - still the largest missing subsystem. No `bidOnCity`, no
   `ACTION_CALL_INTERNATIONAL`. In ATFS05 MertenBot holds 32 offices and earns 41.6M from freight
   by day 20; ClaudeBot holds 1 and earns 0.
2. **ADDON06 (104), ADDON07 (119), ADDON02 (122), ATFS05 (152)** are the closest losses and the
   best return per unit of work.
3. ~~**NORMAL** needs the three-part fix above.~~ Resolved by the mission-city scheduling bonus.
4. **ADDON04** (most miles in 30 days) is untouched - miles are not modelled at all.
5. ATFS09's harness ratio still disagrees with its win flag; unexplained, the win flag is what
   `PLAYER::HasWon()` reports.

2026-09-20 - ADDON09: measured verdict and what would fix it
------------------------------------------------------------

Assessment session, no ClaudeBot change. All game-source edits below were made, measured and
reverted; the tree is unchanged. Raw data and the feasibility model are gone with the scratchpad,
the numbers are here.

### The Uhrig stream is the same in every game

`Sim.cpp:827` reads `if (GlobalUse(USE_TRAVELHOLDING) && Difficulty != DIFF_ADDON09)`, and the
`Auftraege.Random.SRand(AtGetSeedTime())` call sits **inside** it. So in this mission no player's
job generator is ever seeded: all four airlines get the identical stream and the stream is byte
for byte the same for every `/seed`. Verified across 12 seeds - day 1 is TXL-BCN, TXL-MUC,
ARN-WAW, HEL-FBU in all of them, for SA and HA alike.

That makes the mission scripted rather than random, and one particular script entry decides it.

### Why it is not winnable as shipped

Replayed the generator for 60 days (300 jobs) on a copy of the RNG and measured it:

- Supply is **5 jobs/day**, so 200 needs 40 perfect days. The legacy bots are credited for *any*
  job they fly, capped at `5 - [(D+PN)%5==1] - [(D+PN)%11==2] - [(D+PN)%7==0] - (D+PN)%2`,
  mean **4.07/day**, and they never receive Uhrig jobs at all (`Aufsicht.cpp:356`) nor pay fines
  (`ROBOT_USE_NO_FINE`). A legacy bot wins on ~day 50-55 when its economy holds.
- **59% of jobs touch neither end of the home airport**; only 1.00 job/day *departs* home. Median
  home->pickup is 1,323 km, mean 3,418 km, and 28% of pickups are beyond one hop of the best
  starting plane. Uhrig is the only job generator in the game that does not force an endpoint
  home - `RefillForAusland`, which feeds the travel agency, always does.
- **74% of jobs have a one-day window.**
- Premium is 115% of reference *kerosene* for the revenue leg only (`CalculateFlightCost(.., 8000,
  700, -1)`), so the positioning leg is unpaid. Flying all 300 jobs earns **39.5M** gross over 60
  days; a perfect-foresight scheduler burns **37-57M** of fuel doing it, before crew, food, gate
  fees and maintenance. Only 33% of jobs cover their own fuel including one positioning leg.
- Per area type (mean margin per job, fuel only): AreaType 1 **+15,893**, AreaType 2 **-99,754**,
  AreaType 0 **-164,429**. `Add5UhrigFlights` uses `[0,0,1,1,2]`, so the two worst are half the mix.
- The scripted killer: **MNL->SYD, issued the morning of day 4, due day 5 only, 2 passengers,
  fine 8.33M** (9.06M in the live game, kerosene price differs). Manila is 9,990 km from home,
  past the 6,455 km range of the best starting plane. You hold 3,000,000 and `DEBT_GAMEOVER` is
  -5,000,000. It is type C (`Praemie *= 2; Strafe = Praemie * 2`) crossed with the 1% 2-passenger
  case (`Praemie *= 4; Strafe = Praemie * 4`), which compounds to **32x** the base premium.

A scheduler with perfect foresight of the whole stream, no crew/food/gate/maintenance costs and
no walking time still goes bankrupt on **day 6** with the two starting planes. ClaudeBot dies on
day 7-8 in every seed at 8-18 of 200. With three legacy bots and no ClaudeBot, only 1 of 6 games
produced a winner (day 55); the rest ran 107-130 days with the best bot at 38-61%.

### What fixes it (measured, 3 seeds each, ClaudeBot untouched)

| variant | change | HA wins |
|---|---|---|
| baseline | - | 0/3, bankrupt day 7 |
| **A** | `Add5UhrigFlights`: area types `[1,1,1,1,2]` instead of `[0,0,1,1,2]` | **2/3** (day 83, 78) |
| B | `RefillForUhrig`: `if (Strafe > Praemie) Strafe = Praemie;` | 1/3 (day 90) |
| A+B | both | **2/3** (day 80, 53) |
| A+B+C | plus `TARGET_NUM_UHRIG` 200 -> 150 | 2/3 (day 71, 40) |

**A is the fix**: it is one line and it is the change that brings Uhrig in line with every other
generator in the game. **B is a survival fix**: it removes the scripted day-5 knockout (seed 3
went from -10M to +7.6M solvent) but adds no throughput on its own. **C mostly just shortens the
game** - the opponents reach the lower target just as fast, so it did not change who won.

The seed-3 loss survives A+B, but there ClaudeBot is solvent at 29% while the legacy bots finish
on day 54. That is a bot-tuning problem now, not a mission-design one.

Worth fixing regardless of difficulty: the missing `SRand` (above), and `CalcPlayerMaximums`
feeding `RefillForUhrig` the fleet **maximum** - buying one big plane pushes 58% of jobs to 280+
passengers and makes 22% of one-day jobs physically impossible, so expansion makes the mission
harder. `PlayerMinPassagiere`/`PlayerMinLength` are already computed next door.

2026-09-21 - ADDON09 won: a dedicated Uhrig scheduler
-----------------------------------------------------

Merten's commit 5a0aa18a made the Uhrig jobs flyable: every job now uses AreaType 4 (home
region, one end at home, under 10,000 km), the premium went from 115% to 120% of the reference
kerosene, and the base fine from `Praemie` to `Praemie / 2`.

**Before any ClaudeBot change** (6 seeds, `/setbotlevel 006`): ClaudeBot won 4/6 on days 57-81 and
lost seeds 3 and 5 to a legacy bot finishing on day 53-54. It flew under one Uhrig job a day and
stayed on two planes until day 45 while running up 21-29M of fines. The cause was the scheduler:
`schedulePendingJobs()` only appends at the open tail of a plan, by deadline, and never repositions
into a bounded window. A job for the day after tomorrow sat at the tail, so the next morning's
jobs for tomorrow had nowhere to go. The agency and freight jobs it took also competed for the same
tail.

**Change** (`Mission::uhrigJobs`, ADDON09 only):
- `scheduleUhrigJobs()` replaces `schedulePendingJobs()` in the office: it clears every unlocked
  plan entry (`clearFlightPlan`), then runs 300 randomised greedy passes that chain all held jobs
  plane by plane in time order, repositioning legs included, and commits the best one. The score
  is 1M per Uhrig job + premium + fine avoided - kerosene. The timing follows `CheckFlugplaene`:
  an automatic leg departs at the previous landing, the next flight leaves at landing + 1, and one
  extra hour of slack follows an empty leg. After committing it checks that no job was shifted out
  of its window; this never triggered.
- The travel agency, last minute and freight are not visited in ADDON09.

**Result**: won **54/54** games on **day 41-42**. That's 24 seeds against the legacy bots, plus 6 each at
`016`, `026`, `056`, `556` against MertenBot. Zero fines, zero jobs left over, cash positive
throughout, and the fleet grows to 4 planes from job income. 40 days is the floor (5 jobs a day),
so there is nothing left to gain here. The job stream is still unseeded in this mission
(`Sim.cpp:827`), which is why every seed ends on the same day.

The free game is untouched: every change is behind `mMission.uhrigJobs`.

2026-09-21 - ADDON09 with seeded Uhrig jobs; why MertenBot is slower
-------------------------------------------------------------------

Merten's commit f7747168 seeds `qPlayer.Auftraege.Random` from `AtGetSeedTime()` in every mission,
so the Uhrig stream now differs per `/seed`. All four airlines in one game still share the same
stream (same seed per player), so the mission stays fair.

ClaudeBot, unchanged: **24/24** won against the legacy bots (`006`) and **8/8** against MertenBot
level 2 (`026`), all on **day 41-42**.

MertenBot level 2 in the HA slot (`002`), same 24 seeds: **won 23/24, on days 43-53**. It lost seed 23
to FL (legacy) on day 52. Three causes, from the logs of seeds 7, 17 and 23:

1. **Fleet growth is gated on cash for a 767-300 ER.** `condBuyNewPlane` needs
   `mLongTermStrategy` (default true; the short-term override in Bot.cpp is commented out, so
   `condBuyUsedPlane` never fires). It buys only `mBestPlaneTypeId`, and only once
   `getMoneyAvailable() - DEBT_LIMIT - reserve >= Preis` (~22.7M). `howMuchMoneyToRaise()`
   borrows only to cover a negative balance outside route mode. So MertenBot sits on 20-25M with
   two planes until day 40-45. ClaudeBot borrows to the limit and buys the cheapest capacity
   (Tu 154 10.4M, Il 62 9.9M): 3rd plane on day 15-25, 4th on day 35.
2. **It keeps taking other jobs, and taken jobs are never dropped.** It visits the agency, last
   minute and freight 1,300 times and makes 2,600 international calls per game. 10-26% of the jobs
   in its plans are not Uhrig's. `bDropTakenJobs = false` (BotPlanerAlgo.cpp), so a job taken
   during the day keeps its slot when the next morning's Uhrig jobs, due in 1-3 days, arrive.
   `uhrigBonus` can't protect jobs that aren't issued yet. The result is 1-6M of fines and Uhrig
   jobs carried unplanned until they expire.
3. So even on two planes it is slower: seed 17, 3.45 Uhrig jobs a day on days 0-40, against
   ClaudeBot's 4.3 a day on days 0-20 before its third plane arrives.

The obvious MertenBot fixes: skip job taking in ADDON09 (or drop taken non-Uhrig jobs), and allow
cheap or used planes plus credit there. Not done: MertenBot code is outside ClaudeBot's scope.

2026-09-21 - Missions: 55% -> 84% won
-------------------------------------

All 26 missions at `/setbotlevel 006` (ClaudeBot as HA against two legacy bots and the idle human).
`scripts/run_missions.sh` now takes `SEEDS` (default 1, as before) so each mission can be played on
several seeds; all numbers here are HA wins over seeds 1-8 unless stated.

**Start of session: 57/104 (seeds 1-4). End: 175/208 (seeds 1-8) in one full sweep, plus ATFS09
0/8 -> 4/8 afterwards, so ~179/208, 86%.** MertenBot's reference
(Merten's `dataMISS_*_mission_merten` files, 200 games per mission) wins ~90%.

| mission | before (s1-4) | after (s1-8) | what did it |
|---|---|---|---|
| FIRST | 3/4 | 7/8 | used planes; jobs ranked by passengers too |
| EASY | 1/4 | 7/8 | fuel from the tank does not count against Gewinn; used planes |
| NORMAL | 0/4 | **8/8** (day 42-58) | mission-city bonus also in route *scheduling* |
| HARD | 2/4 | 5/8 | jobs not routes; only size-5 image campaigns |
| ADDON02 | 0/4 | 7/8 | freight first, no passenger jobs; used planes |
| ADDON04 | 0/4 | 4/8 | jobs not routes; used planes |
| ADDON06 | 2/4 | **8/8** | jobs not routes |
| ADDON07 | 0/4 | **8/8** | jobs not routes, so the cash buys two new planes |
| ATFS01 | 0/4 | 6/8 | hoard cash (no ads, planes, gates, fittings); jobs not routes |
| ATFS04 | 1/4 | 7/8 | five used planes; jobs not routes |
| ATFS05 | 0/4 | 5/8 | jobs not routes |
| ATFS06 | 1/4 | 6/8 | as ATFS04, plus protection and holding the pliers |
| ATFS09 | 1/4 | 4/8 | jobs not routes (gap 3-4.5x -> 1-1.9x); then no freight (0/8 -> 4/8) |

Unchanged and won: TUTORIAL, FINAL, ADDON01, ADDON03, ADDON05, ADDON08, ADDON09, ADDON10,
ATFS02, ATFS03, ATFS07, ATFS08, ATFS10 (several faster).

### The one lesson: missions are not the free game

Routes are the free game's engine and a mission's worst enemy. A mission starts on two planes with
1-4M; a route pair has to be rented, advertised and flown for weeks before it pays, and at the free
game's 190% fare every flight costs **two image points** (BookFlight: price > 1.5x the threshold is
-20/10). Measured symptoms: 14.6M of ads by day 14 in ATFS01, 30M in ADDON07, 32M in ATFS09, image
at -1000 in ATFS04/ATFS05, airline grounded at the -10M floor in ADDON04. Switching routes off
(`Mission::noRoutes`) won or sped up every mission tried **except FINAL and ADDON10**, which went
8/8 -> 0/8: their 204M/238M bill needs the route economy.

### Rules found in the source that decide missions

- **EASY counts `PLAYER::Gewinn`**, which only sums flight saldos and fines. A flight's saldo is
  charged only for kerosene bought at the gate - fuel drawn from the own tank was paid at the Arab
  and never reaches Gewinn. So a tank refilled daily at any price makes every flight count its whole
  revenue (`Mission::fuelFromTank`). One 1000-unit tank covers ~2.5 days of a two-plane airline.
- **Used planes** cost `ptPreis * (Zustand/100)^2 * (Baujahr-1900)/120`: 0.8-3M where the cheapest
  new plane is 9.9M. Three are on offer, redrawn daily. Condition and build year move together, so
  filtering for Zustand >= 60 (with the plane advisor) left nothing affordable - dropped. The
  breakdown damage that prompted the filter is a random seasonal event (Sim.cpp, 75,000 a time),
  not condition-related. But a plane under Zustand 60 loses 1 image per flight: in HARD six used
  planes took the wins from 5/8 to 2/8, so no museum there.
- **Airline image campaigns** add `cost/10000 * (size+6)/55` points, truncated: 100k/point at
  size 3, 62.5k at size 4, 50k at size 5. ClaudeBot bought the largest *affordable* size, often 3.
  HARD now only buys size 5 (free game untouched).
- **Sabotage protection** (security office) blocks both the legacy bots and ATFS06's third actor,
  but a legacy bot that keeps running into it takes the pliers (`ITEM_ZANGE`) on even days and
  wipes every airline's flags. The pliers return every morning and an airline may hold several
  items, so ClaudeBot picks up today's pair (dropping yesterday's) - no trust needed. Protection
  costs ~100k per plane per day, so it only starts once the five planes are owned (the fifteen
  days only count from then). Not used in ATFS04 (legacy saboteurs only): 7/8 without, 6/8 with.
- **NORMAL**: the flags were never short of planes - on seed 1 Rio, Tokyo and Johannesburg sat at
  100% utilisation while New York was held at 0% and Moscow at 17%. The scheduler's
  "90% of the best reachable pair" gate hid the long, lower-yield mission pairs from every plane;
  New York was confiscated for low utilisation and re-rented five times. Adding the mission bonus
  to the scheduling value (it already ranked the renting) won all 8 seeds. Delhi was never buyable.

### Tried and not kept

- Used planes ranked by speed for ADDON04: bought a 15-seat plane that carries nothing. A per-km job
  bonus and no profit floor there: fines rose (6.5M on seed 1), 0/8. Plain job mode won 4/8.
- Cheap route fare (45% of threshold, +1 image per flight) in HARD with routes on: image grew 3x
  faster, but route income fell by two thirds; jobs-only beat it (5/8 vs 3/8). The constant
  `kImageTicketPercent` is still wired to HARD but unused while HARD flies no routes.
- No freight in every job-only mission: ADDON04 4/8 -> 1/8 (freight legs are miles), ATFS01
  6/8 -> 4/8. Kept for ATFS09 only.
- Protection in ATFS04: 7/8 -> 6/8.
- Used planes (fleet of six) in ATFS05 and ATFS09: 5/8 -> 4/8 and 4/8 -> 0/8. Reverted.

### Open

- **Free-game check not run this session.** Every change is behind a `Mission` flag and the diff
  was reviewed for that (the free game keeps the 190% fare, never enters the museum, security or
  tank paths; the one shared line, `marginalValue()`, now uses 64-bit arithmetic with an identical
  result). Worth one paired `run_measurement_claudebot_tycoon.sh` before the next free-game session.
  Tried at the end of the session: the game directory's `threadpool.rb` is currently Merten's
  mission-mode copy (`miss = 0..50`, `_mission_merten`), so the script ran 0 games and concat.py
  failed on the empty set. Left as it was.
- **Freight contracts ending short.** A contract re-planned by `schedulePendingFreight()` can find
  no window for its last tons before the deadline (Helsinki -> Berlin, 6 of 60 tons, logged five
  days running) and is fined. Seen in the missions; the same code runs in the free game.
- Still below MertenBot: ADDON04 4/8, HARD 5/8, ATFS05 5/8, ATFS09 4/8, and the NASA missions win
  but on day ~100 (MertenBot ~54).

### Sweep over seeds 0-100 (commit 16668dd2)

All 26 missions x 101 seeds at `/setbotlevel 006`, TIMEOUT=600: **2120/2626 won, 81%** - the 8-seed
estimate (~86%) was optimistic. All 2626 games exited 0, none without a result, no display failures.
Seed 0 is the unseeded (wall-clock) game. MertenBot column: Merten's `dataMISS_*_mission_merten` files
(200 games, opponents and seeds not recorded there), so it is a guide, not a paired comparison.

```
mission       ClaudeBot med.day no result      MertenBot med.day
TUTORIAL     96/101  95%     2.0         0    200/200 100%     1.0
FIRST        67/101  66%       4         0    183/200  92%       4
EASY         70/101  69%     7.0         0    116/200  58%     8.0
NORMAL      101/101 100%      47         0    199/200 100%      75
HARD         67/101  66%      48         0    198/200  99%    20.5
FINAL        86/101  85%    98.0         0    200/200 100%    56.0
ADDON01      96/101  95%    37.0         0    200/200 100%    18.0
ADDON02      91/101  90%      10         0    167/200  84%      12
ADDON03      62/101  61%    21.0         0    153/200  76%      21
ADDON04      41/101  41%      30         0     46/200  23%    30.0
ADDON05     101/101 100%      74         0    194/200  97%    43.0
ADDON06     101/101 100%      21         0    200/200 100%    21.0
ADDON07      98/101  97%    46.0         0    199/200 100%      40
ADDON08      68/101  67%    56.5         0    199/200 100%      69
ADDON09     101/101 100%      41         0    196/200  98%    47.0
ADDON10      97/101  96%      97         0    200/200 100%    54.0
ATFS01       67/101  66%      14         0    175/200  88%      10
ATFS02       99/101  98%      61         0    192/200  96%    35.0
ATFS03       99/101  98%      64         0    195/200  98%      42
ATFS04       71/101  70%      26         0    200/200 100%    60.0
ATFS05       76/101  75%    68.0         0    191/200  96%      55
ATFS06       74/101  73%    25.0         0    195/200  98%      95
ATFS07       74/101  73%   143.0         0    200/200 100%    92.0
ATFS08       96/101  95%    51.0         0    199/200 100%      42
ATFS09       29/101  29%      45         0    198/200  99%    45.0
ATFS10       92/101  91%    60.0         0    200/200 100%    60.0
total      2120/2626  81%
```

Largest gaps to MertenBot: ATFS09 29% (99%), HARD 66% (99%), ADDON08 67% (100%), ATFS04 70% (100%),
ATFS07 73% (100%), ATFS06 73% (98%), ATFS05 75% (96%), FIRST 66% (92%), ATFS01 66% (88%).
Ahead of it: EASY 69% (58%), ADDON04 41% (23%), and on speed NORMAL (day 47 vs 75) and ATFS06 (25 vs 95).

### Used planes: at least 100 seats (2026-09-22)

The museum pick is the largest cabin we can pay for, so on a day where only a small plane was
affordable it bought that - 1.2M for a 15-seat plane in EASY and ADDON04. Now a used plane needs
100 seats (`kUsedPlaneMinSeats`), except in ATFS04/ATFS06, whose goal counts planes, not seats.
Paired on seeds 1-100 against the seeds 0-100 sweep:

| mission | before | after | L->W | W->L |
|---|---|---|---|---|
| FIRST | 66 | 66 | 0 | 0 (all 100 games identical) |
| EASY | 69 | 72 | 3 | 0 |
| ADDON02 | 90 | 96 | 6 | 0 |
| ADDON04 | 40 | 42 | 5 | 3 |
| ATFS04 | 70 | 68 | 3 | 5 -> floor removed there |
| ATFS06 | 73 | 70 | 3 | 6 -> floor removed there |

With the exemption, ATFS04/06 replayed identically to the sweep on seeds 1-24 (48/48 games).
Net: +11 wins over the four capacity missions.

### Routes in ADDON08 and ATFS09, re-checked on 100 seeds (2026-09-22)

Merten's question: was switching routes off wrong in these two? Paired on seeds 1-100 against
the sweep (no routes):

| variant | ADDON08 | ATFS09 |
|---|---|---|
| no routes (sweep) | 68 | **29** |
| routes | 73 (22 L->W, 17 W->L - noise), median day 57 -> 68 | 6 |
| routes, no advertising | **91** (26 L->W, 3 W->L), median day 56 -> 32 | 0 |
| routes, no advertising, fare 95% | - | 5 |

- **ADDON08: routes without advertising, kept.** The share price is pulled to
  10 * TrustedDividende, and TrustedDividende only climbs (1 every second day, up to the
  dividend) on days whose *whole* cash flow `Bilanz.GetSumme()` is positive - loans and share
  issues count as income, plane purchases and ads as spending. Routes give the steady daily
  income; ClaudeBot's route advertising produced the negative days.
- **ATFS09: no routes stays.** With routes ClaudeBot spent 32-42M on route ads in 45 days, which
  company value does not count. Without the ads the 190% fare took the image to -500..-766, at
  50,000 of company value a point. At 95% the image still slid (-250..-420) and route income over
  45 days stayed well below the winner's job income. MertenBot wins it with routes, so the gap is
  ClaudeBot's mission route economy, not routes as such.
- After restoring ATFS09, seeds 1-24 replayed identically to the sweep (24/24).

2026-09-22 - Free game: image-aware ticket price (+10.6%)
---------------------------------------------------------

Harness note: the game directory's `threadpool.rb` is in mission mode (and `run_build.sh` reinstalls
the repo copy, also mission mode). Free-game batches were run from a scratch copy with `miss = [-1]`,
`name = ""`, 300 runs, `ruby -I . tp_free.rb --prefix=X "/setbotlevel 6"`. Fresh baseline at
`16668dd2`+: **2.149e9** (seed base 0), 2.135e9 (seed base 1000).

### Kept: `2d172eee` price routes at 147% while image is low

Above the threshold T, CalcPassengers sells min(cabin, 1.5 * cabin * T / price * f) economy seats,
f = (400 + ImageTotal) / 1100. Revenue is therefore flat for any price above 1.5 * T * f. At full
image that is 190%, which is where ClaudeBot always priced. Early on (route image 90, airline
image ~20) it is ~134%, so the 190% fare earned nothing extra and did cost image: BookFlight's
`Add` is +10 (Zustand > 98) -1 (cabin) +3 (crew) -20 (price over 1.5 T). Condition is checked
*before* the flight wears the plane, so a plane's first leg of the day departs at 100 (Add -8 -> 0)
and every later leg at ~97 (Add -18 -> -1 airline image and -1 route image). At <= 150% the
price term is -10 and no leg loses anything.

Rule (free game only): while the full-cabin price 1.5 * T * f + 2% is <= 144% (or <= 148% while
the current price is already <= 150%), keep any price between it and 150%, set 147% on a change;
otherwise the old 190% with the [160, 198] keep band. Airline image comes from `mImageAfterAds`
(it may only be read in the ad agency), route image is read in the office.

| arm | seed base 0 | seed base 1000 |
|---|---|---|
| 147% regime | **+10.6%** (t +23, 291/300) -> 2.377e9 | **+9.9%** (t +20, 280/300) -> 2.346e9 |
| switch at 136 | -5.8% vs kept | |
| switch at 160 | -2.5% vs kept | |

First attempt priced at exactly the full-cabin price with a band of [target-2, 150]: **-18.4%**.
The band was so narrow that the daily kerosene move pushed every route out of it, and each raise
re-stamps HoursBefore on every planned leg (strips up to half the passengers inside 48 h) - the
same pathology as the old daily repricing. Wide keep bands are essential for any price rule.

### Tried and reverted (all paired, 300 games, seed base 0)

Against the old baseline (2.149e9):
- No airline image campaign while the target is 0 (the `Image < target` loop fired whenever image
  was negative): -3.6%. Image drifted to -136 by day 25 and had to be bought back.
- Route image target 100 (+ saturation 400): -10.6%, worse in 300/300. Repeated against the new
  baseline: -16.1%. Partly more early ad spend; mostly because `baseTotal` in the image payback
  rule and the regime switch move with it - image timing is very sensitive.
- Bid for one gate more than needed from day 1: +0.05% (t 2.6) - noise.
- Second broker visit (and bank visit) later the same day: -11%. The afternoon purchase spent the
  cash the ad agency needed; image collapsed from 537 to 4 by day 59. Gated on "ads done today":
  still -11% because `checkRoomOpen(ACTION_WERBUNG)` is false before the agency opens. Gated on
  weekday: +0.03% - buying in the afternoon gains nothing.
- `kPlanesPerGate` 15 -> 8: -0.88% (t -18). Image decay fell only ~20%; gates are not the main
  source of late image loss - the second-leg -1 per flight above is.

Against the new baseline (2.377e9):
- `kImagePaybackDays` 20: -4.9%; 7: -2.2%. 10 stays.
- Rank planes by net revenue per hour per million of price on the best rented pair in range:
  **-66.9%** (bought Il 86s that found no short pairs to fly: 25 flights/day on 39 planes).
  Restricted to types reaching the target route: -30.6% (mostly A 300). Bigger cabins do not
  translate into revenue - route demand, not seats, bounds a bigger plane. The crew ranking with
  the 250-passenger cap stays.

### Learned

- Early cash is extremely sensitive: ~2M extra spent before day 16 delays the first 767 and costs
  ~10% of the day-59 score.
- Late-game image loss (~100 points/day, ~5M/day in ads) is mostly the second-leg -1 per flight at
  190%. Cabin upgrades to level 2 on seats/trays/deco would turn it into 0 (Add -9), but at 3.47M
  per 767 they pay back in ~50 days - not tried.

### Next

- The route box does not rent pairs for a plane's range, so any non-767 type idles. A mixed fleet
  would need range-aware renting first.
- Takeover defence, route gate rework, route re-typing are still open (see 2026-09-16/17).

2026-09-22 - International offices and calls (neutral, free game only)
----------------------------------------------------------------------

Last minute jobs were already implemented (`executeCheckAgent1()`); added international offices and
calls in `d0c28617`:
- `bidOnOffices()` at the boss: offices in cities our route planes wait in, any city from 10 planes;
  no bid that leaves less than 1M after the 3x-price purchase. Offices cost 3 x Preis once (not
  scored) and Preis / 30 a day (Citymiete, scored) - a few hundred to ~2,000 a day.
- `callInternational()` from the personal office, up to 4 rounds a day at least 2 h apart, taking
  passenger jobs with `takeJobsFromBoard()` and freight with the new `takeFreightFromBoard()`
  (the depot logic, now for any board); `bookCallCost()` per round. `JobTaker` is a std::function.

Free game, 300 paired games against `2d172eee`: **+0.13% (t +0.8)**, neutral. Diagnosis: over a 60-day
game the bot held up to 27 offices and read 4,620 international offers, and exactly one fitted an idle
window. The route scheduler fills every plane's week: 0 idle windows abroad, 1-7 at home of 5-10 h
each, at every fleet size. That is also why the travel agency and last minute boards only give ~40
jobs a game. Jobs of any source can only pay if the scheduler leaves room for them.

Missions (seeds 1-8, 208 games, level 006): calls + bids 180 -> 162 wins; calls only 180 -> 165
(ATFS07 8->1 / 8->4, ATFS06 6->2, ATFS01 6->3, ATFS04 7->4, ADDON03 6->3; ADDON04 3->5). Missions start
with offices at their gifted route cities, and the extra jobs took the goal's windows. Both are off in
missions; seeds 1-2 replay identically to before (52/52).

Next: the job sources are only worth anything if some plane time is left for them - e.g. a plane or
two kept off routes for jobs, or route legs that leave the night hours abroad free.

Open points (as of 2026-09-22, after `951b9640`)
------------------------------------------------

Free game: ClaudeBot 2.380e9 vs MertenBot 2.346e9 solo (seeds 1-300), a tie. MertenBot without
international calls (ROBOT_USE_ABROAD off for its own airline only): 0.765e9 (-67%).

Most promising
1. **Leave room for jobs.** International calls are worth 3x to MertenBot and nothing to ClaudeBot,
   because ClaudeBot's route legs fill every plane's week (0 idle windows abroad, 1-7 at home).
   *Tried 2026-09-22 (c5dab076): one job plane -33.6%.* *Done (18d257b2): chaining planner, +9.5% / +10.5%.*
   Next: a second job plane (needs the broker to value types without rented routes), freight from
   the depot and the agency board inside the chain planner.
2. **Non-767 types.** The route box rents long-haul pairs sized for the 767; Il 86 / A 300 idle (-67% /
   -31%). A mixed fleet needs range-aware route renting first.
3. **Cabin upgrades.** At 190% every leg after a plane's first of the day costs 1 airline + 1 route image
   point (~100/day late). Level 2 seats/trays/deco make that 0; 3.47M per 767, ~50-day payback (estimate).

Older structural points
4. Takeover defence (MertenBot took over ClaudeBot 296/300 head-to-head on 2026-09-16; not re-measured).
   Idea: late buy-back to 51%.
5. Route gate knife-edge: `kMinRouteValueShare` 95 good, 98 -41%. Value per hour per actual plane.
6. Route re-typing once a better type is affordable.
7. Start-weekday effect (up to ~65%) - not re-checked since the pricing change.

Missions (seeds 0-100: 81% won)
8. ~~ATFS09 29%~~ 83% after 0379018a. Largest gaps now: HARD 62% (MB 100%), HARD 66% (99%), ATFS04/06/07 ~70% (~100%), FIRST/ATFS01 ~66%.
   FINAL, ADDON10, NASA missions won around day 100 against ~55.
9. International offices in missions (ATFS05): the free-game version cost wins (180 -> 162/165 on
   seeds 1-8), so it is off there. Needs a variant that leaves the goal's plane time alone.
10. ~~ADDON04 (miles) not modelled.~~ Modelled in 1c3d8e31: 44% -> 67% won over seeds 1-48.

Small
11. Freight contracts re-planned by `schedulePendingFreight()` can end short and be fined (free game too).
12. ~~`%ld` with a 32-bit SLONG in the image log prints 4294967292 for -4 (cosmetic).~~ Fixed in
    `8826dbf8`: SLONG is int32_t, so those logs use `%d`. Done for the image and the rejected-route
    value - the other ~125 `%ld` in ClaudeBot.cpp print non-negative values and are left alone.

2026-09-22 - Job planes (open point 1): not worth it with the current job planner
---------------------------------------------------------------------------------

`c5dab076`: `kJobPlanes` starting planes (fewest seats first) are kept off routes - the route box,
route renting and `scheduleRouteFlights()` skip them, `collectGaps()` gives them an open tail, and
`bidOnOffices()` bids in any city while they exist. Off (`kJobPlanes = 0`); the free game replays the
previous HEAD exactly (300/300).

Paired over 300 free games against `951b9640` (2.3805e9), one job plane (the 737-400 that otherwise
flies Berlin-Lanzarote):

| arm | score | note |
|---|---|---|
| 1 job plane | -36.4% (295/300 worse) | fines 3.4M, 98 jobs flown a game of ~175 taken |
| + open-tail bookkeeping fix | -33.6% (295/300 worse) | fines 1.4M |

By day 20 the job plane adds ~4.5M of jobs and freight and saves 6.6M of route ads (no Lanzarote
pair), but route revenue is 10.6M lower, and the fleet ramps later (6.7 aeroplanes on day 40 against
9.8). Per plane: ~0.28M a day from jobs vs ~0.5M on routes; MertenBot's planner gets ~0.72M a day
per plane from jobs in days 0-15. International offices supply most of the jobs (144 of 175 taken
in game 0).

Bug fixed on the way: booking a job or freight leg into an open tail moved the window's start but
not its city, so every later job was planned as if the plane had never left, and the empty legs the
game inserted pushed jobs past their deadlines. Free game only - missions on seeds 1-8 went
180 -> 175 wins with it (4 L->W, 9 W->L), so they keep the old bookkeeping (seeds 1-2: 52/52
identical).

Next for this point: a chaining job planner (sequence jobs by city and time for a plane, split
freight tonnage), then re-try one job plane. Or pick jobs by premium per plane hour against the route
value per hour instead of a fixed job plane.

2026-09-22 - Chaining job planner: one job plane +9.5%
-----------------------------------------------------

`18d257b2`: the smallest starting plane (737-400) is a job plane all game. `planJobPlanes()` runs with
every round of international calls (office, up to 4 a day, >= 2 h apart): it pools every office's
passenger jobs and freight contracts (a contract = one item of n = tons / (seats / 10) round trips)
plus our own unplanned jobs, and chains them per job plane by 200 randomised greedy passes, each
step taking the item with the best gain per hour of plane time (empty leg and waiting included).
The best pass is taken by phone and planned in order. Owned jobs go first (their fine is at stake).

What mattered, all paired over 300 free games against HEAD `07ae9e87` (2.3805e9, seed base 0):

| arm | result |
|---|---|
| greedy boards only (previous entry) | -33.6% |
| chaining planner, 48 h offer horizon | -18.5% |
| + route sizing counts the job plane again | -1.9% |
| + back to routes on day 25 | -8.6% (reverted: kJobPlanesUntilDay = 99) |
| offer horizon 96 / 48 / 36 / **24** / 18 / 12 h | -11.1 / -1.9 / +3.2 / **+9.5** / +7.4 / -6.0% |
| 24 h, seed base 1000 | **+10.5%** (t +7.6, 200/300) |

- The job plane's open tail now ends at the same 24 h horizon, so the greedy agency / last minute
  boards cannot book it further out than the chain planner looks. With a 4-day tail they filled it
  before the planner ran and it planned nothing.
- Leaving the job plane out of `wantRoutes` held the first 767 on Delhi with the 757 until day 25
  (one pair instead of three); the network wants the starting fleet's size.
- Too short a horizon idles the plane between calls; too long books it out with the first call's
  offers. Jobs + freight by day 20: 10.2M (24 h) against 8.1M (48 h), 6.3M (96 h), 1.2M before.
- Route scheduler: a free-game plane in a city no rented pair touches is sent home first (the game
  flies the empty leg). Only relevant if a job plane ever returns to routes.

Now 2.606e9 against MertenBot's 2.346e9 on the same seeds: +11.1% (t +3.9, 192/300).
Missions replay identically (seeds 1-2, 52/52).

Next: a second job plane (the broker needs rented routes to value types, so 2 starting job planes
would never buy a first 767), and the depot / agency / last minute boards inside the chain planner
(today they fill the open tail greedily between rounds of calls).

2026-09-22 - Job planner: what else was tried
---------------------------------------------

All paired over 300 free games against `18d257b2` (2.606e9, seed base 0).

| arm | result |
|---|---|
| calls: 8 rounds a day, >= 1 h apart (was 4, >= 2 h) | -0.16% (t -0.9), 89 games identical |
| agency / last minute / depot boards leave the job plane alone | -0.45% (t -0.6) |
| those boards feed the chain planner too (taken at the counter, planned at the next office visit) | -0.63% (t -0.7) |
| planner passes 50 / 800 (was 200) | +0.18% / -0.36%, both noise -> **50 kept** |
| **second job plane (the 757) once a bought plane flies routes** | **+0.91% (t +2.9)**, seed base 1000 **+0.79% (t +3.2)** |

- The job pool is not the limit: adding the counter boards did not raise job income (10.2M by day
  20 either way). The job plane flies ~17.6 block hours a day (empty legs included), so plane time is.
- A first version of the counter-board arm lost 31% and bankrupted two games: `executePendingChain()`
  rejected ids returned by the take functions (`object >= AnzEntries()`), so 176 of 223 chosen jobs
  were never planned and were fined. The take functions return album ids; test with `IsInAlbum()`.
- Timing (Merten: callbacks must stay in the millisecond range, the game is single-threaded):
  60-day game, 2 job planes, 50 passes: office mean 1.4 ms / max 7.8 ms, international calls with
  the planner 1.4 ms / 4.0 ms, everything else < 4.3 ms.

Committed as `eccadf80`. Score now ~2.63e9 (seed base 0).

Next: a job plane is only worth it while it earns more than it would on routes; later bought
planes could become job planes too if the boards pay (they would need range-appropriate offers).

2026-09-22 - ADDON04: miles are the goal, and aeroplanes make miles
-------------------------------------------------------------------

`1c3d8e31`. ADDON04 is won on `NumMiles`, and `Schedule.cpp:668` adds the distance of **every** flight
before it looks at the type - the empty positioning legs the game inserts count too. So the mission
is distance flown in 30 days; the premium only has to keep the airline flying.

`Mission::wantMiles` (ADDON04 only):
- a job must still clear the mission profit floor (`kMissionJobGain`), and among the jobs that do,
  the ranking adds `kMilesValue` (100) per mile; freight adds the miles of all its legs, out and back;
- the used fleet grows to `kMissionMilesFleet` = 10 instead of 6.

| arm | seeds 1-24 | ratio mean (lower is better) |
|---|---|---|
| baseline | 11/24 | 105 |
| rank by miles per hour alone, solvency only | 7/24 | 138 (11.3M of fines, cash at -4.4M) |
| profit + miles, profit floor dropped to 1,000 | 5/24 | 142 |
| **profit floor kept, miles as the ranking bonus** | 13/24 | 100 |
| miles weight 30 / **100** / 300 | 9 / 13 / 13 | 101 / 100 / 99 |
| used planes ranked by speed | 12/24 | 102 |
| **+ used fleet 8 / 10 / 12** | 17 / **17** / 17 | 96 / 95 / 95 |
| routes on (the 147% fare fixed the old image collapse) | **0/24** | 175 |

Seeds 25-48, not tuned on: baseline 10/24 (ratio median 104) -> **15/24 (93)**. Over seeds 1-48
that is 21/48 -> 32/48 (44% -> 67%); MertenBot's sweep figure was 23%.

- Cash is the real constraint: every arm that let marginal jobs in to chase miles ran up fines and
  grounded the airline. Miles come from *more aeroplanes flying*, not from picking longer jobs at
  any price.
- Routes remain fatal here even at the 147% fare, so the old note stands for a new reason: the
  mission is 30 days, and a route pair's image and rent never pay back inside it.
- Other missions replay identically (all 26, seeds 1-2: 50/52 games, the two differences are
  ADDON04 itself).

Next for ADDON04: the fleet plateau at 8-10 is cash, not the museum - the used planes on offer are
bought as soon as they are affordable. Earlier capital (shares, credit) would buy more of them.

2026-09-22 - ATFS09: the fleet was short of work, not of aeroplanes
--------------------------------------------------------------------

`0379018a`. ATFS09 is won on company value after 45 days
(`PLAYER::CalculateStatistics`: aeroplanes at `CalculatePrice`, shares held at the market price,
cash minus credit, tank, and **image x 50,000**).

Measured against the best opponent over 24 seeds, the old jobs-only build lost on throughput, not
on capital: 199 flights on 6.3 aeroplanes against 407 on 5.8, 3 branch offices against 21, 4.5M of
fines, company value 51.9M against 65.5M. That is 0.7 flights per aeroplane per day.

`Mission::useOffices` (ATFS09 only): bid for offices (at most `kMissionMaxOffices` = 8, and only
once the mission fleet is complete), phone them every round, and put the whole fleet through
`planJobPlanes()`. The decisive detail is the idle-window horizon: `collectGaps()` now gives a job
plane the 24 h tail whether or not routes exist. With the 4-day tail of a no-route mission the
travel agency and last minute counter booked the fleet full before the chain planner ran, and it
planned nothing (`230 item(s) on offer, planned 0`).

| arm | value (24 seeds) | won |
|---|---|---|
| baseline (jobs only) | 51.9M | 7/24 |
| own-share buy-back in the last 10 days | 51.6M | 4/24 |
| offices, uncapped | 39.1M | 3/24 |
| offices <= 8, fleet first | 51.9M | 8/24 |
| low job profit floor | 35.3M | 0/24 |
| routes + the free game's image-aware fare | 38.7M | 1/24 |
| routes + fare + buy-back | 36.3M | 0/24 |
| **offices + planner + 24 h tail** | **120.4M** | **19/24** |
| + museum planes | 79.2M | 12/24 |
| seeds 25-48, not tuned on | **121.0M** (base 48.9M) | **21/24** (base 9/24) |

- The share buy-back does raise our own value (price 87 -> 153 in one game, +9.5M on one seed), but
  the opponents hold part of our float and are revalued with it, so it nets out. Removed again.
- Buying aeroplanes is *not* wasteful here although a purchase is booked at ~85% of its price: with
  the planner keeping them busy, 13 aeroplanes fly 490 flights and earn 164M of job premiums.
- Free game unchanged (300/300 identical), other missions unchanged (50/52 games on seeds 1-2).

Next: HARD is now the largest mission gap (62% against MertenBot's 100%); ATFS06 and ATFS05 sit at
75%. The same "short of work" test - flights per aeroplane per day against the best opponent - is
worth running there.

2026-09-22 - MertenBot: `setConstBonus(-1M)` in the freight missions (review of a pending change)
------------------------------------------------------------------------------------------------

Asked to check the uncommitted `BotFunctions.cpp` hunk that adds `planer.setConstBonus(-1000 * 1000)`
to the `ROBOT_USE_FREE_FRACHT` and `ROBOT_USE_MUCH_FRACHT` branches of `Bot::grabFlights()`, i.e.
ADDON02 (`Level` 8, MUCH_FRACHT) and ADDON03 (`Level` 9, FREE_FRACHT).

Harness: `./AT /quick <12|13> /seed <1..100> /setbotlevel 005` - HA is MertenBot (Nemesis), FL and PT
are the classic cheating bot, SA idle. Paired by seed, 100 seeds per mission, ~5 s a game.

| metric (HA) | ADDON02 base -> change | ADDON03 base -> change |
|---|---|---|
| win rate | 83% -> **88%** (t 1.9) | 81% -> **91%** (t 2.6) |
| best opponent / us (lower better) | 86.4 -> **83.0** (t -2.6) | 76.5 -> **65.4** (t -3.8) |
| days to the end of the game | 11.90 -> **11.48** (t -4.8) | 21 (deadline) |
| mission goal `Ziel` | 92.7 -> 92.3 | 579 -> **642** (t 5.8) |
| freight tons | 993 -> 994 | 618 -> **658** (t 3.9) |
| cumulative op saldo | 2.66M -> 2.09M | 4.64M -> **-4.03M** |

So yes, it wins both missions more often - and yes, it fines away both starter jobs:

- `Scheduled 0/2 existing` at the first planning run in **200 of 200 games** (baseline: 2/2 in all
  200), mean fines booked by day 2 **-171,000** in both missions against 0 in the baseline.
- Mechanism: `runAddNodeToBestPlaneInner()` (BotPlanerAlgo.cpp:855) only inserts a node when
  `score > bestPlaneScore`, which starts at 0. The `minScoreRatio` filter in `prepareGraph()`
  (BotPlaner.cpp:474) exempts `wasTaken()` jobs, but that insertion threshold does not, and a taken
  passenger job scores `Praemie - cost + Strafe - 1,000,000 < 0`.

Tried the obvious fix (not committed, tree restored): apply `constBonus` only to jobs not yet taken.
Keeps every gain and removes the fines - ADDON02 88% won / ratio 82.2, ADDON03 92% won / `Ziel` 643,
total fines -4.6k and -34k (against -192k and -220k with the raw change, -27k and -631k in the
baseline). Note `DIFF_TUTORIAL` uses `constBonus` as a *positive* 1M and was not re-measured, so
such a guard wants to stay negative-only or be checked on mission 0.

Open point: ADDON03 ends ~8.4M of op saldo below the baseline either way. The mission is scored on
tons, not money, so it wins anyway, but the airline finishes the 21 days in the red.

2026-09-23 - ATFS09: the "rating disagrees with the win flag" item is closed
---------------------------------------------------------------------------

Carried open since 2026-09-19 (item 5 of the 2026-09-20 list): ATFS09 printed `BesterGegner` 508
while `SiegHA` was 1. **Does not reproduce.** `MISSIONS=49 SEEDS="1..8" ./scripts/run_missions.sh 006`
today: 5/8 won, and flag and ratio agree in every game (won at 14/51/59/68/95, lost at 101/105/107).
The 100 MertenBot runs from last night agree too (ratio 5-55 -> won, 144 -> lost). The ATFS09 rework
in `0379018a` is the most likely reason it went away; no game-source change was needed.

What the ratio actually is, so it is not misread again. `printPostGameInfo()` (Misc.cpp:2228) prints
`100 * bestEnemy / bestBot` over `STAT_MISSIONSZIEL`, and for ATFS09/ATFS10 that stat falls into the
`default:` branch (Player.cpp:7017) and is the **raw company value**, not a percent of a goal - the
same quantity `HasWon()` compares. So on these two missions the ratio is a head-to-head number and
>100 really does mean an opponent is ahead; it is not comparable to the percent-of-goal ratios of
the other missions.

Three ways the two numbers can still part company, none of which fires in the current runs:

- **Bankrupt opponents, and this is the likely cause of the 508.** `PLAYERS::UpdateStatistics()`
  (Player.cpp:7266) skips `IsOut != 0`, so an eliminated airline's `STAT_MISSIONSZIEL` freezes at its
  last living value - and `printPostGameInfo()` still counts that frozen value in `bestEnemy`, while
  `HasWon()` for ATFS09/ATFS10 (and ADDON03/04/06) skips out players. A rich-but-cash-dry opponent
  that goes under therefore keeps inflating the ratio after it has stopped being a rival. In today's
  8 games: seed 4 has **both** FL and PT out, frozen at 95.6M and 90.5M against HA's 176.6M - had
  ClaudeBot been at ~19M, as it was in September's ATFS09, the row would have read "won, ratio 503".
- `bestBot`/`bestEnemy` start at 0 and are taken with `max`, so a negative company value reads as 0:
  an all-negative field prints `NaN`, and a bankrupt opponent looks like a harmless 0.
- The CSV `Ziel` column is `GetAtPastDay(1)` (BotHelper.cpp:1004) but the `BotMission` row is day 0,
  so the two outputs of one game can legitimately differ by a day.

**Correction to `at-harness-no-bankruptcy` (the memory note, now rewritten): airlines do go bankrupt
in headless runs.** CheatAutoSkip's synthetic right-click (Aufsicht.cpp:767) only dismisses the
briefing while `CanCancel != 0` (Aufsicht.cpp:873-893), and `CanCancel` is forced to FALSE whenever
any player is below `DEBT_GAMEOVER` (-5M) or has image < -990 (Aufsicht.cpp:832-836). The boss
briefing then runs and `bankruptPlayer()` fires (Dialog.cpp:3502 -> 3627/3770). Its signature in the
logs is **Geld == -10,000,000 with Kredit == 0**, because `bankruptPlayer()` sets `Money =
2 * DEBT_GAMEOVER`; the money columns are live but everything else in the row (planes, Firmenwert,
Ziel) is `GetAtPastDay(1)` and stands still from then on. Seen in 4 of 8 ATFS09 games today and in
mission 49 of last night's MertenBot sweep (FL at -9.08M on day 43, out on day 44, Firmenwert pinned
at 87,953,168 for days 44 and 45).

2026-09-23 - ClaudeBot can walk to a coordinate in the airport
-------------------------------------------------------------

On request, and explicitly outside RULES.md: `ClaudeBot::walkToPosition(XY)` and
`walkToPlate(XY)` send the character to a spot in the airport instead of to a room, hold the bot
logic off it while it walks, and let the bot carry on afterwards. Nothing calls them in the free
game, so the score is untouched; `kWalkDemo` in ClaudeBot.cpp switches on a demonstration that
walks once an in-game hour.

**Writing PERSON::Target is not enough - it is overwritten every step.** While a character walks
freely, `PERSON::DoOnePlayerStep()` recomputes `Target` from `PLAYER::TertiaryTarget` on every
step (Person.cpp:1865-1874). The target the machinery actually works from is
`PLAYER::PrimaryTarget`, in plate coordinates: `UpdateWaypoints()` derives the secondary
(room entrance) and tertiary (staircase) waypoints from it, and the main loop calls
`UpdateWaypoints()` every 256 ticks and `UpdateWaypointWalkingDirection()` every tick for every
player (Takeoff.cpp:1992-2003). So a free walk is `PLAYER::WalkToPlate()` - which already exists -
plus `DirectToRoom = 0`, `WaitForRoom = 0`, and leaving whatever room we are in.

**Holding the target against RobotPump() takes exactly one field: WorkCountdown.** It replaces
the target in three ways: shifting the action queue on and calling `WalkToRoom()`
(Player.cpp:3406-3438), re-planning after eleven idle ticks (`StandStillSince`,
Player.cpp:3386-3396), and `WaitForRoom` sending the character back to a room that was busy
(Takeoff.cpp:1225). The first two are both gated on `WorkCountdown <= 0`, so one countdown covers
both, and the third is just cleared. One catch: `RobotActions[0]` has to be `ACTION_NONE` or
RobotPump returns before the countdown is ever decremented - action pending **and** countdown set
is a permanent freeze. A second catch: a walk ordered from `RobotExecuteAction()` has its
countdown divided afterwards by `ROBOT_USE_WORKQUICK_2` and friends (Player.cpp:4234-4242), so the
value has to be multiplied up first.

**Verifying it needs a multiplayer run.** The single player harness plays every day with
`Sim.CallItADay` set - the auto-skip sets it in the briefing room (Aufsicht.cpp:767) - and then
`PERSONS::DoOneStep()` skips walking entirely and calls `RobotExecuteAction()` where the character
stands (Person.cpp:3262-3272). That is why every action of a harness game reports "not in the
room": the bots never walk and never enter a room. `./scripts/run_multiplayer.sh 2 1 66 0` does
run days at walking pace (gohome 0), and both its bots are ClaudeBots.

**Two engine traps found while verifying, both of which strand a bot for the rest of the day.**

- `PLAYER::LeaveAllRooms()` compares the **raw** `Locations[]` value against `ROOM_AIRPORT`, and
  for the frame after a bot steps out of a room the airport entry reads
  `ROOM_AIRPORT | ROOM_ENTERING` (Takeoff.cpp:1891). It therefore flags the airport itself as
  being left.
- Worse, and what actually happened: the bot's leave branch hands `ROOM_ENTERING` to index `d-1`
  and to nothing else (Takeoff.cpp:1889-1893), so a room at index **0** leaves the player with no
  location at all - and `CalcRoom()` returns without writing when it finds none
  (Player.cpp:784-791), so `GetRoom()` answers forever with the room that was left. Nothing walks
  the character out again and the bot stands still. A room does end up at index 0:
  `EnterRoom()` takes the first free slot (Player.cpp:645) and the airport entry is not always
  there - in a network game each peer overwrites the whole array from `ATNET_ENTERROOM`
  (AtNet.cpp:801-806). In the second verification run HA managed **1 action against PT's 54**
  before I put an airport entry underneath first in `leaveRoomsForWalk()`; with that, both bots
  play the day out normally. The engine's own `LeaveRoom()` has the same blind spot, so a bot in
  that state is probably stranded in a multiplayer game with or without this feature - worth a
  look in Takeoff.cpp one day, which is out of ClaudeBot's reach.

**A free walk is not a safe way to cross the airport.** A character that steps on a room's
announcement rune is pulled into that room whatever it was doing (Person.cpp:2265-2277), and the
pathing is "walk x, then y" with no way to avoid one. `roomAtPlate()` says whether the
*destination* carries one - the path cannot be checked - and the first demo walk of run 1 ended
with the bot doing its workshop action inside the aircraft broker's office.

2026-09-24 - Network play check: ClaudeBot froze every walking day
-------------------------------------------------------------------

Checked network play for desyncs, crashes and bugs with `./scripts/run_multiplayer.sh --debug`:

- **Run A** - `2 20 36 10 4 1 1` (Saboteur + ClaudeBot, 20 days, go home 10:00, salary cuts,
  human room actions): no layout mismatch, the peers agree on all 100 day-end fingerprints, no
  crash. The Saboteur ordered 11 sabotage jobs against HA and used 4 items without a divergence.
- **Run B** - `2 1 36 0 0 1 1` (one day at walking pace): no desync, but **HA executed one action a
  day** (ACTION_PERSONAL at 09:04) and nothing else until 18:00, while PT made 62. It even paid the
  penalty for its starting job.

**Cause.** ClaudeBot's first action of the day is a room (RobotInit() plans PERSONAL/VISITMECH),
not ACTION_STARTDAY. Walking out of the morning briefing that way leaves the room it enters
alone in `Locations[]` - logged `loc=29,0,0,...` - and leaving a room at index 0 strands the bot
for the rest of the day, exactly the trap `leaveRoomsForWalk()` already guarded against for the
walk demo. MertenBot starts with ACTION_STARTDAY, executed where it stands, and is not affected.

**Fix.** `ensureAirportUnderneath()` (split out of `leaveRoomsForWalk()`) runs at the start of every
`RobotExecuteAction()` and puts a ROOM_AIRPORT entry underneath when there is none.
**Run C**, same setup: HA 74 actions (8-10 an hour all day), PT 60, no desync; the guard fired once,
on the first action. In the `/quick` harness it fires 0 times (nobody walks), so the free-game
score is unchanged by construction.

Found in code review, not fixed (engine code, outside ClaudeBot's files):
- (Retracted) db7a9fcf dropped `PLAYER::NetSynchronizeFlags()` from the ITEM_XPARFUEM branch of
  `GameMechanic::useItem()`, which looked like `PlayerStinking` no longer being replicated. It is not
  a bug: the end of `useItem()` sends ATNET_SYNC_ITEMS and ATNET_SYNC_FLAGS for every branch. The
  branch now carries a comment saying so. Lesson: read the whole function, not only the diff.
- (Fixed the same day) The electro-room glove->Red Bull swap ran for a SuperBot on every peer, and
  each peer chose glove or shock from its own copy of the inventory. The owner's ATNET_SYNC_ITEMS
  often arrives before the remote copy of the figure reaches the machine, so the other peers showed
  a shock (and reset `IsDrunk`) for a player who had used the glove. The same race existed for remote
  humans. Now only the peer for which `NetIsAuthoritative()` holds decides; the glove outcome
  reaches the others through ATNET_SYNC_ITEMS and the shock through the new ATNET_ELECTROSHOCK
  (0xadaa0609), both handled by `PLAYER::ElectroShock()`. Verified with a temporary test change that
  gave both MertenBots the glove and sent them to the machine hourly, on a walking-pace network day:
  1 glove swap and 27 shocks on the host, 27 ATNET_ELECTROSHOCK received on the client, the same
  items in both fingerprints, no layout mismatch.
- (Fixed the same day; "cosmetic" was wrong) MertenBot executed ACTION_NONE when the humans went home.
  Cause, traced with temporary logging: at walking pace nothing counts `SpeedCount` down, so an
  action executed on arrival or through WaitWorkTill left it behind - 1 for a roomless action like
  ACTION_CALL_INTER_HANDY. MertenBot empties the queue after each action (`kAlwaysReplan`), and when
  `Sim.CallItADay` switched on, `PERSONS::DoOneStep()` counted the stale 1 down and called
  `RobotExecuteAction()` on the empty queue. `PLAYER::RobotExecuteAction()` now sets `SpeedCount = 0`
  where the action is really carried out (host or single player). Replayed with `/mpseed 1234` for 6
  days: before, CALL_INTER_HANDY at 09:59 on day 2 was followed by ACTION_NONE at 10:00; after, the same
  09:59 call and no ACTION_NONE all game, no desync. Seeded single player (`/quicker -1 /setbotlevel 6
  /seed 7`, 5 days): BotStatistics of all four airlines byte-identical before and after.
2026-09-24 - Hurricane: glue and stink bombs
--------------------------------------------

On request: Hurricane (BotLevel 7) now collects and places the two floor items, using the free
walk of f8aae494 and MertenBot's item chains as the model.

- **Chains.** Glue: paperclips at the route box (`collectPaperclips()`, on the daily visit),
  handed over and the glue taken at the freight depot (`collectGlue()`; glue someone else paid
  for is taken too). Stink bomb: glove at the Arab (`executeArab()` now does both MG and glove),
  ACTION_ENERGY_DRINK to the vending machine (the engine swaps glove for drink on arrival and
  RobotExecuteAction() is never called there at walking pace, so plans are capped at 3 a day in
  RobotPlan()), drink traded for the bomb at the kiosk (`executeKiosk()`, before the filler arm).
- **Placement.** `startItemDrop()` runs last in RobotExecuteAction(); `tickItemDrop()` rides on the
  per-tick getOnThePhone() hook, keeps WorkCountdown topped up while the walk is under way (the
  straight-line estimate was half of the real walk upstairs, and the hold ran out every time), and
  uses the item on arrival. Stink bomb: the victim's gate entrance (RUNE_2WAIT of a gate in
  `Gates.Gates[]`), where every passenger of that gate walks through the stench; sick passengers
  cost the airline image. Glue: the corridor plate in front of the victim's office door. The glue
  lands on the plate the character *faces* (SIM::AddGlueSabotage uses Phase, which is the last
  walking direction while standing), so the walk goes to two plates beside it and then one step
  towards it.
- **Rules.** RULES.md now lets ClaudeBot read `Sim.ItemGlue` in the freight depot and a
  competitor's `Gates.Gates[].Miete/Nummer` anywhere (gate owners are visible in the airport); the
  `Sim.ItemClips` line said "freight depot" where the engine keeps the clips at the route box and
  was corrected.
- **Verification.** Only walking days use any of it (`Sim.CallItADay == 0`), so the `/quick`
  harness and the Tycoon score are untouched by construction. `run_multiplayer.sh 2 1 77 0`: PT
  picked up paperclips -> glue, glove -> drink -> bomb; bomb dropped at FL's gate 112/9 after 143
  ticks, glue dropped on 39/2 in front of FL's office after 130 ticks; 0 fingerprint mismatches.
  First try aimed at the door plate itself (39/1) from 37/1 and stopped at the top of the stairs
  (35/2) every time: the door row is a nook, not a corridor. Candidates now start with the
  corridor plate and rotate on each failed try; a walk that stops short gives up after 20 ticks.
- **Not measured / open.** Whether a victim actually walks into the glue was not observed (FL is an
  idle human in the harness and never leaves its office). The glue sticks whoever crosses the plate
  first, ourselves included. Whether the image a stink bomb costs a competitor is worth the Arab
  and kiosk walks is unmeasured - no harness plays walking days at scale.

## 2026-09-25: savegame round trip
- Problem: on load, the game reads the savegame into the existing ClaudeBot objects and runs no RobotInit() until the next morning (the autosave is taken at 17:00). mMission was only built in startNewDay(), so after a mid-day load it held defaults (ticketPercent 0) or the previous game's mission. mCastawayRoutes, RouteState::castaway, mVisitedMuseumToday, mVisitedDesignerToday, mCallsToday, mLastCallTime and LocalRandom were not saved.
- Fix: ClaudeBot savegame version 112 saves these fields. A new flag, mMissionReady, is cleared on load, and RobotPlan()/RobotExecuteAction() then call refreshMission(). The loader resets the designer plane, the item-drop walk and the walk-demo state. No change to free-game play before a load, so no measurement.
- Test: run_loadtest.sh on slots 0/1/2/11 (formats 202/203/204): all load and play on with 0 errors. Slot 11 exercises the v111 fallback. Round trip: the user saved by hand while ClaudeBot played HA (day 2, 15:5x). Loading it read every block to its end marker; HA came back with day 1, 1 route, 10 agency visits, 3 calls, route box and personal office done, fuel 349 units/day, 2 sabotage hints. It resumed at 15:52 and played to day 59 with 0 errors. The loader now logs this summary and reports an error if a block does not end at its end marker (the assert is compiled out of release builds).

## 2026-09-25: rules review, engine fix for robots stranded in a room
- Audited ClaudeBot against RULES.md. The user updated RULES.md (commit 8606edba) to allow what the audit flagged, except that ClaudeBot rewrote `qPlayer.Locations[]` (`ensureAirportUnderneath()`) in every mode.
- Root cause, now fixed in the engine: in the robot leave branch (Takeoff.cpp), leaving the room at index 0 of `Locations[]` emptied the array, `CalcRoom()` kept the stale room, and the robot stood still for the rest of the day. The same branch turned an empty slot under the room into a bogus "entering room 0". Now, when nothing remains, the robot goes back into the airport (`ROOM_AIRPORT | ROOM_ENTERING` at index 0). `PLAYER::LeaveAllRooms()` compares without the ENTERING/LEAVING flags, so it no longer flags the airport itself as being left. `ensureAirportUnderneath()` is removed from ClaudeBot.
- Verification, `run_multiplayer.sh 2 1 36 0 0 1 1 1234` (walking day, same seed both times): old code with the ClaudeBot workaround had HA 29 actions / PT 47; the engine fix has HA 76 / PT 33. Both runs: 0 layout mismatches, peers agree on every fingerprint. One earlier unseeded fix run showed an HA money/hroute divergence at day end; it did not recur with the seed, and the old code's clean run proves nothing about that unseeded game. Watch for it.
- Smoke tests (Tycoon and Hurricane, 5 days) pass. `/quick` never walks, so the free-game score is unchanged by construction; not re-measured.

## 2026-09-26: MertenBot baseline, free game and all missions (commit ed60075b)
- MertenBot was reviewed for rule deviations and bugs; the findings are in `bugs.txt` (committed, nothing fixed). This baseline is the reference for measuring fixes to them.
- Free game: `./scripts/run_measurement_bot.sh` (HA = MertenBot level 2, FL/PT classic bot, SA idle, seeds 1-300). Day 59 SaldoGesamt HA **2.395e9**, Firmenwert HA 2.094e9. 300/300 games complete. Consistent with the 2.346e9 of 2026-09-22.
- Missions: `SEEDS="1..50" ./scripts/run_missions.sh 2` (same setup, all 26 missions, 1300 games, ~31 min). All 1300 exit 0, no display failure. Per mission (HA win rate, mean days, BesterGegner = best opponent / us):
  TUTORIAL 100% 1.1d 54 | FIRST 96% 3.8d 79 | EASY 62% 8.9d 109 | NORMAL 100% 84.4d 20 | HARD 100% 22.1d 18 | FINAL 100% 55.4d 64 |
  ADDON01 100% 22.7d | ADDON02 94% 11.4d 81 | ADDON03 98% 21d 65 | ADDON04 18% 30d 182 | ADDON05 92% 47d 53 | ADDON06 100% 21d 50 | ADDON07 100% 39.7d 67 | ADDON08 100% 71.4d 51 | ADDON09 98% 47.1d 69 | ADDON10 100% 55.5d 56 |
  ATFS01 78% 11.9d 79 | ATFS02 92% 39.4d 43 | ATFS03 26% 346d | ATFS04 100% 39d 40 | ATFS05 98% 55.7d 29 | ATFS06 100% 50d 72 | ATFS07 100% 90.9d 37 | ATFS08 98% 43.6d 30 | ATFS09 96% 45d 51 | ATFS10 100% 60d 3.8
  Weakest: ADDON04 (18%), ATFS03 (26%), EASY (62%), ATFS01 (78%).
- Archived under the game dir in `baseline/mertenbot_ed60075b/`: `freegame/dataBOT_freegame_*.csv` (for `compare_paired.py`), `missions/m<mission>_b2_s<seed>.csv` (BotMission/BotStatistics lines), `mission_summary.txt` and the aggregation script `summ.py` (point its glob at the new run's logs to compare).

## 2026-09-26: MertenBot fixes C1/C2/C3 from bugs.txt (not committed)
- C1: `condUpgradePlanes()` early return checks `mMoneyReservedForUpgrades` (was repairs). C2: `actionSellShares()` refreshes money each pass, final-run target is objective minus cash, stops when a pass sells nothing. C3a: `howMuchCrewToHire()` computes the multiplier in 64 bit. C3b: caps it at the planes the next purchase can buy.
- Free game, paired against the ed60075b baseline (300 games): all three **-0.67% (t -7.7)**; C1+C2 -0.05% (t -2.4); C1+C2+C3a -0.03% (t -1.0, same). So the crew cap C3b costs ~0.6%: spare crew lets the bot buy planes without an HR detour first.
- FINAL (50 seeds, all three fixes): 100% wins, 55.4 days, BesterGegner 64.2 - identical to baseline; HA op saldo 1.456e9 vs 1.473e9.
- Per the user's rule ("commit if same or improved") the requested set was not committed. Working tree holds C1+C2+C3a. Results in the game dir under `runs/fix_c1c2c3`, `runs/fix_c1c2`, `runs/fix_c1c2c3a`.

## 2026-09-26: MertenBot fixes C5/C6/C7 from bugs.txt (not committed)
- C5: `routesFindNextStep()` returns `routeWithPendingPlaneUpgrades` for UpgradePlanes (was routeToBuyPlanes; only the log reads it). C6: route theft targets only routes the competitor rents (`Rang != 0`) with utilization > 0; the fallback short-cut that took the first route regardless is gone. The fallback stays: stealing a route we do not fly still hurts the competitor (RouteWegnehmen() deletes their flights), releasing it next morning is fine. C7: without a spy, `calcRouteScore()` assumes one combined competitor utilization `kUnknownCompetitorUtilization` = 30 instead of 50 per competitor.
- Level-3 baseline recorded first at b9df4b4e: day 59 SaldoGesamt HA 2.268e9 (game dir `baseline/mertenbot_b9df4b4e_l3/`).
- Level 2, paired against b9df4b4e (`runs/fix_c1c2c3a`): identical in 300/300 games. So C7 never fires in the free game (the spy is always hired before the route box).
- Level 3, paired against the new baseline: **+1.04% (t +3.8)**, better in 139, worse in 120. Route thefts 1 -> 0 in 300 games. Likely cause: the broken fallback nearly always supplied a target, so RouteTheft (weight 10) kept being drawn and then waited for 8M and 70 hints, blocking other sabotage.

## 2026-09-26: MertenBot fix C4 from bugs.txt (not committed)
- `planRoutes()` decided whether a plane needs a positioning flight by comparing its location with `fromCity`, although the first leg departs from `toCity` when the time slot says route B. Now the departure city follows the leg. A plane already at the other end starts one slot later from there (no empty leg); otherwise the buffer is for the auto flight to the real departure city.
- Free game, paired on 300 seeds against 2419182c: level 2 **+5.06% (t +19.8, better in 288/300)**, 2.394e9 -> 2.515e9; level 3 **+5.44% (t +19.3, better in 287/300)**, 2.292e9 -> 2.416e9. In the level-2 logs: 7,708 "one slot later" starts, 20,791 positioning buffers.
- Runs in the game dir under `runs/c4_l2`, `runs/c4_l3`. Missions not measured.

## 2026-09-26: MertenBot fix C8 from bugs.txt (measured, not committed)
- Change: `areWeBroke()` no longer returns Somewhat while `mDoRoutesMaxCredit` is set (the drawn credit line is deliberate, see `condDropMoney()`).
- Mechanism confirmed in the logs (300 games each): competitor-share sales 1,250 -> 0 (level 2) and 1,334 -> 0 (level 3); "Are we broke? Somewhat" 1,416 -> 0, so in the free game Somewhat only ever happened with the max credit drawn. Emissions unchanged (7,500), they come from the daily rule, not from Somewhat.
- Score, paired against 2ec7c584: level 2 -0.02% (t -1.1, noise), level 3 -0.07% (t -2.3, slightly worse). The sales did not hurt; the cash they raised, fees included, apparently paid for a little growth. Runs under `runs/c8_l2`, `runs/c8_l3`.

## 2026-09-26: MertenBot fix C10 from bugs.txt (measured, not committed)
- Change: `assignPlanesToRoutes()` returns a route plane taken off for repairs only when `Zustand >= min(100, TargetZustand)`, the rule `findPlanesAvailableForService()` uses for job planes.
- Free game: identical in 300/300 games at levels 2 and 3. Route planes almost never drop to Zustand <= 90 there (0 phase-outs at level 2, 8 at level 3, none held back).
- Missions with damage: ADDON01/ADDON07 fly no routes (0 held back). ATFS10 (all planes -40% on day 35), paired on seeds 1-50 against HEAD: 441 hold-backs in 10 seeds; win rate 100% both; company value (the mission goal) **-2.08% (t -1.5, worse in 11, better in 1, same 38)**, op saldo -5.5% (t -1.9). Grounding planes until fully repaired costs more than flying them damaged.

## 2026-09-26: MertenBot repair budget for raising WorstZustand (experiment, not committed)
- Idea (user): like kImagePaybackDays for image ads, cap the extra repair cost (points above WorstZustand + 20, ptPreis/110 each) per night at kRepairBudgetPercent of the daily op saldo (mWeeklyOperatingSaldo / 7; no limit without a financial advisor). Only points the mechanic reaches tonight (+15, +18 below 60) are counted against the budget.
- ATFS10, paired on seeds 1-50 against HEAD (company value = mission goal / op saldo): no limit (new counting only) -2.9% / -1.0%; 50% -6.8% / -5.4%; 25% -4.6% / +0.2%; 10% +1.1% / +4.7%; 5% +5.0% (t 2.3) / +8.1%; **0% +8.2% (t 3.8) / +12.8% (t 5.0)**, repair cost -94%. All variants win 50/50.
- Free game level 2: identical in 300/300 games at 5% and 0% (paid repairs never happen there).
- Missions, seeds 1-20, 0% and 5% behave identically: ATFS02 identical; ADDON07 still 20/20 wins, 35.4 vs 37.6 days; **ADDON01 wins 20 -> 14: HA goes bankrupt on day 4-6 in 6 games** (Geld -10M, Kredit 0). Not yet explained; suspect: the reserve for repairs drops from the full target cost to tonight's (small) cost, so more money looks available in a mission that starts 10M in debt.
- Open: explain/fix ADDON01 before committing. Working tree has kRepairBudgetPercent = 0. Runs under `runs/m50_rb*`, `runs/mdmg_*`, `runs/rb0_l2`, `runs/rb5_l2`.
- ADDON01 bankruptcy explained (replay of seed 12): on day 1 (no financial advisor, so no budget) Freiburg was at 58; tonight's reach 76 was all free, so points 77-100 counted as "not charged tonight" and the target went to 100 with 0 reserved. On day 2 cash was negative and nothing reserved, so condVisitMech() never sent the bot back to lower it, and night 2 charged 13M (76 -> 91 and 77 -> 92). The old code reserved the whole target and stopped at 80.
- Fix: points above WorstZustand + 20 are only committed as far as tonight's reach, so the reserve holds everything the targets commit to. Seed 12 then matches HEAD for the first nights and wins on day 12.
- Re-check at kRepairBudgetPercent = 0, against HEAD: free game level 2 identical 300/300; ADDON01 20/20 wins (was 14), 19.1 vs 21.6 days, op saldo +12% (t 3.1); ADDON07 20/20, 35.4 vs 37.6 days; ATFS02 identical; ATFS10 50/50, company value +8.2% (t 3.8), op saldo +12.8% (t 5.0), repair cost -94%.

## 2026-09-27: MertenBot ATFS03 (commits 6cb924f3, a792e22c)
- Baseline (26% wins, 346 days mean) reproduced on 2fb5c664: 4/24 wins, the rest ran to day 500 with 2 planes, 0 routes and ~2e9 cash.
- Cause: ATFS03 sets ROBOT_USE_NOCHITCHAT, which raises `condCallInternationalHandy()` to Low permanently. Low actions are ordered by random + walking distance, and the mobile call has no walk, so it won nearly every Low slot (187k calls in 500 days, seed 1). ACTION_VISITMAKLER (Low) never ran, `mBestPlaneTypeId` stayed -1, so `actionStartDay()` never switched to routes and `condBuyNewPlane()` never fired. The few games that won got one broker visit by chance.
- Fix 1: the NOCHITCHAT floor for the mobile call is Lowest. ATFS03, seeds 1-50: **49/50 wins (26%), 39.7 days (346)**. The loss (seed 21) is FL winning on day 32. ATFS08 wins 3.0 days sooner (t -4.2); ATFS09 company value +144% (t 5.5); both still won 24/24.
- Fix 1 alone made ATFS10 company value -52% (still 24/24 wins): the bot grows much faster (10 vs 4.9 planes on day 34), but after "all planes damaged by 40%" on day 35 its fleet never flew again. Route/job planes left service at Zustand <= 90 and returned only above 90; with kRepairBudgetPercent = 0 the mechanic repairs only up to WorstZustand + 20 (~77), so they were grounded for good. The old build hid it because it was still buying new, undamaged planes after day 35.
- Fix 2: `needsRepairs()` = Zustand <= 90 **and** Zustand < TargetZustand, used for phase-out and for returning planes to routes. A plane now flies again once repaired as far as we pay for. Both fixes, against 2fb5c664, seeds 1-24: ATFS10 company value **+52% (t 4.4)**, op saldo +61%; free game identical in 300/300; ADDON01/ADDON07 identical.
- Runs in the game dir under `runs/atfs03_*`, `runs/nochit_*`, `runs/both_fix`, `runs/addon_base`, `runs/fg_head`.

## 2026-09-27/28: MertenBot review, repair budget and grounding (in progress)
- New code review of Bot*.* (after bugs.txt was closed). Fixed since by the user: freight premium counted once per flight in the planner (4f41fe0e), `assignPlanesToRoutes()` stopping at the first crowded route (7ebeff07), endless loop in `actionBuyAdsForRoutes()`, `applySolution()` ignoring per-plane results, CSV "Available" truncated to int, kerosene stats, museum div-by-zero, `requestPlanRoutes(areWeInOffice)`, repair rules made consistent (f2f54fd3). Still open: planner gain is still 32-bit `int` (overflow now unlikely: ~170 freight nodes in ADDON03); optimizer temperature is in $ and too small, so it is effectively greedy; "High-frequency actions" warning counts per priority, not per action.
- Freight fix 4f41fe0e: free game -0.45% (t -1.8, 124/300 identical). Probable cause: the per-hour score ratio is now correct too, so fewer freight jobs pass `kSchedulingMinScoreRatio` (tuned on the inflated ratio). Suggested test not run yet: node-score fix only, old ratio for the filter.
- Repair budget in ATFS10 (f2f54fd3, seeds 1-24): budget 0 leaves day-35-damaged planes at WorstZustand + 20 (68-80) until the end; budget 10 lets them climb (slowly, ~0.5 points/day on average; all at 100 only in games with a high op saldo), company value +6% (t 1.45).
- Budget sweep, same value in all missions, ATFS10/ADDON01/ADDON07 seeds 1-50 vs 0% (`runs_repairbudget/v*`): ATFS10 company value rises monotonically (5% +6.6%, 20% +16.9%, 40% +38.9%, no limit +70.2%, t 6.5); ADDON01 identical up to 20%, slower from 40% (+2.7 days at no limit); ADDON07 identical up to 5%, slower from 20% (+10.6 days at 100%, negative op saldo at no limit).
- Why ATFS10 rewards budget: grounding loop. With budget 0 the target is WorstZustand + 20; any wear after the night's repair puts Zustand below target, `needsRepairs()` grounds the plane (Zustand < 90 && < target), it is reassigned the next day, and the idle plane blocked `condBuyNewPlane()` ("unused plane of this type exists"). Seed 1: 130M cash, 12 spare pilots, wants a 767 every hour, buys nothing from day 40 to 59 (22 planes vs 70 with no limit). Also: company value grows with Zustand^2 (Planetyp.cpp:279), so repairs pay directly in ATFS10.
- Working tree (uncommitted, user's + mine): `condBuyNewPlane()` no longer checks idle route planes (user); `kPlaneMinimumZustand` replaced by `kPlaneGroundZustand` (90) and `kPlaneGroundHysteresis` (10): ground below the threshold if Zustand < target, return at target or threshold + hysteresis (capped at 100). 90/10 = f2f54fd3 behaviour; free game at 90/10 = 2.192349e9, identical to f2f54fd3, so the `condBuyNewPlane()` change does not matter there.
- Grounding sweep, missions (budget 0 as committed), seeds 1-50 vs 90/10 (`runs_ground/v<thr>_<hyst>`): 0/0 (never ground) ATFS10 +108% (t 13.5), ADDON07 -5.0 days (t -9.2), ADDON01 +0.5 days (t 0.7, noise); 60/x +72..79% ATFS10; 70/x +56..67%; 80/x..90/x identical to 90/10 in all 150 games (with budget 0 no mission plane is ever between 80 and 90); 95/x -2..-3% (t -1.8). Paid extra repairs are 0 in every mission run, so these runs cannot show the user's point: keeping planes above 80 keeps WorstZustand >= 80 and repairs to 100 free; letting them drop makes raising WorstZustand expensive later.
- Free game grounding sweep (budget 10) was running at the end of the session: 90/10, 0/0, 60/0, 70/0, 80/0, 85/0, 95/0, 300 games each, CSVs in `runs_ground_fg/v*`. Evaluate: `python compare_paired.py 'runs_ground_fg/v90_10/*.csv' 'runs_ground_fg/v<x>/*.csv'` in the game dir.
- Next: (1) evaluate the free-game sweep; (2) rerun the budget sweep (same value everywhere, `kRepairBudgetPercent` default in Bot.h, drop the free-game override in RobotInit) at the best 1-2 thresholds, since threshold and budget interact (with a budget > 0, a low threshold lets WorstZustand fall and repairs above +20 get paid); (3) free game for the final pair, then commit. Scripts: `runs_ground/scripts/` in the game dir (sweep.sh = budget, sweep_ground.sh = missions, sweep_ground_fg.sh = free game, evalsweep.py DIR REF VALUES... for missions). ~2 min per mission variant (150 games), ~5 min per free-game variant (300 games).
- 2026-09-28, results. Free game grounding (budget 10), 300 games vs 90/10: 0/0, 60/0, 70/0, 80/0, 85/0 identical in 300/300 (no free-game plane ever meets the grounding condition); 95/0 -1.02% (t -4.7).
- Combined sweep threshold x budget, missions seeds 1-50, same budget in missions and free game (free-game override removed), `runs_combo/v<thr>_<hyst>_<budget>`. Best everywhere: **0/0/0 (never ground, budget 0)**. Against it, ATFS10 company value / op saldo: 0/0 with budget 5..no limit -0.6..-7.3% / -3.7..-10.7% (t down to -19.7; paid repairs ~80M per game, the user's point: raising WorstZustand is expensive); 60/0/x -14..-20%; 80/0/0 -52%, 80/0/40 -35% (keeping planes above 80 does not make up for grounding them); 90/10/x -50..-52%. ADDON07: 0/0/0 wins in 31.8 days, every grounding threshold >= 60 takes 36.9, budgets >= 20 slower still, no limit ends with negative op saldo. ADDON01: 0/0 +0.5 days vs thresholds >= 80 (t 0.7, noise), budgets up to 20 identical, no limit +2.2 days. Free game at 0/0/0: identical to 90/10 with budget 10 in 300/300.
- Working tree now: kPlaneGroundZustand = 0, kPlaneGroundHysteresis = 0, kRepairBudgetPercent default 0, free-game override removed, plus the user's `condBuyNewPlane()` change. Not committed. With threshold 0, `needsRepairs()` is never true, so the grounding code (and `stillNeedsRepairs()` for planes grounded for other reasons, e.g. no crew) could be simplified; the repair budget code is dead at 0 as well.

## 2026-09-28: MertenBot planning heuristic (free game)
- Setup: 300 seeded free-game runs per variant (~5.3 min), paired against the previous commit; winners confirmed on seeds 301-600 (`threadpool.rb --seed-base 300`). Scripts and CSVs in the game dir under `runs_plan/` (measure.sh, measure_sb.sh, sweep_params.sh + setparams.py for `name=value` variants).
- Total: 6b3312fb -> 8b475287 **+8.50% (t 6.2)**, 2.503e9 -> 2.716e9, better in 202/300. Seeds 301-600 from c7f1dceb: +8.12% (t 6.0).
- 8ee9c3bc freight regression: node score stays exact (premium - n * flight cost), but the filter/sort ratio again charges one flight only: +0.55% (t 2.2).
- c7f1dceb simulated annealing: temperature was 1000 -> 1 in the same units as the gain ($), i.e. greedy. Now kSARounds / kSATempStart / kSATempEnd in $, geometric cooling, last round greedy. Start temperatures 1k..1M $ all -0.1% (noise): the neighbourhood (remove the worst untaken job per plane, greedy re-insert) almost never produces a worse solution, so acceptance hardly matters. Also noise, before and after the later commits: kNumToRemove 2/3 (with and without 300k $), 30 rounds (+5% runtime), horizon 3/5 days, 5% job skipping, 4 insertions per plane, kFreightMaxFlights 6.
- a47e5cdb kSchedulingMinScoreRatio 140k -> **100k** per flight hour: the strongest lever. 40k -29.5%, 60k -9.2%, 80k +1.2%, 90k +5.0%, 100k +5.96% (t 4.8), 110k +5.5%, 120k +3.7%, 200k -36.6%; seeds 301-600: 100k +5.75% (t 4.3), 110k +3.9%. Re-checked after 8b475287: 90k -0.1%, 110k -1.2% (t -1.7).
- 8b475287 replace a scheduled passenger job (port of the user's stash "dropFlights"): **+1.97% (t 3.2), seeds 301-600 +2.24% (t 3.9)**, runtime unchanged. Differences to the stash: the stash reset bestPlaneScore to 0 whenever a gap was 0 (a worse position could win), rebuilt the whole path for every candidate in the normal pass, and let freight replace jobs (an incomplete freight job is removed again, the dropped job would stay lost). The port estimates the swap gain from node scores and edge costs first and only checks timing for candidates that beat the best normal insertion. Taken jobs carry their fine in the node score, so they are only dropped if the new job pays more than premium + fine.
- Rejected: refit cost 15,000 on freight<->passenger edges (the game charges it, Schedule.cpp:786) -0.21% (t -1.75). kSchedulingMinScoreRatioLastMinute (the filter for every job due today, not only Last Minute jobs) stays 10k: 0 and 1k +0.21% (t 0.6, identical to each other in all games), 2.5k +0.1%, 5k +0.1%, 20k 0%, 40k -1.8% (t -2.3).
- e6763898 / a18a4c66 moves between planes: `runRelocate()` moves a random passenger job to the best position on another plane, `runSwap()` exchanges one passenger job each between two planes (fallback: best position anywhere); after remove-worst in every round, chance per plane and round kRelocatePercent / kSwapPercent. With both at 0 identical to 8b475287 in 300/300. Seeds 1-300 vs 8b475287: relocate 10/25/50% +1.02/+0.83/+0.68% (t 1.8/1.5/1.1), relocate 25% with SA 300k $ +1.03%, swap 10/25% +1.16% (t 2.3)/+0.97%, swap 25% with SA +0.82%, both 25% + SA + 30 rounds +1.02% (357 s). Swap 10% alone on seeds 301-600: +0.05% (did not hold). **Relocate 10% + swap 10%: +1.23% (t 2.1), seeds 301-600 +0.89% (t 2.0)**, together about +1.1% (t ~2.9); wall time 330 s vs 326 s. Committed as default. SA temperature still makes no measurable difference, even with these moves.
- Total of the session so far (seeds 1-300): 6b3312fb -> a18a4c66 **+9.83% (t 6.8)**, 2.503e9 -> 2.749e9, better in 211/300.
- Not tried yet: dropping/replacing freight jobs; the job ratio filter by source (travel agency vs international calls).

## 2026-09-29: MertenBot early route for a starter plane (commit 30e716b9, measured)
- Idea (user): put the long-range starter (757-300, range > 6000) on a route from day 0, keep the other starter (737-400) on jobs. Review found and fixed several bugs before commit (route never rented while !mDoRoutes, route rented with no plane / for a type we do not own, stale mRoutesNextStep gate, plane-type ID vs index mix-up, designer TypeId -1).
- Free game level 2, 300 seeded games, paired against 0e2ab449 (2.759e9). Runs in the game dir under `runs_early/<variant>`.
  - 30e716b9 as committed: **-93.0% (t -37.6)**, worse in 300/300. The early route fixes the route's plane type to the 757-300 with numberOfPlanesTarget 6, so all route purchases are 757-300s (dealer score 369 vs 767-300 ER 1030, much pricier): first purchase around day 25, 4.4 planes on day 59.
  - V1 = route rented for existing planes keeps numberOfPlanesTarget = number of those planes (not in FORCEROUTES missions): **-19.0% (t -11.8)**. Uncommitted in the working tree (BotActions.cpp, actionRentRoute()).
  - V2 = V1 + airline image ads keep the price of the next route plane in reserve: -52.4% (t -28). Image ads pay; do not hold them back. Reverted.
  - V3 = V1 + airline image target only once a route with numberOfPlanesTarget > 1 exists: identical to V1 in 299/300 (image ads start after route 2 anyway). Reverted.
  - V4 = V1 with the short-range starter (737-400, Berlin-Lanzarote in all games) on the route instead: -25.5% (t -16.8). Reverted.
- Why it loses (means over 300 games): the early route is ahead until day 13 (saldo 17.2M V1 vs 16.9M ref, +36% on day 5), but the two starters on jobs earn ~2.5M/day together from day 13 (jobs + freight 13->30: ref +43.2M, V1 +12.4M, V4 +31.4M; freight needs the 757). Total income days 13-30: ref 120.5M, V1 93.8M, V4 91.3M, so fewer plane purchases (day 30: 102M vs 64M/60M) and the gap compounds. Extra ad spend (+10M by day 30) follows from the higher ticket count; plane upgrades are 0 in all variants.
- Recommendation: revert 30e716b9 (both starters on jobs until the first bought route). If the idea is revisited, the only remaining shape that could pay is a bridge: early route only until the first bought route is rented, then kill it and return the starter to jobs; the whole available gain is the +0.3..0.9M cash on day 13.
- Why ClaudeBot gets away with it (read from ClaudeBot.cpp, not measured): its routes are not tied to a plane type (the network is sized by fleet size), the 757 only bridges until the first bought plane flies routes and then becomes a job plane while the 767 takes its pair, route planes also fly jobs/freight in their overnight gaps, and its job planner earned less per plane (second job plane only +0.9% there), so a starter off jobs cost it little.
- Bridge variants, all on top of V1, same 300 seeds, paired against 0e2ab449:
  - V5 = when the first route for bought planes is rented, the starter route is given up (`releaseStarterRoutes()`: flight plans cleared, planes into mPlanesForJobs, route marked planeTypeId -1 and removed by the existing removal code): **-11.1% (t -8.9)**, +9.8% (t +9.7) against V1.
  - V5b = V5 + the removal-only reason for the route box at most every 6 h (`condVisitRouteBoxRenting()`; V5 made up to 25 failed removal trips on day 13, 9,551 in 300 games -> 181): -0.10% against V5 (t -1.2), kept as harmless.
  - V5c = V5b + a starter route always prices at kMaxTicketPriceFactorLowImage (140%): every route flight above 150% of the reference price costs airline image (Schedule.cpp:970-996, -10 per step above 100/150/200%), and at 190% the airline image was -24.7 on day 13 (ref -1.4), which the first 767 route inherited. With 140%: -3.9 on day 13. **-8.5% (t -6.8)**, +2.9% (t +8.8) against V5b. Best variant.
  - V5d = V5c + no route ads for the starter route: -19.8% against V5c (t -24.6); route income by day 13 11.4M instead of 16.4M. Reverted.
- Why the bridge still loses (V5c, means): by day 13 the route (16.4M) only replaces the jobs and freight the 757 would have flown (ref 19.9M vs 6.0M), saldo 16.9M in both, and route rent + route ads eat the rest. After the release on day 13 the 757 needs about a week to fill up with jobs again (jobs + freight 13->20: ref +16.4M, V5c +11.5M); from day 20 on job income matches the reference, but the fleet stays behind (5.0 vs 5.6 planes on day 30).
- Working tree: V5c (V1 cap, `releaseStarterRoutes()`, `isStarterRoute()` which excludes FORCEROUTES missions, 6 h removal interval, low price on starter routes). Still -8.5% against 0e2ab449, so the recommendation stays: revert 30e716b9. Runs under `runs_early/v5*`.
- Mechanism kept but off (`kStarterPlaneFliesEarlyRoute = false` in Bot.cpp; both starters on jobs, V5c otherwise): **+0.81% (t +2.2)** against 0e2ab449, 2.7586e9 -> 2.7809e9, better in 189/300, identical in none (FL's day-1 row identical in 287/300). Remaining differences to 0e2ab449 in the free game: route-box planning visit no longer waits for mDoRoutes (extra trip before the switch), `findBestRoute()` only when RentNewRoute is due, broker visit once a day at Medium. Not yet confirmed on another seed base. Runs under `runs_early/jobs`.
- User merged the two route-box actions into one (`condVisitRouteBox()` / `actionVisitRouteBox()`: remove invalidated routes, refresh, fresh `findBestRoute()` on every visit, rent in the same visit when RentNewRoute is due; removal also at day start). His first run spammed "Missing conditions for action: ACTION_NONE" (~2M warnings in 300 games): the action array in `RobotPlan()` was still `std::array<SLONG, 47>` with 46 entries, the zero-filled last element is ACTION_NONE. Fixed to 46.
- Measured after the fix (flag off): **+1.44% (t +3.8)** against 0e2ab449, 2.7586e9 -> 2.7983e9, better in 197/300; +0.62% (t +3.4) against the previous tree (`runs_early/jobs`). 0 warnings. Route-box visits per game 159 -> 166, of which at High 4 -> 107 (RentNewRoute is the default step and now gets High even without an affordable route), routes rented unchanged (4). Runs under `runs_early/merged`, the buggy run under `runs_early/merged_buggy`.
- Open: missions not measured; confirm on `--seed-base 300`; `releaseStarterRoutes()` should return early for FORCEROUTES missions (safe today only because they switch on the morning of day 0).
- User then gave the route box High only when an affordable candidate is known (`mWantToRentRouteId != -1`, else Medium) and added the FORCEROUTES guard to `releaseStarterRoutes()`: +1.06% (t +2.8) against 0e2ab449, 2.7880e9; **-0.37% (t -2.2)** against "High whenever RentNewRoute is due" (identical in 62/300). High visits 107 -> 5 per game. Renting as soon as a route becomes affordable seems to be worth the extra High visits. Runs under `runs_early/current`.

## 2026-09-30: MertenBot mission sweep at e698a19f (analysis only)
- User's sweep: `threadpool_missions.rb`, HA = MertenBot level 2, FL/PT classic, seeds 1-50, all 26 missions (`dataMISS_*` in the game dir). Mission 42 was re-run after the e698a19f rebuild; all other games ran on 6b6a140e (refactoring only). The archive `baseline/mertenbot_ed60075b/` no longer exists; compared against the table of 2026-09-26.
- **1239/1300 wins vs 1173.** Every game produced a result. Big gains: ATFS03 26% -> 100% (36.5 days instead of 346), ATFS01 78 -> 96%, EASY 62 -> 80%, ADDON07 39.7 -> 30.6 days, ADDON01 22.7 -> 15.5 days, NORMAL 84.4 -> 74.4 days, ATFS08 43.6 -> 36.9 days.
- Slightly worse: ADDON01 100 -> 98% (1 bankruptcy), ADDON02 94 -> 90% (PT won 5), ADDON03 98 -> 94% (PT won 3 on day 21). 2 games in 50 is within noise (no per-seed baseline left to pair against).
- ADDON01 seed 50 went bankrupt: on day 0 the repair targets were set to WorstZustand + 20 (free at that moment, 79/78). Flights pushed WorstZustand from 59/58 down to 53/52 before the night, so the same targets raised WorstZustand, and the mechanic charged 6.01M on night 2 with 0 reserved (cash -6.3M after a 3.5M loan repayment on day 0) -> -10M. `actionVisitMech()` checks costsExtra only when the target is set. Also, without a financial advisor budget = -1 (no limit), even at kRepairBudgetPercent = 0.
- Scale: HA paid WorstZustand raises in 18/50 ADDON01 games (43.9M) and 15/50 ADDON07 games (48.5M), none elsewhere.
- Warnings: `actionSellShares(): We do not actually need money` 405x (HARD, FINAL, ADDON05, ADDON10, ATFS01, ATFS02) = useless trips; `checkPlaneLists(): We lost the plane` once in each ATFS10 game (not checked); one `_planFlightJob` invalid day + `planRouteJob` error in NORMAL.
- Next: fix the repair-target drift (margin for tonight's wear, or budget 0 means 0 without an advisor too), then re-run ADDON01/ADDON07 paired against this sweep.
- Fix (user): `condVisitMech()` gives the mechanic at least Low every 4 hours, also when broke (before: nothing when money < 0 and nothing reserved). Any visit after a night lowers the targets to WorstZustand + 20 again, and a target set during a day cannot cost extra that same night (WorstZustand only drops at night, to Zustand, and a night repairs at most 18 < 20 points).
- ADDON01, seeds 1-50, paired with the sweep: 49 -> 50 wins (seed 50 no longer bankrupt, wins on day 21), games paying WorstZustand raises 18 -> 0 (43.9M -> 0), 1.3 days faster (t -2.6; faster 13, slower 3), mech visits 10 -> 29 per game. ADDON07 identical in 50/50 (still pays 48M in 15 games; it has money, so the visits happened before too, and the paid points are within the uncapped budget without an advisor).
- Free game level 2, paired against `runs_early/current`: -0.01% (t -0.34), identical in 200/300 (the reference is before 6b6a140e/e698a19f).

## 2026-09-30: Rules audit of Bot*.* and ClaudeBot.*
- Bot (MertenBot): no game-rule violations found. Only deviation: savegame version 101 (all tags) -> 103 in untagged commits, before the "never bump" rule existed. Some reads outside their room only feed log lines (`calcRouteScore()` from the office path, `xPiloten` at museum/designer, `Baujahr`/`WorstZustand` after a used-plane purchase) - legal under the logging exception, a violation the moment they drive a decision.
- ClaudeBot: free game (Tycoon and Hurricane) compliant. Two violations in mission-only paths, fixed:
  - `executeKerosinTanks()` called `setKerosinTankOpen()` at the Arab (office-only). Now `executeOffice()` opens the tank when the bot runs one (EASY / kUseFuelArbitrage), `Tank > 0` and `TankOpen == 0`. EASY may open the tank up to one office visit later than before.
  - ATFS06 pliers branch of `executeSabotage()` set `WorkCountdown = 2` after dropping/picking up the pliers. Now only when nothing was done.
- Free game unaffected by both (neither condition can be true there): build OK, Tycoon smoke test OK; no measurement run.
- Kerosene price cache: `gKerosinPrice`/`gKerosinPriceDay`/`gKerosinAvgX100` were file-scope statics shared by every ClaudeBot instance, so with 2+ ClaudeBots one airline priced flights with a price another one looked up. Now members `mKerosinPrice`/`mKerosinPriceDay`/`mKerosinAvgX100`; `cacheKerosinPrice()`, `calcCostAndDuration()`, `routePriceBase()`, `routeValuePerHour()` are const members, `fitLegIntoGap()` no longer static. Savegame layout unchanged (same fields, same place), no version bump.
- Tycoon, 300 paired games against 3308b6d4: identical in 300/300 (2.6293e9). Runs under `runs_early/kerosin_member`. EASY / ATFS06 not re-measured.
- Missions re-checked, `MISSIONS="2 46" SEEDS=1..50 run_missions.sh 006 007`, HEAD 0b261064 paired against bb22d167 (before both commits). All 200 games exit 0.
  - EASY (2): identical in 50/50 for both 006 (38 wins) and 007 (36 wins). Opening the tank from the office instead of at the Arab changes nothing.
  - ATFS06 (46), Tycoon 006: identical in 50/50 (38 wins, mean 30.9 days).
  - ATFS06 (46), Hurricane 007: 48/50 identical, 33 -> 32 wins; seed 34 won day 21 -> lost (FL day 34), seed 38 won 6 days sooner (40 -> 34). Bisected to 3308b6d4 (the cache commit changes nothing): in Hurricane the saboteur is visited from day 0, and the pliers pickup no longer releases WorkCountdown, so the next action starts a minute later. Replays of the new build are deterministic (3x identical). One flip in 50 is noise, kept as is. Hurricane may write WorkCountdown freely, so `!actedHere || isHurricane()` would restore the old timing there if it ever matters.

## 2026-09-30: MertenBot (level 2) vs ClaudeBot (level 6), HEAD 834b2a12
- Free game solo, 300 paired seeds: MertenBot 2.788e9, ClaudeBot 2.629e9 (-5.7%, t -2.6, Claude ahead in 144/300). ClaudeBot leads on day 10 (208/300), trails by 18-28% on days 20-40, closes to -5.7% by day 59.
  - The whole gap is liquidations: MertenBot liquidates a classic rival in 97/300 solo games (106 liquidations). In those games Claude is -14.7% (t -4.6); in the other 203 games -0.5% (t -0.18).
  - Money mix (sum days 0-59): routes 28.9e9 vs 27.9e9, jobs 2.42e9 vs 0.62e9, freight 1.54e9 vs 0.53e9, kerosene 2.63e9 (1.36 gate + 1.27 tank) vs 2.29e9, ads 1.90e9 vs 3.14e9, route rent 0.10e9 vs 0.26e9. Day 59: planes 65 vs 98, route pairs 9.3 vs 69.5, cash 558M vs 136M.
- Head to head (`run_competition.sh`, PT = MertenBot 2, HA = ClaudeBot 6, FL classic): **MertenBot liquidates ClaudeBot in 295/300 games** (day 33-57, median 48; `actionOvertakeAirline()`). ClaudeBot has no takeover defence: it emits every share it may every day, holds none of its own, buys none of anyone's. Day-59 saldo 2.867e9 vs 0.885e9 (frozen at liquidation). Before liquidation too Merten leads: Claude ahead on day 10 in 172/300, day 30 in 86/300, day 40 in 112/300.
- Missions, all 26 x seeds 1-50, `run_missions.sh 2 6` (each bot as HA against classic bots): MertenBot 1241/1300, ClaudeBot 1134/1300, 0 bad exits. ClaudeBot better: ADDON04 33 vs 11 wins, NORMAL -23.7 d, ADDON08 -27.7 d, ATFS06 -16.6 d, ATFS04 -9 d, ADDON09 -5.8 d. Worse: FIRST 37 vs 50, ADDON03 30 vs 47, ATFS01 32 vs 48, ATFS04 37 vs 50, ATFS05 40 vs 50, ATFS06 38 vs 50, ATFS07 37 vs 50 (+52 d), FINAL/ADDON10 +44 d, ADDON05 +25 d, ADDON01 +20 d, ATFS03 +27 d, ATFS02 +20 d.
- Next for ClaudeBot: takeover defence (keep >= 50% of own shares or stop emitting once a rival could reach 50%), then consider buying/liquidating rivals itself - that is worth +15% in a third of solo games.
