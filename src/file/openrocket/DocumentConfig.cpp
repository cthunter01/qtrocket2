#include "QtRocket/file/openrocket/DocumentConfig.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/file/openrocket/BooleanSetter.h"
#include "QtRocket/file/openrocket/ColorSetter.h"
#include "QtRocket/file/openrocket/DoubleSetter.h"
#include "QtRocket/file/openrocket/EnumSetter.h"
#include "QtRocket/file/openrocket/IntSetter.h"
#include "QtRocket/file/openrocket/OverrideSetter.h"
#include "QtRocket/file/openrocket/Setter.h"
#include "QtRocket/file/openrocket/StringSetter.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/DesignType.h"
#include "QtRocket/rocket/EllipticalFinSet.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/RadiusRingComponent.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/ReferenceType.h"
#include "QtRocket/rocket/RingComponent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/ThicknessRingComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/TubeCoupler.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

// ====================================================================== the component table

/// A new C (Java: Constructor.newInstance()).
template <class C>
[[nodiscard]] std::unique_ptr<RocketComponent> make()
{
    return std::make_unique<C>();
}

/// One entry of Java's constructors map.
struct ComponentConstructor
{
    std::string_view element;
    std::unique_ptr<RocketComponent> (*make)();
};

/// DocumentConfig.constructors, sorted by element name (Java: a HashMap, filled in the order
/// external components, internal components, other).
constexpr std::array<ComponentConstructor, 22> kConstructors{{
    {.element = "bodytube", .make = &make<BodyTube>},
    {.element = "boosterset", .make = &make<ParallelStage>},
    {.element = "bulkhead", .make = &make<Bulkhead>},
    {.element = "centeringring", .make = &make<CenteringRing>},
    {.element = "ellipticalfinset", .make = &make<EllipticalFinSet>},
    {.element = "engineblock", .make = &make<EngineBlock>},
    {.element = "freeformfinset", .make = &make<FreeformFinSet>},
    {.element = "innertube", .make = &make<InnerTube>},
    {.element = "launchlug", .make = &make<LaunchLug>},
    {.element = "masscomponent", .make = &make<MassComponent>},
    {.element = "nosecone", .make = &make<NoseCone>},
    {.element = "parachute", .make = &make<Parachute>},
    {.element = "parallelstage", .make = &make<ParallelStage>},
    {.element = "podset", .make = &make<PodSet>},
    {.element = "railbutton", .make = &make<RailButton>},
    {.element = "shockcord", .make = &make<ShockCord>},
    {.element = "stage", .make = &make<AxialStage>},
    {.element = "streamer", .make = &make<Streamer>},
    {.element = "transition", .make = &make<Transition>},
    {.element = "trapezoidfinset", .make = &make<TrapezoidFinSet>},
    {.element = "tubecoupler", .make = &make<TubeCoupler>},
    {.element = "tubefinset", .make = &make<TubeFinSet>},
}};

/// The element names of kConstructors, in its order.
constexpr std::array<std::string_view, kConstructors.size()> kComponentElements = [] {
    std::array<std::string_view, kConstructors.size()> names{};
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        names.at(i) = kConstructors.at(i).element;
    }
    return names;
}();

static_assert(std::ranges::is_sorted(kComponentElements));
static_assert(std::ranges::adjacent_find(kComponentElements) == kComponentElements.end());

// ========================================================================= the setter table

using SetterPtr = std::unique_ptr<const Setter>;

/// Java's Math.PI / 180.0: the multiplier of a parameter that a file holds in degrees.
constexpr double kDegrees = std::numbers::pi / 180.0;

/// DocumentConfig.setters while it is filled: the keys "Class:element" with their setters; a
/// null setter is Java's null entry, the refusal of the element.
class SetterTable
{
public:
    using Entries = std::map<std::string, SetterPtr, std::less<>>;

    /// setters.put(key, setter). A key that is there already is a mistake of the table.
    void put(std::string_view key, SetterPtr setter)
    {
        QTROCKET_ASSERT(setter != nullptr);
        const bool inserted = m_entries.emplace(std::string(key), std::move(setter)).second;
        QTROCKET_ASSERT(inserted);
    }

    /// setters.put(key, null): the element is not allowed for the class of @p key.
    void refuse(std::string_view key)
    {
        const bool inserted = m_entries.emplace(std::string(key), nullptr).second;
        QTROCKET_ASSERT(inserted);
    }

    [[nodiscard]] const Entries& entries() const noexcept { return m_entries; }

private:
    Entries m_entries;
};

