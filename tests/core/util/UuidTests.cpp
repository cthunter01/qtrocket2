#include "QtRocket/util/Uuid.h"

#include <algorithm>
#include <compare>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Error.h"

namespace
{

using QtRocket::ErrorCode;
using QtRocket::Uuid;

constexpr std::string_view kSample = "123e4567-e89b-12d3-a456-426614174000";

TEST(Uuid, RoundTripsThroughText)
{
    const auto parsed = Uuid::parse(kSample);
    ASSERT_TRUE(parsed.has_value()) << parsed.error().toString();
    EXPECT_EQ(parsed->toString(), kSample);
    EXPECT_EQ(parsed->mostSignificantBits(), 0x123e4567e89b12d3ULL);
    EXPECT_EQ(parsed->leastSignificantBits(), 0xa456426614174000ULL);
    EXPECT_FALSE(parsed->isNil());

    const auto again = Uuid::parse(parsed->toString());
    ASSERT_TRUE(again.has_value());
    EXPECT_EQ(*again, *parsed);
}

TEST(Uuid, ParsesEitherCaseAndPrintsLowercase)
{
    const auto upper = Uuid::parse("123E4567-E89B-12D3-A456-426614174000");
    const auto mixed = Uuid::parse("123e4567-E89b-12D3-a456-426614174000");
    ASSERT_TRUE(upper.has_value());
    ASSERT_TRUE(mixed.has_value());
    EXPECT_EQ(upper->toString(), kSample);
    EXPECT_EQ(*upper, *mixed);
}

TEST(Uuid, PrintsLikeJavaUtilUuid)
{
    // java.util.UUID.toString() zero-pads every group.
    EXPECT_EQ(Uuid(0x0000000000000001ULL, 0x0000000000000002ULL).toString(),
              "00000000-0000-0001-0000-000000000002");
    EXPECT_EQ(Uuid(0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL).toString(),
              "ffffffff-ffff-ffff-ffff-ffffffffffff");
    // FlightConfigurationId.ERROR_UUID: new UUID(0xF4F2F1F0L, 2489).
    EXPECT_EQ(Uuid(0xF4F2F1F0ULL, 2489).toString(), "00000000-f4f2-f1f0-0000-0000000009b9");
    // FlightConfigurationId.DEFAULT_VALUE_UUID: new UUID(0xF4F2F1F0L, 5676).
    EXPECT_EQ(Uuid(0xF4F2F1F0ULL, 5676).toString(), "00000000-f4f2-f1f0-0000-00000000162c");
}

TEST(Uuid, FromSignedWidensLikeJavaLongs)
{
    // java.util.UUID(long, long) takes signed longs; FlightConfigurationId(String) relies on it
    // with `new UUID(0, _str.hashCode())`, where the int hash is sign-extended.
    EXPECT_EQ(Uuid::fromSigned(0, -1).toString(), "00000000-0000-0000-ffff-ffffffffffff");
    EXPECT_EQ(Uuid::fromSigned(-1, 0).toString(), "ffffffff-ffff-ffff-0000-000000000000");
    // "abc".hashCode() is 96354; "The quick brown fox jumps over the lazy dog".hashCode() is
    // -609428141, a negative int.
    constexpr std::int32_t kPositiveHash = 96354;
    constexpr std::int32_t kNegativeHash = -609428141;
    EXPECT_EQ(Uuid::fromSigned(0, kPositiveHash), Uuid(0, 96354));
    EXPECT_EQ(Uuid::fromSigned(0, kPositiveHash).toString(),
              "00000000-0000-0000-0000-000000017862");
    EXPECT_EQ(Uuid::fromSigned(0, kNegativeHash).toString(),
              "00000000-0000-0000-ffff-ffffdbacdd53");
    EXPECT_EQ(Uuid::fromSigned(0, kNegativeHash).leastSignificantBits(), 0xFFFFFFFFDBACDD53ULL);
    // Not what a careless zero-extension would give.
    EXPECT_NE(Uuid::fromSigned(0, kNegativeHash),
              Uuid(0, static_cast<std::uint32_t>(kNegativeHash)));
    // Non-negative values are the same either way, and the result is a constant expression.
    static_assert(Uuid::fromSigned(5, 7) == Uuid(5, 7));
    static_assert(Uuid::fromSigned(0, -1).leastSignificantBits() == 0xFFFFFFFFFFFFFFFFULL);
}

TEST(Uuid, NilIsAllZero)
{
    constexpr Uuid kNil = Uuid::nil();
    static_assert(kNil.isNil());
    static_assert(kNil == Uuid{});
    static_assert(kNil.mostSignificantBits() == 0 && kNil.leastSignificantBits() == 0);
    EXPECT_EQ(kNil.toString(), "00000000-0000-0000-0000-000000000000");
    EXPECT_EQ(kNil.version(), 0);
    EXPECT_EQ(kNil.variant(), 0);
    EXPECT_FALSE(Uuid(0, 1).isNil());
    EXPECT_FALSE(Uuid(1, 0).isNil());
}

TEST(Uuid, ParsesNil)
{
    const auto parsed = Uuid::parse("00000000-0000-0000-0000-000000000000");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_TRUE(parsed->isNil());
    EXPECT_EQ(*parsed, Uuid::nil());
}

TEST(Uuid, RejectsAnythingButTheCanonicalForm)
{
    const std::vector<std::string_view> bad = {
        "",
        "not a uuid",
        "123e4567-e89b-12d3-a456-42661417400",     // 35 characters
        "123e4567-e89b-12d3-a456-4266141740000",   // 37 characters
        "123e4567e89b12d3a456426614174000",        // no dashes
        "123e4567-e89b-12d3-a456_426614174000",    // wrong separator
        "123e4567-e89b-12d3-a45-6426614174000",    // dash in the wrong place
        "123e4567-e89b-12d3-a456-42661417400g",    // not a hexadecimal digit
        "123e4567-e89b-12d3-a456-42661417400 ",    // trailing space
        " 23e4567-e89b-12d3-a456-426614174000",    // leading space
        "{123e4567-e89b-12d3-a456-426614174000}",  // braces
        "1-2-3-4-5",                               // java.util.UUID.fromString() takes this
        "urn:uuid:123e4567-e89b-12d3-a456-426614174000",
    };
    for (const std::string_view text : bad)
    {
        const auto parsed = Uuid::parse(text);
        ASSERT_FALSE(parsed.has_value()) << "'" << text << "' parsed";
        EXPECT_EQ(parsed.error().code, ErrorCode::PARSE) << text;
        EXPECT_NE(parsed.error().message.find("UUID"), std::string::npos) << text;
    }
}

/// The RFC 4122 version 4 layout, in the bits and in the text.
void expectRandomLayout(const Uuid& id)
{
    EXPECT_EQ(id.version(), 4);
    EXPECT_EQ(id.variant(), 2);
    const std::string text = id.toString();
    ASSERT_EQ(text.size(), 36U);
    EXPECT_EQ(text[14], '4');                                                    // version
    EXPECT_NE(std::string_view("89ab").find(text[19]), std::string_view::npos);  // variant
}

TEST(Uuid, RandomIsVersion4Variant2)
{
    const Uuid a = Uuid::random();
    const Uuid b = Uuid::random();
    EXPECT_NE(a, b);
    EXPECT_FALSE(a.isNil());
    expectRandomLayout(a);
    expectRandomLayout(b);
    EXPECT_EQ(Uuid::parse(a.toString()).value_or(Uuid::nil()), a);
}

TEST(Uuid, RandomValuesAreDistinct)
{
    std::unordered_set<Uuid> seen;
    for (int i = 0; i < 1000; ++i)
    {
        EXPECT_TRUE(seen.insert(Uuid::random()).second);
    }
    EXPECT_EQ(seen.size(), 1000U);
}

TEST(Uuid, VersionAndVariantFields)
{
    // Version 1 time-based value, RFC 4122 variant.
    EXPECT_EQ(Uuid(0x123e4567e89b12d3ULL, 0xa456426614174000ULL).version(), 1);
    EXPECT_EQ(Uuid(0x123e4567e89b12d3ULL, 0xa456426614174000ULL).variant(), 2);
    // Top bits of the least significant half: 0b0 NCS, 0b10 RFC 4122, 0b110 Microsoft, 0b111
    // reserved, as java.util.UUID.variant() reports them.
    EXPECT_EQ(Uuid(0, 0x7FFFFFFFFFFFFFFFULL).variant(), 0);
    EXPECT_EQ(Uuid(0, 0x8000000000000000ULL).variant(), 2);
    EXPECT_EQ(Uuid(0, 0xBFFFFFFFFFFFFFFFULL).variant(), 2);
    EXPECT_EQ(Uuid(0, 0xC000000000000000ULL).variant(), 6);
    EXPECT_EQ(Uuid(0, 0xE000000000000000ULL).variant(), 7);
}

TEST(Uuid, OrdersLikeJavaUtilUuid)
{
    // java.util.UUID.compareTo() compares the halves as signed longs, so a value with the top
    // bit set sorts first.
    const Uuid negativeMost(0x8000000000000000ULL, 0);
    const Uuid one(1, 0);
    const Uuid two(2, 0);
    const Uuid negativeLeast(1, 0xFFFFFFFFFFFFFFFFULL);
    EXPECT_LT(negativeMost, one);
    EXPECT_LT(one, two);
    EXPECT_LT(negativeLeast, one);  // same most significant half, least half -1 < 0
    EXPECT_GT(two, one);
    EXPECT_LE(one, one);
    EXPECT_EQ(one <=> Uuid(1, 0), std::strong_ordering::equal);
    EXPECT_NE(one, two);

    std::vector<Uuid> ids = {two, one, negativeLeast, negativeMost};
    std::ranges::sort(ids);
    EXPECT_EQ(ids, (std::vector<Uuid>{negativeMost, negativeLeast, one, two}));
}

TEST(Uuid, JavaFromStringTakesWhatJavaUtilUuidTakes)
{
    // The canonical form, in either case.
    EXPECT_EQ(Uuid::javaFromString(kSample).value_or(Uuid::nil()), *Uuid::parse(kSample));
    EXPECT_EQ(Uuid::javaFromString("123E4567-E89B-12D3-A456-426614174000").value_or(Uuid::nil()),
              *Uuid::parse(kSample));
    // Shortened groups: UUID.fromString("1-2-3-4-5").toString().
    EXPECT_EQ(Uuid::javaFromString("1-2-3-4-5").value_or(Uuid::nil()).toString(),
              "00000001-0002-0003-0004-000000000005");
    EXPECT_EQ(Uuid::javaFromString("0-0-0-0-0").value_or(Uuid{1, 1}), Uuid::nil());
    // A '+' sign is Long.parseLong's.
    EXPECT_EQ(Uuid::javaFromString("+a-b-c-d-e").value_or(Uuid::nil()).toString(),
              "0000000a-000b-000c-000d-00000000000e");
    // Longer groups keep their low 32, 16, 16, 16 and 48 bits.
    EXPECT_EQ(
        Uuid::javaFromString("123456789-12345-0-0-1234567890123").value_or(Uuid::nil()).toString(),
        "23456789-2345-0000-0000-234567890123");
    // Up to Long.MAX_VALUE per group.
    EXPECT_EQ(Uuid::javaFromString("7fffffffffffffff-0-0-0-0").value_or(Uuid::nil()).toString(),
              "ffffffff-0000-0000-0000-000000000000");
}

TEST(Uuid, JavaFromStringRejectsWhatJavaUtilUuidRejects)
{
    const std::vector<std::string_view> bad = {
        "",
        "not a uuid",
        "1-2-3-4",                                // four groups
        "1-2-3-4-5-6",                            // six groups
        "1--3-4-5",                               // an empty group
        "1-2-3-4-",                               // an empty last group
        "+-2-3-4-5",                              // a lone sign
        "1-2-3-4-5g",                             // not a hexadecimal digit
        " 1-2-3-4-5",                             // whitespace
        "8000000000000000-0-0-0-0",               // beyond Long.MAX_VALUE
        "123e4567-e89b-12d3-a456-4266141740000",  // 37 characters
        "{123e4567-e89b-12d3-a456-426614174000}",
    };
    for (const std::string_view text : bad)
    {
        const auto parsed = Uuid::javaFromString(text);
        ASSERT_FALSE(parsed.has_value()) << "'" << text << "' parsed";
        EXPECT_EQ(parsed.error().code, ErrorCode::PARSE) << text;
        EXPECT_NE(parsed.error().message.find("UUID"), std::string::npos) << text;
    }
}

TEST(Uuid, HashesConsistentlyWithEquality)
{
    const std::hash<Uuid> hash;
    EXPECT_EQ(hash(Uuid(1, 2)), hash(Uuid(1, 2)));
    EXPECT_NE(hash(Uuid(1, 2)), hash(Uuid(2, 1)));
    EXPECT_NE(hash(Uuid(0xF4F2F1F0ULL, 2489)), hash(Uuid(0xF4F2F1F0ULL, 5676)));

    std::unordered_set<Uuid> set;
    set.insert(Uuid(1, 2));
    set.insert(Uuid(1, 2));
    set.insert(*Uuid::parse(kSample));
    set.insert(Uuid::nil());
    EXPECT_EQ(set.size(), 3U);
    EXPECT_TRUE(set.contains(Uuid(1, 2)));
    EXPECT_TRUE(set.contains(*Uuid::parse(kSample)));
    EXPECT_FALSE(set.contains(Uuid(2, 1)));
}

}  // namespace

namespace
{

// java.util.UUID.hashCode(): (int) (hilo >> 32) ^ (int) hilo with hilo = most ^ least; the
// expected values were computed with that formula.
TEST(Uuid, HashCodeMatchesJavaUtilUuid)
{
    EXPECT_EQ(Uuid{}.hashCode(), 0);
    EXPECT_EQ((Uuid{0U, 1U}.hashCode()), 1);
    EXPECT_EQ((Uuid{std::uint64_t{1} << 32U, 0U}.hashCode()), 1);
    EXPECT_EQ(Uuid::fromSigned(0, -1).hashCode(), 0);
    EXPECT_EQ((Uuid{0x123e4567e89b12d3ULL, 0xa456426614174000ULL}.hashCode()), 1256478162);
    // FlightConfigurationId's default and error keys.
    EXPECT_EQ((Uuid{0xFFFFFFFFF4F2F1F0ULL, 5676U}.hashCode()), 185407523);
    EXPECT_EQ((Uuid{0xFFFFFFFFF4F2F1F0ULL, 2489U}.hashCode()), 185403318);
}

}  // namespace
