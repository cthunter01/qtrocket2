#include "QtRocket/file/openrocket/PhotoStudioSaver.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/openrocket/OrkWriter.h"
#include "QtRocket/file/openrocket/PhotoStudioHandler.h"

// PhotoStudioSaver: the lines of the <photostudio> element. The expectations are what
// OpenRocket's PhotoStudioSaver.getElements() gives for the same maps (the probe of tier 10a S1,
// DocLevelProbe "photo"), except where a test says that QtRocket deviates.

namespace
{

using QtRocket::OrkWriter;
using QtRocket::PhotoStudioHandler;
using QtRocket::PhotoStudioSaver;

using Settings = std::map<std::string, std::string>;
using Lines    = std::vector<std::string>;

/// getElements() of @p settings with a writer of its own.
[[nodiscard]] Lines elementsOf(const Settings& settings)
{
    OrkWriter writer;
    return PhotoStudioSaver::getElements(settings, writer);
}

// Port of PhotoStudioSaverTest.legacySettingsDoNotSerializeMissingFieldsAsNull: settings of an
// older file that hold two of the keys give exactly those two lines, and no "null".
TEST(PhotoStudioSaver, LegacySettingsDoNotSerializeMissingFieldsAsNull)
{
    const Settings legacySettings{{"roll", "1.25"}, {"motionBlurred", "false"}};

    const Lines elements = elementsOf(legacySettings);

    EXPECT_EQ(elements, (Lines{"<roll>1.25</roll>", "<motionBlurred>false</motionBlurred>"}));
    EXPECT_FALSE(std::ranges::any_of(
        elements, [](const std::string& element) { return element.contains(">null<"); }));
}

TEST(PhotoStudioSaver, NoSettingsGiveNoLines)
{
    EXPECT_EQ(elementsOf({}), Lines{});
}

/// Every setting the loader keeps (PhotoStudioHandler's texts and colours) with a value of its
/// own, plus a key no file has: the map of DocLevelProbe's case "all".
[[nodiscard]] Settings everySetting()
{
    Settings settings;
    for (const std::string_view key : PhotoStudioHandler::kTextSettings)
    {
        settings.emplace(key, "v-" + std::string(key));
    }
    settings.emplace("unknownKey", "v-unknownKey");
    settings.emplace("sunlight", "0 1 2 3");
    settings.emplace("skyColor", "10 11 12 13");
    settings.emplace("gradientTopColor", "20 21 22 23");
    settings.emplace("gradientBottomColor", "30 31 32 33");
    settings.emplace("flameColor", "40 41 42 43");
    settings.emplace("smokeColor", "50 51 52 53");
    return settings;
}

// The 29 settings in Java's order, the colours as attributes. smokeOpacity, which the loader
// keeps, and a key that is no setting at all are not written, as in OpenRocket.
TEST(PhotoStudioSaver, WritesEverySettingInJavasOrder)
{
    EXPECT_EQ(PhotoStudioHandler::kTextSettings.size() + PhotoStudioHandler::kColorSettings.size(),
              PhotoStudioSaver::kSettings.size() + 1);
    EXPECT_EQ(elementsOf(everySetting()),
              (Lines{
                  "<roll>v-roll</roll>",
                  "<yaw>v-yaw</yaw>",
                  "<pitch>v-pitch</pitch>",
                  "<advance>v-advance</advance>",
                  "<viewAlt>v-viewAlt</viewAlt>",
                  "<viewAz>v-viewAz</viewAz>",
                  "<viewDistance>v-viewDistance</viewDistance>",
                  "<fov>v-fov</fov>",
                  "<lightAlt>v-lightAlt</lightAlt>",
                  "<lightAz>v-lightAz</lightAz>",
                  "<lightStrength>v-lightStrength</lightStrength>",
                  "<sunlight red=\"0\" green=\"1\" blue=\"2\" alpha=\"3\"/>",
                  "<ambiance>v-ambiance</ambiance>",
                  "<skyColor red=\"10\" green=\"11\" blue=\"12\" alpha=\"13\"/>",
                  "<backgroundType>v-backgroundType</backgroundType>",
                  "<gradientTopColor red=\"20\" green=\"21\" blue=\"22\" alpha=\"23\"/>",
                  "<gradientBottomColor red=\"30\" green=\"31\" blue=\"32\" alpha=\"33\"/>",
                  "<motionBlurred>v-motionBlurred</motionBlurred>",
                  "<motionBlurAmount>v-motionBlurAmount</motionBlurAmount>",
                  "<flame>v-flame</flame>",
                  "<flameColor red=\"40\" green=\"41\" blue=\"42\" alpha=\"43\"/>",
                  "<smoke>v-smoke</smoke>",
                  "<smokeColor red=\"50\" green=\"51\" blue=\"52\" alpha=\"53\"/>",
                  "<sparks>v-sparks</sparks>",
                  "<exhaustScale>v-exhaustScale</exhaustScale>",
                  "<flameAspectRatio>v-flameAspectRatio</flameAspectRatio>",
                  "<sparkConcentration>v-sparkConcentration</sparkConcentration>",
                  "<sparkWeight>v-sparkWeight</sparkWeight>",
                  "<sky>v-sky</sky>",
              }));
}

// A colour of fewer than four fields is an empty element; Integer.parseInt reads the fields, so
// "007" is 7, and any int is taken; a fifth field is not read. A text with markup is escaped
// (decision E3): OpenRocket writes "<sky>a<b&c</sky>", which neither program can read.
TEST(PhotoStudioSaver, WritesColoursAsOpenRocketAndEscapesTexts)
{
    const Settings odd{{"sunlight", "1 2 3"},
                       {"skyColor", "007 -1 300 4 99"},
                       {"flameColor", ""},
                       {"sky", "a<b&c"}};
    EXPECT_EQ(elementsOf(odd), (Lines{
                                   "<sunlight></sunlight>",
                                   "<skyColor red=\"7\" green=\"-1\" blue=\"300\" alpha=\"4\"/>",
                                   "<flameColor></flameColor>",
                                   "<sky>a&lt;b&amp;c</sky>",
                               }));
}

// Deviation: a colour field that Integer.parseInt refuses gives an empty element, as too few
// fields do; OpenRocket's save dies of the NumberFormatException (For input string: "a").
TEST(PhotoStudioSaver, AColourThatIsNoNumberIsAnEmptyElement)
{
    EXPECT_EQ(elementsOf({{"smokeColor", "a b c d"}}), Lines{"<smokeColor></smokeColor>"});
    EXPECT_EQ(elementsOf({{"smokeColor", "1  2 3 4"}}), Lines{"<smokeColor></smokeColor>"});
    EXPECT_EQ(elementsOf({{"smokeColor", "1 2 3 2147483648"}}), Lines{"<smokeColor></smokeColor>"});
    // Trailing spaces make no fields (String.split drops trailing empty strings).
    EXPECT_EQ(elementsOf({{"smokeColor", "1 2 3 4  "}}),
              Lines{"<smokeColor red=\"1\" green=\"2\" blue=\"3\" alpha=\"4\"/>"});
}

// The texts go through the writer's escape(), which counts what it drops.
TEST(PhotoStudioSaver, CountsTheCharactersItDropsInTheWriter)
{
    OrkWriter   writer;
    const Lines elements = PhotoStudioSaver::getElements({{"roll",
                                                           "1\x01"
                                                           "2"}},
                                                         writer);
    EXPECT_EQ(elements, Lines{"<roll>12</roll>"});
    EXPECT_EQ(writer.droppedCharacters(), std::size_t{1});
}

}  // namespace
