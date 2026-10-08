#include "QtRocket/file/openrocket/GravityHandler.h"

#include <array>
#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"
#include "file/openrocket/ConditionsTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// The cases are <conditions> elements with a <gravity> element, run through
// SimulationConditionsHandler as the loader runs them; their expectations are what OpenRocket
// makes of the same elements (the Java probe CondProbe of run 9b, part S1; see
// ConditionsTestSupport.h), but where a case states that QtRocket differs.

namespace
{

using QtRocket::ElementHandler;
using QtRocket::GravityHandler;
using QtRocket::GravityModelType;
using QtRocket::PlainTextHandler;
using QtRocket::Result;
using QtRocket::SimulationOptions;
using QtRocket::WarningSet;
using QtRocket::Test::ConditionsCase;
using QtRocket::Test::conditionsCaseTestName;
using QtRocket::Test::expectConditionsCase;
using QtRocket::Test::warningTexts;

using Texts = std::vector<std::string>;

constexpr auto kGravityCases = std::to_array<ConditionsCase>({
    // BEGIN GENERATED: gravity
    {.name     = "cond: gravity constant",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <gravity model="constant">
    <value>3.71</value>
  </gravity>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  gravity=CONSTANT/3.71
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: gravity constant NaN",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <gravity model="constant">
    <value>NaN</value>
  </gravity>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal gravity value specified, ignoring.
  fcid=11111111-1111-1111-1111-111111111111
  gravity=CONSTANT/9.807
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: gravity constant Infinity",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <gravity model="constant">
    <value>Infinity</value>
  </gravity>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  gravity=CONSTANT/Infinity
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Illegal gravity value specified, ignoring.
  fcid=11111111-1111-1111-1111-111111111111
  gravity=CONSTANT/9.807
)out",
     .why      = "decision U3: a number that is not finite is not applied"},
    {.name     = "cond: gravity constant negative, no value",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <gravity model="constant">
    <value>-9.8</value>
  </gravity>
  <gravity model="constant"/>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  gravity=CONSTANT/-9.8
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: gravity model missing and unknown child",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <gravity><bogus>1</bogus></gravity>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown text in element 'bogus', ignoring.
  W[Other,NORMAL] Unknown gravity model type 'null', using WGS.
  fcid=11111111-1111-1111-1111-111111111111
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: gravity wgs",
     .xml      = R"xml(
<conditions><gravity model="wgs"/></conditions>
)xml",
     .java     = R"out(
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: gravity wgs with a value",
     .xml      = R"xml(
<conditions><gravity model="wgs"><value>3.71</value></gravity></conditions>
)xml",
     .java     = R"out(
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: gravity constant then wgs",
     .xml      = R"xml(
<conditions>
  <gravity model="constant"><value>3.71</value></gravity>
  <gravity model="wgs"/>
</conditions>
)xml",
     .java     = R"out(
  gravity=WGS/3.71
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: gravity constant zero",
     .xml      = R"xml(
<conditions><gravity model="constant"><value>0</value></gravity></conditions>
)xml",
     .java     = R"out(
  gravity=CONSTANT/0.0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: gravity value twice and then no number",
     .xml      = R"xml(
<conditions>
  <gravity model="constant">
    <value>3.71</value>
    <value>1.62</value>
  </gravity>
  <gravity model="constant">
    <value>5</value>
    <value>g</value>
  </gravity>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal gravity value specified, ignoring.
  gravity=CONSTANT/1.62
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: gravity model in capitals with attributes and unknown children",
     .xml      = R"xml(
<conditions>
  <gravity model="CONSTANT" extra="1">
    <value unit="m/s2">3.71</value>
    <bogus/>
  </gravity>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown gravity model type 'CONSTANT', using WGS.
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: gravity value with a child",
     .xml      = R"xml(
<conditions>
  <gravity model="constant">
    <value>3<x/>7</value>
  </gravity>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element x, ignoring.
  closed=conditions {model=constant} []
  gravity=CONSTANT/7.0
)out",
     .qtrocket = {},
     .why      = {}},
    // END GENERATED: gravity
});

class GravityElements : public ::testing::TestWithParam<ConditionsCase>
{ };

