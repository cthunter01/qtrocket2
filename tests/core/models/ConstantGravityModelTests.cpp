#include "QtRocket/models/ConstantGravityModel.h"

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

using QtRocket::ConstantGravityModel;
using QtRocket::GravityModel;
using QtRocket::ModId;
using QtRocket::WorldCoordinate;

static_assert(QtRocket::Monitorable<ConstantGravityModel>);

// ---- Ported from ConstantGravityModelTest.java ----

TEST(ConstantGravityModel, ReturnsTheSameValueEverywhere)
{
    const double               expectedGravity = 9.807;
    const ConstantGravityModel model(expectedGravity);

    // Various locations - always the same value
    const WorldCoordinate coord1(0, 0, 0);
    const WorldCoordinate coord2(45, 90, 1000);
    const WorldCoordinate coord3(-45, -90, 5000);

    EXPECT_NEAR(model.getGravity(coord1), expectedGravity, 1e-6);
    EXPECT_NEAR(model.getGravity(coord2), expectedGravity, 1e-6);
    EXPECT_NEAR(model.getGravity(coord3), expectedGravity, 1e-6);
}

TEST(ConstantGravityModel, GetConstantGravity)
{
    const double               expectedGravity = 10.5;
    const ConstantGravityModel model(expectedGravity);
    EXPECT_NEAR(model.getConstantGravity(), expectedGravity, 1e-6);
}

TEST(ConstantGravityModel, ZeroGravity)
{
    const ConstantGravityModel model(0.0);
    const WorldCoordinate      coord(0, 0, 0);
    EXPECT_NEAR(model.getGravity(coord), 0.0, 1e-6);
}

// ---- QtRocket additions ----

TEST(ConstantGravityModel, WorksThroughTheInterface)
{
    const std::unique_ptr<GravityModel> model = std::make_unique<ConstantGravityModel>(3.71);
    EXPECT_EQ(model->getGravity(WorldCoordinate(10, 20, 1e6)), 3.71);
    EXPECT_EQ(model->modId(), ModId::zero());
}

TEST(ConstantGravityModel, AnyValueIsAccepted)
{
    // A record without validation: negative and NaN values pass through.
    EXPECT_EQ(ConstantGravityModel(-9.8).getGravity(WorldCoordinate(0, 0, 0)), -9.8);
    const ConstantGravityModel nanModel(std::numeric_limits<double>::quiet_NaN());
    EXPECT_TRUE(std::isnan(nanModel.getGravity(WorldCoordinate(0, 0, 0))));
    EXPECT_TRUE(std::isnan(nanModel.gravity()));
}

TEST(ConstantGravityModel, RecordEqualityHashAndString)
{
    const ConstantGravityModel model(9.807);
    EXPECT_EQ(model.gravity(), 9.807);
    EXPECT_TRUE(model == ConstantGravityModel(9.807));
    EXPECT_FALSE(model == ConstantGravityModel(9.8070001));
    // Double.compare: 0.0 and -0.0 differ, NaN equals NaN.
    EXPECT_FALSE(ConstantGravityModel(-0.0) == ConstantGravityModel(0.0));
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(ConstantGravityModel(nan) == ConstantGravityModel(nan));

    // Printed by OpenRocket on JDK 17.
    EXPECT_EQ(model.toString(), "ConstantGravityModel[gravity=9.807]");
    EXPECT_EQ(model.hashCode(), 1522279256);
    EXPECT_EQ(ConstantGravityModel(0.0).hashCode(), 0);
    EXPECT_EQ(ConstantGravityModel(10.0).toString(), "ConstantGravityModel[gravity=10.0]");
}

TEST(ConstantGravityModel, ModIdIsZero)
{
    EXPECT_EQ(ConstantGravityModel(9.807).modId(), ModId::zero());
}

}  // namespace
