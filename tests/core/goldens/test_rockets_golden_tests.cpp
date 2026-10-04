// Test rocket golden tests: the rockets of tests/core/rocket/TestRockets.h (OpenRocket's
// TestRockets.makeEstesAlphaIII(), makeBeta(), makeFalcon9Heavy() and makeSimple2Stage(), rebuilt
// call for call from the real components) compared with what OpenRocket computes for the Java
// rockets, in tests/data/goldens/testrocket-<name>/geometry.json (tools/openrocket-goldens).
//
// - Every component that is a real class is compared: its class, name, placement, mass
//   properties (with and without the overrides), bounds and instances, and its "details": what
//   the public getters of its Java class return (shape, radii, shoulders, wall, finish, material,
//   radial and angular position, motor mount and cluster settings, recovery device dimensions,
//   the rocket's reference type). These are the constructor arguments and setter values of
//   TestRockets.java, so a fixture that drifts from it shows here. Every entry of "details" has
//   to be compared: one that no comparison reads is reported.
// - The fin sets and launch lugs are doubles that carry OpenRocket's values (HOOK(fins-lugs),
//   see TestRockets.h). They are counted apart: what they carry (placement, mass properties,
//   instances) is checked against the same entries, so that a wrong constant shows here, but a
//   double has no shape, so its bounds and its details are not compared.
// - Every flight configuration is compared with it selected, as the harness dumps it: its id,
//   name, stages, motors, reference values, lengths, active components and instances. A rocket
//   with doubles is compared in the x extent of its bounds only (the y and z extents are the
//   fins' and lugs'), and the instances of its doubles are left out.
//
// Tolerances (plan section 6.4): geometry and mass relative 1e-9; positions and CGs absolute
// 1e-9 m.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/material/Material.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/InstanceContext.h"
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
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AnglePositionable.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"
#include "goldens/GoldenData.h"
#include "rocket/TestComponent.h"
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
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestComponent;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::TestFalcon9Heavy;
using QtRocket::Test::TestSimple2Stage;

/// Geometry and mass values: relative tolerance.
constexpr double kRelative = 1e-9;
/// Positions and CGs: absolute tolerance, in m.
constexpr double kAbsolute = 1e-9;

/// A golden number (also "NaN", "Infinity", "-Infinity").
[[nodiscard]] double number(const json& value)
{
    const std::optional<double> parsed = QtRocket::Test::goldenNumber(value);
    return parsed.value_or(std::numeric_limits<double>::quiet_NaN());
}

/// A golden [x, y, z] or [x, y, z, w].
[[nodiscard]] Coordinate coordinate(const json& value)
{
    return Coordinate{number(value.at(0)), number(value.at(1)), number(value.at(2)),
                      value.size() > 3 ? number(value.at(3)) : 0.0};
}

/// Collects the differences between golden and computed values, one line each.
class Mismatches
{
public:
    explicit Mismatches(std::string context) : m_context(std::move(context)) { }

    /// @p actual within kRelative of @p expected, relative to the larger magnitude (exact for 0).
    void relative(std::string_view field, double expected, double actual)
    {
        if (std::isnan(expected) && std::isnan(actual))
        {
            return;
        }
        const double scale = std::max(std::abs(expected), std::abs(actual));
        if (!(std::abs(actual - expected) <= kRelative * scale))
        {
            add(field, expected, actual);
        }
    }

    /// @p actual within kAbsolute of @p expected.
    void absolute(std::string_view field, double expected, double actual)
    {
        if (!(std::abs(actual - expected) <= kAbsolute))
        {
            add(field, expected, actual);
        }
    }

    /// A position: each of x, y and z within kAbsolute.
    void position(std::string_view field, const Coordinate& expected, const Coordinate& actual)
    {
        absolute(std::format("{}.x", field), expected.x, actual.x);
        absolute(std::format("{}.y", field), expected.y, actual.y);
        absolute(std::format("{}.z", field), expected.z, actual.z);
    }

    /// A CG: the position within kAbsolute, the mass (weight) within kRelative.
    void cg(std::string_view field, const Coordinate& expected, const Coordinate& actual)
    {
        position(field, expected, actual);
        relative(std::format("{}.weight", field), expected.weight, actual.weight);
    }

