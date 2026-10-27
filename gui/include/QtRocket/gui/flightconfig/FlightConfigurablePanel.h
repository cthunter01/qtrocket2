#pragma once

#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <QObject>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/flightconfig/ConfigurationTableView.h"
#include "QtRocket/gui/flightconfig/FlightConfigurableTableModel.h"
#include "QtRocket/gui/flightconfig/RowColumnSelectionModel.h"
#include "QtRocket/gui/flightconfig/TableTransferKeys.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/util/Uuid.h"

class QAction;
class QEvent;
class QMenu;
class QMouseEvent;
class QPoint;
class QPushButton;
class QTableView;

namespace QtRocket
{
class ComponentChangeEvent;
class Rocket;
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class FlightConfigurationEditing;
class FlightConfigurationPanel;

/// What the three panels of the "Motors & Configuration" tab share (OpenRocket's
/// FlightConfigurablePanel, swing gui/main/flightconfigpanel/FlightConfigurablePanel.java): the
/// table of the rocket's flight configurations (a FlightConfigurableTableModel of the panel's
/// kind), how its selection follows the rocket and the design, and its mouse and keys. The
/// subclasses (MotorConfigurationPanel, RecoveryConfigurationPanel, SeparationConfigurationPanel)
/// add their buttons, their operations and their popup menu.
///
/// THE TABLE ("table", a ConfigurationTableView): no vertical header, columns that cannot be moved
/// and share the width equally until the user resizes one by hand (then the last column takes
/// the rest), equal again after each change of the table's structure (restructure()), cells
/// selected as rows x columns (RowColumnSelectionModel: Java's cell selection with
/// MULTIPLE_INTERVAL_SELECTION), nothing to edit, no type-ahead search. At its making it selects
/// the rocket's selected configuration in its last column (Java's doTableInitialization(), before
/// any listener: this selection reaches neither the rocket nor the design).
///
/// THE SELECTION (Java's installTableListener()), told by the table's RowColumnSelectionModel; the
/// steps of a gesture of the mouse are left out, as Java's listeners skip a ListSelectionEvent that
/// is adjusting, and its end comes as one change of the rows, then of the columns (BasicTableUI's
/// release); the keys and the panel's own changes come as they happen, the columns first:
/// - when the rows change and exactly one row is selected, its configuration becomes the rocket's
///   selected one (Rocket::setSelectedConfiguration(); the other panels and the design view's
///   configuration combo box follow its event);
/// - when the columns change while rows are selected (a JTable's listener of its column model sees
///   the rows of before the change when the columns change first), the components of the selected
///   columns become the design's selection (updateRocketViewSelection(): the component tree, and
///   the configuration dialog when it is open);
/// - every change refreshes the subclass's buttons (updateButtonState()).
/// synchronizeConfigurationSelection() brings the table to the rocket (the panel calls it at every
/// change of the rocket): nothing selected for the default configuration, otherwise the rocket's
/// selected configuration in the selected column (or the last one) when the first selected row
/// is another configuration. restoreSelection() is what Java's fireTableDataChanged() leaves after
/// an operation that changed the design: the first selected cell alone, or the rocket's selected
/// configuration (synchronizeConfigurationSelection() on an empty table) when that cell is in row
/// or column 0 (as Java's restoreSelection(), whose test is row <= 0 || col <= 0); the first cell
/// is the one selected when the operation fired its change event (runOperation(): Java reads it
/// just before it fires, which restructures and synchronizes the tables).
///
/// THE MOUSE AND THE KEYS (each panel's initializeTable() and constructor):
/// - Enter (on the keypad too) runs the panel's Select operation (selectValue(): Select motor,
///   Select deployment, Select separation), and so does a double click of the left button when
///   the first selected column is a component's;
/// - a click of the left button outside the cells clears the selection;
/// - a typed key selects nothing (no type-ahead search);
/// - a click of the right button selects the cell under the mouse alone (unless it is selected;
///   its row, then its column, as Java's setRowSelectionInterval() and
///   setColumnSelectionInterval(): the rocket, then the design follow) and opens the popup menu:
///   the panel's own (fullPopupMenu()) over a component's column, the tab's configuration popup
///   over column 0 (FlightConfigurationPanel::configurationPopupMenu()); outside the cells it
///   clears the selection and opens nothing. The other buttons select nothing (Java's table
///   ignores them);
/// - Cut, Copy and Paste (Ctrl+X, C and V) are the table's while it has the focus, as a JTable's
///   (TableTransferKeys): Copy puts the selected cells' values on the clipboard as Java's table
///   exports them, the configuration's id in column 0 and "[<id>;<component name>]" (the
///   Pair<FlightConfigurationId, component> of Java's getValueAt()) in a component's column;
///   Cut and Paste do nothing; the window's items of those keys (the components' Cut, Copy and
///   Paste) do not act then.
///
/// The panel keeps no component: its model looks them up by id (Uuid), and so does every
/// operation. It belongs to its FlightConfigurationPanel, whose document, services and operations
/// (FlightConfigurationEditing) it uses.
///
/// A reset of the model (its rows or columns changed) puts back the selection of the rows and
/// columns that are still there, by configuration and component, and the widths of the columns
/// when they are the same, as Java's JTable keeps them when a configuration is added or removed
/// (an event that is no change of its structure). At every change of the rocket that Java's
/// model answers with fireTableStructureChanged() (isStructureChange(): a MOTOR or TREE change,
/// and a NONFUNCTIONAL one of a component whose name is a column's header for the motor and
/// stage tables), the tab clears the table and gives its columns equal widths again
/// (restructure()), before every panel follows the rocket (the rocket's selected configuration in
/// the last column).
///
/// Deviations from OpenRocket:
/// - Re-entrancy: a change of the selection made while the rocket delivers its change event (the
///   synchronization, a model reset) does not set the rocket's selected configuration (Java sets
///   the one it already has), and the design's selection it calls for is set one turn of the
///   event loop later, from the components' ids.
/// - The rocket's selected configuration follows whenever the rows change to one row; Java's
///   listener looks at the rows of the change's range only (ListSelectionEvent's first and last
///   index, which held the selected row in every case the M3 run 2 probe JTableSelectionProbe
///   tried).
/// - A column resized by hand takes its width from the last column (QHeaderView's
///   stretchLastSection), and once one was, a change of the table's width goes to the last column
///   too; Java's JTable (AUTO_RESIZE_SUBSEQUENT_COLUMNS) takes it from all the columns after the
///   one resized, and shares a change of its width among all of them. Java's columns get their
///   default width back at every change of the structure, and are then shared equally: QtRocket
///   shares them equally at once.
/// - A sub-tab switch that has no column 1 to select in (the motor table without a mount, reached
///   when a disabled sub-tab moves to the Motors tab) selects in column 0; Java's selects in a
///   column that does not exist, which throws.
class FlightConfigurablePanel : public QWidget
{
    Q_OBJECT

public:
    /// MigLayout's "para" and "unrel" gaps (M2's rule: 12 pixels).
    static constexpr int kParagraphGap = 12;

