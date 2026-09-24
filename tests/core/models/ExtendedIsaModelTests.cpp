#include "QtRocket/models/ExtendedIsaModel.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/ModId.h"

namespace
{

using QtRocket::AtmosphericConditions;
using QtRocket::ErrorCode;
using QtRocket::ExtendedIsaModel;
using QtRocket::InMemoryPreferences;
using QtRocket::ModId;

constexpr double kIsaGravity = 9.80665;
constexpr double kNaN        = std::numeric_limits<double>::quiet_NaN();

/// ExtendedIsaModel::create(altitude, ...) for arguments that must be accepted.
std::unique_ptr<ExtendedIsaModel> makeModel(double altitude, double temperature, double pressure,
                                            double relativeHumidity)
{
    auto result = ExtendedIsaModel::create(altitude, temperature, pressure, relativeHumidity);
    EXPECT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error().toString());
    return std::move(result).value();  // throws (failing the test) when the model was refused
}

/// The error of a create() call that must be refused.
QtRocket::Error refusal(double altitude, double temperature, double pressure,
                        double relativeHumidity)
{
    const auto result = ExtendedIsaModel::create(altitude, temperature, pressure, relativeHumidity);
    EXPECT_FALSE(result.has_value());
    return result.has_value() ? QtRocket::Error{} : result.error();
}

/// |actual - expected| within @p relative of |expected|: libm's pow and exp may differ from
/// Java's in the last bit, and the differences add up through the layers.
void expectRelativelyNear(double actual, double expected, double relative = 1e-12)
{
    EXPECT_NEAR(actual, expected, std::abs(expected) * relative);
}

/// ExtendedISAModelTest.calculatePressure: the test's own copy of the barometric formula.
double calculatePressure(double alt1, double temp1, double alt2, double temp2, double press2)
{
    const double tempRate = (temp2 - temp1) / (alt2 - alt1);
    if (std::abs(tempRate) > 0.000001)
    {
        return press2 / std::pow(1 + ((alt2 - alt1) * tempRate / temp1),
                                 -kIsaGravity / (tempRate * AtmosphericConditions::kR));
    }
    return press2 / std::exp(-(alt2 - alt1) * kIsaGravity / (AtmosphericConditions::kR * temp1));
}

/// ExtendedISAModelTest.expectedISATemperature: the standard temperature at a geopotential
/// altitude.
double expectedIsaTemperature(double geopotentialAltitude)
{
    if (geopotentialAltitude <= 11000.0)
    {
        return 288.15 - (0.0065 * geopotentialAltitude);
    }
    if (geopotentialAltitude <= 20000.0)
    {
        return 216.65;
    }
    if (geopotentialAltitude <= 32000.0)
    {
        return 216.65 + (0.001 * (geopotentialAltitude - 20000.0));
    }
    if (geopotentialAltitude <= 47000.0)
    {
        return 228.65 + (0.0028 * (geopotentialAltitude - 32000.0));
    }
    if (geopotentialAltitude <= 51000.0)
    {
        return 270.65;
    }
    if (geopotentialAltitude <= 71000.0)
    {
        return 270.65 - (0.0028 * (geopotentialAltitude - 51000.0));
    }
    if (geopotentialAltitude <= 84852.0)
    {
        return 214.65 - (0.0020 * (geopotentialAltitude - 71000.0));
    }
    return 186.95;
}

/// The models of ExtendedISAModelTest.setUp().
class ExtendedIsaModelTest : public ::testing::Test
{
protected:
    ExtendedIsaModel m_standardModel;
    // Custom conditions
    std::unique_ptr<ExtendedIsaModel> m_customModel = makeModel(0, 278.15, 100000.0, 0);
    // A launch site at 1000 m with specific conditions
    std::unique_ptr<ExtendedIsaModel> m_altitudeModel = makeModel(1000.0, 281.15, 89876.0, 0);
    // A launch site at 2750 m (between the 2500 m and 3000 m levels)
    std::unique_ptr<ExtendedIsaModel> m_interpolatedAltitudeModel =
        makeModel(2750.0, 271.15, 72500.0, 0);

