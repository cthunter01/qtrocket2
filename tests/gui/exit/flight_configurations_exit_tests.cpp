// The exit suite of Milestone 3's Motors & Configuration tab (plan section 10.5,
// FlightConfigurationsExitGolden): the scenarios of version 2 (kGuiTabScenarios of
// GoldenGuiReference.h: tests/data/goldens/gui/flight-configurations/ and multi-stage/), each a
// design file opened in the running application (ApplicationSession, started as main() starts it,
// with the preferences at their defaults and the bundled motors), worked on in the tab only
// through QtRocket's GUI (FlightConfigurationScenario: the tab's buttons, the cells of its
// tables, the motor mount table's check boxes, the widgets of the dialogs the operations open,
// the design's tree and configuration dialog, Undo and Redo), saved with Save as, and compared
// with what OpenRocket's GUI did with the same steps (tools/openrocket-gui-reference ran
// OpenRocket's own panels and dialogs: state.json and rocket.ork there):
//
// - after every step the GUI shows what OpenRocket's showed (the step's record of state.json,
//   guiRecordDifferences() of GoldenGuiReference.h): the texts and states of Undo and Redo,
//   whether the design is saved, the selected components, the open window, the component tree
//   (by its digest, line for line where the record has the lines), the window's tab, the
//   questions the step asked (as each dialog opened: its title, its radio buttons, events and
//   values; the motor chooser's search, rows, curve, delay and nozzle, also as the answer left
//   it) and what the tab showed (its sub-tabs, buttons, tables cell by cell with the shading of
//   the motor cells' ignitions and of the Recovery and Stages cells, and the overrides behind
//   these, the selected rows and columns, the motor mount table, the rocket's selected
//   configuration, the simulations' configurations). The one normalisation is OpenRocket's bug
//   1, which QtRocket fixes (the user's U-M3-5), made on the records by guiExpectedRecord():
//   where OpenRocket's deployment and separation dialogs start on "this configuration" for a
//   cell without an override of its own, QtRocket's start on "all configurations"
//   (kGuiExpectedDifferences), and where OpenRocket's Recovery and Stages tables shade no cell,
//   QtRocket's shade every cell without an override of its own. The other fixes of plan 10.3 the
//   tool makes in OpenRocket's run (bugs 5 and 6, kGuiToolFixes lists the records where they
//   show) or the scenarios avoid (bugs 2, 3, 4, 8, 20, 21): the records are QtRocket's behaviour
//   there. Every failure names its step;
// - (a) the save, closed (File > Close design) and opened again (File > Open), is the design that
//   was saved: its tree (with the ids of the saved design) and its document preferences. The
//   application's preferences keep the default configuration name that the flight-configurations
//   scenario's "Save as default" stored, so its reload changes only the name of the rocket's
//   default configuration, which no file holds (reopenChanges()); a load with OpenRocket's fresh
//   preferences gives the configurations saved without a name OpenRocket's default name
//   (kGuiFlightConfigurationsReloadChanges), which (b) compares;
// - (b) the save loads, in the environment of the reference's load (GuiReferenceEnvironment with
//   the bundled databases), into the state OpenRocket's save loads into (designFileState() with
//   the document preferences, state.json's "loaded"), with the same masses, centres of mass and
//   length; its archive holds OpenRocket's entries (the document, then the thrust curves of its
//   motors, each with OpenRocket's bytes), and its document is OpenRocket's text (rocket.ork),
//   line for line, after the normalisations of the re-save goldens (GoldenResaves.h), each named
//   where it is made. The ids that the input file does not spell out (the configurations and the
//   simulations the scenario makes: random in QtRocket, DeterministicUuids' in OpenRocket) are
//   numbered on both sides in the order of their first appearance;
// - (c) a DISABLED_ test writes the saves for the interop check (OpenRocket reads them): see
//   DISABLED_WritesTheSaveForTheInteropCheck below.
//
// Each test runs its scenario in an application of its own (about 1 s in a debug build).

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <QCoreApplication>
#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/gui/app/ApplicationContext.h"
#include "QtRocket/gui/app/ApplicationSession.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Sha256.h"
#include "ScriptedUserPrompts.h"
#include "TestTempDir.h"
#include "exit/FlightConfigurationScenario.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/DesignFileState.h"
#include "goldens/GoldenGuiReference.h"
#include "goldens/GoldenResaves.h"
#include "goldens/GuiReferenceEnvironment.h"

