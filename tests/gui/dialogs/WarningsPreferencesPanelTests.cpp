// The Warnings page of the preferences dialog (OpenRocket's WarningsPreferencesPanel): the port of
// WarningsPreferencesPanelTest, and what the preferences family's probe shows and does on
// OpenRocket's (PreferencesProbe.java, PreferencesPagePins.h: kWarningsPage*), with the edits of
// QtRocket's deliberate differences named where they are made.

#include "QtRocket/gui/dialogs/WarningsPreferencesPanel.h"

#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QString>
#include <QToolButton>
#include <QWidget>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/dialogs/PreferencesStore.h"
#include "QtRocket/gui/dialogs/SimulationPreferencesPanel.h"
#include "QtRocket/gui/dialogs/simulation/RecoveryWarningsPanel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/CollapsiblePanel.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/simulation/DefaultSimulationOptionFactory.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "dialogs/PreferencesPagePins.h"
#include "dialogs/PreferencesPageTranscript.h"
#include "dialogs/simulation/SimulationDialogWalk.h"

namespace QtRocket::Gui
{

namespace
{

using WarningsPreferencesPanelTest = Gui::Test::GuiTest;
using Threshold                    = RecoveryWarningsPanel::Threshold;
namespace Pins                     = Gui::Test::PreferencesPagePins;
namespace Keys                     = PreferenceKeys;
using Gui::Test::expectSameLines;
using Gui::Test::replacedIn;
using Gui::Test::storedDouble;
using Gui::Test::transcriptOf;

/// The page over a store just cleared (the probe's clearPreferences()), its bindings kept by a
/// scope released before the page goes.
class WarningsPage
{
public:
    WarningsPage()
    {
        PreferencesStore::clearPreferences(m_preferences);
        m_page = std::make_unique<WarningsPreferencesPanel>(m_preferences, m_scope, &m_root);
    }
    ~WarningsPage()
    {
        m_scope.release();
        m_page.reset();
    }
    WarningsPage(const WarningsPage&)            = delete;
    WarningsPage& operator=(const WarningsPage&) = delete;
    WarningsPage(WarningsPage&&)                 = delete;
    WarningsPage& operator=(WarningsPage&&)      = delete;