    /// testInterpolationAroundLaunchSite's points around the 2750 m launch site (the
    /// temperature falls with altitude in the troposphere).
    [[nodiscard]] std::vector<AtmosphericConditions> conditionsAroundLaunchSite() const
    {
        constexpr std::array<double, 5>    kAltitudes{2600.0, 2700.0, 2750.0, 2800.0, 2900.0};
        std::vector<AtmosphericConditions> conditions;
        conditions.reserve(kAltitudes.size());
        for (const double altitude : kAltitudes)
        {
            conditions.push_back(m_interpolatedAltitudeModel->getConditions(altitude));
        }
        return conditions;
    }
};

// ---- Ported from ExtendedISAModelTest.java ----

TEST_F(ExtendedIsaModelTest, StandardSeaLevel)
{
    const AtmosphericConditions conditions = m_standardModel.getConditions(0);
    EXPECT_NEAR(conditions.getTemperature(), 288.15, 0.01);
    EXPECT_NEAR(conditions.getPressure(), 101325.0, 0.01);
}

TEST_F(ExtendedIsaModelTest, NegativeAltitudesGiveSeaLevel)
{
    for (const double altitude : {-100.0, -1.0, 0.0})
    {
        const AtmosphericConditions conditions = m_standardModel.getConditions(altitude);
        EXPECT_NEAR(conditions.getTemperature(), 288.15, 0.01) << altitude;
        EXPECT_NEAR(conditions.getPressure(), 101325.0, 0.01) << altitude;
    }
}

TEST_F(ExtendedIsaModelTest, CustomModelSeaLevel)
{
    const AtmosphericConditions conditions = m_customModel->getConditions(0);
    EXPECT_NEAR(conditions.getTemperature(), 278.15, 0.01);
    EXPECT_NEAR(conditions.getPressure(), 100000.0, 0.01);
}

TEST_F(ExtendedIsaModelTest, TemperatureFallsInTheTroposphere)
{
    const AtmosphericConditions cond1 = m_standardModel.getConditions(0);
    const AtmosphericConditions cond2 = m_standardModel.getConditions(5000);
    const AtmosphericConditions cond3 = m_standardModel.getConditions(10000);

    EXPECT_GT(cond1.getTemperature(), cond2.getTemperature());
    EXPECT_GT(cond2.getTemperature(), cond3.getTemperature());
}

TEST_F(ExtendedIsaModelTest, PressureFallsWithAltitude)
{
    const AtmosphericConditions cond1 = m_standardModel.getConditions(0);
    const AtmosphericConditions cond2 = m_standardModel.getConditions(5000);
    const AtmosphericConditions cond3 = m_standardModel.getConditions(10000);

    EXPECT_GT(cond1.getPressure(), cond2.getPressure());
    EXPECT_GT(cond2.getPressure(), cond3.getPressure());
}

TEST_F(ExtendedIsaModelTest, HighAltitude)
{
    // Mainly checking that it doesn't fail
    const AtmosphericConditions conditions = m_standardModel.getConditions(80000);
    EXPECT_GT(conditions.getTemperature(), 0);
    EXPECT_GT(conditions.getPressure(), 0);
}

TEST_F(ExtendedIsaModelTest, LaunchSiteConditions)
{
    const AtmosphericConditions conditions = m_altitudeModel->getConditions(1000.0);
    EXPECT_NEAR(conditions.getTemperature(), 281.15, 0.01);
    EXPECT_NEAR(conditions.getPressure(), 89876.0, 0.01);
}

TEST_F(ExtendedIsaModelTest, BelowLaunchSiteAltitude)
{
    const AtmosphericConditions conditions500 = m_altitudeModel->getConditions(500.0);
    const AtmosphericConditions conditions0   = m_altitudeModel->getConditions(0.0);

    const double launchTemperature  = 281.15;
    const double launchPressure     = 89876.0;
    const double geopotentialLaunch = 999.8427120469674;
    const double layer1Alt          = 11000.0;
    const double layer1Temp         = 216.65;
    const double tempRate     = (layer1Temp - launchTemperature) / (layer1Alt - geopotentialLaunch);
    const double seaLevelTemp = launchTemperature - (tempRate * geopotentialLaunch);
    const double seaLevelPressure =
        calculatePressure(0, seaLevelTemp, geopotentialLaunch, launchTemperature, launchPressure);

    const double geopotential500 = 499.9606749190611;
    const double expectedTemp500 = seaLevelTemp + (tempRate * geopotential500);
    const double expectedPressure500 =
        calculatePressure(geopotential500, expectedTemp500, 0, seaLevelTemp, seaLevelPressure);

    // Below the launch site the lapse rate to 11 km is extended down to sea level
    EXPECT_NEAR(conditions500.getTemperature(), expectedTemp500, 0.01);
    EXPECT_NEAR(conditions500.getPressure(), expectedPressure500, 0.01);
    EXPECT_NEAR(conditions0.getTemperature(), seaLevelTemp, 0.01);
    EXPECT_NEAR(conditions0.getPressure(), seaLevelPressure, 0.01);
}

