// Tests of OpenRocketContentHandler, the handler of the content of the root element of a
// design file (OpenRocket's file/openrocket/importt/OpenRocketContentHandler). OpenRocket has
// no test of the class; the warnings are OpenRocket's for the same documents (the cases c-*
// and s-* of TopProbe.java of the probes of tier 9c, part "loader", which
// OpenRocketHandlerTests.cpp runs through the root handler). Here the handler is given the
// root element directly.

#include "QtRocket/file/openrocket/OpenRocketContentHandler.h"

#include <format>
#include <string>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "file/openrocket/HandlerTestSupport.h"

namespace
{

using QtRocket::BugError;
using QtRocket::DocumentLoadingContext;
using QtRocket::ErrorCode;
using QtRocket::OpenRocketContentHandler;
using QtRocket::Test::HandlerFixture;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::runHandler;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

/// A simulation of the flight configuration the rocket of kRocket selects, with stored data.
constexpr std::string_view kSimulation =
    R"(<simulation status="uptodate"><name>S</name><conditions>)"
    R"(<configid>da326836-0959-4c94-bcd5-49dee07235a4</configid><timestep>0.05</timestep>)"
    R"(</conditions><flightdata maxaltitude="47.272"><databranch name="b" types="Time,Altitude">)"
    R"(<datapoint>0.0,0.0</datapoint></databranch></flightdata></simulation>)";

/// A rocket of one stage with one flight configuration.
constexpr std::string_view kRocket =
    R"(<rocket><name>One</name>)"
    R"(<motorconfiguration configid="da326836-0959-4c94-bcd5-49dee07235a4" default="true"/>)"
    R"(<subcomponents><stage><name>S0</name></stage></subcomponents></rocket>)";

/// What the content handler makes of @p content, the content of the root element.
struct ContentRun
{
    HandlerFixture fixture;
    HandlerRun     run;

    explicit ContentRun(std::string_view content)
    {
        OpenRocketContentHandler handler(fixture.context());
        run = runHandler(handler, std::format("<openrocket>{}</openrocket>", content));
    }

