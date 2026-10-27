// TableTransferKeys: a JTable's Cut, Copy and Paste keys in a table of the "Motors & Configuration"
// tab, against the window's shortcuts of the same keys, and the text a copy puts on the clipboard
// (the M3 run 2 probe TableKeysProbe: a JTable of OpenRocket's FlatLaf look in a frame whose menu
// has OpenRocket's keys, driven by java.awt.Robot).

#include "QtRocket/gui/flightconfig/TableTransferKeys.h"

#include <memory>
#include <string>
#include <utility>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QKeySequence>
#include <QList>
#include <QMimeData>
#include <QSignalSpy>
#include <QString>
#include <QTableView>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using TableTransferKeysTest = Gui::Test::GuiTest;
using Cells                 = TableTransferKeys::Cells;

/// The probe's table of two rows and three columns, its first value with HTML's special
/// characters.
[[nodiscard]] Cells probeCells()
{
    return {{QStringLiteral("a<b&c \"d\""), QStringLiteral("r0c1"), QStringLiteral("r0c2")},
            {QStringLiteral("r1c0"), QStringLiteral("r1c1"), QStringLiteral("r1c2")}};
}

/// A shown, active window holding a table with the keys of @p cells, and the window's shortcuts
/// of Cut, Copy, Paste, Duplicate (Ctrl+D) and Delete, each counting its triggers.
class KeysWindow
{
public:
    explicit KeysWindow(TableTransferKeys::CellSource cells)
      : m_window(std::make_unique<QWidget>()),
        m_table(makeChild<QTableView>(m_window.get())),
        m_cut(shortcut(QKeySequence::Cut)),
        m_copy(shortcut(QKeySequence::Copy)),
        m_paste(shortcut(QKeySequence::Paste)),
        m_duplicate(shortcut(QKeySequence(Qt::ControlModifier | Qt::Key_D))),
        m_delete(shortcut(QKeySequence(Qt::Key_Delete)))
    {
        m_window->setObjectName(QStringLiteral("tableTransferKeysWindow"));
        auto* layout = makeChild<QVBoxLayout>(m_window.get());
        layout->addWidget(m_table);
        m_window->show();
        // The keys belong to the table, their Qt parent, which the analyzer does not see.
        static_cast<void>(makeChild<TableTransferKeys>(m_table, std::move(cells)));
        // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks): the table owns the keys
        m_window->activateWindow();
    }

    /// Whether the window is active and the table has the focus.
    [[nodiscard]] bool focusTable() const
    {
        if (!QTest::qWaitForWindowActive(m_window.get()))
        {
            return false;
        }
        m_table->setFocus();
        return QApplication::focusWidget() == m_table;
    }

    /// Presses @p key with Ctrl (Cmd on macOS), or alone, in the table.
    void press(Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::ControlModifier) const
    {
        QTest::keyClick(m_table, key, modifiers);
    }

    [[nodiscard]] QAction& cut() const noexcept { return *m_cut; }
    [[nodiscard]] QAction& copy() const noexcept { return *m_copy; }
    [[nodiscard]] QAction& paste() const noexcept { return *m_paste; }
    [[nodiscard]] QAction& duplicate() const noexcept { return *m_duplicate; }
    [[nodiscard]] QAction& remove() const noexcept { return *m_delete; }

private:
    /// A window shortcut of @p keys.
    [[nodiscard]] QAction* shortcut(const QKeySequence& keys) const
    {
        auto* action = makeChild<QAction>(m_window.get());
        action->setShortcuts(QList<QKeySequence>{keys});
        action->setShortcutContext(Qt::WindowShortcut);
        m_window->addAction(action);
        return action;
    }

    std::unique_ptr<QWidget> m_window;
    QTableView*              m_table;
    QAction*                 m_cut;
    QAction*                 m_copy;
    QAction*                 m_paste;
    QAction*                 m_duplicate;
    QAction*                 m_delete;
};

/// The clipboard's plain text.
[[nodiscard]] QString clipboardText()
{
    return QGuiApplication::clipboard()->text();
}