TEST_F(ExtendedIsaModelTest, AboveLaunchSiteAltitude)
{
    const AtmosphericConditions conditions2000 = m_altitudeModel->getConditions(2000.0);
    const AtmosphericConditions conditions3000 = m_altitudeModel->getConditions(3000.0);

    // Temperature and pressure should decrease with altitude
    EXPECT_LT(conditions2000.getTemperature(), 281.15);
    EXPECT_LT(conditions2000.getPressure(), 89876.0);
    EXPECT_LT(conditions3000.getTemperature(), conditions2000.getTemperature());
    EXPECT_LT(conditions3000.getPressure(), conditions2000.getPressure());
}

TEST_F(ExtendedIsaModelTest, EdgeCases)
{
    // Negative altitude
    EXPECT_TRUE(ExtendedIsaModel::create(-100.0, 288.15, 101325.0, 0.0).has_value());

    // Zero pressure
    EXPECT_FALSE(ExtendedIsaModel::create(1000.0, 288.15, 0.0, 0.0).has_value());
    // Zero temperature
    EXPECT_FALSE(ExtendedIsaModel::create(1000.0, 0.0, 101325.0, 0.0).has_value());
    // Negative pressure
    EXPECT_FALSE(ExtendedIsaModel::create(1000.0, 288.15, -1000.0, 0.0).has_value());
    // Negative temperature
    EXPECT_FALSE(ExtendedIsaModel::create(1000.0, -273.15, 101325.0, 0.0).has_value());
}

TEST_F(ExtendedIsaModelTest, TooHighLaunchSite)
{
    EXPECT_FALSE(ExtendedIsaModel::create(12000.0, 220.0, 25000.0, 0.0).has_value());
}

TEST_F(ExtendedIsaModelTest, LaunchSiteBetweenInterpolationLevels)
{
    // Conditions at the launch site (2750 m)
    const AtmosphericConditions conditions = m_interpolatedAltitudeModel->getConditions(2750.0);
    EXPECT_NEAR(conditions.getTemperature(), 271.15, 0.01);
    EXPECT_NEAR(conditions.getPressure(), 72500.0, 50);

    // Conditions around it
    const AtmosphericConditions lowerConditions =
        m_interpolatedAltitudeModel->getConditions(2600.0);
    const AtmosphericConditions upperConditions =
        m_interpolatedAltitudeModel->getConditions(2900.0);

    EXPECT_GE(lowerConditions.getTemperature(), conditions.getTemperature());
    EXPECT_LE(upperConditions.getTemperature(), conditions.getTemperature());
    EXPECT_GE(lowerConditions.getPressure(), conditions.getPressure());
    EXPECT_LE(upperConditions.getPressure(), conditions.getPressure());
}

TEST_F(ExtendedIsaModelTest, InterpolationAroundLaunchSiteIsMonotonic)
{
    const std::vector<AtmosphericConditions> conditions = conditionsAroundLaunchSite();

    // Monotonic decrease in temperature and pressure
    for (std::size_t i = 1; i < conditions.size(); ++i)
    {
        EXPECT_LE(conditions[i].getTemperature(), conditions[i - 1].getTemperature())
            << "Temperature should decrease or remain constant with altitude";
        EXPECT_LE(conditions[i].getPressure(), conditions[i - 1].getPressure())
            << "Pressure should decrease or remain constant with altitude";
    }
}

