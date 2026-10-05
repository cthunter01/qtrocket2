#include "QtRocket/aero/BarrowmanDragCalculator.h"

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
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/BarrowmanStabilityCalculator.h"
#include "QtRocket/aero/DragCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/aero/StabilityForceBreakdown.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"
#include "aero/BarrowmanTestRockets.h"
#include "aero/ForcePins.h"
#include "rocket/JavaValueDifferences.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AerodynamicForces;
using QtRocket::BarrowmanDragCalculator;
using QtRocket::BarrowmanStabilityCalculator;
using QtRocket::BugError;
using QtRocket::DragCalculator;
using QtRocket::ExternalComponent;
using QtRocket::Finish;
using QtRocket::FlightConditions;
using QtRocket::FlightConfiguration;
using QtRocket::ForceMap;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::StabilityForceBreakdown;
using QtRocket::TrapezoidFinSet;
using QtRocket::WarningSet;
using QtRocket::Test::allComponents;
using QtRocket::Test::compareDrag;
using QtRocket::Test::DragPin;
using QtRocket::Test::JavaValueDifferences;
using QtRocket::Test::kComponentTolerance;
using QtRocket::Test::kRocketTolerance;
using QtRocket::Test::TestEndPlateRocket;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::TestFalcon9Heavy;
using QtRocket::Test::TestMultiStageEventTestRocket;
using QtRocket::Test::TestStepsRocket;
using QtRocket::Test::TestTubeFinsRocket;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kPi  = std::numbers::pi;

using Areas = std::vector<FlightConditions::NozzleExitArea>;

// ====================================================== the stagnation and base pressure CD

struct PressureCase
{
    double mach;
    double stagnationCD;
    double baseCD;
};

TEST(BarrowmanDragCalculator, StagnationAndBaseCDAreOpenRockets)
{
    // BarrowmanDragCalculator.calculateStagnationCD() and calculateBaseCD() on JDK 17 (the probe
    // EdgeProbe.java); additions, multiplications and divisions only, so the same doubles. Mach
    // 1 belongs to the subsonic branch of both; the first double above it to the supersonic one.
    const std::array<PressureCase, 10> cases{{
        {.mach = 0.0, .stagnationCD = 0.85, .baseCD = 0.12},
        {.mach = 0.5, .stagnationCD = 0.9044531249999999, .baseCD = 0.1525},
        {.mach = 0.9, .stagnationCD = 1.0360671250000002, .baseCD = 0.2253},
        {.mach = 1.0, .stagnationCD = 1.08375, .baseCD = 0.25},
        {.mach         = std::nextafter(1.0, 2.0),
         .stagnationCD = 1.0888500000000003,
         .baseCD       = 0.24999999999999994},
        {.mach = 1.0000001, .stagnationCD = 1.088850054910001, .baseCD = 0.24999997500000248},
        {.mach = 1.5, .stagnationCD = 1.3073722908093282, .baseCD = 0.16666666666666666},
        {.mach = 2.0, .stagnationCD = 1.41178359375, .baseCD = 0.125},
        {.mach = 5.0, .stagnationCD = 1.538387664, .baseCD = 0.05},
        // A negative Mach number is not rejected: the formulas are even in it.
        {.mach = -0.5, .stagnationCD = 0.9044531249999999, .baseCD = 0.1525},
    }};
    for (const PressureCase& c : cases)
    {
        EXPECT_EQ(BarrowmanDragCalculator::calculateStagnationCD(c.mach), c.stagnationCD)
            << "Mach " << c.mach;
        EXPECT_EQ(BarrowmanDragCalculator::calculateBaseCD(c.mach), c.baseCD) << "Mach " << c.mach;
    }
}

TEST(BarrowmanDragCalculator, StagnationAndBaseCDOfNaNAndInfinity)
{
    // A NaN fails `m <= 1` and goes through the supersonic formulas.
    EXPECT_TRUE(std::isnan(BarrowmanDragCalculator::calculateStagnationCD(kNaN)));
    EXPECT_TRUE(std::isnan(BarrowmanDragCalculator::calculateBaseCD(kNaN)));
    // The supersonic limits: 0.85 * 1.84 and 0.
    EXPECT_EQ(BarrowmanDragCalculator::calculateStagnationCD(kInf), 1.564);
    EXPECT_EQ(BarrowmanDragCalculator::calculateBaseCD(kInf), 0.0);
}

TEST(BarrowmanDragCalculator, StagnationCDStepsAtMach1AsOpenRockets)
{
    // The two branches do not meet: 0.85 * 1.275 at Mach 1 and 0.85 * 1.281 just above it.
    EXPECT_GT(BarrowmanDragCalculator::calculateStagnationCD(std::nextafter(1.0, 2.0)),
              BarrowmanDragCalculator::calculateStagnationCD(1.0) + 0.005);
    // The base drag does meet (0.12 + 0.13 = 0.25 / 1).
    EXPECT_NEAR(BarrowmanDragCalculator::calculateBaseCD(std::nextafter(1.0, 2.0)),
                BarrowmanDragCalculator::calculateBaseCD(1.0), 1e-15);
}

// ============================================ ported from BarrowmanDragCalculatorTest.java

/// BarrowmanDragCalculatorTest.EPSILON
constexpr double kEpsilon = 0.000001;

// Java: testBodyFrictionCorrectionUsesDiameter
/// Verify that body fineness is based on maximum diameter. A one-meter body with a 0.1-meter
/// diameter has a fineness ratio of 10, giving a correction of 1 + 1 / (2 * 10) = 1.05.
TEST(BarrowmanDragCalculatorTest, BodyFrictionCorrectionUsesDiameter)
{
    const double correction = BarrowmanDragCalculator::calculateBodyFrictionCorrection(1.0, 0.05);

    EXPECT_NEAR(1.05, correction, kEpsilon)
        << "Body skin-friction correction should use length divided by diameter";
}

// NOLINTBEGIN(modernize-use-std-numbers): OpenRocket's results, not approximations of constants

// ================================================================== the formulas, pinned
//
// The values below were printed by the Java probe DragProbe.java, which calls OpenRocket's
// BarrowmanDragCalculator (the private calculateFrictionCoefficient() and
// calculateRoughnessCorrection() and the package-private calculateBodyFrictionCorrection()
// through reflection). Values made of +, -, * and / alone are compared exactly; those that go
// through log() or pow() within a relative 1e-12.

struct BodyCorrectionCase
{
    double bodyLength;
    double maxRadius;
    double correction;
};

TEST(BarrowmanDragCalculator, BodyFrictionCorrectionIsOpenRockets)
{
    // A radius of 0 gives 1 (the fineness ratio is infinite), but NaN with a length of 0 too;
    // -Double.MAX_VALUE + 0.0001 is the body length of a rocket without a symmetric component.
    const std::array<BodyCorrectionCase, 12> cases{{
        {.bodyLength = 1.0, .maxRadius = 0.05, .correction = 1.05},
        {.bodyLength = 0.2701, .maxRadius = 0.012, .correction = 1.0444279896334692},
        {.bodyLength = 1.0, .maxRadius = 0.0, .correction = 1.0},
        {.bodyLength = 0.0, .maxRadius = 0.0, .correction = kNaN},
        {.bodyLength = 0.0, .maxRadius = 0.05, .correction = kInf},
        {.bodyLength = -1.7976931348623157E308, .maxRadius = 0.0, .correction = 1.0},
        {.bodyLength = 1.0E-4, .maxRadius = 0.012, .correction = 121.0},
        {.bodyLength = 3.0, .maxRadius = 0.5, .correction = 1.1666666666666667},
        {.bodyLength = kNaN, .maxRadius = 0.05, .correction = kNaN},
        {.bodyLength = 1.0, .maxRadius = kNaN, .correction = kNaN},
        {.bodyLength = kInf, .maxRadius = 0.05, .correction = 1.0},
        {.bodyLength = 1.0, .maxRadius = kInf, .correction = kInf},
    }};
    JavaValueDifferences                     diff;
    for (const BodyCorrectionCase& c : cases)
    {
        diff.pinned(
            std::format("length {} radius {}", c.bodyLength, c.maxRadius), c.correction,
            BarrowmanDragCalculator::calculateBodyFrictionCorrection(c.bodyLength, c.maxRadius),
            0.0);
    }
    EXPECT_EQ(diff.text(), "");
}

/// The Mach numbers of FrictionCase::cf: each side of the limits 0.9 and 1.1, and NaN.
constexpr std::array<double, 12> kFrictionMachs{0.0,
                                                0.3,
                                                0.8999999999999999,
                                                0.9,
                                                0.9000000000000001,
                                                1.0,
                                                1.0999999999999999,
                                                1.1,
                                                1.1000000000000003,
                                                2.0,
                                                5.0,
                                                kNaN};

struct FrictionCase
{
    bool                   perfectFinish;
    double                 reynolds;
    std::array<double, 12> cf;
};

