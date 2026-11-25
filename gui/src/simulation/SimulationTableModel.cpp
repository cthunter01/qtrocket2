#include "QtRocket/gui/simulation/SimulationTableModel.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <QAbstractItemModel>
#include <QAbstractTableModel>
#include <QGuiApplication>
#include <QHeaderView>
#include <QList>
#include <QModelIndex>
#include <QObject>
#include <QPalette>
#include <QString>
#include <QVariant>
#include <Qt>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/SimulationIcons.h"
#include "QtRocket/gui/util/TableSortKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// An int of the model's roles for @p count.
[[nodiscard]] QVariant countVariant(std::size_t count)
{
    return static_cast<int>(count);
}

}  // namespace

SimulationTableModel::SimulationTableModel(OpenRocketDocument& document,
                                           const Preferences& preferences, QObject* parent)
  : QAbstractTableModel(parent),
    m_document(&document),
    m_preferences(&preferences),
    m_rows(readRows()),
    // Every event of the document: the rocket's, a simulation's, the list's, an undo or redo, a
    // document preference (OpenRocket's table repaints after each of them).
    m_documentChanged(m_document->documentChanged().connect(
        guarded([this] { refresh(); }, QStringLiteral("Simulations table"))))
{
}

SimulationTableModel::~SimulationTableModel() = default;

int SimulationTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int SimulationTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : SimulationColumns::kColumnCount;
}

QVariant SimulationTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount() || index.column() >= columnCount())
    {
        return {};
    }
    const Row&                          row    = m_rows.at(static_cast<std::size_t>(index.row()));
    const Column                        column = SimulationColumns::columnAt(index.column());
    const SimulationColumns::CellValue& value =
        row.cells.at(static_cast<std::size_t>(index.column()));
    switch (role)
    {
        case Qt::DisplayRole:
            if (column == Column::STATUS || column == Column::WARNINGS ||
                std::holds_alternative<std::monostate>(value))
            {
                return {};
            }
            return SimulationColumns::cellText(column, value);
        case Qt::DecorationRole:
            if (column == Column::STATUS)
            {
                return SimulationIcons::statusIcon(row.status);
            }
            return {};
        case Qt::ToolTipRole:
            if (column == Column::WARNINGS)
            {
                return SimulationColumns::warningsToolTip(*row.simulation, true,
                                                          QGuiApplication::palette());
            }
            return SimulationColumns::statusToolTip(row.status, *row.simulation, true,
                                                    QGuiApplication::palette());
        case Qt::AccessibleTextRole:
            if (column == Column::STATUS)
            {
                return SimulationColumns::statusIconText(row.status);
            }
            return {};
        default:
            break;
    }
    if (column != Column::WARNINGS ||
        (role != kCriticalWarningsRole && role != kNormalWarningsRole &&
         role != kInformationalWarningsRole))
    {
        return {};
    }
    const auto* counts = std::get_if<SimulationColumns::WarningCounts>(&value);
    if (counts == nullptr)
    {
        return {};
    }
    if (role == kCriticalWarningsRole)
    {
        return countVariant(counts->critical);
    }
    return countVariant(role == kNormalWarningsRole ? counts->normal : counts->informational);
}

QVariant SimulationTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || section < 0 || section >= SimulationColumns::kColumnCount)
    {
        return {};
    }
    const Column column = SimulationColumns::columnAt(section);
    if (role == Qt::DisplayRole)
    {
        return SimulationColumns::headerText(column);
    }
    if (role == Qt::ToolTipRole)
    {
        const QString tip = SimulationColumns::headerToolTip(column);
        return tip.isEmpty() ? QVariant() : QVariant(tip);
    }
    return {};
}

Qt::ItemFlags SimulationTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemNeverHasChildren;
}

// ------------------------------------------------------------------------------ the rows

const std::shared_ptr<Simulation>& SimulationTableModel::simulation(int row) const
{
    return rowAt(row).simulation;
}

int SimulationTableModel::rowOf(const Simulation& simulation) const noexcept
{
    for (std::size_t row = 0; row < m_rows.size(); ++row)
    {
        if (m_rows[row].simulation.get() == &simulation)
        {
            return static_cast<int>(row);
        }
    }
    return -1;
}

std::vector<std::shared_ptr<Simulation>> SimulationTableModel::simulations() const
{
    std::vector<std::shared_ptr<Simulation>> simulations;
    simulations.reserve(m_rows.size());
    for (const Row& row : m_rows)
    {
        simulations.push_back(row.simulation);
    }
    return simulations;
}

Simulation::Status SimulationTableModel::status(int row) const
{
    return rowAt(row).status;
}

const SimulationColumns::CellValue& SimulationTableModel::cellValue(int row, Column column) const
{
    return rowAt(row).cells.at(static_cast<std::size_t>(SimulationColumns::indexOf(column)));
}

QString SimulationTableModel::cellText(int row, Column column) const
{
    return SimulationColumns::cellText(column, cellValue(row, column));
}

std::optional<SimulationColumns::WarningCounts> SimulationTableModel::warningCounts(int row) const
{
    const auto* counts =
        std::get_if<SimulationColumns::WarningCounts>(&cellValue(row, Column::WARNINGS));
    if (counts == nullptr)
    {
        return std::nullopt;
    }
    return *counts;
}

const SimulationTableModel::Row& SimulationTableModel::rowAt(int row) const
{
    if (row < 0 || row >= rowCount())
    {
        bug("The simulations table has no row " + std::to_string(row) + " (it has " +
            std::to_string(rowCount()) + ")");
    }
    return m_rows[static_cast<std::size_t>(row)];
}

// ------------------------------------------------------------------------------ reading

