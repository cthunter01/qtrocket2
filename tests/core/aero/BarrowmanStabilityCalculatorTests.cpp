#include "QtRocket/aero/BarrowmanStabilityCalculator.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/aero/StabilityCalculator.h"
#include "QtRocket/aero/StabilityForceBreakdown.h"
#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "aero/BarrowmanTestRockets.h"
#include "aero/ForcePins.h"
#include "rocket/JavaValueDifferences.h"
#include "rocket/TestRockets.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::AerodynamicForces;
using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BarrowmanStabilityCalculator;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::Coordinate;
using QtRocket::FlightConditions;
using QtRocket::FlightConfiguration;
using QtRocket::ForceMap;
using QtRocket::MessageSources;
using QtRocket::NoseCone;
using QtRocket::Parachute;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::StabilityCalculator;
using QtRocket::StabilityForceBreakdown;
using QtRocket::TrapezoidFinSet;
using QtRocket::UnitGroup;
using QtRocket::Warning;
using QtRocket::WarningSet;
using QtRocket::Test::allComponents;
using QtRocket::Test::compareNonAxial;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::JavaValueDifferences;
using QtRocket::Test::kComponentTolerance;
using QtRocket::Test::kRocketTolerance;
using QtRocket::Test::NonAxialPin;
using QtRocket::Test::TestEndPlateRocket;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::TestEstesAlphaIIIWithInlinePod;
using QtRocket::Test::TestEstesAlphaIIIWithPods;
using QtRocket::Test::TestFalcon9Heavy;
using QtRocket::Test::TestFlushPodRocket;
using QtRocket::Test::TestMultiStageEventTestRocket;
using QtRocket::Test::TestShortPodRocket;
using QtRocket::Test::TestSimple2Stage;
using QtRocket::Test::TestStepsRocket;
using QtRocket::Test::TestTubeFinsRocket;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kPi  = std::numbers::pi;

// The pinned values were printed by the Java probe StabilityProbe.java, which calls OpenRocket's
// BarrowmanStabilityCalculator on the rockets of TestRockets (and those of
// BarrowmanTestRockets.h). Values that sum over the components of a rocket are compared within
// a relative 1e-9 (see BarrowmanStabilityCalculator, "Summation order").

TEST(BarrowmanStabilityCalculator, StallAngleIs17AndAHalfDegrees)
{
    const BarrowmanStabilityCalculator calculator;
    EXPECT_EQ(BarrowmanStabilityCalculator::kStallAngle, 17.5 * kPi / 180);
    EXPECT_EQ(calculator.getStallAngle(), 0.30543261909900765);  // Java's STALL_ANGLE
}

// NOLINTBEGIN(modernize-use-std-numbers): OpenRocket's results, not approximations of constants

// ================================================================ the non-axial forces

struct ForcesCase
{
    /// The Mach number, the angle of attack in degrees, theta and the roll rate.
    std::array<double, 4> conditions;
    NonAxialPin           forces;
    /// Whether OpenRocket warns that body calculations may be inaccurate at supersonic speeds.
    bool supersonicWarning;
};

/// FlightConditions made for @p configuration with the Mach number, angle of attack (degrees),
/// theta and roll rate of @p c (StabilityProbe.conditions()).
[[nodiscard]] FlightConditions conditionsOf(const FlightConfiguration&   configuration,
                                            const std::array<double, 4>& c)
{
    FlightConditions conditions{configuration};
    conditions.setMach(c[0]);
    conditions.setAOA(c[1] * kPi / 180);
    conditions.setTheta(c[2]);
    conditions.setRollRate(c[3]);
    return conditions;
}

/// The differences between calculateNonAxialForces() of @p configuration and OpenRocket's, each
/// case by a new calculator.
[[nodiscard]] std::string forcesDifferences(const FlightConfiguration&  configuration,
                                            std::span<const ForcesCase> cases)
{
    JavaValueDifferences diff;
    for (const ForcesCase& c : cases)
    {
        const std::string what = std::format("Mach {} aoa {} theta {} roll {}", c.conditions[0],
                                             c.conditions[1], c.conditions[2], c.conditions[3]);
        const FlightConditions       conditions = conditionsOf(configuration, c.conditions);
        WarningSet                   warnings;
        BarrowmanStabilityCalculator calculator;
        const AerodynamicForces      forces =
            calculator.calculateNonAxialForces(configuration, conditions, warnings);
        compareNonAxial(diff, what, c.forces, forces, kRocketTolerance);
        if (forces.getComponent() != nullptr)
        {
            diff.problem(what + ": the total forces have a component");
        }
        if (!std::isnan(forces.getCD()) && forces.getCD() != 0)
        {
            diff.problem(what + ": the total forces have a drag coefficient");
        }
        if (warnings.contains(Warning::kSupersonic) != c.supersonicWarning ||
            warnings.size() != (c.supersonicWarning ? 1U : 0U))
        {
            diff.problem(std::format("{}: warnings {}", what, warnings.toString()));
        }
        // getCP() is the CP of those forces.
        WarningSet       cpWarnings;
        const Coordinate cp = calculator.getCP(configuration, conditions, cpWarnings);
        if (!cp.exactlyEquals(forces.getCP()))
        {
            diff.problem(what + ": getCP() is not the CP of the forces");
        }
    }
    return diff.text();
}

TEST(BarrowmanStabilityCalculator, ForcesOfTheAlphaAreOpenRockets)
{
    static const std::array<ForcesCase, 8> kCases{{
        {.conditions        = {0.3, 0.0, 0.0, 0.0},
         .forces            = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.22514554750367705, 0.0, 0.0,
                               30.820533821498472},
         .supersonicWarning = false},
        {.conditions        = {0.3, 2.0, 0.0, 0.0},
         .forces            = {1.0933771293819694, 10.199261509562463, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.22387726032632682, 0.0, 0.0, 31.322947464858103},
         .supersonicWarning = false},
        {.conditions        = {0.3, 10.0, 0.7, 0.0},
         .forces            = {5.811960509592721, 53.1033020466168, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.21928560027468486, 0.0, 0.0, 33.30008078963661},
         .supersonicWarning = false},
        {.conditions = {0.6, 5.0, 1.9, 20.0},
         .forces     = {2.950182908075077, 27.42450845721884, 0.0, 0.0, -0.6133015126770895,
                        0.6133015126770895, 0.0, 0.22310081221462436, 0.0, 0.0, 33.80660588486673},
         .supersonicWarning = false},
        {.conditions = {0.95, 3.0, 0.4, -15.0},
         .forces     = {1.9909639540287445, 18.98688108686158, 0.0, 0.0, 0.502165840469925,
                        -0.502165840469925, 0.0, 0.22887664297617857, 0.0, 0.0, 38.02461057617518},
         .supersonicWarning = false},
        {.conditions        = {1.5, 2.0, 0.0, 0.0},
         .forces            = {0.6804718090022704, 6.345611640262827, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.2238074779168401, 0.0, 0.0, 19.494081366731173},
         .supersonicWarning = true},
        {.conditions = {2.0, 12.0, 2.5, 5.0},
         .forces     = {3.5019711719348967, 29.923065889205787, 0.0, 0.0, -0.00676404318747777,
                        0.00676404318747777, 0.0, 0.2050712430462834, 0.0, 0.0, 16.720680677362694},
         .supersonicWarning = true},
        {.conditions        = {0.3, 25.0, 0.0, 0.0},
         .forces            = {13.479319085691248, 117.12452444730675, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.2133852350748932, 0.0, 0.0, 36.65643053710962},
         .supersonicWarning = false},
    }};
    const TestEstesAlphaIII                alpha;
    EXPECT_EQ(forcesDifferences(alpha.rocket->getSelectedConfiguration(), kCases), "");
}

TEST(BarrowmanStabilityCalculator, ForcesOfCantedFinsAreOpenRockets)
{
    // the roll forcing of three fins canted by 0.05 rad, with the roll damping of a roll rate
    static const std::array<ForcesCase, 8> kCases{{
        {.conditions        = {0.3, 0.0, 0.0, 0.0},
         .forces            = {0.0, 0.0, 0.0, 0.0, 2.7740218580559244, 0.0, 2.7740218580559244,
                               0.2259572505314114, 0.0, 0.0, 30.823368498251448},
         .supersonicWarning = false},
        {.conditions = {0.3, 2.0, 0.0, 0.0},
         .forces     = {1.093476078267108, 10.236579028994951, 0.0, 0.0, 2.7740218580559244, 0.0,
                        2.7740218580559244, 0.22467605974994734, 0.0, 0.0, 31.32578214161108},
         .supersonicWarning = false},
        {.conditions = {0.3, 10.0, 0.7, 0.0},
         .forces     = {5.812455254018412, 53.28988964377922, 0.0, 0.0, 2.7740218580559244, 0.0,
                        2.7740218580559244, 0.2200373672668707, 0.0, 0.0, 33.30291546638958},
         .supersonicWarning = false},
        {.conditions        = {0.6, 5.0, 1.9, 20.0},
         .forces            = {2.9503984501952574, 27.53691704436399, 0.0, 0.0, 2.327724206196864,
                               0.613169324188013, 2.9408935303848764, 0.22399890056239635, 0.0, 0.0,
                               33.80907581562545},
         .supersonicWarning = false},
        {.conditions = {0.95, 3.0, 0.4, -15.0},
         .forces     = {1.9823379925047033, 19.03363879551398, 0.0, 0.0, 3.9271075155450648,
                        -0.5020576056025277, 3.4250499099425373, 0.23043867030725426, 0.0, 0.0,
                        37.859866846318575},
         .supersonicWarning = false},
        {.conditions = {1.5, 2.0, 0.0, 0.0},
         .forces     = {0.5944509447975508, 5.440312489019326, 0.0, 0.0, 1.6687513088138897, 0.0,
                        1.6687513088138897, 0.21964385939521144, 0.0, 0.0, 17.02976513223197},
         .supersonicWarning = true},
        {.conditions = {2.0, 12.0, 2.5, 5.0},
         .forces     = {3.28836347274361, 27.66097642836195, 0.0, 0.0, 1.2270631488207013,
                        0.0067625853457503, 1.2338257341664516, 0.20188262027092754, 0.0, 0.0,
                        15.700779041099297},
         .supersonicWarning = true},
        {.conditions = {0.3, 25.0, 0.0, 0.0},
         .forces     = {13.480308574542633, 117.15220920149126, 0.0, 0.0, 1.387010929027962, 0.0,
                        1.387010929027962, 0.21342065818861589, 0.0, 0.0, 36.6592652138626},
         .supersonicWarning = false},
    }};
    const TestEstesAlphaIII                alpha;
    alpha.fins->setCantAngle(0.05);
    EXPECT_EQ(forcesDifferences(alpha.rocket->getSelectedConfiguration(), kCases), "");
}

// Components with several instances: the two boosters of the Falcon 9 Heavy and the three fins
// on each, the three pods with a fin each, the four pods with two end plates each, six tube
// fins, the two side boosters of the multi-stage rocket.
TEST(BarrowmanStabilityCalculator, ForcesOfTheFalconAreOpenRockets)
{
    static const std::array<ForcesCase, 8> kCases{{
        {.conditions        = {0.3, 0.0, 0.0, 0.0},
         .forces            = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0310997664583446, 0.0, 0.0,
                               20.193472360813377},
         .supersonicWarning = false},
        {.conditions        = {0.3, 2.0, 0.0, 0.0},
         .forces            = {0.7426009956617993, 7.284947012463239, 0.0, 0.0, 0.0, 0.0, 0.0,
                               1.020244375811777, 0.0, 0.0, 21.273951456816928},
         .supersonicWarning = false},
        {.conditions        = {0.3, 10.0, 0.7, 0.0},
         .forces            = {4.455832820396502, 42.27409392735706, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.9866855301034188, 0.0, 0.0, 25.530041482459374},
         .supersonicWarning = false},
        {.conditions = {0.6, 5.0, 1.9, 20.0},
         .forces     = {2.0257757119555784, 19.645960897881974, 0.0, 0.0, -0.8495278526554195,
                        0.8495278526554195, 0.0, 1.008591386164536, 0.0, 0.0, 23.213679707032842},
         .supersonicWarning = false},
        {.conditions = {0.95, 3.0, 0.4, -15.0},
         .forces     = {1.198747773282475, 11.877140157831075, 0.0, 0.0, 0.6955812534458622,
                        -0.6955812534458622, 0.0, 1.0304274209678657, 0.0, 0.0, 22.894396036597023},
         .supersonicWarning = false},
        {.conditions        = {1.5, 2.0, 0.0, 0.0},
         .forces            = {0.9653245587051095, 10.142702622381876, 0.0, 0.0, 0.0, 0.0, 0.0,
                               1.092732038375449, 0.0, 0.0, 27.654511537065723},
         .supersonicWarning = true},
        {.conditions = {2.0, 12.0, 2.5, 5.0},
         .forces = {5.392011134070961, 52.233576417008514, 0.0, 0.0, -0.009371694690155674,
                    0.009371694690155674, 0.0, 1.0074704618177432, 0.0, 0.0, 25.744956755817896},
         .supersonicWarning = true},
        {.conditions        = {0.3, 25.0, 0.0, 0.0},
         .forces            = {12.824908636120233, 114.56294298347389, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.9547766892444768, 0.0, 0.0, 32.77341671734761},
         .supersonicWarning = false},
    }};
    const TestFalcon9Heavy                 falcon;
    EXPECT_EQ(forcesDifferences(falcon.rocket->getSelectedConfiguration(), kCases), "");
}