    [[nodiscard]] std::vector<std::string> texts() const { return run.texts(); }
};

/// The warnings the content handler gives for @p content.
[[nodiscard]] std::vector<std::string> warningsOf(std::string_view content)
{
    return ContentRun(content).texts();
}

// The five elements, in the order OpenRocket writes them: each is read into the document by
// its handler, without a warning.
TEST(OpenRocketContentHandler, ReadsTheFiveElementsIntoTheDocument)
{
    ContentRun content(
        std::format("{}<datatypes/><simulations>{}</simulations>"
                    "<photostudio><roll>1.0</roll></photostudio>"
                    R"(<docprefs><pref key="a" type="string">x</pref></docprefs>)",
                    kRocket, kSimulation));

    ASSERT_TRUE(content.run.result.has_value()) << content.run.result.error().message;
    EXPECT_THAT(content.texts(), IsEmpty());
    QtRocket::OpenRocketDocument& document = content.fixture.document();
    EXPECT_EQ(document.getRocket().getName(), "One");
    EXPECT_EQ(document.getRocket().getChildCount(), 1U);
    ASSERT_EQ(document.getSimulationCount(), 1U);
    EXPECT_EQ(document.getSimulation(0)->getName(), "S");
    EXPECT_THAT(document.getPhotoSettings(), ElementsAre(Pair("roll", "1.0")));
    EXPECT_EQ(document.getDocumentPreferences().getString("a", "none"), "x");
}

// A document without a <rocket> is the empty rocket: no warning and no failure.
TEST(OpenRocketContentHandler, ADocumentWithoutARocketIsTheEmptyRocket)
{
    ContentRun content("");

    ASSERT_TRUE(content.run.result.has_value());
    EXPECT_THAT(content.texts(), IsEmpty());
    EXPECT_EQ(content.fixture.rocket().getName(), "Rocket");
    EXPECT_EQ(content.fixture.rocket().getChildCount(), 0U);
}

// <rocket>, <datatypes> and <simulations> are read once each: a later one is ignored, each
// with its own text.
TEST(OpenRocketContentHandler, ASecondRocketDatatypesOrSimulationsElementIsIgnored)
{
    EXPECT_THAT(warningsOf("<rocket/><rocket/>"),
                ElementsAre("Multiple rocket designs within one document, ignoring later ones."));
    EXPECT_THAT(warningsOf("<datatypes/><datatypes/>"),
                ElementsAre("Multiple datatype blocks. Ignoring later ones."));
    EXPECT_THAT(
        warningsOf("<simulations/><simulations/>"),
        ElementsAre("Multiple simulation definitions within one document, ignoring later ones."));
    // The warning is given once however many follow (the warning set keeps equal texts once).
    EXPECT_THAT(warningsOf("<rocket/><rocket/><rocket/>"),
                ElementsAre("Multiple rocket designs within one document, ignoring later ones."));
}

// The later element changes nothing: the rocket is the first one, the simulations are the
// first block's.
TEST(OpenRocketContentHandler, WhatALaterElementHoldsIsNotRead)
{
    ContentRun content(std::format(
        "{}<rocket><name>Second</name><subcomponents><stage/><stage/></subcomponents></rocket>"
        "<simulations>{}</simulations><simulations>{}{}</simulations>",
        kRocket, kSimulation, kSimulation, kSimulation));

    ASSERT_TRUE(content.run.result.has_value());
    EXPECT_EQ(content.fixture.rocket().getName(), "One");
    EXPECT_EQ(content.fixture.rocket().getChildCount(), 1U);
    EXPECT_EQ(content.fixture.document().getSimulationCount(), 1U);
}

// <photostudio> and <docprefs> are read every time: a later one adds to what the earlier ones
// set, and replaces a setting they share.
TEST(OpenRocketContentHandler, PhotoStudioAndDocumentPreferencesAreReadEveryTime)
{
    ContentRun content(
        "<rocket/>"
        "<photostudio><roll>1.0</roll><yaw>2.0</yaw></photostudio>"
        "<photostudio><yaw>3.0</yaw><pitch>4.0</pitch></photostudio>"
        R"(<docprefs><pref key="a" type="string">x</pref></docprefs>)"
        R"(<docprefs><pref key="b" type="string">y</pref></docprefs>)");

    ASSERT_TRUE(content.run.result.has_value());
    EXPECT_THAT(content.texts(), IsEmpty());
    EXPECT_THAT(content.fixture.document().getPhotoSettings(),
                ElementsAre(Pair("pitch", "4.0"), Pair("roll", "1.0"), Pair("yaw", "3.0")));
    EXPECT_EQ(content.fixture.document().getDocumentPreferences().getString("a", "none"), "x");
    EXPECT_EQ(content.fixture.document().getDocumentPreferences().getString("b", "none"), "y");
}

// Any other element is ignored with everything in it; the names are compared exactly.
TEST(OpenRocketContentHandler, AnUnknownElementIsIgnoredWithItsContent)
{
    EXPECT_THAT(warningsOf("<foo/>"), ElementsAre("Unknown element foo, ignoring."));
    EXPECT_THAT(warningsOf("<Rocket/>"), ElementsAre("Unknown element Rocket, ignoring."));
    EXPECT_THAT(warningsOf("<openrocket/>"), ElementsAre("Unknown element openrocket, ignoring."));

    ContentRun content("<foo><rocket><name>Inner</name></rocket><bar/></foo>");
    ASSERT_TRUE(content.run.result.has_value());
    EXPECT_THAT(content.texts(), ElementsAre("Unknown element foo, ignoring."));
    EXPECT_EQ(content.fixture.rocket().getName(), "Rocket");
}

// None of the five elements has text or attributes of its own in a file OpenRocket writes:
// either is warned of when the element closes.
TEST(OpenRocketContentHandler, TextAndAttributesOfTheFiveElementsAreWarnedOf)
{
    for (const std::string_view element :
         {"rocket", "datatypes", "simulations", "photostudio", "docprefs"})
    {
        EXPECT_THAT(warningsOf(std::format("<{}/>", element)), IsEmpty()) << element;
        EXPECT_THAT(
            warningsOf(std::format(R"(<{} a="1"/>)", element)),
            ElementsAre(std::format("Unknown attributes in element '{}', ignoring.", element)))
            << element;
        EXPECT_THAT(warningsOf(std::format("<{0}>text</{0}>", element)),
                    ElementsAre(std::format("Unknown text in element '{}', ignoring.", element)))
            << element;
    }
}

// The slip of SimpleSax's bookkeeping (decision L1), as OpenRocket has it: an element that the
// handler of <simulations> ignores leaves its layer on the stacks, so <simulations> is closed
// with that element's attributes and warned of in their place (the case
// s-ignored-in-simulations of the probe).
TEST(OpenRocketContentHandler, AnElementIgnoredInsideIsWhatTheElementIsClosedWith)
{
    EXPECT_THAT(warningsOf(R"(<rocket/><simulations><bar x="1"/></simulations>)"),
                ElementsAre("Unknown element 'bar', ignoring.",
                            "Unknown attributes in element 'simulations', ignoring."));
    EXPECT_THAT(warningsOf(R"(<rocket/><simulations a="1"><bar/></simulations>)"),
                ElementsAre("Unknown element 'bar', ignoring."));
}

// Not OpenRocket's: there the document is null until <rocket> was opened, and a <photostudio>,
// a <docprefs> or a <simulation> before it ends the load with a NullPointerException. Here the
// document exists from the start, and each element is applied where it stands.
TEST(OpenRocketContentHandler, ElementsBeforeTheRocketAreAppliedToo)
{
    ContentRun content(
        std::format("<photostudio><roll>1.0</roll></photostudio>"
                    R"(<docprefs><pref key="a" type="string">x</pref></docprefs>)"
                    "<simulations>{}</simulations>{}",
                    kSimulation, kRocket));

    ASSERT_TRUE(content.run.result.has_value()) << content.run.result.error().message;
    EXPECT_THAT(content.texts(), IsEmpty());
    QtRocket::OpenRocketDocument& document = content.fixture.document();
    EXPECT_THAT(document.getPhotoSettings(), ElementsAre(Pair("roll", "1.0")));
    EXPECT_EQ(document.getDocumentPreferences().getString("a", "none"), "x");
    EXPECT_EQ(document.getRocket().getName(), "One");
    // The simulation made its flight configuration in the still empty rocket; the
    // <motorconfiguration> of the rocket that follows names the same one.
    ASSERT_EQ(document.getSimulationCount(), 1U);
    EXPECT_EQ(document.getRocket().getFlightConfigurationCount(), 1);
    EXPECT_EQ(document.getSimulation(0)->getFlightConfigurationId(),
              document.getRocket().getSelectedConfiguration().getId());
}

// A failure of a handler below is the failure of the reading: Java's exception message under
// ErrorCode::INVALID_ARGUMENT, as it is.
TEST(OpenRocketContentHandler, AFailureOfAHandlerBelowEndsTheReading)
{
    ContentRun inRocket(
        "<rocket><name>R</name><id>not-a-uuid</id><comment>late</comment></rocket>");
    ASSERT_FALSE(inRocket.run.result.has_value());
    EXPECT_EQ(inRocket.run.result.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(inRocket.run.result.error().message, "Invalid UUID string: not-a-uuid");
    // What was read before the failure is in the document.
    EXPECT_EQ(inRocket.fixture.rocket().getName(), "R");

    const ContentRun inSimulation(
        R"(<rocket/><simulations><simulation status="uptodate"><name>S</name></simulation>)"
        "</simulations>");
    ASSERT_FALSE(inSimulation.run.result.has_value());
    EXPECT_EQ(inSimulation.run.result.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(inSimulation.run.result.error().message,
              "Attempted to set the configuration to an error id. Not Allowed!");
}

// The handler needs the document of the context.
TEST(OpenRocketContentHandler, AContextWithoutADocumentIsABug)
{
    const DocumentLoadingContext empty;
    EXPECT_THROW(static_cast<void>(OpenRocketContentHandler(empty)), BugError);
}

}  // namespace
