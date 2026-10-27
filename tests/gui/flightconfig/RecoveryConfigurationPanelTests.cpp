// RecoveryConfigurationPanel: the Recovery tab (OpenRocket's RecoveryConfigurationPanel): its
// table and buttons, the buttons' start (disabled, as Java's) and enable rule, the popup, Enter,
// and its two operations over the selected cells.

#include "QtRocket/gui/flightconfig/RecoveryConfigurationPanel.h"

#include <string>

#include <QAction>
#include <QModelIndex>
#include <QPushButton>
#include <QString>
#include <QTabWidget>
#include <QTableView>
#include <QTest>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/flightconfig/FlightConfigurableTableModel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationPanel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "ScriptedUserPrompts.h"
#include "flightconfig/FlightConfigurationTabSupport.h"

namespace QtRocket::Gui
{

namespace
{

using RecoveryConfigurationPanelTest = Gui::Test::GuiTest;
using Tab                            = Gui::Test::FlightConfigurationTab;
using Design                         = Tab::Design;
using DeployEvent                    = DeploymentConfiguration::DeployEvent;

/// The text of the cell (@p row, 1) of @p panel's table.
[[nodiscard]] std::string chuteCell(const RecoveryConfigurationPanel& panel, int row)
{
    const FlightConfigurableTableModel& model = panel.tableModel();
    return QtStrings::toUtf8(model.data(model.index(row, 1)).toString());
}

/// A deployment answer for this configuration only.
[[nodiscard]] UserPrompts::DeploymentChoice thisConfiguration(DeployEvent event, double altitude,
                                                              double delay)
{
    DeploymentConfiguration value;
    value.setDeployEvent(event);
    value.setDeployAltitude(altitude);
    value.setDeployDelay(delay);
    return {.scope = UserPrompts::ConfigurationScope::THIS_CONFIGURATION, .deployment = value};
}

TEST_F(RecoveryConfigurationPanelTest, TheButtonsStartDisabledAndThenFollowTheSelection)
{
    const Tab                         tab(Design::TWO_STAGES);
    const RecoveryConfigurationPanel& panel = tab.recovery();
    EXPECT_EQ(panel.selectDeploymentButton()->text(), QStringLiteral("Select deployment"));
    EXPECT_EQ(panel.resetDeploymentButton()->text(), QStringLiteral("Reset deployment"));
    EXPECT_EQ(panel.selectDeploymentButton()->objectName(),
              QStringLiteral("selectDeploymentButton"));
    EXPECT_EQ(panel.resetDeploymentButton()->objectName(), QStringLiteral("resetDeploymentButton"));
    EXPECT_EQ(panel.selectDeploymentButton()->minimumWidth(),
              panel.resetDeploymentButton()->minimumWidth());
    // Java: a device's cell selected, the buttons still disabled (TabProbe's "initial").
    EXPECT_EQ(Tab::lines(panel), "rows=[0] cols=[1]");
    EXPECT_FALSE(panel.selectDeploymentAction()->isEnabled());
    EXPECT_FALSE(panel.resetDeploymentAction()->isEnabled());

    // The rocket's selection moves the table's: the buttons follow.
    tab.rocket().setSelectedConfiguration(tab.id(1));
    EXPECT_EQ(Tab::lines(panel), "rows=[1] cols=[1]");
    EXPECT_TRUE(panel.selectDeploymentAction()->isEnabled());
    EXPECT_TRUE(panel.resetDeploymentButton()->isEnabled());
    EXPECT_EQ(Tab::items(panel.fullPopupMenu()),
              "selectDeploymentAction, resetDeploymentAction, -, renameConfigAction, "
              "deleteConfigAction, duplicateConfigAction");
}

TEST_F(RecoveryConfigurationPanelTest, EnterAsksForTheDeploymentOfTheFirstCell)
{
    Tab tab(Design::TWO_STAGES);
    tab.panel().tabs()->setCurrentIndex(FlightConfigurationPanel::kRecoveryTabIndex);
    tab.show();
    const RecoveryConfigurationPanel& panel = tab.recovery();
    Tab::click(panel, 1, 1);
    Tab::click(panel, 2, 1, Qt::ShiftModifier);
    tab.prompts().answerDeployment(thisConfiguration(DeployEvent::ALTITUDE, 150, 1.5));
    QTest::keyClick(&panel.table(), Qt::Key_Return);
    ASSERT_EQ(tab.prompts().deploymentRequests().size(), 1U);
    EXPECT_EQ(tab.prompts().deploymentRequests()[0].configurationName,
              QStringLiteral("[C6-5; None]"));
    EXPECT_EQ(chuteCell(panel, 1), "Altitude 150 m + 1.5 s");
    EXPECT_EQ(chuteCell(panel, 2), "Altitude 150 m + 1.5 s");
    EXPECT_EQ(chuteCell(panel, 0), "Ejection charge");
    EXPECT_EQ(tab.document().getUndoDescription().value_or("-"), "Select deployment");
    EXPECT_EQ(Tab::lines(panel), "rows=[1] cols=[1]");

    // Reset deployment: the overrides go.
    panel.resetDeploymentButton()->click();
    EXPECT_EQ(chuteCell(panel, 1), "Ejection charge");
    EXPECT_EQ(tab.document().getUndoDescription().value_or("-"), "Reset deployment");
}

}  // namespace

}  // namespace QtRocket::Gui
