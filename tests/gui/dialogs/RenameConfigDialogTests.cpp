// The dialog "Rename Configuration" (OpenRocket's RenameConfigDialog). The texts are
// messages.properties', the widgets' order and the information's text those of the scouting's
// dump of OpenRocket's dialog (A simple model rocket).

#include "QtRocket/gui/dialogs/RenameConfigDialog.h"

#include <vector>

#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPalette>
#include <QPushButton>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/util/BugError.h"
#include "ScopedApplicationPalette.h"

namespace QtRocket::Gui
{

namespace
{

using RenameConfigDialogTest = Test::GuiTest;
using RenameAction           = UserPrompts::RenameAction;

/// A click of "Save as default": whether it was given the rename dialog, and the name.
struct SaveAsDefaultCall
{
    bool    fromTheDialog;
    QString name;
};

/// The rename's "Save as default" action that records its calls into @p calls.
struct RecordSaveAsDefault
{
    std::vector<SaveAsDefaultCall>* calls;

    void operator()(QWidget* dialog, const QString& name) const
    {
        calls->push_back(
            {.fromTheDialog = qobject_cast<RenameConfigDialog*>(dialog) != nullptr, .name = name});
    }
};

/// The question about a configuration named "[{motors}]", recording "Save as default" in
/// @p calls.
[[nodiscard]] UserPrompts::RenameConfigurationRequest request(std::vector<SaveAsDefaultCall>& calls)
{
    return {.name = QStringLiteral("[{motors}]"), .saveAsDefault = RecordSaveAsDefault{&calls}};
}

/// How the dialog is answered.
enum class Answer
{
    OK,
    SAVE_AS_DEFAULT_THEN_CANCEL,
    RESET,
    CANCEL,
    ESCAPE,
    CLOSE,
};

/// Types "My {motors} {cases}" into the text field, then answers.
struct TypeAndAnswer
{
    Answer answer = Answer::OK;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<RenameConfigDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        QLineEdit& text = dialog->textField();
        text.selectAll();
        QTest::keyClicks(&text, QStringLiteral("My {motors} {cases}"));
        switch (answer)
        {
            case Answer::OK:
                dialog->okButton().click();
                break;
            case Answer::SAVE_AS_DEFAULT_THEN_CANCEL:
                dialog->saveAsDefaultButton().click();
                // Still open.
                EXPECT_TRUE(dialog->isVisible());
                dialog->cancelButton().click();
                break;
            case Answer::RESET:
                dialog->resetToDefaultButton().click();
                break;
            case Answer::CANCEL:
                dialog->cancelButton().click();
                break;
            case Answer::ESCAPE:
                QTest::keyClick(&text, Qt::Key_Escape);
                break;
            case Answer::CLOSE:
                dialog->close();
                break;
        }
    }
};

/// The rename of a request recording into @p calls, answered as @p answer.
[[nodiscard]] UserPrompts::RenameConfigurationAnswer rename(std::vector<SaveAsDefaultCall>& calls,
                                                            Answer                          answer)
{
    const Gui::Test::NextModalDialog typing(TypeAndAnswer{.answer = answer});
    return RenameConfigDialog::ask(nullptr, request(calls));
}

TEST_F(RenameConfigDialogTest, ShowsOpenRocketsDialog)
{
    std::vector<SaveAsDefaultCall> calls;
    const RenameConfigDialog       dialog(request(calls));
    EXPECT_EQ(dialog.objectName(), QStringLiteral("RenameConfigDialog"));
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Rename Configuration"));
    EXPECT_EQ(dialog.windowModality(), Qt::ApplicationModal);
    EXPECT_EQ(dialog.nameLabel().text(), QStringLiteral("Name for flight configuration: \u2020"));
    EXPECT_EQ(dialog.textField().objectName(), QStringLiteral("textbox"));
    EXPECT_EQ(dialog.textField().text(), QStringLiteral("[{motors}]"));
    EXPECT_EQ(dialog.textField().minimumWidth(), RenameConfigDialog::kTextFieldWidth);
    EXPECT_EQ(dialog.okButton().text(), QStringLiteral("OK"));
    EXPECT_EQ(dialog.okButton().objectName(), QStringLiteral("okButton"));
    EXPECT_TRUE(dialog.okButton().isDefault());
    EXPECT_EQ(dialog.saveAsDefaultButton().text(), QStringLiteral("Save as default"));
    EXPECT_EQ(dialog.saveAsDefaultButton().objectName(), QStringLiteral("saveAsDefaultButton"));
    EXPECT_EQ(dialog.resetToDefaultButton().text(), QStringLiteral("Reset to default"));
    EXPECT_EQ(dialog.resetToDefaultButton().objectName(), QStringLiteral("resetToDefaultButton"));
    EXPECT_EQ(dialog.cancelButton().text(), QStringLiteral("Cancel"));
    EXPECT_EQ(dialog.cancelButton().objectName(), QStringLiteral("cancel"));
    // The four buttons in OpenRocket's order, left to right.
    EXPECT_EQ(dialog.okButton().parentWidget(), dialog.cancelButton().parentWidget());
    EXPECT_EQ(dialog.infoLabel().objectName(), QStringLiteral("info"));
    EXPECT_EQ(dialog.infoLabel().textFormat(), Qt::RichText);
    EXPECT_EQ(calls.size(), 0U);
}

TEST_F(RenameConfigDialogTest, TheInformationIsOpenRocketsText)
{
    // The probe's text, with its tabs and arrows.
    EXPECT_EQ(
        RenameConfigDialog::infoText(),
        QStringLiteral(
            "<html>\u2020 The text '<b>{motors}</b>' will be replaced with the <b>motor "
            "designation(s).</b> <br><pre>\te.g. '{motors} \u2192 'M1350-0'</pre>The text "
            "'<b>{manufacturers}</b>' will be replaced with the <b>motor manufacturer(s).</b> "
            "<br><pre>\te.g. '{manufacturers}' \u2192 'AeroTech'</pre>The text '<b>{cases}</b>' "
            "will be replaced with the <b>motor case(s).</b> <br><pre>\te.g. '{cases}' \u2192 "
            "'SU 18.0x70.0'</pre>A <b>combination</b> of the above can be used (you may choose "
            "the separator). <br><pre>\te.g. '{manufacturers motors}' \u2192 'AeroTech "
            "M1350-0'</pre>"));
    std::vector<SaveAsDefaultCall> calls;
    const RenameConfigDialog       dialog(request(calls));
    EXPECT_EQ(dialog.infoLabel().text(), RenameConfigDialog::infoText());
}

TEST_F(RenameConfigDialogTest, TheInformationIsInTheDimTextColour)
{
    // UITheme's TEXT_DIM, which follows the theme (here: the palette).
    std::vector<SaveAsDefaultCall>            calls;
    const Gui::Test::ScopedApplicationPalette light(Gui::Test::ScopedApplicationPalette::light());
    const RenameConfigDialog                  dialog(request(calls));
    EXPECT_EQ(dialog.infoLabel().palette().color(QPalette::WindowText),
              ThemeColors::themeColor(ThemeColors::Key::TEXT_DIM, ThemeColors::Theme::LIGHT));
    const Gui::Test::ScopedApplicationPalette dark(Gui::Test::ScopedApplicationPalette::dark());
    // Qt hands a new application palette to the widgets through posted events.
    QCoreApplication::sendPostedEvents();
    EXPECT_EQ(dialog.infoLabel().palette().color(QPalette::WindowText),
              ThemeColors::themeColor(ThemeColors::Key::TEXT_DIM, ThemeColors::Theme::DARK));
}

TEST_F(RenameConfigDialogTest, OkAnswersTheNewName)
{
    std::vector<SaveAsDefaultCall>               calls;
    const UserPrompts::RenameConfigurationAnswer answer = rename(calls, Answer::OK);
    EXPECT_EQ(answer.action, RenameAction::RENAME);
    EXPECT_EQ(answer.name, QStringLiteral("My {motors} {cases}"));
    EXPECT_EQ(calls.size(), 0U);
}

TEST_F(RenameConfigDialogTest, ResetToDefaultAnswersReset)
{
    std::vector<SaveAsDefaultCall>               calls;
    const UserPrompts::RenameConfigurationAnswer answer = rename(calls, Answer::RESET);
    EXPECT_EQ(answer.action, RenameAction::RESET);
    EXPECT_EQ(answer.name, QString());
}

TEST_F(RenameConfigDialogTest, SaveAsDefaultGivesTheTextAndStaysOpen)
{
    // Java shows its message and stores the preference; the dialog stays open.
    std::vector<SaveAsDefaultCall>               calls;
    const UserPrompts::RenameConfigurationAnswer answer =
        rename(calls, Answer::SAVE_AS_DEFAULT_THEN_CANCEL);
    EXPECT_EQ(answer.action, RenameAction::CANCEL);
    ASSERT_EQ(calls.size(), 1U);
    EXPECT_TRUE(calls.front().fromTheDialog);
    EXPECT_EQ(calls.front().name, QStringLiteral("My {motors} {cases}"));
}

TEST_F(RenameConfigDialogTest, CancelEscapeAndTheCloseButtonAnswerCancel)
{
    std::vector<SaveAsDefaultCall> calls;
    for (const Answer cancelling : {Answer::CANCEL, Answer::ESCAPE, Answer::CLOSE})
    {
        const UserPrompts::RenameConfigurationAnswer answer = rename(calls, cancelling);
        EXPECT_EQ(answer.action, RenameAction::CANCEL);
        EXPECT_EQ(answer.name, QString());
    }
    EXPECT_EQ(calls.size(), 0U);
}

TEST_F(RenameConfigDialogTest, NeedsTheSaveAsDefaultAction)
{
    EXPECT_THROW(static_cast<void>(RenameConfigDialog(
                     {.name = QStringLiteral("[{motors}]"), .saveAsDefault = nullptr})),
                 BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
