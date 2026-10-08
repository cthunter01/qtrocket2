#include "QtRocket/file/openrocket/DatatypeHandler.h"

#include <array>
#include <iostream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/CustomExpressionHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/customexpression/CustomExpression.h"
#include "QtRocket/util/BugError.h"
#include "file/openrocket/DocumentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// DatatypeHandler: the handler of the <datatypes> element of a design file. What a file's text
// gives is compared with what OpenRocket makes of the same text.

namespace
{

using QtRocket::BugError;
using QtRocket::CustomExpression;
using QtRocket::CustomExpressionHandler;
using QtRocket::DatatypeHandler;
using QtRocket::DocumentLoadingContext;
using QtRocket::ElementHandler;
using QtRocket::FlightDataType;
using QtRocket::WarningSet;
using QtRocket::Test::DocumentCase;
using QtRocket::Test::documentCasesThatThrowWhenCutOff;
using QtRocket::Test::DocumentFixture;
using QtRocket::Test::failedDocumentCases;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::printedDocumentCases;
using QtRocket::Test::warningTexts;

using Texts = std::vector<std::string>;

// The tables are written by scripts/make_tables.py of the scratchpad's
// probes/tier9b-document-handlers from the cases (scripts/r4_cases.py) and OpenRocket's answers
// to them (DocumentProbe.java): do not edit them by hand.
// clang-format off
// BEGIN GENERATED TABLES DatatypeHandler
// What OpenRocket makes of each case (DocumentProbe.java of part R4), which QtRocket has to
// make of it too.
constexpr std::array<DocumentCase, 12> kJava{{
    {.name = "dt-empty", .xml = R"xml(<datatypes></datatypes>)xml", .expected = R"out(RESULT ok
ROOT datatypes {} []
EVENTS 0)out"},
    {.name = "dt-one", .xml = R"xml(<datatypes><type source="customexpression"><name>Kinetic energy r4a</name><symbol>qr4a</symbol><unit unittype="auto">J</unit><expression>0.5*m*Vt^2</expression></type></datatypes>)xml", .expected = R"out(RESULT ok
ROOT datatypes {} []
EVENTS 0
| expression name='Kinetic energy r4a' symbol='qr4a' unit='J' expression='0.5*m*Vt^2')out"},
    {.name = "dt-two", .xml = R"xml(<datatypes><type source="customexpression"><name>First r4b</name><symbol>qr4b</symbol><unit unittype="auto">m/s</unit><expression>Vt*2</expression></type><type source="customexpression"><name>Second r4c</name><symbol>qr4c</symbol><unit unittype="auto">m</unit><expression>h+1</expression></type></datatypes>)xml", .expected = R"out(RESULT ok
ROOT datatypes {} []
EVENTS 0
| expression name='First r4b' symbol='qr4b' unit='m/s' expression='Vt*2'
| expression name='Second r4c' symbol='qr4c' unit='m' expression='h+1')out"},
    {.name = "dt-equal-ones", .xml = R"xml(<datatypes><type source="customexpression"><name>Same r4d</name><symbol>qr4d</symbol><unit unittype="auto">m</unit><expression>h</expression></type><type source="customexpression"><name>Same r4d</name><symbol>qr4d</symbol><unit unittype="auto">m</unit><expression>h</expression></type></datatypes>)xml", .expected = R"out(RESULT ok
ROOT datatypes {} []
EVENTS 0
| expression name='Same r4d' symbol='qr4d' unit='m' expression='h'
| expression name='Same r4d' symbol='qr4d' unit='m' expression='h')out"},
    {.name = "dt-as-saved", .xml = R"xml(<datatypes>
    <type source="customexpression">
      <name>My expr r4e</name>
      <symbol>qr4e</symbol>
      <unit unittype="auto">m/s</unit>
      <expression>Vt*2</expression>
      </type>
  </datatypes>)xml", .expected = R"out(RESULT ok
ROOT datatypes {} []
EVENTS 0
| expression name='My expr r4e' symbol='qr4e' unit='m/s' expression='Vt*2')out"},
    {.name = "dt-text-and-attributes", .xml = R"xml(<datatypes b="2">txt<type source="customexpression" a="1"><name>N r4f</name><symbol>qr4f</symbol><unit unittype="auto">m</unit><expression>h</expression></type>more</datatypes>)xml", .expected = R"out(RESULT ok
W Unknown attributes in element 'type', ignoring.
ROOT datatypes {b=2} [txtmore]
EVENTS 0
| expression name='N r4f' symbol='qr4f' unit='m' expression='h')out"},
    {.name = "dt-text-in-a-type", .xml = R"xml(<datatypes><type source="customexpression">text<name>N r4g</name>more<symbol>qr4g</symbol></type></datatypes>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'type', ignoring.
ROOT datatypes {} []
EVENTS 0
| expression name='N r4g' symbol='qr4g' unit='' expression='')out"},
    {.name = "dt-type-of-another-source", .xml = R"xml(<datatypes><type source="other"><name>x</name></type></datatypes>)xml", .expected = R"out(RESULT ok
W Unknown datatype type defined, ignoring
W Unknown datatype name defined, ignoring
W Unknown text in element 'name', ignoring.
ROOT datatypes {} []
EVENTS 0)out"},
    {.name = "dt-type-of-another-source-with-more", .xml = R"xml(<datatypes><type source="other" a="1">text<name b="2">x</name></type></datatypes>)xml", .expected = R"out(RESULT ok
W Unknown datatype type defined, ignoring
W Unknown datatype name defined, ignoring
W Unknown text in element 'name', ignoring.
W Unknown attributes in element 'name', ignoring.
W Unknown text in element 'type', ignoring.
W Unknown attributes in element 'type', ignoring.
ROOT datatypes {} []
EVENTS 0)out"},
    {.name = "dt-sources-that-are-none", .xml = R"xml(<datatypes><type source="CustomExpression"/><type source=" customexpression"/><type source=""/><Type source="customexpression"/></datatypes>)xml", .expected = R"out(RESULT ok
W Unknown datatype type defined, ignoring
W Unknown datatype Type defined, ignoring
ROOT datatypes {} []
EVENTS 0)out"},
    {.name = "dt-other-children", .xml = R"xml(<datatypes><bogus source="customexpression"/><name>n</name><datatypes/></datatypes>)xml", .expected = R"out(RESULT ok
W Unknown datatype bogus defined, ignoring
W Unknown datatype name defined, ignoring
W Unknown text in element 'name', ignoring.
W Unknown datatype datatypes defined, ignoring
ROOT datatypes {} []
EVENTS 0)out"},
    {.name = "dt-other-children-before-an-expression", .xml = R"xml(<datatypes><type source="other"><name>x</name></type><bogus/><type source="customexpression"><name>My expr r4j</name><symbol>qr4j</symbol><unit unittype="auto">m/s</unit><expression>Vt*2</expression></type></datatypes>)xml", .expected = R"out(RESULT ok
W Unknown datatype type defined, ignoring
W Unknown datatype name defined, ignoring
W Unknown text in element 'name', ignoring.
W Unknown datatype bogus defined, ignoring
ROOT datatypes {} []
EVENTS 0
| expression name='My expr r4j' symbol='qr4j' unit='m/s' expression='Vt*2')out"},
}};

// Where QtRocket answers otherwise on purpose: the comment of a case says why and gives the
// lines of OpenRocket's answer that QtRocket does not give.
constexpr std::array<DocumentCase, 5> kOwn{{
    // Decision L4: a <type> without source, of which OpenRocket dies; here it is an unknown datatype.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Cannot invoke "String.equals(Object)" because the return value of "java.util.HashMap.get(Object)" is nul ...
    {.name = "dt-type-without-source", .xml = R"xml(<datatypes><type><name>x</name></type></datatypes>)xml", .expected = R"out(RESULT ok
W Unknown datatype type defined, ignoring
W Unknown datatype name defined, ignoring
W Unknown text in element 'name', ignoring.
ROOT datatypes {} []
EVENTS 0)out"},
    // Decision L4: a <type> without source, of which OpenRocket dies; here it is an unknown datatype, and the expression behind it is loaded.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Cannot invoke "String.equals(Object)" because the return value of "java.util.HashMap.get(Object)" is nul ...
    {.name = "dt-type-without-source-then-one-with", .xml = R"xml(<datatypes><type a="1"/><type source="customexpression"><name>After r4h</name><symbol>qr4h</symbol><unit unittype="auto">m</unit><expression>h</expression></type></datatypes>)xml", .expected = R"out(RESULT ok
W Unknown datatype type defined, ignoring
W Unknown attributes in element 'type', ignoring.
ROOT datatypes {} []
EVENTS 0
| expression name='After r4h' symbol='qr4h' unit='m' expression='h')out"},
    // Decision L5: one expression per custom expression element. OpenRocket adds it again when each of the three elements behind it closes.
    {.name = "dt-other-children-after-an-expression", .xml = R"xml(<datatypes><type source="customexpression"><name>My expr r4i</name><symbol>qr4i</symbol><unit unittype="auto">m/s</unit><expression>Vt*2</expression></type><type source="other"><name>x</name></type><bogus source="customexpression"/></datatypes>)xml", .expected = R"out(RESULT ok
W Unknown datatype type defined, ignoring
W Unknown datatype name defined, ignoring
W Unknown text in element 'name', ignoring.
W Unknown datatype bogus defined, ignoring
ROOT datatypes {} []
EVENTS 0
| expression name='My expr r4i' symbol='qr4i' unit='m/s' expression='Vt*2')out"},
    // Decision L5: one expression per custom expression element. OpenRocket adds it again when the element around it closes.
    {.name = "dt-expression-in-another-child", .xml = R"xml(<datatypes><group><type source="customexpression"><name>Inner r4k</name><symbol>qr4k</symbol><unit unittype="auto">m</unit><expression>h</expression></type></group></datatypes>)xml", .expected = R"out(RESULT ok
W Unknown datatype group defined, ignoring
ROOT datatypes {} []
EVENTS 0
| expression name='Inner r4k' symbol='qr4k' unit='m' expression='h')out"},
    // Decision L5: one expression per custom expression element. OpenRocket adds it again when each of the three elements behind it closes (the scout's case: four in all).
    {.name = "dt-scout", .xml = R"xml(<datatypes><type source="customexpression"><name>My expr r4l</name><symbol>qr4l</symbol><unit unittype="auto">m/s</unit><expression>Vt*2</expression></type><type source="other"><name>x</name></type><bogus source="customexpression"/></datatypes>)xml", .expected = R"out(RESULT ok
W Unknown datatype type defined, ignoring
W Unknown datatype name defined, ignoring
W Unknown text in element 'name', ignoring.
W Unknown datatype bogus defined, ignoring
ROOT datatypes {} []
EVENTS 0
| expression name='My expr r4l' symbol='qr4l' unit='m/s' expression='Vt*2')out"},
}};
// END GENERATED TABLES DatatypeHandler
// clang-format on

TEST(DatatypeHandler, ReadsItsCasesAsOpenRocket)
{
    EXPECT_EQ(failedDocumentCases(kJava), Texts{});
}

TEST(DatatypeHandler, ReadsItsOwnCasesAsDecided)
{
    EXPECT_EQ(failedDocumentCases(kOwn), Texts{});
}

// Not a test: prints what QtRocket makes of the cases, for scripts/make_tables.py. Run
// with --gtest_also_run_disabled_tests.
TEST(DatatypeHandler, DISABLED_PrintsCases)
{
    std::cout << printedDocumentCases(kJava) << printedDocumentCases(kOwn);
}

TEST(DatatypeHandler, NoCaseThrowsWhenItsTextIsCutOff)
{
    EXPECT_EQ(documentCasesThatThrowWhenCutOff(kJava), Texts{});
    EXPECT_EQ(documentCasesThatThrowWhenCutOff(kOwn), Texts{});
}

// A custom expression element has a handler of its own, and its expression joins the document
// when the element closes, once (decision L5): what closes afterwards adds nothing.
TEST(DatatypeHandler, AddsTheExpressionWhenItsElementCloses)
{
    DocumentFixture fixture;
    DatatypeHandler handler(fixture.context());
    WarningSet      warnings;

    ElementHandler* const child =
        handler.openElement("type", {{"source", "customexpression"}}, warnings).value_or(nullptr);
    auto* const expression = dynamic_cast<CustomExpressionHandler*>(child);
    ASSERT_NE(expression, nullptr);
    ASSERT_TRUE(expression->closeElement("name", {}, "Twice the time", warnings).has_value());
    ASSERT_TRUE(expression->closeElement("symbol", {}, "qr4twice", warnings).has_value());
    EXPECT_TRUE(fixture.document().getCustomExpressions().empty());

    ASSERT_TRUE(
        handler.closeElement("type", {{"source", "customexpression"}}, "", warnings).has_value());
    EXPECT_EQ(fixture.document().getCustomExpressions(),
              (std::vector<CustomExpression>{{"Twice the time", "qr4twice", "", ""}}));

    // An element of another kind closes: OpenRocket adds the expression once more here.
    ASSERT_TRUE(handler.closeElement("bogus", {}, "", warnings).has_value());
    ASSERT_TRUE(handler.closeElement("type", {{"source", "other"}}, "", warnings).has_value());
    EXPECT_EQ(fixture.document().getCustomExpressions().size(), 1U);
    EXPECT_TRUE(warnings.empty());
}

// Anything but a custom expression element is an unknown datatype, and the handler handles what
// is in it itself.
TEST(DatatypeHandler, HandlesAnUnknownDatatypeItself)
{
    DocumentFixture fixture;
    DatatypeHandler handler(fixture.context());
    WarningSet      warnings;

    EXPECT_EQ(handler.openElement("type", {{"source", "other"}}, warnings).value_or(nullptr),
              &handler);
    EXPECT_EQ(handler.openElement("bogus", {}, warnings).value_or(nullptr), &handler);
    // Decision L4: OpenRocket dies of a <type> without source.
    EXPECT_EQ(handler.openElement("type", {}, warnings).value_or(nullptr), &handler);
    EXPECT_EQ(warningTexts(warnings), (Texts{"Unknown datatype type defined, ignoring",
                                             "Unknown datatype bogus defined, ignoring"}));
    EXPECT_TRUE(fixture.document().getCustomExpressions().empty());
}

// Reading an expression makes no flight data type: the type of an expression is registered,
// process-wide, when someone asks for it (CustomExpression::getType()), not while a file loads.
TEST(DatatypeHandler, RegistersNoFlightDataType)
{
    DocumentFixture  fixture;
    const HandlerRun run = fixture.load(
        R"xml(<datatypes><type source="customexpression"><name>Never asked for r4</name><symbol>qr4NeverAskedFor</symbol><unit unittype="auto">m</unit><expression>h</expression></type></datatypes>)xml");
    ASSERT_TRUE(run.result.has_value());
    ASSERT_EQ(fixture.document().getCustomExpressions().size(), 1U);
    EXPECT_EQ(FlightDataType::findBySymbol("qr4NeverAskedFor"), nullptr);
    EXPECT_EQ(FlightDataType::findByName("Never asked for r4"), nullptr);
}

TEST(DatatypeHandler, AContextWithoutADocumentIsABug)
{
    const DocumentLoadingContext context;
    EXPECT_THROW(DatatypeHandler{context}, BugError);
}

}  // namespace