    /// A list of positions of the same length.
    void positions(std::string_view field, const json& expected, std::span<const Coordinate> actual)
    {
        if (expected.size() != actual.size())
        {
            m_text += std::format("  {}: {} points expected, {} computed\n", field, expected.size(),
                                  actual.size());
            return;
        }
        for (std::size_t i = 0; i < actual.size(); i++)
        {
            position(std::format("{}[{}]", field, i), coordinate(expected.at(i)), actual[i]);
        }
    }

    /// A list of angles of the same length, each within kAbsolute (rad).
    void angles(std::string_view field, const json& expected, std::span<const double> actual)
    {
        if (expected.size() != actual.size())
        {
            m_text += std::format("  {}: {} angles expected, {} computed\n", field, expected.size(),
                                  actual.size());
            return;
        }
        for (std::size_t i = 0; i < actual.size(); i++)
        {
            absolute(std::format("{}[{}]", field, i), number(expected.at(i)), actual[i]);
        }
    }

    void text(std::string_view field, std::string_view expected, std::string_view actual)
    {
        if (expected != actual)
        {
            m_text += std::format("  {}: expected \"{}\", got \"{}\"\n", field, expected, actual);
        }
    }

    void integer(std::string_view field, long long expected, long long actual)
    {
        if (expected != actual)
        {
            m_text += std::format("  {}: expected {}, got {}\n", field, expected, actual);
        }
    }

    void boolean(std::string_view field, bool expected, bool actual)
    {
        if (expected != actual)
        {
            m_text += std::format("  {}: expected {}, got {}\n", field, expected, actual);
        }
    }

    /// Records that @p what is missing or of another kind than expected.
    void missing(std::string_view what) { m_text += std::format("  {}\n", what); }

    /// The report, empty when everything matched.
    [[nodiscard]] std::string report() const
    {
        return m_text.empty() ? std::string{} : m_context + ":\n" + m_text;
    }

private:
    void add(std::string_view field, double expected, double actual)
    {
        m_text += std::format("  {}: expected {}, got {} (difference {})\n", field, expected,
                              actual, actual - expected);
    }

    std::string m_context;
    std::string m_text;
};

/// The geometry.json of the golden input @p name ("testrocket-beta").
[[nodiscard]] QtRocket::Result<json> loadGeometry(const std::string& name)
{
    return QtRocket::Test::loadGoldenJson(name + "/geometry.json");
}

/// The component at the golden @p path ("/", "/0", "/0/1", ...) under @p rocket, or nullptr.
[[nodiscard]] RocketComponent* componentAt(Rocket& rocket, std::string_view path)
{
    RocketComponent* component = &rocket;
    std::size_t      start     = 1;
    while (start < path.size())
    {
        std::size_t end = path.find('/', start);
        if (end == std::string_view::npos)
        {
            end = path.size();
        }
        const std::optional<int> index =
            QtRocket::Strings::parseInt(path.substr(start, end - start));
        if (!index || *index < 0 || std::cmp_greater_equal(*index, component->getChildCount()))
        {
            return nullptr;
        }
        component = &component->getChild(static_cast<std::size_t>(*index));
        start     = end + 1;
    }
    return component;
}

/// Whether @p component is one of the doubles that stand for a fin set or a launch lug.
// HOOK(fins-lugs): no component is a double once tier 6b has replaced them.
[[nodiscard]] bool isDouble(const RocketComponent& component)
{
    return dynamic_cast<const TestComponent*>(&component) != nullptr;
}

/// Compares the class, the name and the placement of @p actual with the golden @p expected.
void comparePlacement(Mismatches& m, const json& expected, const RocketComponent& actual)
{
    m.text("type", expected.at("type").get<std::string>(), QtRocket::className(actual.kind()));
    m.text("name", expected.at("name").get<std::string>(), actual.getName());
    m.integer("stageNumber", expected.at("stageNumber").get<int>(), actual.getStageNumber());
    m.relative("length", number(expected.at("length")), actual.getLength());
    m.text("axialMethod", expected.at("axialMethod").get<std::string>(),
           QtRocket::axialMethodName(actual.getAxialMethod()));
    m.absolute("axialOffset", number(expected.at("axialOffset")), actual.getAxialOffset());
    m.position("position", coordinate(expected.at("position")), actual.getPosition());
    m.boolean("isAerodynamic", expected.at("isAerodynamic").get<bool>(), actual.isAerodynamic());
    m.boolean("isMassive", expected.at("isMassive").get<bool>(), actual.isMassive());
}

