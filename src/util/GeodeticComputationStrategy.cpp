#include "QtRocket/util/GeodeticComputationStrategy.h"

#include <cmath>
#include <format>
#include <numbers>
#include <optional>
#include <source_location>
#include <string_view>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/WorldCoordinate.h"

namespace QtRocket
{

namespace
{

// FLAT
constexpr double kMetersPerDegreeLatitude         = 111325;  // "standard figure"
constexpr double kMetersPerDegreeLongitudeEquator = 111050;

// WGS84. OpenRocket's flattening is that of GRS80 (1 / 298.257222101), not WGS84's
// 1 / 298.257223563; it is kept as it is so that the results agree digit for digit.
constexpr double kEllipsoidSemiMajorAxis = 6378137;  // metres
constexpr double kEllipsoidFlattening    = 1.0 / 298.25722210088;

constexpr double kPrecisionLimit = 0.5e-13;

/// Maximum number of iterations of the Vincenty direct solution. The direct solution converges in
/// a handful of iterations and has no known non-convergent case; it is the inverse solution that
/// struggles near antipodal points. This bound is only here because nothing else stops the loop,
/// so that degenerate input cannot hang the simulation thread.
constexpr int kMaxIterations = 100;

WorldCoordinate addFlat(const WorldCoordinate& location, const Coordinate& delta)
{
    double metersPerDegreeLongitude =
        kMetersPerDegreeLongitudeEquator * std::cos(location.getLatitudeRad());
    // Limit to 1 meter per degree near poles
    metersPerDegreeLongitude = MathUtil::max(metersPerDegreeLongitude, 1.0);

    const double newLat = location.getLatitudeDeg() + (delta.y / kMetersPerDegreeLatitude);
    const double newLon = location.getLongitudeDeg() + (delta.x / metersPerDegreeLongitude);
    const double newAlt = location.getAltitude() + delta.z;

    return {newLat, newLon, newAlt};
}

/// Throws OpenRocket's BugException for a NaN result, raised at @p where: the caller's line, so
/// that the error names the strategy that broke rather than this helper.
[[noreturn]] void nanLocation(const WorldCoordinate& location, const Coordinate& delta,
                              double newLat, double newLon,
                              std::source_location where = std::source_location::current())
{
    bug(std::format("addCoordinate resulted in NaN location:  location={} delta={} newLat={} "
                    "newLon={}",
                    location.toString(), delta.toString(), newLat, newLon),
        where);
}

WorldCoordinate addSpherical(const WorldCoordinate& location, const Coordinate& delta)
{
    const double newAlt = location.getAltitude() + delta.z;

    // bearing (in radians, clockwise from north);
    // d/R is the angular distance (in radians), where d is the distance traveled
    // and R is the earth's radius
    const double d = MathUtil::hypot(delta.x, delta.y);

    // Check for zero movement before computing bearing
    if (MathUtil::equals(d, 0.0))
    {
        return {location.getLatitudeDeg(), location.getLongitudeDeg(), newAlt};
    }

    // Java's Math.atan2(y, x) and std::atan2(y, x) take their arguments in the same order: the
    // east component first gives the angle clockwise from north.
    const double bearing = std::atan2(delta.x, delta.y);

    // Calculate the new lat and lon
    const double sinLat = std::sin(location.getLatitudeRad());
    const double cosLat = std::cos(location.getLatitudeRad());
    const double sinDR  = std::sin(d / WorldCoordinate::kRearth);
    const double cosDR  = std::cos(d / WorldCoordinate::kRearth);

    const double newLat = std::asin((sinLat * cosDR) + (cosLat * sinDR * std::cos(bearing)));
    const double newLon =
        location.getLongitudeRad() +
        std::atan2(std::sin(bearing) * sinDR * cosLat, cosDR - (sinLat * std::sin(newLat)));

    if (std::isnan(newLat) || std::isnan(newLon))
    {
        nanLocation(location, delta, newLat, newLon);
    }

    return {MathUtil::rad2deg(newLat), MathUtil::rad2deg(newLon), newAlt};
}

// ******************************************************************** //
// The Vincenty Direct Solution.
// Code from GeoConstants.java, Ian Cameron Smith, GPL
// ******************************************************************** //

struct DirectSolution
{
    double latitude;     ///< radians, positive north
    double longitude;    ///< radians, positive east
    double backAzimuth;  ///< radians clockwise from north, from the end point back to the start
};

/// Solution of the geodetic direct problem after T. Vincenty. Modified Rainsford's method with
/// Helmert's elliptical terms. Effective in any azimuth and at any distance short of antipodal.
///
/// Programmed for the CDC-6600 by lcdr L. Pfeifer, NGS Rockville MD, 20 Feb 1975.
///
/// @param glat1   The latitude of the starting point, in radians, positive north.
/// @param glon1   The longitude of the starting point, in radians, positive east.
/// @param azimuth The azimuth to the desired location, in radians clockwise from north.
/// @param dist    The distance to the desired location, in meters.
/// @param axis    The semi-major axis of the reference ellipsoid, in meters.
/// @param flat    The flattening of the reference ellipsoid.
/// @return The latitude and longitude of the desired point, in radians, and the azimuth back
///         from that point to the starting point, in radians clockwise from north.
DirectSolution dirct1(double glat1, double glon1, double azimuth, double dist, double axis,
                      double flat)
{
    const double r = 1.0 - flat;

    double tu = r * std::sin(glat1) / std::cos(glat1);

    const double sf = std::sin(azimuth);
    const double cf = std::cos(azimuth);

    double baz = 0.0;

    if (cf != 0.0)
    {
        baz = std::atan2(tu, cf) * 2.0;
    }

    const double cu  = 1.0 / std::sqrt((tu * tu) + 1.0);
    const double su  = tu * cu;
    const double sa  = cu * sf;
    const double c2a = (-sa * sa) + 1.0;

    double x = std::sqrt((((1.0 / r / r) - 1.0) * c2a) + 1.0) + 1.0;
    x        = (x - 2.0) / x;
    double c = 1.0 - x;
    c        = ((x * x / 4.0) + 1) / c;
    double d = ((0.375 * x * x) - 1.0) * x;
    tu       = dist / r / axis / c;
    double y = tu;

    double sy         = 0.0;
    double cy         = 0.0;
    double cz         = 0.0;
    double e          = 0.0;
    int    iterations = 0;
    // Java's do-while, written as a loop with the exit test at the end (a NaN y ends it too,
    // since the comparison is then false).
    while (true)
    {
        sy = std::sin(y);
        cy = std::cos(y);
        cz = std::cos(baz + y);
        e  = (cz * cz * 2.0) - 1.0;

        c = y;
        x = e * cy;
        y = e + e - 1.0;
        y = (((((((sy * sy * 4.0) - 3.0) * y * cz * d / 6.0) + x) * d / 4.0) - cz) * sy * d) + tu;
        iterations++;
        if (!(std::abs(y - c) > kPrecisionLimit) || iterations >= kMaxIterations)
        {
            break;
        }
    }
    // OpenRocket logs a warning when iterations reached kMaxIterations; the core has no logger,
    // so the last value is used silently.

    baz                = (cu * cy * cf) - (su * sy);
    c                  = r * std::sqrt((sa * sa) + (baz * baz));
    d                  = (su * cy) + (cu * sy * cf);
    const double glat2 = std::atan2(d, c);
    c                  = (cu * cy) - (su * sy * cf);
    x                  = std::atan2(sy * sf, c);
    c                  = ((((-3.0 * c2a) + 4.0) * flat) + 4.0) * c2a * flat / 16.0;
    d                  = ((((e * cy * c) + cz) * sy * c) + y) * sa;
    const double glon2 = glon1 + x - ((1.0 - c) * d * flat);
    baz                = std::atan2(sa, baz) + std::numbers::pi;

    return {.latitude = glat2, .longitude = glon2, .backAzimuth = baz};
}

WorldCoordinate addWgs84(const WorldCoordinate& location, const Coordinate& delta)
{
    const double newAlt = location.getAltitude() + delta.z;

    // bearing (in radians, clockwise from north);
    // d/R is the angular distance (in radians), where d is the distance traveled
    // and R is the earth's radius
    const double d = MathUtil::hypot(delta.x, delta.y);

    // Check for zero movement before computing bearing
    if (MathUtil::equals(d, 0.0))
    {
        return {location.getLatitudeDeg(), location.getLongitudeDeg(), newAlt};
    }

    const double bearing = std::atan2(delta.x, delta.y);

    // Calculate the new lat and lon
    const DirectSolution ret    = dirct1(location.getLatitudeRad(), location.getLongitudeRad(),
                                         bearing, d, kEllipsoidSemiMajorAxis, kEllipsoidFlattening);
    const double         newLat = ret.latitude;
    const double         newLon = ret.longitude;

    if (std::isnan(newLat) || std::isnan(newLon))
    {
        nanLocation(location, delta, newLat, newLon);
    }

    return {MathUtil::rad2deg(newLat), MathUtil::rad2deg(newLon), newAlt};
}

Coordinate computeCoriolisAcceleration(const WorldCoordinate& latlon,
                                       const Coordinate&      velocity) noexcept
{
    const double sinlat = std::sin(latlon.getLatitudeRad());
    const double coslat = std::cos(latlon.getLatitudeRad());

    const double vN = velocity.y;
    const double vE = -1 * velocity.x;
    const double vU = velocity.z;

    // Not exactly sure why I have to reverse the x direction, but this gives the precession in
    // the correct direction (e.g, flying north in northern hemisphere should cause defection to
    // the east (+ve x)). All the directions are very confusing because they are tied to the wind
    // direction (to/from?), in which +ve x or east according to WorldCoordinate is what everything
    // is relative to. The directions of everything need so thought, ideally the wind direction
    // and launch rod should be able to be set independently and in terms of bearing with north ==
    // +ve y.

    return Coordinate{2.0 * WorldCoordinate::kErot * ((vN * sinlat) - (vU * coslat)),
                      2.0 * WorldCoordinate::kErot * (-1.0 * vE * sinlat),
                      2.0 * WorldCoordinate::kErot * (vE * coslat)};
}

}  // namespace

WorldCoordinate addCoordinate(GeodeticComputationStrategy strategy, const WorldCoordinate& location,
                              const Coordinate& delta)
{
    switch (strategy)
    {
        case GeodeticComputationStrategy::FLAT:
            return addFlat(location, delta);
        case GeodeticComputationStrategy::SPHERICAL:
            return addSpherical(location, delta);
        case GeodeticComputationStrategy::WGS84:
            return addWgs84(location, delta);
    }
    QTROCKET_UNREACHABLE();
}

Coordinate getCoriolisAcceleration(GeodeticComputationStrategy strategy,
                                   const WorldCoordinate&      location,
                                   const Coordinate&           velocity) noexcept
{
    switch (strategy)
    {
        case GeodeticComputationStrategy::FLAT:
            return Coordinate::kNul;
        case GeodeticComputationStrategy::SPHERICAL:
        case GeodeticComputationStrategy::WGS84:
            return computeCoriolisAcceleration(location, velocity);
    }
    return Coordinate::kNul;
}

std::string_view toString(GeodeticComputationStrategy strategy) noexcept
{
    switch (strategy)
    {
        case GeodeticComputationStrategy::FLAT:
            return "flat";
        case GeodeticComputationStrategy::SPHERICAL:
            return "spherical";
        case GeodeticComputationStrategy::WGS84:
            return "wgs84";
    }
    return "spherical";
}

std::string_view name(GeodeticComputationStrategy strategy) noexcept
{
    switch (strategy)
    {
        case GeodeticComputationStrategy::FLAT:
            return "FLAT";
        case GeodeticComputationStrategy::SPHERICAL:
            return "SPHERICAL";
        case GeodeticComputationStrategy::WGS84:
            return "WGS84";
    }
    return "SPHERICAL";
}

std::string_view displayKey(GeodeticComputationStrategy strategy) noexcept
{
    switch (strategy)
    {
        case GeodeticComputationStrategy::FLAT:
            return "GeodeticComputationStrategy.flat.name";
        case GeodeticComputationStrategy::SPHERICAL:
            return "GeodeticComputationStrategy.spherical.name";
        case GeodeticComputationStrategy::WGS84:
            return "GeodeticComputationStrategy.wgs84.name";
    }
    return "GeodeticComputationStrategy.spherical.name";
}

std::string_view descriptionKey(GeodeticComputationStrategy strategy) noexcept
{
    switch (strategy)
    {
        case GeodeticComputationStrategy::FLAT:
            return "GeodeticComputationStrategy.flat.desc";
        case GeodeticComputationStrategy::SPHERICAL:
            return "GeodeticComputationStrategy.spherical.desc";
        case GeodeticComputationStrategy::WGS84:
            return "GeodeticComputationStrategy.wgs84.desc";
    }
    return "GeodeticComputationStrategy.spherical.desc";
}

std::optional<GeodeticComputationStrategy> geodeticComputationStrategyFromString(
    std::string_view text) noexcept
{
    const std::string_view trimmed = Strings::trim(text);
    for (const GeodeticComputationStrategy strategy : kAllGeodeticComputationStrategies)
    {
        // toString() is the lower-cased enum name; none of the names holds an underscore, so
        // findEnum's replace("_", "") changes nothing.
        if (Strings::equalsIgnoreAsciiCase(trimmed, toString(strategy)))
        {
            return strategy;
        }
    }
    return std::nullopt;
}

}  // namespace QtRocket
