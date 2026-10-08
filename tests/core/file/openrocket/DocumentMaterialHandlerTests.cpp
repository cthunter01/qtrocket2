#include "QtRocket/file/openrocket/DocumentMaterialHandler.h"

#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/DocumentPreferencesHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialDatabase.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/DocumentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// DocumentMaterialHandler: the handler of the <docmaterials> element of a document's
// preferences. What a file's text gives is compared with what OpenRocket makes of the same text.

namespace
{

using QtRocket::BugError;
using QtRocket::DocumentLoadingContext;
using QtRocket::DocumentMaterialHandler;
using QtRocket::DocumentPreferencesHandler;
using QtRocket::ErrorCode;
using QtRocket::Material;
using QtRocket::MaterialDatabase;
using QtRocket::MaterialGroup;
using QtRocket::PlainTextHandler;
using QtRocket::Result;
using QtRocket::WarningSet;
using QtRocket::Test::DocumentCase;
using QtRocket::Test::documentCasesThatThrowWhenCutOff;
using QtRocket::Test::DocumentFixture;
using QtRocket::Test::failedDocumentCases;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::printedDocumentCases;
using QtRocket::Test::RocketLoadFixture;
using QtRocket::Test::runHandler;

using Texts = std::vector<std::string>;

// The tables are written by scripts/make_tables.py of the scratchpad's
// probes/tier9b-document-handlers from the cases (scripts/r4_cases.py) and OpenRocket's answers
// to them (DocumentProbe.java): do not edit them by hand.
// clang-format off
// BEGIN GENERATED TABLES DocumentMaterialHandler
// What OpenRocket makes of each case (DocumentProbe.java of part R4), which QtRocket has to
// make of it too.
constexpr std::array<DocumentCase, 26> kJava{{
    {.name = "dm-empty", .xml = R"xml(<docmaterials></docmaterials>)xml", .expected = R"out(RESULT ok
ROOT docmaterials {} []
EVENTS 0)out"},
    {.name = "dm-forms", .xml = R"xml(<docmaterials><material>BULK|Five|680.5|4.0E8|Woods</material><material>BULK|Four|680.5|Woods</material><material>BULK|Three|680.5</material><material>BULK|Shear only|680.5|12.5</material></docmaterials>)xml", .expected = R"out(RESULT ok
ROOT docmaterials {} []
EVENTS 0
| material [BULK|Five|680.5|4.0E8|Woods] document=true user=true
| material [BULK|Four|680.5|0.0|Woods] document=true user=true
| material [BULK|Shear only|680.5|12.5|Custom] document=true user=true
| material [BULK|Three|680.5|0.0|Custom] document=true user=true)out"},
    {.name = "dm-types", .xml = R"xml(<docmaterials><material>BULK|B|1.0</material><material>SURFACE|S|0.05</material><material>LINE|L|0.001</material></docmaterials>)xml", .expected = R"out(RESULT ok
ROOT docmaterials {} []
EVENTS 0
| material [BULK|B|1.0|0.0|Custom] document=true user=true
| material [LINE|L|0.001|0.0|Custom] document=true user=true
| material [SURFACE|S|0.05|0.0|Custom] document=true user=true)out"},
    {.name = "dm-groups", .xml = R"xml(<docmaterials><material>BULK|G1|1.0|0.0|Metals</material><material>BULK|G2|1.0|0.0|Custom</material><material>BULK|G3|1.0|0.0|Other</material><material>BULK|G4|1.0|0.0|NoSuchGroup</material><material>BULK|G5|1.0|NoSuchGroup</material><material>BULK|G6|1.0|0.0|metals</material><material>BULK|G7|1.0|0.0|</material><material>LINE|G8|0.001|0.0|Elastics</material><material>SURFACE|G9|0.01|0.0|Fabrics</material></docmaterials>)xml", .expected = R"out(RESULT ok
ROOT docmaterials {} []
EVENTS 0
| material [BULK|G1|1.0|0.0|Metals] document=true user=true
| material [BULK|G2|1.0|0.0|Custom] document=true user=true
| material [BULK|G3|1.0|0.0|Other] document=true user=true
| material [BULK|G4|1.0|0.0|Custom] document=true user=true
| material [BULK|G5|1.0|0.0|Custom] document=true user=true
| material [BULK|G6|1.0|0.0|Custom] document=true user=true
| material [BULK|G7|1.0|0.0|Custom] document=true user=true
| material [LINE|G8|0.001|0.0|Elastics] document=true user=true
| material [SURFACE|G9|0.01|0.0|Fabrics] document=true user=true)out"},
    {.name = "dm-threads-and-lines", .xml = R"xml(<docmaterials><material>LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|ThreadsLines</material><material>LINE|Nylon thread|0.0003|ThreadsLines</material><material>LINE|Kevlar thread 138 (0.4 mm, 1/64 in)|1.3E-4|ThreadsLines</material><material>LINE|No such line|0.5|ThreadsLines</material><material>BULK|Bulk|1.0|ThreadsLines</material><material>LINE|elastic cord (ROUND 2 mm, 1/16 in)|0.0018|0.0|ThreadsLines</material></docmaterials>)xml", .expected = R"out(RESULT ok
ROOT docmaterials {} []
EVENTS 0
| material [BULK|Bulk|1.0|0.0|Other] document=true user=true
| material [LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics] document=true user=true
| material [LINE|Kevlar thread 138 (0.4 mm, 1/64 in)|1.3E-4|0.0|Other] document=true user=true
| material [LINE|No such line|0.5|0.0|Other] document=true user=true
| material [LINE|Nylon thread|3.0E-4|0.0|Other] document=true user=true
| material [LINE|elastic cord (ROUND 2 mm, 1/16 in)|0.0018|0.0|Elastics] document=true user=true)out"},
    {.name = "dm-application-material", .xml = R"xml(<docmaterials><material>BULK|Cardboard|680.0|4.0E8|PaperProducts</material><material>BULK|Balsa|170.0|2.3E8|Woods</material></docmaterials>)xml", .expected = R"out(RESULT ok
ROOT docmaterials {} []
EVENTS 0
| material [BULK|Balsa|170.0|2.3E8|Woods] document=true user=true
| material [BULK|Cardboard|680.0|4.0E8|PaperProducts] document=true user=true)out"},
    {.name = "dm-twice", .xml = R"xml(<docmaterials><material>BULK|A|1.0</material><material>BULK|A|1.0</material><material>BULK|A|1.0|0.0|Custom</material><material>BULK|A|1.00000000001</material><material>BULK|A|1.0|5.0</material><material>BULK|A|2.0</material><material>SURFACE|A|1.0</material><material>BULK|A|1.0|0.0|Woods</material></docmaterials>)xml", .expected = R"out(RESULT ok
ROOT docmaterials {} []
EVENTS 0
| material [BULK|A|1.0|0.0|Custom] document=true user=true
| material [SURFACE|A|1.0|0.0|Custom] document=true user=true
| material [BULK|A|1.0|0.0|Woods] document=true user=true
| material [BULK|A|1.0|5.0|Custom] document=true user=true
| material [BULK|A|2.0|0.0|Custom] document=true user=true)out"},
    {.name = "dm-order", .xml = R"xml(<docmaterials><material>BULK|Zeta|1.0</material><material>LINE|Line|0.5</material><material>BULK|Alpha|9.0</material><material>SURFACE|Canopy|0.1</material><material>BULK|Alpha|3.0</material><material>BULK|alpha|1.0</material><material>BULK|Beta|1.0</material></docmaterials>)xml", .expected = R"out(RESULT ok
ROOT docmaterials {} []
EVENTS 0
| material [BULK|Alpha|3.0|0.0|Custom] document=true user=true
| material [BULK|Alpha|9.0|0.0|Custom] document=true user=true
| material [BULK|Beta|1.0|0.0|Custom] document=true user=true
| material [SURFACE|Canopy|0.1|0.0|Custom] document=true user=true
| material [LINE|Line|0.5|0.0|Custom] document=true user=true
| material [BULK|Zeta|1.0|0.0|Custom] document=true user=true
| material [BULK|alpha|1.0|0.0|Custom] document=true user=true)out"},
    {.name = "dm-numbers", .xml = R"xml(<docmaterials><material>BULK|Exponent|6.8E2|4e8</material><material>BULK|Zero|0.0</material><material>BULK|Negative|-5.0|-1.0</material><material>BULK|Blanks| 7.5 | 2.5 </material><material>BULK|Hexadecimal|0x1p3</material><material>BULK|Suffix|1.5d|2f</material></docmaterials>)xml", .expected = R"out(RESULT ok
ROOT docmaterials {} []
EVENTS 0
| material [BULK|Blanks|7.5|2.5|Custom] document=true user=true
| material [BULK|Exponent|680.0|4.0E8|Custom] document=true user=true
| material [BULK|Hexadecimal|8.0|0.0|Custom] document=true user=true
| material [BULK|Negative|-5.0|-1.0|Custom] document=true user=true
| material [BULK|Suffix|1.5|2.0|Custom] document=true user=true
| material [BULK|Zero|0.0|0.0|Custom] document=true user=true)out"},
    {.name = "dm-names", .xml = R"xml(<docmaterials><material>BULK| name with blanks |1.0</material><material>BULK||1.0</material><material>BULK|a &amp; b &lt;c&gt;|1.0</material><material>BULK|[material:Kraft phenolic]|958.7050344900001|0.0|Custom</material><material>LINE|Nylon Paracord, 110 lb, 1/16 in. dia.|0.0016|0.0|Custom</material></docmaterials>)xml", .expected = R"out(RESULT ok
ROOT docmaterials {} []
EVENTS 0
| material [BULK||1.0|0.0|Custom] document=true user=true
| material [BULK| name with blanks |1.0|0.0|Custom] document=true user=true
| material [LINE|Nylon Paracord, 110 lb, 1/16 in. dia.|0.0016|0.0|Custom] document=true user=true
| material [BULK|[material:Kraft phenolic]|958.7050344900001|0.0|Custom] document=true user=true
| material [BULK|a & b <c>|1.0|0.0|Custom] document=true user=true)out"},
    {.name = "dm-more-fields", .xml = R"xml(<docmaterials><material>BULK|Six|1.0|2.0|Woods|extra</material><material>BULK|Five and a pipe|1.0|2.0|Woods|</material><material>BULK|Group then number|1.0|Woods|2.0</material></docmaterials>)xml", .expected = R"out(RESULT ok
ROOT docmaterials {} []
EVENTS 0
| material [BULK|Five and a pipe|1.0|2.0|Custom] document=true user=true
| material [BULK|Group then number|1.0|0.0|Woods] document=true user=true
| material [BULK|Six|1.0|2.0|Custom] document=true user=true)out"},
    {.name = "dm-material-with-attributes", .xml = R"xml(<docmaterials><material a="1" b="2">BULK|A|1.0</material></docmaterials>)xml", .expected = R"out(RESULT ok
ROOT docmaterials {} []
EVENTS 0
| material [BULK|A|1.0|0.0|Custom] document=true user=true)out"},
    {.name = "dm-text-and-attributes", .xml = R"xml(<docmaterials a="1">txt<material>BULK|A|1.0</material>more</docmaterials>)xml", .expected = R"out(RESULT ok
ROOT docmaterials {a=1} [txtmore]
EVENTS 0
| material [BULK|A|1.0|0.0|Custom] document=true user=true)out"},
    {.name = "dm-other-children", .xml = R"xml(<docmaterials><other a="1">x<material>BULK|Hidden|1.0</material></other><material>BULK|A|1.0</material><Material>BULK|B|1.0</Material></docmaterials>)xml", .expected = R"out(RESULT ok
ROOT docmaterials {} []
EVENTS 0
| material [BULK|A|1.0|0.0|Custom] document=true user=true)out"},
    {.name = "dm-other-child-last", .xml = R"xml(<docmaterials a="1">txt<material>BULK|A|1.0</material><other b="2">x</other>tail</docmaterials>)xml", .expected = R"out(RESULT ok
ROOT docmaterials {b=2} [tail]
EVENTS 0
| material [BULK|A|1.0|0.0|Custom] document=true user=true)out"},
    {.name = "dm-no-material", .xml = R"xml(<docmaterials><material>BULK|Before|1.0</material><material>garbage</material><material>BULK|Never|1.0</material></docmaterials>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: Illegal material string: garbage
EVENTS 0
| material [BULK|Before|1.0|0.0|Custom] document=true user=true)out"},
    {.name = "dm-no-material-empty", .xml = R"xml(<docmaterials><material></material></docmaterials>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: Illegal material string: 
EVENTS 0)out"},
    {.name = "dm-no-material-two-fields", .xml = R"xml(<docmaterials><material>BULK|A</material></docmaterials>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: Illegal material string: BULK|A
EVENTS 0)out"},
    {.name = "dm-no-material-type-none", .xml = R"xml(<docmaterials><material>WOOD|A|1.0</material></docmaterials>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: Illegal material string: WOOD|A|1.0
EVENTS 0)out"},
    {.name = "dm-no-material-type-in-lower-case", .xml = R"xml(<docmaterials><material>bulk|A|1.0</material></docmaterials>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: Illegal material string: bulk|A|1.0
EVENTS 0)out"},
    {.name = "dm-no-material-type-custom", .xml = R"xml(<docmaterials><material>CUSTOM|A|1.0</material></docmaterials>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: Illegal material string: CUSTOM|A|1.0
EVENTS 0)out"},
    {.name = "dm-no-material-type-with-blanks", .xml = R"xml(<docmaterials><material> BULK|A|1.0 </material></docmaterials>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: Illegal material string:  BULK|A|1.0 
EVENTS 0)out"},
    {.name = "dm-no-material-density-none", .xml = R"xml(<docmaterials><material>BULK|A|abc</material></docmaterials>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: Illegal material string: BULK|A|abc
EVENTS 0)out"},
    {.name = "dm-no-material-density-empty", .xml = R"xml(<docmaterials><material>BULK|A|</material></docmaterials>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: Illegal material string: BULK|A|
EVENTS 0)out"},
    {.name = "dm-no-material-density-inf", .xml = R"xml(<docmaterials><material>BULK|A|Inf|0.0|Custom</material></docmaterials>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: Illegal material string: BULK|A|Inf|0.0|Custom
EVENTS 0)out"},
    {.name = "dm-element-in-a-material", .xml = R"xml(<docmaterials><material>BULK|A|1.0<x/></material></docmaterials>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: Illegal material string: 
W Unknown element x, ignoring.
EVENTS 0)out"},
}};

// Where QtRocket answers otherwise on purpose: the comment of a case says why and gives the
// lines of OpenRocket's answer that QtRocket does not give.
constexpr std::array<DocumentCase, 3> kOwn{{
    // A material whose density is a NaN or an infinity is not added, with a warning (the rule of MaterialSetter, part R2; OpenRocket stores it).
    // OpenRocket: | material [BULK|Infinite|Infinity|0.0|Custom] document=true user=true
    // OpenRocket: | material [BULK|NaN|NaN|0.0|Custom] document=true user=true
    // OpenRocket: | material [BULK|Negative infinite|-Infinity|0.0|Custom] document=true user=true
    // OpenRocket: | material [BULK|Too large|Infinity|0.0|Custom] document=true user=true
    {.name = "dm-density-not-finite", .xml = R"xml(<docmaterials><material>BULK|Before|1.0</material><material>BULK|NaN|NaN</material><material>BULK|Infinite|Infinity</material><material>BULK|Negative infinite|-Infinity|0.0|Custom</material><material>BULK|Too large|1e999</material><material>BULK|After|2.0</material></docmaterials>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT docmaterials {} []
EVENTS 0
| material [BULK|After|2.0|0.0|Custom] document=true user=true
| material [BULK|Before|1.0|0.0|Custom] document=true user=true)out"},
    // A shear modulus that is a NaN or an infinity is read as 0, with a warning, and the material is added (the rule of MaterialSetter, part R2; OpenRocket stores the value).
    // OpenRocket: | material [BULK|Infinite|1.0|Infinity|Woods] document=true user=true
    // OpenRocket: | material [BULK|NaN|1.0|NaN|Custom] document=true user=true
    // OpenRocket: | material [BULK|Too large|1.0|-Infinity|Custom] document=true user=true
    {.name = "dm-shear-modulus-not-finite", .xml = R"xml(<docmaterials><material>BULK|NaN|1.0|NaN</material><material>BULK|Infinite|1.0|Infinity|Woods</material><material>BULK|Too large|1.0|-1e999</material><material>BULK|After|2.0</material></docmaterials>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT docmaterials {} []
EVENTS 0
| material [BULK|After|2.0|0.0|Custom] document=true user=true
| material [BULK|Infinite|1.0|0.0|Woods] document=true user=true
| material [BULK|NaN|1.0|0.0|Custom] document=true user=true
| material [BULK|Too large|1.0|0.0|Custom] document=true user=true)out"},
    // A material whose density is a NaN is not added, with a warning. OpenRocket stores it once per line: a NaN equals nothing, so the material is never found again.
    // OpenRocket: | material [BULK|NaN|NaN|0.0|Custom] document=true user=true
    // OpenRocket: | material [BULK|NaN|NaN|0.0|Custom] document=true user=true
    // OpenRocket: | material [BULK|NaN|NaN|0.0|Custom] document=true user=true
    {.name = "dm-density-nan-twice", .xml = R"xml(<docmaterials><material>BULK|NaN|NaN</material><material>BULK|NaN|NaN</material><material>BULK|NaN|NaN</material></docmaterials>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT docmaterials {} []
EVENTS 0)out"},
}};
// END GENERATED TABLES DocumentMaterialHandler
// clang-format on

TEST(DocumentMaterialHandler, ReadsItsCasesAsOpenRocket)
{
    EXPECT_EQ(failedDocumentCases(kJava), Texts{});
}

TEST(DocumentMaterialHandler, ReadsItsOwnCasesAsDecided)
{
    EXPECT_EQ(failedDocumentCases(kOwn), Texts{});
}

// Not a test: prints what QtRocket makes of the cases, for scripts/make_tables.py. Run
// with --gtest_also_run_disabled_tests.
TEST(DocumentMaterialHandler, DISABLED_PrintsCases)
{
    std::cout << printedDocumentCases(kJava) << printedDocumentCases(kOwn);
}

TEST(DocumentMaterialHandler, NoCaseThrowsWhenItsTextIsCutOff)
{
    EXPECT_EQ(documentCasesThatThrowWhenCutOff(kJava), Texts{});
    EXPECT_EQ(documentCasesThatThrowWhenCutOff(kOwn), Texts{});
}

// A material of the element is a user-defined material that belongs to the document, in the
// document's database of its type.
TEST(DocumentMaterialHandler, AddsAUserDefinedDocumentMaterial)
{
    DocumentFixture  fixture;
    const HandlerRun run = fixture.load(
        "<docmaterials><material>SURFACE|My cloth|0.05|0.0|Fabrics</material></docmaterials>");
    ASSERT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});

    const MaterialDatabase& surface = fixture.document().getDocumentMaterials().surfaceMaterials();
    ASSERT_EQ(surface.size(), 1U);
    const auto material = surface.begin();
    EXPECT_EQ(material->getType(), Material::Type::SURFACE);
    EXPECT_EQ(material->getName(), "My cloth");
    EXPECT_EQ(material->getDensity(), 0.05);
    EXPECT_EQ(material->getGroup(), MaterialGroup::FABRICS);
    EXPECT_TRUE(material->isUserDefined());
    EXPECT_TRUE(material->isDocumentMaterial());
    EXPECT_EQ(fixture.document().getDocumentMaterials().totalMaterialCount(), 1U);
    // The application's materials are only asked, never added to.
    EXPECT_FALSE(fixture.fixture().materials().surfaceMaterials().contains(*material));
}