/// Compares the mass properties of @p actual, without and with its overrides.
void compareMass(Mismatches& m, const json& expected, const RocketComponent& actual)
{
    m.relative("componentMass", number(expected.at("componentMass")), actual.getComponentMass());
    m.cg("componentCG", coordinate(expected.at("componentCG")), actual.getComponentCG());
    m.relative("longitudinalUnitInertia", number(expected.at("longitudinalUnitInertia")),
               actual.getLongitudinalUnitInertia());
    m.relative("rotationalUnitInertia", number(expected.at("rotationalUnitInertia")),
               actual.getRotationalUnitInertia());

    m.relative("mass", number(expected.at("mass")), actual.getMass());
    m.relative("sectionMass", number(expected.at("sectionMass")), actual.getSectionMass());
    m.cg("cg", coordinate(expected.at("cg")), actual.getCG());
    m.relative("longitudinalInertia", number(expected.at("longitudinalInertia")),
               actual.getLongitudinalInertia());
    m.relative("rotationalInertia", number(expected.at("rotationalInertia")),
               actual.getRotationalInertia());

    const json& overrides = expected.at("overrides");
    m.boolean("massOverridden", overrides.at("massOverridden").get<bool>(),
              actual.isMassOverridden());
    if (actual.isMassOverridden() && !overrides.at("overrideMass").is_null())
    {
        m.relative("overrideMass", number(overrides.at("overrideMass")), actual.getOverrideMass());
    }
    m.boolean("cgOverridden", overrides.at("cgOverridden").get<bool>(), actual.isCGOverridden());
    if (actual.isCGOverridden() && !overrides.at("overrideCGX").is_null())
    {
        m.absolute("overrideCGX", number(overrides.at("overrideCGX")), actual.getOverrideCGX());
    }
    m.boolean("cdOverridden", overrides.at("cdOverridden").get<bool>(), actual.isCDOverridden());
    if (actual.isCDOverridden() && !overrides.at("overrideCD").is_null())
    {
        m.relative("overrideCD", number(overrides.at("overrideCD")), actual.getOverrideCD());
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
    // None of the rebuilt rockets overrides its subcomponents, so no component is overridden by
    // another one (the golden entries are null).
    m.boolean("massOverriddenBy", !overrides.at("massOverriddenBy").is_null(),
              actual.getMassOverriddenBy() != nullptr);
    m.boolean("cgOverriddenBy", !overrides.at("cgOverriddenBy").is_null(),
              actual.getCGOverriddenBy() != nullptr);
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
            m_mismatches->relative(field(key), number(*expected), actual);
        }
    }

    /// A radius, a length, an offset or an angle: within kAbsolute.
    void absolute(std::string_view key, double actual)
    {
        if (const json* expected = read(key))
        {
            m_mismatches->absolute(field(key), number(*expected), actual);
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
            m_mismatches->relative(name + ".density", number(expected->at("density")),
                                   actual.getDensity());
        }
    }

    /// A bounding box: its corners (an empty box is +-DBL_MAX in both).
    void box(std::string_view key, const BoundingBox& actual)
    {
        if (const json* expected = read(key))
        {
            const std::string name = field(key);
            m_mismatches->position(name + ".min", coordinate(expected->at("min")), actual.min());
            m_mismatches->position(name + ".max", coordinate(expected->at("max")), actual.max());
        }
    }

    /// Reports every entry of the golden details that no comparison read.
    void unread() const
    {
        for (const auto& [key, value] : m_details->items())
        {
            if (!m_read.contains(key))
            {
                m_mismatches->missing(std::format("details.{}: not compared", key));
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
            m_mismatches->missing(std::format("details.{}: no golden entry", key));
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
        d.absolute("instanceSeparation", line->getInstanceSeparation());
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
    d.absolute("foreRadius", actual.getForeRadius());
    d.absolute("aftRadius", actual.getAftRadius());
    d.absolute("maxRadius", actual.getMaxRadius());
    d.absolute("innerRadius", actual.getInnerRadius());
    d.absolute("thickness", actual.getThickness());
    d.boolean("filled", actual.isFilled());
    d.text("finish", QtRocket::finishName(actual.getFinish()));
    d.material("material", actual.getMaterial());

    if (const auto* transition = dynamic_cast<const QtRocket::Transition*>(&actual))
    {
        d.text("shapeType", QtRocket::transitionShapeName(transition->getShapeType()));
        d.absolute("shapeParameter", transition->getShapeParameter());
        d.boolean("clipped", transition->isClipped());
        d.absolute("foreShoulderRadius", transition->getForeShoulderRadius());
        d.absolute("foreShoulderLength", transition->getForeShoulderLength());
        d.absolute("foreShoulderThickness", transition->getForeShoulderThickness());
        d.boolean("foreShoulderCapped", transition->isForeShoulderCapped());
        d.absolute("aftShoulderRadius", transition->getAftShoulderRadius());
        d.absolute("aftShoulderLength", transition->getAftShoulderLength());
        d.absolute("aftShoulderThickness", transition->getAftShoulderThickness());
        d.boolean("aftShoulderCapped", transition->isAftShoulderCapped());
    }
    if (const auto* tube = dynamic_cast<const QtRocket::BodyTube*>(&actual))
    {
        d.absolute("outerRadius", tube->getOuterRadius());
        d.absolute("motorOverhang", tube->getMotorOverhang());
        d.text("clusterConfiguration", tube->getClusterConfiguration().getXmlName());
    }
}

/// An inner tube, engine block, centering ring or coupler: its radii, wall, radial position and
/// material; an inner tube's mount and cluster settings.
void compareRingDetails(Details& d, const QtRocket::RingComponent& actual)
{
    d.absolute("outerRadius", actual.getOuterRadius());
    d.absolute("innerRadius", actual.getInnerRadius());
    d.absolute("thickness", actual.getThickness());
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
    d.absolute("radius", actual.getRadius());
    d.absolute("radialPosition", actual.getRadialPosition());
    d.absolute("radialDirection", actual.getRadialDirection());
    if (const auto* chute = dynamic_cast<const QtRocket::Parachute*>(&actual))
    {
        d.absolute("diameter", chute->getDiameter());
        d.relative("cd", chute->getCD());
        d.integer("lineCount", chute->getLineCount());
        d.absolute("lineLength", chute->getLineLength());
        d.material("material", chute->getMaterial());
    }
    if (const auto* cord = dynamic_cast<const QtRocket::ShockCord*>(&actual))
    {
        d.absolute("cordLength", cord->getCordLength());
        d.material("material", cord->getMaterial());
    }
}

/// Compares the golden details of @p expected with @p actual, by the classes @p actual is of
/// (the C++ counterpart of the harness's reflection), then reports the entries left over.
void compareDetails(Mismatches& m, const json& expected, const RocketComponent& actual)
{
    Details d(m, expected.at("details"));
    compareCommonDetails(d, actual);
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
        d.absolute("customReferenceLength", rocket->getCustomReferenceLength());
        d.boolean("perfectFinish", rocket->isPerfectFinish());
    }
    d.unread();
}

/// What a comparison of a rocket's components found.
struct ComponentComparison
{
    int         compared{0};  ///< the components that are real classes, compared in full
    int         doubles{0};   ///< the fin sets and launch lugs (HOOK(fins-lugs))
    std::string report;       ///< the mismatches, empty when everything matched
};

/// Compares every component of @p rocket with its golden entry in @p geometry: a real class in
/// full, a double in what it carries.
[[nodiscard]] ComponentComparison compareComponents(Rocket& rocket, const json& geometry)
{
    ComponentComparison result;
    for (const json& expected : geometry.at("components"))
    {
        const auto path = expected.at("path").get<std::string>();
        Mismatches m(std::format("{} {} \"{}\"", expected.at("type").get<std::string>(), path,
                                 expected.at("name").get<std::string>()));
        const RocketComponent* actual = componentAt(rocket, path);
        if (actual == nullptr)
        {
            m.missing("no such component");
        }
        else
        {
            comparePlacement(m, expected, *actual);
            compareMass(m, expected, *actual);
            compareInstances(m, expected, *actual);
            if (isDouble(*actual))
            {
                // HOOK(fins-lugs): a double has no shape; tier 6b compares the bounds and the
                // details too.
                result.doubles++;
            }
            else
            {
                m.positions("componentBounds", expected.at("componentBounds"),
                            actual->getComponentBounds());
                compareDetails(m, expected, *actual);
                result.compared++;
            }
        }
        result.report += m.report();
    }
    return result;
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
        m.missing(std::format("activeStages: expected {} stages, got {}", expectedStages.size(),
                              activeStages.size()));
    }
    m.boolean("hasMotors", expected.at("hasMotors").get<bool>(), config.hasMotors());
    m.boolean("hasRecoveryDevice", expected.at("hasRecoveryDevice").get<bool>(),
              config.hasRecoveryDevice());
}

