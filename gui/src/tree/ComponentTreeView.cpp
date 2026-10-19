#include "QtRocket/gui/tree/ComponentTreeView.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

#include <QAbstractItemDelegate>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QContextMenuEvent>
#include <QDrag>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMimeData>
#include <QModelIndex>
#include <QMouseEvent>
#include <QObject>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QString>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTreeView>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>
#include <QtTypes>

#include "QtRocket/gui/tree/ComponentTreeDelegate.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

/// Whether @p row is a descendant of @p ancestor (not @p ancestor itself).
[[nodiscard]] bool isDescendantOf(const QModelIndex& row, const QModelIndex& ancestor)
{
    for (QModelIndex parent = row.parent(); parent.isValid(); parent = parent.parent())
    {
        if (parent == ancestor)
        {
            return true;
        }
    }
    return false;
}

/// What the view's slots report a failure as.
[[nodiscard]] QString context()
{
    return QStringLiteral("Component tree view");
}

}  // namespace

ComponentTreeView::ComponentTreeView(ComponentTreeModel& model, QWidget* parent)
  : QTreeView(parent), m_model(&model)
{
    setObjectName(QStringLiteral("componentTree"));
    // BasicTree: no handle on the Rocket, 15 pixels of indentation, no toggle on double-click.
    setHeaderHidden(true);
    setRootIsDecorated(false);
    setIndentation(kIndentation);
    setExpandsOnDoubleClick(false);
    // BasicFrame's DISCONTIGUOUS_TREE_SELECTION.
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    // ComponentTree: drag enabled, drops (the model's rules) between and onto rows; a plain drag
    // moves (COPY_OR_MOVE, MOVE without a modifier).
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    setAutoExpandDelay(kAutoExpandDelay);
    setItemDelegate(makeChild<ComponentTreeDelegate>(this));
    QTreeView::setModel(&model);

    connect(&model, &QAbstractItemModel::modelAboutToBeReset, this,
            guarded([this] { saveState(); }, context()));
    connect(&model, &QAbstractItemModel::modelReset, this,
            guarded([this] { restoreState(); }, context()));
    connect(selectionModel(), &QItemSelectionModel::selectionChanged, this,
            guarded([this] { onSelectionChanged(); }, context()));
    connect(this, &QTreeView::expanded, this,
            guarded([this](const QModelIndex& index) { onExpanded(index); }, context()));
    connect(this, &QTreeView::collapsed, this,
            guarded([this](const QModelIndex& index) { onCollapsed(index); }, context()));

    // ComponentTree's expandTree(), and BasicTree's childless rows.
    expandAll();
    expandChildless(model.rocketIndex());
}

ComponentTreeView::~ComponentTreeView() = default;

void ComponentTreeView::setModel(QAbstractItemModel* /*model*/)
{
    bug("ComponentTreeView shows the model it was made with only");
}

bool ComponentTreeView::isRowVisible(const QModelIndex& index) const
{
    if (!index.isValid() || index.model() != m_model)
    {
        return false;
    }
    for (QModelIndex parent = index.parent(); parent.isValid(); parent = parent.parent())
    {
        if (!isExpanded(parent))
        {
            return false;
        }
    }
    return true;
}

QModelIndexList ComponentTreeView::visibleRows() const
{
    QModelIndexList   rows;
    const QModelIndex rocket = m_model->rocketIndex();
    if (rocket.isValid())
    {
        collectVisible(rocket, rows);
    }
    return rows;
}

void ComponentTreeView::selectAll()
{
    // ComponentTree's key listener: setSelectionInterval(1, getRowCount()), row 0 the Rocket.
    QModelIndexList rows = visibleRows();
    if (!rows.isEmpty())
    {
        rows.removeFirst();
    }
    selectRows(rows);
}

void ComponentTreeView::setEditAllowed(std::function<bool()> allowed)
{
    m_editAllowed = std::move(allowed);
}

// ---------------------------------------------------------------------------------- events

