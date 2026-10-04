// Test rocket golden tests: the thirteen rockets of tests/core/rocket/TestRockets.h (the makers
// of OpenRocket's TestRockets.java that return a Rocket, rebuilt call for call from the real
// components) compared with what OpenRocket computes for the Java rockets, in
// tests/data/goldens/testrocket-<name>/geometry.json (tools/openrocket-goldens).
//
// - Every component is compared: its class, name, placement, mass properties (with and without
//   the overrides), bounds and instances, and its "details": what the public getters of its Java
//   class return (shape, radii, shoulders, wall, finish, material, radial and angular position,
//   motor mount and cluster settings, recovery device dimensions, a fin set's dimensions,
//   cross-section, tab, fillets and outlines, a launch lug's radii, the rocket's reference
//   type). These are the constructor arguments and setter values of TestRockets.java, so a
//   fixture that drifts from it shows here. Every entry of "details" has to be compared: one
//   that no comparison reads is reported.
// - Every flight configuration is compared with it selected, as the harness dumps it: its id,
//   name, stages, motors, reference values, lengths, bounds, active components, the components
//   of its instance map and, for every instance of each of them, its number, its location and the
//   transformations of the instance and of its parent instance. Two makers
//   (makeMultiStageEventTestRocket() and makeClusterPods()) give their configuration a new
//   random id, which is then not compared.
// - Not compared: a component's id (a random UUID) and "loadWarnings" (the .ork loader's, empty
//   for a rocket that was built). The file's "schema", "schemaVersion" and "input" are checked by
//   goldens_schema_tests.cpp.
//
// The numbers of components and configurations compared are taken from each golden file: a rocket
// that skips one, or has one more, fails.
//
// Tolerances (plan section 6.4): geometry and mass relative 1e-9; positions and CGs absolute
// 1e-9 m.

#include <algorithm>
#include <cstddef>
#include <format>
#include <functional>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/material/Material.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
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
#include "QtRocket/util/Error.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenGeometry.h"
#include "goldens/GoldenMismatches.h"
#include "rocket/TestRockets.h"

