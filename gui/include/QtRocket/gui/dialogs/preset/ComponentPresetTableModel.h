#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QAbstractTableModel>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QVariant>
#include <Qt>

#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedKey.h"
#include "QtRocket/unit/UnitGroup.h"

namespace QtRocket
{
class ComponentPreset;
class Preferences;
class Unit;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The model of the parts library's table (OpenRocket: ComponentPresetTable's AbstractTableModel
/// and its columns, ComponentPresetTableColumn, swing gui/dialogs/preset/ComponentPresetTable.java
/// and ComponentPresetTableColumn.java): a row per preset of one preset type, in the order given
/// (the database's, ComponentPresetDatabase::listForType()), and 31 columns: column 0 the
/// favourite ("\u2026 Favorite", a check box), then a column per key of
/// ComponentPreset::orderedKeyList() (ORDERED_KEY_LIST, columnKey(): Legacy, Manufacturer, Part
/// Number, ... Material), whatever the type displays. Which of them the table shows is the table's
/// (isDisplayed(), ComponentPresetTable); the others stay in the model, where the text filter
/// searches them.
///
/// A cell's text (cellText(), the display role) is its value's toString() in Java, which the
/// table's renderer shows and the text filter searches (TableRowSorter's string value): empty
/// for a preset without the key; "true" or "false" for a Boolean (Legacy, Filled; never
/// translated, as in Java); the number for an Integer (Sides, Line Count); a length or mass in
/// its column's unit with the unit ("2.48 cm": Unit::toStringUnit()); the manufacturer's display
/// name; the shape's name; a material's name with its density in the default unit
/// (Material::toString()); a finish's name with its roughness. The favourite column shows a check
/// box (the check state role); ticking it stores the preset as a favourite of the table's type
/// (setFavorite(): ComponentPresetFavorites::setFavorite(), every preset with the same
/// preferenceKey() with it) and re-reads the favourites. No other cell can be edited. A text is
/// made when it is first needed and kept until its column's unit changes (Java makes it at every
/// paint; the default units, which a material's text uses, cannot change while the modal parts
/// library is open).
///
/// The unit of a length or mass column (columnUnit()) starts as its group's default unit when the
/// model is made, and changes only through setColumnUnit() (the header menu's "Units"); it is
/// never stored.
///
/// The sort comparators (compareCells(), what Java's TableRowSorter gets per column): the part
/// number by AlphanumComparator; a length or mass by its value in SI units (Double.compare); an
/// Integer and a Boolean (false first) by their values; a missing value of these first (the
/// sorter's rule for null); the favourite as a Boolean; every other column by its text with the
/// tertiary Java collator (JavaCollator::compare(), Collator.getInstance() on toString(), a
/// missing value as the empty text).
///
/// Deviation from OpenRocket: the header texts and the cell texts follow the translation, but the
/// identifiers of the columns (columnIdentifier(), the names TableUiPreferences stores the columns
/// under) are the English headers, so that the stored layout does not depend on the language
/// (Java: the translated header).
class ComponentPresetTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    using PresetPtr = std::shared_ptr<const ComponentPreset>;

    /// The favourite column (ComponentPresetTableColumn.Favorite).
    static constexpr int kFavoriteColumn = 0;
    /// The column of the first key of ORDERED_KEY_LIST, Legacy (the dialog's legacyColumnIndex).
    static constexpr int kLegacyColumn = 1;
    /// The Manufacturer column, which the table sorts by when it is made.
    static constexpr int kManufacturerColumn = 2;
    /// The favourite and the 30 keys of ORDERED_KEY_LIST.
    static constexpr int kColumnCount = 31;

