#include "QtRocket/gui/dialogs/DetailDialog.h"

#include <optional>

#include <QAbstractButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QLabel>
#include <QObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTextCursor>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/gui/util/UserPrompts.h"

namespace QtRocket::Gui
{

namespace
{

using DetailDialogTest = Test::GuiTest;
using Severity         = UserPrompts::Severity;

/// Checks that the modal dialog is a DetailDialog titled "Memory statistics" and closes it with
/// OK.
struct CloseWithOk
{
    bool* checked;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<DetailDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        *checked = dialog->windowTitle() == QStringLiteral("Memory statistics") &&
                   dialog->messageLines() == QStringList{QStringLiteral("Memory usage:")};
        dialog->buttonBox()->button(QDialogButtonBox::Ok)->click();
    }
};

TEST_F(DetailDialogTest, ShowsTheMessageLinesAboveTheDetails)
{
    const QString      details = QStringLiteral("Age 12 ms: first\nAge 15 ms: second\nTotal: 2");
    const DetailDialog dialog(
        {QStringLiteral("Memory usage:"), QStringLiteral("<html><b>Max</b> memory")}, details,
        QStringLiteral("Memory statistics"), Severity::INFORMATION);
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Memory statistics"));
    EXPECT_EQ(dialog.messageLines(), (QStringList{QStringLiteral("Memory usage:"),
                                                  QStringLiteral("<html><b>Max</b> memory")}));
    // Swing's rule: a line is HTML when it starts with "<html>".
    ASSERT_EQ(dialog.messageLabels().size(), 2);
    EXPECT_EQ(dialog.messageLabels().at(0)->textFormat(), Qt::PlainText);
    EXPECT_EQ(dialog.messageLabels().at(1)->textFormat(), Qt::RichText);
    EXPECT_FALSE(dialog.iconLabel()->pixmap().isNull());

    const QPlainTextEdit* const view = dialog.detailsView();
    ASSERT_NE(view, nullptr);
    EXPECT_EQ(view->toPlainText(), details);
    EXPECT_TRUE(view->isReadOnly());
    EXPECT_TRUE(view->textInteractionFlags().testFlag(Qt::TextSelectableByKeyboard));
    EXPECT_EQ(view->lineWrapMode(), QPlainTextEdit::NoWrap);
    EXPECT_EQ(view->textCursor().position(), 0);
    // JTextArea(5, 40) in a font 2 points smaller than the dialog's.
    EXPECT_DOUBLE_EQ(view->font().pointSizeF(),
                     dialog.font().pointSizeF() - DetailDialog::kDetailFontDecrease);
    const QFontMetrics metrics(view->font());
    EXPECT_GE(view->minimumHeight(), DetailDialog::kDetailRows * metrics.lineSpacing());
    EXPECT_GE(view->minimumWidth(), DetailDialog::kDetailColumns * metrics.horizontalAdvance(u'm'));

    ASSERT_NE(dialog.buttonBox()->button(QDialogButtonBox::Ok), nullptr);
    EXPECT_EQ(dialog.buttonBox()->buttons().size(), 1);
}

TEST_F(DetailDialogTest, WithoutDetailsItIsAPlainMessage)
{
    const DetailDialog dialog({QStringLiteral("An error occurred during the simulation:")},
                              std::nullopt, QStringLiteral("Simulation 1"), Severity::ERROR);
    EXPECT_EQ(dialog.detailsView(), nullptr);
    EXPECT_EQ(dialog.findChild<QPlainTextEdit*>(), nullptr);
    EXPECT_EQ(dialog.messageLines().size(), 1);
}

TEST_F(DetailDialogTest, OkClosesTheDialog)
{
    const DetailDialog dialog({QStringLiteral("Memory usage:")}, std::nullopt,
                              QStringLiteral("Memory statistics"), Severity::WARNING);
    dialog.buttonBox()->button(QDialogButtonBox::Ok)->click();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

TEST_F(DetailDialogTest, ShowDetailedMessageDialogReturnsWhenTheDialogIsClosed)
{
    bool                             checked = false;
    const Gui::Test::NextModalDialog answer(CloseWithOk{.checked = &checked});
    DetailDialog::showDetailedMessageDialog(
        nullptr, {QStringLiteral("Memory usage:")}, QStringLiteral("Total: 0"),
        QStringLiteral("Memory statistics"), Severity::INFORMATION);
    EXPECT_TRUE(answer.acted());
    EXPECT_TRUE(checked);
}

}  // namespace

}  // namespace QtRocket::Gui
