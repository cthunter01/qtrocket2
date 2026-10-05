#include "QtRocket/aero/BarrowmanStabilityCalculator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/aero/StabilityCalculator.h"
#include "QtRocket/aero/StabilityForceBreakdown.h"
#include "QtRocket/aero/barrowman/FinSetCalc.h"
#include "QtRocket/aero/barrowman/RocketComponentCalc.h"
#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/InstanceContext.h"
#include "QtRocket/rocket/InstanceMap.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

namespace
{

/// @p component as a SymmetricComponent, or nullptr (Java: instanceof SymmetricComponent).
[[nodiscard]] const SymmetricComponent* asSymmetric(const RocketComponent& component) noexcept
{
    return dynamic_cast<const SymmetricComponent*>(&component);
}

/// @p component as the source of a warning.
[[nodiscard]] MessageSource sourceOf(const RocketComponent& component)
{
    return MessageSource{component.getId(), component.getName()};
}

/// @p value as the default length unit prints it (Java:
/// UnitGroup.UNITS_LENGTH.getDefaultUnit().toStringUnit(value)).
[[nodiscard]] std::string formatLength(double value)
{
    return unitGroup(UnitGroupId::LENGTH).getDefaultUnit().toStringUnit(value);
}

/// The x of @p c (relative to @p component) in absolute coordinates, at the component's first
/// instance (Java: component.toAbsolute(c)[0].getX()).
[[nodiscard]] double firstAbsoluteX(const RocketComponent& component, const Coordinate& c)
{
    const std::vector<Coordinate> absolute = component.toAbsolute(c);
    if (absolute.empty())
    {
        bug("a component has no absolute location");  // Java: an array index error
    }
    return absolute[0].x;
}

/// The children of @p comp that a walk of the active rocket goes on with: each child, except
/// that an inactive stage is replaced by its active top-level child stages (Java: the loop of
/// addDirectChildStagesToQueue(), which calculateForceAnalysis() repeats).
[[nodiscard]] std::vector<const RocketComponent*> directChildStages(
    const FlightConfiguration& configuration, const RocketComponent& comp)
{
    std::vector<const RocketComponent*> result;
    for (const RocketComponent* const child : comp.getChildren())
    {
        if (isStage(child->kind()) && !configuration.isStageActive(child->getStageNumber()))
        {
            for (const AxialStage* const childStage : child->getTopLevelChildStages())
            {
                if (configuration.isStageActive(childStage->getStageNumber()))
                {
                    result.push_back(childStage);
                }
            }
            continue;
        }
        result.push_back(child);
    }
    return result;
}

/// Appends directChildStages() of @p comp to @p queue (Java: addDirectChildStagesToQueue()).
void appendDirectChildStages(const FlightConfiguration&          configuration,
                             std::deque<const RocketComponent*>& queue, const RocketComponent& comp)
{
    const std::vector<const RocketComponent*> children = directChildStages(configuration, comp);
    queue.insert(queue.end(), children.begin(), children.end());
}

/// The merged forces of the instances @p contextList of @p comp, associated with it.
[[nodiscard]] AerodynamicForces calculateComponentNonAxialForces(
    const FlightConditions& conditions, const RocketComponent& comp, RocketComponentCalc& calcObj,
    std::span<const InstanceContext> contextList, WarningSet& warnings)
{
    AerodynamicForces componentForces = AerodynamicForces{}.zero();

    for (const InstanceContext& context : contextList)
    {
        AerodynamicForces instanceForces = AerodynamicForces{}.zero();
        calcObj.calculateNonaxialForces(conditions, context.transform, instanceForces, warnings);

        const Coordinate cpInst = instanceForces.getCP();
        Coordinate       cpAbs  = context.transform.transform(cpInst);
        cpAbs                   = cpAbs.setY(0.0).setZ(0.0);

        instanceForces.setCP(cpAbs);
        const double cNInst = instanceForces.getCN();
        instanceForces.setCm(cNInst * instanceForces.getCP().x / conditions.getRefLength());

        componentForces.merge(instanceForces);
    }

    componentForces.setComponent(&comp);

    return componentForces;
}

/// checkGeometry() for the first symmetric component in line: the open forward end.
void checkForwardEnd(const FlightConfiguration& configuration, const SymmetricComponent& sym,
                     WarningSet& warnings)
{
    if (sym.getForeRadius() - sym.getThickness() > MathUtil::kEpsilon)
    {
        // only record open airframe warning if it's the sustainer or it has a recovery device
        const bool              sustainer   = configuration.isStageActive(0);
        const AxialStage* const bottomStage = configuration.getBottomStage();
        if (bottomStage == nullptr)
        {
            bug("no stage is active");  // Java: a NullPointerException
        }
        const bool hasRecoveryDevice = bottomStage->hasRecoveryDevice();

        if (sustainer || hasRecoveryDevice)
        {
            warnings.add(Warning::kOpenAirframeForward, MessageSources{sourceOf(sym)});
        }
    }
}

/// The absolute x of the fronts and the ends of a symmetric component and of the one before it.
struct JointPositions
{
    double symXfore;
    double prevXfore;
    double symXaft;
    double prevXaft;
};

/// checkGeometry() for a component whose front does not print as the end of the previous one:
/// a gap, an overlap, or a pod set that is ahead of or overlaps its parent.
void checkSeparatedJoint(const SymmetricComponent& prevComp, const SymmetricComponent& sym,
                         const JointPositions& x, WarningSet& warnings)
{
    if (x.symXfore > x.prevXaft)
    {
        warnings.add(Warning::kAirframeGap, MessageSources{sourceOf(prevComp), sourceOf(sym)});
        return;
    }

    if ((x.symXfore >= x.prevXfore) &&
        ((x.symXaft >= x.prevXaft) || (sym.getNextSymmetricComponent() == nullptr)))
    {
        warnings.add(Warning::kAirframeOverlap, MessageSources{sourceOf(prevComp), sourceOf(sym)});
        return;
    }

    const SymmetricComponent* firstComp = &prevComp;
    const SymmetricComponent* scout     = &prevComp;
    while (scout != nullptr)
    {
        firstComp = scout;
        scout     = scout->getPreviousSymmetricComponent();
    }
    const double firstCompXfore = firstAbsoluteX(*firstComp, Coordinate::kNul);

    const SymmetricComponent* lastComp = &sym;
    scout                              = &sym;
    while (scout != nullptr)
    {
        lastComp = scout;
        scout    = scout->getNextSymmetricComponent();
    }
    const double lastCompXaft =
        firstAbsoluteX(*lastComp, Coordinate{lastComp->getLength(), 0, 0, 0});

    const RocketComponent* const parent = sym.getParent();
    QTROCKET_ASSERT(parent != nullptr);
    if (lastCompXaft <= firstCompXfore)
    {
        warnings.add(Warning::kPodsetForward, MessageSources{sourceOf(*parent)});
    }
    else
    {
        warnings.add(Warning::kPodsetOverlap, MessageSources{sourceOf(*parent)});
    }
}

/// checkGeometry() for a component whose front prints as the end of the previous one: a pod set
/// or a booster set that starts flush with the end of the component ahead of its own parent.
void checkFlushJoint(const SymmetricComponent& prevComp, const SymmetricComponent& sym,
                     const JointPositions& x, WarningSet& warnings)
{
    const RocketComponent* const prevCompParent = prevComp.getParent();
    const RocketComponent* const compParent     = sym.getParent();
    QTROCKET_ASSERT(prevCompParent != nullptr);  // Java: a NullPointerException

    // Java: int prevCompPos, -1 when it is not a child.
    const std::optional<std::size_t> prevCompPos = prevCompParent->getChildPosition(&prevComp);
    const std::size_t                nextPos     = prevCompPos.has_value() ? *prevCompPos + 1 : 0;
    const RocketComponent* const     nextComp =
        nextPos >= prevCompParent->getChildCount() ? nullptr : &prevCompParent->getChild(nextPos);

    const bool onPodOrBooster =
        compParent != nullptr && (compParent->kind() == ComponentKind::POD_SET ||
                                  compParent->kind() == ComponentKind::PARALLEL_STAGE);
    if (onPodOrBooster && MathUtil::equals(x.symXfore, x.prevXaft) &&
        (compParent->getParent() == nextComp))
    {
        warnings.add(Warning::kPodsetOverlap, MessageSources{sourceOf(*compParent)});
    }
}

/// checkGeometry() for @p sym, which follows @p prevComp in line.
void checkJoint(const SymmetricComponent& prevComp, const SymmetricComponent& sym,
                WarningSet& warnings)
{
    if (formatLength(2.0 * sym.getForeRadius()) != formatLength(2.0 * prevComp.getAftRadius()))
    {
        warnings.add(Warning::kDiameterDiscontinuity,
                     MessageSources{sourceOf(prevComp), sourceOf(sym)});
    }

    if ((sym.getLength() < MathUtil::kEpsilon) ||
        (sym.getAftRadius() < MathUtil::kEpsilon && sym.getForeRadius() < MathUtil::kEpsilon))
    {
        warnings.add(Warning::kZeroVolumeBody, MessageSources{sourceOf(sym)});
    }

    const JointPositions x{
        .symXfore  = firstAbsoluteX(sym, Coordinate::kNul),
        .prevXfore = firstAbsoluteX(prevComp, Coordinate::kNul),
        .symXaft   = firstAbsoluteX(sym, Coordinate{sym.getLength(), 0, 0, 0}),
        .prevXaft  = firstAbsoluteX(prevComp, Coordinate{prevComp.getLength(), 0, 0, 0})};

    if (formatLength(x.symXfore) != formatLength(x.prevXaft))
    {
        checkSeparatedJoint(prevComp, sym, x, warnings);
    }
    else
    {
        checkFlushJoint(prevComp, sym, x, warnings);
    }
}

}  // namespace

