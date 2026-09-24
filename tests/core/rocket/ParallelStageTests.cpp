#include "QtRocket/rocket/ParallelStage.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "rocket/TestMotorMount.h"

namespace
{

using QtRocket::AngleMethod;
using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BugError;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::FlightConfiguration;
using QtRocket::ParallelStage;
using QtRocket::RadiusMethod;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Test::TestBodyComponent;
using QtRocket::Test::TestFalcon9Heavy;

// ParallelStageTest's tolerance.
constexpr double kEpsilon = 0.000001;

/// Java's protected setAxialOffset(method, offset), which the JUnit tests (in the same package)
/// call: here the public setAxialMethod() and setAxialOffset(), which end in the same place.
void setAxialOffset(RocketComponent& component, AxialMethod method, double offset)
{
    component.setAxialMethod(method);
    component.setAxialOffset(offset);
}

/// ParallelStageTest.createExtraBooster(): a three-booster set with a nose cone (2 m, radius
/// 0.8 m), a body (2 m, radius 0.8 m) and a tail (1 m, radii 1 and 0.5 m), at a free radius of
/// 0.18 m.
std::unique_ptr<ParallelStage> createExtraBooster()
{
    const double tubeRadius = 0.8;

    auto strapon = std::make_unique<ParallelStage>();
    strapon->setName("Booster Stage");
    TestBodyComponent& boosterNose =
        strapon->addChild(TestBodyComponent::make(2.0, tubeRadius, ComponentKind::NOSE_CONE));
    boosterNose.setForeAftRadii(0, tubeRadius);
    boosterNose.setName("Booster Nosecone");
    strapon->addChild(TestBodyComponent::make(2.0, tubeRadius)).setName("Booster Body ");
    TestBodyComponent& boosterTail =
        strapon->addChild(TestBodyComponent::make(1.0, 1.0, ComponentKind::TRANSITION));
    boosterTail.setForeAftRadii(1.0, 0.5);
    boosterTail.setName("Booster Tail");

    strapon->setInstanceCount(3);
    strapon->setRadiusMethod(RadiusMethod::FREE);
    strapon->setRadiusOffset(0.18);
    return strapon;
}

// =========================================================== ported from ParallelStageTest

TEST(ParallelStageTest, SetRocketPositionFail)
{
    const TestFalcon9Heavy f9h;
    Rocket&                rocket = *f9h.rocket;

    // The Rocket stays put.
    rocket.setAxialOffset(+4.8);

    EXPECT_EQ(AxialMethod::ABSOLUTE, rocket.getAxialMethod());
    EXPECT_NEAR(0, rocket.getAxialOffset(), kEpsilon);
    EXPECT_NEAR(0, rocket.getPosition().x, kEpsilon);
}

TEST(ParallelStageTest, CreatePayloadStage)
{
    const TestFalcon9Heavy f9h;
    const Rocket&          rocket       = *f9h.rocket;
    const AxialStage&      payloadStage = *f9h.payloadStage;
    const RocketComponent& payloadNose  = payloadStage.getChild(0);
    const RocketComponent& payloadBody  = payloadStage.getChild(1);
    EXPECT_FALSE(payloadStage.isAncestor(payloadStage));
    EXPECT_TRUE(payloadStage.isAncestor(payloadNose));
    EXPECT_TRUE(rocket.isAncestor(payloadNose));
    EXPECT_FALSE(payloadBody.isAncestor(payloadNose));

    // (getRelativeToStage() is not ported; see AxialStage.)

    // NOLINTNEXTLINE(modernize-use-std-numbers): a length, not 1/sqrt(pi)
    const double expectedPayloadLength = 0.564;
    EXPECT_NEAR(payloadStage.getLength(), expectedPayloadLength, kEpsilon);

    const double expectedPayloadStageX = 0;
    EXPECT_NEAR(payloadStage.getPosition().x, expectedPayloadStageX, kEpsilon);
    EXPECT_NEAR(payloadStage.getComponentLocations().at(0).x, expectedPayloadStageX, kEpsilon);

    EXPECT_NEAR(0, payloadNose.getPosition().x, kEpsilon);
    EXPECT_NEAR(0, payloadNose.getComponentLocations().at(0).x, kEpsilon);

    const double expectedPayloadBodyX = payloadNose.getLength();
    EXPECT_NEAR(payloadBody.getPosition().x, expectedPayloadBodyX, kEpsilon);
    EXPECT_NEAR(payloadBody.getComponentLocations().at(0).x, expectedPayloadBodyX, kEpsilon);
}

TEST(ParallelStageTest, CreateCoreStage)
{
    const TestFalcon9Heavy f9h;

    const AxialStage& payloadStage = *f9h.payloadStage;
    // NOLINTNEXTLINE(modernize-use-std-numbers): a length, not 1/sqrt(pi)
    const double expectedPayloadLength = 0.564;
    const double payloadLength         = payloadStage.getLength();
    EXPECT_NEAR(payloadLength, expectedPayloadLength, kEpsilon);

    const AxialStage& coreStage = *f9h.coreStage;
    EXPECT_NEAR(coreStage.getLength(), 0.8, kEpsilon) << "createTestRocket failed: @ Core size";

    const double expectedCoreStageX = payloadLength;
    EXPECT_NEAR(expectedCoreStageX, 0.564, kEpsilon);
    EXPECT_NEAR(coreStage.getPosition().x, expectedCoreStageX, kEpsilon);
    EXPECT_NEAR(coreStage.getComponentLocations().at(0).x, expectedCoreStageX, kEpsilon);

    const RocketComponent& coreBody = coreStage.getChild(0);
    EXPECT_NEAR(coreBody.getPosition().x, 0.0, kEpsilon);
    EXPECT_NEAR(coreBody.getComponentLocations().at(0).x, expectedCoreStageX, kEpsilon);
}

TEST(ParallelStageTest, StageAncestry)
{
    const TestFalcon9Heavy f9h;
    EXPECT_EQ(f9h.payloadStage->getUpperStage(), nullptr)
        << "sustainer parent is not found correctly";
    EXPECT_EQ(f9h.coreStage->getUpperStage(), f9h.payloadStage)
        << "core parent is not found correctly";
    EXPECT_EQ(f9h.boosterStage->getUpperStage(), f9h.coreStage)
        << "booster parent is not found correctly";
}

TEST(ParallelStageTest, SetStagePositionTopOfStack)
{
    const TestFalcon9Heavy f9h;
    AxialStage&            sustainer = *f9h.payloadStage;
    const Coordinate       expectedPosition{0, 0.0, 0.0};
    const Coordinate       targetPosition{+4.0, 0.0, 0.0};

    // A centreline stage is positioned AFTER, whatever is asked.
    sustainer.setAxialMethod(AxialMethod::ABSOLUTE);
    EXPECT_TRUE(sustainer.isAfter());
    EXPECT_EQ(sustainer.getAxialMethod(), AxialMethod::AFTER);

    sustainer.setAxialOffset(targetPosition.x);
    const std::string rocketTree = f9h.rocket->toDebugTree();

    EXPECT_NEAR(sustainer.getPosition().x, expectedPosition.x, kEpsilon) << rocketTree;
    // For all stages the absolute position equals the relative one: the parent is the Rocket.
    EXPECT_NEAR(sustainer.getComponentLocations().at(0).x, expectedPosition.x, kEpsilon)
        << rocketTree;
}

TEST(ParallelStageTest, BoosterInitializationFreeRadius)
{
    const TestFalcon9Heavy f9h;
    ParallelStage&         parallelBoosterSet = *f9h.boosterStage;

    parallelBoosterSet.setRadiusMethod(RadiusMethod::FREE);
    parallelBoosterSet.setRadiusOffset(2.0);

    EXPECT_EQ(2, parallelBoosterSet.getInstanceCount());
    EXPECT_FALSE(QtRocket::clampToZero(RadiusMethod::FREE));
    EXPECT_EQ(RadiusMethod::FREE, parallelBoosterSet.getRadiusMethod());
    EXPECT_NEAR(2.0, parallelBoosterSet.getRadiusOffset(), kEpsilon);
    EXPECT_NEAR(2.0, parallelBoosterSet.getInstanceLocations().at(0).y, kEpsilon);
}

TEST(ParallelStageTest, BoosterInitializationSurfaceRadius)
{
    const TestFalcon9Heavy f9h;
    ParallelStage&         parallelBoosterStage = *f9h.boosterStage;

    parallelBoosterStage.setRadiusMethod(RadiusMethod::SURFACE);
    // Under SURFACE this has no effect.
    parallelBoosterStage.setRadiusOffset(4.0);

    EXPECT_EQ(2, parallelBoosterStage.getInstanceCount());
    EXPECT_TRUE(QtRocket::clampToZero(RadiusMethod::SURFACE));
    EXPECT_EQ(RadiusMethod::SURFACE, parallelBoosterStage.getRadiusMethod());
    EXPECT_NEAR(0.0, parallelBoosterStage.getRadiusOffset(), kEpsilon);

    const double expectedRadius =
        f9h.coreBody->getOuterRadius() + f9h.boosterBody->getOuterRadius();
    const std::vector<Coordinate> actualInstanceOffsets = parallelBoosterStage.getInstanceOffsets();
    EXPECT_NEAR(0, actualInstanceOffsets.at(0).x, kEpsilon);
    EXPECT_NEAR(expectedRadius, actualInstanceOffsets.at(0).y, kEpsilon);
    EXPECT_NEAR(0, actualInstanceOffsets.at(1).x, kEpsilon);
    EXPECT_NEAR(-expectedRadius, actualInstanceOffsets.at(1).y, kEpsilon);

    const std::vector<Coordinate> actualLocations = parallelBoosterStage.getComponentLocations();
    EXPECT_NEAR(0.484, actualLocations.at(0).x, kEpsilon);
    EXPECT_NEAR(expectedRadius, actualLocations.at(0).y, kEpsilon);
    EXPECT_NEAR(0.484, actualLocations.at(1).x, kEpsilon);
    EXPECT_NEAR(-expectedRadius, actualLocations.at(1).y, kEpsilon);
}

TEST(ParallelStageTest, BoosterInitializationRelativeRadius)
{
    const TestFalcon9Heavy f9h;
    ParallelStage&         parallelBoosterStage = *f9h.boosterStage;

    setAxialOffset(parallelBoosterStage, AxialMethod::BOTTOM, 0.0);
    const double targetRadiusOffset = 0.01;
    parallelBoosterStage.setRadius(
        RadiusMethod::RELATIVE,
        QtRocket::getRadius(RadiusMethod::RELATIVE, parallelBoosterStage.getParent(),
                            &parallelBoosterStage, targetRadiusOffset));

    EXPECT_FALSE(QtRocket::clampToZero(RadiusMethod::RELATIVE));
    EXPECT_EQ(RadiusMethod::RELATIVE, parallelBoosterStage.getRadiusMethod());
    EXPECT_NEAR(targetRadiusOffset, parallelBoosterStage.getRadiusOffset(), kEpsilon);

    const double expectedRadius =
        targetRadiusOffset + f9h.coreBody->getOuterRadius() + f9h.boosterBody->getOuterRadius();
    const std::vector<Coordinate> actualInstanceOffsets = parallelBoosterStage.getInstanceOffsets();
    EXPECT_NEAR(0, actualInstanceOffsets.at(0).x, kEpsilon);
    EXPECT_NEAR(expectedRadius, actualInstanceOffsets.at(0).y, kEpsilon);
    EXPECT_NEAR(0, actualInstanceOffsets.at(1).x, kEpsilon);
    EXPECT_NEAR(-expectedRadius, actualInstanceOffsets.at(1).y, kEpsilon);

    const std::vector<Coordinate> actualLocations = parallelBoosterStage.getComponentLocations();
    EXPECT_NEAR(0.484, actualLocations.at(0).x, kEpsilon);
    EXPECT_NEAR(expectedRadius, actualLocations.at(0).y, kEpsilon);
    EXPECT_NEAR(0.484, actualLocations.at(1).x, kEpsilon);
    EXPECT_NEAR(-expectedRadius, actualLocations.at(1).y, kEpsilon);
}

TEST(ParallelStageTest, BoosterInstanceLocationBottom)
{
    const TestFalcon9Heavy f9h;
    ParallelStage&         boosterStage = *f9h.boosterStage;

    const int targetInstanceCount = 3;
    boosterStage.setInstanceCount(targetInstanceCount);
    boosterStage.setRadiusMethod(RadiusMethod::SURFACE);

    EXPECT_EQ(targetInstanceCount, boosterStage.getInstanceCount());

    const double expectedX = 0.484;
    const double expectedRadiusOffset =
        f9h.coreBody->getOuterRadius() + f9h.boosterBody->getOuterRadius();
    const double angleIncr = std::numbers::pi * 2 / targetInstanceCount;

    const std::vector<Coordinate> instanceAbsoluteCoords = boosterStage.getComponentLocations();
    for (int index = 0; index < targetInstanceCount; ++index)
    {
        const Coordinate& actualPosition =
            instanceAbsoluteCoords.at(static_cast<std::size_t>(index));
        EXPECT_NEAR(expectedX, actualPosition.x, kEpsilon) << "index " << index;
        const double expectedY = expectedRadiusOffset * std::cos(angleIncr * index);
        EXPECT_NEAR(expectedY, actualPosition.y, kEpsilon) << "index " << index;
        const double expectedZ = expectedRadiusOffset * std::sin(angleIncr * index);
        EXPECT_NEAR(expectedZ, actualPosition.z, kEpsilon) << "index " << index;
    }
}

TEST(ParallelStageTest, SetStagePositionOutsideAbsolute)
{
    const TestFalcon9Heavy f9h;
    ParallelStage&         boosterStage = *f9h.boosterStage;

    const double targetAbsoluteX   = 0.8;
    const double expectedRelativeX = 0.236;
    const double expectedAbsoluteX = 0.8;

    // Substages are freely movable.
    setAxialOffset(boosterStage, AxialMethod::ABSOLUTE, targetAbsoluteX);

    EXPECT_EQ(AxialMethod::ABSOLUTE, boosterStage.getAxialMethod());
    EXPECT_NEAR(targetAbsoluteX, boosterStage.getAxialOffset(), kEpsilon);
    EXPECT_NEAR(expectedRelativeX, boosterStage.getAxialOffset(AxialMethod::TOP), kEpsilon);
    EXPECT_NEAR(expectedAbsoluteX, boosterStage.getComponentLocations().at(0).x, kEpsilon);
}

TEST(ParallelStageTest, SetStagePositionCenterline)
{
    const TestFalcon9Heavy f9h;
    AxialStage&            payloadStage = *f9h.payloadStage;

    // (getRelativeToStage() is not ported; see AxialStage.)

    // A centreline stage is not freely movable.
    setAxialOffset(payloadStage, AxialMethod::TOP, 4.0);

    EXPECT_EQ(AxialMethod::AFTER, payloadStage.getAxialMethod());
    EXPECT_NEAR(0.0, payloadStage.getAxialOffset(), kEpsilon);
    EXPECT_NEAR(0.0, payloadStage.getPosition().x, kEpsilon);
    EXPECT_EQ(RadiusMethod::COAXIAL, payloadStage.getRadiusMethod());
    EXPECT_NEAR(0.0, payloadStage.getRadiusOffset(), kEpsilon);
    EXPECT_NEAR(0.0, payloadStage.getComponentLocations().at(0).x, kEpsilon);
}

TEST(ParallelStageTest, SetStagePositionOutsideTop)
{
    const TestFalcon9Heavy f9h;
    ParallelStage&         boosterStage = *f9h.boosterStage;

    const double targetOffset = 0.2;
    setAxialOffset(boosterStage, AxialMethod::TOP, targetOffset);
    const std::string treeDump = f9h.rocket->toDebugTree();

    EXPECT_NEAR(boosterStage.getPosition().x, 0.2, kEpsilon) << treeDump;
    EXPECT_NEAR(boosterStage.getComponentLocations().at(0).x, 0.764, kEpsilon) << treeDump;
    EXPECT_NEAR(boosterStage.getAxialOffset(), targetOffset, kEpsilon) << treeDump;
}

TEST(ParallelStageTest, SetMiddle)
{
    const TestFalcon9Heavy f9h;
    ParallelStage&         boosterStage = *f9h.boosterStage;

    // An 'external' stage is freely movable.
    const double targetOffset = 0.2;
    setAxialOffset(boosterStage, AxialMethod::MIDDLE, targetOffset);

    EXPECT_NEAR(targetOffset, boosterStage.getAxialOffset(), kEpsilon);
    EXPECT_NEAR(0.16, boosterStage.getPosition().x, kEpsilon);
    EXPECT_NEAR(0.724, boosterStage.getComponentLocations().at(0).x, kEpsilon);
}

TEST(ParallelStageTest, SetBottom)
{
    const TestFalcon9Heavy f9h;
    ParallelStage&         boosterStage = *f9h.boosterStage;

    const double targetOffset = 0.2;
    setAxialOffset(boosterStage, AxialMethod::BOTTOM, targetOffset);

    EXPECT_NEAR(0.120, boosterStage.getPosition().x, kEpsilon);
    EXPECT_NEAR(0.684, boosterStage.getComponentLocations().at(0).x, kEpsilon);
    EXPECT_NEAR(targetOffset, boosterStage.getAxialOffset(), kEpsilon);
}

TEST(ParallelStageTest, SetTopGetAbsolute)
{
    const TestFalcon9Heavy f9h;
    ParallelStage&         boosterStage = *f9h.boosterStage;

    const double targetOffset = 0.2;
    setAxialOffset(boosterStage, AxialMethod::TOP, targetOffset);

    EXPECT_NEAR(targetOffset, boosterStage.getAxialOffset(), kEpsilon);
    EXPECT_NEAR(targetOffset, boosterStage.getPosition().x, kEpsilon);
    EXPECT_NEAR(0.2, boosterStage.getPosition().x, kEpsilon);
    EXPECT_NEAR(0.764, boosterStage.getAxialOffset(AxialMethod::ABSOLUTE), kEpsilon);
}

TEST(ParallelStageTest, SetTopGetAfter)
{
    const TestFalcon9Heavy f9h;
    ParallelStage&         boosterStage = *f9h.boosterStage;

    const double targetOffset = 0.2;
    setAxialOffset(boosterStage, AxialMethod::TOP, targetOffset);

    EXPECT_NEAR(targetOffset, boosterStage.getAxialOffset(), kEpsilon);
    EXPECT_NEAR(0.2, boosterStage.getPosition().x, kEpsilon);
    EXPECT_NEAR(-0.6, boosterStage.getAxialOffset(AxialMethod::AFTER), kEpsilon);
}

TEST(ParallelStageTest, SetTopGetMiddle)
{
    const TestFalcon9Heavy f9h;
    ParallelStage&         boosterStage = *f9h.boosterStage;

    const double targetOffset = 0.2;
    setAxialOffset(boosterStage, AxialMethod::TOP, targetOffset);

    EXPECT_NEAR(targetOffset, boosterStage.getAxialOffset(), kEpsilon);
    EXPECT_NEAR(0.2, boosterStage.getPosition().x, kEpsilon);
    EXPECT_NEAR(0.24, boosterStage.getAxialOffset(AxialMethod::MIDDLE), kEpsilon);
}

TEST(ParallelStageTest, SetTopGetBottom)
{
    const TestFalcon9Heavy f9h;
    ParallelStage&         boosterStage = *f9h.boosterStage;

    const double targetOffset = 0.2;
    setAxialOffset(boosterStage, AxialMethod::TOP, targetOffset);

    EXPECT_NEAR(targetOffset, boosterStage.getAxialOffset(), kEpsilon);
    EXPECT_NEAR(0.2, boosterStage.getPosition().x, kEpsilon);
    EXPECT_NEAR(0.28, boosterStage.getAxialOffset(AxialMethod::BOTTOM), kEpsilon);
}

TEST(ParallelStageTest, SetBottomGetTop)
{
    const TestFalcon9Heavy f9h;
    ParallelStage&         boosterStage = *f9h.boosterStage;

    const double targetOffset = 0.2;
    setAxialOffset(boosterStage, AxialMethod::BOTTOM, targetOffset);

    EXPECT_NEAR(targetOffset, boosterStage.getAxialOffset(), kEpsilon);
    EXPECT_NEAR(0.120, boosterStage.getPosition().x, kEpsilon);
    EXPECT_NEAR(0.12, boosterStage.getAxialOffset(AxialMethod::TOP), kEpsilon);
}

TEST(ParallelStageTest, OutsideStageRepositionTopAfterAdd)
{
    const TestFalcon9Heavy f9h;
    ParallelStage&         boosterStage = *f9h.boosterStage;

    const double targetOffset = 2.50;
    setAxialOffset(boosterStage, AxialMethod::TOP, targetOffset);
    const std::string treeDumpBefore = f9h.rocket->toDebugTree();

    // Whatever the initialisation order, a booster keeps its method and offset while children
    // are added.
    EXPECT_NEAR(2.5, boosterStage.getPosition().x, kEpsilon) << treeDumpBefore;
    EXPECT_NEAR(targetOffset, boosterStage.getAxialOffset(), kEpsilon) << treeDumpBefore;

    const std::string treeDumpAfter = f9h.rocket->toDebugTree();
    EXPECT_NEAR(2.5, boosterStage.getPosition().x, kEpsilon) << treeDumpAfter;
    EXPECT_NEAR(targetOffset, boosterStage.getAxialOffset(), kEpsilon) << treeDumpAfter;
}

TEST(ParallelStageTest, StageInitializationMethodValueOrder)
{
    const TestFalcon9Heavy f9h;
    TestBodyComponent&     coreBody = *f9h.coreBody;

    ParallelStage& boosterA = coreBody.addChild(createExtraBooster());
    boosterA.setName("Booster A Stage");
    ParallelStage& boosterB = coreBody.addChild(createExtraBooster());
    boosterB.setName("Booster B Stage");

    // Two boosters initialised with the same commands in either order end up in the same place.
    const double targetOffset   = 4.5;
    const double expectedOffset = 4.5;
    setAxialOffset(boosterA, AxialMethod::TOP, targetOffset);
    boosterB.setAxialMethod(AxialMethod::TOP);
    boosterB.setAxialOffset(targetOffset);
    const std::string treeDump = f9h.rocket->toDebugTree();

    EXPECT_NEAR(expectedOffset, boosterA.getPosition().x, kEpsilon) << treeDump;
    EXPECT_NEAR(expectedOffset, boosterB.getPosition().x, kEpsilon) << treeDump;
}

TEST(ParallelStageTest, StageNumbering)
{
    const TestFalcon9Heavy f9h;
    Rocket&                rocket   = *f9h.rocket;
    FlightConfiguration&   config   = rocket.getSelectedConfiguration();
    TestBodyComponent&     coreBody = *f9h.coreBody;
    ParallelStage&         boosterA = *f9h.boosterStage;

    ParallelStage& boosterB = coreBody.addChild(createExtraBooster());
    boosterB.setName("Booster A Stage");
    setAxialOffset(boosterB, AxialMethod::BOTTOM, 0.0);

    ParallelStage& boosterC = coreBody.addChild(createExtraBooster());
    boosterC.setName("Booster B Stage");
    setAxialOffset(boosterC, AxialMethod::BOTTOM, 0);

    EXPECT_EQ(0, f9h.payloadStage->getStageNumber());
    EXPECT_EQ(1, f9h.coreStage->getStageNumber());
    EXPECT_EQ(2, boosterA.getStageNumber());
    EXPECT_EQ(3, boosterB.getStageNumber());
    EXPECT_EQ(4, boosterC.getStageNumber());

    // Remove booster B.
    const std::unique_ptr<RocketComponent> removed = coreBody.removeChild(1);
    ASSERT_EQ(removed.get(), &boosterB);

    const std::string treedump = rocket.toDebugTree();
    EXPECT_EQ(4, config.getStageCount()) << treedump;
    EXPECT_EQ(4, rocket.getSelectedConfiguration().getStageCount()) << treedump;

    ParallelStage& boosterD = coreBody.addChild(createExtraBooster());
    boosterD.setName("Booster D Stage");
    setAxialOffset(boosterD, AxialMethod::BOTTOM, 0);

    EXPECT_EQ(4, boosterD.getStageNumber());
}

TEST(ParallelStageTest, ToAbsolute)
{
    const TestFalcon9Heavy f9h;
    const std::string      treeDump = f9h.rocket->toDebugTree();

    const Coordinate              input{3, 0, 0};
    const std::vector<Coordinate> actual = f9h.coreStage->toAbsolute(input);
    EXPECT_NEAR(3.564, actual.at(0).x, kEpsilon) << treeDump;
}

TEST(ParallelStageTest, ToRelative)
{
    const TestFalcon9Heavy f9h;
    const AxialStage&      payloadStage = *f9h.payloadStage;
    const RocketComponent& payloadNose  = payloadStage.getChild(1);
    const RocketComponent& payloadBody  = payloadStage.getChild(3);
    const std::string      treeDump     = f9h.rocket->toDebugTree();

    Coordinate input{1, 0, 0};
    Coordinate actual = payloadStage.toAbsolute(input).at(0);
    EXPECT_NEAR(1.0, actual.x, kEpsilon) << treeDump;

    input  = Coordinate{1, 0, 0};
    actual = payloadNose.toRelative(input, payloadBody).at(0);
    EXPECT_NEAR(0.853999, actual.x, kEpsilon) << treeDump;
}

// ================================================================= QtRocket's own cases

TEST(ParallelStage, Defaults)
{
    const ParallelStage boosters;
    EXPECT_EQ(boosters.kind(), ComponentKind::PARALLEL_STAGE);
    EXPECT_EQ(boosters.getName(), "Booster Set");
    EXPECT_EQ(boosters.getInstanceCount(), 2);
    EXPECT_DOUBLE_EQ(boosters.getInstanceAngleIncrement(), std::numbers::pi);
    EXPECT_EQ(boosters.getAxialMethod(), AxialMethod::BOTTOM);
    EXPECT_FALSE(boosters.isAfter());
    EXPECT_EQ(boosters.getRadiusMethod(), RadiusMethod::RELATIVE);
    EXPECT_EQ(boosters.getRadiusOffset(), 0.0);
    EXPECT_EQ(boosters.getAngleMethod(), AngleMethod::RELATIVE);
    EXPECT_EQ(boosters.getAngleOffset(), 0.0);
    EXPECT_EQ(boosters.getPatternName(), "2-ring");
    EXPECT_EQ(boosters.getStageNumber(), 0);
    EXPECT_TRUE(boosters.isCompatible(ComponentKind::BODY_TUBE));
    EXPECT_FALSE(boosters.isCompatible(ComponentKind::PARALLEL_STAGE));

    const ParallelStage three{3};
    EXPECT_EQ(three.getInstanceCount(), 3);
    EXPECT_DOUBLE_EQ(three.getInstanceAngleIncrement(), 2 * std::numbers::pi / 3);
    EXPECT_EQ(three.getPatternName(), "3-ring");
}

TEST(ParallelStage, AngleOffsetIsReducedToPlusMinusPi)
{
    const TestFalcon9Heavy f9h;
    ParallelStage&         boosters = *f9h.boosterStage;
    boosters.setAngleOffset(3 * std::numbers::pi / 2);
    EXPECT_NEAR(boosters.getAngleOffset(), -std::numbers::pi / 2, 1e-12);
    boosters.setAngleMethod(AngleMethod::FIXED);
    EXPECT_EQ(boosters.getAngleMethod(), AngleMethod::FIXED) << "unlike PodSet, it is stored";
    boosters.setInstanceCount(0);
    EXPECT_EQ(boosters.getInstanceCount(), 2) << "at least one instance";
}

TEST(ParallelStage, SetRadiusClampsByTheRequestedMethod)
{
    const TestFalcon9Heavy f9h;
    ParallelStage&         boosters = *f9h.boosterStage;
    ASSERT_EQ(boosters.getRadiusMethod(), RadiusMethod::SURFACE);
    // Unlike PodSet, the requested method decides: FREE keeps the radius.
    boosters.setRadius(RadiusMethod::FREE, 0.3);
    EXPECT_EQ(boosters.getRadiusOffset(), 0.3);
    boosters.setRadius(RadiusMethod::COAXIAL, 0.3);
    EXPECT_EQ(boosters.getRadiusOffset(), 0.0);
}

TEST(ParallelStage, LaunchStage)
{
    const TestFalcon9Heavy f9h;
    FlightConfiguration&   config = f9h.rocket->getSelectedConfiguration();
    EXPECT_TRUE(f9h.boosterStage->isLaunchStage(config)) << "an active booster set launches";
    EXPECT_TRUE(f9h.coreStage->isLaunchStage(config)) << "the bottom core stage";
    EXPECT_FALSE(f9h.payloadStage->isLaunchStage(config));

    config.setStageActive(f9h.boosterStage->getStageNumber(), false);
    EXPECT_FALSE(f9h.boosterStage->isLaunchStage(config));
    EXPECT_TRUE(f9h.coreStage->isLaunchStage(config));

    config.setOnlyStage(0);
    EXPECT_TRUE(f9h.payloadStage->isLaunchStage(config));
    EXPECT_FALSE(f9h.coreStage->isLaunchStage(config));

    config.clearAllStages();
    EXPECT_FALSE(f9h.payloadStage->isLaunchStage(config)) << "no active core stage at all";
}

TEST(ParallelStage, ComponentBoundsEstimate)
{
    const TestFalcon9Heavy f9h;
    ParallelStage&         boosters = *f9h.boosterStage;
    boosters.setRadius(RadiusMethod::FREE, 0.1);
    const std::vector<Coordinate> bounds = boosters.getComponentBounds();
    ASSERT_EQ(bounds.size(), 8U);
    // Four points around x_min = 0.484 and four around x_max = 0.484 + 0.88, at r = 0.1.
    EXPECT_NEAR(bounds[0].x, 0.484, kEpsilon);
    EXPECT_NEAR(bounds[0].y, -0.1, kEpsilon);
    EXPECT_NEAR(bounds[4].x, 0.484 + 0.88, kEpsilon);
    EXPECT_NEAR(bounds[6].z, 0.1, kEpsilon);
}

TEST(ParallelStage, SetAxialMethodNeedsAParent)
{
    ParallelStage boosters;
    EXPECT_THROW(boosters.setAxialMethod(AxialMethod::TOP), BugError);

    const TestFalcon9Heavy f9h;
    f9h.boosterStage->setAxialMethod(AxialMethod::AFTER);
    EXPECT_EQ(f9h.boosterStage->getAxialMethod(), AxialMethod::TOP) << "AFTER is refused";
}

TEST(ParallelStage, CopiesKeepTheSettings)
{
    const TestFalcon9Heavy f9h;
    ParallelStage&         boosters = *f9h.boosterStage;
    boosters.setInstanceCount(3);
    boosters.setAngleOffset(0.25);
    boosters.setRadius(RadiusMethod::FREE, 0.2);
    boosters.getSeparationConfigurations().getDefault().setSeparationDelay(2);

    const std::unique_ptr<RocketComponent> copy   = boosters.copyWithOriginalId();
    const auto&                            copied = dynamic_cast<const ParallelStage&>(*copy);
    EXPECT_EQ(copied.getInstanceCount(), 3);
    EXPECT_DOUBLE_EQ(copied.getInstanceAngleIncrement(), 2 * std::numbers::pi / 3);
    EXPECT_EQ(copied.getAngleOffset(), 0.25);
    EXPECT_EQ(copied.getRadiusMethod(), RadiusMethod::FREE);
    EXPECT_EQ(copied.getRadiusOffset(), 0.2);
    EXPECT_EQ(copied.getStageNumber(), boosters.getStageNumber());
    EXPECT_EQ(copied.getSeparationConfigurations().getDefault().getSeparationDelay(), 2.0);
    EXPECT_EQ(copied.getChildCount(), 2U);
}

}  // namespace