/// @p component as the class C of a setter's key. A component of another class is a mistake of
/// whoever chose the setter: the walk of findSetter() cannot bring it about (Java: the
/// IllegalArgumentException of Method.invoke(), made a BugException).
template <class C>
[[nodiscard]] C& as(RocketComponent& component)
{
    auto* const typed = dynamic_cast<C*>(&component);
    if (typed == nullptr)
    {
        bug(
            std::format("a setter of the .ork loader was applied to a {}, which is not of the "
                        "class of its key",
                        className(component.kind())));
    }
    return *typed;
}

/// A StringSetter for a method that takes any text (Java: new StringSetter(method)).
template <class Set>
[[nodiscard]] SetterPtr textSetter(Set set)
{
    return std::make_unique<StringSetter>(
        [set](RocketComponent& c, std::string_view v) -> Result<void> {
            set(c, v);
            return {};
        });
}

/// RocketComponent.setID(String): UUID.fromString(), whose IllegalArgumentException leaves the
/// loader and fails the load ("Exception loading stream: Invalid UUID string: x").
[[nodiscard]] Result<void> setId(RocketComponent& component, std::string_view text)
{
    if (const Result<void> result = component.setId(text); !result.has_value())
    {
        return fail(ErrorCode::INVALID_ARGUMENT, result.error().message);
    }
    return {};
}

// One function per class of the Java source, in its order, each entry at its place. An entry
// that waits for a setter class of part R2 is a HOOK(R2) line.

void addRocketComponentSetters(SetterTable& table)
{
    table.put("RocketComponent:name",
              textSetter([](RocketComponent& c, std::string_view v) { c.setName(v); }));
    table.put("RocketComponent:id", std::make_unique<StringSetter>(&setId));
    table.put(
        "RocketComponent:color",
        std::make_unique<ColorSetter>([](RocketComponent& c, const Color& v) { c.setColor(v); }));
    table.put(
        "RocketComponent:linestyle",
        std::make_unique<EnumSetter>(&lineStyleFromOrkName,
                                     [](RocketComponent& c, LineStyle v) { c.setLineStyle(v); }));
    // HOOK(R2): "RocketComponent:position", new AxialPositionSetter()
    // HOOK(R2): "RocketComponent:axialoffset", new AxialPositionSetter()
    table.put("RocketComponent:overridemass",
              std::make_unique<OverrideSetter>(
                  [](RocketComponent& c, double v) { c.setOverrideMass(v); },
                  [](RocketComponent& c, bool v) { c.setMassOverridden(v); }));
    table.put(
        "RocketComponent:overridecg",
        std::make_unique<OverrideSetter>([](RocketComponent& c, double v) { c.setOverrideCGX(v); },
                                         [](RocketComponent& c, bool v) { c.setCGOverridden(v); }));
    table.put(
        "RocketComponent:overridecd",
        std::make_unique<OverrideSetter>([](RocketComponent& c, double v) { c.setOverrideCD(v); },
                                         [](RocketComponent& c, bool v) { c.setCDOverridden(v); }));
    // The one flag of files up to OpenRocket 15.03, which sets the three that follow.
    table.put("RocketComponent:overridesubcomponents",
              std::make_unique<BooleanSetter>(
                  [](RocketComponent& c, bool v) { c.setSubcomponentsOverridden(v); }));
    table.put("RocketComponent:overridesubcomponentsmass",
              std::make_unique<BooleanSetter>(
                  [](RocketComponent& c, bool v) { c.setSubcomponentsOverriddenMass(v); }));
    table.put("RocketComponent:overridesubcomponentscg",
              std::make_unique<BooleanSetter>(
                  [](RocketComponent& c, bool v) { c.setSubcomponentsOverriddenCG(v); }));
    table.put("RocketComponent:overridesubcomponentscd",
              std::make_unique<BooleanSetter>(
                  [](RocketComponent& c, bool v) { c.setSubcomponentsOverriddenCD(v); }));
    table.put("RocketComponent:comment",
              textSetter([](RocketComponent& c, std::string_view v) { c.setComment(v); }));
    // HOOK(R2): "RocketComponent:preset", new ComponentPresetSetter(RocketComponent.loadPreset)
}

void addExternalComponentSetters(SetterTable& table)
{
    table.put("ExternalComponent:finish",
              std::make_unique<EnumSetter>(&finishFromOrkName, [](RocketComponent& c, Finish v) {
                  as<ExternalComponent>(c).setFinish(v);
              }));
    // HOOK(R2): "ExternalComponent:material", new MaterialSetter(setMaterial, Material.Type.BULK)
}

void addBodyComponentSetters(SetterTable& table)
{
    // Java's BodyComponent is folded into SymmetricComponent here.
    table.put("BodyComponent:length",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<SymmetricComponent>(c).setLength(v); }));
}

