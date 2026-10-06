#include "QtRocket/simulation/SimulationOptions.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicCalculator.h"
#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/BarrowmanCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/lookup/CsvMachAoALookup.h"
#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/models/AtmosphericModel.h"
#include "QtRocket/models/ConstantGravityModel.h"
#include "QtRocket/models/ExtendedIsaModel.h"
#include "QtRocket/models/GravityModel.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WgsGravityModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/simulation/DefaultSimulationOptionFactory.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Signal.h"
#include "QtRocket/util/WorldCoordinate.h"
#include "TestTempDir.h"
#include "rocket/JavaValueDifferences.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationOptionsSupport.h"

namespace
{

using QtRocket::AtmosphericModel;
using QtRocket::BugError;
using QtRocket::ConstantGravityModel;
using QtRocket::Coordinate;
using QtRocket::DefaultSimulationOptionFactory;
using QtRocket::ErrorCode;
using QtRocket::ExtendedIsaModel;
using QtRocket::GeodeticComputationStrategy;
using QtRocket::GravityModelType;
using QtRocket::InMemoryPreferences;
using QtRocket::MachAoALookup;
using QtRocket::MultiLevelPinkNoiseWindModel;
using QtRocket::PinkNoiseWindModel;
using QtRocket::Result;
using QtRocket::SimulationConditions;
using QtRocket::SimulationOptions;
using QtRocket::SimulationStepperMethod;
using QtRocket::WgsGravityModel;
using QtRocket::WindModel;
using QtRocket::WindModelType;
using QtRocket::Test::ChangeCounter;
using QtRocket::Test::describe;
using QtRocket::Test::describeLevels;
using QtRocket::Test::isJavaValue;
using QtRocket::Test::JavaValueDifferences;
using QtRocket::Test::storeEverySimulationKey;

namespace Keys = QtRocket::PreferenceKeys;

// The pinned numbers and event counts below are what OpenRocket prints for the same calls
// (probes/options-extensions-impl/OptionsProbe.java, against a MockPreferences store as
// DefaultSimulationOptionFactoryTest sets one up; probes/options-extensions-fix/FixProbe.java for
// the tests that name FixProbe), unless a test names a JUnit original.

constexpr double kPi  = std::numbers::pi;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// ExtendedISAModel.getMaximumAllowedAltitude(), as Java prints it.
constexpr double kMaximumAltitude = 11018.064362274883;

/// The ISA at the altitudes the tests use: temperature (K) and pressure (Pa), as Java prints them.
constexpr double kIsaTemperature100  = 287.5000511226052;
constexpr double kIsaPressure100     = 100152.25761373011;
constexpr double kIsaTemperature200  = 286.85010224521045;
constexpr double kIsaPressure200     = 98979.51522746022;
constexpr double kIsaTemperature1234 = 280.12737165226173;
constexpr double kIsaPressure1234    = 87382.7925670164;
constexpr double kIsaTemperatureTop  = 216.76905034797787;
constexpr double kIsaPressureTop     = 22638.105069079076;

/// A seed that differs from @p seed (Java's tests add 1, which wraps there and overflows here).
[[nodiscard]] constexpr int otherSeed(int seed) noexcept
{
    return seed ^ 1;
}

/// Adds a wind level, which must succeed.
void addLevel(MultiLevelPinkNoiseWindModel& model, double altitude, double speed, double direction,
              std::optional<double> standardDeviation = std::nullopt)
{
    EXPECT_TRUE(model.addWindLevel(altitude, speed, direction, standardDeviation).has_value());
}

[[nodiscard]] std::span<const std::string> dragColumns()
{
    static const std::array<std::string, 1> kColumns{"cd"};
    return kColumns;
}

[[nodiscard]] std::span<const std::string> stabilityColumns()
{
    static const std::array<std::string, 3> kColumns{"cn", "cm", "cp"};
    return kColumns;
}

/// The table of the CSV @p rows, which must parse.
[[nodiscard]] std::shared_ptr<const MachAoALookup> parseTable(const std::vector<std::string>& rows,
                                                              std::span<const std::string> columns)
{
    Result<MachAoALookup> table = QtRocket::CsvMachAoALookup::parse(rows, columns, ',');
    EXPECT_TRUE(table.has_value());
    if (!table.has_value())
    {
        return nullptr;
    }
    return std::make_shared<const MachAoALookup>(std::move(*table));
}

/// The rows of a drag table whose coefficient is @p value everywhere.
[[nodiscard]] std::vector<std::string> dragRows(std::string_view value)
{
    return {"Mach,Cd", std::format("0,{}", value), std::format("1,{}", value)};
}

/// The rows of a stability table whose CP is @p value everywhere.
[[nodiscard]] std::vector<std::string> stabilityRows(std::string_view value)
{
    return {"Mach,Cn,Cm,Cp", std::format("0,1,1,{}", value), std::format("1,1,1,{}", value)};
}

/// SimulationLookupCopyTest.CSV
[[nodiscard]] std::filesystem::path lookupCsv()
{
    return "missing-beta-test-lookup.csv";
}

/// SimulationLookupCopyTest.options(value): options with a drag and a stability table of
/// @p value, parsed from rows that are kept. @p value is the text Java's string concatenation
/// gives the double ("2.0").
[[nodiscard]] SimulationOptions lookupOptions(std::string_view value)
{
    SimulationOptions              options;
    const std::vector<std::string> drag      = dragRows(value);
    const std::vector<std::string> stability = stabilityRows(value);
    options.setDragLookup(lookupCsv(), parseTable(drag, dragColumns()), drag);
    options.setStabilityLookup(lookupCsv(), parseTable(stability, stabilityColumns()), stability);
    return options;
}

/// SimulationLookupCopyTest.assertLookups()
void expectLookups(const SimulationOptions& options, double value)
{
    ASSERT_TRUE(options.hasDragLookup());
    ASSERT_TRUE(options.hasStabilityLookup());
    EXPECT_EQ(options.getDragLookupTable()->interpolate(0.5, 0, "cd"), value);
    EXPECT_EQ(options.getStabilityLookupTable()->interpolate(0.5, 0, "cp"), value);
}

/// Sets every option of @p options to a value that is not its default.
void populate(SimulationOptions& options)
{
    options.setLaunchRodLength(2.5);
    options.setLaunchIntoWind(false);
    options.setLaunchRodAngle(0.25);
    options.setLaunchRodDirection(1.75);
    options.setWindModelType(WindModelType::MULTI_LEVEL);
    options.getAverageWindModel().setAverage(6.0);
    options.getAverageWindModel().setStandardDeviation(1.5);
    options.getAverageWindModel().setDirection(3.5);
    options.getMultiLevelWindModel().clearLevels();
    addLevel(options.getMultiLevelWindModel(), 100, 4, 1, 0.5);
    addLevel(options.getMultiLevelWindModel(), 900, 8, 2, 1.0);
    options.getMultiLevelWindModel().setAltitudeReference(WindModel::AltitudeReference::AGL);
    options.setIsaAtmosphere(false);
    options.setLaunchAltitude(321);
    options.setLaunchLatitude(45.5);
    options.setLaunchLongitude(12.25);
    options.setGeodeticComputation(GeodeticComputationStrategy::WGS84);
    options.setLaunchTemperature(301.5);
    options.setLaunchPressure(98765);
    options.setLaunchRelativeHumidity(0.4);
    options.setTimeStep(0.02);
    options.setMaxSimulationTime(900);
    options.setMaximumStepAngle(0.1);
    options.setGravityModelType(GravityModelType::CONSTANT);
    options.setConstantGravity(3.71);
    options.setSimulationStepperMethodChoice(SimulationStepperMethod::RK6);
    options.setRecoverySpeedWarning(22);
    options.setDrogueLowSpeedWarning(4.5);
    options.setRecoveryDrogueMainHighSpeedWarning(33);
    options.setRecoveryDrogueMainLowSpeedWarning(12);
    const std::vector<std::string> drag      = dragRows("0.5");
    const std::vector<std::string> stability = stabilityRows("0.25");
    options.setDragLookup(lookupCsv(), parseTable(drag, dragColumns()), drag);
    options.setStabilityLookup(lookupCsv(), parseTable(stability, stabilityColumns()), stability);
    options.setRandomSeed(424242);
    options.setRandomSeedFixed(true);
}

using Getter = double (SimulationOptions::*)() const noexcept;
using Setter = void (SimulationOptions::*)(double);

/// What a setter that only compares with MathUtil::equals does: the events after setting the
/// stored value again, a value within the tolerance (and whether the stored value stayed),
/// another value (and whether it was stored), a NaN (and whether it was stored) and the NaN again.
[[nodiscard]] std::string plainSetter(Getter getter, Setter setter)
{
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    const double      initial = (options.*getter)();

    (options.*setter)(initial);
    const int same = events.count();
    (options.*setter)(initial + (1e-10 * std::max(1.0, std::abs(initial))));
    const int  withinTolerance = events.count();
    const bool stayed          = (options.*getter)() == initial;
    (options.*setter)(initial + 7.25);
    const int  changed = events.count();
    const bool stored  = (options.*getter)() == initial + 7.25;
    (options.*setter)(kNaN);
    const int  nan       = events.count();
    const bool nanStored = std::isnan((options.*getter)());
    (options.*setter)(kNaN);
    return std::format("{} {} {} {} {} {} {} {}", same, withinTolerance, stayed, changed, stored,
                       nan, nanStored, events.count());
}

/// plainSetter() of a setter that ports Java's: nothing for the stored value or one within the
/// tolerance, one event per change, and a NaN is a change every time.
constexpr std::string_view kPlainSetter = "0 0 true 1 true 2 true 3";

/// What a listener of the options reads from them, and from their preferences, when it is called.
struct SeenState
{
    /// describe() of the options.
    std::string values;
    /// The stepper method the preferences hold.
    std::string storedStepper;
    bool        isa{false};
    double      altitude{0};
    double      temperature{0};
    double      pressure{0};
    double      humidity{0};
};

/// Records what a listener of the options sees at every change event, while it lives. The
/// record is mutable: the slot fills it through the `this` the constructor captured, also when
/// the recorder is declared const.
class StateRecorder
{
public:
    StateRecorder(SimulationOptions& options, const QtRocket::Preferences& preferences)
      : m_connection(options.changed().connect([this, &options, &preferences] {
            m_seen.push_back(
                SeenState{.values        = describe(options),
                          .storedStepper = preferences.getSimulationStepperMethodName(),
                          .isa           = options.isIsaAtmosphere(),
                          .altitude      = options.getLaunchAltitude(),
                          .temperature   = options.getLaunchTemperature(),
                          .pressure      = options.getLaunchPressure(),
                          .humidity      = options.getLaunchRelativeHumidity()});
        }))
    {
    }

