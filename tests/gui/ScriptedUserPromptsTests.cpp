#include "ScriptedUserPrompts.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/Rocket.h"

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
    prompts.editDesignInformation(nullptr, *document);
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
