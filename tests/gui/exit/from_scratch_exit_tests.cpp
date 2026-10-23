// The exit suite of Milestone 2 in the direction "a design made in QtRocket's GUI opens in
// OpenRocket" (plan section 9.5, FromScratchExitGolden; the tests-exit scout's section 2.1): a
// rocket designed from scratch in the running application (ApplicationSession, started as main()
// starts it, with the preferences at their defaults), only through its GUI (FromScratchScenario:
// the steps of tests/data/goldens/gui/from-scratch/scenario.json, which
// tools/openrocket-gui-reference did with OpenRocket's own GUI code), saved with Save as, and
// compared with what OpenRocket's GUI did with the same steps (state.json and rocket.ork there):
//
// - after every step the GUI shows what OpenRocket's showed (the step's record of state.json):
//   the texts and states of Undo and Redo, whether the design is saved, the selected components,
//   the open window (the dialog's panel class and its component, the "Save Design Info" panel,
//   the file chooser) and the component tree (describeRocket() of ComponentHandlerTestSupport.h
//   with no known id, by its digest, and line for line where the record has the lines); every
//   failure names its step;
// - (a) the save, closed (File > Close design) and opened again (File > Open), is the design
//   that was saved: its tree as describeRocket() gives it (with the ids of the saved design, which
//   the reopened one keeps), but for what a reload changes in OpenRocket as in QtRocket
//   (kGuiSaveAndReloadChanges of GoldenGuiReference.h), and its document preferences and
//   materials (documentPreferenceLines() of DesignFileState.h);
// - (b) the save loads, in the environment of the reference's load (GuiReferenceEnvironment),
//   into the state OpenRocket's save loads into (designFileState() with the document
//   preferences, state.json's "loaded"), the component ids numbered on both sides, with the same
//   masses, centres of mass and length; and its document is OpenRocket's text (rocket.ork), line
//   for line, after the normalisations of the re-save goldens (GoldenResaves.h), each named where
//   it is made (TheSaveIsTheTextOfOpenRocketsSave);
// - (c) a DISABLED_ test writes the save for the interop check (OpenRocket reads it): see
//   DISABLED_WritesTheSaveForTheInteropCheck below.
//
// Each test runs the scenario in an application of its own: about 0.7 s in a debug build.

#include <cstddef>
#include <filesystem>
#include <format>
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
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "ScriptedUserPrompts.h"
#include "TestTempDir.h"
#include "exit/FromScratchScenario.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/DesignFileState.h"
#include "goldens/GoldenGuiReference.h"
#include "goldens/GoldenResaves.h"
#include "goldens/GuiReferenceEnvironment.h"

