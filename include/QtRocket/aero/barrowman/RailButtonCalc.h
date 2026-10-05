#pragma once

#include "QtRocket/aero/barrowman/RocketComponentCalc.h"

namespace QtRocket
{

class AerodynamicForces;
class Coordinate;
class FlightConditions;
class RailButton;
class Transformation;
class WarningSet;

/// The aerodynamic calculation of a rail button (OpenRocket's
/// aerodynamics/barrowman/RailButtonCalc): no non-axial forces and no friction drag (a very
/// small and slick surface), and the pressure drag of a short circular cylinder standing in the
/// boundary layer of the body.
///
/// The pressure drag is that of one instance, the mean over the instances (the calculators of
/// tier 7b multiply by the instance count). For each instance the boundary layer thickness at
/// its absolute axial position x is 0.37 x / Re_x^0.2, and the mean Mach number over the button's
/// height follows from it, assuming that the airspeed grows linearly through the boundary layer:
/// (height - thickness / 2) * Mach / height for a button that reaches beyond the boundary layer,
/// otherwise the Mach number at half its height. The drag coefficient of a cylinder at that Mach
/// number is read from the table of Gowen and Perkins, "Drag of Circular Cylinders for a Wide
/// Range of Reynolds Numbers and Mach Numbers", NACA Technical Note 2960, figure 7, and scaled by
/// (mean Mach / Mach)^2 into an effective coefficient at the rocket's velocity. At a Mach number
/// not above MathUtil::kEpsilon the mean is the constant OpenRocket computed for that velocity.
/// The result is the mean times the stagnation drag coefficient times the button's side area
/// (its outline less the notch) over the reference area.
///
/// Unlike the other component calculators this one keeps the button and reads its dimensions,
/// instances and position at every call, as in Java: it holds a non-owning pointer, so it must
/// not outlive the button (the calculators of tier 7b drop their component calculators when the
/// rocket's tree or aerodynamics change).
///
/// Deviations from OpenRocket:
/// - The class is final; nothing in OpenRocket extends it.
/// - A button without an absolute location throws BugError (Java: an array index error), as
///   does MathUtil::map() for a button no taller than a boundary layer thinner than
///   MathUtil::kEpsilon / 2 (5e-9 m; Java: IllegalArgumentException). The boundary layer is that
///   thin within some 1e-8 m of the front of the rocket, and further aft only at Mach numbers of
///   about 1e10 and more.
class RailButtonCalc final : public RocketComponentCalc
{
public:
    /// A calculation for @p button, which it keeps a pointer to (see the class comment).
    explicit RailButtonCalc(const RailButton& button);

    /// Leaves @p forces untouched.
    void calculateNonaxialForces(const FlightConditions& conditions,
                                 const Transformation& transform, AerodynamicForces& forces,
                                 WarningSet& warnings) override;

    /// 0.
    [[nodiscard]] double calculateFrictionCD(const FlightConditions& conditions, double componentCf,
                                             WarningSet& warnings) override;

    /// The pressure drag coefficient of one instance (see the class comment); @p baseCD is not
    /// used.
    /// @throws BugError see the class comment.
    [[nodiscard]] double calculatePressureCD(const FlightConditions& conditions,
                                             double stagnationCD, double baseCD,
                                             WarningSet& warnings) override;

private:
    /// The effective drag coefficient, at the rocket's velocity, of the instance at the offset
    /// @p instanceOffset of a button @p buttonHt high.
    [[nodiscard]] double effectiveInstanceCD(const FlightConditions& conditions,
                                             const Coordinate&       instanceOffset,
                                             double                  buttonHt) const;

    const RailButton* m_button;  ///< never null; not owned
};

}  // namespace QtRocket
