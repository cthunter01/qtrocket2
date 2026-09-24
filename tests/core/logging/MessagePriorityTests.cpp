#include "QtRocket/logging/MessagePriority.h"

#include <gtest/gtest.h>

namespace
{

using QtRocket::exportLabel;
using QtRocket::kAllPriorities;
using QtRocket::MessagePriority;
using QtRocket::priorityFromExportLabel;

TEST(MessagePriority, ExportLabels)
{
    EXPECT_EQ(exportLabel(MessagePriority::LOW), "LOW");
    EXPECT_EQ(exportLabel(MessagePriority::NORMAL), "NORMAL");
    EXPECT_EQ(exportLabel(MessagePriority::HIGH), "HIGH");
}

TEST(MessagePriority, FromExportLabelRoundTripsAndDefaultsToNormal)
{
    for (const MessagePriority priority : kAllPriorities)
    {
        EXPECT_EQ(priorityFromExportLabel(exportLabel(priority)), priority);
    }
    EXPECT_EQ(priorityFromExportLabel("critical"), MessagePriority::NORMAL);
    EXPECT_EQ(priorityFromExportLabel(""), MessagePriority::NORMAL);
    EXPECT_EQ(priorityFromExportLabel("low"), MessagePriority::NORMAL);  // case-sensitive, as Java
    EXPECT_EQ(priorityFromExportLabel("HIGH "), MessagePriority::NORMAL);
}

TEST(MessagePriority, AllPrioritiesAreInDeclarationOrder)
{
    static_assert(kAllPriorities.size() == 3);
    EXPECT_EQ(kAllPriorities[0], MessagePriority::LOW);
    EXPECT_EQ(kAllPriorities[1], MessagePriority::NORMAL);
    EXPECT_EQ(kAllPriorities[2], MessagePriority::HIGH);
    EXPECT_LT(MessagePriority::LOW, MessagePriority::NORMAL);
    EXPECT_LT(MessagePriority::NORMAL, MessagePriority::HIGH);
}

}  // namespace
