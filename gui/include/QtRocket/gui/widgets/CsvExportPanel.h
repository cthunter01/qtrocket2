#pragma once

#include <cstddef>
#include <vector>

#include <QColor>
#include <QObject>
#include <QString>
#include <QWidget>

class QCheckBox;
class QGridLayout;
class QLabel;
class QPalette;
class QPushButton;
class QScrollArea;

namespace QtRocket
{
class Unit;
class UnitGroup;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class CsvOptionPanel;
class UnitSelector;

/// The choice of the variables a CSV file is written with (OpenRocket's CSVExportPanel, swing
/// gui/widgets/CSVExportPanel.java, of which SimulationExportPanel, the simulation dialog's
/// "Export data" tab, and Milestone 7's component analysis export are made): on the left, in a
/// scroll area (vertical scroll bar as needed, never a horizontal one), a table of one row per
/// variable under the bold headers "Export", "Variable" and "Unit" (with a line below them in the
/// style's table grid colour, Java's Table.gridColor), each row a check box (ticked as the row is
/// given), the variable's name and a unit selector of its unit group starting at the group's
/// default unit, the rows' backgrounds alternating between the palette's base colour (Java's
/// Table.background) and OpenRocket's lighter row colour (ThemeColors ROW_LIGHTER), as the
/// palette is when they are painted; below it the buttons "Select all" and "Select none" and the
/// count, "Exporting 1 variable out of N." or "Exporting n variables out of N."; and in a column
/// on the right, the CSV option panel given (widgets/CsvOptionPanel) and the extra widgets given
/// below it (the export tab's "Stage to export" group), or with OptionsPlacement::SEPARATE_ROW
/// all of them in the rows below the table, two per row.
///
/// The panel owns the rows' ticks and units: isSelected(), selected(), unit() and units() read
/// them when they are needed (Java's selected and units arrays, which the owner shares; a unit
/// chosen is never stored, as in Java).
///
/// The extra column (Java's createExtraComponent(), hasExtraComponent() and
/// getExtraColumnLabelKey(), which M7's component analysis export gives its "Components"
/// column): a subclass overrides hasExtraColumn(), extraColumnLabel() and
/// createExtraColumnWidget(), makes the panel with the protected DeferredBuild constructor and
/// calls build() at the end of its own constructor, once its members exist (Java calls the
/// overrides from the base class's constructor, before the subclass's fields are initialised;
/// C++ calls none of a subclass from a base constructor). The public constructor builds the panel
/// at once, without the extra column.
///
/// The object names are Java's fields: each row's "exportCheckBox", "nameLabel" and
/// "unitSelector", "selectedCountLabel", "csvOptions"; and the locals "selectAllButton",
/// "selectNoneButton", "scrollPane" and "contentPanel".
///
/// Each row's unit selector is a UnitSelector over the row's bare unit group (Java:
/// UnitSelector(UnitGroup)).
///
/// Deviations from OpenRocket:
/// - hasExtraColumn() is false unless a subclass says otherwise; Java's hasExtraComponent()
///   makes a throwaway extra component of the first row to find out.
/// - Each row's check box has the row's name as its accessible name (Java's has none).
class CsvExportPanel : public QWidget
{
    Q_OBJECT

public:
    /// A variable: its name (Java: getDisplayName(type), SimulationExportPanel's type.getName()),
    /// the unit group of its values and whether it starts ticked.
    struct Row
    {
        QString          name;
        const UnitGroup* unitGroup{nullptr};
        bool             selected{false};
    };

    /// Where the CSV options and the extra widgets go (Java's separateRowForOptions).
    enum class OptionsPlacement
    {
        /// In a column right of the table (Java: false, both of OpenRocket's exports).
        RIGHT_COLUMN,
        /// In the rows below the table (Java: true).
        SEPARATE_ROW,
    };

    /// The panel of @p rows with the CSV options @p csvOptions and the widgets @p extraWidgets
    /// (Java's extraComponents), all of which become its children, built at once (without the
    /// extra column).
    /// @throws BugError when @p csvOptions or an extra widget is null, or a row has no unit group
    CsvExportPanel(std::vector<Row> rows, CsvOptionPanel* csvOptions, OptionsPlacement placement,
                   const std::vector<QWidget*>& extraWidgets, QWidget* parent);

    /// The number of rows.
    [[nodiscard]] std::size_t rowCount() const noexcept { return m_rows.size(); }
    /// The name of the row @p row.
    /// @throws BugError when there is no such row
    [[nodiscard]] const QString& name(std::size_t row) const;
    /// Whether the row @p row is ticked.
    /// @throws BugError when there is no such row, or the panel is not built
    [[nodiscard]] bool isSelected(std::size_t row) const;
    /// Ticks the row @p row or not (as the user's click).
    /// @throws BugError when there is no such row, or the panel is not built
    void setSelected(std::size_t row, bool selected);
    /// Whether each row is ticked, in the rows' order (Java's selected array).
    [[nodiscard]] std::vector<bool> selected() const;
    /// The number of rows ticked.
    [[nodiscard]] std::size_t selectedCount() const;
    /// The unit of the row @p row: its unit selector's.
    /// @throws BugError when there is no such row, or the panel is not built
    [[nodiscard]] const Unit& unit(std::size_t row) const;
    /// The unit of each row, in the rows' order (Java's units array).
    [[nodiscard]] std::vector<const Unit*> units() const;
    /// Java's setSelectedUnit(): the row @p row's unit and its selector become @p unit.
    /// @throws BugError when there is no such row, the panel is not built, or @p unit is not one
    ///         of the row's unit group
    void setSelectedUnit(std::size_t row, const Unit& unit);

