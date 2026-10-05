#include "QtRocket/simulation/DefaultSimulationOptionFactory.h"

#include <numbers>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/MathUtil.h"
#include "simulation/SimulationOptionsSupport.h"

namespace
{

using QtRocket::DefaultSimulationOptionFactory;
using QtRocket::GeodeticComputationStrategy;
using QtRocket::GravityModelType;
using QtRocket::InMemoryPreferences;
using QtRocket::PinkNoiseWindModel;
using QtRocket::SimulationOptions;
using QtRocket::SimulationStepperMethod;
using QtRocket::WindModelType;
using QtRocket::Test::ChangeCounter;
using QtRocket::Test::describe;
using QtRocket::Test::describeLevels;
using QtRocket::Test::isJavaValue;
using QtRocket::Test::storeEverySimulationKey;

namespace Keys = QtRocket::PreferenceKeys;

constexpr double kEpsilon = QtRocket::MathUtil::kEpsilon;
constexpr double kPi      = std::numbers::pi;

/// Launch conditions stored in the preferences, i.e. the input of getDefault().
constexpr bool   kPreferredIsaAtmosphere         = false;
constexpr double kPreferredLatitude              = 50.8791;  // Leuven
constexpr double kPreferredLongitude             = 4.7025;
constexpr double kPreferredAltitude              = 25.0;      // m
constexpr double kPreferredTemperature           = 293.15;    // K
constexpr double kPreferredPressure              = 100500.0;  // Pa
constexpr double kPreferredRelativeHumidity      = 0.65;
constexpr bool   kPreferredIntoWind              = false;
constexpr double kPreferredRodLength             = 1.8;  // m
constexpr double kPreferredRodAngle              = 0.2;  // rad
constexpr double kPreferredRodDirection          = 0.7;  // rad
constexpr double kPreferredWindAverage           = 4.2;  // m/s
constexpr double kPreferredWindStandardDeviation = 0.8;  // m/s
constexpr double kPreferredWindDirection         = 1.2;  // rad

/// Launch conditions of the simulation stored by saveDefault().
constexpr bool   kSavedIsaAtmosphere    = false;
constexpr double kSavedLatitude         = -33.8688;  // Sydney
constexpr double kSavedLongitude        = 151.2093;
constexpr double kSavedAltitude         = 80.0;     // m
constexpr double kSavedTemperature      = 301.15;   // K
constexpr double kSavedPressure         = 99000.0;  // Pa
constexpr double kSavedRelativeHumidity = 0.35;
constexpr bool   kSavedIntoWind         = false;
constexpr double kSavedRodLength        = 2.4;  // m
// rad, beyond the old launch preference limit of PI/6
constexpr double kSavedRodAngle              = 0.7;
constexpr double kSavedRodDirection          = 2.1;  // rad
constexpr double kSavedWindAverage           = 7.5;  // m/s
constexpr double kSavedWindStandardDeviation = 1.1;  // m/s
constexpr double kSavedWindDirection         = 2.8;  // rad

/// Obsolete preference key that the factory used to store the launch altitude under.
constexpr std::string_view kLegacySiteAltitudeKey = "SimConditionSiteAlt";

/// The fixture of DefaultSimulationOptionFactoryTest: Java's MockPreferences, which are also the
/// application preferences there, are an InMemoryPreferences store, and the factory and every
/// SimulationOptions the tests make read that store.
class DefaultSimulationOptionFactoryTest : public ::testing::Test
{
protected:
    /// Stores the kPreferred* launch conditions in the launch preferences.
    void storeLaunchPreferences()
    {
        m_preferences.setIsaAtmosphere(kPreferredIsaAtmosphere);
        m_preferences.setLaunchLatitude(kPreferredLatitude);
        m_preferences.setLaunchLongitude(kPreferredLongitude);
        m_preferences.setLaunchAltitude(kPreferredAltitude);
        m_preferences.setLaunchTemperature(kPreferredTemperature);
        m_preferences.setLaunchPressure(kPreferredPressure);
        m_preferences.setLaunchRelativeHumidity(kPreferredRelativeHumidity);
        m_preferences.setLaunchIntoWind(kPreferredIntoWind);
        m_preferences.setLaunchRodLength(kPreferredRodLength);
        m_preferences.setLaunchRodAngle(kPreferredRodAngle);
        m_preferences.setLaunchRodDirection(kPreferredRodDirection);
        // Java: preferences.getAverageWindModel().setAverage(...) and so on, on the model that
        // the preferences store on every change.
        PinkNoiseWindModel wind = preferencesWind();
        wind.setAverage(kPreferredWindAverage);
        wind.setStandardDeviation(kPreferredWindStandardDeviation);
        wind.setDirection(kPreferredWindDirection);
        wind.storeTo(m_preferences);
    }

    /// Simulation conditions holding the kSaved* launch conditions.
    [[nodiscard]] SimulationOptions savedSimulationOptions()
    {
        SimulationOptions options(m_preferences);
        options.setIsaAtmosphere(kSavedIsaAtmosphere);
        options.setLaunchLatitude(kSavedLatitude);
        options.setLaunchLongitude(kSavedLongitude);
        options.setLaunchAltitude(kSavedAltitude);
        options.setLaunchTemperature(kSavedTemperature);
        options.setLaunchPressure(kSavedPressure);
        options.setLaunchRelativeHumidity(kSavedRelativeHumidity);
        options.setLaunchIntoWind(kSavedIntoWind);
        options.setLaunchRodLength(kSavedRodLength);
        options.setLaunchRodAngle(kSavedRodAngle);
        options.setLaunchRodDirection(kSavedRodDirection);
        options.getAverageWindModel().setAverage(kSavedWindAverage);
        options.getAverageWindModel().setStandardDeviation(kSavedWindStandardDeviation);
        options.getAverageWindModel().setDirection(kSavedWindDirection);
        return options;
    }