/// The differences between calculateFrictionCoefficient() and OpenRocket's, over the Reynolds
/// number limits 1e4, 5.39e5, 1e6 and 3e6 with the double below each, and beyond.
[[nodiscard]] std::string frictionCoefficientDifferences()
{
    static const std::array<FrictionCase, 36> kCases{{
        {.perfectFinish = true,
         .reynolds      = 0.0,
         .cf = {0.0133, 0.0133, 0.0133, 0.013300000000000003, 0.013300000000000005,
                0.013300000000000005, 0.013300000000000005, 0.0133, 0.0133, 0.0133, 0.0133,
                0.0133}},
        {.perfectFinish = true,
         .reynolds      = 1.0,
         .cf = {0.0133, 0.0133, 0.0133, 0.013300000000000003, 0.013300000000000005,
                0.013300000000000005, 0.013300000000000005, 0.0133, 0.0133, 0.0133, 0.0133,
                0.0133}},
        {.perfectFinish = true,
         .reynolds      = 9999.999999999998,
         .cf = {0.0133, 0.0133, 0.0133, 0.013300000000000003, 0.013300000000000005,
                0.013300000000000005, 0.013300000000000005, 0.0133, 0.0133, 0.0133, 0.0133,
                0.0133}},
        {.perfectFinish = true,
         .reynolds      = 10000.0,
         .cf = {0.01328, 0.01328, 0.01328, 0.013280000000000004, 0.013280000000000005,
                0.013280000000000005, 0.013280000000000005, 0.01328, 0.01328, 0.01328, 0.01328,
                0.01328}},
        {.perfectFinish = true,
         .reynolds      = 100000.0,
         .cf            = {0.004199504732703608, 0.004199504732703608, 0.004199504732703608,
                           0.0041995047327036085, 0.004199504732703609, 0.004199504732703609,
                           0.004199504732703609, 0.004199504732703608, 0.004199504732703608,
                           0.004199504732703608, 0.004199504732703608, 0.004199504732703608}},
        {.perfectFinish = true,
         .reynolds      = 538999.9999999999,
         .cf            = {0.0018088547407402098, 0.0018088547407402098, 0.0018088547407402098,
                           0.0018088547407402102, 0.0018088547407402106, 0.0018088547407402106,
                           0.0018088547407402106, 0.0018088547407402098, 0.0018088547407402098,
                           0.0018088547407402098, 0.0018088547407402098, 0.0018088547407402098}},
        {.perfectFinish = true,
         .reynolds      = 539000.0,
         .cf            = {0.0018079955678169113, 0.0018079955678169113, 0.0018079955678169113,
                           0.0018079955678169117, 0.0018079955678169122, 0.0018079955678169122,
                           0.0018079955678169122, 0.0018079955678169113, 0.0018079955678169113,
                           0.0018079955678169113, 0.0018079955678169113, 0.0018079955678169113}},
        {.perfectFinish = true,
         .reynolds      = 990000.0,
         .cf            = {0.0026638469646211443, 0.0026638469646211443, 0.0026638469646211443,
                           0.0026638469646211448, 0.0026638469646211456, 0.0026638469646211456,
                           0.0026638469646211456, 0.0026638469646211443, 0.0026638469646211443,
                           0.0026638469646211443, 0.0026638469646211443, 0.0026638469646211443}},
        {.perfectFinish = true,
         .reynolds      = 1000000.0,
         .cf            = {0.0026722886699460267, 0.0026722886699460267, 0.0026722886699460267,
                           0.002672288669946027, 0.002672288669946028, 0.002672288669946028,
                           0.002672288669946028, 0.0026722886699460267, 0.0026722886699460267,
                           0.0026722886699460267, 0.0026722886699460267, 0.0026722886699460267}},
        {.perfectFinish = true,
         .reynolds      = 1000000.0000000001,
         .cf            = {0.0026722886699460267, 0.0026722886699460267, 0.0026722886699460267,
                           0.002672288669946027, 0.002672288669946028, 0.002672288669946028,
                           0.002672288669946028, 0.0026722886699460267, 0.0026722886699460267,
                           0.0026722886699460267, 0.0026722886699460267, 0.0026722886699460267}},
        {.perfectFinish = true,
         .reynolds      = 2000000.0,
         .cf            = {0.0029778664192228496, 0.002964466020336347, 0.0028572628292443244,
                           0.0028572628292443253, 0.0028572628292443253, 0.0028952723933537874,
                           0.002958261088276239, 0.002958261088276238, 0.002958261088276238,
                           0.002917513700024362, 0.0027221387950373483, 0.0029778664192228496}},
        {.perfectFinish = true,
         .reynolds      = 2999999.9999999995,
         .cf            = {0.00298860292744314, 0.002961705501096152, 0.0027465260903202456,
                           0.002746526090320247, 0.002746526090320247, 0.0028228193006772256,
                           0.0029492508939991256, 0.0029492508939991243, 0.0029492508939991243,
                           0.0028674622932552394, 0.0024753036596617977, 0.00298860292744314}},
        {.perfectFinish = true,
         .reynolds      = 3000000.0,
         .cf            = {0.00298860292744314, 0.002961705501096152, 0.0027465260903202456,
                           0.002746526090320247, 0.002746526090320246, 0.0028228193006772256,
                           0.0029492508939991256, 0.0029492508939991243, 0.0029492508939991243,
                           0.0028674622932552394, 0.0024753036596617977, 0.00298860292744314}},
        {.perfectFinish = true,
         .reynolds      = 1.0E7,
         .cf            = {0.002727624662742209, 0.002703076040777529, 0.00250668706506009,
                           0.002506687065060091, 0.00250668706506009, 0.0025763180087557498,
                           0.0026917090260528006, 0.0026917090260527993, 0.0026917090260527993,
                           0.002617062574203454, 0.002259148998306675, 0.002727624662742209}},
        {.perfectFinish = true,
         .reynolds      = 1.0E9,
         .cf            = {0.001537993140013354, 0.001524151201753234, 0.0014134156956722725,
                           0.001413415695672273, 0.0014134156956722727, 0.0014526776642265955,
                           0.0015177418189272671, 0.0015177418189272665, 0.0015177418189272665,
                           0.0014756518156952198, 0.0012738393625501881, 0.001537993140013354}},
        {.perfectFinish = true,
         .reynolds      = kInf,
         .cf            = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.perfectFinish = true,
         .reynolds      = kNaN,
         .cf            = {kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN}},
        {.perfectFinish = true,
         .reynolds      = -1.0,
         .cf = {0.0133, 0.0133, 0.0133, 0.013300000000000003, 0.013300000000000005,
                0.013300000000000005, 0.013300000000000005, 0.0133, 0.0133, 0.0133, 0.0133,
                0.0133}},
        {.perfectFinish = false,
         .reynolds      = 0.0,
         .cf = {0.0148, 0.0146668, 0.0136012, 0.013601200000000006, 0.013601200000000003,
                0.013483810728360973, 0.013435386187486211, 0.013435386187486208,
                0.013435386187486208, 0.01126865610734122, 0.0059948651968880495, 0.0148}},
        {.perfectFinish = false,
         .reynolds      = 1.0,
         .cf = {0.0148, 0.0146668, 0.0136012, 0.013601200000000006, 0.013601200000000003,
                0.013483810728360973, 0.013435386187486211, 0.013435386187486208,
                0.013435386187486208, 0.01126865610734122, 0.0059948651968880495, 0.0148}},
        {.perfectFinish = false,
         .reynolds      = 9999.999999999998,
         .cf = {0.0148, 0.0146668, 0.0136012, 0.013601200000000006, 0.013601200000000003,
                0.013483810728360973, 0.013435386187486211, 0.013435386187486208,
                0.013435386187486208, 0.01126865610734122, 0.0059948651968880495, 0.0148}},
        {.perfectFinish = false,
         .reynolds      = 10000.0,
         .cf            = {0.014815997081331824, 0.014682653107599837, 0.013615901317743947,
                           0.013615901317743952, 0.013615901317743949, 0.013498385161934249,
                           0.013449908279754163, 0.013449908279754157, 0.013449908279754157,
                           0.011280836216006725, 0.006001344950004802, 0.014815997081331824}},
        {.perfectFinish = false,
         .reynolds      = 100000.0,
         .cf            = {0.007343512273942596, 0.007277420663477112, 0.006748687779753246,
                           0.006748687779753249, 0.006748687779753247, 0.006690441188056598,
                           0.006666413741416474, 0.006666413741416471, 0.006666413741416471,
                           0.005591318542912056, 0.0029745517671607223, 0.007343512273942596}},
        {.perfectFinish = false,
         .reynolds      = 538999.9999999999,
         .cf            = {0.004961984436091494, 0.0049173265761666704, 0.004560063696768083,
                           0.004560063696768085, 0.004560063696768084, 0.004520706687387206,
                           0.0045044714294044065, 0.004504471429404405, 0.004504471429404405,
                           0.0037780335283982757, 0.0020098937705016937, 0.004961984436091494}},
        {.perfectFinish = false,
         .reynolds      = 539000.0,
         .cf            = {0.004961984436091494, 0.0049173265761666704, 0.004560063696768083,
                           0.004560063696768085, 0.004560063696768084, 0.004520706687387206,
                           0.0045044714294044065, 0.004504471429404405, 0.004504471429404405,
                           0.0037780335283982757, 0.0020098937705016937, 0.004961984436091494}},
        {.perfectFinish = false,
         .reynolds      = 990000.0,
         .cf            = {0.004381018681792861, 0.0043415895136567255, 0.004026156168567639,
                           0.004026156168567641, 0.00402615616856764, 0.003991407209642461,
                           0.003977072830032355, 0.0039770728300323535, 0.0039770728300323535,
                           0.0033356887111460204, 0.001774568677188945, 0.004381018681792861}},
        {.perfectFinish = false,
         .reynolds      = 1000000.0,
         .cf            = {0.004372288669946027, 0.004332938071916512, 0.004018133287680399,
                           0.0040181332876804005, 0.0040181332876804, 0.003983453572656038,
                           0.003969147757019953, 0.003969147757019952, 0.003969147757019952,
                           0.0033290417178139264, 0.001771032512041039, 0.004372288669946027}},
        {.perfectFinish = false,
         .reynolds      = 1000000.0000000001,
         .cf            = {0.004372288669946027, 0.004332938071916512, 0.004018133287680399,
                           0.0040181332876804005, 0.0040181332876804, 0.003983453572656038,
                           0.003969147757019953, 0.003969147757019952, 0.003969147757019952,
                           0.0033290417178139264, 0.001771032512041039, 0.004372288669946027}},
        {.perfectFinish = false,
         .reynolds      = 2000000.0,
         .cf            = {0.0038278664192228497, 0.003793415621449844, 0.003517809239265799,
                           0.0035178092392658, 0.0035178092392657992, 0.0034874477223141727,
                           0.0034749232173222283, 0.003474923217322227, 0.003474923217322227,
                           0.0029145209664231033, 0.0015505096739821312, 0.0038278664192228497}},
        {.perfectFinish = false,
         .reynolds      = 2999999.9999999995,
         .cf            = {0.0035552695941098067, 0.0035232721677628184, 0.0032672927569869126,
                           0.003267292756986914, 0.003267292756986913, 0.0032390933983292816,
                           0.0032274608106413923, 0.003227460810641391, 0.003227460810641391,
                           0.0027069669206020277, 0.0014400920240056204, 0.0035552695941098067}},
        {.perfectFinish = false,
         .reynolds      = 3000000.0,
         .cf            = {0.0035552695941098067, 0.0035232721677628184, 0.0032672927569869126,
                           0.003267292756986914, 0.003267292756986913, 0.0032390933983292816,
                           0.0032274608106413923, 0.003227460810641391, 0.003227460810641391,
                           0.0027069669206020277, 0.0014400920240056204, 0.0035552695941098067}},
        {.perfectFinish = false,
         .reynolds      = 1.0E7,
         .cf            = {0.002897624662742209, 0.002871546040777529, 0.00266291706506009,
                           0.002662917065060091, 0.0026629170650600904, 0.00263993395366532,
                           0.0026304531331301393, 0.0026304531331301385, 0.0026304531331301385,
                           0.0022062389089589547, 0.0011737073813728202, 0.002897624662742209}},
        {.perfectFinish = false,
         .reynolds      = 1.0E9,
         .cf            = {0.0015396931400133542, 0.001525835901753234, 0.0014149779956722725,
                           0.0014149779956722731, 0.0014149779956722727, 0.0014027655999794498,
                           0.0013977278342096413, 0.001397727834209641, 0.001397727834209641,
                           0.0011723157098407339, 6.236657310103606E-4, 0.0015396931400133542}},
        {.perfectFinish = false,
         .reynolds      = kInf,
         .cf            = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.perfectFinish = false,
         .reynolds      = kNaN,
         .cf            = {kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN}},
        {.perfectFinish = false,
         .reynolds      = -1.0,
         .cf = {0.0148, 0.0146668, 0.0136012, 0.013601200000000006, 0.013601200000000003,
                0.013483810728360973, 0.013435386187486211, 0.013435386187486208,
                0.013435386187486208, 0.01126865610734122, 0.0059948651968880495, 0.0148}},
    }};
    JavaValueDifferences                      diff;
    for (const FrictionCase& c : kCases)
    {
        for (std::size_t i = 0; i < kFrictionMachs.size(); i++)
        {
            diff.pinned(std::format("perfect {} Re {} Mach {}", c.perfectFinish, c.reynolds,
                                    kFrictionMachs.at(i)),
                        c.cf.at(i),
                        BarrowmanDragCalculator::calculateFrictionCoefficient(
                            c.perfectFinish, kFrictionMachs.at(i), c.reynolds));
        }
    }
    return diff.text();
}

TEST(BarrowmanDragCalculator, FrictionCoefficientIsOpenRockets)
{
    EXPECT_EQ(frictionCoefficientDifferences(), "");
}

/// The friction coefficient of a perfect finish at Mach 0 (no compressibility correction).
[[nodiscard]] double perfect(double re)
{
    return BarrowmanDragCalculator::calculateFrictionCoefficient(true, 0.0, re);
}

/// The friction coefficient of a turbulent boundary layer at Mach 0.
[[nodiscard]] double turbulent(double re)
{
    return BarrowmanDragCalculator::calculateFrictionCoefficient(false, 0.0, re);
}

/// The friction coefficient of a perfect finish.
[[nodiscard]] double cf(double mach, double re)
{
    return BarrowmanDragCalculator::calculateFrictionCoefficient(true, mach, re);
}

TEST(BarrowmanDragCalculator, FrictionCoefficientBranchesByReynoldsNumber)
{
    // Below a Reynolds number of 1e4 the coefficient is a constant; at Mach 0 no correction
    // applies.
    EXPECT_EQ(perfect(0), 1.33e-2);
    EXPECT_EQ(perfect(std::nextafter(1.0e4, 0.0)), 1.33e-2);
    EXPECT_EQ(turbulent(0), 1.48e-2);
    EXPECT_EQ(turbulent(std::nextafter(1.0e4, 0.0)), 1.48e-2);

    // From 1e4 the perfect finish is laminar up to 5.39e5 (exclusive): 1.328 / sqrt(Re).
    EXPECT_EQ(perfect(1.0e4), 1.328 / 100);
    EXPECT_EQ(perfect(std::nextafter(5.39e5, 0.0)), 1.328 / std::sqrt(std::nextafter(5.39e5, 0.0)));
    // ... and from there the transitional 1 / (1.5 ln Re - 5.6)^2 - 1700 / Re, which meets the
    // laminar line at 5.39e5 to within a twentieth of a percent.
    EXPECT_NEAR(perfect(5.39e5),
                (1.0 / std::pow((1.5 * std::log(5.39e5)) - 5.6, 2)) - (1700 / 5.39e5), 1e-15);
    EXPECT_LT(perfect(5.39e5), perfect(std::nextafter(5.39e5, 0.0)));
    EXPECT_NEAR(perfect(5.39e5), perfect(std::nextafter(5.39e5, 0.0)), 1e-6);

    // The turbulent coefficient is 1 / (1.5 ln Re - 5.6)^2 from 1e4, a small step up there.
    EXPECT_NEAR(turbulent(1.0e4), 1.0 / std::pow((1.5 * std::log(1.0e4)) - 5.6, 2), 1e-15);
    EXPECT_GT(turbulent(1.0e4), turbulent(std::nextafter(1.0e4, 0.0)));
    EXPECT_NEAR(turbulent(1.0e4), turbulent(std::nextafter(1.0e4, 0.0)), 2e-5);

    // An infinite Reynolds number gives 0, a negative one counts as below 1e4.
    EXPECT_EQ(perfect(kInf), 0.0);
    EXPECT_EQ(turbulent(kInf), 0.0);
    EXPECT_EQ(perfect(-1), 1.33e-2);

    // A NaN Reynolds number fails every comparison: the last branch, NaN.
    EXPECT_TRUE(std::isnan(perfect(kNaN)));
    EXPECT_TRUE(std::isnan(turbulent(kNaN)));
}

TEST(BarrowmanDragCalculator, FrictionCoefficientCompressibilityOfAPerfectFinish)
{
    // Up to a Reynolds number of 1e6 (inclusive) the Mach number does not matter (but for the
    // last bits between Mach 0.9 and 1.1, where the two corrections, both 1, are blended).
    EXPECT_EQ(cf(0.5, 1.0e6), cf(0.0, 1.0e6));
    EXPECT_EQ(cf(2.0, 1.0e6), cf(0.0, 1.0e6));
    EXPECT_NEAR(cf(1.0, 9.9e5), cf(0.0, 9.9e5), 1e-17);

    // From 3e6 the correction is in full: 1 - 0.1 M^2 subsonically.
    EXPECT_EQ(cf(0.5, 3.0e6), cf(0.0, 3.0e6) * (1 - (0.1 * 0.25)));
    EXPECT_EQ(cf(0.5, 1.0e7), cf(0.0, 1.0e7) * (1 - (0.1 * 0.25)));
    // Between them in proportion: half of it at 2e6.
    EXPECT_EQ(cf(0.5, 2.0e6), cf(0.0, 2.0e6) * (1 - (0.1 * 0.25 * 1.0e6 / 2.0e6)));
    // ... and nearly all of it just below 3e6: no step there.
    EXPECT_NEAR(cf(0.5, std::nextafter(3.0e6, 0.0)), cf(0.5, 3.0e6), 1e-12);

    // Supersonically 1 / (1 + 0.045 M^2)^0.25.
    EXPECT_NEAR(cf(2.0, 3.0e6), cf(0.0, 3.0e6) / std::pow(1 + (0.045 * 4), 0.25), 1e-15);
}

