// Simulation golden tests of the stable-step set: the simulations of the thirteen rockets of
// tests/core/rocket/TestRockets.h, run with a time step of 0.01 s, against what OpenRocket's
// BasicEventSimulationEngine computes for the Java rockets at that step, in
// tests/data/goldens/testrocket-<name>/stable/ (tools/openrocket-goldens: GoldenDumper.java and
// SimulationDumper.java; the format is in that tool's README.md).
//
// The default-step set of the same simulations (0.05 s, as a fresh installation runs them) is
// compared by simulation_golden_tests.cpp, whose header comment describes the harness's set-up
// and run, the jitter removal, what is compared of a simulation and why most flights of that set
// are not reproducible beyond their first tenths of a second. Both files work with
// GoldenSimulations.h (the set-up, the run, the golden files, compareSimulation() and the
// scaffolding of the mutation tests); the rules and the tolerances of the stable-step set are in
// this file.
//
// THE STABLE-STEP SET (SimulationStableGolden, one test per rocket). The harness runs every
// simulation once more with the time step set to 0.01 s (SimulationDumper.useStableTimeStep(),
// in a pass of its own that repeats the default pass with the same component ids) and writes
// it to testrocket-<name>/stable/ in the same format; "harness" records the stable time step
// and the one the simulation had. At that step the pitch oscillation is integrated within the
// stability limit of the Runge-Kutta steppers, and the flights can be compared to their last
// row: 50 simulations, 53 branches, 437 events, 35323 rows, 2471514 values. The run is the
// harness's again (runSimulation() with the stable time step, set last).
//
// Compared exactly, as in the default-step set (it is compareSimulation() with nothing taken
// as reproducible): the options (the time step among them), the "harness" block, what
// identifies the simulation, the status, the branches with their names, sources, files,
// columns and excluded columns, the events in order with their types, sources and data
// (simultaneous IGNITION events as a set, as there), the warnings. Compared within
// tolerances: the number of rows of every branch and the number of jitter replacements (both
// exactly), the ten summary values, the optimum altitude, the time to it and the optimum
// delay, the separation time, the time of every event, the parameter of every warning (with
// the texts that print it), the minimum and the maximum of every column, and the time series
// row by row.
//
// WHAT IS NOT REPRODUCIBLE AT THE STABLE STEP EITHER. OpenRocket against itself, nine dumps
// of the harness that differ in nothing but the ids of the components (the committed set and
// eight more, UUID_SALT of generate.sh with salt-b, salt-c, salt-d, salt-e, review-r1,
// review-r2, verify-v1 and verify-v2; 36 pairs): the structure is the same in all of them but
// for the order of simultaneous IGNITION events (as in the other set: 10 of the 50 simulations
// record such a pair in either order), and the summary values agree to 5.7e-9 (maximum altitude,
// velocity, acceleration and Mach number) and 3.2e-8 s (time to apogee); but 7 of the 53
// branches have other numbers of rows in some dump, the flight times differ by up to
// 1.4e-4 s, and the time series differ by up to 8.8e-4 of a column's scale in columns that
// hold a signal and by more than the scale in one that does not. QtRocket against the golden
// files differs in the same places by the same amounts (on glibc and under 46 patterns of the
// libm shim): it behaves as one more run of OpenRocket. The differences are not rounding
// noise that a step size amplifies; they have four causes in OpenRocket's algorithm, which
// the golden rows show, and the comparison has a rule for each:
//
// - H, hunting. AbstractSimulationStepper.calculateFlightConditions() sets the pitch and the
//   yaw rate of the flight conditions to zero while the lateral airspeed is below 1 mm/s
//   (and the direction of the lateral airspeed to zero below 0.1 mm/s). A weathercocked
//   rocket at 100 m/s reaches that after about one second of flight and stays there for two
//   (an angle of attack of about 1e-6 rad): without the pitch damping, and with a direction
//   that switches, its attitude wanders within the threshold, differently in every run. The
//   rows are recognisable: a Runge-Kutta row in free flight whose pitch rate and yaw rate are
//   both exactly zero (5552 rows in 30 of the 34 branches that leave the launch rod: all but
//   those of the two flights that abort under thrust, the Falcon 9 Heavy and the multi-stage
//   rocket with its two dropped stages). They are the rows whose lateral airspeed, which a
//   row holds as its airspeed times the sine of its angle of attack, is below the threshold,
//   and no other row is (misjudgedRows(): none). What the wander does depends on the column:
//   * Six columns are noise in a hunting row (kHuntingNoiseColumns: the angle of attack,
//     which is the lateral airspeed itself, the normal force and pitch moment coefficients
//     that answer it, the lateral accelerations they cause). Two runs of OpenRocket differ
//     there by up to 8.8e-4 of the scales of these columns, as much as their values. Not
//     compared in a hunting row: 33312 values.
//   * Twelve attitude columns keep their signal (kHuntingColumns: the orientation, the
//     normal force slope, the centre of pressure, the stability margin and derivatives, the
//     vertical and the total acceleration, the lateral velocity: 8 % to all of their scales
//     in these rows, where the peak deceleration after the burnout of 24 flights is) and
//     differ by 2.6e-7 to 1.8e-5 of them, more than elsewhere. Compared at hunting
//     tolerances of their own, 1e-4 to 1e-2, in the hunting and the band rows: 69252 values.
//   * Every other column is compared in a hunting row as in any other; so are the pitch
//     rate and the pitch damping moment, which have to be zero there: a run that does not
//     hunt where the golden run does, does not match.
//   The decision is a threshold, though, and where the lateral airspeed passes it slowly a
//   platform can decide a row the other way: at the end of the hunting it rises through the
//   1 mm/s by 0.01 to 0.02 mm/s per row, and two runs differ by up to 1.4e-6 m/s there. The
//   rows within kHuntingBand = 0.15 mm/s of the threshold are band rows (517, of which 298
//   hunt): what the decision switches in the row itself (kSwitchedColumns: the pitch and yaw
//   rate and the pitch damping moment) is not compared in them, and the attitude columns
//   are treated as in a hunting row: 2359 values more. A run that hunts a row longer or
//   shorter than the golden run then still matches (tried with a library whose threshold is
//   0.5 % off, which every whole-flight test but that of the flight into the multi-level
//   wind accepts); the threshold itself is pinned to the last bit by
//   AbstractSimulationStepper.TheThresholdsOfTheLateralAirspeedAreExact, which that library
//   fails.
// - N, noise-dominated columns. The flights are planar but for the Coriolis acceleration (the
//   wind blows along one axis), so the out-of-plane columns (the yaw rate, the lateral
//   acceleration and position across the wind and the direction of the lateral position)
//   hold a signal of 2e-6 to 3e-4 of their counterparts in the plane, which the hunting
//   scatters. Off the rod two runs of OpenRocket differ in them by 1.3 and 0.58 times the
//   column's own scale (the lateral acceleration in world and in body coordinates), by
//   1.2e-3 (the yaw rate and the position) and by 1.4e-4 (the direction); QtRocket differs
//   from the golden files by 2.3, 3.1, 1.8e-3 and 1.3e-4. A tolerance of a hundred times
//   that would compare nothing. Such a column (kOutOfPlaneColumns: its largest magnitude is
//   below kNoiseRatio = 1e-2 of its counterpart's, and it is not a column of zeros) is
//   compared on the launch rod only: 167 columns (five in each of the 33 planar branches
//   that leave the rod, and the roll rate of two branches), 153992 values. In the flight
//   into the multi-level wind, which turns with the altitude, the five columns hold a signal
//   (0.2 to 2 times their counterparts) and are compared. The roll rate of that flight and
//   of the second stage of the multi-stage rocket is another case: 1e-7 of the pitch rate
//   (1.5e-7 rad/s), it is reproducible to 7.8e-7 of its own scale between two runs of
//   OpenRocket and to 1.1e-6 here, but the kick below moves it by 3.8e-3 of it, four times a
//   tolerance from those measurements: what moves it is any change of the rotation of
//   1e-10 rad/s, so it is left a noise column. (The rule is noiseColumns() of
//   GoldenSimulations.h, with its table and its ratio: the comparison of the default-step set
//   applies it too, see "The out-of-plane noise columns" in simulation_golden_tests.cpp.)
// - E, the step to the apogee of an Euler stepper. A rocket whose recovery device is deployed
//   on the way up passes its apogee under BasicLandingStepper, and AbstractEulerStepper.step()
//   then makes one step that ends on the apogee (t = |v / a|), which leaves a rounding error
//   of the vertical velocity: exactly zero in nine runs of ten, else of either sign and at
//   most 1.8e-16 of the velocity before (some 1e-17 m/s). When it is positive the next step
//   is "an apogee" again, of |v / a| = 1e-18 s, raised to the minimum of 1 ms; when it is not,
//   the next step is the regular one of 0.1 s. So the last bit decides: of the 19 branches
//   with such an apogee, 5 come out either way in the nine dumps of OpenRocket (50 of the
//   684 pairs of runs differ; the [A8-0] flight of the Estes Alpha III has 668 rows in the
//   committed file and 667 in the eight other dumps and here), and from that row on the two
//   runs are on other time grids: the APOGEE event of the run with the extra step is 1 ms
//   later, and the ground hit and the flight time differ by up to 1.4e-4 s. The comparison
//   finds the apogee row in the golden rows (eulerApogeeRow()) and looks at the step both
//   runs take from it. When it is of the same kind, the branch is compared to its last row.
//   When it is not, the 1 ms step has been taken by one run only, and the rows from the
//   apogee row on, the events from there on (the APOGEE among them), the number of rows and
//   the flight time are not compared; but only when the run's own rows show that the last bit
//   decided (apogeeStepProblem()): what its step to the apogee left is a rounding error, at
//   most kApogeeResidual = 1e-13 of the velocity before, and its short step follows a
//   positive one. Any other difference there is a mismatch (a stepper whose step to the
//   apogee is 1e-12 short of it takes the extra step in 18 flights, and 12 tests report it).
//   The golden rows show the same in all 19 branches (lastBitApogees). The ground hit
//   velocity is compared in either case: it does not depend on the time grid (two runs of
//   OpenRocket differ in it by 3.9e-11 of itself). On glibc one branch steps otherwise (239
//   rows of the [A8-0] flight); under the shim patterns one to four (up to 1243 rows).
// - T, a stage that tumbles before its apogee. The booster of the Beta and the second stage
//   of the multi-stage rocket are unstable once they are dropped and tumble under the
//   Runge-Kutta stepper, with steps at the minimum, until the tumble stepper takes over. That
//   amplifies what the hunting before the separation left: the second stage has 563, 564,
//   567, 568, 571, 572 or 575 rows in the nine dumps of OpenRocket (566 here on glibc, 561 to
//   577 under the shim patterns) and the simulation as many different numbers of jitter
//   replacements; the booster has 631 rows, and 632 in one dump and under six patterns. Such
//   a branch (its TUMBLE event precedes its APOGEE) is compared up to its (last) stage
//   separation, whose rows are those of the flight before: 542 rows of the 2 branches are
//   not, nor their events after the separation, their numbers of rows, optimum altitudes and
//   delays, and the number of jitter replacements of the two simulations.
//
// What the rules exclude is counted, and the sums are checked: compared plus excluded is what
// the files hold (in the test of each rocket). SimulationStableGoldenCoverage pins what the
// rules decide from the golden files alone (it runs no simulation): the numbers above, and
// the floor, which is what is compared on every platform, whatever the apogee steps do: 26862
// of the 35323 rows and 1732140 of the 2471514 values. On glibc 34542 rows, 2231340 values
// (90 %) and 6412 of the 6920 numbers outside the time series are compared; every simulation
// is compared from its first row to its last but the three branches named above.
//
// Tolerances. Every one is at least 100 times (kToleranceMargin) the largest difference
// measured for what it bounds: OpenRocket against itself (the 72 ordered pairs of the nine
// dumps, the plan taken from the first of a pair) and QtRocket against the golden files (47
// runs: glibc, and the libm shim always up, always down, and up or down at random on one
// call in 1, 2, ... 41, 101, 1009 and 10007), over what the rules compare.
// SimulationStableGoldenRules holds the arithmetic.
// - A value of the time series on the launch rod (and every value of a simulation that never
//   leaves it): kValueRelative, 1e-9 of the scale of its column, as in the default-step set.
//   Measured: 2.7e-13.
// - A value off the rod, and a minimum or maximum: the tolerance of its column, 100 times its
//   largest measured difference rounded up to a power of ten (kMeasuredColumns, 61 columns
//   with both measurements; the other columns are constants and match exactly). In short:
//     1e-9 to 1e-7  the atmosphere, the gravity, the mass and the inertias, the position on
//                   the globe, the wind, the base and pressure drag coefficients;
//     1e-6          the time, the altitude, the thrust, the drag coefficient, the yaw rate
//                   where it is compared;
//     1e-5          the velocities, the Mach and Reynolds numbers, the drag, the axial
//                   acceleration, the stability margin and the centre of pressure, the
//                   accelerations across the wind where they are compared;
//     1e-4          the lateral position, the angle of attack and the orientation, the normal
//                   force slope, the friction drag coefficient (largest: 6.8e-7);
//     1e-3          the pitch rate, the pitch moment and normal force coefficients, the lateral
//                   velocity and accelerations, the time step (largest: 8.9e-6);
//     1e-2          the lateral acceleration in body coordinates (1.2e-5: under a parachute
//                   that opens at 80 m/s the axial deceleration is 5800 m/s^2, of which the
//                   attitude the rocket kept turns 1e-7 into this column, whose scale is
//                   10 m/s^2).
//   An extreme that a run attains only in rows in which the rules exclude its column is one
//   of the excluded values (extremeRule()).
// - In a hunting or band row, the twelve columns of kHuntingColumns: 1e-4 (the centre of
//   pressure and the stability margin, 4.3e-7), 1e-3 (the orientation, the normal force
//   slope, the stability derivatives, the vertical acceleration; largest: 8.2e-6) and 1e-2
//   (the total acceleration and the lateral velocity, 1.8e-5). A minimum or maximum that a
//   run attains in such a row is compared at that tolerance too.
// - In a row of an Euler stepper, the thirteen columns of kEulerColumns: the drag
//   coefficients, the mass, the centre of gravity, the inertias and the thrust are constants
//   of the recovery device or of the tumbling rocket there and agree exactly or to 7e-16:
//   kValueRelative; the vertical velocity, 9.9e-9 under a parachute where the hunting rows
//   make its column 4.3e-8: 1e-6.
// - A summary value, the optimum altitude of a branch, the parameter of a warning:
//   kStableSummaryRelative, 1e-6 of its scale. Measured: 9.5e-9 (maximum velocity), 5.0e-9
//   (maximum altitude), 2.1e-9 (maximum acceleration), 9.0e-10 (maximum Mach number), 5.1e-9
//   (optimum altitude), 3.4e-9 (parameter). The ground hit velocity: 1e-6 of itself
//   (4.6e-11). The launch rod velocity: 1e-9 (3.5e-17). The deployment velocity:
//   kDeploymentVelocityRelative, 1e-5 of the largest velocity (1.4e-8).
// - A time: kStableTimeAbsolute, 1e-4 s. Measured: 8.0e-7 s (GROUND_HIT, SIMULATION_END and
//   the flight time), 2.1e-7 s (TUMBLE), 3.3e-8 s (APOGEE and the time to apogee), 5.9e-10 s
//   (optimum delay); the events the motors and the launch rod time (LAUNCH, IGNITION, LIFTOFF,
//   LAUNCHROD, BURNOUT, EJECTION_CHARGE, STAGE_SEPARATION, RECOVERY_DEVICE_DEPLOYMENT) differ
//   by nothing, SIM_WARN and SIM_ABORT by 5.2e-15 s.
// - The time of an event that follows a late handling: kLateHandlingAbsolute, 1e-3 s, which is
//   the plan's bound. A queued event gets its time from the simulation time at which the
//   event that queues it is handled, and an event is handled at the end of the first step
//   that reaches it; the engine lets that step be no shorter than 1 ms
//   (BasicEventSimulationEngine.simulateLoop()), and a Runge-Kutta stepper no shorter than a
//   twentieth of the time step, so a step can overshoot an event that the step before left
//   less than that away. The golden rows show where that happened: an event without a row at
//   its time (firstLateHandling()). Three branches have one: in the flight into the
//   multi-level wind the BURNOUT at 2.1 s is handled at 2.100345 s, which puts the
//   EJECTION_CHARGE at 7.100345 s; in the multi-stage rocket the BURNOUT at 2.11 s is handled
//   47 microseconds late, in a run of minimum steps. How late depends on where the rows
//   before fall, so the events after such a handling could move by up to the 1 ms on a
//   platform that steps otherwise, and are compared at that. They do not move in any
//   measurement: by 2.7e-10 s at most, and by 1.5e-8 s under the kick below.
//
// What the comparison notices. The tolerances are those of a trajectory that two runs of
// OpenRocket do not reproduce better, so a constant of a stepper that is off by a part in
// 1e7 is noticed where a column holds the constant itself (the drag coefficient of a
// tumbling rocket or under a recovery device, in the Euler rows) and in the flight into the
// multi-level wind (its out-of-plane columns are compared at 1e-9), and otherwise only from
// a part in 1e6 to 1e5 on. Tried with libraries changed so: the drag coefficient of the
// tumble stepper or of the landing stepper times 1 + 1e-7 fails 4 and 15 of the tests of
// this file; the drag force of the Euler steppers times 1 + 1e-7 or 1 + 1e-6 only that of
// the rocket with the flight into the multi-level wind, times 1 + 1e-5 fifteen. Such
// constants are pinned tighter by the scenario tests of the steppers
// (tests/core/simulation/, their expectations pasted from Java probes), which every one of
// these libraries fails.
//
// Sensitivity. The libm shim moves a result by one ulp, and that did not predict MSVC for the
// default-step set, so the stable-step set was also measured with a perturbation a million
// times larger (SimulationStableGoldenMeasurement.DISABLED_PrintsTheSensitivityToAKick, a
// relative 1e-10 kick to the velocity and to the rotation velocity after every step; the
// numbers here are the largest of that run and of the three runs with other seeds that the
// rules were worked out with). The trajectory follows the kick in proportion (the altitude
// moves by 4.9e-7 of its scale, a value on the launch rod by 2e-9), and nothing that is
// compared moves out of proportion: no event sequence changes, no row count changes but where
// rule E or T says it can (the apogee step changes in 4 of 57 runs), the events the motors and
// the rod time do not move at all, the others by 3.1e-8 s (APOGEE), 8.8e-8 s (TUMBLE) and
// 3.7e-7 s (GROUND_HIT), the summary values by 1.2e-7 of their scales at most, the lateral
// airspeed of a band row by 6.5e-7 m/s, no row hunts in one of the two runs only, and the
// attitude columns move no further than between two runs of OpenRocket (the hunting is as
// large as it gets without a kick). One column that two runs reproduce does move out of
// proportion, the roll rate, and is left to rule N for that.
// So what is compared tightly is insensitive for a reason: it is a function of the trajectory,
// which is stable at this step, or of the motors' times; and what is sensitive (the attitude
// while hunting, the apogee step, a tumbling stage) is excluded by a rule, not by a tolerance.
// The places where the engine decides by a threshold were looked at one by one in the golden
// rows (tier8c-implementer/thresholds.py, apogee_drop.py, snap_margin.py and
// tier8c-fix/scripts/hunting_decision.py, tumble_margin.py), since a decision that falls
// otherwise moves a row or an event by a whole step:
// - LIFTOFF (altitude over 2 cm) and LAUNCHROD (distance over the rod length): the row that
//   crosses the threshold and the row before it are at least 9.9e-5 m and 6.6e-5 m from it,
//   where two runs differ by 1e-15 m at most.
// - APOGEE under a Runge-Kutta stepper (altitude 1 cm below its maximum, 11 branches): at
//   least 3.5e-5 m from the threshold, where the drop differs by 7.5e-10 m between two runs
//   (6.0e-9 m under the kick).
// - A step that ends on an event (201 events after a Runge-Kutta step): a step is stretched
//   to an event that is less than 0.5 ms beyond its end; the longest such step is 0.32 ms
//   short of the 10.5 ms at which it would not have been. The three late handlings: the row
//   before is 0.66 ms and 0.95 ms short of the event, 0.16 ms and more beyond the 0.5 ms.
//   The times of the Runge-Kutta rows differ by about 1e-7 s at most between two runs.
// - TUMBLE (the angle of attack, filtered over two pitch periods, over 60 degrees; the 6
//   flights that tumble after their apogee): the filtered angle of the step that crosses the
//   threshold and of the step before are at least 3.2e-4 rad from it, where it differs by
//   2.6e-7 rad between two runs; the dynamic pressure there is 27 Pa and more, its floor 1 Pa.
// - The hunting (lateral airspeed below 1 mm/s, rule H) is the one decision without such a
//   margin everywhere, and it is decided in three kinds of rows. While the rocket still
//   swings into the wind its lateral airspeed passes the threshold fast, in rows that two
//   runs reproduce to 1e-8 m/s. In the middle of the hunting the lateral airspeed wanders
//   at 0.1 to 0.3 mm/s, differently in every run (two runs differ by up to 0.18 mm/s there,
//   which is at most 0.18 of the distance of the golden row from the threshold between two
//   runs of OpenRocket and 0.23 between QtRocket and the golden files); what bounds it is
//   its cause, the direction that is set to zero below 0.1 mm/s, not a margin. Then it rises
//   to the threshold, and the difference between two runs falls from 4e-5 m/s (at 0.3 mm/s)
//   to 2e-6 m/s (at 0.85 mm/s), 17 to 97 times less than the distance from the threshold in
//   the worst of the 47 runs; from there on the rows are band rows, where the switched
//   columns are not compared. No row is decided otherwise in any measurement (the nine
//   dumps, the 47 runs, the kicked run). The flight into the multi-level wind relies on its
//   margin (4100 at the end of its hunting): a row decided otherwise there moves the
//   direction of its lateral position, which is compared at 1e-9, in the rows that follow.
//
// Not vacuous: SimulationStableGoldenMutation changes one golden value at a time, late in a
// flight (a value and a time of a row under the parachute, a drag coefficient and a mass
// there, the pitch angle and the pitch rate of a hunting row, the last row, the times of the
// apogee and of the ground hit, a summary value, a row count, a maximum), removes an event
// and swaps two, and expects the one line that reports it; changes within the tolerances are
// not reported; and a value that a rule excludes is counted as excluded, not compared.
// SimulationStableGoldenMeasurement, disabled, prints the table of the differences.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenGeometry.h"
#include "goldens/GoldenMismatches.h"
#include "goldens/GoldenSimulations.h"
#include "goldens/GoldenWarnings.h"
#include "rocket/TestRockets.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using nlohmann::json;
using QtRocket::Coordinate;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightEvent;
using QtRocket::Rocket;
using QtRocket::SimulationStatus;
using QtRocket::Warning;
using QtRocket::Test::baseName;
using QtRocket::Test::columnKey;
using QtRocket::Test::columnScale;
using QtRocket::Test::compareSimulation;
using QtRocket::Test::csvTypes;
using QtRocket::Test::differs;
using QtRocket::Test::goldenCounts;
using QtRocket::Test::GoldenFiles;
using QtRocket::Test::goldenFormOf;
using QtRocket::Test::GoldenInput;
using QtRocket::Test::goldenRodClearance;
using QtRocket::Test::GoldenSimulation;
using QtRocket::Test::GoldenTable;
using QtRocket::Test::goldenValue;
using QtRocket::Test::inputOf;
using QtRocket::Test::InstallationDefaults;
using QtRocket::Test::kStableHarnessKeys;
using QtRocket::Test::kValueRelative;
using QtRocket::Test::loadGoldenFiles;
using QtRocket::Test::makeReproducible;
using QtRocket::Test::makerTestName;
using QtRocket::Test::manifest;
using QtRocket::Test::Mutation;
using QtRocket::Test::noiseColumns;
using QtRocket::Test::orderedEvents;
using QtRocket::Test::orderedGoldenEvents;
using QtRocket::Test::parameterOf;
using QtRocket::Test::pathOrNull;
using QtRocket::Test::plannedSimulations;
using QtRocket::Test::referenceOf;
using QtRocket::Test::rowsUpToTheClearance;
using QtRocket::Test::runSimulation;
using QtRocket::Test::scale;
using QtRocket::Test::Sensitivity;
using QtRocket::Test::sequenceOf;
using QtRocket::Test::shift;
using QtRocket::Test::shiftSeries;
using QtRocket::Test::SimulationComparison;
using QtRocket::Test::SimulationCounts;
using QtRocket::Test::SimulationRun;
using QtRocket::Test::sortedNames;
using QtRocket::Test::Subject;
using QtRocket::Test::SubjectCase;
using QtRocket::Test::subjectTestName;
using QtRocket::Test::TestRocketMaker;
using QtRocket::Test::testRocketMakers;
using QtRocket::Test::useStableTimeStep;

