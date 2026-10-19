#pragma once

#include <QPainterPath>

#include "QtRocket/gui/figure/Caret.h"
#include "QtRocket/gui/util/ThemeColors.h"

namespace QtRocket::Gui
{

/// The mark of the centre of gravity (OpenRocket's CGCaret): a circle of radius 7 pixels with every
/// second quarter filled, in OR.colors.cg (blue). Its quarters are those of Java's Area: the circle
/// less the squares (-7, -7, 7, 7) and (0, 0, 7, 7), in the coordinates it is painted in, plus the
/// ring between the radius 7 and 6.3. In the figure, whose y axis points up, the filled quarters
/// are the upper left and the lower right one.
///
/// Deviation from OpenRocket: the quarters are drawn as quarter discs of radius 6.3 inside the ring
/// instead of Java's area arithmetic (the same set of points), so that no boolean operation on
/// paths is needed; the circles are Qt's (kappa 0.5522847498, Java's Ellipse2D 0.5522847498307933)
/// and the inner radius is 0.9 * 7 in double (Java computes 0.9f * 7 in float).
class CGCaret final : public Caret
{
public:
    /// A mark at (@p x, @p y).
    CGCaret(double x, double y) noexcept : Caret(x, y) { }

    [[nodiscard]] QPainterPath shape() const override;
    /// ThemeColors::Key::CG.
    [[nodiscard]] ThemeColors::Key colorKey() const override { return ThemeColors::Key::CG; }
};

}  // namespace QtRocket::Gui
