#include "QtRocket/gui/simulation/SimulationTableView.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include <QAbstractItemDelegate>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QItemSelectionRange>
#include <QKeyEvent>
#include <QKeySequence>
#include <QModelIndex>
#include <QMouseEvent>
#include <QObject>
#include <QPoint>
#include <QScopedValueRollback>
#include <QString>
#include <QStyleOptionViewItem>
#include <QTableView>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "QtRocket/gui/simulation/SimulationWarningsDelegate.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/TableRowTraversal.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// What the view's slots report a failure as.
[[nodiscard]] QString context()
{
    return QStringLiteral("Simulations table view");
}

/// The narrowest @p column can be made: an auto-sized column's default width (ColumnTableModel
/// gives it as its minimum), the status column's exact width, Swing's minimum for the others.
[[nodiscard]] int columnMinimumWidth(SimulationColumns::Column column)
{
    if (SimulationColumns::exactWidth(column) > 0)
    {
        return SimulationColumns::exactWidth(column);
    }
    if (SimulationColumns::isAutoSize(column))
    {
        return SimulationColumns::defaultWidth(column);
    }
    return SimulationTableView::kMinimumColumnWidth;
}

/// Whether @p event is one of the window's keys (see the class comment): Copy, Cut and Paste,
/// Ctrl+E, Ctrl+D, Ctrl+Delete and Delete (Cmd on macOS, Qt's ControlModifier there too).
[[nodiscard]] bool isWindowKey(const QKeyEvent& event)
{
    if (event.matches(QKeySequence::Copy) || event.matches(QKeySequence::Cut) ||
        event.matches(QKeySequence::Paste))
    {
        return true;
    }
    const Qt::KeyboardModifiers modifiers = event.modifiers() & ~Qt::KeypadModifier;
    const int                   key       = event.key();
    if (modifiers == Qt::ControlModifier &&
        (key == Qt::Key_E || key == Qt::Key_D || key == Qt::Key_Delete))
    {
        return true;
    }
    return modifiers == Qt::NoModifier && key == Qt::Key_Delete;
}

/// One step of Enter (@p forward) or Shift+Enter from the cell at @p row and the shown column
/// @p column: down the column (up), from the row @p bottom to the row @p top of the next shown
/// column (back), from the column @p last to the first (back).
void step(int& row, int& column, bool forward, int top, int bottom, int last)
{
    if (forward)
    {
        ++row;
        if (row > bottom)
        {
            row    = top;
            column = column >= last ? 0 : column + 1;
        }
        return;
    }
    --row;
    if (row < top)
    {
        row    = bottom;
        column = column <= 0 ? last : column - 1;
    }
}

}  // namespace

SimulationTableView::SimulationTableView(SimulationTableModel& model, QWidget* parent)
  : QTableView(parent), m_model(&model)
{
    setObjectName(QStringLiteral("simulationTable"));
    QTableView::setModel(&model);
    // JTable: rows in several intervals (MULTIPLE_INTERVAL_SELECTION), whole rows, nothing
    // editable, no row header.
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    verticalHeader()->hide();

    // AUTO_RESIZE_OFF: the columns keep their widths, the table scrolls sideways; columns move.
    QHeaderView& header = *horizontalHeader();
    header.setStretchLastSection(false);
    header.setSectionsMovable(true);
    header.setHighlightSections(false);
    header.setMinimumSectionSize(kMinimumColumnWidth);
    header.setSectionResizeMode(QHeaderView::Interactive);
    model.connectHeader(header);

    setItemDelegateForColumn(SimulationColumns::indexOf(Column::WARNINGS),
                             makeChild<SimulationWarningsDelegate>(this));
    setColumnWidths();
    setupAutoSizeColumns();

    // The normal left/right and tab/shift-tab key action traverses each cell/column of the table
    // instead of going to the next row (OpenRocket's comment).
    TableRowTraversal::install(*this);

    connect(&header, &QHeaderView::sectionResized, this,
            guarded([this](int section) { keepMinimumWidth(section); }, context()));
    // The selection model was made by setModel(): it heard of a layout change before the view.
    connect(selectionModel(), &QItemSelectionModel::selectionChanged, this,
            guarded([this] { selectionModelChanged(); }, context()));
    connect(&model, &QAbstractItemModel::layoutAboutToBeChanged, this,
            guarded([this] { rememberRows(); }, context()));
    connect(&model, &QAbstractItemModel::layoutChanged, this,
            guarded([this] { layoutChangedByModel(); }, context()));
}

SimulationTableView::~SimulationTableView() = default;

void SimulationTableView::setModel(QAbstractItemModel* /*model*/)
{
    bug("SimulationTableView shows the model it was made with only");
}

// ---------------------------------------------------------------------------------- the columns

