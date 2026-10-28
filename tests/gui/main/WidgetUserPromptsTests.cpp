#include "QtRocket/gui/main/WidgetUserPrompts.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <system_error>
#include <utility>
#include <vector>

#include <QAbstractButton>
#include <QChar>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QVariant>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/configdialog/RocketConfig.h"
#include "QtRocket/gui/dialogs/DecalNotFoundDialog.h"
#include "QtRocket/gui/dialogs/DeploymentSelectionDialog.h"
#include "QtRocket/gui/dialogs/DetailDialog.h"
#include "QtRocket/gui/dialogs/ErrorWarningDialog.h"
#include "QtRocket/gui/dialogs/IgnitionSelectionDialog.h"
#include "QtRocket/gui/dialogs/InternalErrorDialog.h"
#include "QtRocket/gui/dialogs/MessageList.h"
#include "QtRocket/gui/dialogs/OptionDialog.h"
#include "QtRocket/gui/dialogs/RenameConfigDialog.h"
#include "QtRocket/gui/dialogs/SaveDesignInfoDialog.h"
#include "QtRocket/gui/dialogs/SeparationSelectionDialog.h"
#include "QtRocket/gui/dialogs/StorageOptionsDialog.h"
#include "QtRocket/gui/dialogs/WarningDialog.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "ScriptedUserPrompts.h"
#include "TestTempDir.h"
#include "configdialog/ConfigDialogTestSupport.h"

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
    bool                         qtDialog      = false;
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
        seen->title    = box->windowTitle();
        seen->text     = box->text();
        seen->format   = box->textFormat();
        seen->buttons  = box->standardButtons();
        seen->qtDialog = box->testOption(QMessageBox::Option::DontUseNativeDialog);
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
#ifdef __APPLE__
    // A QMessageBox drops its title on macOS (Apple's guidelines: QMessageBox::setWindowTitle()).
    EXPECT_TRUE(seen.title.isEmpty());
#else
    EXPECT_EQ(seen.title, QStringLiteral("Design not saved"));
#endif
    EXPECT_EQ(seen.text,
              QStringLiteral("Design 'Rocket' has not been saved.  Do you want to save it?"));
    EXPECT_EQ(seen.format, Qt::PlainText);
    EXPECT_EQ(seen.buttons, QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    EXPECT_EQ(seen.defaultButton, QMessageBox::Yes);
    // Never a platform theme's native message box, which would show the name as rich text.
    EXPECT_TRUE(seen.qtDialog);
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

/// What the "Save Design Info" dialog looked like when it was shown.
struct DesignInformationSeen
{
    QString        title;
    const QWidget* parent = nullptr;
    bool           modal  = false;
    QString        designName;
};

/// Records the "Save Design Info" dialog, types the designer "Ann" and clicks OK (@p cancel:
/// Cancel).
struct AnswerDesignInformation
{
    bool                   cancel;
    DesignInformationSeen* seen;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<SaveDesignInfoDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        seen->title      = dialog->windowTitle();
        seen->parent     = dialog->parentWidget();
        seen->modal      = dialog->isModal();
        seen->designName = dialog->panel().nameField().text();
        QTest::keyClicks(&dialog->panel().designerArea(), QStringLiteral("Ann"));
        if (cancel)
        {
            dialog->cancelButton().click();
        }
        else
        {
            dialog->okButton().click();
        }
    }
};

TEST_F(WidgetUserPromptsTest, TheDesignInformationIsTheSaveDesignInfoDialog)
{
    // BasicFrame.showSaveRocketInfoDialog(): SaveDesignInfoPanel in a modal dialog, editing the
    // rocket in place; the design name shown (the user's decision R3-1).
    const Gui::Test::ConfigDialogServices     services;
    WidgetUserPrompts                         prompts;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    document->getRocket().setName("Mars");
    QWidget                          parent;
    DesignInformationSeen            seen;
    const Gui::Test::NextModalDialog answer{
        AnswerDesignInformation{.cancel = false, .seen = &seen}};
    prompts.editDesignInformation(&parent, services.services(), *document);
    EXPECT_TRUE(answer.acted());
    EXPECT_EQ(seen.title, QStringLiteral("Save Design Info"));
    EXPECT_EQ(seen.parent, &parent);
    EXPECT_TRUE(seen.modal);
    EXPECT_EQ(seen.designName, QStringLiteral("Mars"));
    EXPECT_EQ(document->getRocket().getDesigner(), "Ann");
    EXPECT_TRUE(services.prompts().prompts().empty());
}

TEST_F(WidgetUserPromptsTest, ACancelledDesignInformationKeepsItsEdits)
{
    // The user's decision R3-3: the Save As that asks goes on, and the edits stay (Java's Cancel
    // undoes nothing).
    Gui::Test::ConfigDialogServices           services;
    WidgetUserPrompts                         prompts;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    services.prompts().answerDiscardChanges({.discard = true, .doNotAskAgain = false});
    DesignInformationSeen            seen;
    const Gui::Test::NextModalDialog answer{AnswerDesignInformation{.cancel = true, .seen = &seen}};
    prompts.editDesignInformation(nullptr, services.services(), *document);
    EXPECT_TRUE(answer.acted());
    EXPECT_EQ(seen.parent, nullptr);
    EXPECT_EQ(document->getRocket().getDesigner(), "Ann");
    ASSERT_EQ(services.prompts().prompts().size(), 1U);
    EXPECT_EQ(services.prompts().prompts().front().kind,
              Gui::Test::ScriptedUserPrompts::Kind::DISCARD_CHANGES);
}

/// What an OptionDialog question looked like.
struct OptionQuestion
{
    QString     objectName;
    QString     title;
    QStringList lines;
    QStringList options;
    QString     initial;
    bool        icon = false;
    QString     check;
    QString     note;
};

/// Records the OptionDialog question, ticks its check box when @p tick, and
/// answers with the standard option @p standard, or else the option whose text
/// is @p text, or else Esc.
struct AnswerOption
{
    QDialogButtonBox::StandardButton standard = QDialogButtonBox::NoButton;
    QString                          text;
    bool                             tick = false;
    OptionQuestion*                  seen = nullptr;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<OptionDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        if (seen != nullptr)
        {
            record(*dialog);
        }
        if (tick)
        {
            auto* const box = dialog->findChild<QCheckBox*>();
            ASSERT_NE(box, nullptr);
            box->click();
        }
        if (standard != QDialogButtonBox::NoButton)
        {
            dialog->buttonBox()->button(standard)->click();
            return;
        }
        for (QAbstractButton* const option : dialog->buttonBox()->buttons())
        {
            if (!text.isEmpty() && option->text() == text)
            {
                option->click();
                return;
            }
        }
        QTest::keyClick(dialog, Qt::Key_Escape);
    }

    void record(const OptionDialog& dialog) const
    {
        seen->objectName = dialog.objectName();
        seen->title      = dialog.windowTitle();
        seen->lines      = dialog.messageLines();
        seen->icon       = !dialog.iconLabel()->pixmap().isNull();
        for (const QAbstractButton* const option : dialog.buttonBox()->buttons())
        {
            seen->options.append(option->text());
            if (const auto* const push = qobject_cast<const QPushButton*>(option);
                push != nullptr && push->isDefault())
            {
                seen->initial = option->text();
            }
        }
        seen->options.sort();
        if (const auto* const box = dialog.findChild<QCheckBox*>(); box != nullptr)
        {
            seen->check = box->text();
        }
        if (const auto* const note = dialog.findChild<StyledLabel*>(QStringLiteral("note"));
            note != nullptr)
        {
            seen->note = note->text();
        }
    }
};

/// The text of the standard option @p button.
[[nodiscard]] QString optionText(QDialogButtonBox::StandardButton button)
{
    const QDialogButtonBox box(button);
    return box.button(button)->text();
}

/// The texts of the standard options @p first and @p second, sorted.
[[nodiscard]] QStringList optionTexts(QDialogButtonBox::StandardButton first,
                                      QDialogButtonBox::StandardButton second)
{
    QStringList texts{optionText(first), optionText(second)};
    texts.sort();
    return texts;
}

