#include "QtRocket/aero/barrowman/FinSetCalc.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <numbers>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/BarrowmanCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/EllipticalFinSet.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/PolyInterpolator.h"
#include "QtRocket/util/Transformation.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AerodynamicForces;
using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BarrowmanCalculator;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::Coordinate;
using QtRocket::EllipticalFinSet;
using QtRocket::FinSet;
using QtRocket::FinSetCalc;
using QtRocket::FlightConditions;
using QtRocket::FlightConfiguration;
using QtRocket::ForceMap;
using QtRocket::FreeformFinSet;
using QtRocket::MessagePriority;
using QtRocket::NoseCone;
using QtRocket::PolyInterpolator;
using QtRocket::Rocket;
using QtRocket::Transformation;
using QtRocket::Transition;
using QtRocket::TransitionShape;
using QtRocket::TrapezoidFinSet;
using QtRocket::TubeFinSet;
using QtRocket::Warning;
using QtRocket::WarningSet;
using QtRocket::MathUtil::javaToRadians;
using QtRocket::Test::TestEstesAlphaIII;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kPi  = std::numbers::pi;

/// FinSetCalcTest.EPSILON
constexpr double kEpsilon = 0.0001;

// ============================================================ ported from FinSetCalcTest.java
//
// The Java tests take the fin set as rocket.getChild(0).getChild(1).getChild(0) of
// TestRockets.makeEstesAlphaIII(); here it is TestEstesAlphaIII::fins, the same component
// (ThreeFin checks that). FinSetCalcTest.TestableFinSetCalc, a subclass that exposes the
// protected calculateFinCNa1(), calculateCPPos() and the MAC fields, is not needed: they are
// public in the C++ class.

/// FinSetCalcTest.sumFins(fins, conditions): the forces of the fins of @p fins summed as the
/// Java test sums them, fin i turned by pi * i / fin count.
[[nodiscard]] AerodynamicForces sumFins(const FinSet& fins, const FlightConditions& conditions)
{
    WarningSet        warnings;
    AerodynamicForces assemblyForces = AerodynamicForces{}.zero();
    AerodynamicForces componentForces;

    FinSetCalc calcObj{fins};

    // Need to sum forces for fins
    for (int i = 0; i < fins.getFinCount(); i++)
    {
        calcObj.calculateNonaxialForces(conditions,
                                        Transformation::rotateX(kPi * i / fins.getFinCount()),
                                        componentForces, warnings);
        assemblyForces.merge(componentForces);
    }

    return assemblyForces;
}

/// FinSetCalcTest.sumFins(fins, rocket, mach) and, with the NaN default, sumFins(fins, rocket):
/// the conditions of the selected configuration, with @p mach unless it is NaN.
[[nodiscard]] AerodynamicForces sumFins(const FinSet& fins, const Rocket& rocket,
                                        double mach = kNaN)
{
    FlightConditions conditions{rocket.getSelectedConfiguration()};
    if (!std::isnan(mach))
    {
        conditions.setMach(mach);
    }
    return sumFins(fins, conditions);
}

/// FinSetCalcTest.isolatedFinCP(): the isolated-fin CP for tests of the underlying fin formula
/// (TestableFinSetCalc.calculateIsolatedCP(): the CP before fin-body interference is applied).
[[nodiscard]] double isolatedFinCP(const FinSet& fins, const Rocket& rocket, double mach)
{
    FlightConditions conditions{rocket.getSelectedConfiguration()};
    conditions.setMach(mach);
    const FinSetCalc calc{fins};
    return calc.getMACLead() + (calc.calculateCPPos(conditions) * calc.getMACLength());
}

/// TestableFinSetCalc.calculateUncorrectedRollForcing().
[[nodiscard]] double calculateUncorrectedRollForcing(const FinSetCalc&       calculator,
                                                     const FlightConditions& conditions,
                                                     const FinSet&           fins)
{
    return (calculator.getMACSpan() + fins.getBodyRadius()) *
           calculator.calculateFinCNa1(conditions) * fins.getCantAngle() /
           conditions.getRefLength();
}

/// Whether @p warnings holds a warning with the text of @p warning, whatever its sources
/// (testZeroAreaFin's stream().anyMatch() on getMessageDescription()).
[[nodiscard]] bool hasWarningText(const WarningSet& warnings, const Warning& warning)
{
    const std::string text = warning.messageDescription();
    return std::ranges::any_of(
        warnings, [&text](const Warning& w) { return w.messageDescription() == text; });
}

// FinSetCalcTest.testCantDrivenRollUsesWingIncidenceFactor
/// Cant is a wing-incidence case and must use equation 19's lowercase interference factor rather
/// than equation 14's angle-of-attack factor.
TEST(FinSetCalc, CantDrivenRollUsesWingIncidenceFactor)
{
    const TestEstesAlphaIII alpha;
    TrapezoidFinSet&        fins = *alpha.fins;
    fins.setCantAngle(0.05);
    FlightConditions conditions{alpha.rocket->getSelectedConfiguration()};
    conditions.setMach(2.0);
    conditions.setAOA(0.0);

    const AerodynamicForces forces = sumFins(fins, conditions);
    const FinSetCalc        calculator{fins};
    const double            uncorrectedRollForcing =
        fins.getFinCount() * calculateUncorrectedRollForcing(calculator, conditions, fins);

    EXPECT_NEAR(0.94, forces.getCrollForce() / uncorrectedRollForcing, 0.01);
}

// FinSetCalcTest.testRectangularSupersonicCantDrivenRollUsesChartThree
/// A constant-chord fin above the report's beta*A=2 selection boundary must use chart 3 in the
/// actual force calculation, not only in the chart helper.
TEST(FinSetCalc, RectangularSupersonicCantDrivenRollUsesChartThree)
{
    const TestEstesAlphaIII alpha;
    TrapezoidFinSet&        fins = *alpha.fins;
    fins.setTipChord(fins.getRootChord());
    fins.setSweep(0.0);
    fins.setHeight(4.0 * fins.getBodyRadius());
    fins.setCantAngle(0.05);
    const double     aspectRatio = 2.0 * std::pow(fins.getSpan(), 2) / fins.getPlanformArea();
    const double     beta        = 2.25 / aspectRatio;
    FlightConditions conditions{alpha.rocket->getSelectedConfiguration()};
    conditions.setMach(std::sqrt(1.0 + (beta * beta)));
    conditions.setAOA(0.0);

    const AerodynamicForces forces = sumFins(fins, conditions);
    const FinSetCalc        calculator{fins};
    const double            uncorrectedRollForcing =
        fins.getFinCount() * calculateUncorrectedRollForcing(calculator, conditions, fins);

    EXPECT_NEAR(0.884, forces.getCrollForce() / uncorrectedRollForcing, 0.006);
}

// FinSetCalcTest.testFreeformCantDrivenRollUsesWingIncidenceFactor
/// Equation 19 depends only on radius/semispan, so unsupported planforms must not jump back to
/// the historical 1 + tau roll multiplier.
TEST(FinSetCalc, FreeformCantDrivenRollUsesWingIncidenceFactor)
{
    const TestEstesAlphaIII                alpha;
    TrapezoidFinSet&                       trapezoidFins = *alpha.fins;
    const FreeformFinSet::ConversionResult converted = FreeformFinSet::convertFinSet(trapezoidFins);
    ASSERT_NE(converted.freeform, nullptr);
    FreeformFinSet& fins = *converted.freeform;
    fins.setCantAngle(0.05);
    FlightConditions conditions{alpha.rocket->getSelectedConfiguration()};
    conditions.setMach(2.0);
    conditions.setAOA(0.0);

    const AerodynamicForces forces = sumFins(fins, conditions);
    const FinSetCalc        calculator{fins};
    const double            uncorrectedRollForcing =
        fins.getFinCount() * calculateUncorrectedRollForcing(calculator, conditions, fins);

    EXPECT_NEAR(0.94, forces.getCrollForce() / uncorrectedRollForcing, 0.01);
}

// FinSetCalcTest.testCantDrivenRollDoesNotIncreaseBeforeStall
/// The small-angle body-load model is faired out with angle of attack, but fin incidence is still
/// governed by equation 19 until the existing post-stall roll reduction begins.
TEST(FinSetCalc, CantDrivenRollDoesNotIncreaseBeforeStall)
{
    const TestEstesAlphaIII alpha;
    TrapezoidFinSet&        fins = *alpha.fins;
    fins.setCantAngle(0.05);
    FlightConditions conditions{alpha.rocket->getSelectedConfiguration()};
    conditions.setMach(2.0);
    conditions.setAOA(javaToRadians(15.0));

    const AerodynamicForces forces = sumFins(fins, conditions);
    const FinSetCalc        calculator{fins};
    const double            uncorrectedRollForcing =
        fins.getFinCount() * calculateUncorrectedRollForcing(calculator, conditions, fins);

    EXPECT_NEAR(0.94, forces.getCrollForce() / uncorrectedRollForcing, 0.01);
}

// FinSetCalcTest.testTransonicCNaUsesSubsonicEndpointDerivative
/// Verify that the transonic interpolation uses the derivative of the subsonic model at Mach 0.9,
/// independent of the Mach being queried.
TEST(FinSetCalc, TransonicCNaUsesSubsonicEndpointDerivative)
{
    const double subsonicMach   = 0.9;
    const double supersonicMach = 1.5;
    const double queryMach      = 1.2;
    const double derivativeStep = 0.000001;

    const TestEstesAlphaIII alpha;
    const TrapezoidFinSet&  fins = *alpha.fins;
    FlightConditions        conditions{alpha.rocket->getSelectedConfiguration()};
    conditions.setAOA(0);
    const FinSetCalc calculator{fins};

    conditions.setMach(subsonicMach);
    const double subsonicValue = calculator.calculateFinCNa1(conditions);
    conditions.setMach(subsonicMach - derivativeStep);
    const double belowSubsonicValue = calculator.calculateFinCNa1(conditions);
    const double subsonicDerivative = (subsonicValue - belowSubsonicValue) / derivativeStep;

    conditions.setMach(supersonicMach);
    const double supersonicValue      = calculator.calculateFinCNa1(conditions);
    const double supersonicBetaCubed  = std::pow((supersonicMach * supersonicMach) - 1, 1.5);
    const double supersonicDerivative = -fins.getPlanformArea() / conditions.getRefArea() * 2 *
                                        supersonicMach / supersonicBetaCubed;

    const PolyInterpolator interpolator{
        {subsonicMach, supersonicMach}, {subsonicMach, supersonicMach}, {subsonicMach}};
    const double expected = interpolator.interpolate(
        queryMach, {subsonicValue, supersonicValue, subsonicDerivative, supersonicDerivative, 0});

    conditions.setMach(queryMach);
    const double actual = calculator.calculateFinCNa1(conditions);
    EXPECT_NEAR(expected, actual, kEpsilon)
        << "Transonic CNa should use the derivative at the Mach 0.9 endpoint";
}

// FinSetCalcTest.test3Fin
TEST(FinSetCalc, ThreeFin)
{
    const TestEstesAlphaIII alpha;
    const TrapezoidFinSet&  fins = *alpha.fins;

    // The component the Java tests reach as rocket.getChild(0).getChild(1).getChild(0)
    EXPECT_EQ(&alpha.rocket->getChild(0).getChild(1).getChild(0), alpha.fins);

    // to make the fin properties explicit
    EXPECT_NEAR(3, fins.getFinCount(), kEpsilon) << " Estes Alpha III fins have wrong count:";
    EXPECT_NEAR(0.05, fins.getRootChord(), kEpsilon)
        << " Estes Alpha III fins have wrong root chord:";
    EXPECT_NEAR(0.03, fins.getTipChord(), kEpsilon)
        << " Estes Alpha III fins have wrong tip chord:";
    EXPECT_NEAR(0.02, fins.getSweep(), kEpsilon) << " Estes Alpha III fins have wrong sweep: ";
    EXPECT_NEAR(0.05, fins.getHeight(), kEpsilon) << " Estes Alpha III fins have wrong height: ";

    // get the forces for the three fins
    const AerodynamicForces forces = sumFins(fins, *alpha.rocket);

    const double expCnaFins = 28.82053382;
    const double expCpxFins = 0.018588118711734096;

    EXPECT_NEAR(expCnaFins, forces.getCP().weight, kEpsilon) << " FinSetCalc produces bad CNa: ";
    EXPECT_NEAR(expCpxFins, forces.getCP().x, kEpsilon) << " FinSetCalc produces bad C_p.x: ";
    EXPECT_NEAR(0.0, forces.getCN(), kEpsilon) << " FinSetCalc produces bad CN: ";
    EXPECT_NEAR(0.0, forces.getCm(), kEpsilon) << " FinSetCalc produces bad C_m: ";
}

// FinSetCalcTest.test4Fin
TEST(FinSetCalc, FourFin)
{
    const TestEstesAlphaIII alpha;
    TrapezoidFinSet&        fins = *alpha.fins;
    fins.setFinCount(4);

    // to make the fin properties explicit
    EXPECT_NEAR(4, fins.getFinCount(), kEpsilon) << " Estes Alpha III fins have wrong count:";
    EXPECT_NEAR(0.05, fins.getRootChord(), kEpsilon)
        << " Estes Alpha III fins have wrong root chord:";
    EXPECT_NEAR(0.03, fins.getTipChord(), kEpsilon)
        << " Estes Alpha III fins have wrong tip chord:";
    EXPECT_NEAR(0.02, fins.getSweep(), kEpsilon) << " Estes Alpha III fins have wrong sweep: ";
    EXPECT_NEAR(0.05, fins.getHeight(), kEpsilon) << " Estes Alpha III fins have wrong height: ";

    // get the forces for the four fins
    const AerodynamicForces forces = sumFins(fins, *alpha.rocket);

    const double expCnaFins = 38.42737843;
    const double expCpxFins = 0.0185881187117341;

    EXPECT_NEAR(expCnaFins, forces.getCP().weight, kEpsilon) << " FinSetCalc produces bad CNa: ";
    EXPECT_NEAR(expCpxFins, forces.getCP().x, kEpsilon) << " FinSetCalc produces bad C_p.x: ";
    EXPECT_NEAR(0.0, forces.getCN(), kEpsilon) << " FinSetCalc produces bad CN: ";
    EXPECT_NEAR(0.0, forces.getCm(), kEpsilon) << " FinSetCalc produces bad C_m: ";
}

/// One step of testLowAspectRatioFinSupersonicCP's loop: @p cpx at @p mach after @p previous
/// (NaN at the first step) for the root chord @p rootChord.
void expectLowAspectRatioCpStep(double cpx, double previous, double mach, double rootChord)
{
    EXPECT_TRUE(std::isfinite(cpx)) << "CP x should stay finite at mach " << mach;

    // A 0.01 step in mach may not move the CP by anything like a percent of the
    // root chord.  Clamping only the result of the empirical formula would leave
    // a quarter-MAC step across its pole, which this catches.
    if (!std::isnan(previous))
    {
        EXPECT_LT(std::abs(cpx - previous), 0.01 * rootChord)
            << "CP x jumped from " << previous << " to " << cpx << " at mach " << mach;
        EXPECT_GE(cpx, previous - 1.0e-12) << "CP should not move forward at mach " << mach;
    }
}

/// testLowAspectRatioFinSupersonicCP's loop over the Mach numbers 0.5, 0.51, ... 5.5, the Mach
/// number accumulated as Java accumulates it (a while loop: clang-analyzer's
/// security.FloatLoopCounter rejects a double as the counter of a for loop).
void expectLowAspectRatioCpSteady(const TrapezoidFinSet& fins, const Rocket& rocket)
{
    double previous = kNaN;
    double mach     = 0.5;
    while (mach <= 5.5)
    {
        const double cpx = isolatedFinCP(fins, rocket, mach);
        expectLowAspectRatioCpStep(cpx, previous, mach, fins.getRootChord());
        previous = cpx;
        mach += 0.01;
    }
}

// FinSetCalcTest.testLowAspectRatioFinSupersonicCP
TEST(FinSetCalc, LowAspectRatioFinSupersonicCP)
{
    const TestEstesAlphaIII alpha;
    TrapezoidFinSet&        fins   = *alpha.fins;
    const Rocket&           rocket = *alpha.rocket;

    // Aspect ratio 4*h/(cr+ct) = 0.2, which puts the pole of the empirical
    // supersonic CP formula (ar*beta == 0.5) at about mach 2.69
    fins.setHeight(0.005);
    fins.setRootChord(0.06);
    fins.setTipChord(0.04);
    fins.setSweep(0.0);

    expectLowAspectRatioCpSteady(fins, rocket);

    const double quarterChordCP = isolatedFinCP(fins, rocket, 0.5);
    EXPECT_NEAR(quarterChordCP, isolatedFinCP(fins, rocket, 4.0), kEpsilon)
        << "The low-aspect-ratio fallback should avoid the invalid source branch";
    EXPECT_GT(isolatedFinCP(fins, rocket, 5.2), quarterChordCP)
        << "The CP should join the source curve after ar*beta exceeds one";
}

/// testOrdinaryFinSupersonicCPUsesSourceEquation's loop over the Mach numbers 2.0, 2.1, ... 4.0,
/// accumulated as in Java.
void expectOrdinaryCpIsSourceEquation(const TrapezoidFinSet& fins, const Rocket& rocket)
{
    double previous = -kInf;
    double mach     = 2.0;
    while (mach <= 4.0)
    {
        const double cpx                = isolatedFinCP(fins, rocket, mach);
        const double arBeta             = 2.5 * std::sqrt((mach * mach) - 1);
        const double expectedRelativeCP = (arBeta - 0.67) / ((2 * arBeta) - 1);

        EXPECT_NEAR(expectedRelativeCP * fins.getRootChord(), cpx, 1.0e-10)
            << "Ordinary fins should retain the source equation at mach " << mach;
        EXPECT_GT(cpx, previous) << "CP should keep moving aft at mach " << mach;
        previous = cpx;
        mach += 0.1;
    }
}

// FinSetCalcTest.testOrdinaryFinSupersonicCPUsesSourceEquation
TEST(FinSetCalc, OrdinaryFinSupersonicCPUsesSourceEquation)
{
    const TestEstesAlphaIII alpha;
    TrapezoidFinSet&        fins = *alpha.fins;

    // Use an unswept rectangular fin with AR = 2*h/chord = 2.5. Its CP x is
    // therefore the relative source-equation position multiplied by the chord.
    fins.setHeight(0.0625);
    fins.setRootChord(0.05);
    fins.setTipChord(0.05);
    fins.setSweep(0.0);

    expectOrdinaryCpIsSourceEquation(fins, *alpha.rocket);
}

/// testIntermediateAspectRatioTransonicCPIsShapePreserving's loop over the Mach numbers 0.51,
/// 0.52, ... below 2.0, accumulated as in Java, starting from the CP @p subsonicCP at Mach 0.5.
void expectTransonicCpMonotone(const TrapezoidFinSet& fins, const Rocket& rocket, double subsonicCP)
{
    double previous = subsonicCP;
    double mach     = 0.51;
    while (mach < 2.0)
    {
        const double cpx = isolatedFinCP(fins, rocket, mach);
        EXPECT_GE(cpx, previous - 1.0e-12) << "The transonic CP moved forward at mach " << mach;
        previous = cpx;
        mach += 0.01;
    }
}

// FinSetCalcTest.testIntermediateAspectRatioTransonicCPIsShapePreserving
/// Verify that the low-aspect-ratio transonic continuation moves smoothly between its endpoint
/// positions instead of requiring an output clamp.
TEST(FinSetCalc, IntermediateAspectRatioTransonicCPIsShapePreserving)
{
    const TestEstesAlphaIII alpha;
    TrapezoidFinSet&        fins   = *alpha.fins;
    const Rocket&           rocket = *alpha.rocket;

    // AR = 4*h/(cr+ct) = 0.6. The original fifth-order interpolation develops
    // a forward excursion for this geometry.
    fins.setHeight(0.015);
    fins.setRootChord(0.05);
    fins.setTipChord(0.05);
    fins.setSweep(0.0);

    const double subsonicCP   = isolatedFinCP(fins, rocket, 0.5);
    const double transonicCP  = isolatedFinCP(fins, rocket, 1.5);
    const double supersonicCP = isolatedFinCP(fins, rocket, 2.0);

    EXPECT_GT(transonicCP, subsonicCP)
        << "The transonic CP should leave the quarter chord without a flat output clamp";
    EXPECT_LT(transonicCP, supersonicCP)
        << "The transonic CP should remain between its endpoint positions";

    expectTransonicCpMonotone(fins, rocket, subsonicCP);

    // The shape-preserving curve matches both the value and first derivative of
    // the supersonic curve at Mach 2.
    const double step       = 1.0e-4;
    const double leftSlope  = (supersonicCP - isolatedFinCP(fins, rocket, 2.0 - step)) / step;
    const double rightSlope = (isolatedFinCP(fins, rocket, 2.0 + step) - supersonicCP) / step;
    EXPECT_NEAR(leftSlope, rightSlope, 1.0e-5) << "The CP slope should be continuous at Mach 2";
}

/// One step of testTransonicCPIsContinuousAcrossAspectRatios' loop: @p cpx at @p aspectRatio
/// after @p previous (NaN at the first step) for the root chord @p rootChord.
void expectCpContinuousStep(double cpx, double previous, double aspectRatio, double rootChord)
{
    EXPECT_TRUE(std::isfinite(cpx)) << "CP should be finite at AR " << aspectRatio;
    if (!std::isnan(previous))
    {
        EXPECT_GE(cpx, previous - 1.0e-12)
            << "CP should vary monotonically with AR " << aspectRatio;
        EXPECT_LT(cpx - previous, 0.01 * rootChord) << "CP should not jump at AR " << aspectRatio;
    }
}

// FinSetCalcTest.testTransonicCPIsContinuousAcrossAspectRatios
/// Verify continuity through the fallback, bridge, and interpolation boundaries.
TEST(FinSetCalc, TransonicCPIsContinuousAcrossAspectRatios)
{
    const TestEstesAlphaIII alpha;
    TrapezoidFinSet&        fins = *alpha.fins;
    fins.setRootChord(0.05);
    fins.setTipChord(0.05);
    fins.setSweep(0.0);

    // The aspect ratios 0.1, 0.102, ... 1.0, accumulated as in Java
    double previous    = kNaN;
    double aspectRatio = 0.1;
    while (aspectRatio <= 1.0)
    {
        // For an unswept rectangular fin, AR = 2*height/chord.
        fins.setHeight(aspectRatio * fins.getRootChord() / 2);
        const double cpx = isolatedFinCP(fins, *alpha.rocket, 1.5);

        expectCpContinuousStep(cpx, previous, aspectRatio, fins.getRootChord());
        previous = cpx;
        aspectRatio += 0.002;
    }
}

// FinSetCalcTest.testZeroAreaFin
TEST(FinSetCalc, ZeroAreaFin)
{
    const TestEstesAlphaIII alpha;
    TrapezoidFinSet&        fins = *alpha.fins;

    // Set fin dimensions to zero
    fins.setHeight(0.0);

    EXPECT_NEAR(0.0, fins.getPlanformArea(), kEpsilon)
        << "Zero-area fin should have zero planform area";

    // The user should be told about it
    WarningSet        warnings;
    AerodynamicForces ignored;
    FinSetCalc{fins}.calculateNonaxialForces(
        FlightConditions{alpha.rocket->getSelectedConfiguration()}, Transformation::kIdentity,
        ignored, warnings);
    EXPECT_TRUE(hasWarningText(warnings, Warning::kZeroAreaFin))
        << "Zero-area fin should raise a warning";

    // Calculate forces
    AerodynamicForces forces = sumFins(fins, *alpha.rocket);

    // Verify all force components are zero and not NaN
    EXPECT_NEAR(0.0, forces.getCP().weight, kEpsilon) << "CNa should be zero for zero-area fin";
    EXPECT_NEAR(0.0, forces.getCN(), kEpsilon) << "CN should be zero for zero-area fin";
    EXPECT_NEAR(0.0, forces.getCm(), kEpsilon) << "Cm should be zero for zero-area fin";
    EXPECT_NEAR(0.0, forces.getCroll(), kEpsilon) << "Croll should be zero for zero-area fin";
    EXPECT_NEAR(0.0, forces.getCrollDamp(), kEpsilon)
        << "CrollDamp should be zero for zero-area fin";
    EXPECT_NEAR(0.0, forces.getCrollForce(), kEpsilon)
        << "CrollForce should be zero for zero-area fin";
    EXPECT_NEAR(0.0, forces.getCside(), kEpsilon) << "Cside should be zero for zero-area fin";
    EXPECT_NEAR(0.0, forces.getCyaw(), kEpsilon) << "Cyaw should be zero for zero-area fin";

    // Check the same for a canted fin
    fins.setCantAngle(0.1);

    // Calculate forces
    forces = sumFins(fins, *alpha.rocket);

    // Verify all force components are zero and not NaN
    EXPECT_NEAR(0.0, forces.getCP().weight, kEpsilon)
        << "CNa should be zero for canted zero-area fin";
    EXPECT_NEAR(0.0, forces.getCN(), kEpsilon) << "CN should be zero for canted zero-area fin";
    EXPECT_NEAR(0.0, forces.getCm(), kEpsilon) << "Cm should be zero for canted zero-area fin";
    EXPECT_NEAR(0.0, forces.getCroll(), kEpsilon)
        << "Croll should be zero for canted zero-area fin";
    EXPECT_NEAR(0.0, forces.getCrollDamp(), kEpsilon)
        << "CrollDamp should be zero for canted zero-area fin";
    EXPECT_NEAR(0.0, forces.getCrollForce(), kEpsilon)
        << "CrollForce should be zero for canted zero-area fin";
    EXPECT_NEAR(0.0, forces.getCside(), kEpsilon)
        << "Cside should be zero for canted zero-area fin";
    EXPECT_NEAR(0.0, forces.getCyaw(), kEpsilon) << "Cyaw should be zero for canted zero-area fin";
}

// FinSetCalcTest.testVerySmallArea
TEST(FinSetCalc, VerySmallArea)
{
    const TestEstesAlphaIII alpha;
    TrapezoidFinSet&        fins = *alpha.fins;

    // Set fin dimensions to very small values (less than 0.0025m)
    const double tinyDimension = 0.0001;  // 0.1 mm
    fins.setHeight(tinyDimension);

    // Calculate forces
    const AerodynamicForces forces = sumFins(fins, *alpha.rocket);

    // Verify results are not NaN
    EXPECT_FALSE(std::isnan(forces.getCP().weight)) << "CNa should not be NaN for very small fin";
    EXPECT_FALSE(std::isnan(forces.getCN())) << "CN should not be NaN for very small fin";
    EXPECT_FALSE(std::isnan(forces.getCm())) << "Cm should not be NaN for very small fin";
    EXPECT_FALSE(std::isnan(forces.getCroll())) << "Croll should not be NaN for very small fin";
    EXPECT_FALSE(std::isnan(forces.getCrollDamp()))
        << "CrollDamp should not be NaN for very small fin";
    EXPECT_FALSE(std::isnan(forces.getCrollForce()))
        << "CrollForce should not be NaN for very small fin";
    EXPECT_FALSE(std::isnan(forces.getCside())) << "Cside should not be NaN for very small fin";
    EXPECT_FALSE(std::isnan(forces.getCyaw())) << "Cyaw should not be NaN for very small fin";

    // Verify CP location is valid
    EXPECT_FALSE(std::isnan(forces.getCP().x))
        << "CP x-coordinate should not be NaN for very small fin";
    EXPECT_FALSE(std::isnan(forces.getCP().y))
        << "CP y-coordinate should not be NaN for very small fin";
    EXPECT_FALSE(std::isnan(forces.getCP().z))
        << "CP z-coordinate should not be NaN for very small fin";
}

