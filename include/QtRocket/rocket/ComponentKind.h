#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace QtRocket
{

/// The concrete kind of a rocket component: one constant per class that OpenRocket persists in
/// .ork files. It replaces Java's reflection (getClass(), isCompatible(Class), the loader's
/// constructor map): RocketComponent::kind() returns it, isCompatible() takes it, and switches
/// on it choose aerodynamic calculators and savers.
///
/// Sleeve is not a kind: OpenRocket has no saver or loader for it (see the plan, section 8).
///
/// The category predicates below answer Java's Class.isAssignableFrom() for the abstract
/// classes of the hierarchy (ComponentAssembly, ExternalComponent, BodyComponent, FinSet, ...),
/// which is what the isCompatible() implementations ask.
enum class ComponentKind
{
    ROCKET,
    AXIAL_STAGE,
    PARALLEL_STAGE,
    POD_SET,
    BODY_TUBE,
    TRANSITION,
    NOSE_CONE,
    TRAPEZOID_FIN_SET,
    ELLIPTICAL_FIN_SET,
    FREEFORM_FIN_SET,
    TUBE_FIN_SET,
    LAUNCH_LUG,
    RAIL_BUTTON,
    INNER_TUBE,
    TUBE_COUPLER,
    ENGINE_BLOCK,
    CENTERING_RING,
    BULKHEAD,
    MASS_COMPONENT,
    SHOCK_CORD,
    PARACHUTE,
    STREAMER,
};

/// Every kind, in declaration order.
inline constexpr std::array<ComponentKind, 22> kAllComponentKinds{
    ComponentKind::ROCKET,
    ComponentKind::AXIAL_STAGE,
    ComponentKind::PARALLEL_STAGE,
    ComponentKind::POD_SET,
    ComponentKind::BODY_TUBE,
    ComponentKind::TRANSITION,
    ComponentKind::NOSE_CONE,
    ComponentKind::TRAPEZOID_FIN_SET,
    ComponentKind::ELLIPTICAL_FIN_SET,
    ComponentKind::FREEFORM_FIN_SET,
    ComponentKind::TUBE_FIN_SET,
    ComponentKind::LAUNCH_LUG,
    ComponentKind::RAIL_BUTTON,
    ComponentKind::INNER_TUBE,
    ComponentKind::TUBE_COUPLER,
    ComponentKind::ENGINE_BLOCK,
    ComponentKind::CENTERING_RING,
    ComponentKind::BULKHEAD,
    ComponentKind::MASS_COMPONENT,
    ComponentKind::SHOCK_CORD,
    ComponentKind::PARACHUTE,
    ComponentKind::STREAMER,
};

/// The .ork element name of the kind, as OpenRocketSaver writes it and DocumentConfig's
/// constructor map reads it: "bodytube", "stage", "parallelstage", "podset", ...; "rocket" for
/// the root element.
[[nodiscard]] std::string_view xmlName(ComponentKind kind) noexcept;

/// The kind of an .ork component element, or nullopt for an unknown one. Besides every
/// xmlName() this accepts the legacy "boosterset" (a PARALLEL_STAGE), as DocumentConfig does.
/// The comparison is exact (element names are case-sensitive).
[[nodiscard]] std::optional<ComponentKind> componentKindFromXmlName(std::string_view name) noexcept;

/// The constant's name, e.g. "BODY_TUBE".
[[nodiscard]] std::string_view componentKindName(ComponentKind kind) noexcept;

/// The Java class name of the kind (getClass().getSimpleName()), e.g. "BodyTube", as the debug
/// strings print it.
[[nodiscard]] std::string_view className(ComponentKind kind) noexcept;

/// The translation key of the component's name (the key getComponentName() looks up in
/// OpenRocket), e.g. "BodyTube.BodyTube", "Stage.Stage", "Rocket.compname.Rocket".
[[nodiscard]] std::string_view displayKey(ComponentKind kind) noexcept;

/// The English component name for displayKey() (OpenRocket's messages.properties): "Body Tube",
/// "Stage", "Booster Set", ... This is the default name of a new component.
[[nodiscard]] std::string_view displayName(ComponentKind kind) noexcept;

/// ComponentAssembly: ROCKET, AXIAL_STAGE, PARALLEL_STAGE, POD_SET.
[[nodiscard]] constexpr bool isAssembly(ComponentKind kind) noexcept
{
    return kind == ComponentKind::ROCKET || kind == ComponentKind::AXIAL_STAGE ||
           kind == ComponentKind::PARALLEL_STAGE || kind == ComponentKind::POD_SET;
}

/// AxialStage (a ParallelStage is an AxialStage): AXIAL_STAGE, PARALLEL_STAGE.
[[nodiscard]] constexpr bool isStage(ComponentKind kind) noexcept
{
    return kind == ComponentKind::AXIAL_STAGE || kind == ComponentKind::PARALLEL_STAGE;
}

/// BodyComponent (the SymmetricComponents): BODY_TUBE, TRANSITION, NOSE_CONE.
[[nodiscard]] constexpr bool isBodyComponent(ComponentKind kind) noexcept
{
    return kind == ComponentKind::BODY_TUBE || kind == ComponentKind::TRANSITION ||
           kind == ComponentKind::NOSE_CONE;
}

/// FinSet: the trapezoidal, elliptical and freeform fin sets (a TubeFinSet is not a FinSet).
[[nodiscard]] constexpr bool isFinSet(ComponentKind kind) noexcept
{
    return kind == ComponentKind::TRAPEZOID_FIN_SET || kind == ComponentKind::ELLIPTICAL_FIN_SET ||
           kind == ComponentKind::FREEFORM_FIN_SET;
}

/// ExternalComponent: the body components, the fin sets, TUBE_FIN_SET, LAUNCH_LUG and
/// RAIL_BUTTON.
[[nodiscard]] constexpr bool isExternal(ComponentKind kind) noexcept
{
    return isBodyComponent(kind) || isFinSet(kind) || kind == ComponentKind::TUBE_FIN_SET ||
           kind == ComponentKind::LAUNCH_LUG || kind == ComponentKind::RAIL_BUTTON;
}

/// RingComponent (the StructuralComponents): INNER_TUBE, TUBE_COUPLER, ENGINE_BLOCK,
/// CENTERING_RING, BULKHEAD.
[[nodiscard]] constexpr bool isRingComponent(ComponentKind kind) noexcept
{
    return kind == ComponentKind::INNER_TUBE || kind == ComponentKind::TUBE_COUPLER ||
           kind == ComponentKind::ENGINE_BLOCK || kind == ComponentKind::CENTERING_RING ||
           kind == ComponentKind::BULKHEAD;
}

/// RecoveryDevice: PARACHUTE, STREAMER.
[[nodiscard]] constexpr bool isRecoveryDevice(ComponentKind kind) noexcept
{
    return kind == ComponentKind::PARACHUTE || kind == ComponentKind::STREAMER;
}

/// MassObject: MASS_COMPONENT, SHOCK_CORD and the recovery devices.
[[nodiscard]] constexpr bool isMassObject(ComponentKind kind) noexcept
{
    return kind == ComponentKind::MASS_COMPONENT || kind == ComponentKind::SHOCK_CORD ||
           isRecoveryDevice(kind);
}

/// InternalComponent: the ring components and the mass objects.
[[nodiscard]] constexpr bool isInternal(ComponentKind kind) noexcept
{
    return isRingComponent(kind) || isMassObject(kind);
}

}  // namespace QtRocket
