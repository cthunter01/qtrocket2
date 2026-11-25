#include "QtRocket/gui/dialogs/simulation/MultiLevelWindTableModel.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <QAbstractItemModel>
#include <QColor>
#include <QIcon>
#include <QList>
#include <QModelIndex>
#include <QObject>
#include <QPointer>
#include <QScopeGuard>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <Qt>

#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/ObjectSelection.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/dialogs/simulation/TurbulenceIntensityText.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/simulation/SimulationInputRanges.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::Gui
{

namespace
{

using Level = MultiLevelWindTableModel::Level;

/// Java's order of the levels (Comparator.comparingDouble(LevelWindModel::getAltitude)).
[[nodiscard]] bool lowerAltitude(const Level* a, const Level* b) noexcept
{
    return MathUtil::javaDoubleCompare(a->getAltitude(), b->getAltitude()) < 0;
}

/// Sorts @p model's levels by altitude when they are out of order (OpenRocket bug 32): whether
/// it did (MultiLevelPinkNoiseWindModel::sortLevels() announces nothing).
[[nodiscard]] bool sortLevelsOutOfOrder(MultiLevelPinkNoiseWindModel& model)
{
    if (const std::vector<Level*> levels = model.getLevels();
        std::ranges::is_sorted(levels, lowerAltitude))
    {
        return false;
    }
    model.sortLevels();
    return true;
}

[[nodiscard]] bool isNumberColumn(int column) noexcept
{
    return column >= 0 && column < MultiLevelWindTableModel::kNumberColumnCount;
}

/// Whether @p part is @p whole with some of its elements left out (the others in their order).
[[nodiscard]] bool isSubsequence(const std::vector<const Level*>& part,
                                 const std::vector<const Level*>& whole)
{
    std::size_t next = 0;
    for (const Level* const level : whole)
    {
        if (next < part.size() && part.at(next) == level)
        {
            ++next;
        }
    }
    return next == part.size();
}

/// Whether @p a and @p b list the same levels, in any order.
[[nodiscard]] bool sameLevels(std::vector<const Level*> a, std::vector<const Level*> b)
{
    std::ranges::sort(a, std::less<>());
    std::ranges::sort(b, std::less<>());
    return a == b;
}

/// A source of the level's property read by @p get and written by @p set (Java: DoubleModel(level,
/// "<Name>")).
[[nodiscard]] DoubleSource levelSource(double (Level::*get)() const, void (Level::*set)(double))
{
    return DoubleSource::ofObject<Level>({
        .get = [get](const Level& level) { return std::invoke(get, level); },
        .set = [set](Level& level, const double& value) { std::invoke(set, level, value); },
    });
}

/// The speed's binding of the level of @p selection: the deviation's dynamic maximum (Java:
/// DoubleModel(level, "StandardDeviation", ..., 0, dmSpeed)).
[[nodiscard]] std::shared_ptr<DoubleBinding> speedBinding(
    const std::shared_ptr<ObjectSelection>& selection)
{
    return std::make_shared<DoubleBinding>(
        selection, levelSource(&Level::getSpeed, &Level::setSpeed),
        MultiLevelWindTableModel::columnUnitGroup(MultiLevelWindTableModel::kSpeedColumn),
        DoubleBinding::Options{.min = 0, .max = SimulationInputRanges::kWindSpeed.max});
}

}  // namespace

MultiLevelWindTableModel::MultiLevelWindTableModel(MultiLevelPinkNoiseWindModel& model,
                                                   const Preferences& preferences, QObject* parent)
  : QAbstractTableModel(parent),
    m_model(&model),
    m_preferences(&preferences),
    m_reference(model.getAltitudeReference())
{
    for (std::size_t column = 0; column < m_units.size(); ++column)
    {
        m_units.at(column) = &columnUnitGroup(static_cast<int>(column)).getDefaultUnit();
    }
    // OpenRocket bug 32: the levels in order from the start (Java sorts its rows only), and the
    // model's listeners told of the new order (see announceSort()), before the table listens.
    if (sortLevelsOutOfOrder(model))
    {
        model.fireChangeEvent();
    }
    m_rows       = rowsOf(model.getLevels());
    m_connection = model.changed().connect([this] { followModel(); });
}

MultiLevelWindTableModel::~MultiLevelWindTableModel() = default;

// ------------------------------------------------------------------------------------ the model

int MultiLevelWindTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.ids.size());
}

int MultiLevelWindTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : kColumnCount;
}

QVariant MultiLevelWindTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.parent().isValid() || index.row() >= rowCount() ||
        index.column() >= kColumnCount)
    {
        return {};
    }
    const int row    = index.row();
    const int column = index.column();
    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::EditRole:
            if (column == kDeleteColumn || levelAt(row) == nullptr)
            {
                return {};
            }
            return cellText(row, column);
        case Qt::DecorationRole:
            if (column == kDeleteColumn)
            {
                return Icons::icon(Icons::Id::EDIT_DELETE);
            }
            return {};
        case Qt::ToolTipRole:
            if (column == kDeleteColumn)
            {
                //: MultiLevelWindTable.but.deleteWindLevel.ttip
                return tr("Delete the selected wind level");
            }
            return {};
        case Qt::BackgroundRole:
            return background(row);
        default:
            return {};
    }
}

QVariant MultiLevelWindTableModel::headerData(int section, Qt::Orientation orientation,
                                              int role) const
{
    if (orientation != Qt::Horizontal || section < 0 || section >= kColumnCount)
    {
        return {};
    }
    switch (role)
    {
        case Qt::DisplayRole:
            return columnName(section, m_reference);
        case Qt::ToolTipRole:
            return headerToolTip(section);
        case Qt::TextAlignmentRole:
            return QVariant::fromValue(headerAlignment(section));
        default:
            return {};
    }
}

Qt::Alignment MultiLevelWindTableModel::headerAlignment(int section)
{
    Qt::Alignment alignment = Qt::AlignCenter;
    if (isNumberColumn(section))
    {
        // The number columns' unit selectors go below their names (Java: a label above the
        // selector, in a column of the header cell).
        alignment = Qt::AlignHCenter | Qt::AlignTop;
    }
    return alignment;
}

Qt::ItemFlags MultiLevelWindTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return Qt::NoItemFlags;
    }
    // A row whose level has gone (with the model, or replaced without a word) is inert.
    const bool hasLevel = levelAt(index.row()) != nullptr;
    if (isNumberColumn(index.column()))
    {
        return hasLevel ? Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable
                        : Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }
    if (index.column() == kDeleteColumn && (rowCount() <= 1 || !hasLevel))
    {
        // Java: updateDeleteButtonsState(), the last level cannot be deleted.
        return Qt::ItemIsSelectable;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool MultiLevelWindTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::EditRole || !index.isValid() || !isNumberColumn(index.column()))
    {
        return false;
    }
    const std::shared_ptr<DoubleBinding> binding = makeCellBinding(index.row(), index.column());
    return binding != nullptr && binding->commitText(QtStrings::toUtf8(value.toString()));
}

// ------------------------------------------------------------------------------------- the rows

bool MultiLevelWindTableModel::hasModel() const noexcept
{
    return m_connection.connected();
}

MultiLevelWindTableModel::Level* MultiLevelWindTableModel::levelAt(int row) const
{
    if (row < 0 || row >= rowCount())
    {
        return nullptr;
    }
    return m_rows.levels.at(static_cast<std::size_t>(row)).lock().get();
}

int MultiLevelWindTableModel::rowOf(const Level* level) const
{
    if (level == nullptr)
    {
        return -1;
    }
    const auto found = std::ranges::find(m_rows.ids, level);
    return found == m_rows.ids.end()
               ? -1
               : static_cast<int>(std::ranges::distance(m_rows.ids.begin(), found));
}

MultiLevelWindTableModel::Rows MultiLevelWindTableModel::rowsOf(const std::vector<Level*>& levels)
{
    Rows rows;
    rows.levels.reserve(levels.size());
    rows.ids.reserve(levels.size());
    for (Level* const level : levels)
    {
        rows.levels.push_back(level->weak_from_this());
        rows.ids.push_back(level);
    }
    return rows;
}

void MultiLevelWindTableModel::followModel()
{
    // OpenRocket bug 32: an altitude edit (or anything else) that leaves the levels out of order
    // sorts them again, so that the wind is interpolated between the right levels.
    const bool sorted = sortLevelsOutOfOrder(*m_model);
    refresh();
    if (sorted)
    {
        announceSort();
    }
}