    [[nodiscard]] const std::vector<SeenState>& seen() const noexcept { return m_seen; }

private:
    mutable std::vector<SeenState>       m_seen;
    QtRocket::Signal<>::ScopedConnection m_connection;
};

// ------------------------------------------------------------------------------ defaults

TEST(SimulationOptions, Constants)
{
    EXPECT_EQ(SimulationOptions::kMaxLaunchRodAngle, 1.0471975511965976);
    EXPECT_EQ(SimulationOptions::kMaxLaunchRodAngle, kPi / 3);
    EXPECT_EQ(ExtendedIsaModel::getMaximumAllowedAltitude(), kMaximumAltitude);
}

TEST(SimulationOptions, DefaultLaunchRodAndWind)
{
    const SimulationOptions options;
    EXPECT_EQ(options.getLaunchRodLength(), 1.0);
    EXPECT_TRUE(options.getLaunchIntoWind());
    EXPECT_EQ(options.getLaunchRodAngle(), 0.0);
    EXPECT_EQ(options.getLaunchRodDirection(), kPi / 2);
    EXPECT_EQ(options.getWindModelType(), WindModelType::AVERAGE);
    // The options' own average wind is calm; the wind preferences only reach the initial level
    // of the multi-level model (2 m/s from the east with 10 % turbulence in an empty store).
    EXPECT_EQ(options.getAverageWindModel().getAverage(), 0.0);
    EXPECT_EQ(options.getAverageWindModel().getStandardDeviation(), 0.0);
    EXPECT_EQ(options.getAverageWindModel().getDirection(), kPi / 2);
    EXPECT_EQ(options.getAverageWindModel().getTurbulenceIntensity(), 0.0);
    EXPECT_EQ(describeLevels(options.getMultiLevelWindModel()), "[0, 2, 1.5707963267948966, 0.2]");
    EXPECT_EQ(options.getMultiLevelWindModel().getAltitudeReference(),
              WindModel::AltitudeReference::MSL);
    EXPECT_EQ(&options.getWindModel(), &options.getAverageWindModel());
}

TEST(SimulationOptions, DefaultLaunchSiteAndAtmosphere)
{
    const SimulationOptions options;
    EXPECT_EQ(options.getLaunchAltitude(), 0.0);
    EXPECT_EQ(options.getLaunchLatitude(), 28.61);
    EXPECT_EQ(options.getLaunchLongitude(), -80.6);
    EXPECT_EQ(options.getGeodeticComputation(), GeodeticComputationStrategy::SPHERICAL);
    EXPECT_TRUE(options.isIsaAtmosphere());
    EXPECT_EQ(options.getLaunchTemperature(), 288.15);
    EXPECT_EQ(options.getLaunchPressure(), 101325.0);
    EXPECT_EQ(options.getLaunchRelativeHumidity(), 0.0);
}

TEST(SimulationOptions, DefaultStepperGravityAndThresholds)
{
    const SimulationOptions options;
    EXPECT_EQ(options.getTimeStep(), 0.05);
    EXPECT_EQ(options.getMaxSimulationTime(), 1200.0);
    EXPECT_EQ(options.getMaximumStepAngle(), 0.05235987755982988);
    EXPECT_EQ(options.getMaximumStepAngle(), 3 * kPi / 180);
    EXPECT_FALSE(options.isRandomSeedFixed());
    EXPECT_EQ(options.getGravityModelType(), GravityModelType::WGS);
    EXPECT_EQ(options.getConstantGravity(), 9.807);
    EXPECT_EQ(options.getSimulationStepperMethodChoice(), SimulationStepperMethod::RK4);
    EXPECT_EQ(options.getRecoverySpeedWarning(), 20.0);
    EXPECT_EQ(options.getDrogueLowSpeedWarning(), 3.048);
    EXPECT_EQ(options.getRecoveryDrogueMainHighSpeedWarning(), 30.48);
    EXPECT_EQ(options.getRecoveryDrogueMainLowSpeedWarning(), 15.24);
}

TEST(SimulationOptions, DefaultLookupsAreAbsent)
{
    const SimulationOptions options;
    EXPECT_EQ(options.getDragLookupCsvPath(), std::nullopt);
    EXPECT_EQ(options.getDragLookupTable(), nullptr);
    EXPECT_FALSE(options.hasDragLookup());
    EXPECT_EQ(options.getDragLookupCsvRows(), std::nullopt);
    EXPECT_EQ(options.getStabilityLookupCsvPath(), std::nullopt);
    EXPECT_EQ(options.getStabilityLookupTable(), nullptr);
    EXPECT_FALSE(options.hasStabilityLookup());
    EXPECT_EQ(options.getStabilityLookupCsvRows(), std::nullopt);
}

TEST(SimulationOptions, DefaultConstructorHasTheValuesOfAnEmptyStore)
{
    InMemoryPreferences     preferences;
    const SimulationOptions fromStore(preferences);
    const SimulationOptions byDefault;
    EXPECT_EQ(describe(byDefault), describe(fromStore));
    // Constructing reads only.
    EXPECT_TRUE(preferences.empty());
}

// ------------------------------------------------------------- the preferences constructor

TEST(SimulationOptions, ConstructorReadsTheLaunchRodAndSite)
{
    InMemoryPreferences preferences;
    storeEverySimulationKey(preferences);
    const SimulationOptions options(preferences);
    EXPECT_EQ(options.getLaunchRodLength(), 2.5);
    EXPECT_FALSE(options.getLaunchIntoWind());
    EXPECT_EQ(options.getLaunchRodAngle(), 0.25);
    EXPECT_EQ(options.getLaunchRodDirection(), 1.75);
    EXPECT_EQ(options.getLaunchAltitude(), 321.0);
    EXPECT_EQ(options.getLaunchLatitude(), 45.5);
    EXPECT_EQ(options.getLaunchLongitude(), 12.25);
    EXPECT_FALSE(options.isIsaAtmosphere());
    EXPECT_EQ(options.getLaunchTemperature(), 301.5);
    EXPECT_EQ(options.getLaunchPressure(), 98765.0);
    EXPECT_EQ(options.getLaunchRelativeHumidity(), 0.4);
    EXPECT_EQ(options.getTimeStep(), 0.02);
    EXPECT_EQ(options.getMaxSimulationTime(), 900.0);
    EXPECT_EQ(options.getGravityModelType(), GravityModelType::CONSTANT);
    EXPECT_EQ(options.getConstantGravity(), 3.71);
}

TEST(SimulationOptions, ConstructorLeavesTheOtherOptionsAtTheirConstants)
{
    InMemoryPreferences preferences;
    storeEverySimulationKey(preferences);
    const SimulationOptions options(preferences);
    // Java initialises these from constants, whatever the preferences hold.
    EXPECT_EQ(options.getGeodeticComputation(), GeodeticComputationStrategy::SPHERICAL);
    EXPECT_EQ(options.getSimulationStepperMethodChoice(), SimulationStepperMethod::RK4);
    EXPECT_EQ(options.getMaximumStepAngle(), 3 * kPi / 180);
    EXPECT_EQ(options.getRecoverySpeedWarning(), 20.0);
    EXPECT_EQ(options.getDrogueLowSpeedWarning(), 3.048);
    EXPECT_EQ(options.getRecoveryDrogueMainHighSpeedWarning(), 30.48);
    EXPECT_EQ(options.getRecoveryDrogueMainLowSpeedWarning(), 15.24);
    EXPECT_FALSE(options.isRandomSeedFixed());
    EXPECT_EQ(options.getWindModelType(), WindModelType::AVERAGE);
    EXPECT_EQ(options.getAverageWindModel().getAverage(), 0.0);
    EXPECT_EQ(options.getAverageWindModel().getStandardDeviation(), 0.0);
    EXPECT_EQ(options.getAverageWindModel().getDirection(), kPi / 2);
}

TEST(SimulationOptions, ConstructorMakesTheInitialWindLevelFromTheWindPreferences)
{
    // 6 m/s from 3.5 rad with 25 % turbulence. (The probe prints the level of an empty store
    // there: Java's MultiLevelPinkNoiseWindModel holds the preferences in a static field, which
    // in the probe still is the store of an earlier scenario; in the application it is the one
    // preference store.)
    InMemoryPreferences preferences;
    storeEverySimulationKey(preferences);
    const SimulationOptions options(preferences);
    EXPECT_EQ(describeLevels(options.getMultiLevelWindModel()), "[0, 6, 3.5, 1.5]");
}

TEST(SimulationOptions, ConstructorDoesNotChangeTheStore)
{
    InMemoryPreferences preferences;
    storeEverySimulationKey(preferences);
    const InMemoryPreferences before = preferences;
    SimulationOptions         options(preferences);
    EXPECT_TRUE(preferences == before);
    options.setLaunchRodLength(7);
    options.setLaunchAltitude(55);
    options.setRandomSeedFixed(true);
    // Only the stepper choice is ever written back.
    EXPECT_TRUE(preferences == before);
}

TEST(SimulationOptions, ConstructorReadsTheRodDirectionKeyItself)
{
    // Preferences::getLaunchRodDirection() would store the wind direction as the rod direction
    // while launching into the wind; Java's field initialiser reads the key directly.
    InMemoryPreferences preferences;
    preferences.putDouble(Keys::kWindDirection, 4.0);
    preferences.putDouble(Keys::kLaunchRodDirection, 1.0);
    SimulationOptions options(preferences);
    EXPECT_EQ(preferences.getDouble(Keys::kLaunchRodDirection, -1), 1.0);
    // Into the wind, the direction is that of the options' own (calm, easterly) average wind.
    EXPECT_EQ(options.getLaunchRodDirection(), kPi / 2);
    options.setLaunchIntoWind(false);
    EXPECT_EQ(options.getLaunchRodDirection(), 1.0);
}

TEST(SimulationOptions, AStoredMaximumTimeOfZeroIsTheRecommendedTime)
{
    InMemoryPreferences preferences;
    preferences.putDouble(Keys::kSimulationMaxTime, 0.0);
    const SimulationOptions options(preferences);
    EXPECT_EQ(options.getMaxSimulationTime(), 1200.0);
}

// ---------------------------------------------------------------------------- launch rod

TEST(SimulationOptions, SetLaunchRodLength)
{
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    options.setLaunchRodLength(1.0);
    EXPECT_EQ(events.count(), 0);
    options.setLaunchRodLength(1.0 + 1e-10);
    EXPECT_EQ(events.count(), 0);
    EXPECT_EQ(options.getLaunchRodLength(), 1.0);
    options.setLaunchRodLength(2.5);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(options.getLaunchRodLength(), 2.5);
    options.setLaunchRodLength(kNaN);
    EXPECT_EQ(events.count(), 2);
    EXPECT_TRUE(std::isnan(options.getLaunchRodLength()));
    options.setLaunchRodLength(kNaN);
    EXPECT_EQ(events.count(), 3);
}

TEST(SimulationOptions, SetLaunchIntoWind)
{
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    options.setLaunchIntoWind(true);
    EXPECT_EQ(events.count(), 0);
    options.setLaunchIntoWind(false);
    EXPECT_EQ(events.count(), 1);
    EXPECT_FALSE(options.getLaunchIntoWind());
    options.setLaunchIntoWind(false);
    EXPECT_EQ(events.count(), 1);
    options.setLaunchIntoWind(true);
    EXPECT_EQ(events.count(), 2);
    EXPECT_TRUE(options.getLaunchIntoWind());
}

TEST(SimulationOptions, SetLaunchRodAngleClamps)
{
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    options.setLaunchRodAngle(0.0);
    EXPECT_EQ(events.count(), 0);
    options.setLaunchRodAngle(0.3);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(options.getLaunchRodAngle(), 0.3);
    options.setLaunchRodAngle(2.0);
    EXPECT_EQ(events.count(), 2);
    EXPECT_EQ(options.getLaunchRodAngle(), SimulationOptions::kMaxLaunchRodAngle);
    // Clamped to the stored value: nothing happens.
    options.setLaunchRodAngle(5.0);
    EXPECT_EQ(events.count(), 2);
    options.setLaunchRodAngle(-2.0);
    EXPECT_EQ(events.count(), 3);
    EXPECT_EQ(options.getLaunchRodAngle(), -SimulationOptions::kMaxLaunchRodAngle);
    // A NaN passes the clamp.
    options.setLaunchRodAngle(kNaN);
    EXPECT_EQ(events.count(), 4);
    EXPECT_TRUE(std::isnan(options.getLaunchRodAngle()));
}

TEST(SimulationOptions, SetLaunchRodDirectionReduces)
{
    SimulationOptions options;
    options.setLaunchIntoWind(false);
    ChangeCounter events(options.changed());
    EXPECT_EQ(options.getLaunchRodDirection(), kPi / 2);
    options.setLaunchRodDirection(kPi / 2);
    EXPECT_EQ(events.count(), 0);
    options.setLaunchRodDirection(1.0);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(options.getLaunchRodDirection(), 1.0);
    // A full turn more is the stored direction again.
    options.setLaunchRodDirection(1.0 + (2 * kPi));
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(options.getLaunchRodDirection(), 1.0);
    options.setLaunchRodDirection(-1.0);
    EXPECT_EQ(events.count(), 2);
    EXPECT_DOUBLE_EQ(options.getLaunchRodDirection(), 5.283185307179586);
    options.setLaunchRodDirection(7.0);
    EXPECT_EQ(events.count(), 3);
    EXPECT_DOUBLE_EQ(options.getLaunchRodDirection(), 0.7168146928204138);
}

TEST(SimulationOptions, TheStoredRodDirectionIsHiddenWhileLaunchingIntoTheWind)
{
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    options.setLaunchRodDirection(1.0);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(options.getLaunchRodDirection(), kPi / 2);
    options.setLaunchIntoWind(false);
    EXPECT_EQ(options.getLaunchRodDirection(), 1.0);
}

// ------------------------------------------------------------------------------ the wind

TEST(SimulationOptions, LaunchIntoWindFollowsTheAverageWind)
{
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    options.getAverageWindModel().setDirection(2.0);
    EXPECT_EQ(options.getLaunchRodDirection(), 2.0);
    EXPECT_EQ(events.count(), 1);
    // The wind direction is stored reduced, and the rod direction reduces it again.
    options.getAverageWindModel().setDirection(-1.0);
    EXPECT_DOUBLE_EQ(options.getLaunchRodDirection(), 5.283185307179586);
}

TEST(SimulationOptions, LaunchIntoWindFollowsTheMultiLevelWindAtTheLaunchAltitude)
{
    SimulationOptions options;
    options.getAverageWindModel().setDirection(2.0);
    ChangeCounter events(options.changed());
    options.getMultiLevelWindModel().clearLevels();
    addLevel(options.getMultiLevelWindModel(), 0, 5.0, 1.25, 0.0);
    addLevel(options.getMultiLevelWindModel(), 1000, 10.0, 2.5, 0.0);
    EXPECT_EQ(events.count(), 3);
    // Still the average model's direction.
    EXPECT_EQ(options.getLaunchRodDirection(), 2.0);

    options.setWindModelType(WindModelType::MULTI_LEVEL);
    EXPECT_EQ(events.count(), 4);
    options.setWindModelType(WindModelType::MULTI_LEVEL);
    EXPECT_EQ(events.count(), 4);
    EXPECT_EQ(&options.getWindModel(), &options.getMultiLevelWindModel());

    EXPECT_TRUE(isJavaValue(1.25, options.getLaunchRodDirection()));
    options.setLaunchAltitude(500);
    EXPECT_TRUE(isJavaValue(2.1110127928165348, options.getLaunchRodDirection()));
    options.setLaunchAltitude(2000);
    EXPECT_TRUE(isJavaValue(2.5, options.getLaunchRodDirection()));

    options.setLaunchRodDirection(0.7);
    options.setLaunchIntoWind(false);
    EXPECT_EQ(options.getLaunchRodDirection(), 0.7);
}

TEST(SimulationOptions, LaunchIntoATurbulentMultiLevelWindIsTheDirectionTheModelItselfGives)
{
    // Java asks the options' own model for its wind at time 0; here a copy of the model answers
    // (see the class comment), which has the same levels and seeds.
    SimulationOptions             options;
    MultiLevelPinkNoiseWindModel& wind = options.getMultiLevelWindModel();
    wind.clearLevels();
    addLevel(wind, 0, 5.0, 1.25, 2.0);
    addLevel(wind, 1000, 10.0, 2.5, 4.0);
    wind.setSeed(12345);
    options.setWindModelType(WindModelType::MULTI_LEVEL);
    options.setLaunchAltitude(400);
    const ChangeCounter      events(options.changed());
    const SimulationOptions& constOptions = options;

    const double direction = constOptions.getLaunchRodDirection();
    EXPECT_EQ(direction, QtRocket::MathUtil::reduce2Pi(wind.getWindDirection(0, 400)));
    EXPECT_EQ(constOptions.getLaunchRodDirection(), direction);
    // However far the model's own random sources have run.
    static_cast<void>(wind.getWindVelocity(7.0, 400));
    EXPECT_EQ(constOptions.getLaunchRodDirection(), direction);
    // The turbulence is part of it: other seeds, another direction.
    wind.setSeed(54321);
    EXPECT_NE(constOptions.getLaunchRodDirection(), direction);
    EXPECT_EQ(constOptions.getLaunchRodDirection(),
              QtRocket::MathUtil::reduce2Pi(wind.getWindDirection(0, 400)));
    // Asking announces nothing.
    EXPECT_EQ(events.count(), 0);
}

TEST(SimulationOptions, GetWindModelFollowsTheType)
{
    SimulationOptions        options;
    const SimulationOptions& constOptions = options;
    EXPECT_EQ(&options.getWindModel(), &options.getAverageWindModel());
    EXPECT_EQ(&constOptions.getWindModel(), &constOptions.getAverageWindModel());
    options.setWindModelType(WindModelType::MULTI_LEVEL);
    EXPECT_EQ(&options.getWindModel(), &options.getMultiLevelWindModel());
    EXPECT_EQ(&constOptions.getWindModel(), &constOptions.getMultiLevelWindModel());
    EXPECT_EQ(&constOptions.getAverageWindModel(), &options.getAverageWindModel());
    EXPECT_EQ(&constOptions.getMultiLevelWindModel(), &options.getMultiLevelWindModel());
}

TEST(SimulationOptions, SetWindModelType)
{
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    options.setWindModelType(WindModelType::AVERAGE);
    EXPECT_EQ(events.count(), 0);
    options.setWindModelType(WindModelType::MULTI_LEVEL);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(options.getWindModelType(), WindModelType::MULTI_LEVEL);
    options.setWindModelType(WindModelType::AVERAGE);
    EXPECT_EQ(events.count(), 2);
}

/// Changes the average wind of @p options and returns how many events the options had emitted
/// by the time a listener of the wind model itself, connected now, was called.
[[nodiscard]] int optionsEventsWhenAWindListenerRuns(SimulationOptions& options)
{
    int                                        seen = -1;
    const ChangeCounter                        events(options.changed());
    const QtRocket::Signal<>::ScopedConnection connection{
        options.getAverageWindModel().changed().connect(
            [&seen, &events] { seen = events.count(); })};
    options.getAverageWindModel().setDirection(options.getAverageWindModel().getDirection() + 0.5);
    return seen;
}

TEST(SimulationOptions, WindModelChangesAreForwarded)
{
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    options.getAverageWindModel().setAverage(3.0);
    EXPECT_EQ(events.count(), 1);
    options.getAverageWindModel().setAverage(3.0);
    EXPECT_EQ(events.count(), 1);
    options.getAverageWindModel().setStandardDeviation(0.5);
    EXPECT_EQ(events.count(), 2);
    options.getMultiLevelWindModel().getLevels()[0]->setSpeed(4.0);
    // The level reports its deviation (rescaled with the speed) and its speed.
    EXPECT_EQ(events.count(), 4);
    options.getMultiLevelWindModel().setAltitudeReference(WindModel::AltitudeReference::AGL);
    EXPECT_EQ(events.count(), 5);
    // The options' own listeners hear a wind model's change before the model's later ones.
    EXPECT_EQ(optionsEventsWhenAWindListenerRuns(options), 1);
}

TEST(SimulationOptions, DeprecatedWindAccessorsSwitchToTheAverageModel)
{
    SimulationOptions options;
    options.getAverageWindModel().setDirection(2.0);
    options.setWindModelType(WindModelType::MULTI_LEVEL);
    ChangeCounter events(options.changed());

    // Even a getter switches the type.
    EXPECT_EQ(options.getWindSpeedAverage(), 0.0);
    EXPECT_EQ(options.getWindModelType(), WindModelType::AVERAGE);
    EXPECT_EQ(events.count(), 1);

    options.setWindModelType(WindModelType::MULTI_LEVEL);
    options.setWindSpeedAverage(4.0);
    EXPECT_EQ(options.getAverageWindModel().getAverage(), 4.0);
    EXPECT_EQ(options.getAverageWindModel().getStandardDeviation(), 0.0);
    EXPECT_EQ(options.getWindModelType(), WindModelType::AVERAGE);
    EXPECT_EQ(events.count(), 4);

    options.setWindSpeedDeviation(1.0);
    EXPECT_EQ(options.getWindSpeedDeviation(), 1.0);
    EXPECT_EQ(events.count(), 5);

    options.setWindTurbulenceIntensity(0.5);
    EXPECT_EQ(options.getWindTurbulenceIntensity(), 0.5);
    EXPECT_EQ(options.getWindSpeedDeviation(), 2.0);
    EXPECT_EQ(events.count(), 6);

    options.setWindDirection(3.0);
    EXPECT_EQ(options.getWindDirection(), 3.0);
    EXPECT_EQ(events.count(), 7);
    EXPECT_EQ(&options.getWindModel(), &options.getAverageWindModel());
}

TEST(SimulationOptions, EveryDeprecatedWindAccessorSwitchesTheType)
{
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    options.setWindModelType(WindModelType::MULTI_LEVEL);
    EXPECT_EQ(options.getWindSpeedDeviation(), 0.0);
    EXPECT_EQ(options.getWindModelType(), WindModelType::AVERAGE);
    options.setWindModelType(WindModelType::MULTI_LEVEL);
    EXPECT_EQ(options.getWindTurbulenceIntensity(), 0.0);
    EXPECT_EQ(options.getWindModelType(), WindModelType::AVERAGE);
    options.setWindModelType(WindModelType::MULTI_LEVEL);
    EXPECT_EQ(options.getWindDirection(), kPi / 2);
    EXPECT_EQ(options.getWindModelType(), WindModelType::AVERAGE);
    options.setWindModelType(WindModelType::MULTI_LEVEL);
    options.setWindSpeedDeviation(0.0);
    EXPECT_EQ(options.getWindModelType(), WindModelType::AVERAGE);
    options.setWindModelType(WindModelType::MULTI_LEVEL);
    options.setWindTurbulenceIntensity(0.0);
    EXPECT_EQ(options.getWindModelType(), WindModelType::AVERAGE);
    options.setWindModelType(WindModelType::MULTI_LEVEL);
    options.setWindDirection(kPi / 2);
    EXPECT_EQ(options.getWindModelType(), WindModelType::AVERAGE);
    // Six switches to MULTI_LEVEL and six back; the values themselves never changed.
    EXPECT_EQ(events.count(), 12);
}

// ------------------------------------------------------------------- gravity and geodesy

TEST(SimulationOptions, SetGravityModelTypeAndConstantGravity)
{
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    options.setGravityModelType(GravityModelType::WGS);
    options.setConstantGravity(9.807);
    EXPECT_EQ(events.count(), 0);
    options.setGravityModelType(GravityModelType::CONSTANT);
    options.setConstantGravity(3.71);
    EXPECT_EQ(events.count(), 2);
    EXPECT_EQ(options.getGravityModelType(), GravityModelType::CONSTANT);
    EXPECT_EQ(options.getConstantGravity(), 3.71);
}

TEST(SimulationOptions, SetGeodeticComputation)
{
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    options.setGeodeticComputation(GeodeticComputationStrategy::SPHERICAL);
    EXPECT_EQ(events.count(), 0);
    options.setGeodeticComputation(GeodeticComputationStrategy::WGS84);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(options.getGeodeticComputation(), GeodeticComputationStrategy::WGS84);
    options.setGeodeticComputation(GeodeticComputationStrategy::FLAT);
    EXPECT_EQ(events.count(), 2);
    EXPECT_EQ(options.getGeodeticComputation(), GeodeticComputationStrategy::FLAT);
}

TEST(SimulationOptions, SetLaunchLatitudeAndLongitudeClamp)
{
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    options.setLaunchLatitude(28.61);
    options.setLaunchLongitude(-80.6);
    EXPECT_EQ(events.count(), 0);
    options.setLaunchLatitude(100);
    options.setLaunchLongitude(-200);
    EXPECT_EQ(events.count(), 2);
    EXPECT_EQ(options.getLaunchLatitude(), 90.0);
    EXPECT_EQ(options.getLaunchLongitude(), -180.0);
    options.setLaunchLatitude(95);
    options.setLaunchLongitude(-190);
    EXPECT_EQ(events.count(), 2);
    options.setLaunchLatitude(-91);
    options.setLaunchLongitude(181);
    EXPECT_EQ(events.count(), 4);
    EXPECT_EQ(options.getLaunchLatitude(), -90.0);
    EXPECT_EQ(options.getLaunchLongitude(), 180.0);
    options.setLaunchLatitude(kNaN);
    options.setLaunchLongitude(kNaN);
    EXPECT_EQ(events.count(), 6);
    EXPECT_TRUE(std::isnan(options.getLaunchLatitude()));
    EXPECT_TRUE(std::isnan(options.getLaunchLongitude()));
}

// ---------------------------------------------------------------------------- atmosphere

TEST(SimulationOptions, SetLaunchAltitudeUpdatesTheIsaConditions)
{
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    options.setLaunchAltitude(0);
    EXPECT_EQ(events.count(), 0);
    // The temperature, the pressure (the humidity stays 0) and the altitude itself.
    options.setLaunchAltitude(100);
    EXPECT_EQ(events.count(), 3);
    EXPECT_EQ(options.getLaunchAltitude(), 100.0);
    EXPECT_TRUE(isJavaValue(kIsaTemperature100, options.getLaunchTemperature()));
    EXPECT_TRUE(isJavaValue(kIsaPressure100, options.getLaunchPressure()));
    EXPECT_EQ(options.getLaunchRelativeHumidity(), 0.0);
    events.reset();
    options.setLaunchAltitude(1234.5);
    EXPECT_EQ(events.count(), 3);
    EXPECT_TRUE(isJavaValue(kIsaTemperature1234, options.getLaunchTemperature()));
    EXPECT_TRUE(isJavaValue(kIsaPressure1234, options.getLaunchPressure()));
}

TEST(SimulationOptions, SetLaunchAltitudeClampsToTheIsaModelsLimit)
{
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    options.setLaunchAltitude(20000);
    EXPECT_EQ(events.count(), 3);
    EXPECT_EQ(options.getLaunchAltitude(), kMaximumAltitude);
    EXPECT_TRUE(isJavaValue(kIsaTemperatureTop, options.getLaunchTemperature()));
    EXPECT_TRUE(isJavaValue(kIsaPressureTop, options.getLaunchPressure()));
    // The argument is compared before it is clamped: the same call again changes nothing and
    // still announces a change.
    events.reset();
    options.setLaunchAltitude(20000);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(options.getLaunchAltitude(), kMaximumAltitude);
}

TEST(SimulationOptions, SetLaunchAltitudeBelowSeaLevelAndNaN)
{
    SimulationOptions options;
    options.setLaunchAltitude(100);
    ChangeCounter events(options.changed());
    // Below sea level the ISA table gives its sea level conditions.
    options.setLaunchAltitude(-50);
    EXPECT_EQ(events.count(), 3);
    EXPECT_EQ(options.getLaunchAltitude(), -50.0);
    EXPECT_TRUE(isJavaValue(288.15, options.getLaunchTemperature()));
    EXPECT_TRUE(isJavaValue(101325.0, options.getLaunchPressure()));
    // MathUtil.min() skips a NaN: the altitude becomes the limit.
    events.reset();
    options.setLaunchAltitude(kNaN);
    EXPECT_EQ(events.count(), 3);
    EXPECT_EQ(options.getLaunchAltitude(), kMaximumAltitude);
    EXPECT_TRUE(isJavaValue(kIsaTemperatureTop, options.getLaunchTemperature()));
    EXPECT_TRUE(isJavaValue(kIsaPressureTop, options.getLaunchPressure()));
}

TEST(SimulationOptions, SetIsaAtmosphere)
{
    SimulationOptions options;
    options.setLaunchAltitude(100);
    ChangeCounter events(options.changed());
    options.setIsaAtmosphere(true);
    EXPECT_EQ(events.count(), 0);
    // Switching the ISA off keeps the conditions.
    options.setIsaAtmosphere(false);
    EXPECT_EQ(events.count(), 1);
    EXPECT_FALSE(options.isIsaAtmosphere());
    EXPECT_TRUE(isJavaValue(kIsaTemperature100, options.getLaunchTemperature()));
    EXPECT_TRUE(isJavaValue(kIsaPressure100, options.getLaunchPressure()));

    events.reset();
    options.setLaunchTemperature(300);
    options.setLaunchPressure(90000);
    options.setLaunchRelativeHumidity(0.5);
    EXPECT_EQ(events.count(), 3);
    options.setLaunchTemperature(300);
    options.setLaunchPressure(90000);
    options.setLaunchRelativeHumidity(0.5);
    EXPECT_EQ(events.count(), 3);

    // Without the ISA the altitude leaves the conditions alone.
    events.reset();
    options.setLaunchAltitude(200);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(options.getLaunchTemperature(), 300.0);
    EXPECT_EQ(options.getLaunchPressure(), 90000.0);
    EXPECT_EQ(options.getLaunchRelativeHumidity(), 0.5);

    // Switching it on sets the three conditions, each announcing its change, and then itself.
    events.reset();
    options.setIsaAtmosphere(true);
    EXPECT_EQ(events.count(), 4);
    EXPECT_TRUE(options.isIsaAtmosphere());
    EXPECT_TRUE(isJavaValue(kIsaTemperature200, options.getLaunchTemperature()));
    EXPECT_TRUE(isJavaValue(kIsaPressure200, options.getLaunchPressure()));
    EXPECT_EQ(options.getLaunchRelativeHumidity(), 0.0);
}

TEST(SimulationOptions, SwitchingTheIsaOffKeepsConditionsThatAreNotTheIsaOnes)
{
    // The .ork reader stores the conditions of a file while the ISA is still on and only then
    // switches it off (AtmosphereHandler.storeSettings()); FixProbe: 1 event, 300 K, 90000 Pa, 0.5.
    SimulationOptions options;
    options.setLaunchTemperature(300);
    options.setLaunchPressure(90000);
    options.setLaunchRelativeHumidity(0.5);
    const ChangeCounter events(options.changed());
    options.setIsaAtmosphere(false);
    EXPECT_EQ(events.count(), 1);
    EXPECT_FALSE(options.isIsaAtmosphere());
    EXPECT_EQ(options.getLaunchTemperature(), 300.0);
    EXPECT_EQ(options.getLaunchPressure(), 90000.0);
    EXPECT_EQ(options.getLaunchRelativeHumidity(), 0.5);
}

TEST(SimulationOptions, TheConditionsCanBeSetWhileTheIsaIsInUse)
{
    // Nothing ties them to the ISA until the altitude or the flag changes again.
    SimulationOptions options;
    options.setLaunchTemperature(300);
    EXPECT_TRUE(options.isIsaAtmosphere());
    EXPECT_EQ(options.getLaunchTemperature(), 300.0);
    options.setLaunchAltitude(100);
    EXPECT_TRUE(isJavaValue(kIsaTemperature100, options.getLaunchTemperature()));
}

/// Sets the launch altitude of @p options to 100 m while a listener moves the launch site to
/// 200 m on the first change it hears.
void setAltitudeWhileAListenerMovesTheSite(SimulationOptions& options)
{
    bool                                       moved = false;
    const QtRocket::Signal<>::ScopedConnection connection{
        options.changed().connect([&options, &moved] {
            if (!moved)
            {
                moved = true;
                options.setIsaAtmosphere(false);
                options.setLaunchAltitude(200);
                options.setIsaAtmosphere(true);
            }
        })};
    options.setLaunchAltitude(100);
}

TEST(SimulationOptions, AListenerMovingTheLaunchSiteDuringTheIsaUpdate)
{
    // Java looks the ISA up once per value, at the altitude of that moment: the pressure and
    // the humidity that follow the listener's move are those of the new altitude.
    SimulationOptions options;
    setAltitudeWhileAListenerMovesTheSite(options);
    EXPECT_EQ(options.getLaunchAltitude(), 200.0);
    EXPECT_TRUE(isJavaValue(kIsaTemperature200, options.getLaunchTemperature()));
    EXPECT_TRUE(isJavaValue(kIsaPressure200, options.getLaunchPressure()));
}

TEST(SimulationOptions, IsaAtmosphericModelIsShared)
{
    const SimulationOptions                               first;
    const SimulationOptions                               second;
    const Result<std::shared_ptr<const AtmosphericModel>> a = first.getAtmosphericModel();
    const Result<std::shared_ptr<const AtmosphericModel>> b = second.getAtmosphericModel();
    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    ASSERT_NE(*a, nullptr);
    EXPECT_EQ(*a, *b);
    EXPECT_TRUE(isJavaValue(kIsaTemperature100, (*a)->getConditions(100).getTemperature()));
    EXPECT_TRUE(isJavaValue(kIsaPressure100, (*a)->getConditions(100).getPressure()));
}

TEST(SimulationOptions, CustomAtmosphericModelIsFittedToTheLaunchConditions)
{
    SimulationOptions options;
    options.setIsaAtmosphere(false);
    options.setLaunchAltitude(500);
    options.setLaunchTemperature(300);
    options.setLaunchPressure(95000);
    options.setLaunchRelativeHumidity(0.5);
    const Result<std::shared_ptr<const AtmosphericModel>> model = options.getAtmosphericModel();
    ASSERT_TRUE(model.has_value());
    ASSERT_NE(*model, nullptr);
    // 500 m is a table altitude of the model, which holds the launch conditions there.
    const QtRocket::AtmosphericConditions conditions = (*model)->getConditions(500);
    EXPECT_NEAR(conditions.getTemperature(), 300.0, 1e-9);
    EXPECT_NEAR(conditions.getPressure(), 95000.0, 1e-6);
    EXPECT_DOUBLE_EQ(conditions.getRelativeHumidity(), 0.5);
    // A new model every time, and not the shared ISA model.
    const Result<std::shared_ptr<const AtmosphericModel>> again = options.getAtmosphericModel();
    ASSERT_TRUE(again.has_value());
    EXPECT_NE(*model, *again);
    const Result<std::shared_ptr<const AtmosphericModel>> isa =
        SimulationOptions().getAtmosphericModel();
    ASSERT_TRUE(isa.has_value());
    EXPECT_NE(*model, *isa);
}

TEST(SimulationOptions, AtmosphericModelRefusesImpossibleLaunchConditions)
{
    SimulationOptions options;
    options.setIsaAtmosphere(false);
    options.setLaunchTemperature(-5);
    const Result<std::shared_ptr<const AtmosphericModel>> model = options.getAtmosphericModel();
    ASSERT_FALSE(model.has_value());
    EXPECT_EQ(model.error().code, ErrorCode::INVALID_ARGUMENT);
    // The ISA does not look at the launch conditions.
    options.setIsaAtmosphere(true);
    options.setLaunchTemperature(-5);
    EXPECT_TRUE(options.getAtmosphericModel().has_value());
}

// ------------------------------------------------------------------- stepper and limits

TEST(SimulationOptions, SetSimulationStepperMethodChoiceAlwaysAnnouncesAndStores)
{
    InMemoryPreferences preferences;
    SimulationOptions   options(preferences);
    ChangeCounter       events(options.changed());
    EXPECT_EQ(preferences.getSimulationStepperMethodName(), "RK4");
    EXPECT_TRUE(preferences.empty());

    options.setSimulationStepperMethodChoice(SimulationStepperMethod::RK4);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(preferences.get(Keys::kSimulationStepperMethod), "RK4");

    options.setSimulationStepperMethodChoice(SimulationStepperMethod::RK6);
    EXPECT_EQ(events.count(), 2);
    EXPECT_EQ(options.getSimulationStepperMethodChoice(), SimulationStepperMethod::RK6);
    EXPECT_EQ(preferences.getSimulationStepperMethodName(), "RK6");

    // The stored choice is not what new options start from.
    const SimulationOptions later(preferences);
    EXPECT_EQ(later.getSimulationStepperMethodChoice(), SimulationStepperMethod::RK4);
}

TEST(SimulationOptions, StepperChoiceWithoutPreferences)
{
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    options.setSimulationStepperMethodChoice(SimulationStepperMethod::RK6);
    options.setSimulationStepperMethodChoice(SimulationStepperMethod::RK6);
    EXPECT_EQ(events.count(), 2);
    EXPECT_EQ(options.getSimulationStepperMethodChoice(), SimulationStepperMethod::RK6);
}

TEST(SimulationOptions, SetTimeStepAndMaximumTime)
{
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    options.setTimeStep(0.05);
    options.setMaxSimulationTime(1200);
    EXPECT_EQ(events.count(), 0);
    options.setTimeStep(0.01);
    options.setMaxSimulationTime(600);
    EXPECT_EQ(events.count(), 2);
    EXPECT_EQ(options.getTimeStep(), 0.01);
    EXPECT_EQ(options.getMaxSimulationTime(), 600.0);
    // Nothing is validated.
    options.setTimeStep(-1);
    options.setMaxSimulationTime(0);
    EXPECT_EQ(events.count(), 4);
    EXPECT_EQ(options.getTimeStep(), -1.0);
    EXPECT_EQ(options.getMaxSimulationTime(), 0.0);
}

TEST(SimulationOptions, SetMaximumStepAngleClamps)
{
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    options.setMaximumStepAngle(3 * kPi / 180);
    EXPECT_EQ(events.count(), 0);
    options.setMaximumStepAngle(0);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(options.getMaximumStepAngle(), 0.017453292519943295);
    options.setMaximumStepAngle(-5);
    EXPECT_EQ(events.count(), 1);
    options.setMaximumStepAngle(1);
    EXPECT_EQ(events.count(), 2);
    EXPECT_EQ(options.getMaximumStepAngle(), 0.3490658503988659);
    options.setMaximumStepAngle(0.1);
    EXPECT_EQ(events.count(), 3);
    EXPECT_EQ(options.getMaximumStepAngle(), 0.1);
    options.setMaximumStepAngle(kNaN);
    EXPECT_EQ(events.count(), 4);
    EXPECT_TRUE(std::isnan(options.getMaximumStepAngle()));
}

TEST(SimulationOptions, SetRecoveryThresholds)
{
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    options.setRecoverySpeedWarning(20.0);
    options.setDrogueLowSpeedWarning(3.048);
    options.setRecoveryDrogueMainHighSpeedWarning(30.48);
    options.setRecoveryDrogueMainLowSpeedWarning(15.24);
    EXPECT_EQ(events.count(), 0);
    options.setRecoverySpeedWarning(21.0);
    options.setDrogueLowSpeedWarning(4.0);
    options.setRecoveryDrogueMainHighSpeedWarning(31.0);
    options.setRecoveryDrogueMainLowSpeedWarning(16.0);
    EXPECT_EQ(events.count(), 4);
    EXPECT_EQ(options.getRecoverySpeedWarning(), 21.0);
    EXPECT_EQ(options.getDrogueLowSpeedWarning(), 4.0);
    EXPECT_EQ(options.getRecoveryDrogueMainHighSpeedWarning(), 31.0);
    EXPECT_EQ(options.getRecoveryDrogueMainLowSpeedWarning(), 16.0);
}

TEST(SimulationOptions, EveryPlainSetterComparesWithMathUtilEquals)
{
    EXPECT_EQ(
        plainSetter(&SimulationOptions::getLaunchRodLength, &SimulationOptions::setLaunchRodLength),
        kPlainSetter);
    EXPECT_EQ(
        plainSetter(&SimulationOptions::getConstantGravity, &SimulationOptions::setConstantGravity),
        kPlainSetter);
    EXPECT_EQ(plainSetter(&SimulationOptions::getLaunchTemperature,
                          &SimulationOptions::setLaunchTemperature),
              kPlainSetter);
    EXPECT_EQ(
        plainSetter(&SimulationOptions::getLaunchPressure, &SimulationOptions::setLaunchPressure),
        kPlainSetter);
    EXPECT_EQ(plainSetter(&SimulationOptions::getLaunchRelativeHumidity,
                          &SimulationOptions::setLaunchRelativeHumidity),
              kPlainSetter);
    EXPECT_EQ(plainSetter(&SimulationOptions::getTimeStep, &SimulationOptions::setTimeStep),
              kPlainSetter);
    EXPECT_EQ(plainSetter(&SimulationOptions::getMaxSimulationTime,
                          &SimulationOptions::setMaxSimulationTime),
              kPlainSetter);
}

TEST(SimulationOptions, EveryThresholdSetterComparesWithMathUtilEquals)
{
    EXPECT_EQ(plainSetter(&SimulationOptions::getRecoverySpeedWarning,
                          &SimulationOptions::setRecoverySpeedWarning),
              kPlainSetter);
    EXPECT_EQ(plainSetter(&SimulationOptions::getDrogueLowSpeedWarning,
                          &SimulationOptions::setDrogueLowSpeedWarning),
              kPlainSetter);
    EXPECT_EQ(plainSetter(&SimulationOptions::getRecoveryDrogueMainHighSpeedWarning,
                          &SimulationOptions::setRecoveryDrogueMainHighSpeedWarning),
              kPlainSetter);
    EXPECT_EQ(plainSetter(&SimulationOptions::getRecoveryDrogueMainLowSpeedWarning,
                          &SimulationOptions::setRecoveryDrogueMainLowSpeedWarning),
              kPlainSetter);
}

TEST(SimulationOptions, TheClampingSettersCompareWithMathUtilEquals)
{
    // The setters that clamp or reduce their argument compare what is left with the stored
    // value as the plain ones do (FixProbe: no event within the tolerance, six outside it).
    SimulationOptions options;
    options.setIsaAtmosphere(false);
    options.setLaunchIntoWind(false);
    options.setLaunchAltitude(100);
    options.setLaunchLatitude(45);
    options.setLaunchLongitude(10);
    options.setLaunchRodAngle(0.5);
    options.setLaunchRodDirection(1.0);
    options.setMaximumStepAngle(0.1);
    const ChangeCounter events(options.changed());

    options.setLaunchAltitude(100 * (1 + 1e-10));
    options.setLaunchLatitude(45 * (1 + 1e-10));
    options.setLaunchLongitude(10 * (1 + 1e-10));
    options.setLaunchRodAngle(0.5 * (1 + 1e-10));
    options.setLaunchRodDirection(1.0 + 1e-10);
    options.setMaximumStepAngle(0.1 * (1 + 1e-10));
    EXPECT_EQ(events.count(), 0);
    EXPECT_EQ(options.getLaunchAltitude(), 100.0);
    EXPECT_EQ(options.getLaunchLatitude(), 45.0);
    EXPECT_EQ(options.getLaunchLongitude(), 10.0);
    EXPECT_EQ(options.getLaunchRodAngle(), 0.5);
    EXPECT_EQ(options.getLaunchRodDirection(), 1.0);
    EXPECT_EQ(options.getMaximumStepAngle(), 0.1);

    options.setLaunchAltitude(100 * (1 + 1e-7));
    options.setLaunchLatitude(45 * (1 + 1e-7));
    options.setLaunchLongitude(10 * (1 + 1e-7));
    options.setLaunchRodAngle(0.5 * (1 + 1e-7));
    options.setLaunchRodDirection(1.0 + 1e-7);
    options.setMaximumStepAngle(0.1 * (1 + 1e-7));
    EXPECT_EQ(events.count(), 6);
    EXPECT_EQ(options.getLaunchAltitude(), 100 * (1 + 1e-7));
    EXPECT_EQ(options.getLaunchLatitude(), 45 * (1 + 1e-7));
    EXPECT_EQ(options.getLaunchLongitude(), 10 * (1 + 1e-7));
    EXPECT_EQ(options.getLaunchRodAngle(), 0.5 * (1 + 1e-7));
    EXPECT_EQ(options.getLaunchRodDirection(), 1.0 + 1e-7);
    EXPECT_EQ(options.getMaximumStepAngle(), 0.1 * (1 + 1e-7));
}

// --------------------------------------------------------------------------- random seed

TEST(SimulationOptions, SetRandomSeedAnnouncesOnlyAFixedSeed)
{
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    options.setRandomSeed(options.getRandomSeed());
    EXPECT_EQ(events.count(), 0);
    options.setRandomSeed(otherSeed(options.getRandomSeed()));
    EXPECT_EQ(events.count(), 0);
    options.setRandomSeedFixed(false);
    EXPECT_EQ(events.count(), 0);
    options.setRandomSeedFixed(true);
    EXPECT_EQ(events.count(), 1);
    EXPECT_TRUE(options.isRandomSeedFixed());
    options.setRandomSeed(otherSeed(options.getRandomSeed()));
    EXPECT_EQ(events.count(), 2);
    options.setRandomSeed(options.getRandomSeed());
    EXPECT_EQ(events.count(), 2);
    const int before = options.getRandomSeed();
    options.randomizeSeedIfNotFixed();
    EXPECT_EQ(events.count(), 2);
    EXPECT_EQ(options.getRandomSeed(), before);
}

TEST(SimulationOptions, SetRandomSeedTakesEveryInt)
{
    SimulationOptions options;
    options.setRandomSeed(std::numeric_limits<int>::min());
    EXPECT_EQ(options.getRandomSeed(), std::numeric_limits<int>::min());
    options.setRandomSeed(std::numeric_limits<int>::max());
    EXPECT_EQ(options.getRandomSeed(), std::numeric_limits<int>::max());
    options.setRandomSeed(-987654321);
    EXPECT_EQ(options.getRandomSeed(), -987654321);
}

/// The seeds of eight randomizeSeed() calls.
[[nodiscard]] std::set<int> randomizedSeeds(SimulationOptions& options)
{
    std::set<int> seeds;
    for (int i = 0; i < 8; ++i)
    {
        options.randomizeSeed();
        seeds.insert(options.getRandomSeed());
    }
    return seeds;
}

TEST(SimulationOptions, RandomizeSeedDrawsNewSeeds)
{
    // Eight draws of 32 random bits are not all the same (but for one chance in 2^224).
    SimulationOptions options;
    ChangeCounter     events(options.changed());
    EXPECT_GT(randomizedSeeds(options).size(), 1U);
    // A generated seed is no change of the conditions.
    EXPECT_EQ(events.count(), 0);
    // randomizeSeed() itself does not look at the flag, and a fixed seed that changes is
    // announced.
    options.setRandomSeedFixed(true);
    events.reset();
    EXPECT_GT(randomizedSeeds(options).size(), 1U);
    EXPECT_GT(events.count(), 0);
}

TEST(SimulationOptions, RandomizeSeedIfNotFixedDrawsWhileNotFixed)
{
    SimulationOptions options;
    std::set<int>     seeds;
    options.randomizeSeedIfNotFixed();
    seeds.insert(options.getRandomSeed());
    options.randomizeSeedIfNotFixed();
    seeds.insert(options.getRandomSeed());
    options.randomizeSeedIfNotFixed();
    seeds.insert(options.getRandomSeed());
    options.randomizeSeedIfNotFixed();
    seeds.insert(options.getRandomSeed());
    EXPECT_GT(seeds.size(), 1U);
}

TEST(SimulationOptions, TheAverageWindModelStartsWithTheOptionsSeed)
{
    // Java: averageWindModel = new PinkNoiseWindModel(randomSeed). (The models compare with
    // their seeds.)
    const SimulationOptions options;
    EXPECT_TRUE(options.getAverageWindModel() == PinkNoiseWindModel(options.getRandomSeed()));
    EXPECT_FALSE(options.getAverageWindModel() ==
                 PinkNoiseWindModel(otherSeed(options.getRandomSeed())));

    InMemoryPreferences preferences;
    storeEverySimulationKey(preferences);
    const SimulationOptions fromStore(preferences);
    // Not the stored seed either: that one is the factory's to apply.
    EXPECT_TRUE(fromStore.getAverageWindModel() == PinkNoiseWindModel(fromStore.getRandomSeed()));
}

TEST(SimulationOptions, TheSeedDoesNotReachTheConfiguredWindModel)
{
    // The average wind model keeps the seed the options were constructed with; the simulation
    // seeds a clone of it (toSimulationConditions()), so that the seed governs the run without
    // becoming part of the configuration's identity.
    SimulationOptions        options;
    const PinkNoiseWindModel before(options.getAverageWindModel());
    options.setRandomSeed(otherSeed(options.getRandomSeed()));
    EXPECT_TRUE(options.getAverageWindModel() == before);
}

// ------------------------------------------------------------------------- lookup tables

TEST(SimulationOptions, SetDragLookup)
{
    SimulationOptions                          options;
    ChangeCounter                              events(options.changed());
    const std::vector<std::string>             rows  = dragRows("2");
    const std::shared_ptr<const MachAoALookup> table = parseTable(rows, dragColumns());

    // A table without a path is fine.
    options.setDragLookup(std::nullopt, table);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(options.getDragLookupCsvPath(), std::nullopt);
    EXPECT_TRUE(options.hasDragLookup());
    EXPECT_EQ(options.getDragLookupTable(), table);
    EXPECT_EQ(options.getDragLookupCsvRows(), std::nullopt);
    options.setDragLookup(std::nullopt, table);
    EXPECT_EQ(events.count(), 1);

    // The rows alone are stored without an event.
    options.setDragLookup(std::nullopt, table, rows);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(options.getDragLookupCsvRows(), rows);

    // The path is made absolute and normalised.
    const std::filesystem::path expected =
        (std::filesystem::current_path() / "b.csv").lexically_normal();
    options.setDragLookup(std::filesystem::path("a") / ".." / "b.csv", table, rows);
    EXPECT_EQ(events.count(), 2);
    EXPECT_EQ(options.getDragLookupCsvPath(), expected);

    // The same path written differently is no change; the two-argument form drops the rows.
    options.setDragLookup(std::filesystem::path("b.csv"), table);
    EXPECT_EQ(events.count(), 2);
    EXPECT_EQ(options.getDragLookupCsvPath(), expected);
    EXPECT_EQ(options.getDragLookupCsvRows(), std::nullopt);
}

TEST(SimulationOptions, SetDragLookupWithAPathNeedsATable)
{
    SimulationOptions                          options;
    const std::vector<std::string>             rows  = dragRows("2");
    const std::shared_ptr<const MachAoALookup> table = parseTable(rows, dragColumns());
    options.setDragLookup(std::filesystem::path("b.csv"), table);
    ChangeCounter events(options.changed());

    // Java: IllegalArgumentException, thrown before anything is stored.
    EXPECT_THROW(options.setDragLookup(std::filesystem::path("b.csv"), nullptr, rows), BugError);
    EXPECT_EQ(events.count(), 0);
    EXPECT_EQ(options.getDragLookupCsvRows(), std::nullopt);
    EXPECT_EQ(options.getDragLookupTable(), table);

    // Neither a path nor a table: the lookup is removed, but the rows given are kept.
    options.setDragLookup(std::nullopt, nullptr, rows);
    EXPECT_EQ(events.count(), 1);
    EXPECT_FALSE(options.hasDragLookup());
    EXPECT_EQ(options.getDragLookupCsvPath(), std::nullopt);
    EXPECT_EQ(options.getDragLookupCsvRows(), rows);

    // Nothing but the rows is left to clear, so there is no event.
    options.clearDragLookup();
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(options.getDragLookupCsvRows(), std::nullopt);
    options.clearDragLookup();
    EXPECT_EQ(events.count(), 1);
}

TEST(SimulationOptions, ClearDragLookup)
{
    SimulationOptions                          options;
    const std::vector<std::string>             rows  = dragRows("2");
    const std::shared_ptr<const MachAoALookup> table = parseTable(rows, dragColumns());
    options.setDragLookup(lookupCsv(), table, rows);
    ChangeCounter events(options.changed());
    options.clearDragLookup();
    EXPECT_EQ(events.count(), 1);
    EXPECT_FALSE(options.hasDragLookup());
    EXPECT_EQ(options.getDragLookupTable(), nullptr);
    EXPECT_EQ(options.getDragLookupCsvPath(), std::nullopt);
    EXPECT_EQ(options.getDragLookupCsvRows(), std::nullopt);
}

TEST(SimulationOptions, TheLookupPathHasNoTrailingSeparatorAndAnEmptyPathIsTheCurrentDirectory)
{
    SimulationOptions                          options;
    const std::shared_ptr<const MachAoALookup> table = parseTable(dragRows("2"), dragColumns());
    options.setDragLookup(std::filesystem::path("lookup-dir/"), table);
    EXPECT_EQ(options.getDragLookupCsvPath(),
              (std::filesystem::current_path() / "lookup-dir").lexically_normal());
    options.setDragLookup(std::filesystem::path("lookup-dir") / "." / "", table);
    EXPECT_EQ(options.getDragLookupCsvPath(),
              (std::filesystem::current_path() / "lookup-dir").lexically_normal());
    // Java: Path.of("").toAbsolutePath() is the current directory.
    options.setDragLookup(std::filesystem::path(), table);
    ASSERT_TRUE(options.getDragLookupCsvPath().has_value());
    EXPECT_TRUE(std::filesystem::equivalent(
        options.getDragLookupCsvPath().value_or(std::filesystem::path()),
        std::filesystem::current_path()));
}

TEST(SimulationOptions, ARootLookupPathIsTheRootHoweverItIsWritten)
{
    // Java: Path.of("//").toAbsolutePath().normalize() is "/" (FixProbe). The spellings are
    // compared as texts: two paths that differ in the separators of their root compare equal.
    SimulationOptions                          options;
    const std::shared_ptr<const MachAoALookup> table = parseTable(dragRows("2"), dragColumns());
    const std::filesystem::path                root  = std::filesystem::current_path().root_path();
    options.setDragLookup(root, table);
    EXPECT_EQ(options.getDragLookupCsvPath().value_or(std::filesystem::path()).native(),
              root.native());

    std::filesystem::path::string_type doubled = root.native();
    doubled += std::filesystem::path::preferred_separator;
    options.setDragLookup(std::filesystem::path(doubled), table);
    EXPECT_EQ(options.getDragLookupCsvPath().value_or(std::filesystem::path()).native(),
              root.native());

    doubled += std::filesystem::path::preferred_separator;
    options.setStabilityLookup(std::filesystem::path(doubled),
                               parseTable(stabilityRows("2"), stabilityColumns()));
    EXPECT_EQ(options.getStabilityLookupCsvPath().value_or(std::filesystem::path()).native(),
              root.native());
}

TEST(SimulationOptions, GetLookupCsvRowsReturnsACopy)
{
    SimulationOptions              options;
    const std::vector<std::string> rows = dragRows("2");
    options.setDragLookup(lookupCsv(), parseTable(rows, dragColumns()), rows);
    std::vector<std::string> got =
        options.getDragLookupCsvRows().value_or(std::vector<std::string>{});
    EXPECT_EQ(got, rows);
    got.emplace_back("9,9");
    EXPECT_EQ(options.getDragLookupCsvRows(), rows);
}

TEST(SimulationOptions, SetDragLookupCsvPathReadsTheFile)
{
    const QtRocket::Test::TempDir  tempDir;
    const std::filesystem::path    file = tempDir.write("drag.csv", "Mach,Cd\n0,0.5\n1,0.75\n");
    SimulationOptions              options;
    const std::vector<std::string> rows = dragRows("2");
    options.setDragLookup(std::nullopt, nullptr, rows);
    ChangeCounter events(options.changed());

    ASSERT_TRUE(options.setDragLookupCsvPath(tempDir.path() / "sub" / ".." / "drag.csv"));
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(options.getDragLookupCsvPath(), file.lexically_normal());
    ASSERT_TRUE(options.hasDragLookup());
    EXPECT_EQ(options.getDragLookupTable()->interpolate(0.5, 0, "cd"), 0.625);
    // The rows are left as they were.
    EXPECT_EQ(options.getDragLookupCsvRows(), rows);

    // Reading the same file again makes a new table, which is a change.
    const std::shared_ptr<const MachAoALookup> first = options.getDragLookupTable();
    ASSERT_TRUE(options.setDragLookupCsvPath(file));
    EXPECT_EQ(events.count(), 2);
    EXPECT_NE(options.getDragLookupTable(), first);

    // No path removes the path and the table, not the rows.
    ASSERT_TRUE(options.setDragLookupCsvPath(std::nullopt));
    EXPECT_EQ(events.count(), 3);
    EXPECT_FALSE(options.hasDragLookup());
    EXPECT_EQ(options.getDragLookupCsvPath(), std::nullopt);
    EXPECT_EQ(options.getDragLookupCsvRows(), rows);
    ASSERT_TRUE(options.setDragLookupCsvPath(std::nullopt));
    EXPECT_EQ(events.count(), 3);
}

TEST(SimulationOptions, SetDragLookupCsvPathFailsWithoutChangingAnything)
{
    const QtRocket::Test::TempDir              tempDir;
    const std::filesystem::path                missing = tempDir.resolve("does-not-exist.csv");
    SimulationOptions                          options;
    const std::vector<std::string>             rows  = dragRows("2");
    const std::shared_ptr<const MachAoALookup> table = parseTable(rows, dragColumns());
    options.setDragLookup(lookupCsv(), table, rows);
    const std::optional<std::filesystem::path> path = options.getDragLookupCsvPath();
    ChangeCounter                              events(options.changed());

    // Java: UncheckedIOException "Failed to read lookup table from <path>".
    const Result<void> result = options.setDragLookupCsvPath(missing);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::IO);
    EXPECT_TRUE(result.error().message.starts_with("Failed to read lookup table from "))
        << result.error().message;
    EXPECT_EQ(events.count(), 0);
    EXPECT_EQ(options.getDragLookupTable(), table);
    EXPECT_EQ(options.getDragLookupCsvPath(), path);
    EXPECT_EQ(options.getDragLookupCsvRows(), rows);

