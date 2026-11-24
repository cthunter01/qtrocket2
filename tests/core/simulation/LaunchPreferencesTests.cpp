#include "QtRocket/simulation/LaunchPreferences.h"

#include <cstddef>
#include <numbers>
#include <type_traits>

#include <gtest/gtest.h>

#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/simulation/DefaultSimulationOptionFactory.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationOptionsInterface.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/MathUtil.h"
#include "simulation/SimulationOptionsSupport.h"

namespace
{

using QtRocket::DefaultSimulationOptionFactory;
using QtRocket::GeodeticComputationStrategy;
using QtRocket::InMemoryPreferences;
using QtRocket::LaunchPreferences;
using QtRocket::PinkNoiseWindModel;
using QtRocket::SimulationOptions;
using QtRocket::SimulationOptionsInterface;
using QtRocket::Test::ChangeCounter;
using QtRocket::Test::isJavaValue;

namespace Keys = QtRocket::PreferenceKeys;

constexpr double kPi = std::numbers::pi;

static_assert(std::is_base_of_v<SimulationOptionsInterface, LaunchPreferences>);
static_assert(std::is_final_v<LaunchPreferences>);
static_assert(!std::is_copy_constructible_v<LaunchPreferences>);
static_assert(!std::is_move_constructible_v<LaunchPreferences>);

// =================================================== Port of LaunchPreferencesPanelTest
//
// LaunchPreferencesPanelTest.DefaultsFromLaunchPreferences (swing
// gui/dialogs/preferences/LaunchPreferencesPanelTest.java): "the panel binds its controls
// directly to the preferences, so editing the panel is simulated by writing the preferences",
// the wind through prefs.getAverageWindModel(). The panel's target is a LaunchPreferences here,
// so the edits go through it, and the defaults are DefaultSimulationOptionFactory's. Its
// testPanelCreation builds the Swing panel: it belongs to the GUI's Launch preference page.

/// LaunchPreferencesPanelTest.EPSILON
constexpr double kEpsilon = QtRocket::MathUtil::kEpsilon;

constexpr double kLatitude       = 50.8791;  // Leuven
constexpr double kLongitude      = 4.7025;
constexpr double kAltitude       = 10.0;  // m
constexpr bool   kIsaAtmosphere  = false;
constexpr double kTemperature    = 293.15;    // K
constexpr double kPressure       = 101000.0;  // Pa
constexpr bool   kLaunchIntoWind = false;
constexpr double kRodLength      = 1.5;      // m
constexpr double kRodAngle       = 0.1;      // rad
constexpr double kRodDirection   = kPi / 4;  // rad
constexpr double kWindAverage    = 3.0;      // m/s
constexpr double kWindTurbulence = 0.15;

/// The nested class DefaultsFromLaunchPreferences. Java clears the one preferences node its
/// tests share before each of them; here each test has a store of its own.
class LaunchPreferencesPanelTestDefaultsFromLaunchPreferences : public ::testing::Test
{
protected:
    [[nodiscard]] const SimulationOptions& defaults() const noexcept { return m_defaults; }
    [[nodiscard]] double windStandardDeviation() const noexcept { return m_windStandardDeviation; }

private:
    /// Java's @BeforeEach editLaunchPreferences(): the edits, then factory.getDefault().
    [[nodiscard]] SimulationOptions editLaunchPreferences()
    {
        m_launch.setLaunchLatitude(kLatitude);
        m_launch.setLaunchLongitude(kLongitude);
        m_launch.setIsaAtmosphere(kIsaAtmosphere);
        m_launch.setLaunchAltitude(kAltitude);
        m_launch.setLaunchTemperature(kTemperature);
        m_launch.setLaunchPressure(kPressure);
        m_launch.setLaunchIntoWind(kLaunchIntoWind);
        m_launch.setLaunchRodLength(kRodLength);
        m_launch.setLaunchRodAngle(kRodAngle);
        m_launch.setLaunchRodDirection(kRodDirection);
        m_launch.getAverageWindModel().setAverage(kWindAverage);
        m_launch.getAverageWindModel().setTurbulenceIntensity(kWindTurbulence);
        m_windStandardDeviation = m_launch.getAverageWindModel().getStandardDeviation();

        const DefaultSimulationOptionFactory factory(m_preferences);
        return factory.getDefault();
    }