namespace QtRocket::Gui
{

namespace
{

using Gui::Test::FlightConfigurationScenario;
using QtRocket::Test::GuiQuestion;
using QtRocket::Test::GuiQuestionKind;
using QtRocket::Test::GuiReference;
using QtRocket::Test::GuiScenario;
using QtRocket::Test::GuiScenarioStep;
using QtRocket::Test::GuiStepRecord;

/// The contents of @p file; a test failure and "" when it cannot be read.
[[nodiscard]] std::string textOf(const std::filesystem::path& file)
{
    const Result<std::string> text = readTextFile(file);
    if (!text)
    {
        ADD_FAILURE() << text.error().toString();
        return {};
    }
    return *text;
}

[[nodiscard]] Result<GuiScenario> loadScenario(std::string_view scenario)
{
    return QtRocket::Test::parseGuiScenario(
        textOf(QtRocket::Test::guiScenarioDir(scenario) / "scenario.json"));
}

[[nodiscard]] Result<GuiReference> loadReference(std::string_view scenario)
{
    return QtRocket::Test::parseGuiReference(
        textOf(QtRocket::Test::guiScenarioDir(scenario) / "state.json"));
}

/// The text of the design file the scenario's open step names (the input, whose ids both GUIs
/// keep); a test failure and "" when there is none.
[[nodiscard]] std::string inputText(const GuiScenario& scenario)
{
    const Result<std::filesystem::path> file = QtRocket::Test::guiRepositoryFile(
        scenario.steps.empty() ? std::string_view()
                               : std::string_view(scenario.steps.front().file));
    if (!file)
    {
        ADD_FAILURE() << file.error().toString();
        return {};
    }
    return textOf(*file);
}

/// The kinds of @p prompts by name.
[[nodiscard]] std::vector<std::string_view> promptKinds(
    const std::vector<Gui::Test::ScriptedUserPrompts::Prompt>& prompts)
{
    std::vector<std::string_view> kinds;
    kinds.reserve(prompts.size());
    for (const Gui::Test::ScriptedUserPrompts::Prompt& prompt : prompts)
    {
        kinds.push_back(Gui::Test::ScriptedUserPrompts::kindName(prompt.kind));
    }
    return kinds;
}

/// The questions and messages of the script a run of the scenario of @p reference asks: the file
/// of its open step, then the message of each "Save as default" its records show (every other
/// question is a dialog of QtRocket's, which the scenario answers).
[[nodiscard]] std::vector<std::string_view> scriptedPromptsOf(const GuiReference& reference)
{
    std::vector<std::string_view> kinds{"OPEN_FILES"};
    for (const GuiStepRecord& record : reference.steps)
    {
        for (const GuiQuestion& question : record.questions)
        {
            if (question.kind == GuiQuestionKind::MESSAGE)
            {
                kinds.emplace_back("MESSAGE");
            }
        }
    }
    return kinds;
}

/// @p text with every UUID that @p input does not spell out numbered in the order of its first
/// appearance ("drawn-id-<n>", withDrawnIdsNumbered() of GoldenResaves.h): the ids of the input's
/// components stay; those of the configurations and simulations the scenario makes, random in
/// QtRocket's save and DeterministicUuids' in OpenRocket's, stand in the same places.
[[nodiscard]] std::string withIdsNumbered(std::string_view text, std::string_view input)
{
    return QtRocket::Test::withDrawnIdsNumbered(text, input);
}

/// @p lines as one text, each line ended.
[[nodiscard]] std::string joined(std::span<const std::string> lines)
{
    std::string text;
    for (const std::string& line : lines)
    {
        text += line + "\n";
    }
    return text;
}

/// What the save and its reopening in the application change in the tree of the flight-
/// configurations scenario's design (describeRocket() with the saved ids): the rocket's default
/// configuration, which no design file holds, gets the default name of the application's
/// preferences at its making, "Flight {motors}" since the scenario's Save as default stored it,
/// where the saved design's had the old default name, "[{motors}]", which Save as default gave
/// every configuration without a name of its own (OpenRocketDocument::
/// pinFlightConfigurationNames(): OpenRocket's configurations store the default name when they
/// are made, FlightConfiguration.java:111, so OpenRocket's reopening in its application does the
/// same). The two configurations saved without a name get "Flight {motors}" again, the name they
/// had. Nothing changes in the multi-stage scenario's design.
constexpr std::array kFlightConfigurationsReopenChanges{
    QtRocket::Test::GuiReloadChange{.line     = "config default ",
                                    .saved    = " name='[{motors}]' ",
                                    .reloaded = " name='Flight {motors}' "},
};

/// The changes of kFlightConfigurationsReopenChanges for the scenario @p scenario.
[[nodiscard]] std::span<const QtRocket::Test::GuiReloadChange> reopenChanges(
    std::string_view scenario)
{
    if (scenario == "flight-configurations")
    {
        return kFlightConfigurationsReopenChanges;
    }
    return {};
}

/// The number of motors of @p database (in all its sets).
[[nodiscard]] std::size_t motorCount(const ThrustCurveMotorSetDatabase& database)
{
    std::size_t count = 0;
    for (const ThrustCurveMotorSet& set : database.getMotorSets())
    {
        count += set.getMotorCount();
    }
    return count;
}

/// The fixture: the application of a FlightConfigurationScenario with its preferences and saves
/// in a directory of the test's own, for the scenario of the test's parameter; the configuration
/// dialog's state forgotten before and after (ConfigDialogTest's rule), the application's
/// component clipboard left empty.
class FlightConfigurationsExitGolden : public Gui::Test::GuiTest,
                                       public ::testing::WithParamInterface<std::string_view>
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        ComponentConfigDialog::forgetPreviousDialogs();
    }

