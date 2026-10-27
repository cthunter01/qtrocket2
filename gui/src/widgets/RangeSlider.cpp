#include "QtRocket/gui/widgets/RangeSlider.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <map>
#include <utility>

#include <QEvent>
#include <QFontMetrics>
#include <QHideEvent>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QSize>
#include <QSizePolicy>
#include <QSlider>
#include <QString>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>

#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The room QSlider adds to its thickness for the tick marks on one side (QSlider::sizeHint()).
constexpr int kTickSpace = 5;

/// The other thumb of a two-thumb slider: the neighbour on the left of @p thumb, or -1.
[[nodiscard]] constexpr int leftNeighbour(int thumb) noexcept
{
    return thumb - 1 >= 0 ? thumb - 1 : -1;
}

/// The neighbour on the right of @p thumb, or -1.
[[nodiscard]] constexpr int rightNeighbour(int thumb) noexcept
{
    return thumb + 1 < RangeSlider::kThumbCount ? thumb + 1 : -1;
}

/// Checks that @p thumb is a thumb of a range slider, 0 or 1.
void checkThumb(int thumb)
{
    if (thumb < 0 || thumb >= RangeSlider::kThumbCount)
    {
        bug("A range slider has the thumbs 0 and 1 only");
    }
}

/// @p thumb as an index of the thumbs' arrays (checked by the caller).
[[nodiscard]] std::size_t thumbIndex(int thumb) noexcept
{
    return static_cast<std::size_t>(thumb);
}

}  // namespace

RangeSlider::RangeSlider(int minimum, int maximum, int value0, int value1, QWidget* parent)
  : QWidget(parent), m_minimum(minimum), m_maximum(maximum), m_values{value0, value1}
{
    // Java's DefaultBoundedRangeModel refuses such a range ("invalid range properties").
    QTROCKET_ASSERT(minimum <= maximum);
    QTROCKET_ASSERT(minimum <= value0 && value0 <= maximum);
    QTROCKET_ASSERT(minimum <= value1 && value1 <= maximum);
    // A JSlider takes the focus (Tab and a press); its values grow to the right.
    setFocusPolicy(Qt::StrongFocus);
    setLayoutDirection(Qt::LeftToRight);
    setAttribute(Qt::WA_Hover);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // Swing's scroll timer waits its delays out: a precise timer never comes early (a coarse one
    // may by 5 %).
    m_scrollTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_scrollTimer, &QTimer::timeout, this, [this] { repeatScroll(); });
}

RangeSlider::~RangeSlider() = default;

int RangeSlider::valueAt(int thumb) const
{
    checkThumb(thumb);
    return m_values.at(thumbIndex(thumb));
}

void RangeSlider::setValueAt(int thumb, int value)
{
    checkThumb(thumb);
    // DefaultBoundedRangeModel.setValue(): into [minimum, maximum] (the extent is 0).
    const int clamped = std::max(m_minimum, std::min(value, m_maximum));
    if (m_values.at(thumbIndex(thumb)) == clamped)
    {
        return;
    }
    m_values.at(thumbIndex(thumb)) = clamped;
    update();
    const QPointer<RangeSlider> self(this);
    Q_EMIT valueChanged(thumb, clamped);
    if (!self.isNull())
    {
        Q_EMIT stateChanged();
    }
}

bool RangeSlider::isAdjusting(int thumb) const
{
    checkThumb(thumb);
    return m_adjusting.at(thumbIndex(thumb));
}

void RangeSlider::setAdjusting(int thumb, bool adjusting)
{
    if (m_adjusting.at(thumbIndex(thumb)) == adjusting)
    {
        return;
    }
    m_adjusting.at(thumbIndex(thumb)) = adjusting;
    Q_EMIT stateChanged();  // DefaultBoundedRangeModel.setValueIsAdjusting() fires a change
}

void RangeSlider::setTickInterval(int interval)
{
    m_tickInterval = std::max(0, interval);
    updateGeometry();
    update();
}

void RangeSlider::setLabels(std::map<int, QString> labels)
{
    for (const auto& [value, text] : labels)
    {
        if (value < m_minimum || value > m_maximum)
        {
            bug("A label of a range slider lies outside its range");
        }
    }
    m_labels = std::move(labels);
    updateGeometry();
    update();
}

