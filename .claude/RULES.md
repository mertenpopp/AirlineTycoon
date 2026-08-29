The Game
========

Airline Tycoon Deluxe is a resource-management/tycoon game where you control the airline manager during the day. You walk in the airport from room to room and talk to different people in different rooms to perform specific actions. You buy planes, upgrade them, hire staff, take flight jobs and schedule flights. Flights bring you money which you can use to buy more planes.

The game simulates a day/night cycle. You work from 9 am to 18 pm and can only perform actions during this time. The game state however advanced during night: Planes start and land as scheduled, money made or lost as usual. You work the entire week including on Sunday.

There are always four competing airlines:
- "Sunshine Airways" (abbreviation: SA, array index: 0)
- "Falcon Lines" (abbreviation: FL, array index: 1)
- "Phoenix Travel" (abbreviation: PT, array index: 2)
- "Honey Airlines" (abbreviation: HA, array index: 3)

ClaudeBot will usually play as "HoneyAirlines" but shall work playing as any airline. The existing scaffolding in ClaudeBot.cpp has a reference called `qPlayer` to the correct `PLAYER` instance. The airline enumeration is found at `qPlayer.AirlineNum`.

Technical details:
- The test script uses a command line argument ("/quick") which is evaluted in `Takeoff.cpp` and sets bot difficulty in `PLAYER::BotLevel`. The command line argument can be a three digit number. The first digit will set `PLAYER::BotLevel` for the first non-human player and so on. A `BotLevel > 0` indicates that this player is controlled by ClaudeBot.
- The human player (which always must exist for technical reasons) will be determined by the value `OptionLastPlayer` in the `AT.json` game settings in the game directory. Otherwise, it will be determined by `Sim.Options.OptionLastPlayer`.

Testing the game
----------------

Test the game **only** by running the scripts in the `scripts/` folder (`./scripts/run_build.sh`, `./scripts/run_test.sh`, `./scripts/run_smoketest.sh`, `./scripts/run_measurement.sh`). Do not invoke the game binary directly and do not set, export or override any environment variables for a test run (for example `SDL_VIDEODRIVER`).

Reason: in a previous session the video driver was switched to `offscreen`, which caused extreme GPU load on the test machine. The scripts contain the configuration that is known to be safe.

If these rules keep you from doing what a task requires, do not work around them: stop and ask the user to change the test harness.

Target
------

The goal is to maximize the airline's cumulative "operative saldo" which can be queried with `qPlayer.BilanzGesamt.GetOpSaldo()`.

"Operative" means that the saldo only includes money gained from airline operations (flight jobs and routes, both passenger and freight). From the gains, the money spent to facilitate these flights are deducted: 
- Kerosene
- Airline food
- Fines if flights were not executed correctly
- Repair cost (scheduled maintenance and unexpected malfunctions)
- Refitting costs when a plane has to switch from carrying passengers to freight or vice versa
- personal cost
- rent for airport gates
- rent for branch offices in other cities
- rent for routes
- cost for security office

Game over
---------

The game is lost when the airline goes bankrupt. This happens when the money on the account is less than the value given in `DEBT_GAMEOVER` (default: negative 5 million). The game checks this at 9 am before the player has a chance for corrective actions. Note that the cost for plane repairs are deducted at midnight.

Corrective actions to prevent bankruptcy might include the following. Treat these only as suggestions:
- sell shares from other airlines
- sell own shares
- take new loan
- emit new shares
- reduce amount pre-allocated for repairs
- reduce amount pre-allocated for upgrades

Savegames
---------

You are free to add as many member variables in sub-classes as you like to the `ClaudeBot` class. However, you need to ensure that upon saving, the full state of ClaudeBot is saved to disk and restored on savegame load.

The `ClaudeBot` class already has the two friend functions:
- `TEAKFILE &operator<<(TEAKFILE &File, const ClaudeBot &bot)`: Saves the state of the bot to a file
- `TEAKFILE &operator>>(TEAKFILE &File, ClaudeBot &bot)`: Restores the state of the bot from a file

Remember to always update these two functions when you add a new data member to the `ClaudeBot` class.

Game actions
============

Note that most actions have to performed in a particular room. ClaudeBot shall set the desired action ID in `RobotPlan()` and the game will walk the character to the correct room. On arrival, `RobotExecuteAction()` will be called.

Use `RobotPlan()` to determine what shall be done next. In this function, a primary and also a secondary action ID shall be set. The character will attempt to enter the primary room first. If the room is already occupied by a competitor or closed, the secondary ID will used to walk the character to a different room.

Use `RobotExecuteAction()` to actually perform a planned action. It shall be checked which action ID (primary or secondary) was successful by checking `qPlayer.RobotActions[0]`.

ClaudeBot shall check that it is in the correct room by using the function: `qPlayer.GetRoom()`. If it is not the correct room, only print a warning for now and do still perform the planned action.

Note that some rooms open and close at a specific time. Opening hours also depend on the day of the week:
- ClaudeBot shall use the following function to check if the room is open: `bool checkRoomOpen(SLONG actionId)`
- ClaudeBot shall use the following to translate an action ID to a room ID: `SLONG getRoomFromAction(SLONG PlayerNum, SLONG actionId)`
- When planning the next action, consider the time it requires to walk to a room

We will list now all actions that can be performed in the game via the class `GameMechanic`.
If `GameMechanic` returns a bool this usually means whether or not the action could be completed.

In the following, qPlayer always is a reference to the instance of the Player class which refers to the player controlled by ClaudeBot.

Bank actions
------------

Bank actions are all performed in the bank room. All action IDs listed in this section lead to this room.

### Take out loans

`bool GameMechanic::takeOutCredit(qPlayer, amountToRaise)`: Take out a loan. Call qPlayer.CalcCreditLimit() to find out how much money can be raised. Total amount of money owed is stored in qPlayer.Credit. Note that this is a loan and interest has to be paid.

Recommended action ID: ACTION_RAISEMONEY

### Pay back loan

`bool GameMechanic::payBackCredit(qPlayer, amountToPayBack)`: Pay back a part of your loan. Total amount owed is stored in qPlayer.Credit.

Recommended action ID: ACTION_DROPMONEY

### Emit stock

`bool GameMechanic::emitStock(PLAYER &qPlayer, SLONG neueAktien, SLONG mode)`: Issue new shares. Gives the airline some money however, reduces stock price and more float means competitors could take you over. Analyze the code of this function to see how the mode affects how much money is made and by how much the stock price drops.

Recommended action ID: ACTION_EMITSHARES

### Set dividend

`bool GameMechanic::setDividend(PLAYER &qPlayer, SLONG dividend)`: Set the dividend for the airline’s stock. Higher dividend costs more money but improves stock price. Note that increases in dividend have a delay before they take effect. Decreases immediately have a (negative) effect.

Recommended action ID: ACTION_SET_DIVIDEND

### Buy stock

`std::pair<bool, __int64> GameMechanic::buyStock(PLAYER &qPlayer, SLONG airlineNum, SLONG amount, bool commit)`: Purchase shares in another airline identified by airlineNum. Analyze the code to see how high the bank fee is. If commit == false, no action is made. The second return value gives the total amount of money that will be spent. Note that stock price will increase.

Recommended action ID: ACTION_BUYSHARES

### Sell stock

`std::pair<bool, __int64> GameMechanic::sellStock(PLAYER &qPlayer, SLONG airlineNum, SLONG amount, bool commit)`: Sell shares held in another airline identified by airlineNum. Analyze the code to see how high the bank fee is. If commit == false, no action is made. The second return value gives the total amount of money that will be gained. Note that stock price will decrease.

Recommended action ID: ACTION_SELLSHARES

### Overtake airline

`bool GameMechanic::overtakeAirline(PLAYER &qPlayer, SLONG targetAirline, bool liquidate)`: Action for trying to take over another airline via stock acquisition. Airline is taken over with all planes, routes, money and debt. Parameter `liquidate` can be used to erase airline completely instead.

The function `canOvertakeAirline()` checks whether the target is valid, whether you have enough stock (>= 50%), and whether the enemy blocks acquisition by owning stock from your airline (>= 30%). Note that your competitors can also overtake you when they meet the respective conditions.

Recommended action ID: ACTION_OVERTAKE_AIRLINE

### Other

Action ID ACTION_VISITBANK can be used for a generic action for bank interactions.


Flight jobs / freight jobs actions
----------------------------------

These actions send the bot to the special offices where flight jobs can be viewed and taken.

### Last minute jobs

Use the action ID ACTION_CHECKAGENT1 to visit the last minute agency room.

Only while performing this action, the global array `LastMinuteAuftraege` may be accessed. Never write to this array.
Browse this array to find suitable flights. All flights here can go from any city to any other city and typically need to be completed either today or tomorrow. Plane must have required number of seats at least. Plane needs to be able to travel the required distance. The money for the job is paid if completed on time. If flight has been taken but not completed on time, a fine has to be paid. Some jobs have a fine of zero.
You can pick a flight job using:

`bool GameMechanic::takeLastMinuteJob(PLAYER &qPlayer, SLONG jobId, SLONG &outObjectId)`: Pick a flight job from `LastMinuteAuftraege`. Job will be added to qPlayer.Auftraege and can be found using outObjectId.

