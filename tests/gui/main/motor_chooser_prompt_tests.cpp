// UserPrompts::chooseMotor() of the application (WidgetUserPrompts): OpenRocket's motor chooser
// (MotorChooserDialog), one per top-level window as MotorConfigurationPanel keeps one, answered
// from its own event loop by NextModalDialog. The dialog itself is tested in
// tests/gui/dialogs/motor/MotorChooserDialogTests.cpp.

#include <memory>
#include <optional>
#include <utility>

#include <QList>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/gui/dialogs/motor/MotorChooserDialog.h"
#include "QtRocket/gui/main/WidgetUserPrompts.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/util/BugError.h"
#include "TestGuiServices.h"
#include "dialogs/motor/MotorChooserTestSupport.h"
#include "motor/TestMotorDatabase.h"

namespace QtRocket::Gui
{

namespace
{

using MotorChooserPromptTest = Gui::Test::GuiTest;
using Gui::Test::BodyTubeMount;
using Gui::Test::NextModalDialog;
using MotorChoice = UserPrompts::MotorChoice;

/// Clicks the chooser's button named @p name.
void clickButton(QWidget& dialog, const QString& name)
{
    auto* button = dialog.findChild<QPushButton*>(name);
    if (button == nullptr || qobject_cast<MotorChooserDialog*>(&dialog) == nullptr)
    {
        bug("the modal dialog is no motor chooser with a button " + QtStrings::toUtf8(name));
    }
    QTest::mouseClick(button, Qt::LeftButton);
}

void clickOk(QWidget& dialog)
{
    clickButton(dialog, QStringLiteral("okButton"));
}

void clickCancel(QWidget& dialog)
{
    clickButton(dialog, QStringLiteral("cancelButton"));
}

/// The services over smallMotorDatabase(), a design window with a widget in it, and a mount
/// with the configurations of the questions.
class Asking
{
public:
    Asking() : m_child(makeChild<QWidget>(&m_window))
    {
        m_services.motors()           = QtRocket::Test::smallMotorDatabase();
        const ThrustCurveMotorSet& a8 = Gui::Test::findMotorSet(m_services.motors(), "Estes", "A8");
        m_motor                       = a8.getMotors().front();
        m_withMotor                   = m_mount.configure(m_motor, 3.0);
        m_empty                       = m_mount.addEmptyConfiguration();
    }

    /// prompts().chooseMotor() from @p parent for the configuration with a motor (or the empty
    /// one), the dialog answered by @p action.
    [[nodiscard]] std::optional<MotorChoice> choose(QWidget* parent, NextModalDialog::Action action,
                                                    bool empty = false)
    {
        const NextModalDialog answer(std::move(action));
        return m_prompts.chooseMotor(parent, m_services.services(), m_mount.mount(),
                                     empty ? m_empty : m_withMotor);
    }

    [[nodiscard]] QWidget& window() noexcept { return m_window; }
    [[nodiscard]] QWidget& child() const noexcept { return *m_child; }
    [[nodiscard]] const std::shared_ptr<const ThrustCurveMotor>& motor() const noexcept
    {
        return m_motor;
    }
    [[nodiscard]] QList<MotorChooserDialog*> choosers() const
    {
        return m_window.findChildren<MotorChooserDialog*>(Qt::FindDirectChildrenOnly);
    }

private:
    Gui::Test::TestGuiServices              m_services;
    WidgetUserPrompts                       m_prompts;
    BodyTubeMount                           m_mount;
    std::shared_ptr<const ThrustCurveMotor> m_motor;
    FlightConfigurationId                   m_withMotor;
    FlightConfigurationId                   m_empty;
    QWidget                                 m_window;
    QWidget*                                m_child;
};

TEST_F(MotorChooserPromptTest, OkAnswersTheMotorWithItsDelayAndNozzle)
{
    Asking                           asking;
    const std::optional<MotorChoice> choice = asking.choose(&asking.window(), clickOk);
    const MotorChoice                chosen = Gui::Test::chosenMotor(choice);
    EXPECT_EQ(chosen.motor, asking.motor());
    EXPECT_EQ(chosen.delay, 3.0);
    EXPECT_EQ(chosen.nozzleExitDiameter, 0.0);
}

TEST_F(MotorChooserPromptTest, CancelAnswersNothing)
{
    Asking asking;
    EXPECT_EQ(asking.choose(&asking.window(), clickCancel), std::nullopt);
}

TEST_F(MotorChooserPromptTest, TheWindowKeepsItsChooserBetweenQuestions)
{
    Asking asking;
    EXPECT_TRUE(asking.choosers().isEmpty());
    EXPECT_TRUE(asking.choose(&asking.window(), clickOk).has_value());
    ASSERT_EQ(asking.choosers().size(), 1);
    const MotorChooserDialog* const chooser = asking.choosers().front();
    // Asked from a widget of the window, for the configuration without a motor: the same chooser,
    // which selects the motor of the question before (its table row).
    const std::optional<MotorChoice> choice = asking.choose(&asking.child(), clickOk, true);
    const MotorChoice                chosen = Gui::Test::chosenMotor(choice);
    EXPECT_EQ(chosen.motor, asking.motor());
    ASSERT_EQ(asking.choosers().size(), 1);
    EXPECT_EQ(asking.choosers().front(), chooser);
    EXPECT_TRUE(chooser->isHidden());
}

TEST_F(MotorChooserPromptTest, AQuestionWithoutAParentLeavesNoChooser)
{
    Asking                           asking;
    const std::optional<MotorChoice> choice = asking.choose(nullptr, clickOk);
    const MotorChoice                chosen = Gui::Test::chosenMotor(choice);
    EXPECT_EQ(chosen.motor, asking.motor());
    EXPECT_TRUE(asking.choosers().isEmpty());
}

}  // namespace

}  // namespace QtRocket::Gui
