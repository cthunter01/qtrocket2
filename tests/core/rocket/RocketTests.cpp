#include "QtRocket/rocket/Rocket.h"

#include <algorithm>
#include <cmath>
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
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/DesignType.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/ReferenceType.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"
#include "rocket/TestBodyComponent.h"
#include "rocket/TestComponent.h"
#include "rocket/TestMotorMount.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BugError;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::DesignType;
using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::ModId;
using QtRocket::ReferenceType;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Test::TestBodyComponent;
using QtRocket::Test::TestComponent;
using QtRocket::Test::TestMotorMount;

/// A received event: its type and source.
struct Received
{
    int                    type;
    const RocketComponent* source;
};

/// A rocket with one stage holding a nose cone and a body tube, events enabled, and a listener
/// that records every event.
class RocketTest : public ::testing::Test
{
protected:
    RocketTest()
    {
        m_stage = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_nose  = &m_stage->addChild(TestComponent::make(0.1, ComponentKind::NOSE_CONE));
        m_body  = &m_stage->addChild(TestBodyComponent::make(0.3, 0.0));
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
    TestComponent*                          m_nose{nullptr};
    TestComponent*                          m_body{nullptr};
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
    second.addChild(TestBodyComponent::make(0.2, 0.03));
    m_body->setOuterRadius(0.02);
    m_nose->setOuterRadius(0.05);  // not a body tube: not counted
    EXPECT_DOUBLE_EQ(m_stage->getLength(), 0.4);
    EXPECT_DOUBLE_EQ(m_rocket.getLength(), 0.6) << "the second stage counts";
    EXPECT_DOUBLE_EQ(m_rocket.getBoundingRadius(), 0.03);
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
    stage.addChild(TestComponent::make(0.1));
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
    m_body->setMass(0.4);
    ASSERT_EQ(m_events.size(), 1U);
    EXPECT_EQ(m_events[0].type, ComponentChangeEvent::kMassChange);
    EXPECT_EQ(m_events[0].source, m_body);
}

TEST_F(RocketTest, EveryComponentSeesTheChange)
{
    const int noseBefore = m_nose->componentChangedCount();
    const int bodyBefore = m_body->componentChangedCount();
    m_body->setMass(0.4);
    EXPECT_EQ(m_nose->componentChangedCount(), noseBefore + 1);
    EXPECT_EQ(m_body->componentChangedCount(), bodyBefore + 1);
    EXPECT_EQ(m_nose->lastChangeType(), std::optional{ComponentChangeEvent::kMassChange});
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
    const int   before = m_body->componentChangedCount();
    const ModId mod    = m_rocket.getModId();

    m_rocket.freeze();
    EXPECT_TRUE(m_rocket.isFrozen());
    m_nose->setMass(0.1);                                                 // MASS from the nose
    m_body->fireComponentChangeEvent(ComponentChangeEvent::kTreeChange);  // TREE from the body
    m_body->setComment("frozen");                                         // NONFUNCTIONAL
    EXPECT_TRUE(m_events.empty()) << "nothing is delivered while frozen";
    EXPECT_EQ(m_body->componentChangedCount(), before);
    EXPECT_GT(m_rocket.getModId(), mod) << "the ids move while frozen";
    const ModId frozenMod = m_rocket.getModId();

    m_rocket.thaw();
    EXPECT_FALSE(m_rocket.isFrozen());
    ASSERT_EQ(m_events.size(), 1U);
    EXPECT_EQ(m_events[0].type, ComponentChangeEvent::kMassChange |
                                    ComponentChangeEvent::kTreeChange |
                                    ComponentChangeEvent::kNonFunctionalChange);
    EXPECT_EQ(m_events[0].source, m_body) << "the last queued event's source";
    EXPECT_EQ(m_body->componentChangedCount(), before + 1);
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
    m_nose->setMass(0.2);  // the last queued event comes from the nose
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
    m_body->setBypassChangeEvent(true);
    m_body->setMass(1.0);
    EXPECT_TRUE(m_events.empty());
    m_body->setBypassChangeEvent(false);
    m_body->setMass(2.0);
    EXPECT_EQ(m_events.size(), 1U);

    TestComponent detached;
    detached.setMass(3.0);
    EXPECT_EQ(m_events.size(), 1U);
}

TEST_F(RocketTest, TreeChangesFireWithMassAndAeroOfTheSubtree)
{
    auto tube = TestComponent::make(0.2);
    tube->addChild(TestComponent::make(0.05)).setAerodynamic(false);
    tube->setMassive(false);
    m_stage->addChild(std::move(tube));
    ASSERT_EQ(m_events.size(), 1U);
    EXPECT_EQ(m_events[0].type, ComponentChangeEvent::kTreeChange |
                                    ComponentChangeEvent::kAerodynamicChange |
                                    ComponentChangeEvent::kMassChange);

    // A lone non-aerodynamic, non-massive component is a tree change only.
    m_events.clear();
    auto plain = TestComponent::make(0.1);
    plain->setAerodynamic(false);
    plain->setMassive(false);
    TestComponent& added = m_stage->addChild(std::move(plain));
    EXPECT_EQ(types(), std::vector<int>{ComponentChangeEvent::kTreeChange});

    m_events.clear();
    m_stage->moveChild(&added, 0);
    EXPECT_EQ(types(), std::vector<int>{ComponentChangeEvent::kTreeChange});
    m_events.clear();
    const std::unique_ptr<RocketComponent> removed = m_stage->removeChild(&added);
    EXPECT_EQ(types(), std::vector<int>{ComponentChangeEvent::kTreeChange});
    EXPECT_EQ(m_events[0].source, m_stage);
}

TEST_F(RocketTest, ListenersAreRemovedWithTheirConnection)
{
    int        count = 0;
    const auto connection =
        m_rocket.addComponentChangeListener([&count](const ComponentChangeEvent&) { ++count; });
    m_body->setMass(1.0);
    EXPECT_EQ(count, 1);
    EXPECT_TRUE(RocketComponent::removeComponentChangeListener(connection));
    EXPECT_FALSE(RocketComponent::removeComponentChangeListener(connection));
    m_body->setMass(2.0);
    EXPECT_EQ(count, 1);
}

TEST_F(RocketTest, AComponentAddsItsListenerToItsRocket)
{
    int        count = 0;
    const auto fromChild =
        m_body->addComponentChangeListener([&count](const ComponentChangeEvent&) { ++count; });
    m_nose->setMass(1.0);
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
    m_body->setMass(1.0);
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
    m_body->setMass(1.0);
    EXPECT_EQ(count, 2);
    EXPECT_TRUE(RocketComponent::removeChangeListener(first));
    m_body->setMass(2.0);
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
    second.addChild(TestBodyComponent::make(0.2, 0.01));
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
    TestMotorMount&             mount = m_stage->addChild(TestMotorMount::make(0.1, 0.01));
    const FlightConfigurationId a;
    m_rocket.createFlightConfiguration(a);
    mount.addMotor(a, QtRocket::Test::motorD21());
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

/// The positions of RocketTest.testEstesAlphaIII on the test double, and the bounds' x extent.
/// The centering rings' return from one instance to two (the double does not keep the instance
/// separation) is CenteringRing.EstesAlphaIIICenteringRingLocations, on the real CenteringRing.
/// Deferred until the fins and the launch lug exist: the launch lug's radial offset
/// (y = -0.015) and the y and z extents of the bounds (they need the real fin shapes).
TEST(RocketEstesAlphaIII, ComponentLocations)
{
    const QtRocket::Test::TestEstesAlphaIII rocket;
    const RocketComponent&                  stage = rocket.rocket->getChild(0);

    const RocketComponent& nose = stage.getChild(0);
    EXPECT_EQ(nose.getComponentLocations().at(0), (Coordinate{0, 0, 0})) << nose.getName();
    const RocketComponent& body = stage.getChild(1);
    EXPECT_EQ(body.getComponentLocations().at(0), (Coordinate{0.07, 0, 0})) << body.getName();

    const RocketComponent& fins = body.getChild(0);
    EXPECT_EQ(fins.getInstanceCount(), 3) << fins.getName();
    EXPECT_EQ(fins.getComponentLocations().at(0), (Coordinate{0.22, 0.012, 0})) << "fin #1";

    const RocketComponent& lugs = body.getChild(1);
    EXPECT_EQ(lugs.getInstanceCount(), 1) << lugs.getName();
    EXPECT_NEAR(lugs.getComponentLocations().at(0).x, 0.181, 1e-8) << lugs.getName();

    const RocketComponent& mmt = body.getChild(2);
    EXPECT_EQ(mmt.getComponentLocations().at(0), (Coordinate{0.203, 0, 0})) << mmt.getName();
    const RocketComponent& block = mmt.getChild(0);
    EXPECT_EQ(block.getComponentLocations().at(0), (Coordinate{0.203, 0, 0})) << block.getName();

    const RocketComponent& chute = body.getChild(3);
    EXPECT_EQ(chute.getComponentLocations().at(0), (Coordinate{0.098, 0, 0})) << chute.getName();

    TestComponent& ring = *rocket.rings;
    EXPECT_EQ(ring.getInstanceCount(), 2) << ring.getName();
    const std::vector<Coordinate> ringLocations = ring.getComponentLocations();
    EXPECT_EQ(ringLocations.at(0), (Coordinate{0.21, 0, 0})) << "first instance";
    EXPECT_EQ(ringLocations.at(1), (Coordinate{0.245, 0, 0})) << "second instance";
    // A single instance follows a different code path.
    ring.setInstanceCount(1);
    const Coordinate single = ring.getComponentLocations().at(0);
    EXPECT_NEAR(single.x, 0.21, 1e-8);
    EXPECT_NEAR(single.y, 0.0, 1e-8);
    EXPECT_NEAR(single.z, 0.0, 1e-8);
    EXPECT_EQ(single, (Coordinate{0.21, 0, 0}));

    const QtRocket::BoundingBox bounds = rocket.rocket->getBoundingBox();
    EXPECT_NEAR(bounds.min().x, 0.0, 1e-8);
    EXPECT_NEAR(bounds.max().x, 0.27, 1e-8);
}

/// The positions of RocketTest.testBeta on the test double, and the bounds' x extent (the y and z
/// extents need the real fin shapes).
TEST(RocketBeta, ComponentLocations)
{
    const QtRocket::Test::TestBeta beta;
    const RocketComponent&         body = *beta.boosterBody;
    EXPECT_EQ(body.getComponentLocations().at(0), (Coordinate{0.27, 0, 0}));
    EXPECT_EQ(body.getChild(0).getComponentLocations().at(0), (Coordinate{0.255, 0, 0}))
        << "the coupler";
    EXPECT_EQ(body.getChild(1).getInstanceCount(), 3);
    EXPECT_EQ(body.getChild(1).getComponentLocations().at(0), (Coordinate{0.28, 0.012, 0}))
        << "the fins";
    EXPECT_EQ(body.getChild(2).getComponentLocations().at(0), (Coordinate{0.285, 0, 0}))
        << "the motor mount";

    const QtRocket::BoundingBox bounds = beta.rocket->getBoundingBox();
    EXPECT_NEAR(bounds.min().x, 0.0, 1e-8);
    EXPECT_NEAR(bounds.max().x, 0.335, 1e-8);
}

/// Whether @p c sits at @p offset in its parent and at @p location in the rocket (x only).
::testing::AssertionResult isAt(const RocketComponent& c, double offset, double location)
{
    if (std::abs(offset - c.getPosition().x) > 1e-8)
    {
        return ::testing::AssertionFailure() << c.getName() << " offset " << c.getPosition().x;
    }
    const double x = c.getComponentLocations().at(0).x;
    if (std::abs(location - x) > 1e-8)
    {
        return ::testing::AssertionFailure() << c.getName() << " location " << x;
    }
    return ::testing::AssertionSuccess();
}

/// The positions of RocketTest.testFalcon9HComponentLocations on the test double, and the
/// bounds' x extent (the y and z extents need the real fin shapes).
TEST(RocketFalcon9Heavy, PayloadStageLocations)
{
    const QtRocket::Test::TestFalcon9Heavy f9h;
    EXPECT_TRUE(isAt(*f9h.payloadNose, 0.0, 0.0));
    EXPECT_TRUE(isAt(*f9h.payloadBody, 0.118, 0.118));
    EXPECT_TRUE(isAt(*f9h.payloadTransition, 0.250, 0.250));
    EXPECT_TRUE(isAt(*f9h.upperStageBody, 0.264, 0.264));
    EXPECT_TRUE(isAt(*f9h.parachute, 0.0775, 0.3415));
    EXPECT_TRUE(isAt(*f9h.shockCord, 0.155, 0.419));
    EXPECT_TRUE(isAt(*f9h.interstage, 0.444, 0.444));
}

TEST(RocketFalcon9Heavy, CoreAndBoosterLocations)
{
    const QtRocket::Test::TestFalcon9Heavy f9h;
    EXPECT_TRUE(isAt(*f9h.coreBody, 0.0, 0.564));

    const QtRocket::ParallelStage& boosters = *f9h.boosterStage;
    EXPECT_EQ(QtRocket::RadiusMethod::SURFACE, boosters.getRadiusMethod());
    EXPECT_EQ(QtRocket::AngleMethod::RELATIVE, boosters.getAngleMethod());
    EXPECT_NEAR(-0.08, boosters.getPosition().x, 1e-8);
    EXPECT_NEAR(0.0, boosters.getPosition().y, 1e-8);
    const std::vector<Coordinate> offsets = boosters.getInstanceOffsets();
    EXPECT_NEAR(0.0, offsets.at(0).x, 1e-8);
    EXPECT_NEAR(0.077, offsets.at(0).y, 1e-8);
    EXPECT_NEAR(-0.077, offsets.at(1).y, 1e-8);
    EXPECT_NEAR(0.0, offsets.at(0).z, 1e-8);
    const std::vector<Coordinate> locations = boosters.getComponentLocations();
    EXPECT_NEAR(0.484, locations.at(0).x, 1e-8);
    EXPECT_NEAR(0.077, locations.at(0).y, 1e-8);
    EXPECT_NEAR(-0.077, locations.at(1).y, 1e-8);

    EXPECT_TRUE(isAt(*f9h.boosterNose, 0.0, 0.484));
    EXPECT_TRUE(isAt(*f9h.boosterBody, 0.08, 0.564));
    EXPECT_TRUE(isAt(*f9h.boosterMotorTubes, 0.65, 1.214));
    EXPECT_TRUE(isAt(*f9h.boosterFins, 0.480, 1.044));

    const std::string           tree   = f9h.rocket->toDebugTree();
    const QtRocket::BoundingBox bounds = f9h.rocket->getBoundingBox();
    EXPECT_NEAR(0.0, bounds.min().x, 1e-8) << tree;
    EXPECT_NEAR(1.364, bounds.max().x, 1e-8) << tree;
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
    second.addChild(TestComponent::make(0.1));

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
    second.addChild(TestBodyComponent::make(0.1, 0.01));
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

/// A RocketTest whose rocket is changed after a snapshot: the name and mass of the body, the
/// reference type, the designer and an extra stage.
class LoadFromTest : public RocketTest
{
protected:
    LoadFromTest()
    {
        m_rocket.setReferenceType(ReferenceType::NOSECONE);
        m_snapshot    = m_rocket.copyRocketWithOriginalId();
        m_snapshotMod = m_rocket.getModId();

        m_body->setName("Changed");
        m_body->setMass(0.5);
        m_rocket.setReferenceType(ReferenceType::MAXIMUM);
        m_rocket.setDesigner("After");
        AxialStage& extra = m_rocket.addChild(std::make_unique<AxialStage>());
        extra.addChild(TestComponent::make(0.1));
        m_events.clear();
    }

    std::unique_ptr<Rocket> m_snapshot;
    ModId                   m_snapshotMod{ModId::zero()};
};

TEST_F(LoadFromTest, FiresOneUndoEvent)
{
    // During the undo event the replaced components still exist.
    const TestComponent* oldBody = m_body;
    std::string          nameDuringEvent;
    const auto           connection =
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

}  // namespace
