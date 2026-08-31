#include "ClaudeBot.h"

#include "BotHelper.h"
#include "Proto.h"
#include "global.h"
#include "helper.h"

#include <SDL_log.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <utility>
#include <vector>

template <class... Types> void AT_Error(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_ERROR, "ClaudeBot", args...); }
template <class... Types> void AT_Warn(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_WARN, "ClaudeBot", args...); }
template <class... Types> void AT_Info(Types... args) { Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_INFO, "ClaudeBot", args...); }
template <class... Types> void AT_Log(Types... args) { AT_Log_I("ClaudeBot", args...); }

/* used to calculate a sliding average in CRentRoute exclusively for ClaudeBot.
 * Value may be changed. */
const SLONG kRouteAvgDays = 3;

/* Upper bound on jobs accepted per day. Acceptance is really limited by the simulated
 * flight plan (we only take what we can fly); this is just a safety net. */
static const SLONG kMaxJobsPerDay = 20;

/* How often we go back to the travel agency and the freight depot in one day.
 *
 * The boards hold six contracts each and SIM::DoOneStep refills one empty slot every five
 * in-game minutes (Sim.cpp:1341-1358, GameMechanic::flightJobsRefill), so over a working
 * day the agency offers far more than six jobs - it just never shows more than six at a
 * time. Visiting once and latching "nothing more fits" threw all of that away. The fleet
 * now parks twenty planes at home overnight, which is about 140 idle windows a week
 * against the eight jobs a single visit was finding. */
static const SLONG kMaxAgencyVisitsPerDay = 10;

/* Minimum profit (premium minus kerosene for the flight and for the empty leg needed to
 * reach its start) for a job to be worth accepting. */
static const SLONG kMinJobGain = 1000;

/* Hours left free at the end of an idle window a job is fitted into. A job that overruns
 * its window pushes the following route leg later, which costs a sixth of its passengers
 * per night hour, so the window is never filled to the brim. */
static const SLONG kJobGapSlack = 1;

/* Shortest idle window worth remembering: nothing can be flown and returned in less. */
static const SLONG kMinGapHours = 5;

/* Freight.
 *
 * A freight flight carries CPlane::ptPassagiere / 10 tons (BookFlight, Schedule.cpp:707)
 * and the premium is paid *only* on the flight that brings TonsLeft to zero, so a job is
 * worth nothing at all until the last ton is delivered. Every leg has to depart inside
 * [Date, BisDate] as well (UpdateFrachtauftragsUsage, Player.cpp:2366-2371), and for the
 * most common contract type those two are the same day. A job is therefore only accepted
 * if the whole tonnage fits into the idle windows we already know about. */
static const bool kUseFreight = true;

/* Upper bound on freight contracts accepted per day; the tonnage limit binds first. */
static const SLONG kMaxFreightPerDay = 4;

/* Legs one contract may occupy. A 126 seat plane carries 12 tons, so a 100 ton contract
 * would need nine of them - more idle windows than the whole fleet has in a week. */
static const SLONG kMaxFreightLegs = 6;

/* Refit charge for one idle window that hosts freight.
 *
 * BookFlight flips CPlane::OhneSitze whenever a plane flies a job of the other kind and
 * charges 15,000 under FlugzeugUmbau, which *is* part of GetOpVerlust() (Schedule.cpp:
 * 782-787). A window between two route legs pays it twice: once going into the freight
 * configuration and once coming back out. Automatic flights (ObjectType 3) are exempt, so
 * the empty return the game inserts costs nothing extra. */
static const SLONG kFreightRefitCost = 2 * 15000;

/* What one idle window is worth to the rest of the airline, charged per freight leg.
 *
 * The windows are not free capacity: the travel agency wants them too. Across three
 * measurements the passenger job count fell in step with the freight taken - 128 jobs a
 * game with no freight, then 123 / 118 / 116 as freight went to 3 / 5 / 11 contracts - so
 * roughly one passenger job is given up per three freight legs. A job nets on the order of
 * 50,000 after its kerosene, which puts the charge near 20,000 a leg.
 *
 * Route revenue also came out 1.4 to 3.2 million below the baseline in all three runs, but
 * it did not scale with the freight taken (the run with the fewest contracts had the lowest
 * route revenue of the three), so that part is noise and is deliberately not priced in. */
static const SLONG kFreightLegOpportunityCost = 20000;

/* Minimum profit (premium minus kerosene for every leg, minus the refit charge, minus the
 * opportunity cost above) for a freight contract to be worth accepting. */
static const SLONG kMinFreightGain = 20000;

/* Advisors we employ.
 *
 * Only the highest Talent of a type counts (PLAYER::HasBerater, Player.cpp:2224), so one
 * of each is enough and a second is only hired to replace a worse one.
 *
 * BERATERTYP_SICHERHEIT is the bodyguard: PLAYER::DoBodyguardRabatt() refunds
 * Talent / 10 percent of every plane, tank and kerosene purchase, capped at 10% and
 * inactive below Talent 21 (Player.cpp:6373-6390). Note what it does *not* do for the
 * score: the purchase is booked at full price (KerosinVorrat, scored) and the refund lands
 * under category 3130 / Bilanz.BodyguardRabatt, which GetOpVerlust() ignores. So the
 * discount is cash, not score, while the salary is scored - see DECISIONS.md. */
/* The other two are hired for what they let the bot legally *read*, not for an in-game
 * effect. RULES.md gates `qPlayer.BilanzGestern` behind a financial advisor of any talent
 * and `qPlayer.TankInhalt` behind a kerosene advisor above talent 30; the fuel manoeuvre
 * needs both. A salary is trivial against what it protects - `Personal` is 3.3 million of a
 * two billion week - and one of each is enough, because PLAYER::HasBerater() only ever
 * reports the highest talent employed. */
struct AdvisorWanted {
    SLONG typ;
    SLONG minTalent;
};

/* Talent below the bodyguard's threshold buys nothing: DoBodyguardRabatt() returns early at
 * 20 or less and the discount is Talent / 10 percent, so 30 is the first talent worth a
 * salary. The other two only have to clear the thresholds RULES.md names. */
static const AdvisorWanted kAdvisors[] = {
    {BERATERTYP_SICHERHEIT, 30},
    {BERATERTYP_GELD, 1},
    {BERATERTYP_KEROSIN, 31},
};

/* Kerosene stock.
 *
 * Sim.Kerosin is a random walk bounded to [300, 700] (SIM::NewDay, Sim.cpp:2330-2344) and
 * both KerosinVorrat (stocking up) and KerosinFlug (buying at the gate) are part of
 * GetOpVerlust(), while the tanks themselves are not. Every unit bought below the price it
 * would otherwise have been burnt at is therefore score, and the tank is free.
 *
 * Grade 1 is the only sane grade: a flight always buys at HoleKerosinPreis(1), so grade 0
 * at twice the price is a pure loss, and grade 2 at half the price multiplies airframe
 * wear elevenfold (faktorKerosin = 1 + 10 * (quali - 1)^2, Schedule.cpp:1017). */
static const SLONG kKerosinGrade = 1;


/* Kerosene is now bought when it is cheap, not when the calendar says so.
 *
 * Under the seven-day objective the whole point of the tank was that `KerosinVorrat` landed
 * on a day the score never read, so fuel burnt in the scored week was free. That trick is
 * gone: the objective cumulates every day, so stock is charged wherever it is bought.
 *
 * What is left is genuine arbitrage, and it is bigger than the trick was. `Sim.Kerosin` is a
 * random walk clamped to [300, 700] (SIM::NewDay, Sim.cpp:2330-2344), the airline burns 671
 * million of fuel over a game, and the tank itself is *not* scored - `ExpansionTanks` is
 * capital, outside GetOpVerlust(). So every unit bought below the price it would otherwise
 * have been burnt at is score, and the container is free.
 *
 * The rule is therefore: hold capacity for a few days of flying, fill it whenever the spot
 * price is in the lower part of the band, and burn out of it when the price is high. */
static const SLONG kKerosinPriceMin = 300;
static const SLONG kKerosinPriceMax = 700;

/* Stock up when the spot price is at or below this percentage of its running mean.
 *
 * Measured rather than assumed, and the assumption was wrong: `Sim.Kerosin` is clamped to
 * [300, 700] but a game never visits the lower half of that. It opens at 500 (Sim.cpp:590)
 * and drifts up - over 99 days, min 500, max 700, mean 603, with only 2% of days at or below
 * 500 and 16% at or below 550. A fixed threshold picked from the clamp bounds never fires at
 * all, which is exactly what the first version did.
 *
 * So the trigger is relative to what the price has actually been doing. */
static const SLONG kKerosinBuyBelowPercent = 95;

/* Whether to run the tank at all. Off - see the arithmetic in DECISIONS.md session 18.
 *
 * TankPrice/TankSize make capacity cost 600 a unit at the cheapest tank, which is about what
 * one unit of kerosene costs: **the container costs a full fill of itself**. The trigger above
 * captures roughly 5% of the price, so breaking even needs about twenty cycles of the tank,
 * and seven days of capacity over a hundred days gives at most fourteen - before counting that
 * the cash would otherwise have bought aeroplanes, which is what the objective actually pays
 * for. Measured: a 270.6M tank that then sat 99.7% empty, and fifteen fewer aeroplanes. */
static const bool kUseFuelArbitrage = false;

/* Tank capacity to hold, in days of yesterday's burn. Capacity is free of the score but not
 * of cash, and cash is what buys aeroplanes - so this is deliberately a few days of cover,
 * not a strategic reserve. */
static const SLONG kTankDaysOfBurn = 7;

/* Smallest top-up worth a trip to the Arab.
 *
 * It is tempting to raise this to 10,000 for the bulk discount calcKerosinPrice() grants
 * (5% from 10,000 units, 10% from 50,000). Measured at -171,584 against +3,278,875 for
 * small top-ups: the score is the operating saldo of the *last* week, and a single 10,000
 * unit purchase is five to seven million of KerosinVorrat landing in one day. Whenever a
 * refill falls inside the final week it swamps everything else. A certain 5% discount is
 * not worth turning a smooth cost into a lumpy one. */
static const SLONG kKerosinMinPurchase = 100;

/* Cash kept back from kerosene so the route image and the fleet still get funded. */
static const __int64 kKerosinCashReserve = 1000000;

/* Dividend per share and year, capped at 25 by GameMechanic::setDividend(). The share
 * price converges on ten times this, so it sets what an emission is worth. */
static const SLONG kDividend = 25;

/* Share issues raise enormous amounts (392 million over 100 days, measured) and none of
 * it is scored - but the cash is only worth having if the flights it buys make money, and
 * right now they do not: with 25 planes spread over 24 routes the fleet carried 93
 * passengers a flight and ticket revenue (123 million) came in *below* the kerosene bill
 * (127 million). Enabling this measured -9,761,028 against a baseline of +530,990.
 * Turn it back on once a flight is profitable - see DECISIONS.md. */
static const bool kEmitStock = true;

/* Below this much cash we top up at the bank. */
static const __int64 kCashBuffer = 500000;

/* Working capital a plane purchase leaves untouched, for kerosene and wages. */
static const __int64 kPlaneCashReserve = 800000;

/* Repair rate of the mechanic we employ (3 = "Diplom-Dingsbums", 15-18 points a night).
 * Anything slower cannot keep a busy plane above the accident threshold of 80. */
static const SLONG kMechMode = 3;

/* Route rent is charged daily whether we fly or not and it *is* part of the operating
 * result, so the fleet must be able to keep every rented route busy. Two planes per pair
 * measured 657,245 against 614,517 for one per pair - a difference well inside the noise
 * of the measurement (standard error 80,000), so the cheaper option stands. A single pair
 * does saturate: CRouten::NewDay() regenerates demand by a seventh a day while every
 * economy passenger consumes it (Schedule.cpp:855), which is why the load per flight sits
 * near 84 of ~180 seats. The extra rent simply cancels the extra demand at this scale. */
static const SLONG kPlanesPerRoute = 2;

/* Lowest value per plane hour a route pair has to promise before we rent it.
 *
 * The bar used to be zero, which let in pairs like Berlin-Warsaw at 18 an hour, and the
 * flight log shows what those are worth: -7,123 dollars on a 343 mile leg, because the
 * ticket price scales with distance while the minimum kerosene charge does not.
 *
 * Raised from 18,000 once the rent term stopped being overstated thirtyfold. That fix
 * lifted every route's value, so the old bar admitted Berlin-Brussels at 18,541 and
 * Berlin-Paris at 18,185 - short pairs whose tickets are cheap because the price scales
 * with distance. Measured across the range: 10,000 -> 45.5M, 18,000 -> 55.4M,
 * 25,000 -> 57.8M, 35,000 -> 59.3M, 45,000 -> 62.2M, 50,000 -> 62.7M, 55,000 -> 59.6M
 * (45,000 and 18,000 averaged over two runs; the rest single, and the run-to-run spread
 * is about 2M).
 *
 * Re-swept once the network reached past the home airport, where the pairs on offer stopped
 * being Berlin's leftovers: 45,000 -> 1,204.9M, 25,000 -> 1,214.6M, 10,000 -> 1,233.4M,
 * 0 -> 1,223.4M. The bar has almost nothing left to do - the marginal-value ranking in
 * scheduleRouteFlights() decides which pairs actually get hours, and a pair it never
 * chooses costs only its rent - so it is kept low but not at zero, where a pair worth
 * nothing at all is still rented and still charged. */
static const SLONG kMinRouteValuePerHour = 10000;

/* Route pairs wanted per hundred aeroplanes, and a ceiling on the pairs held whatever the
 * fleet does.
 *
 * Re-swept once aeroplanes were bought for the load they carry: 22 -> 1,825.0M,
 * 30 -> 1,891.0M, 37 -> 1,933.4M, 45 -> 1,939.9M / 1,934.0M, 50 -> 1,931.0M,
 * 60 -> 595.4M.
 *
 * That last figure is not a plateau falling away, it is the rounding. The target is rounded
 * *up*, so 60 asks a two-aeroplane airline for two route pairs where 45 asks for one, and
 * the second pair's rent on day three is enough to cost the game. Rounding down instead
 * removes the cliff and costs 160M at the top of the range (45 -> 1,776.9M,
 * 60 -> 1,798.5M): the early network wants to be as large as the fleet can just about keep
 * busy, and one pair either way decides it. */
static const SLONG kRoutePairsPerHundredPlanes = 38;
/* Re-measured on the clean harness (session 17d) once the fleet had grown past 190: 45 was
 * chosen when the airline was half the size. 38 -> +34.0M against 45. The fleet is now large
 * enough that concentration beats spread - a pair flown by more aeroplanes amortises its
 * (scored) rent over more departures and drains more of its demand pool, while a thin pair
 * pays rent for legs nobody fills. Do not read this as "fewer is always better": the same
 * batch measured kMinRouteValueShare 60 at -114.9M, and that is the rule stopping an
 * aeroplane wandering onto a weak pair, so the two work together. */
static const SLONG kMaxRoutes = 200;

/* Fleet cap. wantRoutes derives the number of route pairs from the fleet size, so this
 * caps the network as well as the aircraft.
 *
 * Swept twice. With the crew shortage still in place it peaked at 110 - past that the
 * airline was buying aeroplanes nobody could fly. Once executePersonal() started hiring
 * every applicant it moved out to 170: 110 -> 1,488M, 170 -> 1,578.1M, 260 -> 1,575.9M.
 * The plateau past 170 is cash: the airline finishes with 145 aeroplanes either way. */
static const SLONG kMaxPlanes = 200;

/* Days of flying the aeroplane ranking charges an aircraft's fuel over, alongside its
 * price. */
static const SLONG kPlaneHorizonDays = 20;

/* Planes we want in the air before we start holding cash back for a better type. Below this
 * the airline has nothing earning, so anything that flies beats waiting. */
static const SLONG kMinFleetBeforeSaving = 2;

/* Hours of the day scheduleRouteFlights() can place a leg in. It keeps every departure and
 * every landing inside 05:00-22:00 to dodge the night penalty, so the flying day is
 * seventeen hours long and a leg either fits twice into it or does not. */
static const SLONG kUsableHoursPerDay = 17;

/* Share of the waiting queue one departure actually carries away, in percent. See
 * legTakes() in scheduleRouteFlights(): the price factor (3B - 10) / Ticketpreis is 1/1.9
 * and the image factor (400 + 1000) / 1100 is 1.27, so a departure takes about two thirds
 * of what is standing on the platform and the next one takes two thirds of the rest. */
static const SLONG kPoolCaptureSharePercent = 67;

/* How close to the best pair an aeroplane can reach another pair has to be, in percent,
 * before that plane is allowed to fly it - see scheduleRouteFlights(). Guards the
 * demand-weighted spreading against a pair that is only in the network for a plane that
 * cannot reach anything better.
 *
 * Swept again on the cumulative objective: 60 -> -114.9M, 80 was the old setting, 90 ->
 * +165.9M (and Firmenwert +333M). Tightening keeps paying, which is the same story the route
 * network tells - concentration beats spread now that the fleet is large. The two are a pair:
 * `kRoutePairsPerHundredPlanes` at 38 only works because this gate is tight enough to stop an
 * aeroplane wandering onto the weakest pair it can reach. Session 15 recorded a cliff at 95,
 * measured on a much smaller airline under the seven-day objective - worth retesting. */
static const SLONG kMinRouteValueShare = 90;

/* Economy passengers the plane valuation credits a route flight with.
 *
 * Not a cabin size: a ceiling on what a departure is expected to sell. It decides which
 * aircraft the broker buys, because every type above it is credited with the same revenue
 * and then separates on what it costs.
 *
 * It was 400, set when the fleet was small enough that its cabins filled. They do not any
 * more - the airline flies each pair many times a day and a departure carries 162
 * passengers - so 400 was paying for seats that never fly. Swept against the fuel-aware
 * denominator below: 180 -> 1,661.4M, 250 -> 1,876.3M / 1,855.3M, 300 -> 1,607.5M,
 * 400 -> 1,605.8M. The peak is sharp because it is a discrete choice: at 250 an Ilyushin
 * Il 86 and an Airbus A 300 are credited with the same load, and the Il 86 costs 17.1
 * million against 28.1.
 *
 * Do not lower it without the fuel term: on price alone the cheapest airframe is an Il 62
 * at 9.9 million and 10,500 litres an hour, and buying those bankrupted the airline. */
