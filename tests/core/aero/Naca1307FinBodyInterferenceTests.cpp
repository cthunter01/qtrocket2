#include "QtRocket/aero/barrowman/Naca1307FinBodyInterference.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <numbers>

#include <gtest/gtest.h>

#include "QtRocket/aero/barrowman/FinSetCalc.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/MathUtil.h"

namespace
{

using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::FinSetCalc;
using QtRocket::Naca1307FinBodyInterference;
using QtRocket::Rocket;
using QtRocket::TrapezoidFinSet;
using QtRocket::MathUtil::javaToRadians;
using Loads = Naca1307FinBodyInterference::Loads;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kPi  = std::numbers::pi;

constexpr double kEpsilon = 1.0e-12;  // FinBodyInterferenceTest.EPSILON
constexpr double kTau     = 0.25;     // FinBodyInterferenceTest.TAU

/// FinBodyInterferenceTest.createTableOneModel(): the Mach-2 computing example of NACA Report
/// 1307, table I.
[[nodiscard]] Naca1307FinBodyInterference createTableOneModel(double bodyEnd)
{
    return Naca1307FinBodyInterference{0.562, 2.25, 2.25, 0.0, 2.25, 4.0, bodyEnd};
}

/// The relative tolerance of a Java-pinned value: the values went through atan, asin, acos or
/// log, which may differ in the last bit between math libraries.
constexpr double kPinnedRelative = 1e-12;

/// Expects @p actual to be the Java-pinned @p expected: NaN for NaN, otherwise within the
/// relative tolerance @p relative, or 1e-15 absolute near zero.
void expectPinned(double actual, double expected, double relative = kPinnedRelative)
{
    if (std::isnan(expected))
    {
        EXPECT_TRUE(std::isnan(actual)) << actual;
        return;
    }
    EXPECT_NEAR(actual, expected, (relative * std::abs(expected)) + 1e-15);
}

// ---- Ported from FinBodyInterferenceTest.java: all of its 25 cases, in its order. The six that
// call FinSetCalc's static functions (two of them on a rocket) are the FinBodyInterference
// suite; the others test the NACA model alone. ----

// FinBodyInterferenceTest.separatesSlenderBodyInterferenceFactors
/// Equations 14 and 21 must retain their exact slender-body sum.
TEST(Naca1307FinBodyInterference, SeparatesSlenderBodyInterferenceFactors)
{
    const double finFactor = Naca1307FinBodyInterference::calculateFinInBodyFactor(kTau);
    const double bodyFactor =
        Naca1307FinBodyInterference::calculateSlenderBodyInFinFactor(kTau, finFactor);

    EXPECT_NEAR(1.5625, finFactor + bodyFactor, kEpsilon);
    EXPECT_GT(finFactor, 1.0);
    EXPECT_GT(bodyFactor, 0.0);
}

// FinBodyInterferenceTest.reproducesNacaWingIncidenceFactor
/// Equation 19 uses the lowercase incidence factor, which is distinct from the uppercase
/// body-angle-of-attack factor in equation 14.
TEST(Naca1307FinBodyInterference, ReproducesNacaWingIncidenceFactor)
{
    const double incidenceFactor = Naca1307FinBodyInterference::calculateWingIncidenceFactor(0.2);
    const double angleOfAttackFactor = Naca1307FinBodyInterference::calculateFinInBodyFactor(0.2);

    EXPECT_NEAR(0.94, incidenceFactor, 0.005);
    EXPECT_NEAR(1.16, angleOfAttackFactor, 0.005);
    EXPECT_LT(incidenceFactor, angleOfAttackFactor);
}

// FinBodyInterferenceTest.reproducesNacaChartThreeRectangularWingIncidenceFactors
/// Chart 3 replaces equation 19 for rectangular wings when beta*A exceeds two. These source
/// points are read from the beta*A=3 and 4 curves.
TEST(Naca1307FinBodyInterference, ReproducesNacaChartThreeRectangularWingIncidenceFactors)
{
    const double betaThreeMach = std::sqrt(10.0);
    const double betaFourMach  = std::sqrt(17.0);

    EXPECT_NEAR(
        0.940,
        Naca1307FinBodyInterference::calculateWingIncidenceFactor(0.2, betaThreeMach, 1.0, true),
        0.006);
    EXPECT_NEAR(
        0.965,
        Naca1307FinBodyInterference::calculateWingIncidenceFactor(0.2, betaFourMach, 1.0, true),
        0.006);
}

// FinBodyInterferenceTest.limitsChartThreeToItsPublishedSelectionRegion
/// The report's chart-selection condition must not leak into nonrectangular fins or below its
/// beta*A boundary.
TEST(Naca1307FinBodyInterference, LimitsChartThreeToItsPublishedSelectionRegion)
{
    const double slenderBodyFactor = Naca1307FinBodyInterference::calculateWingIncidenceFactor(0.2);
    const double betaTwoMach       = std::sqrt(5.0);
    const double betaThreeMach     = std::sqrt(10.0);

    EXPECT_NEAR(
        slenderBodyFactor,
        Naca1307FinBodyInterference::calculateWingIncidenceFactor(0.2, betaTwoMach, 1.0, true),
        kEpsilon);
    EXPECT_NEAR(
        slenderBodyFactor,
        Naca1307FinBodyInterference::calculateWingIncidenceFactor(0.2, betaThreeMach, 1.0, false),
        kEpsilon);
}

// FinBodyInterferenceTest.remainsContinuousAtChartThreeSelectionBoundary
/// The two approximations differ at the selection boundary, so the narrow engineering fairing
/// must prevent a Mach-dependent roll-force step.
TEST(Naca1307FinBodyInterference, RemainsContinuousAtChartThreeSelectionBoundary)
{
    const double delta     = 1.0e-6;
    const double belowMach = std::sqrt(1.0 + std::pow(2.0 - delta, 2));
    const double aboveMach = std::sqrt(1.0 + std::pow(2.0 + delta, 2));

    const double below =
        Naca1307FinBodyInterference::calculateWingIncidenceFactor(0.2, belowMach, 1.0, true);
    const double above =
        Naca1307FinBodyInterference::calculateWingIncidenceFactor(0.2, aboveMach, 1.0, true);

    EXPECT_NEAR(below, above, 1.0e-9);
}

// FinBodyInterferenceTest.reproducesNacaTableOneWingBodyExample
/// Reproduce the wing-body entries of the Mach-2 computing example in NACA Report 1307, table I.
/// The source values are rounded to two or three decimal places, so the assertions use the
/// corresponding chart precision.
TEST(Naca1307FinBodyInterference, ReproducesNacaTableOneWingBodyExample)
{
    const Naca1307FinBodyInterference model          = createTableOneModel(kInf);
    const double                      liftCurveSlope = 0.0406 * 180.0 / kPi;

    const Loads result = model.calculate(2.0, liftCurveSlope);

    EXPECT_NEAR(1.16, result.finFactor, 0.005);
    EXPECT_NEAR(0.23, result.bodyFactor, 0.01);
    EXPECT_NEAR(0.938, result.bodyCp / 2.25, 0.015);
    EXPECT_NEAR(0.94, result.incidenceFactor, 0.005);
}

// FinBodyInterferenceTest.matchesPublishedWingBodyExperiment
/// Validate against the independent wind-tunnel result for combination 2a in table II. The
/// report gives beta*CLa=5.69 experimentally and 6.01 from its method for this Mach-1.5
/// triangular wing-body configuration.
TEST(Naca1307FinBodyInterference, MatchesPublishedWingBodyExperiment)
{
    const double                      mach                   = 1.5;
    const double                      beta                   = std::sqrt((mach * mach) - 1.0);
    const double                      totalSemispan          = 1.0;
    const double                      bodyRadius             = 0.201 * totalSemispan;
    const double                      exposedSpan            = totalSemispan - bodyRadius;
    const double                      rootChord              = exposedSpan;
    const double                      betaWingLiftCurveSlope = 4.0;
    const double                      betaNoseLiftCurveSlope = 0.44;
    const Naca1307FinBodyInterference model{bodyRadius, exposedSpan, rootChord, 0.0,
                                            rootChord,  4.0,         kInf};

    const Loads  result = model.calculate(mach, betaWingLiftCurveSlope / beta);
    const double estimatedBetaLiftCurveSlope =
        betaNoseLiftCurveSlope + (betaWingLiftCurveSlope * (result.finFactor + result.bodyFactor));

    EXPECT_NEAR(5.69, estimatedBetaLiftCurveSlope, 0.569);
}

// FinBodyInterferenceTest.reproducesNacaChartSixteenLiftingLineEndpoint
/// At beta*A=7, the slowest chart-16 family has reached its Appendix-D lifting-line result. The
/// no-trailing-edge-sweep triangular case at r/s=0.2 reads about 0.41 root chords from the chart.
TEST(Naca1307FinBodyInterference, ReproducesNacaChartSixteenLiftingLineEndpoint)
{
    const Naca1307FinBodyInterference model{0.562, 2.25, 2.25, 0.0, 2.25, 7.0, kInf};
    const Loads                       result = model.calculate(0.0, 2.4);

    EXPECT_NEAR(0.41, result.bodyCp / 2.25, 0.015);
    const double radiusSemispanRatio = 0.562 / (0.562 + 2.25);
    EXPECT_NEAR(std::pow(1.0 + radiusSemispanRatio, 2), result.finFactor + result.bodyFactor,
                kEpsilon);
}

// FinBodyInterferenceTest.reproducesNacaChartSixteenRadiusFamily
/// The nonzero-radius curves in chart 16(g) reach their lifting-line value near beta*A=4, unlike
/// the r/s=0 curve which continues to about seven.
TEST(Naca1307FinBodyInterference, ReproducesNacaChartSixteenRadiusFamily)
{
    const double                      rootChord = 2.25;
    const Naca1307FinBodyInterference model{0.5625, 2.25, rootChord, 0.0, rootChord, 4.0, kInf};

    const double bodyCp = model.calculate(0.0, 2.4).bodyCp / rootChord;

    EXPECT_NEAR(0.41, bodyCp, 0.015);
}

// FinBodyInterferenceTest.reproducesNacaChartSixteenPlanformFamilies
/// Reproduce interior points from the two triangular extremes in chart 16. These cases ensure
/// that the slow no-trailing-edge-sweep fairing is not replaced by the much faster
/// no-leading-edge-sweep curve.
TEST(Naca1307FinBodyInterference, ReproducesNacaChartSixteenPlanformFamilies)
{
    const double                      bodyRadius  = 0.562;
    const double                      exposedSpan = 2.248;
    const double                      rootChord   = 4.0 * exposedSpan;
    const Naca1307FinBodyInterference noLeadingEdgeSweep{bodyRadius, exposedSpan, rootChord, 0.0,
                                                         0.0,        1.0,         kInf};
    const Naca1307FinBodyInterference noTrailingEdgeSweep{bodyRadius, exposedSpan, rootChord, 0.0,
                                                          rootChord,  1.0,         kInf};

    const double noLeadingEdgeCp  = noLeadingEdgeSweep.calculate(0.0, 2.4).bodyCp / rootChord;
    const double noTrailingEdgeCp = noTrailingEdgeSweep.calculate(0.0, 2.4).bodyCp / rootChord;

    EXPECT_NEAR(0.14, noLeadingEdgeCp, 0.01);
    EXPECT_NEAR(0.46, noTrailingEdgeCp, 0.01);
}

// FinBodyInterferenceTest.accountsForFiniteAfterbody
/// Clipping the pressure field at the root trailing edge represents the report's no-afterbody
/// case and must move the body load forward.
TEST(Naca1307FinBodyInterference, AccountsForFiniteAfterbody)
{
    const double liftCurveSlope   = 0.0406 * 180.0 / kPi;
    const Loads  withAfterbody    = createTableOneModel(kInf).calculate(2.0, liftCurveSlope);
    const Loads  withoutAfterbody = createTableOneModel(2.25).calculate(2.0, liftCurveSlope);

    EXPECT_LT(withoutAfterbody.bodyFactor, withAfterbody.bodyFactor);
    EXPECT_LT(withoutAfterbody.bodyCp, withAfterbody.bodyCp);
}

// FinBodyInterferenceTest.accountsForFiniteAfterbodyAtLowAspectRatio
/// Chart 15 supplies the low-aspect-ratio CP and remains compatible with a finite afterbody by
/// retaining the same normalized fairing.
TEST(Naca1307FinBodyInterference, AccountsForFiniteAfterbodyAtLowAspectRatio)
{
    const double                      rootChord      = 4.0 / 3.0;
    const double                      liftCurveSlope = 2.4;
    const Naca1307FinBodyInterference withAfterbody{0.25, 1.0, rootChord, rootChord,
                                                    0.0,  1.5, kInf};
    const Naca1307FinBodyInterference withoutAfterbody{0.25, 1.0, rootChord, rootChord,
                                                       0.0,  1.5, rootChord};

    const Loads withResult    = withAfterbody.calculate(1.6, liftCurveSlope);
    const Loads withoutResult = withoutAfterbody.calculate(1.6, liftCurveSlope);

    EXPECT_NEAR(withResult.bodyFactor, withoutResult.bodyFactor, kEpsilon);
    EXPECT_LT(withoutResult.bodyCp, withResult.bodyCp);
}

// FinBodyInterferenceTest.reproducesNacaChartFifteenRectangularRadiusFamilies
/// Chart 15(c) publishes independent radius-family curves. Checking their interior ordinates
/// prevents them from being collapsed back onto a shared curve with small abscissa adjustments.
TEST(Naca1307FinBodyInterference, ReproducesNacaChartFifteenRectangularRadiusFamilies)
{
    const Naca1307FinBodyInterference model{0.25, 1.0, 1.0, 1.0, 0.0, 2.0, kInf};

    EXPECT_NEAR(0.359, model.interpolateChart15(0.25, 0.2), 0.006);
    EXPECT_NEAR(0.705, model.interpolateChart15(0.50, 0.2), 0.006);
    EXPECT_NEAR(0.872, model.interpolateChart15(0.75, 0.2), 0.006);
    EXPECT_NEAR(0.336, model.interpolateChart15(0.25, 0.4), 0.006);
    EXPECT_NEAR(0.690, model.interpolateChart15(0.50, 0.4), 0.006);
    EXPECT_NEAR(0.858, model.interpolateChart15(0.75, 0.4), 0.006);
    EXPECT_NEAR(0.275, model.interpolateChart15(0.25, 0.6), 0.006);
    EXPECT_NEAR(0.550, model.interpolateChart15(0.50, 0.6), 0.006);
    EXPECT_NEAR(0.775, model.interpolateChart15(0.75, 0.6), 0.006);
}

// FinBodyInterferenceTest.rejectsGeometryThatRequiresMissingChartFifteenPanel
/// Report 1307 explicitly omits the chart-15(a) low-aspect-ratio extrapolation. A geometry that
/// needs that panel must use the fallback, while either adjacent published family remains usable.
TEST(Naca1307FinBodyInterference, RejectsGeometryThatRequiresMissingChartFifteenPanel)
{
    const Naca1307FinBodyInterference missingPanel{0.2, 0.8, 1.0, 0.0, 0.0, 2.0, kInf};
    const Naca1307FinBodyInterference halfTaperBoundary{0.2, 0.8, 1.0, 0.5, 0.0, 2.0, kInf};
    const Naca1307FinBodyInterference midchordBoundary{0.2, 0.8, 1.0, 0.0, 0.5, 2.0, kInf};

    EXPECT_FALSE(missingPanel.isApplicable());
    EXPECT_TRUE(halfTaperBoundary.isApplicable());
    EXPECT_TRUE(midchordBoundary.isApplicable());
}

// FinBodyInterferenceTest.remainsContinuousAtEquationTwentyTwoBoundary
/// Force and moment must not jump where equation 22 changes from chart 15 to the planar pressure
/// model.
TEST(Naca1307FinBodyInterference, RemainsContinuousAtEquationTwentyTwoBoundary)
{
    const double                      rootChord    = 4.0 / 3.0;
    const double                      boundaryMach = 5.0 / 3.0;
    const double                      delta        = 1.0e-7;
    const Naca1307FinBodyInterference model{0.25, 1.0, rootChord, rootChord, 0.0, 1.5, kInf};

    const Loads below = model.calculate(boundaryMach - delta, 2.4);
    const Loads above = model.calculate(boundaryMach + delta, 2.4);

    EXPECT_NEAR(below.bodyFactor, above.bodyFactor, 1.0e-6);
    EXPECT_NEAR(below.bodyCp, above.bodyCp, 1.0e-5);
    EXPECT_GT(below.bodyCp, 0.5 * rootChord);
}

// FinBodyInterferenceTest.remainsContinuousAtSonicLeadingEdgeLimit
/// Equations 23 and 25 have a finite common limit at m*beta=1.
TEST(Naca1307FinBodyInterference, RemainsContinuousAtSonicLeadingEdgeLimit)
{
    const double                      boundaryMach = std::sqrt(5.0);
    const double                      delta        = 1.0e-8;
    const Naca1307FinBodyInterference model{0.25, 1.0, 2.0, 0.0, 2.0, 2.0, kInf};

    const Loads below = model.calculate(boundaryMach - delta, 2.4);
    const Loads at    = model.calculate(boundaryMach, 2.4);
    const Loads above = model.calculate(boundaryMach + delta, 2.4);

    EXPECT_NEAR(below.bodyFactor, at.bodyFactor, 1.0e-5);
    EXPECT_NEAR(at.bodyFactor, above.bodyFactor, 1.0e-5);
    EXPECT_NEAR(below.bodyCp, at.bodyCp, 1.0e-5);
    EXPECT_NEAR(at.bodyCp, above.bodyCp, 1.0e-5);
}

// FinBodyInterferenceTest.reproducesNacaChartFourSubsonicLeadingEdgeBranch
/// Chart 4(a), printed page 49, includes the subsonic-leading-edge branch. At beta*m=0.5 and
/// 2*beta*r/c_r=1.5, its ordinate is approximately 1.52.
TEST(Naca1307FinBodyInterference, ReproducesNacaChartFourSubsonicLeadingEdgeBranch)
{
    const double                      mach                   = 1.5;
    const double                      beta                   = std::sqrt((mach * mach) - 1.0);
    const double                      bodyRadius             = 0.6;
    const double                      exposedSpan            = 0.4;
    const double                      rootChord              = 2.0 * beta * bodyRadius / 1.5;
    const double                      betaWingLiftCurveSlope = 4.0;
    const Naca1307FinBodyInterference model{
        bodyRadius, exposedSpan, rootChord, 0.0, rootChord, 4.0 * exposedSpan / rootChord, kInf};

    const Loads  result        = model.calculate(mach, betaWingLiftCurveSlope / beta);
    const double totalSemispan = bodyRadius + exposedSpan;
    const double chartOrdinate =
        result.bodyFactor * betaWingLiftCurveSlope * ((totalSemispan / bodyRadius) - 1.0);

    EXPECT_NEAR(1.52, chartOrdinate, 0.10);
}

// FinBodyInterferenceTest.preservesAbsolutePlanarLoadAcrossWingSlopeNormalization
/// The planar carryover is an absolute NACA load. Its stored factor changes inversely with the
/// caller's isolated-wing slope, so the applied load and pressure center remain unchanged when
/// only that normalization changes.
TEST(Naca1307FinBodyInterference, PreservesAbsolutePlanarLoadAcrossWingSlopeNormalization)
{
    const Naca1307FinBodyInterference model  = createTableOneModel(kInf);
    const Loads                       first  = model.calculate(2.0, 2.4);
    const Loads                       second = model.calculate(2.0, 3.6);

    EXPECT_NEAR(first.bodyFactor * 2.4, second.bodyFactor * 3.6, kEpsilon);
    EXPECT_NEAR(first.bodyCp, second.bodyCp, kEpsilon);
}

/// FinBodyInterferenceTest.AfterbodyFixture and createAfterbodyFixture(): a stage of two body
/// tubes, the first (1 m long, radius 0.1 m) with four trapezoidal fins at its end, the second
/// 2 m long with the radius @p followingRadius.
struct AfterbodyFixture
{
    Rocket           rocket;
    AxialStage&      stage{rocket.addChild(std::make_unique<AxialStage>())};
    BodyTube&        firstTube{stage.addChild(std::make_unique<BodyTube>(1.0, 0.1))};
    BodyTube&        followingTube;
    TrapezoidFinSet& fins;