void addBodyTubeSetters(SetterTable& table)
{
    table.put(
        "BodyTube:radius",
        std::make_unique<DoubleSetter>(
            [](RocketComponent& c, double v) { as<BodyTube>(c).setOuterRadius(v); }, "auto", ' ',
            [](RocketComponent& c, bool v) { as<BodyTube>(c).setOuterRadiusAutomatic(v); }));
}

void addParallelStageSetters(SetterTable& table)
{
    table.put("ParallelStage:instancecount",
              std::make_unique<IntSetter>(
                  [](RocketComponent& c, int v) { as<ParallelStage>(c).setInstanceCount(v); },
                  DocumentConfig::kMaxCount));
    // HOOK(R2): "ParallelStage:angleoffset", new AnglePositionSetter()
    // HOOK(R2): "ParallelStage:radiusoffset", new RadiusPositionSetter()
}

void addSymmetricComponentSetters(SetterTable& table)
{
    table.put(
        "SymmetricComponent:thickness",
        std::make_unique<DoubleSetter>(
            [](RocketComponent& c, double v) { as<SymmetricComponent>(c).setThickness(v, false); },
            "filled", [](RocketComponent& c, bool v) { as<SymmetricComponent>(c).setFilled(v); }));
}

void addLaunchLugSetters(SetterTable& table)
{
    table.put("LaunchLug:instancecount",
              std::make_unique<IntSetter>(
                  [](RocketComponent& c, int v) { as<LaunchLug>(c).setInstanceCount(v); },
                  DocumentConfig::kMaxCount));
    table.put("LaunchLug:instanceseparation",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<LaunchLug>(c).setInstanceSeparation(v); }));
    table.put(
        "LaunchLug:radialdirection",
        std::make_unique<DoubleSetter>(
            [](RocketComponent& c, double v) { as<LaunchLug>(c).setAngleOffset(v); }, kDegrees));
    // HOOK(R2): "LaunchLug:angleoffset", new AnglePositionSetter()
    table.put("LaunchLug:radius", std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
                  as<LaunchLug>(c).setOuterRadius(v);
              }));
    table.put("LaunchLug:length", std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
                  as<LaunchLug>(c).setLength(v);
              }));
    table.put("LaunchLug:thickness",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<LaunchLug>(c).setThickness(v); }));
}

void addRailButtonSetters(SetterTable& table)
{
    table.put("RailButton:instancecount",
              std::make_unique<IntSetter>(
                  [](RocketComponent& c, int v) { as<RailButton>(c).setInstanceCount(v); },
                  DocumentConfig::kMaxCount));
    table.put("RailButton:instanceseparation",
              std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
                  as<RailButton>(c).setInstanceSeparation(v);
              }));
    // HOOK(R2): "RailButton:angleoffset", new AnglePositionSetter()
    table.put("RailButton:height", std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
                  as<RailButton>(c).setTotalHeight(v);
              }));
    table.put("RailButton:baseheight",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<RailButton>(c).setBaseHeight(v); }));
    table.put("RailButton:flangeheight",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<RailButton>(c).setFlangeHeight(v); }));
    table.put("RailButton:screwheight",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<RailButton>(c).setScrewHeight(v); }));
    table.put("RailButton:outerdiameter",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<RailButton>(c).setOuterDiameter(v); }));
    table.put("RailButton:innerdiameter",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<RailButton>(c).setInnerDiameter(v); }));
}

