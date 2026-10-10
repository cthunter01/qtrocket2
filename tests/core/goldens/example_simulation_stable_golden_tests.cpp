// Simulation golden tests of OpenRocket's example designs, the stable-step set: every simulation
// of the sixteen designs of data/examples, as the .ork loader loads it, run with a time step of
// 0.01 s, against what OpenRocket's BasicEventSimulationEngine computes for it at that step, in
// tests/data/goldens/example-<name>/stable/sim_<NN>_<name>.json (tools/openrocket-goldens:
// GoldenDumper.examplePass() and SimulationDumper.java; the format is in that tool's README.md).
// 54 simulations with 69 branches and 734 events, the whole flights.
//
// The golden files of this set are the simulation documents alone (the user's decision for
// tier 9: the time series of the 92286 rows would add 37 MB): the summary values, the warnings,
// the result and, of each branch, the number of rows, the columns with their minima and maxima,
// the events, the optimum altitude and the separation time. There is no row to compare a row
// with. So this comparison is what simulation_stable_golden_tests.cpp does for the test rockets
// as far as a document reaches: read the top of that file for the stable-step set, for what of
// a flight at 0.01 s two runs of OpenRocket do not reproduce and why, and for the rules H, N, E
// and T that follow from it. This file says what each of them becomes without golden rows, and
// holds the measurements of the examples that its rules and tolerances rest on. The default-step
// set of the same simulations (example_simulation_golden_tests.cpp) compares the time series of
// the launch rod and a little more; the summary values, the numbers of rows and the times of the
// events of the whole flights are compared here.
//
// THE SET-UP is the harness's (GoldenDumper.examplePass(), the stable pass): the design is
// loaded as GoldenExamples.h loads it and its automatic dimensions are settled; then every
// simulation of the document is run, on the one rocket of the document, with its own flight
// configuration, options and extensions and the harness's changes (runDocumentSimulation() of
// GoldenSimulations.h: seed 0, no wind deviation, the jitter removal), and with the stable time
// step set last (SimulationDumper.useStableTimeStep(): the "harness" of a document records it
// and the step the simulation had, 0.05 s in 49 and 0.04 s in the five of "Pods--airframes and
// winglets"). Each simulation is run once: a run changes the options of its simulation in
// place, and nothing of the rocket (the engine flies a copy of it).
//
// COMPARED EXACTLY (it is compareSimulation() of GoldenSimulations.h with nothing taken as
// reproducible, as in the stable-step set of the test rockets): the options, the "harness"
// block, what identifies the simulation, its flight configuration and its extensions, the
// status, the branches with their names, sources and columns (keys, names, symbols) and with a
// "csv" that is null, the events in order with their types, sources and data (simultaneous
// IGNITION events as a set), the warnings; under the rule of the tumbling dropped stages of the
// default-step comparison of the examples (ComparisonMode::droppedStagesTumbleFreely).
//
// COMPARED WITHIN TOLERANCES, from the documents: the ten summary values, the number of jitter
// replacements and the parameter of every warning (with the texts that print it); and of every
// branch its number of rows, its optimum altitude, the time to it and its optimum delay, its
// separation time, the time of every event, and the minimum and the maximum of its steady
// columns (below). A document holds 11369 numbers outside a time series. 9664 of them are the
// minimum and the maximum of each of the 4832 columns, of which the 3012 of the steady columns
// are compared and 6652 are not; of the other 1705 rule T excludes 53. So 4664 numbers are
// compared, the same on every platform (ExampleSimulationStableGoldenCoverage pins all of these
// numbers from the files alone, and the test of each example holds them against what it
// compared).
//
// WHAT THE COMPARISON OF THE TEST ROCKETS TAKES FROM THE GOLDEN ROWS, and what becomes of it:
// - The rows on the launch rod, compared value by value: not compared, there are none. Of the
//   rod the launch rod velocity is compared (at 1e-9 of the largest velocity, as there) and the
//   times of LIFTOFF and LAUNCHROD.
// - Rule H (hunting rows and band rows) and rule N (noise-dominated out-of-plane columns) are
//   rules about values of the time series: nothing to apply them to. What the hunting does to
//   the numbers that are compared is in the measurement below (59 of the 69 golden branches
//   hunt; it is the hunting flights whose summary values differ by 1e-9 between two runs).
// - The minimum and the maximum of every column: which of them two runs reproduce is decided
//   there by rules H and N, row by row. Here it is decided column by column, from the
//   measurements: the extremes of the steady columns are compared and no other (below).
// - Rule T, the stage that tumbles: from the events of the document (below).
// - Rule E, the step from the apogee of an Euler stepper: the step of the run from its own
//   rows, as there; the step of the golden run from a list, which the documents bear out
//   (below).
// - The late handlings: from a list, read once from the rows of the full dump (below).
//
// WHAT OPENROCKET REPRODUCES OF THESE FLIGHTS. A design that is loaded keeps the ids of its
// components, so two runs of the harness give the same files; run 9a of the port gave the
// harness a last-bit perturbation for this (LAST_BIT of generate.sh: one ulp added to the
// velocity, the rotation velocity, the position or the orientation after every step) and made
// 21 dumps with it, with the time series of this set. Those and the committed run are 22 runs
// of OpenRocket of every simulation (the table of them is in the notes of run 9a, and
// tier9d-fix-simulation-goldens/scripts/rules2.py compares every ordered pair of them, 462 per
// simulation, as this file compares QtRocket's run with the committed documents, with the rules
// and the units of this file: out/openrocket-rules2-22.txt). QtRocket was measured against the
// committed documents in 47 runs of the test binary (glibc, and the 46 patterns of the one-ulp
// libm shim that the other golden measurements use: patterns.py, out/merged-final.txt).
// - The structure is the same in all 22 runs up to the last stage separation of a dropped stage
//   without a recovery device; after it two such stages record no TUMBLE in some runs (and none
//   records two).
// - The number of rows differs in 19 of the 69 branches: by one row in 13 (rule E) and by up to
//   840 in 6 of the 7 dropped stages without a recovery device (rule T). None unexplained: under
//   the rules below every number of rows and of jitter replacements that is compared is the
//   same in all 462 pairs, and so it is between QtRocket and the documents in the 47 runs.
// - No difference of structure that a rule does not explain, in either measurement.
//
// THE EXTREMES OF THE COLUMNS. A document gives the minimum and the maximum of each column of a
// branch. In most columns they are those of a flight that two runs do not reproduce to the
// tolerance of a value: OpenRocket's own runs differ in an extreme of 41 of the 71 columns by
// more than 1e-9 of the column's scale and of 14 by more than 1e-5 (the lateral acceleration
// across the wind: 2.8e-3), QtRocket's from the documents in 41 and in 13. The steady columns
// are those whose two extremes agree to 2e-11 of the scale of the column in every branch, in
// both measurements (the scale: the larger magnitude of the golden minimum and maximum):
// - in the 62 branches under no rule T the 23 of kSteadyColumns: the mass, the motor mass, the
//   two moments of inertia and the centre of gravity; the thrust, its correction and the
//   thrust-to-weight ratio; the roll rate and the roll forcing and damping coefficients, and
//   the fin cant that the roll control adds as a column; the drag coefficient with its base
//   and pressure parts; the side force and yaw moment coefficients; gravity and the latitude;
//   the wind and the reference length and area. The largest difference is 1.7e-11 between
//   OpenRocket's runs (a roll damping coefficient) and 1.4e-11 between QtRocket and the
//   documents (a latitude);
// - in the 7 branches of rule T the 20 of kSteadyColumnsOfATumblingStage, which are what the
//   stage has before it is left to its attitude, or whatever it does: 1.7e-12 and 2.4e-12
//   (the thrust-to-weight ratio, both).
// Their extremes are compared at kStableSummaryRelative, 1e-6 of that scale, the tolerance of
// the extremes that are summary values: 6e4 times these differences. The tolerance of a value,
// 1e-9, would be 60 times the one and 75 times the other, and is less than the 7.5e-9 by which
// the kick of the sensitivity measurement below moves an extreme of a steady column (the
// thrust-to-weight ratio of a dropped stage; under no rule T 3.6e-9, the fin cant of the roll
// control). The other 48 columns (50 under rule T) are counted as excluded, 6652 numbers: in
// each of them one of the two measurements differs by 3.3e-11 and more.
// What these 3012 numbers hold that nothing else here does: the mass, the inertia, the centre
// of gravity and the thrust of every branch, of a dropped stage after its separation too; and
// the roll. A fin cant that the loader mirrored moves the extremes of the roll columns by their
// whole scale, where the summary values and the event times do not move at all; and the gains
// of the roll control are held by the extremes of the roll rate and of the fin cant of its two
// flights (a proportional gain that is 0.1 % off moves the extremes of the roll rate by 2e-5
// of its scale and more, and nothing but extremes of the roll columns beyond a tolerance).
//
// THE RULES. Each applies to the branches of a list that is pinned here (kTumblingStages,
// kEulerApogees, kLateHandlings) and to no other; the lists are those of the table of the 22
// runs, and ExampleSimulationStableGoldenCoverage holds them against what the documents
// themselves show.
// - T, a dropped stage without a recovery device (7 branches: the booster set of "Simulation 1"
//   of the parallel staging, and the two boosters of each simulation of the three-stage rocket;
//   told from the golden events as the default-step comparison tells them,
//   isUnrecoveredDroppedStage(): a branch after the first with a STAGE_SEPARATION and no
//   RECOVERY_DEVICE_DEPLOYMENT). At this step each of them records a TUMBLE in the committed run,
//   two before their apogee and five after it; either way what follows the separation is not
//   reproducible: in OpenRocket's 22 runs the TUMBLE moves by 2.1 s (and is missing in 3 runs of
//   one stage and in 1 of another), the GROUND_HIT by 1.3 s, the number of rows by 840, and the
//   number of jitter replacements of the simulation by 3492. QtRocket's stages differ from the
//   committed ones by up to 990 rows. Such a branch is compared like every other up to its last
//   stage separation, its separation time among it. Of what follows, its number of rows and the
//   times of its events (its APOGEE, its TUMBLE, its GROUND_HIT and its SIMULATION_END) are not
//   compared, nor the number of jitter replacements of its simulation, and its TUMBLE events
//   are left out of the structure on both sides (7 golden events). What is compared of the
//   flight after the separation is its apogee, by its altitude: the stage coasts up to it for
//   0.8 to 1.5 s, tumbling or not, and in the 22 runs the altitude moves by 1.1e-4 of itself
//   (QtRocket against the documents: 7.4e-5). So the optimum altitude of such a branch, which is
//   the altitude of its apogee, is compared at kTumblingAltitudeRelative, 2e-2 of itself. The
//   time of the apogee is bounded as well, and left out all the same: the APOGEE event, the
//   time to the optimum altitude and the optimum delay (that time less the burnout) move by
//   9.3e-3 s, one time step, between OpenRocket's runs (QtRocket: 7.5e-3 s), and a hundred times
//   that is the length of the coast itself, a tolerance that would hold nothing which the
//   altitude does not hold. That is 53 numbers not compared: 7 of each stage, and the jitter
//   replacements of their 4 simulations.
// - E, the step from the apogee of an Euler stepper (16 branches: those whose recovery device
//   is out before their APOGEE, so that the landing stepper passes the apogee; no other branch
//   has such golden events). The stepper lands a step on the apogee, and the last bit of the
//   vertical velocity it is left with decides whether an extra step of 1 ms follows (see rule E
//   of the test rockets). In OpenRocket's 22 runs 13 of the 16 branches take either step (1206
//   of the 7392 ordered pairs of runs of a branch differ); QtRocket's 47 runs take either step
//   in 14 of the 16, among them one that OpenRocket's 22 runs never step short in, and step
//   otherwise than the committed run in 1 to 6 branches of a run. So the rule applies to all
//   16. The step of the run is read from its own rows (the row on which an Euler stepper landed
//   on the apogee, and the step from it: apogeeOutcome()); the step of the golden run is that
//   of the list: the extra one in 2 branches, the regular one in 14. The documents show it for
//   the 14 first branches (the APOGEE event is 1 ms after the summary's time to apogee exactly
//   when the extra step was taken: no exception in 22 runs of 14 branches); for the 2 later
//   branches (the payload sections of two simulations of "Deployable payload") the list has
//   what the rows of the committed run show (tier9d-fix-simulation-goldens/out/
//   rule-rows-base-full.txt). When the two runs step alike, the branch is compared at the
//   tolerances of every other branch. When they do not, the run is on the other time grid from
//   its apogee on, the one that the extra step moves by 1 ms, and is compared as that:
//   * its APOGEE with the golden one moved by the 1 ms of the extra step that one of the two
//     runs took (kExtraStep; the engine gives the event the time of the apogee row, or of the
//     row after the extra step), at the tolerance of every other time: the two differ from that
//     by 2.1e-12 s at most between OpenRocket's runs and by 1.2e-12 s between QtRocket and the
//     documents. That also holds the step of the list to the run: with the wrong step listed
//     for a branch, its APOGEE would be 1 ms off;
//   * its number of rows with the golden one and the row of the extra step, exactly: one row
//     more, or one less. So it is in every one of the 1206 pairs of OpenRocket's runs that step
//     otherwise and in every such branch of QtRocket's 47 runs, and so it has to be while the
//     last step of the descent, which ends on the ground, is longer than the 1 ms on the grid
//     without the extra step: it is 0.31 s and more in every branch of the rule, in the 22
//     runs and in QtRocket's 47;
//   * the events after its APOGEE (the GROUND_HIT and the SIMULATION_END, in every one of the
//     16) and, when it is the first branch, the flight time at kOtherGridTimeAbsolute, 2e-2 s,
//     and the ground hit velocity at kOtherGridVelocityRelative, 1e-4 of itself: on the two
//     grids the descent of one flight ends up to 1.9e-4 s apart, in OpenRocket's pairs and in
//     QtRocket's runs alike (the same flight of the three-stage rocket), and the ground hit
//     velocity differs by 9.3e-7 of itself (the motor pods' flight, whose landing steps fall
//     elsewhere; QtRocket: the same).
//   That is 5 numbers of a first branch and 3 of a later one, 76 at most; the test of each
//   example holds what was compared so against what the rows of its runs say may be. And the
//   rows of the run have to show that the last bit decided (apogeeStepProblem(): the step to
//   the apogee left at most 1e-13 of the velocity before, 1.7e-16 in the 47 runs, and the extra
//   step follows a positive rest): any other step from the apogee is a mismatch.
// - L, a late handling (26 branches). An event without a row at its time was handled at the end
//   of a step that overshot it, and what the engine queues on handling an event gets its time
//   from the time of the handling (see "the time of an event that follows a late handling" at
//   the top of simulation_stable_golden_tests.cpp). The golden rows of the test rockets show
//   such events; here the first of each branch is in the list, by its place among the golden
//   events and its type, read from the rows of the full dump of the committed run
//   (tier9d-stable-step/scripts/documents.py; the rows are in tier9d-fix-simulation-goldens/
//   out/rule-rows-base-full.txt). 19 are the deployment of a recovery device at the apogee,
//   which the engine queues 1 ms after the time of the APOGEE event, under a Runge-Kutta
//   stepper the time of the row before the one that found the apogee: it is handled 9 ms late
//   in every run, and the documents show it by themselves (21 branches have such a deployment,
//   2 of them after an earlier late handling). 6 are a BURNOUT that a step of the minimum
//   length overshot by 0.06 to 0.13 ms, and 1 a SIM_WARN. The times after the late handling of
//   a branch (106 of events, 4 separation times) are compared at kLateHandlingAbsolute, 1e-3 s,
//   as in the test rockets. They do not move in any measurement by more than 5.7e-7 s, and in
//   each of QtRocket's 47 runs the first event without a row at its time is, in every one of
//   the 69 branches, the one of the list or none (measured by the measurement test, not
//   asserted).
//
// TOLERANCES. Those of the numbers under no rule are the tolerances of the stable-step
// comparison of the test rockets (GoldenStableSimulations.h), unchanged; the three of the
// rules E and T are of this file. Over what this comparison compares, each is at least 100 times
// (kToleranceMargin) the largest difference of both measurements, OpenRocket against itself in
// the 462 ordered pairs of its 22 runs and QtRocket against the committed documents in its 47
// runs (ExampleSimulationStableGoldenRules pins the tolerances and holds the arithmetic):
//                                                           tolerance   OpenRocket   QtRocket
//   maximum altitude, optimum altitude (of their scale)     1e-6        6.1e-9       3.1e-9
//   maximum velocity                                        1e-6        2.1e-9       2.1e-9
//   maximum acceleration, maximum Mach number               1e-6        2.6e-11      2.3e-11
//   ground hit velocity (of itself)                         1e-6        2.4e-14      2.1e-14
//   parameter of a warning (of itself)                      1e-6        9.0e-12      8.0e-12
//   minimum, maximum of a steady column (of its scale)      1e-6        1.7e-11      1.4e-11
//   deployment velocity (of the largest velocity)           1e-5        1.8e-9       1.8e-9
//   launch rod velocity (of the largest velocity)           1e-9        5.3e-14      6.0e-17
//   flight time                                             1e-4 s      5.7e-7 s     2.9e-7 s
//   GROUND_HIT, SIMULATION_END                              1e-4 s      1.9e-7 s     2.0e-7 s
//   optimum delay, time to the optimum altitude             1e-4 s      2.9e-9 s     1.6e-9 s
//   APOGEE, time to apogee, RECOVERY_DEVICE_DEPLOYMENT      1e-4 s      1.4e-10 s    6.4e-11 s
//   LAUNCH, LIFTOFF, LAUNCHROD, IGNITION, BURNOUT,
//     EJECTION_CHARGE, STAGE_SEPARATION, SIM_WARN, TUMBLE,
//     separation time                                       1e-4 s      0            0
//   after a late handling: GROUND_HIT, SIMULATION_END       1e-3 s      5.7e-7 s     2.9e-7 s
//     RECOVERY_DEVICE_DEPLOYMENT, SIM_WARN                  1e-3 s      2.4e-7 s     1.2e-7 s
//     the other events, separation time                     1e-3 s      1.3e-11 s    8.2e-12 s
//   numbers of rows and of jitter replacements              exact       equal        equal
//   rule E, a run that steps otherwise:
//     APOGEE, less the 1 ms                                 1e-4 s      2.1e-12 s    1.2e-12 s
//     number of rows, with the row of the extra step        exact       equal        equal
//     GROUND_HIT, SIMULATION_END, flight time               2e-2 s      1.9e-4 s     1.9e-4 s
//     ground hit velocity (of itself)                       1e-4        9.3e-7       9.3e-7
//   rule T, a dropped stage: optimum altitude (of itself)   2e-2        1.1e-4       7.4e-5
// Among the numbers under no rule the largest difference is 6.1e-3 of its tolerance between two
// runs of OpenRocket (a factor of 163: the maximum altitude of "Simulation 2" of the two-stage
// high power rocket) and 3.1e-3 between QtRocket and the documents (a factor of 327: the same
// number). The numbers of rule E on the other time grid are at 9.4e-3 of their tolerances in
// both measurements (a factor of 107: the end of the descent of "Simulation 1" of the
// three-stage rocket, which the two grids put the same 1.9e-4 s apart whichever engine flies
// it). QtRocket behaves as one more run of OpenRocket: in no row of the table is it further
// from the committed run than OpenRocket's own runs are from each other by more than
// a quarter.
// (A summary value is measured against the scale of its column of the first branch, which the
// document gives as the larger magnitude of the column's golden minimum and maximum, or against
// its own magnitude where the table says so.)
//
// SENSITIVITY. The libm shim moves a result by one ulp, and that did not predict MSVC for the
// default-step set of the test rockets; so, as for their stable-step set, two more things were
// looked at.
// - A perturbation a million times larger
//   (ExampleSimulationStableGoldenMeasurement.DISABLED_PrintsTheSensitivityToAKick: a relative
//   1e-10 kick to the velocity and to the rotation velocity after every step, each kicked run
//   compared with the unperturbed one under the rules and tolerances of this file). The
//   trajectory follows the kick in proportion, and nothing that is compared moves beyond its
//   tolerance: the launch rod velocity by 2.3e-10 of its scale (a quarter of its tolerance, the
//   most of all), the other summary values by 2.2e-9 at most, an extreme of a steady column by
//   7.5e-9 of its scale, the flight time and a GROUND_HIT by 9.7e-7 s, an APOGEE by 6.6e-9 s;
//   the events the motors and the rod time by 3.8e-9 s where they are queued at a late handling
//   and not at all elsewhere (a LAUNCHROD: 1.7e-14 s); no sequence of events changes, no number
//   of rows but where rule T or E says it can, and the late handlings stay where they are. The
//   step from the apogee changes in 8 of the 16 branches, and each of those runs is then to the
//   unperturbed one what a run on the other time grid is to its golden run: one row more or
//   less, the end of its descent up to 1.9e-4 s and its ground hit velocity 9.3e-7 of itself
//   apart.
// - The places where the engine decides by a threshold, in the rows of the 22 runs of OpenRocket
//   (tier9d-stable-step/scripts/thresholds.py, out/thresholds-22.txt), since a decision that
//   falls otherwise moves an event by a whole step, a hundred tolerances:
//   * LIFTOFF (altitude over 2 cm) and LAUNCHROD (distance over the rod length), 54 flights: the
//     row that crosses the threshold and the row before are at least 3.6e-5 m and 1.1e-4 m from
//     it, where the 22 runs differ by 1.6e-13 m at most.
//   * APOGEE under a Runge-Kutta stepper (altitude 1 cm below its maximum, 21 branches): at
//     least 4.6e-5 m from the threshold, where the drop differs by 4.4e-9 m between the runs.
//   * A step that ends on an event (373 events after a Runge-Kutta step): a step is stretched
//     to an event less than 0.5 ms beyond its end; the longest such step is 0.043 ms short of
//     the 10.5 ms at which it would not have been. The 6 BURNOUT events handled late: the row
//     before is 0.87 to 0.94 ms short of the event, 0.37 ms and more beyond the 0.5 ms.
//   * TUMBLE of the two sustainers that tumble after their apogee ("ARC payload rocket", "Tube
//     fin rocket"; compared like every other event): the filtered angle of attack of the step
//     that crosses the 60 degrees and of the step before are at least 2.8e-4 rad from it, where
//     the runs differ by 2.5e-7 rad.
//   * The last step of a descent under rule E, which the other time grid would not leave as one
//     row more or less if it were shorter than the extra step: 0.31 s against the 1 ms.
//   * The step from the apogee of an Euler stepper is the one decision without a margin, and
//     has rule E.
//
// THE EXTENSIONS. The whole flights of the four simulations with extensions are compared here
// like every other: "Active roll control" and "Roll control + air-start" of "Simulation
// extensions" (RollControl, the second with AirStart), whose default-step comparison ends on
// the launch rod, before the controller starts: here their summary values, numbers of rows
// (2789 and 2740), jitter replacements and event times are compared over the 163 s and 185 s of
// their flights, and what the controller does in them: the minimum and the maximum of the roll
// rate, of the roll forcing and damping coefficients and of the fin cant that the extension
// adds as a column (see "The extremes of the columns"; a document has the extremes of a column
// only). And the two of "Simulation scripting", whose scripts are disabled.
//
// Not vacuous: ExampleSimulationStableGoldenMutation changes one golden value at a time in a
// copy of the document of a flight under no rule (every summary value, the numbers of the
// header of its branch, extremes of steady columns, the times of events, the structure of a
// branch without a time series), of a flight with a warning and a late handling, and of the
// three-stage rocket (what rule T compares of a dropped stage: the altitude of its apogee, at
// the tolerance of the rule, and its steady columns), and expects the one line that reports
// it; changes within the tolerances are not reported, among them a time after a late handling
// moved by four times the tolerance of a time before it and the optimum altitude of a dropped
// stage moved by five thousand times the tolerance of one under no rule; and a number that a
// rule excludes is counted as excluded, under its rule, and not compared. The other time grid
// of rule E is made for the test (which branch steps otherwise is the platform's to say): the
// document that a golden run on the other grid than the run's would have left matches, a row
// more or less than its one row is reported, and so is the end of the descent beyond the
// tolerances of the other grid, but not within them. ExampleSimulationStableGoldenRules holds
// rule E's reading of a run's rows and the plan of a branch to what they say, on rows and
// documents made for it, and pins every tolerance to its value: the mutations are sized by the
// tolerances, so a tolerance that grew would hide itself.
//
// The tests are cut by what a process has to simulate: one test per example (ctest starts a
// process per test, and a process reads the motor database and runs every simulation of its
// example), three for the mutations (each runs one simulation, the first of its example), two
// more on the three-stage rocket, and the ones that run nothing.

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

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenExamples.h"
#include "goldens/GoldenGeometry.h"
#include "goldens/GoldenMismatches.h"
#include "goldens/GoldenSimulations.h"
#include "goldens/GoldenStableSimulations.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using nlohmann::json;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightEvent;
using QtRocket::LoadedDocument;
using QtRocket::Result;
using QtRocket::Test::ApogeeStep;
using QtRocket::Test::apogeeStepOf;
using QtRocket::Test::apogeeStepProblem;
using QtRocket::Test::baseName;
using QtRocket::Test::columnKey;
using QtRocket::Test::compareSimulation;
using QtRocket::Test::compareStableCount;
using QtRocket::Test::compareStableNumber;
using QtRocket::Test::compareStableWarning;
using QtRocket::Test::compareStableWarnings;
using QtRocket::Test::ComparisonMode;
using QtRocket::Test::csvTypes;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::eulerApogeeRow;
using QtRocket::Test::eventTime;
using QtRocket::Test::findGoldenExample;
using QtRocket::Test::firstLateHandling;
using QtRocket::Test::goldenCounts;
using QtRocket::Test::GoldenExample;
using QtRocket::Test::goldenExampleEnvironment;
using QtRocket::Test::goldenExamples;
using QtRocket::Test::goldenExampleTestName;
using QtRocket::Test::GoldenFiles;
using QtRocket::Test::goldenFilesOf;
using QtRocket::Test::GoldenInput;
using QtRocket::Test::GoldenSimulation;
using QtRocket::Test::goldenValue;
using QtRocket::Test::isUnrecoveredDroppedStage;
using QtRocket::Test::kApogeeResidual;
using QtRocket::Test::kDeploymentVelocityRelative;
using QtRocket::Test::KickListener;
using QtRocket::Test::kKickSize;
using QtRocket::Test::kLateHandlingAbsolute;
using QtRocket::Test::kOnTime;
using QtRocket::Test::kStableSummaryRelative;
using QtRocket::Test::kStableSummaryValues;
using QtRocket::Test::kStableTimeAbsolute;
using QtRocket::Test::kStableTimeStep;
using QtRocket::Test::kToleranceMargin;
using QtRocket::Test::kValueRelative;
using QtRocket::Test::loadGoldenFiles;
using QtRocket::Test::loadSettledGoldenExample;
using QtRocket::Test::manifest;
using QtRocket::Test::Mutation;
using QtRocket::Test::orderedEvents;
using QtRocket::Test::orderedGoldenEvents;
using QtRocket::Test::referenceOf;
using QtRocket::Test::runDocumentSimulation;
using QtRocket::Test::scale;
using QtRocket::Test::Sensitivity;
using QtRocket::Test::sequenceOf;
using QtRocket::Test::shift;
using QtRocket::Test::SimulationComparison;
using QtRocket::Test::SimulationCounts;
using QtRocket::Test::SimulationRun;
using QtRocket::Test::sortedNames;
using QtRocket::Test::StableComparison;
using QtRocket::Test::StableMeasurements;
using QtRocket::Test::StableSummaryValue;
using QtRocket::Test::Subject;
using QtRocket::Test::SubjectCase;
using QtRocket::Test::subjectTestName;
using QtRocket::Test::SummaryExtent;
using QtRocket::Test::timeSeriesOf;

