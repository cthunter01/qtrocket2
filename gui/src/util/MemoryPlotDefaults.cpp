#include "QtRocket/gui/util/MemoryPlotDefaults.h"

#include <memory>
#include <utility>

namespace QtRocket::Gui
{

std::shared_ptr<const SimulationPlotConfiguration> MemoryPlotDefaults::lastPlotted() const
{
    return m_lastPlotted;
}

void MemoryPlotDefaults::setLastPlotted(
    std::shared_ptr<const SimulationPlotConfiguration> configuration)
{
    m_lastPlotted = std::move(configuration);
}

}  // namespace QtRocket::Gui
