#pragma once

#include <memory>

#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"

namespace QtRocket
{

class FlightEvent;
class RecoveryDevice;
class SimulationStatus;

/// A simulation listener which ignores recovery deployment events and ends the simulation when
/// apogee is reached (OpenRocket's simulation/listeners/system/OptimumCoastListener). The
/// engine runs a simulation with it to find the altitude the rocket would coast to when a
/// recovery device opens before apogee. When an APOGEE event is handled, it queues a
/// SIMULATION_END event at the current simulation time, straight into the event queue; every
/// recovery device deployment is refused. A system listener.
///
/// The listener has no state. Java's INSTANCE is instance(): one object every simulation may
/// use, also from several threads.
class OptimumCoastListener final : public CloneableSimulationListener<OptimumCoastListener>
{
public:
    OptimumCoastListener() = default;

    /// The shared instance (Java: INSTANCE).
    [[nodiscard]] static std::shared_ptr<OptimumCoastListener> instance();

    /// Queues the end of the simulation for an APOGEE event. Always true.
    [[nodiscard]] bool handleFlightEvent(SimulationStatus&  status,
                                         const FlightEvent& event) override;

    /// Always false: no recovery device deploys.
    [[nodiscard]] bool recoveryDeviceDeployment(SimulationStatus&     status,
                                                const RecoveryDevice& recoveryDevice) override;

    /// Always true.
    [[nodiscard]] bool isSystemListener() const override;
};

}  // namespace QtRocket
