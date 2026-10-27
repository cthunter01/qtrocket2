#include "QtRocket/gui/dialogs/preset/ComponentPresetTableModel.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <QAbstractTableModel>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QVariant>
#include <Qt>

#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/TableSortKeys.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFavorites.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedKey.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/AlphanumComparator.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/JavaCollator.h"

namespace QtRocket::Gui
{

namespace
{

using Model = ComponentPresetTableModel;

/// The English headers (messages.properties' table.column.<key name>), by column.
constexpr std::array<std::string_view, Model::kColumnCount> kIdentifiers{
    "\u2026 Favorite",
    "Legacy",
    "Manufacturer",
    "Part Number",
    "Description",
    "Outer Diameter",
    "Fore Outer Diameter",
    "Aft Outer Diameter",
    "Inner Diameter",
    "Length",
    "Width",
    "Aft Shoulder Diameter",
    "Aft Shoulder Length",
    "Fore Shoulder Diameter",
    "Fore Shoulder Length",
    "Base Height",
    "Flange Height",
    "Screw Height",
    "Shape",
    "Thickness",
    "Filled",
    "Diameter",
    "Sides",
    "Line Count",
    "Line Length",
    "Line Material",
    "Mass",
    "Screw Mass",
    "Nut Mass",
    "Finish",
    "Material",
};

/// @throws BugError unless @p column is one of the model's
void checkColumn(int column)
{
    if (column < 0 || column >= Model::kColumnCount)
    {
        bug("ComponentPresetTableModel: no column " + std::to_string(column));
    }
}

/// The key of a key column (1 to 30).
[[nodiscard]] const AnyTypedKey& keyAt(int column)
{
    return ComponentPreset::orderedKeyList()[static_cast<std::size_t>(column - 1)];
}

/// Java's Double.compare(a, b): the numeric order, -0.0 before 0.0, every NaN equal to every
/// other and after everything else (Double.compareTo's documented order).
[[nodiscard]] std::weak_ordering javaDoubleCompare(double a, double b) noexcept
{
    if (a < b)
    {
        return std::weak_ordering::less;
    }
    if (a > b)
    {
        return std::weak_ordering::greater;
    }
    // Equal by value, or a NaN: doubleToLongBits() as signed longs (every NaN the canonical one).
    const auto bits = [](double value) {
        constexpr std::int64_t kCanonicalNaN = 0x7ff8000000000000LL;
        return std::isnan(value) ? kCanonicalNaN : std::bit_cast<std::int64_t>(value);
    };
    return bits(a) <=> bits(b);
}

/// Boolean.compareTo(): false before true.
[[nodiscard]] std::weak_ordering booleanOrder(bool a, bool b) noexcept
{
    if (a == b)
    {
        return std::weak_ordering::equivalent;
    }
    return a ? std::weak_ordering::greater : std::weak_ordering::less;
}

/// The index of the cell @p row, @p column in the text cache.
[[nodiscard]] std::size_t cacheIndex(int row, int column) noexcept
{
    return (static_cast<std::size_t>(row) * static_cast<std::size_t>(Model::kColumnCount)) +
           static_cast<std::size_t>(column);
}

/// The text of a cell's value (Java: the value's toString()).
struct ValueText
{
    const Unit* unit;