void RangeSlider::initBaseOption(QStyleOptionSlider& option) const
{
    option.initFrom(this);
    option.direction         = Qt::LeftToRight;
    option.subControls       = QStyle::SC_None;
    option.activeSubControls = QStyle::SC_None;
    option.orientation       = Qt::Horizontal;
    option.minimum           = m_minimum;
    option.maximum           = m_maximum;
    option.tickPosition      = m_tickInterval > 0 ? QSlider::TicksBelow : QSlider::NoTicks;
    option.tickInterval      = m_tickInterval;
    option.upsideDown        = false;
    option.singleStep        = 1;
    option.pageStep          = std::max(1, blockIncrement());
    option.sliderPosition    = m_minimum;
    option.sliderValue       = m_minimum;
}

void RangeSlider::initStyleOption(QStyleOptionSlider& option, int position) const
{
    initBaseOption(option);
    option.rect           = sliderRect();
    option.sliderPosition = position;
    option.sliderValue    = position;
}

int RangeSlider::sliderHeight() const
{
    QStyleOptionSlider option;
    initBaseOption(option);
    // As QSlider::sizeHint(): the style's thickness, room for the tick marks, then the style's
    // own additions.
    int thickness = style()->pixelMetric(QStyle::PM_SliderThickness, &option, this);
    if (m_tickInterval > 0)
    {
        thickness += kTickSpace;
    }
    return style()
        ->sizeFromContents(QStyle::CT_Slider, &option, QSize(kPreferredLength, thickness), this)
        .height();
}

RangeSlider::BandRoom RangeSlider::bandRoom() const
{
    // The style's handles at both ends of a band of the preferred length (the room does not
    // depend on the length).
    QStyleOptionSlider option;
    initBaseOption(option);
    option.rect = QRect(0, 0, kPreferredLength, sliderHeight());
    const QRect atMinimum =
        style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);
    option.sliderPosition = m_maximum;
    option.sliderValue    = m_maximum;
    const QRect atMaximum =
        style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);
    return {.low      = atMinimum.center().x() - option.rect.left(),
            .high     = option.rect.right() - atMaximum.center().x(),
            .overhead = option.rect.width() - (atMaximum.center().x() - atMinimum.center().x())};
}

int RangeSlider::labelRowHeight() const
{
    return m_labels.empty() ? 0 : fontMetrics().height();
}

int RangeSlider::labelMargin() const
{
    // Swing's trackBuffer: half the widest of the outermost labels, beyond the room the style
    // leaves beside the centre of a thumb at either end.
    if (m_labels.empty())
    {
        return 0;
    }
    const QFontMetrics metrics     = fontMetrics();
    const int          lowWidth    = metrics.horizontalAdvance(m_labels.begin()->second);
    const int          highWidth   = metrics.horizontalAdvance(m_labels.rbegin()->second);
    const int          halfOutside = (std::max(lowWidth, highWidth) + 1) / 2;
    const BandRoom     room        = bandRoom();
    return std::max(0, halfOutside - std::min(room.low, room.high));
}

int RangeSlider::labelledSliderWidth() const
{
    if (m_labels.size() < 2 || m_maximum == m_minimum)
    {
        return 0;
    }
    // The distance between the centres of the thumbs at both ends at which the centres of two
    // neighbouring labels lie at least their half widths and a space apart, plus a pixel for the
    // style's rounding; then the band's width with it.
    const QFontMetrics metrics = fontMetrics();
    const int          gap     = metrics.horizontalAdvance(QStringLiteral(" "));
    const double       range   = static_cast<double>(m_maximum) - static_cast<double>(m_minimum);
    double             span    = 0;
    auto               left    = m_labels.begin();
    for (auto right = std::next(left); right != m_labels.end(); left = right, ++right)
    {
        const int widths =
            metrics.horizontalAdvance(left->second) + metrics.horizontalAdvance(right->second) + 1;
        const double need = (widths / 2.0) + gap + 1;
        span = std::max(span, need * range / static_cast<double>(right->first - left->first));
    }
    return static_cast<int>(std::ceil(span)) + bandRoom().overhead;
}

QRect RangeSlider::sliderRect() const
{
    const int margin     = labelMargin();
    const int labels     = labelRowHeight();
    const int bandHeight = std::max(0, std::min(sliderHeight(), height() - labels));
    // The slider and its labels in the middle of the widget's height, as Swing centres them.
    const int top = std::max(0, (height() - bandHeight - labels) / 2);
    return {margin, top, std::max(0, width() - (2 * margin)), bandHeight};
}