    /// Java's preferences.getAverageWindModel(): the average wind the preferences hold.
    [[nodiscard]] PinkNoiseWindModel preferencesWind() const
    {
        PinkNoiseWindModel wind;
        wind.loadFrom(m_preferences);
        return wind;
    }

    // Protected, not private: the tests are subclasses of the fixture.
    InMemoryPreferences            m_preferences;
    DefaultSimulationOptionFactory m_factory{m_preferences};
};

/// The nested class GetDefault: "getDefault() loads the launch preferences".
class DefaultSimulationOptionFactoryGetDefault : public DefaultSimulationOptionFactoryTest
{
protected:
    [[nodiscard]] const SimulationOptions& defaults() const noexcept { return m_defaults; }

private:
    /// Java's @BeforeEach loadDefaults().
    [[nodiscard]] SimulationOptions loadDefaults()
    {
        storeLaunchPreferences();
        return m_factory.getDefault();
    }

    SimulationOptions m_defaults{loadDefaults()};
};

// DefaultSimulationOptionFactoryTest.GetDefault.loadsISAAtmosphere
TEST_F(DefaultSimulationOptionFactoryGetDefault, LoadsIsaAtmosphere)
{
    EXPECT_EQ(defaults().isIsaAtmosphere(), kPreferredIsaAtmosphere);
}

// DefaultSimulationOptionFactoryTest.GetDefault.loadsLatitude
TEST_F(DefaultSimulationOptionFactoryGetDefault, LoadsLatitude)
{
    EXPECT_NEAR(defaults().getLaunchLatitude(), kPreferredLatitude, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.GetDefault.loadsLongitude
TEST_F(DefaultSimulationOptionFactoryGetDefault, LoadsLongitude)
{
    EXPECT_NEAR(defaults().getLaunchLongitude(), kPreferredLongitude, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.GetDefault.loadsAltitude
TEST_F(DefaultSimulationOptionFactoryGetDefault, LoadsAltitude)
{
    EXPECT_NEAR(defaults().getLaunchAltitude(), kPreferredAltitude, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.GetDefault.loadsTemperature
TEST_F(DefaultSimulationOptionFactoryGetDefault, LoadsTemperature)
{
    EXPECT_NEAR(defaults().getLaunchTemperature(), kPreferredTemperature, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.GetDefault.loadsPressure
TEST_F(DefaultSimulationOptionFactoryGetDefault, LoadsPressure)
{
    EXPECT_NEAR(defaults().getLaunchPressure(), kPreferredPressure, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.GetDefault.loadsRelativeHumidity
TEST_F(DefaultSimulationOptionFactoryGetDefault, LoadsRelativeHumidity)
{
    EXPECT_NEAR(defaults().getLaunchRelativeHumidity(), kPreferredRelativeHumidity, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.GetDefault.loadsLaunchIntoWind
TEST_F(DefaultSimulationOptionFactoryGetDefault, LoadsLaunchIntoWind)
{
    EXPECT_EQ(defaults().getLaunchIntoWind(), kPreferredIntoWind);
}

// DefaultSimulationOptionFactoryTest.GetDefault.loadsRodLength
TEST_F(DefaultSimulationOptionFactoryGetDefault, LoadsRodLength)
{
    EXPECT_NEAR(defaults().getLaunchRodLength(), kPreferredRodLength, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.GetDefault.loadsRodAngle
TEST_F(DefaultSimulationOptionFactoryGetDefault, LoadsRodAngle)
{
    EXPECT_NEAR(defaults().getLaunchRodAngle(), kPreferredRodAngle, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.GetDefault.loadsRodDirection
TEST_F(DefaultSimulationOptionFactoryGetDefault, LoadsRodDirection)
{
    EXPECT_NEAR(defaults().getLaunchRodDirection(), kPreferredRodDirection, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.GetDefault.loadsWindAverage
TEST_F(DefaultSimulationOptionFactoryGetDefault, LoadsWindAverage)
{
    EXPECT_NEAR(defaults().getAverageWindModel().getAverage(), kPreferredWindAverage, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.GetDefault.loadsWindStandardDeviation
TEST_F(DefaultSimulationOptionFactoryGetDefault, LoadsWindStandardDeviation)
{
    EXPECT_NEAR(defaults().getAverageWindModel().getStandardDeviation(),
                kPreferredWindStandardDeviation, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.GetDefault.loadsWindDirection
TEST_F(DefaultSimulationOptionFactoryGetDefault, LoadsWindDirection)
{
    EXPECT_NEAR(defaults().getAverageWindModel().getDirection(), kPreferredWindDirection, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.GetDefault.ignoresObsoletePreferenceKeys
// ("ignores the obsolete SimCondition preference keys")
TEST_F(DefaultSimulationOptionFactoryGetDefault, IgnoresObsoletePreferenceKeys)
{
    m_preferences.putDouble(kLegacySiteAltitudeKey, kPreferredAltitude + 999.0);
    EXPECT_NEAR(m_factory.getDefault().getLaunchAltitude(), kPreferredAltitude, kEpsilon);
}

/// The nested class SaveDefault: "saveDefault() stores the launch preferences".
class DefaultSimulationOptionFactorySaveDefault : public DefaultSimulationOptionFactoryTest
{
protected:
    void SetUp() override { m_factory.saveDefault(savedSimulationOptions()); }
};

// DefaultSimulationOptionFactoryTest.SaveDefault.savesISAAtmosphere
TEST_F(DefaultSimulationOptionFactorySaveDefault, SavesIsaAtmosphere)
{
    EXPECT_EQ(m_preferences.isIsaAtmosphere(), kSavedIsaAtmosphere);
}

// DefaultSimulationOptionFactoryTest.SaveDefault.savesLatitude
TEST_F(DefaultSimulationOptionFactorySaveDefault, SavesLatitude)
{
    EXPECT_NEAR(m_preferences.getLaunchLatitude(), kSavedLatitude, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.SaveDefault.savesLongitude
TEST_F(DefaultSimulationOptionFactorySaveDefault, SavesLongitude)
{
    EXPECT_NEAR(m_preferences.getLaunchLongitude(), kSavedLongitude, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.SaveDefault.savesAltitude
TEST_F(DefaultSimulationOptionFactorySaveDefault, SavesAltitude)
{
    EXPECT_NEAR(m_preferences.getLaunchAltitude(), kSavedAltitude, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.SaveDefault.savesTemperature
TEST_F(DefaultSimulationOptionFactorySaveDefault, SavesTemperature)
{
    EXPECT_NEAR(m_preferences.getLaunchTemperature(), kSavedTemperature, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.SaveDefault.savesPressure
TEST_F(DefaultSimulationOptionFactorySaveDefault, SavesPressure)
{
    EXPECT_NEAR(m_preferences.getLaunchPressure(), kSavedPressure, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.SaveDefault.savesRelativeHumidity
TEST_F(DefaultSimulationOptionFactorySaveDefault, SavesRelativeHumidity)
{
    EXPECT_NEAR(m_preferences.getLaunchRelativeHumidity(), kSavedRelativeHumidity, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.SaveDefault.savesLaunchIntoWind
TEST_F(DefaultSimulationOptionFactorySaveDefault, SavesLaunchIntoWind)
{
    EXPECT_EQ(m_preferences.getLaunchIntoWind(), kSavedIntoWind);
}

// DefaultSimulationOptionFactoryTest.SaveDefault.savesRodLength
TEST_F(DefaultSimulationOptionFactorySaveDefault, SavesRodLength)
{
    EXPECT_NEAR(m_preferences.getLaunchRodLength(), kSavedRodLength, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.SaveDefault.savesRodAngle
TEST_F(DefaultSimulationOptionFactorySaveDefault, SavesRodAngle)
{
    EXPECT_NEAR(m_preferences.getLaunchRodAngle(), kSavedRodAngle, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.SaveDefault.savesRodDirection
TEST_F(DefaultSimulationOptionFactorySaveDefault, SavesRodDirection)
{
    EXPECT_NEAR(m_preferences.getLaunchRodDirection(), kSavedRodDirection, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.SaveDefault.savesWindAverage
TEST_F(DefaultSimulationOptionFactorySaveDefault, SavesWindAverage)
{
    EXPECT_NEAR(preferencesWind().getAverage(), kSavedWindAverage, kEpsilon);
    EXPECT_NEAR(m_preferences.getWindAverage(), kSavedWindAverage, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.SaveDefault.savesWindStandardDeviation
TEST_F(DefaultSimulationOptionFactorySaveDefault, SavesWindStandardDeviation)
{
    EXPECT_NEAR(preferencesWind().getStandardDeviation(), kSavedWindStandardDeviation, kEpsilon);
    EXPECT_NEAR(m_preferences.getWindStandardDeviation(), kSavedWindStandardDeviation, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.SaveDefault.savesWindDirection
TEST_F(DefaultSimulationOptionFactorySaveDefault, SavesWindDirection)
{
    EXPECT_NEAR(preferencesWind().getDirection(), kSavedWindDirection, kEpsilon);
    EXPECT_NEAR(m_preferences.getWindDirection(), kSavedWindDirection, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.SaveDefault.savedConditionsAreLoadedAgain
// ("stored conditions are returned by getDefault()")
TEST_F(DefaultSimulationOptionFactorySaveDefault, SavedConditionsAreLoadedAgain)
{
    const SimulationOptions reloaded = m_factory.getDefault();
    EXPECT_NEAR(reloaded.getLaunchLatitude(), kSavedLatitude, kEpsilon);
    EXPECT_NEAR(reloaded.getLaunchRodAngle(), kSavedRodAngle, kEpsilon);
    EXPECT_NEAR(reloaded.getAverageWindModel().getAverage(), kSavedWindAverage, kEpsilon);
    EXPECT_NEAR(reloaded.getAverageWindModel().getStandardDeviation(), kSavedWindStandardDeviation,
                kEpsilon);
    EXPECT_NEAR(reloaded.getAverageWindModel().getDirection(), kSavedWindDirection, kEpsilon);
}

// The nested class UnsetPreferences: "A fresh profile falls back to the same defaults on both
// sides".

// DefaultSimulationOptionFactoryTest.UnsetPreferences.launchIntoWindMatchesTheSimulationDefault
TEST_F(DefaultSimulationOptionFactoryTest, LaunchIntoWindMatchesTheSimulationDefault)
{
    EXPECT_EQ(m_preferences.getLaunchIntoWind(),
              SimulationOptions(m_preferences).getLaunchIntoWind());
    EXPECT_EQ(m_preferences.getLaunchIntoWind(), SimulationOptions().getLaunchIntoWind());
    EXPECT_TRUE(m_factory.getDefault().getLaunchIntoWind());
}

// DefaultSimulationOptionFactoryTest.UnsetPreferences.rodAngleLimitMatchesTheSimulationLimit
TEST_F(DefaultSimulationOptionFactoryTest, RodAngleLimitMatchesTheSimulationLimit)
{
    m_preferences.setLaunchRodAngle(SimulationOptions::kMaxLaunchRodAngle);
    EXPECT_NEAR(m_preferences.getLaunchRodAngle(), SimulationOptions::kMaxLaunchRodAngle, kEpsilon);
}

// DefaultSimulationOptionFactoryTest.copyingConditionsNotifiesNestedWindModels
// ("copyConditionsFrom() notifies the listeners of the nested wind models")
TEST_F(DefaultSimulationOptionFactoryTest, CopyingConditionsNotifiesNestedWindModels)
{
    SimulationOptions source(m_preferences);
    SimulationOptions target(m_preferences);
    source.getAverageWindModel().setAverage(target.getAverageWindModel().getAverage() + 1.0);
    ASSERT_TRUE(source.getMultiLevelWindModel().addWindLevel(100.0, 5.0, 0.5).has_value());
    const ChangeCounter averageWindEvents(target.getAverageWindModel().changed());
    const ChangeCounter multiLevelWindEvents(target.getMultiLevelWindModel().changed());

    target.copyConditionsFrom(source);

    EXPECT_EQ(averageWindEvents.count(), 1);
    EXPECT_EQ(multiLevelWindEvents.count(), 1);
}

// DefaultSimulationOptionFactoryTest.savingKeepsCustomAtmosphere
// ("Saving does not enable the ISA atmosphere of a simulation that has it disabled")
TEST_F(DefaultSimulationOptionFactoryTest, SavingKeepsCustomAtmosphere)
{
    m_factory.saveDefault(savedSimulationOptions());
    EXPECT_FALSE(m_preferences.isIsaAtmosphere());
    EXPECT_NEAR(m_preferences.getLaunchTemperature(), kSavedTemperature, kEpsilon);
}

// ------------------------------------------ beyond the JUnit test (OptionsProbe.factory() and
// preferencesConstructor() print the values pinned here)

TEST_F(DefaultSimulationOptionFactoryTest, GetDefaultOfAnEmptyStore)
{
    const SimulationOptions defaults = m_factory.getDefault();
    // The launch preferences' defaults: unlike a plain SimulationOptions, the wind blows.
    EXPECT_EQ(defaults.getAverageWindModel().getAverage(), 2.0);
    EXPECT_EQ(defaults.getAverageWindModel().getStandardDeviation(), 0.2);
    EXPECT_EQ(defaults.getAverageWindModel().getDirection(), kPi / 2);
    EXPECT_EQ(defaults.getAverageWindModel().getTurbulenceIntensity(), 0.1);
    // Everything else is what SimulationOptions has by itself.
    SimulationOptions plain(m_preferences);
    plain.getAverageWindModel().setAverage(2.0);
    plain.getAverageWindModel().setStandardDeviation(0.2);
    EXPECT_EQ(describe(defaults), describe(plain));
    EXPECT_FALSE(defaults.isRandomSeedFixed());
    // Reading an empty store leaves it empty: every value was its default already.
    EXPECT_TRUE(m_preferences.empty());
}

TEST_F(DefaultSimulationOptionFactoryTest, GetDefaultAppliesTheLaunchPreferences)
{
    storeEverySimulationKey(m_preferences);
    const SimulationOptions defaults = m_factory.getDefault();
    EXPECT_EQ(defaults.getLaunchRodLength(), 2.5);
    EXPECT_FALSE(defaults.getLaunchIntoWind());
    EXPECT_EQ(defaults.getLaunchRodAngle(), 0.25);
    EXPECT_EQ(defaults.getLaunchRodDirection(), 1.75);
    EXPECT_EQ(defaults.getAverageWindModel().getAverage(), 6.0);
    EXPECT_EQ(defaults.getAverageWindModel().getStandardDeviation(), 1.5);
    EXPECT_EQ(defaults.getAverageWindModel().getDirection(), 3.5);
    EXPECT_EQ(defaults.getAverageWindModel().getTurbulenceIntensity(), 0.25);
    EXPECT_EQ(defaults.getLaunchAltitude(), 321.0);
    EXPECT_EQ(defaults.getLaunchLatitude(), 45.5);
    EXPECT_EQ(defaults.getLaunchLongitude(), 12.25);
    EXPECT_FALSE(defaults.isIsaAtmosphere());
    EXPECT_EQ(defaults.getLaunchTemperature(), 301.5);
    EXPECT_EQ(defaults.getLaunchPressure(), 98765.0);
    EXPECT_EQ(defaults.getLaunchRelativeHumidity(), 0.4);
    // The fixed seed of the preferences.
    EXPECT_TRUE(defaults.isRandomSeedFixed());
    EXPECT_EQ(defaults.getRandomSeed(), 424242);
}

TEST_F(DefaultSimulationOptionFactoryTest, GetDefaultKeepsWhatSimulationOptionsReadsAndNoMore)
{
    storeEverySimulationKey(m_preferences);
    const SimulationOptions defaults = m_factory.getDefault();
    // Read by SimulationOptions' constructor, not by the factory.
    EXPECT_EQ(defaults.getTimeStep(), 0.02);
    EXPECT_EQ(defaults.getMaxSimulationTime(), 900.0);
    EXPECT_EQ(defaults.getGravityModelType(), GravityModelType::CONSTANT);
    EXPECT_EQ(defaults.getConstantGravity(), 3.71);
    EXPECT_EQ(describeLevels(defaults.getMultiLevelWindModel()), "[0, 6, 3.5, 1.5]");
    // Not launch conditions: these stay at SimulationOptions' constants, whatever is stored.
    EXPECT_EQ(defaults.getWindModelType(), WindModelType::AVERAGE);
    EXPECT_EQ(defaults.getGeodeticComputation(), GeodeticComputationStrategy::SPHERICAL);
    EXPECT_EQ(defaults.getSimulationStepperMethodChoice(), SimulationStepperMethod::RK4);
    EXPECT_EQ(defaults.getRecoverySpeedWarning(), 20.0);
    EXPECT_EQ(defaults.getDrogueLowSpeedWarning(), 3.048);
    EXPECT_EQ(defaults.getRecoveryDrogueMainHighSpeedWarning(), 30.48);
    EXPECT_EQ(defaults.getRecoveryDrogueMainLowSpeedWarning(), 15.24);
    EXPECT_FALSE(defaults.hasDragLookup());
    EXPECT_FALSE(defaults.hasStabilityLookup());
}

TEST_F(DefaultSimulationOptionFactoryTest,
       GetDefaultDoesNotChangePreferencesThatDoNotLaunchIntoTheWind)
{
    storeEverySimulationKey(m_preferences);
    const InMemoryPreferences before = m_preferences;
    const ChangeCounter       events(m_preferences.changed());
    const SimulationOptions   defaults = m_factory.getDefault();
    EXPECT_TRUE(m_preferences == before);
    EXPECT_EQ(events.count(), 0);
    EXPECT_FALSE(defaults.getLaunchIntoWind());
}

TEST_F(DefaultSimulationOptionFactoryTest, TheSeedIsOnlyAppliedWhenThePreferencesFixIt)
{
    m_preferences.setRandomSeed(246813579);
    const SimulationOptions notFixed = m_factory.getDefault();
    EXPECT_FALSE(notFixed.isRandomSeedFixed());

    m_preferences.setRandomSeedFixed(true);
    const SimulationOptions fixed = m_factory.getDefault();
    EXPECT_TRUE(fixed.isRandomSeedFixed());
    EXPECT_EQ(fixed.getRandomSeed(), 246813579);

    // The default seed 0 is a seed like any other.
    m_preferences.setRandomSeed(0);
    const SimulationOptions zero = m_factory.getDefault();
    EXPECT_TRUE(zero.isRandomSeedFixed());
    EXPECT_EQ(zero.getRandomSeed(), 0);
}

TEST_F(DefaultSimulationOptionFactoryTest, GetDefaultIntoTheWindSyncsTheRodDirectionPreference)
{
    // OptionsProbe: "into wind: rod direction = 4.0 wind 4.0 pref rod raw 4.0 pref events 1"
    m_preferences.putDouble(Keys::kWindDirection, 4.0);
    m_preferences.putDouble(Keys::kLaunchRodDirection, 1.0);
    const ChangeCounter events(m_preferences.changed());

    SimulationOptions defaults = m_factory.getDefault();

    EXPECT_EQ(defaults.getLaunchRodDirection(), 4.0);
    EXPECT_EQ(defaults.getAverageWindModel().getDirection(), 4.0);
    // Reading the rod direction of preferences that launch into the wind stores the wind
    // direction as the rod direction.
    EXPECT_EQ(m_preferences.getDouble(Keys::kLaunchRodDirection, -1), 4.0);
    EXPECT_EQ(events.count(), 1);
    // And that is the direction the options hold themselves.
    defaults.setLaunchIntoWind(false);
    EXPECT_EQ(defaults.getLaunchRodDirection(), 4.0);
}

TEST_F(DefaultSimulationOptionFactoryTest, GetDefaultWithTheIsaFollowsTheAltitude)
{
    // OptionsProbe: "ISA 1500: T P H = 278.4023001554197 84559.67285449419 0.0 isa true"
    m_preferences.setLaunchAltitude(1500);
    EXPECT_TRUE(isJavaValue(278.4023001554197, m_preferences.getLaunchTemperature()));
    EXPECT_TRUE(isJavaValue(84559.67285449419, m_preferences.getLaunchPressure()));

    const SimulationOptions defaults = m_factory.getDefault();

    EXPECT_TRUE(defaults.isIsaAtmosphere());
    EXPECT_EQ(defaults.getLaunchAltitude(), 1500.0);
    EXPECT_TRUE(isJavaValue(278.4023001554197, defaults.getLaunchTemperature()));
    EXPECT_TRUE(isJavaValue(84559.67285449419, defaults.getLaunchPressure()));
    EXPECT_EQ(defaults.getLaunchRelativeHumidity(), 0.0);
}

TEST_F(DefaultSimulationOptionFactoryTest, StoredConditionsWinOverTheIsaOnes)
{
    // The ISA flag and the altitude come first, the stored conditions after them: preferences
    // that say ISA but hold other conditions (written by hand, or by an older version) load as
    // they are.
    m_preferences.putDouble(Keys::kLaunchAltitude, 1500.0);
    m_preferences.putDouble(Keys::kLaunchTemperature, 300.0);
    m_preferences.putDouble(Keys::kLaunchPressure, 90000.0);
    m_preferences.putDouble(Keys::kLaunchRelativeHumidity, 0.5);

    const SimulationOptions defaults = m_factory.getDefault();

    EXPECT_TRUE(defaults.isIsaAtmosphere());
    EXPECT_EQ(defaults.getLaunchTemperature(), 300.0);
    EXPECT_EQ(defaults.getLaunchPressure(), 90000.0);
    EXPECT_EQ(defaults.getLaunchRelativeHumidity(), 0.5);
}

TEST_F(DefaultSimulationOptionFactoryTest, SaveDefaultStoresTheLaunchConditions)
{
    // OptionsProbe: "prefs: after saveDefault"
    SimulationOptions saved = savedSimulationOptions();
    saved.setRandomSeed(99);
    saved.setRandomSeedFixed(true);
    saved.setTimeStep(0.01);
    saved.setGravityModelType(GravityModelType::CONSTANT);

    m_factory.saveDefault(saved);

    EXPECT_FALSE(m_preferences.isIsaAtmosphere());
    EXPECT_EQ(m_preferences.getLaunchLatitude(), -33.8688);
    EXPECT_EQ(m_preferences.getLaunchLongitude(), 151.2093);
    EXPECT_EQ(m_preferences.getLaunchAltitude(), 80.0);
    EXPECT_EQ(m_preferences.getLaunchTemperature(), 301.15);
    EXPECT_EQ(m_preferences.getLaunchPressure(), 99000.0);
    EXPECT_EQ(m_preferences.getLaunchRelativeHumidity(), 0.35);
    EXPECT_FALSE(m_preferences.getLaunchIntoWind());
    EXPECT_EQ(m_preferences.getLaunchRodLength(), 2.4);
    EXPECT_EQ(m_preferences.getLaunchRodAngle(), 0.7);
    EXPECT_EQ(m_preferences.getDouble(Keys::kLaunchRodDirection, -1), 2.1);
    EXPECT_EQ(m_preferences.getWindAverage(), 7.5);
    // The wind keys hold the turbulence intensity, deviation / average.
    EXPECT_EQ(m_preferences.getWindTurbulenceIntensity(), 0.14666666666666667);
    EXPECT_EQ(m_preferences.getWindTurbulenceIntensity(), 1.1 / 7.5);
    EXPECT_EQ(m_preferences.getWindDirection(), 2.8);
}

TEST_F(DefaultSimulationOptionFactoryTest, SaveDefaultStoresOnlyTheLaunchConditions)
{
    SimulationOptions saved = savedSimulationOptions();
    saved.setRandomSeed(99);
    saved.setRandomSeedFixed(true);
    saved.setTimeStep(0.01);
    saved.setMaxSimulationTime(100);
    saved.setGravityModelType(GravityModelType::CONSTANT);
    saved.setConstantGravity(1.62);
    saved.setGeodeticComputation(GeodeticComputationStrategy::FLAT);
    saved.setRecoverySpeedWarning(99);

    m_factory.saveDefault(saved);

    EXPECT_FALSE(m_preferences.isRandomSeedFixed());
    EXPECT_EQ(m_preferences.getRandomSeed(), 0);
    EXPECT_EQ(m_preferences.getTimeStep(), 0.05);
    EXPECT_EQ(m_preferences.getMaxSimulationTime(), 1200.0);
    EXPECT_EQ(m_preferences.getGravityModelName(), "WGS");
    EXPECT_EQ(m_preferences.getConstantGravityValue(), 9.807);
    EXPECT_EQ(m_preferences.getGeodeticComputationName(), "SPHERICAL");
    EXPECT_EQ(m_preferences.getRecoverySpeedWarning(), 20.0);
    EXPECT_EQ(m_preferences.getSimulationStepperMethodName(), "RK4");
    // The keys that were written, sorted: the launch conditions and nothing else.
    const std::vector<std::string> expected{
        "LaunchAltitude",  "LaunchIntoWind",         "LaunchLatitude", "LaunchLongitude",
        "LaunchPressure",  "LaunchRelativeHumidity", "LaunchRodAngle", "LaunchRodDirection",
        "LaunchRodLength", "LaunchTemperature",      "LaunchUseISA",   "WindAverage",
        "WindDirection",   "WindTurbulence"};
    EXPECT_EQ(m_preferences.keys(), expected);
}

TEST_F(DefaultSimulationOptionFactoryTest, SaveDefaultAnnouncesEveryChangedPreference)
{
    const SimulationOptions saved = savedSimulationOptions();
    ChangeCounter           events(m_preferences.changed());

    m_factory.saveDefault(saved);

    // Java announces 10 changes: the latitude, longitude, ISA flag, altitude, temperature,
    // pressure, humidity and the rod length, angle and direction (the launch-into-wind flag is
    // stored silently). Here the wind average, turbulence intensity and direction are announced
    // too (see Preferences).
    EXPECT_EQ(events.count(), 13);

    // Saving the same conditions again changes nothing.
    events.reset();
    const InMemoryPreferences before = m_preferences;
    m_factory.saveDefault(saved);
    EXPECT_EQ(events.count(), 0);
    EXPECT_TRUE(m_preferences == before);
}

TEST_F(DefaultSimulationOptionFactoryTest, SaveDefaultOfUntouchedOptions)
{
    // OptionsProbe: "store: after saveDefault(new SimulationOptions()) into an empty store":
    // LaunchIntoWind true, WindAverage 0.0, WindDirection pi/2, WindTurbulence 0.0. A plain
    // SimulationOptions has no wind, where the launch preferences default to 2 m/s.
    m_factory.saveDefault(SimulationOptions(m_preferences));

    EXPECT_TRUE(m_preferences.getLaunchIntoWind());
    EXPECT_EQ(m_preferences.getWindAverage(), 0.0);
    EXPECT_EQ(m_preferences.getWindTurbulenceIntensity(), 0.0);
    EXPECT_EQ(m_preferences.getWindDirection(), kPi / 2);
    // Deviation: Java also writes the wind direction, although it is the default; here a value
    // that equals what the store gives is not written.
    const std::vector<std::string> expected{"LaunchIntoWind", "WindAverage", "WindTurbulence"};
    EXPECT_EQ(m_preferences.keys(), expected);

    const SimulationOptions reloaded = m_factory.getDefault();
    EXPECT_EQ(reloaded.getAverageWindModel().getAverage(), 0.0);
    EXPECT_EQ(reloaded.getAverageWindModel().getStandardDeviation(), 0.0);
}

TEST_F(DefaultSimulationOptionFactoryTest, SaveDefaultWhileLaunchingIntoTheWind)
{
    // OptionsProbe: "prefs: after saveDefault into wind, zero average": rodDirectionRaw 5.0,
    // windAverageRaw 0.0, windTurbulenceRaw 1.0, windDirectionRaw 5.0.
    SimulationOptions intoWind(m_preferences);
    intoWind.setLaunchRodDirection(1.0);
    intoWind.getAverageWindModel().setDirection(5.0);
    intoWind.getAverageWindModel().setAverage(0.0);
    intoWind.getAverageWindModel().setStandardDeviation(0.5);

    m_factory.saveDefault(intoWind);

    EXPECT_TRUE(m_preferences.getLaunchIntoWind());
    // The rod direction that is saved is the wind's, not the hidden stored one.
    EXPECT_EQ(m_preferences.getDouble(Keys::kLaunchRodDirection, -1), 5.0);
    EXPECT_EQ(m_preferences.getWindAverage(), 0.0);
    EXPECT_EQ(m_preferences.getWindTurbulenceIntensity(), 1.0);
    EXPECT_EQ(m_preferences.getWindDirection(), 5.0);

    const SimulationOptions reloaded = m_factory.getDefault();
    EXPECT_EQ(reloaded.getAverageWindModel().getAverage(), 0.0);
    EXPECT_EQ(reloaded.getAverageWindModel().getDirection(), 5.0);
    EXPECT_EQ(reloaded.getLaunchRodDirection(), 5.0);
    // Deviation (see the class comment): a deviation without an average does not survive the
    // wind keys, which hold the intensity. Java reloads 0.5 from the wind model its preferences
    // keep for the life of the program, and 0 after a restart.
    EXPECT_EQ(reloaded.getAverageWindModel().getStandardDeviation(), 0.0);
}

TEST_F(DefaultSimulationOptionFactoryTest, SavingACalmWindReplacesAStaleTurbulenceIntensity)
{
    // FixProbe: WindTurbulence is 1.0 after the first save and 0.0 after the second, and with
    // the average set to 5 m/s afterwards getDefault() has 5 m/s without a deviation. (The
    // model loaded for the second save is calm already, since the intensity of a wind without an
    // average gives no deviation: no setter changes it, and the key is stored all the same.)
    SimulationOptions gusty(m_preferences);
    gusty.getAverageWindModel().setStandardDeviation(0.5);
    m_factory.saveDefault(gusty);
    EXPECT_EQ(m_preferences.getWindAverage(), 0.0);
    EXPECT_EQ(m_preferences.getWindTurbulenceIntensity(), 1.0);

    const SimulationOptions calm(m_preferences);
    EXPECT_EQ(calm.getAverageWindModel().getAverage(), 0.0);
    EXPECT_EQ(calm.getAverageWindModel().getStandardDeviation(), 0.0);
    ChangeCounter events(m_preferences.changed());
    m_factory.saveDefault(calm);
    EXPECT_EQ(m_preferences.getWindAverage(), 0.0);
    EXPECT_EQ(m_preferences.getWindTurbulenceIntensity(), 0.0);
    // The intensity alone changed.
    EXPECT_EQ(events.count(), 1);
    events.reset();
    m_factory.saveDefault(calm);
    EXPECT_EQ(events.count(), 0);

    // Java: preferences.getAverageWindModel().setAverage(5), which keeps the model's intensity.
    m_preferences.setWindAverage(5.0);
    const SimulationOptions defaults = m_factory.getDefault();
    EXPECT_EQ(defaults.getAverageWindModel().getAverage(), 5.0);
    EXPECT_EQ(defaults.getAverageWindModel().getStandardDeviation(), 0.0);
}

TEST_F(DefaultSimulationOptionFactoryTest, SaveDefaultStoresTheIntensityOfTheWindItSaved)
{
    // The wind keys as a settings file may hold them: an intensity of 0.1 at 3 m/s is a
    // deviation of 0.30000000000000004 m/s, whose intensity is 0.10000000000000002. Java stores
    // that when its preferences first load the wind (FixProbe: WindTurbulence
    // 0.10000000000000002 after getDefault()); here the first saveDefault() does.
    m_preferences.putDouble(Keys::kWindAverage, 3.0);
    m_preferences.putDouble(Keys::kWindTurbulence, 0.1);
    const SimulationOptions defaults = m_factory.getDefault();
    EXPECT_EQ(defaults.getAverageWindModel().getAverage(), 3.0);
    EXPECT_EQ(defaults.getAverageWindModel().getStandardDeviation(), 0.30000000000000004);
    // Deviation: reading the defaults stores nothing.
    EXPECT_EQ(m_preferences.getWindTurbulenceIntensity(), 0.1);

    m_factory.saveDefault(defaults);
    EXPECT_EQ(m_preferences.getWindAverage(), 3.0);
    EXPECT_EQ(m_preferences.getWindTurbulenceIntensity(), 0.10000000000000002);
    // The deviation that comes back is the one that was saved.
    EXPECT_EQ(m_factory.getDefault().getAverageWindModel().getStandardDeviation(),
              0.30000000000000004);

    // From then on saving the same wind changes nothing.
    const InMemoryPreferences before = m_preferences;
    const ChangeCounter       events(m_preferences.changed());
    m_factory.saveDefault(m_factory.getDefault());
    EXPECT_EQ(events.count(), 0);
    EXPECT_TRUE(m_preferences == before);
}

TEST_F(DefaultSimulationOptionFactoryTest, SaveDefaultWithTheIsaStoresTheIsaConditions)
{
    SimulationOptions options(m_preferences);
    options.setLaunchAltitude(1500);

    m_factory.saveDefault(options);

    EXPECT_TRUE(m_preferences.isIsaAtmosphere());
    EXPECT_EQ(m_preferences.getLaunchAltitude(), 1500.0);
    EXPECT_TRUE(isJavaValue(278.4023001554197, m_preferences.getLaunchTemperature()));
    EXPECT_TRUE(isJavaValue(84559.67285449419, m_preferences.getLaunchPressure()));
    // The options and the preferences agree on the ISA.
    EXPECT_TRUE(isJavaValue(options.getLaunchTemperature(), m_preferences.getLaunchTemperature()));
    EXPECT_TRUE(isJavaValue(options.getLaunchPressure(), m_preferences.getLaunchPressure()));
}

TEST_F(DefaultSimulationOptionFactoryTest, SaveDefaultDoesNotChangeTheOptions)
{
    const SimulationOptions saved  = savedSimulationOptions();
    const std::string       before = describe(saved);
    m_factory.saveDefault(saved);
    EXPECT_EQ(describe(saved), before);
}

TEST_F(DefaultSimulationOptionFactoryTest, ARoundTripKeepsTheLaunchConditions)
{
    SimulationOptions saved = savedSimulationOptions();
    m_factory.saveDefault(saved);
    const SimulationOptions reloaded = m_factory.getDefault();
    EXPECT_EQ(reloaded.getLaunchLatitude(), saved.getLaunchLatitude());
    EXPECT_EQ(reloaded.getLaunchLongitude(), saved.getLaunchLongitude());
    EXPECT_EQ(reloaded.getLaunchAltitude(), saved.getLaunchAltitude());
    EXPECT_EQ(reloaded.isIsaAtmosphere(), saved.isIsaAtmosphere());
    EXPECT_EQ(reloaded.getLaunchTemperature(), saved.getLaunchTemperature());
    EXPECT_EQ(reloaded.getLaunchPressure(), saved.getLaunchPressure());
    EXPECT_EQ(reloaded.getLaunchRelativeHumidity(), saved.getLaunchRelativeHumidity());
    EXPECT_EQ(reloaded.getLaunchIntoWind(), saved.getLaunchIntoWind());
    EXPECT_EQ(reloaded.getLaunchRodLength(), saved.getLaunchRodLength());
    EXPECT_EQ(reloaded.getLaunchRodAngle(), saved.getLaunchRodAngle());
    EXPECT_EQ(reloaded.getLaunchRodDirection(), saved.getLaunchRodDirection());
    EXPECT_EQ(reloaded.getAverageWindModel().getAverage(), 7.5);
    EXPECT_EQ(reloaded.getAverageWindModel().getDirection(), 2.8);
    // Through the stored intensity, so within a rounding step.
    EXPECT_DOUBLE_EQ(reloaded.getAverageWindModel().getStandardDeviation(), 1.1);
    // The new defaults are what the next simulation starts from.
    saved.setLaunchRodLength(3.3);
    m_factory.saveDefault(saved);
    EXPECT_EQ(m_factory.getDefault().getLaunchRodLength(), 3.3);
}

TEST(DefaultSimulationOptionFactory, TwoFactoriesOverOneStoreShareIt)
{
    InMemoryPreferences                  preferences;
    const DefaultSimulationOptionFactory first(preferences);
    const DefaultSimulationOptionFactory second(preferences);
    SimulationOptions                    options;
    options.setLaunchIntoWind(false);
    options.setLaunchRodLength(4.5);
    first.saveDefault(options);
    EXPECT_EQ(second.getDefault().getLaunchRodLength(), 4.5);
    // The options getDefault() returns write their stepper choice to the factory's store.
    SimulationOptions defaults = second.getDefault();
    defaults.setSimulationStepperMethodChoice(SimulationStepperMethod::RK6);
    EXPECT_EQ(preferences.getSimulationStepperMethodName(), "RK6");
}

}  // namespace
