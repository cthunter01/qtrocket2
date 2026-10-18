#include "QtRocket/figure/FigureView.h"

#include <numbers>
#include <string_view>

#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket
{

std::string_view figureViewName(FigureView view) noexcept
{
    switch (view)
    {
        case FigureView::TOP:
            return "TOP";
        case FigureView::SIDE:
            return "SIDE";
        case FigureView::BACK:
            return "BACK";
    }
    return "SIDE";
}

std::string_view displayKey(FigureView view) noexcept
{
    switch (view)
    {
        case FigureView::TOP:
            return "RocketPanel.FigTypeAct.TopView";
        case FigureView::SIDE:
            return "RocketPanel.FigTypeAct.SideView";
        case FigureView::BACK:
            return "RocketPanel.FigTypeAct.BackView";
    }
    return "RocketPanel.FigTypeAct.SideView";
}

std::string_view displayName(FigureView view) noexcept
{
    switch (view)
    {
        case FigureView::TOP:
            return "Top view";
        case FigureView::SIDE:
            return "Side view";
        case FigureView::BACK:
            return "Back view";
    }
    return "Side view";
}

Transformation figureRotation(FigureView view, double rotation) noexcept
{
    const Transformation axialRotation = Transformation::rotateX(rotation);
    if (view == FigureView::TOP)
    {
        return axialRotation.applyTransformation(Transformation::rotateX(-std::numbers::pi / 2));
    }
    return axialRotation;
}

double rotationWithTopViewOffset(FigureView view, double rotation) noexcept
{
    if (view == FigureView::TOP)
    {
        return rotation - (std::numbers::pi / 2);
    }
    return rotation;
}

Point2D projectToView(FigureView view, const Coordinate& c) noexcept
{
    if (usesSideShapes(view))
    {
        return Point2D{c.x, c.y};
    }
    return Point2D{c.z, c.y};
}

int displayOrder(FigureView view, const RocketComponent& component) noexcept
{
    if (usesSideShapes(view))
    {
        return component.getDisplayOrderSide();
    }
    return component.getDisplayOrderBack();
}

}  // namespace QtRocket
