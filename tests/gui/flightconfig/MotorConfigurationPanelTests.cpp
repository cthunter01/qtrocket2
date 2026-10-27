// MotorConfigurationPanel: the Motors tab (OpenRocket's MotorConfigurationPanel): the box "Motor
// mounts", the help card and the box "Motor configurations" with its table and buttons, the
// buttons' enable rule, the popup, and the four operations over the selected cells.

#include "QtRocket/gui/flightconfig/MotorConfigurationPanel.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <QAction>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QString>
#include <QTableView>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/flightconfig/FlightConfigurableTableModel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/MotorCellDelegate.h"
#include "QtRocket/gui/flightconfig/MotorMountConfigurationPanel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "ScriptedUserPrompts.h"
#include "flightconfig/FlightConfigurationTabSupport.h"

namespace QtRocket::Gui
{

namespace
{

using MotorConfigurationPanelTest = Gui::Test::GuiTest;
using Tab                         = Gui::Test::FlightConfigurationTab;
using Design                      = Tab::Design;

/// The motor and ignition texts of the cell (@p row, @p column) of @p panel's table, "C6-5 |
/// Default (Automatic)".
[[nodiscard]] std::string cell(const MotorConfigurationPanel& panel, int row, int column)
{
    const FlightConfigurableTableModel& model = panel.tableModel();
    const QModelIndex                   index = model.index(row, column);
    return QtStrings::toUtf8(model.data(index).toString()) + " | " +
           QtStrings::toUtf8(
               model.data(index, FlightConfigurableTableModel::kIgnitionTextRole).toString());
}

/// The enabled ones of @p panel's four actions, "select delete ignition reset".
[[nodiscard]] std::string enabledActions(const MotorConfigurationPanel& panel)
{
    std::string text;
    for (const auto& [action, name] : {std::pair{panel.selectMotorAction(), "select"},
                                       std::pair{panel.deleteMotorAction(), "delete"},
                                       std::pair{panel.selectIgnitionAction(), "ignition"},
                                       std::pair{panel.resetIgnitionAction(), "reset"}})
    {
        if (action->isEnabled())
        {
            text += (text.empty() ? "" : " ") + std::string(name);
        }
    }
    return text;
}

// =========================================================================== the widgets

TEST_F(MotorConfigurationPanelTest, ItHasTheMountsBoxTheCardsAndJavasButtons)
{
    const Tab                      tab(Design::TWO_STAGES);
    const MotorConfigurationPanel& panel = tab.motors();
    EXPECT_EQ(panel.motorMountsBox()->title(), QStringLiteral("Motor mounts"));
    EXPECT_TRUE(panel.motorMountsBox()->font().bold());
    EXPECT_FALSE(panel.motorMountConfigurationPanel().font().bold());
    EXPECT_EQ(panel.configurationBox()->title(), QStringLiteral("Motor configurations"));
    EXPECT_TRUE(panel.configurationBox()->font().bold());
    EXPECT_FALSE(panel.table().font().bold());
    EXPECT_EQ(panel.table().itemDelegate(), &panel.cellDelegate());
    EXPECT_FALSE(panel.isShowingHelp());
    EXPECT_EQ(panel.cards()->currentWidget(), panel.configurationBox());

    EXPECT_EQ(panel.selectMotorButton()->text(), QStringLiteral("Select motor"));
    EXPECT_EQ(panel.deleteMotorButton()->text(), QStringLiteral("Delete motor"));
    EXPECT_EQ(panel.selectIgnitionButton()->text(), QStringLiteral("Select ignition"));
    EXPECT_EQ(panel.resetIgnitionButton()->text(), QStringLiteral("Reset ignition"));
    EXPECT_EQ(panel.selectMotorButton()->objectName(), QStringLiteral("selectMotorButton"));
    EXPECT_EQ(panel.resetIgnitionButton()->objectName(), QStringLiteral("resetIgnitionButton"));
    // One width ("sizegroup button").
    EXPECT_EQ(panel.selectMotorButton()->minimumWidth(),
              panel.resetIgnitionButton()->minimumWidth());
    EXPECT_EQ(panel.deleteMotorButton()->minimumWidth(),
              panel.selectIgnitionButton()->minimumWidth());
    EXPECT_GE(panel.selectMotorButton()->minimumWidth(),
              panel.selectIgnitionButton()->sizeHint().width());
}

TEST_F(MotorConfigurationPanelTest, TheHelpShowsWhileTheTableHasNoMountColumn)
{
    const Tab                      tab(Design::NEW);
    const MotorConfigurationPanel& panel = tab.motors();
    EXPECT_TRUE(panel.isShowingHelp());
    EXPECT_EQ(panel.helpText()->objectName(), QStringLiteral("helpText"));
    EXPECT_EQ(panel.helpText()->text(),
              QStringLiteral("<html>No motor mounts defined.<br>Add a tube in the 'Rocket design' "
                             "tab and select it from the list on the left.</html>"));
    EXPECT_EQ(panel.helpText()->textFormat(), Qt::RichText);
    EXPECT_FALSE(panel.selectMotorAction()->isEnabled());

    auto& body = tab.rocket().getChild(0).addChild(std::make_unique<BodyTube>());
    EXPECT_TRUE(panel.isShowingHelp());
    body.setMotorMount(true);
    EXPECT_FALSE(panel.isShowingHelp());
    // No configuration yet: nothing selected, the buttons off.
    EXPECT_FALSE(panel.selectMotorAction()->isEnabled());
    body.setMotorMount(false);
    EXPECT_TRUE(panel.isShowingHelp());
}

TEST_F(MotorConfigurationPanelTest, TheButtonsActWhileTheFirstSelectedCellIsAMounts)
{
    const Tab tab(Design::TWO_STAGES);
    tab.show();
    const MotorConfigurationPanel& panel = tab.motors();
    EXPECT_EQ(enabledActions(panel), "select delete ignition reset");
    EXPECT_TRUE(panel.selectMotorButton()->isEnabled());
    Tab::click(panel, 1, 0);
    EXPECT_EQ(enabledActions(panel), "");
    EXPECT_FALSE(panel.selectMotorButton()->isEnabled());
    Tab::click(panel, 1, 2);
    EXPECT_EQ(enabledActions(panel), "select delete ignition reset");
    EXPECT_EQ(panel.selectedComponent(), &tab.find("Motor tube"));
    // With column 0 among the selected columns, the first is column 0 (getSelectedComponent()).
    Tab::click(panel, 1, 0, Qt::ShiftModifier);
    EXPECT_EQ(Tab::lines(panel), "rows=[1] cols=[0, 1, 2]");
    EXPECT_EQ(enabledActions(panel), "");
}

// ======================================================================== the operations

TEST_F(MotorConfigurationPanelTest, SelectMotorGivesEverySelectedCellTheChoice)
{
    Tab tab(Design::TWO_STAGES);
    tab.show();
    const MotorConfigurationPanel& panel = tab.motors();
    Tab::click(panel, 1, 1);
    Tab::click(panel, 2, 2, Qt::ShiftModifier);
    tab.prompts().answerMotor(UserPrompts::MotorChoice{.motor = tab.estes("C6"), .delay = 3});
    panel.selectMotorButton()->click();
    ASSERT_EQ(tab.prompts().motorQuestions().size(), 1U);
    EXPECT_EQ(tab.prompts().motorQuestions()[0].mountName, QStringLiteral("Body"));
    EXPECT_EQ(tab.prompts().motorQuestions()[0].configurationId, tab.id(1));
    EXPECT_EQ(cell(panel, 1, 1), "C6-3 | Default (Automatic)");
    EXPECT_EQ(cell(panel, 2, 1), "C6-3 | Default (Automatic)");
    EXPECT_EQ(cell(panel, 1, 2), "C6-3 | Default (Automatic)");
    EXPECT_EQ(cell(panel, 2, 2), "C6-3 | Default (Automatic)");
    EXPECT_EQ(cell(panel, 0, 1), "C6-5 | Default (Automatic)");
    EXPECT_EQ(tab.document().getUndoDescription().value_or("-"), "Select motor");
    EXPECT_EQ(Tab::lines(panel), "rows=[1] cols=[1]");
}

TEST_F(MotorConfigurationPanelTest, TheIgnitionButtonsAskForTheFirstCellAndReset)
{
    Tab tab(Design::TWO_STAGES);
    tab.show();
    const MotorConfigurationPanel& panel = tab.motors();
    Tab::click(panel, 1, 1);
    tab.prompts().answerIgnition(
        UserPrompts::IgnitionChoice{.scope = UserPrompts::ConfigurationScope::THIS_CONFIGURATION,
                                    .event = IgnitionEvent::LAUNCH,
                                    .delay = 2});
    panel.selectIgnitionButton()->click();
    ASSERT_EQ(tab.prompts().ignitionRequests().size(), 1U);
    EXPECT_EQ(tab.prompts().ignitionRequests()[0].configurationName,
              QStringLiteral("[C6-5; None]"));
    EXPECT_EQ(tab.prompts().ignitionRequests()[0].initial.scope,
              UserPrompts::ConfigurationScope::ALL_CONFIGURATIONS);
    EXPECT_EQ(cell(panel, 1, 1), "C6-5 | Launch + 2 s");
    EXPECT_EQ(tab.document().getUndoDescription().value_or("-"), "Select ignition");

    panel.resetIgnitionButton()->click();
    EXPECT_EQ(cell(panel, 1, 1), "C6-5 | Automatic");
    EXPECT_EQ(tab.document().getUndoDescription().value_or("-"), "Reset ignition");

    panel.deleteMotorButton()->click();
    EXPECT_EQ(cell(panel, 1, 1), "None | Default (Automatic)");
    EXPECT_EQ(tab.document().getUndoDescription().value_or("-"), "Delete motor(s)");
    EXPECT_EQ(Tab::lines(panel), "rows=[1] cols=[1]");
}

TEST_F(MotorConfigurationPanelTest, ThePopupHoldsTheMotorActionsAndTheTabsOwn)
{
    Tab tab(Design::TWO_STAGES);
    tab.show();
    const QMenu* menu = Tab::rightClick(tab.motors(), 2, 3);
    ASSERT_EQ(menu, &tab.motors().fullPopupMenu());
    EXPECT_EQ(Tab::items(*menu),
              "selectMotorAction, deleteMotorAction, -, selectIgnitionAction, "
              "resetIgnitionAction, -, renameConfigAction, deleteConfigAction, "
              "duplicateConfigAction");
    // Its items act on the cell.
    tab.prompts().answerMotor(std::nullopt);
    tab.motors().selectMotorAction()->trigger();
    ASSERT_EQ(tab.prompts().motorQuestions().size(), 1U);
    EXPECT_EQ(tab.prompts().motorQuestions()[0].mountName, QStringLiteral("Booster tube"));
    EXPECT_EQ(tab.prompts().motorQuestions()[0].configurationId, tab.id(2));
}

}  // namespace

}  // namespace QtRocket::Gui
