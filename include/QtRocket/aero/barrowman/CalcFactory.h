#pragma once

#include <memory>

#include "QtRocket/aero/barrowman/RocketComponentCalc.h"
#include "QtRocket/rocket/ComponentKind.h"

namespace QtRocket
{

class RocketComponent;

/// Makes the Barrowman calculation object of a component: what OpenRocket's calculators do with
/// Reflection.construct(BARROWMAN_PACKAGE, component, "Calc", component), which looks for a class
/// named after the component's class, then after each of its superclasses, in the package
/// aerodynamics.barrowman. Here a switch over RocketComponent::kind() makes the same choice:
///
/// | kind                                                    | calculation            |
/// |---------------------------------------------------------|------------------------|
/// | NOSE_CONE, TRANSITION, BODY_TUBE                        | SymmetricComponentCalc |
/// | TRAPEZOID_FIN_SET, ELLIPTICAL_FIN_SET, FREEFORM_FIN_SET | FinSetCalc             |
/// | TUBE_FIN_SET                                            | TubeFinSetCalc         |
/// | LAUNCH_LUG                                              | LaunchLugCalc          |
/// | RAIL_BUTTON                                             | RailButtonCalc         |
/// | ROCKET, AXIAL_STAGE, PARALLEL_STAGE, POD_SET            | ComponentAssemblyCalc  |
///
/// The internal components (ring components and mass objects) have none: Java's search ends at
/// the abstract RocketComponentCalc, which cannot be constructed, and throws a BugException.
/// The calculators never ask for one: they make a calculation only for a component that is
/// aerodynamic or an assembly.
namespace CalcFactory
{

/// Whether create() makes a calculation for a component of @p kind: the external components
/// and the assemblies (Java: `comp.isAerodynamic() || comp instanceof ComponentAssembly`, the
/// components the calculators keep a calculation for).
[[nodiscard]] constexpr bool hasCalculation(ComponentKind kind) noexcept
{
    return isExternal(kind) || isAssembly(kind);
}

/// The calculation object of @p component (see the table above), made from the component as it
/// is now: the calculation's own constructor decides what it copies and what it throws.
/// @throws BugError for a component of a kind without a calculation (Java: BugException), or
///         whose class is not the class of its kind().
[[nodiscard]] std::unique_ptr<RocketComponentCalc> create(const RocketComponent& component);

}  // namespace CalcFactory

}  // namespace QtRocket
