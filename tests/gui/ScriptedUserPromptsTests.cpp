#include "ScriptedUserPrompts.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include <QString>
#include <QStringList>
#include <QWidget>
#include <QtContainerFwd>

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "TestGuiServices.h"
#include "document/TestDocuments.h"
#include "rocket/preset/ExamplePresets.h"

// The test double itself: it answers in order, records every question, and fails a test that
// asks what the script does not answer or leaves an answer unused.

namespace QtRocket::Gui::Test
{

namespace
{

using ScriptedUserPromptsTest = GuiTest;
using Kind                    = ScriptedUserPrompts::Kind;

std::filesystem::path savedFile(const UserPrompts::SaveFileAnswer& answer)
{
    return answer.file;
}

std::vector<std::filesystem::path> openedFiles(const UserPrompts::OpenFileAnswer& answer)
{
    return answer.files;
}

TEST_F(ScriptedUserPromptsTest, AnswersFromTheScriptInOrder)
{
    ScriptedUserPrompts prompts;
    prompts.answerSaveChanges(UserPrompts::SaveChangesAnswer::DISCARD);
    prompts.answerOverwrite(true);
    prompts.answerInsertPosition(
        {.position = UserPrompts::InsertPosition::ADD_TO_END, .doNotAskAgain = true});
    prompts.answerStorageOptions(false);
    prompts.answerYesNo(true);

    EXPECT_EQ(prompts.askSaveChanges(nullptr, QStringLiteral("Rocket")),
              UserPrompts::SaveChangesAnswer::DISCARD);
    EXPECT_TRUE(prompts.askOverwrite(nullptr, QStringLiteral("rocket.ork")));
    const UserPrompts::InsertPositionAnswer position =
        prompts.askInsertPosition(nullptr, UserPrompts::InsertPositionQuestion::STAGE);
    EXPECT_EQ(position.position, UserPrompts::InsertPosition::ADD_TO_END);
    EXPECT_TRUE(position.doNotAskAgain);
    EXPECT_EQ(prompts.askStorageOptions(nullptr, true), std::optional<bool>(false));
    const UserPrompts::Question question{.severity = UserPrompts::Severity::WARNING,
                                         .title    = QStringLiteral("Reset"),
                                         .text     = QStringLiteral("Reset all preferences?")};
    EXPECT_TRUE(prompts.askYesNo(nullptr, question));
    EXPECT_EQ(prompts.unusedAnswers(), 0U);

    ASSERT_EQ(prompts.prompts().size(), 5U);
    EXPECT_EQ(prompts.prompts().at(0).kind, Kind::SAVE_CHANGES);
    EXPECT_EQ(prompts.prompts().at(0).texts, QStringList{QStringLiteral("Rocket")});
    EXPECT_EQ(prompts.prompts().at(2).texts, QStringList{QStringLiteral("STAGE")});
    EXPECT_EQ(prompts.prompts().at(4).title, QStringLiteral("Reset"));
}

TEST_F(ScriptedUserPromptsTest, AnswersTheFileQuestionsAndKeepsTheirRequests)
{
    ScriptedUserPrompts         prompts;
    const std::filesystem::path file = std::filesystem::path("designs") / "rocket.ork";
    prompts.answerSaveFile(UserPrompts::SaveFileAnswer{
        .file = file, .directory = file.parent_path(), .saveSimulationData = true});
    prompts.answerOpenFiles(std::nullopt);
    prompts.answerDecalFile(
        UserPrompts::OpenFileAnswer{.files = {file}, .directory = file.parent_path()});

    UserPrompts::SaveFileRequest save;
    save.title              = QStringLiteral("Save design as");
    save.fileName           = QStringLiteral("Rocket.ork");
    save.saveSimulationData = true;
    EXPECT_EQ(prompts.askSaveFile(nullptr, save).transform(&savedFile),
              std::optional<std::filesystem::path>(file));
    EXPECT_EQ(prompts.askOpenFiles(nullptr, {}), std::nullopt);
    EXPECT_EQ(prompts.askDecalFile(nullptr, QStringLiteral("No decal source file"), "designs")
                  .transform(&openedFiles),
              std::optional<std::vector<std::filesystem::path>>({file}));

    ASSERT_EQ(prompts.saveFileRequests().size(), 1U);
    EXPECT_EQ(prompts.saveFileRequests().front().fileName, QStringLiteral("Rocket.ork"));
    EXPECT_EQ(prompts.openFileRequests().size(), 1U);
}

TEST_F(ScriptedUserPromptsTest, EditsTheDesignInformationAsScripted)
{
    ScriptedUserPrompts prompts;
    prompts.answerDesignInformation(
        [](OpenRocketDocument& document) { document.getRocket().setDesigner("Sampo"); });
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const TestGuiServices services;
    prompts.editDesignInformation(nullptr, services.services(), *document);
    EXPECT_EQ(document->getRocket().getDesigner(), "Sampo");
}

TEST_F(ScriptedUserPromptsTest, RecordsMessagesWithoutAScript)
{
    ScriptedUserPrompts prompts;
    prompts.showMessage(nullptr, {.severity = UserPrompts::Severity::ERROR,
                                  .title    = QStringLiteral("Saving failed"),
                                  .lines = {QStringLiteral("An I/O error occurred while saving:"),
                                            QStringLiteral("disk full")}});
    prompts.showWarnings(nullptr,
                         {.title    = QStringLiteral("Warnings while opening file"),
                          .lines    = {QStringLiteral("The following problems were encountered")},
                          .warnings = {QStringLiteral("Unknown element")}});
    ASSERT_EQ(prompts.prompts().size(), 2U);
    EXPECT_EQ(prompts.prompts().at(0).kind, Kind::MESSAGE);
    EXPECT_EQ(
        prompts.prompts().at(0).texts,
        (QStringList{QStringLiteral("ERROR"), QStringLiteral("An I/O error occurred while saving:"),
                     QStringLiteral("disk full")}));
    EXPECT_EQ(prompts.prompts().at(1).kind, Kind::WARNINGS);
    EXPECT_EQ(prompts.prompts().at(1).texts.size(), 2);
}

TEST_F(ScriptedUserPromptsTest, RecordsErrorsAndWarnings)
{
    ScriptedUserPrompts prompts;
    prompts.showErrorsAndWarnings(nullptr,
                                  {.title    = QStringLiteral("Errors/Warnings while saving file"),
                                   .lines    = {QStringLiteral("line")},
                                   .errors   = {QStringLiteral("error")},
                                   .warnings = {QStringLiteral("warning")}});
    ASSERT_EQ(prompts.prompts().size(), 1U);
    EXPECT_EQ(prompts.prompts().front().kind, Kind::ERRORS_AND_WARNINGS);
    EXPECT_EQ(prompts.prompts().front().title, QStringLiteral("Errors/Warnings while saving file"));
    EXPECT_EQ(
        prompts.prompts().front().texts,
        (QStringList{QStringLiteral("line"), QStringLiteral("error"), QStringLiteral("warning")}));
    EXPECT_EQ(ScriptedUserPrompts::kindName(Kind::ERRORS_AND_WARNINGS), "ERRORS_AND_WARNINGS");
}

TEST_F(ScriptedUserPromptsTest, AnswersTheInternalErrorAndRecordsTheFailure)
{
    ScriptedUserPrompts prompts;
    prompts.answerInternalError(UserPrompts::InternalErrorAnswer::QUIT);
    Guard::Failure failure;
    failure.kind    = Guard::Failure::Kind::BUG;
    failure.message = QStringLiteral("BUG: broken (Rocket.cpp:12)");
    failure.context = QStringLiteral("Delete");
    EXPECT_EQ(prompts.showInternalError(nullptr, failure), UserPrompts::InternalErrorAnswer::QUIT);
    ASSERT_EQ(prompts.prompts().size(), 1U);
    EXPECT_EQ(prompts.prompts().front().kind, Kind::INTERNAL_ERROR);
    EXPECT_EQ(prompts.prompts().front().texts,
              (QStringList{QStringLiteral("BUG"), QStringLiteral("BUG: broken (Rocket.cpp:12)"),
                           QStringLiteral("Delete")}));
}

TEST_F(ScriptedUserPromptsTest, AnUnscriptedInternalErrorFailsTheTestAndContinues)
{
    ScriptedUserPrompts              prompts;
    UserPrompts::InternalErrorAnswer answer = UserPrompts::InternalErrorAnswer::QUIT;
    EXPECT_NONFATAL_FAILURE(answer = prompts.showInternalError(nullptr, Guard::Failure{}),
                            "unexpected INTERNAL_ERROR question: the script has no answer left");
    EXPECT_EQ(answer, UserPrompts::InternalErrorAnswer::CONTINUE);
}

TEST_F(ScriptedUserPromptsTest, AnswersTheValueDialogsAndKeepsTheirRequests)
{
    ScriptedUserPrompts prompts;
    prompts.answerIgnition(
        UserPrompts::IgnitionChoice{.scope = UserPrompts::ConfigurationScope::THIS_CONFIGURATION,
                                    .event = IgnitionEvent::BURNOUT,
                                    .delay = 1.5});
    prompts.answerDeployment(std::nullopt);
    StageSeparationConfiguration separation;
    separation.setSeparationEvent(StageSeparationConfiguration::SeparationEvent::NEVER);
    prompts.answerSeparation(UserPrompts::SeparationChoice{
        .scope = UserPrompts::ConfigurationScope::ALL_CONFIGURATIONS, .separation = separation});

    const std::optional<UserPrompts::IgnitionChoice> ignition = prompts.askIgnition(
        nullptr, {.configurationName = QStringLiteral("[B4-4]"),
                  .initial = {.scope = UserPrompts::ConfigurationScope::ALL_CONFIGURATIONS,
                              .event = IgnitionEvent::AUTOMATIC,
                              .delay = 0.0}});
    EXPECT_TRUE(ignition.has_value());
    const UserPrompts::IgnitionChoice ignited = ignition.value_or(UserPrompts::IgnitionChoice{});
    EXPECT_EQ(ignited.scope, UserPrompts::ConfigurationScope::THIS_CONFIGURATION);
    EXPECT_EQ(ignited.event, IgnitionEvent::BURNOUT);
    EXPECT_EQ(ignited.delay, 1.5);
    DeploymentConfiguration deployment;
    deployment.setDeployEvent(DeploymentConfiguration::DeployEvent::ALTITUDE);
    EXPECT_FALSE(
        prompts
            .askDeployment(
                nullptr, {.configurationName = QStringLiteral("[C6-5]"),
                          .initial = {.scope = UserPrompts::ConfigurationScope::THIS_CONFIGURATION,
                                      .deployment = deployment},
                          .bottomStage = true})
            .has_value());
    const std::optional<UserPrompts::SeparationChoice> separated = prompts.askSeparation(
        nullptr, {.configurationName = QStringLiteral("[C6-0; B4-4]"), .initial = {}});
    EXPECT_TRUE(separated.has_value());
    EXPECT_EQ(separated.value_or(UserPrompts::SeparationChoice{}).separation.getSeparationEvent(),
              StageSeparationConfiguration::SeparationEvent::NEVER);
    EXPECT_EQ(prompts.unusedAnswers(), 0U);

    ASSERT_EQ(prompts.prompts().size(), 3U);
    EXPECT_EQ(prompts.prompts().at(0).kind, Kind::IGNITION);
    EXPECT_EQ(prompts.prompts().at(0).texts,
              (QStringList{QStringLiteral("[B4-4]"), QStringLiteral("ALL_CONFIGURATIONS"),
                           QStringLiteral("AUTOMATIC"), QStringLiteral("0")}));
    EXPECT_EQ(prompts.prompts().at(1).kind, Kind::DEPLOYMENT);
    EXPECT_EQ(prompts.prompts().at(1).texts,
              (QStringList{QStringLiteral("[C6-5]"), QStringLiteral("THIS_CONFIGURATION"),
                           QStringLiteral("ALTITUDE"), QStringLiteral("200"), QStringLiteral("0"),
                           QStringLiteral("true")}));
    EXPECT_EQ(prompts.prompts().at(2).kind, Kind::SEPARATION);
    EXPECT_EQ(
        prompts.prompts().at(2).texts,
        (QStringList{QStringLiteral("[C6-0; B4-4]"), QStringLiteral("ALL_CONFIGURATIONS"),
                     QStringLiteral("EJECTION"), QStringLiteral("200"), QStringLiteral("0")}));
    ASSERT_EQ(prompts.ignitionRequests().size(), 1U);
    EXPECT_EQ(prompts.ignitionRequests().front().configurationName, QStringLiteral("[B4-4]"));
    ASSERT_EQ(prompts.deploymentRequests().size(), 1U);
    EXPECT_TRUE(prompts.deploymentRequests().front().bottomStage);
    EXPECT_EQ(prompts.deploymentRequests().front().initial.deployment, deployment);
    EXPECT_EQ(prompts.separationRequests().size(), 1U);
    EXPECT_EQ(ScriptedUserPrompts::kindName(Kind::SEPARATION), "SEPARATION");
}

/// What a rename's "Save as default" was called with.
struct SaveAsDefaultCall
{
    QWidget* dialog;
    QString  name;
};

/// A rename's "Save as default" callback that records its calls.
struct RecordSaveAsDefault
{
    std::vector<SaveAsDefaultCall>* calls;