TEST_F(ExtendedIsaModelTest, InterpolationAroundLaunchSiteHasNoJumps)
{
    const std::vector<AtmosphericConditions> conditions = conditionsAroundLaunchSite();

    // No sudden jumps
    for (std::size_t i = 1; i + 1 < conditions.size(); ++i)
    {
        const double currTemp = conditions[i].getTemperature();
        const double prevTemp = conditions[i - 1].getTemperature();
        const double nextTemp = conditions[i + 1].getTemperature();
        EXPECT_LT(currTemp, prevTemp) << "Temperature should decrease in the troposphere";
        EXPECT_LT(nextTemp, currTemp) << "Temperature should decrease in the troposphere";
    }
}

TEST_F(ExtendedIsaModelTest, VariousLaunchSites)
{
    struct Site
    {
        double altitude;
        double temperature;
        double pressure;
    };
    constexpr std::array<Site, 3> kSites{{
        {.altitude = 1000.0, .temperature = 281.15, .pressure = 89876.0},  // Mountain
        {.altitude = 500.0, .temperature = 285.15, .pressure = 95461.0},   // Hill
        {.altitude = 2000.0, .temperature = 275.15, .pressure = 79501.0},  // High altitude
    }};
    for (const Site& site : kSites)
    {
        const std::unique_ptr<ExtendedIsaModel> model =
            makeModel(site.altitude, site.temperature, site.pressure, 0);
        const AtmosphericConditions conditions = model->getConditions(site.altitude);

        EXPECT_NEAR(conditions.getTemperature(), site.temperature, 0.01) << site.altitude;
        EXPECT_NEAR(conditions.getPressure(), site.pressure, 0.01) << site.altitude;

        // Conditions change appropriately above the launch site
        const AtmosphericConditions higher = model->getConditions(site.altitude + 1000);
        EXPECT_LT(higher.getTemperature(), site.temperature) << site.altitude;
        EXPECT_LT(higher.getPressure(), site.pressure) << site.altitude;
    }
}

TEST_F(ExtendedIsaModelTest, AtmosphereAboveLaunchSiteIsRealistic)
{
    const AtmosphericConditions conditions2000 = m_altitudeModel->getConditions(2000.0);
    const AtmosphericConditions conditions2500 = m_altitudeModel->getConditions(2500.0);

    // Standard tropospheric lapse rate is approximately -0.0065 K/m
    const double lapseRate =
        (conditions2500.getTemperature() - conditions2000.getTemperature()) / 500.0;
    EXPECT_LT(lapseRate, 0) << "Temperature should decrease with altitude";
    EXPECT_GT(lapseRate, -0.01) << "Temperature shouldn't decrease too rapidly";

    const double pressureRatio = conditions2500.getPressure() / conditions2000.getPressure();
    EXPECT_LT(pressureRatio, 1.0) << "Pressure should decrease with altitude";
    EXPECT_GT(pressureRatio, 0.9) << "Pressure shouldn't decrease too rapidly over 500m";
}

TEST_F(ExtendedIsaModelTest, NonZeroHumidityAtSeaLevel)
{
    const auto humidModel = ExtendedIsaModel::create(288.15, 101325.0, 0.65);
    ASSERT_TRUE(humidModel.has_value());
    EXPECT_NEAR((*humidModel)->getConditions(0).getRelativeHumidity(), 0.65, 1e-12);
}

TEST_F(ExtendedIsaModelTest, HumidityAffectsDerivedProperties)
{
    const auto humidModel = ExtendedIsaModel::create(288.15, 101325.0, 0.80);
    ASSERT_TRUE(humidModel.has_value());

    const AtmosphericConditions dry   = m_standardModel.getConditions(0);
    const AtmosphericConditions humid = (*humidModel)->getConditions(0);

    EXPECT_NEAR(dry.getTemperature(), humid.getTemperature(), 1e-9);
    EXPECT_NEAR(dry.getPressure(), humid.getPressure(), 1e-6);
    EXPECT_GT(humid.getGasConstant(), dry.getGasConstant())
        << "Humid air should have higher gas constant";
    EXPECT_LT(humid.getDensity(), dry.getDensity())
        << "Humid air should have lower density at same P/T";
}

