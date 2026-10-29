// The pages of the preferences dialog against OpenRocket's own (GeneralPreferencesPanel,
// DesignPreferencesPanel, UnitsPreferencesPanel), item by item: their texts, tooltips, states,
// the combo boxes' items and current items, for a store that was just reset, and the Units page
// after its steps. The expectations are the transcript of PreferencesProbe.java run on OpenRocket
// (PreferencesPagePins.h); the items QtRocket leaves out are named where they are left out, and
// its deliberate differences are the EDITS rows of the General page.

#include <algorithm>
#include <array>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <QCheckBox>
#include <QDir>
#include <QPushButton>
#include <QString>
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
#include "QtRocket/util/FileIo.h"
#include "ScriptedUserPrompts.h"
#include "TestTempDir.h"
#include "dialogs/PreferencesPagePins.h"
#include "dialogs/PreferencesPageTranscript.h"

namespace QtRocket::Gui
{

namespace
{

namespace Pins = Gui::Test::PreferencesPagePins;
using Gui::Test::pageTranscript;
using Gui::Test::TranscriptEdit;
using Gui::Test::transcriptEdited;
using Gui::Test::transcriptKeeping;
using Gui::Test::transcriptOf;
using Gui::Test::transcriptWithout;

/// The items of OpenRocket's General page that QtRocket has, by the start of their lines (both
/// labels of the user's files, both fields, both Add and Reset, "Reset all preferences" too).
/// The rest (GeneralPreferencesPanel's class comment): the update checks with their buttons (no
/// network in QtRocket), the RASAero and RockSim warnings (no RASAero; RockSim in Milestone 8),
/// the import and export of OpenRocket's preference files, and the four arrow buttons of the
/// description's scroll bars, which Swing's scroll pane holds ("BUTTON ").
constexpr std::array<std::string_view, 9> kGeneralPageItems{
    "LABEL User-defined ",
    "FIELD ",
    "BUTTON Add",
    "BUTTON Reset",
    "TEXT ",
    "SEPARATOR",
    "CHECK Open last design file on startup ",
    "CHECK Show confirmation dialog when discarding preferences ",
    "CHECK Automatically open the Parts Library dialog "};

/// QtRocket's deliberate differences from OpenRocket's General page.
constexpr std::array<TranscriptEdit, 3> kGeneralPageEdits{{
    // QtRocket's user data directory (the user's decision U-M3-3: QStandardPaths::
    // GenericDataLocation + "QtRocket", where the start makes the two directories), written
    // <user data> as the probe writes its home directory <home>: OpenRocket's is ~/.openrocket
    // (on Linux).
    {.openRocket = "FIELD <home>/.openrocket/ThrustCurves",
     .qtRocket   = "FIELD <user data>/ThrustCurves"},
    {.openRocket = "FIELD <home>/.openrocket/Components",
     .qtRocket   = "FIELD <user data>/Components"},
    // The description names QtRocket; QtRocket's line is the text the area was given, where
    // Swing's pane gives back its HTML document (its font colours and line breaks).
    {.openRocket =
         "TEXT <html>\n  <head>\n    <font color=\"#000000\">\n</font>  </head>\n  <body>\n    "
         "<font color=\"#000000\">Add directories, RASP motor files (*.eng), RockSim \n    engine "
         "files (*.rse), SQLite motor databases (*.db) or ZIP archives \n    separated by a "
         "semicolon (;) to load external thrust curves. Changes will \n    take effect the next "
         "time you start OpenRocket.</font>\n  </body>\n</html>\n",
     .qtRocket = "TEXT Add directories, RASP motor files (*.eng), RockSim engine files (*.rse), "
                 "SQLite motor databases (*.db) or ZIP archives separated by a semicolon (;) to "
                 "load external thrust curves.  Changes will take effect the next time you start "
                 "QtRocket."},
}};

/// OpenRocket's General page from the user's thrust curves on: the interface language, its note
/// and the separator below them are left out (QtRocket is English only).
[[nodiscard]] std::span<const std::string_view> generalPageFromThrustCurves()
{
    // Searched through a span: an array's iterator is a pointer on some platforms only.
    const std::span<const std::string_view> page(Pins::kGeneralPage);
    const auto                              first =
        std::ranges::find(page, std::string_view("LABEL User-defined thrust curves:"));
    return {first, page.end()};
}

/// @p lines with the user data directory @p userData of the page written <user data> in its
/// fields, and the platform's separators there written '/', as the probe's pins have them.
[[nodiscard]] std::vector<std::string> withUserDataDirectory(std::vector<std::string>     lines,
                                                             const std::filesystem::path& userData)
{
    const QString directory = QtStrings::fromPath(absolutePath(userData));
    for (std::string& line : lines)
    {
        if (line.starts_with("FIELD "))
        {
            QString field = QtStrings::fromUtf8(line);
            field.replace(directory, QStringLiteral("<user data>"));
            line = QtStrings::toUtf8(QDir::fromNativeSeparators(field));
        }
    }
    return lines;
}

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

TEST_F(PreferencesPagesGolden, TheGeneralPageIsOpenRocketsWithQtRocketsDirectories)
{
    // The default directories of the user's files in a user data directory of QtRocket's (one of
    // the test's: the page makes no directory).
    Gui::Test::ScriptedUserPrompts prompts;
    const QtRocket::Test::TempDir  home;
    const std::filesystem::path    userData = home.path() / "QtRocket";
    const GeneralPreferencesPanel  page(
        m_preferences, prompts,
        {.defaultUserThrustCurveDirectory = userData / "ThrustCurves",
         .defaultUserComponentDirectory   = userData / "Components",
         .openDocuments                   = {}},
        nullptr);
    EXPECT_EQ(withUserDataDirectory(pageTranscript(page), userData),
              transcriptEdited(transcriptKeeping(generalPageFromThrustCurves(), kGeneralPageItems),
                               kGeneralPageEdits));
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
