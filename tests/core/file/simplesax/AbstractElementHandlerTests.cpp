#include "QtRocket/file/simplesax/AbstractElementHandler.h"

#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace
{

using QtRocket::AbstractElementHandler;
using QtRocket::ElementHandler;
using QtRocket::Warning;
using QtRocket::WarningSet;

/// The smallest concrete handler, with parseDouble() in reach.
class Handler final : public AbstractElementHandler
{
public:
    [[nodiscard]] QtRocket::Result<ElementHandler*> openElement(std::string_view /*element*/,
                                                                const Attributes& /*attributes*/,
                                                                WarningSet& /*warnings*/) override
    {
        return nullptr;
    }

    using AbstractElementHandler::parseDouble;
};

[[nodiscard]] std::vector<std::string> texts(const WarningSet& warnings)
{
    std::vector<std::string> result;
    for (const Warning& warning : warnings)
    {
        result.push_back(warning.messageDescription());
    }
    return result;
}

TEST(AbstractElementHandler, WarnsOfUnexpectedTextAndAttributes)
{
    Handler    handler;
    WarningSet warnings;
    EXPECT_TRUE(handler.closeElement("e", {}, " \n\t ", warnings));
    EXPECT_TRUE(warnings.empty());  // blank text is no content (String.trim())
    EXPECT_TRUE(handler.closeElement("e", {{"a", "1"}}, " text ", warnings));
    EXPECT_EQ(texts(warnings), (std::vector<std::string>{"Unknown text in element 'e', ignoring.",
                                                         "Unknown attributes in element 'e', "
                                                         "ignoring."}));
}

TEST(AbstractElementHandler, EndHandlerDoesNothing)
{
    Handler    handler;
    WarningSet warnings;
    EXPECT_TRUE(handler.endHandler("e", {{"a", "1"}}, "text", warnings));
    EXPECT_TRUE(warnings.empty());
}

TEST(AbstractElementHandler, ParseDoubleWarnsOfWhatIsNoNumber)
{
    WarningSet           warnings;
    const Warning::Other warning = Warning::fromString("bad number");
    EXPECT_EQ(Handler::parseDouble("1.5e3", warnings, warning), 1500.0);
    EXPECT_EQ(Handler::parseDouble(" 2 ", warnings, warning), 2.0);  // Double.parseDouble trims
    EXPECT_TRUE(warnings.empty());
    EXPECT_TRUE(std::isnan(Handler::parseDouble("1,5", warnings, warning)));
    EXPECT_EQ(texts(warnings), std::vector<std::string>{"bad number"});
}

}  // namespace
