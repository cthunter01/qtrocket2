// Tests of OpenRocketHandler, the handler of the root element of a design file (OpenRocket's
// file/openrocket/importt/OpenRocketHandler). OpenRocket has no test of the class; what is
// expected is what OpenRocket gives for the same documents (TopProbe.java of the probes of
// tier 9c, part "loader": the "H" lines of its output, in file/RocketLoaderCases.h).

#include "QtRocket/file/openrocket/OpenRocketHandler.h"

#include <format>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/SimpleSax.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Error.h"
#include "file/RocketLoaderCases.h"
#include "file/RocketLoaderTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

namespace
{

using QtRocket::ElementHandler;
using QtRocket::ErrorCode;
using QtRocket::OpenRocketHandler;
using QtRocket::Result;
using QtRocket::SimpleSax;
using QtRocket::WarningSet;
using QtRocket::Test::failedLoaderCases;
using QtRocket::Test::HandlerFixture;
using QtRocket::Test::kOwnRootHandlerCases;
using QtRocket::Test::kRootHandlerCases;
using QtRocket::Test::printedLoaderCases;
using QtRocket::Test::rootHandlerLinesOf;
using QtRocket::Test::warningTexts;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

/// What the handler makes of @p document read as text: the result, the warnings and the
/// fixture's context afterwards.
struct RootRun
{
    HandlerFixture fixture;
    WarningSet     warnings;
    Result<void>   result;

    explicit RootRun(std::string_view document)
    {
        fixture.context().setFileVersion(0);
        OpenRocketHandler handler(fixture.context());
        result = SimpleSax::readXml(document, handler, warnings);
    }

    [[nodiscard]] std::vector<std::string> texts() const { return warningTexts(warnings); }
    [[nodiscard]] int                      version() { return fixture.context().getFileVersion(); }
};

// ------------------------------------------------------------------- the cases of the probe

// Every document of the probe that is about the root element, its content or the slip of
// SimpleSax's bookkeeping after an ignored element gives the warnings, the failure and the
// file version that OpenRocket gives.
TEST(OpenRocketHandler, GivesWhatOpenRocketGivesForEveryCaseOfTheProbe)
{
    EXPECT_EQ(failedLoaderCases(kRootHandlerCases, rootHandlerLinesOf), "");
}

// The three cases with an expectation of QtRocket's own: OpenRocket has no document before
// the <rocket> element was opened, so a <photostudio>, a <docprefs> or a <simulation> before
// it ends its load with a NullPointerException. Here the document exists from the start.
TEST(OpenRocketHandler, OnlyTheElementsBeforeTheRocketAreAnsweredOtherwiseThanOpenRocket)
{
    EXPECT_THAT(kOwnRootHandlerCases,
                ElementsAre("c-docprefs-first", "c-photostudio-first", "c-simulations-first"));
}

// For the script that compares with the probe's output: the cases as the probe prints them.
TEST(OpenRocketHandler, DISABLED_PrintsTheCasesOfTheProbe)
{
    std::cout << printedLoaderCases(kRootHandlerCases, 'H', rootHandlerLinesOf);
}

// ------------------------------------------------------------------------- the root element

// The smallest document: its content goes to the content handler, which reads the rocket.
TEST(OpenRocketHandler, HandsTheContentOfTheRootElementToTheContentHandler)
{
    RootRun run(R"(<openrocket version="1.10"><rocket><name>R</name></rocket></openrocket>)");

    ASSERT_TRUE(run.result.has_value()) << run.result.error().message;
    EXPECT_THAT(run.texts(), IsEmpty());
    EXPECT_EQ(run.fixture.rocket().getName(), "R");
    EXPECT_EQ(run.version(), 110);
}

// A root of another name is ignored with everything in it: the document stays the empty
// rocket, and the file version stays what it was.
TEST(OpenRocketHandler, ARootOfAnotherNameIsIgnoredWithItsContent)
{
    RootRun run(R"(<rocketdoc version="1.10"><rocket><name>R</name></rocket></rocketdoc>)");

    ASSERT_TRUE(run.result.has_value());
    EXPECT_THAT(run.texts(), ElementsAre("Unknown element rocketdoc, ignoring."));
    EXPECT_EQ(run.fixture.rocket().getName(), "Rocket");
    EXPECT_EQ(run.version(), 0);
}

// The name is compared exactly.
TEST(OpenRocketHandler, TheNameOfTheRootIsComparedExactly)
{
    RootRun run(R"(<OpenRocket version="1.10"><rocket><name>R</name></rocket></OpenRocket>)");

    EXPECT_THAT(run.texts(), ElementsAre("Unknown element OpenRocket, ignoring."));
    EXPECT_EQ(run.fixture.rocket().getName(), "Rocket");
}

// No document can hold a second root element, and an <openrocket> inside the first one is the
// content handler's (the case h-openrocket-nested). A caller that hands the handler a second
// one gets Java's warning, and the element is ignored.
TEST(OpenRocketHandler, ASecondRootElementIsIgnoredWithAWarning)
{
    HandlerFixture                   fixture;
    OpenRocketHandler                handler(fixture.context());
    WarningSet                       warnings;
    const ElementHandler::Attributes attributes{{"version", "1.10"}};

    const Result<ElementHandler*> first = handler.openElement("openrocket", attributes, warnings);
    ASSERT_TRUE(first.has_value());
    EXPECT_NE(*first, nullptr);
    EXPECT_THAT(warningTexts(warnings), IsEmpty());

    const Result<ElementHandler*> second =
        handler.openElement("openrocket", {{"version", "1.4"}}, warnings);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, nullptr);
    EXPECT_THAT(warningTexts(warnings),
                ElementsAre("Multiple document elements found, ignoring later ones."));
    // The second element changed nothing.
    EXPECT_EQ(fixture.context().getFileVersion(), 110);
}

