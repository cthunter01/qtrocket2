#include "QtRocket/file/openrocket/PhotoStudioHandler.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/DocumentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// PhotoStudioHandler: the handler of the <photostudio> element of a design file. What a file's
// text gives is compared with what OpenRocket makes of the same text.

namespace
{

using QtRocket::ErrorCode;
using QtRocket::PhotoStudioHandler;
using QtRocket::PlainTextHandler;
using QtRocket::Result;
using QtRocket::WarningSet;
using QtRocket::Test::DocumentCase;
using QtRocket::Test::documentCasesThatThrowWhenCutOff;
using QtRocket::Test::DocumentFixture;
using QtRocket::Test::documentOfDesignFile;
using QtRocket::Test::elementOfDocument;
using QtRocket::Test::failedDocumentCases;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::printedDocumentCases;
using QtRocket::Test::runHandler;

using Settings = std::map<std::string, std::string>;

using Texts = std::vector<std::string>;

// The tables are written by scripts/make_tables.py of the scratchpad's
// probes/tier9b-document-handlers from the cases (scripts/r4_cases.py) and OpenRocket's answers
// to them (DocumentProbe.java): do not edit them by hand.
// clang-format off
// BEGIN GENERATED TABLES PhotoStudioHandler
// What OpenRocket makes of each case (DocumentProbe.java of part R4), which QtRocket has to
// make of it too.
constexpr std::array<DocumentCase, 26> kJava{{
    {.name = "ps-empty", .xml = R"xml(<photostudio></photostudio>)xml", .expected = R"out(RESULT ok
ROOT photostudio {} []
EVENTS 0)out"},
    {.name = "ps-every-text-setting", .xml = R"xml(<photostudio><roll>value of roll</roll><yaw>value of yaw</yaw><pitch>value of pitch</pitch><advance>value of advance</advance><viewAlt>value of viewAlt</viewAlt><viewAz>value of viewAz</viewAz><viewDistance>value of viewDistance</viewDistance><fov>value of fov</fov><lightAlt>value of lightAlt</lightAlt><lightAz>value of lightAz</lightAz><lightStrength>value of lightStrength</lightStrength><ambiance>value of ambiance</ambiance><motionBlurred>value of motionBlurred</motionBlurred><motionBlurAmount>value of motionBlurAmount</motionBlurAmount><flame>value of flame</flame><smoke>value of smoke</smoke><smokeOpacity>value of smokeOpacity</smokeOpacity><sparks>value of sparks</sparks><exhaustScale>value of exhaustScale</exhaustScale><flameAspectRatio>value of flameAspectRatio</flameAspectRatio><sparkConcentration>value of sparkConcentration</sparkConcentration><sparkWeight>value of sparkWeight</sparkWeight><sky>value of sky</sky><backgroundType>value of backgroundType</backgroundType></photostudio>)xml", .expected = R"out(RESULT ok
ROOT photostudio {} []
EVENTS 0
| photo advance='value of advance'
| photo ambiance='value of ambiance'
| photo backgroundType='value of backgroundType'
| photo exhaustScale='value of exhaustScale'
| photo flame='value of flame'
| photo flameAspectRatio='value of flameAspectRatio'
| photo fov='value of fov'
| photo lightAlt='value of lightAlt'
| photo lightAz='value of lightAz'
| photo lightStrength='value of lightStrength'
| photo motionBlurAmount='value of motionBlurAmount'
| photo motionBlurred='value of motionBlurred'
| photo pitch='value of pitch'
| photo roll='value of roll'
| photo sky='value of sky'
| photo smoke='value of smoke'
| photo smokeOpacity='value of smokeOpacity'
| photo sparkConcentration='value of sparkConcentration'
| photo sparkWeight='value of sparkWeight'
| photo sparks='value of sparks'
| photo viewAlt='value of viewAlt'
| photo viewAz='value of viewAz'
| photo viewDistance='value of viewDistance'
| photo yaw='value of yaw')out"},
    {.name = "ps-every-color-setting", .xml = R"xml(<photostudio><sunlight red="10" green="20" blue="30" alpha="40"/><skyColor red="11" green="21" blue="31" alpha="41"/><flameColor red="12" green="22" blue="32" alpha="42"/><smokeColor red="13" green="23" blue="33" alpha="43"/><gradientTopColor red="14" green="24" blue="34" alpha="44"/><gradientBottomColor red="15" green="25" blue="35" alpha="45"/></photostudio>)xml", .expected = R"out(RESULT ok
ROOT photostudio {} []
EVENTS 0
| photo flameColor='12 22 32 42'
| photo gradientBottomColor='15 25 35 45'
| photo gradientTopColor='14 24 34 44'
| photo skyColor='11 21 31 41'
| photo smokeColor='13 23 33 43'
| photo sunlight='10 20 30 40')out"},
    {.name = "ps-two-stage-high-power-rocket", .xml = R"xml(<photostudio>
    <roll>3.14</roll>
    <yaw>0.0</yaw>
    <pitch>2.05</pitch>
    <advance>0.0</advance>
    <viewAlt>-0.00464788732394375</viewAlt>
    <viewAz>2.3942292490118717</viewAz>
    <viewDistance>1.778</viewDistance>
    <fov>1.4</fov>
    <lightAlt>0.35</lightAlt>
    <lightAz>-1.0</lightAz>
    <sunlight red="255" green="255" blue="255" alpha="255"/>
    <ambiance>0.30000001192092896</ambiance>
    <skyColor red="55" green="95" blue="155" alpha="255"/>
    <motionBlurred>false</motionBlurred>
    <flame>false</flame>
    <flameColor red="255" green="100" blue="50" alpha="255"/>
    <smoke>false</smoke>
    <smokeColor red="230" green="230" blue="230" alpha="102"/>
    <sparks>false</sparks>
    <exhaustScale>1.0</exhaustScale>
    <flameAspectRatio>1.0</flameAspectRatio>
    <sparkConcentration>0.2</sparkConcentration>
    <sparkWeight>0.0</sparkWeight>
    <sky>net.sf.openrocket.gui.figure3d.photo.sky.builtin.Mountains</sky>
  </photostudio>)xml", .expected = R"out(RESULT ok
ROOT photostudio {} []
EVENTS 0
| photo advance='0.0'
| photo ambiance='0.30000001192092896'
| photo exhaustScale='1.0'
| photo flame='false'
| photo flameAspectRatio='1.0'
| photo flameColor='255 100 50 255'
| photo fov='1.4'
| photo lightAlt='0.35'
| photo lightAz='-1.0'
| photo motionBlurred='false'
| photo pitch='2.05'
| photo roll='3.14'
| photo sky='net.sf.openrocket.gui.figure3d.photo.sky.builtin.Mountains'
| photo skyColor='55 95 155 255'
| photo smoke='false'
| photo smokeColor='230 230 230 102'
| photo sparkConcentration='0.2'
| photo sparkWeight='0.0'
| photo sparks='false'
| photo sunlight='255 255 255 255'
| photo viewAlt='-0.00464788732394375'
| photo viewAz='2.3942292490118717'
| photo viewDistance='1.778'
| photo yaw='0.0')out"},
    {.name = "ps-texts-as-they-are", .xml = R"xml(<photostudio><roll> 1.5 </roll><yaw>abc</yaw><pitch></pitch><advance/><fov>a &amp; b</fov><sky>some.Class
Next</sky><flame>NaN</flame><smoke>1e999</smoke></photostudio>)xml", .expected = R"out(RESULT ok
ROOT photostudio {} []
EVENTS 0
| photo advance=''
| photo flame='NaN'
| photo fov='a & b'
| photo pitch=''
| photo roll=' 1.5 '
| photo sky='some.Class\nNext'
| photo smoke='1e999'
| photo yaw='abc')out"},
    {.name = "ps-text-setting-twice", .xml = R"xml(<photostudio><roll>1</roll><yaw>5</yaw><roll>2</roll><roll></roll></photostudio>)xml", .expected = R"out(RESULT ok
ROOT photostudio {} []
EVENTS 0
| photo roll=''
| photo yaw='5')out"},
    {.name = "ps-text-setting-with-attributes", .xml = R"xml(<photostudio><roll a="1" red="2">3</roll></photostudio>)xml", .expected = R"out(RESULT ok
ROOT photostudio {} []
EVENTS 0
| photo roll='3')out"},
    {.name = "ps-names-that-are-none", .xml = R"xml(<photostudio><Roll>1</Roll><ROLL>2</ROLL><viewalt>3</viewalt><skycolor red="1" green="2" blue="3"/><bogus b="1">x</bogus><empty/></photostudio>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'Roll', ignoring.
W Unknown text in element 'ROLL', ignoring.
W Unknown text in element 'viewalt', ignoring.
W Unknown attributes in element 'skycolor', ignoring.
W Unknown text in element 'bogus', ignoring.
W Unknown attributes in element 'bogus', ignoring.
ROOT photostudio {} []
EVENTS 0)out"},
    {.name = "ps-color-without-alpha", .xml = R"xml(<photostudio><sunlight red="1" green="2" blue="3"/></photostudio>)xml", .expected = R"out(RESULT ok
ROOT photostudio {} []
EVENTS 0
| photo sunlight='1 2 3 255')out"},
    {.name = "ps-color-numbers", .xml = R"xml(<photostudio><sunlight red="300" green="-1" blue="+5" alpha="-0"/><skyColor red="007" green="0" blue="2147483647" alpha="-2147483648"/></photostudio>)xml", .expected = R"out(RESULT ok
ROOT photostudio {} []
EVENTS 0
| photo skyColor='7 0 2147483647 -2147483648'
| photo sunlight='300 -1 5 0')out"},
    {.name = "ps-color-with-text-and-more", .xml = R"xml(<photostudio><sunlight red="1" green="2" blue="3" hue="4">text</sunlight></photostudio>)xml", .expected = R"out(RESULT ok
ROOT photostudio {} []
EVENTS 0
| photo sunlight='1 2 3 255')out"},
    {.name = "ps-color-twice", .xml = R"xml(<photostudio><sunlight red="1" green="2" blue="3"/><sunlight red="4" green="5" blue="6" alpha="7"/><sunlight red="8" green="9"/></photostudio>)xml", .expected = R"out(RESULT ok
ROOT photostudio {} []
EVENTS 0
| photo sunlight='4 5 6 7')out"},
    {.name = "ps-color-without-a-part", .xml = R"xml(<photostudio><sunlight green="2" blue="3"/><skyColor red="1" blue="3"/><flameColor red="1" green="2"/><smokeColor/><gradientTopColor alpha="5"/><gradientBottomColor red="1" green="2" alpha="x"/><roll>after</roll></photostudio>)xml", .expected = R"out(RESULT ok
ROOT photostudio {} []
EVENTS 0
| photo roll='after')out"},
    {.name = "ps-color-red-no-number", .xml = R"xml(<photostudio><roll>before</roll><sunlight red="x" green="2" blue="3"/><yaw>never</yaw></photostudio>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "x"
EVENTS 0
| photo roll='before')out"},
    {.name = "ps-color-red-no-number-and-without-blue", .xml = R"xml(<photostudio><sunlight red="x" green="2"/></photostudio>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "x"
EVENTS 0)out"},
    {.name = "ps-color-green-no-number-and-without-red", .xml = R"xml(<photostudio><sunlight green="1.5" blue="3"/></photostudio>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "1.5"
EVENTS 0)out"},
    {.name = "ps-color-blue-no-number", .xml = R"xml(<photostudio><sunlight red="1" green="2" blue=""/></photostudio>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: ""
EVENTS 0)out"},
    {.name = "ps-color-alpha-no-number", .xml = R"xml(<photostudio><sunlight red="1" green="2" blue="3" alpha="0.5"/></photostudio>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "0.5"
EVENTS 0)out"},
    {.name = "ps-color-with-a-blank", .xml = R"xml(<photostudio><sunlight red=" 1" green="2" blue="3"/></photostudio>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: " 1"
EVENTS 0)out"},
    {.name = "ps-color-too-large", .xml = R"xml(<photostudio><sunlight red="2147483648" green="2" blue="3"/></photostudio>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "2147483648"
EVENTS 0)out"},
    {.name = "ps-color-in-hexadecimal", .xml = R"xml(<photostudio><sunlight red="0x10" green="2" blue="3"/></photostudio>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: For input string: "0x10"
EVENTS 0)out"},
    {.name = "ps-text-and-attributes", .xml = R"xml(<photostudio a="1">txt<roll>1</roll>more</photostudio>)xml", .expected = R"out(RESULT ok
ROOT photostudio {a=1} [txtmore]
EVENTS 0
| photo roll='1')out"},
    {.name = "ps-element-in-a-text-setting", .xml = R"xml(<photostudio p="1"><roll>1<x a="1"/>2</roll><yaw>3</yaw></photostudio>)xml", .expected = R"out(RESULT ok
W Unknown element x, ignoring.
ROOT photostudio {} [1]
EVENTS 0
| photo roll='2'
| photo yaw='3')out"},
    {.name = "ps-element-in-a-color", .xml = R"xml(<photostudio><sunlight red="1" green="2" blue="3"><x red="9"/></sunlight></photostudio>)xml", .expected = R"out(RESULT ok
W Unknown element x, ignoring.
ROOT photostudio {blue=3, green=2, red=1} []
EVENTS 0)out"},
    {.name = "ps-element-with-a-color-in-a-color", .xml = R"xml(<photostudio><sunlight red="1" green="2" blue="3"><x red="7" green="8" blue="9" alpha="10"/></sunlight></photostudio>)xml", .expected = R"out(RESULT ok
W Unknown element x, ignoring.
ROOT photostudio {blue=3, green=2, red=1} []
EVENTS 0
| photo sunlight='7 8 9 10')out"},
    {.name = "ps-scout", .xml = R"xml(<photostudio a="1">txt<roll> 1.5 </roll><yaw>abc</yaw><sky>some.Class</sky><sunlight red="1" green="2" blue="3"/><skyColor red="1" green="2" blue="3" alpha="4"/><flameColor red="1" green="2"/><bogus b="1">x</bogus><roll>2</roll></photostudio>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'bogus', ignoring.
W Unknown attributes in element 'bogus', ignoring.
ROOT photostudio {a=1} [txt]
EVENTS 0
| photo roll='2'
| photo sky='some.Class'
| photo skyColor='1 2 3 4'
| photo sunlight='1 2 3 255'
| photo yaw='abc')out"},
}};

// Where QtRocket answers otherwise on purpose: the comment of a case says why and gives the
// lines of OpenRocket's answer that QtRocket does not give.
constexpr std::array<DocumentCase, 0> kOwn{};
// END GENERATED TABLES PhotoStudioHandler
// clang-format on

TEST(PhotoStudioHandler, ReadsItsCasesAsOpenRocket)
{
    EXPECT_EQ(failedDocumentCases(kJava), Texts{});
}

TEST(PhotoStudioHandler, ReadsItsOwnCasesAsDecided)
{
    EXPECT_EQ(failedDocumentCases(kOwn), Texts{});
}

// Not a test: prints what QtRocket makes of the cases, for scripts/make_tables.py. Run
// with --gtest_also_run_disabled_tests.
TEST(PhotoStudioHandler, DISABLED_PrintsCases)
{
    std::cout << printedDocumentCases(kJava) << printedDocumentCases(kOwn);
}

TEST(PhotoStudioHandler, NoCaseThrowsWhenItsTextIsCutOff)
{
    EXPECT_EQ(documentCasesThatThrowWhenCutOff(kJava), Texts{});
    EXPECT_EQ(documentCasesThatThrowWhenCutOff(kOwn), Texts{});
}

/// The <photostudio> element of the example design @p file.
[[nodiscard]] std::string photoStudioOfExample(std::string_view file)
{
    return elementOfDocument(documentOfDesignFile(QtRocket::Test::dataDir() / "examples" / file),
                             "photostudio");
}

/// The photo settings of the example "Two stage high power rocket", as its file holds them.
[[nodiscard]] Settings twoStageSettings()
{
    return {{"advance", "0.0"},
            {"ambiance", "0.30000001192092896"},
            {"exhaustScale", "1.0"},
            {"flame", "false"},
            {"flameAspectRatio", "1.0"},
            {"flameColor", "255 100 50 255"},
            {"fov", "1.4"},
            {"lightAlt", "0.35"},
            {"lightAz", "-1.0"},
            {"motionBlurred", "false"},
            {"pitch", "2.05"},
            {"roll", "3.14"},
            {"sky", "net.sf.openrocket.gui.figure3d.photo.sky.builtin.Mountains"},
            {"skyColor", "55 95 155 255"},
            {"smoke", "false"},
            {"smokeColor", "230 230 230 102"},
            {"sparkConcentration", "0.2"},
            {"sparkWeight", "0.0"},
            {"sparks", "false"},
            {"sunlight", "255 255 255 255"},
            {"viewAlt", "-0.00464788732394375"},
            {"viewAz", "2.3942292490118717"},
            {"viewDistance", "1.778"},
            {"yaw", "0.0"}};
}

// Two of the 16 example designs were saved with the settings of their photo: they are kept as
// the texts the file holds, a colour as its four numbers. (The name of the sky is the name of
// a Java class of the GUI that wrote the file; the core keeps it as text.)
TEST(PhotoStudioHandler, ReadsThePhotoSettingsOfTheTwoStageExample)
{
    const std::string element = photoStudioOfExample("Two stage high power rocket.ork");
    ASSERT_TRUE(element.starts_with("<photostudio>"));

    DocumentFixture  fixture;
    const HandlerRun run = fixture.load(element);
    ASSERT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});
    EXPECT_EQ(fixture.document().getPhotoSettings().size(), 24U);
    EXPECT_EQ(fixture.document().getPhotoSettings(), twoStageSettings());
}

