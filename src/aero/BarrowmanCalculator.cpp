#include "QtRocket/aero/BarrowmanCalculator.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/aero/AbstractAerodynamicCalculator.h"
#include "QtRocket/aero/AerodynamicCalculator.h"
#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/BarrowmanDragCalculator.h"
#include "QtRocket/aero/BarrowmanStabilityCalculator.h"
#include "QtRocket/aero/DragCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/aero/StabilityCalculator.h"
#include "QtRocket/aero/StabilityForceBreakdown.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/InstanceMap.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

namespace
{

/// The pressure, base, friction and override drag of an assembly.
struct AssemblyDrag
{
    double pressureCD{0};
    double baseCD{0};
    double frictionCD{0};
    double overrideCD{0};
};

/// Makes the entry @p forces of a force analysis presentable: a NaN CP or drag part becomes 0,
/// CD the sum of the parts and CDaxial @p dragCalculator's axial drag of it.
void finishEntry(AerodynamicForces& forces, const DragCalculator& dragCalculator,
                 const FlightConditions& conditions)
{
    if (forces.getCP().isNaN())
    {
        forces.setCP(Coordinate::kZero);
    }
    if (std::isnan(forces.getBaseCD()))
    {
        forces.setBaseCD(0);
    }
    if (std::isnan(forces.getPressureCD()))
    {
        forces.setPressureCD(0);
    }
    if (std::isnan(forces.getFrictionCD()))
    {
        forces.setFrictionCD(0);
    }
    if (std::isnan(forces.getOverrideCD()))
    {
        forces.setOverrideCD(0);
    }

    const double cd = forces.getBaseCD() + forces.getPressureCD() + forces.getFrictionCD() +
                      forces.getOverrideCD();
    forces.setCD(cd);
    forces.setCDaxial(dragCalculator.toAxialDrag(conditions, cd));
}

/// The drag of the aerodynamic descendants of @p assembly in @p forceMap, each weighted with
/// its number of active instances over @p assemblyInstanceCount.
[[nodiscard]] AssemblyDrag descendantDrag(const RocketComponent& assembly,
                                          int assemblyInstanceCount, const ForceMap& forceMap,
                                          const InstanceMap& activeInstances)
{
    AssemblyDrag drag;
    for (const auto& [component, componentForce] : forceMap)
    {
        if (!component->isAerodynamic() || !assembly.isAncestor(*component))
        {
            continue;
        }

        const double instanceRatio =
            static_cast<double>(activeInstances.count(*component)) / assemblyInstanceCount;
        drag.pressureCD += componentForce.getPressureCD() * instanceRatio;
        drag.baseCD += componentForce.getBaseCD() * instanceRatio;
        drag.frictionCD += componentForce.getFrictionCD() * instanceRatio;
        if (component->isCDOverridden() && !component->isCDOverriddenByAncestor())
        {
            drag.overrideCD += component->getOverrideCD() * instanceRatio;
        }
    }
    return drag;
}

/// The override CD of the overridden assemblies of @p forceMap in or at @p assembly, each
/// weighted with its number of active instances over @p assemblyInstanceCount.
[[nodiscard]] double assemblyOverrideDrag(const RocketComponent& assembly,
                                          int assemblyInstanceCount, const ForceMap& forceMap,
                                          const InstanceMap& activeInstances)
{
    double overrideCD = 0;
    for (const auto& [overriddenAssembly, forces] : forceMap)
    {
        if (!isAssembly(overriddenAssembly->kind()) || !overriddenAssembly->isCDOverridden() ||
            overriddenAssembly->isCDOverriddenByAncestor() ||
            (&assembly != overriddenAssembly && !assembly.isAncestor(*overriddenAssembly)))
        {
            continue;
        }

        const double instanceRatio =
            static_cast<double>(activeInstances.count(*overriddenAssembly)) / assemblyInstanceCount;
        overrideCD += overriddenAssembly->getOverrideCD() * instanceRatio;
    }
    return overrideCD;
}

/// The stages @p component lies in, from the inside out: the nearest stage at or above it,
/// then the stage that one sits on, and so on up to a stage of the rocket.
[[nodiscard]] std::vector<const RocketComponent*> stagesAround(const RocketComponent& component)
{
    std::vector<const RocketComponent*> stages;
    for (const RocketComponent* c = &component; c != nullptr; c = c->getParent())
    {
        if (isStage(c->kind()))
        {
            stages.push_back(c);
        }
    }
    return stages;
}

/// Why the breakdown of the stability calculators has no entry for the active component
/// @p component, when the reason is the selection of stages: the breakdown replaces an inactive
/// stage by its active top-level child stages, one level deep, so it reaches a stage exactly
/// when no two stages in a row around it are inactive. The text names the first active stage
/// below two inactive ones, from @p component outwards; nullopt when there is none, which
/// means that the breakdown should have had the entry.
[[nodiscard]] std::optional<std::string> unreachedStageText(
    const FlightConfiguration& configuration, const RocketComponent& component)
{
    const std::vector<const RocketComponent*> stages = stagesAround(component);
    for (std::size_t i = 0; i + 2 < stages.size(); i++)
    {
        if (configuration.isStageActive(stages[i]->getStageNumber()) &&
            !configuration.isStageActive(stages[i + 1]->getStageNumber()) &&
            !configuration.isStageActive(stages[i + 2]->getStageNumber()))
        {
            return BarrowmanCalculator::unsupportedStageSelectionText(
                stages[i]->getName(), stages[i + 1]->getName(), stages[i + 2]->getName());
        }
    }
    return std::nullopt;
}

}  // namespace

