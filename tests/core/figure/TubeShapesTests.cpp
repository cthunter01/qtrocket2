#include "QtRocket/figure/TubeShapes.h"

#include <gtest/gtest.h>

#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/util/Transformation.h"

namespace
{

using QtRocket::FigureEllipse;
using QtRocket::FigureRect;
using QtRocket::FigureShape;
using QtRocket::Transformation;

namespace TubeShapes = QtRocket::TubeShapes;

TEST(TubeShapes, TheSideIsARectFromTheTransformedOrigin)
{
    const Transformation t = Transformation::translation(0.25, 0.01, -0.02);
    EXPECT_EQ(TubeShapes::getShapesSide(t, 0.3, 0.0125),
              (FigureShape{FigureRect{0.25, 0.01 - 0.0125, 0.3, 2 * 0.0125}}));
    // The rotation moves the origin's image only through the translation.
    const Transformation rotated =
        Transformation::rotateX(0.5).applyTransformation(Transformation::translation(0.25, 0, 0));
    EXPECT_EQ(TubeShapes::getShapesSide(rotated, 0.3, 0.0125),
              (FigureShape{FigureRect{0.25, 0 - 0.0125, 0.3, 2 * 0.0125}}));
}

TEST(TubeShapes, TheScaleFactorMultipliesEveryValue)
{
    // PrintableNoseCone draws the shoulders in points.
    const Transformation t = Transformation::translation(0.25, 0.01, -0.02);
    EXPECT_EQ(
        TubeShapes::getShapesSide(t, 0.3, 0.0125, 4.0),
        (FigureShape{FigureRect{0.25 * 4.0, (0.01 - 0.0125) * 4.0, 0.3 * 4.0, 2 * 0.0125 * 4.0}}));
}

TEST(TubeShapes, TheBackIsACircleAroundTheTransformedOriginsZAndY)
{
    const Transformation t = Transformation::translation(0.25, 0.01, -0.02);
    EXPECT_EQ(TubeShapes::getShapesBack(t, 0.0125),
              (FigureShape{FigureEllipse{-0.02 - 0.0125, 0.01 - 0.0125, 2 * 0.0125, 2 * 0.0125}}));
}

}  // namespace