static const SLONG kExpectedPaxPerFlight = 250;

/* Ticket price as a percentage of the threshold the game considers extortionate. Prices in
 * this game are absolute money; this constant only says where we sit relative to that
 * derived threshold, which is 3 * routePriceBase() (CalcPassengers, Schedule.cpp:338-346).
 *
 * Revenue rises linearly up to the threshold and is flat above it, because passengers then
 * scale by (3B - 10) / price. So overshooting costs little revenue directly - what it costs
 * is one image point per flight (BookFlight, Schedule.cpp:965-977) plus the step penalties
 * on the demand weight at 3x/5x/6x cost. The threshold is recomputed from Sim.Kerosin at
 * flight time, and kerosene moves at every day boundary, so a price set today can be above
 * the threshold on the day the flight actually runs. */
static const SLONG kTicketPriceThresholdPercent = 190;

/* The same, for first class, whose threshold is 9 * routePriceBase() rather than 3
 * (CalcPassengers, Schedule.cpp:509-514).
 *
 * Economy pays to sit above its threshold because every economy seat sells anyway. First
 * class does not: the flight log books 5 of the ~21 first class seats a Boeing 720 has, so
 * the cabin is demand-limited and overpricing it just empties it further. */
static const SLONG kTicketPriceThresholdPercentFC = 60;

/* Passenger food quality, 0 to 2. Costs FoodCosts[level] per passenger on every flight and
 * that cost is scored - 2.3 million in the last week of the measurement - but cutting it
 * is much more expensive than that. Level 0 measured 10,371,466 and level 1 10,813,119
 * against 18,383,803 for level 2.
 *
 * The reasoning that said otherwise was wrong in an instructive way. Food is worth three
 * points of the cabin score (Schedule.cpp:937-956), the cabin score reaches the airline
 * only as `Image += Add / 10`, and ImageTotal is clamped at 1000 in CalcPassengers - so the
 * points looked free. They are not: that clamp is on the *total*, and its dominant term is
 * `4 * routeImage`, which the same Add erodes on every one of ~1200 flights a game. Nothing
 * else refills route image except advertising, one campaign per route per day. */
static const SLONG kEssenTarget = 2;

/* Image is worth roughly a factor two in route passengers ((400 + ImageTotal) / 1100 with
 * ImageTotal = 4 * routeImage + airlineImage + 200, capped at 1000). Advertising is not
 * part of the operating result, so image is simply purchasable: 50,000 per airline image
 * point, 30,000 per route image point. */
/* Raising this to 1000 measured 13,280,374 against 18,383,803: airline image is bought with
 * the same cash that buys planes, and a plane is worth more.
 *
 * It is bounded from above by arithmetic, not by taste. ImageTotal is clamped at 1000
 * (Schedule.cpp:379), route image is clamped at 100 and counts four times, and executeAds()
 * buys every route to 90 before it touches airline image - so once the routes are bought up,
 * anything past 1000 - 4 * 90 - 200 = 440 airline points changes no passenger count at all.
 * Flying lifts route image the last ten points for free (Schedule.cpp:860), which would drop
 * the useful ceiling to 400; 440 covers a pair rented this morning. */
static const SLONG kImageSaturation = 440;

/* But the saturation point is what we need on the day the *flights* run, not what we need
 * to buy. Image erodes on every flight (Schedule.cpp:917, 992) and the only thing that
 * refills it is the advertising agency - which is shut on Saturday and Sunday
 * (BotHelper.cpp, checkRoomOpen: ACTION_WERBUNG). Friday's campaign therefore has to carry
 * three days of erosion, and the erosion scales with the fleet: a flat 450 held the image at
 * 440 all week and let it fall to 179 by Sunday, which is 19% off every route fare in
 * precisely the week the score is averaged over. A flat 700 was the old way of paying for
 * that, at the price of 15 million a week burnt on points that do nothing from Monday to
 * Friday - and in the early game that 15 million is the aeroplane.
 *
 * So the buffer is measured rather than guessed: how much image the last campaign actually
 * lost, per day, times the number of days until the agency opens again. It rises at once
 * when the fleet grows and decays slowly when it shrinks. */
static const SLONG kMaxTargetImage = 950;

/* ...and a lower ceiling on that target while the fleet is small.
 *
 * kImageSaturation is derived from the ImageTotal clamp, which is the right bound once the
 * fleet is large enough that its cabins are the scarce thing. It is not what binds early:
 * CalcPassengers caps the load at 1.5x the cabin *before* image is applied (Schedule.cpp:321),
 * and with two aeroplanes against a route pool of thousands that cap is what a flight actually
 * hits. Measured over the first thirty days: airline image 29 -> 273 lifted the load from 147
 * to 224 passengers a departure, and 273 -> 441 moved it from 224 to 200. The second half of
 * that spend bought seats that cannot exist, out of the cash that buys the third aeroplane.
 *
 * Worth 81M. It was measured as neutral in session 17 and discarded - on a harness whose
 * noise was four times what it is now (see session 17d). A ceiling on the target, not a cash
 * floor: reserving cash from the agency does not work, because executeBuyPlane() spends to the
 * debt limit every day and anything queued behind it is never funded (measured at 1.3M). */
static const SLONG kSmallFleet = 10;
static const SLONG kImageSaturationSmallFleet = 250;
static const __int64 kAdCashBuffer = 1500000;

/* Cash we keep free for the next plane before spending on airline image. */
static const __int64 kFleetCashBuffer = 20000000;

/* Gates.
 *
 * Every airline starts with exactly one (SIM::Init, Sim.cpp:703) and the bot never asked for
 * another, so a measured game ends with `Gatemiete` at -3,267 - one gate's rent for a hundred
 * days - while making about 97 movements a day at the home airport. One gate serves roughly
 * 24: PLAYER::PlanGates holds it for the landing hour, and for the departure hour and the one
 * before it (Player.cpp:2487-2506).
 *
 * Everything that does not get one costs 2 image points (Sim.cpp:2071) and is then shifted up
 * to six hours later to hunt for a slot (Player.cpp:5746), which walks departures into the
 * night penalty. The measured image decay late in a game is 206 points a day, which the
 * advertising agency then buys back at 50,000 a point - 10.3 million a day of the very cash
 * the fleet is short of.
 *
 * Bidding itself is free: GameMechanic::bidOnGate() only raises the asking rent and records
 * us as the high bidder, and the auction settles overnight. So we bid on everything, every
 * day. Winning costs `Preis` a month - about 33 dollars a day, which is scored and trivial.
 * Building one costs a million, which is not scored at all (category 3170). */
/* Left at 15 despite measuring +20.3M on its own.
 *
 * Asking for a gate sooner does win more of the five airport expansions
 * `canExpandAirport()` allows per game (Sim.CheckIn is a global counter shared with the
 * competitors). But it does not combine with the smaller route network below: 6 alone is
 * +20.3M, 38 pairs alone is +34.0M, and the two together measure **-28.5M** against neither.
 * A denser network already concentrates the fleet's departures onto fewer city pairs, which
 * is most of what the extra gates were buying. Taking the larger of the two. */
static const SLONG kPlanesPerGate = 15;
static const SLONG kMaxGates = 8;
/* Airport expansion is only worth a million once there are aeroplanes queueing for the gate. */
static const __int64 kExpandCashReserve = 5000000;

/* CFlugplanEintrag::CalcPassengers() computes the price tolerance from a reference plane
 * with these values (Schedule.cpp:428, 509). */
static const SLONG kRefVerbrauch = 800;
static const SLONG kRefGeschwindigkeit = 800;

/* GameMechanic::_planFlightJob() rejects any date >= Sim.Date + 7. */
static const SLONG kMaxPlanDate = 6;

/* Rooms we use to pass time when there is nothing productive left to do. */
static const SLONG kFillerActions[] = {ACTION_VISITKIOSK, ACTION_VISITRICK, ACTION_VISITMUSEUM, ACTION_VISITTELESCOPE};

/* The kerosene price, cached once a day.
 *
 * RULES.md permits `Sim.Kerosin` / `Sim.HoleKerosinPreis()` at the Arab and in the personal
 * office, and records that the price is fixed for the whole day - so it may be read once and
 * used in any room. Every cost estimate in the bot goes through this value: the route
 * valuation at the route box, the aeroplane ranking at the broker, the museum, and every
 * fit-a-job-into-a-window calculation.
 *
 * File scope rather than a member because the price is global - it is the same number for
 * every airline - and because the two cost helpers below are free functions. The value is
 * still bot state, so it is serialised with the rest of it.
 *
 * The default is the midpoint of the [300, 700] band SIM::NewDay clamps the walk to, and
 * stands only until the first office or Arab visit of the game. */
static SLONG gKerosinPrice = 500;
static SLONG gKerosinPriceDay = -1;
/* Running mean of the daily price, x100 to keep a fraction. Seeded at the opening price. */
static SLONG gKerosinAvgX100 = 500 * 100;

/* Only legal in the personal office or at the Arab. */
static void cacheKerosinPrice() {
    const SLONG price = Sim.HoleKerosinPreis(1);
    if (price > 0) {
        if (gKerosinPriceDay != Sim.Date) {
            /* One sample a day, exponentially weighted over about a fortnight. */
            gKerosinAvgX100 += (price * 100 - gKerosinAvgX100) / 14;
        }
        gKerosinPrice = price;
        gKerosinPriceDay = Sim.Date;
    }
}

ClaudeBot::ClaudeBot(PLAYER &player) : qPlayer(player) {}

void ClaudeBot::RobotInit() {
    auto balance = qPlayer.BilanzWoche.Hole();
    AT_Info("ClaudeBot.cpp: Enter RobotInit() for %s: Current day: %d, money: %s $ (op saldo %s = %s %s)", qPlayer.Abk.c_str(), Sim.Date,
            Insert1000erDots64(qPlayer.Money).c_str(), Insert1000erDots64(balance.GetOpSaldo()).c_str(), Insert1000erDots64(balance.GetOpGewinn()).c_str(),
            Insert1000erDots64(balance.GetOpVerlust()).c_str());

    /* print inventory */
    printf("Inventory: ");
    for (SLONG d = 0; d < 6; d++) {
        if (qPlayer.Items[d] != 0xff) {
            printf("%s, ", Helper::getItemName(qPlayer.Items[d]));
        }
    }
    printf("\n");

    if (mFirstRun) {
        AT_Log("ClaudeBot::RobotInit(): First run.");

        /* random source */
        LocalRandom.SRand(qPlayer.WaitWorkTill);

        /* bot level */
        AT_Log("ClaudeBot::RobotInit(): We are player %d with bot level = %s.", qPlayer.PlayerNum, StandardTexte.GetS(TOKEN_NEWGAME, 5001 + qPlayer.BotLevel));

        mFirstRun = false;
    }

    startNewDay();

    for (auto &i : qPlayer.RobotActions) {
        i = {};
    }

    RobotPlan();
    AT_Log("ClaudeBot.cpp: Leaving RobotInit()");
}

bool ClaudeBot::haveOffice() const {
    if (qPlayer.HasItem(ITEM_LAPTOP) != 0 && qPlayer.LaptopVirus == 0) {
        return true;
    }
    return (qPlayer.OfficeState != 2);
}

bool ClaudeBot::canUseAction(SLONG actionId) const {
    if (!Helper::checkRoomOpen(actionId)) {
        return false;
    }
    /* -1 means "no room", which RobotPump() cannot walk to */
    return Helper::getRoomFromAction(qPlayer.PlayerNum, actionId) != -1;
}

SLONG ClaudeBot::pickFillerAction() {
    const SLONG n = sizeof(kFillerActions) / sizeof(kFillerActions[0]);
    for (SLONG i = 0; i < n; i++) {
        SLONG actionId = kFillerActions[(mFillerIdx + i) % n];
        if (canUseAction(actionId)) {
            mFillerIdx = (mFillerIdx + i + 1) % n;
            return actionId;
        }
    }
    return ACTION_WAIT;
}

/* Per-day bookkeeping. RobotInit() is the one callback that is guaranteed to run exactly
 * once per day, and Sim.Date may be read anywhere, so the reset lives here. */
void ClaudeBot::startNewDay() {
    if (mDay == Sim.Date) {
        return;
    }
    mDay = Sim.Date;
    mJobsTakenToday = 0;
    mVisitedPersonalToday = false;
    mVisitedMechToday = false;
    mVisitedRouteBoxToday = false;
    mVisitedAdsToday = false;
    mVisitedBossToday = false;
    mVisitedBrokerToday = false;
    mVisitedBankToday = false;
    mVisitedStockToday = false;
    mVisitedMuseumToday = false;
    mUpgradedToday = false;
    mAgencyEmptyToday = false;
    mAgencyVisitsToday = 0;
    mFreightVisitsToday = 0;
    mVisitedTanksToday = false;
    mVisitedKerosinToday = false;
    mVisitedFreightToday = false;
    mFreightEmptyToday = false;
    mFreightTakenToday = 0;
    /* Flights were flown overnight, so every cached availability is out of date. */
    mPlaneStateStale = true;
    mNeedSchedule = true;
}

/* Appends every action worth doing right now to `out`, most valuable first.
 *
 * Every branch here has to be self-limiting: the room it sends us to must clear the
 * condition that selected it, otherwise the bot loops on one room for the rest of the
 * game (which is exactly what the previous version did).
 *
 * The list is a priority order, not a plan. Only the first entry is certain to be worth
 * doing when it is reached: the later ones were judged against the state as it is now, and
 * the earlier ones will have changed it. Each execute* handler re-checks its own
 * preconditions on arrival, so a candidate that has gone stale costs a walk and nothing
 * more - which is exactly what the filler action it replaces cost. */
void ClaudeBot::collectActions(std::vector<SLONG> &out) const {
    /* 1) Make sure the planes are crewed. Worker data may only be read in the HR
     *    room, so we go there once a day and decide on arrival. */
    if (!mVisitedPersonalToday && canUseAction(ACTION_PERSONAL)) {
        out.push_back(ACTION_PERSONAL);
    }

    /* 2) Keep the repair targets sane. Getting this wrong is by far the most expensive
     *    mistake available: a wrecked plane climbing back to full condition is charged
     *    Improvement * ptPreis / 110 every single night. */
    if (!mVisitedMechToday && canUseAction(ACTION_VISITMECH)) {
        out.push_back(ACTION_VISITMECH);
    }

    /* 3) Borrow to the limit, every day. Loan interest is not part of the operating
     *    result and the game has no bankruptcy, so leverage is free: the cash buys
     *    planes, which are the only thing that scales revenue. */
    if (!mVisitedBankToday && qPlayer.CalcCreditLimit() > 0 && canUseAction(ACTION_RAISEMONEY)) {
        out.push_back(ACTION_RAISEMONEY);
    }

    /* 3b) Issue shares. AktienEmission sits outside the operating result too, and unlike a
     *     loan it never has to be paid back, so it is the one source of capital that can
     *     grow the fleet and buy the image the routes need. */
    if (kEmitStock && !mVisitedStockToday && canUseAction(ACTION_EMITSHARES)) {
        out.push_back(ACTION_EMITSHARES);
    }

    /* 2) The office is the only place where flight plans may be read. Go there to
     *    rebuild the cached plane availability and to place the jobs we own. */
    if ((mPlaneStateStale || mNeedSchedule) && (qPlayer.OfficeState != 2) && canUseAction(ACTION_BUERO)) {
        out.push_back(ACTION_BUERO);
    }

    /* 4) Routes are the long-term revenue engine: their image, and with it the passenger
     *    count, grows with every flight. Check the route box once a day. */
    if (!mVisitedRouteBoxToday && canUseAction(ACTION_VISITROUTEBOX)) {
        out.push_back(ACTION_VISITROUTEBOX);
    }

    /* 5) Turn spare cash into flying capacity and image. Neither plane purchases nor
     *    advertising are part of the operating result, so this is free score as long as
     *    the cash is not needed elsewhere. */
    /* The Arab is only worth a walk on a cheap day: capacity is useless without stock, and
     * stock is only worth buying below kKerosinBuyBelow. */
    if (kUseFuelArbitrage && fuelIsCheap() && qPlayer.Tank < fuelTankTarget() && !mVisitedTanksToday && canUseAction(ACTION_BUY_KEROSIN_TANKS)) {
        out.push_back(ACTION_BUY_KEROSIN_TANKS);
    }
    if (kUseFuelArbitrage && fuelIsCheap() && !mVisitedKerosinToday && qPlayer.Tank > 0 && canUseAction(ACTION_BUY_KEROSIN)) {
        out.push_back(ACTION_BUY_KEROSIN);
    }

    if (!mVisitedBrokerToday && !mRoutes.empty() && qPlayer.Money > kCashBuffer && canUseAction(ACTION_BUYNEWPLANE)) {
        out.push_back(ACTION_BUYNEWPLANE);
    }
    if (mWantUsedPlane && !mVisitedMuseumToday && qPlayer.Money > kCashBuffer && canUseAction(ACTION_BUYUSEDPLANE)) {
        out.push_back(ACTION_BUYUSEDPLANE);
    }

    /* 5c) The boss. Gate auctions are free to enter and settle overnight, so this is worth a
     *     slot every day the room is open - see kPlanesPerGate. */
    if (!mVisitedBossToday && canUseAction(ACTION_EXPANDAIRPORT)) {
        out.push_back(ACTION_EXPANDAIRPORT);
    }

    if (!mVisitedAdsToday && !mRoutes.empty() && qPlayer.Money > kAdCashBuffer + gWerbePrice[3] && canUseAction(ACTION_WERBUNG)) {
        out.push_back(ACTION_WERBUNG);
    }

    if (!mUpgradedToday && (qPlayer.OfficeState != 2) && canUseAction(ACTION_UPGRADE_PLANES)) {
        out.push_back(ACTION_UPGRADE_PLANES);
    }

    /* 5b) Kerosene is the largest scored cost line there is. Both the tank and the stock
     *     in it sit outside the operating result at purchase time, so buying cheap and
     *     burning it later is a straight transfer into the score. */

    /* 6) Pick up new work. Needs a fresh plane state to decide what we can fly. */
    if (mAgencyVisitsToday < kMaxAgencyVisitsPerDay && !mPlaneStateStale && mJobsTakenToday < kMaxJobsPerDay && canUseAction(ACTION_CHECKAGENT2)) {
        out.push_back(ACTION_CHECKAGENT2);
    }

    /* 7) Freight is a second pool of work for the same idle windows, so it is checked after
     *    the travel agency: a passenger job pays its whole premium on one flight, a freight
     *    contract only on its last one. */
    if (kUseFreight && mFreightVisitsToday < kMaxAgencyVisitsPerDay && !mPlaneStateStale && mFreightTakenToday < kMaxFreightPerDay &&
        canUseAction(ACTION_CHECKAGENT3)) {
        out.push_back(ACTION_CHECKAGENT3);
    }
}

