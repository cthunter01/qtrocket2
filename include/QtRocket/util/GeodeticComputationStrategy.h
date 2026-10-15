#pragma once

#include <array>
#include <optional>
#include <string_view>

#include "QtRocket/util/Error.h"

namespace QtRocket
{

class Coordinate;
class WorldCoordinate;

/// How the simulation turns the rocket's Cartesian movement into a new WorldCoordinate and which
/// Coriolis acceleration it feels (OpenRocket's GeodeticComputationStrategy). The Cartesian frame
/// is positive x = east, positive y = north, positive z = up.
///
/// OpenRocket writes each strategy as an enum constant with a body; here the enum is plain and
/// the two computations are the free functions below. getName() / getDescription(), which look
/// the translated texts up, become displayKey() / descriptionKey(); Enum.name() is name() and
/// the .ork spelling is toString().
enum class GeodeticComputationStrategy
{
    /// A flat Earth: addCoordinate() scales metres to degrees directly and the Coriolis
    /// acceleration is always Coordinate::kNul.
    FLAT,
    /// A spherical Earth of radius WorldCoordinate::kRearth.
    SPHERICAL,
    /// The WGS84 reference ellipsoid, by Vincenty's direct solution.
    WGS84,
};

/// Every strategy, in declaration order (GeodeticComputationStrategy.values()).
inline constexpr std::array<GeodeticComputationStrategy, 3> kAllGeodeticComputationStrategies{
    GeodeticComputationStrategy::FLAT, GeodeticComputationStrategy::SPHERICAL,
    GeodeticComputationStrategy::WGS84};

/// The location reached by moving @p delta metres (x east, y north, z up) from @p location. The
/// altitude is always location.getAltitude() + delta.z, and the new latitude and longitude go
/// through the WorldCoordinate constructor, so they are clamped and reduced as it does.
/// - FLAT: 111325 m per degree of latitude and 111050 * cos(latitude) m per degree of longitude,
///   never less than 1 m per degree near the poles.
/// - SPHERICAL and WGS84: the distance hypot(delta.x, delta.y) is travelled along the bearing
///   atan2(delta.x, delta.y), clockwise from north, as a great circle on the sphere or as a
///   geodesic on the ellipsoid (Vincenty's direct solution). When that distance is zero within
///   MathUtil::kEpsilon / 2 the latitude and longitude are kept as they are.
/// @throws BugError when SPHERICAL or WGS84 arrives at a NaN latitude or longitude (a NaN or
///         infinite input, or a finite distance whose square overflows: hypot() is
///         sqrt(x * x + y * y), as in Java), with tryAddCoordinate()'s message. FLAT lets NaN
///         through.
[[nodiscard]] WorldCoordinate addCoordinate(GeodeticComputationStrategy strategy,
                                            const WorldCoordinate&      location,
                                            const Coordinate&           delta);

/// addCoordinate() with its failure returned rather than thrown: where addCoordinate() throws
/// its BugError (SPHERICAL and WGS84 at a NaN latitude or longitude), an Error
/// (ErrorCode::INVALID_ARGUMENT) with OpenRocket's message without its "BUG: ",
/// "addCoordinate resulted in NaN location:  location=WorldCoordinate[lat=45.0, lon=0.0,
/// alt=0.0] delta=(NaN,0.00000,0.00000) newLat=NaN newLon=NaN": the location and the new
/// latitude and longitude (in radians) as Java's Double.toString() prints them, the delta by
/// Coordinate::toString(). Not in Java, whose only form throws: the steppers end a flight that
/// gets there with an ordinary error (decision D3 of 2026-10-14, see AbstractSimulationStepper).
[[nodiscard]] Result<WorldCoordinate> tryAddCoordinate(GeodeticComputationStrategy strategy,
                                                       const WorldCoordinate&      location,
                                                       const Coordinate&           delta);

/// The Coriolis acceleration at @p location for @p velocity, both in the x east, y north, z up
/// frame: 2 * kErot * (vN sin(lat) - vU cos(lat), -vE sin(lat), vE cos(lat)) with vN = velocity.y,
/// vU = velocity.z and vE = -velocity.x (OpenRocket reverses x here so that flying north in the
/// northern hemisphere deflects east). FLAT always gives Coordinate::kNul. The result carries no
/// weight.
[[nodiscard]] Coordinate getCoriolisAcceleration(GeodeticComputationStrategy strategy,
                                                 const WorldCoordinate&      location,
                                                 const Coordinate&           velocity) noexcept;

/// The .ork spelling, what OpenRocketSaver writes to <geodeticmethod>: the lower-cased enum name
/// ("flat", "spherical", "wgs84"). Deviation: Java's toString() returns getName(), the translated
/// display name ("Flat Earth", ...); use displayKey() for that. This follows LineStyle instead.
[[nodiscard]] std::string_view toString(GeodeticComputationStrategy strategy) noexcept;

/// Java's Enum.name(): the upper-case enum name ("FLAT", "SPHERICAL", "WGS84"), what
/// ApplicationPreferences.putEnum stores for the geodetic computation preference.
[[nodiscard]] std::string_view name(GeodeticComputationStrategy strategy) noexcept;

/// The translation key of the strategy's display name (Java's getName()), e.g.
/// "GeodeticComputationStrategy.flat.name".
[[nodiscard]] std::string_view displayKey(GeodeticComputationStrategy strategy) noexcept;

/// The translation key of the strategy's description (Java's getDescription()), e.g.
/// "GeodeticComputationStrategy.flat.desc".
[[nodiscard]] std::string_view descriptionKey(GeodeticComputationStrategy strategy) noexcept;

/// The strategy whose name is @p text, trimmed and compared without regard to ASCII case (and
/// with underscores removed from the enum name, as DocumentConfig.findEnum does for .ork files),
/// so the .ork spelling toString() and the preference spelling name() both match. Anything else,
/// including the display names, is nullopt. Deviation: this merges Enum.valueOf() (exact) and
/// DocumentConfig.findEnum() (case-sensitive against the lower-cased name) into one permissive
/// parser; OpenRocket itself would reject "WGS84" in an .ork file and "wgs84" in the preferences.
/// The .ork loader does not use it: SimulationConditionsHandler reads <geodeticmethod> with
/// DocumentConfig::findEnum() over kAllGeodeticComputationStrategies and name(), which is exact.
[[nodiscard]] std::optional<GeodeticComputationStrategy> geodeticComputationStrategyFromString(
    std::string_view text) noexcept;

}  // namespace QtRocket
