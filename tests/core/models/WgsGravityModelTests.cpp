#include "QtRocket/models/WgsGravityModel.h"

#include <array>
#include <cmath>
#include <limits>
#include <memory>

#include <gtest/gtest.h>

#include "QtRocket/models/GravityModel.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Monitorable.h"
#include "QtRocket/util/WorldCoordinate.h"

namespace
{

using QtRocket::GravityModel;
using QtRocket::ModId;
using QtRocket::WgsGravityModel;
using QtRocket::WorldCoordinate;

static_assert(QtRocket::Monitorable<WgsGravityModel>);

/// WGSGravityModelTest.test: the gravity at a location, asked twice (Java checks its cache).
void expectGravity(const WgsGravityModel& model, double lat, double lon, double alt, double g)
{
    const WorldCoordinate wc(lat, lon, alt);
    EXPECT_NEAR(model.getGravity(wc), g, 0.001) << lat << ", " << lon << ", " << alt;
    EXPECT_NEAR(model.getGravity(wc), g, 0.001) << lat << ", " << lon << ", " << alt;
}

// ---- Ported from WGSGravityModelTest.java ----

TEST(WgsGravityModel, SurfaceGravity)
{
    const WgsGravityModel model;
    // Equator
    expectGravity(model, 0, 0, 0, 9.780);
    // Mid-latitude
    expectGravity(model, 45, 0, 0, 9.806);
    // Mid-latitude
    expectGravity(model, 45, 99, 0, 9.806);
    // South pole
    expectGravity(model, -90, 0, 0, 9.832);
}

TEST(WgsGravityModel, AltitudeEffect)
{
    const WgsGravityModel model;
    expectGravity(model, 45, 0, -100, 9.806);
    expectGravity(model, 45, 0, 0, 9.806);
    expectGravity(model, 45, 0, 10, 9.806);
    expectGravity(model, 45, 0, 100, 9.806);
    expectGravity(model, 45, 0, 1000, 9.803);
    expectGravity(model, 45, 0, 10000, 9.775);
    expectGravity(model, 45, 0, 100000, 9.505);
}

// ---- QtRocket additions ----

TEST(WgsGravityModel, MatchesOpenRocket)
{
    // WGSGravityModel.getGravity printed by OpenRocket on JDK 17. The latitude goes through
    // WorldCoordinate's degree conversion, whose rounding may differ from Java's Math.toRadians
    // in the last bit, and sin/sqrt from libm, hence the tolerance.
    struct Row
    {
        double lat;
        double lon;
        double alt;
        double g;
    };
    constexpr std::array<Row, 12> kRows{{
        {.lat = 0, .lon = 0, .alt = 0, .g = 9.7803267714},
        {.lat = 45, .lon = 0, .alt = 0, .g = 9.806199202469186},
        {.lat = 45, .lon = 99, .alt = 0, .g = 9.806199202469186},
        {.lat = -90, .lon = 0, .alt = 0, .g = 9.83218636854687},
        {.lat = 45, .lon = 0, .alt = -100, .g = 9.806507048335966},
        {.lat = 45, .lon = 0, .alt = 10, .g = 9.806168418679782},
        {.lat = 45, .lon = 0, .alt = 100, .g = 9.805891371098026},
        {.lat = 45, .lon = 0, .alt = 1000, .g = 9.803121540910272},
        {.lat = 45, .lon = 0, .alt = 10000, .g = 9.775487667293664},
        {.lat = 45, .lon = 0, .alt = 100000, .g = 9.505459630577251},
        {.lat = 28.61, .lon = -80.6, .alt = 0, .g = 9.792177302472455},
        {.lat = 90, .lon = 0, .alt = 0, .g = 9.83218636854687},
    }};
    const WgsGravityModel         model;
    for (const Row& row : kRows)
    {
        EXPECT_NEAR(model.getGravity(WorldCoordinate(row.lat, row.lon, row.alt)), row.g, 1e-13)
            << row.lat << ", " << row.lon << ", " << row.alt;
    }
}

TEST(WgsGravityModel, EquatorAtSeaLevelIsTheNormalGravity)
{
    // sin(0) = 0 exactly: Somigliana's constant itself.
    const WgsGravityModel model;
    EXPECT_EQ(model.getGravity(WorldCoordinate(0, 123, 0)), 9.7803267714);
}

TEST(WgsGravityModel, InverseSquareWithAltitude)
{
    const WgsGravityModel model;
    const double          surface = model.getGravity(WorldCoordinate(30, 0, 0));
    const double          high = model.getGravity(WorldCoordinate(30, 0, WorldCoordinate::kRearth));
    // One Earth radius up: a quarter of the surface gravity.
    EXPECT_NEAR(high, surface / 4, 1e-15);
    // Symmetric in latitude, independent of longitude.
    EXPECT_DOUBLE_EQ(model.getGravity(WorldCoordinate(-30, 50, 0)), surface);
}

TEST(WgsGravityModel, NaNAltitudeGivesNaN)
{
    const WgsGravityModel model;
    EXPECT_TRUE(std::isnan(
        model.getGravity(WorldCoordinate(0, 0, std::numeric_limits<double>::quiet_NaN()))));
}

TEST(WgsGravityModel, WorksThroughTheInterface)
{
    const std::unique_ptr<GravityModel> model = std::make_unique<WgsGravityModel>();
    EXPECT_NEAR(model->getGravity(WorldCoordinate(45, 0, 0)), 9.806199202469186, 1e-13);
    EXPECT_EQ(model->modId(), ModId::zero());
}

}  // namespace
