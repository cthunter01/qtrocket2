#include "QtRocket/figure/TubeFinSetShapes.h"

#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/TubeFinSet.h"
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
using QtRocket::Transformation;
using QtRocket::TubeFinSet;
using QtRocket::Test::handBuiltCasesAsJava;
using QtRocket::Test::BodyShapePins::kHandBuilt;

namespace TubeFinSetShapes = QtRocket::TubeFinSetShapes;

TEST(TubeFinSetShapes, AsJava)
{
    TubeFinSet fins;
    fins.setLength(0.1);
    fins.setOuterRadius(0.01);
    EXPECT_TRUE(handBuiltCasesAsJava(&TubeFinSetShapes::getShapesSide,
                                     &TubeFinSetShapes::getShapesBack, fins, kHandBuilt,
                                     "tube-fin-set"));
}

TEST(TubeFinSetShapes, AFinIsATubeWhoseFrontCentreIsOneOuterRadiusAboveTheOrigin)
{
    TubeFinSet fins;
    fins.setLength(0.1);
    fins.setOuterRadius(0.01);
    const Transformation t = Transformation::translation(0.3, 0.0125, 0.002);

    // loc = t.transform((0, 0.01, 0)) = (0.3, 0.0225, 0.002).
    const std::vector<ComponentShape> side = TubeFinSetShapes::getShapesSide(fins, t);
    ASSERT_EQ(side.size(), 1U);
    EXPECT_EQ(side[0].shape(),
              (FigureShape{FigureRect{0.3, (0.01 + 0.0125) - 0.01, 0.1, 2 * 0.01}}));

    const std::vector<ComponentShape> back = TubeFinSetShapes::getShapesBack(fins, t);
    ASSERT_EQ(back.size(), 1U);
    EXPECT_EQ(
        back[0].shape(),
        (FigureShape{FigureEllipse{0.002 - 0.01, (0.01 + 0.0125) - 0.01, 2 * 0.01, 2 * 0.01}}));
}

TEST(TubeFinSetShapes, AnotherComponentIsABug)
{
    // Java's cast to TubeFinSet throws a ClassCastException.
    const BodyTube tube;
    EXPECT_THROW((void)TubeFinSetShapes::getShapesSide(tube, Transformation::kIdentity), BugError);
    EXPECT_THROW((void)TubeFinSetShapes::getShapesBack(tube, Transformation::kIdentity), BugError);
}

}  // namespace
