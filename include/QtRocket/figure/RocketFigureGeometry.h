#pragma once

#include <span>
#include <vector>

#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket
{

class FlightConfiguration;
class RocketComponent;

/// What the 2D rocket figure shows of a flight configuration in one view, without any painting
/// (the geometry half of OpenRocket's RocketFigure: updateShapes(), addThisShape(), drawMotors()
/// and updateSubjectDimensions()): the shapes of every instance in the order they are drawn, the
/// motors, and the bounds of the subject, all in the figure's coordinates (metres, y up). A
/// painter draws shapes() in order (skipping the invisible shapes and the shapes of components
/// that are not RocketComponent::isVisible()), then motors() filled and outlined; a hit test asks
/// componentsAt().
///
/// The geometry holds observer pointers into the rocket's tree: it is valid only until the next
/// change of the configuration or the rocket, and must be rebuilt after any event (a TREE or
/// UNDO event replaces every component).
///
/// Deviation from OpenRocket: RocketFigure keeps the shapes in a PriorityQueue ordered by display
/// order only, whose order among shapes of equal display order follows the heap and the hash
/// order of its instance maps, so it can change from run to run and from paint to paint. Here
/// shapes of equal display order keep the order in which they were added: the active instances
/// before the extra render instances, each in tree order (InstanceMap), a component's instances
/// in their order, an instance's shapes in the provider's order. Shapes are drawn in a stable
/// order (the user's decision U4).
class RocketFigureGeometry
{
public:
    /// One shape of the figure, with the instance it belongs to.
    struct InstanceShape
    {
        /// The provider's shape (with its component, visibility, colour and line style).
        ComponentShape shape;
        /// The index of the instance among its component's contexts in getActiveInstances(), or
        /// in getExtraRenderInstances() for an extra render instance.
        int contextIndex{0};
        /// The instance's InstanceContext::instanceNumber.
        int instanceNumber{0};
        /// Whether the instance is one of FlightConfiguration::getExtraRenderInstances() (a
        /// booster set without children whose stage is active).
        bool extraRender{false};
    };

    /// One motor in the figure (RocketFigure.drawMotors): in the side and top views a Rect from
    /// the front of the motor, its length long and its diameter high; in the back view an Ellipse
    /// of its diameter. A painter fills it and then outlines it.
    struct MotorShape
    {
        FigureShape shape;
        /// The motor mount (an observer, like ComponentShape::component()).
        const RocketComponent* mount{nullptr};
        /// The index of the mount's location (RocketComponent::getComponentLocations()) the motor
        /// is drawn at.
        int locationIndex{0};
    };

    /// The figure of @p config in @p view, rotated by @p rotation radians about the rocket's axis:
    /// - for every active instance, then every extra render instance (each with the
    ///   transformation figureRotation(view, rotation).applyTransformation(context.transform)),
    ///   the shapes RocketComponentShapeProvider::getShapes() gives in the view (none for the
    ///   rocket and plain stages); when @p showMarkers is set (Preferences::isShowMarkers(),
    ///   "Only show pod set/booster markers when the pod set/booster is selected"), pod sets and
    ///   booster sets that are not in @p selection contribute nothing (their children still do);
    /// - sorted by displayOrder(view, component), highest first, so that the lowest ends on top
    ///   (ties as the class comment says);
    /// - the motors of every active motor configuration whose mount is visible, at each of the
    ///   mount's component locations: p = rotation(location + motor position), the motors in the
    ///   order of getActiveMotors(), each mount's locations in order;
    /// - the subject bounds (subjectBounds()).
    /// The selection is compared by identity, as in Java.
    [[nodiscard]] static RocketFigureGeometry build(
        const FlightConfiguration& config, FigureView view, double rotation,
        std::span<const RocketComponent* const> selection, bool showMarkers);

    [[nodiscard]] FigureView view() const noexcept { return m_view; }

    /// The figure's rotation about the rocket's axis, in radians.
    [[nodiscard]] double rotation() const noexcept { return m_rotation; }

    /// The shapes in the order they are drawn.
    [[nodiscard]] const std::vector<InstanceShape>& shapes() const noexcept { return m_shapes; }

    /// The motors, drawn after the shapes.
    [[nodiscard]] const std::vector<MotorShape>& motors() const noexcept { return m_motors; }

    /// The extent of the subject (RocketFigure.updateSubjectDimensions), from the configuration's
    /// bounding box and maxR, the larger distance from the axis of its two corners (Math.hypot of
    /// their y and z): in the side and top views (min x, -maxR, length, 2 maxR), in the back view
    /// (-maxR, -maxR, 2 maxR, 2 maxR). Finite for a design without components, whose bounding
    /// box is (0, 0, 0) to (1, 0, 0): (0, -0, 1, 0) and (-0, -0, 0, 0).
    [[nodiscard]] const Rectangle2D& subjectBounds() const noexcept { return m_subjectBounds; }

    /// The components that have a shape containing @p p (FigureShape::contains(), the hit test
    /// of RocketFigure.getComponentsByPoint, with @p p in the figure's coordinates), each once, in
    /// the order of their first such shape in shapes(): the shape drawn first comes first.
    /// Invisible shapes and the shapes of hidden components count, as in Java.
    [[nodiscard]] std::vector<const RocketComponent*> componentsAt(Point2D p) const;

private:
    RocketFigureGeometry(FigureView view, double rotation) noexcept
      : m_view(view), m_rotation(rotation)
    {
    }

    FigureView                 m_view;
    double                     m_rotation;
    std::vector<InstanceShape> m_shapes;
    std::vector<MotorShape>    m_motors;
    Rectangle2D                m_subjectBounds;
};

}  // namespace QtRocket