### Travel agency jobs

Use the action ID ACTION_CHECKAGENT2 to visit the travel agency room.

Only while performing this action, the global array `ReisebueroAuftraege` may be accessed. Never write to this array. Browse this array to find suitable flights. All flights here will connect the home airport with any other city. Plane must have required number of seats at least. Plane needs to be able to travel the required distance. There is a premium if completed on time. If flight has been taken but not completed on time, a fine has to be paid. Some jobs have a fine of zero.

`bool GameMechanic::takeFlightJob(PLAYER &qPlayer, SLONG jobId, SLONG &outObjectId)`: Pick a flight job from `ReisebueroAuftraege`. Job will be added to qPlayer.Auftraege and can be found using outObjectId.

### Freight jobs

Use the action ID ACTION_CHECKAGENT3 to visit the freight depot.

Only while performing this action, the global array `gFrachten` may be accessed. Never write to this array.
Browse this array to find suitable flights. All flights here can go from any city to any other city. Total freight volume can be transported via multiple trips and/or planes. Plane needs to be able to travel the required distance. There is a premium if total freight volume was transported on time, otherwise, a fine has to be paid. Some jobs have a fine of zero.
You can pick a freight job using:

`bool GameMechanic::takeFreightJob(PLAYER &qPlayer, SLONG jobId, SLONG &outObjectId):` Pick a flight job from `gFrachten`. Job will be added to qPlayer.Frachten and can be found using outObjectId.

### International jobs (passenger flights)

Each airline can purchase offices in other cities. They grant access to additional international flight jobs.

You can pick up international flight jobs in your personal office. Recommended action ID: ACTION_CALL_INTERNATIONAL

You can call any number of your international offices to take passenger flight jobs. They all either start or land in the city you are calling. Otherwise, the same rules as for the jobs picked up by ACTION_CHECKAGENT2 apply.

`bool GameMechanic::canCallInternational(PLAYER &qPlayer, SLONG cityId)`: Use this function to check if a specific city can be called.

Only while performing this action and after `canCallInternational()` was checked with the cityId, the global array `AuslandsAuftraege[cityId]` may be accessed. Never write to this array.

`bool GameMechanic::takeInternationalFlightJob(PLAYER &qPlayer, SLONG cityId, SLONG jobId, SLONG &outObjectId)`: Pick a flight job from `AuslandsAuftraege[cityId]`. Job will be added to qPlayer.Auftraege and can be found using outObjectId.

### International jobs (freight jobs)

The airline offices in other cities also grant access to additional freight jobs.

You can pick up international freight jobs in your personal office. Recommended action ID: ACTION_CALL_INTERNATIONAL

You can call any number of your international offices to take freight flight jobs. They all either start or land in the city you are calling. Otherwise, the same rules as for the jobs picked up by ACTION_CHECKAGENT3 apply.

`bool GameMechanic::canCallInternational(PLAYER &qPlayer, SLONG cityId)`: Use this function to check if a specific city can be called.

Only while performing this action and after `canCallInternational()` was checked with the cityId, the global array `AuslandsFrachten[cityId]` may be accessed. Never write to this array.

`bool GameMechanic::takeInternationalFreightJob(PLAYER &qPlayer, SLONG cityId, SLONG jobId, SLONG &outObjectId)`: Pick a flight job from `AuslandsFrachten[cityId]`. Job will be added to qPlayer.Frachten and can be found using outObjectId.

### Call via mobile phone

ACTION_CALL_INTER_HANDY can be used for a special “no-walk” international-call action; it does not require a location walk and is thus faster.
The item "phone" is required. Everything else said about ACTION_CALL_INTERNATIONAL also applies here.

Every time the phone is used, set the existing variable `mOnThePhone` to 30.

### General rules

A passenger flight job is an instance of `CAuftrag`, a freight job is an instance of `CFracht`. In the global arrays, for any given job always check for validity using the expression `(job.VonCity != job.NachCity) && (job.Praemie >= 0)`.

Flight planning
----------------

### Important data structures

Familiarize yourself with the data structures:
- CAuftrag
- CFracht
- CRoute
- CFlugplanEintrag
- CPlane

A list of planes is found in `qPlayer.Planes`. Each plane has a flight plan object `Flugplan`.  Each flight plan object has a chronologically sorted list of flights in the array `Flug`.
The items in this array have the type `CFlugplanEintrag ` and are only referring to an actual flight if their `ObjectType` is larger than 0. This is the definition for `ObjectType`:
- 1 means route job
- 2 means passenger job
- 3 is an automatic flight
- 4 means freight job

A flight plan object cannot be altered if the plane has already started or the start happens until the next in-game hour. Check using this expression: `qFPE.Startdate == Sim.Date && qFPE.Startzeit <= Sim.GetHour() + 1` where `qFPE` is a reference to a flight plan object. If this is the case, the entry is considered to be locked.

You can use the following helper functions:
- class `PlaneTime` stores both a date and a time and provides proper operator overloading for adding/subtracting time and comparisons
- `const CFlugplanEintrag *getLastFlight(const CPlane &qPlane)`: Returns a pointer to the last valid flight plan object
- `const CFlugplanEintrag *getLastFlightNotAfter(const CPlane &qPlane, PlaneTime ignoreFrom)`: Returns a pointer to the last valid flight plan object when flights after a certain time are ignored. This is useful when the intention is to replan a flight schedule but you do not want to touch flights that are scheduled for takeoff very soon.
- `std::pair<PlaneTime, int> getPlaneAvailableTimeLoc(const CPlane &qPlane, std::optional<PlaneTime> ignoreFrom, std::optional<PlaneTime> earliest)`: Return both time and location when the plane is available, meaning it has landed and is available for the next flight. As above, there is an option for a cutoff if the intention is to replan. The earliest returned time will be the next full hour or the optional argument `earliest` if it contains a later point in time.

### How to plan flights

Flight jobs that have been taken shall be planned. If not, they will expire and this might incur a fine.

Flights can only be planned in the player's office or when the item "laptop" is available.
To walk to your office, use the action ID ACTION_BUERO. Note that the office is only usuable when `(qPlayer.OfficeState != 2)`.
The laptop can be used at any point during any action as long as the condition `qPlayer.HasItem(ITEM_LAPTOP) && (qPlayer.LaptopVirus == 0)` holds (laptop available and no virus).

Do not modify anything in the plane, flight plan or flight plan object classes directly. Instead, use the following functions:
- `bool GameMechanic::killFlightJob(PLAYER &qPlayer, SLONG par1, bool payFine)`: Remove a passenger flight from the backlog and pay fine immediately. Note that it is assumed the flight has been removed from the plane schedule already.
- `bool GameMechanic::killFreightJob(PLAYER &qPlayer, SLONG par1, bool payFine)`: Remove a freight job from the backlog and pay fine immediately. Note that it is assumed the flight has been removed from the plane schedule already.
- `bool GameMechanic::removeFromFlightPlan(PLAYER &qPlayer, SLONG planeId, SLONG idx)`: Remove a flight plan entry from the plane identified by planeId at position idx in the plan.
- `bool GameMechanic::clearFlightPlan(PLAYER &qPlayer, SLONG planeId)`: Remove all flight plan entries from the plane identified by planeId which are not yet locked.
- `bool GameMechanic::clearFlightPlanFrom(PLAYER &qPlayer, SLONG planeId, SLONG date, SLONG hours)`: Remove all flight plan entries from the plane identified by planeId after the given time.
- `bool GameMechanic::planFlightJob(PLAYER &qPlayer, SLONG planeID, SLONG objectID, SLONG date, SLONG time)`: Attempts to place the given flight (passenger job) for the given plane and time into the plane's flight schedule.
- `bool GameMechanic::planFreightJob(PLAYER &qPlayer, SLONG planeID, SLONG objectID, SLONG date, SLONG time)`: Attempts to place the given flight (freight job) for the given plane and time into the plane's flight schedule.
- `bool GameMechanic::planRouteJob(PLAYER &qPlayer, SLONG planeID, SLONG objectID, SLONG date, SLONG time)`: Attempts to place the given flight (route job) for the given plane and time into the plane's flight schedule.

### Constraints

There are constraints when scheduling flights. In the following, `qPlane` is a reference to the plane that shall fly this job:
- earliest possible start time is `(Sim.GetHour() + 2) % 24`
- latest possible start day is `Sim.Date + 6`
- number of passengers must be ` <= qPlane.ptPassagiere` for passenger flight jobs
- freight jobs can split the total freight volume (`CFracht::Tons`) across multiple trips and/or planes. `CFracht::TonsLeft` tracks the number of tons still left
- distance between start city (`VonCity`) and target city (`NachCity`) must be ` <= qPlane.ptReichweite * 1000`
- flight duration must not exceed 24 hours (relevant for long distance flights and slow planes)

`void BotHelper::calcCostAndDuration(int startCity, int destCity, const CPlane &qPlane, bool emptyFlight, int &cost, int &duration, int &distance)`: You can use this helper functions to determine cost, duration and distance of a flight. The parameter `emptyFlight` has to be set if it is an automatic flight (game assumes a little bit of income from these which reduces cost). Note that this function only includes cost for passenger, freight and route jobs. This function does access the current kerosene price `Sim.Kerosin` which normally is only available while at the Arab. This is permitted as the human player will also see the cost of a flight in the game.

