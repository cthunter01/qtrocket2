#include "QtRocket/file/openrocket/EntryHandler.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>

#include "QtRocket/file/openrocket/EntryHelper.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Error.h"
#include "file/openrocket/EntryTestSupport.h"

namespace
{

using QtRocket::BugError;
using QtRocket::Config;
using QtRocket::ElementHandler;
using QtRocket::EntryHandler;
using QtRocket::Result;
using QtRocket::WarningSet;
using QtRocket::Test::describe;

/// An entry handler that lets a test do what a subclass does.
class TestEntryHandler final : public EntryHandler
{
public:
    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view /*element*/,
                                                      const Attributes& /*attributes*/,
                                                      WarningSet& /*warnings*/) override
    {
        return &QtRocket::PlainTextHandler::instance();
    }

    EntryHandler* open(std::unique_ptr<EntryHandler> handler)
    {
        return openList(std::move(handler));
    }
    void close() noexcept { closeList(); }
    void add(Config::Value value) { addToList(std::move(value)); }
};

TEST(EntryHandler, HasNoNestedListUntilItOpensOne)
{
    const TestEntryHandler handler;
    EXPECT_EQ(handler.getNestedList(), nullptr);
    EXPECT_TRUE(handler.getList().empty());
}

TEST(EntryHandler, TheNestedListIsTheListOfTheHandlerItOpened)
{
    TestEntryHandler  handler;
    auto              nested = std::make_unique<TestEntryHandler>();
    TestEntryHandler* opened = nested.get();
    EXPECT_EQ(handler.open(std::move(nested)), opened);

    ASSERT_NE(handler.getNestedList(), nullptr);
    EXPECT_TRUE(handler.getNestedList()->empty());
    opened->add(Config::Value(1));
    opened->add(Config::Value("two"));
    EXPECT_EQ(describe(Config::Value(*handler.getNestedList())), "List[Integer 1, String two, ]");
    // The handler's own list is another one.
    EXPECT_TRUE(handler.getList().empty());
    handler.add(Config::Value(true));
    EXPECT_EQ(describe(Config::Value(handler.getList())), "List[Boolean true, ]");
    EXPECT_EQ(handler.getNestedList()->size(), 2U);
}

TEST(EntryHandler, TheNextListReplacesTheOneBefore)
{
    TestEntryHandler handler;
    auto             first = std::make_unique<TestEntryHandler>();
    first->add(Config::Value(1));
    static_cast<void>(handler.open(std::move(first)));
    EXPECT_EQ(handler.getNestedList()->size(), 1U);
    static_cast<void>(handler.open(std::make_unique<TestEntryHandler>()));
    EXPECT_TRUE(handler.getNestedList()->empty());
}

TEST(EntryHandler, ClosingTheListForgetsIt)
{
    TestEntryHandler handler;
    handler.close();  // nothing to forget
    EXPECT_EQ(handler.getNestedList(), nullptr);
    static_cast<void>(handler.open(std::make_unique<TestEntryHandler>()));
    EXPECT_NE(handler.getNestedList(), nullptr);
    handler.close();
    EXPECT_EQ(handler.getNestedList(), nullptr);
}

// ---- a nested list is read once (not OpenRocket's, which hands the same list out for ever) ----

TEST(EntryHandler, TakingTheNestedListMovesItOutAndForgetsItsHandler)
{
    TestEntryHandler handler;
    // Nothing to take before a list entry was opened.
    EXPECT_EQ(handler.takeNestedList(), std::nullopt);

    auto              nested = std::make_unique<TestEntryHandler>();
    TestEntryHandler* opened = nested.get();
    static_cast<void>(handler.open(std::move(nested)));
    opened->add(Config::Value(1));
    opened->add(Config::Value("two"));

    const std::optional<Config::List> taken = handler.takeNestedList();
    EXPECT_EQ(describe(Config::Value(taken.value_or(Config::List{}))),
              "List[Integer 1, String two, ]");
    // Taken once: the handler of the list entry is gone, and so is the list.
    EXPECT_EQ(handler.getNestedList(), nullptr);
    EXPECT_EQ(handler.takeNestedList(), std::nullopt);
    // The handler's own list is another one and stays.
    handler.add(Config::Value(true));
    EXPECT_EQ(describe(Config::Value(handler.getList())), "List[Boolean true, ]");
}

TEST(EntryHandler, AnEmptyNestedListIsTakenAsAnEmptyList)
{
    TestEntryHandler handler;
    static_cast<void>(handler.open(std::make_unique<TestEntryHandler>()));
    const std::optional<Config::List> taken = handler.takeNestedList();
    EXPECT_TRUE(taken.has_value());
    EXPECT_TRUE(taken.value_or(Config::List{Config::Value(1)}).empty());
}

/// The value EntryHelper gives an entry of type @p type that @p handler closes, described, or
/// "null".
[[nodiscard]] std::string valueOfEntry(TestEntryHandler& handler, std::string_view type)
{
    const ElementHandler::Attributes   attributes{{"type", std::string(type)}};
    const std::optional<Config::Value> value =
        QtRocket::EntryHelper::getValueFromEntry(handler, attributes, "text");
    return value.has_value() ? describe(*value) : "null";
}

/// A handler that has opened a list entry with the one value 1.
void openListOfOne(TestEntryHandler& handler)
{
    auto              nested = std::make_unique<TestEntryHandler>();
    TestEntryHandler* opened = nested.get();
    static_cast<void>(handler.open(std::move(nested)));
    opened->add(Config::Value(1));
}

// The rule the loader rests on: the close of an entry takes the nested list, whatever type the
// attributes give the entry, so the entry after it finds none. Java's EntryHelper hands the
// list of the last list entry to every later entry that closes as a list.
TEST(EntryHandler, TheNestedListGoesToTheFirstEntryThatClosesAndToNoOther)
{
    TestEntryHandler handler;
    openListOfOne(handler);
    EXPECT_EQ(valueOfEntry(handler, "list"), "List[Integer 1, ]");
    // Java: "List[Integer 1, ]" again, the same list object.
    EXPECT_EQ(valueOfEntry(handler, "list"), "null");

    // An entry that closes as something else takes the list away all the same: it was the
    // list entry's close, with attributes that slipped (see DelegatorHandler).
    openListOfOne(handler);
    EXPECT_EQ(valueOfEntry(handler, "string"), "String text");
    EXPECT_EQ(valueOfEntry(handler, "list"), "null");
    openListOfOne(handler);
    EXPECT_EQ(valueOfEntry(handler, "bogus"), "null");
    EXPECT_EQ(handler.getNestedList(), nullptr);
    // And an entry without a type attribute.
    openListOfOne(handler);
    EXPECT_FALSE(
        QtRocket::EntryHelper::getValueFromEntry(handler, ElementHandler::Attributes{}, "x")
            .has_value());
    EXPECT_EQ(handler.getNestedList(), nullptr);
}

TEST(EntryHandler, ANullListHandlerIsABug)
{
    TestEntryHandler handler;
    EXPECT_THROW(static_cast<void>(handler.open(nullptr)), BugError);
}

TEST(EntryHandler, WarnsOfUnknownContentAsItsBaseDoes)
{
    TestEntryHandler handler;
    WarningSet       warnings;
    EXPECT_TRUE(handler.closeElement("x", {{"a", "1"}}, "text", warnings).has_value());
    EXPECT_EQ(warnings.size(), 2U);
}

}  // namespace
