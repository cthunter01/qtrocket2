#include "QtRocket/util/BoundingBox.h"

#include <array>
#include <format>
#include <limits>
#include <numbers>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/Transformation.h"

namespace
{

using QtRocket::BoundingBox;
using QtRocket::Coordinate;
using QtRocket::Rectangle2D;
using QtRocket::Transformation;

constexpr double kMaxDouble = std::numeric_limits<double>::max();
constexpr double kNaN       = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf       = std::numeric_limits<double>::infinity();
constexpr double kEps       = 0.0000000001;

// Every component within kEps, printing the whole coordinate on failure.
void expectCoordinateNear(const Coordinate& expected, const Coordinate& actual)
{
    SCOPED_TRACE(std::format("expected {}, actual {}", expected.toString(), actual.toString()));
    EXPECT_NEAR(expected.x, actual.x, kEps);
    EXPECT_NEAR(expected.y, actual.y, kEps);
    EXPECT_NEAR(expected.z, actual.z, kEps);
    EXPECT_NEAR(expected.weight, actual.weight, kEps);
}

// OpenRocket's BoundingBoxTest.java builds every box from a rocket model (the aerodynamic and
// full bounding boxes of the Estes Alpha III, Beta, Falcon 9 Heavy and end-plate test rockets);
// those cases move to the rocket component tests. The tests below cover the class itself.

TEST(BoundingBox, NewBoxIsEmpty)
{
    const BoundingBox box;
    EXPECT_TRUE(box.isEmpty());
    EXPECT_TRUE(box.min().exactlyEquals(Coordinate(kMaxDouble, kMaxDouble, kMaxDouble, 0)));
    EXPECT_TRUE(box.max().exactlyEquals(Coordinate(-kMaxDouble, -kMaxDouble, -kMaxDouble, 0)));
    static_assert(BoundingBox().isEmpty());
}

TEST(BoundingBox, ConstructorKeepsTheCornersAsGiven)
{
    const BoundingBox box(Coordinate(1, 2, 3, 4), Coordinate(5, 6, 7, 8));
    EXPECT_TRUE(box.min().exactlyEquals(Coordinate(1, 2, 3, 4)));
    EXPECT_TRUE(box.max().exactlyEquals(Coordinate(5, 6, 7, 8)));
    EXPECT_FALSE(box.isEmpty());

    // The corners are not put in order: a reversed pair is an empty box
    EXPECT_TRUE(BoundingBox(Coordinate(5, 6, 7), Coordinate(1, 2, 3)).isEmpty());
    EXPECT_TRUE(BoundingBox(Coordinate(0, 0, 1), Coordinate(1, 1, 0)).isEmpty());

    // OpenRocket's FlightConfiguration uses this pair as the box of an empty rocket
    const BoundingBox placeholder(Coordinate::kZero, Coordinate::kXUnit);
    EXPECT_FALSE(placeholder.isEmpty());
    EXPECT_TRUE(placeholder.span().exactlyEquals(Coordinate::kXUnit));
}

TEST(BoundingBox, UpdateWithACoordinateGrowsTheBox)
{
    BoundingBox box;
    box.update(Coordinate(1, 2, 3));
    EXPECT_FALSE(box.isEmpty());
    EXPECT_TRUE(box.min().exactlyEquals(Coordinate(1, 2, 3)));
    EXPECT_TRUE(box.max().exactlyEquals(Coordinate(1, 2, 3)));

    box.update(Coordinate(-1, 5, 0));
    EXPECT_TRUE(box.min().exactlyEquals(Coordinate(-1, 2, 0)));
    EXPECT_TRUE(box.max().exactlyEquals(Coordinate(1, 5, 3)));

    // A point inside changes nothing
    box.update(Coordinate(0, 3, 1));
    EXPECT_TRUE(box.min().exactlyEquals(Coordinate(-1, 2, 0)));
    EXPECT_TRUE(box.max().exactlyEquals(Coordinate(1, 5, 3)));

    // update() returns the box itself, for chaining
    BoundingBox chained;
    EXPECT_EQ(&chained, &chained.update(Coordinate(1, 2, 3)).update(Coordinate(-1, 5, 0)));
    EXPECT_EQ(box, chained);
}

TEST(BoundingBox, UpdateIgnoresTheCoordinateWeight)
{
    BoundingBox box;
    box.update(Coordinate(1, 2, 3, 9));
    EXPECT_TRUE(box.min().exactlyEquals(Coordinate(1, 2, 3, 0)));
    EXPECT_TRUE(box.max().exactlyEquals(Coordinate(1, 2, 3, 0)));

    // The corners keep the weights they were constructed with
    BoundingBox weighted(Coordinate(0, 0, 0, 4), Coordinate(1, 1, 1, 5));
    weighted.update(Coordinate(2, 2, 2, 9));
    EXPECT_TRUE(weighted.min().exactlyEquals(Coordinate(0, 0, 0, 4)));
    EXPECT_TRUE(weighted.max().exactlyEquals(Coordinate(2, 2, 2, 5)));
}

TEST(BoundingBox, UpdateWithAScalarAddsTheDiagonalPoint)
{
    BoundingBox box;
    box.update(2.0).update(-1.0);
    EXPECT_TRUE(box.min().exactlyEquals(Coordinate(-1, -1, -1)));
    EXPECT_TRUE(box.max().exactlyEquals(Coordinate(2, 2, 2)));
}

TEST(BoundingBox, UpdateWithARectangleLeavesZAlone)
{
    BoundingBox box;
    box.update(Rectangle2D{.x = 1.0, .y = 2.0, .width = 3.0, .height = 4.0});
    EXPECT_TRUE(box.min().exactlyEquals(Coordinate(1, 2, kMaxDouble)));
    EXPECT_TRUE(box.max().exactlyEquals(Coordinate(4, 6, -kMaxDouble)));
    EXPECT_TRUE(box.isEmpty());  // still, in z

    box.update(Coordinate(0, 0, 0));
    EXPECT_TRUE(box.min().exactlyEquals(Coordinate(0, 0, 0)));
    EXPECT_TRUE(box.max().exactlyEquals(Coordinate(4, 6, 0)));

    // A negative extent is taken as given (Rectangle2D's getMaxX is x + width)
    BoundingBox negative;
    negative.update(Rectangle2D{.x = 5.0, .y = 5.0, .width = -2.0, .height = -2.0});
    EXPECT_TRUE(negative.min().exactlyEquals(Coordinate(5, 5, kMaxDouble)));
    EXPECT_TRUE(negative.max().exactlyEquals(Coordinate(3, 3, -kMaxDouble)));
}

TEST(BoundingBox, UpdateWithASpanAddsEveryCoordinate)
{
    const std::vector<Coordinate> points{Coordinate(1, 0, 0), Coordinate(0, 2, 0),
                                         Coordinate(0, 0, -3)};
    BoundingBox                   box;
    box.update(points);
    EXPECT_TRUE(box.min().exactlyEquals(Coordinate(0, 0, -3)));
    EXPECT_TRUE(box.max().exactlyEquals(Coordinate(1, 2, 0)));

    const std::array<Coordinate, 2> pair{Coordinate(-5, 1, 1), Coordinate(5, 1, 1)};
    box.update(pair);
    EXPECT_TRUE(box.min().exactlyEquals(Coordinate(-5, 0, -3)));
    EXPECT_TRUE(box.max().exactlyEquals(Coordinate(5, 2, 1)));

    BoundingBox untouched;
    untouched.update(std::vector<Coordinate>{});
    EXPECT_TRUE(untouched.isEmpty());
    EXPECT_EQ(BoundingBox(), untouched);
}

TEST(BoundingBox, UpdateWithABoxMergesItAndSkipsAnEmptyOne)
{
    BoundingBox box(Coordinate(0, 0, 0), Coordinate(1, 1, 1));
    box.update(BoundingBox(Coordinate(-1, 0.5, 0.5), Coordinate(0.5, 2, 0.5)));
    EXPECT_TRUE(box.min().exactlyEquals(Coordinate(-1, 0, 0)));
    EXPECT_TRUE(box.max().exactlyEquals(Coordinate(1, 2, 1)));

    const BoundingBox before = box;
    box.update(BoundingBox());
    EXPECT_EQ(before, box);
    // A box that is empty on one axis only is skipped whole
    box.update(BoundingBox(Coordinate(-9, -9, 5), Coordinate(9, 9, 4)));
    EXPECT_EQ(before, box);

    // Updating an empty box with a full one copies its corners
    BoundingBox fresh;
    fresh.update(box);
    EXPECT_EQ(box, fresh);
    EXPECT_TRUE(fresh.min().exactlyEquals(Coordinate(-1, 0, 0)));
    EXPECT_TRUE(fresh.max().exactlyEquals(Coordinate(1, 2, 1)));
}

TEST(BoundingBox, UpdateWithABoxWithNaNCornersIgnoresTheNaNComponents)
{
    // A NaN corner does not make a box empty (no comparison with NaN holds), so the box is not
    // skipped; its NaN components are then ignored one by one, as in Java
    const BoundingBox nanCorners(Coordinate(kNaN, -5, kNaN), Coordinate(kNaN, 5, 9));
    EXPECT_FALSE(nanCorners.isEmpty());

    BoundingBox box(Coordinate(0, 0, 0), Coordinate(1, 1, 1));
    box.update(nanCorners);
    EXPECT_TRUE(box.min().exactlyEquals(Coordinate(0, -5, 0)));
    EXPECT_TRUE(box.max().exactlyEquals(Coordinate(1, 5, 9)));

    BoundingBox fresh;
    fresh.update(BoundingBox(Coordinate::kNaN, Coordinate::kNaN));
    EXPECT_TRUE(fresh.isEmpty());
    EXPECT_EQ(BoundingBox(), fresh);
}

TEST(BoundingBox, UpdateIgnoresNaNComponents)
{
    BoundingBox box;
    box.update(Coordinate(1, 2, 3));
    box.update(Coordinate(kNaN, 5, kNaN));
    EXPECT_TRUE(box.min().exactlyEquals(Coordinate(1, 2, 3)));
    EXPECT_TRUE(box.max().exactlyEquals(Coordinate(1, 5, 3)));

    BoundingBox fresh;
    fresh.update(Coordinate::kNaN);
    EXPECT_TRUE(fresh.isEmpty());
    EXPECT_EQ(BoundingBox(), fresh);
    fresh.update(kNaN);
    EXPECT_EQ(BoundingBox(), fresh);
}

TEST(BoundingBox, ClearEmptiesTheBox)
{
    BoundingBox box(Coordinate(1, 2, 3, 4), Coordinate(5, 6, 7, 8));
    box.clear();
    EXPECT_TRUE(box.isEmpty());
    EXPECT_EQ(BoundingBox(), box);
    EXPECT_TRUE(box.min().exactlyEquals(Coordinate(kMaxDouble, kMaxDouble, kMaxDouble, 0)));
    EXPECT_TRUE(box.max().exactlyEquals(Coordinate(-kMaxDouble, -kMaxDouble, -kMaxDouble, 0)));
}

TEST(BoundingBox, SpanWidthAndHeight)
{
    const BoundingBox box(Coordinate(1, 2, 3, 4), Coordinate(5, 7, 9, 8));
    // Coordinate::sub keeps the maximum corner's weight
    EXPECT_TRUE(box.span().exactlyEquals(Coordinate(4, 5, 6, 8)));
    EXPECT_DOUBLE_EQ(4, box.width());
    EXPECT_DOUBLE_EQ(5, box.height());

    // An empty box has a negative span
    EXPECT_LT(BoundingBox().span().x, 0);
    EXPECT_LT(BoundingBox().width(), 0);

    static_assert(BoundingBox(Coordinate(1, 2, 3), Coordinate(2, 4, 6))
                      .span()
                      .exactlyEquals(Coordinate(1, 2, 3)));
}

TEST(BoundingBox, RectangleAndCornerViews)
{
    const BoundingBox box(Coordinate(1, 2, 3), Coordinate(5, 7, 9));
    const Rectangle2D rect = box.toRectangle();
    EXPECT_EQ((Rectangle2D{.x = 1.0, .y = 2.0, .width = 4.0, .height = 5.0}), rect);
    EXPECT_DOUBLE_EQ(1, rect.x);
    EXPECT_DOUBLE_EQ(2, rect.y);
    EXPECT_DOUBLE_EQ(4, rect.width);
    EXPECT_DOUBLE_EQ(5, rect.height);

    const std::array<Coordinate, 2> corners = box.toArray();
    EXPECT_TRUE(corners[0].exactlyEquals(box.min()));
    EXPECT_TRUE(corners[1].exactlyEquals(box.max()));

    // OpenRocket's toCollection() lists the maximum corner first
    const std::vector<Coordinate> collection = box.toCollection();
    ASSERT_EQ(2U, collection.size());
    EXPECT_TRUE(collection[0].exactlyEquals(box.max()));
    EXPECT_TRUE(collection[1].exactlyEquals(box.min()));
}

TEST(BoundingBox, TransformMovesTheCorners)
{
    const BoundingBox box(Coordinate(0, 0, 0), Coordinate(1, 2, 3));
    const BoundingBox moved = box.transform(Transformation::translation(10, 20, 30));
    EXPECT_EQ(BoundingBox(Coordinate(10, 20, 30), Coordinate(11, 22, 33)), moved);
    EXPECT_EQ(box, box.transform(Transformation::kIdentity));
    // The original is untouched
    EXPECT_EQ(BoundingBox(Coordinate(0, 0, 0), Coordinate(1, 2, 3)), box);

    // The transformed corners are put back in order: a half turn about z swaps x and y
    const BoundingBox turned = box.transform(Transformation::rotateZ(std::numbers::pi));
    expectCoordinateNear(Coordinate(-1, -2, 0), turned.min());
    expectCoordinateNear(Coordinate(0, 0, 3), turned.max());

    // The result's corners are unweighted whatever the input's were
    const BoundingBox weighted(Coordinate(0, 0, 0, 4), Coordinate(1, 1, 1, 5));
    const BoundingBox copy = weighted.transform(Transformation::kIdentity);
    EXPECT_TRUE(copy.min().exactlyEquals(Coordinate(0, 0, 0, 0)));
    EXPECT_TRUE(copy.max().exactlyEquals(Coordinate(1, 1, 1, 0)));
}

TEST(BoundingBox, TransformBoxesOnlyTheTwoCorners)
{
    // The unit square turned by 45 degrees reaches x = -0.707 and 0.707, but only its two
    // corners (0,0,0) and (1,1,0) -> (0, sqrt 2, 0) are boxed: this is what OpenRocket does
    const BoundingBox unit(Coordinate(0, 0, 0), Coordinate(1, 1, 0));
    const BoundingBox rotated = unit.transform(Transformation::rotateZ(std::numbers::pi / 4));
    EXPECT_NEAR(0, rotated.min().x, kEps);
    EXPECT_NEAR(0, rotated.max().x, kEps);
    EXPECT_NEAR(0, rotated.min().y, kEps);
    EXPECT_NEAR(std::numbers::sqrt2, rotated.max().y, kEps);
    EXPECT_NEAR(0, rotated.min().z, kEps);
    EXPECT_NEAR(0, rotated.max().z, kEps);
}

TEST(BoundingBox, TransformOfAnEmptyBoxIsNotEmpty)
{
    // The +/-max corners are transformed like points and then boxed: the smaller corner comes out
    // of the old maximum and the larger out of the old minimum, so the identity (and a translation,
    // absorbed by +/-max) gives the box of the whole space. Check isEmpty() before transform().
    const BoundingBox whole(Coordinate(-kMaxDouble, -kMaxDouble, -kMaxDouble),
                            Coordinate(kMaxDouble, kMaxDouble, kMaxDouble));
    EXPECT_EQ(whole, BoundingBox().transform(Transformation::kIdentity));
    const BoundingBox translated = BoundingBox().transform(Transformation::translation(1, 2, 3));
    EXPECT_FALSE(translated.isEmpty());
    EXPECT_TRUE(translated.min().exactlyEquals(whole.min()));
    EXPECT_TRUE(translated.max().exactlyEquals(whole.max()));

    // A rotation adds two +/-max products in y, which overflows to infinity
    const BoundingBox rotated =
        BoundingBox().transform(Transformation::rotateZ(std::numbers::pi / 4));
    EXPECT_FALSE(rotated.isEmpty());
    EXPECT_EQ(-kInf, rotated.min().y);
    EXPECT_EQ(kInf, rotated.max().y);
    EXPECT_LE(rotated.min().x, rotated.max().x);
    EXPECT_EQ(-kMaxDouble, rotated.min().z);
    EXPECT_EQ(kMaxDouble, rotated.max().z);
}

TEST(BoundingBox, EqualityIsTolerantAndIncludesTheWeights)
{
    EXPECT_EQ(BoundingBox(), BoundingBox());
    const BoundingBox box(Coordinate(0, 0, 0), Coordinate(1, 2, 3));
    EXPECT_EQ(box, BoundingBox(Coordinate(0, 0, 0), Coordinate(1 + 1e-10, 2, 3)));
    EXPECT_NE(box, BoundingBox(Coordinate(0, 0, 0), Coordinate(1 + 1e-7, 2, 3)));
    EXPECT_NE(box, BoundingBox(Coordinate(0, 0, 1e-7), Coordinate(1, 2, 3)));
    EXPECT_NE(box, BoundingBox(Coordinate(0, 0, 0, 1), Coordinate(1, 2, 3)));
    EXPECT_NE(box, BoundingBox());
    // NaN is equal to nothing
    const BoundingBox nan(Coordinate::kNaN, Coordinate::kNaN);
    EXPECT_NE(nan, nan);

    // The tolerance is relative to the corners of the box on the left (Java's
    // other.min.equals(this.min)): |other - this| < 1e-8 |this|, so right at the edge the two
    // orders differ. 1.5000000150000001 is the double just above 1.5 * (1 + 1e-8): its distance
    // from 1.5 is not below 1e-8 * 1.5 but is below 1e-8 * 1.5000000150000001.
    const BoundingBox box15(Coordinate(0, 0, 0), Coordinate(1.5, 1.5, 1.5));
    const BoundingBox edge(Coordinate(0, 0, 0), Coordinate(1.5000000150000001, 1.5, 1.5));
    EXPECT_FALSE(box15 == edge);
    EXPECT_TRUE(edge == box15);
}

TEST(BoundingBox, ToStringUsesJavaGeneralNotation)
{
    EXPECT_EQ(BoundingBox(Coordinate(0, 0, 0), Coordinate(1, 2.5, 3)).toString(),
              "[( 0.00000, 0.00000, 0.00000) < ( 1.00000, 2.50000, 3.00000)]");
    // The empty box: the largest double, in scientific notation
    EXPECT_EQ(BoundingBox().toString(),
              "[( 1.79769e+308, 1.79769e+308, 1.79769e+308) < "
              "( -1.79769e+308, -1.79769e+308, -1.79769e+308)]");
    // Six significant digits with trailing zeros kept; decimal notation from 1e-4 up to 1e6
    EXPECT_EQ(BoundingBox(Coordinate(0.000123456789, 0.0001, 0.00001),
                          Coordinate(123456, 999999.7, -1234567))
                  .toString(),
              "[( 0.000123457, 0.000100000, 1.00000e-05) < ( 123456, 1.00000e+06, -1.23457e+06)]");
    // NaN and the infinities as Java spells them; a negative zero keeps its sign
    EXPECT_EQ(BoundingBox(Coordinate(kNaN, kInf, -kInf), Coordinate(-0.0, 12.5, -0.5)).toString(),
              "[( NaN, Infinity, -Infinity) < ( -0.00000, 12.5000, -0.500000)]");
}

TEST(BoundingBox, StreamInsertionWritesToString)
{
    const BoundingBox  box(Coordinate(0, 0, 0), Coordinate(1, 2, 3));
    std::ostringstream os;
    os << box;
    EXPECT_EQ(os.str(), box.toString());
}

}  // namespace
