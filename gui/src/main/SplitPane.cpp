#include "QtRocket/gui/main/SplitPane.h"

#include <algorithm>

#include <QEvent>
#include <QList>
#include <QObject>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSize>
#include <QSizePolicy>
#include <QSplitter>
#include <QWidget>
#include <QWindow>
#include <Qt>

#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The part of BasicSplitPaneUI.distributeSpace() for a side @p lacking below its minimum
/// @p lackingMinimum: it takes what it lacks from the @p other side when that one keeps its own
/// minimum @p otherMinimum; when it cannot, only a negative length goes.
void takeWhatItLacks(int& lacking, int& other, int lackingMinimum, int otherMinimum)
{
    if (other - (lackingMinimum - lacking) < otherMinimum)
    {
        if (lacking < 0)
        {
            other += lacking;
            lacking = 0;
        }
    }
    else
    {
        other -= lackingMinimum - lacking;
        lacking = lackingMinimum;
    }
}

/// Whether a widget of @p policy may be smaller than its size hint (QSizePolicy::ShrinkFlag).
[[nodiscard]] bool canShrink(QSizePolicy::Policy policy) noexcept
{
    return policy == QSizePolicy::Maximum || policy == QSizePolicy::Preferred ||
           policy == QSizePolicy::Expanding || policy == QSizePolicy::Ignored;
}

}  // namespace

SplitPane::SplitPane(Qt::Orientation orientation, double resizeWeight, double dividerLocation,
                     QWidget* parent)
  : QSplitter(orientation, parent), m_resizeWeight(resizeWeight), m_dividerLocation(dividerLocation)
{
    QTROCKET_ASSERT(resizeWeight >= 0.0 && resizeWeight <= 1.0);
    QTROCKET_ASSERT(dividerLocation >= 0.0 && dividerLocation <= 1.0);
    // How a change of the length is shared is the resize weight's (resizeEvent()); until the
    // divider is placed, both sides stretch alike.
    setChildrenCollapsible(false);
}

int SplitPane::lengthOf(const QSize& size) const
{
    return orientation() == Qt::Horizontal ? size.width() : size.height();
}

int SplitPane::sideMinimum(int index) const
{
    const QWidget* const side = widget(index);
    if (side == nullptr)
    {
        return 0;
    }
    // QSplitter's own minimum of a side (qSmartMinSize()).
    if (const int set = lengthOf(side->minimumSize()); set > 0)
    {
        return set;
    }
    const QSizePolicy::Policy policy = orientation() == Qt::Horizontal
                                           ? side->sizePolicy().horizontalPolicy()
                                           : side->sizePolicy().verticalPolicy();
    if (policy == QSizePolicy::Ignored)
    {
        return 0;
    }
    // An invalid hint (a widget without a layout) is -1.
    const int hint = std::max(lengthOf(side->minimumSizeHint()), 0);
    return canShrink(policy) ? hint : std::max(lengthOf(side->sizeHint()), hint);
}

void SplitPane::resizeEvent(QResizeEvent* event)
{
    // After the divider's first place: until then the sizes are the layout's own.
    if (m_dividerPlaced && event->oldSize().isValid())
    {
        distributeSpace(lengthOf(event->size()) - lengthOf(event->oldSize()));
    }
    QSplitter::resizeEvent(event);
}

void SplitPane::distributeSpace(int change)
{
    // QSplitter has not laid its sides out for its new length yet: sizes() are the old ones.
    const QList<int> sizes = this->sizes();
    if (change == 0 || sizes.size() != 2)
    {
        return;
    }
    const int topExtra =
        static_cast<int>(m_resizeWeight * static_cast<double>(change));  // Java's (int) cast
    int        first         = sizes.at(0) + topExtra;
    int        second        = sizes.at(1) + (change - topExtra);
    const int  firstMinimum  = sideMinimum(0);
    const int  secondMinimum = sideMinimum(1);
    const bool firstValid    = first >= firstMinimum;
    const bool secondValid   = second >= secondMinimum;
    if (!firstValid && !secondValid)
    {
        if (first < 0)
        {
            second += first;
            first = 0;
        }
        else if (second < 0)
        {
            first += second;
            second = 0;
        }
    }
    else if (!firstValid)
    {
        takeWhatItLacks(first, second, firstMinimum, secondMinimum);
    }
    else if (!secondValid)
    {
        takeWhatItLacks(second, first, secondMinimum, firstMinimum);
    }
    setSizes({std::max(first, 0), std::max(second, 0)});
}

void SplitPane::showEvent(QShowEvent* event)
{
    QSplitter::showEvent(event);
    if (m_dividerPlaced || m_waitingForWindow)
    {
        return;
    }
    QWidget* const top = window();
    if (top != this && (top->windowHandle() == nullptr || !top->windowHandle()->isVisible()))
    {
        // The window is being shown, its children first: the layouts above this one (a tab
        // widget's, for one) settle before the window's own show event, when the divider is
        // placed (Java places it once the frame is validated).
        m_waitingForWindow = true;
        top->installEventFilter(this);
        return;
    }
    placeDivider();
}

bool SplitPane::eventFilter(QObject* watched, QEvent* event)
{
    if (m_waitingForWindow && watched == window() && event->type() == QEvent::Show)
    {
        m_waitingForWindow = false;
        watched->removeEventFilter(this);
        if (isVisible())
        {
            placeDivider();
        }
    }
    return QSplitter::eventFilter(watched, event);
}

void SplitPane::placeDivider()
{
    // JSplitPane.setDividerLocation(double): (int) ((length - divider size) * location) before
    // the divider.
    const int length = lengthOf(size()) - handleWidth();
    if (m_dividerPlaced || count() != 2 || length <= 0)
    {
        return;
    }
    m_dividerPlaced = true;
    const int first = static_cast<int>(static_cast<double>(length) * m_dividerLocation);
    setSizes({first, length - first});
}

}  // namespace QtRocket::Gui
