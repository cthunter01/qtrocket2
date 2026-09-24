#include "QtRocket/file/simplesax/NullElementHandler.h"

#include <gtest/gtest.h>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace
{

using QtRocket::ElementHandler;
using QtRocket::NullElementHandler;
using QtRocket::WarningSet;

TEST(NullElementHandler, IgnoresChildElementsWithAWarning)
{
    WarningSet                              warnings;
    const QtRocket::Result<ElementHandler*> child =
        NullElementHandler::instance().openElement("x", {}, warnings);
    ASSERT_TRUE(child);
    EXPECT_EQ(*child, nullptr);
    ASSERT_EQ(warnings.size(), 1U);
    EXPECT_EQ(warnings.begin()->messageDescription(), "Unknown element x, ignoring.");
}

TEST(NullElementHandler, WarnsOfAChildsTextButNotOfItsAttributes)
{
    WarningSet warnings;
    EXPECT_TRUE(NullElementHandler::instance().closeElement("x", {{"a", "1"}}, "  ", warnings));
    EXPECT_TRUE(warnings.empty());
    EXPECT_TRUE(NullElementHandler::instance().closeElement("x", {{"a", "1"}}, "text", warnings));
    ASSERT_EQ(warnings.size(), 1U);
    EXPECT_EQ(warnings.begin()->messageDescription(), "Unknown text in element 'x', ignoring.");
}

}  // namespace
