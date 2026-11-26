// The driver of the scenario of the Flight simulations tab (SimulationsScenario, with its
// questions SimulationsPrompts) on short scenarios of its own: a step the GUI does not allow (a
// disabled button, a column shown already, a value a field or a CSV import setting shows already,
// a setting the CSV import does not show, a lookup action that changes nothing), a question the
// step has no answer for and an answer no dialog takes end the run with a failure that names the
// step, as OpenRocket's tool refuses them (tools/openrocket-gui-reference,
// SimulationQuestions.java); an answered confirmation is recorded as its dialog showed it.
// SimulationsExitGolden runs the scenario of tests/data/goldens/gui/simulations/.

#include "exit/SimulationsScenario.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QClipboard>
#include <QGuiApplication>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "ManualSimulationExecutor.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/simulation/SimulationPanel.h"
#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/util/Error.h"
#include "ScriptedUserPrompts.h"
#include "TestTempDir.h"
#include "goldens/GoldenGuiReference.h"

namespace QtRocket::Gui
{

namespace
{

using Gui::Test::ScriptedUserPrompts;
using Gui::Test::SimulationsScenario;
using QtRocket::Test::GuiQuestion;
using QtRocket::Test::GuiQuestionKind;
using QtRocket::Test::GuiScenario;
using QtRocket::Test::GuiScenarioStep;
using QtRocket::Test::GuiSimulationsTabState;
using QtRocket::Test::GuiStepRecord;

/// A scenario of version 3 that opens the flight-configurations reference's design (three
/// simulations without data) and shows the Flight simulations tab (its first row selected), then
/// does @p steps (JSON text of the array's elements).
[[nodiscard]] Result<GuiScenario> simulationsTabScenario(std::string_view steps)
{
    return QtRocket::Test::parseGuiScenario(
        std::string(R"({"schema": "gui-scenario", "schemaVersion": 3, "title": "t", "steps": [)") +
        R"({"step": "open", "file": "tests/data/goldens/gui/flight-configurations/rocket.ork"}, )" +
        R"({"step": "tab", "tab": "simulations"})" +
        (steps.empty() ? std::string() : ", " + std::string(steps)) + "]}");
}

/// The simulations tab of @p record, or an empty one.
[[nodiscard]] GuiSimulationsTabState tabOfRecord(const GuiStepRecord& record)
{
    return record.simulationsTab.value_or(GuiSimulationsTabState{});
}

/// The files of the file choosers @p record's questions show, in their order: the file chosen, or
/// nullopt for a cancelled chooser.
[[nodiscard]] std::vector<std::optional<std::string>> fileQuestionsOf(const GuiStepRecord& record)
{
    std::vector<std::optional<std::string>> files;
    for (const GuiQuestion& question : record.questions)
    {
        if (question.kind == GuiQuestionKind::OPEN_FILE)
        {
            files.push_back(question.file);
        }
    }
    return files;
}

/// The fixture: the configuration dialog's state forgotten before and after, the application's
/// clipboard and the system clipboard left empty; a scenario's application in a directory of the
/// test's own.
class SimulationsScenarioTest : public Gui::Test::GuiTest
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        ComponentConfigDialog::forgetPreviousDialogs();
    }

    void TearDown() override
    {
        m_application = nullptr;
        OpenRocketClipboard::instance().clear();
        QGuiApplication::clipboard()->clear();
        ComponentConfigDialog::forgetPreviousDialogs();
        GuiTest::TearDown();
    }

    /// The application (started once).
    [[nodiscard]] SimulationsScenario& application()
    {
        if (m_application == nullptr)
        {
            m_application = std::make_unique<SimulationsScenario>(m_directory.path());
        }
        return *m_application;
    }

    /// The run of the scenario of @p steps (simulationsTabScenario()) in the application, the
    /// records of its steps in m_records: the failure, or "".
    [[nodiscard]] std::string runSteps(std::string_view steps)
    {
        const Result<GuiScenario> scenario = simulationsTabScenario(steps);
        if (!scenario)
        {
            return "the scenario does not parse: " + scenario.error().toString();
        }
        return application().run(*scenario,
                                 [this](const GuiScenarioStep&, const GuiStepRecord& record) {
                                     m_records.push_back(record);
                                 });
    }

    /// The failure of the scenario of @p steps in an application of its own (the one of the test
    /// replaced).
    [[nodiscard]] std::string failureOf(std::string_view steps)
    {
        m_application = nullptr;
        m_records.clear();
        return runSteps(steps);
    }

    std::vector<GuiStepRecord> m_records;

private:
    QtRocket::Test::TempDir              m_directory;
    std::unique_ptr<SimulationsScenario> m_application;
};