    ~FlightConfigurablePanel() override;

    FlightConfigurablePanel(const FlightConfigurablePanel&)            = delete;
    FlightConfigurablePanel& operator=(const FlightConfigurablePanel&) = delete;
    FlightConfigurablePanel(FlightConfigurablePanel&&)                 = delete;
    FlightConfigurablePanel& operator=(FlightConfigurablePanel&&)      = delete;

    /// The tab the panel belongs to.
    [[nodiscard]] FlightConfigurationPanel& flightConfigurationPanel() const noexcept
    {
        return *m_panel;
    }
    /// The tab's operations.
    [[nodiscard]] FlightConfigurationEditing& editing() const noexcept;
    /// The design's rocket.
    [[nodiscard]] Rocket& rocket() const noexcept;

    /// The table ("table").
    [[nodiscard]] QTableView& table() const noexcept { return *m_table; }
    /// The table's model.
    [[nodiscard]] FlightConfigurableTableModel& tableModel() const noexcept { return *m_model; }
    /// The table's selection.
    [[nodiscard]] RowColumnSelectionModel& tableSelection() const noexcept { return *m_selection; }
    /// The popup menu over a component's column ("popupMenuFull").
    [[nodiscard]] QMenu& fullPopupMenu() const noexcept { return *m_fullPopup; }
    /// The table's Cut, Copy and Paste keys.
    [[nodiscard]] TableTransferKeys& transferKeys() const noexcept { return *m_transferKeys; }