/// The comparison collector of the golden tests.
using Mismatches = QtRocket::Test::GoldenMismatches;

// ====================================================================== the stable-step set

// The comparison of the stable-step set: see "THE STABLE-STEP SET" at the top of the file.

/// SimulationDumper.STABLE_TIME_STEP: the time step of the stable-step set, in s.
constexpr double kStableTimeStep = 0.01;

/// A tolerance of the stable-step comparison is at least this many times the largest difference
/// measured for what it bounds (OpenRocket against itself in nine dumps with other component
/// ids, and QtRocket against the golden files on glibc and under 46 patterns of the libm shim).
constexpr double kToleranceMargin = 100.0;

/// A time of the stable-step set (that of an event, the time to apogee, the flight time, the
/// optimum delay): within this many seconds. The largest difference measured is 8.0e-7 s (a
/// ground hit, which an Euler stepper times from the altitude of the row before it).
constexpr double kStableTimeAbsolute = 1e-4;

/// The time of an event that follows a late handling in its branch (StablePlan::lateFrom):
/// within the 1 ms that BasicEventSimulationEngine.simulateLoop() allows a step to overshoot
/// an event by, which is also the plan's bound on event times. Measured: 2.7e-10 s.
constexpr double kLateHandlingAbsolute = 1e-3;

/// A summary value of the stable-step set, the optimum altitude of a branch and the parameter
/// of a warning: within this fraction of its scale. Measured: 9.5e-9 (a maximum velocity).
constexpr double kStableSummaryRelative = 1e-6;

/// The velocity at the deployment of the recovery device: within this fraction of the largest
/// velocity of the flight. Measured: 1.4e-8 (the velocity is read off a steep part of the
/// trajectory, between two rows around the deployment).
constexpr double kDeploymentVelocityRelative = 1e-5;

/// A vertical velocity below this, in m/s, in a row of an Euler stepper: the row on which the
/// stepper landed its step to the apogee (AbstractEulerStepper.step(): t = |v / a|, which
/// leaves a rounding error of the velocity: nothing in nine runs of ten, some 1e-17 m/s of
/// either sign in the others).
constexpr double kApogeeVelocity = 1e-9;

/// Rule E: what the step to the apogee leaves of the vertical velocity is a rounding error of
/// the velocity it started from, at most this fraction of it. Measured: 1.8e-16 (the
/// largest in the apogee rows of nine dumps of OpenRocket and of 47 runs of QtRocket).
constexpr double kApogeeResidual = 1e-13;

/// A step from the apogee row that is no longer than this, in s, is the extra step of
/// MIN_TIME_STEP (1 ms) that the stepper takes when the velocity left by its step to the apogee
/// is positive; the regular step there is about 0.1 s.
constexpr double kShortStep = 1.5e-3;

/// A golden row within this many seconds of the time of an event handled the event on time.
constexpr double kOnTime = 1e-9;

/// AbstractSimulationStepper.calculateFlightConditions() sets the pitch and the yaw rate of the
/// flight conditions to zero while the lateral airspeed is below this, in m/s (rule H).
constexpr double kHuntingThreshold = 1e-3;

/// A Runge-Kutta row in free flight whose lateral airspeed is within this, in m/s, of
/// kHuntingThreshold is a band row: a run may decide it the other way than the golden run did,
/// so what the decision switches is not compared in it (rule H). Measured: the lateral airspeed
/// of a band row differs by 1.1e-6 m/s at most between two runs of OpenRocket and by
/// 1.4e-6 m/s between QtRocket and the golden files.
constexpr double kHuntingBand = 1.5e-4;

/// A column of the time series of the stable-step set: the largest difference measured for the
/// values that are compared at a tolerance, as a fraction of the column's scale (OpenRocket
/// against itself, every ordered pair of nine dumps with other component ids; QtRocket against
/// the golden files on glibc and under 46 patterns of the one-ulp libm shim), and the
/// tolerance: kToleranceMargin times the larger, rounded up to a power of ten.
struct MeasuredColumn
{
    std::string_view key;
    double           openRocket;
    double           qtRocket;
    double           tolerance;
};

// The three tables are what tier8c-fix/scripts/cpp_tables.py prints from the measurements
// (rules2.py for OpenRocket, SimulationStableGoldenMeasurement for QtRocket), rounded up to two
// digits.
//
// kMeasuredColumns: a value off the launch rod, and a minimum or maximum. The columns that are
// not listed match exactly in every measurement (the thrust correction, the roll, yaw and side
// force coefficients, the reference length and area).
// clang-format off
constexpr std::array<MeasuredColumn, 61> kMeasuredColumns{{
    {.key = "acceleration_bodyx", .openRocket = 1.4e-06, .qtRocket = 1.2e-05, .tolerance = 1e-02},
    {.key = "pitch_damping_moment_coeff", .openRocket = 8.7e-06, .qtRocket = 8.9e-06, .tolerance = 1e-03},
    {.key = "damping_ratio", .openRocket = 4.3e-06, .qtRocket = 4.4e-06, .tolerance = 1e-03},
    {.key = "normal_force_coeff", .openRocket = 3.8e-06, .qtRocket = 3.9e-06, .tolerance = 1e-03},
    {.key = "velocity_xy", .openRocket = 3.3e-06, .qtRocket = 3.4e-06, .tolerance = 1e-03},
    {.key = "pitch_moment_coeff", .openRocket = 2.8e-06, .qtRocket = 2.8e-06, .tolerance = 1e-03},
    {.key = "pitch_rate", .openRocket = 2.7e-06, .qtRocket = 2.7e-06, .tolerance = 1e-03},
    {.key = "time_step", .openRocket = 2.0e-06, .qtRocket = 1.2e-06, .tolerance = 1e-03},
    {.key = "acceleration_x", .openRocket = 1.2e-06, .qtRocket = 1.2e-06, .tolerance = 1e-03},
    {.key = "acceleration_xy", .openRocket = 1.2e-06, .qtRocket = 1.2e-06, .tolerance = 1e-03},
    {.key = "friction_drag_coeff", .openRocket = 6.7e-07, .qtRocket = 6.8e-07, .tolerance = 1e-04},
    {.key = "orientation_theta", .openRocket = 5.3e-07, .qtRocket = 5.4e-07, .tolerance = 1e-04},
    {.key = "aoa", .openRocket = 4.8e-07, .qtRocket = 4.9e-07, .tolerance = 1e-04},
    {.key = "orientation_phi", .openRocket = 2.3e-07, .qtRocket = 3.3e-07, .tolerance = 1e-04},
    {.key = "cna", .openRocket = 2.2e-07, .qtRocket = 2.2e-07, .tolerance = 1e-04},
    {.key = "position_x", .openRocket = 1.8e-07, .qtRocket = 2.0e-07, .tolerance = 1e-04},
    {.key = "position_xy", .openRocket = 1.8e-07, .qtRocket = 2.0e-07, .tolerance = 1e-04},
    {.key = "coriolis_acceleration", .openRocket = 6.8e-08, .qtRocket = 1.2e-07, .tolerance = 1e-04},
    {.key = "acceleration_y", .openRocket = 5.6e-08, .qtRocket = 9.1e-08, .tolerance = 1e-05},
    {.key = "acceleration_bodyz", .openRocket = 4.8e-08, .qtRocket = 8.4e-08, .tolerance = 1e-05},
    {.key = "stability", .openRocket = 5.8e-08, .qtRocket = 5.6e-08, .tolerance = 1e-05},
    {.key = "cp_location", .openRocket = 5.2e-08, .qtRocket = 5.3e-08, .tolerance = 1e-05},
    {.key = "axial_drag_coeff", .openRocket = 5.1e-08, .qtRocket = 5.2e-08, .tolerance = 1e-05},
    {.key = "velocity_z", .openRocket = 3.3e-08, .qtRocket = 4.3e-08, .tolerance = 1e-05},
    {.key = "drag_force", .openRocket = 2.2e-08, .qtRocket = 4.1e-08, .tolerance = 1e-05},
    {.key = "acceleration_bodyy", .openRocket = 2.1e-08, .qtRocket = 3.4e-08, .tolerance = 1e-05},
    {.key = "acceleration_total", .openRocket = 3.0e-08, .qtRocket = 3.2e-08, .tolerance = 1e-05},
    {.key = "natural_frequency", .openRocket = 2.6e-08, .qtRocket = 2.6e-08, .tolerance = 1e-05},
    {.key = "velocity_total", .openRocket = 2.3e-08, .qtRocket = 2.3e-08, .tolerance = 1e-05},
    {.key = "mach_number", .openRocket = 2.3e-08, .qtRocket = 2.3e-08, .tolerance = 1e-05},
    {.key = "reynolds_number", .openRocket = 2.2e-08, .qtRocket = 2.3e-08, .tolerance = 1e-05},
    {.key = "damping_moment_coeff_aerodynamic", .openRocket = 2.1e-08, .qtRocket = 2.2e-08, .tolerance = 1e-05},
    {.key = "damping_moment_coeff", .openRocket = 1.8e-08, .qtRocket = 2.0e-08, .tolerance = 1e-05},
    {.key = "acceleration_z", .openRocket = 1.1e-08, .qtRocket = 1.8e-08, .tolerance = 1e-05},
    {.key = "damping_moment_coeff_propulsive", .openRocket = 9.3e-09, .qtRocket = 8.1e-09, .tolerance = 1e-06},
    {.key = "thrust_weight_ratio", .openRocket = 7.3e-09, .qtRocket = 8.8e-09, .tolerance = 1e-06},
    {.key = "corrective_moment_coeff", .openRocket = 5.6e-09, .qtRocket = 8.4e-09, .tolerance = 1e-06},
    {.key = "thrust_force", .openRocket = 6.3e-09, .qtRocket = 7.5e-09, .tolerance = 1e-06},
    {.key = "drag_coeff", .openRocket = 7.0e-09, .qtRocket = 7.2e-09, .tolerance = 1e-06},
    {.key = "yaw_rate", .openRocket = 4.4e-09, .qtRocket = 5.6e-09, .tolerance = 1e-06},
    {.key = "altitude", .openRocket = 5.0e-09, .qtRocket = 5.1e-09, .tolerance = 1e-06},
    {.key = "altitude_above_sea", .openRocket = 5.0e-09, .qtRocket = 5.1e-09, .tolerance = 1e-06},
    {.key = "time", .openRocket = 4.8e-09, .qtRocket = 3.7e-09, .tolerance = 1e-06},
    {.key = "base_drag_coeff", .openRocket = 6.0e-10, .qtRocket = 7.2e-10, .tolerance = 1e-07},
    {.key = "pressure_drag_coeff", .openRocket = 5.3e-10, .qtRocket = 5.3e-10, .tolerance = 1e-07},
    {.key = "air_pressure", .openRocket = 3.1e-10, .qtRocket = 1.9e-10, .tolerance = 1e-07},
    {.key = "motor_mass", .openRocket = 2.5e-10, .qtRocket = 3.0e-10, .tolerance = 1e-07},
    {.key = "air_density", .openRocket = 2.6e-10, .qtRocket = 1.5e-10, .tolerance = 1e-07},
    {.key = "mass", .openRocket = 1.3e-10, .qtRocket = 1.6e-10, .tolerance = 1e-07},
    {.key = "cg_location", .openRocket = 7.2e-11, .qtRocket = 4.7e-11, .tolerance = 1e-08},
    {.key = "longitudinal_inertia", .openRocket = 7.0e-11, .qtRocket = 5.6e-11, .tolerance = 1e-08},
    {.key = "air_temperature", .openRocket = 6.0e-11, .qtRocket = 3.6e-11, .tolerance = 1e-08},
    {.key = "speed_of_sound", .openRocket = 3.1e-11, .qtRocket = 1.9e-11, .tolerance = 1e-08},
    {.key = "rotational_inertia", .openRocket = 2.0e-11, .qtRocket = 1.7e-11, .tolerance = 1e-08},
    {.key = "position_direction", .openRocket = 3.8e-12, .qtRocket = 5.0e-12, .tolerance = 1e-09},
    {.key = "longitude", .openRocket = 4.4e-12, .qtRocket = 3.4e-12, .tolerance = 1e-09},
    {.key = "wind_velocity", .openRocket = 2.4e-12, .qtRocket = 3.6e-12, .tolerance = 1e-09},
    {.key = "latitude", .openRocket = 1.7e-12, .qtRocket = 2.4e-12, .tolerance = 1e-09},
    {.key = "wind_direction", .openRocket = 1.6e-12, .qtRocket = 2.3e-12, .tolerance = 1e-09},
    {.key = "position_y", .openRocket = 6.1e-13, .qtRocket = 1.2e-12, .tolerance = 1e-09},
    {.key = "gravity", .openRocket = 8.4e-13, .qtRocket = 5.0e-13, .tolerance = 1e-09},
}};

// kHuntingColumns: the attitude columns that keep their signal while the rocket hunts, in the
// hunting rows and the band rows (rule H), where they differ by more than elsewhere.
constexpr std::array<MeasuredColumn, 12> kHuntingColumns{{
    {.key = "velocity_xy", .openRocket = 1.8e-05, .qtRocket = 1.6e-05, .tolerance = 1e-02},
    {.key = "acceleration_total", .openRocket = 1.6e-05, .qtRocket = 1.7e-05, .tolerance = 1e-02},
    {.key = "orientation_phi", .openRocket = 5.5e-06, .qtRocket = 8.2e-06, .tolerance = 1e-03},
    {.key = "corrective_moment_coeff", .openRocket = 3.3e-06, .qtRocket = 5.1e-06, .tolerance = 1e-03},
    {.key = "acceleration_z", .openRocket = 4.6e-06, .qtRocket = 4.7e-06, .tolerance = 1e-03},
    {.key = "natural_frequency", .openRocket = 1.9e-06, .qtRocket = 2.6e-06, .tolerance = 1e-03},
    {.key = "damping_moment_coeff_aerodynamic", .openRocket = 2.0e-06, .qtRocket = 2.2e-06, .tolerance = 1e-03},
    {.key = "damping_moment_coeff", .openRocket = 1.8e-06, .qtRocket = 1.9e-06, .tolerance = 1e-03},
    {.key = "orientation_theta", .openRocket = 1.5e-06, .qtRocket = 1.6e-06, .tolerance = 1e-03},
    {.key = "cna", .openRocket = 9.1e-07, .qtRocket = 1.2e-06, .tolerance = 1e-03},
    {.key = "stability", .openRocket = 2.9e-07, .qtRocket = 4.3e-07, .tolerance = 1e-04},
    {.key = "cp_location", .openRocket = 2.6e-07, .qtRocket = 4.0e-07, .tolerance = 1e-04},
}};

// kEulerColumns: in the rows of an Euler stepper. The drag coefficients, the mass and what
// follows from them are constants of the recovery devices or of the tumbling rocket there and
// agree to 1e-11 of their scales or exactly in every measurement, where their columns need
// 1e-8 to 1e-4 for the powered flight and the coast; the vertical velocity is steadier under
// a parachute than in the hunting rows.
constexpr std::array<MeasuredColumn, 13> kEulerColumns{{
    {.key = "velocity_z", .openRocket = 5.6e-09, .qtRocket = 9.9e-09, .tolerance = 1e-06},
    {.key = "rotational_inertia", .openRocket = 0.0e+00, .qtRocket = 6.9e-16, .tolerance = 1e-09},
    {.key = "longitudinal_inertia", .openRocket = 1.2e-16, .qtRocket = 1.2e-16, .tolerance = 1e-09},
    {.key = "motor_mass", .openRocket = 7.8e-17, .qtRocket = 7.8e-17, .tolerance = 1e-09},
    {.key = "axial_drag_coeff", .openRocket = 0.0e+00, .qtRocket = 0.0e+00, .tolerance = 1e-09},
    {.key = "base_drag_coeff", .openRocket = 0.0e+00, .qtRocket = 0.0e+00, .tolerance = 1e-09},
    {.key = "cg_location", .openRocket = 0.0e+00, .qtRocket = 0.0e+00, .tolerance = 1e-09},
    {.key = "drag_coeff", .openRocket = 0.0e+00, .qtRocket = 0.0e+00, .tolerance = 1e-09},
    {.key = "friction_drag_coeff", .openRocket = 0.0e+00, .qtRocket = 0.0e+00, .tolerance = 1e-09},
    {.key = "mass", .openRocket = 0.0e+00, .qtRocket = 0.0e+00, .tolerance = 1e-09},
    {.key = "pressure_drag_coeff", .openRocket = 0.0e+00, .qtRocket = 0.0e+00, .tolerance = 1e-09},
    {.key = "thrust_force", .openRocket = 0.0e+00, .qtRocket = 0.0e+00, .tolerance = 1e-09},
    {.key = "thrust_weight_ratio", .openRocket = 0.0e+00, .qtRocket = 0.0e+00, .tolerance = 1e-09},
}};
// clang-format on

/// Rule H: the columns that are noise while the rocket hunts: the angle of attack, which is
/// the lateral airspeed itself (two runs differ in it by as much as it is there), the normal
/// force and pitch moment coefficients that answer it, and the lateral accelerations they
/// cause (up to 8.8e-4 of their scales, a hundredth of their values). Not compared in a
/// hunting row or a band row.
constexpr std::array<std::string_view, 6> kHuntingNoiseColumns{"aoa",
                                                               "normal_force_coeff",
                                                               "pitch_moment_coeff",
                                                               "acceleration_x",
                                                               "acceleration_xy",
                                                               "acceleration_bodyx"};

/// Rule H: what the hunting decision switches in the row itself: the pitch and yaw rate of the
/// flight conditions, which are zero in a hunting row, and the pitch damping moment they cause
/// (the pitch moment coefficient, which holds it, is one of kHuntingNoiseColumns). Not compared
/// in a band row.
constexpr std::array<std::string_view, 3> kSwitchedColumns{"pitch_rate", "yaw_rate",
                                                           "pitch_damping_moment_coeff"};

// Rule N, the noise-dominated out-of-plane columns, is shared with the comparison of the
// default-step set: kOutOfPlaneColumns, kNoiseRatio and noiseColumns() of GoldenSimulations.h.

/// The tolerance the table @p columns lists for the column @p key; nullopt when it does not
/// list it.
[[nodiscard]] std::optional<double> listedTolerance(std::span<const MeasuredColumn> columns,
                                                    std::string_view                key)
{
    for (const MeasuredColumn& measured : columns)
    {
        if (measured.key == key)
        {
            return measured.tolerance;
        }
    }
    return std::nullopt;
}

/// The tolerance of a value of the column @p key in a Runge-Kutta row in free flight, as a
/// fraction of the scale of the column: that of kMeasuredColumns, or kValueRelative for a
/// column that matched exactly in every measurement.
[[nodiscard]] double stableTolerance(std::string_view key)
{
    return listedTolerance(kMeasuredColumns, key).value_or(kValueRelative);
}

