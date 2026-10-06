#pragma once

// What the tests that run whole simulations share: the preferences OpenRocket's JUnit tests run
// under, and small helpers around Simulation::simulate(). Test-only.

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Error.h"

namespace QtRocket::Test
{

/// Stores in @p preferences what OpenRocket's JUnit tests read from their preferences
/// (ServicesForTesting.PreferencesForTesting, which BaseTestCase installs): 0 for every number
/// and false for every flag, but for the launch temperature, pressure and humidity and the
/// constant gravity, which keep their defaults. A Java `new Simulation(rocket)` in a test is
/// then `Simulation(rocket, preferences)` here: a launch rod of length 0, pointing straight up
/// and not into the wind, no wind, the launch site at latitude 0, longitude 0 and sea level,
/// the launch conditions entered by hand (not the ISA model) and a time step of 0. The maximum
/// simulation time stays 1200 s (ApplicationPreferences.getMaxSimulationTime() replaces a 0).
inline void storeJavaTestPreferences(Preferences& preferences)
{
    namespace Keys = PreferenceKeys;
    preferences.putDouble(Keys::kLaunchRodLength, 0.0);
    preferences.putBoolean(Keys::kLaunchIntoWind, false);
    preferences.putDouble(Keys::kLaunchRodAngle, 0.0);
    preferences.putDouble(Keys::kLaunchRodDirection, 0.0);
    preferences.putDouble(Keys::kWindAverage, 0.0);
    preferences.putDouble(Keys::kWindTurbulence, 0.0);
    preferences.putDouble(Keys::kWindDirection, 0.0);
    preferences.putDouble(Keys::kLaunchAltitude, 0.0);
    preferences.putDouble(Keys::kLaunchLatitude, 0.0);
    preferences.putDouble(Keys::kLaunchLongitude, 0.0);
    preferences.putBoolean(Keys::kLaunchUseIsa, false);
    preferences.putDouble(Keys::kSimulationTimeStep, 0.0);
}

/// The preference store of a test that mirrors a JUnit test (storeJavaTestPreferences()).
struct JavaTestPreferences
{
    InMemoryPreferences store;

    JavaTestPreferences() { storeJavaTestPreferences(store); }
};

/// Runs @p simulation and expects the run to succeed (Java: simulate() without an exception).
inline void simulateOrFail(Simulation& simulation)
{
    const Result<void> result = simulation.simulate();
    EXPECT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error().message);
}

/// The simulated data of @p simulation, which a test has just run.
[[nodiscard]] inline const FlightData& simulatedData(const Simulation& simulation)
{
    const std::shared_ptr<FlightData>& data = simulation.getSimulatedData();
    if (data == nullptr)
    {
        ADD_FAILURE() << "the simulation has no simulated data";
        return FlightData::nanData();
    }
    return *data;
}

/// The names of the event types of branch @p branchNo of @p data, in order ("LAUNCH",
/// "IGNITION", ...); empty for a branch the data do not have.
[[nodiscard]] inline std::vector<std::string> eventNames(const FlightData& data,
                                                         std::size_t       branchNo)
{
    std::vector<std::string> names;
    if (branchNo >= data.getBranchCount())
    {
        return names;
    }
    for (const FlightEvent& event : data.getBranch(branchNo).getEvents())
    {
        names.emplace_back(name(event.getType()));
    }
    return names;
}

/// The causes of the SIM_ABORT events of branch @p branchNo of @p data, in order.
[[nodiscard]] inline std::vector<SimulationAbort::Cause> abortCauses(const FlightData& data,
                                                                     std::size_t       branchNo)
{
    std::vector<SimulationAbort::Cause> causes;
    if (branchNo >= data.getBranchCount())
    {
        return causes;
    }
    for (const FlightEvent& event : data.getBranch(branchNo).getEvents())
    {
        if (const SimulationAbort* abort = event.getAbort())
        {
            causes.push_back(abort->cause());
        }
    }
    return causes;
}

}  // namespace QtRocket::Test