    /// The values that Copy puts on the clipboard for the selected cells (see the class comment):
    /// Java's getValueAt(row, column).toString() of the selected rows x columns.
    [[nodiscard]] TableTransferKeys::Cells transferCells() const;

    // ------------------------------------------------------------------ the selection (Java's)

    /// The component of the first selected column when it is a component's and a row is
    /// selected (getSelectedComponent()); null otherwise.
    [[nodiscard]] RocketComponent* selectedComponent() const;
    /// The components of the selected columns, in column order, when a row is selected
    /// (getSelectedComponents()); none otherwise.
    [[nodiscard]] std::vector<Uuid> selectedComponentIds() const;
    /// The configuration of the first selected row when a column is selected
    /// (getSelectedConfigurationId()); nullopt otherwise.
    [[nodiscard]] std::optional<FlightConfigurationId> selectedConfigurationId() const;
    /// The configurations of the selected rows, in row order, when a column is selected
    /// (getSelectedConfigurationIds()); none otherwise.
    [[nodiscard]] std::vector<FlightConfigurationId> selectedConfigurationIds() const;
    /// Selects the rows of @p ids in column 1 (setSelectedConfigurationIds(), a sub-tab switch's);
    /// nothing when they are all selected already or the list is empty.
    void setSelectedConfigurationIds(std::span<const FlightConfigurationId> ids);
    /// Selects nothing (clearSelection()).
    void clearSelection();
    /// Brings the table to the rocket's selected configuration (see the class comment).
    void synchronizeConfigurationSelection();
    /// What Java's fireTableDataChanged() leaves after an operation that changed the design (see
    /// the class comment), from the first selected cell of now, then updateButtonState().
    void restoreSelection();
    /// restoreSelection() from the cell (@p row, @p column) (Java's restoreSelection(row, col),
    /// after its table's selection was cleared).
    void restoreSelection(int row, int column);
    /// Whether the table's model answers the rocket's change @p event with Java's
    /// fireTableStructureChanged(): a MOTOR or TREE change, or a NONFUNCTIONAL change of a
    /// component that names one of the panel's kind of columns (restructuresWith()).
    [[nodiscard]] bool isStructureChange(const ComponentChangeEvent& event) const;
    /// What Java's fireTableStructureChanged() does to the table: nothing selected, the columns'
    /// widths equal again (none resized by hand).
    void restructure();
    /// Makes the components of the selected columns the design's selection (when there are any).
    void updateRocketViewSelection();
    /// Gives the table the keyboard focus when it is shown (takeTheSpotlight()).
    void takeTheSpotlight();
    /// Whether a column was resized by hand since the last change of the structure.
    [[nodiscard]] bool columnsResizedByHand() const noexcept { return m_columnsResizedByHand; }

    /// Enables the panel's buttons and actions for the selection (updateButtonState()).
    virtual void updateButtonState() = 0;
    /// The panel's Select operation for the selected cells (Enter, a double click, New
    /// Configuration's follow-up): Select motor, Select deployment, Select separation.
    virtual void selectValue() = 0;

    /// Enter on the table; the mouse in its viewport and its header (see the class comment).
    bool eventFilter(QObject* watched, QEvent* event) override;

protected:
    /// The panel of @p kind in @p panel (which makes it before it connects to the rocket, and
    /// outlives it), a child of @p parent.
    FlightConfigurablePanel(FlightConfigurationPanel&          panel,
                            FlightConfigurableTableModel::Kind kind, QWidget* parent);

    /// Runs @p operation, one of the tab's operations (FlightConfigurationEditing) on the
    /// selected cells, as Java's panels do: the first selected cell read when the operation fires
    /// its change event of the rocket (or before it, when it fires none), then restoreSelection()
    /// from it when the operation changed the design (it answers true), the table's focus
    /// otherwise (Java: fireTableDataChanged() or table.requestFocusInWindow()). Nothing after the
    /// operation when the panel went during its question.
    void runOperation(const std::function<bool()>& operation);

