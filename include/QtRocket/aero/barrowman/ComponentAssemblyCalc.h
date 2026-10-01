#pragma once

#include "QtRocket/aero/barrowman/RocketComponentCalc.h"

namespace QtRocket
{

class AerodynamicForces;
class FlightConditions;
class RocketComponent;
class Transformation;
class WarningSet;

/// The aerodynamic calculation of a component assembly (rocket, stage, booster set, pod set;
/// OpenRocket's aerodynamics/barrowman/ComponentAssemblyCalc). An assembly has no effect of its
/// own beyond the sum of its parts, so it adds no forces and no drag.
class ComponentAssemblyCalc final : public RocketComponentCalc
{
public:
    /// A calculation for the assembly @p component (which it does not read).
    explicit ComponentAssemblyCalc(const RocketComponent& component);

    /// Leaves @p forces untouched.
    void calculateNonaxialForces(const FlightConditions& conditions,
                                 const Transformation& transform, AerodynamicForces& forces,
                                 WarningSet& warnings) override;

    /// 0.
    [[nodiscard]] double calculateFrictionCD(const FlightConditions& conditions, double componentCf,
                                             WarningSet& warnings) override;

    /// 0.
    [[nodiscard]] double calculatePressureCD(const FlightConditions& conditions,
                                             double stagnationCD, double baseCD,
                                             WarningSet& warnings) override;
};

}  // namespace QtRocket
