#include "QtRocket/figure/RocketComponentShapeProvider.h"

#include <gtest/gtest.h>

#include "QtRocket/figure/FigureView.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Transformation.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::FigureView;
using QtRocket::Rocket;
using QtRocket::Transformation;

namespace Provider = QtRocket::RocketComponentShapeProvider;

TEST(RocketComponentShapeProvider, TheRocketAndAPlainStageHaveNoShapesInAnyView)
{
    // RocketFigure.addThisShape skips the Rocket and a plain AxialStage, and
    // ComponentAssemblyShapes returns no shapes for a plain stage.
    const Rocket     rocket;
    const AxialStage stage;
    for (const FigureView view : QtRocket::kAllFigureViews)
    {
        EXPECT_TRUE(Provider::getShapes(view, rocket, Transformation::kIdentity).empty());
        EXPECT_TRUE(Provider::getShapes(view, stage, Transformation::kIdentity).empty());
    }
    EXPECT_TRUE(Provider::getShapesSide(stage, Transformation::kIdentity).empty());
    EXPECT_TRUE(Provider::getShapesBack(stage, Transformation::kIdentity).empty());
}

}  // namespace
