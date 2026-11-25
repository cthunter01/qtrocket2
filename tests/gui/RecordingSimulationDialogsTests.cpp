#include "RecordingSimulationDialogs.h"

#include <memory>
#include <vector>

#include <QString>
#include <QStringList>
#include <QWidget>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/util/SimulationDialogs.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/Simulation.h"

// The tests' simulation dialogs: what the Simulations tab asks for is recorded, never shown, and
// the test edits as the user would while the settings dialog is open.

namespace QtRocket::Gui::Test
{

namespace
{

using RecordingSimulationDialogsTest = GuiTest;
using Kind                           = RecordingSimulationDialogs::Kind;
using Tab                            = SimulationDialogs::Tab;

/// The test's edit: renames the first simulation of the request to @p name.
struct RenameFirst
{
    const char* name;

    void operator()(QWidget* /*parent*/, OpenRocketDocument& /*document*/,
                    const SimulationDialogs::EditRequest& request) const
    {
        request.simulations.front()->setName(name);
    }
};

TEST_F(RecordingSimulationDialogsTest, RecordsTheSettingsDialogsAndEditsAsTheTestSays)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    InMemoryPreferences preferences;
    const auto          first = std::make_shared<Simulation>(document->getRocket(), preferences);
    first->setName("Simulation 1");
    const auto second = std::make_shared<Simulation>(document->getRocket(), preferences);
    second->setName("Simulation 2");
    QWidget parent;

    RecordingSimulationDialogs dialogs;
    dialogs.editSimulations(nullptr, *document,
                            {.simulations          = {first, second},
                             .tab                  = Tab::WARNINGS,
                             .newSimulation        = false,
                             .cancelBackgroundRuns = {},
                             .pauseBackgroundRuns  = {}});
    dialogs.whileEditing(RenameFirst{.name = "Renamed"});
    dialogs.editSimulations(&parent, *document,
                            {.simulations          = {second},
                             .tab                  = Tab::LAUNCH_CONDITIONS,
                             .newSimulation        = true,
                             .cancelBackgroundRuns = {},
                             .pauseBackgroundRuns  = {}});

    ASSERT_EQ(dialogs.requests().size(), 2U);
    const RecordingSimulationDialogs::Request& warnings = dialogs.requests().front();
    EXPECT_EQ(warnings.kind, Kind::EDIT);
    EXPECT_EQ(warnings.parent, nullptr);
    EXPECT_EQ(warnings.document, document.get());
    EXPECT_EQ(warnings.simulationNames,
              (QStringList{QStringLiteral("Simulation 1"), QStringLiteral("Simulation 2")}));
    EXPECT_EQ(warnings.tab, Tab::WARNINGS);
    EXPECT_FALSE(warnings.newSimulation);
    const RecordingSimulationDialogs::Request& added = dialogs.requests().back();
    EXPECT_EQ(added.parent, &parent);
    EXPECT_EQ(added.simulationNames, QStringList{QStringLiteral("Simulation 2")});
    EXPECT_TRUE(added.newSimulation);
    // The edit came after the request was recorded, as the user edits in the open dialog.
    EXPECT_EQ(second->getName(), "Renamed");
    EXPECT_EQ(first->getName(), "Simulation 1");
}

TEST_F(RecordingSimulationDialogsTest, RecordsThePlotDialogWithACopyOfItsConfiguration)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    InMemoryPreferences preferences;
    const auto simulation = std::make_shared<Simulation>(document->getRocket(), preferences);
    simulation->setName("Simulation 1");
    QWidget                     parent;
    SimulationPlotConfiguration configuration =
        SimulationPlotConfiguration::defaultConfigurations().at(3);

    RecordingSimulationDialogs dialogs;
    dialogs.plotSimulation(&parent, simulation, configuration);
    // The dialog keeps its own: a later change of the caller's is not recorded.
    configuration.addPlotDataType(FlightDataType::builtin(FlightDataTypeId::TYPE_TIME));

    ASSERT_EQ(dialogs.requests().size(), 1U);
    const RecordingSimulationDialogs::Request& plot = dialogs.requests().front();
    EXPECT_EQ(plot.kind, Kind::PLOT);
    EXPECT_EQ(plot.parent, &parent);
    EXPECT_EQ(plot.document, nullptr);
    EXPECT_EQ(plot.simulationNames, QStringList{QStringLiteral("Simulation 1")});
    EXPECT_EQ(plot.tab, Tab::PLOT_DATA);
    ASSERT_EQ(dialogs.plotConfigurations().size(), 1U);
    const SimulationPlotConfiguration& recorded = dialogs.plotConfigurations().front();
    EXPECT_EQ(recorded.getName(), "Ground track");
    EXPECT_EQ(recorded.getDataCount(), 2U);
    EXPECT_EQ(&recorded.getDomainAxisType(),
              &FlightDataType::builtin(FlightDataTypeId::TYPE_POSITION_X));
    // A settings dialog has no configuration.
    dialogs.editSimulations(nullptr, *document,
                            {.simulations          = {simulation},
                             .tab                  = Tab::PLOT_DATA,
                             .newSimulation        = false,
                             .cancelBackgroundRuns = {},
                             .pauseBackgroundRuns  = {}});
    EXPECT_EQ(dialogs.requests().size(), 2U);
    EXPECT_EQ(dialogs.plotConfigurations().size(), 1U);
}

}  // namespace

}  // namespace QtRocket::Gui::Test
