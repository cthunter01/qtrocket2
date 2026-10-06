#pragma once

#include <stop_token>

#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"

namespace QtRocket
{

class SimulationStatus;

/// A simulation listener that cancels the simulation on request (OpenRocket's
/// simulation/listeners/system/InterruptListener): after every step it looks whether a stop was
/// requested and, if so, throws a SimulationCancelledException ("The simulation was
/// interrupted."). A system listener.
///
/// Deviations from OpenRocket: Java's listener tests Thread.interrupted(), the interrupt flag of
/// the thread that runs the simulation, and so needs no state and has an INSTANCE. Here the
/// request comes through a std::stop_token, which the listener holds (its clones hold copies,
/// which watch the same stop source), so there is no shared instance: make one per run, from
/// the token of the std::stop_source (or std::jthread) that may cancel it. Java's test also
/// clears the flag; a stop request stays, so every later step throws again. A token that
/// cannot be stopped (a default-constructed one) never cancels.
class InterruptListener final : public CloneableSimulationListener<InterruptListener>
{
public:
    /// A listener that cancels the simulation when a stop is requested through @p stopToken.
    explicit InterruptListener(std::stop_token stopToken) noexcept;

    /// @throws SimulationCancelledException when a stop was requested
    void postStep(SimulationStatus& status) override;

    /// Always true.
    [[nodiscard]] bool isSystemListener() const override;

private:
    std::stop_token m_stopToken;
};

}  // namespace QtRocket
