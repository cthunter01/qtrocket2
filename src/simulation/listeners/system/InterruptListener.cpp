#include "QtRocket/simulation/listeners/system/InterruptListener.h"

#include <stop_token>
#include <utility>

#include "QtRocket/simulation/exception/SimulationCancelledException.h"

namespace QtRocket
{

InterruptListener::InterruptListener(std::stop_token stopToken) noexcept
  : m_stopToken(std::move(stopToken))
{
}

void InterruptListener::postStep(SimulationStatus& /*status*/)
{
    if (m_stopToken.stop_requested())
    {
        throw SimulationCancelledException("The simulation was interrupted.");
    }
}

bool InterruptListener::isSystemListener() const
{
    return true;
}

}  // namespace QtRocket