namespace
{

using nlohmann::json;
using QtRocket::BoundingBox;
using QtRocket::Coordinate;
using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::InstanceContext;
using QtRocket::MotorConfiguration;
using QtRocket::MotorMount;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Test::componentAtGoldenPath;
using QtRocket::Test::goldenCoordinate;
using QtRocket::Test::goldenGeometry;
using QtRocket::Test::goldenPathOf;
using QtRocket::Test::goldenValue;
using QtRocket::Test::TestRocketMaker;
using QtRocket::Test::testRocketMakers;

/// The comparison collector of the golden tests.
using Mismatches = QtRocket::Test::GoldenMismatches;

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

/// What a comparison of a rocket's components, or of its configurations, found.
struct Comparison
{
    int         compared{0};  ///< the components (configurations) compared
    std::string report;       ///< the mismatches, empty when everything matched
};

/// Compares every component of @p rocket with its golden entry in @p geometry.
[[nodiscard]] Comparison compareComponents(const Rocket& rocket, const json& geometry)
{
    Comparison result;
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

/// The number of components of @p rocket, itself included.
[[nodiscard]] int componentCount(const Rocket& rocket)
{
    int count = 0;
    for ([[maybe_unused]] const RocketComponent& component : rocket.subtree())
    {
        count++;
    }
    return count;
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

/// Compares every flight configuration of @p rocket with its golden entry in @p geometry, each
/// one selected while it is compared (as the golden harness dumps it). The id of a configuration
/// is compared unless it is one @p maker draws at random.
[[nodiscard]] Comparison compareConfigurations(Rocket& rocket, const json& geometry,
                                               const TestRocketMaker& maker)
{
    const QtRocket::InMemoryPreferences preferences;
    const FlightConfigurationId         selected = rocket.getSelectedConfiguration().getId();
    Comparison                          result;
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
        if (config.getId().isDefaultId() || !maker.randomConfigurationId)
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

/// The index of the selected configuration of @p rocket among its configurations, the default
/// first (the "index" of the golden configurations).
[[nodiscard]] int selectedIndex(const Rocket& rocket)
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

/// The index of the configuration OpenRocket's maker leaves selected, from @p geometry; -2 when
/// its id is none of the golden configurations'.
[[nodiscard]] int goldenSelectedIndex(const json& geometry)
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

/// The number of entries of the list @p key of @p geometry.
[[nodiscard]] int goldenCount(const json& geometry, const char* key)
{
    return static_cast<int>(geometry.at(key).size());
}

// ===================================================================================== tests

/// One test rocket of TestRockets.h against its golden data.
class TestRocketsGolden : public ::testing::TestWithParam<TestRocketMaker>
{ };

TEST_P(TestRocketsGolden, ComponentsAndConfigurations)
{
    const TestRocketMaker&              maker    = GetParam();
    const QtRocket::Result<const json*> geometry = goldenGeometry(maker.input);
    ASSERT_TRUE(geometry.has_value()) << geometry.error().message;
    const json&                   golden = **geometry;
    const std::unique_ptr<Rocket> rocket = maker.make();

    EXPECT_EQ(golden.at("rocketName").get<std::string>(), rocket->getName());

    // Every component of the golden file, and no component beyond them.
    const Comparison components = compareComponents(*rocket, golden);
    EXPECT_EQ(components.report, "");
    EXPECT_EQ(components.compared, goldenCount(golden, "components"));
    EXPECT_EQ(componentCount(*rocket), goldenCount(golden, "components"));

    // The configuration OpenRocket's maker leaves selected, then every configuration.
    EXPECT_EQ(selectedIndex(*rocket), goldenSelectedIndex(golden));
    const Comparison configurations = compareConfigurations(*rocket, golden, maker);
    EXPECT_EQ(configurations.report, "");
    EXPECT_EQ(configurations.compared, goldenCount(golden, "configurations"));
    EXPECT_EQ(rocket->getConfigurationCount() + 1, goldenCount(golden, "configurations"));
}

/// The test name of @p maker: its golden input with '-' as '_'.
[[nodiscard]] std::string makerTestName(const ::testing::TestParamInfo<TestRocketMaker>& info)
{
    std::string name{info.param.input};
    std::ranges::replace(name, '-', '_');
    return name;
}

INSTANTIATE_TEST_SUITE_P(Makers, TestRocketsGolden, ::testing::ValuesIn(testRocketMakers()),
                         makerTestName);

/// What the makers of TestRockets.h cover of the golden test rockets.
struct MakerCoverage
{
    int         goldenInputs{0};    ///< the "testrocket" inputs of the manifest
    int         components{0};      ///< the components the makers' rockets hold
    int         configurations{0};  ///< their configurations, the default ones included
    std::string problems;           ///< an input without a maker, a maker without an input, ...
};

/// Checks every "testrocket" input of the manifest against the makers: each has one, whose Java
/// method is the input's source and whose rocket has as many components and configurations as
/// the input's geometry.json.
[[nodiscard]] MakerCoverage makerCoverage()
{
    MakerCoverage                                          coverage;
    const QtRocket::Result<QtRocket::Test::GoldenManifest> manifest =
        QtRocket::Test::loadGoldenManifest();
    if (!manifest)
    {
        coverage.problems = manifest.error().message;
        return coverage;
    }
    const std::span<const TestRocketMaker> makers = testRocketMakers();
    for (const QtRocket::Test::GoldenInput& input : manifest->inputs)
    {
        if (input.kind != "testrocket")
        {
            continue;
        }
        coverage.goldenInputs++;
        const auto maker =
            std::ranges::find(makers, std::string_view{input.name}, &TestRocketMaker::input);
        const QtRocket::Result<const json*> geometry = goldenGeometry(input.name);
        if (maker == makers.end() || !geometry)
        {
            coverage.problems += std::format("{}: no maker or no geometry\n", input.name);
            continue;
        }
        if (input.source !=
            std::format("info.openrocket.core.util.TestRockets.{}()", maker->method))
        {
            coverage.problems += std::format("{}: made by {}\n", input.name, input.source);
        }
        const std::unique_ptr<Rocket> rocket         = maker->make();
        const int                     components     = componentCount(*rocket);
        const int                     configurations = rocket->getConfigurationCount() + 1;
        if (components != goldenCount(**geometry, "components") ||
            configurations != goldenCount(**geometry, "configurations"))
        {
            coverage.problems += std::format("{}: {} components and {} configurations\n",
                                             input.name, components, configurations);
        }
        coverage.components += components;
        coverage.configurations += configurations;
    }
    return coverage;
}

/// Every test rocket of the golden data has a maker, and every maker compares all the components
/// and configurations of its golden file (the per-rocket tests above compare them one by one;
/// the totals are those of the thirteen geometry.json files).
TEST(TestRocketsGoldenCoverage, EveryGoldenTestRocketIsRebuiltInFull)
{
    const MakerCoverage coverage = makerCoverage();
    EXPECT_EQ(coverage.problems, "");
    EXPECT_EQ(coverage.goldenInputs, 13);
    EXPECT_EQ(static_cast<std::size_t>(coverage.goldenInputs), testRocketMakers().size());
    EXPECT_EQ(coverage.components, 151) << "the components of the thirteen golden test rockets";
    EXPECT_EQ(coverage.configurations, 47) << "their configurations, the default ones included";
}

}  // namespace
