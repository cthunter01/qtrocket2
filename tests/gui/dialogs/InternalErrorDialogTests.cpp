#include "QtRocket/gui/dialogs/InternalErrorDialog.h"

#include <algorithm>
#include <memory>

#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QLabel>
#include <QObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QSysInfo>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>
#include <QtVersion>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/gui/util/BuildInfo.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/UserPrompts.h"

namespace QtRocket::Gui
{

namespace
{

using InternalErrorDialogTest = Test::GuiTest;
using Answer                  = UserPrompts::InternalErrorAnswer;

/// A BugError's failure, as Guard reports it.
[[nodiscard]] Guard::Failure bugFailure()
{
    Guard::Failure failure;
    failure.kind     = Guard::Failure::Kind::BUG;
    failure.message  = QStringLiteral("BUG: the stage has no parent (Rocket.cpp:12)");
    failure.file     = QStringLiteral("Rocket.cpp");
    failure.line     = 12;
    failure.function = QStringLiteral("void QtRocket::Rocket::check()");
    failure.context  = QStringLiteral("Delete");
    return failure;
}

/// Whether every message line of @p dialog is plain text that wraps.
[[nodiscard]] bool plainAndWrapped(const InternalErrorDialog& dialog)
{
    return std::ranges::all_of(dialog.messageLabels(), [](const QLabel* label) {
        return label->textFormat() == Qt::PlainText && label->wordWrap();
    });
}

/// Clicks "Quit QtRocket" in the internal error dialog.
struct ClickQuit
{
    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<InternalErrorDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        dialog->quitButton()->click();
    }
};

TEST_F(InternalErrorDialogTest, ShowsTheFailureAndWhatToDo)
{
    const InternalErrorDialog dialog(bugFailure());
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Internal error"));
    EXPECT_EQ(
        dialog.messageLines(),
        (QStringList{
            QStringLiteral("QtRocket ran into an internal error. This is a bug in QtRocket."),
            QStringLiteral("BUG: the stage has no parent (Rocket.cpp:12)"),
            QStringLiteral("In: Delete"),
            QStringLiteral(
                "You can continue, but the design may be in an inconsistent state now: save it "
                "under a new name (File > Save as) rather than over its file. Quit QtRocket ends "
                "QtRocket at once, without saving any design."),
            QStringLiteral(
                "Please report this bug to the developers of QtRocket, with the details below.")}));
    // The failure's texts are never markup.
    EXPECT_TRUE(plainAndWrapped(dialog));
    ASSERT_NE(dialog.detailsView(), nullptr);
    EXPECT_EQ(dialog.detailsView()->toPlainText(), InternalErrorDialog::detailsFor(bugFailure()));
}

TEST_F(InternalErrorDialogTest, TheDetailsNameTheVersionsTheSystemAndTheSource)
{
    const QStringList lines =
        InternalErrorDialog::detailsFor(bugFailure()).split(QStringLiteral("\n"));
    EXPECT_EQ(lines,
              (QStringList{BuildInfo::versionText(),
                           QStringLiteral("Qt ") + QString::fromLatin1(qVersion()),
                           QStringLiteral("%1 (%2 %3, %4)")
                               .arg(QSysInfo::prettyProductName(), QSysInfo::kernelType(),
                                    QSysInfo::kernelVersion(), QSysInfo::currentCpuArchitecture()),
                           QString(), QStringLiteral("Kind: BugError (a broken invariant)"),
                           QStringLiteral("Message: BUG: the stage has no parent (Rocket.cpp:12)"),
                           QStringLiteral("Source: Rocket.cpp, line 12"),
                           QStringLiteral("Function: void QtRocket::Rocket::check()"),
                           QStringLiteral("In: Delete")}));
}

TEST_F(InternalErrorDialogTest, AnErrorConditionHasNoSource)
{
    Guard::Failure failure;
    failure.kind              = Guard::Failure::Kind::ERROR_CONDITION;
    failure.message           = QStringLiteral("Undo/Redo error: no such step");
    const QStringList message = InternalErrorDialog::messageFor(failure);
    EXPECT_EQ(message.size(), 4);
    EXPECT_EQ(message.at(1), QStringLiteral("Undo/Redo error: no such step"));
    const QString details = InternalErrorDialog::detailsFor(failure);
    EXPECT_TRUE(details.contains(QStringLiteral("Kind: error condition (no exception)")));
    EXPECT_FALSE(details.contains(QStringLiteral("Source:")));
    EXPECT_FALSE(details.contains(QStringLiteral("Function:")));
    EXPECT_FALSE(details.contains(QStringLiteral("In:")));
}

TEST_F(InternalErrorDialogTest, ContinueIsTheDefaultAndClosingTheDialogContinues)
{
    const InternalErrorDialog dialog(bugFailure());
    EXPECT_TRUE(dialog.continueButton()->isDefault());
    EXPECT_EQ(dialog.continueButton()->text(), QStringLiteral("Continue"));
    EXPECT_EQ(dialog.buttonBox()->buttonRole(dialog.continueButton()),
              QDialogButtonBox::AcceptRole);
    dialog.continueButton()->click();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.answer(), Answer::CONTINUE);

    InternalErrorDialog closed(bugFailure());
    closed.reject();
    EXPECT_EQ(closed.answer(), Answer::CONTINUE);
}

TEST_F(InternalErrorDialogTest, QuitQtRocketAnswersQuit)
{
    // On the heap: the click changes the dialog's answer, so the dialog cannot be a const object.
    const auto dialog = std::make_unique<InternalErrorDialog>(bugFailure());
    EXPECT_EQ(dialog->quitButton()->text(), QStringLiteral("Quit QtRocket"));
    dialog->quitButton()->click();
    EXPECT_EQ(dialog->answer(), Answer::QUIT);
    EXPECT_EQ(dialog->result(), QDialog::Accepted);
}

TEST_F(InternalErrorDialogTest, CopyDetailsPutsTheDetailsOnTheClipboard)
{
    const InternalErrorDialog dialog(bugFailure());
    EXPECT_EQ(dialog.copyButton()->text(), QStringLiteral("Copy details"));
    dialog.copyButton()->click();
    EXPECT_EQ(QGuiApplication::clipboard()->text(), InternalErrorDialog::detailsFor(bugFailure()));
    EXPECT_EQ(dialog.answer(), Answer::CONTINUE);
    EXPECT_EQ(dialog.result(), QDialog::Rejected);
    QGuiApplication::clipboard()->clear();
}

TEST_F(InternalErrorDialogTest, AskShowsTheDialogAndReturnsTheAnswer)
{
    const Gui::Test::NextModalDialog quit{ClickQuit{}};
    EXPECT_EQ(InternalErrorDialog::ask(nullptr, bugFailure()), Answer::QUIT);
    EXPECT_TRUE(quit.acted());
}

}  // namespace

}  // namespace QtRocket::Gui