/// The friction coefficient at a Reynolds number of 1e7, where the corrections apply in full.
[[nodiscard]] double cfAt1e7(bool perfectFinish, double mach)
{
    return BarrowmanDragCalculator::calculateFrictionCoefficient(perfectFinish, mach, 1.0e7);
}

TEST(BarrowmanDragCalculator, FrictionCoefficientBlendsTheCorrectionsBetweenMach09And11)
{
    // Mach 0.9 and 1.1 belong to the blend and to the supersonic branch: no step at either.
    EXPECT_NEAR(cfAt1e7(true, std::nextafter(0.9, 0.0)), cfAt1e7(true, 0.9), 1e-15);
    EXPECT_NEAR(cfAt1e7(true, std::nextafter(1.1, 0.0)), cfAt1e7(true, 1.1), 1e-15);
    EXPECT_NEAR(cfAt1e7(false, std::nextafter(0.9, 0.0)), cfAt1e7(false, 0.9), 1e-15);
    EXPECT_NEAR(cfAt1e7(false, std::nextafter(1.1, 0.0)), cfAt1e7(false, 1.1), 1e-15);
    // In between the coefficient lies between the two ends: a perfect finish rises through the
    // blend, a turbulent boundary layer falls.
    EXPECT_GT(cfAt1e7(true, 1.0), cfAt1e7(true, 0.9));
    EXPECT_LT(cfAt1e7(true, 1.0), cfAt1e7(true, 1.1));
    EXPECT_LT(cfAt1e7(false, 1.0), cfAt1e7(false, 0.9));
    EXPECT_GT(cfAt1e7(false, 1.0), cfAt1e7(false, 1.1));
}

struct RoughnessCase
{
    double mach;
    double correction;
};

TEST(BarrowmanDragCalculator, RoughnessCorrectionIsOpenRockets)
{
    // Subsonic (below Mach 0.9) 1 - 0.1 M^2, supersonic (above 1.1) 1 / (1 + 0.18 M^2), and
    // between them the line through the two values at 0.9 and 1.1; both limits belong to the
    // line.
    const std::array<RoughnessCase, 18> cases{{
        {.mach = 0.0, .correction = 1.0},
        {.mach = 0.3, .correction = 0.991},
        {.mach = 0.6, .correction = 0.964},
        {.mach = 0.8999999999999999, .correction = 0.919},
        {.mach = 0.9, .correction = 0.9190000000000004},
        {.mach = 0.9000000000000001, .correction = 0.9190000000000003},
        {.mach = 0.95, .correction = 0.8945382246674335},
        {.mach = 1.0, .correction = 0.8700764493348664},
        {.mach = 1.05, .correction = 0.8456146740022994},
        {.mach = 1.0999999999999999, .correction = 0.8211528986697326},
        {.mach = 1.1, .correction = 0.8211528986697325},
        {.mach = 1.1000000000000003, .correction = 0.8211528986697323},
        {.mach = 1.5, .correction = 0.7117437722419929},
        {.mach = 2.0, .correction = 0.5813953488372093},
        {.mach = 5.0, .correction = 0.18181818181818182},
        {.mach = -0.5, .correction = 0.975},
        {.mach = kInf, .correction = 0.0},
        {.mach = kNaN, .correction = kNaN},
    }};
    JavaValueDifferences                diff;
    for (const RoughnessCase& c : cases)
    {
        diff.pinned(std::format("Mach {}", c.mach), c.correction,
                    BarrowmanDragCalculator::calculateRoughnessCorrection(c.mach), 0.0);
    }
    EXPECT_EQ(diff.text(), "");
}

// ======================================================================= the axial drag

struct AxialCase
{
    /// The angle of attack given to setAOA().
    double aoa;
    /// The angle of attack the conditions hold then (clamped to 0 ... pi).
    double storedAoa;
    /// toAxialDrag(conditions, 0.7) and toAxialDrag(conditions, -0.25).
    double axial;
    double negativeAxial;
};

/// The differences between toAxialDrag() and OpenRocket's from 0 to pi: 17 degrees
/// (17 * pi / 180) and pi / 2 exactly with the doubles next to them, their mirror images, and
/// angles that setAOA() clamps.
[[nodiscard]] std::string axialDragDifferences()
{
    static const std::array<AxialCase, 26> kCases{{
        {.aoa = 0.0, .storedAoa = 0.0, .axial = 0.7, .negativeAxial = -0.25},
        {.aoa = 1.0E-9, .storedAoa = 0.0, .axial = 0.7, .negativeAxial = -0.25},
        {.aoa           = 0.017453292519943295,
         .storedAoa     = 0.017453292519943295,
         .axial         = 0.7020944433136576,
         .negativeAxial = -0.2507480154691634},
        {.aoa           = 0.08726646259971647,
         .storedAoa     = 0.08726646259971647,
         .axial         = 0.7438123346224302,
         .negativeAxial = -0.2656472623651537},
        {.aoa           = 0.17453292519943295,
         .storedAoa     = 0.17453292519943295,
         .axial         = 0.8325055973946671,
         .negativeAxial = -0.29732342764095254},
        {.aoa           = 0.296705972839036,
         .storedAoa     = 0.296705972839036,
         .axial         = 0.9099999999999998,
         .negativeAxial = -0.32499999999999996},
        {.aoa           = 0.29670597283903605,
         .storedAoa     = 0.29670597283903605,
         .axial         = 0.9099999999999998,
         .negativeAxial = -0.32499999999999996},
        {.aoa           = 0.2967059728390361,
         .storedAoa     = 0.2967059728390361,
         .axial         = 0.9099999999999998,
         .negativeAxial = -0.32499999999999996},
        {.aoa           = 0.29670597283903605,
         .storedAoa     = 0.29670597283903605,
         .axial         = 0.9099999999999998,
         .negativeAxial = -0.32499999999999996},
        {.aoa           = 0.5235987755982988,
         .storedAoa     = 0.5235987755982988,
         .axial         = 0.7752142113309058,
         .negativeAxial = -0.2768622183324664},
        {.aoa           = 0.7853981633974483,
         .storedAoa     = 0.7853981633974483,
         .axial         = 0.45844489980911096,
         .negativeAxial = -0.16373032136039678},
        {.aoa           = 1.0471975511965976,
         .storedAoa     = 1.0471975511965976,
         .axial         = 0.17476927532236902,
         .negativeAxial = -0.06241759832941751},
        {.aoa           = 1.5533430342749532,
         .storedAoa     = 1.5533430342749532,
         .axial         = 9.260784848663661E-6,
         .negativeAxial = -3.307423160237022E-6},
        {.aoa           = 1.5707963267948963,
         .storedAoa     = 1.5707963267948963,
         .axial         = 3.730349362740525E-15,
         .negativeAxial = -1.3322676295501878E-15},
        {.aoa           = 1.5707963267948966,
         .storedAoa     = 1.5707963267948966,
         .axial         = -0.0,
         .negativeAxial = 0.0},
        {.aoa           = 1.5707963267948968,
         .storedAoa     = 1.5707963267948968,
         .axial         = -3.730349362740525E-15,
         .negativeAxial = 1.3322676295501878E-15},
        {.aoa           = 1.5882496193148399,
         .storedAoa     = 1.5882496193148399,
         .axial         = -9.260784848663661E-6,
         .negativeAxial = 3.307423160237022E-6},
        {.aoa           = 2.0943951023931953,
         .storedAoa     = 2.0943951023931953,
         .axial         = -0.17476927532236808,
         .negativeAxial = 0.06241759832941718},
        {.aoa           = 2.844886680750757,
         .storedAoa     = 2.844886680750757,
         .axial         = -0.9099999999999998,
         .negativeAxial = 0.32499999999999996},
        {.aoa           = 2.844886680750757,
         .storedAoa     = 2.844886680750757,
         .axial         = -0.9099999999999998,
         .negativeAxial = 0.32499999999999996},
        {.aoa           = 2.844886680750757,
         .storedAoa     = 2.844886680750757,
         .axial         = -0.9099999999999998,
         .negativeAxial = 0.32499999999999996},
        {.aoa           = 2.9670597283903604,
         .storedAoa     = 2.9670597283903604,
         .axial         = -0.8325055973946669,
         .negativeAxial = 0.2973234276409525},
        {.aoa           = 3.1415926535897927,
         .storedAoa     = 3.1415926535897927,
         .axial         = -0.7,
         .negativeAxial = 0.25},
        {.aoa           = 3.141592653589793,
         .storedAoa     = 3.141592653589793,
         .axial         = -0.7,
         .negativeAxial = 0.25},
        {.aoa = 4.0, .storedAoa = 3.141592653589793, .axial = -0.7, .negativeAxial = 0.25},
        {.aoa = -0.2, .storedAoa = 0.0, .axial = 0.7, .negativeAxial = -0.25},
    }};
    const BarrowmanDragCalculator          calculator;
    JavaValueDifferences                   diff;
    for (const AxialCase& c : kCases)
    {
        FlightConditions conditions;
        conditions.setAOA(c.aoa);
        diff.pinned(std::format("aoa {}: stored", c.aoa), c.storedAoa, conditions.getAOA(), 0.0);
        diff.pinned(std::format("aoa {}: axial", c.aoa), c.axial,
                    calculator.toAxialDrag(conditions, 0.7));
        diff.pinned(std::format("aoa {}: axial of a negative CD", c.aoa), c.negativeAxial,
                    calculator.toAxialDrag(conditions, -0.25));
    }
    return diff.text();
}

TEST(BarrowmanDragCalculator, ToAxialDragIsOpenRockets)
{
    EXPECT_EQ(axialDragDifferences(), "");
}

TEST(BarrowmanDragCalculator, ToAxialDragAtTheLimits)
{
    const BarrowmanDragCalculator calculator;
    FlightConditions              conditions;

    // 1 at 0, 1.3 at 17 degrees (which belongs to the second polynomial), 0 at 90 degrees.
    conditions.setAOA(0);
    EXPECT_EQ(calculator.toAxialDrag(conditions, 0.7), 0.7);
    conditions.setAOA(17 * kPi / 180);
    EXPECT_NEAR(calculator.toAxialDrag(conditions, 0.7), 1.3 * 0.7, 1e-15);

    // At pi / 2 exactly the multiplier is 0 and the sign has turned: -0.0 for a positive CD.
    conditions.setAOA(kPi / 2);
    EXPECT_EQ(calculator.toAxialDrag(conditions, 0.7), 0.0);
    EXPECT_TRUE(std::signbit(calculator.toAxialDrag(conditions, 0.7)));
    EXPECT_FALSE(std::signbit(calculator.toAxialDrag(conditions, -0.25)));

    // Beyond it the multiplier mirrors and the drag is negative: -1 at pi.
    conditions.setAOA(kPi);
    EXPECT_EQ(calculator.toAxialDrag(conditions, 0.7), -0.7);

    // A NaN angle of attack gives NaN.
    conditions.setAOA(kNaN);
    EXPECT_TRUE(std::isnan(calculator.toAxialDrag(conditions, 0.7)));
}

// ===================================================================== whole rockets
//
// calculateDrag() without force maps, compared with OpenRocket's (DragProbe.java). Sums over
// the components: relative 1e-9 (see BarrowmanDragCalculator, "Summation order").

/// The drag of @p configuration in @p conditions, by a new calculator, without warnings.
[[nodiscard]] AerodynamicForces dragOf(const FlightConfiguration& configuration,
                                       const FlightConditions&    conditions)
{
    AerodynamicForces       total;
    WarningSet              warnings;
    BarrowmanDragCalculator calculator;
    calculator.calculateDrag(configuration, conditions, nullptr, nullptr, total, warnings);
    EXPECT_TRUE(warnings.empty()) << warnings.toString();
    return total;
}

/// The Reynolds number the calculator works with: velocity * aerodynamic length / kinematic
/// viscosity.
[[nodiscard]] double reynoldsNumber(const FlightConfiguration& configuration,
                                    const FlightConditions&    conditions)
{
    return conditions.getVelocity() * configuration.getLengthAerodynamic() /
           conditions.getAtmosphericConditions().getKinematicViscosity();
}

struct AlphaCase
{
    bool    perfectFinish;
    double  mach;
    double  reynolds;
    DragPin drag;
};

