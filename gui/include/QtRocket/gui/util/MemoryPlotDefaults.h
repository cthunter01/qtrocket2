#pragma once

#include <memory>

#include "QtRocket/gui/util/PlotDefaults.h"

namespace QtRocket
{
class SimulationPlotConfiguration;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The PlotDefaults of the application and the tests: the last plotted configuration, kept in
/// memory for as long as the object lives (as OpenRocket's static, for the process; never stored
/// in the preferences, as OpenRocket stores it nowhere either).
class MemoryPlotDefaults final : public PlotDefaults
{
public:
    MemoryPlotDefaults()                                     = default;
    ~MemoryPlotDefaults() override                           = default;
    MemoryPlotDefaults(const MemoryPlotDefaults&)            = delete;
    MemoryPlotDefaults& operator=(const MemoryPlotDefaults&) = delete;
    MemoryPlotDefaults(MemoryPlotDefaults&&)                 = delete;
    MemoryPlotDefaults& operator=(MemoryPlotDefaults&&)      = delete;

    [[nodiscard]] std::shared_ptr<const SimulationPlotConfiguration> lastPlotted() const override;
    void setLastPlotted(std::shared_ptr<const SimulationPlotConfiguration> configuration) override;

private:
    std::shared_ptr<const SimulationPlotConfiguration> m_lastPlotted;
};

}  // namespace QtRocket::Gui
