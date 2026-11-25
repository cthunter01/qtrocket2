#include "RecordingSimulationDialogs.h"

#include <memory>

#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/SimulationDialogs.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/simulation/Simulation.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// The name of @p simulation, or "<none>" for none.
[[nodiscard]] QString nameOf(const Simulation* simulation)
{
    return simulation == nullptr ? QStringLiteral("<none>")
                                 : QtStrings::fromUtf8(simulation->getName());
}

}  // namespace

void RecordingSimulationDialogs::editSimulations(QWidget* parent, OpenRocketDocument& document,
                                                 const EditRequest& request)
{
    QStringList names;
    for (const std::shared_ptr<Simulation>& simulation : request.simulations)
    {
        names.append(nameOf(simulation.get()));
    }
    m_requests.push_back({.kind            = Kind::EDIT,
                          .parent          = parent,
                          .document        = &document,
                          .simulationNames = names,
                          .tab             = request.tab,
                          .newSimulation   = request.newSimulation});
    if (m_edit)
    {
        // A copy: the edit may replace itself.
        const Edit edit = m_edit;
        edit(parent, document, request);
    }
}

void RecordingSimulationDialogs::plotSimulation(QWidget*                           parent,
                                                std::shared_ptr<Simulation>        simulation,
                                                const SimulationPlotConfiguration& configuration)
{
    m_requests.push_back({.kind            = Kind::PLOT,
                          .parent          = parent,
                          .document        = nullptr,
                          .simulationNames = {nameOf(simulation.get())},
                          .tab             = Tab::PLOT_DATA,
                          .newSimulation   = false});
    m_plotConfigurations.push_back(configuration);
}

}  // namespace QtRocket::Gui::Test
