#include "QtRocket/gui/dialogs/GeneralPreferencesPanel.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QWidget>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "ForwardingUserPrompts.h"
#include "GuiTestSupport.h"
#include "HookedUserPrompts.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/FileIo.h"
#include "ScriptedUserPrompts.h"
#include "TestTempDir.h"

namespace QtRocket::Gui
{

namespace
{

using Kind      = Gui::Test::ScriptedUserPrompts::Kind;
using UserFiles = GeneralPreferencesPanel::UserFiles;
using Paths     = std::vector<std::filesystem::path>;

constexpr UserFiles kThrustCurves = UserFiles::THRUST_CURVES;
constexpr UserFiles kPresets      = UserFiles::COMPONENT_PRESETS;

/// @p file as the page shows it: absolute (File.getAbsolutePath()).
[[nodiscard]] QString shown(const std::filesystem::path& file)
{
    return QtStrings::fromPath(absolutePath(withoutRedundantSeparators(file)));
}

/// The answer of a file chooser that chose @p file in @p directory.
[[nodiscard]] UserPrompts::OpenFileAnswer chose(const std::filesystem::path& file,
                                                const std::filesystem::path& directory)
{
    return {.files = {file}, .directory = directory};
}

/// The open documents of the page: @p document.
[[nodiscard]] GeneralPreferencesPanel::OpenDocuments onlyDocument(OpenRocketDocument& document)
{
    return [&document] { return std::vector<OpenRocketDocument*>{&document}; };
}

/// The page over a store in memory, its questions answered by a script, with default directories
/// of the user's files in a user data directory of the test's (which the page never makes).
class GeneralPreferencesPanelTest : public Gui::Test::GuiTest
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        remakePage(environment());
    }

    void TearDown() override
    {
        m_page = nullptr;
        GuiTest::TearDown();
    }

    [[nodiscard]] std::filesystem::path thrustCurveDirectory() const
    {
        return m_home.path() / "QtRocket" / "ThrustCurves";
    }
    [[nodiscard]] std::filesystem::path componentDirectory() const
    {
        return m_home.path() / "QtRocket" / "Components";
    }
    /// A file or directory of the test's.
    [[nodiscard]] std::filesystem::path file(std::string_view name) const
    {
        return m_home.resolve(name);
    }

    /// The test's default directories, no open documents.
    [[nodiscard]] GeneralPreferencesPanel::Environment environment() const
    {
        return {.defaultUserThrustCurveDirectory = thrustCurveDirectory(),
                .defaultUserComponentDirectory   = componentDirectory(),
                .openDocuments                   = {}};
    }

    /// A new page in @p environment in place of the old one.
    void remakePage(GeneralPreferencesPanel::Environment environment)
    {
        m_page = nullptr;
        m_page = std::make_unique<GeneralPreferencesPanel>(m_preferences, m_prompts,
                                                           std::move(environment), nullptr);
    }

    [[nodiscard]] QLineEdit& field(UserFiles files) const { return *m_page->userFilesField(files); }
    void clickAdd(UserFiles files) const { m_page->addUserFilesButton(files)->click(); }
    void clickReset(UserFiles files) const { m_page->resetUserFilesButton(files)->click(); }

    /// The last request of a file chooser.
    [[nodiscard]] UserPrompts::OpenFileRequest lastRequest() const
    {
        return m_prompts.openFileRequests().empty() ? UserPrompts::OpenFileRequest{}
                                                    : m_prompts.openFileRequests().back();
    }

    QtRocket::Test::TempDir                  m_home;
    InMemoryPreferences                      m_preferences;
    Gui::Test::ScriptedUserPrompts           m_prompts;
    std::unique_ptr<GeneralPreferencesPanel> m_page;
};