void MultiLevelWindTableModel::announceSort()
{
    // The model's listeners connected before the table (a SimulationOptions forwards the model's
    // changes to the launch conditions, whose rod direction follows the wind at the launch site)
    // heard of the change while the levels were still out of order: the new order is a change of
    // its own. During the table's own altitude edit it is announced once the level's setter has
    // returned (setLevelAltitude()); otherwise at once, within the model's announcement, where
    // the table finds the levels sorted and only refreshes.
    if (m_movingLevel != nullptr)
    {
        m_sortUnannounced = true;
        return;
    }
    m_model->fireChangeEvent();
}

void MultiLevelWindTableModel::refresh()
{
    // The same levels in the same order need no new rows: their values may have changed.
    if (Rows next = rowsOf(m_model->getLevels()); next.ids != m_rows.ids)
    {
        takeRows(std::move(next));
    }
    announceAll();
    Q_EMIT levelsChanged();
}

void MultiLevelWindTableModel::takeRows(Rows next)
{
    if (next.ids.size() == m_rows.ids.size() && sameLevels(next.ids, m_rows.ids))
    {
        reorder(std::move(next));
    }
    else if (next.ids.size() < m_rows.ids.size() && isSubsequence(next.ids, m_rows.ids))
    {
        removeMissing(std::move(next));
    }
    else if (next.ids.size() > m_rows.ids.size() && isSubsequence(m_rows.ids, next.ids))
    {
        insertNew(std::move(next));
    }
    else
    {
        beginResetModel();
        m_rows = std::move(next);
        endResetModel();
    }
}

void MultiLevelWindTableModel::reorder(Rows next)
{
    Q_EMIT layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);
    std::unordered_map<const Level*, int> rowOfLevel;
    for (std::size_t row = 0; row < next.ids.size(); ++row)
    {
        rowOfLevel.emplace(next.ids.at(row), static_cast<int>(row));
    }
    const QModelIndexList from = persistentIndexList();
    QModelIndexList       to;
    to.reserve(from.size());
    for (const QModelIndex& persistent : from)
    {
        const auto found =
            rowOfLevel.find(m_rows.ids.at(static_cast<std::size_t>(persistent.row())));
        to.append(found == rowOfLevel.end() ? QModelIndex()
                                            : createIndex(found->second, persistent.column()));
    }
    m_rows = std::move(next);
    changePersistentIndexList(from, to);
    Q_EMIT layoutChanged({}, QAbstractItemModel::VerticalSortHint);
    // Java: the rows around a level an altitude edit moved flash.
    if (const int row = rowOf(m_movingLevel); row >= 0)
    {
        flashRows(row - 1, row + 1);
    }
}

void MultiLevelWindTableModel::removeMissing(Rows next)
{
    std::vector<int> removed;
    std::size_t      kept = 0;
    for (std::size_t row = 0; row < m_rows.ids.size(); ++row)
    {
        if (kept < next.ids.size() && m_rows.ids.at(row) == next.ids.at(kept))
        {
            ++kept;
        }
        else
        {
            removed.push_back(static_cast<int>(row));
        }
    }
    // Each run of rows at once, from the last.
    while (!removed.empty())
    {
        const int last  = removed.back();
        int       first = last;
        removed.pop_back();
        while (!removed.empty() && removed.back() == first - 1)
        {
            first = removed.back();
            removed.pop_back();
        }
        beginRemoveRows({}, first, last);
        const auto begin = static_cast<std::ptrdiff_t>(first);
        const auto end   = static_cast<std::ptrdiff_t>(last) + 1;
        m_rows.levels.erase(m_rows.levels.begin() + begin, m_rows.levels.begin() + end);
        m_rows.ids.erase(m_rows.ids.begin() + begin, m_rows.ids.begin() + end);
        endRemoveRows();
    }
}

void MultiLevelWindTableModel::insertNew(Rows next)
{
    std::size_t row = 0;
    std::size_t at  = 0;
    while (at < next.ids.size())
    {
        if (row < m_rows.ids.size() && m_rows.ids.at(row) == next.ids.at(at))
        {
            ++row;
            ++at;
            continue;
        }
        // A run of new levels before the row's level (or at the end).
        std::size_t end = at;
        while (end < next.ids.size() &&
               (row >= m_rows.ids.size() || next.ids.at(end) != m_rows.ids.at(row)))
        {
            ++end;
        }
        beginInsertRows({}, static_cast<int>(row), static_cast<int>(row + (end - at)) - 1);
        const auto first = static_cast<std::ptrdiff_t>(at);
        const auto last  = static_cast<std::ptrdiff_t>(end);
        const auto place = static_cast<std::ptrdiff_t>(row);
        m_rows.levels.insert(m_rows.levels.begin() + place, next.levels.begin() + first,
                             next.levels.begin() + last);
        m_rows.ids.insert(m_rows.ids.begin() + place, next.ids.begin() + first,
                          next.ids.begin() + last);
        endInsertRows();
        row += end - at;
        at = end;
    }
}

