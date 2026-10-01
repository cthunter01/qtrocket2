#pragma once

#include <utility>

#include "QtRocket/aero/ForceMap.h"

namespace QtRocket
{

/// The non-axial forces of a force analysis, broken down by component and by assembly
/// (OpenRocket's aerodynamics/StabilityForceBreakdown), as StabilityCalculator::getForceAnalysis()
/// returns them: the component map holds each aerodynamic component's forces, the assembly map
/// each assembly's forces summed over its subtree, with the rocket's entry the total. The
/// AerodynamicCalculator then adds the drag through DragCalculator::calculateDrag(), which is why
/// the maps can be modified in place.
class StabilityForceBreakdown
{
public:
    StabilityForceBreakdown(ForceMap componentForces, ForceMap assemblyForces)
      : m_componentForces(std::move(componentForces)), m_assemblyForces(std::move(assemblyForces))
    {
    }

    [[nodiscard]] ForceMap&       getComponentForces() noexcept { return m_componentForces; }
    [[nodiscard]] const ForceMap& getComponentForces() const noexcept { return m_componentForces; }

    [[nodiscard]] ForceMap&       getAssemblyForces() noexcept { return m_assemblyForces; }
    [[nodiscard]] const ForceMap& getAssemblyForces() const noexcept { return m_assemblyForces; }

private:
    ForceMap m_componentForces;
    ForceMap m_assemblyForces;
};

}  // namespace QtRocket
