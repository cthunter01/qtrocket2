#include "QtRocket/rocket/AxialStage.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"
#include "rocket/TestComponent.h"

namespace
{

using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BugError;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::FlightConfigurationId;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::StageSeparationConfiguration;
using QtRocket::Test::TestComponent;
using SeparationEvent = StageSeparationConfiguration::SeparationEvent;

TEST(AxialStage, Defaults)
{
    const AxialStage stage;
    EXPECT_EQ(stage.kind(), ComponentKind::AXIAL_STAGE);
    EXPECT_EQ(stage.getName(), "Stage");
    EXPECT_EQ(stage.getStageNumber(), 0);
    EXPECT_EQ(stage.getAxialMethod(), AxialMethod::AFTER);
    EXPECT_TRUE(stage.isAfter());
    EXPECT_TRUE(stage.allowsChildren());
    EXPECT_FALSE(stage.isAerodynamic());
    EXPECT_FALSE(stage.isMassive());
    EXPECT_EQ(stage.getComponentMass(), 0.0);
    EXPECT_TRUE(stage.getComponentBounds().empty());
    EXPECT_TRUE(stage.getInstanceBoundingBox().isEmpty());
    EXPECT_EQ(stage.getSeparationConfigurations().size(), 0U);
    EXPECT_EQ(stage.getSeparationConfigurations().getDefault().getSeparationEvent(),
              SeparationEvent::EJECTION);
    EXPECT_EQ(stage.toDebugSeparation(),
              "====== Dumping ConfigurationSet<StageSeparationConfiguration> (0 configurations)\n");
}

TEST(AxialStage, AcceptsBodyComponentsOnly)
{
    const AxialStage stage;
    EXPECT_TRUE(stage.isCompatible(ComponentKind::BODY_TUBE));
    EXPECT_TRUE(stage.isCompatible(ComponentKind::TRANSITION));
    EXPECT_TRUE(stage.isCompatible(ComponentKind::NOSE_CONE));
    EXPECT_FALSE(stage.isCompatible(ComponentKind::TRAPEZOID_FIN_SET));
    EXPECT_FALSE(stage.isCompatible(ComponentKind::INNER_TUBE));
    EXPECT_FALSE(stage.isCompatible(ComponentKind::AXIAL_STAGE));
    EXPECT_FALSE(stage.isCompatible(ComponentKind::PARALLEL_STAGE));
    EXPECT_TRUE(stage.isCompatible(TestComponent{ComponentKind::NOSE_CONE}));
}

TEST(AxialStage, IsAlwaysPositionedAfter)
{
    AxialStage detached;
    EXPECT_THROW(detached.setAxialMethod(AxialMethod::TOP), BugError);

    Rocket      rocket;
    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    rocket.enableEvents();
    int        events     = 0;
    const auto connection = ComponentChangeSignal::ScopedConnection{
        rocket.addComponentChangeListener([&events](const ComponentChangeEvent& e) {
            EXPECT_EQ(e.getType(), ComponentChangeEvent::kNonFunctionalChange);
            ++events;
        })};
    stage.setAxialMethod(AxialMethod::TOP);
    EXPECT_EQ(stage.getAxialMethod(), AxialMethod::AFTER);
    EXPECT_EQ(events, 1);
}

TEST(AxialStage, SeparationPerConfiguration)
{
    AxialStage                  stage;
    const FlightConfigurationId fcid;
    const FlightConfigurationId other;

    StageSeparationConfiguration apogee;
    apogee.setSeparationEvent(SeparationEvent::APOGEE);
    stage.getSeparationConfigurations().set(fcid, apogee);
    EXPECT_EQ(stage.getSeparationConfigurations().get(fcid).getSeparationEvent(),
              SeparationEvent::APOGEE);
    EXPECT_EQ(stage.getSeparationConfigurations().get(other).getSeparationEvent(),
              SeparationEvent::EJECTION);

    stage.copyFlightConfiguration(fcid, other);
    EXPECT_EQ(stage.getSeparationConfigurations().get(other).getSeparationEvent(),
              SeparationEvent::APOGEE);
    EXPECT_EQ(stage.getSeparationConfigurations().size(), 2U);

    stage.reset(fcid);
    EXPECT_EQ(stage.getSeparationConfigurations().get(fcid).getSeparationEvent(),
              SeparationEvent::EJECTION);
    EXPECT_EQ(stage.getSeparationConfigurations().size(), 1U);
}

TEST(AxialStage, CopiesCloneTheSeparations)
{
    AxialStage                  stage;
    const FlightConfigurationId fcid;
    stage.getSeparationConfigurations().set(fcid, StageSeparationConfiguration{});
    stage.setStageNumber(3);

    const std::unique_ptr<AxialStage> copy =
        QtRocket::componentCast<AxialStage>(stage.copyWithOriginalId());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->getStageNumber(), 3);
    EXPECT_EQ(copy->getSeparationConfigurations().getIds(),
              stage.getSeparationConfigurations().getIds());