QRect RangeSlider::handleRectAt(int value) const
{
    QStyleOptionSlider option;
    initStyleOption(option, std::max(m_minimum, std::min(value, m_maximum)));
    return style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);
}

QRect RangeSlider::thumbRect(int thumb) const
{
    checkThumb(thumb);
    return handleRectAt(m_values.at(thumbIndex(thumb)));
}

QRect RangeSlider::labelRect(int value) const
{
    const auto found = m_labels.find(value);
    if (found == m_labels.end())
    {
        return {};
    }
    const QFontMetrics metrics = fontMetrics();
    const int          width   = metrics.horizontalAdvance(found->second);
    return {positionForValue(value) - (width / 2), sliderRect().bottom() + 1, width,
            metrics.height()};
}

int RangeSlider::positionForValue(int value) const
{
    return handleRectAt(value).center().x();
}

int RangeSlider::valueForHandleLeft(int left) const
{
    // As QSlider's pixelPosToRangeValue(): the handle's left edge over the span it travels.
    const int minLeft = handleRectAt(m_minimum).left();
    const int maxLeft = handleRectAt(m_maximum).left();
    return QStyle::sliderValueFromPosition(m_minimum, m_maximum, left - minLeft, maxLeft - minLeft);
}

int RangeSlider::valueForPosition(int x) const
{
    return valueForHandleLeft(x - ((handleRectAt(m_minimum).width() - 1) / 2));
}

QSize RangeSlider::sizeHint() const
{
    const int width = std::max(kPreferredLength, labelledSliderWidth()) + (2 * labelMargin());
    return {width, sliderHeight() + labelRowHeight()};
}

QSize RangeSlider::minimumSizeHint() const
{
    const int width = std::max(kMinimumLength, labelledSliderWidth()) + (2 * labelMargin());
    return {width, sliderHeight() + labelRowHeight()};
}

int RangeSlider::closestThumb(QPoint point) const
{
    // MultiSliderUI.findClosest(): the thumb ending nearest left of the mouse (a later one on a
    // tie) and the one starting nearest right of it (an earlier one on a tie); the left one when
    // it is as near. A thumb over the mouse's x is neither.
    int leftMin    = -std::numeric_limits<int>::max();
    int rightMin   = std::numeric_limits<int>::max();
    int leftThumb  = -1;
    int rightThumb = -1;
    for (int thumb = 0; thumb < kThumbCount; ++thumb)
    {
        const QRect rect    = thumbRect(thumb);
        const int   toStart = rect.left() - point.x();
        const int   toEnd   = rect.left() + rect.width() - point.x();
        if (toEnd <= 0 && toEnd >= leftMin)
        {
            leftMin   = toEnd;
            leftThumb = thumb;
        }
        if (toStart >= 0 && toStart < rightMin)
        {
            rightMin   = toStart;
            rightThumb = thumb;
        }
    }
    const int closest = std::abs(leftMin) <= std::abs(rightMin) ? leftThumb : rightThumb;
    return closest == -1 ? 0 : closest;
}

bool RangeSlider::shouldScroll(Direction direction) const
{
    // Probed: up, the scroll stops once the thumb's right end reaches the mouse; down, once its
    // left edge does.
    const QRect rect  = thumbRect(m_current);
    const int   value = m_values.at(thumbIndex(m_current));
    if (direction == Direction::POSITIVE)
    {
        return rect.left() + rect.width() < m_mouse.x() && value < m_maximum;
    }
    return rect.left() > m_mouse.x() && value > m_minimum;
}

void RangeSlider::scrollByUnit(Direction direction)
{
    const std::int64_t value = m_values.at(thumbIndex(m_current));
    setValueAt(m_current, static_cast<int>(std::clamp<std::int64_t>(
                              value + static_cast<std::int64_t>(direction), m_minimum, m_maximum)));
}

void RangeSlider::scrollByBlock(Direction direction)
{
    // MultiSliderUI.scrollByBlock(): a tenth of the maximum (not of the range).
    const std::int64_t value = m_values.at(thumbIndex(m_current));
    const std::int64_t delta =
        static_cast<std::int64_t>(blockIncrement()) * static_cast<std::int64_t>(direction);
    setValueAt(m_current,
               static_cast<int>(std::clamp<std::int64_t>(value + delta, m_minimum, m_maximum)));
}

