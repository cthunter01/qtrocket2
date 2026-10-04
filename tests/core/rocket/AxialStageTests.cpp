#include "QtRocket/rocket/AxialStage.h"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::FlightConfigurationId;
using QtRocket::InnerTube;
using QtRocket::NoseCone;
using QtRocket::Parachute;
using QtRocket::ParallelStage;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::ShockCord;
using QtRocket::StageSeparationConfiguration;
using QtRocket::Streamer;
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
    EXPECT_TRUE(stage.isCompatible(NoseCone{}));
    EXPECT_FALSE(stage.isCompatible(InnerTube{}));
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

TEST(AxialStage, ABoosterSetCannotBeSlicedIntoAStage)
{
    // The copy constructor is protected (as Transition's, for the NoseCone); copies come from
    // cloneShallow(), which keeps the class.
    static_assert(!std::is_copy_constructible_v<AxialStage>);
    static_assert(std::is_copy_constructible_v<ParallelStage>);

    ParallelStage                boosters(3);
    StageSeparationConfiguration separation;
    separation.setSeparationEvent(SeparationEvent::BURNOUT);
    const FlightConfigurationId fcid;
    boosters.getSeparationConfigurations().set(fcid, separation);

    const std::unique_ptr<RocketComponent> copy = boosters.copyWithOriginalId();
    EXPECT_EQ(copy->kind(), ComponentKind::PARALLEL_STAGE);
    const auto& copiedBoosters = dynamic_cast<const ParallelStage&>(*copy);
    EXPECT_EQ(copiedBoosters.getInstanceCount(), 3);
    EXPECT_EQ(copiedBoosters.getAxialMethod(), AxialMethod::BOTTOM);
    EXPECT_EQ(copiedBoosters.getSeparationConfigurations().get(fcid).getSeparationEvent(),
              SeparationEvent::BURNOUT);

    // A stage is copied as a stage.
    AxialStage stage;
    stage.getSeparationConfigurations().set(fcid, separation);
    const std::unique_ptr<RocketComponent> stageCopy = stage.copyWithNewIds();
    EXPECT_EQ(stageCopy->kind(), ComponentKind::AXIAL_STAGE);
    EXPECT_NE(stageCopy->getId(), stage.getId());
    const auto& copiedStage = dynamic_cast<const AxialStage&>(*stageCopy);
    EXPECT_EQ(copiedStage.getSeparationConfigurations().get(fcid).getSeparationEvent(),
              SeparationEvent::BURNOUT);
}

TEST(AxialStage, ActiveWhileItHasChildren)
{
    Rocket      rocket;
    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    rocket.enableEvents();
    EXPECT_FALSE(stage.isStageActive()) << "a stage without children is inactive";
    stage.addChild(std::make_unique<BodyTube>(0.1, 0.02));
    EXPECT_TRUE(stage.isStageActive());
    EXPECT_TRUE(stage.isStageActive(rocket.getSelectedConfiguration()));

    const AxialStage detached;
    EXPECT_THROW(static_cast<void>(detached.isStageActive()), BugError);
}

TEST(AxialStage, TheFlagsWaitForAnUpdateWhileEventsAreDisabled)
{
    // As in Java, the selected configuration learns of a new stage on the next update.
    Rocket      rocket;
    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    stage.addChild(std::make_unique<BodyTube>(0.1, 0.02));
    EXPECT_FALSE(stage.isStageActive()) << "no flag yet";
    rocket.enableEvents();
    EXPECT_TRUE(stage.isStageActive());
}

TEST(AxialStage, SeparationOfTheSelectedConfiguration)
{
    Rocket      rocket;
    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    stage.addChild(std::make_unique<BodyTube>(0.1, 0.02));
    rocket.enableEvents();

    // In the default configuration it is the default.
    EXPECT_EQ(&stage.getSeparationConfiguration(),
              &stage.getSeparationConfigurations().getDefault());
    EXPECT_EQ(stage.getSeparationConfigurations().size(), 0U);

    // In another one, a copy is stored first, so that editing it leaves the default alone.
    const FlightConfigurationId fcid;
    rocket.createFlightConfiguration(fcid);
    rocket.setSelectedConfiguration(fcid);
    StageSeparationConfiguration& separation = stage.getSeparationConfiguration();
    EXPECT_NE(&separation, &stage.getSeparationConfigurations().getDefault());
    EXPECT_TRUE(stage.getSeparationConfigurations().containsId(fcid));
    separation.setSeparationEvent(SeparationEvent::APOGEE);
    EXPECT_EQ(stage.getSeparationConfigurations().getDefault().getSeparationEvent(),
              SeparationEvent::EJECTION);
    EXPECT_EQ(&stage.getSeparationConfiguration(), &separation) << "the same one next time";
}

