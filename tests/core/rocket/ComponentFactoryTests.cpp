// The components ComponentFactory makes, against what OpenRocket's add buttons make
// (ComponentButton.fireActionPerformed(): the no-argument constructor of the button's class).
//
// OpenRocket has no test of it. The expectations are OpenRocket's (5f164fd0e, JDK 17), printed by
// the dumper of tools/openrocket-component-factory (ComponentFactoryDumper.java) into
// tests/data/component-factory/report.txt, whose blocks the tests read
// (rocket/ComponentFactoryReport.h) and compare with the same values of QtRocket's components,
// written as the dumper writes them (ReportLines):
// - "== defaults": every value Java's constructors give a new component of each of the 21 classes,
//   read through the getters (named as Java names them: getDisplayOrder_side() is
//   getDisplayOrderSide() here, the private fields defaultMaterial and DEFAULT_LINE_MATERIAL are
//   getDefaultMaterial() and getDefaultLineMaterial()), with OpenRocket's start-up defaults
//   loaded (SwingPreferences.loadDefaultComponentMaterials()). A double is written as Java's
//   Double.toString() writes it (Strings::javaDoubleToString(), shortest digits), so equal texts
//   are equal values;
// - "== materials": the materials of a new component of each class with three sets of
//   preferences.
// The design the add buttons build, saved, is file/openrocket/add_buttons_save_tests.cpp's.

#include "QtRocket/rocket/ComponentFactory.h"

#include <array>
#include <concepts>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/EllipticalFinSet.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/RadiusRingComponent.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/RingComponent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/rocket/StructuralComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Strings.h"
#include "rocket/ComponentFactoryReport.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::ComponentFactory;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::DeploymentConfiguration;
using QtRocket::EllipticalFinSet;
using QtRocket::ExternalComponent;
using QtRocket::FinSet;
using QtRocket::InMemoryPreferences;
using QtRocket::InnerTube;
using QtRocket::LaunchLug;
using QtRocket::MassComponent;
using QtRocket::MassObject;
using QtRocket::Material;
using QtRocket::MaterialStorage;
using QtRocket::MotorConfiguration;
using QtRocket::NoseCone;
using QtRocket::Parachute;
using QtRocket::ParallelStage;
using QtRocket::PodSet;
using QtRocket::RadiusRingComponent;
using QtRocket::RailButton;
using QtRocket::RecoveryDevice;
using QtRocket::RingComponent;
using QtRocket::RocketComponent;
using QtRocket::ShockCord;
using QtRocket::StageSeparationConfiguration;
using QtRocket::Streamer;
using QtRocket::StructuralComponent;
using QtRocket::SymmetricComponent;
using QtRocket::Transition;
using QtRocket::TrapezoidFinSet;
using QtRocket::TubeFinSet;
using QtRocket::Test::componentFactoryReportBlock;

/// The kinds of the add buttons, in the order of ComponentAddButtons, which is the report's.
constexpr std::array kButtonKinds{
    ComponentKind::AXIAL_STAGE,       ComponentKind::PARALLEL_STAGE,
    ComponentKind::POD_SET,           ComponentKind::NOSE_CONE,
    ComponentKind::BODY_TUBE,         ComponentKind::TRANSITION,
    ComponentKind::TRAPEZOID_FIN_SET, ComponentKind::ELLIPTICAL_FIN_SET,
    ComponentKind::FREEFORM_FIN_SET,  ComponentKind::TUBE_FIN_SET,
    ComponentKind::RAIL_BUTTON,       ComponentKind::LAUNCH_LUG,
    ComponentKind::INNER_TUBE,        ComponentKind::TUBE_COUPLER,
    ComponentKind::CENTERING_RING,    ComponentKind::BULKHEAD,
    ComponentKind::ENGINE_BLOCK,      ComponentKind::PARACHUTE,
    ComponentKind::STREAMER,          ComponentKind::SHOCK_CORD,
    ComponentKind::MASS_COMPONENT};