std::unique_ptr<StabilityCalculator> BarrowmanStabilityCalculator::newInstance() const
{
    return std::make_unique<BarrowmanStabilityCalculator>();
}

Coordinate BarrowmanStabilityCalculator::getCP(const FlightConfiguration& configuration,
                                               const FlightConditions&    conditions,
                                               WarningSet&                warnings)
{
    return calculateNonAxialForces(configuration, conditions, warnings).getCP();
}

AerodynamicForces BarrowmanStabilityCalculator::calculateNonAxialForces(
    const FlightConfiguration& configuration, const FlightConditions& conditions,
    WarningSet& warnings)
{
    m_calcMap.ensureBuilt(configuration);

    const InstanceMap& imap           = configuration.getActiveInstances();
    AerodynamicForces  assemblyForces = AerodynamicForces{}.zero();

    for (const auto& [comp, contextList] : imap)
    {
        RocketComponentCalc* const calcObj = m_calcMap.get(*comp);
        if (calcObj == nullptr)
        {
            continue;
        }

        const AerodynamicForces componentForces =
            calculateComponentNonAxialForces(conditions, *comp, *calcObj, contextList, warnings);
        assemblyForces.merge(componentForces);
    }

    return assemblyForces;
}

StabilityForceBreakdown BarrowmanStabilityCalculator::getForceAnalysis(
    const FlightConfiguration& configuration, const FlightConditions& conditions,
    WarningSet& warnings)
{
    m_calcMap.ensureBuilt(configuration);

    const InstanceMap& instances = configuration.getActiveInstances();

    ForceMap eachMap;
    ForceMap assemblyMap;

    // The forces it returns, those of the rocket's subtree, are the rocket's entry of the
    // assembly map as well.
    calculateForceAnalysis(configuration, conditions, configuration.getRocket(), instances, eachMap,
                           assemblyMap, warnings);

    return StabilityForceBreakdown{std::move(eachMap), std::move(assemblyMap)};
}