    /// Whether a NONFUNCTIONAL change of @p source restructures the table (its name is a column's
    /// header: Java's motor table model looks for a BodyTube or an InnerTube, its stage table
    /// model for an AxialStage); none by default (the recovery table's model).
    [[nodiscard]] virtual bool restructuresWith(const RocketComponent& source) const;

    /// An action of the panel with @p text, named @p objectName, that runs @p operation (guarded:
    /// what it throws is reported, with the action's text as the context).
    [[nodiscard]] QAction* makeAction(const QString& text, const QString& objectName,
                                      std::function<void()> operation);

    /// A push button of @p parent named @p objectName that shows @p action's text, is enabled as
    /// the action is and triggers it (Java's new JButton(action)).
    [[nodiscard]] static QPushButton* makeButton(QAction& action, const QString& objectName,
                                                 QWidget* parent);

    /// Gives @p buttons the width of the widest (MigLayout's "sizegroup button").
    static void shareWidth(std::span<QPushButton* const> buttons);

private:
    /// The table's selection changed (Java's listeners).
    void linesChanged(const RowColumnSelectionModel::LinesChange& change);
    /// Java's listener of the row selection.
    void rowsChanged(const RowColumnSelectionModel::LinesChange& change);
    /// Java's listener of the column selection.
    void columnsChanged(const RowColumnSelectionModel::LinesChange& change);
    /// The components of @p columns, in order.
    [[nodiscard]] std::vector<Uuid> componentsOf(const std::vector<int>& columns) const;
    /// Makes @p components the design's selection: now, or one turn of the event loop later while
    /// the rocket delivers its change event.
    void requestRocketViewSelection(std::vector<Uuid> components);
    /// The right button's click at @p position of the viewport.
    void showPopup(const QPoint& position);
    /// A mouse event of the viewport; whether it is consumed.
    bool viewportMouseEvent(const QMouseEvent& event);
    /// The left button's release at @p position of the viewport, after the table took it.
    void leftButtonReleased(const QPoint& position);
    /// Gives the columns equal widths unless one was resized by hand.
    void fitColumns();
    /// A column's width changed (by hand while the header is pressed).
    void columnResized();
    /// The keys of the selection, before a reset of the model.
    void rememberSelection();
    /// Selects what rememberSelection() kept and the model still has.
    void reselectAfterReset();

    FlightConfigurationPanel*     m_panel;
    FlightConfigurableTableModel* m_model;
    ConfigurationTableView*       m_table;
    RowColumnSelectionModel*      m_selection;
    QMenu*                        m_fullPopup;
    TableTransferKeys*            m_transferKeys;
    /// The selection's keys at the last reset of the model: the rows' configurations, whether
    /// column 0 was selected, the other columns' components.
    std::vector<FlightConfigurationId> m_rememberedRows;
    bool                               m_rememberedColumnZero{false};
    std::vector<Uuid>                  m_rememberedColumns;
    /// The model's columns and their widths at the last reset of the model.
    std::vector<Uuid> m_rememberedWidthColumns;
    std::vector<int>  m_rememberedWidths;
    /// The components of the queued change of the design's selection.
    std::vector<Uuid> m_queuedComponents;
    /// While the selection is put back after a reset of the model.
    bool m_reselecting{false};
    /// Whether a column was resized by hand since the last change of the structure.
    bool m_columnsResizedByHand{false};
    /// While the panel sets the columns' widths.
    bool m_fittingColumns{false};
    /// While the left button is pressed in the header (a resize by hand).
    bool m_headerPressed{false};
    /// Whether the left button's press was outside the cells (a click there clears).
    bool m_pressedOutside{false};
    /// Whether the left button's press was a double click (its release runs selectValue()).
    bool m_doubleClicked{false};
    /// Whether a change of the design's selection is queued.
    bool m_viewSelectionQueued{false};
};

}  // namespace QtRocket::Gui
