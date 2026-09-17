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