    // A file without the "cd" column.
    const std::filesystem::path wrong = tempDir.write("wrong.csv", "Mach,Cn\n0,1\n");
    const Result<void>          parse = options.setDragLookupCsvPath(wrong);
    ASSERT_FALSE(parse.has_value());
    EXPECT_EQ(parse.error().code, ErrorCode::PARSE);
    EXPECT_EQ(events.count(), 0);
    EXPECT_EQ(options.getDragLookupTable(), table);
}

TEST(SimulationOptions, SetStabilityLookup)
{
    SimulationOptions                          options;
    ChangeCounter                              events(options.changed());
    const std::vector<std::string>             rows  = stabilityRows("2");
    const std::shared_ptr<const MachAoALookup> table = parseTable(rows, stabilityColumns());

    options.setStabilityLookup(std::nullopt, table);
    EXPECT_EQ(events.count(), 1);
    EXPECT_TRUE(options.hasStabilityLookup());
    EXPECT_EQ(options.getStabilityLookupTable(), table);
    EXPECT_EQ(options.getStabilityLookupCsvRows(), std::nullopt);
    options.setStabilityLookup(std::nullopt, table, rows);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(options.getStabilityLookupCsvRows(), rows);
    options.setStabilityLookup(std::filesystem::path("a") / ".." / "s.csv", table, rows);
    EXPECT_EQ(events.count(), 2);
    EXPECT_EQ(options.getStabilityLookupCsvPath(),
              (std::filesystem::current_path() / "s.csv").lexically_normal());
    options.setStabilityLookup(std::filesystem::path("s.csv"), table);
    EXPECT_EQ(events.count(), 2);
    EXPECT_EQ(options.getStabilityLookupCsvRows(), std::nullopt);

    EXPECT_THROW(options.setStabilityLookup(std::filesystem::path("s.csv"), nullptr, rows),
                 BugError);
    EXPECT_EQ(events.count(), 2);
    EXPECT_EQ(options.getStabilityLookupCsvRows(), std::nullopt);

    options.clearStabilityLookup();
    EXPECT_EQ(events.count(), 3);
    EXPECT_FALSE(options.hasStabilityLookup());
    EXPECT_EQ(options.getStabilityLookupCsvPath(), std::nullopt);
    options.clearStabilityLookup();
    EXPECT_EQ(events.count(), 3);
    // The drag lookup was never touched.
    EXPECT_FALSE(options.hasDragLookup());
}