void addTransitionSetters(SetterTable& table)
{
    table.put("Transition:shape",
              std::make_unique<EnumSetter>(&transitionShapeFromOrkName,
                                           [](RocketComponent& c, TransitionShape v) {
                                               as<Transition>(c).setShapeType(v);
                                           }));
    table.put("Transition:shapeclipped",
              std::make_unique<BooleanSetter>(
                  [](RocketComponent& c, bool v) { as<Transition>(c).setClipped(v); }));
    table.put("Transition:shapeparameter",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<Transition>(c).setShapeParameter(v); }));

    table.put(
        "Transition:foreradius",
        std::make_unique<DoubleSetter>(
            [](RocketComponent& c, double v) { as<Transition>(c).setForeRadius(v, false); }, "auto",
            ' ', [](RocketComponent& c, bool v) { as<Transition>(c).setForeRadiusAutomatic(v); }));
    table.put(
        "Transition:aftradius",
        std::make_unique<DoubleSetter>(
            [](RocketComponent& c, double v) { as<Transition>(c).setAftRadius(v, false); }, "auto",
            ' ', [](RocketComponent& c, bool v) { as<Transition>(c).setAftRadiusAutomatic(v); }));

    table.put("Transition:foreshoulderradius",
              std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
                  as<Transition>(c).setForeShoulderRadius(v, false);
              }));
    table.put("Transition:foreshoulderlength",
              std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
                  as<Transition>(c).setForeShoulderLength(v);
              }));
    table.put("Transition:foreshoulderthickness",
              std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
                  as<Transition>(c).setForeShoulderThickness(v);
              }));
    table.put("Transition:foreshouldercapped",
              std::make_unique<BooleanSetter>(
                  [](RocketComponent& c, bool v) { as<Transition>(c).setForeShoulderCapped(v); }));

    table.put("Transition:aftshoulderradius",
              std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
                  as<Transition>(c).setAftShoulderRadius(v, false);
              }));
    table.put("Transition:aftshoulderlength",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<Transition>(c).setAftShoulderLength(v); }));
    table.put("Transition:aftshoulderthickness",
              std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
                  as<Transition>(c).setAftShoulderThickness(v);
              }));
    table.put("Transition:aftshouldercapped",
              std::make_unique<BooleanSetter>(
                  [](RocketComponent& c, bool v) { as<Transition>(c).setAftShoulderCapped(v); }));
}

void addNoseConeSetters(SetterTable& table)
{
    table.put("NoseCone:isflipped", std::make_unique<BooleanSetter>([](RocketComponent& c, bool v) {
                  as<NoseCone>(c).setFlipped(v, false);
              }));
    // NoseCone - disable disallowed elements
    table.refuse("NoseCone:foreradius");
    table.refuse("NoseCone:foreshoulderradius");
    table.refuse("NoseCone:foreshoulderlength");
    table.refuse("NoseCone:foreshoulderthickness");
    table.refuse("NoseCone:foreshouldercapped");
}

void addFinSetSetters(SetterTable& table)
{
    // FinSet::setFinCount() bounds the count itself (1 to 8), so these two have no maximum.
    table.put("FinSet:fincount", std::make_unique<IntSetter>([](RocketComponent& c, int v) {
                  as<FinSet>(c).setFinCount(v);
              }));
    table.put("FinSet:instancecount", std::make_unique<IntSetter>([](RocketComponent& c, int v) {
                  as<FinSet>(c).setInstanceCount(v);
              }));
    table.put(
        "FinSet:rotation",
        std::make_unique<DoubleSetter>(
            [](RocketComponent& c, double v) { as<FinSet>(c).setBaseRotation(v); }, kDegrees));
    // HOOK(R2): "FinSet:angleoffset", new AnglePositionSetter()
    // HOOK(R2): "FinSet:radiusoffset", new RadiusPositionSetter()
    table.put("FinSet:thickness", std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
                  as<FinSet>(c).setThickness(v);
              }));
    table.put("FinSet:crosssection",
              std::make_unique<EnumSetter>(
                  &finCrossSectionFromOrkName,
                  [](RocketComponent& c, FinCrossSection v) { as<FinSet>(c).setCrossSection(v); }));
    table.put("FinSet:cant",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<FinSet>(c).setCantAngle(v); }, kDegrees));
    table.put("FinSet:tabheight", std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
                  as<FinSet>(c).setTabHeight(v);
              }));
    table.put("FinSet:tablength", std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
                  as<FinSet>(c).setTabLength(v);
              }));
    // HOOK(R2): "FinSet:tabposition", new FinTabPositionSetter()
    table.put("FinSet:filletradius",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<FinSet>(c).setFilletRadius(v); }));
    // HOOK(R2): "FinSet:filletmaterial", new MaterialSetter(setFilletMaterial, Material.Type.BULK)
}

void addTrapezoidFinSetSetters(SetterTable& table)
{
    table.put("TrapezoidFinSet:rootchord",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<TrapezoidFinSet>(c).setRootChord(v); }));
    table.put("TrapezoidFinSet:tipchord",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<TrapezoidFinSet>(c).setTipChord(v); }));
    table.put("TrapezoidFinSet:sweeplength",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<TrapezoidFinSet>(c).setSweep(v); }));
    table.put("TrapezoidFinSet:height",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<TrapezoidFinSet>(c).setHeight(v); }));
}

void addEllipticalFinSetSetters(SetterTable& table)
{
    table.put("EllipticalFinSet:rootchord",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<EllipticalFinSet>(c).setLength(v); }));
    table.put("EllipticalFinSet:height",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<EllipticalFinSet>(c).setHeight(v); }));
    // FreeformFinSet points handled as a special handler
}

