#include "QtRocket/greet.h"

#include <gtest/gtest.h>

namespace
{

TEST(Greet, IncludesName)
{
    EXPECT_EQ(QtRocket::greet("Ada"), "Hello, Ada!");
}

TEST(Greet, EmptyNameGreetsWorld)
{
    EXPECT_EQ(QtRocket::greet(""), "Hello, world!");
}

}  // namespace