TEST_F(GeneralPreferencesPanelTest, TheCheckBoxesStoreTheirStates)
{
    // PreferencesProbe: each click stores the new state.
    EXPECT_FALSE(m_page->openLastDesign()->isChecked());
    m_page->openLastDesign()->click();
    EXPECT_EQ(m_preferences.get(PreferenceKeys::kAutoOpenLastDesign), "true");
    EXPECT_TRUE(m_page->showDiscardConfirmation()->isChecked());
    m_page->showDiscardConfirmation()->click();
    EXPECT_EQ(m_preferences.get(PreferenceKeys::kShowDiscardPreferencesConfirmation), "false");
    // SwingPreferences.isAutoOpenPartsLibrary(): on by default.
    EXPECT_TRUE(m_page->autoOpenPartsLibrary()->isChecked());
    m_page->autoOpenPartsLibrary()->click();
    EXPECT_EQ(m_preferences.get(GuiPreferenceKeys::kAutoOpenPartsLibrary), "false");
    EXPECT_EQ(m_page->openLastDesign()->objectName(), QStringLiteral("AutoOpenLastDesign"));
    EXPECT_EQ(m_page->showDiscardConfirmation()->objectName(),
              QStringLiteral("IgnoreDiscardPreferencesWarning"));
    EXPECT_EQ(m_page->autoOpenPartsLibrary()->objectName(), QStringLiteral("AutoOpenPartsLibrary"));
}

TEST_F(GeneralPreferencesPanelTest, ThePartsLibraryBoxShowsTheStoredStateAndHasJavasTooltip)
{
    m_preferences.putBoolean(GuiPreferenceKeys::kAutoOpenPartsLibrary, false);
    remakePage(environment());
    EXPECT_FALSE(m_page->autoOpenPartsLibrary()->isChecked());
    EXPECT_EQ(m_page->autoOpenPartsLibrary()->text(),
              QStringLiteral("Automatically open the Parts Library dialog"));
    EXPECT_EQ(m_page->autoOpenPartsLibrary()->toolTip(),
              QStringLiteral("<html>If checked, the Parts Library dialog will automatically open "
                             "when you add a new part,<br>unless you already disabled it in the "
                             "Parts Library dialog.</html>"));
}

TEST_F(GeneralPreferencesPanelTest, TheFieldsShowTheDefaultDirectoriesWhileTheListsAreUnset)
{
    // getUserThrustCurveFilesAsString(), getUserComponentPresetFilesAsString(); Java's fields get
    // their texts before their listeners, so showing them stores nothing.
    EXPECT_EQ(field(kThrustCurves).text(), shown(thrustCurveDirectory()));
    EXPECT_EQ(field(kPresets).text(), shown(componentDirectory()));
    EXPECT_EQ(m_preferences.get(PreferenceKeys::kUserThrustCurvesKey), std::nullopt);
    EXPECT_EQ(m_preferences.get(PreferenceKeys::kUserComponentPresetsKey), std::nullopt);
    // The page makes no directory (the application's start does).
    EXPECT_FALSE(std::filesystem::exists(thrustCurveDirectory()));
    EXPECT_FALSE(std::filesystem::exists(componentDirectory()));
}

TEST_F(GeneralPreferencesPanelTest, TheBlocksAreJavas)
{
    EXPECT_EQ(field(kThrustCurves).objectName(), QStringLiteral("UserThrustCurves"));
    EXPECT_EQ(field(kPresets).objectName(), QStringLiteral("UserComponentPresets"));
    EXPECT_EQ(m_page->addUserFilesButton(kThrustCurves)->text(), QStringLiteral("Add"));
    EXPECT_EQ(m_page->resetUserFilesButton(kThrustCurves)->text(), QStringLiteral("Reset"));
    EXPECT_EQ(m_page->addUserFilesButton(kPresets)->text(), QStringLiteral("Add"));
    EXPECT_EQ(m_page->resetUserFilesButton(kPresets)->text(), QStringLiteral("Reset"));
    // DescriptionArea(text, 3, -1.5f, false), naming QtRocket.
    const DescriptionArea& description = *m_page->userThrustCurvesDescription();
    EXPECT_EQ(description.description(),
              QStringLiteral("Add directories, RASP motor files (*.eng), RockSim engine files "
                             "(*.rse), SQLite motor databases (*.db) or ZIP archives separated by "
                             "a semicolon (;) to load external thrust curves.  Changes will take "
                             "effect the next time you start QtRocket."));
    EXPECT_EQ(description.textSize(), -1.5);
}

