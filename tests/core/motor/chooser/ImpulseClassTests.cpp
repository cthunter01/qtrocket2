#include "QtRocket/motor/chooser/ImpulseClass.h"

#include <cstddef>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "QtRocket/util/BugError.h"

// OpenRocket has no JUnit test of ImpulseClass; the values are ImpulseClass.java's.

namespace
{

using QtRocket::ImpulseClass;
using QtRocket::kAllImpulseClasses;

TEST(ImpulseClass, HasOpenRocketsLettersInOrder)
{
    std::string letters;
    for (const ImpulseClass impulseClass : kAllImpulseClasses)
    {
        letters += toString(impulseClass);
    }
    EXPECT_EQ(letters, "ABCDEFGHIJKLMNO");
}

/// A starts below zero "to catch the MMX motors", and O ends at the largest double.
TEST(ImpulseClass, SpansOpenRocketsRanges)
{
    EXPECT_EQ(getLow(ImpulseClass::A), -1.0);
    EXPECT_EQ(getHigh(ImpulseClass::A), 2.5);
    EXPECT_EQ(getLow(ImpulseClass::B), 2.5);
    EXPECT_EQ(getHigh(ImpulseClass::H), 320.0);
    EXPECT_EQ(getLow(ImpulseClass::N), 10240.0);
    EXPECT_EQ(getHigh(ImpulseClass::N), 20480.0);
    EXPECT_EQ(getLow(ImpulseClass::O), 20480.0);
    EXPECT_EQ(getHigh(ImpulseClass::O), std::numeric_limits<double>::max());
}

/// From B on, each class starts where the one before ends and is twice its size.
TEST(ImpulseClass, DoublesFromClassToClass)
{
    for (std::size_t index = 1; index + 1 < kAllImpulseClasses.size(); index++)
    {
        const ImpulseClass impulseClass = kAllImpulseClasses.at(index);
        EXPECT_EQ(getLow(impulseClass), getHigh(kAllImpulseClasses.at(index - 1)));
        EXPECT_EQ(getHigh(impulseClass), 2 * getLow(impulseClass));
    }
}

TEST(ImpulseClass, RefusesAValueOutsideTheEnum)
{
    const auto outside = static_cast<ImpulseClass>(kAllImpulseClasses.size());
    EXPECT_THROW(static_cast<void>(toString(outside)), QtRocket::BugError);
    EXPECT_THROW(static_cast<void>(getLow(outside)), QtRocket::BugError);
}

}  // namespace
