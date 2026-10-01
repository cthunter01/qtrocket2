#include "QtRocket/mass/MassCalculation.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/mass/CMAnalysisEntry.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/InstanceContext.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket
{

MassCalculation::MassCalculation(
    Type type, const FlightConfiguration& config, double time,
    std::optional<std::span<const MotorClusterState* const>> activeMotors,
    const RocketComponent& root, const Transformation& transform, CMAnalysisMap* analysisMap)
  : m_config(&config),
    m_simulationTime(time),
    m_activeMotors(activeMotors),
    m_root(&root),
    m_transform(transform),
    m_type(type),
    m_analysisMap(analysisMap)
{
    reset();
}

// ============================================================================= accumulation

void MassCalculation::merge(const MassCalculation& other)
{
    // Adjust Center-of-mass
    addMass(other.getCM());
    m_bodies.insert(m_bodies.end(), other.m_bodies.begin(), other.m_bodies.end());
}

void MassCalculation::addInertia(const RigidBody& data)
{
    m_bodies.push_back(data);
}

void MassCalculation::scaleInertia(double factor)
{
    for (RigidBody& body : m_bodies)
    {
        body = body.scaleMass(factor);
    }
}

void MassCalculation::addMass(const Coordinate& pointMass)
{
    if (kMinMass > m_centerOfMass.weight)
    {
        m_centerOfMass = pointMass;
    }
    else
    {
        m_centerOfMass = m_centerOfMass.average(pointMass);
    }
}

void MassCalculation::addMass(double mass)
{
    m_centerOfMass = m_centerOfMass.setWeight(getMass() + mass);
}

MassCalculation MassCalculation::copy(const RocketComponent& root,
                                      const Transformation&  transform) const
{
    return MassCalculation{m_type, *m_config, m_simulationTime, m_activeMotors,
                           root,   transform, m_analysisMap};
}

void MassCalculation::reset() noexcept
{
    m_centerOfMass = Coordinate::kZero;
    m_inertia      = RigidBody::kEmpty;
    m_bodies.clear();
}

// ============================================================================== calculation

MassCalculation& MassCalculation::calculateMountData(const MotorConfiguration& motorConfig,
                                                     const MotorClusterState*  motorState)
{
    const RocketComponent& mount = *m_root;
    if (!m_config->isComponentActive(mount))
    {
        return *this;
    }
    if (motorConfig.isEmpty())
    {
        return *this;
    }
    const Motor& motor = *motorConfig.getMotor();

    // Without motor states the time is a synthetic one for a static analysis; with them, each
    // motor's time runs from its own ignition.
    const double motorTime =
        motorState == nullptr ? m_simulationTime : motorState->getMotorTime(m_simulationTime);

    const double mountXPosition           = mount.getPosition().x;
    const int    instanceCount            = mount.getInstanceCount();
    const double motorXPosition           = motorConfig.getX();  // location of motor from mount
    const std::vector<Coordinate> offsets = mount.getInstanceOffsets();

    double eachMass = 0.0;
    double eachCMx  = 0.0;  // CoM from beginning of motor
    if (includesMotorCasing(m_type) && includesPropellant(m_type))
    {
        eachMass = motor.getTotalMass(motorTime);
        eachCMx  = motor.getCMx(motorTime);
    }
    else if (includesMotorCasing(m_type))
    {
        eachMass = motor.getTotalMass(Motor::kPseudoTimeBurnout);
        eachCMx  = motor.getCMx(Motor::kPseudoTimeBurnout);
    }
    else
    {
        const double eachMotorMass  = motor.getTotalMass(motorTime);
        const double eachMotorCMx   = motor.getCMx(motorTime);  // CoM from beginning of motor
        const double eachCasingMass = motor.getBurnoutMass();
        const double eachCasingCMx  = motor.getBurnoutCGx();

        eachMass = eachMotorMass - eachCasingMass;
        eachCMx  = ((eachMotorCMx * eachMotorMass) - (eachCasingCMx * eachCasingMass)) / eachMass;
    }

    // coordinates in rocket frame; Ir, It about CoM.
    const Coordinate clusterLocalCM{mountXPosition + motorXPosition + eachCMx, 0, 0,
                                    eachMass * instanceCount};

    const double clusterBaseIr = motorConfig.getUnitRotationalInertia() * instanceCount * eachMass;
    const double clusterIt = motorConfig.getUnitLongitudinalInertia() * instanceCount * eachMass;

    // if more than 1 motor => motors are not at the centerline => adjust via parallel-axis theorem
    double clusterIr = clusterBaseIr;
    if (1 < instanceCount)
    {
        for (const Coordinate& coord : offsets)
        {
            const double distance = std::hypot(coord.y, coord.z);
            clusterIr += eachMass * MathUtil::pow2(distance);  // Java: Math.pow(distance, 2)
        }
    }

    const Coordinate clusterCM = m_transform.transform(clusterLocalCM);
    addMass(clusterCM);

    if (m_analysisMap != nullptr)
    {
        // The row of the motor's designation, made by its first cluster.
        const auto emplaced =
            m_analysisMap->try_emplace(CMAnalysisEntry::keyOf(motor), motorConfig.getMotor());
        CMAnalysisEntry& entry = emplaced.first->second;
        entry.updateEachMass(eachMass);
        entry.updateAverageCM(clusterCM);
    }

    addInertia(RigidBody{clusterCM, clusterIr, clusterIt, clusterIt});
    return *this;
}

MassCalculation& MassCalculation::calculateAssembly()
{
    if (includesStructure(m_type))
    {
        MassCalculation structureCalc = copy(*m_root, m_transform);
        structureCalc.calculateStructure();
        merge(structureCalc);
    }

    if (includesMotorCasing(m_type) || includesPropellant(m_type))
    {
        MassCalculation motorCalc = copy(*m_root, m_transform);
        motorCalc.calculateMotors();
        merge(motorCalc);
    }

    return *this;
}

MassCalculation& MassCalculation::calculateStructure()
{
    const RocketComponent&        component          = *m_root;
    const Transformation          parentTransform    = m_transform;
    const int                     instanceCount      = component.getInstanceCount();
    const std::vector<Coordinate> allInstanceOffsets = component.getInstanceLocations();
    const std::vector<double>     allInstanceAngles  = component.getInstanceAngles();
    const bool                    active             = m_config->isComponentActive(component);
    // Java indexes the arrays by instance number (an ArrayIndexOutOfBoundsException otherwise).
    QTROCKET_ASSERT(std::cmp_less_equal(instanceCount, allInstanceOffsets.size()) &&
                    std::cmp_less_equal(instanceCount, allInstanceAngles.size()));

    if (m_analysisMap != nullptr && active)
    {
        m_analysisMap->try_emplace(CMAnalysisEntry::keyOf(component), component);
    }

    // iterate over the aggregated instances for the whole tree.
    MassCalculation children = copy(component, parentTransform);
    for (int currentInstanceNumber = 0; currentInstanceNumber < instanceCount;
         ++currentInstanceNumber)
    {
        const auto           index = static_cast<std::size_t>(currentInstanceNumber);
        const Coordinate&    currentInstanceOffset = allInstanceOffsets[index];
        const Transformation offsetTransform = Transformation::translation(currentInstanceOffset);

        const double         currentInstanceAngle = allInstanceAngles[index];
        const Transformation angleTransform = Transformation::axialRotation(currentInstanceAngle);

        const Transformation currentTransform = parentTransform.applyTransformation(offsetTransform)
                                                    .applyTransformation(angleTransform);

        for (const RocketComponent* child : component.getChildren())
        {
            // child data, relative to rocket reference frame
            MassCalculation eachChild = copy(*child, currentTransform);
            eachChild.calculateStructure();

            // accumulate children's data
            children.merge(eachChild);
        }
    }

    if (active)
    {
        addComponentData(component, parentTransform, children);
    }

    merge(children);

    if (m_analysisMap != nullptr && active && isAssembly(component.kind()))
    {
        // Record the complete structural subtree after its child mass has been merged.
        analysisEntry(component).updateAssemblyMass(m_centerOfMass, component.getInstanceCount());
    }

    return *this;
}

void MassCalculation::addComponentData(const RocketComponent& component,
                                       const Transformation&  parentTransform,
                                       MassCalculation&       children)
{
    // mass data for *this component only* in the rocket-frame
    Coordinate compCM =
        parentTransform.transform(component.getComponentCG().add(component.getPosition()));

    // setting zero as the CG position means the top of the component, which is
    // component.getPosition()
    const Coordinate compZero = parentTransform.transform(component.getPosition());

    // Geometric (non-overridden) mass of this component, captured before a mass override
    // rewrites compCM's weight: a subcomponent mass override then rescales the inertia to the
    // overridden mass instead of leaving it at the geometric value.
    const double componentGeometricMass = compCM.weight;
    double       inertiaMass            = componentGeometricMass;

    if (component.isMassOverridden())
    {
        if (!component.isMassive())
        {
            compCM = children.getCM();
        }
        compCM = compCM.setWeight(component.getOverrideMass());

        if (component.isSubcomponentsOverriddenMass())
        {
            // The override replaces the mass of this component AND all of its subcomponents:
            // keep the geometric mass distribution but rescale it to the overridden total, so
            // that the moment of inertia stays consistent with the mass.
            const double geometricMass = componentGeometricMass + children.getMass();
            const double scale =
                geometricMass > kMinMass ? component.getOverrideMass() / geometricMass : 0.0;
            children.scaleInertia(scale);
            children.setCM(children.getCM().setWeight(0));
            inertiaMass = componentGeometricMass * scale;
        }
        else
        {
            // Override applies to this component only; its own inertia follows the overridden
            // mass.
            inertiaMass = component.getOverrideMass();
        }
    }

    if (component.isCGOverridden())
    {
        compCM = compCM.setX(compZero.x + component.getOverrideCGX());

        if (component.isSubcomponentsOverriddenCG())
        {
            children.setCM(children.getCM().setX(compCM.x));
        }
    }
    addMass(compCM);

    if (m_analysisMap != nullptr && !isAssembly(component.kind()))
    {
        // For physical components, record only the component and disregard its children.
        CMAnalysisEntry& entry = analysisEntry(component);
        entry.updateEachMass(compCM.weight);
        entry.updateAverageCM(compCM);
    }

    const double compIx = component.getRotationalUnitInertia() * inertiaMass;
    const double compIt = component.getLongitudinalUnitInertia() * inertiaMass;
    addInertia(RigidBody{compCM.setWeight(inertiaMass), compIx, compIt, compIt});
}

MassCalculation& MassCalculation::calculateMotors()
{
    if (!m_activeMotors)
    {
        for (const MotorConfiguration& motorConfig : m_config->getActiveMotors())
        {
            calculateMotorInstances(motorConfig, nullptr);
        }
    }
    else
    {
        for (const MotorClusterState* motorState : *m_activeMotors)
        {
            calculateMotorInstances(motorState->getConfig(), motorState);
        }
    }

    return *this;
}

void MassCalculation::calculateMotorInstances(const MotorConfiguration& motorConfig,
                                              const MotorClusterState*  motorState)
{
    const RocketComponent& mount = asComponent(motorConfig.getMount());
    for (const InstanceContext& context : m_config->getActiveInstances().getInstanceContexts(mount))
    {
        // calculateMountData aggregates all instances belonging to this parent.
        if (context.instanceNumber != 0)
        {
            continue;
        }

        MassCalculation motor = copy(mount, context.getParentTransform());
        motor.calculateMountData(motorConfig, motorState);
        merge(motor);
        updateAssemblyMotorMass(mount, motor.getCM());
    }
}

void MassCalculation::updateAssemblyMotorMass(const RocketComponent& mount,
                                              const Coordinate&      motorCM)
{
    if (m_analysisMap == nullptr || motorCM.weight <= kMinMass)
    {
        return;
    }

    for (const RocketComponent* component = mount.getParent(); component != nullptr;
         component                        = component->getParent())
    {
        if (isAssembly(component->kind()) && m_config->isComponentActive(*component))
        {
            const auto entry = m_analysisMap->find(CMAnalysisEntry::keyOf(*component));
            if (entry != m_analysisMap->end())
            {
                entry->second.updateAssemblyMass(motorCM, component->getInstanceCount());
            }
        }
    }
}

RigidBody MassCalculation::calculateMomentOfInertia() const
{
    double ir = 0;
    double it = 0;
    for (const RigidBody& eachLocal : m_bodies)
    {
        const RigidBody eachGlobal = eachLocal.rebase(m_centerOfMass);
        ir += eachGlobal.getIxx();
        it += eachGlobal.getIyy();
    }

    return RigidBody{m_centerOfMass, ir, it, it};
}

CMAnalysisEntry& MassCalculation::analysisEntry(const RocketComponent& component) const
{
    const auto entry = m_analysisMap->find(CMAnalysisEntry::keyOf(component));
    if (entry == m_analysisMap->end())
    {
        bug("no CM analysis entry for " + component.getName());
    }
    return entry->second;
}

// =================================================================================== object

bool MassCalculation::operator==(const MassCalculation& other) const noexcept
{
    if (this == &other)
    {
        return true;
    }
    return m_centerOfMass == other.m_centerOfMass && *m_config == *other.m_config &&
           m_simulationTime == other.m_simulationTime && m_type == other.m_type;
}

std::string MassCalculation::toCMDebug() const
{
    return std::format("cm= {}g@[{},{},{}]", Strings::formatFixed(m_centerOfMass.weight, 6),
                       Strings::formatFixed(m_centerOfMass.x, 6),
                       Strings::formatFixed(m_centerOfMass.y, 6),
                       Strings::formatFixed(m_centerOfMass.z, 6));
}

std::string_view name(MassCalculation::Type type) noexcept
{
    switch (type)
    {
        case MassCalculation::Type::STRUCTURE:
            return "STRUCTURE";
        case MassCalculation::Type::MOTOR:
            return "MOTOR";
        case MassCalculation::Type::BURNOUT:
            return "BURNOUT";
        case MassCalculation::Type::LAUNCH:
            break;
    }
    return "LAUNCH";
}

}  // namespace QtRocket
