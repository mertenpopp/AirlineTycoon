# AirlineTycoon

This repository aims to complete the partial source code that is provided as a free bonus in the GOG release
of Airline Tycoon Deluxe.

To run it, you'll need the game assets from the Deluxe edition of the game.
Therefore, you need an existing installation of the game. You can purchase the game from [GOG.com](https://www.gog.com/en/game/airline_tycoon_deluxe) or from [Steam](https://store.steampowered.com/app/331920/Airline_Tycoon_Deluxe/).

## Major Additions

- Many new computer players 
    - you can choose from many different play styles
    - from an easier laid-back competitor to brutal rival
    - from "fair play" to cruel nemesis who will target you specifically
- Massive multiplayer overhaul to reduce desyncs and improve stability
- Native Linux support
- Support for Macos, including native Apple Silicon builds

### Since last release

The new bots got a lot tougher! Update and see if you can still keep up.

The computer opponent has been rebuilt from the ground up. In our test games it now earns seven times as much in the first 60 days as it did in 1.9.0 and ends with a fleet of around 70+ planes instead of 10+.

What you'll notice:

* It expands fast. It opens routes early, borrows to grow and buys several planes a day once the money flows.
* It runs a tight airline. Smarter flight planning, better ticket prices and advertising that is bought when it pays off.
* It plans ahead. It hires crews before it needs them, so it doesn't stall when its fleet explodes late in the game.
* It plays dirty when allowed. On the harder levels it sabotages and it will take you over if you leave the door open.
* It still plays fair. No cheating: Same rules, acts only on same information available to you.

**If 1.9.0's bot felt like a sparring partner, this one is a rival. Good luck.**

## License

The code in the repository is licensed under the terms included in the GOG release. As such the code can only be used for non-commercial purposes and remains property of BFG.

It is therefore *not* open-source in the free software sense, for more information refer to the License.txt.

## Installation

1. Buy and install a version of the game (for example on [GOG.com](https://www.gog.com/en/game/airline_tycoon_deluxe) or on [Steam](https://store.steampowered.com/app/331920/Airline_Tycoon_Deluxe/))
2. Download the latest release of this mod for your platform (Windows, macOS or Linux)
3. Unpack the archive into the folder where the old game executable is (for example At.exe on Windows)
4. Start the game using the new game executable (AT.exe on Windows, AT on macOS and Linux)
5. Enjoy!

## Building

This is for the experts who want to contribute to development.

Check out the repository as usual. Before building, remember to clone the submodules:
```
git submodule update --init
```

You can see the instructions to build and run the project in [BUILDING.md](BUILDING.md).

## All changes

### General
* Now runnable on Linux (including support for FLC animations found in some versions)
* Can run on MacOS as well
* Game can be configured for windowed / borderless / fullscreen display
* Language can be selected in the options menu (Misc page)

### New computer player

When starting a new game, you can pick a play style for each computer-controlled airline. I tried to make each one feel different to play against:

* **Classic**: The original computer player you know from the base game. It is cheating (see the table below), but it is not a particularly clever manager. Pick it for the nostalgia.
* **Laid-Back**: The nicest of the new competitors. It plays by the rules and builds a proper airline, but it strolls through the airport instead of running, is not picky about which jobs it takes, charges modest ticket prices and does not bother with buying kerosene in bulk. A good first opponent if you want to learn the game or just have a relaxed round.
* **Freight Baron**: A bit tougher competitor with a passion for cargo. It heads for the freight office first and fills its fleet with freight jobs. A fair opponent with a noticeably different rhythm than the others.
* **Route Tycoon**: A pure profit machine, fair but relentless. It never uses sabotage. Instead it starts building a huge route network almost immediately, trades kerosene cleverly and keeps an eye on the stock market, where it will happily buy up a struggling rival. Keep hold of your own shares if you want to see the end of the game.
* **Challenger**: The best strategist. Picks only lucrative jobs to then bootstrap a route network, pushes ticket prices as far as the passengers tolerate and stocks up on cheap kerosene. Compared to the *Route Tycoon*, it expands its route network slower and will always fully operate the most profitable routes. It never plays dirty, so expect a tough but honest race for the top spot.
* **Saboteur**: Runs its airline just as well as the *Challenger*, but is also a regular customer of the saboteur. Expect trouble with your aeroplanes and your office whenever you pull ahead, so a visit to the security office is money well spent.
* **Nemesis**: Everything the *Saboteur* does, and it is personal. The Nemesis singles out the human player with extra dirty tricks that no other level uses. Pick it if you want an opponent who is out to ruin your day, not just to win.

All new computer players have one thing in common:

**They always respect the rules of the game**

| Rules | Old computer player | New computer players |
| -------- | ------- | ------- |
| Plane constraints | ❌ Ignores maximum range | ✅ Respects maximum range |
| | ❌ Ignores number of seats | ✅ Respects number of seats |
| | ✅ Respects freight capacity | ✅ Respects freight capacity |
| Charter flights | ❌ Ignores date constraints | ✅ Respects date constraints |
| | ❌ Does not pay fine | ✅ Pays fine if not executed correctly |
| Hidden bonus | ❌ Up to 10% higher premium for charter flights | ✅ No hidden bonus |
| | ❌ Up to 25% bonus when competing for routes | ✅ No hidden bonus |
| | ❌ Small bonus to image every day | ✅ No hidden bonus |
| Employees | ❌ Completely fakes airline employees | ✅ Needs to hire employees |
| Laptop virus | ❌ Removed next day | ✅ Has to act himself |
| Strike | ❌ Will end randomly | ✅ Has to act himself |
| Illness | ❌ No action taken | ✅ Uses medicine |
| Missions | ❌ Uses various cheats | ✅ No cheating |
| Info | ❌ Accesses game state directly | ✅ Only acts on information that would be available to human player |

### Statistics screen
* Showing far more categories where money was spent
   * For example income from freight jobs, total tons transported, money spent on planes, sabotage or stocks and money gained from interest, credit or stocks
* Accurate summation of money spent
   * Fixed many bugs where especially money earned / spent by competitor would not show up in balance
* Whether or not values are shown depends on skill of financial advisor (for your airline) or skill of spy (regarding competitors)
* Unlimited statistics: Store statistics data for each day since beginning of the game
* Fix rendering of graph when zooming out
* Fix display of mission target

### Information menu
* Much more information on balance sheet depending on skill of your financial advisor
* New financial summary for quick assessment of the financial health of your airline (e.g. operating profit)
* Multiple balance sheets (for previous day / week / overall)
* More information from spy (e.g. weekly balance and financial summary for each competitor)
* Detailed information from kerosene advisor (quality / value of kerosene, money saved by using tanks)

### Option menu
* Game Speed is adjustable in options menu. Available values: 1, 5, 10, 15, 20, 25, 30 (default value). The hosts game speed is synced to clients in a network game.

### Keyboard navigation
* Allow Enter/Backspace in calculator
* Enable keyboard navigation in Laptop / Globe (arrow keys)
* Enable keyboard navigation in HR folder
    * Flip pages using arrow keys
    * Jump 10/100 pages in HR files using Shift/Ctrl
    * Change salary using +/-
    * Hire/fire using Enter/Backspace
* Enable keyboard navigation in plane prop menu (arrow keys, jump using Shift/Ctrl)
* Adjust route ticket prices in larger steps (using Shift/Ctrl)
* Arrow key navigation for many different menus

### Employees
* About twice as many advisors and flight attendants are up for hire (#26).
* Slightly increase competence of randomly generated employees
* Generate more and also randomized advisors
* Advisors are now drawn per type with weights depending on usefulness
* Regenerate unemployed employees if not hired within 7 days (prevents buildup of low-skill candidates in long games)
* List automatically sorted by skill
* Update worker happiness based on salary
    * Chance to increase/decrease happiness each day based on how much salary is higher/lower than original salary
* The 10% change when increasing/decreasing salary now always refers to the original salary
* Regularly increase worker happiness if company image is great

### Planes
* Reduce (~ half) cost of plane security upgrades
* Game now supports modded plane types with build years after 1998 (#13).
    * Plane build years now count from 2026 instead of 2002
    * Old saves are compatible, plane build years are shifted on load
* First-class seating changes now take effect when the plane is on the ground.
* Plane maintenance costs reset daily. Old value showed total cost per plane which makes it impossible to compare repair costs unless planes where bought on the exact same day.

### Kerosene
* Adjust impact of bad kerosene:
    * Now depends on ratio of bad kerosene in tank (quadratic function now instead of yes/no)
    * Amount of plane damage due to bad kerosene increased
    * Reasoning: Before, it was very easy to save enormous amounts of money by buying 'bad' kerosene. Now, it is still possible to save money, but you will need to carefully consider how much 'bad' kerosene you put in your tanks (between 10% and 20% can work).
* Kerosene advisor gives hints on how to save money in new kerosene advisor report
* ArabAir offers much larger kerosene tanks
* Do not remember selected kerosene quality for auto purchase (was an undocumented and convoluted 'feature')

### Music
* New sound options (OGG/MIDI)
* Patched stuttering glitch when switching music on Windows 11
* All 16 original MIDI tracks are now played; before it was 8 (#37).
* Add your own music: Game will play all OGG and all MIDI files found in the game directory

### Multiplayer
* Massive sync overhaul covering staff, salaries, strikes, items, kerosene, flight plans, gate/city bids, plane problems, the game clock and bankruptcy.
* Hardening against malformed messages, a use-after-free fix and several crash fixes.
* The game calendar uses UTC in network games.
* The game version is now checked when rejoining a saved game.
    * Preview builds can no longer join games running the release build, because peers now also compare a network protocol version.
* The ENET connection type was removed because it didn't work.

### Bug fixes
* Fixed frozen windows on laptop
* Integer overflow fixed when emitting lots of stock (resulted in loosing money when emitting)
* Fixed formula for credit limit
* Stock trading: Show correct new account balance in form (including fee)
* Fix saving/reloading of plane equipment configuration
* Fix bug in gate planning ('no gate' warning despite plenty of free gates available)
* Fix distant rendering of sticky notes in the boss office
* Use correct security measure to protect against route stealing
* Fix calculation of passenger happiness
    * Set passenger rating based on quality + small randomized delta (old code could yield just 'okay' even with high quality)
    * Passengers will tolerate high prices if quality is good
* Fix sabotage that puts leaflets into opponent's plane
    * Now passenger happiness is booked to the statistic of the sabotaging player
* 'Plane crash' sabotage now also affects stock price of victim
* Fix calculation of plane repair cost
    * All cost will show up in plane's balance
    * All cost will show up in plane repair cost total
    * Correctly calculate plane's balance over past 7 days
* Consider also number of first class passengers for statistics
* Do not show route utilization for defeated players
* Patched Space station mission prices and texts in stats
* Patched various text scrambling on UI
* Bug fixed in calculation in maximum amount of stock that can be emitted
    * Bug limited max amount of stock to around 2.1 million
    * Integer overflow is fixed now, but the originally intended limit of 250 million was changed to 2.5 million
* Fixed game shifting flights on its own even if they are already locked
    * Could previously cause double-booking of flights (income and cost booked twice)
* Fixed crash in plane designer when attaching engines to left side of tail
* Timeout for sabotage: Jobs canceled if it was not possible to execute job for some time
    * Can happen if selected plane is not used anymore by owner
    * Without this fix in a situation like this the player would never be able to use sabotage again
* Fixed bug where player can 'survive' being overtaken by skipping dialog at the right moment
* Classic mission 04 now uses correct route utilization
    * Previously, even though boss said that routes must be 20% utilized, game would check for 20% plane utilization
* Addon mission 05: Formula for luxury score changed
* Addon mission 09: Uhrig flights are now all in the region of the home airport
* Addon mission 09: Fixed counting of Uhrig flights
    * Note that computer players always have and still are cheating in this mission
* First class mission 07: Only need to have two repaired planes, not all of them in case more than two were bought
* Evolution mission 02: Only need to have five planes with full safety upgrades, not all of them in case more than five were bought
* Fixed crash when quickly clicking through dialog options
* Stocks are traded in chunks of 2000 with the price updated after each chunk (exploit fix)
* Food costs now include first-class passengers
* Dragging a flight in the flight planner could make it start before its plane arrived (#44).
* Fixed many random crashes

### Default computer player
* Max. range of a plane now also has to be considered by the computer player
* Max. number of seats now also has to be considered by the computer player
* Uses now same credit limit
* Uses now same rules for trading stock
    * Trading fee (100 + 10% of volume) now also for computer players (fee existed only for player)
    * Do not execute trades in steps of 1000 (this previously made stock prices worse for the human player only)
    * Align function to re-calculate stock price after trade
* Uses now same rules for emitting stock
* Remove sabotage advantages
    * Computer now has to pay for sabotage as well
    * Consider all security measures (e.g. plane crash not possible anymore if plane is protected)
    * Align calculation of ArabAir trust for player and computer
* Remove strange reduction of flight cost in calculation of image change (was a disadvantage for computer player)
* Computer player pays real price for plane upgrades
* Fixed bug that prevented computer players from using routes in most games
    * Computer players will switch to routes in most games eventually
    * Computer players however will use a small cheat that regularly improves their image
* Fixed bug where computer player buys or sells more stock than available

### Misc
* Spy reports enemy activity based on skill
* ArabAir opens one hour earlier
* Calculate route utilization as average of previous 7 days
* Adding NOTFAIR cheat to make competitors much richer
* Adding ODDONEOUT cheat to improve image of competitors
* Adding AUTORUN cheat for ultra-fast forward
* Use player colors when showing routes on laptop
* Buy kerosene by clicking price chart
* Change tooltip of savegames (number of days played)
* Implement actual random generator using Mersenne twister
* Company value includes value of kerosene stored in tanks and tanks themselves
* Company value includes value of plane upgrades
* Company value includes value of airline image (money required to reach current image)
* Strikes will start after 9 am now to give player chance to react
* Make planes in main menu comically long
* Decryption of data files with the run argument "/savedata"
* Option "OptionTicketPriceIncrement" to increase ticket price increment per click
* Director's board now allow for more post-it
* Added options "OptionRentOffice*" to customize the branch number available / day.
* Director's board post-it system improved and allow for more cities (up to 7)
* Safety upgrades now protect against the intended problem
    * Spring: Cockpit upgrade protect against "Migrating birds, due to the cockpit"
    * Summer: Safety upgrade protect against "Missing life vests"
    * Fall: Engine upgrade protect against "Engine damage, due to foliage"
    * Winter: Tire upgrade protect against "Landing gear frozen solid"

## Credits and Contributors

- [WizzardMaker](https://github.com/WizzardMaker) - Main contributor and maintainer
- [CrossVR](https://github.com/CrossVR) - Original contributor
- [mertenpopp](https://github.com/mertenpopp) - Contributor
- [wackfx](https://github.com/wackfx) - Contributor
- [CrazyOrange](https://github.com/CrazyOrange) - Contributor
- [Heftie](https://github.com/Heftie) - Contributor

