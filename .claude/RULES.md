The Game
========

Airline Tycoon Deluxe is a resource-management/tycoon game where you control the airline manager during the day. You walk in the airport from room to room and talk to different people in different rooms to perform specific actions. You buy planes, upgrade them, hire staff, take flight jobs and schedule flights. Flights bring you money which you can use to buy more planes.

The game simulates a day/night cycle. You work from 9 am to 18 pm and can only perform actions during this time. The game state however advanced during night: Planes start and land as scheduled, money made or lost as usual.

There are always four competing airlines:
- "Sunshine Airways" (SA)
- "Falcon Lines" (FL)
- "Phoenix Travel" (PT)
- "Honey Airlines" (HA)

ClaudeBot will play as "HoneyAirlines". Airlines are enumerated starting at 0 in the order given.

We will list now all actions that can be performed in the game via the class GameMechanic.
If GameMechanic returns a bool this usually means whether or not the action could be completed.
In the following, qPlayer always is a reference to the instance of the Player class which refers to Honey Airlines.

Note that most actions have to performed in a particular room. ClaudeBot shall set the desired action ID in RobotPlan() and the game will walk the character to the correct room. On arrival, RobotExecuteAction() will be called.
- ClaudeBot shall use the following function to check if the room is open: bool checkRoomOpen(SLONG roomId)
- ClaudeBot shall use the following to translate an action ID to a room ID: SLONG getRoomFromAction(SLONG PlayerNum, SLONG actionId)
- ClaudeBot shall check that it is in the correct room before performing an action using the function: qPlayer.GetRoom()
- If it is not the correct room, only print a warning for now and do still perform the action

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
Analyze the code to see how the mode affects how much money is made and by how much the stock drops.

Recommended action ID: ACTION_EMITSHARES

### Set dividend

`bool GameMechanic::setDividend(PLAYER &qPlayer, SLONG dividend)`

Set the dividend for the airline’s stock. Higher dividend costs more money but improves stock price. Note that increases in dividend have a delay before they take effect.

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
The function canOvertakeAirline() checks whether the target is valid, whether you have enough stock (>= 50%), and whether the enemy blocks acquisitation by owning stock from your airline (>= 30%).

Recommended action ID: ACTION_OVERTAKE_AIRLINE

### Other

Action ID ACTION_VISITBANK can be used for a generic action for bank interactions.


Flight jobs / freight jobs actions
---------------------------------

These actions send the bot to the special offices where flight jobs can be viewed and taken.

### Last minute jobs

Action ID to access room: ACTION_CHECKAGENT1

Only while performing this action, the global array LastMinuteAuftraege may be accessed. Never write to this array.
Browse this array to find suitable flights. All flights here can go from any city to any other city and typically need to be completed either today or tomorrow. Plane must have required number of seats at least. Plane needs to be able to travel the required distance. There is a premium if completed on time. If flight has been taken but not completed on time, a fine has to be paid. Some jobs have a fine of zero.
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


Flight planning
----------------

Flight jobs that have been taken shall be planned. If not, they will expire and this might incur a fine.

Flights can only be planned in the player's office or when the item "laptop" is available.
To walk to your office, use the action ID ACTION_BUERO. Note that the office is only usuable when `(qPlayer.OfficeState != 2)`.
The laptop can be used at any point during any action as long as the condition `qPlayer.HasItem(ITEM_LAPTOP) && (qPlayer.LaptopVirus == 0)` holds (laptop available and no virus).

A list of planes is found in `qPlayer.Planes`. Each plane has a flight plan object `Flugplan`.  Each flight plan object has a chronologically sorted list of flights in the array `Flug`.
The items in this array have the type `CFlugplanEintrag ` and are only referring to an actual flight if their `ObjectType` is larger than 0.

A flight plan object cannot be altered if the plane has already started or the start happens until the next in-game hour. Check using this expression: `qFPE.Startdate == Sim.Date && qFPE.Startzeit <= Sim.GetHour() + 1` where `qFPE` is a reference to a flight plan object. If this is the case, the entry is considered to be locked.

You can use the following helper functions:
- class `PlaneTime` stores both a date and a time and provides proper operator overloading for adding/subtracting time and comparisons
- `const CFlugplanEintrag *getLastFlight(const CPlane &qPlane)`: Returns a pointer to the last valid flight plan object
- `const CFlugplanEintrag *getLastFlightNotAfter(const CPlane &qPlane, PlaneTime ignoreFrom)`: Returns a pointer to the last valid flight plan object when flights after a certain time are ignored. This is useful when the intention is to replan a flight schedule but you do not want to touch flights that are scheduled for takeoff very soon given the fact that the game is real-time.
- `std::pair<PlaneTime, int> getPlaneAvailableTimeLoc(const CPlane &qPlane, std::optional<PlaneTime> ignoreFrom, std::optional<PlaneTime> earliest)`: Return both time and location when the plane is available, meaning it has landed and is available for the next flight. As above, there is an option for a cutoff if the intention is to replan. The earliest returned time will be the next full hour or the optional argument `earliest` if it contains a later point in time.