void ClaudeBot::RobotPlan() {
    if (mFirstRun) {
        AT_Error("ClaudeBot::RobotPlan(): ClaudeBot was not initialized!");
        RobotInit();
        AT_Log("ClaudeBot.cpp: Leaving RobotPlan() (not initialized)\n");
        return;
    }

    auto &qRobotActions = qPlayer.RobotActions;

    if (qRobotActions[0].ActionId != ACTION_NONE || qRobotActions[1].ActionId != ACTION_NONE) {
        AT_Log("ClaudeBot.cpp: Leaving RobotPlan() (actions already planned)\n");
        return;
    }

    auto &qFirstAction = qRobotActions[1];
    auto &qSecondAction = qRobotActions[2];
    qFirstAction.ActionId = ACTION_NONE;
    qSecondAction.ActionId = ACTION_NONE;

    /* Both slots are worked, not just the first. RobotPump() shifts the queue one entry at
     * a time and only calls back here once both are spent (Player.cpp:3194-3241), so
     * whatever sits in the second slot is walked to and executed exactly like the first.
     * Filling it with a kiosk visit threw away half of every working day: each action costs
     * the same WorkCountdown of 100 whether it earns anything or not, and the day is a
     * fixed 09:00-18:00. The end-game logs show 58 actions a day of which 29 were filler,
     * and advertising - last in the priority order - was missed outright on one day in four.
     *
     * The second slot still has to be a different room than the first. The occupied-room
     * path sets slot 0 to ACTION_NONE and calls RobotPlan(), which returns early while slot
     * 1 is still filled, so the second slot is also what we fall back on when the first room
     * turns out to be full - and a fallback into the same room would fail the same way. */
    std::vector<SLONG> candidates;
    collectActions(candidates);

    const SLONG firstRoom = candidates.empty() ? -1 : Helper::getRoomFromAction(qPlayer.PlayerNum, candidates[0]);
    if (!candidates.empty()) {
        qFirstAction.ActionId = candidates[0];
        for (SLONG candidate : candidates) {
            if (Helper::getRoomFromAction(qPlayer.PlayerNum, candidate) != firstRoom) {
                qSecondAction.ActionId = candidate;
                break;
            }
        }
    }

    /* Nothing left worth doing: pass the time. */
    if (qFirstAction.ActionId == ACTION_NONE) {
        qFirstAction.ActionId = pickFillerAction();
    }
    if (qSecondAction.ActionId == ACTION_NONE) {
        qSecondAction.ActionId = pickFillerAction();
        if (qSecondAction.ActionId == qFirstAction.ActionId) {
            qSecondAction.ActionId = pickFillerAction();
        }
    }

    /* Run, always. RobotPump() halves the walking countdown for an action whose Running
     * flag is set (Player.cpp:3218-3221) and charges nothing for it - no fatigue, no mood,
     * no money. The airport is the one place where the bot pays for its own decisions in
     * wall clock time, and the working day is fixed at 09:00-18:00, so every halved walk
     * is another slot in a day that is otherwise spent in the corridor. */
    qFirstAction.Running = TRUE;
    qSecondAction.Running = TRUE;

    AT_Log("ClaudeBot::RobotPlan(): Current: %s, planned: %s, %s", Translate_ACTION(qRobotActions[0].ActionId), Translate_ACTION(qFirstAction.ActionId),
           Translate_ACTION(qSecondAction.ActionId));

    if (qFirstAction.ActionId == ACTION_NONE) {
        AT_Error("Did not plan action for slot #1");
    }
    if (qSecondAction.ActionId == ACTION_NONE) {
        AT_Error("Did not plan action for slot #2");
    }
}

void ClaudeBot::RobotExecuteAction() {
    if (mFirstRun) {
        AT_Error("ClaudeBot::RobotExecuteAction(): ClaudeBot was not initialized!");
        RobotInit();
        AT_Log("ClaudeBot.cpp: Leaving RobotExecuteAction() (not initialized)\n");
        return;
    }

    /* refuse to work outside working hours (game sometimes calls this too early) */
    if (Sim.Time <= 540000) { /* check if it is precisely 09:00 or earlier */
        AT_Log("ClaudeBot.cpp: Leaving RobotExecuteAction() (too early)\n");
        return;
    }
    if (Sim.GetHour() >= 18) {
        AT_Log("ClaudeBot.cpp: Leaving RobotExecuteAction() (too late)\n");
        return;
    }

    auto &qAction = qPlayer.RobotActions[0];
    LocalRandom.Rand(2); // Sicherheitshalber, damit wir immer genau ein Random ausführen

    AT_Info("ClaudeBot::RobotExecuteAction() for %s: Executing %s, current time: %02ld:%02ld, money: %s $ (available: %s $)", qPlayer.Abk.c_str(),
            Translate_ACTION(qAction.ActionId), Sim.GetHour(), Sim.GetMinute(), Insert1000erDots64(qPlayer.Money).c_str(),
            Insert1000erDots64(getMoneyAvailable()).c_str());

    mOnThePhone = 0;

    /* The room check RULES.md asks for. It warns and performs the action anyway, as required.
     *
     * In the headless runs the whole game plays with Sim.CallItADay set, and
     * PERSONS::DoOneStep() then calls RobotExecuteAction() without ever walking the character
     * anywhere (Person.cpp:3263-3272) - it stays in ROOM_AIRPORT, so every one of the ~5,300
     * actions of a game reports a mismatch. Warning on each would bury the log, so only the
     * first of a day is printed and the rest are counted.
     *
     * Either way the room restrictions of RULES.md are honoured by construction rather than
     * by this check: each action handler only touches the state its own room grants access
     * to, which is why the cached-price and cached-burn machinery above exists. */
    const SLONG wantRoom = Helper::getRoomFromAction(qPlayer.PlayerNum, qAction.ActionId);
    if (wantRoom != -1 && qPlayer.GetRoom() != wantRoom) {
        if (mWrongRoomDay != Sim.Date) {
            AT_Warn("ClaudeBot::RobotExecuteAction(): Not in the room for %s (in %ld, wanted %ld). Doing it anyway; "
                    "further mismatches today are counted only.",
                    Translate_ACTION(qAction.ActionId), static_cast<SLONG>(qPlayer.GetRoom()), wantRoom);
            mWrongRoomDay = Sim.Date;
            mWrongRoomCount = 0;
        }
        mWrongRoomCount++;
    }

    switch (qAction.ActionId) {
    case ACTION_NONE:
        qPlayer.WorkCountdown = 2;
        break;

    case ACTION_PERSONAL:
        executePersonal();
        break;

    case ACTION_VISITMECH:
        executeMech();
        break;

    case ACTION_RAISEMONEY:
        executeBank();
        break;

    case ACTION_EMITSHARES:
        executeStock();
        break;

    case ACTION_VISITROUTEBOX:
        executeRouteBox();
        break;

    case ACTION_BUYNEWPLANE:
        executeBuyPlane();
        break;

    case ACTION_BUYUSEDPLANE:
        executeMuseum();
        break;

    case ACTION_EXPANDAIRPORT:
        executeBoss();
        break;

    case ACTION_WERBUNG:
        executeAds();
        break;

    case ACTION_UPGRADE_PLANES:
        executeUpgrades();
        break;

    case ACTION_CHECKAGENT2:
        executeCheckAgent2();
        break;

    case ACTION_CHECKAGENT3:
        executeCheckAgent3();
        break;

    case ACTION_BUY_KEROSIN_TANKS:
        executeKerosinTanks();
        break;

    case ACTION_BUY_KEROSIN:
        executeBuyKerosin();
        break;

    case ACTION_BUERO:
        executeOffice();
        break;

    case ACTION_WAIT:
    case ACTION_VISITKIOSK:
    case ACTION_VISITRICK:
    case ACTION_VISITMUSEUM:
    case ACTION_VISITTELESCOPE:
        /* filler: nothing to do but pass the time */
        break;

    default:
        AT_Error("ClaudeBot::RobotExecuteAction(): Trying to execute invalid action: %s", Translate_ACTION(qAction.ActionId));
        DebugBreak();
    }

    AT_Log("");
}

/* Cost / duration / distance of one flight leg.
 * Mirrors CITIES::CalcFlugdauer(), CalculateFlightKerosin() and
 * CalculateFlightCostNoTank(); taken from RULES.md. */
static void calcCostAndDuration(int startCity, int destCity, const CPlane &qPlane, bool emptyFlight, int &cost, int &duration, int &distance) {
    assert(startCity >= 0 && startCity < Cities.AnzEntries());
    assert(destCity >= 0 && destCity < Cities.AnzEntries());

    distance = Cities.CalcDistance(startCity, destCity);
    duration = (distance / qPlane.ptGeschwindigkeit + 999) / 1000 + 1 + 2 - 2;
    if (duration < 2) {
        duration = 2;
    }

    SLONG kerosene = distance / 1000            // weil Distanz in m übergeben wird
                     * qPlane.ptVerbrauch / 160 // Liter pro Barrel
                     / qPlane.ptGeschwindigkeit;

    cost = kerosene * gKerosinPrice;
    if (cost < 1000) {
        cost = 1000;
    }

    if (emptyFlight) {
        cost -= (qPlane.ptPassagiere * distance / 1000 / 40);
    }
}

/* Reference cost the game uses to judge whether a ticket price is extortionate.
 * Mirrors CalculateFlightCost(von, nach, 800, 800, -1) * 3 / 180 * 2 from
 * CFlugplanEintrag::CalcPassengers(). Passenger numbers scale with 1/price above three
 * times this value, so revenue is flat beyond that: three times it is the best price. */
static SLONG routePriceBase(ULONG vonCity, ULONG nachCity) {
    SLONG kerosene = Cities.CalcDistance(vonCity, nachCity) / 1000 * kRefVerbrauch / 160 / kRefGeschwindigkeit;
    return kerosene * gKerosinPrice * 3 / 180 * 2;
}

/* What one pair is worth per plane hour to a given aeroplane.
 *
 * Plane hours, not money, are the scarce resource, so every route decision - which pair to
 * rent, and which of the pairs we hold a particular aeroplane should fly - is ranked on
 * this one number. Negative or small means the hour is better spent elsewhere.
 *
 * Returns a value that only makes sense if the plane can actually reach the pair; callers
 * check ptReichweite themselves. */
static SLONG routeValuePerHour(const CPlane &qPlane, const CRoute &qRoute) {
    int cost = 0;
    int duration = 0;
    int dist = 0;
    calcCostAndDuration(Cities.find(qRoute.VonCity), Cities.find(qRoute.NachCity), qPlane, false, cost, duration, dist);
    if (duration > 24) {
        return 0;
    }

    const SLONG base = routePriceBase(qRoute.VonCity, qRoute.NachCity);

    /* Passengers per flight: our seats, but never more than the route wants. Half the
     * demand is a deliberately pessimistic share estimate for a route we do not fly
     * yet (image 0) and may have to share with a competitor. */
    SLONG seats = std::min<SLONG>(qPlane.MaxPassagiere, qRoute.Bedarf / 2);
    SLONG seatsFC = std::min<SLONG>(qPlane.MaxPassagiereFC, qRoute.Bedarf / 2);
    /* Deliberately at the tolerance thresholds rather than at the prices we actually
     * charge. Substituting the real prices measured 15,339,552 against 18,383,803: it
     * scales economy revenue up by nearly two, which lifts marginal routes over the
     * bar, and every extra pair splits the same demand while its rent is scored. */
    SLONG revenue = seats * base * 3 + seatsFC * base * 9;

    /* Rent is charged per day whether we fly or not, and it is part of the score.
     *
     * `Miete` is a *monthly* figure: PLAYER::NewDay charges `Miete / 30` a day
     * (Player.cpp:912), once per direction, so a pair costs `Miete / 15` a day - and
     * that is a cost of the route, to be spread over the plane hours we put on it,
     * not a cost of one hour. Charging the whole monthly rent against every single
     * hour overstated it thirtyfold, and it fell hardest on the dense long haul pairs,
     * which carry the highest rent. */
    const SLONG rentPerHour = qRoute.Miete / 15 / (kPlanesPerRoute * 24);
    return (revenue - cost) / (duration + 1) - rentPerHour;
}

bool ClaudeBot::planeCanFly(const CPlane &qPlane, const CAuftrag &qJob) const {
    if (static_cast<SLONG>(qJob.Personen) > qPlane.ptPassagiere) {
        return false;
    }
    SLONG distance = Cities.CalcDistance(qJob.VonCity, qJob.NachCity);
    if (distance > qPlane.ptReichweite * 1000) {
        return false;
    }
    /* a leg may never take longer than a day */
    int cost = 0;
    int duration = 0;
    int dist = 0;
    calcCostAndDuration(Cities.find(qJob.VonCity), Cities.find(qJob.NachCity), qPlane, false, cost, duration, dist);
    return duration <= 24;
}

//--------------------------------------------------------------------------------------------
// HR office: make sure every plane has a crew.
//--------------------------------------------------------------------------------------------
void ClaudeBot::executePersonal() {
    mVisitedPersonalToday = true;

    SLONG needPilots = 0;
    SLONG needAttendants = 0;
    for (SLONG c = 0; c < qPlayer.Planes.AnzEntries(); c++) {
        if (qPlayer.Planes.IsInAlbum(c) == 0) {
            continue;
        }
        needPilots += qPlayer.Planes[c].ptAnzPiloten;
        needAttendants += qPlayer.Planes[c].ptAnzBegleiter;
    }

    SLONG havePilots = 0;
    SLONG haveAttendants = 0;
    for (SLONG i = 0; i < Workers.Workers.AnzEntries(); i++) {
        const auto &qWorker = Workers.Workers[i];
        if (qWorker.Employer != qPlayer.PlayerNum) {
            continue;
        }
        if (qWorker.Typ == WORKER_PILOT) {
            havePilots++;
        } else if (qWorker.Typ == WORKER_STEWARDESS) {
            haveAttendants++;
        }
    }

    hireAdvisors();

    /* Take every applicant on the board, not only the ones a plane is waiting for.
     *
     * The labour market is a fixed pool shared with three other airlines, and it is what
     * the fleet runs into first: `_planFlightJob` will not schedule an aeroplane whose crew
     * is short of ptAnzPiloten / ptAnzBegleiter (GameMechanic.cpp:2525-2535), and at a fleet
     * of 161 the hiring log reads "still missing 39 pilots / 346 attendants" while the
     * airline makes exactly as many departures a day as it did with 110. Fifty aeroplanes
     * had no crew.
     *
     * Hiring ahead is nearly free - `Personal` is 1.1 million of a 1,250 million week - and
     * a worker already on the payroll is assigned to the next aeroplane by MapWorkers() the
     * moment it is bought, instead of waiting for a board that may be empty by then. */
    SLONG hired = 0;
    for (SLONG i = 0; i < Workers.Workers.AnzEntries(); i++) {
        const auto &qWorker = Workers.Workers[i];
        if (qWorker.Employer != WORKER_JOBLESS) {
            continue;
        }
        if (qWorker.Typ != WORKER_PILOT && qWorker.Typ != WORKER_STEWARDESS) {
            continue;
        }
        if (!GameMechanic::hireWorker(qPlayer, i)) {
            continue;
        }
        hired++;
    }

    AT_Log("ClaudeBot::executePersonal(): Hired %ld, now %ld pilots / %ld attendants against %ld / %ld needed.", hired, havePilots, haveAttendants, needPilots,
           needAttendants);
}