TEST(BarrowmanStabilityCalculator, ForcesOfBoostersWithoutTheirCoreAreOpenRockets)
{
    // An inactive stage with an active child stage: the core is left out, its boosters are not.
    static const std::array<ForcesCase, 8> kWithPayload{{
        {.conditions        = {0.3, 0.0, 0.0, 0.0},
         .forces            = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0310997664583443, 0.0, 0.0,
                               20.193472360813377},
         .supersonicWarning = false},
        {.conditions        = {0.3, 2.0, 0.0, 0.0},
         .forces            = {0.7328857191646183, 7.194893872623984, 0.0, 0.0, 0.0, 0.0, 0.0,
                               1.0209899622628893, 0.0, 0.0, 20.995629286771376},
         .supersonicWarning = false},
        {.conditions        = {0.3, 10.0, 0.7, 0.0},
         .forces            = {4.215309440861189, 40.04462721704897, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.987979954734297, 0.0, 0.0, 24.151944030299703},
         .supersonicWarning = false},
        {.conditions = {0.6, 5.0, 1.9, 20.0},
         .forces     = {1.9651846095754395, 19.084327987358385, 0.0, 0.0, -0.8495278526554195,
                        0.8495278526554195, 0.0, 1.009966239820117, 0.0, 0.0, 22.519356818547436},
         .supersonicWarning = false},
        {.conditions = {0.95, 3.0, 0.4, -15.0},
         .forces     = {1.1768994977208158, 11.674623449740313, 0.0, 0.0, 0.6955812534458622,
                        -0.6955812534458622, 0.0, 1.0316605972934283, 0.0, 0.0, 22.477124710156396},
         .supersonicWarning = false},
        {.conditions        = {1.5, 2.0, 0.0, 0.0},
         .forces            = {0.9556092822079285, 10.052649482542625, 0.0, 0.0, 0.0, 0.0, 0.0,
                               1.094040802710569, 0.0, 0.0, 27.376189367020167},
         .supersonicWarning = true},
        {.conditions = {2.0, 12.0, 2.5, 5.0},
         .forces = {5.047205258322979, 49.037491184113755, 0.0, 0.0, -0.009371694690155674,
                    0.009371694690155674, 0.0, 1.0104401985114353, 0.0, 0.0, 24.098629969845263},
         .supersonicWarning = true},
        {.conditions        = {0.3, 25.0, 0.0, 0.0},
         .forces            = {11.400241664188462, 101.35737605133713, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.9537561297463503, 0.0, 0.0, 29.508320529212682},
         .supersonicWarning = false},
    }};
    // ... and the boosters alone, which changes the reference length.
    static const std::array<ForcesCase, 8> kBoostersOnly{{
        {.conditions        = {0.3, 0.0, 0.0, 0.0},
         .forces            = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0962590600310416, 0.0, 0.0,
                               34.8380160321399},
         .supersonicWarning = false},
        {.conditions        = {0.3, 2.0, 0.0, 0.0},
         .forces            = {1.2538566574896393, 17.773334691575677, 0.0, 0.0, 0.0, 0.0, 0.0,
                               1.091469876621551, 0.0, 0.0, 35.92034729426838},
         .supersonicWarning = false},
        {.conditions        = {0.3, 10.0, 0.7, 0.0},
         .forces            = {7.012884217713486, 97.93370692848566, 0.0, 0.0, 0.0, 0.0, 0.0,
                               1.0752915917884731, 0.0, 0.0, 40.18086678888867},
         .supersonicWarning = false},
        {.conditions = {0.6, 5.0, 1.9, 20.0},
         .forces     = {3.3276497264644354, 46.954821461450976, 0.0, 0.0, -2.0931746411527437,
                        2.0931746411527437, 0.0, 1.0865089627005748, 0.0, 0.0, 38.132057004855},
         .supersonicWarning = false},
        {.conditions = {0.95, 3.0, 0.4, -15.0},
         .forces     = {2.0123504459608643, 28.797487941200206, 0.0, 0.0, 1.7138614537747026,
                        -1.7138614537747026, 0.0, 1.1018988148526192, 0.0, 0.0, 38.43306248494219},
         .supersonicWarning = false},
        {.conditions        = {1.5, 2.0, 0.0, 0.0},
         .forces            = {1.6601609344126358, 24.81463599158142, 0.0, 0.0, 0.0, 0.0, 0.0,
                               1.1509287634380965, 0.0, 0.0, 47.56010742716954},
         .supersonicWarning = true},
        {.conditions = {2.0, 12.0, 2.5, 5.0},
         .forces     = {8.319037499300446, 119.66279693069865, 0.0, 0.0, -0.023091171888882663,
                        0.023091171888882663, 0.0, 1.107584304607187, 0.0, 0.0, 39.72047819341518},
         .supersonicWarning = true},
        {.conditions        = {0.3, 25.0, 0.0, 0.0},
         .forces            = {17.99933313538841, 243.75810713038624, 0.0, 0.0, 0.0, 0.0, 0.0,
                               1.0604642182145805, 0.0, 0.0, 47.41903611473724},
         .supersonicWarning = false},
    }};
    const TestFalcon9Heavy                 falcon;
    FlightConfiguration&                   config = falcon.rocket->getSelectedConfiguration();
    config.setStageActive(TestFalcon9Heavy::kCoreStageNumber, false);
    config.setStageActive(TestFalcon9Heavy::kBoosterStageNumber, true);
    ASSERT_FALSE(config.isComponentActive(*falcon.coreBody));
    ASSERT_TRUE(config.isComponentActive(*falcon.boosterBody));
    EXPECT_EQ(forcesDifferences(config, kWithPayload), "");

    config.setStageActive(TestFalcon9Heavy::kPayloadStageNumber, false);
    EXPECT_EQ(forcesDifferences(config, kBoostersOnly), "");
}

TEST(BarrowmanStabilityCalculator, ForcesOfFinsOnPodsAreOpenRockets)
{
    static const std::array<ForcesCase, 8> kPods{{
        {.conditions        = {0.3, 0.0, 0.0, 0.0},
         .forces            = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.220726594403691, 0.0, 0.0,
                               22.23121475709279},
         .supersonicWarning = false},
        {.conditions        = {0.3, 2.0, 0.0, 0.0},
         .forces            = {0.7935533330255734, 7.243712752923366, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.2190767763614928, 0.0, 0.0, 22.73362840045242},
         .supersonicWarning = false},
        {.conditions        = {0.3, 10.0, 0.7, 0.0},
         .forces            = {4.312841527810739, 38.32555826342129, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.21327317324108172, 0.0, 0.0, 24.710761725230917},
         .supersonicWarning = false},
        {.conditions = {0.6, 5.0, 1.9, 20.0},
         .forces = {2.1555196402000987, 19.593808025651025, 0.0, 0.0, -0.31562715446940026,
                    0.31562715446940026, 0.0, 0.21816149750877273, 0.0, 0.0, 24.700435608204682},
         .supersonicWarning = false},
        {.conditions = {0.95, 3.0, 0.4, -15.0},
         .forces = {1.4521528495085345, 13.651547171326575, 0.0, 0.0, 0.2584329985237151,
                    -0.2584329985237151, 0.0, 0.22562165699204675, 0.0, 0.0, 27.734076494911733},
         .supersonicWarning = false},
        {.conditions        = {1.5, 2.0, 0.0, 0.0},
         .forces            = {0.5121252128559437, 4.5890168065433, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.21505756911058316, 0.0, 0.0, 14.671306639442252},
         .supersonicWarning = true},
        {.conditions = {2.0, 12.0, 2.5, 5.0},
         .forces     = {2.9231480948122823, 23.873419625245212, 0.0, 0.0, -0.003480879728405261,
                        0.003480879728405261, 0.0, 0.1960085676202045, 0.0, 0.0, 13.95700406037093},
         .supersonicWarning = true},
        {.conditions        = {0.3, 25.0, 0.0, 0.0},
         .forces            = {10.481081122127287, 87.22354644077542, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.2054397810430203, 0.0, 0.0, 28.06711147270394},
         .supersonicWarning = false},
    }};
    static const std::array<ForcesCase, 8> kEndPlate{{
        {.conditions        = {0.3, 0.0, 0.0, 0.0},
         .forces            = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.25527062477601287, 0.0, 0.0,
                               45.32530223174485},
         .supersonicWarning = false},
        {.conditions        = {0.3, 2.0, 0.0, 0.0},
         .forces            = {1.6066673061585275, 20.390016392486032, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.25381753041627125, 0.0, 0.0, 46.02762786226846},
         .supersonicWarning = false},
        {.conditions        = {0.3, 10.0, 0.7, 0.0},
         .forces            = {8.516282212763914, 105.82434534686953, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.24852240144945784, 0.0, 0.0, 48.794702793370604},
         .supersonicWarning = false},
        {.conditions = {0.6, 5.0, 1.9, 20.0},
         .forces     = {4.16437790009745, 52.47928009803289, 0.0, 0.0, -0.6081828221478096,
                        0.6081828221478096, 0.0, 0.2520389904902955, 0.0, 0.0, 47.72025559462725},
         .supersonicWarning = false},
        {.conditions = {0.95, 3.0, 0.4, -15.0},
         .forces = {2.537671472711427, 32.491576278766374, 0.0, 0.0, 0.49797607657042653,
                    -0.49797607657042653, 0.0, 0.25607393729378286, 0.0, 0.0, 48.465955059037604},
         .supersonicWarning = false},
        {.conditions        = {1.5, 2.0, 0.0, 0.0},
         .forces            = {1.9564016576987977, 25.80951526440066, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.26384679406537515, 0.0, 0.0, 56.04677900926953},
         .supersonicWarning = true},
        {.conditions = {2.0, 12.0, 2.5, 5.0},
         .forces     = {9.460600407790213, 120.44769895240928, 0.0, 0.0, -0.006706902997704158,
                        0.006706902997704158, 0.0, 0.25463013711736127, 0.0, 0.0, 45.1710395855104},
         .supersonicWarning = true},
        {.conditions        = {0.3, 25.0, 0.0, 0.0},
         .forces            = {19.56574174278574, 231.2624629208441, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.2412705777870095, 0.0, 0.0, 53.50643744253148},
         .supersonicWarning = false},
    }};
    const TestEstesAlphaIIIWithPods        pods;
    EXPECT_EQ(forcesDifferences(pods.rocket->getSelectedConfiguration(), kPods), "");
    const TestEndPlateRocket endPlate;
    EXPECT_EQ(forcesDifferences(endPlate.rocket->getSelectedConfiguration(), kEndPlate), "");
}

TEST(BarrowmanStabilityCalculator, ForcesOfTubeFinsAreOpenRockets)
{
    static const std::array<ForcesCase, 8> kCases{{
        {.conditions        = {0.3, 0.0, 0.0, 0.0},
         .forces            = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.22821801894737045, 0.0, 0.0,
                               35.24777739936762},
         .supersonicWarning = false},
        {.conditions        = {0.3, 2.0, 0.0, 0.0},
         .forces            = {1.2479170838251505, 11.806523536794892, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.22706361548839832, 0.0, 0.0, 35.75019104272725},
         .supersonicWarning = false},
        {.conditions        = {0.3, 10.0, 0.7, 0.0},
         .forces            = {6.584660281808622, 61.13961218277889, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.22284379597236462, 0.0, 0.0, 37.72732436750574},
         .supersonicWarning = false},
        {.conditions = {0.6, 5.0, 1.9, 20.0},
         .forces     = {3.185191934646068, 29.914986831009248, 0.0, 0.0, -3.2271352760304897,
                        3.2271352760304897, 0.0, 0.22540546964683936, 0.0, 0.0, 36.49961095886585},
         .supersonicWarning = false},
        {.conditions = {0.95, 3.0, 0.4, -15.0},
         .forces     = {1.8849927918903386, 17.790130196579938, 0.0, 0.0, 1.5286430254881267,
                        -1.5286430254881267, 0.0, 0.22650650260033328, 0.0, 0.0, 36.00071046263278},
         .supersonicWarning = false},
        {.conditions        = {1.5, 2.0, 0.0, 0.0},
         .forces            = {1.2479170838251505, 11.83349096649896, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.22758225436375842, 0.0, 0.0, 35.75019104272725},
         .supersonicWarning = true},
        {.conditions = {2.0, 12.0, 2.5, 5.0},
         .forces     = {8.002151537551763, 74.96299553923131, 0.0, 0.0, -0.24203514570228676,
                        0.24203514570228676, 0.0, 0.22482852074205847, 0.0, 0.0, 38.20745917715322},
         .supersonicWarning = true},
        {.conditions        = {0.3, 25.0, 0.0, 0.0},
         .forces            = {15.024718630123058, 132.85165427949067, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.21671038520402658, 0.0, 0.0, 41.08367411497877},
         .supersonicWarning = false},
    }};
    const TestTubeFinsRocket               tubeFins;
    EXPECT_EQ(forcesDifferences(tubeFins.rocket->getSelectedConfiguration(), kCases), "");
}