Do not modify anything in the plane, flight plan or flight plan object classes directly. Instead, use the following functions:
- `bool GameMechanic::killFlightJob(PLAYER &qPlayer, SLONG par1, bool payFine)`: Remove a passenger flight from the backlog and pay fine immediately. Note that it is assumed the flight has been removed from the plane schedule already.
- `bool GameMechanic::killFreightJob(PLAYER &qPlayer, SLONG par1, bool payFine)`: Remove a freight job from the backlog and pay fine immediately. Note that it is assumed the flight has been removed from the plane schedule already.
- `bool GameMechanic::removeFromFlightPlan(PLAYER &qPlayer, SLONG planeId, SLONG idx)`: Remove a flight plan entry from the plane identified by planeId at position idx in the plan.
- `bool GameMechanic::clearFlightPlan(PLAYER &qPlayer, SLONG planeId)`: Remove all flight plan entries from the plane identified by planeId which are not yet locked.
- `bool GameMechanic::clearFlightPlanFrom(PLAYER &qPlayer, SLONG planeId, SLONG date, SLONG hours)`: Remove all flight plan entries from the plane identified by planeId after the given time.
- `bool GameMechanic::planFlightJob(PLAYER &qPlayer, SLONG planeID, SLONG objectID, SLONG date, SLONG time)`: Attempts to place the given flight (passenger job) for the given plane and time into the plane's flight schedule.
- `bool GameMechanic::planFreightJob(PLAYER &qPlayer, SLONG planeID, SLONG objectID, SLONG date, SLONG time)`: Attempts to place the given flight (freight job) for the given plane and time into the plane's flight schedule.
- `bool GameMechanic::planRouteJob(PLAYER &qPlayer, SLONG planeID, SLONG objectID, SLONG date, SLONG time)`: Attempts to place the given flight (route job) for the given plane and time into the plane's flight schedule.

TODO:
fuel
empty flight

Flight picking and planning
---------------------------

Routes
------

- `bool GameMechanic::setRouteTicketPrice(PLAYER &qPlayer, SLONG routeA, SLONG ticketpreis, SLONG ticketpreisFC)`: 
- `bool GameMechanic::setRouteTicketPriceBoth(PLAYER &qPlayer, SLONG routeA, SLONG ticketpreis, SLONG ticketpreisFC)`: 

walk to office?

Office / personal / staffing actions
------------------------------------
These are administrative actions.

ACTION_STARTDAY
Start the day by going to the player’s own office. This is a standard “begin day” room action.

bool GameMechanic::killCity(PLAYER &qPlayer, SLONG cityID):
bool GameMechanic::toggleKerosinTankOpen(PLAYER &qPlayer):
bool GameMechanic::setKerosinTankOpen(PLAYER &qPlayer, BOOL open):
ACTION_BUERO
Same office/general office room in multiplayer or split-room contexts.

void GameMechanic::increaseAllSalaries(PLAYER &qPlayer):
void GameMechanic::decreaseAllSalaries(PLAYER &qPlayer):
bool GameMechanic::hireWorker(PLAYER &qPlayer, SLONG workerId):
bool GameMechanic::fireWorker(PLAYER &qPlayer, SLONG workerId):
ACTION_PERSONAL
Go to the HR/personal office. This is the staff management room.

ACTION_UPGRADE_PLANES
Upgrade planes; mapped to the office block. This is a plane-maintenance/admin action.

4) Shops and service rooms
These are mostly “visit a room, then resolve the associated business interaction there”.

ACTION_VISITARAB
Go to the Arab Air / fuel office. This is the room for kerosene and related fuel purchases. The real constraints are:

difficulty must be easy or free game;
opens only at timeArabOpen;
not on Saturday.
ACTION_BUY_KEROSIN
Same room, but specifically buy kerosene. GameMechanic::buyKerosin() requires:

bool GameMechanic::buyKerosin(PLAYER &qPlayer, SLONG type, SLONG amount):
valid kerosene type;
positive amount;
enough storage capacity;
enough money.
GameMechanic::KerosinTransaction GameMechanic::calcKerosinPrice(PLAYER &qPlayer, __int64 type, __int64 amount)

bool GameMechanic::buyKerosinTank(PLAYER &qPlayer, SLONG type, SLONG amount):
ACTION_BUY_KEROSIN_TANKS
Same room, but buy fuel tanks. GameMechanic::buyKerosinTank() requires:

