#pragma once

#include <span>
#include <string_view>
#include <vector>

namespace QtRocket
{

/// A cluster layout of inner tubes (OpenRocket's ClusterConfiguration). The 14 layouts are
/// immutable shared constants and the only instances there are (the constructor is private and
/// copying is disabled), so they are handed around by reference and compared by identity, as in
/// Java; configurations() lists them in OpenRocket's order (CONFIGURATIONS) and single() is its
/// first entry (SINGLE).
///
/// The points are (x, y) pairs, positioned so that the closest members of the cluster are a
/// distance 1 apart. They are OpenRocket's exactly: the 5-ring and 5-star layouts, which Java
/// computes with Math.sin and Math.cos at class initialisation, are written out as the doubles
/// Java computes (the C++ math library may differ in the last bit).
class ClusterConfiguration
{
public:
    ClusterConfiguration(const ClusterConfiguration&)            = delete;
    ClusterConfiguration(ClusterConfiguration&&)                 = delete;
    ClusterConfiguration& operator=(const ClusterConfiguration&) = delete;
    ClusterConfiguration& operator=(ClusterConfiguration&&)      = delete;
    ~ClusterConfiguration()                                      = default;

    /// Every layout, in OpenRocket's order (CONFIGURATIONS): single, double, 3-row, 4-row,
    /// 3-ring, 4-ring, 5-ring, 6-ring, 3-star, 4-star, 5-star, 6-star, 9-grid, 9-star.
    [[nodiscard]] static std::span<const ClusterConfiguration> configurations() noexcept;

    /// A single motor (SINGLE), the first of configurations().
    [[nodiscard]] static const ClusterConfiguration& single() noexcept;

    /// The layout whose xml name is exactly @p xmlName, or nullptr (the lookup of
    /// ClusterConfigurationSetter, which the .ork loader uses for <clusterconfiguration>).
    [[nodiscard]] static const ClusterConfiguration* fromXmlName(std::string_view xmlName) noexcept;

    /// The name the .ork saver writes in <clusterconfiguration>, e.g. "3-ring" (getXMLName()).
    [[nodiscard]] std::string_view getXmlName() const noexcept { return m_xmlName; }

    /// The number of cluster members: half the number of point coordinates.
    [[nodiscard]] int getClusterCount() const noexcept
    {
        return static_cast<int>(m_points.size() / 2);
    }

    /// The (x, y) pairs of the members, 2 * getClusterCount() values.
    [[nodiscard]] std::span<const double> getPoints() const noexcept { return m_points; }

    /// The points rotated by @p rotation radians: each (x, y) becomes
    /// (x * cos + y * sin, -x * sin + y * cos).
    [[nodiscard]] std::vector<double> getPoints(double rotation) const;

    /// The xml name (toString()).
    [[nodiscard]] std::string_view toString() const noexcept { return m_xmlName; }

    /// Identity, as Java's equals(): each layout exists once.
    [[nodiscard]] bool operator==(const ClusterConfiguration& other) const noexcept
    {
        return this == &other;
    }

private:
    /// @p points must hold a positive even number of values (Java throws
    /// IllegalArgumentException otherwise; here every table is fixed and checked by the tests).
    constexpr ClusterConfiguration(std::string_view        xmlName,
                                   std::span<const double> points) noexcept
      : m_xmlName(xmlName), m_points(points)
    {
    }

    std::string_view        m_xmlName;
    std::span<const double> m_points;
};

}  // namespace QtRocket
