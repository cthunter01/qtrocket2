#include "QtRocket/simulation/listeners/RunSampleListener.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/RunSample.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

/// What a listener and its clones share.
struct RunSampleListener::State
{
    explicit State(Clock timeSource) : clock(std::move(timeSource)) { }

    /// Never empty; called on the simulation's thread only.
    const Clock clock;

    std::mutex mutex;
    /// The samples not taken yet, in order (guarded by mutex).
    std::vector<RunSample> samples;
    /// When a step last published a sample; nullopt before the first (Java's field starts at 0,
    /// so its first step publishes). Guarded by mutex.
    std::optional<std::chrono::steady_clock::time_point> lastStepSample;
};

namespace
{

/// @p clock, which must not be empty.
[[nodiscard]] RunSampleListener::Clock requireClock(RunSampleListener::Clock clock)
{
    if (clock == nullptr)
    {
        bug("A run sample listener needs a clock");
    }
    return clock;
}

}  // namespace

RunSampleListener::RunSampleListener()
  : RunSampleListener([] { return std::chrono::steady_clock::now(); })
{
}

RunSampleListener::RunSampleListener(Clock clock)
  : m_state(std::make_shared<State>(requireClock(std::move(clock))))
{
}

bool RunSampleListener::handleFlightEvent(SimulationStatus& status, const FlightEvent& event)
{
    switch (event.getType())
    {
        case FlightEvent::Type::APOGEE:
        case FlightEvent::Type::LAUNCH:
        case FlightEvent::Type::SIMULATION_END:
            publish(status);
            break;
        default:
            break;
    }
    return true;
}

void RunSampleListener::postStep(SimulationStatus& status)
{
    // Java: if (System.currentTimeMillis() >= time + UPDATE_MS) { time = ...; publish(...); },
    // with one reading of the clock.
    const std::chrono::steady_clock::time_point now = m_state->clock();
    const std::scoped_lock                      lock(m_state->mutex);
    if (m_state->lastStepSample.has_value() && now < *m_state->lastStepSample + kUpdateInterval)
    {
        return;
    }
    m_state->lastStepSample = now;
    m_state->samples.push_back(RunSample::of(status));
}

std::vector<RunSample> RunSampleListener::takeSamples()
{
    std::vector<RunSample> taken;
    const std::scoped_lock lock(m_state->mutex);
    taken.swap(m_state->samples);
    return taken;
}

void RunSampleListener::publish(const SimulationStatus& status)
{
    const RunSample        sample = RunSample::of(status);
    const std::scoped_lock lock(m_state->mutex);
    m_state->samples.push_back(sample);
}

}  // namespace QtRocket
