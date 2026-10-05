#pragma once

#include <memory>

#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"

namespace QtRocket
{

class FlightEvent;
class SimulationStatus;

/// A simulation listener that ends the simulation when the ground is hit (OpenRocket's
/// simulation/listeners/system/GroundHitListener): when a GROUND_HIT event is handled, it
/// queues a SIMULATION_END event at the current simulation time, straight into the event queue
/// (no listener is asked about it). A system listener.
///
/// The listener has no state. Java's INSTANCE is instance(): one object every simulation may
/// use, also from several threads.
class GroundHitListener final : public CloneableSimulationListener<GroundHitListener>
{
public:
    GroundHitListener() = default;

    /// The shared instance (Java: INSTANCE).
    [[nodiscard]] static std::shared_ptr<GroundHitListener> instance();

    /// Queues the end of the simulation for a GROUND_HIT event. Always true.
    [[nodiscard]] bool handleFlightEvent(SimulationStatus&  status,
                                         const FlightEvent& event) override;

    /// Always true.
    [[nodiscard]] bool isSystemListener() const override;
};

}  // namespace QtRocket