TEST_F(ExtendedIsaModelTest, RelativeHumidityBounds)
{
    EXPECT_FALSE(ExtendedIsaModel::create(288.15, 101325.0, -0.01).has_value());
    EXPECT_FALSE(ExtendedIsaModel::create(288.15, 101325.0, 1.01).has_value());
    EXPECT_TRUE(ExtendedIsaModel::create(288.15, 101325.0, 0.0).has_value());
    EXPECT_TRUE(ExtendedIsaModel::create(288.15, 101325.0, 1.0).has_value());
}

TEST_F(ExtendedIsaModelTest, GeometricAltitudeIsConvertedToGeopotential)
{
    const double geometricAltitude    = 32000.0;
    const double geopotentialAltitude = 31839.71865615363;
    const double expectedTemp         = expectedIsaTemperature(geopotentialAltitude);

    const AtmosphericConditions conditions = m_standardModel.getConditions(geometricAltitude);
    EXPECT_NEAR(conditions.getTemperature(), expectedTemp, 0.03);
}

TEST_F(ExtendedIsaModelTest, MaximumAllowedAltitudeIsGeometric)
{
    EXPECT_NEAR(ExtendedIsaModel::getMaximumAllowedAltitude(), 11018.064362274883, 1e-6);
}

// ---- QtRocket additions ----

TEST(ExtendedIsaModel, IsaSanityTable)
{
    // The standard model against the U.S. Standard Atmosphere 1976 at geometric altitudes
    // (temperature K, pressure Pa, density kg/m^3). OpenRocket's model interpolates a 500 m
    // table, hence the tolerances.
    const ExtendedIsaModel model;

    const AtmosphericConditions seaLevel = model.getConditions(0);
    EXPECT_EQ(seaLevel.getTemperature(), 288.15);
    EXPECT_EQ(seaLevel.getPressure(), 101325.0);
    EXPECT_NEAR(seaLevel.getDensity(), 1.225, 1e-6);
    EXPECT_NEAR(seaLevel.getMachSpeed(), 340.29, 0.2);  // the linear fit; 340.294 exactly

    const AtmosphericConditions at11km = model.getConditions(11000);
    EXPECT_NEAR(at11km.getTemperature(), 216.774, 0.01);
    EXPECT_NEAR(at11km.getPressure(), 22699.9, 1.0);
    EXPECT_NEAR(at11km.getDensity(), 0.36480, 1e-4);

    const AtmosphericConditions at20km = model.getConditions(20000);
    EXPECT_NEAR(at20km.getTemperature(), 216.65, 1e-9);
    EXPECT_NEAR(at20km.getPressure(), 5529.3, 1.5);
    EXPECT_NEAR(at20km.getDensity(), 0.088910, 5e-5);
}

