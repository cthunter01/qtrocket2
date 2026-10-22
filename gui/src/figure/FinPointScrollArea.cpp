#include "QtRocket/gui/figure/FinPointScrollArea.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <utility>
#include <vector>

#include <QCoreApplication>
#include <QEvent>
#include <QFocusEvent>
#include <QFont>
#include <QHideEvent>
#include <QMouseEvent>
#include <QPoint>
#include <QRect>
#include <QScrollBar>
#include <QString>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/figure/FinPointEditHistory.h"
#include "QtRocket/gui/figure/FinPointFigure.h"
#include "QtRocket/gui/figure/FinPointSelection.h"
#include "QtRocket/gui/figure/Ruler.h"
#include "QtRocket/gui/figure/ScaleFigure.h"
#include "QtRocket/gui/figure/ScaleScrollArea.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

namespace
{

/// Java's Math.signum(): the sign as 1 or -1, the argument itself for zeros and NaN.
[[nodiscard]] double javaSignum(double value) noexcept
{
    if (value > 0)
    {
        return 1.0;
    }
    if (value < 0)
    {
        return -1.0;
    }
    return value;
}

/// @p result of an edit Java makes and turns into a RuntimeException when it fails (an index the
/// area chose itself): a programming error here.
void expectSuccess(const Result<void>& result)
{
    if (!result.has_value())
    {
        bug("A fin point edit of the figure failed: " + result.error().message);
    }
}

}  // namespace

FinPointScrollArea::FinPointScrollArea(FinPointFigure* figure, const Preferences& preferences,
                                       QWidget* parent)
  : ScaleScrollArea(figure, parent),
    m_figure(figure),
    m_preferences(&preferences),
    m_rulerUnit(std::make_shared<DoubleBinding>(0.0, unitGroup(UnitGroupId::LENGTH)))
{
    setObjectName(QStringLiteral("finPointScrollArea"));
    // The press takes the focus; Tab does not reach the area (Java's focus order leaves it out).
    setFocusPolicy(Qt::ClickFocus);
    // Java's ScaleScrollPane pans with whatever button its subclass passes on.
    setPanButtons(Qt::LeftButton | Qt::MiddleButton | Qt::RightButton);

    // The rulers' unit (Java: a UnitSelector of a DoubleModel(0.0, UNITS_LENGTH), font size 8).
    auto* selector = makeChild<UnitSelector>(this, m_rulerUnit);
    selector->setObjectName(QStringLiteral("rulerUnit"));
    QFont font = selector->font();
    font.setPixelSize(kCornerFontPixelSize);
    selector->setFont(font);
    setRulerCornerWidget(selector);
    m_rulerUnitChanged = Signal<>::ScopedConnection(
        m_rulerUnit->addChangeListener([this] { setRulerUnit(m_rulerUnit->getCurrentUnit()); }));
}

FinPointScrollArea::~FinPointScrollArea()
{
    // Never leave the rocket frozen (see the class comment).
    static_cast<void>(Guard::run([this] { thawForDrag(); }, objectName()));
}

void FinPointScrollArea::endDrag()
{
    if (isDragging())
    {
        finishPress();
    }
}

void FinPointScrollArea::finishPress()
{
    // Thaw rocket to fire batched component change events (its listeners still see the drag)
    thawForDrag();
    m_dragIndex  = -1;
    m_dragTarget = nullptr;
    m_figure->setHighlightIndex(-1);
    m_figure->updateFigure();
    commitFinPointEdit();
}

void FinPointScrollArea::revalidateFigure()
{
    m_figure->updateFigure();
    // Lay the figure out now: QScrollArea resizes its widget on a layout request.
    QEvent request(QEvent::LayoutRequest);
    QCoreApplication::sendEvent(this, &request);
    horizontalRuler()->update();
    verticalRuler()->update();
}