/// The tolerance of a value of the column @p key in a hunting row or a band row (rule H):
/// negative for a column that is noise there, which is not compared; that of kHuntingColumns
/// for an attitude column that keeps its signal; stableTolerance() for every other column.
[[nodiscard]] double huntingTolerance(std::string_view key)
{
    if (std::ranges::find(kHuntingNoiseColumns, key) != kHuntingNoiseColumns.end())
    {
        return -1.0;
    }
    return listedTolerance(kHuntingColumns, key).value_or(stableTolerance(key));
}

/// The tolerance of a value of the column @p key in a row of an Euler stepper: that of
/// kEulerColumns, or stableTolerance().
[[nodiscard]] double eulerTolerance(std::string_view key)
{
    return listedTolerance(kEulerColumns, key).value_or(stableTolerance(key));
}

// ------------------------------------------------------------------------------------- plans

/// How the values of a column of a branch are compared.
enum class ColumnKind : std::uint8_t
{
    EVERY_ROW,          ///< in every row that is compared
    NOT_WHILE_HUNTING,  ///< one of kHuntingNoiseColumns: not in a hunting or band row (rule H)
    ON_THE_ROD_ONLY     ///< a noise-dominated out-of-plane column (rule N)
};

/// A column of the golden time series of a branch. The tolerances are fractions of the scale.
struct StableColumn
{
    std::string key;
    double      scale{0};         ///< its largest finite magnitude
    double      tolerance{0};     ///< stableTolerance(): in a Runge-Kutta row in free flight
    double      hunting{0};       ///< huntingTolerance(): in a hunting or band row
    double      euler{0};         ///< eulerTolerance(): in a row of an Euler stepper
    bool        switched{false};  ///< one of kSwitchedColumns: not compared in a band row
    ColumnKind  kind{ColumnKind::EVERY_ROW};
};

/// What the golden files of a branch say about how it is compared: nothing in it depends on
/// the run it is compared with.
struct StablePlan
{
    std::size_t rows{0};     ///< the rows of the golden time series
    std::size_t rodRows{0};  ///< its first rodRows rows are on the launch rod: kValueRelative
    /// Rule H. The hunting rows: the rows of a Runge-Kutta stepper in free flight whose pitch
    /// rate and yaw rate are both exactly zero.
    std::vector<bool> hunting;
    /// Rule H. The band rows: the rows of a Runge-Kutta stepper in free flight whose lateral
    /// airspeed is within kHuntingBand of kHuntingThreshold, hunting or not; and the number of
    /// rows whose hunting state is not what their lateral airspeed says (misjudgedRows()).
    std::vector<bool> band;
    std::size_t       misjudgedRows{0};
    /// The rows of an Euler stepper in free flight (it stores no angle of attack).
    std::vector<bool> euler;
    /// Rule T. Whether the branch tumbles before its apogee; the time of its last stage
    /// separation, after which it is not compared then (infinite for a branch that does not
    /// tumble so); and the rows up to that time.
    bool        tumbles{false};
    double      separation{std::numeric_limits<double>::infinity()};
    std::size_t flownRows{0};
    /// Rule E. The row on which an Euler stepper landed its step to the apogee, when it is
    /// among the flown rows; its time; whether the golden step from it is the short one; and
    /// what is wrong with the golden rows there when they do not show the decision by the last
    /// bit that the rule allows for (apogeeStepProblem(); "" when they do).
    std::optional<std::size_t> apogeeRow;
    double                     apogeeTime{std::numeric_limits<double>::infinity()};
    bool                       shortApogeeStep{false};
    std::string                apogeeProblem;
    /// The time of the first event of the branch that no golden row handled on time: the
    /// events after it are compared at kLateHandlingAbsolute. Infinite when there is none.
    double                    lateFrom{std::numeric_limits<double>::infinity()};
    std::vector<StableColumn> columns;
};

/// The time of the first (@p last: the last) golden event of type @p type among @p events;
/// nullopt when there is none.
[[nodiscard]] std::optional<double> eventTime(const json& events, std::string_view type,
                                              bool last = false)
{
    std::optional<double> time;
    for (const json& event : events)
    {
        if (event.at("type").get<std::string>() == type && (last || !time.has_value()))
        {
            time = goldenValue(event.at("time"));
        }
    }
    return time;
}

/// The values of row @p row of the golden time series in the column @p column; NaN when the
/// series has no such column.
[[nodiscard]] double cell(const GoldenTable& table, std::size_t row,
                          const std::optional<std::size_t>& column)
{
    return column.has_value() ? table.rows[row][*column] : std::numeric_limits<double>::quiet_NaN();
}

/// The number of rows that @p flags marks. (A loop: a vector<bool> is no range of bool for the
/// algorithms of every standard library.)
[[nodiscard]] std::int64_t countOf(const std::vector<bool>& flags)
{
    std::int64_t count = 0;
    for (const bool flag : flags)
    {
        count += flag ? 1 : 0;
    }
    return count;
}

/// The first row that @p flags marks; their number when they mark none.
[[nodiscard]] std::size_t firstOf(const std::vector<bool>& flags)
{
    std::size_t row = 0;
    while (row < flags.size() && !flags[row])
    {
        row++;
    }
    return row;
}

/// Rule H: the hunting rows of @p table, whose first @p rodRows rows are on the launch rod.
/// AbstractSimulationStepper.calculateFlightConditions() sets the pitch and the yaw rate of
/// the flight conditions to zero while the lateral airspeed is below 1 mm/s, and an Euler
/// stepper stores no angle of attack.
[[nodiscard]] std::vector<bool> huntingRows(const GoldenTable& table, std::size_t rodRows)
{
    std::vector<bool>                hunting(table.rows.size(), false);
    const std::optional<std::size_t> aoa   = table.columnIndex("aoa");
    const std::optional<std::size_t> pitch = table.columnIndex("pitch_rate");
    const std::optional<std::size_t> yaw   = table.columnIndex("yaw_rate");
    for (std::size_t row = rodRows; row < table.rows.size(); row++)
    {
        hunting[row] = std::isfinite(cell(table, row, aoa)) && cell(table, row, pitch) == 0 &&
                       cell(table, row, yaw) == 0;
    }
    return hunting;
}

/// The lateral airspeed of every row of @p table, in m/s: what calculateFlightConditions()
/// compares with kHuntingThreshold, recovered from what the row stores as the airspeed (the
/// Mach number times the speed of sound) times the sine of the angle of attack. NaN in a row
/// without an angle of attack (an Euler stepper's) and in a table without these columns.
[[nodiscard]] std::vector<double> lateralAirspeeds(const GoldenTable& table)
{
    std::vector<double>              lateral(table.rows.size());
    const std::optional<std::size_t> aoa   = table.columnIndex("aoa");
    const std::optional<std::size_t> mach  = table.columnIndex("mach_number");
    const std::optional<std::size_t> sound = table.columnIndex("speed_of_sound");
    for (std::size_t row = 0; row < table.rows.size(); row++)
    {
        lateral[row] =
            cell(table, row, mach) * cell(table, row, sound) * std::sin(cell(table, row, aoa));
    }
    return lateral;
}

/// Rule H: the band rows among the rows with the lateral airspeeds @p lateral, whose first
/// @p rodRows rows are on the launch rod.
[[nodiscard]] std::vector<bool> bandRows(const std::vector<double>& lateral, std::size_t rodRows)
{
    std::vector<bool> band(lateral.size(), false);
    for (std::size_t row = rodRows; row < lateral.size(); row++)
    {
        // False for the NaN of a row without an angle of attack.
        band[row] = std::abs(lateral[row] - kHuntingThreshold) <= kHuntingBand;
    }
    return band;
}

/// Rule H: the number of Runge-Kutta rows in free flight whose hunting state (@p hunting) is
/// not what their lateral airspeed (@p lateral) says: a hunting row at or above
/// kHuntingThreshold, another row below it. None in the golden files: their rows show the
/// decision of calculateFlightConditions().
[[nodiscard]] std::size_t misjudgedRows(const std::vector<double>& lateral,
                                        const std::vector<bool>& hunting, std::size_t rodRows)
{
    std::size_t misjudged = 0;
    for (std::size_t row = rodRows; row < lateral.size(); row++)
    {
        const bool below = lateral[row] < kHuntingThreshold;
        if (std::isfinite(lateral[row]) && hunting[row] != below)
        {
            misjudged++;
        }
    }
    return misjudged;
}

/// The rows of an Euler stepper among the rows of @p table after its first @p rodRows rows:
/// those without an angle of attack, in a table that has the column.
[[nodiscard]] std::vector<bool> eulerRows(const GoldenTable& table, std::size_t rodRows)
{
    std::vector<bool>                euler(table.rows.size(), false);
    const std::optional<std::size_t> aoa = table.columnIndex("aoa");
    for (std::size_t row = rodRows; aoa.has_value() && row < table.rows.size(); row++)
    {
        euler[row] = !std::isfinite(table.rows[row][*aoa]);
    }
    return euler;
}

/// Rule E: the row of @p table on which an Euler stepper landed its step to the apogee: the
/// first row of an Euler stepper (it stores no angle of attack) rises, and a later row has no
/// vertical velocity. nullopt for a branch that reaches its apogee under a Runge-Kutta stepper,
/// or whose Euler stepper takes over on the way down.
[[nodiscard]] std::optional<std::size_t> eulerApogeeRow(const GoldenTable& table)
{
    const std::optional<std::size_t> aoa      = table.columnIndex("aoa");
    const std::optional<std::size_t> velocity = table.columnIndex("velocity_z");
    if (!aoa.has_value() || !velocity.has_value())
    {
        return std::nullopt;
    }
    std::size_t row = 0;
    while (row < table.rows.size() && std::isfinite(table.rows[row][*aoa]))
    {
        row++;
    }
    if (row == table.rows.size() || !(table.rows[row][*velocity] > 0))
    {
        return std::nullopt;
    }
    while (row < table.rows.size() && !(std::abs(table.rows[row][*velocity]) < kApogeeVelocity))
    {
        row++;
    }
    return row < table.rows.size() ? std::optional<std::size_t>{row} : std::nullopt;
}

/// Rule E: the step a run takes from an apogee row: what its step to the apogee left of the
/// vertical velocity, the vertical velocity of the row before, and the length of the step that
/// follows. NaN where the run has no such row.
struct ApogeeStep
{
    double left{std::numeric_limits<double>::quiet_NaN()};
    double before{std::numeric_limits<double>::quiet_NaN()};
    double step{std::numeric_limits<double>::quiet_NaN()};

    /// Whether the step from the apogee row is the extra one of 1 ms.
    [[nodiscard]] bool isShort() const noexcept { return step <= kShortStep; }
};

/// Rule E: what is wrong with @p step, "" when it shows the decision by the last bit that the
/// rule allows for: the step to the apogee left a rounding error of the vertical velocity
/// (kApogeeResidual), and the extra step of 1 ms follows exactly when that is positive. A run
/// that lands elsewhere than on its apogee, or steps on from it otherwise, differs from the
/// golden run by more than the last bit.
[[nodiscard]] std::string apogeeStepProblem(const ApogeeStep& step)
{
    if (std::isnan(step.step))
    {
        return "there is no row that steps from the apogee";
    }
    if (!(std::abs(step.left) <= kApogeeResidual * std::abs(step.before)))
    {
        return std::format(
            "the step to the apogee leaves a vertical velocity of {} m/s after {} "
            "m/s, which is no rounding error",
            step.left, step.before);
    }
    if ((step.left > 0) != step.isShort())
    {
        return std::format("a step of {} s follows a vertical velocity of {} m/s at the apogee",
                           step.step, step.left);
    }
    return {};
}

/// The step the golden run of @p table takes from its row @p row.
[[nodiscard]] ApogeeStep apogeeStepOf(const GoldenTable& table, std::size_t row)
{
    const std::optional<std::size_t> velocity = table.columnIndex("velocity_z");
    if (row == 0 || row >= table.rows.size())
    {
        return {};
    }
    return {.left   = cell(table, row, velocity),
            .before = cell(table, row - 1, velocity),
            .step   = cell(table, row, table.columnIndex("time_step"))};
}

/// The time of the first of the golden events @p events that no row of @p times handled on
/// time (the step that reached it overshot it: the engine lets a step be no shorter than 1 ms
/// for an event, and the Runge-Kutta steppers no shorter than a twentieth of the time step);
/// infinite when every event has a row at its time.
[[nodiscard]] double firstLateHandling(const json& events, const std::vector<double>& times)
{
    for (const json& event : events)
    {
        const double time = goldenValue(event.at("time"));
        if (!times.empty() && std::ranges::none_of(times, [time](double row) {
                return std::abs(row - time) <= kOnTime;
            }))
        {
            return time;
        }
    }
    return std::numeric_limits<double>::infinity();
}

/// The columns of @p table with their scales, tolerances and kinds (rules H and N).
[[nodiscard]] std::vector<StableColumn> stableColumns(const GoldenTable& table)
{
    std::vector<StableColumn> columns;
    columns.reserve(table.columns.size());
    for (std::size_t i = 0; i < table.columns.size(); i++)
    {
        const std::string& key     = table.columns[i];
        const double       hunting = huntingTolerance(key);
        columns.push_back(
            {.key       = key,
             .scale     = columnScale(table, i),
             .tolerance = stableTolerance(key),
             .hunting   = hunting,
             .euler     = eulerTolerance(key),
             .switched  = std::ranges::find(kSwitchedColumns, key) != kSwitchedColumns.end(),
             .kind      = hunting < 0 ? ColumnKind::NOT_WHILE_HUNTING : ColumnKind::EVERY_ROW});
    }
    // Rule N: the columns that noiseColumns() of GoldenSimulations.h names, as in the
    // comparison of the default-step set.
    const std::vector<bool> noise = noiseColumns(table);
    for (std::size_t i = 0; i < columns.size(); i++)
    {
        if (noise[i])
        {
            columns[i].kind = ColumnKind::ON_THE_ROD_ONLY;
        }
    }
    return columns;
}

/// The plan of the comparison of the golden branch @p branch with its time series @p table;
/// @p cleared is when the rocket of the simulation clears the launch rod (nullopt: never).
[[nodiscard]] StablePlan stablePlan(const json& branch, const GoldenTable& table,
                                    const std::optional<double>& cleared)
{
    StablePlan plan;
    plan.rows                         = table.rows.size();
    plan.rodRows                      = rowsUpToTheClearance(table, cleared);
    plan.hunting                      = huntingRows(table, plan.rodRows);
    const std::vector<double> lateral = lateralAirspeeds(table);
    plan.band                         = bandRows(lateral, plan.rodRows);
    plan.misjudgedRows                = misjudgedRows(lateral, plan.hunting, plan.rodRows);
    plan.euler                        = eulerRows(table, plan.rodRows);
    plan.columns                      = stableColumns(table);
    plan.flownRows                    = plan.rows;
    const std::vector<double> times   = table.column("time").value_or(std::vector<double>{});
    const json&               events  = branch.at("events");

    const std::optional<double> tumble = eventTime(events, "TUMBLE");
    const std::optional<double> apogee = eventTime(events, "APOGEE");
    if (tumble.has_value() && apogee.has_value() && *tumble < *apogee)
    {
        plan.tumbles    = true;
        plan.separation = eventTime(events, "STAGE_SEPARATION", true)
                              .value_or(-std::numeric_limits<double>::infinity());
        plan.flownRows  = static_cast<std::size_t>(
            std::ranges::count_if(times, [&plan](double time) { return time <= plan.separation; }));
    }
    const std::optional<std::size_t> landed = eulerApogeeRow(table);
    if (landed.has_value() && *landed < plan.flownRows && *landed < times.size())
    {
        const ApogeeStep step = apogeeStepOf(table, *landed);
        plan.apogeeRow        = landed;
        plan.apogeeTime       = times[*landed];
        plan.shortApogeeStep  = step.isShort();
        plan.apogeeProblem    = apogeeStepProblem(step);
    }
    plan.lateFrom = firstLateHandling(events, times);
    return plan;
}

/// How a value of the time series of a branch is compared, by its column and its row.
enum class CellKind : std::uint8_t
{
    ROD,            ///< on the launch rod: at kValueRelative
    FREE,           ///< in a Runge-Kutta row in free flight: at the tolerance of its column
    HUNTING,        ///< in a hunting or band row: at the hunting tolerance of its column
    EULER,          ///< in a row of an Euler stepper: at the Euler tolerance of its column
    NOISE,          ///< not compared: a noise-dominated out-of-plane column off the rod (rule N)
    HUNTING_NOISE,  ///< not compared: a column that is noise while hunting, in a hunting row
    BAND            ///< not compared: what the hunting decision can switch, in a band row
};

/// The kind of the value of @p column in row @p row of a branch with the plan @p plan.
[[nodiscard]] CellKind cellKind(const StablePlan& plan, const StableColumn& column, std::size_t row)
{
    if (row < plan.rodRows)
    {
        return CellKind::ROD;
    }
    if (column.kind == ColumnKind::ON_THE_ROD_ONLY)
    {
        return CellKind::NOISE;
    }
    if (plan.hunting[row] && column.hunting < 0)
    {
        return CellKind::HUNTING_NOISE;
    }
    if (plan.band[row] && (column.switched || column.hunting < 0))
    {
        return CellKind::BAND;
    }
    if (plan.hunting[row] || plan.band[row])
    {
        return CellKind::HUNTING;
    }
    return plan.euler[row] ? CellKind::EULER : CellKind::FREE;
}

/// The tolerance of a value of the kind @p kind of @p column, in the unit of the column;
/// negative for a value that the rules H and N exclude.
[[nodiscard]] double toleranceOf(CellKind kind, const StableColumn& column)
{
    switch (kind)
    {
        case CellKind::ROD:
            return kValueRelative * column.scale;
        case CellKind::FREE:
            return column.tolerance * column.scale;
        case CellKind::HUNTING:
            return column.hunting * column.scale;
        case CellKind::EULER:
            return column.euler * column.scale;
        case CellKind::NOISE:
        case CellKind::HUNTING_NOISE:
        case CellKind::BAND:
            break;
    }
    return -1.0;
}

/// The tolerance of the value of @p column in row @p row of a branch with the plan @p plan,
/// in the unit of the column; negative for a value that the rules H and N exclude.
[[nodiscard]] double cellTolerance(const StablePlan& plan, const StableColumn& column,
                                   std::size_t row)
{
    return toleranceOf(cellKind(plan, column, row), column);
}

/// How far a branch of a run is compared with its golden branch: the plan, and what the step
/// from the apogee row decides (rule E).
struct StableExtent
{
    std::size_t rows{0};       ///< the rows [0, rows) of the golden time series are compared
    bool        whole{false};  ///< every row of the golden time series is
    /// The events before this time are compared (and none after the separation of rule T).
    double until{std::numeric_limits<double>::infinity()};
    /// Rule E: why the run's other step from the apogee row is not the decision by the last
    /// bit that the rule allows for (apogeeStepProblem()); "" when it is, or steps alike.
    std::string problem;
};

/// The step @p branch takes from its row @p row.
[[nodiscard]] ApogeeStep apogeeStepOf(const FlightDataBranch& branch, std::size_t row)
{
    const std::vector<double>* velocity =
        branch.getView(FlightDataType::builtin(QtRocket::FlightDataTypeId::TYPE_VELOCITY_Z));
    const std::vector<double>* steps =
        branch.getView(FlightDataType::builtin(QtRocket::FlightDataTypeId::TYPE_TIME_STEP));
    if (velocity == nullptr || steps == nullptr || row == 0 || row >= velocity->size() ||
        row >= steps->size())
    {
        return {};
    }
    return {.left = (*velocity)[row], .before = (*velocity)[row - 1], .step = (*steps)[row]};
}

/// How far @p branch is compared with the golden branch of the plan @p plan. Rule E: when the
/// run steps from the apogee row of the golden run as that does (with the extra step of 1 ms,
/// or without it), the branch is compared to its last row; when it does not, up to that row,
/// and the run's rows have to show that the last bit decided (apogeeStepProblem()).
[[nodiscard]] StableExtent stableExtent(const StablePlan& plan, const FlightDataBranch& branch)
{
    StableExtent extent;
    extent.rows = plan.flownRows;
    if (plan.apogeeRow.has_value())
    {
        const ApogeeStep step = apogeeStepOf(branch, *plan.apogeeRow);
        if (std::isnan(step.step) || step.isShort() != plan.shortApogeeStep)
        {
            extent.rows    = *plan.apogeeRow;
            extent.until   = plan.apogeeTime - kOnTime;
            extent.problem = apogeeStepProblem(step);
        }
    }
    extent.whole = extent.rows == plan.rows;
    return extent;
}

/// The least of a branch with the plan @p plan that is compared on every platform: its rows
/// up to the apogee row of rule E or to the separation of rule T.
[[nodiscard]] std::size_t floorRows(const StablePlan& plan)
{
    return plan.apogeeRow.value_or(plan.flownRows);
}

/// The number of values of the first @p rows rows of a branch with the plan @p plan that the
/// rules H and N leave to compare.
[[nodiscard]] std::int64_t comparedValues(const StablePlan& plan, std::size_t rows)
{
    std::int64_t values = 0;
    for (const StableColumn& column : plan.columns)
    {
        for (std::size_t row = 0; row < rows; row++)
        {
            values += cellTolerance(plan, column, row) >= 0 ? 1 : 0;
        }
    }
    return values;
}

// ------------------------------------------------------------------------------ measurements

