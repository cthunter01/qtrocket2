#pragma once

#include "QtRocket/motor/chooser/ThrustCurveMotorSelection.h"

namespace QtRocket
{

class FlightConfigurationId;
class MotorMount;

/// What the motor chooser reads off a motor mount when it is opened for one of its motor
/// configurations (OpenRocket's ThrustCurveMotorSelectionPanel.setMotorMountAndConfig(fcid, mount)
/// and MotorRowFilter.setMotorMount(mount), which read the mount themselves). The chooser's logic
/// lives in motor/, below rocket/ in the core's layers, so it takes the data
/// (ThrustCurveMotorSelection::MountConfiguration); this helper sits above both.
namespace MotorChooserInput
{

/// The motor configuration of @p mount in the flight configuration @p fcid (its override, else
/// the mount's default: MotorMount::getMotorConfig()), with its motor, ejection delay and nozzle
/// exit diameter, and the motor of every motor configuration of @p mount that has one, the default
/// first (mount.getMotorIterator(), which iterates the configurations' map). A motor that is not a
/// ThrustCurveMotor counts as none (OpenRocket casts it: a ClassCastException; QtRocket has no
/// other kind of motor). Nothing of @p mount is kept but the motors.
[[nodiscard]] ThrustCurveMotorSelection::MountConfiguration fromMount(
    const MotorMount& mount, const FlightConfigurationId& fcid);

}  // namespace MotorChooserInput

}  // namespace QtRocket
