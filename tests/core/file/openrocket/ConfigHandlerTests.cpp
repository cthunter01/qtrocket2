#include "QtRocket/file/openrocket/ConfigHandler.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Config.h"
#include "file/openrocket/EntryTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// The expectations are what OpenRocket's ConfigHandler makes of the same <config> elements
// (probe EntryProbe of part D4, section D), except for the entries of type "list", which
// OpenRocket never loads (see the class comment) and QtRocket does.

namespace
{

using QtRocket::Config;
using QtRocket::ConfigHandler;
using QtRocket::Test::describe;
using QtRocket::Test::HandlerRun;
using QtRocket::Test::runHandler;

using Texts = std::vector<std::string>;

/// The Config and the warnings of reading @p xml, a <config> element.
struct Read
{
    std::string config;
    Texts       warnings;
    std::string ownList;  ///< the handler's own list: the entries without a key
};

[[nodiscard]] Read read(std::string_view xml)
{
    ConfigHandler    handler;
    const HandlerRun run = runHandler(handler, xml);
    EXPECT_TRUE(run.result.has_value());
    return {.config   = describe(handler.getConfig()),
            .warnings = run.texts(),
            .ownList  = describe(Config::Value(handler.getList()))};
}

TEST(ConfigHandler, ReadsBooleansStringsAndNumbers)
{
    const Read scalars = read(
        "<config><entry key='b' type='boolean'>true</entry>"
        "<entry key='s' type='string'> x </entry>"
        "<entry key='n' type='number'>250</entry></config>");
    EXPECT_EQ(scalars.config, "{b = Boolean true; s = String  x ; n = Integer 250; }");
    EXPECT_EQ(scalars.warnings, Texts{});

    EXPECT_EQ(
        read("<config><entry key='i' type='number'>5</entry>"
             "<entry key='l' type='number'>4294967296</entry>"
             "<entry key='big' type='number'>123456789012345678901234567890</entry>"
             "<entry key='d' type='number'>2.5</entry>"
             "<entry key='dec' type='number'>2.50</entry></config>")
            .config,
        "{i = Integer 5; l = Long 4294967296; big = BigDecimal 123456789012345678901234567890 "
        "unscaled=123456789012345678901234567890 scale=0; d = Double 2.5; dec = BigDecimal "
        "2.50 unscaled=250 scale=2; }");
}

TEST(ConfigHandler, AStringKeepsItsTextAndANumberIsTrimmed)
{
    EXPECT_EQ(read("<config><entry key='s' type='string'>a &lt; b <![CDATA[<c>]]> &#10;d</entry>"
                   "</config>")
                  .config,
              "{s = String a < b <c> \nd; }");
    EXPECT_EQ(read("<config><entry key='n' type='number'>\n  2.5\n</entry></config>").config,
              "{n = Double 2.5; }");
    // A boolean is not trimmed.
    EXPECT_EQ(read("<config><entry key='b' type='boolean'> true </entry></config>").config,
              "{b = Boolean false; }");
}

TEST(ConfigHandler, ALaterEntryReplacesTheValueOfItsKeyInPlace)
{
    EXPECT_EQ(read("<config><entry key='k' type='number'>1</entry>"
                   "<entry key='z' type='string'>z</entry>"
                   "<entry key='k' type='string'>two</entry></config>")
                  .config,
              "{k = String two; z = String z; }");
    // An empty key is a key.
    EXPECT_EQ(read("<config><entry key='' type='number'>7</entry></config>").config,
              "{ = Integer 7; }");
}

TEST(ConfigHandler, AnEntryWithoutAValueIsDroppedSilently)
{
    const Read noType = read(
        "<config><entry key='a'>2</entry>"
        "<entry key='b' type='number'>3</entry></config>");
    EXPECT_EQ(noType.config, "{b = Integer 3; }");
    EXPECT_EQ(noType.warnings, Texts{});
    const Read unknownType = read(
        "<config><entry key='a' type='integer'>2</entry>"
        "<entry key='b' type='number'>3</entry></config>");
    EXPECT_EQ(unknownType.config, "{b = Integer 3; }");
    EXPECT_EQ(unknownType.warnings, Texts{});
    const Read badNumber = read(
        "<config><entry key='a' type='number'>abc</entry>"
        "<entry key='b' type='number'>3</entry></config>");
    EXPECT_EQ(badNumber.config, "{b = Integer 3; }");
    EXPECT_EQ(badNumber.warnings, Texts{});
}

TEST(ConfigHandler, AnEntryWithoutAKeyGoesIntoTheHandlersOwnList)
{
    const Read noKey = read(
        "<config><entry type='number'>1</entry>"
        "<entry key='a' type='number'>2</entry></config>");
    EXPECT_EQ(noKey.config, "{a = Integer 2; }");
    EXPECT_EQ(noKey.ownList, "List[Integer 1, ]");
    EXPECT_EQ(noKey.warnings, Texts{});
}

TEST(ConfigHandler, OtherAttributesAndStrayTextAreNotWarnedOf)
{
    const Read extra = read("<config><entry key='a' type='number' extra='1'>2</entry></config>");
    EXPECT_EQ(extra.config, "{a = Integer 2; }");
    EXPECT_EQ(extra.warnings, Texts{});
    // The text of <config> itself is its parent's to judge.
    const Read stray = read("<config>stray<entry key='a' type='number'>2</entry></config>");
    EXPECT_EQ(stray.config, "{a = Integer 2; }");
    EXPECT_EQ(stray.warnings, Texts{});
}

TEST(ConfigHandler, AnotherElementIsWarnedOfByItsTextAndItsAttributes)
{
    const Read text = read(
        "<config><other>text</other>"
        "<entry key='a' type='number'>2</entry></config>");
    EXPECT_EQ(text.config, "{a = Integer 2; }");
    EXPECT_EQ(text.warnings, Texts{"Unknown text in element 'other', ignoring."});
    const Read attributes = read(
        "<config><other x='1'/>"
        "<entry key='a' type='number'>2</entry></config>");
    EXPECT_EQ(attributes.config, "{a = Integer 2; }");
    EXPECT_EQ(attributes.warnings, Texts{"Unknown attributes in element 'other', ignoring."});
}

TEST(ConfigHandler, AnEntryWithAChildElementLosesItsValue)
{
    // The child is ignored with the text handler's warning, and the reader's bookkeeping for
    // an ignored element (see DelegatorHandler) then hands the entry's close the wrong
    // attributes: the entry is dropped, as in OpenRocket.
    const Read child = read(
        "<config><entry key='a' type='string'>x<child/>y</entry>"
        "<entry key='b' type='number'>2</entry></config>");
    EXPECT_EQ(child.config, "{b = Integer 2; }");
    EXPECT_EQ(child.warnings, Texts{"Unknown element child, ignoring."});
}

// ---- lists: loaded here, never by OpenRocket ---------------------------------------------------

TEST(ConfigHandler, AListEntryIsLoaded)
{
    // OpenRocket: "{after = Integer 9; }".
    const Read list = read(
        "<config><entry key='l' type='list'><entry type='number'>1</entry>"
        "<entry type='string'>two</entry><entry type='boolean'>true</entry>"
        "</entry><entry key='after' type='number'>9</entry></config>");
    EXPECT_EQ(list.config,
              "{l = List[Integer 1, String two, Boolean true, ]; after = Integer 9; }");
    EXPECT_EQ(list.warnings, Texts{});
    EXPECT_EQ(list.ownList, "List[]");
}

TEST(ConfigHandler, AListInAListIsLoaded)
{
    // OpenRocket: "{}".
    EXPECT_EQ(read("<config><entry key='l' type='list'><entry type='list'>"
                   "<entry type='number'>1</entry></entry><entry type='number'>2</entry></entry>"
                   "</config>")
                  .config,
              "{l = List[List[Integer 1, ], Integer 2, ]; }");
}

TEST(ConfigHandler, AnEmptyListEntryIsAnEmptyList)
{
    // OpenRocket: "{}".
    EXPECT_EQ(read("<config><entry key='l' type='list'></entry><entry key='l2' type='list'/>"
                   "</config>")
                  .config,
              "{l = List[]; l2 = List[]; }");
}

TEST(ConfigHandler, TwoListsDoNotShareTheirElements)
{
    EXPECT_EQ(read("<config><entry key='a' type='list'><entry type='number'>1</entry></entry>"
                   "<entry key='b' type='list'><entry type='number'>2</entry>"
                   "<entry type='number'>3</entry></entry></config>")
                  .config,
              "{a = List[Integer 1, ]; b = List[Integer 2, Integer 3, ]; }");
}

TEST(ConfigHandler, InAListOnlyTheEntriesWithoutAKeyAreElements)
{
    // An entry with a key goes into the Config of the list's own handler, which nobody reads;
    // one without a value is dropped; text in the list is not looked at.
    EXPECT_EQ(read("<config><entry key='l' type='list'><entry key='inner' type='number'>1</entry>"
                   "<entry type='number'>2</entry></entry></config>")
                  .config,
              "{l = List[Integer 2, ]; }");
    EXPECT_EQ(read("<config><entry key='l' type='list'><entry type='number'>abc</entry>"
                   "<entry type='bogus'>x</entry><entry type='number'>2</entry></entry></config>")
                  .config,
              "{l = List[Integer 2, ]; }");
    const Read text = read(
        "<config><entry key='l' type='list'>text<entry type='number'>1</entry>"
        "</entry></config>");
    EXPECT_EQ(text.config, "{l = List[Integer 1, ]; }");
    EXPECT_EQ(text.warnings, Texts{});
}

TEST(ConfigHandler, AListWithoutAKeyGoesIntoTheHandlersOwnList)
{
    const Read list = read(
        "<config><entry type='list'><entry type='number'>1</entry></entry>"
        "<entry key='a' type='number'>2</entry></config>");
    EXPECT_EQ(list.config, "{a = Integer 2; }");
    EXPECT_EQ(list.ownList, "List[List[Integer 1, ], ]");
}

/// A <config> with one entry "l" that is @p depth list entries, one in the other, the innermost
/// holding the number 7.
[[nodiscard]] std::string nestedLists(int depth)
{
    std::string xml = "<config>";
    for (int i = 0; i < depth; i++)
    {
        xml += i == 0 ? "<entry key='l' type='list'>" : "<entry type='list'>";
    }
    xml += "<entry type='number'>7</entry>";
    for (int i = 0; i < depth; i++)
    {
        xml += "</entry>";
    }
    return xml + "</config>";
}

/// "List[List[ ... Integer 7, ], ]" for @p depth lists.
[[nodiscard]] std::string describedLists(int depth)
{
    std::string text;
    for (int i = 0; i < depth; i++)
    {
        text += "List[";
    }
    text += "Integer 7, ";
    for (int i = 0; i < depth; i++)
    {
        text += i + 1 < depth ? "], " : "]";
    }
    return text;
}

TEST(ConfigHandler, ListsNestUpToALimit)
{
    EXPECT_EQ(ConfigHandler::kMaxListDepth, 32);
    const Read deepest = read(nestedLists(32));
    EXPECT_EQ(deepest.config, "{l = " + describedLists(32) + "; }");
    EXPECT_EQ(deepest.warnings, Texts{});
}

TEST(ConfigHandler, AListNestedTooDeeplyIsNotRead)
{
    // Not OpenRocket's (which reads no list): the list entry beyond the limit has no value, and
    // what stands in it is ignored as the content of an entry that holds text.
    const Read tooDeep = read(nestedLists(33));
    EXPECT_EQ(tooDeep.warnings, (Texts{"List entries nested too deeply, ignoring.",
                                       "Unknown element entry, ignoring."}));
    EXPECT_FALSE(tooDeep.config.contains("Integer 7"));
}

TEST(ConfigHandler, AFileCannotMakeTheHandlersNestWithoutBound)
{
    // Hostile input: a hundred thousand list entries, one in the other. Read without a deep
    // recursion anywhere, to the two warnings of the test above.
    const Read hostile = read(nestedLists(100000));
    EXPECT_EQ(hostile.warnings, (Texts{"List entries nested too deeply, ignoring.",
                                       "Unknown element entry, ignoring."}));
    EXPECT_FALSE(hostile.config.contains("Integer 7"));
}

TEST(ConfigHandler, AListHandlerOfItsOwnDepthCountsFromThere)
{
    // The handler of a list entry that stands in 31 others: one more list is read, two are not.
    ConfigHandler    nearlyFull(31);
    const HandlerRun one =
        runHandler(nearlyFull,
                   "<entry><entry type='list'><entry type='number'>1</entry></entry>"
                   "</entry>");
    EXPECT_EQ(one.texts(), Texts{});
    EXPECT_EQ(describe(Config::Value(nearlyFull.getList())), "List[List[Integer 1, ], ]");

    ConfigHandler    full(32);
    const HandlerRun none =
        runHandler(full,
                   "<entry><entry type='list'><entry type='number'>1</entry></entry>"
                   "</entry>");
    EXPECT_EQ(none.texts().at(0), "List entries nested too deeply, ignoring.");
    EXPECT_EQ(describe(Config::Value(full.getList())), "List[]");
}

// ---- hostile input: an entry that closes as a list it never opened -----------------------------
//
// DelegatorHandler hands an entry that has an ignored child element the attributes of that
// child when it closes (OpenRocket's slip, kept: see AnEntryWithAChildElementLosesItsValue). So
// a file can make any entry close with type='list'. OpenRocket loads no list at all and so has
// nothing to give such an entry; with the lists loaded, such an entry must not find the list
// of an entry before it.

TEST(ConfigHandler, AnEntryThatClosesAsAListItNeverOpenedGetsNoList)
{
    // The review's probe: this stored the list of 'L' a second and a third time, under the
    // keys 'stolen1' and 'stolen2'.
    const Read stolen = read(
        "<config><entry key='L' type='list'><entry type='number'>1</entry>"
        "<entry type='number'>2</entry></entry>"
        "<entry key='s1' type='string'>a<x type='list' key='stolen1'/></entry>"
        "<entry key='s2' type='string'>b<x type='list' key='stolen2'/></entry></config>");
    EXPECT_EQ(stolen.config, "{L = List[Integer 1, Integer 2, ]; }");
    EXPECT_EQ(stolen.ownList, "List[]");
    EXPECT_EQ(stolen.warnings, Texts{"Unknown element x, ignoring."});

    // Without a key the copies went into the handler's own list.
    const Read unkeyed = read(
        "<config><entry key='a' type='list'><entry type='number'>1</entry></entry>"
        "<entry key='b' type='string'>x<c type='list'/></entry></config>");
    EXPECT_EQ(unkeyed.config, "{a = List[Integer 1, ]; }");
    EXPECT_EQ(unkeyed.ownList, "List[]");
}

TEST(ConfigHandler, AListEntryWhoseCloseSlippedDoesNotLeaveItsListBehind)
{
    // The slip also goes outwards: here the list entry 'a' closes with the attributes and the
    // text of the string entry inside it (which closed with those of its child), so 'a' is a
    // string without a key, and its list, the number 7, is dropped. The entry after it claims
    // to be a list under the key 'late': it used to get the list of 'a'.
    const Read slipped = read(
        "<config><entry key='a' type='list'><entry type='number'>7</entry>"
        "<entry type='string'>x<c key='z' type='number'/></entry></entry>"
        "<entry key='b' type='string'>y<d key='late' type='list'/></entry></config>");
    EXPECT_EQ(slipped.config, "{}");
    EXPECT_EQ(slipped.ownList, "List[String x, ]");
}

/// One level of the review's probe (level(depth, 2)): a list entry that holds the level below,
/// then @p slips entries that close with the attributes of an ignored child and so asked for
/// the stale list of the level below once more each, then an element that leaves {type=list}
/// on the attribute stack for the close of this level's own entry.
[[nodiscard]] std::string blowUpLevel(int depth, int slips)
{
    std::string text = "<entry type='list'>";
    if (depth == 0)
    {
        return text + "<entry type='number'>1</entry></entry>";
    }
    text += blowUpLevel(depth - 1, slips);
    for (int i = 0; i < slips; i++)
    {
        text += "<entry type='string'>a<x type='list'/></entry>";
    }
    return text + "<foo type='list'><x/></foo></entry>";
}

/// The number of values in @p value that are no lists, lists counted by what they hold.
[[nodiscard]] std::size_t leaves(const Config::Value& value)
{
    const Config::List* const list = std::get_if<Config::List>(&value.variant());
    if (list == nullptr)
    {
        return 1;
    }
    std::size_t count = 0;
    for (const Config::Value& element : *list)
    {
        count += leaves(element);
    }
    return count;
}

// The review's probe: every level copied the list of the level below three times, so 32 levels
// (4.7 kB of XML) asked for 3^32 values, and std::bad_alloc left the loader. Now a list is
// moved, once, to the entry that opened it: the file's one number is one value.
TEST(ConfigHandler, AFileCannotMultiplyAListThroughEntriesThatClaimIt)
{
    const std::string xml =
        "<config>" + blowUpLevel(ConfigHandler::kMaxListDepth - 1, 2) + "</config>";
    EXPECT_LT(xml.size(), 5000U);
    ConfigHandler    handler;
    const HandlerRun run = runHandler(handler, xml);
    EXPECT_TRUE(run.result.has_value());
    EXPECT_EQ(leaves(Config::Value(handler.getList())), 1U);
    EXPECT_TRUE(handler.getConfig().keySet().empty());
}

TEST(ConfigHandler, AnEntryThatOnlyClaimsToBeAListAfterAnotherIsNotConfusedWithIt)
{
    // A scalar entry after a list entry: its value is its own text, not the list before it.
    EXPECT_EQ(read("<config><entry key='l' type='list'><entry type='number'>1</entry></entry>"
                   "<entry key='s' type='string'>text</entry></config>")
                  .config,
              "{l = List[Integer 1, ]; s = String text; }");
}

}  // namespace
