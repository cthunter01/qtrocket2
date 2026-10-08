// Flights with OpenRocket's example extensions RollControl and AirStart against OpenRocket's own.
//
// OpenRocket has no test of the two extensions. The pins are the output of a Java probe on
// OpenRocket's compiled core (probes/tier9a-extensions/java: ExtPins2.java, './run.sh ExtPins2
// 0.01' and '... 0.05'), turned into ExtensionFlightPins.h by gen_flight_pins.py. The probe and
// ExtensionFlightSupport.cpp build the same fourteen scenarios, run them the same way and print
// the same lines, so a flight here is compared with OpenRocket's line by line and number by
// number:
// - the Iso-Haisu test rocket (a heavy single stage with two "CONTROL" fins and no recovery
//   device) with an M1350 and its main fins canted by 0.02 rad: without an extension; with
//   RollControl; with RollControl and AirStart; with an AirStart of 250 m and 20 m/s; with a
//   RollControl whose every setting is changed (its limit of 0.05 rad is reached); with
//   RollControl and with nothing as the golden harness runs a simulation (the pitch/yaw
//   jitter removed, the listeners of the extensions between the two jitter listeners); and
//   with a RollControl whose fin set does not exist;
// - the Estes Alpha III with a C6 whose ejection charge fires 3 s after the burnout, before
//   the apogee (so the engine runs its nested optimum-coast simulation), its three fins canted
//   by 0.02 rad: without, and with a RollControl that turns those same fins;
// - the two-stage Beta: the booster's fins canted and a RollControl on the sustainer's fins,
//   and the sustainer's fins canted and a RollControl on the fins of the booster that is
//   dropped, each with its flight without the extension.
// Every simulation is in calm air with the options of the probe, the random seed 0, and a
// listener after those of the extensions that watches the cant of the controlled fin set in
// the rocket the engine simulates.
//
// WHAT THE PINS SHOW (and the comparison checks):
// - The caller's rocket is never touched: the cant before and after a run is the design's.
// - The controller starts from a fin angle of 0, not from the cant of the fin set: the canted
//   fins of the Alpha III are turned back at the first controlled step.
// - Exactly one row holds a fin cant before the start time: the value of a step is written to
//   the row the step began with.
// - After a stage separation each branch has the column. The listener of the sustainer's
//   branch and the clone of it that the dropped stage's branch got both go on turning the one
//   fin set: a RollControl on the sustainer's fins leaves them at the limit when the booster's
//   branch ends (its tumbling booster reports a roll rate that the fins it does not have
//   cannot change), and a RollControl on the booster's fins drives those to the limit during
//   the sustainer's flight.
// - The cant is put back when the simulation ends.
// - The optimum altitude of the flight whose recovery device opens before the apogee is that
//   of the flight without the extension: the nested simulation runs without the listener.
//
// TOLERANCES. A flight is compared at a time step of 0.01 s. What is compared there, and how
// closely, comes from three measurements of every number the probe prints
// (ExtensionFlightMeasurement, disabled, makes the last two):
// J, OpenRocket against itself: six runs of the probe, which differ in nothing but the random
//    ids of the components (the order in which hash maps are summed);
// C, this port (Linux, glibc) against those runs;
// K, this port against itself with a kick: every component of the velocity and of the rotation
//    velocity multiplied by a factor next to 1 after every step. Fifty-three runs: without a
//    kick, with fifty kicks of 1e-15 to 7e-11 either way (a difference in the last bits of
//    every step, and up), and with 1e-10 either way, a million times a difference in the last
//    bit.
// C is of the size of J in every scenario: the port behaves as one more run of OpenRocket.
// The kicks tell two kinds of numbers apart. Most follow a kick in proportion and do not move
// under a small one: they are reproducible, and compared within a tolerance. The others move
// as much under a kick of 1e-15 as under one of 7e-11, because a threshold or the step sizes
// of a controller decide them: another libm decides them another way (the tests run on three
// platforms), so they are not compared, or compared within what their mechanism bounds. The
// rules were worked out with twenty of the small kicks and then checked with the other thirty,
// to which nothing was fitted: no kicked run and none of the runs of OpenRocket differs from
// the pins under the rules, and the numbers below are the largest of all those runs.
// - The Iso-Haisu, and the Alpha III without the extension, are reproducible: J and C at most
//   5e-8 of a value (the flight time), and 9.1e-8 under the kick of 1e-10, less under the
//   smaller ones in proportion. No count and no event changes in any run. Compared at 1e-6
//   (TIGHT), of which the kick of 1e-10 uses 0.09; the times of the events at 1e-4 s
//   (INSTANT, what simulation_golden_tests.cpp allows a time at this step); the times from the
//   tumbling on, and those from the apogee of the Alpha III on, at 0.05 s (TIME), since when a
//   rocket starts to tumble is decided by a threshold and the Alpha III passes its apogee
//   under the landing stepper, whose step to the apogee the last bit decides (rule E of
//   simulation_golden_tests.cpp). The extremes of the Iso-Haisu's fin cant are those of the
//   end of its flight, when it tumbles (J 2.2e-5, and up to 3.1e-7 under a small kick): at
//   5e-3 (COARSE); what the controller does in flight is compared at 1e-6 in three rows.
// - The Alpha III with RollControl: the controller overshoots, and its loop decides the step
//   sizes, so a row index is no longer a time. An event is handled at the end of the step that
//   reaches it, and a step that is cut short may not be shorter than a millisecond: in 4 of
//   the 21 runs the rules were worked out with, of every size of kick, the ejection charge and
//   the deployment come 0.25 to 0.48 ms later than in OpenRocket's run, and the deployment
//   velocity is another by 6e-4. So the events after the start of the controller are compared
//   at 0.05 s, the values of the flight at 5e-3 (they move by 1.4e-5 at most, the deployment
//   velocity by the 6e-4), the numbers of rows and of steps at 0.2 (LOOSE; 12 of 2410 rows),
//   the largest and the last fin cant, a small overshoot late in the flight, at 0.2 of the
//   column's scale, and a value at a row index beyond the start of the controller not at all.
// - The Beta is not reproducible to more than a few digits even at this step and without the
//   extension, and with RollControl the six runs of OpenRocket differ by up to 1.8e-5 in the
//   maximum altitude, 4.4e-4 in the deployment velocity and 8.3e-3 in the extremes of the roll
//   rate. The sustainer's branch is compared at 5e-3 (the runs differ by 4e-4 at most), its
//   times at 0.05 s (7.6 ms), its counts and the extremes of its roll rate at 0.2 (1.1 %).
//   The dropped booster tumbles from 0.15 s after the separation, in steps of the shortest
//   size: its numbers of rows differ by up to 11 % between the runs and the number of its rows
//   without a roll rate by 19 %, under any kick, so the counts of a dropped stage's branch are
//   not compared (as rule T of simulation_golden_tests.cpp has it), nor is the number of steps
//   of the whole flight. Its times and its altitudes after the separation differ by up to
//   3.6 % and are compared at 0.2, a sanity check. What is exact in its branch is what
//   matters: the columns, the events in order, and, with the RollControl on the fins of the
//   sustainer, the fin cant at the limit when the branch ends. With the RollControl on the
//   fins of the booster, the sustainer's listener drives them to the limit, which is
//   compared; where the tumbling booster's own listener has them when its branch ends is not
//   reproducible (at the limit of either side in most runs, at -0.22 in 2 of the 21) and is
//   not compared.
// A value of a column is measured against the scale of the column (the larger magnitude of its
// minimum and maximum), any other value against itself.
//
// At the default time step of 0.05 s the flights of the light rockets are chaotic (see the
// header of simulation_golden_tests.cpp), so only the structure is compared there: the names,
// the outcome, the warnings, the branches with their events in order and their columns, and
// what must be exact at any step (the cant of the caller's rocket, the cant put back, the one
// row before the start time). The structure does not depend on the last bits: all fifty-three
// kicked runs at this step have it too.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/extension/example/AirStart.h"
#include "QtRocket/simulation/extension/example/RollControl.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Strings.h"
#include "simulation/SimulationRunSupport.h"
#include "simulation/extension/example/ExtensionFlightPins.h"
#include "simulation/extension/example/ExtensionFlightSupport.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::AirStart;
using QtRocket::Coordinate;
using QtRocket::ErrorCode;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightEvent;
using QtRocket::Result;
using QtRocket::RollControl;
using QtRocket::Simulation;
using QtRocket::SimulationListener;
using QtRocket::SimulationStatus;
using QtRocket::Warning;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::ExtensionFlightPin;
using QtRocket::Test::ExtensionFlightScenario;
using QtRocket::Test::extensionFlightScenarioIds;
using QtRocket::Test::kExtensionFlightDefaultPins;
using QtRocket::Test::kExtensionFlightStablePins;
using QtRocket::Test::makeExtensionFlightScenario;
using QtRocket::Test::probeLineKind;
using QtRocket::Test::probeTokens;
using QtRocket::Test::runExtensionFlightScenario;

