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
//
// The comparisons themselves (compareComponents() and compareConfigurations()) are in
// GoldenDesign.h, for the tests of other designs to share.

#include <algorithm>
#include <cstddef>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenDesign.h"
#include "goldens/GoldenGeometry.h"
#include "rocket/TestRockets.h"

namespace
{

using nlohmann::json;
using QtRocket::Rocket;
using QtRocket::Test::compareComponents;
using QtRocket::Test::compareConfigurations;
using QtRocket::Test::componentCount;
using QtRocket::Test::GeometryComparison;
using QtRocket::Test::goldenCount;
using QtRocket::Test::goldenGeometry;
using QtRocket::Test::goldenSelectedIndex;
using QtRocket::Test::selectedIndex;
using QtRocket::Test::TestRocketMaker;
using QtRocket::Test::testRocketMakers;

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
    const GeometryComparison components = compareComponents(*rocket, golden);
    EXPECT_EQ(components.report, "");
    EXPECT_EQ(components.compared, goldenCount(golden, "components"));
    EXPECT_EQ(componentCount(*rocket), goldenCount(golden, "components"));

    // The configuration OpenRocket's maker leaves selected, then every configuration.
    EXPECT_EQ(selectedIndex(*rocket), goldenSelectedIndex(golden));
    const QtRocket::InMemoryPreferences preferences;  // the golden names are an empty store's
    const GeometryComparison            configurations =
        compareConfigurations(*rocket, golden, maker.randomConfigurationId, preferences);
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
