// The driver of the scenarios of the Motors & Configuration tab (FlightConfigurationScenario) on
// short scenarios of its own: a step the GUI does not allow and a question the step has no answer
// for end the run with a failure that names the step, the records have the tab's members, and the
// steps of version 1 are FromScratchScenario's. FlightConfigurationsExitGolden runs the
// scenarios of tests/data/goldens/gui/.

#include "exit/FlightConfigurationScenario.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationPanel.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/util/Error.h"
#include "ScriptedUserPrompts.h"
#include "TestTempDir.h"
#include "goldens/GoldenGuiReference.h"

namespace QtRocket::Gui
{

namespace
{

using Gui::Test::FlightConfigurationScenario;
using QtRocket::Test::GuiFlightConfigurationState;
using QtRocket::Test::GuiScenario;
using QtRocket::Test::GuiScenarioStep;
using QtRocket::Test::GuiStepRecord;

/// A scenario of version 2 that opens the from-scratch reference's design and shows the Motors &
/// Configuration tab, then does @p steps (JSON text of the array's elements).
[[nodiscard]] Result<GuiScenario> tabScenario(std::string_view steps)
{
    return QtRocket::Test::parseGuiScenario(
        std::string(R"({"schema": "gui-scenario", "schemaVersion": 2, "title": "t", "steps": [)") +
        R"({"step": "open", "file": "tests/data/goldens/gui/from-scratch/rocket.ork"}, )" +
        R"({"step": "tab", "tab": "flightConfiguration"})" +
        (steps.empty() ? std::string() : ", " + std::string(steps)) + "]}");
}

/// The fixture: the configuration dialog's state forgotten before and after, the application's
/// component clipboard left empty; a scenario's application in a directory of the test's own.
class FlightConfigurationScenarioTest : public Gui::Test::GuiTest
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        ComponentConfigDialog::forgetPreviousDialogs();
    }

    void TearDown() override
    {
        OpenRocketClipboard::instance().clear();
        ComponentConfigDialog::forgetPreviousDialogs();
        GuiTest::TearDown();
    }

    /// The run of @p scenario in @p application, the records of its steps in @p records: the
    /// failure, or "".
    [[nodiscard]] static std::string runIn(FlightConfigurationScenario& application,
                                           const GuiScenario&           scenario,
                                           std::vector<GuiStepRecord>&  records)
    {
        return application.run(scenario,
                               [&records](const GuiScenarioStep&, const GuiStepRecord& record) {
                                   records.push_back(record);
                               });
    }

    QtRocket::Test::TempDir m_directory;
};

TEST_F(FlightConfigurationScenarioTest, TheOpenStepReplacesTheStartsDesignAndTheTabStepShowsTheTab)
{
    const Result<GuiScenario> scenario = tabScenario("");
    ASSERT_TRUE(scenario.has_value()) << scenario.error().toString();
    FlightConfigurationScenario application(m_directory.path());
    std::vector<GuiStepRecord>  records;
    EXPECT_EQ(runIn(application, *scenario, records), "");
    ASSERT_EQ(records.size(), 2U);
    EXPECT_EQ(records[0].tab, "design");
    EXPECT_EQ(records[1].tab, "flightConfiguration");
    ASSERT_TRUE(records[1].flightConfiguration.has_value());
    const GuiFlightConfigurationState tab =
        records[1].flightConfiguration.value_or(GuiFlightConfigurationState{});
    // The design has its inner tube as its motor mount and no configuration.
    EXPECT_EQ(tab.motors.header, (std::vector<std::string>{"Configuration", "Inner Tube"}));
    EXPECT_EQ(tab.selectedConfiguration, -1);
    EXPECT_TRUE(records[1].questions.empty());
    ASSERT_NE(application.tab(), nullptr);
    EXPECT_EQ(application.tab()->tabs()->currentIndex(), FlightConfigurationPanel::kMotorTabIndex);
    // The file chooser was the one question of the script.
    ASSERT_EQ(application.prompts().prompts().size(), 1U);
    EXPECT_EQ(application.prompts().prompts().front().kind,
              Gui::Test::ScriptedUserPrompts::Kind::OPEN_FILES);
}

