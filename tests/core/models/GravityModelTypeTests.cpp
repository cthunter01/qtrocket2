#include "QtRocket/models/GravityModelType.h"

#include <optional>

#include <gtest/gtest.h>

namespace
{

using QtRocket::displayKey;
using QtRocket::displayName;
using QtRocket::GravityModelType;
using QtRocket::gravityModelTypeFromString;
using QtRocket::gravityModelTypeName;
using QtRocket::kAllGravityModelTypes;
using QtRocket::tooltip;
using QtRocket::tooltipKey;
using QtRocket::toStringValue;

// ---- Ported from GravityModelTypeTest.java ----

TEST(GravityModelType, ToStringValue)
{
    EXPECT_EQ(toStringValue(GravityModelType::WGS), "WGS");
    EXPECT_EQ(toStringValue(GravityModelType::CONSTANT), "Constant");
}

TEST(GravityModelType, FromString)
{
    EXPECT_EQ(gravityModelTypeFromString("WGS"), GravityModelType::WGS);
    EXPECT_EQ(gravityModelTypeFromString("wgs"), GravityModelType::WGS);
    EXPECT_EQ(gravityModelTypeFromString("Constant"), GravityModelType::CONSTANT);
    EXPECT_EQ(gravityModelTypeFromString("constant"), GravityModelType::CONSTANT);
}

TEST(GravityModelType, FromStringInvalid)
{
    // Java throws IllegalArgumentException; the port returns nullopt.
    EXPECT_EQ(gravityModelTypeFromString("invalid"), std::nullopt);
}

TEST(GravityModelType, ToString)
{
    // Java's toString() is the string value, toStringValue() here.
    EXPECT_EQ(toStringValue(GravityModelType::WGS), "WGS");
    EXPECT_EQ(toStringValue(GravityModelType::CONSTANT), "Constant");
}

// ---- QtRocket additions ----

TEST(GravityModelType, ValuesInDeclarationOrder)
{
    ASSERT_EQ(kAllGravityModelTypes.size(), 2U);
    EXPECT_EQ(kAllGravityModelTypes[0], GravityModelType::WGS);
    EXPECT_EQ(kAllGravityModelTypes[1], GravityModelType::CONSTANT);
}

TEST(GravityModelType, EnumNames)
{
    EXPECT_EQ(gravityModelTypeName(GravityModelType::WGS), "WGS");
    EXPECT_EQ(gravityModelTypeName(GravityModelType::CONSTANT), "CONSTANT");
}

TEST(GravityModelType, OrkSpellingsAndTooltipKeys)
{
    EXPECT_EQ(toString(GravityModelType::WGS), "wgs");
    EXPECT_EQ(toString(GravityModelType::CONSTANT), "constant");
    EXPECT_EQ(tooltipKey(GravityModelType::WGS), "simedtdlg.GravityModel.WGS84.ttip");
    EXPECT_EQ(tooltipKey(GravityModelType::CONSTANT), "simedtdlg.GravityModel.Constant.ttip");
}

TEST(GravityModelType, TooltipTexts)
{
    // The English texts of the keys of tooltipKey() in OpenRocket's messages.properties.
    EXPECT_EQ(tooltip(GravityModelType::WGS),
              "WGS84 ellipsoid gravity model (varies with latitude and altitude)");
    EXPECT_EQ(tooltip(GravityModelType::CONSTANT),
              "Constant gravity model (same value everywhere)");
}

TEST(GravityModelType, DisplayKeysAndNames)
{
    // OpenRocket shows toString() untranslated; the key is QtRocket's own (no such key in
    // messages.properties), the English name Java's.
    EXPECT_EQ(displayKey(GravityModelType::WGS), "GravityModelType.WGS");
    EXPECT_EQ(displayKey(GravityModelType::CONSTANT), "GravityModelType.CONSTANT");
    EXPECT_EQ(displayName(GravityModelType::WGS), "WGS");
    EXPECT_EQ(displayName(GravityModelType::CONSTANT), "Constant");
    for (const GravityModelType type : kAllGravityModelTypes)
    {
        EXPECT_EQ(displayName(type), toStringValue(type));
    }
}

TEST(GravityModelType, SpellingsParseBack)
{
    for (const GravityModelType type : kAllGravityModelTypes)
    {
        EXPECT_EQ(gravityModelTypeFromString(toStringValue(type)), type);
        EXPECT_EQ(gravityModelTypeFromString(toString(type)), type);
        EXPECT_EQ(gravityModelTypeFromString(gravityModelTypeName(type)), type);
    }
}

TEST(GravityModelType, FromStringIsJavasEqualsIgnoreCase)
{
    EXPECT_EQ(gravityModelTypeFromString("cOnStAnT"), GravityModelType::CONSTANT);
    // No trimming, no partial matches.
    EXPECT_EQ(gravityModelTypeFromString(" WGS"), std::nullopt);
    EXPECT_EQ(gravityModelTypeFromString("WGS84"), std::nullopt);
    EXPECT_EQ(gravityModelTypeFromString(""), std::nullopt);
    // String.equalsIgnoreCase folds the long s (U+017F) to 's'.
    EXPECT_EQ(gravityModelTypeFromString("wg\xC5\xBF"), GravityModelType::WGS);
}

}  // namespace