int FinPointScrollArea::lockIndex(Qt::KeyboardModifiers modifiers) const
{
    const FreeformFinSet* fins = m_figure->target().main();
    if (fins == nullptr || m_dragIndex < 0)
    {
        return -1;
    }
    const auto length = static_cast<int>(fins->getFinPoints().size());
    if (m_dragIndex == 0 || m_dragIndex >= length - 1)
    {
        return -1;
    }
    return modifiers.testFlag(Qt::ControlModifier) ? m_dragIndex + 1 : m_dragIndex - 1;
}

int FinPointScrollArea::highlightIndexFor(int lockIndex) const noexcept
{
    return lockIndex == m_dragIndex + 1 ? m_dragIndex : lockIndex;
}

Point2D FinPointScrollArea::snapPoint(Point2D point, Point2D lockPoint) noexcept
{
    Point2D snappedPoint = point;

    const double diffX     = point.x - lockPoint.x;
    const double diffY     = point.y - lockPoint.y;
    const double distanceX = std::abs(diffX);
    const double distanceY = std::abs(diffY);

    // Calculate distance to 45 or 135 degree line
    const double a            = 1;
    const double b            = javaSignum(diffX) == javaSignum(diffY) ? -1 : 1;
    const double c            = -((a * lockPoint.x) + (b * lockPoint.y));
    const double distanceDiag = std::abs(((a * point.x) + (b * point.y)) + c) / std::numbers::sqrt2;

    // Snap to the closest constraint
    if (distanceX <= distanceY && distanceX <= distanceDiag)
    {
        // Snap horizontal
        snappedPoint.x = lockPoint.x;
    }
    else if (distanceY <= distanceX && distanceY <= distanceDiag)
    {
        // Snap vertical
        snappedPoint.y = lockPoint.y;
    }
    else
    {
        // Snap diagonal (45 degrees)
        const double avgDist = (std::abs(diffX) + std::abs(diffY)) / 2;
        snappedPoint.x       = lockPoint.x + (javaSignum(diffX) * avgDist);
        snappedPoint.y       = lockPoint.y + (javaSignum(diffY) * avgDist);
    }
    return snappedPoint;
}

template <class F>
void FinPointScrollArea::guardedEvent(const F& handle)
{
    if (!Guard::run(handle, objectName()))
    {
        // Never leave the rocket frozen after a failure.
        static_cast<void>(Guard::run([this] { endDrag(); }, objectName()));
    }
}

void FinPointScrollArea::mousePressEvent(QMouseEvent* event)
{
    guardedEvent([this, event] { press(*event); });
}

void FinPointScrollArea::mouseDoubleClickEvent(QMouseEvent* event)
{
    // Qt's second press of a double click (QAbstractScrollArea ignores it): a press as any.
    guardedEvent([this, event] { press(*event); });
}

void FinPointScrollArea::mouseMoveEvent(QMouseEvent* event)
{
    guardedEvent([this, event] { move(*event); });
}

void FinPointScrollArea::mouseReleaseEvent(QMouseEvent* event)
{
    guardedEvent([this, event] { release(*event); });
}

void FinPointScrollArea::press(QMouseEvent& event)
{
    setFocus(Qt::MouseFocusReason);
    const QPoint position = event.position().toPoint();
    m_pressButton         = event.button();
    m_pressPosition       = position;
    m_moved               = false;
    if (isDragging())
    {
        event.accept();  // a second button during a drag (see the class comment)
        return;
    }

    const QPoint pixel      = figurePixel(position);
    const int    pressIndex = m_figure->indexAt(pixel);
    if (pressIndex >= 0)
    {
        beginFinPointEdit();
        beginDrag(pressIndex, position);
        Q_EMIT refreshRequested();
        event.accept();
        return;
    }

    const int segmentIndex = m_figure->segmentAt(pixel);
    if (segmentIndex >= 0)
    {
        beginFinPointEdit();
        const Point2D point = m_figure->toFinPoint(pixel);
        expectSuccess(m_figure->target().addPoint(static_cast<std::size_t>(segmentIndex), point));
        beginDrag(segmentIndex, position);
        Q_EMIT refreshRequested();
        event.accept();
        return;
    }

    // Deselect point if clicking on the figure but not on a point or segment
    m_figure->resetSelectedIndex();
    Q_EMIT pointSelectionCleared();
    m_figure->updateFigure();

    ScaleScrollArea::mousePressEvent(&event);
}

