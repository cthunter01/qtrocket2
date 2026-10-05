#pragma once

#include <limits>
#include <optional>

#include "QtRocket/aero/barrowman/RocketComponentCalc.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/LinearInterpolator.h"

namespace QtRocket
{

class AerodynamicForces;
class FlightConditions;
class SymmetricComponent;
class Transformation;
class WarningSet;

/// The aerodynamic calculation of a body tube, transition or nose cone (OpenRocket's
/// aerodynamics/barrowman/SymmetricComponentCalc).
///
/// The CP and CNa are the Barrowman (slender body) values, extended by the body lift of Galejs;
/// the supersonic CNa and CP are taken to be the subsonic ones. The friction drag is the wetted
/// area times the coefficient of friction. The pressure drag is 0 for a cylinder, the stagnation
/// or base drag of the step for a component shorter than 1 mm, the base drag scaled by the
/// fineness ratio for a boat tail, and for a nose cone or a widening transition the value of an
/// interpolator over the Mach number built from closed forms (conical, ogive) or from the
/// measured drag of fineness-ratio-3 nose cones (NASA TR-R-100), extrapolated to the component's
/// fineness ratio and continued into the subsonic region.
///
/// Everything is copied from the component at construction (its length, radii, shape and shape
/// parameter, areas, volume and the sine of the nose half-angle), as in Java, so the calculator
/// neither keeps the component nor sees later changes to it: make a new one when the component
/// changes. Two things are computed on first use and then kept: the slender-body CNa and CP (by
/// calculateNonaxialForces()) and the pressure drag interpolator (by the first
/// calculatePressureCD() that gets past the simple cases).
///
/// Deviations from OpenRocket:
/// - The class is final and getLiftCP() private (Java: protected); nothing in OpenRocket
///   extends it. getLiftCP() does not take the warning set, which Java's passes and ignores.
/// - A component that is neither a body tube nor a transition throws BugError (Java:
///   UnsupportedOperationException), and so does the pressure drag of a body tube whose radii
///   do not compare equal (a NaN radius; Java: NullPointerException on its null shape), an
///   inconsistent shape parameter (Java: BugException) and an unknown shape.
/// - When building the pressure drag interpolator throws, none is kept and the next call throws
///   the same error again (Java keeps the empty interpolator it started with, whose getValue()
///   then throws IllegalStateException).
class SymmetricComponentCalc final : public RocketComponentCalc
{
public:
    /// The body lift coefficient of Galejs' method (BODY_LIFT_K).
    static constexpr double kBodyLiftK = 1.1;

    /// A calculation for @p component, whose geometry is copied (see the class comment).
    /// @throws BugError when @p component is neither a BodyTube nor a Transition.
    explicit SymmetricComponentCalc(const SymmetricComponent& component);

    /// The CP with CNa as its weight, CN and Cm (about the component's front) into @p forces;
    /// the side force, yaw and roll coefficients are 0. A cylinder has the body lift alone;
    /// otherwise the slender-body CNa 2 (A_aft - A_fore) sinc(aoa) / A_ref at
    /// (length A_aft - volume) / (A_aft - A_fore) is averaged with it. Adds Warning::kSupersonic
    /// above Mach 1.1.
    void calculateNonaxialForces(const FlightConditions& conditions,
                                 const Transformation& transform, AerodynamicForces& forces,
                                 WarningSet& warnings) override;

    /// @p componentCf times the wetted area over the reference area.
    [[nodiscard]] double calculateFrictionCD(const FlightConditions& conditions, double componentCf,
                                             WarningSet& warnings) override;

    /// The pressure drag coefficient (see the class comment).
    /// @throws BugError see the class comment.
    [[nodiscard]] double calculatePressureCD(const FlightConditions& conditions,
                                             double stagnationCD, double baseCD,
                                             WarningSet& warnings) override;

private:
    /// The body lift according to Galejs: at the planform centre, with the weight
    /// kBodyLiftK planform area / reference area * sin(aoa) sinc(aoa), times (Mach / 0.05)^2
    /// below Mach 0.05 when the angle of attack exceeds pi / 4 (which keeps a rocket turning
    /// over at apogee from oscillating).
    [[nodiscard]] Coordinate getLiftCP(const FlightConditions& conditions) const noexcept;

    /// Builds the pressure drag interpolator of a nose cone or widening transition
    /// (calculateNoseInterpolator()).
    [[nodiscard]] LinearInterpolator calculateNoseInterpolator() const;

    // Copied from the component at construction.
    double                         m_length;
    double                         m_foreRadius{0};
    double                         m_aftRadius{0};
    double                         m_fineness{0};
    std::optional<TransitionShape> m_shape;  // none for a body tube (Java: null)
    double                         m_param{0};
    double                         m_frontalArea{0};
    double                         m_fullVolume{0};
    double                         m_planformArea{0};
    double                         m_planformCenter{0};
    double                         m_wetArea{0};
    double                         m_sinphi{0};

    // Computed by the first calculateNonaxialForces().
    bool   m_isTube{false};
    double m_cnaCache{std::numeric_limits<double>::quiet_NaN()};
    double m_cpCache{std::numeric_limits<double>::quiet_NaN()};

    /// The pressure drag coefficient (relative to the frontal area) over the Mach number; built
    /// when first needed (Java: null until then).
    std::optional<LinearInterpolator> m_interpolator;
};

}  // namespace QtRocket
