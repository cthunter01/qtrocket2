#pragma once

#include <functional>
#include <vector>

#include <QObject>
#include <QString>

class QEvent;
class QWidget;

namespace QtRocket::Gui
{

/// The clipboard keys of a table of the "Motors & Configuration" tab, as a Swing JTable has them:
/// its look and feel binds "cut", "copy" and "paste" in every table's map of the keys of a
/// focused table (WHEN_ANCESTOR_OF_FOCUSED_COMPONENT), which Swing asks before the menu bar's
/// keys. So while the table has the keyboard focus, Cut, Copy and Paste (QKeySequence::Cut, Copy
/// and Paste: Ctrl+X, C and V, Cmd on macOS) are the table's, and the window's Edit items of
/// those keys (RocketActions' Cut, Copy and Paste of the design's selection, which would then show
/// the design tab) do not act. Duplicate, Edit and Delete reach the window's items, as in Java.
///
/// - Copy puts the values of the selected cells on the clipboard as a JTable's TransferHandler
///   exports them: as plain text (plainText()), each selected row's values in column order
///   separated by tabs and the rows by newlines, and as HTML (html()), a table of them; nothing
///   when no cell is selected.
/// - Cut and Paste do nothing: a JTable's export allows a copy only, so a cut moves nothing, and
///   it takes nothing in.
/// The values are given by the table's owner (Java's getValueAt(row, column).toString() of the
/// selected rows x columns).
///
/// Pinned by the M3 run 2 probe TableKeysProbe (a JTable of OpenRocket's FlatLaf look in a frame
/// whose menu has OpenRocket's keys, driven by java.awt.Robot): the table takes Ctrl+X, C and V,
/// and the menu items of those keys do not act; Ctrl+D, Ctrl+E and Delete reach the menu; Ctrl+C
/// copies "r0c0\tr0c1\nr1c0\tr1c1" (and the HTML) for two rows and columns, the values not
/// escaped, and nothing without a selection or in a table that allows no selection; Ctrl+X and V
/// leave the clipboard alone.
///
/// It is the table's child and its event filter.
class TableTransferKeys final : public QObject
{
public:
    /// The values of the selected cells, row by row in row order, each row's in column order;
    /// none without a selection.
    using Cells = std::vector<std::vector<QString>>;
    /// What gives the values of the selected cells.
    using CellSource = std::function<Cells()>;

    /// The keys of @p table (not null), whose selected cells' values @p cells gives (an empty
    /// function: none, as a table without a selection has).
    TableTransferKeys(CellSource cells, QWidget* table);

    /// Takes Cut, Copy and Paste of the table (see the class comment).
    bool eventFilter(QObject* watched, QEvent* event) override;

    /// The plain text of @p cells: each row's values separated by tabs, the rows by newlines, none
    /// at the end.
    [[nodiscard]] static QString plainText(const Cells& cells);

    /// The HTML of @p cells: "<html>\n<body>\n<table>\n", for each row "<tr>\n", "  <td>" value
    /// "</td>\n" for each value and "</tr>\n", then "</table>\n</body>\n</html>"; the values as
    /// they are (Java escapes nothing).
    [[nodiscard]] static QString html(const Cells& cells);

private:
    /// Copy: the selected cells' values on the clipboard, when there are any.
    void copy() const;

    CellSource m_cells;
};

}  // namespace QtRocket::Gui
