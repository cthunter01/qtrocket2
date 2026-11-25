#include "PlotDialogTestSupport.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <QPoint>
#include <QString>

#include "QtRocket/gui/plot/DataPlot.h"
#include "QtRocket/gui/plot/SimulationPlot.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/plot/SimulationPlotModel.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// PlotDialogProbe's abort time: 60 % of the branch's last time.
constexpr double kAbortTimeShare = 0.6;

}  // namespace

SimulationPlotConfiguration presetConfiguration(std::size_t preset)
{
    SimulationPlotConfiguration configuration =
        SimulationPlotConfiguration::defaultConfigurations().at(preset);
    configuration.resetUnits();
    return configuration;
}

SimulationPlotModel presetPlotModel(const Simulation& simulation, std::size_t preset)
{
    return presetPlotModelOf(simulation.getName(), simulation.getSimulatedData(), preset);
}

SimulationPlotModel presetPlotModelOf(std::string name, std::shared_ptr<const FlightData> data,
                                      std::size_t preset)
{
    return {std::move(name), std::move(data), presetConfiguration(preset),
            SimulationPlot::translator()};
}

std::shared_ptr<const FlightData> withAborts(const FlightData&            data,
                                             std::span<const std::size_t> aborted)
{
    std::vector<std::shared_ptr<FlightDataBranch>> copies;
    const FlightDataType& time = FlightDataType::builtin(FlightDataTypeId::TYPE_TIME);
    for (std::size_t b = 0; b < data.getBranchCount(); ++b)
    {
        auto copy = std::make_shared<FlightDataBranch>(data.getBranch(b).clone());
        if (std::ranges::find(aborted, b) != aborted.end())
        {
            const SimulationAbort::Cause cause =
                SimulationAbort::kAllCauses.at(b % SimulationAbort::kAllCauses.size());
            copy->addEvent(FlightEvent(FlightEvent::Type::SIM_ABORT,
                                       copy->getLast(time) * kAbortTimeShare, nullptr,
                                       SimulationAbort(cause)));
        }
        copies.push_back(std::move(copy));
    }
    return std::make_shared<const FlightData>(copies);
}

DataPlot::ReadoutHook silentReadout()
{
    return [](const QPoint& /*globalPosition*/, const QString& /*text*/) { };
}

}  // namespace QtRocket::Gui::Test
