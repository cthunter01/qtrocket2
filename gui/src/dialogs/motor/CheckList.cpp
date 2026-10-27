#include "QtRocket/gui/dialogs/motor/CheckList.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <QAbstractItemView>
#include <QEvent>
#include <QKeyEvent>
#include <QListWidget>
#include <QListWidgetItem>
#include <QModelIndex>
#include <QMouseEvent>
#include <QSize>
#include <QStringList>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// Whether @p indices names @p index.
[[nodiscard]] bool contains(const std::vector<int>& indices, int index)
{
    return std::ranges::find(indices, index) != indices.end();
}

}  // namespace

CheckList::CheckList(QWidget* parent) : QListWidget(parent)
{
    // CheckList's constructor: ListSelectionModel.SINGLE_SELECTION.
    setSelectionMode(QAbstractItemView::SingleSelection);
    connect(this, &QListWidget::itemChanged, this, [this] { itemCheckChanged(); });
}

CheckList::~CheckList() = default;

void CheckList::setItems(const QStringList& texts)
{
    m_updating = true;
    clear();
    addItems(texts);
    for (int row = 0; row < count(); ++row)
    {
        QListWidgetItem* added = item(row);
        added->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable |
                        Qt::ItemNeverHasChildren);
        added->setCheckState(Qt::Unchecked);
    }
    m_updating     = false;
    m_pressedIndex = -1;
    m_doubleClick  = false;
}

void CheckList::checkIndex(int index) const
{
    if (index < 0 || index >= count())
    {
        bug("The check list has no item " + std::to_string(index) + " (it has " +
            std::to_string(count()) + ")");
    }
}

bool CheckList::isChecked(int index) const
{
    checkIndex(index);
    return item(index)->checkState() == Qt::Checked;
}

void CheckList::setCheckState(int index, bool checked)
{
    const bool updating = std::exchange(m_updating, true);
    item(index)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    m_updating = updating;
}

void CheckList::setChecked(int index, bool checked)
{
    checkIndex(index);
    setCheckState(index, checked);
    // DefaultCheckListModel.setCheckedIndex(): fireContentsChanged() whatever the state was.
    Q_EMIT checksChanged();
}

void CheckList::toggleIndex(int index)
{
    if (index >= 0 && index < count())
    {
        setChecked(index, !isChecked(index));
    }
}

void CheckList::checkAll()
{
    for (int row = 0; row < count(); ++row)
    {
        setCheckState(row, true);
    }
    Q_EMIT checksChanged();
}

void CheckList::clearAll()
{
    for (int row = 0; row < count(); ++row)
    {
        setCheckState(row, false);
    }
    Q_EMIT checksChanged();
}

void CheckList::setCheckedIndices(const std::vector<int>& indices)
{
    for (int row = 0; row < count(); ++row)
    {
        setCheckState(row, contains(indices, row));
    }
    Q_EMIT checksChanged();
}

void CheckList::setUncheckedIndices(const std::vector<int>& indices)
{
    for (int row = 0; row < count(); ++row)
    {
        setCheckState(row, !contains(indices, row));
    }
    Q_EMIT checksChanged();
}

std::vector<int> CheckList::checkedIndices() const
{
    std::vector<int> checked;
    for (int row = 0; row < count(); ++row)
    {
        if (isChecked(row))
        {
            checked.push_back(row);
        }
    }
    return checked;
}

std::vector<int> CheckList::uncheckedIndices() const
{
    // getUncheckedItems(): every element from the last to the first, less the checked ones.
    std::vector<int> unchecked;
    for (int row = count() - 1; row >= 0; --row)
    {
        if (!isChecked(row))
        {
            unchecked.push_back(row);
        }
    }
    return unchecked;
}

int CheckList::selectedIndex() const
{
    const QModelIndexList selected = selectedIndexes();
    return selected.isEmpty() ? -1 : selected.front().row();
}

void CheckList::setSizeHints(const QSize& preferred, const QSize& minimum)
{
    m_preferredSize = preferred;
    m_minimumSize   = minimum;
    updateGeometry();
}

QSize CheckList::sizeHint() const
{
    return m_preferredSize.isValid() ? m_preferredSize : QListWidget::sizeHint();
}

QSize CheckList::minimumSizeHint() const
{
    return m_minimumSize.isValid() ? m_minimumSize : QListWidget::minimumSizeHint();
}

void CheckList::itemCheckChanged()
{
    if (!m_updating)
    {
        Q_EMIT checksChanged();
    }
}

void CheckList::mousePressEvent(QMouseEvent* event)
{
    QListWidget::mousePressEvent(event);
    m_doubleClick  = false;
    m_pressedIndex = -1;
    if (event->button() == Qt::LeftButton)
    {
        const QModelIndex index = indexAt(event->position().toPoint());
        m_pressedIndex          = index.isValid() ? index.row() : -1;
    }
}

void CheckList::mouseDoubleClickEvent(QMouseEvent* event)
{
    // Widgets get the second press of a double click as this event (Qt drops the press itself);
    // a platform that sends both is handled as well.
    QListWidget::mouseDoubleClickEvent(event);
    if (event->button() == Qt::LeftButton)
    {
        const QModelIndex index = indexAt(event->position().toPoint());
        m_pressedIndex          = index.isValid() ? index.row() : -1;
        m_doubleClick           = true;
    }
}

void CheckList::mouseReleaseEvent(QMouseEvent* event)
{
    QListWidget::mouseReleaseEvent(event);
    if (event->button() != Qt::LeftButton)
    {
        return;
    }
    const int         pressed     = std::exchange(m_pressedIndex, -1);
    const bool        doubleClick = std::exchange(m_doubleClick, false);
    const QModelIndex index       = indexAt(event->position().toPoint());
    // CheckListEditor.mouseClicked(): a click (press and release on the item) of the left button
    // in the item's bounds toggles it when it is the first click (a click count of 1).
    if (!doubleClick && index.isValid() && index.row() == pressed)
    {
        toggleIndex(pressed);
        event->accept();
    }
}

void CheckList::keyPressEvent(QKeyEvent* event)
{
    // The "toggle-check" action: KeyStroke.getKeyStroke(VK_SPACE, 0), Space without modifiers,
    // on the selected item.
    if (event->key() == Qt::Key_Space && event->modifiers() == Qt::NoModifier)
    {
        toggleIndex(selectedIndex());
        event->accept();
        return;
    }
    QListWidget::keyPressEvent(event);
}

bool CheckList::edit(const QModelIndex& index, QAbstractItemView::EditTrigger trigger,
                     QEvent* event)
{
    if (event != nullptr)
    {
        switch (event->type())
        {
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
            case QEvent::MouseButtonDblClick:
            case QEvent::KeyPress:
                // Qt's delegate would toggle a click on the check box and Space: the list does
                // it as Java does (see the class comment).
                return false;
            default:
                break;
        }
    }
    return QListWidget::edit(index, trigger, event);
}

}  // namespace QtRocket::Gui
