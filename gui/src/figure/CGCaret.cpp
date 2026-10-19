#include "QtRocket/gui/figure/CGCaret.h"

#include <QPainterPath>
#include <QRectF>
#include <Qt>

namespace QtRocket::Gui
{

QPainterPath CGCaret::shape() const
{
    // Java: the circle of RADIUS less the squares (-RADIUS, -RADIUS, RADIUS, RADIUS) and
    // (0, 0, RADIUS, RADIUS), plus the ring. Here the two quarters that remain, as quarter discs
    // inside the ring: x >= 0 with y <= 0, and x <= 0 with y >= 0.
    constexpr double kQuarterRadius = kRadius * 0.9;
    const QRectF bounds(-kQuarterRadius, -kQuarterRadius, 2 * kQuarterRadius, 2 * kQuarterRadius);

    QPainterPath caret = ring();
    // Qt's angles turn from +x towards -y: 0 to 90 degrees is the quarter x >= 0, y <= 0.
    caret.moveTo(0, 0);
    caret.arcTo(bounds, 0, 90);
    caret.closeSubpath();
    // 180 to 270 degrees: the quarter x <= 0, y >= 0.
    caret.moveTo(0, 0);
    caret.arcTo(bounds, 180, 90);
    caret.closeSubpath();
    caret.setFillRule(Qt::WindingFill);
    return caret;
}

}  // namespace QtRocket::Gui
