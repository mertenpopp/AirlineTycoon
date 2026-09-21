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
3. **NORMAL** needs the three-part fix above.
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
