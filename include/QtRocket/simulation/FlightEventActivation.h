#pragma once

#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"

namespace QtRocket
{

class AxialStage;
class FlightConfiguration;
class FlightEvent;
class MotorClusterState;
class RocketComponent;

/// Whether a flight event sets something off: the tests the simulation engine applies to every
/// event it handles, to find the motors that ignite on it, the recovery devices that deploy on
/// it and the stages that separate on it. In OpenRocket they are methods of the enums
/// IgnitionEvent, DeploymentConfiguration.DeployEvent and
/// StageSeparationConfiguration.SeparationEvent and of MotorClusterState; they are free
/// functions here because those types live in motor/, rocket/ and mass/, below simulation/,
/// where FlightEvent is.
///
/// Stages are compared as Java compares them: by stage number where Java compares numbers, and
/// with RocketComponent::equals() (the same class and id) where Java calls equals(), so an
/// event whose source is in another copy of the rocket (the same ids) is matched as well.
///
/// Deviations from OpenRocket:
/// - BugError where Java throws: an event without a source where the test needs one
///   (NullPointerException), a component or source that is in no stage or in no assembly
///   (IllegalStateException), an ALTITUDE event whose data is not an altitude change
///   (ClassCastException).
/// - IgnitionEvent::AUTOMATIC takes a target in a rocket without an active core stage as "not
///   in the launch stage" (Java: NullPointerException; see AxialStage::isLaunchStage()).
namespace FlightEventActivation
{

/// Whether @p event ignites a motor in the mount @p targetComponent whose ignition is set to
/// @p ignitionEvent, in the flight configuration @p config (Java:
/// IgnitionEvent.isActivationEvent(config, event, source)):
/// - AUTOMATIC: as LAUNCH when the target's stage is the launch stage of @p config
///   (AxialStage::isLaunchStage()), else as EJECTION_CHARGE;
/// - LAUNCH: a LAUNCH event;
/// - EJECTION_CHARGE, BURNOUT: an event of that type whose source is in the stage right below
///   the target's stage: the upper stage (AxialStage::getUpperStage()) of the source's stage
///   equals the target's stage;
/// - NEVER: never.
/// @throws BugError see the namespace comment
[[nodiscard]] bool isActivationEvent(IgnitionEvent ignitionEvent, const FlightConfiguration& config,
                                     const FlightEvent&     event,
                                     const RocketComponent& targetComponent);

/// Whether @p event deploys the recovery device @p source when its deployment is set to
/// @p deployEvent with the settings @p config (Java: DeployEvent.isActivationEvent(config, e,
/// source)):
/// - LAUNCH: a LAUNCH event;
/// - EJECTION: an EJECTION_CHARGE event. When the event carries the state of the motor that
///   fired: only when that motor's mount is in the same assembly as the device (the innermost
///   stage or pod set of each, compared with equals()), because a charge pressurises only the
///   airframe it sits in. Otherwise: when the event's source has the device's stage number;
/// - APOGEE: an APOGEE event;
/// - ALTITUDE: an ALTITUDE event with data whose previous altitude is at or above the deploy
///   altitude and whose current altitude is at or below it; never without data;
/// - LOWER_STAGE_SEPARATION: a STAGE_SEPARATION event whose source's stage number is the
///   device's stage number + 1;
/// - NEVER: never.
/// @throws BugError see the namespace comment
[[nodiscard]] bool isActivationEvent(DeploymentConfiguration::DeployEvent deployEvent,
                                     const DeploymentConfiguration&       config,
                                     const FlightEvent& event, const RocketComponent& source);

/// isActivationEvent(config.getDeployEvent(), config, event, source) (Java:
/// DeploymentConfiguration.isActivationEvent(e, source)).
[[nodiscard]] bool isActivationEvent(const DeploymentConfiguration& config,
                                     const FlightEvent& event, const RocketComponent& source);

/// Whether @p event separates @p stage when its separation is set to @p separationEvent with
/// the settings @p config (Java: SeparationEvent.isSeparationEvent(config, e, stage)):
/// - LAUNCH: a LAUNCH event;
/// - IGNITION, BURNOUT, EJECTION: an IGNITION, BURNOUT or EJECTION_CHARGE event whose source
///   has the stage's stage number;
/// - UPPER_IGNITION: an IGNITION event whose source's stage number + 1 is the stage's;
/// - ALTITUDE_ASCENDING: an ALTITUDE event with data whose previous altitude is at or below
///   the separation altitude and whose current altitude is at or above it; never without data;
/// - APOGEE: an APOGEE event;
/// - ALTITUDE_DESCENDING: an ALTITUDE event with data whose previous altitude is at or above
///   the separation altitude and whose current altitude is at or below it; never without data;
/// - NEVER: never.
/// @throws BugError see the namespace comment
[[nodiscard]] bool isSeparationEvent(StageSeparationConfiguration::SeparationEvent separationEvent,
                                     const StageSeparationConfiguration&           config,
                                     const FlightEvent& event, const AxialStage& stage);

/// Whether @p event ignites the motors of @p state in @p config (Java:
/// MotorClusterState.testForIgnition(config, event)): isActivationEvent() of the state's
/// ignition event with the state's mount as the target.
/// @throws BugError see the namespace comment
[[nodiscard]] bool testForIgnition(const MotorClusterState&   state,
                                   const FlightConfiguration& config, const FlightEvent& event);

}  // namespace FlightEventActivation

}  // namespace QtRocket
