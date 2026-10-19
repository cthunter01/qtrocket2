#include "QtRocket/figure/EmptyShapes.h"

#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/FigureLine.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/util/Transformation.h"
#include "figure/BodyShapePins.h"
#include "figure/ShapePinSupport.h"
#include "figure/ShapePins.h"

namespace
{

using QtRocket::FigureLine;
using QtRocket::FigureRect;
using QtRocket::FigureShape;
using QtRocket::FigureView;
using QtRocket::Transformation;
using QtRocket::Test::figureShapesMatchPins;
using QtRocket::Test::handBuiltPin;
using QtRocket::Test::HandBuiltPin;
using QtRocket::Test::pinnedTransformation;
using QtRocket::Test::BodyShapePins::kHandBuilt;

namespace EmptyShapes = QtRocket::EmptyShapes;

/// The radius the probe draws its markers with.
constexpr double kRadius = 0.0042;

/// One of EmptyShapes' functions.
using MarkerFunction = std::vector<FigureShape> (*)(const Transformation&, double);

/// Success when @p marker gives, under the transformation of the probe's case @p name in
/// @p view, what the same function of EmptyShapes returned in Java.
::testing::AssertionResult markerAsJava(MarkerFunction marker, std::string_view name,
                                        FigureView view)
{
    const HandBuiltPin& pin = handBuiltPin(kHandBuilt, name, view);
    return figureShapesMatchPins(marker(pinnedTransformation(pin.transform), kRadius), pin.shapes);
}

TEST(EmptyShapes, AsJava)
{
    EXPECT_TRUE(markerAsJava(&EmptyShapes::getShapesSide, "empty/axial", FigureView::SIDE));
    EXPECT_TRUE(markerAsJava(&EmptyShapes::getShapesSide, "empty/off", FigureView::SIDE));
    EXPECT_TRUE(markerAsJava(&EmptyShapes::getShapesSideWithSelectionSquare,
                             "empty-selection/axial", FigureView::SIDE));
    EXPECT_TRUE(markerAsJava(&EmptyShapes::getShapesSideWithSelectionSquare, "empty-selection/off",
                             FigureView::SIDE));
    EXPECT_TRUE(markerAsJava(&EmptyShapes::getShapesBack, "empty/axial", FigureView::BACK));
    EXPECT_TRUE(markerAsJava(&EmptyShapes::getShapesBack, "empty/off", FigureView::BACK));
    EXPECT_TRUE(markerAsJava(&EmptyShapes::getShapesBackWithSelectionSquare,
                             "empty-selection/axial", FigureView::BACK));
    EXPECT_TRUE(markerAsJava(&EmptyShapes::getShapesBackWithSelectionSquare, "empty-selection/off",
                             FigureView::BACK));
}

TEST(EmptyShapes, ASquareWithALineOfThreeRadiiOnEachSide)
{
    EXPECT_EQ(EmptyShapes::getLineLength(0.5), 1.5);
    const Transformation t = Transformation::translation(1, 2, 3);
    EXPECT_EQ(EmptyShapes::getShapesSide(t, 0.5), (std::vector<FigureShape>{
                                                      FigureShape{FigureRect{0.5, 1.5, 1, 1}},
                                                      FigureShape{FigureLine{1, 2.5, 1, 4}},
                                                      FigureShape{FigureLine{1.5, 2, 3, 2}},
                                                      FigureShape{FigureLine{1, 1.5, 1, 0}},
                                                      FigureShape{FigureLine{0.5, 2, -1, 2}},
                                                  }));
    // The back view places it at (z, y).
    EXPECT_EQ(EmptyShapes::getShapesBack(t, 0.5).front(),
              (FigureShape{FigureRect{2.5, 1.5, 1, 1}}));
}

TEST(EmptyShapes, TheSelectionSquareComesLastAndHoldsTheLines)
{
    const Transformation           t    = Transformation::translation(1, 2, 3);
    const std::vector<FigureShape> side = EmptyShapes::getShapesSideWithSelectionSquare(t, 0.5);
    ASSERT_EQ(side.size(), 6U);
    EXPECT_EQ(side.back(), (FigureShape{FigureRect{-1, 0, 4, 4}}));
    const std::vector<FigureShape> back = EmptyShapes::getShapesBackWithSelectionSquare(t, 0.5);
    ASSERT_EQ(back.size(), 6U);
    EXPECT_EQ(back.back(), (FigureShape{FigureRect{1, 0, 4, 4}}));
}

}  // namespace