/// The largest differences the comparison of the stable-step set found, by what was compared
/// (a column, a summary value, the times of the events of a type, ...) over every simulation;
/// see SimulationStableGoldenMeasurement.
class StableMeasurements
{
public:
    /// A difference @p difference of @p what (a column: as a fraction of its scale) against
    /// the tolerance @p tolerance, found at @p where. A negative tolerance: a value that the
    /// rules exclude, recorded for the table only.
    void record(const std::string& what, double difference, double tolerance,
                const std::string& where)
    {
        Entry& entry = m_entries[what];
        entry.count++;
        if (difference > entry.largest || entry.where.empty())
        {
            entry.largest = difference;
            entry.where   = where;
        }
        if (tolerance > 0)
        {
            entry.ofTolerance = std::max(entry.ofTolerance, difference / tolerance);
        }
        else if (tolerance == 0 && difference > 0)
        {
            entry.ofTolerance = std::numeric_limits<double>::infinity();
        }
    }

    /// The table: one line per thing compared, with how many values, their largest
    /// difference, that difference as a multiple of its tolerance and where it was found.
    [[nodiscard]] std::string text() const
    {
        std::string text;
        for (const auto& [what, entry] : m_entries)
        {
            text += std::format("{}\t{}\t{:.2e}\t{:.2e}\t{}\n", what, entry.count, entry.largest,
                                entry.ofTolerance, entry.where);
        }
        return text;
    }

    /// The largest difference of a compared value, as a multiple of its tolerance.
    [[nodiscard]] double largestOfTolerance() const
    {
        double largest = 0;
        for (const auto& [what, entry] : m_entries)
        {
            largest = std::max(largest, entry.ofTolerance);
        }
        return largest;
    }

private:
    struct Entry
    {
        std::int64_t count{0};
        double       largest{0};
        double       ofTolerance{0};  ///< the largest difference, as a multiple of its tolerance
        std::string  where;
    };
    std::map<std::string, Entry> m_entries;
};

// -------------------------------------------------------------------------------- comparison

/// What the comparison of a run with its stable golden files works with and collects.
struct StableComparison
{
    std::string         context;   ///< "testrocket-beta/stable/sim_02_b4-3-d21-0"
    SimulationCounts    compared;  ///< what was compared with the golden files
    SimulationCounts    excluded;  ///< the numbers, rows and values the rules exclude
    std::string         report;    ///< the mismatches
    StableMeasurements* measurements{nullptr};
};

/// Compares the number @p actual with the golden number @p expected within @p tolerance,
/// unless the rules exclude it (@p compared false): it is counted then. @p what names it in
/// the measurements, in which a difference counts as a fraction of @p unit.
struct StableNumber
{
    std::string field;
    std::string what;
    double      expected{0};
    double      actual{0};
    double      tolerance{0};
    double      unit{1};
};
void compareStableNumber(Mismatches& m, StableComparison& c, const StableNumber& number,
                         bool compared)
{
    if (!compared)
    {
        c.excluded.numbers++;
        return;
    }
    c.compared.numbers++;
    if (c.measurements != nullptr && std::isfinite(number.expected) &&
        std::isfinite(number.actual) && number.unit > 0)
    {
        c.measurements->record(number.what, std::abs(number.actual - number.expected) / number.unit,
                               number.tolerance / number.unit, c.context);
    }
    m.within(number.field, number.expected, number.actual, 0.0, number.tolerance);
}

/// Compares the count @p actual with the golden count @p expected, unless the rules exclude
/// it (@p compared false): it is counted then.
void compareStableCount(Mismatches& m, StableComparison& c, std::string_view field,
                        std::int64_t expected, std::int64_t actual, bool compared)
{
    if (!compared)
    {
        c.excluded.numbers++;
        return;
    }
    c.compared.numbers++;
    m.integer(field, expected, actual);
}

/// What the comparison of a branch says of it to the comparison of its simulation.
struct StableBranchOutcome
{
    bool toTheEnd{false};  ///< it is compared to its last row
    bool tumbles{false};   ///< it tumbles before its apogee (rule T)
};

/// How much of the first branch a summary value of the stable-step set needs to be compared.
enum class SummaryExtent : std::uint8_t
{
    ANY,  ///< decided before the apogee: always compared
    /// The descent: not when the first branch tumbles before its apogee (rule T). The ground
    /// hit velocity: it ends a descent whose time grid the step of rule E moves, but not its
    /// course (two runs of OpenRocket on the two grids differ in it by 1.6e-10 m/s).
    THE_DESCENT,
    /// The last row: only when the first branch is compared to it (rules E and T). The flight
    /// time, which the extra step of rule E moves by up to 1.4e-4 s.
    THE_LAST_ROW
};

/// A summary value of the stable-step set: its golden key and getter; whether it is a time;
/// its tolerance (in s, or as a fraction of its scale); the column of the first branch whose
/// scale is its scale ("": its own magnitude); and how much of the first branch it needs.
struct StableSummaryValue
{
    std::string_view key;
    double (FlightData::*get)() const noexcept;
    bool             time;
    double           tolerance;
    std::string_view column;
    SummaryExtent    extent;
};
constexpr std::array<StableSummaryValue, 10> kStableSummaryValues{{
    {.key       = "maxAltitude",
     .get       = &FlightData::getMaxAltitude,
     .time      = false,
     .tolerance = kStableSummaryRelative,
     .column    = "altitude",
     .extent    = SummaryExtent::ANY},
    {.key       = "maxVelocity",
     .get       = &FlightData::getMaxVelocity,
     .time      = false,
     .tolerance = kStableSummaryRelative,
     .column    = "velocity_total",
     .extent    = SummaryExtent::ANY},
    {.key       = "maxAcceleration",
     .get       = &FlightData::getMaxAcceleration,
     .time      = false,
     .tolerance = kStableSummaryRelative,
     .column    = "acceleration_total",
     .extent    = SummaryExtent::ANY},
    {.key       = "maxMachNumber",
     .get       = &FlightData::getMaxMachNumber,
     .time      = false,
     .tolerance = kStableSummaryRelative,
     .column    = "mach_number",
     .extent    = SummaryExtent::ANY},
    {.key       = "timeToApogee",
     .get       = &FlightData::getTimeToApogee,
     .time      = true,
     .tolerance = kStableTimeAbsolute,
     .column    = "",
     .extent    = SummaryExtent::ANY},
    {.key       = "flightTime",
     .get       = &FlightData::getFlightTime,
     .time      = true,
     .tolerance = kStableTimeAbsolute,
     .column    = "",
     .extent    = SummaryExtent::THE_LAST_ROW},
    // The velocity at the ground: a fraction of itself (a thirtieth of the largest velocity).
    {.key       = "groundHitVelocity",
     .get       = &FlightData::getGroundHitVelocity,
     .time      = false,
     .tolerance = kStableSummaryRelative,
     .column    = "",
     .extent    = SummaryExtent::THE_DESCENT},
    // The velocity at the first record after the launch rod: a value of the rod.
    {.key       = "launchRodVelocity",
     .get       = &FlightData::getLaunchRodVelocity,
     .time      = false,
     .tolerance = kValueRelative,
     .column    = "velocity_total",
     .extent    = SummaryExtent::ANY},
    {.key       = "deploymentVelocity",
     .get       = &FlightData::getDeploymentVelocity,
     .time      = false,
     .tolerance = kDeploymentVelocityRelative,
     .column    = "velocity_total",
     .extent    = SummaryExtent::ANY},
    {.key       = "optimumDelay",
     .get       = &FlightData::getOptimumDelay,
     .time      = true,
     .tolerance = kStableTimeAbsolute,
     .column    = "",
     .extent    = SummaryExtent::ANY},
}};

/// The scale of column @p key of the first of @p tables; 0 when there is no such column.
[[nodiscard]] double scaleOfTheFirstBranch(const std::vector<GoldenTable>& tables,
                                           std::string_view                key)
{
    if (tables.empty())
    {
        return 0;
    }
    const std::optional<std::size_t> column = tables.front().columnIndex(key);
    return column.has_value() ? columnScale(tables.front(), *column) : 0;
}

/// Whether a summary value that needs @p extent of the first branch is compared when the
/// comparison of that branch came out as @p first.
[[nodiscard]] bool summaryIsCompared(SummaryExtent extent, const StableBranchOutcome& first)
{
    switch (extent)
    {
        case SummaryExtent::THE_DESCENT:
            return !first.tumbles;
        case SummaryExtent::THE_LAST_ROW:
            return first.toTheEnd;
        case SummaryExtent::ANY:
            break;
    }
    return true;
}

/// Compares the summary values of @p data with the golden "summary"; @p first is how the
/// comparison of the first branch came out.
void compareStableSummary(Mismatches& m, StableComparison& c, const GoldenFiles& golden,
                          const FlightData& data, const StableBranchOutcome& first)
{
    const json& expected = golden.document.at("summary");
    for (const StableSummaryValue& value : kStableSummaryValues)
    {
        const double goldenNumber = goldenValue(expected.at(value.key));
        const double actual       = (data.*value.get)();
        const double unit = value.time
                                ? 1.0
                                : referenceOf(scaleOfTheFirstBranch(golden.tables, value.column),
                                              goldenNumber, actual);
        compareStableNumber(m, c,
                            {.field     = std::string{value.key},
                             .what      = std::format("summary:{}", value.key),
                             .expected  = goldenNumber,
                             .actual    = actual,
                             .tolerance = value.tolerance * unit,
                             .unit      = unit},
                            summaryIsCompared(value.extent, first));
    }
}

/// Compares what the golden warning @p expected prints of its parameter with @p actual: the
/// parameter (a speed or an angle) within kStableSummaryRelative of itself, and the
/// description and the text, which print it with three digits. (What identifies the warning,
/// its class, priority and sources, is compared with the structure.) A golden warning without
/// a parameter has no number: nothing is compared or counted.
void compareStableWarning(Mismatches& m, StableComparison& c, const std::string& field,
                          const json& expected, const Warning* actual)
{
    if (!expected.is_object() || !expected.contains("parameter"))
    {
        return;
    }
    const std::optional<double> parameter =
        actual != nullptr ? parameterOf(*actual) : std::optional<double>{};
    if (!parameter.has_value())
    {
        c.excluded.numbers++;  // the structure reports a warning of another kind
        return;
    }
    const double goldenNumber = goldenValue(expected.at("parameter"));
    const double unit         = referenceOf(0, goldenNumber, *parameter);
    compareStableNumber(m, c,
                        {.field     = field + ".parameter",
                         .what      = "warning parameter",
                         .expected  = goldenNumber,
                         .actual    = *parameter,
                         .tolerance = kStableSummaryRelative * unit,
                         .unit      = unit},
                        true);
    m.text(field + ".description", expected.at("description").get<std::string>(),
           actual->messageDescription());
    m.text(field + ".text", expected.at("text").get<std::string>(), actual->toString());
}

/// compareStableWarning() of the warnings of @p data against the golden "warnings".
void compareStableWarnings(Mismatches& m, StableComparison& c, const json& expected,
                           const FlightData& data)
{
    std::vector<const Warning*> warnings;
    for (const Warning& warning : data.getWarningSet())
    {
        warnings.push_back(&warning);
    }
    for (std::size_t i = 0; i < expected.size(); i++)
    {
        compareStableWarning(m, c, std::format("warnings[{}]", i), expected.at(i),
                             i < warnings.size() ? warnings[i] : nullptr);
    }
}

/// What the comparison of a branch of the stable-step set works with.
struct StableBranch
{
    const FlightData*       data{nullptr};
    const FlightDataBranch* branch{nullptr};
    const GoldenTable*      table{nullptr};
    StablePlan              plan;
    StableExtent            extent;
};

/// Whether the golden event at @p time is compared in @p ours: not after the separation of a
/// branch that tumbles before its apogee (rule T), and not from the apogee row on when the two
/// runs step differently from it (rule E).
[[nodiscard]] bool eventIsCompared(const StableBranch& ours, double time)
{
    return time <= ours.plan.separation && time < ours.extent.until;
}

/// The tolerance of the time of the golden event at @p time in a branch with the plan @p plan.
[[nodiscard]] double eventTolerance(const StablePlan& plan, double time)
{
    return time > plan.lateFrom ? kLateHandlingAbsolute : kStableTimeAbsolute;
}

/// The number of the numbers of the golden events @p events: their times, and the parameters
/// of their warnings.
[[nodiscard]] int eventNumbers(const json& events)
{
    int numbers = static_cast<int>(events.size());
    for (const json& event : events)
    {
        const json& data = event.at("data");
        numbers += data.is_object() && data.contains("parameter") ? 1 : 0;
    }
    return numbers;
}

/// Compares the times of the events of the branch with the golden "events", and what their
/// warnings print of their parameters. The sequence of the events (their types and sources)
/// and their data are compared with the structure: when the sequences differ, nothing is
/// compared here.
void compareStableEvents(Mismatches& m, StableComparison& c, const json& expected,
                         const StableBranch& ours)
{
    const std::vector<const FlightEvent*> events = orderedEvents(*ours.branch, false);
    const std::vector<const json*>        golden = orderedGoldenEvents(expected, false);
    if (sequenceOf(golden) != sequenceOf(events))
    {
        c.excluded.numbers += eventNumbers(expected);
        return;
    }
    for (std::size_t i = 0; i < events.size(); i++)
    {
        const std::string field = std::format("events[{}]", i);
        const double      time  = goldenValue(golden[i]->at("time"));
        const bool        late  = time > ours.plan.lateFrom;
        compareStableNumber(m, c,
                            {.field = field + ".time",
                             .what = std::format("event{}:{}", late ? " after a late handling" : "",
                                                 golden[i]->at("type").get<std::string>()),
                             .expected  = time,
                             .actual    = events[i]->getTime(),
                             .tolerance = eventTolerance(ours.plan, time),
                             .unit      = 1.0},
                            eventIsCompared(ours, time));
        compareStableWarning(m, c, field + ".data", golden[i]->at("data"),
                             ours.data->findWarning(*events[i]));
    }
}

/// Where a column attains one of its values: in a row in which it is compared, in a row in
/// which it is not, in a row in which it is compared at its hunting tolerance.
struct Attainment
{
    bool compared{false};
    bool excluded{false};
    bool hunting{false};

    /// Whether the column attains the value only where the rules exclude it.
    [[nodiscard]] bool onlyExcluded() const noexcept { return excluded && !compared; }
};

/// Where the @p size values of a column (@p value gives them) attain @p extreme; the column
/// is @p column of a branch with the plan @p plan that is compared in its first @p rows rows.
template <class ValueAt>
[[nodiscard]] Attainment attainmentOf(const StablePlan& plan, const StableColumn& column,
                                      std::size_t rows, std::size_t size, double extreme,
                                      const ValueAt& value)
{
    Attainment attainment;
    for (std::size_t row = 0; row < size; row++)
    {
        if (value(row) == extreme)
        {
            // A row beyond the compared ones (it may be beyond the plan's) has no kind.
            const CellKind kind     = row < rows ? cellKind(plan, column, row) : CellKind::NOISE;
            const bool     compared = toleranceOf(kind, column) >= 0;
            attainment.compared     = attainment.compared || compared;
            attainment.excluded     = attainment.excluded || !compared;
            attainment.hunting      = attainment.hunting || kind == CellKind::HUNTING;
        }
    }
    return attainment;
}

/// One column of a branch of a run against the same column of the golden time series.
struct StableSeries
{
    const StableBranch*        ours{nullptr};
    std::size_t                index{0};  ///< of the column, in the plan and in the series
    const std::vector<double>* values{nullptr};
};

/// How a minimum or maximum of a column is compared: whether it is, and its tolerance as a
/// fraction of the scale of the column.
struct ExtremeRule
{
    bool   compared{true};
    double tolerance{0};
};

/// How the golden minimum or maximum @p expected of the column of @p series and the run's
/// @p actual are compared. An extreme that a run attains only in rows in which the rules
/// exclude the column is one of the excluded values: it is not compared, in either run. (Two
/// NaNs, of a column without a number, are compared; so is an extreme that is no value of its
/// column at all, which then differs.) The tolerance is that of the column (that of the launch
/// rod for a branch that never leaves it), or its hunting tolerance when a run attains the
/// extreme in a row in which the column is compared at that.
[[nodiscard]] ExtremeRule extremeRule(const StableSeries& series, double expected, double actual)
{
    const StableBranch& ours   = *series.ours;
    const StableColumn& column = ours.plan.columns[series.index];
    ExtremeRule         rule{
        .compared  = true,
        .tolerance = ours.plan.rodRows == ours.plan.rows ? kValueRelative : column.tolerance};
    if (std::isnan(expected) && std::isnan(actual))
    {
        return rule;
    }
    const Attainment golden =
        attainmentOf(ours.plan, column, ours.extent.rows, ours.table->rows.size(), expected,
                     [&](std::size_t row) { return ours.table->rows[row][series.index]; });
    const Attainment run = attainmentOf(
        ours.plan, column, std::min(ours.extent.rows, series.values->size()), series.values->size(),
        actual, [&](std::size_t row) { return (*series.values)[row]; });
    rule.compared = !golden.onlyExcluded() && !run.onlyExcluded();
    if (golden.hunting || run.hunting)
    {
        rule.tolerance = std::max(rule.tolerance, column.hunting);
    }
    return rule;
}

/// Compares the minimum and the maximum of the column of @p series with the golden "min" and
/// "max" of @p expected, as extremeRule() says.
void compareStableExtremes(Mismatches& m, StableComparison& c, const json& expected,
                           const StableSeries& series, const FlightDataType& type)
{
    const StableBranch& ours   = *series.ours;
    const StableColumn& column = ours.plan.columns[series.index];
    const std::array<std::pair<std::string_view, double>, 2> extremes{
        {{"min", ours.branch->getMinimum(type)}, {"max", ours.branch->getMaximum(type)}}};
    for (const auto& [bound, actual] : extremes)
    {
        const double      goldenNumber = goldenValue(expected.at(bound));
        const ExtremeRule rule         = extremeRule(series, goldenNumber, actual);
        compareStableNumber(
            m, c,
            {.field = std::format("columns[{}].{} ({})", series.index, bound, column.key),
             .what =
                 (rule.tolerance > column.tolerance ? "hunting extreme:" : "extreme:") + column.key,
             .expected  = goldenNumber,
             .actual    = actual,
             .tolerance = rule.tolerance * column.scale,
             .unit      = column.scale},
            rule.compared);
    }
}

/// What the table of the measurements calls a value of the kind @p kind.
[[nodiscard]] std::string_view measurementLabel(CellKind kind)
{
    switch (kind)
    {
        case CellKind::ROD:
            return "rod column:";
        case CellKind::FREE:
            return "free column:";
        case CellKind::HUNTING:
            return "hunting column:";
        case CellKind::EULER:
            return "euler column:";
        case CellKind::NOISE:
            return "excluded, noise column:";
        case CellKind::HUNTING_NOISE:
            return "excluded, hunting column:";
        case CellKind::BAND:
            break;
    }
    return "excluded, band column:";
}

/// Records the difference of the value in row @p row of the column of @p series from the
/// golden one, which is of the kind @p kind.
void measureStableValue(const StableComparison& c, const StableSeries& series, std::size_t row,
                        CellKind kind)
{
    const StableBranch& ours     = *series.ours;
    const StableColumn& column   = ours.plan.columns[series.index];
    const double        expected = ours.table->rows[row][series.index];
    const double        actual   = (*series.values)[row];
    if (!std::isfinite(expected) || !std::isfinite(actual) || !(column.scale > 0))
    {
        return;
    }
    c.measurements->record(std::format("{}{}", measurementLabel(kind), column.key),
                           std::abs(actual - expected) / column.scale,
                           toleranceOf(kind, column) / column.scale,
                           std::format("{} row {}", c.context, row));
}

/// Compares the values of the column of @p series with the golden ones, row by row, each at
/// the tolerance the rules give it. The rows that differ are reported in one line: how many,
/// and the first of them.
void compareStableSeries(Mismatches& m, const StableComparison& c, const StableSeries& series)
{
    const StableBranch& ours      = *series.ours;
    const StableColumn& column    = ours.plan.columns[series.index];
    const std::size_t   rows      = std::min(ours.extent.rows, series.values->size());
    std::size_t         differing = 0;
    std::size_t         first     = 0;
    for (std::size_t row = 0; row < rows; row++)
    {
        const CellKind kind      = cellKind(ours.plan, column, row);
        const double   tolerance = toleranceOf(kind, column);
        if (c.measurements != nullptr)
        {
            measureStableValue(c, series, row, kind);
        }
        if (tolerance >= 0 &&
            differs(ours.table->rows[row][series.index], (*series.values)[row], tolerance))
        {
            first = differing == 0 ? row : first;
            differing++;
        }
    }
    if (differing > 0)
    {
        const double expected = ours.table->rows[first][series.index];
        const double actual   = (*series.values)[first];
        m.note(
            std::format("column {}: {} of {} rows differ, the first at row {}: expected {}, "
                        "got {} (difference {})",
                        column.key, differing, rows, first, expected, actual, actual - expected));
    }
}

/// The time series of @p branch in the form of a golden time series: its CSV columns.
[[nodiscard]] GoldenTable timeSeriesOf(const FlightDataBranch& branch)
{
    // (The rows are made with the table: GCC takes assign() on the rows of a new table for a
    // potential null pointer dereference.)
    GoldenTable table{.columns = {}, .rows = std::vector<std::vector<double>>(branch.getLength())};
    for (const FlightDataType* type : csvTypes(branch))
    {
        table.columns.push_back(columnKey(*type));
        // A type of the branch has a column: getView() gives it.
        const std::vector<double>* values = branch.getView(*type);
        for (std::size_t row = 0; values != nullptr && row < values->size(); row++)
        {
            table.rows[row].push_back((*values)[row]);
        }
    }
    return table;
}