TEST_P(GravityElements, LoadAsInOpenRocketButWhereStated)
{
    expectConditionsCase(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Probe, GravityElements, ::testing::ValuesIn(kGravityCases),
                         conditionsCaseTestName);

// ---- the handler by itself ------------------------------------------------------------------

/// Closes a <value> child with the text @p content on @p handler and returns the warnings.
[[nodiscard]] Texts closeValue(GravityHandler& handler, std::string_view content)
{
    WarningSet         warnings;
    const Result<void> closed = handler.closeElement("value", {}, content, warnings);
    EXPECT_TRUE(closed.has_value());
    return warningTexts(warnings);
}

TEST(GravityHandler, KeepsTheModelAndStartsWithoutAValue)
{
    const GravityHandler constant(std::string("constant"));
    EXPECT_EQ(constant.getModel(), std::optional<std::string>("constant"));
    EXPECT_TRUE(std::isnan(constant.getConstantValue()));

    const GravityHandler none(std::nullopt);
    EXPECT_EQ(none.getModel(), std::nullopt);
}

TEST(GravityHandler, EveryChildIsPlainText)
{
    GravityHandler                handler(std::string("constant"));
    WarningSet                    warnings;
    const Result<ElementHandler*> opened = handler.openElement("value", {}, warnings);
    ASSERT_TRUE(opened.has_value());
    EXPECT_EQ(*opened, &PlainTextHandler::instance());
    const Result<ElementHandler*> other = handler.openElement("bogus", {{"a", "1"}}, warnings);
    ASSERT_TRUE(other.has_value());
    EXPECT_EQ(*other, &PlainTextHandler::instance());
    EXPECT_TRUE(warnings.empty());
}

TEST(GravityHandler, TheLastValueCountsAndARefusedOneTakesBackTheOneBefore)
{
    GravityHandler handler(std::string("constant"));
    EXPECT_EQ(closeValue(handler, " 3.71 "), Texts{});
    EXPECT_EQ(handler.getConstantValue(), 3.71);
    EXPECT_EQ(closeValue(handler, "1.62"), Texts{});
    EXPECT_EQ(handler.getConstantValue(), 1.62);

    const Texts refused{"Illegal gravity value specified, ignoring."};
    EXPECT_EQ(closeValue(handler, "g"), refused);
    EXPECT_TRUE(std::isnan(handler.getConstantValue()));
    EXPECT_EQ(closeValue(handler, "-9.8"), Texts{});
    EXPECT_EQ(handler.getConstantValue(), -9.8);
    // Not OpenRocket's, which stores an infinity: decision U3.
    EXPECT_EQ(closeValue(handler, "Infinity"), refused);
    EXPECT_TRUE(std::isnan(handler.getConstantValue()));
    EXPECT_EQ(closeValue(handler, "-1e999"), refused);
    EXPECT_TRUE(std::isnan(handler.getConstantValue()));
}

/// The gravity model and constant of options that had the constant model with 5 m/s^2 before
/// @p handler stored its settings, and the warnings of that: "WGS 5.0 []".
[[nodiscard]] std::string storedOverConstantFive(const GravityHandler& handler)
{
    SimulationOptions options;
    options.setGravityModelType(GravityModelType::CONSTANT);
    options.setConstantGravity(5.0);
    WarningSet warnings;
    handler.storeSettings(options, warnings);
    return std::format("{} {} {}", gravityModelTypeName(options.getGravityModelType()),
                       QtRocket::Test::javaNumber(options.getConstantGravity()),
                       QtRocket::Strings::join("|", warningTexts(warnings)));
}

TEST(GravityHandler, StoresTheModelAndTheValueOfTheConstantModelOnly)
{
    GravityHandler wgs(std::string("wgs"));
    EXPECT_EQ(closeValue(wgs, "3.71"), Texts{});
    EXPECT_EQ(storedOverConstantFive(wgs), "WGS 5.0 ") << "the value is not applied";

    GravityHandler constant(std::string("constant"));
    EXPECT_EQ(storedOverConstantFive(constant), "CONSTANT 5.0 ") << "no value: it stays";
    EXPECT_EQ(closeValue(constant, "3.71"), Texts{});
    EXPECT_EQ(storedOverConstantFive(constant), "CONSTANT 3.71 ");
}

TEST(GravityHandler, AnUnknownModelIsTheWgsModelWithAWarning)
{
    EXPECT_EQ(storedOverConstantFive(GravityHandler(std::string("Constant"))),
              "WGS 5.0 Unknown gravity model type 'Constant', using WGS.");
    EXPECT_EQ(storedOverConstantFive(GravityHandler(std::string(""))),
              "WGS 5.0 Unknown gravity model type '', using WGS.");
    // Java prints the null of a missing attribute.
    EXPECT_EQ(storedOverConstantFive(GravityHandler(std::nullopt)),
              "WGS 5.0 Unknown gravity model type 'null', using WGS.");
}

// AbstractElementHandler's closeElement(): the warnings of a child the handler does not know.
TEST(GravityHandler, AnotherChildGivesTheWarningsOfTheBaseClass)
{
    GravityHandler handler(std::string("constant"));
    WarningSet     warnings;
    EXPECT_TRUE(handler.closeElement("bogus", {{"a", "1"}}, " text ", warnings).has_value());
    EXPECT_TRUE(handler.closeElement("empty", {}, "  ", warnings).has_value());
    EXPECT_EQ(warningTexts(warnings), (Texts{"Unknown text in element 'bogus', ignoring.",
                                             "Unknown attributes in element 'bogus', ignoring."}));
    EXPECT_TRUE(std::isnan(handler.getConstantValue()));
    // The attributes of <value> itself are not looked at.
    EXPECT_TRUE(handler.closeElement("value", {{"unit", "m/s2"}}, "3.71", warnings).has_value());
    EXPECT_EQ(warnings.size(), 2U);
    EXPECT_EQ(handler.getConstantValue(), 3.71);
}

}  // namespace