/// "<getter> = <value>" lines as ComponentFactoryDumper.show() writes the values.
class ReportLines
{
public:
    /// A string, quoted.
    void text(std::string_view getter, std::string_view value)
    {
        line(getter, std::format("\"{}\"", value));
    }
    /// A double: Double.toString().
    void real(std::string_view getter, double value)
    {
        line(getter, QtRocket::Strings::javaDoubleToString(value));
    }
    void flag(std::string_view getter, bool value) { line(getter, value ? "true" : "false"); }
    template <std::integral Integer>
    void integer(std::string_view getter, Integer value)
    {
        line(getter, std::format("{}", value));
    }
    /// An enum constant: Enum.name().
    void constant(std::string_view getter, std::string_view name) { line(getter, name); }
    void material(std::string_view getter, const Material& value)
    {
        line(getter, value.toStorableString());
    }
    /// An object a new component does not have: "null" (one it has is written otherwise, and
    /// differs).
    void absent(std::string_view getter, bool isAbsent)
    {
        line(getter, isAbsent ? "null" : "(not null)");
    }
    /// Coordinates: "(x,y,z)" each, a blank between two.
    void points(std::string_view getter, const std::vector<Coordinate>& points)
    {
        std::string value;
        for (const Coordinate& point : points)
        {
            value += std::format("{}({},{},{})", value.empty() ? "" : " ",
                                 QtRocket::Strings::javaDoubleToString(point.x),
                                 QtRocket::Strings::javaDoubleToString(point.y),
                                 QtRocket::Strings::javaDoubleToString(point.z));
        }
        line(getter, value);
    }

    [[nodiscard]] const std::string& lines() const noexcept { return m_lines; }

private:
    void line(std::string_view getter, std::string_view value)
    {
        m_lines += std::format("{} = {}\n", getter, value);
    }

    std::string m_lines;
};

// ---- the dumper's groups of getters (ComponentFactoryDumper.COMMON, EXTERNAL, ...)

void describeCommon(const RocketComponent& c, ReportLines& out)
{
    out.text("getName", c.getName());
    out.text("getComponentName", c.getComponentName());
    out.real("getLength", c.getLength());
    out.constant("getAxialMethod", QtRocket::axialMethodName(c.getAxialMethod()));
    out.real("getAxialOffset", c.getAxialOffset());
    out.integer("getInstanceCount", c.getInstanceCount());
    out.integer("getChildCount", c.getChildCount());
    out.flag("isMassOverridden", c.isMassOverridden());
    out.flag("isCGOverridden", c.isCGOverridden());
    out.flag("isCDOverridden", c.isCDOverridden());
    out.real("getOverrideCD", c.getOverrideCD());
    out.flag("isSubcomponentsOverriddenMass", c.isSubcomponentsOverriddenMass());
    out.flag("isSubcomponentsOverriddenCG", c.isSubcomponentsOverriddenCG());
    out.flag("isSubcomponentsOverriddenCD", c.isSubcomponentsOverriddenCD());
    out.text("getComment", c.getComment());
    out.flag("isVisible", c.isVisible());
    out.absent("getColor", !c.getColor().has_value());
    out.absent("getLineStyle", !c.getLineStyle().has_value());
    out.absent("getAppearance", !c.getAppearance().has_value());
    out.absent("getPresetComponent", c.getPresetComponent() == nullptr);
    out.flag("isMotorMount", c.isMotorMount());
    out.integer("getDisplayOrder_side", c.getDisplayOrderSide());
    out.integer("getDisplayOrder_back", c.getDisplayOrderBack());
}

void describeExternal(const ExternalComponent& c, ReportLines& out)
{
    out.material("getMaterial", c.getMaterial());
    out.constant("getFinish", QtRocket::finishName(c.getFinish()));
}

void describeSymmetric(const SymmetricComponent& c, ReportLines& out)
{
    out.real("getThickness", c.getThickness());
    out.flag("isFilled", c.isFilled());
}

void describeTransition(const Transition& c, ReportLines& out)
{
    describeExternal(c, out);
    describeSymmetric(c, out);
    out.constant("getShapeType", QtRocket::transitionShapeName(c.getShapeType()));
    out.real("getShapeParameter", c.getShapeParameter());
    out.flag("isClipped", c.isClipped());
    out.real("getForeRadius", c.getForeRadius());
    out.flag("isForeRadiusAutomatic", c.isForeRadiusAutomatic());
    out.real("getAftRadius", c.getAftRadius());
    out.flag("isAftRadiusAutomatic", c.isAftRadiusAutomatic());
    out.real("getForeShoulderRadius", c.getForeShoulderRadius());
    out.real("getForeShoulderLength", c.getForeShoulderLength());
    out.real("getForeShoulderThickness", c.getForeShoulderThickness());
    out.flag("isForeShoulderCapped", c.isForeShoulderCapped());
    out.real("getAftShoulderRadius", c.getAftShoulderRadius());
    out.real("getAftShoulderLength", c.getAftShoulderLength());
    out.real("getAftShoulderThickness", c.getAftShoulderThickness());
    out.flag("isAftShoulderCapped", c.isAftShoulderCapped());
}

