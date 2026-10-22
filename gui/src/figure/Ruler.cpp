#include "QtRocket/gui/figure/Ruler.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/figure/ScaleFigure.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/unit/Tick.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::Gui
{

namespace
{

/// The most minor distances a ruler's range may span: far more than any widget's pixels hold,
/// and few enough that the tick positions stay within Java's int arithmetic in Unit::getTicks().
constexpr double kMaxMinorSteps = 1e9;

[[nodiscard]] int clampToInt(long long value) noexcept
{
    return static_cast<int>(std::clamp<long long>(value, std::numeric_limits<int>::min(),
                                                  std::numeric_limits<int>::max()));
}

}  // namespace

Ruler::Ruler(Qt::Orientation orientation, ScaleFigure* figure, QWidget* parent)
  : QWidget(parent),
    m_orientation(orientation),
    m_figure(figure),
    m_unit(&unitGroup(UnitGroupId::LENGTH).getDefaultUnit())
{
    QTROCKET_ASSERT(figure != nullptr);
}

void Ruler::setUnit(const Unit& unit)
{
    m_unit = &unit;
    update();
}

std::pair<int, int> Ruler::visibleRange() const
{
    const QWidget* viewport   = m_figure->parentWidget();
    const bool     horizontal = m_orientation == Qt::Horizontal;
    if (viewport == nullptr)
    {
        return {0, horizontal ? m_figure->width() : m_figure->height()};
    }
    const int start = horizontal ? -m_figure->x() : -m_figure->y();
    return {start, start + (horizontal ? viewport->width() : viewport->height())};
}

std::vector<Ruler::Mark> Ruler::marks() const
{
    const auto [start, end] = visibleRange();
    return marks(m_orientation, *m_unit, m_figure->absoluteScale(), m_figure->subjectOrigin(),
                 start, end);
}

std::vector<Ruler::Mark> Ruler::marks(Qt::Orientation orientation, const Unit& unit, double scale,
                                      QPoint origin, int startPx, int endPx)
{
    if (!std::isfinite(scale) || scale <= 0)
    {
        return {};
    }
    const bool horizontal = orientation == Qt::Horizontal;
    const auto fromPx     = [&](int px) {
        const double real = horizontal ? static_cast<double>(px) - origin.x()
                                       : static_cast<double>(origin.y()) - px;
        return real / scale;
    };

    const double start = fromPx(startPx);
    const double end   = fromPx(endPx);
    const double minor = kMinorTicks / scale;
    const double major = kMajorTicks / scale;
    if (!std::isfinite(start) || !std::isfinite(end) || !std::isfinite(minor) ||
        !std::isfinite(major) || minor <= 0 || std::abs(start) / minor > kMaxMinorSteps ||
        std::abs(end) / minor > kMaxMinorSteps)
    {
        return {};
    }

    // The vertical ruler's values decrease downwards; getTicks() wants them increasing.
    const double            low   = horizontal ? start : end;
    const double            high  = horizontal ? end : start;
    const std::vector<Tick> ticks = unit.getTicks(low, high, minor, major);

    std::vector<Mark> marks;
    marks.reserve(ticks.size());
    for (const Tick& tick : ticks)
    {
        const long long px = MathUtil::javaIntCast((tick.value * scale) + 0.5);
        Mark            mark;
        mark.position = clampToInt(horizontal ? px + origin.x() : origin.y() - px);
        if (tick.major)
        {
            mark.length = kMajorLength;
            mark.label  = QtStrings::unitText(unit, tick.value);
            mark.bold   = tick.notable;
        }
        else
        {
            mark.length = tick.notable ? kNotableLength : kMinorLength;
        }
        marks.push_back(std::move(mark));
    }
    return marks;
}

QSize Ruler::sizeHint() const
{
    // Java's updateSize(): as long as the figure, plus 10.
    if (m_orientation == Qt::Horizontal)
    {
        return {m_figure->width() + 10, kSize};
    }
    return {kSize, m_figure->height() + 10};
}

void Ruler::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().window());
    painter.setPen(ThemeColors::color(ThemeColors::Key::TEXT));
    painter.setRenderHint(QPainter::TextAntialiasing);

    QFont regular = font();
    regular.setPixelSize(kLabelPixelSize);
    QFont bold = regular;
    bold.setBold(true);

    // The figure's pixel p is at p plus the figure's position in the viewport, which this ruler
    // lines up with.
    const QPoint offset     = m_figure->parentWidget() == nullptr ? QPoint() : m_figure->pos();
    const bool   horizontal = m_orientation == Qt::Horizontal;
    for (const Mark& mark : marks())
    {
        if (horizontal)
        {
            const int x = mark.position + offset.x();
            painter.drawLine(x, kSize - mark.length, x, kSize);
            if (!mark.label.isEmpty())
            {
                painter.setFont(mark.bold ? bold : regular);
                painter.drawText(x, kSize - mark.length - 1, mark.label);
            }
        }
        else
        {
            const int y = mark.position + offset.y();
            painter.drawLine(kSize - mark.length, y, kSize, y);
            if (!mark.label.isEmpty())
            {
                painter.setFont(mark.bold ? bold : regular);
                painter.drawText(1, y - 1, mark.label);
            }
        }
    }
}

}  // namespace QtRocket::Gui
