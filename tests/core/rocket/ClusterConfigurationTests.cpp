#include "QtRocket/rocket/ClusterConfiguration.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using QtRocket::ClusterConfiguration;

/// One layout as OpenRocket (JDK 17) has it: the xml name, the cluster count, the points
/// (ClusterConfiguration.getPoints()) and the points rotated by 0.3 rad (getPoints(0.3)).
struct Layout
{
    std::string_view    xmlName;
    int                 count;
    std::vector<double> points;
    std::vector<double> rotated;
};

/// One entry of javaLayouts().
[[nodiscard]] Layout layout(std::string_view xmlName, int count, std::vector<double> points,
                            std::vector<double> rotated)
{
    return {.xmlName = xmlName,
            .count   = count,
            .points  = std::move(points),
            .rotated = std::move(rotated)};
}

// NOLINTBEGIN(modernize-use-std-numbers): OpenRocket's values, not approximations of constants
[[nodiscard]] std::vector<Layout> javaLayouts()
{
    return {
        layout("single", 1, {0.0, 0.0}, {0.0, 0.0}),
        layout("double", 2, {-0.5, 0.0, 0.5, 0.0},
               {-0.477668244562803, 0.14776010333066977, 0.477668244562803, -0.14776010333066977}),
        layout("3-row", 3, {-1.0, 0.0, 0.0, 0.0, 1.0, 0.0},
               {-0.955336489125606, 0.29552020666133955, 0.0, 0.0, 0.955336489125606,
                -0.29552020666133955}),
        layout("4-row", 4, {-1.5, 0.0, -0.5, 0.0, 0.5, 0.0, 1.5, 0.0},
               {-1.433004733688409, 0.4432803099920093, -0.477668244562803, 0.14776010333066977,
                0.477668244562803, -0.14776010333066977, 1.433004733688409, -0.4432803099920093}),
        layout("3-ring", 3,
               {-0.5, -0.2886751345948129, 0.5, -0.2886751345948129, 0.0, 0.5773502691896258},
               {-0.5629775799962521, -0.12802178625100055, 0.3923589091293539, -0.42354199291234007,
                0.17061867086689825, 0.5515637791633407}),
        layout("4-ring", 4, {-0.5, 0.5, 0.5, 0.5, 0.5, -0.5, -0.5, -0.5},
               {-0.3299081412321332, 0.6254283478934728, 0.6254283478934728, 0.3299081412321332,
                0.3299081412321332, -0.6254283478934728, -0.6254283478934728, -0.3299081412321332}),
        layout("5-ring", 5,
               {0.0, 0.8506508083520399, 0.8090169943749473, 0.2628655560595668, 0.5000000000000001,
                -0.6881909602355867, -0.4999999999999999, -0.6881909602355868, -0.8090169943749475,
                0.26286555605956663},
               {0.25138450268083035, 0.8126577567228965, 0.8505655384999834, 0.012044188067776462,
                0.2742939097715168, -0.8052140391301147, -0.6810425793540892, -0.5096938324687752,
                -0.6952013715982414, 0.49020592680821684}),
        layout("6-ring", 6,
               {0.0, 1.0, 0.8660254037844386, 0.5, 0.8660254037844386, -0.5, 0.0, -1.0,
                -0.8660254037844386, -0.5, -0.8660254037844386, 0.5},
               {0.29552020666133955, 0.955336489125606, 0.9751057720756806, 0.22174023826245565,
                0.679585565414341, -0.7335962508631504, -0.29552020666133955, -0.955336489125606,
                -0.9751057720756806, -0.22174023826245565, -0.679585565414341, 0.7335962508631504}),
        layout("3-star", 4,
               {0.0, 0.0, 0.0, 1.0, 0.8660254037844386, -0.5, -0.8660254037844386, -0.5},
               {0.0, 0.0, 0.29552020666133955, 0.955336489125606, 0.679585565414341,
                -0.7335962508631504, -0.9751057720756806, -0.22174023826245565}),
        layout("4-star", 5,
               {0.0, 0.0, -0.7071067811865475, 0.7071067811865475, 0.7071067811865475,
                0.7071067811865475, 0.7071067811865475, -0.7071067811865475, -0.7071067811865475,
                -0.7071067811865475},
               {0.0, 0.0, -0.46656056766778126, 0.8844892518835474, 0.8844892518835474,
                0.46656056766778126, 0.46656056766778126, -0.8844892518835474, -0.8844892518835474,
                -0.46656056766778126}),
        layout("5-star", 6,
               {0.0, 0.0, 0.0, 1.0, 0.9510565162951535, 0.30901699437494745, 0.5877852522924732,
                -0.8090169943749473, -0.587785252292473, -0.8090169943749476, -0.9510565162951536,
                0.30901699437494723},
               {0.0, 0.0, 0.29552020666133955, 0.955336489125606, 0.999899759276992,
                0.014158792244152008, 0.3224518299146797, -0.9465858742790716, -0.8006135686551199,
                -0.5991810358191533, -0.8172582271978913, 0.5762716287284666}),
        layout("6-star", 7,
               {0.0, 0.0, 0.0, 1.0, 0.8660254037844386, 0.5, 0.8660254037844386, -0.5, 0.0, -1.0,
                -0.8660254037844386, -0.5, -0.8660254037844386, 0.5},
               {0.0, 0.0, 0.29552020666133955, 0.955336489125606, 0.9751057720756806,
                0.22174023826245565, 0.679585565414341, -0.7335962508631504, -0.29552020666133955,
                -0.955336489125606, -0.9751057720756806, -0.22174023826245565, -0.679585565414341,
                0.7335962508631504}),
        layout("9-grid", 9,
               {-1.4, 1.4, 0.0, 1.4, 1.4, 1.4, -1.4, 0.0, 0.0, 0.0, 1.4, 0.0, -1.4, -1.4, 0.0, -1.4,
                1.4, -1.4},
               {-0.9237427954499728, 1.7511993741017235, 0.41372828932587535, 1.3374710847758482,
                1.7511993741017235, 0.9237427954499728, -1.3374710847758482, 0.41372828932587535,
                0.0, 0.0, 1.3374710847758482, -0.41372828932587535, -1.7511993741017235,
                -0.9237427954499728, -0.41372828932587535, -1.3374710847758482, 0.9237427954499728,
                -1.7511993741017235}),
        layout("9-star", 9,
               {0.0, 0.0, 1.4, 0.0, 0.9899494936611664, -0.9899494936611664, 0.0, -1.4,
                -0.9899494936611664, -0.9899494936611664, -1.4, 0.0, -0.9899494936611664,
                0.9899494936611664, 0.0, 1.4, 0.9899494936611664, 0.9899494936611664},
               {0.0, 0.0, 1.3374710847758482, -0.41372828932587535, 0.6531847947348937,
                -1.2382849526369664, -0.41372828932587535, -1.3374710847758482, -1.2382849526369664,
                -0.6531847947348937, -1.3374710847758482, 0.41372828932587535, -0.6531847947348937,
                1.2382849526369664, 0.41372828932587535, 1.3374710847758482, 1.2382849526369664,
                0.6531847947348937}),
    };
}
// NOLINTEND(modernize-use-std-numbers)