TEST(SimulationOptions, SetStabilityLookupCsvPath)
{
    const QtRocket::Test::TempDir tempDir;
    const std::filesystem::path   file =
        tempDir.write("stability.csv", "Mach,Cn,Cm,Cp\n0,1,2,0.5\n1,3,4,0.75\n");
    SimulationOptions options;
    ChangeCounter     events(options.changed());

    ASSERT_TRUE(options.setStabilityLookupCsvPath(file));
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(options.getStabilityLookupCsvPath(), file.lexically_normal());
    ASSERT_TRUE(options.hasStabilityLookup());
    EXPECT_EQ(options.getStabilityLookupTable()->interpolate(0.5, 0, "cn"), 2.0);
    EXPECT_EQ(options.getStabilityLookupTable()->interpolate(0.5, 0, "cm"), 3.0);
    EXPECT_EQ(options.getStabilityLookupTable()->interpolate(0.5, 0, "cp"), 0.625);

    // A drag file lacks the stability columns: nothing changes.
    const std::filesystem::path drag   = tempDir.write("drag.csv", "Mach,Cd\n0,0.5\n");
    const Result<void>          result = options.setStabilityLookupCsvPath(drag);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::PARSE);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(options.getStabilityLookupCsvPath(), file.lexically_normal());

    const Result<void> missing = options.setStabilityLookupCsvPath(tempDir.resolve("none.csv"));
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code, ErrorCode::IO);

    ASSERT_TRUE(options.setStabilityLookupCsvPath(std::nullopt));
    EXPECT_EQ(events.count(), 2);
    EXPECT_FALSE(options.hasStabilityLookup());
}

TEST(SimulationOptions, TheStabilityRowsAreClearedOnlyByClearStabilityLookup)
{
    // The saver embeds the rows and the reader prefers them to the file (CsvLookupHandler), so
    // rows that stay or go at the wrong moment would replace the table on the next load.
    // FixProbe: 1 event and the rows kept, 2 and kept, 4 and null.
    const QtRocket::Test::TempDir tempDir;
    const std::filesystem::path   file =
        tempDir.write("stability.csv", "Mach,Cn,Cm,Cp\n0,1,2,0.5\n1,3,4,0.75\n");
    SimulationOptions   options = lookupOptions("2.0");
    const ChangeCounter events(options.changed());

    // Reading a file replaces the path and the table and leaves the rows as they were.
    ASSERT_TRUE(options.setStabilityLookupCsvPath(file));
    EXPECT_EQ(events.count(), 1);
    EXPECT_TRUE(options.hasStabilityLookup());
    EXPECT_EQ(options.getStabilityLookupCsvRows(), stabilityRows("2.0"));

    // So does removing the path, which removes the table.
    ASSERT_TRUE(options.setStabilityLookupCsvPath(std::nullopt));
    EXPECT_EQ(events.count(), 2);
    EXPECT_FALSE(options.hasStabilityLookup());
    EXPECT_EQ(options.getStabilityLookupCsvPath(), std::nullopt);
    EXPECT_EQ(options.getStabilityLookupCsvRows(), stabilityRows("2.0"));

    options.setStabilityLookup(lookupCsv(), parseTable(stabilityRows("2.0"), stabilityColumns()),
                               stabilityRows("2.0"));
    options.clearStabilityLookup();
    EXPECT_EQ(events.count(), 4);
    EXPECT_FALSE(options.hasStabilityLookup());
    EXPECT_EQ(options.getStabilityLookupCsvPath(), std::nullopt);
    EXPECT_EQ(options.getStabilityLookupCsvRows(), std::nullopt);
    // The drag lookup was never touched.
    EXPECT_TRUE(options.hasDragLookup());
    EXPECT_EQ(options.getDragLookupCsvRows(), dragRows("2.0"));
}

TEST(SimulationOptions, ClearDragLookupLeavesTheStabilityLookup)
{
    SimulationOptions   options = lookupOptions("2.0");
    const ChangeCounter events(options.changed());
    options.clearDragLookup();
    EXPECT_EQ(events.count(), 1);
    EXPECT_FALSE(options.hasDragLookup());
    EXPECT_EQ(options.getDragLookupCsvRows(), std::nullopt);
    EXPECT_TRUE(options.hasStabilityLookup());
    EXPECT_TRUE(options.getStabilityLookupCsvPath().has_value());
    EXPECT_EQ(options.getStabilityLookupCsvRows(), stabilityRows("2.0"));
}

// ------------------------------------------------------------- the copy (Java's clone())

TEST(SimulationOptions, CopyTakesEveryValue)
{
    SimulationOptions original;
    populate(original);
    SimulationOptions copy = original;
    EXPECT_EQ(describe(copy), describe(original));
    EXPECT_EQ(copy.getRandomSeed(), original.getRandomSeed());
    EXPECT_TRUE(copy == original);
    // Changing the copy leaves the original alone.
    copy.setLaunchRodLength(9);
    EXPECT_EQ(original.getLaunchRodLength(), 2.5);
    EXPECT_FALSE(copy == original);
}

TEST(SimulationOptions, CopyClonesTheWindModelsWithTheirSeeds)
{
    SimulationOptions original;
    populate(original);
    SimulationOptions copy = original;
    EXPECT_NE(&copy.getAverageWindModel(), &original.getAverageWindModel());
    EXPECT_NE(&copy.getMultiLevelWindModel(), &original.getMultiLevelWindModel());
    // Equal with their seeds.
    EXPECT_TRUE(copy.getAverageWindModel() == original.getAverageWindModel());
    EXPECT_TRUE(copy.getMultiLevelWindModel() == original.getMultiLevelWindModel());
    copy.getAverageWindModel().setAverage(1.0);
    copy.getMultiLevelWindModel().clearLevels();
    EXPECT_EQ(original.getAverageWindModel().getAverage(), 6.0);
    EXPECT_EQ(original.getMultiLevelWindModel().getLevels().size(), 2U);
}

TEST(SimulationOptions, CopySharesTheLookupTablesAndCopiesTheRows)
{
    SimulationOptions original;
    populate(original);
    SimulationOptions copy = original;
    EXPECT_EQ(copy.getDragLookupTable(), original.getDragLookupTable());
    EXPECT_EQ(copy.getStabilityLookupTable(), original.getStabilityLookupTable());
    EXPECT_EQ(copy.getDragLookupCsvPath(), original.getDragLookupCsvPath());
    EXPECT_EQ(copy.getStabilityLookupCsvPath(), original.getStabilityLookupCsvPath());
    EXPECT_EQ(copy.getDragLookupCsvRows(), original.getDragLookupCsvRows());
    EXPECT_EQ(copy.getStabilityLookupCsvRows(), original.getStabilityLookupCsvRows());
    copy.clearDragLookup();
    copy.clearStabilityLookup();
    EXPECT_TRUE(original.hasDragLookup());
    EXPECT_EQ(original.getDragLookupCsvRows(), dragRows("0.5"));
    EXPECT_EQ(original.getStabilityLookupCsvRows(), stabilityRows("0.25"));
}

TEST(SimulationOptions, CopyHasNoConnections)
{
    SimulationOptions   original;
    const ChangeCounter originalEvents(original.changed());
    const ChangeCounter originalWindEvents(original.getAverageWindModel().changed());
    SimulationOptions   copy = original;
    EXPECT_TRUE(copy.changed().empty());
    EXPECT_EQ(original.changed().size(), 1U);
    copy.setLaunchRodLength(4);
    copy.getAverageWindModel().setAverage(4);
    copy.getMultiLevelWindModel().clearLevels();
    EXPECT_EQ(originalEvents.count(), 0);
    EXPECT_EQ(originalWindEvents.count(), 0);

    const ChangeCounter copyEvents(copy.changed());
    original.setLaunchRodLength(5);
    original.getAverageWindModel().setAverage(5);
    EXPECT_EQ(copyEvents.count(), 0);
    EXPECT_EQ(originalEvents.count(), 2);
}

TEST(SimulationOptions, CopyKeepsThePreferences)
{
    InMemoryPreferences preferences;
    SimulationOptions   original(preferences);
    SimulationOptions   copy = original;
    copy.setSimulationStepperMethodChoice(SimulationStepperMethod::RK6);
    EXPECT_EQ(preferences.getSimulationStepperMethodName(), "RK6");
    EXPECT_EQ(original.getSimulationStepperMethodChoice(), SimulationStepperMethod::RK4);
}

// ---------------------------------------------------------------------------------- move

TEST(SimulationOptions, MoveKeepsTheWindModelsAndTheConnections)
{
    SimulationOptions original;
    populate(original);
    const std::string                   values = describe(original);
    const ChangeCounter                 events(original.changed());
    const PinkNoiseWindModel*           average = &original.getAverageWindModel();
    const MultiLevelPinkNoiseWindModel* multi   = &original.getMultiLevelWindModel();
    const ChangeCounter                 averageEvents(original.getAverageWindModel().changed());

    SimulationOptions moved = std::move(original);
    EXPECT_EQ(describe(moved), values);
    EXPECT_EQ(&moved.getAverageWindModel(), average);
    EXPECT_EQ(&moved.getMultiLevelWindModel(), multi);

    moved.setLaunchRodLength(9);
    EXPECT_EQ(events.count(), 1);
    // The wind models still report to the options they belong to.
    moved.getAverageWindModel().setDirection(1.0);
    EXPECT_EQ(averageEvents.count(), 1);
    EXPECT_EQ(events.count(), 2);
    moved.getMultiLevelWindModel().clearLevels();
    EXPECT_EQ(events.count(), 3);
}