TEST(ExtendedIsaModel, StandardModelMatchesOpenRocket)
{
    // ExtendedISAModel().getConditions(altitude) printed by OpenRocket on JDK 17.
    struct Row
    {
        double altitude;
        double temperature;
        double pressure;
        double density;
    };
    constexpr std::array<Row, 22> kRows{{
        {.altitude    = -100,
         .temperature = 288.15,
         .pressure    = 101325.0,
         .density     = 1.2249994633486807},
        {.altitude = 0, .temperature = 288.15, .pressure = 101325.0, .density = 1.2249994633486807},
        {.altitude    = 250,
         .temperature = 286.52512780651307,
         .pressure    = 98393.14403432526,
         .density     = 1.1962998083893752},
        {.altitude    = 500,
         .temperature = 284.9002556130261,
         .pressure    = 95461.28806865054,
         .density     = 1.167272787831849},
        {.altitude    = 1000,
         .temperature = 281.6510223716947,
         .pressure    = 89876.28248259629,
         .density     = 1.111659230616184},
        {.altitude    = 1250,
         .temperature = 280.02666126355723,
         .pressure    = 87217.97766854524,
         .density     = 1.0850369800824247},
        {.altitude    = 2750,
         .temperature = 270.2827933837627,
         .pressure    = 72406.45325388075,
         .density     = 0.9332473938578757},
        {.altitude    = 3000,
         .temperature = 268.65919845164115,
         .pressure    = 70121.15575785963,
         .density     = 0.9092540850506489},
        {.altitude    = 5000,
         .temperature = 255.67554322180348,
         .pressure    = 54048.27762044487,
         .density     = 0.7364284894544959},
        {.altitude    = 10000,
         .temperature = 223.2520926479786,
         .pressure    = 26499.889218749628,
         .density     = 0.41351039348769864},
        {.altitude    = 11000,
         .temperature = 216.77351270445553,
         .pressure    = 22699.952216044647,
         .density     = 0.36480151877500605},
        {.altitude    = 15000,
         .temperature = 216.65,
         .pressure    = 12113.729256788607,
         .density     = 0.19478570406805773},
        {.altitude    = 20000,
         .temperature = 216.65,
         .pressure    = 5530.179819886914,
         .density     = 0.08892389346046907},
        {.altitude    = 25000,
         .temperature = 221.55206472628424,
         .pressure    = 2550.0247222620646,
         .density     = 0.04009650315250495},
        {.altitude    = 32000,
         .temperature = 228.48971865615363,
         .pressure    = 889.3437925719186,
         .density     = 0.013559414131832018},
        {.altitude    = 40000,
         .temperature = 250.34964610242113,
         .pressure    = 287.2770244451429,
         .density     = 0.0039975308311366725},
        {.altitude    = 47000,
         .temperature = 269.6841308536258,
         .pressure    = 115.90477537783666,
         .density     = 0.001497213890906607},
        {.altitude    = 50000,
         .temperature = 270.65,
         .pressure    = 79.82611435748018,
         .density     = 0.0010274835284443064},
        {.altitude    = 60000,
         .temperature = 247.02088477279673,
         .pressure    = 21.974374916453655,
         .density     = 3.098994225437816E-4},
        {.altitude    = 71000,
         .temperature = 216.8459106787646,
         .pressure    = 4.4827660447945155,
         .density     = 7.201662208107736E-5},
        {.altitude    = 80000,
         .temperature = 198.64088803599,
         .pressure    = 1.0534013131896012,
         .density     = 1.847409264753737E-5},
        {.altitude    = 85500,
         .temperature = 187.9233248811066,
         .pressure    = 0.4083884911216268,
         .density     = 7.57060725991462E-6},
    }};
    const ExtendedIsaModel        model;
    for (const Row& row : kRows)
    {
        SCOPED_TRACE(row.altitude);
        const AtmosphericConditions conditions = model.getConditions(row.altitude);
        expectRelativelyNear(conditions.getTemperature(), row.temperature);
        expectRelativelyNear(conditions.getPressure(), row.pressure);
        expectRelativelyNear(conditions.getDensity(), row.density);
        EXPECT_EQ(conditions.getRelativeHumidity(), 0.0);
    }

    // Above the table: the top level, as 85500 m.
    for (const double altitude : {86000.0, 100000.0, 1e9})
    {
        const AtmosphericConditions conditions = model.getConditions(altitude);
        expectRelativelyNear(conditions.getTemperature(), 187.9233248811066);
        expectRelativelyNear(conditions.getPressure(), 0.4083884911216268);
    }
}

