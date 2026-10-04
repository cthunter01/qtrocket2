#include "QtRocket/rocket/Rocket.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/DesignType.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/ReferenceType.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TubeCoupler.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"
#include "rocket/TestComponent.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::CenteringRing;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::DesignType;
using QtRocket::EngineBlock;
using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::InnerTube;
using QtRocket::ModId;
using QtRocket::NoseCone;
using QtRocket::Parachute;
using QtRocket::ReferenceType;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::ShockCord;
using QtRocket::Transition;
using QtRocket::TransitionShape;
using QtRocket::TubeCoupler;
using QtRocket::Test::TestComponent;

/// A received event: its type and source.
struct Received
{
    int                    type;
    const RocketComponent* source;
};

/// A rocket with one stage holding a nose cone (0.1 m, base radius 0.015 m) and a body tube
/// (0.3 m, radius 0.02 m), events enabled, and a listener that records every event. Each body
/// holds a TestComponent, the generic component whose setters and counters the event tests
/// drive (a probe: no length, no mass until a test gives it one).
class RocketTest : public ::testing::Test
{
protected:
    RocketTest()
    {
        m_stage = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_nose = &m_stage->addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.1, 0.015));
        m_body = &m_stage->addChild(std::make_unique<BodyTube>(0.3, 0.02));
        m_noseProbe = &m_nose->addChild(
            TestComponent::make(0.0, ComponentKind::MASS_COMPONENT, AxialMethod::TOP));
        m_bodyProbe = &m_body->addChild(
            TestComponent::make(0.0, ComponentKind::MASS_COMPONENT, AxialMethod::TOP));
        m_rocket.enableEvents();
        m_connection = m_rocket.addComponentChangeListener([this](const ComponentChangeEvent& e) {
            m_events.push_back({.type = e.getType(), .source = e.getSource()});
        });
    }

    [[nodiscard]] std::vector<int> types() const
    {
        std::vector<int> result;
        result.reserve(m_events.size());
        for (const Received& event : m_events)
        {
            result.push_back(event.type);
        }
        return result;
    }

    Rocket                                  m_rocket;
    AxialStage*                             m_stage{nullptr};
    NoseCone*                               m_nose{nullptr};
    BodyTube*                               m_body{nullptr};
    TestComponent*                          m_noseProbe{nullptr};
    TestComponent*                          m_bodyProbe{nullptr};
    std::vector<Received>                   m_events;
    ComponentChangeSignal::ScopedConnection m_connection;
};

// ---- Basics ----

TEST(Rocket, Defaults)
{
    const Rocket rocket;
    EXPECT_EQ(rocket.kind(), ComponentKind::ROCKET);
    EXPECT_EQ(rocket.getName(), "Rocket");
    EXPECT_FALSE(rocket.isEventsEnabled());
    EXPECT_FALSE(rocket.isFrozen());
    EXPECT_EQ(rocket.getAxialMethod(), AxialMethod::ABSOLUTE);
    EXPECT_EQ(rocket.getStageNumber(), -1);
    EXPECT_EQ(rocket.getStageCount(), 0U);
    EXPECT_EQ(rocket.getReferenceType(), ReferenceType::MAXIMUM);
    EXPECT_EQ(rocket.getCustomReferenceLength(), Rocket::kDefaultReferenceLength);
    EXPECT_EQ(Rocket::kDefaultReferenceLength, 0.01);
    EXPECT_EQ(rocket.getDesigner(), "");
    EXPECT_EQ(rocket.getRevision(), "");
    EXPECT_EQ(rocket.getKitName(), "");
    EXPECT_EQ(rocket.getDesignType(), DesignType::ORIGINAL);
    EXPECT_FALSE(rocket.isPerfectFinish());
    EXPECT_EQ(rocket.getDocument(), nullptr);
    EXPECT_EQ(rocket.getParent(), nullptr);
    // All modification ids start equal.
    EXPECT_EQ(rocket.getMassModId(), rocket.getModId());
    EXPECT_EQ(rocket.getAerodynamicModId(), rocket.getModId());
    EXPECT_EQ(rocket.getTreeModId(), rocket.getModId());
    EXPECT_EQ(rocket.getFunctionalModId(), rocket.getModId());
}

TEST(Rocket, AcceptsAxialStagesOnly)
{
    const Rocket rocket;
    EXPECT_TRUE(rocket.isCompatible(ComponentKind::AXIAL_STAGE));
    EXPECT_FALSE(rocket.isCompatible(ComponentKind::PARALLEL_STAGE));
    EXPECT_FALSE(rocket.isCompatible(ComponentKind::BODY_TUBE));
    EXPECT_TRUE(rocket.allowsChildren());
}

TEST(Rocket, IsTheOrigin)
{
    Rocket rocket;
    rocket.setAxialMethod(AxialMethod::TOP);
    rocket.setAxialOffset(1.0);
    EXPECT_EQ(rocket.getAxialMethod(), AxialMethod::ABSOLUTE);
    EXPECT_EQ(rocket.getAxialOffset(), 0.0);
    EXPECT_TRUE(rocket.getPosition().exactlyEquals(Coordinate::kZero));
}

TEST_F(RocketTest, LengthAndBoundingRadius)
{
    // The selected configuration's length: the x extent of its active components' bounds.
    EXPECT_DOUBLE_EQ(m_rocket.getLength(), 0.4);
    AxialStage& second = m_rocket.addChild(std::make_unique<AxialStage>());
    EXPECT_DOUBLE_EQ(m_rocket.getLength(), 0.4);
    second.addChild(std::make_unique<BodyTube>(0.2, 0.03));
    // An internal component is not counted, however wide.
    m_body->addChild(std::make_unique<InnerTube>()).setOuterRadius(0.05);
    EXPECT_DOUBLE_EQ(m_stage->getLength(), 0.4);
    EXPECT_DOUBLE_EQ(m_rocket.getLength(), 0.6) << "the second stage counts";
    EXPECT_DOUBLE_EQ(m_rocket.getBoundingRadius(), 0.03);
    EXPECT_DOUBLE_EQ(m_stage->getBoundingRadius(), 0.02);

    // A transition (a nose cone is one) counts with the larger of its radii.
    m_nose->setAftRadius(0.05);
    EXPECT_DOUBLE_EQ(m_stage->getBoundingRadius(), 0.05);
    EXPECT_DOUBLE_EQ(m_rocket.getBoundingRadius(), 0.05);
    m_nose->setAftRadius(0.015);
    EXPECT_DOUBLE_EQ(m_stage->getBoundingRadius(), 0.02);

    // A child's length change reaches the rocket in the same event.
    m_body->setLength(0.5);
    EXPECT_DOUBLE_EQ(m_stage->getLength(), 0.6);
    EXPECT_DOUBLE_EQ(m_rocket.getLength(), 0.8);

    // So do removing and adding a stage back.
    std::unique_ptr<RocketComponent> removed = m_rocket.removeChild(&second);
    EXPECT_DOUBLE_EQ(m_rocket.getLength(), 0.6);
    m_rocket.addChild(std::move(removed));
    EXPECT_DOUBLE_EQ(m_rocket.getLength(), 0.8);
}

TEST_F(RocketTest, OnlyBodyTubesAndTransitionsGiveTheBoundingRadius)
{
    // Java: instanceof BodyTube, instanceof Transition. A component of a body kind that is
    // neither (a TestComponent) does not count, whatever its radius.
    AxialStage&    extra = m_rocket.addChild(std::make_unique<AxialStage>());
    TestComponent& wide  = extra.addChild(TestComponent::make(0.1, ComponentKind::BODY_TUBE));
    wide.setOuterRadius(0.5);
    EXPECT_EQ(extra.getBoundingRadius(), 0.0);
    EXPECT_DOUBLE_EQ(m_rocket.getBoundingRadius(), 0.02);

    TestComponent& cone = extra.addChild(TestComponent::make(0.1, ComponentKind::TRANSITION));
    cone.setOuterRadius(0.5);
    EXPECT_EQ(extra.getBoundingRadius(), 0.0);
}

TEST_F(RocketTest, ANaNRadiusMakesTheBoundingRadiusNaN)
{
    // Java's Math.max keeps a NaN, where std::max would drop it.
    m_body->setOuterRadius(std::numeric_limits<double>::quiet_NaN());
    EXPECT_TRUE(std::isnan(m_stage->getBoundingRadius()));
    EXPECT_TRUE(std::isnan(m_rocket.getBoundingRadius()));
}

// ---- Events ----

TEST(RocketEvents, DisabledUntilEnabled)
{
    Rocket      rocket;
    AxialStage& stage      = rocket.addChild(std::make_unique<AxialStage>());
    int         count      = 0;
    const auto  connection = ComponentChangeSignal::ScopedConnection{
        rocket.addComponentChangeListener([&count](const ComponentChangeEvent&) { ++count; })};
    const ModId before = rocket.getModId();
    stage.addChild(std::make_unique<BodyTube>(0.1, 0.02));
    EXPECT_EQ(count, 0);
    EXPECT_EQ(rocket.getModId(), before);
}

TEST(RocketEvents, EnablingFiresAeromass)
{
    Rocket           rocket;
    AxialStage&      stage = rocket.addChild(std::make_unique<AxialStage>());
    std::vector<int> types;
    const auto       connection =
        ComponentChangeSignal::ScopedConnection{rocket.addComponentChangeListener(
            [&types](const ComponentChangeEvent& e) { types.push_back(e.getType()); })};
    rocket.enableEvents();
    EXPECT_EQ(types, std::vector<int>{ComponentChangeEvent::kAeromassChange});
    EXPECT_TRUE(rocket.isEventsEnabled());
    rocket.enableEvents(true);  // already enabled: nothing
    EXPECT_EQ(types.size(), 1U);

    rocket.enableEvents(false);
    stage.setName("Quiet");
    EXPECT_EQ(types.size(), 1U);
}

TEST_F(RocketTest, ListenersReceiveEveryEventOfTheTree)
{
    m_bodyProbe->setMass(0.4);
    ASSERT_EQ(m_events.size(), 1U);
    EXPECT_EQ(m_events[0].type, ComponentChangeEvent::kMassChange);
    EXPECT_EQ(m_events[0].source, m_bodyProbe);

    // A body tube's own setters reach the listener too.
    m_body->setFilled(true);
    ASSERT_EQ(m_events.size(), 2U);
    EXPECT_EQ(m_events[1].type, ComponentChangeEvent::kMassChange);
    EXPECT_EQ(m_events[1].source, m_body);
}