template <class Mount>
void describeMount(const Mount& c, ReportLines& out)
{
    out.real("getMotorOverhang", c.getMotorOverhang());
    const MotorConfiguration& motor = c.getDefaultMotorConfig();
    out.constant("getDefaultMotorConfig.getIgnitionEvent",
                 QtRocket::name(motor.getIgnitionEvent()));
    out.real("getDefaultMotorConfig.getIgnitionDelay", motor.getIgnitionDelay());
    out.real("getDefaultMotorConfig.getEjectionDelay", motor.getEjectionDelay());
    out.integer("getMotorConfigurationSet.size", c.getMotorConfigurationSet().size());
}

void describeFinSet(const FinSet& c, ReportLines& out)
{
    describeExternal(c, out);
    out.integer("getFinCount", c.getFinCount());
    out.real("getBaseRotation", c.getBaseRotation());
    out.real("getAngleOffset", c.getAngleOffset());
    out.constant("getAngleMethod", QtRocket::angleMethodName(c.getAngleMethod()));
    out.constant("getRadiusMethod", QtRocket::radiusMethodName(c.getRadiusMethod()));
    out.real("getRadiusOffset", c.getRadiusOffset());
    out.real("getCantAngle", c.getCantAngle());
    out.real("getThickness", c.getThickness());
    out.constant("getCrossSection", QtRocket::finCrossSectionName(c.getCrossSection()));
    out.real("getTabHeight", c.getTabHeight());
    out.real("getTabLength", c.getTabLength());
    out.real("getTabOffset", c.getTabOffset());
    out.constant("getTabOffsetMethod", QtRocket::axialMethodName(c.getTabOffsetMethod()));
    out.real("getFilletRadius", c.getFilletRadius());
    out.material("getFilletMaterial", c.getFilletMaterial());
}

template <class Assembly>
void describeAssembly(const Assembly& c, ReportLines& out)
{
    out.constant("getAngleMethod", QtRocket::angleMethodName(c.getAngleMethod()));
    out.real("getAngleOffset", c.getAngleOffset());
    out.constant("getRadiusMethod", QtRocket::radiusMethodName(c.getRadiusMethod()));
    out.real("getRadiusOffset", c.getRadiusOffset());
}

void describeStage(const AxialStage& c, ReportLines& out)
{
    out.integer("getStageNumber", c.getStageNumber());
    const StageSeparationConfiguration& separation = c.getSeparationConfigurations().getDefault();
    out.constant("getSeparationConfigurations.getDefault.getSeparationEvent",
                 QtRocket::separationEventName(separation.getSeparationEvent()));
    out.real("getSeparationConfigurations.getDefault.getSeparationDelay",
             separation.getSeparationDelay());
    out.real("getSeparationConfigurations.getDefault.getSeparationAltitude",
             separation.getSeparationAltitude());
    out.integer("getSeparationConfigurations.size", c.getSeparationConfigurations().size());
}

void describeRing(const RingComponent& c, ReportLines& out)
{
    out.real("getOuterRadius", c.getOuterRadius());
    out.flag("isOuterRadiusAutomatic", c.isOuterRadiusAutomatic());
    out.real("getInnerRadius", c.getInnerRadius());
    out.flag("isInnerRadiusAutomatic", c.isInnerRadiusAutomatic());
    out.real("getThickness", c.getThickness());
    out.real("getRadialPosition", c.getRadialPosition());
    out.real("getRadialDirection", c.getRadialDirection());
    out.material("getMaterial", c.getMaterial());
}

void describeMassObject(const MassObject& c, ReportLines& out)
{
    out.real("getRadius", c.getRadius());
    out.flag("isRadiusAutomatic", c.isRadiusAutomatic());
    out.real("getRadialPosition", c.getRadialPosition());
    out.real("getRadialDirection", c.getRadialDirection());
}

