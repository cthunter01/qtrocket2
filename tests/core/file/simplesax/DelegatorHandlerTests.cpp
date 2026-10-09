#include "QtRocket/file/simplesax/DelegatorHandler.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
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

/// A handler that records everything it is given, the attributes and the text of an end
/// included, and ignores the elements whose names start with an 'i'.
class EverythingRecorder final : public ElementHandler
{
public:
    std::vector<std::string> log;

    [[nodiscard]] QtRocket::Result<ElementHandler*> openElement(std::string_view  element,
                                                                const Attributes& attributes,
                                                                WarningSet& /*warnings*/) override
    {
        log.push_back(entry("open", element, attributes, ""));
        return element.starts_with('i') ? nullptr : this;
    }

    [[nodiscard]] QtRocket::Result<void> closeElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      std::string_view  content,
                                                      WarningSet& /*warnings*/) override
    {
        log.push_back(entry("close", element, attributes, content));
        return {};
    }

    [[nodiscard]] QtRocket::Result<void> endHandler(std::string_view  element,
                                                    const Attributes& attributes,
                                                    std::string_view  content,
                                                    WarningSet& /*warnings*/) override
    {
        log.push_back(entry("end", element, attributes, content));
        return {};
    }

    /// "<what> <element> <name>=<value>... [<content>]".
    [[nodiscard]] static std::string entry(std::string_view what, std::string_view element,
                                           const Attributes& attributes, std::string_view content)
    {
        std::string text = std::string(what) + " " + std::string(element);
        for (const auto& [name, value] : attributes)
        {
            text += ' ';
            text += name;
            text += '=';
            text += value;
        }
        return text + " [" + std::string(content) + "]";
    }
};

/// OpenRocket's DelegatorHandler written out again, with stacks from which nothing is ever
/// dropped: what the handlers of EverythingRecorder are given for a sequence of events.
class UntrimmedDelegator
{
public:
    void start(std::string_view name, const ElementHandler::Attributes& attributes)
    {
        if (m_ignore > 0)
        {
            m_ignore++;
            return;
        }
        m_data.emplace_back();
        m_attributes.push_back(attributes);
        log.push_back(EverythingRecorder::entry("open", name, attributes, ""));
        if (name.starts_with('i'))
        {
            m_ignore++;
        }
    }

    void characters(std::string_view text)
    {
        if (m_ignore == 0)
        {
            m_data.back() += text;
        }
    }

    void end(std::string_view name)
    {
        if (m_ignore > 0)
        {
            m_ignore--;
            return;
        }
        const std::string                data       = m_data.back();
        const ElementHandler::Attributes attributes = m_attributes.back();
        m_data.pop_back();
        m_attributes.pop_back();
        log.push_back(EverythingRecorder::entry("end", name, attributes, data));
        log.push_back(EverythingRecorder::entry("close", name, attributes, data));
    }

    std::vector<std::string> log;

private:
    std::vector<std::string>                m_data{std::string()};
    std::vector<ElementHandler::Attributes> m_attributes;
    int                                     m_ignore{0};
};

/// Feeds a sequence of @p steps events, drawn from a generator with the seed @p seed, to a
/// DelegatorHandler and to UntrimmedDelegator, and says where what the handlers were given
/// differs: "" when it is the same. The events are those of a well-formed document in which
/// about half of the elements are ignored ones, with attributes and text everywhere; a third of
/// the steps end an element, so the document gets some dozen elements deep.
[[nodiscard]] std::string differenceFromTheUntrimmedHandler(std::uint32_t seed, int steps)
{
    // A fixed seed: the test is to do the same on every run.
    // NOLINTNEXTLINE(bugprone-random-generator-seed)
    std::mt19937             random(seed);
    EverythingRecorder       recorder;
    WarningSet               warnings;
    DelegatorHandler         sax(recorder, warnings);
    UntrimmedDelegator       reference;
    std::vector<std::string> open;
    const auto               end = [&] {
        const std::string name = open.back();
        open.pop_back();
        reference.end(name);
        return sax.endElement(name).has_value();
    };
    for (int step = 0; step < steps; step++)
    {
        const auto          drawn = static_cast<std::uint32_t>(random());
        const std::uint32_t kind  = drawn % 6;
        if (kind < 2 && !open.empty())
        {
            if (!end())
            {
                return "an end failed";
            }
        }
        else if (kind < 4)
        {
            const std::string text = "t" + std::to_string(step);
            reference.characters(text);
            sax.characters(text);
        }
        else
        {
            const std::string name = ((drawn >> 8U) % 2 == 0 ? "i" : "e") + std::to_string(step);
            const ElementHandler::Attributes attributes{{"a", std::to_string(drawn >> 16U)},
                                                        {"at", name}};
            open.push_back(name);
            reference.start(name, attributes);
            if (!sax.startElement(name, attributes).has_value())
            {
                return "a start failed";
            }
        }
    }
    while (!open.empty())
    {
        if (!end())
        {
            return "an end failed";
        }
    }
    for (std::size_t i = 0; i < std::min(recorder.log.size(), reference.log.size()); i++)
    {
        if (recorder.log.at(i) != reference.log.at(i))
        {
            return "call " + std::to_string(i) + ": " + recorder.log.at(i) + " instead of " +
                   reference.log.at(i);
        }
    }
    return recorder.log.size() == reference.log.size() ? "" : "another number of calls";
}

// The layers an ignored element leaves on the stacks are dropped when no element can take them
// off any more, and nothing a handler is given changes by that: random documents, of which
// about half the elements are ignored, give every handler call the attributes and the text
// that OpenRocket's handler, which drops nothing, gives it, the shifted ones of its slip
// included.
TEST(DelegatorHandler, DroppingUnreachableLayersChangesNothingAHandlerIsGiven)
{
    EXPECT_EQ(differenceFromTheUntrimmedHandler(1, 6000), "");
    EXPECT_EQ(differenceFromTheUntrimmedHandler(2, 6000), "");
    EXPECT_EQ(differenceFromTheUntrimmedHandler(3, 6000), "");
    EXPECT_EQ(differenceFromTheUntrimmedHandler(20261009, 60000), "");
}

// A document of nothing but ignored elements keeps a few layers, not one for each element
// (OpenRocket keeps every one to the end of the document), and its root is closed as the slip
// has it: with the attributes of the last ignored element and the text behind that.
TEST(DelegatorHandler, KeepsFewLayersForManyIgnoredElements)
{
    RecordingHandler handler;
    WarningSet       warnings;
    DelegatorHandler sax(handler, warnings);
    ASSERT_TRUE(sax.startElement("root", {{"own", "1"}}));
    EXPECT_EQ(sax.retainedLayers(), 2U);
    std::size_t mostLayers = 0;
    for (int i = 0; i < 100000; i++)
    {
        ASSERT_TRUE(sax.startElement("ignore", {{"n", std::to_string(i)}}));
        ASSERT_TRUE(sax.endElement("ignore"));
        mostLayers = std::max(mostLayers, sax.retainedLayers());
    }
    EXPECT_LE(mostLayers, 21U);
    sax.characters("after");
    handler.log.clear();
    ASSERT_TRUE(sax.endElement("root"));
    EXPECT_EQ(handler.log, (std::vector<std::string>{"end root", "close root n=99999 [after]"}));
}

}  // namespace
