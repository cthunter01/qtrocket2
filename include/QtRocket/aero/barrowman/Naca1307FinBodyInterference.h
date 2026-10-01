#pragma once

#include <optional>

namespace QtRocket
{

/// The fin-body interference of zero-incidence trapezoidal fins after NACA Report 1307
/// (OpenRocket's aerodynamics/barrowman/NACA1307FinBodyInterference): the fin lift in the
/// presence of the body (equation 14), the lift the fin carries over onto the body and its centre
/// of pressure (slender-body theory subsonic, charts 15 and 16, the planar pressure integrals of
/// equations 23/25 and 66/69 supersonic, blended through the transonic range), and the wing
/// incidence factor for cant-driven roll (equation 19, chart 3 for rectangular fins).
///
/// The report treats the two exposed fin panels as one wing. FinSetCalc calls this once per
/// radial fin orientation, so the factors apply to one half-wing at a time and the radial
/// projection supplies the other half; the factors themselves are unchanged by that.
///
/// The digitised chart ordinates are transcribed verbatim from OpenRocket (their source is
/// recorded in OpenRocket's doc/techdoc/naca1307-chart-digitization.md).
///
/// Caching: calculate() remembers its last result and the pressure integrals of the last two
/// Mach parameters (bit-identical inputs give the cached value), as Java does. The caches are
/// mutable members, so one object must not be used from two threads at once (Java publishes them
/// through volatile fields); a calculator, and so each model, belongs to one simulation.
class Naca1307FinBodyInterference
{
public:
    /// The separate NACA loads (Java: Result): the fin lift divided by the isolated fin lift
    /// (finFactor), the body carryover divided by the isolated fin lift (bodyFactor; in the
    /// planar supersonic regime it contains the inverse of the caller's lift-curve slope and is
    /// meaningful only multiplied back by the isolated CNa), the centre of pressure of the body
    /// carryover measured from the fin-root leading edge (bodyCp), and the selected equation-19
    /// or chart-3 wing incidence factor (incidenceFactor).
    struct Loads
    {
        double finFactor;
        double bodyFactor;
        double bodyCp;
        double incidenceFactor;
    };

    /// A model for one trapezoidal fin pair: @p bodyRadius at the fin root, @p exposedSpan from
    /// the body surface to the tip, the exposed @p rootChord and @p tipChord, the axial offset
    /// @p leadingEdgeSweep of the tip leading edge from the root leading edge, the effective
    /// @p aspectRatio of the two joined exposed panels, and the axial end @p bodyEnd of the
    /// cylindrical body measured from the fin-root leading edge (+inf for an endless afterbody).
    Naca1307FinBodyInterference(double bodyRadius, double exposedSpan, double rootChord,
                                double tipChord, double leadingEdgeSweep, double aspectRatio,
                                double bodyEnd) noexcept;

    /// Whether the report's trapezoidal, constant-radius model covers this geometry: positive
    /// radius, span, root chord and aspect ratio (above MathUtil::kEpsilon), a tip chord from 0 to
    /// the root chord, a body that reaches the root trailing edge, r/s at most 0.6 (charts 15 and
    /// 16 end there), no forward-swept leading edge or aft-swept trailing edge, and not a geometry
    /// that needs chart 15(a)'s omitted low-aspect-ratio panel (taper and leading-edge sweep
    /// fraction both below one half). Each limit has a 1e-6 tolerance.
    [[nodiscard]] bool isApplicable() const noexcept;

    /// The loads at @p mach for the lift-curve slope @p wingLiftCurveSlope (per radian, based on
    /// the area) of the joined exposed panels: slender-body theory and the chart-16 CP up to Mach
    /// 0.9, the supersonic model from Mach 1.5, and in between the two endpoint results blended
    /// linearly in force and moment.
    [[nodiscard]] Loads calculate(double mach, double wingLiftCurveSlope) const;

    /// Equation 14 with tau = r/s: the fin lift in the presence of the body divided by the
    /// isolated fin lift; 1 for tau not above 1e-6 (and NaN), 2 from tau = 1.
    [[nodiscard]] static double calculateFinInBodyFactor(double tau) noexcept;