void describeRecoveryDevice(const RecoveryDevice& c, ReportLines& out)
{
    describeMassObject(c, out);
    out.real("getCD", c.getCD());
    out.flag("isCDAutomatic", c.isCDAutomatic());
    out.flag("isDrogue", c.isDrogue());
    out.material("getMaterial", c.getMaterial());
    out.material("field:defaultMaterial", c.getDefaultMaterial());
    const DeploymentConfiguration& deployment = c.getDeploymentConfigurations().getDefault();
    out.constant("getDeploymentConfigurations.getDefault.getDeployEvent",
                 QtRocket::deployEventName(deployment.getDeployEvent()));
    out.real("getDeploymentConfigurations.getDefault.getDeployAltitude",
             deployment.getDeployAltitude());
    out.real("getDeploymentConfigurations.getDefault.getDeployDelay", deployment.getDeployDelay());
    out.integer("getDeploymentConfigurations.size", c.getDeploymentConfigurations().size());
}

// ---- the dumper's classes (ComponentFactoryDumper.asked())

void describeStageOrBoosters(const AxialStage& c, ReportLines& out)
{
    describeStage(c, out);
    if (const auto* const boosters = dynamic_cast<const ParallelStage*>(&c))
    {
        describeAssembly(*boosters, out);
    }
}

void describeNoseCone(const NoseCone& c, ReportLines& out)
{
    describeTransition(c, out);
    out.flag("isFlipped", c.isFlipped());
}

void describeBodyTube(const BodyTube& c, ReportLines& out)
{
    describeExternal(c, out);
    describeSymmetric(c, out);
    out.real("getOuterRadius", c.getOuterRadius());
    out.flag("isOuterRadiusAutomatic", c.isOuterRadiusAutomatic());
    out.real("getForeRadius", c.getForeRadius());
    out.flag("isForeRadiusAutomatic", c.isForeRadiusAutomatic());
    out.real("getAftRadius", c.getAftRadius());
    out.flag("isAftRadiusAutomatic", c.isAftRadiusAutomatic());
    describeMount(c, out);
}

void describeTrapezoidFinSet(const TrapezoidFinSet& c, ReportLines& out)
{
    describeFinSet(c, out);
    out.real("getRootChord", c.getRootChord());
    out.real("getTipChord", c.getTipChord());
    out.real("getHeight", c.getHeight());
    out.real("getSweep", c.getSweep());
    out.points("getFinPoints", c.getFinPoints());
}

void describeEllipticalFinSet(const EllipticalFinSet& c, ReportLines& out)
{
    describeFinSet(c, out);
    // The points of an elliptical fin are cosines and sines: the dumper does not ask for them.
    out.real("getHeight", c.getHeight());
}

void describeFreeformFinSet(const FinSet& c, ReportLines& out)
{
    describeFinSet(c, out);
    out.points("getFinPoints", c.getFinPoints());
}

void describeTubeFinSet(const TubeFinSet& c, ReportLines& out)
{
    describeExternal(c, out);
    out.integer("getFinCount", c.getFinCount());
    out.real("getOuterRadius", c.getOuterRadius());
    out.flag("isOuterRadiusAutomatic", c.isOuterRadiusAutomatic());
    out.real("getThickness", c.getThickness());
    out.real("getBaseRotation", c.getBaseRotation());
    out.real("getAngleOffset", c.getAngleOffset());
    out.constant("getAngleMethod", QtRocket::angleMethodName(c.getAngleMethod()));
    out.constant("getRadiusMethod", QtRocket::radiusMethodName(c.getRadiusMethod()));
    out.real("getRadiusOffset", c.getRadiusOffset());
}

void describeRailButton(const RailButton& c, ReportLines& out)
{
    describeExternal(c, out);
    out.real("getOuterDiameter", c.getOuterDiameter());
    out.real("getInnerDiameter", c.getInnerDiameter());
    out.real("getTotalHeight", c.getTotalHeight());
    out.real("getFlangeHeight", c.getFlangeHeight());
    out.real("getBaseHeight", c.getBaseHeight());
    out.real("getScrewHeight", c.getScrewHeight());
    out.real("getInstanceSeparation", c.getInstanceSeparation());
    out.real("getAngleOffset", c.getAngleOffset());
    out.constant("getAngleMethod", QtRocket::angleMethodName(c.getAngleMethod()));
    out.constant("getRadiusMethod", QtRocket::radiusMethodName(c.getRadiusMethod()));
    out.real("getRadiusOffset", c.getRadiusOffset());
}

void describeLaunchLug(const LaunchLug& c, ReportLines& out)
{
    describeExternal(c, out);
    out.real("getOuterRadius", c.getOuterRadius());
    out.real("getInnerRadius", c.getInnerRadius());
    out.real("getThickness", c.getThickness());
    out.real("getInstanceSeparation", c.getInstanceSeparation());
    out.real("getAngleOffset", c.getAngleOffset());
    out.constant("getAngleMethod", QtRocket::angleMethodName(c.getAngleMethod()));
    out.constant("getRadiusMethod", QtRocket::radiusMethodName(c.getRadiusMethod()));
    out.real("getRadiusOffset", c.getRadiusOffset());
}