TEST_F(GeneralPreferencesPanelTest, AFieldShowsTheStoredListJoinedBySemicolons)
{
    m_preferences.setUserThrustCurveFiles(Paths{file("a.eng"), file("motors")});
    // Stored empty: no file at all.
    m_preferences.setUserComponentPresetFiles(Paths{});
    remakePage(environment());
    EXPECT_EQ(field(kThrustCurves).text(),
              shown(file("a.eng")) + QStringLiteral(";") + shown(file("motors")));
    EXPECT_EQ(field(kPresets).text(), QString());
}

TEST_F(GeneralPreferencesPanelTest, TypingInAFieldStoresItsList)
{
    // The DocumentListener: split at ';', each part trimmed, the empty ones skipped.
    field(kThrustCurves).clear();
    EXPECT_EQ(m_preferences.get(PreferenceKeys::kUserThrustCurvesKey), "");
    QTest::keyClicks(&field(kThrustCurves), QStringLiteral(" ") + shown(file("a.eng")) +
                                                QStringLiteral(" ;; ") + shown(file("motors")) +
                                                QStringLiteral(" "));
    EXPECT_EQ(m_preferences.getUserThrustCurveFiles(thrustCurveDirectory()),
              (Paths{file("a.eng"), file("motors")}));
    field(kPresets).setText(shown(file("b.orc")));
    EXPECT_EQ(m_preferences.getUserComponentPresetFiles(componentDirectory()),
              (Paths{file("b.orc")}));
}

TEST_F(GeneralPreferencesPanelTest, AddAsksForAFileOrADirectoryAndAppendsIt)
{
    m_prompts.answerOpenFiles(chose(file("motors"), m_home.path()));
    clickAdd(kThrustCurves);
    // JFileChooser.showDialog(panel, "Add") in FILES_AND_DIRECTORIES mode, without the
    // accept-all filter, in the default directory (none: the home directory).
    ASSERT_EQ(m_prompts.openFileRequests().size(), 1U);
    const UserPrompts::OpenFileRequest request = lastRequest();
    EXPECT_EQ(request.title, QStringLiteral("Add"));
    EXPECT_EQ(request.nameFilters,
              (QStringList{QStringLiteral("All thrust curve files (*.eng; *.rse; *.zip; *.db; "
                                          "directories) (*.eng *.rse *.zip *.db)"),
                           QStringLiteral("RASP motor files (*.eng)"),
                           QStringLiteral("RockSim engine files (*.rse)"),
                           QStringLiteral("ZIP archives (*.zip)")}));
    EXPECT_EQ(request.selectedNameFilter, request.nameFilters.front());
    EXPECT_TRUE(request.directory.empty());
    EXPECT_FALSE(request.multipleFiles);
    EXPECT_EQ(request.selection, UserPrompts::OpenFileSelection::FILES_AND_DIRECTORIES);
    EXPECT_EQ(request.approveButtonText, QStringLiteral("Add"));
    // Appended after a ';' and stored; the chooser's directory is the default directory.
    EXPECT_EQ(field(kThrustCurves).text(),
              shown(thrustCurveDirectory()) + QStringLiteral(";") + shown(file("motors")));
    EXPECT_EQ(m_preferences.getUserThrustCurveFiles(thrustCurveDirectory()),
              (Paths{thrustCurveDirectory(), file("motors")}));
    EXPECT_EQ(m_preferences.getDefaultDirectory(), std::optional(m_home.path()));
}

TEST_F(GeneralPreferencesPanelTest, AddStartsInTheDefaultDirectoryAndAppendsToTheTrimmedText)
{
    m_preferences.setDefaultDirectory(file("designs"));
    field(kThrustCurves)
        .setText(QStringLiteral("  ") + shown(file("a.eng")) + QStringLiteral(" ;  "));
    m_prompts.answerOpenFiles(chose(file("b.rse"), m_home.path()));
    clickAdd(kThrustCurves);
    EXPECT_EQ(lastRequest().directory, file("designs"));
    // getText().trim() + ";" + the path.
    EXPECT_EQ(field(kThrustCurves).text(),
              shown(file("a.eng")) + QStringLiteral(" ;;") + shown(file("b.rse")));
    EXPECT_EQ(m_preferences.getUserThrustCurveFiles(thrustCurveDirectory()),
              (Paths{file("a.eng"), file("b.rse")}));

    // An empty text gets no ';' first.
    field(kThrustCurves).setText(QStringLiteral(" "));
    m_prompts.answerOpenFiles(chose(file("c.zip"), m_home.path()));
    clickAdd(kThrustCurves);
    EXPECT_EQ(field(kThrustCurves).text(), shown(file("c.zip")));
}