Hint: A good place to learn more about the rules of flight jobs is the function `CFlugplanEintrag::BookFlight`. This function trigers all the effects of a flight: Money gained for job, money spent for fuel and passenger food, plane deterioration, changes to company image and much more.

Hint: It is a good idea to regularly check if flight plan entries are scheduled correctly by reading the field `CFlugplanEintrag::Okay`. Rather surprising, the field is set to `0` if everything is fine. If a constraint is violated, an error code larger zero is set. Check the functions `PLAYER::UpdateAuftragsUsage()` and `PLAYER::UpdateFrachtauftragsUsage()` to learn more.

### Automatic flights

A plane is either in the home airport or in the city denoted by `NachCity` of the most recently performed flight. However, for the following scheduled flight, the plane needs to be at the city denoted by `VonCity` by the time the flight is scheduled. To facilitate this, the game automatically inserts "automatic flights" to bring the plane from the previous `NachCity` to the next `VonCity`. These flights have their own `CFlugplanEintrag` with `ObjectType == 3`. They gain little money and usually cost more than they bring.

ClaudeBot never has to add or remove them itself. The game adds them where necessary and also removes them if possible. However, ClaudeBot needs to be aware that these flights take time and usually cost money.

These flights are sometimes also called "empty flights" or "Leerflug".

There are no automatic flights inserted if previous `NachCity` is identical to next `VonCity`.

Flight planning mechanics
-------------------------

If a new flight is added to a (partially) filled plane schedule at a specific point, the game might need to shift the earlier or later flights to make enough room in the schedule. This behavior is compounded by the fact that the automatic flights change. Example:

- Passenger job: Berlin -> London
- Automatic flight: London -> Frankfurt
- Freight job: Frankfurt -> Delhi

If the job Dublin -> Brussels is inserted in the middle, we get:

- Passenger job: Berlin -> London
- Automatic flight: London -> Dublin
- Passenger job: Dublin -> Brussels
- Automatic flight: Brussels -> Frankfurt
- Freight job: Frankfurt -> Delhi

Consider the change from one automatic flight to two new, different automatic flights.

Because this behavior is difficult to predict, my recommendation is:
- First plan how the ideal flight schedule for a given plane shall look like
- Consider automatic flights from the start
- Verify the ideal schedule and check all the constraints
- Only then apply the schedule by following the next steps
- (Partially) Clear the plane schedule
- Add the flight jobs one by one at the calculated point in time
- Do not attempt to schedule automatic flights manually, this is not possible
- In the end, verify that all flight jobs ended up at the intended position in the schedule

### Real-time concerns

Flights can be scheduled and plane schedules can be altered at any point in time when the player has a laptop available and it has no virus. These action can also be done in the player's office unless the office is currently unusable. If both office and laptop are unusable, no flights can be scheduled and no plane schedule can be altered.

Before a laptop is bought there is no legal way to schedule a flight directly after picking it up at one of the agencies. The player has to walk to the office in order to schedule it and time will pass. This can make it impossible to schedule a flight that was supposed to start very soon. Plan accordingly if walking is required.

Hint: This is mainly a restriction for early game. As soon as the player has laptop and antivirus items, this is usually not a concern anymore. It is probably enough to use a "need to check and update flight plans" marker for early game.

### Helper functions

You can use the following helper functions and are also allowed to rewrite them for your convenience:

- `SLONG BotHelper::checkPlaneSchedule(...)`: Checks the current schedule of the specified plane for mistakes, prints every mistake to the log and returns total number of mistakes.
- `ScheduleInfo BotHelper::calculateScheduleInfo(...)`: Returns a struct with lots of useful information about the current flight schedule of the given plane.
- `SLONG BotHelper::checkFlightJobs(...) `: Performs the check for all planes and also prints combined statistics to the log. Use this function every time after you made a change to a plane's flight schedule.

Routes
------

Routes have to be rented once and then as many flights on this route as desired can be added to any number of planes. Routes always connect exactly two cities, called `VonCity` and `NachCity` in the game code.

Familiarize yourself with the data structures:
- CRoute
- CRentRoute

The global array `Routen` contains all available routes and may only be accessed while in the "route box" room.

The array `qPlayer.RentRouten` has one instance of `CRentRoute` for each instance of `CRoute` in the global array at the same index. `CRentRoute` contains information regarding the player for the corresponding route, for example, if the player has rented this routes (`CRentRoute::Rang != 0`). This array may only be accessed while in the player's office or while having a functioning laptop.

### Route box

The action ID ACTION_VISITROUTEBOX or ACTION_VISITROUTEBOX2 shall be used to walk to the route box. All three functions in this section can only be done at the route box.

`bool GameMechanic::rentRoute(PLAYER &qPlayer, SLONG routeA)`: Rents a new route at the "route box" room. A daily fee has to be paid for each rented route.

`BUFFER_V<BOOL> GameMechanic::getBuyableRoutes(PLAYER &qPlayer)`: Check which routes are buyable. It returns an array with a boolean for each route at the corresponding index. A route always connects two cities. A route is buyable if either city is the home airport or either city is already connected by a different route that the player flies sufficiently enough. This can checked via `qPlayer.RentRouten.RentRouten[c].RoutenAuslastung  >= 20` where `c` is the index of any route which has either the same `VonCity` or same `NachCity` as the route that you want to rent.

`bool GameMechanic::killRoute(PLAYER &qPlayer, SLONG routeA)`: Stop renting the specified route. First ensure that no plane will be flying this route anymore.

### Route mechanics

When renting a route from city A to city B, one does automatically rent the route in reverse from B to A. The GameMechanic automatically rents or kills the route in reverse direction. 

`SLONG GameMechanic::findRouteInReverse(PLAYER &qPlayer, SLONG routeA)`: Find the routeId of the reverse direction for a given route.

`bool GameMechanic::setRouteTicketPrice(PLAYER &qPlayer, SLONG routeA, SLONG ticketpreis, SLONG ticketpreisFC)`: Sets the ticket price for one direction of the route. There is a regular ticket price and one for first class (FC).

`bool GameMechanic::setRouteTicketPriceBoth(PLAYER &qPlayer, SLONG routeA, SLONG ticketpreis, SLONG ticketpreisFC)`:  Sets the ticket price for both directions of the route. There is a regular ticket price and one for first class (FC).

These functions can be called while in the player's office or while having a functioning laptop.

Routes have a certain demand which crucially determines how many tickets can be sold per day at most which means that airlines renting the same route will be competing for passengers. The field `CRoute::Bedarf` gives how many passengers still want to fly today. Analyze the function `CRouten::NewDay()` to understand how the demand is renewed on every new day.

The money gained from a single route flight is calculated by the game in `CFlugplanEintrag::BookFlight`. The most important factors are the set ticket price and the number of passengers. As opposed to regular flight jobs, for route flights the number of passengers is calculated dynamically in `CFlugplanEintrag::CalcPassengers` with `(ObjectType == 1)` and depends on price, competitors flying this route, current demand on this route, time of flight, the airlines's image and the specific image for the route and other factors.

Hint: Of these factors, ticket price and route image are the easiest to adjust and have a big impact on passenger count. 

Routes have to be utilized by at least 10%. If a player servers less than 10% of the demand for 20 consecutive days, the route is taken away from the airline and cannot be rented again for several days. In rare cases, competitors can also use sabotage to steal a route from another airline. In any case, ClaudeBot must be resilient enough to detect that a route has been removed and act accordingly (e.g., schedule planes on other routes).

HR / staffing actions
------------------------------------

Familiarize yourself with the data structure:
- CWorker

Use the action ID ACTION_PERSONAL to go to the HR office. This is the staff management room. Only while in this room, the following functions may be called.

Only while in this room, the global array `Workers.Workers` may be accessed. Only the CWorker instances where `Employer` equals `WORKER_JOBLESS` (can be hired) or `qPlayer.PlayerNum` (already hired) may be read.

`bool GameMechanic::hireWorker(PLAYER &qPlayer, SLONG workerId)`: Hire the worker with the given ID.


`bool GameMechanic::fireWorker(PLAYER &qPlayer, SLONG workerId)`: Fire the worker with the given ID.


`void GameMechanic::increaseAllSalaries(PLAYER &qPlayer)`: Increases salary for all workers by 10%. Also ends any currently ongoing strikes.


`void GameMechanic::decreaseAllSalaries(PLAYER &qPlayer)`: Decreases salary for all workers by 10%.

`CWorker::Gehaltsaenderung(BOOL Art)`: Increases the salary for one worker by 10% (Art == true) or decreses by 10 % (Art == false).

Changes in salary affect worker happiness. There is a one-time effect and a chance of a regular increase/decrease in happiness if salary is above/below baseline salary.

ClaudeBot has to hire enough pilots and stewardesses. The required number of pilots depends on the plane type and is found in `CPlaneType::AnzPiloten`, required number of stewardesses in `CPlaneType::AnzBegleiter`. These same values are also found in the `CPlane` instance at `CPlane::ptAnzPiloten` and `CPlane::ptAnzBegleiter`.