void describeInnerTube(const InnerTube& c, ReportLines& out)
{
    describeRing(c, out);
    out.constant("getClusterConfiguration", c.getClusterConfiguration().getXmlName());
    out.real("getClusterScale", c.getClusterScale());
    out.real("getClusterRotation", c.getClusterRotation());
    describeMount(c, out);
}

/// A tube coupler and an engine block: the ring alone; a centering ring and a bulkhead: their
/// separation too.
void describeOtherRing(const RingComponent& c, ReportLines& out)
{
    describeRing(c, out);
    if (const auto* const rings = dynamic_cast<const RadiusRingComponent*>(&c))
    {
        out.real("getInstanceSeparation", rings->getInstanceSeparation());
    }
}

void describeParachute(const Parachute& c, ReportLines& out)
{
    describeRecoveryDevice(c, out);
    out.real("getDiameter", c.getDiameter());
    out.integer("getLineCount", c.getLineCount());
    out.real("getLineLength", c.getLineLength());
    out.flag("isLineLengthAutomatic", c.isLineLengthAutomatic());
    out.material("getLineMaterial", c.getLineMaterial());
    out.material("field:DEFAULT_LINE_MATERIAL", c.getDefaultLineMaterial());
}

void describeStreamer(const Streamer& c, ReportLines& out)
{
    describeRecoveryDevice(c, out);
    out.real("getStripLength", c.getStripLength());
    out.real("getStripWidth", c.getStripWidth());
}

void describeShockCord(const ShockCord& c, ReportLines& out)
{
    describeMassObject(c, out);
    out.real("getCordLength", c.getCordLength());
    out.flag("isCordLengthAutomatic", c.isCordLengthAutomatic());
    out.material("getMaterial", c.getMaterial());
}

void describeMassComponent(const MassComponent& c, ReportLines& out)
{
    describeMassObject(c, out);
    out.real("getComponentMass", c.getComponentMass());
    out.constant("getMassComponentType", QtRocket::massComponentTypeName(c.getMassComponentType()));
}

/// The describe function of @p T applied to @p c, which is a @p T (else a failure).
template <class T>
void describeAs(void (*describe)(const T&, ReportLines&), const RocketComponent& c,
                ReportLines& out)
{
    const auto* const typed = dynamic_cast<const T*>(&c);
    if (typed == nullptr)
    {
        ADD_FAILURE() << "not of the class of its kind: " << c.getComponentName();
        return;
    }
    describe(*typed, out);
}

/// The groups of getters the dumper asks a component of the class of @p c besides COMMON.
void describeClass(const RocketComponent& c, ReportLines& out)
{
    switch (c.kind())
    {
        case ComponentKind::AXIAL_STAGE:
        case ComponentKind::PARALLEL_STAGE:
            describeAs(&describeStageOrBoosters, c, out);
            break;
        case ComponentKind::POD_SET:
            describeAs(&describeAssembly<PodSet>, c, out);
            break;
        case ComponentKind::NOSE_CONE:
            describeAs(&describeNoseCone, c, out);
            break;
        case ComponentKind::BODY_TUBE:
            describeAs(&describeBodyTube, c, out);
            break;
        case ComponentKind::TRANSITION:
            describeAs(&describeTransition, c, out);
            break;
        case ComponentKind::TRAPEZOID_FIN_SET:
            describeAs(&describeTrapezoidFinSet, c, out);
            break;
        case ComponentKind::ELLIPTICAL_FIN_SET:
            describeAs(&describeEllipticalFinSet, c, out);
            break;
        case ComponentKind::FREEFORM_FIN_SET:
            describeAs(&describeFreeformFinSet, c, out);
            break;
        case ComponentKind::TUBE_FIN_SET:
            describeAs(&describeTubeFinSet, c, out);
            break;
        case ComponentKind::RAIL_BUTTON:
            describeAs(&describeRailButton, c, out);
            break;
        case ComponentKind::LAUNCH_LUG:
            describeAs(&describeLaunchLug, c, out);
            break;
        case ComponentKind::INNER_TUBE:
            describeAs(&describeInnerTube, c, out);
            break;
        case ComponentKind::TUBE_COUPLER:
        case ComponentKind::ENGINE_BLOCK:
        case ComponentKind::CENTERING_RING:
        case ComponentKind::BULKHEAD:
            describeAs(&describeOtherRing, c, out);
            break;
        case ComponentKind::PARACHUTE:
            describeAs(&describeParachute, c, out);
            break;
        case ComponentKind::STREAMER:
            describeAs(&describeStreamer, c, out);
            break;
        case ComponentKind::SHOCK_CORD:
            describeAs(&describeShockCord, c, out);
            break;
        case ComponentKind::MASS_COMPONENT:
            describeAs(&describeMassComponent, c, out);
            break;
        case ComponentKind::ROCKET:
            ADD_FAILURE() << "the dumper asks no rocket";
            break;
    }
}