    void TearDown() override
    {
        m_scenario = nullptr;
        OpenRocketClipboard::instance().clear();
        ComponentConfigDialog::forgetPreviousDialogs();
        GuiTest::TearDown();
    }

    [[nodiscard]] static std::string_view scenarioName() { return GetParam(); }

    /// Starts the application (once).
    FlightConfigurationScenario& start()
    {
        if (m_scenario == nullptr)
        {
            m_scenario = std::make_unique<FlightConfigurationScenario>(m_directory.path());
        }
        return *m_scenario;
    }

    /// Starts the application and runs the scenario of the test's parameter in it, calling
    /// @p observer after every step; the failure of a step, or "".
    [[nodiscard]] std::string runScenario(
        const FlightConfigurationScenario::Observer& observer = {})
    {
        const Result<GuiScenario> scenario = loadScenario(scenarioName());
        if (!scenario)
        {
            return scenario.error().toString();
        }
        return start().run(*scenario, observer);
    }

    [[nodiscard]] FlightConfigurationScenario& scenario() const { return *m_scenario; }
    /// The directory of the application's preferences and of the saves.
    [[nodiscard]] const std::filesystem::path& directory() const noexcept
    {
        return m_directory.path();
    }

    /// The scenario's save, or a test failure and an empty path.
    [[nodiscard]] std::filesystem::path savedFile() const
    {
        const std::optional<std::filesystem::path>& file = m_scenario->savedFile();
        if (!file.has_value())
        {
            ADD_FAILURE() << "the scenario saved nothing";
            return {};
        }
        return *file;
    }

private:
    QtRocket::Test::TempDir                      m_directory;
    std::unique_ptr<FlightConfigurationScenario> m_scenario;
};

/// The name of a test of @p param: the scenario's, with '_' for '-' (a test name is an identifier).
[[nodiscard]] std::string scenarioTestName(const ::testing::TestParamInfo<std::string_view>& param)
{
    std::string name(param.param);
    std::ranges::replace(name, '-', '_');
    return name;
}

INSTANTIATE_TEST_SUITE_P(Scenarios, FlightConfigurationsExitGolden,
                         ::testing::ValuesIn(QtRocket::Test::kGuiTabScenarios), scenarioTestName);

// ================================================================================ the start-up

TEST_P(FlightConfigurationsExitGolden, TheApplicationStartsWithTheMotorsOfOpenRocketsStartUp)
{
    // OpenRocket's start-up of the reference loaded the bundled motors (state.json's "startup");
    // so does the application's (ApplicationContext: data/motors of the build tree).
    const Result<GuiReference> reference = loadReference(scenarioName());
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    const ThrustCurveMotorSetDatabase& motors = start().session().context().services().motors();
    EXPECT_EQ(motors.getMotorSets().size(), static_cast<std::size_t>(reference->startup.motorSets));
    EXPECT_EQ(motorCount(motors), static_cast<std::size_t>(reference->startup.motors));
    EXPECT_EQ(promptKinds(scenario().prompts().prompts()), std::vector<std::string_view>{});
}

