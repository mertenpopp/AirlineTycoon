The Game
========

Airline Tycoon Deluxe is a resource-management/tycoon game where you control the airline manager during the day. You walk in the airport from room to room and talk to different people in different rooms to perform specific actions. You buy planes, upgrade them, hire staff, take flight jobs and schedule flights. Flights bring you money which you can use to buy more planes.

The game simulates a day/night cycle. You work from 9 am to 18 pm and can only perform actions during this time. The game state however advanced during night: Planes start and land as scheduled, money made or lost as usual. You work the entire week including on Sunday.

There are always four competing airlines:
- "Sunshine Airways" (SA)
- "Falcon Lines" (FL)
- "Phoenix Travel" (PT)
- "Honey Airlines" (HA)

Airlines are enumerated starting at 0 in the order given. ClaudeBot will usually play as "HoneyAirlines" but shall work playing as any airline. The existing scaffolding in Bot.cpp has a reference called `qPlayer` to the correct `PLAYER` instance. The airline enumeration is found at `qPlayer.AirlineNum`.


In the following, qPlayer always is a reference to the instance of the Player class which refers to Honey Airlines.

Note that most actions have to performed in a particular room. ClaudeBot shall set the desired action ID in `RobotPlan()` and the game will walk the character to the correct room. On arrival, `RobotExecuteAction()` will be called.

In `RobotPlan()`, a secondary action ID shall also be set. This will used to walk the character to a different room in case the room connected to the primary action ID is already occupied by a competitor. Thus in `RobotExecuteAction()`, it shall be checked which action ID was successful by checking `qPlayer.RobotActions[0]`.

ClaudeBot shall check that it is in the correct room by using the function: `qPlayer.GetRoom()`. If it is not the correct room, only print a warning for now and do still perform the planned action.

Note that some rooms open and close at a specific time. Opening hours also depend on the day of the week:
- ClaudeBot shall use the following function to check if the room is open: bool checkRoomOpen(SLONG roomId)
- ClaudeBot shall use the following to translate an action ID to a room ID: SLONG getRoomFromAction(SLONG PlayerNum, SLONG actionId)
- When planning the next action, consider the time it requires to walk to a room

We will list now all actions that can be performed in the game via the class GameMechanic.
If GameMechanic returns a bool this usually means whether or not the action could be completed.

Bank actions
------------

Bank actions are all performed in the bank room. All action IDs listed in this section lead to this room.

### Take out loans

`bool GameMechanic::takeOutCredit(qPlayer, amountToRaise)`

Call this to take out a loan. Call qPlayer.CalcCreditLimit() to find out how much money can be raised.
Total amount of money owed is stored in qPlayer.Credit. Note that this is a loan and interest has to be paid.

Recommended action ID: ACTION_RAISEMONEY

### Pay back loan

`bool GameMechanic::payBackCredit(qPlayer, amountToPayBack)`

Call this to pay back your loans. Total amount owed is stored in qPlayer.Credit.

Recommended action ID: ACTION_DROPMONEY

### Emit stock

`bool GameMechanic::emitStock(PLAYER &qPlayer, SLONG neueAktien, SLONG mode)`

Issue new shares. Gives the airline some money however, reduces stock price and more float means competitors could take you over.
Analyze the code of this function to see how the mode affects how much money is made and by how much the stock price drops.

Recommended action ID: ACTION_EMITSHARES

### Set dividend

`bool GameMechanic::setDividend(PLAYER &qPlayer, SLONG dividend)`

Set the dividend for the airline’s stock. Higher dividend costs more money but improves stock price. Note that increases in dividend have a delay before they take effect. Decreases immediately have a (negative) effect.

Recommended action ID: ACTION_SET_DIVIDEND

### Buy stock

`bool GameMechanic::buyStock(PLAYER &qPlayer, SLONG airlineNum, SLONG amount)`

Purchase shares in another airline identified by airlineNum. Analyze the code to see how high the bank fee is.
If commit == false, no action is made. The second return value gives the total amount of money that will be spent.
Note that stock price will increase.

Recommended action ID: ACTION_BUYSHARES

### Sell stock

`std::pair<bool, __int64> GameMechanic::sellStock(PLAYER &qPlayer, SLONG airlineNum, SLONG amount, bool commit)`

