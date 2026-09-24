#include "QtRocket/rocket/ClusterConfiguration.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <string_view>
#include <vector>

namespace QtRocket
{

namespace
{

// Helper values, as in ClusterConfiguration.java (its literals 1.4142135623730951 and
// 1.7320508075688772 are exactly these doubles).
constexpr double kSqrt2 = std::numbers::sqrt2;
constexpr double kSqrt3 = std::numbers::sqrt3;

// Single row
constexpr std::array<double, 2> kSinglePoints{0, 0};
constexpr std::array<double, 4> kDoublePoints{-0.5, 0, 0.5, 0};
constexpr std::array<double, 6> kRow3Points{-1.0, 0, 0.0, 0, 1.0, 0};
constexpr std::array<double, 8> kRow4Points{-1.5, 0, -0.5, 0, 0.5, 0, 1.5, 0};

// Ring of tubes
constexpr std::array<double, 6> kRing3Points{-0.5, -1.0 / (2 * kSqrt3), 0.5, -1.0 / (2 * kSqrt3),
                                             0,    1.0 / kSqrt3};
constexpr std::array<double, 8> kRing4Points{-0.5, 0.5, 0.5, 0.5, 0.5, -0.5, -0.5, -0.5};
// Java: 0, R5, then R5 * sin(2 pi k / 5), R5 * cos(2 pi k / 5) for k = 1..4, with
// R5 = 1 / (2 sin(2 pi / 10)); the doubles Java's Math.sin and Math.cos give.
constexpr std::array<double, 10> kRing5Points{
    0.0,
    0.8506508083520399,
    0.8090169943749473,
    0.2628655560595668,
    0.5000000000000001,
    -0.6881909602355867,
    -0.4999999999999999,
    -0.6881909602355868,
    -0.8090169943749475,
    0.26286555605956663,
};
constexpr std::array<double, 12> kRing6Points{0, 1,  kSqrt3 / 2,  0.5,  kSqrt3 / 2,  -0.5,
                                              0, -1, -kSqrt3 / 2, -0.5, -kSqrt3 / 2, 0.5};

// Centered with ring
constexpr std::array<double, 8>  kStar3Points{0, 0, 0, 1, kSqrt3 / 2, -0.5, -kSqrt3 / 2, -0.5};
constexpr std::array<double, 10> kStar4Points{0,           0,          -1 / kSqrt2, 1 / kSqrt2,
                                              1 / kSqrt2,  1 / kSqrt2, 1 / kSqrt2,  -1 / kSqrt2,
                                              -1 / kSqrt2, -1 / kSqrt2};
// Java: 0, 0, 0, 1, then sin(2 pi k / 5), cos(2 pi k / 5) for k = 1..4; the doubles Java's
// Math.sin and Math.cos give.
constexpr std::array<double, 12> kStar5Points{
    0.0,
    0.0,
    0.0,
    1.0,
    0.9510565162951535,
    0.30901699437494745,
    0.5877852522924732,
    -0.8090169943749473,
    -0.587785252292473,
    -0.8090169943749476,
    -0.9510565162951536,
    0.30901699437494723,
};
constexpr std::array<double, 14> kStar6Points{
    0, 0, 0, 1, kSqrt3 / 2, 0.5, kSqrt3 / 2, -0.5, 0, -1, -kSqrt3 / 2, -0.5, -kSqrt3 / 2, 0.5};
constexpr std::array<double, 18> kGrid9Points{-1.4, 1.4, 0, 1.4,  1.4,  1.4, -1.4, 0,   0,
                                              0,    1.4, 0, -1.4, -1.4, 0,   -1.4, 1.4, -1.4};
constexpr std::array<double, 18> kStar9Points{0,
                                              0,
                                              1.4,
                                              0,
                                              1.4 / kSqrt2,
                                              -1.4 / kSqrt2,
                                              0,
                                              -1.4,
                                              -1.4 / kSqrt2,
                                              -1.4 / kSqrt2,
                                              -1.4,
                                              0,
                                              -1.4 / kSqrt2,
                                              1.4 / kSqrt2,
                                              0,
                                              1.4,
                                              1.4 / kSqrt2,
                                              1.4 / kSqrt2};

}  // namespace

std::span<const ClusterConfiguration> ClusterConfiguration::configurations() noexcept
{
    static constexpr std::array<ClusterConfiguration, 14> kConfigurations{{
        ClusterConfiguration("single", kSinglePoints),
        ClusterConfiguration("double", kDoublePoints),
        ClusterConfiguration("3-row", kRow3Points),
        ClusterConfiguration("4-row", kRow4Points),
        ClusterConfiguration("3-ring", kRing3Points),
        ClusterConfiguration("4-ring", kRing4Points),
        ClusterConfiguration("5-ring", kRing5Points),
        ClusterConfiguration("6-ring", kRing6Points),
        ClusterConfiguration("3-star", kStar3Points),
        ClusterConfiguration("4-star", kStar4Points),
        ClusterConfiguration("5-star", kStar5Points),
        ClusterConfiguration("6-star", kStar6Points),
        ClusterConfiguration("9-grid", kGrid9Points),
        ClusterConfiguration("9-star", kStar9Points),
    }};
    return kConfigurations;
}

const ClusterConfiguration& ClusterConfiguration::single() noexcept
{
    return configurations().front();
}

const ClusterConfiguration* ClusterConfiguration::fromXmlName(std::string_view xmlName) noexcept
{
    for (const ClusterConfiguration& configuration : configurations())
    {
        if (configuration.getXmlName() == xmlName)
        {
            return &configuration;
        }
    }
    return nullptr;
}

std::vector<double> ClusterConfiguration::getPoints(double rotation) const
{
    const double        cos = std::cos(rotation);
    const double        sin = std::sin(rotation);
    std::vector<double> ret;
    ret.reserve(m_points.size());
    for (std::size_t i = 0; i < m_points.size() / 2; i++)
    {
        const double x = m_points[2 * i];
        const double y = m_points[(2 * i) + 1];
        ret.push_back((x * cos) + (y * sin));
        ret.push_back((-x * sin) + (y * cos));
    }
    return ret;
}

}  // namespace QtRocket
