#pragma once

#include <optional>
#include <vector>

#include <QModelIndex>
#include <QObject>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QString>

#include "QtRocket/gui/util/TableSortKeys.h"

namespace QtRocket::Gui
{

class ComponentPresetTableModel;

/// The rows the parts library's table shows and their order (OpenRocket: ComponentPresetTable's
/// TableRowSorter, with ComponentPresetRowFilter and the filters ComponentPresetChooserDialog's
/// updateFilters() gives it, swing gui/dialogs/preset/): a proxy of a ComponentPresetTableModel.
///
/// The filters, all of which a row must pass:
/// - the text (setTextFilter()): a regular expression that some text column of the row must
///   contain (QRegularExpression with CaseInsensitiveOption and UseUnicodePropertiesOption over
///   ComponentPresetTableModel::cellText(), the empty text for a missing value), over every column
///   of the model, also the ones the table does not show (a hidden Material finds centering rings
///   by "Cardboard"), but not the Boolean ones; an empty text lets every row through;
/// - the fore and the aft diameter (setForeDiameterFilter(), setAftDiameterFilter()): the row's
///   value in the type's fore or aft diameter column (PresetDiameterFilters::foreDiameterKey()
///   and aftDiameterKey()) within diameterTolerance() of the given diameter, |d - v| < tolerance;
///   a row without a value passes (ComponentPresetRowFilter);
/// - the legacy filter (setHideLegacy()): a row whose Legacy is true is hidden; one without a
///   Legacy value passes.
///
/// The order: the sort keys (sortKeys(), TableSortKeys with the table model's comparators,
/// ComponentPresetTableModel::compareCells()), equal rows in the model's order. A new filter model
/// sorts by the Manufacturer column ascending (Java: toggleSortOrder(2) when the table is made);
/// toggleSortOrder() is a header click, setSortKeys() the header menu's Sort items. The rows are
/// sorted and filtered again when a key, a filter, the favourites or a column's unit change.
///
/// Deviations from OpenRocket:
/// - The text filter ignores the Boolean columns (the favourite, Legacy and Filled; bug 17 of the
///   plan's section 10.3, fixed). OpenRocket's RowFilter.regexFilter() tests every column, whose
///   Boolean text is "true" or "false": every row has an unticked favourite, so "e" (or a, l, s,
///   f, r, t, u) matches every row, and "true" matches every legacy or filled one.
/// - Qt's regular expressions (PCRE2) in place of java.util.regex (the plan's decision U-M3-6e),
///   and an invalid expression (isTextFilterValid()) matches no row. OpenRocket drops the text
///   filter when its pattern does not compile (PatternSyntaxException), so a "[" shows every row.
class ComponentPresetFilterModel final : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    /// ComponentPresetRowFilter's tolerance around @p diameter: 5 % of it, at least 1 mm
    /// (MathUtil.max(diameter * 0.05, 0.001), so 1 mm for a NaN diameter).
    [[nodiscard]] static double diameterTolerance(double diameter) noexcept;
    /// ComponentPresetRowFilter.include() for a diameter filter of @p diameter: true without a
    /// value, else |@p diameter - @p value| < diameterTolerance(@p diameter).
    [[nodiscard]] static bool matchesDiameter(double                diameter,
                                              std::optional<double> value) noexcept;

    /// The rows of @p source, which outlives this model, sorted by Manufacturer, without filters.
    ComponentPresetFilterModel(ComponentPresetTableModel& source, QObject* parent);

    [[nodiscard]] ComponentPresetTableModel& tableModel() const noexcept { return *m_source; }

    // ---- the order

    /// The sort keys, the primary key first (TableRowSorter.getSortKeys()).
    [[nodiscard]] const TableSortKeys& sortKeys() const noexcept { return m_sortKeys; }
    /// A click on the header of the model column @p column (toggleSortOrder()).
    /// @throws BugError for a column outside the model
    void toggleSortOrder(int column);
    /// Replaces the sort keys (setSortKeys(); the header menu's Sort Ascending and Sort
    /// Descending give one key).
    /// @throws BugError for a column outside the model or one named twice
    void setSortKeys(std::vector<TableSortKeys::SortKey> keys);

    // ---- the filters

    /// The text filter (see the class comment); an empty @p pattern is none.
    void                  setTextFilter(const QString& pattern);
    [[nodiscard]] QString textFilter() const { return m_text.pattern(); }
    /// Whether the text filter's pattern is a valid regular expression (an empty one is).
    [[nodiscard]] bool isTextFilterValid() const { return m_text.isValid(); }

    /// The model column the fore diameter filter compares, -1 when the type has none.
    [[nodiscard]] int foreDiameterColumn() const noexcept { return m_foreColumn; }
    /// The model column of the aft diameter filter, -1 when the type has none.
    [[nodiscard]] int aftDiameterColumn() const noexcept { return m_aftColumn; }
    /// The fore diameter filter of @p diameter in m, none for nullopt.
    /// @throws BugError for a diameter when the type has no fore diameter column
    void                                setForeDiameterFilter(std::optional<double> diameter);
    [[nodiscard]] std::optional<double> foreDiameterFilter() const noexcept { return m_fore; }
    /// The aft diameter filter of @p diameter in m, none for nullopt.
    /// @throws BugError for a diameter when the type has no aft diameter column
    void                                setAftDiameterFilter(std::optional<double> diameter);
    [[nodiscard]] std::optional<double> aftDiameterFilter() const noexcept { return m_aft; }

    /// Hides the legacy rows (@p hide) or shows them (the dialog's legacy filter, on while "Show
    /// Legacy Database" is unticked).
    void               setHideLegacy(bool hide);
    [[nodiscard]] bool hidesLegacy() const noexcept { return m_hideLegacy; }

    // ---- the rows

    /// The model row of the view row @p row (convertRowIndexToModel()), -1 for none.
    [[nodiscard]] int modelRow(int row) const;
    /// The view row of the model row @p row (convertRowIndexToView()), -1 when it is filtered out.
    [[nodiscard]] int viewRow(int row) const;
    /// Whether the filters let the model row @p row through (also while the view is being
    /// updated to them).
    [[nodiscard]] bool acceptsRow(int row) const;

Q_SIGNALS:
    /// The sort keys changed (toggleSortOrder(), setSortKeys()): the header's sort indicator
    /// follows the primary key.
    void sortKeysChanged();

protected:
    [[nodiscard]] bool filterAcceptsRow(int                sourceRow,
                                        const QModelIndex& sourceParent) const override;
    [[nodiscard]] bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    [[nodiscard]] bool acceptsText(int row) const;
    /// Sorts the rows again by the keys (and filters them again).
    void resort();

    ComponentPresetTableModel* m_source;
    TableSortKeys              m_sortKeys;
    QRegularExpression         m_text;
    int                        m_foreColumn{-1};
    int                        m_aftColumn{-1};
    std::optional<double>      m_fore;
    std::optional<double>      m_aft;
    bool                       m_hideLegacy{false};
};

}  // namespace QtRocket::Gui