void addTubeFinSetSetters(SetterTable& table)
{
    // TubeFinSet::setFinCount() bounds the count itself (1 to 8): no maximum here either.
    table.put("TubeFinSet:fincount", std::make_unique<IntSetter>([](RocketComponent& c, int v) {
                  as<TubeFinSet>(c).setFinCount(v);
              }));
    table.put(
        "TubeFinSet:rotation",
        std::make_unique<DoubleSetter>(
            [](RocketComponent& c, double v) { as<TubeFinSet>(c).setBaseRotation(v); }, kDegrees));
    table.put("TubeFinSet:thickness",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<TubeFinSet>(c).setThickness(v); }));
    table.put("TubeFinSet:length", std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
                  as<TubeFinSet>(c).setLength(v);
              }));
    table.put(
        "TubeFinSet:radius",
        std::make_unique<DoubleSetter>(
            [](RocketComponent& c, double v) { as<TubeFinSet>(c).setOuterRadius(v); }, "auto",
            [](RocketComponent& c, bool v) { as<TubeFinSet>(c).setOuterRadiusAutomatic(v); }));
    table.put("TubeFinSet:instancecount",
              std::make_unique<IntSetter>(
                  [](RocketComponent& c, int v) { as<TubeFinSet>(c).setInstanceCount(v); }));
    // HOOK(R2): "TubeFinSet:angleoffset", new AnglePositionSetter()
    // HOOK(R2): "TubeFinSet:radiusoffset", new RadiusPositionSetter()
}

// InternalComponent - nothing

void addStructuralComponentSetters(SetterTable& /*table*/)
{
    // HOOK(R2): "StructuralComponent:material", new MaterialSetter(setMaterial, Material.Type.BULK)
}

void addRingComponentSetters(SetterTable& table)
{
    table.put("RingComponent:length",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<RingComponent>(c).setLength(v); }));
    table.put("RingComponent:radialposition",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<RingComponent>(c).setRadialPosition(v); }));
    table.put("RingComponent:radialdirection",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<RingComponent>(c).setRadialDirection(v); },
                  kDegrees));
}

void addThicknessRingComponentSetters(SetterTable& table)
{
    // ThicknessRingComponent - radius on separate components due to differing automatics
    table.put("ThicknessRingComponent:thickness",
              std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
                  as<ThicknessRingComponent>(c).setThickness(v);
              }));
}

void addEngineBlockSetters(SetterTable& table)
{
    table.put(
        "EngineBlock:outerradius",
        std::make_unique<DoubleSetter>(
            [](RocketComponent& c, double v) { as<EngineBlock>(c).setOuterRadius(v); }, "auto",
            [](RocketComponent& c, bool v) { as<EngineBlock>(c).setOuterRadiusAutomatic(v); }));
}

void addTubeCouplerSetters(SetterTable& table)
{
    table.put(
        "TubeCoupler:outerradius",
        std::make_unique<DoubleSetter>(
            [](RocketComponent& c, double v) { as<TubeCoupler>(c).setOuterRadius(v); }, "auto",
            [](RocketComponent& c, bool v) { as<TubeCoupler>(c).setOuterRadiusAutomatic(v); }));
}

void addInnerTubeSetters(SetterTable& table)
{
    table.put("InnerTube:outerradius",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<InnerTube>(c).setOuterRadius(v); }));
    // HOOK(R2): "InnerTube:clusterconfiguration", new ClusterConfigurationSetter()
    table.put("InnerTube:clusterscale",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<InnerTube>(c).setClusterScale(v); }));
    table.put("InnerTube:clusterrotation",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<InnerTube>(c).setClusterRotation(v); },
                  kDegrees));
}

void addRadiusRingComponentSetters(SetterTable& table)
{
    table.put("RadiusRingComponent:instancecount",
              std::make_unique<IntSetter>(
                  [](RocketComponent& c, int v) { as<RadiusRingComponent>(c).setInstanceCount(v); },
                  DocumentConfig::kMaxCount));
    table.put("RadiusRingComponent:instanceseparation",
              std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
                  as<RadiusRingComponent>(c).setInstanceSeparation(v);
              }));

    // Java has this entry under "Bulkhead": a centering ring has an entry of its own, so of
    // the two classes below RadiusRingComponent only a bulkhead comes here, and its
    // setInnerRadius() does nothing.
    table.put("RadiusRingComponent:innerradius",
              std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
                  as<RadiusRingComponent>(c).setInnerRadius(v);
              }));
}

void addBulkheadSetters(SetterTable& table)
{
    table.put("Bulkhead:outerradius",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<Bulkhead>(c).setOuterRadius(v); }, "auto",
                  [](RocketComponent& c, bool v) { as<Bulkhead>(c).setOuterRadiusAutomatic(v); }));
}