/// Compares the motor in the mount at the golden path with the golden @p expected motor.
void compareMotor(Mismatches& m, const json& expected, Rocket& rocket,
                  const FlightConfiguration& config, const QtRocket::Preferences& preferences)
{
    const auto        path  = expected.at("mount").get<std::string>();
    const std::string field = std::format("motor in {}", path);
    const auto*       mount = dynamic_cast<const MotorMount*>(componentAt(rocket, path));
    if (mount == nullptr)
    {
        m.missing(field + ": not a motor mount");
        return;
    }
    const MotorConfiguration& motor = mount->getMotorConfig(config.getId());
    if (motor.isEmpty())
    {
        m.missing(field + ": no motor");
        return;
    }
    m.text(field + " motorName", expected.at("motorName").get<std::string>(),
           motor.toMotorName(preferences));
    m.text(field + " designation", expected.at("designation").get<std::string>(),
           motor.getMotor()->getDesignation());
    m.text(field + " digest", expected.at("digest").get<std::string>(),
           motor.getMotor()->getDigest());
    m.relative(field + " ejectionDelay", number(expected.at("ejectionDelay")),
               motor.getEjectionDelay());
    m.relative(field + " nozzleExitDiameter", number(expected.at("nozzleExitDiameter")),
               motor.getNozzleExitDiameter());
    m.text(field + " ignitionEvent", expected.at("ignitionEvent").get<std::string>(),
           QtRocket::name(motor.getIgnitionEvent()));
    m.relative(field + " ignitionDelay", number(expected.at("ignitionDelay")),
               motor.getIgnitionDelay());
    m.integer(field + " motorCount", expected.at("motorCount").get<int>(), mount->getMotorCount());
    m.integer(field + " motorCountIncludingAssemblyCopies",
              expected.at("motorCountIncludingAssemblyCopies").get<int>(),
              mount->getMotorCountIncludingAssemblyCopies());
    m.absolute(field + " motorOverhang", number(expected.at("motorOverhang")),
               mount->getMotorOverhang());
    m.position(field + " position", coordinate(expected.at("position")),
               mount->getMotorPosition(config.getId()));
}