TEST(PhotoStudioHandler, ReadsThePhotoSettingsOfThePoweredPodsExample)
{
    DocumentFixture  fixture;
    const HandlerRun run =
        fixture.load(photoStudioOfExample("Pods--powered with recovery deployment.ork"));
    ASSERT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});

    // The same settings but for the view and the name of the sky.
    Settings expected        = twoStageSettings();
    expected["viewAlt"]      = "-0.1660008401940507";
    expected["viewAz"]       = "3.3843478260869526";
    expected["viewDistance"] = "0.44";
    expected["sky"]          = "info.openrocket.swing.gui.figure3d.photo.sky.builtin.Mountains";
    EXPECT_EQ(fixture.document().getPhotoSettings(), expected);
}

/// The example designs whose <photostudio> element gives a setting, sorted; one whose element
/// cannot be read or gives a warning is listed as "<file>: not read".
[[nodiscard]] Texts examplesWithPhotoSettings()
{
    Texts with;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(QtRocket::Test::dataDir() / "examples"))
    {
        const std::string file = QtRocket::pathToUtf8(entry.path().filename());
        if (!file.ends_with(".ork"))
        {
            continue;
        }
        DocumentFixture  fixture;
        const HandlerRun run = fixture.load(photoStudioOfExample(file));
        if (!run.result.has_value() || !run.warnings.empty())
        {
            with.push_back(file + ": not read");
        }
        else if (!fixture.document().getPhotoSettings().empty())
        {
            with.push_back(file);
        }
    }
    std::ranges::sort(with);
    return with;
}

