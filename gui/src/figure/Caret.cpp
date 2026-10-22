#include "QtRocket/gui/figure/Caret.h"

#include <cmath>

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QTransform>
#include <Qt>

#include "QtRocket/gui/figure/RocketFigurePainter.h"
#include "QtRocket/gui/util/ThemeColors.h"

namespace QtRocket::Gui
{

namespace
{

/// The ring's inner radius: RADIUS * 0.9f.
constexpr double kRingInnerRadius = Caret::kRadius * 0.9;

/// A circle of @p radius about the origin.
[[nodiscard]] QPainterPath circle(double radius)
{
    QPainterPath path;
    path.addEllipse(QRectF(-radius, -radius, 2 * radius, 2 * radius));
    return path;
}

}  // namespace

void Caret::paint(QPainter& painter, double scale) const
{
    if (!std::isfinite(m_x) || !std::isfinite(m_y) || !std::isfinite(scale) || scale <= 0)
    {
        return;
    }
    // Java: new AffineTransform(1.0/scale, 0, 0, 1.0/scale, x, y), applied before the painter's.
    const QTransform   caretTransform(1.0 / scale, 0, 0, 1.0 / scale, m_x, m_y);
    const QTransform   toDevice = caretTransform * painter.worldTransform();
    const QPainterPath caret    = toDevice.map(shape());
    if (!RocketFigurePainter::isDrawable(caret))
    {
        return;
    }

    painter.save();
    painter.setWorldTransform(QTransform{});
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color());
    painter.drawPath(caret);
    painter.restore();
}

void Caret::paint(QPainter& painter, double scale, const QRect& /*visible*/)
{
    // Carets don't need the visible rectangle, just call the single-parameter version
    paint(painter, scale);
}

QColor Caret::color() const
{
    return ThemeColors::color(colorKey());
}

QPainterPath Caret::ring()
{
    QPainterPath path = circle(kRadius);
    path.addPath(circle(kRingInnerRadius).toReversed());
    path.setFillRule(Qt::WindingFill);
    return path;
}

}  // namespace QtRocket::Gui