Sell shares held in another airline identified by airlineNum. Analyze the code to see how high the bank fee is.
If commit == false, no action is made. The second return value gives the total amount of money that will be gained.
Note that stock price will decrease.

Recommended action ID: ACTION_SELLSHARES

### Overtake airline

`bool GameMechanic::overtakeAirline(PLAYER &qPlayer, SLONG targetAirline, bool liquidate)`

Action for trying to take over another airline via stock acquisition. Airline is taken over with all planes, routes, money and debt. Parameter liquidate can be used to erase airline completely instead.
The function canOvertakeAirline() checks whether the target is valid, whether you have enough stock (>= 50%), and whether the enemy blocks acquisition by owning stock from your airline (>= 30%).

Recommended action ID: ACTION_OVERTAKE_AIRLINE

### Other

Action ID ACTION_VISITBANK can be used for a generic action for bank interactions.


Flight jobs / freight jobs actions
----------------------------------

These actions send the bot to the special offices where flight jobs can be viewed and taken.

### Last minute jobs

Action ID to access room: ACTION_CHECKAGENT1

Only while performing this action, the global array LastMinuteAuftraege may be accessed. Never write to this array.
Browse this array to find suitable flights. All flights here can go from any city to any other city and typically need to be completed either today or tomorrow. Plane must have required number of seats at least. Plane needs to be able to travel the required distance. The money for the job is paid if completed on time. If flight has been taken but not completed on time, a fine has to be paid. Some jobs have a fine of zero.
You can pick a flight job using:

`bool GameMechanic::takeLastMinuteJob(PLAYER &qPlayer, SLONG jobId, SLONG &outObjectId)`

Job will be added to qPlayer.Auftraege and can be found using outObjectId.

### Travel agency jobs

Action ID to access room: ACTION_CHECKAGENT2

Only while performing this action, the global array ReisebueroAuftraege may be accessed. Never write to this array.
Browse this array to find suitable flights. All flights here will connect the home airport with any other city. Plane must have required number of seats at least. Plane needs to be able to travel the required distance. There is a premium if completed on time. If flight has been taken but not completed on time, a fine has to be paid. Some jobs have a fine of zero.
You can pick a flight job using:

`bool GameMechanic::takeFlightJob(PLAYER &qPlayer, SLONG jobId, SLONG &outObjectId)`

Job will be added to qPlayer.Auftraege and can be found using outObjectId.

### Freight jobs

Action ID to access room: ACTION_CHECKAGENT3

Only while performing this action, the global array gFrachten may be accessed. Never write to this array.
Browse this array to find suitable flights. All flights here can go from any city to any other city. Total freight volume can be transported via multiple trips and/or planes. Plane needs to be able to travel the required distance. There is a premium if total freight volume was transported on time, otherwise, a fine has to be paid. Some jobs have a fine of zero.
You can pick a freight job using:

`bool GameMechanic::takeFreightJob(PLAYER &qPlayer, SLONG jobId, SLONG &outObjectId):`

Job will be added to qPlayer.Frachten and can be found using outObjectId.

### International jobs (passenger flights)

Each airline can purchase offices in other cities. They grant access to additional flight jobs.

Action ID to access room: ACTION_CALL_INTERNATIONAL

You can call any number of your international offices to take passenger flight jobs. They all either start or land in the city you are calling. Otherwise, the same rules as for the jobs picked up by ACTION_CHECKAGENT2 apply. Use the following function to check if a specific city can be called:

`bool GameMechanic::canCallInternational(PLAYER &qPlayer, SLONG cityId)`

Only while performing this action and after canCallInternational() was checked with the cityId, the global array AuslandsAuftraege[cityId] may be accessed. Never write to this array.

You can pick a flight job using:

`bool GameMechanic::takeInternationalFlightJob(PLAYER &qPlayer, SLONG cityId, SLONG jobId, SLONG &outObjectId)`

Job will be added to qPlayer.Auftraege and can be found using outObjectId.

### International jobs (freight jobs)

Each airline can purchase offices in other cities. They grant access to additional freight jobs.

Action ID to access room: ACTION_CALL_INTERNATIONAL

You can call any number of your international offices to take freight flight jobs. They all either start or land in the city you are calling. Otherwise, the same rules as for the jobs picked up by ACTION_CHECKAGENT3 apply. Use the following function to check if a specific city can be called:

`bool GameMechanic::canCallInternational(PLAYER &qPlayer, SLONG cityId)`