/// Records, for the table of the measurements, how the run decides the hunting (rule H) in
/// the Runge-Kutta rows in free flight that are compared, against the golden run:
/// - whether a row hunts in one of the two runs only (1, else 0);
/// - in a band row, the difference of the lateral airspeed, against kHuntingBand;
/// - outside the band, that difference as a fraction of the distance of the golden lateral
///   airspeed from the threshold: how much of the way to the other decision the run went.
void measureHuntingDecision(const StableComparison& c, const StableBranch& ours)
{
    const GoldenTable         run     = timeSeriesOf(*ours.branch);
    const std::vector<bool>   hunting = huntingRows(run, ours.plan.rodRows);
    const std::vector<double> golden  = lateralAirspeeds(*ours.table);
    const std::vector<double> actual  = lateralAirspeeds(run);
    for (std::size_t row = ours.plan.rodRows; row < std::min(ours.extent.rows, run.rows.size());
         row++)
    {
        if (!std::isfinite(golden[row]) || !std::isfinite(actual[row]))
        {
            continue;
        }
        const std::string where      = std::format("{} row {}", c.context, row);
        const double      difference = std::abs(actual[row] - golden[row]);
        c.measurements->record("hunting decision: a row decided otherwise",
                               hunting[row] != ours.plan.hunting[row] ? 1.0 : 0.0, -1.0, where);
        if (ours.plan.band[row])
        {
            c.measurements->record("hunting decision: lateral airspeed of a band row (m/s)",
                                   difference, kHuntingBand, where);
        }
        else
        {
            c.measurements->record(
                "hunting decision: outside the band, difference / distance from the threshold",
                difference / std::abs(golden[row] - kHuntingThreshold), -1.0, where);
        }
    }
}

/// Records, for the table of the measurements, what the run's step to the apogee of an Euler
/// stepper left of the vertical velocity, as a fraction of that of the row before (rule E).
void measureApogeeStep(const StableComparison& c, const StableBranch& ours)
{
    if (!ours.plan.apogeeRow.has_value())
    {
        return;
    }
    const ApogeeStep step = apogeeStepOf(*ours.branch, *ours.plan.apogeeRow);
    if (std::isfinite(step.left) && std::abs(step.before) > 0)
    {
        c.measurements->record("apogee row: velocity left / velocity before",
                               std::abs(step.left / step.before), kApogeeResidual,
                               std::format("{} row {}", c.context, *ours.plan.apogeeRow));
    }
}

/// Compares the columns of the branch with the golden "columns" and time series: the minimum
/// and maximum of each, and its values.
void compareStableColumns(Mismatches& m, StableComparison& c, const json& expected,
                          const StableBranch& ours)
{
    const std::vector<const FlightDataType*> types = csvTypes(*ours.branch);
    // The number of rows is compared for a branch that is compared to its last row; of
    // another one the rows that are compared have to be there.
    if (!ours.extent.whole && ours.branch->getLength() < ours.extent.rows)
    {
        m.note(std::format("the run has {} rows, fewer than the {} that are compared",
                           ours.branch->getLength(), ours.extent.rows));
    }
    if (c.measurements != nullptr)
    {
        measureHuntingDecision(c, ours);
        measureApogeeStep(c, ours);
    }
    for (std::size_t i = 0; i < std::min({types.size(), expected.size(), ours.plan.columns.size()});
         i++)
    {
        const StableSeries series{
            .ours = &ours, .index = i, .values = ours.branch->getView(*types[i])};
        compareStableExtremes(m, c, expected.at(i), series, *types[i]);
        compareStableSeries(m, c, series);
    }
    const std::int64_t compared = comparedValues(ours.plan, ours.extent.rows);
    c.compared.values += compared;
    c.excluded.values +=
        static_cast<std::int64_t>(ours.plan.rows * ours.plan.columns.size()) - compared;
    c.compared.rows += static_cast<std::int64_t>(ours.extent.rows);
    c.excluded.rows += static_cast<std::int64_t>(ours.plan.rows - ours.extent.rows);
}

/// Compares the numbers of the header of the branch with the golden branch @p expected: its
/// number of rows when it is compared to its last row; its optimum altitude, the time to it
/// and the optimum delay unless it tumbles before its apogee (rule T: the nested coast then
/// starts from a state that is not reproducible); and its separation time.
void compareStableBranchHeader(Mismatches& m, StableComparison& c, const json& expected,
                               const StableBranch& ours)
{
    const FlightDataBranch& branch = *ours.branch;
    compareStableCount(m, c, "rows", expected.at("rows").get<std::int64_t>(),
                       static_cast<std::int64_t>(branch.getLength()), ours.extent.whole);
    const double altitude = goldenValue(expected.at("optimumAltitude"));
    const double unit     = referenceOf(0, altitude, branch.getOptimumAltitude());
    compareStableNumber(m, c,
                        {.field     = "optimumAltitude",
                         .what      = "branch:optimumAltitude",
                         .expected  = altitude,
                         .actual    = branch.getOptimumAltitude(),
                         .tolerance = kStableSummaryRelative * unit,
                         .unit      = unit},
                        !ours.plan.tumbles);
    compareStableNumber(m, c,
                        {.field     = "timeToOptimumAltitude",
                         .what      = "branch:timeToOptimumAltitude",
                         .expected  = goldenValue(expected.at("timeToOptimumAltitude")),
                         .actual    = branch.getTimeToOptimumAltitude(),
                         .tolerance = kStableTimeAbsolute,
                         .unit      = 1.0},
                        !ours.plan.tumbles);
    compareStableNumber(m, c,
                        {.field     = "optimumDelay",
                         .what      = "branch:optimumDelay",
                         .expected  = goldenValue(expected.at("optimumDelay")),
                         .actual    = branch.getOptimumDelay(),
                         .tolerance = kStableTimeAbsolute,
                         .unit      = 1.0},
                        !ours.plan.tumbles);
    // The time of the STAGE_SEPARATION event (NaN without one): compared like that event's.
    const double separation = goldenValue(expected.at("separationTime"));
    compareStableNumber(m, c,
                        {.field     = "separationTime",
                         .what      = "branch:separationTime",
                         .expected  = separation,
                         .actual    = branch.getSeparationTime(),
                         .tolerance = eventTolerance(ours.plan, separation),
                         .unit      = 1.0},
                        true);
}

/// Compares the numbers of branch @p index of @p run with its golden branch of @p golden and
/// the time series of that branch.
[[nodiscard]] StableBranchOutcome compareStableBranch(StableComparison& c, std::size_t index,
                                                      const GoldenFiles&   golden,
                                                      const SimulationRun& run)
{
    const json&        expected = golden.document.at("branches").at(index);
    const GoldenTable& table    = golden.tables[index];
    Mismatches         m(std::format("{} branch {}", c.context, index));
    StableBranch ours{.data   = run.data.get(),
                      .branch = &run.data->getBranch(index),
                      .table  = &table,
                      .plan   = stablePlan(expected, table, goldenRodClearance(golden.document)),
                      .extent = {}};
    ours.extent = stableExtent(ours.plan, *ours.branch);
    if (!ours.extent.problem.empty())
    {
        // Rule E does not excuse this other step from the apogee row: what follows it is not
        // compared, and the run does not match.
        m.note(std::format("row {}: {} (rule E)", ours.plan.apogeeRow.value_or(0),
                           ours.extent.problem));
    }
    compareStableBranchHeader(m, c, expected, ours);
    compareStableColumns(m, c, expected.at("columns"), ours);
    compareStableEvents(m, c, expected.at("events"), ours);
    c.report += m.report();
    return {.toTheEnd = ours.extent.whole, .tumbles = ours.plan.tumbles};
}

/// Compares the numbers of @p run with the golden files @p golden of its simulation in the
/// stable-step set: the number of jitter replacements, the summary values, what the warnings
/// print of their parameters, and per branch the numbers of its header, the minimum and
/// maximum of every column, the times of the events and the time series. (Everything that is
/// not a number of the trajectory is compared with the structure, compareSimulation().)
void compareStableNumbers(StableComparison& c, const GoldenFiles& golden, const SimulationRun& run)
{
    const json& document = golden.document;
    if (run.data == nullptr)
    {
        return;  // the structure reports a run without flight data
    }
    const json&       branches = document.at("branches");
    const std::size_t count =
        std::min({branches.size(), run.data->getBranchCount(), golden.tables.size()});
    bool                tumbles = false;
    StableBranchOutcome first{.toTheEnd = true, .tumbles = false};
    for (std::size_t i = 0; i < count; i++)
    {
        const StableBranchOutcome outcome = compareStableBranch(c, i, golden, run);
        tumbles                           = tumbles || outcome.tumbles;
        first                             = i == 0 ? outcome : first;
    }
    Mismatches summary(c.context + " summary");
    compareStableSummary(summary, c, golden, *run.data, first);
    c.report += summary.report();

    // The number of jitter replacements is that of the force calculations of the Runge-Kutta
    // steppers: not reproducible when a stage of the simulation tumbles under them (rule T).
    Mismatches result(c.context + " result");
    compareStableCount(result, c, "jitterReplacements",
                       document.at("result").at("jitterReplacements").get<std::int64_t>(),
                       run.jitterReplacements, !tumbles);
    c.report += result.report();

    Mismatches warnings(c.context);
    compareStableWarnings(warnings, c, document.at("warnings"), *run.data);
    c.report += warnings.report();
}

/// What the comparison of a run with its stable golden files compared and found.
struct StableResult
{
    SimulationCounts compared;
    SimulationCounts excluded;  ///< what the rules H, N, E and T exclude
    std::string      report;    ///< the mismatches, empty when everything matched
};

/// Compares @p run with the golden files @p golden of its simulation in the stable-step set:
/// the structure (everything that is not a number of the trajectory, as compareSimulation()
/// compares it in every simulation of the default-step set, exactly), then the numbers.
[[nodiscard]] StableResult compareStableSimulation(const GoldenFiles&   golden,
                                                   const SimulationRun& run,
                                                   const std::string&   context,
                                                   bool                 randomConfigurationId,
                                                   StableMeasurements*  measurements = nullptr)
{
    // The structure: compareSimulation() with nothing reproducible, so that it compares what
    // it compares in every simulation and no number of the trajectory.
    Sensitivity none;
    none.horizons.resize(run.data != nullptr ? run.data->getBranchCount() : 0);
    const SimulationComparison structure =
        compareSimulation(golden, run, none, context,
                          {.randomConfigurationId = randomConfigurationId, .stableSet = true});

    const QtRocket::Test::DefaultUnitsGuard units;  // the texts of the warnings print units
    StableComparison                        c;
    c.context      = context;
    c.measurements = measurements;
    compareStableNumbers(c, golden, run);

    StableResult result{
        .compared = c.compared, .excluded = c.excluded, .report = structure.report + c.report};
    result.compared.simulations = structure.compared.simulations;
    result.compared.branches    = structure.compared.branches;
    result.compared.events      = structure.compared.events;
    result.compared.columns     = structure.compared.columns;
    result.compared.warnings    = structure.compared.warnings;
    return result;
}

// ------------------------------------------------------------------------------------ the runs

/// A golden simulation of the stable-step set compared with QtRocket's run of it.
struct StableRun
{
    std::string      problem;  ///< why there is no comparison, else ""
    std::string      context;  ///< "<input>/stable/sim_<NN>_<name>"
    bool             randomConfigurationId{false};
    GoldenFiles      files;
    SimulationRun    run;
    SimulationCounts golden;
    StableResult     comparison;
};

/// Runs simulation @p index of the golden input @p input, the one of @p maker, with the stable
/// time step and compares it with its files of the stable-step set.
[[nodiscard]] StableRun runStable(const TestRocketMaker& maker, const GoldenInput& input,
                                  std::size_t index)
{
    StableRun               result;
    const GoldenSimulation& simulation = input.stableSimulations.at(index);
    result.files                       = loadGoldenFiles(simulation);
    if (!result.files.problem.empty())
    {
        result.problem = result.files.problem;
        return result;
    }
    result.golden = goldenCounts(result.files);
    result.run =
        runSimulation(maker, index, {.stableTimeStep = kStableTimeStep, .perturbation = nullptr});
    if (!result.run.problem.empty())
    {
        result.problem = result.run.problem;
        return result;
    }
    result.context = std::format("{}/stable/{}", input.name, baseName(result.run.planned));
    result.randomConfigurationId = maker.randomConfigurationId;
    Mismatches m(result.context);
    m.text("name in the manifest", simulation.name, result.run.planned.name);
    m.text("file in the manifest", simulation.json, result.context + ".json");
    result.comparison        = compareStableSimulation(result.files, result.run, result.context,
                                                       maker.randomConfigurationId);
    result.comparison.report = m.report() + result.comparison.report;
    return result;
}

/// runStable() of simulation @p index of @p maker, run once per test process and kept (as
/// goldenRun() of simulation_golden_tests.cpp keeps the runs of the default-step set).
[[nodiscard]] const StableRun& stableRun(const TestRocketMaker& maker, const GoldenInput& input,
                                         std::size_t index)
{
    static std::mutex                                    s_mutex;
    static std::map<std::string, StableRun, std::less<>> s_runs;
    const std::scoped_lock                               lock{s_mutex};
    const std::string key    = std::format("{}#{}", maker.input, index);
    auto              cached = s_runs.find(key);
    if (cached == s_runs.end())
    {
        cached = s_runs.emplace(key, runStable(maker, input, index)).first;
    }
    return cached->second;
}

/// What the plans of the branches of a golden simulation say, from the files alone: what the
/// rules exclude on every platform, and what they can exclude at most.
struct StablePlanCounts
{
    int          flights{0};          ///< the branches that leave the launch rod
    int          huntingBranches{0};  ///< the branches with a hunting row (rule H)
    std::int64_t rodRows{0};
    std::int64_t huntingRows{0};
    /// Rule H: the values of kHuntingNoiseColumns in hunting rows, which are not compared, and
    /// those of kHuntingColumns in hunting and band rows, compared at their hunting tolerances.
    std::int64_t huntingValues{0};
    std::int64_t looseValues{0};
    /// Rule H: the band rows; what is not compared in them alone (the switched columns, and
    /// kHuntingNoiseColumns in the band rows that do not hunt); and the rows whose hunting
    /// state is not what their lateral airspeed says.
    std::int64_t bandRows{0};
    std::int64_t bandValues{0};
    std::int64_t misjudgedRows{0};
    std::int64_t eulerRows{0};         ///< the rows of an Euler stepper in free flight
    std::int64_t noiseColumns{0};      ///< the noise-dominated out-of-plane columns (rule N)
    std::int64_t noiseValues{0};       ///< their values outside the launch rod rows
    int          tumblingBranches{0};  ///< the branches that tumble before their apogee (rule T)
    std::int64_t tumblingRows{0};      ///< their rows after the separation
    int          eulerApogees{0};      ///< the branches with an apogee row (rule E)
    int          lastBitApogees{0};    ///< of them: the golden rows show the last-bit decision
    std::int64_t apogeeRows{0};        ///< their rows from the apogee row on
    int          lateBranches{0};      ///< the branches with a late handling
    /// The floor: the rows, and the values, that are compared on every platform.
    std::int64_t floorRows{0};
    std::int64_t floorValues{0};

    [[nodiscard]] bool operator==(const StablePlanCounts&) const = default;

    StablePlanCounts& operator+=(const StablePlanCounts& other)
    {
        flights += other.flights;
        huntingBranches += other.huntingBranches;
        rodRows += other.rodRows;
        huntingRows += other.huntingRows;
        huntingValues += other.huntingValues;
        looseValues += other.looseValues;
        bandRows += other.bandRows;
        bandValues += other.bandValues;
        misjudgedRows += other.misjudgedRows;
        eulerRows += other.eulerRows;
        noiseColumns += other.noiseColumns;
        noiseValues += other.noiseValues;
        tumblingBranches += other.tumblingBranches;
        tumblingRows += other.tumblingRows;
        eulerApogees += other.eulerApogees;
        lastBitApogees += other.lastBitApogees;
        apogeeRows += other.apogeeRows;
        lateBranches += other.lateBranches;
        floorRows += other.floorRows;
        floorValues += other.floorValues;
        return *this;
    }
};

/// "31 flights, ...", for the messages of the tests.
std::ostream& operator<<(std::ostream& out, const StablePlanCounts& counts)
{
    return out << std::format(
               "{} flights, {} with hunting rows; {} rod rows, {} hunting rows with {} values not "
               "compared and {} at hunting tolerances; {} band rows with {} more values not "
               "compared, {} misjudged rows; {} Euler rows; {} noise columns with {} values off "
               "the rod; {} tumbling branches with {} rows after their separation; {} Euler "
               "apogees ({} by the last bit) with {} rows from them on; {} branches with a late "
               "handling; floor {} rows, {} values",
               counts.flights, counts.huntingBranches, counts.rodRows, counts.huntingRows,
               counts.huntingValues, counts.looseValues, counts.bandRows, counts.bandValues,
               counts.misjudgedRows, counts.eulerRows, counts.noiseColumns, counts.noiseValues,
               counts.tumblingBranches, counts.tumblingRows, counts.eulerApogees,
               counts.lastBitApogees, counts.apogeeRows, counts.lateBranches, counts.floorRows,
               counts.floorValues);
}

/// The number of values in the rows of @p plan that @p counted counts (by the column and the
/// kind of the value).
template <class Counted>
[[nodiscard]] std::int64_t valuesWhere(const StablePlan& plan, const Counted& counted)
{
    std::int64_t values = 0;
    for (const StableColumn& column : plan.columns)
    {
        for (std::size_t row = 0; row < plan.rows; row++)
        {
            values += counted(column, cellKind(plan, column, row)) ? 1 : 0;
        }
    }
    return values;
}

/// The counts of the plan @p plan of one branch.
[[nodiscard]] StablePlanCounts countsOf(const StablePlan& plan)
{
    StablePlanCounts counts;
    counts.flights         = plan.rodRows < plan.rows ? 1 : 0;
    counts.rodRows         = static_cast<std::int64_t>(plan.rodRows);
    counts.huntingRows     = countOf(plan.hunting);
    counts.huntingBranches = counts.huntingRows > 0 ? 1 : 0;
    counts.huntingValues   = valuesWhere(plan, [](const StableColumn& /*column*/, CellKind kind) {
        return kind == CellKind::HUNTING_NOISE;
    });
    counts.looseValues     = valuesWhere(plan, [](const StableColumn& column, CellKind kind) {
        return kind == CellKind::HUNTING && column.hunting > column.tolerance;
    });
    counts.bandRows        = countOf(plan.band);
    counts.bandValues      = valuesWhere(
        plan, [](const StableColumn& /*column*/, CellKind kind) { return kind == CellKind::BAND; });
    counts.misjudgedRows = static_cast<std::int64_t>(plan.misjudgedRows);
    counts.eulerRows     = countOf(plan.euler);
    counts.noiseColumns =
        std::ranges::count(plan.columns, ColumnKind::ON_THE_ROD_ONLY, &StableColumn::kind);
    counts.noiseValues      = valuesWhere(plan, [](const StableColumn& /*column*/, CellKind kind) {
        return kind == CellKind::NOISE;
    });
    counts.tumblingBranches = plan.tumbles ? 1 : 0;
    counts.tumblingRows     = static_cast<std::int64_t>(plan.rows - plan.flownRows);
    counts.eulerApogees     = plan.apogeeRow.has_value() ? 1 : 0;
    counts.lastBitApogees   = plan.apogeeRow.has_value() && plan.apogeeProblem.empty() ? 1 : 0;
    counts.apogeeRows       = static_cast<std::int64_t>(plan.flownRows - floorRows(plan));
    counts.lateBranches     = std::isfinite(plan.lateFrom) ? 1 : 0;
    counts.floorRows        = static_cast<std::int64_t>(floorRows(plan));
    counts.floorValues      = comparedValues(plan, floorRows(plan));
    return counts;
}

/// The counts of the plans of the branches of the golden simulation @p files.
[[nodiscard]] StablePlanCounts planCounts(const GoldenFiles& files)
{
    StablePlanCounts            counts;
    const std::optional<double> cleared  = goldenRodClearance(files.document);
    const json&                 branches = files.document.at("branches");
    for (std::size_t i = 0; i < std::min(branches.size(), files.tables.size()); i++)
    {
        counts += countsOf(stablePlan(branches.at(i), files.tables[i], cleared));
    }
    return counts;
}

/// What the stable simulations of one golden input hold, and what their comparison compared
/// and found.
struct StableInputResult
{
    std::string      problems;  ///< a simulation that could not be run
    SimulationCounts golden;
    SimulationCounts compared;
    SimulationCounts excluded;
    std::string      report;
    StablePlanCounts plans;
};

/// Runs and compares every simulation of the stable-step set of the golden input of @p maker.
[[nodiscard]] StableInputResult compareStableInput(const TestRocketMaker& maker)
{
    StableInputResult  result;
    const GoldenInput* input = inputOf(maker);
    if (input == nullptr)
    {
        result.problems = std::format("no golden input named {}", maker.input);
        return result;
    }
    const std::unique_ptr<Rocket> rocket  = maker.make();
    const std::size_t             planned = plannedSimulations(*rocket, maker.input).size();
    if (planned != input->stableSimulations.size())
    {
        result.problems += std::format(
            "{}: the harness's document has {} simulations, the "
            "manifest lists {} in the stable-step set\n",
            maker.input, planned, input->stableSimulations.size());
    }
    for (std::size_t i = 0; i < input->stableSimulations.size(); i++)
    {
        const StableRun& run = stableRun(maker, *input, i);
        if (!run.problem.empty())
        {
            result.problems +=
                std::format("{}: {}\n", input->stableSimulations[i].json, run.problem);
            continue;
        }
        result.golden += run.golden;
        result.compared += run.comparison.compared;
        result.excluded += run.comparison.excluded;
        result.report += run.comparison.report;
        result.plans += planCounts(run.files);
    }
    return result;
}

// ====================================================================================== tests

/// One test rocket of TestRockets.h: its golden simulations of the stable-step set.
class SimulationStableGolden : public ::testing::TestWithParam<TestRocketMaker>
{ };