/// The comparison collector of the golden tests.
using Mismatches = QtRocket::Test::GoldenMismatches;

// ================================================================================== the rules

/// How everything of a simulation that is not a number of its trajectory is compared
/// (compareSimulation() of GoldenSimulations.h): as a simulation of the stable-step set, whose
/// "harness" records the two time steps, and under the rule of the tumbling dropped stages, as
/// in the default-step comparison of the examples.
constexpr ComparisonMode kStructure{.randomConfigurationId     = false,
                                    .strict                    = false,
                                    .measurements              = nullptr,
                                    .stableSet                 = true,
                                    .droppedStagesTumbleFreely = true};

/// AbstractEulerStepper.MIN_TIME_STEP, in s: the extra step that an Euler stepper takes from an
/// apogee it has landed on with a positive rest of the vertical velocity (rule E). The engine
/// records the APOGEE with the time of the row before the one that is 1 cm below the maximum:
/// the apogee row itself after the regular step, and the row this much later after the extra one.
constexpr double kExtraStep = 1e-3;

/// Rule E: a time after the APOGEE of a branch whose run steps from the apogee otherwise than
/// the golden run (its GROUND_HIT and its SIMULATION_END, and the flight time when it is the
/// first branch): within this many seconds. The descent of such a run is on a time grid that
/// the extra step has moved by 1 ms; two runs of OpenRocket on the two grids differ in these
/// times by 1.9e-4 s at most.
constexpr double kOtherGridTimeAbsolute = 2e-2;

/// Rule E: the ground hit velocity of such a run: within this fraction of itself. Two runs of
/// OpenRocket on the two grids differ in it by 9.3e-7 at most (the motor pods' flight, whose
/// landing steps fall elsewhere).
constexpr double kOtherGridVelocityRelative = 1e-4;

/// Rule T: the optimum altitude of a dropped stage without a recovery device, which is the
/// altitude of its apogee after its separation: within this fraction of itself. The stage is
/// left to its attitude from its separation on, and two runs of OpenRocket differ in the
/// altitude by 1.1e-4 at most.
constexpr double kTumblingAltitudeRelative = 2e-2;

/// The columns whose minimum and maximum are compared, at kStableSummaryRelative of the scale
/// of the column in its branch: those in which two runs agree to 2e-11 of that scale and better
/// in every branch (OpenRocket's 22 runs, QtRocket's 47; see "The extremes of the columns" at
/// the top of the file). They are what the flight does not feed back into: the mass, the
/// inertia and the centre of gravity, the thrust, the roll, which the fins and a controller
/// decide, the drag coefficients, whose extremes are those of the launch and of the fastest
/// flight, and what the site sets. In a branch that is not that of a dropped stage without a
/// recovery device (rule T):
constexpr std::array<std::string_view, 23> kSteadyColumns{"mass",
                                                          "motor_mass",
                                                          "longitudinal_inertia",
                                                          "rotational_inertia",
                                                          "cg_location",
                                                          "thrust_force",
                                                          "thrust_correction",
                                                          "thrust_weight_ratio",
                                                          "roll_rate",
                                                          "roll_forcing_coeff",
                                                          "roll_damping_coeff",
                                                          "custom:Control fin cant",
                                                          "drag_coeff",
                                                          "base_drag_coeff",
                                                          "pressure_drag_coeff",
                                                          "side_force_coeff",
                                                          "yaw_moment_coeff",
                                                          "gravity",
                                                          "latitude",
                                                          "wind_velocity",
                                                          "wind_direction",
                                                          "reference_length",
                                                          "reference_area"};

/// ... and in the branch of such a stage, whose extremes after its separation are those of a
/// flight that two runs do not reproduce: what the stage has before it tumbles, or whatever it
/// does.
constexpr std::array<std::string_view, 20> kSteadyColumnsOfATumblingStage{"mass",
                                                                          "motor_mass",
                                                                          "longitudinal_inertia",
                                                                          "rotational_inertia",
                                                                          "cg_location",
                                                                          "thrust_force",
                                                                          "thrust_correction",
                                                                          "thrust_weight_ratio",
                                                                          "velocity_total",
                                                                          "coriolis_acceleration",
                                                                          "roll_rate",
                                                                          "roll_moment_coeff",
                                                                          "roll_forcing_coeff",
                                                                          "roll_damping_coeff",
                                                                          "side_force_coeff",
                                                                          "yaw_moment_coeff",
                                                                          "wind_velocity",
                                                                          "wind_direction",
                                                                          "reference_length",
                                                                          "reference_area"};

/// Whether the minimum and the maximum of the column @p key are compared in a branch that is
/// (@p tumbles) or is not that of a dropped stage without a recovery device.
[[nodiscard]] bool hasSteadyExtremes(std::string_view key, bool tumbles)
{
    const std::span<const std::string_view> keys =
        tumbles ? std::span<const std::string_view>{kSteadyColumnsOfATumblingStage}
                : std::span<const std::string_view>{kSteadyColumns};
    return std::ranges::find(keys, key) != keys.end();
}

/// Rule E: a branch whose recovery device is out before its apogee, so that an Euler stepper
/// lands a step on the apogee, and the step the golden run took from there: the extra one of
/// 1 ms (@p shortStep), or the regular one.
struct EulerApogee
{
    std::string_view input;       ///< the golden input
    std::int64_t     simulation;  ///< the index of the simulation in it
    std::size_t      branch;      ///< the index of the branch
    bool             shortStep;
};

// clang-format off
constexpr std::array<EulerApogee, 16> kEulerApogees{{
    {.input = "example-3d-printable-nose-cone-and-fins", .simulation = 1, .branch = 0, .shortStep = true},
    {.input = "example-3d-printable-nose-cone-and-fins", .simulation = 2, .branch = 0, .shortStep = false},
    {.input = "example-3d-printable-nose-cone-and-fins", .simulation = 3, .branch = 0, .shortStep = false},
    {.input = "example-a-simple-model-rocket", .simulation = 1, .branch = 0, .shortStep = false},
    {.input = "example-a-simple-model-rocket", .simulation = 2, .branch = 0, .shortStep = false},
    {.input = "example-a-simple-model-rocket", .simulation = 3, .branch = 0, .shortStep = false},
    {.input = "example-clustered-motors", .simulation = 1, .branch = 0, .shortStep = false},
    {.input = "example-clustered-motors", .simulation = 2, .branch = 0, .shortStep = false},
    {.input = "example-clustered-motors", .simulation = 3, .branch = 0, .shortStep = false},
    {.input = "example-deployable-payload", .simulation = 2, .branch = 0, .shortStep = false},
    {.input = "example-deployable-payload", .simulation = 2, .branch = 1, .shortStep = false},
    {.input = "example-deployable-payload", .simulation = 3, .branch = 0, .shortStep = false},
    {.input = "example-deployable-payload", .simulation = 3, .branch = 1, .shortStep = false},
    {.input = "example-pods-powered-with-recovery-deployment", .simulation = 0, .branch = 0, .shortStep = false},
    {.input = "example-three-stage-low-power-rocket", .simulation = 0, .branch = 0, .shortStep = true},
    {.input = "example-three-stage-low-power-rocket", .simulation = 1, .branch = 0, .shortStep = false},
}};
// clang-format on

/// Rule L: a branch with a late handling, and the first of its events (by its place among the
/// golden events, and its type) that the golden run handled late.
struct LateHandling
{
    std::string_view input;
    std::int64_t     simulation;
    std::size_t      branch;
    std::size_t      event;
    std::string_view type;
};

// clang-format off
constexpr std::array<LateHandling, 26> kLateHandlings{{
    {.input = "example-airstart-timing", .simulation = 0, .branch = 0, .event = 8, .type = "RECOVERY_DEVICE_DEPLOYMENT"},
    {.input = "example-airstart-timing", .simulation = 1, .branch = 0, .event = 8, .type = "RECOVERY_DEVICE_DEPLOYMENT"},
    {.input = "example-airstart-timing", .simulation = 2, .branch = 0, .event = 8, .type = "RECOVERY_DEVICE_DEPLOYMENT"},
    {.input = "example-airstart-timing", .simulation = 3, .branch = 0, .event = 8, .type = "RECOVERY_DEVICE_DEPLOYMENT"},
    {.input = "example-airstart-timing", .simulation = 4, .branch = 0, .event = 8, .type = "RECOVERY_DEVICE_DEPLOYMENT"},
    {.input = "example-deployable-payload", .simulation = 1, .branch = 0, .event = 4, .type = "BURNOUT"},
    {.input = "example-deployable-payload", .simulation = 1, .branch = 1, .event = 1, .type = "BURNOUT"},
    {.input = "example-dual-parachute-deployment", .simulation = 0, .branch = 0, .event = 6, .type = "RECOVERY_DEVICE_DEPLOYMENT"},
    {.input = "example-dual-parachute-deployment", .simulation = 1, .branch = 0, .event = 6, .type = "RECOVERY_DEVICE_DEPLOYMENT"},
    {.input = "example-dual-parachute-deployment", .simulation = 2, .branch = 0, .event = 4, .type = "SIM_WARN"},
    {.input = "example-dual-parachute-deployment", .simulation = 3, .branch = 0, .event = 6, .type = "RECOVERY_DEVICE_DEPLOYMENT"},
    {.input = "example-dual-parachute-deployment", .simulation = 4, .branch = 0, .event = 6, .type = "RECOVERY_DEVICE_DEPLOYMENT"},
    {.input = "example-dual-parachute-deployment", .simulation = 5, .branch = 0, .event = 6, .type = "RECOVERY_DEVICE_DEPLOYMENT"},
    {.input = "example-pods-airframes-and-winglets", .simulation = 1, .branch = 0, .event = 4, .type = "BURNOUT"},
    {.input = "example-simulation-extensions", .simulation = 0, .branch = 0, .event = 6, .type = "RECOVERY_DEVICE_DEPLOYMENT"},
    {.input = "example-simulation-extensions", .simulation = 1, .branch = 0, .event = 6, .type = "RECOVERY_DEVICE_DEPLOYMENT"},
    {.input = "example-simulation-extensions", .simulation = 2, .branch = 0, .event = 7, .type = "RECOVERY_DEVICE_DEPLOYMENT"},
    {.input = "example-simulation-scripting", .simulation = 0, .branch = 0, .event = 6, .type = "RECOVERY_DEVICE_DEPLOYMENT"},
    {.input = "example-simulation-scripting", .simulation = 1, .branch = 0, .event = 6, .type = "RECOVERY_DEVICE_DEPLOYMENT"},
    {.input = "example-simulation-scripting", .simulation = 2, .branch = 0, .event = 6, .type = "RECOVERY_DEVICE_DEPLOYMENT"},
    {.input = "example-three-stage-low-power-rocket", .simulation = 0, .branch = 0, .event = 8, .type = "BURNOUT"},
    {.input = "example-three-stage-low-power-rocket", .simulation = 0, .branch = 1, .event = 1, .type = "BURNOUT"},
    {.input = "example-two-stage-high-power-rocket", .simulation = 0, .branch = 0, .event = 8, .type = "BURNOUT"},
    {.input = "example-two-stage-high-power-rocket", .simulation = 0, .branch = 1, .event = 6, .type = "RECOVERY_DEVICE_DEPLOYMENT"},
    {.input = "example-two-stage-high-power-rocket", .simulation = 1, .branch = 0, .event = 9, .type = "RECOVERY_DEVICE_DEPLOYMENT"},
    {.input = "example-two-stage-high-power-rocket", .simulation = 1, .branch = 1, .event = 5, .type = "RECOVERY_DEVICE_DEPLOYMENT"},
}};
// clang-format on

/// Rule T: the branch of a dropped stage that flies on without a recovery device.
struct TumblingStage
{
    std::string_view input;
    std::int64_t     simulation;
    std::size_t      branch;
};

// clang-format off
constexpr std::array<TumblingStage, 7> kTumblingStages{{
    {.input = "example-parallel-booster-staging", .simulation = 0, .branch = 1},
    {.input = "example-three-stage-low-power-rocket", .simulation = 0, .branch = 1},
    {.input = "example-three-stage-low-power-rocket", .simulation = 0, .branch = 2},
    {.input = "example-three-stage-low-power-rocket", .simulation = 1, .branch = 1},
    {.input = "example-three-stage-low-power-rocket", .simulation = 1, .branch = 2},
    {.input = "example-three-stage-low-power-rocket", .simulation = 2, .branch = 1},
    {.input = "example-three-stage-low-power-rocket", .simulation = 2, .branch = 2},
}};
// clang-format on

/// The entry of the list @p list for branch @p branch of the simulation @p simulation of the
/// golden input @p input; null when it lists none.
template <class Entry, std::size_t N>
[[nodiscard]] const Entry* listed(const std::array<Entry, N>& list, std::string_view input,
                                  std::int64_t simulation, std::size_t branch)
{
    const std::span<const Entry> entries{list};
    const auto                   found = std::ranges::find_if(entries, [&](const Entry& entry) {
        return entry.input == input && entry.simulation == simulation && entry.branch == branch;
    });
    return found == entries.end() ? nullptr : &*found;
}

// ------------------------------------------------------------------------------------- plans

/// What the document of a golden simulation and the lists of the rules say about how one of
/// its branches is compared: nothing in it depends on the run it is compared with.
struct DocumentPlan
{
    /// Rule T: whether the branch is that of a dropped stage without a recovery device
    /// (isUnrecoveredDroppedStage() of GoldenSimulations.h, which is what the comparison of the
    /// structure goes by): it is compared up to its last stage separation.
    bool tumbles{false};
    /// Rule E: whether the golden run took the extra step of 1 ms from the apogee of the branch,
    /// or the regular one (kEulerApogees); nullopt for a branch that is not listed.
    std::optional<bool> shortApogeeStep;
    /// Rule L: the time of the first event of the branch that the golden run handled late
    /// (kLateHandlings): the events after it are compared at kLateHandlingAbsolute. Infinite
    /// for a branch that is not listed.
    double lateFrom{std::numeric_limits<double>::infinity()};
};

/// The plan of branch @p index of the golden simulation @p document.
[[nodiscard]] DocumentPlan documentPlan(const json& document, std::size_t index)
{
    const json&       branch     = document.at("branches").at(index);
    const std::string input      = document.at("input").get<std::string>();
    const auto        simulation = document.at("index").get<std::int64_t>();
    const json&       events     = branch.at("events");
    DocumentPlan      plan;
    plan.tumbles              = isUnrecoveredDroppedStage(branch);
    const EulerApogee* apogee = listed(kEulerApogees, input, simulation, index);
    if (apogee != nullptr)
    {
        plan.shortApogeeStep = apogee->shortStep;
    }
    const LateHandling* late = listed(kLateHandlings, input, simulation, index);
    if (late != nullptr && late->event < events.size())
    {
        plan.lateFrom = goldenValue(events.at(late->event).at("time"));
    }
    return plan;
}

