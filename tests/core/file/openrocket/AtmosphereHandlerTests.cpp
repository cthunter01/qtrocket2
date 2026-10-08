#include "QtRocket/file/openrocket/AtmosphereHandler.h"

#include <array>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"
#include "file/openrocket/ConditionsTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// The cases are <conditions> elements with an <atmosphere> element, run through
// SimulationConditionsHandler as the loader runs them; their expectations are what OpenRocket
// makes of the same elements (the Java probe CondProbe of run 9b, part S1; see
// ConditionsTestSupport.h), but where a case states that QtRocket differs.

namespace
{

using QtRocket::AtmosphereHandler;
using QtRocket::ElementHandler;
using QtRocket::PlainTextHandler;
using QtRocket::Result;
using QtRocket::SimulationOptions;
using QtRocket::WarningSet;
using QtRocket::Test::ConditionsCase;
using QtRocket::Test::conditionsCaseTestName;
using QtRocket::Test::expectConditionsCase;
using QtRocket::Test::javaNumber;
using QtRocket::Test::warningTexts;

using Texts = std::vector<std::string>;

constexpr auto kAtmosphereCases = std::to_array<ConditionsCase>({
    // BEGIN GENERATED: atmosphere
    {.name     = "cond: atmosphere extendedisa",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <atmosphere model="extendedisa">
    <basetemperature>300</basetemperature>
    <basepressure>90000</basepressure>
    <baserelativehumidity>0.5</baserelativehumidity>
  </atmosphere>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  atmosphere=isa:false T=300.0 p=90000.0 hum=0.5
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: atmosphere model missing and unknown child",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <atmosphere>
    <basetemperature>300</basetemperature>
    <bogus>1</bogus>
  </atmosphere>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown text in element 'bogus', ignoring.
  W[Other,NORMAL] Unknown atmospheric model, using ISA.
  fcid=11111111-1111-1111-1111-111111111111
  atmosphere=isa:true T=288.15 p=101325.0 hum=0.0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: atmosphere NaN values",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <atmosphere model="extendedisa">
    <basetemperature>NaN</basetemperature>
    <basepressure>NaN</basepressure>
    <baserelativehumidity>NaN</baserelativehumidity>
  </atmosphere>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal base temperature specified, ignoring.
  W[Other,NORMAL] Illegal base pressure specified, ignoring.
  W[Other,NORMAL] Illegal base humidity specified, ignoring
  fcid=11111111-1111-1111-1111-111111111111
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: atmosphere Infinity values",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <atmosphere model="extendedisa">
    <basetemperature>Infinity</basetemperature>
    <basepressure>Infinity</basepressure>
    <baserelativehumidity>Infinity</baserelativehumidity>
  </atmosphere>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal base humidity specified, ignoring
  fcid=11111111-1111-1111-1111-111111111111
  atmosphere=isa:false T=Infinity p=Infinity hum=0.0
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Illegal base temperature specified, ignoring.
  W[Other,NORMAL] Illegal base pressure specified, ignoring.
  W[Other,NORMAL] Illegal base humidity specified, ignoring
  fcid=11111111-1111-1111-1111-111111111111
)out",
     .why      = "decision U3: a number that is not finite is not applied"},
    {.name     = "cond: atmosphere negative and zero values",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <atmosphere model="extendedisa">
    <basetemperature>-5</basetemperature>
    <basepressure>-1</basepressure>
    <baserelativehumidity>1.5</baserelativehumidity>
  </atmosphere>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal base humidity specified, ignoring
  fcid=11111111-1111-1111-1111-111111111111
  atmosphere=isa:false T=-5.0 p=0.001 hum=0.0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "cond: atmosphere -Infinity temperature",
     .xml      = R"xml(
<conditions>
  <configid>11111111-1111-1111-1111-111111111111</configid>
  <atmosphere model="extendedisa">
    <basetemperature>-Infinity</basetemperature>
    <basepressure>-Infinity</basepressure>
  </atmosphere>
</conditions>
)xml",
     .java     = R"out(
  fcid=11111111-1111-1111-1111-111111111111
  atmosphere=isa:false T=-Infinity p=0.001 hum=0.0
)out",
     .qtrocket = R"out(
  W[Other,NORMAL] Illegal base temperature specified, ignoring.
  W[Other,NORMAL] Illegal base pressure specified, ignoring.
  fcid=11111111-1111-1111-1111-111111111111
)out",
     .why      = "decision U3: a number that is not finite is not applied"},
    {.name     = "s1: atmosphere isa",
     .xml      = R"xml(
<conditions>
  <atmosphere model="isa"/>
</conditions>
)xml",
     .java     = R"out(
  atmosphere=isa:true T=288.15 p=101325.0 hum=0.0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: atmosphere isa ignores its values",
     .xml      = R"xml(
