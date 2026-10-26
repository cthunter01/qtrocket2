// What the ignition, deployment and separation dialogs share (each of them is tested with its own
// texts in its own file): the scope's radio buttons and the buttons, on a dialog without values.

#include "QtRocket/gui/dialogs/FlightConfigurationValueDialog.h"

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using FlightConfigurationValueDialogTest = Test::GuiTest;
using Scope                              = UserPrompts::ConfigurationScope;

/// A value dialog without values, its buttons added on request.
class BareValueDialog final : public FlightConfigurationValueDialog
{
public:
    explicit BareValueDialog(Scope scope, QWidget* parent = nullptr)
      : FlightConfigurationValueDialog(
            QStringLiteral("Select Nothing"),
            {.title             = QStringLiteral("Which flight configurations are affected:"),
             .allConfigurations = QStringLiteral("All & every"),
             .thisConfiguration = QStringLiteral("Only {0}, really {0}")},
            QStringLiteral("[A&B]"), scope, parent)
    {
    }

    using FlightConfigurationValueDialog::addButtons;
};

/// Clicks OK, Cancel, the close button or presses Escape on a BareValueDialog.
struct Answer
{
    enum class How
    {
        OK,
        CANCEL,
        ESCAPE,
        CLOSE,
    };

    How how = How::OK;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = dynamic_cast<BareValueDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        switch (how)
        {
            case How::OK:
                dialog->okButton().click();
                break;
            case How::CANCEL:
                dialog->cancelButton().click();
                break;
            case How::ESCAPE:
                QTest::keyClick(dialog, Qt::Key_Escape);
                break;
            case How::CLOSE:
                dialog->close();
                break;
        }
    }
};

/// The result of a BareValueDialog with buttons, answered @p how.
[[nodiscard]] int answered(Answer::How how)
{
    BareValueDialog dialog(Scope::ALL_CONFIGURATIONS);
    dialog.addButtons();
    const Gui::Test::NextModalDialog answer(Answer{.how = how});
    return dialog.exec();
}

TEST_F(FlightConfigurationValueDialogTest, ShowsTheScopesRadioButtons)
{
    // Java's str.replace("{0}", name) replaces every "{0}"; an '&' shows as it is.
    const BareValueDialog dialog(Scope::THIS_CONFIGURATION);
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Select Nothing"));
    EXPECT_EQ(dialog.windowModality(), Qt::ApplicationModal);
    EXPECT_EQ(dialog.scopeLabel().text(),
              QStringLiteral("Which flight configurations are affected:"));
    EXPECT_EQ(dialog.defaultButton().text(), QStringLiteral("All && every"));
    EXPECT_EQ(dialog.overrideButton().text(), QStringLiteral("Only [A&&B], really [A&&B]"));
    EXPECT_TRUE(dialog.overrideButton().isChecked());
    EXPECT_EQ(dialog.scope(), Scope::THIS_CONFIGURATION);
    // The radio buttons are exclusive.
    dialog.defaultButton().click();
    EXPECT_FALSE(dialog.overrideButton().isChecked());
    EXPECT_EQ(dialog.scope(), Scope::ALL_CONFIGURATIONS);
    dialog.overrideButton().click();
    EXPECT_FALSE(dialog.defaultButton().isChecked());
    EXPECT_EQ(dialog.scope(), Scope::THIS_CONFIGURATION);
}

TEST_F(FlightConfigurationValueDialogTest, TheButtonsComeOnceAfterTheValues)
{
    BareValueDialog dialog(Scope::ALL_CONFIGURATIONS);
    EXPECT_TRUE(dialog.defaultButton().isChecked());
    EXPECT_THROW(static_cast<void>(dialog.okButton()), BugError);
    EXPECT_THROW(static_cast<void>(dialog.cancelButton()), BugError);
    dialog.addButtons();
    EXPECT_EQ(dialog.okButton().text(), QStringLiteral("OK"));
    EXPECT_TRUE(dialog.okButton().isDefault());
    EXPECT_EQ(dialog.cancelButton().text(), QStringLiteral("Cancel"));
    EXPECT_EQ(dialog.okButton().minimumWidth(), dialog.cancelButton().minimumWidth());
    EXPECT_THROW(dialog.addButtons(), BugError);
}

TEST_F(FlightConfigurationValueDialogTest, OkAndEscapeAcceptCancelAndTheCloseButtonReject)
{
    EXPECT_EQ(answered(Answer::How::OK), QDialog::Accepted);
    EXPECT_EQ(answered(Answer::How::ESCAPE), QDialog::Accepted);
    EXPECT_EQ(answered(Answer::How::CANCEL), QDialog::Rejected);
    EXPECT_EQ(answered(Answer::How::CLOSE), QDialog::Rejected);
}

}  // namespace

}  // namespace QtRocket::Gui