TEST(ExtendedIsaModel, LaunchSiteModelsMatchOpenRocket)
{
    // Printed by OpenRocket on JDK 17.
    struct Row
    {
        double altitude;
        double temperature;
        double pressure;
    };
    const std::unique_ptr<ExtendedIsaModel> site1000 = makeModel(1000.0, 281.15, 89876.0, 0);
    constexpr std::array<Row, 10>           kSite1000{{
        {.altitude = 0, .temperature = 287.59888405952563, .pressure = 101347.15200058524},
        {.altitude = 500, .temperature = 284.3741884268752, .pressure = 95471.43998762341},
        {.altitude = 999.9, .temperature = 281.1506448376854, .pressure = 89877.11908799752},
        {.altitude = 1000, .temperature = 281.15, .pressure = 89876.0},
        {.altitude = 1500, .temperature = 277.92631865924335, .pressure = 84550.39772065001},
        {.altitude = 2000, .temperature = 274.7031442849863, .pressure = 79484.47626198501},
        {.altitude = 2500, .temperature = 271.4804767576475, .pressure = 74668.35176733918},
        {.altitude = 3000, .temperature = 268.2583159576832, .pressure = 70092.40903590687},
        {.altitude = 11000, .temperature = 216.77256067899714, .pressure = 22670.80460348376},
        {.altitude = 20000, .temperature = 216.65, .pressure = 5523.078819861138},
    }};
    for (const Row& row : kSite1000)
    {
        SCOPED_TRACE(row.altitude);
        const AtmosphericConditions conditions = site1000->getConditions(row.altitude);
        expectRelativelyNear(conditions.getTemperature(), row.temperature);
        expectRelativelyNear(conditions.getPressure(), row.pressure);
    }

    const std::unique_ptr<ExtendedIsaModel> site2750 = makeModel(2750.0, 271.15, 72500.0, 0);
    constexpr std::array<Row, 4>            kSite2750{{
        {.altitude = 0, .temperature = 289.3061939339766, .pressure = 101374.06008533257},
        {.altitude = 2750, .temperature = 271.1500648575043, .pressure = 72529.05211113073},
        {.altitude = 2900, .temperature = 270.16015524974114, .pressure = 71159.9417273548},
        {.altitude = 5000, .temperature = 256.30660750632364, .pressure = 54184.83456497578},
    }};
    for (const Row& row : kSite2750)
    {
        SCOPED_TRACE(row.altitude);
        const AtmosphericConditions conditions = site2750->getConditions(row.altitude);
        expectRelativelyNear(conditions.getTemperature(), row.temperature);
        expectRelativelyNear(conditions.getPressure(), row.pressure);
    }

    // A sea level model with other conditions.
    const auto custom = ExtendedIsaModel::create(278.15, 100000.0);
    ASSERT_TRUE(custom.has_value());
    expectRelativelyNear((*custom)->getConditions(1000).getTemperature(), 272.5599702917374);
    expectRelativelyNear((*custom)->getConditions(1000).getPressure(), 88333.17683684432);
    expectRelativelyNear((*custom)->getConditions(12000).getPressure(), 18621.766067651242);

    // Humidity is carried up unchanged.
    const auto humid = ExtendedIsaModel::create(288.15, 101325.0, 0.8);
    ASSERT_TRUE(humid.has_value());
    const AtmosphericConditions humid12km = (*humid)->getConditions(12000);
    EXPECT_EQ(humid12km.getRelativeHumidity(), 0.8);
    expectRelativelyNear(humid12km.getPressure(), 19402.49969633497);
    expectRelativelyNear(humid12km.getDensity(), 0.31197051347554605);
}

TEST(ExtendedIsaModel, SiteAtOrBelowSeaLevelIsTheSeaLevelModel)
{
    // Java takes measurements at or below 0 m as the sea level values.
    const std::unique_ptr<ExtendedIsaModel> below = makeModel(-100.0, 288.15, 101325.0, 0.0);
    const ExtendedIsaModel                  standard;
    for (const double altitude : {0.0, 1000.0, 15000.0})
    {
        EXPECT_EQ(below->getConditions(altitude).getTemperature(),
                  standard.getConditions(altitude).getTemperature());
        EXPECT_EQ(below->getConditions(altitude).getPressure(),
                  standard.getConditions(altitude).getPressure());
    }
}

