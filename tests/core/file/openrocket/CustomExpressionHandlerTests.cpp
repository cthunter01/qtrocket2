#include "QtRocket/file/openrocket/CustomExpressionHandler.h"

#include <array>
#include <iostream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/customexpression/CustomExpression.h"
#include "file/openrocket/DocumentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// CustomExpressionHandler: the handler of one <type source="customexpression"> element of a
// design file's <datatypes>. What a file's text gives is compared with what OpenRocket makes of
// the same text; the cases are <datatypes> elements, read by the DatatypeHandler above it.

namespace
{

using QtRocket::CustomExpression;
using QtRocket::CustomExpressionHandler;
using QtRocket::Warning;
using QtRocket::WarningSet;
using QtRocket::Test::DocumentCase;
using QtRocket::Test::documentCasesThatThrowWhenCutOff;
using QtRocket::Test::DocumentFixture;
using QtRocket::Test::failedDocumentCases;
using QtRocket::Test::printedDocumentCases;
using QtRocket::Test::warningTexts;

using Texts = std::vector<std::string>;

// The tables are written by scripts/make_tables.py of the scratchpad's
// probes/tier9b-document-handlers from the cases (scripts/r4_cases.py) and OpenRocket's answers
// to them (DocumentProbe.java): do not edit them by hand.
// clang-format off
// BEGIN GENERATED TABLES CustomExpressionHandler
// What OpenRocket makes of each case (DocumentProbe.java of part R4), which QtRocket has to
// make of it too.
constexpr std::array<DocumentCase, 12> kJava{{
    {.name = "ce-empty", .xml = R"xml(<datatypes><type source="customexpression"></type></datatypes>)xml", .expected = R"out(RESULT ok
ROOT datatypes {} []
EVENTS 0
| expression name='' symbol='' unit='' expression='')out"},
    {.name = "ce-all-four", .xml = R"xml(<datatypes><type source="customexpression"><name>Kinetic energy r4m</name><symbol>qr4m</symbol><unit unittype="auto">J</unit><expression>0.5*m*Vt^2</expression></type></datatypes>)xml", .expected = R"out(RESULT ok
ROOT datatypes {} []
EVENTS 0
| expression name='Kinetic energy r4m' symbol='qr4m' unit='J' expression='0.5*m*Vt^2')out"},
    {.name = "ce-any-order", .xml = R"xml(<datatypes><type source="customexpression"><expression>h*2</expression><unit unittype="auto">m</unit><symbol>qr4n</symbol><name>Twice r4n</name></type></datatypes>)xml", .expected = R"out(RESULT ok
ROOT datatypes {} []
EVENTS 0
| expression name='Twice r4n' symbol='qr4n' unit='m' expression='h*2')out"},
    {.name = "ce-texts-as-they-are", .xml = R"xml(<datatypes><type source="customexpression"><name> spaced r4o </name><symbol> qr4o </symbol><unit unittype="auto"> m </unit><expression> h +
1 </expression></type></datatypes>)xml", .expected = R"out(RESULT ok
ROOT datatypes {} []
EVENTS 0
| expression name=' spaced r4o ' symbol=' qr4o ' unit=' m ' expression=' h +\n1 ')out"},
    {.name = "ce-empty-texts", .xml = R"xml(<datatypes><type source="customexpression"><name></name><symbol/><unit unittype="auto"></unit><expression></expression></type></datatypes>)xml", .expected = R"out(RESULT ok
ROOT datatypes {} []
EVENTS 0
| expression name='' symbol='' unit='' expression='')out"},
    {.name = "ce-twice", .xml = R"xml(<datatypes><type source="customexpression"><name>First r4p</name><name>Second r4p</name><symbol>qr4pa</symbol><symbol>qr4p</symbol><unit unittype="auto">m</unit><unit unittype="auto">s</unit><expression>h</expression><expression>t</expression></type></datatypes>)xml", .expected = R"out(RESULT ok
ROOT datatypes {} []
EVENTS 0
| expression name='Second r4p' symbol='qr4p' unit='s' expression='t')out"},
    {.name = "ce-unit-types", .xml = R"xml(<datatypes><type source="customexpression"><name>Auto r4q</name><symbol>qr4q</symbol><unit unittype="auto">m</unit></type><type source="customexpression"><name>Fixed r4r</name><symbol>qr4r</symbol><unit unittype="fixed">m</unit></type><type source="customexpression"><name>Empty r4s</name><symbol>qr4s</symbol><unit unittype="">m</unit></type><type source="customexpression"><name>Upper r4t</name><symbol>qr4t</symbol><unit unittype="AUTO">m</unit></type><type source="customexpression"><name>Both r4u</name><symbol>qr4u</symbol><unit unittype="auto">m</unit><unit unittype="other">s</unit></type></datatypes>)xml", .expected = R"out(RESULT ok
ROOT datatypes {} []
EVENTS 0
| expression name='Auto r4q' symbol='qr4q' unit='m' expression=''
| expression name='Fixed r4r' symbol='qr4r' unit='' expression=''
| expression name='Empty r4s' symbol='qr4s' unit='' expression=''
| expression name='Upper r4t' symbol='qr4t' unit='' expression=''
| expression name='Both r4u' symbol='qr4u' unit='m' expression='')out"},
    {.name = "ce-other-children", .xml = R"xml(<datatypes><type source="customexpression"><name>Others r4x</name><symbol>qr4x</symbol><bogus a="1">text</bogus><Name>no</Name><description>d</description></type></datatypes>)xml", .expected = R"out(RESULT ok
ROOT datatypes {} []
EVENTS 0
| expression name='Others r4x' symbol='qr4x' unit='' expression='')out"},
    {.name = "ce-attributes-and-text", .xml = R"xml(<datatypes><type source="customexpression">text<name a="1">Attributes r4y</name><symbol b="2">qr4y</symbol><expression c="3">h</expression>more</type></datatypes>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'type', ignoring.
ROOT datatypes {} []
EVENTS 0
| expression name='Attributes r4y' symbol='qr4y' unit='' expression='h')out"},
    {.name = "ce-children-in-children", .xml = R"xml(<datatypes><type source="customexpression"><group><name>Deep r4z</name><more><symbol>qr4z</symbol></more></group><name>x<unit unittype="auto">m</unit></name></type></datatypes>)xml", .expected = R"out(RESULT ok
ROOT datatypes {} []
EVENTS 0
| expression name='x' symbol='qr4z' unit='m' expression='')out"},
    {.name = "ce-special-characters", .xml = R"xml(<datatypes><type source="customexpression"><name>a &amp; b, (c) [d] r4ac</name><symbol>q.r4ac</symbol><unit unittype="auto">m/s^2</unit><expression>mean(h[0:t]) &lt; 3</expression></type></datatypes>)xml", .expected = R"out(RESULT ok
ROOT datatypes {} []
EVENTS 0
| expression name='a & b, (c) [d] r4ac' symbol='q.r4ac' unit='m/s^2' expression='mean(h[0:t]) < 3')out"},
    {.name = "ce-like-a-built-in-type", .xml = R"xml(<datatypes><type source="customexpression"><name>Altitude</name><symbol>h</symbol><unit unittype="auto">m</unit><expression>t</expression></type></datatypes>)xml", .expected = R"out(RESULT ok
ROOT datatypes {} []
EVENTS 0
| expression name='Altitude' symbol='h' unit='m' expression='t')out"},
}};

// Where QtRocket answers otherwise on purpose: the comment of a case says why and gives the
// lines of OpenRocket's answer that QtRocket does not give.
constexpr std::array<DocumentCase, 5> kOwn{{
    // Decision L4: a <unit> without unittype, of which OpenRocket dies; here it is passed over with a warning and the expression has no unit.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Cannot invoke "String.equals(Object)" because the return value of "java.util.HashMap.get(Object)" is nul ...
    {.name = "ce-unit-without-a-type", .xml = R"xml(<datatypes><type source="customexpression"><name>No unit type r4v</name><symbol>qr4v</symbol><unit>m</unit><expression>h</expression></type></datatypes>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT datatypes {} []
EVENTS 0
| expression name='No unit type r4v' symbol='qr4v' unit='' expression='h')out"},
    // Decision L4: a <unit> without unittype, of which OpenRocket dies; here it is passed over with a warning and the unit read before stays.
    // OpenRocket: RESULT THROWN java.lang.NullPointerException: Cannot invoke "String.equals(Object)" because the return value of "java.util.HashMap.get(Object)" is nul ...
    {.name = "ce-unit-without-a-type-after-one-with", .xml = R"xml(<datatypes><type source="customexpression"><name>Kept r4w</name><symbol>qr4w</symbol><unit unittype="auto">m</unit><unit>s</unit></type></datatypes>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT datatypes {} []
EVENTS 0
| expression name='Kept r4w' symbol='qr4w' unit='m' expression='')out"},
    // Decision L5: one expression per custom expression element. OpenRocket adds it also when the <type> in it closes.
    {.name = "ce-type-in-a-type", .xml = R"xml(<datatypes><type source="customexpression"><name>Nested r4aa</name><type/><symbol>qr4aa</symbol></type></datatypes>)xml", .expected = R"out(RESULT ok
ROOT datatypes {} []
EVENTS 0
| expression name='Nested r4aa' symbol='qr4aa' unit='' expression='')out"},
    // Decision L5: one expression per custom expression element. OpenRocket adds it also when the <type> in it closes, which is read by the handler of the outer one and changes its name.
    {.name = "ce-custom-type-in-a-type", .xml = R"xml(<datatypes><type source="customexpression"><name>Outer r4ab</name><symbol>qr4ab</symbol><type source="customexpression"><name>Inner r4ab</name></type></type></datatypes>)xml", .expected = R"out(RESULT ok
ROOT datatypes {} []
EVENTS 0
| expression name='Inner r4ab' symbol='qr4ab' unit='' expression='')out"},
    // An expression here has no document and builds no parser (run 9a): OpenRocket makes a regular expression of the symbols of the document's expressions when the second one is made, and the first one's symbol is none.
    // OpenRocket: RESULT THROWN java.util.regex.PatternSyntaxException: Unclosed group near index 240
    {.name = "ce-symbol-that-is-no-regular-expression", .xml = R"xml(<datatypes><type source="customexpression"><name>Bracket r4ad</name><symbol>(</symbol><unit unittype="auto">m</unit><expression>h</expression></type><type source="customexpression"><name>After r4ae</name><symbol>qr4ae</symbol><unit unittype="auto">m</unit><expression>h</expression></type></datatypes>)xml", .expected = R"out(RESULT ok
ROOT datatypes {} []
EVENTS 0
| expression name='Bracket r4ad' symbol='(' unit='m' expression='h'
| expression name='After r4ae' symbol='qr4ae' unit='m' expression='h')out"},
}};
// END GENERATED TABLES CustomExpressionHandler
// clang-format on

TEST(CustomExpressionHandler, ReadsItsCasesAsOpenRocket)
{
    EXPECT_EQ(failedDocumentCases(kJava), Texts{});
}

TEST(CustomExpressionHandler, ReadsItsOwnCasesAsDecided)
{
    EXPECT_EQ(failedDocumentCases(kOwn), Texts{});
}

// Not a test: prints what QtRocket makes of the cases, for scripts/make_tables.py. Run
// with --gtest_also_run_disabled_tests.
TEST(CustomExpressionHandler, DISABLED_PrintsCases)
{
    std::cout << printedDocumentCases(kJava) << printedDocumentCases(kOwn);
}

TEST(CustomExpressionHandler, NoCaseThrowsWhenItsTextIsCutOff)
{
    EXPECT_EQ(documentCasesThatThrowWhenCutOff(kJava), Texts{});
    EXPECT_EQ(documentCasesThatThrowWhenCutOff(kOwn), Texts{});
}

// The handler handles every element below its own, and its expression is what it has read so
// far: four empty texts at first, each replaced by the text of the element of its name.
TEST(CustomExpressionHandler, CollectsTheFourTextsOfAnExpression)
{
    DocumentFixture         fixture;
    CustomExpressionHandler handler(fixture.context());
    WarningSet              warnings;
    EXPECT_EQ(handler.getExpression(), CustomExpression());

    EXPECT_EQ(handler.openElement("name", {}, warnings).value_or(nullptr), &handler);
    EXPECT_EQ(handler.openElement("bogus", {}, warnings).value_or(nullptr), &handler);
    EXPECT_EQ(
        handler.openElement("type", {{"source", "customexpression"}}, warnings).value_or(nullptr),
        &handler);

    ASSERT_TRUE(handler.closeElement("name", {}, " Kinetic energy ", warnings).has_value());
    ASSERT_TRUE(handler.closeElement("symbol", {}, "qr4ke", warnings).has_value());
    ASSERT_TRUE(handler.closeElement("unit", {{"unittype", "auto"}}, "J", warnings).has_value());
    ASSERT_TRUE(handler.closeElement("expression", {}, "0.5*m*Vt^2", warnings).has_value());
    EXPECT_EQ(handler.getExpression(),
              CustomExpression(" Kinetic energy ", "qr4ke", "J", "0.5*m*Vt^2"));

    // A unit of another type than "auto" is not the expression's; anything else is passed
    // over, text and attributes included.
    ASSERT_TRUE(handler.closeElement("unit", {{"unittype", "fixed"}}, "W", warnings).has_value());
    ASSERT_TRUE(handler.closeElement("bogus", {{"a", "1"}}, "text", warnings).has_value());
    ASSERT_TRUE(handler.closeElement("type", {}, "", warnings).has_value());
    ASSERT_TRUE(handler.endHandler("type", {{"a", "1"}}, "text", warnings).has_value());
    EXPECT_EQ(handler.getExpression(),
              CustomExpression(" Kinetic energy ", "qr4ke", "J", "0.5*m*Vt^2"));
    EXPECT_TRUE(warnings.empty());
    // The document gets the expression from the handler of <datatypes>, not from this one.
    EXPECT_TRUE(fixture.document().getCustomExpressions().empty());
}

// Decision L4: OpenRocket dies of a <unit> without unittype; here it is passed over with a
// warning and the unit stays.
TEST(CustomExpressionHandler, PassesAUnitWithoutItsTypeOver)
{
    DocumentFixture         fixture;
    CustomExpressionHandler handler(fixture.context());
    WarningSet              warnings;
    ASSERT_TRUE(handler.closeElement("unit", {{"unittype", "auto"}}, "m", warnings).has_value());
    ASSERT_TRUE(handler.closeElement("unit", {}, "s", warnings).has_value());
    EXPECT_EQ(handler.getExpression().getUnit(), "m");
    EXPECT_EQ(warningTexts(warnings), Texts{Warning::kFileInvalidParameter.toString()});
}

}  // namespace
