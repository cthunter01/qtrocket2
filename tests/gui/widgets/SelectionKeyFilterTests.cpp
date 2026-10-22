// Left and Right with a selection (Java: TextComponentSelectionKeyListener). The expectations are
// Java's rule worked by hand: the caret goes to start + 1 (Left) or end - 1 (Right) without a
// selection, then the key's own action moves it. The offscreen platform has Qt's default (the
// Windows) keyboard scheme, under which a plain QLineEdit moves the cursor from where it is, as
// the bindings scout's probe found (Left with [1,4) selected: 3; Right: 5).

#include "QtRocket/gui/widgets/SelectionKeyFilter.h"

#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QString>
#include <QTest>
#include <QTextCursor>
#include <QTextEdit>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/util/BugError.h"
#include "WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using SelectionKeyFilterTest = Test::GuiTest;
using Test::editorOf;

/// "12345" with [1, 4) selected and the cursor at 4, as in the scout's probe.
void selectMiddle(QLineEdit& edit)
{
    edit.setText(QStringLiteral("12345"));
    edit.setSelection(1, 3);
}

/// Where the cursor of a QLineEdit holding "12345" with [1, 4) selected is after @p key with
/// @p modifiers, with the filter or without.
[[nodiscard]] int cursorAfter(Qt::Key key, Qt::KeyboardModifiers modifiers, bool filtered)
{
    QLineEdit edit;
    if (filtered)
    {
        SelectionKeyFilter::install(edit);
    }
    selectMiddle(edit);
    QTest::keyClick(&edit, key, modifiers);
    return edit.cursorPosition();
}

/// "ab\ncd" in a text area with [1, 4) selected.
template <class TextArea>
void selectAcrossLines(TextArea& area)
{
    area.setPlainText(QStringLiteral("ab\ncd"));
    QTextCursor cursor = area.textCursor();
    cursor.setPosition(1);
    cursor.setPosition(4, QTextCursor::KeepAnchor);
    area.setTextCursor(cursor);
}

TEST_F(SelectionKeyFilterTest, LeftAndRightGoToTheEdgesOfTheSelection)
{
    QLineEdit edit;
    SelectionKeyFilter::install(edit);
    selectMiddle(edit);
    QTest::keyClick(&edit, Qt::Key_Left);
    EXPECT_EQ(edit.cursorPosition(), 1);
    EXPECT_FALSE(edit.hasSelectedText());
    selectMiddle(edit);
    QTest::keyClick(&edit, Qt::Key_Right);
    EXPECT_EQ(edit.cursorPosition(), 4);
    EXPECT_FALSE(edit.hasSelectedText());

    // Without the filter (the scout's probe: 3 and 5).
    EXPECT_EQ(cursorAfter(Qt::Key_Left, Qt::NoModifier, false), 3);
    EXPECT_EQ(cursorAfter(Qt::Key_Right, Qt::NoModifier, false), 5);
}

TEST_F(SelectionKeyFilterTest, TheKeypadsArrowsToo)
{
    // Java: VK_KP_LEFT and VK_KP_RIGHT.
    EXPECT_EQ(cursorAfter(Qt::Key_Left, Qt::KeypadModifier, true), 1);
    EXPECT_EQ(cursorAfter(Qt::Key_Right, Qt::KeypadModifier, true), 4);
}

TEST_F(SelectionKeyFilterTest, ShiftExtendsTheSelectionAsUsual)
{
    EXPECT_EQ(cursorAfter(Qt::Key_Left, Qt::ShiftModifier, true),
              cursorAfter(Qt::Key_Left, Qt::ShiftModifier, false));
    EXPECT_EQ(cursorAfter(Qt::Key_Right, Qt::ShiftModifier, true),
              cursorAfter(Qt::Key_Right, Qt::ShiftModifier, false));
    QLineEdit edit;
    SelectionKeyFilter::install(edit);
    selectMiddle(edit);
    QTest::keyClick(&edit, Qt::Key_Right, Qt::ShiftModifier);
    EXPECT_EQ(edit.selectedText(), QStringLiteral("2345"));
}

TEST_F(SelectionKeyFilterTest, CtrlLeftGoesOnToTheWordBeforeTheSelectionsStart)
{
    // Java: the caret at start + 1, then the previous-word action: "abc def ghi" with "ef g"
    // selected gives 4, the start of "def" (without the filter: 8, the start of "ghi").
    QLineEdit edit;
    SelectionKeyFilter::install(edit);
    edit.setText(QStringLiteral("abc def ghi"));
    edit.setSelection(5, 4);
    QTest::keyClick(&edit, Qt::Key_Left, Qt::ControlModifier);
    EXPECT_EQ(edit.cursorPosition(), 4);
}

TEST_F(SelectionKeyFilterTest, WithoutASelectionTheCursorMovesOneCharacter)
{
    QLineEdit edit;
    SelectionKeyFilter::install(edit);
    edit.setText(QStringLiteral("12345"));
    edit.setCursorPosition(2);
    QTest::keyClick(&edit, Qt::Key_Left);
    EXPECT_EQ(edit.cursorPosition(), 1);
    QTest::keyClick(&edit, Qt::Key_Right);
    QTest::keyClick(&edit, Qt::Key_Right);
    EXPECT_EQ(edit.cursorPosition(), 3);
}

TEST_F(SelectionKeyFilterTest, WorksInTextAreas)
{
    // The comment field (Java: RocketComponentConfig's commentTextArea).
    QPlainTextEdit plain;
    SelectionKeyFilter::install(plain);
    selectAcrossLines(plain);
    QTest::keyClick(&plain, Qt::Key_Left);
    EXPECT_EQ(plain.textCursor().position(), 1);
    EXPECT_FALSE(plain.textCursor().hasSelection());
    selectAcrossLines(plain);
    QTest::keyClick(&plain, Qt::Key_Right);
    EXPECT_EQ(plain.textCursor().position(), 4);

    QTextEdit rich;
    SelectionKeyFilter::install(rich);
    selectAcrossLines(rich);
    QTest::keyClick(&rich, Qt::Key_Left);
    EXPECT_EQ(rich.textCursor().position(), 1);
    selectAcrossLines(rich);
    QTest::keyClick(&rich, Qt::Key_Right);
    EXPECT_EQ(rich.textCursor().position(), 4);
}

TEST_F(SelectionKeyFilterTest, WorksInASpinBoxThroughItsKeys)
{
    // A spin box hands its keys to its line edit: the filter is on the spin box.
    QDoubleSpinBox spin;
    spin.setRange(0, 99999);
    spin.setDecimals(0);
    spin.setValue(12345);
    SelectionKeyFilter::install(spin);
    QLineEdit& edit = *editorOf(spin);
    edit.setSelection(1, 3);
    QTest::keyClick(&spin, Qt::Key_Left);
    EXPECT_EQ(edit.cursorPosition(), 1);
    edit.setSelection(1, 3);
    QTest::keyClick(&spin, Qt::Key_Right);
    EXPECT_EQ(edit.cursorPosition(), 4);
}

TEST_F(SelectionKeyFilterTest, IsOnlyForTextFields)
{
    QLabel label;
    EXPECT_THROW(SelectionKeyFilter::install(label), BugError);
    EXPECT_EQ(label.findChild<SelectionKeyFilter*>(), nullptr);
}

}  // namespace

}  // namespace QtRocket::Gui
