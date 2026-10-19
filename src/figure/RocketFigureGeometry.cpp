#include "QtRocket/figure/RocketFigureGeometry.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/RocketComponentShapeProvider.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/InstanceContext.h"
#include "QtRocket/rocket/InstanceMap.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket
{

namespace
{

/// Whether @p component is a pod set or a booster set, whose markers the preference
/// ShowMarkers limits to the selection.
[[nodiscard]] bool isMarkerAssembly(const RocketComponent& component) noexcept
{
    return component.kind() == ComponentKind::POD_SET ||
           component.kind() == ComponentKind::PARALLEL_STAGE;
}

/// Whether @p component is in @p selection (by identity).
[[nodiscard]] bool isSelected(const RocketComponent&                  component,
                              std::span<const RocketComponent* const> selection) noexcept
{
    return std::ranges::find(selection, &component) != selection.end();
}

/// RocketFigure.updateSubjectDimensions.
[[nodiscard]] Rectangle2D subjectBoundsOf(const FlightConfiguration& config, FigureView view)
{
    const BoundingBox bounds = config.getBoundingBox();

    const double maxR = MathUtil::javaMax(std::hypot(bounds.min().y, bounds.min().z),
                                          std::hypot(bounds.max().y, bounds.max().z));

    if (usesSideShapes(view))
    {
        return Rectangle2D{
            .x = bounds.min().x, .y = -maxR, .width = bounds.span().x, .height = 2 * maxR};
    }
    return Rectangle2D{.x = -maxR, .y = -maxR, .width = 2 * maxR, .height = 2 * maxR};
}

/// RocketFigure.addShapesFromInstanceEntries: the shapes of the instances of @p instances in
/// @p view, with the figure's rotation @p viewRotation applied after each instance's.
void addShapesFromInstanceEntries(std::vector<RocketFigureGeometry::InstanceShape>& allShapes,
                                  const InstanceMap& instances, bool extraRender, FigureView view,
                                  const Transformation&                   viewRotation,
                                  std::span<const RocketComponent* const> selection,
                                  bool                                    showMarkers)
{
    for (const auto& [comp, contextList] : instances)
    {
        // Only draw pod sets and boosters when they are selected
        if (showMarkers && isMarkerAssembly(*comp) && !isSelected(*comp, selection))
        {
            continue;
        }

        for (std::size_t index = 0; index < contextList.size(); ++index)
        {
            const InstanceContext& context = contextList[index];
            const Transformation   currentTransform =
                viewRotation.applyTransformation(context.transform);
            for (ComponentShape& shape :
                 RocketComponentShapeProvider::getShapes(view, *comp, currentTransform))
            {
                allShapes.push_back(
                    RocketFigureGeometry::InstanceShape{.shape          = std::move(shape),
                                                        .contextIndex   = static_cast<int>(index),
                                                        .instanceNumber = context.instanceNumber,
                                                        .extraRender    = extraRender});
            }
        }
    }
}

/// RocketFigure.drawMotors, without the drawing.
[[nodiscard]] std::vector<RocketFigureGeometry::MotorShape> motorShapesOf(
    const FlightConfiguration& config, FigureView view, const Transformation& viewRotation)
{
    std::vector<RocketFigureGeometry::MotorShape> motors;
    for (const MotorConfiguration& curInstance : config.getActiveMotors())
    {
        const MotorMount&      mount          = curInstance.getMount();
        const RocketComponent& mountComponent = asComponent(mount);
        if (!mountComponent.isVisible())
        {
            continue;
        }

        const std::shared_ptr<const Motor>& motor = curInstance.getMotor();
        // The active motors all have one (Java would throw a NullPointerException).
        QTROCKET_ASSERT(motor != nullptr);
        const double motorLength = motor->getLength();
        const double motorRadius = motor->getDiameter() / 2;

        // <component>.getComponentLocations() will return all the parent instances of this
        // owning component, AND all of its own instances as well. so, just draw a motor once for
        // each Coordinate returned...
        const std::vector<Coordinate> mountLocations = mountComponent.getComponentLocations();

        const Coordinate motorPosition = mount.getMotorPosition(config.getId());
        for (std::size_t index = 0; index < mountLocations.size(); ++index)
        {
            Coordinate curMotorLocation = mountLocations[index].add(motorPosition);

            // rotate by figure's axial rotation:
            curMotorLocation = viewRotation.transform(curMotorLocation);

            FigureShape s =
                usesSideShapes(view)
                    ? FigureShape{FigureRect{curMotorLocation.x, (curMotorLocation.y - motorRadius),
                                             motorLength, 2 * motorRadius}}
                    : FigureShape{FigureEllipse{(curMotorLocation.z - motorRadius),
                                                (curMotorLocation.y - motorRadius), 2 * motorRadius,
                                                2 * motorRadius}};
            motors.push_back(
                RocketFigureGeometry::MotorShape{.shape         = std::move(s),
                                                 .mount         = &mountComponent,
                                                 .locationIndex = static_cast<int>(index)});
        }
    }
    return motors;
}

}  // namespace

RocketFigureGeometry RocketFigureGeometry::build(const FlightConfiguration& config, FigureView view,
                                                 double                                  rotation,
                                                 std::span<const RocketComponent* const> selection,
                                                 bool showMarkers)
{
    RocketFigureGeometry geometry{view, rotation};
    const Transformation figureRot = figureRotation(view, rotation);

    geometry.m_subjectBounds = subjectBoundsOf(config, view);

    addShapesFromInstanceEntries(geometry.m_shapes, config.getActiveInstances(), false, view,
                                 figureRot, selection, showMarkers);
    addShapesFromInstanceEntries(geometry.m_shapes, config.getExtraRenderInstances(), true, view,
                                 figureRot, selection, showMarkers);

    // The highest display order is drawn first; equal ones keep their order (see the class
    // comment).
    std::ranges::stable_sort(geometry.m_shapes, [view](const InstanceShape& a,
                                                       const InstanceShape& b) {
        return displayOrder(view, a.shape.component()) > displayOrder(view, b.shape.component());
    });

    geometry.m_motors = motorShapesOf(config, view, figureRot);
    return geometry;
}

std::vector<const RocketComponent*> RocketFigureGeometry::componentsAt(Point2D p) const
{
    std::vector<const RocketComponent*> components;
    for (const InstanceShape& instanceShape : m_shapes)
    {
        const RocketComponent* component = &instanceShape.shape.component();
        if (instanceShape.shape.shape().contains(p) &&
            std::ranges::find(components, component) == components.end())
        {
            components.push_back(component);
        }
    }
    return components;
}

}  // namespace QtRocket
