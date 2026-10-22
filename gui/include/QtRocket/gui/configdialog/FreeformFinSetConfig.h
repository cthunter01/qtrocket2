#pragma once

#include <filesystem>
#include <memory>
#include <string_view>

#include <QObject>
#include <QPoint>

#include "QtRocket/gui/configdialog/FinSetConfig.h"
#include "QtRocket/gui/figure/FinPointSelection.h"
#include "QtRocket/util/Error.h"

class QAction;
class QCheckBox;
class QEvent;
class QMenu;
class QPushButton;
class QTableView;
class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ComponentSelection;
class DropdownButton;
class FinPointFigure;
class FinPointScrollArea;
class FinPointTableModel;
class GuiServices;
class ScaleSelector;
class ScrollAreaZoomModel;

/// The configuration of a freeform fin set with its fin point editor (OpenRocket's
/// FreeformFinSetConfig, swing gui/configdialog/FreeformFinSetConfig.java): the tabs "General"
/// and "Shape" before FinSetConfig's "Fin tabs" and the common ones, and FinSetConfig's button
/// "Split fins" (no "Convert to freeform").
///
/// General, in OpenRocket's rows: "Number of fins:" (1 to 8, no tooltip), "Fin cant:" (+-15
/// degrees, the label's tooltip), "Fin cross section:", "Thickness:" (slider 0 to 1 cm), and to
/// their right FinSetConfig's column with "Fin rotation:" unbounded.
///
/// Shape ("Fin shape"):
/// - a splitter (nearly all extra width to the right): on the left the point table
///   (FinPointTableModel, the property "FinPoints": single rows; a press of the left button on
///   an editable cell edits it with its text selected, as Java's changeSelection() override;
///   the right button selects the row it is on (when it is one) and shows the menu
///   "Insert point" (a point halfway to the next one, for every row but the last) and "Delete
///   point" (not the first nor the last); the middle button does nothing) and below it "Point
///   Actions..." (DropdownButton; its menu: "Import from image", "Export CSV"); on the right the
///   FinPointFigure in a FinPointScrollArea, fitted when first shown, whose mouse rules are the
///   area's and whose right click on a point shows the menu "Delete point";
/// - below it the hints "Ctrl+click: Delete point", "Click+drag: Add and move points",
///   "Shift+click+drag: Lock angle to previous point" and "Ctrl+shift+click+drag: Lock angle to
///   following point" (2 points smaller), in two rows;
/// - the zoom (ScaleSelector of the figure's user scale), the undo and redo buttons of the
///   editor's own history (icons, not focusable, enabled when no drag nor edit is in progress
///   and the history can undo or redo) and "Live update the rocket"
///   (GuiPreferenceKeys::kUpdateRocketWhileDraggingPoint, written when it is clicked).
/// - Ctrl+Z and Ctrl+Y (Cmd on macOS) undo and redo a point edit while the focus is in the Shape
///   tab (Qt::WidgetWithChildrenShortcut: the design window's undo actions are window shortcuts,
///   which do not reach a dialog, and two shortcuts active for one key would cancel each other).
///
/// The table selects the figure's point (and the figure's press on a point the table's row);
/// every edit path (a cell, Insert point, Delete point, a drag, a press on a segment, Ctrl+click,
/// Import from image) is one edit of the history (beginFinPointEdit(), commitFinPointEdit()), and
/// the points of several selected fin sets are edited as FinPointSelection edits them (the user's
/// decision U3, OpenRocket's config listeners). The history's undo and redo set the snapshot on
/// every selected fin set (Java's setPoints() forwards it).
///
/// updateFields() (the dialog's, after every change of the rocket; the area's after a drag step)
/// refreshes the table, selects the dragged point's row (or none) and the figure's point, brings
/// the figure up to date and the undo and redo buttons.
///
/// The Point Actions menu has no "Scale Fin" (Milestone 5, the scale dialog), which Java shows
/// before "Import from image".
///
/// As OpenRocket: Insert point and Delete point are enabled until the table's selection first
/// changes; the tab order leaves out the Point Actions button, the zoom and the check box (Java's
/// focus traversal policy lists none of them), the table follows the General tab's fields.
///
/// As OpenRocket: Delete point cancels an open cell edit before it removes the point; any other
/// change of the points leaves an open cell editor on its row with the text typed in it (the
/// table's delegate does not set an editor's text again once the user has typed in it: Java's
/// JTable never does).
///
/// Once the panel is released (the dialog closing, a TREE or UNDO change ending the selection),
/// no edit path changes the points any more: the focus loss of an open cell editor as the dialog
/// hides, Insert point, Delete point, the editor's undo and redo, Import from image after its
/// question, and the mouse in the figure (the panel drops its events, ending a drag).
///
/// Deviations from OpenRocket:
/// - Enter in a cell commits it and closes its editor (Qt's item views); Java's JTable then moves
///   on to edit the cell below. The arrow keys move between the cells without editing them
///   (typing, F2 or a press edits); Java edits every cell it moves to.
/// - An open cell editor that has not been typed in shows its cell's new value when the points
///   change; Java's keeps the text it began with. In the cell editor Ctrl+Z is the editor's own
///   undo of the typing.
/// - commitAll() (OK, Esc, the window's close button, the dialog's commit before a save or an
///   undo) commits an open cell editor and closes it, as the editor's focus loss does; Java's
///   JTable leaves it uncommitted when its dialog closes, and the typed text is lost.
/// - "Export CSV" ends its lines with '\n' (FinPointTableModel::csvText(); Java: "\r\n") and shows
///   a write failure ("Error writing file"); Java prints its stack trace only.
/// - "Import from image" reads the image with CustomFinImporter, whose deviations are its own (a
///   walk that never ends, palette images, TIFF files with alpha, files that are no image).
class FreeformFinSetConfig final : public FinSetConfig
{
    Q_OBJECT

public:
    /// The configuration of the freeform fin sets of @p selection (see RocketComponentConfig).
    /// @throws BugError when @p selection is null or invalid, or its main component is not a
    ///         FreeformFinSet
    FreeformFinSetConfig(OpenRocketDocument& document, const GuiServices& services,
                         std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    // ------------------------------------------------------------------------------ Shape

    [[nodiscard]] QWidget&            shapeTab() const noexcept { return *m_shapeTab; }
    [[nodiscard]] QTableView&         pointTable() const noexcept { return *m_table; }
    [[nodiscard]] FinPointTableModel& pointTableModel() const noexcept { return *m_tableModel; }
    [[nodiscard]] FinPointFigure&     finPointFigure() const noexcept { return *m_figure; }
    [[nodiscard]] FinPointScrollArea& figureArea() const noexcept { return *m_area; }
    [[nodiscard]] ScaleSelector&      scaleSelector() const noexcept { return *m_selector; }
    [[nodiscard]] DropdownButton& pointActionsButton() const noexcept { return *m_pointActions; }
    [[nodiscard]] QAction&        importImageAction() const noexcept { return *m_importImage; }
    [[nodiscard]] QAction&        exportCsvAction() const noexcept { return *m_exportCsv; }
    [[nodiscard]] QAction&        insertPointAction() const noexcept { return *m_insertPoint; }
    [[nodiscard]] QAction&        deletePointAction() const noexcept { return *m_deletePoint; }
    /// The table's menu (Insert point, Delete point) and the figure's (Delete point).
    [[nodiscard]] QMenu&       tablePointMenu() const noexcept { return *m_tableMenu; }
    [[nodiscard]] QMenu&       figurePointMenu() const noexcept { return *m_figureMenu; }
    [[nodiscard]] QPushButton& finPointUndoButton() const noexcept { return *m_undoButton; }
    [[nodiscard]] QPushButton& finPointRedoButton() const noexcept { return *m_redoButton; }
    [[nodiscard]] QCheckBox&   liveUpdateCheckBox() const noexcept { return *m_liveUpdate; }

    /// The fin sets the editor edits.
    [[nodiscard]] const FinPointSelection& finPoints() const noexcept { return m_finPoints; }
    /// The point being dragged, -1 when none.
    [[nodiscard]] int dragIndex() const noexcept;
    /// The table's selected row (getSelectedRow()), -1 when none.
    [[nodiscard]] int selectedPointRow() const;
    /// Selects the row @p row of the table, none for -1 (setRowSelectionInterval(),
    /// clearSelection()), without editing it.
    void selectPointRow(int row);

    /// FinSetConfig's, then the table, the selection, the figure and the undo and redo buttons
    /// (see the class comment).
    void updateFields() override;

    /// Commits the point table's open cell editor (its text set as the cell's edit, then the
    /// editor closed, as its focus loss does), then the fields (RocketComponentConfig's); true
    /// when either changed something. Nothing once the panel is released.
    bool commitAll() override;

    // ------------------------------------------------------------------------------ actions

    /// Begins and commits an edit of the history, then updates the undo and redo buttons
    /// (beginFinPointEdit(), commitFinPointEdit()); every change of the points is bracketed by
    /// them (Import from image too).
    void beginFinPointEdit();
    void commitFinPointEdit();
    /// The undo and redo buttons' and keys' action: nothing while a drag or an edit is in
    /// progress (performFinPointUndo(), performFinPointRedo()).
    void performFinPointUndo();
    void performFinPointRedo();
    /// Enables the undo and redo buttons (updateFinPointUndoRedoControls()).
    void updateFinPointUndoRedoControls();
    /// Enables Insert point and Delete point for the selected row (updateActionStates()).
    void updateActionStates();
    /// Insert point: a point halfway between the selected one and the next (insertPoint()).
    void insertPoint();
    /// Delete point: removes the selected point (deletePoint()).
    void deletePoint();
    /// A cell's edit (FinPointTableModel::pointEntered(), Java's setValueAt()): the point @p row
    /// to (@p x, @p y) as one edit, then updateFields().
    void setPointFromTable(int row, double x, double y);
    /// Import from image (importImage()): asks for the image (UserPrompts::askOpenFiles(), the
    /// filters CustomFinImporter::imageNameFilter() and "All Files (*)", the text
    /// "CustomFinImport.description", in the default directory) and, unless cancelled, as one
    /// edit of the history: reads the fin's points from it (CustomFinImporter::getPoints()) and
    /// sets them on every selected fin set (FinPointSelection::setPoints()) inside
    /// OpenRocketDocument::startUndo("Import freeform fin set") and stopUndo(), or shows the
    /// failure ("Error loading fin profile" and its text); then stores the dialog's directory as
    /// the default one, after a failure too.
    void importImage();
    /// Export CSV: asks for the file (UserPrompts::askSaveFile(), the filters "Comma Separated
    /// Files (*.csv)", chosen, and "All Files (*)", in the default directory), forces the
    /// extension ".csv", asks before overwriting an existing file, writes the table
    /// (writeCsvFile()) and stores the dialog's directory as the default one. Nothing more once
    /// the panel has gone during a question (a modal dialog runs the event loop).
    void exportCsv();

    /// Writes @p model's FinPointTableModel::csvText() as UTF-8 to @p file.
    [[nodiscard]] static Result<void> writeCsvFile(const FinPointTableModel&    model,
                                                   const std::filesystem::path& file);
    /// @p file with ".<extension>" appended unless its name ends with that extension, ignoring
    /// case (FileHelper.forceExtension()); a name that starts with its only dot has none.
    [[nodiscard]] static std::filesystem::path forceExtension(const std::filesystem::path& file,
                                                              std::string_view extension);

    /// The table's right and middle buttons (see the class comment), and the figure's mouse once
    /// the panel is released.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// The General and the Shape tab (generalPane(), shapePane()).
    [[nodiscard]] QWidget& makeGeneralTab();
    [[nodiscard]] QWidget& makeShapeTab();
    /// The left of the splitter: the table and Point Actions.
    [[nodiscard]] QWidget& makePointList(QWidget& page);
    /// The menus and their actions.
    void makePointActions();
    /// The row of the zoom, the undo and redo buttons and the check box.
    [[nodiscard]] QWidget& makeBottomControls(QWidget& page);
    /// The right click on the table at its viewport's @p position (Java's mouse listener).
    void tableRightClick(QPoint position, QPoint globalPosition);
    /// The figure's right click on the point @p index.
    void figurePointMenuRequested(int index, QPoint globalPosition);
    /// The table's selection changed: the figure's point and the actions.
    void pointSelectionChanged();
    /// Whether the panel is released (its selection ended): no edit path changes the points.
    [[nodiscard]] bool editsEnded() const;
    /// The table's open cell editor; null when none is open.
    [[nodiscard]] QWidget* openCellEditor() const;
    /// Closes the open cell editor without committing it (Java: cancelCellEditing()).
    void cancelCellEditing();
    /// Commits the open cell editor and closes it (see commitAll()); true when the points changed.
    bool commitCellEditing();

    FinPointSelection m_finPoints;

    QWidget*             m_shapeTab{nullptr};
    FinPointFigure*      m_figure{nullptr};
    FinPointScrollArea*  m_area{nullptr};
    FinPointTableModel*  m_tableModel{nullptr};
    QTableView*          m_table{nullptr};
    ScrollAreaZoomModel* m_zoomModel{nullptr};
    ScaleSelector*       m_selector{nullptr};
    DropdownButton*      m_pointActions{nullptr};
    QAction*             m_importImage{nullptr};
    QAction*             m_exportCsv{nullptr};
    QAction*             m_insertPoint{nullptr};
    QAction*             m_deletePoint{nullptr};
    QMenu*               m_tableMenu{nullptr};
    QMenu*               m_figureMenu{nullptr};
    QPushButton*         m_undoButton{nullptr};
    QPushButton*         m_redoButton{nullptr};
    QCheckBox*           m_liveUpdate{nullptr};
    /// Where the table's right button was pressed, in its viewport.
    QPoint m_tableRightPress{-1, -1};
};

}  // namespace QtRocket::Gui