TEST_F(WidgetUserPromptsTest, AsksForTheInsertPositionOfAComponent)
{
    WidgetUserPrompts                 prompts;
    OptionQuestion                    seen;
    UserPrompts::InsertPositionAnswer answer;
    {
        const Gui::Test::NextModalDialog choose(
            AnswerOption{.standard = QDialogButtonBox::NoButton,
                         .text     = QStringLiteral("Add to the end"),
                         .tick     = true,
                         .seen     = &seen});
        answer =
            prompts.askInsertPosition(nullptr, UserPrompts::InsertPositionQuestion::BODY_COMPONENT);
    }
    EXPECT_EQ(answer.position, UserPrompts::InsertPosition::ADD_TO_END);
    EXPECT_TRUE(answer.doNotAskAgain);
    // ComponentAddButtons.askPosition(): QUESTION_MESSAGE, the options with
    // "Insert here" first.
    EXPECT_EQ(seen.objectName, QStringLiteral("insertPositionQuestion"));
    EXPECT_EQ(seen.title, QStringLiteral("Select component position"));
    EXPECT_EQ(seen.lines, QStringList{QStringLiteral(
                              "Insert the component after the current component or as the last "
                              "component?")});
    EXPECT_EQ(seen.options, (QStringList{QStringLiteral("Add to the end"), QStringLiteral("Cancel"),
                                         QStringLiteral("Insert here")}));
    EXPECT_EQ(seen.initial, QStringLiteral("Insert here"));
    EXPECT_TRUE(seen.icon);
    EXPECT_EQ(seen.check, QStringLiteral("Do not ask me again"));
    EXPECT_EQ(seen.note,
              QStringLiteral("You can change the default operation in the preferences."));
}

TEST_F(WidgetUserPromptsTest, AsksForTheInsertPositionOfAStage)
{
    WidgetUserPrompts prompts;
    OptionQuestion    seen;
    {
        const Gui::Test::NextModalDialog choose(AnswerOption{.standard = QDialogButtonBox::NoButton,
                                                             .text = QStringLiteral("Insert here"),
                                                             .tick = false,
                                                             .seen = &seen});
        const UserPrompts::InsertPositionAnswer answer =
            prompts.askInsertPosition(nullptr, UserPrompts::InsertPositionQuestion::STAGE);
        EXPECT_EQ(answer.position, UserPrompts::InsertPosition::INSERT_HERE);
        EXPECT_FALSE(answer.doNotAskAgain);
    }
    EXPECT_EQ(seen.title, QStringLiteral("Select stage position"));
    EXPECT_EQ(seen.lines, QStringList{QStringLiteral(
                              "Insert the stage after the current stage or as the last stage?")});
    {
        const Gui::Test::NextModalDialog cancel(AnswerOption{.standard = QDialogButtonBox::NoButton,
                                                             .text     = QStringLiteral("Cancel"),
                                                             .tick     = false,
                                                             .seen     = nullptr});
        EXPECT_EQ(
            prompts.askInsertPosition(nullptr, UserPrompts::InsertPositionQuestion::STAGE).position,
            UserPrompts::InsertPosition::CANCEL);
    }
    // Closing the dialog cancels (JOptionPane.CLOSED_OPTION); the box is still
    // told.
    const Gui::Test::NextModalDialog        close(AnswerOption{
        .standard = QDialogButtonBox::NoButton, .text = QString(), .tick = true, .seen = nullptr});
    const UserPrompts::InsertPositionAnswer closed =
        prompts.askInsertPosition(nullptr, UserPrompts::InsertPositionQuestion::STAGE);
    EXPECT_EQ(closed.position, UserPrompts::InsertPosition::CANCEL);
    EXPECT_TRUE(closed.doNotAskAgain);
}

/// The question whether to discard @p question's changes, answered by @p
/// answer.
[[nodiscard]] UserPrompts::DiscardAnswer askToDiscard(UserPrompts::DiscardQuestion question,
                                                      const AnswerOption&          answer)
{
    WidgetUserPrompts                prompts;
    const Gui::Test::NextModalDialog respond(answer);
    return prompts.askDiscardChanges(nullptr, question);
}

TEST_F(WidgetUserPromptsTest, AsksWhetherToDiscardTheChangesToAComponent)
{
    OptionQuestion                   seen;
    const UserPrompts::DiscardAnswer yes = askToDiscard(
        UserPrompts::DiscardQuestion::COMPONENT_CHANGES,
        AnswerOption{
            .standard = QDialogButtonBox::Yes, .text = QString(), .tick = false, .seen = &seen});
    EXPECT_TRUE(yes.discard);
    EXPECT_FALSE(yes.doNotAskAgain);
    // RocketComponentConfig: JOptionPane.showConfirmDialog(YES_NO_OPTION,
    // WARNING_MESSAGE).
    EXPECT_EQ(seen.objectName, QStringLiteral("discardChangesQuestion"));
    EXPECT_EQ(seen.title, QStringLiteral("Cancel operation"));
    EXPECT_EQ(seen.lines, QStringList{QStringLiteral(
                              "<html>Are you sure you want to <b>discard</b> your <b>changes</b> "
                              "to this component?</html>")});
    EXPECT_EQ(seen.options, optionTexts(QDialogButtonBox::Yes, QDialogButtonBox::No));
    EXPECT_EQ(seen.initial, optionText(QDialogButtonBox::Yes));
    EXPECT_EQ(seen.check, QStringLiteral("Don't ask me again"));
    EXPECT_TRUE(seen.icon);

    // The box is told whatever the answer (OpenRocket stores it as soon as it is
    // ticked).
    const UserPrompts::DiscardAnswer no = askToDiscard(
        UserPrompts::DiscardQuestion::COMPONENT_CHANGES,
        AnswerOption{
            .standard = QDialogButtonBox::No, .text = QString(), .tick = true, .seen = nullptr});
    EXPECT_FALSE(no.discard);
    EXPECT_TRUE(no.doNotAskAgain);
    const UserPrompts::DiscardAnswer closed =
        askToDiscard(UserPrompts::DiscardQuestion::COMPONENT_CHANGES, AnswerOption{});
    EXPECT_FALSE(closed.discard);
    EXPECT_FALSE(closed.doNotAskAgain);
}

TEST_F(WidgetUserPromptsTest, AsksWhetherToUndoAddingAComponentOrToDiscardThePreferences)
{
    OptionQuestion added;
    EXPECT_TRUE(askToDiscard(UserPrompts::DiscardQuestion::COMPONENT_ADDITION,
                             AnswerOption{.standard = QDialogButtonBox::Yes,
                                          .text     = QString(),
                                          .tick     = false,
                                          .seen     = &added})
                    .discard);
    EXPECT_EQ(added.title, QStringLiteral("Cancel operation"));
    EXPECT_EQ(added.lines, QStringList{QStringLiteral("<html>Are you sure you want to <b>undo "
                                                      "adding</b> this component?</html>")});

    OptionQuestion preferences;
    EXPECT_TRUE(askToDiscard(UserPrompts::DiscardQuestion::PREFERENCE_CHANGES,
                             AnswerOption{.standard = QDialogButtonBox::Yes,
                                          .text     = QString(),
                                          .tick     = false,
                                          .seen     = &preferences})
                    .discard);
    EXPECT_EQ(preferences.title, QStringLiteral("Discard Preference Changes"));
    EXPECT_EQ(preferences.lines,
              QStringList{QStringLiteral("<html>Are you sure you want to <b>discard the "
                                         "preference changes</b>?</html>")});
    EXPECT_EQ(preferences.check, QStringLiteral("Don't ask me again"));
}

