#include "QtRocket/simulation/SimulationBatchCompletion.h"

#include <algorithm>
#include <cstddef>
#include <optional>

#include "QtRocket/util/BugError.h"

namespace QtRocket
{

SimulationBatchCompletion::SimulationBatchCompletion(std::size_t count) : m_completed(count, false)
{
}

bool SimulationBatchCompletion::markSimulationComplete(std::size_t index)
{
    if (isSimulationComplete(index))
    {
        return false;
    }
    m_completed[index] = true;
    return isBatchComplete();
}

bool SimulationBatchCompletion::isSimulationComplete(std::size_t index) const
{
    if (index >= m_completed.size())
    {
        bug("The simulation is not one of the batch");
    }
    return m_completed[index];
}

bool SimulationBatchCompletion::isBatchComplete() const noexcept
{
    return std::ranges::all_of(m_completed, [](bool completed) { return completed; });
}

std::optional<std::size_t> SimulationBatchCompletion::getFirstIncomplete() const noexcept
{
    for (std::size_t index = 0; index < m_completed.size(); index++)
    {
        if (!m_completed[index])
        {
            return index;
        }
    }
    return std::nullopt;
}

}  // namespace QtRocket