/// The group the document's line material gets for the text of an old file's material whose
/// group is "ThreadsLines", as its database string.
[[nodiscard]] std::string groupOfThreadsAndLines(DocumentFixture& fixture)
{
    const HandlerRun run = fixture.load(
        "<docmaterials><material>LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|ThreadsLines"
        "</material></docmaterials>");
    const MaterialDatabase& lines = fixture.document().getDocumentMaterials().lineMaterials();
    if (!run.result.has_value() || lines.size() != 1)
    {
        return "not loaded";
    }
    return std::string(QtRocket::databaseString(lines.begin()->getGroup()));
}

// Java asks its global material database for the group that a material of the old group
// "ThreadsLines" has now; here the application's materials are the context's, and a context
// without them gives the group of a material that is not found.
TEST(DocumentMaterialHandler, AsksTheApplicationsMaterialsForTheGroupOfAnOldFile)
{
    DocumentFixture with;
    EXPECT_EQ(groupOfThreadsAndLines(with), "Elastics");

    DocumentFixture without;
    without.context().setApplicationMaterials(nullptr);
    EXPECT_EQ(groupOfThreadsAndLines(without), "Other");
}

// A <material> is plain text; any other child is ignored with everything in it, and nothing
// says so.
TEST(DocumentMaterialHandler, IgnoresAnyOtherChildWithoutAWarning)
{
    DocumentFixture         fixture;
    DocumentMaterialHandler handler(fixture.context());
    WarningSet              warnings;
    EXPECT_EQ(handler.openElement("material", {}, warnings).value_or(nullptr),
              &PlainTextHandler::instance());
    EXPECT_EQ(handler.openElement("other", {}, warnings).value_or(&handler), nullptr);
    EXPECT_EQ(handler.openElement("Material", {}, warnings).value_or(&handler), nullptr);
    // Nothing but a <material> is read when it closes.
    ASSERT_TRUE(handler.closeElement("other", {{"a", "1"}}, "BULK|A|1.0", warnings).has_value());
    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(fixture.document().getDocumentMaterials().totalMaterialCount(), 0U);
}