// ------------------------------------------------------------------------------ the version

// Every version the loader knows is read without a warning and gives its file version.
TEST(OpenRocketHandler, EverySupportedVersionIsReadWithoutAWarning)
{
    int expected = 100;
    for (const std::string_view version : QtRocket::DocumentConfig::kSupportedVersions)
    {
        RootRun run(std::format(R"(<openrocket version="{}"><rocket/></openrocket>)", version));
        EXPECT_THAT(run.texts(), IsEmpty()) << version;
        EXPECT_EQ(run.version(), expected) << version;
        expected++;
    }
}

// The warning for a version that is not known: with the version when the attribute is there,
// with the creator when that is there and not blank, trimmed.
TEST(OpenRocketHandler, AnUnknownVersionIsReadWithAWarningThatNamesVersionAndCreator)
{
    EXPECT_THAT(RootRun(R"(<openrocket><rocket/></openrocket>)").texts(),
                ElementsAre("Unsupported document version, attempting to read file anyway."));
    EXPECT_THAT(RootRun(R"(<openrocket version="2.0"><rocket/></openrocket>)").texts(),
                ElementsAre("Unsupported document version 2.0, attempting to read file anyway."));
    EXPECT_THAT(
        RootRun(R"(<openrocket version="2.0" creator="  My Tool  "><rocket/></openrocket>)")
            .texts(),
        ElementsAre("Unsupported document version 2.0 (written using 'My Tool'), attempting to "
                    "read file anyway."));
    EXPECT_THAT(
        RootRun(R"(<openrocket creator="X"><rocket/></openrocket>)").texts(),
        ElementsAre(
            "Unsupported document version (written using 'X'), attempting to read file anyway."));
    EXPECT_THAT(
        RootRun(R"(<openrocket version="2.0" creator="   "><rocket/></openrocket>)").texts(),
        ElementsAre("Unsupported document version 2.0, attempting to read file anyway."));
}

// The file is read all the same.
TEST(OpenRocketHandler, AFileOfAnUnknownVersionIsReadAllTheSame)
{
    RootRun run(R"(<openrocket version="7.3"><rocket><name>Later</name></rocket></openrocket>)");

    ASSERT_TRUE(run.result.has_value());
    EXPECT_EQ(run.fixture.rocket().getName(), "Later");
    EXPECT_EQ(run.version(), 703);
}

// "1.010" is the file version 110, and is no version the loader knows by its text.
TEST(OpenRocketHandler, AVersionWithALeadingZeroCountsAsItsNumberAndWarns)
{
    RootRun run(R"(<openrocket version="1.010"><rocket/></openrocket>)");

    EXPECT_THAT(run.texts(),
                ElementsAre("Unsupported document version 1.010, attempting to read file anyway."));
    EXPECT_EQ(run.version(), 110);
}