void BarrowmanStabilityCalculator::calculateDampingMoments(const FlightConfiguration& configuration,
                                                           const FlightConditions&    conditions,
                                                           AerodynamicForces&         total)
{
    m_calcMap.ensureBuilt(configuration);

    double mul = getDampingMultiplier(configuration, conditions, conditions.getPitchCenter().x);
    const double pitchRate = conditions.getPitchRate();
    const double yawRate   = conditions.getYawRate();
    const double velocity  = conditions.getVelocity();

    mul *= 3;  // Higher damping yields much more realistic apogee turn

    const double pitchDampingMomentMagnitude =
        MathUtil::min(mul * MathUtil::pow2(pitchRate / velocity), total.getCm());
    const double yawDampingMomentMagnitude =
        MathUtil::min(mul * MathUtil::pow2(yawRate / velocity), total.getCyaw());

    total.setPitchDampingMoment(MathUtil::sign(pitchRate) * pitchDampingMomentMagnitude);
    total.setYawDampingMoment(MathUtil::sign(yawRate) * yawDampingMomentMagnitude);
}

void BarrowmanStabilityCalculator::checkGeometry(const FlightConfiguration& configuration,
                                                 const RocketComponent&     component,
                                                 WarningSet&                warnings)
{
    std::deque<const RocketComponent*> queue;
    appendDirectChildStages(configuration, queue, component);

    const SymmetricComponent* prevComp = nullptr;
    if (isAssembly(component.kind()) && (component.kind() != ComponentKind::ROCKET) &&
        (component.getChildCount() > 0))
    {
        if (const SymmetricComponent* const firstChild = asSymmetric(component.getChild(0)))
        {
            prevComp = firstChild->getPreviousSymmetricComponent();
        }
    }

    while (!queue.empty())
    {
        const RocketComponent* const comp = queue.front();
        queue.pop_front();

        const SymmetricComponent* const sym = asSymmetric(*comp);
        if ((sym != nullptr) || (comp->kind() == ComponentKind::AXIAL_STAGE))
        {
            appendDirectChildStages(configuration, queue, *comp);

            if (sym != nullptr)
            {
                if (prevComp == nullptr)
                {
                    checkForwardEnd(configuration, *sym, warnings);
                }
                else
                {
                    checkJoint(*prevComp, *sym, warnings);
                }
                prevComp = sym;
            }
        }
        else if ((comp->kind() == ComponentKind::POD_SET) ||
                 (comp->kind() == ComponentKind::PARALLEL_STAGE))
        {
            checkGeometry(configuration, *comp, warnings);
        }
    }
}