valid tank type;
positive amount;
enough space or tank capacity;
enough money.
ACTION_VISITKIOSK
Visit the kiosk room. This is a generic service-room action.

bool GameMechanic::setPlaneTargetZustand(PLAYER &qPlayer, SLONG idx, SLONG zustand):
SLONG GameMechanic::setMechMode(PLAYER &qPlayer, SLONG mode):
ACTION_VISITMECH
Visit the mechanic/workshop. This is the room for airplane maintenance and repair-related choices.

ACTION_VISITMUSEUM
Visit the museum. This is the room for used-plane purchases and related old-aircraft transactions.

GameMechanic::BuyItemResult GameMechanic::buyDutyFreeItem(PLAYER &qPlayer, UBYTE item):
ACTION_VISITDUTYFREE
Visit duty-free. Open only during business hours and not on weekend.

bool GameMechanic::bidOnGate(PLAYER &qPlayer, SLONG idx):
bool GameMechanic::bidOnCity(PLAYER &qPlayer, SLONG idx):
ACTION_VISITAUFSICHT
Visit airport authority / airport expansion room. This is the place for airport expansion and related authority interactions.

ACTION_VISITNASA
Visit NASA room; only available in the later difficulty settings (DIFF_FINAL or DIFF_ADDON10).

ACTION_VISITTELESCOPE
Visit telescope room / “research” room. Same difficulty-gated behavior as the NASA room.

ACTION_VISITMAKLER
Visit the broker / airplane dealer room. This is where new aircraft can be bought.

ACTION_VISITRICK
Visit Rick’s room. This is a special service room.

BUFFER_V<BOOL> GameMechanic::getBuyableRoutes(PLAYER &qPlayer):
bool GameMechanic::killRoute(PLAYER &qPlayer, SLONG routeA):
bool GameMechanic::rentRoute(PLAYER &qPlayer, SLONG routeA):
ACTION_VISITROUTEBOX / ACTION_VISITROUTEBOX2
Visit route-box. This is the route-management room, with access gated by difficulty.

SLONG GameMechanic::findRouteInReverse(PLAYER &qPlayer, SLONG routeA):

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


GameMechanic::ExpandAirportResult GameMechanic::canExpandAirport(PLAYER & /*qPlayer*/):
bool GameMechanic::expandAirport(PLAYER &qPlayer):
ACTION_EXPANDAIRPORT
Expand the airport. This is the airport-authority branch of the same room family, and there are explicit legality checks like “not already expanded”, “limit reached”, “too early”, “expanding right now”.

6) Plane purchase actions
These are direct buy actions, usually routed to either the museum or the broker.

ACTION_BUYUSEDPLANE
SLONG GameMechanic::buyUsedPlane(PLAYER &qPlayer, SLONG planeID):
Buy a used plane from the museum. Room is the museum. Requires a valid plane ID and allowed engine/airline rules.

ACTION_BUYNEWPLANE
bool GameMechanic::checkPlaneTypeAvailable(SLONG planeType)
std::vector<SLONG> GameMechanic::getAvailablePlaneTypes()
std::vector<SLONG> GameMechanic::buyPlane(PLAYER &qPlayer, SLONG planeType, SLONG amount):
Buy a new airplane from the broker. Room is the dealer/makler. The availability gate is timeMaklClose.

bool GameMechanic::sellPlane(PLAYER &qPlayer, SLONG planeID)
std::vector<SLONG> GameMechanic::buyXPlane(PLAYER &qPlayer, const CString &filename, SLONG amount) 

7) Advertisement / marketing actions
------

bool GameMechanic::buyAdvertisement(PLAYER &qPlayer, SLONG adCampaignType, SLONG adCampaignSize, SLONG routeA):
ACTION_WERBUNG
General ad campaign action. It is routed to the advertising room, and the room is open only during business hours.

ACTION_WERBUNG_ROUTES
Route-focused advertising campaign.

ACTION_VISITADS
Another ad-room visit variant, with the same general room-open gating.

8) Misc and convenience actions
ACTION_STARTDAY_LAPTOP
“Start day with laptop” style no-walk action; it is treated as a zero-location action.

ACTION_CALL_INTER_HANDY
Another no-walk/high-priority action that acts like a quick international call.

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

Item management
---------------

GameMechanic::PickUpItemResult GameMechanic::pickUpItem(PLAYER &qPlayer, SLONG item)
bool GameMechanic::removeItem(PLAYER &qPlayer, SLONG item)
bool GameMechanic::useItem(PLAYER &qPlayer, SLONG item)


Global game state read permissions
==================================

GameMechanic class
------------------

Found in src/GameMechanic.cpp

Main interface to perform actions in the game.

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

Open questions / ambiguities
-----------------------------

Use this section to list any open questions or ambiguities regarding rules that you need me to clarify.