TEST_F(TableTransferKeysTest, TheTextsAreThoseOfAJTablesExport)
{
    // TableKeysProbe's copy of two rows and three columns: tabs between the values, newlines
    // between the rows, none at the end; the HTML as Java writes it, nothing escaped.
    EXPECT_EQ(TableTransferKeys::plainText(probeCells()),
              QStringLiteral("a<b&c \"d\"\tr0c1\tr0c2\nr1c0\tr1c1\tr1c2"));
    EXPECT_EQ(TableTransferKeys::html(probeCells()),
              QStringLiteral("<html>\n<body>\n<table>\n<tr>\n  <td>a<b&c \"d\"</td>\n  "
                             "<td>r0c1</td>\n  <td>r0c2</td>\n</tr>\n<tr>\n  <td>r1c0</td>\n  "
                             "<td>r1c1</td>\n  <td>r1c2</td>\n</tr>\n</table>\n</body>\n</html>"));
    // One cell (TableKeysProbe's first run: "r1c1").
    EXPECT_EQ(TableTransferKeys::plainText({{QStringLiteral("r1c1")}}), QStringLiteral("r1c1"));
    EXPECT_EQ(TableTransferKeys::plainText({}), QString());
}

TEST_F(TableTransferKeysTest, TheTableTakesCutCopyAndPasteAndTheWindowTheOtherKeys)
{
    const KeysWindow window{TableTransferKeys::CellSource(probeCells)};
    ASSERT_TRUE(window.focusTable());
    const QSignalSpy cut(&window.cut(), &QAction::triggered);
    const QSignalSpy copy(&window.copy(), &QAction::triggered);
    const QSignalSpy paste(&window.paste(), &QAction::triggered);
    const QSignalSpy duplicate(&window.duplicate(), &QAction::triggered);
    const QSignalSpy remove(&window.remove(), &QAction::triggered);

    // Copy: the selected cells on the clipboard, as text and as HTML; the window's Copy does not
    // act.
    window.press(Qt::Key_C);
    EXPECT_EQ(clipboardText(), TableTransferKeys::plainText(probeCells()));
    const QMimeData* data = QGuiApplication::clipboard()->mimeData();
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->html(), TableTransferKeys::html(probeCells()));

    // Cut and Paste do nothing, and leave the clipboard alone.
    QGuiApplication::clipboard()->setText(QStringLiteral("before"));
    window.press(Qt::Key_X);
    window.press(Qt::Key_V);
    EXPECT_EQ(clipboardText(), QStringLiteral("before"));

    // Duplicate and Delete are the window's, as in Java.
    window.press(Qt::Key_D);
    window.press(Qt::Key_Delete, Qt::NoModifier);
    EXPECT_EQ(cut.count(), 0);
    EXPECT_EQ(copy.count(), 0);
    EXPECT_EQ(paste.count(), 0);
    EXPECT_EQ(duplicate.count(), 1);
    EXPECT_EQ(remove.count(), 1);
    QGuiApplication::clipboard()->clear();
}

/// No cells to copy.
[[nodiscard]] Cells noCells()
{
    return {};
}

/// What Copy does in a table of the keys of @p cells over a clipboard holding "before": "<the
/// clipboard's text then> <the number of the window's Copy triggers>"; "inactive" when the table
/// could not have the focus.
[[nodiscard]] std::string copyWith(const TableTransferKeys::CellSource& cells)
{
    const KeysWindow window(cells);
    if (!window.focusTable())
    {
        return "inactive";
    }
    const QSignalSpy copy(&window.copy(), &QAction::triggered);
    QGuiApplication::clipboard()->setText(QStringLiteral("before"));
    window.press(Qt::Key_C);
    std::string text = QtStrings::toUtf8(clipboardText()) + " " + std::to_string(copy.count());
    QGuiApplication::clipboard()->clear();
    return text;
}

TEST_F(TableTransferKeysTest, NothingSelectedCopiesNothingAndStillTakesTheKey)
{
    // TableKeysProbe: no selection, or a table that allows none, exports nothing.
    EXPECT_EQ(copyWith(TableTransferKeys::CellSource(noCells)), "before 0");
    EXPECT_EQ(copyWith(TableTransferKeys::CellSource()), "before 0");
}

TEST_F(TableTransferKeysTest, ATableIsNeeded)
{
    EXPECT_THROW(static_cast<void>(TableTransferKeys(TableTransferKeys::CellSource(), nullptr)),
                 BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
