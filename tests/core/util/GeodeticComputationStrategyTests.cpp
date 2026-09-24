#include "QtRocket/util/GeodeticComputationStrategy.h"

#include <cmath>
#include <format>
#include <limits>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/WorldCoordinate.h"

namespace
{

using QtRocket::addCoordinate;
using QtRocket::BugError;
using QtRocket::Coordinate;
using QtRocket::descriptionKey;
using QtRocket::displayKey;
using QtRocket::GeodeticComputationStrategy;
using QtRocket::geodeticComputationStrategyFromString;
using QtRocket::getCoriolisAcceleration;
using QtRocket::kAllGeodeticComputationStrategies;
using QtRocket::name;
using QtRocket::toString;
using QtRocket::WorldCoordinate;
namespace MathUtil = QtRocket::MathUtil;

constexpr double kPi  = std::numbers::pi;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

/// The distance of one degree of arc along a great circle of WorldCoordinate::kRearth.
constexpr double kSphereDegree = WorldCoordinate::kRearth * kPi / 180;
/// The distance of one degree of longitude on the equator of the ellipsoid (a circle of radius
/// 6378137 m).
constexpr double kEquatorDegree = 6378137.0 * kPi / 180;

/// Java's assertEquals on the latitude and longitude in degrees, printing the whole coordinate
/// on failure.
void expectLatLon(const WorldCoordinate& result, double latitude, double longitude,
                  double tolerance)
{
    SCOPED_TRACE(result.toString());
    EXPECT_NEAR(latitude, result.getLatitudeDeg(), tolerance);
    EXPECT_NEAR(longitude, result.getLongitudeDeg(), tolerance);
}

/// True when addCoordinate() throws a BugError for these inputs.
bool throwsBugError(GeodeticComputationStrategy strategy, const WorldCoordinate& location,
                    const Coordinate& delta)
{
    try
    {
        static_cast<void>(addCoordinate(strategy, location, delta));
        return false;
    }
    catch (const BugError&)
    {
        return true;
    }
}

/// The BugError addCoordinate() throws for these inputs, or one raised here saying that nothing
/// was thrown.
BugError catchBugError(GeodeticComputationStrategy strategy, const WorldCoordinate& location,
                       const Coordinate& delta)
{
    try
    {
        static_cast<void>(addCoordinate(strategy, location, delta));
    }
    catch (const BugError& e)
    {
        return e;
    }
    return BugError{"nothing was thrown"};
}

/// True when the value right after @p label in @p what is a NaN, whatever its sign: the NaNs in
/// the message are computed by libm, whose sign bit is unspecified, and a negative NaN formats
/// as "-nan" (or "-nan(ind)" on MSVC).
bool isNaNAfter(std::string_view what, std::string_view label)
{
    const auto pos = what.find(label);
    if (pos == std::string_view::npos)
    {
        return false;
    }
    const std::string_view value = what.substr(pos + label.size());
    return value.starts_with("nan") || value.starts_with("-nan");
}

// ---- Ported from GeodeticComputationStrategyTest.java ----

TEST(GeodeticComputationStrategy, SphericalAddCoordinate)
{
    const double arcmin = (1.0 / 60.0);
    const double arcsec = (1.0 / (60.0 * 60.0));

    const double lat1 = 50.0 + (3 * arcmin) + (59 * arcsec);
    const double lon1 = -1.0 * (5 + (42 * arcmin) + (53 * arcsec));  // W

    const double lat2 = 58 + (38 * arcmin) + (38 * arcsec);
    const double lon2 = -1.0 * (3 + (4 * arcmin) + (12 * arcsec));

    const double range   = 968.9 * 1000.0;
    const double bearing = (9.0 + (7 * arcmin) + (11 * arcsec)) * (kPi / 180.0);

    const Coordinate coord(range * std::sin(bearing), range * std::cos(bearing), 1000.0);
    WorldCoordinate  wc(lat1, lon1, 0.0);
    wc = addCoordinate(GeodeticComputationStrategy::SPHERICAL, wc, coord);

    EXPECT_NEAR(lat2, wc.getLatitudeDeg(), 0.001);
    EXPECT_NEAR(lon2, wc.getLongitudeDeg(), 0.001);
    EXPECT_NEAR(1000.0, wc.getAltitude(), 0.0);
}

void testAddCoordinate(double initialLatitude, double initialLongitude, double distance,
                       double bearingDeg, double finalLatitude, double finalLongitude,
                       bool testFlat)
{
    SCOPED_TRACE(std::format("from ({}, {}) {} m at {} deg", initialLatitude, initialLongitude,
                             distance, bearingDeg));

    const double bearing = MathUtil::deg2rad(bearingDeg);

    // positive X is EAST, positive Y is NORTH
    const double deltaX = distance * std::sin(bearing);
    const double deltaY = distance * std::cos(bearing);

    const Coordinate      coord(deltaX, deltaY, 1000.0);
    const WorldCoordinate wc(initialLatitude, initialLongitude, 0.0);

    // Test SPHERICAL
    double          tolerance = 0.0015 * distance / 111325;
    WorldCoordinate result    = addCoordinate(GeodeticComputationStrategy::SPHERICAL, wc, coord);

    expectLatLon(result, finalLatitude, finalLongitude, tolerance);
    EXPECT_NEAR(1000.0, result.getAltitude(), 0.0);

    // Test WGS84
    /*
     * Note: Since the example values are computed using a spherical earth approximation, the
     * WGS84 method will have significantly larger errors. A tolerance of 1% accommodates all
     * cases except the NE flight near the north pole, where the ellipsoidal effect is the
     * greatest.
     */
    tolerance = 0.04 * distance / 111325;
    result    = addCoordinate(GeodeticComputationStrategy::WGS84, wc, coord);

    expectLatLon(result, finalLatitude, finalLongitude, tolerance);
    EXPECT_NEAR(1000.0, result.getAltitude(), 0.0);

    // Test FLAT
    if (testFlat)
    {
        tolerance = 0.02 * distance / 111325;
        result    = addCoordinate(GeodeticComputationStrategy::FLAT, wc, coord);

        expectLatLon(result, finalLatitude, finalLongitude, tolerance);
        EXPECT_NEAR(1000.0, result.getAltitude(), 0.0);
    }
}

TEST(GeodeticComputationStrategy, AddCoordinates)
{
    const double min = 1 / 60.0;
    const double sec = 1 / 3600.0;

    // Test zero movement
    testAddCoordinate(50.0, 20.0, 0, 123, 50.0, 20.0, false);

    /*
     * These example values have been computed using the calculator at
     * http://www.movable-type.co.uk/scripts/latlong.html
     */

    // Long distance NE over England, crosses Greenwich meridian
    // 50 03N 005 42W to 58 38N 003 04E is 1109km at 027 16'07"
    testAddCoordinate(50 + (3 * min), -5 - (42 * min), 1109000, 27 + (16 * min) + (7 * sec),
                      58 + (38 * min), 3 + (4 * min), false);

    // SW over Brazil
    // -10N -60E to -11N -61E is 155.9km at 224 25'34"
    testAddCoordinate(-10, -60, 155900, 224 + (25 * min) + (34 * sec), -11, -61, true);

    // NW over the 180 meridian
    // 63N -179E to 63 01N 179E is 100.9km at 271 56'34"
    testAddCoordinate(63, -179, 100900, 271 + (56 * min) + (34 * sec), 63 + (1 * min), 179, true);

    // NE near the north pole
    // 89 50N 0E to 89 45N 175E is 46.29 km at 003 00'01"
    testAddCoordinate(89 + (50 * min), 0, 46290, 3 + (0 * min) + (1 * sec), 89 + (45 * min), 175,
                      false);

    // S directly over south pole
    // -89 50N 12E to -89 45N 192E is 46.33km at 180 00'00"
    testAddCoordinate(-89 - (50 * min), 12, 46330, 180, -89 - (45 * min), -168, false);
}

TEST(GeodeticComputationStrategy, SphericalGetCoriolisAcceleration)
{
    // For positive latitude and rotational velocity, a movement due east results in an
    // acceleration due south
    const Coordinate      velocity(-1000, 0, 0);
    const WorldCoordinate wc(45, 0, 0);
    const double          northAccel =
        getCoriolisAcceleration(GeodeticComputationStrategy::SPHERICAL, wc, velocity).y;
    EXPECT_TRUE(northAccel < 0.0);
}

// ---- Additional tests ----

TEST(GeodeticComputationStrategy, AllStrategiesInDeclarationOrder)
{
    ASSERT_EQ(kAllGeodeticComputationStrategies.size(), 3U);
    EXPECT_EQ(kAllGeodeticComputationStrategies[0], GeodeticComputationStrategy::FLAT);
    EXPECT_EQ(kAllGeodeticComputationStrategies[1], GeodeticComputationStrategy::SPHERICAL);
    EXPECT_EQ(kAllGeodeticComputationStrategies[2], GeodeticComputationStrategy::WGS84);
}

TEST(GeodeticComputationStrategy, OrkSpelling)
{
    // OpenRocketSaver writes the enum name in lower case to <geodeticmethod>.
    EXPECT_EQ(toString(GeodeticComputationStrategy::FLAT), "flat");
    EXPECT_EQ(toString(GeodeticComputationStrategy::SPHERICAL), "spherical");
    EXPECT_EQ(toString(GeodeticComputationStrategy::WGS84), "wgs84");
}

TEST(GeodeticComputationStrategy, NameIsTheUpperCaseEnumName)
{
    // ApplicationPreferences.putEnum stores Enum.name().
    EXPECT_EQ(name(GeodeticComputationStrategy::FLAT), "FLAT");
    EXPECT_EQ(name(GeodeticComputationStrategy::SPHERICAL), "SPHERICAL");
    EXPECT_EQ(name(GeodeticComputationStrategy::WGS84), "WGS84");
    for (const GeodeticComputationStrategy strategy : kAllGeodeticComputationStrategies)
    {
        // The .ork spelling is exactly the name lower-cased, as OpenRocketSaver writes it.
        EXPECT_EQ(QtRocket::Strings::toLower(name(strategy)), toString(strategy));
        EXPECT_NE(name(strategy), toString(strategy));
    }
}

TEST(GeodeticComputationStrategy, DisplayAndDescriptionKeys)
{
    EXPECT_EQ(displayKey(GeodeticComputationStrategy::FLAT),
              "GeodeticComputationStrategy.flat.name");
    EXPECT_EQ(displayKey(GeodeticComputationStrategy::SPHERICAL),
              "GeodeticComputationStrategy.spherical.name");
    EXPECT_EQ(displayKey(GeodeticComputationStrategy::WGS84),
              "GeodeticComputationStrategy.wgs84.name");
    EXPECT_EQ(descriptionKey(GeodeticComputationStrategy::FLAT),
              "GeodeticComputationStrategy.flat.desc");
    EXPECT_EQ(descriptionKey(GeodeticComputationStrategy::SPHERICAL),
              "GeodeticComputationStrategy.spherical.desc");
    EXPECT_EQ(descriptionKey(GeodeticComputationStrategy::WGS84),
              "GeodeticComputationStrategy.wgs84.desc");
}

TEST(GeodeticComputationStrategy, FromStringRoundTrips)
{
    for (const GeodeticComputationStrategy strategy : kAllGeodeticComputationStrategies)
    {
        EXPECT_EQ(geodeticComputationStrategyFromString(toString(strategy)), strategy);
        EXPECT_EQ(geodeticComputationStrategyFromString(std::string(toString(strategy)) + " "),
                  strategy);
        // The preference spelling reads back too.
        EXPECT_EQ(geodeticComputationStrategyFromString(name(strategy)), strategy);
    }
}

TEST(GeodeticComputationStrategy, FromStringTrimsAndIgnoresCase)
{
    // DocumentConfig.findEnum trims; the preference store keeps the upper-case enum name.
    EXPECT_EQ(geodeticComputationStrategyFromString(" wgs84\n"),
              GeodeticComputationStrategy::WGS84);
    EXPECT_EQ(geodeticComputationStrategyFromString("SPHERICAL"),
              GeodeticComputationStrategy::SPHERICAL);
    EXPECT_EQ(geodeticComputationStrategyFromString("Flat"), GeodeticComputationStrategy::FLAT);
    EXPECT_EQ(geodeticComputationStrategyFromString("\tWGS84 "),
              GeodeticComputationStrategy::WGS84);
}

TEST(GeodeticComputationStrategy, FromStringRejectsUnknownNames)
{
    EXPECT_EQ(geodeticComputationStrategyFromString(""), std::nullopt);
    EXPECT_EQ(geodeticComputationStrategyFromString("   "), std::nullopt);
    EXPECT_EQ(geodeticComputationStrategyFromString("wgs-84"), std::nullopt);
    EXPECT_EQ(geodeticComputationStrategyFromString("wgs_84"), std::nullopt);
    EXPECT_EQ(geodeticComputationStrategyFromString("wgs 84"), std::nullopt);
    EXPECT_EQ(geodeticComputationStrategyFromString("wgs"), std::nullopt);
    EXPECT_EQ(geodeticComputationStrategyFromString("sphere"), std::nullopt);
    EXPECT_EQ(geodeticComputationStrategyFromString("Flat Earth"), std::nullopt);
    EXPECT_EQ(geodeticComputationStrategyFromString("GeodeticComputationStrategy.flat.name"),
              std::nullopt);
    EXPECT_EQ(geodeticComputationStrategyFromString("0"), std::nullopt);
}

TEST(GeodeticComputationStrategy, FlatCoriolisIsNul)
{
    const WorldCoordinate wc(45, 10, 100);
    EXPECT_TRUE(
        getCoriolisAcceleration(GeodeticComputationStrategy::FLAT, wc, Coordinate(-1000, 500, 200))
            .exactlyEquals(Coordinate::kNul));
    EXPECT_TRUE(
        getCoriolisAcceleration(GeodeticComputationStrategy::FLAT, wc, Coordinate(1e9, 0, 0))
            .exactlyEquals(Coordinate::kNul));
}

TEST(GeodeticComputationStrategy, SphericalAndWgs84CoriolisAgree)
{
    const WorldCoordinate wc(-30, 140, 500);
    const Coordinate      velocity(200, -300, 400);
    EXPECT_TRUE(getCoriolisAcceleration(GeodeticComputationStrategy::SPHERICAL, wc, velocity)
                    .exactlyEquals(
                        getCoriolisAcceleration(GeodeticComputationStrategy::WGS84, wc, velocity)));
}

TEST(GeodeticComputationStrategy, CoriolisPinnedValues)
{
    constexpr double kTol = 1e-12;

    // Due east at 45 N: no east-west deflection, an acceleration south and up.
    Coordinate a = getCoriolisAcceleration(GeodeticComputationStrategy::SPHERICAL,
                                           WorldCoordinate(45, 0, 0), Coordinate(-1000, 0, 0));
    EXPECT_NEAR(0.0, a.x, kTol);
    EXPECT_NEAR(-0.10312607931384281, a.y, kTol);
    EXPECT_NEAR(0.10312607931384282, a.z, kTol);

    // Flying north in the northern hemisphere deflects east (+x), as OpenRocket's comment says.
    a = getCoriolisAcceleration(GeodeticComputationStrategy::SPHERICAL, WorldCoordinate(45, 0, 0),
                                Coordinate(0, 1000, 0));
    EXPECT_NEAR(0.10312607931384281, a.x, kTol);
    EXPECT_NEAR(0.0, a.y, kTol);
    EXPECT_NEAR(0.0, a.z, kTol);

    // Straight up on the equator deflects west: -2 * kErot * vU.
    a = getCoriolisAcceleration(GeodeticComputationStrategy::WGS84, WorldCoordinate(0, 0, 0),
                                Coordinate(0, 0, 1000));
    EXPECT_NEAR(-0.1458423, a.x, kTol);
    EXPECT_NEAR(0.0, a.y, kTol);
    EXPECT_NEAR(0.0, a.z, kTol);

    // A general case in the southern hemisphere.
    a = getCoriolisAcceleration(GeodeticComputationStrategy::WGS84, WorldCoordinate(-30, 140, 500),
                                Coordinate(200, -300, 400));
    EXPECT_NEAR(-0.028644909698540506, a.x, kTol);
    EXPECT_NEAR(-0.014584229999999998, a.y, kTol);
    EXPECT_NEAR(-0.02526062734927025, a.z, kTol);
}

TEST(GeodeticComputationStrategy, CoriolisIsZeroForZeroVelocityAndUnweighted)
{
    for (const GeodeticComputationStrategy strategy : kAllGeodeticComputationStrategies)
    {
        const Coordinate a =
            getCoriolisAcceleration(strategy, WorldCoordinate(45, 0, 0), Coordinate(0, 0, 0, 5.0));
        EXPECT_TRUE(a.exactlyEquals(Coordinate::kNul)) << toString(strategy);
        // The velocity's weight is not carried over.
        const Coordinate b = getCoriolisAcceleration(strategy, WorldCoordinate(45, 0, 0),
                                                     Coordinate(100, 100, 100, 5.0));
        EXPECT_EQ(0.0, b.weight) << toString(strategy);
    }
}

TEST(GeodeticComputationStrategy, CoriolisDoesNotDependOnLongitudeOrAltitude)
{
    const Coordinate velocity(300, -200, 100);
    const Coordinate a = getCoriolisAcceleration(GeodeticComputationStrategy::SPHERICAL,
                                                 WorldCoordinate(37, 0, 0), velocity);
    const Coordinate b = getCoriolisAcceleration(GeodeticComputationStrategy::SPHERICAL,
                                                 WorldCoordinate(37, -122, 30000), velocity);
    EXPECT_TRUE(a.exactlyEquals(b));
}

TEST(GeodeticComputationStrategy, FlatScalesMetresPerDegree)
{
    constexpr double kTol = 1e-9;

    // On the equator: 111050 m east and 111325 m north are one degree each.
    WorldCoordinate result =
        addCoordinate(GeodeticComputationStrategy::FLAT, WorldCoordinate(0, 0, 0),
                      Coordinate(111050, 111325, 10));
    EXPECT_NEAR(1, result.getLatitudeDeg(), kTol);
    EXPECT_NEAR(1, result.getLongitudeDeg(), kTol);
    EXPECT_EQ(10, result.getAltitude());

    // At 60 degrees a degree of longitude is half as long.
    result = addCoordinate(GeodeticComputationStrategy::FLAT, WorldCoordinate(60, 10, 100),
                           Coordinate(55525, -222650, -50));
    EXPECT_NEAR(58, result.getLatitudeDeg(), kTol);
    EXPECT_NEAR(11, result.getLongitudeDeg(), kTol);
    EXPECT_EQ(50, result.getAltitude());
}

TEST(GeodeticComputationStrategy, FlatLimitsMetresPerDegreeNearThePoles)
{
    // cos(90 deg) is about 6e-17, so the scale is clamped to 1 m per degree.
    const WorldCoordinate result = addCoordinate(GeodeticComputationStrategy::FLAT,
                                                 WorldCoordinate(90, 10, 0), Coordinate(5, 0, 0));
    EXPECT_NEAR(90, result.getLatitudeDeg(), 1e-9);
    EXPECT_NEAR(15, result.getLongitudeDeg(), 1e-9);
}

TEST(GeodeticComputationStrategy, FlatResultIsClampedAndReducedByTheConstructor)
{
    WorldCoordinate result = addCoordinate(GeodeticComputationStrategy::FLAT,
                                           WorldCoordinate(0, 179.5, 0), Coordinate(111050, 0, 0));
    EXPECT_NEAR(-179.5, result.getLongitudeDeg(), 1e-9);

    result = addCoordinate(GeodeticComputationStrategy::FLAT, WorldCoordinate(89.5, 0, 0),
                           Coordinate(0, 111325, 0));
    EXPECT_NEAR(90, result.getLatitudeDeg(), 1e-9);
}

TEST(GeodeticComputationStrategy, FlatLetsNaNThrough)
{
    WorldCoordinate result = addCoordinate(GeodeticComputationStrategy::FLAT,
                                           WorldCoordinate(kNaN, 0, 0), Coordinate(1000, 0, 0));
    EXPECT_TRUE(std::isnan(result.getLatitudeDeg()));
    // cos(NaN) is NaN, and MathUtil::max(NaN, 1) is 1 (as OpenRocket's), so the 1000 m become
    // 1000 degrees of longitude, reduced to -80.
    EXPECT_NEAR(-80, result.getLongitudeDeg(), 1e-9);

    result = addCoordinate(GeodeticComputationStrategy::FLAT, WorldCoordinate(10, 20, 0),
                           Coordinate(kNaN, 0, kNaN));
    EXPECT_NEAR(10, result.getLatitudeDeg(), 1e-9);
    EXPECT_TRUE(std::isnan(result.getLongitudeDeg()));
    EXPECT_TRUE(std::isnan(result.getAltitude()));
}

TEST(GeodeticComputationStrategy, ZeroMovementKeepsLatitudeAndLongitude)
{
    for (const GeodeticComputationStrategy strategy : kAllGeodeticComputationStrategies)
    {
        SCOPED_TRACE(toString(strategy));
        const WorldCoordinate start(50, 20, 100);

        WorldCoordinate result = addCoordinate(strategy, start, Coordinate(0, 0, 123));
        expectLatLon(result, 50, 20, 1e-12);
        EXPECT_EQ(223, result.getAltitude());

        // Below MathUtil::kEpsilon / 2 metres the movement counts as zero (bearing is undefined).
        result = addCoordinate(strategy, start, Coordinate(1e-9, -1e-9, -100));
        expectLatLon(result, 50, 20, 1e-12);
        EXPECT_EQ(0, result.getAltitude());
    }
}

/// Pins the atan2(delta.x, delta.y) argument order for one strategy: a positive x moves east.
void expectEastMove(GeodeticComputationStrategy strategy)
{
    SCOPED_TRACE(toString(strategy));
    // A great circle heading east bends towards the equator by about 1.2e-5 degrees over these
    // 10 km; the flat Earth does not bend at all.
    const WorldCoordinate result =
        addCoordinate(strategy, WorldCoordinate(10, 20, 0), Coordinate(10000, 0, 0));
    EXPECT_NEAR(10, result.getLatitudeDeg(), 1e-4);
    EXPECT_GT(result.getLongitudeDeg(), 20.05);
    EXPECT_LT(result.getLongitudeDeg(), 20.15);
}

/// A positive y moves north, and a negative x and y south-west.
void expectNorthAndSouthWestMoves(GeodeticComputationStrategy strategy)
{
    SCOPED_TRACE(toString(strategy));
    const WorldCoordinate start(10, 20, 0);

    WorldCoordinate result = addCoordinate(strategy, start, Coordinate(0, 10000, 0));
    EXPECT_GT(result.getLatitudeDeg(), 10.05);
    EXPECT_LT(result.getLatitudeDeg(), 10.15);
    EXPECT_NEAR(20, result.getLongitudeDeg(), 1e-6);

    result = addCoordinate(strategy, start, Coordinate(-10000, -10000, 0));
    EXPECT_LT(result.getLatitudeDeg(), 9.95);
    EXPECT_LT(result.getLongitudeDeg(), 19.95);
}

TEST(GeodeticComputationStrategy, EastIncreasesLongitudeAndNorthIncreasesLatitude)
{
    for (const GeodeticComputationStrategy strategy : kAllGeodeticComputationStrategies)
    {
        expectEastMove(strategy);
        expectNorthAndSouthWestMoves(strategy);
    }
}

TEST(GeodeticComputationStrategy, LongitudeWrapsAcrossTheDateLine)
{
    for (const GeodeticComputationStrategy strategy : kAllGeodeticComputationStrategies)
    {
        SCOPED_TRACE(toString(strategy));
        const WorldCoordinate result =
            addCoordinate(strategy, WorldCoordinate(0, 179.9, 0), Coordinate(50000, 0, 0));
        EXPECT_NEAR(0, result.getLatitudeDeg(), 1e-6);
        EXPECT_GT(result.getLongitudeDeg(), -179.7);
        EXPECT_LT(result.getLongitudeDeg(), -179.5);
    }
}

TEST(GeodeticComputationStrategy, SphericalOneDegreeNorthAndEast)
{
    constexpr double kTol = 1e-9;

    WorldCoordinate result =
        addCoordinate(GeodeticComputationStrategy::SPHERICAL, WorldCoordinate(0, 0, 0),
                      Coordinate(0, kSphereDegree, 0));
    EXPECT_NEAR(1, result.getLatitudeDeg(), kTol);
    EXPECT_NEAR(0, result.getLongitudeDeg(), kTol);

    result = addCoordinate(GeodeticComputationStrategy::SPHERICAL, WorldCoordinate(0, 0, 0),
                           Coordinate(kSphereDegree, 0, 0));
    EXPECT_NEAR(0, result.getLatitudeDeg(), kTol);
    EXPECT_NEAR(1, result.getLongitudeDeg(), kTol);

    // Due south along a meridian at any longitude.
    result = addCoordinate(GeodeticComputationStrategy::SPHERICAL, WorldCoordinate(30, -100, 0),
                           Coordinate(0, -10 * kSphereDegree, 0));
    EXPECT_NEAR(20, result.getLatitudeDeg(), kTol);
    EXPECT_NEAR(-100, result.getLongitudeDeg(), kTol);
}

TEST(GeodeticComputationStrategy, SphericalOverThePole)
{
    // Two degrees north from 89 N goes over the pole to 89 N on the opposite meridian.
    const WorldCoordinate result =
        addCoordinate(GeodeticComputationStrategy::SPHERICAL, WorldCoordinate(89, 0, 0),
                      Coordinate(0, 2 * kSphereDegree, 0));
    EXPECT_NEAR(89, result.getLatitudeDeg(), 1e-8);
    EXPECT_NEAR(180, std::abs(result.getLongitudeDeg()), 1e-8);
}

TEST(GeodeticComputationStrategy, Wgs84OneDegreeEastOnTheEquator)
{
    // The equator is a circle of the semi-major axis, so the geodesic is exact there.
    WorldCoordinate result =
        addCoordinate(GeodeticComputationStrategy::WGS84, WorldCoordinate(0, 0, 0),
                      Coordinate(kEquatorDegree, 0, 0));
    EXPECT_NEAR(0, result.getLatitudeDeg(), 1e-9);
    EXPECT_NEAR(1, result.getLongitudeDeg(), 1e-9);

    result = addCoordinate(GeodeticComputationStrategy::WGS84, WorldCoordinate(0, 100, 0),
                           Coordinate(-30 * kEquatorDegree, 0, 0));
    EXPECT_NEAR(0, result.getLatitudeDeg(), 1e-9);
    EXPECT_NEAR(70, result.getLongitudeDeg(), 1e-9);
}

TEST(GeodeticComputationStrategy, Wgs84FlindersPeakToBuninyong)
{
    // Geoscience Australia's worked example of Vincenty's formulae on GRS80, the ellipsoid
    // OpenRocket's constants describe: from Flinders Peak (37 57' 03.72030" S, 144 25'
    // 29.52440" E) 54972.271 m at azimuth 306 52' 05.37" reaches Buninyong (37 39' 10.15611" S,
    // 143 55' 35.38390" E).
    const double azimuth  = MathUtil::deg2rad(306 + (52 / 60.0) + (5.37 / 3600.0));
    const double distance = 54972.271;
    const double lat2     = -(37 + (39 / 60.0) + (10.15611 / 3600.0));
    const double lon2     = 143 + (55 / 60.0) + (35.38390 / 3600.0);

    const WorldCoordinate flinders(-(37 + (57 / 60.0) + (3.72030 / 3600.0)),
                                   144 + (25 / 60.0) + (29.52440 / 3600.0), 0);
    const Coordinate      delta(distance * std::sin(azimuth), distance * std::cos(azimuth), 0);
    const WorldCoordinate result =
        addCoordinate(GeodeticComputationStrategy::WGS84, flinders, delta);

    // 1e-8 degrees is about a millimetre.
    EXPECT_NEAR(lat2, result.getLatitudeDeg(), 1e-8);
    EXPECT_NEAR(lon2, result.getLongitudeDeg(), 1e-8);
}

TEST(GeodeticComputationStrategy, Wgs84AndSphericalAgreeClosely)
{
    // Over 10 km the sphere and the ellipsoid differ by well under a metre in latitude.
    const WorldCoordinate start(40, -75, 0);
    const Coordinate      delta(6000, 8000, 0);
    const WorldCoordinate sphere =
        addCoordinate(GeodeticComputationStrategy::SPHERICAL, start, delta);
    const WorldCoordinate ellipsoid =
        addCoordinate(GeodeticComputationStrategy::WGS84, start, delta);
    EXPECT_NEAR(sphere.getLatitudeDeg(), ellipsoid.getLatitudeDeg(), 1e-3);
    EXPECT_NEAR(sphere.getLongitudeDeg(), ellipsoid.getLongitudeDeg(), 1e-3);
    EXPECT_GT(std::abs(sphere.getLatitudeDeg() - ellipsoid.getLatitudeDeg()), 1e-7);
}

/// The NaN and infinite inputs that must make one of the curved-Earth strategies throw.
void expectNaNInputsThrow(GeodeticComputationStrategy strategy)
{
    SCOPED_TRACE(toString(strategy));
    EXPECT_TRUE(throwsBugError(strategy, WorldCoordinate(kNaN, 0, 0), Coordinate(1000, 0, 0)));
    EXPECT_TRUE(throwsBugError(strategy, WorldCoordinate(0, kNaN, 0), Coordinate(1000, 0, 0)));
    EXPECT_TRUE(throwsBugError(strategy, WorldCoordinate(10, 20, 0), Coordinate(kNaN, 0, 0)));
    EXPECT_TRUE(throwsBugError(strategy, WorldCoordinate(10, 20, 0), Coordinate(kInf, 0, 0)));
    EXPECT_TRUE(throwsBugError(strategy, WorldCoordinate(10, 20, 0), Coordinate(0, -kInf, 0)));
    // A NaN location with no movement is returned as it is (the check comes after the
    // zero-movement shortcut).
    EXPECT_FALSE(throwsBugError(strategy, WorldCoordinate(kNaN, 0, 0), Coordinate(0, 0, 5)));
}

TEST(GeodeticComputationStrategy, NaNLocationThrowsBugError)
{
    expectNaNInputsThrow(GeodeticComputationStrategy::SPHERICAL);
    expectNaNInputsThrow(GeodeticComputationStrategy::WGS84);
}

TEST(GeodeticComputationStrategy, NaNLocationErrorNamesTheInputs)
{
    const BugError    e    = catchBugError(GeodeticComputationStrategy::SPHERICAL,
                                           WorldCoordinate(0, 0, 0), Coordinate(kNaN, 0, 0));
    const std::string what = e.what();
    EXPECT_NE(what.find("addCoordinate resulted in NaN location:  location="), std::string::npos)
        << what;
    EXPECT_NE(what.find("WorldCoordinate[lat=0, lon=0, alt=0]"), std::string::npos) << what;
    // The delta's NaN is the constant quiet NaN, formatted as it is.
    EXPECT_NE(what.find("delta=(nan,0.00000,0.00000)"), std::string::npos) << what;
    // The new latitude and longitude are computed NaNs, whose spelling may carry a sign.
    EXPECT_TRUE(isNaNAfter(what, " newLat=")) << what;
    EXPECT_TRUE(isNaNAfter(what, " newLon=")) << what;
}

TEST(GeodeticComputationStrategy, NaNLocationErrorIsRaisedAtTheStrategy)
{
    // OpenRocket throws from two separate sites, one in SPHERICAL and one in WGS84; the ported
    // error must report those lines, not the shared helper's.
    const BugError spherical = catchBugError(GeodeticComputationStrategy::SPHERICAL,
                                             WorldCoordinate(0, 0, 0), Coordinate(kNaN, 0, 0));
    const BugError wgs84     = catchBugError(GeodeticComputationStrategy::WGS84,
                                             WorldCoordinate(0, 0, 0), Coordinate(kNaN, 0, 0));
    EXPECT_NE(spherical.where().line(), wgs84.where().line());
    EXPECT_TRUE(std::string_view(spherical.where().file_name())
                    .ends_with("GeodeticComputationStrategy.cpp"))
        << spherical.where().file_name();
    EXPECT_TRUE(
        std::string_view(wgs84.where().file_name()).ends_with("GeodeticComputationStrategy.cpp"))
        << wgs84.where().file_name();
    EXPECT_NE(std::string_view(spherical.what()).find("GeodeticComputationStrategy.cpp:"),
              std::string_view::npos)
        << spherical.what();
}

TEST(GeodeticComputationStrategy, Wgs84HugeDistanceGivesFiniteInRangeResult)
{
    // A distance of many Earth circumferences wraps around the ellipsoid and still gives a
    // finite location inside the constructor's ranges. (This does not reach the 100-iteration
    // cap: the direct solution's iteration is a strong contraction and converges in a few steps
    // for any finite input; only a NaN y, which exits after one iteration, stops it early.)
    const WorldCoordinate result = addCoordinate(
        GeodeticComputationStrategy::WGS84, WorldCoordinate(10, 20, 0), Coordinate(1e15, 1e15, 0));
    EXPECT_FALSE(std::isnan(result.getLatitudeDeg()));
    EXPECT_FALSE(std::isnan(result.getLongitudeDeg()));
    EXPECT_LE(std::abs(result.getLatitudeDeg()), 90.0);
    EXPECT_LE(std::abs(result.getLongitudeDeg()), 180.0);
}

}  // namespace
