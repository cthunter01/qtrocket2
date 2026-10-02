// Body geometry golden tests: the symmetric body components (nose cones, transitions, body tubes)
// compared with OpenRocket's own values in tests/data/goldens/<input>/geometry.json
// (tools/openrocket-goldens).
//
// - BodyGeometryGolden rebuilds every body component of every golden input on its own, from the
//   dimensions geometry.json records (automatic radii as OpenRocket settled them), and compares
//   the volumes, areas, mass, CG, unit inertias, component bounds and instance bounding box. A
//   symmetric component's geometry depends on its own fields and its two radii only, so this
//   covers the 106 body components of the 29 inputs without the components that are not
//   ported yet.
// - BodyGeometryGoldenRocket rebuilds TestRockets.makeSimple2Stage(), whose body is made of body
//   tubes only, and compares every component and the configurations' reference values and
//   bounds.
//
// Tolerances (plan section 6.4): geometry and mass relative 1e-9; positions and CGs absolute
// 1e-9 m.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenData.h"

namespace
{

using nlohmann::json;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BoundingBox;
using QtRocket::Coordinate;
using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::Material;
using QtRocket::NoseCone;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::SymmetricComponent;
using QtRocket::Transition;
using QtRocket::TransitionShape;

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
    void positions(std::string_view field, const json& expected,
                   const std::vector<Coordinate>& actual)
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

