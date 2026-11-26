// The exit suite of Milestone 4's Flight simulations tab (plan section 11.6 E6,
// SimulationsExitGolden): the scenario of version 3 (kGuiSimulationsScenario of
// GoldenGuiReference.h: tests/data/goldens/gui/simulations/), the flight-configurations
// reference's design opened in the running application (ApplicationSession, started as main()
// starts it, with the preferences at their defaults and the bundled motors; its executor of
// simulations a ManualSimulationExecutor that runs nothing), worked on only through QtRocket's
// GUI (SimulationsScenario: the window's tabs, the rows of the simulations table, the tab's
// buttons, the Edit menu, the column chooser, the widgets of the settings dialog, the wind
// editor, the CSV import settings, the lookup dialog, the run dialog and the confirmations),
// saved with Save as, and compared with what OpenRocket's GUI did with the same steps
// (tools/openrocket-gui-reference ran OpenRocket's own SimulationPanel and dialogs: state.json and
// rocket.ork there):
//
// - after every step the GUI shows what OpenRocket's showed (the step's record of state.json,
//   guiRecordDifferences() of GoldenGuiReference.h): FlightConfigurationsExitGolden's members (the
//   texts and states of Undo and Redo, whether the design is saved, the selected components, the
//   component tree, the window's tab, the Motors & Configuration tab), the questions the step
//   asked (each as its dialog opened, and as the answer left it: the settings dialog's title, name,
//   configuration, status and tabs; the run dialog's simulations; the confirmations' texts, boxes
//   and buttons; the wind editor's table; the CSV import settings; the file choosers; the lookup
//   dialog's sections), what the simulations tab showed (its columns, its rows cell by cell with
//   their statuses and warnings, the selected rows, the tab's actions, the simulations on the
//   clipboard, the document preference of the hidden columns), every simulation of the document
//   (its name, configuration, status, the ten lines of its options) and after Copy and Cut the
//   system clipboard's text. QtRocket's records are made as the tool makes OpenRocket's: a lookup
//   table's file inside the repository written relative to it (guiSimulationStates()), the column
//   lists of the lookup dialog's summaries sorted (guiSortedLookupColumns()). The expected
//   differences, each listed here and checked to be there and nowhere else: QtRocket's fix of
//   OpenRocket bug 27 that the records show, made by guiExpectedRecord() (kPasteDifferences: Paste
//   without a selected row), with the Monte Carlo action of Milestone 7 left out; and the Undo item
//   after the cancelled New simulation (kUndoDifferences: QtRocket's document hears a simulation
//   once it is in its list). The fixes the tool made in OpenRocket's run (bugs 6, 32, 33 and 34:
//   kGuiSimulationToolFixes) are in the records as they are, and the bugs the scenario avoids
//   (29-31, 35, 37, 38) show nowhere. Every failure names its step;
// - no simulation ran: every run dialog was cancelled before its simulations started;
// - (a) the save, closed (File > Close design) and opened again (File > Open), is the design and
//   the simulations that were saved;
// - (b) the save loads, in the environment of the reference's load (GuiReferenceEnvironment with
//   the bundled databases), into the state OpenRocket's save loads into (designFileState() with
//   the document preferences: the simulations' options included), with the same masses, centres
//   of mass and length; its archive holds OpenRocket's entries, and its document is OpenRocket's
//   text (rocket.ork), line for line, after the normalisations of the re-save goldens
//   (GoldenResaves.h);
// - (c) a DISABLED_ test writes the save for the interop check (the input gui-simulations of
//   InteropCheck).
//
// Each test runs the scenario in an application of its own.

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

#include <QClipboard>
#include <QGuiApplication>
#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "InteropSaves.h"
#include "ManualSimulationExecutor.h"
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
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Sha256.h"
#include "ScriptedUserPrompts.h"
#include "TestTempDir.h"
#include "exit/SimulationsScenario.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/DesignFileState.h"
#include "goldens/GoldenGuiReference.h"
#include "goldens/GoldenResaves.h"
#include "goldens/GuiReferenceEnvironment.h"

