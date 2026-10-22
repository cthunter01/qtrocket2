#include "QtRocket/gui/figure/FinPointFigure.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include <QColor>
#include <QLineF>
#include <QObject>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QTransform>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/figure/FinPointEditHistory.h"
#include "QtRocket/gui/figure/FinPointSelection.h"
#include "QtRocket/gui/figure/ScaleFigure.h"
#include "QtRocket/gui/figure/ScaleScrollArea.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/unit/Tick.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::Gui
{

namespace
{

/// The most minor grid steps painted across the visible range (Unit::getTicks() works in Java's
/// int arithmetic; the ruler's guard).
constexpr double kMaxGridSteps = 1e9;

[[nodiscard]] int clampToInt(long long value) noexcept
{
    return static_cast<int>(std::clamp<long long>(value, std::numeric_limits<int>::min(),
                                                  std::numeric_limits<int>::max()));
}

/// Java's (int) narrowing, widened for the int sums Java computes (which wrap; here they
/// saturate in clampToInt()).
[[nodiscard]] long long javaInt(double value) noexcept
{
    return MathUtil::javaIntCast(value);
}

/// A cosmetic pen of @p width pixels (Java strokes in metres with width / scale).
[[nodiscard]] QPen pixelPen(const QColor& color, int width, Qt::PenCapStyle cap = Qt::FlatCap,
                            Qt::PenJoinStyle join = Qt::BevelJoin)
{
    QPen pen(color, width, Qt::SolidLine, cap, join);
    pen.setCosmetic(true);
    return pen;
}

}  // namespace

FinPointFigure::FinPointFigure(FinPointSelection target, double dotsPerInch, QWidget* parent)
  : ScaleFigure(dotsPerInch, parent), m_target(std::move(target)), m_history(m_target)
{
    updateFigure();
}

FinPointFigure::FinPointFigure(FinPointSelection target, const Preferences& preferences,
                               QWidget* parent)
  : FinPointFigure(std::move(target), ScaleFigure::dotsPerInch(preferences), parent)
{
}

QPoint FinPointFigure::autoZoomPoint() const
{
    return {std::max(0, subjectOrigin().x() - kBorderWidth), 0};
}

Point2D FinPointFigure::toFinFrame(QPointF pixel) const
{
    // Java's AffineTransform.inverseTransform() of translate(origin) * scale(s, -s).
    const QTransform& p = projection();
    return {(pixel.x() - p.dx()) / p.m11(), (pixel.y() - p.dy()) / p.m22()};
}

Point2D FinPointFigure::toFinPoint(QPoint pixel) const
{
    const Point2D p = toFinFrame(QPointF(pixel));
    return {
        MathUtil::clamp(p.x, -FreeformFinSet::kSnapLargerThan, FreeformFinSet::kSnapLargerThan),
        MathUtil::clamp(p.y, -FreeformFinSet::kSnapLargerThan, FreeformFinSet::kSnapLargerThan)};
}

int FinPointFigure::indexAt(QPoint pixel) const
{
    const FreeformFinSet* fins = m_target.main();
    if (fins == nullptr)
    {
        return -1;
    }
    const Point2D p = toFinFrame(QPointF(pixel));

    // Use a larger hit detection radius for easier point selection
    const double hitDetectionRadius        = (kBoxWidth * kPointHitMultiplier) / absoluteScale();
    const double hitDetectionRadiusSquared = hitDetectionRadius * hitDetectionRadius;

    const std::vector<Coordinate> points = fins->getFinPoints();
    // Find the closest point within the threshold (using squared distance to avoid sqrt)
    int    closestIndex           = -1;
    double closestDistanceSquared = std::numeric_limits<double>::max();
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const double dx              = p.x - points[i].x;
        const double dy              = p.y - points[i].y;
        const double distanceSquared = (dx * dx) + (dy * dy);
        if (distanceSquared <= hitDetectionRadiusSquared &&
            distanceSquared < closestDistanceSquared)
        {
            closestIndex           = static_cast<int>(i);
            closestDistanceSquared = distanceSquared;
        }
    }
    return closestIndex;
}