Only while performing this action and after canCallInternational() was checked with the cityId, the global array AuslandsFrachten[cityId] may be accessed. Never write to this array.

You can pick a freight job using:

`bool GameMechanic::takeInternationalFreightJob(PLAYER &qPlayer, SLONG cityId, SLONG jobId, SLONG &outObjectId)`

Job will be added to qPlayer.Frachten and can be found using outObjectId.

### Call via mobile phone

ACTION_CALL_INTER_HANDY can be used for a special “no-walk” international-call action; it does not require a location walk and is thus faster.
The item "phone" is required. Everything else said about ACTION_CALL_INTERNATIONAL also applies here.

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
- number of passengers must be ` <= qPlane.ptPassagiere` for passenger flight jobs
- freight jobs can split the total freight volume (`CFracht::Tons`) across multiple trips and/or planes. `CFracht::TonsLeft` tracks the number of tons still left
- distance between start city (`VonCity`) and target city (`NachCity`) must be ` <= qPlane.ptReichweite * 1000`
- flight duration must not exceed 24 hours (relevant for long distance flights and slow planes)

You can use the following helper functions to determine cost, duration and distance of a flight. The parameter `emptyFlight` has to be set if it is an automatic flight (game assumes a little bit of income from these which reduces cost). Note that this function only includes cost for passenger, freight and route jobs.
```
inline void calcCostAndDuration(int startCity, int destCity, const CPlane &qPlane, bool emptyFlight, int &cost, int &duration, int &distance) {
    assert(startCity >= 0 && startCity < Cities.AnzEntries());
    assert(destCity >= 0 && destCity < Cities.AnzEntries());
    /* needs to match CITIES::CalcFlugdauer() */
    distance = Cities.CalcDistance(startCity, destCity);
    duration = (distance / qPlane.ptGeschwindigkeit + 999) / 1000 + 1 + 2 - 2;
    if (duration < 2) {
        duration = 2;
    }

    /* needs to match CalculateFlightKerosin() */
    SLONG kerosene = distance / 1000            // weil Distanz in m übergeben wird
                     * qPlane.ptVerbrauch / 160 // Liter pro Barrel
                     / qPlane.ptGeschwindigkeit;

    /* needs to match CalculateFlightCostNoTank() */
    cost = kerosene * Sim.Kerosin;
    if (cost < 1000) {
        cost = 1000;
    }

    if (emptyFlight) {
        cost -= (qPlane.ptPassagiere * distance / 1000 / 40);
    }
}
```

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

Real-time concerns
------------------

Flights can be scheduled and plane schedules can be altered at any point in time when the player has a laptop available and it has no virus. These action can also be done in the player's office unless the office is currently unusable. If both office and laptop are unusable, no flights can be scheduled and no plane schedule can be altered.

Note that in case a flight job is picked up in one room and then the player has to walk to the office in order to schedule it, time will pass. This can make it impossible to schedule a flight that was supposed to start very soon. Plan accordingly if walking is required.

Routes
------

Routes have to be rented once and then as many flights on this route as desired can be added to any number of planes.

Familiarize yourself with the data structures:
- CRoute
- CRentRoute

The global array `Routen` contains all available routes and may only be accessed while in the "route box" room.

The array `qPlayer.RentRouten` has one instance of `CRentRoute` for each instance of `CRoute` in the global array at the same index. `CRentRoute` contains information regarding the player for the corresponding route, for example, if the player has rented this routes (`CRentRoute::Rang != 0`). This array may only be accessed while in the player's office or while having a functioning laptop.

### Route box

The action ID ACTION_VISITROUTEBOX or ACTION_VISITROUTEBOX2 shall be used to walk to the route box. All three functions in this section can only be done at the route box.

`bool GameMechanic::rentRoute(PLAYER &qPlayer, SLONG routeA)`

Rents a new route at the "route box" room. A daily fee has to be paid for each rented route.

Use the following function to check which routes are buyable. It returns an array with a boolean for each route at the corresponding index:

`BUFFER_V<BOOL> GameMechanic::getBuyableRoutes(PLAYER &qPlayer)`

A route always connects two cities. A route is buyable if either city is the home airport or either city is already connected by a different route that the player flies sufficiently enough.

Use this function to stop renting a route but ensure that no plane will be flying this route anymore beforehand:

`bool GameMechanic::killRoute(PLAYER &qPlayer, SLONG routeA)`

### Route mechanics

When renting a route from city A to city B, one does automatically rent the route in reverse from B to A. The GameMechanic automatically rents or kills the route in reverse direction. To find the routeId of the reverse direction for a given route, the following function shall be used:

`SLONG GameMechanic::findRouteInReverse(PLAYER &qPlayer, SLONG routeA)`

The ticket price for one direction of the route can set using the following function. There is a regular ticket price and one for first class (FC):

`bool GameMechanic::setRouteTicketPrice(PLAYER &qPlayer, SLONG routeA, SLONG ticketpreis, SLONG ticketpreisFC)`

The following function set for both direction at once:

`bool GameMechanic::setRouteTicketPriceBoth(PLAYER &qPlayer, SLONG routeA, SLONG ticketpreis, SLONG ticketpreisFC)`

These functions can be called while in the player's office or while having a functioning laptop.

The money gained from a single route flight is calculated by the game in `CFlugplanEintrag::BookFlight`. The most important factors are the set ticket price and the number of passengers. As opposed to regular flight jobs, for route flights the number of passengers is calculated dynamically in `CFlugplanEintrag::CalcPassengers` with `(ObjectType == 1)` and depends on price, competitors flying this route, current demand on this route, time of flight, the airlines's image and the specific image for the route and other factors.

Hint: Of these factors, ticket price and route image are the easiest to adjust and have a big impact on passenger count. 

HR / staffing actions
------------------------------------

Familiarize yourself with the data structure:
- CWorker

Use the action ID ACTION_PERSONAL to go to the HR office. This is the staff management room. Only while in this room, the following functions may be called.

Only while in this room, the global array `Workers.Workers` may be accessed. Only the CWorker instances where `Employer` equals `WORKER_JOBLESS` (can be hired) or `qPlayer.PlayerNum` (already hired) may be read.

`bool GameMechanic::hireWorker(PLAYER &qPlayer, SLONG workerId)`: Hire the worker with the given ID.


`bool GameMechanic::fireWorker(PLAYER &qPlayer, SLONG workerId)`: Fire the worker with the given ID.


`void GameMechanic::increaseAllSalaries(PLAYER &qPlayer)`: Increases salary for all workers by 10%.


`void GameMechanic::decreaseAllSalaries(PLAYER &qPlayer)`: Decreases salary for all workers by 10%.

`CWorker::Gehaltsaenderung(BOOL Art)`: Increases the salary for one worker by 10% (Art == true) or decreses by 10 % (Art == false).

Changes in salary affect worker happiness. There is a one-time effect and a chance of a regular increase/decrease in happiness if salary is above/below baseline salary.

ClaudeBot has to hire enough pilots and stewardesses. The required number of pilots depends on the plane type and is found in `CPlaneType::AnzPiloten`, required number of stewardesses in `CPlaneType::AnzBegleiter`.

Workers are paid daily, amount is `CWorker::Gehalt`/30. Worker talent (`CWorker::Talent`) affects customer satisfaction.

Workers might have quit over night. It should be checked regularly to see if there is a crew deficit. Assignment of crew to planes is done automatically. While it is possible to assign manually, we shall leave it to the automatic assignment and ClaudeBot only needs to ensure that enough people are hired in total.

Claude shall hire the following types of employees:
- pilots (CWorker::Typ == WORKER_PILOT)
- stewardesses (CWorker::Typ == WORKER_STEWARDESS)
- advisors (CWorker::Typ one of BERATERTYP_PERSONAL, BERATERTYP_KEROSIN, BERATERTYP_GELD, BERATERTYP_INFO, BERATERTYP_FLUGZEUG, BERATERTYP_FITNESS, BERATERTYP_SICHERHEIT)

For each type of advisor, only the one with the hightest `CWorker::Talent` is relevant. Advisors, depending on `Talent`, gate access to important information. The advisor BERATERTYP_SICHERHEIT gives up to 10% discount depending on `Talent` discount on various purchases (for example planes).

Office  actions
---------------

Use the action ID ACTION_BUERO or ACTION_UPGRADE_PLANES to go to the player’s own office. Only while in this room, the following functions may be called.

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

Analyze the function `CFlugplanEintrag::BookFlight` (variable `Add`) to see how plane upgrades affect customer satisfation, airline and route image, if applicable.