    void text(std::string_view field, std::string_view expected, std::string_view actual)
    {
        if (expected != actual)
        {
            m_text += std::format("  {}: expected \"{}\", got \"{}\"\n", field, expected, actual);
        }
    }

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

/// Sets a Transition's shoulders, wall and radii to the golden @p details. The shoulder setters
/// compare with MathUtil::equals, so each value is first moved away from the target.
void applyTransitionDetails(Transition& transition, const json& details)
{
    const std::optional<TransitionShape> shape =
        QtRocket::transitionShapeFromName(details.at("shapeType").get<std::string>());
    if (!shape)
    {
        ADD_FAILURE() << "unknown shape " << details.at("shapeType");
        return;
    }
    transition.setShapeType(*shape);
    transition.setShapeParameter(number(details.at("shapeParameter")));
    transition.setClipped(details.at("clipped").get<bool>());
    transition.setForeRadius(number(details.at("foreRadius")), false);
    transition.setAftRadius(number(details.at("aftRadius")), false);
    transition.setThickness(number(details.at("thickness")), false);
    transition.setFilled(details.at("filled").get<bool>());

    const double foreLength = number(details.at("foreShoulderLength"));
    if (foreLength != 0)
    {
        transition.setForeShoulderLength(foreLength);
    }
    transition.setForeShoulderRadius(-1, false);
    transition.setForeShoulderRadius(number(details.at("foreShoulderRadius")), false);
    transition.setForeShoulderThickness(-1);
    transition.setForeShoulderThickness(number(details.at("foreShoulderThickness")));
    transition.setForeShoulderCapped(details.at("foreShoulderCapped").get<bool>());

    const double aftLength = number(details.at("aftShoulderLength"));
    if (aftLength != 0)
    {
        transition.setAftShoulderLength(aftLength);
    }
    transition.setAftShoulderRadius(-1, false);
    transition.setAftShoulderRadius(number(details.at("aftShoulderRadius")), false);
    transition.setAftShoulderThickness(-1);
    transition.setAftShoulderThickness(number(details.at("aftShoulderThickness")));
    transition.setAftShoulderCapped(details.at("aftShoulderCapped").get<bool>());
}

/// The body component the golden @p component describes, built on its own (not in a rocket), with
/// its material; nullptr for a component that is not a body component.
[[nodiscard]] std::unique_ptr<SymmetricComponent> rebuild(const json& component)
{
    const std::string type    = component.at("type").get<std::string>();
    const json&       details = component.at("details");
    const double      length  = number(component.at("length"));

    std::unique_ptr<SymmetricComponent> result;
    if (type == "BodyTube")
    {
        const double radius = number(details.at("outerRadius"));
        if (details.at("filled").get<bool>())
        {
            result = std::make_unique<BodyTube>(length, radius, true);
        }
        else
        {
            result = std::make_unique<BodyTube>(length, radius, number(details.at("thickness")));
        }
    }
    else if (type == "NoseCone")
    {
        // A flipped nose cone (a tail cone) has its base at the front.
        const double fore    = number(details.at("foreRadius"));
        const double aft     = number(details.at("aftRadius"));
        const bool   flipped = fore > 0 && aft == 0;
        auto         nose =
            std::make_unique<NoseCone>(TransitionShape::CONICAL, length, flipped ? fore : aft);
        if (flipped)
        {
            nose->setFlipped(true, false);
        }
        applyTransitionDetails(*nose, details);
        result = std::move(nose);
    }
    else if (type == "Transition")
    {
        auto transition = std::make_unique<Transition>();
        transition->setLength(length);
        applyTransitionDetails(*transition, details);
        result = std::move(transition);
    }
    else
    {
        return nullptr;
    }

    const json& material = details.at("material");
    result->setMaterial(Material::newMaterial(Material::Type::BULK,
                                              material.at("name").get<std::string>(),
                                              number(material.at("density")), true));
    return result;
}

/// Compares the geometry and mass properties of @p actual with the golden @p expected entry.
void compareBody(Mismatches& m, const json& expected, const SymmetricComponent& actual)
{
    const json& details = expected.at("details");
    m.relative("length", number(expected.at("length")), actual.getLength());
    m.absolute("foreRadius", number(details.at("foreRadius")), actual.getForeRadius());
    m.absolute("aftRadius", number(details.at("aftRadius")), actual.getAftRadius());
    m.absolute("maxRadius", number(details.at("maxRadius")), actual.getMaxRadius());
    m.absolute("innerRadius", number(details.at("innerRadius")), actual.getInnerRadius());
    m.absolute("thickness", number(details.at("thickness")), actual.getThickness());

    m.relative("componentVolume", number(details.at("componentVolume")),
               actual.getComponentVolume());
    m.relative("fullVolume", number(details.at("fullVolume")), actual.getFullVolume());
    m.relative("componentWetArea", number(details.at("componentWetArea")),
               actual.getComponentWetArea());
    m.relative("componentPlanformArea", number(details.at("componentPlanformArea")),
               actual.getComponentPlanformArea());

    m.relative("componentMass", number(expected.at("componentMass")), actual.getComponentMass());
    m.cg("componentCG", coordinate(expected.at("componentCG")), actual.getComponentCG());
    m.relative("longitudinalUnitInertia", number(expected.at("longitudinalUnitInertia")),
               actual.getLongitudinalUnitInertia());
    m.relative("rotationalUnitInertia", number(expected.at("rotationalUnitInertia")),
               actual.getRotationalUnitInertia());

    m.positions("componentBounds", expected.at("componentBounds"), actual.getComponentBounds());
    const BoundingBox box = actual.getInstanceBoundingBox();
    m.position("instanceBoundingBox.min", coordinate(details.at("instanceBoundingBox").at("min")),
               box.min());
    m.position("instanceBoundingBox.max", coordinate(details.at("instanceBoundingBox").at("max")),
               box.max());
}

/// The geometry.json of the golden input @p name.
[[nodiscard]] QtRocket::Result<json> loadGeometry(std::string_view name)
{
    const QtRocket::Result<QtRocket::Test::GoldenManifest> manifest =
        QtRocket::Test::loadGoldenManifest();
    if (!manifest)
    {
        return std::unexpected(manifest.error());
    }
    const QtRocket::Test::GoldenInput* input = manifest->find(name);
    if (input == nullptr)
    {
        return std::unexpected(QtRocket::Error{.code    = QtRocket::ErrorCode::NOT_FOUND,
                                               .message = std::format("no input {}", name),
                                               .where   = std::source_location::current()});
    }
    return QtRocket::Test::loadGoldenJson(input->geometry);
}

/// The names of every golden input, from manifest.json.
[[nodiscard]] std::vector<std::string> inputNames()
{
    std::vector<std::string>                               names;
    const QtRocket::Result<QtRocket::Test::GoldenManifest> manifest =
        QtRocket::Test::loadGoldenManifest();
    if (manifest)
    {
        for (const QtRocket::Test::GoldenInput& input : manifest->inputs)
        {
            names.push_back(input.name);
        }
    }
    return names;
}

// ===================================================================== per body component

class BodyGeometryGolden : public ::testing::TestWithParam<std::string>
{ };

TEST_P(BodyGeometryGolden, BodyComponentsRebuiltStandalone)
{
    const QtRocket::Result<json> geometry = loadGeometry(GetParam());
    ASSERT_TRUE(geometry.has_value()) << geometry.error().message;

    int compared = 0;
    for (const json& component : geometry->at("components"))
    {
        const std::unique_ptr<SymmetricComponent> body = rebuild(component);
        if (!body)
        {
            continue;
        }
        Mismatches m(std::format("{} {} \"{}\"", component.at("type").get<std::string>(),
                                 component.at("path").get<std::string>(),
                                 component.at("name").get<std::string>()));
        compareBody(m, component, *body);
        EXPECT_EQ(m.report(), "");
        compared++;
    }
    RecordProperty("bodyComponents", compared);
}

INSTANTIATE_TEST_SUITE_P(Inputs, BodyGeometryGolden, ::testing::ValuesIn(inputNames()),
                         [](const ::testing::TestParamInfo<std::string>& paramInfo) {
                             std::string name = paramInfo.param;
                             for (char& c : name)
                             {
                                 if (c == '-')
                                 {
                                     c = '_';
                                 }
                             }
                             return name;
                         });

/// The number of body components (body tubes, nose cones, transitions) in @p geometry.
[[nodiscard]] int countBodyComponents(const json& geometry)
{
    int count = 0;
    for (const json& component : geometry.at("components"))
    {
        const std::string type = component.at("type").get<std::string>();
        if (type == "BodyTube" || type == "NoseCone" || type == "Transition")
        {
            count++;
        }
    }
    return count;
}

TEST(BodyGeometryGoldenCoverage, EveryInputHasBodyComponents)
{
    const std::vector<std::string> names = inputNames();
    ASSERT_EQ(names.size(), 29U);
    int total = 0;
    for (const std::string& name : names)
    {
        const QtRocket::Result<json> geometry = loadGeometry(name);
        ASSERT_TRUE(geometry.has_value()) << geometry.error().message;
        const int count = countBodyComponents(*geometry);
        EXPECT_GT(count, 0) << name;
        total += count;
    }
    EXPECT_EQ(total, 106) << "the body components the per-input tests rebuild";
}

// =================================================================== Simple 2-stage rocket

/// TestRockets.makeSimple2Stage(): two stages, each holding a body tube 0.1 m long with radius
/// 0.01 m and a 1 mm wall; TEST_FCID_0 selected with every stage active.
[[nodiscard]] std::unique_ptr<Rocket> makeSimple2Stage()
{
    auto                        rocket = std::make_unique<Rocket>();
    const FlightConfigurationId testId0 =
        FlightConfigurationId::fromString("d010716e-ce0e-469d-ae46-190f3653ebbf");
    rocket->createFlightConfiguration(testId0);
    rocket->setName("Simple 2-Stage Rocket");

    const double bodytubeLength    = 0.10;
    const double bodytubeRadius    = 0.01;
    const double bodytubeThickness = 0.001;

    auto& sustainerStage = rocket->addChild(std::make_unique<AxialStage>());
    sustainerStage.setName("Sustainer Stage");
    sustainerStage
        .addChild(std::make_unique<BodyTube>(bodytubeLength, bodytubeRadius, bodytubeThickness))
        .setName("Sustainer Body Tube");

    auto& boosterStage = rocket->addChild(std::make_unique<AxialStage>());
    boosterStage.setName("Booster Stage");
    boosterStage
        .addChild(std::make_unique<BodyTube>(bodytubeLength, bodytubeRadius, bodytubeThickness))
        .setName("Booster Body Tube");

    rocket->setSelectedConfiguration(testId0);
    rocket->getSelectedConfiguration().setAllStages();

    rocket->enableEvents();
    return rocket;
}

/// The component at the golden @p path ("/", "/0", "/0/1", ...) under @p rocket, or nullptr.
[[nodiscard]] const RocketComponent* componentAt(const Rocket& rocket, const std::string& path)
{
    const RocketComponent* component = &rocket;
    std::size_t            start     = 1;
    while (start < path.size())
    {
        std::size_t end = path.find('/', start);
        if (end == std::string::npos)
        {
            end = path.size();
        }
        const std::size_t index = std::stoul(path.substr(start, end - start));
        if (index >= component->getChildCount())
        {
            return nullptr;
        }
        component = &component->getChild(index);
        start     = end + 1;
    }
    return component;
}

TEST(BodyGeometryGoldenRocket, Simple2Stage)
{
    const QtRocket::Result<json> geometry = loadGeometry("testrocket-simple-2-stage");
    ASSERT_TRUE(geometry.has_value()) << geometry.error().message;
    const std::unique_ptr<Rocket> rocket = makeSimple2Stage();

    const json& components = geometry->at("components");
    ASSERT_EQ(components.size(), 5U);
    for (const json& expected : components)
    {
        const std::string      path   = expected.at("path").get<std::string>();
        const RocketComponent* actual = componentAt(*rocket, path);
        ASSERT_NE(actual, nullptr) << path;

        Mismatches m(path);
        m.text("type", expected.at("type").get<std::string>(), QtRocket::className(actual->kind()));
        m.text("name", expected.at("name").get<std::string>(), actual->getName());
        m.relative("length", number(expected.at("length")), actual->getLength());
        m.position("position", coordinate(expected.at("position")), actual->getPosition());
        m.positions("componentLocations", expected.at("componentLocations"),
                    actual->getComponentLocations());
        m.relative("componentMass", number(expected.at("componentMass")),
                   actual->getComponentMass());
        m.cg("componentCG", coordinate(expected.at("componentCG")), actual->getComponentCG());
        m.relative("longitudinalUnitInertia", number(expected.at("longitudinalUnitInertia")),
                   actual->getLongitudinalUnitInertia());
        m.relative("rotationalUnitInertia", number(expected.at("rotationalUnitInertia")),
                   actual->getRotationalUnitInertia());
        m.relative("mass", number(expected.at("mass")), actual->getMass());
        m.relative("sectionMass", number(expected.at("sectionMass")), actual->getSectionMass());
        m.cg("cg", coordinate(expected.at("cg")), actual->getCG());
        m.relative("longitudinalInertia", number(expected.at("longitudinalInertia")),
                   actual->getLongitudinalInertia());
        m.relative("rotationalInertia", number(expected.at("rotationalInertia")),
                   actual->getRotationalInertia());
        m.positions("componentBounds", expected.at("componentBounds"),
                    actual->getComponentBounds());
        EXPECT_EQ(expected.at("instanceCount").get<int>(), actual->getInstanceCount()) << path;

        if (const auto* body = dynamic_cast<const SymmetricComponent*>(actual))
        {
            compareBody(m, expected, *body);
        }
        EXPECT_EQ(m.report(), "");
    }

    for (const json& expected : geometry->at("configurations"))
    {
        const int                  index  = expected.at("index").get<int>();
        const FlightConfiguration& config = rocket->getFlightConfigurationByIndex(index, true);
        Mismatches                 m(std::format("configuration {}", index));
        m.relative("referenceLength", number(expected.at("referenceLength")),
                   config.getReferenceLength());
        m.relative("referenceArea", number(expected.at("referenceArea")),
                   config.getReferenceArea());
        m.relative("length", number(expected.at("length")), config.getLength());
        m.relative("lengthAerodynamic", number(expected.at("lengthAerodynamic")),
                   config.getLengthAerodynamic());
        const BoundingBox box = config.getBoundingBox();
        m.position("boundingBox.min", coordinate(expected.at("boundingBox").at("min")), box.min());
        m.position("boundingBox.max", coordinate(expected.at("boundingBox").at("max")), box.max());
        const BoundingBox aero = config.getBoundingBoxAerodynamic();
        m.position("boundingBoxAerodynamic.min",
                   coordinate(expected.at("boundingBoxAerodynamic").at("min")), aero.min());
        m.position("boundingBoxAerodynamic.max",
                   coordinate(expected.at("boundingBoxAerodynamic").at("max")), aero.max());
        EXPECT_EQ(m.report(), "");
    }
}

}  // namespace