TEST(SimulationOptions, MoveAssignmentReplacesTheTarget)
{
    SimulationOptions source;
    populate(source);
    const std::string         values = describe(source);
    const ChangeCounter       sourceEvents(source.changed());
    const PinkNoiseWindModel* average = &source.getAverageWindModel();

    SimulationOptions   target;
    const ChangeCounter targetEvents(target.changed());
    EXPECT_TRUE(targetEvents.connected());
    target = std::move(source);
    EXPECT_EQ(describe(target), values);
    EXPECT_EQ(&target.getAverageWindModel(), average);
    // The target's own connections went with its old state.
    EXPECT_FALSE(targetEvents.connected());
    target.getAverageWindModel().setDirection(1.0);
    target.setLaunchRodLength(9);
    EXPECT_EQ(sourceEvents.count(), 2);
    EXPECT_EQ(targetEvents.count(), 0);
}

TEST(SimulationOptions, ReturningOptionsFromAFunctionKeepsTheForwarding)
{
    SimulationOptions   options = lookupOptions("2.0");
    const ChangeCounter events(options.changed());
    options.getAverageWindModel().setAverage(2.0);
    options.getMultiLevelWindModel().clearLevels();
    EXPECT_EQ(events.count(), 2);
}

// -------------------------------------------------------------------- copyConditionsFrom

/// The options a copyConditionsFrom() case changes one of.
enum class Field
{
    LAUNCH_ROD_LENGTH,
    LAUNCH_INTO_WIND,
    LAUNCH_ROD_ANGLE,
    LAUNCH_ROD_DIRECTION,
    WIND_MODEL_TYPE,
    AVERAGE_WIND,
    MULTI_LEVEL_WIND,
    LAUNCH_ALTITUDE,
    LAUNCH_LATITUDE,
    LAUNCH_LONGITUDE,
    GEODETIC_COMPUTATION,
    USE_ISA,
    LAUNCH_TEMPERATURE,
    LAUNCH_PRESSURE,
    LAUNCH_HUMIDITY,
    TIME_STEP,
    MAX_SIMULATION_TIME,
    MAXIMUM_ANGLE,
    GRAVITY_MODEL_TYPE,
    CONSTANT_GRAVITY,
    STEPPER,
    RECOVERY_SPEED_WARNING,
    DROGUE_LOW_SPEED_WARNING,
    MAIN_HIGH_SPEED_WARNING,
    MAIN_LOW_SPEED_WARNING,
    SEED,
    SEED_FIXED,
    DRAG_LOOKUP,
    STABILITY_LOOKUP,
};

/// Changes @p field of @p options (OptionsProbe.copyAndEquals()'s changes).
void change(SimulationOptions& options, Field field)
{
    switch (field)
    {
        case Field::LAUNCH_ROD_LENGTH:
            options.setLaunchRodLength(3);
            return;
        case Field::LAUNCH_INTO_WIND:
            options.setLaunchIntoWind(false);
            return;
        case Field::LAUNCH_ROD_ANGLE:
            options.setLaunchRodAngle(0.2);
            return;
        case Field::LAUNCH_ROD_DIRECTION:
            options.setLaunchRodDirection(1.0);
            return;
        case Field::WIND_MODEL_TYPE:
            options.setWindModelType(WindModelType::MULTI_LEVEL);
            return;
        case Field::AVERAGE_WIND:
            options.getAverageWindModel().setAverage(7);
            return;
        case Field::MULTI_LEVEL_WIND:
            addLevel(options.getMultiLevelWindModel(), 100.0, 5.0, 0.5);
            return;
        case Field::LAUNCH_ALTITUDE:
            options.setLaunchAltitude(50);
            return;
        case Field::LAUNCH_LATITUDE:
            options.setLaunchLatitude(10);
            return;
        case Field::LAUNCH_LONGITUDE:
            options.setLaunchLongitude(10);
            return;
        case Field::GEODETIC_COMPUTATION:
            options.setGeodeticComputation(GeodeticComputationStrategy::FLAT);
            return;
        case Field::USE_ISA:
            options.setIsaAtmosphere(true);
            return;
        case Field::LAUNCH_TEMPERATURE:
            options.setLaunchTemperature(300);
            return;
        case Field::LAUNCH_PRESSURE:
            options.setLaunchPressure(90000);
            return;
        case Field::LAUNCH_HUMIDITY:
            options.setLaunchRelativeHumidity(0.5);
            return;
        case Field::TIME_STEP:
            options.setTimeStep(0.01);
            return;
        case Field::MAX_SIMULATION_TIME:
            options.setMaxSimulationTime(100);
            return;
        case Field::MAXIMUM_ANGLE:
            options.setMaximumStepAngle(0.1);
            return;
        case Field::GRAVITY_MODEL_TYPE:
            options.setGravityModelType(GravityModelType::CONSTANT);
            return;
        case Field::CONSTANT_GRAVITY:
            options.setConstantGravity(1.62);
            return;
        case Field::STEPPER:
            options.setSimulationStepperMethodChoice(SimulationStepperMethod::RK6);
            return;
        case Field::RECOVERY_SPEED_WARNING:
            options.setRecoverySpeedWarning(25);
            return;
        case Field::DROGUE_LOW_SPEED_WARNING:
            options.setDrogueLowSpeedWarning(5);
            return;
        case Field::MAIN_HIGH_SPEED_WARNING:
            options.setRecoveryDrogueMainHighSpeedWarning(35);
            return;
        case Field::MAIN_LOW_SPEED_WARNING:
            options.setRecoveryDrogueMainLowSpeedWarning(10);
            return;
        case Field::SEED:
            options.setRandomSeed(otherSeed(options.getRandomSeed()));
            return;
        case Field::SEED_FIXED:
            options.setRandomSeedFixed(true);
            return;
        case Field::DRAG_LOOKUP:
            options.setDragLookup(lookupCsv(), parseTable(dragRows("2"), dragColumns()),
                                  dragRows("2"));
            return;
        case Field::STABILITY_LOOKUP:
            options.setStabilityLookup(lookupCsv(),
                                       parseTable(stabilityRows("2"), stabilityColumns()),
                                       stabilityRows("2"));
            return;
    }
}

/// One case of OptionsProbe.copyAndEquals(): options without the ISA and a copy of them;
/// @p field of the options is changed (after both seeds were fixed, for @p bothSeedsFixed),
/// and the copy takes the options' conditions twice. The text tells whether the two were equal
/// before, the events of the copy for the first and the second copyConditionsFrom(), whether
/// they are equal after, whether the seed was taken, the events of the copy's two wind models,
/// and whether every value of the copy is now the options'.
[[nodiscard]] std::string copyCase(Field field, bool bothSeedsFixed = false)
{
    SimulationOptions src;
    src.setIsaAtmosphere(false);
    SimulationOptions dst = src;
    if (bothSeedsFixed)
    {
        src.setRandomSeedFixed(true);
        dst.setRandomSeedFixed(true);
    }
    change(src, field);
    const bool          equalBefore = src == dst;
    const ChangeCounter events(dst.changed());
    const ChangeCounter averageEvents(dst.getAverageWindModel().changed());
    const ChangeCounter multiEvents(dst.getMultiLevelWindModel().changed());
    dst.copyConditionsFrom(src);
    const int first = events.count();
    dst.copyConditionsFrom(src);
    return std::format(
        "equalsBefore {} copyEvents {} again {} equalsAfter {} seedCopied {} "
        "averageEvents {} multiEvents {} sameValues {}",
        equalBefore, first, events.count() - first, src == dst,
        src.getRandomSeed() == dst.getRandomSeed(), averageEvents.count(), multiEvents.count(),
        describe(src) == describe(dst));
}

/// copyCase() of an option that equals() compares.
constexpr std::string_view kCopied =
    "equalsBefore false copyEvents 1 again 0 equalsAfter true "
    "seedCopied true averageEvents 0 multiEvents 0 sameValues true";
/// copyCase() of an option that equals() ignores.
constexpr std::string_view kCopiedAndIgnoredByEquals =
    "equalsBefore true copyEvents 1 again 0 equalsAfter true seedCopied true averageEvents 0 "
    "multiEvents 0 sameValues true";

/// Whether a copy of options whose @p field was changed holds every value of them, the seed
/// included, and equals them.
[[nodiscard]] bool copyTakes(Field field)
{
    SimulationOptions original;
    original.setIsaAtmosphere(false);
    change(original, field);
    const std::string values = describe(original);
    SimulationOptions copy   = original;
    const bool        same   = describe(copy) == values && copy == original &&
                               copy.getRandomSeed() == original.getRandomSeed() &&
                               copy.getDragLookupTable() == original.getDragLookupTable() &&
                               copy.getStabilityLookupTable() == original.getStabilityLookupTable();
    // The copy is its own object: populating it leaves the original as it was.
    populate(copy);
    return same && describe(original) == values;
}

TEST(SimulationOptions, CopyTakesTheLaunchRodAndWind)
{
    EXPECT_TRUE(copyTakes(Field::LAUNCH_ROD_LENGTH));
    EXPECT_TRUE(copyTakes(Field::LAUNCH_INTO_WIND));
    EXPECT_TRUE(copyTakes(Field::LAUNCH_ROD_ANGLE));
    EXPECT_TRUE(copyTakes(Field::LAUNCH_ROD_DIRECTION));
    EXPECT_TRUE(copyTakes(Field::WIND_MODEL_TYPE));
    EXPECT_TRUE(copyTakes(Field::AVERAGE_WIND));
    EXPECT_TRUE(copyTakes(Field::MULTI_LEVEL_WIND));
}

TEST(SimulationOptions, CopyTakesTheLaunchSiteAndAtmosphere)
{
    EXPECT_TRUE(copyTakes(Field::LAUNCH_ALTITUDE));
    EXPECT_TRUE(copyTakes(Field::LAUNCH_LATITUDE));
    EXPECT_TRUE(copyTakes(Field::LAUNCH_LONGITUDE));
    EXPECT_TRUE(copyTakes(Field::GEODETIC_COMPUTATION));
    EXPECT_TRUE(copyTakes(Field::USE_ISA));
    EXPECT_TRUE(copyTakes(Field::LAUNCH_TEMPERATURE));
    EXPECT_TRUE(copyTakes(Field::LAUNCH_PRESSURE));
    EXPECT_TRUE(copyTakes(Field::LAUNCH_HUMIDITY));
}

TEST(SimulationOptions, CopyTakesTheStepperGravityThresholdsLookupsAndSeed)
{
    EXPECT_TRUE(copyTakes(Field::TIME_STEP));
    EXPECT_TRUE(copyTakes(Field::MAX_SIMULATION_TIME));
    EXPECT_TRUE(copyTakes(Field::MAXIMUM_ANGLE));
    EXPECT_TRUE(copyTakes(Field::GRAVITY_MODEL_TYPE));
    EXPECT_TRUE(copyTakes(Field::CONSTANT_GRAVITY));
    EXPECT_TRUE(copyTakes(Field::STEPPER));
    EXPECT_TRUE(copyTakes(Field::RECOVERY_SPEED_WARNING));
    EXPECT_TRUE(copyTakes(Field::DROGUE_LOW_SPEED_WARNING));
    EXPECT_TRUE(copyTakes(Field::MAIN_HIGH_SPEED_WARNING));
    EXPECT_TRUE(copyTakes(Field::MAIN_LOW_SPEED_WARNING));
    EXPECT_TRUE(copyTakes(Field::DRAG_LOOKUP));
    EXPECT_TRUE(copyTakes(Field::STABILITY_LOOKUP));
    EXPECT_TRUE(copyTakes(Field::SEED));
    EXPECT_TRUE(copyTakes(Field::SEED_FIXED));
}

TEST(SimulationOptions, CopyConditionsFromOfACopyChangesNothing)
{
    SimulationOptions original;
    SimulationOptions copy = original;
    EXPECT_TRUE(original == copy);
    const ChangeCounter events(copy.changed());
    copy.copyConditionsFrom(original);
    EXPECT_EQ(events.count(), 0);
    // Nor does copying from itself.
    copy.copyConditionsFrom(copy);
    EXPECT_EQ(events.count(), 0);
}

TEST(SimulationOptions, CopyConditionsFromTakesTheLaunchRod)
{
    EXPECT_EQ(copyCase(Field::LAUNCH_ROD_LENGTH), kCopied);
    EXPECT_EQ(copyCase(Field::LAUNCH_INTO_WIND), kCopiedAndIgnoredByEquals);
    EXPECT_EQ(copyCase(Field::LAUNCH_ROD_ANGLE), kCopied);
    EXPECT_EQ(copyCase(Field::LAUNCH_ROD_DIRECTION), kCopied);
}

TEST(SimulationOptions, CopyConditionsFromTakesTheWind)
{
    EXPECT_EQ(copyCase(Field::WIND_MODEL_TYPE), kCopied);
    // A wind model that was loaded announces it, which the options forward, and the options
    // announce the copy.
    EXPECT_EQ(copyCase(Field::AVERAGE_WIND),
              "equalsBefore false copyEvents 2 again 0 equalsAfter true seedCopied true "
              "averageEvents 1 multiEvents 0 sameValues true");
    EXPECT_EQ(copyCase(Field::MULTI_LEVEL_WIND),
              "equalsBefore false copyEvents 2 again 0 equalsAfter true seedCopied true "
              "averageEvents 0 multiEvents 1 sameValues true");
}

TEST(SimulationOptions, CopyConditionsFromTakesTheLaunchSiteAndAtmosphere)
{
    EXPECT_EQ(copyCase(Field::LAUNCH_ALTITUDE), kCopied);
    EXPECT_EQ(copyCase(Field::LAUNCH_LATITUDE), kCopied);
    EXPECT_EQ(copyCase(Field::LAUNCH_LONGITUDE), kCopied);
    EXPECT_EQ(copyCase(Field::GEODETIC_COMPUTATION), kCopiedAndIgnoredByEquals);
    EXPECT_EQ(copyCase(Field::USE_ISA), kCopiedAndIgnoredByEquals);
    EXPECT_EQ(copyCase(Field::LAUNCH_TEMPERATURE), kCopied);
    EXPECT_EQ(copyCase(Field::LAUNCH_PRESSURE), kCopied);
    EXPECT_EQ(copyCase(Field::LAUNCH_HUMIDITY), kCopied);
}

TEST(SimulationOptions, CopyConditionsFromTakesTheStepperGravityAndThresholds)
{
    EXPECT_EQ(copyCase(Field::TIME_STEP), kCopied);
    EXPECT_EQ(copyCase(Field::MAX_SIMULATION_TIME), kCopied);
    EXPECT_EQ(copyCase(Field::MAXIMUM_ANGLE), kCopied);
    EXPECT_EQ(copyCase(Field::GRAVITY_MODEL_TYPE), kCopied);
    EXPECT_EQ(copyCase(Field::CONSTANT_GRAVITY), kCopied);
    EXPECT_EQ(copyCase(Field::STEPPER), kCopied);
    EXPECT_EQ(copyCase(Field::RECOVERY_SPEED_WARNING), kCopied);
    EXPECT_EQ(copyCase(Field::DROGUE_LOW_SPEED_WARNING), kCopied);
    EXPECT_EQ(copyCase(Field::MAIN_HIGH_SPEED_WARNING), kCopied);
    EXPECT_EQ(copyCase(Field::MAIN_LOW_SPEED_WARNING), kCopied);
}

TEST(SimulationOptions, CopyConditionsFromTakesTheLookups)
{
    EXPECT_EQ(copyCase(Field::DRAG_LOOKUP), kCopied);
    EXPECT_EQ(copyCase(Field::STABILITY_LOOKUP), kCopied);
}

TEST(SimulationOptions, CopyConditionsFromTakesALookupPathThatAloneDiffers)
{
    // FixProbe: 1 event, then 2, then no more.
    SimulationOptions   src = lookupOptions("2.0");
    SimulationOptions   dst = src;
    const ChangeCounter events(dst.changed());

    src.setDragLookup(std::filesystem::path("other-drag.csv"), src.getDragLookupTable(),
                      src.getDragLookupCsvRows());
    EXPECT_NE(dst.getDragLookupCsvPath(), src.getDragLookupCsvPath());
    dst.copyConditionsFrom(src);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(dst.getDragLookupCsvPath(), src.getDragLookupCsvPath());
    EXPECT_EQ(dst.getDragLookupTable(), src.getDragLookupTable());
    EXPECT_EQ(dst.getDragLookupCsvRows(), dragRows("2.0"));

    src.setStabilityLookup(std::filesystem::path("other-stability.csv"),
                           src.getStabilityLookupTable(), src.getStabilityLookupCsvRows());
    EXPECT_NE(dst.getStabilityLookupCsvPath(), src.getStabilityLookupCsvPath());
    dst.copyConditionsFrom(src);
    EXPECT_EQ(events.count(), 2);
    EXPECT_EQ(dst.getStabilityLookupCsvPath(), src.getStabilityLookupCsvPath());
    EXPECT_EQ(dst.getStabilityLookupTable(), src.getStabilityLookupTable());
    EXPECT_EQ(dst.getStabilityLookupCsvRows(), stabilityRows("2.0"));

    dst.copyConditionsFrom(src);
    EXPECT_EQ(events.count(), 2);
}

TEST(SimulationOptions, CopyConditionsFromTakesALookupTableThatAloneDiffers)
{
    // Another table object with the same content, path and rows (FixProbe: 1 event, then 2,
    // then no more).
    SimulationOptions   src = lookupOptions("2.0");
    SimulationOptions   dst = src;
    const ChangeCounter events(dst.changed());

    src.setDragLookup(lookupCsv(), parseTable(dragRows("2.0"), dragColumns()), dragRows("2.0"));
    EXPECT_NE(dst.getDragLookupTable(), src.getDragLookupTable());
    dst.copyConditionsFrom(src);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(dst.getDragLookupTable(), src.getDragLookupTable());

    src.setStabilityLookup(lookupCsv(), parseTable(stabilityRows("2.0"), stabilityColumns()),
                           stabilityRows("2.0"));
    EXPECT_NE(dst.getStabilityLookupTable(), src.getStabilityLookupTable());
    dst.copyConditionsFrom(src);
    EXPECT_EQ(events.count(), 2);
    EXPECT_EQ(dst.getStabilityLookupTable(), src.getStabilityLookupTable());

    dst.copyConditionsFrom(src);
    EXPECT_EQ(events.count(), 2);
}

TEST(SimulationOptions, CopyConditionsFromTakesTheSeedOnlyWithAChange)
{
    // A seed that is not fixed is no difference: nothing is announced, nothing is taken.
    EXPECT_EQ(copyCase(Field::SEED),
              "equalsBefore true copyEvents 0 again 0 equalsAfter true seedCopied false "
              "averageEvents 0 multiEvents 0 sameValues true");
    EXPECT_EQ(copyCase(Field::SEED_FIXED), kCopied);
    EXPECT_EQ(copyCase(Field::SEED, true), kCopied);
}

TEST(SimulationOptions, CopyConditionsFromTakesTheGeneratedSeedAlongWithAnotherChange)
{
    SimulationOptions src;
    SimulationOptions dst = src;
    src.setRandomSeed(otherSeed(src.getRandomSeed()));
    src.setLaunchRodLength(3);
    dst.copyConditionsFrom(src);
    EXPECT_EQ(dst.getRandomSeed(), src.getRandomSeed());
    EXPECT_FALSE(dst.isRandomSeedFixed());
}

TEST(SimulationOptions, CopyConditionsFromTakesEveryCondition)
{
    SimulationOptions src;
    populate(src);
    SimulationOptions   dst;
    const ChangeCounter events(dst.changed());
    dst.copyConditionsFrom(src);
    EXPECT_EQ(describe(dst), describe(src));
    EXPECT_EQ(dst.getRandomSeed(), 424242);
    EXPECT_TRUE(dst.isRandomSeedFixed());
    EXPECT_EQ(dst.getDragLookupTable(), src.getDragLookupTable());
    EXPECT_EQ(dst.getStabilityLookupTable(), src.getStabilityLookupTable());
    EXPECT_EQ(dst.getDragLookupCsvPath(), src.getDragLookupCsvPath());
    EXPECT_EQ(dst.getStabilityLookupCsvPath(), src.getStabilityLookupCsvPath());
    // The two wind models and the options themselves.
    EXPECT_EQ(events.count(), 3);
    // The levels came with their seeds; the average model keeps its own.
    EXPECT_TRUE(dst.getMultiLevelWindModel() == src.getMultiLevelWindModel());
    EXPECT_NE(&dst.getMultiLevelWindModel(), &src.getMultiLevelWindModel());
}

