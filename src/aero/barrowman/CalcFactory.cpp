#include "QtRocket/aero/barrowman/CalcFactory.h"

#include <format>
#include <memory>

#include "QtRocket/aero/barrowman/ComponentAssemblyCalc.h"
#include "QtRocket/aero/barrowman/FinSetCalc.h"
#include "QtRocket/aero/barrowman/LaunchLugCalc.h"
#include "QtRocket/aero/barrowman/RailButtonCalc.h"
#include "QtRocket/aero/barrowman/RocketComponentCalc.h"
#include "QtRocket/aero/barrowman/SymmetricComponentCalc.h"
#include "QtRocket/aero/barrowman/TubeFinSetCalc.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::CalcFactory
{

namespace
{

/// @p component as the class its kind() stands for (Java: the constructor parameter's type,
/// which Reflection.construct() checks with isInstance()).
template <class Component>
[[nodiscard]] const Component& as(const RocketComponent& component)
{
    const auto* const typed = dynamic_cast<const Component*>(&component);
    if (typed == nullptr)
    {
        bug(std::format("component {} is not of the class of its kind {}", component.getName(),
                        componentKindName(component.kind())));
    }
    return *typed;
}

}  // namespace

std::unique_ptr<RocketComponentCalc> create(const RocketComponent& component)
{
    // No default: a kind added to ComponentKind must be given its place here (-Wswitch).
    switch (component.kind())
    {
        case ComponentKind::NOSE_CONE:
        case ComponentKind::TRANSITION:
        case ComponentKind::BODY_TUBE:
            return std::make_unique<SymmetricComponentCalc>(as<SymmetricComponent>(component));
        case ComponentKind::TRAPEZOID_FIN_SET:
        case ComponentKind::ELLIPTICAL_FIN_SET:
        case ComponentKind::FREEFORM_FIN_SET:
            return std::make_unique<FinSetCalc>(as<FinSet>(component));
        case ComponentKind::TUBE_FIN_SET:
            return std::make_unique<TubeFinSetCalc>(as<TubeFinSet>(component));
        case ComponentKind::LAUNCH_LUG:
            return std::make_unique<LaunchLugCalc>(as<LaunchLug>(component));
        case ComponentKind::RAIL_BUTTON:
            return std::make_unique<RailButtonCalc>(as<RailButton>(component));
        case ComponentKind::ROCKET:
        case ComponentKind::AXIAL_STAGE:
        case ComponentKind::PARALLEL_STAGE:
        case ComponentKind::POD_SET:
            return std::make_unique<ComponentAssemblyCalc>(component);
        case ComponentKind::INNER_TUBE:
        case ComponentKind::TUBE_COUPLER:
        case ComponentKind::ENGINE_BLOCK:
        case ComponentKind::CENTERING_RING:
        case ComponentKind::BULKHEAD:
        case ComponentKind::MASS_COMPONENT:
        case ComponentKind::SHOCK_CORD:
        case ComponentKind::PARACHUTE:
        case ComponentKind::STREAMER:
            // Java: the class search ends at the abstract RocketComponentCalc, a BugException.
            break;
    }
    bug(std::format("no Barrowman calculation for component {} of kind {}", component.getName(),
                    componentKindName(component.kind())));
}

}  // namespace QtRocket::CalcFactory
