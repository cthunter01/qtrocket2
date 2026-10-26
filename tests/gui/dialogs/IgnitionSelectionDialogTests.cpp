// The dialog "Select Ignition Configuration" (OpenRocket's IgnitionSelectionDialog). The texts are
// messages.properties', the widgets' order and first states those of the scouting's dump of
// OpenRocket's dialog (A simple model rocket, the cell of [B4-4]).

#include "QtRocket/gui/dialogs/IgnitionSelectionDialog.h"

#include <optional>

#include <QComboBox>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QRadioButton>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/motor/IgnitionEvent.h"

namespace QtRocket::Gui
{

namespace
{

using IgnitionSelectionDialogTest = Test::GuiTest;
using Scope                       = UserPrompts::ConfigurationScope;

/// The texts of @p combo's items.
[[nodiscard]] QStringList itemTexts(const QComboBox& combo)
{
    QStringList texts;
    for (int index = 0; index < combo.count(); ++index)
    {
        texts.append(combo.itemText(index));
    }
    return texts;
}

/// The question about the configuration [B4-4], starting on @p scope with @p event and @p delay.
[[nodiscard]] UserPrompts::IgnitionRequest request(Scope scope, IgnitionEvent event, double delay)
{
    return {.configurationName = QStringLiteral("[B4-4]"),
            .initial           = {.scope = scope, .event = event, .delay = delay}};
}

/// How the dialog is answered.
enum class Answer
{
    OK,
    CANCEL,
    ESCAPE,
    CLOSE,
};

/// Chooses "Override ...", "Never" and the delay "2.5" (typed, not committed), then answers.
struct ChooseAndAnswer
{
    Answer answer = Answer::OK;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<IgnitionSelectionDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        dialog->overrideButton().click();
        Q_EMIT dialog->eventBox().activated(4);
        UnitSpinBox& delay = dialog->delaySpinBox();
        delay.setFocus();
        delay.selectAll();
        QTest::keyClicks(&delay, QStringLiteral("2.5"));
        switch (answer)
        {
            case Answer::OK:
                dialog->okButton().click();
                break;
            case Answer::CANCEL:
                dialog->cancelButton().click();
                break;
            case Answer::ESCAPE:
                // The spin box takes the first Escape (it reverts the edit), the dialog the
                // second.
                QTest::keyClick(&delay, Qt::Key_Escape);
                QTest::keyClick(dialog, Qt::Key_Escape);
                break;
            case Answer::CLOSE:
                dialog->close();
                break;
        }
    }
};

/// @p answer's choice, or a test failure and a default choice when there is none. (A plain if is
/// what clang-tidy's unchecked-optional-access check follows; it cannot see through ASSERT_TRUE.)
template <class Choice>
[[nodiscard]] Choice required(const std::optional<Choice>& answer)
{
    if (!answer.has_value())
    {
        ADD_FAILURE() << "no answer";
        return {};
    }
    return *answer;
}

TEST_F(IgnitionSelectionDialogTest, ShowsOpenRocketsDialog)
{
    const IgnitionSelectionDialog dialog(
        request(Scope::THIS_CONFIGURATION, IgnitionEvent::AUTOMATIC, 0.0));
    EXPECT_EQ(dialog.objectName(), QStringLiteral("IgnitionSelectionDialog"));
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Select Ignition Configuration"));
    EXPECT_EQ(dialog.windowModality(), Qt::ApplicationModal);
    EXPECT_EQ(dialog.scopeLabel().text(),
              QStringLiteral("Which flight configurations are affected:"));
    EXPECT_EQ(dialog.defaultButton().text(),
              QStringLiteral("Change all configurations using the default ignition event"));
    EXPECT_EQ(dialog.defaultButton().objectName(), QStringLiteral("defaultButton"));
    EXPECT_EQ(dialog.overrideButton().text(),
              QStringLiteral("Override for the [B4-4] flight configuration only"));
    EXPECT_EQ(dialog.overrideButton().objectName(), QStringLiteral("overrideButton"));
    EXPECT_EQ(dialog.eventLabel().text(), QStringLiteral("Ignition at:"));
    EXPECT_EQ(dialog.eventBox().objectName(), QStringLiteral("IgnitionEvent"));
    EXPECT_EQ(itemTexts(dialog.eventBox()),
              (QStringList{
                  QStringLiteral("Automatic (launch or ejection charge)"), QStringLiteral("Launch"),
                  QStringLiteral("First ejection charge of previous stage"),
                  QStringLiteral("First burnout of previous stage"), QStringLiteral("Never")}));
    EXPECT_EQ(dialog.plusLabel().text(), QStringLiteral("plus"));
    EXPECT_EQ(dialog.delaySpinBox().objectName(), QStringLiteral("IgnitionDelay"));
    EXPECT_EQ(dialog.delaySpinBox().text(), QStringLiteral("0"));
    EXPECT_EQ(dialog.secondsLabel().text(), QStringLiteral("seconds"));
    EXPECT_EQ(dialog.okButton().text(), QStringLiteral("OK"));
    EXPECT_EQ(dialog.okButton().objectName(), QStringLiteral("okButton"));
    EXPECT_TRUE(dialog.okButton().isDefault());
    EXPECT_EQ(dialog.cancelButton().text(), QStringLiteral("Cancel"));
    EXPECT_EQ(dialog.cancelButton().objectName(), QStringLiteral("cancel"));
    // "sizegroup btn".
    EXPECT_EQ(dialog.okButton().minimumWidth(), dialog.cancelButton().minimumWidth());
}

TEST_F(IgnitionSelectionDialogTest, StartsWithTheRequestsRadioButtonEventAndDelay)
{
    // The caller computes the radio button (fixed OpenRocket bug 2: Java's follows "no motor").
    const IgnitionSelectionDialog all(
        request(Scope::ALL_CONFIGURATIONS, IgnitionEvent::BURNOUT, 1.5));
    EXPECT_TRUE(all.defaultButton().isChecked());
    EXPECT_FALSE(all.overrideButton().isChecked());
    EXPECT_EQ(all.eventBox().currentIndex(), 3);
    EXPECT_EQ(all.eventBox().currentText(), QStringLiteral("First burnout of previous stage"));
    EXPECT_EQ(all.delaySpinBox().text(), QStringLiteral("1.5"));
    const UserPrompts::IgnitionChoice choice = all.choice();
    EXPECT_EQ(choice.scope, Scope::ALL_CONFIGURATIONS);
    EXPECT_EQ(choice.event, IgnitionEvent::BURNOUT);
    EXPECT_EQ(choice.delay, 1.5);

    const IgnitionSelectionDialog overriding(
        request(Scope::THIS_CONFIGURATION, IgnitionEvent::LAUNCH, 0.0));
    EXPECT_FALSE(overriding.defaultButton().isChecked());
    EXPECT_TRUE(overriding.overrideButton().isChecked());
    EXPECT_EQ(overriding.scope(), Scope::THIS_CONFIGURATION);
    overriding.defaultButton().click();
    EXPECT_EQ(overriding.scope(), Scope::ALL_CONFIGURATIONS);
    EXPECT_FALSE(overriding.overrideButton().isChecked());
}

TEST_F(IgnitionSelectionDialogTest, ShowsAnAmpersandOfTheNameAsItIs)
{
    // Swing shows the text as it is; Qt's radio button would read "&B" as a mnemonic.
    const IgnitionSelectionDialog dialog(
        {.configurationName = QStringLiteral("A&B"), .initial = {}});
    EXPECT_EQ(dialog.overrideButton().text(),
              QStringLiteral("Override for the A&&B flight configuration only"));
}

TEST_F(IgnitionSelectionDialogTest, OkAnswersTheRadioButtonTheEventAndTheDelay)
{
    // The delay typed and not committed is committed by OK.
    const Gui::Test::NextModalDialog  answer(ChooseAndAnswer{.answer = Answer::OK});
    const UserPrompts::IgnitionChoice choice = required(IgnitionSelectionDialog::ask(
        nullptr, request(Scope::ALL_CONFIGURATIONS, IgnitionEvent::AUTOMATIC, 0.0)));
    EXPECT_EQ(choice.scope, Scope::THIS_CONFIGURATION);
    EXPECT_EQ(choice.event, IgnitionEvent::NEVER);
    EXPECT_EQ(choice.delay, 2.5);
}

TEST_F(IgnitionSelectionDialogTest, EscapeAnswersAsOk)
{
    // GUIUtil.installEscapeCloseButtonOperation(this, okButton): Escape clicks OK. The spin box's
    // first Escape reverted the typed delay.
    const Gui::Test::NextModalDialog  answer(ChooseAndAnswer{.answer = Answer::ESCAPE});
    const UserPrompts::IgnitionChoice choice = required(IgnitionSelectionDialog::ask(
        nullptr, request(Scope::ALL_CONFIGURATIONS, IgnitionEvent::AUTOMATIC, 1.0)));
    EXPECT_EQ(choice.scope, Scope::THIS_CONFIGURATION);
    EXPECT_EQ(choice.event, IgnitionEvent::NEVER);
    EXPECT_EQ(choice.delay, 1.0);
}

TEST_F(IgnitionSelectionDialogTest, CancelAndTheCloseButtonAnswerNothing)
{
    // Fixed OpenRocket bug 3: Java's Cancel marks the ignition overridden, and its close button
    // keeps the live edits.
    {
        const Gui::Test::NextModalDialog answer(ChooseAndAnswer{.answer = Answer::CANCEL});
        EXPECT_EQ(IgnitionSelectionDialog::ask(
                      nullptr, request(Scope::ALL_CONFIGURATIONS, IgnitionEvent::AUTOMATIC, 0.0)),
                  std::nullopt);
    }
    const Gui::Test::NextModalDialog answer(ChooseAndAnswer{.answer = Answer::CLOSE});
    EXPECT_EQ(IgnitionSelectionDialog::ask(
                  nullptr, request(Scope::ALL_CONFIGURATIONS, IgnitionEvent::AUTOMATIC, 0.0)),
              std::nullopt);
}

TEST_F(IgnitionSelectionDialogTest, TheDelayIsAtLeastZero)
{
    // DoubleModel(curMotorInstance, "IgnitionDelay", UNITS_SHORT_TIME, 0).
    const IgnitionSelectionDialog dialog(
        request(Scope::ALL_CONFIGURATIONS, IgnitionEvent::AUTOMATIC, 1.0));
    UnitSpinBox& delay = dialog.delaySpinBox();
    delay.selectAll();
    QTest::keyClicks(&delay, QStringLiteral("-3"));
    EXPECT_TRUE(delay.commitEdit());
    EXPECT_EQ(dialog.choice().delay, 0.0);
    EXPECT_EQ(delay.text(), QStringLiteral("0"));
}

}  // namespace

}  // namespace QtRocket::Gui