TEST_F(WidgetUserPromptsTest, AsksWhetherToOverwriteAFile)
{
    // FileHelper.confirmWrite() (the Java probe FileOperationsProbe: EXISTS).
    EXPECT_EQ(WidgetUserPrompts::overwriteText(QStringLiteral("rocket.ork")),
              QStringLiteral("File 'rocket.ork' exists.  Do you want to overwrite it?"));
    EXPECT_EQ(WidgetUserPrompts::overwriteText(QStringLiteral("{filename}.ork")),
              QStringLiteral("File '{filename}.ork' exists.  Do you want to overwrite it?"));
    // Deviations: Java's L10N.replace() throws for these names, or drops the
    // backslash.
    EXPECT_EQ(WidgetUserPrompts::overwriteText(QStringLiteral("a$b.ork")),
              QStringLiteral("File 'a$b.ork' exists.  Do you want to overwrite it?"));
    EXPECT_EQ(WidgetUserPrompts::overwriteText(QStringLiteral("a\\b.ork")),
              QStringLiteral("File 'a\\b.ork' exists.  Do you want to overwrite it?"));

    WidgetUserPrompts prompts;
    OptionQuestion    seen;
    {
        const Gui::Test::NextModalDialog yes(AnswerOption{
            .standard = QDialogButtonBox::Yes, .text = QString(), .tick = false, .seen = &seen});
        EXPECT_TRUE(prompts.askOverwrite(nullptr, QStringLiteral("rocket.ork")));
    }
    EXPECT_EQ(seen.objectName, QStringLiteral("overwriteQuestion"));
    EXPECT_EQ(seen.title, QStringLiteral("File exists"));
    EXPECT_EQ(
        seen.lines,
        QStringList{QStringLiteral("File 'rocket.ork' exists.  Do you want to overwrite it?")});
    EXPECT_EQ(seen.options, optionTexts(QDialogButtonBox::Yes, QDialogButtonBox::No));
    EXPECT_TRUE(seen.icon);
    {
        const Gui::Test::NextModalDialog no(AnswerOption{
            .standard = QDialogButtonBox::No, .text = QString(), .tick = false, .seen = nullptr});
        EXPECT_FALSE(prompts.askOverwrite(nullptr, QStringLiteral("rocket.ork")));
    }
    const Gui::Test::NextModalDialog close(AnswerOption{});
    EXPECT_FALSE(prompts.askOverwrite(nullptr, QStringLiteral("rocket.ork")));
}

TEST_F(WidgetUserPromptsTest, AsksAYesNoQuestionWithTheCallersTexts)
{
    WidgetUserPrompts prompts;
    OptionQuestion    seen;
    {
        const Gui::Test::NextModalDialog yes(AnswerOption{
            .standard = QDialogButtonBox::Yes, .text = QString(), .tick = false, .seen = &seen});
        EXPECT_TRUE(prompts.askYesNo(
            nullptr,
            {.severity = UserPrompts::Severity::WARNING,
             .title    = QStringLiteral("Reset preferences?"),
             .text     = QStringLiteral("Are you sure you want to reset all your preferences?")}));
    }
    EXPECT_EQ(seen.objectName, QStringLiteral("yesNoQuestion"));
    EXPECT_EQ(seen.title, QStringLiteral("Reset preferences?"));
    EXPECT_EQ(seen.lines,
              QStringList{QStringLiteral("Are you sure you want to reset all your preferences?")});
    EXPECT_EQ(seen.options, optionTexts(QDialogButtonBox::Yes, QDialogButtonBox::No));
    const Gui::Test::NextModalDialog no(AnswerOption{
        .standard = QDialogButtonBox::No, .text = QString(), .tick = false, .seen = nullptr});
    EXPECT_FALSE(prompts.askYesNo(nullptr, {.severity = UserPrompts::Severity::INFORMATION,
                                            .title    = QStringLiteral("Title"),
                                            .text     = QStringLiteral("Text")}));
}

/// Chooses "Only summary data" in the storage options and OK, or Cancel when @p
/// cancel.
struct ChooseStorage
{
    bool cancel = false;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<StorageOptionsDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        dialog->summaryButton().click();
        (cancel ? dialog->cancelButton() : dialog->okButton())->click();
    }
};

TEST_F(WidgetUserPromptsTest, AsksForTheStorageOptions)
{
    WidgetUserPrompts prompts;
    {
        const Gui::Test::NextModalDialog choose{ChooseStorage{}};
        EXPECT_EQ(prompts.askStorageOptions(nullptr, true), std::optional<bool>(false));
    }
    const Gui::Test::NextModalDialog cancel(ChooseStorage{.cancel = true});
    EXPECT_EQ(prompts.askStorageOptions(nullptr, true), std::nullopt);
}

/// Whether the save dialog is Qt's own: on macOS, whose save panel always confirms an overwrite.
#ifdef __APPLE__
constexpr bool kQtSaveDialog = true;
#else
constexpr bool kQtSaveDialog = false;
#endif

/// What a file dialog looked like.
struct FileDialogSeen
{
    QString                 objectName;
    QString                 title;
    QStringList             nameFilters;
    QString                 selectedFilter;
    QString                 directory;
    QStringList             proposed;
    QFileDialog::FileMode   fileMode          = QFileDialog::AnyFile;
    QFileDialog::AcceptMode acceptMode        = QFileDialog::AcceptOpen;
    bool                    confirmsOverwrite = true;
    bool                    qtDialog          = false;
    QString                 description;
    QString                 acceptLabel;
};

/// Records the modal file dialog and chooses @p file in it, or cancels it for
/// an empty one.
struct ChooseFile
{
    QString         file;
    FileDialogSeen* seen = nullptr;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<QFileDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        if (seen != nullptr)
        {
            record(*dialog);
        }
        if (file.isEmpty())
        {
            dialog->reject();
            return;
        }
        dialog->selectFile(file);
        // QFileDialog::accept() is protected; QDialog's is its public face.
        QDialog& asDialog = *dialog;
        asDialog.accept();
    }

    void record(const QFileDialog& dialog) const
    {
        seen->objectName        = dialog.objectName();
        seen->title             = dialog.windowTitle();
        seen->nameFilters       = dialog.nameFilters();
        seen->selectedFilter    = dialog.selectedNameFilter();
        seen->directory         = dialog.directory().absolutePath();
        seen->proposed          = dialog.selectedFiles();
        seen->fileMode          = dialog.fileMode();
        seen->acceptMode        = dialog.acceptMode();
        seen->confirmsOverwrite = !dialog.testOption(QFileDialog::DontConfirmOverwrite);
        seen->qtDialog          = dialog.testOption(QFileDialog::DontUseNativeDialog);
        seen->acceptLabel       = dialog.labelText(QFileDialog::Accept);
        if (const auto* const area = dialog.findChild<DescriptionArea*>(); area != nullptr)
        {
            seen->description = area->description();
        }
    }
};

/// Records the modal file dialog as it opened, chooses the name filter @p filter in it and then
/// @p file (ChooseFile).
struct ChooseFileAs
{
    QString         file;
    QString         filter;
    FileDialogSeen* seen = nullptr;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<QFileDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        if (seen != nullptr)
        {
            ChooseFile{.file = file, .seen = seen}.record(*dialog);
        }
        dialog->selectNameFilter(filter);
        ChooseFile{.file = file, .seen = nullptr}(widget);
    }
};

/// The first action, then arms @p next for the dialog that follows (a question
/// of two dialogs).
struct ThenNext
{
    Gui::Test::NextModalDialog::Action         first;
    Gui::Test::NextModalDialog::Action         second;
    std::optional<Gui::Test::NextModalDialog>* next;

    void operator()(QWidget& widget) const
    {
        next->emplace(second);
        first(widget);
    }
};

/// The state Qt's file dialog keeps (QSettings(UserScope, "QtProject"), group "FileDialog": its
/// history, last directory, view), put back as it was when this goes. The dialog writes it in the
/// native format, the registry on Windows and CFPreferences on macOS, which the test main's
/// QSettings::setPath() does not redirect; a test leaves the user's state as it found it. (ctest
/// runs the tests in parallel processes, so a test that starts after another test's dialog wrote
/// its state may put that state back instead of the user's.)
class QtFileDialogState
{
public:
    QtFileDialogState()
    {
        QSettings settings(QSettings::UserScope, organization());
        settings.beginGroup(group());
        for (const QString& key : settings.allKeys())
        {
            m_values.emplace_back(key, settings.value(key));
        }
    }

    ~QtFileDialogState()
    {
        QSettings settings(QSettings::UserScope, organization());
        settings.remove(group());
        settings.beginGroup(group());
        for (const auto& [key, value] : m_values)
        {
            settings.setValue(key, value);
        }
    }

