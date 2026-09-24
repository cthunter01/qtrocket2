#include "QtRocket/util/Quaternion.h"

#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <numbers>
#include <sstream>

#include <gtest/gtest.h>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"

namespace
{

using QtRocket::BugError;
using QtRocket::Coordinate;
using QtRocket::Quaternion;

constexpr double kPi  = std::numbers::pi;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

void expectQuaternionNear(const Quaternion& expected, const Quaternion& actual, double eps)
{
    SCOPED_TRACE(std::format("expected {}, actual {}", expected.toString(), actual.toString()));
    EXPECT_NEAR(expected.w(), actual.w(), eps);
    EXPECT_NEAR(expected.x(), actual.x(), eps);
    EXPECT_NEAR(expected.y(), actual.y(), eps);
    EXPECT_NEAR(expected.z(), actual.z(), eps);
}

void expectCoordinateNear(const Coordinate& expected, const Coordinate& actual, double eps)
{
    SCOPED_TRACE(std::format("expected {}, actual {}", expected.toString(), actual.toString()));
    EXPECT_NEAR(expected.x, actual.x, eps);
    EXPECT_NEAR(expected.y, actual.y, eps);
    EXPECT_NEAR(expected.z, actual.z, eps);
    EXPECT_NEAR(expected.weight, actual.weight, eps);
}

// ---- Ported from QuaternionTest.java ----

TEST(Quaternion, OldMainTest)
{
    // This is normalized already
    const Quaternion q(0.237188, 0.570190, -0.514542, 0.594872);
    EXPECT_NEAR(1.0, q.norm(), 0.01);

    const Quaternion n = q.normalize();
    EXPECT_NEAR(0.237188, n.w(), 0.00001);
    EXPECT_NEAR(0.570190, n.x(), 0.00001);
    EXPECT_NEAR(-0.514542, n.y(), 0.00001);
    EXPECT_NEAR(0.594872, n.z(), 0.00001);
    EXPECT_NEAR(1.0, n.norm(), 0.01);

    // OpenRocket rotates with q as given (normalize() returned a new object it discarded).
    Coordinate c(148578428.914, 8126778.954, -607.741);

    const Coordinate r = q.rotate(c);

    EXPECT_NEAR(-42312599.537, r.x, 0.001);
    EXPECT_NEAR(-48162747.551, r.y, 0.001);
    EXPECT_NEAR(134281904.197, r.z, 0.001);

    c = Coordinate(0, 1, 0);
    const Coordinate rot(kPi / 4, 0, 0);

    c = Quaternion::rotation(rot).invRotate(c);

    EXPECT_NEAR(0.0, c.x, 0.001);
    EXPECT_NEAR(0.707, c.y, 0.001);
    EXPECT_NEAR(-0.707, c.z, 0.001);
}

TEST(Quaternion, RotationAboutAxisMatchesRotationVector)
{
    // A quarter turn about z takes the x axis onto the y axis
    const Quaternion q = Quaternion::rotation(Coordinate::kZUnit, kPi / 2);
    const Coordinate c = q.rotate(Coordinate::kXUnit);

    EXPECT_NEAR(0.0, c.x, 1.0e-12);
    EXPECT_NEAR(1.0, c.y, 1.0e-12);
    EXPECT_NEAR(0.0, c.z, 1.0e-12);

    // and must agree with the rotation vector form of the same rotation
    const Quaternion v = Quaternion::rotation(Coordinate(0, 0, kPi / 2));
    expectQuaternionNear(v, q, 1.0e-12);
}

TEST(Quaternion, RotationVectorZeroLengthReturnsIdentityQuaternion)
{
    const Quaternion q = Quaternion::rotation(Coordinate(0, 0, 0));
    EXPECT_NEAR(1.0, q.w(), 1.0e-12);
    EXPECT_NEAR(0.0, q.x(), 1.0e-12);
    EXPECT_NEAR(0.0, q.y(), 1.0e-12);
    EXPECT_NEAR(0.0, q.z(), 1.0e-12);
}

TEST(Quaternion, MultiplyWithIdentityReturnsOriginalQuaternion)
{
    const Quaternion q(0.5, -0.5, 0.25, 0.75);
    const Quaternion identity;

    const Quaternion right = q.multiplyRight(identity);
    const Quaternion left  = q.multiplyLeft(identity);

    expectQuaternionNear(q, right, 1.0e-12);
    expectQuaternionNear(q, left, 1.0e-12);
}

TEST(Quaternion, RotateAndInverseRotateReturnOriginalCoordinate)
{
    const Quaternion rotation = Quaternion::rotation(Coordinate(0, 0, kPi / 2));
    const Coordinate original(1, 0, 0);

    const Coordinate rotated  = rotation.rotate(original);
    const Coordinate restored = rotation.invRotate(rotated);

    EXPECT_NEAR(original.x, restored.x, 1.0e-12);
    EXPECT_NEAR(original.y, restored.y, 1.0e-12);
    EXPECT_NEAR(original.z, restored.z, 1.0e-12);
}

// rotateInPlaceMatchesImmutableRotation, without the MutableCoordinate: the value it checked.
TEST(Quaternion, RotationAboutXByPiOver3)
{
    const Quaternion rotation = Quaternion::rotation(Coordinate(kPi / 3, 0, 0));
    const Coordinate rotated  = rotation.rotate(Coordinate(0, 1, 0));

    EXPECT_NEAR(0.0, rotated.x, 1.0e-12);
    EXPECT_NEAR(0.5, rotated.y, 1.0e-12);
    EXPECT_NEAR(std::sqrt(3.0) / 2, rotated.z, 1.0e-12);
}

// inverseRotateInPlaceRestoresCoordinate, without the MutableCoordinate.
TEST(Quaternion, InverseRotateRestoresCoordinate)
{
    const Quaternion rotation = Quaternion::rotation(Coordinate(0, kPi / 6, 0));
    Coordinate       c(0, 0, 1);

    c = rotation.rotate(c);
    c = rotation.invRotate(c);

    EXPECT_NEAR(0.0, c.x, 1.0e-12);
    EXPECT_NEAR(0.0, c.y, 1.0e-12);
    EXPECT_NEAR(1.0, c.z, 1.0e-12);
}

TEST(Quaternion, NormalizeThrowsForZeroQuaternion)
{
    const Quaternion zero(0, 0, 0, 0);
    EXPECT_THROW(static_cast<void>(zero.normalize()), BugError);
    EXPECT_THROW(static_cast<void>(zero.normalizeIfNecessary()), BugError);
}

// normalizeIfNecessaryReturnsSameInstanceWhenAlreadyUnit. Java's assertSame checks object
// identity; a value type has none, so the closest reading is exact (operator==) value equality:
// the components come back bit for bit untouched.
TEST(Quaternion, NormalizeIfNecessaryReturnsSameValueWhenAlreadyUnit)
{
    const double     component = 1.0 / std::numbers::sqrt2;
    const Quaternion unit(component, component, 0, 0);

    const Quaternion normalized = unit.normalizeIfNecessary();
    EXPECT_TRUE(unit == normalized);  // exact: the value is passed through untouched
}

// normalizeIfNecessaryNormalizesWhenLengthDeviates: assertNotSame becomes exact value inequality.
TEST(Quaternion, NormalizeIfNecessaryNormalizesWhenLengthDeviates)
{
    const Quaternion scaled(2, 0, 0, 0);
    const Quaternion normalized = scaled.normalizeIfNecessary();

    EXPECT_FALSE(scaled == normalized);
    EXPECT_NEAR(1.0, normalized.norm(), 1.0e-12);
    EXPECT_NEAR(1.0, normalized.w(), 1.0e-12);
}

// cloneReturnsIndependentInstance: a copy of a value type. Java's assertNotSame(q, copy) has no
// counterpart (two objects are always distinct); the component equalities are kept.
TEST(Quaternion, CopyIsIndependent)
{
    const Quaternion q(0.1, 0.2, 0.3, 0.4);
    // The copy is the point of the test.
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    const Quaternion copy = q;

    EXPECT_TRUE(q == copy);
    EXPECT_EQ(q.w(), copy.w());
    EXPECT_EQ(q.x(), copy.x());
    EXPECT_EQ(q.y(), copy.y());
    EXPECT_EQ(q.z(), copy.z());
}

TEST(Quaternion, IsNaNDetectsAnyNaNComponent)
{
    EXPECT_TRUE(Quaternion(kNaN, 0, 0, 0).isNaN());
    EXPECT_TRUE(Quaternion(0, kNaN, 0, 0).isNaN());
    EXPECT_TRUE(Quaternion(0, 0, kNaN, 0).isNaN());
    EXPECT_TRUE(Quaternion(0, 0, 0, kNaN).isNaN());
    EXPECT_FALSE(Quaternion(1, 0, 0, 0).isNaN());
}

// ---- Behaviour OpenRocket does not test ----

TEST(Quaternion, DefaultIsIdentity)
{
    static_assert(Quaternion() == Quaternion(1, 0, 0, 0));
    static_assert(Quaternion().norm2() == 1.0);
    static_assert(
        Quaternion().rotate(Coordinate(1, 2, 3, 4)).exactlyEquals(Coordinate(1, 2, 3, 4)));
    static_assert(
        Quaternion().invRotate(Coordinate(1, 2, 3, 4)).exactlyEquals(Coordinate(1, 2, 3, 4)));
    static_assert(Quaternion().rotateZ().exactlyEquals(Coordinate::kZUnit));

    const Quaternion identity;
    EXPECT_NEAR(1.0, identity.norm(), 1.0e-15);
    EXPECT_TRUE(identity.normalizeIfNecessary() == identity);
}

TEST(Quaternion, HandednessIsPinned)
{
    // The standard q*v*q^-1, in numbers: a positive quarter turn about z takes +x to +y and +y to
    // -x (the right-hand rule in a right-handed frame), and the inverse rotation goes the other
    // way. OpenRocket's "left-hand rule" remark (RocketComponent.java:1658) is about how component
    // angles are applied, not about Quaternion: these signs are the Java class's.
    const Quaternion q = Quaternion::rotation(Coordinate::kZUnit, kPi / 2);
    expectCoordinateNear(Coordinate::kYUnit, q.rotate(Coordinate::kXUnit), 1.0e-12);
    expectCoordinateNear(-Coordinate::kXUnit, q.rotate(Coordinate::kYUnit), 1.0e-12);
    expectCoordinateNear(Coordinate::kZUnit, q.rotate(Coordinate::kZUnit), 1.0e-12);
    expectCoordinateNear(-Coordinate::kYUnit, q.invRotate(Coordinate::kXUnit), 1.0e-12);
    expectCoordinateNear(Coordinate::kXUnit, q.invRotate(Coordinate::kYUnit), 1.0e-12);

    // The same about x and y, from the rotation-vector form.
    const Quaternion aboutX = Quaternion::rotation(Coordinate(kPi / 2, 0, 0));
    expectCoordinateNear(Coordinate::kZUnit, aboutX.rotate(Coordinate::kYUnit), 1.0e-12);
    expectCoordinateNear(-Coordinate::kYUnit, aboutX.rotate(Coordinate::kZUnit), 1.0e-12);
    const Quaternion aboutY = Quaternion::rotation(Coordinate(0, kPi / 2, 0));
    expectCoordinateNear(Coordinate::kXUnit, aboutY.rotate(Coordinate::kZUnit), 1.0e-12);
    expectCoordinateNear(-Coordinate::kZUnit, aboutY.rotate(Coordinate::kXUnit), 1.0e-12);
}

TEST(Quaternion, RotateZMatchesRotateOfZUnit)
{
    const std::array<Quaternion, 4> quaternions = {
        Quaternion::rotation(Coordinate(0.3, -1.2, 2.5)),
        Quaternion::rotation(Coordinate::kXUnit, 1.0),
        Quaternion(0.237188, 0.570190, -0.514542, 0.594872).normalize(),
        Quaternion(),
    };
    for (const Quaternion& q : quaternions)
    {
        expectCoordinateNear(q.rotate(Coordinate::kZUnit), q.rotateZ(), 1.0e-12);
        EXPECT_NEAR(1.0, q.rotateZ().length(), 1.0e-12);
    }
}

TEST(Quaternion, RotationKeepsTheWeightAndLength)
{
    const Quaternion q = Quaternion::rotation(Coordinate(0.3, -1.2, 2.5));
    const Coordinate c(1, 2, 3, 7);

    EXPECT_EQ(7.0, q.rotate(c).weight);
    EXPECT_EQ(7.0, q.invRotate(c).weight);
    EXPECT_EQ(0.0, q.rotateZ().weight);
    EXPECT_NEAR(c.length(), q.rotate(c).length(), 1.0e-12);
    EXPECT_NEAR(c.length(), q.invRotate(c).length(), 1.0e-12);
    expectCoordinateNear(c, q.invRotate(q.rotate(c)), 1.0e-12);
    expectCoordinateNear(c, q.rotate(q.invRotate(c)), 1.0e-12);
}

TEST(Quaternion, MultiplyLeftAndRightAreTheSameProduct)
{
    const Quaternion a(0.5, -0.5, 0.25, 0.75);
    const Quaternion b(0.1, 0.9, -0.3, 0.2);

    EXPECT_TRUE(a.multiplyRight(b) == b.multiplyLeft(a));
    EXPECT_TRUE(a.multiplyLeft(b) == b.multiplyRight(a));
    // Quaternion products do not commute.
    EXPECT_FALSE(a.multiplyRight(b) == a.multiplyLeft(b));

    // Hamilton's i*j = k, j*i = -k.
    const Quaternion i(0, 1, 0, 0);
    const Quaternion j(0, 0, 1, 0);
    EXPECT_TRUE(i.multiplyRight(j) == Quaternion(0, 0, 0, 1));
    EXPECT_TRUE(i.multiplyLeft(j) == Quaternion(0, 0, 0, -1));
    static_assert(Quaternion(0, 1, 0, 0).multiplyRight(Quaternion(0, 1, 0, 0)) ==
                  Quaternion(-1, 0, 0, 0));
}

TEST(Quaternion, ProductComposesRotations)
{
    // Applying a and then b is the rotation b * a, i.e. a.multiplyLeft(b).
    const Quaternion a = Quaternion::rotation(Coordinate::kZUnit, kPi / 2);
    const Quaternion b = Quaternion::rotation(Coordinate::kXUnit, kPi / 3);
    const Coordinate v(1, 2, 3);

    const Coordinate stepwise = b.rotate(a.rotate(v));
    expectCoordinateNear(stepwise, a.multiplyLeft(b).rotate(v), 1.0e-12);
    expectCoordinateNear(stepwise, b.multiplyRight(a).rotate(v), 1.0e-12);
    EXPECT_NEAR(1.0, a.multiplyLeft(b).norm(), 1.0e-12);
}

TEST(Quaternion, RotationAboutAxisNormalizesTheAxis)
{
    const Quaternion unit   = Quaternion::rotation(Coordinate::kZUnit, kPi / 2);
    const Quaternion scaled = Quaternion::rotation(Coordinate(0, 0, 5), kPi / 2);
    expectQuaternionNear(unit, scaled, 1.0e-12);
    EXPECT_NEAR(1.0, scaled.norm(), 1.0e-12);

    EXPECT_THROW(static_cast<void>(Quaternion::rotation(Coordinate::kZero, 1.0)), BugError);
}

TEST(Quaternion, RotationVectorBelowThresholdIsIdentity)
{
    EXPECT_TRUE(Quaternion::rotation(Coordinate(5e-7, 0, 0)) == Quaternion());
    const Quaternion tiny = Quaternion::rotation(Coordinate(2e-6, 0, 0));
    EXPECT_FALSE(tiny == Quaternion());
    EXPECT_NEAR(1e-6, tiny.x(), 1.0e-15);
}

TEST(Quaternion, NormAndNorm2)
{
    const Quaternion q(1, 2, 3, 4);
    static_assert(Quaternion(1, 2, 3, 4).norm2() == 30.0);
    EXPECT_NEAR(std::sqrt(30.0), q.norm(), 1.0e-12);

    const Quaternion n = q.normalize();
    EXPECT_NEAR(1.0, n.norm(), 1.0e-12);
    EXPECT_NEAR(1.0 / std::sqrt(30.0), n.w(), 1.0e-12);
    EXPECT_NEAR(4.0 / std::sqrt(30.0), n.z(), 1.0e-12);
    // Just inside the 1 ppm band: untouched. Just outside: normalized.
    const Quaternion inside(1.0000004, 0, 0, 0);
    EXPECT_TRUE(inside.normalizeIfNecessary() == inside);
    const Quaternion outside(1.000001, 0, 0, 0);
    EXPECT_FALSE(outside.normalizeIfNecessary() == outside);
}

TEST(Quaternion, ToString)
{
    EXPECT_EQ(Quaternion().toString(),
              "Quaternion[1.000000,0.000000,0.000000,0.000000,norm=1.000000]");
    EXPECT_EQ(Quaternion(0, 3, 0, -4).toString(),
              "Quaternion[0.000000,3.000000,0.000000,-4.000000,norm=5.000000]");
}

TEST(Quaternion, ToStringDeviatesFromJavaAsDocumented)
{
    // 5e-7 as a double is just below the tie, so the exact value rounds down; Java rounds the
    // shortest decimal "5.0E-7" half up and prints 0.000001.
    EXPECT_EQ(Quaternion(5e-7, 0, 0, 0).toString(),
              "Quaternion[0.000000,0.000000,0.000000,0.000000,norm=0.000000]");
    // 2^-7 = 0.0078125 is an exact tie at six decimals: half to even here, half up in Java.
    EXPECT_EQ(Quaternion(1, 0.0078125, 0, 0).toString(),
              "Quaternion[1.000000,0.007812,0.000000,0.000000,norm=1.000031]");
}

TEST(Quaternion, StreamInsertionWritesToString)
{
    const Quaternion   q(0.5, -0.5, 0.25, 0.75);
    std::ostringstream os;
    os << q;
    EXPECT_EQ(os.str(), q.toString());
    EXPECT_EQ(::testing::PrintToString(Quaternion()),
              "Quaternion[1.000000,0.000000,0.000000,0.000000,norm=1.000000]");
}

}  // namespace
