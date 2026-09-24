#include "QtRocket/rocket/FlightConfigurationId.h"

#include <compare>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_set>

#include <gtest/gtest.h>

#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"

namespace
{

using QtRocket::FlightConfigurationId;
using QtRocket::Uuid;

// The literal keys of OpenRocket's DEFAULT_VALUE_FCID and ERROR_FCID: new UUID(0xF4F2F1F0, 5676)
// and new UUID(0xF4F2F1F0, 2489), the int literal sign-extended to a long.
TEST(FlightConfigurationId, SpecialIdsHaveOpenRocketsKeys)
{
    EXPECT_EQ(FlightConfigurationId::defaultValueId().toString(),
              "ffffffff-f4f2-f1f0-0000-00000000162c");
    EXPECT_EQ(FlightConfigurationId::errorId().toString(), "ffffffff-f4f2-f1f0-0000-0000000009b9");
    EXPECT_EQ(FlightConfigurationId::defaultValueId().key(),
              Uuid::fromSigned(static_cast<std::int32_t>(0xF4F2F1F0U), 5676));
}

TEST(FlightConfigurationId, SpecialIdsAreRecognised)
{
    const FlightConfigurationId defaultId = FlightConfigurationId::defaultValueId();
    const FlightConfigurationId errorId   = FlightConfigurationId::errorId();
    EXPECT_TRUE(defaultId.isDefaultId());
    EXPECT_FALSE(defaultId.hasError());
    EXPECT_TRUE(defaultId.isValid());
    EXPECT_TRUE(errorId.hasError());
    EXPECT_FALSE(errorId.isValid());
    EXPECT_FALSE(errorId.isDefaultId());

    const FlightConfigurationId random;
    EXPECT_FALSE(random.isDefaultId());
    EXPECT_FALSE(random.hasError());
    EXPECT_TRUE(random.isValid());
}

TEST(FlightConfigurationId, ShortKeys)
{
    EXPECT_EQ(FlightConfigurationId::defaultValueId().toShortKey(), "DefaultKey");
    EXPECT_EQ(FlightConfigurationId::errorId().toShortKey(), "ErrorKey");
    const FlightConfigurationId id =
        FlightConfigurationId::fromString("123e4567-e89b-12d3-a456-426614174000");
    EXPECT_EQ(id.toShortKey(), "123e4567");
    EXPECT_EQ(id.toDebug(), "123e4567");
    EXPECT_EQ(id.toFullKey(), "123e4567-e89b-12d3-a456-426614174000");
    EXPECT_EQ(id.toString(), "123e4567-e89b-12d3-a456-426614174000");
}

TEST(FlightConfigurationId, DefaultConstructionIsRandom)
{
    const FlightConfigurationId a;
    const FlightConfigurationId b;
    EXPECT_NE(a, b);
    EXPECT_EQ(a.key().version(), 4);
}

TEST(FlightConfigurationId, FromString)
{
    // A canonical UUID is parsed.
    EXPECT_EQ(FlightConfigurationId::fromString("123e4567-e89b-12d3-a456-426614174000").key(),
              Uuid(0x123e4567e89b12d3ULL, 0xa456426614174000ULL));
    // Anything else is new UUID(0, text.hashCode()), sign-extended.
    const FlightConfigurationId hashed = FlightConfigurationId::fromString("config one");
    EXPECT_EQ(hashed.key(), Uuid::fromSigned(0, QtRocket::Strings::javaHashCode("config one")));
    // "a".hashCode() = 97.
    EXPECT_EQ(FlightConfigurationId::fromString("a").toString(),
              "00000000-0000-0000-0000-000000000061");
    // A negative hash fills the low half with ones: "zzzzzzzz".hashCode() < 0.
    ASSERT_LT(QtRocket::Strings::javaHashCode("zzzzzzzz"), 0);
    EXPECT_EQ(FlightConfigurationId::fromString("zzzzzzzz").key().leastSignificantBits() >> 32U,
              0xFFFFFFFFU);
    // An empty text gives a random id.
    EXPECT_NE(FlightConfigurationId::fromString(""), FlightConfigurationId::fromString(""));
}

TEST(FlightConfigurationId, EqualityAndOrderFollowTheKey)
{
    const Uuid                  key{1U, 2U};
    const FlightConfigurationId a{key};
    const FlightConfigurationId b{key};
    const FlightConfigurationId c{Uuid{1U, 3U}};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_LT(a, c);
    // Java's UUID.compareTo compares signed halves: a negative most significant half sorts
    // first.
    const FlightConfigurationId negative{Uuid::fromSigned(-1, 0)};
    EXPECT_LT(negative, a);
    EXPECT_EQ(a <=> b, std::strong_ordering::equal);
}

TEST(FlightConfigurationId, HashesLikeJava)
{
    EXPECT_EQ(FlightConfigurationId::defaultValueId().hashCode(), 185407523);
    EXPECT_EQ(FlightConfigurationId::errorId().hashCode(), 185403318);

    std::unordered_set<FlightConfigurationId> ids;
    ids.insert(FlightConfigurationId::defaultValueId());
    ids.insert(FlightConfigurationId::defaultValueId());
    ids.insert(FlightConfigurationId::errorId());
    EXPECT_EQ(ids.size(), 2U);
    EXPECT_EQ(std::hash<FlightConfigurationId>{}(FlightConfigurationId::errorId()),
              std::hash<Uuid>{}(FlightConfigurationId::kErrorUuid));
}

}  // namespace