// ================================================================================== the steps

TEST_P(FlightConfigurationsExitGolden, EveryStepLeavesWhatOpenRocketsGuiShowed)
{
    const Result<GuiScenario>  steps     = loadScenario(scenarioName());
    const Result<GuiReference> reference = loadReference(scenarioName());
    ASSERT_TRUE(steps.has_value()) << steps.error().toString();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    ASSERT_EQ(reference->steps.size(), steps->steps.size());

    std::vector<std::size_t> observed;
    std::string              expectedDifferences;
    const std::string        failure =
        runScenario([&reference, &observed, &expectedDifferences](const GuiScenarioStep& step,
                                                                  const GuiStepRecord&   record) {
            observed.push_back(step.index);
            const GuiStepRecord expected = QtRocket::Test::guiExpectedRecord(
                scenarioName(), reference->steps.at(step.index), expectedDifferences);
            EXPECT_EQ(QtRocket::Test::guiRecordDifferences(expected, record),
                      std::vector<std::string>{})
                << FlightConfigurationScenario::stepText(step);
        });
    EXPECT_EQ(failure, "");
    EXPECT_EQ(expectedDifferences, "");
    // Every step was done, in order, and recorded once.
    std::vector<std::size_t> all;
    for (const GuiScenarioStep& step : steps->steps)
    {
        all.push_back(step.index);
    }
    EXPECT_EQ(observed, all);
    // The script was asked nothing beyond the file to open and the messages of the records.
    EXPECT_EQ(promptKinds(scenario().prompts().prompts()), scriptedPromptsOf(*reference));
}

TEST_P(FlightConfigurationsExitGolden, SaveAsNamesTheWindowAfterTheFile)
{
    // BasicFrame.saveAsOpenRocket(): the file and the saved state, which the title shows without
    // its modified mark.
    ASSERT_EQ(runScenario(), "");
    const MainWindow* const window = scenario().window();
    ASSERT_NE(window, nullptr);
    const std::filesystem::path file = savedFile();
    ASSERT_FALSE(file.empty());
    EXPECT_EQ(file.filename(), std::filesystem::path("rocket.ork"));
    std::error_code error;
    EXPECT_TRUE(std::filesystem::equivalent(file.parent_path(), directory(), error));
    EXPECT_TRUE(window->document().isSaved());
    EXPECT_EQ(window->windowTitle(),
              QStringLiteral("[*]%1 (rocket.ork)")
                  .arg(QtStrings::fromUtf8(window->document().getRocket().getName())));
    EXPECT_FALSE(window->isWindowModified());
}

// ====================================================================== (a) closed and opened

TEST_P(FlightConfigurationsExitGolden, TheSaveOpensAgainAsTheDesignThatWasSaved)
{
    const Result<GuiReference> reference = loadReference(scenarioName());
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    ASSERT_EQ(runScenario(), "");
    const std::filesystem::path file = savedFile();
    ASSERT_FALSE(file.empty());
    ASSERT_EQ(scenario().closeWindow(), "");
    std::string             problem;
    const MainWindow* const window = scenario().openSavedFile(problem);
    ASSERT_NE(window, nullptr) << problem;

    const OpenRocketDocument& document = window->document();
    EXPECT_TRUE(document.isSaved());
    const std::filesystem::path reopened = document.getFile().value_or(std::filesystem::path());
    std::error_code             error;
    EXPECT_TRUE(!reopened.empty() && std::filesystem::equivalent(reopened, file, error));
    // With the ids of the saved design known: the reopened design keeps every component's id and
    // every flight configuration's, and is the saved design but for its default configuration's
    // name (reopenChanges()).
    ASSERT_FALSE(scenario().savedIds().empty());
    std::string                    changes;
    const std::vector<std::string> reloaded = QtRocket::Test::guiReloadedTree(
        scenario().savedTree(), reopenChanges(scenarioName()), changes);
    EXPECT_EQ(changes, "");
    EXPECT_EQ(QtRocket::Test::describeRocket(
                  document.getRocket(), scenario().session().context().services().preferences(),
                  scenario().savedIds()),
              reloaded);
    EXPECT_EQ(QtRocket::Test::documentPreferenceLines(document),
              scenario().savedDocumentPreferences());
    // The file was asked for, and the load gave no warning.
    std::vector<std::string_view> kinds = scriptedPromptsOf(*reference);
    kinds.emplace_back("OPEN_FILES");
    EXPECT_EQ(promptKinds(scenario().prompts().prompts()), kinds);
}