    void operator()(QWidget* dialog, const QString& name) const
    {
        calls->push_back({.dialog = dialog, .name = name});
    }
};

TEST_F(ScriptedUserPromptsTest, TheRenameClicksSaveAsDefaultBeforeItAnswers)
{
    ScriptedUserPrompts prompts;
    prompts.answerRenameConfiguration(
        {.action = UserPrompts::RenameAction::RENAME, .name = QStringLiteral("Flight {motors}")},
        {QStringLiteral("My {motors}"), QStringLiteral("[{motors}]")});
    prompts.answerRenameConfiguration(
        {.action = UserPrompts::RenameAction::RESET, .name = QString()});
    std::vector<SaveAsDefaultCall>                calls;
    QWidget                                       window;
    const UserPrompts::RenameConfigurationRequest request{
        .name          = QStringLiteral("[{motors}]"),
        .saveAsDefault = RecordSaveAsDefault{.calls = &calls}};

    const UserPrompts::RenameConfigurationAnswer renamed =
        prompts.askRenameConfiguration(&window, request);
    EXPECT_EQ(renamed.action, UserPrompts::RenameAction::RENAME);
    EXPECT_EQ(renamed.name, QStringLiteral("Flight {motors}"));
    ASSERT_EQ(calls.size(), 2U);
    EXPECT_EQ(calls.at(0).dialog, &window);
    EXPECT_EQ(calls.at(0).name, QStringLiteral("My {motors}"));
    EXPECT_EQ(calls.at(1).name, QStringLiteral("[{motors}]"));
    EXPECT_EQ(prompts.askRenameConfiguration(nullptr, request).action,
              UserPrompts::RenameAction::RESET);
    EXPECT_EQ(calls.size(), 2U);
    ASSERT_EQ(prompts.prompts().size(), 2U);
    EXPECT_EQ(prompts.prompts().front().kind, Kind::RENAME_CONFIGURATION);
    EXPECT_EQ(prompts.prompts().front().texts, QStringList{QStringLiteral("[{motors}]")});
}

TEST_F(ScriptedUserPromptsTest, ASaveAsDefaultClickWithoutTheCallbackFailsTheTest)
{
    ScriptedUserPrompts prompts;
    prompts.answerRenameConfiguration({.action = UserPrompts::RenameAction::CANCEL, .name = {}},
                                      {QStringLiteral("default")});
    UserPrompts::RenameConfigurationAnswer answer{.action = UserPrompts::RenameAction::RENAME,
                                                  .name   = QString()};
    EXPECT_NONFATAL_FAILURE(
        answer = prompts.askRenameConfiguration(
            nullptr, {.name = QStringLiteral("[{motors}]"), .saveAsDefault = {}}),
        "the script clicks Save as default, but the rename has no callback");
    EXPECT_EQ(answer.action, UserPrompts::RenameAction::CANCEL);
}

TEST_F(ScriptedUserPromptsTest, AnswersTheMotorChooserAndKeepsTheMountByNameAndId)
{
    const TestGuiServices                         services;
    ScriptedUserPrompts                           prompts;
    const std::shared_ptr<const ThrustCurveMotor> motor = QtRocket::Test::getTestMotor();
    prompts.answerMotor(
        UserPrompts::MotorChoice{.motor = motor, .delay = 5.0, .nozzleExitDiameter = 0.004});
    prompts.answerMotor(std::nullopt);
    InnerTube mount;
    mount.setName("Motor tube");
    const FlightConfigurationId configuration = FlightConfigurationId::fromString("1-2-3-4-5");

    const std::optional<UserPrompts::MotorChoice> chosen =
        prompts.chooseMotor(nullptr, services.services(), mount, configuration);
    EXPECT_TRUE(chosen.has_value());
    const UserPrompts::MotorChoice choice = chosen.value_or(UserPrompts::MotorChoice{});
    EXPECT_EQ(choice.motor, motor);
    EXPECT_EQ(choice.delay, 5.0);
    EXPECT_EQ(choice.nozzleExitDiameter, 0.004);
    EXPECT_FALSE(
        prompts.chooseMotor(nullptr, services.services(), mount, configuration).has_value());

    ASSERT_EQ(prompts.motorQuestions().size(), 2U);
    EXPECT_EQ(prompts.motorQuestions().front().mountName, QStringLiteral("Motor tube"));
    EXPECT_EQ(prompts.motorQuestions().front().mountId, mount.getId());
    EXPECT_EQ(prompts.motorQuestions().front().configurationId, configuration);
    EXPECT_EQ(prompts.prompts().front().kind, Kind::MOTOR);
    EXPECT_EQ(prompts.prompts().front().texts,
              (QStringList{QStringLiteral("Motor tube"),
                           QStringLiteral("00000001-0002-0003-0004-000000000005")}));
}

TEST_F(ScriptedUserPromptsTest, AnswersThePartsLibraryAndKeepsItsRequests)
{
    const TestGuiServices         services;
    ScriptedUserPrompts           prompts;
    const ComponentPresetDatabase presets = QtRocket::Test::makeExamplePresetDatabase();
    const std::shared_ptr<const ComponentPreset>& preset = presets.listAll().front();
    prompts.answerPreset({.outcome = UserPrompts::PresetOutcome::CHOSEN, .preset = preset});
    prompts.answerPreset({.outcome = UserPrompts::PresetOutcome::NONE, .preset = nullptr});

    const UserPrompts::PresetRequest request{.componentKind   = ComponentKind::INNER_TUBE,
                                             .foreDiameter    = 0.0248,
                                             .aftDiameter     = std::nullopt,
                                             .flippedNoseCone = false};
    const UserPrompts::PresetAnswer  chosen =
        prompts.choosePreset(nullptr, services.services(), request);
    EXPECT_EQ(chosen.outcome, UserPrompts::PresetOutcome::CHOSEN);
    EXPECT_EQ(chosen.preset, preset);
    EXPECT_EQ(prompts.choosePreset(nullptr, services.services(), request).outcome,
              UserPrompts::PresetOutcome::NONE);
    // Without an answer left: a test failure and "closed without a choice".
    UserPrompts::PresetAnswer unanswered{.outcome = UserPrompts::PresetOutcome::CHOSEN,
                                         .preset  = preset};
    EXPECT_NONFATAL_FAILURE(
        unanswered = prompts.choosePreset(nullptr, services.services(), request),
        "unexpected PRESET question: the script has no answer left");
    EXPECT_EQ(unanswered.outcome, UserPrompts::PresetOutcome::CLOSED);
    EXPECT_EQ(unanswered.preset, nullptr);

    ASSERT_EQ(prompts.presetRequests().size(), 3U);
    EXPECT_EQ(prompts.presetRequests().front().componentKind, ComponentKind::INNER_TUBE);
    EXPECT_EQ(prompts.presetRequests().front().foreDiameter, std::optional<double>(0.0248));
    // The English component name and its preset type (BODY_TUBE for an inner tube).
    EXPECT_EQ(
        prompts.prompts().front().texts,
        (QStringList{QStringLiteral("Inner Tube"), QStringLiteral("BODY_TUBE"),
                     QStringLiteral("0.0248"), QStringLiteral("none"), QStringLiteral("false")}));
}

TEST_F(ScriptedUserPromptsTest, AnAnswerThatContradictsItselfFailsTheTest)
{
    ScriptedUserPrompts prompts;
    EXPECT_NONFATAL_FAILURE(
        prompts.answerPreset({.outcome = UserPrompts::PresetOutcome::CHOSEN, .preset = nullptr}),
        "a scripted CHOSEN parts library answer without a preset");
    EXPECT_NONFATAL_FAILURE(prompts.answerMotor(UserPrompts::MotorChoice{
                                .motor = nullptr, .delay = 0.0, .nozzleExitDiameter = 0.0}),
                            "a scripted motor choice without a motor");
    // Both were scripted all the same: used up here.
    const TestGuiServices services;
    const InnerTube       mount;
    static_cast<void>(prompts.choosePreset(nullptr, services.services(),
                                           {.componentKind   = ComponentKind::BODY_TUBE,
                                            .foreDiameter    = std::nullopt,
                                            .aftDiameter     = std::nullopt,
                                            .flippedNoseCone = false}));
    static_cast<void>(prompts.chooseMotor(nullptr, services.services(), mount,
                                          FlightConfigurationId::defaultValueId()));
    EXPECT_EQ(prompts.unusedAnswers(), 0U);
}

TEST_F(ScriptedUserPromptsTest, RecordsTheDatabaseProblems)
{
    ScriptedUserPrompts                             prompts;
    const std::vector<UserPrompts::DatabaseProblem> problems{
        {.database = UserPrompts::DatabaseKind::THRUST_CURVES,
         .file     = "motors/broken.eng",
         .message  = QStringLiteral("Illegal number")},
        {.database = UserPrompts::DatabaseKind::COMPONENT_PRESETS,
         .file     = "broken.orc",
         .message  = QStringLiteral("Unexpected end of file")}};
    prompts.showDatabaseProblems(nullptr, problems);
    ASSERT_EQ(prompts.prompts().size(), 1U);
    EXPECT_EQ(prompts.prompts().front().kind, Kind::DATABASE_PROBLEMS);
    EXPECT_EQ(
        prompts.prompts().front().texts,
        (QStringList{QStringLiteral("THRUST_CURVES"),
                     QtStrings::fromPath(std::filesystem::path("motors/broken.eng")),
                     QStringLiteral("Illegal number"), QStringLiteral("COMPONENT_PRESETS"),
                     QStringLiteral("broken.orc"), QStringLiteral("Unexpected end of file")}));
    ASSERT_EQ(prompts.databaseProblems().size(), 2U);
    EXPECT_EQ(prompts.databaseProblems().back().file, std::filesystem::path("broken.orc"));
}

TEST_F(ScriptedUserPromptsTest, AnUnscriptedQuestionFailsTheTestAndCancels)
{
    ScriptedUserPrompts            prompts;
    UserPrompts::SaveChangesAnswer answer = UserPrompts::SaveChangesAnswer::SAVE;
    EXPECT_NONFATAL_FAILURE(answer = prompts.askSaveChanges(nullptr, QStringLiteral("Rocket")),
                            "unexpected SAVE_CHANGES question: the script has no answer left");
    EXPECT_EQ(answer, UserPrompts::SaveChangesAnswer::CANCEL);
}

TEST_F(ScriptedUserPromptsTest, AnAnswerOfAnotherKindFailsTheTestAndCancels)
{
    ScriptedUserPrompts prompts;
    prompts.answerYesNo(true);
    bool overwrite = true;
    EXPECT_NONFATAL_FAILURE(
        overwrite = prompts.askOverwrite(nullptr, QStringLiteral("rocket.ork")),
        "a OVERWRITE question, but the script's next answer is to a YES_NO question");
    EXPECT_FALSE(overwrite);
    EXPECT_TRUE(prompts.askYesNo(nullptr, {}));

    // The value dialogs cancel too.
    prompts.answerYesNo(false);
    std::optional<UserPrompts::IgnitionChoice> ignition = UserPrompts::IgnitionChoice{};
    EXPECT_NONFATAL_FAILURE(
        ignition = prompts.askIgnition(nullptr, {.configurationName = QString(), .initial = {}}),
        "a IGNITION question, but the script's next answer is to a YES_NO question");
    EXPECT_FALSE(ignition.has_value());
    EXPECT_FALSE(prompts.askYesNo(nullptr, {}));
}

TEST_F(ScriptedUserPromptsTest, AnUnusedAnswerFailsTheTest)
{
    EXPECT_NONFATAL_FAILURE(
        {
            ScriptedUserPrompts prompts;
            prompts.answerDiscardChanges({.discard = true, .doNotAskAgain = false});
        },
        "the script's answer to a DISCARD_CHANGES question was never asked for");
}

}  // namespace

}  // namespace QtRocket::Gui::Test
