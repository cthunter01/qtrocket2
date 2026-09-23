#include "QtRocket/util/Version.h"

#include <string>

#include <gtest/gtest.h>

namespace
{

TEST(Version, MatchesCreatorString)
{
    EXPECT_EQ(QtRocket::creatorString(), "QtRocket " + std::string(QtRocket::version()));
}

TEST(Version, LooksLikeSemver)
{
    const auto v = QtRocket::version();
    EXPECT_FALSE(v.empty());
    EXPECT_NE(v.find('.'), std::string::npos);
}

}  // namespace