    QtFileDialogState(const QtFileDialogState&)            = delete;
    QtFileDialogState& operator=(const QtFileDialogState&) = delete;
    QtFileDialogState(QtFileDialogState&&)                 = delete;
    QtFileDialogState& operator=(QtFileDialogState&&)      = delete;

private:
    [[nodiscard]] static QString organization() { return QStringLiteral("QtProject"); }
    [[nodiscard]] static QString group() { return QStringLiteral("FileDialog"); }

    std::vector<std::pair<QString, QVariant>> m_values;
};

/// A directory of the test with the files a.ork and b.png; Qt's file dialog state is put back
/// afterwards.
// Whether this build runs under ThreadSanitizer (GCC defines the macro, Clang has the feature).
#ifdef __SANITIZE_THREAD__
constexpr bool kThreadSanitizer = true;
#else
#ifdef __has_feature
#if __has_feature(thread_sanitizer)
constexpr bool kThreadSanitizer = true;
#else
constexpr bool kThreadSanitizer = false;
#endif
#else
constexpr bool kThreadSanitizer = false;
#endif
#endif

class WidgetFileDialogTest : public Test::GuiTest
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        // A QFileDialog starts Qt's QFileInfoGatherer thread. Under ThreadSanitizer the
        // uninstrumented libQt6Core then shows races on implicitly shared QStrings between that
        // thread and the main thread, and TSan's own thread registry fails a CHECK: false
        // positives of Qt's code, not of QtRocket's (O7). These tests run in every other preset.
        if constexpr (kThreadSanitizer)
        {
            GTEST_SKIP() << "ThreadSanitizer: Qt's QFileInfoGatherer thread (uninstrumented Qt)";
        }
    }

    WidgetFileDialogTest()
    {
        static_cast<void>(m_directory.write("a.ork", "design"));
        static_cast<void>(m_directory.write("b.png", "image"));
    }

    [[nodiscard]] const QtRocket::Test::TempDir& directory() const noexcept { return m_directory; }
    [[nodiscard]] QString                        path(const char* name) const
    {
        return QtStrings::fromPath(m_directory.resolve(name));
    }

private:
    /// Declared first: it goes last, after the dialogs (GuiTest::TearDown() deletes those given
    /// to deleteLater()).
    QtFileDialogState       m_fileDialogState;
    QtRocket::Test::TempDir m_directory;
};

/// Whether @p first and @p second are the same file or directory (by another path too: macOS's
/// temporary directory is behind a link); false when one does not exist.
[[nodiscard]] bool isSamePath(const std::filesystem::path& first,
                              const std::filesystem::path& second)
{
    std::error_code error;
    return std::filesystem::equivalent(first, second, error);
}

/// Whether @p files is the one file @p expected.
[[nodiscard]] bool isFile(const std::vector<std::filesystem::path>& files,
                          const std::filesystem::path&              expected)
{
    return files.size() == 1 && isSamePath(files.front(), expected);
}

/// Whether @p directory is @p expected.
[[nodiscard]] bool isDirectory(const QString& directory, const std::filesystem::path& expected)
{
    return isSamePath(QtStrings::toPath(directory), expected);
}

/// The files of @p answer, none without an answer.
[[nodiscard]] std::vector<std::filesystem::path> filesOf(
    const std::optional<UserPrompts::OpenFileAnswer>& answer)
{
    return answer.has_value() ? answer->files : std::vector<std::filesystem::path>();
}

/// The directory of @p answer, empty without an answer.
[[nodiscard]] std::filesystem::path openedDirectoryOf(
    const std::optional<UserPrompts::OpenFileAnswer>& answer)
{
    return answer.has_value() ? answer->directory : std::filesystem::path();
}

/// The file of @p answer, empty without an answer.
[[nodiscard]] std::filesystem::path savedFileOf(
    const std::optional<UserPrompts::SaveFileAnswer>& answer)
{
    return answer.has_value() ? answer->file : std::filesystem::path();
}

/// The directory of @p answer, empty without an answer.
[[nodiscard]] std::filesystem::path savedDirectoryOf(
    const std::optional<UserPrompts::SaveFileAnswer>& answer)
{
    return answer.has_value() ? answer->directory : std::filesystem::path();
}

/// The name filter of @p answer, empty without an answer.
[[nodiscard]] QString nameFilterOf(const std::optional<UserPrompts::SaveFileAnswer>& answer)
{
    return answer.has_value() ? answer->selectedNameFilter : QString();
}

/// The storage option of @p answer, nullopt without an answer.
[[nodiscard]] std::optional<bool> storageOf(
    const std::optional<UserPrompts::SaveFileAnswer>& answer)
{
    return answer.has_value() ? answer->saveSimulationData : std::nullopt;
}

TEST_F(WidgetFileDialogTest, AsksForTheFilesToOpen)
{
    WidgetUserPrompts                          prompts;
    FileDialogSeen                             seen;
    std::optional<UserPrompts::OpenFileAnswer> answer;
    {
        const Gui::Test::NextModalDialog choose(ChooseFile{.file = path("a.ork"), .seen = &seen});
        answer = prompts.askOpenFiles(
            nullptr, {.title              = QString(),
                      .nameFilters        = {QStringLiteral("All Files (*)"),
                                             QStringLiteral("OpenRocket designs (*.ork *.ork.gz)")},
                      .directory          = directory().path(),
                      .multipleFiles      = true,
                      .description        = QString(),
                      .selectedNameFilter = QStringLiteral("OpenRocket designs (*.ork *.ork.gz)"),
                      .selection          = UserPrompts::OpenFileSelection::FILES,
                      .approveButtonText  = QString()});
    }
    ASSERT_TRUE(answer.has_value());
    EXPECT_TRUE(isFile(filesOf(answer), directory().resolve("a.ork")));
    EXPECT_TRUE(isSamePath(openedDirectoryOf(answer), directory().path()));
    // With the platform's separators, as Java's File (Qt names files with '/' on Windows).
    EXPECT_EQ(QtStrings::fromPath(filesOf(answer).front()),
              QDir::toNativeSeparators(QtStrings::fromPath(filesOf(answer).front())));
    EXPECT_EQ(QtStrings::fromPath(openedDirectoryOf(answer)),
              QDir::toNativeSeparators(QtStrings::fromPath(openedDirectoryOf(answer))));
    EXPECT_EQ(seen.objectName, QStringLiteral("openFileDialog"));
    EXPECT_EQ(seen.nameFilters,
              (QStringList{QStringLiteral("All Files (*)"),
                           QStringLiteral("OpenRocket designs (*.ork *.ork.gz)")}));
    EXPECT_EQ(seen.selectedFilter, QStringLiteral("OpenRocket designs (*.ork *.ork.gz)"));
    EXPECT_TRUE(isDirectory(seen.directory, directory().path()));
    EXPECT_EQ(seen.fileMode, QFileDialog::ExistingFiles);
    EXPECT_EQ(seen.acceptMode, QFileDialog::AcceptOpen);
    EXPECT_TRUE(seen.description.isEmpty());

    // Without a filter asked for, the first is chosen; the accepting button named as asked
    // (JFileChooser.showDialog(parent, "Add")).
    {
        const Gui::Test::NextModalDialog cancel{ChooseFile{.file = QString(), .seen = &seen}};
        EXPECT_EQ(prompts.askOpenFiles(nullptr, {.title       = QStringLiteral("Open"),
                                                 .nameFilters = {QStringLiteral("All Files (*)"),
                                                                 QStringLiteral("Images (*.png)")},
                                                 .directory   = directory().path(),
                                                 .multipleFiles      = false,
                                                 .description        = QString(),
                                                 .selectedNameFilter = QString(),
                                                 .selection = UserPrompts::OpenFileSelection::FILES,
                                                 .approveButtonText = QStringLiteral("Add")}),
                  std::nullopt);
    }
    EXPECT_EQ(seen.selectedFilter, QStringLiteral("All Files (*)"));
    EXPECT_EQ(seen.acceptLabel, QStringLiteral("Add"));
}

