#include "QtRocket/util/Color.h"

#include <functional>
#include <optional>
#include <unordered_set>

#include <gtest/gtest.h>

namespace
{

using QtRocket::Color;

// ------------------------------------------------------------------ ORColorTest

TEST(Color, SettersMutateChannels)
{
    Color color(10, 20, 30);
    color.setRed(40);
    color.setGreen(50);
    color.setBlue(60);
    color.setAlpha(70);

    EXPECT_EQ(40, color.red());
    EXPECT_EQ(50, color.green());
    EXPECT_EQ(60, color.blue());
    EXPECT_EQ(70, color.alpha());
}

TEST(Color, XmlAttributesRoundTripPreservesChannels)
{
    // ORColorTest goes through java.awt.Color here; the .ork attributes are the conversion kept.
    const Color color(1, 2, 3, 4);
    EXPECT_EQ(color.toXmlAttributes(), "red=\"1\" green=\"2\" blue=\"3\" alpha=\"4\"");

    const std::optional<Color> converted = Color::fromXmlAttributes("1", "2", "3", "4");
    EXPECT_EQ(converted, color);
}

TEST(Color, EqualityComparesChannels)
{
    // ORColorTest also asserts that a Color differs from the String "color"; a typed operator==
    // cannot be handed a string, so that case has no analogue here.
    const Color first(10, 20, 30, 40);
    const Color same(10, 20, 30, 40);
    const Color different(10, 21, 30, 40);

    EXPECT_EQ(first, first);
    EXPECT_EQ(first, same);
    EXPECT_NE(first, different);
    EXPECT_NE(first, Color(10, 20, 30));
    EXPECT_NE(Color(1, 20, 30, 40), first);
    EXPECT_NE(Color(10, 20, 3, 40), first);
}

// ---------------------------------------------------- behaviour OpenRocket did not test

TEST(Color, ThreeChannelConstructorIsOpaque)
{
    constexpr Color kColor(10, 20, 30);
    static_assert(kColor.red() == 10);
    static_assert(kColor.green() == 20);
    static_assert(kColor.blue() == 30);
    static_assert(kColor.alpha() == Color::kOpaque);
    EXPECT_EQ(kColor.alpha(), 255);
    EXPECT_EQ(kColor, Color(10, 20, 30, 255));
}

TEST(Color, Constants)
{
    EXPECT_EQ(Color::black(), Color(0, 0, 0, 255));
    EXPECT_EQ(Color::invisible(), Color(1, 1, 1, 0));
    EXPECT_EQ(Color::darkRed(), Color(200, 0, 0, 255));
}

TEST(Color, ToString)
{
    EXPECT_EQ(Color(200, 0, 0).toString(), "Color [r=200, g=0, b=0, a=255]");
    EXPECT_EQ(Color(1, 2, 3, 4).toString(), "Color [r=1, g=2, b=3, a=4]");
}

TEST(Color, FromXmlAttributesDefaultsAlphaToOpaque)
{
    EXPECT_EQ(Color::fromXmlAttributes("200", "0", "0", std::nullopt), Color(200, 0, 0, 255));
    EXPECT_EQ(Color::fromXmlAttributes("0", "0", "0", "0"), Color(0, 0, 0, 0));
    EXPECT_EQ(Color::fromXmlAttributes("255", "255", "255", "255"), Color(255, 255, 255, 255));
    EXPECT_EQ(Color::fromXmlAttributes("+7", "007", "0", std::nullopt), Color(7, 7, 0, 255));
}

TEST(Color, FromXmlAttributesRejectsMissingChannels)
{
    EXPECT_EQ(Color::fromXmlAttributes(std::nullopt, "0", "0", "0"), std::nullopt);
    EXPECT_EQ(Color::fromXmlAttributes("0", std::nullopt, "0", "0"), std::nullopt);
    EXPECT_EQ(Color::fromXmlAttributes("0", "0", std::nullopt, "0"), std::nullopt);
    EXPECT_EQ(Color::fromXmlAttributes(std::nullopt, std::nullopt, std::nullopt, std::nullopt),
              std::nullopt);
}

TEST(Color, FromXmlAttributesRejectsBadValues)
{
    EXPECT_EQ(Color::fromXmlAttributes("", "0", "0", std::nullopt), std::nullopt);
    EXPECT_EQ(Color::fromXmlAttributes("red", "0", "0", std::nullopt), std::nullopt);
    EXPECT_EQ(Color::fromXmlAttributes("1.0", "0", "0", std::nullopt), std::nullopt);
    EXPECT_EQ(Color::fromXmlAttributes(" 1", "0", "0", std::nullopt), std::nullopt);
    EXPECT_EQ(Color::fromXmlAttributes("0", "0", "0", "x"), std::nullopt);
    EXPECT_EQ(Color::fromXmlAttributes("0", "0", "0", ""), std::nullopt);
}

TEST(Color, FromXmlAttributesRejectsOutOfRange)
{
    EXPECT_EQ(Color::fromXmlAttributes("256", "0", "0", std::nullopt), std::nullopt);
    EXPECT_EQ(Color::fromXmlAttributes("0", "256", "0", std::nullopt), std::nullopt);
    EXPECT_EQ(Color::fromXmlAttributes("0", "0", "256", std::nullopt), std::nullopt);
    EXPECT_EQ(Color::fromXmlAttributes("0", "0", "0", "256"), std::nullopt);
    EXPECT_EQ(Color::fromXmlAttributes("-1", "0", "0", std::nullopt), std::nullopt);
    EXPECT_EQ(Color::fromXmlAttributes("0", "-1", "0", std::nullopt), std::nullopt);
    EXPECT_EQ(Color::fromXmlAttributes("0", "0", "-1", std::nullopt), std::nullopt);
    EXPECT_EQ(Color::fromXmlAttributes("0", "0", "0", "-1"), std::nullopt);
    EXPECT_EQ(Color::fromXmlAttributes("99999999999", "0", "0", std::nullopt), std::nullopt);
}

TEST(Color, HashFollowsEquality)
{
    const std::hash<Color> hash;
    EXPECT_EQ(hash(Color(10, 20, 30, 40)), hash(Color(10, 20, 30, 40)));
    EXPECT_NE(hash(Color(10, 20, 30, 40)), hash(Color(10, 21, 30, 40)));
    EXPECT_NE(hash(Color(10, 20, 30, 40)), hash(Color(40, 30, 20, 10)));
    // Objects.hash(0, 0, 0, 255): 31^4 + 255.
    EXPECT_EQ(hash(Color::black()), 923521U + 255U);

    std::unordered_set<Color> colors;
    colors.insert(Color::black());
    colors.insert(Color(0, 0, 0));
    colors.insert(Color::darkRed());
    EXPECT_EQ(colors.size(), 2U);
    EXPECT_TRUE(colors.contains(Color(200, 0, 0, 255)));
}

}  // namespace