// Every simulation of the rocket is compared over its whole flight: what was compared and
// what the rules exclude add up to what the files hold. And the comparison has a floor, which
// the files alone decide: the rows of every branch up to its apogee row (rule E) or to its
// separation (rule T), and in them every value the rules H and N leave.
TEST_P(SimulationStableGolden, EverySimulationOfTheRocketOverItsWholeFlight)
{
    const StableInputResult result = compareStableInput(GetParam());
    ASSERT_EQ(result.problems, "");
    EXPECT_EQ(result.report, "");
    // Everything the files hold was compared or is excluded by a rule: nothing skipped.
    EXPECT_EQ(result.compared + result.excluded, result.golden)
        << "what was compared or is excluded, and what the files hold";
    EXPECT_GT(result.golden.simulations, 0);
    // The structure is compared in every simulation.
    EXPECT_EQ(result.excluded.simulations + result.excluded.branches + result.excluded.events +
                  result.excluded.columns + result.excluded.warnings,
              0);
    EXPECT_GE(result.compared.rows, result.plans.floorRows);
    EXPECT_GE(result.compared.values, result.plans.floorValues);
}

INSTANTIATE_TEST_SUITE_P(Makers, SimulationStableGolden, ::testing::ValuesIn(testRocketMakers()),
                         makerTestName);

/// What the simulations of the stable-step set of the test rockets hold and what their plans
/// say, read from the files alone (no simulation is run).
struct StableCoverage
{
    int              goldenInputs{0};  ///< the "testrocket" inputs of the manifest
    SimulationCounts golden;           ///< what their stable simulation files hold
    StablePlanCounts plans;
    std::string      problems;  ///< an input without a maker, a file that cannot be read
};

/// Reads every simulation of the stable-step set of every "testrocket" input of the manifest.
[[nodiscard]] StableCoverage stableCoverage()
{
    StableCoverage coverage;
    if (!manifest())
    {
        coverage.problems = manifest().error().message;
        return coverage;
    }
    const std::span<const TestRocketMaker> makers = testRocketMakers();
    for (const GoldenInput& input : manifest()->inputs)
    {
        if (input.kind != "testrocket")
        {
            continue;
        }
        coverage.goldenInputs++;
        if (std::ranges::find(makers, std::string_view{input.name}, &TestRocketMaker::input) ==
            makers.end())
        {
            coverage.problems += std::format("{}: no maker\n", input.name);
        }
        for (const GoldenSimulation& simulation : input.stableSimulations)
        {
            const GoldenFiles files = loadGoldenFiles(simulation);
            if (!files.problem.empty())
            {
                coverage.problems += std::format("{}: {}\n", simulation.json, files.problem);
                continue;
            }
            coverage.golden += goldenCounts(files);
            coverage.plans += planCounts(files);
        }
    }
    return coverage;
}

/// Every golden simulation of the stable-step set is compared: each of the thirteen inputs has
/// a maker, whose test (SimulationStableGolden) checks that what it compared and what the
/// rules exclude add up to what the files of its input hold, and that the floor holds. This
/// test pins what the files hold in all and what the rules decide from the files alone; it
/// runs no simulation.
TEST(SimulationStableGoldenCoverage, EverySimulationOfTheStableStepSetHasAMaker)
{
    const StableCoverage coverage = stableCoverage();
    EXPECT_EQ(coverage.problems, "");
    EXPECT_EQ(coverage.goldenInputs, 13);
    ASSERT_TRUE(manifest().has_value());
    EXPECT_EQ(manifest()->stableTimeStep, kStableTimeStep);
    // The same simulations, branches, events, columns and warnings as in the default-step set
    // (SimulationGoldenCoverage), in 35323 rows where that has 22330.
    EXPECT_EQ(coverage.golden, (SimulationCounts{.simulations = 50,
                                                 .branches    = 53,
                                                 .events      = 437,
                                                 .columns     = 2826,
                                                 .warnings    = 21,
                                                 .numbers     = 6920,
                                                 .rows        = 35323,
                                                 .values      = 2471514}));
    // What the rules decide from the files alone: 31 flights and 3 dropped stages leave the
    // launch rod, and 30 of these 34 branches hunt (rule H: 5552 rows, in which the six columns
    // that are noise there hold 33312 values; the twelve attitude columns with a hunting
    // tolerance hold 69252 values in these and the band rows; 517 band rows, in which 2359
    // more values are not compared; no row whose hunting state is not what its lateral
    // airspeed says); 13615 rows are an Euler stepper's; 167 out-of-plane columns are noise
    // (rule N); 2 stages tumble before their apogee (rule T: 542 rows after their
    // separations); 19 other branches pass their apogee under an Euler stepper, each by the
    // last bit (rule E: 7919 rows from their apogee rows on, which are compared when the run
    // steps from the apogee as the golden run does); 3 branches have a late handling. The
    // floor, what is compared on every platform: 26862 of the 35323 rows and 1732140 of the
    // 2471514 values.
    EXPECT_EQ(coverage.plans, (StablePlanCounts{.flights          = 34,
                                                .huntingBranches  = 30,
                                                .rodRows          = 3620,
                                                .huntingRows      = 5552,
                                                .huntingValues    = 33312,
                                                .looseValues      = 69252,
                                                .bandRows         = 517,
                                                .bandValues       = 2359,
                                                .misjudgedRows    = 0,
                                                .eulerRows        = 13615,
                                                .noiseColumns     = 167,
                                                .noiseValues      = 153992,
                                                .tumblingBranches = 2,
                                                .tumblingRows     = 542,
                                                .eulerApogees     = 19,
                                                .lastBitApogees   = 19,
                                                .apogeeRows       = 7919,
                                                .lateBranches     = 3,
                                                .floorRows        = 26862,
                                                .floorValues      = 1732140}))
        << coverage.plans;
}

// =============================================================================== measurement

/// "branch 1: 237 of 631 rows (rule T); " for every branch of @p run that is not compared to
/// its last row.
[[nodiscard]] std::string extentText(const StableRun& run)
{
    std::string                 text;
    const std::optional<double> cleared  = goldenRodClearance(run.files.document);
    const json&                 branches = run.files.document.at("branches");
    for (std::size_t i = 0;
         run.run.data != nullptr &&
         i < std::min({branches.size(), run.files.tables.size(), run.run.data->getBranchCount()});
         i++)
    {
        const StablePlan   plan   = stablePlan(branches.at(i), run.files.tables[i], cleared);
        const StableExtent extent = stableExtent(plan, run.run.data->getBranch(i));
        if (!extent.whole)
        {
            text += std::format(" branch {}: {} of {} rows (rule {}); the run has {} rows;", i,
                                extent.rows, plan.rows, extent.rows < plan.flownRows ? "E" : "T",
                                run.run.data->getBranch(i).getLength());
        }
    }
    return text;
}

/// The measurement behind the tolerances of the stable-step set: prints the branches that are
/// not compared to their last row on this platform, and a table of the largest differences
/// from the golden files by what was compared (a column in its rows on the launch rod, in the
/// Runge-Kutta rows in free flight, in the hunting and band rows and in the rows of an Euler
/// stepper; a minimum or maximum; a summary value; the times of the events of a type; ...)
/// and, for information, by what the rules H and N exclude, with how the run decides the
/// hunting (measureHuntingDecision()) and what its steps to the apogee leave
/// (measureApogeeStep()): the number of values, the largest difference (of a column: as a
/// fraction of its scale; of a time: in s), that difference as a multiple of its tolerance,
/// and where it is. Disabled; run it with --gtest_also_run_disabled_tests (under a libm of
/// another platform, or the one-ulp shim, to see what the tolerances have to cover there).
TEST(SimulationStableGoldenMeasurement, DISABLED_PrintsTheDifferencesFromTheGoldenFiles)
{
    StableMeasurements measurements;
    std::string        extents;
    SimulationCounts   compared;
    SimulationCounts   excluded;
    for (const TestRocketMaker& maker : testRocketMakers())
    {
        const GoldenInput* input = inputOf(maker);
        ASSERT_NE(input, nullptr) << maker;
        for (std::size_t i = 0; i < input->stableSimulations.size(); i++)
        {
            const StableRun& run = stableRun(maker, *input, i);
            ASSERT_EQ(run.problem, "");
            const StableResult result = compareStableSimulation(
                run.files, run.run, run.context, run.randomConfigurationId, &measurements);
            compared += result.compared;
            excluded += result.excluded;
            const std::string extent = extentText(run);
            extents += extent.empty() ? std::string{} : std::format("{}:{}\n", run.context, extent);
        }
    }
    std::cout << "Not compared to the last row:\n"
              << extents << "compared: " << compared << "\nexcluded: " << excluded
              << "\nwhat\tvalues\tlargest difference\t... as a multiple of its tolerance\twhere\n"
              << measurements.text()
              << std::format(
                     "The largest difference of a compared value is {:.3e} of its "
                     "tolerance.\n",
                     measurements.largestOfTolerance());
    EXPECT_LE(measurements.largestOfTolerance(), 1.0);
}

/// The listener of a kicked run: after every step it changes each component of the velocity
/// and of the rotation velocity of the rocket by a random fraction of itself of up to the
/// size it was made with: a perturbation of some hundred thousand ulps, where the libm of
/// another platform moves a result by one. A system listener, as the LastBitListener is. The
/// random numbers are those of a linear congruential generator, the same on every platform,
/// and the clones of the listener (the simulation runs on clones) draw from the one sequence.
class KickListener final : public QtRocket::CloneableSimulationListener<KickListener>
{
public:
    KickListener(double size, std::uint64_t seed)
      : m_size(size), m_state(std::make_shared<std::uint64_t>(seed))
    {
    }

    [[nodiscard]] bool isSystemListener() const override { return true; }

    void postStep(SimulationStatus& status) override
    {
        const Coordinate& velocity = status.getRocketVelocity();
        status.setRocketVelocity(Coordinate{kicked(velocity.x), kicked(velocity.y),
                                            kicked(velocity.z), velocity.weight});
        const Coordinate& rotation = status.getRocketRotationVelocity();
        status.setRocketRotationVelocity(Coordinate{kicked(rotation.x), kicked(rotation.y),
                                                    kicked(rotation.z), rotation.weight});
    }

private:
    /// @p value changed by a fraction of itself, uniform in [-m_size, m_size).
    [[nodiscard]] double kicked(double value) const
    {
        constexpr std::uint64_t kMultiplier = 6364136223846793005ULL;  // Knuth's MMIX generator
        constexpr std::uint64_t kIncrement  = 1442695040888963407ULL;
        constexpr double        kTwoTo53    = 9007199254740992.0;
        *m_state                            = (*m_state * kMultiplier) + kIncrement;
        const double unit = (static_cast<double>(*m_state >> 11U) / kTwoTo53 * 2.0) - 1.0;
        return value * (1.0 + (m_size * unit));
    }

    double                         m_size;
    std::shared_ptr<std::uint64_t> m_state;
};

/// The golden form of the number @p value: itself, or the string of a value JSON cannot hold.
[[nodiscard]] json numberJson(double value)
{
    if (std::isnan(value))
    {
        return "NaN";
    }
    if (std::isinf(value))
    {
        return value > 0 ? "Infinity" : "-Infinity";
    }
    return value;
}

/// Branch @p index of @p run in the form of a golden branch, as far as the comparison of the
/// numbers reads it, with its time series in @p table.
[[nodiscard]] json goldenBranchOf(const SimulationRun& run, std::size_t index, GoldenTable& table)
{
    const FlightDataBranch& branch = run.data->getBranch(index);
    json                    form   = json::object();
    form["rows"]                   = branch.getLength();
    form["optimumAltitude"]        = numberJson(branch.getOptimumAltitude());
    form["timeToOptimumAltitude"]  = numberJson(branch.getTimeToOptimumAltitude());
    form["optimumDelay"]           = numberJson(branch.getOptimumDelay());
    form["separationTime"]         = numberJson(branch.getSeparationTime());
    form["columns"]                = json::array();
    table                          = timeSeriesOf(branch);
    for (const FlightDataType* type : csvTypes(branch))
    {
        form["columns"].push_back({{"min", numberJson(branch.getMinimum(*type))},
                                   {"max", numberJson(branch.getMaximum(*type))}});
    }
    form["events"] = json::array();
    for (const FlightEvent& event : branch.getEvents())
    {
        const Warning* warning = run.data->findWarning(event);
        form["events"].push_back(
            {{"time", numberJson(event.getTime())},
             {"type", std::string{name(event.getType())}},
             {"source",
              event.getSource() == nullptr ? json(nullptr) : json(pathOrNull(event.getSource()))},
             {"data", warning != nullptr ? goldenFormOf(*warning) : json(nullptr)}});
    }
    return form;
}

/// @p run in the form of the golden files of a simulation, as far as the comparison of the
/// numbers (compareStableNumbers()) reads them: what a second run is compared with to see by
/// how much a perturbation moves each number.
[[nodiscard]] GoldenFiles goldenFilesOf(const SimulationRun& run)
{
    GoldenFiles files;
    files.document["result"]   = {{"jitterReplacements", run.jitterReplacements}};
    files.document["summary"]  = json::object();
    files.document["warnings"] = json::array();
    files.document["branches"] = json::array();
    if (run.data == nullptr)
    {
        return files;
    }
    for (const StableSummaryValue& value : kStableSummaryValues)
    {
        files.document["summary"][std::string{value.key}] = numberJson((*run.data.*value.get)());
    }
    for (const Warning& warning : run.data->getWarningSet())
    {
        files.document["warnings"].push_back(goldenFormOf(warning));
    }
    files.tables.resize(run.data->getBranchCount());
    for (std::size_t i = 0; i < run.data->getBranchCount(); i++)
    {
        files.document["branches"].push_back(goldenBranchOf(run, i, files.tables[i]));
    }
    return files;
}

/// The size of the kick of the sensitivity measurement: a relative 1e-10 per step.
constexpr double kKickSize = 1e-10;

/// The sensitivity of what the stable-step set compares: every simulation is run once more
/// with a KickListener (a relative 1e-10 kick to the velocity and to the rotation velocity
/// after every step, about a million times what one ulp of a libm result is) and compared with
/// the unperturbed run under the rules and at the tolerances of the comparison with the golden
/// files. Prints the table of SimulationStableGoldenMeasurement for the two runs: by how much
/// the kick moves each column, summary value and event time. The kick is a physical
/// perturbation, which the trajectory follows in proportion (an altitude moves by some 1e-7 of
/// its scale); what it shows is that nothing compared moves out of proportion: no row count,
/// no event sequence and no event time jumps where the rules do not say so. Disabled; run it
/// with --gtest_also_run_disabled_tests.
TEST(SimulationStableGoldenMeasurement, DISABLED_PrintsTheSensitivityToAKick)
{
    StableMeasurements measurements;
    std::string        report;
    std::uint64_t      seed = 1;
    for (const TestRocketMaker& maker : testRocketMakers())
    {
        const GoldenInput* input = inputOf(maker);
        ASSERT_NE(input, nullptr) << maker;
        for (std::size_t i = 0; i < input->stableSimulations.size(); i++)
        {
            const StableRun& run = stableRun(maker, *input, i);
            ASSERT_EQ(run.problem, "");
            const SimulationRun kicked =
                runSimulation(maker, i,
                              {.stableTimeStep = kStableTimeStep,
                               .perturbation = std::make_shared<KickListener>(kKickSize, seed++)});
            ASSERT_EQ(kicked.problem, "");
            StableComparison c;
            c.context      = run.context;
            c.measurements = &measurements;
            compareStableNumbers(c, goldenFilesOf(run.run), kicked);
            report += c.report;
        }
    }
    std::cout << "what\tvalues\tlargest difference\t... as a multiple of its tolerance\twhere\n"
              << measurements.text() << "Beyond the tolerances of the comparison:\n"
              << report;
}

// ================================================================================= mutations

// The comparison of the stable-step set is not vacuous either: a golden value changed in a
// copy of the files of a simulation is reported, in one line that names it, wherever in the
// flight it is. The simulations: the [A8-0; None] one of the Beta, which ends on the launch pad
// after one step; the [C6-5] flight of the Estes Alpha III, which reaches its apogee under a
// Runge-Kutta stepper and is compared to its last row on every platform (1334 rows; the apogee
// at 6.01 s, the ground hit at 89.6 s); the [A8-0] flight of the Estes Alpha III with pods,
// whose warning prints a speed; and the two-stage flight of the Beta, whose booster tumbles.

/// Four times a tolerance of the stable-step set, and a quarter of it.
constexpr double kBeyond = 4.0;
constexpr double kWithin = 0.25;

constexpr Subject kStableOnThePad{.input = "testrocket-beta", .index = 1};
constexpr Subject kStableFlight{.input = "testrocket-estes-alpha-iii", .index = 4};
constexpr Subject kStableWithASpeed{.input = "testrocket-estes-alpha-iii-with-pods", .index = 1};
constexpr Subject kStableTwoStages{.input = "testrocket-beta", .index = 2};

/// Rows of the [C6-5] flight: one late in it (under the parachute, 60 s after the launch); one
/// of its coast; one in the middle of its hunting (1.8 s; the hunting rows are 216 to 458);
/// and the second row after the hunting, a band row that does not hunt (the lateral airspeed
/// has risen to 1.02 mm/s).
constexpr std::size_t kLateRow    = 1200;
constexpr std::size_t kCoastRow   = 600;
constexpr std::size_t kHuntingRow = 300;
constexpr std::size_t kBandRow    = 460;

/// The changes to the settings of a stable simulation and to how it ended.
[[nodiscard]] std::vector<Mutation> stableSettingMutations()
{
    return {
        {.name    = "TimeStep",
         .subject = kStableFlight,
         .apply   = [](GoldenFiles& files) { scale(files, "/options/timeStep", 1e-15); },
         .heading = " options",
         .line    = "  timeStep: expected "},
        {.name    = "TheDefaultTimeStep",
         .subject = kStableFlight,
         .apply   = [](GoldenFiles& files) { files.document["options"]["timeStep"] = 0.05; },
         .heading = " options",
         .line    = "  timeStep: expected 0.05"},
        {.name    = "HarnessTimeStep",
         .subject = kStableFlight,
         .apply   = [](GoldenFiles& files) { files.document["harness"]["timeStep"] = 0.02; },
         .heading = " harness",
         .line    = R"(  timeStep: expected "0.02", got "0.01")"},
        {.name    = "HarnessDocumentTimeStep",
         .subject = kStableFlight,
         .apply = [](GoldenFiles& files) { files.document["harness"]["documentTimeStep"] = 0.01; },
         .heading = " harness",
         .line    = R"(  documentTimeStep: expected "0.01", got "0.05")"},
        {.name    = "HarnessWithoutTheTimeStep",
         .subject = kStableFlight,
         .apply   = [](GoldenFiles& files) { files.document["harness"].erase("timeStep"); },
         .heading = " harness",
         .line    = R"(  timeStep: expected "nothing", got "0.01")"},
        {.name    = "Status",
         .subject = kStableFlight,
         .apply   = [](GoldenFiles& files) { files.document["result"]["status"] = "exception"; },
         .heading = " result",
         .line    = R"(  status: expected "exception", got "completed")"},
        {.name    = "JitterReplacements",
         .subject = kStableFlight,
         .apply = [](GoldenFiles& files) { files.document["result"]["jitterReplacements"] = 3345; },
         .heading = " result",
         .line    = "  jitterReplacements: expected 3345, got 3344"},
        {.name    = "JitterReplacementsOnThePad",
         .subject = kStableOnThePad,
         .apply   = [](GoldenFiles& files) { files.document["result"]["jitterReplacements"] = 5; },
         .heading = " result",
         .line    = "  jitterReplacements: expected 5, got 4"},
    };
}

