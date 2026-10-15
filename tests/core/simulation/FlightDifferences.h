#pragma once

// The comparison of two flights to the last bit (flightDifferences()): what the threading tests
// and the round trips of saved flight data hold flights to. Test-only.

#include <array>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "simulation/SimulationRunSupport.h"

namespace QtRocket::Test
{

/// The first row in which the columns @p expected and @p actual differ in a bit (a NaN equals a
/// NaN), as a text; "" when they are the same column.
[[nodiscard]] inline std::string columnDifference(const std::vector<double>& expected,
                                                  const std::vector<double>& actual)
{
    if (expected.size() != actual.size())
    {
        return std::format("{} rows, not {}", actual.size(), expected.size());
    }
    for (std::size_t row = 0; row < expected.size(); row++)
    {
        if (!junitEquals(expected[row], actual[row]))
        {
            return std::format("row {}: {}, not {}", row, actual[row], expected[row]);
        }
    }
    return {};
}

/// The events of @p branch, each as FlightEvent::toString() prints it (type, time, source and
/// data), one per line.
[[nodiscard]] inline std::string eventsText(const FlightDataBranch& branch)
{
    std::string text;
    for (const FlightEvent& event : branch.getEvents())
    {
        text += event.toString() + "\n";
    }
    return text;
}

/// What differs between the columns of the branches @p expected and @p actual, which have the
/// same types: every column but the wall clock (TYPE_COMPUTATION_TIME), bit by bit.
[[nodiscard]] inline std::string columnDifferences(const FlightDataBranch& expected,
                                                   const FlightDataBranch& actual)
{
    const FlightDataType& wallClock =
        FlightDataType::builtin(FlightDataTypeId::TYPE_COMPUTATION_TIME);
    std::string differences;
    for (const FlightDataType* type : expected.getTypes())
    {
        const std::vector<double>* expectedColumn = expected.getView(*type);
        const std::vector<double>* actualColumn   = actual.getView(*type);
        if (type == &wallClock || expectedColumn == nullptr || actualColumn == nullptr)
        {
            continue;
        }
        const std::string difference = columnDifference(*expectedColumn, *actualColumn);
        if (!difference.empty())
        {
            differences += std::format("  {}: {}\n", type->getName(), difference);
        }
    }
    return differences;
}

/// What differs between the branches @p expected and @p actual: the name, the events, the data
/// types and every value; "" when they are the same branch.
[[nodiscard]] inline std::string branchDifferences(const FlightDataBranch& expected,
                                                   const FlightDataBranch& actual)
{
    std::string differences;
    if (expected.getName() != actual.getName())
    {
        differences += std::format("  name: {}, not {}\n", actual.getName(), expected.getName());
    }
    if (eventsText(expected) != eventsText(actual))
    {
        differences +=
            std::format("  events:\n{}  not\n{}", eventsText(actual), eventsText(expected));
    }
    if (expected.getTypes() != actual.getTypes())
    {
        differences += "  other data types\n";
        return differences;
    }
    return differences + columnDifferences(expected, actual);
}

/// One summary value of a flight.
struct SummaryValue
{
    std::string_view name;
    double (FlightData::*get)() const noexcept;
};

/// What differs between the summary values of @p expected and @p actual, bit by bit.
[[nodiscard]] inline std::string summaryDifferences(const FlightData& expected,
                                                    const FlightData& actual)
{
    static constexpr std::array<SummaryValue, 10> kValues{{
        {.name = "maxAltitude", .get = &FlightData::getMaxAltitude},
        {.name = "maxVelocity", .get = &FlightData::getMaxVelocity},
        {.name = "maxAcceleration", .get = &FlightData::getMaxAcceleration},
        {.name = "maxMachNumber", .get = &FlightData::getMaxMachNumber},
        {.name = "timeToApogee", .get = &FlightData::getTimeToApogee},
        {.name = "flightTime", .get = &FlightData::getFlightTime},
        {.name = "groundHitVelocity", .get = &FlightData::getGroundHitVelocity},
        {.name = "launchRodVelocity", .get = &FlightData::getLaunchRodVelocity},
        {.name = "deploymentVelocity", .get = &FlightData::getDeploymentVelocity},
        {.name = "optimumDelay", .get = &FlightData::getOptimumDelay},
    }};
    std::string                                   differences;
    for (const SummaryValue& value : kValues)
    {
        const double e = (expected.*value.get)();
        const double a = (actual.*value.get)();
        if (!junitEquals(e, a))
        {
            differences += std::format("  {}: {}, not {}\n", value.name, a, e);
        }
    }
    return differences;
}

/// What differs between the flights @p expected and @p actual: the summary values, the
/// warnings, the branches with their events and every value of every column (but the wall
/// clock), all to the last bit; "" when they are the same flight.
[[nodiscard]] inline std::string flightDifferences(const FlightData& expected,
                                                   const FlightData& actual)
{
    std::string differences = summaryDifferences(expected, actual);
    if (expected.getWarningSet().toString() != actual.getWarningSet().toString())
    {
        differences += std::format("  warnings: {}, not {}\n", actual.getWarningSet().toString(),
                                   expected.getWarningSet().toString());
    }
    if (expected.getBranchCount() != actual.getBranchCount())
    {
        differences += std::format("  {} branches, not {}\n", actual.getBranchCount(),
                                   expected.getBranchCount());
        return differences;
    }
    for (std::size_t i = 0; i < expected.getBranchCount(); i++)
    {
        const std::string branch = branchDifferences(expected.getBranch(i), actual.getBranch(i));
        if (!branch.empty())
        {
            differences += std::format(" branch {}:\n{}", i, branch);
        }
    }
    return differences;
}

}  // namespace QtRocket::Test
