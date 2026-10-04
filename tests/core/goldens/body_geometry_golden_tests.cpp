// Body geometry golden tests: the symmetric body components (nose cones, transitions, body tubes)
// compared with OpenRocket's own values in tests/data/goldens/<input>/geometry.json
// (tools/openrocket-goldens).
//
// BodyGeometryGolden rebuilds every body component of every golden input on its own, from the
// dimensions geometry.json records (automatic radii as OpenRocket settled them), and compares the
// dimensions, volumes, areas, mass, CG, unit inertias, component bounds and instance bounding box.
// A symmetric component's geometry depends on its own fields and its two radii only, so this
// covers the 106 body components of the 29 inputs without rebuilding their rockets. (The test
// rockets are rebuilt whole, and compared in every field, by TestRocketsGolden in
// test_rockets_golden_tests.cpp; makeSimple2Stage(), once rebuilt here, is one of them.)
//
// Tolerances (plan section 6.4): geometry and mass relative 1e-9; positions and CGs absolute
// 1e-9 m.

#include <format>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenBodies.h"
#include "goldens/GoldenGeometry.h"
#include "goldens/GoldenMismatches.h"

namespace
{

using nlohmann::json;
using QtRocket::BoundingBox;
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
    m.relative("foreRadius", goldenValue(details.at("foreRadius")), actual.getForeRadius());
    m.relative("aftRadius", goldenValue(details.at("aftRadius")), actual.getAftRadius());
    m.relative("maxRadius", goldenValue(details.at("maxRadius")), actual.getMaxRadius());
    m.relative("innerRadius", goldenValue(details.at("innerRadius")), actual.getInnerRadius());
    m.relative("thickness", goldenValue(details.at("thickness")), actual.getThickness());

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

}  // namespace
