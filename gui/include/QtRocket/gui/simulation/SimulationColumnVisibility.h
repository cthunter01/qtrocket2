#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <QObject>
#include <QPoint>

#include "QtRocket/gui/simulation/SimulationColumns.h"

class QMenu;

namespace QtRocket
{
class DocumentPreferences;
class Preferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class SimulationTableView;

/// The column chooser of the simulations table (OpenRocket's ColumnVisibilityController,
/// SimulationPanel.java:1787-2009): which of the fourteen columns the table shows, chosen in a
/// menu on its header and stored as the ids of the hidden columns (SimulationColumns::id()).
///
/// The stored text: the hidden columns' ids joined by ',' in the order they were hidden (a
/// column shown again leaves the list; hidden again, it goes last), "" when none is hidden
/// (formatHiddenColumns()). Every change made in the menu stores it in the design's own
/// preferences, the document preference GuiPreferenceKeys::kSimulationTableHiddenColumns
/// ("simulation.table.hiddenColumns", saved in the design file's <docprefs>): the design
/// becomes unsaved, and no undo position is added. "Save as default" stores it in the
/// application preference GuiPreferenceKeys::kSimulationTableHiddenColumnsDefault
/// ("simulation.table.hiddenColumns.default"). Nothing else is stored (OpenRocket stores no
/// widths and no order: a moved column stays where it is until the table is made again).
///
/// When the chooser is made (as the tab is built), the table shows what is stored
/// (storedHiddenColumns()): the design's text, or without one the application's, or without
/// either every column but those SimulationColumns::isHiddenByDefault() names (the stepper
/// column). A text is read as OpenRocket's String.split(",") reads it, each id trimmed (Java's
/// String.trim(): the characters up to ' '); an empty one or an id of no column is ignored, as
/// is a repeat (parseHiddenColumns()). When the text would hide every column, the first one
/// (the status column) is shown and leaves the list. Nothing is stored then.
///
/// The menu (populateMenu(), shown by a click of the right button on the header: Qt's context
/// menu of the header): a check box item per column in the model's order (its text
/// SimulationColumns::chooserText(): "Status" for the status column), ticked when it is shown;
/// the item of the only column shown is disabled (a table keeps one column); a separator; "Save
/// as default" with its tooltip. An item shows or hides its column (setColumnVisible()); a
/// column shown again goes to the place among the shown columns that the shown columns before it
/// in the model's order make (ColumnVisibilityController.calculateInsertionIndex(): after the
/// user moved columns this need not be the place it left).
///
/// Deviation (an OpenRocket bug found while porting, fixed: a stored choice is lost): an empty
/// stored text means "no column hidden". OpenRocket takes an empty design text for no text at all
/// and goes on to the application's text and then to its default, so a design saved with every
/// column shown (the stepper column shown, "") opens with the stepper column hidden again, or
/// with the application's hidden columns; and "Save as default" with every column shown stores
/// "", after which a new design hides the stepper column all the same. Here a text that is
/// stored is the choice, empty or not (OpenRocket's own files and preferences keep their
/// meaning otherwise; the stored texts are OpenRocket's). A document preference of another type
/// than a string under the key counts as no text (OpenRocket's getString() throws a
/// ClassCastException).
///
/// The table, the document preferences and the application preferences must outlive the chooser
/// (make it a child of the table). Not copyable.
class SimulationColumnVisibility final : public QObject
{
    Q_OBJECT

public:
    using Column = SimulationColumns::Column;

    /// The chooser of @p table's columns, stored in @p documentPreferences (the design's) and
    /// @p applicationPreferences: the table shows the stored choice from now on, and a click of
    /// the right button on its header shows the menu.
    SimulationColumnVisibility(SimulationTableView& table, DocumentPreferences& documentPreferences,
                               Preferences& applicationPreferences, QObject* parent);
    ~SimulationColumnVisibility() override;

    SimulationColumnVisibility(const SimulationColumnVisibility&)            = delete;
    SimulationColumnVisibility& operator=(const SimulationColumnVisibility&) = delete;
    SimulationColumnVisibility(SimulationColumnVisibility&&)                 = delete;
    SimulationColumnVisibility& operator=(SimulationColumnVisibility&&)      = delete;

    /// The hidden columns in the order they were hidden (what is stored).
    [[nodiscard]] const std::vector<Column>& hiddenColumns() const noexcept { return m_hidden; }

    /// Whether @p column is shown.
    [[nodiscard]] bool isColumnVisible(Column column) const;

    /// A check box item of the menu ticked (@p visible) or unticked
    /// (ColumnVisibilityController.toggleColumn()): shows or hides @p column and stores the
    /// hidden columns in the design. Hiding the only column shown does nothing; a column already
    /// in that state only leaves or joins the list, which is stored when that changed it.
    void setColumnVisible(Column column, bool visible);

    /// "Save as default": stores the hidden columns in the application's preferences.
    void saveAsDefault();

    /// Fills @p menu with the header menu (see the class comment); its items act on the chooser
    /// while it lives.
    void populateMenu(QMenu& menu);

    /// Shows the header menu at @p position of the header's viewport (the header's context
    /// menu): the table's one menu, filled again.
    void showMenu(const QPoint& position);

    /// The columns @p text names (see the class comment), in its order, each once.
    [[nodiscard]] static std::vector<Column> parseHiddenColumns(std::string_view text);

    /// The stored text of @p hidden: their ids joined by ','.
    [[nodiscard]] static std::string formatHiddenColumns(std::span<const Column> hidden);

    /// The hidden columns a table of a design with @p documentPreferences starts with, in an
    /// application with @p applicationPreferences (see the class comment), before the rule that
    /// keeps one column shown.
    [[nodiscard]] static std::vector<Column> storedHiddenColumns(
        const DocumentPreferences& documentPreferences, const Preferences& applicationPreferences);

private:
    /// Hides the stored hidden columns, then shows the first column when none is shown
    /// (applyHiddenColumnsFromPreferences()).
    void apply();
    /// The number of columns shown.
    [[nodiscard]] int visibleCount() const;
    /// Moves the shown @p column to its place among the shown columns (see the class comment).
    void place(Column column);
    /// Stores the hidden columns in the design.
    void store();

    SimulationTableView* m_table;
    DocumentPreferences* m_documentPreferences;
    Preferences*         m_applicationPreferences;
    std::vector<Column>  m_hidden;
    /// The header menu, a child of the table, made when it is first shown.
    QMenu* m_menu{nullptr};
};

}  // namespace QtRocket::Gui
