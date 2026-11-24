#include "QtRocket/gui/dialogs/simulation/MultiLevelWindTableView.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <QAbstractItemDelegate>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAction>
#include <QContextMenuEvent>
#include <QEvent>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QModelIndex>
#include <QModelIndexList>
#include <QMouseEvent>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QSize>
#include <QString>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/dialogs/simulation/MultiLevelWindTableModel.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using Model = MultiLevelWindTableModel;

/// The context of the table's reports (Guard).
QString context()
{
    return QStringLiteral("Wind levels");
}

/// Java's CELL_PADDING, around a header cell's contents.
constexpr int kCellPadding = 8;

/// The object name of the unit selector of a number column (Java's fields).
[[nodiscard]] QString unitSelectorName(int column)
{
    switch (column)
    {
        case Model::kAltitudeColumn:
            return QStringLiteral("altitudeUnitSelector");
        case Model::kSpeedColumn:
            return QStringLiteral("speedUnitSelector");
        case Model::kDirectionColumn:
            return QStringLiteral("directionUnitSelector");
        case Model::kDeviationColumn:
            return QStringLiteral("stdDeviationUnitSelector");
        default:
            return QStringLiteral("turbulenceUnitSelector");
    }
}

/// The cells' delegate: a number cell is edited in a UnitSpinBox over its binding
/// (MultiLevelWindTableModel::makeCellBinding()), which shows the level's value and commits into
/// it (Java: the row's JSpinner over its DoubleModel); the selected row is drawn in
/// OR.colors.table.selection (unless it flashes), and the Delete column's icon in the middle.
class LevelCellDelegate final : public QStyledItemDelegate
{
public:
    /// Told of every editor the delegate opens, with its cell.
    using Opened = std::function<void(QWidget&, const QModelIndex&)>;

    LevelCellDelegate(Model& model, Opened opened, QObject* parent)
      : QStyledItemDelegate(parent), m_model(&model), m_opened(std::move(opened))
    {
    }

    QWidget* createEditor(QWidget*           parent, const QStyleOptionViewItem& /*option*/,
                          const QModelIndex& index) const override
    {
        UnitSpinBox* editor = nullptr;
        Guard::run(
            [&] {
                std::shared_ptr<DoubleBinding> binding =
                    m_model->makeCellBinding(index.row(), index.column());
                if (binding != nullptr)
                {
                    editor = makeChild<UnitSpinBox>(parent, std::move(binding));
                    editor->setObjectName(QStringLiteral("cellEditor"));
                    editor->setFrame(false);
                    m_opened(*editor, index);
                }
            },
            context());
        return editor;
    }

    void setEditorData(QWidget* /*editor*/, const QModelIndex& /*index*/) const override
    {
        // The editor shows its binding, which follows the level (Java's spinner its DoubleModel).
    }

    void setModelData(QWidget* editor, QAbstractItemModel* /*model*/,
                      const QModelIndex& /*index*/) const override
    {
        if (auto* const spinBox = qobject_cast<UnitSpinBox*>(editor); spinBox != nullptr)
        {
            Guard::run([spinBox] { spinBox->commitEdit(); }, context());
        }
    }

protected:
    /// Escape drops the editor's edit before it closes (Qt's delegate closes it, and the spin
    /// box would commit its edited text when it loses the focus then).
    bool eventFilter(QObject* object, QEvent* event) override
    {
        auto* const       spinBox = qobject_cast<UnitSpinBox*>(object);
        const auto* const key     = dynamic_cast<const QKeyEvent*>(event);
        if (spinBox != nullptr && event->type() == QEvent::KeyPress && key != nullptr &&
            key->key() == Qt::Key_Escape)
        {
            spinBox->setBinding(spinBox->binding());
            Q_EMIT closeEditor(spinBox, QAbstractItemDelegate::RevertModelCache);
            return true;
        }
        return QStyledItemDelegate::eventFilter(object, event);
    }

    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        if (option->state.testFlag(QStyle::State_Selected))
        {
            // Java: the selected row's background is OR.colors.table.selection (a flash's colour
            // wins), its text as the others'.
            option->state.setFlag(QStyle::State_Selected, false);
            if (!m_model->isFlashing(index.row()))
            {
                option->backgroundBrush =
                    ThemeColors::color(ThemeColors::Key::TABLE_SELECTION, option->palette);
            }
        }
        if (index.column() == Model::kDeleteColumn)
        {
            option->decorationPosition  = QStyleOptionViewItem::Top;
            option->decorationAlignment = Qt::AlignCenter;
        }
    }

