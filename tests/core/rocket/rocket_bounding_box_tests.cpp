// BoundingBoxTest.java (core/src/test/.../rocketcomponent), ported: the bounding boxes of the
// Estes Alpha III, the Beta, the Falcon 9 Heavy and the end-plate rocket of TestRockets.h, with
// Java's expectations and its tolerance (MathUtil.EPSILON). The tests of the BoundingBox class
// itself are in tests/core/util/BoundingBoxTests.cpp.

#include <memory>

#include <gtest/gtest.h>

#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/MathUtil.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::BodyTube;
using QtRocket::BoundingBox;
using QtRocket::FlightConfiguration;
using QtRocket::MassComponent;
using QtRocket::PodSet;
using QtRocket::Rocket;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestEndPlateRocket;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::TestFalcon9Heavy;

/// BoundingBoxTest.EPSILON.
constexpr double kEpsilon = QtRocket::MathUtil::kEpsilon;

// Java: testEstesAlphaIIIBoundingBox
TEST(BoundingBoxTest, EstesAlphaIIIBoundingBox)
{
    const TestEstesAlphaIII alpha;
    const Rocket&           rocket = *alpha.rocket;

    const FlightConfiguration& config = rocket.getSelectedConfiguration();
    const BoundingBox          bounds = config.getBoundingBoxAerodynamic();

    EXPECT_NEAR(0.000000000, bounds.min().x, kEpsilon) << "bounds max x";
    EXPECT_NEAR(0.270000000, bounds.max().x, kEpsilon) << "bounds max x";
    EXPECT_NEAR(-0.032385640, bounds.min().y, kEpsilon) << "bounds min y";
    EXPECT_NEAR(0.062000000, bounds.max().y, kEpsilon) << "bounds max y";
    EXPECT_NEAR(-0.054493575, bounds.min().z, kEpsilon) << "bounds min z";
    EXPECT_NEAR(0.052893575, bounds.max().z, kEpsilon) << "bounds max z";
}

// Java: testBetaBoundingBox
TEST(BoundingBoxTest, BetaBoundingBox)
{
    const TestBeta beta;
    Rocket&        rocket = *beta.rocket;

    FlightConfiguration& config = rocket.getSelectedConfiguration();

    {  // case A: All Stages
        config.setAllStages();

        const BoundingBox bounds = config.getBoundingBoxAerodynamic();

        EXPECT_NEAR(0.000000000, bounds.min().x, kEpsilon) << "bounds min x";
        EXPECT_NEAR(0.335000000, bounds.max().x, kEpsilon) << "bounds max x";
        EXPECT_NEAR(-0.032385640, bounds.min().y, kEpsilon) << "bounds min y";
        EXPECT_NEAR(0.062000000, bounds.max().y, kEpsilon) << "bounds max y";
        EXPECT_NEAR(-0.054493575, bounds.min().z, kEpsilon) << "bounds min z";
        EXPECT_NEAR(0.052893575, bounds.max().z, kEpsilon) << "bounds max z";
    }
    {  // case B: Sustainer Only
        config.setOnlyStage(0);

        const BoundingBox bounds = config.getBoundingBoxAerodynamic();

        EXPECT_NEAR(0.000000000, bounds.min().x, kEpsilon) << "bounds min x";
        EXPECT_NEAR(0.270000000, bounds.max().x, kEpsilon) << "bounds max x";
        EXPECT_NEAR(-0.032385640, bounds.min().y, kEpsilon) << "bounds min y";
        EXPECT_NEAR(0.062000000, bounds.max().y, kEpsilon) << "bounds max y";
        EXPECT_NEAR(-0.054493575, bounds.min().z, kEpsilon) << "bounds min z";
        EXPECT_NEAR(0.052893575, bounds.max().z, kEpsilon) << "bounds max z";
    }
    {  // case C: Booster Only
        config.setOnlyStage(1);

        const BoundingBox bounds = config.getBoundingBoxAerodynamic();

        EXPECT_NEAR(0.270000000, bounds.min().x, kEpsilon) << "bounds min x";
        EXPECT_NEAR(0.335000000, bounds.max().x, kEpsilon) << "bounds max x";
        EXPECT_NEAR(-0.032385640, bounds.min().y, kEpsilon) << "bounds min y";
        EXPECT_NEAR(0.062000000, bounds.max().y, kEpsilon) << "bounds max y";
        EXPECT_NEAR(-0.054493575, bounds.min().z, kEpsilon) << "bounds min z";
        EXPECT_NEAR(0.052893575, bounds.max().z, kEpsilon) << "bounds max z";
    }
}

// Java: testFalcon9HBoundingBox
TEST(BoundingBoxTest, Falcon9HBoundingBox)
{
    const TestFalcon9Heavy f9h;
    const Rocket&          rocket = *f9h.rocket;

    const BoundingBox bounds = rocket.getBoundingBox();
    EXPECT_NEAR(0.0, bounds.min().x, kEpsilon);
    EXPECT_NEAR(1.364, bounds.max().x, kEpsilon);

    EXPECT_NEAR(-0.215500, bounds.min().y, kEpsilon);
    EXPECT_NEAR(0.215500, bounds.max().y, kEpsilon);

    EXPECT_NEAR(-0.12069451, bounds.min().z, kEpsilon);
    EXPECT_NEAR(0.12069451, bounds.max().z, kEpsilon);
}

// Java: testPodsBoundingBox
TEST(BoundingBoxTest, PodsBoundingBox)
{
    const TestEndPlateRocket plates;
    Rocket&                  rocket = *plates.rocket;

    BoundingBox bounds = rocket.getBoundingBox();
    EXPECT_NEAR(0.0, bounds.min().x, kEpsilon);
    EXPECT_NEAR(0.304, bounds.max().x, kEpsilon);

    EXPECT_NEAR(-0.0365, bounds.min().y, kEpsilon);
    EXPECT_NEAR(0.0365, bounds.max().y, kEpsilon);

    EXPECT_NEAR(-0.0365, bounds.min().z, kEpsilon);
    EXPECT_NEAR(0.0365, bounds.max().z, kEpsilon);

    // Add a mass component to the pod set (to test GitHub issue #1849)
    auto& podSet = dynamic_cast<PodSet&>(rocket.getChild(0).getChild(1).getChild(1));
    auto& tube   = dynamic_cast<BodyTube&>(podSet.getChild(0));
    tube.addChild(std::make_unique<MassComponent>());

    bounds = rocket.getBoundingBox();
    EXPECT_NEAR(0.0, bounds.min().x, kEpsilon);
    EXPECT_NEAR(0.304, bounds.max().x, kEpsilon);

    EXPECT_NEAR(-0.0365, bounds.min().y, kEpsilon);
    EXPECT_NEAR(0.0365, bounds.max().y, kEpsilon);

    EXPECT_NEAR(-0.0365, bounds.min().z, kEpsilon);
    EXPECT_NEAR(0.0365, bounds.max().z, kEpsilon);
}

}  // namespace