TEST_F(RocketTest, EveryComponentSeesTheChange)
{
    const int noseBefore = m_noseProbe->componentChangedCount();
    const int bodyBefore = m_bodyProbe->componentChangedCount();
    m_body->setFilled(true);
    EXPECT_EQ(m_noseProbe->componentChangedCount(), noseBefore + 1);
    EXPECT_EQ(m_bodyProbe->componentChangedCount(), bodyBefore + 1);
    EXPECT_EQ(m_noseProbe->lastChangeType(), std::optional{ComponentChangeEvent::kMassChange});
}

TEST_F(RocketTest, ModIdsFollowTheChangeTypes)
{
    const ModId mod        = m_rocket.getModId();
    const ModId mass       = m_rocket.getMassModId();
    const ModId aero       = m_rocket.getAerodynamicModId();
    const ModId tree       = m_rocket.getTreeModId();
    const ModId functional = m_rocket.getFunctionalModId();

    m_body->fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
    EXPECT_GT(m_rocket.getModId(), mod);
    EXPECT_EQ(m_rocket.getMassModId(), m_rocket.getModId());
    EXPECT_EQ(m_rocket.getAerodynamicModId(), aero);
    EXPECT_EQ(m_rocket.getTreeModId(), tree);
    EXPECT_EQ(m_rocket.getFunctionalModId(), m_rocket.getModId());

    const ModId afterMass = m_rocket.getModId();
    m_body->fireComponentChangeEvent(ComponentChangeEvent::kAerodynamicChange);
    EXPECT_EQ(m_rocket.getAerodynamicModId(), m_rocket.getModId());
    EXPECT_EQ(m_rocket.getMassModId(), afterMass);
    EXPECT_NE(m_rocket.getMassModId(), mass);

    m_body->fireComponentChangeEvent(ComponentChangeEvent::kTreeChange);
    EXPECT_EQ(m_rocket.getTreeModId(), m_rocket.getModId());
    EXPECT_NE(m_rocket.getTreeModId(), tree);

    // A non-functional change moves the modification id only.
    const ModId functionalBefore = m_rocket.getFunctionalModId();
    const ModId massBefore       = m_rocket.getMassModId();
    m_body->fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    EXPECT_GT(m_rocket.getModId(), functionalBefore);
    EXPECT_EQ(m_rocket.getFunctionalModId(), functionalBefore);
    EXPECT_EQ(m_rocket.getMassModId(), massBefore);
    EXPECT_NE(functional, functionalBefore);

    // An undo change moves nothing, but is still delivered.
    const ModId modBefore = m_rocket.getModId();
    const auto  count     = m_events.size();
    m_body->fireComponentChangeEvent(ComponentChangeEvent::kUndoChange |
                                     ComponentChangeEvent::kMassChange);
    EXPECT_EQ(m_rocket.getModId(), modBefore);
    EXPECT_EQ(m_rocket.getMassModId(), massBefore);
    EXPECT_EQ(m_events.size(), count + 1);
}

TEST_F(RocketTest, FreezeQueuesAndThawFiresOneCombinedEvent)
{
    const int   before = m_bodyProbe->componentChangedCount();
    const ModId mod    = m_rocket.getModId();

    m_rocket.freeze();
    EXPECT_TRUE(m_rocket.isFrozen());
    m_nose->setFilled(true);                                              // MASS from the nose
    m_body->fireComponentChangeEvent(ComponentChangeEvent::kTreeChange);  // TREE from the body
    m_body->setComment("frozen");                                         // NONFUNCTIONAL
    EXPECT_TRUE(m_events.empty()) << "nothing is delivered while frozen";
    EXPECT_EQ(m_bodyProbe->componentChangedCount(), before);
    EXPECT_GT(m_rocket.getModId(), mod) << "the ids move while frozen";
    const ModId frozenMod = m_rocket.getModId();

    m_rocket.thaw();
    EXPECT_FALSE(m_rocket.isFrozen());
    ASSERT_EQ(m_events.size(), 1U);
    EXPECT_EQ(m_events[0].type, ComponentChangeEvent::kMassChange |
                                    ComponentChangeEvent::kTreeChange |
                                    ComponentChangeEvent::kNonFunctionalChange);
    EXPECT_EQ(m_events[0].source, m_body) << "the last queued event's source";
    EXPECT_EQ(m_bodyProbe->componentChangedCount(), before + 1);
    EXPECT_GT(m_rocket.getModId(), frozenMod) << "the combined event is fired anew";
}

TEST_F(RocketTest, ThawWithoutEventsFiresNothing)
{
    m_rocket.freeze();
    m_rocket.thaw();
    EXPECT_TRUE(m_events.empty());
}

TEST_F(RocketTest, FreezeAndThawMisuseIsABug)
{
    EXPECT_THROW(m_rocket.thaw(), BugError);
    m_rocket.freeze();
    EXPECT_THROW(m_rocket.freeze(), BugError);
    m_rocket.thaw();
    EXPECT_FALSE(m_rocket.isFrozen());
}

TEST_F(RocketTest, ThawSourceThatLeftTheTreeIsTheRocket)
{
    m_rocket.freeze();
    m_nose->setFilled(true);  // the last queued event comes from the nose
    // Take the nose out without an event of its own (the stage bypasses its events) and
    // destroy it before the rocket thaws.
    m_stage->setBypassChangeEvent(true);
    std::unique_ptr<RocketComponent> nose = m_stage->removeChild(m_nose);
    m_stage->setBypassChangeEvent(false);
    ASSERT_NE(nose, nullptr);
    nose.reset();
    m_rocket.thaw();
    ASSERT_EQ(m_events.size(), 1U);
    EXPECT_EQ(m_events[0].source, &m_rocket);
    EXPECT_EQ(m_events[0].type, ComponentChangeEvent::kMassChange);
}

TEST_F(RocketTest, BypassedAndDetachedComponentsFireNothing)
{
    m_bodyProbe->setBypassChangeEvent(true);
    m_bodyProbe->setMass(1.0);
    EXPECT_TRUE(m_events.empty());
    m_bodyProbe->setBypassChangeEvent(false);
    m_bodyProbe->setMass(2.0);
    EXPECT_EQ(m_events.size(), 1U);

    // A body tube that bypasses its events fires nothing either.
    m_body->setBypassChangeEvent(true);
    m_body->setFilled(true);
    EXPECT_EQ(m_events.size(), 1U);
    m_body->setBypassChangeEvent(false);
    m_body->setFilled(false);
    EXPECT_EQ(m_events.size(), 2U);

    TestComponent detached;
    detached.setMass(3.0);
    EXPECT_EQ(m_events.size(), 2U);
}

TEST_F(RocketTest, TreeChangesFireWithMassAndAeroOfTheSubtree)
{
    // A subtree whose root is aerodynamic only and whose child is massive only.
    auto part = TestComponent::make(0.2);
    part->addChild(TestComponent::make(0.05)).setAerodynamic(false);
    part->setMassive(false);
    m_body->addChild(std::move(part));
    ASSERT_EQ(m_events.size(), 1U);
    EXPECT_EQ(m_events[0].type, ComponentChangeEvent::kTreeChange |
                                    ComponentChangeEvent::kAerodynamicChange |
                                    ComponentChangeEvent::kMassChange);

    // A lone non-aerodynamic, non-massive component is a tree change only.
    m_events.clear();
    auto plain = TestComponent::make(0.1);
    plain->setAerodynamic(false);
    plain->setMassive(false);
    TestComponent& added = m_body->addChild(std::move(plain));
    EXPECT_EQ(types(), std::vector<int>{ComponentChangeEvent::kTreeChange});

    m_events.clear();
    m_body->moveChild(&added, 0);
    EXPECT_EQ(types(), std::vector<int>{ComponentChangeEvent::kTreeChange});
    m_events.clear();
    const std::unique_ptr<RocketComponent> removed = m_body->removeChild(&added);
    EXPECT_EQ(types(), std::vector<int>{ComponentChangeEvent::kTreeChange});
    EXPECT_EQ(m_events[0].source, m_body);

    // A real body tube is massive and aerodynamic.
    m_events.clear();
    m_stage->addChild(std::make_unique<BodyTube>(0.1, 0.02));
    EXPECT_EQ(types(), std::vector<int>{ComponentChangeEvent::kTreeChange |
                                        ComponentChangeEvent::kAerodynamicChange |
                                        ComponentChangeEvent::kMassChange});
}

TEST_F(RocketTest, ListenersAreRemovedWithTheirConnection)
{
    int        count = 0;
    const auto connection =
        m_rocket.addComponentChangeListener([&count](const ComponentChangeEvent&) { ++count; });
    m_bodyProbe->setMass(1.0);
    EXPECT_EQ(count, 1);
    EXPECT_TRUE(RocketComponent::removeComponentChangeListener(connection));
    EXPECT_FALSE(RocketComponent::removeComponentChangeListener(connection));
    m_bodyProbe->setMass(2.0);
    EXPECT_EQ(count, 1);
}

TEST_F(RocketTest, AComponentAddsItsListenerToItsRocket)
{
    int        count = 0;
    const auto fromChild =
        m_body->addComponentChangeListener([&count](const ComponentChangeEvent&) { ++count; });
    m_noseProbe->setMass(1.0);
    EXPECT_EQ(count, 1);
    EXPECT_TRUE(RocketComponent::removeChangeListener(fromChild));
}

TEST(RocketEvents, ADetachedComponentHasNoRocketToListenTo)
{
    TestComponent detached;
    EXPECT_THROW(
        static_cast<void>(detached.addComponentChangeListener([](const ComponentChangeEvent&) { })),
        BugError);
}

TEST_F(RocketTest, ResetListenersDisconnectsThemAll)
{
    EXPECT_GE(m_rocket.getListenerCount(), 1U);
    m_rocket.resetListeners();
    EXPECT_EQ(m_rocket.getListenerCount(), 0U);
    m_bodyProbe->setMass(1.0);
    EXPECT_TRUE(m_events.empty());
}

// ---- Ported from ComponentChangeAdapterTest.java: a StateChangeListener is a slot too ----

TEST_F(RocketTest, ChangeListenerReceivesTheEvent)
{
    const ComponentChangeEvent* received   = nullptr;
    int                         type       = 0;
    const auto                  connection = ComponentChangeSignal::ScopedConnection{
        m_body->addChangeListener([&received, &type](const ComponentChangeEvent& e) {
            received = &e;
            type     = e.getType();
        })};
    m_body->fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
    EXPECT_NE(received, nullptr);
    EXPECT_EQ(type, ComponentChangeEvent::kMassChange);
}

