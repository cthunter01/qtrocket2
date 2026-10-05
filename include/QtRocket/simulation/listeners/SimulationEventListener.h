#pragma once

namespace QtRocket
{

class FlightEvent;
class MotorClusterState;
class MotorConfigurationId;
class MotorMount;
class RecoveryDevice;
class SimulationStatus;

/// The hooks of a simulation listener for the flight events (OpenRocket's
/// simulation/listeners/SimulationEventListener). A listener that also implements this
/// interface is asked before an event is queued, before it is handled, before a motor ignites
/// and before a recovery device deploys, and may veto each.
///
/// As in Java, this interface does not derive from SimulationListener: the simulation holds
/// SimulationListeners and asks each whether it is a SimulationEventListener too (a
/// dynamic_cast across the hierarchy), so a class implements both (AbstractSimulationListener
/// does).
///
/// Deviations from OpenRocket: the status, the mount, the motor state and the recovery device
/// are passed by reference; the mount and the device are const, because an event names its
/// source by a pointer to const (see FlightEvent). A listener that wants to change the rocket
/// reaches it through the status.
class SimulationEventListener
{
public:
    virtual ~SimulationEventListener() = default;

    SimulationEventListener& operator=(const SimulationEventListener&) = delete;
    SimulationEventListener& operator=(SimulationEventListener&&)      = delete;

    /// Called before @p event is added to the event queue. True to add it, false to drop it.
    /// @throws SimulationException to stop the simulation
    [[nodiscard]] virtual bool addFlightEvent(SimulationStatus&  status,
                                              const FlightEvent& event) = 0;

    /// Called before @p event is handled. True to handle it, false to skip the handling.
    /// @throws SimulationException to stop the simulation
    [[nodiscard]] virtual bool handleFlightEvent(SimulationStatus&  status,
                                                 const FlightEvent& event) = 0;

    /// Called before a motor ignites: the motor @p motorId in @p mount, whose state is
    /// @p instance. True to ignite it, false to abort the ignition.
    /// @throws SimulationException to stop the simulation
    [[nodiscard]] virtual bool motorIgnition(SimulationStatus&           status,
                                             const MotorConfigurationId& motorId,
                                             const MotorMount&           mount,
                                             MotorClusterState&          instance) = 0;

    /// Called before @p recoveryDevice deploys. True to deploy it, false to abort the
    /// deployment.
    /// @throws SimulationException to stop the simulation
    [[nodiscard]] virtual bool recoveryDeviceDeployment(SimulationStatus&     status,
                                                        const RecoveryDevice& recoveryDevice) = 0;

protected:
    SimulationEventListener()                               = default;
    SimulationEventListener(const SimulationEventListener&) = default;
    SimulationEventListener(SimulationEventListener&&)      = default;
};

}  // namespace QtRocket
