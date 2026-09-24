#pragma once

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <string>

namespace QtRocket
{

/// The latitude, longitude and altitude of a rocket (OpenRocket's WorldCoordinate). An immutable
/// value: the simulation makes a new one for every movement through addCoordinate() in
/// GeodeticComputationStrategy.h.
///
/// Deviations from OpenRocket:
/// - clone() is not ported: this is a value type, so a copy is a clone.
/// - hashCode() is the std::hash specialisation below.
/// - The degree conversions are MathUtil::deg2rad / rad2deg (deg * pi / 180, two roundings),
///   while Java's Math.toRadians / toDegrees multiply by a pre-rounded pi / 180 and 180 / pi; the
///   two can differ in the last ulp.
class WorldCoordinate
{
public:
    /// Mean Earth radius in metres (REARTH).
    static constexpr double kRearth = 6371000.0;
    /// Sidereal Earth rotation rate in rad/s (EROT).
    static constexpr double kErot = 7.2921150e-5;

    /// @param latitudeDeg  latitude in degrees north; values outside -90 ... 90 are clamped.
    /// @param longitudeDeg longitude in degrees east; values outside -180 ... 180 are reduced
    ///                     into it (on the boundary either -180 or 180 may come back, as with
    ///                     MathUtil::reducePi).
    /// @param altitude     altitude in metres, unbounded.
    /// A NaN latitude or longitude stays NaN (clamping and reduction pass it through), and an
    /// infinite longitude reduces to NaN.
    WorldCoordinate(double latitudeDeg, double longitudeDeg, double altitude) noexcept;

    /// The altitude in metres.
    [[nodiscard]] double getAltitude() const noexcept { return m_alt; }

    /// The longitude in radians east, -pi ... pi.
    [[nodiscard]] double getLongitudeRad() const noexcept { return m_lon; }

    /// The longitude in degrees east, -180 ... 180.
    [[nodiscard]] double getLongitudeDeg() const noexcept;

    /// The latitude in radians north, -pi/2 ... pi/2.
    [[nodiscard]] double getLatitudeRad() const noexcept { return m_lat; }

    /// The latitude in degrees north, -90 ... 90.
    [[nodiscard]] double getLatitudeDeg() const noexcept;

    /// OpenRocket's equals(): the latitude and longitude in radians and the altitude each agree
    /// within MathUtil::kEpsilon (relative, see MathUtil::equals). Never true when a value is NaN.
    /// Not transitive, so it must not order or key containers; std::hash follows OpenRocket's
    /// hashCode and is coarse for the same reason.
    [[nodiscard]] bool operator==(const WorldCoordinate& other) const noexcept;

    /// "WorldCoordinate[lat=<degrees>, lon=<degrees>, alt=<metres>]" with the shortest digits that
    /// read back as the same double. Deviation: Java's Double.toString always writes a fraction
    /// ("10.0") and switches to "1.0E7" notation from 1e7 and below 1e-3, where std::format writes
    /// "10" and "1e+16"; NaN and infinity print as nan and inf (Java: NaN and Infinity). For people
    /// and exception messages only; nothing written to a file goes through it.
    [[nodiscard]] std::string toString() const;

private:
    double m_lat;  ///< radians north
    double m_lon;  ///< radians east
    double m_alt;  ///< metres
};

/// Writes toString(), so that GoogleTest prints a readable value when EXPECT_EQ on two world
/// coordinates fails.
std::ostream& operator<<(std::ostream& os, const WorldCoordinate& wc);

}  // namespace QtRocket

/// OpenRocket's hashCode: (int)(1000 * (lat + lon + alt)), the angles in radians as stored. Coarse
/// on purpose, since operator== is tolerant; two coordinates that compare equal can still hash
/// differently across a bucket edge, exactly as in OpenRocket.
template <>
struct std::hash<QtRocket::WorldCoordinate>
{
    [[nodiscard]] std::size_t operator()(const QtRocket::WorldCoordinate& wc) const noexcept;
};