TEST_F(WidgetFileDialogTest, ADescriptionStandsBesideTheFiles)
{
    // FreeformFinSetConfig's image import: the JFileChooser accessory, in Qt's
    // own dialog.
    WidgetUserPrompts                prompts;
    FileDialogSeen                   seen;
    const Gui::Test::NextModalDialog choose(ChooseFile{.file = path("b.png"), .seen = &seen});
    const std::optional<UserPrompts::OpenFileAnswer> answer =
        prompts.askOpenFiles(nullptr, {.title         = QStringLiteral("Import"),
                                       .nameFilters   = {},
                                       .directory     = directory().path(),
                                       .multipleFiles = false,
                                       .description   = QStringLiteral("The image will be ..."),
                                       .selectedNameFilter = QString(),
                                       .selection          = UserPrompts::OpenFileSelection::FILES,
                                       .approveButtonText  = QString()});
    ASSERT_TRUE(answer.has_value());
    EXPECT_TRUE(isFile(filesOf(answer), directory().resolve("b.png")));
    EXPECT_EQ(seen.title, QStringLiteral("Import"));
    EXPECT_EQ(seen.fileMode, QFileDialog::ExistingFile);
    EXPECT_TRUE(seen.qtDialog);
    EXPECT_EQ(seen.description, QStringLiteral("The image will be ..."));
}

TEST_F(WidgetFileDialogTest, AsksForTheFileToSaveWithoutConfirmingAnOverwrite)
{
    WidgetUserPrompts                          prompts;
    FileDialogSeen                             seen;
    std::optional<UserPrompts::SaveFileAnswer> answer;
    {
        const Gui::Test::NextModalDialog choose(ChooseFile{.file = path("a.ork"), .seen = &seen});
        answer = prompts.askSaveFile(
            nullptr, {.title              = QStringLiteral("Save as OpenRocket ork file"),
                      .nameFilters        = {QStringLiteral("OpenRocket designs (*.ork *.ork.gz)")},
                      .directory          = directory().path(),
                      .fileName           = QStringLiteral("rocket.ork"),
                      .saveSimulationData = std::nullopt,
                      .refuseIllegalNames = true});
    }
    ASSERT_TRUE(answer.has_value());
    EXPECT_TRUE(isSamePath(savedFileOf(answer), directory().resolve("a.ork")));
    EXPECT_TRUE(isSamePath(savedDirectoryOf(answer), directory().path()));
    EXPECT_EQ(storageOf(answer), std::nullopt);
    EXPECT_EQ(seen.objectName, QStringLiteral("saveFileDialog"));
    EXPECT_EQ(seen.title, QStringLiteral("Save as OpenRocket ork file"));
    EXPECT_EQ(seen.acceptMode, QFileDialog::AcceptSave);
    EXPECT_EQ(seen.fileMode, QFileDialog::AnyFile);
    EXPECT_FALSE(seen.confirmsOverwrite);
    // macOS's save panel would confirm an overwrite all the same: Qt's dialog there.
    EXPECT_EQ(seen.qtDialog, kQtSaveDialog);
    // The proposed name in the directory.
    ASSERT_EQ(seen.proposed.size(), 1);
    EXPECT_EQ(QtStrings::toPath(seen.proposed.front()).filename(), "rocket.ork");
    EXPECT_TRUE(std::filesystem::equivalent(QtStrings::toPath(seen.proposed.front()).parent_path(),
                                            directory().path()));
}

TEST_F(WidgetFileDialogTest, ASaveThatOffersTheStorageOptionAsksForThemAfterTheFile)
{
    WidgetUserPrompts                          prompts;
    std::optional<Gui::Test::NextModalDialog>  storage;
    std::optional<UserPrompts::SaveFileAnswer> answer;
    const UserPrompts::SaveFileRequest         request{.title              = QString(),
                                                       .nameFilters        = {},
                                                       .directory          = directory().path(),
                                                       .fileName           = QString(),
                                                       .saveSimulationData = true,
                                                       .refuseIllegalNames = false};
    {
        const Gui::Test::NextModalDialog choose(
            ThenNext{.first  = ChooseFile{.file = path("a.ork"), .seen = nullptr},
                     .second = ChooseStorage{},
                     .next   = &storage});
        answer = prompts.askSaveFile(nullptr, request);
    }
    ASSERT_TRUE(answer.has_value());
    EXPECT_EQ(storageOf(answer), std::optional<bool>(false));
    storage.reset();

    // Cancel in the storage options cancels the save.
    {
        const Gui::Test::NextModalDialog choose(
            ThenNext{.first  = ChooseFile{.file = path("a.ork"), .seen = nullptr},
                     .second = ChooseStorage{.cancel = true},
                     .next   = &storage});
        EXPECT_EQ(prompts.askSaveFile(nullptr, request), std::nullopt);
    }
    storage.reset();
}

TEST_F(WidgetFileDialogTest, ASaveThatRefusesIllegalNamesWarnsAndAsksAgain)
{
    // SaveFileChooser.approveSelection(): a name with one of FileUtils.ILLEGAL_CHARS is refused
    // with "Illegal filename" and the chooser stays: here it is shown again, in its directory
    // with the name (the probe: "x|y.ork" refused, warned of "x|y.ork" and '|'; "ab.ork" taken).
    WidgetUserPrompts                          prompts;
    std::optional<Gui::Test::NextModalDialog>  warning;
    std::optional<Gui::Test::NextModalDialog>  again;
    QStringList                                lines;
    QString                                    title;
    FileDialogSeen                             seen;
    std::optional<UserPrompts::SaveFileAnswer> answer;
    const UserPrompts::SaveFileRequest         request{.title              = QString(),
                                                       .nameFilters        = {},
                                                       .directory          = directory().path(),
                                                       .fileName           = QString(),
                                                       .saveSimulationData = std::nullopt,
                                                       .refuseIllegalNames = true};
    {
        const Gui::Test::NextModalDialog choose(
            ThenNext{.first  = ChooseFile{.file = path("x|y.ork"), .seen = nullptr},
                     .second = ThenNext{.first  = CloseMessage{.lines = &lines, .title = &title},
                                        .second = ChooseFile{.file = path("ab.ork"), .seen = &seen},
                                        .next   = &again},
                     .next   = &warning});
        answer = prompts.askSaveFile(nullptr, request);
    }
    warning.reset();
    again.reset();
    EXPECT_TRUE(isSamePath(savedFileOf(answer).parent_path(), directory().path()));
    EXPECT_EQ(savedFileOf(answer).filename(), "ab.ork");
    EXPECT_EQ(title, QStringLiteral("Illegal filename"));
    EXPECT_EQ(lines, QStringList{QStringLiteral(
                         "The filename 'x|y.ork' may not contain the character ' | '. Please "
                         "remove it.")});
    EXPECT_TRUE(isDirectory(seen.directory, directory().path()));
    ASSERT_EQ(seen.proposed.size(), 1);
    EXPECT_EQ(QtStrings::toPath(seen.proposed.front()).filename(), "x|y.ork");

    // A save that does not refuse them (a plain JFileChooser) takes the name.
    const Gui::Test::NextModalDialog choose(ChooseFile{.file = path("x|y.ork"), .seen = nullptr});
    UserPrompts::SaveFileRequest     plain = request;
    plain.refuseIllegalNames               = false;
    EXPECT_EQ(savedFileOf(prompts.askSaveFile(nullptr, plain)).filename(), "x|y.ork");
}