TEST_F(RocketTest, ChangeListenerIdentityIsItsConnection)
{
    // Java's adapters were equal when they wrapped the same listener, so that removing one
    // removed "the" listener; here each connection is a listener of its own.
    int        count = 0;
    const auto first =
        m_body->addChangeListener([&count](const ComponentChangeEvent&) { ++count; });
    const auto second =
        m_body->addChangeListener([&count](const ComponentChangeEvent&) { ++count; });
    EXPECT_EQ(first, first);
    EXPECT_FALSE(first == second);
    m_bodyProbe->setMass(1.0);
    EXPECT_EQ(count, 2);
    EXPECT_TRUE(RocketComponent::removeChangeListener(first));
    m_bodyProbe->setMass(2.0);
    EXPECT_EQ(count, 3);
    EXPECT_TRUE(RocketComponent::removeChangeListener(second));
}

TEST_F(RocketTest, EventsForSpecificConfigurations)
{
    const FlightConfigurationId a;
    const FlightConfigurationId b;
    m_rocket.createFlightConfiguration(a);
    m_rocket.createFlightConfiguration(b);
    m_events.clear();
    const ModId aBefore       = m_rocket.getFlightConfiguration(a).getModId();
    const ModId bBefore       = m_rocket.getFlightConfiguration(b).getModId();
    const ModId defaultBefore = m_rocket.getEmptyConfiguration().getModId();

    // Only the configurations given draw new ids; the event itself is the same.
    const std::vector<FlightConfigurationId> ids{a};
    m_rocket.fireComponentChangeEvent(ComponentChangeEvent::kMotorChange, ids);
    ASSERT_EQ(m_events.size(), 1U);
    EXPECT_EQ(m_events[0].type, ComponentChangeEvent::kMotorChange);
    EXPECT_EQ(m_events[0].source, &m_rocket);
    EXPECT_NE(m_rocket.getFlightConfiguration(a).getModId(), aBefore);
    EXPECT_EQ(m_rocket.getFlightConfiguration(b).getModId(), bBefore);
    EXPECT_EQ(m_rocket.getEmptyConfiguration().getModId(), defaultBefore);

    // Without ids, every configuration.
    m_rocket.fireComponentChangeEvent(ComponentChangeEvent::kMotorChange);
    EXPECT_NE(m_rocket.getFlightConfiguration(b).getModId(), bBefore);
    EXPECT_NE(m_rocket.getEmptyConfiguration().getModId(), defaultBefore);

    // A non-functional change draws none.
    const ModId bNow = m_rocket.getFlightConfiguration(b).getModId();
    m_rocket.fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange, b);
    EXPECT_EQ(m_rocket.getFlightConfiguration(b).getModId(), bNow);
}

// ---- Metadata ----

TEST_F(RocketTest, MetadataSettersFire)
{
    m_rocket.setDesigner("Jane");
    m_rocket.setDesigner("Jane");  // fires anyway, as in Java
    m_rocket.setRevision("B");
    m_rocket.setKitName("Kit");
    m_rocket.setDesignType(DesignType::CLONE_KIT);
    EXPECT_EQ(m_rocket.getDesigner(), "Jane");
    EXPECT_EQ(m_rocket.getRevision(), "B");
    EXPECT_EQ(m_rocket.getKitName(), "Kit");
    EXPECT_EQ(m_rocket.getDesignType(), DesignType::CLONE_KIT);
    EXPECT_EQ(types(), std::vector<int>(5, ComponentChangeEvent::kNonFunctionalChange));
}

TEST_F(RocketTest, ReferenceSettings)
{
    m_rocket.setReferenceType(ReferenceType::MAXIMUM);  // unchanged: nothing
    EXPECT_TRUE(m_events.empty());
    m_rocket.setCustomReferenceLength(0.05);  // not CUSTOM: silent
    EXPECT_TRUE(m_events.empty());
    EXPECT_EQ(m_rocket.getCustomReferenceLength(), 0.05);

    m_rocket.setReferenceType(ReferenceType::CUSTOM);
    m_rocket.setCustomReferenceLength(0.0001);  // at least 1 mm
    EXPECT_EQ(m_rocket.getCustomReferenceLength(), 0.001);
    m_rocket.setCustomReferenceLength(0.001 + 1e-12);  // equal: nothing
    EXPECT_EQ(types(), std::vector<int>(2, ComponentChangeEvent::kNonFunctionalChange));
}

TEST_F(RocketTest, PerfectFinishIsAerodynamic)
{
    m_rocket.setPerfectFinish(false);
    EXPECT_TRUE(m_events.empty());
    m_rocket.setPerfectFinish(true);
    EXPECT_TRUE(m_rocket.isPerfectFinish());
    EXPECT_EQ(types(), std::vector<int>{ComponentChangeEvent::kAerodynamicChange});
}

TEST(RocketEnums, ReferenceTypeNames)
{
    EXPECT_EQ(QtRocket::referenceTypeName(ReferenceType::NOSECONE), "NOSECONE");
    EXPECT_EQ(QtRocket::orkName(ReferenceType::NOSECONE), "nosecone");
    EXPECT_EQ(QtRocket::orkName(ReferenceType::MAXIMUM), "maximum");
    EXPECT_EQ(QtRocket::orkName(ReferenceType::CUSTOM), "custom");
    EXPECT_TRUE(std::ranges::all_of(QtRocket::kAllReferenceTypes, [](ReferenceType type) {
        return QtRocket::referenceTypeFromOrkName(QtRocket::orkName(type)) == type;
    }));
    EXPECT_EQ(QtRocket::referenceTypeFromOrkName("largest"), std::nullopt);
}

TEST(RocketEnums, DesignTypeNames)
{
    EXPECT_EQ(QtRocket::designTypeName(DesignType::KIT_BASH), "KIT_BASH");
    // getStorableString(): lower case without underscores.
    EXPECT_EQ(QtRocket::orkName(DesignType::ORIGINAL), "original");
    EXPECT_EQ(QtRocket::orkName(DesignType::COMMERCIAL_KIT), "commercialkit");
    EXPECT_EQ(QtRocket::orkName(DesignType::MODIFIED_KIT), "modifiedkit");
    EXPECT_EQ(QtRocket::orkName(DesignType::KIT_BASH), "kitbash");
    EXPECT_TRUE(std::ranges::all_of(QtRocket::kAllDesignTypes, [](DesignType type) {
        return QtRocket::orkName(type) ==
                   QtRocket::Strings::toOrkEnumName(QtRocket::designTypeName(type)) &&
               QtRocket::designTypeFromOrkName(QtRocket::orkName(type)) == type;
    }));
}

TEST(RocketEnums, DesignTypeDisplayNames)
{
    EXPECT_EQ(QtRocket::displayKey(DesignType::MODIFIED_KIT), "DesignType.Modificationkit");
    EXPECT_EQ(QtRocket::displayName(DesignType::ORIGINAL), "Original Design/Other");
    EXPECT_EQ(QtRocket::displayName(DesignType::KIT_BASH), "Kit Bash of Commercial Kits");
}

// ---- Stages ----

TEST_F(RocketTest, TrackStage)
{
    EXPECT_EQ(m_rocket.getStage(0), m_stage);
    EXPECT_EQ(m_rocket.getStage(m_stage->getId()), m_stage);
    EXPECT_EQ(m_rocket.getStage(5), nullptr);
    EXPECT_EQ(m_rocket.getStage(QtRocket::Uuid{}), nullptr);

    // An equal stage (same class and id) under the same number takes over the entry.
    AxialStage twin;
    twin.setId(m_stage->getId());
    m_rocket.trackStage(twin);
    EXPECT_EQ(m_rocket.getStage(0), &twin);
    EXPECT_EQ(m_rocket.getStageCount(), 1U);
    m_rocket.trackStage(*m_stage);
    EXPECT_EQ(m_rocket.getStage(0), m_stage);

    // Another stage gets the smallest free number.
    AxialStage other;
    other.setStageNumber(0);
    m_rocket.trackStage(other);
    EXPECT_EQ(other.getStageNumber(), 1);
    m_rocket.forgetStage(other);
    EXPECT_EQ(m_rocket.getStageCount(), 1U);
}

TEST_F(RocketTest, ActivenessInTheSelectedConfiguration)
{
    const FlightConfiguration& selected = m_rocket.getSelectedConfiguration();
    EXPECT_TRUE(selected.isStageActive(-1));
    EXPECT_TRUE(selected.isStageActive(0));
    EXPECT_FALSE(selected.isStageActive(1));
    EXPECT_TRUE(selected.isComponentActive(*m_body));
    EXPECT_TRUE(selected.isComponentActive(m_rocket));
    EXPECT_TRUE(m_stage->isStageActive());
}

TEST_F(RocketTest, AnInactiveSiblingIsSkippedByAfter)
{
    // setAfter() places a component after the previous sibling active in the selected
    // configuration: a stage whose flag is cleared no longer takes room.
    AxialStage& second = m_rocket.addChild(std::make_unique<AxialStage>());
    second.addChild(std::make_unique<BodyTube>(0.2, 0.01));
    EXPECT_DOUBLE_EQ(second.getPosition().x, 0.4);
    m_rocket.getSelectedConfiguration().setStageActive(0, false);
    m_rocket.fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange);
    EXPECT_DOUBLE_EQ(second.getPosition().x, 0.0);
}

// ---- Flight configurations ----

TEST_F(RocketTest, TheDefaultConfigurationIsSelected)
{
    EXPECT_EQ(&m_rocket.getSelectedConfiguration(), &m_rocket.getEmptyConfiguration());
    EXPECT_TRUE(m_rocket.getSelectedConfiguration().getId().isDefaultId());
    EXPECT_EQ(&m_rocket.getSelectedConfiguration().getRocket(), &m_rocket);
    EXPECT_EQ(m_rocket.getConfigurationCount(), 0);
    EXPECT_EQ(&m_rocket.getFlightConfigurations().getDefault(), &m_rocket.getEmptyConfiguration());
}