void BarrowmanStabilityCalculator::voidAerodynamicCache()
{
    m_calcMap.clear();
    m_cacheDiameter = -1;
    m_cacheLength   = -1;
}

AerodynamicForces BarrowmanStabilityCalculator::calculateForceAnalysis(
    const FlightConfiguration& configuration, const FlightConditions& conds,
    const RocketComponent& comp, const InstanceMap& instances, ForceMap& eachForces,
    ForceMap& assemblyForces, WarningSet& warnings)
{
    AerodynamicForces aggregateForces = AerodynamicForces{}.zero();
    aggregateForces.setComponent(&comp);

    if (comp.isAerodynamic() || isAssembly(comp.kind()))
    {
        RocketComponentCalc* const calcObj = m_calcMap.get(comp);
        if (calcObj == nullptr)
        {
            // Java: a NullPointerException with this message
            bug(std::format("Could not find a CalculationObject for aerodynamic Component!: {}",
                            comp.getComponentName()));
        }
        if (!instances.containsKey(comp))
        {
            // Java: instances.get(comp) is null, a NullPointerException in the loop over it
            bug(std::format("component {} has no active instance", comp.getName()));
        }
        const AerodynamicForces compForces = calculateComponentNonAxialForces(
            conds, comp, *calcObj, instances.getInstanceContexts(comp), warnings);
        eachForces.put(&comp, compForces);
        aggregateForces.merge(compForces);
    }

    for (const RocketComponent* const child : directChildStages(configuration, comp))
    {
        const AerodynamicForces childForces = calculateForceAnalysis(
            configuration, conds, *child, instances, eachForces, assemblyForces, warnings);
        aggregateForces.merge(childForces);
    }

    assemblyForces.put(&comp, aggregateForces);

    return aggregateForces;
}

double BarrowmanStabilityCalculator::getDampingMultiplier(const FlightConfiguration& configuration,
                                                          const FlightConditions&    conditions,
                                                          double                     cgx)
{
    if (m_cacheDiameter < 0)
    {
        double area     = 0;
        m_cacheLength   = 0;
        m_cacheDiameter = 0;

        for (const RocketComponent* const c : configuration.getActiveComponents())
        {
            if (const SymmetricComponent* const s = asSymmetric(*c))
            {
                area += s->getComponentPlanformArea();
                m_cacheLength += s->getLength();
            }
        }
        if (m_cacheLength > 0)
        {
            m_cacheDiameter = area / m_cacheLength;
        }
    }

    double mul = 0.275 * m_cacheDiameter / (conditions.getRefArea() * conditions.getRefLength());
    mul *= (MathUtil::pow4(cgx) + MathUtil::pow4(m_cacheLength - cgx));

    for (const RocketComponent* const c : configuration.getActiveComponents())
    {
        if (const auto* const f = dynamic_cast<const FinSet*>(c))
        {
            const auto* const finCalc = dynamic_cast<const FinSetCalc*>(m_calcMap.get(*f));
            if (finCalc == nullptr)
            {
                // Java: a NullPointerException
                bug(std::format("fin set {} has no calculation", f->getName()));
            }
            mul += 0.6 * std::min(f->getFinCount(), 4) * f->getPlanformArea() *
                   MathUtil::pow3(
                       std::abs(firstAbsoluteX(*f, Coordinate{finCalc->getMidchordPos()}) - cgx)) /
                   (conditions.getRefArea() * conditions.getRefLength());
        }
    }

    return mul;
}

}  // namespace QtRocket
