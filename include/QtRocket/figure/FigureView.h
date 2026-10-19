#pragma once

#include <array>
#include <string_view>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket
{

class RocketComponent;

/// The 2D views of the rocket figure (RocketPanel.VIEW_TYPE's TopView, SideView and BackView;
/// the 3D types arrive with the 3D figure). The side and top views look at the rocket from the
/// side, x (the rocket's axis, nose at 0) to the right and y up, the top view with the rocket
/// turned a further quarter turn; the back view looks along the axis from behind, z to the right
/// and y up.
enum class FigureView
{
    TOP,
    SIDE,
    BACK,
};

/// Every view, in the order of the view selector (VIEW_TYPE.values() without the 3D types).
inline constexpr std::array<FigureView, 3> kAllFigureViews{FigureView::TOP, FigureView::SIDE,
                                                           FigureView::BACK};

/// The view a new figure shows (RocketFigure's currentViewType).
inline constexpr FigureView kDefaultFigureView = FigureView::SIDE;

/// The constant's name: "TOP", "SIDE" or "BACK".
[[nodiscard]] std::string_view figureViewName(FigureView view) noexcept;

/// The translation key of the view's name, e.g. "RocketPanel.FigTypeAct.TopView".
[[nodiscard]] std::string_view displayKey(FigureView view) noexcept;

/// The English name for displayKey() (OpenRocket's messages.properties): "Top view", "Side view"
/// or "Back view".
[[nodiscard]] std::string_view displayName(FigureView view) noexcept;

/// True for the views drawn with the components' side shapes, TOP and SIDE; BACK uses the back
/// shapes (RocketFigure.addThisShape's switch).
[[nodiscard]] constexpr bool usesSideShapes(FigureView view) noexcept
{
    return view != FigureView::BACK;
}

/// The rotation from the rocket's frame into the figure's for a figure rotated by @p rotation
/// radians about the rocket's axis (RocketFigure.getFigureRotation): rotateX(rotation), and for
/// the top view rotateX(rotation).applyTransformation(rotateX(-PI/2)), a further quarter turn
/// applied first. Every instance's transformation in the figure is this one applied after the
/// instance's own: figureRotation(view, rotation).applyTransformation(context.transform).
[[nodiscard]] Transformation figureRotation(FigureView view, double rotation) noexcept;

/// The figure's rotation as the carets and the rotation control see it
/// (RocketFigure.getRotation(true)): @p rotation, less PI/2 in the top view.
[[nodiscard]] double rotationWithTopViewOffset(FigureView view, double rotation) noexcept;

/// The point of the figure's plane at which @p c, a coordinate already in the figure's frame
/// (transformed by figureRotation()), is drawn: (x, y) in the side and top views, (z, y) in the
/// back view, as the shape providers project their coordinates.
[[nodiscard]] Point2D projectToView(FigureView view, const Coordinate& c) noexcept;

/// The display order of @p component in the view: getDisplayOrderSide() in the side and top
/// views, getDisplayOrderBack() in the back view. RocketFigure draws the shapes of the highest
/// order first, so the lowest ends up on top.
[[nodiscard]] int displayOrder(FigureView view, const RocketComponent& component) noexcept;

}  // namespace QtRocket