/// Compares the reference values, the lengths and the bounds of @p config. With @p fullBounds
/// the whole boxes are compared, else their x extent only (the rocket has fin and lug doubles).
void compareExtent(Mismatches& m, const json& expected, const FlightConfiguration& config,
                   bool fullBounds)
{
    m.relative("referenceLength", number(expected.at("referenceLength")),
               config.getReferenceLength());
    m.relative("referenceArea", number(expected.at("referenceArea")), config.getReferenceArea());
    m.relative("length", number(expected.at("length")), config.getLength());
    m.relative("lengthAerodynamic", number(expected.at("lengthAerodynamic")),
               config.getLengthAerodynamic());

    const BoundingBox box      = config.getBoundingBox();
    const BoundingBox aero     = config.getBoundingBoxAerodynamic();
    const json&       boxJson  = expected.at("boundingBox");
    const json&       aeroJson = expected.at("boundingBoxAerodynamic");
    if (fullBounds)
    {
        m.position("boundingBox.min", coordinate(boxJson.at("min")), box.min());
        m.position("boundingBox.max", coordinate(boxJson.at("max")), box.max());
        m.position("boundingBoxAerodynamic.min", coordinate(aeroJson.at("min")), aero.min());
        m.position("boundingBoxAerodynamic.max", coordinate(aeroJson.at("max")), aero.max());
        return;
    }
    // HOOK(fins-lugs): the y and z extents are the fins' and the lugs'.
    m.absolute("boundingBox.min.x", coordinate(boxJson.at("min")).x, box.min().x);
    m.absolute("boundingBox.max.x", coordinate(boxJson.at("max")).x, box.max().x);
    m.absolute("boundingBoxAerodynamic.min.x", coordinate(aeroJson.at("min")).x, aero.min().x);
    m.absolute("boundingBoxAerodynamic.max.x", coordinate(aeroJson.at("max")).x, aero.max().x);
}