Workers are paid daily, amount is `CWorker::Gehalt`/30. Worker talent (`CWorker::Talent`) affects customer satisfaction.

Workers might have quit over night. It should be checked regularly to see if there is a crew deficit. Assignment of crew to planes is done automatically. While it is possible to assign manually, we shall leave it to the automatic assignment and ClaudeBot only needs to ensure that enough people are hired in total.

Claude shall hire the following types of employees:
- pilots (CWorker::Typ == WORKER_PILOT)
- stewardesses (CWorker::Typ == WORKER_STEWARDESS)
- advisors (CWorker::Typ one of BERATERTYP_PERSONAL, BERATERTYP_KEROSIN, BERATERTYP_GELD, BERATERTYP_INFO, BERATERTYP_FLUGZEUG, BERATERTYP_FITNESS, BERATERTYP_SICHERHEIT)

For each type of advisor, only the one with the highest skill level (`CWorker::Talent`) is relevant. The following advisors, depending on `Talent`, gate access to important information:
- BERATERTYP_PERSONAL: Info regarding staff
- BERATERTYP_KEROSIN: Info regarding kerosene
- BERATERTYP_GELD: Info regarding money and saldos
- BERATERTYP_INFO: Info about competitors
- BERATERTYP_FLUGZEUG: Info about used planes

The advisor BERATERTYP_SICHERHEIT gives up to 10% discount depending on `Talent` discount on various purchases (for example planes).

The advisor BERATERTYP_FITNESS increases movement speed of the player character.

Office actions
--------------

Use the action ID ACTION_BUERO, ACTION_UPGRADE_PLANES or ACTION_CALL_INTERNATIONAL to go to the player’s personal office. Only while in this room, the following functions may be called.

The action ID ACTION_STARTDAY is the standard “begin day” room action and is automatically executed at the beginning of the day. Do not return this action ID from the `RobotPlan()` function.

### Open kerosine tanks

`bool GameMechanic::setKerosinTankOpen(PLAYER &qPlayer, BOOL open)`: Sets if the kerosine tanks are open or closed. Open means that planes are refueled from the tanks first as long as there is still kerosine in them.

### Upgrade planes

The action ID ACTION_UPGRADE_PLANES can be used to update planes in the office. While in the office, the following members of all `CPlane` instances may be written if the plane belongs to ClaudeBot:
- SitzeTarget
- TablettsTarget
- DecoTarget
- ReifenTarget
- TriebwerkTarget
- SicherheitTarget
- ElektronikTarget
- EssenTarget

Permitted values are 0, 1 and 2. These refer to the targeted upgrade levels. There are additional variables without the "Target" suffix which refer to the current upgrade levels. These may only be read.

`EssenTarget` determines the quality of passenger food. Higher value means higher cost per flight but also higher passenger satisfaction. Check `CFlugplanEintrag::BookFlight` to see the cost of better food.

All other upgrades are applied the next time the plane is on the ground. At this point, the money for the upgrade is deducted and the value of the target variable is copied to the other one (e.g., Triebwerk is set TriebwerkTarget). Attention: A common cause for airlines going bankrupt is because an ugprade is planned when money is available but at the time the upgrade is applied, the money has been spent elsewhere. While in the personal office, all target variables may be rewritten to cancel previously planned upgrades. You can use the function `PLAYER::CalcPlanePropSum` to calculate the cost of open plane upgrades.

Additionally, ClaudeBot may hire more stewardesses than necessary to improve service quality. The value `CPlane::AnzBegleiter` gives the current number of stewardesses, `CPlane::MaxBegleiter` gives the target value and may be written by ClaudeBot and `CPlane::ptAnzBegleiter` gives the minimum for the plane to be operational. The maximum number is twice the minimum amount. If the number of stewardesses hired is not sufficient, the game will try to meet the requirement minimum per plane first before attempting to reach a higher target amount.

Analyze the function `CFlugplanEintrag::BookFlight` (variable `Add`) to see how plane upgrades affect customer satisfation, airline and route image, if applicable.

For route flights, the number of first class seats is important. First class passengers will pay a higher ticket price but a seat for first class replaces two regular seats.

`bool increaseFirstClassRatio(PLAYER &qPlayer, SLONG planeId)`: Increases the amount of first class seats by 10%. Returns true if change was made.

`bool decreaseFirstClassRatio(PLAYER &qPlayer, SLONG planeId)`: Decreases the amount of first class seats by 10%. Returns true if change was made.

Kerosene actions
----------------

By default, kerosine is automatically bought for the regular market price. There is an opportunity to save money by buying kerosine manually and filling up the airline's tank.

If tanks are set to open and still full, the function `CFlugplanEintrag::BookFlight` deducts volume from the tank instead of buying for the market price.

The action IDs ACTION_BUY_KEROSIN, ACTION_BUY_KEROSIN_TANKS and ACTION_VISITARAB can be used to visit the Arab where tanks and kerosine can be bought. Only while in this room, the following functions may be called.

`bool GameMechanic::buyKerosin(PLAYER &qPlayer, SLONG type, SLONG amount)`: Buys kerosene for the tanks. Type is either 0, 1 or 2 and determines the quality (0="good", 1="normal", 2="bad") of the kerosene. Use the function `GameMechanic::calcKerosinPrice(PLAYER &qPlayer, __int64 type, __int64 amount)` to find out the price before buying.

`bool GameMechanic::buyKerosinTank(PLAYER &qPlayer, SLONG type, SLONG amount)`: Permanently increases the maximum tank capacity. Use `type` to select a tank size from global array `TankSize`. The price is found at the corresponding index in array `TankPrice`. Note that larger tanks are cheaper per volume unit. Multiple tanks can be bought at once using the parameter `amount`.

`SLONG SIM::HoleKerosinPreis(SLONG typ)`: Checks current market price for the different qualities of kerosene. Price for normal quality (`typ == 1`) is stored in `Sim.Kerosin` and may also be accessed directly. Both may be accessed while visiting the Arab and while in the personal office. The price is fixed for the whole day, so ClaudeBot may cache it once at the start of each day and use the cached value anywhere.

### Kerosene quality

The game updates the quality of the kerosene in the tank using the following formula:

`qPlayer.KerosinQuali = (oldAmount * qPlayer.KerosinQuali + amountBought * type) / (oldAmount + amountBought);`

So the resulting quality `KerosinQuali` is the weighted sum of the original quality plus the quality of the new kerosene. 

Analyze in `CFlugplanEintrag::BookFlight` how bad kerosene affects the amount the plane is damaged after each flight. If total quality is larger than 1, the game calculates `faktorKerosin` which in turn affects the resulting plane condition `CPlane::Zustand`. Note that a "better than normal" quality (factor below 1) does not have any benefits. Buying kerosene of high quality (type == 0) might however still be useful to correct a bad current quality of the kerosene in the tanks.

Hints:
- At an amount of 10000, 5% discount is granted. At 50000, it is 10%.
- Buying kerosene manually also grants the discount from the advisor BERATERTYP_SICHERHEIT.

Mechanic
--------

Use the action ID ACTION_VISITMECH to visit your airline's mechanic. Only while in this room, the following functions may be called.

`bool GameMechanic::setPlaneTargetZustand(PLAYER &qPlayer, SLONG idx, SLONG zustand)`: Sets the plane repair target for the `qPlayer.Planes[idx]`.

`SLONG GameMechanic::setMechMode(PLAYER &qPlayer, SLONG mode)`: Defines which mechanic will be used from now on.

### Explanation of repair mechanic

Planes get damaged after each flight as calculated in `CFlugplanEintrag::BookFlight`. Repair happens at midnight for all planes at once. The current plane condition is stored in `CPlane::Zustand`. If this is lower that `CPlane::WorstZustand`, the game will set `CPlane::WorstZustand` to `CPlane::Zustand`. Depending on the mechanic chosen, the planes will be repaired a certain amount:
- Mechanic 0: Actually has a 50% chance to damage the plane by 2 points, never repairs
- Mechanic 1: Has an 1/8 change to damage the plane by 2 points, else repairs by 5 points
- Mechanic 2: Repairs between 2 to 9 points or 2 to 6 points if condition is below 60, value choosen uniformly at random between the two limits
- Mechanic 3: Repairs exactly 15 points or 18 points if condition is below 60

The repair amount which we will call `Delta` is then capped to ensure that planes is not repaired beyond repair target `CPlane::TargetZustand` or 100. The game then checks if the new `CPlane::Zustand` exceeds the value of `Planes[c].WorstZustand + 20`. If so, the difference is stored in variable `Improvement`. `CPlane::WorstZustand` is then updated to `Planes[c].Zustand - 20` or 0, whatever is higher.

The total repair cost is then calculated as sum of three parts:
- mechanic base salary: Calulated from global array as `gRepairPrice[MechMode] / 30`, per plane. You may check this array. `MechMode` is set via `GameMechanic::setMechMode`.
- regular repair cost: Calculated as `Delta * 10 * CPlane::ptWartungsfaktor * (2100 - CPlane::Baujahr) / 100 * (200 - CPlane::Zustand) / 100` where 
`CPlane::ptWartungsfaktor` is a plane type specific factor and `CPlane::Baujahr` the year when the plane was built.
- extra repair cost: Calculated as `Improvement * CPlane::ptPreis / 110` where `CPlane::ptPreis` is the price for a new plane.