/// parseVersion() of @p text, or -1 for a failure.
[[nodiscard]] int version(std::string_view text)
{
    const Result<int> parsed = OpenRocketHandler::parseVersion(text);
    return parsed.has_value() ? *parsed : -1;
}

// parseVersion(): digits, a full stop, digits and nothing else; anything else is 0.
TEST(OpenRocketHandler, ParseVersionReadsMajorAndMinorNumbers)
{
    EXPECT_EQ(OpenRocketHandler::parseVersion(std::nullopt), 0);
    EXPECT_EQ(version("1.0"), 100);
    EXPECT_EQ(version("1.11"), 111);
    EXPECT_EQ(version("1.12"), 112);
    EXPECT_EQ(version("0.9"), 9);
    EXPECT_EQ(version("12.34"), 1234);
    EXPECT_EQ(version("01.5"), 105);
    EXPECT_EQ(version("1.100"), 200);
    EXPECT_EQ(version(""), 0);
    EXPECT_EQ(version("1"), 0);
    EXPECT_EQ(version("1."), 0);
    EXPECT_EQ(version(".1"), 0);
    EXPECT_EQ(version("."), 0);
    EXPECT_EQ(version("1.x"), 0);
    EXPECT_EQ(version("1.5.1"), 0);
    EXPECT_EQ(version(" 1.10"), 0);
    EXPECT_EQ(version("1.10 "), 0);
    EXPECT_EQ(version("1.10\n"), 0);
    EXPECT_EQ(version("+1.10"), 0);
    EXPECT_EQ(version("-1.10"), 0);
    EXPECT_EQ(version("1,10"), 0);
    // Digits of another script are no digits (Java's [0-9] says the same).
    EXPECT_EQ(version("\xD9\xA1.\xD9\xA1\xD9\xA0"), 0);
}

// Java computes major * 100 + minor as an int, which wraps (Java: -2147483596 and
// -2147483597).
TEST(OpenRocketHandler, ParseVersionWrapsAsJavasIntArithmeticDoes)
{
    EXPECT_EQ(OpenRocketHandler::parseVersion("21474836.47"), 2147483647);
    EXPECT_EQ(OpenRocketHandler::parseVersion("21474837.0"), -2147483596);
    EXPECT_EQ(OpenRocketHandler::parseVersion("21474836.99"), -2147483597);
    EXPECT_EQ(OpenRocketHandler::parseVersion("2147483647.2147483647"), 2147483547);
}