TEST_F(FlightConfigurationScenarioTest, AStepTheGuiRefusesEndsTheRunWithItsName)
{
    // Remove configuration acts on the selected configuration, of which the design has none.
    const Result<GuiScenario> disabled =
        tabScenario(R"({"step": "deleteConfiguration"}, {"step": "undo"})");
    ASSERT_TRUE(disabled.has_value()) << disabled.error().toString();
    {
        FlightConfigurationScenario application(m_directory.path());
        std::vector<GuiStepRecord>  records;
        EXPECT_EQ(runIn(application, *disabled, records),
                  "step 2 (deleteConfiguration): the button removeConfButton is disabled");
        EXPECT_EQ(records.size(), 2U);
    }
    // Reset deployment is the Recovery sub-tab's, which is not the current one.
    const Result<GuiScenario> hidden = tabScenario(R"({"step": "resetDeployment"})");
    ASSERT_TRUE(hidden.has_value()) << hidden.error().toString();
    {
        FlightConfigurationScenario application(m_directory.path());
        std::vector<GuiStepRecord>  records;
        EXPECT_EQ(runIn(application, *hidden, records),
                  "step 2 (resetDeployment): the button resetDeploymentButton is not shown");
    }
    // The tab is the current one already.
    const Result<GuiScenario> shown =
        tabScenario(R"({"step": "tab", "tab": "flightConfiguration"})");
    ASSERT_TRUE(shown.has_value()) << shown.error().toString();
    {
        FlightConfigurationScenario application(m_directory.path());
        std::vector<GuiStepRecord>  records;
        EXPECT_EQ(runIn(application, *shown, records),
                  "step 2 (tab flightConfiguration): the tab flightConfiguration is shown "
                  "already, or disabled");
    }
}

TEST_F(FlightConfigurationScenarioTest, AQuestionTheStepHasNoAnswerForEndsTheRun)
{
    // New on a design with a motor mount asks the motor chooser for the new configuration; the
    // step has no answer, so the question goes to the script (Cancel, scripted once the tab is
    // shown: the script answers in the order of the questions, the file to open first).
    const Result<GuiScenario> scenario = tabScenario(R"({"step": "newConfiguration"})");
    ASSERT_TRUE(scenario.has_value()) << scenario.error().toString();
    FlightConfigurationScenario application(m_directory.path());
    const std::string           failure = application.run(
        *scenario, [&application](const GuiScenarioStep& step, const GuiStepRecord&) {
            if (step.index == 1)
            {
                application.prompts().answerMotor(std::nullopt);
            }
        });
    EXPECT_EQ(failure,
              "step 2 (newConfiguration): the operation asked a question the step has no answer "
              "for");
    ASSERT_EQ(application.prompts().motorQuestions().size(), 1U);
    EXPECT_EQ(QtStrings::toUtf8(application.prompts().motorQuestions().front().mountName),
              "Inner Tube");
}

TEST_F(FlightConfigurationScenarioTest, AnAnsweredQuestionIsRecordedAsItsDialogShowedIt)
{
    // New, the chooser cancelled: the new configuration stays (New's change), without a motor.
    const Result<GuiScenario> scenario = tabScenario(
        R"({"step": "newConfiguration", "answer": {"search": "A8", "button": "cancel"}})");
    ASSERT_TRUE(scenario.has_value()) << scenario.error().toString();
    FlightConfigurationScenario application(m_directory.path());
    std::vector<GuiStepRecord>  records;
    EXPECT_EQ(runIn(application, *scenario, records), "");
    ASSERT_EQ(records.size(), 3U);
    ASSERT_EQ(records[2].questions.size(), 1U);
    const QtRocket::Test::GuiQuestion& question = records[2].questions.front();
    EXPECT_EQ(question.kind, QtRocket::Test::GuiQuestionKind::MOTOR);
    EXPECT_EQ(question.title, "Select a rocket motor");
    EXPECT_EQ(question.shown.search, "");
    EXPECT_FALSE(question.shown.motor.has_value());
    EXPECT_EQ(question.answered.search, "A8");
    EXPECT_LT(question.answered.rows, question.shown.rows);
    ASSERT_TRUE(records[2].flightConfiguration.has_value());
    const GuiFlightConfigurationState tab =
        records[2].flightConfiguration.value_or(GuiFlightConfigurationState{});
    ASSERT_EQ(tab.motors.rows.size(), 1U);
    ASSERT_EQ(tab.motors.rows.front().motorCells.size(), 1U);
    EXPECT_EQ(tab.motors.rows.front().motorCells.front().motor, "None");
    // The step before asked nothing.
    EXPECT_TRUE(records[1].questions.empty());
}

}  // namespace

}  // namespace QtRocket::Gui
