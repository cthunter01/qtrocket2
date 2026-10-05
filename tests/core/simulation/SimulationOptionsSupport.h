#pragma once

// What the tests of SimulationOptions and DefaultSimulationOptionFactory share: a counter of
// change events, the comparison with a Java-pinned value, a preference store with every
// simulation key set, and a text dump of every option. Test-only.

#include <format>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/Signal.h"
#include "QtRocket/util/Strings.h"
#include "rocket/JavaValueDifferences.h"

namespace QtRocket::Test
{

/// Counts the emissions of a signal while it lives (Java's tests count with an AtomicInteger in
/// a StateChangeListener). The count is mutable: the slot changes it through the `this` the
/// constructor captured, also when the counter is declared const, as most tests declare it.
class ChangeCounter
{
public:
    explicit ChangeCounter(Signal<>& signal) : m_connection(signal.connect([this] { ++m_count; }))
    {
    }

    [[nodiscard]] int count() const noexcept { return m_count; }
    /// Whether the signal this counter listens to still exists.
    [[nodiscard]] bool connected() const noexcept { return m_connection.connected(); }
    void               reset() noexcept { m_count = 0; }

private:
    mutable int                m_count{0};
    Signal<>::ScopedConnection m_connection;
};

/// Whether @p actual is the value @p expected that a Java probe printed, within 1e-12 relative
/// (matchesPinnedValue()): for values that went through a transcendental function, whose last
/// bits differ between math libraries.
[[nodiscard]] inline ::testing::AssertionResult isJavaValue(double expected, double actual)
{
    if (matchesPinnedValue(expected, actual))
    {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure()
           << std::format("expected {} (Java), got {}", expected, actual);
}

/// Stores a value that is not the default under every simulation key of @p preferences (the
/// store of OptionsProbe.preferencesConstructor()), the keys SimulationOptions does not read
/// included.
inline void storeEverySimulationKey(Preferences& preferences)
{
    namespace Keys = PreferenceKeys;
    preferences.putDouble(Keys::kLaunchRodLength, 2.5);
    preferences.putBoolean(Keys::kLaunchIntoWind, false);
    preferences.putDouble(Keys::kLaunchRodAngle, 0.25);
    preferences.putDouble(Keys::kLaunchRodDirection, 1.75);
    preferences.putDouble(Keys::kWindAverage, 6.0);
    preferences.putDouble(Keys::kWindTurbulence, 0.25);
    preferences.putDouble(Keys::kWindDirection, 3.5);
    preferences.putDouble(Keys::kLaunchAltitude, 321.0);
    preferences.putDouble(Keys::kLaunchLatitude, 45.5);
    preferences.putDouble(Keys::kLaunchLongitude, 12.25);
    preferences.putBoolean(Keys::kLaunchUseIsa, false);
    preferences.putDouble(Keys::kLaunchTemperature, 301.5);
    preferences.putDouble(Keys::kLaunchPressure, 98765.0);
    preferences.putDouble(Keys::kLaunchRelativeHumidity, 0.4);
    preferences.putDouble(Keys::kSimulationTimeStep, 0.02);
    preferences.putDouble(Keys::kSimulationMaxTime, 900.0);
    preferences.setGeodeticComputationName("WGS84");
    preferences.setGravityModelName("CONSTANT");
    preferences.putDouble(Keys::kConstantGravityValue, 3.71);
    preferences.setSimulationStepperMethodName("RK6");
    preferences.putDouble(Keys::kRecoverySpeedWarning, 22.0);
    preferences.putDouble(Keys::kDrogueLowSpeedWarning, 4.5);
    preferences.putDouble(Keys::kRecoveryDrogueMainHighSpeedWarning, 33.0);
    preferences.putDouble(Keys::kRecoveryDrogueMainLowSpeedWarning, 12.0);
    preferences.putInt(Keys::kSimulationRandomSeed, 424242);
    preferences.putBoolean(Keys::kSimulationRandomSeedFixed, true);
}

/// The levels of @p model as "[altitude, speed, direction, deviation]" each, in order.
[[nodiscard]] inline std::string describeLevels(const MultiLevelPinkNoiseWindModel& model)
{
    std::string text;
    for (const MultiLevelPinkNoiseWindModel::LevelWindModel* level : model.getLevels())
    {
        text += std::format("[{}, {}, {}, {}]", level->getAltitude(), level->getSpeed(),
                            level->getDirection(), level->getStandardDeviation());
    }
    return text;
}

/// The CSV rows joined with '|', or "null".
[[nodiscard]] inline std::string describeRows(const std::optional<std::vector<std::string>>& rows)
{
    return rows.has_value() ? Strings::join("|", *rows) : "null";
}

/// Every option of @p options but the random seed and the identity of the lookup tables, one
/// "name = value" line each with the doubles written exactly: two options with the same text
/// hold the same values.
[[nodiscard]] inline std::string describe(const SimulationOptions& options)
{
    const PinkNoiseWindModel& wind = options.getAverageWindModel();
    std::string               text;
    text += std::format("launchRodLength = {}\n", options.getLaunchRodLength());
    text += std::format("launchIntoWind = {}\n", options.getLaunchIntoWind());
    text += std::format("launchRodAngle = {}\n", options.getLaunchRodAngle());
    text += std::format("launchRodDirection = {}\n", options.getLaunchRodDirection());
    text += std::format("windModelType = {}\n", windModelTypeName(options.getWindModelType()));
    text += std::format("wind = {} {} {}\n", wind.getAverage(), wind.getStandardDeviation(),
                        wind.getDirection());
    text += std::format("multi.levels = {}\n", describeLevels(options.getMultiLevelWindModel()));
    text += std::format("multi.altitudeReference = {}\n",
                        toString(options.getMultiLevelWindModel().getAltitudeReference()));
    text += std::format("launchSite = {} {} {}\n", options.getLaunchAltitude(),
                        options.getLaunchLatitude(), options.getLaunchLongitude());
    text += std::format("geodeticComputation = {}\n", name(options.getGeodeticComputation()));
    text += std::format("atmosphere = {} {} {} {}\n", options.isIsaAtmosphere(),
                        options.getLaunchTemperature(), options.getLaunchPressure(),
                        options.getLaunchRelativeHumidity());
    text += std::format("stepper = {} {} {} {}\n",
                        simulationStepperMethodName(options.getSimulationStepperMethodChoice()),
                        options.getTimeStep(), options.getMaxSimulationTime(),
                        options.getMaximumStepAngle());
    text += std::format("randomSeedFixed = {}\n", options.isRandomSeedFixed());
    text += std::format("gravity = {} {}\n", gravityModelTypeName(options.getGravityModelType()),
                        options.getConstantGravity());
    text += std::format("recovery = {} {} {} {}\n", options.getRecoverySpeedWarning(),
                        options.getDrogueLowSpeedWarning(),
                        options.getRecoveryDrogueMainHighSpeedWarning(),
                        options.getRecoveryDrogueMainLowSpeedWarning());
    text += std::format("dragLookup = {} {} {}\n", options.getDragLookupCsvPath().has_value(),
                        options.hasDragLookup(), describeRows(options.getDragLookupCsvRows()));
    text += std::format(
        "stabilityLookup = {} {} {}\n", options.getStabilityLookupCsvPath().has_value(),
        options.hasStabilityLookup(), describeRows(options.getStabilityLookupCsvRows()));
    return text;
}

}  // namespace QtRocket::Test
