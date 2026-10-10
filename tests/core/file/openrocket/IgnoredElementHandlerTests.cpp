#include "QtRocket/file/openrocket/IgnoredElementHandler.h"

#include <format>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/SimpleSax.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

// IgnoredElementHandler is QtRocket's own: OpenRocket ignores an element by returning null from
// openElement(), which leaves the element's attributes and text to the elements around it.
// These tests show the difference with a handler that ignores every <skip> the one way or the
// other and writes down what it is told.

namespace
{

using QtRocket::ElementHandler;
using QtRocket::IgnoredElementHandler;
using QtRocket::Result;
using QtRocket::SimpleSax;
using QtRocket::WarningSet;

/// "name=value" for each attribute, separated by blanks, in the order of the names.
[[nodiscard]] std::string attributesText(const ElementHandler::Attributes& attributes)
{
    std::string text;
    for (const auto& [name, value] : attributes)
    {
        text += std::format("{}{}={}", text.empty() ? "" : " ", name, value);
    }
    return text;
}

/// The handler of every element of a document but those named "skip", which it ignores: by
/// handing them to IgnoredElementHandler, or as OpenRocket's handlers do, by returning null. It
/// writes down every call that tells it of an element's end.
class Recorder final : public ElementHandler
{
public:
    enum class Ignore
    {
        WITH_THE_HANDLER,
        WITH_NULL,
    };

    explicit Recorder(Ignore ignore) noexcept : m_ignore(ignore) { }

    [[nodiscard]] const std::string& log() const noexcept { return m_log; }

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view element,
                                                      const Attributes& /*attributes*/,
                                                      WarningSet& /*warnings*/) override
    {
        if (element != "skip")
        {
            return this;
        }
        if (m_ignore == Ignore::WITH_NULL)
        {
            return nullptr;
        }
        return &IgnoredElementHandler::instance();
    }

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet& /*warnings*/) override
    {
        m_log +=
            std::format("close {} {{{}}} [{}]\n", element, attributesText(attributes), content);
        return {};
    }

    [[nodiscard]] Result<void> endHandler(std::string_view element, const Attributes& attributes,
                                          std::string_view content,
                                          WarningSet& /*warnings*/) override
    {
        m_log += std::format("end {} {{{}}} [{}]\n", element, attributesText(attributes), content);
        return {};
    }

private:
    Ignore      m_ignore;
    std::string m_log;
};

/// What a Recorder that ignores the one way or the other is told while @p xml is read, then
/// "warnings <n>" and, when the reading failed, the failure.
[[nodiscard]] std::string logOf(std::string_view xml, Recorder::Ignore ignore)
{
    Recorder           recorder(ignore);
    WarningSet         warnings;
    const Result<void> read = SimpleSax::readXml(xml, recorder, warnings);
    std::string        log  = recorder.log() + std::format("warnings {}\n", warnings.size());
    if (!read)
    {
        log += "FAILED " + read.error().message + "\n";
    }
    return log;
}

constexpr std::string_view kDocument =
    "<a x='1'>one<skip y='2'>two<inner z='3'>three<deeper/></inner><skip/></skip>four"
    "<b w='4'>five</b></a>";

TEST(IgnoredElementHandler, IsOneSharedHandlerThatIsItsOwnForEveryElement)
{
    IgnoredElementHandler& handler = IgnoredElementHandler::instance();
    EXPECT_EQ(&handler, &IgnoredElementHandler::instance());

    WarningSet                       warnings;
    const ElementHandler::Attributes attributes{{"name", "value"}};
    const Result<ElementHandler*>    child = handler.openElement("any", attributes, warnings);
    ASSERT_TRUE(child.has_value());
    EXPECT_EQ(*child, &handler);
    EXPECT_TRUE(handler.closeElement("any", attributes, "text", warnings).has_value());
    EXPECT_TRUE(handler.endHandler("any", attributes, "text", warnings).has_value());
    EXPECT_TRUE(warnings.empty());
}

// The element is ignored with everything in it, and the handler that ignored it is told of
// its end with the element's own attributes and text. Every other element closes with its own.
TEST(IgnoredElementHandler, AnIgnoredElementLeavesTheOthersTheirAttributesAndText)
{
    EXPECT_EQ(logOf(kDocument, Recorder::Ignore::WITH_THE_HANDLER),
              "close skip {y=2} [two]\n"
              "end b {w=4} [five]\n"
              "close b {w=4} [five]\n"
              "end a {x=1} [onefour]\n"
              "close a {x=1} [onefour]\n"
              "warnings 0\n");
}

// What the null return of OpenRocket's handlers gives for the same document: nobody is told of
// the end of <skip>, and <a> closes with the attributes of <skip> and with the text behind it.
TEST(IgnoredElementHandler, TheNullReturnHandsTheAttributesToTheElementAround)
{
    EXPECT_EQ(logOf(kDocument, Recorder::Ignore::WITH_NULL),
              "end b {w=4} [five]\n"
              "close b {w=4} [five]\n"
              "end a {y=2} [four]\n"
              "close a {y=2} [four]\n"
              "warnings 0\n");
}

// Elements one in the other, each ignored: every level is told of its own end.
TEST(IgnoredElementHandler, ElementsIgnoredOneBehindTheOtherEachCloseAsTheirOwn)
{
    EXPECT_EQ(logOf("<a><skip n='1'/><skip n='2'>t</skip><b><skip n='3'/></b></a>",
                    Recorder::Ignore::WITH_THE_HANDLER),
              "close skip {n=1} []\n"
              "close skip {n=2} [t]\n"
              "close skip {n=3} []\n"
              "end b {} []\n"
              "close b {} []\n"
              "end a {} []\n"
              "close a {} []\n"
              "warnings 0\n");
}

}  // namespace