TEST(SimulationOptions, CopyConditionsFromBetweenOptionsThatAreNoCopiesAlwaysAnnounces)
{
    // The wind models compare with their seeds, which copyConditionsFrom() does not take for
    // the average model. (Seeds set here: two constructed options differ by their random ones.)
    SimulationOptions x;
    SimulationOptions y;
    x.getAverageWindModel().setSeed(1);
    y.getAverageWindModel().setSeed(2);
    x.getMultiLevelWindModel().setSeed(10);
    y.getMultiLevelWindModel().setSeed(20);
    const ChangeCounter events(y.changed());
    y.copyConditionsFrom(x);
    // Both wind models and the options.
    EXPECT_EQ(events.count(), 3);
    y.copyConditionsFrom(x);
    // The multi-level models are equal now; the average ones still differ by their seeds.
    EXPECT_EQ(events.count(), 5);
    EXPECT_FALSE(x == y);
    EXPECT_EQ(x.getRandomSeed(), y.getRandomSeed());
    EXPECT_EQ(describe(x), describe(y));
}

TEST(SimulationOptions, CopyConditionsFromComparesExactly)
{
    // != where the setters use MathUtil.equals: a NaN differs from itself, every time.
    SimulationOptions n;
    n.setIsaAtmosphere(false);
    n.setLaunchTemperature(kNaN);
    SimulationOptions   m = n;
    const ChangeCounter events(m.changed());
    m.copyConditionsFrom(n);
    EXPECT_EQ(events.count(), 1);
    m.copyConditionsFrom(n);
    EXPECT_EQ(events.count(), 2);
}

TEST(SimulationOptions, CopyConditionsFromKeepsConnectionsAndPreferences)
{
    InMemoryPreferences preferences;
    SimulationOptions   dst(preferences);
    SimulationOptions   src;
    src.setSimulationStepperMethodChoice(SimulationStepperMethod::RK6);
    const ChangeCounter srcEvents(src.changed());
    const ChangeCounter events(dst.changed());
    dst.copyConditionsFrom(src);
    EXPECT_GT(events.count(), 0);
    EXPECT_EQ(srcEvents.count(), 0);
    EXPECT_EQ(dst.getSimulationStepperMethodChoice(), SimulationStepperMethod::RK6);
    // The stepper was copied as a field: nothing was written to the store yet.
    EXPECT_TRUE(preferences.empty());
    dst.setSimulationStepperMethodChoice(SimulationStepperMethod::RK4);
    EXPECT_EQ(preferences.get(Keys::kSimulationStepperMethod), "RK4");
    // The source is only read.
    EXPECT_EQ(src.changed().size(), 1U);
}

// ------------------------------------------------- what a listener sees when it is called

TEST(SimulationOptions, AListenerSeesTheNewValue)
{
    // Every setter stores before it announces: a listener that reads the options (a bound
    // control, a simulation marking itself outdated) finds the new value. FixProbe: 2.5; true;
    // true; RK6 with the preference RK6 already.
    InMemoryPreferences preferences;
    SimulationOptions   options(preferences);
    const StateRecorder recorder(options, preferences);

    options.setLaunchRodLength(2.5);
    ASSERT_EQ(recorder.seen().size(), 1U);
    EXPECT_EQ(recorder.seen().back().values, describe(options));
    EXPECT_EQ(options.getLaunchRodLength(), 2.5);

    options.setDragLookup(std::nullopt, parseTable(dragRows("2"), dragColumns()));
    ASSERT_EQ(recorder.seen().size(), 2U);
    EXPECT_EQ(recorder.seen().back().values, describe(options));
    EXPECT_TRUE(options.hasDragLookup());

    options.setStabilityLookup(std::nullopt, parseTable(stabilityRows("2"), stabilityColumns()));
    ASSERT_EQ(recorder.seen().size(), 3U);
    EXPECT_EQ(recorder.seen().back().values, describe(options));
    EXPECT_TRUE(options.hasStabilityLookup());

    // The choice is in the preferences before the listeners hear of it.
    options.setSimulationStepperMethodChoice(SimulationStepperMethod::RK6);
    ASSERT_EQ(recorder.seen().size(), 4U);
    EXPECT_EQ(recorder.seen().back().values, describe(options));
    EXPECT_EQ(options.getSimulationStepperMethodChoice(), SimulationStepperMethod::RK6);
    EXPECT_EQ(recorder.seen().back().storedStepper, "RK6");
}

TEST(SimulationOptions, TheIsaConditionsAnnounceThemselvesBeforeTheFlagDoes)
{
    // Switching the ISA on stores the flag, then sets the three conditions, each announcing
    // itself, and announces itself last (FixProbe: four events, the flag set in all of them).
    InMemoryPreferences preferences;
    SimulationOptions   options(preferences);
    options.setIsaAtmosphere(false);
    options.setLaunchAltitude(200);
    options.setLaunchTemperature(300);
    options.setLaunchPressure(90000);
    options.setLaunchRelativeHumidity(0.5);
    const StateRecorder recorder(options, preferences);

    options.setIsaAtmosphere(true);
    ASSERT_EQ(recorder.seen().size(), 4U);
    // The temperature's own event: the ISA temperature, the old pressure and humidity.
    EXPECT_TRUE(recorder.seen()[0].isa);
    EXPECT_EQ(recorder.seen()[0].temperature, options.getLaunchTemperature());
    EXPECT_EQ(recorder.seen()[0].pressure, 90000.0);
    EXPECT_EQ(recorder.seen()[0].humidity, 0.5);
    // The pressure's.
    EXPECT_EQ(recorder.seen()[1].pressure, options.getLaunchPressure());
    EXPECT_EQ(recorder.seen()[1].humidity, 0.5);
    // The humidity's, and the flag's own, which sees nothing new.
    EXPECT_EQ(recorder.seen()[2].humidity, 0.0);
    EXPECT_EQ(recorder.seen()[3].values, recorder.seen()[2].values);
    EXPECT_EQ(recorder.seen()[3].values, describe(options));
    EXPECT_TRUE(isJavaValue(kIsaTemperature200, options.getLaunchTemperature()));
    EXPECT_TRUE(isJavaValue(kIsaPressure200, options.getLaunchPressure()));
}

TEST(SimulationOptions, TheAltitudeIsStoredBeforeTheIsaConditionsAnnounceThemselves)
{
    // FixProbe: three events, all at 100 m; the first with the ISA temperature and the old
    // pressure.
    InMemoryPreferences preferences;
    SimulationOptions   options(preferences);
    const StateRecorder recorder(options, preferences);

    options.setLaunchAltitude(100);
    ASSERT_EQ(recorder.seen().size(), 3U);
    EXPECT_EQ(recorder.seen()[0].altitude, 100.0);
    EXPECT_EQ(recorder.seen()[0].temperature, options.getLaunchTemperature());
    EXPECT_EQ(recorder.seen()[0].pressure, 101325.0);
    EXPECT_EQ(recorder.seen()[1].altitude, 100.0);
    EXPECT_EQ(recorder.seen()[1].pressure, options.getLaunchPressure());
    EXPECT_EQ(recorder.seen()[2].values, describe(options));
    EXPECT_TRUE(isJavaValue(kIsaTemperature100, options.getLaunchTemperature()));
    EXPECT_TRUE(isJavaValue(kIsaPressure100, options.getLaunchPressure()));
}

/// Whether a listener of options whose @p field is changed finds, at the last event of that
/// change, every value the options have afterwards; false also when nothing was announced.
[[nodiscard]] bool listenerSeesTheChange(Field field)
{
    InMemoryPreferences preferences;
    SimulationOptions   options(preferences);
    options.setIsaAtmosphere(false);
    const StateRecorder recorder(options, preferences);
    change(options, field);
    return !recorder.seen().empty() && recorder.seen().back().values == describe(options);
}

TEST(SimulationOptions, AListenerSeesTheChangedLaunchRodAndWind)
{
    EXPECT_TRUE(listenerSeesTheChange(Field::LAUNCH_ROD_LENGTH));
    EXPECT_TRUE(listenerSeesTheChange(Field::LAUNCH_INTO_WIND));
    EXPECT_TRUE(listenerSeesTheChange(Field::LAUNCH_ROD_ANGLE));
    EXPECT_TRUE(listenerSeesTheChange(Field::LAUNCH_ROD_DIRECTION));
    EXPECT_TRUE(listenerSeesTheChange(Field::WIND_MODEL_TYPE));
    EXPECT_TRUE(listenerSeesTheChange(Field::AVERAGE_WIND));
    EXPECT_TRUE(listenerSeesTheChange(Field::MULTI_LEVEL_WIND));
}

TEST(SimulationOptions, AListenerSeesTheChangedLaunchSiteAndAtmosphere)
{
    EXPECT_TRUE(listenerSeesTheChange(Field::LAUNCH_ALTITUDE));
    EXPECT_TRUE(listenerSeesTheChange(Field::LAUNCH_LATITUDE));
    EXPECT_TRUE(listenerSeesTheChange(Field::LAUNCH_LONGITUDE));
    EXPECT_TRUE(listenerSeesTheChange(Field::GEODETIC_COMPUTATION));
    EXPECT_TRUE(listenerSeesTheChange(Field::USE_ISA));
    EXPECT_TRUE(listenerSeesTheChange(Field::LAUNCH_TEMPERATURE));
    EXPECT_TRUE(listenerSeesTheChange(Field::LAUNCH_PRESSURE));
    EXPECT_TRUE(listenerSeesTheChange(Field::LAUNCH_HUMIDITY));
}

TEST(SimulationOptions, AListenerSeesTheChangedStepperGravityThresholdsAndLookups)
{
    EXPECT_TRUE(listenerSeesTheChange(Field::TIME_STEP));
    EXPECT_TRUE(listenerSeesTheChange(Field::MAX_SIMULATION_TIME));
    EXPECT_TRUE(listenerSeesTheChange(Field::MAXIMUM_ANGLE));
    EXPECT_TRUE(listenerSeesTheChange(Field::GRAVITY_MODEL_TYPE));
    EXPECT_TRUE(listenerSeesTheChange(Field::CONSTANT_GRAVITY));
    EXPECT_TRUE(listenerSeesTheChange(Field::STEPPER));
    EXPECT_TRUE(listenerSeesTheChange(Field::RECOVERY_SPEED_WARNING));
    EXPECT_TRUE(listenerSeesTheChange(Field::DROGUE_LOW_SPEED_WARNING));
    EXPECT_TRUE(listenerSeesTheChange(Field::MAIN_HIGH_SPEED_WARNING));
    EXPECT_TRUE(listenerSeesTheChange(Field::MAIN_LOW_SPEED_WARNING));
    EXPECT_TRUE(listenerSeesTheChange(Field::DRAG_LOOKUP));
    EXPECT_TRUE(listenerSeesTheChange(Field::STABILITY_LOOKUP));
    EXPECT_TRUE(listenerSeesTheChange(Field::SEED_FIXED));
    // A generated seed is not announced at all.
    EXPECT_FALSE(listenerSeesTheChange(Field::SEED));
}

/// copyConditionsFrom() of options with every option changed: how many of the events a
/// listener of the target hears show it every value the target has afterwards, how many events
/// there are, and whether the target then has the source's values.
[[nodiscard]] std::string eventsThatSeeTheCopiedConditions()
{
    InMemoryPreferences preferences;
    SimulationOptions   src;
    populate(src);
    SimulationOptions   dst(preferences);
    const StateRecorder recorder(dst, preferences);
    dst.copyConditionsFrom(src);
    const std::string result = describe(dst);
    return std::format("{} of {} {}",
                       std::ranges::count(recorder.seen(), result, &SeenState::values),
                       recorder.seen().size(), result == describe(src));
}

TEST(SimulationOptions, AListenerSeesTheCopiedConditions)
{
    // The two wind models and the options announce the copy only after every condition has
    // been taken (FixProbe).
    EXPECT_EQ(eventsThatSeeTheCopiedConditions(), "3 of 3 true");
}

// ------------------------------------------------------------------ equality and hashing