TEST_F(SimulationsScenarioTest, TheTabStepShowsTheTabWithItsFirstRowSelected)
{
    EXPECT_EQ(runSteps(""), "");
    ASSERT_EQ(m_records.size(), 2U);
    EXPECT_EQ(m_records[1].tab, "simulations");
    const GuiSimulationsTabState tab = tabOfRecord(m_records[1]);
    EXPECT_EQ(tab.rows.size(), 3U);
    EXPECT_EQ(tab.selectedRows, std::vector<int>{0});
    EXPECT_EQ(m_records[1].simulations.size(), 3U);
    EXPECT_TRUE(m_records[1].questions.empty());
}

TEST_F(SimulationsScenarioTest, AStepTheGuiRefusesEndsTheRunWithItsName)
{
    // Edit simulation acts on the selected simulations, of which a click below the rows leaves
    // none.
    EXPECT_EQ(failureOf(R"({"step": "selectSimulations", "rows": []}, )"
                        R"({"step": "editSimulation", "answer": {"button": "cancel"}})"),
              "step 3 (editSimulation): the button Edit simulation is disabled");
    EXPECT_EQ(m_records.size(), 3U);
    // The apogee column is shown.
    EXPECT_EQ(failureOf(R"({"step": "columnVisibility", "column": "apogee", "visible": true})"),
              "step 2 (columnVisibility): the column chooser's item apogee is shown already, or "
              "disabled");
    // Simulation 1's launch rod is 1 m long already: the edit of a field must change it.
    EXPECT_EQ(failureOf(R"({"step": "editSimulation", "answer": {"edits": [)"
                        R"({"field": "LaunchRodLength", "text": "1", "unit": "m"}], )"
                        R"("button": "ok"}})"),
              "step 2 (editSimulation): edit 0 of the settings dialog: the field LaunchRodLength "
              "shows that value already");
}

TEST_F(SimulationsScenarioTest, ACsvImportSettingTheDialogDoesNotLetAUserSetEndsTheRun)
{
    // With the header box on (the import's default), each row shows its column name, not its
    // index (the tool's csvColumn()).
    EXPECT_EQ(failureOf(R"({"step": "editSimulation", "answer": {"edits": [)"
                        R"({"windModel": "MULTI_LEVEL"}, {"windLevels": {"edits": [)"
                        R"({"importLevels": {"settings": {"columns": {"speed": {"index": 2}}, )"
                        R"("button": "ok"}}}], "button": "close"}}], )"
                        R"("button": "ok", "run": {"button": "cancel"}}})"),
              "step 2 (editSimulation): the column index of speed is not shown");
    // The altitude column's unit is metres.
    EXPECT_EQ(failureOf(R"({"step": "editSimulation", "answer": {"edits": [)"
                        R"({"windModel": "MULTI_LEVEL"}, {"windLevels": {"edits": [)"
                        R"({"importLevels": {"settings": {"columns": {"altitude": {"unit": "m"}}, )"
                        R"("button": "ok"}}}], "button": "close"}}], )"
                        R"("button": "ok", "run": {"button": "cancel"}}})"),
              "step 2 (editSimulation): the unit of altitude is m already");
    // Without the header row, each row shows its column index, not its name.
    EXPECT_EQ(failureOf(R"({"step": "editSimulation", "answer": {"edits": [)"
                        R"({"windModel": "MULTI_LEVEL"}, {"windLevels": {"edits": [)"
                        R"({"importLevels": {"settings": {"hasHeader": false, )"
                        R"("columns": {"speed": {"name": "v"}}, "button": "ok"}}}], )"
                        R"("button": "close"}}], "button": "ok", "run": {"button": "cancel"}}})"),
              "step 2 (editSimulation): the column name of speed is not shown");
}