    /// Java's selectAll() and selectNone(): every row ticked, or none (the buttons).
    void selectAll();
    void selectNone();

    /// The CSV options given to the constructor.
    [[nodiscard]] CsvOptionPanel& csvOptions() const noexcept { return *m_csvOptions; }

    /// The count's text (Java's updateSelectedCount()): "Exporting 1 variable out of <total>." for
    /// one, "Exporting <selected> variables out of <total>." otherwise.
    [[nodiscard]] static QString selectedCountText(std::size_t selected, std::size_t total);

    /// The background of the row @p row (from 0): @p palette's base colour for an even row (Java's
    /// Table.background), OpenRocket's ROW_LIGHTER for the palette's theme for an odd one.
    [[nodiscard]] static QColor rowBackground(std::size_t row, const QPalette& palette);

    /// The widgets, for the tests and the owner.
    /// @throws BugError (the row accessors) when there is no such row, or the panel is not built
    [[nodiscard]] QCheckBox*    exportCheckBox(std::size_t row) const;
    [[nodiscard]] QLabel*       nameLabel(std::size_t row) const;
    [[nodiscard]] UnitSelector* unitSelector(std::size_t row) const;
    /// The extra column's widget of the row @p row, null without the extra column.
    [[nodiscard]] QWidget*     extraColumnWidget(std::size_t row) const;
    [[nodiscard]] QPushButton* selectAllButton() const noexcept { return m_selectAll; }
    [[nodiscard]] QPushButton* selectNoneButton() const noexcept { return m_selectNone; }
    [[nodiscard]] QLabel*      selectedCountLabel() const noexcept { return m_selectedCount; }
    [[nodiscard]] QScrollArea* scrollPane() const noexcept { return m_scrollPane; }
    /// The table's widget in the scroll area (its grid: the headers in row 0, the rows below).
    [[nodiscard]] QWidget* contentPanel() const noexcept { return m_contentPanel; }

protected:
    /// The tag of the constructor that leaves the building to build().
    struct DeferredBuild
    {
        explicit DeferredBuild() = default;
    };

    /// The panel as the public constructor makes it, but empty until the subclass calls build()
    /// (see the class comment).
    CsvExportPanel(DeferredBuild deferred, std::vector<Row> rows, CsvOptionPanel* csvOptions,
                   OptionsPlacement placement, const std::vector<QWidget*>& extraWidgets,
                   QWidget* parent);

    /// Builds the panel, with the extra column when hasExtraColumn() (once).
    /// @throws BugError when it was built already, or createExtraColumnWidget() gives null
    void build();

    /// Whether every row has an extra column (Java's hasExtraComponent()): false here.
    [[nodiscard]] virtual bool hasExtraColumn() const;
    /// The extra column's header (Java: the text of getExtraColumnLabelKey(), here "Extra",
    /// CSVExportPanel.lbl.Extra).
    [[nodiscard]] virtual QString extraColumnLabel() const;
    /// The extra column's widget of the row @p row, a child of @p parent (Java:
    /// createExtraComponent(type, index)); null here.
    [[nodiscard]] virtual QWidget* createExtraColumnWidget(std::size_t row, QWidget* parent);

private:
    /// The widgets of a row.
    struct RowWidgets
    {
        QCheckBox*    exportCheckBox{nullptr};
        QLabel*       nameLabel{nullptr};
        UnitSelector* unitSelector{nullptr};
        QWidget*      extra{nullptr};
    };

    /// The panel's layout and widgets, the extra column's when @p withExtraColumn.
    void buildPanel(bool withExtraColumn);
    /// The left part: the table in its scroll area, the buttons and the count.
    [[nodiscard]] QWidget* makeExportPanel(bool withExtraColumn);
    /// The right column: the CSV options and the extra widgets below each other.
    [[nodiscard]] QWidget* makeOptionsColumn();
    /// The table's grid in m_contentPanel.
    void buildTable(bool withExtraColumn);
    /// Adds a bold header @p text in the column @p column of @p grid.
    void addHeader(QGridLayout& grid, const QString& text, int column);
    /// The scroll area's width: wide enough for the table and the vertical scroll bar.
    void fitScrollPaneWidth();
    /// Shows the number of ticked rows (Java's updateSelectedCount()).
    void updateSelectedCount();
    /// The widgets of the row @p row.
    /// @throws BugError when there is no such row, or the panel is not built
    [[nodiscard]] const RowWidgets& rowWidgets(std::size_t row) const;

    std::vector<Row>        m_rows;
    CsvOptionPanel*         m_csvOptions;
    OptionsPlacement        m_placement;
    std::vector<QWidget*>   m_extraWidgets;
    std::vector<RowWidgets> m_rowWidgets;
    QScrollArea*            m_scrollPane{nullptr};
    QWidget*                m_contentPanel{nullptr};
    QPushButton*            m_selectAll{nullptr};
    QPushButton*            m_selectNone{nullptr};
    QLabel*                 m_selectedCount{nullptr};
    bool                    m_built{false};
};

}  // namespace QtRocket::Gui
