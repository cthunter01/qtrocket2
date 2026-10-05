#include "QtRocket/aero/barrowman/TubeCalc.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/BarrowmanDragCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/Tube.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Transformation.h"
#include "rocket/JavaValueDifferences.h"

namespace
{

using QtRocket::AerodynamicForces;
using QtRocket::BarrowmanDragCalculator;
using QtRocket::Finish;
using QtRocket::FlightConditions;
using QtRocket::LaunchLug;
using QtRocket::Transformation;
using QtRocket::Tube;
using QtRocket::TubeCalc;
using QtRocket::TubeFinSet;
using QtRocket::WarningSet;
using QtRocket::Test::JavaValueDifferences;
namespace MathUtil = QtRocket::MathUtil;

/// The smallest concrete TubeCalc, giving access to its protected members.
class TestTubeCalc final : public TubeCalc
{
public:
    explicit TestTubeCalc(const Tube& tube) : TubeCalc(tube) { }

    using TubeCalc::getInnerArea;

    void calculateNonaxialForces(const FlightConditions& /*conditions*/,
                                 const Transformation& /*transform*/, AerodynamicForces& /*forces*/,
                                 WarningSet& /*warnings*/) override
    {
    }
    [[nodiscard]] double calculateFrictionCD(const FlightConditions& /*conditions*/,
                                             double /*componentCf*/,
                                             WarningSet& /*warnings*/) override
    {
        return 0;
    }
};

constexpr double kRefLength = 0.05;

/// The Mach numbers of the pins: at rest, at a velocity below and just above
/// MathUtil::kEpsilon (the speed of sound is 343.4189 m/s), and the usual ones.
constexpr std::array<double, 11> kMachs{0.0, 1.0E-12, 1.0E-10, 0.05, 0.3, 0.6,
                                        0.9, 1.0,     1.1,     1.5,  2.0};

struct TubeCase
{
    std::string_view name;
    double           length;
    double           outerRadius;
    double           thickness;
    Finish           finish;
    /// pi * inner radius^2.
    double innerArea;
    /// calculatePressureCD() at each of kMachs, with the stagnation and base drag coefficients
    /// of that Mach number.
    std::array<double, 11> pressureCD;
};

// OpenRocket's LaunchLugCalc (JDK 17, the probe TubeProbe.java of the port's scratch directory)
// for a launch lug with these dimensions and finish, in fresh flight conditions with the
// reference length kRefLength. The inner area of 7.85e-9 m^2 is below MathUtil::kEpsilon and the
// one of 1.13e-8 m^2 above it.
// NOLINTBEGIN(modernize-use-std-numbers): OpenRocket's results, not approximations of constants
constexpr std::array<TubeCase, 9> kTubeCases{{
    // PINS-BEGIN (TubeProbe.java)
    {.name        = "launch lug",
     .length      = 0.05,
     .outerRadius = 0.003,
     .thickness   = 0.001,
     .finish      = Finish::NORMAL,
     .innerArea   = 1.2566370614359172E-5,
     .pressureCD  = {0.0, 0.0, 0.006144018614179293, 0.009746530163387238, 0.009279643071907383,
                     0.009730283744099729, 0.010626660038981214, 0.011025932576060344,
                     0.011227011597976013, 0.011792807013234582, 0.012134419941073466}},
    {.name        = "launch lug, default dimensions",
     .length      = 0.03,
     .outerRadius = 0.005,
     .thickness   = 0.001,
     .finish      = Finish::NORMAL,
     .innerArea   = 5.0265482457436686E-5,
     .pressureCD  = {0.0, 0.0, 0.010726562734135721, 0.013809827934027248, 0.01355918926445299,
                     0.014444494888875205, 0.016084538199199153, 0.016808712231924462,
                     0.017175193106976645, 0.018205998903355294, 0.018829696714070566}},
    {.name        = "long thin-walled lug",
     .length      = 0.3,
     .outerRadius = 0.01,
     .thickness   = 5.0E-4,
     .finish      = Finish::NORMAL,
     .innerArea   = 2.835287369864788E-4,
     .pressureCD  = {0.0, 0.0, 0.03649618760864684, 0.08324994067107559, 0.07423715844845738,
                     0.07401531383864554, 0.07536342444848594, 0.07605910110789721,
                     0.07638248622336073, 0.07729797305726066, 0.07783022054590923}},
    {.name        = "rough lug",
     .length      = 0.05,
     .outerRadius = 0.003,
     .thickness   = 0.001,
     .finish      = Finish::ROUGH,
     .innerArea   = 1.2566370614359172E-5,
     .pressureCD  = {0.0, 0.0, 0.006144018596788991, 0.01515119229291435, 0.014939830314291021,
                     0.015428040200516233, 0.016338212641971727, 0.01674035542471113,
                     0.016943816026147942, 0.01751613177309953, 0.017862403224459207}},
    {.name        = "mirror-smooth lug",
     .length      = 0.05,
     .outerRadius = 0.003,
     .thickness   = 0.001,
     .finish      = Finish::MIRROR,
     .innerArea   = 1.2566370614359172E-5,
     .pressureCD  = {0.0, 0.0, 0.006144018616550698, 0.008563449768844638, 0.007523203111022714,
                     0.007770167413378656, 0.008557582828195999, 0.008929857295708478,
                     0.009106985521201148, 0.009597936752324055, 0.009874307759937015}},
    {.name        = "solid lug (inner radius 0)",
     .length      = 0.05,
     .outerRadius = 0.003,
     .thickness   = 0.003,
     .finish      = Finish::NORMAL,
     .innerArea   = 0.0,
     .pressureCD  = {0.0, 0.0, 0.009777599999999997, 0.00978623233875, 0.010090051019999999,
                     0.011048224319999998, 0.012714580619999999, 0.013444199999999996,
                     0.013815191655449626, 0.014858312691358025, 0.015490778624999996}},
    {.name        = "lug with an inner area just below epsilon",
     .length      = 0.05,
     .outerRadius = 0.003,
     .thickness   = 0.00295,
     .finish      = Finish::NORMAL,
     .innerArea   = 7.853981633974524E-9,
     .pressureCD  = {0.0, 0.0, 0.009774883999999998, 0.009783513940878124, 0.010087248228049999,
                     0.011045155368799997, 0.012711048792049998, 0.013440465499999997,
                     0.013811354102212002, 0.014854185382277093, 0.015486475630937498}},
    {.name        = "lug with an inner area just above epsilon",
     .length      = 0.05,
     .outerRadius = 0.003,
     .thickness   = 0.00294,
     .finish      = Finish::NORMAL,
     .innerArea   = 1.1309733552923314E-8,
     .pressureCD  = {0.0, 0.0, 0.009787173403924366, 0.011045464743012498, 0.010963533757910912,
                     0.011882068800320153, 0.013534127912608601, 0.014260669789869866,
                     0.014629214529841691, 0.015665684444692296, 0.016293495825003362}},
    {.name        = "lug of no length",
     .length      = 0.0,
     .outerRadius = 0.003,
     .thickness   = 0.001,
     .finish      = Finish::NORMAL,
     .innerArea   = 1.2566370614359172E-5,
     .pressureCD  = {0.0, 0.0, 0.0054319999999999985, 0.005436795743749999, 0.005605583899999999,
                     0.006137902399999997, 0.007063655899999999, 0.007468999999999998,
                     0.007675106475249792, 0.00825461816186557, 0.008605988124999997}},
    // PINS-END
}};
// NOLINTEND(modernize-use-std-numbers)

/// A launch lug with these dimensions, set as the probe set them.
[[nodiscard]] std::unique_ptr<LaunchLug> makeLug(double length, double outerRadius,
                                                 double thickness, Finish finish)
{
    auto lug = std::make_unique<LaunchLug>();
    lug->setLength(length);
    lug->setOuterRadius(outerRadius);
    lug->setThickness(thickness);
    lug->setFinish(finish);
    return lug;
}

/// Fresh conditions with the reference length kRefLength.
[[nodiscard]] FlightConditions conditionsAt(double mach, double aoaDeg = 0)
{
    FlightConditions conditions;
    conditions.setRefLength(kRefLength);
    conditions.setMach(mach);
    conditions.setAOA(MathUtil::deg2rad(aoaDeg));
    return conditions;
}

/// Compares a value with the one OpenRocket printed: within 1e-12 of the value itself, so a small
/// value is held as strictly as a large one and a zero must be a zero.
/// JavaValueDifferences::number(), which also allows an absolute 1e-15, only decides for NaN (NaN
/// with NaN) and the infinities.
void pin(JavaValueDifferences& differences, std::string_view field, double expected, double actual)
{
    const bool finite = std::isfinite(expected) && std::isfinite(actual);
    const bool within =
        std::abs(actual - expected) <= 1e-12 * std::max(std::abs(expected), std::abs(actual));
    if (finite && !within)
    {
        differences.problem(std::format("{}: expected {}, got {}", field, expected, actual));
    }
    else
    {
        differences.number(field, expected, actual);
    }
}

/// What differs between Java's values and the calculator's for a case; empty when nothing does.
[[nodiscard]] std::string differencesOf(const TubeCase& c)
{
    const std::unique_ptr<LaunchLug> lug = makeLug(c.length, c.outerRadius, c.thickness, c.finish);
    TestTubeCalc                     calc{*lug};
    JavaValueDifferences             differences;
    WarningSet                       warnings;
    if (calc.getInnerArea() != c.innerArea)  // a product: the same double
    {
        differences.problem(
            std::format("inner area: expected {}, got {}", c.innerArea, calc.getInnerArea()));
    }
    for (std::size_t i = 0; i < kMachs.size(); i++)
    {
        const double mach = kMachs.at(i);
        pin(differences, std::format("pressure CD at Mach {}", mach), c.pressureCD.at(i),
            calc.calculatePressureCD(conditionsAt(mach),
                                     BarrowmanDragCalculator::calculateStagnationCD(mach),
                                     BarrowmanDragCalculator::calculateBaseCD(mach), warnings));
    }
    if (!warnings.empty())
    {
        differences.problem("warnings: " + warnings.toString());
    }
    return differences.text();
}

TEST(TubeCalc, MatchesOpenRocketForEveryTube)
{
    for (const TubeCase& c : kTubeCases)
    {
        EXPECT_EQ(differencesOf(c), "") << c.name;
    }
}

TEST(TubeCalc, HasNoPressureDragAtRest)
{
    const std::unique_ptr<LaunchLug> lug = makeLug(0.05, 0.003, 0.001, Finish::NORMAL);
    TestTubeCalc                     calc{*lug};
    WarningSet                       warnings;

    // The limit is the velocity MathUtil::kEpsilon, whatever the coefficients.
    FlightConditions slow = conditionsAt(0.3);
    slow.setVelocity(0.99 * MathUtil::kEpsilon);
    ASSERT_LT(slow.getVelocity(), MathUtil::kEpsilon);
    EXPECT_EQ(calc.calculatePressureCD(slow, 0.9, 0.2, warnings), 0.0);
    EXPECT_EQ(calc.calculatePressureCD(conditionsAt(0.0), 0.9, 0.2, warnings), 0.0);

    FlightConditions moving = conditionsAt(0.3);
    moving.setVelocity(1.01 * MathUtil::kEpsilon);
    ASSERT_GE(moving.getVelocity(), MathUtil::kEpsilon);
    EXPECT_GT(calc.calculatePressureCD(moving, 0.9, 0.2, warnings), 0.0);
    EXPECT_TRUE(warnings.empty());
}

TEST(TubeCalc, SolidTubeHasTheDragOfItsFaceOnly)
{
    // No flow through a tube without an inner area: 0.7 (stagnation + base) on the whole face.
    const std::unique_ptr<LaunchLug> lug = makeLug(0.05, 0.003, 0.003, Finish::NORMAL);
    TestTubeCalc                     calc{*lug};
    WarningSet                       warnings;
    EXPECT_EQ(calc.getInnerArea(), 0.0);

    const FlightConditions conditions  = conditionsAt(0.6, 5);
    const double           frontalArea = std::numbers::pi * MathUtil::pow2(0.003);
    EXPECT_EQ(calc.calculatePressureCD(conditions, 0.9, 0.2, warnings),
              (0.7 * (0.9 + 0.2) * frontalArea) / conditions.getRefArea());
    // The length and the roughness do not matter then.
    const std::unique_ptr<LaunchLug> longLug = makeLug(0.5, 0.003, 0.003, Finish::ROUGH);
    TestTubeCalc                     longCalc{*longLug};
    EXPECT_EQ(longCalc.calculatePressureCD(conditions, 0.9, 0.2, warnings),
              calc.calculatePressureCD(conditions, 0.9, 0.2, warnings));
}

TEST(TubeCalc, CopiesTheTubeWhenItIsMade)
{
    const std::unique_ptr<LaunchLug> lug = makeLug(0.05, 0.003, 0.001, Finish::NORMAL);
    TestTubeCalc                     kept{*lug};
    WarningSet                       warnings;
    const FlightConditions           conditions = conditionsAt(0.6);
    const double before    = kept.calculatePressureCD(conditions, 0.9, 0.2, warnings);
    const double innerArea = kept.getInnerArea();

    lug->setLength(0.2);
    lug->setOuterRadius(0.006);
    lug->setThickness(0.0005);
    lug->setFinish(Finish::ROUGH);
    EXPECT_EQ(kept.calculatePressureCD(conditions, 0.9, 0.2, warnings), before);
    EXPECT_EQ(kept.getInnerArea(), innerArea);

    TestTubeCalc renewed{*lug};
    EXPECT_NE(renewed.calculatePressureCD(conditions, 0.9, 0.2, warnings), before);
    EXPECT_EQ(renewed.getInnerArea(), std::numbers::pi * MathUtil::pow2(lug->getInnerRadius()));
}

TEST(TubeCalc, RoughnessRaisesTheDragOfTheFlowThroughTheTube)
{
    WarningSet             warnings;
    const FlightConditions conditions = conditionsAt(0.6);
    double                 previous   = 0;
    // From the smoothest finish to the roughest.
    for (const Finish finish :
         {Finish::MIRROR, Finish::FINISHPOLISHED, Finish::POLISHED, Finish::OPTIMUM, Finish::SMOOTH,
          Finish::NORMAL, Finish::UNFINISHED, Finish::ROUGHUNFINISHED, Finish::ROUGH})
    {
        const std::unique_ptr<LaunchLug> lug = makeLug(0.05, 0.003, 0.001, finish);
        TestTubeCalc                     calc{*lug};
        const double cd = calc.calculatePressureCD(conditions, 0.9, 0.2, warnings);
        EXPECT_GT(cd, previous);
        previous = cd;
    }
}

TEST(TubeCalc, TubeWithoutAWallThicknessHasNaNDrag)
{
    // A tube fin set that was never given a wall thickness (it gets its body tube's when it is
    // added to one) has a NaN inner radius: the inner area is NaN, which is not above
    // MathUtil::kEpsilon, and the frontal area with it. At rest the drag is still 0.
    TubeFinSet tubes;
    tubes.setOuterRadius(0.01);
    ASSERT_TRUE(std::isnan(tubes.getInnerRadius()));
    TestTubeCalc calc{tubes};
    WarningSet   warnings;
    EXPECT_TRUE(std::isnan(calc.getInnerArea()));
    EXPECT_TRUE(std::isnan(calc.calculatePressureCD(conditionsAt(0.3), 0.9, 0.2, warnings)));
    EXPECT_EQ(calc.calculatePressureCD(conditionsAt(0.0), 0.9, 0.2, warnings), 0.0);
}

}  // namespace