// ================================================================================ the rules

/// How a number of a probe line is compared with OpenRocket's (see "TOLERANCES" above).
enum class Rule
{
    EXACT,    ///< the same number
    TIGHT,    ///< within 1e-6 of the scale
    MEDIUM,   ///< within 1e-4 of the scale
    COARSE,   ///< within 5e-3 of the scale
    LOOSE,    ///< within 0.2 of the scale
    COUNT,    ///< a count: within 2 % and 2
    INSTANT,  ///< a time of a reproducible flight: within 1e-4 s
    TIME,     ///< a time: within 0.05 s
    SKIP,     ///< not compared: not reproducible
};

/// The name of @p rule, for a report.
[[nodiscard]] std::string_view ruleName(Rule rule) noexcept
{
    switch (rule)
    {
        case Rule::EXACT:
            return "exact";
        case Rule::TIGHT:
            return "1e-6";
        case Rule::MEDIUM:
            return "1e-4";
        case Rule::COARSE:
            return "5e-3";
        case Rule::LOOSE:
            return "0.2";
        case Rule::COUNT:
            return "a count";
        case Rule::INSTANT:
            return "1e-4 s";
        case Rule::TIME:
            return "0.05 s";
        case Rule::SKIP:
            break;
    }
    return "not compared";
}

/// Where a line of the probe is: which rocket flies, whether a RollControl acts, and in which
/// branch of the flight data the line is (-1 before the first branch).
struct Context
{
    char family{'h'};  ///< 'h' the Iso-Haisu, 'a' the Alpha III, 'b' the Beta
    bool roll{false};
    bool boosterControlled{false};  ///< the RollControl turns the fins of the dropped stage
    int  branch{-1};
};

[[nodiscard]] Context contextOf(std::string_view scenario)
{
    Context context;
    context.family            = scenario.empty() ? 'h' : scenario.front();
    context.roll              = scenario.contains("roll");
    context.boosterControlled = scenario == "beta-roll-booster";
    return context;
}

/// Whether the line is in the branch of a dropped stage, which tumbles from the separation on:
/// nothing but the flight before the separation is reproducible there.
[[nodiscard]] bool isDropped(const Context& c) noexcept
{
    return c.branch > 0;
}

/// Whether the flight is one that two runs reproduce (see "TOLERANCES"): the Iso-Haisu, and
/// the Alpha III without a controller.
[[nodiscard]] bool isReproducible(const Context& c) noexcept
{
    return c.family == 'h' || (c.family == 'a' && !c.roll);
}

/// A value of the flight of the stage a branch follows.
[[nodiscard]] Rule trajectoryRule(const Context& c)
{
    if (isDropped(c))
    {
        return Rule::LOOSE;
    }
    return isReproducible(c) ? Rule::TIGHT : Rule::COARSE;
}

/// A number of rows or of steps.
[[nodiscard]] Rule countRule(const Context& c)
{
    if (isDropped(c))
    {
        // A tumbling stage takes steps at the minimum, and as many as the last bits decide.
        return Rule::SKIP;
    }
    return isReproducible(c) ? Rule::COUNT : Rule::LOOSE;
}

/// A time that depends on when a rocket began to tumble or passed its apogee under an Euler
/// stepper.
[[nodiscard]] Rule lateTimeRule(const Context& c)
{
    return isDropped(c) ? Rule::LOOSE : Rule::TIME;
}

/// A value at a row index.
[[nodiscard]] Rule rowRule(const Context& c, std::string_view key)
{
    if (c.family == 'h')
    {
        return Rule::TIGHT;
    }
    if (key == "at600")
    {
        // Beyond the start of a controller the row index is not a time any more.
        return c.roll ? Rule::SKIP : Rule::COARSE;
    }
    return c.family == 'a' ? Rule::MEDIUM : Rule::COARSE;
}

[[nodiscard]] Rule summaryRule(const Context& c, std::string_view key)
{
    if (key == "flightTime")
    {
        return Rule::TIME;
    }
    if (key == "timeToApogee")
    {
        return c.family == 'h' ? Rule::INSTANT : Rule::TIME;
    }
    if (key == "optimumDelay")
    {
        return isReproducible(c) ? Rule::INSTANT : Rule::TIME;
    }
    if (key == "groundHitVelocity")
    {
        return c.family == 'b' ? Rule::COARSE : Rule::MEDIUM;
    }
    return trajectoryRule(c);
}

[[nodiscard]] Rule branchRule(const Context& c, std::string_view key)
{
    if (key == "rows")
    {
        return countRule(c);
    }
    if (key == "types")
    {
        return Rule::EXACT;
    }
    if (key == "timeToOptimumAltitude")
    {
        return c.family == 'h' ? Rule::INSTANT : lateTimeRule(c);
    }
    return trajectoryRule(c);
}

/// The time of an event of type @p type.
[[nodiscard]] Rule eventRule(const Context& c, std::string_view type)
{
    const bool late = type == "TUMBLE" || type == "GROUND_HIT" || type == "SIMULATION_END";
    if (isDropped(c))
    {
        return late || type == "APOGEE" ? Rule::LOOSE : Rule::TIME;
    }
    if (late)
    {
        return Rule::TIME;
    }
    if (isReproducible(c))
    {
        // The apogee of the Alpha III is passed under the landing stepper (rule E).
        return type == "APOGEE" && c.family == 'a' ? Rule::TIME : Rule::INSTANT;
    }
    // A flight whose steps a controller or the hunting of a light rocket decides: an event is
    // handled at the end of the step that reaches it, up to a millisecond late. Before the
    // controller of the Alpha III starts, the flight is the one without it.
    const bool beforeTheController =
        type == "LAUNCH" || type == "IGNITION" || type == "LIFTOFF" || type == "LAUNCHROD";
    return c.family == 'a' && beforeTheController ? Rule::INSTANT : Rule::TIME;
}

/// The extremes of the roll rate.
[[nodiscard]] Rule rollRateRule(const Context& c)
{
    if (c.family == 'b')
    {
        return c.roll || isDropped(c) ? Rule::LOOSE : Rule::COARSE;
    }
    return trajectoryRule(c);
}

/// The minimum, the maximum and the last value of the fin cant column.
[[nodiscard]] Rule finCantRule(const Context& c, std::string_view key)
{
    if (c.family == 'h')
    {
        // The extremes are those of the end of the flight, when the rocket tumbles.
        return Rule::COARSE;
    }
    if (c.family == 'a')
    {
        return key == "min" ? Rule::COARSE : Rule::LOOSE;
    }
    // The Beta. A controller on the fins of the sustainer leaves them at the limit in the
    // booster's branch, and one on the fins of the booster drives them there in the
    // sustainer's; what the booster's own controller does to them while it tumbles is not
    // reproducible.
    if (isDropped(c) && c.boosterControlled)
    {
        return Rule::SKIP;
    }
    const bool atTheLimit = isDropped(c) || c.boosterControlled;
    if (key == "max")
    {
        return atTheLimit ? Rule::COARSE : Rule::LOOSE;
    }
    if (key == "min")
    {
        return atTheLimit ? Rule::TIGHT : Rule::COARSE;
    }
    return atTheLimit ? Rule::TIGHT : Rule::LOOSE;
}

/// A number of the line of the column @p label.
[[nodiscard]] Rule columnRule(const Context& c, std::string_view label, std::string_view key)
{
    if (key == "nan" || key == "first")
    {
        return countRule(c);
    }
    if (key.starts_with("at"))
    {
        return rowRule(c, key);
    }
    if (label == "finCant")
    {
        return finCantRule(c, key);
    }
    if (label == "rollRate")
    {
        return rollRateRule(c);
    }
    if (label == "time")
    {
        return key == "min" ? Rule::EXACT : lateTimeRule(c);
    }
    if (label == "velocity" && key == "last" && !isDropped(c))
    {
        return c.family == 'b' ? Rule::COARSE : Rule::MEDIUM;  // the ground hit velocity
    }
    return trajectoryRule(c);
}