    /// Equation 21 through its exact identity with equation 14: the slender-body load the body
    /// carries due to the fin, max(0, (1 + tau)^2 - finFactor) (Java's Math.max).
    [[nodiscard]] static double calculateSlenderBodyInFinFactor(double tau,
                                                                double finFactor) noexcept;

    /// Equation 19 (wing incidence on a zero-angle body), with the radius/semispan ratio rather
    /// than the report's semispan/radius: 1 for a ratio not above 1e-6 (and NaN) or from 1.
    [[nodiscard]] static double calculateWingIncidenceFactor(double radiusSemispanRatio) noexcept;

    /// The report's wing-incidence approximation for cant-driven roll: chart 3 for a rectangular
    /// planform above Mach 1 with beta * A above 2 (faired in with a smooth step over the first
    /// 0.25 of beta * A, interpolated in 1 / (beta * A) towards 1 beyond 4), equation 19
    /// otherwise.
    [[nodiscard]] static double calculateWingIncidenceFactor(double radiusSemispanRatio,
                                                             double mach, double aspectRatio,
                                                             bool rectangularPlanform) noexcept;

    /// The chart-15 planar weight at @p normalizedCriterion (beta over the equation-22 boundary
    /// beta) for @p radiusSemispanRatio, interpolated over the sweep and taper families of this
    /// planform (Java: package-private, for the tests).
    [[nodiscard]] double interpolateChart15(double normalizedCriterion,
                                            double radiusSemispanRatio) const noexcept;

private:
    /// A body-only load and its CP, used while selecting and blending regimes (BodyResult).
    struct BodyResult
    {
        double factor;
        double cp;
    };

    /// The slope-independent pressure integrals of the planar supersonic model for one beta
    /// (PressureIntegral).
    struct PressureIntegral
    {
        double beta;
        double lift;
        double moment;
        double betaM;
    };

    /// The last result (CachedResult).
    struct CachedResult
    {
        double mach;
        double wingLiftCurveSlope;
        Loads  result;
    };

    /// The two body results blended linearly in force and in moment (interpolateBodyResult()).
    [[nodiscard]] static BodyResult interpolateBodyResult(const BodyResult& start,
                                                          const BodyResult& end,
                                                          double            fraction) noexcept;

    [[nodiscard]] BodyResult calculateSubsonicBodyResult(double mach,
                                                         double finFactor) const noexcept;
    [[nodiscard]] BodyResult calculateSupersonicBodyResult(double mach, double wingLiftCurveSlope,
                                                           double finFactor) const;
    [[nodiscard]] double     calculatePlanarCriterion(double beta) const noexcept;
    [[nodiscard]] BodyResult calculateLowAspectSupersonicBodyResult(double beta,
                                                                    double wingLiftCurveSlope,
                                                                    double slenderFactor) const;
    [[nodiscard]] double     calculatePlanarBoundaryBeta() const noexcept;
    [[nodiscard]] BodyResult integratePlanarBodyPressure(double beta,
                                                         double wingLiftCurveSlope) const;
    [[nodiscard]] PressureIntegral getPlanarPressureIntegral(double beta) const;
    [[nodiscard]] PressureIntegral calculatePlanarPressureIntegral(double beta) const noexcept;
    [[nodiscard]] double           calculateLiftingLineBodyCp() const noexcept;
    [[nodiscard]] double           calculateSlenderBodyCp() const noexcept;
    [[nodiscard]] double           getRadiusSemispanRatio() const noexcept;
    [[nodiscard]] bool             isRectangularPlanform() const noexcept;
    [[nodiscard]] double           interpolateChart16(double betaAspectRatio,
                                                      double radiusSemispanRatio) const noexcept;
    [[nodiscard]] double           getLeadingEdgeSweepFraction() const noexcept;

    double m_bodyRadius;
    double m_exposedSpan;
    double m_rootChord;
    double m_tipChord;
    double m_leadingEdgeSweep;
    double m_aspectRatio;
    double m_bodyEnd;

    mutable std::optional<CachedResult>     m_cachedResult;
    mutable std::optional<PressureIntegral> m_recentPressureIntegral;
    mutable std::optional<PressureIntegral> m_previousPressureIntegral;
};

}  // namespace QtRocket