std::optional<SimulationColumns::Column> SimulationTableView::columnAtPoint(
    const QPoint& position) const
{
    const int section = columnAt(position.x());
    if (section < 0)
    {
        return std::nullopt;
    }
    return SimulationColumns::columnAt(section);
}

std::vector<SimulationColumns::Column> SimulationTableView::visibleColumns() const
{
    std::vector<Column> columns;
    for (const int section : visibleSections())
    {
        columns.push_back(SimulationColumns::columnAt(section));
    }
    return columns;
}

bool SimulationTableView::isColumnVisible(Column column) const
{
    return !horizontalHeader()->isSectionHidden(SimulationColumns::indexOf(column));
}

int SimulationTableView::viewColumn(Column column) const
{
    const std::vector<int> sections = visibleSections();
    const auto             found = std::ranges::find(sections, SimulationColumns::indexOf(column));
    return found == sections.end() ? -1 : static_cast<int>(found - sections.begin());
}

std::vector<int> SimulationTableView::visibleSections() const
{
    const QHeaderView& header = *horizontalHeader();
    std::vector<int>   sections;
    for (int visual = 0; visual < header.count(); ++visual)
    {
        const int section = header.logicalIndex(visual);
        if (!header.isSectionHidden(section))
        {
            sections.push_back(section);
        }
    }
    return sections;
}

void SimulationTableView::setColumnWidths()
{
    QHeaderView& header = *horizontalHeader();
    for (const Column column : SimulationColumns::kAllColumns)
    {
        const int section = SimulationColumns::indexOf(column);
        if (SimulationColumns::exactWidth(column) > 0)
        {
            // Its minimum, maximum and preferred width, and not resizable.
            header.setSectionResizeMode(section, QHeaderView::Fixed);
            header.resizeSection(section, SimulationColumns::exactWidth(column));
        }
        else
        {
            header.resizeSection(section, SimulationColumns::defaultWidth(column));
        }
    }
}

void SimulationTableView::setupAutoSizeColumns()
{
    QHeaderView& header = *horizontalHeader();
    for (const Column column : SimulationColumns::kAllColumns)
    {
        if (!SimulationColumns::isAutoSize(column))
        {
            continue;
        }
        const int section = SimulationColumns::indexOf(column);
        int       widest  = 0;
        if (SimulationColumns::includesHeaderInAutoSize(column))
        {
            widest = header.sectionSizeHint(section);
        }
        const QAbstractItemDelegate* delegate = itemDelegateForColumn(section);
        if (delegate == nullptr)
        {
            delegate = itemDelegate();
        }
        for (int row = 0; row < m_model->rowCount(); ++row)
        {
            QStyleOptionViewItem option;
            initViewItemOption(&option);
            widest =
                std::max(widest, delegate->sizeHint(option, m_model->index(row, section)).width());
        }
        header.resizeSection(section, std::max(widest + SimulationColumns::kAutoSizePadding,
                                               columnMinimumWidth(column)));
    }
}

void SimulationTableView::keepMinimumWidth(int section)
{
    QHeaderView& header = *horizontalHeader();
    if (m_resizing || section < 0 || section >= header.count() || header.isSectionHidden(section))
    {
        return;
    }
    const int minimum = columnMinimumWidth(SimulationColumns::columnAt(section));
    if (header.sectionSize(section) < minimum)
    {
        const QScopedValueRollback resizing(m_resizing, true);
        header.resizeSection(section, minimum);
    }
}

// ---------------------------------------------------------------------------------- the selection

std::vector<int> SimulationTableView::selectedRows() const
{
    std::set<int> rows;
    for (const QItemSelectionRange& range : selectionModel()->selection())
    {
        if (range.parent() != rootIndex())
        {
            continue;
        }
        for (int row = range.top(); row <= range.bottom(); ++row)
        {
            rows.insert(row);
        }
    }
    return {rows.begin(), rows.end()};
}

std::vector<std::shared_ptr<Simulation>> SimulationTableView::selectedSimulations() const
{
    std::vector<std::shared_ptr<Simulation>> simulations;
    for (const int row : selectedRows())
    {
        simulations.push_back(m_model->simulation(row));
    }
    return simulations;
}

std::shared_ptr<Simulation> SimulationTableView::firstSelectedSimulation() const
{
    const int row = TableRowTraversal::selectedRow(*this);
    return row < 0 ? nullptr : m_model->simulation(row);
}

void SimulationTableView::selectSimulations(
    const std::vector<std::shared_ptr<Simulation>>& simulations)
{
    QItemSelection selection;
    int            last    = -1;
    const int      columns = m_model->columnCount();
    for (const std::shared_ptr<Simulation>& simulation : simulations)
    {
        const int row = simulation == nullptr ? -1 : m_model->rowOf(*simulation);
        if (row < 0)
        {
            continue;
        }
        selection.select(m_model->index(row, 0), m_model->index(row, columns - 1));
        last = row;
    }
    if (last < 0)
    {
        clearSelection();
        return;
    }
    selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
    selectionModel()->setCurrentIndex(m_model->index(last, leadSection(currentSection())),
                                      QItemSelectionModel::NoUpdate);
}