void addCenteringRingSetters(SetterTable& table)
{
    table.put(
        "CenteringRing:innerradius",
        std::make_unique<DoubleSetter>(
            [](RocketComponent& c, double v) { as<CenteringRing>(c).setInnerRadius(v); }, "auto",
            [](RocketComponent& c, bool v) { as<CenteringRing>(c).setInnerRadiusAutomatic(v); }));
    table.put(
        "CenteringRing:outerradius",
        std::make_unique<DoubleSetter>(
            [](RocketComponent& c, double v) { as<CenteringRing>(c).setOuterRadius(v); }, "auto",
            [](RocketComponent& c, bool v) { as<CenteringRing>(c).setOuterRadiusAutomatic(v); }));
}

void addMassObjectSetters(SetterTable& table)
{
    table.put("MassObject:packedlength",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<MassObject>(c).setLength(v); }));
    table.put("MassObject:packedradius",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<MassObject>(c).setRadius(v); }, "auto", ' ',
                  [](RocketComponent& c, bool v) { as<MassObject>(c).setRadiusAutomatic(v); }));
    table.put("MassObject:radialposition",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<MassObject>(c).setRadialPosition(v); }));
    table.put("MassObject:radialdirection",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<MassObject>(c).setRadialDirection(v); },
                  kDegrees));
}

void addMassComponentSetters(SetterTable& table)
{
    table.put("MassComponent:mass",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<MassComponent>(c).setComponentMass(v); }));
    table.put(
        "MassComponent:masscomponenttype",
        std::make_unique<EnumSetter>(&massComponentTypeFromOrkName,
                                     [](RocketComponent& c, MassComponent::MassComponentType v) {
                                         as<MassComponent>(c).setMassComponentType(v);
                                     }));
}

void addShockCordSetters(SetterTable& table)
{
    table.put("ShockCord:cordlength",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<ShockCord>(c).setCordLength(v); }, "auto",
                  [](RocketComponent& c, bool v) { as<ShockCord>(c).setCordLengthAutomatic(v); }));
    // HOOK(R2): "ShockCord:material", new MaterialSetter(setMaterial, Material.Type.LINE)
}

void addRecoveryDeviceSetters(SetterTable& table)
{
    using DeployEvent = DeploymentConfiguration::DeployEvent;

    table.put("RecoveryDevice:cd",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<RecoveryDevice>(c).setCD(v); }, "auto",
                  [](RocketComponent& c, bool v) { as<RecoveryDevice>(c).setCDAutomatic(v); }));
    // The next three set the default of the device's deployment configurations (Java: the
    // setter's form with a getter of the parameter set).
    table.put(
        "RecoveryDevice:deployevent",
        std::make_unique<EnumSetter>(
            &deployEventFromOrkName, [](RocketComponent& c, DeployEvent v) {
                as<RecoveryDevice>(c).getDeploymentConfigurations().getDefault().setDeployEvent(v);
            }));
    table.put(
        "RecoveryDevice:deployaltitude",
        std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
            as<RecoveryDevice>(c).getDeploymentConfigurations().getDefault().setDeployAltitude(v);
        }));
    table.put(
        "RecoveryDevice:deploydelay",
        std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
            as<RecoveryDevice>(c).getDeploymentConfigurations().getDefault().setDeployDelay(v);
        }));
    // HOOK(R2): "RecoveryDevice:material", new MaterialSetter(setMaterial, Material.Type.SURFACE)
    table.put("RecoveryDevice:isdrogue",
              std::make_unique<BooleanSetter>(
                  [](RocketComponent& c, bool v) { as<RecoveryDevice>(c).setDrogue(v); }));
}

void addParachuteSetters(SetterTable& table)
{
    table.put("Parachute:diameter",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<Parachute>(c).setDiameter(v); }));
    table.put("Parachute:linecount",
              std::make_unique<IntSetter>(
                  [](RocketComponent& c, int v) { as<Parachute>(c).setLineCount(v); },
                  DocumentConfig::kMaxCount));
    table.put("Parachute:linelength",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<Parachute>(c).setLineLength(v); }, "auto",
                  [](RocketComponent& c, bool v) { as<Parachute>(c).setLineLengthAutomatic(v); }));
    // HOOK(R2): "Parachute:linematerial", new MaterialSetter(setLineMaterial, Material.Type.LINE)
    // HOOK(R2): "Parachute:preset", new ComponentPresetSetter(Parachute.loadPreset, false)
}