//--------------------------------------------------------------------------------------------
// HR office: keep the best advisor of every type we want on the payroll.
//
// PLAYER::HasBerater() returns the highest Talent among the ones we employ, so a second
// advisor of a type is dead salary unless it is better than the one we have - in which case
// the old one goes. Only legal in the HR room, which is where the worker pool may be read.
//--------------------------------------------------------------------------------------------
void ClaudeBot::hireAdvisors() {
    for (const auto &qWanted : kAdvisors) {
        const SLONG typ = qWanted.typ;
        SLONG bestApplicant = -1;
        SLONG bestApplicantTalent = -1;
        SLONG worstOwn = -1;
        SLONG worstOwnTalent = -1;
        SLONG ownCount = 0;
        SLONG ownBestTalent = 0;

        for (SLONG i = 0; i < Workers.Workers.AnzEntries(); i++) {
            const auto &qWorker = Workers.Workers[i];
            /* Employer first: RULES.md only permits reading a worker who is jobless or ours,
             * so `Typ` may not be looked at until that is established. */
            if (qWorker.Employer != qPlayer.PlayerNum && qWorker.Employer != WORKER_JOBLESS) {
                continue;
            }
            if (qWorker.Typ != typ) {
                continue;
            }
            if (qWorker.Employer == qPlayer.PlayerNum) {
                ownCount++;
                ownBestTalent = std::max<SLONG>(ownBestTalent, qWorker.Talent);
                if (worstOwn < 0 || qWorker.Talent < worstOwnTalent) {
                    worstOwn = i;
                    worstOwnTalent = qWorker.Talent;
                }
            } else if (qWorker.Employer == WORKER_JOBLESS && qWorker.Talent > bestApplicantTalent) {
                bestApplicant = i;
                bestApplicantTalent = qWorker.Talent;
            }
        }

        /* A second one of the same type only raises HasBerater() if it is better. */
        if (bestApplicant >= 0 && bestApplicantTalent >= qWanted.minTalent && bestApplicantTalent > ownBestTalent) {
            if (GameMechanic::hireWorker(qPlayer, bestApplicant)) {
                AT_Log("ClaudeBot::hireAdvisors(): Hired advisor type %ld with talent %ld (had %ld).", typ, bestApplicantTalent, ownBestTalent);
                ownCount++;
            }
        }

        /* Never pay two salaries for one advice: the weaker one contributes nothing. */
        while (ownCount > 1 && worstOwn >= 0) {
            if (!GameMechanic::fireWorker(qPlayer, worstOwn)) {
                break;
            }
            AT_Log("ClaudeBot::hireAdvisors(): Fired surplus advisor type %ld with talent %ld.", typ, worstOwnTalent);
            ownCount--;

            worstOwn = -1;
            worstOwnTalent = -1;
            for (SLONG i = 0; i < Workers.Workers.AnzEntries(); i++) {
                const auto &qWorker = Workers.Workers[i];
                if (qWorker.Employer != qPlayer.PlayerNum || qWorker.Typ != typ) {
                    continue;
                }
                if (worstOwn < 0 || qWorker.Talent < worstOwnTalent) {
                    worstOwn = i;
                    worstOwnTalent = qWorker.Talent;
                }
            }
        }
    }
}

/* Fits one out-and-back into one idle window. Returns false if it does not fit; otherwise
 * sets the departure time, the hour the plane is back where it started, and the kerosene
 * both legs burn.
 *
 * The flight has to be back in the window's city before the window closes: the plane's next
 * route leg departs from there, and an entry that overruns pushes every following flight
 * later (Planetyp.cpp:753-780). The game flies the empty return itself, so its kerosene
 * is part of the cost even though we never plan it.
 *
 * [fromDate, toDate] is the contract window the departure has to fall into. */
bool ClaudeBot::fitLegIntoGap(const PlaneGap &qGap, const CPlane &qPlane, ULONG vonCity, ULONG nachCity, SLONG fromDate, SLONG toDate, PlaneTime &outStart,
                              PlaneTime &outBack, SLONG &outCost) {
    if (qGap.city < 0 || static_cast<ULONG>(qGap.city) != vonCity) {
        return false; /* repositioning first would eat the premium and the window */
    }

    PlaneTime start = qGap.start;
    if (start.getDate() < fromDate) {
        start = PlaneTime{fromDate, 0};
    }
    if (start.getDate() > toDate || start.getDate() > Sim.Date + kMaxPlanDate) {
        return false;
    }

    int costOut = 0;
    int durationOut = 0;
    int costBack = 0;
    int durationBack = 0;
    int dist = 0;
    calcCostAndDuration(Cities.find(vonCity), Cities.find(nachCity), qPlane, false, costOut, durationOut, dist);
    calcCostAndDuration(Cities.find(nachCity), Cities.find(vonCity), qPlane, true, costBack, durationBack, dist);

    /* One idle hour after each landing, plus one hour of slack so a rounding difference
     * against CalcFlugdauer() cannot turn into a shifted route leg. */
    PlaneTime back = start + durationOut + 1 + durationBack + 1;
    if (back + kJobGapSlack > qGap.end) {
        return false;
    }

    outStart = start;
    outBack = back;
    outCost = costOut + costBack;
    return true;
}

/* Fits a passenger job into one idle window, return leg included. */
bool ClaudeBot::fitJobIntoGap(const PlaneGap &qGap, const CPlane &qPlane, const CAuftrag &qJob, PlaneTime &outStart, PlaneTime &outBack, SLONG &outGain) {
    SLONG cost = 0;
    if (!fitLegIntoGap(qGap, qPlane, qJob.VonCity, qJob.NachCity, static_cast<SLONG>(qJob.Date), static_cast<SLONG>(qJob.BisDate), outStart, outBack, cost)) {
        return false;
    }
    outGain = qJob.Praemie - cost;
    return true;
}

/* Spreads one freight contract over the idle windows of a fleet, earliest window first.
 *
 * Returns the tons that can be delivered before the deadline. The windows are consumed as
 * they are used, so the caller may keep planning further contracts against the same
 * vectors, and outLegs records where every leg went.
 *
 * Windows are taken in chronological order rather than cheapest-first: every leg of one
 * contract flies the same city pair, so the kerosene per leg is fixed and the only thing
 * that differs between windows is how soon the tonnage is done. */
SLONG ClaudeBot::fitFreightIntoGaps(const std::vector<SLONG> &planeIds, std::vector<std::vector<PlaneGap>> &gaps, const CFracht &qFreight, SLONG tons,
                                    SLONG &outCost, std::vector<FreightLeg> &outLegs) const {
    outCost = 0;
    outLegs.clear();
    if (tons <= 0) {
        return 0;
    }

    /* (start, slot, gap) of every window, oldest first. */
    std::vector<std::pair<PlaneTime, std::pair<SLONG, SLONG>>> order;
    for (SLONG p = 0; p < static_cast<SLONG>(planeIds.size()); p++) {
        for (SLONG g = 0; g < static_cast<SLONG>(gaps[p].size()); g++) {
            order.emplace_back(gaps[p][g].start, std::make_pair(p, g));
        }
    }
    std::sort(order.begin(), order.end(), [](const auto &a, const auto &b) { return a.first < b.first; });

    SLONG covered = 0;
    for (const auto &entry : order) {
        if (covered >= tons || static_cast<SLONG>(outLegs.size()) >= kMaxFreightLegs) {
            break;
        }
        const SLONG p = entry.second.first;
        const SLONG g = entry.second.second;
        const auto &qPlane = qPlayer.Planes[planeIds[p]];
        if (Cities.CalcDistance(qFreight.VonCity, qFreight.NachCity) > qPlane.ptReichweite * 1000) {
            continue;
        }
        const SLONG tonsPerLeg = qPlane.ptPassagiere / 10;
        if (tonsPerLeg <= 0) {
            continue;
        }

        bool usedThisGap = false;
        while (covered < tons && static_cast<SLONG>(outLegs.size()) < kMaxFreightLegs) {
            PlaneTime start{};
            PlaneTime back{};
            SLONG cost = 0;
            if (!fitLegIntoGap(gaps[p][g], qPlane, qFreight.VonCity, qFreight.NachCity, static_cast<SLONG>(qFreight.Date), static_cast<SLONG>(qFreight.BisDate),
                               start, back, cost)) {
                break;
            }
            if (!usedThisGap) {
                outCost += kFreightRefitCost;
                usedThisGap = true;
            }
            outCost += cost + kFreightLegOpportunityCost;
            covered += tonsPerLeg;
            outLegs.push_back(FreightLeg{p, g, start, back});
            gaps[p][g].start = back;
        }
    }

    return covered;
}

/* Finds the idle window that can serve a job most profitably. Works purely on the cached
 * plane state, so it may be called at the travel agency where flight plans are off
 * limits. Returns an index into mPlanes, or -1 if no plane can fly the job at a profit. */
SLONG ClaudeBot::findPlaneForJob(const CAuftrag &qJob, SLONG &outGap, PlaneTime &outStart, SLONG &outGain) const {
    SLONG best = -1;
    SLONG bestGap = -1;
    SLONG bestGain = 0;
    PlaneTime bestStart{};

    for (SLONG p = 0; p < static_cast<SLONG>(mPlanes.size()); p++) {
        const auto &qState = mPlanes[p];
        if (qPlayer.Planes.IsInAlbum(qState.id) == 0) {
            continue;
        }
        const auto &qPlane = qPlayer.Planes[qState.id];
        if (!planeCanFly(qPlane, qJob)) {
            continue;
        }

        for (SLONG g = 0; g < static_cast<SLONG>(qState.gaps.size()); g++) {
            PlaneTime start{};
            PlaneTime back{};
            SLONG gain = 0;
            if (!fitJobIntoGap(qState.gaps[g], qPlane, qJob, start, back, gain)) {
                continue;
            }
            if (gain <= bestGain) {
                continue;
            }
            best = p;
            bestGap = g;
            bestGain = gain;
            bestStart = start;
        }
    }

    outGap = bestGap;
    outStart = bestStart;
    outGain = bestGain;
    return best;
}

//--------------------------------------------------------------------------------------------
// Mechanic: keep every plane repairable for free.
//
// The nightly repair charges Improvement * ptPreis / 110 for every point the condition is
// pushed above WorstZustand + 20 (Player.cpp:1723-1745). WorstZustand tracks the worst
// condition the plane has ever been in, so a plane that was wrecked once and is then
// repaired back to 100 pays that penalty every night for weeks - measured at 3.1 million
// per night per plane in the previous run.
//
// Capping the repair target at WorstZustand + 20 makes Improvement zero by construction:
// repairs then only ever cost the mechanic's salary plus the small wear term, while a
// healthy plane (which never drops 20 points in a single day) still reaches 100.
//--------------------------------------------------------------------------------------------
void ClaudeBot::executeMech() {
    mVisitedMechToday = true;

    if (qPlayer.MechMode != kMechMode) {
        GameMechanic::setMechMode(qPlayer, kMechMode);
    }

    SLONG changed = 0;
    SLONG worst = 100;
    for (SLONG c = 0; c < qPlayer.Planes.AnzEntries(); c++) {
        if (qPlayer.Planes.IsInAlbum(c) == 0) {
            continue;
        }
        const auto &qPlane = qPlayer.Planes[c];
        worst = std::min(worst, static_cast<SLONG>(qPlane.Zustand));

        SLONG target = std::min<SLONG>(100, static_cast<SLONG>(qPlane.WorstZustand) + 20);
        if (static_cast<SLONG>(qPlane.TargetZustand) != target) {
            GameMechanic::setPlaneTargetZustand(qPlayer, c, target);
            changed++;
        }
    }

    AT_Log("ClaudeBot::executeMech(): Mech mode %ld, adjusted %ld repair target(s), worst condition %ld.", kMechMode, changed, worst);
}

//--------------------------------------------------------------------------------------------
// Route box: rent the routes that pay best per plane hour.
//
// The global Routen array may only be read here, so everything the office needs later
// (cities, distance, ticket prices, the id of the reverse direction) is cached.
//--------------------------------------------------------------------------------------------
void ClaudeBot::executeRouteBox() {
    mVisitedRouteBoxToday = true;

    /* The plane we would put on a new route: the largest one we own. */
    SLONG refPlane = -1;
    for (SLONG c = 0; c < qPlayer.Planes.AnzEntries(); c++) {
        if (qPlayer.Planes.IsInAlbum(c) == 0) {
            continue;
        }
        if (refPlane < 0 || qPlayer.Planes[c].ptPassagiere > qPlayer.Planes[refPlane].ptPassagiere) {
            refPlane = c;
        }
    }
    if (refPlane < 0) {
        return;
    }
    const auto &qRef = qPlayer.Planes[refPlane];

    /* Refresh the cache of the routes we already hold. */
    mRoutes.clear();
    for (SLONG r = 0; r < Routen.AnzEntries(); r++) {
        if (Routen.IsInAlbum(r) == 0) {
            continue;
        }
        if (qPlayer.RentRouten.RentRouten[r].Rang == 0) {
            continue;
        }
        /* Both directions are rented as a pair; keep one entry per pair. */
        SLONG reverse = GameMechanic::findRouteInReverse(qPlayer, r);
        if (reverse >= 0 && reverse < r) {
            continue;
        }
        RouteState state;
        state.id = r;
        state.reverseId = reverse;
        state.vonCity = Routen[r].VonCity;
        state.nachCity = Routen[r].NachCity;
        state.distance = Cities.CalcDistance(state.vonCity, state.nachCity);
        state.bedarf = Routen[state.id].Bedarf;
        state.anzPax = Routen[state.id].AnzPassagiere();
        state.valuePerHour = routeValuePerHour(qRef, Routen[r]);
        state.ticketPrice = routePriceBase(state.vonCity, state.nachCity) * 3 * kTicketPriceThresholdPercent / 100;
        state.ticketPriceFC = routePriceBase(state.vonCity, state.nachCity) * 9 * kTicketPriceThresholdPercentFC / 100;
        mRoutes.push_back(state);
    }

    SLONG numPlanes = 0;
    for (SLONG c = 0; c < qPlayer.Planes.AnzEntries(); c++) {
        if (qPlayer.Planes.IsInAlbum(c) != 0) {
            numPlanes++;
        }
    }
    const SLONG wantRoutes = std::min<SLONG>(kMaxRoutes, std::max<SLONG>(1, (numPlanes * kRoutePairsPerHundredPlanes + 99) / 100));
    if (static_cast<SLONG>(mRoutes.size()) >= wantRoutes) {
        AT_Log("ClaudeBot::executeRouteBox(): Holding %ld route(s) for %ld plane(s), enough.", static_cast<SLONG>(mRoutes.size()), numPlanes);
        return;
    }

    /* Rank what we could rent by profit per plane hour: plane hours, not money, are the
     * scarce resource. */
    auto buyable = GameMechanic::getBuyableRoutes(qPlayer);

    /* Every route has to touch the home airport.
     *
     * scheduleRouteFlights() only ever lays a leg that departs from where the plane already
     * stands and never plans a repositioning flight, so a route pair the fleet cannot reach
     * earns nothing while its Routenmiete is charged daily and scored - renting one is what
     * made the second pair cost 371,000 of score in an earlier session. Merely sharing a
     * city with an existing pair is not enough: the parking rule puts every plane at the
     * home airport overnight, so a pair between two outstations would only ever be flown by
     * accident. Anchoring every pair at home also means any plane can serve any route,
     * which is what lets the fleet grow past one pair. */
    auto touchesNetwork = [&](ULONG vonCity, ULONG nachCity) {
        if (static_cast<SLONG>(vonCity) == Sim.HomeAirportId || static_cast<SLONG>(nachCity) == Sim.HomeAirportId) {
            return true;
        }
        /* Sixteen pairs touch Berlin and the seventh-best of them is already below the bar,
         * so the home airport is the ceiling on how good the network can get. It is not the
         * ceiling the game imposes: getBuyableRoutes() opens every pair that shares a city
         * with a route we fly at 20% utilisation or better (GameMechanic.cpp:2688-2701), and
         * ours all sit at 100. New York, Delhi, Rio, Tokyo and Johannesburg are large cities
         * with large cities of their own to reach.
         *
         * A pair between two outstations is flyable for the same reason a home pair is: the
         * scheduler lays the leg that departs from wherever the aeroplane already stands, and
         * both directions of every pair are rented, so a plane parked at New York can always
         * come home again. */
        for (const auto &qRoute : mRoutes) {
            if (vonCity == qRoute.vonCity || vonCity == qRoute.nachCity || nachCity == qRoute.vonCity || nachCity == qRoute.nachCity) {
                return true;
            }
        }
        return false;
    };

    /* Every pair the fleet can use, not one a day.
     *
     * The route box opens once a day and this took a single pair away from it, while
     * executeBuyPlane() takes ten aeroplanes. A measured game ends with 45 aeroplanes
     * against six pairs - `wantRoutes` asks for twenty-three - and the aeroplanes carry
     * 248 passengers into cabins that hold 467, because `CRoute::Bedarf` is a pool the
     * fleet has already drained by the time the next leg departs. Demand, not plane hours,
     * is what the last week is short of. */
    while (static_cast<SLONG>(mRoutes.size()) < wantRoutes) {
        SLONG bestRoute = -1;
        SLONG bestValue = kMinRouteValuePerHour;
        SLONG nHome = 0;
        SLONG nInRange = 0;
        SLONG bestRejected = -1;
        SLONG bestRejectedValue = 0;
        for (SLONG r = 0; r < Routen.AnzEntries(); r++) {
            if (Routen.IsInAlbum(r) == 0 || buyable[r] == 0) {
                continue;
            }
            const auto &qRoute = Routen[r];
            if (!touchesNetwork(qRoute.VonCity, qRoute.NachCity)) {
                continue;
            }
            nHome++;
            if (Cities.CalcDistance(qRoute.VonCity, qRoute.NachCity) > qRef.ptReichweite * 1000) {
                continue;
            }
            nInRange++;

            SLONG value = routeValuePerHour(qRef, qRoute);
            if (value > bestRejectedValue) {
                bestRejectedValue = value;
                bestRejected = r;
            }
            if (value <= bestValue) {
                continue;
            }
            bestValue = value;
            bestRoute = r;
        }

        if (bestRoute < 0) {
            AT_Log(
                "ClaudeBot::executeRouteBox(): Nothing worth renting: %ld buyable pair(s) touch home, %ld in range, best %s at %ld/h against a floor of %ld.",
                nHome, nInRange, bestRejected < 0 ? "none" : Cities[Routen[bestRejected].NachCity].Name.c_str(), bestRejectedValue, kMinRouteValuePerHour);
            break;
        }

        if (!GameMechanic::rentRoute(qPlayer, bestRoute)) {
            AT_Log("ClaudeBot::executeRouteBox(): Renting route %ld failed.", bestRoute);
            break;
        }

        /* Both directions are gone from the market now; the cached list must not offer them
         * again on the next turn of this loop. */
        buyable[bestRoute] = 0;

        RouteState state;
        state.id = bestRoute;
        state.reverseId = GameMechanic::findRouteInReverse(qPlayer, bestRoute);
        if (state.reverseId >= 0) {
            buyable[state.reverseId] = 0;
        }
        state.vonCity = Routen[bestRoute].VonCity;
        state.nachCity = Routen[bestRoute].NachCity;
        state.distance = Cities.CalcDistance(state.vonCity, state.nachCity);
        /* Same price rule as the cache refresh above - a freshly rented route was priced at the
         * full threshold, which measured worse, and kept that price until the next route box
         * visit. Bedarf has to be carried over too: executeBuyPlane() picks the route with the
         * most demand, and a default of 0 makes a new route invisible to it. */
        state.bedarf = Routen[bestRoute].Bedarf;
        state.anzPax = Routen[bestRoute].AnzPassagiere();
        state.valuePerHour = routeValuePerHour(qRef, Routen[bestRoute]);
        state.ticketPrice = routePriceBase(state.vonCity, state.nachCity) * 3 * kTicketPriceThresholdPercent / 100;
        state.ticketPriceFC = routePriceBase(state.vonCity, state.nachCity) * 9 * kTicketPriceThresholdPercentFC / 100;
        mRoutes.push_back(state);
        mNeedSchedule = true;

        AT_Log("ClaudeBot::executeRouteBox(): Rented %s (%ld km, value %ld/h), now holding %ld route(s).", Helper::getRouteName(Routen[bestRoute]).c_str(),
               state.distance / 1000, bestValue, static_cast<SLONG>(mRoutes.size()));
    }
}