std::vector<std::shared_ptr<Simulation>> SimulationTableView::selectionByIdentity() const
{
    std::vector<std::shared_ptr<Simulation>> simulations = selectedSimulations();
    std::ranges::sort(simulations, [](const std::shared_ptr<Simulation>& a,
                                      const std::shared_ptr<Simulation>& b) {
        return std::less<const Simulation*>{}(a.get(), b.get());
    });
    return simulations;
}

void SimulationTableView::selectionModelChanged()
{
    ++m_selectionChanges;
    Q_EMIT selectedSimulationsChanged();
}

void SimulationTableView::rememberRows()
{
    const QModelIndex current = currentIndex();
    m_before                  = Before{.simulations      = m_model->simulations(),
                                       .selectedRows     = selectedRows(),
                                       .currentColumn    = current.isValid() ? current.column() : -1,
                                       .selected         = selectionByIdentity(),
                                       .selectionChanges = m_selectionChanges};
}

void SimulationTableView::layoutChangedByModel()
{
    const std::optional<Before> before = std::exchange(m_before, std::nullopt);
    if (!before.has_value())
    {
        return;
    }
    reselectReplacedRows(*before);
    // The selection model tells of no row a layout change takes away (the simulations of the
    // rows before are still held: no address was given to another simulation meanwhile).
    if (m_selectionChanges == before->selectionChanges && selectionByIdentity() != before->selected)
    {
        selectionModelChanged();
    }
}

void SimulationTableView::reselectReplacedRows(const Before& before)
{
    if (before.selectedRows.empty() || !selectionModel()->selection().isEmpty())
    {
        return;
    }
    // Only when the list was replaced as a whole (an undo or a redo that changed the number of
    // simulations): a simulation still in the table keeps its own selection.
    for (const std::shared_ptr<Simulation>& simulation : before.simulations)
    {
        if (m_model->rowOf(*simulation) >= 0)
        {
            return;
        }
    }
    // OpenRocket's fireMaintainSelection(): the same rows, up to the first that is no more.
    QItemSelection selection;
    int            last    = -1;
    const int      columns = m_model->columnCount();
    for (const int row : before.selectedRows)
    {
        if (row >= m_model->rowCount())
        {
            break;
        }
        selection.select(m_model->index(row, 0), m_model->index(row, columns - 1));
        last = row;
    }
    if (last < 0)
    {
        return;
    }
    selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
    selectionModel()->setCurrentIndex(m_model->index(last, leadSection(before.currentColumn)),
                                      QItemSelectionModel::NoUpdate);
}

// ---------------------------------------------------------------------------------- the mouse

void SimulationTableView::mousePressEvent(QMouseEvent* event)
{
    m_press = Press{.position = event->position().toPoint(), .button = event->button()};
    if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton)
    {
        // A JTable selects with the left button only (and Qt's views with the right one too:
        // see the class comment); the middle and the other buttons select nothing.
        event->accept();
        return;
    }
    QTableView::mousePressEvent(event);
}

void SimulationTableView::mouseDoubleClickEvent(QMouseEvent* event)
{
    // After QTableView's, which handles a double click away from the first click's cell as a
    // press (mousePressEvent()).
    QTableView::mouseDoubleClickEvent(event);
    m_press =
        Press{.position = event->position().toPoint(), .button = event->button(), .second = true};
}

void SimulationTableView::mouseReleaseEvent(QMouseEvent* event)
{
    QTableView::mouseReleaseEvent(event);
    const std::optional<Press> press    = std::exchange(m_press, std::nullopt);
    const QPoint               position = event->position().toPoint();
    // A click: the press of the same button at the same place (AWT's MOUSE_CLICKED; a drag
    // over the rows is none).
    if (!press.has_value() || press->button != event->button() ||
        (position - press->position).manhattanLength() >= QApplication::startDragDistance())
    {
        return;
    }
    clicked(position, event->button(), press->second);
}