void MultiLevelWindTableModel::announceAll()
{
    if (rowCount() > 0)
    {
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, kColumnCount - 1));
    }
    if (m_model->getAltitudeReference() != m_reference)
    {
        m_reference = m_model->getAltitudeReference();
        Q_EMIT headerDataChanged(Qt::Horizontal, kAltitudeColumn, kAltitudeColumn);
    }
}

// ------------------------------------------------------------------------------------- the cells

QString MultiLevelWindTableModel::columnName(int column, WindModel::AltitudeReference reference)
{
    switch (column)
    {
        case kAltitudeColumn:
            if (reference == WindModel::AltitudeReference::AGL)
            {
                //: MultiLevelWindTable.col.AltitudeAGL
                return tr("Altitude AGL");
            }
            //: MultiLevelWindTable.col.AltitudeMSL
            return tr("Altitude MSL");
        case kSpeedColumn:
            //: MultiLevelWindTable.col.Speed
            return tr("Speed");
        case kDirectionColumn:
            //: MultiLevelWindTable.col.Direction
            return tr("Direction");
        case kDeviationColumn:
            //: MultiLevelWindTable.col.StandardDeviation
            return tr("Deviation");
        case kTurbulenceColumn:
            //: MultiLevelWindTable.col.Turbulence
            return tr("Turbulence");
        case kIntensityColumn:
            //: MultiLevelWindTable.col.Intensity
            return tr("Intensity");
        case kDeleteColumn:
            //: MultiLevelWindTable.col.Delete
            return tr("Delete");
        default:
            bug("The wind levels' table has no column " + std::to_string(column));
    }
}

QVariant MultiLevelWindTableModel::headerToolTip(int section) const
{
    if (section == kAltitudeColumn)
    {
        if (m_reference == WindModel::AltitudeReference::AGL)
        {
            //: MultiLevelWindTable.col.AltitudeAGL.ttip
            return tr("Altitude above ground level (the launch site)");
        }
        //: MultiLevelWindTable.col.AltitudeMSL.ttip
        return tr("Altitude above mean sea level");
    }
    if (section == kDirectionColumn)
    {
        //: MultiLevelWindTable.col.Direction.ttip
        return tr(
            "<html>Wind direction.<br>0 degrees is wind coming North, blowing South,<br>90 "
            "degrees is coming East,<br>180 degrees is coming South,<br>and 270 degrees is "
            "coming West.</html>");
    }
    return {};
}

const UnitGroup& MultiLevelWindTableModel::columnUnitGroup(int column)
{
    switch (column)
    {
        case kAltitudeColumn:
            return unitGroup(UnitGroupId::DISTANCE);
        case kSpeedColumn:
        case kDeviationColumn:
            return unitGroup(UnitGroupId::WINDSPEED);
        case kDirectionColumn:
            return unitGroup(UnitGroupId::ANGLE);
        case kTurbulenceColumn:
            return unitGroup(UnitGroupId::RELATIVE);
        default:
            bug("The column " + std::to_string(column) + " of the wind levels' table has no unit");
    }
}

const Unit& MultiLevelWindTableModel::columnUnit(int column) const
{
    if (!isNumberColumn(column))
    {
        bug("The column " + std::to_string(column) + " of the wind levels' table has no unit");
    }
    return *m_units.at(static_cast<std::size_t>(column));
}

void MultiLevelWindTableModel::setColumnUnit(int column, const Unit& unit)
{
    const UnitGroup& group = columnUnitGroup(column);
    if (!group.contains(unit))
    {
        bug("The unit " + unit.getUnit() + " is not one of the column " + std::to_string(column) +
            " of the wind levels' table");
    }
    m_units.at(static_cast<std::size_t>(column)) = &group.getUnit(group.getUnitIndex(unit));
    if (rowCount() > 0)
    {
        Q_EMIT dataChanged(index(0, column), index(rowCount() - 1, column));
    }
    Q_EMIT columnUnitChanged(column);
}

