#include <array>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// The failure policy of the loader (decision D9) for the component handlers: nothing a file can
// hold may make them throw, or leave a rocket that throws when it is used. A design that has
// one of everything these handlers read (flight configurations with stage entries, a motor
// mount with motors and ignition, a parachute with a deployment, a stage with a separation, a
// freeform fin set with points) is read with one value after the other replaced by texts that
// are numbers at and beyond the edge of what the model expects, ids of every kind, and
// nonsense; then the top-level loader's last steps are run and the rocket is looked at
// (whatUsingTheRocketThrows()).
//
// What the handlers make of such a value is OpenRocket's business and pinned by the case
// tables; here only "no exception" counts. (The values of the components' own parameters are
// setter_robustness_tests.cpp's.)

namespace
{

using QtRocket::Test::whatReadingThrows;

using Texts = std::vector<std::string>;

constexpr auto kHostileTexts = std::to_array<std::string_view>({
    // numbers
    "0",
    "-0.0",
    "1",
    "-1",
    "0.5",
    "1e-300",
    "4.9e-324",
    "1e18",
    "-1e18",
    "1e300",
    "-1e300",
    "1.7976931348623157e308",
    "-1.7976931348623157e308",
    "1e400",
    "NaN",
    "Infinity",
    "-Infinity",
    "0x1p-3",
    "1e-1d",
    // whole numbers, as a stage number has them
    "2",
    "+3",
    "007",
    "2147483647",
    "-2147483648",
    "2147483648",
    "99999999999999999999",
    // truth values
    "true",
    "TRUE",
    "false",
    // ids: a UUID, a short one, the two reserved keys, texts that are none
    "11111111-2222-3333-4444-555555555555",
    "1-2-3-4-5",
    "ffffffff-f4f2-f1f0-0000-0000000009b9",
    "ffffffff-f4f2-f1f0-0000-00000000162c",
    "00000000-0000-0000-0000-000000000000",
    "abc",
    // event names
    "burnout",
    "apogee",
    "never",
    "automatic",
    // nothing
    "",
    " ",
    "x",
    "none",
    "auto",
});

/// A design with one of everything the component handlers read; "{0}" to "{23}" stand for the
/// values a trial replaces.
constexpr std::string_view kDesign =
    "<motorconfiguration configid=\"{0}\" default=\"{1}\"><name>{2}</name>"
    "<stage number=\"{3}\" active=\"{4}\"/><stage number=\"1\" active=\"true\"/>"
    "</motorconfiguration>"
    "<motorconfiguration configid=\"22222222-3333-4444-5555-666666666666\"/>"
    "<subcomponents><stage><separationconfiguration configid=\"{5}\">"
    "<separationevent>{6}</separationevent><separationdelay>{7}</separationdelay>"
    "<separationaltitude>{8}</separationaltitude></separationconfiguration><subcomponents>"
    "<nosecone><length>0.1</length><aftradius>0.02</aftradius></nosecone>"
    "<bodytube><length>0.4</length><radius>0.02</radius><motormount>"
    "<ignitionevent>{9}</ignitionevent><ignitiondelay>{10}</ignitiondelay><overhang>{11}</overhang>"
    "<motor configid=\"{12}\"><designation>C6</designation><delay>{13}</delay>"
    "<nozzleexitdiameter>{14}</nozzleexitdiameter></motor>"
    "<ignitionconfiguration configid=\"{15}\"><ignitionevent>{16}</ignitionevent>"
    "<ignitiondelay>{17}</ignitiondelay></ignitionconfiguration>"
    "<motor configid=\"22222222-3333-4444-5555-666666666666\"><designation>D12</designation>"
    "<delay>3</delay></motor></motormount><subcomponents>"
    "<parachute><deploymentconfiguration configid=\"{18}\"><deployevent>{19}</deployevent>"
    "<deploydelay>{20}</deploydelay><deployaltitude>{21}</deployaltitude>"
    "</deploymentconfiguration></parachute>"
    "<freeformfinset><tabheight>0.01</tabheight><tablength>0.02</tablength>"
    "<tabposition relativeto=\"end\">-0.01</tabposition><finpoints><point x=\"0\" y=\"0\"/>"
    "<point x=\"{22}\" y=\"0.05\"/><point x=\"0.08\" y=\"{23}\"/><point x=\"0.1\" y=\"0\"/>"
    "</finpoints></freeformfinset><shockcord><cordlength>auto</cordlength></shockcord>"
    "</subcomponents></bodytube></subcomponents></stage>"
    "<stage><subcomponents><bodytube><length>0.2</length><radius>0.02</radius></bodytube>"
    "</subcomponents></stage></subcomponents>";

/// The values of kDesign as a file of OpenRocket would have them.
constexpr std::array<std::string_view, 24> kUsualValues{
    "11111111-2222-3333-4444-555555555555",
    "true",
    "Configuration",
    "0",
    "true",
    "11111111-2222-3333-4444-555555555555",
    "burnout",
    "1.0",
    "100.0",
    "automatic",
    "0.0",
    "0.003",
    "11111111-2222-3333-4444-555555555555",
    "5.0",
    "0.01",
    "11111111-2222-3333-4444-555555555555",
    "burnout",
    "2.0",
    "11111111-2222-3333-4444-555555555555",
    "apogee",
    "1.5",
    "120.0",
    "0.02",
    "0.05",
};

/// kDesign with @p values.
[[nodiscard]] std::string design(const std::array<std::string_view, 24>& values)
{
    return std::vformat(
        kDesign, std::make_format_args(values[0], values[1], values[2], values[3], values[4],
                                       values[5], values[6], values[7], values[8], values[9],
                                       values[10], values[11], values[12], values[13], values[14],
                                       values[15], values[16], values[17], values[18], values[19],
                                       values[20], values[21], values[22], values[23]));
}

/// What throws when each value of the design in turn is each hostile text: the value's number,
/// the text and what was thrown.
[[nodiscard]] Texts thrownForOneHostileValue()
{
    Texts thrown;
    for (std::size_t value = 0; value < kUsualValues.size(); ++value)
    {
        for (const std::string_view text : kHostileTexts)
        {
            std::array<std::string_view, 24> values = kUsualValues;
            values.at(value)                        = text;
            const std::string wrong                 = whatReadingThrows(design(values));
            if (!wrong.empty())
            {
                thrown.push_back(std::format("value {} as '{}': {}", value, text, wrong));
            }
        }
    }
    return thrown;
}

/// What throws when every value of the design is the same hostile text.
[[nodiscard]] Texts thrownForAllValuesHostile()
{
    Texts thrown;
    for (const std::string_view text : kHostileTexts)
    {
        std::array<std::string_view, 24> values{};
        values.fill(text);
        const std::string wrong = whatReadingThrows(design(values));
        if (!wrong.empty())
        {
            thrown.push_back(std::format("every value as '{}': {}", text, wrong));
        }
    }
    return thrown;
}

TEST(ComponentHandlerRobustness, TheDesignOfTheTrialsLoadsWithoutAWarning)
{
    QtRocket::Test::RocketLoadFixture fixture;
    const QtRocket::Test::HandlerRun  run = fixture.load(design(kUsualValues));
    ASSERT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});
    EXPECT_EQ(QtRocket::Test::whatUsingTheRocketThrows(fixture), "");
    // Two stages, the first with a nose cone and a tube that holds three components.
    EXPECT_EQ(fixture.rocket().getChildCount(), 2U);
    EXPECT_EQ(fixture.rocket().getChild(0).getChild(1).getChildCount(), 3U);
    EXPECT_EQ(fixture.rocket().getFlightConfigurationCount(), 2);
}

TEST(ComponentHandlerRobustness, NoSingleHostileValueMakesTheLoaderThrow)
{
    EXPECT_EQ(thrownForOneHostileValue(), Texts{});
}

TEST(ComponentHandlerRobustness, NoHostileValueEverywhereMakesTheLoaderThrow)
{
    EXPECT_EQ(thrownForAllValuesHostile(), Texts{});
}

}  // namespace