//--------------------------------------------------------------------------------------------
// Broker: turn cash into flying capacity.
//
// FlugzeugKauf is outside the operating result, so a plane costs nothing in score terms -
// only the crew, maintenance and kerosene it then consumes, all of which the flights it
// makes have to outweigh. Plane hours are the binding constraint on revenue, so spare cash
// belongs here.
//--------------------------------------------------------------------------------------------
void ClaudeBot::executeBuyPlane() {
    mVisitedBrokerToday = true;

    if (mRoutes.empty()) {
        return; /* nothing to fly it on yet */
    }

    /* Every plane drags scored cost behind it - kerosene, wages, maintenance, and via
     * wantRoutes another route pair's rent - while the cash that buys it is not scored.
     * Uncapped growth funded by share issues measured -914,252 against +1,472,113 for the
     * two-plane airline, so the fleet is capped until expansion is shown to pay. */
    SLONG havePlanes = 0;
    for (SLONG c = 0; c < qPlayer.Planes.AnzEntries(); c++) {
        if (qPlayer.Planes.IsInAlbum(c) != 0) {
            havePlanes++;
        }
    }
    if (havePlanes >= kMaxPlanes) {
        return;
    }

    /* The route a new plane would serve: the one with the most unserved demand. */
    const RouteState *target = nullptr;
    for (const auto &qRoute : mRoutes) {
        if (target == nullptr || qRoute.bedarf > target->bedarf) {
            target = &qRoute;
        }
    }
    if (target == nullptr || target->distance <= 0) {
        return;
    }

    /* A plane is the only thing that scales revenue, and buyPlane() only requires
     * Money - price >= DEBT_LIMIT, so the overdraft counts towards the purchase. */
    const __int64 budget = qPlayer.Money - DEBT_LIMIT - kPlaneCashReserve;
    if (budget <= 0) {
        return;
    }

    /* Ranked without reference to what we can afford today, then bought only when we can
     * afford it. The fleet is capped at kMaxPlanes, so the scarce thing is a slot in it,
     * not the cash - and buying the best plane on the lot the moment the money clears the
     * cheapest one fills those slots with the cheapest one. On Delhi an Airbus A 300 is
     * worth about 80,000 a plane hour and an Ilyushin Il 62 about 26,000: the A 300 carries
     * twice the cabin on a quarter of the fuel, for 28.1 million against 9.9. Waiting a few
     * days for it beats flying the Ilyushin for the rest of the game. */
    SLONG bestType = -1;
    SLONG bestValue = 0;
    SLONG bestAffordableType = -1;
    SLONG bestAffordableValue = 0;
    for (SLONG type : GameMechanic::getAvailablePlaneTypes()) {
        const auto &qType = PlaneTypes[type];
        if (target->distance > qType.Reichweite * 1000) {
            continue;
        }

        SLONG duration = (target->distance / qType.Geschwindigkeit + 999) / 1000 + 1;
        duration = std::max<SLONG>(2, duration);
        if (duration > 24) {
            continue;
        }

        SLONG kerosene = target->distance / 1000 * qType.Verbrauch / 160 / qType.Geschwindigkeit;
        SLONG cost = std::max<SLONG>(1000, kerosene * gKerosinPrice);

        /* A new plane is fitted out 6/8 economy, 1/8 first class (Planetyp.cpp:241), and is
         * valued at the load that cabin will actually carry rather than at a full one -
         * see expectedPaxPerFlight(). Going from two planes to four left revenue per flight
         * unchanged at ~101,000 while kerosene per flight rose from 55,922 to 77,384,
         * because the seats the old estimate paid for never flew. */
        SLONG seats = std::min<SLONG>(qType.Passagiere * 6 / 8, kExpectedPaxPerFlight);
        SLONG seatsFC = std::min<SLONG>(qType.Passagiere / 8, kExpectedPaxPerFlight / 6);
        SLONG revenue = seats * target->ticketPrice + seatsFC * target->ticketPriceFC;

        /* Ranked per dollar, not per plane hour.
         *
         * Per hour is the right question when the fleet has a slot to fill and the money to
         * fill it with. It stopped being the question when the network grew: the airline
         * finishes a game with 51 aeroplanes against a cap of 110, having spent 1.2 billion
         * on them in the last week alone, because the ranking always names the largest
         * aircraft in the catalogue and one costs 61 million. Cash is the constraint, and a
         * leg carries 306 passengers into a 467 seat cabin - the seats it is paying for do
         * not fly. */
        SLONG value = (revenue - cost) / (duration + 1);
        /* Ranked per crew member, because that is what runs out.
         *
         * `_planFlightJob` refuses to schedule an aeroplane whose crew is short of
         * ptAnzPiloten / ptAnzBegleiter (GameMechanic.cpp:2525-2535), and the labour market
         * is a fixed pool shared with the other three airlines. At a fleet of 161 the hiring
         * log reads "still missing 39 pilots / 346 attendants" and the airline makes 201
         * departures a day - exactly what it made with 110 aeroplanes. Fifty of them never
         * flew, while cash sat in the bank at over a billion. */
        /* Per dollar of cash the aeroplane will consume over the next few weeks, not per
         * dollar of purchase price.
         *
         * Price alone names the cheapest airframe on the board and the cheapest airframe on
         * the board is an Ilyushin Il 62: 9.9 million, and 10,500 litres an hour against an
         * A 300's 2,983. Its fuel is not scored in the last week, because the tank is
         * prepaid - but it is still cash, and cash is what buys the next aeroplane. Valuing
         * it on price alone bankrupted the airline outright (Money at the debt limit, image
         * at -831, score 0). So the denominator carries the fuel the aeroplane will burn
         * over kPlaneHorizonDays as well as what it costs to buy. */
        const __int64 legsPerDay = 24 / std::max<SLONG>(1, duration + 1);
        const __int64 cashOverHorizon = qType.Preis + legsPerDay * cost * kPlaneHorizonDays;
        const SLONG perMillion = cashOverHorizon > 0 ? static_cast<SLONG>(static_cast<__int64>(value) * 1000000 / cashOverHorizon) : 0;
        if (qType.Preis <= budget && perMillion > bestAffordableValue) {
            bestAffordableValue = perMillion;
            bestAffordableType = type;
        }
        if (perMillion <= bestValue) {
            continue;
        }
        bestValue = perMillion;
        bestType = type;
    }

    /* An airline with nothing in the air earns nothing, so the first planes are bought from
     * whatever is affordable; after that the slot is worth more than the wait. */
    if (bestType >= 0 && PlaneTypes[bestType].Preis > budget) {
        if (havePlanes >= kMinFleetBeforeSaving) {
            AT_Log("ClaudeBot::executeBuyPlane(): Saving for %s (%s), budget %s.", PlaneTypes[bestType].Name.c_str(),
                   Insert1000erDots64(PlaneTypes[bestType].Preis).c_str(), Insert1000erDots64(budget).c_str());
            return;
        }
        bestType = bestAffordableType;
    }

    if (bestType < 0) {
        mWantUsedPlane = true; /* new planes start at 9.9 million; try the museum */
        SLONG cheapest = -1;
        for (SLONG type : GameMechanic::getAvailablePlaneTypes()) {
            if (cheapest < 0 || PlaneTypes[type].Preis < PlaneTypes[cheapest].Preis) {
                cheapest = type;
            }
        }
        if (cheapest >= 0) {
            AT_Log("ClaudeBot::executeBuyPlane(): Budget %s, cheapest plane %s costs %s.", Insert1000erDots64(budget).c_str(),
                   PlaneTypes[cheapest].Name.c_str(), Insert1000erDots64(PlaneTypes[cheapest].Preis).c_str());
        }
        return;
    }

    /* As many as the cash and the cap allow, not one.
     *
     * The broker opens once a day and executeBuyPlane() took one aeroplane away from it,
     * so the fleet could only ever grow by one a day however rich the airline was - and it
     * ends a measured game with 1.3 billion in the bank against a fleet of 36. A hard cap
     * of 24 aeroplanes measured 582,176,386 against 652,779,269, so the last twelve are
     * worth about 70 million between them: the cash sitting idle is the expensive thing,
     * not the aeroplanes. GameMechanic::buyPlane() takes at most ten at a time. */
    const SLONG affordable = static_cast<SLONG>(budget / PlaneTypes[bestType].Preis);
    SLONG want = std::max<SLONG>(1, std::min<SLONG>(affordable, kMaxPlanes - havePlanes));

    /* buyPlane() takes ten at a time and the broker opens once a day, so one call is a cap
     * of ten aeroplanes a day however rich the airline is - and the fleet grew by exactly
     * ten on every one of the last ten days of a measured game. Nothing stops us calling it
     * again. */
    SLONG amount = 0;
    while (want > 0) {
        const SLONG batch = std::min<SLONG>(want, 10);
        if (GameMechanic::buyPlane(qPlayer, bestType, batch).empty()) {
            break;
        }
        amount += batch;
        want -= batch;
    }
    if (amount == 0) {
        return;
    }
    mWantUsedPlane = false;

    /* New plane: it needs crew before it may be scheduled, and the office has to learn
     * about it before the agency may count on it. */
    mVisitedPersonalToday = false;
    mPlaneStateStale = true;
    mNeedSchedule = true;

    AT_Log("ClaudeBot::executeBuyPlane(): Bought %ld x %s %s for %s each (%ld/h), cash now %s.", amount, PlaneTypes[bestType].Hersteller.c_str(),
           PlaneTypes[bestType].Name.c_str(), Insert1000erDots64(PlaneTypes[bestType].Preis).c_str(), bestValue, Insert1000erDots64(qPlayer.Money).c_str());
}

//--------------------------------------------------------------------------------------------
// Museum: second-hand planes.
//
// The cheapest new plane costs 9.9 million, which the early game never has. Used planes
// are priced by condition (ptPreis * Zustand^2 / 10000 / 120 * age) and are often a
// fraction of that, so they are the only way to grow the fleet before the routes have
// paid for a new one. Their WorstZustand is set to Zustand - 20 on purchase, so they can
// still be repaired 20 points for free.
//--------------------------------------------------------------------------------------------
void ClaudeBot::executeMuseum() {
    mVisitedMuseumToday = true;

    if (mRoutes.empty()) {
        return;
    }

    const RouteState *target = nullptr;
    for (const auto &qRoute : mRoutes) {
        if (target == nullptr || qRoute.bedarf > target->bedarf) {
            target = &qRoute;
        }
    }
    if (target == nullptr || target->distance <= 0) {
        return;
    }

    const __int64 budget = qPlayer.Money - kCashBuffer;
    if (budget <= 0) {
        return;
    }

    SLONG bestPlane = -1;
    SLONG bestValue = 0;
    for (SLONG i = 0; i < Sim.UsedPlanes.AnzEntries(); i++) {
        if (Sim.UsedPlanes.IsInAlbum(i) == 0) {
            continue;
        }
        const auto &qPlane = Sim.UsedPlanes[i];
        if (qPlane.Name.empty()) {
            continue;
        }
        if (qPlane.CalculatePrice() > budget) {
            continue;
        }
        /* Below 80 the game starts throwing accidents, each costing image and route
         * image, and repairing that far up is exactly the charge we avoid elsewhere. */
        if (qPlane.Zustand < 80) {
            continue;
        }
        if (target->distance > qPlane.ptReichweite * 1000) {
            continue;
        }

        int cost = 0;
        int duration = 0;
        int dist = 0;
        calcCostAndDuration(Cities.find(target->vonCity), Cities.find(target->nachCity), qPlane, false, cost, duration, dist);
        if (duration > 24) {
            continue;
        }

        SLONG seats = std::min<SLONG>(qPlane.MaxPassagiere, target->bedarf);
        SLONG seatsFC = std::min<SLONG>(qPlane.MaxPassagiereFC, target->bedarf);
        SLONG value = (seats * target->ticketPrice + seatsFC * target->ticketPriceFC - cost) / (duration + 1);
        if (value <= bestValue) {
            continue;
        }
        bestValue = value;
        bestPlane = i;
    }

    if (bestPlane < 0) {
        return;
    }

    CString name = Sim.UsedPlanes[bestPlane].Name;
    SLONG price = Sim.UsedPlanes[bestPlane].CalculatePrice();
    if (GameMechanic::buyUsedPlane(qPlayer, bestPlane) < 0) {
        return;
    }

    mWantUsedPlane = false;
    mVisitedPersonalToday = false; /* it needs a crew before it may be scheduled */
    mPlaneStateStale = true;
    mNeedSchedule = true;

    AT_Log("ClaudeBot::executeMuseum(): Bought used plane %s for %s (%ld/h), cash now %s.", name.c_str(), Insert1000erDots64(price).c_str(), bestValue,
           Insert1000erDots64(qPlayer.Money).c_str());
}

//--------------------------------------------------------------------------------------------
// Boss: gates.
//
// TafelData may only be read here. Bidding costs nothing and the auction settles at the next
// day boundary in favour of whoever bid last, so the only strategy available is to bid on
// everything, every day, and let the competitors decide how much rent we end up paying.
//--------------------------------------------------------------------------------------------
void ClaudeBot::executeBoss() {
    mVisitedBossToday = true;

    SLONG numPlanes = 0;
    for (SLONG c = 0; c < qPlayer.Planes.AnzEntries(); c++) {
        if (qPlayer.Planes.IsInAlbum(c) != 0) {
            numPlanes++;
        }
    }
    const SLONG wantGates = std::min<SLONG>(kMaxGates, std::max<SLONG>(1, numPlanes / kPlanesPerGate));
    const SLONG haveGates = qPlayer.Gates.NumRented;
    if (haveGates >= wantGates) {
        return;
    }

    SLONG bids = 0;
    for (SLONG i = 0; i < static_cast<SLONG>(TafelData.ByPositions.size()); i++) {
        const auto *pZettel = TafelData.ByPositions[i];
        if (pZettel == nullptr || pZettel->Type != CTafelZettel::Type::GATE) {
            continue;
        }
        /* bidOnGate() rejects a bid we are already the high bidder on. */
        if (pZettel->Player == qPlayer.PlayerNum) {
            continue;
        }
        if (GameMechanic::bidOnGate(qPlayer, i)) {
            bids++;
        }
    }

    /* Only ever when nothing is up for auction: canExpandAirport() refuses while a gate is
     * still unassigned, which is exactly the case the bidding above already covers. */
    bool expanded = false;
    if (bids == 0 && qPlayer.Money > 1000000 + kExpandCashReserve) {
        if (GameMechanic::canExpandAirport(qPlayer) == GameMechanic::ExpandAirportResult::Ok) {
            expanded = GameMechanic::expandAirport(qPlayer);
        }
    }

    if (bids > 0 || expanded) {
        AT_Log("ClaudeBot::executeBoss(): %ld gate(s) for %ld aeroplane(s), want %ld: bid on %ld, %s.", haveGates, numPlanes, wantGates, bids,
               expanded ? "commissioned a new one" : "did not build");
    }
}

