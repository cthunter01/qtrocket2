#include "QtRocket/gui/flightconfig/MotorMountTableModel.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include <QAbstractTableModel>
#include <QModelIndex>
#include <QObject>
#include <QScopedValueRollback>
#include <QString>
#include <QVariant>
#include <Qt>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

MotorMountTableModel::MotorMountTableModel(Rocket& rocket, QObject* parent)
  : QAbstractTableModel(parent), m_rocket(&rocket), m_mountIds(currentMounts())
{
    m_changes = m_rocket->addComponentChangeListener(
        guarded([this](const ComponentChangeEvent& /*event*/) { refresh(); },
                QStringLiteral("Motor mount table")));
}

MotorMountTableModel::~MotorMountTableModel() = default;

// ---------------------------------------------------------------------- QAbstractTableModel

int MotorMountTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_mountIds.size());
}

int MotorMountTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : kColumnCount;
}

QVariant MotorMountTableModel::data(const QModelIndex& index, int role) const
{
    if (!owns(index))
    {
        return {};
    }
    const MotorMount* found = mount(index.row());
    if (found == nullptr)
    {
        return {};
    }
    if (index.column() == kMotorMountColumn)
    {
        if (role != Qt::CheckStateRole)
        {
            return {};
        }
        return {static_cast<int>(found->isMotorMount() ? Qt::Checked : Qt::Unchecked)};
    }
    return role == Qt::DisplayRole ? QVariant(QtStrings::fromUtf8(asComponent(*found).toString()))
                                   : QVariant();
}

QVariant MotorMountTableModel::headerData(int /*section*/, Qt::Orientation /*orientation*/,
                                          int /*role*/) const
{
    return {};
}

Qt::ItemFlags MotorMountTableModel::flags(const QModelIndex& index) const
{
    if (!owns(index))
    {
        return Qt::NoItemFlags;
    }
    const Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemNeverHasChildren;
    return index.column() == kMotorMountColumn ? flags | Qt::ItemIsUserCheckable : flags;
}

bool MotorMountTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::CheckStateRole || !owns(index) || index.column() != kMotorMountColumn)
    {
        return false;
    }
    bool      isNumber = false;
    const int state    = value.toInt(&isNumber);
    if (!isNumber || (state != Qt::Checked && state != Qt::Unchecked))
    {
        return false;
    }
    const MotorMount* found = mount(index.row());
    if (found == nullptr)
    {
        return false;
    }
    const bool motorMount = state == Qt::Checked;
    if (found->isMotorMount() != motorMount)
    {
        Q_EMIT motorMountChangeRequested(m_mountIds[static_cast<std::size_t>(index.row())],
                                         motorMount);
    }
    return true;
}

// ------------------------------------------------------------------------------- the keys

std::optional<Uuid> MotorMountTableModel::mountId(int row) const
{
    if (row < 0 || static_cast<std::size_t>(row) >= m_mountIds.size())
    {
        return std::nullopt;
    }
    return m_mountIds[static_cast<std::size_t>(row)];
}

MotorMount* MotorMountTableModel::mount(int row) const
{
    const std::optional<Uuid> id = mountId(row);
    if (!id.has_value())
    {
        return nullptr;
    }
    return dynamic_cast<MotorMount*>(m_rocket->findComponent(*id));
}

int MotorMountTableModel::rowOf(const Uuid& id) const noexcept
{
    const auto found = std::ranges::find(m_mountIds, id);
    return found == m_mountIds.end() ? -1
                                     : static_cast<int>(std::distance(m_mountIds.begin(), found));
}

void MotorMountTableModel::refresh()
{
    if (m_resetting)
    {
        m_changedWhileResetting = true;
        return;
    }
    std::vector<Uuid> mounts = currentMounts();
    if (mounts != m_mountIds)
    {
        reset(std::move(mounts));
        return;
    }
    emitEverythingChanged();
}

bool MotorMountTableModel::owns(const QModelIndex& index) const noexcept
{
    return index.isValid() && index.model() == this && index.row() < rowCount() &&
           index.column() < kColumnCount;
}

std::vector<Uuid> MotorMountTableModel::currentMounts() const
{
    std::vector<Uuid> mounts;
    m_rocket->forEach([&mounts](const RocketComponent& component) {
        if (dynamic_cast<const MotorMount*>(&component) != nullptr)
        {
            mounts.push_back(component.getId());
        }
    });
    return mounts;
}

void MotorMountTableModel::reset(std::vector<Uuid> mounts)
{
    std::optional<std::vector<Uuid>> next = std::move(mounts);
    while (next.has_value())
    {
        {
            // Set while the views answer the reset (and cleared if one of them throws).
            const QScopedValueRollback<bool> resetting(m_resetting, true);
            m_changedWhileResetting = false;
            beginResetModel();
            m_mountIds = std::move(*next);
            endResetModel();
        }
        next.reset();
        if (m_changedWhileResetting)
        {
            m_changedWhileResetting   = false;
            std::vector<Uuid> current = currentMounts();
            if (current != m_mountIds)
            {
                next = std::move(current);
            }
            else
            {
                emitEverythingChanged();
            }
        }
    }
}

void MotorMountTableModel::emitEverythingChanged()
{
    const int rows = rowCount();
    if (rows > 0)
    {
        Q_EMIT dataChanged(index(0, 0), index(rows - 1, kColumnCount - 1));
    }
}

}  // namespace QtRocket::Gui
