#include "QtRocket/figure/RocketComponentShapeProvider.h"

#include <vector>

#include "QtRocket/figure/BodyTubeShapes.h"
#include "QtRocket/figure/ComponentAssemblyShapes.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/FinSetShapes.h"
#include "QtRocket/figure/LaunchLugShapes.h"
#include "QtRocket/figure/MassComponentShapes.h"
#include "QtRocket/figure/ParachuteShapes.h"
#include "QtRocket/figure/RailButtonShapes.h"
#include "QtRocket/figure/RingComponentShapes.h"
#include "QtRocket/figure/ShockCordShapes.h"
#include "QtRocket/figure/StreamerShapes.h"
#include "QtRocket/figure/TransitionShapes.h"
#include "QtRocket/figure/TubeFinSetShapes.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket::RocketComponentShapeProvider
{

std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    switch (component.kind())
    {
        case ComponentKind::BODY_TUBE:
            return BodyTubeShapes::getShapesSide(component, transformation);
        case ComponentKind::TRANSITION:
        case ComponentKind::NOSE_CONE:
            return TransitionShapes::getShapesSide(component, transformation);
        case ComponentKind::TUBE_FIN_SET:
            return TubeFinSetShapes::getShapesSide(component, transformation);
        case ComponentKind::LAUNCH_LUG:
            return LaunchLugShapes::getShapesSide(component, transformation);
        case ComponentKind::INNER_TUBE:
        case ComponentKind::TUBE_COUPLER:
        case ComponentKind::ENGINE_BLOCK:
        case ComponentKind::CENTERING_RING:
        case ComponentKind::BULKHEAD:
            return RingComponentShapes::getShapesSide(component, transformation);
        case ComponentKind::TRAPEZOID_FIN_SET:
        case ComponentKind::ELLIPTICAL_FIN_SET:
        case ComponentKind::FREEFORM_FIN_SET:
            return FinSetShapes::getShapesSide(component, transformation);
        case ComponentKind::RAIL_BUTTON:
            return RailButtonShapes::getShapesSide(component, transformation);
        case ComponentKind::MASS_COMPONENT:
            return MassComponentShapes::getShapesSide(component, transformation);
        case ComponentKind::SHOCK_CORD:
            return ShockCordShapes::getShapesSide(component, transformation);
        case ComponentKind::PARACHUTE:
            return ParachuteShapes::getShapesSide(component, transformation);
        case ComponentKind::STREAMER:
            return StreamerShapes::getShapesSide(component, transformation);
        case ComponentKind::ROCKET:
        case ComponentKind::AXIAL_STAGE:
        case ComponentKind::PARALLEL_STAGE:
        case ComponentKind::POD_SET:
            // ComponentAssemblyShapes returns nothing for a plain AxialStage.
            return ComponentAssemblyShapes::getShapesSide(component, transformation);
    }
    QTROCKET_UNREACHABLE();
}

std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    switch (component.kind())
    {
        case ComponentKind::BODY_TUBE:
            return BodyTubeShapes::getShapesBack(component, transformation);
        case ComponentKind::TRANSITION:
        case ComponentKind::NOSE_CONE:
            return TransitionShapes::getShapesBack(component, transformation);
        case ComponentKind::TUBE_FIN_SET:
            return TubeFinSetShapes::getShapesBack(component, transformation);
        case ComponentKind::LAUNCH_LUG:
            return LaunchLugShapes::getShapesBack(component, transformation);
        case ComponentKind::INNER_TUBE:
        case ComponentKind::TUBE_COUPLER:
        case ComponentKind::ENGINE_BLOCK:
        case ComponentKind::CENTERING_RING:
        case ComponentKind::BULKHEAD:
            return RingComponentShapes::getShapesBack(component, transformation);
        case ComponentKind::TRAPEZOID_FIN_SET:
        case ComponentKind::ELLIPTICAL_FIN_SET:
        case ComponentKind::FREEFORM_FIN_SET:
            return FinSetShapes::getShapesBack(component, transformation);
        case ComponentKind::RAIL_BUTTON:
            return RailButtonShapes::getShapesBack(component, transformation);
        case ComponentKind::MASS_COMPONENT:
            return MassComponentShapes::getShapesBack(component, transformation);
        case ComponentKind::SHOCK_CORD:
            return ShockCordShapes::getShapesBack(component, transformation);
        case ComponentKind::PARACHUTE:
            return ParachuteShapes::getShapesBack(component, transformation);
        case ComponentKind::STREAMER:
            return StreamerShapes::getShapesBack(component, transformation);
        case ComponentKind::ROCKET:
        case ComponentKind::AXIAL_STAGE:
        case ComponentKind::PARALLEL_STAGE:
        case ComponentKind::POD_SET:
            // ComponentAssemblyShapes returns nothing for a plain AxialStage.
            return ComponentAssemblyShapes::getShapesBack(component, transformation);
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
