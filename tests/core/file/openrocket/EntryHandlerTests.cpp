#include "QtRocket/file/openrocket/EntryHandler.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>

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
