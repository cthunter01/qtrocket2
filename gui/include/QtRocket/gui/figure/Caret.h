#pragma once

#include <QColor>
#include <QPainterPath>

#include "QtRocket/gui/figure/FigureElement.h"
#include "QtRocket/gui/util/ThemeColors.h"

class QPainter;
class QRect;

namespace QtRocket::Gui
{

/// A mark at a point of the figure, filled in one colour (OpenRocket's Caret): the base of the CG
/// and CP marks (CGCaret, CPCaret). The mark is designed in pixels about its centre (shape()), and
/// paint() draws it at the same size whatever the scale: it applies (1/scale, 0, 0, 1/scale, x, y)
/// before the painter's transform, as Java's Area transform does. In the design view the painter's
/// transform is the figure's projection, so the mark is centred at (x, y) metres, 7 pixels in
/// radius and turned upside down with the y axis; the information overlay paints carets in its own
/// pixels at the scale 1.7, upright.
///
/// Deviation from OpenRocket: nothing is drawn when the position or the scale is not a finite
/// number, or when the mark would land beyond RocketFigurePainter::kMaxDeviceCoordinate pixels
/// (Java2D quietly drops NaN coordinates; RocketPanel moves the carets to NaN in the back view and
/// for a design without length, which Qt would warn about).
class Caret : public FigureElement
{
public:
    /// RADIUS of CGCaret and CPCaret: the radius of the mark in pixels.
    static constexpr double kRadius = 7.0;

    /// The position of the mark (getX(), getY()), in the units of the painter it is painted with.
    [[nodiscard]] double x() const noexcept { return m_x; }
    [[nodiscard]] double y() const noexcept { return m_y; }
    /// Moves the mark to (@p x, @p y) (setPosition()); NaN hides it.
    void setPosition(double x, double y) noexcept
    {
        m_x = x;
        m_y = y;
    }

    /// Fills shape() in color() at the position, its pixels divided by @p scale (paint(g2, scale)).
    void paint(QPainter& painter, double scale) const;
    /// paint(@p painter, @p scale): a caret does not need the visible rectangle.
    void paint(QPainter& painter, double scale, const QRect& visible) override;

    /// The mark in pixels about its centre, with the axes of the coordinates it is painted in (y
    /// up in the figure), filled with Qt::WindingFill (getCaret()).
    [[nodiscard]] virtual QPainterPath shape() const = 0;
    /// The key of the mark's colour in the theme (OR.colors.cg, OR.colors.cp).
    [[nodiscard]] virtual ThemeColors::Key colorKey() const = 0;
    /// The mark's colour: colorKey() in the application palette's theme (getColor()).
    [[nodiscard]] QColor color() const;

protected:
    /// A mark at (@p x, @p y).
    Caret(double x, double y) noexcept : m_x(x), m_y(y) { }

    /// The ring between the radius kRadius and 0.9 kRadius that both marks have, with
    /// Qt::WindingFill: the outer circle and the inner one in the opposite direction.
    [[nodiscard]] static QPainterPath ring();

private:
    double m_x;
    double m_y;
};

}  // namespace QtRocket::Gui
