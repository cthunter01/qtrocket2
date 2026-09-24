#include "QtRocket/file/simplesax/DelegatorHandler.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"
#include "file/simplesax/RecordingHandler.h"

namespace
{

using QtRocket::DelegatorHandler;
using QtRocket::ElementHandler;
using QtRocket::WarningSet;
using QtRocket::Test::RecordingHandler;

TEST(DelegatorHandler, CallsTheHandlersInOpenRocketsOrder)
{
    // ElementHandler's documented sequence: the child's handler ends, then the parent closes the
    // child with its text; the initial handler never ends.
    RecordingHandler handler;
    WarningSet       warnings;
    DelegatorHandler sax(handler, warnings);
    ASSERT_TRUE(sax.startElement("foo", {}));
    ASSERT_TRUE(sax.startElement("bar", {{"x", "1"}}));
    sax.characters("mess");
    sax.characters("age");
    ASSERT_TRUE(sax.endElement("bar"));
    ASSERT_TRUE(sax.endElement("foo"));
    EXPECT_EQ(handler.log,
              (std::vector<std::string>{"open foo", "open bar x=1", "end bar",
                                        "close bar x=1 [message]", "end foo", "close foo []"}));
}

TEST(DelegatorHandler, KeepsOpenRocketsStackSlip)
{
    // An ignored element pushes a text buffer and its attributes but never pops them, so its
    // parent closes with the ignored element's attributes and the text after it.
    RecordingHandler handler;
    WarningSet       warnings;
    DelegatorHandler sax(handler, warnings);
    ASSERT_TRUE(sax.startElement("a", {{"own", "1"}}));
    sax.characters("before");
    ASSERT_TRUE(sax.startElement("ignore", {{"ignored", "2"}}));
    ASSERT_TRUE(sax.startElement("inside", {}));  // not even opened
    sax.characters("hidden");
    ASSERT_TRUE(sax.endElement("inside"));
    ASSERT_TRUE(sax.endElement("ignore"));
    sax.characters("after");
    ASSERT_TRUE(sax.endElement("a"));
    EXPECT_EQ(handler.log, (std::vector<std::string>{"open a own=1", "open ignore ignored=2",
                                                     "end a", "close a ignored=2 [after]"}));
}

TEST(DelegatorHandler, ReturnsAHandlersFailure)
{
    RecordingHandler handler;
    handler.failOn = "bad";
    WarningSet       warnings;
    DelegatorHandler sax(handler, warnings);
    ASSERT_TRUE(sax.startElement("a", {}));
    const QtRocket::Result<void> failed = sax.startElement("bad", {});
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error().message, "fail bad");
}

/// A handler whose closeElement() or endHandler() fails.
class FailingHandler final : public ElementHandler
{
public:
    bool failEnd = false;

    [[nodiscard]] QtRocket::Result<ElementHandler*> openElement(std::string_view /*element*/,
                                                                const Attributes& /*attributes*/,
                                                                WarningSet& /*warnings*/) override
    {
        return this;
    }

    [[nodiscard]] QtRocket::Result<void> closeElement(std::string_view element,
                                                      const Attributes& /*attributes*/,
                                                      std::string_view /*content*/,
                                                      WarningSet& /*warnings*/) override
    {
        return QtRocket::fail(QtRocket::ErrorCode::PARSE, "close " + std::string(element));
    }

    [[nodiscard]] QtRocket::Result<void> endHandler(std::string_view element,
                                                    const Attributes& /*attributes*/,
                                                    std::string_view /*content*/,
                                                    WarningSet& /*warnings*/) override
    {
        if (failEnd)
        {
            return QtRocket::fail(QtRocket::ErrorCode::PARSE, "end " + std::string(element));
        }
        return {};
    }
};

TEST(DelegatorHandler, ReturnsTheFirstFailureOfAnEnd)
{
    for (const bool failEnd : {false, true})
    {
        FailingHandler handler;
        handler.failEnd = failEnd;
        WarningSet       warnings;
        DelegatorHandler sax(handler, warnings);
        ASSERT_TRUE(sax.startElement("a", {}));
        const QtRocket::Result<void> ended = sax.endElement("a");
        ASSERT_FALSE(ended);
        EXPECT_EQ(ended.error().message, failEnd ? "end a" : "close a");
    }
}

}  // namespace
