#pragma once

#include <memory>

#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"

namespace QtRocket
{

class FlightEvent;
class SimulationStatus;

/// A simulation listener that ends the simulation when apogee is reached (OpenRocket's
/// simulation/listeners/system/ApogeeEndListener): when an APOGEE event is handled, it queues a
/// SIMULATION_END event at the current simulation time, straight into the event queue (no
/// listener is asked about it). A system listener.
///
/// The listener has no state. Java's INSTANCE is instance(): one object every simulation may
/// use, also from several threads.
class ApogeeEndListener final : public CloneableSimulationListener<ApogeeEndListener>
{
public:
    ApogeeEndListener() = default;

    /// The shared instance (Java: INSTANCE).
    [[nodiscard]] static std::shared_ptr<ApogeeEndListener> instance();

    /// Queues the end of the simulation for an APOGEE event. Always true.
    [[nodiscard]] bool handleFlightEvent(SimulationStatus&  status,
                                         const FlightEvent& event) override;

    /// Always true.
    [[nodiscard]] bool isSystemListener() const override;
};

}  // namespace QtRocket
