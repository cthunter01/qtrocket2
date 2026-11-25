#include "QtRocket/gui/dialogs/plot/FlightEventTableModel.h"

#include <bitset>
#include <cstddef>
#include <format>
#include <memory>
#include <utility>
#include <vector>

#include <QAbstractTableModel>
#include <QList>
#include <QMetaType>
#include <QModelIndex>
#include <QObject>
#include <QVariant>
#include <Qt>

#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

std::vector<FlightEvent::Type> FlightEventTableModel::eventTypesOf(const FlightData& data)
{
    // EnumSet.noneOf(Type), every branch's events added, ALTITUDE removed, then the types in
    // declaration order.
    std::bitset<FlightEvent::kAllTypes.size()> present;
    for (const std::shared_ptr<FlightDataBranch>& branch : data.getBranches())
    {
        for (const FlightEvent& event : branch->getEvents())
        {
            present.set(static_cast<std::size_t>(event.getType()));
        }
    }
    present.reset(static_cast<std::size_t>(FlightEvent::Type::ALTITUDE));
    std::vector<FlightEvent::Type> types;
    for (const FlightEvent::Type type : FlightEvent::kAllTypes)
    {
        if (present.test(static_cast<std::size_t>(type)))
        {
            types.push_back(type);
        }
    }
    return types;
}

FlightEventTableModel::FlightEventTableModel(std::vector<FlightEvent::Type> eventTypes,
                                             SimulationPlotConfiguration&   configuration,
                                             QObject*                       parent)
  : QAbstractTableModel(parent),
    m_eventTypes(std::move(eventTypes)),
    m_configuration(&configuration)
{
}

FlightEventTableModel::~FlightEventTableModel() = default;

FlightEvent::Type FlightEventTableModel::eventType(int row) const
{
    checkRow(row);
    return m_eventTypes[static_cast<std::size_t>(row)];
}

bool FlightEventTableModel::isChecked(int row) const
{
    return m_configuration->isEventActive(eventType(row));
}

void FlightEventTableModel::toggle(int row)
{
    // BooleanTableClickListener: setValueAt(!value) and fireTableCellUpdated().
    setData(index(row, kSelectedColumn), isChecked(row) ? Qt::Unchecked : Qt::Checked,
            Qt::CheckStateRole);
}

void FlightEventTableModel::refresh()
{
    if (!m_eventTypes.empty())
    {
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, kColumnCount - 1));
    }
}

int FlightEventTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_eventTypes.size());
}

int FlightEventTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : kColumnCount;
}

QVariant FlightEventTableModel::data(const QModelIndex& index, int role) const
{
    if (!isCell(index))
    {
        return {};
    }
    if (index.column() == kSelectedColumn && role == Qt::CheckStateRole)
    {
        return isChecked(index.row()) ? Qt::Checked : Qt::Unchecked;
    }
    if (index.column() == kNameColumn && role == Qt::DisplayRole)
    {
        return enumText(eventType(index.row()));
    }
    return {};
}

Qt::ItemFlags FlightEventTableModel::flags(const QModelIndex& index) const
{
    if (!isCell(index))
    {
        return Qt::NoItemFlags;
    }
    // Only the check can be changed (isCellEditable(): column 0); no row can be selected (the
    // table allows no selection).
    if (index.column() == kSelectedColumn)
    {
        return Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
    }
    return Qt::ItemIsEnabled;
}

bool FlightEventTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    // Java's setValueAt() takes a Boolean of column 0; a check state here (Qt::CheckState, or its
    // int, as the item views give it).
    const bool isState = value.metaType() == QMetaType::fromType<Qt::CheckState>() ||
                         value.metaType() == QMetaType::fromType<int>();
    if (!isCell(index) || index.column() != kSelectedColumn || role != Qt::CheckStateRole ||
        !isState)
    {
        return false;
    }
    const auto state = static_cast<Qt::CheckState>(value.toInt());
    if (state != Qt::Checked && state != Qt::Unchecked)
    {
        return false;
    }
    m_configuration->setEvent(eventType(index.row()), state == Qt::Checked);
    // fireTableCellUpdated(row, column).
    Q_EMIT dataChanged(index, index, {Qt::CheckStateRole});
    return true;
}

bool FlightEventTableModel::isCell(const QModelIndex& index) const
{
    return index.isValid() && index.model() == this && !index.parent().isValid() &&
           index.row() >= 0 && index.row() < rowCount() && index.column() >= 0 &&
           index.column() < kColumnCount;
}

void FlightEventTableModel::checkRow(int row) const
{
    if (row < 0 || std::cmp_greater_equal(row, m_eventTypes.size()))
    {
        bug(std::format("no row {} of {} flight event types", row, m_eventTypes.size()));
    }
}

}  // namespace QtRocket::Gui