<conditions>
  <atmosphere model="isa">
    <basetemperature>300</basetemperature>
    <basepressure>90000</basepressure>
    <baserelativehumidity>0.5</baserelativehumidity>
  </atmosphere>
</conditions>
)xml",
     .java     = R"out(
  atmosphere=isa:true T=288.15 p=101325.0 hum=0.0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: atmosphere isa then extendedisa without values",
     .xml      = R"xml(
<conditions>
  <atmosphere model="isa"/>
  <atmosphere model="extendedisa"/>
</conditions>
)xml",
     .java     = R"out(
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: atmosphere extendedisa with the pressure only",
     .xml      = R"xml(
<conditions>
  <atmosphere model="extendedisa">
    <basepressure>95000</basepressure>
  </atmosphere>
</conditions>
)xml",
     .java     = R"out(
  atmosphere=isa:false T=288.15 p=95000.0 hum=0.0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: atmosphere unknown model with values",
     .xml      = R"xml(
<conditions>
  <atmosphere model="ISA">
    <basetemperature>300</basetemperature>
  </atmosphere>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown atmospheric model, using ISA.
  atmosphere=isa:true T=288.15 p=101325.0 hum=0.0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: atmosphere humidity bounds",
     .xml      = R"xml(
<conditions>
  <atmosphere model="extendedisa">
    <baserelativehumidity>1</baserelativehumidity>
  </atmosphere>
</conditions>
)xml",
     .java     = R"out(
  atmosphere=isa:false T=288.15 p=101325.0 hum=1.0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: atmosphere humidity zero after a valid one and beyond the bounds",
     .xml      = R"xml(
<conditions>
  <atmosphere model="extendedisa">
    <baserelativehumidity>0.5</baserelativehumidity>
    <baserelativehumidity>-0.001</baserelativehumidity>
    <baserelativehumidity>1.001</baserelativehumidity>
  </atmosphere>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal base humidity specified, ignoring
  atmosphere=isa:false T=288.15 p=101325.0 hum=0.5
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: atmosphere pressure zero and tiny",
     .xml      = R"xml(
<conditions>
  <atmosphere model="extendedisa">
    <basepressure>0</basepressure>
  </atmosphere>
</conditions>
)xml",
     .java     = R"out(
  atmosphere=isa:false T=288.15 p=0.001 hum=0.0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: atmosphere valid values then NaN",
     .xml      = R"xml(
<conditions>
  <atmosphere model="extendedisa">
    <basetemperature>300</basetemperature>
    <basetemperature>warm</basetemperature>
    <basepressure>90000</basepressure>
    <basepressure></basepressure>
  </atmosphere>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Illegal base temperature specified, ignoring.
  W[Other,NORMAL] Illegal base pressure specified, ignoring.
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: atmosphere child with attributes and unknown child without text",
     .xml      = R"xml(
<conditions>
  <atmosphere model="extendedisa" extra="1">
    <basetemperature unit="K">300</basetemperature>
    <bogus/>
    <bogus2 a="1"/>
  </atmosphere>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown attributes in element 'bogus2', ignoring.
  atmosphere=isa:false T=300.0 p=101325.0 hum=0.0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: atmosphere child with a child",
     .xml      = R"xml(
