#include "QtRocket/util/WorldCoordinate.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <limits>
#include <numbers>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "QtRocket/util/MathUtil.h"

namespace
{

using QtRocket::WorldCoordinate;
namespace MathUtil = QtRocket::MathUtil;

constexpr double kEps = 1.0e-10;
constexpr double kPi  = std::numbers::pi;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// ---- Ported from WorldCoordinateTest.java ----

TEST(WorldCoordinate, Constructor)
{
    WorldCoordinate wc(10, 15, 130);
    EXPECT_NEAR(10, wc.getLatitudeDeg(), kEps);
    EXPECT_NEAR(15, wc.getLongitudeDeg(), kEps);
    EXPECT_EQ(130, wc.getAltitude());

    wc = WorldCoordinate(100, 190, 13000);
    EXPECT_NEAR(90, wc.getLatitudeDeg(), kEps);
    EXPECT_NEAR(-170, wc.getLongitudeDeg(), kEps);
    EXPECT_EQ(13000, wc.getAltitude());

    wc = WorldCoordinate(-100, -200, -13000);
    EXPECT_NEAR(-90, wc.getLatitudeDeg(), kEps);
    EXPECT_NEAR(160, wc.getLongitudeDeg(), kEps);
    EXPECT_EQ(-13000, wc.getAltitude());
}

TEST(WorldCoordinate, GetLatitude)
{
    const WorldCoordinate wc(10, 15, 130);
    EXPECT_NEAR(10, wc.getLatitudeDeg(), kEps);
    EXPECT_NEAR(10 * kPi / 180, wc.getLatitudeRad(), kEps);
}

TEST(WorldCoordinate, GetLongitude)
{
    const WorldCoordinate wc(10, 15, 130);
    EXPECT_NEAR(15, wc.getLongitudeDeg(), kEps);
    EXPECT_NEAR(15 * kPi / 180, wc.getLongitudeRad(), kEps);
}

// ---- Additional tests ----

TEST(WorldCoordinate, Constants)
{
    EXPECT_EQ(6371000.0, WorldCoordinate::kRearth);
    EXPECT_EQ(7.2921150e-5, WorldCoordinate::kErot);
}

TEST(WorldCoordinate, LatitudeIsClampedNotWrapped)
{
    EXPECT_NEAR(90, WorldCoordinate(90.5, 0, 0).getLatitudeDeg(), kEps);
    EXPECT_NEAR(90, WorldCoordinate(1e6, 0, 0).getLatitudeDeg(), kEps);
    EXPECT_NEAR(-90, WorldCoordinate(-90.5, 0, 0).getLatitudeDeg(), kEps);
    EXPECT_NEAR(-90, WorldCoordinate(-1e6, 0, 0).getLatitudeDeg(), kEps);
    EXPECT_NEAR(90, WorldCoordinate(kInf, 0, 0).getLatitudeDeg(), kEps);
    EXPECT_NEAR(-90, WorldCoordinate(-kInf, 0, 0).getLatitudeDeg(), kEps);
    // Clamping happens in radians, to exactly +-pi/2.
    EXPECT_EQ(kPi / 2, WorldCoordinate(100, 0, 0).getLatitudeRad());
    EXPECT_EQ(-kPi / 2, WorldCoordinate(-100, 0, 0).getLatitudeRad());
    // Values inside the range are untouched.
    EXPECT_NEAR(89.999, WorldCoordinate(89.999, 0, 0).getLatitudeDeg(), kEps);
    EXPECT_NEAR(-89.999, WorldCoordinate(-89.999, 0, 0).getLatitudeDeg(), kEps);
    EXPECT_EQ(0, WorldCoordinate(0, 0, 0).getLatitudeRad());
}

TEST(WorldCoordinate, LongitudeIsReducedIntoHalfTurn)
{
    EXPECT_NEAR(-179, WorldCoordinate(0, 181, 0).getLongitudeDeg(), kEps);
    EXPECT_NEAR(179, WorldCoordinate(0, -181, 0).getLongitudeDeg(), kEps);
    EXPECT_NEAR(0, WorldCoordinate(0, 360, 0).getLongitudeDeg(), kEps);
    EXPECT_NEAR(0, WorldCoordinate(0, -720, 0).getLongitudeDeg(), kEps);
    EXPECT_NEAR(-10, WorldCoordinate(0, 350, 0).getLongitudeDeg(), kEps);
    EXPECT_NEAR(5, WorldCoordinate(0, 1085, 0).getLongitudeDeg(), kEps);  // three turns and 5
    EXPECT_NEAR(-5, WorldCoordinate(0, -1085, 0).getLongitudeDeg(), kEps);
    EXPECT_NEAR(179.5, WorldCoordinate(0, 179.5, 0).getLongitudeDeg(), kEps);
    EXPECT_NEAR(-179.5, WorldCoordinate(0, -179.5, 0).getLongitudeDeg(), kEps);
    // Exactly on the boundary either end may come back (reducePi rounds half to even), but the
    // magnitude is a half turn.
    EXPECT_NEAR(180, std::abs(WorldCoordinate(0, 180, 0).getLongitudeDeg()), kEps);
    EXPECT_NEAR(180, std::abs(WorldCoordinate(0, -180, 0).getLongitudeDeg()), kEps);
    EXPECT_NEAR(180, std::abs(WorldCoordinate(0, 540, 0).getLongitudeDeg()), kEps);
    EXPECT_NEAR(kPi, std::abs(WorldCoordinate(0, 180, 0).getLongitudeRad()), kEps);
}

TEST(WorldCoordinate, AltitudeIsUnbounded)
{
    EXPECT_EQ(-1e9, WorldCoordinate(0, 0, -1e9).getAltitude());
    EXPECT_EQ(1e12, WorldCoordinate(0, 0, 1e12).getAltitude());
    EXPECT_EQ(kInf, WorldCoordinate(0, 0, kInf).getAltitude());
    EXPECT_EQ(-kInf, WorldCoordinate(0, 0, -kInf).getAltitude());
    EXPECT_TRUE(std::isnan(WorldCoordinate(0, 0, kNaN).getAltitude()));
}

TEST(WorldCoordinate, NaNAnglesPassThrough)
{
    const WorldCoordinate wc(kNaN, kNaN, 0);
    EXPECT_TRUE(std::isnan(wc.getLatitudeRad()));
    EXPECT_TRUE(std::isnan(wc.getLatitudeDeg()));
    EXPECT_TRUE(std::isnan(wc.getLongitudeRad()));
    EXPECT_TRUE(std::isnan(wc.getLongitudeDeg()));
    EXPECT_EQ(0, wc.getAltitude());
    // An infinite longitude reduces to inf - inf.
    EXPECT_TRUE(std::isnan(WorldCoordinate(0, kInf, 0).getLongitudeRad()));
    EXPECT_TRUE(std::isnan(WorldCoordinate(0, -kInf, 0).getLongitudeDeg()));
}

TEST(WorldCoordinate, RadiansAndDegreesAgree)
{
    const WorldCoordinate wc(-33.5, 151.25, 12.5);
    EXPECT_NEAR(MathUtil::deg2rad(-33.5), wc.getLatitudeRad(), kEps);
    EXPECT_NEAR(MathUtil::deg2rad(151.25), wc.getLongitudeRad(), kEps);
    EXPECT_NEAR(-33.5, wc.getLatitudeDeg(), kEps);
    EXPECT_NEAR(151.25, wc.getLongitudeDeg(), kEps);
    EXPECT_NEAR(MathUtil::rad2deg(wc.getLatitudeRad()), wc.getLatitudeDeg(), kEps);
    EXPECT_NEAR(MathUtil::rad2deg(wc.getLongitudeRad()), wc.getLongitudeDeg(), kEps);
    EXPECT_EQ(12.5, wc.getAltitude());
}

TEST(WorldCoordinate, CopiesAreIndependentValues)
{
    const WorldCoordinate original(10, 20, 30);
    WorldCoordinate       copy = original;
    EXPECT_EQ(original, copy);
    copy = WorldCoordinate(11, 20, 30);
    EXPECT_NEAR(10, original.getLatitudeDeg(), kEps);
    EXPECT_NEAR(11, copy.getLatitudeDeg(), kEps);
}

TEST(WorldCoordinate, EqualsIsTolerant)
{
    const WorldCoordinate a(10, 20, 30);
    EXPECT_TRUE(a == a);
    EXPECT_FALSE(a != a);
    EXPECT_TRUE(a == WorldCoordinate(10, 20, 30));
    // Within MathUtil::kEpsilon relative
    EXPECT_TRUE(a == WorldCoordinate(10 * (1 + 1e-9), 20, 30));
    EXPECT_TRUE(a == WorldCoordinate(10, 20 * (1 + 1e-9), 30));
    EXPECT_TRUE(a == WorldCoordinate(10, 20, 30 * (1 + 1e-9)));
    // Outside it
    EXPECT_FALSE(a == WorldCoordinate(10.001, 20, 30));
    EXPECT_FALSE(a == WorldCoordinate(10, 20.001, 30));
    EXPECT_FALSE(a == WorldCoordinate(10, 20, 30.001));
    EXPECT_TRUE(a != WorldCoordinate(10, 20, 30.001));
    // The constructor's reduction and clamping happen first
    EXPECT_TRUE(WorldCoordinate(0, 370, 0) == WorldCoordinate(0, 10, 0));
    EXPECT_TRUE(WorldCoordinate(95, 0, 0) == WorldCoordinate(90, 0, 0));
    // Zero altitude compares absolutely (within kEpsilon / 2)
    EXPECT_TRUE(WorldCoordinate(0, 0, 0) == WorldCoordinate(0, 0, 1e-9));
    EXPECT_FALSE(WorldCoordinate(0, 0, 0) == WorldCoordinate(0, 0, 1e-8));
}

TEST(WorldCoordinate, EqualsIsNeverTrueForNaN)
{
    EXPECT_FALSE(WorldCoordinate(kNaN, 0, 0) == WorldCoordinate(kNaN, 0, 0));
    EXPECT_FALSE(WorldCoordinate(0, kNaN, 0) == WorldCoordinate(0, kNaN, 0));
    EXPECT_FALSE(WorldCoordinate(0, 0, kNaN) == WorldCoordinate(0, 0, kNaN));
    EXPECT_FALSE(WorldCoordinate(0, 0, kNaN) == WorldCoordinate(0, 0, 0));
}

TEST(WorldCoordinate, ToString)
{
    // 0 degrees converts to and from radians exactly, so these are pinned digit for digit.
    EXPECT_EQ("WorldCoordinate[lat=0, lon=0, alt=130]", WorldCoordinate(0, 0, 130).toString());
    EXPECT_EQ("WorldCoordinate[lat=0, lon=0, alt=-2.5]", WorldCoordinate(0, 0, -2.5).toString());
    EXPECT_EQ("WorldCoordinate[lat=0, lon=0, alt=nan]", WorldCoordinate(0, 0, kNaN).toString());
    EXPECT_EQ("WorldCoordinate[lat=0, lon=0, alt=inf]", WorldCoordinate(0, 0, kInf).toString());

    // 10 and 20 degrees also survive the round trip through radians exactly.
    EXPECT_EQ("WorldCoordinate[lat=10, lon=20, alt=12.5]",
              WorldCoordinate(10, 20, 12.5).toString());

    // Other angles are whatever the degree getters give (-33.5 comes back as -33.49999...), in
    // the shortest round-trip digits.
    const WorldCoordinate wc(-33.5, 151.25, 12.5);
    EXPECT_EQ(std::format("WorldCoordinate[lat={}, lon={}, alt={}]", wc.getLatitudeDeg(),
                          wc.getLongitudeDeg(), wc.getAltitude()),
              wc.toString());
    EXPECT_TRUE(wc.toString().starts_with("WorldCoordinate[lat=-33.4"));
    EXPECT_TRUE(wc.toString().ends_with(", alt=12.5]"));
}

TEST(WorldCoordinate, HashFollowsOpenRocket)
{
    const std::hash<WorldCoordinate> hash;

    // (int)(1000 * (lat + lon + alt)) with the angles in radians: 0 + 0 + 130 m.
    EXPECT_EQ(hash(WorldCoordinate(0, 0, 130)), static_cast<std::size_t>(130000));
    EXPECT_EQ(hash(WorldCoordinate(0, 0, -2.5)), static_cast<std::size_t>(-2500));
    // 90 N is pi/2 rad: 1000 * (pi/2 + 0 + 0) truncates to 1570.
    EXPECT_EQ(hash(WorldCoordinate(90, 0, 0)), static_cast<std::size_t>(1570));
    // -170 E is -2.967 rad: 1000 * (0 - 2.967 + 0) truncates to -2967.
    EXPECT_EQ(hash(WorldCoordinate(0, -170, 0)), static_cast<std::size_t>(-2967));
    // The sum is hashed, not the parts.
    EXPECT_EQ(hash(WorldCoordinate(90, 0, 2)), hash(WorldCoordinate(0, 90, 2)));
    // Values below a thousandth do not register.
    EXPECT_EQ(hash(WorldCoordinate(0, 0, 0.0009)), static_cast<std::size_t>(0));
    // The constructor's clamping and reduction happen first.
    EXPECT_EQ(hash(WorldCoordinate(100, 0, 0)), hash(WorldCoordinate(90, 0, 0)));
    EXPECT_EQ(hash(WorldCoordinate(0, 370, 0)), hash(WorldCoordinate(0, 10, 0)));
    // Java's (int) cast: NaN is 0 and huge values saturate instead of being undefined.
    EXPECT_EQ(hash(WorldCoordinate(kNaN, 0, 0)), static_cast<std::size_t>(0));
    EXPECT_EQ(hash(WorldCoordinate(0, 0, kNaN)), static_cast<std::size_t>(0));
    EXPECT_EQ(hash(WorldCoordinate(0, 0, kInf)),
              static_cast<std::size_t>(std::numeric_limits<int>::max()));
    EXPECT_EQ(hash(WorldCoordinate(0, 0, -1e12)),
              static_cast<std::size_t>(std::numeric_limits<int>::min()));
    // Equal coordinates hash equally when they do not straddle a thousandth.
    EXPECT_EQ(hash(WorldCoordinate(10, 20, 30)), hash(WorldCoordinate(10, 20, 30 * (1 + 1e-9))));
}

TEST(WorldCoordinate, StreamOperatorWritesToString)
{
    const WorldCoordinate wc(0, 0, 42);
    std::ostringstream    out;
    out << wc;
    EXPECT_EQ(wc.toString(), out.str());
}

}  // namespace