TEST_F(GeneralPreferencesPanelTest, ACancelledAddChangesNothing)
{
    m_prompts.answerOpenFiles(std::nullopt);
    clickAdd(kThrustCurves);
    EXPECT_EQ(m_prompts.openFileRequests().size(), 1U);
    EXPECT_EQ(field(kThrustCurves).text(), shown(thrustCurveDirectory()));
    EXPECT_EQ(m_preferences.get(PreferenceKeys::kUserThrustCurvesKey), std::nullopt);
    EXPECT_EQ(m_preferences.getDefaultDirectory(), std::nullopt);
}

TEST_F(GeneralPreferencesPanelTest, ResetShowsTheDefaultDirectoryAndRemovesTheList)
{
    // "First one sets to the default, but does not un-set the pref", then the key goes.
    field(kThrustCurves).setText(shown(file("a.eng")));
    EXPECT_EQ(m_preferences.getUserThrustCurveFiles(thrustCurveDirectory()),
              (Paths{file("a.eng")}));
    clickReset(kThrustCurves);
    EXPECT_EQ(field(kThrustCurves).text(), shown(thrustCurveDirectory()));
    EXPECT_EQ(m_preferences.get(PreferenceKeys::kUserThrustCurvesKey), std::nullopt);
}

TEST_F(GeneralPreferencesPanelTest, TheComponentPresetsBlockAddsOrcFilesAndResets)
{
    m_prompts.answerOpenFiles(chose(file("parts.orc"), m_home.path()));
    clickAdd(kPresets);
    EXPECT_EQ(
        lastRequest().nameFilters,
        (QStringList{QStringLiteral("All component preset files (*.orc; directories) (*.orc)"),
                     QStringLiteral("OpenRocket component files (*.orc)")}));
    EXPECT_EQ(lastRequest().selectedNameFilter, lastRequest().nameFilters.front());
    EXPECT_EQ(lastRequest().title, QStringLiteral("Add"));
    EXPECT_EQ(lastRequest().selection, UserPrompts::OpenFileSelection::FILES_AND_DIRECTORIES);
    EXPECT_EQ(field(kPresets).text(),
              shown(componentDirectory()) + QStringLiteral(";") + shown(file("parts.orc")));
    EXPECT_EQ(m_preferences.getUserComponentPresetFiles(componentDirectory()),
              (Paths{componentDirectory(), file("parts.orc")}));
    // The thrust curves are left alone.
    EXPECT_EQ(m_preferences.get(PreferenceKeys::kUserThrustCurvesKey), std::nullopt);
    clickReset(kPresets);
    EXPECT_EQ(field(kPresets).text(), shown(componentDirectory()));
    EXPECT_EQ(m_preferences.get(PreferenceKeys::kUserComponentPresetsKey), std::nullopt);
}

TEST_F(GeneralPreferencesPanelTest, WithoutADefaultDirectoryAnUnsetListShowsNothing)
{
    // A start without default directories (a test's): nothing to show, and Reset empties the
    // field before it removes the list.
    remakePage({});
    EXPECT_EQ(field(kThrustCurves).text(), QString());
    EXPECT_EQ(field(kPresets).text(), QString());
    field(kPresets).setText(shown(file("parts.orc")));
    clickReset(kPresets);
    EXPECT_EQ(field(kPresets).text(), QString());
    EXPECT_EQ(m_preferences.get(PreferenceKeys::kUserComponentPresetsKey), std::nullopt);
    // A stored list is shown all the same.
    m_preferences.setUserThrustCurveFiles(Paths{file("a.eng")});
    remakePage({});
    EXPECT_EQ(field(kThrustCurves).text(), shown(file("a.eng")));
}

