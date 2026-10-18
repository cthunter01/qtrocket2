#include "QtRocket/figure/BodyTubeShapes.h"

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

namespace BodyTubeShapes = QtRocket::BodyTubeShapes;

TEST(BodyTubeShapes, AsJava)
{
    BodyTube body;
    body.setLength(0.3);
    body.setOuterRadius(0.0125);
    EXPECT_TRUE(handBuiltCasesAsJava(&BodyTubeShapes::getShapesSide, &BodyTubeShapes::getShapesBack,
                                     body, kHandBuilt, "body-tube"));
}

TEST(BodyTubeShapes, ATubeOfTheOuterRadiusAtTheTransformedOrigin)
{
    BodyTube body;
    body.setLength(0.3);
    body.setOuterRadius(0.0125);
    body.setInnerRadius(0.012);
    const Transformation t = Transformation::translation(0.1, 0.02, -0.03);

    const std::vector<ComponentShape> side = BodyTubeShapes::getShapesSide(body, t);
    ASSERT_EQ(side.size(), 1U);
    EXPECT_EQ(side[0].shape(), (FigureShape{FigureRect{0.1, 0.02 - 0.0125, 0.3, 2 * 0.0125}}));
    EXPECT_EQ(&side[0].component(), &body);

    const std::vector<ComponentShape> back = BodyTubeShapes::getShapesBack(body, t);
    ASSERT_EQ(back.size(), 1U);
    EXPECT_EQ(back[0].shape(),
              (FigureShape{FigureEllipse{-0.03 - 0.0125, 0.02 - 0.0125, 2 * 0.0125, 2 * 0.0125}}));
}

TEST(BodyTubeShapes, AnotherComponentIsABug)
{
    // Java's cast to BodyTube throws a ClassCastException.
    const LaunchLug lug;
    EXPECT_THROW((void)BodyTubeShapes::getShapesSide(lug, Transformation::kIdentity), BugError);
    EXPECT_THROW((void)BodyTubeShapes::getShapesBack(lug, Transformation::kIdentity), BugError);
}

}  // namespace
