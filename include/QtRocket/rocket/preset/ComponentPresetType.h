#pragma once

#include <array>
#include <optional>
#include <string_view>

#include "QtRocket/rocket/ComponentKind.h"

namespace QtRocket
{

/// The kind of component a preset describes (OpenRocket's ComponentPreset.Type), in Java's
/// declaration order. ComponentPreset::Type names it too; it has a header of its own so that
/// TypedKey (a preset value can be a type) and RocketComponent (getPresetType()) do not depend on
/// ComponentPreset.
enum class ComponentPresetType
{
    BODY_TUBE,
    NOSE_CONE,
    TRANSITION,
    TUBE_COUPLER,
    BULK_HEAD,
    CENTERING_RING,
    ENGINE_BLOCK,
    LAUNCH_LUG,
    RAIL_BUTTON,
    STREAMER,
    PARACHUTE,
};

/// Every preset type, in declaration order (Type.values()).
inline constexpr std::array<ComponentPresetType, 11> kAllComponentPresetTypes{
    ComponentPresetType::BODY_TUBE,    ComponentPresetType::NOSE_CONE,
    ComponentPresetType::TRANSITION,   ComponentPresetType::TUBE_COUPLER,
    ComponentPresetType::BULK_HEAD,    ComponentPresetType::CENTERING_RING,
    ComponentPresetType::ENGINE_BLOCK, ComponentPresetType::LAUNCH_LUG,
    ComponentPresetType::RAIL_BUTTON,  ComponentPresetType::STREAMER,
    ComponentPresetType::PARACHUTE};

/// The constant's name, e.g. "BULK_HEAD" (Java: name() and toString()), which the preset digest
/// and the type="" attribute of the .ork <preset> element hold.
[[nodiscard]] std::string_view componentPresetTypeName(ComponentPresetType type) noexcept;

/// The preset type named exactly @p name (Type.valueOf()), or nullopt.
[[nodiscard]] std::optional<ComponentPresetType> componentPresetTypeFromName(
    std::string_view name) noexcept;

/// The component a preset of @p type is made for: BODY_TUBE, NOSE_CONE, TRANSITION,
/// TUBE_COUPLER, BULKHEAD, CENTERING_RING, ENGINE_BLOCK, LAUNCH_LUG, RAIL_BUTTON, STREAMER,
/// PARACHUTE.
[[nodiscard]] ComponentKind componentKind(ComponentPresetType type) noexcept;

/// The preset type of a component of @p kind (RocketComponent.getPresetType() of the concrete
/// classes): the type of the same name, BODY_TUBE also for an inner tube and a tube fin set, and
/// nullopt for the kinds without presets (the assemblies, the fin sets, mass components and
/// shock cords).
[[nodiscard]] std::optional<ComponentPresetType> presetTypeOf(ComponentKind kind) noexcept;

}  // namespace QtRocket