namespace QtRocket::Gui
{

namespace
{

using Gui::Test::SimulationsScenario;
using QtRocket::Test::GuiQuestion;
using QtRocket::Test::GuiQuestionKind;
using QtRocket::Test::GuiReference;
using QtRocket::Test::GuiScenario;
using QtRocket::Test::GuiScenarioStep;
using QtRocket::Test::GuiStepRecord;
using QtRocket::Test::kGuiSimulationsScenario;

/// The steps whose records show OpenRocket bug 27 (guiExpectedRecord()): OpenRocket's Paste is
/// disabled without a selected row, QtRocket's is enabled while simulations are on the clipboard
/// and the table is shown (SimulationPanel::updateActions()). The cancelled New simulation of
/// step 29 leaves no row selected, with the two simulations of the Cut still on the clipboard,
/// up to the end of the scenario.
constexpr std::array<std::size_t, 5> kPasteDifferences{29, 30, 31, 32, 33};

/// An Undo item of OpenRocket's records that QtRocket's GUI shows otherwise: the step, the
/// item's text in OpenRocket's record and in QtRocket's GUI.
struct UndoDifference
{
    std::size_t      step{0};
    std::string_view openRocket;
    std::string_view qtRocket;
};

/// The Undo item after the cancelled New simulation of step 29, up to the design information of
/// Save as, which changes the rocket (step 31). The undo and redo of steps 19 and 20 change the
/// number of simulations, so both GUIs replace every simulation with a copy (cloneForUndo()) whose
/// options no longer tell the document of a change. The edits of steps 25 and 26 are made on such
/// copies, so the last position of the undo history, which the settings dialog of step 28 added
/// after them, has no description in either GUI until a change is heard while the document is not
/// clean. OpenRocket's new simulation tells the document of its name before it is added
/// (Simulation's constructor makes its document a listener, and SimulationPanel.newSimulation()
/// names it next), which gives that position the description of the last undo position, "Edit
/// Simulation 2", the description the Cancel's undo goes back to. QtRocket's document hears a
/// simulation only once it is in its list (the core's Simulation, its documented difference from
/// Java), so its Undo item names no change there, enabled as OpenRocket's.
constexpr std::array kUndoDifferences{
    UndoDifference{.step = 29, .openRocket = "Undo (Edit Simulation 2)", .qtRocket = "Undo"},
    UndoDifference{.step = 30, .openRocket = "Undo (Edit Simulation 2)", .qtRocket = "Undo"},
};

/// The contents of @p file; a test failure and "" when it cannot be read.
[[nodiscard]] std::string fileText(const std::filesystem::path& file)
{
    const Result<std::string> text = readTextFile(file);
    if (!text)
    {
        ADD_FAILURE() << text.error().toString();
        return {};
    }
    return *text;
}

[[nodiscard]] Result<GuiScenario> loadSimulationsScenario()
{
    return QtRocket::Test::parseGuiScenario(
        fileText(QtRocket::Test::guiScenarioDir(kGuiSimulationsScenario) / "scenario.json"));
}

[[nodiscard]] Result<GuiReference> loadSimulationsReference()
{
    return QtRocket::Test::parseGuiReference(
        fileText(QtRocket::Test::guiScenarioDir(kGuiSimulationsScenario) / "state.json"));
}

/// The text of the design file the scenario's open step names (the input, whose ids both GUIs
/// keep); a test failure and "" when there is none.
[[nodiscard]] std::string scenarioInputText(const GuiScenario& scenario)
{
    const Result<std::filesystem::path> file = QtRocket::Test::guiRepositoryFile(
        scenario.steps.empty() ? std::string_view()
                               : std::string_view(scenario.steps.front().file));
    if (!file)
    {
        ADD_FAILURE() << file.error().toString();
        return {};
    }
    return fileText(*file);
}

/// The kinds of @p prompts by name.
[[nodiscard]] std::vector<std::string_view> scriptedKinds(
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

/// The questions of the script a run of the scenario of @p reference asks: the file of its open
/// step, then the file of each file chooser its records show (the wind imports' and the lookup
/// dialog's; every other question is a dialog of QtRocket's, which the scenario answers).
[[nodiscard]] std::vector<std::string_view> scriptedQuestionsOf(const GuiReference& reference)
{
    std::vector<std::string_view> kinds{"OPEN_FILES"};
    for (const GuiStepRecord& record : reference.steps)
    {
        for (const GuiQuestion& question : record.questions)
        {
            if (question.kind == GuiQuestionKind::OPEN_FILE)
            {
                kinds.emplace_back("OPEN_FILES");
            }
            else if (question.kind == GuiQuestionKind::MESSAGE)
            {
                kinds.emplace_back("MESSAGE");
            }
        }
    }
    return kinds;
}

/// @p lines as one text, each line ended.
[[nodiscard]] std::string joinedLines(std::span<const std::string> lines)
{
    std::string text;
    for (const std::string& line : lines)
    {
        text += line + "\n";
    }
    return text;
}

/// The number of motors of @p database (in all its sets).
[[nodiscard]] std::size_t motorsIn(const ThrustCurveMotorSetDatabase& database)
{
    std::size_t count = 0;
    for (const ThrustCurveMotorSet& set : database.getMotorSets())
    {
        count += set.getMotorCount();
    }
    return count;
}

/// Empties the system clipboard (Copy and Cut of simulations put the table's text there).
void clearSystemClipboard()
{
    QGuiApplication::clipboard()->clear();
}

/// The state of Paste in @p record; false without the simulations tab.
[[nodiscard]] bool pasteEnabled(const GuiStepRecord& record)
{
    if (!record.simulationsTab.has_value())
    {
        return false;
    }
    const auto paste = record.simulationsTab->actions.find("pasteSimulation");
    return paste != record.simulationsTab->actions.end() && paste->second;
}

/// The comparison of every step's record with OpenRocket's, the scenario's observer: QtRocket's
/// record against guiExpectedRecord() of OpenRocket's with the Undo items of kUndoDifferences
/// made QtRocket's (a test failure for each difference), noting the steps it saw and those
/// whose expected record differs from OpenRocket's.
class RecordComparison
{
public:
    explicit RecordComparison(const GuiReference& reference) noexcept : m_reference(&reference) { }

    void operator()(const GuiScenarioStep& step, const GuiStepRecord& record)
    {
        m_observed.push_back(step.index);
        const GuiStepRecord expected = expectedRecord(m_reference->steps.at(step.index));
        EXPECT_EQ(QtRocket::Test::guiRecordDifferences(expected, record),
                  std::vector<std::string>{})
            << SimulationsScenario::stepText(step);
    }

    /// The steps compared, in order.
    [[nodiscard]] const std::vector<std::size_t>& observed() const noexcept { return m_observed; }
    /// The steps whose expected Paste is not OpenRocket's.
    [[nodiscard]] const std::vector<std::size_t>& pasteDifferences() const noexcept
    {
        return m_paste;
    }
    /// The steps whose expected Undo item is not OpenRocket's.
    [[nodiscard]] const std::vector<std::size_t>& undoDifferences() const noexcept
    {
        return m_undo;
    }
    /// What did not hold of the expected differences.
    [[nodiscard]] const std::string& problems() const noexcept { return m_problems; }

private:
    /// What QtRocket's GUI is to show where OpenRocket's recorded @p openRocket.
    [[nodiscard]] GuiStepRecord expectedRecord(const GuiStepRecord& openRocket)
    {
        GuiStepRecord expected =
            QtRocket::Test::guiExpectedRecord(kGuiSimulationsScenario, openRocket, m_problems);
        if (pasteEnabled(expected) != pasteEnabled(openRocket))
        {
            m_paste.push_back(openRocket.index);
        }
        for (const UndoDifference& difference : kUndoDifferences)
        {
            if (difference.step != openRocket.index)
            {
                continue;
            }
            if (expected.undo.text != difference.openRocket)
            {
                m_problems +=
                    std::format("step {}: OpenRocket's Undo is '{}', not '{}'\n", difference.step,
                                expected.undo.text, difference.openRocket);
                continue;
            }
            expected.undo.text = std::string(difference.qtRocket);
            m_undo.push_back(openRocket.index);
        }
        return expected;
    }

    const GuiReference*      m_reference;
    std::vector<std::size_t> m_observed;
    std::vector<std::size_t> m_paste;
    std::vector<std::size_t> m_undo;
    std::string              m_problems;
};

/// The steps of @p differences.
template <std::size_t N>
[[nodiscard]] std::vector<std::size_t> undoDifferenceSteps(
    const std::array<UndoDifference, N>& differences)
{
    std::vector<std::size_t> steps;
    steps.reserve(N);
    for (const UndoDifference& difference : differences)
    {
        steps.push_back(difference.step);
    }
    return steps;
}

/// The fixture: the application of a SimulationsScenario with its preferences and saves in a
/// directory of the test's own; the configuration dialog's state forgotten before and after
/// (ConfigDialogTest's rule), the application's clipboard and the system clipboard left empty
/// (Copy puts the table's text there).
class SimulationsExitGolden : public Gui::Test::GuiTest
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
        clearSystemClipboard();
        ComponentConfigDialog::forgetPreviousDialogs();
        GuiTest::TearDown();
    }