// FinSetCalcTest.testSquareFinPressureAndBaseDragSeparation
/// Test that pressure and base drag are calculated separately for square cross-section fins.
/// Square fins should have both pressure drag (stagnation) and base drag. The sum of the two
/// should equal what the old combined method would have returned.
TEST(FinSetCalc, SquareFinPressureAndBaseDragSeparation)
{
    const TestEstesAlphaIII alpha;
    const TrapezoidFinSet&  fins = *alpha.fins;
    EXPECT_EQ(FinSet::CrossSection::SQUARE, fins.getCrossSection())
        << "Default cross-section should be SQUARE";

    FlightConditions conditions{alpha.rocket->getSelectedConfiguration()};
    conditions.setMach(0.3);
    WarningSet warnings;

    FinSetCalc calc{fins};

    const double stagnationCD = 1.0;
    const double baseCD       = 0.5;

    const double pressureCD = calc.calculatePressureCD(conditions, stagnationCD, baseCD, warnings);
    const double componentBaseCD = calc.calculateComponentBaseCD(conditions, baseCD, warnings);

    // Square fins: both pressure and base drag should be positive
    EXPECT_GT(pressureCD, 0) << "Square fin pressure CD should be positive";
    EXPECT_GT(componentBaseCD, 0) << "Square fin base CD should be positive";

    // Base drag should scale with baseCD (doubling baseCD should double component base drag)
    const double componentBaseCD2 = calc.calculateComponentBaseCD(conditions, baseCD * 2, warnings);
    EXPECT_NEAR(componentBaseCD * 2, componentBaseCD2, kEpsilon)
        << "Square fin base CD should scale linearly with baseCD";

    // Pressure CD should not change when baseCD changes (it depends on stagnationCD)
    const double pressureCD2 =
        calc.calculatePressureCD(conditions, stagnationCD, baseCD * 2, warnings);
    EXPECT_NEAR(pressureCD, pressureCD2, kEpsilon)
        << "Square fin pressure CD should not depend on baseCD";
}

// FinSetCalcTest.testRoundedFinPressureAndBaseDragSeparation
/// Test that rounded cross-section fins get half the base drag.
TEST(FinSetCalc, RoundedFinPressureAndBaseDragSeparation)
{
    const TestEstesAlphaIII alpha;
    TrapezoidFinSet&        fins = *alpha.fins;
    fins.setCrossSection(FinSet::CrossSection::ROUNDED);

    FlightConditions conditions{alpha.rocket->getSelectedConfiguration()};
    conditions.setMach(0.3);
    WarningSet warnings;

    FinSetCalc calc{fins};

    const double stagnationCD = 1.0;
    const double baseCD       = 0.5;

    const double pressureCD = calc.calculatePressureCD(conditions, stagnationCD, baseCD, warnings);
    const double componentBaseCD = calc.calculateComponentBaseCD(conditions, baseCD, warnings);

    EXPECT_GT(pressureCD, 0) << "Rounded fin pressure CD should be positive";
    EXPECT_GT(componentBaseCD, 0) << "Rounded fin base CD should be positive";

    // Rounded fins get half the base drag
    const double refArea      = conditions.getRefArea();
    const double span         = fins.getSpan();
    const double thickness    = fins.getThickness();
    const double scaleFactor  = span * thickness / refArea;
    const double expectedBase = (baseCD / 2) * scaleFactor;

    EXPECT_NEAR(expectedBase, componentBaseCD, kEpsilon)
        << "Rounded fin base CD should be half of baseCD * scaleFactor";
}

// FinSetCalcTest.testAirfoilFinZeroBaseDrag
/// Test that airfoil cross-section fins have zero base drag.
TEST(FinSetCalc, AirfoilFinZeroBaseDrag)
{
    const TestEstesAlphaIII alpha;
    TrapezoidFinSet&        fins = *alpha.fins;
    fins.setCrossSection(FinSet::CrossSection::AIRFOIL);

    FlightConditions conditions{alpha.rocket->getSelectedConfiguration()};
    conditions.setMach(0.3);
    WarningSet warnings;

    FinSetCalc calc{fins};

    const double componentBaseCD = calc.calculateComponentBaseCD(conditions, 0.5, warnings);
    EXPECT_NEAR(0.0, componentBaseCD, kEpsilon) << "Airfoil fin should have zero base drag";
}

// FinSetCalcTest.testZeroAreaFinDragSeparation
/// Test that zero-area fins return zero for both pressure and base drag.
TEST(FinSetCalc, ZeroAreaFinDragSeparation)
{
    const TestEstesAlphaIII alpha;
    TrapezoidFinSet&        fins = *alpha.fins;
    fins.setHeight(0.0);

    const FlightConditions conditions{alpha.rocket->getSelectedConfiguration()};
    WarningSet             warnings;

    FinSetCalc calc{fins};

    EXPECT_NEAR(0.0, calc.calculatePressureCD(conditions, 1.0, 0.5, warnings), kEpsilon)
        << "Zero-area fin pressure CD should be zero";
    EXPECT_NEAR(0.0, calc.calculateComponentBaseCD(conditions, 0.5, warnings), kEpsilon)
        << "Zero-area fin base CD should be zero";
}

/// The forces of the first fin set in @p forceMap, or nullptr (the loop of the two force
/// analysis tests: `if (entry.getKey() instanceof FinSet) { finForces = entry.getValue();
/// break; }`).
[[nodiscard]] const AerodynamicForces* finSetForces(const ForceMap& forceMap)
{
    for (const auto& [component, forces] : forceMap)
    {
        if (dynamic_cast<const FinSet*>(component) != nullptr)
        {
            return &forces;
        }
    }
    return nullptr;
}

// FinSetCalcTest.testForceAnalysisFinDragSeparation
/// Integration test: verify that getForceAnalysis reports separate pressure and base drag for
/// fins. The sum of pressureCD + baseCD for the fin should equal the total fin drag minus
/// friction.
TEST(FinSetCalc, ForceAnalysisFinDragSeparation)
{
    const TestEstesAlphaIII    alpha;
    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    FlightConditions           conditions{config};
    conditions.setMach(0.3);
    WarningSet warnings;

    BarrowmanCalculator calculator;
    const ForceMap      forceMap = calculator.getForceAnalysis(config, conditions, &warnings);

    // Find the fin set in the results
    const AerodynamicForces* const finForces = finSetForces(forceMap);

    ASSERT_NE(finForces, nullptr) << "Fin set should be present in force analysis";

    // Verify that both pressure and base CD are reported (not NaN)
    EXPECT_FALSE(std::isnan(finForces->getPressureCD())) << "Fin pressure CD should not be NaN";
    EXPECT_FALSE(std::isnan(finForces->getBaseCD())) << "Fin base CD should not be NaN";
    EXPECT_FALSE(std::isnan(finForces->getFrictionCD())) << "Fin friction CD should not be NaN";

    // For square fins, base drag should be positive
    EXPECT_TRUE(finForces->getBaseCD() > 0)
        << "Square fin base CD should be positive in force analysis";
    EXPECT_TRUE(finForces->getPressureCD() > 0)
        << "Square fin pressure CD should be positive in force analysis";

    // Total CD should equal sum of components
    const double expectedCD =
        finForces->getPressureCD() + finForces->getBaseCD() + finForces->getFrictionCD();
    EXPECT_NEAR(expectedCD, finForces->getCD(), kEpsilon)
        << "Total CD should equal pressureCD + baseCD + frictionCD";
}

// FinSetCalcTest.testForceAnalysisAirfoilFinZeroBaseDrag
/// Integration test: verify that airfoil fins report zero base drag in force analysis.
TEST(FinSetCalc, ForceAnalysisAirfoilFinZeroBaseDrag)
{
    const TestEstesAlphaIII alpha;
    TrapezoidFinSet&        fins = *alpha.fins;
    fins.setCrossSection(FinSet::CrossSection::AIRFOIL);

    const FlightConfiguration& config = alpha.rocket->getSelectedConfiguration();
    FlightConditions           conditions{config};
    conditions.setMach(0.3);
    WarningSet warnings;

    BarrowmanCalculator calculator;
    const ForceMap      forceMap = calculator.getForceAnalysis(config, conditions, &warnings);

    const AerodynamicForces* const finForces = finSetForces(forceMap);

    ASSERT_NE(finForces, nullptr) << "Fin set should be present in force analysis";
    EXPECT_NEAR(0.0, finForces->getBaseCD(), kEpsilon)
        << "Airfoil fin base CD should be zero in force analysis";
    EXPECT_TRUE(finForces->getPressureCD() > 0) << "Airfoil fin pressure CD should be positive";
}

// ============================================================ beyond the JUnit tests
//
// Values pinned with OpenRocket's FinSetCalc on JDK 17 (the probe ProbeFinCalc, which builds the
// same rockets call for call and prints the tables below). The whole-rocket goldens hold only
// trapezoidal fin sets and one freeform set, so the other paths are pinned here.

/// The reference length of the pinned flight conditions (ProbeFinCalc.REF_LENGTH); the reference
/// area follows from it.
constexpr double kRefLength = 0.05;

/// One row of flight conditions: the Mach number, the angle of attack in degrees, the direction
/// theta of the lateral airflow, the rotation of the fin's instance about the x axis, and the
/// roll rate.
using Condition = std::array<double, 5>;

/// The angles of the conditions, as ProbeFinCalc computes them (PI4 and R3; PI2 is
/// ProbeFinCalcFix's): the same doubles.
constexpr double kQuarterPi   = kPi / 4;
constexpr double kHalfPi      = kPi / 2;
constexpr double kTwoThirdsPi = 2 * kPi / 3;

/// The forces Java gives for one row of conditions: the CP's x and its weight CNa, CN, Cm, Croll,
/// CrollDamp and CrollForce. (Cside, Cyaw and the CP's y and z are 0 in every row, the latter NaN
/// where CNa is NaN: the probe checks that, and expectForceRow() expects it.)
using ForcePins = std::array<double, 7>;

/// The geometry Java derives at construction: the fin area, span, aspect ratio, MAC length, MAC
/// leading edge, MAC spanwise position, cosines of the midchord and leading-edge sweep, roll
/// damping sum and midchord position. (The number of interfering fins and whether Java made a
/// NACA 1307 model are in the comment above each table, and asserted by the test.)
using GeometryPins = std::array<double, 10>;

/// The Mach numbers of the pinned drag (ProbeFinCalc.DRAG_MACH): the regime limits 0.9 and 1 of
/// the leading-edge drag with a value on each side.
constexpr std::array<double, 12> kDragMach{0.05, 0.3, 0.6,  0.89, 0.9, 0.91,
                                           0.99, 1.0, 1.01, 1.1,  1.5, 2.0};

/// The coefficient of friction, stagnation CD and base CD the drag is pinned for
/// (ProbeFinCalc.DRAG_CF, DRAG_STAGNATION and DRAG_BASE).
constexpr double kDragCf         = 0.006;
constexpr double kDragStagnation = 1.1;
constexpr double kDragBase       = 0.25;

/// The drag Java gives for a fin set with each cross-section: the friction CD (the same for
/// all three), the pressure CD and the base CD of square edges (the same at every Mach number),
/// the base CD of rounded edges, and then the pressure CD of rounded edges at each of kDragMach.
/// An airfoil has the pressure CD of rounded edges and no base CD (the probe checks all that).
using DragPins = std::array<double, 16>;

/// Two fin sets on one body tube: the forces, for kPairConditions, of the first and of the
/// second.
using PairPins = std::array<std::array<ForcePins, 2>, 2>;

/// The Mach numbers of the pinned single-fin CNa of the fins at the limits of "no area", "no
/// span" and "no sweep cosine" (ProbeFinCalcFix.SINGLE_FIN_MACH): subsonic, the two ends of the
/// transonic interpolation with a value between them, and supersonic.
constexpr std::array<double, 5> kSingleFinMach{0.3, 0.9, 1.2, 1.5, 2.0};

/// calculateFinCNa1() at each of kSingleFinMach, at an angle of attack of 0.
using SingleFinPins = std::array<double, 5>;

// ---- the tables, as ProbeFinCalc prints them (Double.toString, the shortest digits that give
// the double back)

// NOLINTBEGIN(modernize-use-std-numbers): pinned results, a few of them near e or 1/sqrt(pi)

// mach, angle of attack in degrees, theta, instance rotation, roll rate
constexpr std::array<Condition, 75> kFullConditions{{
    // the Mach numbers: each regime limit (0.5, 0.9, 1, 1.5, 2, the end of the K tables) and
    // a value on each side of it
    {0.05, 2.0, kQuarterPi, 0.0, 0.0},
    {0.3, 2.0, kQuarterPi, 0.0, 0.0},
    {0.49, 2.0, kQuarterPi, 0.0, 0.0},
    {0.5, 2.0, kQuarterPi, 0.0, 0.0},
    {0.51, 2.0, kQuarterPi, 0.0, 0.0},
    {0.6, 2.0, kQuarterPi, 0.0, 0.0},
    {0.89, 2.0, kQuarterPi, 0.0, 0.0},
    {0.9, 2.0, kQuarterPi, 0.0, 0.0},
    {0.91, 2.0, kQuarterPi, 0.0, 0.0},
    {0.99, 2.0, kQuarterPi, 0.0, 0.0},
    {1.0, 2.0, kQuarterPi, 0.0, 0.0},
    {1.01, 2.0, kQuarterPi, 0.0, 0.0},
    {1.1, 2.0, kQuarterPi, 0.0, 0.0},
    {1.49, 2.0, kQuarterPi, 0.0, 0.0},
    {1.5, 2.0, kQuarterPi, 0.0, 0.0},
    {1.51, 2.0, kQuarterPi, 0.0, 0.0},
    {1.99, 2.0, kQuarterPi, 0.0, 0.0},
    {2.0, 2.0, kQuarterPi, 0.0, 0.0},
    {2.01, 2.0, kQuarterPi, 0.0, 0.0},
    {3.0, 2.0, kQuarterPi, 0.0, 0.0},
    {4.9, 2.0, kQuarterPi, 0.0, 0.0},
    {5.0, 2.0, kQuarterPi, 0.0, 0.0},
    // the angles of attack, subsonic, transonic and supersonic
    {0.3, 0.0, 0.0, kTwoThirdsPi, 0.0},
    {0.3, 2.0, 0.0, kTwoThirdsPi, 0.0},
    {0.3, 10.0, 0.0, kTwoThirdsPi, 0.0},
    {0.3, 17.0, 0.0, kTwoThirdsPi, 0.0},
    {0.3, 25.0, 0.0, kTwoThirdsPi, 0.0},
    {1.1, 0.0, 0.0, kTwoThirdsPi, 0.0},
    {1.1, 2.0, 0.0, kTwoThirdsPi, 0.0},
    {1.1, 10.0, 0.0, kTwoThirdsPi, 0.0},
    {1.1, 17.0, 0.0, kTwoThirdsPi, 0.0},
    {1.1, 25.0, 0.0, kTwoThirdsPi, 0.0},
    {2.0, 0.0, 0.0, kTwoThirdsPi, 0.0},
    {2.0, 2.0, 0.0, kTwoThirdsPi, 0.0},
    {2.0, 10.0, 0.0, kTwoThirdsPi, 0.0},
    {2.0, 17.0, 0.0, kTwoThirdsPi, 0.0},
    {2.0, 25.0, 0.0, kTwoThirdsPi, 0.0},
    // the sides of the angle of attack limits (10, 20 and 30 degrees), reverse flow, and the
    // middle of the NACA blend
    {1.5, 9.99, 1.0, 0.7, 0.0},
    {1.5, 10.01, 1.0, 0.7, 0.0},
    {1.5, 19.99, 1.0, 0.7, 0.0},
    {1.5, 20.01, 1.0, 0.7, 0.0},
    {1.5, 29.99, 1.0, 0.7, 0.0},
    {1.5, 30.01, 1.0, 0.7, 0.0},
    {1.5, 170.0, 1.0, 0.7, 0.0},
    {0.6, 15.0, 1.0, 0.7, 0.0},
    // theta and the instance rotation, and a fin in line with the airflow
    {0.6, 2.0, 0.0, kTwoThirdsPi, 0.0},
    {0.6, 2.0, 0.0, 0.7, 0.0},
    {0.6, 2.0, 0.0, -1.2, 0.0},
    {0.6, 2.0, kQuarterPi, kTwoThirdsPi, 0.0},
    {0.6, 2.0, kQuarterPi, 0.7, 0.0},
    {0.6, 2.0, kQuarterPi, -1.2, 0.0},
    {0.6, 2.0, 1.0, kTwoThirdsPi, 0.0},
    {0.6, 2.0, 1.0, 0.7, 0.0},
    {0.6, 2.0, 1.0, -1.2, 0.0},
    {2.0, 10.0, kQuarterPi, 2 * kTwoThirdsPi, 0.0},
    {2.0, 10.0, 1.0, -2.5, 0.0},
    {0.3, 2.0, 0.0, 0.0, 0.0},
    // the roll rate: the stalled fin tips of a slow flight, each Mach regime, both directions,
    // and the sides of the 0.1 rad/s limit
    {0.0, 2.0, kQuarterPi, 0.0, 20.0},
    {0.01, 2.0, kQuarterPi, 0.0, 20.0},
    {0.05, 2.0, kQuarterPi, 0.0, 20.0},
    {0.3, 2.0, kQuarterPi, 0.0, 20.0},
    {0.9, 2.0, kQuarterPi, 0.0, 20.0},
    {1.2, 2.0, kQuarterPi, 0.0, 20.0},
    {1.5, 2.0, kQuarterPi, 0.0, 20.0},
    {2.0, 2.0, kQuarterPi, 0.0, 20.0},
    {0.01, 2.0, kQuarterPi, 0.0, -20.0},
    {0.6, 2.0, kQuarterPi, 0.0, -20.0},
    {2.0, 2.0, kQuarterPi, 0.0, -20.0},
    {0.3, 2.0, kQuarterPi, 0.0, 0.09},
    {0.3, 2.0, kQuarterPi, 0.0, 0.1},
    {0.3, 2.0, kQuarterPi, 0.0, 400.0},
    // NaN as the Mach number, the angle of attack, theta and the roll rate
    {kNaN, 2.0, kQuarterPi, 0.0, 20.0},
    {0.3, kNaN, kQuarterPi, 0.0, 20.0},
    {0.3, 2.0, kNaN, 0.0, 20.0},
    {0.3, 2.0, kQuarterPi, 0.0, kNaN},
}};
constexpr std::array<Condition, 8>  kShortConditions{{
    {0.3, 2.0, kQuarterPi, 0.0, 0.0},
    {0.6, 10.0, 1.0, kTwoThirdsPi, 20.0},
    {0.9, 17.0, 0.0, 0.7, 0.0},
    {1.1, 2.0, kQuarterPi, -1.2, 20.0},
    {1.5, 10.0, 1.0, 0.0, 0.0},
    {2.0, 25.0, 0.0, kTwoThirdsPi, 20.0},
    {0.01, 2.0, kQuarterPi, 0.7, -20.0},
    {3.0, 0.0, 1.0, -1.2, 0.0},
}};
constexpr std::array<Condition, 2>  kPairConditions{{
    {0.3, 2.0, kQuarterPi, 0.0, 0.0},
    {2.0, 10.0, 1.0, kTwoThirdsPi, 20.0},
}};
// a fin square to the lateral airflow, sub- and supersonic (in line with it, the sin^2 of the
// angle between the two would hide the single-fin CNa)
constexpr std::array<Condition, 2> kSquareConditions{{
    {0.3, 2.0, kHalfPi, 0.0, 0.0},
    {2.0, 2.0, kHalfPi, 0.0, 0.0},
}};
// a fin nearly in line with the lateral airflow: a CNa on each side of 1e-8
constexpr std::array<Condition, 2> kSmallCNaConditions{{
    {0.3, 2.0, 3.0E-5, 0.0, 0.0},
    {0.3, 2.0, 4.0E-5, 0.0, 0.0},
}};
// the roll forcing of canted fins, supersonic (beta * aspect ratio above 2)
constexpr std::array<Condition, 2> kCantConditions{{
    {2.0, 2.0, kQuarterPi, 0.0, 0.0},
    {4.9, 10.0, 1.0, 0.2, 20.0},
}};

// ---- Swept: a swept trapezoid on the body tube: 3 fins, square, no cant
// geometry warnings: []
// interfering fins: 3, NACA model: true
constexpr GeometryPins              kGeometrySwept{0.0038999999999999985, 0.06,
                                                   1.8461538461538467,    0.06620294599018006,
                                                   0.013797054009819966,  0.02759410801963993,
                                                   0.970142500145332,     0.8944271909999159,
                                                   1.2240162969669531E-5, 0.04689852700491};
constexpr std::array<ForcePins, 75> kForcesSwept{{
    {0.028613416945880987, 4.053979261762691, 0.14151057185065602, 0.0809820198922574, 0.0, 0.0,
     0.0},
    {0.028567756199827005, 4.104025277260992, 0.14325750734655496, 0.08185091087342615, 0.0, 0.0,
     0.0},
    {0.028476043126109865, 4.1955666303065495, 0.146452903371306, 0.0834079838469062, 0.0, 0.0,
     0.0},
    {0.028469688071545176, 4.201878689268643, 0.14667323579424307, 0.0835148254301259, 0.0, 0.0,
     0.0},
    {0.028467402040406122, 4.208355331891685, 0.14689931327073727, 0.08363683620675288, 0.0, 0.0,
     0.0},
    {0.028770320332580448, 4.2745213565693385, 0.14920894323789902, 0.08585578186880335, 0.0, 0.0,
     0.0},
    {0.03163127648167676, 4.615434042633997, 0.16110904090519118, 0.10192169233139746, 0.0, 0.0,
     0.0},
    {0.03172548090766979, 4.631964161831805, 0.16168605091668892, 0.10259135442787878, 0.0, 0.0,
     0.0},
    {0.03195803716290915, 4.6388075187752245, 0.1619249291377927, 0.10349605805974019, 0.0, 0.0,
     0.0},
    {0.03382684046464444, 4.6676259246617375, 0.1629308812735842, 0.11022873855210914, 0.0, 0.0,
     0.0},
    {0.03406062051970767, 4.665846209587764, 0.1628687574980078, 0.11094821887311875, 0.0, 0.0,
     0.0},
    {0.03429430201785161, 4.662493759217902, 0.1627517349040802, 0.11162914301459695, 0.0, 0.0,
     0.0},
    {0.03638918112108809, 4.55079921713994, 0.15885285987254466, 0.11561050979009707, 0.0, 0.0,
     0.0},
    {0.04511819370518574, 2.898864555068937, 0.10118946211062686, 0.0913097150486163, 0.0, 0.0,
     0.0},
    {0.045320955736844734, 2.867829000277398, 0.10010611687803594, 0.09073809784033744, 0.0, 0.0,
     0.0},
    {0.04536452031875543, 2.832864179637029, 0.09888561439294852, 0.08971796926723054, 0.0, 0.0,
     0.0},
    {0.04618381964057369, 1.7603847318775234, 0.06144901934620073, 0.056759008531501155, 0.0, 0.0,
     0.0},
    {0.04619177307195061, 1.746446164897799, 0.06096247157258882, 0.05631929305572525, 0.0, 0.0,
     0.0},
    {0.04619864178238036, 1.7340626937239674, 0.060530206882970485, 0.05592826689599455, 0.0, 0.0,
     0.0},
    {0.046559880474380096, 1.0190292313182518, 0.03557083052114082, 0.03312347234877496, 0.0, 0.0,
     0.0},
    {0.046729548166239955, 0.5873626358039681, 0.020502823795943148, 0.01916175384232912, 0.0, 0.0,
     0.0},
    {0.04672470646013913, 0.5857048935084626, 0.020444957673530866, 0.019105692917713975, 0.0, 0.0,
     0.0},
    {0.028567756199827002, 6.156037915891486, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.028567756199827002, 6.156037915891486, 0.21488626101983235, 0.12277636631013918, 0.0, 0.0,
     0.0},
    {0.028567756199827002, 6.156037915891486, 1.0744313050991618, 0.6138818315506959, 0.0, 0.0,
     0.0},
    {0.02996330309693678, 6.156037915891486, 1.8265332186685752, 1.0945793689518006, 0.0, 0.0, 0.0},
    {0.030347790507364983, 6.156037915891486, 2.1488626101983237, 1.3042646464681644, 0.0, 0.0,
     0.0},
    {0.0363904474051762, 6.814488895595873, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.036389181121088084, 6.826198825709909, 0.23827928980881694, 0.17341576468514552, 0.0, 0.0,
     0.0},
    {0.03638080504223401, 6.904681587664864, 1.2050942750658138, 0.8768459975736339, 0.0, 0.0, 0.0},
    {0.03854132501359799, 6.8354134331667415, 2.0281079924447534, 1.5633193859897794, 0.0, 0.0,
     0.0},
    {0.03915991000281817, 6.845666045234335, 2.3895882396263644, 1.8715212081512225, 0.0, 0.0, 0.0},
    {0.04621864474373272, 2.5696058103805233, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.046191773071950604, 2.619669247346698, 0.09144370735888321, 0.08447893958358786, 0.0, 0.0,
     0.0},
    {0.04607148866517871, 2.869959898887439, 0.5009024963578935, 0.46154647366624824, 0.0, 0.0,
     0.0},
    {0.045081413017592706, 2.9025439892357965, 0.8612021380343036, 0.7764841855271665, 0.0, 0.0,
     0.0},
    {0.04481253089182228, 2.9791484741827023, 1.0399189956050687, 0.9320280423108986, 0.0, 0.0,
     0.0},
    {0.04509776200814342, 0.5677474962433422, 0.09899154066271058, 0.08928593883252749, 0.0, 0.0,
     0.0},
    {0.0450971058528442, 0.5679686003034774, 0.09922835045362585, 0.08949822848020571, 0.0, 0.0,
     0.0},
    {0.043425121584820245, 0.631064624203168, 0.2201729681492675, 0.19122075823145393, 0.0, 0.0,
     0.0},
    {0.04342511707336368, 0.6312492071341554, 0.22034754130189382, 0.19137235555725154, 0.0, 0.0,
     0.0},
    {0.04342511707336368, 0.6312492071341554, 0.22034754130189382, 0.19137235555725154, 0.0, 0.0,
     0.0},
    {0.04342511707336368, 0.6312492071341554, 0.22034754130189382, 0.19137235555725154, 0.0, 0.0,
     0.0},
    {0.04342511707336368, 0.48139816418514175, 0.16803965956173939, 0.14594283778873418, 0.0, 0.0,
     0.0},
    {0.02981163669532825, 0.7466066443006315, 0.19546116240468264, 0.11654034323309902, 0.0, 0.0,
     0.0},
    {0.028770320332580448, 6.411782034854006, 0.22381341485684847, 0.128783672803205, 0.0, 0.0,
     0.0},
    {0.028770320332580444, 3.547993174327186, 0.12384832546058909, 0.07126311992309653, 0.0, 0.0,
     0.0},
    {0.028770320332580448, 7.426526541850409, 0.2592346802840762, 0.149165295861739, 0.0, 0.0, 0.0},
    {0.028770320332580448, 7.976365440377509, 0.27842767855375, 0.16020907002896254, 0.0, 0.0, 0.0},
    {0.028770320332580448, 0.06219543990817738, 0.0021710304122479498, 0.001249224808242954, 0.0,
     0.0, 0.0},
    {0.028770320332580448, 7.1618031474114865, 0.2499940906040465, 0.14384820135861115, 0.0, 0.0,
     0.0},
    {0.028770320332580448, 6.751184386341217, 0.23566079190177427, 0.13560072945687252, 0.0, 0.0,
     0.0},
    {0.028770320332580448, 0.7466066443006315, 0.026061488320624354, 0.014995947346563334, 0.0, 0.0,
     0.0},
    {0.028770320332580448, 5.588222272867841, 0.19506575598964962, 0.11224208571478385, 0.0, 0.0,
     0.0},
    {0.04607148866517871, 0.25633447907219753, 0.04473880646194346, 0.04122366829610104, 0.0, 0.0,
     0.0},
    {0.04607148866517871, 0.47086044082754425, 0.0821806500983258, 0.07572369779004069, 0.0, 0.0,
     0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.028614640395012034, 4.052582563546725, 0.14146181788560536, 0.08095758096842558,
     -3.3992032511841557, 3.3992032511841557, 0.0},
    {0.02861459148642942, 4.052638397069487, 0.1414637668432157, 0.08095855796700233,
     -3.21109379268183, 3.21109379268183, 0.0},
    {0.028613416945880987, 4.053979261762691, 0.14151057185065602, 0.0809820198922574,
     -0.9135798675661776, 0.9135798675661776, 0.0},
    {0.028567756199827005, 4.104025277260992, 0.14325750734655496, 0.08185091087342615,
     -0.15941567637793963, 0.15941567637793963, 0.0},
    {0.03172548090766979, 4.631964161831805, 0.16168605091668892, 0.10259135442787878,
     -0.11629302490304835, 0.11629302490304835, 0.0},
    {0.03869218567133341, 4.243341729530151, 0.14812056893514366, 0.11462217109964236,
     -0.0604740914938328, 0.0604740914938328, 0.0},
    {0.045320955736844734, 2.867829000277398, 0.10010611687803594, 0.09073809784033744,
     -0.008672968948118146, 0.008672968948118146, 0.0},
    {0.04619177307195061, 1.746446164897799, 0.06096247157258882, 0.05631929305572525,
     -0.004197036709069692, 0.004197036709069692, 0.0},
    {0.02861459148642942, 4.052638397069487, 0.1414637668432157, 0.08095855796700233,
     3.21109379268183, -3.21109379268183, 0.0},
    {0.028770320332580448, 4.2745213565693385, 0.14920894323789902, 0.08585578186880335,
     0.09504553938582122, -0.09504553938582122, 0.0},
    {0.04619177307195061, 1.746446164897799, 0.06096247157258882, 0.05631929305572525,
     0.004187208662177475, -0.004187208662177475, 0.0},
    {0.028567756199827005, 4.104025277260992, 0.14325750734655496, 0.08185091087342615, 0.0, 0.0,
     0.0},
    {0.028567756199827005, 4.104025277260992, 0.14325750734655496, 0.08185091087342615,
     -7.97078381889698E-4, 7.97078381889698E-4, 0.0},
    {0.028567756199827005, 4.104025277260992, 0.14325750734655496, 0.08185091087342615,
     -2.8936535122676172, 2.8936535122676172, 0.0},
    {kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN},
    {0.030347790507364983, 4.104025277260992, 1.4325750734655496, 0.8695097643121099,
     -0.15941567637793963, 0.15941567637793963, 0.0},
    {kNaN, kNaN, kNaN, kNaN, -0.15941567637793963, 0.15941567637793963, 0.0},
    {0.028567756199827005, 4.104025277260992, 0.14325750734655496, 0.08185091087342615, kNaN, kNaN,
     0.0},
}};
// warnings after the rows: []
constexpr DragPins kDragSwept{
    0.0259952240628836,   0.0806724575544199,   0.022918311805232926, 0.011459155902616463,
    7.659118331154726E-5, 0.002941693276223718, 0.015001027538773793, 0.06784598670626531,
    0.07333859777674535,  0.07202950380643044,  0.061556752043911214, 0.06024765807359632,
    0.060659727920450844, 0.06409162295949149,  0.0742566883711357,   0.08033097470852194};