    explicit AfterbodyFixture(double followingRadius)
      : followingTube{stage.addChild(std::make_unique<BodyTube>(2.0, followingRadius))},
        fins{firstTube.addChild(std::make_unique<TrapezoidFinSet>(4, 0.4, 0.2, 0.1, 0.2))}
    {
        fins.setAxialMethod(AxialMethod::BOTTOM);
        fins.setAxialOffset(0.0);
        rocket.enableEvents();
    }
};

// FinBodyInterferenceTest.includesFollowingEqualRadiusBodyTubeInAfterbody
/// Flush equal-radius body tubes are one physical cylinder for the pressure model, even when the
/// design tree splits them into separate components.
TEST(FinBodyInterference, IncludesFollowingEqualRadiusBodyTubeInAfterbody)
{
    const AfterbodyFixture fixture{0.1};

    const double bodyEnd =
        FinSetCalc::calculateCylindricalAfterbodyEnd(fixture.fins, fixture.firstTube);

    EXPECT_NEAR(2.4, bodyEnd, kEpsilon);
}

// FinBodyInterferenceTest.stopsAfterbodyAtRadiusChange
/// A radius change invalidates the constant-cylinder assumption and ends the NACA integration at
/// the parent tube rather than extrapolating aft.
TEST(FinBodyInterference, StopsAfterbodyAtRadiusChange)
{
    const AfterbodyFixture fixture{0.12};

    const double bodyEnd =
        FinSetCalc::calculateCylindricalAfterbodyEnd(fixture.fins, fixture.firstTube);

    EXPECT_NEAR(0.4, bodyEnd, kEpsilon);
}

// FinBodyInterferenceTest.remainsContinuousAtTransonicBoundaries
/// The body load and its moment are blended together through the transonic interval, avoiding a
/// CP discontinuity at either endpoint.
TEST(Naca1307FinBodyInterference, RemainsContinuousAtTransonicBoundaries)
{
    const Naca1307FinBodyInterference model          = createTableOneModel(kInf);
    const double                      liftCurveSlope = 0.0406 * 180.0 / kPi;

    const Loads belowSubsonic   = model.calculate(0.9 - 1.0e-7, liftCurveSlope);
    const Loads aboveSubsonic   = model.calculate(0.9 + 1.0e-7, liftCurveSlope);
    const Loads belowSupersonic = model.calculate(1.5 - 1.0e-7, liftCurveSlope);
    const Loads aboveSupersonic = model.calculate(1.5 + 1.0e-7, liftCurveSlope);

    EXPECT_NEAR(belowSubsonic.bodyFactor, aboveSubsonic.bodyFactor, 1.0e-6);
    EXPECT_NEAR(belowSubsonic.bodyCp, aboveSubsonic.bodyCp, 1.0e-6);
    EXPECT_NEAR(belowSupersonic.bodyFactor, aboveSupersonic.bodyFactor, 1.0e-6);
    EXPECT_NEAR(belowSupersonic.bodyCp, aboveSupersonic.bodyCp, 1.0e-6);
}

// FinBodyInterferenceTest.fallbackIncludesBodyContributionAtSubsonicSpeeds
/// Unsupported geometries retain the simplified issue-2489 correction.
TEST(FinBodyInterference, FallbackIncludesBodyContributionAtSubsonicSpeeds)
{
    EXPECT_NEAR(1.5625, FinSetCalc::calculateBodyFinInterferenceFactor(kTau, 0.5), kEpsilon);
    EXPECT_NEAR(1.5625, FinSetCalc::calculateBodyFinInterferenceFactor(kTau, 0.9), kEpsilon);
}

// FinBodyInterferenceTest.fallbackBlendsBodyContributionThroughTransonicSpeeds
/// The fallback body term is smoothly removed through the transonic interval.
TEST(FinBodyInterference, FallbackBlendsBodyContributionThroughTransonicSpeeds)
{
    EXPECT_NEAR(1.40625, FinSetCalc::calculateBodyFinInterferenceFactor(kTau, 1.2), kEpsilon);
}

// FinBodyInterferenceTest.fallbackRetainsClassicalCorrectionAtSupersonicSpeeds
/// The fallback retains the established classical correction supersonically.
TEST(FinBodyInterference, FallbackRetainsClassicalCorrectionAtSupersonicSpeeds)
{
    EXPECT_NEAR(1.25, FinSetCalc::calculateBodyFinInterferenceFactor(kTau, 1.5), kEpsilon);
    EXPECT_NEAR(1.25, FinSetCalc::calculateBodyFinInterferenceFactor(kTau, 3.0), kEpsilon);
}

// FinBodyInterferenceTest.limitsNacaModelToForwardLinearAngles
/// The small-angle NACA model is smoothly removed before stall and is never reused for reverse
/// flow.
TEST(FinBodyInterference, LimitsNacaModelToForwardLinearAngles)
{
    EXPECT_NEAR(1.0, FinSetCalc::calculateNacaApplicabilityWeight(javaToRadians(10.0)), kEpsilon);
    EXPECT_NEAR(0.5, FinSetCalc::calculateNacaApplicabilityWeight(javaToRadians(15.0)), kEpsilon);
    EXPECT_NEAR(0.0, FinSetCalc::calculateNacaApplicabilityWeight(javaToRadians(20.0)), kEpsilon);
    EXPECT_NEAR(0.0, FinSetCalc::calculateNacaApplicabilityWeight(javaToRadians(160.0)), kEpsilon);
}

// ---- Beyond the JUnit tests (values pinned with OpenRocket's NACA1307FinBodyInterference on
// JDK 17) ----

/// The geometries of the pinned cases, one row each: body radius, exposed span, root chord, tip
/// chord, leading-edge sweep, aspect ratio and body end.
constexpr std::array<std::array<double, 7>, 11> kGeometries{{
    {0.562, 2.25, 2.25, 0.0, 2.25, 4.0, kInf},
    {0.562, 2.25, 2.25, 0.0, 2.25, 4.0, 2.25},
    {0.25, 1.0, 1.0, 1.0, 0.0, 2.0, kInf},
    {0.012, 0.03, 0.05, 0.025, 0.02, 1.6, 0.05},
    {0.25, 1.0, 1.3333333333333333, 1.3333333333333333, 0.0, 1.5, kInf},
    {0.25, 1.0, 2.0, 0.0, 2.0, 2.0, kInf},
    {0.6, 0.4, 0.8944271909999159, 0.0, 0.8944271909999159, 1.788854381999832, kInf},
    {0.1, 0.2, 0.3, 0.1, 0.15, 1.2, 0.5},
    {0.562, 2.248, 8.992, 0.0, 0.0, 1.0, kInf},
    {0.2, 0.8, 1.0, 0.0, 0.0, 2.0, kInf},
    {0.05, 0.1, 0.12, 0.06, 0.03, 2.5, 0.3},
}};

[[nodiscard]] Naca1307FinBodyInterference modelFor(double geometry)
{
    const std::array<double, 7>& g = kGeometries.at(static_cast<std::size_t>(geometry));
    return Naca1307FinBodyInterference{g[0], g[1], g[2], g[3], g[4], g[5], g[6]};
}

TEST(Naca1307FinBodyInterference, ApplicabilityIsJavas)
{
    constexpr std::array<bool, 11> kApplicable{
        true, true, true, true, true, true, true, true, false, false, true,
    };
    for (std::size_t i = 0; i < kGeometries.size(); i++)
    {
        EXPECT_EQ(modelFor(static_cast<double>(i)).isApplicable(), kApplicable.at(i))
            << "geometry " << i;
    }
}

TEST(Naca1307FinBodyInterference, ApplicabilityLimits)
{
    // The base: a trapezoid well inside every limit.
    EXPECT_TRUE((Naca1307FinBodyInterference{0.1, 0.2, 0.3, 0.2, 0.05, 1.5, 1.0}.isApplicable()));
    // Not positive radius, span, root chord or aspect ratio.
    EXPECT_FALSE((Naca1307FinBodyInterference{0.0, 0.2, 0.3, 0.2, 0.05, 1.5, 1.0}.isApplicable()));
    EXPECT_FALSE((Naca1307FinBodyInterference{0.1, 0.0, 0.3, 0.2, 0.05, 1.5, 1.0}.isApplicable()));
    EXPECT_FALSE((Naca1307FinBodyInterference{0.1, 0.2, 0.0, 0.0, 0.0, 1.5, 1.0}.isApplicable()));
    EXPECT_FALSE((Naca1307FinBodyInterference{0.1, 0.2, 0.3, 0.2, 0.05, 0.0, 1.0}.isApplicable()));
    EXPECT_FALSE((Naca1307FinBodyInterference{kNaN, 0.2, 0.3, 0.2, 0.05, 1.5, 1.0}.isApplicable()));
    // A tip chord below zero or above the root chord.
    EXPECT_FALSE((Naca1307FinBodyInterference{0.1, 0.2, 0.3, -0.1, 0.05, 1.5, 1.0}.isApplicable()));
    EXPECT_FALSE((Naca1307FinBodyInterference{0.1, 0.2, 0.3, 0.4, 0.0, 1.5, 1.0}.isApplicable()));
}

TEST(Naca1307FinBodyInterference, ApplicabilityLimitsOfTheBodyAndPlanform)
{
    // A body that ends before the root trailing edge (a 1e-6 tolerance), or a NaN end.
    EXPECT_FALSE((Naca1307FinBodyInterference{0.1, 0.2, 0.3, 0.2, 0.05, 1.5, 0.29}.isApplicable()));
    EXPECT_TRUE(
        (Naca1307FinBodyInterference{0.1, 0.2, 0.3, 0.2, 0.05, 1.5, 0.3 - 5e-7}.isApplicable()));
    EXPECT_FALSE((Naca1307FinBodyInterference{0.1, 0.2, 0.3, 0.2, 0.05, 1.5, kNaN}.isApplicable()));
    // r/s above 0.6.
    EXPECT_FALSE((Naca1307FinBodyInterference{0.4, 0.2, 0.3, 0.2, 0.05, 1.5, 1.0}.isApplicable()));
    EXPECT_TRUE((Naca1307FinBodyInterference{0.3, 0.2, 0.3, 0.2, 0.05, 1.5, 1.0}.isApplicable()));
    // A forward-swept leading edge, or an aft-swept trailing edge.
    EXPECT_FALSE((Naca1307FinBodyInterference{0.1, 0.2, 0.3, 0.2, -0.01, 1.5, 1.0}.isApplicable()));
    EXPECT_FALSE((Naca1307FinBodyInterference{0.1, 0.2, 0.3, 0.2, 0.2, 1.5, 1.0}.isApplicable()));
}

/// Checks one row of kLoads: geometry, Mach, slope, then the pinned finFactor, bodyFactor,
/// bodyCp and incidenceFactor.
void expectLoads(const std::array<double, 7>& row)
{
    SCOPED_TRACE(::testing::Message()
                 << "geometry " << row[0] << " Mach " << row[1] << " slope " << row[2]);
    const Loads loads = modelFor(row[0]).calculate(row[1], row[2]);
    expectPinned(loads.finFactor, row[3]);
    expectPinned(loads.bodyFactor, row[4]);
    expectPinned(loads.bodyCp, row[5]);
    expectPinned(loads.incidenceFactor, row[6]);
}

TEST(Naca1307FinBodyInterference, LoadsAreJavas)
{
    // geometry, Mach, slope, finFactor, bodyFactor, bodyCp, incidenceFactor
    constexpr std::array<std::array<double, 7>, 168> kLoads{{
        {0, 0.0, 2.4, 1.16151337433109, 0.2781452518776599, 0.9327844230772511, 0.9438914069383321},
        {0, 0.0, 3.6, 1.16151337433109, 0.2781452518776599, 0.9327844230772511, 0.9438914069383321},
        {0, 0.3, 2.4, 1.16151337433109, 0.2781452518776599, 0.9340261921255549, 0.9438914069383321},
        {0, 0.3, 3.6, 1.16151337433109, 0.2781452518776599, 0.9340261921255549, 0.9438914069383321},
        {0, 0.8, 2.4, 1.16151337433109, 0.2781452518776599, 0.956637661650862, 0.9438914069383321},
        {0, 0.8, 3.6, 1.16151337433109, 0.2781452518776599, 0.956637661650862, 0.9438914069383321},
        {0, 0.9, 2.4, 1.16151337433109, 0.2781452518776599, 0.9833266699731211, 0.9438914069383321},
        {0, 0.9, 3.6, 1.16151337433109, 0.2781452518776599, 0.9833266699731211, 0.9438914069383321},
        {0, 1.0, 2.4, 1.16151337433109, 0.2866397299662671, 1.1435029163786021, 0.9438914069383321},
        {0, 1.0, 3.6, 1.16151337433109, 0.26835572327686136, 1.0973864099233643,
         0.9438914069383321},
        {0, 1.2, 2.4, 1.16151337433109, 0.3036286861434814, 1.4369683525147463, 0.9438914069383321},
        {0, 1.2, 3.6, 1.16151337433109, 0.24877666607526422, 1.3524358532498755,
         0.9438914069383321},
        {0, 1.5, 2.4, 1.16151337433109, 0.3291121204093028, 1.820358304534208, 0.9438914069383321},
        {0, 1.5, 3.6, 1.16151337433109, 0.2194080802728685, 1.820358304534208, 0.9438914069383321},
        {0, 1.8, 2.4, 1.16151337433109, 0.25490720155623114, 2.0109713272012337,
         0.9438914069383321},
        {0, 1.8, 3.6, 1.16151337433109, 0.16993813437082075, 2.0109713272012337,
         0.9438914069383321},
        {0, 2.0, 2.4, 1.16151337433109, 0.22251368004752556, 2.1273086685394875,
         0.9438914069383321},
        {0, 2.0, 3.6, 1.16151337433109, 0.14834245336501703, 2.1273086685394875,
         0.9438914069383321},
        {0, 2.5, 2.4, 1.16151337433109, 0.1690542554538835, 2.3986510764828686, 0.9438914069383321},
        {0, 2.5, 3.6, 1.16151337433109, 0.11270283696925566, 2.3986510764828686,
         0.9438914069383321},
        {0, 3.0, 2.4, 1.16151337433109, 0.1358303566320321, 2.6540561918934067, 0.9438914069383321},
        {0, 3.0, 3.6, 1.16151337433109, 0.09055357108802141, 2.6540561918934067,
         0.9438914069383321},
        {0, 5.0, 2.4, 1.16151337433109, 0.07345157303748637, 3.607606778560258, 0.9438914069383321},
        {0, 5.0, 3.6, 1.16151337433109, 0.04896771535832423, 3.607606778560258, 0.9438914069383321},
        {1, 0.0, 2.4, 1.16151337433109, 0.2781452518776599, 0.9327844230772511, 0.9438914069383321},
        {1, 0.3, 2.4, 1.16151337433109, 0.2781452518776599, 0.9340261921255549, 0.9438914069383321},
        {1, 0.8, 2.4, 1.16151337433109, 0.2781452518776599, 0.956637661650862, 0.9438914069383321},
        {1, 0.9, 2.4, 1.16151337433109, 0.2781452518776599, 0.9833266699731211, 0.9438914069383321},
        {1, 1.0, 2.4, 1.16151337433109, 0.269887139698241, 1.049250300267751, 0.9438914069383321},
        {1, 1.2, 2.4, 1.16151337433109, 0.25337091533940315, 1.193989444683179, 0.9438914069383321},
        {1, 1.5, 2.4, 1.16151337433109, 0.22859657880114637, 1.4503137118278187,
         0.9438914069383321},
        {1, 1.8, 2.4, 1.16151337433109, 0.15340224392443944, 1.4859988213299151,
         0.9438914069383321},
        {1, 2.0, 2.4, 1.16151337433109, 0.12218114715682331, 1.4967746840283858,
         0.9438914069383321},
        {1, 2.5, 2.4, 1.16151337433109, 0.07569769915479346, 1.4999333829787864,
         0.9438914069383321},
        {1, 3.0, 2.4, 1.16151337433109, 0.052120749183299245, 1.4999317326390709,
         0.9438914069383321},
        {1, 5.0, 2.4, 1.16151337433109, 0.019082799961445344, 1.4999285021395161,
         0.9438914069383321},
        {2, 0.0, 2.4, 1.1616386696109016, 0.2783613303890984, 0.215, 0.9438733078572681},
        {2, 0.3, 2.4, 1.1616386696109016, 0.2783613303890984, 0.2117757440991862,
         0.9438733078572681},
        {2, 0.8, 2.4, 1.1616386696109016, 0.2783613303890984, 0.178, 0.9438733078572681},
        {2, 0.9, 2.4, 1.1616386696109016, 0.2783613303890984, 0.15153687781435413,
         0.9438733078572681},
        {2, 1.0, 2.4, 1.1616386696109016, 0.277401377593229, 0.2589612742646966,
         0.9438733078572681},
        {2, 1.2, 2.4, 1.1616386696109016, 0.27548147200149026, 0.47605607701064323,
         0.9438733078572681},
        {2, 1.5, 2.4, 1.1616386696109016, 0.27260161361388213, 0.8074319438939334,
         0.8832538211627501},
        {2, 1.8, 2.4, 1.1616386696109016, 0.19220079407534704, 0.8946104197378073,
         0.9394994432064364},
        {2, 2.0, 2.4, 1.1616386696109016, 0.16090572861228727, 0.9474621438310432,
         0.9516025403784438},
        {2, 2.5, 2.4, 1.1616386696109016, 0.11389592256975278, 1.0700525714773266,
         0.969449495366961},
        {2, 3.0, 2.4, 1.1616386696109016, 0.08742790972979893, 1.1848712141180155,
         0.9752512626584708},
        {2, 5.0, 2.4, 1.1616386696109016, 0.04311301333603279, 1.6112509833167048,
         0.9857113098337648},
        {3, 0.0, 2.4, 1.2393926695788287, 0.41366855491096755, 0.014401768349664466,
         0.9364698172146929},
        {3, 0.3, 2.4, 1.2393926695788287, 0.41366855491096755, 0.014290766494285078,
         0.9364698172146929},
        {3, 0.8, 2.4, 1.2393926695788287, 0.41366855491096755, 0.013216773821737162,
         0.9364698172146929},
        {3, 0.9, 2.4, 1.2393926695788287, 0.41366855491096755, 0.012548611045969672,
         0.9364698172146929},
        {3, 1.0, 2.4, 1.2393926695788287, 0.39260404346535127, 0.01492961397930104,
         0.9364698172146929},
        {3, 1.2, 2.4, 1.2393926695788287, 0.3504750205741187, 0.020550248745547756,
         0.9364698172146929},
        {3, 1.5, 2.4, 1.2393926695788287, 0.2872814862372699, 0.03207213951358627,
         0.9364698172146929},
        {3, 1.8, 2.4, 1.2393926695788287, 0.18994805987028685, 0.03292248492405351,
         0.9364698172146929},
        {3, 2.0, 2.4, 1.2393926695788287, 0.15030695595075724, 0.03320877664239938,
         0.9364698172146929},
        {3, 2.5, 2.4, 1.2393926695788287, 0.0916732515581634, 0.033331787170165265,
         0.9364698172146929},
        {3, 3.0, 2.4, 1.2393926695788287, 0.062307084815600426, 0.03333176034310761,
         0.9364698172146929},
        {3, 5.0, 2.4, 1.2393926695788287, 0.02220813777374006, 0.033331708924914785,
         0.9364698172146929},
        {4, 0.0, 2.4, 1.1616386696109016, 0.2783613303890984, 0.2633333333333333,
         0.9438733078572681},
        {4, 0.0, 3.6, 1.1616386696109016, 0.2783613303890984, 0.2633333333333333,
         0.9438733078572681},
        {4, 0.3, 2.4, 1.1616386696109016, 0.2783613303890984, 0.25734542951753625,
         0.9438733078572681},
        {4, 0.3, 3.6, 1.1616386696109016, 0.2783613303890984, 0.25734542951753625,
         0.9438733078572681},
        {4, 0.8, 2.4, 1.1616386696109016, 0.2783613303890984, 0.20599999999999996,
         0.9438733078572681},
        {4, 0.8, 3.6, 1.1616386696109016, 0.2783613303890984, 0.20599999999999996,
         0.9438733078572681},
        {4, 0.9, 2.4, 1.1616386696109016, 0.2783613303890984, 0.17153687781435414,
         0.9438733078572681},
        {4, 0.9, 3.6, 1.1616386696109016, 0.2783613303890984, 0.17153687781435414,
         0.9438733078572681},
        {4, 1.0, 2.4, 1.1616386696109016, 0.2783613303890984, 0.3018591591263194,
         0.9438733078572681},
        {4, 1.0, 3.6, 1.1616386696109016, 0.2783613303890984, 0.3018591591263194,
         0.9438733078572681},
        {4, 1.2, 2.4, 1.1616386696109016, 0.2783613303890984, 0.5625037217502499,
         0.9438733078572681},
        {4, 1.2, 3.6, 1.1616386696109016, 0.2783613303890984, 0.5625037217502499,
         0.9438733078572681},
        {4, 1.5, 2.4, 1.1616386696109016, 0.2783613303890984, 0.9534705656861457,
         0.9438733078572681},
        {4, 1.5, 3.6, 1.1616386696109016, 0.2783613303890984, 0.9534705656861457,
         0.9438733078572681},
        {4, 1.8, 2.4, 1.1616386696109016, 0.20348729335368282, 1.0779715101773861,
         0.8834463714378439},
        {4, 1.8, 3.6, 1.1616386696109016, 0.13565819556912187, 1.0779715101773861,
         0.8834463714378439},
        {4, 2.0, 2.4, 1.1616386696109016, 0.17096020186226327, 1.1326530096735425,
         0.9098557158514987},
        {4, 2.0, 3.6, 1.1616386696109016, 0.1139734679081755, 1.1326530096735425,
         0.9098557158514987},
        {4, 2.5, 2.4, 1.1616386696109016, 0.12184668372319843, 1.2592468406353423,
         0.950923294280422},
        {4, 2.5, 3.6, 1.1616386696109016, 0.08123112248213228, 1.2592468406353423,
         0.950923294280422},
        {4, 3.0, 2.4, 1.1616386696109016, 0.09401462170129028, 1.3775636752106353,
         0.9670016835446278},
        {4, 3.0, 3.6, 1.1616386696109016, 0.06267641446752685, 1.3775636752106353,
         0.9670016835446278},
        {4, 5.0, 2.4, 1.1616386696109016, 0.04696678618998585, 1.8154314737478179,
         0.9809484131116863},
        {4, 5.0, 3.6, 1.1616386696109016, 0.031311190793323895, 1.8154314737478179,
         0.9809484131116863},
        {5, 0.0, 2.4, 1.1616386696109016, 0.2783613303890984, 0.8633917559635855,
         0.9438733078572681},
        {5, 0.0, 3.6, 1.1616386696109016, 0.2783613303890984, 0.8633917559635855,
         0.9438733078572681},
        {5, 0.3, 2.4, 1.1616386696109016, 0.2783613303890984, 0.8672457804117599,
         0.9438733078572681},
        {5, 0.3, 3.6, 1.1616386696109016, 0.2783613303890984, 0.8672457804117599,
         0.9438733078572681},
        {5, 0.8, 2.4, 1.1616386696109016, 0.2783613303890984, 0.905740311614874,
         0.9438733078572681},
        {5, 0.8, 3.6, 1.1616386696109016, 0.2783613303890984, 0.905740311614874,
         0.9438733078572681},
        {5, 0.9, 2.4, 1.1616386696109016, 0.2783613303890984, 0.9289385240337504,
         0.9438733078572681},
        {5, 0.9, 3.6, 1.1616386696109016, 0.2783613303890984, 0.9289385240337504,
         0.9438733078572681},
        {5, 1.0, 2.4, 1.1616386696109016, 0.2726099688940582, 0.9932402076831297,
         0.9438733078572681},
        {5, 1.0, 3.6, 1.1616386696109016, 0.259062571037455, 0.9740480379484628,
         0.9438733078572681},
        {5, 1.2, 2.4, 1.1616386696109016, 0.2611072459039779, 1.130341743451942,
         0.9438733078572681},
        {5, 1.2, 3.6, 1.1616386696109016, 0.22046505233416833, 1.0879594644857817,
         0.9438733078572681},
        {5, 1.5, 2.4, 1.1616386696109016, 0.24385316141885738, 1.3602459520982197,
         0.9438733078572681},
        {5, 1.5, 3.6, 1.1616386696109016, 0.16256877427923824, 1.3602459520982197,
         0.9438733078572681},
        {5, 1.8, 2.4, 1.1616386696109016, 0.21250217543029873, 1.4488208883918676,
         0.9438733078572681},
        {5, 1.8, 3.6, 1.1616386696109016, 0.14166811695353246, 1.4488208883918676,
         0.9438733078572681},
        {5, 2.0, 2.4, 1.1616386696109016, 0.19641411304547032, 1.5032655122819905,
         0.9438733078572681},
        {5, 2.0, 3.6, 1.1616386696109016, 0.13094274203031356, 1.5032655122819905,
         0.9438733078572681},
        {5, 2.5, 2.4, 1.1616386696109016, 0.16131972164418146, 1.631065820091083,
         0.9438733078572681},
        {5, 2.5, 3.6, 1.1616386696109016, 0.10754648109612096, 1.631065820091083,
         0.9438733078572681},
        {5, 3.0, 2.4, 1.1616386696109016, 0.13425399292691148, 1.751545566747897,
         0.9438733078572681},
        {5, 3.0, 3.6, 1.1616386696109016, 0.0895026619512743, 1.751545566747897,
         0.9438733078572681},
        {5, 5.0, 2.4, 1.1616386696109016, 0.07899921156042942, 2.2003332017365107,
         0.9438733078572681},
        {5, 5.0, 3.6, 1.1616386696109016, 0.05266614104028628, 2.2003332017365107,
         0.9438733078572681},
        {6, 0.0, 2.4, 1.554990040642583, 1.0050099593574175, 0.4576655978472933,
         0.9478605394657247},
        {6, 0.3, 2.4, 1.554990040642583, 1.0050099593574175, 0.4573759671403117,
         0.9478605394657247},
        {6, 0.8, 2.4, 1.554990040642583, 1.0050099593574175, 0.4542087665201725,
         0.9478605394657247},
        {6, 0.9, 2.4, 1.554990040642583, 1.0050099593574175, 0.4524864968158916,
         0.9478605394657247},
        {6, 1.0, 2.4, 1.554990040642583, 0.9788168443470879, 0.5417871663969209,
         0.9478605394657247},
        {6, 1.2, 2.4, 1.554990040642583, 0.9264306143264286, 0.7355373748169498,
         0.9478605394657247},
        {6, 1.5, 2.4, 1.554990040642583, 0.8478512692954396, 1.0710548903268544,
         0.9478605394657247},
        {6, 1.8, 2.4, 1.554990040642583, 0.7344738776386391, 1.255093447936412, 0.9478605394657247},
        {6, 2.0, 2.4, 1.554990040642583, 0.6763655526643737, 1.3684952536399482,
         0.9478605394657247},
        {6, 2.5, 2.4, 1.554990040642583, 0.562680234515931, 1.6350345030292202, 0.9478605394657247},
        {6, 3.0, 2.4, 1.554990040642583, 0.46174733120667066, 1.8858585006981787,
         0.9478605394657247},
        {6, 5.0, 2.4, 1.554990040642583, 0.26154352781588014, 2.832902734756122,
         0.9478605394657247},
        {7, 0.0, 2.4, 1.284384694257353, 0.49339308352042477, 0.09197443071164464,
         0.9349197014944814},
        {7, 0.3, 2.4, 1.284384694257353, 0.49339308352042477, 0.09145330176895106,
         0.9349197014944814},
        {7, 0.8, 2.4, 1.284384694257353, 0.49339308352042477, 0.08686142677442689,
         0.9349197014944814},
        {7, 0.9, 2.4, 1.284384694257353, 0.49339308352042477, 0.08443899282266737,
         0.9349197014944814},
        {7, 1.0, 2.4, 1.284384694257353, 0.49339308352042477, 0.11399604559605821,
         0.9349197014944814},
        {7, 1.2, 2.4, 1.284384694257353, 0.49339308352042477, 0.1731101511428399,
         0.9349197014944814},
        {7, 1.5, 2.4, 1.284384694257353, 0.49339308352042477, 0.2617813094630124,
         0.9349197014944814},
        {7, 1.8, 2.4, 1.284384694257353, 0.49339308352042477, 0.28756538923396385,
         0.9349197014944814},
        {7, 2.0, 2.4, 1.284384694257353, 0.49339308352042477, 0.2939252938625062,
         0.9349197014944814},
        {7, 2.5, 2.4, 1.284384694257353, 0.19301612965249734, 0.3031100595507329,
         0.9349197014944814},
        {7, 3.0, 2.4, 1.284384694257353, 0.13189318405516534, 0.3033500869023204,
         0.9349197014944814},
        {7, 5.0, 2.4, 1.284384694257353, 0.04746554974829631, 0.30365777908300823,
         0.9349197014944814},
        {8, 0.0, 2.4, 1.1616386696109016, 0.2783613303890984, 1.214168975129126,
         0.9438733078572681},
        {8, 0.3, 2.4, 1.1616386696109016, 0.2783613303890984, 1.1938030078783708,
         0.9438733078572681},
        {8, 0.8, 2.4, 1.1616386696109016, 0.2783613303890984, 0.9334362641166111,
         0.9438733078572681},
        {8, 0.9, 2.4, 1.1616386696109016, 0.2783613303890984, 0.7709218759933483,
         0.9438733078572681},
        {8, 1.0, 2.4, 1.1616386696109016, 0.2783613303890984, 1.1850379086703533,
         0.9438733078572681},
        {8, 1.2, 2.4, 1.1616386696109016, 0.2783613303890984, 2.0132699740243636,
         0.9438733078572681},
        {8, 1.5, 2.4, 1.1616386696109016, 0.2783613303890984, 3.2556180720553796,
         0.9438733078572681},
        {8, 1.8, 2.4, 1.1616386696109016, 0.2783613303890984, 4.191980566794018,
         0.9438733078572681},
        {8, 2.0, 2.4, 1.1616386696109016, 0.2783613303890984, 4.663399621822565,
         0.9438733078572681},
        {8, 2.5, 2.4, 1.1616386696109016, 0.2783613303890984, 5.635838552653053,
         0.9438733078572681},
        {8, 3.0, 2.4, 1.1616386696109016, 0.2783613303890984, 6.333277133292871,
         0.9438733078572681},
        {8, 5.0, 2.4, 1.1616386696109016, 0.12229088445301504, 7.484222905773739,
         0.9438733078572681},
        {9, 0.0, 2.4, 1.1616386696109016, 0.2783613303890984, 0.17366277858175186,
         0.9438733078572681},
        {9, 0.3, 2.4, 1.1616386696109016, 0.2783613303890984, 0.1717753631707611,
         0.9438733078572681},
        {9, 0.8, 2.4, 1.1616386696109016, 0.2783613303890984, 0.1448620750776321,
         0.9438733078572681},
        {9, 0.9, 2.4, 1.1616386696109016, 0.2783613303890984, 0.1260806428973559,
         0.9438733078572681},
        {9, 1.0, 2.4, 1.1616386696109016, 0.2783613303890984, 0.22624755331174823,
         0.9438733078572681},
        {9, 1.2, 2.4, 1.1616386696109016, 0.2783613303890984, 0.4265813741405329,
         0.9438733078572681},
        {9, 1.5, 2.4, 1.1616386696109016, 0.2783613303890984, 0.72708210538371, 0.9438733078572681},
        {9, 1.8, 2.4, 1.1616386696109016, 0.2783613303890984, 0.8530734695928693,
         0.9438733078572681},
        {9, 2.0, 2.4, 1.1616386696109016, 0.2783613303890984, 0.8954633184435753,
         0.9438733078572681},
        {9, 2.5, 2.4, 1.1616386696109016, 0.2401798691511592, 0.9698812897635519,
         0.9438733078572681},
        {9, 3.0, 2.4, 1.1616386696109016, 0.18510317552619032, 1.0638790722118074,
         0.9438733078572681},
        {9, 5.0, 2.4, 1.1616386696109016, 0.09219914089724385, 1.4120163569347908,
         0.9438733078572681},
        {10, 0.0, 2.4, 1.284384694257353, 0.49339308352042477, 0.032488306380824256,
         0.9349197014944814},
        {10, 0.3, 2.4, 1.284384694257353, 0.49339308352042477, 0.03230105706520324,
         0.9349197014944814},
        {10, 0.8, 2.4, 1.284384694257353, 0.49339308352042477, 0.02883723890833638,
         0.9349197014944814},
        {10, 0.9, 2.4, 1.284384694257353, 0.49339308352042477, 0.026306700733984088,
         0.9349197014944814},
        {10, 1.0, 2.4, 1.284384694257353, 0.5011712364690535, 0.04260526131124364,
         0.9349197014944814},
        {10, 1.2, 2.4, 1.284384694257353, 0.5167275423663108, 0.07373035684353405,
         0.9349197014944814},
        {10, 1.5, 2.4, 1.284384694257353, 0.5400620012121969, 0.11705594514114423,
         0.9349197014944814},
        {10, 1.8, 2.4, 1.284384694257353, 0.3918528787608087, 0.13339238107683132,
         0.9349197014944814},
        {10, 2.0, 2.4, 1.284384694257353, 0.33173907365733235, 0.14335319275786132,
         0.9349197014944814},
        {10, 2.5, 2.4, 1.284384694257353, 0.22853734283670496, 0.15990403815629137,
         0.9349197014944814},
        {10, 3.0, 2.4, 1.284384694257353, 0.159382688183031, 0.16491008745476163,
         0.9349197014944814},
        {10, 5.0, 2.4, 1.284384694257353, 0.05493064000540121, 0.16481590884658612,
         0.9349197014944814},
    }};
    for (const std::array<double, 7>& row : kLoads)
    {
        expectLoads(row);
    }
}

TEST(Naca1307FinBodyInterference, ChartFifteenIsJavas)
{
    // geometry, normalized criterion, r/s, value
    constexpr std::array<std::array<double, 4>, 176> kChart15{{
        {0, -0.1, 0.1, 0.0},
        {0, -0.1, 0.33, 0.0},
        {0, -0.1, 0.6, 0.0},
        {0, -0.1, 0.8, 0.0},
        {0, 0.3, 0.1, 0.502},
        {0, 0.3, 0.33, 0.502},
        {0, 0.3, 0.6, 0.502},
        {0, 0.3, 0.8, 0.502},
        {0, 0.77, 0.1, 0.9471999999999999},
        {0, 0.77, 0.33, 0.9471999999999999},
        {0, 0.77, 0.6, 0.9471999999999999},
        {0, 0.77, 0.8, 0.9471999999999999},
        {0, 1.2, 0.1, 1.0},
        {0, 1.2, 0.33, 1.0},
        {0, 1.2, 0.6, 1.0},
        {0, 1.2, 0.8, 1.0},
        {1, -0.1, 0.1, 0.0},
        {1, -0.1, 0.33, 0.0},
        {1, -0.1, 0.6, 0.0},
        {1, -0.1, 0.8, 0.0},
        {1, 0.3, 0.1, 0.502},
        {1, 0.3, 0.33, 0.502},
        {1, 0.3, 0.6, 0.502},
        {1, 0.3, 0.8, 0.502},
        {1, 0.77, 0.1, 0.9471999999999999},
        {1, 0.77, 0.33, 0.9471999999999999},
        {1, 0.77, 0.6, 0.9471999999999999},
        {1, 0.77, 0.8, 0.9471999999999999},
        {1, 1.2, 0.1, 1.0},
        {1, 1.2, 0.33, 1.0},
        {1, 1.2, 0.6, 1.0},
        {1, 1.2, 0.8, 1.0},
        {2, -0.1, 0.1, 0.0},
        {2, -0.1, 0.33, 0.0},
        {2, -0.1, 0.6, 0.0},
        {2, -0.1, 0.8, 0.0},
        {2, 0.3, 0.1, 0.32409999999999994},
        {2, 0.3, 0.33, 0.41429},
        {2, 0.3, 0.6, 0.33},
        {2, 0.3, 0.8, 0.33},
        {2, 0.77, 0.1, 0.91352},
        {2, 0.77, 0.33, 0.873868},
        {2, 0.77, 0.6, 0.793},
        {2, 0.77, 0.8, 0.793},
        {2, 1.2, 0.1, 1.0},
        {2, 1.2, 0.33, 1.0},
        {2, 1.2, 0.6, 1.0},
        {2, 1.2, 0.8, 1.0},
        {3, -0.1, 0.1, 0.0},
        {3, -0.1, 0.33, 0.0},
        {3, -0.1, 0.6, 0.0},
        {3, -0.1, 0.8, 0.0},
        {3, 0.3, 0.1, 0.49768},
        {3, 0.3, 0.33, 0.48100479999999995},
        {3, 0.3, 0.6, 0.455344},
        {3, 0.3, 0.8, 0.455344},
        {3, 0.77, 0.1, 0.9444279999999999},
        {3, 0.77, 0.33, 0.92839912},
        {3, 0.77, 0.6, 0.8995776},
        {3, 0.77, 0.8, 0.8995776},
        {3, 1.2, 0.1, 1.0},
        {3, 1.2, 0.33, 1.0},
        {3, 1.2, 0.6, 1.0},
        {3, 1.2, 0.8, 1.0},
        {4, -0.1, 0.1, 0.0},
        {4, -0.1, 0.33, 0.0},
        {4, -0.1, 0.6, 0.0},
        {4, -0.1, 0.8, 0.0},
        {4, 0.3, 0.1, 0.32409999999999994},
        {4, 0.3, 0.33, 0.41429},
        {4, 0.3, 0.6, 0.33},
        {4, 0.3, 0.8, 0.33},
        {4, 0.77, 0.1, 0.91352},
        {4, 0.77, 0.33, 0.873868},
        {4, 0.77, 0.6, 0.793},
        {4, 0.77, 0.8, 0.793},
        {4, 1.2, 0.1, 1.0},
        {4, 1.2, 0.33, 1.0},
        {4, 1.2, 0.6, 1.0},
        {4, 1.2, 0.8, 1.0},
        {5, -0.1, 0.1, 0.0},
        {5, -0.1, 0.33, 0.0},
        {5, -0.1, 0.6, 0.0},
        {5, -0.1, 0.8, 0.0},
        {5, 0.3, 0.1, 0.502},
        {5, 0.3, 0.33, 0.502},
        {5, 0.3, 0.6, 0.502},
        {5, 0.3, 0.8, 0.502},
        {5, 0.77, 0.1, 0.9471999999999999},
        {5, 0.77, 0.33, 0.9471999999999999},
        {5, 0.77, 0.6, 0.9471999999999999},
        {5, 0.77, 0.8, 0.9471999999999999},
        {5, 1.2, 0.1, 1.0},
        {5, 1.2, 0.33, 1.0},
        {5, 1.2, 0.6, 1.0},
        {5, 1.2, 0.8, 1.0},
        {6, -0.1, 0.1, 0.0},
        {6, -0.1, 0.33, 0.0},
        {6, -0.1, 0.6, 0.0},
        {6, -0.1, 0.8, 0.0},
        {6, 0.3, 0.1, 0.502},
        {6, 0.3, 0.33, 0.502},
        {6, 0.3, 0.6, 0.502},
        {6, 0.3, 0.8, 0.502},
        {6, 0.77, 0.1, 0.9471999999999999},
        {6, 0.77, 0.33, 0.9471999999999999},
        {6, 0.77, 0.6, 0.9471999999999999},
        {6, 0.77, 0.8, 0.9471999999999999},
        {6, 1.2, 0.1, 1.0},
        {6, 1.2, 0.33, 1.0},
        {6, 1.2, 0.6, 1.0},
        {6, 1.2, 0.8, 1.0},
        {7, -0.1, 0.1, 0.0},
        {7, -0.1, 0.33, 0.0},
        {7, -0.1, 0.6, 0.0},
        {7, -0.1, 0.8, 0.0},
        {7, 0.3, 0.1, 0.4984},
        {7, 0.3, 0.33, 0.48403599999999997},
        {7, 0.3, 0.6, 0.46168},
        {7, 0.3, 0.8, 0.46168},
        {7, 0.77, 0.1, 0.94489},
        {7, 0.77, 0.33, 0.9309987333333334},
        {7, 0.77, 0.6, 0.905872},
        {7, 0.77, 0.8, 0.905872},
        {7, 1.2, 0.1, 1.0},
        {7, 1.2, 0.33, 1.0},
        {7, 1.2, 0.6, 1.0},
        {7, 1.2, 0.8, 1.0},
        {8, -0.1, 0.1, 0.0},
        {8, -0.1, 0.33, 0.0},
        {8, -0.1, 0.6, 0.0},
        {8, -0.1, 0.8, 0.0},
        {8, 0.3, 0.1, 0.49768},
        {8, 0.3, 0.33, 0.48212799999999995},
        {8, 0.3, 0.6, 0.45880000000000004},
        {8, 0.3, 0.8, 0.45880000000000004},
        {8, 0.77, 0.1, 0.9444279999999999},
        {8, 0.77, 0.33, 0.9296804},
        {8, 0.77, 0.6, 0.90352},
        {8, 0.77, 0.8, 0.90352},
        {8, 1.2, 0.1, 1.0},
        {8, 1.2, 0.33, 1.0},
        {8, 1.2, 0.6, 1.0},
        {8, 1.2, 0.8, 1.0},
        {9, -0.1, 0.1, 0.0},
        {9, -0.1, 0.33, 0.0},
        {9, -0.1, 0.6, 0.0},
        {9, -0.1, 0.8, 0.0},
        {9, 0.3, 0.1, 0.49768},
        {9, 0.3, 0.33, 0.48212799999999995},
        {9, 0.3, 0.6, 0.45880000000000004},
        {9, 0.3, 0.8, 0.45880000000000004},
        {9, 0.77, 0.1, 0.9444279999999999},
        {9, 0.77, 0.33, 0.9296804},
        {9, 0.77, 0.6, 0.90352},
        {9, 0.77, 0.8, 0.90352},
        {9, 1.2, 0.1, 1.0},
        {9, 1.2, 0.33, 1.0},
        {9, 1.2, 0.6, 1.0},
        {9, 1.2, 0.8, 1.0},
        {10, -0.1, 0.1, 0.0},
        {10, -0.1, 0.33, 0.0},
        {10, -0.1, 0.6, 0.0},
        {10, -0.1, 0.8, 0.0},
        {10, 0.3, 0.1, 0.49768},
        {10, 0.3, 0.33, 0.47931999999999997},
        {10, 0.3, 0.6, 0.45016},
        {10, 0.3, 0.8, 0.45016},
        {10, 0.77, 0.1, 0.9444279999999999},
        {10, 0.77, 0.33, 0.9264772},
        {10, 0.77, 0.6, 0.893664},
        {10, 0.77, 0.8, 0.893664},
        {10, 1.2, 0.1, 1.0},
        {10, 1.2, 0.33, 1.0},
        {10, 1.2, 0.6, 1.0},
        {10, 1.2, 0.8, 1.0},
    }};
    for (const std::array<double, 4>& row : kChart15)
    {
        // Linear interpolation of the charts only: the same doubles as Java.
        EXPECT_EQ(modelFor(row[0]).interpolateChart15(row[1], row[2]), row[3])
            << "geometry " << row[0] << " criterion " << row[1] << " r/s " << row[2];
    }
}

/// The tau of the ill-conditioned row of kFactors.
constexpr double kIllConditionedTau = 0.999;

/// The relative tolerance of the fin-in-body and slender-body factors at kIllConditionedTau:
/// the braces of the fin factor cancel to 2e-6 of their terms there, so one ulp of atan(tau)
/// moves the fin factor (and the slender-body factor, which is its complement) by a relative
/// 1.4e-10. 1e-9 is seven such ulps; a pin at 1e-12 would demand that every math library
/// rounds that atan() as Java's does (Math.atan is itself specified to 1 ulp only).
constexpr double kIllConditionedRelative = 1e-9;

/// Checks one row of kFactors: tau, then the pinned fin-in-body, slender-body-in-fin and
/// equation-19 incidence factors.
void expectFactors(const std::array<double, 4>& row)
{
    SCOPED_TRACE(::testing::Message() << "tau " << row[0]);
    const double relative =
        row[0] == kIllConditionedTau ? kIllConditionedRelative : kPinnedRelative;
    const double finFactor = Naca1307FinBodyInterference::calculateFinInBodyFactor(row[0]);
    expectPinned(finFactor, row[1], relative);
    expectPinned(Naca1307FinBodyInterference::calculateSlenderBodyInFinFactor(row[0], finFactor),
                 row[2], relative);
    expectPinned(Naca1307FinBodyInterference::calculateWingIncidenceFactor(row[0]), row[3]);
}

TEST(Naca1307FinBodyInterference, SlenderBodyFactorsAreJavas)
{
    // The row of tau 0.999 is the ill-conditioned one: see kIllConditionedRelative. Its two
    // conditioned values are compared within a relative 1e-9, everything else within 1e-12.
    // tau, finFactor, slenderBodyFactor, incidenceFactor
    constexpr std::array<std::array<double, 4>, 14> kFactors{{
        {-1.0, 1.0, 0.0, 1.0},
        {0.0, 1.0, 0.0, 1.0},
        {1e-07, 1.0, 2.0000001010878066e-07, 1.0},
        {1e-06, 1.0, 2.0000009999243673e-06, 1.0},
        {2e-06, 1.0000014535227246, 2.5464812754005806e-06, 0.9999989070754387},
        {0.05, 1.0374416787258856, 0.06505832127411448, 0.9781532618102293},
        {0.2, 1.1616386696109016, 0.2783613303890984, 0.9438733078572681},
        {0.25, 1.2064644187487894, 0.3560355812512106, 0.9387581778430303},
        {0.5, 1.4502751208182116, 0.7997248791817884, 0.9396745909794577},
        {0.75, 1.7176864529503968, 1.3448135470496032, 0.9647839249907711},
        {0.999, 1.9988489021231197, 1.9971520978768809, 0.99984885464994},
        {1.0, 2.0, 2.0, 1.0},
        {1.5, 2.0, 4.25, 1.0},
        {kNaN, 1.0, kNaN, 1.0},
    }};
    for (const std::array<double, 4>& row : kFactors)
    {
        expectFactors(row);
    }
}

TEST(Naca1307FinBodyInterference, WingIncidenceSelectionIsJavas)
{
    // r/s, Mach, rectangular (1) or not (0), factor; the aspect ratio is 1
    constexpr std::array<std::array<double, 4>, 72> kIncidence{{
        {0.0, 0.5, 1.0, 1.0},
        {0.0, 1.0, 1.0, 1.0},
        {0.0, 1.1, 1.0, 1.0},
        {0.0, 2.23606797749979, 0.0, 1.0},
        {0.0, 2.23606797749979, 1.0, 1.0},
        {0.0, 2.3259406699226015, 1.0, 1.0},
        {0.0, 3.1622776601683795, 1.0, 1.0},
        {0.0, 3.640054944640259, 1.0, 1.0},
        {0.0, 4.123105625617661, 1.0, 1.0},
        {0.0, 5.0, 1.0, 1.0},
        {0.0, 10.0, 0.0, 1.0},
        {0.0, 10.0, 1.0, 1.0},
        {0.1, 0.5, 1.0, 0.9627503158599217},
        {0.1, 1.0, 1.0, 0.9627503158599217},
        {0.1, 1.1, 1.0, 0.9627503158599217},
        {0.1, 2.23606797749979, 0.0, 0.9627503158599217},
        {0.1, 2.23606797749979, 1.0, 0.9627503158599217},
        {0.1, 2.3259406699226015, 1.0, 0.9332702046772292},
        {0.1, 3.1622776601683795, 1.0, 0.915},
        {0.1, 3.640054944640259, 1.0, 0.9295},
        {0.1, 4.123105625617661, 1.0, 0.944},
        {0.1, 5.0, 1.0, 0.9542761914680473},
        {0.1, 10.0, 0.0, 0.9627503158599217},
        {0.1, 10.0, 1.0, 0.9774871529381937},
        {0.2, 0.5, 1.0, 0.9438733078572681},
        {0.2, 1.0, 1.0, 0.9438733078572681},
        {0.2, 1.1, 1.0, 0.9438733078572681},
        {0.2, 2.23606797749979, 0.0, 0.9438733078572681},
        {0.2, 2.23606797749979, 1.0, 0.9438733078572681},
        {0.2, 2.3259406699226015, 1.0, 0.9187499034915096},
        {0.2, 3.1622776601683795, 1.0, 0.94},
        {0.2, 3.640054944640259, 1.0, 0.9524999999999999},
        {0.2, 4.123105625617661, 1.0, 0.965},
        {0.2, 5.0, 1.0, 0.9714226196675295},
        {0.2, 10.0, 0.0, 0.9438733078572681},
        {0.2, 10.0, 1.0, 0.985929470586371},
        {0.45, 0.5, 1.0, 0.9368788681746459},
        {0.45, 1.0, 1.0, 0.9368788681746459},
        {0.45, 1.1, 1.0, 0.9368788681746459},
        {0.45, 2.23606797749979, 0.0, 0.9368788681746459},
        {0.45, 2.23606797749979, 1.0, 0.9368788681746459},
        {0.45, 2.3259406699226015, 1.0, 0.9373087065771706},
        {0.45, 3.1622776601683795, 1.0, 0.97725},
        {0.45, 3.640054944640259, 1.0, 0.9828749999999999},
        {0.45, 4.123105625617661, 1.0, 0.9884999999999999},
        {0.45, 5.0, 1.0, 0.9906102893193311},
        {0.45, 10.0, 0.0, 0.9368788681746459},
        {0.45, 10.0, 1.0, 0.9953768260498076},
        {0.9, 0.5, 1.0, 0.9852108326511243},
        {0.9, 1.0, 1.0, 0.9852108326511243},
        {0.9, 1.1, 1.0, 0.9852108326511243},
        {0.9, 2.23606797749979, 0.0, 0.9852108326511243},
        {0.9, 2.23606797749979, 1.0, 0.9852108326511243},
        {0.9, 2.3259406699226015, 1.0, 0.9880054195579285},
        {0.9, 3.1622776601683795, 1.0, 0.999},
        {0.9, 3.640054944640259, 1.0, 0.9995},
        {0.9, 4.123105625617661, 1.0, 1.0},
        {0.9, 5.0, 1.0, 1.0},
        {0.9, 10.0, 0.0, 0.9852108326511243},
        {0.9, 10.0, 1.0, 1.0},
        {1.0, 0.5, 1.0, 1.0},
        {1.0, 1.0, 1.0, 1.0},
        {1.0, 1.1, 1.0, 1.0},
        {1.0, 2.23606797749979, 0.0, 1.0},
        {1.0, 2.23606797749979, 1.0, 1.0},
        {1.0, 2.3259406699226015, 1.0, 1.0},
        {1.0, 3.1622776601683795, 1.0, 1.0},
        {1.0, 3.640054944640259, 1.0, 1.0},
        {1.0, 4.123105625617661, 1.0, 1.0},
        {1.0, 5.0, 1.0, 1.0},
        {1.0, 10.0, 0.0, 1.0},
        {1.0, 10.0, 1.0, 1.0},
    }};
    for (const std::array<double, 4>& row : kIncidence)
    {
        SCOPED_TRACE(::testing::Message()
                     << "r/s " << row[0] << " Mach " << row[1] << " rectangular " << row[2]);
        expectPinned(Naca1307FinBodyInterference::calculateWingIncidenceFactor(row[0], row[1], 1.0,
                                                                               row[2] != 0.0),
                     row[3]);
    }
}

TEST(Naca1307FinBodyInterference, WingIncidenceWithoutAValidAspectRatioOrMachIsEquationNineteen)
{
    const double equationNineteen = Naca1307FinBodyInterference::calculateWingIncidenceFactor(0.2);
    EXPECT_EQ(Naca1307FinBodyInterference::calculateWingIncidenceFactor(0.2, 5.0, 0.0, true),
              equationNineteen);
    EXPECT_EQ(Naca1307FinBodyInterference::calculateWingIncidenceFactor(0.2, 5.0, kNaN, true),
              equationNineteen);
    EXPECT_EQ(Naca1307FinBodyInterference::calculateWingIncidenceFactor(0.2, kNaN, 1.0, true),
              equationNineteen);
}

TEST(Naca1307FinBodyInterference, CalculateRemembersItsLastResult)
{
    const Naca1307FinBodyInterference model = createTableOneModel(kInf);
    const Loads                       first = model.calculate(2.0, 2.4);
    const Loads                       again = model.calculate(2.0, 2.4);
    EXPECT_EQ(first.bodyFactor, again.bodyFactor);
    EXPECT_EQ(first.bodyCp, again.bodyCp);

    // Another slope reuses the pressure integrals: the load times the slope is unchanged.
    const Loads other = model.calculate(2.0, 4.8);
    EXPECT_NEAR(other.bodyFactor * 4.8, first.bodyFactor * 2.4, 1e-15);

    // A fresh model gives the same as one that has cached other Mach numbers.
    const Naca1307FinBodyInterference fresh = createTableOneModel(kInf);
    (void)model.calculate(1.7, 2.4);
    (void)model.calculate(3.0, 2.4);
    const Loads cached   = model.calculate(1.2, 2.4);
    const Loads uncached = fresh.calculate(1.2, 2.4);
    EXPECT_EQ(cached.bodyFactor, uncached.bodyFactor);
    EXPECT_EQ(cached.bodyCp, uncached.bodyCp);

    // A NaN Mach number is remembered as any other (Java compares doubleToLongBits).
    const Loads nan = model.calculate(kNaN, 2.4);
    EXPECT_EQ(nan.finFactor, first.finFactor);
    const Loads nanAgain = model.calculate(-kNaN, 2.4);
    EXPECT_EQ(std::isnan(nanAgain.bodyCp), std::isnan(nan.bodyCp));
}

}  // namespace
