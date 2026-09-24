#include "QtRocket/util/LineStyle.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using QtRocket::dashes;
using QtRocket::displayKey;
using QtRocket::kAllLineStyles;
using QtRocket::LineStyle;
using QtRocket::lineStyleFromString;
using QtRocket::lineStyleName;
using QtRocket::toString;

std::vector<double> dashList(LineStyle style)
{
    const auto pattern = dashes(style);
    return {pattern.begin(), pattern.end()};
}

TEST(LineStyle, DashPatternsMatchOpenRocket)
{
    EXPECT_EQ(dashList(LineStyle::SOLID), (std::vector<double>{10.0, 0.0}));
    EXPECT_EQ(dashList(LineStyle::DASHED), (std::vector<double>{6.0, 4.0}));
    EXPECT_EQ(dashList(LineStyle::DOTTED), (std::vector<double>{2.0, 3.0}));
    EXPECT_EQ(dashList(LineStyle::DASHDOT), (std::vector<double>{8.0, 3.0, 2.0, 3.0}));
}

TEST(LineStyle, DisplayKeys)
{
    EXPECT_EQ(displayKey(LineStyle::SOLID), "LineStyle.Solid");
    EXPECT_EQ(displayKey(LineStyle::DASHED), "LineStyle.Dashed");
    EXPECT_EQ(displayKey(LineStyle::DOTTED), "LineStyle.Dotted");
    EXPECT_EQ(displayKey(LineStyle::DASHDOT), "LineStyle.Dash-dotted");
}

TEST(LineStyle, OrkSpelling)
{
    // RocketComponentSaver writes the enum name in lower case.
    EXPECT_EQ(toString(LineStyle::SOLID), "solid");
    EXPECT_EQ(toString(LineStyle::DASHED), "dashed");
    EXPECT_EQ(toString(LineStyle::DOTTED), "dotted");
    EXPECT_EQ(toString(LineStyle::DASHDOT), "dashdot");
}

TEST(LineStyle, FromStringRoundTrips)
{
    for (const LineStyle style : kAllLineStyles)
    {
        EXPECT_EQ(lineStyleFromString(toString(style)), style);
        EXPECT_EQ(lineStyleFromString(std::string(toString(style)) + " "), style);
    }
}

TEST(LineStyle, NameIsTheEnumConstant)
{
    // ApplicationPreferences.setDefaultLineStyle stores LineStyle.name().
    EXPECT_EQ(lineStyleName(LineStyle::SOLID), "SOLID");
    EXPECT_EQ(lineStyleName(LineStyle::DASHED), "DASHED");
    EXPECT_EQ(lineStyleName(LineStyle::DOTTED), "DOTTED");
    EXPECT_EQ(lineStyleName(LineStyle::DASHDOT), "DASHDOT");
    for (const LineStyle style : kAllLineStyles)
    {
        EXPECT_EQ(lineStyleFromString(lineStyleName(style)), style);
    }
}

TEST(LineStyle, FromStringTrimsAndIgnoresCase)
{
    // DocumentConfig.findEnum trims; the preference store keeps the upper-case enum name.
    EXPECT_EQ(lineStyleFromString(" dashed\n"), LineStyle::DASHED);
    EXPECT_EQ(lineStyleFromString("DASHDOT"), LineStyle::DASHDOT);
    EXPECT_EQ(lineStyleFromString("Dotted"), LineStyle::DOTTED);
    EXPECT_EQ(lineStyleFromString("\tSOLID "), LineStyle::SOLID);
}

TEST(LineStyle, FromStringRejectsUnknownNames)
{
    EXPECT_EQ(lineStyleFromString(""), std::nullopt);
    EXPECT_EQ(lineStyleFromString("   "), std::nullopt);
    EXPECT_EQ(lineStyleFromString("dash-dot"), std::nullopt);
    EXPECT_EQ(lineStyleFromString("dash_dot"), std::nullopt);
    EXPECT_EQ(lineStyleFromString("dash"), std::nullopt);
    EXPECT_EQ(lineStyleFromString("solids"), std::nullopt);
    EXPECT_EQ(lineStyleFromString("LineStyle.Solid"), std::nullopt);
    EXPECT_EQ(lineStyleFromString("0"), std::nullopt);
}

TEST(LineStyle, AllStylesInDeclarationOrder)
{
    ASSERT_EQ(kAllLineStyles.size(), 4U);
    EXPECT_EQ(kAllLineStyles[0], LineStyle::SOLID);
    EXPECT_EQ(kAllLineStyles[1], LineStyle::DASHED);
    EXPECT_EQ(kAllLineStyles[2], LineStyle::DOTTED);
    EXPECT_EQ(kAllLineStyles[3], LineStyle::DASHDOT);
}

}  // namespace