/// Rule E: what the rows of a run say of the step it takes from the apogee of a branch.
struct ApogeeOutcome
{
    /// Whether the run steps from the apogee as the golden run did (true as well for a branch
    /// the rule does not list): everything of the branch is compared then.
    bool alike{true};
    /// By how much later than the golden run the run records its APOGEE when the two step
    /// otherwise: kExtraStep when the run took the extra step and the golden run did not, as
    /// much earlier the other way round, 0 when they step alike.
    double later{0};
    /// Why the rows of the run do not show the decision by the last bit that the rule allows
    /// for (apogeeStepProblem()); "" when they do, and for a branch the rule does not list.
    std::string problem;
    ApogeeStep  step;  ///< the step of the run, for the measurements

    /// Whether the rows of the run show the other step from the apogee than the golden run
    /// took: the descent of the run is then on the other time grid. (A run whose rows show no
    /// step from an apogee at all is not alike either; it is a mismatch, and is compared like
    /// a run that steps alike.)
    [[nodiscard]] bool otherGrid() const noexcept { return !alike && later != 0; }

    /// The rows the run has more than the golden run for it: the one of the extra step that
    /// the run took and the golden run did not, or one less the other way round; 0 for a run
    /// that steps alike.
    [[nodiscard]] std::int64_t extraRows() const noexcept
    {
        if (alike || later == 0)
        {
            return 0;
        }
        return later > 0 ? 1 : -1;
    }
};

/// The outcome of rule E for the branch @p branch of a run, whose golden run took the step
/// @p shortStep from its apogee (the short one, or the regular one; nullopt: the branch is not
/// one of the rule).
[[nodiscard]] ApogeeOutcome apogeeOutcome(const std::optional<bool>& shortStep,
                                          const FlightDataBranch&    branch)
{
    ApogeeOutcome outcome;
    if (!shortStep.has_value())
    {
        return outcome;
    }
    const std::optional<std::size_t> row = eulerApogeeRow(timeSeriesOf(branch));
    if (!row.has_value())
    {
        outcome.alike   = false;
        outcome.problem = "no step of an Euler stepper lands on the apogee";
        return outcome;
    }
    outcome.step    = apogeeStepOf(branch, *row);
    outcome.problem = apogeeStepProblem(outcome.step);
    outcome.alike   = !std::isnan(outcome.step.step) && outcome.step.isShort() == *shortStep;
    if (!outcome.alike && !std::isnan(outcome.step.step))
    {
        outcome.later = outcome.step.isShort() ? kExtraStep : -kExtraStep;
    }
    return outcome;
}

// -------------------------------------------------------------------------------- comparison

/// The numbers of the golden documents that the rules exclude from the comparison, by rule.
struct Exclusions
{
    /// The minimum and the maximum of every column that is not one of kSteadyColumns (in the
    /// branch of a dropped stage without a recovery device: of kSteadyColumnsOfATumblingStage).
    int extremes{0};
    /// Rule T: of a dropped stage without a recovery device, its number of rows, the time to
    /// its optimum altitude and its optimum delay, and the times of its events after its last
    /// separation; and the number of jitter replacements of its simulation.
    int tumbling{0};

    [[nodiscard]] bool operator==(const Exclusions&) const = default;

    Exclusions& operator+=(const Exclusions& other)
    {
        extremes += other.extremes;
        tumbling += other.tumbling;
        return *this;
    }

    /// The numbers the rules exclude in all.
    [[nodiscard]] int total() const noexcept { return extremes + tumbling; }
};

/// "6652 extremes, 53 by rule T", for the messages of the tests.
[[nodiscard]] std::string toText(const Exclusions& exclusions)
{
    return std::format("{} extremes, {} by rule T", exclusions.extremes, exclusions.tumbling);
}

/// toText() of @p exclusions, for the messages of the tests.
std::ostream& operator<<(std::ostream& out, const Exclusions& exclusions)
{
    return out << toText(exclusions);
}

/// What the comparison of a run with the document of its golden simulation works with and
/// collects.
struct DocumentComparison
{
    StableComparison c;
    Exclusions       excluded;   ///< the numbers the rules exclude, by rule (their sum is c's)
    int              unlike{0};  ///< rule E: the branches whose run steps otherwise
    /// Rule E: the numbers that were compared as those of a run on the other time grid: of a
    /// branch whose run steps from the apogee otherwise than the golden run, its number of rows
    /// (with the row of the extra step) and the times of the events after its APOGEE; and,
    /// when it is the first branch, the flight time and the ground hit velocity.
    int otherGrid{0};
    /// The minima and maxima of columns that were compared (kSteadyColumns).
    int steadyExtremes{0};
    /// Rule E: the steps the golden run took from the apogees of its branches
    /// (DocumentPlan::shortApogeeStep, by the index of the branch), when they are not those of
    /// kEulerApogees: of a run that stands in for the golden files (the sensitivity measurement).
    /// Null: those of the list.
    const std::vector<std::optional<bool>>* steps{nullptr};
};

/// The number of numbers of @p d that a rule excluded since they were @p before, added to
/// @p rule.
void attribute(const DocumentComparison& d, int before, int& rule)
{
    rule += d.c.excluded.numbers - before;
}

/// What the comparison of a branch works with.
struct DocumentBranch
{
    const FlightData*       data{nullptr};
    const FlightDataBranch* branch{nullptr};
    DocumentPlan            plan;
    ApogeeOutcome           apogee;
};

/// The number of the first events of @p events up to the last one that @p isSeparation says is
/// a stage separation; 0 when none is.
template <class Event, class IsSeparation>
[[nodiscard]] std::size_t eventsUpToTheLastSeparation(const std::vector<Event>& events,
                                                      const IsSeparation&       isSeparation)
{
    std::size_t flown = 0;
    for (std::size_t i = 0; i < events.size(); i++)
    {
        flown = isSeparation(events[i]) ? i + 1 : flown;
    }
    return flown;
}

/// Whether the golden event @p event is of the type @p type.
[[nodiscard]] bool isOfType(const json* event, std::string_view type)
{
    return event->at("type").get<std::string>() == type;
}

/// The place of the first APOGEE among the golden events @p events; their number without one.
[[nodiscard]] std::size_t placeOfTheApogee(std::span<const json* const> events)
{
    const auto apogee =
        std::ranges::find_if(events, [](const json* event) { return isOfType(event, "APOGEE"); });
    return static_cast<std::size_t>(apogee - events.begin());
}

/// Whether the golden event @p event has a warning with a parameter.
[[nodiscard]] bool hasParameter(const json* event)
{
    const json& data = event->at("data");
    return data.is_object() && data.contains("parameter");
}

/// The numbers of the golden events @p events from the place @p from on: their times, and the
/// parameters of their warnings.
[[nodiscard]] int numbersFrom(const std::vector<const json*>& events, std::size_t from)
{
    int numbers = 0;
    for (std::size_t i = from; i < events.size(); i++)
    {
        numbers += hasParameter(events[i]) ? 2 : 1;
    }
    return numbers;
}

/// The number of the events of @p events after the place @p place (none when there are no more
/// than @p place of them).
[[nodiscard]] int eventsAfter(const std::vector<const json*>& events, std::size_t place)
{
    return place < events.size() ? static_cast<int>(events.size() - place - 1) : 0;
}

/// Records, for the table of the measurements, by how much the number @p actual differs from
/// its golden number @p expected, as a fraction of @p unit, when the rules exclude it from the
/// comparison of @p c: @p what names it.
void measureExcluded(const StableComparison& c, const std::string& what, double expected,
                     double actual, double unit)
{
    if (c.measurements != nullptr && std::isfinite(expected) && std::isfinite(actual) && unit > 0)
    {
        c.measurements->record(what, std::abs(actual - expected) / unit, -1.0, c.context);
    }
}

/// The golden events of a branch and the events of the run that are compared one with one:
/// those of both sides in the order of the comparison, of a dropped stage without a recovery
/// device (rule T) those up to its last stage separation.
struct PairedEvents
{
    std::vector<const json*>        golden;
    std::vector<const FlightEvent*> ours;
    std::size_t                     apogee{0};  ///< placeOfTheApogee() of the golden events
    /// Rule T: the golden events after the last stage separation, which are not compared (they
    /// follow the paired ones, in the order of the comparison), and those of the run after its
    /// own.
    std::vector<const json*>        goldenAfter;
    std::vector<const FlightEvent*> oursAfter;
};

/// The events of @p ours and the golden events @p expected that are compared one with one, and
/// those after the last separation of a dropped stage without a recovery device.
[[nodiscard]] PairedEvents pairedEvents(const json& expected, const DocumentBranch& ours)
{
    PairedEvents paired{.golden      = orderedGoldenEvents(expected, false),
                        .ours        = orderedEvents(*ours.branch, false),
                        .apogee      = 0,
                        .goldenAfter = {},
                        .oursAfter   = {}};
    if (ours.plan.tumbles)
    {
        const std::size_t flown = eventsUpToTheLastSeparation(
            paired.golden, [](const json* event) { return isOfType(event, "STAGE_SEPARATION"); });
        const std::size_t flownByTheRun =
            eventsUpToTheLastSeparation(paired.ours, [](const FlightEvent* event) {
                return event->getType() == FlightEvent::Type::STAGE_SEPARATION;
            });
        const std::span<const json* const> golden =
            std::span<const json* const>{paired.golden}.subspan(flown);
        const std::span<const FlightEvent* const> run =
            std::span<const FlightEvent* const>{paired.ours}.subspan(flownByTheRun);
        paired.goldenAfter.assign(golden.begin(), golden.end());
        paired.oursAfter.assign(run.begin(), run.end());
        paired.golden.resize(flown);
        paired.ours.resize(flownByTheRun);
    }
    paired.apogee = placeOfTheApogee(paired.golden);
    return paired;
}

/// The time of the first APOGEE among the events @p events of a run; NaN without one.
[[nodiscard]] double apogeeTimeOf(const std::vector<const FlightEvent*>& events)
{
    for (const FlightEvent* event : events)
    {
        if (event->getType() == FlightEvent::Type::APOGEE)
        {
            return event->getTime();
        }
    }
    return std::numeric_limits<double>::quiet_NaN();
}

/// Rule T: counts the times of what a dropped stage without a recovery device records after its
/// last separation as excluded (its APOGEE, a TUMBLE, which the structure leaves out on both
/// sides, its GROUND_HIT and its SIMULATION_END, and the parameter of a warning of one of them,
/// if it had one), and records for the table of the measurements by how much the time of its
/// APOGEE differs from the golden one.
void countTheFlightOfATumblingStage(DocumentComparison& d, const PairedEvents& paired)
{
    const int numbers = numbersFrom(paired.goldenAfter, 0);
    d.c.excluded.numbers += numbers;
    d.excluded.tumbling += numbers;
    const std::size_t apogee = placeOfTheApogee(paired.goldenAfter);
    if (apogee < paired.goldenAfter.size())
    {
        measureExcluded(d.c, "excluded by rule T: event:APOGEE of a dropped stage",
                        goldenValue(paired.goldenAfter[apogee]->at("time")),
                        apogeeTimeOf(paired.oursAfter), 1.0);
    }
}

/// How the time of one golden event is compared with the time of the event of the run.
struct EventRule
{
    std::string field;             ///< names it in a mismatch
    std::string what;              ///< names it in the measurements
    double      shift{0};          ///< what the golden time is moved by (rule E: the extra step)
    double      tolerance{0};      ///< in s
    bool        otherGrid{false};  ///< rule E: an event after the apogee of a run on the other grid
};

/// The rule for the golden event at the place @p place of the paired events @p paired, whose
/// type is @p type and whose time is @p time, in the branch @p ours. An event after the late
/// handling of its branch is compared at kLateHandlingAbsolute (rule L). Of a branch whose run
/// steps from the apogee otherwise than the golden run (rule E), the APOGEE is compared with
/// the golden one moved by the extra step that one of the two runs took (ApogeeOutcome::later),
/// and the events after it at kOtherGridTimeAbsolute.
[[nodiscard]] EventRule eventRule(const DocumentBranch& ours, std::size_t place, std::size_t apogee,
                                  const std::string& type, double time)
{
    const bool late = time > ours.plan.lateFrom;
    EventRule  rule{.field = std::format("events[{}].time", place),
                    .what  = std::format("event{}:{}", late ? " after a late handling" : "", type),
                    .shift = 0,
                    .tolerance = late ? kLateHandlingAbsolute : kStableTimeAbsolute,
                    .otherGrid = false};
    if (!ours.apogee.otherGrid() || place < apogee)
    {
        return rule;
    }
    if (place == apogee)
    {
        rule.field += ", moved by the extra step of rule E";
        rule.what += " of a run that steps otherwise, less the extra step";
        rule.shift = ours.apogee.later;
        return rule;
    }
    rule.what      = std::format("event after the apogee of a run that steps otherwise:{}", type);
    rule.tolerance = kOtherGridTimeAbsolute;
    rule.otherGrid = true;
    return rule;
}

/// Compares the times of the events of the branch with the golden "events", and what their
/// warnings print of their parameters. The sequence of the events (their types and sources)
/// and their data are compared with the structure: when the sequences differ, nothing is
/// compared here. Each time is compared under its rule (eventRule()). Of a dropped stage
/// without a recovery device (rule T), the events up to its last separation are compared like
/// every other, and the later ones are not (countTheFlightOfATumblingStage()).
void compareDocumentEvents(Mismatches& m, DocumentComparison& d, const json& expected,
                           const DocumentBranch& ours)
{
    const PairedEvents paired = pairedEvents(expected, ours);
    if (sequenceOf(paired.golden) != sequenceOf(paired.ours))
    {
        // The structure reports it; the numbers are counted under no rule.
        d.c.excluded.numbers += numbersFrom(paired.golden, 0) + numbersFrom(paired.goldenAfter, 0);
        return;
    }
    for (std::size_t i = 0; i < paired.golden.size(); i++)
    {
        const json&     golden = *paired.golden[i];
        const double    time   = goldenValue(golden.at("time"));
        const EventRule rule =
            eventRule(ours, i, paired.apogee, golden.at("type").get<std::string>(), time);
        compareStableNumber(m, d.c,
                            {.field     = rule.field,
                             .what      = rule.what,
                             .expected  = time + rule.shift,
                             .actual    = paired.ours[i]->getTime(),
                             .tolerance = rule.tolerance,
                             .unit      = 1.0},
                            true);
        d.otherGrid += rule.otherGrid ? 1 : 0;
        compareStableWarning(m, d.c, std::format("events[{}].data", i), golden.at("data"),
                             ours.data->findWarning(*paired.ours[i]));
    }
    countTheFlightOfATumblingStage(d, paired);
}

/// The minimum or the maximum of a column of a branch against the golden one.
struct DocumentExtreme
{
    std::string field;  ///< "columns[26].min (mass)"
    std::string what;   ///< names it in the measurements
    double      expected{0};
    double      actual{0};
    double      scale{0};  ///< of the column in the golden branch
};

/// Compares the extreme @p extreme with its golden value, within kStableSummaryRelative of the
/// scale of its column, when its column is a steady one (@p steady); the extreme of any other
/// column is counted as excluded (and its difference recorded for the table of the
/// measurements).
void compareDocumentExtreme(Mismatches& m, DocumentComparison& d, const DocumentExtreme& extreme,
                            bool steady)
{
    const double unit   = referenceOf(extreme.scale, extreme.expected, extreme.actual);
    const int    before = d.c.excluded.numbers;
    compareStableNumber(m, d.c,
                        {.field     = extreme.field,
                         .what      = extreme.what,
                         .expected  = extreme.expected,
                         .actual    = extreme.actual,
                         .tolerance = kStableSummaryRelative * unit,
                         .unit      = unit},
                        steady);
    attribute(d, before, d.excluded.extremes);
    d.steadyExtremes += steady ? 1 : 0;
    if (!steady)
    {
        measureExcluded(d.c, extreme.what, extreme.expected, extreme.actual, unit);
    }
}

/// Compares the minimum and the maximum of the columns of the branch with the golden "columns"
/// @p columns: those of the columns of kSteadyColumns (of a dropped stage without a recovery
/// device: of kSteadyColumnsOfATumblingStage), each within kStableSummaryRelative of the scale
/// of its column in the golden branch, which is the larger magnitude of the golden minimum and
/// maximum. The extremes of the other columns are not compared: they are counted as excluded.
/// (The columns themselves, their keys, names and symbols in order, are compared with the
/// structure; a column that is not the golden one there is counted under no rule.)
void compareDocumentExtremes(Mismatches& m, DocumentComparison& d, const json& columns,
                             const DocumentBranch& ours)
{
    const std::vector<const FlightDataType*> types = csvTypes(*ours.branch);
    for (std::size_t i = 0; i < columns.size(); i++)
    {
        const json&       column = columns.at(i);
        const std::string key    = column.at("key").get<std::string>();
        if (i >= types.size() || columnKey(*types[i]) != key)
        {
            d.c.excluded.numbers += 2;
            continue;
        }
        const bool        steady    = hasSteadyExtremes(key, ours.plan.tumbles);
        const double      goldenMin = goldenValue(column.at("min"));
        const double      goldenMax = goldenValue(column.at("max"));
        const double      scale     = referenceOf(0, goldenMin, goldenMax);
        const std::string what =
            std::format("{}{}:{}", steady ? "extreme" : "excluded extreme",
                        ours.plan.tumbles ? " of a dropped stage under rule T" : "", key);
        compareDocumentExtreme(m, d,
                               {.field    = std::format("columns[{}].min ({})", i, key),
                                .what     = what,
                                .expected = goldenMin,
                                .actual   = ours.branch->getMinimum(*types[i]),
                                .scale    = scale},
                               steady);
        compareDocumentExtreme(m, d,
                               {.field    = std::format("columns[{}].max ({})", i, key),
                                .what     = what,
                                .expected = goldenMax,
                                .actual   = ours.branch->getMaximum(*types[i]),
                                .scale    = scale},
                               steady);
    }
}

/// Compares the numbers of the header of the branch with the golden branch @p expected: its
/// number of rows, its optimum altitude, the time to it and its optimum delay, its separation
/// time, and the minimum and the maximum of its steady columns (compareDocumentExtremes()).
/// - Rule T, a dropped stage without a recovery device: its number of rows, the time to its
///   optimum altitude and its optimum delay are not compared, and its optimum altitude, which
///   is the altitude of its apogee after its separation, is compared at
///   kTumblingAltitudeRelative.
/// - Rule E, a branch whose run steps from the apogee otherwise than the golden run: the run
///   has the row of the extra step more than the golden run, or less
///   (ApogeeOutcome::extraRows()).
void compareDocumentBranchHeader(Mismatches& m, DocumentComparison& d, const json& expected,
                                 const DocumentBranch& ours)
{
    const FlightDataBranch& branch  = *ours.branch;
    const bool              tumbles = ours.plan.tumbles;
    const std::int64_t      extra   = tumbles ? 0 : ours.apogee.extraRows();
    int                     before  = d.c.excluded.numbers;
    compareStableCount(m, d.c,
                       extra == 0 ? "rows" : "rows, with the row of the extra step of rule E",
                       expected.at("rows").get<std::int64_t>() + extra,
                       static_cast<std::int64_t>(branch.getLength()), !tumbles);
    attribute(d, before, d.excluded.tumbling);
    d.otherGrid += extra != 0 ? 1 : 0;
    const double altitude   = goldenValue(expected.at("optimumAltitude"));
    const double unit       = referenceOf(0, altitude, branch.getOptimumAltitude());
    const double ofAltitude = tumbles ? kTumblingAltitudeRelative : kStableSummaryRelative;
    compareStableNumber(m, d.c,
                        {.field = "optimumAltitude",
                         .what  = tumbles ? "branch of a dropped stage under rule T:optimumAltitude"
                                          : "branch:optimumAltitude",
                         .expected  = altitude,
                         .actual    = branch.getOptimumAltitude(),
                         .tolerance = ofAltitude * unit,
                         .unit      = unit},
                        true);
    const double timeToIt = goldenValue(expected.at("timeToOptimumAltitude"));
    const double delay    = goldenValue(expected.at("optimumDelay"));
    before                = d.c.excluded.numbers;
    compareStableNumber(m, d.c,
                        {.field     = "timeToOptimumAltitude",
                         .what      = "branch:timeToOptimumAltitude",
                         .expected  = timeToIt,
                         .actual    = branch.getTimeToOptimumAltitude(),
                         .tolerance = kStableTimeAbsolute,
                         .unit      = 1.0},
                        !tumbles);
    compareStableNumber(m, d.c,
                        {.field     = "optimumDelay",
                         .what      = "branch:optimumDelay",
                         .expected  = delay,
                         .actual    = branch.getOptimumDelay(),
                         .tolerance = kStableTimeAbsolute,
                         .unit      = 1.0},
                        !tumbles);
    attribute(d, before, d.excluded.tumbling);
    if (tumbles)
    {
        measureExcluded(d.c, "excluded by rule T: branch:timeToOptimumAltitude of a dropped stage",
                        timeToIt, branch.getTimeToOptimumAltitude(), 1.0);
        measureExcluded(d.c, "excluded by rule T: branch:optimumDelay of a dropped stage", delay,
                        branch.getOptimumDelay(), 1.0);
    }
    // The time of the STAGE_SEPARATION event (NaN without one): compared like that event's.
    const double separation = goldenValue(expected.at("separationTime"));
    const bool   late       = separation > ours.plan.lateFrom;
    compareStableNumber(
        m, d.c,
        {.field    = "separationTime",
         .what     = late ? "branch:separationTime after a late handling" : "branch:separationTime",
         .expected = separation,
         .actual   = branch.getSeparationTime(),
         .tolerance = late ? kLateHandlingAbsolute : kStableTimeAbsolute,
         .unit      = 1.0},
        true);
    compareDocumentExtremes(m, d, expected.at("columns"), ours);
}