//--------------------------------------------------------------------------------------------
// Advertising: buy image.
//
// WerbeKosten is not part of GetOpSaldo(), so campaigns are free in score terms and only
// limited by cash. Image pays twice: route passengers scale with
// (400 + 4*routeImage + airlineImage + 200) / 1100, which is a factor of 2.3 between no
// image and full image.
//--------------------------------------------------------------------------------------------
void ClaudeBot::executeAds() {
    mVisitedAdsToday = true;

    /* Largest campaign of a kind we can still pay for, or -1. */
    auto affordableSize = [&](SLONG type, SLONG minSize) {
        for (SLONG size = 5; size >= minSize; size--) {
            if (qPlayer.Money - gWerbePrice[type * 6 + size] >= kAdCashBuffer) {
                return size;
            }
        }
        return static_cast<SLONG>(-1);
    };

    /* Route image is the cheaper of the two - a flat 30,000 per point at every size - and
     * it counts four times as much in the passenger formula. Buy it first. */
    for (const auto &qRoute : mRoutes) {
        if (qPlayer.RentRouten.RentRouten[qRoute.id].Image >= 90) {
            continue;
        }
        /* Size 4 only, bought repeatedly until the route reaches the target.
         *
         * 120,000 buys exactly 4 points (cost / 30000), which is 30,000 a point. Size 3 pays
         * 56,000 for one point because 56000/30000 truncates from 1.87 to 1 - 87% worse - and
         * size 5 buys 46 points at a time for 1,400,000, the same 30,435 a point but in a
         * granularity that overshoots a clamp at 100. Looping the small one costs the same per
         * point and stops exactly on target. */
        while (qPlayer.RentRouten.RentRouten[qRoute.id].Image < 90) {
            if (qPlayer.Money - gWerbePrice[1 * 6 + 4] < kAdCashBuffer) {
                break;
            }
            if (!GameMechanic::buyAdvertisement(qPlayer, 1, 4, qRoute.id)) {
                break;
            }
        }
        AT_Log("ClaudeBot::executeAds(): Route %ld image now %ld.", qRoute.id, static_cast<SLONG>(qPlayer.RentRouten.RentRouten[qRoute.id].Image));
    }

    /* How much erosion the campaign we are about to buy has to survive. */
    if (mImageAdsDay >= 0 && Sim.Date > mImageAdsDay) {
        SLONG perDay = (mImageAfterAds - qPlayer.Image) / (Sim.Date - mImageAdsDay);
        perDay = std::max<SLONG>(0, perDay);
        /* Up immediately, down slowly: the fleet doubles about every ten days late in the
         * game, so an average that lags is an image that is short exactly when it grows. */
        mImageDecayPerDay = std::max(perDay, mImageDecayPerDay * 3 / 4);
    }

    /* Mirrors the ACTION_WERBUNG arm of Helper::checkRoomOpen(), which can only answer for
     * today: the agency is shut on Saturday (5) and Sunday (6). */
    auto adsOpenOn = [](SLONG weekday) { return weekday != 5 && weekday != 6; };
    SLONG daysToCover = 1;
    while (daysToCover < 7 && !adsOpenOn((Sim.Weekday + daysToCover) % 7)) {
        daysToCover++;
    }
    SLONG numPlanes = 0;
    for (SLONG c = 0; c < qPlayer.Planes.AnzEntries(); c++) {
        if (qPlayer.Planes.IsInAlbum(c) != 0) {
            numPlanes++;
        }
    }
    const SLONG saturation = (numPlanes < kSmallFleet) ? kImageSaturationSmallFleet : kImageSaturation;
    const SLONG target = std::min<SLONG>(kMaxTargetImage, saturation + mImageDecayPerDay * daysToCover);

    /* Airline image costs 50,000 a point and counts once; route image costs 30,000 and
     * counts four times, so a route point is worth six airline points. Airline image only
     * gets the cash a plane cannot use. */
    while (qPlayer.Image < target && qPlayer.Money > kAdCashBuffer) {
        SLONG size = affordableSize(0, 3);
        if (size < 0) {
            break;
        }
        if (!GameMechanic::buyAdvertisement(qPlayer, 0, size, -1)) {
            break;
        }
        AT_Log("ClaudeBot::executeAds(): Image campaign size %ld, image now %ld.", size, qPlayer.Image);
    }

    AT_Log("ClaudeBot::executeAds(): Image %ld, target %ld (saturation %ld + %ld a day over %ld days, %ld aeroplanes).", qPlayer.Image, target, saturation,
           mImageDecayPerDay, daysToCover, numPlanes);
    mImageAfterAds = qPlayer.Image;
    mImageAdsDay = Sim.Date;
}

//--------------------------------------------------------------------------------------------
// Office: fit out the planes.
//
// Every flight scores the cabin (Schedule.cpp:934-955): a plane with none of the four
// fittings loses 4 points, one with all of them at level 2 gains 8, and that score divided
// by ten is added to both the airline image and the route image on every single flight.
// FlugzeugUpgrades is outside the operating result, so only the cash matters.
//--------------------------------------------------------------------------------------------
void ClaudeBot::executeUpgrades() {
    mUpgradedToday = true;

    for (SLONG c = 0; c < qPlayer.Planes.AnzEntries(); c++) {
        if (qPlayer.Planes.IsInAlbum(c) == 0) {
            continue;
        }
        auto &qPlane = qPlayer.Planes[c];

        /* Committing to an upgrade we cannot pay for when it is applied is a classic way
         * to go bankrupt, so check the running total after every step. */
        auto affordable = [&]() { return qPlayer.Money - qPlayer.CalcPlanePropSum() > kCashBuffer; };

        if (qPlane.SitzeTarget != 2) {
            qPlane.SitzeTarget = 2;
            if (!affordable()) {
                qPlane.SitzeTarget = qPlane.Sitze;
            }
        }
        if (qPlane.TablettsTarget != 2) {
            qPlane.TablettsTarget = 2;
            if (!affordable()) {
                qPlane.TablettsTarget = qPlane.Tabletts;
            }
        }
        if (qPlane.DecoTarget != 2) {
            qPlane.DecoTarget = 2;
            if (!affordable()) {
                qPlane.DecoTarget = qPlane.Deco;
            }
        }
        /* Food is the one fitting with a recurring per-flight cost, which *is* scored:
         * FoodCosts[2] is 50 a passenger, which was 2.3 million in the last week of the
         * measurement. What it buys is three points of the cabin score, and the cabin score
         * only reaches the airline through `Image += Add / 10`. Image is already saturated -
         * CalcPassengers clamps 4 * routeImage + airlineImage + 200 at 1000 and we sit well
         * over it - so those points buy nothing at the margin. */
        if (qPlane.EssenTarget != kEssenTarget) {
            qPlane.EssenTarget = kEssenTarget;
            if (!affordable()) {
                qPlane.EssenTarget = qPlane.Essen;
            }
        }

        /* Measured, not assumed: raising the first class share from its default 25% of the
         * cabin to 50% cost 559,000 a week (657,245 -> 98,360). First class demand is
         * drawn against the competitors with a weight of 10000/price and needs 72 hours
         * of lead time rather than 48, so the converted seats do not fill.
         *
         * Which is an argument for going the other way as well. A first class seat costs two
         * economy seats (GameMechanic::decreaseFirstClassRatio splits ptPassagiere into
         * MaxPassagiere + 2 * MaxPassagiereFC) and the instrumented load shows first class
         * filling to about a fifth while economy is the cabin the demand is queueing for.
         *
         * The split is a target: the call moves MaxPassagiereTargetFC by ten percent of the
         * cabin and CPlane::DoOneStep copies it onto MaxPassagiereFC at the next day
         * boundary. So drive the loop off the target - MaxPassagiereFC does not move while
         * we are standing here, and decreaseFirstClassRatio always reports success. */
        while (qPlane.MaxPassagiereTargetFC > 0) {
            SLONG before = qPlane.MaxPassagiereTargetFC;
            GameMechanic::decreaseFirstClassRatio(qPlayer, c);
            if (qPlane.MaxPassagiereTargetFC >= before) {
                break;
            }
        }
    }
}

//--------------------------------------------------------------------------------------------
// Bank: borrow what we can whenever cash runs low. Loan interest sits outside the
// operating result, so debt costs us nothing in score terms - running out of cash does.
//--------------------------------------------------------------------------------------------
void ClaudeBot::executeBank() {
    mVisitedBankToday = true;

    SLONG limit = qPlayer.CalcCreditLimit();
    if (limit <= 0) {
        return;
    }
    if (GameMechanic::takeOutCredit(qPlayer, limit)) {
        AT_Log("ClaudeBot::executeBank(): Borrowed %s, now at %s cash / %s debt.", Insert1000erDots64(limit).c_str(), Insert1000erDots64(qPlayer.Money).c_str(),
               Insert1000erDots64(qPlayer.Credit).c_str());
    }
}

//--------------------------------------------------------------------------------------------
// Bank: sell shares.
//
// AktienEmission, AktienEmissionFee and the dividend (SollRendite) are all outside
// GetOpSaldo(), so equity is free money in score terms - and unlike the overdraft it is
// not capped at a million. That matters because everything that actually raises the score
// has to be bought: image (a factor 2.3 on route passengers), cabin fit-out and planes.
//
// MaxAktien starts at 20,000 and grows 5% every night up to 2,500,000, so roughly 5% of
// the company may be floated per day. Emitting early and often compounds: the cash buys
// planes and image, those raise the company value, and the share price follows it.
//--------------------------------------------------------------------------------------------
void ClaudeBot::executeStock() {
    mVisitedStockToday = true;

    /* The share price is pulled towards 10 * TrustedDividende every night, and
     * TrustedDividende creeps up towards Dividende by one every second profitable day. The
     * maximum of 25 therefore triples the price we can sell shares at over the first
     * month. The payout itself is (AnzAktien - own) * Dividende / 365 a day and is booked
     * as SollRendite, which the score ignores. */
    if (qPlayer.Dividende != kDividend) {
        GameMechanic::setDividend(qPlayer, kDividend);
    }

    SLONG maxShares = 0;
    if (GameMechanic::canEmitStock(qPlayer, &maxShares) != GameMechanic::EmitStockResult::Ok) {
        return;
    }

    const __int64 before = qPlayer.Money;
    if (!GameMechanic::emitStock(qPlayer, maxShares, 0)) {
        return;
    }

    AT_Log("ClaudeBot::executeStock(): Emitted %ld shares for %s at %ld, cash now %s.", maxShares, Insert1000erDots64(qPlayer.Money - before).c_str(),
           static_cast<SLONG>(qPlayer.Kurse[0]), Insert1000erDots64(qPlayer.Money).c_str());
}

//--------------------------------------------------------------------------------------------
// Travel agency: take the jobs we can actually fly, most profitable first.
//--------------------------------------------------------------------------------------------
void ClaudeBot::executeCheckAgent2() {
    mAgencyVisitsToday++;

    if (mPlaneStateStale) {
        /* No trustworthy view of the flight plans: accepting now risks a fine. */
        AT_Log("ClaudeBot::executeCheckAgent2(): Plane state stale, taking nothing.");
        return;
    }

    SLONG taken = 0;
    SLONG totalGain = 0;

    /* Greedy: repeatedly take the single most profitable job that still fits into an idle
     * window of the simulated flight plans, then shrink that window so the next choice
     * sees the hours it has already spent. */
    while (mJobsTakenToday < kMaxJobsPerDay) {
        SLONG bestJob = -1;
        SLONG bestPlane = -1;
        SLONG bestGap = -1;
        SLONG bestGain = kMinJobGain;
        PlaneTime bestStart{};

        for (SLONG i = 0; i < ReisebueroAuftraege.AnzEntries(); i++) {
            if (ReisebueroAuftraege.IsInAlbum(i) == 0) {
                continue;
            }
            const auto &qJob = ReisebueroAuftraege[i];
            if (qJob.VonCity == qJob.NachCity || qJob.Praemie <= 0) {
                continue;
            }
            if (qJob.BisDate < Sim.Date || qJob.Date > Sim.Date + kMaxPlanDate) {
                continue;
            }

            SLONG gap = -1;
            PlaneTime start{};
            SLONG gain = 0;
            SLONG p = findPlaneForJob(qJob, gap, start, gain);
            if (p < 0 || gain <= bestGain) {
                continue;
            }
            bestJob = i;
            bestPlane = p;
            bestGap = gap;
            bestGain = gain;
            bestStart = start;
        }

        if (bestJob < 0) {
            break;
        }

        const auto &qJob = ReisebueroAuftraege[bestJob];
        auto &qGap = mPlanes[bestPlane].gaps[bestGap];
        PlaneTime start{};
        PlaneTime back{};
        SLONG gain = 0;
        if (!fitJobIntoGap(qGap, qPlayer.Planes[mPlanes[bestPlane].id], qJob, start, back, gain)) {
            break; /* cannot happen: findPlaneForJob() just fitted it */
        }

        SLONG outObjectId = -1;
        if (!GameMechanic::takeFlightJob(qPlayer, bestJob, outObjectId)) {
            break;
        }

        /* Book it into the simulated state: the window is used up until the plane is back
         * in the city it started from. */
        qGap.start = back;

        mJobsTakenToday++;
        taken++;
        totalGain += bestGain;
        mNeedSchedule = true;
    }

    AT_Log("ClaudeBot::executeCheckAgent2(): Took %ld job(s) worth %ld, %ld taken today.", taken, totalGain, mJobsTakenToday);
}

//--------------------------------------------------------------------------------------------
// Freight depot: take the contracts we can deliver in full.
//
// gFrachten may only be read here and qPlayer.Frachten may not, so the decision runs
// entirely on the cached idle windows - exactly like the travel agency. A contract is only
// taken if every ton of it fits, because the premium is paid on the last flight and
// nothing before it.
//--------------------------------------------------------------------------------------------
void ClaudeBot::executeCheckAgent3() {
    mVisitedFreightToday = true;
    mFreightVisitsToday++;

    if (mPlaneStateStale) {
        AT_Log("ClaudeBot::executeCheckAgent3(): Plane state stale, taking nothing.");
        return;
    }

    /* The windows we are planning against, in the parallel form fitFreightIntoGaps() wants.
     * Committed back into mPlanes for every contract we actually take. */
    std::vector<SLONG> planeIds;
    for (const auto &qState : mPlanes) {
        if (qPlayer.Planes.IsInAlbum(qState.id) == 0) {
            continue;
        }
        planeIds.push_back(qState.id);
    }
    if (planeIds.empty()) {
        return;
    }

    std::vector<std::vector<PlaneGap>> gaps;
    for (const auto &qState : mPlanes) {
        if (qPlayer.Planes.IsInAlbum(qState.id) == 0) {
            continue;
        }
        gaps.push_back(qState.gaps);
    }

    SLONG taken = 0;
    SLONG totalGain = 0;

    /* Greedy, most profitable contract first, re-evaluated after every acceptance because
     * the windows the last one used are gone. */
    while (mFreightTakenToday < kMaxFreightPerDay) {
        SLONG bestJob = -1;
        SLONG bestGain = kMinFreightGain;
        std::vector<FreightLeg> bestLegs;

        for (SLONG i = 0; i < gFrachten.AnzEntries(); i++) {
            if (gFrachten.IsInAlbum(i) == 0) {
                continue;
            }
            const auto &qFreight = gFrachten[i];
            if (qFreight.VonCity == qFreight.NachCity || qFreight.Praemie <= 0) {
                continue;
            }
            if (qFreight.BisDate < Sim.Date || qFreight.Date > Sim.Date + kMaxPlanDate) {
                continue;
            }

            auto trial = gaps;
            SLONG cost = 0;
            std::vector<FreightLeg> legs;
            SLONG covered = fitFreightIntoGaps(planeIds, trial, qFreight, qFreight.Tons, cost, legs);
            if (covered < qFreight.Tons) {
                continue; /* a part delivery earns nothing and still risks the fine */
            }

            const SLONG gain = qFreight.Praemie - cost;
            if (gain <= bestGain) {
                continue;
            }
            bestJob = i;
            bestGain = gain;
            bestLegs = legs;
        }

        if (bestJob < 0) {
            break;
        }

        const SLONG tons = gFrachten[bestJob].Tons;
        SLONG outObjectId = -1;
        if (!GameMechanic::takeFreightJob(qPlayer, bestJob, outObjectId)) {
            break;
        }

        /* Book the windows the contract will use, so the next contract sees them spent. */
        for (const auto &qLeg : bestLegs) {
            gaps[qLeg.slot][qLeg.gap].start = qLeg.back;
        }

        mFreightTakenToday++;
        taken++;
        totalGain += bestGain;
        mNeedSchedule = true;

        AT_Log("ClaudeBot::executeCheckAgent3(): Took %ld tons over %ld leg(s), gain %ld.", tons, static_cast<SLONG>(bestLegs.size()), bestGain);
    }

    /* Hand the consumed windows back so the travel agency does not sell them twice. */
    SLONG slot = 0;
    for (auto &qState : mPlanes) {
        if (qPlayer.Planes.IsInAlbum(qState.id) == 0) {
            continue;
        }
        qState.gaps = gaps[slot++];
    }

    AT_Log("ClaudeBot::executeCheckAgent3(): Took %ld contract(s) worth %ld, %ld taken today.", taken, totalGain, mFreightTakenToday);
}

//--------------------------------------------------------------------------------------------
// Arab: tanks, and kerosene bought when it is cheap.
//
// The kerosene price may be read here and in the personal office (RULES.md), and holds for
// the whole day - cacheKerosinPrice() seeds gKerosinPrice from whichever comes first.
//--------------------------------------------------------------------------------------------
/* Measures the day's burn off yesterday's balance and caches it for the Arab and the broker,
 * neither of which may read it.
 *
 * `BilanzGestern` is legal in the personal office and only with a financial advisor employed,
 * and the kerosene price is legal there too - so this is the one room where the estimate can
 * be formed at all. `KerosinGespart` is the value of what came out of the tank and
 * `KerosinFlug` what was bought at the gate, so the two together are the day's burn
 * (BookFlight, Schedule.cpp:743-751). */
void ClaudeBot::cacheFuelBurn() {
    if (qPlayer.HasBerater(BERATERTYP_GELD) <= 0) {
        return; /* not allowed to look; keep yesterday's figure */
    }
    if (gKerosinPrice <= 0) {
        return;
    }
    const __int64 spentYesterday = qPlayer.BilanzGestern.KerosinGespart - qPlayer.BilanzGestern.KerosinFlug;
    mFuelUnitsPerDay = static_cast<SLONG>(std::max<__int64>(0, spentYesterday / gKerosinPrice));
}

/* RULES.md gates `qPlayer.TankInhalt` behind a kerosene advisor above talent 30. */
bool ClaudeBot::canReadTankInhalt() const { return qPlayer.HasBerater(BERATERTYP_KEROSIN) > 30; }

/* Capacity we want: a few days of flying, so a cheap phase of the price walk can be stocked
 * against and a dear one sat out. Zero until the office has measured a day's burn. */
SLONG ClaudeBot::fuelTankTarget() const { return mFuelUnitsPerDay * kTankDaysOfBurn; }

/* Whether today is a day to stock up. The price is cached once a day in the office or at the
 * Arab (RULES.md permits it in both and fixes it for the day), so this may be asked anywhere
 * - including from RobotPlan(), which decides whether the walk to the Arab is worth it. */
bool ClaudeBot::fuelIsCheap() const { return gKerosinPrice > 0 && gKerosinPrice * 100 * 100 <= gKerosinAvgX100 * kKerosinBuyBelowPercent; }

