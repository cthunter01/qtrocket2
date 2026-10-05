#include "QtRocket/aero/barrowman/LaunchLugCalc.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/BarrowmanDragCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/barrowman/RocketComponentCalc.h"
#include "QtRocket/aero/barrowman/TubeCalc.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Transformation.h"
#include "rocket/JavaValueDifferences.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AerodynamicForces;
using QtRocket::BarrowmanDragCalculator;
using QtRocket::Coordinate;
using QtRocket::Finish;
using QtRocket::FlightConditions;
using QtRocket::LaunchLug;
using QtRocket::LaunchLugCalc;
using QtRocket::ModId;
using QtRocket::RocketComponentCalc;
using QtRocket::Transformation;
using QtRocket::TubeCalc;
using QtRocket::WarningSet;
using QtRocket::Test::JavaValueDifferences;
using QtRocket::Test::TestEstesAlphaIII;

/// The Mach numbers of the pins: at rest, at a velocity below and just above
/// MathUtil::kEpsilon, and the usual ones.
constexpr std::array<double, 11> kMachs{0.0, 1.0E-12, 1.0E-10, 0.05, 0.3, 0.6,
                                        0.9, 1.0,     1.1,     1.5,  2.0};

/// OpenRocket's LaunchLugCalc.calculatePressureCD() (JDK 17, the probe TubeProbe.java) for the
/// launch lug of TestRockets.makeEstesAlphaIII() at each of kMachs, in the flight conditions of
/// the rocket's selected configuration, with the stagnation and base drag coefficients of that
/// Mach number.
constexpr std::array<double, 11> kAlphaLugPressureCD{0.0,
                                                     0.0,
                                                     0.026666747457375406,
                                                     0.04230264827859045,
                                                     0.04027622861070913,
                                                     0.04223213430598841,
                                                     0.0461226564191893,
                                                     0.0478556101391508,
                                                     0.048728348949548674,
                                                     0.05118405821716399,
                                                     0.052666753216464704};

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

/// What differs between @p expected and the pressure drag of @p calc over kMachs, in fresh
/// conditions of @p alpha's selected configuration.
[[nodiscard]] std::string pressureDifferences(const TestEstesAlphaIII& alpha, LaunchLugCalc& calc,
                                              const std::array<double, 11>& expected)
{
    JavaValueDifferences differences;
    WarningSet           warnings;
    for (std::size_t i = 0; i < kMachs.size(); i++)
    {
        const double     mach = kMachs.at(i);
        FlightConditions conditions{alpha.rocket->getSelectedConfiguration()};
        conditions.setMach(mach);
        pin(differences, std::format("pressure CD at Mach {}", mach), expected.at(i),
            calc.calculatePressureCD(conditions,
                                     BarrowmanDragCalculator::calculateStagnationCD(mach),
                                     BarrowmanDragCalculator::calculateBaseCD(mach), warnings));
    }
    if (!warnings.empty())
    {
        differences.problem("warnings: " + warnings.toString());
    }
    return differences.text();
}

TEST(LaunchLugCalc, PressureDragOfTheEstesAlphaIIILugIsOpenRockets)
{
    const TestEstesAlphaIII alpha;
    // Java: (LaunchLug) body.getChild(1), 0.05 m long with radii 3 and 2 mm.
    ASSERT_EQ(&alpha.body->getChild(1), alpha.lug);
    LaunchLugCalc calc{*alpha.lug};
    EXPECT_EQ(pressureDifferences(alpha, calc, kAlphaLugPressureCD), "");
}

TEST(LaunchLugCalc, PressureDragIsThatOfOneLug)
{
    // The drag calculator multiplies by the instance count; the calculator does not.
    const TestEstesAlphaIII alpha;
    alpha.lug->setInstanceCount(3);
    alpha.lug->setInstanceSeparation(0.06);
    LaunchLugCalc calc{*alpha.lug};
    EXPECT_EQ(pressureDifferences(alpha, calc, kAlphaLugPressureCD), "");
}

TEST(LaunchLugCalc, AddsNoForcesAndNoFrictionDrag)
{
    const LaunchLug      lug;
    LaunchLugCalc        calc{lug};
    RocketComponentCalc& base = calc;

    FlightConditions conditions;
    conditions.setMach(1.2);
    conditions.setAOA(0.1);
    conditions.setRollRate(3);
    WarningSet        warnings;
    AerodynamicForces forces;
    forces.setCN(0.5);
    forces.setCP(Coordinate{0.3, 0, 0, 2});
    const ModId before = forces.modId();

    base.calculateNonaxialForces(conditions, Transformation::kIdentity, forces, warnings);
    EXPECT_EQ(forces.modId(), before);  // untouched
    EXPECT_EQ(forces.getCN(), 0.5);
    EXPECT_TRUE(std::isnan(forces.getCm()));

    EXPECT_EQ(base.calculateFrictionCD(conditions, 0.004, warnings), 0.0);
    EXPECT_EQ(base.calculateComponentBaseCD(conditions, 0.12, warnings), 0.0);
    EXPECT_GT(base.calculatePressureCD(conditions, 0.85, 0.12, warnings), 0.0);
    EXPECT_TRUE(warnings.empty());
}

TEST(LaunchLugCalc, IsATubeCalcThatCopiesTheLug)
{
    LaunchLug lug;
    lug.setFinish(Finish::SMOOTH);
    LaunchLugCalc    kept{lug};
    TubeCalc&        tubeCalc = kept;
    WarningSet       warnings;
    FlightConditions conditions;
    conditions.setMach(0.6);
    const double before = tubeCalc.calculatePressureCD(conditions, 0.9, 0.2, warnings);

    lug.setLength(0.2);
    lug.setOuterRadius(0.008);
    lug.setFinish(Finish::ROUGH);
    EXPECT_EQ(kept.calculatePressureCD(conditions, 0.9, 0.2, warnings), before);

    LaunchLugCalc renewed{lug};
    EXPECT_NE(renewed.calculatePressureCD(conditions, 0.9, 0.2, warnings), before);
}

}  // namespace