/// Destroys the page (with its dialog's window) while the file chooser is open, then lets the
/// script answer.
class AddThatDestroysThePage final : public Gui::Test::ForwardingUserPrompts
{
public:
    AddThatDestroysThePage(UserPrompts& delegate, std::function<void()> destroy)
      : ForwardingUserPrompts(delegate), m_destroy(std::move(destroy))
    {
    }

    // The parent goes with the page: it is not passed on.
    [[nodiscard]] std::optional<OpenFileAnswer> askOpenFiles(
        QWidget* /*parent*/, const OpenFileRequest& request) override
    {
        m_destroy();
        return delegate().askOpenFiles(nullptr, request);
    }

private:
    std::function<void()> m_destroy;
};

/// Resets @p page.
struct ResetPage
{
    std::unique_ptr<GeneralPreferencesPanel>* page;

    void operator()() const { page->reset(); }
};

TEST_F(GeneralPreferencesPanelTest, AnAddWhosePageGoesDuringTheQuestionChangesNothing)
{
    // Inside its own button's click (proved under ASan).
    std::unique_ptr<GeneralPreferencesPanel> page;
    AddThatDestroysThePage                   prompts(m_prompts, ResetPage{.page = &page});
    page =
        std::make_unique<GeneralPreferencesPanel>(m_preferences, prompts, environment(), nullptr);
    const QPointer<GeneralPreferencesPanel> alive(page.get());
    m_prompts.answerOpenFiles(chose(file("motors"), m_home.path()));
    page->addUserFilesButton(kThrustCurves)->click();
    EXPECT_TRUE(alive.isNull());
    EXPECT_EQ(m_preferences.get(PreferenceKeys::kUserThrustCurvesKey), std::nullopt);
    EXPECT_EQ(m_preferences.getDefaultDirectory(), std::nullopt);
}

TEST_F(GeneralPreferencesPanelTest, ResetAllPreferencesAsksFirst)
{
    m_preferences.setAutoOpenLastDesignOnStartup(true);
    const QSignalSpy cleared(m_page.get(), &GeneralPreferencesPanel::preferencesCleared);
    m_prompts.answerYesNo(false);
    m_page->resetAllPreferencesButton()->click();
    ASSERT_EQ(m_prompts.prompts().size(), 1U);
    const Gui::Test::ScriptedUserPrompts::Prompt& question = m_prompts.prompts().front();
    EXPECT_EQ(question.kind, Kind::YES_NO);
    EXPECT_EQ(question.title, QStringLiteral("Reset preferences?"));
    EXPECT_EQ(
        question.texts,
        QStringList({QStringLiteral("WARNING"),
                     QStringLiteral("Are you sure you want to reset all your preferences?")}));
    EXPECT_TRUE(m_preferences.isAutoOpenLastDesignOnStartupEnabled());
    EXPECT_EQ(cleared.count(), 0);
}

TEST_F(GeneralPreferencesPanelTest, ResetAllPreferencesEmptiesTheStoreAndResetsTheUnits)
{
    m_preferences.setAutoOpenLastDesignOnStartup(true);
    m_preferences.getNode("windows").put("position.ComponentConfigDialog", "10,20");
    static_cast<void>(unitGroup(UnitGroupId::LENGTH).setDefaultUnit(std::string_view{"in"}));
    const QSignalSpy cleared(m_page.get(), &GeneralPreferencesPanel::preferencesCleared);
    m_prompts.answerYesNo(true);
    EXPECT_TRUE(m_page->resetAllPreferences());

    // SwingPreferences.clearPreferences(): the node removed, the default units reset and stored.
    EXPECT_EQ(m_preferences.keys(), std::vector<std::string>{});
    EXPECT_EQ(m_preferences.childrenNames(), std::vector<std::string>{"units"});
    EXPECT_EQ(m_preferences.getNode("units").get("LENGTH"), "cm");
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getDefaultUnit().getUnit(), "cm");
    // Then the message, with QtRocket's name.
    ASSERT_EQ(m_prompts.prompts().size(), 2U);
    const Gui::Test::ScriptedUserPrompts::Prompt& message = m_prompts.prompts().back();
    EXPECT_EQ(message.kind, Kind::MESSAGE);
    EXPECT_EQ(message.title, QStringLiteral("Reload QtRocket"));
    EXPECT_EQ(message.texts,
              QStringList({QStringLiteral("WARNING"),
                           QStringLiteral("You may need to restart QtRocket for some of the "
                                          "changes to take effect.")}));
    EXPECT_EQ(cleared.count(), 1);
}

