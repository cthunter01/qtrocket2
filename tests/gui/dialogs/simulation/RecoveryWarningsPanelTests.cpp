// The editor of the recovery warnings' thresholds (OpenRocket's RecoveryWarningsPanel): the port of
// RecoveryWarningsPanelTest.testThresholdEditorsUpdateSimulationOptions, the editor over the
// preferences, its bounds and its descriptions. The tests of the Warnings tab around it
// (SimulationWarningsPanelTests.cpp) port the rest of RecoveryWarningsPanelTest.

#include "QtRocket/gui/dialogs/simulation/RecoveryWarningsPanel.h"

#include <string_view>

#include <QLabel>
#include <QLayout>
#include <QList>
#include <QSize>
#include <QString>
#include <QTabWidget>
#include <QToolButton>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/CollapsiblePanel.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/simulation/SimulationInputRanges.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "dialogs/simulation/SimulationDialogWalk.h"

namespace QtRocket::Gui
{

namespace
{

using RecoveryWarningsPanelTest = Gui::Test::GuiTest;
using Threshold                 = RecoveryWarningsPanel::Threshold;

/// The spin boxes of @p panel that would be visible were it shown (Java's findComponents(panel,
/// JSpinner.class): a collapsed section's are removed there, hidden here: K3's deviation).
[[nodiscard]] QList<UnitSpinBox*> visibleSpinBoxes(const QWidget& panel)
{
    QList<UnitSpinBox*> shown;
    for (UnitSpinBox* const spinBox : panel.findChildren<UnitSpinBox*>())
    {
        if (spinBox->isVisibleTo(&panel))
        {
            shown.append(spinBox);
        }
    }
    return shown;
}

/// Java's findComponents(panel, JToggleButton.class): the checkable tool buttons of @p panel (the
/// unit selectors are tool buttons too).
[[nodiscard]] QList<QToolButton*> toggleButtons(const QWidget& panel)
{
    QList<QToolButton*> buttons;
    for (QToolButton* const button : panel.findChildren<QToolButton*>())
    {
        if (button->isCheckable())
        {
            buttons.append(button);
        }
    }
    return buttons;
}

/// @p label is a word-wrapped label of rich text (Java's HTML JLabel that wraps).
void expectWrappingRichText(const QLabel& label)
{
    EXPECT_TRUE(label.wordWrap());
    EXPECT_EQ(label.textFormat(), Qt::RichText);
}

/// Java's spinner.setValue("<text>"): the text in the current unit.
void enter(const Threshold& threshold, std::string_view text)
{
    static_cast<void>(Gui::Test::enterText(*threshold.spinBox, text));
}

TEST_F(RecoveryWarningsPanelTest, ThresholdEditorsUpdateSimulationOptions)
{
    // Port of RecoveryWarningsPanelTest.testThresholdEditorsUpdateSimulationOptions.
    SimulationOptions           options;
    QWidget                     root;
    BindingScope                scope(root, nullptr);
    const RecoveryWarningsPanel panel(options, scope, &root);
    // "Recovery configuration should use collapsible sections instead of tabs"
    EXPECT_TRUE(panel.findChildren<QTabWidget*>().isEmpty());

    const QList<CollapsiblePanel*> sections = panel.findChildren<CollapsiblePanel*>();
    ASSERT_EQ(sections.size(), 2);
    EXPECT_EQ(sections.at(0), &panel.singleDeploymentSection());
    EXPECT_EQ(sections.at(1), &panel.dualDeploymentSection());
    EXPECT_TRUE(sections.at(0)->isExpanded());
    EXPECT_FALSE(sections.at(1)->isExpanded());
    const QList<QToolButton*> sectionButtons = toggleButtons(panel);
    ASSERT_EQ(sectionButtons.size(), 2);
    // Java: isRolloverEnabled(), getIcon() and getSelectedIcon(); here the tool button's hover
    // look (auto-raise) and its disclosure arrow, right collapsed and down expanded.
    EXPECT_TRUE(sectionButtons.at(0)->autoRaise());
    EXPECT_EQ(sectionButtons.at(0)->arrowType(), Qt::DownArrow);
    EXPECT_EQ(sectionButtons.at(1)->arrowType(), Qt::RightArrow);

    QList<UnitSpinBox*> thresholdSpinners = visibleSpinBoxes(panel);
    // "Collapsed content should be removed from the component hierarchy"
    ASSERT_EQ(thresholdSpinners.size(), 1);
    static_cast<void>(Gui::Test::enterText(*thresholdSpinners.at(0), "37"));
    EXPECT_NEAR(options.getRecoverySpeedWarning(), 37.0, 0.0001);

    sectionButtons.at(1)->click();
    EXPECT_TRUE(sections.at(1)->isExpanded());
    thresholdSpinners = visibleSpinBoxes(panel);
    ASSERT_EQ(thresholdSpinners.size(), 3);
    QWidget* const dualDeploymentPanel = thresholdSpinners.at(1)->parentWidget();
    ASSERT_NE(dualDeploymentPanel, nullptr);
    dualDeploymentPanel->resize(1000, dualDeploymentPanel->sizeHint().height());
    dualDeploymentPanel->layout()->activate();
    // "Dual-deployment thresholds should share one compact row"
    EXPECT_EQ(thresholdSpinners.at(1)->y(), thresholdSpinners.at(2)->y());

    sectionButtons.at(1)->click();
    EXPECT_FALSE(sections.at(1)->isExpanded());
    // "Collapsing should remove the dual deployment controls from layout"
    EXPECT_EQ(visibleSpinBoxes(panel).size(), 1);
}

TEST_F(RecoveryWarningsPanelTest, ShowsJavasSectionsDescriptionsAndThresholds)
{
    SimulationOptions           options;
    QWidget                     root;
    BindingScope                scope(root, nullptr);
    const RecoveryWarningsPanel panel(options, scope, &root);
    EXPECT_EQ(panel.singleDeploymentSection().headerButton()->text(),
              QStringLiteral("Single Deployment Warnings"));
    EXPECT_EQ(panel.dualDeploymentSection().headerButton()->text(),
              QStringLiteral("Dual Deployment Warnings"));
    EXPECT_EQ(panel.singleDescription().text(),
              QStringLiteral("<html><body style='margin:0;font-style:italic;'>Warn when a recovery "
                             "device deploys at an unsafe speed. Warnings apply independently by "
                             "stage.</body></html>"));
    EXPECT_EQ(panel.dualDescription().text(), panel.singleDescription().text());
    EXPECT_EQ(panel.dualHowTo().text(),
              QStringLiteral("<html><body style='margin:0;'>Mark a recovery device as a drogue in "
                             "the Recovery tab of a Stage configuration dialog to enable "
                             "dual-deployment warnings.</body></html>"));
    expectWrappingRichText(panel.singleDescription());
    expectWrappingRichText(panel.dualDescription());
    expectWrappingRichText(panel.dualHowTo());

    const QString highTip = QStringLiteral("Warn when a recovery device deploys above this speed.");
    const Threshold& high = panel.highSpeedThreshold();
    EXPECT_EQ(high.label->text(), QStringLiteral("High Speed Threshold"));
    EXPECT_EQ(high.label->toolTip(), highTip);
    EXPECT_EQ(high.spinBox->toolTip(), highTip);
    EXPECT_EQ(high.unitSelector->toolTip(), QString());
    EXPECT_EQ(high.spinBox->objectName(), QStringLiteral("RecoverySpeedWarning"));
    EXPECT_EQ(high.unitSelector->objectName(), QStringLiteral("RecoverySpeedWarning"));
    EXPECT_EQ(high.slider, nullptr);
    EXPECT_EQ(high.spinBox->text(), QStringLiteral("20"));
    EXPECT_EQ(high.unitSelector->text(), QStringLiteral("m/s"));

    const Threshold& low = panel.dualLowSpeedThreshold();
    EXPECT_EQ(low.label->text(), QStringLiteral("Low Speed Threshold"));
    EXPECT_EQ(low.spinBox->toolTip(),
              QStringLiteral("Warn when a recovery device deploys below this speed."));
    EXPECT_EQ(low.spinBox->objectName(), QStringLiteral("RecoveryDrogueMainLowSpeedWarning"));
    EXPECT_EQ(low.spinBox->text(), QStringLiteral("15.2"));
    const Threshold& dualHigh = panel.dualHighSpeedThreshold();
    EXPECT_EQ(dualHigh.label->text(), QStringLiteral("High Speed Threshold"));
    EXPECT_EQ(dualHigh.spinBox->toolTip(), highTip);
    EXPECT_EQ(dualHigh.spinBox->objectName(), QStringLiteral("RecoveryDrogueMainHighSpeedWarning"));
    EXPECT_EQ(dualHigh.spinBox->text(), QStringLiteral("30.5"));
}

TEST_F(RecoveryWarningsPanelTest, EditsThePreferencesOnTheWarningsPage)
{
    InMemoryPreferences         preferences;
    QWidget                     root;
    BindingScope                scope(root, nullptr);
    const RecoveryWarningsPanel panel(preferences, scope, &root);
    enter(panel.highSpeedThreshold(), "25");
    enter(panel.dualLowSpeedThreshold(), "12.5");
    enter(panel.dualHighSpeedThreshold(), "40");
    EXPECT_EQ(preferences.getRecoverySpeedWarning(), 25.0);
    EXPECT_EQ(preferences.getRecoveryDrogueMainLowSpeedWarning(), 12.5);
    EXPECT_EQ(preferences.getRecoveryDrogueMainHighSpeedWarning(), 40.0);

    // A change elsewhere reaches the fields (the preferences' setters announce it).
    preferences.setRecoverySpeedWarning(33);
    EXPECT_EQ(panel.highSpeedThreshold().spinBox->text(), QStringLiteral("33"));

    // Released, the fields write nothing.
    scope.release();
    enter(panel.highSpeedThreshold(), "50");
    EXPECT_EQ(preferences.getRecoverySpeedWarning(), 33.0);
}

TEST_F(RecoveryWarningsPanelTest, TakesJavasMinimumAndTheInputRangesMaximum)
{
    SimulationOptions           options;
    QWidget                     root;
    BindingScope                scope(root, nullptr);
    const RecoveryWarningsPanel panel(options, scope, &root);
    // Java's minimum 0 (DoubleModel(..., UNITS_VELOCITY, 0)).
    enter(panel.highSpeedThreshold(), "-5");
    EXPECT_EQ(options.getRecoverySpeedWarning(), 0.0);
    enter(panel.dualLowSpeedThreshold(), "-1");
    EXPECT_EQ(options.getRecoveryDrogueMainLowSpeedWarning(), 0.0);
    // Java has no maximum; QtRocket's is the input ranges' (the user's decision U-M4-7).
    enter(panel.dualHighSpeedThreshold(), "20000");
    EXPECT_EQ(options.getRecoveryDrogueMainHighSpeedWarning(),
              SimulationInputRanges::kRecoverySpeedThreshold.max);
    enter(panel.highSpeedThreshold(), "10000");
    EXPECT_EQ(options.getRecoverySpeedWarning(), 10000.0);
}

TEST_F(RecoveryWarningsPanelTest, TheDescriptionsAreJavasWrappingHtml)
{
    // Java: createWrappingInfoText() and getHtmlContent().
    EXPECT_EQ(RecoveryWarningsPanel::wrappingInfoHtml(
                  QStringLiteral("<html>Some <b>text</b></html>"), true),
              QStringLiteral("<html><body style='margin:0;font-style:italic;'>Some "
                             "<b>text</b></body></html>"));
    // Trimmed, and the tags without regard to case.
    EXPECT_EQ(RecoveryWarningsPanel::wrappingInfoHtml(QStringLiteral("  <HTML>x</Html>\n"), false),
              QStringLiteral("<html><body style='margin:0;'>x</body></html>"));
    // Plain text is escaped.
    EXPECT_EQ(RecoveryWarningsPanel::wrappingInfoHtml(QStringLiteral("a < b & c"), false),
              QStringLiteral("<html><body style='margin:0;'>a &lt; b &amp; c</body></html>"));
    // An HTML text without its closing tag is kept as it is.
    EXPECT_EQ(RecoveryWarningsPanel::wrappingInfoHtml(QStringLiteral("<html>open"), false),
              QStringLiteral("<html><body style='margin:0;'><html>open</body></html>"));
}

TEST_F(RecoveryWarningsPanelTest, TheDescriptionsWrapToTheWidthTheyGet)
{
    SimulationOptions           options;
    QWidget                     root;
    BindingScope                scope(root, nullptr);
    const RecoveryWarningsPanel panel(options, scope, &root);
    panel.dualDeploymentSection().setExpanded(true);
    const QLabel& howTo = panel.dualHowTo();
    ASSERT_TRUE(howTo.hasHeightForWidth());
    // Narrower is taller; the panel's layout asks the height for its width.
    EXPECT_GT(howTo.heightForWidth(150), howTo.heightForWidth(1000));
    EXPECT_TRUE(panel.layout()->hasHeightForWidth());
    // "wmin 0": the descriptions leave the panel's minimum width to the thresholds' row.
    EXPECT_LT(howTo.minimumSizeHint().width(), panel.minimumSizeHint().width());
}

}  // namespace

}  // namespace QtRocket::Gui
