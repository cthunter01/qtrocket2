#include "QtRocket/rocket/ComponentKind.h"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <string_view>

namespace QtRocket
{

namespace
{

struct KindInfo
{
    ComponentKind    kind;
    std::string_view xmlName;
    std::string_view name;
    std::string_view className;
    std::string_view displayKey;
    std::string_view displayName;
};

// Element names from DocumentConfig.java's constructor map and the savers; translation keys from
// each class's getComponentName(); English names from messages.properties.
constexpr std::array<KindInfo, kAllComponentKinds.size()> kKinds{{
    {.kind        = ComponentKind::ROCKET,
     .xmlName     = "rocket",
     .name        = "ROCKET",
     .className   = "Rocket",
     .displayKey  = "Rocket.compname.Rocket",
     .displayName = "Rocket"},
    {.kind        = ComponentKind::AXIAL_STAGE,
     .xmlName     = "stage",
     .name        = "AXIAL_STAGE",
     .className   = "AxialStage",
     .displayKey  = "Stage.Stage",
     .displayName = "Stage"},
    {.kind        = ComponentKind::PARALLEL_STAGE,
     .xmlName     = "parallelstage",
     .name        = "PARALLEL_STAGE",
     .className   = "ParallelStage",
     .displayKey  = "BoosterSet.BoosterSet",
     .displayName = "Booster Set"},
    {.kind        = ComponentKind::POD_SET,
     .xmlName     = "podset",
     .name        = "POD_SET",
     .className   = "PodSet",
     .displayKey  = "PodSet.PodSet",
     .displayName = "Pod Set"},
    {.kind        = ComponentKind::BODY_TUBE,
     .xmlName     = "bodytube",
     .name        = "BODY_TUBE",
     .className   = "BodyTube",
     .displayKey  = "BodyTube.BodyTube",
     .displayName = "Body Tube"},
    {.kind        = ComponentKind::TRANSITION,
     .xmlName     = "transition",
     .name        = "TRANSITION",
     .className   = "Transition",
     .displayKey  = "Transition.Transition",
     .displayName = "Transition"},
    {.kind        = ComponentKind::NOSE_CONE,
     .xmlName     = "nosecone",
     .name        = "NOSE_CONE",
     .className   = "NoseCone",
     .displayKey  = "NoseCone.NoseCone",
     .displayName = "Nose Cone"},
    {.kind        = ComponentKind::TRAPEZOID_FIN_SET,
     .xmlName     = "trapezoidfinset",
     .name        = "TRAPEZOID_FIN_SET",
     .className   = "TrapezoidFinSet",
     .displayKey  = "TrapezoidFinSet.TrapezoidFinSet",
     .displayName = "Trapezoidal Fin Set"},
    {.kind        = ComponentKind::ELLIPTICAL_FIN_SET,
     .xmlName     = "ellipticalfinset",
     .name        = "ELLIPTICAL_FIN_SET",
     .className   = "EllipticalFinSet",
     .displayKey  = "EllipticalFinSet.Ellipticalfinset",
     .displayName = "Elliptical Fin Set"},
    {.kind        = ComponentKind::FREEFORM_FIN_SET,
     .xmlName     = "freeformfinset",
     .name        = "FREEFORM_FIN_SET",
     .className   = "FreeformFinSet",
     .displayKey  = "FreeformFinSet.FreeformFinSet",
     .displayName = "Freeform Fin Set"},
    {.kind        = ComponentKind::TUBE_FIN_SET,
     .xmlName     = "tubefinset",
     .name        = "TUBE_FIN_SET",
     .className   = "TubeFinSet",
     .displayKey  = "TubeFinSet.TubeFinSet",
     .displayName = "Tube Fin Set"},
    {.kind        = ComponentKind::LAUNCH_LUG,
     .xmlName     = "launchlug",
     .name        = "LAUNCH_LUG",
     .className   = "LaunchLug",
     .displayKey  = "LaunchLug.Launchlug",
     .displayName = "Launch Lug"},
    {.kind        = ComponentKind::RAIL_BUTTON,
     .xmlName     = "railbutton",
     .name        = "RAIL_BUTTON",
     .className   = "RailButton",
     .displayKey  = "RailButton.RailButton",
     .displayName = "Rail Button"},
    {.kind        = ComponentKind::INNER_TUBE,
     .xmlName     = "innertube",
     .name        = "INNER_TUBE",
     .className   = "InnerTube",
     .displayKey  = "InnerTube.InnerTube",
     .displayName = "Inner Tube"},
    {.kind        = ComponentKind::TUBE_COUPLER,
     .xmlName     = "tubecoupler",
     .name        = "TUBE_COUPLER",
     .className   = "TubeCoupler",
     .displayKey  = "TubeCoupler.TubeCoupler",
     .displayName = "Tube Coupler"},
    {.kind        = ComponentKind::ENGINE_BLOCK,
     .xmlName     = "engineblock",
     .name        = "ENGINE_BLOCK",
     .className   = "EngineBlock",
     .displayKey  = "EngineBlock.EngineBlock",
     .displayName = "Engine Block"},
    {.kind        = ComponentKind::CENTERING_RING,
     .xmlName     = "centeringring",
     .name        = "CENTERING_RING",
     .className   = "CenteringRing",
     .displayKey  = "CenteringRing.CenteringRing",
     .displayName = "Centering Ring"},
    {.kind        = ComponentKind::BULKHEAD,
     .xmlName     = "bulkhead",
     .name        = "BULKHEAD",
     .className   = "Bulkhead",
     .displayKey  = "Bulkhead.Bulkhead",
     .displayName = "Bulkhead"},
    {.kind        = ComponentKind::MASS_COMPONENT,
     .xmlName     = "masscomponent",
     .name        = "MASS_COMPONENT",
     .className   = "MassComponent",
     .displayKey  = "MassComponent.MassComponent",
     .displayName = "Mass Component"},
    {.kind        = ComponentKind::SHOCK_CORD,
     .xmlName     = "shockcord",
     .name        = "SHOCK_CORD",
     .className   = "ShockCord",
     .displayKey  = "ShockCord.ShockCord",
     .displayName = "Shock Cord"},
    {.kind        = ComponentKind::PARACHUTE,
     .xmlName     = "parachute",
     .name        = "PARACHUTE",
     .className   = "Parachute",
     .displayKey  = "Parachute.Parachute",
     .displayName = "Parachute"},
    {.kind        = ComponentKind::STREAMER,
     .xmlName     = "streamer",
     .name        = "STREAMER",
     .className   = "Streamer",
     .displayKey  = "Streamer.Streamer",
     .displayName = "Streamer"},
}};

[[nodiscard]] constexpr const KindInfo& info(ComponentKind kind) noexcept
{
    for (const KindInfo& row : kKinds)
    {
        if (row.kind == kind)
        {
            return row;
        }
    }
    return kKinds.front();  // not reached: every kind has a row (checked below)
}

static_assert(std::ranges::all_of(kAllComponentKinds,
                                  [](ComponentKind kind) { return info(kind).kind == kind; }));

// The superclass walks of componentClassChain(), from the class declarations of OpenRocket's
// rocketcomponent package.
constexpr std::array<std::string_view, 3> kRocketChain{"Rocket", "ComponentAssembly",
                                                       "RocketComponent"};
constexpr std::array<std::string_view, 3> kAxialStageChain{"AxialStage", "ComponentAssembly",
                                                           "RocketComponent"};
constexpr std::array<std::string_view, 4> kParallelStageChain{
    "ParallelStage", "AxialStage", "ComponentAssembly", "RocketComponent"};
constexpr std::array<std::string_view, 3> kPodSetChain{"PodSet", "ComponentAssembly",
                                                       "RocketComponent"};
constexpr std::array<std::string_view, 5> kBodyTubeChain{
    "BodyTube", "SymmetricComponent", "BodyComponent", "ExternalComponent", "RocketComponent"};
constexpr std::array<std::string_view, 5> kTransitionChain{
    "Transition", "SymmetricComponent", "BodyComponent", "ExternalComponent", "RocketComponent"};
constexpr std::array<std::string_view, 6> kNoseConeChain{"NoseCone",           "Transition",
                                                         "SymmetricComponent", "BodyComponent",
                                                         "ExternalComponent",  "RocketComponent"};
constexpr std::array<std::string_view, 4> kTrapezoidFinSetChain{
    "TrapezoidFinSet", "FinSet", "ExternalComponent", "RocketComponent"};
constexpr std::array<std::string_view, 4> kEllipticalFinSetChain{
    "EllipticalFinSet", "FinSet", "ExternalComponent", "RocketComponent"};
constexpr std::array<std::string_view, 4> kFreeformFinSetChain{
    "FreeformFinSet", "FinSet", "ExternalComponent", "RocketComponent"};
constexpr std::array<std::string_view, 4> kTubeFinSetChain{"TubeFinSet", "Tube",
                                                           "ExternalComponent", "RocketComponent"};
constexpr std::array<std::string_view, 4> kLaunchLugChain{"LaunchLug", "Tube", "ExternalComponent",
                                                          "RocketComponent"};
constexpr std::array<std::string_view, 3> kRailButtonChain{"RailButton", "ExternalComponent",
                                                           "RocketComponent"};
constexpr std::array<std::string_view, 6> kInnerTubeChain{
    "InnerTube",           "ThicknessRingComponent", "RingComponent",
    "StructuralComponent", "InternalComponent",      "RocketComponent"};
constexpr std::array<std::string_view, 6> kTubeCouplerChain{
    "TubeCoupler",         "ThicknessRingComponent", "RingComponent",
    "StructuralComponent", "InternalComponent",      "RocketComponent"};
constexpr std::array<std::string_view, 6> kEngineBlockChain{
    "EngineBlock",         "ThicknessRingComponent", "RingComponent",
    "StructuralComponent", "InternalComponent",      "RocketComponent"};
constexpr std::array<std::string_view, 6> kCenteringRingChain{
    "CenteringRing",       "RadiusRingComponent", "RingComponent",
    "StructuralComponent", "InternalComponent",   "RocketComponent"};
constexpr std::array<std::string_view, 6> kBulkheadChain{"Bulkhead",          "RadiusRingComponent",
                                                         "RingComponent",     "StructuralComponent",
                                                         "InternalComponent", "RocketComponent"};
constexpr std::array<std::string_view, 4> kMassComponentChain{
    "MassComponent", "MassObject", "InternalComponent", "RocketComponent"};
constexpr std::array<std::string_view, 4> kShockCordChain{"ShockCord", "MassObject",
                                                          "InternalComponent", "RocketComponent"};
constexpr std::array<std::string_view, 5> kParachuteChain{
    "Parachute", "RecoveryDevice", "MassObject", "InternalComponent", "RocketComponent"};
constexpr std::array<std::string_view, 5> kStreamerChain{"Streamer", "RecoveryDevice", "MassObject",
                                                         "InternalComponent", "RocketComponent"};

[[nodiscard]] constexpr std::span<const std::string_view> classChain(ComponentKind kind) noexcept
{
    switch (kind)
    {
        case ComponentKind::ROCKET:
            return kRocketChain;
        case ComponentKind::AXIAL_STAGE:
            return kAxialStageChain;
        case ComponentKind::PARALLEL_STAGE:
            return kParallelStageChain;
        case ComponentKind::POD_SET:
            return kPodSetChain;
        case ComponentKind::BODY_TUBE:
            return kBodyTubeChain;
        case ComponentKind::TRANSITION:
            return kTransitionChain;
        case ComponentKind::NOSE_CONE:
            return kNoseConeChain;
        case ComponentKind::TRAPEZOID_FIN_SET:
            return kTrapezoidFinSetChain;
        case ComponentKind::ELLIPTICAL_FIN_SET:
            return kEllipticalFinSetChain;
        case ComponentKind::FREEFORM_FIN_SET:
            return kFreeformFinSetChain;
        case ComponentKind::TUBE_FIN_SET:
            return kTubeFinSetChain;
        case ComponentKind::LAUNCH_LUG:
            return kLaunchLugChain;
        case ComponentKind::RAIL_BUTTON:
            return kRailButtonChain;
        case ComponentKind::INNER_TUBE:
            return kInnerTubeChain;
        case ComponentKind::TUBE_COUPLER:
            return kTubeCouplerChain;
        case ComponentKind::ENGINE_BLOCK:
            return kEngineBlockChain;
        case ComponentKind::CENTERING_RING:
            return kCenteringRingChain;
        case ComponentKind::BULKHEAD:
            return kBulkheadChain;
        case ComponentKind::MASS_COMPONENT:
            return kMassComponentChain;
        case ComponentKind::SHOCK_CORD:
            return kShockCordChain;
        case ComponentKind::PARACHUTE:
            return kParachuteChain;
        case ComponentKind::STREAMER:
            return kStreamerChain;
    }
    return kRocketChain;  // not reached: every kind has a chain
}

// Every chain starts with the kind's class and ends with RocketComponent.
static_assert(std::ranges::all_of(kAllComponentKinds, [](ComponentKind kind) {
    const std::span<const std::string_view> chain = classChain(kind);
    return chain.front() == info(kind).className && chain.back() == "RocketComponent";
}));

}  // namespace

std::string_view xmlName(ComponentKind kind) noexcept
{
    return info(kind).xmlName;
}

std::optional<ComponentKind> componentKindFromXmlName(std::string_view name) noexcept
{
    if (name == "boosterset")
    {
        return ComponentKind::PARALLEL_STAGE;  // legacy element name (DocumentConfig)
    }
    for (const KindInfo& row : kKinds)
    {
        if (row.xmlName == name)
        {
            return row.kind;
        }
    }
    return std::nullopt;
}

std::string_view componentKindName(ComponentKind kind) noexcept
{
    return info(kind).name;
}

std::string_view className(ComponentKind kind) noexcept
{
    return info(kind).className;
}

std::span<const std::string_view> componentClassChain(ComponentKind kind) noexcept
{
    return classChain(kind);
}

std::string_view displayKey(ComponentKind kind) noexcept
{
    return info(kind).displayKey;
}

std::string_view displayName(ComponentKind kind) noexcept
{
    return info(kind).displayName;
}

}  // namespace QtRocket
