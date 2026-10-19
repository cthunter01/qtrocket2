#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <QItemSelectionModel>
#include <QModelIndex>
#include <QObject>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QTreeView>
#include <Qt>
#include <QtContainerFwd>

#include "QtRocket/util/Uuid.h"

class QAbstractItemModel;
class QContextMenuEvent;
class QDragMoveEvent;
class QDropEvent;
class QKeyEvent;
class QMouseEvent;
class QWidget;

namespace QtRocket
{
class RocketComponent;
}

namespace QtRocket::Gui
{

class ComponentTreeModel;

/// The component tree of a design window (OpenRocket's ComponentTree, a BasicTree, with the
/// mouse handling of DesignPanel and the state that ComponentTreeModel keeps across a change of
/// the tree): a QTreeView of a ComponentTreeModel drawn by a ComponentTreeDelegate.
///
/// - BasicTree's look: the Rocket's row without an expand handle (showsRootHandles false), 15
///   pixels of indentation, a double-click that does not expand or collapse (toggleClickCount
///   0); no header, one column; several rows selectable at once (DISCONTIGUOUS_TREE_SELECTION).
///   The whole tree is expanded at creation (expandTree()).
/// - Childless rows: whenever rows become visible (a change of the tree, an expansion), every
///   visible row that allows children but has none is expanded (BasicTree.treeDidChange()'s
///   expandChildlessNodes()), so that a child added later shows at once.
/// - Collapsing a row whose descendants are selected deselects them and selects the row, as
///   JTree does (setExpandedState()).
/// - A double-click on a row asks for the component's configuration dialog (editRequested(),
///   DesignPanel's mouse listener); with Shift or Ctrl (Cmd on macOS) held the row is added to
///   the selection and the request is for every selected component. The tree only asks: whoever
///   shows the dialog (the window) answers, through a signal (decision O12), and
///   setEditAllowed() stops the requests while the dialog is open (DesignPanel's
///   !ComponentConfigDialog.isDialogVisible()).
/// - A right-click selects the row alone when it is not selected, then asks for the context
///   menu (contextMenuRequested()); a click of any button where there is no row clears the
///   selection.
/// - Ctrl+A (Cmd+A) selects every visible row but the Rocket (ComponentTree's key listener); the
///   view's own Copy, Cut, Paste, Save, Open and New keys do nothing (DesignPanel removes
///   JTree's bindings), so they reach the window's actions or nobody.
/// - A range of rows (Shift and a click, a drag over rows) is selected in the order of the rows,
///   the top one first (JTree's getPathBetweenRows()).
/// - A change of the selection scrolls every selected row into view, expanding its ancestors
///   (DesignPanel's listener, JTree.scrollPathToVisible()); one made with the mouse scrolls as
///   Qt's views do, the clicked row only, after the double-click interval.
/// - Dragging: startDrag() drags the selection's ComponentMimeData (none when the Rocket is
///   selected) and never removes rows afterwards (QAbstractItemView's clearOrRemove() would ask
///   the model to remove the moved rows, which the drop moved already). A collapsed row hovered
///   for 2 seconds during a drag expands (JTree's 2000 ms timer).
/// - Dropping: the model's rules (ComponentDrop); a drag of another document's components is
///   always a copy, so the view proposes the copy action for it (Java's canImportTarget()).
///
/// THE STATE ACROSS A CHANGE OF THE TREE. The model answers every change event of the rocket
/// with the TREE or the UNDO bit with a model reset (ComponentTreeModel), after which a QTreeView
/// has nothing selected and nothing expanded. So the view saves, in modelAboutToBeReset, the ids
/// of what it shows (the mirror's ids only: no component may be read there): the selected rows
/// in their order, each with the ids of its path from the Rocket, the current row, the expanded
/// rows that are visible (JTree.getExpandedDescendants() returns those only) and the scroll bars.
/// After the reset (OpenRocket's ComponentTreeModel.fireTreeStructureChanged()):
/// - after an undo or redo (the TREE and UNDO bits, which Rocket::loadFrom() always fires) the
///   whole tree is expanded (ComponentTreeModel.expandAll());
/// - after another change of the tree the saved rows are expanded again, and the source of the
///   event (the parent of an added, removed or moved component) with its ancestors;
/// - the selection is each saved row again, or the nearest of its ancestors that still exists,
///   each once, in the saved order, in one selection change (restoreSelection(),
///   findNearestExistingComponent()), with each made visible;
/// - the scroll bars are put back, and the selection scrolled into view.
/// Then stateRestored() tells that the selection is final (DocumentSelectionModel's changed()).
///
/// The model must outlive the view; the view shows no other (setModel() is a BugError) and keeps
/// the selection model QTreeView made for it (DocumentSelectionModel listens to that one).
///
/// Deviations from OpenRocket:
/// - A click to the right of a row's name, in its indentation or on its expand handle is a click
///   on the row: JTree counts any point outside the name's label as blank space, so that such a
///   click clears the selection there (also the click that expands or collapses a row), and a
///   double-click there edits nothing here either.
/// - Shift and a click selects the range from the anchor as Qt's views do, keeping the rows
///   picked before with Ctrl; JTree's setSelectionInterval() replaces them.
/// - A selection made with the mouse scrolls as Qt's views do (see above); OpenRocket scrolls
///   every selected row into view at once, which can move a row away under the mouse.
/// - The keyboard's context menu key asks for the menu too, at the current row.
class ComponentTreeView final : public QTreeView
{
    Q_OBJECT

public:
    /// BasicTreeUI.setLeftChildIndent(15).
    static constexpr int kIndentation = 15;
    /// How long a collapsed row is hovered during a drag before it expands (JTree's TreeTimer).
    static constexpr int kAutoExpandDelay = 2000;