// mach, aoa in degrees, calculateFinCNa1, calculateCPPos
constexpr std::array<std::array<double, 4>, 13> kSingleFinSwept{{
    {0.3, 0.0, 4.901088037720772, 0.25},
    {0.3, 25.0, 4.901088037720772, 0.25},
    {0.9, 10.0, 5.531560507311536, 0.32995443015100046},
    {1.2, 0.0, 5.3730455552385905, 0.4051474273554937},
    {1.2, 10.0, 5.593782639468763, 0.4051474273554937},
    {1.2, 25.0, 6.0081272254926095, 0.4051474273554937},
    {1.5, 10.0, 4.259477286274919, 0.4475339068436391},
    {2.0, 0.0, 2.2935282049546544, 0.4684908868950161},
    {2.0, 10.0, 2.6146136899953745, 0.4684908868950161},
    {2.0, 25.0, 3.0694257006730887, 0.4684908868950161},
    {2.0, 170.0, 2.614613689995374, 0.4684908868950161},
    {4.95, 10.0, 1.1535830842827282, 0.48994087464054353},
    {5.5, 10.0, 1.1535830842827282, 0.4910381072110464},
}};

// ---- Rectangular: a rectangular fin on the body tube: 4 fins, rounded, canted
// geometry warnings: []
// interfering fins: 4, NACA model: true
constexpr GeometryPins              kGeometryRectangular{0.0030005778931200973,
                                                         0.05,
                                                         1.666345676765898,
                                                         0.05999999999999998,
                                                         0.0,
                                                         0.02499999999999999,
                                                         1.0,
                                                         1.0,
                                                         8.325033952014491E-6,
                                                         0.02999999999999999};
constexpr std::array<ForcePins, 75> kForcesRectangular{{
    {0.014221504078558432, 3.0905260837762603, 0.1078797115613237, 0.030684235159261445,
     0.13002271756672545, 0.0, 0.13002271756672545},
    {0.014177985462065269, 3.122495770108669, 0.10899566413598442, 0.030906778830962718,
     0.13136772659885, 0.0, 0.13136772659885},
    {0.014074454425411718, 3.1803921029751407, 0.1110166274026855, 0.031249969256840215,
     0.1338035055997016, 0.0, 0.1338035055997016},
    {0.01406417914392229, 3.1843561446876087, 0.11115499856182343, 0.03126607625031818,
     0.13397027833095557, 0.0, 0.13397027833095557},
    {0.014057211183860044, 3.1884197408821655, 0.11129684482795647, 0.031290465036877706,
     0.13414123945731, 0.0, 0.13414123945731},
    {0.014263290800022047, 3.229711264058926, 0.11273819089315251, 0.03216035201954863,
     0.1358784310908301, 0.0, 0.1358784310908301},
    {0.016387568903199888, 3.435769754792774, 0.11993098912314427, 0.03930754695768886,
     0.14454759750995827, 0.0, 0.14454759750995827},
    {0.016447339023140925, 3.445464530754183, 0.12026940064468385, 0.039562232130261575,
     0.1449554701771136, 0.0, 0.1449554701771136},
    {0.016796543678513486, 3.4552501965781226, 0.1206109848209391, 0.04051695349306861,
     0.14536716670681052, 0.0, 0.14536716670681052},
    {0.01957510073365693, 3.521936845723927, 0.1229387880103722, 0.0481307831875346,
     0.14817276650189834, 0.0, 0.14817276650189834},
    {0.01991932038879305, 3.527545984854188, 0.12313458390242321, 0.04905514455386174,
     0.14840875076255705, 0.0, 0.14840875076255705},
    {0.020262611691693442, 3.532304238418128, 0.12330067828509422, 0.04996787530826564,
     0.14860893708763054, 0.0, 0.14860893708763054},
    {0.02330105703782559, 3.528030168247201, 0.12315148509120631, 0.05739119556806252,
     0.1484291210292539, 0.0, 0.1484291210292539},
    {0.03501974680937163, 2.5224245215171344, 0.0880492260670331, 0.06166923207257247,
     0.10612189713146217, 0.0, 0.10612189713146217},
    {0.03528943501441403, 2.4926693586117645, 0.08701057494270112, 0.061411080600145065,
     0.1048700561704988, 0.0, 0.1048700561704988},
    {0.035429447412549796, 2.4660391846150533, 0.08608100650945795, 0.06099604986692394,
     0.10374968782592998, 0.0, 0.10374968782592998},
    {0.04141451447075133, 1.6931768490965942, 0.05910302167055749, 0.04895445892480865,
     0.0696430916585334, 0.0, 0.0696430916585334},
    {0.041489478766537746, 1.680446553572441, 0.058658650527260756, 0.048674336710490866,
     0.06922158734355578, 0.0, 0.06922158734355578},
    {0.041555494536441424, 1.6689659473763, 0.05825790177076571, 0.04841871837479191,
     0.06886929641337011, 0.0, 0.06886929641337011},
    {0.04605297479373801, 0.9868495523453036, 0.03444754782051535, 0.031728241029685564,
     0.043928027280323266, 0.0, 0.043928027280323266},
    {0.041403401000659375, 0.5299399486901827, 0.018498393884987, 0.015317928397765237,
     0.026667356558935967, 0.0, 0.026667356558935967},
    {0.041006063980087756, 0.5259132061891576, 0.018357834055441242, 0.015055650356265148,
     0.026672786423565423, 0.0, 0.026672786423565423},
    {0.01417798546206527, 4.683743655163002, 0.0, 0.0, 0.13136772659885, 0.0, 0.13136772659885},
    {0.01417798546206527, 4.683743655163002, 0.16349349620397657, 0.04636016824644406,
     0.13136772659885, 0.0, 0.13136772659885},
    {0.01417798546206527, 4.683743655163002, 0.8174674810198829, 0.23180084123222033,
     0.13136772659885, 0.0, 0.13136772659885},
    {0.014822444859806095, 4.683743655163002, 1.389694717733801, 0.4119734665114612,
     0.13136772659885, 0.0, 0.13136772659885},
    {0.014999999999999994, 4.683743655163002, 1.6349349620397657, 0.4904804886119295,
     0.06568386329942498, 0.0, 0.06568386329942498},
    {0.02330105703782559, 5.281592160712934, 0.0, 0.0, 0.14813593698927996, 0.0,
     0.14813593698927996},
    {0.02330105703782559, 5.2920452523708, 0.1847272276368094, 0.08608679335209375,
     0.1484291210292539, 0.0, 0.1484291210292539},
    {0.02330105703782559, 5.362104381355818, 0.9358637629027265, 0.4361322983806103,
     0.15039411082798299, 0.0, 0.15039411082798299},
    {0.022850037267515843, 5.103728635146999, 1.5143067697977362, 0.6920393224865962,
     0.1531533098028682, 0.0, 0.1531533098028682},
    {0.022714480305010873, 5.05379738222725, 1.7641080809707175, 0.8014159652223977,
     0.07731645938190806, 0.0, 0.07731645938190806},
    {0.041707334910391386, 2.4810051033144958, 0.0, 0.0, 0.06764314936785412, 0.0,
     0.06764314936785412},
    {0.04148947876653774, 2.520669830358661, 0.08798797579089111, 0.07301150506573627,
     0.06922158734355578, 0.0, 0.06922158734355578},
    {0.040495638403322584, 2.7189724435367686, 0.4745502141071223, 0.3843442774940267,
     0.07711294066038751, 0.0, 0.07711294066038751},
    {0.03095833486869094, 2.396701173025409, 0.7111155531469628, 0.4402990684931613,
     0.08608847666852323, 0.0, 0.08608847666852323},
    {0.027862706443423795, 2.361551513337009, 0.8243369872637117, 0.45936518973170365,
     0.04526336774399185, 0.0, 0.04526336774399185},
    {0.035289435014414026, 0.5086802273827395, 0.08869266662814601, 0.06259828190458092,
     0.12252573685396695, 0.0, 0.12252573685396695},
    {0.03528941528390591, 0.5089226029057538, 0.0889125743358359, 0.06275345519396934,
     0.12258420968993777, 0.0, 0.12258420968993777},
    {0.026514511970606287, 0.5002411080756506, 0.17452977925100052, 0.09255143842355852,
     0.16065718256958855, 0.0, 0.16065718256958855},
    {0.026514476894208747, 0.500387118439747, 0.17466805502680832, 0.09262464218329382,
     0.16054353154581305, 0.0, 0.16054353154581305},
    {0.026514476894208747, 0.500387118439747, 0.17466805502680832, 0.09262464218329382,
     1.6070423578161267E-4, 0.0, 1.6070423578161267E-4},
    {0.026514476894208747, 0.500387118439747, 0.17466805502680832, 0.09262464218329382, 0.0, 0.0,
     0.0},
    {0.026514476894208747, 0.3816011766452697, 0.1332039392388889, 0.07063665538334211, 0.0, 0.0,
     0.0},
    {0.014851860808601602, 0.5641155319561382, 0.14768510091410847, 0.0438679712456124,
     0.1358784310908301, 0.0, 0.1358784310908301},
    {0.014263290800022047, 4.844566896088388, 0.16910728633972874, 0.04824052802932294,
     0.1358784310908301, 0.0, 0.1358784310908301},
    {0.014263290800022047, 2.6807664681141046, 0.09357640269130142, 0.026694148872119957,
     0.1358784310908301, 0.0, 0.1358784310908301},
    {0.014263290800022048, 5.61128005318875, 0.19587062435925245, 0.055875193488357996,
     0.1358784310908301, 0.0, 0.1358784310908301},
    {0.014263290800022047, 6.026723265622708, 0.2103723281833221, 0.06001203386312793,
     0.1358784310908301, 0.0, 0.1358784310908301},
    {0.014263290800022045, 0.04699317095136897, 0.0016403711181078892, 4.679418055506026E-4,
     0.1358784310908301, 0.0, 0.1358784310908301},
    {0.014263290800022047, 5.411262306742048, 0.18888868788342417, 0.05388348568231759,
     0.1358784310908301, 0.0, 0.1358784310908301},
    {0.014263290800022047, 5.101010017132026, 0.17805883995233243, 0.05079410027509402,
     0.1358784310908301, 0.0, 0.1358784310908301},
    {0.014263290800022045, 0.5641155319561382, 0.0196913467885478, 0.005617268109782749,
     0.1358784310908301, 0.0, 0.1358784310908301},
    {0.014263290800022047, 4.222307696043785, 0.14738634265652, 0.04204428530523278,
     0.1358784310908301, 0.0, 0.1358784310908301},
    {0.040495638403322584, 0.2428488234960501, 0.04238511554600641, 0.03432824625668244,
     0.07711294066038751, 0.0, 0.07711294066038751},
    {0.04049563840332258, 0.4460886592380503, 0.07785715859510997, 0.06305750683155423,
     0.07711294066038751, 0.0, 0.07711294066038751},
    {0.0, 0.0, 0.0, 0.0, 0.13136772659885, 0.0, 0.13136772659885},
    {0.014222718813935215, 3.08963069094687, 0.10784845645538048, 0.030677965413636255,
     -2.3571091369430492, 2.48709418409192, 0.12998504714887066},
    {0.014222670253693498, 3.0896664878931714, 0.10784970600453073, 0.030678216109204567,
     -2.210516545155055, 2.3405030983310913, 0.1299865531760364},
    {0.014221504078558432, 3.0905260837762603, 0.1078797115613237, 0.030684235159261445,
     -0.49134020335283746, 0.6213629209195629, 0.13002271756672545},
    {0.014177985462065269, 3.122495770108669, 0.10899566413598442, 0.030906778830962718,
     0.022942624611377582, 0.1084251019874724, 0.13136772659885},
    {0.016447339023140925, 3.445464530754183, 0.12026940064468385, 0.039562232130261575,
     0.0658598418673148, 0.0790956283097988, 0.1449554701771136},
    {0.026542430861011955, 3.4041118761213234, 0.11882592068911686, 0.06307857568773949,
     0.10208520320261921, 0.04113050345585331, 0.14321570665847252},
    {0.03528943501441403, 2.4926693586117645, 0.08701057494270112, 0.061411080600145065,
     0.09897202360910369, 0.005898032561395112, 0.1048700561704988},
    {0.041489478766537746, 1.680446553572441, 0.058658650527260756, 0.048674336710490866,
     0.06636729759603177, 0.0028542897475240143, 0.06922158734355578},
    {0.014222670253693498, 3.0896664878931714, 0.10784970600453073, 0.030678216109204567,
     2.4704896515071275, -2.3405030983310913, 0.1299865531760364},
    {0.014263290800022047, 3.229711264058926, 0.11273819089315251, 0.03216035201954863,
     0.20052277809300528, -0.06464434700217518, 0.1358784310908301},
    {0.041489478766537746, 1.680446553572441, 0.058658650527260756, 0.048674336710490866,
     0.0720697613613338, -0.0028481740177780275, 0.06922158734355578},
    {0.014177985462065269, 3.122495770108669, 0.10899566413598442, 0.030906778830962718,
     0.13136772659885, 0.0, 0.13136772659885},
    {0.014177985462065269, 3.122495770108669, 0.10899566413598442, 0.030906778830962718,
     0.1308256010889126, 5.42125509937362E-4, 0.13136772659885},
    {0.014177985462065269, 3.122495770108669, 0.10899566413598442, 0.030906778830962718,
     -1.916505209830198, 2.047872936429048, 0.13136772659885},
    {kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN},
    {0.014999999999999993, 3.1224957701086686, 1.089956641359844, 0.3269869924079531,
     0.022942624611377582, 0.1084251019874724, 0.13136772659885},
    {kNaN, kNaN, kNaN, kNaN, 0.022942624611377582, 0.1084251019874724, 0.13136772659885},
    {0.014177985462065269, 3.122495770108669, 0.10899566413598442, 0.030906778830962718, kNaN, kNaN,
     0.13136772659885},
}};
// warnings after the rows: []
constexpr DragPins kDragRectangular{
    0.019560726684562567,  0.05602253996834716,   0.012732395447351627, 0.006366197723675813,
    5.3188321744130056E-5, 0.0020428425529331377, 0.010417380235259581, 0.04711526854601758,
    0.05092958178940651,   0.050020488754465596,  0.04274774447493836,  0.041838651439997446,
    0.04212481105586865,   0.04450807149964687,   0.05156714470217759,  0.05578539910314024};

// ---- Elliptical: an elliptical fin on the body tube: 3 fins, airfoil, no cant
// geometry warnings: []
// interfering fins: 3, NACA model: false
constexpr GeometryPins kGeometryElliptical{
    0.002743872160775905, 0.05, 1.8222423301915474, 0.05955669520398204,  0.005221652398008998,
    0.020889318657849276, 1.0,  0.84746522956208,   6.344542600635597E-6, 0.03500000000000002};
constexpr std::array<ForcePins, 75> kForcesElliptical{{
    {0.02011082619900451, 3.023354383172786, 0.10553497688193472, 0.04244791155977096, 0.0, 0.0,
     0.0},
    {0.02011082619900451, 3.058639708012974, 0.10676666707412877, 0.04294331770749561, 0.0, 0.0,
     0.0},
    {0.02011082619900451, 3.1229662048375397, 0.10901208540585347, 0.043846462063763095, 0.0, 0.0,
     0.0},
    {0.02011082619900451, 3.1273912126358914, 0.10916654731686878, 0.043908589196699, 0.0, 0.0,
     0.0},
    {0.020115925470331142, 3.131930168932898, 0.10932498678084258, 0.04398346572256733, 0.0, 0.0,
     0.0},
    {0.020563225265227313, 3.1782165124532114, 0.1109406849671198, 0.045625965921150036, 0.0, 0.0,
     0.0},
    {0.024684869347255386, 3.4141084921746936, 0.11917486841749493, 0.05883632112724429, 0.0, 0.0,
     0.0},
    {0.02485232515429519, 3.4254283267900614, 0.11957000518713372, 0.05943185295222819, 0.0, 0.0,
     0.0},
    {0.025019584143436326, 3.42254229636643, 0.11946926372072353, 0.05978142592430054, 0.0, 0.0,
     0.0},
    {0.026335107182352626, 3.3813964309601943, 0.11803300207088104, 0.06216823521183003, 0.0, 0.0,
     0.0},
    {0.02649499927638555, 3.3724877184682427, 0.11772202934068482, 0.062380901643921655, 0.0, 0.0,
     0.0},
    {0.026653539865519667, 3.3624759738087806, 0.11737255352433168, 0.06256788068957231, 0.0, 0.0,
     0.0},
    {0.028006223090637616, 3.2150575026788566, 0.11222667812538492, 0.0628609076860142, 0.0, 0.0,
     0.0},
    {0.03178284002314781, 1.7400094024420905, 0.060737786176547084, 0.03860838682818708, 0.0, 0.0,
     0.0},
    {0.0318354492296226, 1.7095622000338948, 0.05967497831645876, 0.037995594849449046, 0.0, 0.0,
     0.0},
    {0.03188623446645092, 1.6912982699671906, 0.05903744688842281, 0.037649637475697856, 0.0, 0.0,
     0.0},
    {0.033079064203723994, 1.1087309715217009, 0.03870201194377833, 0.02560452675803072, 0.0, 0.0,
     0.0},
    {0.0330941615736974, 1.1008542593141353, 0.038427062819270226, 0.025434228514870972, 0.0, 0.0,
     0.0},
    {0.03310912787746426, 1.0940956879649926, 0.03819114417372324, 0.025289509524689537, 0.0, 0.0,
     0.0},
    {0.033912283525462016, 0.6806666100236922, 0.023759746906603336, 0.016114945471799026, 0.0, 0.0,
     0.0},
    {0.03438572041688049, 0.41040237757139186, 0.014325745493267435, 0.009852021585897592, 0.0, 0.0,
     0.0},
    {0.03439928298033647, 0.41040237757139186, 0.014325745493267435, 0.009855907462543726, 0.0, 0.0,
     0.0},
    {0.02011082619900451, 4.58795956201946, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.02011082619900451, 4.58795956201946, 0.1601500006111931, 0.06441497656124341, 0.0, 0.0, 0.0},
    {0.02011082619900451, 4.58795956201946, 0.8007500030559656, 0.32207488280621704, 0.0, 0.0, 0.0},
    {0.02011082619900451, 4.58795956201946, 1.3612750051951417, 0.5475273007705691, 0.0, 0.0, 0.0},
    {0.02011082619900451, 4.58795956201946, 1.601500006111931, 0.6441497656124341, 0.0, 0.0, 0.0},
    {0.02800622309063761, 4.813824013588511, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.028006223090637616, 4.822586254018284, 0.16834001718807734, 0.09429136152902128, 0.0, 0.0,
     0.0},
    {0.02800622309063761, 4.881312892276346, 0.8519498179026951, 0.47719793324421933, 0.0, 0.0,
     0.0},
    {0.028006223090637612, 4.963775651210248, 1.4727818835470565, 0.8249411598893666, 0.0, 0.0,
     0.0},
    {0.02800622309063761, 5.007995962312562, 1.7481203693787213, 0.9791649810821659, 0.0, 0.0, 0.0},
    {0.0330941615736974, 1.6136277414177969, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0330941615736974, 1.6512813889712028, 0.057640594228905336, 0.03815134277230646, 0.0, 0.0,
     0.0},
    {0.0330941615736974, 1.8395296705543251, 0.32105849439299555, 0.2125032337609963, 0.0, 0.0,
     0.0},
    {0.0330941615736974, 2.0536411368620273, 0.6093275913749118, 0.40330371520546393, 0.0, 0.0,
     0.0},
    {0.0330941615736974, 2.159515828114606, 0.7538132289906361, 0.4989363359321333, 0.0, 0.0, 0.0},
    {0.0318354492296226, 0.348871175245839, 0.060828617226686865, 0.03873012710856673, 0.0, 0.0,
     0.0},
    {0.0318354492296226, 0.34903766669105607, 0.06097948353734708, 0.0388261850440364, 0.0, 0.0,
     0.0},
    {0.0318354492296226, 0.4574439749057755, 0.15959823107526683, 0.10161762765068466, 0.0, 0.0,
     0.0},
    {0.0318354492296226, 0.45757795091598497, 0.15972483656025882, 0.10169823850047774, 0.0, 0.0,
     0.0},
    {0.0318354492296226, 0.45757795091598497, 0.15972483656025882, 0.10169823850047774, 0.0, 0.0,
     0.0},
    {0.0318354492296226, 0.45757795091598497, 0.15972483656025882, 0.10169823850047774, 0.0, 0.0,
     0.0},
    {0.0318354492296226, 0.3489543955906148, 0.12180806284726022, 0.07755628801065263, 0.0, 0.0,
     0.0},
    {0.020563225265227313, 0.5551212328315471, 0.1453303989096081, 0.05976923461327234, 0.0, 0.0,
     0.0},
    {0.020563225265227313, 4.767324768679816, 0.16641102745067968, 0.06843894888172504, 0.0, 0.0,
     0.0},
    {0.020563225265227313, 2.6380241323131712, 0.09208441371186275, 0.03787105085146841, 0.0, 0.0,
     0.0},
    {0.020563225265227313, 5.521813395365625, 0.19274764885971513, 0.07927026645690513, 0.0, 0.0,
     0.0},
    {0.020563225265227313, 5.930632750964876, 0.20701813646189202, 0.08513921148106907, 0.0, 0.0,
     0.0},
    {0.020563225265227313, 0.04624390841132865, 0.0016142169215367704, 6.638701236900475E-4, 0.0,
     0.0, 0.0},
    {0.020563225265227313, 5.324984746435046, 0.18587703288753388, 0.07644462597796849, 0.0, 0.0,
     0.0},
    {0.020563225265227313, 5.019679141925443, 0.17521985684056543, 0.07206170774306855, 0.0, 0.0,
     0.0},
    {0.020563225265227313, 0.5551212328315471, 0.019377386521281084, 0.007969231281769648, 0.0, 0.0,
     0.0},
    {0.020563225265227313, 4.154986914638261, 0.14503640407543647, 0.059648324973234655, 0.0, 0.0,
     0.0},
    {0.0330941615736974, 0.1643001632260395, 0.028675788098584974, 0.0189800232917536, 0.0, 0.0,
     0.0},
    {0.0330941615736974, 0.3018027366613487, 0.052674514462699336, 0.034864377848892646, 0.0, 0.0,
     0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.02011082619900451, 3.022368462234827, 0.10550056174887125, 0.042434069224577854,
     -2.0721175936477665, 2.0721175936477665, 0.0},
    {0.02011082619900451, 3.0224078759931214, 0.1055019375485769, 0.042434622593953156,
     -1.903910197056637, 1.903910197056637, 0.0},
    {0.02011082619900451, 3.023354383172786, 0.10553497688193472, 0.04244791155977096,
     -0.473543236574499, 0.473543236574499, 0.0},
    {0.02011082619900451, 3.058639708012974, 0.10676666707412877, 0.04294331770749561,
     -0.08263121598096523, 0.08263121598096523, 0.0},
    {0.02485232515429519, 3.4254283267900614, 0.11957000518713372, 0.05943185295222819,
     -0.06027910351213952, 0.06027910351213952, 0.0},
    {0.029314790102169547, 2.921758522316189, 0.10198861232524344, 0.05979549526252106,
     -0.031345464697953455, 0.031345464697953455, 0.0},
    {0.0318354492296226, 1.7095622000338948, 0.05967497831645876, 0.037995594849449046,
     -0.0044943774264866575, 0.0044943774264866575, 0.0},
    {0.0330941615736974, 1.1008542593141353, 0.038427062819270226, 0.025434228514870972,
     -0.002175074954871226, 0.002175074954871226, 0.0},
    {0.02011082619900451, 3.0224078759931214, 0.1055019375485769, 0.042434622593953156,
     1.903910197056637, -1.903910197056637, 0.0},
    {0.020563225265227313, 3.1782165124532114, 0.1109406849671198, 0.045625965921150036,
     0.0492657226156207, -0.0492657226156207, 0.0},
    {0.0330941615736974, 1.1008542593141353, 0.038427062819270226, 0.025434228514870972,
     0.002170794238653609, -0.002170794238653609, 0.0},
    {0.02011082619900451, 3.058639708012974, 0.10676666707412877, 0.04294331770749561, 0.0, 0.0,
     0.0},
    {0.02011082619900451, 3.058639708012974, 0.10676666707412877, 0.04294331770749561,
     -4.1315607990482616E-4, 4.1315607990482616E-4, 0.0},
    {0.02011082619900451, 3.058639708012974, 0.10676666707412877, 0.04294331770749561,
     -1.577755797532502, 1.577755797532502, 0.0},
    {kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN},
    {0.02011082619900451, 3.058639708012974, 1.0676666707412876, 0.42943317707495615,
     -0.08263121598096523, 0.08263121598096523, 0.0},
    {kNaN, kNaN, kNaN, kNaN, -0.08263121598096523, 0.08263121598096523, 0.0},
    {0.02011082619900451, 3.058639708012974, 0.10676666707412877, 0.04294331770749561, kNaN, kNaN,
     0.0},
}};
// warnings after the rows: []
constexpr DragPins kDragElliptical{
    0.019021862359363526, 0.08047047560497993,   0.025464790894703253, 0.012732395447351627,
    7.639941976567109E-5, 0.0029343280742626233, 0.014963469035193556, 0.06767611876035122,
    0.07315497782270902,  0.07184916146857366,   0.06140263063549082,  0.06009681428135546,
    0.060507852417096976, 0.06393115492194103,   0.07407076976730441,  0.08012984773949293};