    /// The presets @p presets of @p type, each of that type (else a BugError) and none null; the
    /// favourites of @p preferences, which ticking a favourite changes and which must outlive the
    /// model.
    ComponentPresetTableModel(ComponentPresetType type, std::vector<PresetPtr> presets,
                              Preferences& preferences, QObject* parent);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    /// cellText() for the display role (none for a missing value and for the favourite column);
    /// the favourite's check state (Qt::Checked or Qt::Unchecked) for the check state role of
    /// column 0.
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    /// columnName() for the display role of a horizontal section.
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role) const override;
    /// Enabled and selectable; the favourite column also user-checkable.
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    /// The check state role of the favourite column: setFavorite(). False for anything else.
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    // ---- the columns

    /// The key of @p column (ORDERED_KEY_LIST's key at @p column - 1), nullopt for the favourite.
    /// @throws BugError for a column outside the model
    [[nodiscard]] static std::optional<AnyTypedKey> columnKey(int column);
    /// The column of @p key, -1 for a key that is not one of ORDERED_KEY_LIST.
    [[nodiscard]] static int columnOf(const AnyTypedKey& key);
    /// Whether the table of presets of @p type shows @p column: the favourite and the keys of the
    /// type's displayed columns (ComponentPreset::displayedColumns()). OpenRocket removes the
    /// other columns from the table's column model for good.
    [[nodiscard]] static bool isDisplayed(ComponentPresetType type, int column);
    /// Whether @p column holds Booleans: the favourite, Legacy and Filled.
    [[nodiscard]] static bool isBooleanColumn(int column);
    /// The header of @p column, translated (messages.properties' table.column.<key name>).
    /// @throws BugError for a column outside the model
    [[nodiscard]] static QString columnName(int column);
    /// The English header of @p column ("\u2026 Favorite", "Legacy", "Part Number"), which
    /// identifies the column in the stored table layout (see the class comment).
    /// @throws BugError for a column outside the model
    [[nodiscard]] static std::string_view columnIdentifier(int column);
    /// The unit group of a length or mass column (ComponentPresetTableColumn.DoubleWithUnit),
    /// nullopt for the others.
    [[nodiscard]] static std::optional<UnitGroupId> columnUnitGroup(int column);

    /// The unit @p column shows its values in, nullptr for a column without a unit group.
    [[nodiscard]] const Unit* columnUnit(int column) const;
    /// Shows @p column's values in @p unit (the header menu's "Units"): every cell of the column
    /// changes its text (dataChanged()), then unitChanged().
    /// @throws BugError for a column without a unit group or a unit not of its group
    void setColumnUnit(int column, const Unit& unit);

    // ---- the rows

    [[nodiscard]] ComponentPresetType           presetType() const noexcept { return m_type; }
    [[nodiscard]] const std::vector<PresetPtr>& presets() const noexcept { return m_presets; }
    /// The preset of @p row. @throws BugError for a row outside the model
    [[nodiscard]] const PresetPtr& preset(int row) const;

    // ---- the cells

    /// Whether the cell has a value (Java: getValueAt() is not null); a favourite always has one.
    [[nodiscard]] bool hasValue(int row, int column) const;
    /// The cell's text (see the class comment), empty without a value. For the favourite
    /// column "true" or "false".
    [[nodiscard]] QString cellText(int row, int column) const;
    /// The value in SI units of a length or mass cell, nullopt without a value or for another
    /// column.
    [[nodiscard]] std::optional<double> numberValue(int row, int column) const;
    /// The value of a Boolean cell (isBooleanColumn()), nullopt without a value or for another
    /// column.
    [[nodiscard]] std::optional<bool> booleanValue(int row, int column) const;
    /// How @p column's comparator orders the rows @p rowA and @p rowB, ascending (see the class
    /// comment).
    [[nodiscard]] std::weak_ordering compareCells(int column, int rowA, int rowB) const;

    // ---- the favourites

    /// Whether the preset of @p row is a favourite of the table's type.
    [[nodiscard]] bool isFavorite(int row) const;
    /// ComponentPresetTable's setValueAt() of the favourite column: stores the preset of @p row as
    /// a favourite of the table's type, or removes it (every preset with its preferenceKey()
    /// changes with it), then updateFavorites(), then favoriteSet().
    void setFavorite(int row, bool favorite);
    /// Reads the favourites again (Java: updateFavorites()): every favourite cell may change
    /// (dataChanged()), then favoritesChanged().
    void updateFavorites();

Q_SIGNALS:
    /// The favourites were read again: a sort by the favourite column may have changed.
    void favoritesChanged();
    /// setFavorite() set the favourite of @p row, after favoritesChanged(): the table selects the
    /// row (Java's setValueAt() ends with setRowSelectionInterval()).
    void favoriteSet(int row);
    /// A column shows its values in another unit: the text filter may match other rows.
    void unitChanged(int column);

private:
    [[nodiscard]] const ComponentPreset& presetAt(int row) const;
    void                                 checkRow(int row) const;
    [[nodiscard]] QString                computeText(int row, int column) const;

    ComponentPresetType                   m_type;
    std::vector<PresetPtr>                m_presets;
    Preferences*                          m_preferences;
    std::vector<std::string>              m_preferenceKeys;
    std::vector<bool>                     m_favorites;
    std::array<const Unit*, kColumnCount> m_units{};
    /// cellText() of every cell, made when first asked for.
    mutable std::vector<std::optional<QString>> m_texts;
};

}  // namespace QtRocket::Gui
