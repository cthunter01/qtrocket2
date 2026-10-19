#include "QtRocket/gui/widgets/SingleRowScrollArea.h"

#include <algorithm>

#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QMargins>
#include <QObject>
#include <QScrollArea>
#include <QScrollBar>
#include <QSize>
#include <QSizePolicy>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>

#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

SingleRowScrollArea::SingleRowScrollArea(QWidget* row, QWidget* parent) : QScrollArea(parent)
{
    QTROCKET_ASSERT(row != nullptr);
    // JScrollPane(view, VERTICAL_SCROLLBAR_NEVER, HORIZONTAL_SCROLLBAR_AS_NEEDED), no border.
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // The row is sized here (layoutRow()), at its preferred size as a Swing viewport sizes it,
    // not at its minimum size, which a resizable QScrollArea would give it.
    setWidgetResizable(false);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setWidget(row);
    row->installEventFilter(this);

    // The scroll bar lives in a container that the area shows and hides; whichever of the two
    // is the area's child carries the visibility.
    QScrollBar* bar    = horizontalScrollBar();
    QWidget*    holder = bar->parentWidget();
    m_scrollBarHolder  = holder != nullptr && holder != this ? holder : bar;
    m_scrollBarHolder->installEventFilter(this);
    m_scrollBarShown = isHorizontalScrollBarShown();
    layoutRow();
}

bool SingleRowScrollArea::isHorizontalScrollBarShown() const
{
    return !m_scrollBarHolder->isHidden();
}

int SingleRowScrollArea::horizontalScrollBarSpace() const
{
    if (!isHorizontalScrollBarShown())
    {
        return 0;
    }
    const QScrollBar*  bar = horizontalScrollBar();
    QStyleOptionSlider option;
    option.initFrom(bar);
    option.orientation = Qt::Horizontal;
    // A transient scroll bar is laid over the viewport (QAbstractScrollArea takes no room for it).
    if (bar->style()->styleHint(QStyle::SH_ScrollBar_Transient, &option, bar) != 0)
    {
        return 0;
    }
    return bar->sizeHint().height();
}

QSize SingleRowScrollArea::sizeHint() const
{
    const int      frame   = 2 * frameWidth();
    const QMargins margins = viewportMargins();
    QSize          size    = rowPreferredSize();
    size +=
        QSize(frame + margins.left() + margins.right(), frame + margins.top() + margins.bottom());
    size.rheight() += horizontalScrollBarSpace();
    return size;
}

QSize SingleRowScrollArea::minimumSizeHint() const
{
    return {QScrollArea::minimumSizeHint().width(), sizeHint().height()};
}

bool SingleRowScrollArea::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == widget() && event->type() == QEvent::LayoutRequest)
    {
        // The row's preferred size may have changed (its layout has just been activated).
        layoutRow();
        updateGeometry();
    }
    else if (watched == m_scrollBarHolder &&
             (event->type() == QEvent::ShowToParent || event->type() == QEvent::HideToParent))
    {
        checkScrollBarVisibility();
    }
    return QScrollArea::eventFilter(watched, event);
}

bool SingleRowScrollArea::viewportEvent(QEvent* event)
{
    if (event->type() == QEvent::Resize)
    {
        layoutRow();
    }
    return QScrollArea::viewportEvent(event);
}

void SingleRowScrollArea::wheelEvent(QWheelEvent* event)
{
    if (!isHorizontalScrollBarShown())
    {
        event->ignore();
        return;
    }
    // Swing scrolls the horizontal bar when there is no vertical one, with either wheel.
    QCoreApplication::sendEvent(horizontalScrollBar(), event);
}

QSize SingleRowScrollArea::rowPreferredSize() const
{
    const QWidget* row = widget();
    if (row == nullptr)
    {
        return {0, 0};
    }
    return row->sizeHint()
        .expandedTo(row->minimumSize())
        .boundedTo(row->maximumSize())
        .expandedTo(QSize(0, 0));
}

void SingleRowScrollArea::layoutRow()
{
    QWidget* row = widget();
    if (row == nullptr)
    {
        return;
    }
    const QSize preferred = rowPreferredSize();
    const QSize view      = viewport()->size();
    const QSize size(std::max(preferred.width(), view.width()),
                     std::max(preferred.height(), view.height()));
    if (row->size() != size)
    {
        row->resize(size);
    }
}

void SingleRowScrollArea::checkScrollBarVisibility()
{
    const bool shown = isHorizontalScrollBarShown();
    if (shown == m_scrollBarShown)
    {
        return;
    }
    m_scrollBarShown = shown;
    updateGeometry();
    Q_EMIT horizontalScrollBarVisibilityChanged(shown);
}

}  // namespace QtRocket::Gui