/// The times of the events of @p branch, in the form of golden events (for firstLateHandling()).
[[nodiscard]] json eventTimesOf(const FlightDataBranch& branch)
{
    json events = json::array();
    for (const FlightEvent& event : branch.getEvents())
    {
        events.push_back({{"time", event.getTime()}});
    }
    return events;
}

/// Records, for the table of the measurements, what the rules read from the rows of the run:
/// what its step to the apogee of an Euler stepper left of the vertical velocity, as a fraction
/// of that of the row before, whether it steps on as the golden run did, and what the last step
/// of its descent has over the extra step (rule E: the extra step moves the time grid of the
/// descent by 1 ms, and the run has exactly its one row more as long as the last step, which
/// ends on the ground, is longer than that on the grid without it); whether the first of its
/// events without a row at its time is the one the golden run handled late (rule L); and by how
/// many rows it differs from the golden run.
void measureTheRows(const StableComparison& c, const json& expected, const DocumentBranch& ours,
                    std::size_t index)
{
    const std::string         where = std::format("{} branch {}", c.context, index);
    const std::vector<double> times =
        timeSeriesOf(*ours.branch).column("time").value_or(std::vector<double>{});
    if (ours.plan.shortApogeeStep.has_value())
    {
        c.measurements->record("rule E: the run steps from the apogee otherwise (1) or alike (0)",
                               ours.apogee.alike ? 0.0 : 1.0, -1.0, where);
    }
    if (ours.plan.shortApogeeStep.has_value() && std::abs(ours.apogee.step.before) > 0)
    {
        c.measurements->record("rule E: velocity left / velocity before",
                               std::abs(ours.apogee.step.left / ours.apogee.step.before),
                               kApogeeResidual, where);
    }
    if (ours.plan.shortApogeeStep.has_value() && times.size() > 1)
    {
        // On the grid without the extra step the last step is 1 ms longer than on the other.
        const double last = times.back() - times[times.size() - 2];
        const double over = ours.apogee.step.isShort() ? last : last - kExtraStep;
        c.measurements->record(
            "rule E: the extra step / what the last step of the descent has over it",
            over > 0 ? kExtraStep / over : std::numeric_limits<double>::infinity(), -1.0, where);
    }
    const double late   = firstLateHandling(eventTimesOf(*ours.branch), times);
    const bool   golden = std::isfinite(ours.plan.lateFrom);
    const bool   same   = golden == std::isfinite(late) &&
                          (!golden || std::abs(late - ours.plan.lateFrom) <= kLateHandlingAbsolute);
    c.measurements->record("rule L: the first late handling of the run is not the golden run's (1)",
                           same ? 0.0 : 1.0, -1.0, where);
    const double rows = static_cast<double>(ours.branch->getLength()) -
                        static_cast<double>(expected.at("rows").get<std::int64_t>());
    c.measurements->record(
        ours.plan.tumbles ? "rows of a dropped stage under rule T: difference" : "rows: difference",
        std::abs(rows), -1.0, where);
}

/// What the comparison of a branch says of it to the comparison of its simulation.
struct DocumentBranchOutcome
{
    /// Rule E: its run steps from the apogee otherwise than the golden run, so that its descent
    /// is on the other time grid (ApogeeOutcome::otherGrid()).
    bool otherGrid{false};
    bool tumbles{false};  ///< it is a dropped stage without a recovery device (rule T)
};

/// Compares the numbers of branch @p index of @p run with its golden branch of @p golden.
[[nodiscard]] DocumentBranchOutcome compareDocumentBranch(DocumentComparison& d, std::size_t index,
                                                          const GoldenFiles&   golden,
                                                          const SimulationRun& run)
{
    const json&    expected = golden.document.at("branches").at(index);
    Mismatches     m(std::format("{} branch {}", d.c.context, index));
    DocumentBranch ours{.data   = run.data.get(),
                        .branch = &run.data->getBranch(index),
                        .plan   = documentPlan(golden.document, index),
                        .apogee = {}};
    if (d.steps != nullptr && index < d.steps->size())
    {
        ours.plan.shortApogeeStep = (*d.steps)[index];
    }
    ours.apogee = apogeeOutcome(ours.plan.shortApogeeStep, *ours.branch);
    if (!ours.apogee.problem.empty())
    {
        // Rule E does not excuse this step from the apogee: the run does not match.
        m.note(std::format("the apogee: {} (rule E)", ours.apogee.problem));
    }
    d.unlike += ours.apogee.alike ? 0 : 1;
    compareDocumentBranchHeader(m, d, expected, ours);
    compareDocumentEvents(m, d, expected.at("events"), ours);
    if (d.c.measurements != nullptr)
    {
        measureTheRows(d.c, expected, ours, index);
    }
    d.c.report += m.report();
    return {.otherGrid = ours.apogee.otherGrid() && !ours.plan.tumbles,
            .tumbles   = ours.plan.tumbles};
}

/// The scale of column @p key of the first branch of the golden simulation @p document: the
/// larger magnitude of its golden minimum and maximum, as far as finite, which is the largest
/// finite magnitude of the golden column; 0 when the branch has no such column.
[[nodiscard]] double scaleOfTheFirstBranch(const json& document, std::string_view key)
{
    const json& branches = document.at("branches");
    if (branches.empty())
    {
        return 0;
    }
    for (const json& column : branches.at(0).at("columns"))
    {
        if (column.at("key").get<std::string>() == key)
        {
            return referenceOf(0, goldenValue(column.at("min")), goldenValue(column.at("max")));
        }
    }
    return 0;
}

/// Compares the summary values of @p data with the golden "summary" of @p document; @p first is
/// how the comparison of the first branch came out. When the run of that branch steps from the
/// apogee otherwise than the golden run (rule E), the two values that the end of its descent
/// decides are those of a run on the other time grid: the flight time is compared at
/// kOtherGridTimeAbsolute and the ground hit velocity at kOtherGridVelocityRelative of itself.
void compareDocumentSummary(Mismatches& m, DocumentComparison& d, const json& document,
                            const FlightData& data, const DocumentBranchOutcome& first)
{
    const json& expected = document.at("summary");
    for (const StableSummaryValue& value : kStableSummaryValues)
    {
        const double goldenNumber = goldenValue(expected.at(value.key));
        const double actual       = (data.*value.get)();
        const double unit = value.time ? 1.0
                                       : referenceOf(scaleOfTheFirstBranch(document, value.column),
                                                     goldenNumber, actual);
        const bool   otherGrid = first.otherGrid && value.extent != SummaryExtent::ANY;
        const double ofTheOtherGrid =
            value.time ? kOtherGridTimeAbsolute : kOtherGridVelocityRelative * unit;
        compareStableNumber(m, d.c,
                            {.field = std::string{value.key},
                             .what = std::format("summary:{}{}", value.key,
                                                 otherGrid ? " of a run that steps otherwise" : ""),
                             .expected  = goldenNumber,
                             .actual    = actual,
                             .tolerance = otherGrid ? ofTheOtherGrid : value.tolerance * unit,
                             .unit      = unit},
                            true);
        d.otherGrid += otherGrid ? 1 : 0;
    }
}

/// Compares the numbers of @p run with the document @p golden of its simulation in the
/// stable-step set: per branch the numbers of its header and the times of its events, then the
/// summary values, the number of jitter replacements and what the warnings print of their
/// parameters. (Everything that is not a number of the trajectory is compared with the
/// structure, compareSimulation().)
void compareDocumentNumbers(DocumentComparison& d, const GoldenFiles& golden,
                            const SimulationRun& run)
{
    const json& document = golden.document;
    if (run.data == nullptr)
    {
        return;  // the structure reports a run without flight data
    }
    const json&           branches = document.at("branches");
    bool                  tumbles  = false;
    DocumentBranchOutcome first;
    for (std::size_t i = 0; i < std::min(branches.size(), run.data->getBranchCount()); i++)
    {
        const DocumentBranchOutcome outcome = compareDocumentBranch(d, i, golden, run);
        tumbles                             = tumbles || outcome.tumbles;
        first                               = i == 0 ? outcome : first;
    }
    Mismatches summary(d.c.context + " summary");
    compareDocumentSummary(summary, d, document, *run.data, first);
    d.c.report += summary.report();

    // The number of jitter replacements is that of the force calculations of the Runge-Kutta
    // steppers: not reproducible when a dropped stage of the simulation flies on under them
    // without a recovery device (rule T).
    Mismatches         result(d.c.context + " result");
    const int          before = d.c.excluded.numbers;
    const std::int64_t jitter = document.at("result").at("jitterReplacements").get<std::int64_t>();
    compareStableCount(result, d.c, "jitterReplacements", jitter, run.jitterReplacements, !tumbles);
    attribute(d, before, d.excluded.tumbling);
    d.c.report += result.report();

    Mismatches warnings(d.c.context);
    compareStableWarnings(warnings, d.c, document.at("warnings"), *run.data);
    d.c.report += warnings.report();
}

/// What the comparison of a run with the document of its golden simulation compared and found.
struct DocumentResult
{
    SimulationCounts compared;
    SimulationCounts excluded;    ///< what the rules exclude
    Exclusions       exclusions;  ///< the numbers among it, by rule
    int              unlike{0};   ///< rule E: the branches whose run steps otherwise
    /// Rule E: the numbers compared as those of a run on the other time grid
    /// (DocumentComparison::otherGrid).
    int         otherGrid{0};
    int         steadyExtremes{0};  ///< the minima and maxima of columns that were compared
    std::string report;             ///< the mismatches, empty when everything matched
};

/// Compares @p run with the document @p golden of its simulation in the stable-step set: the
/// structure (everything that is not a number of the trajectory, as compareSimulation()
/// compares it, exactly), then the numbers. @p steps: DocumentComparison::steps.
[[nodiscard]] DocumentResult compareDocumentSimulation(
    const GoldenFiles& golden, const SimulationRun& run, const std::string& context,
    StableMeasurements*                     measurements = nullptr,
    const std::vector<std::optional<bool>>* steps        = nullptr)
{
    // The structure: compareSimulation() with nothing reproducible, so that it compares what
    // it compares in every simulation and no number of the trajectory.
    Sensitivity none;
    none.horizons.resize(run.data != nullptr ? run.data->getBranchCount() : 0);
    const SimulationComparison structure =
        compareSimulation(golden, run, none, context, kStructure);

    const DefaultUnitsGuard units;  // the texts of the warnings print units
    DocumentComparison      d;
    d.c.context      = context;
    d.c.measurements = measurements;
    d.steps          = steps;
    compareDocumentNumbers(d, golden, run);

    DocumentResult result{.compared       = d.c.compared,
                          .excluded       = d.c.excluded,
                          .exclusions     = d.excluded,
                          .unlike         = d.unlike,
                          .otherGrid      = d.otherGrid,
                          .steadyExtremes = d.steadyExtremes,
                          .report         = structure.report + d.c.report};
    result.compared.simulations = structure.compared.simulations;
    result.compared.branches    = structure.compared.branches;
    result.compared.events      = structure.compared.events;
    result.compared.columns     = structure.compared.columns;
    result.compared.warnings    = structure.compared.warnings;
    // The golden TUMBLE events of the dropped stages, which the structure leaves out.
    result.excluded.events = structure.sensitive.events;
    return result;
}

// ==================================================================================== the runs

/// A golden simulation of the stable-step set of an example compared with QtRocket's run of it.
struct DocumentRun
{
    std::string      problem;  ///< why there is no comparison, else ""
    std::string      context;  ///< "<input>/stable/sim_<NN>_<name>"
    GoldenFiles      files;
    SimulationRun    run;
    SimulationCounts golden;
    DocumentResult   comparison;
};

/// Runs simulation @p index of @p document with the stable time step, as the harness runs it,
/// and compares it with the document of simulation @p index of the stable-step set of the
/// golden input @p input.
[[nodiscard]] DocumentRun runStable(const std::shared_ptr<LoadedDocument>& document,
                                    const GoldenInput& input, std::size_t index)
{
    DocumentRun             result;
    const GoldenSimulation& simulation = input.stableSimulations.at(index);
    // The run first, whatever the files are: every simulation of a session is run, once.
    result.run = runDocumentSimulation(
        document, index, {.stableTimeStep = kStableTimeStep, .perturbation = nullptr});
    result.files = loadGoldenFiles(simulation);
    if (!result.files.problem.empty() || !result.run.problem.empty())
    {
        result.problem = result.files.problem + result.run.problem;
        return result;
    }
    result.golden  = goldenCounts(result.files);
    result.context = std::format("{}/stable/{}", input.name, baseName(result.run.planned));
    Mismatches m(result.context);
    m.text("name in the manifest", simulation.name, result.run.planned.name);
    m.text("file in the manifest", simulation.json, result.context + ".json");
    result.comparison        = compareDocumentSimulation(result.files, result.run, result.context);
    result.comparison.report = m.report() + result.comparison.report;
    return result;
}

/// The simulations of the stable-step set of one example: its document, loaded and settled as
/// the harness has it when it runs them (GoldenExamples.h), and the runs made so far, in the
/// order of the document.
struct ExampleSession
{
    std::string                     problems;  ///< a document that cannot be loaded
    const GoldenInput*              input{nullptr};
    std::shared_ptr<LoadedDocument> document;
    std::size_t                     simulations{0};  ///< those the document and the manifest have
    std::vector<DocumentRun>        runs;
};

/// Loads @p example for a session.
[[nodiscard]] ExampleSession openSession(const GoldenExample& example)
{
    ExampleSession session;
    session.input = manifest().has_value() ? manifest()->find(example.name) : nullptr;
    if (session.input == nullptr)
    {
        session.problems = std::format("no golden input named {}\n", example.name);
        return session;
    }
    Result<LoadedDocument> loaded = loadSettledGoldenExample(example);
    if (!loaded)
    {
        session.problems = std::format("{}: {}\n", example.file, loaded.error().message);
        return session;
    }
    session.document              = std::make_shared<LoadedDocument>(std::move(*loaded));
    const std::size_t simulations = session.document->document->getSimulationCount();
    if (simulations != session.input->stableSimulations.size())
    {
        session.problems += std::format(
            "{}: the document has {} simulations, the manifest lists {} in the "
            "stable-step set\n",
            example.file, simulations, session.input->stableSimulations.size());
    }
    // (Reserved, so that a run stays where it is while the later ones are made.)
    session.simulations = std::min(simulations, session.input->stableSimulations.size());
    session.runs.reserve(session.simulations);
    return session;
}

/// The session of @p example with at least its first @p simulations simulations run (as many
/// as it has, at most): made once per test process and kept, and a simulation is run when it
/// is first asked for, after those before it (a simulation of a document is run once: the run
/// changes its options in place, see runDocumentSimulation()).
[[nodiscard]] const ExampleSession& stableRuns(const GoldenExample& example,
                                               std::size_t          simulations)
{
    // The environment of the fixture first: the documents of the sessions point into it (their
    // simulations keep its preference store), and what is made first is destroyed last when the
    // process ends, so the sessions go before the environment they were loaded in.
    static_cast<void>(goldenExampleEnvironment());
    static std::mutex                                         s_mutex;
    static std::map<std::string, ExampleSession, std::less<>> s_sessions;
    const std::scoped_lock                                    lock{s_mutex};
    const DefaultUnitsGuard units;  // the names of the configurations and of an extension
    auto                    cached = s_sessions.find(example.name);
    if (cached == s_sessions.end())
    {
        cached = s_sessions.emplace(example.name, openSession(example)).first;
    }
    ExampleSession& session = cached->second;
    while (session.runs.size() < std::min(simulations, session.simulations))
    {
        session.runs.push_back(runStable(session.document, *session.input, session.runs.size()));
    }
    return session;
}

/// Every simulation of @p example, run and compared.
[[nodiscard]] const ExampleSession& stableRuns(const GoldenExample& example)
{
    return stableRuns(example, std::numeric_limits<std::size_t>::max());
}

/// The numbers of @p run that rule E has compared as those of a run on the other time grid,
/// counted from its document and from its rows without the comparison: of every branch of the
/// rule that steps from the apogee otherwise than its golden run, the number of rows and the
/// times of the events after the APOGEE, and of a first branch the flight time and the ground
/// hit velocity.
[[nodiscard]] int numbersOfTheOtherSteps(const DocumentRun& run)
{
    int         numbers  = 0;
    const json& branches = run.files.document.at("branches");
    for (std::size_t i = 0;
         run.run.data != nullptr && i < std::min(branches.size(), run.run.data->getBranchCount());
         i++)
    {
        const DocumentPlan plan = documentPlan(run.files.document, i);
        if (!apogeeOutcome(plan.shortApogeeStep, run.run.data->getBranch(i)).otherGrid())
        {
            continue;
        }
        const std::vector<const json*> events =
            orderedGoldenEvents(branches.at(i).at("events"), false);
        numbers += 1 + eventsAfter(events, placeOfTheApogee(events)) + (i == 0 ? 2 : 0);
    }
    return numbers;
}

/// Rule E: the steps that @p run takes from the apogees of its branches, in the form of
/// DocumentComparison::steps; nullopt for a branch that the rule does not list.
[[nodiscard]] std::vector<std::optional<bool>> apogeeStepsOf(const DocumentRun& run)
{
    std::vector<std::optional<bool>> steps;
    const std::size_t                branches =
        std::min(run.files.document.at("branches").size(), run.run.data->getBranchCount());
    steps.reserve(branches);
    for (std::size_t i = 0; i < branches; i++)
    {
        const DocumentPlan plan = documentPlan(run.files.document, i);
        steps.push_back(plan.shortApogeeStep.has_value()
                            ? std::optional<bool>{apogeeOutcome(plan.shortApogeeStep,
                                                                run.run.data->getBranch(i))
                                                      .step.isShort()}
                            : std::nullopt);
    }
    return steps;
}

/// What the stable simulations of one example hold, and what their comparison compared and
/// found.
struct ExampleResult
{
    std::string      problems;  ///< a document that cannot be loaded, a simulation without a run
    SimulationCounts golden;
    SimulationCounts compared;
    SimulationCounts excluded;
    Exclusions       exclusions;
    int              unlike{0};
    int              otherGrid{0};       ///< DocumentResult::otherGrid of the runs
    int              otherSteps{0};      ///< numbersOfTheOtherSteps() of the runs
    int              steadyExtremes{0};  ///< DocumentResult::steadyExtremes of the runs
    std::string      report;
};

/// Runs and compares every simulation of the stable-step set of @p example.
[[nodiscard]] ExampleResult compareExample(const GoldenExample& example)
{
    const ExampleSession& session = stableRuns(example);
    ExampleResult         result;
    result.problems = session.problems;
    for (std::size_t i = 0; i < session.runs.size(); i++)
    {
        const DocumentRun& run = session.runs[i];
        if (!run.problem.empty())
        {
            result.problems +=
                std::format("{}: {}\n", session.input->stableSimulations[i].json, run.problem);
            continue;
        }
        result.golden += run.golden;
        result.compared += run.comparison.compared;
        result.excluded += run.comparison.excluded;
        result.exclusions += run.comparison.exclusions;
        result.unlike += run.comparison.unlike;
        result.otherGrid += run.comparison.otherGrid;
        result.otherSteps += numbersOfTheOtherSteps(run);
        result.steadyExtremes += run.comparison.steadyExtremes;
        result.report += run.comparison.report;
    }
    return result;
}

/// What the stable-step documents of an example hold and what the rules decide of them, counted
/// from the files alone (tier9d-fix-simulation-goldens/scripts/coverage.py): the simulations,
/// their branches and events and the numbers outside the time series (goldenCounts()); of the
/// numbers the minima and maxima of the steady columns, which are compared, and those of the
/// other columns, which are not; those rule T excludes; the golden TUMBLE events that the
/// structure leaves out with it; and the most that rule E can compare as numbers of a run on the
/// other time grid, when every listed branch of the example steps otherwise than its golden run
/// (5 numbers of a first branch, 3 of a later one).
struct ExampleDocuments
{
    std::string_view input;
    int              simulations;
    int              branches;
    int              events;
    int              numbers;
    int              steady;
    int              extremes;
    int              tumbling;
    int              tumbles;
    int              apogeeAtMost;
};