void ComponentTreeView::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::SelectAll))
    {
        selectAll();
        event->accept();
        return;
    }
    // DesignPanel removes JTree's Ctrl+X, C, V, S, O and N: they belong to the window's actions.
    // QAbstractItemView would copy the current row's text to the system clipboard (and on macOS
    // take Cmd+O for activated()).
    if (event->matches(QKeySequence::Copy) || event->matches(QKeySequence::Cut) ||
        event->matches(QKeySequence::Paste) || event->matches(QKeySequence::Save) ||
        event->matches(QKeySequence::Open) || event->matches(QKeySequence::New))
    {
        event->ignore();
        return;
    }
    QTreeView::keyPressEvent(event);
}

void ComponentTreeView::mousePressEvent(QMouseEvent* event)
{
    m_pressPosition = event->position().toPoint();
    {
        const QScopedValueRollback mouse(m_mouseSelecting, true);
        QTreeView::mousePressEvent(event);
    }
    // DesignPanel: a click of any button where there is no row clears the selection.
    if (!indexAt(m_pressPosition).isValid())
    {
        clearSelection();
    }
}

void ComponentTreeView::mouseMoveEvent(QMouseEvent* event)
{
    const QScopedValueRollback mouse(m_mouseSelecting, true);
    QTreeView::mouseMoveEvent(event);
}

void ComponentTreeView::mouseReleaseEvent(QMouseEvent* event)
{
    const QScopedValueRollback mouse(m_mouseSelecting, true);
    QTreeView::mouseReleaseEvent(event);
}

