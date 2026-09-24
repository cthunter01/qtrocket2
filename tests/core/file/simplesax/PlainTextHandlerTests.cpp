#include "QtRocket/file/simplesax/PlainTextHandler.h"

#include <string>

#include <gtest/gtest.h>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace
{

using QtRocket::ElementHandler;
using QtRocket::PlainTextHandler;
using QtRocket::WarningSet;

TEST(PlainTextHandler, IgnoresChildElementsWithAWarning)
{
    PlainTextHandler&                       handler = PlainTextHandler::instance();
    WarningSet                              warnings;
    const QtRocket::Result<ElementHandler*> child = handler.openElement("b", {}, warnings);
    ASSERT_TRUE(child);
    EXPECT_EQ(*child, nullptr);
    ASSERT_EQ(warnings.size(), 1U);
    EXPECT_EQ(warnings.begin()->messageDescription(), "Unknown element b, ignoring.");
}

TEST(PlainTextHandler, ClosesSilently)
{
    WarningSet warnings;
    EXPECT_TRUE(PlainTextHandler::instance().closeElement("b", {{"x", "1"}}, "text", warnings));
    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(&PlainTextHandler::instance(), &PlainTextHandler::instance());
}

}  // namespace