private:
    Model* m_model;
    Opened m_opened;
};

}  // namespace

MultiLevelWindTableView::MultiLevelWindTableView(MultiLevelWindTableModel& model, QWidget* parent)
  : QTableView(parent), m_model(&model)
{
    setObjectName(QStringLiteral("windTable"));
    setModel(&model);
    setItemDelegate(makeChild<LevelCellDelegate>(
        this, model, LevelCellDelegate::Opened([this](QWidget& editor, const QModelIndex& index) {
            editorOpened(editor, index);
        })));
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    // A press of the left button edits a number cell too (mousePressEvent()).
    setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                    QAbstractItemView::AnyKeyPressed);
    setWordWrap(false);
    verticalHeader()->hide();

    QHeaderView* const header = horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(false);
    for (int column = 0; column < Model::kColumnCount; ++column)
    {
        setColumnWidth(column, kColumnWidths.at(static_cast<std::size_t>(column)));
    }
    int selectorHeight = 0;
    for (int column = 0; column < Model::kNumberColumnCount; ++column)
    {
        auto* const selector = makeChild<UnitSelector>(header, Model::columnUnitGroup(column));
        selector->setObjectName(unitSelectorName(column));
        selector->setSelectedUnit(model.columnUnit(column));
        connect(selector, &UnitSelector::unitSelected, this,
                guarded([this, column,
                         selector] { m_model->setColumnUnit(column, selector->getSelectedUnit()); },
                        context()));
        selectorHeight = std::max(selectorHeight, selector->sizeHint().height());
        m_unitSelectors.at(static_cast<std::size_t>(column)) = selector;
    }
    // Java: the column's name, a gap, the unit selector (Box.createVerticalStrut(3)).
    header->setMinimumHeight(header->sizeHint().height() + kHeaderGap + selectorHeight);

    connect(this, &QAbstractItemView::clicked, this,
            guarded(
                [this](const QModelIndex& index) {
                    // Java: the row's delete button.
                    if (index.column() == Model::kDeleteColumn)
                    {
                        deleteLevel(index.row());
                    }
                },
                context()));
    connect(selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this] { Q_EMIT selectedLevelChanged(); });
    connect(m_model, &MultiLevelWindTableModel::levelsChanged, this,
            [this] { Q_EMIT selectedLevelChanged(); });

    // Java: installContextMenu(), the rows' popup.
    m_popupMenu = makeChild<QMenu>(this);
    m_popupMenu->setObjectName(QStringLiteral("popupMenu"));
    //: MultiLevelWindTable.popupmenu.Delete
    m_deleteAction =
        m_popupMenu->addAction(Icons::icon(Icons::Id::EDIT_DELETE), tr("Delete level"));
    m_deleteAction->setObjectName(QStringLiteral("deleteItem"));
    connect(m_deleteAction, &QAction::triggered, this,
            guarded([this] { deletePopupLevel(); }, context()));
}

MultiLevelWindTableView::~MultiLevelWindTableView() = default;

UnitSelector& MultiLevelWindTableView::unitSelector(int column) const
{
    if (column < 0 || column >= Model::kNumberColumnCount)
    {
        bug("The column " + std::to_string(column) + " of the wind levels' table has no unit");
    }
    return *m_unitSelectors.at(static_cast<std::size_t>(column));
}

int MultiLevelWindTableView::selectedRow() const
{
    const QModelIndexList selected = selectionModel()->selectedIndexes();
    return selected.isEmpty() ? -1 : selected.first().row();
}

MultiLevelWindTableModel::Level* MultiLevelWindTableView::selectedLevel() const
{
    return m_model->levelAt(selectedRow());
}

int MultiLevelWindTableView::addLevel()
{
    commitEditing();
    const int row = m_model->addLevel();
    if (row >= 0)
    {
        // Java: the new row is scrolled to.
        scrollTo(m_model->index(row, 0));
    }
    return row;
}

bool MultiLevelWindTableView::deleteSelectedLevel()
{
    commitEditing();
    return deleteLevel(selectedRow());
}

bool MultiLevelWindTableView::deleteLevel(int row)
{
    const MultiLevelWindTableModel::Level* const level = m_model->levelAt(row);
    if (level == nullptr)
    {
        return false;
    }
    // A commit of an open editor may move the rows: the level is found again after it.
    const std::weak_ptr<const MultiLevelWindTableModel::Level> deleted = level->weak_from_this();
    commitEditing();
    if (!m_model->deleteLevel(m_model->rowOf(deleted.lock().get())))
    {
        return false;
    }
    // Java: selectRow(null).
    clearSelection();
    return true;
}