QString MultiLevelWindTableModel::cellText(int row, int column) const
{
    const Level* const level = levelAt(row);
    if (level == nullptr)
    {
        return {};
    }
    switch (column)
    {
        case kAltitudeColumn:
            return QtStrings::unitText(columnUnit(column), level->getAltitude());
        case kSpeedColumn:
            return QtStrings::unitText(columnUnit(column), level->getSpeed());
        case kDirectionColumn:
            return QtStrings::unitText(columnUnit(column), level->getDirection());
        case kDeviationColumn:
            return QtStrings::unitText(columnUnit(column), level->getStandardDeviation());
        case kTurbulenceColumn:
            return QtStrings::unitText(columnUnit(column), level->getTurbulenceIntensity());
        case kIntensityColumn:
            return TurbulenceIntensityText::text(level->getIntensityDescriptionKey());
        default:
            return {};
    }
}

QVariant MultiLevelWindTableModel::background(int row) const
{
    if (isFlashing(row))
    {
        return ThemeColors::color(ThemeColors::Key::TABLE_FLASH);
    }
    // Java: the even rows OR.colors.row.lighter, the odd ones OR.colors.row.darker.
    return ThemeColors::color(row % 2 == 0 ? ThemeColors::Key::ROW_LIGHTER
                                           : ThemeColors::Key::ROW_DARKER);
}

std::shared_ptr<DoubleBinding> MultiLevelWindTableModel::makeCellBinding(int row, int column)
{
    Level* const level = levelAt(row);
    if (level == nullptr || !isNumberColumn(column) || !hasModel())
    {
        return nullptr;
    }
    const std::shared_ptr<ObjectSelection> selection = ObjectSelection::make<Level>(*level);
    const UnitGroup&                       units     = columnUnitGroup(column);
    std::shared_ptr<DoubleBinding>         binding;
    switch (column)
    {
        case kAltitudeColumn:
        {
            // OpenRocket bug 31: the table sets the altitude, refusing another level's.
            DoubleSource altitude = DoubleSource::ofObject<Level>({
                .get = [](const Level& edited) { return edited.getAltitude(); },
                .set =
                    [table = QPointer<MultiLevelWindTableModel>(this)](Level&        edited,
                                                                       const double& value) {
                        if (!table.isNull())
                        {
                            table->setLevelAltitude(edited, value);
                        }
                    },
            });
            binding               = std::make_shared<DoubleBinding>(
                selection, std::move(altitude), units,
                DoubleBinding::Options{.min = kMinimumAltitude,
                                       .max = SimulationInputRanges::kWindLevelAltitude.max});
            break;
        }
        case kSpeedColumn:
            binding = speedBinding(selection);
            break;
        case kDirectionColumn:
            binding = std::make_shared<DoubleBinding>(
                selection, levelSource(&Level::getDirection, &Level::setDirection), units,
                DoubleBinding::Options{.min = 0, .max = 2 * std::numbers::pi});
            break;
        case kDeviationColumn:
            binding = std::make_shared<DoubleBinding>(
                selection, levelSource(&Level::getStandardDeviation, &Level::setStandardDeviation),
                units, DoubleBinding::Options{.min = 0, .maxBinding = speedBinding(selection)});
            break;
        default:
            binding = std::make_shared<DoubleBinding>(
                selection,
                levelSource(&Level::getTurbulenceIntensity, &Level::setTurbulenceIntensity), units,
                DoubleBinding::Options{.min = 0, .max = 1});
            break;
    }
    binding->setCurrentUnit(columnUnit(column));
    return binding;
}

void MultiLevelWindTableModel::setLevelAltitude(Level& level, double altitude)
{
    if (!hasModel())
    {
        return;
    }
    // OpenRocket bug 31: Java takes any altitude, also one another level has; the two levels then
    // go together on a delete (by altitude), and the design saved with them cannot be opened.
    const std::vector<const Level*> levels = std::as_const(*m_model).getLevels();
    if (std::ranges::any_of(levels, [&level, altitude](const Level* other) {
            return other != &level && other->getAltitude() == altitude;
        }))
    {
        return;
    }
    m_sortUnannounced = false;
    {
        m_movingLevel     = &level;
        const auto moving = qScopeGuard([this] { m_movingLevel = nullptr; });
        level.setAltitude(altitude);
    }
    // OpenRocket bug 32: the sort the edit led to, told to the model's listeners (announceSort()).
    if (std::exchange(m_sortUnannounced, false) && hasModel())
    {
        m_model->fireChangeEvent();
    }
}