// ---- Ported from AxialStageTest.java, on TestRockets.makeFalcon9Heavy()'s shape ----

/// The selected configuration and a second one of TestRockets.makeFalcon9Heavy().
class DisableStageTest : public ::testing::Test
{
protected:
    DisableStageTest()
      : m_config(&m_f9h.rocket->getSelectedConfiguration()),
        m_config2(&m_f9h.rocket->createFlightConfiguration(FlightConfigurationId{}))
    {
    }

    /// Whether stages 0, 1 and 2 are active in @p config.
    [[nodiscard]] static std::vector<bool> active(const QtRocket::FlightConfiguration& config,
                                                  int                                  count = 3)
    {
        std::vector<bool> result;
        result.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; i++)
        {
            result.push_back(config.isStageActive(i));
        }
        return result;
    }

    QtRocket::Test::TestFalcon9Heavy m_f9h;
    QtRocket::FlightConfiguration*   m_config;
    QtRocket::FlightConfiguration*   m_config2;
};

using Flags = std::vector<bool>;

TEST_F(DisableStageTest, DisableStage)
{
    QtRocket::FlightConfiguration& config  = *m_config;
    QtRocket::FlightConfiguration& config2 = *m_config2;

    // Disable the payload stage.
    config.setStageActive(0, false);
    EXPECT_EQ(active(config), (Flags{false, true, true}));
    EXPECT_EQ(active(config2), (Flags{true, true, true}));

    // Enable the payload stage.
    config.setStageActive(0, true);
    EXPECT_EQ(active(config), (Flags{true, true, true}));
    EXPECT_EQ(active(config2), (Flags{true, true, true}));

    // Toggle the payload stage to false, then to true.
    config.toggleStage(0);
    EXPECT_EQ(active(config), (Flags{false, true, true}));
    EXPECT_EQ(active(config2), (Flags{true, true, true}));
    config.toggleStage(0);
    EXPECT_EQ(active(config), (Flags{true, true, true}));
    EXPECT_EQ(active(config2), (Flags{true, true, true}));

    // Only one stage.
    config.setOnlyStage(1);
    EXPECT_EQ(active(config), (Flags{false, true, false}));
    EXPECT_EQ(active(config2), (Flags{true, true, true}));

    // Stage activeness in the other configuration.
    config2.toggleStage(1);
    EXPECT_EQ(active(config), (Flags{false, true, false}));
    EXPECT_EQ(active(config2), (Flags{true, false, false}));
    config.setAllStages();
    EXPECT_EQ(active(config), (Flags{true, true, true}));
    EXPECT_EQ(active(config2), (Flags{true, false, false}));

    // With and without the sub-stages.
    config.setAllStages();
    config2.setAllStages();
    config.setStageActive(1, false, true);
    EXPECT_EQ(active(config), (Flags{true, false, false}));
    EXPECT_EQ(active(config2), (Flags{true, true, true}));
    config.setStageActive(1, true, false);
    EXPECT_EQ(active(config), (Flags{true, true, false}));
    EXPECT_EQ(active(config2), (Flags{true, true, true}));
}

TEST_F(DisableStageTest, DisableStageAndMove)
{
    QtRocket::FlightConfiguration&       config  = *m_config;
    const QtRocket::FlightConfiguration& config2 = *m_config2;
    Rocket&                              rocket  = *m_f9h.rocket;

    // Disable the payload stage.
    config.setAllStages();
    config.setStageActive(0, false);
    AxialStage* payloadStage = rocket.getStage(0);

    // Move the payload stage to the back of the rocket.
    rocket.freeze();
    std::unique_ptr<RocketComponent> payload = rocket.removeChild(payloadStage);
    rocket.addChild(std::move(payload));
    rocket.thaw();

    // core, booster, payload
    EXPECT_EQ(active(config), (Flags{true, true, false}));
    EXPECT_EQ(active(config2), (Flags{true, true, true}));

    // Re-enable the payload stage.
    config.setStageActive(payloadStage->getStageNumber(), true);
    EXPECT_EQ(active(config), (Flags{true, true, true}));
    EXPECT_EQ(active(config2), (Flags{true, true, true}));

    // Disable the core stage (and the booster stage).
    config.setStageActive(0, false);
    EXPECT_EQ(active(config), (Flags{false, false, true}));
    EXPECT_EQ(active(config2), (Flags{true, true, true}));

    // Move the core stage to the back of the rocket.
    AxialStage* coreStage = rocket.getStage(0);
    rocket.freeze();
    std::unique_ptr<RocketComponent> core = rocket.removeChild(coreStage);
    rocket.addChild(std::move(core));
    rocket.thaw();

    // payload, core, booster
    EXPECT_EQ(active(config), (Flags{true, false, false}));
    EXPECT_EQ(active(config2), (Flags{true, true, true}));
}

