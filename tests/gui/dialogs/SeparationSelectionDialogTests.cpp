// The dialog "Select Separation Configuration" (OpenRocket's SeparationSelectionDialog). The texts
// are messages.properties', the widgets' order and first states those of the scouting's dumps of
// OpenRocket's dialog (the booster of a two-stage design, the configuration [H148R-0; H148R-0]:
// "slider 111 [0,1000]" for 200 m, "slider 222" for 300 m).

#include "QtRocket/gui/dialogs/SeparationSelectionDialog.h"

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
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"

namespace QtRocket::Gui
{

namespace
{

using SeparationSelectionDialogTest = Test::GuiTest;
using Scope                         = UserPrompts::ConfigurationScope;
using SeparationEvent               = StageSeparationConfiguration::SeparationEvent;

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

/// A separation at @p event, @p altitude m and @p delay s.
[[nodiscard]] StageSeparationConfiguration separation(SeparationEvent event, double altitude,
                                                      double delay)
{
    StageSeparationConfiguration configuration;
    configuration.setSeparationEvent(event);
    configuration.setSeparationAltitude(altitude);
    configuration.setSeparationDelay(delay);
    return configuration;
}

/// The question about the configuration [H148R-0; H148R-0], starting on @p scope with @p initial.
[[nodiscard]] UserPrompts::SeparationRequest request(Scope                               scope,
                                                     const StageSeparationConfiguration& initial)
{
    return {.configurationName = QStringLiteral("[H148R-0; H148R-0]"),
            .initial           = {.scope = scope, .separation = initial}};
}

/// How the dialog is answered.
enum class Answer
{
    OK,
    CANCEL,
    ESCAPE,
    CLOSE,
};

/// Chooses "Change all ..." and the event at @p eventIndex, the delay 0.5 (committed with Enter)
/// and the altitude 300 (typed, not committed), then answers.
struct ChooseAndAnswer
{
    int    eventIndex = 5;
    Answer answer     = Answer::OK;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<SeparationSelectionDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        dialog->defaultButton().click();
        Q_EMIT dialog->eventBox().activated(eventIndex);
        UnitSpinBox& delay = dialog->delaySpinBox();
        delay.selectAll();
        QTest::keyClicks(&delay, QStringLiteral("0.5"));
        QTest::keyClick(&delay, Qt::Key_Return);
        UnitSpinBox& altitude = *dialog->altitude().spinBox;
        altitude.setFocus();
        altitude.selectAll();
        QTest::keyClicks(&altitude, QStringLiteral("300"));
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
                QTest::keyClick(&altitude, Qt::Key_Escape);
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

TEST_F(SeparationSelectionDialogTest, ShowsOpenRocketsDialog)
{
    const SeparationSelectionDialog dialog(
        request(Scope::THIS_CONFIGURATION, separation(SeparationEvent::BURNOUT, 200, 0)));
    EXPECT_EQ(dialog.objectName(), QStringLiteral("SeparationSelectionDialog"));
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Select Separation Configuration"));
    EXPECT_EQ(dialog.windowModality(), Qt::ApplicationModal);
    EXPECT_EQ(dialog.scopeLabel().text(),
              QStringLiteral("Which flight configurations are affected:"));
    EXPECT_EQ(dialog.defaultButton().text(),
              QStringLiteral("Change all configurations using the default separation event"));
    EXPECT_EQ(dialog.overrideButton().text(),
              QStringLiteral("Override for the [H148R-0; H148R-0] flight configuration only"));
    EXPECT_TRUE(dialog.overrideButton().isChecked());
    EXPECT_EQ(dialog.eventLabel().text(), QStringLiteral("Upper stage separates at:"));
    EXPECT_EQ(dialog.eventBox().objectName(), QStringLiteral("SeparationEvent"));
    EXPECT_EQ(
        itemTexts(dialog.eventBox()),
        (QStringList{QStringLiteral("Launch"), QStringLiteral("Current stage motor ignition"),
                     QStringLiteral("Current stage motor burnout"),
                     QStringLiteral("Current stage ejection charge"),
                     QStringLiteral("Upper stage motor ignition"),
                     QStringLiteral("Specific altitude during ascent"), QStringLiteral("Apogee"),
                     QStringLiteral("Specific altitude during descent"), QStringLiteral("Never")}));
    EXPECT_EQ(dialog.eventBox().currentIndex(), 2);
    EXPECT_EQ(dialog.altitude().label->text(), QStringLiteral("Altitude:"));
    EXPECT_EQ(dialog.altitude().spinBox->objectName(), QStringLiteral("SeparationAltitude"));
    EXPECT_EQ(dialog.altitude().spinBox->text(), QStringLiteral("200"));
    EXPECT_EQ(dialog.altitude().unitSelector->text(), QStringLiteral("m"));
    EXPECT_EQ(dialog.altitude().slider->value(), 111);
    EXPECT_EQ(dialog.altitude().slider->maximum(), SliderMapping::kMaximum);
    EXPECT_FALSE(dialog.altitude().spinBox->isEnabled());
    EXPECT_EQ(dialog.plusLabel().text(), QStringLiteral("plus"));
    EXPECT_EQ(dialog.delaySpinBox().objectName(), QStringLiteral("SeparationDelay"));
    EXPECT_EQ(dialog.delaySpinBox().text(), QStringLiteral("0"));
    EXPECT_EQ(dialog.secondsLabel().text(), QStringLiteral("seconds"));
    EXPECT_EQ(dialog.okButton().text(), QStringLiteral("OK"));
    EXPECT_TRUE(dialog.okButton().isDefault());
    EXPECT_EQ(dialog.cancelButton().text(), QStringLiteral("Cancel"));
}

TEST_F(SeparationSelectionDialogTest, TheAltitudeIsEnabledForTheTwoAltitudesOnly)
{
    const SeparationSelectionDialog dialog(request(
        Scope::ALL_CONFIGURATIONS, separation(SeparationEvent::ALTITUDE_ASCENDING, 300, 0.5)));
    EXPECT_TRUE(dialog.defaultButton().isChecked());
    EXPECT_TRUE(dialog.isAltitudeEnabled());
    EXPECT_TRUE(dialog.altitude().label->isEnabled());
    EXPECT_TRUE(dialog.altitude().spinBox->isEnabled());
    EXPECT_TRUE(dialog.altitude().unitSelector->isEnabled());
    EXPECT_TRUE(dialog.altitude().slider->isEnabled());
    // Probe: "spinner 300", "slider 222", "spinner 0.5".
    EXPECT_EQ(dialog.altitude().spinBox->text(), QStringLiteral("300"));
    EXPECT_EQ(dialog.altitude().slider->value(), 222);
    EXPECT_EQ(dialog.delaySpinBox().text(), QStringLiteral("0.5"));

    Q_EMIT dialog.eventBox().activated(6);  // Apogee
    EXPECT_FALSE(dialog.isAltitudeEnabled());
    EXPECT_FALSE(dialog.altitude().label->isEnabled());
    EXPECT_FALSE(dialog.altitude().spinBox->isEnabled());
    EXPECT_FALSE(dialog.altitude().unitSelector->isEnabled());
    EXPECT_FALSE(dialog.altitude().slider->isEnabled());
    Q_EMIT dialog.eventBox().activated(7);  // Specific altitude during descent
    EXPECT_TRUE(dialog.isAltitudeEnabled());
    EXPECT_TRUE(dialog.altitude().slider->isEnabled());
}

TEST_F(SeparationSelectionDialogTest, TheSliderIsLinearFrom100To1000Metres)
{
    const SeparationSelectionDialog dialog(request(
        Scope::THIS_CONFIGURATION, separation(SeparationEvent::ALTITUDE_DESCENDING, 200, 0)));
    ValueSlider&                    slider = *dialog.altitude().slider;
    EXPECT_EQ(slider.value(), 111);
    // Probe (ValueSliderModel.setValue(): the value rounded in the unit, m): 250 gives 325 m,
    // rounded to 320 m; 500 gives 550 m.
    slider.setValue(250);
    EXPECT_EQ(dialog.choice().separation.getSeparationAltitude(), 320.0);
    EXPECT_EQ(dialog.altitude().spinBox->text(), QStringLiteral("320"));
    slider.setValue(500);
    EXPECT_EQ(dialog.choice().separation.getSeparationAltitude(), 550.0);
}

TEST_F(SeparationSelectionDialogTest, OkAnswersTheRadioButtonAndTheSeparation)
{
    // The altitude typed and not committed is committed by OK.
    const Gui::Test::NextModalDialog answer(ChooseAndAnswer{.eventIndex = 5, .answer = Answer::OK});
    const UserPrompts::SeparationChoice choice = required(SeparationSelectionDialog::ask(
        nullptr, request(Scope::THIS_CONFIGURATION, separation(SeparationEvent::BURNOUT, 200, 0))));
    EXPECT_EQ(choice.scope, Scope::ALL_CONFIGURATIONS);
    const StageSeparationConfiguration& chosen = choice.separation;
    EXPECT_EQ(chosen.getSeparationEvent(), SeparationEvent::ALTITUDE_ASCENDING);
    EXPECT_EQ(chosen.getSeparationAltitude(), 300.0);
    EXPECT_EQ(chosen.getSeparationDelay(), 0.5);
}

TEST_F(SeparationSelectionDialogTest, NeverIsAnsweredWithoutADelay)
{
    // OK's action: NEVER sets the delay to 0.
    const Gui::Test::NextModalDialog answer(ChooseAndAnswer{.eventIndex = 8, .answer = Answer::OK});
    const UserPrompts::SeparationChoice choice = required(SeparationSelectionDialog::ask(
        nullptr, request(Scope::THIS_CONFIGURATION, separation(SeparationEvent::BURNOUT, 200, 0))));
    EXPECT_EQ(choice.separation.getSeparationEvent(), SeparationEvent::NEVER);
    EXPECT_EQ(choice.separation.getSeparationDelay(), 0.0);
    // The altitude's spin box is disabled for NEVER: it took no keys.
    EXPECT_EQ(choice.separation.getSeparationAltitude(), 200.0);

    // The dialog shows the delay all the same, as Java's spinner does until OK.
    const SeparationSelectionDialog dialog(
        request(Scope::THIS_CONFIGURATION, separation(SeparationEvent::NEVER, 200, 2.0)));
    EXPECT_EQ(dialog.delaySpinBox().text(), QStringLiteral("2"));
    EXPECT_EQ(dialog.choice().separation.getSeparationDelay(), 0.0);
}

TEST_F(SeparationSelectionDialogTest, EscapeAnswersAsOk)
{
    // GUIUtil.installEscapeCloseButtonOperation(this, okButton). The spin box's first Escape
    // reverted the typed altitude.
    const Gui::Test::NextModalDialog answer(
        ChooseAndAnswer{.eventIndex = 7, .answer = Answer::ESCAPE});
    const UserPrompts::SeparationChoice choice = required(SeparationSelectionDialog::ask(
        nullptr, request(Scope::THIS_CONFIGURATION, separation(SeparationEvent::BURNOUT, 200, 0))));
    EXPECT_EQ(choice.scope, Scope::ALL_CONFIGURATIONS);
    EXPECT_EQ(choice.separation.getSeparationEvent(), SeparationEvent::ALTITUDE_DESCENDING);
    EXPECT_EQ(choice.separation.getSeparationAltitude(), 200.0);
    EXPECT_EQ(choice.separation.getSeparationDelay(), 0.5);
}

TEST_F(SeparationSelectionDialogTest, CancelAndTheCloseButtonAnswerNothing)
{
    // Fixed OpenRocket bug 3: Java edits an existing override in place, so that its Cancel and
    // close button keep the edits (the scouting's probe: "Specific altitude during ascent + 0.5 s"
    // after Cancel).
    {
        const Gui::Test::NextModalDialog answer(
            ChooseAndAnswer{.eventIndex = 5, .answer = Answer::CANCEL});
        EXPECT_EQ(SeparationSelectionDialog::ask(
                      nullptr, request(Scope::THIS_CONFIGURATION,
                                       separation(SeparationEvent::BURNOUT, 200, 0))),
                  std::nullopt);
    }
    const Gui::Test::NextModalDialog answer(
        ChooseAndAnswer{.eventIndex = 5, .answer = Answer::CLOSE});
    EXPECT_EQ(SeparationSelectionDialog::ask(
                  nullptr,
                  request(Scope::THIS_CONFIGURATION, separation(SeparationEvent::BURNOUT, 200, 0))),
              std::nullopt);
}

}  // namespace

}  // namespace QtRocket::Gui