    /// Starts the application (once).
    SimulationsScenario& start()
    {
        if (m_scenario == nullptr)
        {
            m_scenario = std::make_unique<SimulationsScenario>(m_directory.path());
        }
        return *m_scenario;
    }

    /// Starts the application and runs the scenario in it, calling @p observer after every step;
    /// the failure of a step, or "".
    [[nodiscard]] std::string runScenario(const SimulationsScenario::Observer& observer = {})
    {
        const Result<GuiScenario> scenario = loadSimulationsScenario();
        if (!scenario)
        {
            return scenario.error().toString();
        }
        return start().run(*scenario, observer);
    }

    [[nodiscard]] SimulationsScenario& scenario() const { return *m_scenario; }
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
    std::unique_ptr<SimulationsScenario> m_scenario;
};

// ================================================================================ the start-up

TEST_F(SimulationsExitGolden, TheApplicationStartsWithTheMotorsOfOpenRocketsStartUp)
{
    // OpenRocket's start-up of the reference loaded the bundled motors (state.json's "startup");
    // so does the application's (ApplicationContext: data/motors of the build tree).
    const Result<GuiReference> reference = loadSimulationsReference();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    const ThrustCurveMotorSetDatabase& motors = start().session().context().services().motors();
    EXPECT_EQ(motors.getMotorSets().size(), static_cast<std::size_t>(reference->startup.motorSets));
    EXPECT_EQ(motorsIn(motors), static_cast<std::size_t>(reference->startup.motors));
    EXPECT_EQ(scriptedKinds(scenario().prompts().prompts()), std::vector<std::string_view>{});
}

// ================================================================================== the steps

TEST_F(SimulationsExitGolden, EveryStepLeavesWhatOpenRocketsGuiShowed)
{
    const Result<GuiScenario>  steps     = loadSimulationsScenario();
    const Result<GuiReference> reference = loadSimulationsReference();
    ASSERT_TRUE(steps.has_value()) << steps.error().toString();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    ASSERT_EQ(reference->steps.size(), steps->steps.size());

    RecordComparison  comparison(*reference);
    const std::string failure = runScenario(std::ref(comparison));
    EXPECT_EQ(failure, "");
    EXPECT_EQ(comparison.problems(), "");
    // The expected differences are those listed, no more and no fewer.
    EXPECT_EQ(comparison.pasteDifferences(),
              std::vector<std::size_t>(kPasteDifferences.begin(), kPasteDifferences.end()));
    EXPECT_EQ(comparison.undoDifferences(), undoDifferenceSteps(kUndoDifferences));
    // Every step was done, in order, and recorded once.
    std::vector<std::size_t> all;
    for (const GuiScenarioStep& step : steps->steps)
    {
        all.push_back(step.index);
    }
    EXPECT_EQ(comparison.observed(), all);
    // The script was asked nothing beyond the files to open.
    EXPECT_EQ(scriptedKinds(scenario().prompts().prompts()), scriptedQuestionsOf(*reference));
}

TEST_F(SimulationsExitGolden, NoSimulationRanBeforeSaveAs)
{
    // As in OpenRocket's tool, every run dialog the settings dialog's OK opened was cancelled
    // before any of its simulations started: each job submitted was cancelled while it waited,
    // and no simulation has flight data.
    ASSERT_EQ(runScenario(), "");
    const Gui::Test::ManualSimulationExecutor& executor = scenario().executor();
    EXPECT_FALSE(executor.submittedJobs().empty());
    EXPECT_EQ(executor.cancelledJobs().size(), executor.submittedJobs().size());
    EXPECT_EQ(executor.waitingJobs(), std::vector<SimulationExecutor::JobId>{});
    const MainWindow* const window = scenario().window();
    ASSERT_NE(window, nullptr);
    for (const std::shared_ptr<Simulation>& simulation : window->document().getSimulations())
    {
        EXPECT_FALSE(simulation->hasSimulationData()) << simulation->getName();
    }
}

TEST_F(SimulationsExitGolden, SaveAsNamesTheWindowAfterTheFile)
{
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

TEST_F(SimulationsExitGolden, TheSaveOpensAgainAsTheDesignThatWasSaved)
{
    const Result<GuiReference> reference = loadSimulationsReference();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    ASSERT_EQ(runScenario(), "");
    const std::filesystem::path file = savedFile();
    ASSERT_FALSE(file.empty());
    const MainWindow* const saved = scenario().window();
    ASSERT_NE(saved, nullptr);
    const std::vector<QtRocket::Test::GuiSimulationState> simulations =
        QtRocket::Test::guiSimulationStates(saved->document());
    ASSERT_EQ(scenario().closeWindow(), "");
    std::string             problem;
    const MainWindow* const window = scenario().openSavedFile(problem);
    ASSERT_NE(window, nullptr) << problem;

    OpenRocketDocument& document = window->document();
    EXPECT_TRUE(document.isSaved());
    const std::filesystem::path reopened = document.getFile().value_or(std::filesystem::path());
    std::error_code             error;
    EXPECT_TRUE(!reopened.empty() && std::filesystem::equivalent(reopened, file, error));
    // The reopened design keeps every id of the saved one and is the saved design
    // (kGuiSimulationsReloadChanges: nothing changes), with its document preferences and its
    // simulations: their names, configurations, statuses and options.
    ASSERT_FALSE(scenario().savedIds().empty());
    std::string                    changes;
    const std::vector<std::string> reloaded = QtRocket::Test::guiReloadedTree(
        scenario().savedTree(), QtRocket::Test::guiReloadChanges(kGuiSimulationsScenario), changes);
    EXPECT_EQ(changes, "");
    EXPECT_EQ(QtRocket::Test::describeRocket(
                  document.getRocket(), scenario().session().context().services().preferences(),
                  scenario().savedIds()),
              reloaded);
    EXPECT_EQ(QtRocket::Test::documentPreferenceLines(document),
              scenario().savedDocumentPreferences());
    EXPECT_EQ(QtRocket::Test::guiSimulationStates(document), simulations);
    // The file was asked for, and the load gave no warning.
    std::vector<std::string_view> kinds = scriptedQuestionsOf(*reference);
    kinds.emplace_back("OPEN_FILES");
    EXPECT_EQ(scriptedKinds(scenario().prompts().prompts()), kinds);
}

// ================================================================= (b) OpenRocket's reference

TEST_F(SimulationsExitGolden, TheSaveLoadsIntoTheStateOfOpenRocketsSave)
{
    const Result<GuiScenario>  steps     = loadSimulationsScenario();
    const Result<GuiReference> reference = loadSimulationsReference();
    ASSERT_TRUE(steps.has_value()) << steps.error().toString();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    ASSERT_EQ(runScenario(), "");
    const std::filesystem::path file = savedFile();
    ASSERT_FALSE(file.empty());
    const std::string input = scenarioInputText(*steps);
    ASSERT_FALSE(input.empty());

    // The state of the load with OpenRocket's databases and fresh preferences (the simulations'
    // options among it), the drawn ids numbered on both sides.
    QtRocket::Test::GuiReferenceEnvironment environment(
        QtRocket::Test::GuiReferenceEnvironment::Databases::BUNDLED);
    const std::vector<std::string> state = QtRocket::Test::designFileState(
        environment.get(), file, QtRocket::Test::StateDetail::WITH_DOCUMENT_PREFERENCES);
    const std::vector<std::string> found =
        QtRocket::Test::linesOf(QtRocket::Test::withDrawnIdsNumbered(joinedLines(state), input));
    const std::vector<std::string> expected = QtRocket::Test::linesOf(
        QtRocket::Test::withDrawnIdsNumbered(joinedLines(reference->loadedLines), input));
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

TEST_F(SimulationsExitGolden, TheSaveIsTheTextOfOpenRocketsSave)
{
    const Result<GuiScenario>  steps     = loadSimulationsScenario();
    const Result<GuiReference> reference = loadSimulationsReference();
    ASSERT_TRUE(steps.has_value()) << steps.error().toString();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    ASSERT_EQ(runScenario(), "");
    const std::filesystem::path file = savedFile();
    ASSERT_FALSE(file.empty());
    const std::string input = scenarioInputText(*steps);
    ASSERT_FALSE(input.empty());
    const Result<std::vector<std::byte>> bytes = readFile(file);
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    const Result<ZipArchive> archive = ZipArchive::fromBytes(*bytes);
    ASSERT_TRUE(archive.has_value()) << archive.error().toString();

    // The archive holds OpenRocket's entries in OpenRocket's order: the document, then the thrust
    // curves of its motors, each with OpenRocket's bytes.
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
    // goldens: the ids the input does not spell out numbered on both sides
    // (withDrawnIdsNumbered()), the creator set aside and the numbers that read as the same
    // double counted apart (compareOrkTexts()). Every other byte is compared, line for line.
    const QtRocket::Test::OrkTextComparison comparison = QtRocket::Test::compareOrkTexts(
        QtRocket::Test::withDrawnIdsNumbered(bytesToString(*document), input),
        QtRocket::Test::withDrawnIdsNumbered(
            fileText(QtRocket::Test::guiScenarioDir(kGuiSimulationsScenario) / "rocket.ork"),
            input));
    EXPECT_EQ(comparison.differing, 0U) << comparison.differences;
    EXPECT_GT(comparison.lines, 200U);
}

// ============================================================================ (c) the interop

// The fixture of the interop check's save, a suite of its own (no "Golden" in its name: ctest
// lists it disabled, as the core's OrkInteropSaves).
using SimulationsInteropSave = SimulationsExitGolden;

// The scenario's save for the interop check of the milestone's exit (plan section 11.6 E7: the
// design QtRocket's simulations tab made opens in OpenRocket): a DISABLED_ test, which neither
// ctest nor CI runs. It writes build/<preset>/interop-saves/gui-simulations.zip-nodata.ork (an
// .ork archive without simulation data: no simulation of the scenario ran), or into the directory
// of --interop-saves=<directory>, replacing the one an earlier run wrote, and touches no other
// file. The core's writer (OrkInteropSaves of tests/core/goldens/ork_interop_saves_tests.cpp)
// removes it with its own saves of an earlier run, so it runs first. By hand, from the source
// tree, one command per line:
//
// clang-format off
//   build/<preset>/bin/QtRocket_core_tests --gtest_also_run_disabled_tests --gtest_filter='OrkInteropSaves.*'
//   build/<preset>/bin/QtRocket_gui_tests --gtest_also_run_disabled_tests --gtest_filter='*InteropSave*'
//   tools/openrocket-goldens/interop.sh build/<preset>/interop-saves
// clang-format on
//
// The input name of InteropCheck (tools/openrocket-goldens) for this save is "gui-simulations",
// against OpenRocket's save of the same scenario (tests/data/goldens/gui/simulations/rocket.ork).
TEST_F(SimulationsInteropSave, DISABLED_WritesTheSaveForTheInteropCheck)
{
    ASSERT_EQ(runScenario(), "");
    const std::filesystem::path file = savedFile();
    ASSERT_FALSE(file.empty());
    // InteropSaves.h: into the directory of the interop check's saves.
    const Result<std::filesystem::path> written = Gui::Test::writeInteropSave(
        file, std::format("gui-{}.zip-nodata.ork", kGuiSimulationsScenario));
    ASSERT_TRUE(written.has_value()) << written.error().message;
    std::println("{}", pathToUtf8(written.value_or(std::filesystem::path())));
}

}  // namespace

}  // namespace QtRocket::Gui