Kerosene  actions
-----------------

By default, kerosine is automatically bought for the regular market price. There is an opportunity to save money by buying kerosine manually and filling up the airline's tank.

If tanks are set to open and still full, the function `CFlugplanEintrag::BookFlight` deducts volume from the tank instead of buying for the market price.

The action IDs ACTION_VISITARAB and ACTION_BUY_KEROSIN_TANKS can be used to visit the Arab where tanks and kerosine can be bought. Only while in this room, the following functions may be called.

`bool GameMechanic::buyKerosin(PLAYER &qPlayer, SLONG type, SLONG amount)`: Buys kerosene for the tanks. Type is either 0, 1 or 2 and determines the quality (0="good", 1="normal", 2="bad") of the kerosene. Use the function `GameMechanic::calcKerosinPrice(PLAYER &qPlayer, __int64 type, __int64 amount)` to find out the price before buying.

`bool GameMechanic::buyKerosinTank(PLAYER &qPlayer, SLONG type, SLONG amount)`: Permanently increases the maximum tank capacity. Use `type` to select a tank size from global array `TankSize`. The price is found at the corresponding index in array `TankPrice`. Note that larger tanks are cheaper per volume unit. Multiple tanks can be bought at once using the parameter `amount`.

`SLONG SIM::HoleKerosinPreis(SLONG typ)`: Checks current market price for the different qualities of kerosene.

### Kerosene quality

The game updates the quality of the kerosene in the tank using the following formula:

`qPlayer.KerosinQuali = (oldAmount * qPlayer.KerosinQuali + amount * type) / qPlayer.TankInhalt;`

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

Use action ID ACTION_BUYNEWPLANE to walk to the plane broker. Only there, the following two functions may be used:

`bool GameMechanic::checkPlaneTypeAvailable(SLONG planeType)`: Checks if planes with the given type ID can be bought. You may check the global array `PlaneTypes` at index planeType if this function returns true.

`std::vector<SLONG> GameMechanic::getAvailablePlaneTypes()`: Returns a list of plane type IDs for all planes that can be bought. You may check the global array `PlaneTypes` at all indices returned by this function.

`std::vector<SLONG> GameMechanic::buyPlane(PLAYER &qPlayer, SLONG planeType, SLONG amount)`: Buys specified amount of planes of the given type.

Use action ID ACTION_BUYUSEDPLANE to walk to the museum. Only there, the following two functions may be used:

`SLONG GameMechanic::buyUsedPlane(PLAYER &qPlayer, SLONG planeID)`: Buy a used plane from the museum. You may check the global array `Sim.UsedPlanes[planeID]` for valid entries to find planes that can be bought.

`bool GameMechanic::sellPlane(PLAYER &qPlayer, SLONG planeID)`: Sells a plane to the museum. Ensure that no flights are scheduled for this plane before selling.

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

Branch offices can be called (ACTION_CALL_INTERNATIONAL or ACTION_CALL_INTERNATIONAL_HANDY) to get access to additional flight jobs starting or landing in their city. If you lost an auction to a competitor, there might be a later auction for another office in the same city.

Bids can be placed by ClaudeBot and all competitors the entire day. It is possible to overbid a competitor who made a bid on the same gate or city before. On the beginning of the next day, the airline that placed the last bid wins.

`bool GameMechanic::bidOnGate(PLAYER &qPlayer, SLONG idx)`: Places a bid on the gate at index `idx` in `TafelData.ByPositions`. Price increases by 10% after every bid.

`bool GameMechanic::bidOnCity(PLAYER &qPlayer, SLONG idx)`: Places a bid on the city office at index `idx` in `TafelData.ByPositions`. Price increases by 10% after every bid.

Item management
---------------

GameMechanic::PickUpItemResult GameMechanic::pickUpItem(PLAYER &qPlayer, SLONG item)
bool GameMechanic::removeItem(PLAYER &qPlayer, SLONG item)
bool GameMechanic::useItem(PLAYER &qPlayer, SLONG item)

GameMechanic::BuyItemResult GameMechanic::buyDutyFreeItem(PLAYER &qPlayer, UBYTE item):
ACTION_VISITDUTYFREE
Visit duty-free. Open only during business hours and not on weekend.


4) Shops and service rooms

ACTION_VISITKIOSK
Visit the kiosk room. This is a generic service-room action.

