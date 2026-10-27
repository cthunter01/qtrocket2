#include "QtRocket/gui/dialogs/preset/ComponentPresetFilterModel.h"

#include <cmath>
#include <compare>
#include <optional>
#include <utility>
#include <vector>

#include <QModelIndex>
#include <QObject>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QString>
#include <Qt>

#include "QtRocket/gui/binding/PresetDiameterFilters.h"
#include "QtRocket/gui/dialogs/preset/ComponentPresetTableModel.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/TableSortKeys.h"
#include "QtRocket/rocket/preset/TypedKey.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::Gui
{

namespace
{

using TableModel = ComponentPresetTableModel;

/// The model column of @p key, -1 for none.
[[nodiscard]] int columnOf(const std::optional<AnyTypedKey>& key)
{
    return key.has_value() ? TableModel::columnOf(*key) : -1;
}

/// The options of the text filter's expression: OpenRocket's "(?iu)", case-insensitive for every
/// letter (the plan's decision U-M3-6e).
constexpr QRegularExpression::PatternOptions kTextOptions =
    QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption;

}  // namespace

double ComponentPresetFilterModel::diameterTolerance(double diameter) noexcept
{
    // Accept 5% difference, but at least 1mm (OpenRocket's comment).
    return MathUtil::max(diameter * 0.05, 0.001);
}

bool ComponentPresetFilterModel::matchesDiameter(double                diameter,
                                                 std::optional<double> value) noexcept
{
    return !value.has_value() || std::abs(diameter - *value) < diameterTolerance(diameter);
}

ComponentPresetFilterModel::ComponentPresetFilterModel(ComponentPresetTableModel& source,
                                                       QObject*                   parent)
  : QSortFilterProxyModel(parent),
    m_source(&source),
    m_sortKeys(TableModel::kColumnCount),
    m_text(QString(), kTextOptions),
    m_foreColumn(columnOf(PresetDiameterFilters::foreDiameterKey(source.presetType()))),
    m_aftColumn(columnOf(PresetDiameterFilters::aftDiameterKey(source.presetType())))
{
    setSourceModel(&source);
    // Sort by the first column (manufacturer) by default (OpenRocket's comment).
    m_sortKeys.toggle(TableModel::kManufacturerColumn);
    // The keys decide the whole order (lessThan()); Qt's sort column is only its trigger.
    sort(0, Qt::AscendingOrder);
    connect(&source, &TableModel::favoritesChanged, this, guarded([this] { resort(); }));
    connect(&source, &TableModel::unitChanged, this, guarded([this] { invalidateFilter(); }));
}

void ComponentPresetFilterModel::toggleSortOrder(int column)
{
    m_sortKeys.toggle(column);
    resort();
    Q_EMIT sortKeysChanged();
}

void ComponentPresetFilterModel::setSortKeys(std::vector<TableSortKeys::SortKey> keys)
{
    m_sortKeys.setKeys(std::move(keys));
    resort();
    Q_EMIT sortKeysChanged();
}

void ComponentPresetFilterModel::setTextFilter(const QString& pattern)
{
    m_text = QRegularExpression(pattern, kTextOptions);
    m_text.optimize();
    invalidateFilter();
}

void ComponentPresetFilterModel::setForeDiameterFilter(std::optional<double> diameter)
{
    if (diameter.has_value() && m_foreColumn < 0)
    {
        bug("ComponentPresetFilterModel: a fore diameter filter for a type without its column");
    }
    m_fore = diameter;
    invalidateFilter();
}

void ComponentPresetFilterModel::setAftDiameterFilter(std::optional<double> diameter)
{
    if (diameter.has_value() && m_aftColumn < 0)
    {
        bug("ComponentPresetFilterModel: an aft diameter filter for a type without its column");
    }
    m_aft = diameter;
    invalidateFilter();
}

void ComponentPresetFilterModel::setHideLegacy(bool hide)
{
    if (hide != m_hideLegacy)
    {
        m_hideLegacy = hide;
        invalidateFilter();
    }
}

int ComponentPresetFilterModel::modelRow(int row) const
{
    if (row < 0 || row >= rowCount())
    {
        return -1;
    }
    return mapToSource(index(row, 0)).row();
}

int ComponentPresetFilterModel::viewRow(int row) const
{
    if (row < 0 || row >= m_source->rowCount())
    {
        return -1;
    }
    return mapFromSource(m_source->index(row, 0)).row();
}

bool ComponentPresetFilterModel::acceptsRow(int row) const
{
    return row >= 0 && row < m_source->rowCount() && filterAcceptsRow(row, QModelIndex());
}

bool ComponentPresetFilterModel::filterAcceptsRow(int sourceRow,
                                                  const QModelIndex& /*sourceParent*/) const
{
    if (m_hideLegacy &&
        m_source->booleanValue(sourceRow, TableModel::kLegacyColumn).value_or(false))
    {
        return false;
    }
    if (m_fore.has_value() &&
        !matchesDiameter(*m_fore, m_source->numberValue(sourceRow, m_foreColumn)))
    {
        return false;
    }
    if (m_aft.has_value() &&
        !matchesDiameter(*m_aft, m_source->numberValue(sourceRow, m_aftColumn)))
    {
        return false;
    }
    return acceptsText(sourceRow);
}

bool ComponentPresetFilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    return std::is_lt(
        m_sortKeys.compareRows(left.row(), right.row(), [this](int column, int rowA, int rowB) {
            return m_source->compareCells(column, rowA, rowB);
        }));
}

bool ComponentPresetFilterModel::acceptsText(int row) const
{
    if (m_text.pattern().isEmpty())
    {
        return true;
    }
    if (!m_text.isValid())
    {
        return false;
    }
    for (int column = 0; column < TableModel::kColumnCount; ++column)
    {
        if (!TableModel::isBooleanColumn(column) &&
            m_text.match(m_source->cellText(row, column)).hasMatch())
        {
            return true;
        }
    }
    return false;
}

void ComponentPresetFilterModel::resort()
{
    invalidate();
}

}  // namespace QtRocket::Gui