/// What the report's block "-- <class>" of the section "== defaults" says of @p c.
[[nodiscard]] std::string defaultsAsReported(const RocketComponent& c)
{
    ReportLines out;
    describeCommon(c, out);
    describeClass(c, out);
    return out.lines();
}

/// The line of @p c in a block of the report's section "== materials": its Java class, then
/// each material it has as "<Java getter or field>=<storable string>", in the dumper's order.
[[nodiscard]] std::string materialsAsReported(const RocketComponent& c)
{
    std::string line = std::string(QtRocket::className(c.kind())) + ":";
    const auto  add  = [&line](std::string_view what, const Material& material) {
        line += std::format(" {}={}", what, material.toStorableString());
    };
    if (const auto* const external = dynamic_cast<const ExternalComponent*>(&c))
    {
        add("getMaterial", external->getMaterial());
    }
    if (const auto* const fins = dynamic_cast<const FinSet*>(&c))
    {
        add("getFilletMaterial", fins->getFilletMaterial());
    }
    if (const auto* const structural = dynamic_cast<const StructuralComponent*>(&c))
    {
        add("getMaterial", structural->getMaterial());
    }
    if (const auto* const cord = dynamic_cast<const ShockCord*>(&c))
    {
        add("getMaterial", cord->getMaterial());
    }
    if (const auto* const device = dynamic_cast<const RecoveryDevice*>(&c))
    {
        add("getMaterial", device->getMaterial());
        add("field:defaultMaterial", device->getDefaultMaterial());
    }
    if (const auto* const chute = dynamic_cast<const Parachute*>(&c))
    {
        add("getLineMaterial", chute->getLineMaterial());
        add("field:DEFAULT_LINE_MATERIAL", chute->getDefaultLineMaterial());
    }
    return line + "\n";
}

/// materialsAsReported() of a new component of each button's kind, made with @p preferences.
[[nodiscard]] std::string materialsOfNewComponents(const InMemoryPreferences& preferences,
                                                   const MaterialStorage&     materials)
{
    std::string lines;
    for (const ComponentKind kind : kButtonKinds)
    {
        lines += materialsAsReported(*ComponentFactory::create(kind, preferences, materials));
    }
    return lines;
}

/// materialsAsReported() of each button's kind as createWithBuiltinMaterials() makes it.
[[nodiscard]] std::string materialsOfBareComponents()
{
    std::string lines;
    for (const ComponentKind kind : kButtonKinds)
    {
        lines += materialsAsReported(*ComponentFactory::createWithBuiltinMaterials(kind));
    }
    return lines;
}

/// One default the dumper stores along the class chains (storeChainDefaults()).
struct StoredDefault
{
    std::string_view className;
    Material::Type   type;
    std::string_view material;
};

/// Stores the defaults ComponentFactoryDumper.storeChainDefaults() stores in @p preferences, each
/// taken from @p materials, and returns the report's lines of what was stored.
[[nodiscard]] std::string storeChainDefaults(InMemoryPreferences&   preferences,
                                             const MaterialStorage& materials)
{
    using enum Material::Type;
    constexpr std::array kStored{
        StoredDefault{.className = "ExternalComponent", .type = BULK, .material = "Aluminum"},
        StoredDefault{.className = "Transition", .type = BULK, .material = "Fiberglass"},
        StoredDefault{.className = "RingComponent", .type = BULK, .material = "Kraft phenolic"},
        StoredDefault{.className = "CenteringRing", .type = BULK, .material = "Plywood (birch)"},
        StoredDefault{.className = "RecoveryDevice", .type = SURFACE, .material = "Silk"},
        StoredDefault{.className = "Parachute", .type = SURFACE, .material = "Mylar"},
        StoredDefault{
            .className = "MassObject", .type = LINE, .material = "Tubular nylon (11 mm, 7/16 in)"},
    };
    std::string lines;
    for (const StoredDefault& stored : kStored)
    {
        const std::optional<Material> material =
            materials.findMaterial(stored.type, stored.material);
        QtRocket::setDefaultComponentMaterial(preferences, stored.className, material);
        lines += std::format("stored {} = {}\n", stored.className,
                             material.has_value() ? material->toStorableString() : "none");
    }
    return lines;
}