ACTION_VISITMUSEUM
Visit the museum. This is the room for used-plane purchases and related old-aircraft transactions.

ACTION_VISITTELESCOPE
Visit telescope room / “research” room. Same difficulty-gated behavior as the NASA room.

ACTION_VISITMAKLER
Visit the broker / airplane dealer room. This is where new aircraft can be bought.

ACTION_VISITRICK
Visit Rick’s room. This is a special service room.

5) Security / sabotage actions
These are the “consequence” actions that can affect the game state directly.

bool GameMechanic::setSecurity(PLAYER &qPlayer, SLONG securityType, bool targetState):
bool GameMechanic::toggleSecurity(PLAYER &qPlayer, SLONG securityType):
bool GameMechanic::sabotageSecurityOffice(PLAYER &qPlayer):
ACTION_VISITSECURITY / ACTION_VISITSECURITY2
Visit the security office. Security settings are handled through GameMechanic::setSecurity() / toggleSecurity(). The important constraints are:

SLONG GameMechanic::setSaboteurTarget(PLAYER &qPlayer, SLONG target):
GameMechanic::CheckSabotage GameMechanic::checkPrerequisitesForSaboteurJob(PLAYER &qPlayer, SLONG type, SLONG number, BOOL fremdSabotage);
bool GameMechanic::activateSaboteurJob(PLAYER &qPlayer, BOOL fremdSabotage);
the player must have a laptop;
security type must be valid.
ACTION_SABOTAGE / ACTION_VISITSABOTEUR
Sabotage action. The room is the sabotage office. The actual sabotage logic checks:

target validity;
whether the saboteur is idle;
trust level;
money;
whether the victim has a laptop;
whether the chosen attack is legal for that target.


8) Misc and convenience actions
Key constraints to keep in mind
The main gameplay constraints are not in the action name alone; they are enforced by the GameMechanic functions and the room-open gates:

Difficulty gates:

ACTION_VISITARAB, ACTION_BUY_KEROSIN, ACTION_BUY_KEROSIN_TANKS
ACTION_WERBUNG, ACTION_WERBUNG_ROUTES, ACTION_VISITADS
ACTION_VISITROUTEBOX, ACTION_VISITROUTEBOX2
ACTION_VISITNASA, ACTION_VISITTELESCOPE
ACTION_SABOTAGE, ACTION_VISITSABOTEUR
Time/weekday gates:

some rooms only open during set hours;
duty-free, kiosk, museum, advertisements, and agent rooms are all time-limited in BotHelper.cpp:686-796.
Resource/legality checks:

money for fuel, stock, sabotage, and plane purchases;
valid plane/route/airline identifiers;
enough trust for sabotage jobs;
laptop requirement for security and sabotage-related actions;
storage/credit capacity for fuel and loans.



Global game state read permissions
==================================

The game unfortunately stores its state in global variables. Thus there is a risk of accidently implement

ing a cheating computer player by:

- modifying a global variable directly when there is no player action that would have the same effect
- reading a global variable to learn something about the game's state that a human player would not be able to know

This section outline what can be accessed. Only access global variables and call global functions defined here.

If you think you should have access to a variable or you find that you are strongly limited by not having access, flag it to me and I might grant you access.

Global variables
----------------

### Global Sim instance

Global object that manages most of the game state

You have read access to:
- `Sim.Date`, `Sim.Time`, `Sim.GetHour()`, `Sim.GetMinute()`: Query in-game time.
- `Sim.Weekday`: Get current day of the week.
- `Sim.StartWeekday`: Get day of the week where game was started.
- `Sim.Difficulty`: Denotes whether we are in a free game or a mission. Always assume free game `Sim.Difficulty == -1`.
- `Sim.UsedPlanes`: List of used planes to buy. Access permitted while in museum.
- `Sim.HoleKerosinPreis()`: Fetches current price for kerosene. Only permitted while visiting the Arab.
- `Sim.HomeAirportId`: City ID of the home airport.
- `Sim.ItemZange`
- `Sim.ItemPostcard`

### Player objects (yourself)

- `Sim.Persons[Sim.Persons.GetPlayerIndex(playerNum)]`

### Player objects (competitors)


Global read-only helpers and tables
You may also read the following global tables and helpers when the rules permit them:

StandardTexte.GetS(...)
PlaneTypes[...]
Cities[...], Cities.AnzEntries(), Cities.find(...), Cities.CalcDistance(...), Cities.CalcFlugdauer(...)
Routen[...], Routen.AnzEntries()
Einheiten[...]
SeatCosts[...]
StationPrices[...]
RocketPrices[...]
Hdu.HercPrintfMsg(...) for logging
All of these are non-authoritative command paths. They are for observation and diagnostics only.

What the agent must never do
Do not write directly to any of the above global variables.
Do not modify Sim state except through the permitted GameMechanic and PLAYER API calls.
Do not use global arrays or tables outside the legal access window defined by the room/action rules.
Do not treat the bot’s own internal Bot state as a substitute for a legal game action.
Main working model
The bot’s main working object is the per-player PLAYER instance passed in as qPlayer.
Any legal game-state changes must happen through GameMechanic or through a legal PLAYER method/API call.
Access to job arrays and route arrays is only legal when the corresponding room/action is active and the relevant rules allow it.
Compliance rule
If a global variable or function is not listed here, assume it is forbidden. If you see a bot-side implementation reading or writing a forbidden global, stop and replace it with a legal interface call or a qPlayer/GameMechanic pattern.

StandardTexte

Access pattern: StandardTexte.GetS(...)
Classification: read-only
Evidence: localization table reads only.
AppPath

Access pattern: CString path{AppPath + MyPlanePath};
Classification: read-only
Evidence: string path lookup/concatenation only.
MyPlanePath

Access pattern: FullFilename(..., MyPlanePath) and AppPath + MyPlanePath
Classification: read-only
Evidence: path prefix for designer plane file materialization.
PlaneTypes

Access pattern: PlaneTypes[...], PlaneTypes.IsInAlbum(), PlaneTypes.AnzEntries(), PlaneTypes[i].Name, PlaneTypes[i].Geschwindigkeit
Classification: read-only
Evidence: bot reads plane catalog data.
Cities

Access pattern: Cities.AnzEntries(), Cities.find(...), Cities.CalcDistance(...), Cities.CalcFlugdauer(...), Cities[...]
Classification: read-only
Evidence: city graph / lookup / distance helpers are read from.
Routen

Access pattern: Routen.AnzEntries(), Routen[...], Routen[c].VonCity, Routen[c].NachCity, Routen[c].AnzPassagiere(), Routen[c].Miete
Classification: read-only
Evidence: route catalog is consumed, not written in Bot files.
Einheiten

Access pattern: Einheiten[EINH_KM]
Classification: read-only
Evidence: numeric/unit formatting table only.
RocketPrices

Access pattern: const auto &qPrices = (Sim.Difficulty == DIFF_FINAL) ? RocketPrices : StationPrices;
Classification: read-only
Evidence: Bot code selects this table and only reads price values.
StationPrices

Same as above, read-only.
SeatCosts

Access pattern: SeatCosts[0], SeatCosts[2], SeatCosts[qPlane.Sitze]
Classification: read-only
Evidence: cost formulas only.

Global functions
----------------

`Hdu.HercPrintfMsg(...)`: Read-only. Logging interface for ClaudeBot.

GameMechanic class
------------------

Found in src/GameMechanic.cpp

Main interface to perform actions in the game. This class was implemented as part of a larger refactoring to reduce write access to global variables. Thus most of this class is safe to use with exceptions listed in the following:

# Forbidden actions in GameMechanic

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
- Call GameMechanic::endStrike(PLAYER &qPlayer, EndStrikeMode mode) only with EndStrikeMode::Drunk as second parameter
- Only call GameMechanic::endStrike if item horse shoe has been given to the trinker (check via qPlayer.TrinkerTrust == TRUE)
- A call to GameMechanic::killFlightJob(PLAYER &qPlayer, SLONG par1, bool payFine) must use payFine == true
- A call to GameMechanic::killFreightJob(PLAYER &qPlayer, SLONG par1, bool payFine) must use payFine == true

Player class
------------

You can access everything in the PLAYER class instance that refers to your player. A reference to this instance is passed as variable qPlayer.

All instances of the PLAYER class can be found in the global array `Sim.Players.Players`.




action IDs

operative saldo

gates

home airport and niederlassungen

bankruptcy

album check for valid entries

saving

Open questions / ambiguities
-----------------------------

Use this section to list any open questions or ambiguities regarding rules that you need me to clarify.