TEST(BarrowmanStabilityCalculator, ForcesOfDiameterStepsAndZeroLengthComponentsAreOpenRockets)
{
    static const std::array<ForcesCase, 8> kCases{{
        {.conditions        = {0.3, 0.0, 0.0, 0.0},
         .forces            = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.22116070670159432, 0.0, 0.0,
                               3.5534708252375147},
         .supersonicWarning = false},
        {.conditions        = {0.3, 2.0, 0.0, 0.0},
         .forces            = {0.1286295519469514, 0.4700048060961037, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.21923646579595027, 0.0, 0.0, 3.6849652236095483},
         .supersonicWarning = false},
        {.conditions        = {0.3, 10.0, 0.7, 0.0},
         .forces            = {0.733203416375082, 2.6023705138535975, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.21295895155968392, 0.0, 0.0, 4.200946128286539},
         .supersonicWarning = false},
        {.conditions = {0.6, 5.0, 1.9, 20.0},
         .forces     = {0.3490050523983986, 1.2712672624528079, 0.0, 0.0, -0.01907210359998241,
                        0.01907210359998241, 0.0, 0.2185528124105703, 0.0, 0.0, 3.9993033062340775},
         .supersonicWarning = false},
        {.conditions = {0.95, 3.0, 0.4, -15.0},
         .forces = {0.2157748820871804, 0.8097617678376591, 0.0, 0.0, 0.015616076092541366,
                    -0.015616076092541366, 0.0, 0.22516849783576418, 0.0, 0.0, 4.120996689509475},
         .supersonicWarning = false},
        {.conditions        = {1.5, 2.0, 0.0, 0.0},
         .forces            = {0.09081963570429617, 0.30056492302155463, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.198568241784306, 0.0, 0.0, 2.6017909113859066},
         .supersonicWarning = true},
        {.conditions = {2.0, 12.0, 2.5, 5.0},
         .forces = {0.5685339190426727, 1.6440452520910642, 0.0, 0.0, -2.1034098752269794E-4,
                    2.1034098752269794E-4, 0.0, 0.17350365883457516, 0.0, 0.0, 2.714549505931464},
         .supersonicWarning = true},
        {.conditions        = {0.3, 25.0, 0.0, 0.0},
         .forces            = {1.979788675946797, 6.501939444280797, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.2060004522448995, 0.0, 0.0, 5.07025780565152},
         .supersonicWarning = false},
    }};
    const TestStepsRocket                  steps;
    EXPECT_EQ(forcesDifferences(steps.rocket->getSelectedConfiguration(), kCases), "");
}

TEST(BarrowmanStabilityCalculator, ForcesOfTheMultiStageRocketAreOpenRockets)
{
    static const std::array<ForcesCase, 8> kCases{{
        {.conditions        = {0.3, 0.0, 0.0, 0.0},
         .forces            = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.3213666218514193, 0.0, 0.0,
                               27.484318301185283},
         .supersonicWarning = false},
        {.conditions        = {0.3, 2.0, 0.0, 0.0},
         .forces            = {1.004530583897598, 15.977828992490052, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.3181153316506456, 0.0, 0.0, 28.77768142457231},
         .supersonicWarning = false},
        {.conditions        = {0.3, 10.0, 0.7, 0.0},
         .forces            = {5.911821945007363, 90.97307368490844, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.3077666226457878, 0.0, 0.0, 33.87224466817434},
         .supersonicWarning = false},
        {.conditions = {0.6, 5.0, 1.9, 20.0},
         .forces = {2.7727310057250674, 43.67565147382948, 0.0, 0.0, -0.27492302224252974,
                    0.27492302224252974, 0.0, 0.31503706189781155, 0.0, 0.0, 31.773156870622095},
         .supersonicWarning = false},
        {.conditions = {0.95, 3.0, 0.4, -15.0},
         .forces = {1.7367497867811945, 27.97374763294098, 0.0, 0.0, 0.22510499323775304,
                    -0.22510499323775304, 0.0, 0.32213906511871393, 0.0, 0.0, 33.16947761760269},
         .supersonicWarning = false},
        {.conditions        = {1.5, 2.0, 0.0, 0.0},
         .forces            = {0.7950114561066158, 12.628146020852448, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.31768463017365467, 0.0, 0.0, 22.775400549729593},
         .supersonicWarning = true},
        {.conditions = {2.0, 12.0, 2.5, 5.0},
         .forces = {4.913836133754363, 71.12719012803798, 0.0, 0.0, -0.0030318692954956505,
                    0.0030318692954956505, 0.0, 0.2894976071320231, 0.0, 0.0, 23.46183930691724},
         .supersonicWarning = true},
        {.conditions        = {0.3, 25.0, 0.0, 0.0},
         .forces            = {16.50964811596608, 239.20942804601899, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.296377073245998, 0.0, 0.0, 42.54218999187576},
         .supersonicWarning = false},
    }};
    const TestMultiStageEventTestRocket    multi;
    FlightConfiguration&                   config = multi.rocket->getSelectedConfiguration();
    config.setAllStages();
    EXPECT_EQ(forcesDifferences(config, kCases), "");
}

TEST(BarrowmanStabilityCalculator, AnEmptyRocketHasNoForces)
{
    Rocket rocket;
    rocket.enableEvents();
    const FlightConfiguration&   config = rocket.getSelectedConfiguration();
    const FlightConditions       conditions{config};
    WarningSet                   warnings;
    BarrowmanStabilityCalculator calculator;

    const AerodynamicForces forces =
        calculator.calculateNonAxialForces(config, conditions, warnings);
    EXPECT_EQ(forces.getCN(), 0.0);
    EXPECT_EQ(forces.getCm(), 0.0);
    EXPECT_EQ(forces.getCroll(), 0.0);
    EXPECT_TRUE(forces.getCP().exactlyEquals(Coordinate::kZero));
    EXPECT_TRUE(calculator.getCP(config, conditions, warnings).exactlyEquals(Coordinate::kZero));
    EXPECT_TRUE(warnings.empty());
}

// ================================================================ the force analysis

using N                          = NonAxialPin;
constexpr std::nullopt_t kAbsent = std::nullopt;

struct BreakdownPin
{
    /// The component's index in allComponents(rocket), and its name.
    int              index;
    std::string_view name;
    /// Its forces in the component map, and the forces of its subtree in the assembly map.
    std::optional<NonAxialPin> each;
    std::optional<NonAxialPin> assembly;
};

/// The conditions of every breakdown below (StabilityProbe.analyses()).
constexpr std::array<double, 4> kBreakdownConditions{0.6, 5, 1.9, 20};