<conditions>
  <atmosphere model="extendedisa">
    <basetemperature>30<x/>0</basetemperature>
    <basepressure>90000</basepressure>
  </atmosphere>
</conditions>
)xml",
     .java     = R"out(
  W[Other,NORMAL] Unknown element x, ignoring.
  closed=conditions {model=extendedisa} []
  atmosphere=isa:false T=0.0 p=90000.0 hum=0.0
)out",
     .qtrocket = {},
     .why      = {}},
    {.name     = "s1: atmosphere extendedisa then the launch altitude",
     .xml      = R"xml(
<conditions>
  <atmosphere model="extendedisa">
    <basetemperature>300</basetemperature>
  </atmosphere>
  <launchaltitude>1000</launchaltitude>
</conditions>
)xml",
     .java     = R"out(
  site=1000.0,0.0,0.0 geo=FLAT
  atmosphere=isa:false T=300.0 p=101325.0 hum=0.0
)out",
     .qtrocket = {},
     .why      = {}},
    // END GENERATED: atmosphere
});

class AtmosphereElements : public ::testing::TestWithParam<ConditionsCase>
{ };

TEST_P(AtmosphereElements, LoadAsInOpenRocketButWhereStated)
{
    expectConditionsCase(GetParam());
}

INSTANTIATE_TEST_SUITE_P(Probe, AtmosphereElements, ::testing::ValuesIn(kAtmosphereCases),
                         conditionsCaseTestName);

// ---- the handler by itself ------------------------------------------------------------------

/// Closes the child @p element with the text @p content on @p handler and returns the warnings.
[[nodiscard]] Texts closeChild(AtmosphereHandler& handler, std::string_view element,
                               std::string_view content)
{
    WarningSet         warnings;
    const Result<void> closed = handler.closeElement(element, {}, content, warnings);
    EXPECT_TRUE(closed.has_value());
    return warningTexts(warnings);
}

/// The temperature, pressure and humidity @p handler holds: "300.0 NaN 0.5".
[[nodiscard]] std::string valuesOf(const AtmosphereHandler& handler)
{
    return std::format("{} {} {}", javaNumber(handler.getTemperature()),
                       javaNumber(handler.getPressure()), javaNumber(handler.getHumidity()));
}

TEST(AtmosphereHandler, KeepsTheModelAndStartsWithoutValues)
{
    const AtmosphereHandler extended(std::string("extendedisa"));
    EXPECT_EQ(extended.getModel(), std::optional<std::string>("extendedisa"));
    EXPECT_EQ(valuesOf(extended), "NaN NaN NaN");

    const AtmosphereHandler none(std::nullopt);
    EXPECT_EQ(none.getModel(), std::nullopt);
}

TEST(AtmosphereHandler, EveryChildIsPlainText)
{
    AtmosphereHandler             handler(std::string("isa"));
    WarningSet                    warnings;
    const Result<ElementHandler*> opened = handler.openElement("basetemperature", {}, warnings);
    ASSERT_TRUE(opened.has_value());
    EXPECT_EQ(*opened, &PlainTextHandler::instance());
    const Result<ElementHandler*> other = handler.openElement("bogus", {{"a", "1"}}, warnings);
    ASSERT_TRUE(other.has_value());
    EXPECT_EQ(*other, &PlainTextHandler::instance());
    EXPECT_TRUE(warnings.empty());
}

TEST(AtmosphereHandler, TheLastTemperatureCountsAndARefusedOneTakesBackTheOneBefore)
{
    AtmosphereHandler handler(std::string("extendedisa"));
    EXPECT_EQ(closeChild(handler, "basetemperature", " 300 "), Texts{});
    EXPECT_EQ(valuesOf(handler), "300.0 NaN NaN");
    // Zero and a negative temperature are taken; the atmospheric model refuses them later.
    EXPECT_EQ(closeChild(handler, "basetemperature", "-5"), Texts{});
    EXPECT_EQ(valuesOf(handler), "-5.0 NaN NaN");

    const Texts refused{"Illegal base temperature specified, ignoring."};
    EXPECT_EQ(closeChild(handler, "basetemperature", "warm"), refused);
    EXPECT_EQ(valuesOf(handler), "NaN NaN NaN");
    EXPECT_EQ(closeChild(handler, "basetemperature", "280"), Texts{});
    // Not OpenRocket's, which stores an infinity: decision U3.
    EXPECT_EQ(closeChild(handler, "basetemperature", "Infinity"), refused);
    EXPECT_EQ(valuesOf(handler), "NaN NaN NaN");
    EXPECT_EQ(closeChild(handler, "basetemperature", "-Infinity"), refused);
    EXPECT_EQ(valuesOf(handler), "NaN NaN NaN");
}

