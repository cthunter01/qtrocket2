#include "QtRocket/motor/MotorConfigurationId.h"

#include <functional>
#include <string_view>
#include <unordered_set>

#include <gtest/gtest.h>

#include "QtRocket/util/Uuid.h"

namespace
{

using QtRocket::MotorConfigurationId;
using QtRocket::Uuid;

Uuid uuid(std::string_view text)
{
    return Uuid::parse(text).value();
}

/// 123e4567-e89b-12d3-a456-426614174000
constexpr Uuid kMount{0x123e4567e89b12d3ULL, 0xa456426614174000ULL};
/// 6ba7b810-9dad-11d1-80b4-00c04fd430c8
constexpr Uuid kFlightConfiguration{0x6ba7b8109dad11d1ULL, 0x80b400c04fd430c8ULL};

// ---- Keys pinned by running OpenRocket's MotorConfigurationId on the same ids ----

TEST(MotorConfigurationId, KeyMatchesOpenRocket)
{
    ASSERT_EQ(uuid("123e4567-e89b-12d3-a456-426614174000"), kMount);
    ASSERT_EQ(uuid("6ba7b810-9dad-11d1-80b4-00c04fd430c8"), kFlightConfiguration);

    const MotorConfigurationId id(kMount, kFlightConfiguration);
    EXPECT_EQ(id.toString(), "4ae455d2-0000-0000-6ba7-b8109dad11d1");
    EXPECT_EQ(id.toShortKey(), "4ae4/d11d");
    EXPECT_EQ(id.toDebug(), "4ae4/d11d");
    EXPECT_EQ(id.hashCode(), -1125188589);
    // The mount's UUID.hashCode() lands in the upper 32 bits.
    EXPECT_EQ(kMount.javaHashCode(), 1256478162);
    EXPECT_EQ(id.getKey().mostSignificantBits(), 0x4ae455d200000000ULL);
    EXPECT_EQ(id.getKey().leastSignificantBits(), kFlightConfiguration.mostSignificantBits());
}

TEST(MotorConfigurationId, MoreKeysMatchOpenRocket)
{
    const MotorConfigurationId second(uuid("00000000-0000-0001-0000-000000000000"),
                                      kFlightConfiguration);
    EXPECT_EQ(second.toString(), "00000001-0000-0000-6ba7-b8109dad11d1");
    EXPECT_EQ(second.toShortKey(), "0000/d11d");
    EXPECT_EQ(second.hashCode(), -167073344);

    // The roles swapped: a different key.
    const MotorConfigurationId third(kFlightConfiguration, kMount);
    EXPECT_EQ(third.toString(), "396a99c9-0000-0000-123e-4567e89b12d3");
    EXPECT_EQ(third.toShortKey(), "396a/b12d");
    EXPECT_EQ(third.hashCode(), -1009791363);

    // A negative hash: the int's sign extension is shifted out of the key.
    const MotorConfigurationId fourth(uuid("ffffffff-ffff-ffff-0000-000000000001"),
                                      uuid("80000000-0000-0000-ffff-ffffffffffff"));
    EXPECT_EQ(fourth.toString(), "00000001-0000-0000-8000-000000000000");
    EXPECT_EQ(fourth.toShortKey(), "0000/0000");
    EXPECT_EQ(fourth.hashCode(), -2147483647);
}

TEST(MotorConfigurationId, OnlyTheFlightConfigurationsUpperHalfCounts)
{
    // Two configuration ids that differ only in their lower 64 bits give the same key.
    const MotorConfigurationId a(kMount, Uuid(0x1122334455667788ULL, 1));
    const MotorConfigurationId b(kMount, Uuid(0x1122334455667788ULL, 2));
    EXPECT_EQ(a, b);
    EXPECT_EQ(a.hashCode(), b.hashCode());
    EXPECT_EQ(std::hash<MotorConfigurationId>{}(a), std::hash<MotorConfigurationId>{}(b));

    const MotorConfigurationId c(kMount, Uuid(0x1122334455667789ULL, 1));
    EXPECT_NE(a, c);
}

TEST(MotorConfigurationId, EqualityAndHashing)
{
    const MotorConfigurationId a(kMount, kFlightConfiguration);
    const MotorConfigurationId same(kMount, kFlightConfiguration);
    const MotorConfigurationId other(kFlightConfiguration, kMount);
    EXPECT_EQ(a, same);
    EXPECT_NE(a, other);

    const std::unordered_set<MotorConfigurationId> ids{a, same, other};
    EXPECT_EQ(ids.size(), 2U);
}

TEST(MotorConfigurationId, NilIds)
{
    const MotorConfigurationId id(Uuid::nil(), Uuid::nil());
    EXPECT_TRUE(id.getKey().isNil());
    EXPECT_EQ(id.toString(), "00000000-0000-0000-0000-000000000000");
    EXPECT_EQ(id.toShortKey(), "0000/0000");
    EXPECT_EQ(id.hashCode(), 0);
}

}  // namespace