void addPodSetSetters(SetterTable& table)
{
    table.put("PodSet:instancecount",
              std::make_unique<IntSetter>(
                  [](RocketComponent& c, int v) { as<PodSet>(c).setInstanceCount(v); },
                  DocumentConfig::kMaxCount));
    // HOOK(R2): "PodSet:radiusoffset", new RadiusPositionSetter()
    // HOOK(R2): "PodSet:angleoffset", new AnglePositionSetter()
}

void addStreamerSetters(SetterTable& table)
{
    table.put("Streamer:striplength",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<Streamer>(c).setStripLength(v); }));
    table.put("Streamer:stripwidth",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<Streamer>(c).setStripWidth(v); }));
}

void addRocketSetters(SetterTable& table)
{
    // <motorconfiguration> handled by separate handler
    table.put("Rocket:referencetype",
              std::make_unique<EnumSetter>(
                  &referenceTypeFromOrkName,
                  [](RocketComponent& c, ReferenceType v) { as<Rocket>(c).setReferenceType(v); }));
    table.put("Rocket:customreference",
              std::make_unique<DoubleSetter>(
                  [](RocketComponent& c, double v) { as<Rocket>(c).setCustomReferenceLength(v); }));
    table.put("Rocket:designer", textSetter([](RocketComponent& c, std::string_view v) {
                  as<Rocket>(c).setDesigner(v);
              }));
    table.put("Rocket:revision", textSetter([](RocketComponent& c, std::string_view v) {
                  as<Rocket>(c).setRevision(v);
              }));
    table.put(
        "Rocket:designtype",
        std::make_unique<EnumSetter>(&designTypeFromOrkName, [](RocketComponent& c, DesignType v) {
            as<Rocket>(c).setDesignType(v);
        }));
    table.put("Rocket:kitname", textSetter([](RocketComponent& c, std::string_view v) {
                  as<Rocket>(c).setKitName(v);
              }));
}

void addAxialStageSetters(SetterTable& table)
{
    using SeparationEvent = StageSeparationConfiguration::SeparationEvent;

    // All three set the default of the stage's separation configurations.
    table.put(
        "AxialStage:separationevent",
        std::make_unique<EnumSetter>(
            &separationEventFromOrkName, [](RocketComponent& c, SeparationEvent v) {
                as<AxialStage>(c).getSeparationConfigurations().getDefault().setSeparationEvent(v);
            }));
    table.put(
        "AxialStage:separationaltitude",
        std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
            as<AxialStage>(c).getSeparationConfigurations().getDefault().setSeparationAltitude(v);
        }));
    table.put(
        "AxialStage:separationdelay",
        std::make_unique<DoubleSetter>([](RocketComponent& c, double v) {
            as<AxialStage>(c).getSeparationConfigurations().getDefault().setSeparationDelay(v);
        }));
}

/// DocumentConfig.setters, filled in the order of the Java source.
[[nodiscard]] SetterTable buildSetterTable()
{
    SetterTable table;
    addRocketComponentSetters(table);
    addExternalComponentSetters(table);
    addBodyComponentSetters(table);
    addBodyTubeSetters(table);
    addParallelStageSetters(table);
    addSymmetricComponentSetters(table);
    addLaunchLugSetters(table);
    addRailButtonSetters(table);
    addTransitionSetters(table);
    addNoseConeSetters(table);
    addFinSetSetters(table);
    addTrapezoidFinSetSetters(table);
    addEllipticalFinSetSetters(table);
    addTubeFinSetSetters(table);
    addStructuralComponentSetters(table);
    addRingComponentSetters(table);
    addThicknessRingComponentSetters(table);
    addEngineBlockSetters(table);
    addTubeCouplerSetters(table);
    addInnerTubeSetters(table);
    addRadiusRingComponentSetters(table);
    addBulkheadSetters(table);
    addCenteringRingSetters(table);
    addMassObjectSetters(table);
    addMassComponentSetters(table);
    addShockCordSetters(table);
    addRecoveryDeviceSetters(table);
    addParachuteSetters(table);
    addPodSetSetters(table);
    addStreamerSetters(table);
    addRocketSetters(table);
    addAxialStageSetters(table);
    return table;
}

/// The setter table, made at the first use and never changed.
[[nodiscard]] const SetterTable::Entries& setterEntries()
{
    static const SetterTable kTable = buildSetterTable();
    return kTable.entries();
}

/// The keys of the entries that have a setter (@p withSetter) or that refuse their element.
[[nodiscard]] std::vector<std::string_view> keysOf(bool withSetter)
{
    std::vector<std::string_view> keys;
    for (const auto& [key, setter] : setterEntries())
    {
        if ((setter != nullptr) == withSetter)
        {
            keys.emplace_back(key);
        }
    }
    return keys;
}

}  // namespace