TEST_F(RocketTest, CreateFlightConfiguration)
{
    const FlightConfigurationId a;
    FlightConfiguration&        config = m_rocket.createFlightConfiguration(a);
    EXPECT_EQ(config.getId(), a);
    EXPECT_EQ(&config.getRocket(), &m_rocket);
    ASSERT_EQ(m_events.size(), 1U);
    EXPECT_EQ(m_events[0].type, ComponentChangeEvent::kTreeChange);

    // Again: the same configuration, no event.
    EXPECT_EQ(&m_rocket.createFlightConfiguration(a), &config);
    EXPECT_EQ(m_events.size(), 1U);

    // The error id gives the default.
    EXPECT_EQ(&m_rocket.createFlightConfiguration(FlightConfigurationId::errorId()),
              &m_rocket.getEmptyConfiguration());

    // Java's null id: a new random one.
    FlightConfiguration& fresh = m_rocket.createFlightConfiguration();
    EXPECT_NE(fresh.getId(), a);
    EXPECT_EQ(m_rocket.getIds(), (std::vector<FlightConfigurationId>{a, fresh.getId()}));
    EXPECT_EQ(m_rocket.getConfigurationCount(), 2);
    EXPECT_EQ(m_rocket.getFlightConfigurationId(1), fresh.getId());
    EXPECT_THROW(static_cast<void>(m_rocket.getFlightConfigurationId(2)), BugError);
    EXPECT_THROW(static_cast<void>(m_rocket.getFlightConfigurationId(-1)), BugError);
    EXPECT_EQ(&m_rocket.getFlightConfigurationByIndex(0), &config);
    EXPECT_EQ(&m_rocket.getFlightConfigurationByIndex(0, true), &m_rocket.getEmptyConfiguration());
    EXPECT_EQ(&m_rocket.getFlightConfigurationByIndex(1, true), &config);

    EXPECT_TRUE(m_rocket.containsFlightConfigurationId(a));
    EXPECT_TRUE(m_rocket.containsFlightConfigurationId(FlightConfigurationId::defaultValueId()));
    EXPECT_FALSE(m_rocket.containsFlightConfigurationId(FlightConfigurationId::errorId()));
    EXPECT_FALSE(m_rocket.containsFlightConfigurationId(FlightConfigurationId{}));
    EXPECT_EQ(&m_rocket.getFlightConfiguration(FlightConfigurationId{}),
              &m_rocket.getEmptyConfiguration())
        << "an unknown id gives the default";
}

TEST_F(RocketTest, SelectConfiguration)
{
    const FlightConfigurationId a;
    FlightConfiguration&        config = m_rocket.createFlightConfiguration(a);
    m_events.clear();

    m_rocket.setSelectedConfiguration(a);
    EXPECT_EQ(&m_rocket.getSelectedConfiguration(), &config);
    ASSERT_EQ(m_events.size(), 1U);
    EXPECT_EQ(m_events[0].type, ComponentChangeEvent::kNonFunctionalChange);

    m_rocket.setSelectedConfiguration(a);
    EXPECT_EQ(m_events.size(), 1U) << "already selected: no event";

    // An unknown id selects the default.
    m_rocket.setSelectedConfiguration(FlightConfigurationId{});
    EXPECT_TRUE(m_rocket.getSelectedConfiguration().getId().isDefaultId());
}

TEST_F(RocketTest, RemoveFlightConfiguration)
{
    BodyTube&                   mount = m_stage->addChild(std::make_unique<BodyTube>(0.1, 0.01));
    const FlightConfigurationId a;
    m_rocket.createFlightConfiguration(a);
    QtRocket::Test::addMotor(mount, a, QtRocket::Test::motorD21());
    m_stage->getSeparationConfigurations().set(a, QtRocket::StageSeparationConfiguration{});
    m_rocket.setSelectedConfiguration(a);
    m_events.clear();

    m_rocket.removeFlightConfiguration(a);
    EXPECT_FALSE(m_rocket.containsFlightConfigurationId(a));
    EXPECT_TRUE(m_rocket.getSelectedConfiguration().getId().isDefaultId()) << "the default again";
    EXPECT_FALSE(mount.hasMotor()) << "the components forget the id";
    EXPECT_FALSE(m_stage->getSeparationConfigurations().containsId(a));
    ASSERT_EQ(m_events.size(), 1U);
    EXPECT_EQ(m_events[0].type, ComponentChangeEvent::kNonFunctionalChange);

    // The error id is ignored; the default cannot be removed.
    m_rocket.removeFlightConfiguration(FlightConfigurationId::errorId());
    EXPECT_EQ(m_events.size(), 1U);
    m_rocket.removeFlightConfiguration(FlightConfigurationId::defaultValueId());
    EXPECT_TRUE(m_rocket.containsFlightConfigurationId(FlightConfigurationId::defaultValueId()));
}

TEST_F(RocketTest, SetFlightConfiguration)
{
    const FlightConfigurationId a;
    m_rocket.setFlightConfiguration(a, FlightConfiguration{m_rocket, a});
    EXPECT_TRUE(m_rocket.containsFlightConfigurationId(a));
    ASSERT_EQ(m_events.size(), 1U);
    EXPECT_EQ(m_events[0].type, ComponentChangeEvent::kNonFunctionalChange);

    // The mapping exists: nothing happens.
    const FlightConfiguration* stored = &m_rocket.getFlightConfiguration(a);
    m_rocket.setFlightConfiguration(a, FlightConfiguration{m_rocket, a});
    EXPECT_EQ(&m_rocket.getFlightConfiguration(a), stored);
    EXPECT_EQ(m_events.size(), 1U);

    // The error id is refused.
    m_rocket.setFlightConfiguration(FlightConfigurationId::errorId(),
                                    FlightConfiguration{m_rocket, FlightConfigurationId{}});
    EXPECT_EQ(m_events.size(), 1U);

    // Java's null configuration removes it.
    m_rocket.setFlightConfiguration(a, std::nullopt);
    EXPECT_FALSE(m_rocket.containsFlightConfigurationId(a));
    EXPECT_EQ(m_events.size(), 2U);
}

TEST_F(RocketTest, SetFlightConfigurationRefusesAMismatchedConfiguration)
{
    // Stored under another id, the configuration would be selected and found by the wrong key.
    const FlightConfigurationId key;
    EXPECT_THROW(m_rocket.setFlightConfiguration(
                     key, FlightConfiguration{m_rocket, FlightConfigurationId{}}),
                 BugError);
    EXPECT_FALSE(m_rocket.containsFlightConfigurationId(key));

    // A configuration of another rocket would refer to that rocket's components.
    Rocket other;
    EXPECT_THROW(m_rocket.setFlightConfiguration(key, FlightConfiguration{other, key}), BugError);
    EXPECT_FALSE(m_rocket.containsFlightConfigurationId(key));
    EXPECT_TRUE(m_events.empty());
}

TEST_F(RocketTest, ASelectionDroppedFromTheSetIsOrphaned)
{
    const FlightConfigurationId a;
    m_rocket.createFlightConfiguration(a);
    m_rocket.setSelectedConfiguration(a);
    m_events.clear();

    // Java keeps the dropped object selected; the default stands in for it here.
    m_rocket.setFlightConfiguration(a, std::nullopt);
    EXPECT_FALSE(m_rocket.containsFlightConfigurationId(a));
    EXPECT_EQ(&m_rocket.getSelectedConfiguration(), &m_rocket.getEmptyConfiguration());
    EXPECT_EQ(m_events.size(), 1U);

    // Made again, the configuration is not the selected one, and selecting its id does nothing,
    // since the dropped object has that id.
    FlightConfiguration& again = m_rocket.createFlightConfiguration(a);
    EXPECT_NE(&m_rocket.getSelectedConfiguration(), &again);
    m_events.clear();
    m_rocket.setSelectedConfiguration(a);
    EXPECT_TRUE(m_events.empty());
    EXPECT_NE(&m_rocket.getSelectedConfiguration(), &again);

    // Java's equals() compares ids: the dump and a copy see the new configuration as selected.
    const QtRocket::InMemoryPreferences prefs;
    EXPECT_NE(m_rocket.toDebugConfigs(prefs).find("=>" + a.toShortKey()), std::string::npos);
    const std::unique_ptr<Rocket> copy = m_rocket.copyRocketWithOriginalId();
    EXPECT_EQ(&copy->getSelectedConfiguration(), &copy->getFlightConfiguration(a));

    // Selecting the default moves away from the dropped object: an event.
    m_rocket.setSelectedConfiguration(FlightConfigurationId::defaultValueId());
    EXPECT_EQ(m_events.size(), 1U);
    EXPECT_EQ(&m_rocket.getSelectedConfiguration(), &m_rocket.getEmptyConfiguration());
    m_rocket.setSelectedConfiguration(a);
    EXPECT_EQ(m_events.size(), 2U);
    EXPECT_EQ(&m_rocket.getSelectedConfiguration(), &again);
}

TEST_F(RocketTest, RemovingAnOrphanedSelectionSelectsTheDefault)
{
    const FlightConfigurationId a;
    m_rocket.createFlightConfiguration(a);
    m_rocket.setSelectedConfiguration(a);
    m_rocket.setFlightConfiguration(a, std::nullopt);

    // Java compares the selected object's id with the removed one.
    m_rocket.removeFlightConfiguration(a);
    m_events.clear();
    m_rocket.setSelectedConfiguration(FlightConfigurationId::defaultValueId());
    EXPECT_TRUE(m_events.empty()) << "the default is selected already";
    FlightConfiguration& again = m_rocket.createFlightConfiguration(a);
    m_rocket.setSelectedConfiguration(a);
    EXPECT_EQ(&m_rocket.getSelectedConfiguration(), &again);
}

TEST_F(RocketTest, DroppingAnotherConfigurationKeepsTheSelection)
{
    const FlightConfigurationId a;
    const FlightConfigurationId b;
    FlightConfiguration&        selected = m_rocket.createFlightConfiguration(a);
    m_rocket.createFlightConfiguration(b);
    m_rocket.setSelectedConfiguration(a);
    m_rocket.setFlightConfiguration(b, std::nullopt);
    EXPECT_EQ(&m_rocket.getSelectedConfiguration(), &selected);

    // The default cannot be dropped, so it cannot be orphaned.
    m_rocket.setSelectedConfiguration(FlightConfigurationId::defaultValueId());
    m_rocket.setFlightConfiguration(FlightConfigurationId::defaultValueId(), std::nullopt);
    m_events.clear();
    m_rocket.setSelectedConfiguration(FlightConfigurationId::defaultValueId());
    EXPECT_TRUE(m_events.empty());
}

TEST_F(RocketTest, TheConfigurationSetDumpsTheNames)
{
    // FlightConfigurableParameterSet.toDebug() on the rocket's set: the class name and each
    // configuration's toString(), its name.
    const QtRocket::InMemoryPreferences prefs;
    const FlightConfigurationId         a;
    m_rocket.createFlightConfiguration(a).setName("Alpha");
    const FlightConfigurationId b;
    m_rocket.createFlightConfiguration(b);
    EXPECT_EQ(m_rocket.getFlightConfigurations().toDebug(prefs),
              "====== Dumping ConfigurationSet<FlightConfiguration> (2 configurations)\n"
              "    [" +
                  std::format("{:<12}", a.toShortKey()) + "]: Alpha\n    [" +
                  std::format("{:<12}", b.toShortKey()) + "]: [No motors]\n");
}