void FinPointScrollArea::move(QMouseEvent& event)
{
    const QPoint position = event.position().toPoint();
    if (position != m_pressPosition)
    {
        m_moved = true;
    }
    if (!isDragging())
    {
        ScaleScrollArea::mouseMoveEvent(&event);
        return;
    }
    event.accept();
    if (isDragTargetGone())
    {
        endDrag();
        return;
    }
    const FreeformFinSet& fins  = *m_figure->target().main();
    Point2D               point = m_figure->toFinPoint(figurePixel(position));

    // If shift is held down, apply snapping
    const Qt::KeyboardModifiers modifiers = event.modifiers();
    if (modifiers.testFlag(Qt::ShiftModifier))
    {
        const int lock = lockIndex(modifiers);
        if (lock != -1)
        {
            const Coordinate lockPoint = fins.getFinPoints().at(static_cast<std::size_t>(lock));
            point                      = snapPoint(point, {lockPoint.x, lockPoint.y});
            m_figure->setHighlightIndex(highlightIndexFor(lock));
        }
        else
        {
            // No valid lock index, reset highlight
            m_figure->setHighlightIndex(-1);
        }
    }
    else
    {
        // Shift not held down, reset highlight
        m_figure->setHighlightIndex(-1);
    }

    // The rocket is frozen during dragging (unless the live update is on): setPoint() queues its
    // events. The core refuses an illegal move silently, so the figure is painted again whatever
    // happened.
    expectSuccess(
        m_figure->target().setPoint(static_cast<std::size_t>(m_dragIndex), point.x, point.y));

    m_dragPoint = position;

    // During dragging, just repaint the figure (layout doesn't change, only point positions)
    m_figure->update();
    Q_EMIT refreshRequested();

    // Handle scrolling if point is dragged out of view
    if (isDragging())
    {
        handleScrolling();
    }
}

void FinPointScrollArea::release(QMouseEvent& event)
{
    const QPoint position = event.position().toPoint();
    const bool   click = !m_moved && event.button() == m_pressButton && position == m_pressPosition;
    m_pressButton      = Qt::NoButton;

    // Java's finally block, for every release, with or without a drag.
    finishPress();
    ScaleScrollArea::mouseReleaseEvent(&event);
    event.accept();

    if (click)
    {
        this->click(event.button(), event.modifiers(), position, event.globalPosition().toPoint());
    }
}

void FinPointScrollArea::focusInEvent(QFocusEvent* event)
{
    ScaleScrollArea::focusInEvent(event);
    // A menu that closes gives the focus back; Swing's lightweight popups never take it.
    if (event->reason() != Qt::PopupFocusReason)
    {
        Q_EMIT focusEntered();
    }
}

void FinPointScrollArea::hideEvent(QHideEvent* event)
{
    static_cast<void>(Guard::run([this] { endDrag(); }, objectName()));
    ScaleScrollArea::hideEvent(event);
}

QPoint FinPointScrollArea::figurePixel(QPoint position) const
{
    return position + viewRect().topLeft();
}

void FinPointScrollArea::beginDrag(int index, QPoint position)
{
    m_dragIndex  = index;
    m_dragPoint  = position;
    m_dragTarget = m_figure->target().main();

    // Freeze rocket to batch component change events during dragging (unless preference is
    // enabled)
    if (!m_preferences->getBoolean(GuiPreferenceKeys::kUpdateRocketWhileDraggingPoint,
                                   GuiPreferenceKeys::kUpdateRocketWhileDraggingPointDefault))
    {
        Rocket& rocket = m_figure->target().rocket();
        if (!rocket.isFrozen())
        {
            rocket.freeze();
            m_frozenRocket = &rocket;
        }
    }
}

