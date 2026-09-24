#include "QtRocket/util/Rotation2D.h"

#include <numbers>

#include <gtest/gtest.h>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Quaternion.h"

namespace
{

using QtRocket::Coordinate;
using QtRocket::Quaternion;
using QtRocket::Rotation2D;

// Rotation2DTest.rotationTest. Java's assertEquals(Coordinate, Coordinate) is Coordinate.equals(),
// the tolerant comparison that Coordinate::operator== ports, so EXPECT_EQ is the same check.
TEST(Rotation2D, RotationTest)
{
    const double rot60 = 0.5;
    const double rot30 = std::numbers::sqrt3 / 2;

    const Coordinate x{1, 1, 0};
    const Coordinate y{0, 1, 1};

    const Rotation2D rot(std::numbers::pi / 3);  // 60 deg

    EXPECT_EQ(Coordinate(rot60, 1, -rot30), rot.rotateY(x));
    EXPECT_EQ(Coordinate(rot60, 1, rot30), rot.invRotateY(x));

    EXPECT_EQ(Coordinate(1, rot60, rot30), rot.rotateX(x));
    EXPECT_EQ(Coordinate(1, rot60, -rot30), rot.invRotateX(x));

    EXPECT_EQ(Coordinate(-rot30, rot60, 1), rot.rotateZ(y));
    EXPECT_EQ(Coordinate(rot30, rot60, 1), rot.invRotateZ(y));
}

TEST(Rotation2D, AngleConstructorStoresSineAndCosine)
{
    const Rotation2D quarter(std::numbers::pi / 2);
    EXPECT_DOUBLE_EQ(quarter.sin(), 1.0);
    EXPECT_NEAR(quarter.cos(), 0.0, 1e-15);

    const Rotation2D zero(0.0);
    EXPECT_DOUBLE_EQ(zero.sin(), 0.0);
    EXPECT_DOUBLE_EQ(zero.cos(), 1.0);
}

TEST(Rotation2D, SineCosineConstructorAndRotationsAreConstexpr)
{
    constexpr Rotation2D kRot{0.6, 0.8};
    static_assert(kRot.sin() == 0.6);
    static_assert(kRot.cos() == 0.8);
    static_assert(Rotation2D::identity().sin() == 0.0);
    static_assert(Rotation2D::identity().cos() == 1.0);

    constexpr Coordinate kTurned = Rotation2D::identity().rotateZ(Coordinate{1, 2, 3, 4});
    static_assert(kTurned.exactlyEquals(Coordinate{1, 2, 3, 4}));
    // Coordinate::operator== is tolerant and not constexpr, so the round trip is checked at run
    // time; the value itself is still computed at compile time.
    constexpr Coordinate kBack = kRot.invRotateZ(kRot.rotateZ(Coordinate{1, 0, 0}));
    EXPECT_EQ(kBack, Coordinate(1, 0, 0));
}

TEST(Rotation2D, IdentityLeavesCoordinatesAlone)
{
    const Coordinate c{1.5, -2.5, 3.5, 0.25};
    const Rotation2D id = Rotation2D::identity();
    EXPECT_TRUE(id.rotateX(c).exactlyEquals(c));
    EXPECT_TRUE(id.rotateY(c).exactlyEquals(c));
    EXPECT_TRUE(id.rotateZ(c).exactlyEquals(c));
    EXPECT_TRUE(id.invRotateX(c).exactlyEquals(c));
    EXPECT_TRUE(id.invRotateY(c).exactlyEquals(c));
    EXPECT_TRUE(id.invRotateZ(c).exactlyEquals(c));
}

TEST(Rotation2D, QuarterTurnsFollowTheRightHandRule)
{
    const Rotation2D quarter(std::numbers::pi / 2);
    EXPECT_EQ(quarter.rotateZ(Coordinate::kXUnit), Coordinate::kYUnit);
    EXPECT_EQ(quarter.rotateX(Coordinate::kYUnit), Coordinate::kZUnit);
    EXPECT_EQ(quarter.rotateY(Coordinate::kZUnit), Coordinate::kXUnit);
    EXPECT_EQ(quarter.invRotateZ(Coordinate::kYUnit), Coordinate::kXUnit);
    EXPECT_EQ(quarter.invRotateX(Coordinate::kZUnit), Coordinate::kYUnit);
    EXPECT_EQ(quarter.invRotateY(Coordinate::kXUnit), Coordinate::kZUnit);
}

TEST(Rotation2D, AgreesWithQuaternionAboutEachAxis)
{
    // The simulation mixes both: the theta rotation is a Rotation2D, the orientation a
    // Quaternion, so they must turn the same way.
    const Coordinate c{0.3, -1.2, 2.7, 0.75};
    const double     angle = 1.234;
    const Rotation2D rot(angle);
    EXPECT_EQ(rot.rotateX(c), Quaternion::rotation(Coordinate::kXUnit, angle).rotate(c));
    EXPECT_EQ(rot.rotateY(c), Quaternion::rotation(Coordinate::kYUnit, angle).rotate(c));
    EXPECT_EQ(rot.rotateZ(c), Quaternion::rotation(Coordinate::kZUnit, angle).rotate(c));
    EXPECT_EQ(rot.invRotateX(c), Quaternion::rotation(Coordinate::kXUnit, angle).invRotate(c));
    EXPECT_EQ(rot.invRotateY(c), Quaternion::rotation(Coordinate::kYUnit, angle).invRotate(c));
    EXPECT_EQ(rot.invRotateZ(c), Quaternion::rotation(Coordinate::kZUnit, angle).invRotate(c));
}

TEST(Rotation2D, InverseUndoesTheRotation)
{
    const Coordinate c{0.3, -1.2, 2.7, 0.75};
    const Rotation2D rot(1.234);
    EXPECT_EQ(rot.invRotateX(rot.rotateX(c)), c);
    EXPECT_EQ(rot.invRotateY(rot.rotateY(c)), c);
    EXPECT_EQ(rot.invRotateZ(rot.rotateZ(c)), c);
    EXPECT_EQ(rot.rotateX(rot.invRotateX(c)), c);
    EXPECT_EQ(rot.rotateY(rot.invRotateY(c)), c);
    EXPECT_EQ(rot.rotateZ(rot.invRotateZ(c)), c);
}

TEST(Rotation2D, WeightIsCarriedThroughUnchanged)
{
    const Coordinate c{0.3, -1.2, 2.7, 0.75};
    const Rotation2D rot(-2.5);
    for (const Coordinate& r : {rot.rotateX(c), rot.rotateY(c), rot.rotateZ(c), rot.invRotateX(c),
                                rot.invRotateY(c), rot.invRotateZ(c)})
    {
        EXPECT_EQ(r.weight, 0.75);
    }
    EXPECT_EQ(rot.rotateZ(Coordinate{1, 2, 3}).weight, 0.0);
}

TEST(Rotation2D, RotationsPreserveLength)
{
    const Coordinate c{0.3, -1.2, 2.7, 1.0};
    const Rotation2D rot(-2.5);
    for (const Coordinate& r : {rot.rotateX(c), rot.rotateY(c), rot.rotateZ(c)})
    {
        EXPECT_NEAR(r.length(), c.length(), 1e-12);
    }
}

TEST(Rotation2D, UnitVectorConstructorTurnsTheXAxisOntoIt)
{
    // The simulation builds the theta rotation from the airspeed direction as (y / len, x / len).
    const Rotation2D rot(0.6, 0.8);
    EXPECT_EQ(rot.rotateZ(Coordinate::kXUnit), Coordinate(0.8, 0.6, 0));
    EXPECT_EQ(rot.invRotateZ(Coordinate(0.8, 0.6, 0)), Coordinate::kXUnit);
}

}  // namespace