TEST_F(RocketTest, ToDebugConfigs)
{
    const QtRocket::InMemoryPreferences prefs;
    const FlightConfigurationId         a;
    m_rocket.createFlightConfiguration(a).setName("Alpha");
    m_rocket.setSelectedConfiguration(a);
    m_rocket.setName("Test");
    EXPECT_EQ(m_rocket.toDebugConfigs(prefs),
              "====== Dumping 1 Configurations from rocket: Test ======\n"
              "    [  DefaultKey]: [{motors}]\n"
              "    [" +
                  std::format("{:>12}", "=>" + a.toShortKey()) + "]: Alpha\n");
}

/// RocketTest.testCopyIndependence: the copy's configurations are new objects of the copy with
/// the same ids and names.
TEST(RocketCopy, CopyIndependence)
{
    const QtRocket::Test::TestEstesAlphaIII rkt1;
    const QtRocket::InMemoryPreferences     prefs;
    FlightConfiguration                     config1{*rkt1.rocket, FlightConfigurationId{}};
    config1.setName("Test config 1");
    const FlightConfigurationId id1       = config1.getId();
    const int                   instance1 = config1.getConfigurationInstanceId();
    rkt1.rocket->setFlightConfiguration(id1, std::move(config1));
    rkt1.rocket->setSelectedConfiguration(id1);
    FlightConfiguration         config2{*rkt1.rocket, FlightConfigurationId{}};
    const FlightConfigurationId id2       = config2.getId();
    const int                   instance2 = config2.getConfigurationInstanceId();
    rkt1.rocket->setFlightConfiguration(id2, std::move(config2));

    const std::unique_ptr<Rocket> rkt2 = rkt1.rocket->copyRocketWithOriginalId();

    const FlightConfiguration& config4 = rkt2->getSelectedConfiguration();
    EXPECT_EQ(id1.key(), config4.getId().key()) << "fcids should match";
    EXPECT_EQ(rkt1.rocket->getFlightConfiguration(id1).getName(prefs), config4.getName(prefs));
    EXPECT_EQ("Test config 1", config4.getName(prefs));
    EXPECT_NE(instance1, config4.getConfigurationInstanceId());
    EXPECT_EQ(&config4.getRocket(), rkt2.get());

    const FlightConfiguration& config5 = rkt2->getFlightConfiguration(id2);
    EXPECT_EQ(id2, config5.getId());
    EXPECT_NE(instance2, config5.getConfigurationInstanceId());
    EXPECT_EQ(&config5.getRocket(), rkt2.get());
}

/// RocketTest's tolerance (MathUtil.EPSILON).
constexpr double kEpsilon = QtRocket::MathUtil::kEpsilon;

/// The child @p index of @p parent as a @p Component (Java's cast, which asserts the class).
/// @throws std::bad_cast when the child is of another class.
template <class Component>
[[nodiscard]] Component& childAs(RocketComponent& parent, std::size_t index)
{
    return dynamic_cast<Component&>(parent.getChild(index));
}

/// The first absolute location of @p component.
[[nodiscard]] Coordinate location(const RocketComponent& component)
{
    return component.getComponentLocations().at(0);
}

/// RocketTest.testEstesAlphaIII: the location of every component and the x extent of the bounds.
/// Deferred until the fins and the launch lug are real classes (HOOK(fins-lugs)): the y and z
/// extents of the bounds, which need the real fin and lug shapes.
TEST(RocketEstesAlphaIII, ComponentLocations)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    Rocket&                                 rocket = *alpha.rocket;

    auto& stage = childAs<AxialStage>(rocket, 0);

    const auto& nose = childAs<NoseCone>(stage, 0);
    EXPECT_EQ(location(nose), (Coordinate{0, 0, 0})) << nose.getName();

    auto& body = childAs<BodyTube>(stage, 1);
    EXPECT_EQ(location(body), (Coordinate{0.07, 0, 0})) << body.getName();

    {
        // HOOK(fins-lugs): the fins are a double carrying OpenRocket's instance offsets.
        const RocketComponent& fins = body.getChild(0);
        EXPECT_EQ(fins.kind(), ComponentKind::TRAPEZOID_FIN_SET);
        EXPECT_EQ(fins.getInstanceCount(), 3) << fins.getName() << " have incorrect count: ";
        EXPECT_EQ(location(fins), (Coordinate{0.22, 0.012, 0})) << "fin #1";

        // HOOK(fins-lugs): the lug is a double carrying OpenRocket's instance offset (its radial
        // offset, y = -0.015), which LaunchLug computes.
        const RocketComponent& lugs = body.getChild(1);
        EXPECT_EQ(lugs.kind(), ComponentKind::LAUNCH_LUG);
        EXPECT_EQ(lugs.getInstanceCount(), 1) << lugs.getName() << " have incorrect count: ";
        EXPECT_EQ(location(lugs), (Coordinate{0.181, -0.015, 0})) << lugs.getName();

        auto& mmt = childAs<InnerTube>(body, 2);
        EXPECT_EQ(location(mmt), (Coordinate{0.203, 0, 0})) << mmt.getName();
        const auto& block = childAs<EngineBlock>(mmt, 0);
        EXPECT_EQ(location(block), (Coordinate{0.203, 0, 0})) << block.getName();
    }

    const auto& chute = childAs<Parachute>(body, 3);
    EXPECT_EQ(location(chute), (Coordinate{0.098, 0, 0})) << chute.getName();

    const auto& ring = childAs<CenteringRing>(body, 4);
    EXPECT_EQ(ring.getInstanceCount(), 2) << ring.getName() << " not instanced correctly: ";

    const QtRocket::BoundingBox bounds = rocket.getBoundingBox();
    EXPECT_NEAR(bounds.min().x, 0.0, kEpsilon);
    EXPECT_NEAR(bounds.max().x, 0.27, kEpsilon);
    // HOOK(fins-lugs): min y -0.032385640, min z -0.054493575, max y 0.062000000 and max z
    // 0.052893575 need the real fins and lug.
}

/// RocketTest.testEstesAlphaIII, the centering rings: two instances, also after a round trip
/// through one (a single instance follows a different code path).
TEST(RocketEstesAlphaIII, CenteringRingLocations)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    auto& ring = childAs<CenteringRing>(childAs<BodyTube>(alpha.rocket->getChild(0), 1), 4);
    EXPECT_EQ(ring.getInstanceCount(), 2) << ring.getName() << " not instanced correctly: ";

    // singleton instances follow different code paths
    ring.setInstanceCount(1);
    Coordinate expLoc{0.21, 0, 0};
    Coordinate actLoc = location(ring);
    EXPECT_NEAR(expLoc.x, actLoc.x, kEpsilon) << " position x fail: ";
    EXPECT_NEAR(expLoc.y, actLoc.y, kEpsilon) << " position y fail: ";
    EXPECT_NEAR(expLoc.z, actLoc.z, kEpsilon) << " position z fail: ";
    EXPECT_EQ(actLoc, expLoc) << ring.getName() << " not positioned correctly: ";

    ring.setInstanceCount(2);
    const std::vector<Coordinate> actLocs = ring.getComponentLocations();
    {  // first instance
        expLoc = Coordinate{0.21, 0, 0};
        actLoc = actLocs.at(0);
        EXPECT_EQ(actLoc, expLoc) << ring.getName() << " not positioned correctly: ";
    }
    {  // second instance
        EXPECT_EQ(ring.getInstanceCount(), 2) << ring.getName() << " not instanced correctly: ";
        expLoc = Coordinate{0.245, 0, 0};
        actLoc = actLocs.at(1);
        EXPECT_EQ(actLoc, expLoc) << ring.getName() << " not positioned correctly: ";
    }
}

/// One case of RocketTest.testChangeAxialMethod: position the fins with the begin method and
/// offset, switch to the end method, and expect the end offset and position.
struct AxialPositionTestCase
{
    AxialMethod beginMethod;
    double      beginOffset;
    AxialMethod endMethod;
    double      endOffset;
    double      endPosition;
};

/// Whether @p fins follow @p cur (see AxialPositionTestCase).
::testing::AssertionResult repositions(TestComponent& fins, const AxialPositionTestCase& cur)
{
    // test repositioning
    fins.setAxialOffset(cur.beginMethod, cur.beginOffset);
    if (fins.getAxialMethod() != cur.beginMethod)
    {
        return ::testing::AssertionFailure() << "incorrect start axial-position-method";
    }
    if (std::abs(cur.beginOffset - fins.getAxialOffset()) > kEpsilon)
    {
        return ::testing::AssertionFailure() << "incorrect start axial-position-value";
    }
    fins.setAxialMethod(cur.endMethod);
    if (std::abs(cur.endOffset - fins.getAxialOffset()) > kEpsilon)
    {
        return ::testing::AssertionFailure()
               << "offset doesn't match: " << fins.getAxialOffset() << " != " << cur.endOffset;
    }
    if (std::abs(cur.endPosition - fins.getPosition().x) > kEpsilon)
    {
        return ::testing::AssertionFailure()
               << "position doesn't match: " << fins.getPosition().x << " != " << cur.endPosition;
    }
    return ::testing::AssertionSuccess();
}

/// RocketTest.testChangeAxialMethod, on the real body tube.
// HOOK(fins-lugs): the fins are a double; tier 6b runs this on the TrapezoidFinSet.
TEST(RocketEstesAlphaIII, ChangeAxialMethod)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    const BodyTube&                         body = *alpha.body;
    TestComponent&                          fins = *alpha.fins;

    {  // verify construction:
        EXPECT_NEAR(0.20, body.getLength(), kEpsilon) << "incorrect body length:";
        EXPECT_NEAR(0.05, fins.getLength(), kEpsilon) << "incorrect fin length:";
        // fin #1
        EXPECT_EQ(location(fins), (Coordinate{0.22, 0.012, 0}))
            << fins.getName() << " not positioned correctly: ";
    }

    using enum AxialMethod;
    const std::vector<AxialPositionTestCase> allTestCases{
        {.beginMethod = BOTTOM,
         .beginOffset = 0.0,
         .endMethod   = TOP,
         .endOffset   = 0.15,
         .endPosition = 0.15},
        {.beginMethod = TOP,
         .beginOffset = 0.0,
         .endMethod   = BOTTOM,
         .endOffset   = -0.15,
         .endPosition = 0.0},
        {.beginMethod = BOTTOM,
         .beginOffset = -0.03,
         .endMethod   = TOP,
         .endOffset   = 0.12,
         .endPosition = 0.12},
        {.beginMethod = BOTTOM,
         .beginOffset = 0.03,
         .endMethod   = TOP,
         .endOffset   = 0.18,
         .endPosition = 0.18},
        {.beginMethod = BOTTOM,
         .beginOffset = 0.03,
         .endMethod   = MIDDLE,
         .endOffset   = 0.105,
         .endPosition = 0.18},
        {.beginMethod = MIDDLE,
         .beginOffset = 0.0,
         .endMethod   = TOP,
         .endOffset   = 0.075,
         .endPosition = 0.075},
        {.beginMethod = MIDDLE,
         .beginOffset = 0.0,
         .endMethod   = BOTTOM,
         .endOffset   = -0.075,
         .endPosition = 0.075},
        {.beginMethod = MIDDLE,
         .beginOffset = 0.005,
         .endMethod   = TOP,
         .endOffset   = 0.08,
         .endPosition = 0.08},
    };

    for (std::size_t caseIndex = 0; caseIndex < allTestCases.size(); ++caseIndex)
    {
        EXPECT_TRUE(repositions(fins, allTestCases[caseIndex])) << " Test Case # " << caseIndex;
    }
}

