#pragma once

#include <QPainterPath>

#include "QtRocket/gui/figure/Caret.h"
#include "QtRocket/gui/util/ThemeColors.h"

namespace QtRocket::Gui
{

/// The mark of the centre of pressure (OpenRocket's CPCaret): a filled disc of radius 5.25 pixels
/// inside a ring between the radius 7 and 6.3, in OR.colors.cp (red).
///
/// Deviation from OpenRocket: the circles are Qt's (kappa 0.5522847498, Java's Ellipse2D
/// 0.5522847498307933) and the radii are 0.9 * 7 and 0.75 * 7 in double (Java computes them in
/// float); the disc and the ring do not overlap, so no boolean operation on paths is needed.
class CPCaret final : public Caret
{
public:
    /// A mark at (@p x, @p y).
    CPCaret(double x, double y) noexcept : Caret(x, y) { }

    [[nodiscard]] QPainterPath shape() const override;
    /// ThemeColors::Key::CP.
    [[nodiscard]] ThemeColors::Key colorKey() const override { return ThemeColors::Key::CP; }
};

}  // namespace QtRocket::Gui
