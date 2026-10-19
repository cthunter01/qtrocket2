#include "QtRocket/figure/StreamerShapes.h"

#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Transformation.h"
#include "figure/FinMassShapePins.h"
#include "figure/ShapePinSupport.h"

namespace
{

using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::ComponentShape;
using QtRocket::FigurePath;
using QtRocket::PathSegment;
using QtRocket::Streamer;
using QtRocket::Transformation;
using QtRocket::Test::handBuiltVariantsAsJava;
using QtRocket::Test::pinnedSegmentTypes;
using QtRocket::Test::FinMassShapePins::kHandBuilt;

namespace StreamerShapes = QtRocket::StreamerShapes;

TEST(StreamerShapes, AsJava)
{
    Streamer streamer;
    streamer.setLength(0.05);
    streamer.setRadius(0.01);
    EXPECT_TRUE(handBuiltVariantsAsJava(&StreamerShapes::getShapesSide,
                                        &StreamerShapes::getShapesBack, streamer, kHandBuilt,
                                        "streamer"));
}

TEST(StreamerShapes, TheStreamerFluttersAlongTheTopAndTheBottom)
{
    Streamer streamer;
    streamer.setLength(0.05);
    streamer.setRadius(0.01);
    const std::vector<ComponentShape> side =
        StreamerShapes::getShapesSide(streamer, Transformation::kIdentity);
    ASSERT_EQ(side.size(), 2U);
    const auto* path = side[1].shape().getIf<FigurePath>();
    ASSERT_TRUE(path != nullptr);
    std::vector<PathSegment::Type> types;
    for (const PathSegment& segment : path->segments())
    {
        types.push_back(segment.type);
    }
    EXPECT_EQ(types, pinnedSegmentTypes("MLC8LMC8"));
}

TEST(StreamerShapes, AnotherComponentIsABug)
{
    const BodyTube tube;
    EXPECT_THROW((void)StreamerShapes::getShapesSide(tube, Transformation::kIdentity), BugError);
    EXPECT_THROW((void)StreamerShapes::getShapesBack(tube, Transformation::kIdentity), BugError);
}

}  // namespace