// ---- Freeform: a freeform fin with a notch in its tip on the body tube: 4 fins, square, canted
// geometry warnings: [Jagged-edged fin predictions may be inaccurate:  "Freeform Fin Set"]
// interfering fins: 4, NACA model: false
constexpr GeometryPins              kGeometryFreeform{0.00265018369842504,   0.05,
                                                      1.886661669140678,     0.06831936392569711,
                                                      0.013008381644581735,  0.02165998238769602,
                                                      0.9950443675219557,    0.8576583346498127,
                                                      5.2220264173123745E-6, 0.04716806360743029};
constexpr std::array<ForcePins, 75> kForcesFreeform{{
    {0.03008822262600601, 2.9914577969698475, 0.10442157598315978, 0.06283719250279428,
     0.08808572543652972, 0.0, 0.08808572543652972},
    {0.030088222626006012, 3.0282155927117036, 0.10570466510610166, 0.06360930992639592,
     0.08916808638664873, 0.0, 0.08916808638664873},
    {0.030088222626006012, 3.095431818300089, 0.10805095400066284, 0.06502122317848556,
     0.09114731871880989, 0.0, 0.09114731871880989},
    {0.03008822262600601, 3.100065651750648, 0.10821270530206545, 0.06511855936181853,
     0.0912837654309975, 0.0, 0.0912837654309975},
    {0.030094142486134973, 3.104820182537812, 0.10837866973531235, 0.06523126258944507,
     0.09142376616706736, 0.0, 0.09142376616706736},
    {0.030613346788178734, 3.153385437530371, 0.11007391693869388, 0.06739461983154833,
     0.09285380663809216, 0.0, 0.09285380663809216},
    {0.035394353605395126, 3.403378009910128, 0.11880030392580787, 0.08409719931156909,
     0.10021502601217816, 0.0, 0.10021502601217816},
    {0.03558847175536127, 3.4154888082626487, 0.11922305053840106, 0.0848593233334779,
     0.10057163758115231, 0.0, 0.10057163758115231},
    {0.03578234979305694, 3.413444977146458, 0.11915170737373658, 0.08527056143374007,
     0.1009320054590183, 0.0, 0.1009320054590183},
    {0.03730677069005309, 3.3774560793861066, 0.11789545785357308, 0.08796597623084138,
     0.10332647819812407, 0.0, 0.10332647819812407},
    {0.03749198925381418, 3.3688607144860536, 0.11759542301774054, 0.08817772672157721,
     0.10351162255256637, 0.0, 0.10351162255256637},
    {0.03767562679841452, 3.3590662399473565, 0.1172535313593345, 0.0883520057658096,
     0.10366137915242471, 0.0, 0.10366137915242471},
    {0.03924170646823784, 3.208744717897824, 0.11200632036658731, 0.08790638292826056,
     0.1030733479611531, 0.0, 0.1030733479611531},
    {0.043597619572874696, 1.6807729066176944, 0.05867004239758791, 0.051157483775489336,
     0.0656242511661191, 0.0, 0.0656242511661191},
    {0.043657798506395104, 1.6511898545201573, 0.05763739907380584, 0.050326439103937984,
     0.06482737003841343, 0.0, 0.06482737003841343},
    {0.0437158539184244, 1.6335495393393409, 0.05702163591181627, 0.049854990114210844,
     0.06413479356897203, 0.0, 0.06413479356897203},
    {0.04505343564551425, 1.0708737778202004, 0.037380546592465445, 0.03368244100595578,
     0.042043578737563266, 0.0, 0.042043578737563266},
    {0.04506995001538499, 1.0632660129293705, 0.037114985500340224, 0.03345541082644145,
     0.04174489025640953, 0.0, 0.04174489025640953},
    {0.04508632183097544, 1.056738210406284, 0.036887122206444516, 0.033262093264365564,
     0.04148860218115074, 0.0, 0.04148860218115074},
    {0.04596731843104085, 0.6574255097354549, 0.022948479462971447, 0.021097601259652196,
     0.025811184992229872, 0.0, 0.025811184992229872},
    {0.04648887147049408, 0.3963893458239762, 0.01383659840890964, 0.012864956900412863,
     0.015562643345142861, 0.0, 0.015562643345142861},
    {0.04650383690791811, 0.3963893458239762, 0.01383659840890964, 0.012869098315365863,
     0.015562643345142861, 0.0, 0.015562643345142861},
    {0.030088222626006012, 4.542323389067555, 0.0, 0.0, 0.08916808638664873, 0.0,
     0.08916808638664873},
    {0.030088222626006012, 4.542323389067555, 0.15855699765915246, 0.09541396488959387,
     0.08916808638664873, 0.0, 0.08916808638664873},
    {0.030088222626006012, 4.542323389067555, 0.7927849882957623, 0.4770698244479693,
     0.08916808638664873, 0.0, 0.08916808638664873},
    {0.030088222626006012, 4.542323389067555, 1.347734480102796, 0.8110187015615479,
     0.08916808638664873, 0.0, 0.08916808638664873},
    {0.030088222626006012, 4.542323389067555, 1.5855699765915245, 0.9541396488959386,
     0.04458404319332436, 0.0, 0.04458404319332436},
    {0.03924170646823784, 4.804654019755179, 0.0, 0.0, 0.10289211080974255, 0.0,
     0.10289211080974255},
    {0.03924170646823784, 4.813117076846735, 0.16800948054988094, 0.13185957439239082,
     0.1030733479611531, 0.0, 0.1030733479611531},
    {0.03924170646823784, 4.869838516569798, 0.8499471615457941, 0.6670675405378396,
     0.10428804284600368, 0.0, 0.10428804284600368},
    {0.03924170646823784, 4.949485616233312, 1.4685419448173205, 1.1525618386963283,
     0.10599369286170876, 0.0, 0.10599369286170876},
    {0.03924170646823784, 4.99219604177667, 1.7426051566806255, 1.367656000969974,
     0.0534541702901447, 0.0, 0.0534541702901447},
    {0.04506995001538498, 1.5585310411920175, 0.0, 0.0, 0.040792994719180874, 0.0,
     0.040792994719180874},
    {0.04506995001538499, 1.5948990193940555, 0.05567247825051033, 0.05018311623966217,
     0.04174489025640953, 0.0, 0.04174489025640953},
    {0.04506995001538499, 1.776719635616585, 0.3100960752634332, 0.27952029224179986,
     0.04650386344422046, 0.0, 0.04650386344422046},
    {0.04506995001538499, 1.9835203480426684, 0.5885223345120233, 0.5304934439878914,
     0.05191666572210639, 0.0, 0.05191666572210639},
    {0.04506995001538499, 2.0857799885770962, 0.7280745654576009, 0.6562856854529443,
     0.027296604883250367, 0.0, 0.027296604883250367},
    {0.043657798506395104, 0.3369591027977994, 0.05875164742599249, 0.05129935170485493,
     0.07574155647773792, 0.0, 0.07574155647773792},
    {0.043657798506395104, 0.3371199094564874, 0.05889736246434914, 0.05142658366053345,
     0.07577770254567284, 0.0, 0.07577770254567284},
    {0.043657798506395104, 0.4418247258630392, 0.15414881069879727, 0.13459595434977065,
     0.09931321679502947, 0.0, 0.09931321679502947},
    {0.043657798506395104, 0.4419541273137747, 0.15427109328807143, 0.1347027261226381,
     0.09924296130702286, 0.0, 0.09924296130702286},
    {0.043657798506395104, 0.4419541273137747, 0.15427109328807143, 0.1347027261226381,
     9.934230361064463E-5, 0.0, 9.934230361064463E-5},
    {0.043657798506395104, 0.4419541273137747, 0.15427109328807143, 0.1347027261226381, 0.0, 0.0,
     0.0},
    {0.043657798506395104, 0.337039481615827, 0.11764897326822157, 0.10272590338856562, 0.0, 0.0,
     0.0},
    {0.03061334678817873, 0.5507841283990175, 0.14419494762435087, 0.08828579873455043,
     0.09285380663809216, 0.0, 0.09285380663809216},
    {0.03061334678817873, 4.730078156295556, 0.16511087540804079, 0.10109192974732246,
     0.09285380663809216, 0.0, 0.09285380663809216},
    {0.03061334678817873, 2.6174135242501078, 0.09136496776878565, 0.05593974885193214,
     0.09285380663809216, 0.0, 0.09285380663809216},
    {0.030613346788178727, 5.478672041844548, 0.19124173153429583, 0.11709098895662347,
     0.09285380663809216, 0.0, 0.09285380663809216},
    {0.03061334678817873, 5.884297334355581, 0.20540072530166106, 0.12576007268406375,
     0.09285380663809216, 0.0, 0.09285380663809216},
    {0.03061334678817873, 0.04588260956652455, 0.001601605212685803, 9.806099158741048E-4,
     0.09285380663809216, 0.0, 0.09285380663809216},
    {0.03061334678817873, 5.28338119466832, 0.18442479496982733, 0.11291720409500168,
     0.09285380663809216, 0.0, 0.09285380663809216},
    {0.030613346788178727, 4.98046091108013, 0.17385088233044962, 0.10644314700425817,
     0.09285380663809216, 0.0, 0.09285380663809216},
    {0.03061334678817873, 0.5507841283990175, 0.01922599301658012, 0.011771439831273394,
     0.09285380663809216, 0.0, 0.09285380663809216},
    {0.030613346788178727, 4.122524434194735, 0.14390324974122887, 0.0881072017654826,
     0.09285380663809216, 0.0, 0.09285380663809216},
    {0.04506995001538499, 0.15869019717999347, 0.027696664314299065, 0.02496574552476712,
     0.04650386344422046, 0.0, 0.04650386344422046},
    {0.04506995001538499, 0.29149779799281794, 0.05087596337287991, 0.0458595425240051,
     0.04650386344422046, 0.0, 0.04650386344422046},
    {0.0, 0.0, 0.0, 0.0, 0.08916808638664873, 0.0, 0.08916808638664873},
    {0.030088222626006012, 2.9904318470971956, 0.10438576357668339, 0.06281564186961758,
     -1.7698420334566245, 1.8578975490272869, 0.08805551557066246},
    {0.030088222626006012, 2.990472859925502, 0.10438719519446239, 0.06281650336630658,
     -1.567543441849934, 1.6556001650741818, 0.08805672322424768},
    {0.03008822262600601, 2.9914577969698475, 0.10442157598315978, 0.06283719250279428,
     -0.3016752781201186, 0.3897610035566483, 0.08808572543652972},
    {0.030088222626006012, 3.0282155927117036, 0.10570466510610166, 0.06360930992639592,
     0.021156502272765262, 0.06801158411388347, 0.08916808638664873},
    {0.03558847175536127, 3.4154888082626487, 0.11922305053840106, 0.0848593233334779,
     0.05095749031058861, 0.0496141472705637, 0.10057163758115231},
    {0.04075498931832852, 2.9020928103104504, 0.1013021494767452, 0.0825713603969694,
     0.07186262914900236, 0.025799435046596878, 0.09766206419559924},
    {0.043657798506395104, 1.6511898545201573, 0.05763739907380584, 0.050326439103937984,
     0.061128565049791195, 0.0036988049886222374, 0.06482737003841343},
    {0.04506995001538499, 1.0632660129293705, 0.037114985500340224, 0.03345541082644145,
     0.039954785135178715, 0.0017901051212308155, 0.04174489025640953},
    {0.030088222626006012, 2.990472859925502, 0.10438719519446239, 0.06281650336630658,
     1.7436568882984296, -1.6556001650741818, 0.08805672322424768},
    {0.030613346788178734, 3.153385437530371, 0.11007391693869388, 0.06739461983154833,
     0.1334031292860293, -0.04054932264793714, 0.09285380663809216},
    {0.04506995001538499, 1.0632660129293705, 0.037114985500340224, 0.03345541082644145,
     0.04353175464867717, -0.0017868643922676444, 0.04174489025640953},
    {0.030088222626006012, 3.0282155927117036, 0.10570466510610166, 0.06360930992639592,
     0.08916808638664873, 0.0, 0.08916808638664873},
    {0.030088222626006012, 3.0282155927117036, 0.10570466510610166, 0.06360930992639592,
     0.08882802846607932, 3.4005792056941735E-4, 0.08916808638664873},
    {0.030088222626006012, 3.0282155927117036, 0.10570466510610166, 0.06360930992639592,
     -1.2156961864495148, 1.3048642728361635, 0.08916808638664873},
    {kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN},
    {0.030088222626006012, 3.0282155927117036, 1.0570466510610166, 0.6360930992639592,
     0.021156502272765262, 0.06801158411388347, 0.08916808638664873},
    {kNaN, kNaN, kNaN, kNaN, 0.021156502272765262, 0.06801158411388347, 0.08916808638664873},
    {0.030088222626006012, 3.0282155927117036, 0.10570466510610166, 0.06360930992639592, kNaN, kNaN,
     0.08916808638664873},
}};
// warnings after the rows: [Jagged-edged fin predictions may be inaccurate:  "Freeform Fin Set"]
constexpr DragPins kDragFreeform{
    0.01761917235688354,   0.06181340664665588,   0.019098593171027443, 0.009549296585513721,
    5.8686224556770646E-5, 0.0022540045044529278, 0.011494190749629702, 0.051985419717614834,
    0.05619400604241444,   0.055190943034557335,  0.04716643897170056,  0.04616337596384346,
    0.046479114963033634,  0.04910872524202825,   0.05689747174768614,  0.06155175330602089};
// division, chordLead, chordTrail, chordLength
constexpr std::array<std::array<double, 4>, 8> kChordsFreeform{{
    {0, 2.1863451416390278E-5, 0.08998542436572236, 0.08996356091430598},
    {12, 0.007675855761693057, 0.08488276282553794, 0.07720690706384489},
    {18, 0.01150285191683139, 0.08233143205544573, 0.07082858013861434},
    {19, 0.012140684609354444, 0.08190621026043035, 0.06948183770781349},
    {20, 0.0127785173018775, 0.08148098846541499, 0.06700034350396303},
    {30, 0.019156844227108054, 0.07722877051526128, 0.0421854014654582},
    {46, 0.029362167307476945, 0.07042522179501537, 0.002481494203850465},
    {47, 0.03, 0.07, 0.0},
}};

// ---- SweptCanted: the swept trapezoid, canted
// geometry warnings: []
// interfering fins: 3, NACA model: false
constexpr GeometryPins             kGeometrySweptCanted{0.003900936609408321,  0.06,
                                                        1.8457105872048682,    0.06618012242083796,
                                                        0.01381987757916203,   0.027596574967584078,
                                                        0.9702185050852055,    0.8946655067315761,
                                                        1.2237479396318202E-5, 0.04690993878958101};
constexpr std::array<ForcePins, 8> kForcesSweptCanted{{
    {0.03036490818437152, 4.104364431546404, 0.14326934606446035, 0.08700721077764575,
     0.24132259958596963, 0.0, 0.24132259958596963},
    {0.03086985504302859, 6.7516235401824725, 1.1783806063133981, 0.7275287700482148,
     0.15631889231755886, 0.09502470129099434, 0.2513435936085532},
    {0.0356558902000286, 3.844775836461771, 1.1407679549054082, 0.8135019388763682,
     0.27235027571782344, 0.0, 0.27235027571782344},
    {0.03917333985374905, 7.366943840609095, 0.2571548516562901, 0.20147228797944547,
     0.20194347688218645, 0.07777338738277977, 0.2797168642649662},
    {0.04343691844941417, 3.904025476177899, 0.6813809864104379, 0.5919418067938315,
     0.20976363271675966, 0.0, 0.20976363271675966},
    {0.04482406818746788, 2.9798639353339103, 1.0401687386602425, 0.9324918893635833,
     0.07138286189562451, 0.004196116703701231, 0.07557897859932575},
    {0.03036490818437152, 0.05897215617926682, 0.002058516584657051, 0.001250133341782348,
     3.4485951862673745, -3.210292996008186, 0.23830219025918858},
    {0.04571825134363548, 1.1883762114314782, 0.0, 0.0, 0.06916615093136332, 0.0,
     0.06916615093136332},
}};
// warnings after the rows: []
constexpr DragPins kDragSweptCanted{
    0.026002212139745594, 0.08071545284171104,  0.022918311805232926, 0.011459155902616463,
    7.663200343814657E-5, 0.002943261084511303, 0.015009022503948331, 0.06788214598265799,
    0.07337768440155548,  0.07206789273498772,  0.0615895594024456,   0.06027976773587783,
    0.06069205719995969,  0.06412578130579251,  0.07429626430258235,  0.0803737879987163};

// ---- OnTransition: a freeform fin on the conical boattail
// geometry warnings: []
// interfering fins: 3, NACA model: false
constexpr GeometryPins kGeometryOnTransition{
    0.0016583333333333337, 0.04833333333333333,  2.8174204355108867, 0.040745465382427605,
    0.008072519047177127,  0.016145038094354254, 0.8912853969785655, 0.894484938108927,
    2.5573158951305687E-6, 0.02844525173839093};
constexpr std::array<ForcePins, 8> kForcesOnTransition{{
    {0.01825888539278403, 2.3654047906088858, 0.0825682034771442, 0.030152067287544958, 0.0, 0.0,
     0.0},
    {0.01860349857704794, 4.017888618295822, 0.7012538536766777, 0.26091550138046915,
     -0.019857698727942793, 0.019857698727942793, 0.0},
    {0.021851391948073846, 2.4908096186796858, 0.7390380910671845, 0.3229802198493068, 0.0, 0.0,
     0.0},
    {0.024211151657710563, 4.939393709622965, 0.1724173665704349, 0.0834884602091975,
     -0.01625235540993686, 0.01625235540993686, 0.0},
    {0.02696815850002965, 1.7099387629290947, 0.2984406142059146, 0.16096787573502608, 0.0, 0.0,
     0.0},
    {0.027654513952796732, 1.3051617829785143, 0.45558740768349515, 0.2519809664500342,
     -8.765878313759242E-4, 8.765878313759242E-4, 0.0},
    {0.01825888539278403, 0.033685397983337355, 0.0011758422093077897, 4.293913627949781E-4,
     0.8477352738049512, -0.8477352738049512, 0.0},
    {0.02798154500524881, 0.5205013546322743, 0.0, 0.0, 0.0, 0.0, 0.0},
}};
// warnings after the rows: []
constexpr DragPins kDragOnTransition{
    0.01162742084417293,  0.06499453806528202,   0.018461973398659858, 0.009230986699329929,
    6.170642038330485E-5, 0.0023700033619148304, 0.012085721508229414, 0.054660768981561574,
    0.05908594369571092,  0.05803125960074248,   0.04959378684099496,  0.048539102746026526,
    0.048871090764723915, 0.05163602987173589,   0.059825612176049814, 0.06471941913994886};

// ---- OnTransitionCanted: a freeform fin on the conical boattail: 4 fins, rounded, canted
// geometry warnings: []
// interfering fins: 4, NACA model: false
constexpr GeometryPins kGeometryOnTransitionCanted{0.0012750421337872292, 0.0375,
                                                   2.205809459524364,     0.037559712914457595,
                                                   0.004600165127231944,  0.013798152258280901,
                                                   0.9754974945643908,    0.9400554323410629,
                                                   1.6172922356424667E-6, 0.02338002158446074};
constexpr std::array<ForcePins, 8> kForcesOnTransitionCanted{{
    {0.013990093355846342, 1.7431846334013834, 0.06084862264604892, 0.01702555822785781,
     0.025288944739997796, 0.0, 0.025288944739997796},
    {0.014292052124550498, 2.8967163265214317, 0.5055723739407413, 0.1445133344218762,
     0.014048936814411557, 0.01255836325562331, 0.026607300070034867},
    {0.01714558617714188, 1.6904291562014806, 0.5015604273062311, 0.17199095058846178,
     0.029545341145777934, 0.0, 0.029545341145777934},
    {0.01923018149947414, 3.199426267749445, 0.11168104509404297, 0.042952935344188044,
     0.020245269419648357, 0.01027824043171455, 0.030523509851362908},
    {0.02170926238000938, 1.3642471038465769, 0.23810603772919764, 0.10338212894655129,
     0.019336175812524323, 0.0, 0.019336175812524323},
    {0.022418568671752216, 1.0413023092298344, 0.36348307609361497, 0.16297540604848884,
     0.006412602851519485, 5.543265394340508E-4, 0.006966929390953535},
    {0.013990093355846344, 0.024974045717644684, 8.717586506329797E-4, 2.4391969812244047E-4,
     0.5879934676258539, -0.5630931338668749, 0.02490033375897904},
    {0.022823724048411834, 0.4152736232430479, 0.0, 0.0, 0.00637578991821864, 0.0,
     0.00637578991821864},
}};
// warnings after the rows: []
constexpr DragPins kDragOnTransitionCanted{
    0.008622360308630124, 0.03713051606549613,   0.00954929658551372,  0.00477464829275686,
    3.525205812041057E-5, 0.0013539514322952763, 0.006904412124195582, 0.031226971084611525,
    0.03375501460499648,  0.03315248759429729,   0.0283322715087038,   0.02772974449800461,
    0.027919404842223296, 0.02949897781849261,   0.034177577380421986, 0.03697334427874162};

// ---- OnNoseCone: a freeform fin at the base of the ogive nose cone: 2 fins
// geometry warnings: []
// interfering fins: 2, NACA model: false
constexpr GeometryPins             kGeometryOnNoseCone{7.986197882042433E-4, 0.03,
                                                       2.253888554461486,    0.029293313069908806,
                                                       0.009752731454859113, 0.014629097182288674,
                                                       0.8709102650434513,   0.8320502943378435,
                                                       1.11489369286467E-6,  0.024399387989813517};
constexpr std::array<ForcePins, 8> kForcesOnNoseCone{{
    {0.017076059722336313, 1.1123502219973043, 0.03882834761828563, 0.013260703656989606, 0.0, 0.0,
     0.0},
    {0.017312814364360093, 1.8643808019977652, 0.32539583505833475, 0.11267035374601772,
     -0.008657210909589189, 0.008657210909589189, 0.0},
    {0.019549514684588043, 1.1126274908012828, 0.33012322206565026, 0.12907497554991898, 0.0, 0.0,
     0.0},
    {0.021182531644901224, 2.129864397813547, 0.07434629272570542, 0.03149685396686689,
     -0.007085377273503478, 0.007085377273503478, 0.0},
    {0.023120908164876913, 0.8733400528812031, 0.15242659412318382, 0.070484825692142, 0.0, 0.0,
     0.0},
    {0.023667883374941304, 0.6666028546030726, 0.23268829232033308, 0.11014478730703788,
     -3.8211608472729685E-4, 3.8211608472729685E-4, 0.0},
    {0.017076059722336313, 0.0158982387042317, 5.549532213136802E-4, 1.8952828700510648E-4,
     0.39278053353536835, -0.39278053353536835, 0.0},
    {0.02397556692331209, 0.2658426666698928, 0.0, 0.0, 0.0, 0.0, 0.0},
}};
// warnings after the rows: []
constexpr DragPins kDragOnNoseCone{
    0.005547278172789158,  0.023270901217621116, 0.0076394372684109755, 0.0038197186342054878,
    2.2093610570638624E-5, 8.485653681414567E-4, 0.004327219482338592,  0.01957095770373037,
    0.02115536474329192,   0.020777741482624158, 0.017756755397282074,  0.017379132136614312,
    0.01749799843859158,   0.01848796816139177,  0.021420198568596834,  0.023172396550535164};

// ---- ZeroRadiusTube: a trapezoid on a body tube of radius 0
// geometry warnings: []
// interfering fins: 3, NACA model: false
constexpr GeometryPins             kGeometryZeroRadiusTube{0.00225,
                                                           0.05,
                                                           2.2222222222222228,
                                                           0.04673758865248228,
                                                           0.008841607565011822,
                                                           0.02210401891252956,
                                                           0.9950371902099896,
                                                           0.9284766908852595,
                                                           1.6025350837483026E-6,
                                                           0.03221040189125296};