/// RocketTest.testComponentLocationCacheInvalidatesOnMove, on the real body tube.
// HOOK(fins-lugs): the fins are a double; tier 6b runs this on the TrapezoidFinSet.
TEST(RocketEstesAlphaIII, ComponentLocationCacheInvalidatesOnMove)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    const BodyTube&                         body = *alpha.body;
    TestComponent&                          fins = *alpha.fins;

    // Warm the caches before moving the fin set so the assertions cover invalidation as well.
    const Coordinate initialAbsolute = location(fins);
    const Coordinate initialRelative = fins.toRelative(Coordinate::kNul, body).at(0);

    fins.setAxialMethod(AxialMethod::TOP);
    fins.setAxialOffset(0.16);

    const Coordinate movedAbsolute = location(fins);
    const Coordinate movedRelative = fins.toRelative(Coordinate::kNul, body).at(0);

    EXPECT_NE(initialAbsolute, movedAbsolute)
        << "Absolute component location cache was not invalidated";
    EXPECT_EQ((Coordinate{0.23, 0.012, 0}), movedAbsolute)
        << "Absolute component location is incorrect";

    EXPECT_NE(initialRelative, movedRelative)
        << "Relative component location cache was not invalidated";
    EXPECT_EQ((Coordinate{0.16, 0.012, 0}), movedRelative)
        << "Relative component location is incorrect";
}

/// RocketTest.testRemoveReadjustLocation.
TEST(RocketEstesAlphaIII, RemoveReadjustLocation)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    Rocket&                                 rocket = *alpha.rocket;

    {
        const auto& bodyPrior = childAs<BodyTube>(rocket.getChild(0), 1);
        EXPECT_NEAR(location(bodyPrior).x, 0.07, kEpsilon);
    }

    // remove the nose cone, causing the bodytube to reposition:
    static_cast<void>(rocket.getChild(0).removeChild(0));

    {
        const auto& tubePost = childAs<BodyTube>(rocket.getChild(0), 0);
        EXPECT_NEAR(location(tubePost).x, 0.0, kEpsilon);
    }
}

/// UUIDSearchTest.testUUIDSearch: a component is found by its id until it is removed.
TEST(RocketEstesAlphaIII, UuidSearch)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    Rocket&                                 rocket = *alpha.rocket;

    auto&       stage    = childAs<AxialStage>(rocket, 0);
    const auto& noseCone = childAs<NoseCone>(stage, 0);

    // If I search for the NoseCone using its UUID I should get it back
    const QtRocket::Uuid noseConeId = noseCone.getId();
    EXPECT_EQ(&noseCone, rocket.findComponent(noseConeId)) << "UUID search didn't find NoseCone";

    // Once removed (without stage tracking, as Java's removeChild(noseCone, false)), it is not
    // found any more (Java: RocketComponent.REMOVED).
    const std::unique_ptr<RocketComponent> removed =
        stage.removeChild(&noseCone, RocketComponent::StageTracking::SKIP);
    ASSERT_NE(removed, nullptr) << "failed to remove NoseCone";
    EXPECT_EQ(rocket.findComponent(noseConeId), nullptr)
        << "Search for NoseCone failed to find REMOVED";

    // The nil id is not found either.
    EXPECT_EQ(rocket.findComponent(QtRocket::Uuid{0U, 0U}), nullptr) << "Failed to find REMOVED";
}

/// RocketTest.testBeta: the locations of the booster's components and the x extent of the bounds
/// (HOOK(fins-lugs): the y and z extents need the real fins and lugs).
TEST(RocketBeta, ComponentLocations)
{
    const QtRocket::Test::TestBeta beta;
    Rocket&                        rocket = *beta.rocket;

    auto& boosterStage = childAs<AxialStage>(rocket, 1);
    {
        auto& body = childAs<BodyTube>(boosterStage, 0);
        EXPECT_EQ(location(body), (Coordinate{0.27, 0, 0})) << body.getName();

        {
            const auto& coupler = childAs<TubeCoupler>(body, 0);
            EXPECT_EQ(location(coupler), (Coordinate{0.255, 0, 0})) << coupler.getName();

            // HOOK(fins-lugs): the fins are a double carrying OpenRocket's instance offsets.
            const RocketComponent& fins = body.getChild(1);
            EXPECT_EQ(fins.kind(), ComponentKind::TRAPEZOID_FIN_SET);
            EXPECT_EQ(fins.getInstanceCount(), 3) << fins.getName() << " have incorrect count: ";
            EXPECT_EQ(location(fins), (Coordinate{0.28, 0.012, 0})) << "fin #1";

            const auto& mmt = childAs<InnerTube>(body, 2);
            EXPECT_EQ(location(mmt), (Coordinate{0.285, 0, 0})) << mmt.getName();
        }
    }

    const QtRocket::BoundingBox bounds = rocket.getBoundingBox();
    EXPECT_NEAR(bounds.min().x, 0.0, kEpsilon);
    EXPECT_NEAR(bounds.max().x, 0.335, kEpsilon);
    // HOOK(fins-lugs): min y -0.032385640, min z -0.054493575, max y 0.062000000 and max z
    // 0.052893575 need the real fins and lugs.
}

/// Whether @p c sits at @p offset in its parent and at @p location in the rocket (x only, as
/// RocketTest.testFalcon9HComponentLocations checks them).
::testing::AssertionResult isAt(const RocketComponent& c, double offset, double location)
{
    if (std::abs(offset - c.getPosition().x) > kEpsilon)
    {
        return ::testing::AssertionFailure()
               << c.getName() << " offset is incorrect: " << c.getPosition().x;
    }
    const double x = c.getComponentLocations().at(0).x;
    if (std::abs(location - x) > kEpsilon)
    {
        return ::testing::AssertionFailure() << c.getName() << " location is incorrect: " << x;
    }
    return ::testing::AssertionSuccess();
}

/// RocketTest.testFalcon9HComponentLocations, the payload stage.
TEST(RocketFalcon9Heavy, PayloadStageLocations)
{
    const QtRocket::Test::TestFalcon9Heavy f9h;
    auto&                                  payloadStage = childAs<AxialStage>(*f9h.rocket, 0);

    EXPECT_TRUE(isAt(childAs<NoseCone>(payloadStage, 0), 0.0, 0.0));
    EXPECT_TRUE(isAt(childAs<BodyTube>(payloadStage, 1), 0.118, 0.118));
    EXPECT_TRUE(isAt(childAs<Transition>(payloadStage, 2), 0.250, 0.250));
    auto& upperBody = childAs<BodyTube>(payloadStage, 3);
    EXPECT_TRUE(isAt(upperBody, 0.264, 0.264));
    EXPECT_TRUE(isAt(childAs<Parachute>(upperBody, 0), 0.0775, 0.3415));
    EXPECT_TRUE(isAt(childAs<ShockCord>(upperBody, 1), 0.155, 0.419));
    EXPECT_TRUE(isAt(childAs<BodyTube>(payloadStage, 4), 0.444, 0.444));
}

/// RocketTest.testFalcon9HComponentLocations, the booster set itself.
TEST(RocketFalcon9Heavy, BoosterSetLocations)
{
    const QtRocket::Test::TestFalcon9Heavy f9h;
    auto&                                  coreBody = childAs<BodyTube>(f9h.rocket->getChild(1), 0);
    const auto&                            boosters = childAs<QtRocket::ParallelStage>(coreBody, 0);

    EXPECT_EQ(QtRocket::RadiusMethod::SURFACE, boosters.getRadiusMethod());
    EXPECT_EQ(QtRocket::AngleMethod::RELATIVE, boosters.getAngleMethod());

    const Coordinate boosterPosition = boosters.getPosition();
    EXPECT_NEAR(-0.08, boosterPosition.x, kEpsilon) << boosters.getName();
    EXPECT_NEAR(0.0, boosterPosition.y, kEpsilon) << boosters.getName();
    EXPECT_NEAR(0.0, boosterPosition.z, kEpsilon) << boosters.getName();

    const std::vector<Coordinate> boosterInstanceOffsets = boosters.getInstanceOffsets();
    EXPECT_NEAR(0.0, boosterInstanceOffsets.at(0).x, kEpsilon) << boosters.getName();
    EXPECT_NEAR(0.077, boosterInstanceOffsets.at(0).y, kEpsilon) << boosters.getName();
    EXPECT_NEAR(-0.077, boosterInstanceOffsets.at(1).y, kEpsilon) << boosters.getName();
    EXPECT_NEAR(0.0, boosterInstanceOffsets.at(0).z, kEpsilon) << boosters.getName();

    const std::vector<Coordinate> boosterLocations = boosters.getComponentLocations();
    EXPECT_NEAR(0.484, boosterLocations.at(0).x, kEpsilon) << boosters.getName();
    EXPECT_NEAR(0.077, boosterLocations.at(0).y, kEpsilon) << boosters.getName();
    EXPECT_NEAR(-0.077, boosterLocations.at(1).y, kEpsilon) << boosters.getName();
    EXPECT_NEAR(0.0, boosterLocations.at(0).z, kEpsilon) << boosters.getName();
}