TEST_F(WidgetFileDialogTest, ASaveAnswersTheNameFilterChosenAndKeepsItWhenAskedAgain)
{
    // JFileChooser.getFileFilter() after the save dialog (a plot's export gives the name the
    // chosen kind's suffix); SaveFileChooser's chooser stays open after "Illegal filename", with
    // its filter: shown again here, the dialog starts with the filter chosen.
    WidgetUserPrompts                          prompts;
    const QString                              png = QStringLiteral("PNG image (*.png)");
    const QString                              svg = QStringLiteral("SVG files (*.svg)");
    std::optional<Gui::Test::NextModalDialog>  warning;
    std::optional<Gui::Test::NextModalDialog>  again;
    QStringList                                lines;
    QString                                    title;
    FileDialogSeen                             first;
    FileDialogSeen                             second;
    std::optional<UserPrompts::SaveFileAnswer> answer;
    const UserPrompts::SaveFileRequest         request{.title              = QString(),
                                                       .nameFilters        = {png, svg},
                                                       .directory          = directory().path(),
                                                       .fileName           = QString(),
                                                       .saveSimulationData = std::nullopt,
                                                       .refuseIllegalNames = true};
    {
        const Gui::Test::NextModalDialog choose(ThenNext{
            .first = ChooseFileAs{.file = path("x|y"), .filter = svg, .seen = &first},
            .second =
                ThenNext{
                    .first  = CloseMessage{.lines = &lines, .title = &title},
                    .second = ChooseFileAs{.file = path("plot"), .filter = svg, .seen = &second},
                    .next   = &again},
            .next = &warning});
        answer = prompts.askSaveFile(nullptr, request);
    }
    warning.reset();
    again.reset();
    EXPECT_EQ(first.selectedFilter, png);
    EXPECT_EQ(title, QStringLiteral("Illegal filename"));
    EXPECT_EQ(second.selectedFilter, svg);
    ASSERT_TRUE(answer.has_value());
    EXPECT_EQ(savedFileOf(answer).filename(), "plot");
    EXPECT_EQ(nameFilterOf(answer), svg);

    // Without filters, none.
    const Gui::Test::NextModalDialog choose(ChooseFile{.file = path("plain"), .seen = nullptr});
    UserPrompts::SaveFileRequest     plain = request;
    plain.nameFilters.clear();
    const std::optional<UserPrompts::SaveFileAnswer> unfiltered =
        prompts.askSaveFile(nullptr, plain);
    ASSERT_TRUE(unfiltered.has_value());
    EXPECT_TRUE(nameFilterOf(unfiltered).isEmpty());
}

TEST_F(WidgetUserPromptsTest, TheIllegalNameTextPutsInTheNameAsItIs)
{
    // String.format("... '%s' ... ' %c ' ...", fileName, c).
    EXPECT_EQ(WidgetUserPrompts::illegalFileNameText(QStringLiteral("a%1b:c.ork"), QChar(u':')),
              QStringLiteral("The filename 'a%1b:c.ork' may not contain the character ' : '. "
                             "Please remove it."));
}

/// Answers the decal question with Yes or No.
struct AnswerDecal
{
    bool yes;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<DecalNotFoundDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        (yes ? dialog->yesButton() : dialog->noButton())->click();
    }
};

TEST_F(WidgetFileDialogTest, AsksWhetherToLookForADecalFileAndThenForTheFile)
{
    // DecalNotFoundDialog.showDialog(): the question, then a file chooser in the
    // directory.
    WidgetUserPrompts                          prompts;
    const QString                              message = QStringLiteral("Could not find it.");
    std::optional<Gui::Test::NextModalDialog>  chooser;
    FileDialogSeen                             seen;
    std::optional<UserPrompts::OpenFileAnswer> answer;
    {
        const Gui::Test::NextModalDialog yes(
            ThenNext{.first  = AnswerDecal{.yes = true},
                     .second = ChooseFile{.file = path("b.png"), .seen = &seen},
                     .next   = &chooser});
        answer = prompts.askDecalFile(nullptr, message, directory().path());
    }
    chooser.reset();
    ASSERT_TRUE(answer.has_value());
    EXPECT_TRUE(isFile(filesOf(answer), directory().resolve("b.png")));
    EXPECT_EQ(seen.objectName, QStringLiteral("decalFileDialog"));
    EXPECT_TRUE(isDirectory(seen.directory, directory().path()));
    EXPECT_EQ(seen.fileMode, QFileDialog::ExistingFile);
    EXPECT_TRUE(seen.nameFilters.isEmpty() ||
                seen.nameFilters == QStringList{QStringLiteral("All Files (*)")});

    // No: no file chooser; Yes and then Cancel in the chooser: no file.
    {
        const Gui::Test::NextModalDialog no(AnswerDecal{.yes = false});
        EXPECT_EQ(prompts.askDecalFile(nullptr, message, directory().path()), std::nullopt);
    }
    {
        const Gui::Test::NextModalDialog yes(
            ThenNext{.first = AnswerDecal{.yes = true}, .second = ChooseFile{}, .next = &chooser});
        EXPECT_EQ(prompts.askDecalFile(nullptr, message, directory().path()), std::nullopt);
    }
    chooser.reset();
}

/// What the modal warnings dialog showed; closed with OK.
struct CloseWarningsDialog
{
    QString*     title;
    QStringList* lines;
    QStringList* items;

    void operator()(QWidget& widget) const
    {
        if (auto const* const warnings = qobject_cast<WarningDialog*>(&widget); warnings != nullptr)
        {
            *title = warnings->windowTitle();
            *lines = warnings->messageLines();
            *items = warnings->warningList().messages();
            warnings->buttonBox()->button(QDialogButtonBox::Ok)->click();
            return;
        }
        auto* const both = qobject_cast<ErrorWarningDialog*>(&widget);
        ASSERT_NE(both, nullptr);
        *title = both->windowTitle();
        *lines = both->messageLines();
        *items = both->errorList().messages() + both->warningList().messages();
        both->buttonBox()->button(QDialogButtonBox::Ok)->click();
    }
};

TEST_F(WidgetUserPromptsTest, ShowsTheWarningsAndTheErrors)
{
    WidgetUserPrompts prompts;
    QString           title;
    QStringList       lines;
    QStringList       items;
    {
        const Gui::Test::NextModalDialog close(
            CloseWarningsDialog{.title = &title, .lines = &lines, .items = &items});
        prompts.showWarnings(nullptr, {.title    = QStringLiteral("Warnings while opening file"),
                                       .lines    = {QStringLiteral("line")},
                                       .warnings = {QStringLiteral("warning")}});
    }
    EXPECT_EQ(title, QStringLiteral("Warnings while opening file"));
    EXPECT_EQ(lines, QStringList{QStringLiteral("line")});
    EXPECT_EQ(items, QStringList{QStringLiteral("warning")});

    const Gui::Test::NextModalDialog close(
        CloseWarningsDialog{.title = &title, .lines = &lines, .items = &items});
    prompts.showErrorsAndWarnings(nullptr,
                                  {.title    = QStringLiteral("Errors/Warnings while saving file"),
                                   .lines    = {QStringLiteral("line")},
                                   .errors   = {QStringLiteral("error")},
                                   .warnings = {QStringLiteral("warning")}});
    EXPECT_EQ(title, QStringLiteral("Errors/Warnings while saving file"));
    EXPECT_EQ(items, (QStringList{QStringLiteral("error"), QStringLiteral("warning")}));
}

/// Destroys the parent of the modal dialog, and so the dialog, while it is
/// open.
struct DestroyParent
{
    std::unique_ptr<QWidget>* parent;

    void operator()(QWidget& /*dialog*/) const { parent->reset(); }
};