bool FinPointScrollArea::isDragTargetGone() const
{
    const FreeformFinSet* fins = m_figure->target().main();
    return fins == nullptr || static_cast<const void*>(fins) != m_dragTarget;
}

void FinPointScrollArea::thawForDrag()
{
    if (Rocket* rocket = std::exchange(m_frozenRocket, nullptr);
        rocket != nullptr && rocket->isFrozen())
    {
        rocket->thaw();
    }
}

void FinPointScrollArea::click(Qt::MouseButton button, Qt::KeyboardModifiers modifiers,
                               QPoint position, QPoint globalPosition)
{
    const int clickIndex = m_figure->indexAt(figurePixel(position));
    if (button == Qt::LeftButton && modifiers.testFlag(Qt::ControlModifier))
    {
        if (0 < clickIndex)
        {
            // if ctrl+click, delete point (Java ignores and logs a refusal)
            beginFinPointEdit();
            try
            {
                [[maybe_unused]] const Result<void> refused =
                    m_figure->target().removePoint(static_cast<std::size_t>(clickIndex));
            }
            catch (...)
            {
                commitFinPointEdit();
                throw;
            }
            commitFinPointEdit();
        }
        return;
    }
    if (button == Qt::RightButton && clickIndex >= 0)
    {
        // Right-click: show context menu if clicking on a fin point
        Q_EMIT pointMenuRequested(clickIndex, globalPosition);
    }
}

void FinPointScrollArea::handleScrolling()
{
    QRect        dragRectangle = viewRect();
    const QPoint canvasPoint   = m_dragPoint + dragRectangle.topLeft();
    const int    borderWidth   = ScaleFigure::kBorderWidth;
    const int    borderHeight  = ScaleFigure::kBorderHeight;

    // isPointWithinFigureBounds()
    const bool withinFigure =
        borderWidth < canvasPoint.x() && canvasPoint.x() < (m_figure->width() - borderWidth) &&
        borderHeight < canvasPoint.y() && canvasPoint.y() < (m_figure->height() - borderHeight);
    if (!withinFigure)
    {
        return;
    }
    // updateScrollPosition()
    bool hitBorder = false;
    if (m_dragPoint.x() < borderWidth)
    {
        hitBorder = true;
        dragRectangle.translate(m_dragPoint.x() - borderWidth, 0);
    }
    else if (m_dragPoint.x() > (dragRectangle.width() - borderWidth))
    {
        hitBorder = true;
        dragRectangle.translate(m_dragPoint.x() - (dragRectangle.width() - borderWidth), 0);
    }
    if (m_dragPoint.y() < borderHeight)
    {
        hitBorder = true;
        dragRectangle.translate(0, m_dragPoint.y() - borderHeight);
    }
    else if (m_dragPoint.y() > (dragRectangle.height() - borderHeight))
    {
        hitBorder = true;
        dragRectangle.translate(0, m_dragPoint.y() - (dragRectangle.height() - borderHeight));
    }
    if (hitBorder)
    {
        setFitting(false);
        Q_EMIT autoScrolled();
        horizontalScrollBar()->setValue(dragRectangle.x());
        verticalScrollBar()->setValue(dragRectangle.y());
        revalidateFigure();
    }
}

void FinPointScrollArea::beginFinPointEdit()
{
    m_figure->editHistory().beginEdit();
    Q_EMIT editStateChanged();
}

void FinPointScrollArea::commitFinPointEdit()
{
    m_figure->editHistory().commitEdit();
    Q_EMIT editStateChanged();
}

}  // namespace QtRocket::Gui