/// A number of the line of the probe's listener, which watches the controlled fin set through
/// every branch of the flight.
[[nodiscard]] Rule spyRule(const Context& c, std::string_view key)
{
    if (key == "steps")
    {
        // The steps of the Beta include those of its tumbling booster.
        return c.family == 'b' ? Rule::SKIP : countRule(c);
    }
    if (key != "cantLast" && key != "cantMaxAbs")
    {
        return Rule::EXACT;  // the cant at the start and at the end, and the number of ends
    }
    if (c.family == 'h')
    {
        return Rule::COARSE;
    }
    if (c.family == 'a')
    {
        return key == "cantLast" ? Rule::LOOSE : Rule::TIGHT;
    }
    // The Beta: the limit, which the fins reach in every run, and where they are at the end,
    // which the booster's own controller leaves anywhere.
    return key == "cantLast" && c.boosterControlled ? Rule::SKIP : Rule::TIGHT;
}

/// The rule of the number of key @p key in a line whose tokens are @p tokens.
[[nodiscard]] Rule ruleOf(const Context& c, const std::vector<std::string_view>& tokens,
                          std::string_view key)
{
    const std::string_view first = tokens.empty() ? std::string_view{} : tokens[0];
    if (first == "summary")
    {
        return summaryRule(c, key);
    }
    if (first == "branch")
    {
        return branchRule(c, key);
    }
    if (first == "event" && tokens.size() > 1)
    {
        return eventRule(c, tokens[1]);
    }
    if (first == "column" && tokens.size() > 1)
    {
        return columnRule(c, tokens[1], key);
    }
    if (first == "spy")
    {
        return spyRule(c, key);
    }
    if (first.starts_with("jitterReplacements"))
    {
        return countRule(c);
    }
    return Rule::EXACT;  // the caller's cant, the rows around the start time, the time step
}

// =========================================================================== the comparison

/// A token "key=value" whose value is a number: the key and the number.
struct NumberToken
{
    std::string_view key;
    double           value;
};

[[nodiscard]] std::optional<NumberToken> numberToken(std::string_view token)
{
    const std::size_t equals = token.find('=');
    if (equals == std::string_view::npos)
    {
        return std::nullopt;
    }
    const std::optional<double> value =
        QtRocket::Strings::javaParseDouble(token.substr(equals + 1));
    if (!value.has_value())
    {
        return std::nullopt;
    }
    return NumberToken{.key = token.substr(0, equals), .value = *value};
}

/// The number of key @p key among @p tokens, or nullopt.
[[nodiscard]] std::optional<double> numberOf(const std::vector<std::string_view>& tokens,
                                             std::string_view                     key)
{
    for (const std::string_view token : tokens)
    {
        const std::optional<NumberToken> number = numberToken(token);
        if (number.has_value() && number->key == key)
        {
            return number->value;
        }
    }
    return std::nullopt;
}

/// The scale the numbers of the pinned line @p tokens are measured against, or 0 for a line
/// whose numbers are measured against themselves: for a column the larger magnitude of its
/// minimum and maximum, for the probe's listener the largest cant it saw.
[[nodiscard]] double scaleOf(const std::vector<std::string_view>& tokens)
{
    if (tokens.empty())
    {
        return 0.0;
    }
    if (tokens[0] == "spy")
    {
        return std::abs(numberOf(tokens, "cantMaxAbs").value_or(0.0));
    }
    if (tokens[0] != "column")
    {
        return 0.0;
    }
    const double minimum = std::abs(numberOf(tokens, "min").value_or(0.0));
    const double maximum = std::abs(numberOf(tokens, "max").value_or(0.0));
    const double scale   = std::fmax(minimum, maximum);
    return std::isnan(scale) ? 0.0 : scale;
}

/// The largest difference @p rule allows for a number that OpenRocket has as @p pinned, with
/// the scale @p scale; 0 for a rule without a tolerance.
[[nodiscard]] double allowedUnder(Rule rule, double pinned, double scale)
{
    const double reference = std::fmax(std::abs(pinned), scale);
    switch (rule)
    {
        case Rule::TIGHT:
            return 1e-6 * reference;
        case Rule::MEDIUM:
            return 1e-4 * reference;
        case Rule::COARSE:
            return 5e-3 * reference;
        case Rule::LOOSE:
            return 0.2 * reference;
        case Rule::COUNT:
            return (0.02 * std::abs(pinned)) + 2;
        case Rule::INSTANT:
            return 1e-4;
        case Rule::TIME:
            return 0.05;
        case Rule::EXACT:
        case Rule::SKIP:
            break;
    }
    return 0.0;
}

/// How much of its tolerance the difference between @p actual and @p pinned uses under
/// @p rule, with the scale @p scale: 0 for numbers that are the same (and for what is not
/// compared), up to 1 for a difference within the tolerance, more than 1 beyond it, and
/// infinity where no tolerance covers the difference (an exact rule, a NaN on one side only).
[[nodiscard]] double toleranceUsed(Rule rule, double pinned, double actual, double scale)
{
    constexpr double kInfinity = std::numeric_limits<double>::infinity();
    if (rule == Rule::SKIP)
    {
        return 0.0;
    }
    if (std::isnan(pinned) || std::isnan(actual))
    {
        return std::isnan(pinned) && std::isnan(actual) ? 0.0 : kInfinity;
    }
    const double difference = std::abs(actual - pinned);
    if (difference == 0.0)
    {
        return 0.0;
    }
    const double allowed = allowedUnder(rule, pinned, scale);
    if (difference <= allowed)
    {
        return std::fmin(difference / allowed, 1.0);
    }
    return allowed > 0.0 ? std::fmax(difference / allowed, std::nextafter(1.0, 2.0)) : kInfinity;
}

/// The largest part of its tolerance that each number of a comparison used (toleranceUsed()),
/// by where the number is and the rule it is compared by: what the measurement of the
/// tolerances collects.
using ToleranceUse = std::map<std::string, double>;

/// Whether the token @p pinned is one the structure of a flight consists of: a text, or one of
/// the numbers that are exact at any time step.
[[nodiscard]] bool isStructural(std::string_view pinned)
{
    const std::optional<NumberToken> number = numberToken(pinned);
    if (!number.has_value())
    {
        // "at600=none" depends on the number of rows.
        return !pinned.ends_with("=none");
    }
    static constexpr std::array<std::string_view, 9> kExactKeys{
        "dt",      "types", "cantBefore",        "cantAfter",       "cantStart",
        "cantEnd", "ends",  "nanAtOrAfterStart", "valueBeforeStart"};
    return std::ranges::any_of(kExactKeys,
                               [&number](std::string_view key) { return number->key == key; });
}

/// How two outputs of the probe are compared.
enum class Mode
{
    VALUES,     ///< every number by its rule
    STRUCTURE,  ///< the texts and the numbers that are exact at any time step
};

/// The difference between the tokens @p pinned and @p actual of a line, or "" when they agree.
/// A number that is compared by a rule with a tolerance is noted in @p use when that is not
/// null.
[[nodiscard]] std::string tokenDifference(const Context&                       context,
                                          const std::vector<std::string_view>& pinnedTokens,
                                          std::string_view pinned, std::string_view actual,
                                          Mode mode, ToleranceUse* use)
{
    if (mode == Mode::STRUCTURE && !isStructural(pinned))
    {
        return "";
    }
    const std::optional<NumberToken> pinnedNumber = numberToken(pinned);
    const std::optional<NumberToken> actualNumber = numberToken(actual);
    if (!pinnedNumber.has_value() || !actualNumber.has_value() ||
        pinnedNumber->key != actualNumber->key)
    {
        return pinned == actual ? "" : std::format("'{}' (OpenRocket: '{}')", actual, pinned);
    }
    const Rule rule =
        mode == Mode::STRUCTURE ? Rule::EXACT : ruleOf(context, pinnedTokens, pinnedNumber->key);
    const double used =
        toleranceUsed(rule, pinnedNumber->value, actualNumber->value, scaleOf(pinnedTokens));
    if (use != nullptr && rule != Rule::EXACT && rule != Rule::SKIP)
    {
        double& worst =
            (*use)[std::format("branch {} {} {} [{}]", context.branch, probeLineKind(pinnedTokens),
                               pinnedNumber->key, ruleName(rule))];
        worst = std::fmax(worst, used);
    }
    if (used <= 1.0)
    {
        return "";
    }
    return std::format("{} (OpenRocket: {}; tolerance: {})", actual, pinned, ruleName(rule));
}

