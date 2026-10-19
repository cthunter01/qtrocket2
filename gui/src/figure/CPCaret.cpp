#include "QtRocket/gui/figure/CPCaret.h"

#include <QPainterPath>
#include <QRectF>
#include <Qt>

namespace QtRocket::Gui
{

QPainterPath CPCaret::shape() const
{
    // Java: the ring plus the disc of RADIUS * 0.75f.
    constexpr double kDiscRadius = kRadius * 0.75;

    QPainterPath caret = ring();
    caret.addEllipse(QRectF(-kDiscRadius, -kDiscRadius, 2 * kDiscRadius, 2 * kDiscRadius));
    caret.setFillRule(Qt::WindingFill);
    return caret;
}

}  // namespace QtRocket::Gui
