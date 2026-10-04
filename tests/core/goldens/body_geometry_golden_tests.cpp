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

#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenBodies.h"

namespace
{

using nlohmann::json;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BoundingBox;
using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::SymmetricComponent;
using QtRocket::Test::goldenCoordinate;
using QtRocket::Test::goldenInputNames;
using QtRocket::Test::GoldenMismatches;
using QtRocket::Test::goldenValue;
using QtRocket::Test::loadGoldenGeometry;
using QtRocket::Test::rebuildGoldenBody;

/// Compares the geometry and mass properties of @p actual with the golden @p expected entry.
void compareBody(GoldenMismatches& m, const json& expected, const SymmetricComponent& actual)
{
    const json& details = expected.at("details");
    m.relative("length", goldenValue(expected.at("length")), actual.getLength());
    m.absolute("foreRadius", goldenValue(details.at("foreRadius")), actual.getForeRadius());
    m.absolute("aftRadius", goldenValue(details.at("aftRadius")), actual.getAftRadius());
    m.absolute("maxRadius", goldenValue(details.at("maxRadius")), actual.getMaxRadius());
    m.absolute("innerRadius", goldenValue(details.at("innerRadius")), actual.getInnerRadius());
    m.absolute("thickness", goldenValue(details.at("thickness")), actual.getThickness());

    m.relative("componentVolume", goldenValue(details.at("componentVolume")),
               actual.getComponentVolume());
    m.relative("fullVolume", goldenValue(details.at("fullVolume")), actual.getFullVolume());
    m.relative("componentWetArea", goldenValue(details.at("componentWetArea")),
               actual.getComponentWetArea());
    m.relative("componentPlanformArea", goldenValue(details.at("componentPlanformArea")),
               actual.getComponentPlanformArea());

    m.relative("componentMass", goldenValue(expected.at("componentMass")),
               actual.getComponentMass());
    m.cg("componentCG", goldenCoordinate(expected.at("componentCG")), actual.getComponentCG());
    m.relative("longitudinalUnitInertia", goldenValue(expected.at("longitudinalUnitInertia")),
               actual.getLongitudinalUnitInertia());
    m.relative("rotationalUnitInertia", goldenValue(expected.at("rotationalUnitInertia")),
               actual.getRotationalUnitInertia());

    m.positions("componentBounds", expected.at("componentBounds"), actual.getComponentBounds());
    const BoundingBox box = actual.getInstanceBoundingBox();
    m.position("instanceBoundingBox.min",
               goldenCoordinate(details.at("instanceBoundingBox").at("min")), box.min());
    m.position("instanceBoundingBox.max",
               goldenCoordinate(details.at("instanceBoundingBox").at("max")), box.max());
}

// ===================================================================== per body component

class BodyGeometryGolden : public ::testing::TestWithParam<std::string>
{ };

TEST_P(BodyGeometryGolden, BodyComponentsRebuiltStandalone)
{
    const QtRocket::Result<json> geometry = loadGoldenGeometry(GetParam());
    ASSERT_TRUE(geometry.has_value()) << geometry.error().message;

    int compared = 0;
    for (const json& component : geometry->at("components"))
    {
        const std::unique_ptr<SymmetricComponent> body = rebuildGoldenBody(component);
        if (!body)
        {
            continue;
        }
        GoldenMismatches m(std::format("{} {} \"{}\"", component.at("type").get<std::string>(),
                                       component.at("path").get<std::string>(),
                                       component.at("name").get<std::string>()));
        compareBody(m, component, *body);
        EXPECT_EQ(m.report(), "");
        compared++;
    }
    RecordProperty("bodyComponents", compared);
}

INSTANTIATE_TEST_SUITE_P(Inputs, BodyGeometryGolden, ::testing::ValuesIn(goldenInputNames()),
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
    const std::vector<std::string> names = goldenInputNames();
    ASSERT_EQ(names.size(), 29U);
    int total = 0;
    for (const std::string& name : names)
    {
        const QtRocket::Result<json> geometry = loadGoldenGeometry(name);
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
    const QtRocket::Result<json> geometry = loadGoldenGeometry("testrocket-simple-2-stage");
    ASSERT_TRUE(geometry.has_value()) << geometry.error().message;
    const std::unique_ptr<Rocket> rocket = makeSimple2Stage();

    const json& components = geometry->at("components");
    ASSERT_EQ(components.size(), 5U);
    for (const json& expected : components)
    {
        const std::string      path   = expected.at("path").get<std::string>();
        const RocketComponent* actual = componentAt(*rocket, path);
        ASSERT_NE(actual, nullptr) << path;

        GoldenMismatches m(path);
        m.text("type", expected.at("type").get<std::string>(), QtRocket::className(actual->kind()));
        m.text("name", expected.at("name").get<std::string>(), actual->getName());
        m.relative("length", goldenValue(expected.at("length")), actual->getLength());
        m.position("position", goldenCoordinate(expected.at("position")), actual->getPosition());
        m.positions("componentLocations", expected.at("componentLocations"),
                    actual->getComponentLocations());
        m.relative("componentMass", goldenValue(expected.at("componentMass")),
                   actual->getComponentMass());
        m.cg("componentCG", goldenCoordinate(expected.at("componentCG")), actual->getComponentCG());
        m.relative("longitudinalUnitInertia", goldenValue(expected.at("longitudinalUnitInertia")),
                   actual->getLongitudinalUnitInertia());
        m.relative("rotationalUnitInertia", goldenValue(expected.at("rotationalUnitInertia")),
                   actual->getRotationalUnitInertia());
        m.relative("mass", goldenValue(expected.at("mass")), actual->getMass());
        m.relative("sectionMass", goldenValue(expected.at("sectionMass")),
                   actual->getSectionMass());
        m.cg("cg", goldenCoordinate(expected.at("cg")), actual->getCG());
        m.relative("longitudinalInertia", goldenValue(expected.at("longitudinalInertia")),
                   actual->getLongitudinalInertia());
        m.relative("rotationalInertia", goldenValue(expected.at("rotationalInertia")),
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
        GoldenMismatches           m(std::format("configuration {}", index));
        m.relative("referenceLength", goldenValue(expected.at("referenceLength")),
                   config.getReferenceLength());
        m.relative("referenceArea", goldenValue(expected.at("referenceArea")),
                   config.getReferenceArea());
        m.relative("length", goldenValue(expected.at("length")), config.getLength());
        m.relative("lengthAerodynamic", goldenValue(expected.at("lengthAerodynamic")),
                   config.getLengthAerodynamic());
        const BoundingBox box = config.getBoundingBox();
        m.position("boundingBox.min", goldenCoordinate(expected.at("boundingBox").at("min")),
                   box.min());
        m.position("boundingBox.max", goldenCoordinate(expected.at("boundingBox").at("max")),
                   box.max());
        const BoundingBox aero = config.getBoundingBoxAerodynamic();
        m.position("boundingBoxAerodynamic.min",
                   goldenCoordinate(expected.at("boundingBoxAerodynamic").at("min")), aero.min());
        m.position("boundingBoxAerodynamic.max",
                   goldenCoordinate(expected.at("boundingBoxAerodynamic").at("max")), aero.max());
        EXPECT_EQ(m.report(), "");
    }
}

}  // namespace