BarrowmanCalculator::BarrowmanCalculator()
  : BarrowmanCalculator(std::make_unique<BarrowmanStabilityCalculator>(),
                        std::make_unique<BarrowmanDragCalculator>())
{
}

BarrowmanCalculator::BarrowmanCalculator(std::unique_ptr<StabilityCalculator> stabilityCalculator,
                                         std::unique_ptr<DragCalculator>      dragCalculator)
  : m_stabilityCalculator(std::move(stabilityCalculator)),
    m_dragCalculator(std::move(dragCalculator))
{
    if (m_stabilityCalculator == nullptr || m_dragCalculator == nullptr)
    {
        bug("Calculators must not be null");  // Java: IllegalArgumentException
    }
}

std::unique_ptr<AerodynamicCalculator> BarrowmanCalculator::newInstance() const
{
    return std::make_unique<BarrowmanCalculator>(m_stabilityCalculator->newInstance(),
                                                 m_dragCalculator->newInstance());
}

double BarrowmanCalculator::getStallAngle() const
{
    return m_stabilityCalculator->getStallAngle();
}

Coordinate BarrowmanCalculator::getCP(const FlightConfiguration& configuration,
                                      const FlightConditions& conditions, WarningSet* warnings)
{
    checkCache(configuration);
    return m_stabilityCalculator->getCP(configuration, conditions, actualWarnings(warnings));
}

std::string BarrowmanCalculator::unsupportedStageSelectionText(std::string_view active,
                                                               std::string_view above,
                                                               std::string_view top)
{
    return std::format(
        "The force analysis cannot be made for this selection of stages: stage "
        "'{}' is active below two inactive stages ('{}' and '{}').",
        active, above, top);
}

