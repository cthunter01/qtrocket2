#include "QtRocket/file/openrocket/DocumentPreferencesHandler.h"

#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/ConfigHandler.h"
#include "QtRocket/file/openrocket/DocumentMaterialHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/preferences/DocumentPreferences.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Error.h"
#include "file/openrocket/DocumentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// DocumentPreferencesHandler: the handler of the <docprefs> element of a design file. What a
// file's text gives is compared with what OpenRocket makes of the same text.

namespace
{

using QtRocket::BugError;
using QtRocket::Color;
using QtRocket::Config;
using QtRocket::ConfigHandler;
using QtRocket::DocumentLoadingContext;
using QtRocket::DocumentMaterialHandler;
using QtRocket::DocumentPreferences;
using QtRocket::DocumentPreferencesHandler;
using QtRocket::ElementHandler;
using QtRocket::PlainTextHandler;
using QtRocket::Result;
using QtRocket::WarningSet;
using QtRocket::Test::DocumentCase;
using QtRocket::Test::documentCasesThatThrowWhenCutOff;
using QtRocket::Test::DocumentFixture;
using QtRocket::Test::failedDocumentCases;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::printedDocumentCases;

using Texts = std::vector<std::string>;

// The tables are written by scripts/make_tables.py of the scratchpad's
// probes/tier9b-document-handlers from the cases (scripts/r4_cases.py) and OpenRocket's answers
// to them (DocumentProbe.java): do not edit them by hand.
// clang-format off
// BEGIN GENERATED TABLES DocumentPreferencesHandler
// What OpenRocket makes of each case (DocumentProbe.java of part R4), which QtRocket has to
// make of it too.
constexpr std::array<DocumentCase, 25> kJava{{
    {.name = "dp-empty", .xml = R"xml(<docprefs></docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {} []
EVENTS 0)out"},
    {.name = "dp-booleans", .xml = R"xml(<docprefs><pref key="b1" type="boolean">true</pref><pref key="b2" type="boolean">TRUE</pref><pref key="b3" type="boolean"> true </pref><pref key="b4" type="boolean">yes</pref><pref key="b5" type="boolean"></pref><pref key="b6" type="boolean">false</pref><pref key="b7" type="boolean">tRuE</pref></docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {} []
EVENTS 7
| pref 'b1' Boolean true
| pref 'b2' Boolean true
| pref 'b3' Boolean false
| pref 'b4' Boolean false
| pref 'b5' Boolean false
| pref 'b6' Boolean false
| pref 'b7' Boolean true)out"},
    {.name = "dp-strings", .xml = R"xml(<docprefs><pref key="s1" type="string"> text </pref><pref key="s2" type="string"></pref><pref key="s3" type="string">a &amp; b &lt;c&gt;</pref><pref key="s4" type="string">line
break</pref><pref key="s5" type="string">255,128,64</pref></docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {} []
EVENTS 5
| pref 's1' String ' text '
| pref 's2' String ''
| pref 's3' String 'a & b <c>'
| pref 's4' String 'line\nbreak'
| pref 's5' String '255,128,64')out"},
    {.name = "dp-number-that-is-none", .xml = R"xml(<docprefs><pref key="n1" type="number">abc</pref><pref key="n2" type="number"></pref><pref key="n3" type="number">NaN</pref><pref key="n4" type="number">1e999</pref><pref key="s1" type="string">after</pref></docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {} []
EVENTS 1
| pref 's1' String 'after')out"},
    {.name = "dp-list-without-a-key", .xml = R"xml(<docprefs><pref type="list"><entry type="string">a</entry></pref><pref key="s1" type="string">after</pref></docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {} []
EVENTS 1
| pref 's1' String 'after')out"},
    {.name = "dp-list-of-prefs", .xml = R"xml(<docprefs><pref type="list"><pref type="string">a</pref><pref key="k" type="boolean">true</pref><entry key="e" type="string">b</entry><bogus a="1">t</bogus></pref></docprefs>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'pref', ignoring.
W Unknown attributes in element 'pref', ignoring.
W Unknown text in element 'bogus', ignoring.
W Unknown attributes in element 'bogus', ignoring.
ROOT docprefs {} []
EVENTS 0)out"},
    {.name = "dp-list-in-a-list", .xml = R"xml(<docprefs><pref type="list"><entry type="list"><entry type="list"><entry type="string">deep</entry></entry></entry></pref></docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {} []
EVENTS 0)out"},
    {.name = "dp-types-that-are-none", .xml = R"xml(<docprefs><pref key="u1" type="bogus">x</pref><pref key="u2">x</pref><pref key="u3" type="Boolean">true</pref><pref key="u4" type="STRING">x</pref><pref key="u5" type=" string">x</pref><pref key="u6" type="">x</pref><pref key="u7" type="int">5</pref><pref key="u8" type="float">5</pref></docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {} []
EVENTS 0)out"},
    {.name = "dp-without-a-key", .xml = R"xml(<docprefs><pref type="string">nokey</pref><pref type="boolean">true</pref><pref>x</pref></docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {} []
EVENTS 0)out"},
    {.name = "dp-key-that-is-empty", .xml = R"xml(<docprefs><pref key="" type="string">empty key</pref><pref key=" " type="boolean">true</pref></docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {} []
EVENTS 2
| pref '' String 'empty key'
| pref ' ' Boolean true)out"},
    {.name = "dp-key-twice-with-the-same-value", .xml = R"xml(<docprefs><pref key="k" type="string">one</pref><pref key="k" type="string">one</pref><pref key="b" type="boolean">true</pref><pref key="b" type="boolean">TRUE</pref></docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {} []
EVENTS 2
| pref 'b' Boolean true
| pref 'k' String 'one')out"},
    {.name = "dp-pref-with-more-attributes", .xml = R"xml(<docprefs><pref key="k" type="string" more="1">v</pref></docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {} []
EVENTS 1
| pref 'k' String 'v')out"},
    {.name = "dp-other-children", .xml = R"xml(<docprefs><bogus a="1">t</bogus><entry key="e" type="string">v</entry><Pref key="p" type="string">v</Pref><empty/></docprefs>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'bogus', ignoring.
W Unknown attributes in element 'bogus', ignoring.
W Unknown text in element 'entry', ignoring.
W Unknown attributes in element 'entry', ignoring.
W Unknown text in element 'Pref', ignoring.
W Unknown attributes in element 'Pref', ignoring.
ROOT docprefs {} []
EVENTS 0)out"},
    {.name = "dp-text-and-attributes", .xml = R"xml(<docprefs a="1">txt<pref key="k" type="string">v</pref>more</docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {a=1} [txtmore]
EVENTS 1
| pref 'k' String 'v')out"},
    {.name = "dp-element-in-a-pref", .xml = R"xml(<docprefs a="1"><pref key="k" type="string">x<y/>z</pref><pref key="m" type="string">after</pref></docprefs>)xml", .expected = R"out(RESULT ok
W Unknown element y, ignoring.
ROOT docprefs {key=k, type=string} [x]
EVENTS 1
| pref 'm' String 'after')out"},
    {.name = "dp-element-with-a-type-in-a-pref", .xml = R"xml(<docprefs><pref key="k" type="string">x<y type="boolean" key="q"/>true</pref></docprefs>)xml", .expected = R"out(RESULT ok
W Unknown element y, ignoring.
ROOT docprefs {key=k, type=string} [x]
EVENTS 1
| pref 'q' Boolean true)out"},
    {.name = "dp-element-that-claims-a-list", .xml = R"xml(<docprefs><pref key="k" type="string">x<y type="list"/></pref></docprefs>)xml", .expected = R"out(RESULT ok
W Unknown element y, ignoring.
ROOT docprefs {key=k, type=string} [x]
EVENTS 0)out"},
    {.name = "dp-list-and-later-a-claimed-list", .xml = R"xml(<docprefs><pref type="list"><entry type="string">a</entry></pref><pref key="k" type="string">x<y type="list"/></pref></docprefs>)xml", .expected = R"out(RESULT ok
W Unknown element y, ignoring.
ROOT docprefs {key=k, type=string} [x]
EVENTS 0)out"},
    {.name = "dp-rocket-panel", .xml = R"xml(<docprefs><pref key="RocketPanel.showWarnings" type="boolean">false</pref><pref key="RocketPanel.2DBackgroundColor" type="string">1,2,3</pref><pref key="RocketPanel.3DBackgroundColor" type="string">204,204,204</pref><pref key="RocketPanel.2DTextColor" type="string">0,0,0</pref><pref key="RocketPanel.3DTextColor" type="string">9,9,9</pref><pref key="RocketPanel.3DShadowsEnabled" type="boolean">true</pref><pref key="RocketPanel.3DOriginAxesVisible" type="boolean">false</pref></docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {} []
EVENTS 7
| pref 'RocketPanel.2DBackgroundColor' String '1,2,3'
| pref 'RocketPanel.2DTextColor' String '0,0,0'
| pref 'RocketPanel.3DBackgroundColor' String '204,204,204'
| pref 'RocketPanel.3DOriginAxesVisible' Boolean false
| pref 'RocketPanel.3DShadowsEnabled' Boolean true
| pref 'RocketPanel.3DTextColor' String '9,9,9'
| pref 'RocketPanel.showWarnings' Boolean false)out"},
    {.name = "dp-materials", .xml = R"xml(<docprefs><pref key="k" type="string">v</pref><docmaterials><material>BULK|MyBulk|123.0|4.0|Other</material><material>LINE|MyLine|0.01</material></docmaterials><pref key="m" type="boolean">true</pref></docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {} []
EVENTS 2
| pref 'k' String 'v'
| pref 'm' Boolean true
| material [BULK|MyBulk|123.0|4.0|Other] document=true user=true
| material [LINE|MyLine|0.01|0.0|Custom] document=true user=true)out"},
    {.name = "dp-materials-with-text-and-attributes", .xml = R"xml(<docprefs><docmaterials a="1">txt<material a="1">BULK|MyBulk|123.0|4.0|Other</material></docmaterials></docprefs>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'docmaterials', ignoring.
W Unknown attributes in element 'docmaterials', ignoring.
ROOT docprefs {} []
EVENTS 0
| material [BULK|MyBulk|123.0|4.0|Other] document=true user=true)out"},
    {.name = "dp-materials-with-an-unknown-child", .xml = R"xml(<docprefs p="1"><docmaterials a="1">txt<material>BULK|MyBulk|123.0|4.0|Other</material><other b="2">x</other><material>LINE|MyLine|0.01</material></docmaterials><pref key="k" type="string">v</pref></docprefs>)xml", .expected = R"out(RESULT ok
W Unknown attributes in element 'docmaterials', ignoring.
ROOT docprefs {a=1} [txt]
EVENTS 1
| pref 'k' String 'v'
| material [BULK|MyBulk|123.0|4.0|Other] document=true user=true
| material [LINE|MyLine|0.01|0.0|Custom] document=true user=true)out"},
    {.name = "dp-materials-twice", .xml = R"xml(<docprefs><docmaterials><material>BULK|A|1.0</material></docmaterials><docmaterials><material>BULK|B|2.0</material><material>BULK|A|1.0</material></docmaterials></docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {} []
EVENTS 0
| material [BULK|A|1.0|0.0|Custom] document=true user=true
| material [BULK|B|2.0|0.0|Custom] document=true user=true)out"},
    {.name = "dp-material-that-is-none", .xml = R"xml(<docprefs><pref key="k" type="string">v</pref><docmaterials><material>garbage</material></docmaterials><pref key="never" type="string">set</pref></docprefs>)xml", .expected = R"out(RESULT FAILED INVALID_ARGUMENT: Illegal material string: garbage
EVENTS 1
| pref 'k' String 'v')out"},
    {.name = "dp-scout", .xml = R"xml(<docprefs a="1">
<pref key="b1" type="boolean">true</pref><pref key="b2" type="boolean"> TRUE </pref><pref key="b3" type="boolean">yes</pref>
<pref key="s1" type="string"> text </pref><pref key="s2" type="string"></pref>
<pref key="u1" type="bogus">x</pref><pref key="nt">x</pref><pref type="string">nokey</pref>
<bogus a="1">t</bogus>
<docmaterials a="1">txt<material a="1">BULK|MyBulk|123.0|4.0|Other</material><material>BULK|Cardboard|680.0|4.0E8|PaperProducts</material><material>LINE|MyLine|0.01</material><other>x</other></docmaterials>
</docprefs>)xml", .expected = R"out(RESULT ok
W Unknown text in element 'bogus', ignoring.
W Unknown attributes in element 'bogus', ignoring.
ROOT docprefs {a=1} [txt]
EVENTS 5
| pref 'b1' Boolean true
| pref 'b2' Boolean false
| pref 'b3' Boolean false
| pref 's1' String ' text '
| pref 's2' String ''
| material [BULK|Cardboard|680.0|4.0E8|PaperProducts] document=true user=true
| material [BULK|MyBulk|123.0|4.0|Other] document=true user=true
| material [LINE|MyLine|0.01|0.0|Custom] document=true user=true)out"},
}};

// Where QtRocket answers otherwise on purpose: the comment of a case says why and gives the
// lines of OpenRocket's answer that QtRocket does not give.
constexpr std::array<DocumentCase, 13> kOwn{{
    // Decision L5: OpenRocket's loader does not know the type integer, which its saver writes, and drops the preference; here it is loaded.
    // OpenRocket: EVENTS 0
    {.name = "dp-integers", .xml = R"xml(<docprefs><pref key="i1" type="integer">5</pref><pref key="i2" type="integer">-3</pref><pref key="i3" type="integer">+7</pref><pref key="i4" type="integer"> 12 </pref><pref key="i5" type="integer">2147483647</pref><pref key="i6" type="integer">-2147483648</pref><pref key="i7" type="integer">007</pref><pref key="i8" type="integer">0</pref></docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {} []
EVENTS 8
| pref 'i1' Integer 5
| pref 'i2' Integer -3
| pref 'i3' Integer 7
| pref 'i4' Integer 12
| pref 'i5' Integer 2147483647
| pref 'i6' Integer -2147483648
| pref 'i7' Integer 7
| pref 'i8' Integer 0)out"},
    // Decision L5: OpenRocket's loader does not know the type double, which its saver writes, and drops the preference; here it is loaded.
    // OpenRocket: EVENTS 0
    {.name = "dp-doubles", .xml = R"xml(<docprefs><pref key="d1" type="double">1.5</pref><pref key="d2" type="double">1.0E10</pref><pref key="d3" type="double"> 2.5 </pref><pref key="d4" type="double">-0.0</pref><pref key="d5" type="double">5</pref><pref key="d6" type="double">1e-400</pref><pref key="d7" type="double">0x1p3</pref></docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {} []
EVENTS 7
| pref 'd1' Double 1.5
| pref 'd2' Double 1.0E10
| pref 'd3' Double 2.5
| pref 'd4' Double -0.0
| pref 'd5' Double 5.0
| pref 'd6' Double 0.0
| pref 'd7' Double 8.0)out"},
    // Decision L5: a double preference is loaded, also a NaN and an infinity, which OpenRocket's saver writes so (a preference is no input of a simulation).
    // OpenRocket: EVENTS 0
    {.name = "dp-doubles-that-are-not-finite", .xml = R"xml(<docprefs><pref key="n1" type="double">NaN</pref><pref key="n2" type="double">Infinity</pref><pref key="n3" type="double">-Infinity</pref><pref key="n4" type="double">1e999</pref></docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {} []
EVENTS 4
| pref 'n1' Double NaN
| pref 'n2' Double Infinity
| pref 'n3' Double -Infinity
| pref 'n4' Double Infinity)out"},
    // Decision L5: an integer preference is loaded; one that cannot be read gives a warning (OpenRocket drops all of them without a word).
    // OpenRocket: EVENTS 0
    {.name = "dp-integers-that-are-none", .xml = R"xml(<docprefs><pref key="i1" type="integer">abc</pref><pref key="i2" type="integer"></pref><pref key="i3" type="integer">2147483648</pref><pref key="i4" type="integer">1.5</pref><pref key="i5" type="integer">1 2</pref><pref key="i6" type="integer">5</pref></docprefs>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT docprefs {} []
EVENTS 1
| pref 'i6' Integer 5)out"},
    // Decision L5: a double preference is loaded; one that cannot be read gives a warning (OpenRocket drops all of them without a word).
    // OpenRocket: EVENTS 0
    {.name = "dp-doubles-that-are-none", .xml = R"xml(<docprefs><pref key="d1" type="double">abc</pref><pref key="d2" type="double"></pref><pref key="d3" type="double">Inf</pref><pref key="d4" type="double">1,5</pref><pref key="d5" type="double">2.5</pref></docprefs>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT docprefs {} []
EVENTS 1
| pref 'd5' Double 2.5)out"},
    // Decision L4: a preference of type number with a key, for which OpenRocket throws a RuntimeException; here the text of that exception is a warning, and the preference behind it is loaded.
    // OpenRocket: RESULT THROWN java.lang.RuntimeException: Number preferences are not supported
    // OpenRocket: EVENTS 0
    {.name = "dp-number", .xml = R"xml(<docprefs><pref key="n1" type="number">5</pref><pref key="s1" type="string">after</pref></docprefs>)xml", .expected = R"out(RESULT ok
W Number preferences are not supported
ROOT docprefs {} []
EVENTS 1
| pref 's1' String 'after')out"},
    // Decision L4: preferences of type number with a key, for which OpenRocket throws a RuntimeException; here the text of that exception is a warning.
    // OpenRocket: RESULT THROWN java.lang.RuntimeException: Number preferences are not supported
    {.name = "dp-number-forms", .xml = R"xml(<docprefs><pref key="n1" type="number">2.5</pref><pref key="n2" type="number"> 7 </pref><pref key="n3" type="number">99999999999999999999</pref><pref key="n4" type="number">2.50</pref></docprefs>)xml", .expected = R"out(RESULT ok
W Number preferences are not supported
ROOT docprefs {} []
EVENTS 0)out"},
    // Decision L5: an integer that cannot be read gives a warning, also without a key (OpenRocket drops the element without a word).
    {.name = "dp-number-without-a-key", .xml = R"xml(<docprefs><pref type="number">5</pref><pref type="integer">6</pref><pref type="double">7.5</pref><pref type="integer">x</pref></docprefs>)xml", .expected = R"out(RESULT ok
W Invalid parameter encountered, ignoring.
ROOT docprefs {} []
EVENTS 0)out"},
    // Decision L4: a preference of type list with a key, for which OpenRocket throws a RuntimeException; here the text of that exception is a warning, and the preference behind it is loaded.
    // OpenRocket: RESULT THROWN java.lang.RuntimeException: Nested preferences are not supported
    // OpenRocket: EVENTS 0
    {.name = "dp-list", .xml = R"xml(<docprefs><pref key="l1" type="list"><entry type="string">a</entry><entry type="number">2</entry></pref><pref key="s1" type="string">after</pref></docprefs>)xml", .expected = R"out(RESULT ok
W Nested preferences are not supported
ROOT docprefs {} []
EVENTS 1
| pref 's1' String 'after')out"},
    // Decision L4: preferences of type list with a key, for which OpenRocket throws a RuntimeException; here the text of that exception is a warning.
    // OpenRocket: RESULT THROWN java.lang.RuntimeException: Nested preferences are not supported
    {.name = "dp-list-that-is-empty", .xml = R"xml(<docprefs><pref key="l1" type="list"/><pref key="l2" type="list"></pref></docprefs>)xml", .expected = R"out(RESULT ok
W Nested preferences are not supported
ROOT docprefs {} []
EVENTS 0)out"},
    // Decision L5: the integer preference is loaded and replaces the text under its key (OpenRocket drops it, so the text stays).
    // OpenRocket: EVENTS 5
    // OpenRocket: | pref 'n' String '5'
    {.name = "dp-key-twice", .xml = R"xml(<docprefs><pref key="k" type="string">one</pref><pref key="k" type="string">two</pref><pref key="m" type="boolean">true</pref><pref key="m" type="string">now a text</pref><pref key="n" type="string">5</pref><pref key="n" type="integer">5</pref></docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {} []
EVENTS 6
| pref 'k' String 'two'
| pref 'm' String 'now a text'
| pref 'n' Integer 5)out"},
    // The list of an entry is handed to that entry alone (EntryHandler, run 9a): the later pref, which closes with the attributes of the ignored element in it, finds no list and has no value. OpenRocket finds the list handler of the earlier pref and throws its RuntimeException.
    // OpenRocket: RESULT THROWN java.lang.RuntimeException: Nested preferences are not supported
    {.name = "dp-list-and-later-a-claimed-list-with-a-key", .xml = R"xml(<docprefs><pref type="list"><entry type="string">a</entry></pref><pref key="k" type="string">x<y type="list" key="q"/></pref></docprefs>)xml", .expected = R"out(RESULT ok
W Unknown element y, ignoring.
ROOT docprefs {key=k, type=string} [x]
EVENTS 0)out"},
    // Decision L5: the integer and the double preference are loaded (OpenRocket drops both).
    // OpenRocket: EVENTS 1
    {.name = "dp-rocket-panel-numbers", .xml = R"xml(<docprefs><pref key="RocketPanel.3DRenderQuality" type="integer">2</pref><pref key="RocketPanel.showWarnings" type="boolean">true</pref><pref key="Custom.scale" type="double">0.25</pref></docprefs>)xml", .expected = R"out(RESULT ok
ROOT docprefs {} []
EVENTS 3
| pref 'Custom.scale' Double 0.25
| pref 'RocketPanel.3DRenderQuality' Integer 2
| pref 'RocketPanel.showWarnings' Boolean true)out"},
}};
// END GENERATED TABLES DocumentPreferencesHandler
// clang-format on

TEST(DocumentPreferencesHandler, ReadsItsCasesAsOpenRocket)
{
    EXPECT_EQ(failedDocumentCases(kJava), Texts{});
}

TEST(DocumentPreferencesHandler, ReadsItsOwnCasesAsDecided)
{
    EXPECT_EQ(failedDocumentCases(kOwn), Texts{});
}

// Not a test: prints what QtRocket makes of the cases, for scripts/make_tables.py. Run
// with --gtest_also_run_disabled_tests.
TEST(DocumentPreferencesHandler, DISABLED_PrintsCases)
{
    std::cout << printedDocumentCases(kJava) << printedDocumentCases(kOwn);
}

TEST(DocumentPreferencesHandler, NoCaseThrowsWhenItsTextIsCutOff)
{
    EXPECT_EQ(documentCasesThatThrowWhenCutOff(kJava), Texts{});
    EXPECT_EQ(documentCasesThatThrowWhenCutOff(kOwn), Texts{});
}

// Each type goes into the document's preferences through the putter of its type, so the typed
// getters find it; the document is told of every change and is no longer the one that was saved.
TEST(DocumentPreferencesHandler, StoresEachTypeForItsTypedGetter)
{
    DocumentFixture fixture;
    fixture.document().setSaved(true);
    const HandlerRun run = fixture.load(
        R"xml(<docprefs><pref key="RocketPanel.showWarnings" type="boolean">false</pref><pref key="RocketPanel.3DTextColor" type="string">1,2,3</pref><pref key="quality" type="integer">3</pref><pref key="scale" type="double">0.25</pref></docprefs>)xml");
    ASSERT_TRUE(run.result.has_value());
    EXPECT_EQ(run.texts(), Texts{});

    const DocumentPreferences& prefs = fixture.document().getDocumentPreferences();
    EXPECT_FALSE(prefs.getBoolean(DocumentPreferences::kPrefShowWarnings, true));
    EXPECT_EQ(prefs.getString(DocumentPreferences::kPref3DTextColor, ""), "1,2,3");
    EXPECT_EQ(prefs.getColor(DocumentPreferences::kPref3DTextColor), Color(1, 2, 3));
    // Decision L5: OpenRocket's loader drops these two, which its saver writes.
    EXPECT_EQ(prefs.getInt("quality", 0), 3);
    EXPECT_EQ(prefs.getDouble("scale", 0.0), 0.25);
    EXPECT_EQ(prefs.size(), 4U);
    EXPECT_FALSE(fixture.document().isSaved());
}

// A file may hold several <docprefs>: each has a handler of its own and adds to the document
// (the scout's case docprefs-twice: OpenRocket saves s1 = two and s3 = three).
TEST(DocumentPreferencesHandler, ASecondElementAddsToTheFirst)
{
    DocumentFixture  fixture;
    const HandlerRun first = fixture.load(
        R"xml(<docprefs><pref key="s1" type="string">one</pref><pref key="s2" type="string">kept</pref></docprefs>)xml");
    const HandlerRun second = fixture.load(
        R"xml(<docprefs><pref key="s1" type="string">two</pref><pref key="s3" type="string">three</pref></docprefs>)xml");
    ASSERT_TRUE(first.result.has_value());
    ASSERT_TRUE(second.result.has_value());
    EXPECT_EQ(fixture.describe(), (Texts{"pref 's1' String 'two'", "pref 's2' String 'kept'",
                                         "pref 's3' String 'three'"}));
}

// A <pref> of type list is read by a ConfigHandler, <docmaterials> by its own handler, and
// everything else is plain text. A <pref> without a key goes into the handler's own list.
TEST(DocumentPreferencesHandler, HandsItsChildrenToTheirHandlers)
{
    DocumentFixture            fixture;
    DocumentPreferencesHandler handler(fixture.context());
    WarningSet                 warnings;

    ElementHandler* const list =
        handler.openElement("pref", {{"type", "list"}}, warnings).value_or(nullptr);
    EXPECT_NE(dynamic_cast<ConfigHandler*>(list), nullptr);
    // What closes the list entry takes its (empty) list, which has no key and so joins the
    // handler's own list.
    ASSERT_TRUE(handler.closeElement("pref", {{"type", "list"}}, "", warnings).has_value());
    ASSERT_EQ(handler.getList().size(), 1U);
    EXPECT_EQ(handler.getList().front(), Config::Value(Config::List{}));

    ElementHandler* const materials =
        handler.openElement("docmaterials", {}, warnings).value_or(nullptr);
    EXPECT_NE(dynamic_cast<DocumentMaterialHandler*>(materials), nullptr);

    EXPECT_EQ(handler.openElement("pref", {{"type", "string"}}, warnings).value_or(nullptr),
              &PlainTextHandler::instance());
    EXPECT_EQ(handler.openElement("pref", {}, warnings).value_or(nullptr),
              &PlainTextHandler::instance());
    EXPECT_EQ(handler.openElement("list", {{"type", "list"}}, warnings).value_or(nullptr),
              &PlainTextHandler::instance());
    EXPECT_EQ(handler.openElement("bogus", {}, warnings).value_or(nullptr),
              &PlainTextHandler::instance());

    ASSERT_TRUE(handler.closeElement("pref", {{"type", "string"}}, "text", warnings).has_value());
    ASSERT_TRUE(handler.closeElement("pref", {{"type", "integer"}}, "7", warnings).has_value());
    ASSERT_EQ(handler.getList().size(), 3U);
    EXPECT_EQ(handler.getList().at(1), Config::Value("text"));
    EXPECT_EQ(handler.getList().at(2), Config::Value(7));
    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(fixture.document().getDocumentPreferences().size(), 0U);
}

/// A <docprefs> whose one <pref> is a list that holds @p depth lists, one in the other.
[[nodiscard]] std::string nestedLists(int depth)
{
    std::string xml = "<docprefs><pref type=\"list\">";
    for (int i = 0; i < depth; ++i)
    {
        xml += "<entry type=\"list\">";
    }
    xml += "<entry type=\"string\">deep</entry>";
    for (int i = 0; i < depth; ++i)
    {
        xml += "</entry>";
    }
    return xml + "</pref></docprefs>";
}

// The lists of a <pref> are read by ConfigHandlers, whose nesting is bounded: a file cannot
// make the handlers nest without limit. The list of the <pref> itself is the first of the
// ConfigHandler::kMaxListDepth lists that may stand in one another.
TEST(DocumentPreferencesHandler, TheNestingOfAListIsBounded)
{
    DocumentFixture  within;
    const HandlerRun shallow = within.load(nestedLists(ConfigHandler::kMaxListDepth - 1));
    ASSERT_TRUE(shallow.result.has_value());
    EXPECT_EQ(shallow.texts(), Texts{});

    DocumentFixture  atTheBound;
    const HandlerRun oneMore = atTheBound.load(nestedLists(ConfigHandler::kMaxListDepth));
    ASSERT_TRUE(oneMore.result.has_value());
    // The list that is one too many is read as an entry without a list, whose child is unknown.
    EXPECT_EQ(oneMore.texts(), (Texts{"List entries nested too deeply, ignoring.",
                                      "Unknown element entry, ignoring."}));

    DocumentFixture  beyond;
    const HandlerRun deep = beyond.load(nestedLists(1000));
    ASSERT_TRUE(deep.result.has_value());
    EXPECT_EQ(deep.texts().front(), "List entries nested too deeply, ignoring.");
}

TEST(DocumentPreferencesHandler, AContextWithoutADocumentIsABug)
{
    const DocumentLoadingContext context;
    EXPECT_THROW(DocumentPreferencesHandler{context}, BugError);
}

}  // namespace