    QString operator()(bool value) const
    {
        return value ? QStringLiteral("true") : QStringLiteral("false");
    }
    QString operator()(int value) const { return QString::number(value); }
    QString operator()(double value) const
    {
        // Every double key of ORDERED_KEY_LIST has a unit group (a DoubleWithUnit column, a Value
        // shown in its unit); a Parameter column would show Double.toString().
        return unit != nullptr ? QtStrings::unitTextWithUnit(*unit, value)
                               : QtStrings::javaDoubleText(value);
    }
    QString operator()(const std::string& value) const { return QtStrings::fromUtf8(value); }
    QString operator()(const ManufacturerRef& value) const
    {
        return QtStrings::fromUtf8(value.get().getDisplayName());
    }
    QString operator()(ComponentPresetType value) const
    {
        return QtStrings::fromUtf8(componentPresetTypeName(value));
    }
    QString operator()(TransitionShape value) const { return enumText(value); }
    QString operator()(const Material& value) const
    {
        return QtStrings::fromUtf8(value.toString());
    }
    QString operator()(Finish value) const { return MaterialPanel::finishText(value); }
    // The image is no column (not in ORDERED_KEY_LIST).
    QString operator()(const std::vector<std::byte>& /*value*/) const { return {}; }
};

/// A column's value of a preset, nullptr without one; @p column is a key column.
[[nodiscard]] const TypedValue* valueOf(const ComponentPreset& preset, int column)
{
    return preset.getProperties().getValue(keyAt(column));
}

/// The value of type @p T of a key column of @p preset, nullopt without one or of another type.
template <class T>
[[nodiscard]] std::optional<T> typedValue(const ComponentPreset& preset, int column)
{
    const TypedValue* value = valueOf(preset, column);
    if (value == nullptr)
    {
        return std::nullopt;
    }
    const T* typed = std::get_if<T>(value);
    return typed == nullptr ? std::nullopt : std::optional<T>(*typed);
}

}  // namespace

ComponentPresetTableModel::ComponentPresetTableModel(ComponentPresetType    type,
                                                     std::vector<PresetPtr> presets,
                                                     Preferences& preferences, QObject* parent)
  : QAbstractTableModel(parent),
    m_type(type),
    m_presets(std::move(presets)),
    m_preferences(&preferences),
    m_favorites(m_presets.size(), false),
    m_texts(m_presets.size() * static_cast<std::size_t>(kColumnCount))
{
    m_preferenceKeys.reserve(m_presets.size());
    for (const PresetPtr& preset : m_presets)
    {
        if (preset == nullptr || preset->getType() != m_type)
        {
            bug("ComponentPresetTableModel: a null preset or one of another type");
        }
        m_preferenceKeys.push_back(preset->preferenceKey());
    }
    for (int column = 0; column < kColumnCount; ++column)
    {
        if (const std::optional<UnitGroupId> group = columnUnitGroup(column); group.has_value())
        {
            m_units.at(static_cast<std::size_t>(column)) = &unitGroup(*group).getDefaultUnit();
        }
    }
    const ComponentPresetFavorites::FavoriteKeys favorites =
        ComponentPresetFavorites::getFavorites(*m_preferences, m_type);
    for (std::size_t row = 0; row < m_presets.size(); ++row)
    {
        m_favorites[row] = favorites.contains(m_preferenceKeys[row]);
    }
}

int ComponentPresetTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_presets.size());
}

int ComponentPresetTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : kColumnCount;
}

QVariant ComponentPresetTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount() || index.column() >= kColumnCount)
    {
        return {};
    }
    if (index.column() == kFavoriteColumn)
    {
        if (role == Qt::CheckStateRole)
        {
            return static_cast<int>(isFavorite(index.row()) ? Qt::Checked : Qt::Unchecked);
        }
        return {};
    }
    if (role == Qt::DisplayRole && hasValue(index.row(), index.column()))
    {
        return cellText(index.row(), index.column());
    }
    return {};
}

QVariant ComponentPresetTableModel::headerData(int section, Qt::Orientation orientation,
                                               int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 ||
        section >= kColumnCount)
    {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    return columnName(section);
}

Qt::ItemFlags ComponentPresetTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return Qt::NoItemFlags;
    }
    Qt::ItemFlags result = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == kFavoriteColumn)
    {
        result |= Qt::ItemIsUserCheckable;
    }
    return result;
}

bool ComponentPresetTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.column() != kFavoriteColumn || role != Qt::CheckStateRole ||
        index.row() >= rowCount())
    {
        return false;
    }
    setFavorite(index.row(), value.toInt() == static_cast<int>(Qt::Checked));
    return true;
}

std::optional<AnyTypedKey> ComponentPresetTableModel::columnKey(int column)
{
    checkColumn(column);
    if (column == kFavoriteColumn)
    {
        return std::nullopt;
    }
    return keyAt(column);
}

int ComponentPresetTableModel::columnOf(const AnyTypedKey& key)
{
    const std::span<const AnyTypedKey> keys  = ComponentPreset::orderedKeyList();
    const auto                         found = std::ranges::find(keys, key);
    return found == keys.end() ? -1 : static_cast<int>(found - keys.begin()) + 1;
}

bool ComponentPresetTableModel::isDisplayed(ComponentPresetType type, int column)
{
    checkColumn(column);
    if (column == kFavoriteColumn)
    {
        return true;
    }
    const std::span<const AnyTypedKey> displayed = ComponentPreset::displayedColumns(type);
    return std::ranges::find(displayed, keyAt(column)) != displayed.end();
}

bool ComponentPresetTableModel::isBooleanColumn(int column)
{
    checkColumn(column);
    return column == kFavoriteColumn || keyAt(column).hasType<bool>();
}

