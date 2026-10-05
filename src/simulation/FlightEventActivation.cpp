#include "QtRocket/simulation/FlightEventActivation.h"

#include <memory>
#include <optional>

#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::FlightEventActivation
{

namespace
{

using DeployEvent     = DeploymentConfiguration::DeployEvent;
using SeparationEvent = StageSeparationConfiguration::SeparationEvent;
using Type            = FlightEvent::Type;

/// The source of @p event (Java dereferences getSource(): a NullPointerException without one).
[[nodiscard]] const RocketComponent& sourceOf(const FlightEvent& event)
{
    const RocketComponent* source = event.getSource();
    if (source == nullptr)
    {
        bug("The flight event has no source component");
    }
    return *source;
}

/// The altitudes of the ALTITUDE event @p event, which has data (Java casts getData() to a
/// Pair: a ClassCastException for anything else).
[[nodiscard]] FlightEvent::AltitudeChange altitudesOf(const FlightEvent& event)
{
    const std::optional<FlightEvent::AltitudeChange> altitude = event.getAltitudeChange();
    if (!altitude.has_value())
    {
        bug("The data of the altitude event is not an altitude change");
    }
    return *altitude;
}

/// Whether the source of @p event is in the stage right below the stage of @p targetComponent:
/// the test of IgnitionEvent's EJECTION_CHARGE and BURNOUT.
[[nodiscard]] bool isFromStageBelow(const FlightEvent&     event,
                                    const RocketComponent& targetComponent)
{
    const AxialStage& targetStage      = targetComponent.getStage();
    const AxialStage& eventStage       = sourceOf(event).getStage();
    const AxialStage* eventParentStage = eventStage.getUpperStage();
    return eventParentStage != nullptr && targetStage.equals(*eventParentStage);
}

/// DeployEvent.EJECTION.
[[nodiscard]] bool isEjectionOf(const FlightEvent& event, const RocketComponent& source)
{
    if (event.getType() != Type::EJECTION_CHARGE)
    {
        return false;
    }

    // An ejection charge only pressurizes the airframe assembly its motor sits in, so a motor
    // in a pod cannot deploy a recovery device in the parent airframe, and vice versa.
    if (const std::shared_ptr<MotorClusterState> motorState = event.getMotorState())
    {
        return asComponent(motorState->getMount()).getAssembly().equals(source.getAssembly());
    }

    // Fall back on the stage of the ejection charge if we don't know which motor fired it
    const RocketComponent& charge = sourceOf(event);
    return charge.getStageNumber() == source.getStageNumber();
}

/// Whether @p event is an ALTITUDE event with data that goes down through @p alt.
[[nodiscard]] bool descendsThrough(const FlightEvent& event, double alt)
{
    if (event.getType() != Type::ALTITUDE || !event.hasData())
    {
        return false;
    }
    const FlightEvent::AltitudeChange altitude = altitudesOf(event);
    return altitude.previous >= alt && altitude.current <= alt;
}

/// Whether @p event is an ALTITUDE event with data that goes up through @p alt.
[[nodiscard]] bool ascendsThrough(const FlightEvent& event, double alt)
{
    if (event.getType() != Type::ALTITUDE || !event.hasData())
    {
        return false;
    }
    const FlightEvent::AltitudeChange altitude = altitudesOf(event);
    return altitude.previous <= alt && altitude.current >= alt;
}

/// Whether @p event is of type @p type and its source has the stage number of @p stage plus
/// @p offset.
[[nodiscard]] bool isOfStage(const FlightEvent& event, Type type, const AxialStage& stage,
                             int offset)
{
    if (event.getType() != type)
    {
        return false;
    }
    const int ignition = sourceOf(event).getStageNumber();
    const int mount    = stage.getStageNumber();
    return mount == ignition + offset;
}

}  // namespace

bool isActivationEvent(IgnitionEvent ignitionEvent, const FlightConfiguration& config,
                       const FlightEvent& event, const RocketComponent& targetComponent)
{
    switch (ignitionEvent)
    {
        case IgnitionEvent::AUTOMATIC:
        {
            const AxialStage& targetStage = targetComponent.getStage();
            if (targetStage.isLaunchStage(config))
            {
                return isActivationEvent(IgnitionEvent::LAUNCH, config, event, targetComponent);
            }
            return isActivationEvent(IgnitionEvent::EJECTION_CHARGE, config, event,
                                     targetComponent);
        }
        case IgnitionEvent::LAUNCH:
            return event.getType() == Type::LAUNCH;
        case IgnitionEvent::EJECTION_CHARGE:
            return event.getType() == Type::EJECTION_CHARGE &&
                   isFromStageBelow(event, targetComponent);
        case IgnitionEvent::BURNOUT:
            return event.getType() == Type::BURNOUT && isFromStageBelow(event, targetComponent);
        case IgnitionEvent::NEVER:
            break;
    }
    // default behavior. Also for the NEVER case.
    return false;
}

bool isActivationEvent(DeployEvent deployEvent, const DeploymentConfiguration& config,
                       const FlightEvent& event, const RocketComponent& source)
{
    switch (deployEvent)
    {
        case DeployEvent::LAUNCH:
            return event.getType() == Type::LAUNCH;
        case DeployEvent::EJECTION:
            return isEjectionOf(event, source);
        case DeployEvent::APOGEE:
            return event.getType() == Type::APOGEE;
        case DeployEvent::ALTITUDE:
            return descendsThrough(event, config.getDeployAltitude());
        case DeployEvent::LOWER_STAGE_SEPARATION:
        {
            if (event.getType() != Type::STAGE_SEPARATION)
            {
                return false;
            }
            const int separation = sourceOf(event).getStageNumber();
            const int current    = source.getStageNumber();
            return current + 1 == separation;
        }
        case DeployEvent::NEVER:
            break;
    }
    return false;
}

bool isActivationEvent(const DeploymentConfiguration& config, const FlightEvent& event,
                       const RocketComponent& source)
{
    return isActivationEvent(config.getDeployEvent(), config, event, source);
}

bool isSeparationEvent(SeparationEvent separationEvent, const StageSeparationConfiguration& config,
                       const FlightEvent& event, const AxialStage& stage)
{
    switch (separationEvent)
    {
        case SeparationEvent::LAUNCH:
            return event.getType() == Type::LAUNCH;
        case SeparationEvent::IGNITION:
            return isOfStage(event, Type::IGNITION, stage, 0);
        case SeparationEvent::BURNOUT:
            return isOfStage(event, Type::BURNOUT, stage, 0);
        case SeparationEvent::EJECTION:
            return isOfStage(event, Type::EJECTION_CHARGE, stage, 0);
        case SeparationEvent::UPPER_IGNITION:
            return isOfStage(event, Type::IGNITION, stage, 1);
        case SeparationEvent::ALTITUDE_ASCENDING:
            return ascendsThrough(event, config.getSeparationAltitude());
        case SeparationEvent::APOGEE:
            return event.getType() == Type::APOGEE;
        case SeparationEvent::ALTITUDE_DESCENDING:
            return descendsThrough(event, config.getSeparationAltitude());
        case SeparationEvent::NEVER:
            break;
    }
    return false;
}

bool testForIgnition(const MotorClusterState& state, const FlightConfiguration& config,
                     const FlightEvent& event)
{
    const RocketComponent& mount = asComponent(state.getMount());
    return isActivationEvent(state.getIgnitionEvent(), config, event, mount);
}

}  // namespace QtRocket::FlightEventActivation