/// The changes to the summary, to a warning and to the header of a branch of a stable
/// simulation.
[[nodiscard]] std::vector<Mutation> stableNumberMutations()
{
    return {
        {.name    = "MaximumAltitude",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/summary/maxAltitude", kBeyond * kStableSummaryRelative);
             },
         .heading = " summary",
         .line    = "  maxAltitude: expected "},
        {.name    = "MaximumVelocity",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/summary/maxVelocity", kBeyond * kStableSummaryRelative);
             },
         .heading = " summary",
         .line    = "  maxVelocity: expected "},
        {.name    = "TimeToApogee",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/summary/timeToApogee", kBeyond * kStableTimeAbsolute);
             },
         .heading = " summary",
         .line    = "  timeToApogee: expected "},
        {.name    = "FlightTime",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/summary/flightTime", kBeyond * kStableTimeAbsolute);
             },
         .heading = " summary",
         .line    = "  flightTime: expected "},
        {.name    = "GroundHitVelocity",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/summary/groundHitVelocity", kBeyond * kStableSummaryRelative);
             },
         .heading = " summary",
         .line    = "  groundHitVelocity: expected "},
        {.name    = "MaximumMachNumberOnThePad",
         .subject = kStableOnThePad,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/summary/maxMachNumber", kBeyond * kStableSummaryRelative);
             },
         .heading = " summary",
         .line    = "  maxMachNumber: expected "},
        {.name    = "BranchCount",
         .subject = kStableFlight,
         .apply   = [](GoldenFiles& files) { files.document["summary"]["branchCount"] = 2; },
         .heading = " summary",
         .line    = "  branchCount: expected 2, got 1"},
        {.name    = "SpeedOfAWarning",
         .subject = kStableWithASpeed,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/warnings/0/parameter", kBeyond * kStableSummaryRelative);
             },
         .heading = "",
         .line    = "  warnings[0].parameter: expected "},
        {.name    = "TextOfAWarningWithASpeed",
         .subject = kStableWithASpeed,
         .apply =
             [](GoldenFiles& files) {
                 files.document["warnings"][0]["text"] = "Recovery device deployment at 80.6 m/s";
             },
         .heading = "",
         .line    = R"(  warnings[0].text: expected "Recovery device deployment at 80.6 m/s")"},
        {.name    = "SpeedOfTheWarningOfAnEvent",
         .subject = kStableWithASpeed,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/branches/0/events/6/data/parameter",
                       kBeyond * kStableSummaryRelative);
             },
         .heading = " branch 0",
         .line    = "  events[6].data.parameter: expected "},
        {.name    = "RowCount",
         .subject = kStableFlight,
         .apply   = [](GoldenFiles& files) { files.document["branches"][0]["rows"] = 1335; },
         .heading = " branch 0",
         .line    = "  rows: expected 1335, got 1334"},
        {.name    = "RowCountOnThePad",
         .subject = kStableOnThePad,
         .apply   = [](GoldenFiles& files) { files.document["branches"][0]["rows"] = 3; },
         .heading = " branch 0",
         .line    = "  rows: expected 3, got 2"},
        {.name    = "OptimumAltitude",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/branches/0/optimumAltitude", kBeyond * kStableSummaryRelative);
             },
         .heading = " branch 0",
         .line    = "  optimumAltitude: expected "},
        {.name    = "OptimumDelay",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/optimumDelay", kBeyond * kStableTimeAbsolute);
             },
         .heading = " branch 0",
         .line    = "  optimumDelay: expected "},
        {.name    = "BranchName",
         .subject = kStableFlight,
         .apply   = [](GoldenFiles& files) { files.document["branches"][0]["name"] = "Booster"; },
         .heading = " branch 0",
         .line    = R"(  name: expected "Booster", got "Stage")"},
        {.name    = "ColumnSymbol",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["columns"][1]["symbol"] = "H";
             },
         .heading = " branch 0",
         .line    = R"(  columns[1].symbol: expected "H", got "h")"},
        {.name    = "MaximumOfTheAltitude",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/branches/0/columns/1/max", kBeyond * stableTolerance("altitude"));
             },
         .heading = " branch 0",
         .line    = "  columns[1].max (altitude): expected "},
        {.name    = "MaximumOfTheTime",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/branches/0/columns/0/max", kBeyond * stableTolerance("time"));
             },
         .heading = " branch 0",
         .line    = "  columns[0].max (time): expected "},
    };
}

/// The changes to the events and to the time series of a stable simulation, late in its
/// flight.
[[nodiscard]] std::vector<Mutation> stableTrajectoryMutations()
{
    return {
        {.name    = "AltitudeUnderTheParachute",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 shiftSeries(files, "altitude", kLateRow, kBeyond * stableTolerance("altitude"));
             },
         .heading = " branch 0",
         .line    = "  column altitude: 1 of 1334 rows differ, the first at row 1200: "},
        {.name    = "TimeOfARecordUnderTheParachute",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 shiftSeries(files, "time", kLateRow, kBeyond * stableTolerance("time"));
             },
         .heading = " branch 0",
         .line    = "  column time: 1 of 1334 rows differ, the first at row 1200: "},
        {.name    = "AngleOfAttackOfTheCoast",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 shiftSeries(files, "aoa", kCoastRow, kBeyond * stableTolerance("aoa"));
             },
         .heading = " branch 0",
         .line    = "  column aoa: 1 of 1334 rows differ, the first at row 600: "},
        {.name    = "LastRecord",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 shiftSeries(files, "velocity_z", files.tables.at(0).rows.size() - 1,
                             kBeyond * eulerTolerance("velocity_z"));
             },
         .heading = " branch 0",
         .line    = "  column velocity_z: 1 of 1334 rows differ, the first at row 1333: "},
        {.name    = "DragCoefficientUnderTheParachute",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 shiftSeries(files, "drag_coeff", kLateRow, kBeyond * eulerTolerance("drag_coeff"));
             },
         .heading = " branch 0",
         .line    = "  column drag_coeff: 1 of 1334 rows differ, the first at row 1200: "},
        {.name    = "MassUnderTheParachute",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 shiftSeries(files, "mass", kLateRow, kBeyond * eulerTolerance("mass"));
             },
         .heading = " branch 0",
         .line    = "  column mass: 1 of 1334 rows differ, the first at row 1200: "},
        {.name    = "PitchAngleWhileHunting",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 shiftSeries(files, "orientation_theta", kHuntingRow,
                             kBeyond * huntingTolerance("orientation_theta"));
             },
         .heading = " branch 0",
         .line    = "  column orientation_theta: 1 of 1334 rows differ, the first at row 300: "},
        {.name    = "PitchRateWhileHunting",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 // Zero in a hunting row: any other value says the run did not hunt there.
                 shiftSeries(files, "pitch_rate", kHuntingRow,
                             kBeyond * stableTolerance("pitch_rate"));
             },
         .heading = " branch 0",
         .line    = "  column pitch_rate: 1 of 1334 rows differ, the first at row 300: "},
        {.name    = "TimeOfTheApogee",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/events/5/time", kBeyond * kStableTimeAbsolute);
             },
         .heading = " branch 0",
         .line    = "  events[5].time: expected "},
        {.name    = "TimeOfTheEjectionCharge",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/events/6/time", -kBeyond * kStableTimeAbsolute);
             },
         .heading = " branch 0",
         .line    = "  events[6].time: expected "},
        {.name    = "TimeOfTheGroundHit",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/events/8/time", -kBeyond * kStableTimeAbsolute);
             },
         .heading = " branch 0",
         .line    = "  events[8].time: expected "},
        {.name    = "RemovedEvent",
         .subject = kStableFlight,
         .apply   = [](GoldenFiles& files) { files.document["branches"][0]["events"].erase(5); },
         .heading = " branch 0",
         .line    = R"(  events: expected "LAUNCH(/) IGNITION(/0/1/2) LIFTOFF(null) )"
                    "LAUNCHROD(null) BURNOUT(/0/1/2) EJECTION_CHARGE(/0) "},
        {.name    = "SwappedEvents",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 json& events = files.document["branches"][0]["events"];
                 std::swap(events[5], events[6]);
             },
         .heading = " branch 0",
         .line    = R"(  events: expected "LAUNCH(/) IGNITION(/0/1/2) LIFTOFF(null) )"
                    "LAUNCHROD(null) BURNOUT(/0/1/2) EJECTION_CHARGE(/0) APOGEE(/) "},
        {.name    = "MotorOfAnEvent",
         .subject = kStableFlight,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["events"][4]["data"]["designation"] = "C7";
             },
         .heading = " branch 0",
         .line    = R"(  events[4].data.designation: expected "C7", got "C6")"},
        {.name    = "TimeOfTheSeparation",
         .subject = kStableTwoStages,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/separationTime", kBeyond * kStableTimeAbsolute);
             },
         .heading = " branch 0",
         .line    = "  separationTime: expected "},
        {.name    = "AltitudeOfTheBoosterBeforeItsSeparation",
         .subject = kStableTwoStages,
         .apply =
             [](GoldenFiles& files) {
                 GoldenTable& booster = files.tables.at(1);
                 booster.rows.at(200).at(1) +=
                     kBeyond * stableTolerance("altitude") * columnScale(booster, 1);
             },
         .heading = " branch 1",
         .line    = "  column altitude: 1 of 317 rows differ, the first at row 200: "},
    };
}

/// Every change to a simulation of the stable-step set.
[[nodiscard]] std::vector<Mutation> stableMutations()
{
    std::vector<Mutation> all = stableSettingMutations();
    for (const Mutation& mutation : stableNumberMutations())
    {
        all.push_back(mutation);
    }
    for (const Mutation& mutation : stableTrajectoryMutations())
    {
        all.push_back(mutation);
    }
    return all;
}

/// The stable run of the simulation @p subject, or null (with a test failure) when there is
/// none.
[[nodiscard]] const StableRun* stableSubjectRun(const Subject& subject)
{
    const std::span<const TestRocketMaker> makers = testRocketMakers();
    const auto maker = std::ranges::find(makers, subject.input, &TestRocketMaker::input);
    if (maker == makers.end() || inputOf(*maker) == nullptr)
    {
        ADD_FAILURE() << "no golden input or maker named " << subject.input;
        return nullptr;
    }
    const StableRun& run = stableRun(*maker, *inputOf(*maker), subject.index);
    if (!run.problem.empty())
    {
        ADD_FAILURE() << run.problem;
        return nullptr;
    }
    return &run;
}

/// What the comparison of the stable simulation @p subject with a copy of its golden files
/// finds after @p change changed the copy (null: nothing changed).
[[nodiscard]] StableResult stableComparisonAfter(const Subject& subject,
                                                 void (*change)(GoldenFiles& files))
{
    const StableRun* run = stableSubjectRun(subject);
    if (run == nullptr)
    {
        StableResult none;
        none.report = "no run";
        return none;
    }
    GoldenFiles files = run->files;
    if (change != nullptr)
    {
        change(files);
    }
    return compareStableSimulation(files, run->run, run->context, run->randomConfigurationId);
}

/// The stable mutations of the simulation @p subject.
[[nodiscard]] std::vector<Mutation> stableMutationsOf(const Subject& subject)
{
    std::vector<Mutation> own;
    for (const Mutation& mutation : stableMutations())
    {
        if (mutation.subject.input == subject.input && mutation.subject.index == subject.index)
        {
            own.push_back(mutation);
        }
    }
    return own;
}

/// What is wrong with the reports of the stable mutations of the simulation @p subject, each
/// named; "" when every one is reported in the heading and the one line that name the change.
[[nodiscard]] std::string stableMutationProblems(const Subject& subject)
{
    const StableRun* run = stableSubjectRun(subject);
    if (run == nullptr)
    {
        return "no run";
    }
    std::string problems;
    for (const Mutation& mutation : stableMutationsOf(subject))
    {
        const std::string report = stableComparisonAfter(mutation.subject, mutation.apply).report;
        const std::string start =
            std::format("{}{}:\n{}", run->context, mutation.heading, mutation.line);
        if (!report.starts_with(start))
        {
            problems += std::format("{}: the report\n{}\ndoes not start with\n{}\n", mutation.name,
                                    report, start);
        }
        else if (std::ranges::count(report, '\n') != 2)
        {
            problems += std::format("{}: the report is not one line:\n{}\n", mutation.name, report);
        }
    }
    return problems;
}

/// Changes within the tolerances to the [C6-5] flight of the stable-step set: values and
/// times late in the flight, the times of events, summary values, a maximum.
void changeTheStableFlightWithinTheTolerances(GoldenFiles& files)
{
    shiftSeries(files, "altitude", kLateRow, kWithin * stableTolerance("altitude"));
    shiftSeries(files, "time", kLateRow, kWithin * stableTolerance("time"));
    shiftSeries(files, "aoa", kCoastRow, kWithin * stableTolerance("aoa"));
    shiftSeries(files, "orientation_theta", kHuntingRow,
                kWithin * huntingTolerance("orientation_theta"));
    shiftSeries(files, "drag_coeff", kLateRow, kWithin * eulerTolerance("drag_coeff"));
    shiftSeries(files, "velocity_z", kLateRow, kWithin * eulerTolerance("velocity_z"));
    scale(files, "/summary/groundHitVelocity", kWithin * kStableSummaryRelative);
    shift(files, "/branches/0/events/5/time", kWithin * kStableTimeAbsolute);
    shift(files, "/branches/0/events/8/time", -kWithin * kStableTimeAbsolute);
    scale(files, "/summary/maxAltitude", kWithin * kStableSummaryRelative);
    shift(files, "/summary/flightTime", kWithin * kStableTimeAbsolute);
    scale(files, "/branches/0/columns/1/max", kWithin * stableTolerance("altitude"));
    scale(files, "/branches/0/optimumAltitude", kWithin * kStableSummaryRelative);
}

/// A simulation of the stable-step set that the mutations change, with the changes within the
/// tolerances that go with it (null: none).
constexpr std::array<SubjectCase, 4> kStableSubjectCases{{
    {.name = "TheRunOnThePad", .subject = kStableOnThePad, .withinTheTolerances = nullptr},
    {.name                = "TheFlight",
     .subject             = kStableFlight,
     .withinTheTolerances = changeTheStableFlightWithinTheTolerances},
    {.name = "TheFlightWithASpeed", .subject = kStableWithASpeed, .withinTheTolerances = nullptr},
    {.name = "TheTwoStages", .subject = kStableTwoStages, .withinTheTolerances = nullptr},
}};

/// The golden values of one simulation of the stable-step set, changed one at a time in a copy
/// of its files.
class SimulationStableGoldenMutation : public ::testing::TestWithParam<SubjectCase>
{ };

TEST_P(SimulationStableGoldenMutation, EveryChangeIsReportedInOneLine)
{
    EXPECT_FALSE(stableMutationsOf(GetParam().subject).empty());
    EXPECT_EQ(stableMutationProblems(GetParam().subject), "");
}

TEST_P(SimulationStableGoldenMutation, TheFilesAsTheyAreAndChangesWithinTheTolerancesMatch)
{
    EXPECT_EQ(stableComparisonAfter(GetParam().subject, nullptr).report, "");
    if (GetParam().withinTheTolerances != nullptr)
    {
        EXPECT_EQ(stableComparisonAfter(GetParam().subject, GetParam().withinTheTolerances).report,
                  "");
    }
}

INSTANTIATE_TEST_SUITE_P(Changes, SimulationStableGoldenMutation,
                         ::testing::ValuesIn(kStableSubjectCases), subjectTestName);

// The tests above run every stable mutation: each belongs to one of the four simulations, and
// each has a name of its own for the report. No simulation is run here.
TEST(SimulationStableGoldenMutations, EveryMutationBelongsToASimulationOfTheTests)
{
    const std::vector<Mutation> all = stableMutations();
    EXPECT_EQ(all.size(), 42U);
    std::size_t covered = 0;
    for (const SubjectCase& subjectCase : kStableSubjectCases)
    {
        covered += stableMutationsOf(subjectCase.subject).size();
    }
    EXPECT_EQ(covered, all.size());
    const std::vector<std::string_view> names = sortedNames(all);
    EXPECT_EQ(std::ranges::adjacent_find(names), names.end()) << "two mutations of one name";
}

/// The plan of the first branch of @p files.
[[nodiscard]] StablePlan firstPlan(const GoldenFiles& files)
{
    return stablePlan(files.document.at("branches").at(0), files.tables.at(0),
                      goldenRodClearance(files.document));
}

/// Changes to what the rules exclude in the [C6-5] flight: the angle of attack and the
/// lateral acceleration in its first hunting row and in the middle of its hunting, and the
/// pitch rate, the pitch damping moment and the lateral acceleration in a band row (rule H;
/// the angle of attack of that row is left alone: it is what makes it a band row); and the
/// yaw rate, which is noise in this planar flight (rule N), in a row of the coast and in its
/// maximum.
void changeWhatTheRulesExcludeInTheFlight(GoldenFiles& files)
{
    const std::size_t hunting = firstOf(firstPlan(files).hunting);
    shiftSeries(files, "aoa", hunting, 0.5);
    shiftSeries(files, "acceleration_xy", hunting, 0.5);
    shiftSeries(files, "aoa", kHuntingRow, 0.5);
    shiftSeries(files, "pitch_moment_coeff", kHuntingRow, 0.5);
    shiftSeries(files, "pitch_rate", kBandRow, 0.5);
    shiftSeries(files, "pitch_damping_moment_coeff", kBandRow, 0.5);
    shiftSeries(files, "acceleration_bodyx", kBandRow, 0.5);
    shiftSeries(files, "yaw_rate", kCoastRow, 0.5);
    scale(files, "/branches/0/columns/23/max", 0.5);
}

// The other side of the rules: a value they exclude is not compared, whatever it is. It is
// counted as excluded, with the same sums. (And the same values are compared one row earlier,
// or in a column of another kind: the mutations above.)
TEST(SimulationStableGoldenMutations, AValueTheRulesExcludeIsCountedNotCompared)
{
    const StableResult unchanged = stableComparisonAfter(kStableFlight, nullptr);
    const StableResult changed =
        stableComparisonAfter(kStableFlight, changeWhatTheRulesExcludeInTheFlight);
    EXPECT_EQ(changed.report, "");
    EXPECT_EQ(changed.compared, unchanged.compared);
    EXPECT_EQ(changed.excluded, unchanged.excluded);
    EXPECT_GT(unchanged.excluded.values, 0);
    EXPECT_GT(unchanged.excluded.numbers, 0);
    EXPECT_EQ(unchanged.excluded.rows, 0) << "the flight is compared to its last row";

    // The rows the changes rely on are what their names say.
    const StableRun* run = stableSubjectRun(kStableFlight);
    ASSERT_NE(run, nullptr);
    const StablePlan plan = firstPlan(run->files);
    ASSERT_GT(plan.rows, kLateRow);
    EXPECT_EQ(firstOf(plan.hunting), 216U);
    EXPECT_TRUE(plan.hunting[kHuntingRow] && !plan.band[kHuntingRow]);
    EXPECT_TRUE(plan.band[kBandRow] && !plan.hunting[kBandRow]);
    EXPECT_TRUE(plan.euler[kLateRow]);
    EXPECT_FALSE(plan.euler[kCoastRow] || plan.hunting[kCoastRow] || plan.band[kCoastRow]);
}

/// Changes to the booster of the two-stage flight of the Beta after its separation: it tumbles
/// before its apogee (rule T).
void changeTheTumblingBoosterAfterItsSeparation(GoldenFiles& files)
{
    GoldenTable& booster = files.tables.at(1);
    booster.rows.at(400).at(1) += 1.0;
    booster.rows.back().at(0) += 1.0;
    shift(files, "/branches/1/events/5/time", 1.0);
    files.document["branches"][1]["rows"]          = 1;
    files.document["branches"][1]["optimumDelay"]  = 1.0;
    files.document["result"]["jitterReplacements"] = 1;
}

TEST(SimulationStableGoldenMutations, ATumblingStageIsComparedUpToItsSeparation)
{
    const StableResult unchanged = stableComparisonAfter(kStableTwoStages, nullptr);
    const StableResult changed =
        stableComparisonAfter(kStableTwoStages, changeTheTumblingBoosterAfterItsSeparation);
    EXPECT_EQ(changed.report, "");
    EXPECT_EQ(changed.compared, unchanged.compared);
    EXPECT_EQ(changed.excluded, unchanged.excluded);
    // The booster has 631 rows, of which the 317 up to its separation at 2 s are compared.
    EXPECT_GE(unchanged.excluded.rows, 314);
}

// ===================================================================================== rules

/// A golden time series with the columns @p columns and the rows @p rows.
[[nodiscard]] GoldenTable tableOf(std::vector<std::string>         columns,
                                  std::vector<std::vector<double>> rows)
{
    GoldenTable table;
    table.columns = std::move(columns);
    table.rows    = std::move(rows);
    return table;
}

/// What is wrong with the tolerance of @p column of one of the tables of tolerances, "" when
/// it is kToleranceMargin times the larger of its two measurements, rounded up to a power of
/// ten and no further (but for the floor, kValueRelative).
[[nodiscard]] std::string toleranceProblem(const MeasuredColumn& column)
{
    constexpr std::array<double, 8> kDecades{1e-9, 1e-8, 1e-7, 1e-6, 1e-5, 1e-4, 1e-3, 1e-2};
    const double                    largest = std::max(column.openRocket, column.qtRocket);
    if (column.tolerance < kToleranceMargin * largest)
    {
        return std::format("{}: below {} times {}\n", column.key, kToleranceMargin, largest);
    }
    if (std::ranges::find(kDecades, column.tolerance) == kDecades.end())
    {
        return std::format("{}: {} is not a power of ten\n", column.key, column.tolerance);
    }
    if (column.tolerance >= 10 * kToleranceMargin * largest && column.tolerance != kValueRelative)
    {
        return std::format("{}: rounded up by more than a power of ten\n", column.key);
    }
    return {};
}

/// One of the tables of tolerances: its name, its columns, the function that answers the
/// tolerance of a column from it, and how its tolerances stand to those of kMeasuredColumns
/// (-1: tighter, 0: they are those, 1: wider).
struct ToleranceTable
{
    std::string_view                name;
    std::span<const MeasuredColumn> columns;
    double (*tolerance)(std::string_view key);
    int side;
};

/// What is wrong with the column @p column of @p table: toleranceProblem(); a tolerance that
/// the function of the table does not answer, or that is not on the table's side of the
/// column's own; a column without a measurement in a table that is not the tighter one (in
/// that one a column can match exactly).
[[nodiscard]] std::string tableColumnProblem(const ToleranceTable& table,
                                             const MeasuredColumn& column)
{
    std::string  problem = toleranceProblem(column);
    const double own     = stableTolerance(column.key);
    if (table.tolerance(column.key) != column.tolerance)
    {
        problem += std::format("{}: not the tolerance {} answers\n", column.key, table.name);
    }
    if ((table.side < 0 && !(column.tolerance < own)) ||
        (table.side > 0 && !(column.tolerance > own)))
    {
        problem += std::format("{}: {} is not on the side of the column's {}\n", column.key,
                               column.tolerance, own);
    }
    if (table.side >= 0 && !(std::max(column.openRocket, column.qtRocket) > 0))
    {
        problem += std::format("{}: no measurement\n", column.key);
    }
    return problem;
}

/// tableColumnProblem() of every column of @p table, and a column that is listed twice.
[[nodiscard]] std::string tableProblems(const ToleranceTable& table)
{
    std::string                   problems;
    std::vector<std::string_view> keys;
    for (const MeasuredColumn& column : table.columns)
    {
        problems += tableColumnProblem(table, column);
        keys.push_back(column.key);
    }
    std::ranges::sort(keys);
    if (std::ranges::adjacent_find(keys) != keys.end())
    {
        problems += "a column is listed twice\n";
    }
    return problems.empty() ? problems : std::format("{}:\n{}", table.name, problems);
}

