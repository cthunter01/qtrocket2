#include "QtRocket/figure/RocketComponentShapeProvider.h"

#include <format>
#include <vector>

#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket::RocketComponentShapeProvider
{

namespace
{

/// Java's IllegalArgumentException of findShapeService, for a kind that has no provider yet.
[[noreturn]] void noProvider(const RocketComponent& component)
{
    bug(std::format("No suitable shape provider found for component: {} ({})", component.toString(),
                    className(component.kind())));
}

}  // namespace

std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                          const Transformation& /*transformation*/)
{
    switch (component.kind())
    {
        case ComponentKind::AXIAL_STAGE:
            // ComponentAssemblyShapes ignores normal stages.
            return {};
        case ComponentKind::ROCKET:
        case ComponentKind::PARALLEL_STAGE:
        case ComponentKind::POD_SET:
        case ComponentKind::BODY_TUBE:
        case ComponentKind::TRANSITION:
        case ComponentKind::NOSE_CONE:
        case ComponentKind::TRAPEZOID_FIN_SET:
        case ComponentKind::ELLIPTICAL_FIN_SET:
        case ComponentKind::FREEFORM_FIN_SET:
        case ComponentKind::TUBE_FIN_SET:
        case ComponentKind::LAUNCH_LUG:
        case ComponentKind::RAIL_BUTTON:
        case ComponentKind::INNER_TUBE:
        case ComponentKind::TUBE_COUPLER:
        case ComponentKind::ENGINE_BLOCK:
        case ComponentKind::CENTERING_RING:
        case ComponentKind::BULKHEAD:
        case ComponentKind::MASS_COMPONENT:
        case ComponentKind::SHOCK_CORD:
        case ComponentKind::PARACHUTE:
        case ComponentKind::STREAMER:
            // The providers of these kinds arrive with the shape ports (run 1, F2 and F3).
            noProvider(component);
    }
    QTROCKET_UNREACHABLE();
}

std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                          const Transformation& /*transformation*/)
{
    switch (component.kind())
    {
        case ComponentKind::AXIAL_STAGE:
            // ComponentAssemblyShapes ignores normal stages.
            return {};
        case ComponentKind::ROCKET:
        case ComponentKind::PARALLEL_STAGE:
        case ComponentKind::POD_SET:
        case ComponentKind::BODY_TUBE:
        case ComponentKind::TRANSITION:
        case ComponentKind::NOSE_CONE:
        case ComponentKind::TRAPEZOID_FIN_SET:
        case ComponentKind::ELLIPTICAL_FIN_SET:
        case ComponentKind::FREEFORM_FIN_SET:
        case ComponentKind::TUBE_FIN_SET:
        case ComponentKind::LAUNCH_LUG:
        case ComponentKind::RAIL_BUTTON:
        case ComponentKind::INNER_TUBE:
        case ComponentKind::TUBE_COUPLER:
        case ComponentKind::ENGINE_BLOCK:
        case ComponentKind::CENTERING_RING:
        case ComponentKind::BULKHEAD:
        case ComponentKind::MASS_COMPONENT:
        case ComponentKind::SHOCK_CORD:
        case ComponentKind::PARACHUTE:
        case ComponentKind::STREAMER:
            // The providers of these kinds arrive with the shape ports (run 1, F2 and F3).
            noProvider(component);
    }
    QTROCKET_UNREACHABLE();
}

std::vector<ComponentShape> getShapes(FigureView view, const RocketComponent& component,
                                      const Transformation& transformation)
{
    const ComponentKind kind = component.kind();
    if (kind == ComponentKind::ROCKET || kind == ComponentKind::AXIAL_STAGE)
    {
        // no-op; no shapes here
        return {};
    }
    if (usesSideShapes(view))
    {
        return getShapesSide(component, transformation);
    }
    return getShapesBack(component, transformation);
}

}  // namespace QtRocket::RocketComponentShapeProvider