/// RocketTest.testFalcon9HComponentLocations, the core stage and the boosters, and the x extent
/// of the bounds (HOOK(fins-lugs): the y extent, -0.2155 to 0.2155, and the z extent,
/// -0.12069451 to 0.12069451, need the real fins).
TEST(RocketFalcon9Heavy, CoreAndBoosterLocations)
{
    const QtRocket::Test::TestFalcon9Heavy f9h;
    Rocket&                                rocket = *f9h.rocket;

    auto& coreBody = childAs<BodyTube>(rocket.getChild(1), 0);
    EXPECT_TRUE(isAt(coreBody, 0.0, 0.564));

    auto& boosters = childAs<QtRocket::ParallelStage>(coreBody, 0);

    EXPECT_TRUE(isAt(childAs<NoseCone>(boosters, 0), 0.0, 0.484));
    auto& boosterBody = childAs<BodyTube>(boosters, 1);
    EXPECT_TRUE(isAt(boosterBody, 0.08, 0.564));
    EXPECT_TRUE(isAt(childAs<InnerTube>(boosterBody, 0), 0.65, 1.214));
    // HOOK(fins-lugs): the fins are a double (positioned BOTTOM, as the Java fins).
    const RocketComponent& boosterFins = boosterBody.getChild(1);
    EXPECT_EQ(boosterFins.kind(), ComponentKind::TRAPEZOID_FIN_SET);
    EXPECT_TRUE(isAt(boosterFins, 0.480, 1.044));

    const std::string           tree   = rocket.toDebugTree();
    const QtRocket::BoundingBox bounds = rocket.getBoundingBox();
    EXPECT_NEAR(0.0, bounds.min().x, kEpsilon) << tree;
    EXPECT_NEAR(1.364, bounds.max().x, kEpsilon) << tree;
}

TEST(RocketFalcon9Heavy, DebugTreeShowsTheMountedMotors)
{
    const QtRocket::Test::TestFalcon9Heavy f9h;
    const std::string                      tree = f9h.rocket->toDebugTree();
    EXPECT_NE(tree.find("  Mounted: M1350"), std::string::npos) << tree;
    EXPECT_NE(tree.find("  Mounted: G77"), std::string::npos) << tree;
    EXPECT_NE(tree.find("Thrust: "), std::string::npos) << tree;

    // Another configuration has no motors.
    f9h.rocket->setSelectedConfiguration(FlightConfigurationId::defaultValueId());
    const std::string empty = f9h.rocket->toDebugTree();
    EXPECT_NE(empty.find("[X] This Instance doesn't have any motors for the active configuration."),
              std::string::npos)
        << empty;
}

// ---- Copies and undo ----

TEST_F(RocketTest, CopyWithOriginalIdRebuildsTheStageMap)
{
    m_rocket.setDesigner("Jane");
    m_rocket.setReferenceType(ReferenceType::CUSTOM);
    m_rocket.setPerfectFinish(true);
    AxialStage& second = m_rocket.addChild(std::make_unique<AxialStage>());
    second.addChild(std::make_unique<BodyTube>(0.1, 0.02));

    const std::unique_ptr<Rocket> copy = m_rocket.copyRocketWithOriginalId();
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->getId(), m_rocket.getId());
    EXPECT_EQ(copy->getStageCount(), 2U);
    ASSERT_NE(copy->getStage(0), nullptr);
    EXPECT_NE(copy->getStage(0), m_stage);
    EXPECT_EQ(copy->getStage(0)->getId(), m_stage->getId());
    EXPECT_EQ(copy->getStage(0)->getParent(), copy.get());
    EXPECT_EQ(copy->getStage(1)->getId(), second.getId());
    EXPECT_EQ(copy->getModId(), m_rocket.getModId());
    EXPECT_EQ(copy->getMassModId(), m_rocket.getMassModId());
    EXPECT_EQ(copy->getDesigner(), "Jane");
    EXPECT_EQ(copy->getReferenceType(), ReferenceType::CUSTOM);
    EXPECT_TRUE(copy->isPerfectFinish());
    EXPECT_TRUE(copy->isEventsEnabled());
    EXPECT_EQ(copy->getListenerCount(), 0U) << "listeners are not copied";
    EXPECT_NO_THROW(copy->checkComponentStructure());

    // The copy is independent.
    const auto before = m_events.size();
    copy->getStage(0)->setName("Copy");
    EXPECT_EQ(m_events.size(), before);
    EXPECT_EQ(m_stage->getName(), "Stage");

    // Through the base interface too.
    const std::unique_ptr<RocketComponent> base =
        static_cast<const RocketComponent&>(m_rocket).copyWithOriginalId();
    EXPECT_EQ(dynamic_cast<Rocket&>(*base).getStageCount(), 2U);

    // New ids for every component, the stage map still pointing at the copied stages.
    const std::unique_ptr<RocketComponent> fresh       = m_rocket.copyWithNewIds();
    auto&                                  freshRocket = dynamic_cast<Rocket&>(*fresh);
    EXPECT_NE(freshRocket.getId(), m_rocket.getId());
    ASSERT_NE(freshRocket.getStage(0), nullptr);
    EXPECT_NE(freshRocket.getStage(0)->getId(), m_stage->getId());
    EXPECT_EQ(freshRocket.getStage(0)->getParent(), &freshRocket);
}

TEST_F(RocketTest, CopyRebuildsTheConfigurationsForTheCopy)
{
    const QtRocket::InMemoryPreferences prefs;
    const FlightConfigurationId         a;
    FlightConfiguration&                config = m_rocket.createFlightConfiguration(a);
    config.setName("Alpha");
    AxialStage& second = m_rocket.addChild(std::make_unique<AxialStage>());
    second.addChild(std::make_unique<BodyTube>(0.1, 0.01));
    config.setStageActive(1, false);
    m_rocket.setSelectedConfiguration(a);
    m_rocket.getEmptyConfiguration().setStageActive(0, false);

    const std::unique_ptr<Rocket> copy = m_rocket.copyRocketWithOriginalId();
    EXPECT_EQ(copy->getIds(), m_rocket.getIds());
    const FlightConfiguration& copied = copy->getFlightConfiguration(a);
    EXPECT_NE(&copied, &config);
    EXPECT_EQ(&copied.getRocket(), copy.get());
    EXPECT_EQ(copied.getNameRaw(prefs), "Alpha");
    EXPECT_TRUE(copied.isStageActive(0));
    EXPECT_FALSE(copied.isStageActive(1));
    EXPECT_EQ(&copy->getSelectedConfiguration(), &copied) << "the selected id is kept";
    // The default is a new one (Java: new FlightConfiguration(copy)): its flags are not copied.
    EXPECT_TRUE(copy->getEmptyConfiguration().isStageActive(0));
    EXPECT_EQ(&copy->getEmptyConfiguration().getRocket(), copy.get());
    // The instances are the copy's components.
    EXPECT_TRUE(copied.getActiveInstances().containsKey(*copy->getStage(0)));
    EXPECT_FALSE(copied.getActiveInstances().containsKey(*m_stage));
    EXPECT_DOUBLE_EQ(copy->getLength(), 0.4);
}

TEST_F(RocketTest, LoadFromRebuildsTheConfigurations)
{
    const QtRocket::InMemoryPreferences prefs;
    const FlightConfigurationId         a;
    m_rocket.createFlightConfiguration(a).setName("Alpha");
    m_rocket.getFlightConfiguration(a).setStageActive(0, false);
    m_rocket.setSelectedConfiguration(a);
    const std::unique_ptr<Rocket> snapshot = m_rocket.copyRocketWithOriginalId();

    // Changes after the snapshot.
    m_rocket.removeFlightConfiguration(a);
    const FlightConfigurationId b;
    m_rocket.createFlightConfiguration(b);
    m_rocket.getEmptyConfiguration().setStageActive(0, false);

    m_rocket.loadFrom(*snapshot);
    EXPECT_EQ(m_rocket.getIds(), std::vector<FlightConfigurationId>{a});
    const FlightConfiguration& restored = m_rocket.getFlightConfiguration(a);
    EXPECT_EQ(&restored.getRocket(), &m_rocket);
    EXPECT_EQ(restored.getNameRaw(prefs), "Alpha");
    EXPECT_FALSE(restored.isStageActive(0));
    EXPECT_EQ(&m_rocket.getSelectedConfiguration(), &restored);
    // As in Java, the default keeps its own activeness.
    EXPECT_FALSE(m_rocket.getEmptyConfiguration().isStageActive(0));
    // The configurations refer to the loaded components only (the stage is inactive in both).
    EXPECT_EQ(restored.getActiveInstances().keys(), std::vector<RocketComponent*>{&m_rocket});
    EXPECT_EQ(m_rocket.getEmptyConfiguration().getActiveInstances().keys(),
              std::vector<RocketComponent*>{&m_rocket});
}

/// A RocketTest whose rocket is changed after a snapshot: the name and mass of the body (made
/// solid), the reference type, the designer and an extra stage.
class LoadFromTest : public RocketTest
{
protected:
    LoadFromTest()
    {
        m_rocket.setReferenceType(ReferenceType::NOSECONE);
        m_snapshot    = m_rocket.copyRocketWithOriginalId();
        m_snapshotMod = m_rocket.getModId();

        m_body->setName("Changed");
        m_body->setFilled(true);
        m_rocket.setReferenceType(ReferenceType::MAXIMUM);
        m_rocket.setDesigner("After");
        AxialStage& extra = m_rocket.addChild(std::make_unique<AxialStage>());
        extra.addChild(std::make_unique<BodyTube>(0.1, 0.02));
        m_events.clear();
    }

    std::unique_ptr<Rocket> m_snapshot;
    ModId                   m_snapshotMod{ModId::zero()};
};

TEST_F(LoadFromTest, FiresOneUndoEvent)
{
    // During the undo event the replaced components still exist.
    const BodyTube* oldBody = m_body;
    std::string     nameDuringEvent;
    const auto      connection =
        ComponentChangeSignal::ScopedConnection{m_rocket.addComponentChangeListener(
            [oldBody, &nameDuringEvent](const ComponentChangeEvent&) {
                nameDuringEvent = oldBody->getName();
            })};

    m_rocket.loadFrom(*m_snapshot);

    ASSERT_EQ(m_events.size(), 1U);
    EXPECT_EQ(m_events[0].type,
              ComponentChangeEvent::kUndoChange | ComponentChangeEvent::kNonFunctionalChange |
                  ComponentChangeEvent::kMassChange | ComponentChangeEvent::kAerodynamicChange |
                  ComponentChangeEvent::kTreeChange);
    EXPECT_EQ(m_events[0].source, &m_rocket);
    EXPECT_EQ(nameDuringEvent, "Changed");
}

TEST_F(LoadFromTest, RestoresTheSnapshotState)
{
    m_rocket.loadFrom(*m_snapshot);

    EXPECT_EQ(m_rocket.getModId(), m_snapshotMod) << "undo restores the ids";
    EXPECT_EQ(m_rocket.getReferenceType(), ReferenceType::NOSECONE);
    EXPECT_EQ(m_rocket.getDesigner(), "After") << "Java's loadFrom() keeps the designer";
    ASSERT_EQ(m_rocket.getChildCount(), 1U);
    EXPECT_EQ(m_rocket.getStageCount(), 1U);
    EXPECT_NO_THROW(m_rocket.checkComponentStructure());
}

