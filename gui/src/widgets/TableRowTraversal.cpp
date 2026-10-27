#include "QtRocket/gui/widgets/TableRowTraversal.h"

#include <optional>

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QCoreApplication>
#include <QEvent>
#include <QHeaderView>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QModelIndex>
#include <QObject>
#include <QPointer>
#include <QTableView>
#include <Qt>

#include "QtRocket/gui/util/MakeChild.h"

namespace QtRocket::Gui
{

namespace
{

/// The modifiers of @p event that choose a binding: the keypad's is not one of them (see the
/// class comment).
[[nodiscard]] Qt::KeyboardModifiers bindingModifiers(const QKeyEvent& event)
{
    return event.modifiers() & ~Qt::KeyboardModifiers(Qt::KeypadModifier);
}

/// The logical index of the first column @p table shows, in its header's order (Java: view
/// column 0, the first column of the JTable's column model, which holds the shown columns
/// only); 0 when it shows none.
[[nodiscard]] int firstShownColumn(const QTableView& table)
{
    const QHeaderView& header = *table.horizontalHeader();
    for (int visual = 0; visual < header.count(); ++visual)
    {
        const int logical = header.logicalIndex(visual);
        if (!header.isSectionHidden(logical))
        {
            return logical;
        }
    }
    return 0;
}

/// The selection of the row an action goes to: that row alone (Java: setSelectionInterval() of
/// the row and the column selection models), as the table selects.
[[nodiscard]] QItemSelectionModel::SelectionFlags selectionFlags(const QTableView& table)
{
    QItemSelectionModel::SelectionFlags flags = QItemSelectionModel::ClearAndSelect;
    switch (table.selectionBehavior())
    {
        case QAbstractItemView::SelectRows:
            flags |= QItemSelectionModel::Rows;
            break;
        case QAbstractItemView::SelectColumns:
            flags |= QItemSelectionModel::Columns;
            break;
        case QAbstractItemView::SelectItems:
            break;
    }
    return flags;
}

}  // namespace

void TableRowTraversal::install(QTableView& table)
{
    if (table.findChild<TableRowTraversal*>(QString(), Qt::FindDirectChildrenOnly) != nullptr)
    {
        return;
    }
    table.installEventFilter(makeChild<TableRowTraversal>(&table));
}

TableRowTraversal::TableRowTraversal(QTableView* parent) : QObject(parent) { }

std::optional<TableRowTraversal::Action> TableRowTraversal::actionFor(const QKeyEvent& event)
{
    const Qt::KeyboardModifiers modifiers = bindingModifiers(event);
    switch (event.key())
    {
        case Qt::Key_Tab:
            if (modifiers == Qt::NoModifier)
            {
                return Action::NEXT_ROW_CYCLE;
            }
            if (modifiers == Qt::ShiftModifier)
            {
                return Action::PREVIOUS_ROW_CYCLE;
            }
            return std::nullopt;
        case Qt::Key_Backtab:
            // Shift+Tab: the platforms send Backtab with Shift; Backtab alone means the same.
            if (modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier)
            {
                return Action::PREVIOUS_ROW_CYCLE;
            }
            return std::nullopt;
        case Qt::Key_Right:
            return modifiers == Qt::NoModifier ? std::optional<Action>(Action::NEXT_ROW)
                                               : std::nullopt;
        case Qt::Key_Left:
            return modifiers == Qt::NoModifier ? std::optional<Action>(Action::PREVIOUS_ROW)
                                               : std::nullopt;
        default:
            return std::nullopt;
    }
}

std::optional<TableRowTraversal::FocusMove> TableRowTraversal::focusMoveFor(const QKeyEvent& event)
{
    const Qt::KeyboardModifiers modifiers = bindingModifiers(event);
    const Qt::KeyboardModifiers control   = modifiers & ~Qt::KeyboardModifiers(Qt::ShiftModifier);
    if (control != Qt::ControlModifier && control != Qt::MetaModifier)
    {
        return std::nullopt;
    }
    const bool shift = modifiers.testFlag(Qt::ShiftModifier);
    switch (event.key())
    {
        case Qt::Key_Tab:
            return shift ? FocusMove::BACKWARD : FocusMove::FORWARD;
        case Qt::Key_Backtab:
            return FocusMove::BACKWARD;
        default:
            return std::nullopt;
    }
}

int TableRowTraversal::selectedRow(const QTableView& table)
{
    const QItemSelectionModel* selection = table.selectionModel();
    if (selection == nullptr)
    {
        return -1;
    }
    int row = -1;
    for (const QItemSelectionRange& range : selection->selection())
    {
        if (range.parent() == table.rootIndex() && (row < 0 || range.top() < row))
        {
            row = range.top();
        }
    }
    return row;
}

void TableRowTraversal::perform(QTableView& table, Action action)
{
    const QAbstractItemModel* model     = table.model();
    QItemSelectionModel*      selection = table.selectionModel();
    if (model == nullptr || selection == nullptr)
    {
        return;
    }
    const bool next  = action == Action::NEXT_ROW || action == Action::NEXT_ROW_CYCLE;
    const bool cycle = action == Action::NEXT_ROW_CYCLE || action == Action::PREVIOUS_ROW_CYCLE;
    const int  rows  = model->rowCount(table.rootIndex());
    int        row   = selectedRow(table) + (next ? 1 : -1);
    if (next ? row >= rows : row < 0)
    {
        if (!cycle)
        {
            return;
        }
        row = next ? 0 : rows - 1;
    }
    const QModelIndex index = model->index(row, firstShownColumn(table), table.rootIndex());
    if (!index.isValid())
    {
        return;  // no rows (or no columns): Java's phantom selection is not made
    }
    selection->setCurrentIndex(index, selectionFlags(table));
}

void TableRowTraversal::moveFocus(QTableView& table, FocusMove direction)
{
    // The table's own Tab moves from cell to cell (tabKeyNavigation); without it, QWidget::event()
    // gives the Tab to focusNextPrevChild(), which moves the focus along the focus chain. The
    // filter, the table's child, is gone afterwards when a slot of the focus change deleted the
    // table.
    const QPointer<TableRowTraversal> self(this);
    const bool                        navigation = table.tabKeyNavigation();
    const bool                        forward    = direction == FocusMove::FORWARD;
    m_passing                                    = true;
    table.setTabKeyNavigation(false);
    QKeyEvent tab(QEvent::KeyPress, forward ? Qt::Key_Tab : Qt::Key_Backtab,
                  forward ? Qt::NoModifier : Qt::ShiftModifier);
    QCoreApplication::sendEvent(&table, &tab);
    if (!self.isNull())
    {
        m_passing = false;
        table.setTabKeyNavigation(navigation);
    }
}

bool TableRowTraversal::eventFilter(QObject* watched, QEvent* event)
{
    auto* const       table = qobject_cast<QTableView*>(watched);
    const auto* const key   = dynamic_cast<const QKeyEvent*>(event);
    if (m_passing || table == nullptr || watched != parent() || key == nullptr ||
        (event->type() != QEvent::KeyPress && event->type() != QEvent::ShortcutOverride))
    {
        return QObject::eventFilter(watched, event);
    }
    const std::optional<Action>    action    = actionFor(*key);
    const std::optional<FocusMove> focusMove = focusMoveFor(*key);
    if (!action.has_value() && !focusMove.has_value())
    {
        return QObject::eventFilter(watched, event);
    }
    event->accept();  // a ShortcutOverride: the key is the table's, as a key press
    if (event->type() == QEvent::KeyPress)
    {
        if (action.has_value())
        {
            perform(*table, *action);
        }
        else
        {
            moveFocus(*table, *focusMove);
        }
    }
    return true;
}

}  // namespace QtRocket::Gui