    /// The view of @p model, which must outlive it.
    explicit ComponentTreeView(ComponentTreeModel& model, QWidget* parent = nullptr);
    ~ComponentTreeView() override;

    ComponentTreeView(const ComponentTreeView&)            = delete;
    ComponentTreeView& operator=(const ComponentTreeView&) = delete;
    ComponentTreeView(ComponentTreeView&&)                 = delete;
    ComponentTreeView& operator=(ComponentTreeView&&)      = delete;

    /// The model, set at construction.
    [[nodiscard]] ComponentTreeModel& componentModel() const noexcept { return *m_model; }

    /// A BugError: the view shows the model it was made with, and only that one.
    void setModel(QAbstractItemModel* model) override;

    /// Whether the row at @p index is visible: all its ancestors are expanded (JTree.isVisible()).
    [[nodiscard]] bool isRowVisible(const QModelIndex& index) const;

    /// The visible rows from the top, the Rocket first.
    [[nodiscard]] QModelIndexList visibleRows() const;

    /// Selects every visible row but the Rocket's, in their order (Ctrl+A).
    void selectAll() override;

    /// Whether the view is putting back its state after a model reset (until stateRestored()).
    [[nodiscard]] bool isRestoring() const noexcept { return m_restoring; }

    /// Asks @p allowed before each request to edit (an empty function allows all, the default);
    /// while it answers false, a double-click neither changes the selection nor asks.
    void setEditAllowed(std::function<bool()> allowed);

Q_SIGNALS:
    /// The row of @p component (never null) was double-clicked: open its configuration dialog,
    /// for it alone or, when @p withSelection, for every selected component, @p component
    /// being the one whose dialog it is. The pointer is valid during the emission only: connect
    /// directly.
    void editRequested(QtRocket::RocketComponent* component, bool withSelection);

    /// The context menu was asked for at @p globalPosition, the selection being its subject.
    void contextMenuRequested(const QPoint& globalPosition);

    /// The view has put back its state after a model reset: the selection is final.
    void stateRestored();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void startDrag(Qt::DropActions supportedActions) override;
    /// QTreeView's, then the copy action for another document's components (ComponentDrop).
    void dragMoveEvent(QDragMoveEvent* event) override;
    /// QTreeView's with the copy action for another document's components.
    void dropEvent(QDropEvent* event) override;
    /// The rows from the top of @p rect to its bottom (a range selected with Shift, or by
    /// dragging over rows), in their order from the top, as JTree's getPathBetweenRows() gives
    /// them: QTreeView puts a child's rows before its parent's, which would make a child the
    /// first selected component.
    void setSelection(const QRect& rect, QItemSelectionModel::SelectionFlags command) override;

private:
    /// What saveState() keeps of the view through a model reset (ids only).
    struct SavedState
    {
        /// The id path (the Rocket's id first) of each selected row, in selection order.
        std::vector<std::vector<Uuid>> selection;
        /// The id path of the current row; empty when there is none.
        std::vector<Uuid> current;
        /// The ids of the expanded rows that are visible.
        std::vector<Uuid> expanded;
        int               verticalScroll   = 0;
        int               horizontalScroll = 0;
    };

    /// modelAboutToBeReset: saves the state (from the model's mirror, reading no component).
    void saveState();
    /// modelReset: puts the state back (the class comment).
    void restoreState();
    /// The ids of the rows from the Rocket to @p index; empty for an invalid index.
    [[nodiscard]] std::vector<Uuid> idPath(const QModelIndex& index) const;
    /// The row of the last id of @p path that is in the tree; invalid when none is.
    [[nodiscard]] QModelIndex nearestExisting(const std::vector<Uuid>& path) const;
    /// Expands the ancestors of @p index (JTree.makeVisible()).
    void makeVisible(const QModelIndex& index);
    /// Expands every visible row from @p index down that allows children but has none
    /// (BasicTree.expandChildlessNodes()).
    void expandChildless(const QModelIndex& index);
    /// Appends the visible rows from @p index down to @p rows.
    void collectVisible(const QModelIndex& index, QModelIndexList& rows) const;
    /// Selects @p rows in their order, each once, as one change (ClearAndSelect).
    void selectRows(const QModelIndexList& rows);
    /// Scrolls every selected row into view, in selection order.
    void scrollToSelection();
    /// QItemSelectionModel::selectionChanged.
    void onSelectionChanged();
    /// QTreeView::expanded.
    void onExpanded(const QModelIndex& index);
    /// QTreeView::collapsed: moves the selection off the hidden descendants of @p collapsed.
    void onCollapsed(const QModelIndex& collapsed);
    /// What the drag shows: the rows at @p indexes as the delegate draws them selected, and
    /// their rectangle in the viewport in @p rect.
    [[nodiscard]] QPixmap dragPixmap(const QModelIndexList& indexes, QRect& rect) const;
    /// Whether editRequested() may be emitted now.
    [[nodiscard]] bool editAllowed() const;
    /// Whether @p event carries components of another document than the view's (a copy).
    [[nodiscard]] bool isForeignDrag(const QDropEvent& event) const;

    ComponentTreeModel*       m_model;
    std::optional<SavedState> m_saved;
    std::function<bool()>     m_editAllowed;
    QPoint                    m_pressPosition;
    bool                      m_restoring{false};
    bool                      m_mouseSelecting{false};
};

}  // namespace QtRocket::Gui