// A number an int cannot hold is Java's NumberFormatException, which fails the load: the
// message is Java's, and the warning for the unknown version was given before.
TEST(OpenRocketHandler, AVersionNumberBeyondAnIntFailsTheLoad)
{
    const Result<int> major = OpenRocketHandler::parseVersion("99999999999.1");
    ASSERT_FALSE(major.has_value());
    EXPECT_EQ(major.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(major.error().message, "For input string: \"99999999999\"");

    const Result<int> minor = OpenRocketHandler::parseVersion("1.2147483648");
    ASSERT_FALSE(minor.has_value());
    EXPECT_EQ(minor.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(minor.error().message, "For input string: \"2147483648\"");

    RootRun run(
        R"(<openrocket version="99999999999.1"><rocket><name>R</name></rocket></openrocket>)");
    ASSERT_FALSE(run.result.has_value());
    EXPECT_EQ(run.result.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(run.result.error().message, "For input string: \"99999999999\"");
    EXPECT_THAT(run.texts(), ElementsAre("Unsupported document version 99999999999.1, attempting "
                                         "to read file anyway."));
    // Nothing of the content was read.
    EXPECT_EQ(run.fixture.rocket().getName(), "Rocket");
    EXPECT_EQ(run.version(), 0);
}

// ----------------------------------------------------------- when the root element closes

// Text in the root element and attributes other than version and creator are warned of.
TEST(OpenRocketHandler, TextAndOtherAttributesOfTheRootElementAreWarnedOf)
{
    EXPECT_THAT(RootRun(R"(<openrocket version="1.10" creator="c"><rocket/></openrocket>)").texts(),
                IsEmpty());
    EXPECT_THAT(RootRun(R"(<openrocket version="1.10">text<rocket/></openrocket>)").texts(),
                ElementsAre("Unknown text in element 'openrocket', ignoring."));
    EXPECT_THAT(
        RootRun(R"(<openrocket version="1.10" creator="c" foo="bar"><rocket/></openrocket>)")
            .texts(),
        ElementsAre("Unknown attributes in element 'openrocket', ignoring."));
    EXPECT_THAT(
        RootRun(R"(<openrocket version="1.10" foo="bar">text<rocket/></openrocket>)").texts(),
        ElementsAre("Unknown text in element 'openrocket', ignoring.",
                    "Unknown attributes in element 'openrocket', ignoring."));
}

// closeElement() judges a copy of the attributes without version and creator: the map it is
// given stays as it is (Java removes the two from its map).
TEST(OpenRocketHandler, ClosingLeavesTheAttributesItIsGivenAsTheyAre)
{
    HandlerFixture                   fixture;
    OpenRocketHandler                handler(fixture.context());
    WarningSet                       warnings;
    const ElementHandler::Attributes attributes{{"creator", "c"}, {"version", "1.10"}};

    const Result<void> closed = handler.closeElement("openrocket", attributes, "", warnings);

    ASSERT_TRUE(closed.has_value());
    EXPECT_THAT(warningTexts(warnings), IsEmpty());
    EXPECT_EQ(attributes.size(), 2U);
}

// The bookkeeping slip of SimpleSax's DelegatorHandler, which is OpenRocket's (decision L1):
// an element that a handler ignores leaves its layer of text and attributes on the stacks, so
// the root element is closed with the layer of the last element ignored inside it. An unknown
// element between <rocket> and <simulations> therefore hides the root's own unknown
// attributes and brings its own, and of the root's text only what follows it counts. Each
// expectation is OpenRocket's (the cases s-* of the probe).
TEST(OpenRocketHandler, AnIgnoredElementInTheRootShiftsWhatTheRootIsClosedWith)
{
    // The root's unknown attribute is warned of ...
    EXPECT_THAT(RootRun(R"(<openrocket version="1.10" extra="1"><rocket><name>R</name></rocket>)"
                        R"(<simulations/></openrocket>)")
                    .texts(),
                ElementsAre("Unknown attributes in element 'openrocket', ignoring."));
    // ... but not behind an ignored element without attributes,
    EXPECT_THAT(RootRun(R"(<openrocket version="1.10" extra="1"><rocket><name>R</name></rocket>)"
                        R"(<foo/><simulations/></openrocket>)")
                    .texts(),
                ElementsAre("Unknown element foo, ignoring."));
    // whose own attributes are warned of in the root's name,
    EXPECT_THAT(RootRun(R"(<openrocket version="1.10"><rocket><name>R</name></rocket>)"
                        R"(<foo a="1">text</foo><simulations></simulations></openrocket>)")
                    .texts(),
                ElementsAre("Unknown element foo, ignoring.",
                            "Unknown attributes in element 'openrocket', ignoring."));
    // unless they are called version and creator.
    EXPECT_THAT(RootRun(R"(<openrocket version="1.10"><rocket><name>R</name></rocket>)"
                        R"(<foo version="9" creator="z"/><simulations/></openrocket>)")
                    .texts(),
                ElementsAre("Unknown element foo, ignoring."));
    // The text before the ignored element is forgotten, the text after it is the root's.
    EXPECT_THAT(RootRun(R"(<openrocket version="1.10">early<rocket/><foo/></openrocket>)").texts(),
                ElementsAre("Unknown element foo, ignoring."));
    EXPECT_THAT(RootRun(R"(<openrocket version="1.10"><rocket/><foo/>late</openrocket>)").texts(),
                ElementsAre("Unknown element foo, ignoring.",
                            "Unknown text in element 'openrocket', ignoring."));
    // A second <rocket> is an ignored element like any other.
    EXPECT_THAT(
        RootRun(R"(<openrocket version="1.10"><rocket/><rocket a="b"/></openrocket>)").texts(),
        ElementsAre("Multiple rocket designs within one document, ignoring later ones.",
                    "Unknown attributes in element 'openrocket', ignoring."));
}

}  // namespace
