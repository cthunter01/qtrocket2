#pragma once

#include <memory>

#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"

namespace QtRocket
{

class RecoveryDevice;
class SimulationStatus;

/// A simulation listener that ends the simulation when a recovery device deploys (OpenRocket's
/// simulation/listeners/system/RecoveryDeviceDeploymentEndListener): before a device deploys,
/// it queues a SIMULATION_END event at the current simulation time, straight into the event
/// queue (no listener is asked about it), and lets the deployment go on. A system listener.
///
/// The listener has no state. Java's INSTANCE is instance(): one object every simulation may
/// use, also from several threads.
class RecoveryDeviceDeploymentEndListener final
  : public CloneableSimulationListener<RecoveryDeviceDeploymentEndListener>
{
public:
    RecoveryDeviceDeploymentEndListener() = default;

    /// The shared instance (Java: INSTANCE).
    [[nodiscard]] static std::shared_ptr<RecoveryDeviceDeploymentEndListener> instance();

    /// Queues the end of the simulation. Always true.
    [[nodiscard]] bool recoveryDeviceDeployment(SimulationStatus&     status,
                                                const RecoveryDevice& recoveryDevice) override;

    /// Always true.
    [[nodiscard]] bool isSystemListener() const override;
};

}  // namespace QtRocket
