#pragma once

// What the two comparisons of the stable-step sets of the golden simulations share. Test-only.
//
// The stable-step set is every golden simulation once more at a time step of 0.01 s, at which a
// flight can be compared as a whole (tools/openrocket-goldens:
// SimulationDumper.useStableTimeStep()). simulation_stable_golden_tests.cpp compares that of the
// thirteen test rockets, whose golden files have the time series of their branches, row by row; its
// rules (H, N, E, T) and how they were measured are described at the top of that file.
// example_simulation_stable_golden_tests.cpp compares that of OpenRocket's sixteen example designs,
// whose golden files are the simulation documents alone (the summary values, the warnings and, per
// branch, the number of rows and the events); its rules are those of the first as far as a document
// can say them, each derived from its own measurement and described at the top of that file.
//
// Here is what both read and do alike: the time step and the tolerances of a time, of a summary
// value and of the velocity at the deployment; the table of the largest differences found
// (StableMeasurements); the comparison of one number or count that a rule may exclude; the ten
// summary values with their getters, tolerances and scales; what a warning prints of its
// parameter; rule E's reading of the rows of a run (the row on which an Euler stepper landed on
// the apogee, the step from it, and whether the last bit decided it); the events that no row
// handled on time; and what the sensitivity measurements need: the listener of a kicked run and
// a run in the form of its golden files.
//
// Everything of a simulation that is not a number of its trajectory is compared by
// compareSimulation() of GoldenSimulations.h, in both comparisons.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/util/Coordinate.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenMismatches.h"
#include "goldens/GoldenSimulations.h"