TEST_F(DisableStageTest, DisableStageAndCopy)
{
    QtRocket::FlightConfiguration&       config  = *m_config;
    const QtRocket::FlightConfiguration& config2 = *m_config2;
    Rocket&                              rocket  = *m_f9h.rocket;

    // Disable the core stage.
    config.setAllStages();
    config.setStageActive(1, false);
    const AxialStage* coreStage = rocket.getStage(1);

    // Copy the core stage to the back of the rocket.
    rocket.addChild(QtRocket::componentCast<AxialStage>(coreStage->copyWithNewIds()));

    EXPECT_EQ(active(config, 5), (Flags{true, false, false, true, true}));
    EXPECT_EQ(active(config2, 5), (Flags{true, true, true, true, true}));

    // Disable the copied core stage (not the booster copy).
    config.setStageActive(3, false, false);
    EXPECT_EQ(active(config, 5), (Flags{true, false, false, false, true}));
    EXPECT_EQ(active(config2, 5), (Flags{true, true, true, true, true}));

    // Toggle the original core stage back.
    config.toggleStage(1);
    EXPECT_EQ(active(config, 5), (Flags{true, true, true, false, true}));
    EXPECT_EQ(active(config2, 5), (Flags{true, true, true, true, true}));
}

TEST(AxialStage, RecoveryDevicesOfItsOwn)
{
    Rocket         rocket;
    AxialStage&    core    = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&      body    = core.addChild(std::make_unique<BodyTube>(0.3, 0.02));
    ParallelStage& booster = body.addChild(std::make_unique<ParallelStage>());
    BodyTube&      pod     = booster.addChild(std::make_unique<BodyTube>(0.2, 0.01));
    EXPECT_FALSE(core.hasRecoveryDevice());
    EXPECT_FALSE(booster.hasRecoveryDevice());

    // A shock cord is not a recovery device.
    body.addChild(std::make_unique<ShockCord>());
    EXPECT_FALSE(core.hasRecoveryDevice());

    // A parachute in the booster belongs to the booster only.
    pod.addChild(std::make_unique<Parachute>());
    EXPECT_FALSE(core.hasRecoveryDevice());
    EXPECT_TRUE(booster.hasRecoveryDevice());

    body.addChild(std::make_unique<Streamer>());
    EXPECT_TRUE(core.hasRecoveryDevice());
}

TEST(AxialStage, UpperStage)
{
    Rocket         rocket;
    AxialStage&    payload = rocket.addChild(std::make_unique<AxialStage>());
    AxialStage&    core    = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&      body    = core.addChild(std::make_unique<BodyTube>(0.3, 0.02));
    ParallelStage& booster = body.addChild(std::make_unique<ParallelStage>());

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
    first.addChild(std::make_unique<BodyTube>(0.1, 0.02));
    first.addChild(std::make_unique<BodyTube>(0.25, 0.02));
    second.addChild(std::make_unique<BodyTube>(0.4, 0.02));
    rocket.enableEvents();

    EXPECT_DOUBLE_EQ(first.getLength(), 0.35);
    EXPECT_DOUBLE_EQ(second.getLength(), 0.4);
    EXPECT_EQ(first.getPosition().x, 0.0);
    EXPECT_DOUBLE_EQ(second.getPosition().x, 0.35);
    EXPECT_DOUBLE_EQ(second.getChild(0).getComponentLocations().at(0).x, 0.35);
    EXPECT_DOUBLE_EQ(second.getAxialOffset(), 0.35) << "an assembly computes its offset";
}

/// The stages of TestRockets.makeFalcon9Heavy() that AxialStageTest moves around: a payload stage
/// and a core stage whose body tube holds a booster set.
class StageNumberingTest : public ::testing::Test
{
protected:
    StageNumberingTest()
    {
        m_payload = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_payload->setName("Payload");
        m_payload->addChild(std::make_unique<BodyTube>(0.4, 0.05));
        m_core = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_core->setName("Core");
        BodyTube& coreBody = m_core->addChild(std::make_unique<BodyTube>(0.8, 0.04));
        m_booster          = &coreBody.addChild(std::make_unique<ParallelStage>());
        m_booster->setName("Boosters");
        m_booster->addChild(std::make_unique<BodyTube>(0.6, 0.04));
        m_rocket.enableEvents();
    }

    Rocket         m_rocket;
    AxialStage*    m_payload{nullptr};
    AxialStage*    m_core{nullptr};
    ParallelStage* m_booster{nullptr};
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