void ClaudeBot::executeKerosinTanks() {
    mVisitedTanksToday = true;

    cacheKerosinPrice();

    /* Planes only draw from the tank while it is open, and a closed tank is dead capital.
     * Set unconditionally: `TankOpen` may only be *read* in the personal office (RULES.md),
     * and the call is idempotent. */
    GameMechanic::setKerosinTankOpen(qPlayer, TRUE);

    const SLONG target = fuelTankTarget();
    if (qPlayer.Tank >= target) {
        return;
    }

    /* Capacity is worthless without the cash to fill it, so a tank is only bought out of the
     * surplus left after a full load at the top of the price band. Larger tanks are much
     * cheaper per unit (1000 / 800 / 700 / 600), so take the biggest that still fits inside
     * what is missing. */
    while (qPlayer.Tank < target) {
        SLONG bought = -1;
        for (SLONG type = static_cast<SLONG>(TankSize.size()) - 1; type >= 0; type--) {
            const SLONG capacity = TankSize[type] / 1000;
            /* The smallest type may overshoot; anything larger has to earn its place. */
            if (type > 0 && qPlayer.Tank + capacity > target) {
                continue;
            }
            const __int64 needed = TankPrice[type] + static_cast<__int64>(capacity) * kKerosinPriceMax;
            if (qPlayer.Money - needed < kKerosinCashReserve) {
                continue;
            }
            if (!GameMechanic::buyKerosinTank(qPlayer, type, 1)) {
                continue;
            }
            bought = capacity;
            break;
        }
        if (bought < 0) {
            break;
        }
        AT_Log("ClaudeBot::executeKerosinTanks(): Bought a %ld unit tank, capacity now %ld of %ld wanted.", bought, static_cast<SLONG>(qPlayer.Tank), target);
    }
}

void ClaudeBot::executeBuyKerosin() {
    mVisitedKerosinToday = true;

    cacheKerosinPrice();

    if (qPlayer.Tank <= 0) {
        return;
    }

    const SLONG price = Sim.HoleKerosinPreis(kKerosinGrade);
    if (price <= 0 || !fuelIsCheap()) {
        return; /* dear today - burn what is in the tank, or buy at the gate */
    }

    /* Fill the tank. Every unit bought here at a low price replaces one that would have been
     * bought at the gate at the price of the day it is burnt, and both land in the same
     * cumulated saldo, so the whole spread is score. */
    const SLONG want = qPlayer.Tank;
    /* Without a kerosene advisor the tank content may not be read, so assume it is empty and
     * let GameMechanic::buyKerosin() clamp the purchase to the free capacity. */
    SLONG amount = want - (canReadTankInhalt() ? static_cast<SLONG>(qPlayer.TankInhalt) : 0);
    if (amount <= 0) {
        return;
    }

    const __int64 budget = qPlayer.Money - kKerosinCashReserve;
    if (budget <= 0) {
        return;
    }
    amount = static_cast<SLONG>(std::min<__int64>(amount, budget / price));
    if (amount < kKerosinMinPurchase) {
        return;
    }

    if (GameMechanic::buyKerosin(qPlayer, kKerosinGrade, amount)) {
        AT_Log("ClaudeBot::executeBuyKerosin(): Bought %ld units at %ld (running mean %ld), tank now %ld/%ld at an average of %.0f.", amount, price,
               gKerosinAvgX100 / 100, static_cast<SLONG>(qPlayer.TankInhalt), static_cast<SLONG>(qPlayer.Tank), qPlayer.TankPreis);
    }
}

//--------------------------------------------------------------------------------------------
// Office: put the jobs we own into the flight plans.
//--------------------------------------------------------------------------------------------
void ClaudeBot::executeOffice() {
    /* The office is one of the two rooms where the kerosene price and yesterday's balance
     * may be read, and the price holds for the whole day - so everything that needs either
     * of them anywhere else is cached here. */
    cacheKerosinPrice();
    cacheFuelBurn();

    /* A grounded plane will not fly anything in its plan, and every job left on it turns
     * into a fine. Free them up so the scheduler below can move them to a healthy plane. */
    for (SLONG c = 0; c < qPlayer.Planes.AnzEntries(); c++) {
        if (qPlayer.Planes.IsInAlbum(c) == 0) {
            continue;
        }
        if (qPlayer.Planes[c].Problem == 0) {
            continue;
        }
        if (GameMechanic::clearFlightPlan(qPlayer, c)) {
            AT_Log("ClaudeBot::executeOffice(): Cleared plan of grounded plane %s (%ld hours).", qPlayer.Planes[c].ptName.c_str(),
                   static_cast<SLONG>(qPlayer.Planes[c].Problem));
        }
    }

    SLONG planned = schedulePendingJobs();
    if (kUseFreight) {
        planned += schedulePendingFreight();
    }
    planned += scheduleRouteFlights();
    refreshPlaneState();

    mPlaneStateStale = false;
    mNeedSchedule = false;

    AT_Log("ClaudeBot::executeOffice(): Scheduled %ld job(s), %ld plane(s) known.", planned, static_cast<SLONG>(mPlanes.size()));
}

/* Rebuilds the cached availability of every plane from the real flight plans. Only legal
 * in the office (or with a working laptop), which is why the agency has to work off the
 * cache this produces. */
void ClaudeBot::refreshPlaneState() {
    mPlanes.clear();
    for (SLONG c = 0; c < qPlayer.Planes.AnzEntries(); c++) {
        if (qPlayer.Planes.IsInAlbum(c) == 0) {
            continue;
        }
        const auto &qPlane = qPlayer.Planes[c];
        if (qPlane.Problem != 0) {
            continue; /* grounded, cannot be planned at all */
        }

        PlaneTime earliest{Sim.Date, static_cast<int>(Sim.GetHour()) + 2};
        auto avail = Helper::getPlaneAvailableTimeLoc(qPlane, {}, earliest);

        PlaneState state;
        state.id = c;
        state.avail = avail.first;
        state.city = avail.second;
        state.gaps = collectGaps(qPlane);
        mPlanes.push_back(state);
    }
}

/* The idle windows in a plane's flight plan: the hours between one flight landing and the
 * next taking off, together with the city the plane waits in.
 *
 * The open tail after the last planned flight is deliberately *not* a window. A job placed
 * there would leave the plane wherever the job ended, and scheduleRouteFlights() gives up
 * on a plane that does not stand on one of our routes - it would idle for the whole
 * horizon. A window bounded by a following flight cannot strand the plane: the game flies
 * the empty return itself. */
std::vector<ClaudeBot::PlaneGap> ClaudeBot::collectGaps(const CPlane &qPlane) const {
    std::vector<PlaneGap> gaps;

    /* planFlightJob() refuses anything earlier than this, so no window may start before it. */
    const PlaneTime earliest{Sim.Date, static_cast<int>(Sim.GetHour()) + 2};
    const auto &qPlan = qPlane.Flugplan.Flug;

    /* Entries are kept sorted by departure (CPlane::UpdateFlightPlan), so one pass is
     * enough. Automatic flights count as occupied time like any other. */
    PlaneTime free = earliest;
    SLONG city = qPlane.Flugplan.StartCity;

    for (SLONG e = 0; e < qPlan.AnzEntries(); e++) {
        const auto &qFPE = qPlan[e];
        if (qFPE.ObjectType == 0) {
            continue;
        }

        const PlaneTime start{qFPE.Startdate, qFPE.Startzeit};
        const PlaneTime end{qFPE.Landedate, qFPE.Landezeit + 1};

        if (start >= free && start - free >= kMinGapHours) {
            PlaneGap gap;
            gap.start = free;
            gap.end = start;
            gap.city = city;
            gaps.push_back(gap);
        }

        if (end > free) {
            free = end;
        }
        city = static_cast<SLONG>(qFPE.NachCity);
    }

    return gaps;
}

/* Fills the remaining free time of every plane with flights on the routes we rent.
 *
 * Two mechanics drive the layout (CFlugplanEintrag::CalcPassengers):
 *  - a flight booked less than 48 hours ahead loses up to half its economy demand, and
 *    first class needs 72 hours, so the plan is filled to the end of the horizon;
 *  - departing or landing outside 05:00-22:00 costs a sixth of the passengers each.
 *
 * Route flights always continue from where the plane already is, so no automatic flight
 * is ever inserted between them. Only legal in the office. */
SLONG ClaudeBot::scheduleRouteFlights() {
    if (mRoutes.empty()) {
        return 0;
    }

    /* Ticket prices live on the route, not on the flight, so they must be set before the
     * flights are planned - planFlightJob() copies them into the plan entry. */
    for (auto &qRoute : mRoutes) {
        if (qRoute.pricesSet) {
            continue;
        }
        if (GameMechanic::setRouteTicketPriceBoth(qPlayer, qRoute.id, qRoute.ticketPrice, qRoute.ticketPriceFC)) {
            qRoute.pricesSet = true;
            AT_Log("ClaudeBot::scheduleRouteFlights(): Route %ld priced at %ld / %ld (FC).", qRoute.id, qRoute.ticketPrice, qRoute.ticketPriceFC);
        }
    }

    const PlaneTime horizon{Sim.Date + kMaxPlanDate, 23};
    SLONG planned = 0;

    /* Legs already laid on each route in this pass. A route's demand regenerates by a
     * seventh a day (CRouten::NewDay) while every flight consumes it, so piling all our
     * flights onto one route empties it - spread them out instead.
     *
     * Spread them in proportion to what each route wants per day, not evenly. Evenly is
     * what the game punishes: a route is confiscated after twenty days below 10% of its
     * weekly demand (PLAYER::NewDay, Player.cpp:1511), and that share is measured against
     * the route's own size. An equal split gave Lanzarote (299 passengers a day) 41 legs a
     * week and pinned it at 100%, while New York (4,362 a day) got 6 and sat at 1-4% - one
     * route saturated and drained, the other a fortnight from being taken away. Weighting
     * by demand parks every pair we hold at roughly the same utilisation instead. */
    std::vector<SLONG> legsOnRoute(mRoutes.size(), 0);

    /* How many passengers each pair can still deliver over the horizon, and how many seats
     * we have already pointed at it.
     *
     * A pair's value per hour is computed from its demand, and demand is finite: CRouten
     * ::NewDay pulls `Bedarf` a seventh of the way towards `AnzPassagiere() * 4.27` every
     * night while every flight subtracts the passengers it carried (Schedule.cpp:855). Past
     * that inflow a pair simply has nothing more to sell, and the eighth aeroplane on it
     * flies the same route for none of the fare and all of the kerosene. That is what the
     * end of a measured game looks like: from day 86 the fleet grows 23 -> 36 and the daily
     * departures 52 -> 81, while passengers a day sit flat at 22,000 and revenue per flight
     * falls from 2.33M to 1.51M. Kerosene over the same fortnight goes 13.2M -> 22.1M a day.
     *
     * So the gate below ranks pairs on what the *next* leg would earn, not on what the pair
     * is worth in the abstract. A pair we have filled drops out of the running and the
     * weaker pair nobody would otherwise touch becomes the best hour available. */
    /* Passengers waiting on pair r by the end of day d, counted from today - the pool
     * `Bedarf` holds now plus what arrives on each night in between. Both directions, and
     * both are flown.
     *
     * The pool is the whole story and it is a *dated* quantity. Charging a week of demand
     * against a single number said New York could absorb 31,000 more passengers whenever it
     * was asked, so New York was the best hour available to every aeroplane on every day,
     * and the seven pairs rented beside it - London with 12,056 people standing on the
     * platform, Paris, Warsaw, Frankfurt - were flown exactly zero times and taken away
     * again for it. What New York can really sell is what has arrived since the last
     * departure, which is a seventh of `AnzPassagiere() * 4.27` a night. */
    const SLONG kHorizonDays = kMaxPlanDate + 1;
    std::vector<SLONG> arrivedBy(mRoutes.size() * kHorizonDays, 0);
    std::vector<SLONG> soldOn(mRoutes.size() * kHorizonDays, 0);
    for (SLONG r = 0; r < static_cast<SLONG>(mRoutes.size()); r++) {
        const auto &qRoute = mRoutes[r];
        /* What a night refills once we are draining the pair, i.e. what it is worth a day. */
        const SLONG inflowPerDay = qRoute.anzPax * 427 / 100 / 7;
        for (SLONG d = 0; d < kHorizonDays; d++) {
            arrivedBy[r * kHorizonDays + d] = 2 * (qRoute.bedarf + inflowPerDay * d);
        }
    }

    /* Passengers we may still pick up off pair r on day d: everything that has arrived by
     * then, less everything already taken on that day and before it. */
    auto headroom = [&](SLONG r, SLONG day) {
        day = std::min<SLONG>(std::max<SLONG>(day, 0), kHorizonDays - 1);
        SLONG sold = 0;
        for (SLONG d = 0; d <= day; d++) {
            sold += soldOn[r * kHorizonDays + d];
        }
        return std::max<SLONG>(0, arrivedBy[r * kHorizonDays + day] - sold);
    };

    /* What one departure actually takes out of the pool - which is not a full cabin.
     * CalcPassengers scales the queue by `(3B - 10) / Ticketpreis` and then by
     * `(400 + ImageTotal) / 1100`, so at our price and a saturated image a departure lifts
     * about two thirds of what is standing there, and the rest waits for the next one
     * (Schedule.cpp:343-346, 386). Charging a whole cabin against the pool drained it
     * roughly twice as fast as the game does, which retired New York from the running
     * halfway through the day and handed the hour to a pair worth a third as much. */
    auto legTakes = [&](SLONG r, SLONG day, SLONG seats) { return std::min<SLONG>(seats, headroom(r, day) * kPoolCaptureSharePercent / 100); };

    /* The legs already standing in the flight plans have a claim on that demand too. This
     * pass only ever fills the gaps left by the last one, so counting only what it adds
     * would report a full pair as empty on every day but the first. They are collected
     * first and charged against the pool in date order, because what each one takes
     * depends on what the ones before it left behind. */
    struct PlannedLeg {
        SLONG day;
        SLONG route;
        SLONG seats;
    };
    std::vector<PlannedLeg> plannedLegs;
    for (SLONG c = 0; c < qPlayer.Planes.AnzEntries(); c++) {
        if (qPlayer.Planes.IsInAlbum(c) == 0) {
            continue;
        }
        const auto &qPlane = qPlayer.Planes[c];
        const auto &qPlan = qPlane.Flugplan.Flug;
        for (SLONG e = 0; e < qPlan.AnzEntries(); e++) {
            const auto &qFPE = qPlan[e];
            if (qFPE.ObjectType != 1 || qFPE.Startdate < Sim.Date || qFPE.Startdate > horizon.getDate()) {
                continue;
            }
            for (SLONG r = 0; r < static_cast<SLONG>(mRoutes.size()); r++) {
                if (qFPE.ObjectId != mRoutes[r].id && qFPE.ObjectId != mRoutes[r].reverseId) {
                    continue;
                }
                plannedLegs.push_back({qFPE.Startdate - Sim.Date, r, qPlane.MaxPassagiere + qPlane.MaxPassagiereFC});
                break;
            }
        }
    }

    std::sort(plannedLegs.begin(), plannedLegs.end(), [](const PlannedLeg &a, const PlannedLeg &b) { return a.day < b.day; });
    for (const auto &qLeg : plannedLegs) {
        soldOn[qLeg.route * kHorizonDays + qLeg.day] += legTakes(qLeg.route, qLeg.day, qLeg.seats);
    }

    for (SLONG c = 0; c < qPlayer.Planes.AnzEntries(); c++) {
        if (qPlayer.Planes.IsInAlbum(c) == 0) {
            continue;
        }
        const auto &qPlane = qPlayer.Planes[c];
        if (qPlane.Problem != 0) {
            continue;
        }

        PlaneTime earliest{Sim.Date, static_cast<int>(Sim.GetHour()) + 2};
        auto avail = Helper::getPlaneAvailableTimeLoc(qPlane, {}, earliest);
        PlaneTime time = avail.first;
        SLONG city = avail.second;

        /* The best any pair we hold is worth to *this* aeroplane, per hour it flies.
         *
         * The spreading rule below hands legs out by demand share, which says nothing about
         * what an hour on a pair is worth. That is fine while every pair we hold cleared
         * kMinRouteValuePerHour, and stops being fine the moment one does not - a pair
         * rented for a castaway, or one the fleet has outgrown. Renting Berlin-Lanzarote
         * for the starting 737-400 and leaving the spreading alone pulled Boeing 777-300s
         * onto a 3,504 km pair worth a quarter of Rio an hour: 440,211,765 against
         * 532,263,251.
         *
         * Measured against the best pair *this* plane can reach rather than against a fixed
         * floor. An absolute gate leaks: Berlin-Delhi is worth 45,644/h to the starting
         * 757-300 against a floor of 45,000, so on any day the route's Bedarf dipped the
         * 757 counted as having nothing worthwhile either and joined the 737 on Lanzarote -
         * twice the legs for a fifth of the saldo by day 60, and 473,873,231 overall. */
        /* What one more leg on pair r is worth to this aeroplane: the pair's value per hour,
         * scaled by the share of a full cabin the pair can still fill. */
        auto marginalValue = [&](SLONG r, SLONG day) {
            const auto &qRoute = mRoutes[r];
            const SLONG seats = qPlane.MaxPassagiere + qPlane.MaxPassagiereFC;
            if (seats <= 0) {
                return static_cast<SLONG>(0);
            }
            const SLONG sellable = std::min<SLONG>(seats, headroom(r, day));
            return qRoute.valuePerHour * sellable / seats;
        };

        while (time < horizon) {
            const SLONG day = time.getDate() - Sim.Date;
            SLONG bestReachableValue = 0;
            for (SLONG r = 0; r < static_cast<SLONG>(mRoutes.size()); r++) {
                if (mRoutes[r].distance <= qPlane.ptReichweite * 1000) {
                    bestReachableValue = std::max(bestReachableValue, marginalValue(r, day));
                }
            }

            /* Pick the route leg that departs where the plane already stands; anything
             * else would need an empty flight to reposition. */
            SLONG routeId = -1;
            SLONG routeIdx = -1;
            ULONG destCity = 0;
            for (SLONG r = 0; r < static_cast<SLONG>(mRoutes.size()); r++) {
                const auto &qRoute = mRoutes[r];
                if (qRoute.distance > qPlane.ptReichweite * 1000) {
                    continue;
                }
                if (marginalValue(r, day) * 100 < bestReachableValue * kMinRouteValueShare) {
                    continue;
                }
                /* Least-served route first, measured as legs per unit of daily demand so
                 * that a small route cannot soak up the fleet. Cross-multiplied to keep it
                 * in integers. */
                if (routeIdx >= 0 &&
                    legsOnRoute[r] * std::max<SLONG>(1, mRoutes[routeIdx].anzPax) >= legsOnRoute[routeIdx] * std::max<SLONG>(1, qRoute.anzPax)) {
                    continue;
                }
                if (static_cast<ULONG>(city) == qRoute.vonCity) {
                    routeId = qRoute.id;
                    routeIdx = r;
                    destCity = qRoute.nachCity;
                } else if (static_cast<ULONG>(city) == qRoute.nachCity && qRoute.reverseId >= 0) {
                    routeId = qRoute.reverseId;
                    routeIdx = r;
                    destCity = qRoute.vonCity;
                }
            }
            if (routeId < 0) {
                /* Nothing from here clears the gate *now*. If a pair is reachable at all,
                 * that is a statement about today's queue, not about the week: wait for the
                 * night to refill it rather than parking the aeroplane until the horizon. */
                bool reachable = false;
                for (const auto &qRoute : mRoutes) {
                    if (qRoute.distance > qPlane.ptReichweite * 1000) {
                        continue;
                    }
                    if (static_cast<ULONG>(city) == qRoute.vonCity || (static_cast<ULONG>(city) == qRoute.nachCity && qRoute.reverseId >= 0)) {
                        reachable = true;
                        break;
                    }
                }
                if (!reachable) {
                    break; /* plane is somewhere our routes do not touch */
                }
                time = PlaneTime{time.getDate() + 1, 5};
                continue;
            }

            int cost = 0;
            int duration = 0;
            int dist = 0;
            calcCostAndDuration(Cities.find(city), Cities.find(destCity), qPlane, false, cost, duration, dist);
            if (duration > 24) {
                break;
            }

            /* Fly around the clock. CalcPassengers docks a night departure and a night
             * landing a sixth each (Schedule.cpp:355-361), but it applies them to a figure
             * that was capped at one and a half times the cabin first (line 321): while the
             * pool holds more than 1.5x our seats, the two penalties together come to 25/36
             * of 1.5, which is still a full aeroplane. Daylight, by contrast, costs whole
             * legs - a nine hour leg to Delhi fits the 05:00-22:00 window once a day and
             * leaves the plane parked for the other fifteen hours. */
            if (time > horizon) {
                break;
            }
            if (time.getDate() == Sim.Date && time.getHour() < Sim.GetHour() + 2) {
                time = PlaneTime{Sim.Date, static_cast<int>(Sim.GetHour()) + 2};
            }

            /* Park the night at home rather than at the far end of the route.
             *
             * A route pair of even leg length puts the plane back where it started, so the
             * city it stands in at 05:00 is the city it will stand in all night, every
             * night - the phase never breaks by itself. Both cities are worth the same to
             * the route, but not to the agency: jobs are overwhelmingly offered out of the
             * home airport, and an overnight window there is the only free capacity the
             * airline has. Giving up the last leg of the day once costs one leg and moves
             * the plane into the home phase for good. */
            const PlaneTime after = time + duration + 1;
            const bool anotherLegFitsToday = (after.getDate() == time.getDate() && after.getHour() >= 5 && after.getHour() + duration <= 22);
            /* ...but only where giving up the leg actually buys the night at home. On a pair
             * whose leg is long enough that two of them never fit between 05:00 and 22:00,
             * `anotherLegFitsToday` is false at every hour of every day, so the rule does not
             * postpone the leg - it cancels it, and the loop offers the same pair again
             * tomorrow and cancels it again. Any plane whose turn came up on Delhi or New
             * York was parked for the entire planning horizon, which is why 41 legs a week
             * went to Lanzarote (short enough to fly twice a day) against 6 to New York. */
            const bool pairFitsTwiceADay = (2 * (duration + 1) <= kUsableHoursPerDay);
            if (pairFitsTwiceADay && !anotherLegFitsToday && city == Sim.HomeAirportId && static_cast<SLONG>(destCity) != Sim.HomeAirportId) {
                time = PlaneTime{time.getDate() + 1, 5};
                continue;
            }

            if (!GameMechanic::planRouteJob(qPlayer, c, routeId, time.getDate(), time.getHour())) {
                break;
            }
            planned++;
            legsOnRoute[routeIdx]++;
            const SLONG legDay = std::min<SLONG>(std::max<SLONG>(day, 0), kHorizonDays - 1);
            soldOn[routeIdx * kHorizonDays + legDay] += legTakes(routeIdx, legDay, qPlane.MaxPassagiere + qPlane.MaxPassagiereFC);

            time += duration + 1;
            city = static_cast<SLONG>(destCity);
        }
    }

    return planned;
}

