#pragma once

// What the tests that need a simulation status share: a status of a real rocket, built as the
// simulation engine builds one, and helpers to compare what a Java probe printed. Test-only.

#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Quaternion.h"
#include "rocket/JavaValueDifferences.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Test
{

/// A flight configuration for a simulation status: a clone of @p rocket's configuration
/// @p fcid, as a status holds one (the engine clones the configuration of the simulated rocket).
[[nodiscard]] inline std::shared_ptr<FlightConfiguration> statusConfiguration(
    Rocket& rocket, const FlightConfigurationId& fcid)
{
    return std::make_shared<FlightConfiguration>(rocket.getFlightConfiguration(fcid).clone());
}

/// A flight data branch as the engine makes one for the sustainer: a TIME column only.
[[nodiscard]] inline std::shared_ptr<FlightDataBranch> newBranch(const std::string& name = "branch")
{
    return std::make_shared<FlightDataBranch>(
        name, std::initializer_list<std::reference_wrapper<const FlightDataType>>{
                  FlightDataType::builtin(FlightDataTypeId::TYPE_TIME)});
}

/// A simulation status of a real rocket: the Estes Alpha III in its first test configuration
/// (TestRockets' TEST_FCID_0), on default simulation conditions (Java: new
/// SimulationStatus(config, new SimulationConditions())), with a flight data branch, so that
/// warnings can be added to it.
struct TestStatus
{
    TestEstesAlphaIII                     alpha;
    std::shared_ptr<SimulationConditions> conditions = std::make_shared<SimulationConditions>();
    std::shared_ptr<FlightDataBranch>     branch     = newBranch();
    SimulationStatus                      status;

    TestStatus() : status(statusConfiguration(*alpha.rocket, testFcid(0)), conditions)
    {
        status.setFlightDataBranch(branch);
    }
};

/// The display names of the events of @p branch, in order, for a readable comparison.
[[nodiscard]] inline std::vector<std::string> eventTypeNames(const FlightDataBranch& branch)
{
    std::vector<std::string> names;
    for (const FlightEvent& event : branch.getEvents())
    {
        names.emplace_back(name(event.getType()));
    }
    return names;
}

/// Whether the four components of @p actual are the values (w, x, y, z) a Java probe printed,
/// each within 1e-12 of the larger magnitude, or within 1e-15 absolute for a component that
/// cancels to (nearly) zero: the components went through sin() and cos().
[[nodiscard]] inline ::testing::AssertionResult isJavaQuaternion(const Quaternion& actual, double w,
                                                                 double x, double y, double z)
{
    const auto close = [](double expected, double value) {
        return matchesPinnedValue(expected, value) || std::abs(expected - value) <= 1e-15;
    };
    if (close(w, actual.w()) && close(x, actual.x()) && close(y, actual.y()) &&
        close(z, actual.z()))
    {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure()
           << std::format("expected ({}, {}, {}, {}) (Java), got ({}, {}, {}, {})", w, x, y, z,
                          actual.w(), actual.x(), actual.y(), actual.z());
}

}  // namespace QtRocket::Test
