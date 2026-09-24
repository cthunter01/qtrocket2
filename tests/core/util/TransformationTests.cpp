#include "QtRocket/util/Transformation.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Quaternion.h"

namespace
{

using QtRocket::Coordinate;
using QtRocket::Quaternion;
using QtRocket::Transformation;
using Matrix3 = Transformation::Matrix3;

constexpr Coordinate kXUnit = Coordinate::kXUnit;
constexpr Coordinate kYUnit = Coordinate::kYUnit;
constexpr Coordinate kZUnit = Coordinate::kZUnit;

constexpr double kPi    = std::numbers::pi;
constexpr double kPi2   = std::numbers::pi / 2.0;
constexpr double kEps   = 0.0000000001;
constexpr double kNaN   = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf   = std::numeric_limits<double>::infinity();
constexpr double kGlEps = 1.0e-6;

// Every component within eps, printing the whole coordinate on failure.
void expectCoordinateNear(const Coordinate& expected, const Coordinate& actual, double eps = kEps)
{
    SCOPED_TRACE(std::format("expected {}, actual {}", expected.toString(), actual.toString()));
    EXPECT_NEAR(expected.x, actual.x, eps);
    EXPECT_NEAR(expected.y, actual.y, eps);
    EXPECT_NEAR(expected.z, actual.z, eps);
    EXPECT_NEAR(expected.weight, actual.weight, eps);
}

// The sixteen assertEquals(..., buf.get(i), 1.0e-6) of the OpenGL tests.
void expectGlMatrix(const std::array<double, 16>& expected, const std::array<double, 16>& actual)
{
    const std::span<const double> e{expected};
    const std::span<const double> a{actual};
    for (std::size_t i = 0; i < e.size(); i++)
    {
        EXPECT_NEAR(e[i], a[i], kGlEps) << "element " << i;
    }
}

// The inverse, or a failed assertion and the identity when there is none. A plain if is what
// clang-tidy's unchecked-optional-access check follows; it cannot see through ASSERT_TRUE.
Transformation requireInverse(const Transformation& t)
{
    const std::optional<Transformation> inverse = t.inverse();
    if (!inverse.has_value())
    {
        ADD_FAILURE() << "no inverse for\n" << t.toString();
        return Transformation::kIdentity;
    }
    return *inverse;
}

// ---- Ported from TestTransformation.java ----

TEST(Transformation, TransformIdentity)
{
    const Transformation t = Transformation::kIdentity;
    EXPECT_EQ(kXUnit, t.transform(kXUnit));
    EXPECT_EQ(kYUnit, t.transform(kYUnit));
    EXPECT_EQ(kZUnit, t.transform(kZUnit));
}

TEST(Transformation, TransformIdentityToOpenGl)
{
    const Transformation t = Transformation::kIdentity;
    expectGlMatrix({1.0, 0.0, 0.0, 0.0,  //
                    0.0, 1.0, 0.0, 0.0,  //
                    0.0, 0.0, 1.0, 0.0,  //
                    0.0, 0.0, 0.0, 1.0},
                   t.glMatrix());
}

TEST(Transformation, TransformTranslationToOpenGl)
{
    const Transformation translate(1, 2, 3);
    expectGlMatrix({1.0, 0.0, 0.0, 0.0,  //
                    0.0, 1.0, 0.0, 0.0,  //
                    0.0, 0.0, 1.0, 0.0,  //
                    1.0, 2.0, 3.0, 1.0},
                   translate.glMatrix());
}

TEST(Transformation, TransformRotateByPi2ToOpenGl)
{
    const Transformation translate = Transformation::axialRotation(kPi2);
    expectGlMatrix({1.0, 0.0, 0.0, 0.0,   //
                    0.0, 0.0, 1.0, 0.0,   //
                    0.0, -1.0, 0.0, 0.0,  //
                    0.0, 0.0, 0.0, 1.0},
                   translate.glMatrix());
}

TEST(Transformation, TransformTranslationIndividual)
{
    const Transformation translate(1, 2, 3);

    EXPECT_EQ(Coordinate(2, 2, 3), translate.transform(kXUnit));
    EXPECT_EQ(Coordinate(1, 3, 3), translate.transform(kYUnit));
    EXPECT_EQ(Coordinate(1, 2, 4), translate.transform(kZUnit));
}

TEST(Transformation, TransformTranslationCoordinate)
{
    const Transformation translate(Coordinate(2, 3, 4));

    EXPECT_EQ(Coordinate(3, 3, 4), translate.transform(kXUnit));
    EXPECT_EQ(Coordinate(2, 4, 4), translate.transform(kYUnit));
    EXPECT_EQ(Coordinate(2, 3, 5), translate.transform(kZUnit));
}

TEST(Transformation, TransformTranslationConvenience)
{
    const Transformation translate = Transformation::translation(2, 3, 4);

    EXPECT_EQ(Coordinate(3, 3, 4), translate.transform(kXUnit));
    EXPECT_EQ(Coordinate(2, 4, 4), translate.transform(kYUnit));
    EXPECT_EQ(Coordinate(2, 3, 5), translate.transform(kZUnit));
}

TEST(Transformation, TransformSmallYRotation)
{
    const Transformation t = Transformation::rotateY(0.01);

    const Coordinate v1 = t.transform(kXUnit);
    // we need to test individual coordinates due to error.
    EXPECT_NEAR(1, v1.x, 0.001);
    EXPECT_NEAR(0, v1.y, 0.001);
    EXPECT_NEAR(-0.01, v1.z, 0.001);

    EXPECT_EQ(kYUnit, t.transform(kYUnit));

    const Coordinate v2 = t.transform(kZUnit);
    // we need to test individual coordinates due to error.
    EXPECT_NEAR(0.01, v2.x, 0.001);
    EXPECT_NEAR(0, v2.y, 0.001);
    EXPECT_NEAR(1, v2.z, 0.001);
}

TEST(Transformation, TransformRotateXByPi2)
{
    const Transformation t = Transformation::axialRotation(kPi2);

    EXPECT_EQ(kXUnit, t.transform(kXUnit));
    EXPECT_EQ(kZUnit, t.transform(kYUnit));
    EXPECT_EQ(kYUnit.multiply(-1), t.transform(kZUnit));
}

TEST(Transformation, TransformEuler313Transform)
{
    {
        const Transformation r313 = Transformation::eulerAngle313(0.0, 0.0, kPi2);
        EXPECT_EQ(kYUnit, r313.transform(kXUnit));
        EXPECT_EQ(kXUnit.multiply(-1), r313.transform(kYUnit));
        EXPECT_EQ(kZUnit, r313.transform(kZUnit));
    }
    {
        const Transformation r313 = Transformation::eulerAngle313(kPi / 4.0, 0.0, kPi / 4.0);
        // precision = 8 decimal places
        EXPECT_EQ(kYUnit, r313.transform(kXUnit));
        EXPECT_EQ(kXUnit.multiply(-1), r313.transform(kYUnit));
        EXPECT_EQ(kZUnit, r313.transform(kZUnit));
    }
    {
        const Transformation r313 = Transformation::eulerAngle313(kPi / 4.0, kPi2, kPi / 4.0);
        // precision = 8 decimal places
        EXPECT_EQ(Coordinate(+0.500000, +0.500000, 0.707106781), r313.transform(kXUnit));
        EXPECT_EQ(Coordinate(-0.500000, -0.5000000, 0.707106781), r313.transform(kYUnit));
        EXPECT_EQ(Coordinate(+0.707106781, -0.707106781, 0.0), r313.transform(kZUnit));
    }
}

TEST(Transformation, TransformEuler121Transform)
{
    const Transformation r123 = Transformation::rotateX(-1.0)
                                    .applyTransformation(Transformation::rotateY(0.01))
                                    .applyTransformation(Transformation::rotateZ(1.0));

    EXPECT_EQ(Coordinate(+0.540275291, +0.450102302, -0.710992634), r123.transform(kXUnit));
    EXPECT_EQ(Coordinate(-0.841428912, +0.299007198, -0.450102302), r123.transform(kYUnit));
    EXPECT_EQ(Coordinate(+0.009999833334, +0.841428911609, +0.540275290977),
              r123.transform(kZUnit));
}

TEST(Transformation, TransformRotateTranslate)
{
    const Transformation r = Transformation::translation(2, 3, 4).applyTransformation(
        Transformation::axialRotation(kPi2));

    EXPECT_EQ(Coordinate(3, 3, 4), r.transform(kXUnit));
    EXPECT_EQ(Coordinate(2, 3, 5), r.transform(kYUnit));
    EXPECT_EQ(Coordinate(2, 2, 4), r.transform(kZUnit));
}

TEST(Transformation, LinearTransformIgnoresTranslation)
{
    const Transformation rotation    = Transformation::rotateZ(kPi2);
    const Transformation translation = Transformation::translation(5, -3, 2);
    const Transformation composite   = translation.applyTransformation(rotation);

    const Coordinate result = composite.linearTransform(kXUnit);
    EXPECT_EQ(rotation.transform(kXUnit), result);
}

TEST(Transformation, ApplyTransformationMatchesSequentialApplication)
{
    const Transformation translate = Transformation::translation(2, 0, 1);
    const Transformation rotate    = Transformation::rotateY(kPi2);
    const Transformation combined  = rotate.applyTransformation(translate);

    const Coordinate point(1, 0, 0);
    const Coordinate sequential = rotate.transform(translate.transform(point));
    const Coordinate composed   = combined.transform(point);

    EXPECT_EQ(sequential, composed);
}

TEST(Transformation, RotationAroundOriginKeepsOriginFixed)
{
    const Coordinate     origin(2, 3, 4);
    const Transformation rotation = Transformation::rotation(Coordinate(0, 0, kPi2), origin);

    EXPECT_EQ(origin, rotation.transform(origin));

    const Coordinate point(3, 3, 4);
    const Coordinate rotated = rotation.transform(point);

    EXPECT_EQ(Coordinate(2, 4, 4), rotated);
}

// Java: testTransformCollectionReplacesElementsInPlace. The span overload returns a new vector
// instead of replacing the elements.
TEST(Transformation, TransformSpanReturnsTheTransformedCoordinates)
{
    const std::vector<Coordinate> points{Coordinate(1, 0, 0), Coordinate(0, 1, 0)};

    const Transformation          rotation = Transformation::rotateZ(kPi2);
    const std::vector<Coordinate> rotated  = rotation.transform(points);

    ASSERT_EQ(2U, rotated.size());
    EXPECT_EQ(Coordinate(0, 1, 0), rotated[0]);
    EXPECT_EQ(Coordinate(-1, 0, 0), rotated[1]);

    // The input is left alone
    EXPECT_TRUE(points[0].exactlyEquals(Coordinate(1, 0, 0)));
    EXPECT_TRUE(points[1].exactlyEquals(Coordinate(0, 1, 0)));
}

TEST(Transformation, IdentityDetection)
{
    const Transformation identityCopy = Transformation::translation(0, 0, 0);
    EXPECT_TRUE(identityCopy.isIdentity());
    EXPECT_EQ(Transformation::kIdentity, identityCopy);

    const Transformation translated = Transformation::translation(0, 0, 1);
    EXPECT_FALSE(translated.isIdentity());
}

// ---- Behaviour OpenRocket does not test ----

TEST(Transformation, ConstructorsAndAccessors)
{
    const Transformation identity;
    EXPECT_TRUE(identity.isIdentity());
    EXPECT_TRUE(identity.translationVector().exactlyEquals(Coordinate::kZero));
    EXPECT_EQ((Matrix3{{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}}), identity.matrix());

    // The translation is kept as given, weight included
    const Transformation weighted(Coordinate(1, 2, 3, 4));
    EXPECT_TRUE(weighted.translationVector().exactlyEquals(Coordinate(1, 2, 3, 4)));
    EXPECT_EQ(identity.matrix(), weighted.matrix());

    const Matrix3        m{{{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}, {7.0, 8.0, 9.0}}};
    const Transformation linear(m);
    EXPECT_EQ(m, linear.matrix());
    EXPECT_TRUE(linear.translationVector().exactlyEquals(Coordinate::kNul));

    const Transformation affine(m, Coordinate(-1, -2, -3));
    EXPECT_EQ(m, affine.matrix());
    EXPECT_TRUE(affine.translationVector().exactlyEquals(Coordinate(-1, -2, -3)));
    EXPECT_TRUE(affine.transform(Coordinate(1, 1, 1)).exactlyEquals(Coordinate(5, 13, 21)));
    EXPECT_TRUE(affine.linearTransform(Coordinate(1, 1, 1)).exactlyEquals(Coordinate(6, 15, 24)));
}

TEST(Transformation, TransformKeepsTheWeight)
{
    const Coordinate c(1, 2, 3, 4);
    EXPECT_TRUE(
        Transformation::translation(1, 1, 1).transform(c).exactlyEquals(Coordinate(2, 3, 4, 4)));
    EXPECT_TRUE(Transformation::translation(1, 1, 1).linearTransform(c).exactlyEquals(c));
    EXPECT_TRUE(Transformation::kProjectXY.transform(c).exactlyEquals(Coordinate(1, 2, 0, 4)));
}

TEST(Transformation, ProjectionsDropOneComponent)
{
    const Coordinate c(1, 2, 3);
    EXPECT_TRUE(Transformation::kProjectXY.transform(c).exactlyEquals(Coordinate(1, 2, 0)));
    EXPECT_TRUE(Transformation::kProjectYZ.transform(c).exactlyEquals(Coordinate(0, 2, 3)));
    EXPECT_TRUE(Transformation::kProjectXZ.transform(c).exactlyEquals(Coordinate(1, 0, 3)));
    EXPECT_FALSE(Transformation::kProjectXY.isIdentity());
}

TEST(Transformation, CompositionIsConstexpr)
{
    constexpr Transformation kShiftedProjection =
        Transformation::translation(1, 2, 3).applyTransformation(Transformation::kProjectXY);
    static_assert(
        kShiftedProjection.transform(Coordinate(1, 1, 1)).exactlyEquals(Coordinate(2, 3, 3)));
    static_assert(
        kShiftedProjection.linearTransform(Coordinate(1, 1, 1)).exactlyEquals(Coordinate(1, 1, 0)));
    static_assert(kShiftedProjection.translationVector().exactlyEquals(Coordinate(1, 2, 3)));
    static_assert(Transformation::kIdentity.glMatrix()[15] == 1.0);
    EXPECT_TRUE(
        kShiftedProjection.transform(Coordinate(1, 1, 1)).exactlyEquals(Coordinate(2, 3, 3)));
}

TEST(Transformation, ApplyTransformationAppliesTheArgumentFirst)
{
    const Transformation translate = Transformation::translation(1, 0, 0);
    const Transformation rotate    = Transformation::rotateZ(kPi2);

    // rotate, then translate: (1,0,0) -> (0,1,0) -> (1,1,0)
    EXPECT_EQ(Coordinate(1, 1, 0), translate.applyTransformation(rotate).transform(kXUnit));
    // translate, then rotate: (1,0,0) -> (2,0,0) -> (0,2,0)
    EXPECT_EQ(Coordinate(0, 2, 0), rotate.applyTransformation(translate).transform(kXUnit));

    // The identity is neutral on either side
    EXPECT_EQ(translate, translate.applyTransformation(Transformation::kIdentity));
    EXPECT_EQ(translate, Transformation::kIdentity.applyTransformation(translate));
}

// OpenRocket combines the translations with Coordinate.add, which sums their weights.
TEST(Transformation, CompositionSumsTheTranslationWeights)
{
    const Transformation a(Coordinate(1, 0, 0, 2));
    const Transformation b(Coordinate(0, 1, 0, 3));
    EXPECT_TRUE(a.applyTransformation(b).translationVector().exactlyEquals(Coordinate(1, 1, 0, 5)));
}

TEST(Transformation, RotationsBelowAngleEpsilonAreTheIdentity)
{
    const Matrix3 identity{{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};
    EXPECT_EQ(identity, Transformation::rotateX(5e-10).matrix());
    EXPECT_EQ(identity, Transformation::rotateY(-5e-10).matrix());
    EXPECT_EQ(identity, Transformation::rotateZ(0.0).matrix());
    EXPECT_EQ(identity, Transformation::axialRotation(-0.0).matrix());

    // From the epsilon on the matrix is built; 1e-8 is past the tolerance of operator== too
    EXPECT_NE(identity, Transformation::rotateX(Transformation::kAngleEpsilon).matrix());
    EXPECT_FALSE(Transformation::rotateX(1e-8).isIdentity());
    EXPECT_FALSE(Transformation::rotateY(1e-8).isIdentity());
    EXPECT_FALSE(Transformation::rotateZ(1e-8).isIdentity());

    // A NaN angle is not below the epsilon (Java: ANGLE_EPSILON > Math.abs(NaN) is false), so the
    // matrix is built from cos(NaN) and sin(NaN)
    EXPECT_TRUE(std::isnan(Transformation::rotateX(kNaN).matrix()[1][1]));
    EXPECT_TRUE(std::isnan(Transformation::rotateY(kNaN).matrix()[0][0]));
    EXPECT_TRUE(std::isnan(Transformation::rotateZ(kNaN).matrix()[0][0]));
    EXPECT_TRUE(std::isnan(Transformation::axialRotation(kNaN).matrix()[2][2]));
}

TEST(Transformation, SingleAxisRotationsFollowTheRightHandRule)
{
    EXPECT_EQ(kZUnit, Transformation::rotateX(kPi2).transform(kYUnit));
    EXPECT_EQ(kXUnit, Transformation::rotateY(kPi2).transform(kZUnit));
    EXPECT_EQ(kYUnit, Transformation::rotateZ(kPi2).transform(kXUnit));
    EXPECT_EQ(Transformation::rotateX(0.4), Transformation::axialRotation(0.4));
    // A full turn is the identity, a half turn negates the other two axes
    EXPECT_TRUE(Transformation::rotateZ(2 * kPi).isIdentity());
    EXPECT_EQ(Coordinate(-1, -2, 3), Transformation::rotateZ(kPi).transform(Coordinate(1, 2, 3)));
}

TEST(Transformation, RotationFromAnglesIsRxTimesRyTimesRz)
{
    const Coordinate     angles(0.3, -0.7, 1.1);
    const Transformation rx = Transformation::rotateX(0.3);
    const Transformation ry = Transformation::rotateY(-0.7);
    const Transformation rz = Transformation::rotateZ(1.1);

    const Transformation rotation = Transformation::rotation(angles);
    EXPECT_EQ(rx.applyTransformation(ry).applyTransformation(rz), rotation);
    EXPECT_TRUE(rotation.translationVector().exactlyEquals(Coordinate::kZero));

    // On a coordinate the z rotation acts first
    const Coordinate v(0.2, -0.4, 0.9, 1.5);
    EXPECT_EQ(rx.transform(ry.transform(rz.transform(v))), rotation.transform(v));

    // Angles not above kAngleEpsilon are skipped
    EXPECT_EQ(ry, Transformation::rotation(Coordinate(Transformation::kAngleEpsilon, -0.7,
                                                      -Transformation::kAngleEpsilon)));
    EXPECT_TRUE(Transformation::rotation(Coordinate::kZero).isIdentity());
    EXPECT_EQ(rz, Transformation::rotation(Coordinate(0, 0, 1.1)));

    // So is a NaN angle (Java: Math.abs(NaN) > ANGLE_EPSILON is false), unlike in rotateX/Y/Z
    EXPECT_TRUE(Transformation::rotation(Coordinate(kNaN, kNaN, kNaN)).isIdentity());
    EXPECT_EQ(ry, Transformation::rotation(Coordinate(kNaN, -0.7, kNaN)));
    EXPECT_TRUE(Transformation::rotation(Coordinate::kNaN, Coordinate(1, 2, 3)).isIdentity());
}

TEST(Transformation, RotationAboutAPointComposesTranslateRotateTranslateBack)
{
    const Coordinate angles(0.3, -0.7, 1.1);
    const Coordinate origin(1, -2, 3);

    const Transformation expected = Transformation::translation(origin).applyTransformation(
        Transformation::rotation(angles).applyTransformation(
            Transformation::translation(-1, 2, -3)));
    const Transformation rotation = Transformation::rotation(angles, origin);
    EXPECT_EQ(expected, rotation);
    EXPECT_EQ(origin, rotation.transform(origin));

    // A half turn about z around (1, 1, 0) takes (2, 1, 0) to (0, 1, 0)
    const Transformation halfTurn =
        Transformation::rotation(Coordinate(0, 0, kPi), Coordinate(1, 1, 0));
    EXPECT_EQ(Coordinate(0, 1, 0), halfTurn.transform(Coordinate(2, 1, 0)));
    // With no rotation the result is the identity (translate there and back)
    EXPECT_TRUE(Transformation::rotation(Coordinate::kZero, origin).isIdentity());
}

TEST(Transformation, RotationFromQuaternionMatchesQuaternionRotate)
{
    const Quaternion     q = Quaternion::rotation(Coordinate(0.3, -1.2, 0.5));
    const Transformation t = Transformation::rotation(q);
    EXPECT_TRUE(t.translationVector().exactlyEquals(Coordinate::kZero));

    for (const Coordinate& v : {kXUnit, kYUnit, kZUnit, Coordinate(1.5, -2, 0.25, 3)})
    {
        expectCoordinateNear(q.rotate(v), t.transform(v), 1e-14);
    }

    // A rotation about one axis is the matching rotateX/Y/Z
    EXPECT_EQ(Transformation::rotateX(0.4),
              Transformation::rotation(Quaternion::rotation(kXUnit, 0.4)));
    EXPECT_EQ(Transformation::rotateY(-1.3),
              Transformation::rotation(Quaternion::rotation(kYUnit, -1.3)));
    EXPECT_EQ(Transformation::rotateZ(2.9),
              Transformation::rotation(Quaternion::rotation(kZUnit, 2.9)));
    EXPECT_TRUE(Transformation::rotation(Quaternion()).isIdentity());

    // A non-unit quaternion scales, exactly as Quaternion::rotate does (q * v * conj(q))
    const Quaternion     doubled(2, 0, 0, 0);
    const Transformation scaled = Transformation::rotation(doubled);
    expectCoordinateNear(doubled.rotate(Coordinate(1, 2, 3)),
                         scaled.transform(Coordinate(1, 2, 3)));
    EXPECT_TRUE(scaled.transform(Coordinate(1, 2, 3)).exactlyEquals(Coordinate(4, 8, 12)));
}

TEST(Transformation, InverseUndoesARotationAndTranslation)
{
    const Transformation t = Transformation::translation(2, -3, 4).applyTransformation(
        Transformation::rotation(Coordinate(0.3, -0.7, 1.1)));
    const Transformation inverse = requireInverse(t);

    const Coordinate v(1.5, -2, 0.25, 3);
    expectCoordinateNear(v, inverse.transform(t.transform(v)), 1e-12);
    expectCoordinateNear(v, t.transform(inverse.transform(v)), 1e-12);
    EXPECT_TRUE(t.applyTransformation(inverse).isIdentity());
    EXPECT_TRUE(inverse.applyTransformation(t).isIdentity());

    // The inverse of the inverse is the original
    EXPECT_EQ(t, requireInverse(inverse));
}

TEST(Transformation, InverseOfSimpleTransformations)
{
    EXPECT_EQ(Transformation::kIdentity, requireInverse(Transformation::kIdentity));
    EXPECT_EQ(Transformation::translation(-1, -2, -3),
              requireInverse(Transformation::translation(1, 2, 3)));
    EXPECT_EQ(Transformation::rotateX(-0.4), requireInverse(Transformation::rotateX(0.4)));
    EXPECT_EQ(Transformation::rotateY(1.3), requireInverse(Transformation::rotateY(-1.3)));
    EXPECT_EQ(Transformation::rotateZ(-2.9), requireInverse(Transformation::rotateZ(2.9)));

    // The translation's weight is kept
    const Transformation weighted(Coordinate(1, 2, 3, 7));
    EXPECT_TRUE(
        requireInverse(weighted).translationVector().exactlyEquals(Coordinate(-1, -2, -3, 7)));
}

TEST(Transformation, InverseOfAGeneralMatrix)
{
    // Not orthogonal: the inverse is the adjugate over the determinant (5)
    const Matrix3        m{{{2.0, 1.0, 0.0}, {0.0, 1.0, 3.0}, {1.0, 0.0, 1.0}}};
    const Transformation t(m, Coordinate(1, 2, 3));
    const Transformation inverse = requireInverse(t);

    const Matrix3 expected{{{0.2, -0.2, 0.6}, {0.6, 0.4, -1.2}, {-0.2, 0.2, 0.4}}};
    EXPECT_EQ(Transformation(expected), Transformation(inverse.matrix()));
    // -A^-1 * (1, 2, 3)
    expectCoordinateNear(Coordinate(-1.6, 2.2, -1.4), inverse.translationVector(), 1e-12);
    EXPECT_TRUE(t.applyTransformation(inverse).isIdentity());
    EXPECT_TRUE(inverse.applyTransformation(t).isIdentity());
    expectCoordinateNear(Coordinate(4, 5, 6, 1),
                         inverse.transform(t.transform(Coordinate(4, 5, 6, 1))), 1e-12);
}

TEST(Transformation, InverseOfASingularOrNonFiniteMatrixIsNullopt)
{
    EXPECT_FALSE(Transformation::kProjectXY.inverse().has_value());
    EXPECT_FALSE(Transformation::kProjectYZ.inverse().has_value());
    EXPECT_FALSE(Transformation::kProjectXZ.inverse().has_value());

    const Matrix3 zero{};
    EXPECT_FALSE(Transformation(zero).inverse().has_value());
    // Two equal rows
    const Matrix3 dependent{{{1.0, 2.0, 3.0}, {1.0, 2.0, 3.0}, {0.0, 0.0, 1.0}}};
    EXPECT_FALSE(Transformation(dependent).inverse().has_value());

    const Matrix3 nan{{{kNaN, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};
    EXPECT_FALSE(Transformation(nan).inverse().has_value());
    const Matrix3 infinite{{{kInf, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};
    EXPECT_FALSE(Transformation(infinite).inverse().has_value());

    // A NaN or infinite translation has no inverse either, however sound the matrix
    EXPECT_FALSE(Transformation::translation(kNaN, 0, 0).inverse().has_value());
    EXPECT_FALSE(Transformation::translation(0, kInf, 0).inverse().has_value());
    EXPECT_FALSE(Transformation::translation(0, 0, -kInf).inverse().has_value());
    EXPECT_FALSE(Transformation(Coordinate::kNaN).inverse().has_value());
    // The weight is not looked at
    EXPECT_TRUE(Transformation(Coordinate(1, 2, 3, kNaN)).inverse().has_value());
}

TEST(Transformation, XyzRotationAnglesReadBackSingleAxisRotations)
{
    const Transformation rx = Transformation::rotateX(0.3);
    EXPECT_NEAR(0.3, rx.xRotation(), kEps);
    EXPECT_NEAR(0.0, rx.yRotation(), kEps);
    EXPECT_NEAR(0.0, rx.zRotation(), kEps);

    const Transformation ry = Transformation::rotateY(-0.8);
    EXPECT_NEAR(0.0, ry.xRotation(), kEps);
    EXPECT_NEAR(-0.8, ry.yRotation(), kEps);
    EXPECT_NEAR(0.0, ry.zRotation(), kEps);

    const Transformation rz = Transformation::rotateZ(2.5);
    EXPECT_NEAR(0.0, rz.xRotation(), kEps);
    EXPECT_NEAR(0.0, rz.yRotation(), kEps);
    EXPECT_NEAR(2.5, rz.zRotation(), kEps);

    EXPECT_NEAR(kPi, Transformation::axialRotation(kPi).xRotation(), kEps);
    EXPECT_NEAR(0.0, Transformation::kIdentity.xRotation(), kEps);
    EXPECT_NEAR(0.0, Transformation::kIdentity.yRotation(), kEps);
    EXPECT_NEAR(0.0, Transformation::kIdentity.zRotation(), kEps);
    // The translation plays no part
    EXPECT_NEAR(0.3, Transformation::translation(5, 6, 7).applyTransformation(rx).xRotation(),
                kEps);
}

TEST(Transformation, EqualityIsTolerantAndIncludesTheTranslationWeight)
{
    const Matrix3 nearlyIdentity{{{1.0 + 1e-10, 0.0, 0.0}, {0.0, 1.0, 1e-10}, {0.0, 0.0, 1.0}}};
    EXPECT_EQ(Transformation::kIdentity, Transformation(nearlyIdentity));
    EXPECT_TRUE(Transformation(nearlyIdentity).isIdentity());
    EXPECT_EQ(Transformation::kIdentity, Transformation::translation(1e-10, 0, 0));

    const Matrix3 offIdentity{{{1.0 + 1e-7, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};
    EXPECT_NE(Transformation::kIdentity, Transformation(offIdentity));
    EXPECT_NE(Transformation::kIdentity, Transformation::translation(0, 0, 1e-7));

    // OpenRocket compares the translations with Coordinate.equals, so their weights count
    EXPECT_NE(Transformation::kIdentity, Transformation(Coordinate(0, 0, 0, 1)));
    EXPECT_FALSE(Transformation(Coordinate(0, 0, 0, 1)).isIdentity());

    // NaN is equal to nothing, not even itself
    const Matrix3 nan{{{kNaN, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};
    EXPECT_NE(Transformation(nan), Transformation(nan));
    EXPECT_FALSE(Transformation(nan) == Transformation(nan));
}

TEST(Transformation, TransformOfAnEmptySpanIsEmpty)
{
    const std::vector<Coordinate> none;
    EXPECT_TRUE(Transformation::rotateX(1).transform(none).empty());
}

TEST(Transformation, GlMatrixOfACompositeIsColumnMajor)
{
    const Transformation t =
        Transformation::translation(1, 2, 3).applyTransformation(Transformation::rotateZ(kPi2));
    expectGlMatrix({0.0, 1.0, 0.0, 0.0,   //
                    -1.0, 0.0, 0.0, 0.0,  //
                    0.0, 0.0, 1.0, 0.0,   //
                    1.0, 2.0, 3.0, 1.0},
                   t.glMatrix());
}

TEST(Transformation, ToString)
{
    EXPECT_EQ(Transformation::kIdentity.toString(),
              "[1.00 0.00 0.00]   [0.00]\n"
              "[0.00 1.00 0.00] + [0.00]\n"
              "[0.00 0.00 1.00]   [0.00]\n");
    EXPECT_EQ(Transformation::translation(1, -2.5, 3.14159).toString(),
              "[1.00 0.00 0.00]   [1.00]\n"
              "[0.00 1.00 0.00] + [-2.50]\n"
              "[0.00 0.00 1.00]   [3.14]\n");
    EXPECT_EQ(Transformation::rotateZ(kPi2).toString(),
              "[0.00 -1.00 0.00]   [0.00]\n"
              "[1.00 0.00 0.00] + [0.00]\n"
              "[0.00 0.00 1.00]   [0.00]\n");
}

TEST(Transformation, StreamInsertionWritesToString)
{
    const Transformation t = Transformation::translation(1, 2, 3);
    std::ostringstream   os;
    os << t;
    EXPECT_EQ(os.str(), t.toString());
}

}  // namespace