// clang-format off
constexpr std::array<ExampleDocuments, 16> kExampleDocuments{{
    {.input = "example-3d-printable-nose-cone-and-fins", .simulations = 5, .branches = 5, .events = 51, .numbers = 833, .steady = 220, .extremes = 480, .tumbling = 0, .tumbles = 0, .apogeeAtMost = 15},
    {.input = "example-a-simple-model-rocket", .simulations = 5, .branches = 5, .events = 51, .numbers = 833, .steady = 220, .extremes = 480, .tumbling = 0, .tumbles = 0, .apogeeAtMost = 15},
    {.input = "example-airstart-timing", .simulations = 5, .branches = 5, .events = 65, .numbers = 855, .steady = 220, .extremes = 480, .tumbling = 0, .tumbles = 0, .apogeeAtMost = 0},
    {.input = "example-arc-payload-rocket", .simulations = 1, .branches = 2, .events = 19, .numbers = 320, .steady = 88, .extremes = 192, .tumbling = 0, .tumbles = 0, .apogeeAtMost = 0},
    {.input = "example-chute-release", .simulations = 2, .branches = 2, .events = 22, .numbers = 334, .steady = 88, .extremes = 192, .tumbling = 0, .tumbles = 0, .apogeeAtMost = 0},
    {.input = "example-clustered-motors", .simulations = 5, .branches = 5, .events = 51, .numbers = 833, .steady = 220, .extremes = 480, .tumbling = 0, .tumbles = 0, .apogeeAtMost = 15},
    {.input = "example-deployable-payload", .simulations = 5, .branches = 10, .events = 94, .numbers = 1603, .steady = 440, .extremes = 960, .tumbling = 0, .tumbles = 0, .apogeeAtMost = 16},
    {.input = "example-dual-parachute-deployment", .simulations = 6, .branches = 6, .events = 67, .numbers = 1015, .steady = 264, .extremes = 576, .tumbling = 0, .tumbles = 0, .apogeeAtMost = 0},
    {.input = "example-parallel-booster-staging", .simulations = 2, .branches = 3, .events = 32, .numbers = 489, .steady = 128, .extremes = 292, .tumbling = 8, .tumbles = 1, .apogeeAtMost = 0},
    {.input = "example-pods-airframes-and-winglets", .simulations = 5, .branches = 5, .events = 50, .numbers = 830, .steady = 220, .extremes = 480, .tumbling = 0, .tumbles = 0, .apogeeAtMost = 0},
    {.input = "example-pods-powered-with-recovery-deployment", .simulations = 1, .branches = 1, .events = 12, .numbers = 168, .steady = 44, .extremes = 96, .tumbling = 0, .tumbles = 0, .apogeeAtMost = 5},
    {.input = "example-simulation-extensions", .simulations = 3, .branches = 3, .events = 31, .numbers = 503, .steady = 136, .extremes = 288, .tumbling = 0, .tumbles = 0, .apogeeAtMost = 0},
    {.input = "example-simulation-scripting", .simulations = 3, .branches = 3, .events = 30, .numbers = 498, .steady = 132, .extremes = 288, .tumbling = 0, .tumbles = 0, .apogeeAtMost = 0},
    {.input = "example-three-stage-low-power-rocket", .simulations = 3, .branches = 9, .events = 102, .numbers = 1440, .steady = 372, .extremes = 888, .tumbling = 45, .tumbles = 6, .apogeeAtMost = 10},
    {.input = "example-tube-fin-rocket", .simulations = 1, .branches = 1, .events = 11, .numbers = 167, .steady = 44, .extremes = 96, .tumbling = 0, .tumbles = 0, .apogeeAtMost = 0},
    {.input = "example-two-stage-high-power-rocket", .simulations = 2, .branches = 4, .events = 46, .numbers = 648, .steady = 176, .extremes = 384, .tumbling = 0, .tumbles = 0, .apogeeAtMost = 0},
}};
// clang-format on

/// The pinned counts of the example whose golden input is @p input; those of an example
/// without simulations when the table has none (the test then fails on its counts).
[[nodiscard]] ExampleDocuments pinnedDocuments(std::string_view input)
{
    const std::span<const ExampleDocuments> all{kExampleDocuments};
    const auto found = std::ranges::find(all, input, &ExampleDocuments::input);
    return found == all.end() ? ExampleDocuments{.input        = input,
                                                 .simulations  = 0,
                                                 .branches     = 0,
                                                 .events       = 0,
                                                 .numbers      = 0,
                                                 .steady       = 0,
                                                 .extremes     = 0,
                                                 .tumbling     = 0,
                                                 .tumbles      = 0,
                                                 .apogeeAtMost = 0}
                              : *found;
}

// ====================================================================================== tests

/// One of OpenRocket's example designs: its golden simulations of the stable-step set.
class ExampleSimulationStableGolden : public ::testing::TestWithParam<GoldenExample>
{ };

TEST_P(ExampleSimulationStableGolden, EverySimulationOfTheExampleOverItsWholeFlight)
{
    const ExampleResult    result = compareExample(GetParam());
    const ExampleDocuments pinned = pinnedDocuments(GetParam().name);
    ASSERT_EQ(result.problems, "");
    EXPECT_EQ(result.report, "");
    // Everything the documents hold was compared or is excluded by a rule: nothing skipped.
    // And the documents hold what is pinned: no row and no value of a time series.
    EXPECT_EQ(result.compared + result.excluded, result.golden)
        << "what was compared or is excluded, and what the files hold";
    EXPECT_EQ(result.golden.simulations, pinned.simulations);
    EXPECT_EQ(result.golden.branches, pinned.branches);
    EXPECT_EQ(result.golden.events, pinned.events);
    EXPECT_EQ(result.golden.numbers, pinned.numbers);
    EXPECT_EQ(result.golden.rows + result.golden.values, 0);
    // The structure is compared in every simulation, but for the TUMBLE events of the dropped
    // stages that tumble or do not.
    EXPECT_EQ(result.excluded.events, pinned.tumbles);
    EXPECT_EQ(result.excluded.simulations + result.excluded.branches + result.excluded.columns +
                  result.excluded.warnings,
              0);
    // The numbers the rules exclude are those the files alone decide: the minima and maxima of
    // the columns that are not steady, and what rule T excludes. Every other number was
    // compared, on every platform, the minima and maxima of the steady columns among them.
    EXPECT_EQ(result.excluded.numbers, result.exclusions.total()) << result.exclusions;
    EXPECT_EQ(result.exclusions.extremes, pinned.extremes);
    EXPECT_EQ(result.exclusions.tumbling, pinned.tumbling);
    EXPECT_EQ(result.steadyExtremes, pinned.steady);
    EXPECT_EQ(result.compared.numbers, pinned.numbers - pinned.extremes - pinned.tumbling);
    // Rule E: what was compared as numbers of a run on the other time grid is what the rows of
    // the runs say may be, and at most what is pinned.
    EXPECT_LE(result.otherGrid, pinned.apogeeAtMost);
    EXPECT_EQ(result.otherGrid, result.otherSteps)
        << "what rule E compared on the other grid, and what the rows of the runs say it may: "
        << result.unlike << " branches step from the apogee otherwise than their golden runs";
}

INSTANTIATE_TEST_SUITE_P(Examples, ExampleSimulationStableGolden,
                         ::testing::ValuesIn(goldenExamples()), goldenExampleTestName);

// =============================================================================== measurement

/// "branch 0: the run takes the regular step from its apogee (its step to the apogee left 0
/// m/s), the golden run the short one; 745 and 746 rows;" for every branch of @p run that rule E
/// lists.
[[nodiscard]] std::string apogeeText(const DocumentRun& run)
{
    std::string text;
    const json& branches = run.files.document.at("branches");
    for (std::size_t i = 0; i < std::min(branches.size(), run.run.data->getBranchCount()); i++)
    {
        const DocumentPlan plan = documentPlan(run.files.document, i);
        if (!plan.shortApogeeStep.has_value())
        {
            continue;
        }
        const FlightDataBranch& branch  = run.run.data->getBranch(i);
        const ApogeeOutcome     outcome = apogeeOutcome(plan.shortApogeeStep, branch);
        text += std::format(
            " branch {}: the run takes the {} step from its apogee (its step to the apogee left {} "
            "m/s), the golden run the {} one; {} and {} rows;",
            i, outcome.step.isShort() ? "short" : "regular", outcome.step.left,
            plan.shortApogeeStep.value_or(false) ? "short" : "regular", branch.getLength(),
            branches.at(i).at("rows").get<std::int64_t>());
    }
    return text;
}

/// What the measurement prints of the examples.
struct MeasuredExamples
{
    std::string        problems;
    StableMeasurements measurements;
    std::string        apogees;  ///< apogeeText() of the runs with a branch of rule E
    SimulationCounts   compared;
    SimulationCounts   excluded;
    Exclusions         exclusions;
    int                unlike{0};
    int                otherGrid{0};  ///< DocumentResult::otherGrid of the runs
    std::string        report;
};

/// Adds the runs of @p example to @p measured.
void measure(MeasuredExamples& measured, const GoldenExample& example)
{
    const ExampleSession& session = stableRuns(example);
    measured.problems += session.problems;
    for (const DocumentRun& run : session.runs)
    {
        if (!run.problem.empty())
        {
            measured.problems += std::format("{}: {}\n", example.name, run.problem);
            continue;
        }
        const DocumentResult result =
            compareDocumentSimulation(run.files, run.run, run.context, &measured.measurements);
        measured.compared += result.compared;
        measured.excluded += result.excluded;
        measured.exclusions += result.exclusions;
        measured.unlike += result.unlike;
        measured.otherGrid += result.otherGrid;
        measured.report += result.report;
        const std::string apogees = run.run.data != nullptr ? apogeeText(run) : std::string{};
        measured.apogees +=
            apogees.empty() ? std::string{} : std::format("{}:{}\n", run.context, apogees);
    }
}

/// The runs of every example, measured.
[[nodiscard]] MeasuredExamples measuredExamples()
{
    MeasuredExamples measured;
    for (const GoldenExample& example : goldenExamples())
    {
        measure(measured, example);
    }
    return measured;
}

/// The measurement behind the tolerances: a table of the largest differences from the golden
/// documents by what was compared (a summary value, the times of the events of a type, ...)
/// over every simulation of every example, and, for information, what the rules read from the
/// rows of the runs (measureTheRows()): the number of values, the largest difference (of a
/// value: as a fraction of its scale; of a time: in s), that difference as a multiple of its
/// tolerance, and where it is; a line that starts with "excluded" is of numbers that are not
/// compared (the minimum and the maximum of a column that is not steady). Disabled; run it with
/// --gtest_also_run_disabled_tests (under a libm of another platform, or the one-ulp shim, to
/// see what the tolerances have to cover there: tier9d-stable-step/scripts/patterns.py).
TEST(ExampleSimulationStableGoldenMeasurement, DISABLED_PrintsTheDifferencesFromTheGoldenFiles)
{
    const MeasuredExamples measured = measuredExamples();
    std::cout << "compared: " << measured.compared << "\nexcluded: " << measured.excluded
              << "\nof them " << measured.exclusions << "; " << measured.unlike
              << " branches step from the apogee otherwise than the golden run, with "
              << measured.otherGrid << " numbers compared as those of the other time grid\n"
              << "The steps from the apogee of an Euler stepper (rule E):\n"
              << measured.apogees
              << "what\tvalues\tlargest difference\t... as a multiple of its tolerance\twhere\n"
              << measured.measurements.text()
              << std::format(
                     "The largest difference of a compared value is {:.3e} of its "
                     "tolerance.\n",
                     measured.measurements.largestOfTolerance())
              << "Mismatches:\n"
              << measured.report;
    EXPECT_EQ(measured.problems, "");
    EXPECT_LE(measured.measurements.largestOfTolerance(), 1.0);
}

// ================================================================================== coverage

/// What the rules decide of golden simulations, from their documents alone.
struct RuleCounts
{
    int tumblingStages{0};  ///< the branches of rule T
    int eulerApogees{0};    ///< the branches of rule E
    int shortSteps{0};      ///< of them: the golden run took the extra step of 1 ms
    int lateHandlings{0};   ///< the branches of rule L
    /// The times that are compared at kLateHandlingAbsolute: those of the events after the late
    /// handling of their branch (as far as rule T leaves them), and the separation times.
    int lateTimes{0};
    /// The branches whose document shows a late handling by itself: a recovery device deployed
    /// at the apogee (deploymentAtTheApogee()).
    int deploymentsAtTheApogee{0};
    /// Rule T: the numbers of the dropped stages without a recovery device that are compared at
    /// the tolerance of the rule: their optimum altitudes.
    int tumblingCompared{0};
    /// Rule E: the most numbers that can be compared as those of a run on the other time grid,
    /// when every listed branch steps otherwise than its golden run.
    int otherGridAtMost{0};
    /// The minima and maxima of the steady columns, which are compared.
    int steadyExtremes{0};

    [[nodiscard]] bool operator==(const RuleCounts&) const = default;

    RuleCounts& operator+=(const RuleCounts& other)
    {
        tumblingStages += other.tumblingStages;
        eulerApogees += other.eulerApogees;
        shortSteps += other.shortSteps;
        lateHandlings += other.lateHandlings;
        lateTimes += other.lateTimes;
        deploymentsAtTheApogee += other.deploymentsAtTheApogee;
        tumblingCompared += other.tumblingCompared;
        otherGridAtMost += other.otherGridAtMost;
        steadyExtremes += other.steadyExtremes;
        return *this;
    }
};

/// "7 branches of rule T, ...", for the messages of the tests.
std::ostream& operator<<(std::ostream& out, const RuleCounts& counts)
{
    return out << std::format(
               "{} branches of rule T with {} numbers at its tolerance; {} of rule E, {} with "
               "the short step in the golden run, with {} numbers on the other grid at most; "
               "{} of rule L with {} times at its tolerance; {} deployments at the apogee; "
               "{} extremes of steady columns",
               counts.tumblingStages, counts.tumblingCompared, counts.eulerApogees,
               counts.shortSteps, counts.otherGridAtMost, counts.lateHandlings, counts.lateTimes,
               counts.deploymentsAtTheApogee, counts.steadyExtremes);
}

/// The type of the golden event @p event.
[[nodiscard]] std::string typeOf(const json& event)
{
    return event.at("type").get<std::string>();
}

/// Whether the golden events @p events put a recovery device out before the APOGEE of their
/// branch: an Euler stepper then lands a step on the apogee (rule E).
[[nodiscard]] bool deviceIsOutBeforeTheApogee(const json& events)
{
    bool out = false;
    for (const json& event : events)
    {
        if (typeOf(event) == "APOGEE")
        {
            return out;
        }
        out = out || typeOf(event) == "RECOVERY_DEVICE_DEPLOYMENT";
    }
    return false;
}

/// The place among the golden events @p events of the first RECOVERY_DEVICE_DEPLOYMENT that
/// follows the APOGEE of its branch by 1 ms: a device that is deployed at the apogee, whose
/// deployment the engine queues 1 ms after the time of the APOGEE event
/// (BasicEventSimulationEngine.java:456). Under a Runge-Kutta stepper that time is the row
/// before the one that found the apogee, so the step that handles the deployment has ended
/// 9 ms after it: a late handling that the document shows by itself. Their number when there
/// is none.
[[nodiscard]] std::size_t deploymentAtTheApogee(const json& events)
{
    for (std::size_t i = 1; i < events.size(); i++)
    {
        const double after =
            goldenValue(events.at(i).at("time")) - goldenValue(events.at(i - 1).at("time"));
        if (typeOf(events.at(i)) == "RECOVERY_DEVICE_DEPLOYMENT" &&
            typeOf(events.at(i - 1)) == "APOGEE" && std::abs(after - 1e-3) <= kOnTime)
        {
            return i;
        }
    }
    return events.size();
}

/// A branch of a golden simulation, named for the problems of the lists.
struct BranchOfADocument
{
    std::string  input;
    std::int64_t simulation{0};
    std::size_t  index{0};
    const json*  branch{nullptr};
    const json*  summary{nullptr};

    [[nodiscard]] std::string name() const
    {
        return std::format("{} simulation {} branch {}", input, simulation, index);
    }
};

/// What is wrong with kTumblingStages for the branch @p branch: "" when it lists the branch
/// exactly when it is that of a dropped stage without a recovery device, and such a branch has
/// a TUMBLE among its golden events (the stable-step set records one for each of them).
[[nodiscard]] std::string tumblingListProblem(const BranchOfADocument& branch)
{
    const bool isListed =
        listed(kTumblingStages, branch.input, branch.simulation, branch.index) != nullptr;
    const json& events = branch.branch->at("events");
    if (isListed != isUnrecoveredDroppedStage(*branch.branch))
    {
        return std::format("{}: kTumblingStages and the events of the branch disagree\n",
                           branch.name());
    }
    return isListed && !eventTime(events, "TUMBLE").has_value()
               ? std::format("{}: no TUMBLE\n", branch.name())
               : std::string{};
}

/// What is wrong with kEulerApogees for the branch @p branch: "" when it lists the branch
/// exactly when a recovery device is out before its APOGEE and it is no dropped stage of rule
/// T, and, for a first branch, with the step that the document shows: the APOGEE event is 1 ms
/// after the time to apogee of the summary exactly when the golden run took the extra step.
[[nodiscard]] std::string apogeeListProblem(const BranchOfADocument& branch)
{
    const EulerApogee* entry = listed(kEulerApogees, branch.input, branch.simulation, branch.index);
    const json&        events = branch.branch->at("events");
    if ((entry != nullptr) !=
        (deviceIsOutBeforeTheApogee(events) && !isUnrecoveredDroppedStage(*branch.branch)))
    {
        return std::format("{}: kEulerApogees and the events of the branch disagree\n",
                           branch.name());
    }
    if (entry == nullptr || branch.index != 0)
    {
        return {};
    }
    const double afterTheSummit =
        eventTime(events, "APOGEE").value_or(0) - goldenValue(branch.summary->at("timeToApogee"));
    return (afterTheSummit > 0.5e-3) == entry->shortStep
               ? std::string{}
               : std::format("{}: the document shows the other step from the apogee\n",
                             branch.name());
}

/// What is wrong with kLateHandlings for the branch @p branch: "" when the event it names is of
/// the type it names, and when the list has every branch whose document shows a late handling
/// by itself (deploymentAtTheApogee()), with that event or an earlier one.
[[nodiscard]] std::string lateListProblem(const BranchOfADocument& branch)
{
    const LateHandling* entry =
        listed(kLateHandlings, branch.input, branch.simulation, branch.index);
    const json&       events = branch.branch->at("events");
    const std::size_t shown  = deploymentAtTheApogee(events);
    if (entry == nullptr)
    {
        return shown < events.size()
                   ? std::format("{}: a late handling that kLateHandlings lacks\n", branch.name())
                   : std::string{};
    }
    if (entry->event >= events.size() || typeOf(events.at(entry->event)) != entry->type)
    {
        return std::format("{}: event {} is no {}\n", branch.name(), entry->event, entry->type);
    }
    const bool deployment = entry->type == "RECOVERY_DEVICE_DEPLOYMENT";
    return entry->event > shown || (deployment && entry->event != shown)
               ? std::format("{}: the document shows the late handling of event {}\n",
                             branch.name(), shown)
               : std::string{};
}

/// What the documents of golden simulations hold and what the rules decide of them.
struct DocumentCounts
{
    SimulationCounts golden;      ///< goldenCounts()
    Exclusions       excluded;    ///< what the rules exclude
    int              tumbles{0};  ///< the golden TUMBLE events that the structure leaves out
    RuleCounts       rules;
    std::string      problems;  ///< what is wrong with the lists of the rules

    DocumentCounts& operator+=(const DocumentCounts& other)
    {
        golden += other.golden;
        excluded += other.excluded;
        tumbles += other.tumbles;
        rules += other.rules;
        problems += other.problems;
        return *this;
    }
};

/// Adds what rule T decides of the golden branch @p branch, whose plan is @p plan and whose
/// events in the order of the comparison are @p events, to @p counts.
void countTheTumbling(DocumentCounts& counts, const DocumentPlan& plan,
                      const std::vector<const json*>& events)
{
    if (!plan.tumbles)
    {
        return;
    }
    const std::size_t flown = eventsUpToTheLastSeparation(
        events, [](const json* event) { return isOfType(event, "STAGE_SEPARATION"); });
    counts.rules.tumblingStages++;
    // Not compared: the rows, the time to the optimum altitude and the optimum delay, and the
    // later events. Compared at the tolerance of the rule: the optimum altitude.
    const std::span<const json* const> after = std::span<const json* const>{events}.subspan(flown);
    counts.excluded.tumbling += 3 + numbersFrom(events, flown);
    counts.rules.tumblingCompared += 1;
    counts.tumbles += static_cast<int>(
        std::ranges::count_if(after, [](const json* event) { return isOfType(event, "TUMBLE"); }));
}

/// Adds what rules E and L decide of the golden branch @p branch (branch @p index of its
/// simulation), whose plan is @p plan and whose events in the order of the comparison are
/// @p events, to @p counts.
void countTheApogeeAndTheLateHandling(DocumentCounts& counts, const DocumentPlan& plan,
                                      const BranchOfADocument&        branch,
                                      const std::vector<const json*>& events)
{
    if (plan.shortApogeeStep.has_value())
    {
        counts.rules.eulerApogees++;
        counts.rules.shortSteps += plan.shortApogeeStep.value_or(false) ? 1 : 0;
        // The rows and the events after the APOGEE; of a first branch, the flight time and the
        // ground hit velocity.
        counts.rules.otherGridAtMost +=
            1 + eventsAfter(events, placeOfTheApogee(events)) + (branch.index == 0 ? 2 : 0);
    }
    if (!std::isfinite(plan.lateFrom))
    {
        return;
    }
    const std::size_t flown =
        plan.tumbles
            ? eventsUpToTheLastSeparation(
                  events, [](const json* event) { return isOfType(event, "STAGE_SEPARATION"); })
            : events.size();
    counts.rules.lateHandlings++;
    counts.rules.lateTimes += static_cast<int>(std::ranges::count_if(
        std::span{events}.first(flown),
        [&plan](const json* event) { return goldenValue(event->at("time")) > plan.lateFrom; }));
    counts.rules.lateTimes +=
        goldenValue(branch.branch->at("separationTime")) > plan.lateFrom ? 1 : 0;
}

