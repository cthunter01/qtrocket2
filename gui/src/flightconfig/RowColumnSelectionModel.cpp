#include "QtRocket/gui/flightconfig/RowColumnSelectionModel.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

#include <QAbstractItemModel>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QItemSelectionRange>
#include <QModelIndex>
#include <QObject>

namespace QtRocket::Gui
{

namespace
{

using Lines = RowColumnSelectionModel::Lines;

/// @p values in increasing order, each once.
[[nodiscard]] std::vector<int> sortedUnique(std::vector<int> values)
{
    std::ranges::sort(values);
    const auto [first, last] = std::ranges::unique(values);
    values.erase(first, last);
    return values;
}

/// The union of the sorted @p a and @p b.
[[nodiscard]] std::vector<int> unite(const std::vector<int>& a, const std::vector<int>& b)
{
    std::vector<int> result;
    std::ranges::set_union(a, b, std::back_inserter(result));
    return result;
}

/// The sorted @p a without the sorted @p b.
[[nodiscard]] std::vector<int> subtract(const std::vector<int>& a, const std::vector<int>& b)
{
    std::vector<int> result;
    std::ranges::set_difference(a, b, std::back_inserter(result));
    return result;
}

/// Whether the sorted @p a holds every value of the sorted @p b.
[[nodiscard]] bool includesAll(const std::vector<int>& a, const std::vector<int>& b)
{
    return std::ranges::includes(a, b);
}

/// The runs of consecutive values of the sorted @p values, as (first, last) pairs.
[[nodiscard]] std::vector<std::pair<int, int>> runsOf(const std::vector<int>& values)
{
    std::vector<std::pair<int, int>> runs;
    for (const int value : values)
    {
        if (!runs.empty() && runs.back().second + 1 == value)
        {
            runs.back().second = value;
        }
        else
        {
            runs.emplace_back(value, value);
        }
    }
    return runs;
}

/// The values from 0 to @p count - 1.
[[nodiscard]] std::vector<int> allUpTo(int count)
{
    std::vector<int> values;
    values.reserve(static_cast<std::size_t>(std::max(count, 0)));
    for (int value = 0; value < count; ++value)
    {
        values.push_back(value);
    }
    return values;
}

}  // namespace

RowColumnSelectionModel::RowColumnSelectionModel(QAbstractItemModel* model, QObject* parent)
  : QItemSelectionModel(model, parent)
{
    if (model == nullptr)
    {
        return;
    }
    const auto changed = [this] { structureChanged(); };
    connect(model, &QAbstractItemModel::rowsInserted, this, changed);
    connect(model, &QAbstractItemModel::rowsRemoved, this, changed);
    connect(model, &QAbstractItemModel::rowsMoved, this, changed);
    connect(model, &QAbstractItemModel::columnsInserted, this, changed);
    connect(model, &QAbstractItemModel::columnsRemoved, this, changed);
    connect(model, &QAbstractItemModel::columnsMoved, this, changed);
    connect(model, &QAbstractItemModel::layoutChanged, this, changed);
}

RowColumnSelectionModel::~RowColumnSelectionModel() = default;

void RowColumnSelectionModel::select(const QModelIndex&                  index,
                                     QItemSelectionModel::SelectionFlags command)
{
    QItemSelection selection;
    if (index.isValid())
    {
        selection.select(index, index);
    }
    select(selection, command);
}

void RowColumnSelectionModel::select(const QItemSelection&               selection,
                                     QItemSelectionModel::SelectionFlags command)
{
    if (command == QItemSelectionModel::NoUpdate || model() == nullptr)
    {
        return;
    }
    const Lines named   = linesOf(selection, command);
    const bool  current = command.testFlag(QItemSelectionModel::Current);
    Lines       next;
    if (!command.testFlag(QItemSelectionModel::Clear))
    {
        next = current ? m_committed : m_lines;
    }
    if (command.testFlag(QItemSelectionModel::Toggle))
    {
        // JTable.changeSelection(row, column, true, false): a selected cell goes with its row
        // and its column, another one comes with them.
        const bool selected = !named.rows.empty() && !named.columns.empty() &&
                              includesAll(next.rows, named.rows) &&
                              includesAll(next.columns, named.columns);
        next                = selected ? Lines{.rows    = subtract(next.rows, named.rows),
                                               .columns = subtract(next.columns, named.columns)}
                                       : Lines{.rows    = unite(next.rows, named.rows),
                                               .columns = unite(next.columns, named.columns)};
    }
    else if (command.testFlag(QItemSelectionModel::Select))
    {
        next = Lines{.rows    = unite(next.rows, named.rows),
                     .columns = unite(next.columns, named.columns)};
    }
    else if (command.testFlag(QItemSelectionModel::Deselect))
    {
        next = Lines{.rows    = subtract(next.rows, named.rows),
                     .columns = subtract(next.columns, named.columns)};
    }
    next = valid(std::move(next));
    if (!current)
    {
        m_committed = next;
    }
    apply(std::move(next), Order::COLUMNS_FIRST);
}

void RowColumnSelectionModel::reset()
{
    QItemSelectionModel::reset();
    m_lines     = {};
    m_committed = {};
}

bool RowColumnSelectionModel::isCellSelected(int row, int column) const noexcept
{
    return std::ranges::binary_search(m_lines.rows, row) &&
           std::ranges::binary_search(m_lines.columns, column);
}

int RowColumnSelectionModel::firstRow() const noexcept
{
    return m_lines.rows.empty() ? -1 : m_lines.rows.front();
}

int RowColumnSelectionModel::firstColumn() const noexcept
{
    return m_lines.columns.empty() ? -1 : m_lines.columns.front();
}

void RowColumnSelectionModel::setLines(const Lines& lines, Order order)
{
    Lines checked = valid(lines);
    m_committed   = checked;
    apply(std::move(checked), order);
}

void RowColumnSelectionModel::selectCell(int row, int column)
{
    if (model() == nullptr)
    {
        return;
    }
    setCurrentIndex(model()->index(row, column), QItemSelectionModel::NoUpdate);
    setLines(Lines{.rows = {row}, .columns = {column}}, Order::COLUMNS_FIRST);
}

void RowColumnSelectionModel::addCell(int row, int column)
{
    if (model() == nullptr)
    {
        return;
    }
    setCurrentIndex(model()->index(row, column), QItemSelectionModel::NoUpdate);
    setLines(Lines{.rows = unite(m_lines.rows, {row}), .columns = unite(m_lines.columns, {column})},
             Order::COLUMNS_FIRST);
}

void RowColumnSelectionModel::clearLines()
{
    setLines({}, Order::ROWS_FIRST);
}

RowColumnSelectionModel::Lines RowColumnSelectionModel::linesOf(
    const QItemSelection& selection, QItemSelectionModel::SelectionFlags command) const
{
    Lines named;
    for (const QItemSelectionRange& range : selection)
    {
        if (!range.isValid() || range.model() != model())
        {
            continue;
        }
        for (int row = range.top(); row <= range.bottom(); ++row)
        {
            named.rows.push_back(row);
        }
        for (int column = range.left(); column <= range.right(); ++column)
        {
            named.columns.push_back(column);
        }
    }
    if (command.testFlag(QItemSelectionModel::Rows) && !named.rows.empty())
    {
        named.columns = allUpTo(model()->columnCount());
    }
    if (command.testFlag(QItemSelectionModel::Columns) && !named.columns.empty())
    {
        named.rows = allUpTo(model()->rowCount());
    }
    named.rows    = sortedUnique(std::move(named.rows));
    named.columns = sortedUnique(std::move(named.columns));
    return named;
}

RowColumnSelectionModel::Lines RowColumnSelectionModel::valid(Lines lines) const
{
    const int rows    = model() == nullptr ? 0 : model()->rowCount();
    const int columns = model() == nullptr ? 0 : model()->columnCount();
    std::erase_if(lines.rows, [rows](int row) { return row < 0 || row >= rows; });
    std::erase_if(lines.columns, [columns](int column) { return column < 0 || column >= columns; });
    lines.rows    = sortedUnique(std::move(lines.rows));
    lines.columns = sortedUnique(std::move(lines.columns));
    return lines;
}

void RowColumnSelectionModel::apply(Lines lines, Order order)
{
    if (lines == m_lines)
    {
        showCells();
        return;
    }
    const LinesChange change{.before = m_lines, .after = lines, .order = order};
    m_lines = std::move(lines);
    showCells();
    Q_EMIT linesChanged(change);
}

void RowColumnSelectionModel::showCells()
{
    QItemSelection cells;
    if (model() != nullptr)
    {
        const std::vector<std::pair<int, int>> rowRuns    = runsOf(m_lines.rows);
        const std::vector<std::pair<int, int>> columnRuns = runsOf(m_lines.columns);
        for (const auto& [top, bottom] : rowRuns)
        {
            for (const auto& [left, right] : columnRuns)
            {
                cells.append(
                    QItemSelectionRange(model()->index(top, left), model()->index(bottom, right)));
            }
        }
    }
    QItemSelectionModel::select(cells, QItemSelectionModel::ClearAndSelect);
}

void RowColumnSelectionModel::structureChanged()
{
    m_committed = {};
    apply({}, Order::ROWS_FIRST);
}

}  // namespace QtRocket::Gui