    copy->getSeparationConfigurations().get(fcid).setSeparationEvent(SeparationEvent::NEVER);
    copy->getSeparationConfigurations().getDefault().setSeparationDelay(4);
    EXPECT_EQ(stage.getSeparationConfigurations().get(fcid).getSeparationEvent(),
              SeparationEvent::EJECTION);
    EXPECT_EQ(stage.getSeparationConfigurations().getDefault().getSeparationDelay(), 0.0);
}

TEST(AxialStage, ActiveWhileItHasChildren)
{
    Rocket      rocket;
    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    EXPECT_FALSE(stage.isStageActive()) << "a stage without children is inactive";
    stage.addChild(TestComponent::make(0.1));
    EXPECT_TRUE(stage.isStageActive());

    const AxialStage detached;
    EXPECT_THROW(static_cast<void>(detached.isStageActive()), BugError);
}

TEST(AxialStage, RecoveryDevicesOfItsOwn)
{
    Rocket         rocket;
    AxialStage&    core    = rocket.addChild(std::make_unique<AxialStage>());
    TestComponent& body    = core.addChild(TestComponent::make(0.3));
    AxialStage&    booster = body.addChild(std::make_unique<AxialStage>());
    TestComponent& pod     = booster.addChild(TestComponent::make(0.2));
    EXPECT_FALSE(core.hasRecoveryDevice());

    // A parachute in the booster belongs to the booster only.
    pod.addChild(TestComponent::make(0.05, ComponentKind::PARACHUTE));
    EXPECT_FALSE(core.hasRecoveryDevice());
    EXPECT_TRUE(booster.hasRecoveryDevice());

    body.addChild(TestComponent::make(0.05, ComponentKind::STREAMER));
    EXPECT_TRUE(core.hasRecoveryDevice());
    body.addChild(TestComponent::make(0.05, ComponentKind::SHOCK_CORD));
}

TEST(AxialStage, UpperStage)
{
    Rocket         rocket;
    AxialStage&    payload = rocket.addChild(std::make_unique<AxialStage>());
    AxialStage&    core    = rocket.addChild(std::make_unique<AxialStage>());
    TestComponent& body    = core.addChild(TestComponent::make(0.3));
    AxialStage&    booster = body.addChild(std::make_unique<AxialStage>());

    EXPECT_EQ(payload.getUpperStage(), nullptr);
    EXPECT_EQ(core.getUpperStage(), &payload);
    EXPECT_EQ(booster.getUpperStage(), &core) << "a booster's upper stage is its parent's stage";
    AxialStage detached;
    EXPECT_EQ(detached.getUpperStage(), nullptr);
}

TEST(AxialStage, StagesFollowEachOther)
{
    Rocket      rocket;
    AxialStage& first  = rocket.addChild(std::make_unique<AxialStage>());
    AxialStage& second = rocket.addChild(std::make_unique<AxialStage>());
    first.addChild(TestComponent::make(0.1));
    first.addChild(TestComponent::make(0.25));
    second.addChild(TestComponent::make(0.4));
    rocket.enableEvents();

    EXPECT_DOUBLE_EQ(first.getLength(), 0.35);
    EXPECT_DOUBLE_EQ(second.getLength(), 0.4);
    EXPECT_EQ(first.getPosition().x, 0.0);
    EXPECT_DOUBLE_EQ(second.getPosition().x, 0.35);
    EXPECT_DOUBLE_EQ(second.getChild(0).getComponentLocations().at(0).x, 0.35);
    EXPECT_DOUBLE_EQ(second.getAxialOffset(), 0.35) << "an assembly computes its offset";
}

