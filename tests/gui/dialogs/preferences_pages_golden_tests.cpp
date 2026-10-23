// The pages of the preferences dialog against OpenRocket's own (GeneralPreferencesPanel,
// DesignPreferencesPanel, UnitsPreferencesPanel), item by item: their texts, tooltips, states,
// the combo boxes' items and current items, for a store that was just reset, and the Units page
// after its steps. The expectations are the transcript of PreferencesProbe.java run on OpenRocket
// (PreferencesPagePins.h); the items QtRocket leaves out are named where they are left out.

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <QCheckBox>
#include <QPushButton>
#include <QWidget>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/dialogs/DefaultUnitSelector.h"
#include "QtRocket/gui/dialogs/DesignPreferencesPanel.h"
#include "QtRocket/gui/dialogs/GeneralPreferencesPanel.h"
#include "QtRocket/gui/dialogs/PreferencesStore.h"
#include "QtRocket/gui/dialogs/UnitsPreferencesPanel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/unit/UnitGroup.h"
#include "ScriptedUserPrompts.h"
#include "dialogs/PreferencesPagePins.h"
#include "dialogs/PreferencesPageTranscript.h"

namespace QtRocket::Gui
{

namespace
{

namespace Pins = Gui::Test::PreferencesPagePins;
using Gui::Test::pageTranscript;
using Gui::Test::transcriptKeeping;
using Gui::Test::transcriptOf;
using Gui::Test::transcriptWithout;

/// The units the combo boxes of @p page show, in their order.
[[nodiscard]] std::vector<std::string> shownUnits(const UnitsPreferencesPanel& page)
{
    std::vector<std::string> units;
    for (const DefaultUnitSelector* selector : page.unitSelectors())
    {
        units.push_back(QtStrings::toUtf8(selector->currentText()));
    }
    return units;
}

/// The probe's store: cleared (SwingPreferences.clearPreferences(): the units node alone).
class PreferencesPagesGolden : public Gui::Test::GuiTest
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        PreferencesStore::clearPreferences(m_preferences);
    }

    InMemoryPreferences m_preferences;
};

TEST(PreferencesPagePinsGolden, ComeFromTheGoldensOpenRocket)
{
    EXPECT_EQ(Pins::kOpenRocketCommit, "5f164fd0e05d6e188857b0185601293b7a6e9eaf");
}

TEST_F(PreferencesPagesGolden, TheUnitsPageIsOpenRockets)
{
    const UnitsPreferencesPanel page(m_preferences, nullptr);
    EXPECT_EQ(pageTranscript(page), transcriptOf(Pins::kUnitsPage));
}

TEST_F(PreferencesPagesGolden, TheDesignPageIsOpenRocketsWithoutTheItemsOfMilestone4)
{
    QWidget                      root;
    BindingScope                 scope(root, nullptr);
    const DesignPreferencesPanel page(m_preferences, scope, &root);
    // The simulations' discard confirmation and the design view's flight estimate come with the
    // simulations (Milestone 4).
    constexpr std::array<std::string_view, 2> kLater{
        "CHECK Show confirmation dialog for discarding simulation changes ",
        "CHECK Update estimated flight parameters in design window "};
    EXPECT_EQ(pageTranscript(page), transcriptWithout(Pins::kDesignPage, kLater));
}

TEST_F(PreferencesPagesGolden, TheGeneralPageIsOpenRocketsItemsOfMilestone2)
{
    Gui::Test::ScriptedUserPrompts prompts;
    const GeneralPreferencesPanel  page(m_preferences, prompts, nullptr);
    // The rest (GeneralPreferencesPanel's class comment): the language (QtRocket is English
    // only), the thrust curve and preset paths (Milestone 3), the update checks (none in
    // QtRocket), the RASAero and RockSim warnings (no RASAero; RockSim in Milestone 8), the parts
    // library (Milestone 3), the import and export of OpenRocket's preference files, and the
    // separators between them.
    constexpr std::array<std::string_view, 3> kMilestone2{
        "CHECK Open last design file on startup ",
        "CHECK Show confirmation dialog when discarding preferences ",
        "BUTTON Reset all preferences "};
    EXPECT_EQ(pageTranscript(page), transcriptKeeping(Pins::kGeneralPage, kMilestone2));
}

TEST_F(PreferencesPagesGolden, TheUnitsPageAfterTheProbesSteps)
{
    const UnitsPreferencesPanel page(m_preferences, nullptr);
    // Rocket dimensions: the last unit (ft), at once the default, stored only by Apply or OK.
    DefaultUnitSelector* const length = page.unitSelector(UnitGroupId::LENGTH);
    ASSERT_NE(length, nullptr);
    Gui::Test::chooseItem(*length, length->count() - 1);
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getDefaultUnit().getUnit(), "ft");
    EXPECT_EQ(m_preferences.getNode("units").get("LENGTH"), "cm");

    page.defaultImperialButton()->click();
    EXPECT_EQ(shownUnits(page), transcriptOf(Pins::kDefaultImperialUnits));
    page.defaultMetricButton()->click();
    EXPECT_EQ(shownUnits(page), transcriptOf(Pins::kDefaultMetricUnits));

    // Display secondary stability unit, off and on again.
    page.displaySecondaryStabilityCheckBox()->click();
    EXPECT_FALSE(page.unitSelector(UnitGroupId::SECONDARY_STABILITY)->isEnabled());
    EXPECT_FALSE(m_preferences.isDisplaySecondaryStability());
    page.displaySecondaryStabilityCheckBox()->click();
    EXPECT_TRUE(page.unitSelector(UnitGroupId::SECONDARY_STABILITY)->isEnabled());
    EXPECT_TRUE(m_preferences.isDisplaySecondaryStability());

    EXPECT_EQ(pageTranscript(page), transcriptOf(Pins::kUnitsPageAfterSteps));
    UnitGroup::resetDefaultUnits();
}

}  // namespace

}  // namespace QtRocket::Gui