std::span<const std::string_view> DocumentConfig::componentElements() noexcept
{
    return kComponentElements;
}

std::unique_ptr<RocketComponent> DocumentConfig::createComponent(std::string_view element)
{
    for (const ComponentConstructor& constructor : kConstructors)
    {
        if (constructor.element == element)
        {
            return constructor.make();
        }
    }
    return nullptr;
}

DocumentConfig::SetterLookup DocumentConfig::findSetter(ComponentKind    kind,
                                                        std::string_view element)
{
    const SetterTable::Entries& entries = setterEntries();
    // for (c = component.getClass(); c != null; c = c.getSuperclass())
    for (const std::string_view owner : componentClassChain(kind))
    {
        const auto entry = entries.find(std::format("{}:{}", owner, element));
        if (entry != entries.end())
        {
            // A setter, or a key that exists but is null: either ends the search.
            return SetterLookup{.setter = entry->second.get(), .owner = owner};
        }
    }
    return SetterLookup{};
}

std::vector<std::string_view> DocumentConfig::setterKeys()
{
    return keysOf(true);
}

std::vector<std::string_view> DocumentConfig::refusedKeys()
{
    return keysOf(false);
}

std::optional<std::string_view> DocumentConfig::attribute(
    const ElementHandler::Attributes& attributes, std::string_view name)
{
    const auto found = attributes.find(name);
    if (found == attributes.end())
    {
        return std::nullopt;
    }
    return std::string_view(found->second);
}

bool DocumentConfig::isSupportedVersion(std::string_view version) noexcept
{
    return std::ranges::find(kSupportedVersions, version) != kSupportedVersions.end();
}

namespace
{

/// Whether FloatingDecimal.readJavaFormatString() fails @p trimmed, a text that is no number,
/// with "multiple points": it meets a second point in the digits and points the number starts
/// with, before it looks at anything else.
[[nodiscard]] bool hasMultiplePoints(std::string_view trimmed) noexcept
{
    if (trimmed.starts_with('+') || trimmed.starts_with('-'))
    {
        trimmed.remove_prefix(1);
    }
    // "NaN", "Infinity" and a hexadecimal number are told by their start and read apart.
    if (trimmed.starts_with('N') || trimmed.starts_with('I') || trimmed.starts_with("0x") ||
        trimmed.starts_with("0X"))
    {
        return false;
    }
    bool pointSeen = false;
    for (const char c : trimmed)
    {
        if (c == '.')
        {
            if (pointSeen)
            {
                return true;
            }
            pointSeen = true;
        }
        else if (c < '0' || c > '9')
        {
            break;
        }
    }
    return false;
}

}  // namespace

Result<double> DocumentConfig::stringToDouble(std::optional<std::string_view> text)
{
    if (!text.has_value())
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "null string");
    }
    if (Strings::javaEqualsIgnoreCase(*text, "NaN"))
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (Strings::javaEqualsIgnoreCase(*text, "Inf"))
    {
        return std::numeric_limits<double>::infinity();
    }
    if (Strings::javaEqualsIgnoreCase(*text, "-Inf"))
    {
        return -std::numeric_limits<double>::infinity();
    }
    return parseDouble(*text);
}

Result<double> DocumentConfig::parseDouble(std::string_view text)
{
    if (const std::optional<double> value = Strings::javaParseDouble(text))
    {
        return *value;
    }
    // FloatingDecimal.readJavaFormatString(): the text is trimmed before anything is said of it.
    const std::string_view trimmed = Strings::trim(text);
    if (trimmed.empty())
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "empty String");
    }
    if (hasMultiplePoints(trimmed))
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "multiple points");
    }
    return fail(ErrorCode::INVALID_ARGUMENT, std::format("For input string: \"{}\"", trimmed));
}

Result<int> DocumentConfig::parseInt(std::optional<std::string_view> text)
{
    if (!text.has_value())
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "Cannot parse null string");
    }
    if (const std::optional<int> value = Strings::parseInt(*text))
    {
        return *value;
    }
    // NumberFormatException.forInputString(): the text as it is, nothing trimmed.
    return fail(ErrorCode::INVALID_ARGUMENT, std::format("For input string: \"{}\"", *text));
}

std::optional<double> DocumentConfig::parseFiniteDouble(std::string_view text) noexcept
{
    const std::optional<double> value = Strings::javaParseDouble(text);
    if (!value.has_value() || !std::isfinite(*value))
    {
        return std::nullopt;
    }
    return value;
}

}  // namespace QtRocket