namespace QtRocket::Test
{

// ================================================================================ tolerances

// (A number that a comment gives as "measured" is the largest difference found for the test
// rockets, whose comparison these constants were made for; the measurement of the examples is at
// the top of example_simulation_stable_golden_tests.cpp, and its numbers are named as such.)

/// SimulationDumper.STABLE_TIME_STEP: the time step of the stable-step set, in s.
inline constexpr double kStableTimeStep = 0.01;

/// A tolerance of the stable-step comparison is at least this many times the largest difference
/// measured for what it bounds: OpenRocket against itself (the test rockets: nine dumps with
/// other component ids; the examples: 22 dumps with last-bit perturbations), and QtRocket against
/// the golden files on glibc and under 46 patterns of the libm shim.
inline constexpr double kToleranceMargin = 100.0;

/// A time of the stable-step set (that of an event, the time to apogee, the flight time, the
/// optimum delay): within this many seconds. The largest difference measured is 8.0e-7 s (a
/// ground hit, which an Euler stepper times from the altitude of the row before it); in the
/// examples 2.0e-7 s.
inline constexpr double kStableTimeAbsolute = 1e-4;

/// The time of an event that follows a late handling in its branch (the test rockets:
/// StablePlan::lateFrom, read from the golden rows; the examples: the list kLateHandlings):
/// within the 1 ms that BasicEventSimulationEngine.simulateLoop() allows a step to overshoot
/// an event by, which is also the plan's bound on event times. Measured: 2.7e-10 s; in the
/// examples 5.7e-7 s (a ground hit).
inline constexpr double kLateHandlingAbsolute = 1e-3;

/// A summary value of the stable-step set, the optimum altitude of a branch and the parameter
/// of a warning: within this fraction of its scale. Measured: 9.5e-9 (a maximum velocity); in
/// the examples 6.1e-9 (a maximum altitude).
inline constexpr double kStableSummaryRelative = 1e-6;

/// The velocity at the deployment of the recovery device: within this fraction of the largest
/// velocity of the flight. Measured: 1.4e-8 (the velocity is read off a steep part of the
/// trajectory, between two rows around the deployment); in the examples 1.8e-9.
inline constexpr double kDeploymentVelocityRelative = 1e-5;

/// A vertical velocity below this, in m/s, in a row of an Euler stepper: the row on which the
/// stepper landed its step to the apogee (AbstractEulerStepper.step(): t = |v / a|, which
/// leaves a rounding error of the velocity: nothing in nine runs of ten, some 1e-17 m/s of
/// either sign in the others).
inline constexpr double kApogeeVelocity = 1e-9;

/// Rule E: what the step to the apogee leaves of the vertical velocity is a rounding error of
/// the velocity it started from, at most this fraction of it. Measured: 1.8e-16 (the
/// largest in the apogee rows of nine dumps of OpenRocket and of 47 runs of QtRocket); in the
/// 47 runs of the examples 1.7e-16.
inline constexpr double kApogeeResidual = 1e-13;

/// A step from the apogee row that is no longer than this, in s, is the extra step of
/// MIN_TIME_STEP (1 ms) that the stepper takes when the velocity left by its step to the apogee
/// is positive; the regular step there is about 0.1 s.
inline constexpr double kShortStep = 1.5e-3;

/// A golden row within this many seconds of the time of an event handled the event on time.
inline constexpr double kOnTime = 1e-9;

// ================================================================== the events and the rows

/// The time of the first (@p last: the last) golden event of type @p type among @p events;
/// nullopt when there is none.
[[nodiscard]] std::optional<double> eventTime(const nlohmann::json& events, std::string_view type,
                                              bool last = false);

/// Rule E: the row of @p table on which an Euler stepper landed its step to the apogee: the
/// first row of an Euler stepper (it stores no angle of attack) rises, and a later row has no
/// vertical velocity. nullopt for a branch that reaches its apogee under a Runge-Kutta stepper,
/// or whose Euler stepper takes over on the way down.
[[nodiscard]] std::optional<std::size_t> eulerApogeeRow(const GoldenTable& table);

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
[[nodiscard]] std::string apogeeStepProblem(const ApogeeStep& step);

/// The time of the first of the golden events @p events that no row of @p times handled on
/// time (the step that reached it overshot it: the engine lets a step be no shorter than 1 ms
/// for an event, and the Runge-Kutta steppers no shorter than a twentieth of the time step);
/// infinite when every event has a row at its time.
[[nodiscard]] double firstLateHandling(const nlohmann::json&      events,
                                       const std::vector<double>& times);

/// The step @p branch takes from its row @p row.
[[nodiscard]] ApogeeStep apogeeStepOf(const FlightDataBranch& branch, std::size_t row);

// ============================================================================== measurements

/// The largest differences the comparison of the stable-step set found, by what was compared
/// (a column, a summary value, the times of the events of a type, ...) over every simulation;
/// see SimulationStableGoldenMeasurement and ExampleSimulationStableGoldenMeasurement.
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

// ================================================================================ comparison

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
void compareStableNumber(GoldenMismatches& m, StableComparison& c, const StableNumber& number,
                         bool compared);

/// Compares the count @p actual with the golden count @p expected, unless the rules exclude
/// it (@p compared false): it is counted then.
void compareStableCount(GoldenMismatches& m, StableComparison& c, std::string_view field,
                        std::int64_t expected, std::int64_t actual, bool compared);

/// How much of the first branch a summary value of the stable-step set needs to be compared.
/// (What follows is how the comparison of the test rockets reads the extents. That of the
/// examples compares a value that needs the descent, like one that needs the last row, only
/// when the first branch is compared to its end: in the examples two runs of OpenRocket on the
/// two time grids of rule E differ in a ground hit velocity by 9.2e-7 of it.)
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
inline constexpr std::array<StableSummaryValue, 10> kStableSummaryValues{{
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

/// Compares what the golden warning @p expected prints of its parameter with @p actual: the
/// parameter (a speed or an angle) within kStableSummaryRelative of itself, and the
/// description and the text, which print it with three digits. (What identifies the warning,
/// its class, priority and sources, is compared with the structure.) A golden warning without
/// a parameter has no number: nothing is compared or counted.
void compareStableWarning(GoldenMismatches& m, StableComparison& c, const std::string& field,
                          const nlohmann::json& expected, const Warning* actual);

/// compareStableWarning() of the warnings of @p data against the golden "warnings".
void compareStableWarnings(GoldenMismatches& m, StableComparison& c, const nlohmann::json& expected,
                           const FlightData& data);

/// The number of the numbers of the golden events @p events: their times, and the parameters
/// of their warnings.
[[nodiscard]] int eventNumbers(const nlohmann::json& events);

/// The time series of @p branch in the form of a golden time series: its CSV columns.
[[nodiscard]] GoldenTable timeSeriesOf(const FlightDataBranch& branch);

/// What the comparison of a run with its stable golden files compared and found.
struct StableResult
{
    SimulationCounts compared;
    SimulationCounts excluded;  ///< what the rules exclude (the test rockets: H, N, E and T)
    std::string      report;    ///< the mismatches, empty when everything matched
};

// ====================================================================== the sensitivity runs

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
[[nodiscard]] nlohmann::json numberJson(double value);

/// Branch @p index of @p run in the form of a golden branch, as far as the comparison of the
/// numbers reads it, with its time series in @p table.
[[nodiscard]] nlohmann::json goldenBranchOf(const SimulationRun& run, std::size_t index,
                                            GoldenTable& table);

/// @p run in the form of the golden files of a simulation, as far as the comparison of the
/// numbers (compareStableNumbers() of simulation_stable_golden_tests.cpp) reads them: what a
/// second run is compared with to see by how much a perturbation moves each number.
[[nodiscard]] GoldenFiles goldenFilesOf(const SimulationRun& run);

/// The size of the kick of the sensitivity measurement: a relative 1e-10 per step.
inline constexpr double kKickSize = 1e-10;

}  // namespace QtRocket::Test