// What ends OpenRocket's load with an IllegalArgumentException ends this one with that
// exception's message under ErrorCode::INVALID_ARGUMENT (Material::fromStorableString() itself
// reports a parse error).
TEST(DocumentMaterialHandler, FailsTheLoadForATextThatIsNoMaterial)
{
    DocumentFixture         fixture;
    DocumentMaterialHandler handler(fixture.context());
    WarningSet              warnings;
    const Result<void>      closed = handler.closeElement("material", {}, "garbage", warnings);
    ASSERT_FALSE(closed.has_value());
    EXPECT_EQ(closed.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(closed.error().message, "Illegal material string: garbage");
    EXPECT_TRUE(warnings.empty());
}

// The materials a file lists come on top of the ones its components were given, which the
// document registered when the components got them (the scout's case docprefs-material-used:
// OpenRocket saves MyBulk and Unlisted).
TEST(DocumentMaterialHandler, TheMaterialsOfTheListJoinTheOnesOfTheComponents)
{
    RocketLoadFixture rocket;
    const HandlerRun  components = rocket.load(
        R"xml(<name>A</name><subcomponents><stage><subcomponents><bodytube><material type="bulk" density="123.0" shearModulus="4.0" group="Other">MyBulk</material></bodytube><bodytube><material type="bulk" density="55.0">Unlisted</material></bodytube></subcomponents></stage></subcomponents>)xml");
    ASSERT_TRUE(components.result.has_value());
    EXPECT_EQ(components.texts(), Texts{});

    DocumentPreferencesHandler handler(rocket.context());
    const HandlerRun           listed =
        runHandler(handler,
                   "<docprefs><docmaterials><material>BULK|MyBulk|123.0|4.0|Other</material>"
                   "<material>BULK|Listed only|7.0</material></docmaterials></docprefs>");
    ASSERT_TRUE(listed.result.has_value());
    EXPECT_EQ(listed.texts(), Texts{});

    Texts materials;
    for (const Material& material : rocket.document().getDocumentMaterials().allMaterials())
    {
        materials.push_back(material.toStorableString());
    }
    EXPECT_EQ(materials, (Texts{"BULK|Listed only|7.0|0.0|Custom", "BULK|MyBulk|123.0|4.0|Other",
                                "BULK|Unlisted|55.0|0.0|Custom"}));
}

TEST(DocumentMaterialHandler, AContextWithoutADocumentIsABug)
{
    const DocumentLoadingContext context;
    EXPECT_THROW(DocumentMaterialHandler{context}, BugError);
}

}  // namespace
