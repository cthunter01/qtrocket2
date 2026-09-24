#include "QtRocket/rocket/Rocket.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/DesignType.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/ReferenceType.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Strings.h"
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
using QtRocket::Coordinate;
using QtRocket::DesignType;
using QtRocket::FlightConfigurationId;
using QtRocket::ModId;
using QtRocket::ReferenceType;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Test::TestComponent;

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
        m_body  = &m_stage->addChild(TestComponent::make(0.3));
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
    // HOOK(rocket-config): the selected configuration's length; until then the stage lengths.
    EXPECT_DOUBLE_EQ(m_rocket.getLength(), 0.4);
    AxialStage& second = m_rocket.addChild(std::make_unique<AxialStage>());
    EXPECT_DOUBLE_EQ(m_rocket.getLength(), 0.4);
    second.addChild(TestComponent::make(0.2)).setOuterRadius(0.03);
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
    int  count = 0;
    auto connection =
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
    int  count = 0;
    auto fromChild =
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
    int  count  = 0;
    auto first  = m_body->addChangeListener([&count](const ComponentChangeEvent&) { ++count; });
    auto second = m_body->addChangeListener([&count](const ComponentChangeEvent&) { ++count; });
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
    // rocket-config will update only these configurations; the event itself is the same.
    const std::vector<FlightConfigurationId> ids{FlightConfigurationId{}};
    m_rocket.fireComponentChangeEvent(ComponentChangeEvent::kMotorChange, ids);
    ASSERT_EQ(m_events.size(), 1U);
    EXPECT_EQ(m_events[0].type, ComponentChangeEvent::kMotorChange);
    EXPECT_EQ(m_events[0].source, &m_rocket);
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

TEST_F(RocketTest, ActivenessHook)
{
    EXPECT_TRUE(m_rocket.isStageActiveInSelectedConfiguration(-1));
    EXPECT_TRUE(m_rocket.isStageActiveInSelectedConfiguration(0));
    EXPECT_FALSE(m_rocket.isStageActiveInSelectedConfiguration(1));
    EXPECT_TRUE(m_rocket.isComponentActiveInSelectedConfiguration(*m_body));
    EXPECT_TRUE(m_rocket.isComponentActiveInSelectedConfiguration(m_rocket));
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