/// Compares the active components of @p config and the instances of those that are real
/// classes: their numbers and their locations in the rocket.
void compareActiveInstances(Mismatches& m, const json& expected, Rocket& rocket,
                            const FlightConfiguration& config)
{
    std::vector<const RocketComponent*> expectedActive;
    for (const json& path : expected.at("activeComponents"))
    {
        expectedActive.push_back(componentAt(rocket, path.get<std::string>()));
    }
    std::vector<const RocketComponent*> active;
    for (const RocketComponent* component : config.getAllActiveComponents())
    {
        active.push_back(component);
    }
    if (active != expectedActive)
    {
        m.missing(std::format("activeComponents: expected {} in tree order, got {}",
                              expectedActive.size(), active.size()));
    }

    for (const json& entry : expected.at("instances"))
    {
        const auto             path      = entry.at("path").get<std::string>();
        const RocketComponent* component = componentAt(rocket, path);
        if (component == nullptr || isDouble(*component))
        {
            continue;
        }
        const std::span<const InstanceContext> contexts =
            config.getActiveInstances().getInstanceContexts(*component);
        const json& instances = entry.at("instances");
        if (contexts.size() != instances.size())
        {
            m.missing(std::format("instances of {}: expected {}, got {}", path, instances.size(),
                                  contexts.size()));
            continue;
        }
        for (std::size_t i = 0; i < contexts.size(); i++)
        {
            const std::string field = std::format("instance {} of {}", i, path);
            m.integer(field + " number", instances.at(i).at("instanceNumber").get<int>(),
                      contexts[i].instanceNumber);
            m.position(field + " location", coordinate(instances.at(i).at("location")),
                       contexts[i].getLocation());
        }
    }
}

/// Compares every flight configuration of @p rocket with its golden entry in @p geometry, each
/// one selected while it is compared (as the golden harness dumps it); returns the mismatches.
[[nodiscard]] std::string compareConfigurations(Rocket& rocket, const json& geometry,
                                                bool fullBounds)
{
    const QtRocket::InMemoryPreferences preferences;
    const FlightConfigurationId         selected = rocket.getSelectedConfiguration().getId();
    std::string                         report;
    for (const json& expected : geometry.at("configurations"))
    {
        const int                  index  = expected.at("index").get<int>();
        const FlightConfiguration& config = rocket.getFlightConfigurationByIndex(index, true);
        rocket.setSelectedConfiguration(config.getId());

        Mismatches m(std::format("configuration {}", index));
        m.text("id", expected.at("id").get<std::string>(), config.getId().toString());
        m.boolean("isDefault", expected.at("isDefault").get<bool>(), config.getId().isDefaultId());
        m.text("name", expected.at("name").get<std::string>(), config.getName(preferences));
        compareStages(m, expected, config);
        m.integer("motors", static_cast<long long>(expected.at("motors").size()),
                  static_cast<long long>(config.getActiveMotors().size()));
        for (const json& motor : expected.at("motors"))
        {
            compareMotor(m, motor, rocket, config, preferences);
        }
        compareExtent(m, expected, config, fullBounds);
        compareActiveInstances(m, expected, rocket, config);
        report += m.report();
    }
    rocket.setSelectedConfiguration(selected);
    return report;
}

/// Expects @p rocket to be the golden input @p input: its components (@p components of them
/// real classes, @p doubles of them doubles), then its configurations.
void expectGoldenRocket(Rocket& rocket, const std::string& input, int components, int doubles)
{
    const QtRocket::Result<json> geometry = loadGeometry(input);
    ASSERT_TRUE(geometry.has_value()) << geometry.error().message;

    EXPECT_EQ(geometry->at("rocketName").get<std::string>(), rocket.getName());
    const ComponentComparison comparison = compareComponents(rocket, *geometry);
    EXPECT_EQ(comparison.report, "");
    EXPECT_EQ(comparison.compared, components);
    EXPECT_EQ(comparison.doubles, doubles);

    EXPECT_EQ(compareConfigurations(rocket, *geometry, doubles == 0), "");
}

/// The number of real components and of doubles of each rebuilt rocket.
// HOOK(fins-lugs): with the real fin sets and launch lugs every component is compared (10, 17,
// 16 and 5) and no double is left.
constexpr int kAlphaComponents  = 8;
constexpr int kAlphaDoubles     = 2;
constexpr int kBetaComponents   = 13;
constexpr int kBetaDoubles      = 4;
constexpr int kFalconComponents = 15;
constexpr int kFalconDoubles    = 1;
constexpr int kSimpleComponents = 5;
constexpr int kSimpleDoubles    = 0;