/// The differences between the lines @p pinned and @p actual of one line of the probe.
void compareLine(std::vector<std::string>& differences, Context& context, std::string_view pinned,
                 std::string_view actual, Mode mode, ToleranceUse* use)
{
    const std::vector<std::string_view> pinnedTokens = probeTokens(pinned);
    const std::vector<std::string_view> actualTokens = probeTokens(actual);
    if (!pinnedTokens.empty() && pinnedTokens[0] == "branch")
    {
        context.branch++;
    }
    const std::string kind = probeLineKind(pinnedTokens);
    if (pinnedTokens.size() != actualTokens.size())
    {
        differences.push_back(std::format("{}: '{}' (OpenRocket: '{}')", kind, actual, pinned));
        return;
    }
    for (std::size_t i = 0; i < pinnedTokens.size(); i++)
    {
        const std::string difference =
            tokenDifference(context, pinnedTokens, pinnedTokens[i], actualTokens[i], mode, use);
        if (!difference.empty())
        {
            differences.push_back(
                std::format("branch {} {}: {}", context.branch, kind, difference));
        }
    }
}

/// The differences between the lines @p actual of a run of the scenario @p scenario and the
/// lines @p pinned of OpenRocket's run. With @p use, how much of its tolerance every number
/// used is noted there as well (the measurement of the tolerances).
[[nodiscard]] std::vector<std::string> differencesFrom(std::string_view                  scenario,
                                                       std::span<const std::string_view> pinned,
                                                       const std::vector<std::string>&   actual,
                                                       Mode mode, ToleranceUse* use = nullptr)
{
    std::vector<std::string> differences;
    if (pinned.size() != actual.size())
    {
        differences.push_back(
            std::format("{} lines (OpenRocket: {})", actual.size(), pinned.size()));
        return differences;
    }
    Context context = contextOf(scenario);
    for (std::size_t i = 0; i < pinned.size(); i++)
    {
        compareLine(differences, context, pinned[i], actual[i], mode, use);
    }
    return differences;
}

/// @p texts, one per line.
[[nodiscard]] std::string joined(const std::vector<std::string>& texts)
{
    return QtRocket::Strings::join("\n", texts);
}

// ================================================================================ the flights

/// The name of a test parameterised by a pin: its scenario with '_' for '-'.
[[nodiscard]] std::string pinTestName(const ::testing::TestParamInfo<ExtensionFlightPin>& info)
{
    std::string name(info.param.scenario);
    for (char& c : name)
    {
        if (c == '-')
        {
            c = '_';
        }
    }
    return name;
}

class ExtensionFlightStable : public ::testing::TestWithParam<ExtensionFlightPin>
{ };

// The flight at a time step of 0.01 s, every number by its rule.
TEST_P(ExtensionFlightStable, TheFlightIsOpenRockets)
{
    const DefaultUnitsGuard        units;
    const ExtensionFlightPin&      pin      = GetParam();
    ExtensionFlightScenario        scenario = makeExtensionFlightScenario(pin.scenario);
    const std::vector<std::string> lines    = runExtensionFlightScenario(scenario, 0.01);

    const std::vector<std::string> differences =
        differencesFrom(pin.scenario, pin.lines, lines, Mode::VALUES);
    EXPECT_TRUE(differences.empty()) << joined(differences);
}

INSTANTIATE_TEST_SUITE_P(Scenarios, ExtensionFlightStable,
                         ::testing::ValuesIn(kExtensionFlightStablePins), pinTestName);

class ExtensionFlightDefault : public ::testing::TestWithParam<ExtensionFlightPin>
{ };

// The flight at the default time step of 0.05 s: its structure.
TEST_P(ExtensionFlightDefault, TheFlightHasOpenRocketsStructure)
{
    const DefaultUnitsGuard        units;
    const ExtensionFlightPin&      pin      = GetParam();
    ExtensionFlightScenario        scenario = makeExtensionFlightScenario(pin.scenario);
    const std::vector<std::string> lines    = runExtensionFlightScenario(scenario, 0.05);

    const std::vector<std::string> differences =
        differencesFrom(pin.scenario, pin.lines, lines, Mode::STRUCTURE);
    EXPECT_TRUE(differences.empty()) << joined(differences);
}

INSTANTIATE_TEST_SUITE_P(Scenarios, ExtensionFlightDefault,
                         ::testing::ValuesIn(kExtensionFlightDefaultPins), pinTestName);

/// The scenarios of @p pins whose name or first line is not that of the probe's scenario at
/// the same place with the time step @p timeStep ("0.01"), by name; "<count>" when the number
/// of pins is not the number of scenarios.
[[nodiscard]] std::vector<std::string> misplacedPins(std::span<const ExtensionFlightPin> pins,
                                                     std::string_view                    timeStep)
{
    const std::vector<std::string>& ids = extensionFlightScenarioIds();
    std::vector<std::string>        misplaced;
    if (pins.size() != ids.size())
    {
        misplaced.emplace_back("<count>");
        return misplaced;
    }
    for (std::size_t i = 0; i < ids.size(); i++)
    {
        const ExtensionFlightPin& pin = pins[i];
        if (pin.scenario != ids[i] || pin.lines.empty() ||
            pin.lines.front() != std::format("SCENARIO {} dt={}", ids[i], timeStep))
        {
            misplaced.push_back(ids[i]);
        }
    }
    return misplaced;
}

TEST(ExtensionFlightPins, HoldEveryScenarioOfTheProbeAtBothTimeSteps)
{
    EXPECT_EQ(extensionFlightScenarioIds().size(), 14U);
    EXPECT_EQ(misplacedPins(kExtensionFlightStablePins, "0.01"), std::vector<std::string>{});
    EXPECT_EQ(misplacedPins(kExtensionFlightDefaultPins, "0.05"), std::vector<std::string>{});
}

// ----------------------------------------------------------------- what the pins have to show

/// The line of @p pin that starts (after its indentation) with @p start, the @p occurrence-th
/// one; "" when there is none.
[[nodiscard]] std::string_view pinnedLine(const ExtensionFlightPin& pin, std::string_view start,
                                          int occurrence = 0)
{
    for (const std::string_view line : pin.lines)
    {
        const std::size_t text = line.find_first_not_of(' ');
        if (text != std::string_view::npos && line.substr(text).starts_with(start) &&
            occurrence-- == 0)
        {
            return line;
        }
    }
    return "";
}

/// The pin of the scenario @p scenario at 0.01 s.
[[nodiscard]] const ExtensionFlightPin& stablePin(std::string_view scenario)
{
    for (const ExtensionFlightPin& pin : kExtensionFlightStablePins)
    {
        if (pin.scenario == scenario)
        {
            return pin;
        }
    }
    ADD_FAILURE() << "no pin for " << scenario;
    return kExtensionFlightStablePins.front();
}

/// The number of key @p key in the pinned line @p line; NaN when it has none.
[[nodiscard]] double pinned(std::string_view line, std::string_view key)
{
    return numberOf(probeTokens(line), key).value_or(std::nan(""));
}