/// The stages of TestRockets.makeFalcon9Heavy() that AxialStageTest moves around: a payload stage
/// and a core stage whose body holds a booster stage.
class StageNumberingTest : public ::testing::Test
{
protected:
    StageNumberingTest()
    {
        m_payload = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_payload->setName("Payload");
        m_payload->addChild(TestComponent::make(0.4));
        m_core = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_core->setName("Core");
        TestComponent& coreBody = m_core->addChild(TestComponent::make(0.8));
        m_booster               = &coreBody.addChild(std::make_unique<AxialStage>());
        m_booster->setName("Boosters");
        m_booster->addChild(TestComponent::make(0.6));
        m_rocket.enableEvents();
    }

    Rocket      m_rocket;
    AxialStage* m_payload{nullptr};
    AxialStage* m_core{nullptr};
    AxialStage* m_booster{nullptr};
};

TEST_F(StageNumberingTest, StagesAreNumberedInTreeOrder)
{
    EXPECT_EQ(m_rocket.getStageCount(), 3U);
    EXPECT_EQ(m_payload->getStageNumber(), 0);
    EXPECT_EQ(m_core->getStageNumber(), 1);
    EXPECT_EQ(m_booster->getStageNumber(), 2);
    EXPECT_EQ(m_rocket.getStage(1), m_core);
    EXPECT_EQ(m_rocket.getStageList(), (std::vector<AxialStage*>{m_payload, m_core, m_booster}));
}

// From AxialStageTest.testDisableStageAndMove: moving the payload stage to the back renumbers
// every stage once the rocket thaws.
TEST_F(StageNumberingTest, MovingAStageRenumbers)
{
    m_rocket.freeze();
    std::unique_ptr<RocketComponent> payload = m_rocket.removeChild(m_payload);
    ASSERT_NE(payload, nullptr);
    m_rocket.addChild(std::move(payload));  // moves to the back
    m_rocket.thaw();

    EXPECT_EQ(m_core->getStageNumber(), 0);
    EXPECT_EQ(m_booster->getStageNumber(), 1);
    EXPECT_EQ(m_payload->getStageNumber(), 2);
    EXPECT_EQ(m_rocket.getStage(0), m_core);
    EXPECT_EQ(m_rocket.getStage(1), m_booster);
    EXPECT_EQ(m_rocket.getStage(2), m_payload);
    EXPECT_EQ(m_rocket.getStageCount(), 3U);

    // Move the core stage (with its booster) to the back as well.
    m_rocket.freeze();
    std::unique_ptr<RocketComponent> core = m_rocket.removeChild(m_core);
    m_rocket.addChild(std::move(core));
    m_rocket.thaw();
    EXPECT_EQ(m_payload->getStageNumber(), 0);
    EXPECT_EQ(m_core->getStageNumber(), 1);
    EXPECT_EQ(m_booster->getStageNumber(), 2);
}

// From AxialStageTest.testDisableStageAndCopy: a copy of the core stage takes the next numbers.
TEST_F(StageNumberingTest, CopiedStagesTakeNewNumbers)
{
    std::unique_ptr<AxialStage> copy =
        QtRocket::componentCast<AxialStage>(m_core->copyWithNewIds());
    ASSERT_NE(copy, nullptr);
    AxialStage& coreCopy    = m_rocket.addChild(std::move(copy));
    auto*       boosterCopy = dynamic_cast<AxialStage*>(&coreCopy.getChild(0).getChild(0));
    ASSERT_NE(boosterCopy, nullptr);

    EXPECT_EQ(m_payload->getStageNumber(), 0);
    EXPECT_EQ(m_core->getStageNumber(), 1);
    EXPECT_EQ(m_booster->getStageNumber(), 2);
    EXPECT_EQ(coreCopy.getStageNumber(), 3);
    EXPECT_EQ(boosterCopy->getStageNumber(), 4);
    EXPECT_EQ(m_rocket.getStageCount(), 5U);
    EXPECT_EQ(m_rocket.getStage(4), boosterCopy);
}