// ================================================================= (b) OpenRocket's reference

TEST_P(FlightConfigurationsExitGolden, TheSaveLoadsIntoTheStateOfOpenRocketsSave)
{
    const Result<GuiScenario>  steps     = loadScenario(scenarioName());
    const Result<GuiReference> reference = loadReference(scenarioName());
    ASSERT_TRUE(steps.has_value()) << steps.error().toString();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    ASSERT_EQ(runScenario(), "");
    const std::filesystem::path file = savedFile();
    ASSERT_FALSE(file.empty());
    const std::string input = inputText(*steps);
    ASSERT_FALSE(input.empty());

    // The state of the load with OpenRocket's databases and fresh preferences, the drawn ids
    // numbered on both sides (withIdsNumbered()).
    QtRocket::Test::GuiReferenceEnvironment environment(
        QtRocket::Test::GuiReferenceEnvironment::Databases::BUNDLED);
    const std::vector<std::string> state = QtRocket::Test::designFileState(
        environment.get(), file, QtRocket::Test::StateDetail::WITH_DOCUMENT_PREFERENCES);
    const std::vector<std::string> found =
        QtRocket::Test::linesOf(withIdsNumbered(joined(state), input));
    const std::vector<std::string> expected =
        QtRocket::Test::linesOf(withIdsNumbered(joined(reference->loadedLines), input));
    const std::vector<std::string_view> expectedViews(expected.begin(), expected.end());
    EXPECT_EQ(QtRocket::Test::firstDifference(expectedViews, found), "");

    // The masses, the centres of mass and the length of a second load.
    const Result<LoadedDocument> loaded =
        GeneralRocketLoader(environment.get().context()).load(file);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    const std::vector<std::string_view> numbers(reference->loadedNumbers.begin(),
                                                reference->loadedNumbers.end());
    EXPECT_EQ(QtRocket::Test::wrongNumbers(
                  numbers, QtRocket::Test::massAndLength(loaded->document->getRocket())),
              "");
}

TEST_P(FlightConfigurationsExitGolden, TheSaveIsTheTextOfOpenRocketsSave)
{
    const Result<GuiScenario>  steps     = loadScenario(scenarioName());
    const Result<GuiReference> reference = loadReference(scenarioName());
    ASSERT_TRUE(steps.has_value()) << steps.error().toString();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    ASSERT_EQ(runScenario(), "");
    const std::filesystem::path file = savedFile();
    ASSERT_FALSE(file.empty());
    const std::string input = inputText(*steps);
    ASSERT_FALSE(input.empty());
    const Result<std::vector<std::byte>> bytes = readFile(file);
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    const Result<ZipArchive> archive = ZipArchive::fromBytes(*bytes);
    ASSERT_TRUE(archive.has_value()) << archive.error().toString();

    // The archive holds OpenRocket's entries in OpenRocket's order: the document, then the thrust
    // curves of its motors (no decal, and no preview: decision O26), each curve with OpenRocket's
    // bytes.
    std::vector<std::string> entries;
    for (const ZipArchive::Entry& entry : archive->entries())
    {
        entries.push_back(entry.name);
    }
    EXPECT_EQ(entries, reference->archiveEntries);
    std::map<std::string, std::string, std::less<>> curves;
    std::map<std::string, std::string, std::less<>> expectedCurves;
    for (const std::string& entry : entries)
    {
        if (const std::vector<std::byte>* const curve = archive->find(entry);
            curve != nullptr && entry != "rocket.ork")
        {
            curves.emplace(entry, sha256Hex(*curve));
        }
    }
    for (const auto& [entry, sha256] : reference->archiveSha256)
    {
        if (entry != "rocket.ork")
        {
            expectedCurves.emplace(entry, sha256);
        }
    }
    EXPECT_EQ(curves, expectedCurves);
    EXPECT_FALSE(curves.empty());
    const std::vector<std::byte>* const document = archive->find("rocket.ork");
    ASSERT_NE(document, nullptr);

    // The document against OpenRocket's (rocket.ork), after the normalisations of the re-save
    // goldens:
    // 1. the ids that the input does not spell out, random in QtRocket's save, numbered on both
    //    sides in the order of their first appearance (withIdsNumbered()), so that each stands
    //    where the other does;
    // 2. the value of the creator attribute set aside (compareOrkTexts(): QtRocket writes its
    //    own name and version);
    // 3. a line that differs only in numbers that read as the same double counted apart, not as
    //    a difference (compareOrkTexts(): the user's decision U10, JDK 17's longer forms of a
    //    double are the same number).
    // Every other byte is compared, line for line.
    const QtRocket::Test::OrkTextComparison comparison = QtRocket::Test::compareOrkTexts(
        withIdsNumbered(bytesToString(*document), input),
        withIdsNumbered(textOf(QtRocket::Test::guiScenarioDir(scenarioName()) / "rocket.ork"),
                        input));
    EXPECT_EQ(comparison.differing, 0U) << comparison.differences;
    EXPECT_GT(comparison.lines, 200U);
}