// The comparison above would also pass on pins that showed nothing. These are the facts of
// OpenRocket's runs that the header of this file states, read from the pins themselves.
TEST(ExtensionFlightPins, ShowWhatRollControlDoesInOpenRocket)
{
    constexpr double kLimit = 0.2617993877991494;  // 15 degrees, the default largest fin angle

    // The canted fins of the Alpha III are turned back from the first controlled step: the
    // column never holds the cant of the design (0.02), and the fins get it back at the end.
    const ExtensionFlightPin& alpha = stablePin("alpha-roll");
    EXPECT_EQ(pinnedLine(alpha, "caller"), "  caller cantBefore=0.02 cantAfter=0.02");
    EXPECT_EQ(pinned(pinnedLine(alpha, "spy"), "cantStart"), 0.02);
    EXPECT_EQ(pinned(pinnedLine(alpha, "spy"), "cantEnd"), 0.02);
    EXPECT_LT(pinned(pinnedLine(alpha, "column finCant"), "max"), 0.001);
    // The nested optimum-coast simulation runs without the listener.
    EXPECT_EQ(pinned(pinnedLine(alpha, "branch"), "optimumAltitude"),
              pinned(pinnedLine(stablePin("alpha-plain"), "branch"), "optimumAltitude"));

    // A RollControl on the sustainer's fins: both branches have the column, and the clone of
    // the listener in the booster's branch leaves the fins at the limit.
    const ExtensionFlightPin& sustainer = stablePin("beta-roll-sustainer");
    EXPECT_EQ(pinned(pinnedLine(sustainer, "branch", 0), "types"), 72);
    EXPECT_EQ(pinned(pinnedLine(sustainer, "branch", 1), "types"), 72);
    EXPECT_GT(pinned(pinnedLine(sustainer, "column finCant", 0), "min"), -0.05);
    EXPECT_EQ(pinned(pinnedLine(sustainer, "column finCant", 1), "last"), -kLimit);
    EXPECT_EQ(pinned(pinnedLine(sustainer, "spy"), "cantEnd"), 0.0);

    // A RollControl on the booster's fins: the sustainer's listener drives the fins of the
    // dropped stage to the limit. (Where the booster's own listener has them when its branch
    // ends is not reproducible, and is not compared.)
    const ExtensionFlightPin& booster = stablePin("beta-roll-booster");
    EXPECT_EQ(pinned(pinnedLine(booster, "branch", 1), "types"), 72);
    EXPECT_EQ(pinned(pinnedLine(booster, "column finCant", 0), "last"), -kLimit);
    EXPECT_EQ(pinned(pinnedLine(booster, "spy"), "cantMaxAbs"), kLimit);
    EXPECT_EQ(pinned(pinnedLine(booster, "spy"), "cantEnd"), 0.0);

    // The limit of the RollControl with other settings is reached.
    EXPECT_EQ(pinned(pinnedLine(stablePin("haisu-roll-custom"), "column finCant"), "min"), -0.05);
    EXPECT_EQ(pinned(pinnedLine(stablePin("haisu-roll-custom"), "column finCant"), "max"), 0.05);

    // A missing fin set ends the simulation before a branch exists.
    const ExtensionFlightPin& missing = stablePin("haisu-roll-missing");
    EXPECT_EQ(pinnedLine(missing, "result"),
              "  result=error:A fin set with name 'NOPE' was not found");
    EXPECT_EQ(pinnedLine(missing, "branch"), "");
}

// ------------------------------------------------------------------- the comparison itself

/// The lines of a small flight of the Iso-Haisu, as OpenRocket's.
constexpr std::array<std::string_view, 4> kSmallFlight{
    "SCENARIO haisu-roll dt=0.01",
    "  summary maxAltitude=100.0 flightTime=40.0",
    "  branch 0 rows=1000 types=72 name=Stage1",
    "    event APOGEE t=10.0",
};

/// How many differences the comparison reports when line @p line of kSmallFlight is @p text in
/// the run.
[[nodiscard]] std::size_t reported(std::size_t line, std::string_view text,
                                   Mode mode = Mode::VALUES)
{
    std::vector<std::string> lines(kSmallFlight.begin(), kSmallFlight.end());
    lines.at(line) = std::string(text);
    return differencesFrom("haisu-roll", kSmallFlight, lines, mode).size();
}

// Not vacuous: a number beyond its tolerance, another text, another event and a missing line
// are each reported, and a number within its tolerance is not.
TEST(ExtensionFlightComparison, ReportsWhatDiffers)
{
    EXPECT_EQ(reported(1, "  summary maxAltitude=100.0 flightTime=40.0"), 0U);
    // Within the tolerances: 1e-6 of the altitude, 0.05 s of the flight time, 2 % of the rows,
    // 1e-4 s of an event of a reproducible flight.
    EXPECT_EQ(reported(1, "  summary maxAltitude=100.00009 flightTime=40.04"), 0U);
    EXPECT_EQ(reported(2, "  branch 0 rows=1020 types=72 name=Stage1"), 0U);
    EXPECT_EQ(reported(3, "    event APOGEE t=10.00009"), 0U);
    // Beyond them.
    EXPECT_EQ(reported(1, "  summary maxAltitude=100.0002 flightTime=40.0"), 1U);
    EXPECT_EQ(reported(1, "  summary maxAltitude=100.0 flightTime=40.06"), 1U);
    EXPECT_EQ(reported(1, "  summary maxAltitude=NaN flightTime=40.0"), 1U);
    EXPECT_EQ(reported(2, "  branch 0 rows=1023 types=72 name=Stage1"), 1U);
    EXPECT_EQ(reported(2, "  branch 0 rows=1000 types=71 name=Stage1"), 1U);
    EXPECT_EQ(reported(2, "  branch 0 rows=1000 types=72 name=Stage2"), 1U);
    EXPECT_EQ(reported(3, "    event TUMBLE t=10.0"), 1U);
    EXPECT_EQ(reported(3, "    event APOGEE t=10.0002"), 1U);
    EXPECT_EQ(reported(3, "    event APOGEE"), 1U);

    // The structure alone: no value counts, every text and the number of types do.
    EXPECT_EQ(reported(1, "  summary maxAltitude=1.0 flightTime=2.0", Mode::STRUCTURE), 0U);
    EXPECT_EQ(reported(2, "  branch 0 rows=5 types=72 name=Stage1", Mode::STRUCTURE), 0U);
    EXPECT_EQ(reported(2, "  branch 0 rows=1000 types=71 name=Stage1", Mode::STRUCTURE), 1U);
    EXPECT_EQ(reported(3, "    event TUMBLE t=10.0", Mode::STRUCTURE), 1U);

    // A line too few.
    const std::vector<std::string> shorter{std::string(kSmallFlight.front())};
    EXPECT_EQ(differencesFrom("haisu-roll", kSmallFlight, shorter, Mode::VALUES).size(), 1U);
}

/// How many differences the comparison reports for the fin cant column of the last branch of
/// the scenario @p scenario, a flight of @p branches branches, when OpenRocket's line of the
/// column is @p pinnedColumn and the run's @p column.
[[nodiscard]] std::size_t reportedForColumn(std::string_view scenario, int branches,
                                            std::string_view pinnedColumn, std::string_view column)
{
    std::vector<std::string> pinnedLines{std::format("SCENARIO {} dt=0.01", scenario)};
    for (int b = 0; b < branches; b++)
    {
        pinnedLines.push_back(std::format("  branch {} rows=1000 types=72 name=Stage", b));
    }
    std::vector<std::string> lines = pinnedLines;
    pinnedLines.emplace_back(pinnedColumn);
    lines.emplace_back(column);
    const std::vector<std::string_view> pinnedViews(pinnedLines.begin(), pinnedLines.end());
    return differencesFrom(scenario, pinnedViews, lines, Mode::VALUES).size();
}

// What is not reproducible is not compared: the fin cant in the branch of a dropped booster
// whose own controller turns its fins, and the counts of any dropped stage's branch. The texts
// of such a line are compared all the same. And a value of a column is measured against the
// scale of the column.
TEST(ExtensionFlightComparison, KnowsTheScaleAndWhatIsNotReproducible)
{
    const std::string_view booster =
        "    column finCant min=-0.03 max=0.26 last=0.26 nan=0 first=0 symbol=x";
    // The booster's own branch of the flight with the controller on the booster's fins.
    EXPECT_EQ(
        reportedForColumn("beta-roll-booster", 2, booster,
                          "    column finCant min=-0.26 max=0.1 last=-0.22 nan=7 first=3 symbol=x"),
        0U);
    EXPECT_EQ(
        reportedForColumn("beta-roll-booster", 2, booster,
                          "    column finCant min=-0.03 max=0.26 last=0.26 nan=0 first=0 symbol=y"),
        1U);
    // In the branch of the sustainer of that flight, where the fins of the dropped booster
    // are driven to the limit, the values count.
    EXPECT_EQ(reportedForColumn(
                  "beta-roll-booster", 1, booster,
                  "    column finCant min=-0.03 max=0.26 last=-0.26 nan=0 first=0 symbol=x"),
              1U);
    // So they do in the booster's branch of the flight with the controller on the sustainer's
    // fins, which that branch leaves at the limit; its counts do not.
    EXPECT_EQ(
        reportedForColumn("beta-roll-sustainer", 2, booster,
                          "    column finCant min=-0.03 max=0.26 last=0.26 nan=5 first=2 symbol=x"),
        0U);
    EXPECT_EQ(
        reportedForColumn("beta-roll-sustainer", 2, booster,
                          "    column finCant min=-0.03 max=0.26 last=0.25 nan=0 first=0 symbol=x"),
        1U);

    // In the Iso-Haisu's column a last value near zero is measured against the scale: 5e-3 of
    // 0.2.
    const std::string_view haisu =
        "    column finCant min=-0.2 max=0.0 last=0.0 nan=0 first=0 symbol=x";
    EXPECT_EQ(
        reportedForColumn("haisu-roll", 1, haisu,
                          "    column finCant min=-0.2 max=0.0 last=0.0009 nan=0 first=0 symbol=x"),
        0U);
    EXPECT_EQ(
        reportedForColumn("haisu-roll", 1, haisu,
                          "    column finCant min=-0.2 max=0.0 last=0.0011 nan=0 first=0 symbol=x"),
        1U);
}

