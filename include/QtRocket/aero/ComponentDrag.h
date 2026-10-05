#pragma once

namespace QtRocket
{

class RocketComponent;

/// The drag coefficient of one component of a rocket, calculated with the Barrowman method: the
/// two methods of OpenRocket's RocketComponent that need an aerodynamic calculator,
/// getComponentCD() and the calculating half of getOverrideCD(). They are free functions here
/// because rocket/ does not include aero/ (see RocketComponent, "In aero/").
///
/// The GUI calls getOverrideCD() wherever OpenRocket calls component.getOverrideCD() to show a
/// value (the override tab of a component's configuration dialog, the component analysis), with
/// Preferences::getDefaultMach() as the Mach number.
namespace ComponentDrag
{

/// The CD of @p component at the angle of attack @p aoa and the wind direction @p theta (both
/// in radians), the Mach number @p mach and the roll rate @p rollRate (in radians per second)
/// (Java: RocketComponent.getComponentCD(AOA, theta, mach, rollRate)): AerodynamicForces::getCD()
/// of the component's entry in a new BarrowmanCalculator's force analysis of the selected
/// configuration of the component's rocket, in FlightConditions made for that configuration.
/// 0 for a component without an entry (one that is neither aerodynamic nor an assembly, or
/// whose stage is not active) and for a component that is not in a rocket (Java: the
/// IllegalStateException of getRocket() is caught). Nothing is cached: every call builds a
/// calculator and runs a force analysis of the whole rocket.
/// @throws BugError when the force analysis cannot be made of the selected configuration, for
///         whichever component of the rocket is asked: with an active stage below two inactive
///         ones (see BarrowmanCalculator; Java: a NullPointerException). The selection of
///         stages is the user's, so the GUI has to expect it.
[[nodiscard]] double getComponentCD(const RocketComponent& component, double aoa, double theta,
                                    double mach, double rollRate);

/// The override CD of @p component as OpenRocket's RocketComponent.getOverrideCD() gives it:
/// the stored override CD (RocketComponent::getOverrideCD()) while the CD is overridden,
/// otherwise the calculated getComponentCD(component, 0, 0, @p defaultMach, 0). Java takes the
/// Mach number from the application preferences (ApplicationPreferences.getDefaultMach()); pass
/// Preferences::getDefaultMach().
///
/// Deviation: Java also stores the calculated value in the component (without an event), so
/// that the override starts from it when it is switched on; here the component is not touched.
/// A caller that wants that sets it before switching the override on:
/// component.setOverrideCD(ComponentDrag::getOverrideCD(component, mach)).
/// @throws BugError as getComponentCD(), while the CD is not overridden.
[[nodiscard]] double getOverrideCD(const RocketComponent& component, double defaultMach);

}  // namespace ComponentDrag

}  // namespace QtRocket
