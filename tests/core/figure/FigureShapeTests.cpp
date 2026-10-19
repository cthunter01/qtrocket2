#include "QtRocket/figure/FigureShape.h"

#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/FigureArc.h"
#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/FigureLine.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureRoundRect.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/figure/WindingRule.h"
#include "QtRocket/util/Geometry2D.h"

namespace
{

using QtRocket::FigureArc;
using QtRocket::FigureEllipse;
using QtRocket::FigureLine;
using QtRocket::FigurePath;
using QtRocket::FigureRect;
using QtRocket::FigureRoundRect;
using QtRocket::FigureShape;
using QtRocket::PathSegment;
using QtRocket::Rectangle2D;
using QtRocket::WindingRule;

using Type = FigureShape::Type;

TEST(FigureShape, TellsWhichPrimitiveItHolds)
{
    EXPECT_EQ(FigureShape{FigureRect{}}.type(), Type::RECT);
    EXPECT_EQ(FigureShape{FigureEllipse{}}.type(), Type::ELLIPSE);
    EXPECT_EQ(FigureShape{FigureRoundRect{}}.type(), Type::ROUND_RECT);
    EXPECT_EQ(FigureShape{FigureArc{}}.type(), Type::ARC);
    EXPECT_EQ(FigureShape{FigureLine{}}.type(), Type::LINE);
    EXPECT_EQ(FigureShape{FigurePath{}}.type(), Type::PATH);

    const FigureShape rect{FigureRect{1, 2, 3, 4}};
    const auto*       held = rect.getIf<FigureRect>();
    EXPECT_TRUE(held != nullptr && *held == (FigureRect{1, 2, 3, 4}));
    EXPECT_EQ(rect.getIf<FigureEllipse>(), nullptr);
    EXPECT_TRUE(std::holds_alternative<FigureRect>(rect.geometry()));
}

TEST(FigureShape, ForwardsToItsPrimitive)
{
    const FigureRect  rect{1, 2, 3, 4};
    const FigureShape shape{rect};
    EXPECT_EQ(shape.segments(), rect.segments());
    EXPECT_EQ(shape.bounds(), rect.bounds());
    EXPECT_TRUE(shape.contains({2, 3}));
    EXPECT_FALSE(shape.contains({5, 3}));

    const FigureEllipse ellipse{0, 0, 2, 1};
    EXPECT_EQ(FigureShape{ellipse}.segments(), ellipse.segments());
    EXPECT_EQ(FigureShape{ellipse}.bounds(), ellipse.bounds());
    const FigureRoundRect roundRect{0, 0, 2, 1, 0.5, 0.5};
    EXPECT_EQ(FigureShape{roundRect}.segments(), roundRect.segments());
    const FigureArc arc{0, 0, 2, 2, 10, 80, FigureArc::Type::PIE};
    EXPECT_EQ(FigureShape{arc}.segments(), arc.segments());
    EXPECT_EQ(FigureShape{arc}.contains({1.5, 0.5}), arc.contains({1.5, 0.5}));
    const FigureLine line{0, 0, 1, 1};
    EXPECT_EQ(FigureShape{line}.segments(), line.segments());
    EXPECT_FALSE(FigureShape{line}.contains({0.5, 0.5}));
}

TEST(FigureShape, OnlyAPathHasItsOwnWindingRule)
{
    EXPECT_EQ(FigureShape{FigureRect{}}.windingRule(), WindingRule::NON_ZERO);
    EXPECT_EQ(FigureShape{FigureArc{}}.windingRule(), WindingRule::NON_ZERO);
    EXPECT_EQ(FigureShape{FigurePath{}}.windingRule(), WindingRule::NON_ZERO);
    const FigurePath evenOdd{FigurePath::Precision::DOUBLE, WindingRule::EVEN_ODD};
    EXPECT_EQ(FigureShape{evenOdd}.windingRule(), WindingRule::EVEN_ODD);
}

TEST(FigureShape, AnEmptyPathHasZeroBounds)
{
    EXPECT_EQ(FigureShape{FigurePath{}}.bounds(), Rectangle2D{});
    EXPECT_TRUE(FigureShape{FigurePath{}}.segments().empty());
}

TEST(FigureShape, ComparesByPrimitiveAndParameters)
{
    EXPECT_EQ((FigureShape{FigureRect{1, 2, 3, 4}}), (FigureShape{FigureRect{1, 2, 3, 4}}));
    EXPECT_NE((FigureShape{FigureRect{1, 2, 3, 4}}), (FigureShape{FigureRect{1, 2, 3, 5}}));
    EXPECT_NE((FigureShape{FigureRect{1, 2, 3, 4}}), (FigureShape{FigureEllipse{1, 2, 3, 4}}));
    FigurePath a;
    a.moveTo(1, 1);
    FigurePath b{FigurePath::Precision::FLOAT};
    b.moveTo(1, 1);
    EXPECT_EQ(a.segments(), b.segments());
    EXPECT_NE(FigureShape{a}, FigureShape{b});
    EXPECT_EQ(FigureShape{a}.segments(),
              (std::vector<PathSegment>{PathSegment::moveTo({1.0, 1.0})}));
}

}  // namespace