// ------------------------------------------------------------------------ direct questions

/// The fin set named @p name of @p rocket.
[[nodiscard]] const QtRocket::FinSet* finSetOf(const QtRocket::Rocket& rocket,
                                               std::string_view        name)
{
    for (const QtRocket::RocketComponent* c : rocket.getSelectedConfiguration().getAllComponents())
    {
        const auto* finSet = dynamic_cast<const QtRocket::FinSet*>(c);
        if (finSet != nullptr && c->getName() == name)
        {
            return finSet;
        }
    }
    return nullptr;
}

/// A simulation of the scenario @p scenario with the probe's options and the time step
/// @p timeStep, on the scenario's rocket and @p preferences.
[[nodiscard]] std::unique_ptr<Simulation> simulationOf(ExtensionFlightScenario&       scenario,
                                                       QtRocket::InMemoryPreferences& preferences,
                                                       double                         timeStep)
{
    std::unique_ptr<Simulation> sim = std::make_unique<Simulation>(*scenario.rocket, preferences);
    sim->setFlightConfigurationId(scenario.fcid);
    QtRocket::SimulationOptions& o = sim->getOptions();
    o.setLaunchRodLength(scenario.rodLength);
    o.setLaunchIntoWind(false);
    o.setLaunchRodAngle(0);
    o.setLaunchRodDirection(0);
    o.setIsaAtmosphere(true);
    o.setTimeStep(timeStep);
    o.setRandomSeed(0);
    o.setRandomSeedFixed(true);
    for (const std::shared_ptr<QtRocket::SimulationExtension>& extension : scenario.extensions)
    {
        sim->getSimulationExtensions().push_back(extension);
    }
    return sim;
}

// The caller's rocket is the design: a run with RollControl changes neither the cant of its
// fins nor anything else of it (its modification id stays), and the fin set of the rocket the
// engine simulated has the design's cant again.
TEST(ExtensionFlight, RollControlLeavesTheCallersRocketAlone)
{
    ExtensionFlightScenario             scenario = makeExtensionFlightScenario("alpha-roll");
    QtRocket::Test::JavaTestPreferences preferences;
    const std::unique_ptr<Simulation>   sim  = simulationOf(scenario, preferences.store, 0.05);
    const QtRocket::FinSet*             fins = finSetOf(*scenario.rocket, "3 Fin Set");
    ASSERT_NE(fins, nullptr);
    ASSERT_EQ(fins->getCantAngle(), 0.02);
    const QtRocket::ModId before = scenario.rocket->getModId();

    const Result<void> result = sim->simulate();
    ASSERT_TRUE(result.has_value()) << result.error().message;

    EXPECT_EQ(fins->getCantAngle(), 0.02);
    EXPECT_EQ(scenario.rocket->getModId(), before);
    const std::shared_ptr<FlightData>& data = sim->getSimulatedData();
    ASSERT_NE(data, nullptr);
    ASSERT_NE(data->getSimulatedRocket(), nullptr);
    const QtRocket::FinSet* simulated = finSetOf(*data->getSimulatedRocket(), "3 Fin Set");
    ASSERT_NE(simulated, nullptr);
    EXPECT_NE(simulated, fins);
    EXPECT_EQ(simulated->getCantAngle(), 0.02);
    // The column is there, and it never held the design's cant.
    const FlightDataBranch& branch = data->getBranch(0);
    ASSERT_TRUE(branch.hasType(RollControl::finCantType()));
    EXPECT_LT(branch.getMaximum(RollControl::finCantType()), 0.02);
}

// A fin set that the rocket does not have ends simulate() with the listener's exception as an
// ordinary error, and the simulation is left as after any failed run.
TEST(ExtensionFlight, AMissingFinSetIsAnErrorOfSimulate)
{
    ExtensionFlightScenario scenario = makeExtensionFlightScenario("haisu-roll-missing");
    QtRocket::Test::JavaTestPreferences preferences;
    const std::unique_ptr<Simulation>   sim = simulationOf(scenario, preferences.store, 0.05);

    const Result<void> result = sim->simulate();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::SIMULATION_ABORTED);
    EXPECT_EQ(result.error().message, "A fin set with name 'NOPE' was not found");
    EXPECT_EQ(sim->getStoredStatus(), Simulation::Status::UPTODATE);
    const std::shared_ptr<FlightData>& data = sim->getSimulatedData();
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->getBranchCount(), 0U);
}

// AirStart changes the status when the simulation starts, so the flight has the warning for
// that, as an event before the launch; the rocket is in the air at once and lifts off after
// the first step.
TEST(ExtensionFlight, AirStartStartsTheFlightInTheAir)
{
    const DefaultUnitsGuard             units;
    ExtensionFlightScenario             scenario = makeExtensionFlightScenario("haisu-plain");
    QtRocket::Test::JavaTestPreferences preferences;
    scenario.extensions.push_back(std::make_shared<AirStart>());
    const std::unique_ptr<Simulation> sim = simulationOf(scenario, preferences.store, 0.05);

    const Result<void> result = sim->simulate();
    ASSERT_TRUE(result.has_value()) << result.error().message;

    const FlightData& data = QtRocket::Test::simulatedData(*sim);
    EXPECT_TRUE(data.getWarningSet().contains(Warning::kListenersAffected));
    const std::vector<std::string> events = QtRocket::Test::eventNames(data, 0);
    ASSERT_GE(events.size(), 5U);
    EXPECT_EQ(events[0], "SIM_WARN");
    EXPECT_EQ(events[1], "SIM_WARN");
    EXPECT_EQ(events[2], "LAUNCH");
    EXPECT_EQ(events[3], "IGNITION");
    EXPECT_EQ(events[4], "LIFTOFF");
    const FlightDataBranch&    branch = data.getBranch(0);
    const std::vector<double>* altitude =
        branch.getView(FlightDataType::builtin(QtRocket::FlightDataTypeId::TYPE_ALTITUDE));
    const std::vector<double>* velocity =
        branch.getView(FlightDataType::builtin(QtRocket::FlightDataTypeId::TYPE_VELOCITY_TOTAL));
    ASSERT_NE(altitude, nullptr);
    ASSERT_NE(velocity, nullptr);
    ASSERT_FALSE(altitude->empty());
    // The first row: 100 m up at 50 m/s, straight up the vertical rod (the velocity is the
    // launch velocity turned by the orientation of the rocket, which rounds).
    EXPECT_EQ(altitude->front(), 100.0);
    EXPECT_NEAR(velocity->front(), 50.0, 1e-12);
    const FlightEvent* liftoff = branch.getFirstEvent(FlightEvent::Type::LIFTOFF);
    ASSERT_NE(liftoff, nullptr);
    EXPECT_LT(liftoff->getTime(), 0.01);
}

/// The number of key @p key in the first line of @p lines that starts (after its indentation)
/// with @p start; NaN when there is no such line or number.
[[nodiscard]] double numberIn(const std::vector<std::string>& lines, std::string_view start,
                              std::string_view key)
{
    for (const std::string& line : lines)
    {
        const std::vector<std::string_view> tokens = probeTokens(line);
        if (!tokens.empty() && tokens.front().starts_with(start))
        {
            return numberOf(tokens, key).value_or(std::nan(""));
        }
    }
    return std::nan("");
}

