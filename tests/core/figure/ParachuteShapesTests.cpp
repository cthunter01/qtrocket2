#include "QtRocket/figure/ParachuteShapes.h"

#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureArc.h"
#include "QtRocket/figure/FigureRoundRect.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Transformation.h"
#include "figure/FigureTestSupport.h"
#include "figure/FinMassShapePins.h"
#include "figure/ShapePinSupport.h"

namespace
{

using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::ComponentShape;
using QtRocket::FigureArc;
using QtRocket::FigureRoundRect;
using QtRocket::Parachute;
using QtRocket::Streamer;
using QtRocket::Transformation;
using QtRocket::Test::handBuiltVariantsAsJava;
using QtRocket::Test::kTranscendentalTolerance;
using QtRocket::Test::FinMassShapePins::kHandBuilt;

namespace ParachuteShapes = QtRocket::ParachuteShapes;

TEST(ParachuteShapes, AsJava)
{
    Parachute chute;
    chute.setLength(0.04);
    chute.setRadius(0.015);
    EXPECT_TRUE(handBuiltVariantsAsJava(&ParachuteShapes::getShapesSide,
                                        &ParachuteShapes::getShapesBack, chute, kHandBuilt,
                                        "parachute"));

    // Short and wide, off the axis: the canopy is at most 0.75 of the width.
    Parachute wide;
    wide.setLength(0.01);
    wide.setRadius(0.02);
    wide.setRadialPosition(0.004);
    wide.setRadialDirection(-0.5);
    EXPECT_TRUE(handBuiltVariantsAsJava(&ParachuteShapes::getShapesSide,
                                        &ParachuteShapes::getShapesBack, wide, kHandBuilt,
                                        "parachute-wide", kTranscendentalTolerance));
}

TEST(ParachuteShapes, TheCanopyIsAPieUnderHalfTheHeight)
{
    Parachute chute;
    chute.setLength(0.04);
    chute.setRadius(0.015);
    const std::vector<ComponentShape> side =
        ParachuteShapes::getShapesSide(chute, Transformation::kIdentity);
    ASSERT_EQ(side.size(), 3U);
    const auto* base   = side[0].shape().getIf<FigureRoundRect>();
    const auto* canopy = side[1].shape().getIf<FigureArc>();
    ASSERT_TRUE(base != nullptr && canopy != nullptr);
    // arc = min(0.04, 0.03) * 0.7; the canopy's diameter is half the height, 0.015.
    EXPECT_EQ(*base, (FigureRoundRect{0, -0.015, 0.04, 0.03, 0.03 * 0.7, 0.03 * 0.7}));
    EXPECT_EQ(canopy->type(), FigureArc::Type::PIE);
    EXPECT_EQ(canopy->width(), 0.03 / 2);
    EXPECT_EQ(canopy->angleStart(), 180.0);
    EXPECT_EQ(canopy->angleExtent(), 180.0);
}

TEST(ParachuteShapes, AnyMassObjectWillDo)
{
    // Java casts to MassObject.
    Streamer streamer;
    EXPECT_EQ(ParachuteShapes::getShapesSide(streamer, Transformation::kIdentity).size(), 3U);
    const BodyTube tube;
    EXPECT_THROW((void)ParachuteShapes::getShapesSide(tube, Transformation::kIdentity), BugError);
    EXPECT_THROW((void)ParachuteShapes::getShapesBack(tube, Transformation::kIdentity), BugError);
}

}  // namespace