TEST(SimulationOptions, EqualityDependsOnTheWindModelSeeds)
{
    // Two constructed options differ by the random seeds of their wind models, which is why
    // Java's new SimulationOptions().equals(new SimulationOptions()) is false.
    SimulationOptions a;
    SimulationOptions b;
    a.getAverageWindModel().setSeed(1);
    b.getAverageWindModel().setSeed(1);
    a.getMultiLevelWindModel().setSeed(7);
    b.getMultiLevelWindModel().setSeed(7);
    // The options' own seeds differ, but they are not fixed.
    b.setRandomSeed(otherSeed(a.getRandomSeed()));
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(b == a);
    b.getAverageWindModel().setSeed(2);
    EXPECT_FALSE(a == b);
    b.getAverageWindModel().setSeed(1);
    EXPECT_TRUE(a == b);
    b.getMultiLevelWindModel().setSeed(8);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

TEST(SimulationOptions, EqualityComparesTheLookupTablesByIdentity)
{
    SimulationOptions a = lookupOptions("2.0");
    SimulationOptions b = a;
    EXPECT_TRUE(a == b);
    // The same content in another table object.
    b.setDragLookup(lookupCsv(), parseTable(dragRows("2.0"), dragColumns()), dragRows("2.0"));
    EXPECT_FALSE(a == b);
    b.setDragLookup(lookupCsv(), a.getDragLookupTable(), std::nullopt);
    // The rows and the path do not count.
    EXPECT_TRUE(a == b);
    b.setDragLookup(std::nullopt, a.getDragLookupTable(), std::nullopt);
    EXPECT_TRUE(a == b);
    b.setStabilityLookup(lookupCsv(), parseTable(stabilityRows("2.0"), stabilityColumns()),
                         stabilityRows("2.0"));
    EXPECT_FALSE(a == b);
}

TEST(SimulationOptions, OptionsWithANaNEqualNothing)
{
    SimulationOptions options;
    options.setIsaAtmosphere(false);
    options.setLaunchTemperature(kNaN);
    const SimulationOptions& same = options;
    EXPECT_FALSE(options == same);
}

TEST(SimulationOptions, HashCodeIsZero)
{
    SimulationOptions options;
    EXPECT_EQ(options.hashCode(), 0);
    populate(options);
    EXPECT_EQ(options.hashCode(), 0);
}

// ------------------------------------------------------------------------------ toString

TEST(SimulationOptions, ToStringOfTheDefaults)
{
    const SimulationOptions options;
    // Java's text, but for the atmospheric model and the two wind models, which Java prints as
    // class@hash, and the seed.
    const std::string expected = std::format(
        "SimulationOptions [\n"
        "    AtmosphericModel: ExtendedIsaModel\n"
        "    launchRodLength:  1.000000\n"
        "    launchIntoWind: true\n"
        "    launchRodAngle:  0.000000\n"
        "    launchRodDirection:  1.570796\n"
        "    windModelType: AVERAGE\n"
        "    pinkNoiseWindModel: PinkNoiseWindModel\n"
        "    multiLevelPinkNoiseWindModel: "
        "MultiLevelPinkNoiseWindModel\n"
        "    launchAltitude:  0.000000\n"
        "    launchLatitude:  28.610000\n"
        "    launchLongitude:  -80.600000\n"
        "    geodeticComputation:  Spherical approximation\n"
        "    useISA:  true\n"
        "    launchTemperature:  288.150000\n"
        "    launchPressure:  101325.000000\n"
        "    launchHumidity:  0.000000\n"
        "    timeStep:  0.050000\n"
        "    maxTime:  1200.000000\n"
        "    maximumAngle:  0.052360\n"
        "    stepperMethodChoice: 6-DOF Runge-Kutta 4\n"
        "    randomSeedFixed: false\n"
        "    randomSeed: {}\n"
        "]\n",
        options.getRandomSeed());
    EXPECT_EQ(options.toString(), expected);
}

TEST(SimulationOptions, ToStringOfChangedOptions)
{
    SimulationOptions options;
    populate(options);
    options.setGeodeticComputation(GeodeticComputationStrategy::FLAT);
    const std::string text = options.toString();
    EXPECT_NE(text.find("    launchIntoWind: false\n"), std::string::npos) << text;
    // The stored direction, not the wind's.
    EXPECT_NE(text.find("    launchRodDirection:  1.750000\n"), std::string::npos) << text;
    EXPECT_NE(text.find("    windModelType: MULTI_LEVEL\n"), std::string::npos) << text;
    EXPECT_NE(text.find("    geodeticComputation:  Flat Earth\n"), std::string::npos) << text;
    EXPECT_NE(text.find("    useISA:  false\n"), std::string::npos) << text;
    EXPECT_NE(text.find("    launchPressure:  98765.000000\n"), std::string::npos) << text;
    EXPECT_NE(text.find("    stepperMethodChoice: 6-DOF Runge-Kutta 6\n"), std::string::npos)
        << text;
    EXPECT_NE(text.find("    randomSeedFixed: true\n    randomSeed: 424242\n]\n"),
              std::string::npos)
        << text;
    options.setGeodeticComputation(GeodeticComputationStrategy::WGS84);
    EXPECT_NE(options.toString().find("    geodeticComputation:  WGS84 ellipsoid\n"),
              std::string::npos);
}

TEST(SimulationOptions, ToStringPrintsTheStoredRodDirectionWhileLaunchingIntoTheWind)
{
    // Java prints the field, not getLaunchRodDirection().
    SimulationOptions options;
    options.setLaunchRodDirection(1.0);
    EXPECT_TRUE(options.getLaunchIntoWind());
    EXPECT_EQ(options.getLaunchRodDirection(), kPi / 2);
    const std::string text = options.toString();
    EXPECT_NE(text.find("    launchRodDirection:  1.000000\n"), std::string::npos) << text;
    EXPECT_EQ(text.find("    launchRodDirection:  1.570796\n"), std::string::npos) << text;
}

TEST(SimulationOptions, ToStringNeverFails)
{
    // Java builds the atmospheric model for its first line, which throws for these conditions.
    SimulationOptions options;
    options.setIsaAtmosphere(false);
    options.setLaunchTemperature(kNaN);
    options.setLaunchPressure(-1);
    const std::string text = options.toString();
    EXPECT_NE(text.find("    launchTemperature:  NaN\n"), std::string::npos) << text;
    EXPECT_NE(text.find("    launchPressure:  -1.000000\n"), std::string::npos) << text;
}

// ---------------------------------------------------- SimulationOptionsGravityTest.java

// SimulationOptionsGravityTest.testDefaultGravityModel
TEST(SimulationOptionsGravity, DefaultGravityModel)
{
    const SimulationOptions options;
    EXPECT_EQ(options.getGravityModelType(), GravityModelType::WGS);
    EXPECT_NEAR(options.getConstantGravity(), 9.807, 1e-6);
}

// SimulationOptionsGravityTest.testSetGravityModelType
TEST(SimulationOptionsGravity, SetGravityModelType)
{
    SimulationOptions options;

    options.setGravityModelType(GravityModelType::CONSTANT);
    EXPECT_EQ(options.getGravityModelType(), GravityModelType::CONSTANT);

    options.setGravityModelType(GravityModelType::WGS);
    EXPECT_EQ(options.getGravityModelType(), GravityModelType::WGS);
}

// SimulationOptionsGravityTest.testSetConstantGravity
TEST(SimulationOptionsGravity, SetConstantGravity)
{
    SimulationOptions options;

    const double customGravity = 5.0;
    options.setConstantGravity(customGravity);
    EXPECT_NEAR(options.getConstantGravity(), customGravity, 1e-6);
}

// SimulationOptionsGravityTest.testCopyConditionsFromWithGravity
TEST(SimulationOptionsGravity, CopyConditionsFromWithGravity)
{
    SimulationOptions source;
    source.setGravityModelType(GravityModelType::CONSTANT);
    source.setConstantGravity(1.62);  // Moon gravity

    SimulationOptions target;
    target.copyConditionsFrom(source);

    EXPECT_EQ(target.getGravityModelType(), GravityModelType::CONSTANT);
    EXPECT_NEAR(target.getConstantGravity(), 1.62, 1e-6);
}

// SimulationOptionsGravityTest.testEqualsWithGravity
TEST(SimulationOptionsGravity, EqualsWithGravity)
{
    SimulationOptions options1;
    options1.setGravityModelType(GravityModelType::CONSTANT);
    options1.setConstantGravity(10.0);

    SimulationOptions options2;
    options2.setGravityModelType(GravityModelType::CONSTANT);
    options2.setConstantGravity(10.0);

    // Note: equals() checks many fields, so we just verify it doesn't crash
    // and includes gravity fields in the comparison
    EXPECT_TRUE(options1.getGravityModelType() == options2.getGravityModelType());
    EXPECT_NEAR(options1.getConstantGravity(), options2.getConstantGravity(), 1e-6);
}

// SimulationOptionsGravityTest.testCloneWithGravity
TEST(SimulationOptionsGravity, CloneWithGravity)
{
    SimulationOptions original;
    original.setGravityModelType(GravityModelType::CONSTANT);
    original.setConstantGravity(8.87);  // Venus gravity

    SimulationOptions clone = original;
    original.setConstantGravity(1.0);

    EXPECT_EQ(clone.getGravityModelType(), GravityModelType::CONSTANT);
    EXPECT_NEAR(clone.getConstantGravity(), 8.87, 1e-6);
}

// SimulationOptionsGravityTest.testToSimulationConditionsWithWGS
TEST(SimulationOptionsGravity, ToSimulationConditionsWithWGS)
{
    SimulationOptions options;
    options.setGravityModelType(GravityModelType::WGS);

    const Result<SimulationConditions> conditions = options.toSimulationConditions();
    ASSERT_TRUE(conditions.has_value());
    const std::shared_ptr<const QtRocket::GravityModel> model = conditions->getGravityModel();

    ASSERT_NE(model, nullptr);
    EXPECT_NE(dynamic_cast<const WgsGravityModel*>(model.get()), nullptr);
}

// SimulationOptionsGravityTest.testToSimulationConditionsWithConstant
TEST(SimulationOptionsGravity, ToSimulationConditionsWithConstant)
{
    SimulationOptions options;
    const double      customGravity = 3.71;  // Mars gravity
    options.setGravityModelType(GravityModelType::CONSTANT);
    options.setConstantGravity(customGravity);

    const Result<SimulationConditions> conditions = options.toSimulationConditions();
    ASSERT_TRUE(conditions.has_value());
    const std::shared_ptr<const QtRocket::GravityModel> model = conditions->getGravityModel();

    ASSERT_NE(model, nullptr);
    const auto* constant = dynamic_cast<const ConstantGravityModel*>(model.get());
    ASSERT_NE(constant, nullptr);
    EXPECT_NEAR(customGravity, constant->getConstantGravity(), 1e-6);
}

// Beyond the JUnit test: equals() does include the gravity fields.
TEST(SimulationOptionsGravity, EqualsIncludesTheGravityFields)
{
    SimulationOptions options1;
    SimulationOptions options2 = options1;
    options2.setGravityModelType(GravityModelType::CONSTANT);
    EXPECT_FALSE(options1 == options2);
    options1.setGravityModelType(GravityModelType::CONSTANT);
    EXPECT_TRUE(options1 == options2);
    options2.setConstantGravity(10.0);
    EXPECT_FALSE(options1 == options2);
    options1.setConstantGravity(10.0);
    EXPECT_TRUE(options1 == options2);
}

// -------------------------------------------------- SimulationOptionsRandomSeedTest.java

// SimulationOptionsRandomSeedTest.testRandomSeedIsNotFixedByDefault
TEST(SimulationOptionsRandomSeed, RandomSeedIsNotFixedByDefault)
{
    const SimulationOptions options;

    EXPECT_FALSE(options.isRandomSeedFixed());
}

// SimulationOptionsRandomSeedTest.testFixedSeedIsNotRandomizedBeforeRun
TEST(SimulationOptionsRandomSeed, FixedSeedIsNotRandomizedBeforeRun)
{
    SimulationOptions options;
    options.setRandomSeed(12345);
    options.setRandomSeedFixed(true);

    options.randomizeSeedIfNotFixed();

    EXPECT_EQ(options.getRandomSeed(), 12345);
}

// SimulationOptionsRandomSeedTest.testDefaultFactoryAppliesFixedSeedPreference
TEST(SimulationOptionsRandomSeed, DefaultFactoryAppliesFixedSeedPreference)
{
    InMemoryPreferences preferences;
    preferences.setRandomSeed(246813579);
    preferences.setRandomSeedFixed(true);
    const DefaultSimulationOptionFactory factory(preferences);

    const SimulationOptions options = factory.getDefault();

    EXPECT_TRUE(options.isRandomSeedFixed());
    EXPECT_EQ(options.getRandomSeed(), 246813579);
}

// SimulationOptionsRandomSeedTest.testCopyConditionsCopiesFixedSeed
TEST(SimulationOptionsRandomSeed, CopyConditionsCopiesFixedSeed)
{
    SimulationOptions source;
    source.setRandomSeed(-987654321);
    source.setRandomSeedFixed(true);
    SimulationOptions target;

    target.copyConditionsFrom(source);

    EXPECT_TRUE(target.isRandomSeedFixed());
    EXPECT_EQ(target.getRandomSeed(), -987654321);
}

// SimulationOptionsRandomSeedTest.testEqualityIncludesOnlyFixedSeeds
TEST(SimulationOptionsRandomSeed, EqualityIncludesOnlyFixedSeeds)
{
    SimulationOptions first;
    SimulationOptions second = first;
    // Java: first.getRandomSeed() + 1, which wraps there.
    second.setRandomSeed(otherSeed(first.getRandomSeed()));

    EXPECT_TRUE(first == second) << "Generated seed values must not make simulations outdated";

    first.setRandomSeedFixed(true);
    second.setRandomSeedFixed(true);
    EXPECT_FALSE(first == second) << "Different user-selected seeds must make simulations unequal";

    second.setRandomSeed(first.getRandomSeed());
    EXPECT_TRUE(first == second);
}

// ---------------------------------------------------------- SimulationLookupCopyTest.java

// SimulationLookupCopyTest.testAbsentSourceRowsClearDestinationRows
TEST(SimulationLookupCopy, AbsentSourceRowsClearDestinationRows)
{
    SimulationOptions source = lookupOptions("2.0");
    source.setDragLookup(lookupCsv(), source.getDragLookupTable(), std::nullopt);
    source.setStabilityLookup(lookupCsv(), source.getStabilityLookupTable(), std::nullopt);
    SimulationOptions target = lookupOptions("1.0");

    target.copyConditionsFrom(source);

    EXPECT_EQ(target.getDragLookupCsvRows(), std::nullopt);
    EXPECT_EQ(target.getStabilityLookupCsvRows(), std::nullopt);
    EXPECT_EQ(target.getDragLookupTable(), source.getDragLookupTable());
    EXPECT_EQ(target.getStabilityLookupTable(), source.getStabilityLookupTable());
}

// SimulationLookupCopyTest.testChangedRowsAreCopiedAndNotifyEvenWhenTablesAreShared
TEST(SimulationLookupCopy, ChangedRowsAreCopiedAndNotifyEvenWhenTablesAreShared)
{
    SimulationOptions              source = lookupOptions("2.0");
    SimulationOptions              target = source;
    const std::vector<std::string> rows{"# edited comment", "Mach,Cd", "0,2", "1,2"};
    source.setDragLookup(lookupCsv(), source.getDragLookupTable(), rows);
    const ChangeCounter events(target.changed());

    target.copyConditionsFrom(source);

    EXPECT_EQ(target.getDragLookupCsvRows(), rows);
    EXPECT_EQ(events.count(), 1);
    target.copyConditionsFrom(source);
    EXPECT_EQ(events.count(), 1) << "Copying unchanged conditions should not notify again";
    source.clearDragLookup();
    EXPECT_EQ(target.getDragLookupCsvRows(), rows);
}

// The stability twin of testChangedRowsAreCopiedAndNotifyEvenWhenTablesAreShared, which the JUnit
// test leaves out (FixProbe: 1 event and the rows; still 1; 2 and null once the source has none;
// and the copied rows stay when the source's lookup is cleared).
TEST(SimulationLookupCopy, ChangedStabilityRowsAreCopiedAndNotifyEvenWhenTablesAreShared)
{
    SimulationOptions              source = lookupOptions("2.0");
    SimulationOptions              target = source;
    const std::vector<std::string> rows{"# edited comment", "Mach,Cn,Cm,Cp", "0,1,1,2", "1,1,1,2"};
    source.setStabilityLookup(lookupCsv(), source.getStabilityLookupTable(), rows);
    const ChangeCounter events(target.changed());

    target.copyConditionsFrom(source);

    EXPECT_EQ(target.getStabilityLookupCsvRows(), rows);
    EXPECT_EQ(events.count(), 1);
    target.copyConditionsFrom(source);
    EXPECT_EQ(events.count(), 1) << "Copying unchanged conditions should not notify again";

    // Rows that the source no longer has are cleared, and that is a change too.
    source.setStabilityLookup(lookupCsv(), source.getStabilityLookupTable(), std::nullopt);
    target.copyConditionsFrom(source);
    EXPECT_EQ(events.count(), 2);
    EXPECT_EQ(target.getStabilityLookupCsvRows(), std::nullopt);
    EXPECT_EQ(target.getStabilityLookupTable(), source.getStabilityLookupTable());
    // The drag rows were never touched.
    EXPECT_EQ(target.getDragLookupCsvRows(), dragRows("2.0"));

    // The copy holds its own rows: clearing the source's leaves them.
    source.setStabilityLookup(lookupCsv(), source.getStabilityLookupTable(), rows);
    target.copyConditionsFrom(source);
    source.clearStabilityLookup();
    EXPECT_EQ(target.getStabilityLookupCsvRows(), rows);
}

// The part of SimulationLookupCopyTest.testCopiedLookupsSurviveSaveAndReloadWithoutExternalFiles
// that needs no .ork round trip (the JUnit test itself waits for the file tier).
TEST(SimulationLookupCopy, CopiedLookupsCarryTheirTablesAndRows)
{
    const SimulationOptions source = lookupOptions("2.0");
    SimulationOptions       target;

    target.copyConditionsFrom(source);

    expectLookups(target, 2);
    EXPECT_EQ(target.getDragLookupCsvRows(), source.getDragLookupCsvRows());
    EXPECT_EQ(target.getStabilityLookupCsvRows(), source.getStabilityLookupCsvRows());
    EXPECT_EQ(target.getDragLookupCsvRows(), dragRows("2.0"));
    EXPECT_EQ(target.getStabilityLookupCsvRows(), stabilityRows("2.0"));
}

// The part of SimulationLookupCopyTest.testCopiedLookupsReplaceStaleDestinationRows that needs
// no .ork round trip.
TEST(SimulationLookupCopy, CopiedLookupsReplaceStaleDestinationRows)
{
    SimulationOptions target = lookupOptions("1.0");
    expectLookups(target, 1);

    target.copyConditionsFrom(lookupOptions("2.0"));

    expectLookups(target, 2);
    EXPECT_EQ(target.getDragLookupCsvRows(), dragRows("2.0"));
    EXPECT_EQ(target.getStabilityLookupCsvRows(), stabilityRows("2.0"));
}

// --------------------------------------------- WindModelSeedReproducibilityTest.java

/// WindModelSeedReproducibilityTest.SEED
constexpr int kSeed = 12345;
/// Turbulence has to be non-zero, or there is no randomness to reproduce.
constexpr double kTurbulence = 0.2;
constexpr double kWindSpeed  = 5.0;

/// WindModelSeedReproducibilityTest.sample(): wind speed sampled over the first seconds of a
/// flight.
[[nodiscard]] std::vector<double> sample(WindModel& model)
{
    std::vector<double> out;
    out.reserve(40);
    for (int i = 0; i < 40; ++i)
    {
        out.push_back(model.getWindVelocity(i * 0.1, 50.0).length());
    }
    return out;
}

/// WindModelSeedReproducibilityTest.averageModel()
void configureAverageModel(PinkNoiseWindModel& model)
{
    model.setAverage(kWindSpeed);
    model.setStandardDeviation(kWindSpeed * kTurbulence);
}

/// WindModelSeedReproducibilityTest.multiLevelModel()
void configureMultiLevelModel(MultiLevelPinkNoiseWindModel& model)
{
    model.clearLevels();
    addLevel(model, 0, kWindSpeed, 0, kWindSpeed * kTurbulence);
    addLevel(model, 200, kWindSpeed * 1.5, 0, kWindSpeed * kTurbulence);
}

/// The body of testSeedControlsTheWindItself for one model.
void expectSeedControlsTheWind(WindModel& model)
{
    model.setSeed(kSeed);
    const std::vector<double> first = sample(model);

    model.setSeed(kSeed + 1);
    const std::vector<double> other = sample(model);

    model.setSeed(kSeed);
    const std::vector<double> repeat = sample(model);

    EXPECT_EQ(first, repeat) << "the same seed must reproduce the same wind exactly";
    EXPECT_NE(first, other) << "a different seed must produce different wind, but the samples "
                               "were identical -- the seed is being ignored";
}

// WindModelSeedReproducibilityTest.testSeedControlsTheWindItself
TEST(WindModelSeedReproducibility, SeedControlsTheWindItself)
{
    PinkNoiseWindModel average;
    configureAverageModel(average);
    expectSeedControlsTheWind(average);

    const InMemoryPreferences    preferences;
    MultiLevelPinkNoiseWindModel multiLevel(preferences);
    configureMultiLevelModel(multiLevel);
    expectSeedControlsTheWind(multiLevel);
}

// WindModelSeedReproducibilityTest.testClonedWindModelsDoNotShareListeners
TEST(WindModelSeedReproducibility, ClonedWindModelsDoNotShareListeners)
{
    PinkNoiseWindModel average;
    configureAverageModel(average);
    const ChangeCounter averageEvents(average.changed());
    PinkNoiseWindModel  averageClone(average);
    averageClone.setDirection(1.25);
    EXPECT_EQ(averageEvents.count(), 0)
        << "editing a clone must not notify the original average model";

    const InMemoryPreferences    preferences;
    MultiLevelPinkNoiseWindModel multiLevel(preferences);
    configureMultiLevelModel(multiLevel);
    const ChangeCounter          multiLevelEvents(multiLevel.changed());
    MultiLevelPinkNoiseWindModel multiLevelClone(multiLevel);
    multiLevelClone.getLevels()[0]->setDirection(1.25);
    EXPECT_EQ(multiLevelEvents.count(), 0)
        << "editing a clone must not notify the original multi-level model";
}

// WindModelSeedReproducibilityTest.testClonedSimulationOptionsForwardWindChangesToTheCopy
TEST(WindModelSeedReproducibility, ClonedSimulationOptionsForwardWindChangesToTheCopy)
{
    SimulationOptions   original;
    const ChangeCounter originalEvents(original.changed());

    SimulationOptions   copy = original;
    const ChangeCounter copyEvents(copy.changed());

    PinkNoiseWindModel& averageCopy = copy.getAverageWindModel();
    averageCopy.setDirection(averageCopy.getDirection() + 0.25);
    EXPECT_EQ(copyEvents.count(), 1) << "average-wind edits must notify cloned options";
    EXPECT_EQ(originalEvents.count(), 0) << "average-wind edits must not notify original options";

    MultiLevelPinkNoiseWindModel&                 multiLevelCopy = copy.getMultiLevelWindModel();
    MultiLevelPinkNoiseWindModel::LevelWindModel* level          = multiLevelCopy.getLevels()[0];
    level->setDirection(level->getDirection() + 0.25);
    EXPECT_EQ(copyEvents.count(), 2) << "multi-level wind edits must notify cloned options";
    EXPECT_EQ(originalEvents.count(), 0)
        << "multi-level wind edits must not notify original options";
}

// ============================================================== toSimulationConditions()
//
// The values are what OpenRocket's conditions give for the same options
// (probes/tier8b-status/ConditionsProbe.java, "toSimulationConditions").

/// ConditionsProbe.explicit(): options with every value that reaches the conditions set, not
/// launching into the wind.
[[nodiscard]] SimulationOptions explicitOptions()
{
    SimulationOptions o;
    o.setLaunchIntoWind(false);
    o.setLaunchRodLength(1.2);
    o.setLaunchRodAngle(0.1);
    o.setLaunchRodDirection(1.0);
    o.setLaunchLatitude(45);
    o.setLaunchLongitude(10);
    o.setIsaAtmosphere(true);
    o.setLaunchAltitude(100);
    o.setGeodeticComputation(GeodeticComputationStrategy::WGS84);
    o.setTimeStep(0.02);
    o.setMaxSimulationTime(300);
    o.setMaximumStepAngle(0.1);
    o.setRandomSeedFixed(true);
    o.setRandomSeed(42);
    o.setGravityModelType(GravityModelType::WGS);
    o.setConstantGravity(9.5);
    o.setWindModelType(WindModelType::AVERAGE);
    o.getAverageWindModel().setAverage(3);
    o.getAverageWindModel().setStandardDeviation(0.5);
    o.getAverageWindModel().setDirection(2.0);
    o.getMultiLevelWindModel().clearLevels();
    addLevel(o.getMultiLevelWindModel(), 0, 4.0, 0.5, 0.0);
    addLevel(o.getMultiLevelWindModel(), 1000, 8.0, 1.5, 0.0);
    o.setRecoverySpeedWarning(21);
    o.setDrogueLowSpeedWarning(4);
    o.setRecoveryDrogueMainHighSpeedWarning(31);
    o.setRecoveryDrogueMainLowSpeedWarning(16);
    return o;
}

/// The conditions of @p options, which must be made.
[[nodiscard]] SimulationConditions conditionsOf(const SimulationOptions& options)
{
    Result<SimulationConditions> conditions = options.toSimulationConditions();
    if (!conditions.has_value())
    {
        ADD_FAILURE() << conditions.error().toString();
        return {};
    }
    return std::move(*conditions);
}

/// A wind velocity of the probe: the time, the altitude, and Java's x and y (z is 0).
struct WindPin
{
    constexpr WindPin(double windTime, double windAltitude, double windX, double windY) noexcept
      : time(windTime), altitude(windAltitude), x(windX), y(windY)
    {
    }

    double time;
    double altitude;
    double x;
    double y;
};

/// The differences between the wind of @p conditions and Java's, asked in the order of @p pins.
[[nodiscard]] std::string windDifferences(const SimulationConditions& conditions,
                                          std::span<const WindPin>    pins)
{
    JavaValueDifferences differences;
    for (const WindPin& pin : pins)
    {
        const Coordinate wind = conditions.getWindModel()->getWindVelocity(pin.time, pin.altitude);
        differences.coordinate(std::format("wind({}, {})", pin.time, pin.altitude),
                               Coordinate{pin.x, pin.y, 0}, wind);
    }
    return differences.text();
}

/// The atmosphere of the probe at one altitude: Java's temperature, pressure and humidity.
struct AtmospherePin
{
    constexpr AtmospherePin(double pinAltitude, double pinTemperature, double pinPressure,
                            double pinHumidity) noexcept
      : altitude(pinAltitude),
        temperature(pinTemperature),
        pressure(pinPressure),
        humidity(pinHumidity)
    {
    }

    double altitude;
    double temperature;
    double pressure;
    double humidity;
};

/// The differences between the atmosphere of @p conditions and Java's.
[[nodiscard]] std::string atmosphereDifferences(const SimulationConditions&    conditions,
                                                std::span<const AtmospherePin> pins)
{
    JavaValueDifferences differences;
    for (const AtmospherePin& pin : pins)
    {
        const QtRocket::AtmosphericConditions air =
            conditions.getAtmosphericModel()->getConditions(pin.altitude);
        differences.number(std::format("temperature({})", pin.altitude), pin.temperature,
                           air.getTemperature());
        differences.number(std::format("pressure({})", pin.altitude), pin.pressure,
                           air.getPressure());
        differences.number(std::format("humidity({})", pin.altitude), pin.humidity,
                           air.getRelativeHumidity());
    }
    return differences.text();
}

TEST(SimulationOptionsToConditions, TheValuesOfTheOptionsReachTheConditions)
{
    // "-- explicit": "rod 1.2 0.1 1.0", "site 45.0 10.0 100.0 geodetic WGS84", "position (0.0,
    // 0.0, 0.0) velocity (0.0, 0.0, 0.0)", "seed 42", "steps 0.02 300.0 0.1", "thresholds 21.0
    // 4.0 31.0 16.0", "listeners 0 simulation null modID invalid false"
    const SimulationOptions    o = explicitOptions();
    const SimulationConditions c = conditionsOf(o);
    EXPECT_EQ(c.getLaunchRodLength(), 1.2);
    EXPECT_EQ(c.getLaunchRodAngle(), 0.1);
    EXPECT_EQ(c.getLaunchRodDirection(), 1.0);
    EXPECT_TRUE(isJavaValue(45.0, c.getLaunchSite().getLatitudeDeg()));
    EXPECT_TRUE(isJavaValue(10.0, c.getLaunchSite().getLongitudeDeg()));
    EXPECT_EQ(c.getLaunchSite().getAltitude(), 100.0);
    EXPECT_EQ(c.getGeodeticComputation(), GeodeticComputationStrategy::WGS84);
    EXPECT_TRUE(c.getLaunchPosition().exactlyEquals(Coordinate::kNul));
    EXPECT_TRUE(c.getLaunchVelocity().exactlyEquals(Coordinate::kNul));
    EXPECT_EQ(c.getRandomSeed(), 42);
    EXPECT_EQ(c.getTimeStep(), 0.02);
    EXPECT_EQ(c.getMaxSimulationTime(), 300.0);
    EXPECT_EQ(c.getMaximumAngleStep(), 0.1);
    EXPECT_EQ(c.getRecoverySpeedWarning(), 21.0);
    EXPECT_EQ(c.getDrogueLowSpeedWarning(), 4.0);
    EXPECT_EQ(c.getRecoveryDrogueMainHighSpeedWarning(), 31.0);
    EXPECT_EQ(c.getRecoveryDrogueMainLowSpeedWarning(), 16.0);
    EXPECT_TRUE(c.getSimulationListenerList().empty());
    EXPECT_EQ(c.getSimulation(), nullptr);
    EXPECT_NE(c.getModId(), QtRocket::ModId::invalid());
    EXPECT_TRUE(c.getMassCalculator().has_value()) << "mass true";
}

TEST(SimulationOptionsToConditions, TheWindIsACloneOfTheModelInUse)
{
    // probes/tier8b-status/WindProbe.java: "wind PinkNoiseWindModel", "steady wind(t, 100) =
    // (2.727892280477045, -1.2484405096414273, 0.0)" at every time (the same at 600 m: the
    // average model does not look at the altitude), "steady 7.5 from 0.25: (1.8555296944089221,
    // 7.266843162829836, 0.0)". A steady wind: the turbulence of this port is not OpenRocket's
    // (see PinkNoise), so only the wind without it can be compared.
    SimulationOptions o = explicitOptions();
    o.getAverageWindModel().setStandardDeviation(0);
    const SimulationConditions c = conditionsOf(o);
    ASSERT_NE(c.getWindModel(), nullptr);
    EXPECT_NE(dynamic_cast<const PinkNoiseWindModel*>(c.getWindModel().get()), nullptr);
    EXPECT_NE(c.getWindModel().get(), &o.getAverageWindModel())
        << "the options' own wind model is not the conditions'";
    const std::array<WindPin, 5> pins{{
        {0.0, 100.0, 2.727892280477045, -1.2484405096414273},
        {0.0, 600.0, 2.727892280477045, -1.2484405096414273},
        {0.05, 100.0, 2.727892280477045, -1.2484405096414273},
        {1.0, 100.0, 2.727892280477045, -1.2484405096414273},
        {7.5, 600.0, 2.727892280477045, -1.2484405096414273},
    }};
    EXPECT_EQ(windDifferences(c, pins), "");

    o.getAverageWindModel().setDirection(0.25);
    o.getAverageWindModel().setAverage(7.5);
    const std::array<WindPin, 1> other{{{2.0, 100.0, 1.8555296944089221, 7.266843162829836}}};
    EXPECT_EQ(windDifferences(conditionsOf(o), other), "");
    // The conditions made before are not touched by what the options became.
    EXPECT_EQ(windDifferences(c, pins), "");
}

/// The turbulent wind of the conditions of @p options at 1 s and 100 m.
[[nodiscard]] Coordinate windOf(const SimulationOptions& options)
{
    return conditionsOf(options).getWindModel()->getWindVelocity(1.0, 100);
}

TEST(SimulationOptionsToConditions, TheSeedOfTheOptionsGovernsTheTurbulence)
{
    // WindProbe: "same seed same wind true, other seed same wind false", "the options' model
    // gives the conditions' wind false", "multi-level: same seed same wind true, other seed same
    // wind false". Every call gives a wind model of its own ("second call: wind same false"),
    // seeded with the seed of the options, whether or not that seed is fixed.
    SimulationOptions          o = explicitOptions();
    const SimulationConditions c = conditionsOf(o);
    EXPECT_NE(conditionsOf(o).getWindModel(), c.getWindModel());
    EXPECT_EQ(c.getRandomSeed(), 42);
    const Coordinate a = windOf(o);
    EXPECT_TRUE(a.exactlyEquals(windOf(o))) << "the same seed, the same wind";
    EXPECT_FALSE(a == o.getAverageWindModel().getWindVelocity(1.0, 100))
        << "the configured model has a seed of its own";

    o.setRandomSeed(43);
    EXPECT_FALSE(a == windOf(o)) << "another seed, another wind";
    o.setRandomSeedFixed(false);
    o.setRandomSeed(42);
    EXPECT_EQ(conditionsOf(o).getRandomSeed(), 42);
    EXPECT_TRUE(a.exactlyEquals(windOf(o))) << "a seed that is not fixed governs all the same";

    SimulationOptions m = explicitOptions();
    m.setWindModelType(WindModelType::MULTI_LEVEL);
    m.getMultiLevelWindModel().clearLevels();
    addLevel(m.getMultiLevelWindModel(), 0, 4.0, 0.5, 1.0);
    addLevel(m.getMultiLevelWindModel(), 1000, 8.0, 1.5, 2.0);
    const Coordinate levels = windOf(m);
    EXPECT_TRUE(levels.exactlyEquals(windOf(m)));
    m.setRandomSeed(43);
    EXPECT_FALSE(levels == windOf(m));
}

TEST(SimulationOptionsToConditions, TheConfiguredWindModelIsNotReseeded)
{
    // The seed goes to the throwaway clone: the options compare equal before and after, and
    // their own model still gives the wind of its own seed.
    SimulationOptions       o      = explicitOptions();
    const SimulationOptions before = o;
    const ChangeCounter     events(o.changed());
    static_cast<void>(conditionsOf(o));
    EXPECT_TRUE(o == before);
    EXPECT_EQ(events.count(), 0);
    PinkNoiseWindModel twin(before.getAverageWindModel());
    EXPECT_TRUE(o.getAverageWindModel().getWindVelocity(1.0, 100).exactlyEquals(
        twin.getWindVelocity(1.0, 100)));
}

TEST(SimulationOptionsToConditions, TheMultiLevelWindModelIsUsedWhenItIsChosen)
{
    // "-- multi-level wind": "wind MultiLevelPinkNoiseWindModel", "wind(t, 100.0) =
    // (2.5239279282583746, 3.2158869841395044, 0.0)", "wind(t, 600.0) = (5.555056797466186,
    // 1.7436706670295705, 0.0)" at every time (the levels have no turbulence)
    SimulationOptions o = explicitOptions();
    o.setWindModelType(WindModelType::MULTI_LEVEL);
    const SimulationConditions c = conditionsOf(o);
    ASSERT_NE(c.getWindModel(), nullptr);
    EXPECT_NE(dynamic_cast<const MultiLevelPinkNoiseWindModel*>(c.getWindModel().get()), nullptr);
    EXPECT_NE(c.getWindModel().get(), &o.getMultiLevelWindModel());
    const std::array<WindPin, 4> pins{{
        {0.0, 100.0, 2.5239279282583746, 3.2158869841395044},
        {0.0, 600.0, 5.555056797466186, 1.7436706670295705},
        {7.5, 100.0, 2.5239279282583746, 3.2158869841395044},
        {7.5, 600.0, 5.555056797466186, 1.7436706670295705},
    }};
    EXPECT_EQ(windDifferences(c, pins), "");
    EXPECT_EQ(c.getLaunchRodDirection(), 1.0) << "not launching into the wind";
}

TEST(SimulationOptionsToConditions, LaunchingIntoTheWindTurnsTheRodToTheWind)
{
    // "-- launch into wind (average, direction 2.0)": "rod 1.2 0.1 2.0"; "direction 7.0: rod
    // 0.7168146928204138", "direction -1.0: rod 5.283185307179586" (reduced to 0 ... 2 pi)
    SimulationOptions o = explicitOptions();
    o.setLaunchIntoWind(true);
    EXPECT_EQ(conditionsOf(o).getLaunchRodDirection(), 2.0);
    EXPECT_EQ(conditionsOf(o).getLaunchRodAngle(), 0.1);
    o.getAverageWindModel().setDirection(7.0);
    EXPECT_TRUE(isJavaValue(0.7168146928204138, conditionsOf(o).getLaunchRodDirection()));
    o.getAverageWindModel().setDirection(-1.0);
    EXPECT_TRUE(isJavaValue(5.283185307179586, conditionsOf(o).getLaunchRodDirection()));

    // "multi-level, into wind: rod 0.6654228956259569", "multi-level, into wind at 500 m: rod
    // 1.1801270895652323": the direction of the wind at the launch altitude.
    SimulationOptions m = explicitOptions();
    m.setWindModelType(WindModelType::MULTI_LEVEL);
    m.setLaunchIntoWind(true);
    EXPECT_TRUE(isJavaValue(0.6654228956259569, conditionsOf(m).getLaunchRodDirection()));
    m.setLaunchAltitude(500);
    EXPECT_TRUE(isJavaValue(1.1801270895652323, conditionsOf(m).getLaunchRodDirection()));
}

TEST(SimulationOptionsToConditions, TheIsaAtmosphereIsTheOneSharedModel)
{
    // "atmosphere ExtendedISAModel", "atmosphere(0.0) = 288.15 101325.0 0.0", "atmosphere(100.0)
    // = 287.5000511226052 100152.25761373011 0.0", "atmosphere(1000.0) = 281.6510223716947
    // 89876.28248259629 0.0", "atmosphere(11000.0) = 216.77351270445553 22699.952216044647 0.0"
    const SimulationOptions            o = explicitOptions();
    const SimulationConditions         c = conditionsOf(o);
    const std::array<AtmospherePin, 4> pins{{
        {0.0, 288.15, 101325.0, 0.0},
        {100.0, 287.5000511226052, 100152.25761373011, 0.0},
        {1000.0, 281.6510223716947, 89876.28248259629, 0.0},
        {11000.0, 216.77351270445553, 22699.952216044647, 0.0},
    }};
    ASSERT_NE(c.getAtmosphericModel(), nullptr);
    EXPECT_EQ(atmosphereDifferences(c, pins), "");
    // "second call: ... atmosphere same true": every simulation shares the ISA model.
    EXPECT_EQ(conditionsOf(o).getAtmosphericModel(), c.getAtmosphericModel());
    EXPECT_EQ(conditionsOf(SimulationOptions()).getAtmosphericModel(), c.getAtmosphericModel());
}

TEST(SimulationOptionsToConditions, ACustomAtmosphereIsFittedToTheLaunchConditions)
{
    // "-- custom atmosphere": "site 45.0 10.0 500.0", "atmosphere(0.0) = 303.96872058897577
    // 95237.19409860326 0.3", "atmosphere(500.0) = 300.0 90000.0 0.3", "atmosphere(2000.0) =
    // 288.097583035686 75609.44877343957 0.3", "atmosphere(11000.0) = 216.80083875985372
    // 22241.221297643588 0.3", "atmosphere(20000.0) = 216.65 5418.4244608909785 0.3", "gravity
    // WGSGravityModel 9.804660190551365"
    SimulationOptions o = explicitOptions();
    o.setIsaAtmosphere(false);
    o.setLaunchAltitude(500);
    o.setLaunchTemperature(300);
    o.setLaunchPressure(90000);
    o.setLaunchRelativeHumidity(0.3);
    const SimulationConditions         c = conditionsOf(o);
    const std::array<AtmospherePin, 5> pins{{
        {0.0, 303.96872058897577, 95237.19409860326, 0.3},
        {500.0, 300.0, 90000.0, 0.3},
        {2000.0, 288.097583035686, 75609.44877343957, 0.3},
        {11000.0, 216.80083875985372, 22241.221297643588, 0.3},
        {20000.0, 216.65, 5418.4244608909785, 0.3},
    }};
    ASSERT_NE(c.getAtmosphericModel(), nullptr);
    EXPECT_EQ(atmosphereDifferences(c, pins), "");
    EXPECT_EQ(c.getLaunchSite().getAltitude(), 500.0);
    EXPECT_TRUE(isJavaValue(9.804660190551365, c.getGravityModel()->getGravity(c.getLaunchSite())));
    // A model of its own each time.
    EXPECT_NE(conditionsOf(o).getAtmosphericModel(), c.getAtmosphericModel());
}

TEST(SimulationOptionsToConditions, ARefusedAtmosphereIsTheError)
{
    // "temperature -5: IllegalArgumentException: Temperature must be positive (Kelvin)"
    SimulationOptions o = explicitOptions();
    o.setIsaAtmosphere(false);
    o.setLaunchTemperature(-5);
    const Result<SimulationConditions> conditions = o.toSimulationConditions();
    ASSERT_FALSE(conditions.has_value());
    EXPECT_EQ(conditions.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(conditions.error().message, "Temperature must be positive (Kelvin)");
}

TEST(SimulationOptionsToConditions, TheGravityModelIsANewModelOfTheChosenType)
{
    // "gravity WGSGravityModel 9.805891371098026", "-- constant gravity": "gravity
    // ConstantGravityModel 3.71", "second call: ... gravity same false"
    SimulationOptions          o = explicitOptions();
    const SimulationConditions c = conditionsOf(o);
    ASSERT_NE(c.getGravityModel(), nullptr);
    EXPECT_NE(dynamic_cast<const WgsGravityModel*>(c.getGravityModel().get()), nullptr);
    EXPECT_TRUE(isJavaValue(9.805891371098026, c.getGravityModel()->getGravity(c.getLaunchSite())));
    EXPECT_NE(conditionsOf(o).getGravityModel(), c.getGravityModel());

    o.setGravityModelType(GravityModelType::CONSTANT);
    o.setConstantGravity(3.71);
    const SimulationConditions constant = conditionsOf(o);
    ASSERT_NE(constant.getGravityModel(), nullptr);
    EXPECT_NE(dynamic_cast<const ConstantGravityModel*>(constant.getGravityModel().get()), nullptr);
    EXPECT_EQ(constant.getGravityModel()->getGravity(constant.getLaunchSite()), 3.71);
}

TEST(SimulationOptionsToConditions, TheLaunchSiteIsClampedAsAWorldCoordinate)
{
    // "clamped site 90.0 -180.0": the options clamp the latitude and the longitude.
    SimulationOptions o = explicitOptions();
    o.setLaunchLatitude(123);
    o.setLaunchLongitude(-400);
    const SimulationConditions c = conditionsOf(o);
    EXPECT_TRUE(isJavaValue(90.0, c.getLaunchSite().getLatitudeDeg()));
    EXPECT_TRUE(isJavaValue(-180.0, c.getLaunchSite().getLongitudeDeg()));
}

TEST(SimulationOptionsToConditions, TheCalculatorIsANewBarrowmanCalculator)
{
    // "aero BarrowmanCalculator stall 0.30543261909900765", "second call: ... aero same false"
    const SimulationOptions    o = explicitOptions();
    const SimulationConditions c = conditionsOf(o);
    ASSERT_NE(c.getAerodynamicCalculator(), nullptr);
    EXPECT_NE(
        dynamic_cast<const QtRocket::BarrowmanCalculator*>(c.getAerodynamicCalculator().get()),
        nullptr);
    EXPECT_EQ(c.getAerodynamicCalculator()->getStallAngle(), 0.30543261909900765);
    EXPECT_NE(conditionsOf(o).getAerodynamicCalculator(), c.getAerodynamicCalculator());

    // Without lookup tables it is the Barrowman calculation.
    QtRocket::Test::TestEstesAlphaIII    alpha;
    const QtRocket::FlightConfiguration& config =
        alpha.rocket->getFlightConfiguration(QtRocket::Test::testFcid(0));
    const QtRocket::FlightConditions flight(config);
    QtRocket::BarrowmanCalculator    barrowman;
    EXPECT_EQ(c.getAerodynamicCalculator()->getCP(config, flight, nullptr).x,
              barrowman.getCP(config, flight, nullptr).x);
    EXPECT_EQ(c.getAerodynamicCalculator()->getAerodynamicForces(config, flight, nullptr).getCD(),
              barrowman.getAerodynamicForces(config, flight, nullptr).getCD());
}

TEST(SimulationOptionsToConditions, ALookupTableReplacesItsHalfOfTheCalculation)
{
    // Java: new BarrowmanCalculator(stabilityLookupTable != null ? new
    // LookupTableStabilityCalculator(...) : new BarrowmanStabilityCalculator(), dragLookupTable
    // != null ? new LookupTableDragCalculator(...) : new BarrowmanDragCalculator()).
    QtRocket::Test::TestEstesAlphaIII    alpha;
    const QtRocket::FlightConfiguration& config =
        alpha.rocket->getFlightConfiguration(QtRocket::Test::testFcid(0));
    const QtRocket::FlightConditions flight(config);
    QtRocket::BarrowmanCalculator    barrowman;
    const double                     barrowmanCp = barrowman.getCP(config, flight, nullptr).x;
    const double barrowmanCd = barrowman.getAerodynamicForces(config, flight, nullptr).getCD();

    // Both tables: the CP and the drag are the tables' (0.125 everywhere).
    const SimulationOptions    both = lookupOptions("0.125");
    const SimulationConditions c    = conditionsOf(both);
    EXPECT_EQ(c.getAerodynamicCalculator()->getCP(config, flight, nullptr).x, 0.125);
    EXPECT_EQ(c.getAerodynamicCalculator()->getAerodynamicForces(config, flight, nullptr).getCD(),
              0.125);

    // The drag table only.
    SimulationOptions drag = lookupOptions("0.125");
    drag.clearStabilityLookup();
    const SimulationConditions d = conditionsOf(drag);
    EXPECT_EQ(d.getAerodynamicCalculator()->getCP(config, flight, nullptr).x, barrowmanCp);
    EXPECT_EQ(d.getAerodynamicCalculator()->getAerodynamicForces(config, flight, nullptr).getCD(),
              0.125);

    // The stability table only.
    SimulationOptions stability = lookupOptions("0.125");
    stability.clearDragLookup();
    const SimulationConditions t = conditionsOf(stability);
    EXPECT_EQ(t.getAerodynamicCalculator()->getCP(config, flight, nullptr).x, 0.125);
    EXPECT_EQ(t.getAerodynamicCalculator()->getAerodynamicForces(config, flight, nullptr).getCD(),
              barrowmanCd);
    EXPECT_NE(barrowmanCp, 0.125);
    EXPECT_NE(barrowmanCd, 0.125);
}

TEST(SimulationOptionsToConditions, TheDefaultOptionsGiveConditions)
{
    // The built-in defaults: a vertical rod of 1 m, the rod direction of the calm wind (launching
    // into the wind from the east: pi / 2), the default launch site, the stepper's
    // recommendations, and the seed the options drew.
    const SimulationOptions    o;
    const SimulationConditions c = conditionsOf(o);
    EXPECT_EQ(c.getLaunchRodLength(), 1.0);
    EXPECT_EQ(c.getLaunchRodAngle(), 0.0);
    EXPECT_EQ(c.getLaunchRodDirection(), o.getLaunchRodDirection());
    EXPECT_EQ(c.getLaunchSite(),
              QtRocket::WorldCoordinate(o.getLaunchLatitude(), o.getLaunchLongitude(), 0));
    EXPECT_EQ(c.getGeodeticComputation(), GeodeticComputationStrategy::SPHERICAL);
    EXPECT_EQ(c.getRandomSeed(), o.getRandomSeed());
    EXPECT_EQ(c.getTimeStep(), 0.05);
    EXPECT_EQ(c.getMaxSimulationTime(), 1200.0);
    EXPECT_EQ(c.getMaximumAngleStep(), 3 * kPi / 180);
    EXPECT_EQ(c.getRecoverySpeedWarning(), 20.0);
    EXPECT_EQ(c.getDrogueLowSpeedWarning(), 3.048);
    EXPECT_EQ(c.getRecoveryDrogueMainHighSpeedWarning(), 30.48);
    EXPECT_EQ(c.getRecoveryDrogueMainLowSpeedWarning(), 15.24);
    EXPECT_TRUE(c.getWindModel()->getWindVelocity(0, 0).exactlyEquals(Coordinate{0, 0, 0}))
        << "calm";
}

}  // namespace