Hint:
- The extra repair cost is often the dominating factor.
- The rule is that `CPlane::WorstZustand` must never be lower then `Planes[c].Zustand - 20`. But since raising `CPlane::WorstZustand` incurs additional cost, it is thus advisable to ensure that `CPlane::Zustand` never drops below 80.

Plane purchase actions
----------------------

Familiarize yourself with the data structures:
- CPlaneType
- CPlane

These are direct buy actions, usually routed to either the museum or the broker.

Use action ID ACTION_BUYNEWPLANE or ACTION_VISITMAKLER to walk to the plane broker. Only there, the following two functions may be used:

`bool GameMechanic::checkPlaneTypeAvailable(SLONG planeType)`: Checks if planes with the given type ID can be bought. You may check the global array `PlaneTypes` at index planeType if this function returns true.

`std::vector<SLONG> GameMechanic::getAvailablePlaneTypes()`: Returns a list of plane type IDs for all planes that can be bought. You may check the global array `PlaneTypes` at all indices returned by this function.

`std::vector<SLONG> GameMechanic::buyPlane(PLAYER &qPlayer, SLONG planeType, SLONG amount)`: Buys specified amount of planes of the given type.

Use action ID ACTION_BUYUSEDPLANE to walk to the museum. Only there, the following two functions may be used:

`SLONG GameMechanic::buyUsedPlane(PLAYER &qPlayer, SLONG planeID)`: Buy a used plane from the museum. You may check the global array `Sim.UsedPlanes[planeID]` for valid entries to find planes that can be bought.

`bool GameMechanic::sellPlane(PLAYER &qPlayer, SLONG planeID)`: Sells a plane to the museum. Ensure that no flights are scheduled for this plane before selling. Use the function `CPlane::CanBeSold` to check. Value of a plane is calculated as `CPlane::ptPreis * CPlane::Zustand / 10000 * CPlane::Zustand * (CPlane::Baujahr - kYearsSinceRelease - 1900) / 120`. Value is reduced to only 10% if it is a starting plane (`CPlane::Sponsored != 0`).

`std::vector<SLONG> GameMechanic::buyXPlane(PLAYER &qPlayer, const CString &filename, SLONG amount)`: Buys a designed plane. DO NOT USE CURRENTLY.

Advertisement / marketing actions
---------------------------------

Use the action IDs ACTION_WERBUNG, ACTION_WERBUNG_ROUTES or ACTION_VISITADS to visit the advertising room. Only while in this room, the following functions may be called.

`bool GameMechanic::buyAdvertisement(PLAYER &qPlayer, SLONG adCampaignType, SLONG adCampaignSize, SLONG routeA)`: Buys an ad campaign to improve the airline's image (`adCampaignType` == 0), the image of a single route (`adCampaignType` == 1) or combined for both (`adCampaignType` == 2). `routeA` has to be given for `adCampaignType` 1 or 2. `adCampaignSize` determines size of effect and also cost.

The cost of the campaign is found in the global array at `gWerbePrice[adCampaignType * 6 + adCampaignSize]`. The effect is:

- `adCampaignType` == 0: Airline's image increases by `cost / 10000 * (adCampaignSize + 6) / 55`
- `adCampaignType` == 1: Routes's image increases by `cost / 30000`
- `adCampaignType` == 2: Airline's image increases by `cost / 15000 * (adCampaignSize + 6) / 55` and routes's image increases by `cost * (adCampaignSize + 6) / 6 / 120000`

The airline's image is capped at 1000, a route's individual image is capped at 100.

Boss actions
------------

Use the action IDs ACTION_EXPANDAIRPORT or ACTION_VISITAUFSICHT to visit the boss room. Only while in this room, the following functions may be called.

### Airport expansion

`GameMechanic::ExpandAirportResult GameMechanic::canExpandAirport(PLAYER & /*qPlayer*/)`: Asks the boss whether a new airport gate can be constructed. The resulting enum will be `ExpandAirportResult::Ok` if an extension is possible or will state the reason otherwise.

`bool GameMechanic::expandAirport(PLAYER &qPlayer)`: Asks the boss to build a new airport gate. Action is only triggered if `expandAirport` returns `ExpandAirportResult::Ok`. This costs 1000000. Note that this does not grant ownership of the gate yet. Auction of the new gate starts the next day.

### Bidding on cities and gates

There might be auctions every day for unassigned gates (either recently built or became unassigned due to airline liquidation) and branch offices in other cities. You may check the global array `TafelData.ByPositions` for available auctions.

Gates are either unassigned or owned by one airline. Only gates owned by your airline will be used. A gate is occupied for one full hour after each arrival and one full hour before and after each departure each. For each planned flight, you can check `CFlugplanEintrag::Gate` to see which gate was assigned and `CFlugplanEintrag::GateWarning` if there is a "no gate available" warning. If no gate was available for the flight, airline image is reduced by 2 points. Gate assignment is done automatically. Note that the game only requires gates for planes landing in or starting from the home airport (city ID given by `Sim.HomeAirportId`).

Branch offices can be called (ACTION_CALL_INTERNATIONAL or ACTION_CALL_INTER_HANDY) to get access to additional flight jobs starting or landing in their city. If you lost an auction to a competitor, there might be a later auction for another office in the same city.

Bids can be placed by ClaudeBot and all competitors the entire day. It is possible to overbid a competitor who made a bid on the same gate or city before. On the beginning of the next day, the airline that placed the last bid wins.

`bool GameMechanic::bidOnGate(PLAYER &qPlayer, SLONG idx)`: Places a bid on the gate at index `idx` in `TafelData.ByPositions`. Price increases by 10% after every bid.

`bool GameMechanic::bidOnCity(PLAYER &qPlayer, SLONG idx)`: Places a bid on the city office at index `idx` in `TafelData.ByPositions`. Price increases by 10% after every bid.

Item management
---------------

The game has an item mechanic. Items can be used to sabotage competitors or protect yourself against their sabotage.

`GameMechanic::PickUpItemResult GameMechanic::pickUpItem(PLAYER &qPlayer, SLONG item)`: Attempts to pick up the specified item. Item IDs are defined in `defines.h` and are macros that start with `ITEM_`. The returned enum tells you if the item was picked up, there was no space, it is not allowed yet or other conditions are not met. One has to be in the correct room for the given item.

`bool GameMechanic::removeItem(PLAYER &qPlayer, SLONG item)`: Drops the specified item from the inventory to make space.

`bool GameMechanic::useItem(PLAYER &qPlayer, SLONG item)`: Attempts to use the specified item at the current location.

`GameMechanic::BuyItemResult GameMechanic::buyDutyFreeItem(PLAYER &qPlayer, UBYTE item):` This action can only be used in the "Duty Free" shop. Use the action ID ACTION_VISITDUTYFREE to walk there. Use this function to buy certain items for money.

We now explain certain items. The are more items but for now, please do not use these yet.

### Pills

Necessary if a competitor poisened you which will cause you to walk to the bathroom constantly. Check variable `mIsSickToday` to see if this is the case. Follow the steps:

- Pick up the item `ITEM_POSTKARTE` in the boss room. Check `Sim.ItemPostcard != 0` to see if it is there. Only needs to be done once.
- Use item `ITEM_POSTKARTE` while in HR office. Only needs to be done once.
- Pick up item `ITEM_TABLETTEN` while in HR office.
- Use item `ITEM_TABLETTEN` to cure sickness.

Repeat the last two steps to cure sickness again.

### Antivirus

Necessary if competitor infected your laptop with a virus. Check `qPlayer.LaptopVirus != 0` to see if laptop is infected. Follow the steps:

- Pick up item `ITEM_SPINNE` while in travel agency. Only needs to be done once.
- Use item `ITEM_SPINNE` while in the saboteur room. You need to have his trust (`qPlayer.ArabTrust != 0`). Only needs to be done once.
- Pick up item `ITEM_DART` while in the saboteur room. Only needs to be done once.
- Use item `ITEM_DART` while in the advertising room. Only needs to be done once.
- Pick up item `ITEM_DISKETTE` while in the advertising room.
- Use item `ITEM_DISKETTE` to fix laptop.

Repeat the last two steps to repair laptop again.

### End strike

Can be used to end a strike. Strikes can be stirred up by a competitor or are caused by low wages. Check `qPlayer.StrikeHours > 0` if there is currently a strike. Follow the steps:

- Pick up item `ITEM_BH` while at the plane broker. Only needs to be done once.
- Use item `ITEM_BH` while in "Duty Free" shop. Only needs to be done once.
- Pick up item `ITEM_HUFEISEN` while in "Duty Free" shop. Only needs to be done once.
- Use item `ITEM_HUFEISEN` while at Rick's bar. `qPlayer.TrinkerTrust` should now be `1`. Only needs to be done once.

