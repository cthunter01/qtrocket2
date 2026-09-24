#include "QtRocket/util/Rotation2D.h"

#include <cmath>
#include <numbers>

#include <gtest/gtest.h>

namespace
{

using QtRocket::Rotation2D;

/// Stands in for Coordinate: the same public fields and four-double construction. A test against
/// Coordinate itself is pending that class's port (its constructor order x, y, z, weight is what
/// the WeightedCoordinate concept assumes and cannot check).
struct TestCoordinate
{
    constexpr TestCoordinate(double px, double py, double pz, double w) noexcept
      : x(px), y(py), z(pz), weight(w)
    {
    }

    double x;
    double y;
    double z;
    double weight;
};

struct Unweighted
{
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

static_assert(QtRocket::WeightedCoordinate<TestCoordinate>);
static_assert(!QtRocket::WeightedCoordinate<Unweighted>);
static_assert(!QtRocket::WeightedCoordinate<double>);

// Coordinate.equals compares each component with MathUtil.equals(a, b, MathUtil.EPSILON = 1e-8):
// |a - b| < EPSILON * |b|, or |a| < EPSILON / 2 when |b| < EPSILON / 2.
constexpr double kEpsilon = 1e-8;

void expectMathUtilEquals(double actual, double expected, const char* component)
{
    const double absExpected = std::abs(expected);
    if (absExpected < kEpsilon / 2)
    {
        EXPECT_LT(std::abs(actual), kEpsilon / 2) << component << " = " << actual;
    }
    else
    {
        EXPECT_LT(std::abs(actual - expected), kEpsilon * absExpected)
            << component << " = " << actual << ", expected " << expected;
    }
}

void expectCoordinate(const TestCoordinate& actual, double x, double y, double z,
                      double weight = 0.0)
{
    expectMathUtilEquals(actual.x, x, "x");
    expectMathUtilEquals(actual.y, y, "y");
    expectMathUtilEquals(actual.z, z, "z");
    expectMathUtilEquals(actual.weight, weight, "weight");
}

// Rotation2DTest.rotationTest
TEST(Rotation2D, RotationTest)
{
    const double rot60 = 0.5;
    const double rot30 = std::numbers::sqrt3 / 2;

    const TestCoordinate x{1, 1, 0, 0};
    const TestCoordinate y{0, 1, 1, 0};

    const Rotation2D rot(std::numbers::pi / 3);  // 60 deg

    expectCoordinate(rot.rotateY(x), rot60, 1, -rot30);
    expectCoordinate(rot.invRotateY(x), rot60, 1, rot30);

    expectCoordinate(rot.rotateX(x), 1, rot60, rot30);
    expectCoordinate(rot.invRotateX(x), 1, rot60, -rot30);

    expectCoordinate(rot.rotateZ(y), -rot30, rot60, 1);
    expectCoordinate(rot.invRotateZ(y), rot30, rot60, 1);
}

TEST(Rotation2D, AngleConstructorStoresSineAndCosine)
{
    const Rotation2D quarter(std::numbers::pi / 2);
    EXPECT_NEAR(quarter.sin(), 1.0, kEpsilon);
    EXPECT_NEAR(quarter.cos(), 0.0, kEpsilon);

    const Rotation2D zero(0.0);
    EXPECT_DOUBLE_EQ(zero.sin(), 0.0);
    EXPECT_DOUBLE_EQ(zero.cos(), 1.0);
}

TEST(Rotation2D, SineCosineConstructorIsConstexpr)
{
    constexpr Rotation2D kRot{0.6, 0.8};
    static_assert(kRot.sin() == 0.6);
    static_assert(kRot.cos() == 0.8);
    static_assert(Rotation2D::identity().sin() == 0.0);
    static_assert(Rotation2D::identity().cos() == 1.0);

    constexpr TestCoordinate kTurned = Rotation2D::identity().rotateZ(TestCoordinate{1, 2, 3, 4});
    static_assert(kTurned.x == 1.0 && kTurned.y == 2.0 && kTurned.z == 3.0 &&
                  kTurned.weight == 4.0);
}

TEST(Rotation2D, IdentityLeavesCoordinatesAlone)
{
    const TestCoordinate c{1.5, -2.5, 3.5, 0.25};
    const Rotation2D     id = Rotation2D::identity();
    expectCoordinate(id.rotateX(c), 1.5, -2.5, 3.5, 0.25);
    expectCoordinate(id.rotateY(c), 1.5, -2.5, 3.5, 0.25);
    expectCoordinate(id.rotateZ(c), 1.5, -2.5, 3.5, 0.25);
    expectCoordinate(id.invRotateX(c), 1.5, -2.5, 3.5, 0.25);
    expectCoordinate(id.invRotateY(c), 1.5, -2.5, 3.5, 0.25);
    expectCoordinate(id.invRotateZ(c), 1.5, -2.5, 3.5, 0.25);
}

TEST(Rotation2D, InverseUndoesTheRotationAndKeepsTheWeight)
{
    const TestCoordinate c{0.3, -1.2, 2.7, 0.75};
    const Rotation2D     rot(1.234);
    expectCoordinate(rot.invRotateX(rot.rotateX(c)), 0.3, -1.2, 2.7, 0.75);
    expectCoordinate(rot.invRotateY(rot.rotateY(c)), 0.3, -1.2, 2.7, 0.75);
    expectCoordinate(rot.invRotateZ(rot.rotateZ(c)), 0.3, -1.2, 2.7, 0.75);
    expectCoordinate(rot.rotateX(rot.invRotateX(c)), 0.3, -1.2, 2.7, 0.75);
    expectCoordinate(rot.rotateY(rot.invRotateY(c)), 0.3, -1.2, 2.7, 0.75);
    expectCoordinate(rot.rotateZ(rot.invRotateZ(c)), 0.3, -1.2, 2.7, 0.75);
}

TEST(Rotation2D, RotationsPreserveLength)
{
    const TestCoordinate c{0.3, -1.2, 2.7, 1.0};
    const double         length = std::sqrt((c.x * c.x) + (c.y * c.y) + (c.z * c.z));
    const Rotation2D     rot(-2.5);
    for (const TestCoordinate& r : {rot.rotateX(c), rot.rotateY(c), rot.rotateZ(c)})
    {
        EXPECT_NEAR(std::sqrt((r.x * r.x) + (r.y * r.y) + (r.z * r.z)), length, kEpsilon);
    }
}

TEST(Rotation2D, UnitVectorConstructorTurnsTheXAxisOntoIt)
{
    // The simulation builds the theta rotation from the airspeed direction as (y / len, x / len).
    const Rotation2D     rot(0.6, 0.8);
    const TestCoordinate xAxis{1, 0, 0, 0};
    expectCoordinate(rot.rotateZ(xAxis), 0.8, 0.6, 0);
    expectCoordinate(rot.invRotateZ(TestCoordinate{0.8, 0.6, 0, 0}), 1, 0, 0);
}

}  // namespace