SimulationTableModel::Row SimulationTableModel::readRow(
    const std::shared_ptr<Simulation>& simulation, std::size_t documentIndex) const
{
    Row row;
    row.simulation    = simulation;
    row.documentIndex = documentIndex;
    // The one getStatus() of this reading (see the class comment).
    row.status = simulation->getStatus();
    for (const Column column : SimulationColumns::kAllColumns)
    {
        row.cells.at(static_cast<std::size_t>(SimulationColumns::indexOf(column))) =
            SimulationColumns::cellValue(column, *simulation, row.status, *m_preferences);
    }
    return row;
}

std::vector<SimulationTableModel::Row> SimulationTableModel::readRows() const
{
    const std::vector<std::shared_ptr<Simulation>> simulations = m_document->getSimulations();
    std::vector<Row>                               rows;
    rows.reserve(simulations.size());
    for (std::size_t index = 0; index < simulations.size(); ++index)
    {
        rows.push_back(readRow(simulations[index], index));
    }
    return rows;
}

void SimulationTableModel::refresh()
{
    setRows(sorted(readRows()));
}

std::vector<SimulationTableModel::Row> SimulationTableModel::sorted(std::vector<Row> rows) const
{
    // The base order, which the keys keep for equal rows: the document's (the model rows').
    std::ranges::sort(rows, {}, &Row::documentIndex);
    std::vector<int> order(rows.size());
    for (std::size_t row = 0; row < order.size(); ++row)
    {
        order[row] = static_cast<int>(row);
    }
    m_sortKeys.sortRows(order, [&rows](int column, int a, int b) {
        const auto cell = static_cast<std::size_t>(column);
        return SimulationColumns::compare(SimulationColumns::columnAt(column),
                                          rows.at(static_cast<std::size_t>(a)).cells.at(cell),
                                          rows.at(static_cast<std::size_t>(b)).cells.at(cell));
    });
    std::vector<Row> result;
    result.reserve(rows.size());
    for (const int row : order)
    {
        result.push_back(std::move(rows.at(static_cast<std::size_t>(row))));
    }
    return result;
}

void SimulationTableModel::setRows(std::vector<Row> rows)
{
    bool sameOrder = rows.size() == m_rows.size();
    for (std::size_t row = 0; sameOrder && row < rows.size(); ++row)
    {
        sameOrder = rows[row].simulation == m_rows[row].simulation;
    }
    if (sameOrder)
    {
        m_rows = std::move(rows);
        if (!m_rows.empty())
        {
            Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
        }
        Q_EMIT refreshed();
        return;
    }

    // Another order of the same simulations, or another list: the indexes the views keep go
    // with their simulations.
    bool sameSimulations = rows.size() == m_rows.size();
    for (std::size_t row = 0; sameSimulations && row < rows.size(); ++row)
    {
        sameSimulations = rowOf(*rows[row].simulation) >= 0;
    }
    const QAbstractItemModel::LayoutChangeHint hint = sameSimulations
                                                          ? QAbstractItemModel::VerticalSortHint
                                                          : QAbstractItemModel::NoLayoutChangeHint;
    Q_EMIT layoutAboutToBeChanged({}, hint);
    std::vector<std::shared_ptr<Simulation>> before = simulations();
    m_rows                                          = std::move(rows);
    const QModelIndexList from                      = persistentIndexList();
    QModelIndexList       to;
    to.reserve(from.size());
    for (const QModelIndex& old : from)
    {
        int moved = -1;
        if (old.isValid() && old.row() < static_cast<int>(before.size()))
        {
            moved = rowOf(*before.at(static_cast<std::size_t>(old.row())));
        }
        to.append(moved < 0 ? QModelIndex() : index(moved, old.column()));
    }
    changePersistentIndexList(from, to);
    Q_EMIT layoutChanged({}, hint);
    Q_EMIT refreshed();
}

// ------------------------------------------------------------------------------ the order

void SimulationTableModel::resort()
{
    std::vector<Row> rows  = m_rows;
    std::vector<Row> order = sorted(std::move(rows));
    bool             same  = true;
    for (std::size_t row = 0; same && row < order.size(); ++row)
    {
        same = order[row].simulation == m_rows[row].simulation;
    }
    if (!same)
    {
        setRows(std::move(order));
    }
}

void SimulationTableModel::toggleSortOrder(int column)
{
    m_sortKeys.toggle(column);
    resort();
    updateSortIndicator();
    Q_EMIT sortKeysChanged();
}

void SimulationTableModel::setSortKeys(std::vector<TableSortKeys::SortKey> keys)
{
    m_sortKeys.setKeys(std::move(keys));
    resort();
    updateSortIndicator();
    Q_EMIT sortKeysChanged();
}

void SimulationTableModel::connectHeader(QHeaderView& header)
{
    if (!m_header.isNull())
    {
        QObject::disconnect(m_header, nullptr, this, nullptr);
    }
    m_header = &header;
    header.setSectionsClickable(true);
    header.setSortIndicatorShown(true);
    connect(&header, &QHeaderView::sectionClicked, this,
            guarded([this](int section) { toggleSortOrder(section); },
                    QStringLiteral("Simulations table header")));
    updateSortIndicator();
}

void SimulationTableModel::updateSortIndicator()
{
    if (m_header.isNull())
    {
        return;
    }
    const std::optional<TableSortKeys::SortKey> primary = m_sortKeys.primaryKey();
    if (!primary.has_value())
    {
        m_header->setSortIndicator(-1, Qt::AscendingOrder);
        return;
    }
    m_header->setSortIndicator(primary->column,
                               primary->order == TableSortKeys::SortOrder::ASCENDING
                                   ? Qt::AscendingOrder
                                   : Qt::DescendingOrder);
}

}  // namespace QtRocket::Gui
