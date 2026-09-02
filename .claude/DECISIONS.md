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