constexpr std::array<ForcePins, 8> kForcesZeroRadiusTube{{
    {0.02052600472813239, 1.6253170430270683, 0.05673426757920137, 0.023290556891556312, 0.0, 0.0,
     0.0},
    {0.020902442614396336, 2.6987880965489306, 0.4710273809840945, 0.19691245601658872,
     -0.012443773158656881, 0.012443773158656881, 0.0},
    {0.024459449950371637, 1.5719082533520996, 0.46639456752454467, 0.22815509162983647, 0.0, 0.0,
     0.0},
    {0.027057420852725163, 3.271247974212777, 0.11418809559841503, 0.06179270717955058,
     -0.010184410262370821, 0.010184410262370821, 0.0},
    {0.030144992647206152, 1.7400130627796062, 0.3036895697321492, 0.18309439693217677, 0.0, 0.0,
     0.0},
    {0.031024169312378384, 1.3281168897143172, 0.4636002515372249, 0.28765625393904143,
     -5.492185630610934E-4, 5.492185630610934E-4, 0.0},
    {0.02052600472813239, 0.023290407323590025, 8.129885838544927E-4, 3.3374815032229945E-4,
     0.5552035936190116, -0.5552035936190116, 0.0},
    {0.031523725183860304, 0.5296559010705303, 0.0, 0.0, 0.0, 0.0, 0.0},
}};
// warnings after the rows: []
constexpr DragPins kDragZeroRadiusTube{
    0.015516288308076968, 0.07244293961424206,  0.019098593171027443, 0.009549296585513721,
    6.877800225534063E-5, 0.002641606749482507, 0.013470750304214983, 0.06092491622329863,
    0.06585721783112915,  0.06468166649284349,  0.05527725578655825,  0.05410170444827259,
    0.05447173843431294,  0.05755354073230202,  0.06668165263212623,  0.07213629194371586};

// ---- NoseTip: a freeform fin at the tip of the nose cone (body radius 0): 4 fins
// geometry warnings: []
// interfering fins: 4, NACA model: false
constexpr GeometryPins             kGeometryNoseTip{2.892006723637023E-4, 0.02,
                                                    2.7662452976385556,   0.018551033243639898,
                                                    0.006450056713104104, 0.012900113426208209,
                                                    0.7358967767031501,   0.8944271909999153,
                                                    5.55045704881051E-8,  0.015725573334924053};
constexpr std::array<ForcePins, 8> kForcesNoseTip{{
    {0.011087815024014078, 0.2096331153043551, 0.0073175761665478225, 0.001622718619176326, 0.0,
     0.0, 0.0},
    {0.011244217586469582, 0.36026278646628845, 0.06287771796246001, 0.014140214842211344,
     -4.309960458445362E-4, 4.309960458445362E-4, 0.0},
    {0.012718511192093447, 0.23169676096748223, 0.06874581286651041, 0.017486887807045486, 0.0, 0.0,
     0.0},
    {0.013790014568272009, 0.5191299165156381, 0.018121052577602347, 0.004997791580751187,
     -3.5273530674653043E-4, 3.5273530674653043E-4, 0.0},
    {0.015043255234621836, 0.22365019896777205, 0.03903432344728046, 0.011744065810564473, 0.0, 0.0,
     0.0},
    {0.015358121483621779, 0.1707076877702087, 0.05958822420113198, 0.018303263725485523,
     -1.9015130569473223E-5, 1.9015130569473223E-5, 0.0},
    {0.011087815024014078, 0.002976186481250052, 1.0388850650231578E-4, 2.3037930864375226E-5,
     0.020688844668633525, -0.020688844668633525, 0.0},
    {0.01551027960276948, 0.06807859676044442, 0.0, 0.0, 0.0, 0.0, 0.0},
}};
// warnings after the rows: []
constexpr DragPins kDragNoseTip{
    0.0021485674687038887, 0.01792721278987107,  0.005092958178940651,  0.0025464790894703256,
    1.7020262958121596E-5, 6.537096169386032E-4, 0.0033335616752830613, 0.015076885934725608,
    0.01629746617261006,   0.01600655640142897,  0.013679278231980257,  0.013388368460799166,
    0.01347993953787795,   0.014242582879886981, 0.01650148630469681,   0.017851327713004853};

// ---- NoseTipCanted: the fin at the tip of the nose cone, canted: a NaN in the outline
// geometry warnings: []
// interfering fins: 4, NACA model: false
constexpr GeometryPins             kGeometryNoseTipCanted{kNaN,
                                                          0.02,
                                                          kNaN,
                                                          7.333109661287806E-4,
                                                          0.019151748912887068,
                                                          0.008799705677089662,
                                                          0.6370823250694674,
                                                          0.6255049312010381,
                                                          0.0,
                                                          0.01951840439595146};
constexpr std::array<ForcePins, 8> kForcesNoseTipCanted{{
    {kNaN, kNaN, kNaN, kNaN, kNaN, 0.0, kNaN},
    {kNaN, kNaN, kNaN, kNaN, kNaN, 0.0, kNaN},
    {kNaN, kNaN, kNaN, kNaN, kNaN, 0.0, kNaN},
    {kNaN, kNaN, kNaN, kNaN, kNaN, 0.0, kNaN},
    {kNaN, kNaN, kNaN, kNaN, kNaN, 0.0, kNaN},
    {kNaN, kNaN, kNaN, kNaN, kNaN, 0.0, kNaN},
    {kNaN, kNaN, kNaN, kNaN, kNaN, -0.0, kNaN},
    {kNaN, kNaN, kNaN, kNaN, kNaN, 0.0, kNaN},
}};
// warnings after the rows: []
constexpr DragPins kDragNoseTipCanted{kNaN,
                                      0.008767671347552231,
                                      0.005092958178940651,
                                      0.0025464790894703256,
                                      8.324108918372499E-6,
                                      3.1971010470128726E-4,
                                      0.0016303467543036688,
                                      0.007373660499801409,
                                      0.007970610315956573,
                                      0.007828334921816747,
                                      0.00669013176869815,
                                      0.006547856374558326,
                                      0.006592641089180655,
                                      0.006965627467850294,
                                      0.008070390548800769,
                                      0.008730558193268559};

// ---- the swept trapezoid 20 mm ahead of the tube's end, canted, with 1 to 8 fins
// geometry warnings: []
// interfering fins: the fin count, NACA model: true
constexpr GeometryPins kGeometryFinCounts{0.003900936609408309,  0.06,
                                          1.845710587204874,     0.06618012242083826,
                                          0.013819877579162018,  0.027596574967584046,
                                          0.9702185050852061,    0.8946655067315761,
                                          1.2237479396318236E-5, 0.04690993878958115};
constexpr std::array<std::array<ForcePins, 2>, 8> kForcesByFinCount{{
    {{
        {0.028580248907071174, 4.104364431546402, 0.14326934606446026, 0.08189347142551183,
         0.2413225995859694, 0.0, 0.2413225995859694},
        {0.04883211195479951, 3.2264443895140453, 0.563120777295185, 0.5499675368190438,
         0.12456401720560326, 0.004196116703701241, 0.1287601339093045},
    }},
    {{
        {0.028580248907071174, 4.104364431546402, 0.14326934606446026, 0.08189347142551183,
         0.2413225995859694, 0.0, 0.2413225995859694},
        {0.04883211195479951, 3.2264443895140453, 0.563120777295185, 0.5499675368190438,
         0.12456401720560326, 0.004196116703701241, 0.1287601339093045},
    }},
    {{
        {0.028580248907071174, 4.104364431546402, 0.14326934606446026, 0.08189347142551183,
         0.2413225995859694, 0.0, 0.2413225995859694},
        {0.04883211195479951, 3.2264443895140453, 0.563120777295185, 0.5499675368190438,
         0.12456401720560326, 0.004196116703701241, 0.1287601339093045},
    }},
    {{
        {0.028580248907071174, 4.104364431546402, 0.14326934606446026, 0.08189347142551183,
         0.2413225995859694, 0.0, 0.2413225995859694},
        {0.04883211195479951, 3.2264443895140453, 0.563120777295185, 0.5499675368190438,
         0.12456401720560326, 0.004196116703701241, 0.1287601339093045},
    }},
    {{
        {0.028580248907071178, 3.890937481105989, 0.13581934006910834, 0.07763501091138524,
         0.2413225995859694, 0.0, 0.2413225995859694},
        {0.048832111954799515, 3.058669281259315, 0.5338384968758354, 0.5213692249044536,
         0.12456401720560326, 0.004196116703701241, 0.1287601339093045},
    }},
    {{
        {0.028580248907071174, 3.7472847260018654, 0.13080491295685223, 0.07476873941149233,
         0.2413225995859694, 0.0, 0.2413225995859694},
        {0.04883211195479952, 2.945743727626324, 0.514129269670504, 0.5021203611157873,
         0.12456401720560326, 0.004196116703701241, 0.1287601339093045},
    }},
    {{
        {0.028580248907071178, 3.505127224540627, 0.12235202153904905, 0.06993702459738711,
         0.2413225995859694, 0.0, 0.2413225995859694},
        {0.048832111954799515, 2.7553835086449947, 0.480905143810088, 0.46967227644346354,
         0.12456401720560326, 0.004196116703701241, 0.1287601339093045},
    }},
    {{
        {0.02858024890707118, 3.3245351895525856, 0.11604817031221282, 0.0663337118546646,
         0.2413225995859694, 0.0, 0.2413225995859694},
        {0.048832111954799515, 2.6134199555063775, 0.4561278296091, 0.4454737048234257,
         0.12456401720560326, 0.004196116703701241, 0.1287601339093045},
    }},
}};

// ---- Overlap: a second set of 4 fins that overlaps the first axially
// interfering fins: 8 for the first set, 8 for the second
constexpr PairPins kPairOverlap{
    {{{
         {0.028567756199827002, 3.3242604745814037, 0.11603858095070953, 0.06629923780747518, 0.0,
          0.0, 0.0},
         {0.04607148866517872, 2.4477218605234023, 0.4272080563917478, 0.39364222255450876,
          -0.004197036709069692, 0.004197036709069692, 0.0},
     }},
     {{
         {0.013668034175166922, 1.106823599345101, 0.03863543209469313, 0.010561408124852133, 0.0,
          0.0, 0.0},
         {0.028888798105478824, 0.9812153453310504, 0.17125438447119998, 0.09894666675333087,
          -6.611487041839017E-4, 6.611487041839017E-4, 0.0},
     }}}};

// ---- TenFins: a second set of 6 fins that overlaps the first
// interfering fins: 10 for the first set, 10 for the second
constexpr PairPins kPairTenFins{
    {{{
         {0.028567756199827, 3.078018957945744, 0.10744313050991623, 0.061388183155069616, 0.0, 0.0,
          0.0},
         {0.04607148866517871, 2.2664091301142615, 0.3955630151775443, 0.36448353940232286,
          -0.004197036709069692, 0.004197036709069692, 0.0},
     }},
     {{
         {0.01366803417516692, 1.024836666060279, 0.035773548235826985, 0.00977908159708531, 0.0,
          0.0, 0.0},
         {0.028888798105478817, 0.90853272715838, 0.15856887451037036, 0.0916172840308619,
          -6.611487041839017E-4, 6.611487041839017E-4, 0.0},
     }}}};

// ---- Apart: a second set of 4 fins well ahead of the first
// interfering fins: 4 for the first set, 4 for the second
constexpr PairPins kPairApart{
    {{{
         {0.028567756199827005, 4.104025277260992, 0.14325750734655496, 0.08185091087342615, 0.0,
          0.0, 0.0},
         {0.04607148866517872, 3.0218788401523486, 0.527417353570059, 0.4859780525364306,
          -0.004197036709069692, 0.004197036709069692, 0.0},
     }},
     {{
         {0.01366803417516692, 1.3664488880803718, 0.047698064314435976, 0.013038775462780412, 0.0,
          0.0, 0.0},
         {0.03972497921895785, 1.4542603717503342, 0.25381631668320065, 0.2016569581134514,
          -6.611487041839017E-4, 6.611487041839017E-4, 0.0},
     }}}};

// ---- Overlap4mm: a second set that overlaps the first by 4 mm
// interfering fins: 4 for the first set, 4 for the second
constexpr PairPins kPairOverlap4mm{
    {{{
         {0.028567756199827005, 4.104025277260992, 0.14325750734655496, 0.08185091087342615, 0.0,
          0.0, 0.0},
         {0.04607148866517872, 3.0218788401523486, 0.527417353570059, 0.4859780525364306,
          -0.004197036709069692, 0.004197036709069692, 0.0},
     }},
     {{
         {0.01366803417516692, 1.3664488880803718, 0.047698064314435976, 0.013038775462780412, 0.0,
          0.0, 0.0},
         {0.03929873849104673, 1.4474038132066256, 0.252619621463766, 0.19855264883223508,
          -6.611487041839017E-4, 6.611487041839017E-4, 0.0},
     }}}};

// ---- Overlap6mm: a second set that overlaps the first by 6 mm
// interfering fins: 8 for the first set, 8 for the second
constexpr PairPins kPairOverlap6mm{
    {{{
         {0.028567756199827002, 3.3242604745814037, 0.11603858095070953, 0.06629923780747518, 0.0,
          0.0, 0.0},
         {0.04607148866517872, 2.4477218605234023, 0.4272080563917478, 0.39364222255450876,
          -0.004197036709069692, 0.004197036709069692, 0.0},
     }},
     {{
         {0.013668034175166922, 1.106823599345101, 0.03863543209469313, 0.010561408124852133, 0.0,
          0.0, 0.0},
         {0.03909544758640705, 1.169622666730802, 0.20413766540408837, 0.15961706796434072,
          -6.611487041839017E-4, 6.611487041839017E-4, 0.0},
     }}}};

// ---- ShortRoot: a second set with a root chord of 6 mm inside the first
// interfering fins: 8 for the first set, 4 for the second
constexpr PairPins kPairShortRoot{
    {{{
         {0.028567756199827002, 3.3242604745814037, 0.11603858095070953, 0.06629923780747518, 0.0,
          0.0, 0.0},
         {0.04607148866517872, 2.4477218605234023, 0.4272080563917478, 0.39364222255450876,
          -0.004197036709069692, 0.004197036709069692, 0.0},
     }},
     {{
         {0.011353427895981087, 0.9488955263102701, 0.03312270238311739, 0.007521124264535284, 0.0,
          0.0, 0.0},
         {0.016213522396490203, 0.41584019291421226, 0.07257780528481397, 0.023534837429468727,
          -3.6980205613092984E-4, 3.6980205613092984E-4, 0.0},
     }}}};

// ---- NaNHeight: the swept trapezoid with a NaN height
// geometry warnings: []
// interfering fins: 3, NACA model: false
constexpr GeometryPins kGeometryNaNHeight{kNaN, kNaN, kNaN, 0.0, 0.0, 0.0, kNaN, kNaN, kNaN, 0.0};
constexpr std::array<ForcePins, 2> kForcesNaNHeight{{
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
}};
// warnings after the rows: []
// square: friction 0.0, pressure NaN, base NaN
// airfoil: friction 0.0, pressure NaN, base NaN

// ---- the limits that the cases above leave open, from the probe ProbeFinCalcFix (which uses
// ProbeFinCalc's rockets, helpers and table formats)

/// What Java gives for a fin without area: no forces, no drag and no single-fin CNa.
constexpr std::array<ForcePins, 2> kNoForces{};
constexpr DragPins                 kNoDrag{};
constexpr SingleFinPins            kNoSingleFinCNa{};