void RangeSlider::pressThumbs(QPoint point, PressedThumbs pressed)
{
    // MultiTrackListener.mousePressed() of a bounded slider: the first thumb under the mouse
    // becomes current, the drag's offset is from the last one; nothing changes until a move.
    m_pressedThumbs = pressed;
    m_current       = pressed.first;
    m_dragging      = true;
    m_offset        = point.x() - thumbRect(pressed.last).left();
    update();
}

void RangeSlider::pressTrack(QPoint point)
{
    m_current  = closestThumb(point);
    m_dragging = false;
    const QPointer<RangeSlider> self(this);
    setAdjusting(m_current, true);
    if (self.isNull())
    {
        return;
    }
    const Direction direction =
        point.x() < thumbRect(m_current).left() ? Direction::NEGATIVE : Direction::POSITIVE;
    scrollByBlock(direction);  // BasicSliderUI.scrollDueToClickInTrack()
    if (self.isNull() || !m_pressed)
    {
        return;
    }
    if (!thumbRect(m_current).contains(point) && shouldScroll(direction))
    {
        m_scrollDirection = direction;
        m_scrollTimer.start(kScrollDelay);
    }
}

void RangeSlider::repeatScroll()
{
    if (m_scrollTimer.intervalAsDuration() != kScrollInterval)
    {
        m_scrollTimer.setInterval(kScrollInterval);
    }
    const QPointer<RangeSlider> self(this);
    scrollByBlock(m_scrollDirection);
    // Probed: the step comes first, then the check (a mouse moved behind the thumb stops it
    // after one more block).
    if (!self.isNull() && !shouldScroll(m_scrollDirection))
    {
        m_scrollTimer.stop();
    }
}

void RangeSlider::drag(QPoint point)
{
    if (!m_dragging)
    {
        return;
    }
    // The first move picks the thumb of those pressed: to the left (or none) the first, to the
    // right the last; then it keeps it.
    const int thumb =
        point.x() - m_pressPoint.x() <= 0 ? m_pressedThumbs.first : m_pressedThumbs.last;
    m_pressedThumbs = {.first = thumb, .last = thumb};
    m_current       = thumb;
    const QPointer<RangeSlider> self(this);
    setAdjusting(thumb, true);
    if (self.isNull() || !m_dragging)
    {
        return;
    }
    // The thumb's value stays between its neighbours' (setThumbBounds() keeps its pixel between
    // theirs: see the deviation in the header).
    const int left  = leftNeighbour(thumb);
    const int right = rightNeighbour(thumb);
    int       value = valueForHandleLeft(point.x() - m_offset);
    value           = std::max(value, left >= 0 ? m_values.at(thumbIndex(left)) : m_minimum);
    value           = std::min(value, right >= 0 ? m_values.at(thumbIndex(right)) : m_maximum);
    setValueAt(thumb, value);
}

void RangeSlider::endPress()
{
    m_pressed       = false;
    m_dragging      = false;
    m_offset        = 0;
    m_pressedThumbs = {};
    m_scrollTimer.stop();
    update();
    setAdjusting(m_current, false);
}

void RangeSlider::hover(QPoint point)
{
    int hovered = -1;
    for (int thumb = 0; thumb < kThumbCount; ++thumb)
    {
        if (thumbRect(thumb).contains(point))
        {
            hovered = thumb;
        }
    }
    if (hovered != m_hoverThumb)
    {
        m_hoverThumb = hovered;
        update();
    }
}

void RangeSlider::mousePressEvent(QMouseEvent* event)
{
    event->accept();
    if (m_pressed)
    {
        if (event->buttons().testFlag(m_pressButton))
        {
            return;  // another button while the first is held
        }
        endPress();  // the release of the first went elsewhere
    }
    // MultiTrackListener.mousePressed(), for any button.
    const QPoint point = event->position().toPoint();
    m_pressed          = true;
    m_pressButton      = event->button();
    m_pressPoint       = point;
    m_mouse            = point;
    setFocus(Qt::MouseFocusReason);
    PressedThumbs pressed;
    for (int thumb = 0; thumb < kThumbCount; ++thumb)
    {
        if (thumbRect(thumb).contains(point))
        {
            if (pressed.first == -1)
            {
                pressed.first = thumb;
            }
            pressed.last = thumb;
        }
    }
    if (pressed.first >= 0)
    {
        pressThumbs(point, pressed);
    }
    else
    {
        pressTrack(point);
    }
}

