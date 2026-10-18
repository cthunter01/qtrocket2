#include "QtRocket/figure/LaunchLugShapes.h"

#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Transformation.h"
#include "figure/BodyShapePins.h"
#include "figure/ShapePinSupport.h"

namespace
{

using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::ComponentShape;
using QtRocket::FigureEllipse;
using QtRocket::FigureRect;
using QtRocket::FigureShape;
using QtRocket::LaunchLug;
using QtRocket::Transformation;
using QtRocket::Test::handBuiltCasesAsJava;
using QtRocket::Test::BodyShapePins::kHandBuilt;

namespace LaunchLugShapes = QtRocket::LaunchLugShapes;

TEST(LaunchLugShapes, AsJava)
{
    LaunchLug lug;
    lug.setLength(0.035);
    lug.setOuterRadius(0.0035);
    EXPECT_TRUE(handBuiltCasesAsJava(&LaunchLugShapes::getShapesSide,
                                     &LaunchLugShapes::getShapesBack, lug, kHandBuilt,
                                     "launch-lug"));
}

TEST(LaunchLugShapes, ATubeOfTheOuterRadius)
{
    LaunchLug lug;
    lug.setLength(0.035);
    lug.setOuterRadius(0.0035);
    lug.setThickness(0.001);
    const Transformation t = Transformation::translation(0.2, 0.014, 0);

    const std::vector<ComponentShape> side = LaunchLugShapes::getShapesSide(lug, t);
    ASSERT_EQ(side.size(), 1U);
    EXPECT_EQ(side[0].shape(), (FigureShape{FigureRect{0.2, 0.014 - 0.0035, 0.035, 2 * 0.0035}}));

    const std::vector<ComponentShape> back = LaunchLugShapes::getShapesBack(lug, t);
    ASSERT_EQ(back.size(), 1U);
    EXPECT_EQ(back[0].shape(),
              (FigureShape{FigureEllipse{0 - 0.0035, 0.014 - 0.0035, 2 * 0.0035, 2 * 0.0035}}));
}

TEST(LaunchLugShapes, AnotherComponentIsABug)
{
    // Java's cast to LaunchLug throws a ClassCastException.
    const BodyTube tube;
    EXPECT_THROW((void)LaunchLugShapes::getShapesSide(tube, Transformation::kIdentity), BugError);
    EXPECT_THROW((void)LaunchLugShapes::getShapesBack(tube, Transformation::kIdentity), BugError);
}

}  // namespace