// ---- AreaBelowLimit: the swept trapezoid 0.15 um high: an area below the 1e-8 limit
// geometry warnings: [Fins with zero area will not affect aerodynamics:  "Trapezoidal Fin Set"]
// interfering fins: 3, NACA model: false
constexpr GeometryPins kGeometryAreaBelowLimit{
    9.74999999963852E-9, 1.5E-7, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
// forces, drag and single-fin CNa: 0.0 in every place

// ---- AreaAboveLimit: the swept trapezoid 0.16 um high: an area above the limit
// geometry warnings: []
// interfering fins: 3, NACA model: false
constexpr GeometryPins kGeometryAreaAboveLimit{
    1.0399999999643333E-8, 1.6E-7, 4.92307692324576E-6, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
// forces: 0.0 in every place; warnings after the rows: []
constexpr DragPins      kDragAreaAboveLimit{0.0,
                                            2.6890819184806637E-7,
                                            6.11154981472878E-8,
                                            3.05577490736439E-8,
                                            2.5530394437182424E-10,
                                            9.80564425407906E-9,
                                            5.000342512924598E-8,
                                            2.2615328902088435E-7,
                                            2.444619925891512E-7,
                                            2.400983460214348E-7,
                                            2.0518917347970408E-7,
                                            2.0082552691198772E-7,
                                            2.0219909306816947E-7,
                                            2.1363874319830494E-7,
                                            2.475222945704524E-7,
                                            2.677699156950731E-7};
constexpr SingleFinPins kSingleFinAreaAboveLimit{4.095999999994354E-11, 4.095999999998821E-11,
                                                 3.3873345745087094E-6, 9.474982977644517E-6,
                                                 6.1160752130026635E-6};

// ---- HalfMillimetreLess: the swept trapezoid 0.49 mm high: no chord is found
// geometry warnings: []
// interfering fins: 3, NACA model: false
constexpr GeometryPins kGeometryHalfMillimetreLess{
    3.1849999999999934E-5, 4.9E-4, 0.015076923076923108, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
// forces: 0.0 in every place; warnings after the rows: []
constexpr DragPins      kDragHalfMillimetreLess{0.0,
                                                8.235313375347032E-4,
                                                1.871662130760689E-4,
                                                9.358310653803445E-5,
                                                7.818683296387117E-7,
                                                3.0029785528117122E-5,
                                                1.5313548945831583E-4,
                                                6.925944476264583E-4,
                                                7.486648523042756E-4,
                                                7.353011846906442E-4,
                                                6.283918437815937E-4,
                                                6.150281761679624E-4,
                                                6.19234722521269E-4,
                                                6.542686510448089E-4,
                                                7.580370271220105E-4,
                                                8.200453668161614E-4};
constexpr SingleFinPins kSingleFinHalfMillimetreLess{3.841550335306321E-4, 3.841589630236432E-4,
                                                     0.010637736655455976, 0.02901713537003141,
                                                     0.018730480340462978};

// ---- HalfMillimetreMore: the swept trapezoid 0.51 mm high
// geometry warnings: []
// interfering fins: 3, NACA model: false
constexpr GeometryPins kGeometryHalfMillimetreMore{3.3149999999999586E-5, 5.1E-4,
                                                   0.01569230769230789,   0.06620294599018006,
                                                   0.013797054009819966,  2.3454991816693944E-4,
                                                   0.03398036502188705,   0.016997544032318183,
                                                   2.1559225971715714E-8, 0.04689852700491};
constexpr std::array<ForcePins, 2> kForcesHalfMillimetreMore{{
    {0.030347790507364983, 8.061015745818446E-4, 2.813825316392764E-5, 1.7078676245241518E-5, 0.0,
     0.0, 0.0},
    {0.03034779050736498, 0.03475008321784132, 0.00606503367493357, 0.0036812074277399586,
     -7.387224644994738E-6, 7.387224644994738E-6, 0.0},
}};
// warnings after the rows: []
constexpr DragPins kDragHalfMillimetreMore{
    2.2095940453450793E-4, 2.476432960654779E-7,  1.948056503444799E-4,  9.740282517223995E-5,
    2.351148540631957E-10, 9.030227180649254E-9,  4.6049221961100563E-8, 2.082693930753273E-7,
    2.2513026915043443E-7, 2.2111169384609915E-7, 1.8896309141141715E-7, 1.849445161070819E-7,
    1.8620946251106885E-7, 1.967444805950967E-7,  2.2794856659387318E-7, 2.4659503324974616E-7};
constexpr SingleFinPins kSingleFinHalfMillimetreMore{4.1123109422284217E-4, 4.1511137525210415E-4,
                                                     0.011083910927275342, 0.03020150824227728,
                                                     0.019494989742114326};

// ---- SpanBelowLimit: a 3 m chord 5 nm high: a span below the limit, an area above it
// geometry warnings: []
// interfering fins: 3, NACA model: false
constexpr GeometryPins kGeometrySpanBelowLimit{
    1.4999999992104662E-8, 5.0E-9, 3.333333335087853E-9, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
// single-fin CNa: 0.0 in every place

// ---- SpanAboveLimit: the same 20 nm high
// geometry warnings: []
// interfering fins: 3, NACA model: false
constexpr GeometryPins kGeometrySpanAboveLimit{
    5.999999999617422E-8, 2.0E-8, 1.3333333334183507E-8, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
constexpr SingleFinPins kSingleFinSpanAboveLimit{6.4E-13, 6.4E-13, 1.9542152830816142E-5,
                                                 5.4663363330953704E-5, 3.5285049304744815E-5};

// ---- SweepCosineBelowLimit: a sweep of 10000 km: a midchord sweep cosine below the limit
// geometry warnings: []
// interfering fins: 3, NACA model: false
constexpr GeometryPins kGeometrySweepCosineBelowLimit{0.0038999999891966585, 0.06,
                                                      1.846153851267854,     0.0662029462429208,
                                                      4599018.0155231785,    0.027594108093139075,
                                                      6.0000000089999955E-9, 6.0000000000000024E-9,
                                                      1.2240163062517182E-5, 4599018.0486246515};
// single-fin CNa and forces: 0.0 in every place; warnings after the rows: []

// ---- SweepCosineAboveLimit: a sweep of 5000 km
// geometry warnings: []
// interfering fins: 3, NACA model: false
constexpr GeometryPins  kGeometrySweepCosineAboveLimit{0.0038999999891966585, 0.06,
                                                       1.846153851267854,     0.06620294586946765,
                                                       2299508.9998483094,    0.027594107998179716,
                                                       1.2000000036E-8,       1.1999999999999992E-8,
                                                       1.2240162934139377E-5, 2299509.0329497824};
constexpr SingleFinPins kSingleFinSweepCosineAboveLimit{1.5699113504414012E-7, 3.435729928768083E-7,
                                                        1.270240382289586, 3.553118606896098,
                                                        2.2935281986013814};
constexpr std::array<ForcePins, 2> kForcesSweepCosineAboveLimit{{
    {2299509.016399046, 2.6291940955489213E-7, 9.177618728264613E-9, 0.42208034029434444, 0.0, 0.0,
     0.0},
    {2299509.030863786, 3.0373551655898177, 0.10602369638399979, 4876048.946411354, 0.0, 0.0, 0.0},
}};
// warnings after the rows: []

// ---- Sliver: a freeform fin of two thin strokes: no area, but chords from one stroke to the
// other
// fin points (5): (0.0, 0.0) (0.05, 0.03) (0.1, 5.0E-4) (0.05, 0.0299999)
//                 (1.0000000000287557E-7, 0.0)
// geometry warnings: [Fins with zero area will not affect aerodynamics:  "Freeform Fin Set" |
//                     Jagged-edged fin predictions may be inaccurate:  "Freeform Fin Set"]
// interfering fins: 3, NACA model: false
constexpr GeometryPins kGeometrySliver{8.999994999692867E-9,
                                       0.03,
                                       0.0,
                                       0.06790031849527253,
                                       0.016317770242575542,
                                       0.00979066214554533,
                                       0.9964673218221082,
                                       0.5144957554275262,
                                       1.2018230418278674E-11,
                                       0.050267929490211805};
// forces, drag and single-fin CNa: 0.0 in every place

// ---- OnDivisions: a freeform fin with outline points on chord divisions (span 47 mm)
// fin points (5): (0.0, 0.0) (0.01, 0.011) (0.025, 0.047) (0.045, 0.022) (0.06, 0.0)
// geometry warnings: []
// division, chordLead, chordTrail, chordLength
constexpr std::array<std::array<double, 4>, 6> kChordsOnDivisions{{
    {10, 0.009090909090909092, 0.05318181818181818, 0.04409090909090908},
    {11, 0.01, 0.0525, 0.0325},
    {12, 0.010416666666666668, 0.05181818181818181, 0.04140151515151515},
    {21, 0.014166666666666666, 0.04568181818181818, 0.03151515151515151},
    {22, 0.014583333333333334, 0.045, 0.030416666666666665},
    {23, 0.015, 0.044199999999999996, 0.029199999999999997},
}};

// ---- NearlyRectangular: four canted fins with a root chord of 30 mm and a tip chord within and
// beyond MathUtil.equals of it
// tip chord 0.03000000015: interfering fins: 4, NACA model: false
constexpr std::array<ForcePins, 2> kForcesTipWithinEquals{{
    {0.014769159080555046, 1.0832937089286347, 0.03781408397389154, 0.011169644433917424,
     0.1337698715293094, 0.0, 0.1337698715293094},
    {0.014918990936339034, 0.5479902813510653, 0.0956423467850617, 0.0285377460963306,
     0.06525809624445277, 0.0010059223968402033, 0.06626401864129297},
}};
// warnings after the rows: []
// tip chord 0.0300000006: interfering fins: 4, NACA model: false
constexpr std::array<ForcePins, 2> kForcesTipBeyondEquals{{
    {0.014769159186812184, 1.08329372923962, 0.037814084682878675, 0.01116964472370063,
     0.12781150474588301, 0.0, 0.12781150474588301},
    {0.014918991046701056, 0.5479902916254926, 0.09564234857828754, 0.028537746842498667,
     0.061813943811512596, 0.001005922407414668, 0.06281986621892727},
}};
// warnings after the rows: []

// ---- SweptSmallCNa: the swept trapezoid nearly in line with the airflow: CNa against the 1e-8
// limit
constexpr std::array<ForcePins, 2> kForcesSweptSmallCNa{{
    {0.030347790507364983, 7.3872454968536125E-9, 2.578635131464399E-10, 1.565117575292263E-10, 0.0,
     0.0, 0.0},
    {0.028567756199827002, 1.3132880880230977E-8, 4.5842402326448325E-10, 2.6192291465527157E-10,
     0.0, 0.0, 0.0},
}};
// warnings after the rows: []

// ---- Mixed: a trapezoidal, an elliptical and a freeform fin set and a set of tube fins on one
// tube
// MixedTrapezoid: interfering fins 7, NACA model true, geometry warnings []
constexpr std::array<ForcePins, 2> kForcesMixedTrapezoid{{
    {0.028567756199827005, 3.504837586780887, 0.12234191127395794, 0.06990067788590594, 0.0, 0.0,
     0.0},
    {0.04607148866517872, 2.5806845294901057, 0.4504144199488304, 0.41502525686611175,
     -0.004197036709069692, 0.004197036709069692, 0.0},
}};
// warnings after the rows: []
// MixedElliptical: interfering fins 7, NACA model false, geometry warnings []
constexpr std::array<ForcePins, 2> kForcesMixedElliptical{{
    {0.017237851027718146, 1.8321671013469523, 0.0639546967304499, 0.022048830695247764, 0.0, 0.0,
     0.0},
    {0.028225886288366364, 1.1778758039029094, 0.20557810957680844, 0.1160524868858463,
     -0.0012105391037849082, 0.0012105391037849082, 0.0},
}};
// warnings after the rows: []
// MixedFreeform: interfering fins 7, NACA model false, geometry warnings []
constexpr std::array<ForcePins, 2> kForcesMixedFreeform{{
    {0.0218049645390071, 1.2088968664837094, 0.04219846127436603, 0.018402719033764312, 0.0, 0.0,
     0.0},
    {0.03215779348073151, 0.9864653465242699, 0.1721706825367531, 0.11073258504906987,
     -7.935789987534346E-4, 7.935789987534346E-4, 0.0},
}};
// warnings after the rows: []

// NOLINTEND(modernize-use-std-numbers)

// ---- the helpers that compare with the tables

/// Expects @p actual to be the Java-pinned @p expected: NaN for NaN, exactly 0 for 0 (either
/// sign), otherwise within a relative 1e-12 (the values went through sin, atan2 or pow, which
/// may differ in the last bit between math libraries).
void expectPinned(double actual, double expected)
{
    if (std::isnan(expected))
    {
        EXPECT_TRUE(std::isnan(actual)) << actual;
        return;
    }
    if (expected == 0)
    {
        EXPECT_EQ(actual, 0.0);
        return;
    }
    EXPECT_NEAR(actual, expected, 1e-12 * std::abs(expected));
}

/// Flight conditions set as ProbeFinCalc.cond() sets them on OpenRocket's
/// `new FlightConditions(null)`: the reference length kRefLength, the standard atmosphere, and
/// the Mach number, angle of attack (in degrees), theta and roll rate given.
[[nodiscard]] FlightConditions conditionsFor(double mach, double aoaDegrees, double theta,
                                             double rollRate)
{
    FlightConditions conditions;
    conditions.setRefLength(kRefLength);
    conditions.setMach(mach);
    conditions.setAOA(javaToRadians(aoaDegrees));
    conditions.setTheta(theta);
    conditions.setRollRate(rollRate);
    return conditions;
}

/// The flight conditions of @p row (its rotation is the instance's, not a flight condition).
[[nodiscard]] FlightConditions conditionsOf(const Condition& row)
{
    return conditionsFor(row[0], row[1], row[2], row[4]);
}

/// The flight conditions at @p mach and nothing else (ProbeFinCalc.condMach()).
[[nodiscard]] FlightConditions conditionsAtMach(double mach)
{
    return conditionsFor(mach, 0, 0, 0);
}

/// Checks calculateNonaxialForces() of @p calc for @p row against @p pins; the warnings go to
/// @p warnings.
void expectForceRow(FinSetCalc& calc, const Condition& row, const ForcePins& pins,
                    WarningSet& warnings)
{
    SCOPED_TRACE(::testing::Message()
                 << "Mach " << row[0] << " angle of attack " << row[1] << " theta " << row[2]
                 << " rotation " << row[3] << " roll rate " << row[4]);
    AerodynamicForces forces;
    calc.calculateNonaxialForces(conditionsOf(row), Transformation::rotateX(row[3]), forces,
                                 warnings);
    const Coordinate cp = forces.getCP();
    expectPinned(cp.x, pins[0]);
    expectPinned(cp.weight, pins[1]);
    expectPinned(forces.getCN(), pins[2]);
    expectPinned(forces.getCm(), pins[3]);
    expectPinned(forces.getCroll(), pins[4]);
    expectPinned(forces.getCrollDamp(), pins[5]);
    expectPinned(forces.getCrollForce(), pins[6]);
    // What Java gives in every row (the probe checks it): no side force, no yaw moment, and a
    // CP on the axis, its y and z NaN where CNa is NaN
    const double onAxis = std::isnan(pins[1]) ? kNaN : 0.0;
    expectPinned(forces.getCside(), 0.0);
    expectPinned(forces.getCyaw(), 0.0);
    expectPinned(cp.y, onAxis);
    expectPinned(cp.z, onAxis);
}

/// The text of every warning of @p warnings (Message::toString(), with the sources), in order.
[[nodiscard]] std::vector<std::string> textsOf(const WarningSet& warnings)
{
    std::vector<std::string> texts;
    for (const Warning& warning : warnings)
    {
        texts.push_back(warning.toString());
    }
    return texts;
}

/// No warning texts: what textsOf() gives for an empty set.
[[nodiscard]] std::vector<std::string> noWarnings()
{
    return {};
}

/// Checks calculateNonaxialForces() of @p calc for every row of @p conditions against @p pins,
/// in order and with the one calculator (as the probe does), and returns the text of the
/// warnings the calls added.
template <std::size_t N>
[[nodiscard]] std::vector<std::string> expectForces(FinSetCalc&                     calc,
                                                    const std::array<Condition, N>& conditions,
                                                    const std::array<ForcePins, N>& pins)
{
    WarningSet                       warnings;
    const std::span<const Condition> rows{conditions};
    const std::span<const ForcePins> expected{pins};
    for (std::size_t i = 0; i < N; i++)
    {
        SCOPED_TRACE(::testing::Message() << "row " << i);
        expectForceRow(calc, rows[i], expected[i], warnings);
    }
    return textsOf(warnings);
}

/// The geometry @p calc derived at construction, in the order of GeometryPins.
[[nodiscard]] GeometryPins geometryOf(const FinSetCalc& calc)
{
    return GeometryPins{calc.getFinArea(),    calc.getSpan(),         calc.getAspectRatio(),
                        calc.getMACLength(),  calc.getMACLead(),      calc.getMACSpan(),
                        calc.getCosGamma(),   calc.getCosGammaLead(), calc.getRollSum(),
                        calc.getMidchordPos()};
}

/// Checks the geometry @p calc derived at construction against @p pins, within the tolerance of
/// expectPinned(): the outline of an elliptical fin, of a canted fin and of a fin on a nose cone
/// goes through sin, cos or pow. (The tests of an outline that is made with sums, products,
/// quotients and square roots only compare geometryOf() with the pins exactly.)
void expectGeometry(const FinSetCalc& calc, const GeometryPins& pins)
{
    const GeometryPins            geometry = geometryOf(calc);
    const std::span<const double> actual{geometry};
    const std::span<const double> expected{pins};
    for (std::size_t i = 0; i < expected.size(); i++)
    {
        SCOPED_TRACE(::testing::Message() << "geometry value " << i);
        expectPinned(actual[i], expected[i]);
    }
}

/// Checks the three drag coefficients of @p calc at @p mach, for kDragCf, kDragStagnation and
/// kDragBase; none adds a warning.
void expectDragRow(FinSetCalc& calc, double mach, double friction, double pressure, double base)
{
    SCOPED_TRACE(::testing::Message() << "Mach " << mach);
    WarningSet             warnings;
    const FlightConditions conditions = conditionsAtMach(mach);
    expectPinned(calc.calculateFrictionCD(conditions, kDragCf, warnings), friction);
    expectPinned(calc.calculatePressureCD(conditions, kDragStagnation, kDragBase, warnings),
                 pressure);
    expectPinned(calc.calculateComponentBaseCD(conditions, kDragBase, warnings), base);
    EXPECT_TRUE(warnings.empty());
}

/// Checks the drag of @p fins with each of the three cross-sections against @p pins, at every
/// Mach number of kDragMach; @p fins has its own cross-section again afterwards.
void expectDrag(FinSet& fins, const DragPins& pins)
{
    const FinSet::CrossSection original = fins.getCrossSection();
    fins.setCrossSection(FinSet::CrossSection::SQUARE);
    FinSetCalc square{fins};
    fins.setCrossSection(FinSet::CrossSection::ROUNDED);
    FinSetCalc rounded{fins};
    fins.setCrossSection(FinSet::CrossSection::AIRFOIL);
    FinSetCalc airfoil{fins};
    fins.setCrossSection(original);

    const double                  friction        = pins[0];
    const double                  pressureSquare  = pins[1];
    const double                  baseSquare      = pins[2];
    const double                  baseRounded     = pins[3];
    const std::span<const double> pressureRounded = std::span<const double>{pins}.subspan(4);
    const std::span<const double> machs{kDragMach};
    for (std::size_t i = 0; i < machs.size(); i++)
    {
        expectDragRow(square, machs[i], friction, pressureSquare, baseSquare);
        expectDragRow(rounded, machs[i], friction, pressureRounded[i], baseRounded);
        expectDragRow(airfoil, machs[i], friction, pressureRounded[i], 0.0);
    }
}

/// Checks calculateFinCNa1() of @p calc at each Mach number of kSingleFinMach, at an angle of
/// attack of 0, against @p pins.
void expectSingleFinCNa(const FinSetCalc& calc, const SingleFinPins& pins)
{
    const std::span<const double> machs{kSingleFinMach};
    const std::span<const double> expected{pins};
    for (std::size_t i = 0; i < machs.size(); i++)
    {
        SCOPED_TRACE(::testing::Message() << "Mach " << machs[i]);
        expectPinned(calc.calculateFinCNa1(conditionsAtMach(machs[i])), expected[i]);
    }
}

// ---- the rockets, built call for call as ProbeFinCalc builds them

/// ProbeFinCalc.Base: an ogive nose cone, a body tube and a conical boattail in one stage. The
/// rocket's events are off until enableEvents(), as for every new Rocket.
struct Base
{
    Rocket      rocket;
    AxialStage& stage{rocket.addChild(std::make_unique<AxialStage>())};
    NoseCone& nose{stage.addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.10, 0.025))};
    BodyTube& tube{stage.addChild(std::make_unique<BodyTube>(0.40, 0.025, 0.001))};
    Transition& tail{stage.addChild(std::make_unique<Transition>())};

    Base()
    {
        tail.setShapeType(TransitionShape::CONICAL);
        tail.setLength(0.06);
        tail.setForeRadius(0.025);
        tail.setAftRadius(0.015);
    }
};

/// ProbeFinCalc.sweptTrapezoid(): three swept trapezoidal fins at the end of the body tube, with
/// a root chord of 80 mm, a tip chord of 50 mm, a sweep of 30 mm and a height of 60 mm, 3 mm
/// thick.
TrapezoidFinSet& addSweptTrapezoid(Base& base)
{
    TrapezoidFinSet& fins =
        base.tube.addChild(std::make_unique<TrapezoidFinSet>(3, 0.08, 0.05, 0.03, 0.06));
    fins.setThickness(0.003);
    fins.setAxialMethod(AxialMethod::BOTTOM);
    fins.setAxialOffset(0.0);
    return fins;
}

// ---- the four planforms, over the whole table of conditions

/// A swept trapezoid on the body tube: 3 fins, square, no cant. Java makes a NACA 1307 model.
TEST(FinSetCalc, SweptTrapezoidIsJavas)
{
    Base             base;
    TrapezoidFinSet& fins = addSweptTrapezoid(base);
    base.rocket.enableEvents();

    FinSetCalc calc{fins};
    EXPECT_EQ(geometryOf(calc), kGeometrySwept);  // exactly: arithmetic and square roots only
    EXPECT_EQ(calc.getInterferenceFinCount(), 3);
    EXPECT_TRUE(calc.usesNacaInterference());
    EXPECT_TRUE(calc.getGeometryWarnings().empty());
    EXPECT_EQ(expectForces(calc, kFullConditions, kForcesSwept), noWarnings());
    expectDrag(fins, kDragSwept);
}

/// One row of kSingleFinSwept: the Mach number, the angle of attack in degrees, and the pinned
/// calculateFinCNa1() and calculateCPPos().
void expectSingleFin(const FinSetCalc& calc, const std::array<double, 4>& row)
{
    SCOPED_TRACE(::testing::Message() << "Mach " << row[0] << " angle of attack " << row[1]);
    const FlightConditions conditions = conditionsFor(row[0], row[1], 0, 0);
    expectPinned(calc.calculateFinCNa1(conditions), row[2]);
    expectPinned(calc.calculateCPPos(conditions), row[3]);
}

/// The single-fin CNa and the CP position along the MAC of the swept trapezoid: the angle of
/// attack counts only from Mach 1.5 (and through the transonic interpolation), at most the stall
/// angle and mirrored beyond 90 degrees, and the K coefficients are held beyond Mach 4.9.
TEST(FinSetCalc, SingleFinCNaAndCPPositionAreJavas)
{
    Base                   base;
    const TrapezoidFinSet& fins = addSweptTrapezoid(base);
    base.rocket.enableEvents();

    const FinSetCalc calc{fins};
    for (const std::array<double, 4>& row : kSingleFinSwept)
    {
        expectSingleFin(calc, row);
    }

    // The ends of the regimes belong to the formulas, not to the interpolations between them: at
    // Mach 1.5 and an angle of attack of 0 the single-fin CNa is fin area * K1 / reference area,
    // and at Mach 2 the CP position is the empirical one. Both are sums, products, quotients and
    // square roots only, so these are Java's doubles to the last bit, which the interpolations
    // do not reach.
    EXPECT_EQ(calc.calculateFinCNa1(conditionsAtMach(1.5)), 3.5531186167385465);
    EXPECT_EQ(calc.calculateCPPos(conditionsAtMach(2.0)), 0.4684908868950161);
}

/// A rectangular fin in the middle of the body tube: 4 fins, rounded, canted. Java makes a NACA
/// 1307 model, and uses chart 3 for the cant above beta * A = 2.
TEST(FinSetCalc, RectangularFinIsJavas)
{
    Base             base;
    TrapezoidFinSet& fins =
        base.tube.addChild(std::make_unique<TrapezoidFinSet>(4, 0.06, 0.06, 0.0, 0.05));
    fins.setThickness(0.002);
    fins.setCrossSection(FinSet::CrossSection::ROUNDED);
    fins.setCantAngle(0.04);
    fins.setAxialMethod(AxialMethod::MIDDLE);
    fins.setAxialOffset(0.05);
    base.rocket.enableEvents();

    FinSetCalc calc{fins};
    expectGeometry(calc, kGeometryRectangular);
    EXPECT_EQ(calc.getInterferenceFinCount(), 4);
    EXPECT_TRUE(calc.usesNacaInterference());
    EXPECT_TRUE(calc.getGeometryWarnings().empty());
    EXPECT_EQ(expectForces(calc, kFullConditions, kForcesRectangular), noWarnings());
    expectDrag(fins, kDragRectangular);
}

/// An elliptical fin on the body tube: 3 fins, airfoil, no cant; no NACA model.
TEST(FinSetCalc, EllipticalFinIsJavas)
{
    Base              base;
    EllipticalFinSet& fins = base.tube.addChild(std::make_unique<EllipticalFinSet>());
    fins.setFinCount(3);
    fins.setHeight(0.05);
    fins.setLength(0.07);
    fins.setThickness(0.004);
    fins.setCrossSection(FinSet::CrossSection::AIRFOIL);
    fins.setAxialMethod(AxialMethod::BOTTOM);
    fins.setAxialOffset(-0.01);
    base.rocket.enableEvents();

    FinSetCalc calc{fins};
    expectGeometry(calc, kGeometryElliptical);
    EXPECT_EQ(calc.getInterferenceFinCount(), 3);
    EXPECT_FALSE(calc.usesNacaInterference());
    EXPECT_TRUE(calc.getGeometryWarnings().empty());
    EXPECT_EQ(expectForces(calc, kFullConditions, kForcesElliptical), noWarnings());
    expectDrag(fins, kDragElliptical);
}

/// The freeform fin of the pinned case: a notch in the tip, so that the outline is concave and a
/// chord between the notch's sides crosses it twice. 4 fins, square, canted.
FreeformFinSet& addNotchedFreeform(Base& base)
{
    FreeformFinSet& fins = base.tube.addChild(std::make_unique<FreeformFinSet>());
    fins.setFinCount(4);
    const std::vector<Coordinate> points{Coordinate{0, 0}, Coordinate{0.03, 0.05},
                                         Coordinate{0.05, 0.02}, Coordinate{0.07, 0.05},
                                         Coordinate{0.09, 0}};
    fins.setPoints(points);
    fins.setThickness(0.003);
    fins.setCantAngle(0.03);
    fins.setAxialMethod(AxialMethod::BOTTOM);
    fins.setAxialOffset(0.0);
    return fins;
}

/// A freeform fin with a notch in its tip on the body tube. Its outline rises again after
/// falling, which is the jagged edge warning, on every call.
TEST(FinSetCalc, NotchedFreeformFinIsJavas)
{
    Base            base;
    FreeformFinSet& fins = addNotchedFreeform(base);
    base.rocket.enableEvents();

    FinSetCalc calc{fins};
    expectGeometry(calc, kGeometryFreeform);
    EXPECT_EQ(calc.getInterferenceFinCount(), 4);
    EXPECT_FALSE(calc.usesNacaInterference());
    const std::vector<std::string> jagged{
        "Jagged-edged fin predictions may be inaccurate:  \"Freeform Fin Set\""};
    EXPECT_EQ(textsOf(calc.getGeometryWarnings()), jagged);
    EXPECT_EQ(expectForces(calc, kFullConditions, kForcesFreeform), jagged);
    expectDrag(fins, kDragFreeform);
}

/// One row of kChordsFreeform: the division and the pinned chord lead, trail and length.
void expectChords(const FinSetCalc& calc, const std::array<double, 4>& row)
{
    const auto i = static_cast<std::size_t>(row[0]);
    SCOPED_TRACE(::testing::Message() << "division " << i);
    expectPinned(calc.getChordLead()[i], row[1]);
    expectPinned(calc.getChordTrail()[i], row[2]);
    expectPinned(calc.getChordLength()[i], row[3]);
}

/// The chords of the notched freeform fin: above the bottom of the notch (division 19 of 47 is
/// the first above it) the chord length is less than trailing edge - leading edge.
TEST(FinSetCalc, ChordsOfTheNotchedFreeformFinAreJavas)
{
    Base                  base;
    const FreeformFinSet& fins = addNotchedFreeform(base);
    base.rocket.enableEvents();

    const FinSetCalc calc{fins};
    for (const std::array<double, 4>& row : kChordsFreeform)
    {
        expectChords(calc, row);
    }
    EXPECT_LT(calc.getChordLength()[30], calc.getChordTrail()[30] - calc.getChordLead()[30]);
    EXPECT_EQ(FinSetCalc::kDivisions, 48);
    EXPECT_EQ(calc.getChordLead().size(), 48U);
}

/// ProbeFinCalcFix.freeform(): three freeform fins with the outline @p points, 3 mm thick, whose
/// root begins 100 mm behind the front of the body tube.
FreeformFinSet& addFreeform(Base& base, const std::vector<Coordinate>& points)
{
    FreeformFinSet& fins = base.tube.addChild(std::make_unique<FreeformFinSet>());
    fins.setFinCount(3);
    fins.setPoints(points);
    fins.setThickness(0.003);
    fins.setAxialMethod(AxialMethod::TOP);
    fins.setAxialOffset(0.1);
    return fins;
}

/// The chord division @p division of @p calc in the layout of the chord tables: the division and
/// the chord lead, trail and length.
[[nodiscard]] std::array<double, 4> chordRowOf(const FinSetCalc& calc, double division)
{
    const auto i = static_cast<std::size_t>(division);
    return {division, calc.getChordLead()[i], calc.getChordTrail()[i], calc.getChordLength()[i]};
}

/// Outline points on chord divisions. The fin is 47 mm high, so that its 48 divisions are a
/// millimetre apart and its points at 11 mm and 22 mm lie on divisions 11 and 22. The division
/// an outline segment reaches is (int)(y * 1.0001 / span * 47), and it is the factor 1.0001 that
/// puts such a point into both of its segments (0.011 / 0.047 * 47 alone is 10.999999999999998).
/// Division 11 so counts its leading-edge point twice and has a chord length 0.01 below trailing
/// edge - leading edge, which OpenRocket leaves as it is. Division 22 counts its trailing-edge
/// point twice, which the limit of the length to trailing edge - leading edge takes back.
TEST(FinSetCalc, OutlinePointOnAChordDivisionIsInBothOfItsSegments)
{
    Base                          base;
    const std::vector<Coordinate> points{Coordinate{0, 0}, Coordinate{0.01, 0.011},
                                         Coordinate{0.025, 0.047}, Coordinate{0.045, 0.022},
                                         Coordinate{0.06, 0}};
    const FreeformFinSet&         fins = addFreeform(base, points);
    base.rocket.enableEvents();

    const FinSetCalc calc{fins};
    EXPECT_EQ(calc.getSpan(), 0.047);
    EXPECT_TRUE(calc.getGeometryWarnings().empty());
    for (const std::array<double, 4>& row : kChordsOnDivisions)
    {
        EXPECT_EQ(chordRowOf(calc, row[0]), row);  // exactly: sums, products and quotients only
    }
    EXPECT_NEAR(calc.getChordTrail()[11] - calc.getChordLead()[11] - calc.getChordLength()[11],
                0.01, 1e-15);
    EXPECT_EQ(calc.getChordLength()[22], calc.getChordTrail()[22] - calc.getChordLead()[22]);
}

// ---- other parents, cant and a body radius of 0, over the short table

/// The swept trapezoid at the end of the tube, canted: the cant moves the fin's absolute front
/// back by a few hundredths of a millimetre, the tube then ends before the root does, and Java
/// makes no NACA model.
TEST(FinSetCalc, CantedSweptTrapezoidAtTheEndOfTheTubeIsJavas)
{
    Base             base;
    TrapezoidFinSet& fins = addSweptTrapezoid(base);
    fins.setCantAngle(0.05);
    base.rocket.enableEvents();

    FinSetCalc calc{fins};
    expectGeometry(calc, kGeometrySweptCanted);
    EXPECT_EQ(calc.getInterferenceFinCount(), 3);
    EXPECT_FALSE(calc.usesNacaInterference());
    EXPECT_LT(FinSetCalc::calculateCylindricalAfterbodyEnd(fins, base.tube), fins.getRootChord());
    EXPECT_TRUE(calc.getGeometryWarnings().empty());
    EXPECT_EQ(expectForces(calc, kShortConditions, kForcesSweptCanted), noWarnings());
    expectDrag(fins, kDragSweptCanted);
}

// Transitions and nose cones accept freeform fin sets only (Transition::isCompatible()).

/// A freeform fin on the conical boattail: the root follows the cone.
TEST(FinSetCalc, FreeformFinOnATransitionIsJavas)
{
    Base            base;
    FreeformFinSet& fins = base.tail.addChild(std::make_unique<FreeformFinSet>());
    fins.setAxialMethod(AxialMethod::TOP);
    fins.setAxialOffset(0.005);
    const std::vector<Coordinate> points{Coordinate{0, 0}, Coordinate{0.02, 0.04},
                                         Coordinate{0.05, 0.03}, Coordinate{0.05, -0.05 / 6}};
    fins.setPoints(points);
    fins.setThickness(0.003);
    base.rocket.enableEvents();

    FinSetCalc calc{fins};
    EXPECT_EQ(geometryOf(calc),
              kGeometryOnTransition);  // exactly: arithmetic and square roots only
    EXPECT_EQ(calc.getInterferenceFinCount(), 3);
    EXPECT_FALSE(calc.usesNacaInterference());
    EXPECT_TRUE(calc.getGeometryWarnings().empty());
    EXPECT_EQ(expectForces(calc, kShortConditions, kForcesOnTransition), noWarnings());
    expectDrag(fins, kDragOnTransition);
}

/// A freeform fin on the conical boattail: 4 fins, rounded, canted.
TEST(FinSetCalc, CantedFreeformFinOnATransitionIsJavas)
{
    Base            base;
    FreeformFinSet& fins = base.tail.addChild(std::make_unique<FreeformFinSet>());
    fins.setFinCount(4);
    fins.setAxialMethod(AxialMethod::TOP);
    fins.setAxialOffset(0.01);
    const std::vector<Coordinate> points{Coordinate{0, 0}, Coordinate{0.01, 0.03},
                                         Coordinate{0.04, 0.03}, Coordinate{0.045, -0.0075}};
    fins.setPoints(points);
    fins.setThickness(0.002);
    fins.setCrossSection(FinSet::CrossSection::ROUNDED);
    fins.setCantAngle(0.02);
    base.rocket.enableEvents();

    FinSetCalc calc{fins};
    expectGeometry(calc, kGeometryOnTransitionCanted);
    EXPECT_EQ(calc.getInterferenceFinCount(), 4);
    EXPECT_FALSE(calc.usesNacaInterference());
    EXPECT_TRUE(calc.getGeometryWarnings().empty());
    EXPECT_EQ(expectForces(calc, kShortConditions, kForcesOnTransitionCanted), noWarnings());
    expectDrag(fins, kDragOnTransitionCanted);
}

/// A freeform fin at the base of the ogive nose cone: 2 fins, on a curved root.
TEST(FinSetCalc, FreeformFinOnANoseConeIsJavas)
{
    Base            base;
    FreeformFinSet& fins = base.nose.addChild(std::make_unique<FreeformFinSet>());
    fins.setFinCount(2);
    const std::vector<Coordinate> points{
        Coordinate{0, 0}, Coordinate{0.02, 0.03}, Coordinate{0.04, 0.03},
        Coordinate{0.04, base.nose.getRadius(0.10) - base.nose.getRadius(0.06)}};
    fins.setPoints(points);
    fins.setThickness(0.002);
    fins.setAxialMethod(AxialMethod::BOTTOM);
    fins.setAxialOffset(0.0);
    base.rocket.enableEvents();

    FinSetCalc calc{fins};
    expectGeometry(calc, kGeometryOnNoseCone);
    EXPECT_EQ(calc.getInterferenceFinCount(), 2);
    EXPECT_FALSE(calc.usesNacaInterference());
    EXPECT_TRUE(calc.getGeometryWarnings().empty());
    EXPECT_EQ(expectForces(calc, kShortConditions, kForcesOnNoseCone), noWarnings());
    expectDrag(fins, kDragOnNoseCone);
}

/// A trapezoid on a body tube of radius 0: no body-fin interference at all (tau is 0), no NACA
/// model, and no thick fin warning whatever the thickness.
TEST(FinSetCalc, TrapezoidOnABodyTubeOfRadiusZeroIsJavas)
{
    Rocket           rocket;
    AxialStage&      stage = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&        tube  = stage.addChild(std::make_unique<BodyTube>(0.3, 0.0));
    TrapezoidFinSet& fins =
        tube.addChild(std::make_unique<TrapezoidFinSet>(3, 0.06, 0.03, 0.02, 0.05));
    fins.setThickness(0.003);
    fins.setAxialMethod(AxialMethod::BOTTOM);
    fins.setAxialOffset(0.0);
    rocket.enableEvents();

    FinSetCalc calc{fins};
    EXPECT_EQ(geometryOf(calc),
              kGeometryZeroRadiusTube);  // exactly: arithmetic and square roots only
    EXPECT_EQ(fins.getBodyRadius(), 0.0);
    EXPECT_EQ(calc.getInterferenceFinCount(), 3);
    EXPECT_FALSE(calc.usesNacaInterference());
    EXPECT_TRUE(calc.getGeometryWarnings().empty());
    EXPECT_EQ(expectForces(calc, kShortConditions, kForcesZeroRadiusTube), noWarnings());
    expectDrag(fins, kDragZeroRadiusTube);
}

/// The freeform fin of the two nose tip cases: 4 fins at the very tip of the nose cone, where
/// the body radius is 0.
FreeformFinSet& addNoseTipFin(Base& base)
{
    FreeformFinSet& fins = base.nose.addChild(std::make_unique<FreeformFinSet>());
    fins.setFinCount(4);
    fins.setAxialMethod(AxialMethod::TOP);
    fins.setAxialOffset(0.0);
    const std::vector<Coordinate> points{Coordinate{0, 0}, Coordinate{0.01, 0.02},
                                         Coordinate{0.03, 0.02},
                                         Coordinate{0.03, base.nose.getRadius(0.03)}};
    fins.setPoints(points);
    fins.setThickness(0.002);
    return fins;
}

/// A freeform fin at the tip of the nose cone (body radius 0).
TEST(FinSetCalc, FreeformFinAtTheTipOfTheNoseConeIsJavas)
{
    Base            base;
    FreeformFinSet& fins = addNoseTipFin(base);
    base.rocket.enableEvents();

    FinSetCalc calc{fins};
    expectGeometry(calc, kGeometryNoseTip);
    EXPECT_EQ(fins.getBodyRadius(), 0.0);
    EXPECT_EQ(calc.getInterferenceFinCount(), 4);
    EXPECT_FALSE(calc.usesNacaInterference());
    EXPECT_TRUE(calc.getGeometryWarnings().empty());
    EXPECT_EQ(expectForces(calc, kShortConditions, kForcesNoseTip), noWarnings());
    expectDrag(fins, kDragNoseTip);
}

/// The fin at the tip of the nose cone, canted: on a radius of 0 the front of a canted root has
/// a NaN y, so the fin's area is NaN. Java then goes on with the NaN (it is not below the
/// zero-area limit): no warning, forces that are NaN but for the roll damping, a NaN friction
/// drag and a finite pressure and base drag.
TEST(FinSetCalc, NaNOutlineOfACantedFinOnARadiusOfZeroIsJavas)
{
    Base            base;
    FreeformFinSet& fins = addNoseTipFin(base);
    base.rocket.enableEvents();
    fins.setCantAngle(0.03);

    FinSetCalc calc{fins};
    EXPECT_TRUE(std::isnan(fins.getPlanformArea()));
    expectGeometry(calc, kGeometryNoseTipCanted);
    EXPECT_EQ(calc.getInterferenceFinCount(), 4);
    EXPECT_FALSE(calc.usesNacaInterference());
    EXPECT_TRUE(calc.getGeometryWarnings().empty());
    EXPECT_EQ(expectForces(calc, kShortConditions, kForcesNoseTipCanted), noWarnings());
    expectDrag(fins, kDragNoseTipCanted);

    // The quarter chord, up to Mach 0.5 inclusive, does not need the aspect ratio, which is NaN
    // here; beyond it the CP position is NaN (Java: 0.25, 0.25 and NaN)
    EXPECT_EQ(calc.calculateCPPos(conditionsAtMach(0.49)), 0.25);
    EXPECT_EQ(calc.calculateCPPos(conditionsAtMach(0.5)), 0.25);
    EXPECT_TRUE(std::isnan(calc.calculateCPPos(conditionsAtMach(0.51))));
}

/// The swept trapezoid with a NaN height: the span and the area are NaN, no chord is found, the
/// MAC span is 0 and the forces are the zeros of a fin without area. The friction drag is 0 too
/// (no MAC length), but the pressure and base drag are NaN, an airfoil's base drag included.
TEST(FinSetCalc, NaNHeightIsJavas)
{
    Base             base;
    TrapezoidFinSet& fins = addSweptTrapezoid(base);
    base.rocket.enableEvents();
    fins.setHeight(kNaN);

    FinSetCalc calc{fins};
    expectGeometry(calc, kGeometryNaNHeight);
    EXPECT_EQ(calc.getInterferenceFinCount(), 3);
    EXPECT_FALSE(calc.usesNacaInterference());
    EXPECT_TRUE(calc.getGeometryWarnings().empty());
    EXPECT_EQ(expectForces(calc, kPairConditions, kForcesNaNHeight), noWarnings());

    WarningSet             warnings;
    const FlightConditions conditions = conditionsAtMach(0.3);
    EXPECT_EQ(calc.calculateFrictionCD(conditions, kDragCf, warnings), 0.0);
    EXPECT_TRUE(
        std::isnan(calc.calculatePressureCD(conditions, kDragStagnation, kDragBase, warnings)));
    EXPECT_TRUE(std::isnan(calc.calculateComponentBaseCD(conditions, kDragBase, warnings)));

    fins.setCrossSection(FinSet::CrossSection::AIRFOIL);
    FinSetCalc airfoil{fins};
    EXPECT_EQ(airfoil.calculateFrictionCD(conditions, kDragCf, warnings), 0.0);
    EXPECT_TRUE(
        std::isnan(airfoil.calculatePressureCD(conditions, kDragStagnation, kDragBase, warnings)));
    EXPECT_TRUE(std::isnan(airfoil.calculateComponentBaseCD(conditions, kDragBase, warnings)));
    EXPECT_TRUE(warnings.empty());
}

// ---- the fin-fin interference

/// Checks the calculator of @p fins with @p count fins against row @p count - 1 of
/// kForcesByFinCount: the geometry does not depend on the count, and the number of interfering
/// fins is the count.
void expectFinCount(TrapezoidFinSet& fins, int count)
{
    SCOPED_TRACE(::testing::Message() << count << " fins");
    fins.setFinCount(count);
    FinSetCalc calc{fins};
    expectGeometry(calc, kGeometryFinCounts);
    EXPECT_EQ(calc.getInterferenceFinCount(), count);
    EXPECT_TRUE(calc.usesNacaInterference());
    const std::span<const std::array<ForcePins, 2>> rows{kForcesByFinCount};
    EXPECT_EQ(expectForces(calc, kPairConditions, rows[static_cast<std::size_t>(count - 1)]),
              noWarnings());
}

/// The swept trapezoid 20 mm ahead of the tube's end, canted, with 1 to 8 fins: no fin-fin
/// interference up to four fins, then the factors 0.948, 0.913, 0.854 and 0.81 on CNa (and so
/// on CN and Cm), while the roll coefficients of one fin stay as they are.
TEST(FinSetCalc, OneToEightFinsAreJavas)
{
    Base             base;
    TrapezoidFinSet& fins = addSweptTrapezoid(base);
    fins.setAxialOffset(-0.02);
    fins.setCantAngle(0.05);
    base.rocket.enableEvents();

    for (int count = 1; count <= 8; count++)
    {
        expectFinCount(fins, count);
    }
}

/// The second fin set of a pair (ProbeFinCalc.pair()): its fin count, its root chord, and where
/// its end is from the end of the tube.
struct SecondFinSet
{
    int    finCount;
    double rootChord;
    double axialOffset;
};

/// Checks the calculator of @p fins, one fin set of a pair, against the pinned @p forces: the
/// @p count interfering fins, and the PARALLEL_FINS warning (without a source) for more than 8
/// of them and no other warning.
void expectPairMember(const TrapezoidFinSet& fins, int count,
                      const std::array<ForcePins, 2>& forces)
{
    FinSetCalc calc{fins};
    EXPECT_EQ(calc.getInterferenceFinCount(), count);
    EXPECT_TRUE(calc.getGeometryWarnings().empty());
    const std::vector<std::string> expected =
        count > 8 ? std::vector<std::string>{"Too many parallel fins"} : noWarnings();
    EXPECT_EQ(expectForces(calc, kPairConditions, forces), expected);
}

/// ProbeFinCalc.pair(): the swept trapezoid with four fins and, on the same tube, a trapezoidal
/// fin set @p second turned by 45 degrees. Checks both calculators against @p pins, with
/// @p firstCount and @p secondCount interfering fins.
void expectPair(const PairPins& pins, const SecondFinSet& second, int firstCount, int secondCount)
{
    Base             base;
    TrapezoidFinSet& firstFins = addSweptTrapezoid(base);
    firstFins.setFinCount(4);
    TrapezoidFinSet& secondFins = base.tube.addChild(
        std::make_unique<TrapezoidFinSet>(second.finCount, second.rootChord, 0.03, 0.01, 0.03));
    secondFins.setThickness(0.002);
    secondFins.setAngleOffset(kPi / 4);
    secondFins.setAxialMethod(AxialMethod::BOTTOM);
    secondFins.setAxialOffset(second.axialOffset);
    base.rocket.enableEvents();

    expectPairMember(firstFins, firstCount, pins[0]);
    expectPairMember(secondFins, secondCount, pins[1]);
}

/// A second set of 4 fins that overlaps the first axially: 8 fins interfere, for both.
TEST(FinSetCalc, TwoOverlappingFinSetsInterfere)
{
    expectPair(kPairOverlap, {.finCount = 4, .rootChord = 0.05, .axialOffset = -0.02}, 8, 8);
}

/// A second set of 6 fins that overlaps the first: 10 fins, which is 75 % efficiency and the
/// warning for too many parallel fins.
TEST(FinSetCalc, MoreThanEightInterferingFinsWarn)
{
    expectPair(kPairTenFins, {.finCount = 6, .rootChord = 0.05, .axialOffset = -0.02}, 10, 10);
}

/// A second set of 4 fins well ahead of the first: each interferes with itself only.
TEST(FinSetCalc, FinSetsApartDoNotInterfere)
{
    expectPair(kPairApart, {.finCount = 4, .rootChord = 0.05, .axialOffset = -0.20}, 4, 4);
}

/// Fin sets count as overlapping when they share more than 5 mm of the body: 4 mm is not enough,
/// 6 mm is.
TEST(FinSetCalc, FinSetsInterfereFromFiveMillimetresOfOverlap)
{
    expectPair(kPairOverlap4mm, {.finCount = 4, .rootChord = 0.05, .axialOffset = -0.076}, 4, 4);
    expectPair(kPairOverlap6mm, {.finCount = 4, .rootChord = 0.05, .axialOffset = -0.074}, 8, 8);
}

/// A fin set with a root chord below 7 mm counts its own fins only, although the set around it
/// counts it.
TEST(FinSetCalc, ShortRootChordCountsItsOwnFinsOnly)
{
    expectPair(kPairShortRoot, {.finCount = 4, .rootChord = 0.006, .axialOffset = -0.03}, 8, 4);
}

/// A trapezoidal fin set of the count cases (ProbeFinCalcFix.counts()): its fin count, its root
/// chord, and where its root begins behind the front of the body tube.
struct RootOnTube
{
    int    finCount;
    double rootChord;
    double front;
};

/// Adds the fin set @p root to @p tube: unswept, 30 mm high, with a tip chord of half the root
/// chord.
TrapezoidFinSet& addRootOnTube(BodyTube& tube, const RootOnTube& root)
{
    TrapezoidFinSet& fins = tube.addChild(std::make_unique<TrapezoidFinSet>(
        root.finCount, root.rootChord, root.rootChord / 2, 0.0, 0.03));
    fins.setAxialMethod(AxialMethod::TOP);
    fins.setAxialOffset(root.front);
    return fins;
}

/// The numbers of interfering fins of two fin sets on one body tube.
using Counts = std::array<int, 2>;

/// The numbers of interfering fins that the calculators of @p first and @p second count, both
/// added to @p tube of @p rocket, whose events are switched on first.
[[nodiscard]] Counts interferingFinsOn(Rocket& rocket, BodyTube& tube, const RootOnTube& first,
                                       const RootOnTube& second)
{
    const TrapezoidFinSet& firstFins  = addRootOnTube(tube, first);
    const TrapezoidFinSet& secondFins = addRootOnTube(tube, second);
    rocket.enableEvents();
    return {FinSetCalc{firstFins}.getInterferenceFinCount(),
            FinSetCalc{secondFins}.getInterferenceFinCount()};
}

/// interferingFinsOn() the body tube of a Base.
[[nodiscard]] Counts interferingFinsOf(const RootOnTube& first, const RootOnTube& second)
{
    Base base;
    return interferingFinsOn(base.rocket, base.tube, first, second);
}

/// The limit of the short root chord is 7 mm: 4 fins inside a set of 3 count all 7 with a root
/// chord of 7.5 mm, and themselves only with one of 6.9 mm or 6.5 mm (the set around them counts
/// them either way).
TEST(FinSetCalc, RootChordFromSevenMillimetresCountsTheOtherFinSets)
{
    EXPECT_EQ(interferingFinsOf({.finCount = 3, .rootChord = 0.08, .front = 0.1},
                                {.finCount = 4, .rootChord = 0.0075, .front = 0.12}),
              (Counts{7, 7}));
    EXPECT_EQ(interferingFinsOf({.finCount = 3, .rootChord = 0.08, .front = 0.1},
                                {.finCount = 4, .rootChord = 0.0069, .front = 0.12}),
              (Counts{7, 4}));
    EXPECT_EQ(interferingFinsOf({.finCount = 3, .rootChord = 0.08, .front = 0.1},
                                {.finCount = 4, .rootChord = 0.0065, .front = 0.12}),
              (Counts{7, 4}));
}

/// A root chord of exactly 7 mm is not short. (At the front of a tube at the front of the rocket,
/// where the root's end - the root's front is the root chord to the last bit.)
TEST(FinSetCalc, RootChordOfSevenMillimetresIsNotShort)
{
    Rocket      rocket;
    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&   tube  = stage.addChild(std::make_unique<BodyTube>(0.4, 0.025));

    EXPECT_EQ(interferingFinsOn(rocket, tube, {.finCount = 3, .rootChord = 0.08, .front = 0.0},
                                {.finCount = 4, .rootChord = 0.007, .front = 0.0}),
              (Counts{7, 7}));
}

/// The 5 mm of overlap from which fin sets interfere, nearer than the pinned forces of
/// FinSetsInterfereFromFiveMillimetresOfOverlap have it: 4.9 mm is not enough, 5.1 mm is.
TEST(FinSetCalc, OverlapLimitOfInterferenceToATenthOfAMillimetre)
{
    EXPECT_EQ(interferingFinsOf({.finCount = 4, .rootChord = 0.08, .front = 0.1},
                                {.finCount = 4, .rootChord = 0.05, .front = 0.0549}),
              (Counts{4, 4}));
    EXPECT_EQ(interferingFinsOf({.finCount = 4, .rootChord = 0.08, .front = 0.1},
                                {.finCount = 4, .rootChord = 0.05, .front = 0.0551}),
              (Counts{8, 8}));
}

/// Checks the calculator of @p fins, one fin set of the mixed case, against the pinned
/// @p forces: 7 interfering fins, the NACA model or not, and no warning.
void expectMixedMember(const FinSet& fins, bool naca, const std::array<ForcePins, 2>& forces)
{
    FinSetCalc calc{fins};
    EXPECT_EQ(calc.getInterferenceFinCount(), 7);
    EXPECT_EQ(calc.usesNacaInterference(), naca);
    EXPECT_TRUE(calc.getGeometryWarnings().empty());
    EXPECT_EQ(expectForces(calc, kPairConditions, forces), noWarnings());
}

/// Every fin set on the parent counts, whatever its shape, and nothing else does: the swept
/// trapezoid's 3 fins and the 2 fins each of an elliptical and a freeform fin set that overlap
/// it interfere as 7 fins, for each of the three; the 6 tube fins beside them (a TubeFinSet is
/// no FinSet) are not counted.
TEST(FinSetCalc, FinSetsOfEveryShapeInterfereAndTubeFinsDoNot)
{
    Base                   base;
    const TrapezoidFinSet& trapezoid  = addSweptTrapezoid(base);
    EllipticalFinSet&      elliptical = base.tube.addChild(std::make_unique<EllipticalFinSet>());
    elliptical.setFinCount(2);
    elliptical.setHeight(0.04);
    elliptical.setLength(0.06);
    elliptical.setThickness(0.003);
    elliptical.setAxialMethod(AxialMethod::BOTTOM);
    elliptical.setAxialOffset(-0.01);
    FreeformFinSet& freeform = base.tube.addChild(std::make_unique<FreeformFinSet>());
    freeform.setFinCount(2);
    const std::vector<Coordinate> points{Coordinate{0, 0}, Coordinate{0.02, 0.03},
                                         Coordinate{0.05, 0.03}, Coordinate{0.07, 0}};
    freeform.setPoints(points);
    freeform.setThickness(0.003);
    freeform.setAxialMethod(AxialMethod::BOTTOM);
    freeform.setAxialOffset(-0.005);
    TubeFinSet& tubeFins = base.tube.addChild(std::make_unique<TubeFinSet>());
    tubeFins.setFinCount(6);
    tubeFins.setLength(0.05);
    tubeFins.setAxialMethod(AxialMethod::BOTTOM);
    tubeFins.setAxialOffset(0.0);
    base.rocket.enableEvents();

    expectMixedMember(trapezoid, true, kForcesMixedTrapezoid);
    expectMixedMember(elliptical, false, kForcesMixedElliptical);
    expectMixedMember(freeform, false, kForcesMixedFreeform);
}

// ---- the static functions

/// The end of the cylindrical afterbody: flush tubes of the same radius (within
/// MathUtil::equals) in the same stage count, the next stage does not.
TEST(FinSetCalc, CylindricalAfterbodyEnd)
{
    Rocket      rocket;
    AxialStage& stage      = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&   firstTube  = stage.addChild(std::make_unique<BodyTube>(1.0, 0.1));
    BodyTube&   secondTube = stage.addChild(std::make_unique<BodyTube>(0.5, 0.1));
    BodyTube&   thirdTube  = stage.addChild(std::make_unique<BodyTube>(0.25, 0.1));
    AxialStage& booster    = rocket.addChild(std::make_unique<AxialStage>());
    booster.addChild(std::make_unique<BodyTube>(2.0, 0.1));
    TrapezoidFinSet& fins =
        firstTube.addChild(std::make_unique<TrapezoidFinSet>(4, 0.4, 0.2, 0.1, 0.2));
    fins.setAxialMethod(AxialMethod::BOTTOM);
    fins.setAxialOffset(0.0);
    rocket.enableEvents();

    // Java: 1.15, 1.45, 1.45 and 1.2 (sums and differences only)
    EXPECT_DOUBLE_EQ(FinSetCalc::calculateCylindricalAfterbodyEnd(fins, firstTube), 1.15);
    fins.setAxialOffset(-0.3);
    EXPECT_DOUBLE_EQ(FinSetCalc::calculateCylindricalAfterbodyEnd(fins, firstTube), 1.45);
    // A radius that differs by less than the relative 1e-8 of MathUtil::equals is the same
    thirdTube.setOuterRadius(0.1000000001);
    EXPECT_DOUBLE_EQ(FinSetCalc::calculateCylindricalAfterbodyEnd(fins, firstTube), 1.45);
    thirdTube.setOuterRadius(0.100001);
    EXPECT_DOUBLE_EQ(FinSetCalc::calculateCylindricalAfterbodyEnd(fins, firstTube), 1.2);
    // From the second tube on, the walk is the same
    EXPECT_DOUBLE_EQ(FinSetCalc::calculateCylindricalAfterbodyEnd(fins, secondTube), 1.2);
}

/// The afterbody ends where the next tube does not begin at the end of the one before it. An
/// up-to-date component tree has no such gap (a body tube follows the component before it), but
/// positions that were not updated have one: with the rocket's events off, a shorter first tube
/// moves neither the second tube nor the fins.
TEST(FinSetCalc, CylindricalAfterbodyEndsAtAGap)
{
    Rocket           rocket;
    AxialStage&      stage      = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&        firstTube  = stage.addChild(std::make_unique<BodyTube>(1.0, 0.1));
    const BodyTube&  secondTube = stage.addChild(std::make_unique<BodyTube>(0.5, 0.1));
    TrapezoidFinSet& fins =
        firstTube.addChild(std::make_unique<TrapezoidFinSet>(4, 0.4, 0.2, 0.1, 0.2));
    fins.setAxialMethod(AxialMethod::BOTTOM);
    fins.setAxialOffset(0.0);
    rocket.enableEvents();

    // Java: 0.9, 0.20000000000000007 and 0.9 (sums and differences only)
    EXPECT_DOUBLE_EQ(FinSetCalc::calculateCylindricalAfterbodyEnd(fins, firstTube), 0.9);

    rocket.enableEvents(false);
    firstTube.setLength(0.8);
    EXPECT_EQ(secondTube.getAxialOffset(AxialMethod::ABSOLUTE), 1.0);
    EXPECT_EQ(fins.getAxialOffset(AxialMethod::ABSOLUTE), 0.6);
    EXPECT_DOUBLE_EQ(FinSetCalc::calculateCylindricalAfterbodyEnd(fins, firstTube),
                     0.20000000000000007);
    // The 0.2 m of tube left behind the front of the 0.4 m root are no afterbody for the model
    EXPECT_FALSE(FinSetCalc{fins}.usesNacaInterference());

    rocket.enableEvents();
    EXPECT_EQ(secondTube.getAxialOffset(AxialMethod::ABSOLUTE), 0.8);
    EXPECT_DOUBLE_EQ(FinSetCalc::calculateCylindricalAfterbodyEnd(fins, firstTube), 0.9);
}

/// Where Java makes a NACA model for the swept trapezoid: not for a root that begins ahead of
/// the tube or ends behind it.
TEST(FinSetCalc, NacaModelNeedsTheRootOnTheTube)
{
    Base             base;
    TrapezoidFinSet& fins = addSweptTrapezoid(base);
    fins.setAxialMethod(AxialMethod::TOP);
    fins.setAxialOffset(-0.01);
    base.rocket.enableEvents();
    EXPECT_FALSE(FinSetCalc{fins}.usesNacaInterference());

    fins.setAxialOffset(0.0);
    EXPECT_TRUE(FinSetCalc{fins}.usesNacaInterference());
    EXPECT_DOUBLE_EQ(FinSetCalc::calculateCylindricalAfterbodyEnd(fins, base.tube), 0.4);

    fins.setAxialMethod(AxialMethod::BOTTOM);
    fins.setAxialOffset(0.01);
    EXPECT_FALSE(FinSetCalc{fins}.usesNacaInterference());
    EXPECT_DOUBLE_EQ(FinSetCalc::calculateCylindricalAfterbodyEnd(fins, base.tube),
                     0.06999999999999995);
}

TEST(FinSetCalc, NacaApplicabilityWeightIsJavas)
{
    // FinSetCalc.calculateNacaApplicabilityWeight on JDK 17; arithmetic only, so the same
    // doubles. angle of attack, weight
    constexpr std::array<std::array<double, 2>, 13> kCases{{
        {-0.1, 0.0},
        {0.0, 1.0},
        {0.1, 1.0},
        {0.17453292519943295, 1.0},  // 10 degrees
        {0.2181661564992912, 0.84375},
        {0.2617993877991494, 0.5000000000000001},  // 15 degrees
        {0.30543261909900765, 0.15625},
        {0.34904839710634594, 2.9997999972941614E-8},  // 19.999 degrees
        {0.3490658503988659, 0.0},                     // 20 degrees
        {1.0, 0.0},
        {kPi, 0.0},
        {kNaN, 0.0},
        {kInf, 0.0},
    }};
    for (const std::array<double, 2>& row : kCases)
    {
        EXPECT_EQ(FinSetCalc::calculateNacaApplicabilityWeight(row[0]), row[1])
            << "angle of attack " << row[0];
    }
    EXPECT_EQ(FinSetCalc::calculateNacaApplicabilityWeight(-kInf), 0.0);
}

/// One row of the body-fin interference factor cases: tau, the Mach number and the factor.
void expectBodyFinFactor(const std::array<double, 3>& row)
{
    const double factor = FinSetCalc::calculateBodyFinInterferenceFactor(row[0], row[1]);
    if (std::isnan(row[2]))
    {
        EXPECT_TRUE(std::isnan(factor)) << "tau " << row[0] << " Mach " << row[1];
    }
    else
    {
        EXPECT_EQ(factor, row[2]) << "tau " << row[0] << " Mach " << row[1];
    }
}

TEST(FinSetCalc, BodyFinInterferenceFactorIsJavas)
{
    // FinSetCalc.calculateBodyFinInterferenceFactor on JDK 17; arithmetic only, so the same
    // doubles. tau, Mach, factor
    constexpr std::array<std::array<double, 3>, 14> kCases{{
        {0.25, 0.0, 1.5625},
        {0.25, 0.9, 1.5625},
        {0.25, 0.91, 1.5572916666666665},
        {0.25, 1.2, 1.40625},
        {0.25, 1.49, 1.2552083333333333},
        {0.25, 1.5, 1.25},
        {0.25, 2.0, 1.25},
        {0.0, 1.2, 1.0},
        {0.6, 1.0, 2.4000000000000004},
        {0.3, kNaN, kNaN},
        {kNaN, 1.2, kNaN},
        // Mach 0.9 itself is subsonic: the square of 1.3, where the blend that begins above it
        // gives 1.3 + 0.3 * 1.3, the double below
        {0.3, 0.9, 1.6900000000000002},
        {0.3, 0.9000000000000001, 1.69},
        {0.3, 1.4999999999999998, 1.3000000000000003},
    }};
    for (const std::array<double, 3>& row : kCases)
    {
        expectBodyFinFactor(row);
    }
}

// ---- warnings

/// A fin thicker than half the body radius (a quarter of the diameter) is a thick fin; exactly
/// half is not.
TEST(FinSetCalc, ThickFinWarning)
{
    Base             base;
    TrapezoidFinSet& fins = addSweptTrapezoid(base);
    fins.setThickness(0.0126);
    base.rocket.enableEvents();

    FinSetCalc                     thick{fins};
    const std::vector<std::string> expected{
        "Thick fins may not simulate accurately:  \"Trapezoidal Fin Set\""};
    EXPECT_EQ(textsOf(thick.getGeometryWarnings()), expected);

    fins.setThickness(0.0125);
    EXPECT_TRUE(FinSetCalc{fins}.getGeometryWarnings().empty());

    // The warning goes into the caller's set on every call, and is in it once
    WarningSet        warnings;
    AerodynamicForces forces;
    thick.calculateNonaxialForces(conditionsAtMach(0.3), Transformation::kIdentity, forces,
                                  warnings);
    thick.calculateNonaxialForces(conditionsAtMach(0.3), Transformation::kIdentity, forces,
                                  warnings);
    EXPECT_EQ(textsOf(warnings), expected);
    WarningSet again;
    thick.calculateNonaxialForces(conditionsAtMach(2.0), Transformation::kIdentity, forces, again);
    EXPECT_EQ(textsOf(again), expected);
}

/// The geometry warnings of three freeform fins with the outline @p points on the body tube.
[[nodiscard]] std::vector<std::string> outlineWarnings(const std::vector<Coordinate>& points)
{
    Base                  base;
    const FreeformFinSet& fins = addFreeform(base, points);
    base.rocket.enableEvents();
    return textsOf(FinSetCalc{fins}.getGeometryWarnings());
}

/// An edge is jagged when the outline rises by more than 1 mm from one point to the next after
/// having fallen by more than 1 mm: neither a smaller fall before a rise nor a smaller rise
/// after a fall is one.
TEST(FinSetCalc, JaggedEdgeIsAFallAndARiseOfMoreThanAMillimetre)
{
    const std::vector<std::string> jagged{
        "Jagged-edged fin predictions may be inaccurate:  \"Freeform Fin Set\""};

    // falls 0.5 mm, then rises 5.5 mm
    EXPECT_EQ(outlineWarnings({Coordinate{0, 0}, Coordinate{0.01, 0.03}, Coordinate{0.02, 0.0295},
                               Coordinate{0.03, 0.035}, Coordinate{0.05, 0}}),
              noWarnings());
    // falls 5 mm, then rises 0.5 mm
    EXPECT_EQ(outlineWarnings({Coordinate{0, 0}, Coordinate{0.01, 0.03}, Coordinate{0.02, 0.025},
                               Coordinate{0.03, 0.0255}, Coordinate{0.05, 0}}),
              noWarnings());
    // falls and rises 0.9 mm
    EXPECT_EQ(outlineWarnings({Coordinate{0, 0}, Coordinate{0.01, 0.03}, Coordinate{0.02, 0.0291},
                               Coordinate{0.03, 0.03}, Coordinate{0.06, 0}}),
              noWarnings());
    // falls and rises 1.1 mm
    EXPECT_EQ(outlineWarnings({Coordinate{0, 0}, Coordinate{0.01, 0.03}, Coordinate{0.02, 0.0289},
                               Coordinate{0.03, 0.03}, Coordinate{0.06, 0}}),
              jagged);
    // the notch in the tip of the pinned freeform fin, 0.9 mm and 1.1 mm deep
    EXPECT_EQ(
        outlineWarnings({Coordinate{0, 0}, Coordinate{0.03, 0.05}, Coordinate{0.05, 0.05 - 0.0009},
                         Coordinate{0.07, 0.05}, Coordinate{0.09, 0}}),
        noWarnings());
    EXPECT_EQ(
        outlineWarnings({Coordinate{0, 0}, Coordinate{0.03, 0.05}, Coordinate{0.05, 0.05 - 0.0011},
                         Coordinate{0.07, 0.05}, Coordinate{0.09, 0}}),
        jagged);
}

/// Expects every warning of @p warnings to have the one source @p fins.
void expectSourcedBy(const WarningSet& warnings, const FinSet& fins)
{
    for (const Warning& warning : warnings)
    {
        ASSERT_EQ(warning.sources().size(), 1U);
        EXPECT_EQ(warning.sources().front().id, fins.getId());
    }
}

/// A geometry warning names the fin set as its source, by id and by the name it had when the
/// calculator was made; the warning for too many parallel fins has no source.
TEST(FinSetCalc, GeometryWarningsNameTheFinSet)
{
    Base             base;
    TrapezoidFinSet& fins = addSweptTrapezoid(base);
    fins.setName("Aft fins");
    fins.setHeight(0.0);
    fins.setThickness(0.02);
    base.rocket.enableEvents();

    const FinSetCalc               calc{fins};
    const std::vector<std::string> expected{
        "Fins with zero area will not affect aerodynamics:  \"Aft fins\"",
        "Thick fins may not simulate accurately:  \"Aft fins\""};
    EXPECT_EQ(textsOf(calc.getGeometryWarnings()), expected);
    expectSourcedBy(calc.getGeometryWarnings(), fins);
    EXPECT_EQ(calc.getGeometryWarnings().begin()->priority(), Warning::kZeroAreaFin.priority());
    EXPECT_EQ(Warning::kParallelFins.priority(), MessagePriority::LOW);
    EXPECT_TRUE(Warning::kParallelFins.sources().empty());
}

// ---- construction

TEST(FinSetCalc, DetachedFinSetIsABug)
{
    // Java: IllegalStateException("fin set without parent component")
    const TrapezoidFinSet fins{3, 0.05, 0.03, 0.02, 0.04};
    EXPECT_THROW(static_cast<void>(FinSetCalc{fins}), BugError);
}

/// A fin set whose root chord is NaN (set while the rocket's events are off: with them on, the
/// update of the position throws first, in Java too) overlaps nothing, itself included, so no
/// fin is counted.
TEST(FinSetCalc, FewerInterferingFinsThanItsOwnIsABug)
{
    Base             base;
    TrapezoidFinSet& fins = addSweptTrapezoid(base);
    fins.setFinShape(kNaN, 0.05, 0.03, 0.06, 0.003);

    try
    {
        static_cast<void>(FinSetCalc{fins});
        FAIL() << "no BugError";
    }
    catch (const BugError& error)
    {
        // Java: BugException with this text
        EXPECT_NE(std::string{error.what()}.find(
                      "Counted 0 parallel fins, when component itself has 3, fin points="
                      "[(0.00000,0.00000,0.00000), (0.03000,0.06000,0.00000), "
                      "(0.08000,0.06000,0.00000), (NaN,0.00000,0.00000)]"),
                  std::string::npos)
            << error.what();
    }
}

/// A cross-section that is none of the three (Java's enum has no such value) is a bug for the
/// leading edge; the trailing edge treats it as an airfoil, as Java's if-else chain would.
TEST(FinSetCalc, UnknownCrossSectionIsABug)
{
    Base             base;
    TrapezoidFinSet& fins = addSweptTrapezoid(base);
    fins.setCrossSection(static_cast<FinSet::CrossSection>(7));
    base.rocket.enableEvents();

    FinSetCalc             calc{fins};
    WarningSet             warnings;
    const FlightConditions conditions = conditionsAtMach(0.3);
    EXPECT_THROW(static_cast<void>(
                     calc.calculatePressureCD(conditions, kDragStagnation, kDragBase, warnings)),
                 BugError);
    EXPECT_EQ(calc.calculateComponentBaseCD(conditions, kDragBase, warnings), 0.0);
}

/// The calculator copies what it needs at construction: a later change of the fin set does not
/// reach it, and it does not need the fin set (or its rocket) any more.
TEST(FinSetCalc, KeepsTheGeometryItWasMadeFor)
{
    const FlightConditions conditions = conditionsFor(0.6, 2, kPi / 4, 20);
    WarningSet             warnings;
    AerodynamicForces      before;
    AerodynamicForces      after;

    auto             base = std::make_unique<Base>();
    TrapezoidFinSet& fins = addSweptTrapezoid(*base);
    base->rocket.enableEvents();
    FinSetCalc calc{fins};
    calc.calculateNonaxialForces(conditions, Transformation::kIdentity, before, warnings);
    const double pressure =
        calc.calculatePressureCD(conditions, kDragStagnation, kDragBase, warnings);

    fins.setHeight(0.03);
    fins.setThickness(0.02);
    fins.setCantAngle(0.1);
    fins.setFinCount(8);
    fins.setCrossSection(FinSet::CrossSection::AIRFOIL);
    base.reset();

    calc.calculateNonaxialForces(conditions, Transformation::kIdentity, after, warnings);
    EXPECT_EQ(after.getCP().x, before.getCP().x);
    EXPECT_EQ(after.getCP().weight, before.getCP().weight);
    EXPECT_EQ(after.getCN(), before.getCN());
    EXPECT_EQ(after.getCm(), before.getCm());
    EXPECT_EQ(after.getCroll(), before.getCroll());
    EXPECT_EQ(after.getCrollDamp(), before.getCrollDamp());
    EXPECT_EQ(after.getCrollForce(), before.getCrollForce());
    EXPECT_EQ(calc.calculatePressureCD(conditions, kDragStagnation, kDragBase, warnings), pressure);
    EXPECT_EQ(calc.getSpan(), 0.06);
    EXPECT_EQ(calc.getInterferenceFinCount(), 3);
    EXPECT_TRUE(warnings.empty());
}

/// CNa and the roll damping coefficient of one rolling fin of @p calc at Mach 1.2 and at Mach 2:
/// what needs the tables every calculator shares (the transonic interpolation of the single-fin
/// CNa and the K coefficients).
[[nodiscard]] std::array<double, 4> sharedTableValues(FinSetCalc& calc)
{
    WarningSet        warnings;
    AerodynamicForces transonic;
    calc.calculateNonaxialForces(conditionsFor(1.2, 2, kHalfPi, 20), Transformation::kIdentity,
                                 transonic, warnings);
    AerodynamicForces supersonic;
    calc.calculateNonaxialForces(conditionsFor(2.0, 2, kHalfPi, 20), Transformation::kIdentity,
                                 supersonic, warnings);
    return {transonic.getCP().weight, transonic.getCrollDamp(), supersonic.getCP().weight,
            supersonic.getCrollDamp()};
}

TEST(FinSetCalc, ConcurrentFirstUseOfTheSharedTablesIsSafe)
{
    // A calculator belongs to one simulation, but simulations run side by side, and the tables
    // the calculators share are built at their first use: several threads, each with a
    // calculator of its own, may make them at once. Run under the tsan preset to check.
    Base                   base;
    const TrapezoidFinSet& fins = addSweptTrapezoid(base);
    base.rocket.enableEvents();

    constexpr std::size_t                             kThreads = 4;
    std::array<std::unique_ptr<FinSetCalc>, kThreads> calcs;
    for (std::unique_ptr<FinSetCalc>& calc : calcs)
    {
        calc = std::make_unique<FinSetCalc>(fins);
    }
    std::array<std::array<double, 4>, kThreads> values{};
    {
        std::vector<std::jthread> threads;
        threads.reserve(kThreads);
        for (std::size_t i = 0; i < kThreads; ++i)
        {
            threads.emplace_back(
                [&calcs, &values, i] { values.at(i) = sharedTableValues(*calcs.at(i)); });
        }
    }

    FinSetCalc                  reference{fins};
    const std::array<double, 4> expected = sharedTableValues(reference);
    EXPECT_GT(expected[0], 0.0);
    EXPECT_GT(expected[3], 0.0);
    for (const std::array<double, 4>& value : values)
    {
        EXPECT_EQ(value, expected);
    }
}

// ---- the forces

/// Only the rotation about x of the instance's transformation counts: a translation changes
/// nothing, and a fin in line with the lateral airflow has no normal force.
TEST(FinSetCalc, UsesTheRotationAboutXOfTheTransformationOnly)
{
    Base                   base;
    const TrapezoidFinSet& fins = addSweptTrapezoid(base);
    base.rocket.enableEvents();
    FinSetCalc             calc{fins};
    const FlightConditions conditions = conditionsFor(0.6, 2, 0.25, 0);
    WarningSet             warnings;

    AerodynamicForces rotated;
    calc.calculateNonaxialForces(conditions, Transformation::rotateX(0.7), rotated, warnings);
    AerodynamicForces moved;
    calc.calculateNonaxialForces(conditions,
                                 Transformation::translation(0.3, 0.02, -0.01)
                                     .applyTransformation(Transformation::rotateX(0.7)),
                                 moved, warnings);
    EXPECT_EQ(moved.getCP().x, rotated.getCP().x);
    EXPECT_EQ(moved.getCP().weight, rotated.getCP().weight);
    EXPECT_EQ(moved.getCN(), rotated.getCN());
    EXPECT_GT(rotated.getCN(), 0.0);

    // theta equal to the rotation: the square of sin(0)
    AerodynamicForces inLine;
    calc.calculateNonaxialForces(conditionsFor(0.6, 2, 0, 0), Transformation::kIdentity, inLine,
                                 warnings);
    EXPECT_EQ(inLine.getCP().weight, 0.0);
    EXPECT_EQ(inLine.getCN(), 0.0);
    EXPECT_EQ(inLine.getCm(), 0.0);
}

/// The fins of a set add up: with the fins at their real angles (2 pi i / n) the sum of
/// sin^2(theta - angle) is n / 2 for three fins and more, whatever theta, so the set's CNa is
/// n / 2 times the CNa of a fin square to the airflow, at the same CP.
TEST(FinSetCalc, FinsOfASetAddUpToHalfTheFinCount)
{
    Base                   base;
    const TrapezoidFinSet& fins = addSweptTrapezoid(base);
    base.rocket.enableEvents();
    FinSetCalc calc{fins};
    WarningSet warnings;

    AerodynamicForces square;
    calc.calculateNonaxialForces(conditionsFor(0.3, 2, kPi / 2, 0), Transformation::kIdentity,
                                 square, warnings);

    AerodynamicForces      total      = AerodynamicForces{}.zero();
    const FlightConditions conditions = conditionsFor(0.3, 2, 0.4, 0);
    for (const double angle : fins.getInstanceAngles())
    {
        AerodynamicForces fin;
        calc.calculateNonaxialForces(conditions, Transformation::rotateX(angle), fin, warnings);
        total.merge(fin);
    }
    EXPECT_NEAR(total.getCP().weight, 1.5 * square.getCP().weight, 1e-12 * square.getCP().weight);
    EXPECT_NEAR(total.getCP().x, square.getCP().x, 1e-12);
}

/// The NACA model moves the CP only for a CNa above MathUtil::kEpsilon. With the swept trapezoid
/// nearly in line with the lateral airflow, a CNa of 7.4e-9 leaves the CP at the isolated fin's
/// (the quarter chord of the MAC), and one of 1.3e-8 has it where the model puts it.
TEST(FinSetCalc, NacaModelMovesTheCPOnlyForACNaAboveTheLimit)
{
    Base                   base;
    const TrapezoidFinSet& fins = addSweptTrapezoid(base);
    base.rocket.enableEvents();

    FinSetCalc calc{fins};
    ASSERT_TRUE(calc.usesNacaInterference());
    EXPECT_EQ(expectForces(calc, kSmallCNaConditions, kForcesSweptSmallCNa), noWarnings());
    EXPECT_EQ(calc.getMACLead() + (0.25 * calc.getMACLength()), 0.030347790507364983);
}

/// ProbeFinCalcFix.nearlyRectangular(): four canted fins with a root chord of 30 mm and the tip
/// chord @p tipChord, unswept and 100 mm high.
TrapezoidFinSet& addNearlyRectangular(Base& base, double tipChord)
{
    TrapezoidFinSet& fins =
        base.tube.addChild(std::make_unique<TrapezoidFinSet>(4, 0.03, tipChord, 0.0, 0.1));
    fins.setAxialMethod(AxialMethod::TOP);
    fins.setAxialOffset(0.1);
    fins.setCantAngle(0.05);
    return fins;
}

/// A planform is rectangular when its root and tip chords are MathUtil::equals, not only when
/// they are the same double: a tip chord within a relative 1e-8 of the root chord gets the roll
/// forcing of chart 3, one beyond it that of equation 19.
TEST(FinSetCalc, RectangularPlanformIsRootAndTipChordWithinEquals)
{
    Base                   within;
    const TrapezoidFinSet& nearlyEqual = addNearlyRectangular(within, 0.03000000015);
    within.rocket.enableEvents();
    FinSetCalc withinCalc{nearlyEqual};
    EXPECT_EQ(withinCalc.getInterferenceFinCount(), 4);
    EXPECT_FALSE(withinCalc.usesNacaInterference());
    EXPECT_EQ(expectForces(withinCalc, kCantConditions, kForcesTipWithinEquals), noWarnings());

    Base                   beyond;
    const TrapezoidFinSet& tapered = addNearlyRectangular(beyond, 0.0300000006);
    beyond.rocket.enableEvents();
    FinSetCalc beyondCalc{tapered};
    EXPECT_EQ(beyondCalc.getInterferenceFinCount(), 4);
    EXPECT_FALSE(beyondCalc.usesNacaInterference());
    EXPECT_EQ(expectForces(beyondCalc, kCantConditions, kForcesTipBeyondEquals), noWarnings());
}

// ---- degenerate geometry

/// A fin set without area has the zero-area warning, no aspect ratio and no MAC.
void expectNoAreaGeometry(const FinSetCalc& calc)
{
    const std::vector<std::string> expected{
        "Fins with zero area will not affect aerodynamics:  \"Trapezoidal Fin Set\""};
    EXPECT_EQ(textsOf(calc.getGeometryWarnings()), expected);
    const std::array<double, 3> geometry{calc.getAspectRatio(), calc.getMACLength(),
                                         calc.getMACSpan()};
    EXPECT_EQ(geometry, (std::array<double, 3>{0.0, 0.0, 0.0}));
}

/// A fin set without area has forces and drag of exactly 0, whatever the flight conditions.
void expectNoForcesOrDrag(FinSetCalc& calc)
{
    WarningSet             warnings;
    AerodynamicForces      forces;
    const FlightConditions conditions = conditionsFor(2.0, 10, 1, 20);
    calc.calculateNonaxialForces(conditions, Transformation::kIdentity, forces, warnings);
    EXPECT_EQ(warnings.size(), 1U);
    EXPECT_TRUE(forces.getCP().exactlyEquals(Coordinate::kZero));
    const std::array<double, 7> coefficients{
        forces.getCN(),         forces.getCm(),    forces.getCroll(), forces.getCrollDamp(),
        forces.getCrollForce(), forces.getCside(), forces.getCyaw()};
    EXPECT_EQ(coefficients, (std::array<double, 7>{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}));
    const std::array<double, 4> drag{
        calc.calculateFinCNa1(conditions), calc.calculateFrictionCD(conditions, kDragCf, warnings),
        calc.calculatePressureCD(conditions, kDragStagnation, kDragBase, warnings),
        calc.calculateComponentBaseCD(conditions, kDragBase, warnings)};
    EXPECT_EQ(drag, (std::array<double, 4>{0.0, 0.0, 0.0, 0.0}));
}

/// A fin set without a span has no area. (Not so one without chords, which a trapezoidal fin set
/// draws 0.1 mm wide, nor a canted one, whose root drops below the body's surface.)
TEST(FinSetCalc, FinWithoutSpanHasNoAerodynamics)
{
    Base             base;
    TrapezoidFinSet& fins = addSweptTrapezoid(base);
    fins.setHeight(0.0);
    base.rocket.enableEvents();

    FinSetCalc calc{fins};
    EXPECT_EQ(calc.getFinArea(), 0.0);
    // Java: all of the geometry is 0, the sweep cosines too (with a hypotenuse of 0 between two
    // chords, 0 / 0 is not formed)
    EXPECT_EQ(geometryOf(calc), GeometryPins{});
    expectNoAreaGeometry(calc);
    expectNoForcesOrDrag(calc);
    // Without the early return, the sub- and transonic formulas would give NaN (0 / 0)
    expectSingleFinCNa(calc, kNoSingleFinCNa);
}

/// "No area" is an area below MathUtil::kEpsilon (1e-8 m2), not an area of 0. The swept
/// trapezoid 0.15 um high has 9.75e-9 m2: the warning, no aspect ratio, no forces, no drag for
/// any cross-section and no single-fin CNa, although its span and its sweep cosines are above
/// their limits.
TEST(FinSetCalc, AreaBelowTheLimitIsNoArea)
{
    Base             base;
    TrapezoidFinSet& fins = addSweptTrapezoid(base);
    fins.setHeight(1.5e-7);
    base.rocket.enableEvents();

    FinSetCalc calc{fins};
    EXPECT_EQ(geometryOf(calc), kGeometryAreaBelowLimit);  // exactly: arithmetic only
    EXPECT_EQ(calc.getInterferenceFinCount(), 3);
    EXPECT_FALSE(calc.usesNacaInterference());
    expectNoAreaGeometry(calc);
    expectNoForcesOrDrag(calc);
    expectSingleFinCNa(calc, kNoSingleFinCNa);
    expectDrag(fins, kNoDrag);
}

/// The swept trapezoid 0.16 um high has 1.04e-8 m2, which is an area: no warning, an aspect
/// ratio, a single-fin CNa, and leading and trailing edge drag. No chord is found on a span
/// below 0.5 mm, though, so there is no MAC and with it neither forces nor friction drag.
TEST(FinSetCalc, AreaAboveTheLimitIsAnArea)
{
    Base             base;
    TrapezoidFinSet& fins = addSweptTrapezoid(base);
    fins.setHeight(1.6e-7);
    base.rocket.enableEvents();

    FinSetCalc calc{fins};
    EXPECT_EQ(geometryOf(calc), kGeometryAreaAboveLimit);  // exactly: arithmetic only
    EXPECT_EQ(calc.getInterferenceFinCount(), 3);
    EXPECT_FALSE(calc.usesNacaInterference());
    EXPECT_TRUE(calc.getGeometryWarnings().empty());
    EXPECT_EQ(expectForces(calc, kPairConditions, kNoForces), noWarnings());
    expectSingleFinCNa(calc, kSingleFinAreaAboveLimit);
    expectDrag(fins, kDragAreaAboveLimit);
}

/// The chords leave out every outline segment whose ends are MathUtil::equals(y1, y2, 0.001),
/// which two heights within 0.5 mm of 0 are: on a span of 0.49 mm no chord is found. Such a fin
/// has an area, an aspect ratio and a single-fin CNa, and leading and trailing edge drag, but no
/// MAC: no forces and no friction drag, and no warning either.
TEST(FinSetCalc, SpanBelowHalfAMillimetreHasNoChords)
{
    Base             base;
    TrapezoidFinSet& fins = addSweptTrapezoid(base);
    fins.setHeight(0.00049);
    base.rocket.enableEvents();

    FinSetCalc calc{fins};
    EXPECT_EQ(geometryOf(calc), kGeometryHalfMillimetreLess);  // exactly: arithmetic only
    EXPECT_FALSE(calc.usesNacaInterference());
    EXPECT_TRUE(calc.getGeometryWarnings().empty());
    EXPECT_EQ(expectForces(calc, kPairConditions, kNoForces), noWarnings());
    expectSingleFinCNa(calc, kSingleFinHalfMillimetreLess);
    expectDrag(fins, kDragHalfMillimetreLess);
}

/// On a span of 0.51 mm the chords are found: the swept trapezoid's MAC length and leading edge
/// (its 30 mm of sweep over half a millimetre leave next to nothing of the sweep cosines).
TEST(FinSetCalc, SpanAboveHalfAMillimetreHasChords)
{
    Base             base;
    TrapezoidFinSet& fins = addSweptTrapezoid(base);
    fins.setHeight(0.00051);
    base.rocket.enableEvents();

    FinSetCalc calc{fins};
    EXPECT_EQ(geometryOf(calc),
              kGeometryHalfMillimetreMore);  // exactly: arithmetic and square roots only
    EXPECT_FALSE(calc.usesNacaInterference());
    EXPECT_TRUE(calc.getGeometryWarnings().empty());
    EXPECT_EQ(expectForces(calc, kPairConditions, kForcesHalfMillimetreMore), noWarnings());
    expectSingleFinCNa(calc, kSingleFinHalfMillimetreMore);
    expectDrag(fins, kDragHalfMillimetreMore);
}

/// The single-fin CNa is 0 for a span below MathUtil::kEpsilon, whatever the area: a 3 m chord
/// 5 nm high has 1.5e-8 m2, which is an area, and no CNa; 20 nm high it has one.
TEST(FinSetCalc, SingleFinCNaNeedsASpan)
{
    Base base;
    base.tube.setLength(5.0);
    TrapezoidFinSet& fins =
        base.tube.addChild(std::make_unique<TrapezoidFinSet>(3, 3.0, 3.0, 0.0, 5e-9));
    fins.setAxialMethod(AxialMethod::TOP);
    fins.setAxialOffset(0.1);
    base.rocket.enableEvents();

    const FinSetCalc below{fins};
    EXPECT_EQ(geometryOf(below), kGeometrySpanBelowLimit);  // exactly: arithmetic only
    EXPECT_TRUE(below.getGeometryWarnings().empty());
    expectSingleFinCNa(below, kNoSingleFinCNa);

    fins.setHeight(2e-8);
    const FinSetCalc above{fins};
    EXPECT_EQ(geometryOf(above), kGeometrySpanAboveLimit);  // exactly: arithmetic only
    EXPECT_TRUE(above.getGeometryWarnings().empty());
    expectSingleFinCNa(above, kSingleFinSpanAboveLimit);
}

/// The single-fin CNa is 0 for a mean midchord sweep cosine below MathUtil::kEpsilon: a sweep
/// of 10000 km on a span of 60 mm leaves 6e-9 of it and no CNa, at any Mach number, and so no
/// forces on the fin square to the airflow; 5000 km leave 1.2e-8 and a CNa.
TEST(FinSetCalc, SingleFinCNaNeedsAMidchordSweepCosine)
{
    Base             base;
    TrapezoidFinSet& fins =
        base.tube.addChild(std::make_unique<TrapezoidFinSet>(3, 0.08, 0.05, 1e7, 0.06));
    fins.setAxialMethod(AxialMethod::TOP);
    fins.setAxialOffset(0.1);
    base.rocket.enableEvents();

    FinSetCalc below{fins};
    expectGeometry(below, kGeometrySweepCosineBelowLimit);
    EXPECT_FALSE(below.usesNacaInterference());
    EXPECT_TRUE(below.getGeometryWarnings().empty());
    expectSingleFinCNa(below, kNoSingleFinCNa);
    EXPECT_EQ(expectForces(below, kSquareConditions, kNoForces), noWarnings());

    fins.setSweep(5e6);
    FinSetCalc above{fins};
    expectGeometry(above, kGeometrySweepCosineAboveLimit);
    EXPECT_FALSE(above.usesNacaInterference());
    EXPECT_TRUE(above.getGeometryWarnings().empty());
    expectSingleFinCNa(above, kSingleFinSweepCosineAboveLimit);
    EXPECT_EQ(expectForces(above, kSquareConditions, kForcesSweepCosineAboveLimit), noWarnings());
}

/// A fin without area that has chords all the same: two strokes a tenth of a micrometre wide,
/// one from the root up to the tip and one from the tip back down, 9e-9 m2 together. The chords
/// reach from one stroke to the other, so the fin has a MAC of 68 mm at a span position of
/// 10 mm, and only the area says that it has no forces, no friction drag and no single-fin CNa.
TEST(FinSetCalc, FinOfNoAreaWithChordsHasNoAerodynamics)
{
    Base                          base;
    const std::vector<Coordinate> points{Coordinate{0, 0}, Coordinate{0.05, 0.03},
                                         Coordinate{0.1, 0.0005}, Coordinate{0.05, 0.0299999},
                                         Coordinate{1e-7, 0}};
    FreeformFinSet&               fins = addFreeform(base, points);
    base.rocket.enableEvents();

    FinSetCalc calc{fins};
    expectGeometry(calc, kGeometrySliver);
    EXPECT_GT(calc.getMACSpan(), 0.009);
    EXPECT_GT(calc.getMACLength(), 0.06);
    const std::vector<std::string> expected{
        "Fins with zero area will not affect aerodynamics:  \"Freeform Fin Set\"",
        "Jagged-edged fin predictions may be inaccurate:  \"Freeform Fin Set\""};
    EXPECT_EQ(textsOf(calc.getGeometryWarnings()), expected);
    EXPECT_EQ(expectForces(calc, kPairConditions, kNoForces), expected);
    expectSingleFinCNa(calc, kNoSingleFinCNa);
    expectDrag(fins, kNoDrag);
}

/// A fin of no thickness has no leading or trailing edge drag, and its friction drag is that of
/// its two faces.
TEST(FinSetCalc, FinWithoutThicknessHasFaceFrictionOnly)
{
    Base             base;
    TrapezoidFinSet& fins = addSweptTrapezoid(base);
    fins.setThickness(0.0);
    base.rocket.enableEvents();

    FinSetCalc             calc{fins};
    WarningSet             warnings;
    const FlightConditions conditions = conditionsAtMach(0.6);
    EXPECT_EQ(calc.calculatePressureCD(conditions, kDragStagnation, kDragBase, warnings), 0.0);
    EXPECT_EQ(calc.calculateComponentBaseCD(conditions, kDragBase, warnings), 0.0);
    EXPECT_EQ(calc.calculateFrictionCD(conditions, kDragCf, warnings),
              kDragCf * 2 * calc.getFinArea() / conditions.getRefArea());
    EXPECT_TRUE(warnings.empty());
}

/// The roll damping follows the sign of the roll rate, and there is none below 0.1 rad/s.
TEST(FinSetCalc, RollDampingOpposesTheRoll)
{
    Base                   base;
    const TrapezoidFinSet& fins = addSweptTrapezoid(base);
    base.rocket.enableEvents();
    FinSetCalc calc{fins};
    WarningSet warnings;

    AerodynamicForces forward;
    calc.calculateNonaxialForces(conditionsFor(0.3, 0, 0, 5), Transformation::kIdentity, forward,
                                 warnings);
    AerodynamicForces backward;
    calc.calculateNonaxialForces(conditionsFor(0.3, 0, 0, -5), Transformation::kIdentity, backward,
                                 warnings);
    AerodynamicForces slow;
    calc.calculateNonaxialForces(conditionsFor(0.3, 0, 0, -0.0999), Transformation::kIdentity, slow,
                                 warnings);

    EXPECT_GT(forward.getCrollDamp(), 0.0);
    EXPECT_EQ(backward.getCrollDamp(), -forward.getCrollDamp());
    EXPECT_EQ(forward.getCroll(), -forward.getCrollDamp());
    EXPECT_EQ(forward.getCrollForce(), 0.0);
    EXPECT_EQ(slow.getCrollDamp(), 0.0);
    EXPECT_EQ(slow.getCroll(), 0.0);
}

}  // namespace
