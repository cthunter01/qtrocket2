#include "QtRocket/mass/RigidBody.h"

#include <cmath>
#include <limits>

#include <gtest/gtest.h>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace
{

namespace MathUtil = QtRocket::MathUtil;
using QtRocket::BugError;
using QtRocket::Coordinate;
using QtRocket::RigidBody;

// tolerance for compared double test results
constexpr double kEpsilon = MathUtil::kEpsilon;

// ---- Ported from RigidBodyTest.java ----

TEST(RigidBody, EqualityIncludesMassAndCenterOfMass)
{
    const RigidBody reference(Coordinate(1, 2, 3, 4), 5, 6, 7);
    const RigidBody differentMass(Coordinate(1, 2, 3, 8), 5, 6, 7);
    const RigidBody differentCenter(Coordinate(2, 2, 3, 4), 5, 6, 7);

    EXPECT_FALSE(reference == differentMass);
    EXPECT_FALSE(reference == differentCenter);
}

TEST(RigidBody, TwoPointInline)
{
    const double     m1 = 2.5;
    const Coordinate r1(0, -40, 0, m1);
    const double     i1ax = 28.7;
    const double     i1t  = i1ax / 2;
    const RigidBody  body1(r1, i1ax, i1t);

    const double     m2 = 5.7;
    const Coordinate r2(0, 32, 0, m2);
    const double     i2ax = 20;
    const double     i2t  = i2ax / 2;
    const RigidBody  body2(r2, i2ax, i2t);

    // point 3 is defined as the CM of bodies 1 and 2 combined.
    const RigidBody asbly3 = body1.add(body2);

    const Coordinate cm3Expected = r1.average(r2);
    EXPECT_EQ(cm3Expected, asbly3.getCM()) << " Center of Mass calculated incorrectly: ";

    // these are a bit of a hack, and depend upon all the bodies being along the y=0, z=0 line.
    const Coordinate delta13 = asbly3.getCM().sub(r1);
    const Coordinate delta23 = asbly3.getCM().sub(r2);

    const double y13     = delta13.y;
    const double dy13Sq  = MathUtil::pow2(y13);  // hack
    const double i13ax   = i1ax + (m1 * dy13Sq);
    const double i13zz   = i1t + (m1 * dy13Sq);
    const double y23     = delta23.y;
    const double dy23Sq  = MathUtil::pow2(y23);  // hack
    const double i23ax   = i2ax + (m2 * dy23Sq);
    const double i23zz   = i2t + (m2 * dy23Sq);
    const double expI3xx = i13ax + i23ax;
    const double expI3yy = i1t + i2t;
    const double expI3zz = i13zz + i23zz;

    EXPECT_NEAR(asbly3.getIxx(), expI3xx, kEpsilon * 10) << "x-axis MOI don't match: ";
    EXPECT_NEAR(asbly3.getIyy(), expI3yy, kEpsilon * 10) << "y-axis MOI don't match: ";
    EXPECT_NEAR(asbly3.getIzz(), expI3zz, kEpsilon * 10) << "z-axis MOI don't match: ";
}

TEST(RigidBody, TwoPointGeneral)
{
    const double     m1 = 2.5;
    const Coordinate r1(0, -40, -10, m1);
    const double     i1xx = 28.7;
    const double     i1t  = i1xx / 2;
    const RigidBody  body1(r1, i1xx, i1t);

    const double     m2 = 5.7;
    const Coordinate r2(0, 32, 15, m2);
    const double     i2xx = 20;
    const double     i2t  = i2xx / 2;
    const RigidBody  body2(r2, i2xx, i2t);

    // point 3 is defined as the CM of bodies 1 and 2 combined.
    const RigidBody asbly3 = body1.add(body2);

    const Coordinate cm3Expected = r1.average(r2);
    EXPECT_EQ(cm3Expected, asbly3.getCM()) << " Center of Mass calculated incorrectly: ";

    const Coordinate delta13 = asbly3.getCM().sub(r1);
    const Coordinate delta23 = asbly3.getCM().sub(r2);

    double       x2    = MathUtil::pow2(delta13.x);
    double       y2    = MathUtil::pow2(delta13.y);
    double       z2    = MathUtil::pow2(delta13.z);
    const double i13xx = i1xx + (m1 * (y2 + z2));
    const double i13yy = i1t + (m1 * (x2 + z2));
    const double i13zz = i1t + (m1 * (x2 + y2));

    x2                 = MathUtil::pow2(delta23.x);
    y2                 = MathUtil::pow2(delta23.y);
    z2                 = MathUtil::pow2(delta23.z);
    const double i23xx = i2xx + (m2 * (y2 + z2));
    const double i23yy = i2t + (m2 * (x2 + z2));
    const double i23zz = i2t + (m2 * (x2 + y2));

    EXPECT_NEAR(asbly3.getIxx(), i13xx + i23xx, kEpsilon * 10) << "x-axis MOI don't match: ";
    EXPECT_NEAR(asbly3.getIyy(), i13yy + i23yy, kEpsilon * 10) << "y-axis MOI don't match: ";
    EXPECT_NEAR(asbly3.getIzz(), i13zz + i23zz, kEpsilon * 10) << "z-axis MOI don't match: ";
}

TEST(RigidBody, CompoundCalculations)
{
    const double     m1 = 2.5;
    const Coordinate r1(0, -40, 0, m1);
    const double     i1ax = 28.7;
    const double     i1t  = i1ax / 2;
    const RigidBody  body1(r1, i1ax, i1t);

    const double     m2 = m1;
    const Coordinate r2(0, -2, 0, m2);
    const double     i2ax = 28.7;
    const double     i2t  = i2ax / 2;
    const RigidBody  body2(r2, i2ax, i2t);

    const double     m5 = 5.7;
    const Coordinate r5(0, 32, 0, m5);
    const double     i5ax = 20;
    const double     i5t  = i5ax / 2;
    const RigidBody  body5(r5, i5ax, i5t);

    // point 3 is defined as the CM of bodies 1 and 2 combined.
    const RigidBody asbly3 = body1.add(body2);

    // point 4 is defined as the CM of bodies 1, 2 and 5 combined.
    const RigidBody  asbly4Indirect = asbly3.add(body5);
    const Coordinate cm4Expected    = r1.average(r2).average(r5);

    EXPECT_EQ(cm4Expected, Coordinate(0, 7.233644859813085, 0, m1 + m2 + m5))
        << " Center of Mass calculated incorrectly: ";

    // these are a bit of a hack, and depend upon all the bodies being along the y=0, z=0 line.
    const double y4    = cm4Expected.y;
    const double i14ax = i1ax + (m1 * MathUtil::pow2(std::abs(body1.getCM().y - y4)));
    const double i24ax = i2ax + (m2 * MathUtil::pow2(std::abs(body2.getCM().y - y4)));
    const double i54ax = i5ax + (m5 * MathUtil::pow2(std::abs(body5.getCM().y - y4)));

    const double i14zz = i1t + (m1 * MathUtil::pow2(std::abs(body1.getCM().y - y4)));
    const double i24zz = i2t + (m2 * MathUtil::pow2(std::abs(body2.getCM().y - y4)));
    const double i54zz = i5t + (m5 * MathUtil::pow2(std::abs(body5.getCM().y - y4)));

    const double    i4xx = i14ax + i24ax + i54ax;
    const double    i4yy = i1t + i2t + i5t;
    const double    i4zz = i14zz + i24zz + i54zz;
    const RigidBody asbly4Expected(cm4Expected, i4xx, i4yy, i4zz);

    EXPECT_NEAR(asbly4Indirect.getIxx(), asbly4Expected.getIxx(), kEpsilon * 10)
        << "x-axis MOI don't match: ";
    EXPECT_NEAR(asbly4Indirect.getIyy(), asbly4Expected.getIyy(), kEpsilon * 10)
        << "y-axis MOI don't match: ";
    EXPECT_NEAR(asbly4Indirect.getIzz(), asbly4Expected.getIzz(), kEpsilon * 10)
        << "z-axis MOI don't match: ";
}

// ---- QtRocket additions ----

TEST(RigidBody, OperationsMatchOpenRocket)
{
    // Printed by OpenRocket's RigidBody on JDK 17.
    const RigidBody a(Coordinate(1, 2, 3, 4), 0.5, 1.5, 2.5);
    const RigidBody b(Coordinate(-1, 0.5, 0, 2), 0.25, 0.75);

    const RigidBody sum = a.add(b);
    EXPECT_TRUE(sum.getCM().exactlyEquals(Coordinate(0.3333333333333333, 1.5, 2.0, 6.0)));
    EXPECT_EQ(sum.getIxx(), 15.75);
    EXPECT_EQ(sum.getIyy(), 19.583333333333336);
    EXPECT_EQ(sum.getIzz(), 11.583333333333334);

    // rebase() keeps the new location as given: weightless here, so the body loses its mass.
    const RigidBody rebased = a.rebase(Coordinate(0.1, 0.2, 0.3));
    EXPECT_TRUE(rebased.getCM().exactlyEquals(Coordinate(0.1, 0.2, 0.3, 0.0)));
    EXPECT_EQ(rebased.getIxx(), 42.620000000000005);
    EXPECT_EQ(rebased.getIyy(), 33.900000000000006);
    EXPECT_EQ(rebased.getIzz(), 18.700000000000003);
    EXPECT_EQ(rebased.getMass(), 0.0);

    // translateInertia() adds the delta's weight to the mass.
    const RigidBody translated = a.translateInertia(Coordinate(0.5, -0.5, 1, 7));
    EXPECT_TRUE(translated.getCM().exactlyEquals(Coordinate(1.5, 1.5, 4.0, 11.0)));
    EXPECT_EQ(translated.getIxx(), 5.5);
    EXPECT_EQ(translated.getIyy(), 6.5);
    EXPECT_EQ(translated.getIzz(), 4.5);

    const RigidBody scaled = a.scaleMass(0.5);
    EXPECT_TRUE(scaled.getCM().exactlyEquals(Coordinate(1.0, 2.0, 3.0, 2.0)));
    EXPECT_EQ(scaled.getIxx(), 0.25);
    EXPECT_EQ(scaled.getIyy(), 0.75);
    EXPECT_EQ(scaled.getIzz(), 1.25);
}

TEST(RigidBody, ToStringMatchesOpenRocket)
{
    const RigidBody a(Coordinate(1, 2, 3, 4), 0.5, 1.5, 2.5);
    EXPECT_EQ(a.toString(),
              "CoM: 4.00000000g @[1.00000000,2.00000000,3.00000000] // MOI: [ "
              "0.50000000, 1.50000000, 2.50000000]");
    EXPECT_EQ(a.toCMString(), "CoM: 4.00000000g @[1.00000000,2.00000000,3.00000000]");
    EXPECT_EQ(a.toMOIString(), "MOI: [ 0.50000000, 1.50000000, 2.50000000]");

    const RigidBody sum = a.add(RigidBody(Coordinate(-1, 0.5, 0, 2), 0.25, 0.75));
    EXPECT_EQ(sum.toString(),
              "CoM: 6.00000000g @[0.33333333,1.50000000,2.00000000] // MOI: [ "
              "15.75000000, 19.58333333, 11.58333333]");

    EXPECT_EQ(RigidBody::kEmpty.toString(),
              "CoM: 0.00000000g @[0.00000000,0.00000000,0.00000000] // MOI: [ 0.00000000, "
              "0.00000000, 0.00000000]");

    // Java's %.8f spellings of NaN, infinity and negative zero, and its half-up rounding.
    const double    nan = std::numeric_limits<double>::quiet_NaN();
    const double    inf = std::numeric_limits<double>::infinity();
    const RigidBody odd(Coordinate(nan, inf, -0.0, 1e-9), 1234567.123456789, 0, 0.000000005);
    EXPECT_EQ(odd.toString(),
              "CoM: 0.00000000g @[NaN,Infinity,-0.00000000] // MOI: [ "
              "1234567.12345679, 0.00000000, 0.00000001]");
}

TEST(RigidBody, EmptyBody)
{
    EXPECT_TRUE(RigidBody::kEmpty.isEmpty());
    EXPECT_EQ(RigidBody::kEmpty.getMass(), 0.0);
    EXPECT_TRUE(RigidBody(Coordinate(0, 0, 0, 0), 0, 0).isEmpty());
    EXPECT_TRUE(RigidBody(Coordinate(0, 0, 0, 1e-12), 1e-12, 0).isEmpty());  // within epsilon
    EXPECT_FALSE(RigidBody(Coordinate(1, 2, 3, 4), 0.5, 1.5, 2.5).isEmpty());
    EXPECT_FALSE(RigidBody(Coordinate(0, 0, 0, 0), 1, 0).isEmpty());

    // Adding empty bodies: the weightless average is the midpoint.
    const RigidBody empty2 = RigidBody::kEmpty.add(RigidBody::kEmpty);
    EXPECT_TRUE(empty2.isEmpty());
    const RigidBody a(Coordinate(1, 2, 3, 4), 0.5, 1.5, 2.5);
    const RigidBody withEmpty = RigidBody::kEmpty.add(a);
    EXPECT_TRUE(withEmpty.getCM().exactlyEquals(a.getCM()));
    EXPECT_EQ(withEmpty.getIxx(), 0.5);
    EXPECT_EQ(withEmpty.getIyy(), 1.5);
    EXPECT_EQ(withEmpty.getIzz(), 2.5);
}

TEST(RigidBody, NegativeInertiaIsABug)
{
    EXPECT_THROW(RigidBody(Coordinate(0, 0, 0, 1), -1e-12, 0, 0), BugError);
    EXPECT_THROW(RigidBody(Coordinate(0, 0, 0, 1), 0, -1, 0), BugError);
    EXPECT_THROW(RigidBody(Coordinate(0, 0, 0, 1), 0, 0, -1), BugError);
    EXPECT_THROW(RigidBody(Coordinate(0, 0, 0, 1), 1, -1), BugError);
    EXPECT_THROW((void)RigidBody(Coordinate(0, 0, 0, 1), 1, 1).scaleMass(-1), BugError);
    // NaN passes, as in Java; -0.0 is not negative.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_NO_THROW(RigidBody(Coordinate(0, 0, 0, 1), nan, nan, nan));
    EXPECT_NO_THROW(RigidBody(Coordinate(0, 0, 0, 1), -0.0, -0.0));
}

TEST(RigidBody, GettersAndAliases)
{
    const RigidBody body(Coordinate(1, 2, 3, 4), 0.5, 1.5, 2.5);
    EXPECT_TRUE(body.getCenterOfMass().exactlyEquals(Coordinate(1, 2, 3, 4)));
    EXPECT_TRUE(body.getCM().exactlyEquals(body.getCenterOfMass()));
    EXPECT_EQ(body.getMass(), 4.0);
    EXPECT_EQ(body.getRotationalInertia(), 0.5);
    EXPECT_EQ(body.getLongitudinalInertia(), 1.5);

    // The three-argument form uses the longitudinal inertia for both Iyy and Izz.
    const RigidBody symmetric(Coordinate(0, 0, 0, 2), 0.3, 0.7);
    EXPECT_EQ(symmetric.getIxx(), 0.3);
    EXPECT_EQ(symmetric.getIyy(), 0.7);
    EXPECT_EQ(symmetric.getIzz(), 0.7);
}

TEST(RigidBody, EqualityIsTolerant)
{
    const RigidBody body(Coordinate(1, 2, 3, 4), 5, 6, 7);
    EXPECT_TRUE(body == RigidBody(Coordinate(1, 2, 3, 4), 5 * (1 + 1e-10), 6, 7));
    EXPECT_FALSE(body == RigidBody(Coordinate(1, 2, 3, 4), 5, 6.001, 7));
    EXPECT_FALSE(body == RigidBody(Coordinate(1, 2, 3, 4), 5, 6, 7.001));
    EXPECT_FALSE(body == RigidBody(Coordinate(1, 2, 3, 4), 5.001, 6, 7));
}

TEST(RigidBody, ParallelAxisTheoremAboutAnOffsetPoint)
{
    // A 2 kg point mass at (1, 0, 0) seen from the origin: nothing about x, m d^2 = 2 about y and
    // z.
    const RigidBody point(Coordinate(1, 0, 0, 2), 0, 0);
    const RigidBody moved = point.rebase(Coordinate(0, 0, 0));
    EXPECT_EQ(moved.getIxx(), 0.0);
    EXPECT_EQ(moved.getIyy(), 2.0);
    EXPECT_EQ(moved.getIzz(), 2.0);

    // Two equal masses on either side of the origin: centred there, inertia 2 m d^2.
    const RigidBody pair =
        RigidBody(Coordinate(0, 1, 0, 3), 0, 0).add(RigidBody(Coordinate(0, -1, 0, 3), 0, 0));
    EXPECT_TRUE(pair.getCM().exactlyEquals(Coordinate(0, 0, 0, 6)));
    EXPECT_EQ(pair.getIxx(), 6.0);
    EXPECT_EQ(pair.getIyy(), 0.0);
    EXPECT_EQ(pair.getIzz(), 6.0);
}

TEST(RigidBody, EmptyIsAConstant)
{
    static_assert(RigidBody::kEmpty.getMass() == 0.0);
    static_assert(RigidBody::kEmpty.getIxx() == 0.0);
    EXPECT_TRUE(RigidBody::kEmpty.getCM().exactlyEquals(Coordinate::kZero));
}

}  // namespace
