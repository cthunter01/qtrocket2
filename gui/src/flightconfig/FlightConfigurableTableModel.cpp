#include "QtRocket/gui/flightconfig/FlightConfigurableTableModel.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include <QAbstractTableModel>
#include <QFont>
#include <QModelIndex>
#include <QObject>
#include <QScopedValueRollback>
#include <QString>
#include <QVariant>
#include <Qt>

#include "QtRocket/gui/flightconfig/FlightConfigurationCellText.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

/// The italic font of a shaded cell: italic only, the rest the view's (QFont::resolve()).
QFont shadedFont()
{
    QFont font;
    font.setItalic(true);
    return font;
}

/// Whether the cell of @p component, a recovery device or a stage of a table of @p kind, shows
/// the default for @p id; nullopt for a component of another kind.
std::optional<bool> valueIsDefault(FlightConfigurableTableModel::Kind kind,
                                   const RocketComponent&             component,
                                   const FlightConfigurationId&       id)
{
    if (kind == FlightConfigurableTableModel::Kind::RECOVERY_DEVICES)
    {
        const auto* device = dynamic_cast<const RecoveryDevice*>(&component);
        if (device != nullptr)
        {
            return FlightConfigurationCellText::deploymentIsDefault(*device, id);
        }
    }
    else if (kind == FlightConfigurableTableModel::Kind::STAGES)
    {
        const auto* stage = dynamic_cast<const AxialStage*>(&component);
        if (stage != nullptr)
        {
            return FlightConfigurationCellText::separationIsDefault(*stage, id);
        }
    }
    return std::nullopt;
}

/// The text of the cell of @p component, a recovery device or a stage of a table of @p kind
/// (valueIsDefault() tells it is one), for @p id.
QString valueText(FlightConfigurableTableModel::Kind kind, const RocketComponent& component,
                  const FlightConfigurationId& id)
{
    if (kind == FlightConfigurableTableModel::Kind::RECOVERY_DEVICES)
    {
        return FlightConfigurationCellText::deployment(
            dynamic_cast<const RecoveryDevice&>(component).getDeploymentConfigurations().get(id));
    }
    return FlightConfigurationCellText::separation(
        dynamic_cast<const AxialStage&>(component).getSeparationConfigurations().get(id));
}

}  // namespace

FlightConfigurableTableModel::FlightConfigurableTableModel(Kind kind, Rocket& rocket,
                                                           const Preferences& preferences,
                                                           QObject*           parent)
  : QAbstractTableModel(parent), m_kind(kind), m_rocket(&rocket), m_preferences(&preferences)
{
    Keys keys          = currentKeys();
    m_configurationIds = std::move(keys.configurations);
    m_componentIds     = std::move(keys.components);
    m_changes          = m_rocket->addComponentChangeListener(
        guarded([this](const ComponentChangeEvent& event) { componentChanged(event); },
                QStringLiteral("Flight configuration table")));
}

FlightConfigurableTableModel::~FlightConfigurableTableModel() = default;

// ---------------------------------------------------------------------- QAbstractTableModel

int FlightConfigurableTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_configurationIds.size());
}

int FlightConfigurableTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_componentIds.size()) + 1;
}

QVariant FlightConfigurableTableModel::data(const QModelIndex& index, int role) const
{
    if (!owns(index))
    {
        return {};
    }
    const FlightConfigurationId& id = m_configurationIds.at(static_cast<std::size_t>(index.row()));
    if (index.column() == 0)
    {
        if (role == Qt::DisplayRole)
        {
            return FlightConfigurationCellText::configurationName(*m_rocket, id, *m_preferences);
        }
        return role == kDefaultRole ? QVariant(false) : QVariant();
    }
    const RocketComponent* found = component(index.column());
    return found == nullptr ? QVariant() : componentData(*found, id, role);
}

QVariant FlightConfigurableTableModel::componentData(const RocketComponent&       component,
                                                     const FlightConfigurationId& id,
                                                     int                          role) const
{
    if (m_kind == Kind::MOTOR_MOUNTS)
    {
        const auto* mount = dynamic_cast<const MotorMount*>(&component);
        if (mount == nullptr)
        {
            return {};
        }
        switch (role)
        {
            case Qt::DisplayRole:
                return FlightConfigurationCellText::motor(*mount, id, *m_preferences);
            case kIgnitionTextRole:
                return FlightConfigurationCellText::ignition(*mount, id).text;
            case kDefaultRole:
                return FlightConfigurationCellText::ignition(*mount, id).usesDefault;
            default:
                // The motor label is never shaded; MotorCellDelegate styles the ignition.
                return {};
        }
    }
    const std::optional<bool> isDefault = valueIsDefault(m_kind, component, id);
    if (!isDefault.has_value())
    {
        return {};
    }
    switch (role)
    {
        case Qt::DisplayRole:
            return valueText(m_kind, component, id);
        case kDefaultRole:
            return *isDefault;
        case Qt::FontRole:
            return *isDefault ? QVariant(shadedFont()) : QVariant();
        case Qt::ForegroundRole:
            return *isDefault ? QVariant(ThemeColors::color(ThemeColors::Key::TEXT_DIM))
                              : QVariant();
        default:
            return {};
    }
}

