#include "QtRocket/util/ModId.h"

#include <algorithm>
#include <compare>
#include <cstddef>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using QtRocket::ModId;

// ModIDTest.modIDTest
TEST(ModId, FreshIdsAreEqualToThemselvesAndIncreasing)
{
    const ModId n1;
    const ModId n2;
    const ModId n3;
    EXPECT_EQ(n2, n2);
    EXPECT_TRUE(n1.toInt() < n2.toInt());
    EXPECT_TRUE(n3.toInt() > n2.toInt());
}

TEST(ModId, FreshIdsArePositive)
{
    const ModId first;
    EXPECT_GT(first.toInt(), 0);
    EXPECT_GT(first, ModId::zero());
    EXPECT_GT(first, ModId::invalid());
}

TEST(ModId, FreshIdsAreDistinctAndConsecutive)
{
    constexpr std::size_t kCount = 100;
    std::vector<ModId>    ids;
    ids.reserve(kCount);
    for (std::size_t i = 0; i < kCount; ++i)
    {
        ids.emplace_back();
    }
    EXPECT_TRUE(std::ranges::is_sorted(ids));
    EXPECT_EQ(std::ranges::adjacent_find(ids), ids.end());  // no two equal
    // Sorted, distinct and spanning exactly kCount - 1: drawn back to back with nothing between.
    EXPECT_EQ(static_cast<std::size_t>(ids.back().toInt() - ids.front().toInt()), kCount - 1);
}

TEST(ModId, ConstantsMatchOpenRocket)
{
    constexpr ModId kZero    = ModId::zero();
    constexpr ModId kInvalid = ModId::invalid();
    static_assert(kZero.toInt() == 0);
    static_assert(kInvalid.toInt() == -1);
    static_assert(kInvalid < kZero);
    static_assert(kZero != kInvalid);
    static_assert(kZero == ModId::zero());
    EXPECT_EQ(kZero.toString(), "0");
    EXPECT_EQ(kInvalid.toString(), "-1");
    EXPECT_LT(kInvalid, ModId{});
    EXPECT_LT(kZero, ModId{});
}

TEST(ModId, CopiesCompareEqualAndOrderByValue)
{
    const ModId a;
    const ModId b;
    const ModId aCopy = a;
    EXPECT_EQ(a, aCopy);
    EXPECT_NE(a, b);
    EXPECT_LT(a, b);
    EXPECT_LE(a, aCopy);
    EXPECT_GE(b, a);
    EXPECT_EQ(a <=> aCopy, std::strong_ordering::equal);
    EXPECT_EQ(a.toString(), std::to_string(a.toInt()));
}

TEST(ModId, DrawsAreUniqueAcrossThreads)
{
    constexpr int kThreads        = 8;
    constexpr int kDrawsPerThread = 2000;

    std::vector<ModId> all;
    std::mutex         mutex;
    {
        std::vector<std::jthread> threads;
        threads.reserve(kThreads);
        for (int t = 0; t < kThreads; ++t)
        {
            threads.emplace_back([&] {
                std::vector<ModId> mine;
                mine.reserve(kDrawsPerThread);
                for (int i = 0; i < kDrawsPerThread; ++i)
                {
                    mine.emplace_back();
                }
                const std::scoped_lock lock{mutex};
                all.insert(all.end(), mine.begin(), mine.end());
            });
        }
    }
    ASSERT_EQ(all.size(), static_cast<std::size_t>(kThreads * kDrawsPerThread));
    std::ranges::sort(all);
    EXPECT_EQ(std::ranges::adjacent_find(all), all.end());
    EXPECT_GT(all.front().toInt(), 0);
}

}  // namespace