namespace QtRocket::Gui
{

namespace
{

using Gui::Test::FromScratchScenario;
using QtRocket::Test::GuiComponentPath;
using QtRocket::Test::GuiReference;
using QtRocket::Test::GuiScenario;
using QtRocket::Test::GuiScenarioStep;
using QtRocket::Test::GuiStepRecord;
using PromptKind = Gui::Test::ScriptedUserPrompts::Kind;

/// The contents of @p file of the reference's directory; a test failure and "" when it cannot be
/// read.
[[nodiscard]] std::string referenceText(std::string_view file)
{
    const Result<std::string> text =
        readTextFile(QtRocket::Test::guiFromScratchDir() / pathFromUtf8(file));
    if (!text)
    {
        ADD_FAILURE() << text.error().toString();
        return {};
    }
    return *text;
}

[[nodiscard]] Result<GuiScenario> loadScenario()
{
    return QtRocket::Test::parseGuiScenario(referenceText("scenario.json"));
}

[[nodiscard]] Result<GuiReference> loadReference()
{
    return QtRocket::Test::parseGuiReference(referenceText("state.json"));
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

// =============================================================================== the records

/// @p paths as text, "[Sustainer/Body Tube, (rocket)]".
[[nodiscard]] std::string pathsText(const std::vector<GuiComponentPath>& paths)
{
    std::string text = "[";
    for (const GuiComponentPath& path : paths)
    {
        text += (text.size() > 1 ? ", " : "") + QtRocket::Test::guiComponentPathText(path);
    }
    return text + "]";
}

/// The open window of @p record as text: "<window> <component>", or "none".
[[nodiscard]] std::string windowText(const GuiStepRecord& record)
{
    if (!record.window.has_value())
    {
        return "none";
    }
    return *record.window +
           (record.windowComponent.has_value()
                ? " " + QtRocket::Test::guiComponentPathText(*record.windowComponent)
                : std::string());
}

/// An action's state as text: "'Undo (Add Nose Cone)' enabled".
[[nodiscard]] std::string actionText(const QtRocket::Test::GuiActionState& action)
{
    return std::format("'{}' {}", action.text, action.enabled ? "enabled" : "disabled");
}

/// What differs between the record @p found of QtRocket's GUI and OpenRocket's @p expected, one
/// line each ("undo: OpenRocket ..., QtRocket ..."); empty when they agree.
[[nodiscard]] std::vector<std::string> recordDifferences(const GuiStepRecord& expected,
                                                         const GuiStepRecord& found)
{
    std::vector<std::string> differences;
    const auto compare = [&differences](std::string_view what, const std::string& wanted,
                                        const std::string& got) {
        if (wanted != got)
        {
            differences.push_back(std::format("{}: OpenRocket {}, QtRocket {}", what, wanted, got));
        }
    };
    compare("undo", actionText(expected.undo), actionText(found.undo));
    compare("redo", actionText(expected.redo), actionText(found.redo));
    compare("saved", expected.saved ? "true" : "false", found.saved ? "true" : "false");
    compare("selection", pathsText(expected.selection), pathsText(found.selection));
    compare("window", windowText(expected), windowText(found));
    if (expected.treeSha256 != found.treeSha256)
    {
        // The record's lines where it has them, else the tree QtRocket shows.
        const std::vector<std::string>& lines = found.tree.value_or(std::vector<std::string>{});
        if (expected.tree.has_value())
        {
            const std::vector<std::string_view> wanted(expected.tree->begin(),
                                                       expected.tree->end());
            differences.push_back("tree: " + QtRocket::Test::firstDifference(wanted, lines));
        }
        else
        {
            std::string tree = "tree (digest differs; QtRocket's):";
            for (const std::string& line : lines)
            {
                tree += "\n  " + line;
            }
            differences.push_back(tree);
        }
    }
    return differences;
}

// ============================================================================ the comparisons

/// @p text with every UUID numbered in the order of its first appearance ("drawn-id-<n>"): the
/// ids of the components are random in QtRocket's save and DeterministicUuids' in OpenRocket's,
/// in the same places, which this compares (withDrawnIdsNumbered() of GoldenResaves.h, with no
/// id known).
[[nodiscard]] std::string withIdsNumbered(std::string_view text)
{
    return QtRocket::Test::withDrawnIdsNumbered(text, {});
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

/// The fixture: the application of a FromScratchScenario with its preferences and saves in a
/// directory of the test's own; the configuration dialog's state forgotten before and after
/// (ConfigDialogTest's rule), the application's component clipboard (which the scenario's Copy
/// fills) left empty.
class FromScratchExitGolden : public Gui::Test::GuiTest
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

    /// Starts the application and runs the scenario of scenario.json in it, calling @p observer
    /// after every step; the failure of a step, or "".
    [[nodiscard]] std::string runScenario(const FromScratchScenario::Observer& observer = {})
    {
        const Result<GuiScenario> scenario = loadScenario();
        if (!scenario)
        {
            return scenario.error().toString();
        }
        m_scenario = std::make_unique<FromScratchScenario>(m_directory.path());
        return m_scenario->run(*scenario, observer);
    }

    [[nodiscard]] FromScratchScenario& scenario() const { return *m_scenario; }
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
    QtRocket::Test::TempDir              m_directory;
    std::unique_ptr<FromScratchScenario> m_scenario;
};

// ================================================================================= the steps

TEST_F(FromScratchExitGolden, EveryStepLeavesWhatOpenRocketsGuiShowed)
{
    const Result<GuiScenario>  steps     = loadScenario();
    const Result<GuiReference> reference = loadReference();
    ASSERT_TRUE(steps.has_value()) << steps.error().toString();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    ASSERT_EQ(reference->steps.size(), steps->steps.size());

    std::vector<std::size_t> observed;
    const std::string        failure = runScenario(
        [&reference, &observed](const GuiScenarioStep& step, const GuiStepRecord& record) {
            observed.push_back(step.index);
            EXPECT_EQ(recordDifferences(reference->steps.at(step.index), record),
                      std::vector<std::string>{})
                << FromScratchScenario::stepText(step);
        });
    EXPECT_EQ(failure, "");
    // Every step was done, in order, and recorded once.
    std::vector<std::size_t> all;
    for (const GuiScenarioStep& step : steps->steps)
    {
        all.push_back(step.index);
    }
    EXPECT_EQ(observed, all);
    // The GUI asked nothing beyond the scenario's own windows.
    EXPECT_EQ(promptKinds(scenario().prompts().prompts()), std::vector<std::string_view>{});
}

TEST_F(FromScratchExitGolden, SaveAsNamesTheWindowAfterTheFile)
{
    // BasicFrame.saveAsOpenRocket(): the file and the saved state, which the title shows without
    // its modified mark.
    ASSERT_EQ(runScenario(), "");
    const MainWindow* const window = scenario().window();
    ASSERT_NE(window, nullptr);
    const std::filesystem::path file = savedFile();
    ASSERT_FALSE(file.empty());
    // The name typed, with the extension Save as adds, in the directory chosen.
    EXPECT_EQ(file.filename(), std::filesystem::path("rocket.ork"));
    std::error_code error;
    EXPECT_TRUE(std::filesystem::equivalent(file.parent_path(), directory(), error));
    EXPECT_TRUE(window->document().isSaved());
    EXPECT_EQ(window->windowTitle(), QStringLiteral("[*]Rocket (rocket.ork)"));
    EXPECT_FALSE(window->isWindowModified());
}

// ====================================================================== (a) closed and opened

TEST_F(FromScratchExitGolden, TheSaveOpensAgainAsTheDesignThatWasSaved)
{
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
    EXPECT_EQ(window->windowTitle(), QStringLiteral("[*]Rocket (rocket.ork)"));
    std::string                    changes;
    const std::vector<std::string> reloaded =
        QtRocket::Test::guiReloadedTree(scenario().savedTree(), changes);
    EXPECT_EQ(changes, "");
    // With the ids of the saved design known: the reopened design keeps every component's id and
    // every flight configuration's.
    ASSERT_FALSE(scenario().savedIds().empty());
    EXPECT_EQ(QtRocket::Test::describeRocket(
                  document.getRocket(), scenario().session().context().services().preferences(),
                  scenario().savedIds()),
              reloaded);
    EXPECT_EQ(QtRocket::Test::documentPreferenceLines(document),
              scenario().savedDocumentPreferences());
    // The file was asked for, and the load gave no warning.
    EXPECT_EQ(promptKinds(scenario().prompts().prompts()),
              std::vector<std::string_view>{"OPEN_FILES"});
}

// ================================================================= (b) OpenRocket's reference

TEST_F(FromScratchExitGolden, TheSaveLoadsIntoTheStateOfOpenRocketsSave)
{
    const Result<GuiReference> reference = loadReference();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    ASSERT_EQ(runScenario(), "");
    const std::filesystem::path file = savedFile();
    ASSERT_FALSE(file.empty());

    // The state of the load, the component ids numbered on both sides (withIdsNumbered()).
    QtRocket::Test::GuiReferenceEnvironment environment;
    const std::vector<std::string>          state = QtRocket::Test::designFileState(
        environment.get(), file, QtRocket::Test::StateDetail::WITH_DOCUMENT_PREFERENCES);
    const std::vector<std::string> found = QtRocket::Test::linesOf(withIdsNumbered(joined(state)));
    const std::vector<std::string> expected =
        QtRocket::Test::linesOf(withIdsNumbered(joined(reference->loadedLines)));
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

TEST_F(FromScratchExitGolden, TheSaveIsTheTextOfOpenRocketsSave)
{
    const Result<GuiReference> reference = loadReference();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    ASSERT_EQ(runScenario(), "");
    const std::filesystem::path file = savedFile();
    ASSERT_FALSE(file.empty());
    const Result<std::vector<std::byte>> bytes = readFile(file);
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    const Result<ZipArchive> archive = ZipArchive::fromBytes(*bytes);
    ASSERT_TRUE(archive.has_value()) << archive.error().toString();

    // The archive holds the document alone, as OpenRocket's: no decal, no motor file, and no
    // preview (decision O26: QtRocket's saves carry none before Milestone 5).
    std::vector<std::string> entries;
    for (const ZipArchive::Entry& entry : archive->entries())
    {
        entries.push_back(entry.name);
    }
    EXPECT_EQ(entries, reference->archiveEntries);
    const std::vector<std::byte>* const document = archive->find("rocket.ork");
    ASSERT_NE(document, nullptr);

    // The document against OpenRocket's (rocket.ork), after the normalisations of the re-save
    // goldens:
    // 1. the component ids, random in QtRocket's save, numbered on both sides in the order of
    //    their first appearance (withIdsNumbered()), so that each stands where the other does;
    // 2. the value of the creator attribute set aside (compareOrkTexts(): QtRocket writes its
    //    own name and version);
    // 3. a line that differs only in numbers that read as the same double counted apart, not as
    //    a difference (compareOrkTexts(): the user's decision U10, JDK 17's longer forms of a
    //    double are the same number).
    // Every other byte is compared, line for line.
    const QtRocket::Test::OrkTextComparison comparison = QtRocket::Test::compareOrkTexts(
        withIdsNumbered(bytesToString(*document)), withIdsNumbered(referenceText("rocket.ork")));
    EXPECT_EQ(comparison.differing, 0U) << comparison.differences;
    EXPECT_GT(comparison.lines, 200U);
}

// ============================================================================ (c) the interop

/// The command-line argument that names the directory of the interop check's saves.
constexpr std::string_view kInteropArgument = "--interop-saves=";

/// The directory of the interop check's saves: the one --interop-saves=<directory> names on the
/// command line (GoogleTest keeps the arguments it does not know), else interop-saves in the
/// build tree, the directory above the bin/ of the test program (build/<preset>/interop-saves),
/// as for the core's saves (tests/core/goldens/ork_interop_saves_tests.cpp).
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

/// The name of the save in that directory: <input>.<form>.ork, the input "gui-from-scratch" (the
/// GUI's reference, tests/data/goldens/gui/from-scratch) and the form "zip-nodata" (an .ork
/// archive without simulation data: the design has no simulation).
constexpr std::string_view kInteropSave = "gui-from-scratch.zip-nodata.ork";

// The fixture of the interop check's save, a suite of its own (no "Golden" in its name: ctest
// lists it disabled, as the core's OrkInteropSaves).
using FromScratchInteropSave = FromScratchExitGolden;

// The scenario's save for the interop check of the milestone's exit (plan section 9.5: the
// design QtRocket's GUI made opens in OpenRocket): a DISABLED_ test, which neither ctest nor CI
// runs. It writes build/<preset>/interop-saves/gui-from-scratch.zip-nodata.ork (or into the
// directory of --interop-saves=<directory>), replacing the one an earlier run wrote, and touches
// no other file. The core's writer (OrkInteropSaves of
// tests/core/goldens/ork_interop_saves_tests.cpp) removes it with its own saves of an earlier
// run, so it runs first. By hand, from the source tree, one command per line:
//
// clang-format off
//   build/<preset>/bin/QtRocket_core_tests --gtest_also_run_disabled_tests --gtest_filter='OrkInteropSaves.*'
//   build/<preset>/bin/QtRocket_gui_tests --gtest_also_run_disabled_tests --gtest_filter='FromScratchInteropSave.*'
//   tools/openrocket-goldens/interop.sh build/<preset>/interop-saves
// clang-format on
//
// InteropCheck (tools/openrocket-goldens) checks this save as its input "gui-from-scratch",
// against OpenRocket's save of the same scenario (tests/data/goldens/gui/from-scratch/rocket.ork),
// both loaded in the environment of the GUI reference's start-up.
TEST_F(FromScratchInteropSave, DISABLED_WritesTheSaveForTheInteropCheck)
{
    ASSERT_EQ(runScenario(), "");
    const std::filesystem::path file = savedFile();
    ASSERT_FALSE(file.empty());
    const std::filesystem::path directory = interopSavesDirectory();
    std::error_code             error;
    std::filesystem::create_directories(directory, error);
    ASSERT_FALSE(error) << pathToUtf8(directory) << ": " << error.message();
    const std::filesystem::path target = directory / pathFromUtf8(kInteropSave);
    std::filesystem::copy_file(file, target, std::filesystem::copy_options::overwrite_existing,
                               error);
    ASSERT_FALSE(error) << pathToUtf8(target) << ": " << error.message();
    std::println("{}", pathToUtf8(target));
}

}  // namespace

}  // namespace QtRocket::Gui
