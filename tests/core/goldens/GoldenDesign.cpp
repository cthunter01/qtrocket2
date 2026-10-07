#include "goldens/GoldenDesign.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <numbers>
#include <optional>
#include <ostream>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/BarrowmanCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/mass/CMAnalysisEntry.h"
#include "QtRocket/mass/MassCalculator.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/InstanceContext.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/LineInstanceable.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ReferenceType.h"
#include "QtRocket/rocket/RingComponent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AnglePositionable.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenGeometry.h"
#include "goldens/GoldenMismatches.h"
#include "goldens/GoldenWarnings.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Test
{

namespace
{

using nlohmann::json;

/// The comparison collector of the golden tests.
using Mismatches = GoldenMismatches;

// ============================================================================= geometry.json

/// Compares the class, the name and the placement of @p actual with the golden @p expected.
void comparePlacement(Mismatches& m, const json& expected, const RocketComponent& actual)
{
    m.text("type", expected.at("type").get<std::string>(), QtRocket::className(actual.kind()));
    m.text("name", expected.at("name").get<std::string>(), actual.getName());
    m.integer("stageNumber", expected.at("stageNumber").get<int>(), actual.getStageNumber());
    m.relative("length", goldenValue(expected.at("length")), actual.getLength());
    m.text("axialMethod", expected.at("axialMethod").get<std::string>(),
           QtRocket::axialMethodName(actual.getAxialMethod()));
    m.absolute("axialOffset", goldenValue(expected.at("axialOffset")), actual.getAxialOffset());
    m.position("position", goldenCoordinate(expected.at("position")), actual.getPosition());
    m.boolean("isAerodynamic", expected.at("isAerodynamic").get<bool>(), actual.isAerodynamic());
    m.boolean("isMassive", expected.at("isMassive").get<bool>(), actual.isMassive());
}

/// The golden path @p entry holds, "" when it is null.
[[nodiscard]] std::string overriderPath(const json& entry)
{
    return entry.is_null() ? std::string{} : entry.get<std::string>();
}

/// The golden path of @p component, "" for none.
[[nodiscard]] std::string pathOrNone(const RocketComponent* component)
{
    return component == nullptr ? std::string{} : goldenPathOf(*component);
}

/// Compares the mass properties of @p actual, without and with its overrides.
void compareMass(Mismatches& m, const json& expected, const RocketComponent& actual)
{
    m.relative("componentMass", goldenValue(expected.at("componentMass")),
               actual.getComponentMass());
    m.cg("componentCG", goldenCoordinate(expected.at("componentCG")), actual.getComponentCG());
    m.relative("longitudinalUnitInertia", goldenValue(expected.at("longitudinalUnitInertia")),
               actual.getLongitudinalUnitInertia());
    m.relative("rotationalUnitInertia", goldenValue(expected.at("rotationalUnitInertia")),
               actual.getRotationalUnitInertia());

    m.relative("mass", goldenValue(expected.at("mass")), actual.getMass());
    m.relative("sectionMass", goldenValue(expected.at("sectionMass")), actual.getSectionMass());
    m.cg("cg", goldenCoordinate(expected.at("cg")), actual.getCG());
    m.relative("longitudinalInertia", goldenValue(expected.at("longitudinalInertia")),
               actual.getLongitudinalInertia());
    m.relative("rotationalInertia", goldenValue(expected.at("rotationalInertia")),
               actual.getRotationalInertia());

    const json& overrides = expected.at("overrides");
    m.boolean("massOverridden", overrides.at("massOverridden").get<bool>(),
              actual.isMassOverridden());
    if (actual.isMassOverridden() && !overrides.at("overrideMass").is_null())
    {
        m.relative("overrideMass", goldenValue(overrides.at("overrideMass")),
                   actual.getOverrideMass());
    }
    m.boolean("cgOverridden", overrides.at("cgOverridden").get<bool>(), actual.isCGOverridden());
    if (actual.isCGOverridden() && !overrides.at("overrideCGX").is_null())
    {
        m.absolute("overrideCGX", goldenValue(overrides.at("overrideCGX")),
                   actual.getOverrideCGX());
    }
    m.boolean("cdOverridden", overrides.at("cdOverridden").get<bool>(), actual.isCDOverridden());
    if (actual.isCDOverridden() && !overrides.at("overrideCD").is_null())
    {
        m.relative("overrideCD", goldenValue(overrides.at("overrideCD")), actual.getOverrideCD());
    }
    m.boolean("subcomponentsOverriddenMass",
              overrides.at("subcomponentsOverriddenMass").get<bool>(),
              actual.isSubcomponentsOverriddenMass());
    m.boolean("subcomponentsOverriddenCG", overrides.at("subcomponentsOverriddenCG").get<bool>(),
              actual.isSubcomponentsOverriddenCG());
    m.boolean("subcomponentsOverriddenCD", overrides.at("subcomponentsOverriddenCD").get<bool>(),
              actual.isSubcomponentsOverriddenCD());
    m.boolean("cdOverriddenByAncestor", overrides.at("cdOverriddenByAncestor").get<bool>(),
              actual.isCDOverriddenByAncestor());
    // The component whose override covers this one, by its path ("" for none; the golden entry
    // is then null).
    m.text("massOverriddenBy", overriderPath(overrides.at("massOverriddenBy")),
           pathOrNone(actual.getMassOverriddenBy()));
    m.text("cgOverriddenBy", overriderPath(overrides.at("cgOverriddenBy")),
           pathOrNone(actual.getCGOverriddenBy()));
}

/// Compares the instances of @p actual: their count, offsets, angles and locations.
void compareInstances(Mismatches& m, const json& expected, const RocketComponent& actual)
{
    m.integer("instanceCount", expected.at("instanceCount").get<int>(), actual.getInstanceCount());
    m.positions("instanceOffsets", expected.at("instanceOffsets"), actual.getInstanceOffsets());
    m.angles("instanceAngles", expected.at("instanceAngles"), actual.getInstanceAngles());
    m.positions("instanceLocations", expected.at("instanceLocations"),
                actual.getInstanceLocations());
    m.positions("componentLocations", expected.at("componentLocations"),
                actual.getComponentLocations());
    m.positions("componentAngles", expected.at("componentAngles"), actual.getComponentAngles());
}

/// The "details" of a golden component: what the public getters of its Java class return
/// (GeometryDumper lists the getters; an entry exists when the class has the getter). Each
/// comparison reads one entry; unread() then reports the entries nothing compared, so the golden
/// file decides what has to be checked.
class Details
{
public:
    Details(Mismatches& mismatches, const json& details)
      : m_mismatches(&mismatches), m_details(&details)
    {
    }

    /// Whether the golden component has the entry @p key (its Java class has the getter).
    [[nodiscard]] bool has(std::string_view key) const { return m_details->contains(key); }

    /// A volume, an area, a density or a coefficient: within kRelative.
    void relative(std::string_view key, double actual)
    {
        if (const json* expected = read(key))
        {
            m_mismatches->relative(field(key), goldenValue(*expected), actual);
        }
    }

    /// A dimension (a radius, a length, a chord, a wall): a geometry value, within kRelative.
    void length(std::string_view key, double actual) { relative(key, actual); }

    /// An offset, a position or an angle: within kAbsolute.
    void absolute(std::string_view key, double actual)
    {
        if (const json* expected = read(key))
        {
            m_mismatches->absolute(field(key), goldenValue(*expected), actual);
        }
    }

    void text(std::string_view key, std::string_view actual)
    {
        if (const json* expected = read(key))
        {
            m_mismatches->text(field(key), expected->get<std::string>(), actual);
        }
    }

    void boolean(std::string_view key, bool actual)
    {
        if (const json* expected = read(key))
        {
            m_mismatches->boolean(field(key), expected->get<bool>(), actual);
        }
    }

    void integer(std::string_view key, long long actual)
    {
        if (const json* expected = read(key))
        {
            m_mismatches->integer(field(key), expected->get<long long>(), actual);
        }
    }

    /// A material: its name, type and density.
    void material(std::string_view key, const QtRocket::Material& actual)
    {
        if (const json* expected = read(key))
        {
            const std::string name = field(key);
            m_mismatches->text(name + ".name", expected->at("name").get<std::string>(),
                               actual.getName());
            m_mismatches->text(name + ".type", expected->at("type").get<std::string>(),
                               QtRocket::toString(actual.getType()));
            m_mismatches->relative(name + ".density", goldenValue(expected->at("density")),
                                   actual.getDensity());
        }
    }

    /// A bounding box: its corners (an empty box is +-DBL_MAX in both).
    void box(std::string_view key, const BoundingBox& actual)
    {
        if (const json* expected = read(key))
        {
            const std::string name = field(key);
            m_mismatches->position(name + ".min", goldenCoordinate(expected->at("min")),
                                   actual.min());
            m_mismatches->position(name + ".max", goldenCoordinate(expected->at("max")),
                                   actual.max());
        }
    }

    /// A list of points (an outline): as many, each within kAbsolute.
    void points(std::string_view key, std::span<const Coordinate> actual)
    {
        if (const json* expected = read(key))
        {
            m_mismatches->positions(field(key), *expected, actual);
        }
    }

    /// Reports every entry of the golden details that no comparison read.
    void unread() const
    {
        for (const auto& [key, value] : m_details->items())
        {
            if (!m_read.contains(key))
            {
                m_mismatches->note(std::format("details.{}: not compared", key));
            }
        }
    }

private:
    /// The golden entry @p key, now read; null, and a mismatch, when the golden component has
    /// no such entry (the rebuilt component is of a class that the Java one is not).
    [[nodiscard]] const json* read(std::string_view key)
    {
        m_read.emplace(key);
        const auto entry = m_details->find(key);
        if (entry == m_details->end())
        {
            m_mismatches->note(std::format("details.{}: no golden entry", key));
            return nullptr;
        }
        return &*entry;
    }

    [[nodiscard]] static std::string field(std::string_view key)
    {
        return std::format("details.{}", key);
    }

    Mismatches*                        m_mismatches;
    const json*                        m_details;
    std::set<std::string, std::less<>> m_read;
};

/// The details every component has: its radial and angular position and whether it is a motor
/// mount; the angle method of an AnglePositionable and the instance box of a BoxBounded.
void compareCommonDetails(Details& d, const RocketComponent& actual)
{
    d.text("radiusMethod", QtRocket::radiusMethodName(actual.getRadiusMethod()));
    d.absolute("radiusOffset", actual.getRadiusOffset());
    d.absolute("angleOffset", actual.getAngleOffset());
    d.boolean("motorMount", actual.isMotorMount());
    if (const auto* angled = dynamic_cast<const QtRocket::AnglePositionable*>(&actual))
    {
        d.text("angleMethod", QtRocket::angleMethodName(angled->getAngleMethod()));
    }
    if (const auto* boxed = dynamic_cast<const QtRocket::BoxBounded*>(&actual))
    {
        d.box("instanceBoundingBox", boxed->getInstanceBoundingBox());
    }
    if (const auto* line = dynamic_cast<const QtRocket::LineInstanceable*>(&actual))
    {
        d.length("instanceSeparation", line->getInstanceSeparation());
    }
}

/// A nose cone, body tube or transition: its volumes and areas, radii, wall, finish and
/// material; a transition's shape and shoulders; a body tube's mount settings.
void compareBodyDetails(Details& d, const QtRocket::SymmetricComponent& actual)
{
    d.relative("componentVolume", actual.getComponentVolume());
    d.relative("fullVolume", actual.getFullVolume());
    d.relative("componentWetArea", actual.getComponentWetArea());
    d.relative("componentPlanformArea", actual.getComponentPlanformArea());
    d.length("foreRadius", actual.getForeRadius());
    d.length("aftRadius", actual.getAftRadius());
    d.length("maxRadius", actual.getMaxRadius());
    d.length("innerRadius", actual.getInnerRadius());
    d.length("thickness", actual.getThickness());
    d.boolean("filled", actual.isFilled());
    d.text("finish", QtRocket::finishName(actual.getFinish()));
    d.material("material", actual.getMaterial());

    if (const auto* transition = dynamic_cast<const QtRocket::Transition*>(&actual))
    {
        d.text("shapeType", QtRocket::transitionShapeName(transition->getShapeType()));
        d.absolute("shapeParameter", transition->getShapeParameter());
        d.boolean("clipped", transition->isClipped());
        d.length("foreShoulderRadius", transition->getForeShoulderRadius());
        d.length("foreShoulderLength", transition->getForeShoulderLength());
        d.length("foreShoulderThickness", transition->getForeShoulderThickness());
        d.boolean("foreShoulderCapped", transition->isForeShoulderCapped());
        d.length("aftShoulderRadius", transition->getAftShoulderRadius());
        d.length("aftShoulderLength", transition->getAftShoulderLength());
        d.length("aftShoulderThickness", transition->getAftShoulderThickness());
        d.boolean("aftShoulderCapped", transition->isAftShoulderCapped());
    }
    if (const auto* tube = dynamic_cast<const QtRocket::BodyTube*>(&actual))
    {
        d.length("outerRadius", tube->getOuterRadius());
        d.absolute("motorOverhang", tube->getMotorOverhang());
        d.text("clusterConfiguration", tube->getClusterConfiguration().getXmlName());
    }
}

/// An inner tube, engine block, centering ring or coupler: its radii, wall, radial position and
/// material; an inner tube's mount and cluster settings.
void compareRingDetails(Details& d, const QtRocket::RingComponent& actual)
{
    d.length("outerRadius", actual.getOuterRadius());
    d.length("innerRadius", actual.getInnerRadius());
    d.length("thickness", actual.getThickness());
    d.absolute("radialPosition", actual.getRadialPosition());
    d.absolute("radialDirection", actual.getRadialDirection());
    d.material("material", actual.getMaterial());
    if (const auto* tube = dynamic_cast<const QtRocket::InnerTube*>(&actual))
    {
        d.absolute("motorOverhang", tube->getMotorOverhang());
        d.text("clusterConfiguration", tube->getClusterConfiguration().getXmlName());
        d.relative("clusterScale", tube->getClusterScale());
        d.absolute("clusterRotation", tube->getClusterRotation());
    }
}

/// A parachute or a shock cord: its packed radius and radial position, and what the class adds.
void compareMassObjectDetails(Details& d, const QtRocket::MassObject& actual)
{
    d.length("radius", actual.getRadius());
    d.absolute("radialPosition", actual.getRadialPosition());
    d.absolute("radialDirection", actual.getRadialDirection());
    if (const auto* chute = dynamic_cast<const QtRocket::Parachute*>(&actual))
    {
        d.length("diameter", chute->getDiameter());
        d.relative("cd", chute->getCD());
        d.integer("lineCount", chute->getLineCount());
        d.length("lineLength", chute->getLineLength());
        d.material("material", chute->getMaterial());
    }
    if (const auto* cord = dynamic_cast<const QtRocket::ShockCord*>(&actual))
    {
        d.length("cordLength", cord->getCordLength());
        d.material("material", cord->getMaterial());
    }
}

/// A fin set: its fins (count, thickness, cross-section, cant, base rotation), its tab and
/// fillets, its areas and outlines, and the dimensions of a trapezoidal one.
void compareFinSetDetails(Details& d, const QtRocket::FinSet& actual)
{
    d.relative("planformArea", actual.getPlanformArea());
    d.length("thickness", actual.getThickness());
    d.length("bodyRadius", actual.getBodyRadius());
    d.integer("finCount", actual.getFinCount());
    d.absolute("cantAngle", actual.getCantAngle());
    d.absolute("baseRotation", actual.getBaseRotation());
    d.text("crossSection", QtRocket::finCrossSectionName(actual.getCrossSection()));
    d.length("span", actual.getSpan());
    d.length("tabHeight", actual.getTabHeight());
    d.length("tabLength", actual.getTabLength());
    d.absolute("tabOffset", actual.getTabOffset());
    d.length("filletRadius", actual.getFilletRadius());
    d.material("filletMaterial", actual.getFilletMaterial());
    d.points("finPoints", actual.getFinPoints());
    d.points("rootPoints", actual.getRootPoints());
    d.points("tabPoints", actual.getTabPoints());
    if (const auto* trapezoid = dynamic_cast<const QtRocket::TrapezoidFinSet*>(&actual))
    {
        d.length("rootChord", trapezoid->getRootChord());
        d.length("tipChord", trapezoid->getTipChord());
        d.length("height", trapezoid->getHeight());
        d.length("sweep", trapezoid->getSweep());
        d.absolute("sweepAngle", trapezoid->getSweepAngle());
    }
}

/// A fin set or a launch lug: its volume, finish and material, and what the class adds.
void compareExternalDetails(Details& d, const QtRocket::ExternalComponent& actual)
{
    d.relative("componentVolume", actual.getComponentVolume());
    d.text("finish", QtRocket::finishName(actual.getFinish()));
    d.material("material", actual.getMaterial());
    if (const auto* fins = dynamic_cast<const QtRocket::FinSet*>(&actual))
    {
        compareFinSetDetails(d, *fins);
    }
    if (const auto* lug = dynamic_cast<const QtRocket::LaunchLug*>(&actual))
    {
        d.length("outerRadius", lug->getOuterRadius());
        d.length("innerRadius", lug->getInnerRadius());
        d.length("thickness", lug->getThickness());
    }
}

/// Compares the golden details of @p expected with @p actual, by the classes @p actual is of
/// (the C++ counterpart of the harness's reflection), then reports the entries left over.
void compareDetails(Mismatches& m, const json& expected, const RocketComponent& actual)
{
    Details d(m, expected.at("details"));
    compareCommonDetails(d, actual);
    if (const auto* external = dynamic_cast<const QtRocket::ExternalComponent*>(&actual))
    {
        compareExternalDetails(d, *external);
    }
    if (const auto* body = dynamic_cast<const QtRocket::SymmetricComponent*>(&actual))
    {
        compareBodyDetails(d, *body);
    }
    if (const auto* ring = dynamic_cast<const QtRocket::RingComponent*>(&actual))
    {
        compareRingDetails(d, *ring);
    }
    if (const auto* massObject = dynamic_cast<const QtRocket::MassObject*>(&actual))
    {
        compareMassObjectDetails(d, *massObject);
    }
    if (const auto* rocket = dynamic_cast<const Rocket*>(&actual))
    {
        d.text("referenceType", QtRocket::referenceTypeName(rocket->getReferenceType()));
        d.length("customReferenceLength", rocket->getCustomReferenceLength());
        d.boolean("perfectFinish", rocket->isPerfectFinish());
    }
    d.unread();
}

/// Compares the stages of @p config with the golden @p expected configuration.
void compareStages(Mismatches& m, const json& expected, const FlightConfiguration& config)
{
    m.integer("stageCount", expected.at("stageCount").get<int>(), config.getStageCount());
    std::vector<int> activeStages;
    for (const QtRocket::AxialStage* stage : config.getActiveStages())
    {
        activeStages.push_back(stage->getStageNumber());
    }
    std::ranges::sort(activeStages);
    std::vector<int> expectedStages = expected.at("activeStages").get<std::vector<int>>();
    std::ranges::sort(expectedStages);
    if (activeStages != expectedStages)
    {
        m.note(std::format("activeStages: expected {} stages, got {}", expectedStages.size(),
                           activeStages.size()));
    }
    m.boolean("hasMotors", expected.at("hasMotors").get<bool>(), config.hasMotors());
    m.boolean("hasRecoveryDevice", expected.at("hasRecoveryDevice").get<bool>(),
              config.hasRecoveryDevice());
}

/// Compares the motor in the mount at the golden path with the golden @p expected motor.
void compareMotor(Mismatches& m, const json& expected, const Rocket& rocket,
                  const FlightConfiguration& config, const QtRocket::Preferences& preferences)
{
    const auto        path  = expected.at("mount").get<std::string>();
    const std::string field = std::format("motor in {}", path);
    const auto*       mount = dynamic_cast<const MotorMount*>(componentAtGoldenPath(rocket, path));
    if (mount == nullptr)
    {
        m.note(field + ": not a motor mount");
        return;
    }
    const MotorConfiguration& motor = mount->getMotorConfig(config.getId());
    if (motor.isEmpty())
    {
        m.note(field + ": no motor");
        return;
    }
    m.text(field + " motorName", expected.at("motorName").get<std::string>(),
           motor.toMotorName(preferences));
    m.text(field + " designation", expected.at("designation").get<std::string>(),
           motor.getMotor()->getDesignation());
    // The manufacturer is the thrust curve motor's (every motor of the test rockets is one).
    const auto* curve = dynamic_cast<const QtRocket::ThrustCurveMotor*>(motor.getMotor().get());
    m.text(field + " manufacturer", expected.at("manufacturer").get<std::string>(),
           curve != nullptr ? curve->getManufacturer().getSimpleName() : std::string{});
    m.text(field + " digest", expected.at("digest").get<std::string>(),
           motor.getMotor()->getDigest());
    m.relative(field + " ejectionDelay", goldenValue(expected.at("ejectionDelay")),
               motor.getEjectionDelay());
    m.relative(field + " nozzleExitDiameter", goldenValue(expected.at("nozzleExitDiameter")),
               motor.getNozzleExitDiameter());
    m.text(field + " ignitionEvent", expected.at("ignitionEvent").get<std::string>(),
           QtRocket::name(motor.getIgnitionEvent()));
    m.relative(field + " ignitionDelay", goldenValue(expected.at("ignitionDelay")),
               motor.getIgnitionDelay());
    m.integer(field + " motorCount", expected.at("motorCount").get<int>(), mount->getMotorCount());
    m.integer(field + " motorCountIncludingAssemblyCopies",
              expected.at("motorCountIncludingAssemblyCopies").get<int>(),
              mount->getMotorCountIncludingAssemblyCopies());
    m.absolute(field + " motorOverhang", goldenValue(expected.at("motorOverhang")),
               mount->getMotorOverhang());
    m.position(field + " position", goldenCoordinate(expected.at("position")),
               mount->getMotorPosition(config.getId()));
}

/// Compares the reference values, the lengths and the bounds of @p config.
void compareExtent(Mismatches& m, const json& expected, const FlightConfiguration& config)
{
    m.relative("referenceLength", goldenValue(expected.at("referenceLength")),
               config.getReferenceLength());
    m.relative("referenceArea", goldenValue(expected.at("referenceArea")),
               config.getReferenceArea());
    m.relative("length", goldenValue(expected.at("length")), config.getLength());
    m.relative("lengthAerodynamic", goldenValue(expected.at("lengthAerodynamic")),
               config.getLengthAerodynamic());

    const BoundingBox box      = config.getBoundingBox();
    const BoundingBox aero     = config.getBoundingBoxAerodynamic();
    const json&       boxJson  = expected.at("boundingBox");
    const json&       aeroJson = expected.at("boundingBoxAerodynamic");
    m.position("boundingBox.min", goldenCoordinate(boxJson.at("min")), box.min());
    m.position("boundingBox.max", goldenCoordinate(boxJson.at("max")), box.max());
    m.position("boundingBoxAerodynamic.min", goldenCoordinate(aeroJson.at("min")), aero.min());
    m.position("boundingBoxAerodynamic.max", goldenCoordinate(aeroJson.at("max")), aero.max());
}

/// Compares the instance contexts @p contexts of the component at @p path with the golden
/// @p instances: as many, each with its number, location and transformations.
void compareInstanceContexts(Mismatches& m, const std::string& path, const json& instances,
                             std::span<const InstanceContext> contexts)
{
    if (contexts.size() != instances.size())
    {
        m.note(std::format("instances of {}: expected {}, got {}", path, instances.size(),
                           contexts.size()));
        return;
    }
    for (std::size_t i = 0; i < contexts.size(); i++)
    {
        const json&       expected = instances.at(i);
        const std::string field    = std::format("instance {} of {}", i, path);
        m.integer(field + " number", expected.at("instanceNumber").get<int>(),
                  contexts[i].instanceNumber);
        m.position(field + " location", goldenCoordinate(expected.at("location")),
                   contexts[i].getLocation());
        m.transformation(field + " transform", expected.at("transform"), contexts[i].transform);
        m.transformation(field + " parentTransform", expected.at("parentTransform"),
                         contexts[i].getParentTransform());
    }
}

/// @p paths as one text, separated by spaces.
[[nodiscard]] std::string joinedPaths(std::span<const std::string> paths)
{
    std::string text;
    for (const std::string& path : paths)
    {
        text += text.empty() ? "" : " ";
        text += path;
    }
    return text;
}

/// Compares the keys of the instance map of @p config with the golden `instances` entries
/// (OpenRocket's key set, in tree order): the same components in the same order, so that the map
/// holds no component beyond them and no golden entry is missing or repeated.
void compareInstanceKeys(Mismatches& m, const json& expected, const FlightConfiguration& config)
{
    std::vector<std::string> expectedKeys;
    for (const json& entry : expected.at("instances"))
    {
        expectedKeys.push_back(entry.at("path").get<std::string>());
    }
    std::vector<std::string> keys;
    for (const RocketComponent* key : config.getActiveInstances().keys())
    {
        keys.push_back(goldenPathOf(*key));
    }
    if (keys != expectedKeys)
    {
        m.note(std::format("instances: expected the components [{}], got [{}]",
                           joinedPaths(expectedKeys), joinedPaths(keys)));
    }
}

/// Compares the active components of @p config and their instances.
void compareActiveInstances(Mismatches& m, const json& expected, const Rocket& rocket,
                            const FlightConfiguration& config)
{
    std::vector<const RocketComponent*> expectedActive;
    for (const json& path : expected.at("activeComponents"))
    {
        expectedActive.push_back(componentAtGoldenPath(rocket, path.get<std::string>()));
    }
    std::vector<const RocketComponent*> active;
    for (const RocketComponent* component : config.getAllActiveComponents())
    {
        active.push_back(component);
    }
    if (active != expectedActive)
    {
        m.note(std::format("activeComponents: expected {} in tree order, got {}",
                           expectedActive.size(), active.size()));
    }

    compareInstanceKeys(m, expected, config);
    for (const json& entry : expected.at("instances"))
    {
        const auto             path      = entry.at("path").get<std::string>();
        const RocketComponent* component = componentAtGoldenPath(rocket, path);
        if (component == nullptr)
        {
            m.note(std::format("instances of {}: no such component", path));
            continue;
        }
        compareInstanceContexts(m, path, entry.at("instances"),
                                config.getActiveInstances().getInstanceContexts(*component));
    }
}

// ================================================================================= mass.json

/// A number of a golden file.
/// @throws std::runtime_error when @p value is not one.
double number(const json& value)
{
    const std::optional<double> parsed = QtRocket::Test::goldenNumber(value);
    if (!parsed)
    {
        throw std::runtime_error("not a golden number: " + value.dump());
    }
    return *parsed;
}

/// A golden [x, y, z] or [x, y, z, w].
Coordinate coordinate(const json& value)
{
    const double weight = value.size() > 3 ? number(value.at(3)) : 0.0;
    return Coordinate{number(value.at(0)), number(value.at(1)), number(value.at(2)), weight};
}

/// The component at the golden @p path ("/", "/0", "/0/1/2": child indices from the root).
/// @throws std::runtime_error when @p root has no such component.
RocketComponent& componentAt(RocketComponent& root, std::string_view path)
{
    RocketComponent* const component = QtRocket::Test::componentAtGoldenPath(root, path);
    if (component == nullptr)
    {
        throw std::runtime_error("no component at the golden path " + std::string{path});
    }
    return *component;
}

/// The golden vector of coordinates @p values.
std::vector<Coordinate> coordinates(const json& values)
{
    std::vector<Coordinate> result;
    for (const json& value : values)
    {
        result.push_back(coordinate(value));
    }
    return result;
}

/// Expects @p actual within 1e-12 of @p expected in x, y and z.
void expectLocation(const Coordinate& actual, const Coordinate& expected, const std::string& what)
{
    EXPECT_NEAR(actual.x, expected.x, 1e-12) << what;
    EXPECT_NEAR(actual.y, expected.y, 1e-12) << what;
    EXPECT_NEAR(actual.z, expected.z, 1e-12) << what;
}

/// Expects @p actual within relative 1e-9 of @p expected (and 1e-15 absolute, for zeros).
void expectClose(double actual, double expected, const std::string& what)
{
    EXPECT_NEAR(actual, expected, (1e-9 * std::abs(expected)) + 1e-15) << what;
}

/// Expects @p actual within relative 1e-9 of @p expected, weight included.
void expectCloseCoordinate(const Coordinate& actual, const Coordinate& expected,
                           const std::string& what)
{
    expectClose(actual.x, expected.x, what + " x");
    expectClose(actual.y, expected.y, what + " y");
    expectClose(actual.z, expected.z, what + " z");
    expectClose(actual.weight, expected.weight, what + " weight");
}

/// Expects @p actual to be the golden rigid body @p expected (mass, CM, the three inertias).
void expectRigidBody(const RigidBody& actual, const json& expected, const std::string& what)
{
    expectClose(actual.getMass(), number(expected.at("mass")), what + " mass");
    expectCloseCoordinate(actual.getCM(), coordinate(expected.at("cm")), what + " cm");
    expectClose(actual.getIxx(), number(expected.at("ixx")), what + " ixx");
    expectClose(actual.getIyy(), number(expected.at("iyy")), what + " iyy");
    expectClose(actual.getIzz(), number(expected.at("izz")), what + " izz");
    expectClose(actual.getRotationalInertia(), number(expected.at("rotationalInertia")),
                what + " rotationalInertia");
    expectClose(actual.getLongitudinalInertia(), number(expected.at("longitudinalInertia")),
                what + " longitudinalInertia");
}

/// The key of the golden CM analysis row @p row: a motor's designation hash, else the key of
/// the component at its path. Expects the row's kind to fit its path: a "motor" has none, the
/// rocket's row is the "total" and every other row a "component".
std::int32_t analysisKey(Rocket& rocket, const json& row, const std::string& what)
{
    const auto kind = row.at("kind").get<std::string>();
    if (kind == "motor")
    {
        EXPECT_TRUE(row.at("path").is_null()) << what;
        return QtRocket::Strings::javaHashCode(row.at("name").get<std::string>());
    }
    const auto path = row.at("path").get<std::string>();
    EXPECT_EQ(kind, path == "/" ? "total" : "component") << what;
    return CMAnalysisEntry::keyOf(componentAt(rocket, path));
}

/// Expects @p analysis to hold exactly the golden CM analysis @p rows.
void expectGoldenAnalysis(Rocket& rocket, const CMAnalysisMap& analysis, const json& rows,
                          const std::string& configName)
{
    std::set<std::int32_t> keys;
    for (const json& row : rows)
    {
        const auto         rowName = row.at("name").get<std::string>();
        const std::string  what    = std::format("{} analysis {}", configName, rowName);
        const std::int32_t key     = analysisKey(rocket, row, what);
        keys.insert(key);
        const auto entry = analysis.find(key);
        ASSERT_TRUE(entry != analysis.end()) << what;
        EXPECT_EQ(entry->second.name, rowName) << what;
        expectClose(entry->second.eachMass, number(row.at("eachMass")), what + " eachMass");
        expectCloseCoordinate(entry->second.totalCM, coordinate(row.at("totalCM")),
                              what + " totalCM");
    }
    EXPECT_EQ(analysis.size(), keys.size()) << configName;
    // The rows end with the rocket's.
    ASSERT_FALSE(rows.empty()) << configName;
    EXPECT_EQ(rows.back().at("kind").get<std::string>(), "total") << configName;
}

/// Expects the four rigid bodies and the CM analysis of @p config to be the @p golden
/// configuration's.
void expectGoldenBodies(Rocket& rocket, const FlightConfiguration& config, const json& golden,
                        const std::string& name)
{
    expectRigidBody(MassCalculator::calculateStructure(config), golden.at("structure"),
                    name + " structure");
    expectRigidBody(MassCalculator::calculateLaunch(config), golden.at("launch"), name + " launch");
    expectRigidBody(MassCalculator::calculateBurnout(config), golden.at("burnout"),
                    name + " burnout");
    expectRigidBody(MassCalculator::calculateMotor(config), golden.at("motor"), name + " motor");
    expectGoldenAnalysis(rocket, MassCalculator::getCMAnalysis(config), golden.at("cmAnalysis"),
                         name);
}

/// Expects the header of the @p golden configuration to be that of @p config: the default flag,
/// the name (made with @p preferences) and, unless @p randomConfigurationId says that the maker
/// draws it, the id.
void expectGoldenHeader(const FlightConfiguration& config, const json& golden,
                        bool randomConfigurationId, const Preferences& preferences)
{
    const auto name = golden.at("name").get<std::string>();
    EXPECT_EQ(config.getId().isDefaultId(), golden.at("isDefault").get<bool>()) << name;
    if (config.getId().isDefaultId() || !randomConfigurationId)
    {
        EXPECT_EQ(config.getId().toString(), golden.at("id").get<std::string>()) << name;
    }
    EXPECT_EQ(config.getName(preferences), name);
}

// ================================================================================= aero.json

/// AeroDumper.NOZZLE_EXIT_DIAMETER_FRACTION: the nozzle exit diameter of the nozzle point, as a
/// fraction of the motor mount's diameter.
constexpr double kNozzleExitDiameterFraction = 0.5;

/// The index of the point whose pitch centre is the structure CG.
constexpr std::size_t kStructureCgPoint = 23;

/// The index of the point with thrusting nozzles.
constexpr std::size_t kNozzlePoint = 24;

/// AeroDumper.MACHS: the Mach numbers of the grid of points and of the worst CPs.
constexpr std::array<double, 7> kDumperMachs{0.05, 0.3, 0.6, 0.9, 1.1, 1.5, 2.0};

/// AeroDumper.AOAS_DEG: the angles of attack of the grid of points, in degrees.
constexpr std::array<double, 3> kDumperAoasDegrees{0.0, 2.0, 10.0};

/// The number of points of the grid: index = 3 * Mach index + angle index.
constexpr std::size_t kGridPoints = kDumperMachs.size() * kDumperAoasDegrees.size();

/// The number of points of a configuration: the grid and the four after it.
constexpr std::size_t kDumperPoints = kGridPoints + 4;

/// The keys of the objects of an aero.json document that the comparison reads (compares, sets
/// as an input, follows or, for the first three of the file, leaves to goldens_schema_tests.cpp).
/// Those of a "forces" object are kForceFields and isComparedForceKey().
constexpr std::array<std::string_view, 6> kFileKeys{
    "schema", "schemaVersion", "input", "atmosphere", "stallAngle", "configurations"};
constexpr std::array<std::string_view, 6> kAtmosphereKeys{
    "temperature", "pressure", "relativeHumidity", "machSpeed", "density", "kinematicViscosity"};
constexpr std::array<std::string_view, 10> kConfigurationKeys{"index",
                                                              "id",
                                                              "name",
                                                              "isDefault",
                                                              "referenceLength",
                                                              "referenceArea",
                                                              "sameResultsAs",
                                                              "geometryWarnings",
                                                              "points",
                                                              "worstCP"};
constexpr std::array<std::string_view, 5>  kPointKeys{"conditions", "cp", "forces", "components",
                                                      "warnings"};
constexpr std::array<std::string_view, 12> kConditionKeys{
    "mach",        "aoa",       "theta",   "rollRate", "pitchRate", "yawRate",
    "pitchCenter", "refLength", "refArea", "velocity", "beta",      "thrustingNozzleExitAreas"};
constexpr std::array<std::string_view, 2> kNozzleKeys{"assembly", "area"};
constexpr std::array<std::string_view, 3> kWorstCpKeys{"mach", "cp", "theta"};

/// What the results @p results of one configuration hold.
[[nodiscard]] AeroCounts goldenResultCounts(const json& results)
{
    AeroCounts counts;
    counts.configurations = 1;
    counts.warnings       = static_cast<int>(results.at("geometryWarnings").size());
    for (const json& point : results.at("points"))
    {
        counts.points++;
        counts.components += static_cast<int>(point.at("components").size());
        counts.warnings += static_cast<int>(point.at("warnings").size());
    }
    counts.worstCPs = static_cast<int>(results.at("worstCP").size());
    return counts;
}

// -------------------------------------------------------------------------------- conditions

/// The nozzle exit areas of the golden list @p expected (assembly path and area), for
/// FlightConditions::setThrustingNozzleExitAreas(); an entry whose path is no assembly of
/// @p rocket is reported and left out.
[[nodiscard]] std::vector<FlightConditions::NozzleExitArea> goldenNozzleAreas(Mismatches& m,
                                                                              const json& expected,
                                                                              const Rocket& rocket)
{
    std::vector<FlightConditions::NozzleExitArea> areas;
    for (const json& entry : expected)
    {
        const auto  path = entry.at("assembly").get<std::string>();
        const auto* assembly =
            dynamic_cast<const ComponentAssembly*>(componentAtGoldenPath(rocket, path));
        if (assembly == nullptr)
        {
            m.note(std::format("thrustingNozzleExitAreas: {} is not an assembly", path));
            continue;
        }
        areas.emplace_back(assembly, goldenValue(entry.at("area")));
    }
    return areas;
}

/// The nozzle exit areas the dumper gives the nozzle point (AeroDumper.nozzleExitAreas()): every
/// active motor mount contributes getMotorCount() nozzles of half its motor mount diameter to its
/// assembly, whether or not the configuration gives it a motor. By assembly path, in tree order
/// of the first mount of each assembly.
[[nodiscard]] std::vector<std::pair<std::string, double>> dumperNozzleAreas(
    const Rocket& rocket, const FlightConfiguration& config)
{
    std::vector<std::pair<std::string, double>> areas;
    for (const RocketComponent& component : rocket.subtree())
    {
        const auto* mount = dynamic_cast<const MotorMount*>(&component);
        if (mount == nullptr || !mount->isMotorMount() || !config.isComponentActive(component))
        {
            continue;
        }
        const double radius = kNozzleExitDiameterFraction * mount->getMotorMountDiameter() / 2;
        const double area =
            mount->getMotorCount() * std::numbers::pi * QtRocket::MathUtil::pow2(radius);
        const std::string path = goldenPathOf(component.getAssembly());
        const auto same = std::ranges::find(areas, path, &std::pair<std::string, double>::first);
        if (same == areas.end())
        {
            areas.emplace_back(path, area);
        }
        else
        {
            same->second += area;
        }
    }
    return areas;
}

/// Compares the nozzle exit areas the motor mounts of @p config give (dumperNozzleAreas()) with
/// the golden list @p expected of the nozzle point, which is in tree order of the assemblies.
void compareDumperNozzleAreas(Mismatches& m, const json& expected, const Rocket& rocket,
                              const FlightConfiguration& config)
{
    std::vector<std::pair<std::string, double>> areas = dumperNozzleAreas(rocket, config);
    m.integer("nozzle areas of the motor mounts: number",
              static_cast<std::int64_t>(expected.size()), static_cast<std::int64_t>(areas.size()));
    for (const json& entry : expected)
    {
        const auto path = entry.at("assembly").get<std::string>();
        const auto same = std::ranges::find(areas, path, &std::pair<std::string, double>::first);
        if (same == areas.end())
        {
            m.note(std::format("nozzle areas of the motor mounts: none for {}", path));
            continue;
        }
        m.relative(std::format("nozzle area of the motor mounts of {}", path),
                   goldenValue(entry.at("area")), same->second);
    }
}

/// One row of the dumper's table of points (AeroDumper.Point): the Mach number, the angle of
/// attack in degrees (the dumper converts with Math.toRadians), theta and the three rates.
struct DumperPoint
{
    double mach;
    double aoaDegrees;
    double theta;
    double rollRate;
    double pitchRate;
    double yawRate;
};

/// The conditions the dumper gives point @p index (AeroDumper.points()): the grid of Mach
/// numbers and angles of attack at theta 0 without rotation, then a wind direction off the fin
/// planes (21), rotation about the nose tip (22) and about the structure CG (23), and thrusting
/// nozzles (24). nullopt beyond them.
[[nodiscard]] std::optional<DumperPoint> dumperPoint(std::size_t index)
{
    if (index < kGridPoints)
    {
        return DumperPoint{.mach       = kDumperMachs.at(index / kDumperAoasDegrees.size()),
                           .aoaDegrees = kDumperAoasDegrees.at(index % kDumperAoasDegrees.size()),
                           .theta      = 0.0,
                           .rollRate   = 0.0,
                           .pitchRate  = 0.0,
                           .yawRate    = 0.0};
    }
    switch (index)
    {
        case kGridPoints:
            return DumperPoint{.mach       = 0.3,
                               .aoaDegrees = 5.0,
                               .theta      = std::numbers::pi / 4,
                               .rollRate   = 0.0,
                               .pitchRate  = 0.0,
                               .yawRate    = 0.0};
        case kGridPoints + 1:
        case kStructureCgPoint:
            return DumperPoint{.mach       = 0.8,
                               .aoaDegrees = 2.0,
                               .theta      = 1.0,
                               .rollRate   = 20.0,
                               .pitchRate  = 2.0,
                               .yawRate    = 1.0};
        case kNozzlePoint:
            return DumperPoint{.mach       = 0.6,
                               .aoaDegrees = 0.0,
                               .theta      = 0.0,
                               .rollRate   = 0.0,
                               .pitchRate  = 0.0,
                               .yawRate    = 0.0};
        default:
            return std::nullopt;
    }
}

/// Compares the inputs of the golden "conditions" @p expected of point @p index with the
/// dumper's table: the conditions are set from the file, so a file whose inputs are not the
/// dumper's would have the forces compared at other conditions than the README states. The
/// pitch centre is the nose tip, but for its x at point 23, and there are no thrusting nozzles
/// but at point 24 (compareResults() derives both of those).
void compareWithDumperPoint(Mismatches& m, std::size_t index, const json& expected)
{
    const std::optional<DumperPoint> point = dumperPoint(index);
    if (!point)
    {
        m.note(std::format("point {} is beyond the dumper's {} points", index, kDumperPoints));
        return;
    }
    m.exact("the dumper's mach", point->mach, goldenValue(expected.at("mach")));
    m.absolute("the dumper's aoa", point->aoaDegrees * std::numbers::pi / 180,
               goldenValue(expected.at("aoa")));
    m.absolute("the dumper's theta", point->theta, goldenValue(expected.at("theta")));
    m.exact("the dumper's rollRate", point->rollRate, goldenValue(expected.at("rollRate")));
    m.exact("the dumper's pitchRate", point->pitchRate, goldenValue(expected.at("pitchRate")));
    m.exact("the dumper's yawRate", point->yawRate, goldenValue(expected.at("yawRate")));
    const Coordinate pitchCenter = goldenCoordinate(expected.at("pitchCenter"));
    if (index != kStructureCgPoint)
    {
        m.absolute("the dumper's pitchCenter.x", 0.0, pitchCenter.x);
    }
    m.absolute("the dumper's pitchCenter.y", 0.0, pitchCenter.y);
    m.absolute("the dumper's pitchCenter.z", 0.0, pitchCenter.z);
    if (index != kNozzlePoint)
    {
        m.integer("the dumper's thrustingNozzleExitAreas: number", 0,
                  static_cast<std::int64_t>(expected.at("thrustingNozzleExitAreas").size()));
    }
}

/// The flight conditions of the golden point @p expected (its "conditions") for @p config: new
/// conditions as the dumper makes them, then every value set from the golden one, in the
/// dumper's order (the reference length and area come first: the dumper's conditions take them
/// from the configuration, and so must new conditions here, which is compared before they are
/// set).
[[nodiscard]] FlightConditions goldenConditions(Mismatches& m, const json& expected,
                                                const Rocket&              rocket,
                                                const FlightConfiguration& config)
{
    FlightConditions conditions(config);
    m.relative("refLength of new conditions", goldenValue(expected.at("refLength")),
               conditions.getRefLength());
    m.relative("refArea of new conditions", goldenValue(expected.at("refArea")),
               conditions.getRefArea());
    conditions.setRefLength(goldenValue(expected.at("refLength")));
    conditions.setRefArea(goldenValue(expected.at("refArea")));
    conditions.setMach(goldenValue(expected.at("mach")));
    conditions.setAOA(goldenValue(expected.at("aoa")));
    conditions.setTheta(goldenValue(expected.at("theta")));
    conditions.setRollRate(goldenValue(expected.at("rollRate")));
    conditions.setPitchRate(goldenValue(expected.at("pitchRate")));
    conditions.setYawRate(goldenValue(expected.at("yawRate")));
    conditions.setPitchCenter(goldenCoordinate(expected.at("pitchCenter")));
    conditions.setThrustingNozzleExitAreas(
        goldenNozzleAreas(m, expected.at("thrustingNozzleExitAreas"), rocket));
    return conditions;
}

/// Compares what @p actual answers with the golden "conditions" @p expected: the values that were
/// set come back exactly (a setter that kept a slightly different value would show), the derived
/// velocity and beta within the tolerance.
void compareConditions(Mismatches& m, const json& expected, const FlightConditions& actual,
                       const Rocket& rocket)
{
    m.exact("conditions.mach", goldenValue(expected.at("mach")), actual.getMach());
    m.exact("conditions.aoa", goldenValue(expected.at("aoa")), actual.getAOA());
    m.exact("conditions.theta", goldenValue(expected.at("theta")), actual.getTheta());
    m.exact("conditions.rollRate", goldenValue(expected.at("rollRate")), actual.getRollRate());
    m.exact("conditions.pitchRate", goldenValue(expected.at("pitchRate")), actual.getPitchRate());
    m.exact("conditions.yawRate", goldenValue(expected.at("yawRate")), actual.getYawRate());
    const Coordinate pitchCenter = goldenCoordinate(expected.at("pitchCenter"));
    m.exact("conditions.pitchCenter.x", pitchCenter.x, actual.getPitchCenter().x);
    m.exact("conditions.pitchCenter.y", pitchCenter.y, actual.getPitchCenter().y);
    m.exact("conditions.pitchCenter.z", pitchCenter.z, actual.getPitchCenter().z);
    m.exact("conditions.refLength", goldenValue(expected.at("refLength")), actual.getRefLength());
    m.exact("conditions.refArea", goldenValue(expected.at("refArea")), actual.getRefArea());
    m.relative("conditions.velocity", goldenValue(expected.at("velocity")), actual.getVelocity());
    m.relative("conditions.beta", goldenValue(expected.at("beta")), actual.getBeta());

    const json& nozzles = expected.at("thrustingNozzleExitAreas");
    m.integer("conditions.thrustingNozzleExitAreas: number",
              static_cast<std::int64_t>(nozzles.size()),
              static_cast<std::int64_t>(actual.getThrustingNozzleExitAreas().size()));
    for (const json& entry : nozzles)
    {
        const auto  path = entry.at("assembly").get<std::string>();
        const auto* assembly =
            dynamic_cast<const ComponentAssembly*>(componentAtGoldenPath(rocket, path));
        if (assembly != nullptr)
        {
            m.exact(std::format("conditions.thrustingNozzleExitAreas {}", path),
                    goldenValue(entry.at("area")), actual.getThrustingNozzleExitArea(*assembly));
        }
        noteUncomparedKeys(m, std::format("conditions.thrustingNozzleExitAreas {}", path), entry,
                           kNozzleKeys);
    }
    noteUncomparedKeys(m, "conditions", expected, kConditionKeys);
}

// ------------------------------------------------------------------------------------ forces

/// One coefficient of the golden "forces" and the getter that answers it.
struct ForceField
{
    std::string_view key;
    double (AerodynamicForces::*getter)() const;
};

/// The coefficients of a golden "forces" object (AeroDumper.forces()), read through the getters,
/// so with the component's CD override applied; "cp" and "axisymmetric" are compared apart.
constexpr std::array<ForceField, 15> kForceFields{{
    {.key = "cn", .getter = &AerodynamicForces::getCN},
    {.key = "cm", .getter = &AerodynamicForces::getCm},
    {.key = "cside", .getter = &AerodynamicForces::getCside},
    {.key = "cyaw", .getter = &AerodynamicForces::getCyaw},
    {.key = "croll", .getter = &AerodynamicForces::getCroll},
    {.key = "crollDamp", .getter = &AerodynamicForces::getCrollDamp},
    {.key = "crollForce", .getter = &AerodynamicForces::getCrollForce},
    {.key = "cd", .getter = &AerodynamicForces::getCD},
    {.key = "cdAxial", .getter = &AerodynamicForces::getCDaxial},
    {.key = "pressureCD", .getter = &AerodynamicForces::getPressureCD},
    {.key = "baseCD", .getter = &AerodynamicForces::getBaseCD},
    {.key = "frictionCD", .getter = &AerodynamicForces::getFrictionCD},
    {.key = "overrideCD", .getter = &AerodynamicForces::getOverrideCD},
    {.key = "pitchDampingMoment", .getter = &AerodynamicForces::getPitchDampingMoment},
    {.key = "yawDampingMoment", .getter = &AerodynamicForces::getYawDampingMoment},
}};

/// Whether the golden forces key @p key is one compareForces() compares.
[[nodiscard]] bool isComparedForceKey(std::string_view key)
{
    return key == "path" || key == "cp" || key == "axisymmetric" ||
           std::ranges::find(kForceFields, key, &ForceField::key) != kForceFields.end();
}

/// Compares @p actual with the golden forces @p expected: the CP (its position within the
/// absolute tolerance, its weight, CNa, within the relative one), every coefficient and the
/// axisymmetric flag. A golden field that nothing compares is reported.
void compareForces(Mismatches& m, std::string_view what, const json& expected,
                   const AerodynamicForces& actual)
{
    m.cg(std::format("{}.cp", what), goldenCoordinate(expected.at("cp")), actual.getCP());
    for (const ForceField& field : kForceFields)
    {
        m.relative(std::format("{}.{}", what, field.key), goldenValue(expected.at(field.key)),
                   (actual.*field.getter)());
    }
    m.boolean(std::format("{}.axisymmetric", what), expected.at("axisymmetric").get<bool>(),
              actual.isAxisymmetric());
    for (const auto& [key, value] : expected.items())
    {
        if (!isComparedForceKey(key))
        {
            m.note(std::format("{}.{}: not compared", what, key));
        }
    }
}

/// The golden paths of the components of @p forceMap, in its order, separated by spaces.
[[nodiscard]] std::string keyPaths(const ForceMap& forceMap)
{
    std::string paths;
    for (const auto& [component, forces] : forceMap)
    {
        paths += paths.empty() ? "" : " ";
        paths += component != nullptr ? goldenPathOf(*component) : std::string{"null"};
    }
    return paths;
}

/// The paths of the golden force-analysis entries @p expected, in their order (the tree's),
/// separated by spaces.
[[nodiscard]] std::string entryPaths(const json& expected)
{
    std::string paths;
    for (const json& entry : expected)
    {
        paths += paths.empty() ? "" : " ";
        paths += entry.at("path").get<std::string>();
    }
    return paths;
}

/// Compares the force analysis @p actual with the golden "components" @p expected: the same
/// components in the same (tree) order, so none missing and none beyond them, and the forces of
/// each. Returns the number of golden entries compared.
[[nodiscard]] int compareForceAnalysis(Mismatches& m, const json& expected, const ForceMap& actual,
                                       const Rocket& rocket)
{
    m.text("components", entryPaths(expected), keyPaths(actual));
    int compared = 0;
    for (const json& entry : expected)
    {
        const auto               path      = entry.at("path").get<std::string>();
        const RocketComponent*   component = componentAtGoldenPath(rocket, path);
        const AerodynamicForces* forces    = component != nullptr ? actual.get(component) : nullptr;
        if (forces == nullptr)
        {
            m.note(std::format("components {}: not in the force analysis", path));
            continue;
        }
        compareForces(m, std::format("components {}", path), entry, *forces);
        compared++;
    }
    return compared;
}

// ----------------------------------------------------------------------------- configuration

/// Calculates the golden point @p expected, the one of index @p index, as the dumper does
/// (getCP(), getAerodynamicForces() and getForceAnalysis() with one warning set) and compares
/// everything it records. Adds what was compared to @p counts.
void comparePoint(Mismatches& m, std::size_t index, const json& expected,
                  const AeroSubject& subject, AeroCounts& counts)
{
    const json& expectedConditions = expected.at("conditions");
    compareWithDumperPoint(m, index, expectedConditions);
    const FlightConditions conditions =
        goldenConditions(m, expectedConditions, *subject.rocket, *subject.config);
    compareConditions(m, expectedConditions, conditions, *subject.rocket);

    WarningSet       warnings;
    const Coordinate cp = subject.calculator->getCP(*subject.config, conditions, &warnings);
    m.cg("cp", goldenCoordinate(expected.at("cp")), cp);

    const AerodynamicForces total =
        subject.calculator->getAerodynamicForces(*subject.config, conditions, &warnings);
    compareForces(m, "forces", expected.at("forces"), total);

    const ForceMap analysis =
        subject.calculator->getForceAnalysis(*subject.config, conditions, &warnings);
    counts.components +=
        compareForceAnalysis(m, expected.at("components"), analysis, *subject.rocket);
    counts.warnings +=
        compareWarnings(m, "warnings", expected.at("warnings"), warnings, *subject.rocket);
    noteUncomparedKeys(m, "", expected, kPointKeys);
    counts.points++;
}

/// The CP of the configuration of @p subject at the Mach number @p mach, an angle of attack of 0
/// and the wind direction @p theta, from a calculator of its own (so that the compared one sees
/// the dumper's calls only).
[[nodiscard]] Coordinate cpAtTheta(const AeroSubject& subject, double mach, double theta)
{
    FlightConditions conditions(*subject.config);
    conditions.setMach(mach);
    conditions.setAOA(0.0);
    conditions.setTheta(theta);
    BarrowmanCalculator calculator;
    WarningSet          warnings;
    return calculator.getCP(*subject.config, conditions, &warnings);
}

/// What is wrong with @p theta as the direction of the worst CP @p worst that getWorstCP() found
/// at the Mach number @p mach, "" when nothing is: without a CP in any direction (a rocket
/// without lift: the golden worst CP @p goldenWorst has no weight) getWorstCP() leaves 0;
/// otherwise the theta is one of the 360 directions, and the CP at it is the worst CP found.
///
/// The CP at the theta is compared with the worst CP found, not with the golden one: the two
/// worst CPs are compared by the caller, and a difference between them must not be reported
/// twice, here only on the platforms whose theta is not the golden one.
[[nodiscard]] std::string foundThetaProblem(const AeroSubject& subject, double mach, double theta,
                                            const Coordinate& worst, const Coordinate& goldenWorst)
{
    if (!(goldenWorst.weight > 0))
    {
        return "no direction has a CP, which leaves a theta of 0";
    }
    if (!isWorstCpDirection(theta))
    {
        return "it is none of the 360 directions getWorstCP() tries";
    }
    const Coordinate cp = cpAtTheta(subject, mach, theta);
    if (!(std::abs(cp.x - worst.x) <= kGoldenAbsolute))
    {
        return std::format("the CP at the theta found, x = {}, is not the worst CP found, x = {}",
                           cp.x, worst.x);
    }
    return "";
}

/// Calculates the golden worst CP @p expected as the dumper does (new conditions with the Mach
/// number at an angle of attack of 0; their theta is kThetaBeforeWorstCp, see there) and
/// compares the CP and the theta found. Returns the theta found when it is not the golden one,
/// else nullopt.
[[nodiscard]] std::optional<double> compareWorstCP(Mismatches& m, const json& expected,
                                                   const AeroSubject& subject)
{
    const double     mach = goldenValue(expected.at("mach"));
    FlightConditions conditions(*subject.config);
    conditions.setMach(mach);
    conditions.setAOA(0.0);
    conditions.setTheta(kThetaBeforeWorstCp);
    WarningSet       warnings;
    const Coordinate cp = subject.calculator->getWorstCP(*subject.config, conditions, &warnings);
    m.cg("cp", goldenCoordinate(expected.at("cp")), cp);
    noteUncomparedKeys(m, "", expected, kWorstCpKeys);
    const double theta = conditions.getTheta();
    if (compareWorstTheta(m, subject, expected, theta, cp))
    {
        return std::nullopt;
    }
    return theta;
}

/// Compares the worst CPs of the configuration of @p subject with the golden list @p expected
/// and adds what it found to @p result.
void compareWorstCPs(AeroComparison& result, const json& expected, const AeroSubject& subject,
                     const std::string& context)
{
    for (const json& worst : expected)
    {
        const std::string what =
            std::format("{} worst CP at Mach {}", context, goldenValue(worst.at("mach")));
        Mismatches m(what);
        if (const std::optional<double> theta = compareWorstCP(m, worst, subject))
        {
            result.otherThetas += std::format("{}: theta {}, golden {}\n", what, *theta,
                                              goldenValue(worst.at("theta")));
        }
        result.report += m.report();
        result.compared.worstCPs++;
    }
}

/// Compares the Mach numbers of the golden worst CPs @p expected with the dumper's
/// (AeroDumper.MACHS): as many, and each one.
void compareWorstCpMachs(Mismatches& m, const json& expected)
{
    m.integer("worstCP: number", static_cast<std::int64_t>(kDumperMachs.size()),
              static_cast<std::int64_t>(expected.size()));
    for (std::size_t i = 0; i < std::min(kDumperMachs.size(), expected.size()); i++)
    {
        m.exact(std::format("worstCP[{}].mach", i), kDumperMachs.at(i),
                goldenValue(expected.at(i).at("mach")));
    }
}

/// Compares the header and the reference values of @p config with the golden configuration
/// @p expected. The id is compared unless it is one the maker draws at random; the name is made
/// with @p preferences.
void compareHeader(Mismatches& m, const json& expected, const FlightConfiguration& config,
                   bool randomConfigurationId, const Preferences& preferences)
{
    m.boolean("isDefault", expected.at("isDefault").get<bool>(), config.getId().isDefaultId());
    if (config.getId().isDefaultId() || !randomConfigurationId)
    {
        m.text("id", expected.at("id").get<std::string>(), config.getId().toString());
    }
    m.text("name", expected.at("name").get<std::string>(), config.getName(preferences));
    m.relative("referenceLength", goldenValue(expected.at("referenceLength")),
               config.getReferenceLength());
    m.relative("referenceArea", goldenValue(expected.at("referenceArea")),
               config.getReferenceArea());
    noteUncomparedKeys(m, "", expected, kConfigurationKeys);
}

/// Compares what every file records once: the atmosphere of the default flight conditions, the
/// stall angle, and the number of configurations, which is that of @p rocket with its default
/// one (the dumper writes one entry per configuration of rocket.getFlightConfigurations(); a
/// rocket with more configurations than the file would otherwise be compared without a
/// mismatch, since the loop follows the file).
void compareFileValues(Mismatches& m, const json& aero, const Rocket& rocket)
{
    const QtRocket::AtmosphericConditions atmosphere =
        FlightConditions().getAtmosphericConditions();
    const json& expected = aero.at("atmosphere");
    m.relative("atmosphere.temperature", goldenValue(expected.at("temperature")),
               atmosphere.getTemperature());
    m.relative("atmosphere.pressure", goldenValue(expected.at("pressure")),
               atmosphere.getPressure());
    m.relative("atmosphere.relativeHumidity", goldenValue(expected.at("relativeHumidity")),
               atmosphere.getRelativeHumidity());
    m.relative("atmosphere.machSpeed", goldenValue(expected.at("machSpeed")),
               atmosphere.getMachSpeed());
    m.relative("atmosphere.density", goldenValue(expected.at("density")), atmosphere.getDensity());
    m.relative("atmosphere.kinematicViscosity", goldenValue(expected.at("kinematicViscosity")),
               atmosphere.getKinematicViscosity());
    m.relative("stallAngle", goldenValue(aero.at("stallAngle")),
               BarrowmanCalculator().getStallAngle());
    m.integer("configurations: number", static_cast<std::int64_t>(aero.at("configurations").size()),
              static_cast<std::int64_t>(rocket.getConfigurationCount()) + 1);
    noteUncomparedKeys(m, "atmosphere", expected, kAtmosphereKeys);
    noteUncomparedKeys(m, "", aero, kFileKeys);
}

}  // namespace

// ============================================================================= geometry.json

GeometryComparison compareComponents(const Rocket& rocket, const json& geometry)
{
    GeometryComparison result;
    for (const json& expected : geometry.at("components"))
    {
        const auto path = expected.at("path").get<std::string>();
        Mismatches m(std::format("{} {} \"{}\"", expected.at("type").get<std::string>(), path,
                                 expected.at("name").get<std::string>()));
        const RocketComponent* actual = componentAtGoldenPath(rocket, path);
        if (actual == nullptr)
        {
            m.note("no such component");
        }
        else
        {
            comparePlacement(m, expected, *actual);
            compareMass(m, expected, *actual);
            compareInstances(m, expected, *actual);
            m.positions("componentBounds", expected.at("componentBounds"),
                        actual->getComponentBounds());
            compareDetails(m, expected, *actual);
            result.compared++;
        }
        result.report += m.report();
    }
    return result;
}

int componentCount(const Rocket& rocket)
{
    int count = 0;
    for ([[maybe_unused]] const RocketComponent& component : rocket.subtree())
    {
        count++;
    }
    return count;
}

GeometryComparison compareConfigurations(Rocket& rocket, const json& geometry,
                                         bool randomConfigurationId, const Preferences& preferences)
{
    const FlightConfigurationId selected = rocket.getSelectedConfiguration().getId();
    GeometryComparison          result;
    for (const json& expected : geometry.at("configurations"))
    {
        const int  index = expected.at("index").get<int>();
        Mismatches m(std::format("configuration {}", index));
        if (index < 0 || index > rocket.getConfigurationCount())
        {
            m.note("no such configuration");
            result.report += m.report();
            continue;
        }
        const FlightConfiguration& config = rocket.getFlightConfigurationByIndex(index, true);
        rocket.setSelectedConfiguration(config.getId());

        m.boolean("isDefault", expected.at("isDefault").get<bool>(), config.getId().isDefaultId());
        if (config.getId().isDefaultId() || !randomConfigurationId)
        {
            m.text("id", expected.at("id").get<std::string>(), config.getId().toString());
        }
        m.text("name", expected.at("name").get<std::string>(), config.getName(preferences));
        compareStages(m, expected, config);
        m.integer("motors", static_cast<long long>(expected.at("motors").size()),
                  static_cast<long long>(config.getActiveMotors().size()));
        for (const json& motor : expected.at("motors"))
        {
            compareMotor(m, motor, rocket, config, preferences);
        }
        compareExtent(m, expected, config);
        compareActiveInstances(m, expected, rocket, config);
        result.report += m.report();
        result.compared++;
    }
    rocket.setSelectedConfiguration(selected);
    return result;
}

int selectedIndex(const Rocket& rocket)
{
    const FlightConfigurationId& selected = rocket.getSelectedConfiguration().getId();
    if (selected.isDefaultId())
    {
        return 0;
    }
    const std::vector<FlightConfigurationId> ids = rocket.getIds();
    for (std::size_t i = 0; i < ids.size(); i++)
    {
        if (ids[i] == selected)
        {
            return static_cast<int>(i) + 1;
        }
    }
    return -1;
}

int goldenSelectedIndex(const json& geometry)
{
    const auto selected = geometry.at("selectedConfiguration").get<std::string>();
    for (const json& configuration : geometry.at("configurations"))
    {
        if (configuration.at("id").get<std::string>() == selected)
        {
            return configuration.at("index").get<int>();
        }
    }
    return -2;
}

int goldenCount(const json& geometry, const char* key)
{
    return static_cast<int>(geometry.at(key).size());
}

// ================================================================================= mass.json

void expectGoldenLocations(Rocket& rocket, const json& geometry)
{
    for (const json& golden : geometry.at("components"))
    {
        const auto                    path     = golden.at("path").get<std::string>();
        const std::vector<Coordinate> actual   = componentAt(rocket, path).getComponentLocations();
        const std::vector<Coordinate> expected = coordinates(golden.at("componentLocations"));
        ASSERT_EQ(actual.size(), expected.size()) << path;
        for (std::size_t i = 0; i < actual.size(); i++)
        {
            expectLocation(actual[i], expected[i], std::format("{} #{}", path, i));
        }
    }
}

int expectGoldenMass(Rocket& rocket, const json& mass, bool randomConfigurationId,
                     const Preferences& preferences)
{
    const FlightConfigurationId selected = rocket.getSelectedConfiguration().getId();
    int                         compared = 0;
    int                         position = 0;
    for (const json& golden : mass.at("configurations"))
    {
        const auto name  = golden.at("name").get<std::string>();
        const int  index = golden.at("index").get<int>();
        EXPECT_EQ(index, position) << name << ": the golden configurations are in order";
        position++;
        if (index < 0 || index > rocket.getConfigurationCount())
        {
            ADD_FAILURE() << "the rocket has no configuration " << index << " (" << name << ")";
            continue;
        }
        const FlightConfiguration& config = rocket.getFlightConfigurationByIndex(index, true);
        rocket.setSelectedConfiguration(config.getId());

        expectGoldenHeader(config, golden, randomConfigurationId, preferences);
        expectGoldenBodies(rocket, config, golden, name);
        compared++;
    }
    rocket.setSelectedConfiguration(selected);
    return compared;
}

// ================================================================================= aero.json

std::string toText(const AeroCounts& counts)
{
    return std::format(
        "{} configurations, {} points, {} force-analysis entries, {} worst CPs, {} warnings",
        counts.configurations, counts.points, counts.components, counts.worstCPs, counts.warnings);
}

std::ostream& operator<<(std::ostream& out, const AeroCounts& counts)
{
    return out << toText(counts);
}

AeroCounts goldenCounts(const json& aero)
{
    AeroCounts counts;
    for (std::size_t i = 0; i < aero.at("configurations").size(); i++)
    {
        if (const json* results = aeroResults(aero, i))
        {
            counts += goldenResultCounts(*results);
        }
    }
    return counts;
}

bool isWorstCpDirection(double theta)
{
    const double i = std::round(theta * 360 / (2 * std::numbers::pi));
    return i >= 0 && i < 360 && theta == 2 * std::numbers::pi * i / 360;
}

bool compareWorstTheta(Mismatches& m, const AeroSubject& subject, const json& expected,
                       double theta, const Coordinate& worst)
{
    const double expectedTheta = goldenValue(expected.at("theta"));
    if (std::abs(theta - expectedTheta) <= kGoldenAbsolute)
    {
        return true;
    }
    const double      mach = goldenValue(expected.at("mach"));
    const std::string problem =
        foundThetaProblem(subject, mach, theta, worst, goldenCoordinate(expected.at("cp")));
    if (!problem.empty())
    {
        m.note(std::format("theta: expected {}, got {}; {}", expectedTheta, theta, problem));
        return false;
    }
    const Coordinate cp = cpAtTheta(subject, mach, expectedTheta);
    if (!(std::abs(cp.x - worst.x) <= kGoldenAbsolute))
    {
        m.note(
            std::format("theta: expected {}, got {}; the CP at the golden theta, x = {}, is "
                        "not the worst CP, x = {}",
                        expectedTheta, theta, cp.x, worst.x));
    }
    return false;
}

AeroComparison compareResults(const json& results, const Rocket& rocket,
                              const FlightConfiguration& config, const std::string& context)
{
    AeroComparison      result;
    BarrowmanCalculator calculator;
    const AeroSubject   subject{.rocket = &rocket, .config = &config, .calculator = &calculator};

    Mismatches geometry(context + " geometry warnings");
    WarningSet geometryWarnings;
    calculator.checkGeometry(config, rocket, &geometryWarnings);
    result.compared.warnings += compareWarnings(
        geometry, "geometryWarnings", results.at("geometryWarnings"), geometryWarnings, rocket);
    result.report += geometry.report();

    // The dumper calculates the structure CG here, for the pitch centre of point 23, and takes
    // the nozzle exit areas of point 24 from the motor mounts.
    const json&  points       = results.at("points");
    const double structureCgX = QtRocket::MassCalculator::calculateStructure(config).getCM().x;
    Mismatches   inputs(context + " inputs of the points");
    inputs.integer("points: number", static_cast<std::int64_t>(kDumperPoints),
                   static_cast<std::int64_t>(points.size()));
    compareWorstCpMachs(inputs, results.at("worstCP"));
    if (points.size() > kNozzlePoint)
    {
        inputs.absolute(
            "structure CG x (the pitch centre of point 23)",
            goldenCoordinate(points.at(kStructureCgPoint).at("conditions").at("pitchCenter")).x,
            structureCgX);
        compareDumperNozzleAreas(
            inputs, points.at(kNozzlePoint).at("conditions").at("thrustingNozzleExitAreas"), rocket,
            config);
    }
    else
    {
        inputs.note("no structure CG point and no nozzle point");
    }
    result.report += inputs.report();

    for (std::size_t i = 0; i < points.size(); i++)
    {
        const json& conditions = points.at(i).at("conditions");
        Mismatches  m(std::format("{} point {} (Mach {}, angle of attack {})", context, i,
                                  goldenValue(conditions.at("mach")),
                                  goldenValue(conditions.at("aoa"))));
        comparePoint(m, i, points.at(i), subject, result.compared);
        result.report += m.report();
    }

    compareWorstCPs(result, results.at("worstCP"), subject, context);
    result.compared.configurations = 1;
    return result;
}

AeroComparison compareAero(Rocket& rocket, const json& aero, bool randomConfigurationId,
                           const Preferences& preferences)
{
    const QtRocket::Test::DefaultUnitsGuard units;  // checkGeometry() prints lengths
    AeroComparison                          result;
    Mismatches                              file("file");
    compareFileValues(file, aero, rocket);
    result.report += file.report();

    const FlightConfigurationId selected       = rocket.getSelectedConfiguration().getId();
    const json&                 configurations = aero.at("configurations");
    for (std::size_t i = 0; i < configurations.size(); i++)
    {
        const std::string context = std::format("configuration {}", i);
        Mismatches        m(context);
        const json*       results = aeroResults(aero, i);
        if (results == nullptr || std::cmp_greater(i, rocket.getConfigurationCount()))
        {
            m.note("no golden results, or no such configuration in the rocket");
            result.report += m.report();
            continue;
        }
        const FlightConfiguration& config =
            rocket.getFlightConfigurationByIndex(static_cast<int>(i), true);
        rocket.setSelectedConfiguration(config.getId());
        m.integer("index", configurations.at(i).at("index").get<int>(), static_cast<int>(i));
        compareHeader(m, configurations.at(i), config, randomConfigurationId, preferences);
        result.report += m.report();

        const AeroComparison compared = compareResults(*results, rocket, config, context);
        result.compared += compared.compared;
        result.report += compared.report;
        result.otherThetas += compared.otherThetas;
    }
    rocket.setSelectedConfiguration(selected);
    return result;
}

}  // namespace QtRocket::Test
