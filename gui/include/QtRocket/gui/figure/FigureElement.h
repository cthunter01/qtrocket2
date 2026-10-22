#pragma once

class QPainter;
class QRect;

namespace QtRocket::Gui
{

/// Something drawn over the rocket in the design view (OpenRocket's FigureElement): the CG and CP
/// carets, the information overlay (RocketInfo), the calipers (Milestone 5). RocketFigure paints
/// its elements after the rocket, in three lists (addRelativeExtra(), addAbsoluteExtra(),
/// addRelativeTopExtra()): a relative element is painted with the painter's world transform set
/// to the figure's projection (metres, y up) and the figure's absolute scale in pixels per metre;
/// an absolute one in the widget's pixels with the scale 1.
///
/// Deviation from OpenRocket: its interface has a second paint(g2, scale) without the visible
/// rectangle, which RocketFigure never calls (RocketInfo's throws); here there is only the one
/// RocketFigure calls. Caret keeps the two-argument paint() for the information overlay, which
/// draws carets of its own.
class FigureElement
{
public:
    virtual ~FigureElement() = default;

    /// Paints the element with @p painter, at @p scale pixels per unit of the painter's
    /// coordinates (the figure's absolute scale for a relative element, 1 for an absolute one);
    /// @p visible is the part of the figure that can be seen, in the widget's pixels
    /// (getVisibleRect()).
    virtual void paint(QPainter& painter, double scale, const QRect& visible) = 0;

protected:
    FigureElement()                                = default;
    FigureElement(const FigureElement&)            = default;
    FigureElement& operator=(const FigureElement&) = default;
    FigureElement(FigureElement&&)                 = default;
    FigureElement& operator=(FigureElement&&)      = default;
};

}  // namespace QtRocket::Gui