Result<ForceMap> BarrowmanCalculator::getForceAnalysis(const FlightConfiguration& configuration,
                                                       const FlightConditions&    conditions,
                                                       WarningSet*                warnings)
{
    checkCache(configuration);

    WarningSet&             actual = actualWarnings(warnings);
    StabilityForceBreakdown breakdown =
        m_stabilityCalculator->getForceAnalysis(configuration, conditions, actual);

    ForceMap& eachMap     = breakdown.getComponentForces();
    ForceMap& assemblyMap = breakdown.getAssemblyForces();

    const Rocket&            rocket       = configuration.getRocket();
    AerodynamicForces* const rocketForces = assemblyMap.get(&rocket);
    if (rocketForces == nullptr)
    {
        bug("the force analysis has no entry for the rocket");  // Java: a NullPointerException
    }
    m_dragCalculator->calculateDrag(configuration, conditions, &eachMap, &assemblyMap,
                                    *rocketForces, actual);

    ForceMap finalMap;
    for (const auto& [comp, contexts] : configuration.getActiveInstances())
    {
        const AerodynamicForces* source = nullptr;
        if (isAssembly(comp->kind()))
        {
            source = assemblyMap.get(comp);
        }
        else if (comp->isAerodynamic())
        {
            source = eachMap.get(comp);
        }
        else
        {
            continue;
        }
        if (source == nullptr)
        {
            // Java: a NullPointerException, in both cases. An active stage below two inactive
            // ones is a selection of the user's, and so an ordinary failure (see the header);
            // any other missing entry is a defect of the stability calculator.
            if (std::optional<std::string> text = unreachedStageText(configuration, *comp))
            {
                return fail(ErrorCode::INVALID_ARGUMENT, std::move(*text));
            }
            bug(std::format("the force analysis has no entry for component {}", comp->getName()));
        }

        // Java changes the object the breakdown holds and puts that; the breakdown is dropped.
        finishEntry(finalMap.put(comp, *source), *m_dragCalculator, conditions);
    }

    aggregateAssemblyDrag(configuration, conditions, finalMap);

    return finalMap;
}

void BarrowmanCalculator::aggregateAssemblyDrag(const FlightConfiguration& configuration,
                                                const FlightConditions&    conditions,
                                                ForceMap&                  forceMap) const
{
    const InstanceMap& activeInstances = configuration.getActiveInstances();
    const Rocket&      rocket          = configuration.getRocket();
    for (auto& [assembly, assemblyForce] : forceMap)
    {
        const int assemblyInstanceCount = activeInstances.count(*assembly);
        if (!isAssembly(assembly->kind()) || assembly == &rocket || assemblyInstanceCount == 0)
        {
            continue;
        }

        AssemblyDrag drag =
            descendantDrag(*assembly, assemblyInstanceCount, forceMap, activeInstances);

        // An assembly override has no inherent pressure, base, or friction component, but it
        // contributes to the selected assembly's total CD.
        drag.overrideCD +=
            assemblyOverrideDrag(*assembly, assemblyInstanceCount, forceMap, activeInstances);

        const double totalCD = drag.pressureCD + drag.baseCD + drag.frictionCD + drag.overrideCD;
        assemblyForce.setPressureCD(drag.pressureCD);
        assemblyForce.setBaseCD(drag.baseCD);
        assemblyForce.setFrictionCD(drag.frictionCD);
        assemblyForce.setOverrideCD(drag.overrideCD);
        assemblyForce.setCD(totalCD);
        assemblyForce.setCDaxial(m_dragCalculator->toAxialDrag(conditions, totalCD));
    }
}

AerodynamicForces BarrowmanCalculator::getAerodynamicForces(
    const FlightConfiguration& configuration, const FlightConditions& conditions,
    WarningSet* warnings)
{
    checkCache(configuration);

    WarningSet& actual = actualWarnings(warnings);

    AerodynamicForces total =
        m_stabilityCalculator->calculateNonAxialForces(configuration, conditions, actual);

    m_dragCalculator->calculateDrag(configuration, conditions, nullptr, nullptr, total, actual);

    m_stabilityCalculator->calculateDampingMoments(configuration, conditions, total);
    total.setCm(total.getCm() - total.getPitchDampingMoment());
    total.setCyaw(total.getCyaw() - total.getYawDampingMoment());

    return total;
}

void BarrowmanCalculator::checkGeometry(const FlightConfiguration& configuration,
                                        const RocketComponent& component, WarningSet* warnings)
{
    m_stabilityCalculator->checkGeometry(configuration, component, actualWarnings(warnings));
}

void BarrowmanCalculator::voidAerodynamicCache()
{
    AbstractAerodynamicCalculator::voidAerodynamicCache();
    m_stabilityCalculator->voidAerodynamicCache();
    m_dragCalculator->voidAerodynamicCache();
}

double BarrowmanCalculator::calculateStagnationCD(double mach) noexcept
{
    return BarrowmanDragCalculator::calculateStagnationCD(mach);
}

double BarrowmanCalculator::calculateBaseCD(double mach) noexcept
{
    return BarrowmanDragCalculator::calculateBaseCD(mach);
}

}  // namespace QtRocket