    [[nodiscard]] WarningsPreferencesPanel& page() const noexcept { return *m_page; }
    [[nodiscard]] InMemoryPreferences&      preferences() noexcept { return m_preferences; }

private:
    InMemoryPreferences                       m_preferences;
    QWidget                                   m_root;
    BindingScope                              m_scope{m_root, nullptr};
    std::unique_ptr<WarningsPreferencesPanel> m_page;
};

/// The pinned walk @p pins with QtRocket's differences: the sections' headers draw the style's
/// disclosure arrow, not an icon (CollapsiblePanel, K3's deviation), and the @p descriptions
/// descriptions shown are word-wrapped labels (the walk's TEXTAREA) of Java's HTML.
[[nodiscard]] std::vector<std::string> expectedWalk(std::span<const std::string_view> pins,
                                                    int                               descriptions)
{
    std::vector<std::string> lines = transcriptOf(pins);
    lines = replacedIn(std::move(lines), "BUTTON Single Deployment Warnings icon",
                       "BUTTON Single Deployment Warnings", 1);
    lines = replacedIn(std::move(lines), "BUTTON Dual Deployment Warnings icon",
                       "BUTTON Dual Deployment Warnings", 1);
    return replacedIn(std::move(lines),
                      "LABEL <html><body style=", "TEXTAREA <html><body style=", descriptions);
}

/// The probe's "[<text>]" of a threshold's spin box.
[[nodiscard]] std::string spinnerText(const Threshold& threshold)
{
    return "[" + QtStrings::toUtf8(threshold.spinBox->text()) + "]";
}

/// The number of the page's spin boxes that would be visible were it shown (Java's find(): the
/// collapsed section's are removed there, hidden here).
[[nodiscard]] int visibleSpinBoxes(const QWidget& page)
{
    int count = 0;
    for (const UnitSpinBox* const spinBox : page.findChildren<UnitSpinBox*>())
    {
        count += spinBox->isVisibleTo(&page) ? 1 : 0;
    }
    return count;
}

/// The probe's steps of a threshold: @p typed texts, each with the field and the stored key.
void typeThreshold(std::vector<std::string>& lines, std::string_view name,
                   const Threshold& threshold, std::initializer_list<std::string_view> typed,
                   const Preferences& preferences, std::string_view key, double javaDefault)
{
    for (const std::string_view text : typed)
    {
        static_cast<void>(Gui::Test::enterText(*threshold.spinBox, text));
        lines.push_back(std::string(name) + " typed=[" + std::string(text) + "] | " +
                        spinnerText(threshold) + " | " +
                        storedDouble(preferences, key, javaDefault));
    }
}

TEST_F(WarningsPreferencesPanelTest, RecoveryThresholdsHaveDedicatedPreferencesTab)
{
    // Port of WarningsPreferencesPanelTest.testRecoveryThresholdsHaveDedicatedPreferencesTab.
    InMemoryPreferences              preferences;
    QWidget                          root;
    BindingScope                     scope(root, nullptr);
    const SimulationPreferencesPanel simulationPanel(preferences, scope, &root);
    const WarningsPreferencesPanel   warningsPanel(preferences, scope, &root);
    EXPECT_TRUE(simulationPanel.findChildren<RecoveryWarningsPanel*>().isEmpty());
    EXPECT_EQ(warningsPanel.findChildren<RecoveryWarningsPanel*>().size(), 1);
    scope.release();
}

TEST_F(WarningsPreferencesPanelTest, ShowsAndDoesWhatOpenRocketsPageDoes)
{
    WarningsPage page;
    expectSameLines(Gui::Test::dialogWalk(page.page()), expectedWalk(Pins::kWarningsPageWalk, 1));

    const RecoveryWarningsPanel& recovery    = page.page().recoveryWarnings();
    const Preferences&           preferences = page.preferences();
    std::vector<std::string>     lines;
    lines.push_back(
        "sections 2 expanded=" +
        std::string(recovery.singleDeploymentSection().isExpanded() ? "true" : "false") + "," +
        (recovery.dualDeploymentSection().isExpanded() ? "true" : "false") +
        " spinners=" + std::to_string(visibleSpinBoxes(page.page())));
    typeThreshold(lines, "single high", recovery.highSpeedThreshold(), {"25", "-5", "20", "37"},
                  preferences, Keys::kRecoverySpeedWarning, 20.0);
    recovery.dualDeploymentSection().headerButton()->click();
    lines.push_back("expand dual | expanded=" +
                    std::string(recovery.dualDeploymentSection().isExpanded() ? "true" : "false") +
                    " spinners=" + std::to_string(visibleSpinBoxes(page.page())));
    typeThreshold(lines, "dual low", recovery.dualLowSpeedThreshold(), {"10", "-1", "12.5"},
                  preferences, Keys::kRecoveryDrogueMainLowSpeedWarning, 15.24);
    typeThreshold(lines, "dual high", recovery.dualHighSpeedThreshold(), {"40", "0", "45"},
                  preferences, Keys::kRecoveryDrogueMainHighSpeedWarning, 30.48);
    expectSameLines(lines, transcriptOf(Pins::kWarningsPageSteps));

    expectSameLines(Gui::Test::dialogWalk(page.page()),
                    expectedWalk(Pins::kWarningsPageWalkExpanded, 3));
    recovery.singleDeploymentSection().headerButton()->click();
    expectSameLines(Gui::Test::dialogWalk(page.page()),
                    expectedWalk(Pins::kWarningsPageWalkSingleCollapsed, 2));
}

TEST_F(WarningsPreferencesPanelTest, NewSimulationsTakeTheThresholds)
{
    // OpenRocket bug 38, fixed in the core (DefaultSimulationOptionFactory::getDefault()): the
    // page's thresholds reach a new simulation's options.
    WarningsPage                 page;
    const RecoveryWarningsPanel& recovery = page.page().recoveryWarnings();
    static_cast<void>(Gui::Test::enterText(*recovery.highSpeedThreshold().spinBox, "25"));
    static_cast<void>(Gui::Test::enterText(*recovery.dualLowSpeedThreshold().spinBox, "12"));
    static_cast<void>(Gui::Test::enterText(*recovery.dualHighSpeedThreshold().spinBox, "35"));
    const SimulationOptions defaults =
        DefaultSimulationOptionFactory(page.preferences()).getDefault();
    EXPECT_EQ(defaults.getRecoverySpeedWarning(), 25.0);
    EXPECT_EQ(defaults.getRecoveryDrogueMainLowSpeedWarning(), 12.0);
    EXPECT_EQ(defaults.getRecoveryDrogueMainHighSpeedWarning(), 35.0);
}

}  // namespace

}  // namespace QtRocket::Gui