/// The smallest distance between two members of @p points.
[[nodiscard]] double closestDistance(std::span<const double> points)
{
    double closest = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < points.size(); i += 2)
    {
        for (std::size_t j = i + 2; j < points.size(); j += 2)
        {
            closest =
                std::min(closest, std::hypot(points[i] - points[j], points[i + 1] - points[j + 1]));
        }
    }
    return closest;
}

/// One layout against OpenRocket's: names, count and the points bit for bit.
void expectLayout(const ClusterConfiguration& configuration, const Layout& layout)
{
    SCOPED_TRACE(std::string(layout.xmlName));
    EXPECT_EQ(configuration.getXmlName(), layout.xmlName);
    EXPECT_EQ(configuration.toString(), layout.xmlName);
    EXPECT_EQ(configuration.getClusterCount(), layout.count);
    const std::span<const double> points = configuration.getPoints();
    ASSERT_EQ(points.size(), 2 * static_cast<std::size_t>(layout.count));
    // Bit for bit, the 5-ring and 5-star included.
    EXPECT_EQ(std::vector<double>(points.begin(), points.end()), layout.points);
}

/// getPoints(0.3) against OpenRocket's, within the last bits of cos and sin.
void expectRotated(const ClusterConfiguration& configuration, const Layout& layout)
{
    SCOPED_TRACE(std::string(layout.xmlName));
    const std::vector<double> rotated = configuration.getPoints(0.3);
    ASSERT_EQ(rotated.size(), layout.rotated.size());
    for (std::size_t k = 0; k < rotated.size(); k++)
    {
        EXPECT_NEAR(rotated[k], layout.rotated[k], (1e-12 * std::abs(layout.rotated[k])) + 1e-15)
            << k;
    }
}

TEST(ClusterConfiguration, ConfigurationsAreOpenRocketsInOrder)
{
    const std::vector<Layout>                   layouts = javaLayouts();
    const std::span<const ClusterConfiguration> configurations =
        ClusterConfiguration::configurations();
    ASSERT_EQ(configurations.size(), layouts.size());
    ASSERT_EQ(configurations.size(), 14U);
    for (std::size_t i = 0; i < layouts.size(); i++)
    {
        expectLayout(configurations[i], layouts[i]);
    }
}

TEST(ClusterConfiguration, RotatedPointsMatchOpenRocket)
{
    const std::vector<Layout> layouts = javaLayouts();
    for (std::size_t i = 0; i < layouts.size(); i++)
    {
        expectRotated(ClusterConfiguration::configurations()[i], layouts[i]);
    }
}