// The other 14 examples have an empty element, which leaves the document without settings.
TEST(PhotoStudioHandler, TheOtherExamplesHaveAnEmptyElement)
{
    EXPECT_EQ(examplesWithPhotoSettings(), (Texts{"Pods--powered with recovery deployment.ork",
                                                  "Two stage high power rocket.ork"}));
}

// The handler stores into the map it was given, which is the document's: a setting the element
// does not hold stays as it was.
TEST(PhotoStudioHandler, StoresIntoTheMapItWasGiven)
{
    Settings           settings{{"roll", "old"}, {"mine", "kept"}};
    PhotoStudioHandler handler(settings);
    const HandlerRun   run = runHandler(
        handler,
        R"xml(<photostudio><roll>1.5</roll><sunlight red="1" green="2" blue="3"/></photostudio>)xml");
    ASSERT_TRUE(run.result.has_value());
    EXPECT_EQ(settings, (Settings{{"mine", "kept"}, {"roll", "1.5"}, {"sunlight", "1 2 3 255"}}));
}

// Every child is plain text; a colour value that is no int ends the load with the message of
// Java's NumberFormatException.
TEST(PhotoStudioHandler, FailsTheLoadForAColorThatIsNoNumber)
{
    Settings           settings;
    PhotoStudioHandler handler(settings);
    WarningSet         warnings;
    EXPECT_EQ(handler.openElement("roll", {}, warnings).value_or(nullptr),
              &PlainTextHandler::instance());
    EXPECT_EQ(handler.openElement("bogus", {}, warnings).value_or(nullptr),
              &PlainTextHandler::instance());

    const Result<void> closed = handler.closeElement(
        "skyColor", {{"red", "1"}, {"green", "2"}, {"blue", "3"}, {"alpha", "opaque"}}, "",
        warnings);
    ASSERT_FALSE(closed.has_value());
    EXPECT_EQ(closed.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(closed.error().message, "For input string: \"opaque\"");
    EXPECT_TRUE(settings.empty());
    EXPECT_TRUE(warnings.empty());
}

// The names OpenRocket's handler knows: 24 settings kept as text and 6 colours, none twice.
TEST(PhotoStudioHandler, KnowsOpenRocketsThirtySettings)
{
    std::vector<std::string_view> names(PhotoStudioHandler::kTextSettings.begin(),
                                        PhotoStudioHandler::kTextSettings.end());
    names.insert(names.end(), PhotoStudioHandler::kColorSettings.begin(),
                 PhotoStudioHandler::kColorSettings.end());
    EXPECT_EQ(PhotoStudioHandler::kTextSettings.size(), 24U);
    EXPECT_EQ(PhotoStudioHandler::kColorSettings.size(), 6U);
    std::ranges::sort(names);
    EXPECT_EQ(std::ranges::adjacent_find(names), names.end());
}

}  // namespace