QVariant FlightConfigurableTableModel::headerData(int section, Qt::Orientation orientation,
                                                  int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 ||
        section >= columnCount())
    {
        return {};
    }
    if (section == 0)
    {
        return FlightConfigurationCellText::configurationHeader();
    }
    const RocketComponent* found = component(section);
    return found == nullptr ? QVariant()
                            : QVariant(FlightConfigurationCellText::componentHeader(*found));
}

Qt::ItemFlags FlightConfigurableTableModel::flags(const QModelIndex& index) const
{
    if (!owns(index))
    {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren;
}

// ------------------------------------------------------------------------------- the keys

bool FlightConfigurableTableModel::includes(Kind kind, const RocketComponent& component)
{
    switch (kind)
    {
        case Kind::MOTOR_MOUNTS:
        {
            const auto* mount = dynamic_cast<const MotorMount*>(&component);
            return mount != nullptr && mount->isMotorMount();
        }
        case Kind::RECOVERY_DEVICES:
            return dynamic_cast<const RecoveryDevice*>(&component) != nullptr;
        case Kind::STAGES:
        {
            const auto* stage = dynamic_cast<const AxialStage*>(&component);
            return stage != nullptr && stage->getStageNumber() > 0;
        }
    }
    return false;
}

std::optional<FlightConfigurationId> FlightConfigurableTableModel::configurationId(int row) const
{
    if (row < 0 || static_cast<std::size_t>(row) >= m_configurationIds.size())
    {
        return std::nullopt;
    }
    return m_configurationIds[static_cast<std::size_t>(row)];
}

std::optional<Uuid> FlightConfigurableTableModel::componentId(int column) const
{
    if (column < 1 || static_cast<std::size_t>(column) > m_componentIds.size())
    {
        return std::nullopt;
    }
    return m_componentIds[static_cast<std::size_t>(column) - 1];
}

RocketComponent* FlightConfigurableTableModel::component(int column) const
{
    const std::optional<Uuid> id = componentId(column);
    return id.has_value() ? m_rocket->findComponent(*id) : nullptr;
}

int FlightConfigurableTableModel::rowOf(const FlightConfigurationId& id) const noexcept
{
    const auto found = std::ranges::find(m_configurationIds, id);
    return found == m_configurationIds.end()
               ? -1
               : static_cast<int>(std::distance(m_configurationIds.begin(), found));
}

int FlightConfigurableTableModel::columnOf(const Uuid& id) const noexcept
{
    const auto found = std::ranges::find(m_componentIds, id);
    return found == m_componentIds.end()
               ? -1
               : static_cast<int>(std::distance(m_componentIds.begin(), found)) + 1;
}

void FlightConfigurableTableModel::refresh()
{
    if (m_resetting)
    {
        m_changedWhileResetting = true;
        return;
    }
    Keys keys = currentKeys();
    if (keys.configurations != m_configurationIds || keys.components != m_componentIds)
    {
        reset(std::move(keys));
        return;
    }
    emitEverythingChanged();
}

bool FlightConfigurableTableModel::owns(const QModelIndex& index) const noexcept
{
    return index.isValid() && index.model() == this && index.row() < rowCount() &&
           index.column() < columnCount();
}

FlightConfigurableTableModel::Keys FlightConfigurableTableModel::currentKeys() const
{
    Keys keys{.configurations = m_rocket->getIds(), .components = {}};
    m_rocket->forEach([this, &keys](const RocketComponent& component) {
        if (includes(m_kind, component))
        {
            keys.components.push_back(component.getId());
        }
    });
    return keys;
}

void FlightConfigurableTableModel::componentChanged(const ComponentChangeEvent& /*event*/)
{
    refresh();
}

void FlightConfigurableTableModel::reset(Keys keys)
{
    std::optional<Keys> next = std::move(keys);
    while (next.has_value())
    {
        {
            // Set while the views answer the reset (and cleared if one of them throws).
            const QScopedValueRollback<bool> resetting(m_resetting, true);
            m_changedWhileResetting = false;
            beginResetModel();
            m_configurationIds = std::move(next->configurations);
            m_componentIds     = std::move(next->components);
            endResetModel();
        }
        next.reset();
        if (m_changedWhileResetting)
        {
            m_changedWhileResetting = false;
            Keys current            = currentKeys();
            if (current.configurations != m_configurationIds ||
                current.components != m_componentIds)
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

void FlightConfigurableTableModel::emitEverythingChanged()
{
    const int rows    = rowCount();
    const int columns = columnCount();
    if (rows > 0)
    {
        Q_EMIT dataChanged(index(0, 0), index(rows - 1, columns - 1));
    }
    Q_EMIT headerDataChanged(Qt::Horizontal, 0, columns - 1);
}

}  // namespace QtRocket::Gui