TEST(ClusterConfiguration, RotationByZeroKeepsThePoints)
{
    for (const ClusterConfiguration& configuration : ClusterConfiguration::configurations())
    {
        const std::span<const double> points = configuration.getPoints();
        EXPECT_EQ(configuration.getPoints(0.0), std::vector<double>(points.begin(), points.end()))
            << configuration.getXmlName();
    }
}

TEST(ClusterConfiguration, QuarterTurn)
{
    // A quarter turn takes (1, 0) to (0, -1): x' = x cos + y sin, y' = -x sin + y cos.
    const ClusterConfiguration& row     = ClusterConfiguration::configurations()[2];  // 3-row
    const std::vector<double>   quarter = row.getPoints(std::numbers::pi / 2);
    EXPECT_NEAR(quarter[4], 0.0, 1e-15);
    EXPECT_NEAR(quarter[5], -1.0, 1e-15);
}

TEST(ClusterConfiguration, RotationKeepsDistances)
{
    for (const ClusterConfiguration& configuration : ClusterConfiguration::configurations())
    {
        EXPECT_NEAR(closestDistance(configuration.getPoints(1.234)),
                    closestDistance(configuration.getPoints()), 1e-12)
            << configuration.getXmlName();
    }
}

/// The distance between the closest members OpenRocket's layout of @p xmlName has: 1.4 for the
/// 9-grid, 2 * 1.4 sin(pi / 8) between the 9-star's ring members, 1 for the others (infinity for
/// the single one).
[[nodiscard]] double expectedClosestDistance(std::string_view xmlName)
{
    if (xmlName == "single")
    {
        return std::numeric_limits<double>::infinity();
    }
    if (xmlName == "9-grid")
    {
        return 1.4;
    }
    if (xmlName == "9-star")
    {
        return 2 * 1.4 * std::sin(std::numbers::pi / 8);
    }
    return 1.0;
}

TEST(ClusterConfiguration, ClosestMembersAreOneApart)
{
    for (const ClusterConfiguration& configuration : ClusterConfiguration::configurations())
    {
        const double expected = expectedClosestDistance(configuration.getXmlName());
        const double closest  = closestDistance(configuration.getPoints());
        EXPECT_TRUE(closest == expected || std::abs(closest - expected) < 1e-12)
            << configuration.getXmlName() << ": " << closest;
    }
}

/// The member @p k (1 to 4) of a five-member ring of radius @p radius whose first member is at
/// (0, radius), starting at @p index of @p points: (radius sin(2 pi k / 5), radius cos(2 pi k /
/// 5)).
void expectRingMember(std::span<const double> points, std::size_t index, std::size_t k,
                      double radius)
{
    const double angle = 2 * std::numbers::pi * static_cast<double>(k) / 5;
    EXPECT_NEAR(points[index], radius * std::sin(angle), 1e-15) << k;
    EXPECT_NEAR(points[index + 1], radius * std::cos(angle), 1e-15) << k;
}

TEST(ClusterConfiguration, FiveMemberLayoutsMatchTheirFormulas)
{
    // The written-out doubles are Java's Math.sin/Math.cos results; the C++ library agrees to
    // within an ulp or two.
    const double                  r5   = 1.0 / (2 * std::sin(2 * std::numbers::pi / 10));
    const std::span<const double> ring = ClusterConfiguration::fromXmlName("5-ring")->getPoints();
    const std::span<const double> star = ClusterConfiguration::fromXmlName("5-star")->getPoints();
    EXPECT_NEAR(ring[1], r5, 1e-15);
    for (std::size_t k = 1; k <= 4; k++)
    {
        expectRingMember(ring, 2 * k, k, r5);
        // The star's ring follows its centre member.
        expectRingMember(star, (2 * k) + 2, k, 1.0);
    }
}

TEST(ClusterConfiguration, SingleAndIdentity)
{
    const ClusterConfiguration& single = ClusterConfiguration::single();
    EXPECT_EQ(&single, ClusterConfiguration::configurations().data());
    EXPECT_EQ(single.getXmlName(), "single");
    EXPECT_EQ(single.getClusterCount(), 1);
    EXPECT_TRUE(single == ClusterConfiguration::configurations()[0]);
    EXPECT_FALSE(single == ClusterConfiguration::configurations()[1]);
    EXPECT_TRUE(single != ClusterConfiguration::configurations()[13]);
}

TEST(ClusterConfiguration, LookupByXmlName)
{
    for (const ClusterConfiguration& configuration : ClusterConfiguration::configurations())
    {
        EXPECT_EQ(ClusterConfiguration::fromXmlName(configuration.getXmlName()), &configuration);
    }
}

TEST(ClusterConfiguration, LookupIsExact)
{
    // ClusterConfigurationSetter compares with String.equals: exact, untrimmed.
    EXPECT_EQ(ClusterConfiguration::fromXmlName("Single"), nullptr);
    EXPECT_EQ(ClusterConfiguration::fromXmlName(" single"), nullptr);
    EXPECT_EQ(ClusterConfiguration::fromXmlName("7-star"), nullptr);
    EXPECT_EQ(ClusterConfiguration::fromXmlName(""), nullptr);
}

}  // namespace