TEST_F(LoadFromTest, RestoresTheSnapshotTree)
{
    m_rocket.loadFrom(*m_snapshot);

    AxialStage* stage = m_rocket.getStage(0);
    ASSERT_NE(stage, nullptr);
    EXPECT_EQ(stage->getParent(), &m_rocket);
    EXPECT_EQ(stage->getId(), m_snapshot->getStage(0)->getId());
    ASSERT_EQ(stage->getChildCount(), 2U);
    EXPECT_EQ(stage->getChild(1).getName(), "Body Tube");

    // The snapshot is still usable (Java's is invalidated).
    EXPECT_EQ(m_snapshot->getStage(0)->getChildCount(), 2U);
    EXPECT_EQ(m_snapshot->getListenerCount(), 0U);
}

TEST_F(LoadFromTest, WhileFrozenTheUndoEventWaitsForTheThaw)
{
    m_rocket.freeze();
    m_rocket.loadFrom(*m_snapshot);  // the replaced components are destroyed here
    EXPECT_TRUE(m_events.empty());
    EXPECT_EQ(m_rocket.getChildCount(), 1U);

    m_rocket.thaw();
    ASSERT_EQ(m_events.size(), 1U);
    EXPECT_EQ(m_events[0].type,
              ComponentChangeEvent::kUndoChange | ComponentChangeEvent::kNonFunctionalChange |
                  ComponentChangeEvent::kMassChange | ComponentChangeEvent::kAerodynamicChange |
                  ComponentChangeEvent::kTreeChange);
    EXPECT_EQ(m_events[0].source, &m_rocket);
    EXPECT_EQ(m_rocket.getModId(), m_snapshotMod) << "an undo event draws no new ids";
    EXPECT_EQ(m_rocket.getStageCount(), 1U);
    EXPECT_DOUBLE_EQ(m_rocket.getLength(), 0.4);
}

TEST_F(RocketTest, LoadFromWithUnchangedMassIsNoMassChange)
{
    const std::unique_ptr<Rocket> snapshot = m_rocket.copyRocketWithOriginalId();
    m_body->setComment("only a comment");
    m_events.clear();
    m_rocket.loadFrom(*snapshot);
    ASSERT_EQ(m_events.size(), 1U);
    EXPECT_EQ(m_events[0].type, ComponentChangeEvent::kUndoChange |
                                    ComponentChangeEvent::kNonFunctionalChange |
                                    ComponentChangeEvent::kTreeChange);
}

// ========================================================================= automatic radii

/// A stand-in for OpenRocket's LaunchLug in the automatic radius tests: a launch lug's
/// componentChanged() reads the radius of the symmetric component it sits on at both of its ends
/// (to compute its radial offset), and reading an automatic body tube radius refreshes the tube's
/// reference component. RocketTest.testAutoSizeNextComponent depends on that side effect, so
/// LuggedBeta carries the two lugs of TestRockets.makeBeta() as this stand-in until LaunchLug is
/// ported.
/// HOOK(launch-lug): replace with the real LaunchLug once it is ported.
// HOOK(fins-lugs): tier 6b deletes this with LuggedBeta (see there)
class LaunchLugStandIn : public TestComponent
{
public:
    LaunchLugStandIn() : TestComponent(ComponentKind::LAUNCH_LUG, AxialMethod::TOP, 0.050) { }

protected:
    void componentChanged(const ComponentChangeEvent& event) override
    {
        TestComponent::componentChanged(event);
        const RocketComponent* body = getParent();
        while (body != nullptr &&
               dynamic_cast<const QtRocket::SymmetricComponent*>(body) == nullptr)
        {
            body = body->getParent();
        }
        if (body == nullptr)
        {
            return;
        }
        const auto&  symmetric = dynamic_cast<const QtRocket::SymmetricComponent&>(*body);
        const double x1        = toRelative(Coordinate::kNul, *body).at(0).x;
        const double x2        = toRelative(Coordinate{getLength(), 0, 0}, *body).at(0).x;
        static_cast<void>(symmetric.getRadius(QtRocket::MathUtil::clamp(x1, 0, body->getLength())));
        static_cast<void>(symmetric.getRadius(QtRocket::MathUtil::clamp(x2, 0, body->getLength())));
    }
};

/// TestRockets.makeBeta() for the automatic radius tests: TestBeta (TestRockets.h, built from the
/// real body and internal components) with its two launch lug doubles replaced, at the same
/// child index, offset and name, by lugs that read the body radius as Java's LaunchLug does.
/// TestBeta's own lug doubles are inert, and without that side effect the last step of
/// RocketTest.testAutoSizeNextComponent would give 0.025 m instead of OpenRocket's 0.012 m.
// HOOK(launch-lug), HOOK(fins-lugs): once TestBeta's lugs are real LaunchLugs, RocketAutoSize
// runs on TestBeta itself; this struct and LaunchLugStandIn are then deleted.
struct LuggedBeta : QtRocket::Test::TestBeta
{
    LuggedBeta()
    {
        lug        = &replaceLug(*body, *lug, 0.111);
        boosterLug = &replaceLug(*boosterBody, *boosterLug, 0.0);
    }

private:
    /// Replaces the lug double @p old of @p parent by a LaunchLugStandIn positioned TOP at
    /// @p offset, with the same name and child index.
    static TestComponent& replaceLug(BodyTube& parent, const TestComponent& old, double offset)
    {
        const std::optional<std::size_t> index = parent.getChildPosition(&old);
        if (!index)
        {
            QtRocket::bug("the launch lug double is not a child of its body");
        }
        const std::string name = old.getName();
        static_cast<void>(parent.removeChild(&old));
        auto lug = std::make_unique<LaunchLugStandIn>();
        lug->setName(name);
        lug->setAxialOffset(AxialMethod::TOP, offset);
        return parent.addChild(std::move(lug), *index);
    }
};

/// RocketTest's tolerance (MathUtil.EPSILON).
constexpr double kAutoSizeEpsilon = kEpsilon;

/// RocketTest.testAutoSizePreviousComponent.
TEST(RocketAutoSize, PreviousComponent)
{
    const LuggedBeta beta;
    const double     expRadius = 0.012;

    {  // test auto-radius within a stage: nose -> body tube
        EXPECT_NEAR(expRadius, beta.nose->getAftRadius(), kAutoSizeEpsilon) << " radius match: ";
        EXPECT_NEAR(expRadius, beta.body->getOuterRadius(), kAutoSizeEpsilon) << " radius match: ";

        beta.body->setOuterRadiusAutomatic(true);
        EXPECT_NEAR(expRadius, beta.body->getOuterRadius(), kAutoSizeEpsilon) << " radius match: ";
    }
    {  // test auto-radius within a stage: tail cone -> body tube
        EXPECT_NEAR(expRadius, beta.boosterBody->getOuterRadius(), kAutoSizeEpsilon)
            << " radius match: ";
        EXPECT_NEAR(expRadius, beta.boosterTail->getForeRadius(), kAutoSizeEpsilon)
            << " radius match: ";

        beta.boosterTail->setForeRadiusAutomatic(true);
        EXPECT_NEAR(expRadius, beta.boosterTail->getForeRadius(), kAutoSizeEpsilon)
            << " trailing transition match: ";
    }
    {  // test auto-radius across stages: sustainer body -> booster body
        EXPECT_NEAR(expRadius, beta.body->getOuterRadius(), kAutoSizeEpsilon) << " radius match: ";
        EXPECT_NEAR(expRadius, beta.boosterBody->getOuterRadius(), kAutoSizeEpsilon)
            << " radius match: ";

        beta.boosterBody->setOuterRadiusAutomatic(true);
        EXPECT_NEAR(expRadius, beta.boosterBody->getOuterRadius(), kAutoSizeEpsilon)
            << " radius match: ";
    }
}

/// RocketTest.testAutoSizeNextComponent.
TEST(RocketAutoSize, NextComponent)
{
    const LuggedBeta beta;
    const double     expRadius = 0.012;

    {  // test auto-radius within a stage: nose <- body tube
        EXPECT_NEAR(expRadius, beta.nose->getAftRadius(), kAutoSizeEpsilon) << " radius match: ";
        EXPECT_NEAR(expRadius, beta.body->getOuterRadius(), kAutoSizeEpsilon) << " radius match: ";

        beta.nose->setAftRadiusAutomatic(true);
        EXPECT_NEAR(expRadius, beta.nose->getAftRadius(), kAutoSizeEpsilon) << " radius match: ";
    }
    {  // test auto-radius within a stage: body tube <- trailing transition
        EXPECT_NEAR(expRadius, beta.boosterBody->getOuterRadius(), kAutoSizeEpsilon)
            << " radius match: ";
        EXPECT_NEAR(expRadius, beta.boosterTail->getForeRadius(), kAutoSizeEpsilon)
            << " radius match: ";

        beta.boosterBody->setOuterRadiusAutomatic(true);
        EXPECT_NEAR(expRadius, beta.boosterBody->getOuterRadius(), kAutoSizeEpsilon)
            << " trailing transition match: ";
    }
    {  // test auto-radius across stages: sustainer body <- booster body
        EXPECT_NEAR(expRadius, beta.body->getOuterRadius(), kAutoSizeEpsilon) << " radius match: ";
        EXPECT_NEAR(expRadius, beta.boosterBody->getOuterRadius(), kAutoSizeEpsilon)
            << " radius match: ";

        beta.body->setOuterRadiusAutomatic(true);
        EXPECT_NEAR(expRadius, beta.body->getOuterRadius(), kAutoSizeEpsilon) << " radius match: ";
    }
}

TEST(RocketAutoSize, AChangedNeighbourResizesTheAutomaticComponents)
{
    // Not in OpenRocket's tests: the automatic radii follow a change of the radius they come
    // from, across the stage boundary too.
    const LuggedBeta beta;
    beta.body->setOuterRadiusAutomatic(true);
    beta.boosterBody->setOuterRadiusAutomatic(true);
    beta.boosterTail->setForeRadiusAutomatic(true);

    beta.nose->setAftRadius(0.02);
    EXPECT_EQ(beta.body->getOuterRadius(), 0.02);
    EXPECT_EQ(beta.boosterBody->getOuterRadius(), 0.02);
    EXPECT_EQ(beta.boosterTail->getForeRadius(), 0.02);
    EXPECT_DOUBLE_EQ(beta.rocket->getBoundingRadius(), 0.02);
}

}  // namespace