    InMemoryPreferences m_preferences;
    LaunchPreferences   m_launch{m_preferences};
    /// The standard deviation is derived from the turbulence intensity and the average.
    double            m_windStandardDeviation{0};
    SimulationOptions m_defaults{editLaunchPreferences()};
};

using LaunchDefaults = LaunchPreferencesPanelTestDefaultsFromLaunchPreferences;

// LaunchPreferencesPanelTest.DefaultsFromLaunchPreferences.usesLatitude
TEST_F(LaunchDefaults, UsesLatitude)
{
    EXPECT_NEAR(kLatitude, defaults().getLaunchLatitude(), kEpsilon);
}

// LaunchPreferencesPanelTest.DefaultsFromLaunchPreferences.usesLongitude
TEST_F(LaunchDefaults, UsesLongitude)
{
    EXPECT_NEAR(kLongitude, defaults().getLaunchLongitude(), kEpsilon);
}

// LaunchPreferencesPanelTest.DefaultsFromLaunchPreferences.usesAltitude
TEST_F(LaunchDefaults, UsesAltitude)
{
    EXPECT_NEAR(kAltitude, defaults().getLaunchAltitude(), kEpsilon);
}

// LaunchPreferencesPanelTest.DefaultsFromLaunchPreferences.usesISAAtmosphereFlag
TEST_F(LaunchDefaults, UsesIsaAtmosphereFlag)
{
    EXPECT_EQ(kIsaAtmosphere, defaults().isIsaAtmosphere());
}

// LaunchPreferencesPanelTest.DefaultsFromLaunchPreferences.usesTemperature
TEST_F(LaunchDefaults, UsesTemperature)
{
    EXPECT_NEAR(kTemperature, defaults().getLaunchTemperature(), kEpsilon);
}

// LaunchPreferencesPanelTest.DefaultsFromLaunchPreferences.usesPressure
TEST_F(LaunchDefaults, UsesPressure)
{
    EXPECT_NEAR(kPressure, defaults().getLaunchPressure(), kEpsilon);
}

// LaunchPreferencesPanelTest.DefaultsFromLaunchPreferences.usesLaunchIntoWind
TEST_F(LaunchDefaults, UsesLaunchIntoWind)
{
    EXPECT_EQ(kLaunchIntoWind, defaults().getLaunchIntoWind());
}

// LaunchPreferencesPanelTest.DefaultsFromLaunchPreferences.usesRodLength
TEST_F(LaunchDefaults, UsesRodLength)
{
    EXPECT_NEAR(kRodLength, defaults().getLaunchRodLength(), kEpsilon);
}

// LaunchPreferencesPanelTest.DefaultsFromLaunchPreferences.usesRodAngle
TEST_F(LaunchDefaults, UsesRodAngle)
{
    EXPECT_NEAR(kRodAngle, defaults().getLaunchRodAngle(), kEpsilon);
}

// LaunchPreferencesPanelTest.DefaultsFromLaunchPreferences.usesRodDirection
TEST_F(LaunchDefaults, UsesRodDirection)
{
    EXPECT_NEAR(kRodDirection, defaults().getLaunchRodDirection(), kEpsilon);
}

// LaunchPreferencesPanelTest.DefaultsFromLaunchPreferences.usesWindAverage
TEST_F(LaunchDefaults, UsesWindAverage)
{
    EXPECT_NEAR(kWindAverage, defaults().getAverageWindModel().getAverage(), kEpsilon);
}

// LaunchPreferencesPanelTest.DefaultsFromLaunchPreferences.usesWindStandardDeviation
TEST_F(LaunchDefaults, UsesWindStandardDeviation)
{
    EXPECT_NEAR(windStandardDeviation(), defaults().getAverageWindModel().getStandardDeviation(),
                kEpsilon);
}

// LaunchPreferencesPanelTest.DefaultsFromLaunchPreferences.usesWindTurbulenceIntensity
TEST_F(LaunchDefaults, UsesWindTurbulenceIntensity)
{
    EXPECT_NEAR(kWindTurbulence, defaults().getAverageWindModel().getTurbulenceIntensity(),
                kEpsilon);
}

// ================================================================== beyond the JUnit test

TEST(LaunchPreferences, TheValuesAreThePreferences)
{
    InMemoryPreferences preferences;
    LaunchPreferences   launch(preferences);
    EXPECT_EQ(&launch.getPreferences(), &preferences);

    // The defaults of the store: ApplicationPreferences'.
    EXPECT_EQ(launch.getLaunchRodLength(), 1.0);
    EXPECT_TRUE(launch.getLaunchIntoWind());
    EXPECT_EQ(launch.getLaunchRodAngle(), 0.0);
    EXPECT_EQ(launch.getLaunchLatitude(), 28.61);
    EXPECT_EQ(launch.getLaunchLongitude(), -80.60);
    EXPECT_EQ(launch.getLaunchAltitude(), 0.0);
    EXPECT_TRUE(launch.isIsaAtmosphere());
    EXPECT_EQ(launch.getGeodeticComputation(), GeodeticComputationStrategy::SPHERICAL);

    // What the store holds is what the getters give, and what the setters write is stored.
    preferences.setLaunchRodLength(2.5);
    EXPECT_EQ(launch.getLaunchRodLength(), 2.5);
    launch.setLaunchRodAngle(0.25);
    EXPECT_EQ(preferences.getLaunchRodAngle(), 0.25);
    launch.setLaunchLatitude(45.5);
    EXPECT_EQ(preferences.getLaunchLatitude(), 45.5);
    launch.setLaunchLongitude(12.25);
    EXPECT_EQ(preferences.getLaunchLongitude(), 12.25);
    launch.setIsaAtmosphere(false);
    EXPECT_FALSE(preferences.isIsaAtmosphere());
    launch.setLaunchTemperature(301.5);
    EXPECT_EQ(preferences.getLaunchTemperature(), 301.5);
    launch.setLaunchPressure(98765.0);
    EXPECT_EQ(preferences.getLaunchPressure(), 98765.0);
    launch.setLaunchRelativeHumidity(0.4);
    EXPECT_EQ(preferences.getLaunchRelativeHumidity(), 0.4);
    launch.setLaunchAltitude(321.0);
    EXPECT_EQ(preferences.getLaunchAltitude(), 321.0);
    launch.setLaunchIntoWind(false);
    launch.setLaunchRodDirection(1.75);
    EXPECT_EQ(preferences.getDouble(Keys::kLaunchRodDirection, -1), 1.75);
    EXPECT_EQ(launch.getLaunchRodDirection(), 1.75);
}

TEST(LaunchPreferences, TheSettersFollowThePreferencesRules)
{
    InMemoryPreferences preferences;
    LaunchPreferences   launch(preferences);
    // The rod angle, the latitude and the longitude are clamped, the direction reduced.
    launch.setLaunchRodAngle(2.0);
    EXPECT_EQ(launch.getLaunchRodAngle(), SimulationOptions::kMaxLaunchRodAngle);
    launch.setLaunchLatitude(100);
    EXPECT_EQ(launch.getLaunchLatitude(), 90.0);
    launch.setLaunchLongitude(-200);
    EXPECT_EQ(launch.getLaunchLongitude(), -180.0);
    launch.setLaunchIntoWind(false);
    launch.setLaunchRodDirection(-kPi / 2);
    EXPECT_TRUE(isJavaValue(3 * kPi / 2, launch.getLaunchRodDirection()));
    // With the ISA the launch conditions follow the altitude (OptionsProbe: "ISA 1500").
    launch.setLaunchAltitude(1500);
    EXPECT_TRUE(isJavaValue(278.4023001554197, launch.getLaunchTemperature()));
    EXPECT_TRUE(isJavaValue(84559.67285449419, launch.getLaunchPressure()));
}

TEST(LaunchPreferences, LaunchingIntoTheWindSyncsTheRodDirection)
{
    // Java's ApplicationPreferences.getLaunchRodDirection() (OptionsProbe: "into wind: rod
    // direction = 4.0 wind 4.0 pref rod raw 4.0 pref events 1").
    InMemoryPreferences preferences;
    preferences.putDouble(Keys::kWindDirection, 4.0);
    preferences.putDouble(Keys::kLaunchRodDirection, 1.0);
    const LaunchPreferences launch(preferences);
    EXPECT_EQ(launch.getAverageWindModel().getDirection(), 4.0);
    const ChangeCounter events(preferences.changed());
    EXPECT_EQ(launch.getLaunchRodDirection(), 4.0);
    EXPECT_EQ(preferences.getDouble(Keys::kLaunchRodDirection, -1), 4.0);
    EXPECT_EQ(events.count(), 1);
}

TEST(LaunchPreferences, TheWindIsLoadedFromTheWindKeys)
{
    InMemoryPreferences preferences;
    preferences.putDouble(Keys::kWindAverage, 4.0);
    preferences.putDouble(Keys::kWindTurbulence, 0.25);
    preferences.putDouble(Keys::kWindDirection, 1.2);
    const LaunchPreferences   launch(preferences);
    const PinkNoiseWindModel& wind = launch.getAverageWindModel();
    EXPECT_EQ(wind.getAverage(), 4.0);
    EXPECT_EQ(wind.getStandardDeviation(), 1.0);
    EXPECT_EQ(wind.getTurbulenceIntensity(), 0.25);
    EXPECT_EQ(wind.getDirection(), 1.2);
}

TEST(LaunchPreferences, AnEmptyStoreStaysEmpty)
{
    // The defaults of the wind keys, 2 m/s from the east with an intensity of 0.1, describe the
    // model they load: nothing is written.
    InMemoryPreferences     preferences;
    const LaunchPreferences launch(preferences);
    EXPECT_EQ(launch.getAverageWindModel().getAverage(), 2.0);
    EXPECT_EQ(launch.getAverageWindModel().getStandardDeviation(), 0.2);
    EXPECT_EQ(launch.getAverageWindModel().getDirection(), kPi / 2);
    EXPECT_TRUE(preferences.empty());
}

TEST(LaunchPreferences, LoadingTheWindStoresItAsJavaDoes)
{
    // An intensity of 0.1 at 3 m/s is a deviation of 0.30000000000000004 m/s, whose intensity is
    // 0.10000000000000002: Java's preferences store that when they first load the wind
    // (FixProbe: WindTurbulence 0.10000000000000002 after getDefault()).
    InMemoryPreferences preferences;
    preferences.putDouble(Keys::kWindAverage, 3.0);
    preferences.putDouble(Keys::kWindTurbulence, 0.1);
    const LaunchPreferences launch(preferences);
    EXPECT_EQ(launch.getAverageWindModel().getStandardDeviation(), 0.30000000000000004);
    EXPECT_EQ(preferences.getWindTurbulenceIntensity(), 0.10000000000000002);
}

TEST(LaunchPreferences, AChangeOfTheWindIsStoredAndAnnounced)
{
    InMemoryPreferences preferences;
    LaunchPreferences   launch(preferences);
    const ChangeCounter events(launch.changed());
    const ChangeCounter windEvents(launch.getAverageWindModel().changed());

    launch.getAverageWindModel().setAverage(5.0);
    // Java's stateChanged(): the three keys. The intensity is kept, so the deviation follows.
    EXPECT_EQ(preferences.getWindAverage(), 5.0);
    EXPECT_EQ(preferences.getWindTurbulenceIntensity(), 0.1);
    EXPECT_EQ(launch.getAverageWindModel().getStandardDeviation(), 0.5);
    EXPECT_GE(windEvents.count(), 1);
    // The preferences announce the changed key, and this object passes it on.
    EXPECT_EQ(events.count(), 1);

    launch.getAverageWindModel().setStandardDeviation(1.5);
    EXPECT_EQ(preferences.getWindTurbulenceIntensity(), 0.3);
    EXPECT_EQ(events.count(), 2);
    launch.getAverageWindModel().setDirection(1.0);
    EXPECT_EQ(preferences.getWindDirection(), 1.0);
    EXPECT_EQ(events.count(), 3);

    // The keys describe the model, so the next simulation starts with it.
    const SimulationOptions defaults = DefaultSimulationOptionFactory(preferences).getDefault();
    EXPECT_EQ(defaults.getAverageWindModel().getAverage(), 5.0);
    EXPECT_TRUE(isJavaValue(1.5, defaults.getAverageWindModel().getStandardDeviation()));
    EXPECT_EQ(defaults.getAverageWindModel().getDirection(), 1.0);
}

TEST(LaunchPreferences, EveryChangeOfThePreferencesIsPassedOn)
{
    // Java's panel listens to the ApplicationPreferences, which fire for any of their setters.
    InMemoryPreferences preferences;
    LaunchPreferences   launch(preferences);
    const ChangeCounter events(launch.changed());
    preferences.setTimeStep(0.02);
    EXPECT_EQ(events.count(), 1);
    launch.setLaunchRodLength(3.0);
    EXPECT_EQ(events.count(), 2);
    // A value that is already stored changes nothing.
    launch.setLaunchRodLength(3.0);
    EXPECT_EQ(events.count(), 2);
}

TEST(LaunchPreferences, LaunchIntoWindIsAnnouncedWhenItChanges)
{
    // Deviation: Java's setLaunchIntoWind() stores the flag silently (putBoolean()).
    InMemoryPreferences preferences;
    LaunchPreferences   launch(preferences);
    const ChangeCounter events(launch.changed());
    // Java writes the key also when it holds the value.
    launch.setLaunchIntoWind(true);
    EXPECT_EQ(preferences.get(Keys::kLaunchIntoWind), "true");
    EXPECT_EQ(events.count(), 0);
    launch.setLaunchIntoWind(false);
    EXPECT_FALSE(preferences.getLaunchIntoWind());
    EXPECT_FALSE(launch.getLaunchIntoWind());
    EXPECT_EQ(events.count(), 1);
}

TEST(LaunchPreferences, TheGeodeticComputationIsThePreferenceName)
{
    // Deviation: announced when it changes (Java: putEnum(), silent).
    InMemoryPreferences preferences;
    LaunchPreferences   launch(preferences);
    const ChangeCounter events(launch.changed());
    launch.setGeodeticComputation(GeodeticComputationStrategy::WGS84);
    EXPECT_EQ(preferences.getGeodeticComputationName(), "WGS84");
    EXPECT_EQ(launch.getGeodeticComputation(), GeodeticComputationStrategy::WGS84);
    EXPECT_EQ(events.count(), 1);
    launch.setGeodeticComputation(GeodeticComputationStrategy::WGS84);
    EXPECT_EQ(events.count(), 1);
    preferences.setGeodeticComputationName("FLAT");
    EXPECT_EQ(launch.getGeodeticComputation(), GeodeticComputationStrategy::FLAT);
}

TEST(LaunchPreferences, TheWindFollowsAnotherWriterOfTheWindKeys)
{
    // Deviation: DefaultSimulationOptionFactory::saveDefault() writes the wind keys through a
    // model of its own; the model here takes them, without writing them again.
    InMemoryPreferences preferences;
    LaunchPreferences   launch(preferences);
    const ChangeCounter windEvents(launch.getAverageWindModel().changed());
    const ChangeCounter events(launch.changed());

    SimulationOptions saved(preferences);
    saved.getAverageWindModel().setAverage(7.5);
    saved.getAverageWindModel().setStandardDeviation(1.1);
    saved.getAverageWindModel().setDirection(2.8);
    DefaultSimulationOptionFactory(preferences).saveDefault(saved);
    const double storedIntensity = preferences.getWindTurbulenceIntensity();

    const PinkNoiseWindModel& wind = launch.getAverageWindModel();
    EXPECT_EQ(wind.getAverage(), 7.5);
    EXPECT_TRUE(isJavaValue(1.1, wind.getStandardDeviation()));
    EXPECT_EQ(wind.getDirection(), 2.8);
    EXPECT_GE(windEvents.count(), 1);
    EXPECT_GE(events.count(), 1);
    // Nothing was written back.
    EXPECT_EQ(preferences.getWindTurbulenceIntensity(), storedIntensity);

    // Another change of the preferences loads nothing again.
    const int windEventsBefore = windEvents.count();
    preferences.setTimeStep(0.02);
    EXPECT_EQ(windEvents.count(), windEventsBefore);

    // And the model stores its own changes as before.
    launch.getAverageWindModel().setDirection(0.5);
    EXPECT_EQ(preferences.getWindDirection(), 0.5);
}

TEST(LaunchPreferences, TheWindFollowsAClearedStore)
{
    InMemoryPreferences preferences;
    preferences.putDouble(Keys::kWindAverage, 6.0);
    LaunchPreferences launch(preferences);
    EXPECT_EQ(launch.getAverageWindModel().getAverage(), 6.0);
    preferences.clear();
    // clear() announces nothing; the next change of the preferences does.
    preferences.setTimeStep(0.02);
    EXPECT_EQ(launch.getAverageWindModel().getAverage(), 2.0);
    EXPECT_EQ(launch.getAverageWindModel().getDirection(), kPi / 2);
}

TEST(LaunchPreferences, TheLaunchConditionsCopyIntoSimulationOptionsAsTheFactoryCopiesThem)
{
    // Java's DefaultSimulationOptionFactory copies from the preferences through this interface;
    // copying from the adapter through it gives the same launch conditions.
    InMemoryPreferences preferences;
    QtRocket::Test::storeEverySimulationKey(preferences);
    LaunchPreferences       launch(preferences);
    const SimulationOptions defaults = DefaultSimulationOptionFactory(preferences).getDefault();

    SimulationOptions copied(preferences);
    copied.getAverageWindModel().setAverage(launch.getAverageWindModel().getAverage());
    copied.getAverageWindModel().setStandardDeviation(
        launch.getAverageWindModel().getStandardDeviation());
    copied.getAverageWindModel().setDirection(launch.getAverageWindModel().getDirection());
    copied.setLaunchLatitude(launch.getLaunchLatitude());
    copied.setLaunchLongitude(launch.getLaunchLongitude());
    copied.setIsaAtmosphere(launch.isIsaAtmosphere());
    copied.setLaunchAltitude(launch.getLaunchAltitude());
    copied.setLaunchTemperature(launch.getLaunchTemperature());
    copied.setLaunchPressure(launch.getLaunchPressure());
    copied.setLaunchRelativeHumidity(launch.getLaunchRelativeHumidity());
    copied.setLaunchIntoWind(launch.getLaunchIntoWind());
    copied.setLaunchRodLength(launch.getLaunchRodLength());
    copied.setLaunchRodAngle(launch.getLaunchRodAngle());
    copied.setLaunchRodDirection(launch.getLaunchRodDirection());

    EXPECT_EQ(copied.getAverageWindModel().getAverage(),
              defaults.getAverageWindModel().getAverage());
    EXPECT_EQ(copied.getAverageWindModel().getStandardDeviation(),
              defaults.getAverageWindModel().getStandardDeviation());
    EXPECT_EQ(copied.getAverageWindModel().getDirection(),
              defaults.getAverageWindModel().getDirection());
    EXPECT_EQ(copied.getLaunchLatitude(), defaults.getLaunchLatitude());
    EXPECT_EQ(copied.getLaunchLongitude(), defaults.getLaunchLongitude());
    EXPECT_EQ(copied.isIsaAtmosphere(), defaults.isIsaAtmosphere());
    EXPECT_EQ(copied.getLaunchAltitude(), defaults.getLaunchAltitude());
    EXPECT_EQ(copied.getLaunchTemperature(), defaults.getLaunchTemperature());
    EXPECT_EQ(copied.getLaunchPressure(), defaults.getLaunchPressure());
    EXPECT_EQ(copied.getLaunchRelativeHumidity(), defaults.getLaunchRelativeHumidity());
    EXPECT_EQ(copied.getLaunchIntoWind(), defaults.getLaunchIntoWind());
    EXPECT_EQ(copied.getLaunchRodLength(), defaults.getLaunchRodLength());
    EXPECT_EQ(copied.getLaunchRodAngle(), defaults.getLaunchRodAngle());
    EXPECT_EQ(copied.getLaunchRodDirection(), defaults.getLaunchRodDirection());
}

TEST(LaunchPreferences, ItLetsGoOfThePreferencesWhenItIsDestroyed)
{
    InMemoryPreferences preferences;
    const std::size_t   before = preferences.changed().size();
    {
        const LaunchPreferences launch(preferences);
        EXPECT_EQ(preferences.changed().size(), before + 1);
    }
    EXPECT_EQ(preferences.changed().size(), before);
    preferences.setTimeStep(0.02);
}

}  // namespace