QWidget* MultiLevelWindTableView::openEditor() const
{
    if (m_editor.isNull() || !m_editorIndex.isValid())
    {
        return nullptr;
    }
    return indexWidget(m_editorIndex) == m_editor.data() ? m_editor.data() : nullptr;
}

bool MultiLevelWindTableView::commitEditing()
{
    const QPointer<QWidget> editor(openEditor());
    if (editor.isNull())
    {
        return false;
    }
    // What the delegate does when the editor loses the focus: the text committed, the editor
    // closed.
    QAbstractItemDelegate& delegate = *itemDelegate();
    Q_EMIT delegate.commitData(editor.data());
    if (!editor.isNull() && openEditor() == editor.data())
    {
        Q_EMIT delegate.closeEditor(editor.data(), QAbstractItemDelegate::NoHint);
    }
    return true;
}

void MultiLevelWindTableView::showPopupMenu(int row, const QPoint& globalPosition)
{
    MultiLevelWindTableModel::Level* const level = m_model->levelAt(row);
    if (level == nullptr)
    {
        return;
    }
    // Java: popupMenuWillBecomeVisible() selects the row.
    const QModelIndex current = currentIndex();
    selectionModel()->setCurrentIndex(
        m_model->index(row, current.isValid() ? current.column() : 0),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_popupLevel = level->weak_from_this();
    m_deleteAction->setEnabled(m_model->rowCount() > 1);
    m_popupMenu->popup(globalPosition);
}

void MultiLevelWindTableView::deletePopupLevel()
{
    const int row = m_model->rowOf(m_popupLevel.lock().get());
    m_popupLevel.reset();
    deleteLevel(row);
}

void MultiLevelWindTableView::editorOpened(QWidget& editor, const QModelIndex& index)
{
    m_editor      = &editor;
    m_editorIndex = index;
}

// ------------------------------------------------------------------------------------- the events

void MultiLevelWindTableView::mousePressEvent(QMouseEvent* event)
{
    QTableView::mousePressEvent(event);
    if (event->button() != Qt::LeftButton || state() == QAbstractItemView::EditingState)
    {
        return;
    }
    // Java: a click into a cell's spinner edits it.
    if (const QModelIndex index = indexAt(event->position().toPoint());
        index.isValid() && index.flags().testFlag(Qt::ItemIsEditable))
    {
        edit(index);
    }
}

void MultiLevelWindTableView::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Delete) && state() != QAbstractItemView::EditingState)
    {
        // QtRocket's: the Delete key deletes the selected row's level.
        event->accept();
        Guard::run([this] { deleteSelectedLevel(); }, context());
        return;
    }
    QTableView::keyPressEvent(event);
}

void MultiLevelWindTableView::contextMenuEvent(QContextMenuEvent* event)
{
    event->accept();
    const bool        mouse = event->reason() == QContextMenuEvent::Mouse;
    const QModelIndex index = mouse ? indexAt(event->pos()) : currentIndex();
    if (!index.isValid())
    {
        // Java: the popup belongs to the rows.
        return;
    }
    const QPoint position =
        mouse ? event->globalPos() : viewport()->mapToGlobal(visualRect(index).center());
    Guard::run([this, &index, &position] { showPopupMenu(index.row(), position); }, context());
}

void MultiLevelWindTableView::updateGeometries()
{
    QTableView::updateGeometries();
    layOutUnitSelectors();
}

void MultiLevelWindTableView::scrollContentsBy(int dx, int dy)
{
    QTableView::scrollContentsBy(dx, dy);
    layOutUnitSelectors();
}

void MultiLevelWindTableView::layOutUnitSelectors()
{
    const QHeaderView* const header = horizontalHeader();
    for (int column = 0; column < Model::kNumberColumnCount; ++column)
    {
        UnitSelector* const selector = m_unitSelectors.at(static_cast<std::size_t>(column));
        if (selector == nullptr)
        {
            continue;
        }
        const QSize size    = selector->sizeHint();
        const int   section = header->sectionSize(column);
        const int   width   = std::min(size.width(), section);
        selector->setGeometry(header->sectionViewportPosition(column) + ((section - width) / 2),
                              header->height() - size.height() - (kCellPadding / 2), width,
                              size.height());
        selector->raise();
    }
}

}  // namespace QtRocket::Gui