TEST(AtmosphereHandler, ThePressureIsNeverBelowAThousandthOfAPascal)
{
    AtmosphereHandler handler(std::string("extendedisa"));
    EXPECT_EQ(closeChild(handler, "basepressure", "90000"), Texts{});
    EXPECT_EQ(valuesOf(handler), "NaN 90000.0 NaN");
    EXPECT_EQ(closeChild(handler, "basepressure", "0.002"), Texts{});
    EXPECT_EQ(valuesOf(handler), "NaN 0.002 NaN");
    EXPECT_EQ(closeChild(handler, "basepressure", "0.0005"), Texts{});
    EXPECT_EQ(valuesOf(handler), "NaN 0.001 NaN");
    EXPECT_EQ(closeChild(handler, "basepressure", "-1e300"), Texts{});
    EXPECT_EQ(valuesOf(handler), "NaN 0.001 NaN");

    const Texts refused{"Illegal base pressure specified, ignoring."};
    EXPECT_EQ(closeChild(handler, "basepressure", ""), refused);
    EXPECT_EQ(valuesOf(handler), "NaN NaN NaN");
    EXPECT_EQ(closeChild(handler, "basepressure", "90000"), Texts{});
    // Not OpenRocket's: it stores positive infinity and makes 0.001 Pa of negative infinity.
    EXPECT_EQ(closeChild(handler, "basepressure", "Infinity"), refused);
    EXPECT_EQ(valuesOf(handler), "NaN NaN NaN");
    EXPECT_EQ(closeChild(handler, "basepressure", "-Infinity"), refused);
    EXPECT_EQ(valuesOf(handler), "NaN NaN NaN");
}

TEST(AtmosphereHandler, AHumidityOutsideItsRangeLeavesTheOneBefore)
{
    AtmosphereHandler handler(std::string("extendedisa"));
    EXPECT_EQ(closeChild(handler, "baserelativehumidity", "0"), Texts{});
    EXPECT_EQ(valuesOf(handler), "NaN NaN 0.0");
    EXPECT_EQ(closeChild(handler, "baserelativehumidity", "1"), Texts{});
    EXPECT_EQ(valuesOf(handler), "NaN NaN 1.0");
    EXPECT_EQ(closeChild(handler, "baserelativehumidity", "0.25"), Texts{});

    // OpenRocket's text has no full stop.
    const Texts refused{"Illegal base humidity specified, ignoring"};
    EXPECT_EQ(closeChild(handler, "baserelativehumidity", "1.0000001"), refused);
    EXPECT_EQ(closeChild(handler, "baserelativehumidity", "-1e-9"), refused);
    EXPECT_EQ(closeChild(handler, "baserelativehumidity", "damp"), refused);
    EXPECT_EQ(closeChild(handler, "baserelativehumidity", "NaN"), refused);
    EXPECT_EQ(closeChild(handler, "baserelativehumidity", "Infinity"), refused);
    EXPECT_EQ(closeChild(handler, "baserelativehumidity", "-Infinity"), refused);
    EXPECT_EQ(valuesOf(handler), "NaN NaN 0.25");
}