/// The differences between the drag of the Estes Alpha III and OpenRocket's over the Mach
/// number, with and without a perfect finish. The Mach numbers put the Reynolds number in every
/// branch of the friction coefficient: below 1e4, the laminar range, the turbulent range below
/// 1e6, the partly and the fully corrected range; and the Mach number on each side of 0.9, 1
/// and 1.1.
[[nodiscard]] std::string alphaDragDifferences()
{
    static const std::array<AlphaCase, 28> kCases{{
        {.perfectFinish = false,
         .mach          = 0.001,
         .reynolds      = 6077.697547890676,
         .drag          = {1.0916397214118396, 0.8909248996713645, 0.24732422240780028, 0.0,
                           2.229888843491004, 2.229888843491004}},
        {.perfectFinish = false,
         .mach          = 0.05,
         .reynolds      = 303884.8773945338,
         .drag          = {0.4387494108745972, 0.8202697392521366, 0.24799379018354872, 0.0,
                           1.5070129403102825, 1.5070129403102825}},
        {.perfectFinish = false,
         .mach          = 0.12,
         .reynolds      = 729323.705746881,
         .drag          = {0.43822716851506655, 0.8208605487991828, 0.2511822081633031, 0.0,
                           1.5102699254775525, 1.5102699254775525}},
        {.perfectFinish = false,
         .mach          = 0.3,
         .reynolds      = 1823309.2643672025,
         .drag          = {0.43490939352510716, 0.8354080358482135, 0.2714380400346841, 0.0,
                           1.541755469408005, 1.541755469408005}},
        {.perfectFinish = false,
         .mach          = 0.6,
         .reynolds      = 3646618.528734405,
         .drag          = {0.4230601971323948, 0.8922055106733369, 0.34378029671818766, 0.0,
                           1.6590460045239195, 1.6590460045239195}},
        {.perfectFinish = false,
         .mach          = 0.89,
         .reynolds      = 5409150.817622701,
         .drag          = {0.40409709431279855, 0.9895987272193346, 0.45955470084019456, 0.0,
                           1.8532505223723277, 1.8532505223723277}},
        {.perfectFinish = false,
         .mach          = 0.9,
         .reynolds      = 5469927.793101608,
         .drag          = {0.40331153647787443, 0.9937962444580469, 0.4643507245240268, 0.0,
                           1.8614585054599482, 1.8614585054599482}},
        {.perfectFinish = false,
         .mach          = 0.95,
         .reynolds      = 5773812.670496142,
         .drag          = {0.3925762631434304, 1.0156924580229882, 0.48913464579522714, 0.0,
                           1.8974033669616457, 1.8974033669616457}},
        {.perfectFinish = false,
         .mach          = 1.0,
         .reynolds      = 6077697.547890675,
         .drag          = {0.3818409898089862, 1.0391439410348067, 0.5152582384864923, 0.0,
                           1.9362431693302853, 1.9362431693302853}},
        {.perfectFinish = false,
         .mach          = 1.1,
         .reynolds      = 6685467.302679744,
         .drag          = {0.36037044314009803, 1.1685436972746759, 0.46841658044226564, 0.0,
                           1.9973307208570397, 1.9973307208570397}},
        {.perfectFinish = false,
         .mach          = 1.15,
         .reynolds      = 6989352.180074276,
         .drag          = {0.3544760919639848, 1.1806542405109932, 0.44805064216216717, 0.0,
                           1.9831809746371452, 1.9831809746371452}},
        {.perfectFinish = false,
         .mach          = 1.5,
         .reynolds      = 9116546.321836011,
         .drag          = {0.31235524957723215, 1.247016026859001, 0.34350549232432814, 0.0,
                           1.9028767687605612, 1.9028767687605612}},
        {.perfectFinish = false,
         .mach          = 2.0,
         .reynolds      = 1.215539509578135E7,
         .drag          = {0.2551506544511693, 1.344002026327688, 0.25762911924324616, 0.0,
                           1.8567818000221035, 1.8567818000221035}},
        {.perfectFinish = false,
         .mach          = 5.0,
         .reynolds      = 3.038848773945338E7,
         .drag          = {0.07979256830109296, 1.4609780733110078, 0.10305164769729845, 0.0,
                           1.6438222893093992, 1.6438222893093992}},
        {.perfectFinish = true,
         .mach          = 0.001,
         .reynolds      = 6077.697547890676,
         .drag          = {0.9810006585580028, 0.8909248996713645, 0.24732422240780028, 0.0,
                           2.1192497806371673, 2.1192497806371673}},
        {.perfectFinish = true,
         .mach          = 0.05,
         .reynolds      = 303884.8773945338,
         .drag          = {0.1776892641494343, 0.8202697392521366, 0.24799379018354872, 0.0,
                           1.2459527935851198, 1.2459527935851198}},
        {.perfectFinish = true,
         .mach          = 0.12,
         .reynolds      = 729323.705746881,
         .drag          = {0.17175170620623867, 0.8208605487991828, 0.2511822081633031, 0.0,
                           1.2437944631687246, 1.2437944631687246}},
        {.perfectFinish = true,
         .mach          = 0.3,
         .reynolds      = 1823309.2643672025,
         .drag          = {0.43490939352510716, 0.8354080358482135, 0.2714380400346841, 0.0,
                           1.541755469408005, 1.541755469408005}},
        {.perfectFinish = true,
         .mach          = 0.6,
         .reynolds      = 3646618.528734405,
         .drag          = {0.4230601971323948, 0.8922055106733369, 0.34378029671818766, 0.0,
                           1.6590460045239195, 1.6590460045239195}},
        {.perfectFinish = true,
         .mach          = 0.89,
         .reynolds      = 5409150.817622701,
         .drag          = {0.40409709431279855, 0.9895987272193346, 0.45955470084019456, 0.0,
                           1.8532505223723277, 1.8532505223723277}},
        {.perfectFinish = true,
         .mach          = 0.9,
         .reynolds      = 5469927.793101608,
         .drag          = {0.40331153647787443, 0.9937962444580469, 0.4643507245240268, 0.0,
                           1.8614585054599482, 1.8614585054599482}},
        {.perfectFinish = true,
         .mach          = 0.95,
         .reynolds      = 5773812.670496142,
         .drag          = {0.3925762631434304, 1.0156924580229882, 0.48913464579522714, 0.0,
                           1.8974033669616457, 1.8974033669616457}},
        {.perfectFinish = true,
         .mach          = 1.0,
         .reynolds      = 6077697.547890675,
         .drag          = {0.3818409898089862, 1.0391439410348067, 0.5152582384864923, 0.0,
                           1.9362431693302853, 1.9362431693302853}},
        {.perfectFinish = true,
         .mach          = 1.1,
         .reynolds      = 6685467.302679744,
         .drag          = {0.36037044314009803, 1.1685436972746759, 0.46841658044226564, 0.0,
                           1.9973307208570397, 1.9973307208570397}},
        {.perfectFinish = true,
         .mach          = 1.15,
         .reynolds      = 6989352.180074276,
         .drag          = {0.3544760919639848, 1.1806542405109932, 0.44805064216216717, 0.0,
                           1.9831809746371452, 1.9831809746371452}},
        {.perfectFinish = true,
         .mach          = 1.5,
         .reynolds      = 9116546.321836011,
         .drag          = {0.31235524957723215, 1.247016026859001, 0.34350549232432814, 0.0,
                           1.9028767687605612, 1.9028767687605612}},
        {.perfectFinish = true,
         .mach          = 2.0,
         .reynolds      = 1.215539509578135E7,
         .drag          = {0.2551506544511693, 1.344002026327688, 0.25762911924324616, 0.0,
                           1.8567818000221035, 1.8567818000221035}},
        {.perfectFinish = true,
         .mach          = 5.0,
         .reynolds      = 3.038848773945338E7,
         .drag          = {0.1456453912366737, 1.4609780733110078, 0.10305164769729845, 0.0,
                           1.70967511224498, 1.70967511224498}},
    }};
    JavaValueDifferences                   diff;
    for (const AlphaCase& c : kCases)
    {
        const TestEstesAlphaIII alpha;
        alpha.rocket->setPerfectFinish(c.perfectFinish);
        const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
        FlightConditions           conditions{config};
        conditions.setMach(c.mach);
        const std::string what = std::format("perfect {} Mach {}", c.perfectFinish, c.mach);
        diff.pinned(what + ": Reynolds number", c.reynolds, reynoldsNumber(config, conditions));
        compareDrag(diff, what, c.drag, dragOf(config, conditions), kRocketTolerance);
    }
    return diff.text();
}

TEST(BarrowmanDragCalculator, DragOfTheAlphaOverTheMachNumberIsOpenRockets)
{
    EXPECT_EQ(alphaDragDifferences(), "");
}

TEST(BarrowmanDragCalculator, APerfectFinishOnlyMattersBelowTheRoughnessLimit)
{
    // With a perfect finish the roughness-limited coefficient is used above a Reynolds number
    // of 1e6 when it is the larger: at Mach 0.3 (Re 1.8e6) the Alpha III's regular paint limits
    // both, so the drag is the same; at Mach 0.05 (Re 3e5) the perfect finish is laminar.
    const TestEstesAlphaIII rough;
    const TestEstesAlphaIII perfect;
    perfect.rocket->setPerfectFinish(true);
    const FlightConfiguration& roughConfig   = rough.rocket->getSelectedConfiguration();
    const FlightConfiguration& perfectConfig = perfect.rocket->getSelectedConfiguration();

    FlightConditions conditions{roughConfig};
    conditions.setMach(0.3);
    EXPECT_EQ(dragOf(perfectConfig, conditions).getFrictionCD(),
              dragOf(roughConfig, conditions).getFrictionCD());

    conditions.setMach(0.05);
    EXPECT_LT(dragOf(perfectConfig, conditions).getFrictionCD(),
              0.5 * dragOf(roughConfig, conditions).getFrictionCD());
}

struct AoaCase
{
    double  aoaDegrees;
    DragPin drag;
};

[[nodiscard]] std::string alphaAoaDifferences()
{
    // Only the axial drag depends on the angle of attack.
    static const std::array<AoaCase, 8> kCases{{
        {.aoaDegrees = 0.0,
         .drag       = {0.4230601971323948, 0.8922055106733369, 0.34378029671818766, 0.0,
                        1.6590460045239195, 1.6590460045239195}},
        {.aoaDegrees = 2.0,
         .drag       = {0.4230601971323948, 0.8922055106733369, 0.34378029671818766, 0.0,
                        1.6590460045239195, 1.6780914339265551}},
        {.aoaDegrees = 10.0,
         .drag       = {0.4230601971323948, 0.8922055106733369, 0.34378029671818766, 0.0,
                        1.6590460045239195, 1.973092978716316}},
        {.aoaDegrees = 17.0,
         .drag       = {0.4230601971323948, 0.8922055106733369, 0.34378029671818766, 0.0,
                        1.6590460045239195, 2.156759805881095}},
        {.aoaDegrees = 45.0,
         .drag       = {0.4230601971323948, 0.8922055106733369, 0.34378029671818766, 0.0,
                        1.6590460045239195, 1.0865445418895345}},
        {.aoaDegrees = 90.0,
         .drag       = {0.4230601971323948, 0.8922055106733369, 0.34378029671818766, 0.0,
                        1.6590460045239195, -0.0}},
        {.aoaDegrees = 135.0,
         .drag       = {0.4230601971323948, 0.8922055106733369, 0.34378029671818766, 0.0,
                        1.6590460045239195, -1.0865445418895345}},
        {.aoaDegrees = 180.0,
         .drag       = {0.4230601971323948, 0.8922055106733369, 0.34378029671818766, 0.0,
                        1.6590460045239195, -1.6590460045239195}},
    }};
    const TestEstesAlphaIII             alpha;
    const FlightConfiguration&          config = alpha.rocket->getSelectedConfiguration();
    JavaValueDifferences                diff;
    for (const AoaCase& c : kCases)
    {
        FlightConditions conditions{config};
        conditions.setMach(0.6);
        conditions.setAOA(QtRocket::MathUtil::javaToRadians(c.aoaDegrees));
        compareDrag(diff, std::format("aoa {}", c.aoaDegrees), c.drag, dragOf(config, conditions),
                    kRocketTolerance);
    }
    return diff.text();
}

TEST(BarrowmanDragCalculator, DragAtAnAngleOfAttackIsOpenRockets)
{
    EXPECT_EQ(alphaAoaDifferences(), "");
}

/// Gives every external component of @p rocket the finish @p finish.
void setFinish(Rocket& rocket, Finish finish)
{
    for (RocketComponent* const component : allComponents(rocket))
    {
        if (auto* const external = dynamic_cast<ExternalComponent*>(component))
        {
            external->setFinish(finish);
        }
    }
}

/// The Mach numbers of FinishCase::drag and of the components with finishes of their own.
constexpr std::array<double, 4> kFinishMachs{0.05, 0.3, 0.95, 2.0};

struct FinishCase
{
    bool                   perfectFinish;
    Finish                 finish;
    std::array<DragPin, 4> drag;
};