std::string_view ComponentPresetTableModel::columnIdentifier(int column)
{
    checkColumn(column);
    return kIdentifiers.at(static_cast<std::size_t>(column));
}

std::optional<UnitGroupId> ComponentPresetTableModel::columnUnitGroup(int column)
{
    checkColumn(column);
    if (column == kFavoriteColumn || !keyAt(column).hasType<double>())
    {
        return std::nullopt;
    }
    return keyAt(column).getUnitGroup();
}

QString ComponentPresetTableModel::columnName(int column)
{
    checkColumn(column);
    switch (column)
    {
        case 0:
            //: table.column.Favorite
            return tr("\u2026 Favorite");
        case 1:
            //: table.column.Legacy
            return tr("Legacy");
        case 2:
            //: table.column.Manufacturer
            return tr("Manufacturer");
        case 3:
            //: table.column.PartNo
            return tr("Part Number");
        case 4:
            //: table.column.Description
            return tr("Description");
        case 5:
            //: table.column.OuterDiameter
            return tr("Outer Diameter");
        case 6:
            //: table.column.ForeOuterDiameter
            return tr("Fore Outer Diameter");
        case 7:
            //: table.column.AftOuterDiameter
            return tr("Aft Outer Diameter");
        case 8:
            //: table.column.InnerDiameter
            return tr("Inner Diameter");
        case 9:
            //: table.column.Length
            return tr("Length");
        case 10:
            //: table.column.Width
            return tr("Width");
        case 11:
            //: table.column.AftShoulderDiameter
            return tr("Aft Shoulder Diameter");
        case 12:
            //: table.column.AftShoulderLength
            return tr("Aft Shoulder Length");
        case 13:
            //: table.column.ForeShoulderDiameter
            return tr("Fore Shoulder Diameter");
        case 14:
            //: table.column.ForeShoulderLength
            return tr("Fore Shoulder Length");
        case 15:
            //: table.column.BaseHeight
            return tr("Base Height");
        case 16:
            //: table.column.FlangeHeight
            return tr("Flange Height");
        case 17:
            //: table.column.ScrewHeight
            return tr("Screw Height");
        case 18:
            //: table.column.Shape
            return tr("Shape");
        case 19:
            //: table.column.Thickness
            return tr("Thickness");
        case 20:
            //: table.column.Filled
            return tr("Filled");
        case 21:
            //: table.column.Diameter
            return tr("Diameter");
        case 22:
            //: table.column.Sides
            return tr("Sides");
        case 23:
            //: table.column.LineCount
            return tr("Line Count");
        case 24:
            //: table.column.LineLength
            return tr("Line Length");
        case 25:
            //: table.column.LineMaterial
            return tr("Line Material");
        case 26:
            //: table.column.Mass
            return tr("Mass");
        case 27:
            //: table.column.ScrewMass
            return tr("Screw Mass");
        case 28:
            //: table.column.NutMass
            return tr("Nut Mass");
        case 29:
            //: table.column.Finish
            return tr("Finish");
        default:
            //: table.column.Material
            return tr("Material");
    }
}

const Unit* ComponentPresetTableModel::columnUnit(int column) const
{
    checkColumn(column);
    return m_units.at(static_cast<std::size_t>(column));
}

void ComponentPresetTableModel::setColumnUnit(int column, const Unit& unit)
{
    const std::optional<UnitGroupId> group = columnUnitGroup(column);
    if (!group.has_value() || !unitGroup(*group).contains(unit))
    {
        bug("ComponentPresetTableModel: a unit for a column without its unit group");
    }
    m_units.at(static_cast<std::size_t>(column)) = &unit;
    for (std::size_t row = 0; row < m_presets.size(); ++row)
    {
        m_texts[cacheIndex(static_cast<int>(row), column)].reset();
    }
    if (!m_presets.empty())
    {
        Q_EMIT dataChanged(index(0, column), index(rowCount() - 1, column));
    }
    Q_EMIT unitChanged(column);
}

const ComponentPresetTableModel::PresetPtr& ComponentPresetTableModel::preset(int row) const
{
    checkRow(row);
    return m_presets[static_cast<std::size_t>(row)];
}

bool ComponentPresetTableModel::hasValue(int row, int column) const
{
    checkRow(row);
    checkColumn(column);
    return column == kFavoriteColumn || valueOf(presetAt(row), column) != nullptr;
}