// The parachute of the Alpha III opens before the apogee, so the engine finds the optimum
// coast with a nested simulation: of a new copy of the caller's rocket, from the launch, with
// the system listeners only. The listener of a RollControl is cloned for it with the
// conditions and then taken out (OpenRocket's computeCoastTime()), so neither the controller
// nor the cant it has given the fins of the rocket that is flying reaches the nested run: the
// optimum is that of the flight without the extension, to the last bit, while the flight
// itself is another one. (The probe's lines print a number so that it reads back as it was.)
TEST(ExtensionFlight, TheNestedOptimumCoastRunFliesWithoutTheRollControl)
{
    const DefaultUnitsGuard        units;
    ExtensionFlightScenario        plainScenario = makeExtensionFlightScenario("alpha-plain");
    ExtensionFlightScenario        rollScenario  = makeExtensionFlightScenario("alpha-roll");
    const std::vector<std::string> plain         = runExtensionFlightScenario(plainScenario, 0.05);
    const std::vector<std::string> roll          = runExtensionFlightScenario(rollScenario, 0.05);

    const double optimum = numberIn(plain, "branch", "optimumAltitude");
    ASSERT_FALSE(std::isnan(optimum));
    // The nested run coasts on where the flight opens its parachute: it gets higher.
    EXPECT_GT(optimum, numberIn(plain, "summary", "maxAltitude"));
    EXPECT_EQ(numberIn(roll, "branch", "optimumAltitude"), optimum);
    EXPECT_EQ(numberIn(roll, "branch", "timeToOptimumAltitude"),
              numberIn(plain, "branch", "timeToOptimumAltitude"));

    // The flight with the controller is not the flight without it, and only it has the
    // column; its fins are back at the cant of the design when the simulation ends.
    EXPECT_NE(numberIn(roll, "summary", "maxAltitude"), numberIn(plain, "summary", "maxAltitude"));
    EXPECT_EQ(numberIn(plain, "branch", "types") + 1, numberIn(roll, "branch", "types"));
    EXPECT_LT(numberIn(roll, "spy", "cantLast"), 0.02);
    EXPECT_EQ(numberIn(roll, "spy", "cantEnd"), 0.02);
}

/// What a run on a thread of its own gave: the error, or the three numbers compared.
struct ThreadedRun
{
    std::string error{"not run"};
    double      maxAltitude{0};
    double      finCantMinimum{0};
    std::size_t rows{0};

    [[nodiscard]] bool operator==(const ThreadedRun&) const = default;
};

/// For a failed expectation.
std::ostream& operator<<(std::ostream& out, const ThreadedRun& run)
{
    return out << std::format("error '{}', maximum altitude {}, fin cant minimum {}, {} rows",
                              run.error, run.maxAltitude, run.finCantMinimum, run.rows);
}

/// Runs @p simulation and notes in @p run what it gave.
void runOnThread(Simulation& simulation, ThreadedRun& run)
{
    const Result<void> result = simulation.simulate();
    if (!result.has_value())
    {
        run.error = result.error().message;
        return;
    }
    const std::shared_ptr<FlightData>& data = simulation.getSimulatedData();
    if (data == nullptr || data->getBranchCount() == 0)
    {
        run.error = "no flight data";
        return;
    }
    const FlightDataBranch& branch = data->getBranch(0);
    run.error                      = "";
    run.maxAltitude                = data->getMaxAltitude();
    run.finCantMinimum             = branch.getMinimum(RollControl::finCantType());
    run.rows                       = branch.getLength();
}

/// Runs @p first and @p second at the same time, each on a thread of its own, and waits for
/// both.
void runOnTwoThreads(Simulation& first, ThreadedRun& firstRun, Simulation& second,
                     ThreadedRun& secondRun)
{
    const std::jthread a([&first, &firstRun] { runOnThread(first, firstRun); });
    const std::jthread b([&second, &secondRun] { runOnThread(second, secondRun); });
}

// Two simulations with RollControl at the same time, each on its own duplicate of the
// simulation (a copy of the rocket and clones of the extensions): they share the flight data
// type of the fin cant and the preferences, and nothing else. Each gives what the same
// duplicate gives alone. (The sanitizer presets run this test too.)
TEST(ExtensionFlight, TwoSimulationsWithRollControlRunOnTwoThreads)
{
    ExtensionFlightScenario             scenario = makeExtensionFlightScenario("haisu-roll");
    QtRocket::Test::JavaTestPreferences preferences;
    const std::unique_ptr<Simulation>   sim = simulationOf(scenario, preferences.store, 0.05);

    const std::unique_ptr<Simulation> alone  = sim->duplicateForIndependentSimulation();
    const std::unique_ptr<Simulation> first  = sim->duplicateForIndependentSimulation();
    const std::unique_ptr<Simulation> second = sim->duplicateForIndependentSimulation();
    // The duplicates have extensions of their own.
    ASSERT_EQ(first->getSimulationExtensions().size(), 1U);
    ASSERT_NE(first->getSimulationExtensions()[0], sim->getSimulationExtensions()[0]);
    ASSERT_NE(first->getSimulationExtensions()[0], second->getSimulationExtensions()[0]);

    ThreadedRun expected;
    runOnThread(*alone, expected);
    ASSERT_EQ(expected.error, "");

    ThreadedRun firstRun;
    ThreadedRun secondRun;
    runOnTwoThreads(*first, firstRun, *second, secondRun);

    EXPECT_EQ(firstRun, expected);
    EXPECT_EQ(secondRun, expected);
    EXPECT_LT(expected.finCantMinimum, -0.1);
    // The simulation the duplicates were made from did not run.
    EXPECT_EQ(sim->getSimulatedData(), nullptr);
}

// ============================================================================== measurement

/// The listener of a perturbed run: after every step it multiplies every component of the
/// velocity and of the rotation velocity of the rocket by a factor next to 1. A system
/// listener, so that it stays in the nested optimum-coast simulation and adds no "listeners
/// affected the simulation" warning.
class KickListener final : public QtRocket::CloneableSimulationListener<KickListener>
{
public:
    explicit KickListener(double factor) noexcept : m_factor(factor) { }

    [[nodiscard]] bool isSystemListener() const override { return true; }

    void postStep(SimulationStatus& status) override
    {
        const Coordinate& velocity = status.getRocketVelocity();
        status.setRocketVelocity(Coordinate{velocity.x * m_factor, velocity.y * m_factor,
                                            velocity.z * m_factor, velocity.weight});
        const Coordinate& rotation = status.getRocketRotationVelocity();
        status.setRocketRotationVelocity(Coordinate{rotation.x * m_factor, rotation.y * m_factor,
                                                    rotation.z * m_factor, rotation.weight});
    }

private:
    double m_factor;
};

/// Prints the probe's lines of every scenario, run with the time step @p timeStep and, when
/// @p factor is not 1, with the kick of that factor; then what the comparison with the pins
/// @p pins reports for them.
void printRuns(double timeStep, double factor, std::span<const ExtensionFlightPin> pins)
{
    const DefaultUnitsGuard  units;
    std::vector<std::string> reports;
    for (const ExtensionFlightPin& pin : pins)
    {
        ExtensionFlightScenario scenario = makeExtensionFlightScenario(pin.scenario);
        const std::shared_ptr<SimulationListener> kick =
            factor == 1.0 ? nullptr : std::make_shared<KickListener>(factor);
        const std::vector<std::string> lines = runExtensionFlightScenario(scenario, timeStep, kick);
        for (const std::string& line : lines)
        {
            std::cout << line << '\n';
        }
        const Mode mode = timeStep == 0.01 ? Mode::VALUES : Mode::STRUCTURE;
        for (const std::string& difference : differencesFrom(pin.scenario, pin.lines, lines, mode))
        {
            reports.push_back(std::format("{}: {}", pin.scenario, difference));
        }
    }
    std::cout << "COMPARISON " << reports.size() << " differences from the pins\n";
    for (const std::string& report : reports)
    {
        std::cout << "  " << report << '\n';
    }
}

// Not tests: they print the runs in the probe's format, for
// probes/tier9a-extensions/compare_runs.py and per_scenario.py, which made the tables the
// tolerances come from. Run with --gtest_also_run_disabled_tests.
TEST(ExtensionFlightMeasurement, DISABLED_PrintsTheStableRuns)
{
    printRuns(0.01, 1.0, kExtensionFlightStablePins);
}

TEST(ExtensionFlightMeasurement, DISABLED_PrintsTheStableRunsKickedUp)
{
    printRuns(0.01, 1.0 + 1e-10, kExtensionFlightStablePins);
}

