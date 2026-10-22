#include "QtRocket/gui/dialogs/OptionDialog.h"

#include <QAbstractButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/UserPrompts.h"

namespace QtRocket::Gui
{

namespace
{

using OptionDialogTest = Test::GuiTest;
using MessageType      = OptionDialog::MessageType;

/// Clicks the option of the modal dialog whose text is @p text.
struct ClickOption
{
    QString text;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<OptionDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        for (QAbstractButton* const option : dialog->buttonBox()->buttons())
        {
            if (option->text() == text)
            {
                option->click();
                return;
            }
        }
        ADD_FAILURE() << "no option " << text.toStdString();
        dialog->reject();
    }
};

/// Presses @p key in the modal dialog.
struct PressKey
{
    Qt::Key key;

    void operator()(QWidget& widget) const { QTest::keyClick(&widget, key); }
};

TEST_F(OptionDialogTest, ShowsTheIconTheMessageLinesAndTheOptions)
{
    OptionDialog dialog(MessageType::WARNING, QStringLiteral("Cancel operation"));
    dialog.addMessageLines(
        {QStringLiteral("Plain <b>text</b>"), QStringLiteral("<html>Rich <b>text</b></html>")});
    auto* const check = makeChild<QWidget>(dialog.messageArea());
    dialog.addMessageWidget(check);
    QPushButton* const yes = dialog.addOption(QDialogButtonBox::Yes);
    dialog.addOption(QStringLiteral("Later"), QDialogButtonBox::RejectRole);
    dialog.setInitialOption(yes);

    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Cancel operation"));
    EXPECT_EQ(dialog.messageLines(),
              (QStringList{QStringLiteral("Plain <b>text</b>"),
                           QStringLiteral("<html>Rich <b>text</b></html>")}));
    // Swing's rule: a line is HTML when it starts with "<html>".
    ASSERT_EQ(dialog.messageLabels().size(), 2);
    EXPECT_EQ(dialog.messageLabels().at(0)->textFormat(), Qt::PlainText);
    EXPECT_EQ(dialog.messageLabels().at(1)->textFormat(), Qt::RichText);
    EXPECT_EQ(check->parentWidget(), dialog.messageArea());
    EXPECT_FALSE(dialog.iconLabel()->pixmap().isNull());
    EXPECT_FALSE(dialog.iconLabel()->isHidden());
    EXPECT_EQ(dialog.buttonBox()->buttons().size(), 2);
    EXPECT_TRUE(yes->isDefault());
    EXPECT_EQ(dialog.chosenOption(), nullptr);
}

TEST_F(OptionDialogTest, APlainMessageHasNoIcon)
{
    const OptionDialog dialog(MessageType::PLAIN, QStringLiteral("Plain"));
    EXPECT_TRUE(dialog.iconLabel()->pixmap().isNull());
    EXPECT_TRUE(dialog.iconLabel()->isHidden());
}

TEST_F(OptionDialogTest, TheSeveritiesAreJOptionPanesMessageTypes)
{
    EXPECT_EQ(OptionDialog::messageType(UserPrompts::Severity::INFORMATION),
              MessageType::INFORMATION);
    EXPECT_EQ(OptionDialog::messageType(UserPrompts::Severity::WARNING), MessageType::WARNING);
    EXPECT_EQ(OptionDialog::messageType(UserPrompts::Severity::ERROR), MessageType::ERROR);
    EXPECT_TRUE(OptionDialog::isHtml(QStringLiteral("<HTML>x")));
    EXPECT_FALSE(OptionDialog::isHtml(QStringLiteral(" <html>x")));
}

TEST_F(OptionDialogTest, AnOptionClosesTheDialogChosen)
{
    OptionDialog             dialog(MessageType::QUESTION, QStringLiteral("File exists"));
    QPushButton* const       yes = dialog.addOption(QDialogButtonBox::Yes);
    const QPushButton* const no  = dialog.addOption(QDialogButtonBox::No);
    dialog.setInitialOption(yes);
    {
        const Gui::Test::NextModalDialog answer(ClickOption{.text = no->text()});
        EXPECT_EQ(dialog.exec(), QDialog::Accepted);
    }
    EXPECT_EQ(dialog.chosenOption(), no);
}

TEST_F(OptionDialogTest, EnterChoosesTheInitialOption)
{
    OptionDialog       dialog(MessageType::QUESTION, QStringLiteral("File exists"));
    QPushButton* const yes = dialog.addOption(QDialogButtonBox::Yes);
    dialog.addOption(QDialogButtonBox::No);
    dialog.setInitialOption(yes);
    {
        const Gui::Test::NextModalDialog answer(PressKey{.key = Qt::Key_Return});
        dialog.exec();
    }
    EXPECT_EQ(dialog.chosenOption(), yes);
}

TEST_F(OptionDialogTest, EscapeClosesTheDialogWithoutAnOption)
{
    // JOptionPane.CLOSED_OPTION.
    OptionDialog       dialog(MessageType::QUESTION, QStringLiteral("File exists"));
    QPushButton* const yes = dialog.addOption(QDialogButtonBox::Yes);
    dialog.addOption(QDialogButtonBox::No);
    dialog.setInitialOption(yes);
    {
        const Gui::Test::NextModalDialog answer(PressKey{.key = Qt::Key_Escape});
        EXPECT_EQ(dialog.exec(), QDialog::Rejected);
    }
    EXPECT_EQ(dialog.chosenOption(), nullptr);
}

}  // namespace

}  // namespace QtRocket::Gui