Only after these steps have been completed once, you may call the function `GameMechanic::endStrike(PLAYER &qPlayer, EndStrikeMode mode)` with `EndStrikeMode::Drunk` to end the worker's strike. Check ``qPlayer.TrinkerTrust == 1` to see if this condition is met.

### Gain saboteur trust

To gain the trust of the saboteur, buy item `ITEM_MG` at the "Duty Free" shop. Give it to the saboteur to gain his trust.

### Buy laptop

Buy a laptop (`ITEM_LAPTOP`) at the "Duty Free" shop to be able to plan flights anywhere. Shop only has one laptop in stock at any given day, a competitor might have been faster. This action can be repeated to improve laptop quality (`qPlayer.LaptopQuality`) point-by-point until maximum quality of 4. Laptops only become available starting at a specific day. Use `Sim.Date > DAYS_WITHOUT_LAPTOP` to check.

### Buy mobile phone

Buy a mobile phone (`ITEM_HANDY`) at the "Duty Free" shop to be able to get international flight jobs from other cities.

Security office
---------------

Use the action IDs ACTION_VISITSECURITY or ACTION_VISITSECURITY2 to visit the security office. Only while in this room, the following functions may be called.

`bool GameMechanic::setSecurity(PLAYER &qPlayer, SLONG securityType, bool targetState)`:

`bool GameMechanic::toggleSecurity(PLAYER &qPlayer, SLONG securityType)`:

`bool GameMechanic::sabotageSecurityOffice(PLAYER &qPlayer)`:

For now, please do not use these actions.

Sabotage actions
----------------

Use the action IDs ACTION_SABOTAGE or ACTION_VISITSABOTEUR to visit the saboteur room. Only while in this room, the following functions may be called.

`SLONG GameMechanic::setSaboteurTarget(PLAYER &qPlayer, SLONG target)`:

`GameMechanic::CheckSabotage GameMechanic::checkPrerequisitesForSaboteurJob(PLAYER &qPlayer, SLONG type, SLONG number, BOOL fremdSabotage)`:

`bool GameMechanic::activateSaboteurJob(PLAYER &qPlayer, BOOL fremdSabotage)`:

For now, please do not use these actions.

Misc rooms
----------

The following actions and corresponding rooms do not serve any real purpose. They can be used as default actions to give the player character a more "human" 

Use ACTION_VISITKIOSK to visit the kiosk room.

Use ACTION_VISITMUSEUM to visit the museum.

Use ACTION_VISITTELESCOPE to visit telescope.

Use ACTION_VISITRICK to visit Rick’s bar. This can be useful to end a strike.

Global game state read permissions
==================================

The game unfortunately stores its state in global variables. Thus there is a risk of accidently implementing a cheating computer player by:

- modifying a global variable directly when there is no player action that would have the same effect
- reading a global variable to learn something about the game's state that a human player would not be able to know

This section outline what can be accessed. Only access global variables and call global functions if defined here.

If you think you should have access to a variable or you find that you are strongly limited by not having access, flag it to me and I might grant you access.

Global variables
----------------

This section lists which functions and variables outside of the `ClaudeBot` class may be access by ClaudeBot.

What the agent must never do:
- Do not write directly to any of the listed global variables.
- Do not modify Sim state except through the permitted GameMechanic and PLAYER API calls.
- Do not use global arrays or tables outside the legal access window defined by the room/action rules.
- Do not treat the bot’s own internal ClaudeBot state as a substitute for a legal game action.

If a global variable or function is not listed here, assume it is forbidden. If you see a bot-side implementation reading or writing a forbidden global, stop and replace it with a legal interface call or a GameMechanic pattern. If you find yourself unable to do so or it comes with a massive cost, ask me if access rights might be granted.

Logging exception
-----------------

For logging purposes, every read access is permitted. Any variable may be read in any room,
regardless of the restrictions listed below, as long as the value read is only written to the
log and nothing else.

The value must not influence what ClaudeBot does: it may not be stored in a member variable,
cached for later, or used in any condition, calculation or comparison that affects a decision.
The moment a value is used for anything other than a log message, the normal access rules
apply to it in full.

Hint: It might be reasonable to cache certain global variables for later use and update the cached value whenever access is permitted.

### Global Sim instance

Global object that manages most of the game state.

All classifications are read-only.

You have read access to:
- `Sim.Date`, `Sim.Time`, `Sim.GetHour()`, `Sim.GetMinute()`: Query in-game time.
- `Sim.Weekday`: Get current day of the week.
- `Sim.StartWeekday`: Get day of the week where game was started.
- `Sim.Difficulty`: Denotes whether we are in a free game or a mission. Always assume free game `Sim.Difficulty == -1`.
- `Sim.UsedPlanes`: List of used planes to buy. Access permitted while in museum.
- `Sim.HoleKerosinPreis()`: Fetches current price for kerosene. Permitted while visiting the Arab and while in the personal office. `Sim.HoleKerosinPreis(1)` returns `Sim.Kerosin` directly (price for regular quality kerosene) which may also be accessed directly under the same conditions. The price does not change during the day, so ClaudeBot may read it once per day and cache the value for use in any room.
- `Sim.HomeAirportId`: City ID of the home airport.
- `Sim.ItemZange`: Is the item `ITEM_ZANGE` still available at the saboteur?
- `Sim.ItemPostcard`: Is the item `ITEM_POSTKARTE` still available at the HR office?
- `Sim.nSecOutDays`: Check for how many days the security office is closed. Security office can close due to sabotage.

### Player objects (yourself)

You can access everything in the PLAYER class instance that refers to your player. A reference to this instance is passed as variable qPlayer.

All instances of the PLAYER class can be found in the global array `Sim.Players.Players`. If the reference `qPlayer` is not available, use this expression `Sim.Persons[Sim.Persons.GetPlayerIndex(playerNum)]`.

All classifications are read-only except where explicitly shown as read/write.