void RangeSlider::mouseMoveEvent(QMouseEvent* event)
{
    event->accept();
    if (!m_pressed)
    {
        return;
    }
    // MultiTrackListener.mouseDragged(): the mouse's place counts for a held press too.
    m_mouse = event->position().toPoint();
    drag(m_mouse);
}

void RangeSlider::mouseReleaseEvent(QMouseEvent* event)
{
    event->accept();
    if (m_pressed && event->button() == m_pressButton)
    {
        endPress();
    }
}

void RangeSlider::keyPressEvent(QKeyEvent* event)
{
    // The slider's key bindings (Swing's Slider.focusInputMap, probed): without modifiers.
    if ((event->modifiers() & ~Qt::KeypadModifier) != Qt::NoModifier)
    {
        QWidget::keyPressEvent(event);
        return;
    }
    switch (event->key())
    {
        case Qt::Key_Right:
        case Qt::Key_Up:
            scrollByUnit(Direction::POSITIVE);
            break;
        case Qt::Key_Left:
        case Qt::Key_Down:
            scrollByUnit(Direction::NEGATIVE);
            break;
        case Qt::Key_PageUp:
            scrollByBlock(Direction::POSITIVE);
            break;
        case Qt::Key_PageDown:
            scrollByBlock(Direction::NEGATIVE);
            break;
        case Qt::Key_Home:
            setValueAt(m_current, m_minimum);
            break;
        case Qt::Key_End:
            setValueAt(m_current, m_maximum);
            break;
        default:
            QWidget::keyPressEvent(event);
            return;
    }
    event->accept();
}

void RangeSlider::wheelEvent(QWheelEvent* event)
{
    event->ignore();  // on to the widgets around it: a scroll area scrolls
}

void RangeSlider::changeEvent(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::EnabledChange:
            if (!isEnabled() && m_pressed)
            {
                endPress();
            }
            break;
        case QEvent::FontChange:
        case QEvent::StyleChange:
            updateGeometry();
            break;
        default:
            break;
    }
    QWidget::changeEvent(event);
}

void RangeSlider::hideEvent(QHideEvent* event)
{
    if (m_pressed)
    {
        endPress();
    }
    QWidget::hideEvent(event);
}

bool RangeSlider::event(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::HoverEnter:
        case QEvent::HoverMove:
            if (const auto* const hoverEvent = dynamic_cast<const QHoverEvent*>(event))
            {
                hover(hoverEvent->position().toPoint());
            }
            break;
        case QEvent::HoverLeave:
            if (m_hoverThumb != -1)
            {
                m_hoverThumb = -1;
                update();
            }
            break;
        default:
            break;
    }
    return QWidget::event(event);
}

void RangeSlider::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    // The groove (with no part of it highlighted: a handle at the minimum) and the tick marks.
    QStyleOptionSlider groove;
    initStyleOption(groove, m_minimum);
    groove.subControls = QStyle::SC_SliderGroove;
    if (m_tickInterval > 0)
    {
        groove.subControls |= QStyle::SC_SliderTickmarks;
    }
    style()->drawComplexControl(QStyle::CC_Slider, &groove, &painter, this);
    // The thumbs, the current one last: on top, and the one that shows the focus.
    const int other = m_current == 0 ? 1 : 0;
    for (const int thumb : {other, m_current})
    {
        QStyleOptionSlider handle;
        initStyleOption(handle, m_values.at(thumbIndex(thumb)));
        handle.subControls = QStyle::SC_SliderHandle;
        if (thumb != m_current)
        {
            handle.state &= ~QStyle::State_HasFocus;
        }
        if (m_dragging && thumb == m_current)
        {
            handle.activeSubControls = QStyle::SC_SliderHandle;
            handle.state |= QStyle::State_Sunken;
        }
        else if (!m_pressed && thumb == m_hoverThumb)
        {
            handle.activeSubControls = QStyle::SC_SliderHandle;
            handle.state |= QStyle::State_MouseOver;
        }
        else
        {
            handle.state &= ~QStyle::State_MouseOver;
        }
        style()->drawComplexControl(QStyle::CC_Slider, &handle, &painter, this);
    }
    for (const auto& [value, text] : m_labels)
    {
        style()->drawItemText(&painter, labelRect(value), Qt::AlignCenter, palette(), isEnabled(),
                              text, QPalette::WindowText);
    }
}

}  // namespace QtRocket::Gui