TEST(TestRocketsGolden, EstesAlphaIII)
{
    const TestEstesAlphaIII alpha;
    expectGoldenRocket(*alpha.rocket, "testrocket-estes-alpha-iii", kAlphaComponents,
                       kAlphaDoubles);
}

TEST(TestRocketsGolden, Beta)
{
    const TestBeta beta;
    expectGoldenRocket(*beta.rocket, "testrocket-beta", kBetaComponents, kBetaDoubles);
}

TEST(TestRocketsGolden, Falcon9Heavy)
{
    const TestFalcon9Heavy f9h;
    expectGoldenRocket(*f9h.rocket, "testrocket-falcon-9-heavy", kFalconComponents, kFalconDoubles);
}

TEST(TestRocketsGolden, Simple2Stage)
{
    const TestSimple2Stage simple;
    expectGoldenRocket(*simple.rocket, "testrocket-simple-2-stage", kSimpleComponents,
                       kSimpleDoubles);
}

/// The selected configuration of each rebuilt rocket is the one OpenRocket's maker leaves
/// selected.
TEST(TestRocketsGolden, SelectedConfigurations)
{
    const auto selectedOf = [](const std::string& input) {
        const QtRocket::Result<json> geometry = loadGeometry(input);
        return geometry ? geometry->at("selectedConfiguration").get<std::string>() : std::string{};
    };
    const auto isSelected = [&selectedOf](const Rocket& rocket, const std::string& input) {
        const FlightConfigurationId& id = rocket.getSelectedConfiguration().getId();
        // The default configuration's id is the same constant in every rocket.
        return id.isDefaultId() ? FlightConfigurationId::fromString(selectedOf(input)).isDefaultId()
                                : id == FlightConfigurationId::fromString(selectedOf(input));
    };
    EXPECT_TRUE(isSelected(*TestEstesAlphaIII{}.rocket, "testrocket-estes-alpha-iii"));
    EXPECT_TRUE(isSelected(*TestBeta{}.rocket, "testrocket-beta"));
    EXPECT_TRUE(isSelected(*TestFalcon9Heavy{}.rocket, "testrocket-falcon-9-heavy"));
    EXPECT_TRUE(isSelected(*TestSimple2Stage{}.rocket, "testrocket-simple-2-stage"));
}

/// The number of fin sets and launch lugs among the golden @p components.
[[nodiscard]] int countFinsAndLugs(const json& components)
{
    int count = 0;
    for (const json& component : components)
    {
        const auto type = component.at("type").get<std::string>();
        if (type == "TrapezoidFinSet" || type == "LaunchLug")
        {
            count++;
        }
    }
    return count;
}

/// Expects the golden input @p input to hold @p doubles fin sets and launch lugs and
/// @p components other components.
void expectGoldenCounts(const std::string& input, int components, int doubles)
{
    const QtRocket::Result<json> geometry = loadGeometry(input);
    ASSERT_TRUE(geometry.has_value()) << geometry.error().message;
    const json& all        = geometry->at("components");
    const int   finsOrLugs = countFinsAndLugs(all);
    EXPECT_EQ(finsOrLugs, doubles) << input;
    EXPECT_EQ(static_cast<int>(all.size()) - finsOrLugs, components) << input;
}

/// How many components of each rocket the golden tests compare: every component of the golden
/// file is either compared in full or one of the known doubles.
TEST(TestRocketsGoldenCoverage, ComponentsComparedPerRocket)
{
    expectGoldenCounts("testrocket-estes-alpha-iii", kAlphaComponents, kAlphaDoubles);
    expectGoldenCounts("testrocket-beta", kBetaComponents, kBetaDoubles);
    expectGoldenCounts("testrocket-falcon-9-heavy", kFalconComponents, kFalconDoubles);
    expectGoldenCounts("testrocket-simple-2-stage", kSimpleComponents, kSimpleDoubles);
    EXPECT_EQ(kAlphaComponents + kBetaComponents + kFalconComponents + kSimpleComponents, 41)
        << "the components the four rocket tests compare in full";
    EXPECT_EQ(kAlphaDoubles + kBetaDoubles + kFalconDoubles + kSimpleDoubles, 7)
        << "the fin sets and launch lugs";
}

}  // namespace
