#include "QtRocket/rocket/preset/ComponentPresetType.h"

#include <optional>
#include <string_view>

#include "QtRocket/rocket/ComponentKind.h"

namespace QtRocket
{

std::string_view componentPresetTypeName(ComponentPresetType type) noexcept
{
    switch (type)
    {
        case ComponentPresetType::BODY_TUBE:
            return "BODY_TUBE";
        case ComponentPresetType::NOSE_CONE:
            return "NOSE_CONE";
        case ComponentPresetType::TRANSITION:
            return "TRANSITION";
        case ComponentPresetType::TUBE_COUPLER:
            return "TUBE_COUPLER";
        case ComponentPresetType::BULK_HEAD:
            return "BULK_HEAD";
        case ComponentPresetType::CENTERING_RING:
            return "CENTERING_RING";
        case ComponentPresetType::ENGINE_BLOCK:
            return "ENGINE_BLOCK";
        case ComponentPresetType::LAUNCH_LUG:
            return "LAUNCH_LUG";
        case ComponentPresetType::RAIL_BUTTON:
            return "RAIL_BUTTON";
        case ComponentPresetType::STREAMER:
            return "STREAMER";
        case ComponentPresetType::PARACHUTE:
            return "PARACHUTE";
    }
    return "BODY_TUBE";
}

std::optional<ComponentPresetType> componentPresetTypeFromName(std::string_view name) noexcept
{
    for (const ComponentPresetType type : kAllComponentPresetTypes)
    {
        if (componentPresetTypeName(type) == name)
        {
            return type;
        }
    }
    return std::nullopt;
}

ComponentKind componentKind(ComponentPresetType type) noexcept
{
    switch (type)
    {
        case ComponentPresetType::BODY_TUBE:
            return ComponentKind::BODY_TUBE;
        case ComponentPresetType::NOSE_CONE:
            return ComponentKind::NOSE_CONE;
        case ComponentPresetType::TRANSITION:
            return ComponentKind::TRANSITION;
        case ComponentPresetType::TUBE_COUPLER:
            return ComponentKind::TUBE_COUPLER;
        case ComponentPresetType::BULK_HEAD:
            return ComponentKind::BULKHEAD;
        case ComponentPresetType::CENTERING_RING:
            return ComponentKind::CENTERING_RING;
        case ComponentPresetType::ENGINE_BLOCK:
            return ComponentKind::ENGINE_BLOCK;
        case ComponentPresetType::LAUNCH_LUG:
            return ComponentKind::LAUNCH_LUG;
        case ComponentPresetType::RAIL_BUTTON:
            return ComponentKind::RAIL_BUTTON;
        case ComponentPresetType::STREAMER:
            return ComponentKind::STREAMER;
        case ComponentPresetType::PARACHUTE:
            return ComponentKind::PARACHUTE;
    }
    return ComponentKind::BODY_TUBE;
}

std::optional<ComponentPresetType> presetTypeOf(ComponentKind kind) noexcept
{
    switch (kind)
    {
        case ComponentKind::BODY_TUBE:
        case ComponentKind::INNER_TUBE:
        case ComponentKind::TUBE_FIN_SET:
            return ComponentPresetType::BODY_TUBE;
        case ComponentKind::NOSE_CONE:
            return ComponentPresetType::NOSE_CONE;
        case ComponentKind::TRANSITION:
            return ComponentPresetType::TRANSITION;
        case ComponentKind::TUBE_COUPLER:
            return ComponentPresetType::TUBE_COUPLER;
        case ComponentKind::BULKHEAD:
            return ComponentPresetType::BULK_HEAD;
        case ComponentKind::CENTERING_RING:
            return ComponentPresetType::CENTERING_RING;
        case ComponentKind::ENGINE_BLOCK:
            return ComponentPresetType::ENGINE_BLOCK;
        case ComponentKind::LAUNCH_LUG:
            return ComponentPresetType::LAUNCH_LUG;
        case ComponentKind::RAIL_BUTTON:
            return ComponentPresetType::RAIL_BUTTON;
        case ComponentKind::STREAMER:
            return ComponentPresetType::STREAMER;
        case ComponentKind::PARACHUTE:
            return ComponentPresetType::PARACHUTE;
        case ComponentKind::ROCKET:
        case ComponentKind::AXIAL_STAGE:
        case ComponentKind::PARALLEL_STAGE:
        case ComponentKind::POD_SET:
        case ComponentKind::TRAPEZOID_FIN_SET:
        case ComponentKind::ELLIPTICAL_FIN_SET:
        case ComponentKind::FREEFORM_FIN_SET:
        case ComponentKind::MASS_COMPONENT:
        case ComponentKind::SHOCK_CORD:
            return std::nullopt;
    }
    return std::nullopt;
}

}  // namespace QtRocket
