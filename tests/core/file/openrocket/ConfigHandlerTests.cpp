#include "QtRocket/file/openrocket/ConfigHandler.h"

#include <string>
#include <string_view>
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

TEST(ConfigHandler, AnEntryThatOnlyClaimsToBeAListAfterAnotherIsNotConfusedWithIt)
{
    // A scalar entry after a list entry: its value is its own text, not the list before it.
    EXPECT_EQ(read("<config><entry key='l' type='list'><entry type='number'>1</entry></entry>"
                   "<entry key='s' type='string'>text</entry></config>")
                  .config,
              "{l = List[Integer 1, ]; s = String text; }");
}

}  // namespace