/// The atmosphere of options that had the launch conditions 250 K, 80000 Pa and 0.1 entered by
/// hand before @p handler stored its settings, and the warnings of that: "isa:false 300.0
/// 80000.0 0.1 []".
[[nodiscard]] std::string storedOverManualConditions(const AtmosphereHandler& handler)
{
    SimulationOptions options;
    options.setIsaAtmosphere(false);
    options.setLaunchTemperature(250.0);
    options.setLaunchPressure(80000.0);
    options.setLaunchRelativeHumidity(0.1);
    WarningSet warnings;
    handler.storeSettings(options, warnings);
    return std::format(
        "isa:{} {} {} {} {}", options.isIsaAtmosphere(), javaNumber(options.getLaunchTemperature()),
        javaNumber(options.getLaunchPressure()), javaNumber(options.getLaunchRelativeHumidity()),
        QtRocket::Strings::join("|", warningTexts(warnings)));
}

TEST(AtmosphereHandler, TheExtendedModelStoresTheValuesItRead)
{
    AtmosphereHandler handler(std::string("extendedisa"));
    EXPECT_EQ(storedOverManualConditions(handler), "isa:false 250.0 80000.0 0.1 ")
        << "nothing read: everything stays";
    EXPECT_EQ(closeChild(handler, "basetemperature", "300"), Texts{});
    EXPECT_EQ(storedOverManualConditions(handler), "isa:false 300.0 80000.0 0.1 ");
    EXPECT_EQ(closeChild(handler, "basepressure", "90000"), Texts{});
    EXPECT_EQ(closeChild(handler, "baserelativehumidity", "0.5"), Texts{});
    EXPECT_EQ(storedOverManualConditions(handler), "isa:false 300.0 90000.0 0.5 ");
}

// The options are at sea level, where the standard atmosphere is 288.15 K and 101325 Pa (the
// first level of the model's table, set from constants).
TEST(AtmosphereHandler, TheIsaModelReplacesTheValuesByTheStandardAtmosphere)
{
    AtmosphereHandler handler(std::string("isa"));
    EXPECT_EQ(closeChild(handler, "basetemperature", "300"), Texts{});
    EXPECT_EQ(closeChild(handler, "basepressure", "90000"), Texts{});
    EXPECT_EQ(closeChild(handler, "baserelativehumidity", "0.5"), Texts{});
    EXPECT_EQ(storedOverManualConditions(handler), "isa:true 288.15 101325.0 0.0 ");
}

TEST(AtmosphereHandler, AnUnknownModelIsTheIsaModelWithAWarning)
{
    EXPECT_EQ(storedOverManualConditions(AtmosphereHandler(std::string("ISA"))),
              "isa:true 288.15 101325.0 0.0 Unknown atmospheric model, using ISA.");
    EXPECT_EQ(storedOverManualConditions(AtmosphereHandler(std::string(""))),
              "isa:true 288.15 101325.0 0.0 Unknown atmospheric model, using ISA.");
    EXPECT_EQ(storedOverManualConditions(AtmosphereHandler(std::nullopt)),
              "isa:true 288.15 101325.0 0.0 Unknown atmospheric model, using ISA.");
}

// AbstractElementHandler's closeElement(): the warnings of a child the handler does not know.
TEST(AtmosphereHandler, AnotherChildGivesTheWarningsOfTheBaseClass)
{
    AtmosphereHandler handler(std::string("extendedisa"));
    WarningSet        warnings;
    EXPECT_TRUE(handler.closeElement("bogus", {{"a", "1"}}, " text ", warnings).has_value());
    EXPECT_TRUE(handler.closeElement("empty", {}, "  ", warnings).has_value());
    EXPECT_EQ(warningTexts(warnings), (Texts{"Unknown text in element 'bogus', ignoring.",
                                             "Unknown attributes in element 'bogus', ignoring."}));
    EXPECT_EQ(valuesOf(handler), "NaN NaN NaN");
    // The attributes of a known child are not looked at.
    EXPECT_TRUE(
        handler.closeElement("basetemperature", {{"unit", "K"}}, "300", warnings).has_value());
    EXPECT_EQ(warnings.size(), 2U);
    EXPECT_EQ(valuesOf(handler), "300.0 NaN NaN");
}

}  // namespace
