#include "QtRocket/gui/main/WidgetUserPrompts.h"

#include <filesystem>
#include <memory>
#include <optional>

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/dialogs/DetailDialog.h"
#include "QtRocket/gui/dialogs/InternalErrorDialog.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "RecordingGuardHook.h"

// The dialogs are shown (offscreen) and answered by NextModalDialog from their own event loop.

namespace QtRocket::Gui
{

namespace
{

using WidgetUserPromptsTest = Test::GuiTest;
using SaveChangesAnswer     = UserPrompts::SaveChangesAnswer;

/// What the "Design not saved" question looked like.
struct SaveQuestion
{
    QString                      title;
    QString                      text;
    Qt::TextFormat               format = Qt::AutoText;
    QMessageBox::StandardButtons buttons;
    QMessageBox::StandardButton  defaultButton = QMessageBox::NoButton;
};

/// Records the "Design not saved" question and clicks @p button (Esc for NoButton).
struct AnswerSaveQuestion
{
    QMessageBox::StandardButton button;
    SaveQuestion*               seen;

    void operator()(QWidget& widget) const
    {
        auto* const box = qobject_cast<QMessageBox*>(&widget);
        ASSERT_NE(box, nullptr);
        seen->title   = box->windowTitle();
        seen->text    = box->text();
        seen->format  = box->textFormat();
        seen->buttons = box->standardButtons();
        if (box->defaultButton() != nullptr)
        {
            seen->defaultButton = box->standardButton(box->defaultButton());
        }
        if (button == QMessageBox::NoButton)
        {
            box->reject();
            return;
        }
        box->button(button)->click();
    }
};

/// Records the message lines and the title of the message dialog and closes it.
struct CloseMessage
{
    QStringList* lines;
    QString*     title;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<DetailDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        *lines = dialog->messageLines();
        *title = dialog->windowTitle();
        EXPECT_EQ(dialog->detailsView(), nullptr);
        dialog->buttonBox()->button(QDialogButtonBox::Ok)->click();
    }
};

/// Clicks "Continue" in the internal error dialog.
struct ClickContinue
{
    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<InternalErrorDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        dialog->continueButton()->click();
    }
};

/// The answer to "Design not saved" when the user clicks @p button, and what the question
/// looked like.
[[nodiscard]] SaveChangesAnswer askToSave(QMessageBox::StandardButton button, SaveQuestion& seen)
{
    WidgetUserPrompts                prompts;
    const Gui::Test::NextModalDialog answer(AnswerSaveQuestion{.button = button, .seen = &seen});
    return prompts.askSaveChanges(nullptr, QStringLiteral("Rocket"));
}

TEST_F(WidgetUserPromptsTest, AsksBasicFramesQuestionWhetherToSaveTheDesign)
{
    SaveQuestion seen;
    EXPECT_EQ(askToSave(QMessageBox::Yes, seen), SaveChangesAnswer::SAVE);
    EXPECT_EQ(seen.title, QStringLiteral("Design not saved"));
    EXPECT_EQ(seen.text,
              QStringLiteral("Design 'Rocket' has not been saved.  Do you want to save it?"));
    EXPECT_EQ(seen.format, Qt::PlainText);
    EXPECT_EQ(seen.buttons, QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    EXPECT_EQ(seen.defaultButton, QMessageBox::Yes);
}

TEST_F(WidgetUserPromptsTest, NoDiscardsAndCancelOrEscCancels)
{
    SaveQuestion seen;
    EXPECT_EQ(askToSave(QMessageBox::No, seen), SaveChangesAnswer::DISCARD);
    EXPECT_EQ(askToSave(QMessageBox::Cancel, seen), SaveChangesAnswer::CANCEL);
    EXPECT_EQ(askToSave(QMessageBox::NoButton, seen), SaveChangesAnswer::CANCEL);
}

TEST_F(WidgetUserPromptsTest, TheDesignsNameIsPlainText)
{
    EXPECT_EQ(WidgetUserPrompts::saveChangesText(QStringLiteral("<b>Mars</b>")),
              QStringLiteral("Design '<b>Mars</b>' has not been saved.  Do you want to save it?"));
}

TEST_F(WidgetUserPromptsTest, ShowsAMessageWithItsLines)
{
    WidgetUserPrompts                prompts;
    QStringList                      lines;
    QString                          title;
    const Gui::Test::NextModalDialog close(CloseMessage{.lines = &lines, .title = &title});
    prompts.showMessage(nullptr, {.severity = UserPrompts::Severity::WARNING,
                                  .title    = QStringLiteral("Design not saved"),
                                  .lines    = {QStringLiteral("first"), QStringLiteral("second")}});
    EXPECT_EQ(title, QStringLiteral("Design not saved"));
    EXPECT_EQ(lines, (QStringList{QStringLiteral("first"), QStringLiteral("second")}));
}

TEST_F(WidgetUserPromptsTest, ShowsTheInternalErrorDialog)
{
    WidgetUserPrompts                prompts;
    const Gui::Test::NextModalDialog close{ClickContinue{}};
    Guard::Failure                   failure;
    failure.kind    = Guard::Failure::Kind::EXCEPTION;
    failure.message = QStringLiteral("boom");
    EXPECT_EQ(prompts.showInternalError(nullptr, failure),
              UserPrompts::InternalErrorAnswer::CONTINUE);
}

TEST_F(WidgetUserPromptsTest, TheOtherQuestionsAreNotAvailableYetAndCancel)
{
    const Gui::Test::RecordingGuardHook       hook;
    WidgetUserPrompts                         prompts;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    EXPECT_EQ(
        prompts.askInsertPosition(nullptr, UserPrompts::InsertPositionQuestion::STAGE).position,
        UserPrompts::InsertPosition::CANCEL);
    EXPECT_FALSE(prompts.askDiscardChanges(nullptr, UserPrompts::DiscardQuestion::COMPONENT_CHANGES)
                     .discard);
    EXPECT_FALSE(prompts.askOverwrite(nullptr, QStringLiteral("rocket.ork")));
    EXPECT_EQ(prompts.askStorageOptions(nullptr, true), std::nullopt);
    EXPECT_FALSE(prompts.askYesNo(nullptr, {}));
    EXPECT_EQ(prompts.askDecalFile(nullptr, QString(), std::filesystem::path()), std::nullopt);
    EXPECT_EQ(prompts.askOpenFiles(nullptr, {}), std::nullopt);
    EXPECT_EQ(prompts.askSaveFile(nullptr, {}), std::nullopt);
    prompts.editDesignInformation(nullptr, *document);
    prompts.showWarnings(nullptr, {});
    ASSERT_EQ(hook.failures().size(), 10U);
    EXPECT_EQ(hook.failures().front().kind, Guard::Failure::Kind::ERROR_CONDITION);
    EXPECT_EQ(hook.failures().front().message,
              QStringLiteral("UserPrompts::askInsertPosition() is not available yet: its dialog "
                             "comes later in Milestone 2"));
    EXPECT_TRUE(hook.failures().back().message.contains(QStringLiteral("showWarnings")));
}

}  // namespace

}  // namespace QtRocket::Gui