TEST_F(StageNumberingTest, RemovingAStageForgetsItAndItsSubStages)
{
    const std::unique_ptr<RocketComponent> core = m_rocket.removeChild(m_core);
    EXPECT_EQ(m_rocket.getStageCount(), 1U);
    EXPECT_EQ(m_rocket.getStage(0), m_payload);
    EXPECT_EQ(m_rocket.getStage(1), nullptr);
    EXPECT_EQ(m_rocket.getStage(2), nullptr);

    // Removing without tracking still drops the stage from the map (deviation: Java keeps the
    // entry, a stale but live object there), so that the map never holds a destroyed stage.
    const std::unique_ptr<RocketComponent> payload =
        m_rocket.removeChild(m_payload, RocketComponent::StageTracking::SKIP);
    EXPECT_EQ(m_rocket.getStageCount(), 0U);
    EXPECT_TRUE(m_rocket.getStageList().empty());
}

TEST_F(StageNumberingTest, ASkippedRemovalLeavesNoDanglingStage)
{
    // The core body holds the booster stage: removing (and destroying) the body without tracking
    // drops the booster from the map too.
    const QtRocket::Uuid boosterId = m_booster->getId();
    static_cast<void>(m_core->removeChild(std::size_t{0}, RocketComponent::StageTracking::SKIP));
    EXPECT_EQ(m_rocket.getStageList(), (std::vector<AxialStage*>{m_payload, m_core}));
    EXPECT_EQ(m_rocket.getStage(boosterId), nullptr);

    static_cast<void>(m_rocket.removeChild(m_core, RocketComponent::StageTracking::SKIP));
    EXPECT_EQ(m_rocket.getStageList(), std::vector<AxialStage*>{m_payload});
    EXPECT_EQ(m_rocket.getStage(1), nullptr);

    // A new stage is numbered against the stages that are left.
    AxialStage& added = m_rocket.addChild(std::make_unique<AxialStage>());
    EXPECT_EQ(m_rocket.getStageList(), (std::vector<AxialStage*>{m_payload, &added}));
    EXPECT_EQ(added.getStageNumber(), 1);
}

TEST(AxialStage, RemovingWithoutTrackingDropsTheStageWithEventsDisabled)
{
    // No event, so no update() that would rebuild the map: removeChild() itself drops it.
    Rocket      rocket;
    AxialStage& first  = rocket.addChild(std::make_unique<AxialStage>());
    AxialStage& second = rocket.addChild(std::make_unique<AxialStage>());
    ASSERT_EQ(rocket.getStageList(), (std::vector<AxialStage*>{&first, &second}));
    static_cast<void>(rocket.removeChild(&second, RocketComponent::StageTracking::SKIP));
    EXPECT_EQ(rocket.getStageList(), std::vector<AxialStage*>{&first});

    AxialStage& third = rocket.addChild(std::make_unique<AxialStage>());
    EXPECT_EQ(third.getStageNumber(), 1);
    EXPECT_EQ(rocket.getStageList(), (std::vector<AxialStage*>{&first, &third}));
}

TEST_F(StageNumberingTest, RenamingAStageIsATreeChange)
{
    int        lastType = 0;
    const auto connection =
        ComponentChangeSignal::ScopedConnection{m_rocket.addComponentChangeListener(
            [&lastType](const ComponentChangeEvent& e) { lastType = e.getType(); })};
    m_booster->setName("Side boosters");
    EXPECT_EQ(lastType, ComponentChangeEvent::kTreeChange);
}

TEST_F(StageNumberingTest, DebugTreeNodeShowsTheStageNumber)
{
    std::string buffer;
    m_core->toDebugTreeNode(buffer, "");
    EXPECT_TRUE(buffer.starts_with("Core (# 1)")) << buffer;
    EXPECT_NE(buffer.find("via: AFTER"), std::string::npos) << buffer;
}

}  // namespace
