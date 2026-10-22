#include "QtRocket/gui/dialogs/WarningDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/gui/dialogs/MessageList.h"

namespace QtRocket::Gui
{

namespace
{

using WarningDialogTest = Test::GuiTest;

/// What the modal WarningDialog showed; it is closed with OK.
struct Shown
{
    QString     title;
    QStringList lines;
    QStringList warnings;
};

struct CloseWarnings
{
    Shown* shown;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<WarningDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        shown->title    = dialog->windowTitle();
        shown->lines    = dialog->messageLines();
        shown->warnings = dialog->warningList().messages();
        dialog->buttonBox()->button(QDialogButtonBox::Ok)->click();
    }
};

TEST_F(WarningDialogTest, ShowsTheLinesAboveTheListOfWarnings)
{
    // BasicFrame.open()'s warnings (the texts of the Java probe FileOperationsProbe: OPEN-*).
    const QStringList lines{
        QStringLiteral("The following problems were encountered while opening rocket.ork."),
        QStringLiteral("Some design features may not have been loaded correctly.")};
    const QStringList   warnings{QStringLiteral("Unknown element foo, ignoring."),
                                 QStringLiteral("Second warning")};
    const WarningDialog dialog(lines, QStringLiteral("Warnings while opening file"), warnings);
    EXPECT_EQ(dialog.objectName(), QStringLiteral("warningDialog"));
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Warnings while opening file"));
    EXPECT_EQ(dialog.messageLines(), lines);
    EXPECT_EQ(dialog.warningList().messages(), warnings);
    EXPECT_EQ(dialog.warningList().objectName(), QStringLiteral("warnings"));
    EXPECT_EQ(dialog.warningList().parentWidget(), dialog.messageArea());
    EXPECT_FALSE(dialog.iconLabel()->pixmap().isNull());
    ASSERT_EQ(dialog.buttonBox()->buttons().size(), 1);
    EXPECT_TRUE(dialog.buttonBox()->button(QDialogButtonBox::Ok)->isDefault());
}

TEST_F(WarningDialogTest, ShowWarningsShowsTheDialogUntilItIsClosed)
{
    Shown                            shown;
    const Gui::Test::NextModalDialog close(CloseWarnings{.shown = &shown});
    WarningDialog::showWarnings(nullptr, {QStringLiteral("line")}, QStringLiteral("Title"),
                                {QStringLiteral("warning")});
    EXPECT_EQ(shown.title, QStringLiteral("Title"));
    EXPECT_EQ(shown.lines, QStringList{QStringLiteral("line")});
    EXPECT_EQ(shown.warnings, QStringList{QStringLiteral("warning")});
}

}  // namespace

}  // namespace QtRocket::Gui