TEST_F(SimulationsScenarioTest, ALookupActionThatChangesNothingEndsTheRun)
{
    // The same file loaded again leaves the drag section as the first load left it.
    EXPECT_EQ(failureOf(R"({"step": "editSimulation", "answer": {"edits": [{"lookup": {"drag": [)"
                        R"({"load": {"file": "tests/data/goldens/gui/simulations/drag.csv", )"
                        R"("button": "open"}}, )"
                        R"({"load": {"file": "tests/data/goldens/gui/simulations/drag.csv", )"
                        R"("button": "open"}}], "button": "ok"}}], )"
                        R"("button": "cancel", "discard": {"button": "yes"}}})"),
              "step 2 (editSimulation): the drag action left the lookup dialog as it was");
    // A load whose file chooser is cancelled changes nothing, and is no edit that must: the step
    // goes on (the table loaded before it cleared, as before a Save as).
    EXPECT_EQ(failureOf(R"({"step": "editSimulation", "answer": {"edits": [)"
                        R"({"field": "LaunchRodLength", "text": "2", "unit": "m"}, )"
                        R"({"lookup": {"drag": [)"
                        R"({"load": {"file": "tests/data/goldens/gui/simulations/drag.csv", )"
                        R"("button": "open"}}, {"load": {"button": "cancel"}}, {"clear": true}], )"
                        R"("button": "ok"}}], "button": "ok", "run": {"button": "cancel"}}})"),
              "");
    ASSERT_EQ(m_records.size(), 3U);
    EXPECT_EQ(fileQuestionsOf(m_records[2]),
              (std::vector<std::optional<std::string>>{
                  "tests/data/goldens/gui/simulations/drag.csv", std::nullopt}));
}

TEST_F(SimulationsScenarioTest, AQuestionTheStepHasNoAnswerForEndsTheRun)
{
    // Cancel after a change asks the discard question, for which the step has no answer: it goes
    // to the script (Yes, scripted once the tab is shown: the script answers in the order of the
    // questions, the file to open first).
    const Result<GuiScenario> scenario =
        simulationsTabScenario(R"({"step": "editSimulation", "answer": {"edits": [)"
                               R"({"name": "Renamed"}], "button": "cancel"}})");
    ASSERT_TRUE(scenario.has_value()) << scenario.error().toString();
    SimulationsScenario& started = application();
    const std::string    failure =
        started.run(*scenario, [&started](const GuiScenarioStep& step, const GuiStepRecord&) {
            if (step.index == 1)
            {
                started.prompts().answerDiscardChanges(
                    UserPrompts::DiscardAnswer{.discard = true, .doNotAskAgain = false});
            }
        });
    EXPECT_EQ(failure,
              "step 2 (editSimulation): Edit simulation asked a question the step has no answer "
              "for");
    ASSERT_EQ(started.prompts().prompts().size(), 2U);
    EXPECT_EQ(started.prompts().prompts().at(1).kind, ScriptedUserPrompts::Kind::DISCARD_CHANGES);
}

TEST_F(SimulationsScenarioTest, AnAnswerNoDialogTakesEndsTheRun)
{
    // Once "Do not ask me again" was ticked, Delete asks nothing: the step's answer is left over.
    EXPECT_EQ(
        failureOf(
            R"({"step": "deleteSimulations", "answer": {"button": "ok", "doNotAskAgain": true}}, )"
            R"({"step": "selectSimulations", "rows": [{"row": 0}]}, )"
            R"({"step": "deleteSimulations", "answer": {"button": "ok"}})"),
        "step 4 (deleteSimulations): no dialog took the answer to the delete confirmation");
    // The failed step is not recorded; its Delete deleted the second simulation all the same.
    EXPECT_EQ(m_records.size(), 4U);
    ASSERT_NE(application().simulationsTab(), nullptr);
    EXPECT_EQ(application().simulationsTab()->tableModel().rowCount(), 1);
}

TEST_F(SimulationsScenarioTest, AnAnsweredConfirmationIsRecordedAsItsDialogShowedIt)
{
    // Delete, its confirmation cancelled: the question as OpenRocket's tool records it, nothing
    // deleted.
    EXPECT_EQ(runSteps(R"({"step": "deleteSimulations", "answer": {"button": "cancel"}})"), "");
    ASSERT_EQ(m_records.size(), 3U);
    ASSERT_EQ(m_records[2].questions.size(), 1U);
    const GuiQuestion& question = m_records[2].questions.front();
    EXPECT_EQ(question.kind, GuiQuestionKind::CONFIRM);
    EXPECT_EQ(question.title, "Delete simulations");
    EXPECT_EQ(question.message,
              (std::vector<std::string>{"Delete the selected simulations?", "",
                                        "You can change the default operation in the "
                                        "preferences."}));
    EXPECT_EQ(question.checkBoxes, std::vector<std::string>{"Do not ask me again"});
    EXPECT_EQ(question.buttons, (std::vector<std::string>{"OK", "Cancel"}));
    EXPECT_EQ(m_records[2].simulations.size(), 3U);
    EXPECT_EQ(tabOfRecord(m_records[2]).selectedRows, std::vector<int>{0});
    // No simulation ran, and the step before asked nothing of the tab.
    EXPECT_TRUE(application().executor().waitingJobs().empty());
    EXPECT_TRUE(m_records[1].questions.empty());
}

}  // namespace

}  // namespace QtRocket::Gui