void SimulationTableView::clicked(const QPoint& position, Qt::MouseButton button, bool second)
{
    const QModelIndex index = indexAt(position);
    const int         first = TableRowTraversal::selectedRow(*this);
    // Clear the table selection when clicked outside the table rows (OpenRocket's comment).
    if (!index.isValid() || first < 0)
    {
        clearSelection();
        return;
    }
    if (button != Qt::LeftButton)
    {
        return;  // the right button's menu: contextMenuEvent()
    }
    const Column column = SimulationColumns::columnAt(index.column());
    // A copy: a slot may take the simulation out of the document.
    const std::shared_ptr<Simulation> simulation = m_model->simulation(first);
    if (!second)
    {
        Q_EMIT cellClicked(simulation, column);
        return;
    }
    if (column != Column::STATUS && column != Column::WARNINGS)
    {
        // Edit the simulation or plot/export (OpenRocket's comment): the first selected row
        // alone.
        selectionModel()->setCurrentIndex(
            m_model->index(first, index.column()),
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    Q_EMIT cellDoubleClicked(simulation, column);
}

void SimulationTableView::contextMenuEvent(QContextMenuEvent* event)
{
    event->accept();
    if (event->reason() != QContextMenuEvent::Mouse)
    {
        // The keyboard's menu key (OpenRocket has none): the menu of the selection, at the
        // current row.
        const int first = TableRowTraversal::selectedRow(*this);
        if (first < 0)
        {
            return;
        }
        const QModelIndex current = currentIndex();
        const QModelIndex row     = current.isValid() ? current : m_model->index(first, 0);
        Q_EMIT contextMenuRequested(viewport()->mapToGlobal(visualRect(row).center()));
        return;
    }
    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid())
    {
        clearSelection();
        return;
    }
    if (!selectionModel()->isRowSelected(index.row()))
    {
        selectionModel()->setCurrentIndex(
            index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    Q_EMIT contextMenuRequested(event->globalPos());
}

// ---------------------------------------------------------------------------------- the keys

void SimulationTableView::keyPressEvent(QKeyEvent* event)
{
    // Unregister the default actions that would otherwise conflict with RocketActions and their
    // acceleration keys (OpenRocket's comment): QAbstractItemView would copy the current cell's
    // text to the system clipboard.
    if (isWindowKey(*event))
    {
        event->ignore();
        return;
    }
    // JTable's ENTER and shift ENTER; the keypad's Enter too (Java's VK_ENTER for both keys).
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
        (modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier))
    {
        selectNextRowCell(modifiers == Qt::NoModifier);
        event->accept();
        return;
    }
    QTableView::keyPressEvent(event);
}

void SimulationTableView::keyboardSearch(const QString& /*search*/) { }

int SimulationTableView::currentSection() const
{
    const QModelIndex current = currentIndex();
    if (!current.isValid())
    {
        return -1;
    }
    const QHeaderView& header = *horizontalHeader();
    if (!header.isSectionHidden(current.column()))
    {
        return current.column();
    }
    for (int visual = header.visualIndex(current.column()) - 1; visual >= 0; --visual)
    {
        if (!header.isSectionHidden(header.logicalIndex(visual)))
        {
            return header.logicalIndex(visual);
        }
    }
    const std::vector<int> sections = visibleSections();
    return sections.empty() ? -1 : sections.front();
}

int SimulationTableView::leadSection(int section) const
{
    if (section >= 0)
    {
        return section;
    }
    const std::vector<int> sections = visibleSections();
    return sections.empty() ? 0 : sections.front();
}

void SimulationTableView::selectNextRowCell(bool forward)
{
    const std::vector<int> sections = visibleSections();
    const int              rows     = m_model->rowCount();
    if (rows == 0 || sections.empty())
    {
        return;
    }
    const int              last     = static_cast<int>(sections.size()) - 1;
    const std::vector<int> selected = selectedRows();
    const QModelIndex      current  = currentIndex();
    const int              section  = currentSection();
    int                    row      = current.isValid() ? current.row() : -1;
    int column = section < 0
                     ? -1
                     : static_cast<int>(std::ranges::find(sections, section) - sections.begin());
    const bool currentSelected = row >= 0 && std::ranges::binary_search(selected, row);
    if (selected.empty() || (selected.size() == 1 && currentSelected))
    {
        // Through the whole table: the cell's row becomes the selection.
        if (row < 0 || column < 0)
        {
            row    = forward ? 0 : rows - 1;
            column = forward ? 0 : last;
        }
        else
        {
            step(row, column, forward, 0, rows - 1, last);
        }
        selectionModel()->setCurrentIndex(
            m_model->index(row, sections[static_cast<std::size_t>(column)]),
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        return;
    }
    // Within the selection: the rows from the first selected to the last, those not selected
    // skipped; the selection stays.
    const int top    = selected.front();
    const int bottom = selected.back();
    if (row < 0 || column < 0)
    {
        row    = forward ? bottom : top;
        column = forward ? last : 0;
    }
    step(row, column, forward, top, bottom, last);
    while (!std::ranges::binary_search(selected, row))
    {
        step(row, column, forward, top, bottom, last);
    }
    selectionModel()->setCurrentIndex(
        m_model->index(row, sections[static_cast<std::size_t>(column)]),
        QItemSelectionModel::NoUpdate);
}

}  // namespace QtRocket::Gui
