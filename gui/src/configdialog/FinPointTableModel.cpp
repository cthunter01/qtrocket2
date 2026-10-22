#include "QtRocket/gui/configdialog/FinPointTableModel.h"

#include <cstddef>
#include <utility>
#include <vector>

#include <QAbstractTableModel>
#include <QLatin1Char>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QVariant>
#include <Qt>

#include "QtRocket/gui/figure/FinPointSelection.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"

namespace QtRocket::Gui
{

namespace
{

[[nodiscard]] const Unit& defaultLengthUnit()
{
    return unitGroup(UnitGroupId::LENGTH).getDefaultUnit();
}

}  // namespace

FinPointTableModel::FinPointTableModel(FinPointSelection target, QObject* parent)
  : QAbstractTableModel(parent), m_target(std::move(target)), m_rows(pointCount())
{
    m_rocketConnection = ComponentChangeSignal::ScopedConnection(
        m_target.rocket().addComponentChangeListener([this](const ComponentChangeEvent& /*event*/) {
            Guard::run([this] { refresh(); }, QStringLiteral("FinPoints"));
        }));
}

int FinPointTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows;
}

int FinPointTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : kColumnCount;
}

QVariant FinPointTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || (role != Qt::DisplayRole && role != Qt::EditRole))
    {
        return {};
    }
    return valueText(index.row(), index.column());
}

QVariant FinPointTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 ||
        section >= kColumnCount)
    {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    return columnName(section);
}

Qt::ItemFlags FinPointTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return Qt::NoItemFlags;
    }
    const Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    // The first and the last point: only X
    if (index.row() == 0 || index.row() == m_rows - 1)
    {
        return index.column() == kXColumn ? flags | Qt::ItemIsEditable : flags;
    }
    return flags | Qt::ItemIsEditable;
}

bool FinPointTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::EditRole || !index.isValid())
    {
        return false;
    }
    const FreeformFinSet* fins = m_target.main();
    // bounds check that indices are valid (the points may have changed since the edit began)
    if (fins == nullptr || index.row() < 0 ||
        static_cast<std::size_t>(index.row()) >= fins->getPointCount() || index.column() < 0 ||
        index.column() >= kColumnCount)
    {
        return false;
    }
    const Result<double> parsed =
        unitGroup(UnitGroupId::LENGTH).fromString(QtStrings::toUtf8(value.toString()));
    if (!parsed.has_value())
    {
        return false;  // Java ignores the NumberFormatException
    }
    Coordinate c = fins->getFinPoints().at(static_cast<std::size_t>(index.row()));
    if (index.column() == kXColumn)
    {
        c.x = *parsed;
    }
    else
    {
        c.y = *parsed;
    }
    Q_EMIT pointEntered(index.row(), c.x, c.y);
    return true;
}

void FinPointTableModel::refresh()
{
    const int count = pointCount();
    if (count > m_rows)
    {
        beginInsertRows(QModelIndex(), m_rows, count - 1);
        m_rows = count;
        endInsertRows();
    }
    else if (count < m_rows)
    {
        beginRemoveRows(QModelIndex(), count, m_rows - 1);
        m_rows = count;
        endRemoveRows();
    }
    if (m_rows > 0)
    {
        Q_EMIT dataChanged(index(0, 0), index(m_rows - 1, kColumnCount - 1));
    }
    Q_EMIT headerDataChanged(Qt::Horizontal, 0, kColumnCount - 1);
}

QString FinPointTableModel::columnName(int column)
{
    const QString unit = QtStrings::unitSymbol(defaultLengthUnit());
    return (column == kXColumn ? QStringLiteral("X / ") : QStringLiteral("Y / ")) + unit;
}

QString FinPointTableModel::valueText(int row, int column) const
{
    const FreeformFinSet* fins = m_target.main();
    if (fins == nullptr || row < 0 || static_cast<std::size_t>(row) >= fins->getPointCount())
    {
        return {};
    }
    const Coordinate point = fins->getFinPoints().at(static_cast<std::size_t>(row));
    return QtStrings::unitText(defaultLengthUnit(), column == kXColumn ? point.x : point.y);
}

QString FinPointTableModel::csvText() const
{
    // The separator after every text of a line.
    const QString separator = QStringLiteral(", ");
    // write the header information
    QString text;
    for (int j = 0; j < kColumnCount; ++j)
    {
        text += columnName(j) + separator;
    }
    text += QLatin1Char('\n');
    // write row information
    for (int i = 0; i < m_rows; ++i)
    {
        for (int j = 0; j < kColumnCount; ++j)
        {
            text += valueText(i, j) + separator;
        }
        text += QLatin1Char('\n');
    }
    return text;
}

int FinPointTableModel::pointCount() const
{
    const FreeformFinSet* fins = m_target.main();
    return fins == nullptr ? 0 : static_cast<int>(fins->getPointCount());
}

}  // namespace QtRocket::Gui