/// The kinds the factory makes that are not of their kind, have a parent or share an id with
/// another new component; one line each, empty when there is none.
[[nodiscard]] std::string wronglyMadeKinds(const InMemoryPreferences& preferences,
                                           const MaterialStorage&     materials)
{
    std::string wrong;
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        if (!ComponentFactory::isCreatable(kind))
        {
            continue;
        }
        const std::unique_ptr<RocketComponent> bare =
            ComponentFactory::createWithBuiltinMaterials(kind);
        const std::unique_ptr<RocketComponent> made =
            ComponentFactory::create(kind, preferences, materials);
        if (bare->kind() != kind || made->kind() != kind || made->getParent() != nullptr ||
            made->getId() == bare->getId())
        {
            wrong += std::format("{}\n", QtRocket::componentKindName(kind));
        }
    }
    return wrong;
}

/// The kinds whose component createWithBuiltinMaterials() made and applyDefaultMaterials()
/// completed has other materials than create() gives; one line each, empty when there is none.
[[nodiscard]] std::string kindsCompletedOtherwise(const InMemoryPreferences& preferences,
                                                  const MaterialStorage&     materials)
{
    std::string other;
    for (const ComponentKind kind : kButtonKinds)
    {
        const std::unique_ptr<RocketComponent> completed =
            ComponentFactory::createWithBuiltinMaterials(kind);
        ComponentFactory::applyDefaultMaterials(*completed, preferences, materials);
        const std::string created =
            materialsAsReported(*ComponentFactory::create(kind, preferences, materials));
        if (materialsAsReported(*completed) != created)
        {
            other += created;
        }
    }
    return other;
}

/// The built-in materials, and preferences as OpenRocket's start leaves them
/// (loadDefaultComponentMaterials(): "Balsa" for FinSet, "Polystyrene" for NoseCone).
class ComponentFactoryTest : public ::testing::Test
{
protected:
    ComponentFactoryTest()
    {
        QtRocket::addBuiltinMaterials(m_materials);
        QtRocket::loadDefaultComponentMaterials(m_preferences, m_materials);
    }

    InMemoryPreferences m_preferences;
    MaterialStorage     m_materials;
};

// ------------------------------------------------------------------------------- the kinds

// Every kind but the rocket is made, as an object of its class (each with an id of its own),
// which is no child of anything.
TEST_F(ComponentFactoryTest, MakesEveryKindButTheRocket)
{
    EXPECT_EQ(QtRocket::kAllComponentKinds.size(), kButtonKinds.size() + 1);
    EXPECT_EQ(wronglyMadeKinds(m_preferences, m_materials), "");
}

TEST_F(ComponentFactoryTest, MakesNoRocket)
{
    EXPECT_FALSE(ComponentFactory::isCreatable(ComponentKind::ROCKET));
    EXPECT_THROW(
        static_cast<void>(ComponentFactory::createWithBuiltinMaterials(ComponentKind::ROCKET)),
        BugError);
    EXPECT_THROW(static_cast<void>(
                     ComponentFactory::create(ComponentKind::ROCKET, m_preferences, m_materials)),
                 BugError);
}

// -------------------------------------------------------------------------------- the defaults

// Every value Java's constructors give, with OpenRocket's start-up defaults: the report's
// section "== defaults", a block per class.
TEST_F(ComponentFactoryTest, GivesEveryClassOpenRocketsDefaults)
{
    for (const ComponentKind kind : kButtonKinds)
    {
        SCOPED_TRACE(QtRocket::className(kind));
        EXPECT_EQ(defaultsAsReported(*ComponentFactory::create(kind, m_preferences, m_materials)),
                  componentFactoryReportBlock(std::format("-- {}", QtRocket::className(kind))));
    }
}

// ------------------------------------------------------------------------------- the materials