/// The number of the minima and maxima of the golden branch @p branch that are compared: two
/// per steady column (hasSteadyExtremes()); @p tumbles: whether rule T applies to the branch.
[[nodiscard]] int steadyExtremesOf(const json& branch, bool tumbles)
{
    int extremes = 0;
    for (const json& column : branch.at("columns"))
    {
        extremes += hasSteadyExtremes(column.at("key").get<std::string>(), tumbles) ? 2 : 0;
    }
    return extremes;
}

/// What the document of the golden simulation @p files holds and what the rules decide of it.
[[nodiscard]] DocumentCounts countsOf(const GoldenFiles& files)
{
    DocumentCounts counts;
    counts.golden        = goldenCounts(files);
    const json& document = files.document;
    bool        tumbles  = false;
    for (std::size_t i = 0; i < document.at("branches").size(); i++)
    {
        const BranchOfADocument branch{.input      = document.at("input").get<std::string>(),
                                       .simulation = document.at("index").get<std::int64_t>(),
                                       .index      = i,
                                       .branch     = &document.at("branches").at(i),
                                       .summary    = &document.at("summary")};
        const DocumentPlan      plan = documentPlan(document, i);
        const std::vector<const json*> events =
            orderedGoldenEvents(branch.branch->at("events"), false);
        const int steady = steadyExtremesOf(*branch.branch, plan.tumbles);
        counts.rules.steadyExtremes += steady;
        counts.excluded.extremes +=
            (2 * static_cast<int>(branch.branch->at("columns").size())) - steady;
        countTheTumbling(counts, plan, events);
        countTheApogeeAndTheLateHandling(counts, plan, branch, events);
        counts.rules.deploymentsAtTheApogee +=
            deploymentAtTheApogee(branch.branch->at("events")) < events.size() ? 1 : 0;
        counts.problems +=
            tumblingListProblem(branch) + apogeeListProblem(branch) + lateListProblem(branch);
        tumbles = tumbles || plan.tumbles;
    }
    counts.excluded.tumbling += tumbles ? 1 : 0;  // the jitter replacements of the simulation
    return counts;
}

/// What is wrong with the pinned counts of the golden input @p input, whose documents hold
/// @p counts: "" when kExampleDocuments has them.
[[nodiscard]] std::string pinProblem(const GoldenInput& input, const DocumentCounts& counts)
{
    const ExampleDocuments pinned = pinnedDocuments(input.name);
    const bool             same =
        pinned.simulations == counts.golden.simulations &&
        pinned.branches == counts.golden.branches && pinned.events == counts.golden.events &&
        pinned.numbers == counts.golden.numbers && pinned.steady == counts.rules.steadyExtremes &&
        pinned.extremes == counts.excluded.extremes &&
        pinned.tumbling == counts.excluded.tumbling && pinned.tumbles == counts.tumbles &&
        pinned.apogeeAtMost == counts.rules.otherGridAtMost;
    return same ? std::string{}
                : std::format(
                      "{}: the documents hold {} simulations, {} branches, {} events and "
                      "{} numbers, of which {} are extremes of steady columns and the rules "
                      "exclude {}, with {} TUMBLE events left out; rule E has {} numbers at most\n",
                      input.name, counts.golden.simulations, counts.golden.branches,
                      counts.golden.events, counts.golden.numbers, counts.rules.steadyExtremes,
                      toText(counts.excluded), counts.tumbles, counts.rules.otherGridAtMost);
}

/// What the stable-step documents of the examples hold and what the rules decide of them, read
/// from the files alone (no simulation is run).
struct DocumentCoverage
{
    int            inputs{0};  ///< the "example" inputs of the manifest
    DocumentCounts counts;
    std::string    problems;  ///< a file that cannot be read, counts that are not the pinned ones
};

/// Adds the stable-step documents of the golden input @p input to @p coverage.
void cover(DocumentCoverage& coverage, const GoldenInput& input)
{
    DocumentCounts counts;
    for (const GoldenSimulation& simulation : input.stableSimulations)
    {
        const GoldenFiles files = loadGoldenFiles(simulation);
        if (!files.problem.empty())
        {
            coverage.problems += std::format("{}: {}\n", simulation.json, files.problem);
            continue;
        }
        if (!files.tables.empty())
        {
            coverage.problems += std::format("{}: has time series\n", simulation.json);
        }
        counts += countsOf(files);
    }
    coverage.problems += pinProblem(input, counts);
    coverage.counts += counts;
}

/// Reads every stable-step document of every "example" input of the manifest.
[[nodiscard]] DocumentCoverage documentCoverage()
{
    DocumentCoverage coverage;
    if (!manifest())
    {
        coverage.problems = manifest().error().message;
        return coverage;
    }
    for (const GoldenInput& input : manifest()->inputs)
    {
        if (input.kind != "example")
        {
            continue;
        }
        coverage.inputs++;
        if (findGoldenExample(input.name) == nullptr)
        {
            coverage.problems += std::format("{}: not an example of the fixture\n", input.name);
        }
        cover(coverage, input);
    }
    return coverage;
}

/// Every golden simulation of the stable-step set of the examples is compared: each of the
/// sixteen inputs is an example of the fixture (goldenExamples()), and the test of each example
/// (ExampleSimulationStableGolden) checks that what it compared and what the rules exclude add
/// up to what the documents of its input hold, and that the floor holds. This test pins what
/// the documents hold, per example (kExampleDocuments) and in all, and what the rules decide
/// from the documents alone, and holds the three lists of the rules against the documents; it
/// runs no simulation.
TEST(ExampleSimulationStableGoldenCoverage, EveryDocumentOfTheExamplesIsOfAnExampleOfTheFixture)
{
    const DocumentCoverage coverage = documentCoverage();
    const DocumentCounts&  counts   = coverage.counts;
    EXPECT_EQ(coverage.problems, "");
    EXPECT_EQ(counts.problems, "") << "the lists of the rules against the documents";
    EXPECT_EQ(coverage.inputs, 16);
    EXPECT_EQ(goldenExamples().size(), 16U);
    EXPECT_EQ(kExampleDocuments.size(), 16U);
    ASSERT_TRUE(manifest().has_value());
    EXPECT_EQ(manifest()->stableTimeStep, kStableTimeStep);
    // The simulations, branches, columns and warnings of the default-step set
    // (ExampleSimulationGoldenCoverage), with one event more (the booster set of the parallel
    // staging records a TUMBLE at this step), and no row: the documents alone.
    EXPECT_EQ(counts.golden, (SimulationCounts{.simulations = 54,
                                               .branches    = 69,
                                               .events      = 734,
                                               .columns     = 4832,
                                               .warnings    = 20,
                                               .numbers     = 11369,
                                               .rows        = 0,
                                               .values      = 0}));
    // Of the 11369 numbers, 9664 are the minimum and the maximum of a column: 3012 of them are
    // of a steady column and are compared (22 columns in each of the 62 branches under no rule
    // T, the fin cant of the roll control in two of them, and 20 columns in each of the 7
    // branches of the rule), and 6652 are not. Rule T excludes 53 more (of each of the 7
    // dropped stages without a recovery device its rows, the time to its optimum altitude, its
    // optimum delay and its four events after its separation, and the jitter replacements of
    // their 4 simulations), with whose 7 TUMBLE events the structure deals. So 4664 numbers are
    // compared, on every platform.
    EXPECT_EQ(counts.excluded, (Exclusions{.extremes = 6652, .tumbling = 53})) << counts.excluded;
    EXPECT_EQ(counts.golden.numbers - counts.excluded.total(), 4664);
    EXPECT_EQ(counts.tumbles, 7);
    // The lists of the rules, which the documents bear out as far as they can (the problems
    // above): 7 branches of rule T, whose 7 optimum altitudes are compared at the tolerance of
    // the rule; 16 of rule E, in 2 of which the
    // golden run took the extra step, and of which 76 numbers at most are compared as those of
    // a run on the other time grid (of each the rows, the GROUND_HIT and the SIMULATION_END,
    // and of the 14 first ones the flight time and the ground hit velocity); 26 of rule L,
    // after whose late handlings 110 times are compared at 1 ms (106 of events, 4 of
    // separations); 21 branches show a late handling by themselves, a deployment 1 ms after
    // the apogee, which is the first late handling of 19.
    EXPECT_EQ(counts.rules, (RuleCounts{.tumblingStages         = 7,
                                        .eulerApogees           = 16,
                                        .shortSteps             = 2,
                                        .lateHandlings          = 26,
                                        .lateTimes              = 110,
                                        .deploymentsAtTheApogee = 21,
                                        .tumblingCompared       = 7,
                                        .otherGridAtMost        = 76,
                                        .steadyExtremes         = 3012}))
        << counts.rules;
    EXPECT_EQ(kSteadyColumns.size(), 23U);
    EXPECT_EQ(kSteadyColumnsOfATumblingStage.size(), 20U);
    EXPECT_EQ(kTumblingStages.size(), 7U);
    EXPECT_EQ(kEulerApogees.size(), 16U);
    EXPECT_EQ(kLateHandlings.size(), 26U);
}

// ================================================================================= mutations

// The comparison with the documents is not vacuous: a golden value changed in a copy of the
// document of a simulation is reported, in one line that names it. The simulations:
// "Simulation 1" of "A simple model rocket", a flight under no rule (its apogee under a
// Runge-Kutta stepper, 575 rows); "Simulation 1" of "Dual parachute deployment", whose warning
// prints a speed and whose drogue is deployed at the apogee, a late handling (rule L); and
// "Simulation 1" of the three-stage rocket, whose two boosters fly on without a recovery
// device (rule T) and whose sustainer handles a BURNOUT late.

/// Four times a tolerance, and a quarter of it.
constexpr double kBeyond = 4.0;
constexpr double kWithin = 0.25;

constexpr Subject kPlainFlight{.input = "example-a-simple-model-rocket", .index = 0};
constexpr Subject kDualDeployment{.input = "example-dual-parachute-deployment", .index = 0};
constexpr Subject kThreeStages{.input = "example-three-stage-low-power-rocket", .index = 0};

/// Adds @p relative times the scale of the column @p column of the first branch to the number
/// at @p pointer: a change by that many of the units that a summary value is compared in.
void shiftByTheScale(GoldenFiles& files, std::string_view pointer, double relative,
                     std::string_view column)
{
    shift(files, pointer, relative * scaleOfTheFirstBranch(files.document, column));
}

/// Adds @p relative times the scale of column @p column of branch @p branch (the larger
/// magnitude of its golden minimum and maximum) to its minimum or maximum (@p extreme: "min" or
/// "max"): a change by that many of the units that an extreme is compared in.
void shiftAnExtreme(GoldenFiles& files, std::size_t branch, std::size_t column,
                    std::string_view extreme, double relative)
{
    const json&  golden = files.document.at("branches").at(branch).at("columns").at(column);
    const double scale =
        referenceOf(0, goldenValue(golden.at("min")), goldenValue(golden.at("max")));
    shift(files, std::format("/branches/{}/columns/{}/{}", branch, column, extreme),
          relative * scale);
}

/// The places of columns among the 70 of a branch of the examples: the mass, the centre of
/// gravity and the thrust, which are steady, and the time and the vertical velocity, which are
/// not (the mutations name each by its key in the line they expect).
constexpr std::size_t kTimeColumn     = 0;
constexpr std::size_t kVelocityColumn = 3;
constexpr std::size_t kMassColumn     = 26;
constexpr std::size_t kCgColumn       = 32;
constexpr std::size_t kThrustColumn   = 36;

/// The changes to the settings and to the structure of the flight under no rule.
[[nodiscard]] std::vector<Mutation> structureMutations()
{
    return {
        {.name    = "TimeStep",
         .subject = kPlainFlight,
         .apply   = [](GoldenFiles& files) { files.document["options"]["timeStep"] = 0.05; },
         .heading = " options",
         .line    = "  timeStep: expected 0.05"},
        {.name    = "HarnessDocumentTimeStep",
         .subject = kPlainFlight,
         .apply = [](GoldenFiles& files) { files.document["harness"]["documentTimeStep"] = 0.01; },
         .heading = " harness",
         .line    = R"(  documentTimeStep: expected "0.01", got "0.05")"},
        {.name    = "OptionsSource",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) { files.document["optionsSource"] = "applicationDefaults"; },
         .heading = "",
         .line    = R"(  optionsSource: expected "applicationDefaults", got "document")"},
        {.name    = "Status",
         .subject = kPlainFlight,
         .apply   = [](GoldenFiles& files) { files.document["result"]["status"] = "exception"; },
         .heading = " result",
         .line    = R"(  status: expected "exception", got "completed")"},
        {.name    = "JitterReplacements",
         .subject = kPlainFlight,
         .apply = [](GoldenFiles& files) { files.document["result"]["jitterReplacements"] = 2069; },
         .heading = " result",
         .line    = "  jitterReplacements: expected 2069, got 2068"},
        {.name    = "BranchCount",
         .subject = kPlainFlight,
         .apply   = [](GoldenFiles& files) { files.document["summary"]["branchCount"] = 2; },
         .heading = " summary",
         .line    = "  branchCount: expected 2, got 1"},
        {.name    = "BranchName",
         .subject = kPlainFlight,
         .apply   = [](GoldenFiles& files) { files.document["branches"][0]["name"] = "Booster"; },
         .heading = " branch 0",
         .line    = R"(  name: expected "Booster", got "Sustainer")"},
        {.name    = "ColumnSymbol",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["columns"][1]["symbol"] = "H";
             },
         .heading = " branch 0",
         .line    = R"(  columns[1].symbol: expected "H", got "h")"},
        {.name    = "ATimeSeriesIsNamed",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["csv"] = "sim_00_simulation-1_branch0.csv.gz";
             },
         .heading = " branch 0",
         .line    = R"(  csv: expected "sim_00_simulation-1_branch0.csv.gz", got "null")"},
        {.name    = "RemovedEvent",
         .subject = kPlainFlight,
         .apply   = [](GoldenFiles& files) { files.document["branches"][0]["events"].erase(5); },
         .heading = " branch 0",
         .line    = R"(  events: expected "LAUNCH(/) IGNITION(/0/1/7) LIFTOFF(null) )"
                    "LAUNCHROD(null) BURNOUT(/0/1/7) EJECTION_CHARGE(/0) "},
        {.name    = "SwappedEvents",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 json& events = files.document["branches"][0]["events"];
                 std::swap(events[5], events[6]);
             },
         .heading = " branch 0",
         .line    = R"(  events: expected "LAUNCH(/) IGNITION(/0/1/7) LIFTOFF(null) )"
                    "LAUNCHROD(null) BURNOUT(/0/1/7) EJECTION_CHARGE(/0) APOGEE(/) "},
        {.name    = "MotorOfAnEvent",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["events"][4]["data"]["designation"] = "B4";
             },
         .heading = " branch 0",
         .line    = R"(  events[4].data.designation: expected "B4", got "A8")"},
    };
}

/// The changes to the summary and to the header of the branch of the flight under no rule.
[[nodiscard]] std::vector<Mutation> numberMutations()
{
    return {
        {.name    = "MaximumAltitude",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/summary/maxAltitude", kBeyond * kStableSummaryRelative);
             },
         .heading = " summary",
         .line    = "  maxAltitude: expected "},
        {.name    = "MaximumVelocity",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/summary/maxVelocity", kBeyond * kStableSummaryRelative);
             },
         .heading = " summary",
         .line    = "  maxVelocity: expected "},
        {.name    = "MaximumAcceleration",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/summary/maxAcceleration", kBeyond * kStableSummaryRelative);
             },
         .heading = " summary",
         .line    = "  maxAcceleration: expected "},
        {.name    = "MaximumMachNumber",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/summary/maxMachNumber", kBeyond * kStableSummaryRelative);
             },
         .heading = " summary",
         .line    = "  maxMachNumber: expected "},
        {.name    = "TimeToApogee",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/summary/timeToApogee", kBeyond * kStableTimeAbsolute);
             },
         .heading = " summary",
         .line    = "  timeToApogee: expected "},
        {.name    = "FlightTime",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/summary/flightTime", kBeyond * kStableTimeAbsolute);
             },
         .heading = " summary",
         .line    = "  flightTime: expected "},
        {.name    = "GroundHitVelocity",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/summary/groundHitVelocity", kBeyond * kStableSummaryRelative);
             },
         .heading = " summary",
         .line    = "  groundHitVelocity: expected "},
        {.name    = "LaunchRodVelocity",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 shiftByTheScale(files, "/summary/launchRodVelocity", kBeyond * kValueRelative,
                                 "velocity_total");
             },
         .heading = " summary",
         .line    = "  launchRodVelocity: expected "},
        {.name    = "DeploymentVelocity",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 shiftByTheScale(files, "/summary/deploymentVelocity",
                                 kBeyond * kDeploymentVelocityRelative, "velocity_total");
             },
         .heading = " summary",
         .line    = "  deploymentVelocity: expected "},
        {.name    = "OptimumDelayOfTheSummary",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/summary/optimumDelay", kBeyond * kStableTimeAbsolute);
             },
         .heading = " summary",
         .line    = "  optimumDelay: expected "},
        {.name    = "RowCount",
         .subject = kPlainFlight,
         .apply   = [](GoldenFiles& files) { files.document["branches"][0]["rows"] = 576; },
         .heading = " branch 0",
         .line    = "  rows: expected 576, got 575"},
        {.name    = "OptimumAltitude",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/branches/0/optimumAltitude", kBeyond * kStableSummaryRelative);
             },
         .heading = " branch 0",
         .line    = "  optimumAltitude: expected "},
        {.name    = "TimeToTheOptimumAltitude",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/timeToOptimumAltitude", kBeyond * kStableTimeAbsolute);
             },
         .heading = " branch 0",
         .line    = "  timeToOptimumAltitude: expected "},
        {.name    = "OptimumDelay",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/optimumDelay", kBeyond * kStableTimeAbsolute);
             },
         .heading = " branch 0",
         .line    = "  optimumDelay: expected "},
        {.name    = "MinimumOfTheMass",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 shiftAnExtreme(files, 0, kMassColumn, "min", kBeyond * kStableSummaryRelative);
             },
         .heading = " branch 0",
         .line    = "  columns[26].min (mass): expected "},
        {.name    = "MaximumOfTheCentreOfGravity",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 shiftAnExtreme(files, 0, kCgColumn, "max", -kBeyond * kStableSummaryRelative);
             },
         .heading = " branch 0",
         .line    = "  columns[32].max (cg_location): expected "},
        {.name    = "MaximumOfTheThrust",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 shiftAnExtreme(files, 0, kThrustColumn, "max", kBeyond * kStableSummaryRelative);
             },
         .heading = " branch 0",
         .line    = "  columns[36].max (thrust_force): expected "},
    };
}

/// The changes to the times of events, to the warnings, and to what the rules L and T leave
/// to compare.
[[nodiscard]] std::vector<Mutation> eventMutations()
{
    return {
        {.name    = "TimeOfTheLiftoff",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/events/2/time", kBeyond * kStableTimeAbsolute);
             },
         .heading = " branch 0",
         .line    = "  events[2].time: expected "},
        {.name    = "TimeOfTheApogee",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/events/5/time", kBeyond * kStableTimeAbsolute);
             },
         .heading = " branch 0",
         .line    = "  events[5].time: expected "},
        {.name    = "TimeOfTheGroundHit",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/events/8/time", -kBeyond * kStableTimeAbsolute);
             },
         .heading = " branch 0",
         .line    = "  events[8].time: expected "},
        {.name    = "TimeOfTheEnd",
         .subject = kPlainFlight,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/events/9/time", kBeyond * kStableTimeAbsolute);
             },
         .heading = " branch 0",
         .line    = "  events[9].time: expected "},
        {.name    = "SpeedOfAWarning",
         .subject = kDualDeployment,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/warnings/0/parameter", kBeyond * kStableSummaryRelative);
             },
         .heading = "",
         .line    = "  warnings[0].parameter: expected "},
        {.name    = "TextOfAWarningWithASpeed",
         .subject = kDualDeployment,
         .apply =
             [](GoldenFiles& files) {
                 files.document["warnings"][0]["text"] = "Recovery device deployment at 23.6 m/s";
             },
         .heading = "",
         .line    = R"(  warnings[0].text: expected "Recovery device deployment at 23.6 m/s")"},
        {.name    = "SpeedOfTheWarningOfAnEvent",
         .subject = kDualDeployment,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/branches/0/events/7/data/parameter",
                       kBeyond * kStableSummaryRelative);
             },
         .heading = " branch 0",
         .line    = "  events[7].data.parameter: expected "},
        {.name    = "TimeOfTheApogeeBeforeTheLateHandling",
         .subject = kDualDeployment,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/events/5/time", kBeyond * kStableTimeAbsolute);
             },
         .heading = " branch 0",
         .line    = "  events[5].time: expected "},
        {.name    = "TimeOfTheGroundHitAfterTheLateHandling",
         .subject = kDualDeployment,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/events/9/time", kBeyond * kLateHandlingAbsolute);
             },
         .heading = " branch 0",
         .line    = "  events[9].time: expected "},
        {.name    = "MaximumAltitudeOfTheStagedFlight",
         .subject = kThreeStages,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/summary/maxAltitude", kBeyond * kStableSummaryRelative);
             },
         .heading = " summary",
         .line    = "  maxAltitude: expected "},
        {.name    = "TimeOfTheBurnoutOfADroppedStage",
         .subject = kThreeStages,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/2/events/1/time", kBeyond * kStableTimeAbsolute);
             },
         .heading = " branch 2",
         .line    = "  events[1].time: expected "},
        {.name    = "SeparationTimeOfADroppedStage",
         .subject = kThreeStages,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/2/separationTime", kBeyond * kStableTimeAbsolute);
             },
         .heading = " branch 2",
         .line    = "  separationTime: expected "},
        {.name    = "TimeOfAnEjectionChargeAfterTheLateHandling",
         .subject = kThreeStages,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/events/9/time", kBeyond * kLateHandlingAbsolute);
             },
         .heading = " branch 0",
         .line    = "  events[9].time: expected "},
        // Rule T: what is compared of a dropped stage after its separation.
        {.name    = "OptimumAltitudeOfADroppedStage",
         .subject = kThreeStages,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/branches/2/optimumAltitude", kBeyond * kTumblingAltitudeRelative);
             },
         .heading = " branch 2",
         .line    = "  optimumAltitude: expected "},
        {.name    = "MaximumOfTheMassOfADroppedStage",
         .subject = kThreeStages,
         .apply =
             [](GoldenFiles& files) {
                 shiftAnExtreme(files, 2, kMassColumn, "max", kBeyond * kStableSummaryRelative);
             },
         .heading = " branch 2",
         .line    = "  columns[26].max (mass): expected "},
    };
}

