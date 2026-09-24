#include "QtRocket/models/GravityModelType.h"

#include <optional>

#include <gtest/gtest.h>

namespace
{

using QtRocket::GravityModelType;
using QtRocket::gravityModelTypeFromString;
using QtRocket::gravityModelTypeName;
using QtRocket::kAllGravityModelTypes;
using QtRocket::orkName;
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

TEST(GravityModelType, OrkNamesAndTooltipKeys)
{
    EXPECT_EQ(orkName(GravityModelType::WGS), "wgs");
    EXPECT_EQ(orkName(GravityModelType::CONSTANT), "constant");
    EXPECT_EQ(tooltipKey(GravityModelType::WGS), "simedtdlg.GravityModel.WGS84.ttip");
    EXPECT_EQ(tooltipKey(GravityModelType::CONSTANT), "simedtdlg.GravityModel.Constant.ttip");
}

TEST(GravityModelType, SpellingsParseBack)
{
    for (const GravityModelType type : kAllGravityModelTypes)
    {
        EXPECT_EQ(gravityModelTypeFromString(toStringValue(type)), type);
        EXPECT_EQ(gravityModelTypeFromString(orkName(type)), type);
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