/// The differences between the drag of the Estes Alpha III and OpenRocket's with each Finish
/// on every component, with and without a perfect finish of the rocket, subsonic, transonic
/// and supersonic (the three branches of the roughness correction).
[[nodiscard]] std::string finishDifferences()
{
    static const std::array<FinishCase, 18> kCases{{
        {.perfectFinish = false,
         .finish        = Finish::ROUGH,
         .drag          = {{{0.6704714618218306, 0.8437274741893204, 0.24799379018354872, 0.0,
                             1.7621927261946997, 1.7621927261946997},
                            {0.6646033695078111, 0.8599748207543926, 0.2714380400346841, 0.0,
                             1.7960162302968878, 1.7960162302968878},
                            {0.5999123292305868, 1.0404887163838585, 0.48913464579522714, 0.0,
                             2.1295356914096724, 2.1295356914096724},
                            {0.38990646604780876, 1.3688630648840499, 0.25762911924324616, 0.0,
                             2.0163986501751046, 2.0163986501751046}}}},
        {.perfectFinish = false,
         .finish        = Finish::ROUGHUNFINISHED,
         .drag          = {{{0.5836793087629705, 0.8315209627723754, 0.24799379018354872, 0.0,
                             1.6631940617188945, 1.6631940617188945},
                            {0.5785708376935271, 0.8475673286211959, 0.2714380400346841, 0.0,
                             1.697576206349407, 1.697576206349407},
                            {0.5222540161399774, 1.0280442770870697, 0.48913464579522714, 0.0,
                             2.0394329390222743, 2.0394329390222743},
                            {0.33943329365072106, 1.3564084810333707, 0.25762911924324616, 0.0,
                             1.9534708939273377, 1.9534708939273377}}}},
        {.perfectFinish = false,
         .finish        = Finish::UNFINISHED,
         .drag          = {{{0.526992637796437, 0.8260465417436035, 0.24799379018354872, 0.0,
                             1.6010329697235892, 1.6010329697235892},
                            {0.5223802991310518, 0.8418376988921807, 0.2714380400346841, 0.0,
                             1.6356560380579166, 1.6356560380579166},
                            {0.4715329418626982, 1.0222656010952684, 0.48913464579522714, 0.0,
                             1.9829331887531936, 1.9829331887531936},
                            {0.30646768540765246, 1.3506162088623395, 0.25762911924324616, 0.0,
                             1.914713013513238, 1.914713013513238}}}},
        {.perfectFinish = false,
         .finish        = Finish::NORMAL,
         .drag          = {{{0.4387494108745972, 0.8202697392521366, 0.24799379018354872, 0.0,
                             1.5070129403102825, 1.5070129403102825},
                            {0.43490939352510716, 0.8354080358482135, 0.2714380400346841, 0.0,
                             1.541755469408005, 1.541755469408005},
                            {0.3925762631434304, 1.0156924580229882, 0.48913464579522714, 0.0,
                             1.8974033669616457, 1.8974033669616457},
                            {0.2551506544511693, 1.344002026327688, 0.25762911924324616, 0.0,
                             1.8567818000221035, 1.8567818000221035}}}},
        {.perfectFinish = false,
         .finish        = Finish::SMOOTH,
         .drag          = {{{0.4145896036860959, 0.8170749876180318, 0.24799379018354872, 0.0,
                             1.4796583814876765, 1.4796583814876765},
                            {0.3491198457825393, 0.8312639438528041, 0.2714380400346841, 0.0,
                             1.4518218296700274, 1.4518218296700274},
                            {0.3151372825857527, 1.0112604012816084, 0.48913464579522714, 0.0,
                             1.815532329662588, 1.815532329662588},
                            {0.20482003483827663, 1.3394792084137028, 0.25762911924324616, 0.0,
                             1.8019283624952256, 1.8019283624952256}}}},
        {.perfectFinish = false,
         .finish        = Finish::OPTIMUM,
         .drag          = {{{0.4145896036860959, 0.8156534374136961, 0.24799379018354872, 0.0,
                             1.4782368312833407, 1.4782368312833407},
                            {0.28466634880133657, 0.828899507851021, 0.2714380400346841, 0.0,
                             1.3850038966870415, 1.3850038966870415},
                            {0.23882939997014702, 1.0083987350532062, 0.48913464579522714, 0.0,
                             1.7363627808185802, 1.7363627808185802},
                            {0.15772033712952305, 1.3364138785470587, 0.25762911924324616, 0.0,
                             1.7517633349198278, 1.7517633349198278}}}},
        {.perfectFinish = false,
         .finish        = Finish::POLISHED,
         .drag          = {{{0.4145896036860959, 0.8153455294411056, 0.24799379018354872, 0.0,
                             1.4779289233107502, 1.4779289233107502},
                            {0.28466634880133657, 0.8282665580334673, 0.2714380400346841, 0.0,
                             1.384370946869488, 1.384370946869488},
                            {0.21403040303984552, 1.007478674775651, 0.48913464579522714, 0.0,
                             1.7106437236107235, 1.7106437236107235},
                            {0.15772033712952305, 1.3353260473832016, 0.25762911924324616, 0.0,
                             1.750675503755971, 1.750675503755971}}}},
        {.perfectFinish = false,
         .finish        = Finish::FINISHPOLISHED,
         .drag          = {{{0.4145896036860959, 0.8151879424738147, 0.24799379018354872, 0.0,
                             1.4777713363434593, 1.4777713363434593},
                            {0.28466634880133657, 0.8279107534820621, 0.2714380400346841, 0.0,
                             1.3840151423180826, 1.3840151423180826},
                            {0.21403040303984552, 1.0068845946073648, 0.48913464579522714, 0.0,
                             1.7100496434424373, 1.7100496434424373},
                            {0.15772033712952305, 1.3345386863076516, 0.25762911924324616, 0.0,
                             1.7498881426804207, 1.7498881426804207}}}},
        {.perfectFinish = false,
         .finish        = Finish::MIRROR,
         .drag          = {{{0.4145896036860959, 0.8151348417063788, 0.24799379018354872, 0.0,
                             1.4777182355760234, 1.4777182355760234},
                            {0.28466634880133657, 0.8277845985179849, 0.2714380400346841, 0.0,
                             1.3838889873540057, 1.3838889873540057},
                            {0.21403040303984552, 1.006651659271596, 0.48913464579522714, 0.0,
                             1.7098167081066684, 1.7098167081066684},
                            {0.15772033712952305, 1.3341925116526165, 0.25762911924324616, 0.0,
                             1.7495419680253859, 1.7495419680253859}}}},
        {.perfectFinish = true,
         .finish        = Finish::ROUGH,
         .drag          = {{{0.1776892641494343, 0.8437274741893204, 0.24799379018354872, 0.0,
                             1.2694105285223034, 1.2694105285223034},
                            {0.6646033695078111, 0.8599748207543926, 0.2714380400346841, 0.0,
                             1.7960162302968878, 1.7960162302968878},
                            {0.5999123292305868, 1.0404887163838585, 0.48913464579522714, 0.0,
                             2.1295356914096724, 2.1295356914096724},
                            {0.38990646604780876, 1.3688630648840499, 0.25762911924324616, 0.0,
                             2.0163986501751046, 2.0163986501751046}}}},
        {.perfectFinish = true,
         .finish        = Finish::ROUGHUNFINISHED,
         .drag          = {{{0.1776892641494343, 0.8315209627723754, 0.24799379018354872, 0.0,
                             1.2572040171053585, 1.2572040171053585},
                            {0.5785708376935271, 0.8475673286211959, 0.2714380400346841, 0.0,
                             1.697576206349407, 1.697576206349407},
                            {0.5222540161399774, 1.0280442770870697, 0.48913464579522714, 0.0,
                             2.0394329390222743, 2.0394329390222743},
                            {0.33943329365072106, 1.3564084810333707, 0.25762911924324616, 0.0,
                             1.9534708939273377, 1.9534708939273377}}}},
        {.perfectFinish = true,
         .finish        = Finish::UNFINISHED,
         .drag          = {{{0.1776892641494343, 0.8260465417436035, 0.24799379018354872, 0.0,
                             1.2517295960765866, 1.2517295960765866},
                            {0.5223802991310518, 0.8418376988921807, 0.2714380400346841, 0.0,
                             1.6356560380579166, 1.6356560380579166},
                            {0.4715329418626982, 1.0222656010952684, 0.48913464579522714, 0.0,
                             1.9829331887531936, 1.9829331887531936},
                            {0.30646768540765246, 1.3506162088623395, 0.25762911924324616, 0.0,
                             1.914713013513238, 1.914713013513238}}}},
        {.perfectFinish = true,
         .finish        = Finish::NORMAL,
         .drag          = {{{0.1776892641494343, 0.8202697392521366, 0.24799379018354872, 0.0,
                             1.2459527935851198, 1.2459527935851198},
                            {0.43490939352510716, 0.8354080358482135, 0.2714380400346841, 0.0,
                             1.541755469408005, 1.541755469408005},
                            {0.3925762631434304, 1.0156924580229882, 0.48913464579522714, 0.0,
                             1.8974033669616457, 1.8974033669616457},
                            {0.2551506544511693, 1.344002026327688, 0.25762911924324616, 0.0,
                             1.8567818000221035, 1.8567818000221035}}}},
        {.perfectFinish = true,
         .finish        = Finish::SMOOTH,
         .drag          = {{{0.1776892641494343, 0.8170749876180318, 0.24799379018354872, 0.0,
                             1.242758041951015, 1.242758041951015},
                            {0.3491198457825393, 0.8312639438528041, 0.2714380400346841, 0.0,
                             1.4518218296700274, 1.4518218296700274},
                            {0.3151372825857527, 1.0112604012816084, 0.48913464579522714, 0.0,
                             1.815532329662588, 1.815532329662588},
                            {0.20482003483827663, 1.3394792084137028, 0.25762911924324616, 0.0,
                             1.8019283624952256, 1.8019283624952256}}}},
        {.perfectFinish = true,
         .finish        = Finish::OPTIMUM,
         .drag          = {{{0.1776892641494343, 0.8156534374136961, 0.24799379018354872, 0.0,
                             1.2413364917466791, 1.2413364917466791},
                            {0.2645833669750751, 0.828899507851021, 0.2714380400346841, 0.0,
                             1.3649209148607802, 1.3649209148607802},
                            {0.23882939997014702, 1.0083987350532062, 0.48913464579522714, 0.0,
                             1.7363627808185802, 1.7363627808185802},
                            {0.18885230478205411, 1.3364138785470587, 0.25762911924324616, 0.0,
                             1.782895302572359, 1.782895302572359}}}},
        {.perfectFinish = true,
         .finish        = Finish::POLISHED,
         .drag          = {{{0.1776892641494343, 0.8153455294411056, 0.24799379018354872, 0.0,
                             1.2410285837740886, 1.2410285837740886},
                            {0.22027973080028554, 0.8282665580334673, 0.2714380400346841, 0.0,
                             1.319984328868437, 1.319984328868437},
                            {0.19883818296701067, 1.007478674775651, 0.48913464579522714, 0.0,
                             1.695451503537889, 1.695451503537889},
                            {0.18885230478205411, 1.3353260473832016, 0.25762911924324616, 0.0,
                             1.7818074714085017, 1.7818074714085017}}}},
        {.perfectFinish = true,
         .finish        = Finish::FINISHPOLISHED,
         .drag          = {{{0.1776892641494343, 0.8151879424738147, 0.24799379018354872, 0.0,
                             1.2408709968067977, 1.2408709968067977},
                            {0.21767102413353062, 0.8279107534820621, 0.2714380400346841, 0.0,
                             1.3170198176502768, 1.3170198176502768},
                            {0.197411061711596, 1.0068845946073648, 0.48913464579522714, 0.0,
                             1.693430302114188, 1.693430302114188},
                            {0.18885230478205411, 1.3345386863076516, 0.25762911924324616, 0.0,
                             1.781020110332952, 1.781020110332952}}}},
        {.perfectFinish = true,
         .finish        = Finish::MIRROR,
         .drag          = {{{0.1776892641494343, 0.8151348417063788, 0.24799379018354872, 0.0,
                             1.2408178960393619, 1.2408178960393619},
                            {0.21767102413353062, 0.8277845985179849, 0.2714380400346841, 0.0,
                             1.3168936626861996, 1.3168936626861996},
                            {0.197411061711596, 1.006651659271596, 0.48913464579522714, 0.0,
                             1.6931973667784193, 1.6931973667784193},
                            {0.18885230478205411, 1.3341925116526165, 0.25762911924324616, 0.0,
                             1.7806739356779167, 1.7806739356779167}}}},
    }};
    JavaValueDifferences                    diff;
    for (const FinishCase& c : kCases)
    {
        const TestEstesAlphaIII alpha;
        alpha.rocket->setPerfectFinish(c.perfectFinish);
        setFinish(*alpha.rocket, c.finish);
        const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
        for (std::size_t i = 0; i < kFinishMachs.size(); i++)
        {
            FlightConditions conditions{config};
            conditions.setMach(kFinishMachs.at(i));
            compareDrag(diff,
                        std::format("perfect {} {} Mach {}", c.perfectFinish,
                                    QtRocket::finishName(c.finish), kFinishMachs.at(i)),
                        c.drag.at(i), dragOf(config, conditions), kRocketTolerance);
        }
    }
    return diff.text();
}

TEST(BarrowmanDragCalculator, DragOfEachFinishIsOpenRockets)
{
    EXPECT_EQ(finishDifferences(), "");
}

TEST(BarrowmanDragCalculator, ARougherFinishNeverHasLessFriction)
{
    double previous = kInf;
    for (const Finish finish : QtRocket::kAllFinishes)  // from the roughest to the smoothest
    {
        const TestEstesAlphaIII alpha;
        setFinish(*alpha.rocket, finish);
        const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
        FlightConditions           conditions{config};
        conditions.setMach(0.3);
        const double friction = dragOf(config, conditions).getFrictionCD();
        EXPECT_LE(friction, previous) << QtRocket::finishName(finish);
        previous = friction;
    }
    // A mirror finish (no roughness) leaves the turbulent coefficient of the rocket.
    EXPECT_GT(previous, 0);
}

struct MixedFinishCase
{
    double  mach;
    DragPin drag;
};