/// Every change.
[[nodiscard]] std::vector<Mutation> mutations()
{
    std::vector<Mutation> all = structureMutations();
    for (const Mutation& mutation : numberMutations())
    {
        all.push_back(mutation);
    }
    for (const Mutation& mutation : eventMutations())
    {
        all.push_back(mutation);
    }
    return all;
}

/// The mutations of the simulation @p subject.
[[nodiscard]] std::vector<Mutation> mutationsOf(const Subject& subject)
{
    std::vector<Mutation> own;
    for (const Mutation& mutation : mutations())
    {
        if (mutation.subject.input == subject.input && mutation.subject.index == subject.index)
        {
            own.push_back(mutation);
        }
    }
    return own;
}

/// The run of the simulation @p subject of an example, or null (with a test failure) when
/// there is none. The simulations of the example before it are run as well, and no later one.
[[nodiscard]] const DocumentRun* subjectRun(const Subject& subject)
{
    const GoldenExample* example = findGoldenExample(subject.input);
    if (example == nullptr)
    {
        ADD_FAILURE() << "no example named " << subject.input;
        return nullptr;
    }
    const ExampleSession& session = stableRuns(*example, subject.index + 1);
    if (!session.problems.empty() || subject.index >= session.runs.size() ||
        !session.runs[subject.index].problem.empty())
    {
        ADD_FAILURE() << "no run of simulation " << subject.index << " of " << subject.input << ": "
                      << session.problems;
        return nullptr;
    }
    return &session.runs[subject.index];
}

/// What the comparison of @p run with a copy of its document finds after @p change changed the
/// copy (null: nothing changed).
[[nodiscard]] DocumentResult comparisonAfter(const DocumentRun& run,
                                             void (*change)(GoldenFiles& files))
{
    GoldenFiles files = run.files;
    if (change != nullptr)
    {
        change(files);
    }
    return compareDocumentSimulation(files, run.run, run.context);
}

/// What is wrong with the report @p report of the mutation @p mutation of the simulation whose
/// context is @p context: "" when it is the heading and the one line that name the change.
[[nodiscard]] std::string oneLineProblem(const Mutation& mutation, const std::string& context,
                                         const std::string& report)
{
    const std::string start = std::format("{}{}:\n{}", context, mutation.heading, mutation.line);
    if (!report.starts_with(start))
    {
        return std::format("{}: the report\n{}\ndoes not start with\n{}\n", mutation.name, report,
                           start);
    }
    return std::ranges::count(report, '\n') == 2
               ? std::string{}
               : std::format("{}: the report is not one line:\n{}\n", mutation.name, report);
}

/// What is wrong with the reports of the mutations of the simulation @p subject, each named;
/// "" when every one is reported in the heading and the one line that name the change.
[[nodiscard]] std::string mutationProblems(const Subject& subject)
{
    const DocumentRun* run = subjectRun(subject);
    if (run == nullptr)
    {
        return "no run";
    }
    std::string problems;
    for (const Mutation& mutation : mutationsOf(subject))
    {
        problems +=
            oneLineProblem(mutation, run->context, comparisonAfter(*run, mutation.apply).report);
    }
    return problems;
}

/// Changes within the tolerances to the flight under no rule: summary values, the numbers of
/// the header of its branch, the times of events.
void changeThePlainFlightWithinTheTolerances(GoldenFiles& files)
{
    scale(files, "/summary/maxAltitude", kWithin * kStableSummaryRelative);
    scale(files, "/summary/maxVelocity", -kWithin * kStableSummaryRelative);
    shift(files, "/summary/timeToApogee", kWithin * kStableTimeAbsolute);
    shift(files, "/summary/flightTime", -kWithin * kStableTimeAbsolute);
    scale(files, "/summary/groundHitVelocity", kWithin * kStableSummaryRelative);
    shiftByTheScale(files, "/summary/launchRodVelocity", kWithin * kValueRelative,
                    "velocity_total");
    shiftByTheScale(files, "/summary/deploymentVelocity", kWithin * kDeploymentVelocityRelative,
                    "velocity_total");
    scale(files, "/branches/0/optimumAltitude", kWithin * kStableSummaryRelative);
    shift(files, "/branches/0/optimumDelay", kWithin * kStableTimeAbsolute);
    shift(files, "/branches/0/events/5/time", kWithin * kStableTimeAbsolute);
    shift(files, "/branches/0/events/8/time", -kWithin * kStableTimeAbsolute);
    shiftAnExtreme(files, 0, kMassColumn, "min", kWithin * kStableSummaryRelative);
    shiftAnExtreme(files, 0, kCgColumn, "max", -kWithin * kStableSummaryRelative);
    shiftAnExtreme(files, 0, kThrustColumn, "max", kWithin * kStableSummaryRelative);
}

/// Changes within the tolerances to the flight with the two deployments: the speed of its
/// warning, and, after the late handling of its drogue, the time of an event by four times the
/// tolerance of an event before it, which is less than half of the tolerance there (rule L).
void changeTheDualDeploymentWithinTheTolerances(GoldenFiles& files)
{
    scale(files, "/warnings/0/parameter", kWithin * kStableSummaryRelative);
    scale(files, "/branches/0/events/7/data/parameter", -kWithin * kStableSummaryRelative);
    shift(files, "/branches/0/events/9/time", kBeyond * kStableTimeAbsolute);
    shift(files, "/branches/0/events/8/time", -kBeyond * kStableTimeAbsolute);
}

/// Changes within the tolerances to the three-stage rocket: of its first booster, a dropped
/// stage without a recovery device (rule T), the optimum altitude by a quarter of the tolerance
/// of the rule, which is five thousand times the tolerance of an optimum altitude under no
/// rule; and an extreme of a steady column of it.
void changeTheThreeStagesWithinTheTolerances(GoldenFiles& files)
{
    scale(files, "/branches/2/optimumAltitude", kWithin * kTumblingAltitudeRelative);
    shiftAnExtreme(files, 2, kMassColumn, "max", kWithin * kStableSummaryRelative);
}

/// A simulation that the mutations change, with the changes within the tolerances that go with
/// it (null: none).
constexpr std::array<SubjectCase, 3> kSubjectCases{{
    {.name                = "TheFlightUnderNoRule",
     .subject             = kPlainFlight,
     .withinTheTolerances = changeThePlainFlightWithinTheTolerances},
    {.name                = "TheFlightWithTwoDeployments",
     .subject             = kDualDeployment,
     .withinTheTolerances = changeTheDualDeploymentWithinTheTolerances},
    {.name                = "TheThreeStages",
     .subject             = kThreeStages,
     .withinTheTolerances = changeTheThreeStagesWithinTheTolerances},
}};

/// The golden values of one simulation of the stable-step set of an example, changed one at a
/// time in a copy of its document.
class ExampleSimulationStableGoldenMutation : public ::testing::TestWithParam<SubjectCase>
{ };

/// What the comparison finds of the document as it is, and after the changes within the
/// tolerances of @p subjectCase: "" when nothing.
[[nodiscard]] std::string reportWithinTheTolerances(const SubjectCase& subjectCase)
{
    const DocumentRun* run = subjectRun(subjectCase.subject);
    if (run == nullptr)
    {
        return "no run";
    }
    std::string report = comparisonAfter(*run, nullptr).report;
    if (subjectCase.withinTheTolerances != nullptr)
    {
        report += comparisonAfter(*run, subjectCase.withinTheTolerances).report;
    }
    return report;
}

TEST_P(ExampleSimulationStableGoldenMutation, EveryChangeIsReportedInOneLineAndNoneWithinATolerance)
{
    EXPECT_FALSE(mutationsOf(GetParam().subject).empty());
    EXPECT_EQ(mutationProblems(GetParam().subject), "");
    EXPECT_EQ(reportWithinTheTolerances(GetParam()), "");
}

INSTANTIATE_TEST_SUITE_P(Changes, ExampleSimulationStableGoldenMutation,
                         ::testing::ValuesIn(kSubjectCases), subjectTestName);

// The tests above run every mutation: each belongs to one of the three simulations, and each
// has a name of its own for the report. No simulation is run here.
TEST(ExampleSimulationStableGoldenMutations, EveryMutationBelongsToASimulationOfTheTests)
{
    const std::vector<Mutation> all = mutations();
    EXPECT_EQ(all.size(), 44U);
    std::size_t covered = 0;
    for (const SubjectCase& subjectCase : kSubjectCases)
    {
        covered += mutationsOf(subjectCase.subject).size();
    }
    EXPECT_EQ(covered, all.size());
    const std::vector<std::string_view> names = sortedNames(all);
    EXPECT_EQ(std::ranges::adjacent_find(names), names.end()) << "two mutations of one name";
}

/// Changes to what the rules exclude in "Simulation 1" of the three-stage rocket: the minimum
/// and the maximum of columns that are not steady (the time and the vertical velocity), of the
/// sustainer too; and of its two boosters (rule T) what follows their separations but the
/// altitude of their apogees: the number of rows, the time to the optimum altitude, the
/// optimum delay and the times of the later events; and the number of jitter replacements of
/// the simulation.
void changeWhatTheRulesExcludeOfTheThreeStages(GoldenFiles& files)
{
    shiftAnExtreme(files, 0, kTimeColumn, "max", -0.5);
    shiftAnExtreme(files, 0, kVelocityColumn, "min", 0.5);
    shiftAnExtreme(files, 2, kTimeColumn, "max", -0.5);
    for (const std::size_t booster : {std::size_t{1}, std::size_t{2}})
    {
        json& branch                    = files.document["branches"][booster];
        branch["rows"]                  = 1;
        branch["timeToOptimumAltitude"] = 1.0;
        branch["optimumDelay"]          = 1.0;
        for (std::size_t event = 4; event < branch["events"].size(); event++)
        {
            branch["events"][event]["time"] = 100.0 + static_cast<double>(event);
        }
    }
    files.document["result"]["jitterReplacements"] = 1;
}

/// Removes the TUMBLE that the first booster of the three-stage rocket records after its
/// separation (the sixth of its golden events).
void removeTheTumbleOfTheFirstBooster(GoldenFiles& files)
{
    files.document["branches"][2]["events"].erase(5);
}

// The other side of the rules: a number they exclude is not compared, whatever it is. It is
// counted as excluded, under its rule, with the same sums. (What the rules leave of the same
// branches is compared: the mutations of the three-stage rocket above.)
TEST(ExampleSimulationStableGoldenMutations, ANumberTheRulesExcludeIsCountedNotCompared)
{
    const DocumentRun* run = subjectRun(kThreeStages);
    ASSERT_NE(run, nullptr);
    const DocumentResult unchanged = comparisonAfter(*run, nullptr);
    const DocumentResult changed = comparisonAfter(*run, changeWhatTheRulesExcludeOfTheThreeStages);
    EXPECT_EQ(unchanged.report, "");
    EXPECT_EQ(changed.report, "");
    EXPECT_EQ(changed.compared, unchanged.compared);
    EXPECT_EQ(changed.excluded, unchanged.excluded);
    EXPECT_EQ(changed.exclusions, unchanged.exclusions);
    EXPECT_EQ(unchanged.compared + unchanged.excluded, run->golden);
    // Three branches of 70 columns, of which 22 are steady in the sustainer and 20 in each
    // booster: the extremes of the others. Of each booster its rows, two optimum values and
    // four later events, and the jitter replacements of the simulation; the two TUMBLE events.
    EXPECT_EQ(unchanged.exclusions.extremes, 2 * ((70 - 22) + (2 * (70 - 20))));
    EXPECT_EQ(unchanged.steadyExtremes, 2 * (22 + (2 * 20)));
    EXPECT_EQ(changed.steadyExtremes, unchanged.steadyExtremes);
    EXPECT_EQ(unchanged.exclusions.tumbling, (2 * (3 + 4)) + 1);
    EXPECT_EQ(unchanged.excluded.events, 2);

    // A dropped stage without a recovery device tumbles or does not: the document matches
    // without the TUMBLE as well, with one event and one time less to leave out.
    const DocumentResult without = comparisonAfter(*run, removeTheTumbleOfTheFirstBooster);
    EXPECT_EQ(without.report, "");
    EXPECT_EQ(without.compared, unchanged.compared);
    EXPECT_EQ(without.excluded.events, 1);
    EXPECT_EQ(without.exclusions.tumbling, unchanged.exclusions.tumbling - 1);
}

// -------------------------------------------------------------------- rule E, the other grid

// Which of its branches a run takes the other step from the apogee in is the platform's to say
// (the last bit decides), so the tests of rule E make the other grid themselves: the document
// that a golden run on the other time grid than the run's would have left, with the steps of
// that golden run in the place of the list's.

/// The document of the other grid for a run, and the steps its golden run took from the apogees
/// of its branches (DocumentComparison::steps).
struct OtherGrid
{
    GoldenFiles                      files;
    std::vector<std::optional<bool>> steps;
};

/// The place of the first APOGEE among the golden events @p events (as they are in the
/// document); their number without one.
[[nodiscard]] std::size_t apogeeAmong(const json& events)
{
    for (std::size_t i = 0; i < events.size(); i++)
    {
        if (events.at(i).at("type").get<std::string>() == "APOGEE")
        {
            return i;
        }
    }
    return events.size();
}

/// The document of the golden simulation of @p run as a golden run would have left it that took,
/// from the apogee of every branch of rule E, the other step than @p run takes: where the golden
/// run of the files already did, the document as it is; where it stepped as the run does, the
/// document with what the extra step changes for certain: one row more or less, and the APOGEE
/// 1 ms later or earlier. (What the other grid moves by less than the tolerances of the rule,
/// the end of the descent, stays as it is.)
[[nodiscard]] OtherGrid documentOfTheOtherGrid(const DocumentRun& run)
{
    OtherGrid other{.files = run.files, .steps = apogeeStepsOf(run)};
    for (std::size_t i = 0; i < other.steps.size(); i++)
    {
        const std::optional<bool> ours = other.steps[i];
        if (!ours.has_value())
        {
            continue;
        }
        other.steps[i] = !*ours;
        if (documentPlan(run.files.document, i).shortApogeeStep != ours)
        {
            continue;  // the golden run of the files is on the other grid already
        }
        // The golden run of the other grid took the extra step exactly when the run does not.
        const double sign   = *ours ? -1.0 : 1.0;
        json&        branch = other.files.document["branches"][i];
        branch["rows"]      = branch.at("rows").get<std::int64_t>() + (*ours ? -1 : 1);
        json& apogee        = branch["events"][apogeeAmong(branch.at("events"))];
        apogee["time"]      = goldenValue(apogee.at("time")) + (sign * kExtraStep);
    }
    return other;
}

/// What the comparison of @p run with a copy of the document @p other of the other grid finds
/// after @p change changed the copy (null: nothing changed).
[[nodiscard]] DocumentResult comparisonOnTheOtherGrid(const DocumentRun& run,
                                                      const OtherGrid&   other,
                                                      void (*change)(GoldenFiles& files))
{
    GoldenFiles files = other.files;
    if (change != nullptr)
    {
        change(files);
    }
    return compareDocumentSimulation(files, run.run, run.context, nullptr, &other.steps);
}

/// The sustainer of "Simulation 1" of the three-stage rocket is a branch of rule E whose events
/// are, from the fifteenth on, RECOVERY_DEVICE_DEPLOYMENT (14), APOGEE (15), GROUND_HIT (16) and
/// SIMULATION_END (17); a BURNOUT before them is handled late (rule L).
[[nodiscard]] std::vector<Mutation> otherGridMutations()
{
    return {
        {.name    = "OneRowMore",
         .subject = kThreeStages,
         .apply =
             [](GoldenFiles& files) {
                 json& rows = files.document["branches"][0]["rows"];
                 rows       = rows.get<std::int64_t>() + 1;
             },
         .heading = " branch 0",
         .line    = "  rows, with the row of the extra step of rule E: expected "},
        {.name    = "OneRowLess",
         .subject = kThreeStages,
         .apply =
             [](GoldenFiles& files) {
                 json& rows = files.document["branches"][0]["rows"];
                 rows       = rows.get<std::int64_t>() - 1;
             },
         .heading = " branch 0",
         .line    = "  rows, with the row of the extra step of rule E: expected "},
        {.name    = "TimeOfTheApogee",
         .subject = kThreeStages,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/events/15/time", kBeyond * kLateHandlingAbsolute);
             },
         .heading = " branch 0",
         .line    = "  events[15].time, moved by the extra step of rule E: expected "},
        {.name    = "TimeOfTheGroundHit",
         .subject = kThreeStages,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/events/16/time", kBeyond * kOtherGridTimeAbsolute);
             },
         .heading = " branch 0",
         .line    = "  events[16].time: expected "},
        {.name    = "TimeOfTheEnd",
         .subject = kThreeStages,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/events/17/time", -kBeyond * kOtherGridTimeAbsolute);
             },
         .heading = " branch 0",
         .line    = "  events[17].time: expected "},
        {.name    = "FlightTime",
         .subject = kThreeStages,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/summary/flightTime", kBeyond * kOtherGridTimeAbsolute);
             },
         .heading = " summary",
         .line    = "  flightTime: expected "},
        {.name    = "GroundHitVelocity",
         .subject = kThreeStages,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/summary/groundHitVelocity", kBeyond * kOtherGridVelocityRelative);
             },
         .heading = " summary",
         .line    = "  groundHitVelocity: expected "},
        // What the other grid does not reach is compared as everywhere.
        {.name    = "TimeToApogee",
         .subject = kThreeStages,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/summary/timeToApogee", kBeyond * kStableTimeAbsolute);
             },
         .heading = " summary",
         .line    = "  timeToApogee: expected "},
        {.name    = "TimeOfTheDeploymentBeforeTheApogee",
         .subject = kThreeStages,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/events/14/time", kBeyond * kLateHandlingAbsolute);
             },
         .heading = " branch 0",
         .line    = "  events[14].time: expected "},
    };
}

/// Changes within the tolerances of a run on the other grid: the times after the apogee by a
/// hundred times the tolerance of a time on the grid of the golden run, and the ground hit
/// velocity by a quarter of its own.
void changeTheOtherGridWithinTheTolerances(GoldenFiles& files)
{
    shift(files, "/branches/0/events/16/time", 100.0 * kStableTimeAbsolute);
    shift(files, "/branches/0/events/17/time", -100.0 * kStableTimeAbsolute);
    shift(files, "/summary/flightTime", 100.0 * kStableTimeAbsolute);
    scale(files, "/summary/groundHitVelocity", kWithin * kOtherGridVelocityRelative);
}

/// What is wrong with the reports of the mutations @p all of a document of the other grid,
/// each named; "" when every one is reported in the heading and the one line that name it.
[[nodiscard]] std::string otherGridProblems(const DocumentRun& run, const OtherGrid& other,
                                            const std::vector<Mutation>& all)
{
    std::string problems;
    for (const Mutation& mutation : all)
    {
        problems += oneLineProblem(mutation, run.context,
                                   comparisonOnTheOtherGrid(run, other, mutation.apply).report);
    }
    return problems;
}