TEST_F(GeneralPreferencesPanelTest, ResetAllPreferencesKeepsTheConfigurationNamesOfTheOpenDocuments)
{
    // O-M3-7 (C5's probe): Java's configurations keep the default name they were made with when
    // the store is cleared; QtRocket's are pinned to it first.
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    m_preferences.setDefaultFlightConfigName("Flight {motors}");
    remakePage({.defaultUserThrustCurveDirectory = std::nullopt,
                .defaultUserComponentDirectory   = std::nullopt,
                .openDocuments                   = onlyDocument(*document)});
    m_prompts.answerYesNo(true);
    EXPECT_TRUE(m_page->resetAllPreferences());
    EXPECT_EQ(m_preferences.getDefaultFlightConfigName(), "[{motors}]");
    const FlightConfiguration& configuration = document->getRocket().getSelectedConfiguration();
    EXPECT_EQ(configuration.getStoredName(), std::optional<std::string>("Flight {motors}"));
    EXPECT_EQ(configuration.getNameRaw(m_preferences), "Flight {motors}");
}

TEST_F(GeneralPreferencesPanelTest, ANoToResetAllPreferencesPinsNothing)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    m_preferences.setDefaultFlightConfigName("Flight {motors}");
    remakePage({.defaultUserThrustCurveDirectory = std::nullopt,
                .defaultUserComponentDirectory   = std::nullopt,
                .openDocuments                   = onlyDocument(*document)});
    m_prompts.answerYesNo(false);
    EXPECT_FALSE(m_page->resetAllPreferences());
    EXPECT_EQ(document->getRocket().getSelectedConfiguration().getStoredName(), std::nullopt);
}

TEST_F(GeneralPreferencesPanelTest, APageThatGoesDuringTheReloadMessageTellsNothingMore)
{
    // The page goes with its dialog's window while "Reload QtRocket" is shown (macOS: a design
    // file the Finder opens in place of the dialog's New design): the preferences are reset, and
    // nothing of the page is used afterwards (no preferencesCleared() of a destroyed page).
    m_preferences.setAutoOpenLastDesignOnStartup(true);
    Gui::Test::HookedUserPrompts prompts;
    auto page = std::make_unique<GeneralPreferencesPanel>(m_preferences, prompts, nullptr);
    GeneralPreferencesPanel* const          shown = page.get();
    const QPointer<GeneralPreferencesPanel> alive(shown);
    const QSignalSpy cleared(shown, &GeneralPreferencesPanel::preferencesCleared);
    prompts.scripted().answerYesNo(true);
    prompts.beforeNextMessage([&page] { page.reset(); });
    EXPECT_TRUE(shown->resetAllPreferences());
    EXPECT_TRUE(alive.isNull());
    EXPECT_EQ(cleared.count(), 0);
    EXPECT_FALSE(m_preferences.isAutoOpenLastDesignOnStartupEnabled());
    ASSERT_EQ(prompts.scripted().prompts().size(), 2U);
    EXPECT_EQ(prompts.scripted().prompts().back().title, QStringLiteral("Reload QtRocket"));
}

TEST_F(GeneralPreferencesPanelTest, TheResetButtonIsJavas)
{
    EXPECT_EQ(m_page->resetAllPreferencesButton()->text(), QStringLiteral("Reset all preferences"));
    EXPECT_EQ(m_page->resetAllPreferencesButton()->toolTip(),
              QStringLiteral("Reset all the preferences, including cached preferences (UI "
                             "settings, recent files, etc.)"));
}

}  // namespace

}  // namespace QtRocket::Gui