[[nodiscard]] std::string mixedFinishDifferences()
{
    // A mirror nose, a rough body tube, polished fins and an unfinished launch lug: each
    // component is limited by its own finish.
    static const std::array<MixedFinishCase, 4> kCases{{
        {.mach = 0.05,
         .drag = {0.53536517069474, 0.8260465417436035, 0.24799379018354872, 0.0, 1.609405502621892,
                  1.609405502621892}},
        {.mach = 0.3,
         .drag = {0.4639956277026106, 0.8418376988921807, 0.2714380400346841, 0.0,
                  1.5772713666294755, 1.5772713666294755}},
        {.mach = 0.95,
         .drag = {0.3961656617628475, 1.0222656010952684, 0.48913464579522714, 0.0,
                  1.9075659086533432, 1.9075659086533432}},
        {.mach = 2.0,
         .drag = {0.2673115819952406, 1.3506162088623395, 0.25762911924324616, 0.0,
                  1.8755569101008263, 1.8755569101008263}},
    }};
    const TestEstesAlphaIII                     alpha;
    alpha.nose->setFinish(Finish::MIRROR);
    alpha.body->setFinish(Finish::ROUGH);
    alpha.fins->setFinish(Finish::POLISHED);
    alpha.lug->setFinish(Finish::UNFINISHED);
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    JavaValueDifferences       diff;
    for (const MixedFinishCase& c : kCases)
    {
        FlightConditions conditions{config};
        conditions.setMach(c.mach);
        compareDrag(diff, std::format("Mach {}", c.mach), c.drag, dragOf(config, conditions),
                    kRocketTolerance);
    }
    return diff.text();
}

TEST(BarrowmanDragCalculator, ComponentsWithFinishesOfTheirOwnAreOpenRockets)
{
    EXPECT_EQ(mixedFinishDifferences(), "");
}

struct NozzleCase
{
    double  nozzleExitArea;
    DragPin drag;
};

/// The differences between the drag of the Estes Alpha III and OpenRocket's at Mach 0.5 with a
/// motor thrusting into the wake of its stage: no nozzle, a small one, half the base, the base
/// area (4.523893421169302e-4 m^2) with the doubles next to it, and more than the base, which
/// is clamped: the base drag of the fins is what stays.
[[nodiscard]] std::string nozzleDifferences()
{
    static const std::array<NozzleCase, 8> kCases{{
        {.nozzleExitArea = 0.0,
         .drag           = {0.42788764751461095, 0.8686439705952614, 0.31430752547676033, 0.0,
                            1.6108391435866327, 1.6108391435866327}},
        {.nozzleExitArea = 1.0E-5,
         .drag           = {0.42788764751461095, 0.8686439705952614, 0.3109365353626611, 0.0,
                            1.6074681534725337, 1.6074681534725337}},
        {.nozzleExitArea = 2.261946710584651E-4,
         .drag           = {0.42788764751461095, 0.8686439705952614, 0.2380575254767603, 0.0,
                            1.5345891435866328, 1.5345891435866328}},
        {.nozzleExitArea = 4.5238934211693014E-4,
         .drag           = {0.42788764751461095, 0.8686439705952614, 0.16180752547676033, 0.0,
                            1.4583391435866329, 1.4583391435866329}},
        {.nozzleExitArea = 4.523893421169302E-4,
         .drag           = {0.42788764751461095, 0.8686439705952614, 0.1618075254767603, 0.0,
                            1.4583391435866329, 1.4583391435866329}},
        {.nozzleExitArea = 4.5238934211693025E-4,
         .drag           = {0.42788764751461095, 0.8686439705952614, 0.1618075254767603, 0.0,
                            1.4583391435866329, 1.4583391435866329}},
        {.nozzleExitArea = 9.047786842338604E-4,
         .drag           = {0.42788764751461095, 0.8686439705952614, 0.1618075254767603, 0.0,
                            1.4583391435866329, 1.4583391435866329}},
        {.nozzleExitArea = 1.0,
         .drag           = {0.42788764751461095, 0.8686439705952614, 0.1618075254767603, 0.0,
                            1.4583391435866329, 1.4583391435866329}},
    }};
    const TestEstesAlphaIII                alpha;
    const FlightConfiguration&             config = alpha.rocket->getSelectedConfiguration();
    JavaValueDifferences                   diff;
    for (const NozzleCase& c : kCases)
    {
        FlightConditions conditions{config};
        conditions.setMach(0.5);
        conditions.setThrustingNozzleExitAreas(
            Areas{{&alpha.body->getAssembly(), c.nozzleExitArea}});
        compareDrag(diff, std::format("nozzle area {}", c.nozzleExitArea), c.drag,
                    dragOf(config, conditions), kRocketTolerance);
    }
    return diff.text();
}

TEST(BarrowmanDragCalculator, ThrustingNozzleExitAreaIsOpenRockets)
{
    EXPECT_EQ(nozzleDifferences(), "");
}

TEST(BarrowmanDragCalculator, ThrustingNozzleInAnotherWakeChangesNothing)
{
    // The nozzle area of a motor that thrusts into the wake of another assembly (here the
    // rocket itself, which is not the body tube's assembly) does not reduce the base drag.
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    FlightConditions           conditions{config};
    const AerodynamicForces    coasting = dragOf(config, conditions);

    conditions.setThrustingNozzleExitAreas(Areas{{alpha.rocket.get(), 1.0}});
    EXPECT_EQ(dragOf(config, conditions).getBaseCD(), coasting.getBaseCD());
}

/// compareDrag() of @p expected with the drag of @p configuration at Mach @p mach.
void compareDragAtMach(JavaValueDifferences& diff, std::string_view what,
                       const FlightConfiguration& configuration, double mach,
                       const DragPin& expected)
{
    FlightConditions conditions{configuration};
    conditions.setMach(mach);
    compareDrag(diff, std::format("{} Mach {}", what, mach), expected,
                dragOf(configuration, conditions), kRocketTolerance);
}

// Every part of the drag counts each instance of a component: the two boosters of the Falcon 9
// Heavy with three fins each, the side boosters of the multi-stage rocket, the four pods of the
// end plate rocket, the two rail buttons... An inactive stage counts for nothing, and the
// components that follow or precede it get the stagnation and base drag of an exposed end.
struct FalconStagesCase
{
    double  mach;
    DragPin allStages;
    DragPin withoutPayload;
    DragPin boostersOnly;
};

/// The differences between the drag of the Falcon 9 Heavy and OpenRocket's with all its stages,
/// without the payload stage, and with the boosters alone, in conditions made for the whole
/// rocket (the reference area stays that of the payload fairing).
[[nodiscard]] std::string falconStagesDifferences(const FalconStagesCase& c)
{
    const TestFalcon9Heavy falcon;
    FlightConfiguration&   config = falcon.rocket->getSelectedConfiguration();
    FlightConditions       conditions{config};
    conditions.setMach(c.mach);

    JavaValueDifferences diff;
    compareDrag(diff, std::format("falcon Mach {}", c.mach), c.allStages,
                dragOf(config, conditions), kRocketTolerance);

    // the core and the boosters without the payload stage
    config.setStageActive(TestFalcon9Heavy::kPayloadStageNumber, false);
    compareDrag(diff, std::format("falcon without payload Mach {}", c.mach), c.withoutPayload,
                dragOf(config, conditions), kRocketTolerance);

    // the boosters alone: their stage is active under an inactive core
    config.setStageActive(TestFalcon9Heavy::kCoreStageNumber, false);
    config.setStageActive(TestFalcon9Heavy::kBoosterStageNumber, true);
    compareDrag(diff, std::format("falcon boosters only Mach {}", c.mach), c.boostersOnly,
                dragOf(config, conditions), kRocketTolerance);
    return diff.text();
}

TEST(BarrowmanDragCalculator, DragOfTheFalconAndItsStagesIsOpenRockets)
{
    EXPECT_EQ(
        falconStagesDifferences(
            {.mach      = 0.3,
             .allStages = DragPin{0.530348823850528, 0.06151063350596056, 0.2305348542194396, 0.0,
                                  0.822394311575928, 0.822394311575928},
             .withoutPayload = DragPin{0.4962962417759589, 0.4785266055959199, 0.2305348542194396,
                                       0.0, 1.2053577015913184, 1.2053577015913184},
             .boostersOnly = DragPin{0.38560180536753247, 0.002004540680516794, 0.15834094704488338,
                                     0.0, 0.5459472930929327, 0.5459472930929327}}),
        "");
    EXPECT_EQ(
        falconStagesDifferences(
            {.mach      = 1.2,
             .allStages = DragPin{0.42500421827781665, 0.7423481797173132, 0.3646780154066053, 0.0,
                                  1.5320304134017353, 1.5320304134017353},
             .withoutPayload = DragPin{0.39771559167190135, 1.0284083532290118, 0.3646780154066053,
                                       0.0, 1.7908019603075185, 1.7908019603075185},
             .boostersOnly   = DragPin{0.3090086872766048, 0.3742241052513556, 0.2504760615111418,
                                       0.0, 0.9337088540391023, 0.9337088540391023}}),
        "");
}

TEST(BarrowmanDragCalculator, DragOfRocketsWithSeveralInstancesIsOpenRockets)
{
    JavaValueDifferences diff;
    {
        const TestMultiStageEventTestRocket multi;
        FlightConfiguration&                config = multi.rocket->getSelectedConfiguration();
        config.setAllStages();
        compareDragAtMach(diff, "multi", config, 0.3,
                          DragPin{0.7456190658747085, 0.7969129024857299, 0.4116830832374587, 0.0,
                                  1.954215051597897, 1.954215051597897});
        compareDragAtMach(diff, "multi", config, 1.2,
                          DragPin{0.597514756277518, 1.1673600452104966, 0.6512324146378431, 0.0,
                                  2.416107216125858, 2.416107216125858});
    }
    {
        const TestEndPlateRocket   endPlate;
        const FlightConfiguration& config = endPlate.rocket->getSelectedConfiguration();
        compareDragAtMach(diff, "endPlate", config, 0.3,
                          DragPin{0.9585630598196154, 2.490352820267833, 0.508992708093647, 0.0,
                                  3.957908588181095, 3.957908588181095});
        compareDragAtMach(diff, "endPlate", config, 1.2,
                          DragPin{0.768161115076681, 3.455870511653093, 0.8051643699279409, 0.0,
                                  5.029195996657715, 5.029195996657715});
    }
    {
        const TestTubeFinsRocket   tubeFins;
        const FlightConfiguration& config = tubeFins.rocket->getSelectedConfiguration();
        compareDragAtMach(diff, "tubeFins", config, 0.3,
                          DragPin{0.37193825593005314, 0.8114615979826996, 0.13169999999999998, 0.0,
                                  1.3150998539127525, 1.3150998539127525});
        compareDragAtMach(diff, "tubeFins", config, 1.2,
                          DragPin{0.2980591652140974, 1.0563976502568024, 0.20833333333333334, 0.0,
                                  1.562790148804233, 1.562790148804233});
    }
    EXPECT_EQ(diff.text(), "");
}

// ================================================================ the force analysis
//
// calculateDrag() with the maps of a stability breakdown, as BarrowmanCalculator calls it
// (DragProbe.analysis()): the drag of one instance goes into each component's entry, an
// override into the entry of the component or of the assembly, and the total into the rocket's
// assembly entry. The pins are what the getters of the entries give afterwards: NaN where the
// calculator wrote nothing, and with the component's override applied (AerodynamicForces).

using D                          = DragPin;
constexpr std::nullopt_t kAbsent = std::nullopt;

struct AnalysisPin
{
    /// The component's index in allComponents(rocket), and its name.
    int              index;
    std::string_view name;
    /// The drag of its entry in the component map, and in the assembly map.
    std::optional<DragPin> each;
    std::optional<DragPin> assembly;
};

/// compareDrag() of an entry that the pins expect (or expect to be absent).
void compareEntry(JavaValueDifferences& diff, const std::string& what,
                  const std::optional<DragPin>& expected, const AerodynamicForces* actual,
                  double tolerance)
{
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
    compareDrag(diff, what, *expected, *actual, tolerance);
}

/// The differences between the maps and the total that a force analysis of @p configuration in
/// @p conditions leaves, and OpenRocket's.
[[nodiscard]] std::string analysisDifferences(Rocket&                      rocket,
                                              const FlightConfiguration&   configuration,
                                              const FlightConditions&      conditions,
                                              std::span<const AnalysisPin> pins,
                                              const DragPin&               total)
{
    WarningSet                   warnings;
    BarrowmanStabilityCalculator stability;
    StabilityForceBreakdown      breakdown =
        stability.getForceAnalysis(configuration, conditions, warnings);
    ForceMap& each     = breakdown.getComponentForces();
    ForceMap& assembly = breakdown.getAssemblyForces();

    AerodynamicForces* const rocketForces = assembly.get(&rocket);
    if (rocketForces == nullptr)
    {
        return "the breakdown has no entry for the rocket";
    }
    const std::size_t       eachSize     = each.size();
    const std::size_t       assemblySize = assembly.size();
    BarrowmanDragCalculator drag;
    drag.calculateDrag(configuration, conditions, &each, &assembly, *rocketForces, warnings);

    JavaValueDifferences diff;
    if (each.size() != eachSize || assembly.size() != assemblySize)
    {
        diff.problem("calculateDrag() added an entry to a map");
    }
    compareDrag(diff, "total", total, *rocketForces, kRocketTolerance);

    const std::vector<RocketComponent*> components = allComponents(rocket);
    if (components.size() != pins.size())
    {
        diff.problem(std::format("{} components, {} pins", components.size(), pins.size()));
        return diff.text();
    }
    for (const AnalysisPin& pin : pins)
    {
        const RocketComponent& component = *components.at(static_cast<std::size_t>(pin.index));
        diff.name(std::format("component {}", pin.index), pin.name, component.getName());
        compareEntry(diff, std::format("{} (component map)", pin.name), pin.each,
                     each.get(&component), kComponentTolerance);
        // The rocket's entry of the assembly map is the total, a sum over the components.
        compareEntry(diff, std::format("{} (assembly map)", pin.name), pin.assembly,
                     assembly.get(&component),
                     &component == &rocket ? kRocketTolerance : kComponentTolerance);
    }
    return diff.text();
}