TEST_F(WidgetUserPromptsTest, AQuestionWhoseParentIsDestroyedGivesItsCancellingAnswer)
{
    // The dialog the question is parented to can be deleted while the question is
    // open (run 2's configuration dialog by its window): each dialog is made for
    // its question (ModalDialog).
    WidgetUserPrompts prompts;
    auto              parent = std::make_unique<QWidget>();
    {
        const Gui::Test::NextModalDialog destroy(DestroyParent{.parent = &parent});
        EXPECT_FALSE(prompts.askOverwrite(parent.get(), QStringLiteral("rocket.ork")));
    }
    parent = std::make_unique<QWidget>();
    {
        const Gui::Test::NextModalDialog destroy(DestroyParent{.parent = &parent});
        const UserPrompts::DiscardAnswer answer = prompts.askDiscardChanges(
            parent.get(), UserPrompts::DiscardQuestion::COMPONENT_CHANGES);
        EXPECT_FALSE(answer.discard);
        EXPECT_FALSE(answer.doNotAskAgain);
    }
    parent = std::make_unique<QWidget>();
    {
        const Gui::Test::NextModalDialog destroy(DestroyParent{.parent = &parent});
        EXPECT_EQ(prompts
                      .askInsertPosition(parent.get(),
                                         UserPrompts::InsertPositionQuestion::BODY_COMPONENT)
                      .position,
                  UserPrompts::InsertPosition::CANCEL);
    }
    parent = std::make_unique<QWidget>();
    {
        const Gui::Test::NextModalDialog destroy(DestroyParent{.parent = &parent});
        EXPECT_EQ(prompts.askSaveChanges(parent.get(), QStringLiteral("Rocket")),
                  SaveChangesAnswer::CANCEL);
    }
    parent = std::make_unique<QWidget>();
    {
        const Gui::Test::NextModalDialog destroy(DestroyParent{.parent = &parent});
        prompts.showMessage(parent.get(), {.severity = UserPrompts::Severity::ERROR,
                                           .title    = QStringLiteral("Saving failed"),
                                           .lines    = {QStringLiteral("gone")}});
    }
    parent = std::make_unique<QWidget>();
    {
        const Gui::Test::NextModalDialog destroy(DestroyParent{.parent = &parent});
        Guard::Failure                   failure;
        failure.message = QStringLiteral("boom");
        EXPECT_EQ(prompts.showInternalError(parent.get(), failure),
                  UserPrompts::InternalErrorAnswer::CONTINUE);
    }
    EXPECT_EQ(parent, nullptr);
}

TEST_F(WidgetFileDialogTest, AFileQuestionWhoseParentIsDestroyedAsksNothingMore)
{
    WidgetUserPrompts prompts;
    auto              parent = std::make_unique<QWidget>();
    {
        // The save dialog goes with its parent: no storage options are asked after
        // it.
        const Gui::Test::NextModalDialog destroy(DestroyParent{.parent = &parent});
        EXPECT_EQ(prompts.askSaveFile(parent.get(), {.title              = QString(),
                                                     .nameFilters        = {},
                                                     .directory          = directory().path(),
                                                     .fileName           = QString(),
                                                     .saveSimulationData = true,
                                                     .refuseIllegalNames = false}),
                  std::nullopt);
    }
    parent = std::make_unique<QWidget>();
    {
        // The decal question goes with its parent: no file chooser after it.
        const Gui::Test::NextModalDialog destroy(DestroyParent{.parent = &parent});
        EXPECT_EQ(prompts.askDecalFile(parent.get(), QStringLiteral("gone"), directory().path()),
                  std::nullopt);
    }
    EXPECT_EQ(parent, nullptr);
}

/// A rename's "Save as default" callback that counts its calls.
struct CountSaveAsDefault
{
    int* count;

    void operator()(QWidget* /*dialog*/, const QString& /*name*/) const { ++*count; }
};

/// @p answer's choice, or a test failure and a default choice when there is none. (A plain if is
/// what clang-tidy's unchecked-optional-access check follows; it cannot see through ASSERT_TRUE.)
template <class Choice>
[[nodiscard]] Choice required(const std::optional<Choice>& answer)
{
    if (!answer.has_value())
    {
        ADD_FAILURE() << "no answer";
        return {};
    }
    return *answer;
}

/// Checks that the modal dialog is a @p Dialog parented to @p parent, then
/// clicks its OK.
template <class Dialog>
struct ExpectDialogAndOk
{
    const QWidget* parent;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<Dialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        EXPECT_EQ(dialog->parentWidget(), parent);
        dialog->okButton().click();
    }
};

TEST_F(WidgetUserPromptsTest, AskIgnitionIsOpenRocketsIgnitionSelectionDialog)
{
    // OK answers what the dialog started with: the request's radio button, event
    // and delay.
    WidgetUserPrompts                 prompts;
    QWidget                           parent;
    const UserPrompts::IgnitionChoice initial{
        .scope = UserPrompts::ConfigurationScope::THIS_CONFIGURATION,
        .event = IgnitionEvent::BURNOUT,
        .delay = 1.5};
    const Gui::Test::NextModalDialog ok(
        ExpectDialogAndOk<IgnitionSelectionDialog>{.parent = &parent});
    const UserPrompts::IgnitionChoice answer = required(prompts.askIgnition(
        &parent, {.configurationName = QStringLiteral("[B4-4]"), .initial = initial}));
    EXPECT_EQ(answer.scope, initial.scope);
    EXPECT_EQ(answer.event, initial.event);
    EXPECT_EQ(answer.delay, initial.delay);
}

TEST_F(WidgetUserPromptsTest, AskDeploymentIsOpenRocketsDeploymentSelectionDialog)
{
    WidgetUserPrompts       prompts;
    QWidget                 parent;
    DeploymentConfiguration deployment;
    deployment.setDeployEvent(DeploymentConfiguration::DeployEvent::ALTITUDE);
    deployment.setDeployAltitude(150);
    deployment.setDeployDelay(2);
    const Gui::Test::NextModalDialog ok(
        ExpectDialogAndOk<DeploymentSelectionDialog>{.parent = &parent});
    const UserPrompts::DeploymentChoice answer = required(prompts.askDeployment(
        &parent, {.configurationName = QStringLiteral("[B4-4]"),
                  .initial     = {.scope      = UserPrompts::ConfigurationScope::ALL_CONFIGURATIONS,
                                  .deployment = deployment},
                  .bottomStage = false}));
    EXPECT_EQ(answer.scope, UserPrompts::ConfigurationScope::ALL_CONFIGURATIONS);
    EXPECT_EQ(answer.deployment, deployment);
}

TEST_F(WidgetUserPromptsTest, AskSeparationIsOpenRocketsSeparationSelectionDialog)
{
    WidgetUserPrompts            prompts;
    QWidget                      parent;
    StageSeparationConfiguration separation;
    separation.setSeparationEvent(
        StageSeparationConfiguration::SeparationEvent::ALTITUDE_ASCENDING);
    separation.setSeparationAltitude(300);
    separation.setSeparationDelay(0.5);
    const Gui::Test::NextModalDialog ok(
        ExpectDialogAndOk<SeparationSelectionDialog>{.parent = &parent});
    const UserPrompts::SeparationChoice answer = required(prompts.askSeparation(
        &parent, {.configurationName = QStringLiteral("[H148R-0; H148R-0]"),
                  .initial = {.scope      = UserPrompts::ConfigurationScope::THIS_CONFIGURATION,
                              .separation = separation}}));
    EXPECT_EQ(answer.scope, UserPrompts::ConfigurationScope::THIS_CONFIGURATION);
    EXPECT_EQ(answer.separation.getSeparationEvent(), separation.getSeparationEvent());
    EXPECT_EQ(answer.separation.getSeparationAltitude(), 300.0);
    EXPECT_EQ(answer.separation.getSeparationDelay(), 0.5);
}

/// Checks that the modal dialog is a RenameConfigDialog parented to @p parent,
/// then clicks "Save as default" and OK.
struct SaveAsDefaultThenOk
{
    const QWidget* parent;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<RenameConfigDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        EXPECT_EQ(dialog->parentWidget(), parent);
        dialog->saveAsDefaultButton().click();
        dialog->okButton().click();
    }
};

TEST_F(WidgetUserPromptsTest, AskRenameConfigurationIsOpenRocketsRenameConfigDialog)
{
    WidgetUserPrompts                            prompts;
    QWidget                                      parent;
    int                                          savedAsDefault = 0;
    const Gui::Test::NextModalDialog             answering(SaveAsDefaultThenOk{.parent = &parent});
    const UserPrompts::RenameConfigurationAnswer answer = prompts.askRenameConfiguration(
        &parent, {.name          = QStringLiteral("[{motors}]"),
                  .saveAsDefault = CountSaveAsDefault{.count = &savedAsDefault}});
    EXPECT_EQ(answer.action, UserPrompts::RenameAction::RENAME);
    EXPECT_EQ(answer.name, QStringLiteral("[{motors}]"));
    EXPECT_EQ(savedAsDefault, 1);
}