/// What is wrong with kHuntingNoiseColumns, "" when every one of them has no tolerance in a
/// hunting row and none is listed for it.
[[nodiscard]] std::string huntingNoiseProblems()
{
    std::string problems;
    for (const std::string_view key : kHuntingNoiseColumns)
    {
        if (!(huntingTolerance(key) < 0) || listedTolerance(kHuntingColumns, key).has_value())
        {
            problems += std::format("{} has a hunting tolerance\n", key);
        }
    }
    return problems;
}

TEST(SimulationStableGoldenRules, EveryToleranceIsAHundredTimesTheLargestMeasurement)
{
    EXPECT_EQ(tableProblems({.name      = "kMeasuredColumns",
                             .columns   = kMeasuredColumns,
                             .tolerance = stableTolerance,
                             .side      = 0}),
              "");
    EXPECT_EQ(tableProblems({.name      = "kHuntingColumns",
                             .columns   = kHuntingColumns,
                             .tolerance = huntingTolerance,
                             .side      = 1}),
              "");
    EXPECT_EQ(tableProblems({.name      = "kEulerColumns",
                             .columns   = kEulerColumns,
                             .tolerance = eulerTolerance,
                             .side      = -1}),
              "");
    EXPECT_EQ(stableTolerance("reference_area"), kValueRelative) << "a column that is not listed";
    EXPECT_EQ(huntingTolerance("altitude"), stableTolerance("altitude"));
    EXPECT_EQ(eulerTolerance("altitude"), stableTolerance("altitude"));
    // A column that is noise while hunting has no tolerance there, and none is listed for it.
    EXPECT_EQ(huntingNoiseProblems(), "");
    EXPECT_LT(huntingTolerance("aoa"), 0.0);
    // The other tolerances against their largest measurements (the header has the table).
    EXPECT_GE(kStableTimeAbsolute, kToleranceMargin * 8.0e-7);
    EXPECT_GE(kLateHandlingAbsolute, kToleranceMargin * 2.7e-10);
    EXPECT_GE(kStableSummaryRelative, kToleranceMargin * 9.5e-9);
    EXPECT_GE(kDeploymentVelocityRelative, kToleranceMargin * 1.4e-8);
    EXPECT_GE(kHuntingBand, kToleranceMargin * 1.4e-6);
    EXPECT_GE(kApogeeResidual, kToleranceMargin * 1.8e-16);
    EXPECT_LT(kHuntingBand, kHuntingThreshold);
    // ... and never beyond the plan's bounds: 1e-3 s for an event time, 1e-4 for the apogee
    // and the maximum velocity.
    EXPECT_LE(kStableTimeAbsolute, 1e-3);
    EXPECT_LE(kLateHandlingAbsolute, 1e-3);
    EXPECT_LE(kStableSummaryRelative, 1e-4);
}

TEST(SimulationStableGoldenRules, AHuntingRowIsARungeKuttaRowInFreeFlightWithoutPitchAndYawRate)
{
    const double      nan   = std::numeric_limits<double>::quiet_NaN();
    const GoldenTable table = tableOf({"time", "aoa", "pitch_rate", "yaw_rate"},
                                      {{0.00, 1.5, 0.0, 0.0},    // on the rod
                                       {0.10, 0.2, 0.0, 0.0},    // on the rod
                                       {0.20, 0.1, 0.5, 0.0},    // pitching
                                       {0.30, 1e-6, 0.0, 0.0},   // hunting
                                       {0.40, 1e-6, 0.0, 1e-9},  // yawing
                                       {0.50, 2e-6, 0.0, 0.0},   // hunting
                                       {0.60, nan, nan, nan}});  // an Euler stepper
    EXPECT_EQ(huntingRows(table, 2),
              (std::vector<bool>{false, false, false, true, false, true, false}));
    // A simulation that never clears the rod has no free flight.
    EXPECT_EQ(countOf(huntingRows(table, table.rows.size())), 0);
    // A branch that stored no flight conditions has no hunting rows.
    EXPECT_EQ(countOf(huntingRows(tableOf({"time"}, {{0.0}, {1.0}}), 0)), 0);
}

/// A row of the columns aoa, mach_number, speed_of_sound, pitch_rate and yaw_rate with an
/// airspeed of 34 m/s, the lateral airspeed @p lateral (the airspeed times the sine of the
/// angle of attack) and the pitch rate @p rate.
[[nodiscard]] std::vector<double> rowAtLateralAirspeed(double lateral, double rate)
{
    return {lateral / 34.0, 0.1, 340.0, rate, 0.0};
}

TEST(SimulationStableGoldenRules, ABandRowIsWithinTheBandOfTheHuntingThreshold)
{
    const double      nan = std::numeric_limits<double>::quiet_NaN();
    const GoldenTable table =
        tableOf({"aoa", "mach_number", "speed_of_sound", "pitch_rate", "yaw_rate"},
                {rowAtLateralAirspeed(1.0e-3, 0.0),  // on the rod
                 rowAtLateralAirspeed(5.0e-3, 0.3),  // free flight
                 rowAtLateralAirspeed(1.1e-3, 0.2),  // in the band, above the threshold
                 rowAtLateralAirspeed(0.9e-3, 0.0),  // in the band, hunting
                 rowAtLateralAirspeed(0.5e-3, 0.0),  // hunting
                 rowAtLateralAirspeed(1.2e-3, 0.1),  // out of the band again
                 {nan, 0.1, 340.0, nan, nan}});      // an Euler stepper
    const std::vector<double> lateral = lateralAirspeeds(table);
    EXPECT_NEAR(lateral[1], 5.0e-3, 1e-9);
    EXPECT_NEAR(lateral[3], 0.9e-3, 1e-9);
    EXPECT_TRUE(std::isnan(lateral[6]));
    EXPECT_EQ(bandRows(lateral, 1),
              (std::vector<bool>{false, false, true, true, false, false, false}));
    EXPECT_EQ(eulerRows(table, 1),
              (std::vector<bool>{false, false, false, false, false, false, true}));
    const std::vector<bool> hunting = huntingRows(table, 1);
    EXPECT_EQ(hunting, (std::vector<bool>{false, false, false, true, true, false, false}));
    EXPECT_EQ(misjudgedRows(lateral, hunting, 1), 0U);
    // A row that hunts above the threshold, and one that does not hunt below it.
    EXPECT_EQ(misjudgedRows(lateral, {false, false, true, true, false, false, false}, 1), 2U);
    // A table without the columns has no lateral airspeed, no band row and no Euler row.
    const GoldenTable bare = tableOf({"time"}, {{0.0}, {1.0}});
    EXPECT_EQ(countOf(bandRows(lateralAirspeeds(bare), 0)), 0);
    EXPECT_EQ(countOf(eulerRows(bare, 0)), 0);
    EXPECT_EQ(firstOf({false, false, true, true}), 2U);
    EXPECT_EQ(firstOf({false, false}), 2U);
}

/// A branch of a run with the rows @p rows: the vertical velocity and the time step of each.
[[nodiscard]] std::shared_ptr<FlightDataBranch> branchOfSteps(
    const std::vector<std::pair<double, double>>& rows)
{
    const FlightDataType& velocity =
        FlightDataType::builtin(QtRocket::FlightDataTypeId::TYPE_VELOCITY_Z);
    const FlightDataType& step =
        FlightDataType::builtin(QtRocket::FlightDataTypeId::TYPE_TIME_STEP);
    auto branch = std::make_shared<FlightDataBranch>(
        "branch",
        std::initializer_list<std::reference_wrapper<const FlightDataType>>{velocity, step});
    for (const auto& [left, length] : rows)
    {
        branch->addPoint();
        branch->setValue(velocity, left);
        branch->setValue(step, length);
    }
    return branch;
}

// Rule E: another step from the apogee row than the golden run's is the decision by the last
// bit only when the step to the apogee left a rounding error and the extra step of 1 ms
// follows a positive one. Anything else is reported.
TEST(SimulationStableGoldenRules, AnotherStepFromTheApogeeIsExcusedOnlyByTheLastBit)
{
    EXPECT_EQ(apogeeStepProblem({.left = 0.0, .before = 8.0, .step = 0.1}), "");
    EXPECT_EQ(apogeeStepProblem({.left = -1.7e-18, .before = 8.0, .step = 0.102}), "");
    EXPECT_EQ(apogeeStepProblem({.left = 1.7e-18, .before = 8.0, .step = 0.001}), "");
    EXPECT_EQ(apogeeStepProblem({.left = 8e-12, .before = 8.0, .step = 0.001}),
              "the step to the apogee leaves a vertical velocity of 8e-12 m/s after 8 m/s, which "
              "is no rounding error");
    EXPECT_EQ(apogeeStepProblem({.left = 0.0, .before = 8.0, .step = 0.001}),
              "a step of 0.001 s follows a vertical velocity of 0 m/s at the apogee");
    EXPECT_EQ(apogeeStepProblem({.left = 1.7e-18, .before = 8.0, .step = 0.1}),
              "a step of 0.1 s follows a vertical velocity of 1.7e-18 m/s at the apogee");
    EXPECT_EQ(apogeeStepProblem({}), "there is no row that steps from the apogee");

    // A golden branch of five rows whose apogee row is row 2, with the regular step after it.
    StablePlan plan;
    plan.rows            = 5;
    plan.flownRows       = 5;
    plan.apogeeRow       = 2;
    plan.apogeeTime      = 4.0;
    plan.shortApogeeStep = false;
    const std::vector<std::pair<double, double>> alike{
        {20.0, 0.1}, {8.0, 0.1}, {0.0, 0.1}, {-1.0, 0.1}, {-2.0, 0.1}};
    const StableExtent whole = stableExtent(plan, *branchOfSteps(alike));
    EXPECT_TRUE(whole.whole);
    EXPECT_EQ(whole.rows, 5U);
    EXPECT_EQ(whole.problem, "");

    // The last bit: the extra step, after a positive rounding error. Not compared from the
    // apogee row on, and no mismatch.
    const std::vector<std::pair<double, double>> lastBit{
        {20.0, 0.1}, {8.0, 0.1}, {8.9e-16, 0.001}, {-0.01, 0.1}, {-1.0, 0.1}, {-2.0, 0.1}};
    const StableExtent excused = stableExtent(plan, *branchOfSteps(lastBit));
    EXPECT_FALSE(excused.whole);
    EXPECT_EQ(excused.rows, 2U);
    EXPECT_EQ(excused.until, 4.0 - kOnTime);
    EXPECT_EQ(excused.problem, "");

    // A step to the apogee that stops short of it (1e-12 of the velocity is left): the same
    // extent, and a mismatch.
    const std::vector<std::pair<double, double>> shortOfIt{
        {20.0, 0.1}, {8.0, 0.1}, {8e-12, 0.001}, {-0.01, 0.1}, {-1.0, 0.1}, {-2.0, 0.1}};
    const StableExtent reported = stableExtent(plan, *branchOfSteps(shortOfIt));
    EXPECT_EQ(reported.rows, 2U);
    EXPECT_EQ(reported.problem,
              "the step to the apogee leaves a vertical velocity of 8e-12 m/s "
              "after 8 m/s, which is no rounding error");

    // A run that ends before the apogee row.
    const std::vector<std::pair<double, double>> tooShort{{20.0, 0.1}, {8.0, 0.1}};
    EXPECT_EQ(stableExtent(plan, *branchOfSteps(tooShort)).problem,
              "there is no row that steps from the apogee");
}

TEST(SimulationStableGoldenRules, TheApogeeRowIsWhereAnEulerStepperLandsOnTheApogee)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    // Deployed on the way up: the stepper lands on the apogee in row 3.
    EXPECT_EQ(
        eulerApogeeRow(tableOf({"aoa", "velocity_z"},
                               {{0.1, 50.0}, {nan, 40.0}, {nan, 9.0}, {nan, 2e-18}, {nan, -1.0}})),
        std::optional<std::size_t>{3});
    // Deployed on the way down, after an apogee under a Runge-Kutta stepper: none.
    EXPECT_EQ(eulerApogeeRow(tableOf({"aoa", "velocity_z"},
                                     {{0.1, 5.0}, {0.2, -1.0}, {nan, -2.0}, {nan, -3.0}})),
              std::nullopt);
    // An apogee less than the minimum step away is stepped over, not landed on: none.
    EXPECT_EQ(
        eulerApogeeRow(tableOf({"aoa", "velocity_z"}, {{0.1, 5.0}, {nan, 0.004}, {nan, -0.006}})),
        std::nullopt);
    // No Euler stepper at all, and a branch without these columns: none.
    EXPECT_EQ(eulerApogeeRow(tableOf({"aoa", "velocity_z"}, {{0.1, 5.0}, {0.2, 1e-18}})),
              std::nullopt);
    EXPECT_EQ(eulerApogeeRow(tableOf({"time"}, {{0.0}})), std::nullopt);
}

TEST(SimulationStableGoldenRules, AnOutOfPlaneColumnIsNoiseBelowAHundredthOfItsCounterpart)
{
    const std::vector<StableColumn> planar = stableColumns(
        tableOf({"pitch_rate", "yaw_rate", "roll_rate", "aoa", "altitude", "acceleration_x",
                 "acceleration_y", "orientation_theta", "drag_coeff"},
                {{1.0, 1e-4, 1e-7, 0.1, 5.0, 3.0, 0.0, 1.5, 0.7},
                 {-2.0, -5e-5, 2e-7, 0.2, 9.0, 4.0, 0.0, 1.4, 0.8}}));
    EXPECT_EQ(planar[0].kind, ColumnKind::EVERY_ROW);
    EXPECT_TRUE(planar[0].switched) << "the pitch rate is zero while hunting";
    EXPECT_EQ(planar[1].kind, ColumnKind::ON_THE_ROD_ONLY) << "5e-5 of the pitch rate";
    EXPECT_EQ(planar[2].kind, ColumnKind::ON_THE_ROD_ONLY) << "1e-7 of the pitch rate";
    EXPECT_EQ(planar[3].kind, ColumnKind::NOT_WHILE_HUNTING);
    EXPECT_LT(planar[3].hunting, 0.0);
    EXPECT_EQ(planar[4].kind, ColumnKind::EVERY_ROW);
    EXPECT_FALSE(planar[4].switched);
    EXPECT_EQ(planar[0].scale, 2.0);
    EXPECT_EQ(planar[4].tolerance, stableTolerance("altitude"));
    EXPECT_EQ(planar[4].hunting, stableTolerance("altitude"));
    EXPECT_EQ(planar[4].euler, stableTolerance("altitude"));
    EXPECT_EQ(planar[5].kind, ColumnKind::NOT_WHILE_HUNTING);
    EXPECT_EQ(planar[6].kind, ColumnKind::EVERY_ROW) << "a column of zeros is compared";
    // An attitude column that keeps its signal while hunting, and a constant of an Euler row.
    EXPECT_EQ(planar[7].kind, ColumnKind::EVERY_ROW);
    EXPECT_GT(planar[7].hunting, planar[7].tolerance);
    EXPECT_EQ(planar[8].euler, kValueRelative);
    EXPECT_GT(planar[8].tolerance, kValueRelative);

    const std::vector<StableColumn> turning = stableColumns(
        tableOf({"pitch_rate", "yaw_rate", "position_x", "position_y", "position_direction"},
                {{1.0, 0.5, 100.0, 0.5, 0.1}, {-2.0, 0.1, 200.0, -0.2, 0.2}}));
    EXPECT_EQ(turning[1].kind, ColumnKind::EVERY_ROW) << "a quarter of the pitch rate";
    EXPECT_EQ(turning[3].kind, ColumnKind::ON_THE_ROD_ONLY);
    EXPECT_EQ(turning[4].kind, ColumnKind::ON_THE_ROD_ONLY) << "measured by the lateral position";
}

TEST(SimulationStableGoldenRules, ThePlanOfABranchFollowsItsEvents)
{
    const double      nan = std::numeric_limits<double>::quiet_NaN();
    const GoldenTable table =
        tableOf({"time", "aoa", "pitch_rate", "yaw_rate", "velocity_z", "time_step"},
                {{0.0, 1.5, 0.0, 0.0, 0.0, 0.1},
                 {0.1, 0.2, 0.0, 0.0, 9.0, 0.1},
                 {0.2, 0.1, 0.0, 0.0, 20.0, 0.1},
                 {0.3, nan, nan, nan, 10.0, 0.1},
                 {0.4, nan, nan, nan, 1e-17, 0.001},
                 {0.401, nan, nan, nan, -0.01, 0.1},
                 {0.501, nan, nan, nan, -1.0, 0.1}});
    const json       flight = json::parse(R"({"events": [
        {"time": 0.0, "type": "LAUNCH"}, {"time": 0.1, "type": "LAUNCHROD"},
        {"time": 0.25, "type": "BURNOUT"}, {"time": 0.3, "type": "RECOVERY_DEVICE_DEPLOYMENT"},
        {"time": 0.401, "type": "APOGEE"}]})");
    const StablePlan plan   = stablePlan(flight, table, 0.1);
    EXPECT_EQ(plan.rows, 7U);
    EXPECT_EQ(plan.rodRows, 2U);
    EXPECT_EQ(plan.hunting, (std::vector<bool>{false, false, true, false, false, false, false}));
    EXPECT_FALSE(plan.tumbles);
    EXPECT_EQ(plan.flownRows, 7U);
    EXPECT_EQ(plan.euler, (std::vector<bool>{false, false, false, true, true, true, true}));
    EXPECT_EQ(countOf(plan.band), 0) << "no lateral airspeed without the airspeed";
    EXPECT_EQ(plan.apogeeRow, std::optional<std::size_t>{4});
    EXPECT_EQ(plan.apogeeTime, 0.4);
    EXPECT_TRUE(plan.shortApogeeStep);
    EXPECT_EQ(plan.apogeeProblem, "") << "the short step follows a positive rounding error";
    EXPECT_EQ(plan.lateFrom, 0.25) << "no row at the time of the burnout";
    EXPECT_EQ(floorRows(plan), 4U);
    EXPECT_EQ(eventTolerance(plan, 0.25), kStableTimeAbsolute);
    EXPECT_EQ(eventTolerance(plan, 0.3), kLateHandlingAbsolute);
    // The values of the plan: the rod rows at kValueRelative, the others at the column's
    // tolerance, and the angle of attack (an attitude column) not in the hunting row.
    EXPECT_EQ(cellTolerance(plan, plan.columns[1], 1), kValueRelative * 1.5);
    EXPECT_LT(cellTolerance(plan, plan.columns[1], 2), 0.0);
    EXPECT_EQ(cellKind(plan, plan.columns[1], 2), CellKind::HUNTING_NOISE);
    EXPECT_EQ(cellKind(plan, plan.columns[4], 2), CellKind::HUNTING);
    EXPECT_EQ(cellTolerance(plan, plan.columns[4], 2), stableTolerance("velocity_z") * 20.0);
    EXPECT_EQ(cellKind(plan, plan.columns[4], 3), CellKind::EULER);
    EXPECT_EQ(cellTolerance(plan, plan.columns[4], 3), eulerTolerance("velocity_z") * 20.0);
    EXPECT_EQ(cellKind(plan, plan.columns[0], 1), CellKind::ROD);
    EXPECT_EQ(comparedValues(plan, plan.rows), (7 * 6) - 1);
    // In a band row the pitch rate is not compared, nor the angle of attack; the time is.
    StablePlan banded = plan;
    banded.band[5]    = true;
    banded.euler[5]   = false;
    EXPECT_EQ(cellKind(banded, banded.columns[2], 5), CellKind::BAND);
    EXPECT_EQ(cellKind(banded, banded.columns[1], 5), CellKind::BAND);
    EXPECT_EQ(cellKind(banded, banded.columns[0], 5), CellKind::HUNTING);
    EXPECT_LT(cellTolerance(banded, banded.columns[2], 5), 0.0);
    // A golden branch whose short step follows no positive velocity does not show rule E.
    GoldenTable odd = table;
    odd.rows[4][4]  = -1e-17;
    EXPECT_EQ(stablePlan(flight, odd, 0.1).apogeeProblem,
              "a step of 0.001 s follows a vertical velocity of -1e-17 m/s at the apogee");

    // A stage that tumbles before its apogee is compared up to its last separation.
    const json       booster  = json::parse(R"({"events": [
        {"time": 0.0, "type": "IGNITION"}, {"time": 0.2, "type": "STAGE_SEPARATION"},
        {"time": 0.3, "type": "TUMBLE"}, {"time": 0.401, "type": "APOGEE"}]})");
    const StablePlan tumbling = stablePlan(booster, table, 0.1);
    EXPECT_TRUE(tumbling.tumbles);
    EXPECT_EQ(tumbling.separation, 0.2);
    EXPECT_EQ(tumbling.flownRows, 3U);
    EXPECT_EQ(tumbling.apogeeRow, std::nullopt) << "the apogee row is after the separation";
    EXPECT_EQ(floorRows(tumbling), 3U);

    // A simulation that never clears the rod: every row at kValueRelative.
    const StablePlan onThePad = stablePlan(flight, table, std::nullopt);
    EXPECT_EQ(onThePad.rodRows, 7U);
    EXPECT_EQ(countOf(onThePad.hunting), 0);
}

TEST(SimulationStableGoldenHarness, TheStableTimeStepIsSetLastAndRecorded)
{
    InstallationDefaults defaults;
    json                 harness = makeReproducible(defaults.options);
    EXPECT_FALSE(harness.contains("timeStep"));
    useStableTimeStep(defaults.options, harness, kStableTimeStep);
    EXPECT_EQ(defaults.options.getTimeStep(), 0.01);
    EXPECT_EQ(harness.at("documentTimeStep"), 0.05);
    EXPECT_EQ(harness.at("timeStep"), 0.01);
    EXPECT_EQ(harness.size(), kStableHarnessKeys.size());
}

}  // namespace