// ------------------------------------------------------------------------------------ the actions

int MultiLevelWindTableModel::addLevel()
{
    if (!hasModel())
    {
        return -1;
    }
    const std::vector<const Level*> levels    = std::as_const(*m_model).getLevels();
    double                          altitude  = 0;
    double                          speed     = 0;
    double                          direction = 0;
    double                          deviation = 0;
    if (levels.empty())
    {
        // Java: prefs.getAverageWindModel().getAverage(), direction and deviation 0.
        PinkNoiseWindModel average;
        average.loadFrom(*m_preferences);
        speed = average.getAverage();
    }
    else
    {
        // Java: the highest altitude (DoubleStream.max()) plus ALTITUDE_INCREASE, and the values of
        // the last row, the highest level.
        double highest = -std::numeric_limits<double>::infinity();
        for (const Level* const level : levels)
        {
            highest = MathUtil::javaMax(highest, level->getAltitude());
        }
        altitude          = highest + kAltitudeIncrease;
        const Level& last = *levels.back();
        speed             = last.getSpeed();
        direction         = last.getDirection();
        deviation         = last.getStandardDeviation();
    }
    if (!SimulationInputRanges::kWindLevelAltitude.contains(altitude) ||
        !m_model->addWindLevel(altitude, speed, direction, deviation).has_value())
    {
        return -1;
    }
    // Java: the level whose altitude is within 1e-6 of the new one, the first in the model.
    const auto added = std::ranges::find_if(m_rows.ids, [altitude](const Level* level) {
        return std::abs(level->getAltitude() - altitude) < 1e-6;
    });
    if (added == m_rows.ids.end())
    {
        return -1;
    }
    const int row = static_cast<int>(std::ranges::distance(m_rows.ids.begin(), added));
    flashRows(row, row);
    return row;
}

bool MultiLevelWindTableModel::deleteLevel(int row)
{
    const Level* const level = levelAt(row);
    if (!hasModel() || level == nullptr || rowCount() <= 1)
    {
        return false;
    }
    // OpenRocket bug 31: by its index (Java's removeWindLevel(altitude) removes every level there).
    const std::vector<const Level*> levels = std::as_const(*m_model).getLevels();
    const auto                      found  = std::ranges::find(levels, level);
    if (found == levels.end())
    {
        return false;
    }
    m_model->removeWindLevelIdx(
        static_cast<std::size_t>(std::ranges::distance(levels.begin(), found)));
    // Java: the row now at the deleted row's place flashes.
    if (row < rowCount())
    {
        flashRows(row, row);
    }
    return true;
}

void MultiLevelWindTableModel::resetLevels()
{
    if (hasModel())
    {
        m_model->resetLevels(*m_preferences);
    }
}

// ------------------------------------------------------------------------------------ the flashes

bool MultiLevelWindTableModel::isFlashing(int row) const
{
    const Level* const level = levelAt(row);
    return level != nullptr &&
           std::ranges::any_of(m_flashing, [level](const std::weak_ptr<Level>& flashing) {
               return flashing.lock().get() == level;
           });
}

void MultiLevelWindTableModel::flashRows(int first, int last)
{
    first = std::max(first, 0);
    last  = std::min(last, rowCount() - 1);
    for (int row = first; row <= last; ++row)
    {
        const std::weak_ptr<Level>& level = m_rows.levels.at(static_cast<std::size_t>(row));
        m_flashing.push_back(level);
        QTimer::singleShot(m_flashDuration, this, [this, level] { endFlash(level); });
    }
    if (first <= last)
    {
        Q_EMIT dataChanged(index(first, 0), index(last, kColumnCount - 1), {Qt::BackgroundRole});
    }
}

void MultiLevelWindTableModel::endFlash(const std::weak_ptr<Level>& level)
{
    const auto found =
        std::ranges::find_if(m_flashing, [&level](const std::weak_ptr<Level>& flashing) {
            return !flashing.owner_before(level) && !level.owner_before(flashing);
        });
    if (found == m_flashing.end())
    {
        return;
    }
    m_flashing.erase(found);
    if (const int row = rowOf(level.lock().get()); row >= 0)
    {
        Q_EMIT dataChanged(index(row, 0), index(row, kColumnCount - 1), {Qt::BackgroundRole});
    }
}

}  // namespace QtRocket::Gui
