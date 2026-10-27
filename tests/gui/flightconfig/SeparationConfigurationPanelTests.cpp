// SeparationConfigurationPanel: the Stages tab (OpenRocket's SeparationConfigurationPanel): its
// table and buttons (Reset separation named as Java's field, resetDeploymentButton), the buttons'
// start (disabled, as Java's) and enable rule, the popup, a double click, and its two operations
// over the selected cells.

#include "QtRocket/gui/flightconfig/SeparationConfigurationPanel.h"

#include <string>

#include <QAction>
#include <QModelIndex>
#include <QPushButton>
#include <QString>
#include <QTabWidget>
#include <QTableView>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/flightconfig/FlightConfigurableTableModel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationPanel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "ScriptedUserPrompts.h"
#include "flightconfig/FlightConfigurationTabSupport.h"

namespace QtRocket::Gui
{

namespace
{

using SeparationConfigurationPanelTest = Gui::Test::GuiTest;
using Tab                              = Gui::Test::FlightConfigurationTab;
using Design                           = Tab::Design;
using SeparationEvent                  = StageSeparationConfiguration::SeparationEvent;

/// The text of the cell (@p row, 1) of @p panel's table.
[[nodiscard]] std::string boosterCell(const SeparationConfigurationPanel& panel, int row)
{
    const FlightConfigurableTableModel& model = panel.tableModel();
    return QtStrings::toUtf8(model.data(model.index(row, 1)).toString());
}

TEST_F(SeparationConfigurationPanelTest, TheButtonsStartDisabledAndThenFollowTheSelection)
{
    const Tab                           tab(Design::TWO_STAGES);
    const SeparationConfigurationPanel& panel = tab.stages();
    EXPECT_EQ(panel.selectSeparationButton()->text(), QStringLiteral("Select separation"));
    EXPECT_EQ(panel.resetSeparationButton()->text(), QStringLiteral("Reset separation"));
    EXPECT_EQ(panel.selectSeparationButton()->objectName(),
              QStringLiteral("selectSeparationButton"));
    EXPECT_EQ(panel.resetSeparationButton()->objectName(), QStringLiteral("resetDeploymentButton"));
    EXPECT_EQ(Tab::lines(panel), "rows=[0] cols=[1]");
    EXPECT_FALSE(panel.selectSeparationAction()->isEnabled());
    tab.rocket().setSelectedConfiguration(tab.id(2));
    EXPECT_TRUE(panel.selectSeparationAction()->isEnabled());
    EXPECT_TRUE(panel.resetSeparationButton()->isEnabled());
    EXPECT_EQ(Tab::items(panel.fullPopupMenu()),
              "selectSeparationAction, resetSeparationAction, -, renameConfigAction, "
              "deleteConfigAction, duplicateConfigAction");
}

TEST_F(SeparationConfigurationPanelTest, ADoubleClickAsksForTheSeparationOfTheCell)
{
    Tab tab(Design::TWO_STAGES);
    tab.panel().tabs()->setCurrentIndex(FlightConfigurationPanel::kSeparationTabIndex);
    tab.show();
    const SeparationConfigurationPanel& panel = tab.stages();
    StageSeparationConfiguration        value;
    value.setSeparationEvent(SeparationEvent::BURNOUT);
    value.setSeparationDelay(1);
    tab.prompts().answerSeparation(UserPrompts::SeparationChoice{
        .scope = UserPrompts::ConfigurationScope::THIS_CONFIGURATION, .separation = value});
    Tab::doubleClick(panel, 2, 1);
    ASSERT_EQ(tab.prompts().separationRequests().size(), 1U);
    EXPECT_EQ(tab.prompts().separationRequests()[0].configurationName,
              QStringLiteral("[No motors]"));
    EXPECT_EQ(boosterCell(panel, 2), "Current stage motor burnout + 1 s");
    EXPECT_EQ(tab.document().getUndoDescription().value_or("-"), "Select separation");
    EXPECT_EQ(Tab::lines(panel), "rows=[2] cols=[1]");

    panel.resetSeparationButton()->click();
    EXPECT_EQ(boosterCell(panel, 2), "Current stage ejection charge");
    EXPECT_EQ(tab.document().getUndoDescription().value_or("-"), "Reset separation");
}

}  // namespace

}  // namespace QtRocket::Gui