TEST_F(WidgetUserPromptsTest, AValueQuestionWhoseParentIsDestroyedAnswersNothing)
{
    // The tab's window can go while its question is open (another window's event,
    // a timer): each dialog is made for its question (ModalDialog). Proved under
    // ASan.
    WidgetUserPrompts prompts;
    auto              parent = std::make_unique<QWidget>();
    {
        const Gui::Test::NextModalDialog destroy(DestroyParent{.parent = &parent});
        EXPECT_FALSE(prompts
                         .askIgnition(parent.get(), {.configurationName = QStringLiteral("[B4-4]"),
                                                     .initial           = {}})
                         .has_value());
    }
    parent = std::make_unique<QWidget>();
    {
        const Gui::Test::NextModalDialog destroy(DestroyParent{.parent = &parent});
        EXPECT_FALSE(
            prompts
                .askDeployment(parent.get(), {.configurationName = QStringLiteral("[B4-4]"),
                                              .initial           = {},
                                              .bottomStage       = true})
                .has_value());
    }
    parent = std::make_unique<QWidget>();
    {
        const Gui::Test::NextModalDialog destroy(DestroyParent{.parent = &parent});
        EXPECT_FALSE(
            prompts
                .askSeparation(parent.get(),
                               {.configurationName = QStringLiteral("[B4-4]"), .initial = {}})
                .has_value());
    }
    parent = std::make_unique<QWidget>();
    {
        int                              savedAsDefault = 0;
        const Gui::Test::NextModalDialog destroy(DestroyParent{.parent = &parent});
        EXPECT_EQ(
            prompts
                .askRenameConfiguration(
                    parent.get(), {.name          = QStringLiteral("[{motors}]"),
                                   .saveAsDefault = CountSaveAsDefault{.count = &savedAsDefault}})
                .action,
            UserPrompts::RenameAction::CANCEL);
        EXPECT_EQ(savedAsDefault, 0);
    }
    EXPECT_EQ(parent, nullptr);
}

/// A rename's "Save as default" that destroys the parent of the dialog, and so
/// the dialog (as a window closed during the message it shows would).
struct DestroyParentOnSaveAsDefault
{
    std::unique_ptr<QWidget>* parent;

    void operator()(QWidget* /*dialog*/, const QString& /*name*/) const { parent->reset(); }
};

/// Clicks "Save as default" of the rename dialog.
struct ClickSaveAsDefault
{
    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<RenameConfigDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        dialog->saveAsDefaultButton().click();
    }
};

TEST_F(WidgetUserPromptsTest, ASaveAsDefaultThatTakesTheDialogAlongCancelsTheRename)
{
    // The dialog goes inside its own button's click (proved under ASan).
    WidgetUserPrompts                            prompts;
    auto                                         parent = std::make_unique<QWidget>();
    const Gui::Test::NextModalDialog             click(ClickSaveAsDefault{});
    const UserPrompts::RenameConfigurationAnswer answer = prompts.askRenameConfiguration(
        parent.get(), {.name          = QStringLiteral("[{motors}]"),
                       .saveAsDefault = DestroyParentOnSaveAsDefault{.parent = &parent}});
    EXPECT_EQ(answer.action, UserPrompts::RenameAction::CANCEL);
    EXPECT_EQ(parent, nullptr);
}

TEST_F(WidgetUserPromptsTest, TheMilestoneThreeQuestionsCancelAtOnceUntilTheirDialogsCome)
{
    // Run 3 of Milestone 3 brings the preferences' user files: until then this gives its
    // cancelling answer without a dialog (one would hang here: nothing answers it). The value
    // dialogs, the motor chooser, the parts library and the start-up's problems have their
    // dialogs (the tests above and below, tests/gui/main/motor_chooser_prompt_tests.cpp and
    // tests/gui/dialogs/preset/).
    WidgetUserPrompts            prompts;
    UserPrompts::OpenFileRequest files;
    files.selection         = UserPrompts::OpenFileSelection::FILES_AND_DIRECTORIES;
    files.approveButtonText = QStringLiteral("Add");
    EXPECT_EQ(prompts.askOpenFiles(nullptr, files), std::nullopt);
}

/// The text of OpenRocket's warning about a thrust-curve file @p file that its loader refused
/// with @p message (MotorDatabaseLoader.loadFile(), without the paragraph's width).
[[nodiscard]] QString thrustCurveWarning(const QString& message, const QString& file)
{
    return QStringLiteral("<html><body><p><i>") + message +
           QStringLiteral("</i>.<br><br>Thrust curve <b>") + file +
           QStringLiteral(
               "</b> will be ignored during import.<br>You can try replacing, deleting "
               "or manually editing the thrust curve file to fix this issue."
               "</p></body></html>");
}

/// The text of OpenRocket's warning about a component preset file @p file that could not be
/// read (ComponentPresetDatabaseLoader.showLoadingErrorDialog(), without the paragraph's width).
[[nodiscard]] QString componentPresetWarning(const QString& message, const QString& file)
{
    return QStringLiteral("<html><body><p><i>") + message +
           QStringLiteral("</i>.<br><br>Component preset <b>") + file +
           QStringLiteral(
               "</b> will be ignored during import.<br>You can try replacing, deleting "
               "or manually editing the component preset file to fix this issue."
               "</p></body></html>");
}

TEST_F(WidgetUserPromptsTest, ShowsTheStartsDatabaseProblemsInOneMessage)
{
    const std::filesystem::path motorFile = std::filesystem::path("motors") / "broken.eng";
    const std::vector<UserPrompts::DatabaseProblem> problems{
        {.database = UserPrompts::DatabaseKind::THRUST_CURVES,
         .file     = motorFile,
         .message  = QStringLiteral("Illegal number")},
        {.database = UserPrompts::DatabaseKind::COMPONENT_PRESETS,
         .file     = "broken.orc",
         .message  = QStringLiteral("Unexpected end of file")}};
    WidgetUserPrompts                prompts;
    QStringList                      lines;
    QString                          title;
    const Gui::Test::NextModalDialog close(CloseMessage{.lines = &lines, .title = &title});
    prompts.showDatabaseProblems(nullptr, problems);
    EXPECT_EQ(title, QStringLiteral(
                         "Error upon thrust curve import / Error upon component preset import"));
    EXPECT_EQ(lines, (QStringList{thrustCurveWarning(QStringLiteral("Illegal number"),
                                                     QtStrings::fromPath(motorFile)),
                                  componentPresetWarning(QStringLiteral("Unexpected end of file"),
                                                         QStringLiteral("broken.orc"))}));
}

TEST_F(WidgetUserPromptsTest, TheDatabaseProblemsAreTitledAfterTheirDatabase)
{
    const std::vector<UserPrompts::DatabaseProblem> motors{
        {.database = UserPrompts::DatabaseKind::THRUST_CURVES, .file = "a.eng", .message = {}},
        {.database = UserPrompts::DatabaseKind::THRUST_CURVES, .file = "b.rse", .message = {}}};
    EXPECT_EQ(WidgetUserPrompts::databaseProblemsTitle(motors),
              QStringLiteral("Error upon thrust curve import"));
    EXPECT_EQ(WidgetUserPrompts::databaseProblemLines(motors).size(), 2);
    const std::vector<UserPrompts::DatabaseProblem> presets{
        {.database = UserPrompts::DatabaseKind::COMPONENT_PRESETS, .file = "a.orc", .message = {}}};
    EXPECT_EQ(WidgetUserPrompts::databaseProblemsTitle(presets),
              QStringLiteral("Error upon component preset import"));
}

TEST_F(WidgetUserPromptsTest, TheDatabaseProblemsShowTheFileAndTheMessageAsTheyAre)
{
    // OpenRocket's HTML would read them as markup.
    const std::vector<UserPrompts::DatabaseProblem> problems{
        {.database = UserPrompts::DatabaseKind::COMPONENT_PRESETS,
         .file     = "a<b>&c.orc",
         .message  = QStringLiteral("Expected '<' & more")}};
    EXPECT_EQ(WidgetUserPrompts::databaseProblemLines(problems),
              QStringList{componentPresetWarning(QStringLiteral("Expected '&lt;' &amp; more"),
                                                 QStringLiteral("a&lt;b&gt;&amp;c.orc"))});
}
}  // namespace

}  // namespace QtRocket::Gui
