## Bot implementation

### Performance

* Free game, 300 seeded games, cumulative operating saldo after 59 days: 0.44e9 (1.9.0) → 2.97e9. Fleet on day 59: 12 → 74 planes.

### Routes

* New route selection and scoring; routes start earlier and are funded with maximum credit.
* Buys several planes per day and may go into debt for them; checks the security advisor's discount first.
* Route utilization is averaged; both directions' route image are maintained.
* Ticket pricing fixed; "image preservation mode" lowers fares when image collapses.
* Designer planes can be planned onto routes.

### Jobs and planner

* Simulated annealing with a temperature in dollars; a new passenger job may replace a scheduled one; jobs can be relocated or swapped between planes.
* Minimum score for new jobs lowered to 100k per flight hour; freight score no longer overstated.
* Travel agency checked every 30 minutes.
* Integer overflow in the cost of long empty legs fixed.

### Money, image, crew

* Airline image is bought up to a payback- and weekend-aware target.
* Only food is upgraded by default; cabin upgrades only in the very late game.
* Pilots and attendants are stockpiled from the late game on (+6.7% on its own).
* Kerosene is topped up once per day for the bulk discount.
* Damaged planes are never grounded; no paid extra repairs by default.
* Bankruptcy avoidance; credit bugs fixed; several money reserves removed.

### Competition

* Sabotage in regular games, including item chains; improved target and job selection.
* "Nemesis" tracking; buys a rival's shares when a takeover is possible and liquidates it.
* Stock trading exploit fixed (trades in chunks of 2,000).

### Missions and rules

* Fixes for freight, plane-upgrade, credit-payback and route missions (NORMAL: mission routes ignore competitors and aim above the goal threshold).
* Many rule-compliance fixes (room and advisor restrictions).
* New difficulty levels; savegame compatibility kept; state for the image target is saved.

### Multiplayer and tooling

* Only the host plans a bot's actions; the bot's mobile phone is shown on every peer.
* `/seed N` replays a game deterministically for paired measurements.
