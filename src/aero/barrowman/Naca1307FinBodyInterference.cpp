#include "QtRocket/aero/barrowman/Naca1307FinBodyInterference.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <span>

#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

namespace
{

constexpr double kSubsonicMach                 = 0.9;
constexpr double kSupersonicMach               = 1.5;
constexpr double kMaxChartRadiusSemispanRatio  = 0.6;
constexpr double kEdgeEpsilon                  = 1.0e-6;
constexpr double kPlanarCriterion              = 4.0;
constexpr double kPlanarTransitionWidth        = 0.25;
constexpr double kBetaMLimitEpsilon            = 1.0e-10;
constexpr double kChart3MinimumBetaAspectRatio = 2.0;
constexpr double kChart3TransitionWidth        = 0.25;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNaN      = std::numeric_limits<double>::quiet_NaN();

// ---- The digitised charts and the quadrature, copied literal for literal from
// NACA1307FinBodyInterference.java (generated from the Java source and checked against it by a
// script: every value, in order, and every dimension). Each table keeps its Java name.
//
// Chart 15 is an extrapolated low-aspect-ratio fairing between the exact slender-body ordinate and
// the planar solution at the equation-22 boundary; CHART_15_GUIDE_* follow the dotted portions of
// the published curves. Chart 15(c), the rectangular unswept planform, has its own ordinates per
// radius family (CHART_15_RECTANGULAR_*). CHART_15_RADIUS_PROGRESS_SCALE holds the
// radius-dependent abscissa corrections fitted to the four r/s families of charts 15 and 16,
// indexed by sweep family, taper family and r/s. CHART_3_* are the linear-theory wing-incidence
// factors of chart 3 for beta*A = 2, 3 and 4. GAUSS_* are the sixteen-point Gauss-Legendre
// quadrature. CHART_16_LIFTING_LINE_WEIGHT holds the normalised fairings of chart 16 (no leading-
// edge, midchord or trailing-edge sweep; taper 0, 1/2 or 1; over CHART_16_BETA_ASPECT_RATIO), and
// CHART_16_RADIUS_BETA_SCALE their radius-dependent beta*A scaling.

/// CHART_RADIUS_SEMISPAN_RATIO
constexpr std::array<double, 4> kChartRadiusSemispanRatio{0.0, 0.2, 0.4, 0.6};

/// CHART_15_GUIDE_NORMALIZED_CRITERION
constexpr std::array<double, 9> kChart15GuideNormalizedCriterion{0.0,   0.125, 0.25,  0.375, 0.5,
                                                                 0.625, 0.75,  0.875, 1.0};

/// CHART_15_GUIDE_PLANAR_WEIGHT
constexpr std::array<double, 9> kChart15GuidePlanarWeight{0.0,  0.23, 0.43,  0.61, 0.75,
                                                          0.86, 0.94, 0.985, 1.0};

/// CHART_15_RECTANGULAR_NORMALIZED_CRITERION
constexpr std::array<double, 5> kChart15RectangularNormalizedCriterion{0.0, 0.25, 0.5, 0.75, 1.0};

/// CHART_15_RECTANGULAR_PLANAR_WEIGHT
constexpr std::array<std::array<double, 5>, 4> kChart15RectangularPlanarWeight{
    {{0.0, 0.10, 0.70, 0.94, 1.0},
     {0.0, 0.359, 0.705, 0.872, 1.0},
     {0.0, 0.336, 0.690, 0.858, 1.0},
     {0.0, 0.275, 0.550, 0.775, 1.0}}};

/// CHART_15_RADIUS_PROGRESS_SCALE
constexpr std::array<std::array<std::array<double, 4>, 3>, 3> kChart15RadiusProgressScale{
    {{{{1.00, 0.98, 0.94, 0.90}, {1.00, 0.97, 0.92, 0.87}, {1.00, 0.97, 0.92, 0.87}}},
     {{{1.00, 0.98, 0.93, 0.88}, {1.00, 0.98, 0.93, 0.88}, {1.00, 0.97, 0.92, 0.87}}},
     {{{1.00, 1.00, 1.00, 1.00}, {1.00, 0.98, 0.94, 0.90}, {1.00, 0.97, 0.92, 0.87}}}}};

/// CHART_3_RADIUS_SEMISPAN_RATIO
constexpr std::array<double, 10> kChart3RadiusSemispanRatio{0.0,  0.05, 0.10, 0.15, 0.20,
                                                            0.30, 0.40, 0.60, 0.80, 1.0};

/// CHART_3_BETA_ASPECT_RATIO
constexpr std::array<double, 3> kChart3BetaAspectRatio{2.0, 3.0, 4.0};

/// CHART_3_INCIDENCE_FACTOR
constexpr std::array<std::array<double, 10>, 3> kChart3IncidenceFactor{
    {{1.000, 0.944, 0.875, 0.852, 0.865, 0.905, 0.925, 0.960, 0.985, 1.000},
     {1.000, 0.945, 0.915, 0.920, 0.940, 0.960, 0.973, 0.990, 0.998, 1.000},
     {1.000, 0.960, 0.944, 0.950, 0.965, 0.978, 0.986, 0.996, 1.000, 1.000}}};

/// GAUSS_NODES
constexpr std::array<double, 16> kGaussNodes{
    -0.9894009349916499, -0.9445750230732326, -0.8656312023878318, -0.7554044083550030,
    -0.6178762444026438, -0.4580167776572274, -0.2816035507792589, -0.0950125098376374,
    0.0950125098376374,  0.2816035507792589,  0.4580167776572274,  0.6178762444026438,
    0.7554044083550030,  0.8656312023878318,  0.9445750230732326,  0.9894009349916499};

/// GAUSS_WEIGHTS
constexpr std::array<double, 16> kGaussWeights{
    0.0271524594117541, 0.0622535239386479, 0.0951585116824928, 0.1246289712555339,
    0.1495959888165767, 0.1691565193950025, 0.1826034150449236, 0.1894506104550685,
    0.1894506104550685, 0.1826034150449236, 0.1691565193950025, 0.1495959888165767,
    0.1246289712555339, 0.0951585116824928, 0.0622535239386479, 0.0271524594117541};

/// CHART_16_BETA_ASPECT_RATIO
constexpr std::array<double, 17> kChart16BetaAspectRatio{
    0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5, 7.0, 7.5, 8.0};

/// CHART_16_LIFTING_LINE_WEIGHT
constexpr std::array<std::array<std::array<double, 17>, 3>, 3> kChart16LiftingLineWeight{
    {{{{0.0, 0.48, 0.68, 0.80, 0.88, 0.93, 0.96, 0.98, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
       {0.0, 0.44, 0.67, 0.80, 0.88, 0.93, 0.96, 0.98, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
       {0.0, 0.45, 0.66, 0.79, 0.86, 0.91, 0.94, 0.97, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
        1.0}}},
     {{{0.0, 0.35, 0.57, 0.71, 0.82, 0.90, 0.95, 0.99, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
       {0.0, 0.34, 0.54, 0.70, 0.82, 0.90, 0.94, 0.97, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
       {0.0, 0.45, 0.66, 0.79, 0.86, 0.91, 0.94, 0.97, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
        1.0}}},
     {{{0.0, 0.15, 0.29, 0.41, 0.53, 0.64, 0.73, 0.80, 0.86, 0.90, 0.94, 0.96, 0.98, 0.99, 1.0, 1.0,
        1.0},
       {0.0, 0.30, 0.50, 0.65, 0.76, 0.84, 0.90, 0.95, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
       {0.0, 0.45, 0.66, 0.79, 0.86, 0.91, 0.94, 0.97, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
        1.0}}}}};

/// CHART_16_RADIUS_BETA_SCALE
constexpr std::array<std::array<std::array<double, 4>, 3>, 3> kChart16RadiusBetaScale{
    {{{{1.00, 1.05, 1.10, 1.15}, {1.00, 1.02, 1.04, 1.06}, {1.00, 1.00, 1.00, 1.00}}},
     {{{1.00, 1.15, 1.10, 1.05}, {1.00, 1.08, 1.05, 1.02}, {1.00, 1.00, 1.00, 1.00}}},
     {{{1.00, 1.75, 1.75, 1.75}, {1.00, 1.12, 1.08, 1.05}, {1.00, 1.00, 1.00, 1.00}}}}};

/// Java's Double.doubleToLongBits(a) == Double.doubleToLongBits(b): the same bits, every NaN
/// counting as the canonical one.
[[nodiscard]] bool sameBits(double a, double b) noexcept
{
    if (std::isnan(a) || std::isnan(b))
    {
        return std::isnan(a) && std::isnan(b);
    }
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

/// @p table[@p i], read through a std::span (a variable index into a std::array).
template <class T, std::size_t N>
[[nodiscard]] constexpr const T& element(const std::array<T, N>& table, std::size_t i) noexcept
{
    return std::span{table}[i];
}

/// start + fraction * (end - start) (Java's interpolate(); MathUtil::interpolate() is the same
/// expression with the commutative product swapped, so the same double).
[[nodiscard]] double interpolate(double start, double end, double fraction) noexcept
{
    return MathUtil::interpolate(start, end, fraction);
}

[[nodiscard]] double smoothStep(double value) noexcept
{
    return value * value * (3.0 - (2.0 * value));
}

/// Piecewise-linear interpolation of the curve (abscissae[i], ordinates[i]), held at the end
/// ordinates outside the abscissae (and at the last one for a NaN).
[[nodiscard]] double interpolateCurve(std::span<const double> abscissae,
                                      std::span<const double> ordinates, double abscissa) noexcept
{
    if (abscissa <= abscissae[0])
    {
        return ordinates[0];
    }
    for (std::size_t i = 1; i < abscissae.size(); i++)
    {
        if (abscissa <= abscissae[i])
        {
            const double fraction =
                (abscissa - abscissae[i - 1]) / (abscissae[i] - abscissae[i - 1]);
            return interpolate(ordinates[i - 1], ordinates[i], fraction);
        }
    }
    return ordinates[ordinates.size() - 1];
}

/// Interpolates between the four published r/s curves (0, 0.2, 0.4, 0.6), with the ratio clamped
/// to 0 ... 0.6.
[[nodiscard]] double interpolateRadiusFamily(double radiusZero, double radiusPointTwo,
                                             double radiusPointFour, double radiusPointSix,
                                             double radiusSemispanRatio) noexcept
{
    const std::span<const double> ratios{kChartRadiusSemispanRatio};
    const double radius = MathUtil::clamp(radiusSemispanRatio, 0.0, kMaxChartRadiusSemispanRatio);
    if (radius <= ratios[1])
    {
        return interpolate(radiusZero, radiusPointTwo, radius / ratios[1]);
    }
    if (radius <= ratios[2])
    {
        return interpolate(radiusPointTwo, radiusPointFour,
                           (radius - ratios[1]) / (ratios[2] - ratios[1]));
    }
    return interpolate(radiusPointFour, radiusPointSix,
                       (radius - ratios[2]) / (ratios[3] - ratios[2]));
}

/// The chart-15 curve of one sweep, taper and radius family at @p normalizedCriterion.
[[nodiscard]] double interpolateChart15RadiusCurve(std::size_t sweepFamily, std::size_t taperFamily,
                                                   std::size_t radiusFamily,
                                                   double      normalizedCriterion) noexcept
{
    if (sweepFamily == 0 && taperFamily == 2)
    {
        return interpolateCurve(kChart15RectangularNormalizedCriterion,
                                element(kChart15RectangularPlanarWeight, radiusFamily),
                                normalizedCriterion);
    }
    const double scale = element(
        element(element(kChart15RadiusProgressScale, sweepFamily), taperFamily), radiusFamily);
    const double scaledCriterion = MathUtil::clamp(normalizedCriterion * scale, 0.0, 1.0);
    return interpolateCurve(kChart15GuideNormalizedCriterion, kChart15GuidePlanarWeight,
                            scaledCriterion);
}

[[nodiscard]] double interpolateChart15RadiusFamily(std::size_t sweepFamily,
                                                    std::size_t taperFamily,
                                                    double      normalizedCriterion,
                                                    double      radiusSemispanRatio) noexcept
{
    const double radiusZero =
        interpolateChart15RadiusCurve(sweepFamily, taperFamily, 0, normalizedCriterion);
    const double radiusPointTwo =
        interpolateChart15RadiusCurve(sweepFamily, taperFamily, 1, normalizedCriterion);
    const double radiusPointFour =
        interpolateChart15RadiusCurve(sweepFamily, taperFamily, 2, normalizedCriterion);
    const double radiusPointSix =
        interpolateChart15RadiusCurve(sweepFamily, taperFamily, 3, normalizedCriterion);
    return interpolateRadiusFamily(radiusZero, radiusPointTwo, radiusPointFour, radiusPointSix,
                                   radiusSemispanRatio);
}

[[nodiscard]] double interpolateChart15TaperFamily(std::size_t sweepFamily, double taperRatio,
                                                   double normalizedCriterion,
                                                   double radiusSemispanRatio) noexcept
{
    const double zeroTaper =
        interpolateChart15RadiusFamily(sweepFamily, 0, normalizedCriterion, radiusSemispanRatio);
    const double halfTaper =
        interpolateChart15RadiusFamily(sweepFamily, 1, normalizedCriterion, radiusSemispanRatio);
    const double fullTaper =
        interpolateChart15RadiusFamily(sweepFamily, 2, normalizedCriterion, radiusSemispanRatio);
    if (taperRatio <= 0.5)
    {
        return interpolate(zeroTaper, halfTaper, 2.0 * taperRatio);
    }
    return interpolate(halfTaper, fullTaper, (2.0 * taperRatio) - 1.0);
}

/// The chart-16 curve of one sweep, taper and radius family at @p betaAspectRatio, scaled in
/// beta*A for the radius family.
[[nodiscard]] double interpolateChart16RadiusCurve(std::span<const double> curve,
                                                   std::size_t sweepFamily, std::size_t taperFamily,
                                                   std::size_t radiusFamily,
                                                   double      betaAspectRatio) noexcept
{
    const double scale =
        element(element(element(kChart16RadiusBetaScale, sweepFamily), taperFamily), radiusFamily);
    const double scaledBetaAspectRatio = betaAspectRatio * scale;
    return interpolateCurve(kChart16BetaAspectRatio, curve, scaledBetaAspectRatio);
}

[[nodiscard]] double interpolateChart16RadiusFamily(std::size_t sweepFamily,
                                                    std::size_t taperFamily, double betaAspectRatio,
                                                    double radiusSemispanRatio) noexcept
{
    const std::span<const double> curve =
        element(element(kChart16LiftingLineWeight, sweepFamily), taperFamily);
    const double radiusZero =
        interpolateChart16RadiusCurve(curve, sweepFamily, taperFamily, 0, betaAspectRatio);
    const double radiusPointTwo =
        interpolateChart16RadiusCurve(curve, sweepFamily, taperFamily, 1, betaAspectRatio);
    const double radiusPointFour =
        interpolateChart16RadiusCurve(curve, sweepFamily, taperFamily, 2, betaAspectRatio);
    const double radiusPointSix =
        interpolateChart16RadiusCurve(curve, sweepFamily, taperFamily, 3, betaAspectRatio);
    return interpolateRadiusFamily(radiusZero, radiusPointTwo, radiusPointFour, radiusPointSix,
                                   radiusSemispanRatio);
}

[[nodiscard]] double interpolateChart16TaperFamily(std::size_t sweepFamily, double taperRatio,
                                                   double betaAspectRatio,
                                                   double radiusSemispanRatio) noexcept
{
    const double zeroTaper =
        interpolateChart16RadiusFamily(sweepFamily, 0, betaAspectRatio, radiusSemispanRatio);
    const double halfTaper =
        interpolateChart16RadiusFamily(sweepFamily, 1, betaAspectRatio, radiusSemispanRatio);
    const double fullTaper =
        interpolateChart16RadiusFamily(sweepFamily, 2, betaAspectRatio, radiusSemispanRatio);
    if (taperRatio <= 0.5)
    {
        return interpolate(zeroTaper, halfTaper, 2.0 * taperRatio);
    }
    return interpolate(halfTaper, fullTaper, (2.0 * taperRatio) - 1.0);
}

/// Interpolates the chart-3 linear-theory families of a rectangular wing.
[[nodiscard]] double interpolateChartThree(double betaAspectRatio,
                                           double radiusSemispanRatio) noexcept
{
    const std::span<const double> betaAspectRatios{kChart3BetaAspectRatio};
    const double                  radius = MathUtil::clamp(radiusSemispanRatio, 0.0, 1.0);
    const double                  betaTwo =
        interpolateCurve(kChart3RadiusSemispanRatio, kChart3IncidenceFactor[0], radius);
    const double betaThree =
        interpolateCurve(kChart3RadiusSemispanRatio, kChart3IncidenceFactor[1], radius);
    const double betaFour =
        interpolateCurve(kChart3RadiusSemispanRatio, kChart3IncidenceFactor[2], radius);
    if (betaAspectRatio <= betaAspectRatios[1])
    {
        return interpolate(
            betaTwo, betaThree,
            (betaAspectRatio - betaAspectRatios[0]) / (betaAspectRatios[1] - betaAspectRatios[0]));
    }
    if (betaAspectRatio <= betaAspectRatios[2])
    {
        return interpolate(
            betaThree, betaFour,
            (betaAspectRatio - betaAspectRatios[1]) / (betaAspectRatios[2] - betaAspectRatios[1]));
    }

    const double reciprocalFraction = 1.0 - (betaAspectRatios[2] / betaAspectRatio);
    return interpolate(betaFour, 1.0, MathUtil::clamp(reciprocalFraction, 0.0, 1.0));
}

/// The planar pressure kernel of equations 23/25 (and their limits) at (eta, xi).
[[nodiscard]] double calculatePlanarPressureKernel(double beta, double m, double betaM, double eta,
                                                   double xi) noexcept
{
    if (std::isinf(betaM))
    {
        return std::acos(MathUtil::clamp(beta * eta / xi, -1.0, 1.0));
    }
    if (betaM > 1.0)
    {
        const double argument = ((xi / beta) + (betaM * eta)) / (eta + (m * xi));
        return std::acos(MathUtil::clamp(argument, -1.0, 1.0));
    }

    const double argument = ((xi / beta) - eta) / (eta + (m * xi));
    return std::sqrt(MathUtil::javaMax(0.0, argument));
}

}  // namespace

Naca1307FinBodyInterference::Naca1307FinBodyInterference(double bodyRadius, double exposedSpan,
                                                         double rootChord, double tipChord,
                                                         double leadingEdgeSweep,
                                                         double aspectRatio,
                                                         double bodyEnd) noexcept
  : m_bodyRadius(bodyRadius),
    m_exposedSpan(exposedSpan),
    m_rootChord(rootChord),
    m_tipChord(tipChord),
    m_leadingEdgeSweep(leadingEdgeSweep),
    m_aspectRatio(aspectRatio),
    m_bodyEnd(bodyEnd)
{
}

bool Naca1307FinBodyInterference::isApplicable() const noexcept
{
    if (!(m_bodyRadius > MathUtil::kEpsilon) || !(m_exposedSpan > MathUtil::kEpsilon) ||
        !(m_rootChord > MathUtil::kEpsilon) || m_tipChord < 0 || m_tipChord > m_rootChord ||
        !(m_aspectRatio > MathUtil::kEpsilon) || std::isnan(m_bodyEnd) ||
        m_bodyEnd + kEdgeEpsilon < m_rootChord)
    {
        return false;
    }

    const double radiusSemispanRatio = getRadiusSemispanRatio();
    const double trailingEdgeSweep   = m_leadingEdgeSweep + m_tipChord - m_rootChord;
    const double taperRatio          = m_tipChord / m_rootChord;
    const double leadingEdgeFraction = getLeadingEdgeSweepFraction();
    const bool   requiresMissingChartFifteenPanel =
        taperRatio < 0.5 - kEdgeEpsilon && leadingEdgeFraction < 0.5 - kEdgeEpsilon;
    return radiusSemispanRatio <= kMaxChartRadiusSemispanRatio + kEdgeEpsilon &&
           m_leadingEdgeSweep >= -kEdgeEpsilon && trailingEdgeSweep <= kEdgeEpsilon &&
           !requiresMissingChartFifteenPanel;
}

Naca1307FinBodyInterference::Loads Naca1307FinBodyInterference::calculate(
    double mach, double wingLiftCurveSlope) const
{
    if (m_cachedResult && sameBits(m_cachedResult->mach, mach) &&
        sameBits(m_cachedResult->wingLiftCurveSlope, wingLiftCurveSlope))
    {
        return m_cachedResult->result;
    }

    const double radiusSemispanRatio = getRadiusSemispanRatio();
    const double finFactor           = calculateFinInBodyFactor(radiusSemispanRatio);
    const double incidenceFactor     = calculateWingIncidenceFactor(
        radiusSemispanRatio, mach, m_aspectRatio, isRectangularPlanform());
    BodyResult bodyResult{};

    if (mach <= kSubsonicMach)
    {
        bodyResult = calculateSubsonicBodyResult(mach, finFactor);
    }
    else if (mach >= kSupersonicMach)
    {
        bodyResult = calculateSupersonicBodyResult(mach, wingLiftCurveSlope, finFactor);
    }
    else
    {
        const BodyResult subsonic = calculateSubsonicBodyResult(kSubsonicMach, finFactor);
        const BodyResult supersonic =
            calculateSupersonicBodyResult(kSupersonicMach, wingLiftCurveSlope, finFactor);
        const double fraction = (mach - kSubsonicMach) / (kSupersonicMach - kSubsonicMach);
        bodyResult            = interpolateBodyResult(subsonic, supersonic, fraction);
    }

    const Loads result{.finFactor       = finFactor,
                       .bodyFactor      = bodyResult.factor,
                       .bodyCp          = bodyResult.cp,
                       .incidenceFactor = incidenceFactor};
    // Each radial instance normally requests the same result consecutively.
    m_cachedResult =
        CachedResult{.mach = mach, .wingLiftCurveSlope = wingLiftCurveSlope, .result = result};
    return result;
}

Naca1307FinBodyInterference::BodyResult Naca1307FinBodyInterference::interpolateBodyResult(
    const BodyResult& start, const BodyResult& end, double fraction) noexcept
{
    const double factor       = interpolate(start.factor, end.factor, fraction);
    const double momentFactor = interpolate(start.factor * start.cp, end.factor * end.cp, fraction);
    const double cp           = factor > MathUtil::kEpsilon ? momentFactor / factor : 0.0;
    return BodyResult{.factor = factor, .cp = cp};
}

double Naca1307FinBodyInterference::calculateFinInBodyFactor(double tau) noexcept
{
    // The printed-page-47 selection guide points one rectangular supersonic case to linear theory
    // (chart 2), but the report's discussion on printed page 5 recommends this slender-body value
    // for every combination because linear theory omits the observed loss of wing lift near the
    // body.
    if (!(tau > kEdgeEpsilon))
    {
        return 1.0;
    }
    if (tau >= 1.0)
    {
        return 2.0;
    }

    const double inverseTau = 1.0 / tau;
    const double angle  = (0.5 * std::atan(0.5 * (inverseTau - tau))) + (std::numbers::pi / 4.0);
    const double braces = ((1.0 + MathUtil::javaPow(tau, 4)) * angle) -
                          (MathUtil::pow2(tau) * (inverseTau - tau + (2.0 * std::atan(tau))));
    return (2.0 / std::numbers::pi) * braces / MathUtil::pow2(1.0 - tau);
}

double Naca1307FinBodyInterference::calculateSlenderBodyInFinFactor(double tau,
                                                                    double finFactor) noexcept
{
    return MathUtil::javaMax(0.0, MathUtil::pow2(1.0 + tau) - finFactor);
}

double Naca1307FinBodyInterference::calculateWingIncidenceFactor(
    double radiusSemispanRatio) noexcept
{
    if (!(radiusSemispanRatio > kEdgeEpsilon))
    {
        return 1.0;
    }
    if (radiusSemispanRatio >= 1.0)
    {
        return 1.0;
    }

    const double tau         = 1.0 / radiusSemispanRatio;
    const double tauSquared  = MathUtil::pow2(tau);
    const double tauMinusOne = tau - 1.0;
    const double angle =
        std::asin(MathUtil::clamp((tauSquared - 1.0) / (tauSquared + 1.0), -1.0, 1.0));
    const double common =
        MathUtil::pow2(tauSquared + 1.0) / (tauSquared * MathUtil::pow2(tauMinusOne));
    const double braces =
        (MathUtil::pow2(std::numbers::pi) * MathUtil::pow2(tau + 1.0) / (4.0 * tauSquared)) +
        (std::numbers::pi * common * angle) -
        (2.0 * std::numbers::pi * (tau + 1.0) / (tau * tauMinusOne)) +
        (common * MathUtil::pow2(angle)) - (4.0 * (tau + 1.0) * angle / (tau * tauMinusOne)) +
        (8.0 / MathUtil::pow2(tauMinusOne) * std::log((tauSquared + 1.0) / (2.0 * tau)));
    return braces / MathUtil::pow2(std::numbers::pi);
}

double Naca1307FinBodyInterference::calculateWingIncidenceFactor(double radiusSemispanRatio,
                                                                 double mach, double aspectRatio,
                                                                 bool rectangularPlanform) noexcept
{
    const double slenderBodyFactor = calculateWingIncidenceFactor(radiusSemispanRatio);
    if (!rectangularPlanform || !(mach > 1.0) || !(aspectRatio > MathUtil::kEpsilon))
    {
        return slenderBodyFactor;
    }

    const double beta            = std::sqrt(MathUtil::pow2(mach) - 1.0);
    const double betaAspectRatio = beta * aspectRatio;
    if (!(betaAspectRatio > kChart3MinimumBetaAspectRatio))
    {
        return slenderBodyFactor;
    }

    const double chartFactor = interpolateChartThree(betaAspectRatio, radiusSemispanRatio);
    if (betaAspectRatio < kChart3MinimumBetaAspectRatio + kChart3TransitionWidth)
    {
        const double fraction =
            (betaAspectRatio - kChart3MinimumBetaAspectRatio) / kChart3TransitionWidth;
        return interpolate(slenderBodyFactor, chartFactor,
                           smoothStep(MathUtil::clamp(fraction, 0.0, 1.0)));
    }
    return chartFactor;
}

Naca1307FinBodyInterference::BodyResult Naca1307FinBodyInterference::calculateSubsonicBodyResult(
    double mach, double finFactor) const noexcept
{
    const double factor      = calculateSlenderBodyInFinFactor(getRadiusSemispanRatio(), finFactor);
    const double beta        = std::sqrt(MathUtil::javaMax(0.0, 1.0 - MathUtil::pow2(mach)));
    const double chartWeight = interpolateChart16(beta * m_aspectRatio, getRadiusSemispanRatio());
    const double slenderCp   = calculateSlenderBodyCp();
    const double liftingLineCp = calculateLiftingLineBodyCp();
    const double cp            = interpolate(slenderCp, liftingLineCp, chartWeight);
    return BodyResult{.factor = factor, .cp = cp};
}

Naca1307FinBodyInterference::BodyResult Naca1307FinBodyInterference::calculateSupersonicBodyResult(
    double mach, double wingLiftCurveSlope, double finFactor) const
{
    const double slenderFactor =
        calculateSlenderBodyInFinFactor(getRadiusSemispanRatio(), finFactor);
    const BodyResult slenderResult{.factor = slenderFactor, .cp = calculateSlenderBodyCp()};
    if (!(mach > 1.0) || !(wingLiftCurveSlope > MathUtil::kEpsilon))
    {
        return slenderResult;
    }

    const double     beta      = std::sqrt(MathUtil::pow2(mach) - 1.0);
    const double     criterion = calculatePlanarCriterion(beta);
    const BodyResult lowAspectResult =
        calculateLowAspectSupersonicBodyResult(beta, wingLiftCurveSlope, slenderFactor);
    if (criterion <= kPlanarCriterion)
    {
        return lowAspectResult;
    }

    // Java's null planar result cannot occur here (integratePlanarBodyPressure() always returns
    // one).
    const BodyResult planarResult = integratePlanarBodyPressure(beta, wingLiftCurveSlope);
    if (!std::isfinite(planarResult.factor) || !std::isfinite(planarResult.cp) ||
        planarResult.factor < 0)
    {
        return lowAspectResult;
    }

    // The report changes approximations at criterion=4. Their values are close but not
    // algebraically identical, so fair force and moment over a narrow interval to prevent a
    // numerical step in CNa or CP.
    if (criterion < kPlanarCriterion + kPlanarTransitionWidth)
    {
        const double fraction = (criterion - kPlanarCriterion) / kPlanarTransitionWidth;
        return interpolateBodyResult(lowAspectResult, planarResult,
                                     smoothStep(MathUtil::clamp(fraction, 0.0, 1.0)));
    }
    return planarResult;
}

double Naca1307FinBodyInterference::calculatePlanarCriterion(double beta) const noexcept
{
    const double taperRatio = m_tipChord / m_rootChord;
    return (1.0 + taperRatio) * m_aspectRatio * (beta + (m_leadingEdgeSweep / m_exposedSpan));
}

Naca1307FinBodyInterference::BodyResult
Naca1307FinBodyInterference::calculateLowAspectSupersonicBodyResult(double beta,
                                                                    double wingLiftCurveSlope,
                                                                    double slenderFactor) const
{
    const double boundaryBeta = calculatePlanarBoundaryBeta();
    if (!(boundaryBeta > MathUtil::kEpsilon))
    {
        // Java falls back to the slender result when the planar one is null, which cannot occur.
        return integratePlanarBodyPressure(beta, wingLiftCurveSlope);
    }

    const BodyResult boundary = integratePlanarBodyPressure(boundaryBeta, wingLiftCurveSlope);
    if (!std::isfinite(boundary.cp))
    {
        return BodyResult{.factor = slenderFactor, .cp = calculateSlenderBodyCp()};
    }

    const double normalizedCriterion = MathUtil::clamp(beta / boundaryBeta, 0.0, 1.0);
    const double chartWeight = interpolateChart15(normalizedCriterion, getRadiusSemispanRatio());
    const double cp          = interpolate(calculateSlenderBodyCp(), boundary.cp, chartWeight);
    return BodyResult{.factor = slenderFactor, .cp = cp};
}

double Naca1307FinBodyInterference::calculatePlanarBoundaryBeta() const noexcept
{
    const double taperRatio        = m_tipChord / m_rootChord;
    const double sweepContribution = m_aspectRatio * m_leadingEdgeSweep / m_exposedSpan;
    const double betaAspectRatio   = (kPlanarCriterion / (1.0 + taperRatio)) - sweepContribution;
    return betaAspectRatio / m_aspectRatio;
}

Naca1307FinBodyInterference::BodyResult Naca1307FinBodyInterference::integratePlanarBodyPressure(
    double beta, double wingLiftCurveSlope) const
{
    const PressureIntegral integral = getPlanarPressureIntegral(beta);
    if (!(integral.lift > MathUtil::kEpsilon))
    {
        return BodyResult{.factor = 0.0, .cp = 0.0};
    }

    const double exposedWingArea = m_exposedSpan * (m_rootChord + m_tipChord);
    const double betaM           = integral.betaM;
    double       factor          = 0;
    if (betaM > 1.0 || std::isinf(betaM))
    {
        const double edgeFactor =
            std::isinf(betaM) ? 1.0 : betaM / std::sqrt(MathUtil::pow2(betaM) - 1.0);
        factor = 8.0 * edgeFactor * integral.lift /
                 (std::numbers::pi * beta * exposedWingArea * wingLiftCurveSlope);
    }
    else
    {
        factor = 16.0 * MathUtil::javaPow(betaM, 1.5) * integral.lift /
                 (std::numbers::pi * beta * (betaM + 1.0) * exposedWingArea * wingLiftCurveSlope);
    }
    return BodyResult{.factor = factor, .cp = integral.moment / integral.lift};
}

Naca1307FinBodyInterference::PressureIntegral
Naca1307FinBodyInterference::getPlanarPressureIntegral(double beta) const
{
    // The final supersonic factor is inversely proportional to the caller's lift-curve slope, but
    // the expensive pressure field is not; keeping the two most recent betas covers the
    // equation-22 boundary and the current-Mach integrations.
    if (m_recentPressureIntegral && sameBits(m_recentPressureIntegral->beta, beta))
    {
        return *m_recentPressureIntegral;
    }
    if (m_previousPressureIntegral && sameBits(m_previousPressureIntegral->beta, beta))
    {
        return *m_previousPressureIntegral;
    }

    const PressureIntegral calculated = calculatePlanarPressureIntegral(beta);
    m_previousPressureIntegral        = m_recentPressureIntegral;
    m_recentPressureIntegral          = calculated;
    return calculated;
}

Naca1307FinBodyInterference::PressureIntegral
Naca1307FinBodyInterference::calculatePlanarPressureIntegral(double beta) const noexcept
{
    const double diameter = 2.0 * m_bodyRadius;
    const double etaEnd   = MathUtil::javaMin(diameter, m_bodyEnd / beta);
    if (!(etaEnd > MathUtil::kEpsilon))
    {
        return PressureIntegral{.beta = beta, .lift = 0.0, .moment = 0.0, .betaM = kNaN};
    }

    double m = m_leadingEdgeSweep <= kEdgeEpsilon ? kInfinity : m_exposedSpan / m_leadingEdgeSweep;
    double betaM = beta * m;
    if (std::isfinite(betaM) && std::abs(betaM - 1.0) < kBetaMLimitEpsilon)
    {
        // Equation 25 is the finite common limit of the two edge cases.
        betaM = 1.0;
        m     = 1.0 / beta;
    }

    const std::span<const double> nodes{kGaussNodes};
    const std::span<const double> weights{kGaussWeights};
    double                        liftIntegral   = 0.0;
    double                        momentIntegral = 0.0;
    const double                  etaScale       = etaEnd / 2.0;
    for (std::size_t i = 0; i < nodes.size(); i++)
    {
        const double eta   = etaScale * (nodes[i] + 1.0);
        const double lower = beta * eta;
        const double upper = MathUtil::javaMin(m_rootChord + lower, m_bodyEnd);
        if (!(upper > lower))
        {
            continue;
        }

        const double xiScale     = (upper - lower) / 2.0;
        const double xiCenter    = (upper + lower) / 2.0;
        double       stripLift   = 0.0;
        double       stripMoment = 0.0;
        for (std::size_t j = 0; j < nodes.size(); j++)
        {
            const double xi       = xiCenter + (xiScale * nodes[j]);
            const double pressure = calculatePlanarPressureKernel(beta, m, betaM, eta, xi);
            const double weight   = weights[j];
            stripLift += weight * pressure;
            stripMoment += weight * xi * pressure;
        }
        liftIntegral += weights[i] * xiScale * stripLift;
        momentIntegral += weights[i] * xiScale * stripMoment;
    }
    liftIntegral *= etaScale;
    momentIntegral *= etaScale;
    return PressureIntegral{
        .beta = beta, .lift = liftIntegral, .moment = momentIntegral, .betaM = betaM};
}

double Naca1307FinBodyInterference::calculateLiftingLineBodyCp() const noexcept
{
    // Appendix D, equation D5, evaluated for the trapezoid's quarter-chord line.
    const std::span<const double> nodes{kGaussNodes};
    const std::span<const double> weights{kGaussWeights};
    double                        weightedOffset = 0.0;
    double                        totalWeight    = 0.0;
    const double                  scale          = m_exposedSpan / 2.0;
    for (std::size_t i = 0; i < nodes.size(); i++)
    {
        const double offset = scale * (nodes[i] + 1.0);
        const double eta    = m_bodyRadius + offset;
        const double circulation =
            std::sqrt(MathUtil::javaMax(0.0, 1.0 - MathUtil::pow2(offset / m_exposedSpan)));
        const double weight = weights[i] * circulation / MathUtil::pow2(eta);
        totalWeight += weight;
        weightedOffset += weight * offset;
    }
    if (!(totalWeight > MathUtil::kEpsilon))
    {
        return m_rootChord / 4.0;
    }

    const double quarterChordSweep = m_leadingEdgeSweep + (m_tipChord / 4.0) - (m_rootChord / 4.0);
    return (m_rootChord / 4.0) +
           ((weightedOffset / totalWeight) * quarterChordSweep / m_exposedSpan);
}

double Naca1307FinBodyInterference::calculateSlenderBodyCp() const noexcept
{
    // The slender-body endpoint of charts 15 and 16 at beta*A = 0.
    return m_leadingEdgeSweep / 2.0;
}

double Naca1307FinBodyInterference::getRadiusSemispanRatio() const noexcept
{
    return m_bodyRadius / (m_bodyRadius + m_exposedSpan);
}

bool Naca1307FinBodyInterference::isRectangularPlanform() const noexcept
{
    return MathUtil::equals(m_rootChord, m_tipChord);
}

double Naca1307FinBodyInterference::interpolateChart15(double normalizedCriterion,
                                                       double radiusSemispanRatio) const noexcept
{
    const double taperRatio          = m_tipChord / m_rootChord;
    const double leadingEdgeFraction = getLeadingEdgeSweepFraction();

    const double noLeadingSweep =
        interpolateChart15TaperFamily(0, taperRatio, normalizedCriterion, radiusSemispanRatio);
    const double noMidchordSweep =
        interpolateChart15TaperFamily(1, taperRatio, normalizedCriterion, radiusSemispanRatio);
    const double noTrailingSweep =
        interpolateChart15TaperFamily(2, taperRatio, normalizedCriterion, radiusSemispanRatio);
    if (leadingEdgeFraction <= 0.5)
    {
        return interpolate(noLeadingSweep, noMidchordSweep, 2.0 * leadingEdgeFraction);
    }
    return interpolate(noMidchordSweep, noTrailingSweep, (2.0 * leadingEdgeFraction) - 1.0);
}

double Naca1307FinBodyInterference::interpolateChart16(double betaAspectRatio,
                                                       double radiusSemispanRatio) const noexcept
{
    // The nine chart-16 planform families, with their four published radius/semispan curves.
    const double taperRatio          = m_tipChord / m_rootChord;
    const double leadingEdgeFraction = getLeadingEdgeSweepFraction();

    const double noLeadingSweep =
        interpolateChart16TaperFamily(0, taperRatio, betaAspectRatio, radiusSemispanRatio);
    const double noMidchordSweep =
        interpolateChart16TaperFamily(1, taperRatio, betaAspectRatio, radiusSemispanRatio);
    const double noTrailingSweep =
        interpolateChart16TaperFamily(2, taperRatio, betaAspectRatio, radiusSemispanRatio);
    if (leadingEdgeFraction <= 0.5)
    {
        return interpolate(noLeadingSweep, noMidchordSweep, 2.0 * leadingEdgeFraction);
    }
    return interpolate(noMidchordSweep, noTrailingSweep, (2.0 * leadingEdgeFraction) - 1.0);
}

double Naca1307FinBodyInterference::getLeadingEdgeSweepFraction() const noexcept
{
    const double maximumSweep = m_rootChord - m_tipChord;
    if (maximumSweep <= kEdgeEpsilon)
    {
        return 0.0;
    }
    return MathUtil::clamp(m_leadingEdgeSweep / maximumSweep, 0.0, 1.0);
}

}  // namespace QtRocket