QString ComponentPresetTableModel::cellText(int row, int column) const
{
    checkRow(row);
    checkColumn(column);
    std::optional<QString>& text = m_texts[cacheIndex(row, column)];
    if (!text.has_value())
    {
        text = computeText(row, column);
    }
    return *text;
}

std::optional<double> ComponentPresetTableModel::numberValue(int row, int column) const
{
    checkRow(row);
    checkColumn(column);
    if (!columnUnitGroup(column).has_value())
    {
        return std::nullopt;
    }
    return typedValue<double>(presetAt(row), column);
}

std::optional<bool> ComponentPresetTableModel::booleanValue(int row, int column) const
{
    checkRow(row);
    checkColumn(column);
    if (column == kFavoriteColumn)
    {
        return isFavorite(row);
    }
    if (!isBooleanColumn(column))
    {
        return std::nullopt;
    }
    return typedValue<bool>(presetAt(row), column);
}

std::weak_ordering ComponentPresetTableModel::compareCells(int column, int rowA, int rowB) const
{
    checkRow(rowA);
    checkRow(rowB);
    checkColumn(column);
    if (column == kFavoriteColumn)
    {
        return booleanOrder(isFavorite(rowA), isFavorite(rowB));
    }
    const AnyTypedKey&     key = keyAt(column);
    const ComponentPreset& a   = presetAt(rowA);
    const ComponentPreset& b   = presetAt(rowB);
    if (key == ComponentPreset::kPartNo)
    {
        return TableSortKeys::compareMissingFirst(typedValue<std::string>(a, column),
                                                  typedValue<std::string>(b, column),
                                                  [](const std::string& x, const std::string& y) {
                                                      return AlphanumComparator::compare(x, y);
                                                  });
    }
    if (key.hasType<double>())
    {
        return TableSortKeys::compareMissingFirst(typedValue<double>(a, column),
                                                  typedValue<double>(b, column), javaDoubleCompare);
    }
    if (key.hasType<int>())
    {
        return TableSortKeys::compareMissingFirst(typedValue<int>(a, column),
                                                  typedValue<int>(b, column),
                                                  [](int x, int y) { return x <=> y; });
    }
    if (key.hasType<bool>())
    {
        return TableSortKeys::compareMissingFirst(typedValue<bool>(a, column),
                                                  typedValue<bool>(b, column), booleanOrder);
    }
    // Collator.getInstance() on the texts (TableRowSorter's useToString: "" for null).
    return JavaCollator::compare(QtStrings::toUtf8(cellText(rowA, column)),
                                 QtStrings::toUtf8(cellText(rowB, column))) <=> 0;
}

bool ComponentPresetTableModel::isFavorite(int row) const
{
    checkRow(row);
    return m_favorites[static_cast<std::size_t>(row)];
}

void ComponentPresetTableModel::setFavorite(int row, bool favorite)
{
    checkRow(row);
    ComponentPresetFavorites::setFavorite(*m_preferences, presetAt(row), m_type, favorite);
    updateFavorites();
    Q_EMIT favoriteSet(row);
}

void ComponentPresetTableModel::updateFavorites()
{
    const ComponentPresetFavorites::FavoriteKeys favorites =
        ComponentPresetFavorites::getFavorites(*m_preferences, m_type);
    for (std::size_t row = 0; row < m_presets.size(); ++row)
    {
        m_favorites[row] = favorites.contains(m_preferenceKeys[row]);
        m_texts[cacheIndex(static_cast<int>(row), kFavoriteColumn)].reset();
    }
    if (!m_presets.empty())
    {
        Q_EMIT dataChanged(index(0, kFavoriteColumn), index(rowCount() - 1, kFavoriteColumn));
    }
    Q_EMIT favoritesChanged();
}

const ComponentPreset& ComponentPresetTableModel::presetAt(int row) const
{
    return *m_presets[static_cast<std::size_t>(row)];
}

void ComponentPresetTableModel::checkRow(int row) const
{
    if (row < 0 || row >= rowCount())
    {
        bug("ComponentPresetTableModel: no row " + std::to_string(row));
    }
}

QString ComponentPresetTableModel::computeText(int row, int column) const
{
    if (column == kFavoriteColumn)
    {
        return ValueText{nullptr}(isFavorite(row));
    }
    const TypedValue* value = valueOf(presetAt(row), column);
    if (value == nullptr)
    {
        return {};
    }
    return std::visit(ValueText{columnUnit(column)}, *value);
}

}  // namespace QtRocket::Gui