// Without a stored default every material is the built-in fallback, as the bare constructor's:
// Cardboard, Ripstop nylon, the elastic cord, and Delrin for a rail button.
TEST(ComponentFactory, TakesTheBuiltInMaterialsWhenThePreferencesNameNone)
{
    MaterialStorage materials;
    QtRocket::addBuiltinMaterials(materials);
    const InMemoryPreferences preferences;
    const std::string         expected = componentFactoryReportBlock("-- preferences: none stored");
    EXPECT_EQ(materialsOfNewComponents(preferences, materials), expected);
    EXPECT_EQ(materialsOfBareComponents(), expected);
}

// OpenRocket's start-up defaults: Polystyrene for nose cones, Balsa for fins and their fillets.
TEST_F(ComponentFactoryTest, TakesOpenRocketsStartUpDefaults)
{
    EXPECT_EQ(materialsOfNewComponents(m_preferences, m_materials),
              componentFactoryReportBlock("-- preferences: loadDefaultComponentMaterials()"));
}

// Each class takes the nearest default of its chain (a nose cone is a Transition, the rings are
// RingComponents but for the centering ring); a rail button stays Delrin; the canopies are asked
// for RecoveryDevice, so Parachute's Mylar is not seen; the lines of a parachute are asked for
// Parachute, whose default is a surface material, so they keep the built-in line material
// although MassObject further up names a line material, which the shock cord takes.
TEST(ComponentFactory, TakesTheNearestDefaultAlongTheClassChainAsOpenRocket)
{
    MaterialStorage materials;
    QtRocket::addBuiltinMaterials(materials);
    InMemoryPreferences preferences;
    EXPECT_EQ(storeChainDefaults(preferences, materials),
              componentFactoryReportBlock("-- stored along the class chains"));
    EXPECT_EQ(materialsOfNewComponents(preferences, materials),
              componentFactoryReportBlock("-- preferences: defaults along the class chains"));
}

// Every call reads the preferences it is given: nothing is cached.
TEST_F(ComponentFactoryTest, ReadsThePreferencesAtEveryCall)
{
    const std::unique_ptr<RocketComponent> before =
        ComponentFactory::create(ComponentKind::NOSE_CONE, m_preferences, m_materials);
    QtRocket::setDefaultComponentMaterial(m_preferences, "NoseCone", std::nullopt);
    const std::unique_ptr<RocketComponent> after =
        ComponentFactory::create(ComponentKind::NOSE_CONE, m_preferences, m_materials);
    EXPECT_EQ(materialsAsReported(*before),
              "NoseCone: getMaterial=BULK|Polystyrene|1050.0|1.23E9|Plastics\n");
    EXPECT_EQ(materialsAsReported(*after),
              "NoseCone: getMaterial=BULK|Cardboard|680.0|4.0E8|PaperProducts\n");
}

// --------------------------------------------------------------------- applyDefaultMaterials()

// The loader's half: a component the constructor made is completed as create() completes it.
TEST_F(ComponentFactoryTest, CompletesAComponentAsCreateDoes)
{
    EXPECT_EQ(kindsCompletedOtherwise(m_preferences, m_materials), "");
}

// A component in a tree is not new: completing it is a programming error, before anything
// changes.
TEST_F(ComponentFactoryTest, RefusesAComponentInATree)
{
    AxialStage stage;
    NoseCone&  nose  = stage.addChild(std::make_unique<NoseCone>());
    BodyTube&  tube  = stage.addChild(std::make_unique<BodyTube>());
    InnerTube& inner = tube.addChild(std::make_unique<InnerTube>());
    EXPECT_THROW(ComponentFactory::applyDefaultMaterials(nose, m_preferences, m_materials),
                 BugError);
    EXPECT_THROW(ComponentFactory::applyDefaultMaterials(inner, m_preferences, m_materials),
                 BugError);
    EXPECT_EQ(materialsAsReported(nose),
              "NoseCone: getMaterial=BULK|Cardboard|680.0|4.0E8|PaperProducts\n");
}

// The material database must hold the built-in fallbacks, as OpenRocket's always do: a lookup
// that needs one in a database without it is a programming error (getDefaultComponentMaterial()).
TEST(ComponentFactory, NeedsTheBuiltInMaterials)
{
    const MaterialStorage     empty;
    const InMemoryPreferences preferences;
    EXPECT_THROW(
        static_cast<void>(ComponentFactory::create(ComponentKind::BODY_TUBE, preferences, empty)),
        BugError);
    // A kind without a material asks for none.
    EXPECT_NE(ComponentFactory::create(ComponentKind::MASS_COMPONENT, preferences, empty), nullptr);
}

}  // namespace