void ComponentTreeView::mouseDoubleClickEvent(QMouseEvent* event)
{
    const QPoint position = event->position().toPoint();
    // Persistent: a slot of QAbstractItemView's doubleClicked() may change the tree.
    const QPersistentModelIndex index(indexAt(position));
    const bool                  onRow = index.isValid();
    {
        const QScopedValueRollback mouse(m_mouseSelecting, true);
        QTreeView::mouseDoubleClickEvent(event);
    }
    if (!onRow)
    {
        clearSelection();
        return;
    }
    // Only a double-click of the first button on the row itself (not on its expand handle or in
    // its indentation) asks.
    if (!index.isValid() || event->button() != Qt::LeftButton ||
        !visualRect(index).contains(position) || !editAllowed())
    {
        return;
    }
    RocketComponent* component = m_model->component(index);
    if (component == nullptr)
    {
        return;
    }
    const bool withSelection =
        (event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) != Qt::NoModifier;
    if (withSelection && !selectionModel()->isSelected(index))
    {
        // DesignPanel adds the row's path to the selection's paths (setSelectionPaths()).
        selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
    Q_EMIT editRequested(component, withSelection);
}

void ComponentTreeView::contextMenuEvent(QContextMenuEvent* event)
{
    event->accept();
    if (event->reason() != QContextMenuEvent::Mouse)
    {
        // The keyboard's menu key (OpenRocket has none): the menu of the selection, at the
        // current row.
        if (!selectionModel()->hasSelection())
        {
            return;
        }
        const QModelIndex row =
            currentIndex().isValid() ? currentIndex() : selectionModel()->selectedIndexes().first();
        Q_EMIT contextMenuRequested(viewport()->mapToGlobal(visualRect(row).center()));
        return;
    }
    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid())
    {
        clearSelection();
        return;
    }
    if (!selectionModel()->isSelected(index))
    {
        selectionModel()->setCurrentIndex(
            index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    Q_EMIT contextMenuRequested(event->globalPos());
}

void ComponentTreeView::startDrag(Qt::DropActions supportedActions)
{
    // Every selected row: the model makes no drag when the Rocket is among them (Java's
    // createTransferable()), where QAbstractItemView would leave the Rocket out.
    const QModelIndexList      indexes = selectionModel()->selectedIndexes();
    std::unique_ptr<QMimeData> dragged(m_model->mimeData(indexes));
    if (dragged == nullptr)
    {
        return;
    }
    QRect         rect;
    const QPixmap pixmap = dragPixmap(indexes, rect);
    auto*         drag   = makeChild<QDrag>(this);
    drag->setPixmap(pixmap);
    drag->setHotSpot(m_pressPosition - rect.topLeft());
    const Qt::DropAction defaultAction =
        supportedActions.testFlag(defaultDropAction()) ? defaultDropAction() : Qt::CopyAction;
    // The drag owns the data, and Qt deletes the drag when it ends. Whatever the drop did, the
    // rows stay: the drop has moved the components already, where QAbstractItemView would ask
    // the model to remove the rows after a move (clearOrRemove()).
    drag->setMimeData(dragged.release());
    static_cast<void>(drag->exec(supportedActions, defaultAction));
}

void ComponentTreeView::setSelection(const QRect& rect, QItemSelectionModel::SelectionFlags command)
{
    // The rows as QTreeView::setSelection() finds them: those at the top and the bottom of the
    // rectangle, the first or the last row where there is none.
    const QModelIndexList rows = visibleRows();
    if (rect.isNull() || rows.isEmpty())
    {
        return;
    }
    const QModelIndex top    = indexAt(QPoint(0, std::min(rect.top(), rect.bottom())));
    const QModelIndex bottom = indexAt(QPoint(0, std::max(rect.top(), rect.bottom())));
    const qsizetype   first  = top.isValid() ? rows.indexOf(top) : 0;
    const qsizetype   last   = bottom.isValid() ? rows.indexOf(bottom) : rows.size() - 1;
    QItemSelection    selection;
    for (qsizetype i = std::min(first, last); i <= std::max(first, last); ++i)
    {
        if (i >= 0)
        {
            selection.select(rows.at(i), rows.at(i));
        }
    }
    selectionModel()->select(selection, command);
}

// ----------------------------------------------------------------------- the saved state

void ComponentTreeView::saveState()
{
    SavedState state;
    for (const QModelIndex& index : selectionModel()->selectedIndexes())
    {
        std::vector<Uuid> path = idPath(index);
        if (!path.empty())
        {
            state.selection.push_back(std::move(path));
        }
    }
    state.current = idPath(currentIndex());
    for (const QModelIndex& row : visibleRows())
    {
        const std::optional<Uuid> id = m_model->componentId(row);
        if (id.has_value() && isExpanded(row))
        {
            state.expanded.push_back(*id);
        }
    }
    state.verticalScroll   = verticalScrollBar()->value();
    state.horizontalScroll = horizontalScrollBar()->value();
    m_saved                = std::move(state);
}

void ComponentTreeView::restoreState()
{
    {
        const QScopedValueRollback restoring(m_restoring, true);
        const SavedState saved = std::exchange(m_saved, std::nullopt).value_or(SavedState{});
        const int        type  = m_model->lastResetChangeType();
        const bool tree = ComponentChangeEvent::matches(ComponentChangeEvent::Type::TREE, type);
        const bool undo = ComponentChangeEvent::matches(ComponentChangeEvent::Type::UNDO, type);
        // JTree marks a new root expanded.
        const QModelIndex rocket = m_model->rocketIndex();
        expand(rocket);
        if (tree && undo)
        {
            expandAll();
        }
        else
        {
            for (const Uuid& id : saved.expanded)
            {
                expand(m_model->indexOf(id));
            }
            const std::optional<Uuid>& source = m_model->lastResetSource();
            if ((tree || undo) && source.has_value())
            {
                const QModelIndex sourceIndex = m_model->indexOf(*source);
                makeVisible(sourceIndex);
                expand(sourceIndex);
            }
        }
        QModelIndexList selection;
        for (const std::vector<Uuid>& path : saved.selection)
        {
            const QModelIndex index = nearestExisting(path);
            if (index.isValid() && !selection.contains(index))
            {
                selection.append(index);
                makeVisible(index);
            }
        }
        expandChildless(rocket);
        const QModelIndex current = nearestExisting(saved.current);
        if (current.isValid())
        {
            selectionModel()->setCurrentIndex(current, QItemSelectionModel::NoUpdate);
        }
        selectRows(selection);
        executeDelayedItemsLayout();
        verticalScrollBar()->setValue(saved.verticalScroll);
        horizontalScrollBar()->setValue(saved.horizontalScroll);
        scrollToSelection();
    }
    Q_EMIT stateRestored();
}

std::vector<Uuid> ComponentTreeView::idPath(const QModelIndex& index) const
{
    std::vector<Uuid> path;
    for (QModelIndex row = index; row.isValid(); row = row.parent())
    {
        const std::optional<Uuid> id = m_model->componentId(row);
        if (!id.has_value())
        {
            return {};
        }
        path.push_back(*id);
    }
    std::ranges::reverse(path);
    return path;
}

QModelIndex ComponentTreeView::nearestExisting(const std::vector<Uuid>& path) const
{
    for (const Uuid& id : std::views::reverse(path))
    {
        const QModelIndex index = m_model->indexOf(id);
        if (index.isValid())
        {
            return index;
        }
    }
    return {};
}

void ComponentTreeView::makeVisible(const QModelIndex& index)
{
    for (QModelIndex parent = index.parent(); parent.isValid(); parent = parent.parent())
    {
        expand(parent);
    }
}

void ComponentTreeView::expandChildless(const QModelIndex& index)
{
    if (!isRowVisible(index))
    {
        return;
    }
    const int rows = m_model->rowCount(index);
    if (rows == 0)
    {
        if (!m_model->flags(index).testFlag(Qt::ItemNeverHasChildren) && !isExpanded(index))
        {
            expand(index);
        }
        return;
    }
    if (!isExpanded(index))
    {
        return;
    }
    for (int row = 0; row < rows; ++row)
    {
        expandChildless(m_model->index(row, 0, index));
    }
}

void ComponentTreeView::collectVisible(const QModelIndex& index, QModelIndexList& rows) const
{
    rows.append(index);
    if (!isExpanded(index))
    {
        return;
    }
    const int count = m_model->rowCount(index);
    for (int row = 0; row < count; ++row)
    {
        collectVisible(m_model->index(row, 0, index), rows);
    }
}

void ComponentTreeView::selectRows(const QModelIndexList& rows)
{
    QItemSelection selection;
    for (const QModelIndex& row : rows)
    {
        if (!selection.contains(row))
        {
            selection.select(row, row);
        }
    }
    selectionModel()->select(selection,
                             QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void ComponentTreeView::scrollToSelection()
{
    for (const QModelIndex& index : selectionModel()->selectedIndexes())
    {
        makeVisible(index);
        scrollTo(index);
    }
}

// ----------------------------------------------------------------------------------- slots

void ComponentTreeView::onSelectionChanged()
{
    if (m_restoring || m_mouseSelecting)
    {
        return;
    }
    scrollToSelection();
}

void ComponentTreeView::onExpanded(const QModelIndex& index)
{
    if (m_restoring)
    {
        return;
    }
    expandChildless(index);
}

void ComponentTreeView::onCollapsed(const QModelIndex& collapsed)
{
    if (m_restoring)
    {
        return;
    }
    // JTree.setExpandedState(): a selected descendant is deselected, and the collapsed row
    // selected instead.
    QModelIndexList kept;
    bool            moved = false;
    for (const QModelIndex& selected : selectionModel()->selectedIndexes())
    {
        if (isDescendantOf(selected, collapsed))
        {
            moved = true;
        }
        else
        {
            kept.append(selected);
        }
    }
    if (!moved)
    {
        return;
    }
    if (!kept.contains(collapsed))
    {
        kept.append(collapsed);
    }
    if (isDescendantOf(currentIndex(), collapsed))
    {
        selectionModel()->setCurrentIndex(collapsed, QItemSelectionModel::NoUpdate);
    }
    selectRows(kept);
}

// --------------------------------------------------------------------------------- helpers

QPixmap ComponentTreeView::dragPixmap(const QModelIndexList& indexes, QRect& rect) const
{
    // As QAbstractItemView's renderToPixmap(): each visible row as its delegate draws it.
    rect = QRect();
    std::vector<std::pair<QModelIndex, QRect>> rows;
    const QRect                                area = viewport()->rect();
    for (const QModelIndex& index : indexes)
    {
        const QRect row = visualRect(index).intersected(area);
        if (!row.isEmpty())
        {
            rows.emplace_back(index, row);
            rect |= row;
        }
    }
    if (rect.isEmpty())
    {
        return {};
    }
    const qreal scale = devicePixelRatioF();
    QPixmap     pixmap(rect.size() * scale);
    pixmap.setDevicePixelRatio(scale);
    pixmap.fill(Qt::transparent);
    QPainter             painter(&pixmap);
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.state |= QStyle::State_Selected;
    for (const auto& [index, row] : rows)
    {
        option.rect = row.translated(-rect.topLeft());
        itemDelegateForIndex(index)->paint(&painter, option, index);
    }
    return pixmap;
}

bool ComponentTreeView::editAllowed() const
{
    return !m_editAllowed || m_editAllowed();
}

}  // namespace QtRocket::Gui