// Rule E: a run that steps from the apogee of a branch otherwise than the golden run is on the
// other time grid from there on. It has exactly the row of the extra step more, or less; its
// APOGEE is the golden one moved by that step; and the end of its descent (the GROUND_HIT, the
// SIMULATION_END, the flight time and the ground hit velocity) is compared at the tolerances of
// the other grid, a change beyond which is reported like any other.
TEST(ExampleSimulationStableGoldenMutations, ARunOnTheOtherGridIsComparedAtItsOwnTolerances)
{
    const DocumentRun* run = subjectRun(kThreeStages);
    ASSERT_NE(run, nullptr);
    const DocumentResult unchanged = comparisonAfter(*run, nullptr);
    const OtherGrid      other     = documentOfTheOtherGrid(*run);
    ASSERT_EQ(other.steps.size(), 3U);
    ASSERT_TRUE(other.steps[0].has_value());
    EXPECT_EQ(other.steps[1], std::nullopt);
    EXPECT_EQ(other.steps[2], std::nullopt);

    // The document of the other grid matches: the sustainer steps otherwise, and its rows, the
    // two events after its APOGEE, the flight time and the ground hit velocity are compared as
    // those of the other grid. Everything is compared that is compared on the same grid.
    const DocumentResult onTheOtherGrid = comparisonOnTheOtherGrid(*run, other, nullptr);
    EXPECT_EQ(onTheOtherGrid.report, "");
    EXPECT_EQ(onTheOtherGrid.unlike, 1);
    EXPECT_EQ(onTheOtherGrid.otherGrid, 5);
    EXPECT_EQ(onTheOtherGrid.compared, unchanged.compared);
    EXPECT_EQ(onTheOtherGrid.excluded, unchanged.excluded);

    const std::vector<Mutation>         all   = otherGridMutations();
    const std::vector<std::string_view> names = sortedNames(all);
    EXPECT_EQ(all.size(), 9U);
    EXPECT_EQ(std::ranges::adjacent_find(names), names.end()) << "two mutations of one name";
    EXPECT_EQ(otherGridProblems(*run, other, all), "");
    EXPECT_EQ(comparisonOnTheOtherGrid(*run, other, changeTheOtherGridWithinTheTolerances).report,
              "");
}

// ===================================================================================== rules

/// A branch of a run with the rows @p rows: the angle of attack (NaN: a row of an Euler
/// stepper), the vertical velocity and the time step of each.
[[nodiscard]] std::shared_ptr<FlightDataBranch> branchOfRows(
    const std::vector<std::array<double, 3>>& rows)
{
    const FlightDataType& aoa = FlightDataType::builtin(QtRocket::FlightDataTypeId::TYPE_AOA);
    const FlightDataType& velocity =
        FlightDataType::builtin(QtRocket::FlightDataTypeId::TYPE_VELOCITY_Z);
    const FlightDataType& step =
        FlightDataType::builtin(QtRocket::FlightDataTypeId::TYPE_TIME_STEP);
    auto branch = std::make_shared<FlightDataBranch>(
        "branch",
        std::initializer_list<std::reference_wrapper<const FlightDataType>>{aoa, velocity, step});
    for (const std::array<double, 3>& row : rows)
    {
        branch->addPoint();
        branch->setValue(aoa, row[0]);
        branch->setValue(velocity, row[1]);
        branch->setValue(step, row[2]);
    }
    return branch;
}

// Rule E: a run that steps from the apogee as the golden run did is compared to its end; one
// that steps otherwise is not, and its rows have to show that the last bit decided: the step
// to the apogee left a rounding error, and the extra step of 1 ms follows a positive one.
// Anything else is reported.
TEST(ExampleSimulationStableGoldenRules, AnotherStepFromTheApogeeIsExcusedOnlyByTheLastBit)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    // The device is out at 40 m/s; the stepper lands on the apogee in the fourth row.
    const auto regular = branchOfRows(
        {{0.1, 50.0, 0.01}, {nan, 40.0, 0.1}, {nan, 8.0, 0.1}, {nan, 0.0, 0.1}, {nan, -1.0, 0.1}});
    const auto extra = branchOfRows({{0.1, 50.0, 0.01},
                                     {nan, 40.0, 0.1},
                                     {nan, 8.0, 0.1},
                                     {nan, 8.9e-16, 0.001},
                                     {nan, -0.01, 0.1},
                                     {nan, -1.0, 0.1}});

    // A branch that the rule does not list: nothing is read from its rows.
    const ApogeeOutcome unlisted = apogeeOutcome(std::nullopt, *extra);
    EXPECT_TRUE(unlisted.alike);
    EXPECT_EQ(unlisted.later, 0.0);
    EXPECT_EQ(unlisted.problem, "");

    // The golden run took the regular step: a run with the extra step records its APOGEE 1 ms
    // later.
    EXPECT_TRUE(apogeeOutcome(false, *regular).alike);
    EXPECT_EQ(apogeeOutcome(false, *regular).later, 0.0);
    EXPECT_EQ(apogeeOutcome(false, *regular).problem, "");
    const ApogeeOutcome excused = apogeeOutcome(false, *extra);
    EXPECT_FALSE(excused.alike);
    EXPECT_EQ(excused.later, kExtraStep);
    EXPECT_EQ(excused.problem, "") << "the extra step follows a positive rounding error";
    EXPECT_EQ(excused.step.left, 8.9e-16);
    EXPECT_EQ(excused.step.before, 8.0);

    // The golden run took the extra step: a run without it records its APOGEE 1 ms earlier.
    EXPECT_TRUE(apogeeOutcome(true, *extra).alike);
    EXPECT_EQ(apogeeOutcome(true, *extra).later, 0.0);
    EXPECT_FALSE(apogeeOutcome(true, *regular).alike);
    EXPECT_EQ(apogeeOutcome(true, *regular).later, -kExtraStep);
    EXPECT_EQ(apogeeOutcome(true, *regular).problem, "");

    // A step to the apogee that stops short of it (1e-12 of the velocity is left), and an extra
    // step after no velocity at all: not the last bit.
    const auto shortOfIt =
        branchOfRows({{0.1, 50.0, 0.01}, {nan, 8.0, 0.1}, {nan, 8e-12, 0.001}, {nan, -0.01, 0.1}});
    EXPECT_EQ(apogeeOutcome(false, *shortOfIt).problem,
              "the step to the apogee leaves a vertical velocity of 8e-12 m/s after 8 m/s, which "
              "is no rounding error");
    const auto unprompted =
        branchOfRows({{0.1, 50.0, 0.01}, {nan, 8.0, 0.1}, {nan, 0.0, 0.001}, {nan, -0.01, 0.1}});
    EXPECT_EQ(apogeeOutcome(true, *unprompted).problem,
              "a step of 0.001 s follows a vertical velocity of 0 m/s at the apogee");
    EXPECT_TRUE(apogeeOutcome(true, *unprompted).alike);

    // A run whose recovery device is not out before its apogee: it does not match.
    const auto coasting =
        branchOfRows({{0.1, 50.0, 0.01}, {0.1, 8.0, 0.01}, {0.2, -1.0, 0.01}, {nan, -2.0, 0.1}});
    EXPECT_FALSE(apogeeOutcome(false, *coasting).alike);
    EXPECT_EQ(apogeeOutcome(false, *coasting).later, 0.0);
    EXPECT_EQ(apogeeOutcome(false, *coasting).problem,
              "no step of an Euler stepper lands on the apogee");
}

/// The document of a simulation with the index @p index of the golden input @p input, whose
/// branches have the events of @p branches: the types and times of each.
[[nodiscard]] json documentWith(
    std::string_view input, int index,
    std::initializer_list<std::initializer_list<std::pair<std::string_view, double>>> branches)
{
    json document = {{"input", std::string{input}}, {"index", index}, {"branches", json::array()}};
    for (const auto& events : branches)
    {
        json branch = {{"index", document["branches"].size()}, {"events", json::array()}};
        for (const auto& [type, time] : events)
        {
            branch["events"].push_back({{"type", std::string{type}}, {"time", time}});
        }
        document["branches"].push_back(std::move(branch));
    }
    return document;
}

// The plan of a branch is what its document and the lists say: the dropped stage without a
// recovery device is told from its events (rule T), the branch of rule E and the step of its
// golden run are those of the list, and the late handling is the event the list names, by its
// time (rule L). A branch of another simulation, or of another input, is under no rule.
TEST(ExampleSimulationStableGoldenRules, ThePlanOfABranchFollowsTheListsAndItsEvents)
{
    // "Simulation 1" of the three-stage rocket, in short: the sustainer is listed by rule E
    // with the extra step and by rule L with its event 8; the first booster is listed by rule L
    // with its event 1, and both boosters are dropped stages without a recovery device.
    const json staged = documentWith(
        "example-three-stage-low-power-rocket", 0,
        {{{"LAUNCH", 0.0},
          {"e1", 0.1},
          {"e2", 0.2},
          {"e3", 0.3},
          {"e4", 0.4},
          {"e5", 0.5},
          {"e6", 0.6},
          {"STAGE_SEPARATION", 0.7},
          {"BURNOUT", 1.714},
          {"RECOVERY_DEVICE_DEPLOYMENT", 7.4},
          {"APOGEE", 7.6}},
         {{"IGNITION", 0.857}, {"BURNOUT", 1.714}, {"STAGE_SEPARATION", 1.7141}, {"TUMBLE", 1.8}},
         {{"IGNITION", 0.0}, {"STAGE_SEPARATION", 0.857}, {"APOGEE", 2.3}}});
    const DocumentPlan sustainer = documentPlan(staged, 0);
    EXPECT_FALSE(sustainer.tumbles);
    EXPECT_EQ(sustainer.shortApogeeStep, std::optional<bool>{true});
    EXPECT_EQ(sustainer.lateFrom, 1.714);
    const DocumentPlan booster = documentPlan(staged, 1);
    EXPECT_TRUE(booster.tumbles);
    EXPECT_EQ(booster.shortApogeeStep, std::nullopt);
    EXPECT_EQ(booster.lateFrom, 1.714);
    const DocumentPlan first = documentPlan(staged, 2);
    EXPECT_TRUE(first.tumbles);
    EXPECT_EQ(first.shortApogeeStep, std::nullopt);
    EXPECT_TRUE(std::isinf(first.lateFrom));

    // The same events as "Simulation 3" of that rocket, which no list names, and as a
    // simulation of a design that is no example: the boosters are still told from their
    // events, and nothing else applies.
    json third     = staged;
    third["index"] = 2;
    EXPECT_EQ(documentPlan(third, 0).shortApogeeStep, std::nullopt);
    EXPECT_TRUE(std::isinf(documentPlan(third, 0).lateFrom));
    EXPECT_TRUE(documentPlan(third, 1).tumbles);
    EXPECT_TRUE(std::isinf(documentPlan(third, 1).lateFrom));
    json other     = staged;
    other["input"] = "testrocket-beta";
    EXPECT_EQ(documentPlan(other, 0).shortApogeeStep, std::nullopt);
    EXPECT_TRUE(documentPlan(other, 2).tumbles);

    // A list that names an event the branch does not have changes no tolerance.
    json shorter = staged;
    shorter["branches"][0]["events"].erase(8);
    shorter["branches"][0]["events"].erase(8);
    shorter["branches"][0]["events"].erase(8);
    EXPECT_TRUE(std::isinf(documentPlan(shorter, 0).lateFrom));
}

// What the documents themselves show of the rules, which the coverage test holds the lists
// against: a recovery device that is out before the apogee (rule E), and a deployment 1 ms
// after the apogee (a late handling, rule L).
TEST(ExampleSimulationStableGoldenRules, TheDocumentShowsADeviceBeforeTheApogeeAndADeploymentAtIt)
{
    const json  early       = documentWith("x", 0,
                                           {{{"LAUNCH", 0.0},
                                             {"RECOVERY_DEVICE_DEPLOYMENT", 4.861},
                                             {"APOGEE", 5.4168},
                                             {"GROUND_HIT", 72.4}}});
    const json& earlyEvents = early.at("branches").at(0).at("events");
    EXPECT_TRUE(deviceIsOutBeforeTheApogee(earlyEvents));
    EXPECT_EQ(deploymentAtTheApogee(earlyEvents), earlyEvents.size());

    const json  atTheApogee = documentWith("x", 0,
                                           {{{"LAUNCH", 0.0},
                                             {"APOGEE", 10.354},
                                             {"RECOVERY_DEVICE_DEPLOYMENT", 10.355},
                                             {"RECOVERY_DEVICE_DEPLOYMENT", 30.7},
                                             {"GROUND_HIT", 65.1}}});
    const json& lateEvents  = atTheApogee.at("branches").at(0).at("events");
    EXPECT_FALSE(deviceIsOutBeforeTheApogee(lateEvents));
    EXPECT_EQ(deploymentAtTheApogee(lateEvents), 2U);

    // A deployment by the ejection charge after the apogee, and a branch without an apogee.
    const json later = documentWith(
        "x", 0,
        {{{"APOGEE", 3.5071}, {"EJECTION_CHARGE", 3.73}, {"RECOVERY_DEVICE_DEPLOYMENT", 3.731}},
         {{"IGNITION", 0.0}, {"RECOVERY_DEVICE_DEPLOYMENT", 3.731}}});
    const json& laterEvents = later.at("branches").at(0).at("events");
    EXPECT_FALSE(deviceIsOutBeforeTheApogee(laterEvents));
    EXPECT_EQ(deploymentAtTheApogee(laterEvents), laterEvents.size());
    EXPECT_FALSE(deviceIsOutBeforeTheApogee(later.at("branches").at(1).at("events")));
}

// The tolerances are those of the stable-step comparison of the test rockets. Over what this
// comparison compares, each is at least a hundred times the largest difference of the two
// measurements of the examples (the top of the file has the table; the numbers here are its
// larger ones, rounded up).
TEST(ExampleSimulationStableGoldenRules, EveryToleranceIsAHundredTimesTheLargestMeasurement)
{
    // The tolerances themselves: a mutation of the tests is sized by its tolerance, so a
    // tolerance that grew would hide itself.
    EXPECT_EQ(kStableSummaryRelative, 1e-6);
    EXPECT_EQ(kDeploymentVelocityRelative, 1e-5);
    EXPECT_EQ(kValueRelative, 1e-9);
    EXPECT_EQ(kStableTimeAbsolute, 1e-4);
    EXPECT_EQ(kLateHandlingAbsolute, 1e-3);
    EXPECT_EQ(kApogeeResidual, 1e-13);
    EXPECT_EQ(kExtraStep, 1e-3);
    EXPECT_EQ(kOtherGridTimeAbsolute, 2e-2);
    EXPECT_EQ(kOtherGridVelocityRelative, 1e-4);
    EXPECT_EQ(kTumblingAltitudeRelative, 2e-2);
    EXPECT_EQ(kToleranceMargin, 100.0);
    // A summary value, an optimum altitude, the parameter of a warning: a maximum altitude.
    EXPECT_GE(kStableSummaryRelative, kToleranceMargin * 6.2e-9);
    // The minimum or the maximum of a steady column: a roll damping coefficient.
    EXPECT_GE(kStableSummaryRelative, kToleranceMargin * 1.7e-11);
    EXPECT_GE(kDeploymentVelocityRelative, kToleranceMargin * 1.9e-9);
    // The launch rod velocity.
    EXPECT_GE(kValueRelative, kToleranceMargin * 5.3e-14);
    // A time: the flight time; after a late handling, a GROUND_HIT.
    EXPECT_GE(kStableTimeAbsolute, kToleranceMargin * 5.8e-7);
    EXPECT_GE(kLateHandlingAbsolute, kToleranceMargin * 5.8e-7);
    // Rule E: what the step to the apogee leaves of the velocity; and, of a run on the other
    // time grid, the flight time and the ground hit velocity.
    EXPECT_GE(kApogeeResidual, kToleranceMargin * 1.7e-16);
    EXPECT_GE(kOtherGridTimeAbsolute, kToleranceMargin * 1.9e-4);
    EXPECT_GE(kOtherGridVelocityRelative, kToleranceMargin * 9.3e-7);
    // Rule T: the altitude of the apogee of a dropped stage that tumbles.
    EXPECT_GE(kTumblingAltitudeRelative, kToleranceMargin * 1.1e-4);
    // ... and never beyond the plan's bounds: 1e-3 s for an event time, 1e-4 for the apogee
    // and the maximum velocity.
    EXPECT_LE(kStableTimeAbsolute, 1e-3);
    EXPECT_LE(kLateHandlingAbsolute, 1e-3);
    EXPECT_LE(kStableSummaryRelative, 1e-4);
    // What rule E can excuse is what OpenRocket's own runs differ by when they step otherwise:
    // more than the tolerances (the APOGEE by the 1 ms of the extra step, the flight time by
    // 1.9e-4 s), which is why it is a rule and not a tolerance.
    EXPECT_GT(1.0e-3, kStableTimeAbsolute);
    EXPECT_GT(1.9e-4, kStableTimeAbsolute);
}

// ================================================================================ sensitivity

/// @p run in the form of the document of a golden simulation, as far as the comparison of the
/// numbers reads it (goldenFilesOf() of GoldenStableSimulations.h, and what a document has
/// beyond the golden files of a test rocket: the input and the index of the simulation, by
/// which the lists of the rules know its branches, the index of every branch and the keys of
/// its columns).
[[nodiscard]] GoldenFiles documentOf(const DocumentRun& run)
{
    GoldenFiles files       = goldenFilesOf(run.run);
    files.document["input"] = run.files.document.at("input");
    files.document["index"] = run.files.document.at("index");
    for (std::size_t i = 0; i < files.tables.size(); i++)
    {
        json& branch    = files.document["branches"][i];
        branch["index"] = i;
        for (std::size_t column = 0; column < files.tables[i].columns.size(); column++)
        {
            branch["columns"][column]["key"] = files.tables[i].columns[column];
        }
    }
    files.tables.clear();
    return files;
}

/// What the sensitivity measurement prints.
struct KickedExamples
{
    std::string        problems;
    StableMeasurements measurements;
    Exclusions         exclusions;
    int                unlike{0};  ///< rule E: the branches that the kick makes step otherwise
    /// The numbers of the events of the branches whose sequence of events the kick changes
    /// (they are compared under no rule; a change before the separation of rule T counts).
    int         unpaired{0};
    std::string report;  ///< what the kick moves beyond a tolerance
};

/// Runs every simulation of @p example once more, in a second load of the example, with a
/// KickListener whose seed is @p seed and the seeds after it, and compares each kicked run with
/// the run of the session as that is compared with its golden document.
void kick(KickedExamples& kicked, const GoldenExample& example, std::uint64_t& seed)
{
    const DefaultUnitsGuard units;  // the texts of the warnings print units
    const ExampleSession&   session = stableRuns(example);
    Result<LoadedDocument>  loaded  = loadSettledGoldenExample(example);
    kicked.problems += session.problems;
    if (!loaded)
    {
        kicked.problems += std::format("{}: {}\n", example.file, loaded.error().message);
        return;
    }
    const auto document = std::make_shared<LoadedDocument>(std::move(*loaded));
    for (std::size_t i = 0; i < session.runs.size(); i++)
    {
        const DocumentRun&  run  = session.runs[i];
        const SimulationRun twin = runDocumentSimulation(
            document, i,
            {.stableTimeStep = kStableTimeStep,
             .perturbation   = std::make_shared<KickListener>(kKickSize, seed++)});
        if (!run.problem.empty() || !twin.problem.empty() || run.run.data == nullptr)
        {
            kicked.problems += std::format("{}: {}{}\n", example.name, run.problem, twin.problem);
            continue;
        }
        const std::vector<std::optional<bool>> steps = apogeeStepsOf(run);
        DocumentComparison                     d;
        d.c.context      = run.context;
        d.c.measurements = &kicked.measurements;
        d.steps          = &steps;
        compareDocumentNumbers(d, documentOf(run), twin);
        kicked.exclusions += d.excluded;
        kicked.unlike += d.unlike;
        kicked.unpaired += d.c.excluded.numbers - d.excluded.total();
        kicked.report += d.c.report;
    }
}

/// The sensitivity of what the comparison with the documents compares: every simulation of
/// every example is run once more with a KickListener (a relative 1e-10 kick to the velocity
/// and to the rotation velocity after every step, about a million times what one ulp of a libm
/// result is) and compared with the unperturbed run under the rules and at the tolerances of
/// the comparison with the golden documents; the step the unperturbed run takes from an apogee
/// of rule E stands for the golden run's. Prints the table of
/// ExampleSimulationStableGoldenMeasurement for the two runs: by how much the kick moves each
/// summary value and event time, and then what it moves beyond a tolerance. The kick is a
/// physical perturbation, which the trajectory follows in proportion; what the measurement
/// shows is whether anything that is compared moves out of proportion: a row count, a sequence
/// of events or the time of an event that jumps where the rules do not say so. Disabled; run it
/// with --gtest_also_run_disabled_tests.
TEST(ExampleSimulationStableGoldenMeasurement, DISABLED_PrintsTheSensitivityToAKick)
{
    KickedExamples kicked;
    std::uint64_t  seed = 1;
    for (const GoldenExample& example : goldenExamples())
    {
        kick(kicked, example, seed);
    }
    std::cout << "excluded: " << kicked.exclusions << "; " << kicked.unlike
              << " branches step from the apogee otherwise than the unperturbed run; "
              << kicked.unpaired << " numbers of events whose sequence changed\n"
              << "what\tvalues\tlargest difference\t... as a multiple of its tolerance\twhere\n"
              << kicked.measurements.text() << "Beyond the tolerances of the comparison:\n"
              << kicked.report;
    EXPECT_EQ(kicked.problems, "");
}

}  // namespace