// ============================================================================ (c) the interop

/// The command-line argument that names the directory of the interop check's saves.
constexpr std::string_view kInteropArgument = "--interop-saves=";

/// The directory of the interop check's saves: the one --interop-saves=<directory> names on the
/// command line (GoogleTest keeps the arguments it does not know), else interop-saves in the
/// build tree, the directory above the bin/ of the test program (build/<preset>/interop-saves),
/// as for the core's saves (tests/core/goldens/ork_interop_saves_tests.cpp) and the from-scratch
/// scenario's (from_scratch_exit_tests.cpp).
[[nodiscard]] std::filesystem::path interopSavesDirectory()
{
    for (const std::string& argument : ::testing::internal::GetArgvs())
    {
        if (argument.starts_with(kInteropArgument))
        {
            return pathFromUtf8(std::string_view(argument).substr(kInteropArgument.size()));
        }
    }
    return QtStrings::toPath(QCoreApplication::applicationDirPath()).parent_path() /
           "interop-saves";
}

// The fixture of the interop check's saves, a suite of its own (no "Golden" in its name: ctest
// lists it disabled, as the core's OrkInteropSaves).
using FlightConfigurationsInteropSave = FlightConfigurationsExitGolden;

INSTANTIATE_TEST_SUITE_P(Scenarios, FlightConfigurationsInteropSave,
                         ::testing::ValuesIn(QtRocket::Test::kGuiTabScenarios), scenarioTestName);

// The scenario's save for the interop check of the milestone's exit (plan section 10.5: the
// designs QtRocket's tab made open in OpenRocket): a DISABLED_ test, which neither ctest nor CI
// runs. It writes build/<preset>/interop-saves/gui-<scenario>.zip-nodata.ork (an .ork archive
// without simulation data: the scenarios' simulations were never run), or into the directory of
// --interop-saves=<directory>, replacing the one an earlier run wrote, and touches no other file.
// The core's writer (OrkInteropSaves of tests/core/goldens/ork_interop_saves_tests.cpp) removes
// it with its own saves of an earlier run, so it runs first. By hand, from the source tree, one
// command per line:
//
// clang-format off
//   build/<preset>/bin/QtRocket_core_tests --gtest_also_run_disabled_tests --gtest_filter='OrkInteropSaves.*'
//   build/<preset>/bin/QtRocket_gui_tests --gtest_also_run_disabled_tests --gtest_filter='*InteropSave*'
//   tools/openrocket-goldens/interop.sh build/<preset>/interop-saves
// clang-format on
//
// The input names of InteropCheck (tools/openrocket-goldens) for these saves are
// "gui-flight-configurations" and "gui-multi-stage", against OpenRocket's saves of the same
// scenarios (tests/data/goldens/gui/<scenario>/rocket.ork).
TEST_P(FlightConfigurationsInteropSave, DISABLED_WritesTheSaveForTheInteropCheck)
{
    ASSERT_EQ(runScenario(), "");
    const std::filesystem::path file = savedFile();
    ASSERT_FALSE(file.empty());
    const std::filesystem::path directory = interopSavesDirectory();
    std::error_code             error;
    std::filesystem::create_directories(directory, error);
    ASSERT_FALSE(error) << pathToUtf8(directory) << ": " << error.message();
    const std::filesystem::path target =
        directory / pathFromUtf8(std::format("gui-{}.zip-nodata.ork", scenarioName()));
    std::filesystem::copy_file(file, target, std::filesystem::copy_options::overwrite_existing,
                               error);
    ASSERT_FALSE(error) << pathToUtf8(target) << ": " << error.message();
    std::println("{}", pathToUtf8(target));
}

}  // namespace

}  // namespace QtRocket::Gui