- `Abk`: Abbreviation of airline name.
- `AnzAktien`: Total number of shares.
- `ArabTrust`: Current trust level of the saboteur.
- `Auftraege`: List of taken passenger jobs. Only access while in personal office or while you have a access to a laptop.
- `BilanzGestern`, `BilanzWoche.Hole()` and `BilanzGesamt`: Yesterday's balance, the sum of the last seven daily balances, and the balance over the whole game. Only read while in the personal office and while a financial advisor is employed (`qPlayer.HasBerater(BERATERTYP_GELD) > 0`).
- `BotLevel`: Either 1, 2 or 3. Can be used to implement different difficulty levels of ClaudeBot. For now, the test harness only uses `BotLevel = 2` and we implement a single strategy only.
- `CalcCreditLimit()`: Calculate how much money can be loaned from the bank.
- `Credit`: Current loan amount.
- `Dividende`: Check current dividend.
- `Frachten`: List of taken freight jobs. Only access while in personal office or while you have a access to a laptop.
- `Gates.Auslastung` and `Gates.NumRented`: Current gate utilization level and total number of owned gates.
- `HasBerater()`: Check advisor availability.
- `HasItem()`: Check item ownership.
- `Image`: Current airline image. May always be read while in the advertising room, even without an advisor. With `qPlayer.HasBerater(BERATERTYP_GELD) >= 50` it may be read anywhere.
- `KerosinQuali`: Current kerosene quality level. Only read if `qPlayer.HasBerater(BERATERTYP_KEROSIN) >= 30`.
- `Kooperation`: Cooperation flags with other players.
- `Kurse`: The last ten share prices of your own airline. May always be read.
- `LaptopVirus`: Laptop virus status.
- `MaxAktien`: Maximum number of shares including those that can still be emitted.
- `MechMode`: Which mechanic is currently employed. Only read while visiting the mechanic.
- `Money`: Current cash balance.
- `OfficeState`: Office usability status.
- `OwnsAktien`: Shares owned in each airline, array access by airline ID.
- `Planes`: Plane collection (accessing, iterating, reading plane data). Access rights depend on the exact field of `CPlane` and are given below.
- `PlayerNum`: Player number, used as index in many arrays.
- `PlayerWalkRandom`: Random number generator.
- `RentRouten`: Rented routes. Special access rights are explained in a dedicated section further below.
- `RobotActions`: Read and write access permitted. Used to store the planned actions. 
- `Tank`: Total volume of kerosene tank.
- `TankInhalt`: Current amount of kerosene in tank. Only read when `qPlayer.HasBerater(BERATERTYP_KEROSIN) > 30`.
- `TankOpen`: Whether the tanks are released for use. Only read while in the personal office.
- `TankPreis`: Average price paid for the kerosene currently in the tank. May always be read.
- `TrinkerTrust`: Whether or not the trust of the drunk guy was earned (at Rick's bar, can help to end a strike).
- `xBegleiter`: Number of superfluous stewardesses. A negative number indicates a shortage. Only read when `qPlayer.HasBerater(BERATERTYP_PERSONAL) > 0` or while in personal office or while in the HR room.
- `xPiloten`: Number of superfluous pilots. A negative number indicates a shortage. Only read when `qPlayer.HasBerater(BERATERTYP_PERSONAL) > 0` or while in personal office or while in the HR room.

### Player objects (competitors)

You can read some fields in the PLAYER class instance that refer to a competitor.

Some values may only be read if the spy has been hired and has sufficient skill level. Current skill level can be queried using `qPlayer.HasBerater(BERATERTYP_INFO) > 0`.

All classifications are read-only.

- `Abk`: Abbreviation of airline name.
- `AnzAktien`: Total number of shares. Only read if `qPlayer.HasBerater(BERATERTYP_INFO) >= 50`.
- `BilanzWoche`: Weekly balance. Only read if `qPlayer.HasBerater(BERATERTYP_INFO) >= 50`.
- `Credit`: Current loan amount. Only read if `qPlayer.HasBerater(BERATERTYP_INFO) >= 0`.
- `Image`: Current airline image. Only read when `qPlayer.HasBerater(BERATERTYP_INFO) >= 50`.
- `MaxAktien`: Maximum number of shares including those that can still be emitted.
- `Money`: Current cash balance. Only read if `qPlayer.HasBerater(BERATERTYP_INFO) >= 0`.
- `OfficeState`: Office usability status.
- `OwnsAktien`: Shares owned in each airline, array access by airline ID. Only read for at index referring to own airline when `qPlayer.HasBerater(BERATERTYP_GELD) > 0`. Only read at other indices when `qPlayer.HasBerater(BERATERTYP_INFO) > 0`.
- `PlayerNum`: Player number, used as index in many arrays.

### CPlane object

Located in each `PLAYER` object at `qPlayer.Planes`. Contains instances of type `CPlane` which describe a plane that the player owns.

The access rights refer to planes owned by ClaudeBot. For competitor planes, you may only browse the entire list and access `Name` and `TypeId`.

- `Name`: Individual name of the plane.
- `TypeId`: Gives the plane type ID (index for the global array `PlaneTypes`).
- `Flugplan`: Flight plan, only access while in personal office or while you have a access to a laptop.
- `WorstZustand`: Low point of plane's condition given as percentage. Access permitted only while visiting the mechanic.
- `Zustand`: Current condition of the plane given as percentage. Access permitted only while visiting the mechanic.
- `TargetZustand`: Target condition given as percentage. Access permitted only while visiting the mechanic.
- `Salden`: Array holding the daily saldo of this plane. Access permitted while in personal office or while having access to a laptop. Needs financial advisor (`qPlayer.HasBerater(BERATERTYP_GELD) > 0`).
- `Baujahr`: Access permitted only while visiting the mechanic, while in personal office or while you have a access to a laptop.
- `AnzPiloten`: Current number of pilots assigned to this plane. Only access while in personal office, HR office or while having access to a laptop.
- `AnzBegleiter`: Current number of stewardesses assigned to this plane. Only access while in personal office, HR office or while having access to a laptop.
- `MaxBegleiter`: Target number of stewardesses. Only access while in personal office, HR office or while having access to a laptop. Write access permitted while in personal office (valid range: `ptAnzBegleiter` up to including `ptAnzBegleiter *2`).
- `PersonalQuality`: Average personal skill ranging from 0 to 100. Read access while in HR office.
- `Wartungskosten`: Amount spent for plane maintenance and repairs on the previous day. Access permitted only while visiting the mechanic.
- `Sitze`, `Tabletts`, `Deco`, `Reifen`, `Triebwerk`, `Sicherheit`, `Elektronik` and `Essen`: Current plane upgrade levels. Read access while in personal office.
- `SitzeTarget`, `TablettsTarget`, `DecoTarget`, `ReifenTarget`, `TriebwerkTarget`, `SicherheitTarget`, `ElektronikTarget`, `EssenTarget`: Planned plane upgrade levels. Read and write access while in personal office.
- `Auslastung`: Gives the average utilization over the previous day in % of seats. Only access while in personal office or while you have a access to a laptop.
- `AuslastungFC`: Gives the average utilization over the previous day in % of first class seats. Only access while in personal office or while you have a access to a laptop.
- `Kilometer`: Total number of kilometers flown. Only access while in personal office or while you have a access to a laptop.
- `SummePassagiere`: Total number of passengers transported. Only access while in personal office or while you have a access to a laptop.
- `MaxPassagiere` and `MaxPassagiereFC`: Denotes the current split of seats between regular and first-class passengers (affects utilization and income for route flights).
- `MaxPassagiereTarget` and `MaxPassagiereTargetFC`: Denotes the target split of seats between regular and first-class passengers (will be applied the next day).
- `Sponsored`: Denotes a starting plane. Can only be sold for 10% of the usual value.
- `Problem`: If larger than zero, plane has technical problem and cannot be used. Only access while in personal office or while you have a access to a laptop.

The following member variables are copied over from the corresponding CPlaneType and can always be read:
- `ptHersteller`: String containing manufactorer name.
- `ptName`: String containing type name.
- `ptErstbaujahr`: First year where this type entered the market.
- `ptReichweite`: Maximum range of the plane in kilometers.
- `ptGeschwindigkeit`: Aircraft speed in kilometers per hour.
- `ptPassagiere`: Maximum number of passengers that can be transported. `ptPassagiere / 10` rounded down is freight capacity in tons.
- `ptAnzPiloten`: Number of pilots required for normal operation.
- `ptAnzBegleiter`: Number of stewardesses required for normal operation.
- `ptTankgroesse`: Tank size in liters.
- `ptVerbrauch`: Fuel consumption in liters per flight hour.
- `ptPreis`: Price to pay for a new plane of this type.
- `ptLaerm`: Noise level of this plane type, can reduce customer satisfaction.
- `ptWartungsfaktor`: Multiplier in repair cost.

A plane is only able to operate if the following conditions are met:
- `AnzBegleiter >= ptAnzBegleiter`
- `AnzPiloten >= ptAnzPiloten`
- `Problem == 0`

The following member function may be called:
- `CPlane::CalculatePrice`: Current value of the plane. Call allowed while in museum.
- `CPlane::CanBeSold`: Checks if plane has flights scheduled. Call allowed while in museum.

### RentRouten object

Located in each `PLAYER` object at `qPlayer.RentRouten.RentRouten`. Contains an instance of type `CRentRoute` for every instance of `CRoute` in the global array `Routen` at the same index. `CRentRoute` describes whether `qPlayer` rents and flies the corresponding route.

Familiarize yourself with the data structure:
- CRentRoute

All classifications are read-only.

The following may be accessed if the player object is ClaudeBot:
- `Rang`: You may always use the expression `Rang != 0` to check if you are currently renting this route. If the value is `> 0`, it gives the position of the player in the ranking of who flies this route the most of all four airlines. The exact value may only be read while at the route box or in the personal office.
- `Auslastung`: Gives the average utilization in % of seats in planes that fly this route. You may only read this while in the personal office. The value is averaged over the past days. `AuslastungBot` is the data but filtered using a faster time constant `kRouteAvgDays` which may also be changed.
- `AuslastungFC`: Gives the average utilization in %  of first class seats in planes that fly this route. You may only read this while in the personal office. The value is averaged over the past days. `AuslastungFirstClassBot` is the data but filtered using a faster time constant `kRouteAvgDays` which may also be changed.
- `RoutenAuslastung`: Gives how much the route is being utilized by your airline in percent of the weekly demand. You may only read this while in the personal office or at the route box. `RoutenAuslastungBot` is the data but filtered using a faster time constant `kRouteAvgDays` which may also be changed.
- `Image`: Image of this route. You may only read this while in the personal office, at the route box or in the advertising room.
- `Miete`: Monthly rent that needs to be paid for this route. You can always read this value.
- `Ticketpreis`: Price that each passenger has to pay. You may only read this while in the personal office, Change via `GameMechanic`.
- `TicketpreisFC`: Price that each first-class passenger has to pay. You may only read this while in the personal office, Change via `GameMechanic`.

The following may be accessed if the player object is a competitor:
- `Rang`: You may only read this value while at the route box and while having a spy (`qPlayer.HasBerater(BERATERTYP_INFO) > 0`).
- `RoutenAuslastung`: Gives how much the route is being utilized by the competitor in percent of the weekly demand. You may only read this while in the personal office or at the route box and while having a spy (`qPlayer.HasBerater(BERATERTYP_INFO) > 0`).
- `Miete`: Monthly rent that needs to be paid for this route. You can always read this value.

### Global read-only helpers and tables

You may also read the following global tables and helpers when the rules permit them:

- `LastMinuteAuftraege` may only be read while in the room accessed via ACTION_CHECKAGENT1
- `ReisebueroAuftraege` may only be read while in the room accessed via ACTION_CHECKAGENT2
- `gFrachten` may only be read while in the room accessed via ACTION_CHECKAGENT3
- `AuslandsAuftraege[cityId]` may only be if canCallInternational() was checked with the cityId
- `AuslandsFrachten[cityId]` may only be if canCallInternational() was checked with the cityId
- `Routen` while at the route box.
- `PlaneTypes` may only be read while at the plane broker
- `TafelData` may only be read while in the boss office
- `Cities[...]`, `Cities.find(...)`, `Cities.CalcDistance(...)`, `Cities.CalcFlugdauer(...)` to query informations about cities and flight distances/duration.
- `SeatCosts`, `FoodCosts`, `TrayCosts`, `DecoCosts`, `TriebwerkCosts`, `ReifenCosts`, `ElektronikCosts`, `SicherheitCosts` any time to check costs of plane upgrades

Global functions
----------------

`Hdu.HercPrintfMsg(...)`: Read-only. Logging interface for ClaudeBot.

GameMechanic class
------------------

Found in src/GameMechanic.cpp

Main interface to perform actions in the game. This class was implemented as part of a larger refactoring to reduce write access to global variables. Thus most of this class is safe to use with exceptions listed in the following.

### Forbidden actions in GameMechanic

You are not allowed to call the following functions:
- GameMechanic::bankruptPlayer(PLAYER &qPlayer)
- GameMechanic::paySaboteurFine(SLONG player, SLONG opfer)
- GameMechanic::planStrike(PLAYER &qPlayer)
- GameMechanic::refillFlightJobs(SLONG cityNum, SLONG minimum)
- GameMechanic::flightJobsInitialFill()
- GameMechanic::flightJobsRefill()
- GameMechanic::executeAirlineOvertake()
- GameMechanic::executeSabotageMode1()
- GameMechanic::executeSabotageMode2(bool &outBAnyBombs)
- GameMechanic::executeSabotageMode3()
- GameMechanic::injectFakeSabotage()

Further restrictions apply:
- Call `GameMechanic::endStrike(PLAYER &qPlayer, EndStrikeMode mode)` only with `EndStrikeMode::Drunk` as second parameter
- Only call `GameMechanic::endStrike` if item horse shoe has been given to the trinker (check `via qPlayer.TrinkerTrust == 1`)
- A call to `GameMechanic::killFlightJob(PLAYER &qPlayer, SLONG par1, bool payFine)` must use `payFine == true`
- A call to `GameMechanic::killFreightJob(PLAYER &qPlayer, SLONG par1, bool payFine)` must use `payFine == true`

### Restrictions for allowed functions

Note that even for allowed functions there are usage restrictions (player character almost always must be in the correct room) which are listed in this document together with the explanation for the given function.

Notes regarding code base
=========================

Album containers
----------------

The game uses a legacy container to store objects called `ALBUM_V`. It comes with a few quirks that we will now explain.

Generally, these are sortable containers for objects of the same type. The storage is handled automatically, but they do not expand automatically. On insertion, a unique ID of type `uint32_t` is created for the new element. "Unique" here means that it is unique to the album not globally unique. Elements can be retrieved using either the unique ID or the index. When accessing elements using the unique ID, an `ALBUM_V<T>` is comparable to an `std::unordered_map<uint32_t, T>`. When accessing elements using the index, an `ALBUM_V<T>` behaves like an `std::vector<T>`. This also holds true regarding time complexity. When the album is sorted, unique IDs are preserved but indices are not. Confusingly, access via unique ID and index are made using the same overloaded `operator[id]`. If `id` is below magic number `0x1000000`, it is treated as an index. Otherwise, it is treated as a unique ID.

Another pitfall of these data structures that it does not grow as required and after manually growing the array using `ReSize()`, it does not hide the new, empty slots. Empty slots also remain after an item is deleted from the album. When iterating over an album, the code will also iterate over empty slots. Thus, these need to be explicitly skipped during iteration like this:

```
    for (SLONG i = 0; i < album.AnzEntries(); i++) {
        if (album.IsInAlbum(i) == 0) {
            continue;
        }
        [...]
    }
```

For new data structures, never use `ALBUM_V`. Use C++ STL containers instead.

### Member functions

Only ever use the following functions of existing `ALBUM_V` objects:

- `SLONG AnzEntries()`: Query capacity of the container, includes previously deleted items.
- `SLONG GetNumUsed()`: Query number of valid items of the container.
- `SLONG GetNumFree()`: Query number free slots in the container.
- `ULONG GetIdFromIndex(SLONG i)`: Converts an index into the corresponding unique ID.
- `SLONG IsInAlbum(ULONG id)`: Checks if specified item is a valid item or an empty slot. Works both with indices and unique IDs.
- `SLONG operator()(ULONG id)`: Alias for `find`. Use that one instead.
- `SLONG find(ULONG id)`: Converts a valid unique ID into the corresponding index. If parameter already is a valid index, it is directly returned. Raises an exception otherwise (invalid unique ID and/or index).
- `T &operator[](ULONG id)`: Retrieve an element either using an index or an global ID.
- `operators == and !=`: Can be used to compare the contents of two albums.
- `SLONG GetRandomUsedIndex(TEAKRAND *random = NULL)`: Returns the unique ID of a randomly chosen, valid element of the album. Can optionally be called with a pointer to a random number generated of type `TEAKRAND`.

You should not need to use the following functions:

- `void ReSize(SLONG anz)`: Resizes a container.
- `void ClearAlbum()`: Removes all items, leaving capacity the same.
- `void FillAlbum()`: Fills all empty slots with default-constructed items.
- `ULONG operator*=(T rhs)`: Places the given element into the last empty slot in the album and returns its unique ID. Raises an exception if no empty slot is available.
- `ULONG operator+=(T rhs)`: Places the given element into the first empty slot in the album and returns its unique ID. Raises an exception if no empty slot is available.
- `void operator-=(ULONG id)`: Removes an element either using an index or an global ID. Raises an exception if no matching element can be found.
- `void Sort()`: Stable-sorts the album.
- `void Swap(SLONG a, SLONG b)`: Swaps two items in the item, both either specified as index or both as unique ID.

### Iterators

It is possible to iterator over albums using range-based for-loops:

```
    for (auto &i : album) {
        if (!i.IsInAlbum()) {
            continue;
        }
        [...]
    }
```

Open questions / ambiguities
============================

Use this section to list any open questions or ambiguities regarding rules that you need me to clarify.

Raised 2026-08-16 (session 3). All five items from session 2 were answered by commit
`c93480f` and have been removed; the resolutions are recorded in DECISIONS.md.
Items marked **NEEDS DECISION** change what the bot may legally do; the rest are
documentation fixes or traps recorded so they are not rediscovered.

The new fine-grained *CPlane object* section resolved the plane-access problem. Two
fields the bot needs are missing from it, both for decisions that show up directly in the
scored operating saldo.

Blocking / needs your decision
------------------------------

### 1. `CPlane::OhneSitze` is not listed, but refit cost is scored — **NEEDS DECISION**

`Schedule.cpp:781-787`: whenever a plane flies a job whose type differs from its current
seat configuration, the game flips `OhneSitze` and charges 15,000 under category 2111
(`FlugzeugUmbau`) — which *is* part of `GetOpSaldo()`. Avoiding needless
passenger↔freight switches is therefore worth real score, but `OhneSitze` is the only
field that says which configuration a plane is currently in, and it is absent from the
*CPlane object* list.

Request: read access to `CPlane::OhneSitze`, gated like `Flugplan` (personal office or
working laptop). Without it the only legal alternative is to guess the configuration from
the job types the bot itself scheduled, which desynchronises as soon as a plan is changed.

Merten: The human player also does not have access. This cost can be derived from analyzing the flight plan. This is part of the challenge when the scheduling flights to all planes:
Not only are there constraints to meet but you also need to minimize automatic flights and this refitting cost.
Hint: I believe that you worry a bit too much about refitting cost. Automatic flights usually add more cost. BUT: I do not want to prescribe a solution for you. I suggest you monitor the cost of refitting and collect some data and then you decide whether it is worth to try to minimize this cost.

### 2. `CPlane::MaxPassagiere` / `MaxPassagiereFC` are not listed, but they cap route revenue — **NEEDS DECISION**

The *Constraints* section is correct that passenger **jobs** are limited by `ptPassagiere`
(verified at `Player.cpp:2314`). Route flights are different: `CFlugplanEintrag::CalcPassengers`
caps the passenger count at `qPlane.MaxPassagiere + qPlane.MaxPassagiere / 2`
(`Schedule.cpp:320`), i.e. the capacity of the *current seat configuration*, not the plane
type's nominal capacity. `MaxPassagiereFC` plays the same role for first class.

These are also the fields that make the `SitzeTarget` upgrade pay off, so without them the
bot cannot evaluate either route profitability or seat upgrades.

Request: read access to `MaxPassagiere`, `MaxPassagiereFC` and their
`MaxPassagiereTarget` / `MaxPassagiereTargetFC` counterparts, gated like the other
configuration fields (personal office, or office/laptop).

Merten: Access granted to MaxPassagiere and MaxPassagiereFC. MaxPassagiereTarget and MaxPassagiereTargetFC appear to not be used by the game's code.

Documentation fixes
-------------------

### 3. `ptAnzPiloten` is described as stewardesses

In the new *CPlane object* section both `ptAnzPiloten` and `ptAnzBegleiter` read "Number
of stewardesses required for normal operation". `class.h:884` says `ptAnzPiloten` is
"Piloten und Co-Piloten" — the first one should say pilots (and co-pilots).

Merten: Fixed

### 4. Minor: plane type name fields for logging

The always-readable pt* list omits `ptName`, `ptHersteller` and `ptErstbaujahr`. I would
like to use `ptName` in log messages so schedules are readable in GameLog.txt. Fine to
read them? They carry no information a human player could not see at the broker.

Merten: Granted