TEST(ExtendedIsaModel, RefusalsCarryOpenRocketsMessages)
{
    const QtRocket::Error tooHigh = refusal(12000.0, 220.0, 25000.0, 0.0);
    EXPECT_EQ(tooHigh.code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(tooHigh.message, "Too high first altitude: 12000.0");

    EXPECT_EQ(refusal(1000.0, 0.0, 101325.0, 0.0).message, "Temperature must be positive (Kelvin)");
    EXPECT_EQ(refusal(1000.0, 288.15, 0.0, 0.0).message, "Pressure must be positive (Pascals)");
    EXPECT_EQ(refusal(0, 288.15, 101325.0, 1.5).message,
              "Relative humidity must be between 0 and 1");

    // The altitude is checked first, as in Java.
    EXPECT_EQ(refusal(20000.0, -1.0, -1.0, 2.0).message, "Too high first altitude: 20000.0");

    // The limit: 1 m below the 11 km layer is the highest site, the layer itself is refused.
    EXPECT_TRUE(
        ExtendedIsaModel::create(ExtendedIsaModel::getMaximumAllowedAltitude(), 216.7, 22700.0, 0)
            .has_value());
    EXPECT_FALSE(ExtendedIsaModel::create(11019.1, 216.7, 22700.0, 0).has_value());
}

TEST(ExtendedIsaModel, ColdSiteWithoutPositiveSeaLevelTemperatureIsRefused)
{
    // 50 K at 5 km: the lapse rate to 11 km extends to a negative sea level temperature. Java
    // builds the model and fails in its first getConditions() with this message.
    const QtRocket::Error cold = refusal(5000.0, 50.0, 25000.0, 0.0);
    EXPECT_EQ(cold.code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(cold.message, "Temperature must be positive (Kelvin)");

    // A cold but consistent site is fine.
    const std::unique_ptr<ExtendedIsaModel> chilly = makeModel(5000.0, 200.0, 50000.0, 0.0);
    EXPECT_GT(chilly->getConditions(0).getTemperature(), 0);
}

TEST(ExtendedIsaModel, NaNPassesTheChecksAsInJava)
{
    // Java lets NaN through every check; a NaN altitude counts as sea level.
    EXPECT_TRUE(ExtendedIsaModel::create(kNaN, 288.15, 101325.0, 0).has_value());
    const auto nanTemperature = ExtendedIsaModel::create(0, kNaN, 101325.0, 0);
    ASSERT_TRUE(nanTemperature.has_value());
    EXPECT_TRUE(std::isnan((*nanTemperature)->getConditions(0).getTemperature()));

    // A NaN altitude gives NaN conditions.
    const ExtendedIsaModel      model;
    const AtmosphericConditions conditions = model.getConditions(kNaN);
    EXPECT_TRUE(std::isnan(conditions.getTemperature()));
    EXPECT_TRUE(std::isnan(conditions.getPressure()));
    EXPECT_TRUE(std::isnan(conditions.getRelativeHumidity()));
}

TEST(ExtendedIsaModel, ModIdIsZero)
{
    const ExtendedIsaModel model;
    EXPECT_EQ(model.modId(), ModId::zero());
}

TEST(ExtendedIsaModel, ConstantsMatchOpenRocket)
{
    EXPECT_EQ(ExtendedIsaModel::kStandardTemperature, 288.15);
    EXPECT_EQ(ExtendedIsaModel::kStandardPressure, 101325.0);
    EXPECT_EQ(ExtendedIsaModel::kStandardRelativeHumidity, 0.0);
    static_assert(ExtendedIsaModel::getMaximumAllowedAltitude() > 11018.0);
}

TEST(ExtendedIsaModel, AgreesWithThePreferencesIsaAtmosphere)
{
    // Preferences computes the ISA conditions of the launch altitude privately (preferences/
    // sits below models/); both must give the model's values.
    const ExtendedIsaModel model;
    InMemoryPreferences    prefs;
    for (const double altitude : {10.0, 777.0, 1000.0, 4321.5, 9999.0, 11018.0})
    {
        SCOPED_TRACE(altitude);
        prefs.setLaunchAltitude(altitude);
        const AtmosphericConditions conditions = model.getConditions(altitude);
        EXPECT_DOUBLE_EQ(prefs.getLaunchTemperature(), conditions.getTemperature());
        EXPECT_DOUBLE_EQ(prefs.getLaunchPressure(), conditions.getPressure());
        EXPECT_EQ(prefs.getLaunchRelativeHumidity(), conditions.getRelativeHumidity());
    }
}

TEST(ExtendedIsaModel, ConcurrentFirstUseIsSafe)
{
    // The table is built on the first call; several threads may make it at once (OpenRocket
    // shares one standard model between simulations). Run under the tsan preset to check.
    const ExtendedIsaModel       model;
    constexpr std::size_t        kThreads = 4;
    std::array<double, kThreads> pressures{};
    {
        std::vector<std::jthread> threads;
        threads.reserve(kThreads);
        for (std::size_t i = 0; i < kThreads; ++i)
        {
            threads.emplace_back([&model, &pressures, i] {
                pressures.at(i) = model.getConditions(1234.5).getPressure();
            });
        }
    }
    for (const double pressure : pressures)
    {
        EXPECT_EQ(pressure, model.getConditions(1234.5).getPressure());
    }
}

}  // namespace
