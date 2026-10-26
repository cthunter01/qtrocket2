// The dialog "Select Deployment Configuration" (OpenRocket's DeploymentSelectionDialog). The texts
// are messages.properties', the widgets' order and first states those of the scouting's dump of
// OpenRocket's dialog (the parachute of A simple model rocket, the cell of [B4-4]: "slider 111
// [0,1000]" for 200 m).

#include "QtRocket/gui/dialogs/DeploymentSelectionDialog.h"

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
#include "QtRocket/rocket/DeploymentConfiguration.h"

namespace QtRocket::Gui
{

namespace
{

using DeploymentSelectionDialogTest = Test::GuiTest;
using Scope                         = UserPrompts::ConfigurationScope;
using DeployEvent                   = DeploymentConfiguration::DeployEvent;

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

/// A deployment at @p event, @p altitude m and @p delay s.
[[nodiscard]] DeploymentConfiguration deployment(DeployEvent event, double altitude, double delay)
{
    DeploymentConfiguration configuration;
    configuration.setDeployEvent(event);
    configuration.setDeployAltitude(altitude);
    configuration.setDeployDelay(delay);
    return configuration;
}

/// The question about the configuration [B4-4], starting on @p scope with @p initial, of a device
/// in the bottom stage when @p bottomStage.
[[nodiscard]] UserPrompts::DeploymentRequest request(Scope                          scope,
                                                     const DeploymentConfiguration& initial,
                                                     bool bottomStage = true)
{
    return {.configurationName = QStringLiteral("[B4-4]"),
            .initial           = {.scope = scope, .deployment = initial},
            .bottomStage       = bottomStage};
}

/// How the dialog is answered.
enum class Answer
{
    OK,
    CANCEL,
    ESCAPE,
    CLOSE,
};

/// Chooses "Change all ...", "Specific altitude during descent", the delay 1.5 (committed with
/// Enter) and the altitude 350 (typed, not committed), then answers.
struct ChooseAndAnswer
{
    Answer answer = Answer::OK;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<DeploymentSelectionDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        dialog->defaultButton().click();
        Q_EMIT dialog->eventBox().activated(3);
        UnitSpinBox& delay = dialog->delaySpinBox();
        delay.selectAll();
        QTest::keyClicks(&delay, QStringLiteral("1.5"));
        QTest::keyClick(&delay, Qt::Key_Return);
        UnitSpinBox& altitude = *dialog->altitude().spinBox;
        altitude.setFocus();
        altitude.selectAll();
        QTest::keyClicks(&altitude, QStringLiteral("350"));
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

TEST_F(DeploymentSelectionDialogTest, ShowsOpenRocketsDialog)
{
    const DeploymentSelectionDialog dialog(
        request(Scope::THIS_CONFIGURATION, deployment(DeployEvent::EJECTION, 200, 0)));
    EXPECT_EQ(dialog.objectName(), QStringLiteral("DeploymentSelectionDialog"));
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Select Deployment Configuration"));
    EXPECT_EQ(dialog.windowModality(), Qt::ApplicationModal);
    EXPECT_EQ(dialog.scopeLabel().text(),
              QStringLiteral("Which flight configurations are affected:"));
    EXPECT_EQ(dialog.defaultButton().text(),
              QStringLiteral("Change all configurations using the default deployment event"));
    EXPECT_EQ(dialog.overrideButton().text(),
              QStringLiteral("Override for the [B4-4] flight configuration only"));
    EXPECT_TRUE(dialog.overrideButton().isChecked());
    EXPECT_EQ(dialog.eventLabel().text(), QStringLiteral("Deploys at:"));
    EXPECT_EQ(dialog.eventBox().objectName(), QStringLiteral("DeployEvent"));
    // The bottom stage: no "Lower stage separation".
    EXPECT_EQ(itemTexts(dialog.eventBox()),
              (QStringList{
                  QStringLiteral("Launch (plus NN seconds)"),
                  QStringLiteral("First ejection charge of this stage"), QStringLiteral("Apogee"),
                  QStringLiteral("Specific altitude during descent"), QStringLiteral("Never")}));
    EXPECT_EQ(dialog.eventBox().currentIndex(), 1);
    EXPECT_EQ(dialog.altitude().label->text(), QStringLiteral("Altitude:"));
    EXPECT_EQ(dialog.altitude().spinBox->objectName(), QStringLiteral("DeployAltitude"));
    EXPECT_EQ(dialog.altitude().spinBox->text(), QStringLiteral("200"));
    EXPECT_EQ(dialog.altitude().unitSelector->text(), QStringLiteral("m"));
    EXPECT_EQ(dialog.altitude().slider->objectName(), QStringLiteral("DeployAltitude"));
    EXPECT_EQ(dialog.altitude().slider->minimum(), 0);
    EXPECT_EQ(dialog.altitude().slider->maximum(), SliderMapping::kMaximum);
    EXPECT_EQ(dialog.altitude().slider->value(), 111);
    EXPECT_EQ(dialog.plusLabel().text(), QStringLiteral("plus"));
    EXPECT_EQ(dialog.delaySpinBox().objectName(), QStringLiteral("DeployDelay"));
    EXPECT_EQ(dialog.delaySpinBox().text(), QStringLiteral("0"));
    EXPECT_EQ(dialog.secondsLabel().text(), QStringLiteral("seconds"));
    EXPECT_EQ(dialog.okButton().text(), QStringLiteral("OK"));
    EXPECT_TRUE(dialog.okButton().isDefault());
    EXPECT_EQ(dialog.cancelButton().text(), QStringLiteral("Cancel"));
}

TEST_F(DeploymentSelectionDialogTest, AnUpperStageOffersTheLowerStagesSeparation)
{
    const DeploymentSelectionDialog dialog(request(
        Scope::ALL_CONFIGURATIONS, deployment(DeployEvent::APOGEE, 200, 0), /*bottomStage=*/false));
    EXPECT_TRUE(dialog.defaultButton().isChecked());
    EXPECT_EQ(
        itemTexts(dialog.eventBox()),
        (QStringList{QStringLiteral("Launch (plus NN seconds)"),
                     QStringLiteral("First ejection charge of this stage"),
                     QStringLiteral("Apogee"), QStringLiteral("Specific altitude during descent"),
                     QStringLiteral("Lower stage separation"), QStringLiteral("Never")}));
    EXPECT_EQ(dialog.eventBox().currentIndex(), 2);
}

TEST_F(DeploymentSelectionDialogTest, ABottomStageShowsTheLowerStagesSeparationItHas)
{
    // Java's removeItem() leaves the selected item: the combo box shows it, as no item.
    const DeploymentSelectionDialog dialog(request(
        Scope::THIS_CONFIGURATION, deployment(DeployEvent::LOWER_STAGE_SEPARATION, 200, 0)));
    EXPECT_EQ(dialog.eventBox().count(), 5);
    EXPECT_EQ(dialog.eventBox().currentIndex(), -1);
    EXPECT_EQ(dialog.eventBox().shownText(), QStringLiteral("Lower stage separation"));
    EXPECT_EQ(dialog.choice().deployment.getDeployEvent(), DeployEvent::LOWER_STAGE_SEPARATION);
}

TEST_F(DeploymentSelectionDialogTest, TheAltitudeIsEnabledForASpecificAltitudeOnly)
{
    // updateState(): the label, the spin box, the unit selector and the slider.
    const DeploymentSelectionDialog dialog(
        request(Scope::THIS_CONFIGURATION, deployment(DeployEvent::EJECTION, 200, 0)));
    EXPECT_FALSE(dialog.isAltitudeEnabled());
    EXPECT_FALSE(dialog.altitude().label->isEnabled());
    EXPECT_FALSE(dialog.altitude().spinBox->isEnabled());
    EXPECT_FALSE(dialog.altitude().unitSelector->isEnabled());
    EXPECT_FALSE(dialog.altitude().slider->isEnabled());

    Q_EMIT dialog.eventBox().activated(3);  // Specific altitude during descent
    EXPECT_TRUE(dialog.isAltitudeEnabled());
    EXPECT_TRUE(dialog.altitude().label->isEnabled());
    EXPECT_TRUE(dialog.altitude().spinBox->isEnabled());
    EXPECT_TRUE(dialog.altitude().unitSelector->isEnabled());
    EXPECT_TRUE(dialog.altitude().slider->isEnabled());

    Q_EMIT dialog.eventBox().activated(4);  // Never
    EXPECT_FALSE(dialog.isAltitudeEnabled());
    EXPECT_FALSE(dialog.altitude().spinBox->isEnabled());
}

TEST_F(DeploymentSelectionDialogTest, TheSliderIsLinearFrom100To1000Metres)
{
    // getSliderModel(100, 1000), probe: 200 m at 111, 300 m at 222; the positions 0, 500 and 1000
    // give 100, 550 and 1000 m (ValueSliderModel.setValue(), rounded in m).
    const DeploymentSelectionDialog dialog(
        request(Scope::THIS_CONFIGURATION, deployment(DeployEvent::ALTITUDE, 300, 0)));
    ValueSlider& slider = *dialog.altitude().slider;
    EXPECT_EQ(slider.value(), 222);
    slider.setValue(500);
    EXPECT_EQ(dialog.choice().deployment.getDeployAltitude(), 550.0);
    EXPECT_EQ(dialog.altitude().spinBox->text(), QStringLiteral("550"));
    slider.setValue(0);
    EXPECT_EQ(dialog.choice().deployment.getDeployAltitude(), 100.0);
    slider.setValue(SliderMapping::kMaximum);
    EXPECT_EQ(dialog.choice().deployment.getDeployAltitude(), 1000.0);
}

TEST_F(DeploymentSelectionDialogTest, OkAnswersTheRadioButtonAndTheDeployment)
{
    // The altitude typed and not committed is committed by OK.
    const Gui::Test::NextModalDialog    answer(ChooseAndAnswer{.answer = Answer::OK});
    const UserPrompts::DeploymentChoice choice = required(DeploymentSelectionDialog::ask(
        nullptr, request(Scope::THIS_CONFIGURATION, deployment(DeployEvent::EJECTION, 200, 0))));
    EXPECT_EQ(choice.scope, Scope::ALL_CONFIGURATIONS);
    EXPECT_EQ(choice.deployment, deployment(DeployEvent::ALTITUDE, 350, 1.5));
}

TEST_F(DeploymentSelectionDialogTest, EscapeAnswersAsOk)
{
    // GUIUtil.installEscapeCloseButtonOperation(this, okButton). The spin box's first Escape
    // reverted the typed altitude.
    const Gui::Test::NextModalDialog    answer(ChooseAndAnswer{.answer = Answer::ESCAPE});
    const UserPrompts::DeploymentChoice choice = required(DeploymentSelectionDialog::ask(
        nullptr, request(Scope::THIS_CONFIGURATION, deployment(DeployEvent::EJECTION, 200, 0))));
    EXPECT_EQ(choice.scope, Scope::ALL_CONFIGURATIONS);
    EXPECT_EQ(choice.deployment, deployment(DeployEvent::ALTITUDE, 200, 1.5));
}

TEST_F(DeploymentSelectionDialogTest, CancelAndTheCloseButtonAnswerNothing)
{
    {
        const Gui::Test::NextModalDialog answer(ChooseAndAnswer{.answer = Answer::CANCEL});
        EXPECT_EQ(DeploymentSelectionDialog::ask(
                      nullptr, request(Scope::THIS_CONFIGURATION,
                                       deployment(DeployEvent::EJECTION, 200, 0))),
                  std::nullopt);
    }
    const Gui::Test::NextModalDialog answer(ChooseAndAnswer{.answer = Answer::CLOSE});
    EXPECT_EQ(
        DeploymentSelectionDialog::ask(
            nullptr, request(Scope::THIS_CONFIGURATION, deployment(DeployEvent::EJECTION, 200, 0))),
        std::nullopt);
}

}  // namespace

}  // namespace QtRocket::Gui
