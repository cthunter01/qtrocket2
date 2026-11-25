// The Launch page of the preferences dialog (OpenRocket's LaunchPreferencesPanel): the port of
// LaunchPreferencesPanelTest.testPanelCreation (its 13 DefaultsFromLaunchPreferences cases are
// ported in the core, tests/core/simulation/LaunchPreferencesTests.cpp), and what the preferences
// family's probe shows and does on OpenRocket's (PreferencesProbe.java, PreferencesPagePins.h:
// kLaunchPage*). The conditions themselves are SimulationConditionsPanel's, whose tests pin the
// rest (SimulationConditionsPanelTests.cpp).

#include "QtRocket/gui/dialogs/LaunchPreferencesPanel.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <QString>
#include <QWidget>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/dialogs/PreferencesStore.h"
#include "QtRocket/gui/dialogs/simulation/SimulationConditionsPanel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/simulation/LaunchPreferences.h"
#include "QtRocket/util/BugError.h"
#include "ScriptedUserPrompts.h"
#include "dialogs/PreferencesPagePins.h"
#include "dialogs/PreferencesPageTranscript.h"
#include "dialogs/simulation/SimulationConditionsPins.h"
#include "dialogs/simulation/SimulationDialogWalk.h"

namespace QtRocket::Gui
{

namespace
{

using LaunchPreferencesPanelTest = Gui::Test::GuiTest;
using Row                        = ConfigFields::NumberWidgets;
namespace Pins                   = Gui::Test::PreferencesPagePins;
namespace Keys                   = PreferenceKeys;
using Gui::Test::expectSameLines;
using Gui::Test::storedDouble;
using Gui::Test::transcriptOf;

/// The page over a store just cleared (the probe's clearPreferences()), with the launch
/// preferences made for it (as the dialog makes them), its bindings kept by a scope released
/// before the page goes.
class LaunchPage
{
public:
    LaunchPage()
    {
        PreferencesStore::clearPreferences(m_preferences);
        m_launch = std::make_shared<LaunchPreferences>(m_preferences);
        m_page   = std::make_unique<LaunchPreferencesPanel>(m_launch, m_prompts, m_scope, &m_root);
    }
    ~LaunchPage()
    {
        m_scope.release();
        m_page.reset();
    }
    LaunchPage(const LaunchPage&)            = delete;
    LaunchPage& operator=(const LaunchPage&) = delete;
    LaunchPage(LaunchPage&&)                 = delete;
    LaunchPage& operator=(LaunchPage&&)      = delete;

    [[nodiscard]] LaunchPreferencesPanel& page() const noexcept { return *m_page; }
    [[nodiscard]] InMemoryPreferences&    preferences() noexcept { return m_preferences; }

private:
    InMemoryPreferences                     m_preferences;
    Gui::Test::ScriptedUserPrompts          m_prompts;
    std::shared_ptr<LaunchPreferences>      m_launch;
    QWidget                                 m_root;
    BindingScope                            m_scope{m_root, nullptr};
    std::unique_ptr<LaunchPreferencesPanel> m_page;
};

/// The probe's typed number row, up to its stored keys: "<name> typed=[<text>] | [<shown>] | ".
[[nodiscard]] std::string typed(const Row& row, std::string_view name, std::string_view text)
{
    static_cast<void>(Gui::Test::enterText(*row.spinBox, text));
    return std::string(name) + " typed=[" + std::string(text) + "] | [" +
           QtStrings::toUtf8(row.spinBox->text()) + "] | ";
}

/// The three wind keys as the probe prints them.
[[nodiscard]] std::string windKeys(const Preferences& preferences)
{
    return storedDouble(preferences, Keys::kWindAverage, 2.0) + " " +
           storedDouble(preferences, Keys::kWindTurbulence, 0.1) + " " +
           storedDouble(preferences, Keys::kWindDirection, 1.5707963267948966);
}

TEST_F(LaunchPreferencesPanelTest, PanelCreation)
{
    // Port of LaunchPreferencesPanelTest.testPanelCreation: "The panel can be built on top of the
    // launch preferences".
    const LaunchPage page;
    EXPECT_EQ(page.page().conditions().variant(),
              SimulationConditionsPanel::Variant::LAUNCH_PREFERENCES);
    EXPECT_EQ(page.page().conditions().parentWidget(), &page.page());
    EXPECT_NE(page.page().launchPreferences(), nullptr);
}

TEST_F(LaunchPreferencesPanelTest, ShowsWhatOpenRocketsPageShows)
{
    const LaunchPage page;
    expectSameLines(Gui::Test::dialogWalk(page.page()), transcriptOf(Pins::kLaunchPageWalk));
    // The conditions below the NOTE are those the conditions probe saw on OpenRocket's Launch page
    // (tools/openrocket-probes/simulation-conditions).
    std::vector<std::string> conditions = transcriptOf(Pins::kLaunchPageWalk);
    conditions.erase(conditions.begin());
    expectSameLines(conditions, Gui::Test::linesWithPrefix(
                                    Gui::Test::SimulationConditionsPins::kLaunchPage, "WALK "));
    const StyledLabel& warning = page.page().launchWarningLabel();
    EXPECT_TRUE(warning.font().bold());
}

TEST_F(LaunchPreferencesPanelTest, DoesWhatOpenRocketsPageDoes)
{
    LaunchPage                       page;
    const SimulationConditionsPanel& conditions  = page.page().conditions();
    const Preferences&               preferences = page.preferences();
    std::vector<std::string>         lines;
    // The launch preferences loaded and stored the average wind; the rod direction follows the
    // wind (Java's getter stores the default it reads, here the absent key shows it).
    lines.push_back("made | spinners " +
                    std::to_string(page.page().findChildren<UnitSpinBox*>().size()) + " | " +
                    windKeys(preferences) + " " +
                    storedDouble(preferences, Keys::kLaunchRodDirection, 1.5707963267948966));
    // Typed first, then the keys read (the operands of + are not sequenced).
    const std::string wind = typed(conditions.averageWind(), "average wind", "5");
    lines.push_back(wind + windKeys(preferences));
    const std::string rod = typed(conditions.rodLength(), "rod length", "150");
    lines.push_back(rod + storedDouble(preferences, Keys::kLaunchRodLength, 1.0));
    const std::string latitude = typed(conditions.latitude(), "latitude", "45");
    lines.push_back(latitude + storedDouble(preferences, Keys::kLaunchLatitude, 28.61));
    expectSameLines(lines, transcriptOf(Pins::kLaunchPageSteps));
}

TEST_F(LaunchPreferencesPanelTest, RefusesNoLaunchPreferences)
{
    Gui::Test::ScriptedUserPrompts prompts;
    QWidget                        root;
    BindingScope                   scope(root, nullptr);
    EXPECT_THROW(static_cast<void>(LaunchPreferencesPanel(nullptr, prompts, scope, &root)),
                 BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