int FinPointFigure::segmentAt(QPoint pixel) const
{
    const FreeformFinSet* fins = m_target.main();
    if (fins == nullptr)
    {
        return -1;
    }
    const Point2D p         = toFinFrame(QPointF(pixel));
    const double  threshold = kBoxWidth / absoluteScale();

    const std::vector<Coordinate> points = fins->getFinPoints();
    for (std::size_t i = 1; i < points.size(); ++i)
    {
        const double x1 = points[i - 1].x;
        const double y1 = points[i - 1].y;
        const double x2 = points[i].x;
        const double y2 = points[i].y;

        const double segmentLength = MathUtil::hypot(x2 - x1, y2 - y1);

        // Distance to an infinite line, defined by two points
        const double x0 = p.x;
        const double y0 = p.y;
        const double distanceToLine =
            std::abs(((((y2 - y1) * x0) - ((x2 - x1) * y0)) + (x2 * y1)) - (y2 * x1)) /
            segmentLength;

        const double distanceToStart = MathUtil::hypot(x1 - x0, y1 - y0);
        const double distanceToEnd   = MathUtil::hypot(x2 - x0, y2 - y0);
        const bool withinSegment = distanceToStart < segmentLength && distanceToEnd < segmentLength;

        if (distanceToLine < threshold && withinSegment)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void FinPointFigure::setSelectedIndex(int index)
{
    m_selectedIndex = index;
}

void FinPointFigure::setHighlightIndex(int index)
{
    m_highlightIndex = index;
}

const Unit& FinPointFigure::gridUnit() const
{
    const QWidget* viewport = parentWidget();
    if (const auto* area = qobject_cast<const ScaleScrollArea*>(
            viewport != nullptr ? viewport->parentWidget() : nullptr))
    {
        return area->rulerUnit();
    }
    return unitGroup(UnitGroupId::LENGTH).getDefaultUnit();
}

void FinPointFigure::updateSubjectDimensions()
{
    const FreeformFinSet* fins = m_target.main();
    if (fins == nullptr)
    {
        return;  // the figure keeps what it showed
    }
    // update subject (i.e. Fin) bounds
    m_finBounds               = BoundingBox().update(fins->getFinPoints());
    const Rectangle2D subject = m_finBounds.toRectangle();

    // update to bound the parent body:
    const auto* parent = dynamic_cast<const SymmetricComponent*>(fins->getParent());
    m_hasMount         = parent != nullptr;
    if (parent == nullptr)
    {
        m_mountBoundsMax = BoundingBox();
        m_mountBoundsMin = BoundingBox();
        setSubjectBounds(subject, subject);
        return;
    }
    const double xFinFront   = fins->getAxialOffset(AxialMethod::TOP);
    const double xParent     = -xFinFront;
    const double yParent     = -parent->getRadius(xParent);  // from fin-front to parent centerline
    const double rParentMax  = MathUtil::javaMax(parent->getForeRadius(), parent->getAftRadius());
    const double rParentMin  = MathUtil::javaMin(parent->getForeRadius(), parent->getAftRadius());
    const double xParentBack = xParent + parent->getLength();

    m_mountBoundsMax = BoundingBox()
                           .update(Coordinate(xParent, yParent, 0))
                           .update(Coordinate(xParentBack, yParent + rParentMax));
    m_mountBoundsMin = BoundingBox()
                           .update(Coordinate(xParent, yParent, 0))
                           .update(Coordinate(xParentBack, yParent + rParentMin));

    const BoundingBox combinedBounds = BoundingBox().update(m_finBounds).update(m_mountBoundsMax);
    setSubjectBounds(subject, combinedBounds.toRectangle());
}

void FinPointFigure::updateCanvasOrigin()
{
    if (m_finBounds.isEmpty())
    {
        return;  // no fin set was ever shown
    }
    const double      scale       = absoluteScale();
    const long long   finBottomPy = javaInt(m_finBounds.max().y * scale);
    const long long   mountHeight = m_hasMount ? javaInt(m_mountBoundsMin.span().y * scale) : 0;
    const Rectangle2D content     = contentBounds();
    // this is non-intuitive: it's an offset _from_ the origin(0,0) _to_ the lower-left of the
    // content -- because the canvas is drawn from that lower-left corner of the content, and the
    // fin-front is fixed-- by definition-- to the origin.
    const long long finFrontPx    = -javaInt(content.x * scale);  // left border to fin-front
    const long long contentHeight = javaInt(content.height * scale);

    const long long originX = kBorderWidth + finFrontPx;
    long long       originY = 0;
    if (visibleBounds().height() > (contentHeight + (2LL * kBorderHeight)))
    {
        originY = height() - mountHeight - kBorderHeight;
    }
    else
    {
        originY = kBorderHeight + finBottomPy;
    }
    setSubjectOrigin({clampToInt(originX), clampToInt(originY)});
}

void FinPointFigure::paintEvent(QPaintEvent* event)
{
    m_paintRect = event->rect();
    ScaleFigure::paintEvent(event);
}

void FinPointFigure::paintFigure(QPainter& painter)
{
    const FreeformFinSet* fins  = m_target.main();
    const double          scale = absoluteScale();
    if (fins == nullptr || !std::isfinite(scale) || scale <= 0)
    {
        return;
    }
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setWorldTransform(projection(), true);

    paintBackgroundGrid(painter);
    paintRocketBody(painter, *fins);
    paintFinShape(painter, *fins);
    paintHighlight(painter, *fins);
    paintFinHandles(painter, *fins);
    painter.restore();
}

void FinPointFigure::paintBackgroundGrid(QPainter& painter) const
{
    // Java: the clip's bounds in the fin frame (Graphics2D.getClipBounds(), a Rectangle of whole
    // metres around it) grown by 3 and 4 m.
    const QRect   area   = m_paintRect.isValid() ? m_paintRect : rect();
    const Point2D corner = toFinFrame(QPointF(area.topLeft()));
    const Point2D other =
        toFinFrame(QPointF(area.left() + area.width(), area.top() + area.height()));
    const double x0 = std::floor(std::min(corner.x, other.x)) - 3;
    const double x1 = std::ceil(std::max(corner.x, other.x)) + 4;
    const double y0 = std::floor(std::min(corner.y, other.y)) - 3;
    const double y1 = std::ceil(std::max(corner.y, other.y)) + 4;

    const double scale = absoluteScale();
    const double minor = kMinorTicks / scale;
    const double major = kMajorTicks / scale;
    if (!std::isfinite(x0) || !std::isfinite(x1) || !std::isfinite(y0) || !std::isfinite(y1) ||
        !std::isfinite(minor) || minor <= 0 ||
        std::max({std::abs(x0), std::abs(x1), std::abs(y0), std::abs(y1)}) / minor > kMaxGridSteps)
    {
        return;
    }
    const Unit& unit = gridUnit();
    const QPen  majorPen =
        pixelPen(ThemeColors::color(ThemeColors::Key::FIN_GRID_MAJOR), kGridLineWidth);
    const QPen minorPen =
        pixelPen(ThemeColors::color(ThemeColors::Key::FIN_GRID_MINOR), kGridLineWidth);

    // vertical
    for (const Tick& tick : unit.getTicks(x0, x1, minor, major))
    {
        painter.setPen(tick.major ? majorPen : minorPen);
        painter.drawLine(QLineF(tick.value, y0, tick.value, y1));
    }
    // horizontal
    for (const Tick& tick : unit.getTicks(y0, y1, minor, major))
    {
        painter.setPen(tick.major ? majorPen : minorPen);
        painter.drawLine(QLineF(x0, tick.value, x1, tick.value));
    }
}

void FinPointFigure::paintRocketBody(QPainter& painter, const FreeformFinSet& fins) const
{
    if (!m_hasMount)
    {
        return;
    }
    if (dynamic_cast<const Transition*>(fins.getParent()) != nullptr)
    {
        paintBodyTransition(painter, fins);
    }
    else
    {
        paintBodyTube(painter);
    }
}

void FinPointFigure::paintBodyTransition(QPainter& painter, const FreeformFinSet& fins)
{
    // Relative to the reference point of the BODY component: 0,0 == the front, foreRadius of the
    // body component.
    const auto& body = dynamic_cast<const Transition&>(*fins.getParent());

    const double xFinStart = fins.getAxialOffset(AxialMethod::TOP);  //<<  in body frame

    // vv in fin-frame == draw-frame vv
    const double xOffset = -xFinStart;
    const double yOffset = -body.getRadius(xFinStart);
    // Java's float length and step, compared and added as doubles
    const auto length   = static_cast<double>(static_cast<float>(body.getLength()));
    const auto stepSize = static_cast<double>(kBodyResolution);

    QPainterPath bodyShape;
    // draw front-cap:
    bodyShape.moveTo(xOffset, yOffset);
    bodyShape.lineTo(xOffset, yOffset + body.getForeRadius());

    // draw edge (xBody is the distance from the front of the parent body)
    double xBody = stepSize;
    while (xBody < length)
    {
        bodyShape.lineTo(xOffset + xBody, yOffset + body.getRadius(xBody));
        xBody += stepSize;
    }

    // draw end-cap
    bodyShape.lineTo(xOffset + length, yOffset + body.getAftRadius());
    bodyShape.lineTo(xOffset + length, yOffset);

    painter.setPen(pixelPen(ThemeColors::color(ThemeColors::Key::FIN_BODY_LINE), kBodyLineWidth));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(bodyShape);
}

void FinPointFigure::paintBodyTube(QPainter& painter) const
{
    // in-figure left extent, right extent and the axis
    const double xFore   = m_mountBoundsMax.min().x;
    const double xAft    = m_mountBoundsMax.max().x;
    const double yCenter = m_mountBoundsMax.min().y;

    QPainterPath shape;
    shape.moveTo(xFore, yCenter);
    shape.lineTo(xFore, 0);       // body tube fore edge
    shape.lineTo(xAft, 0);        // body tube side
    shape.lineTo(xAft, yCenter);  // body tube aft edge

    painter.setPen(pixelPen(ThemeColors::color(ThemeColors::Key::FIN_BODY_LINE), kBodyLineWidth));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(shape);
}

void FinPointFigure::paintFinShape(QPainter& painter, const FreeformFinSet& fins)
{
    // excludes fin tab points
    const std::vector<Coordinate> drawPoints = fins.getFinPointsWithRoot();
    if (drawPoints.empty())
    {
        return;
    }
    QPainterPath shape;
    shape.moveTo(drawPoints.front().x, drawPoints.front().y);
    for (std::size_t i = 1; i < drawPoints.size(); ++i)
    {
        shape.lineTo(drawPoints[i].x, drawPoints[i].y);
    }
    painter.setPen(pixelPen(ThemeColors::color(ThemeColors::Key::FIN_BODY_LINE), kFinLineWidth));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(shape);
}

void FinPointFigure::paintHighlight(QPainter& painter, const FreeformFinSet& fins) const
{
    const std::vector<Coordinate> points = fins.getFinPointsWithRoot();
    // Java reads points[index + 1] for the last index too (an exception it never reaches).
    if (m_highlightIndex < 0 || static_cast<std::size_t>(m_highlightIndex) + 1 >= points.size())
    {
        return;
    }
    const Coordinate& start = points[static_cast<std::size_t>(m_highlightIndex)];
    const Coordinate& end   = points[static_cast<std::size_t>(m_highlightIndex) + 1];

    // Java: BasicStroke(width), square caps and mitre joins.
    painter.setPen(pixelPen(ThemeColors::color(ThemeColors::Key::FIN_SNAP_HIGHLIGHT),
                            kHighlightLineWidth, Qt::SquareCap, Qt::MiterJoin));
    painter.drawLine(QLineF(start.x, start.y, end.x, end.y));
}

void FinPointFigure::paintFinHandles(QPainter& painter, const FreeformFinSet& fins) const
{
    // Excludes fin tab points
    const std::vector<Coordinate> drawPoints = fins.getFinPoints();

    // Fin point boxes
    const double scale           = absoluteScale();
    const double boxWidth        = kBoxWidth / scale;
    const double boxHalfWidth    = boxWidth / 2;
    const double selBoxWidth     = kSelectedBoxWidth / scale;
    const double selBoxHalfWidth = selBoxWidth / 2;
    const QPen pointPen = pixelPen(ThemeColors::color(ThemeColors::Key::FIN_POINT), kBoxLineWidth);
    const QPen selectedPen =
        pixelPen(ThemeColors::color(ThemeColors::Key::FIN_SELECTED_POINT), kBoxLineWidth);
    painter.setBrush(Qt::NoBrush);

    for (std::size_t currentIndex = 0; currentIndex < drawPoints.size(); ++currentIndex)
    {
        const Coordinate& c = drawPoints[currentIndex];
        if (std::cmp_equal(currentIndex, m_selectedIndex))
        {
            // The highlight colour, then the normal one again.
            painter.setPen(selectedPen);
            painter.drawRect(
                QRectF(c.x - selBoxHalfWidth, c.y - selBoxHalfWidth, selBoxWidth, selBoxWidth));
        }
        // Normal boxes
        painter.setPen(pointPen);
        painter.drawRect(QRectF(c.x - boxHalfWidth, c.y - boxHalfWidth, boxWidth, boxWidth));
    }
}

}  // namespace QtRocket::Gui
