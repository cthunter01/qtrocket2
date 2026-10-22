#include "QtRocket/gui/dialogs/ErrorWarningDialog.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidgetItem>
#include <QObject>
#include <QPalette>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/gui/dialogs/MessageList.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/widgets/StyledLabel.h"

namespace QtRocket::Gui
{

namespace
{

using ErrorWarningDialogTest = Test::GuiTest;

/// What the modal ErrorWarningDialog showed; it is closed with OK.
struct Shown
{
    QString     title;
    QStringList lines;
    QStringList errors;
    QStringList warnings;
};

struct CloseErrorsAndWarnings
{
    Shown* shown;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<ErrorWarningDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        shown->title    = dialog->windowTitle();
        shown->lines    = dialog->messageLines();
        shown->errors   = dialog->errorList().messages();
        shown->warnings = dialog->warningList().messages();
        dialog->buttonBox()->button(QDialogButtonBox::Ok)->click();
    }
};

TEST_F(ErrorWarningDialogTest, ShowsTheLinesTheErrorsAndTheWarnings)
{
    // BasicFrame.saveRASAeroFile()'s errors (the texts of the Java probe FileOperationsProbe:
    // SAVE-TXT1, ERRWARN-*).
    const QStringList lines{
        QStringLiteral("The following problems were encountered while saving 'rocket.ork'."),
        QStringLiteral("<html>Please <b>correct the errors</b>.</html>")};
    const ErrorWarningDialog dialog(lines, QStringLiteral("Errors/Warnings while saving file"),
                                    {QStringLiteral("an error")}, {QStringLiteral("a warning")});
    EXPECT_EQ(dialog.objectName(), QStringLiteral("errorWarningDialog"));
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Errors/Warnings while saving file"));
    EXPECT_EQ(dialog.messageLines(), lines);
    ASSERT_EQ(dialog.messageLabels().size(), 2);
    EXPECT_EQ(dialog.messageLabels().at(1)->textFormat(), Qt::RichText);
    EXPECT_FALSE(dialog.iconLabel()->pixmap().isNull());

    // "Errors" in the dark error colour, over the errors in that colour, one selectable.
    const QColor darkError = ThemeColors::color(ThemeColors::Key::DARK_ERROR);
    EXPECT_EQ(dialog.errorsLabel().text(), QStringLiteral("Errors"));
    EXPECT_EQ(dialog.errorsLabel().palette().color(QPalette::WindowText), darkError);
    EXPECT_EQ(dialog.errorList().messages(), QStringList{QStringLiteral("an error")});
    EXPECT_EQ(dialog.errorList().item(0)->foreground().color(), darkError);
    EXPECT_EQ(dialog.errorList().selectionMode(), QAbstractItemView::SingleSelection);

    // "Warnings:" over the warnings in the list's colour.
    const auto* const warningsLabel = dialog.findChild<QLabel*>(QStringLiteral("warningsLabel"));
    ASSERT_NE(warningsLabel, nullptr);
    EXPECT_EQ(warningsLabel->text(), QStringLiteral("Warnings:"));
    EXPECT_EQ(dialog.warningList().messages(), QStringList{QStringLiteral("a warning")});
    EXPECT_EQ(dialog.warningList().item(0)->data(Qt::ForegroundRole), QVariant());
    EXPECT_EQ(ErrorWarningDialog::kErrorsGapTop, 15);
    ASSERT_EQ(dialog.buttonBox()->buttons().size(), 1);
}

TEST_F(ErrorWarningDialogTest, ShowErrorsAndWarningsShowsTheDialogUntilItIsClosed)
{
    Shown                            shown;
    const Gui::Test::NextModalDialog close(CloseErrorsAndWarnings{.shown = &shown});
    ErrorWarningDialog::showErrorsAndWarnings(nullptr, {QStringLiteral("line")},
                                              QStringLiteral("Title"), {QStringLiteral("error")},
                                              {QStringLiteral("warning")});
    EXPECT_EQ(shown.title, QStringLiteral("Title"));
    EXPECT_EQ(shown.lines, QStringList{QStringLiteral("line")});
    EXPECT_EQ(shown.errors, QStringList{QStringLiteral("error")});
    EXPECT_EQ(shown.warnings, QStringList{QStringLiteral("warning")});
}

}  // namespace

}  // namespace QtRocket::Gui