/* Places every job we own that is not in a flight plan yet, into the idle windows between
 * the route legs. Jobs are taken in order of urgency (tightest deadline first) so that a
 * lucrative long-window job cannot squeeze out one that expires tomorrow.
 *
 * Runs before scheduleRouteFlights() so that the windows it works on are the ones the
 * agency saw. The route legs that follow each window keep the plane on its network: the
 * game flies the empty return itself, and fitJobIntoGap() has already paid for it.
 *
 * Everything here reads qPlayer.Auftraege and the flight plans, so it may only run in
 * the office. */
SLONG ClaudeBot::schedulePendingJobs() {
    /* (deadline, jobId) of everything still unplanned */
    std::vector<std::pair<SLONG, SLONG>> open;
    for (SLONG j = 0; j < qPlayer.Auftraege.AnzEntries(); j++) {
        if (qPlayer.Auftraege.IsInAlbum(j) == 0) {
            continue;
        }
        const auto &qJob = qPlayer.Auftraege[j];
        if (qJob.InPlan != 0) {
            continue; /* already in a flight plan or already flown */
        }
        if (qJob.BisDate < Sim.Date) {
            continue; /* expired: nothing to be done, the fine is already sunk */
        }
        open.emplace_back(qJob.BisDate, j);
    }
    std::sort(open.begin(), open.end(), [](const auto &a, const auto &b) { return a.first < b.first; });

    /* The windows are tracked across the whole loop: two jobs may share one night, but
     * only if the second still fits after the first has been booked into it. */
    std::vector<SLONG> planeIds;
    std::vector<std::vector<PlaneGap>> planeGaps;
    for (SLONG c = 0; c < qPlayer.Planes.AnzEntries(); c++) {
        if (qPlayer.Planes.IsInAlbum(c) == 0) {
            continue;
        }
        if (qPlayer.Planes[c].Problem != 0) {
            continue;
        }
        planeIds.push_back(c);
        planeGaps.push_back(collectGaps(qPlayer.Planes[c]));
    }

    SLONG planned = 0;
    for (const auto &entry : open) {
        const SLONG j = entry.second;
        const auto &qJob = qPlayer.Auftraege[j];

        SLONG bestPlane = -1;
        SLONG bestGap = -1;
        SLONG bestGain = 0;
        PlaneTime bestStart{};
        PlaneTime bestBack{};

        for (SLONG p = 0; p < static_cast<SLONG>(planeIds.size()); p++) {
            const auto &qPlane = qPlayer.Planes[planeIds[p]];
            if (!planeCanFly(qPlane, qJob)) {
                continue;
            }

            for (SLONG g = 0; g < static_cast<SLONG>(planeGaps[p].size()); g++) {
                PlaneTime start{};
                PlaneTime back{};
                SLONG gain = 0;
                if (!fitJobIntoGap(planeGaps[p][g], qPlane, qJob, start, back, gain)) {
                    continue;
                }
                /* Earliest slot wins: the job is already owned, so the fine for letting it
                 * expire dwarfs the difference between two windows that both fit it. */
                if (bestPlane >= 0 && !(start < bestStart)) {
                    continue;
                }
                bestPlane = p;
                bestGap = g;
                bestGain = gain;
                bestStart = start;
                bestBack = back;
            }
        }

        if (bestPlane < 0) {
            continue;
        }

        /* planFlightJob() refuses anything earlier than Sim.GetHour() + 2 today */
        if (bestStart.getDate() == Sim.Date && bestStart.getHour() < Sim.GetHour() + 2) {
            bestStart = PlaneTime{Sim.Date, static_cast<int>(Sim.GetHour()) + 2};
        }

        if (GameMechanic::planFlightJob(qPlayer, planeIds[bestPlane], j, bestStart.getDate(), bestStart.getHour())) {
            planeGaps[bestPlane][bestGap].start = bestBack;
            planned++;
            AT_Log("ClaudeBot::schedulePendingJobs(): Job %s -> %s on plane %s at %ld/%02ld, gain %ld.", Cities[qJob.VonCity].Name.c_str(),
                   Cities[qJob.NachCity].Name.c_str(), qPlayer.Planes[planeIds[bestPlane]].Name.c_str(), bestStart.getDate(), bestStart.getHour(), bestGain);
        }
    }

    return planned;
}

/* Places every ton of every freight contract we own that is not in a flight plan yet.
 *
 * CFracht::TonsOpen is the tonnage neither flown nor planned; PLAYER::UpdateFrachtauftrags-
 * Usage() recomputes it after every successful planFreightJob(), so the loop simply keeps
 * planning until it reaches zero. Contracts are taken in deadline order: an incomplete
 * delivery pays nothing at all, so the tightest one has first call on the windows.
 *
 * Reads qPlayer.Frachten and the flight plans, so office only. */
SLONG ClaudeBot::schedulePendingFreight() {
    /* (deadline, index) of every contract that still has unplanned tonnage */
    std::vector<std::pair<SLONG, SLONG>> open;
    for (SLONG j = 0; j < qPlayer.Frachten.AnzEntries(); j++) {
        if (qPlayer.Frachten.IsInAlbum(j) == 0) {
            continue;
        }
        const auto &qFreight = qPlayer.Frachten[j];
        if (qFreight.InPlan == -1 || qFreight.TonsOpen <= 0) {
            continue; /* delivered, or every ton is already in a plan */
        }
        if (qFreight.BisDate < Sim.Date) {
            continue; /* expired: the fine is already sunk */
        }
        open.emplace_back(qFreight.BisDate, j);
    }
    if (open.empty()) {
        return 0;
    }
    std::sort(open.begin(), open.end(), [](const auto &a, const auto &b) { return a.first < b.first; });

    std::vector<SLONG> planeIds;
    std::vector<std::vector<PlaneGap>> gaps;
    for (SLONG c = 0; c < qPlayer.Planes.AnzEntries(); c++) {
        if (qPlayer.Planes.IsInAlbum(c) == 0) {
            continue;
        }
        if (qPlayer.Planes[c].Problem != 0) {
            continue;
        }
        planeIds.push_back(c);
        gaps.push_back(collectGaps(qPlayer.Planes[c]));
    }
    if (planeIds.empty()) {
        return 0;
    }

    SLONG planned = 0;
    for (const auto &entry : open) {
        const SLONG j = entry.second;

        SLONG cost = 0;
        std::vector<FreightLeg> legs;
        fitFreightIntoGaps(planeIds, gaps, qPlayer.Frachten[j], qPlayer.Frachten[j].TonsOpen, cost, legs);

        for (const auto &qLeg : legs) {
            /* planFlightJob() refuses anything earlier than Sim.GetHour() + 2 today */
            PlaneTime start = qLeg.start;
            if (start.getDate() == Sim.Date && start.getHour() < Sim.GetHour() + 2) {
                start = PlaneTime{Sim.Date, static_cast<int>(Sim.GetHour()) + 2};
            }
            if (qPlayer.Frachten[j].TonsOpen <= 0) {
                break; /* the tonnage ran out before the legs did */
            }
            if (GameMechanic::planFreightJob(qPlayer, planeIds[qLeg.slot], j, start.getDate(), start.getHour())) {
                planned++;
            }
        }

        if (qPlayer.Frachten[j].TonsOpen > 0) {
            AT_Log("ClaudeBot::schedulePendingFreight(): %s -> %s still has %ld of %ld tons unplanned (due %ld).",
                   Cities[qPlayer.Frachten[j].VonCity].Name.c_str(), Cities[qPlayer.Frachten[j].NachCity].Name.c_str(),
                   static_cast<SLONG>(qPlayer.Frachten[j].TonsOpen), static_cast<SLONG>(qPlayer.Frachten[j].Tons),
                   static_cast<SLONG>(qPlayer.Frachten[j].BisDate));
        }
    }

    return planned;
}

SLONG ClaudeBot::getNextMood() {
    SLONG mood = mMood;
    mMood = mMoodNext;
    mMoodNext = -1;
    return mood;
}

TEAKFILE &operator<<(TEAKFILE &File, const ClaudeBot &bot) {
    SLONG savegameVersion = 107;
    File << savegameVersion;

    File << bot.mFirstRun;
    File << bot.mIsSickToday;

    File << bot.mOnThePhone;
    File << bot.mMood;
    File << bot.mMoodNext;

    /* The cached kerosene price is bot state even though it lives at file scope. */
    File << gKerosinPrice;
    File << gKerosinPriceDay;
    File << gKerosinAvgX100;

    /* Routes we rent, cached at the route box. Without these the bot would not advertise,
     * buy an aeroplane or schedule a route leg until its next route box visit. */
    File << static_cast<SLONG>(bot.mRoutes.size());
    for (const auto &qRoute : bot.mRoutes) {
        File << qRoute.id << qRoute.reverseId;
        File << qRoute.vonCity << qRoute.nachCity;
        File << qRoute.distance;
        File << qRoute.ticketPrice << qRoute.ticketPriceFC;
        File << qRoute.bedarf << qRoute.valuePerHour << qRoute.anzPax;
        File << qRoute.pricesSet;
    }

    File << bot.mDay;
    File << bot.mJobsTakenToday;
    File << bot.mFreightTakenToday;
    File << bot.mVisitedPersonalToday;
    File << bot.mVisitedMechToday;
    File << bot.mVisitedRouteBoxToday;
    File << bot.mVisitedAdsToday;
    File << bot.mVisitedBossToday;
    File << bot.mVisitedBrokerToday;
    File << bot.mVisitedBankToday;
    File << bot.mVisitedStockToday;
    File << bot.mVisitedMuseumToday;
    File << bot.mWantUsedPlane;
    File << bot.mUpgradedToday;
    File << bot.mAgencyEmptyToday;
    File << bot.mAgencyVisitsToday;
    File << bot.mFreightVisitsToday;
    File << bot.mVisitedTanksToday;
    File << bot.mVisitedKerosinToday;
    File << bot.mVisitedFreightToday;
    File << bot.mFreightEmptyToday;
    File << bot.mNeedSchedule;
    File << bot.mFillerIdx;
    File << bot.mImageDecayPerDay;
    File << bot.mImageAfterAds;
    File << bot.mImageAdsDay;
    File << bot.mFuelUnitsPerDay;
    File << bot.mWrongRoomDay;
    File << bot.mWrongRoomCount;
    /* mPlanes is deliberately not serialised. It is a pure function of the flight plans,
     * which the game saves itself, and the loader marks it stale so it is rebuilt from them
     * before anything is allowed to read it - so no state is lost. */

    SLONG magicnumber = 0x42;
    File << magicnumber;

    return (File);
}

TEAKFILE &operator>>(TEAKFILE &File, ClaudeBot &bot) {
    SLONG savegameVersion = 0;
    File >> savegameVersion;

    File >> bot.mFirstRun;
    File >> bot.mIsSickToday;

    File >> bot.mOnThePhone;
    File >> bot.mMood;
    File >> bot.mMoodNext;

    File >> gKerosinPrice;
    File >> gKerosinPriceDay;
    File >> gKerosinAvgX100;

    SLONG numRoutes = 0;
    File >> numRoutes;
    bot.mRoutes.clear();
    bot.mRoutes.reserve(numRoutes);
    for (SLONG i = 0; i < numRoutes; i++) {
        ClaudeBot::RouteState qRoute;
        File >> qRoute.id >> qRoute.reverseId;
        File >> qRoute.vonCity >> qRoute.nachCity;
        File >> qRoute.distance;
        File >> qRoute.ticketPrice >> qRoute.ticketPriceFC;
        File >> qRoute.bedarf >> qRoute.valuePerHour >> qRoute.anzPax;
        File >> qRoute.pricesSet;
        bot.mRoutes.push_back(qRoute);
    }

    File >> bot.mDay;
    File >> bot.mJobsTakenToday;
    File >> bot.mFreightTakenToday;
    File >> bot.mVisitedPersonalToday;
    File >> bot.mVisitedMechToday;
    File >> bot.mVisitedRouteBoxToday;
    File >> bot.mVisitedAdsToday;
    File >> bot.mVisitedBossToday;
    File >> bot.mVisitedBrokerToday;
    File >> bot.mVisitedBankToday;
    File >> bot.mVisitedStockToday;
    File >> bot.mVisitedMuseumToday;
    File >> bot.mWantUsedPlane;
    File >> bot.mUpgradedToday;
    File >> bot.mAgencyEmptyToday;
    File >> bot.mAgencyVisitsToday;
    File >> bot.mFreightVisitsToday;
    File >> bot.mVisitedTanksToday;
    File >> bot.mVisitedKerosinToday;
    File >> bot.mVisitedFreightToday;
    File >> bot.mFreightEmptyToday;
    File >> bot.mNeedSchedule;
    File >> bot.mFillerIdx;
    File >> bot.mImageDecayPerDay;
    File >> bot.mImageAfterAds;
    File >> bot.mImageAdsDay;
    File >> bot.mFuelUnitsPerDay;
    File >> bot.mWrongRoomDay;
    File >> bot.mWrongRoomCount;

    bot.mPlanes.clear();
    bot.mPlaneStateStale = true;

    SLONG magicnumber = 0;
    File >> magicnumber;
    assert(magicnumber == 0x42);

    return (File);
}