/// The index of @p component in @p components, or -1.
[[nodiscard]] int indexOf(std::span<RocketComponent* const> components,
                          const RocketComponent*            component)
{
    for (std::size_t i = 0; i < components.size(); i++)
    {
        if (components[i] == component)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

/// The keys of @p map, in order, as indices into @p components.
[[nodiscard]] std::vector<int> keyOrder(const ForceMap&                   map,
                                        std::span<RocketComponent* const> components)
{
    std::vector<int> order;
    for (const RocketComponent* const key : map.keys())
    {
        order.push_back(indexOf(components, key));
    }
    return order;
}

/// The differences between one map of a breakdown and its pins.
void compareMap(JavaValueDifferences& diff, std::string_view mapName, const ForceMap& map,
                const RocketComponent& component, std::string_view name,
                const std::optional<NonAxialPin>& expected)
{
    const std::string              what   = std::format("{} ({} map)", name, mapName);
    const AerodynamicForces* const actual = map.get(&component);
    if (!expected.has_value())
    {
        if (actual != nullptr)
        {
            diff.problem(what + ": an entry that OpenRocket does not have");
        }
        return;
    }
    if (actual == nullptr)
    {
        diff.problem(what + ": no entry");
        return;
    }
    compareNonAxial(diff, what, *expected, *actual, kRocketTolerance);
    if (actual->getComponent() != &component)
    {
        diff.problem(what + ": the entry is not associated with its component");
    }
}

/// The differences between getForceAnalysis() of @p configuration and OpenRocket's: the order of
/// the two maps (by component index) and every entry.
[[nodiscard]] std::string breakdownDifferences(Rocket&                       rocket,
                                               const FlightConfiguration&    configuration,
                                               std::span<const BreakdownPin> pins,
                                               const std::vector<int>&       eachOrder,
                                               const std::vector<int>&       assemblyOrder)
{
    const FlightConditions        conditions = conditionsOf(configuration, kBreakdownConditions);
    WarningSet                    warnings;
    BarrowmanStabilityCalculator  calculator;
    const StabilityForceBreakdown breakdown =
        calculator.getForceAnalysis(configuration, conditions, warnings);
    const ForceMap& each     = breakdown.getComponentForces();
    const ForceMap& assembly = breakdown.getAssemblyForces();

    JavaValueDifferences                diff;
    const std::vector<RocketComponent*> components = allComponents(rocket);
    if (keyOrder(each, components) != eachOrder)
    {
        diff.problem("the component map is not in OpenRocket's order");
    }
    if (keyOrder(assembly, components) != assemblyOrder)
    {
        diff.problem("the assembly map is not in OpenRocket's order");
    }
    if (components.size() != pins.size())
    {
        diff.problem(std::format("{} components, {} pins", components.size(), pins.size()));
        return diff.text();
    }
    for (const BreakdownPin& pin : pins)
    {
        const RocketComponent& component = *components.at(static_cast<std::size_t>(pin.index));
        diff.name(std::format("component {}", pin.index), pin.name, component.getName());
        compareMap(diff, "component", each, component, pin.name, pin.each);
        compareMap(diff, "assembly", assembly, component, pin.name, pin.assembly);
    }

    // The rocket's assembly entry is the total: what calculateNonAxialForces() gives.
    const AerodynamicForces total =
        calculator.calculateNonAxialForces(configuration, conditions, warnings);
    const AerodynamicForces* const rocketEntry = assembly.get(&rocket);
    if (rocketEntry == nullptr)
    {
        diff.problem("no assembly entry for the rocket");
        return diff.text();
    }
    diff.pinned("rocket entry CN against the total", total.getCN(), rocketEntry->getCN(),
                kRocketTolerance);
    diff.pinned("rocket entry Cm against the total", total.getCm(), rocketEntry->getCm(),
                kRocketTolerance);
    diff.pinned("rocket entry CP.x against the total", total.getCP().x, rocketEntry->getCP().x,
                kRocketTolerance);
    return diff.text();
}

// The component map holds every aerodynamic component and assembly in pre-order (an assembly
// has no forces of its own); the assembly map every component visited, internal ones too, each
// after its children, with the forces of its subtree.
TEST(BarrowmanStabilityCalculator, BreakdownOfTheAlphaIsOpenRockets)
{
    static const std::array<BreakdownPin, 10> kPins{{
        {.index    = 0,
         .name     = "Estes Alpha III / Code Verification Rocket",
         .each     = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = N{2.950182908075077, 27.42450845721884, 0.0, 0.0, -0.6133015126770895,
                       0.6133015126770895, 0.0, 0.2231008122146244, 0.0, 0.0, 33.80660588486672}},
        {.index    = 1,
         .name     = "Stage",
         .each     = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = N{2.950182908075077, 27.42450845721884, 0.0, 0.0, -0.6133015126770895,
                       0.6133015126770895, 0.0, 0.2231008122146244, 0.0, 0.0, 33.80660588486672}},
        {.index    = 2,
         .name     = "Nose Cone",
         .each     = N{0.1951189008585206, 0.27283969709491196, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.03355980738650177, 0.0, 0.0, 2.235897904484953},
         .assembly = N{0.1951189008585206, 0.27283969709491196, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.03355980738650177, 0.0, 0.0, 2.235897904484953}},
        {.index    = 3,
         .name     = "Body Tube",
         .each     = N{0.08865711084193498, 0.6279878684637062, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.17000000000000004, 0.0, 0.0, 1.0159356550132814},
         .assembly = N{2.7550640072165566, 27.151668760123925, 0.0, 0.0, -0.6133015126770895,
                       0.6133015126770895, 0.0, 0.23652446859168505, 0.0, 0.0, 31.57070798038177}},
        {.index    = 4,
         .name     = "3 Fin Set",
         .each     = N{2.6664068963746215, 26.52368089166022, 0.0, 0.0, -0.6133015126770895,
                       0.6133015126770895, 0.0, 0.23873638425753965, 0.0, 0.0, 30.55477232536849},
         .assembly = N{2.6664068963746215, 26.52368089166022, 0.0, 0.0, -0.6133015126770895,
                       0.6133015126770895, 0.0, 0.23873638425753965, 0.0, 0.0, 30.55477232536849}},
        {.index    = 5,
         .name     = "Launch Lugs",
         .each     = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 6,
         .name     = "Motor Mount Tube",
         .each     = kAbsent,
         .assembly = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 7,
         .name     = "Engine Block",
         .each     = kAbsent,
         .assembly = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 8,
         .name     = "Parachute",
         .each     = kAbsent,
         .assembly = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 9,
         .name     = "Centering Rings",
         .each     = kAbsent,
         .assembly = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    }};
    const TestEstesAlphaIII                   alpha;
    EXPECT_EQ(breakdownDifferences(*alpha.rocket, alpha.rocket->getSelectedConfiguration(), kPins,
                                   {0, 1, 2, 3, 4, 5}, {2, 4, 5, 7, 6, 8, 9, 3, 1, 0}),
              "");
}

TEST(BarrowmanStabilityCalculator, BreakdownOfTheFalconIsOpenRockets)
{
    // The forces of the boosters' components are those of both boosters.
    static const std::array<BreakdownPin, 16> kPins{{
        {.index    = 0,
         .name     = "Falcon9H Scale Rocket",
         .each     = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = N{2.025775711955579, 19.645960897881977, 0.0, 0.0, -0.8495278526554195,
                       0.8495278526554195, 0.0, 1.0085913861645355, 0.0, 0.0, 23.213679707032846}},
        {.index    = 1,
         .name     = "Payload Fairing Stage",
         .each     = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = N{0.1410689264941121, 0.027423769372384332, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.020217577928807764, 0.0, 0.0, 1.6165308217107732}},
        {.index    = 2,
         .name     = "PL Fairing Nose",
         .each     = N{0.18235712112362296, 0.10437103740044923, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.059523794973096844, 0.0, 0.0, 2.0896586809079096},
         .assembly = N{0.18235712112362296, 0.10437103740044923, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.059523794973096844, 0.0, 0.0, 2.0896586809079096}},
        {.index = 3,
         .name  = "PL Fairing Body",
         .each  = N{0.013503159959002402, 0.023890206081311944, 0.0, 0.0, 0.0, 0.0, 0.0, 0.184, 0.0,
                    0.0, 0.1547348151481767},
         .assembly = N{0.013503159959002402, 0.023890206081311944, 0.0, 0.0, 0.0, 0.0, 0.0, 0.184,
                       0.0, 0.0, 0.1547348151481767}},
        {.index    = 4,
         .name     = "PL Fairing Transition",
         .each     = N{-0.07751301798106537, -0.19128717261434394, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.25665193370165745, -0.0, -0.0, -0.8882337575273415},
         .assembly = N{-0.07751301798106537, -0.19128717261434394, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.25665193370165745, -0.0, -0.0, -0.8882337575273415}},
        {.index = 5,
         .name  = "Upper Stage Body",
         .each  = N{0.013632998035531271, 0.04640462792863529, 0.0, 0.0, 0.0, 0.0, 0.0, 0.354, 0.0,
                    0.0, 0.15622264990921683},
         .assembly = N{0.013632998035531271, 0.04640462792863529, 0.0, 0.0, 0.0, 0.0, 0.0, 0.354,
                       0.0, 0.0, 0.15622264990921683}},
        {.index    = 6,
         .name     = "Parachute",
         .each     = kAbsent,
         .assembly = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 7,
         .name     = "Shock Cord",
         .each     = kAbsent,
         .assembly = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index = 8,
         .name  = "Interstage",
         .each  = N{0.009088665357020848, 0.0440450705763318, 0.0, 0.0, 0.0, 0.0, 0.0, 0.504, 0.0,
                    0.0, 0.10414843327281123},
         .assembly = N{0.009088665357020848, 0.0440450705763318, 0.0, 0.0, 0.0, 0.0, 0.0, 0.504,
                       0.0, 0.0, 0.10414843327281123}},
        {.index    = 9,
         .name     = "Core Stage",
         .each     = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = N{1.8847067854614665, 19.618537128509594, 0.0, 0.0, -0.8495278526554195,
                       0.8495278526554195, 0.0, 1.082570443903521, 0.0, 0.0, 21.59714888532207}},
        {.index    = 10,
         .name     = "Core Stage Body",
         .each     = N{0.060591102380138984, 0.5616329105235963, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.9640000000000006, 0.0, 0.0, 0.6943228884854081},
         .assembly = N{1.8847067854614665, 19.618537128509594, 0.0, 0.0, -0.8495278526554195,
                       0.8495278526554195, 0.0, 1.082570443903521, 0.0, 0.0, 21.59714888532207}},
        {.index    = 11,
         .name     = "Booster Stage",
         .each     = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = N{1.8241156830813274, 19.056904217986, 0.0, 0.0, -0.8495278526554195,
                       0.8495278526554195, 0.0, 1.086508962700575, 0.0, 0.0, 20.90282599683666}},
        {.index    = 12,
         .name     = "Booster Nose",
         .each     = N{0.19918154227591664, 1.004193925185274, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.5243265366155166, 0.0, 0.0, 2.2824523458633212},
         .assembly = N{0.19918154227591664, 1.004193925185274, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.5243265366155166, 0.0, 0.0, 2.2824523458633212}},
        {.index    = 13,
         .name     = "Booster Body",
         .each     = N{0.12118220476027797, 1.1232658210471926, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.9640000000000006, 0.0, 0.0, 1.3886457769708163},
         .assembly = N{1.6249341408054108, 18.052710292800725, 0.0, 0.0, -0.8495278526554195,
                       0.8495278526554195, 0.0, 1.1554202864620025, 0.0, 0.0, 18.620373650973338}},
        {.index    = 14,
         .name     = "Booster Motor Tubes",
         .each     = kAbsent,
         .assembly = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 15,
         .name     = "Booster Fins",
         .each     = N{1.5037519360451328, 16.929444471753534, 0.0, 0.0, -0.8495278526554195,
                       0.8495278526554195, 0.0, 1.170846190025802, 0.0, 0.0, 17.231727874002523},
         .assembly = N{1.5037519360451328, 16.929444471753534, 0.0, 0.0, -0.8495278526554195,
                       0.8495278526554195, 0.0, 1.170846190025802, 0.0, 0.0, 17.231727874002523}},
    }};
    const TestFalcon9Heavy                    falcon;
    EXPECT_EQ(breakdownDifferences(*falcon.rocket, falcon.rocket->getSelectedConfiguration(), kPins,
                                   {0, 1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 15},
                                   {2, 3, 4, 6, 7, 5, 8, 1, 12, 14, 15, 13, 11, 10, 9, 0}),
              "");
}

TEST(BarrowmanStabilityCalculator, BreakdownLeavesOutInactiveStagesButNotTheirActiveChildStages)
{
    // Without the core: neither the core stage nor its body has an entry, but the booster
    // stage, a child of the core's body, and its components do, and count in the rocket's total.
    static const std::array<BreakdownPin, 16> kBoostersWithoutCore{{
        {.index    = 0,
         .name     = "Falcon9H Scale Rocket",
         .each     = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = N{1.9651846095754395, 19.08432798735838, 0.0, 0.0, -0.8495278526554195,
                       0.8495278526554195, 0.0, 1.0099662398201172, 0.0, 0.0, 22.519356818547436}},
        {.index    = 1,
         .name     = "Payload Fairing Stage",
         .each     = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = N{0.1410689264941121, 0.027423769372384332, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.020217577928807764, 0.0, 0.0, 1.6165308217107732}},
        {.index    = 2,
         .name     = "PL Fairing Nose",
         .each     = N{0.18235712112362296, 0.10437103740044923, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.059523794973096844, 0.0, 0.0, 2.0896586809079096},
         .assembly = N{0.18235712112362296, 0.10437103740044923, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.059523794973096844, 0.0, 0.0, 2.0896586809079096}},
        {.index = 3,
         .name  = "PL Fairing Body",
         .each  = N{0.013503159959002402, 0.023890206081311944, 0.0, 0.0, 0.0, 0.0, 0.0, 0.184, 0.0,
                    0.0, 0.1547348151481767},
         .assembly = N{0.013503159959002402, 0.023890206081311944, 0.0, 0.0, 0.0, 0.0, 0.0, 0.184,
                       0.0, 0.0, 0.1547348151481767}},
        {.index    = 4,
         .name     = "PL Fairing Transition",
         .each     = N{-0.07751301798106537, -0.19128717261434394, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.25665193370165745, -0.0, -0.0, -0.8882337575273415},
         .assembly = N{-0.07751301798106537, -0.19128717261434394, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.25665193370165745, -0.0, -0.0, -0.8882337575273415}},
        {.index = 5,
         .name  = "Upper Stage Body",
         .each  = N{0.013632998035531271, 0.04640462792863529, 0.0, 0.0, 0.0, 0.0, 0.0, 0.354, 0.0,
                    0.0, 0.15622264990921683},
         .assembly = N{0.013632998035531271, 0.04640462792863529, 0.0, 0.0, 0.0, 0.0, 0.0, 0.354,
                       0.0, 0.0, 0.15622264990921683}},
        {.index    = 6,
         .name     = "Parachute",
         .each     = kAbsent,
         .assembly = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 7,
         .name     = "Shock Cord",
         .each     = kAbsent,
         .assembly = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index = 8,
         .name  = "Interstage",
         .each  = N{0.009088665357020848, 0.0440450705763318, 0.0, 0.0, 0.0, 0.0, 0.0, 0.504, 0.0,
                    0.0, 0.10414843327281123},
         .assembly = N{0.009088665357020848, 0.0440450705763318, 0.0, 0.0, 0.0, 0.0, 0.0, 0.504,
                       0.0, 0.0, 0.10414843327281123}},
        {.index = 9, .name = "Core Stage", .each = kAbsent, .assembly = kAbsent},
        {.index = 10, .name = "Core Stage Body", .each = kAbsent, .assembly = kAbsent},
        {.index    = 11,
         .name     = "Booster Stage",
         .each     = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = N{1.8241156830813274, 19.056904217986, 0.0, 0.0, -0.8495278526554195,
                       0.8495278526554195, 0.0, 1.086508962700575, 0.0, 0.0, 20.90282599683666}},
        {.index    = 12,
         .name     = "Booster Nose",
         .each     = N{0.19918154227591664, 1.004193925185274, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.5243265366155166, 0.0, 0.0, 2.2824523458633212},
         .assembly = N{0.19918154227591664, 1.004193925185274, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.5243265366155166, 0.0, 0.0, 2.2824523458633212}},
        {.index    = 13,
         .name     = "Booster Body",
         .each     = N{0.12118220476027797, 1.1232658210471926, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.9640000000000006, 0.0, 0.0, 1.3886457769708163},
         .assembly = N{1.6249341408054108, 18.052710292800725, 0.0, 0.0, -0.8495278526554195,
                       0.8495278526554195, 0.0, 1.1554202864620025, 0.0, 0.0, 18.620373650973338}},
        {.index    = 14,
         .name     = "Booster Motor Tubes",
         .each     = kAbsent,
         .assembly = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 15,
         .name     = "Booster Fins",
         .each     = N{1.5037519360451328, 16.929444471753534, 0.0, 0.0, -0.8495278526554195,
                       0.8495278526554195, 0.0, 1.170846190025802, 0.0, 0.0, 17.231727874002523},
         .assembly = N{1.5037519360451328, 16.929444471753534, 0.0, 0.0, -0.8495278526554195,
                       0.8495278526554195, 0.0, 1.170846190025802, 0.0, 0.0, 17.231727874002523}},
    }};
    // Without the boosters either: the payload stage alone.
    static const std::array<BreakdownPin, 16> kPayloadOnly{{
        {.index    = 0,
         .name     = "Falcon9H Scale Rocket",
         .each     = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = N{0.1410689264941121, 0.027423769372384332, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.020217577928807764, 0.0, 0.0, 1.6165308217107732}},
        {.index    = 1,
         .name     = "Payload Fairing Stage",
         .each     = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = N{0.1410689264941121, 0.027423769372384332, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.020217577928807764, 0.0, 0.0, 1.6165308217107732}},
        {.index    = 2,
         .name     = "PL Fairing Nose",
         .each     = N{0.18235712112362296, 0.10437103740044923, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.059523794973096844, 0.0, 0.0, 2.0896586809079096},
         .assembly = N{0.18235712112362296, 0.10437103740044923, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.059523794973096844, 0.0, 0.0, 2.0896586809079096}},
        {.index = 3,
         .name  = "PL Fairing Body",
         .each  = N{0.013503159959002402, 0.023890206081311944, 0.0, 0.0, 0.0, 0.0, 0.0, 0.184, 0.0,
                    0.0, 0.1547348151481767},
         .assembly = N{0.013503159959002402, 0.023890206081311944, 0.0, 0.0, 0.0, 0.0, 0.0, 0.184,
                       0.0, 0.0, 0.1547348151481767}},
        {.index    = 4,
         .name     = "PL Fairing Transition",
         .each     = N{-0.07751301798106537, -0.19128717261434394, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.25665193370165745, -0.0, -0.0, -0.8882337575273415},
         .assembly = N{-0.07751301798106537, -0.19128717261434394, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.25665193370165745, -0.0, -0.0, -0.8882337575273415}},
        {.index = 5,
         .name  = "Upper Stage Body",
         .each  = N{0.013632998035531271, 0.04640462792863529, 0.0, 0.0, 0.0, 0.0, 0.0, 0.354, 0.0,
                    0.0, 0.15622264990921683},
         .assembly = N{0.013632998035531271, 0.04640462792863529, 0.0, 0.0, 0.0, 0.0, 0.0, 0.354,
                       0.0, 0.0, 0.15622264990921683}},
        {.index    = 6,
         .name     = "Parachute",
         .each     = kAbsent,
         .assembly = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 7,
         .name     = "Shock Cord",
         .each     = kAbsent,
         .assembly = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index = 8,
         .name  = "Interstage",
         .each  = N{0.009088665357020848, 0.0440450705763318, 0.0, 0.0, 0.0, 0.0, 0.0, 0.504, 0.0,
                    0.0, 0.10414843327281123},
         .assembly = N{0.009088665357020848, 0.0440450705763318, 0.0, 0.0, 0.0, 0.0, 0.0, 0.504,
                       0.0, 0.0, 0.10414843327281123}},
        {.index = 9, .name = "Core Stage", .each = kAbsent, .assembly = kAbsent},
        {.index = 10, .name = "Core Stage Body", .each = kAbsent, .assembly = kAbsent},
        {.index = 11, .name = "Booster Stage", .each = kAbsent, .assembly = kAbsent},
        {.index = 12, .name = "Booster Nose", .each = kAbsent, .assembly = kAbsent},
        {.index = 13, .name = "Booster Body", .each = kAbsent, .assembly = kAbsent},
        {.index = 14, .name = "Booster Motor Tubes", .each = kAbsent, .assembly = kAbsent},
        {.index = 15, .name = "Booster Fins", .each = kAbsent, .assembly = kAbsent},
    }};
    const TestFalcon9Heavy                    falcon;
    FlightConfiguration&                      config = falcon.rocket->getSelectedConfiguration();
    config.setStageActive(TestFalcon9Heavy::kCoreStageNumber, false);
    config.setStageActive(TestFalcon9Heavy::kBoosterStageNumber, true);
    EXPECT_EQ(breakdownDifferences(*falcon.rocket, config, kBoostersWithoutCore,
                                   {0, 1, 2, 3, 4, 5, 8, 11, 12, 13, 15},
                                   {2, 3, 4, 6, 7, 5, 8, 1, 12, 14, 15, 13, 11, 0}),
              "");

    config.setStageActive(TestFalcon9Heavy::kBoosterStageNumber, false);
    EXPECT_EQ(breakdownDifferences(*falcon.rocket, config, kPayloadOnly, {0, 1, 2, 3, 4, 5, 8},
                                   {2, 3, 4, 6, 7, 5, 8, 1, 0}),
              "");
}

TEST(BarrowmanStabilityCalculator, BreakdownOfFinsOnPodsIsOpenRockets)
{
    static const std::array<BreakdownPin, 12> kPins{{
        {.index = 0,
         .name  = "Estes Alpha III / Code Verification Rocket",
         .each  = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly =
             N{2.1555196402000987, 19.593808025651025, 0.0, 0.0, -0.31562715446940026,
               0.31562715446940026, 0.0, 0.21816149750877273, 0.0, 0.0, 24.700435608204682}},
        {.index = 1,
         .name  = "Stage",
         .each  = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly =
             N{2.1555196402000987, 19.593808025651025, 0.0, 0.0, -0.31562715446940026,
               0.31562715446940026, 0.0, 0.21816149750877273, 0.0, 0.0, 24.700435608204682}},
        {.index    = 2,
         .name     = "Nose Cone",
         .each     = N{0.1951189008585206, 0.27283969709491196, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.03355980738650177, 0.0, 0.0, 2.235897904484953},
         .assembly = N{0.1951189008585206, 0.27283969709491196, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.03355980738650177, 0.0, 0.0, 2.235897904484953}},
        {.index    = 3,
         .name     = "Body Tube",
         .each     = N{0.08865711084193498, 0.6279878684637062, 0.0, 0.0, 0.0, 0.0, 0.0,
                       0.17000000000000004, 0.0, 0.0, 1.0159356550132814},
         .assembly = N{1.960400739341578, 19.320968328556113, 0.0, 0.0, -0.31562715446940026,
                       0.31562715446940026, 0.0, 0.2365349239977773, 0.0, 0.0, 22.464537703719728}},
        {.index    = 4,
         .name     = "Launch Lugs",
         .each     = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 5,
         .name     = "Motor Mount Tube",
         .each     = kAbsent,
         .assembly = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 6,
         .name     = "Engine Block",
         .each     = kAbsent,
         .assembly = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 7,
         .name     = "Parachute",
         .each     = kAbsent,
         .assembly = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 8,
         .name     = "Centering Rings",
         .each     = kAbsent,
         .assembly = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 9,
         .name     = "Pod Set",
         .each     = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = N{1.8717436284996432, 18.692980460092407, 0.0, 0.0, -0.31562715446940026,
                       0.31562715446940026, 0.0, 0.2396864208384312, 0.0, 0.0, 21.448602048706448}},
        {.index    = 10,
         .name     = "Pod Body",
         .each     = N{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = N{1.8717436284996432, 18.692980460092407, 0.0, 0.0, -0.31562715446940026,
                       0.31562715446940026, 0.0, 0.2396864208384312, 0.0, 0.0, 21.448602048706448}},
        {.index    = 11,
         .name     = "3 Fin Set",
         .each     = N{1.8717436284996432, 18.692980460092407, 0.0, 0.0, -0.31562715446940026,
                       0.31562715446940026, 0.0, 0.2396864208384312, 0.0, 0.0, 21.448602048706448},
         .assembly = N{1.8717436284996432, 18.692980460092407, 0.0, 0.0, -0.31562715446940026,
                       0.31562715446940026, 0.0, 0.2396864208384312, 0.0, 0.0, 21.448602048706448}},
    }};
    const TestEstesAlphaIIIWithPods           pods;
    EXPECT_EQ(
        breakdownDifferences(*pods.rocket, pods.rocket->getSelectedConfiguration(), kPins,
                             {0, 1, 2, 3, 4, 9, 10, 11}, {2, 4, 6, 5, 7, 8, 11, 10, 9, 3, 1, 0}),
        "");
}

// ======================================================================= the cache

TEST(BarrowmanStabilityCalculator, TheCalculationsStayUntilTheCacheIsVoided)
{
    // The calculator does not look at the rocket's modification ids: after a change of the
    // rocket it goes on with the calculations it made, which hold copies of the geometry, until
    // voidAerodynamicCache() (which BarrowmanCalculator calls).
    const TestEstesAlphaIII      alpha;
    const FlightConfiguration&   config = alpha.rocket->getSelectedConfiguration();
    const FlightConditions       conditions{config};
    WarningSet                   warnings;
    BarrowmanStabilityCalculator calculator;
    const Coordinate             before = calculator.getCP(config, conditions, warnings);

    alpha.fins->setHeight(2 * alpha.fins->getHeight());
    EXPECT_TRUE(calculator.getCP(config, conditions, warnings).exactlyEquals(before));

    calculator.voidAerodynamicCache();
    const Coordinate after = calculator.getCP(config, conditions, warnings);
    EXPECT_GT(after.weight, before.weight);
    BarrowmanStabilityCalculator fresh;
    EXPECT_TRUE(after.exactlyEquals(fresh.getCP(config, conditions, warnings)));
}

TEST(BarrowmanStabilityCalculator, AComponentAddedBehindItsBackIsSkippedOrABug)
{
    // An aerodynamic component that became active after the calculations were made has none:
    // the total forces go without it, as in Java, and the force analysis, which insists on a
    // calculation for every aerodynamic component it visits, throws (Java: a
    // NullPointerException), and so do the damping moments, which take the midchord position of
    // every active fin set from its calculation (Java: a NullPointerException too). Voiding the
    // cache makes one.
    const TestEstesAlphaIII      alpha;
    const FlightConfiguration&   config = alpha.rocket->getSelectedConfiguration();
    const FlightConditions       conditions{config};
    WarningSet                   warnings;
    BarrowmanStabilityCalculator calculator;
    const Coordinate             before = calculator.getCP(config, conditions, warnings);

    // four small fins at the top of the body tube, clear of the three at its bottom
    auto forwardFins = std::make_unique<TrapezoidFinSet>(4, 0.02, 0.02, 0, 0.02);
    forwardFins->setAxialMethod(AxialMethod::TOP);
    const TrapezoidFinSet& added = alpha.body->addChild(std::move(forwardFins));
    ASSERT_TRUE(config.getActiveInstances().containsKey(added));

    EXPECT_TRUE(calculator.getCP(config, conditions, warnings).exactlyEquals(before));
    EXPECT_THROW(static_cast<void>(calculator.getForceAnalysis(config, conditions, warnings)),
                 BugError);
    FlightConditions rotating{config};
    rotating.setPitchRate(0.5);
    rotating.setPitchCenter(Coordinate{0.15, 0, 0});
    AerodynamicForces total;
    total.setCm(5.0);
    total.setCyaw(5.0);
    EXPECT_THROW(calculator.calculateDampingMoments(config, rotating, total), BugError);
    // (the moments are set last: untouched)
    EXPECT_TRUE(std::isnan(total.getPitchDampingMoment()));

    calculator.voidAerodynamicCache();
    EXPECT_GT(calculator.getCP(config, conditions, warnings).weight, before.weight);
    const StabilityForceBreakdown breakdown =
        calculator.getForceAnalysis(config, conditions, warnings);
    EXPECT_TRUE(breakdown.getComponentForces().containsKey(&added));
    EXPECT_NO_THROW(calculator.calculateDampingMoments(config, rotating, total));
    // With four more fins ahead of the pitch centre: more than the three alone give
    // (DampingMomentsOfTheAlphaAreOpenRockets, the same rate and centre).
    EXPECT_GT(total.getPitchDampingMoment(), 5.1158283860786216E-5);
    EXPECT_LT(total.getPitchDampingMoment(), 5.0);
    EXPECT_EQ(total.getYawDampingMoment(), 0.0);
}

TEST(BarrowmanStabilityCalculator, AMovedFromCalculatorBuildsItsCalculationsAgain)
{
    // The calculator can be moved, with its calculations (ComponentCalcMap). The one moved from
    // must not go on with a map that counts as built and is empty: it would give a rocket
    // without lift.
    const TestEstesAlphaIII      alpha;
    const FlightConfiguration&   config = alpha.rocket->getSelectedConfiguration();
    const FlightConditions       conditions{config};
    WarningSet                   warnings;
    BarrowmanStabilityCalculator calculator;
    const Coordinate             before = calculator.getCP(config, conditions, warnings);
    ASSERT_GT(before.weight, 0);

    BarrowmanStabilityCalculator other{std::move(calculator)};
    EXPECT_TRUE(other.getCP(config, conditions, warnings).exactlyEquals(before));
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move): moved from on purpose
    EXPECT_TRUE(calculator.getCP(config, conditions, warnings).exactlyEquals(before));
}

TEST(BarrowmanStabilityCalculator, NewInstanceIsAnIndependentBarrowmanStabilityCalculator)
{
    const TestEstesAlphaIII      alpha;
    const FlightConfiguration&   config = alpha.rocket->getSelectedConfiguration();
    const FlightConditions       conditions{config};
    WarningSet                   warnings;
    BarrowmanStabilityCalculator calculator;
    const Coordinate             before = calculator.getCP(config, conditions, warnings);

    const std::unique_ptr<StabilityCalculator> instance = calculator.newInstance();
    ASSERT_NE(dynamic_cast<BarrowmanStabilityCalculator*>(instance.get()), nullptr);
    EXPECT_EQ(instance->getStallAngle(), calculator.getStallAngle());

    // The new instance has no calculations yet: it sees the rocket as it is now.
    alpha.fins->setHeight(2 * alpha.fins->getHeight());
    EXPECT_GT(instance->getCP(config, conditions, warnings).weight, before.weight);
    EXPECT_TRUE(calculator.getCP(config, conditions, warnings).exactlyEquals(before));
}

// ================================================================ the damping moments

struct DampingCase
{
    /// The pitch rate, the yaw rate, the x of the pitch centre, and the total Cm and Cyaw.
    std::array<double, 5> input;
    /// The pitch and the yaw damping moment.
    std::array<double, 2> damping;
};

/// The pitch and yaw damping moments @p calculator gives @p configuration at Mach @p mach for
/// the rates, pitch centre and total moments of @p input (StabilityProbe.damp()).
[[nodiscard]] std::array<double, 2> dampingOf(BarrowmanStabilityCalculator& calculator,
                                              const FlightConfiguration& configuration, double mach,
                                              const std::array<double, 5>& input)
{
    FlightConditions conditions{configuration};
    conditions.setMach(mach);
    conditions.setPitchRate(input[0]);
    conditions.setYawRate(input[1]);
    conditions.setPitchCenter(Coordinate{input[2], 0, 0});
    AerodynamicForces total;
    total.setCm(input[3]);
    total.setCyaw(input[4]);
    calculator.calculateDampingMoments(configuration, conditions, total);
    return {total.getPitchDampingMoment(), total.getYawDampingMoment()};
}

/// The differences between the damping moments of @p configuration and OpenRocket's, each case
/// by a new calculator: no rates; small moments well below the total; totals that cap them (the
/// result is the total with the sign of the rate, a negative total included); a NaN total, which
/// does not cap; a NaN rate or pitch centre, whose NaN damping gives way to the total; high
/// rates; and a pitch centre behind the rocket.
[[nodiscard]] std::string dampingDifferences(const FlightConfiguration& configuration, double mach,
                                             std::span<const DampingCase> cases)
{
    JavaValueDifferences diff;
    for (const DampingCase& c : cases)
    {
        const std::string what =
            std::format("pitch rate {} yaw rate {} centre {} Cm {} Cyaw {}", c.input[0], c.input[1],
                        c.input[2], c.input[3], c.input[4]);
        BarrowmanStabilityCalculator calculator;
        const std::array<double, 2>  damping = dampingOf(calculator, configuration, mach, c.input);
        diff.pinned(what + ": pitch damping", c.damping[0], damping[0], kComponentTolerance);
        diff.pinned(what + ": yaw damping", c.damping[1], damping[1], kComponentTolerance);
    }
    return diff.text();
}

TEST(BarrowmanStabilityCalculator, DampingMomentsOfTheAlphaAreOpenRockets)
{
    static const std::array<DampingCase, 10> kCases{{
        {.input = {0.0, 0.0, 0.15, 1.0, 1.0}, .damping = {0.0, 0.0}},
        {.input   = {0.5, -0.3, 0.15, 5.0, 5.0},
         .damping = {5.1158283860786216E-5, -1.841698218988304E-5}},
        {.input = {0.5, -0.3, 0.15, 1.0E-7, 2.0E-7}, .damping = {1.0E-7, -2.0E-7}},
        {.input   = {-2.0, 4.0, 0.2, 5.0, 5.0},
         .damping = {-0.0010664110272034708, 0.004265644108813883}},
        {.input   = {0.5, 0.3, 0.15, kNaN, kNaN},
         .damping = {5.1158283860786216E-5, 1.841698218988304E-5}},
        {.input = {0.5, 0.3, 0.15, -1.0, -2.0}, .damping = {-1.0, -2.0}},
        {.input   = {30.0, 40.0, 0.0, 1.0E9, 1.0E9},
         .damping = {2.062588260492159, 3.666823574208283}},
        {.input = {kNaN, 0.3, 0.15, 5.0, 5.0}, .damping = {5.0, 1.841698218988304E-5}},
        {.input = {0.5, 0.3, kNaN, 5.0, 5.0}, .damping = {5.0, 5.0}},
        {.input   = {0.5, 0.3, 0.6, 5.0, 5.0},
         .damping = {0.0065653499128317154, 0.0023635259686194177}},
    }};
    const TestEstesAlphaIII                  alpha;
    EXPECT_EQ(dampingDifferences(alpha.rocket->getSelectedConfiguration(), 0.3, kCases), "");
}

TEST(BarrowmanStabilityCalculator, DampingCountsAtMostFourFins)
{
    // six fins: 0.6 * min(6, 4) * the planform area of one fin
    static const std::array<DampingCase, 10> kCases{{
        {.input = {0.0, 0.0, 0.15, 1.0, 1.0}, .damping = {0.0, 0.0}},
        {.input   = {0.5, -0.3, 0.15, 5.0, 5.0},
         .damping = {5.8866537394463044E-5, -2.1191953462006695E-5}},
        {.input = {0.5, -0.3, 0.15, 1.0E-7, 2.0E-7}, .damping = {1.0E-7, -2.0E-7}},
        {.input   = {-2.0, 4.0, 0.2, 5.0, 5.0},
         .damping = {-0.0010816266302067234, 0.004326506520826894}},
        {.input   = {0.5, 0.3, 0.15, kNaN, kNaN},
         .damping = {5.8866537394463044E-5, 2.1191953462006695E-5}},
        {.input = {0.5, 0.3, 0.15, -1.0, -2.0}, .damping = {-1.0, -2.0}},
        {.input   = {30.0, 40.0, 0.0, 1.0E9, 1.0E9},
         .damping = {2.499591533858585, 4.443718282415262}},
        {.input = {kNaN, 0.3, 0.15, 5.0, 5.0}, .damping = {5.0, 2.1191953462006695E-5}},
        {.input = {0.5, 0.3, kNaN, 5.0, 5.0}, .damping = {5.0, 5.0}},
        {.input   = {0.5, 0.3, 0.6, 5.0, 5.0},
         .damping = {0.006901434187367369, 0.002484516307452253}},
    }};
    const TestEstesAlphaIII                  alpha;
    alpha.fins->setFinCount(6);
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    EXPECT_EQ(dampingDifferences(config, 0.3, kCases), "");

    // ... so that eight fins damp as much as six, and four as well.
    const std::array<double, 5>  input{0.5, -0.3, 0.15, 5.0, 5.0};
    BarrowmanStabilityCalculator sixFins;
    const std::array<double, 2>  six = dampingOf(sixFins, config, 0.3, input);
    alpha.fins->setFinCount(8);
    BarrowmanStabilityCalculator eightFins;
    EXPECT_EQ(dampingOf(eightFins, config, 0.3, input), six);
    alpha.fins->setFinCount(4);
    BarrowmanStabilityCalculator fourFins;
    EXPECT_EQ(dampingOf(fourFins, config, 0.3, input), six);
}

TEST(BarrowmanStabilityCalculator, DampingMomentsOfTheFalconAreOpenRockets)
{
    static const std::array<DampingCase, 10> kCases{{
        {.input = {0.0, 0.0, 0.15, 1.0, 1.0}, .damping = {0.0, 0.0}},
        {.input   = {0.5, -0.3, 0.15, 5.0, 5.0},
         .damping = {0.002299228360873807, -8.277222099145706E-4}},
        {.input = {0.5, -0.3, 0.15, 1.0E-7, 2.0E-7}, .damping = {1.0E-7, -2.0E-7}},
        {.input   = {-2.0, 4.0, 0.2, 5.0, 5.0},
         .damping = {-0.03323875441918084, 0.13295501767672335}},
        {.input   = {0.5, 0.3, 0.15, kNaN, kNaN},
         .damping = {0.002299228360873807, 8.277222099145706E-4}},
        {.input = {0.5, 0.3, 0.15, -1.0, -2.0}, .damping = {-1.0, -2.0}},
        {.input   = {30.0, 40.0, 0.0, 1.0E9, 1.0E9},
         .damping = {11.057642495607372, 19.658031103301994}},
        {.input = {kNaN, 0.3, 0.15, 5.0, 5.0}, .damping = {5.0, 8.277222099145706E-4}},
        {.input = {0.5, 0.3, kNaN, 5.0, 5.0}, .damping = {5.0, 5.0}},
        {.input   = {0.5, 0.3, 0.6, 5.0, 5.0},
         .damping = {8.418550047264901E-4, 3.0306780170153645E-4}},
    }};
    const TestFalcon9Heavy                   falcon;
    EXPECT_EQ(dampingDifferences(falcon.rocket->getSelectedConfiguration(), 1.2, kCases), "");
}

TEST(BarrowmanStabilityCalculator, DampingMomentsOfFinsOnPodsAreOpenRockets)
{
    static const std::array<DampingCase, 10> kCases{{
        {.input = {0.0, 0.0, 0.15, 1.0, 1.0}, .damping = {0.0, 0.0}},
        {.input   = {0.5, -0.3, 0.15, 5.0, 5.0},
         .damping = {5.21722268973166E-5, -1.8782001683033976E-5}},
        {.input = {0.5, -0.3, 0.15, 1.0E-7, 2.0E-7}, .damping = {1.0E-7, -2.0E-7}},
        {.input   = {-2.0, 4.0, 0.2, 5.0, 5.0},
         .damping = {-9.737223869372709E-4, 0.0038948895477490834}},
        {.input   = {0.5, 0.3, 0.15, kNaN, kNaN},
         .damping = {5.21722268973166E-5, 1.8782001683033976E-5}},
        {.input = {0.5, 0.3, 0.15, -1.0, -2.0}, .damping = {-1.0, -2.0}},
        {.input   = {30.0, 40.0, 0.0, 1.0E9, 1.0E9},
         .damping = {1.6882211941505492, 3.00128212293431}},
        {.input = {kNaN, 0.3, 0.15, 5.0, 5.0}, .damping = {5.0, 1.8782001683033976E-5}},
        {.input = {0.5, 0.3, kNaN, 5.0, 5.0}, .damping = {5.0, 5.0}},
        {.input   = {0.5, 0.3, 0.6, 5.0, 5.0},
         .damping = {0.004835533701313738, 0.001740792132472946}},
    }};
    const TestEstesAlphaIIIWithPods          pods;
    EXPECT_EQ(dampingDifferences(pods.rocket->getSelectedConfiguration(), 0.3, kCases), "");
}

TEST(BarrowmanStabilityCalculator, AnEmptyRocketIsNotDamped)
{
    // No symmetric component: the cached diameter stays 0 and the multiplier is 0 (-0.0 with a
    // negative rate), which a negative total still caps.
    static const std::array<DampingCase, 10> kCases{{
        {.input = {0.0, 0.0, 0.15, 1.0, 1.0}, .damping = {0.0, 0.0}},
        {.input = {0.5, -0.3, 0.15, 5.0, 5.0}, .damping = {0.0, -0.0}},
        {.input = {0.5, -0.3, 0.15, 1.0E-7, 2.0E-7}, .damping = {0.0, -0.0}},
        {.input = {-2.0, 4.0, 0.2, 5.0, 5.0}, .damping = {-0.0, 0.0}},
        {.input = {0.5, 0.3, 0.15, kNaN, kNaN}, .damping = {0.0, 0.0}},
        {.input = {0.5, 0.3, 0.15, -1.0, -2.0}, .damping = {-1.0, -2.0}},
        {.input = {30.0, 40.0, 0.0, 1.0E9, 1.0E9}, .damping = {0.0, 0.0}},
        {.input = {kNaN, 0.3, 0.15, 5.0, 5.0}, .damping = {5.0, 0.0}},
        {.input = {0.5, 0.3, kNaN, 5.0, 5.0}, .damping = {5.0, 5.0}},
        {.input = {0.5, 0.3, 0.6, 5.0, 5.0}, .damping = {0.0, 0.0}},
    }};
    Rocket                                   rocket;
    rocket.enableEvents();
    EXPECT_EQ(dampingDifferences(rocket.getSelectedConfiguration(), 0.3, kCases), "");
}

TEST(BarrowmanStabilityCalculator, DampingKeepsTheLengthAndDiameterOfItsFirstCall)
{
    // The summed length and planform area of the active symmetric components are cached at the
    // first call, and only voidAerodynamicCache() drops them: when the active stages change (as
    // OpenRocket's do at a stage separation, without a change of the rocket) the body term is
    // still that of the whole rocket, while the fin term follows the active fin sets.
    const std::array<double, 5> input{0.5, -0.3, 0.4, 500.0, 500.0};
    const TestFalcon9Heavy      falcon;
    FlightConfiguration&        config = falcon.rocket->getSelectedConfiguration();

    BarrowmanStabilityCalculator calculator;
    JavaValueDifferences         diff;
    const auto compare = [&](std::string_view what, const std::array<double, 2>& expected,
                             BarrowmanStabilityCalculator& with) {
        const std::array<double, 2> damping = dampingOf(with, config, 0.3, input);
        diff.pinned(std::format("{}: pitch", what), expected[0], damping[0], kComponentTolerance);
        diff.pinned(std::format("{}: yaw", what), expected[1], damping[1], kComponentTolerance);
    };

    compare("all stages", {0.021576638552940404, -0.007767589879058546}, calculator);

    config.setStageActive(TestFalcon9Heavy::kCoreStageNumber, false);
    compare("payload only, cached", {0.01971492688227166, -0.0070973736776177975}, calculator);

    calculator.voidAerodynamicCache();
    compare("payload only, voided", {4.7502739565886695E-5, -1.710098624371921E-5}, calculator);
    BarrowmanStabilityCalculator fresh;
    compare("payload only, a new calculator", {4.7502739565886695E-5, -1.710098624371921E-5},
            fresh);

    // ... and the other way round: the payload stage's length and diameter for all the stages.
    config.setAllStages();
    compare("all stages after payload only", {0.0019092144102346319, -6.873171876844675E-4},
            calculator);
    EXPECT_EQ(diff.text(), "");
}

TEST(BarrowmanStabilityCalculator, DampingLeavesTheMomentsOfTheTotalAlone)
{
    const TestEstesAlphaIII      alpha;
    const FlightConfiguration&   config = alpha.rocket->getSelectedConfiguration();
    FlightConditions             conditions{config};
    BarrowmanStabilityCalculator calculator;
    conditions.setPitchRate(0.5);
    conditions.setYawRate(-0.3);
    AerodynamicForces total;
    total.setCm(5.0);
    total.setCyaw(kNaN);
    calculator.calculateDampingMoments(config, conditions, total);
    EXPECT_EQ(total.getCm(), 5.0);
    EXPECT_TRUE(std::isnan(total.getCyaw()));
    EXPECT_GT(total.getPitchDampingMoment(), 0);
    // A NaN total does not cap the damping moment (MathUtil::min keeps its first argument).
    EXPECT_LT(total.getYawDampingMoment(), 0);
}

// NOLINTEND(modernize-use-std-numbers)

// ================================================================ the geometry checks
//
// What OpenRocket's checkGeometry() warns of in each case (StabilityProbe.geometry()), in
// UnitGroup's default units (centimetres), the metric defaults (centimetres) and the imperial
// ones (inches): diameters and positions are compared as the default length unit prints them.

enum class Units
{
    DEFAULT,
    METRIC,
    IMPERIAL,
};

constexpr std::array<Units, 3> kAllUnits{Units::DEFAULT, Units::METRIC, Units::IMPERIAL};

void apply(Units units)
{
    switch (units)
    {
        case Units::DEFAULT:
            UnitGroup::resetDefaultUnits();
            break;
        case Units::METRIC:
            UnitGroup::setDefaultMetricUnits();
            break;
        case Units::IMPERIAL:
            UnitGroup::setDefaultImperialUnits();
            break;
    }
}

/// A warning OpenRocket gives, with the components it names, in order.
struct ExpectedWarning
{
    ExpectedWarning(const Warning* expected, std::vector<const RocketComponent*> expectedSources)
      : warning(expected), sources(std::move(expectedSources))
    {
    }

    const Warning*                      warning;
    std::vector<const RocketComponent*> sources;
};

/// "<text> <- 'name' 'name'" of a warning, as the probe prints it.
[[nodiscard]] std::string describe(const Warning& warning)
{
    std::string text = warning.messageDescription() + " <-";
    for (const QtRocket::MessageSource& source : warning.sources())
    {
        text += " '" + source.name + "'";
    }
    return text;
}

/// The differences between the warnings of checkGeometry(@p configuration, @p component) and
/// @p expected: their number, order, texts and sources (ids and names).
[[nodiscard]] std::string geometryDifferences(const FlightConfiguration&          configuration,
                                              const RocketComponent&              component,
                                              const std::vector<ExpectedWarning>& expected)
{
    WarningSet                   warnings;
    BarrowmanStabilityCalculator calculator;
    calculator.checkGeometry(configuration, component, warnings);

    std::string actualText;
    for (const Warning& warning : warnings)
    {
        actualText += describe(warning) + "; ";
    }
    std::string expectedText;
    bool        sameSources = warnings.size() == expected.size();
    auto        actual      = warnings.begin();
    for (const ExpectedWarning& e : expected)
    {
        MessageSources sources;
        for (const RocketComponent* const source : e.sources)
        {
            sources.emplace_back(source->getId(), source->getName());
        }
        expectedText += e.warning->messageDescription() + " <-";
        for (const QtRocket::MessageSource& source : sources)
        {
            expectedText += " '" + source.name + "'";
        }
        expectedText += "; ";
        if (sameSources)
        {
            sameSources = actual->sources() == sources;  // by component id
            ++actual;
        }
    }
    if (actualText != expectedText)
    {
        return std::format("expected [{}], got [{}]", expectedText, actualText);
    }
    return sameSources ? "" : "the sources are not the expected components: [" + actualText + "]";
}

/// geometryDifferences() of @p rocket and its selected configuration, in each unit system; the
/// same warnings are expected in all three.
[[nodiscard]] std::string geometryDifferencesInAllUnits(
    const Rocket& rocket, const std::vector<ExpectedWarning>& expected)
{
    std::string text;
    for (const Units units : kAllUnits)
    {
        apply(units);
        const std::string differences =
            geometryDifferences(rocket.getSelectedConfiguration(), rocket, expected);
        if (!differences.empty())
        {
            text += std::format("units {}: {}\n", static_cast<int>(units), differences);
        }
    }
    return text;
}

TEST(BarrowmanStabilityCalculator, ContinuousRocketsHaveNoGeometryWarnings)
{
    const DefaultUnitsGuard guard;
    EXPECT_EQ(geometryDifferencesInAllUnits(*TestEstesAlphaIII{}.rocket, {}), "");
    EXPECT_EQ(geometryDifferencesInAllUnits(*TestFalcon9Heavy{}.rocket, {}), "");
    // Phantom pod tubes of radius 0 on a body are not in line with it.
    EXPECT_EQ(geometryDifferencesInAllUnits(*TestEstesAlphaIIIWithPods{}.rocket, {}), "");
    EXPECT_EQ(geometryDifferencesInAllUnits(*TestEndPlateRocket{}.rocket, {}), "");
    const TestMultiStageEventTestRocket multi;
    multi.rocket->getSelectedConfiguration().setAllStages();
    EXPECT_EQ(geometryDifferencesInAllUnits(*multi.rocket, {}), "");
}

TEST(BarrowmanStabilityCalculator, RocketsWithoutBodyHaveNoGeometryWarnings)
{
    const DefaultUnitsGuard guard;
    Rocket                  empty;
    empty.enableEvents();
    EXPECT_EQ(geometryDifferencesInAllUnits(empty, {}), "");

    Rocket emptyStage;
    emptyStage.addChild(std::make_unique<AxialStage>());
    emptyStage.enableEvents();
    EXPECT_EQ(geometryDifferencesInAllUnits(emptyStage, {}), "");
}

TEST(BarrowmanStabilityCalculator, DiameterDiscontinuityNamesBothComponents)
{
    const DefaultUnitsGuard guard;
    const TestEstesAlphaIII alpha;
    alpha.nose->setAftRadius(0.015);
    alpha.body->setOuterRadius(0.012);
    EXPECT_EQ(geometryDifferencesInAllUnits(
                  *alpha.rocket, {{&Warning::kDiameterDiscontinuity, {alpha.nose, alpha.body}}}),
              "");
}

TEST(BarrowmanStabilityCalculator, DiameterDiscontinuityDependsOnTheDefaultLengthUnit)
{
    const DefaultUnitsGuard guard;
    {
        // 2.4 cm and 2.404 cm print alike ("2.4 cm"); 0.945 in and 0.946 in do not.
        const TestEstesAlphaIII alpha;
        alpha.body->setOuterRadius(0.01202);
        const FlightConfiguration&         config = alpha.rocket->getSelectedConfiguration();
        const std::vector<ExpectedWarning> discontinuity{
            {&Warning::kDiameterDiscontinuity, {alpha.nose, alpha.body}}};
        apply(Units::DEFAULT);
        EXPECT_EQ(geometryDifferences(config, *alpha.rocket, {}), "");
        apply(Units::METRIC);
        EXPECT_EQ(geometryDifferences(config, *alpha.rocket, {}), "");
        apply(Units::IMPERIAL);
        EXPECT_EQ(geometryDifferences(config, *alpha.rocket, discontinuity), "");
    }
    {
        // 2.5449 cm and 2.5451 cm print as "2.54 cm" and "2.55 cm"; both are "1 in".
        const TestEstesAlphaIII alpha;
        alpha.nose->setAftRadius(0.0127245);
        alpha.body->setOuterRadius(0.0127255);
        const FlightConfiguration&         config = alpha.rocket->getSelectedConfiguration();
        const std::vector<ExpectedWarning> discontinuity{
            {&Warning::kDiameterDiscontinuity, {alpha.nose, alpha.body}}};
        apply(Units::DEFAULT);
        EXPECT_EQ(geometryDifferences(config, *alpha.rocket, discontinuity), "");
        apply(Units::METRIC);
        EXPECT_EQ(geometryDifferences(config, *alpha.rocket, discontinuity), "");
        apply(Units::IMPERIAL);
        EXPECT_EQ(geometryDifferences(config, *alpha.rocket, {}), "");
    }
}

TEST(BarrowmanStabilityCalculator, ZeroVolumeBodies)
{
    const DefaultUnitsGuard guard;
    {
        // a length of 0
        const TestEstesAlphaIII alpha;
        auto                    tube = std::make_unique<BodyTube>(0, 0.012, 0.0003);
        tube->setName("Zero Length");
        const BodyTube& added = alpha.stage->addChild(std::move(tube));
        EXPECT_EQ(
            geometryDifferencesInAllUnits(*alpha.rocket, {{&Warning::kZeroVolumeBody, {&added}}}),
            "");
    }
    {
        // both radii 0: a diameter discontinuity too
        const TestEstesAlphaIII alpha;
        auto                    tube = std::make_unique<BodyTube>(0.05, 0, 0);
        tube->setName("Phantom");
        const BodyTube& added = alpha.stage->addChild(std::move(tube));
        EXPECT_EQ(geometryDifferencesInAllUnits(
                      *alpha.rocket, {{&Warning::kDiameterDiscontinuity, {alpha.body, &added}},
                                      {&Warning::kZeroVolumeBody, {&added}}}),
                  "");
    }
    {
        // one radius 0 only (a tail cone): no warning
        const TestEstesAlphaIII alpha;
        auto                    tail = std::make_unique<NoseCone>();
        tail->setName("Tail Cone");
        tail->setLength(0.03);
        tail->setAftRadius(0.012);
        tail->setFlipped(true);
        alpha.stage->addChild(std::move(tail));
        EXPECT_EQ(geometryDifferencesInAllUnits(*alpha.rocket, {}), "");
    }
    {
        // the rocket with the steps: a disk and a transition of length 0, and the steps
        // between the disk and its neighbours (the transition's radii match its neighbours')
        const TestStepsRocket steps;
        EXPECT_EQ(geometryDifferencesInAllUnits(
                      *steps.rocket, {{&Warning::kDiameterDiscontinuity, {steps.tubeA, steps.disk}},
                                      {&Warning::kZeroVolumeBody, {steps.disk}},
                                      {&Warning::kDiameterDiscontinuity, {steps.disk, steps.tubeB}},
                                      {&Warning::kZeroVolumeBody, {steps.flat}}}),
                  "");
    }
}

TEST(BarrowmanStabilityCalculator, OpenForwardAirframe)
{
    const DefaultUnitsGuard guard;
    {
        // The first component is a body tube with a wall: stage 0 is active.
        const TestSimple2Stage twoStage;
        EXPECT_EQ(geometryDifferencesInAllUnits(*twoStage.rocket, {{&Warning::kOpenAirframeForward,
                                                                    {twoStage.sustainerBody}}}),
                  "");
    }
    {
        // The booster alone: neither the sustainer nor a recovery device, so no warning ...
        const TestSimple2Stage twoStage;
        twoStage.rocket->getSelectedConfiguration().setStageActive(0, false);
        EXPECT_EQ(geometryDifferencesInAllUnits(*twoStage.rocket, {}), "");
    }
    {
        // ... unless the bottom stage has a recovery device.
        const TestSimple2Stage twoStage;
        twoStage.boosterBody->addChild(std::make_unique<Parachute>());
        twoStage.rocket->getSelectedConfiguration().setStageActive(0, false);
        EXPECT_EQ(geometryDifferencesInAllUnits(
                      *twoStage.rocket, {{&Warning::kOpenAirframeForward, {twoStage.boosterBody}}}),
                  "");
    }
    {
        // The sustainer alone (the parachute is in the booster, which is not active).
        const TestSimple2Stage twoStage;
        twoStage.boosterBody->addChild(std::make_unique<Parachute>());
        twoStage.rocket->getSelectedConfiguration().setStageActive(1, false);
        EXPECT_EQ(geometryDifferencesInAllUnits(*twoStage.rocket, {{&Warning::kOpenAirframeForward,
                                                                    {twoStage.sustainerBody}}}),
                  "");
    }
    {
        // A filled tube is not open.
        const TestSimple2Stage twoStage;
        twoStage.sustainerBody->setFilled(true);
        EXPECT_EQ(geometryDifferencesInAllUnits(*twoStage.rocket, {}), "");
    }
    {
        // The following component is checked against the open one all the same.
        const TestSimple2Stage twoStage;
        twoStage.boosterBody->setOuterRadius(0.02);
        EXPECT_EQ(geometryDifferencesInAllUnits(
                      *twoStage.rocket, {{&Warning::kOpenAirframeForward, {twoStage.sustainerBody}},
                                         {&Warning::kDiameterDiscontinuity,
                                          {twoStage.sustainerBody, twoStage.boosterBody}}}),
                  "");
    }
}

TEST(BarrowmanStabilityCalculator, OpenForwardAirframeWithoutAnActiveStage)
{
    const DefaultUnitsGuard guard;
    const TestSimple2Stage  twoStage;
    FlightConfiguration&    config = twoStage.rocket->getSelectedConfiguration();
    config.clearAllStages();

    // From the rocket nothing is reached: an inactive stage is left out.
    EXPECT_EQ(geometryDifferencesInAllUnits(*twoStage.rocket, {}), "");

    // From the stage itself its body tube is, and OpenRocket then asks the bottom active stage
    // for its recovery device: a NullPointerException in Java.
    WarningSet                   warnings;
    BarrowmanStabilityCalculator calculator;
    EXPECT_THROW(calculator.checkGeometry(config, *twoStage.sustainerStage, warnings), BugError);
}

TEST(BarrowmanStabilityCalculator, AnAssemblyOnItsOwnStartsFromTheComponentAheadOfIt)
{
    const DefaultUnitsGuard guard;
    {
        // The booster stage alone: its body tube follows the sustainer's, so it is not an open
        // forward end ...
        const TestSimple2Stage     twoStage;
        const FlightConfiguration& config = twoStage.rocket->getSelectedConfiguration();
        EXPECT_EQ(geometryDifferences(config, *twoStage.boosterStage, {}), "");

        // ... and a wider one is a discontinuity against the sustainer's.
        twoStage.boosterBody->setOuterRadius(0.02);
        EXPECT_EQ(geometryDifferences(config, *twoStage.boosterStage,
                                      {{&Warning::kDiameterDiscontinuity,
                                        {twoStage.sustainerBody, twoStage.boosterBody}}}),
                  "");
    }
    {
        // A component that is not an assembly: only what is under it is checked (the Alpha
        // III's body tube holds no symmetric component).
        const TestEstesAlphaIII alpha;
        alpha.nose->setAftRadius(0.015);
        EXPECT_EQ(geometryDifferences(alpha.rocket->getSelectedConfiguration(), *alpha.body, {}),
                  "");
    }
    {
        // An assembly without children has no first child to start from, and nothing to check.
        const TestEstesAlphaIII alpha;
        const AxialStage&       empty = alpha.rocket->addChild(std::make_unique<AxialStage>());
        EXPECT_EQ(geometryDifferences(alpha.rocket->getSelectedConfiguration(), empty, {}), "");
    }
}

/// The Estes Alpha III with an inline pod, the pod moved by @p move along the axis.
struct MovedInlinePod : TestEstesAlphaIIIWithInlinePod
{
    explicit MovedInlinePod(double move) { pod->setAxialOffset(pod->getAxialOffset() + move); }
};

/// The differences, in every unit system, between the warnings of the inline pod moved by
/// @p move and an overlap of the front tube and the pod's first tube.
[[nodiscard]] std::string inlinePodOverlapDifferences(double move)
{
    const MovedInlinePod moved{move};
    return geometryDifferencesInAllUnits(
        *moved.rocket, {{&Warning::kAirframeOverlap, {moved.frontTube, moved.middleTube}}});
}

/// The differences, in every unit system, between the warnings of the inline pod moved by
/// @p move and @p warning for the pod set.
[[nodiscard]] std::string inlinePodSetDifferences(double move, const Warning& warning)
{
    const MovedInlinePod moved{move};
    return geometryDifferencesInAllUnits(*moved.rocket, {{&warning, {moved.pod}}});
}

TEST(BarrowmanStabilityCalculator, GapsAndOverlapsOfAnInlinePod)
{
    const DefaultUnitsGuard guard;
    {
        const MovedInlinePod untouched{0};
        EXPECT_EQ(geometryDifferencesInAllUnits(*untouched.rocket, {}), "");
    }
    {
        // moved back: a gap between the front tube and the pod's first tube
        const MovedInlinePod moved{0.1};
        EXPECT_EQ(
            geometryDifferencesInAllUnits(
                *moved.rocket, {{&Warning::kAirframeGap, {moved.frontTube, moved.middleTube}}}),
            "");
    }
    // moved forward by less than the middle tube's length: it starts inside the front tube and
    // ends behind it
    EXPECT_EQ(inlinePodOverlapDifferences(-0.01), "");
    EXPECT_EQ(inlinePodOverlapDifferences(-0.04), "");
}

TEST(BarrowmanStabilityCalculator, AnInlinePodFarForwardOverlapsOrLeadsItsParent)
{
    const DefaultUnitsGuard guard;
    // The middle tube ends inside the front tube (and another tube follows it), or starts ahead
    // of it: the pod set overlaps its parent ...
    EXPECT_EQ(inlinePodSetDifferences(-0.06, Warning::kPodsetOverlap), "");
    EXPECT_EQ(inlinePodSetDifferences(-0.2, Warning::kPodsetOverlap), "");
    EXPECT_EQ(inlinePodSetDifferences(-0.26, Warning::kPodsetOverlap), "");
    // ... until its last tube ends at or ahead of the tip of the nose cone.
    EXPECT_EQ(inlinePodSetDifferences(-0.27, Warning::kPodsetForward), "");
    EXPECT_EQ(inlinePodSetDifferences(-0.5, Warning::kPodsetForward), "");
}

TEST(BarrowmanStabilityCalculator, GapsAndOverlapsDependOnTheDefaultLengthUnit)
{
    const DefaultUnitsGuard guard;
    {
        // A gap of 0.1 mm at 17 cm: "17 cm" on both sides, but "6.69 in" and "6.7 in".
        const MovedInlinePod       moved{0.0001};
        const FlightConfiguration& config = moved.rocket->getSelectedConfiguration();
        apply(Units::DEFAULT);
        EXPECT_EQ(geometryDifferences(config, *moved.rocket, {}), "");
        apply(Units::METRIC);
        EXPECT_EQ(geometryDifferences(config, *moved.rocket, {}), "");
        apply(Units::IMPERIAL);
        EXPECT_EQ(
            geometryDifferences(config, *moved.rocket,
                                {{&Warning::kAirframeGap, {moved.frontTube, moved.middleTube}}}),
            "");
    }
    {
        // An overlap of 0.1 mm.
        const MovedInlinePod       moved{-0.0001};
        const FlightConfiguration& config = moved.rocket->getSelectedConfiguration();
        apply(Units::DEFAULT);
        EXPECT_EQ(geometryDifferences(config, *moved.rocket, {}), "");
        apply(Units::METRIC);
        EXPECT_EQ(geometryDifferences(config, *moved.rocket, {}), "");
        apply(Units::IMPERIAL);
        EXPECT_EQ(geometryDifferences(
                      config, *moved.rocket,
                      {{&Warning::kAirframeOverlap, {moved.frontTube, moved.middleTube}}}),
                  "");
    }
}

TEST(BarrowmanStabilityCalculator, APodFlushWithTheEndOfTheComponentAheadOfItsParentOverlapsIt)
{
    const DefaultUnitsGuard guard;
    {
        // The pod on Tube B starts where Tube A ends, which is where Tube B starts.
        const TestFlushPodRocket flush;
        EXPECT_EQ(
            geometryDifferencesInAllUnits(*flush.rocket, {{&Warning::kPodsetOverlap, {flush.pod}}}),
            "");
        // Within MathUtil::equals() of it counts as well.
        flush.pod->setAxialOffset(1e-9);
        EXPECT_EQ(
            geometryDifferencesInAllUnits(*flush.rocket, {{&Warning::kPodsetOverlap, {flush.pod}}}),
            "");
    }
    for (const bool withTail : {false, true})
    {
        // The same for a pod at the front of the first tube, which follows the nose cone.
        const TestShortPodRocket atFront{0.0, withTail};
        EXPECT_EQ(geometryDifferencesInAllUnits(*atFront.rocket,
                                                {{&Warning::kPodsetOverlap, {atFront.pod}}}),
                  "")
            << withTail;
    }
}

TEST(BarrowmanStabilityCalculator, AShortTubeInsideItsParentOverlapsIt)
{
    const DefaultUnitsGuard guard;
    for (const bool withTail : {false, true})
    {
        // Inside the front tube and ending inside it: an overlap, since no symmetric component
        // follows the short tube in line (not even the tail tube behind the front tube).
        const TestShortPodRocket inside{0.05, withTail};
        EXPECT_EQ(inside.shortTube->getNextSymmetricComponent(), nullptr);
        EXPECT_EQ(
            geometryDifferencesInAllUnits(
                *inside.rocket, {{&Warning::kAirframeOverlap, {inside.front, inside.shortTube}}}),
            "")
            << withTail;

        // Ending behind the front tube: an overlap as well.
        const TestShortPodRocket across{0.09, withTail};
        EXPECT_EQ(
            geometryDifferencesInAllUnits(
                *across.rocket, {{&Warning::kAirframeOverlap, {across.front, across.shortTube}}}),
            "")
            << withTail;
    }
}

TEST(BarrowmanStabilityCalculator, BoosterSetsAreCheckedOnTheirOwn)
{
    const DefaultUnitsGuard guard;
    const TestFalcon9Heavy  falcon;
    falcon.boosterNose->setAftRadius(0.015);
    falcon.boosterBody->setOuterRadius(0.012);
    const std::vector<ExpectedWarning> discontinuity{
        {&Warning::kDiameterDiscontinuity, {falcon.boosterNose, falcon.boosterBody}}};
    EXPECT_EQ(geometryDifferencesInAllUnits(*falcon.rocket, discontinuity), "");

    // An active booster set under an inactive core is still checked ...
    FlightConfiguration& config = falcon.rocket->getSelectedConfiguration();
    config.setStageActive(TestFalcon9Heavy::kCoreStageNumber, false);
    config.setStageActive(TestFalcon9Heavy::kBoosterStageNumber, true);
    EXPECT_EQ(geometryDifferencesInAllUnits(*falcon.rocket, discontinuity), "");

    // ... an inactive one is not.
    config.setAllStages();
    config.setStageActive(TestFalcon9Heavy::kBoosterStageNumber, false);
    EXPECT_EQ(geometryDifferencesInAllUnits(*falcon.rocket, {}), "");
}

TEST(BarrowmanStabilityCalculator, CheckGeometryUsesNoCalculation)
{
    // The geometry checks read the components themselves: a change shows without voiding.
    const DefaultUnitsGuard      guard;
    const TestEstesAlphaIII      alpha;
    const FlightConfiguration&   config = alpha.rocket->getSelectedConfiguration();
    BarrowmanStabilityCalculator calculator;
    WarningSet                   warnings;
    calculator.checkGeometry(config, *alpha.rocket, warnings);
    EXPECT_TRUE(warnings.empty());

    alpha.nose->setAftRadius(0.015);
    calculator.checkGeometry(config, *alpha.rocket, warnings);
    EXPECT_EQ(warnings.size(), 1U);
    // The same warning again is not added twice.
    calculator.checkGeometry(config, *alpha.rocket, warnings);
    EXPECT_EQ(warnings.size(), 1U);
}

}  // namespace