/// analysisDifferences() in the conditions the flight configuration starts with (Mach 0.3).
[[nodiscard]] std::string analysisDifferences(Rocket& rocket, std::span<const AnalysisPin> pins,
                                              const DragPin& total)
{
    const FlightConfiguration& configuration = rocket.getSelectedConfiguration();
    const FlightConditions     conditions{configuration};
    return analysisDifferences(rocket, configuration, conditions, pins, total);
}

TEST(BarrowmanDragCalculator, ForceAnalysisOfTheAlphaIsOpenRockets)
{
    // The symmetric components carry the body's friction correction; the nose cone has no base
    // drag (the body tube follows it), the body tube has it all, and the internal components
    // have no entry in the component map.
    static const std::array<AnalysisPin, 10> kPins{{
        {.index    = 0,
         .name     = "Estes Alpha III / Code Verification Rocket",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{0.43490939352510716, 0.8354080358482135, 0.2714380400346841, 0.0,
                       1.541755469408005, 1.541755469408005}},
        {.index    = 1,
         .name     = "Stage",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0}},
        {.index    = 2,
         .name     = "Nose Cone",
         .each     = D{0.048735737950248124, 1.4210854715202004E-14, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 3,
         .name     = "Body Tube",
         .each     = D{0.20527609492548407, 0.0, 0.13169999999999998, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index = 4,
         .name  = "3 Fin Set",
         .each  = D{0.060299186883124974, 0.2650439357458301, 0.04657934667822804, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 5,
         .name     = "Launch Lugs",
         .each     = D{0.0, 0.04027622861070913, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 6,
         .name     = "Motor Mount Tube",
         .each     = kAbsent,
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 7,
         .name     = "Engine Block",
         .each     = kAbsent,
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 8,
         .name     = "Parachute",
         .each     = kAbsent,
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 9,
         .name     = "Centering Rings",
         .each     = kAbsent,
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
    }};
    const TestEstesAlphaIII                  alpha;
    EXPECT_EQ(
        analysisDifferences(*alpha.rocket, kPins,
                            DragPin{0.43490939352510716, 0.8354080358482135, 0.2714380400346841,
                                    0.0, 1.541755469408005, 1.541755469408005}),
        "");
}

TEST(BarrowmanDragCalculator, AnOverriddenComponentHasItsOverrideCDOnly)
{
    // the body tube alone: its friction and base drag are gone from the total and 0.3 comes in.
    // The nose cone still has no base drag (the body tube follows it all the same), and its
    // friction grows: the body whose fineness corrects it is now the nose cone alone.
    static const std::array<AnalysisPin, 10> kPins{{
        {.index    = 0,
         .name     = "Estes Alpha III / Code Verification Rocket",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{0.2355480661839165, 0.8354080358482135, 0.1397380400346841, 0.3,
                       1.5106941420668143, 1.5106941420668143}},
        {.index    = 1,
         .name     = "Stage",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0}},
        {.index    = 2,
         .name     = "Nose Cone",
         .each     = D{0.05465050553454156, 1.4210854715202004E-14, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 3,
         .name     = "Body Tube",
         .each     = D{0.0, 0.0, 0.0, 0.3, 0.3, 0.0},
         .assembly = D{0.0, 0.0, 0.0, kNaN, 0.3, 0.0}},
        {.index = 4,
         .name  = "3 Fin Set",
         .each  = D{0.060299186883124974, 0.2650439357458301, 0.04657934667822804, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 5,
         .name     = "Launch Lugs",
         .each     = D{0.0, 0.04027622861070913, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 6,
         .name     = "Motor Mount Tube",
         .each     = kAbsent,
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 7,
         .name     = "Engine Block",
         .each     = kAbsent,
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 8,
         .name     = "Parachute",
         .each     = kAbsent,
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 9,
         .name     = "Centering Rings",
         .each     = kAbsent,
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
    }};
    const TestEstesAlphaIII                  alpha;
    alpha.body->setCDOverridden(true);
    alpha.body->setOverrideCD(0.3);
    EXPECT_EQ(
        analysisDifferences(*alpha.rocket, kPins,
                            DragPin{0.2355480661839165, 0.8354080358482135, 0.1397380400346841, 0.3,
                                    1.5106941420668143, 1.5106941420668143}),
        "");
}

TEST(BarrowmanDragCalculator, AnOverrideOfTheSubcomponentsSilencesThem)
{
    // the body tube and what it holds: only the nose cone is left; a fin set overridden under
    // an overriding ancestor does not count.
    static const std::array<AnalysisPin, 10> kPins{{
        {.index    = 0,
         .name     = "Estes Alpha III / Code Verification Rocket",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{0.05465050553454156, 1.4210854715202004E-14, 0.0, 0.3, 0.35465050553455574,
                       0.35465050553455574}},
        {.index    = 1,
         .name     = "Stage",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0}},
        {.index    = 2,
         .name     = "Nose Cone",
         .each     = D{0.05465050553454156, 1.4210854715202004E-14, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 3,
         .name     = "Body Tube",
         .each     = D{0.0, 0.0, 0.0, 0.3, 0.3, 0.0},
         .assembly = D{0.0, 0.0, 0.0, kNaN, 0.3, 0.0}},
        {.index    = 4,
         .name     = "3 Fin Set",
         .each     = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 5,
         .name     = "Launch Lugs",
         .each     = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 6,
         .name     = "Motor Mount Tube",
         .each     = kAbsent,
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 7,
         .name     = "Engine Block",
         .each     = kAbsent,
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 8,
         .name     = "Parachute",
         .each     = kAbsent,
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 9,
         .name     = "Centering Rings",
         .each     = kAbsent,
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    }};
    const TestEstesAlphaIII                  alpha;
    alpha.body->setCDOverridden(true);
    alpha.body->setOverrideCD(0.3);
    alpha.body->setSubcomponentsOverriddenCD(true);
    alpha.fins->setCDOverridden(true);
    alpha.fins->setOverrideCD(0.11);
    EXPECT_EQ(analysisDifferences(*alpha.rocket, kPins,
                                  DragPin{0.05465050553454156, 1.4210854715202004E-14, 0.0, 0.3,
                                          0.35465050553455574, 0.35465050553455574}),
              "");
}

TEST(BarrowmanDragCalculator, AnOverriddenAssemblyAddsItsOverrideCD)
{
    // the stage (0.5, into the assembly map) and a fin set of three (3 * 0.07, into the
    // component map): the other components keep their drag ...
    static const std::array<AnalysisPin, 10> kStagePins{{
        {.index    = 0,
         .name     = "Estes Alpha III / Code Verification Rocket",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{0.2540118328757322, 0.040276228610723344, 0.13169999999999998, 0.71,
                       1.1359880614864555, 1.1359880614864555}},
        {.index    = 1,
         .name     = "Stage",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.5, 0.0, 0.0}},
        {.index    = 2,
         .name     = "Nose Cone",
         .each     = D{0.048735737950248124, 1.4210854715202004E-14, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 3,
         .name     = "Body Tube",
         .each     = D{0.20527609492548407, 0.0, 0.13169999999999998, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 4,
         .name     = "3 Fin Set",
         .each     = D{0.0, 0.0, 0.0, 0.21000000000000002, 0.07, 0.0},
         .assembly = D{0.0, 0.0, 0.0, kNaN, 0.07, 0.0}},
        {.index    = 5,
         .name     = "Launch Lugs",
         .each     = D{0.0, 0.04027622861070913, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 6,
         .name     = "Motor Mount Tube",
         .each     = kAbsent,
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 7,
         .name     = "Engine Block",
         .each     = kAbsent,
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 8,
         .name     = "Parachute",
         .each     = kAbsent,
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 9,
         .name     = "Centering Rings",
         .each     = kAbsent,
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
    }};
    // ... until the stage overrides its subcomponents too.
    static const std::array<AnalysisPin, 10> kStageAndChildrenPins{{
        {.index    = 0,
         .name     = "Estes Alpha III / Code Verification Rocket",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{0.0, 0.0, 0.0, 0.5, 0.5, 0.5}},
        {.index    = 1,
         .name     = "Stage",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.5, 0.0, 0.0}},
        {.index    = 2,
         .name     = "Nose Cone",
         .each     = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 3,
         .name     = "Body Tube",
         .each     = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 4,
         .name     = "3 Fin Set",
         .each     = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 5,
         .name     = "Launch Lugs",
         .each     = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 6,
         .name     = "Motor Mount Tube",
         .each     = kAbsent,
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 7,
         .name     = "Engine Block",
         .each     = kAbsent,
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 8,
         .name     = "Parachute",
         .each     = kAbsent,
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 9,
         .name     = "Centering Rings",
         .each     = kAbsent,
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    }};
    const TestEstesAlphaIII                  alpha;
    alpha.stage->setCDOverridden(true);
    alpha.stage->setOverrideCD(0.5);
    alpha.fins->setCDOverridden(true);
    alpha.fins->setOverrideCD(0.07);
    EXPECT_EQ(
        analysisDifferences(*alpha.rocket, kStagePins,
                            DragPin{0.2540118328757322, 0.040276228610723344, 0.13169999999999998,
                                    0.71, 1.1359880614864555, 1.1359880614864555}),
        "");

    alpha.stage->setSubcomponentsOverriddenCD(true);
    EXPECT_EQ(analysisDifferences(*alpha.rocket, kStageAndChildrenPins,
                                  DragPin{0.0, 0.0, 0.0, 0.5, 0.5, 0.5}),
              "");
}

TEST(BarrowmanDragCalculator, AnOverriddenRocketAddsItsOverrideCD)
{
    static const std::array<AnalysisPin, 10> kRocketPins{{
        {.index    = 0,
         .name     = "Estes Alpha III / Code Verification Rocket",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{0.43490939352510716, 0.8354080358482135, 0.2714380400346841, 0.9,
                       2.441755469408005, 2.441755469408005}},
        {.index    = 1,
         .name     = "Stage",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0}},
        {.index    = 2,
         .name     = "Nose Cone",
         .each     = D{0.048735737950248124, 1.4210854715202004E-14, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 3,
         .name     = "Body Tube",
         .each     = D{0.20527609492548407, 0.0, 0.13169999999999998, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index = 4,
         .name  = "3 Fin Set",
         .each  = D{0.060299186883124974, 0.2650439357458301, 0.04657934667822804, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 5,
         .name     = "Launch Lugs",
         .each     = D{0.0, 0.04027622861070913, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 6,
         .name     = "Motor Mount Tube",
         .each     = kAbsent,
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 7,
         .name     = "Engine Block",
         .each     = kAbsent,
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 8,
         .name     = "Parachute",
         .each     = kAbsent,
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 9,
         .name     = "Centering Rings",
         .each     = kAbsent,
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
    }};
    static const std::array<AnalysisPin, 10> kRocketAndChildrenPins{{
        {.index    = 0,
         .name     = "Estes Alpha III / Code Verification Rocket",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{0.0, 0.0, 0.0, 0.9, 0.9, 0.9}},
        {.index    = 1,
         .name     = "Stage",
         .each     = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 2,
         .name     = "Nose Cone",
         .each     = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 3,
         .name     = "Body Tube",
         .each     = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 4,
         .name     = "3 Fin Set",
         .each     = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 5,
         .name     = "Launch Lugs",
         .each     = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 6,
         .name     = "Motor Mount Tube",
         .each     = kAbsent,
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 7,
         .name     = "Engine Block",
         .each     = kAbsent,
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 8,
         .name     = "Parachute",
         .each     = kAbsent,
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {.index    = 9,
         .name     = "Centering Rings",
         .each     = kAbsent,
         .assembly = D{0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
    }};
    const TestEstesAlphaIII                  alpha;
    alpha.rocket->setCDOverridden(true);
    alpha.rocket->setOverrideCD(0.9);
    EXPECT_EQ(
        analysisDifferences(*alpha.rocket, kRocketPins,
                            DragPin{0.43490939352510716, 0.8354080358482135, 0.2714380400346841,
                                    0.9, 2.441755469408005, 2.441755469408005}),
        "");

    alpha.rocket->setSubcomponentsOverriddenCD(true);
    EXPECT_EQ(analysisDifferences(*alpha.rocket, kRocketAndChildrenPins,
                                  DragPin{0.0, 0.0, 0.0, 0.9, 0.9, 0.9}),
              "");
}

TEST(BarrowmanDragCalculator, OverridesCountEveryInstance)
{
    // a booster set of two with an override (2 * 0.2) and its fin set, three fins on each of
    // the two boosters (6 * 0.05)
    static const std::array<AnalysisPin, 16> kPins{{
        {.index    = 0,
         .name     = "Falcon9H Scale Rocket",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{0.39443893966375154, 0.059506092825443765, 0.2165817215236686,
                       0.7000000000000001, 1.370526754012864, 1.370526754012864}},
        {.index    = 1,
         .name     = "Payload Fairing Stage",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0}},
        {.index    = 2,
         .name     = "PL Fairing Nose",
         .each     = D{0.0142382921023787, 0.0, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 3,
         .name     = "PL Fairing Body",
         .each     = D{0.022477049702254344, 0.0, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 4,
         .name     = "PL Fairing Transition",
         .each     = D{0.002881841445784801, 0.059506092825443765, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 5,
         .name     = "Upper Stage Body",
         .each     = D{0.022693175180160636, 0.0, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 6,
         .name     = "Parachute",
         .each     = kAbsent,
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 7,
         .name     = "Shock Cord",
         .each     = kAbsent,
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 8,
         .name     = "Interstage",
         .each     = D{0.015128783453440424, 0.0, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 9,
         .name     = "Core Stage",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0}},
        {.index    = 10,
         .name     = "Core Stage Body",
         .each     = D{0.10085855635626952, 0.0, 0.0721939071745562, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 11,
         .name     = "Booster Stage",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.4, 0.0, 0.0}},
        {.index    = 12,
         .name     = "Booster Nose",
         .each     = D{0.007222064355462051, 0.0, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 13,
         .name     = "Booster Body",
         .each     = D{0.10085855635626952, 0.0, 0.0721939071745562, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 14,
         .name     = "Booster Motor Tubes",
         .each     = kAbsent,
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 15,
         .name     = "Booster Fins",
         .each     = D{0.0, 0.0, 0.0, 0.30000000000000004, 0.05, 0.0},
         .assembly = D{0.0, 0.0, 0.0, kNaN, 0.05, 0.0}},
    }};
    const TestFalcon9Heavy                   falcon;
    falcon.boosterStage->setCDOverridden(true);
    falcon.boosterStage->setOverrideCD(0.2);
    falcon.boosterFins->setCDOverridden(true);
    falcon.boosterFins->setOverrideCD(0.05);
    EXPECT_EQ(
        analysisDifferences(*falcon.rocket, kPins,
                            DragPin{0.39443893966375154, 0.059506092825443765, 0.2165817215236686,
                                    0.7000000000000001, 1.370526754012864, 1.370526754012864}),
        "");
}

// The rocket with the steps: a body tube of length 0 ("Disk") is a disk as wide as its radius,
// with the stagnation drag of the ring beyond the tube ahead of it and the base drag of the ring
// beyond the tube behind it; a transition of length 0 whose aft end is the wider one counts
// with its aft radius at the front (the stagnation drag of the step), and has no base drag.
TEST(BarrowmanDragCalculator, ZeroLengthComponentsAndDiameterStepsAreOpenRockets)
{
    static const std::array<AnalysisPin, 11> kSubsonicPins{{
        {.index    = 0,
         .name     = "Steps",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{0.08131515656090255, 1.1545886503072258, 0.22110142360312154, 0.0,
                       1.4570052304712497, 1.4570052304712497}},
        {.index    = 1,
         .name     = "Stage",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0}},
        {.index    = 2,
         .name     = "Nose Cone",
         .each     = D{0.00884105091602706, 0.03569605278167591, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 3,
         .name     = "Tube A",
         .each     = D{0.02795785780416357, 0.0, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 4,
         .name     = "Rail Button",
         .each     = D{0.0, 0.022366536228044697, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 5,
         .name     = "Disk",
         .each     = D{0.0, 0.48294284722222225, 0.11706666666666665, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 6,
         .name     = "Tube B",
         .each     = D{0.013978928902081786, 0.0, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 7,
         .name     = "Launch Lug",
         .each     = D{0.0, 0.009416103655870133, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 8,
         .name     = "Flat Transition",
         .each     = D{0.0, 0.5070899895833335, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 9,
         .name     = "Tube C",
         .each     = D{0.017473661127602232, 0.0, 0.09145833333333334, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index = 10,
         .name  = "Trapezoidal Fin Set",
         .each = D{0.004354552603675964, 0.024903528202678164, 0.004192141201040523, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
    }};
    static const std::array<AnalysisPin, 11> kSupersonicPins{{
        {.index    = 0,
         .name     = "Steps",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{0.05840116677205349, 1.8777692120130087, 0.27980438319807843, 0.0,
                       2.2159747619831407, 2.2159747619831407}},
        {.index    = 1,
         .name     = "Stage",
         .each     = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, kNaN, 0.0, 0.0}},
        {.index    = 2,
         .name     = "Nose Cone",
         .each     = D{0.006349710322458755, 0.1561872694388068, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 3,
         .name     = "Tube A",
         .each     = D{0.020079547101251885, 0.0, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 4,
         .name     = "Rail Button",
         .each     = D{0.0, 0.05381337018133057, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 5,
         .name     = "Disk",
         .each     = D{0.0, 0.7263179393385157, 0.14814814814814814, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 6,
         .name     = "Tube B",
         .each     = D{0.010039773550625943, 0.0, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 7,
         .name     = "Launch Lug",
         .each     = D{0.0, 0.012643054793996734, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 8,
         .name     = "Flat Transition",
         .each     = D{0.0, 0.7626338363054417, kNaN, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index    = 9,
         .name     = "Tube C",
         .each     = D{0.012549716938282426, 0.0, 0.11574074074074076, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
        {.index = 10,
         .name  = "Trapezoidal Fin Set",
         .each =
             D{0.0031274729531448257, 0.037453457257862276, 0.005305164769729844, 0.0, 0.0, 0.0},
         .assembly = D{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0}},
    }};
    const TestStepsRocket                    steps;
    const FlightConfiguration&               config = steps.rocket->getSelectedConfiguration();

    FlightConditions conditions{config};
    conditions.setMach(0.3);
    EXPECT_EQ(
        analysisDifferences(*steps.rocket, config, conditions, kSubsonicPins,
                            DragPin{0.08131515656090255, 1.1545886503072258, 0.22110142360312154,
                                    0.0, 1.4570052304712497, 1.4570052304712497}),
        "");
    conditions.setMach(1.5);
    EXPECT_EQ(
        analysisDifferences(*steps.rocket, config, conditions, kSupersonicPins,
                            DragPin{0.05840116677205349, 1.8777692120130087, 0.27980438319807843,
                                    0.0, 2.2159747619831407, 2.2159747619831407}),
        "");

    // The same totals without the maps.
    JavaValueDifferences diff;
    compareDragAtMach(diff, "steps", config, 0.3,
                      DragPin{0.08131515656090255, 1.1545886503072258, 0.22110142360312154, 0.0,
                              1.4570052304712497, 1.4570052304712497});
    compareDragAtMach(diff, "steps", config, 1.5,
                      DragPin{0.05840116677205349, 1.8777692120130087, 0.27980438319807843, 0.0,
                              2.2159747619831407, 2.2159747619831407});
    EXPECT_EQ(diff.text(), "");
}

// NOLINTEND(modernize-use-std-numbers)

// ================================================================== the calculator itself

TEST(BarrowmanDragCalculator, WithoutMapsTheTotalIsTheSame)
{
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    const FlightConditions     conditions{config};

    WarningSet                   warnings;
    BarrowmanStabilityCalculator stability;
    StabilityForceBreakdown breakdown    = stability.getForceAnalysis(config, conditions, warnings);
    AerodynamicForces*      rocketForces = breakdown.getAssemblyForces().get(alpha.rocket.get());
    ASSERT_NE(rocketForces, nullptr);
    BarrowmanDragCalculator calculator;
    calculator.calculateDrag(config, conditions, &breakdown.getComponentForces(),
                             &breakdown.getAssemblyForces(), *rocketForces, warnings);

    // The same calculator, now without maps and into forces without a component.
    AerodynamicForces total;
    calculator.calculateDrag(config, conditions, nullptr, nullptr, total, warnings);
    EXPECT_EQ(total.getFrictionCD(), rocketForces->getFrictionCD());
    EXPECT_EQ(total.getPressureCD(), rocketForces->getPressureCD());
    EXPECT_EQ(total.getBaseCD(), rocketForces->getBaseCD());
    EXPECT_EQ(total.getOverrideCD(), rocketForces->getOverrideCD());
    EXPECT_EQ(total.getCD(), rocketForces->getCD());
    EXPECT_EQ(total.getCDaxial(), rocketForces->getCDaxial());
    // Nothing but the drag is touched: the forces were never zeroed.
    EXPECT_TRUE(std::isnan(total.getCN()));
    EXPECT_EQ(total.getComponent(), nullptr);
    EXPECT_TRUE(warnings.empty());
}

TEST(BarrowmanDragCalculator, MapsWithoutEntriesAreLeftEmpty)
{
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    const FlightConditions     conditions{config};
    alpha.stage->setCDOverridden(true);
    alpha.stage->setOverrideCD(0.5);
    alpha.fins->setCDOverridden(true);
    alpha.fins->setOverrideCD(0.07);

    ForceMap                each;
    ForceMap                assembly;
    AerodynamicForces       total;
    WarningSet              warnings;
    BarrowmanDragCalculator calculator;
    calculator.calculateDrag(config, conditions, &each, &assembly, total, warnings);

    EXPECT_TRUE(each.empty());
    EXPECT_TRUE(assembly.empty());
    EXPECT_NEAR(total.getOverrideCD(), 0.5 + (3 * 0.07), 1e-15);
}

TEST(BarrowmanDragCalculator, TheCalculationsStayUntilTheCacheIsVoided)
{
    // The calculator does not look at the rocket's modification ids: after a change of the
    // rocket it goes on with the calculations it made, which hold copies of the geometry, until
    // voidAerodynamicCache() (which BarrowmanCalculator calls).
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    const FlightConditions     conditions{config};
    WarningSet                 warnings;
    BarrowmanDragCalculator    calculator;

    AerodynamicForces before;
    calculator.calculateDrag(config, conditions, nullptr, nullptr, before, warnings);

    alpha.fins->setThickness(2 * alpha.fins->getThickness());
    AerodynamicForces stale;
    calculator.calculateDrag(config, conditions, nullptr, nullptr, stale, warnings);
    EXPECT_EQ(stale.getPressureCD(), before.getPressureCD());
    EXPECT_EQ(stale.getCD(), before.getCD());

    calculator.voidAerodynamicCache();
    AerodynamicForces fresh;
    calculator.calculateDrag(config, conditions, nullptr, nullptr, fresh, warnings);
    EXPECT_GT(fresh.getPressureCD(), before.getPressureCD());
    EXPECT_EQ(fresh.getCD(), dragOf(config, conditions).getCD());
}

TEST(BarrowmanDragCalculator, AComponentAddedBehindItsBackHasNoCalculation)
{
    // An aerodynamic component that became active after the calculations were made (Java: a
    // NullPointerException from calcMap.get(c)); voiding the cache makes one.
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    const FlightConditions     conditions{config};
    WarningSet                 warnings;
    BarrowmanDragCalculator    calculator;
    AerodynamicForces          total;
    calculator.calculateDrag(config, conditions, nullptr, nullptr, total, warnings);

    alpha.body->addChild(std::make_unique<TrapezoidFinSet>(3, 0.02, 0.02, 0, 0.02));
    EXPECT_THROW(calculator.calculateDrag(config, conditions, nullptr, nullptr, total, warnings),
                 BugError);

    calculator.voidAerodynamicCache();
    EXPECT_NO_THROW(
        calculator.calculateDrag(config, conditions, nullptr, nullptr, total, warnings));
    EXPECT_EQ(total.getCD(), dragOf(config, conditions).getCD());
}

TEST(BarrowmanDragCalculator, NewInstanceIsAnIndependentBarrowmanDragCalculator)
{
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    const FlightConditions     conditions{config};
    WarningSet                 warnings;
    BarrowmanDragCalculator    calculator;
    AerodynamicForces          total;
    calculator.calculateDrag(config, conditions, nullptr, nullptr, total, warnings);

    const std::unique_ptr<DragCalculator> instance = calculator.newInstance();
    ASSERT_NE(dynamic_cast<BarrowmanDragCalculator*>(instance.get()), nullptr);

    // The new instance has no calculations of its own yet: it sees the rocket as it is now.
    alpha.fins->setThickness(2 * alpha.fins->getThickness());
    AerodynamicForces fromInstance;
    instance->calculateDrag(config, conditions, nullptr, nullptr, fromInstance, warnings);
    EXPECT_EQ(fromInstance.getCD(), dragOf(config, conditions).getCD());
    EXPECT_NE(fromInstance.getCD(), total.getCD());
    EXPECT_EQ(instance->toAxialDrag(conditions, 0.7), calculator.toAxialDrag(conditions, 0.7));
}

TEST(BarrowmanDragCalculator, AnEmptyRocketHasNoDrag)
{
    Rocket rocket;
    rocket.enableEvents();
    const FlightConfiguration& config = rocket.getSelectedConfiguration();
    const FlightConditions     conditions{config};
    const AerodynamicForces    total = dragOf(config, conditions);
    EXPECT_EQ(total.getFrictionCD(), 0.0);
    EXPECT_EQ(total.getPressureCD(), 0.0);
    EXPECT_EQ(total.getBaseCD(), 0.0);
    EXPECT_EQ(total.getOverrideCD(), 0.0);
    EXPECT_EQ(total.getCD(), 0.0);
    EXPECT_EQ(total.getCDaxial(), 0.0);
}

}  // namespace