TEST(ExtensionFlightMeasurement, DISABLED_PrintsTheStableRunsKickedDown)
{
    printRuns(0.01, 1.0 - 1e-10, kExtensionFlightStablePins);
}

TEST(ExtensionFlightMeasurement, DISABLED_PrintsTheDefaultRuns)
{
    printRuns(0.05, 1.0, kExtensionFlightDefaultPins);
}

TEST(ExtensionFlightMeasurement, DISABLED_PrintsTheDefaultRunsKickedUp)
{
    printRuns(0.05, 1.0 + 1e-10, kExtensionFlightDefaultPins);
}

TEST(ExtensionFlightMeasurement, DISABLED_PrintsTheDefaultRunsKickedDown)
{
    printRuns(0.05, 1.0 - 1e-10, kExtensionFlightDefaultPins);
}

/// Runs every scenario with the kick of the factor 1 + @p kick (0: without a kick) and
/// compares each with its pins as the tests do: at the time step of 0.01 s (@p stable) every
/// number by its rule, at 0.05 s the structure. Notes in @p use how much of its tolerance
/// every number used (by scenario and place), and returns what differs.
[[nodiscard]] std::vector<std::string> measureKick(double kick, bool stable, ToleranceUse& use)
{
    const std::span<const ExtensionFlightPin> pins =
        stable ? std::span<const ExtensionFlightPin>(kExtensionFlightStablePins)
               : std::span<const ExtensionFlightPin>(kExtensionFlightDefaultPins);
    std::vector<std::string> reports;
    for (const ExtensionFlightPin& pin : pins)
    {
        ExtensionFlightScenario scenario = makeExtensionFlightScenario(pin.scenario);
        const std::shared_ptr<SimulationListener> listener =
            kick == 0.0 ? nullptr : std::make_shared<KickListener>(1.0 + kick);
        const std::vector<std::string> lines =
            runExtensionFlightScenario(scenario, stable ? 0.01 : 0.05, listener);
        ToleranceUse ofRun;
        for (const std::string& difference : differencesFrom(
                 pin.scenario, pin.lines, lines, stable ? Mode::VALUES : Mode::STRUCTURE, &ofRun))
        {
            reports.push_back(std::format("{}: {}", pin.scenario, difference));
        }
        for (const auto& [where, used] : ofRun)
        {
            double& worst = use[std::format("{} {}", pin.scenario, where)];
            worst         = std::fmax(worst, used);
        }
    }
    return reports;
}

/// Runs measureKick() for each of @p kicks, prints what differs from the pins in each run, and
/// returns the largest use of every tolerance in them.
[[nodiscard]] ToleranceUse measureKicks(std::span<const double> kicks, bool stable)
{
    ToleranceUse use;
    for (const double kick : kicks)
    {
        const std::vector<std::string> reports = measureKick(kick, stable, use);
        std::cout << std::format("KICK {:+.0e} AT {} s: {} differences from the pins\n", kick,
                                 stable ? "0.01" : "0.05", reports.size());
        for (const std::string& report : reports)
        {
            std::cout << "  " << report << '\n';
        }
    }
    return use;
}

/// Prints the numbers of @p use that used at least @p least of their tolerance, the largest
/// use first.
void printToleranceUse(std::string_view title, const ToleranceUse& use, double least)
{
    std::vector<std::pair<double, std::string>> sorted;
    for (const auto& [where, used] : use)
    {
        if (used >= least)
        {
            sorted.emplace_back(used, where);
        }
    }
    std::ranges::sort(sorted, std::greater<>{});
    std::cout << std::format("{}: {} of {} numbers use at least {} of their tolerance\n", title,
                             sorted.size(), use.size(), least);
    for (const auto& [used, where] : sorted)
    {
        std::cout << std::format("  {:10.3g}  {}\n", used, where);
    }
}

/// The kicks the rules and the tolerances of the stable flights were worked out with: none, and
/// twenty from a difference in the last bits of every step up to 3e-11. What moves as much
/// under the smallest of them as under the largest is not reproducible, whatever its tolerance.
constexpr std::array<double, 21> kSmallKicks{
    0.0,   1e-15,  -1e-15, 3e-15,  -3e-15, 1e-14,  -1e-14, 3e-14,  -3e-14, 1e-13, -1e-13,
    3e-13, -3e-13, 1e-12,  -1e-12, 3e-12,  -3e-12, 1e-11,  -1e-11, 3e-11,  -3e-11};

/// Thirty other kicks of the same sizes, which the rules were not worked out with: what they
/// were checked with afterwards.
constexpr std::array<double, 30> kOtherKicks{
    2e-15, -2e-15, 5e-15, -5e-15, 7e-15, -7e-15, 2e-14, -2e-14, 5e-14, -5e-14,
    7e-14, -7e-14, 2e-13, -2e-13, 5e-13, -5e-13, 7e-13, -7e-13, 2e-12, -2e-12,
    5e-12, -5e-12, 7e-12, -7e-12, 2e-11, -2e-11, 5e-11, -5e-11, 7e-11, -7e-11};

/// The kick under which what is reproducible moves in proportion (K in the header).
constexpr std::array<double, 2> kLargeKicks{1e-10, -1e-10};

// Not tests: how much of its tolerances the comparison of the stable flights uses (the part of
// the tolerance that the difference from OpenRocket's number is, toleranceUsed()), under the
// kicks the rules were worked out with and under the kick K ...
TEST(ExtensionFlightMeasurement, DISABLED_PrintsTheToleranceInUse)
{
    const DefaultUnitsGuard units;
    const ToleranceUse      small = measureKicks(kSmallKicks, true);
    const ToleranceUse      large = measureKicks(kLargeKicks, true);
    printToleranceUse("NO KICK AND KICKS OF 1e-15 TO 3e-11", small, 0.001);
    printToleranceUse("KICKS OF 1e-10", large, 0.001);
}

// ... under thirty kicks they were not worked out with ...
TEST(ExtensionFlightMeasurement, DISABLED_PrintsTheToleranceInUseUnderOtherKicks)
{
    const DefaultUnitsGuard units;
    const ToleranceUse      other = measureKicks(kOtherKicks, true);
    printToleranceUse("THIRTY OTHER KICKS OF 2e-15 TO 7e-11", other, 0.001);
}

// ... and what the comparison of the structure at the default time step reports under all of
// them.
TEST(ExtensionFlightMeasurement, DISABLED_PrintsTheStructureUnderKicks)
{
    const DefaultUnitsGuard units;
    static_cast<void>(measureKicks(kSmallKicks, false));
    static_cast<void>(measureKicks(kOtherKicks, false));
    static_cast<void>(measureKicks(kLargeKicks, false));
}

// Not a test: compares the lines of the file "extension-flight-lines.txt" of the working
// directory (an output of the probe at 0.01 s, of another run of OpenRocket for example) with
// the pins, by the rules of the stable flights, and prints what differs.
TEST(ExtensionFlightMeasurement, DISABLED_ComparesTheLinesOfAFileWithThePins)
{
    std::ifstream file("extension-flight-lines.txt");
    ASSERT_TRUE(file.is_open()) << "no extension-flight-lines.txt in the working directory";
    std::vector<std::vector<std::string>> scenarios;
    for (std::string line; std::getline(file, line);)
    {
        if (line.starts_with("NAMES"))
        {
            break;
        }
        if (line.starts_with("SCENARIO "))
        {
            scenarios.emplace_back();
        }
        if (!scenarios.empty())
        {
            scenarios.back().push_back(line);
        }
    }
    ASSERT_EQ(scenarios.size(), kExtensionFlightStablePins.size());
    std::size_t  count = 0;
    ToleranceUse use;
    for (std::size_t i = 0; i < scenarios.size(); i++)
    {
        const ExtensionFlightPin& pin = kExtensionFlightStablePins.at(i);
        ToleranceUse              ofRun;
        for (const std::string& difference :
             differencesFrom(pin.scenario, pin.lines, scenarios[i], Mode::VALUES, &ofRun))
        {
            std::cout << pin.scenario << ": " << difference << '\n';
            count++;
        }
        for (const auto& [where, used] : ofRun)
        {
            use[std::format("{} {}", pin.scenario, where)] = used;
        }
    }
    std::cout << "COMPARISON " << count << " differences from the pins\n";
    printToleranceUse("THE RUN OF THE FILE", use, 0.001);
}

}  // namespace
