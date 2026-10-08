#include "QtRocket/rocket/RocketComponent.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Uuid.h"
#include "rocket/FailingListener.h"
#include "rocket/TestComponent.h"

namespace
{

using QtRocket::Appearance;
using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::Color;
using QtRocket::ComponentAssembly;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::Coordinate;
using QtRocket::LineStyle;
using QtRocket::Manufacturer;
using QtRocket::ModId;
using QtRocket::NoseCone;
using QtRocket::ParallelStage;
using QtRocket::RadiusMethod;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::TransitionShape;
using QtRocket::TypedPropertyMap;
using QtRocket::Uuid;
using QtRocket::Test::FailingListener;
using QtRocket::Test::TestComponent;
using StageTracking = RocketComponent::StageTracking;

constexpr double kPi = std::numbers::pi;

::testing::AssertionResult coordinatesNear(const Coordinate& actual, const Coordinate& expected,
                                           double tolerance = 1e-12)
{
    if (std::abs(actual.x - expected.x) <= tolerance &&
        std::abs(actual.y - expected.y) <= tolerance &&
        std::abs(actual.z - expected.z) <= tolerance)
    {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure()
           << actual.toPreciseString() << " is not " << expected.toPreciseString();
}

/// The bodies of TestRockets.makeEstesAlphaIII(): a stage holding a nose cone (0.07 m) and a body
/// tube (0.2 m, radius 0.012 m), and in the body tube a generic part 0.05 m long positioned at
/// its bottom, which the positioning tests move. RocketTest's own positioning tests
/// (testChangeAxialMethod and its neighbours) run on the whole rocket, in RocketTests.cpp.
class EstesTreeTest : public ::testing::Test
{
protected:
    EstesTreeTest()
    {
        m_stage = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_nose =
            &m_stage->addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.07, 0.012));
        m_body = &m_stage->addChild(std::make_unique<BodyTube>(0.2, 0.012));
        m_part = &m_body->addChild(std::make_unique<TestComponent>(ComponentKind::MASS_COMPONENT,
                                                                   AxialMethod::BOTTOM, 0.05));
        m_rocket.enableEvents();
    }

    Rocket         m_rocket;
    AxialStage*    m_stage{nullptr};
    NoseCone*      m_nose{nullptr};
    BodyTube*      m_body{nullptr};
    TestComponent* m_part{nullptr};
};

// ---- Positions ----

TEST_F(EstesTreeTest, EveryAxialMethodPositionsAsItsArithmetic)
{
    m_part->setAxialOffset(AxialMethod::TOP, 0.03);
    EXPECT_DOUBLE_EQ(m_part->getPosition().x, 0.03);
    m_part->setAxialOffset(AxialMethod::MIDDLE, 0.03);
    EXPECT_DOUBLE_EQ(m_part->getPosition().x, 0.03 + ((0.2 - 0.05) / 2));
    m_part->setAxialOffset(AxialMethod::BOTTOM, 0.03);
    EXPECT_DOUBLE_EQ(m_part->getPosition().x, 0.03 + (0.2 - 0.05));
    // ABSOLUTE is from the rocket's tip: the body starts at 0.07.
    m_part->setAxialOffset(AxialMethod::ABSOLUTE, 0.25);
    EXPECT_DOUBLE_EQ(m_part->getPosition().x, 0.25 - 0.07);
    EXPECT_EQ(m_part->getAxialMethod(), AxialMethod::ABSOLUTE);
    EXPECT_EQ(m_part->getAxialOffset(), 0.25);
    // Asking for another method's offset changes nothing.
    EXPECT_DOUBLE_EQ(m_part->getAxialOffset(AxialMethod::TOP), 0.18);
    EXPECT_DOUBLE_EQ(m_part->getAxialOffset(AxialMethod::BOTTOM), 0.18 + (0.05 - 0.2));
    EXPECT_EQ(m_part->getAxialMethod(), AxialMethod::ABSOLUTE);
    EXPECT_DOUBLE_EQ(m_part->getAxialFront(), 0.18);
}

TEST_F(EstesTreeTest, AfterFromAnotherMethodUsesTheParentLengthUntilTheNextUpdate)
{
    // Java tests isAfter() on the current method: a component switched to AFTER through the
    // two-argument setter is first placed parent length + offset, then setAfter() on the next
    // update.
    m_part->setAxialOffset(AxialMethod::AFTER, 0.01);
    EXPECT_DOUBLE_EQ(m_part->getPosition().x, 0.21);
    m_part->setAxialOffset(0.01);             // fires, which updates every component
    EXPECT_EQ(m_part->getPosition().x, 0.0);  // the first child of the body
    EXPECT_EQ(m_part->getAxialOffset(), 0.0);
}

TEST_F(EstesTreeTest, SmallPositionsSnapToZeroAndNaNIsABug)
{
    m_part->setAxialOffset(AxialMethod::TOP, 5e-7);
    EXPECT_EQ(m_part->getPosition().x, 0.0);
    EXPECT_EQ(m_part->getAxialOffset(), 5e-7);
    m_part->setAxialOffset(AxialMethod::TOP, 2e-6);
    EXPECT_EQ(m_part->getPosition().x, 2e-6);
    EXPECT_THROW(m_part->setAxialOffset(AxialMethod::TOP, std::numeric_limits<double>::quiet_NaN()),
                 BugError);
}

// OpenRocket throws its BugException for every position that comes out NaN, also when every
// component recomputes its position at an event (probe n000 of tier 9b, part R2: a mass object
// whose length a packed radius of 1e300 has made NaN, at the next element of the file). Here
// only a NaN that is asked for is a bug; one that the tree's own lengths make is stored.
TEST_F(EstesTreeTest, APositionThatComesOutNaNFromTheTreeIsStoredAndOnlyANaNAskedForIsABug)
{
    const double        infinity = std::numeric_limits<double>::infinity();
    QtRocket::Bulkhead& ring     = m_body->addChild(std::make_unique<QtRocket::Bulkhead>());
    ASSERT_EQ(ring.getAxialMethod(), AxialMethod::BOTTOM);

    m_body->setLength(infinity);
    EXPECT_EQ(ring.getPosition().x, infinity);  // 0 + (infinity - 0.002)
    // The event of this setter makes every component compute its position again: 0 +
    // (infinity - infinity).
    EXPECT_NO_THROW(ring.setLength(infinity));
    EXPECT_TRUE(std::isnan(ring.getPosition().x));
    EXPECT_EQ(ring.getAxialOffset(), 0.0);

    // An offset that is a number is taken, whatever comes of it in such a tree.
    EXPECT_NO_THROW(ring.setAxialOffset(0.1));
    EXPECT_EQ(ring.getAxialOffset(), 0.1);
    EXPECT_TRUE(std::isnan(ring.getPosition().x));
    // Another method takes its offset from the position, a NaN, and fires.
    EXPECT_NO_THROW(ring.setAxialMethod(AxialMethod::TOP));
    EXPECT_TRUE(std::isnan(ring.getAxialOffset()));
    EXPECT_NO_THROW(m_rocket.update());

    // Asking for a NaN is the caller's mistake, as ever.
    EXPECT_THROW(ring.setAxialOffset(std::numeric_limits<double>::quiet_NaN()), BugError);

    // With lengths that are numbers again the ring is placed as any other.
    m_body->setLength(0.2);
    ring.setLength(0.002);
    ring.setAxialOffset(0.01);
    EXPECT_EQ(ring.getPosition().x, 0.01);
}

TEST_F(EstesTreeTest, AfterSkipsEmptyStages)
{
    // A second stage after an empty one: the empty stage is inactive, so the reference point
    // restarts at 0.
    AxialStage& empty = m_rocket.addChild(std::make_unique<AxialStage>());
    AxialStage& third = m_rocket.addChild(std::make_unique<AxialStage>());
    third.addChild(std::make_unique<BodyTube>(0.3, 0.012));
    // The third stage follows the first (0.27 long): the empty one in between is skipped.
    EXPECT_DOUBLE_EQ(empty.getPosition().x, 0.27);
    EXPECT_DOUBLE_EQ(third.getPosition().x, 0.27);

    // With the first stage removed as well, nothing active precedes it.
    const std::unique_ptr<RocketComponent> first = m_rocket.removeChild(0);
    EXPECT_EQ(third.getPosition().x, 0.0);
}

TEST(RocketComponentPosition, DetachedComponentTakesTheOffsetAsPosition)
{
    TestComponent lonely{ComponentKind::MASS_COMPONENT, AxialMethod::TOP, 0.1};
    lonely.setAxialOffset(AxialMethod::MIDDLE, 0.4);
    EXPECT_EQ(lonely.getPosition().x, 0.4);
    lonely.setAfter();  // nothing without a parent
    EXPECT_EQ(lonely.getAxialMethod(), AxialMethod::MIDDLE);
}

TEST(RocketComponentPosition, RadiusOffsetInAnotherMethod)
{
    // A radius-positionable component (a TestComponent of kind POD_SET, with a bounding radius
    // of its own) on a body tube of radius 0.05 m.
    BodyTube       body{0.3, 0.05};
    TestComponent& pod = body.addChild(TestComponent::make(0.1, ComponentKind::POD_SET));
    pod.setBoundingRadius(0.01);
    pod.setRadius(RadiusMethod::RELATIVE, 0.02);
    // The radius is 0.02 + 0.05 + 0.01 = 0.08 from the axis.
    EXPECT_NEAR(pod.getRadiusOffset(RadiusMethod::FREE), 0.08, 1e-15);
    EXPECT_NEAR(pod.getRadiusOffset(RadiusMethod::RELATIVE), 0.02, 1e-15);
    EXPECT_EQ(pod.getRadiusOffset(RadiusMethod::SURFACE), 0.0);
    // The base answers are coaxial: no offset, no angle.
    const Rocket rocket;
    EXPECT_EQ(rocket.getRadiusOffset(), 0.0);
    EXPECT_EQ(rocket.getRadiusMethod(), RadiusMethod::COAXIAL);
    EXPECT_EQ(rocket.getAngleOffset(), 0.0);
}

// ---- Instances ----

/// A body tube holding a two-instance "pod" (at y = +-0.1, angles 0 and pi) whose child has two
/// instances of its own (offsets (0, 0.01, 0) and (0, 0, 0.02)).
class InstanceTreeTest : public ::testing::Test
{
protected:
    InstanceTreeTest()
    {
        AxialStage& stage = m_rocket.addChild(std::make_unique<AxialStage>());
        m_body            = &stage.addChild(std::make_unique<BodyTube>(0.3, 0.025));
        m_pod             = &m_body->addChild(
            std::make_unique<TestComponent>(ComponentKind::POD_SET, AxialMethod::TOP));
        m_pod->setAxialOffset(AxialMethod::TOP, 0.1);
        m_pod->setInstances({Coordinate{0, 0.1, 0}, Coordinate{0, -0.1, 0}}, {0.0, kPi});
        m_leaf = &m_pod->addChild(
            std::make_unique<TestComponent>(ComponentKind::MASS_COMPONENT, AxialMethod::TOP));
        m_leaf->setAxialOffset(AxialMethod::TOP, 0.05);
        m_leaf->setInstances({Coordinate{0, 0.01, 0}, Coordinate{0, 0, 0.02}}, {0.0, 0.0});
        m_rocket.enableEvents();
    }

    Rocket         m_rocket;
    BodyTube*      m_body{nullptr};
    TestComponent* m_pod{nullptr};
    TestComponent* m_leaf{nullptr};
};

TEST_F(InstanceTreeTest, InstanceLocationsAreRelativeToTheParent)
{
    const std::vector<Coordinate> locations = m_pod->getInstanceLocations();
    ASSERT_EQ(locations.size(), 2U);
    EXPECT_TRUE(coordinatesNear(locations[0], Coordinate{0.1, 0.1, 0}));
    EXPECT_TRUE(coordinatesNear(locations[1], Coordinate{0.1, -0.1, 0}));
    EXPECT_EQ(m_pod->getInstanceCount(), 2);
}

TEST_F(InstanceTreeTest, LocationsComposeThroughTwoLevelsOfParents)
{
    const std::vector<Coordinate> pod = m_pod->getComponentLocations();
    ASSERT_EQ(pod.size(), 2U);
    EXPECT_TRUE(coordinatesNear(pod[0], Coordinate{0.1, 0.1, 0}));
    EXPECT_TRUE(coordinatesNear(pod[1], Coordinate{0.1, -0.1, 0}));

    // Index = parent instance + parent count * own instance; the second pod instance is turned
    // by pi about the x axis, which flips y and z of the leaf's offsets.
    const std::vector<Coordinate> leaf = m_leaf->getComponentLocations();
    ASSERT_EQ(leaf.size(), 4U);
    EXPECT_TRUE(coordinatesNear(leaf[0], Coordinate{0.15, 0.11, 0}));
    EXPECT_TRUE(coordinatesNear(leaf[1], Coordinate{0.15, -0.11, 0}));
    EXPECT_TRUE(coordinatesNear(leaf[2], Coordinate{0.15, 0.1, 0.02}));
    EXPECT_TRUE(coordinatesNear(leaf[3], Coordinate{0.15, -0.1, -0.02}));
}

TEST_F(InstanceTreeTest, AnglesComposeThroughTheParents)
{
    const std::vector<Coordinate> pod = m_pod->getComponentAngles();
    ASSERT_EQ(pod.size(), 2U);
    EXPECT_EQ(pod[0].x, 0.0);
    EXPECT_EQ(pod[1].x, kPi);

    const std::vector<Coordinate> leaf = m_leaf->getComponentAngles();
    ASSERT_EQ(leaf.size(), 4U);
    EXPECT_EQ(leaf[0].x, 0.0);
    EXPECT_EQ(leaf[1].x, kPi);
    EXPECT_EQ(leaf[2].x, 0.0);
    EXPECT_EQ(leaf[3].x, kPi);
    EXPECT_TRUE(std::ranges::all_of(
        leaf, [](const Coordinate& angle) { return angle.y == 0.0 && angle.z == 0.0; }));
}

TEST_F(InstanceTreeTest, ToAbsoluteAddsToEveryInstance)
{
    const std::vector<Coordinate> absolute = m_pod->toAbsolute(Coordinate{0.01, 0, 0});
    ASSERT_EQ(absolute.size(), 2U);
    EXPECT_TRUE(coordinatesNear(absolute[0], Coordinate{0.11, 0.1, 0}));
    EXPECT_TRUE(coordinatesNear(absolute[1], Coordinate{0.11, -0.1, 0}));

    // Relative to the pod: from the leaf's first instance to each pod instance.
    const std::vector<Coordinate> relative = m_leaf->toRelative(Coordinate{}, *m_pod);
    ASSERT_EQ(relative.size(), 2U);
    EXPECT_TRUE(coordinatesNear(relative[0], Coordinate{0.05, 0.01, 0}));
    EXPECT_TRUE(coordinatesNear(relative[1], Coordinate{0.05, 0.21, 0}));
}

TEST_F(InstanceTreeTest, CachesAreClearedByEventsOnly)
{
    const Coordinate before = m_leaf->getComponentLocations().at(0);
    // Without an event (the two-argument setter fires none) the cache stays, as in OpenRocket.
    m_leaf->setAxialOffset(AxialMethod::TOP, 0.07);
    EXPECT_TRUE(coordinatesNear(m_leaf->getComponentLocations().at(0), before));
    // Any event clears it.
    m_leaf->setAxialOffset(0.07);
    EXPECT_TRUE(coordinatesNear(m_leaf->getComponentLocations().at(0), Coordinate{0.17, 0.11, 0}));
}

TEST(RocketComponentInstances, DefaultsAreASingleInstance)
{
    const TestComponent component;
    const Rocket        rocket;
    EXPECT_EQ(rocket.getInstanceCount(), 1);
    EXPECT_EQ(rocket.getInstanceOffsets().size(), 1U);
    EXPECT_EQ(rocket.getInstanceAngles(), std::vector<double>{0.0});
    // A root's locations are its instance offsets.
    EXPECT_TRUE(coordinatesNear(component.getComponentLocations().at(0), Coordinate{}));
    EXPECT_TRUE(coordinatesNear(component.getComponentAngles().at(0), Coordinate{}));
}

TEST(RocketComponentInstances, InconsistentInstancesAreBugs)
{
    // A parent whose instance offsets and angles differ in number (Java runs off the shorter
    // array).
    TestComponent  parent;
    TestComponent& child = parent.addChild(TestComponent::make());
    parent.setInstances({Coordinate{}, Coordinate{}}, {0.0});
    EXPECT_THROW(static_cast<void>(child.getComponentLocations()), BugError);

    // Without instances there are no locations, and no first location.
    TestComponent lonely;
    lonely.setInstances({}, {});
    EXPECT_TRUE(lonely.getComponentLocations().empty());
    EXPECT_TRUE(lonely.getComponentAngles().empty());
    EXPECT_THROW(static_cast<void>(lonely.getAxialOffset(AxialMethod::ABSOLUTE)), BugError);
    EXPECT_THROW(static_cast<void>(lonely.toRelative(Coordinate{}, parent)), BugError);
}

// ---- Tree ownership ----

TEST(RocketComponentTree, AddChildTakesOwnershipAndLinksTheParent)
{
    TestComponent  parent;
    auto           owned = TestComponent::make(0.1);
    TestComponent* raw   = owned.get();
    TestComponent& added = parent.addChild(std::move(owned));
    EXPECT_EQ(&added, raw);
    EXPECT_EQ(added.getParent(), &parent);
    EXPECT_EQ(parent.getChildCount(), 1U);
    EXPECT_EQ(&parent.getChild(0), raw);
    EXPECT_EQ(parent.getChildPosition(raw), std::optional<std::size_t>{0});
    EXPECT_NO_THROW(parent.checkComponentStructure());
    EXPECT_NO_THROW(added.checkComponentStructure());
}

TEST(RocketComponentTree, AddChildAtAnIndex)
{
    TestComponent  parent;
    TestComponent& a = parent.addChild(TestComponent::make());
    TestComponent& c = parent.addChild(TestComponent::make());
    TestComponent& b = parent.addChild(TestComponent::make(), 1);
    TestComponent& z = parent.addChild(TestComponent::make(), std::size_t{0});
    EXPECT_EQ(parent.getChildren(), (std::vector<RocketComponent*>{&z, &a, &b, &c}));
    EXPECT_THROW(parent.addChild(TestComponent::make(), 9), BugError);
    EXPECT_THROW(parent.addChild(TestComponent::make(), -1), BugError);
    EXPECT_EQ(parent.getChildCount(), 4U);
}

TEST(RocketComponentTree, AddChildRejectsIncompatibleAndNull)
{
    TestComponent parent;
    parent.setAccepted({ComponentKind::PARACHUTE});
    EXPECT_TRUE(parent.allowsChildren());
    EXPECT_THROW(parent.addChild(TestComponent::make()), BugError);
    EXPECT_NO_THROW(parent.addChild(TestComponent::make(0.0, ComponentKind::PARACHUTE)));
    EXPECT_THROW(parent.addChild(std::unique_ptr<TestComponent>{}), BugError);
    parent.setAcceptsNothing();
    EXPECT_FALSE(parent.allowsChildren());
    EXPECT_FALSE(parent.isCompatible(TestComponent{ComponentKind::PARACHUTE}));
}

TEST(RocketComponentTree, ARejectedComponentStaysWithTheCaller)
{
    TestComponent parent;
    parent.setAccepted({ComponentKind::PARACHUTE});
    auto                 incompatible = TestComponent::make();
    const TestComponent* raw          = incompatible.get();
    EXPECT_THROW(parent.addChild(std::move(incompatible)), BugError);
    // NOLINTNEXTLINE(bugprone-use-after-move): addChild() refused it before taking it
    EXPECT_EQ(incompatible.get(), raw);

    auto chute = TestComponent::make(0.0, ComponentKind::PARACHUTE);
    EXPECT_THROW(parent.addChild(std::move(chute), 1), BugError);  // index out of range
    // NOLINTNEXTLINE(bugprone-use-after-move): addChild() refused it before taking it
    EXPECT_NE(chute, nullptr);
    EXPECT_EQ(parent.getChildCount(), 0U);
}

TEST(RocketComponentTree, AddingTheRootBelowItselfIsABug)
{
    // Java: IllegalStateException "attempting to create cycle in tree". The root stays with the
    // caller (nothing leaks).
    auto           root  = TestComponent::make();
    TestComponent& child = root->addChild(TestComponent::make());
    EXPECT_THROW(child.addChild(std::move(root)), BugError);
    // NOLINTBEGIN(bugprone-use-after-move): addChild() refused the root before taking it
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->getParent(), nullptr);
    EXPECT_EQ(child.getParent(), root.get());
    EXPECT_EQ(child.getChildCount(), 0U);

    // Java compares with equals(): a copy of the root that keeps its id is refused too, and one
    // with new ids is not.
    std::unique_ptr<RocketComponent> sameId = root->copyWithOriginalId();
    EXPECT_THROW(child.addChild(std::move(sameId)), BugError);
    EXPECT_NE(sameId, nullptr);
    EXPECT_NO_THROW(child.addChild(root->copyWithNewIds()));
    EXPECT_EQ(child.getChildCount(), 1U);
    // NOLINTEND(bugprone-use-after-move)
}

TEST(RocketComponentTree, ChildAddedRunsAfterEveryAddAndItsEvent)
{
    Rocket         rocket;
    AxialStage&    stage  = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&      body   = stage.addChild(std::make_unique<BodyTube>(0.3, 0.025));
    TestComponent& holder = body.addChild(TestComponent::make(0.3));
    rocket.enableEvents();

    std::vector<std::string>            log;
    std::vector<const RocketComponent*> added;
    const auto                          connection = ComponentChangeSignal::ScopedConnection{
        rocket.addComponentChangeListener([&log](const ComponentChangeEvent& e) {
            if (e.isTreeChange())
            {
                log.emplace_back("event");
            }
        })};
    holder.setOnChildAdded([&log, &added](RocketComponent& child) {
        log.emplace_back("hook");
        added.push_back(&child);
    });

    // Every add path: at the end, at an index, and without stage tracking.
    TestComponent& atEnd   = holder.addChild(TestComponent::make());
    TestComponent& atIndex = holder.addChild(TestComponent::make(), 0);
    TestComponent& skipped = holder.addChild(TestComponent::make(), StageTracking::SKIP);
    EXPECT_EQ(added, (std::vector<const RocketComponent*>{&atEnd, &atIndex, &skipped}));
    EXPECT_EQ(log, (std::vector<std::string>{"event", "hook", "event", "hook", "event", "hook"}));
}

/// A childAdded() hook that counts its calls in @p count.
std::function<void(RocketComponent&)> countingHook(int& count)
{
    return [&count](RocketComponent& /*added*/) { ++count; };
}

TEST(RocketComponentTree, ChildAddedRunsOnTheNewParentOnly)
{
    TestComponent  parent;  // detached: no events, the hook runs all the same
    TestComponent& child = parent.addChild(TestComponent::make());
    int            count = 0;
    parent.setOnChildAdded(countingHook(count));

    parent.addChild(TestComponent::make());
    child.addChild(TestComponent::make());  // a grandchild: not the parent's hook
    EXPECT_EQ(count, 1);
}

TEST(RocketComponentTree, ChildAddedDoesNotRunForARefusedComponent)
{
    TestComponent parent;
    int           count = 0;
    parent.setOnChildAdded(countingHook(count));
    parent.setAcceptsNothing();
    EXPECT_THROW(parent.addChild(TestComponent::make()), BugError);
    EXPECT_EQ(count, 0);
}

TEST(RocketComponentTree, RemoveChildHandsOwnershipBack)
{
    TestComponent  parent;
    TestComponent& first  = parent.addChild(TestComponent::make());
    TestComponent& second = parent.addChild(TestComponent::make());
    TestComponent& grand  = second.addChild(TestComponent::make());

    std::unique_ptr<RocketComponent> removed = parent.removeChild(&second);
    ASSERT_EQ(removed.get(), &second);
    EXPECT_EQ(removed->getParent(), nullptr);
    EXPECT_EQ(parent.getChildren(), std::vector<RocketComponent*>{&first});
    // The subtree comes along.
    EXPECT_EQ(grand.getParent(), removed.get());
    EXPECT_EQ(&grand.getRoot(), removed.get());

    // Not a child: nothing happens.
    EXPECT_EQ(parent.removeChild(&grand), nullptr);
    EXPECT_EQ(parent.removeChild(static_cast<const RocketComponent*>(nullptr)), nullptr);
    EXPECT_THROW(static_cast<void>(parent.removeChild(5)), BugError);

    // A removed component can be added elsewhere.
    TestComponent& readded =
        first.addChild(QtRocket::componentCast<TestComponent>(std::move(removed)));
    EXPECT_EQ(readded.getParent(), &first);
}

TEST(RocketComponentTree, MoveChild)
{
    TestComponent  parent;
    TestComponent& a = parent.addChild(TestComponent::make());
    TestComponent& b = parent.addChild(TestComponent::make());
    TestComponent& c = parent.addChild(TestComponent::make());
    parent.moveChild(&a, 2);
    EXPECT_EQ(parent.getChildren(), (std::vector<RocketComponent*>{&b, &c, &a}));
    parent.moveChild(&c, 0);
    EXPECT_EQ(parent.getChildren(), (std::vector<RocketComponent*>{&c, &b, &a}));
    // Not a child: nothing happens; a bad index is a bug and keeps the children.
    TestComponent outsider;
    parent.moveChild(&outsider, 0);
    EXPECT_THROW(parent.moveChild(&a, 3), BugError);
    EXPECT_EQ(parent.getChildren(), (std::vector<RocketComponent*>{&c, &b, &a}));
    EXPECT_EQ(a.getParent(), &parent);
}

TEST(RocketComponentTree, GetChildOutOfRangeIsABug)
{
    TestComponent parent;
    EXPECT_THROW(static_cast<void>(parent.getChild(0)), BugError);
    const TestComponent& constParent = parent;
    EXPECT_THROW(static_cast<void>(constParent.getChild(0)), BugError);
    EXPECT_EQ(parent.getChildPosition(&parent), std::nullopt);
}

TEST(RocketComponentTree, NavigationHelpers)
{
    TestComponent  root;
    TestComponent& a  = root.addChild(TestComponent::make());
    TestComponent& a1 = a.addChild(TestComponent::make());
    TestComponent& a2 = a.addChild(TestComponent::make());
    TestComponent& b  = root.addChild(TestComponent::make());

    EXPECT_EQ(root.getAllChildren(), (std::vector<RocketComponent*>{&a, &a1, &a2, &b}));
    EXPECT_EQ(a2.getParents(), (std::vector<RocketComponent*>{&a, &root}));
    EXPECT_TRUE(root.isAncestor(a2));
    EXPECT_TRUE(a.isAncestor(a1));
    EXPECT_FALSE(b.isAncestor(a1));
    EXPECT_FALSE(a1.isAncestor(a1));
    EXPECT_TRUE(root.containsChild(&a2));
    EXPECT_FALSE(a.containsChild(&b));
    EXPECT_EQ(&a2.getRoot(), &root);

    // Tree order: next and previous.
    EXPECT_EQ(root.getNextComponent(), &a);
    EXPECT_EQ(a.getNextComponent(), &a1);
    EXPECT_EQ(a1.getNextComponent(), &a2);
    EXPECT_EQ(a2.getNextComponent(), &b);
    EXPECT_EQ(b.getNextComponent(), nullptr);
    EXPECT_EQ(b.getPreviousComponent(), &a2);
    EXPECT_EQ(a1.getPreviousComponent(), &a);
    EXPECT_EQ(a.getPreviousComponent(), &root);
    EXPECT_EQ(root.getPreviousComponent(), nullptr);

    const std::vector<const RocketComponent*> list{&a};
    EXPECT_TRUE(RocketComponent::listContainsParent(list, a2));
    EXPECT_FALSE(RocketComponent::listContainsParent(list, b));
    EXPECT_TRUE(root.checkAllClassesEqual(list));
    const Rocket                              rocket;
    const std::vector<const RocketComponent*> mixed{&a, &rocket};
    EXPECT_FALSE(root.checkAllClassesEqual(mixed));
    EXPECT_TRUE(root.checkAllClassesEqual({}));
}

TEST(RocketComponentTree, RocketStageAndAssemblyLookups)
{
    Rocket         rocket;
    AxialStage&    stage = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&      body  = stage.addChild(std::make_unique<BodyTube>());
    TestComponent& inner = body.addChild(TestComponent::make());

    EXPECT_EQ(&inner.getRocket(), &rocket);
    EXPECT_EQ(inner.findRocket(), &rocket);
    EXPECT_EQ(&inner.getStage(), &stage);
    EXPECT_EQ(&inner.getAssembly(), &stage);
    EXPECT_EQ(&stage.getAssembly(), &stage);
    EXPECT_EQ(&rocket.getAssembly(), &rocket);
    EXPECT_EQ(inner.getStageNumber(), 0);
    EXPECT_EQ(inner.getParentAssemblies(), (std::vector<RocketComponent*>{&stage, &rocket}));
    EXPECT_EQ(rocket.getAllChildAssemblies(), std::vector<ComponentAssembly*>{&stage});
    EXPECT_EQ(rocket.getDirectChildAssemblies(), std::vector<ComponentAssembly*>{&stage});
    EXPECT_EQ(body.getDirectChildAssemblies(), std::vector<ComponentAssembly*>{});

    // Without a rocket, stage or assembly above.
    TestComponent lonely;
    EXPECT_EQ(lonely.findRocket(), nullptr);
    EXPECT_EQ(lonely.findStage(), nullptr);
    EXPECT_EQ(lonely.findAssembly(), nullptr);
    EXPECT_THROW(static_cast<void>(lonely.getRocket()), BugError);
    EXPECT_THROW(static_cast<void>(lonely.getStage()), BugError);
    EXPECT_THROW(static_cast<void>(lonely.getAssembly()), BugError);
    EXPECT_THROW(static_cast<void>(lonely.getStageNumber()), BugError);
    EXPECT_EQ(rocket.findStage(), nullptr);
}

TEST(RocketComponentTree, ConstQueries)
{
    // A core stage whose body tube carries a booster set (a stage below a component), then an
    // upper stage.
    Rocket         rocket;
    AxialStage&    core    = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&      body    = core.addChild(std::make_unique<BodyTube>());
    ParallelStage& booster = body.addChild(std::make_unique<ParallelStage>());
    BodyTube&      inner   = booster.addChild(std::make_unique<BodyTube>());
    AxialStage&    upper   = rocket.addChild(std::make_unique<AxialStage>());

    const Rocket& constRocket = rocket;
    EXPECT_EQ(constRocket.getSubStages(),
              (std::vector<const AxialStage*>{&core, &booster, &upper}));
    EXPECT_EQ(constRocket.getAllChildStages(),
              (std::vector<const AxialStage*>{&core, &booster, &upper}));
    EXPECT_EQ(constRocket.getTopLevelChildStages(),
              (std::vector<const AxialStage*>{&core, &upper}));
    EXPECT_EQ(std::as_const(core).getTopLevelChildStages(),
              std::vector<const AxialStage*>{&booster});
    EXPECT_EQ(constRocket.getAllChildAssemblies(),
              (std::vector<const ComponentAssembly*>{&core, &booster, &upper}));
    EXPECT_EQ(constRocket.getDirectChildAssemblies(),
              (std::vector<const ComponentAssembly*>{&core, &upper}));
    EXPECT_EQ(std::as_const(body).getDirectChildAssemblies(),
              std::vector<const ComponentAssembly*>{&booster});

    const RocketComponent& constInner = inner;
    EXPECT_EQ(constInner.getParentAssemblies(),
              (std::vector<const RocketComponent*>{&booster, &core, &rocket}));
    EXPECT_EQ(constInner.getNextComponent(), &upper);
    EXPECT_EQ(constInner.getPreviousComponent(), &booster);
    EXPECT_EQ(std::as_const(upper).getPreviousComponent(), &inner);
    EXPECT_EQ(std::as_const(upper).getNextComponent(), nullptr);
    EXPECT_EQ(constRocket.getNextComponent(), &core);
    EXPECT_EQ(constRocket.getPreviousComponent(), nullptr);
}

TEST(RocketComponentTree, StagesBelowAComponent)
{
    Rocket         rocket;
    AxialStage&    core    = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&      body    = core.addChild(std::make_unique<BodyTube>());
    ParallelStage& booster = body.addChild(std::make_unique<ParallelStage>());
    booster.addChild(std::make_unique<BodyTube>());
    AxialStage& upper = rocket.addChild(std::make_unique<AxialStage>());

    EXPECT_EQ(rocket.getSubStages(), (std::vector<AxialStage*>{&core, &booster, &upper}));
    EXPECT_EQ(rocket.getAllChildStages(), (std::vector<AxialStage*>{&core, &booster, &upper}));
    EXPECT_EQ(rocket.getTopLevelChildStages(), (std::vector<AxialStage*>{&core, &upper}));
    EXPECT_EQ(core.getTopLevelChildStages(), std::vector<AxialStage*>{&booster});
}

// ---- Iteration ----

TEST(RocketComponentIteration, SubtreeIsPreOrder)
{
    TestComponent  root;
    TestComponent& a  = root.addChild(TestComponent::make());
    TestComponent& a1 = a.addChild(TestComponent::make());
    TestComponent& b  = root.addChild(TestComponent::make());
    TestComponent& b1 = b.addChild(TestComponent::make());
    TestComponent& b2 = b.addChild(TestComponent::make());

    std::vector<const RocketComponent*> visited;
    for (const RocketComponent& c : std::as_const(root).subtree())
    {
        visited.push_back(&c);
    }
    EXPECT_EQ(visited, (std::vector<const RocketComponent*>{&root, &a, &a1, &b, &b1, &b2}));

    visited.clear();
    for (RocketComponent& c : root.subtree(false))
    {
        visited.push_back(&c);
    }
    EXPECT_EQ(visited, (std::vector<const RocketComponent*>{&a, &a1, &b, &b1, &b2}));

    // A leaf alone.
    visited.clear();
    for (RocketComponent& c : b2.subtree(false))
    {
        visited.push_back(&c);
    }
    EXPECT_TRUE(visited.empty());

    // forEach visits the same order with one visitor object.
    int count = 0;
    root.forEach([&count](RocketComponent& /*c*/) { ++count; });
    EXPECT_EQ(count, 6);
    std::vector<const RocketComponent*> visitedConst;
    std::as_const(root).forEach(
        [&visitedConst](const RocketComponent& c) { visitedConst.push_back(&c); }, false);
    EXPECT_EQ(visitedConst, (std::vector<const RocketComponent*>{&a, &a1, &b, &b1, &b2}));
}

TEST(RocketComponentIteration, IteratorOperations)
{
    TestComponent  root;
    TestComponent& a     = root.addChild(TestComponent::make());
    const auto     range = root.subtree();
    auto           it    = range.begin();
    EXPECT_EQ(&*it, &root);
    EXPECT_EQ(it->getParent(), nullptr);
    const auto previous = it++;
    EXPECT_EQ(&*previous, &root);
    EXPECT_EQ(&*it, &a);
    ++it;
    EXPECT_EQ(it, range.end());
    EXPECT_THROW(static_cast<void>(*it), BugError);
}

TEST(RocketComponentIteration, FailsFastWhenTheRocketTreeChanges)
{
    Rocket      rocket;
    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    stage.addChild(std::make_unique<BodyTube>());
    rocket.enableEvents();

    auto it = rocket.subtree().begin();
    ++it;
    stage.addChild(std::make_unique<BodyTube>());  // a tree change
    EXPECT_THROW(++it, BugError);
}

TEST(RocketComponentIteration, FailsFastOnADetachedTree)
{
    // No rocket, so no tree modification id: the child lists are checked.
    TestComponent  root;
    TestComponent& a = root.addChild(TestComponent::make());
    a.addChild(TestComponent::make());
    root.addChild(TestComponent::make());

    auto it = root.subtree().begin();
    ++it;
    ASSERT_EQ(&*it, &a);
    static_cast<void>(root.removeChild(&a));  // destroys the current component
    EXPECT_THROW(++it, BugError);
    EXPECT_THROW(static_cast<void>(*it), BugError);

    // A change to the children of the current component is caught too.
    auto again = root.subtree().begin();
    root.addChild(TestComponent::make());
    EXPECT_THROW(++again, BugError);
}

TEST(RocketComponentIteration, FailsFastWithEventsDisabled)
{
    Rocket      rocket;  // events disabled: the tree modification id stays
    AxialStage& stage  = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&   body   = stage.addChild(std::make_unique<BodyTube>());
    const ModId treeId = rocket.getTreeModId();

    auto it = rocket.subtree().begin();
    ++it;
    ++it;
    ASSERT_EQ(&*it, &body);
    stage.addChild(std::make_unique<BodyTube>());
    EXPECT_EQ(rocket.getTreeModId(), treeId);
    EXPECT_THROW(++it, BugError);

    // Moving a child counts as a change as well.
    auto moved = rocket.subtree().begin();
    ++moved;
    ++moved;
    stage.moveChild(&body, 1);
    EXPECT_THROW(static_cast<void>(*moved), BugError);
}

TEST(RocketComponentIteration, FailsFastAcrossLoadFrom)
{
    Rocket      rocket;
    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    stage.addChild(std::make_unique<BodyTube>(0.1, 0.025));
    rocket.enableEvents();
    const std::unique_ptr<Rocket> copy = rocket.copyRocketWithOriginalId();

    auto it = rocket.subtree().begin();
    ++it;  // at the stage, which loadFrom() replaces by a copy
    rocket.loadFrom(*copy);
    // An undo restores the tree modification id the iterator saw.
    EXPECT_EQ(rocket.getTreeModId(), copy->getTreeModId());
    EXPECT_THROW(++it, BugError);
}

// ---- Properties ----

TEST(RocketComponentProperties, DefaultValues)
{
    const TestComponent component;
    EXPECT_EQ(component.getName(), "Mass Component");
    EXPECT_EQ(component.toString(), "Mass Component");
    EXPECT_EQ(component.getComponentName(), "Mass Component");
    EXPECT_EQ(component.getComment(), "");
    EXPECT_FALSE(component.getColor().has_value());
    EXPECT_FALSE(component.getLineStyle().has_value());
    EXPECT_FALSE(component.getAppearance().has_value());
    EXPECT_TRUE(component.isVisible());
    EXPECT_EQ(component.getDisplayOrderSide(), 100);
    EXPECT_EQ(component.getDisplayOrderBack(), 100);
    EXPECT_EQ(component.getAxialMethod(), AxialMethod::AFTER);
    EXPECT_EQ(component.getAxialOffset(), 0.0);
    EXPECT_EQ(component.getPresetComponent(), nullptr);
    EXPECT_FALSE(component.isMotorMount());
    EXPECT_TRUE(component.isAxisymmetric());
    EXPECT_TRUE(component.getAllMaterials().empty());
    EXPECT_FALSE(component.isBypassComponentChangeEvent());
    EXPECT_EQ(component.getId().version(), 4);
    EXPECT_NE(component.getId(), TestComponent{}.getId());
}

TEST(RocketComponentProperties, Names)
{
    TestComponent component;
    component.setName("Main tube");
    EXPECT_EQ(component.getName(), "Main tube");
    EXPECT_EQ(component.toString(), "Main tube");
    // A blank name restores the component name (Java's \s: space, tab, newline, vertical tab,
    // form feed, carriage return).
    component.setName(" \t\n\x0B\f\r");
    EXPECT_EQ(component.getName(), "Mass Component");
    component.setName("x");
    component.setName("");
    EXPECT_EQ(component.getName(), "Mass Component");
    // Other characters are kept.
    component.setName("\xC2\xA0");
    EXPECT_EQ(component.getName(), "\xC2\xA0");

    // The default name is the component name of the kind.
    const TestComponent chute{ComponentKind::PARACHUTE};
    EXPECT_EQ(chute.getName(), "Parachute");
    EXPECT_EQ(NoseCone{}.getName(), "Nose Cone");
    EXPECT_EQ(BodyTube{}.getName(), "Body Tube");
    EXPECT_EQ(Rocket{}.getName(), "Rocket");
    EXPECT_EQ(AxialStage{}.getName(), "Stage");
}

TEST(RocketComponentProperties, Ids)
{
    TestComponent component;
    const Uuid    id{0x123e4567e89b12d3ULL, 0xa456426614174000ULL};
    component.setId(id);
    EXPECT_EQ(component.getId(), id);
    EXPECT_EQ(component.getDebugName(), "Mass Component/123e4567");
    EXPECT_EQ(component.toDebugName(), "Mass Component<MassComponent>(123e4567)");

    ASSERT_TRUE(component.setId("00000000-0000-0000-0000-000000000001").has_value());
    EXPECT_EQ(component.getId(), (Uuid{0U, 1U}));
    const auto failed = component.setId("not a uuid");
    EXPECT_FALSE(failed.has_value());
    EXPECT_EQ(component.getId(), (Uuid{0U, 1U}));
    EXPECT_EQ(component.hashCode(), (Uuid{0U, 1U}).hashCode());

    // java.util.UUID.fromString() takes shortened groups, and so does setId().
    ASSERT_TRUE(component.setId("1-2-3-4-5").has_value());
    EXPECT_EQ(component.getId().toString(), "00000001-0002-0003-0004-000000000005");
    EXPECT_FALSE(component.setId("1-2-3-4").has_value());
    EXPECT_EQ(component.getId().toString(), "00000001-0002-0003-0004-000000000005");
}

TEST(RocketComponentProperties, EqualityIsClassAndId)
{
    TestComponent a;
    TestComponent b;
    EXPECT_TRUE(a.equals(a));
    EXPECT_FALSE(a.equals(b));
    b.setId(a.getId());
    EXPECT_TRUE(a.equals(b));
    AxialStage stage;
    stage.setId(a.getId());
    EXPECT_FALSE(a.equals(stage));
}

/// A rocket with one stage and one body tube holding a generic component (m_body, the component
/// the tests drive), events enabled, recording the event types.
class PropertyEventsTest : public ::testing::Test
{
protected:
    PropertyEventsTest()
    {
        m_stage = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_tube  = &m_stage->addChild(std::make_unique<BodyTube>(0.5, 0.025));
        m_body  = &m_tube->addChild(TestComponent::make(0.1));
        m_rocket.enableEvents();
        m_connection = m_rocket.addComponentChangeListener(
            [this](const ComponentChangeEvent& e) { m_types.push_back(e.getType()); });
    }

    Rocket                                  m_rocket;
    AxialStage*                             m_stage{nullptr};
    BodyTube*                               m_tube{nullptr};
    TestComponent*                          m_body{nullptr};
    std::vector<int>                        m_types;
    ComponentChangeSignal::ScopedConnection m_connection;
};

TEST_F(PropertyEventsTest, VisualSettersFireOnlyOnChange)
{
    m_body->setColor(Color{1, 2, 3});
    m_body->setColor(Color{1, 2, 3});
    m_body->setColor(std::nullopt);
    m_body->setColor(std::nullopt);
    m_body->setLineStyle(LineStyle::DASHED);
    m_body->setLineStyle(LineStyle::DASHED);
    m_body->setComment("hello");
    m_body->setComment("hello");
    m_body->setName("Body");
    m_body->setName("Body");
    EXPECT_EQ(m_types, std::vector<int>(5, ComponentChangeEvent::kNonFunctionalChange));
}

TEST_F(PropertyEventsTest, VisibilityAndAppearanceFireAlways)
{
    m_body->setVisible(false);
    m_body->setVisible(false);  // fires every time, as in Java
    EXPECT_EQ(m_types, std::vector<int>(2, ComponentChangeEvent::kGraphicChange));
    EXPECT_FALSE(m_body->isVisible());

    m_types.clear();
    m_body->setAppearance(Appearance{Color{9, 9, 9}, 0.5});
    m_body->setAppearance(std::nullopt);
    EXPECT_EQ(m_types, std::vector<int>(2, ComponentChangeEvent::kNonFunctionalChange));
}

TEST_F(PropertyEventsTest, AStageNameIsATreeChange)
{
    // A stage's name is part of the tree display.
    m_stage->setName("Sustainer");
    EXPECT_EQ(m_types, std::vector<int>{ComponentChangeEvent::kTreeChange});
}

TEST_F(PropertyEventsTest, DisplayOrdersFireNothing)
{
    m_body->setDisplayOrderSide(3);
    m_body->setDisplayOrderBack(4);
    EXPECT_TRUE(m_types.empty());
    EXPECT_EQ(m_body->getDisplayOrderSide(), 3);
    EXPECT_EQ(m_body->getDisplayOrderBack(), 4);
}

TEST(RocketComponentProperties, ClearPresetWithoutAPresetDoesNothing)
{
    TestComponent component;
    component.clearPreset();
    component.setIgnorePresetClearing(true);
    component.clearPreset();
    EXPECT_EQ(component.getPresetComponent(), nullptr);
}

// ---- Presets ----

/// A body tube preset of @p length made by the factory, part number @p partNo.
[[nodiscard]] ComponentPreset bodyTubePreset(double length, const std::string& partNo)
{
    TypedPropertyMap props;
    props.put(ComponentPreset::kType, ComponentPresetType::BODY_TUBE);
    props.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("Estes"));
    props.put(ComponentPreset::kPartNo, partNo);
    props.put(ComponentPreset::kLength, length);
    props.put(ComponentPreset::kOuterDiameter, 0.0247);
    props.put(ComponentPreset::kInnerDiameter, 0.0237);
    const QtRocket::MaterialStorage materials;
    return ComponentPresetFactory::create(props, materials).value();
}

TEST_F(PropertyEventsTest, LoadPresetCopiesTheLengthAndFiresOnce)
{
    const ComponentPreset preset = bodyTubePreset(0.3, "BT-20");
    m_body->loadPreset(&preset);
    EXPECT_EQ(m_body->getLength(), 0.3);
    EXPECT_EQ(m_body->getPresetComponent(), &preset);
    EXPECT_FALSE(m_rocket.isFrozen());
    // The base loadFromPreset() assigns the field, so only loadPreset()'s own event fires.
    EXPECT_EQ(m_types, std::vector<int>{ComponentChangeEvent::kNonFunctionalChange});

    // The same preset again: nothing happens.
    m_body->setLength(0.1);
    m_types.clear();
    m_body->loadPreset(&preset);
    EXPECT_EQ(m_body->getLength(), 0.1);
    EXPECT_TRUE(m_types.empty());

    // Another preset replaces it.
    const ComponentPreset longer = bodyTubePreset(0.5, "BT-20L");
    m_body->loadPreset(&longer);
    EXPECT_EQ(m_body->getLength(), 0.5);
    EXPECT_EQ(m_body->getPresetComponent(), &longer);
    EXPECT_EQ(m_types, std::vector<int>{ComponentChangeEvent::kNonFunctionalChange});

    // nullptr clears the preset (clearPreset()), keeping the values.
    m_types.clear();
    m_body->loadPreset(nullptr);
    EXPECT_EQ(m_body->getPresetComponent(), nullptr);
    EXPECT_EQ(m_body->getLength(), 0.5);
    EXPECT_EQ(m_types, std::vector<int>{ComponentChangeEvent::kNonFunctionalChange});
    m_types.clear();
    m_body->loadPreset(nullptr);
    EXPECT_TRUE(m_types.empty());
}

TEST_F(PropertyEventsTest, LoadPresetCombinesTheChangesOfLoadFromPreset)
{
    // The rocket is frozen while loadFromPreset() runs: its events come out as one on the thaw,
    // before NONFUNCTIONAL_CHANGE.
    const ComponentPreset preset = bodyTubePreset(0.3, "BT-20");
    m_body->setOnLoadFromPreset([this](const ComponentPreset& /*preset*/,
                                       const RocketComponent::PresetLoadOptions& /*options*/) {
        EXPECT_TRUE(m_rocket.isFrozen());
        m_body->setMass(0.02);
        m_body->setOuterRadius(0.012);
    });
    m_body->loadPreset(&preset);
    EXPECT_EQ(m_types, (std::vector<int>{ComponentChangeEvent::kMassChange |
                                             ComponentChangeEvent::kAeromassChange,
                                         ComponentChangeEvent::kNonFunctionalChange}));
    EXPECT_EQ(m_body->getComponentMass(), 0.02);
}

/// Loads @p preset into @p component through a loadFromPreset() that changes the mass and then
/// throws; true when loadPreset() let the exception through (a helper, for clang-tidy's
/// cognitive complexity limit on the test body).
[[nodiscard]] bool loadFails(TestComponent& component, const ComponentPreset& preset)
{
    component.setOnLoadFromPreset(
        [&component](const ComponentPreset& /*preset*/,
                     const RocketComponent::PresetLoadOptions& /*options*/) {
            component.setMass(0.02);
            throw std::runtime_error("load failed");
        });
    try
    {
        component.loadPreset(&preset);
    }
    catch (const std::runtime_error&)
    {
        return true;
    }
    return false;
}

TEST_F(PropertyEventsTest, AFailedLoadKeepsTheOldPresetAndThaws)
{
    const ComponentPreset first = bodyTubePreset(0.3, "BT-20");
    m_body->loadPreset(&first);
    m_types.clear();

    const ComponentPreset second = bodyTubePreset(0.4, "BT-50");
    EXPECT_TRUE(loadFails(*m_body, second));
    EXPECT_FALSE(m_rocket.isFrozen());
    EXPECT_EQ(m_body->getPresetComponent(), &first);
    // The base version had already copied the length (as in Java); the queued change fired on
    // the thaw, and no NONFUNCTIONAL_CHANGE.
    EXPECT_EQ(m_body->getLength(), 0.4);
    EXPECT_EQ(m_types, std::vector<int>{ComponentChangeEvent::kMassChange});
}

TEST_F(PropertyEventsTest, LoadPresetInAFrozenRocketIsABug)
{
    // Deviation: Java reports the second freeze() through the error handler and carries on.
    const ComponentPreset preset = bodyTubePreset(0.3, "BT-20");
    m_rocket.freeze();
    EXPECT_THROW(m_body->loadPreset(&preset), BugError);
    EXPECT_EQ(m_body->getPresetComponent(), nullptr);
    m_rocket.thaw();
}

TEST(RocketComponentPresets, LoadPresetOfADetachedComponent)
{
    // Without a Rocket at the root nothing is frozen and no event goes anywhere.
    const ComponentPreset preset = bodyTubePreset(0.3, "BT-20");
    TestComponent         component;
    component.loadPreset(&preset);
    EXPECT_EQ(component.getLength(), 0.3);
    EXPECT_EQ(component.getPresetComponent(), &preset);
}

TEST(RocketComponentPresets, APresetWithoutALengthKeepsTheLength)
{
    TypedPropertyMap props;
    props.put(ComponentPreset::kType, ComponentPresetType::PARACHUTE);
    props.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("Rocketman"));
    props.put(ComponentPreset::kPartNo, "R4C");
    props.put(ComponentPreset::kDiameter, 1.2192);
    props.put(ComponentPreset::kLineCount, 8);
    props.put(ComponentPreset::kLineLength, 1.2);
    const QtRocket::MaterialStorage materials;
    const ComponentPreset parachute = ComponentPresetFactory::create(props, materials).value();

    TestComponent component(ComponentKind::PARACHUTE, AxialMethod::AFTER, 0.07);
    component.loadPreset(&parachute);
    EXPECT_EQ(component.getLength(), 0.07);
    EXPECT_EQ(component.getPresetComponent(), &parachute);
}

TEST(RocketComponentPresets, OptionsReachLoadFromPreset)
{
    const ComponentPreset            first  = bodyTubePreset(0.3, "BT-20");
    const ComponentPreset            second = bodyTubePreset(0.4, "BT-50");
    std::vector<std::optional<bool>> seen;
    TestComponent                    component(ComponentKind::PARACHUTE);
    component.setOnLoadFromPreset([&seen](const ComponentPreset& /*preset*/,
                                          const RocketComponent::PresetLoadOptions& options) {
        seen.push_back(options.allowAutoRadius);
    });
    component.loadPreset(&first);
    component.loadPreset(&second, {.allowAutoRadius = false});
    EXPECT_EQ(seen, (std::vector<std::optional<bool>>{std::nullopt, false}));
}

TEST(RocketComponentPresets, OptionsAreForParachutesOnly)
{
    // Java passes params to a parachute only; for another component they would skip the
    // overrides of the one-argument loadFromPreset(), which the single C++ method cannot.
    const ComponentPreset preset = bodyTubePreset(0.3, "BT-20");
    TestComponent         component(ComponentKind::INNER_TUBE);
    EXPECT_THROW(component.loadPreset(&preset, {.allowAutoRadius = true}), BugError);
    EXPECT_EQ(component.getPresetComponent(), nullptr);
    component.loadPreset(&preset, {});
    EXPECT_EQ(component.getPresetComponent(), &preset);
}

TEST(RocketComponentPresets, PresetTypeFollowsTheKind)
{
    // RocketComponent.getPresetType() and the overrides of the concrete classes: the base
    // answer follows the kind, and the real classes give the same.
    EXPECT_EQ(TestComponent(ComponentKind::INNER_TUBE).getPresetType(),
              ComponentPresetType::BODY_TUBE);
    EXPECT_EQ(TestComponent(ComponentKind::BULKHEAD).getPresetType(),
              ComponentPresetType::BULK_HEAD);
    EXPECT_EQ(TestComponent(ComponentKind::MASS_COMPONENT).getPresetType(), std::nullopt);
    EXPECT_EQ(BodyTube().getPresetType(), ComponentPresetType::BODY_TUBE);
    EXPECT_EQ(NoseCone().getPresetType(), ComponentPresetType::NOSE_CONE);
    EXPECT_EQ(QtRocket::Transition().getPresetType(), ComponentPresetType::TRANSITION);
    EXPECT_EQ(QtRocket::InnerTube().getPresetType(), ComponentPresetType::BODY_TUBE);
    EXPECT_EQ(QtRocket::Bulkhead().getPresetType(), ComponentPresetType::BULK_HEAD);
    EXPECT_EQ(QtRocket::MassComponent().getPresetType(), std::nullopt);
    EXPECT_EQ(Rocket().getPresetType(), std::nullopt);
    EXPECT_EQ(AxialStage().getPresetType(), std::nullopt);
}

// ---- Who owns a preset ----
//
// OpenRocket has no counterpart: its garbage collector keeps a preset while a component refers
// to it. Here the component co-owns the preset that loadPreset(std::shared_ptr) gives it, and
// only points at the one loadPreset(const ComponentPreset*) gives it.

using SharedPreset = std::shared_ptr<const ComponentPreset>;

/// bodyTubePreset() as a shared preset, the way a ComponentPresetDatabase holds one.
[[nodiscard]] SharedPreset sharedBodyTubePreset(double length, const std::string& partNo)
{
    return std::make_shared<const ComponentPreset>(bodyTubePreset(length, partNo));
}

TEST(RocketComponentPresetOwnership, TheOwningFormKeepsThePresetAlive)
{
    SharedPreset                               preset = sharedBodyTubePreset(0.3, "BT-20");
    const std::weak_ptr<const ComponentPreset> watch  = preset;
    const ComponentPreset* const               object = preset.get();
    TestComponent                              component;

    component.loadPreset(preset);
    EXPECT_EQ(component.getLength(), 0.3);
    EXPECT_EQ(component.getPresetComponent(), object);
    EXPECT_EQ(preset.use_count(), 2);

    // The caller lets go: the component still has the preset.
    preset.reset();
    EXPECT_FALSE(watch.expired());
    ASSERT_EQ(component.getPresetComponent(), object);
    EXPECT_EQ(component.getPresetComponent()->getPartNo(), "BT-20");
    EXPECT_EQ(component.getPresetComponent()->get(ComponentPreset::kLength), 0.3);

    // Clearing the preset gives up the share.
    component.clearPreset();
    EXPECT_EQ(component.getPresetComponent(), nullptr);
    EXPECT_TRUE(watch.expired());
}

TEST(RocketComponentPresetOwnership, TheRawPointerFormOnlyPointsAtThePreset)
{
    const SharedPreset preset = sharedBodyTubePreset(0.3, "BT-20");
    TestComponent      component;

    component.loadPreset(preset.get());
    EXPECT_EQ(component.getLength(), 0.3);
    EXPECT_EQ(component.getPresetComponent(), preset.get());
    // No share was taken, nor by a copy of the component.
    EXPECT_EQ(preset.use_count(), 1);
    const std::unique_ptr<RocketComponent> copy = component.copyWithOriginalId();
    EXPECT_EQ(copy->getPresetComponent(), preset.get());
    EXPECT_EQ(preset.use_count(), 1);
}

TEST(RocketComponentPresetOwnership, APresetIsReleasedWhenAnotherReplacesIt)
{
    SharedPreset                               first  = sharedBodyTubePreset(0.3, "BT-20");
    const SharedPreset                         second = sharedBodyTubePreset(0.4, "BT-50");
    const std::weak_ptr<const ComponentPreset> watch  = first;
    TestComponent                              component;

    component.loadPreset(first);
    first.reset();
    EXPECT_FALSE(watch.expired());

    component.loadPreset(second);
    EXPECT_TRUE(watch.expired());
    EXPECT_EQ(component.getPresetComponent(), second.get());
    EXPECT_EQ(second.use_count(), 2);

    // A null preset clears, as nullptr does in the raw form.
    component.loadPreset(SharedPreset{});
    EXPECT_EQ(component.getPresetComponent(), nullptr);
    EXPECT_EQ(second.use_count(), 1);
}

TEST(RocketComponentPresetOwnership, TheComponentIsReleasedWithItsShare)
{
    const SharedPreset preset = sharedBodyTubePreset(0.3, "BT-20");
    {
        TestComponent component;
        component.loadPreset(preset);
        EXPECT_EQ(preset.use_count(), 2);
    }
    EXPECT_EQ(preset.use_count(), 1);
}

TEST_F(PropertyEventsTest, TheSameOwnedPresetAgainDoesNothing)
{
    const SharedPreset preset = sharedBodyTubePreset(0.3, "BT-20");
    m_body->loadPreset(preset);
    EXPECT_EQ(m_types, std::vector<int>{ComponentChangeEvent::kNonFunctionalChange});
    EXPECT_EQ(preset.use_count(), 2);

    m_body->setLength(0.1);
    m_types.clear();
    m_body->loadPreset(preset);
    EXPECT_EQ(m_body->getLength(), 0.1);
    EXPECT_TRUE(m_types.empty());
    EXPECT_EQ(preset.use_count(), 2);

    // Nor does the raw pointer of the preset the component co-owns: the share stays.
    m_body->loadPreset(preset.get());
    EXPECT_EQ(m_body->getLength(), 0.1);
    EXPECT_TRUE(m_types.empty());
    EXPECT_EQ(preset.use_count(), 2);
}

TEST_F(PropertyEventsTest, AShareOfferedForAPresetOnlyPointedAtIsTaken)
{
    SharedPreset                               preset = sharedBodyTubePreset(0.3, "BT-20");
    const std::weak_ptr<const ComponentPreset> watch  = preset;
    m_body->loadPreset(preset.get());
    EXPECT_EQ(preset.use_count(), 1);
    m_body->setLength(0.1);
    m_types.clear();

    // The same preset: nothing is loaded and nothing fires, but the component owns it now.
    m_body->loadPreset(preset);
    EXPECT_EQ(preset.use_count(), 2);
    EXPECT_EQ(m_body->getLength(), 0.1);
    EXPECT_TRUE(m_types.empty());

    preset.reset();
    EXPECT_FALSE(watch.expired());
    ASSERT_NE(m_body->getPresetComponent(), nullptr);
    EXPECT_EQ(m_body->getPresetComponent()->getPartNo(), "BT-20");
}

TEST_F(PropertyEventsTest, ClearingIsIgnoredForAnOwnedPresetToo)
{
    SharedPreset                               preset = sharedBodyTubePreset(0.3, "BT-20");
    const std::weak_ptr<const ComponentPreset> watch  = preset;
    m_body->loadPreset(std::move(preset));
    m_types.clear();

    // What the .ork loader does around the elements of a component: its setters must not clear
    // the preset the file names.
    m_body->setIgnorePresetClearing(true);
    m_body->clearPreset();
    EXPECT_FALSE(watch.expired());
    EXPECT_NE(m_body->getPresetComponent(), nullptr);
    EXPECT_TRUE(m_types.empty());

    m_body->setIgnorePresetClearing(false);
    m_body->clearPreset();
    EXPECT_TRUE(watch.expired());
    EXPECT_EQ(m_body->getPresetComponent(), nullptr);
    EXPECT_EQ(m_types, std::vector<int>{ComponentChangeEvent::kNonFunctionalChange});
}

/// loadFails() with the owning form.
[[nodiscard]] bool owningLoadFails(TestComponent& component, SharedPreset preset)
{
    component.setOnLoadFromPreset(
        [&component](const ComponentPreset& /*preset*/,
                     const RocketComponent::PresetLoadOptions& /*options*/) {
            component.setMass(0.02);
            throw std::runtime_error("load failed");
        });
    try
    {
        component.loadPreset(std::move(preset));
    }
    catch (const std::runtime_error&)
    {
        return true;
    }
    return false;
}

TEST_F(PropertyEventsTest, AFailedOwningLoadKeepsTheOldPresetAndNoShareOfTheNew)
{
    const SharedPreset first = sharedBodyTubePreset(0.3, "BT-20");
    m_body->loadPreset(first);
    m_types.clear();

    const SharedPreset second = sharedBodyTubePreset(0.4, "BT-50");
    EXPECT_TRUE(owningLoadFails(*m_body, second));
    EXPECT_FALSE(m_rocket.isFrozen());
    EXPECT_EQ(m_body->getPresetComponent(), first.get());
    EXPECT_EQ(first.use_count(), 2);
    EXPECT_EQ(second.use_count(), 1);
    EXPECT_EQ(m_types, std::vector<int>{ComponentChangeEvent::kMassChange});
}

TEST_F(PropertyEventsTest, AnOwningLoadInAFrozenRocketIsABugAndTakesNoShare)
{
    const SharedPreset preset = sharedBodyTubePreset(0.3, "BT-20");
    m_rocket.freeze();
    EXPECT_THROW(m_body->loadPreset(preset), BugError);
    EXPECT_EQ(m_body->getPresetComponent(), nullptr);
    EXPECT_EQ(preset.use_count(), 1);
    m_rocket.thaw();
}

TEST(RocketComponentPresetOwnership, CopiesOfAComponentShareThePreset)
{
    SharedPreset                               preset   = sharedBodyTubePreset(0.3, "BT-20");
    const std::weak_ptr<const ComponentPreset> watch    = preset;
    const ComponentPreset* const               object   = preset.get();
    auto                                       original = std::make_unique<TestComponent>();
    original->loadPreset(std::move(preset));

    std::unique_ptr<RocketComponent> sameIds = original->copyWithOriginalId();
    std::unique_ptr<RocketComponent> newIds  = original->copyWithNewIds();
    EXPECT_EQ(watch.use_count(), 3);
    EXPECT_EQ(sameIds->getPresetComponent(), object);
    EXPECT_EQ(newIds->getPresetComponent(), object);

    // Each copy holds the preset by itself.
    original.reset();
    sameIds.reset();
    EXPECT_FALSE(watch.expired());
    ASSERT_EQ(newIds->getPresetComponent(), object);
    EXPECT_EQ(newIds->getPresetComponent()->getPartNo(), "BT-20");
    newIds.reset();
    EXPECT_TRUE(watch.expired());
}

/// A rocket whose body tube is based on a preset of a database that is gone again; with the
/// tube's id and the preset's digest, both as they were while the database lived.
struct RocketWithPreset
{
    std::unique_ptr<Rocket>              rocket;
    QtRocket::Uuid                       tubeId;
    std::string                          digest;
    std::weak_ptr<const ComponentPreset> watch;
};

[[nodiscard]] RocketWithPreset makeRocketWithPreset()
{
    RocketWithPreset made{.rocket = std::make_unique<Rocket>(),
                          .tubeId = QtRocket::Uuid::random(),
                          .digest = {},
                          .watch  = {}};
    auto&            stage = made.rocket->addChild(std::make_unique<AxialStage>());
    auto&            tube  = stage.addChild(std::make_unique<BodyTube>(0.5, 0.025));
    made.rocket->enableEvents();
    made.tubeId = tube.getId();
    {
        // The database is the only other owner, and it goes first.
        QtRocket::ComponentPresetDatabase database;
        database.add(sharedBodyTubePreset(0.3, "BT-20"));
        const SharedPreset found = database.find("ESTES", "BT-20").at(0);
        made.digest              = found->getDigest();
        made.watch               = found;
        tube.loadPreset(found);
    }
    return made;
}

/// The preset of the component @p id of @p rocket, or nullptr.
[[nodiscard]] const ComponentPreset* presetOf(const Rocket& rocket, const QtRocket::Uuid& id)
{
    const RocketComponent* component = rocket.findComponent(id);
    return component != nullptr ? component->getPresetComponent() : nullptr;
}

TEST(RocketComponentPresetOwnership, ACopyOfTheRocketKeepsThePresetAfterTheDatabaseAndTheRocket)
{
    // The lifetimes of the product: an undo snapshot, a simulation's rocket and the rocket a
    // FlightData co-owns are copies that may be read when the preset database, and even the
    // document's rocket, are gone. The asan preset checks that the reads below are valid.
    RocketWithPreset made = makeRocketWithPreset();
    EXPECT_FALSE(made.watch.expired());

    std::unique_ptr<Rocket>             snapshot  = made.rocket->copyRocketWithOriginalId();
    const std::shared_ptr<const Rocket> simulated = made.rocket->copyRocketWithOriginalId();
    made.rocket.reset();
    EXPECT_FALSE(made.watch.expired());

    const ComponentPreset* preset = presetOf(*snapshot, made.tubeId);
    ASSERT_NE(preset, nullptr);
    EXPECT_EQ(preset->getDigest(), made.digest);
    EXPECT_EQ(preset->getPartNo(), "BT-20");
    EXPECT_EQ(preset->get(ComponentPreset::kLength), 0.3);
    EXPECT_EQ(preset->toOrkElement(),
              R"(<preset type="BODY_TUBE" manufacturer="Estes" partno="BT-20" digest=")" +
                  made.digest + R"("/>)");

    // The snapshot gone too: the last holder still reads the same preset.
    snapshot.reset();
    EXPECT_FALSE(made.watch.expired());
    const ComponentPreset* last = presetOf(*simulated, made.tubeId);
    ASSERT_EQ(last, preset);
    EXPECT_EQ(last->getDigest(), made.digest);
}

TEST(RocketComponentPresetOwnership, ThePresetGoesWithItsLastHolder)
{
    RocketWithPreset        made = makeRocketWithPreset();
    std::unique_ptr<Rocket> copy = made.rocket->copyRocketWithOriginalId();
    made.rocket.reset();
    EXPECT_FALSE(made.watch.expired());
    copy.reset();
    EXPECT_TRUE(made.watch.expired());
}

TEST(RocketComponentPresetOwnership, LoadFromSharesThePresetsOfTheLoadedComponents)
{
    // Rocket::loadFrom() (undo) replaces the children by copies of the source's, which share the
    // presets; the snapshot may go afterwards.
    RocketWithPreset made = makeRocketWithPreset();
    Rocket           target;
    target.loadFrom(*made.rocket);
    made.rocket.reset();
    EXPECT_FALSE(made.watch.expired());

    const ComponentPreset* preset = presetOf(target, made.tubeId);
    ASSERT_NE(preset, nullptr);
    EXPECT_EQ(preset->getDigest(), made.digest);
}

// ---- Mass, CG and overrides ----

TEST(RocketComponentMass, MassAndCGWithoutOverrides)
{
    TestComponent component;
    component.setMass(0.2);
    component.setCG(Coordinate{0.05, 0, 0});
    component.setUnitInertias(0.003, 0.0005);
    EXPECT_EQ(component.getMass(), 0.2);
    EXPECT_EQ(component.getCG(), (Coordinate{0.05, 0, 0, 0.2}));
    EXPECT_DOUBLE_EQ(component.getLongitudinalInertia(), 0.003 * 0.2);
    EXPECT_DOUBLE_EQ(component.getRotationalInertia(), 0.0005 * 0.2);
    // While not overridden, the override values follow the component.
    EXPECT_EQ(component.getOverrideMass(), 0.2);
    EXPECT_EQ(component.getOverrideCGX(), 0.05);
    EXPECT_EQ(component.getOverrideCG(), (Coordinate{0.05, 0, 0, 0.2}));
}

TEST(RocketComponentMass, OverridesReplaceMassAndCGSeparately)
{
    TestComponent component;
    component.setMass(0.2);
    component.setCG(Coordinate{0.05, 0, 0});

    component.setMassOverridden(true);
    component.setOverrideMass(0.5);
    EXPECT_EQ(component.getMass(), 0.5);
    EXPECT_EQ(component.getCG(), (Coordinate{0.05, 0, 0, 0.5}));
    EXPECT_DOUBLE_EQ(component.getLongitudinalInertia(), 0.0);

    component.setCGOverridden(true);
    component.setOverrideCGX(0.08);
    EXPECT_EQ(component.getCG(), (Coordinate{0.08, 0, 0, 0.5}));
    EXPECT_EQ(component.getOverrideCG(), (Coordinate{0.08, 0, 0, 0.2}));

    // Negative override masses become 0; switching the override off restores the component
    // values.
    component.setOverrideMass(-1.0);
    EXPECT_EQ(component.getOverrideMass(), 0.0);
    component.setMassOverridden(false);
    EXPECT_EQ(component.getOverrideMass(), 0.2);
    component.setCGOverridden(false);
    EXPECT_EQ(component.getOverrideCGX(), 0.05);
    EXPECT_EQ(component.getCG(), (Coordinate{0.05, 0, 0, 0.2}));
}

TEST(RocketComponentMass, SectionMassStopsAtASubcomponentOverride)
{
    TestComponent  parent;
    TestComponent& child      = parent.addChild(TestComponent::make());
    TestComponent& grandchild = child.addChild(TestComponent::make());
    parent.setMass(1.0);
    child.setMass(2.0);
    grandchild.setMass(4.0);
    EXPECT_EQ(parent.getSectionMass(), 7.0);

    child.setMassOverridden(true);
    child.setOverrideMass(3.0);
    EXPECT_EQ(parent.getSectionMass(), 8.0);  // 1 + 3 + 4
    child.setSubcomponentsOverriddenMass(true);
    EXPECT_EQ(parent.getSectionMass(), 4.0);  // 1 + 3
}

TEST(RocketComponentMass, OverriddenByFollowsTheSubcomponentFlags)
{
    TestComponent  root;
    TestComponent& parent = root.addChild(TestComponent::make());
    TestComponent& child  = parent.addChild(TestComponent::make());
    TestComponent& leaf   = child.addChild(TestComponent::make());

    parent.setMassOverridden(true);
    EXPECT_EQ(child.getMassOverriddenBy(), nullptr) << "the subcomponents flag is off";
    parent.setSubcomponentsOverriddenMass(true);
    EXPECT_EQ(child.getMassOverriddenBy(), &parent);
    EXPECT_EQ(leaf.getMassOverriddenBy(), &parent);
    EXPECT_EQ(parent.getMassOverriddenBy(), nullptr);

    parent.setCGOverridden(true);
    parent.setSubcomponentsOverriddenCG(true);
    EXPECT_EQ(leaf.getCGOverriddenBy(), &parent);

    parent.setCDOverridden(true);
    parent.setSubcomponentsOverriddenCD(true);
    EXPECT_EQ(leaf.getCDOverriddenBy(), &parent);
    EXPECT_TRUE(leaf.isCDOverriddenByAncestor());
    EXPECT_TRUE(child.isCDOverriddenByAncestor());
    EXPECT_FALSE(parent.isCDOverriddenByAncestor());
    EXPECT_TRUE(parent.isOverrideSubcomponentsEnabled());
    EXPECT_FALSE(child.isOverrideSubcomponentsEnabled());

    // A child added below the overrider inherits it.
    TestComponent& added = child.addChild(TestComponent::make());
    EXPECT_EQ(added.getMassOverriddenBy(), &parent);
    EXPECT_EQ(added.getCGOverriddenBy(), &parent);
    EXPECT_EQ(added.getCDOverriddenBy(), &parent);

    // Removing a subtree clears the pointers that refer to the overrider.
    const std::unique_ptr<RocketComponent> removed = parent.removeChild(&child);
    EXPECT_EQ(child.getMassOverriddenBy(), nullptr);
    EXPECT_EQ(leaf.getCGOverriddenBy(), nullptr);
    EXPECT_EQ(added.getCDOverriddenBy(), nullptr);

    // Switching the flag off clears the children.
    TestComponent& other = parent.addChild(TestComponent::make());
    EXPECT_EQ(other.getMassOverriddenBy(), &parent);
    parent.setSubcomponentsOverriddenMass(false);
    EXPECT_EQ(other.getMassOverriddenBy(), nullptr);
    parent.setSubcomponentsOverriddenMass(true);
    parent.setMassOverridden(false);
    EXPECT_EQ(other.getMassOverriddenBy(), nullptr);
}

TEST(RocketComponentMass, OverriddenByKeepsOpenRocketsTreeOrderWalk)
{
    // OpenRocket's walk hands the overrider found in one child's subtree on to the later
    // siblings too; kept as it is.
    TestComponent  root;
    TestComponent& first  = root.addChild(TestComponent::make());
    TestComponent& inner  = first.addChild(TestComponent::make());
    TestComponent& second = root.addChild(TestComponent::make());
    first.setMassOverridden(true);
    first.setSubcomponentsOverriddenMass(true);
    EXPECT_EQ(inner.getMassOverriddenBy(), &first);
    root.setMassOverridden(true);  // walks root's descendants with no overrider of its own
    EXPECT_EQ(first.getMassOverriddenBy(), nullptr);
    EXPECT_EQ(inner.getMassOverriddenBy(), &first);
    EXPECT_EQ(second.getMassOverriddenBy(), &first);
}

TEST(RocketComponentMass, RemovingAnOverriderClearsThePointersLeftInTheTree)
{
    // The tree-order walk gives `second` (and its child) the earlier sibling's descendant
    // `overrider`; once `first` is removed and destroyed they must not refer to it (Java keeps a
    // stale reference there).
    TestComponent  root;
    TestComponent& first       = root.addChild(TestComponent::make());
    TestComponent& overrider   = first.addChild(TestComponent::make());
    TestComponent& inner       = overrider.addChild(TestComponent::make());
    TestComponent& second      = root.addChild(TestComponent::make());
    TestComponent& secondChild = second.addChild(TestComponent::make());
    overrider.setMassOverridden(true);
    overrider.setSubcomponentsOverriddenMass(true);
    overrider.setCGOverridden(true);
    overrider.setSubcomponentsOverriddenCG(true);
    overrider.setCDOverridden(true);
    overrider.setSubcomponentsOverriddenCD(true);
    root.setMassOverridden(true);
    root.setCGOverridden(true);
    root.setCDOverridden(true);
    ASSERT_EQ(second.getMassOverriddenBy(), &overrider);
    ASSERT_EQ(secondChild.getCGOverriddenBy(), &overrider);
    ASSERT_EQ(second.getCDOverriddenBy(), &overrider);

    std::unique_ptr<RocketComponent> removed = root.removeChild(&first);
    // Inside the removed subtree the overrider is kept, as in Java.
    EXPECT_EQ(inner.getMassOverriddenBy(), &overrider);
    EXPECT_EQ(inner.getCGOverriddenBy(), &overrider);
    EXPECT_EQ(inner.getCDOverriddenBy(), &overrider);
    // The tree it left forgets it.
    EXPECT_EQ(second.getMassOverriddenBy(), nullptr);
    EXPECT_EQ(second.getCGOverriddenBy(), nullptr);
    EXPECT_EQ(second.getCDOverriddenBy(), nullptr);
    EXPECT_EQ(secondChild.getMassOverriddenBy(), nullptr);
    EXPECT_EQ(secondChild.getCGOverriddenBy(), nullptr);
    EXPECT_EQ(secondChild.getCDOverriddenBy(), nullptr);
    removed.reset();
    EXPECT_EQ(root.getAllChildren(), (std::vector<RocketComponent*>{&second, &secondChild}));
}

TEST(RocketComponentMass, ARemovedSubtreeForgetsOverridersOutsideIt)
{
    // stage[bt1 (overrides its subcomponents){c}, bt2{bt2child}]: the walk gives bt2 and bt2child
    // bt1 as their overrider.
    TestComponent  stage;
    TestComponent& bt1 = stage.addChild(TestComponent::make());
    bt1.addChild(TestComponent::make());
    TestComponent& bt2      = stage.addChild(TestComponent::make());
    TestComponent& bt2child = bt2.addChild(TestComponent::make());
    bt1.setMassOverridden(true);
    bt1.setSubcomponentsOverriddenMass(true);
    stage.setMassOverridden(true);
    stage.setMassOverridden(false);
    ASSERT_EQ(bt2.getMassOverriddenBy(), &bt1);
    ASSERT_EQ(bt2child.getMassOverriddenBy(), &bt1);

    // bt2 is removed and kept (a drag and drop, the clipboard): its pointers to bt1 go.
    std::unique_ptr<RocketComponent> kept = stage.removeChild(&bt2);
    EXPECT_EQ(bt2.getMassOverriddenBy(), nullptr);
    EXPECT_EQ(bt2child.getMassOverriddenBy(), nullptr);

    // bt1 is destroyed and bt2 comes back: nothing refers to bt1.
    static_cast<void>(stage.removeChild(&bt1));
    stage.addChild(std::move(kept));
    EXPECT_EQ(bt2.getMassOverriddenBy(), nullptr);
    EXPECT_EQ(bt2child.getMassOverriddenBy(), nullptr);
    EXPECT_EQ(stage.getSectionMass(), 0.0);
}

TEST(RocketComponentMass, SetSubcomponentsOverriddenSetsAllThree)
{
    TestComponent component;
    component.setSubcomponentsOverridden(true);
    EXPECT_TRUE(component.isSubcomponentsOverriddenMass());
    EXPECT_TRUE(component.isSubcomponentsOverriddenCG());
    EXPECT_TRUE(component.isSubcomponentsOverriddenCD());
}

TEST(RocketComponentMass, OverrideEventsDependOnTheOverrideState)
{
    Rocket      rocket;
    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&   body  = stage.addChild(std::make_unique<BodyTube>(0.1, 0.025));
    rocket.enableEvents();
    std::vector<int> types;
    const auto       connection =
        ComponentChangeSignal::ScopedConnection{rocket.addComponentChangeListener(
            [&types](const ComponentChangeEvent& e) { types.push_back(e.getType()); })};

    body.setOverrideMass(0.3);  // not overridden: silent
    body.setOverrideCGX(0.1);   // not overridden: non-functional
    body.setOverrideCD(0.7);    // not overridden: non-functional
    EXPECT_EQ(types, (std::vector<int>{ComponentChangeEvent::kNonFunctionalChange,
                                       ComponentChangeEvent::kNonFunctionalChange}));

    types.clear();
    body.setMassOverridden(true);
    body.setOverrideMass(0.4);
    body.setCGOverridden(true);
    body.setOverrideCGX(0.2);
    body.setCDOverridden(true);
    body.setOverrideCD(0.8);
    EXPECT_EQ(types, (std::vector<int>{
                         ComponentChangeEvent::kMassChange, ComponentChangeEvent::kMassChange,
                         ComponentChangeEvent::kMassChange, ComponentChangeEvent::kMassChange,
                         ComponentChangeEvent::kAerodynamicChange,
                         ComponentChangeEvent::kAerodynamicChange}));
    EXPECT_EQ(body.getOverrideCD(), 0.8);

    types.clear();
    body.setOverrideMass(0.4 + 1e-12);  // equal within MathUtil.equals: nothing
    body.setMassOverridden(true);       // unchanged: nothing
    body.setSubcomponentsOverriddenMass(true);
    body.setSubcomponentsOverriddenCD(true);
    EXPECT_EQ(types, (std::vector<int>{ComponentChangeEvent::kMassChange |
                                           ComponentChangeEvent::kTreeChangeChildren,
                                       ComponentChangeEvent::kAerodynamicChange |
                                           ComponentChangeEvent::kTreeChangeChildren}));
}

// ---- Copies ----

TEST(RocketComponentCopy, CopyWithOriginalIdKeepsIdsAndFields)
{
    TestComponent  root{ComponentKind::MASS_COMPONENT, AxialMethod::TOP, 0.3};
    TestComponent& child = root.addChild(TestComponent::make(0.1));
    child.addChild(TestComponent::make(0.02));
    root.setName("Root");
    root.setComment("note");
    root.setColor(Color{1, 2, 3});
    root.setMass(0.7);
    root.setMassOverridden(true);
    root.setOverrideMass(1.5);
    root.setSubcomponentsOverriddenMass(true);
    root.getInsideColorComponentHandler().setSeparateInsideOutside(true);
    root.setBypassChangeEvent(true);

    const std::unique_ptr<RocketComponent> copy   = root.copyWithOriginalId();
    auto&                                  copied = dynamic_cast<TestComponent&>(*copy);
    EXPECT_NE(&copied, &root);
    EXPECT_EQ(copied.getParent(), nullptr);
    EXPECT_EQ(copied.getId(), root.getId());
    EXPECT_EQ(copied.getName(), "Root");
    EXPECT_EQ(copied.getComment(), "note");
    EXPECT_EQ(copied.getColor(), root.getColor());
    EXPECT_EQ(copied.getLength(), 0.3);
    EXPECT_EQ(copied.getAxialMethod(), AxialMethod::TOP);
    EXPECT_EQ(copied.getMass(), 1.5);
    EXPECT_TRUE(copied.getInsideColorComponentHandler().isSeparateInsideOutside());
    EXPECT_FALSE(copied.isBypassComponentChangeEvent()) << "Java's clone() clears the bypass";

    ASSERT_EQ(copied.getChildCount(), 1U);
    const RocketComponent& copiedChild = copied.getChild(0);
    EXPECT_NE(&copiedChild, &child);
    EXPECT_EQ(copiedChild.getId(), child.getId());
    EXPECT_EQ(copiedChild.getParent(), &copied);
    ASSERT_EQ(copiedChild.getChildCount(), 1U);
    EXPECT_EQ(copiedChild.getChild(0).getLength(), 0.02);
    EXPECT_NO_THROW(copied.checkComponentStructure());

    // The overriddenBy pointers point into the copy.
    EXPECT_EQ(copiedChild.getMassOverriddenBy(), &copied);
    EXPECT_EQ(copiedChild.getChild(0).getMassOverriddenBy(), &copied);

    // The original is untouched.
    EXPECT_EQ(child.getParent(), &root);
    EXPECT_EQ(child.getMassOverriddenBy(), &root);
}

TEST(RocketComponentCopy, OverriderOutsideTheCopyIsDropped)
{
    TestComponent  root;
    TestComponent& child = root.addChild(TestComponent::make());
    TestComponent& leaf  = child.addChild(TestComponent::make());
    root.setMassOverridden(true);
    root.setSubcomponentsOverriddenMass(true);
    ASSERT_EQ(leaf.getMassOverriddenBy(), &root);

    const std::unique_ptr<RocketComponent> copy = child.copyWithOriginalId();
    EXPECT_EQ(copy->getMassOverriddenBy(), nullptr);
    EXPECT_EQ(copy->getChild(0).getMassOverriddenBy(), nullptr);
}

TEST(RocketComponentCopy, CopyWithNewIdsChangesEveryId)
{
    TestComponent  root;
    TestComponent& child = root.addChild(TestComponent::make());

    const std::unique_ptr<RocketComponent> copy = root.copyWithNewIds();
    EXPECT_NE(copy->getId(), root.getId());
    EXPECT_NE(copy->getChild(0).getId(), child.getId());
    EXPECT_NE(copy->getId(), copy->getChild(0).getId());
    EXPECT_EQ(copy->getChildCount(), 1U);
}

TEST(RocketComponentCopy, CopyFromLoadsFieldsAndReturnsTheOldChildren)
{
    TestComponent  target;
    TestComponent& oldChild = target.addChild(TestComponent::make());

    TestComponent source{ComponentKind::MASS_COMPONENT, AxialMethod::MIDDLE, 0.4};
    source.setName("Source");
    source.addChild(TestComponent::make(0.1));
    source.getInsideColorComponentHandler().setEdgesSameAsInside(true);

    const std::vector<std::unique_ptr<RocketComponent>> previous = target.loadFields(source);
    ASSERT_EQ(previous.size(), 1U);
    EXPECT_EQ(previous[0].get(), &oldChild);
    EXPECT_EQ(oldChild.getParent(), nullptr);
    EXPECT_EQ(target.getId(), source.getId());
    EXPECT_EQ(target.getName(), "Source");
    EXPECT_EQ(target.getLength(), 0.4);
    EXPECT_EQ(target.getAxialMethod(), AxialMethod::MIDDLE);
    ASSERT_EQ(target.getChildCount(), 1U);
    EXPECT_EQ(target.getChild(0).getId(), source.getChild(0).getId());
    EXPECT_NE(&target.getChild(0), &source.getChild(0));
    EXPECT_TRUE(target.getInsideColorComponentHandler().isEdgesSameAsInside());

    // Only a root can load.
    TestComponent& child = target.addChild(TestComponent::make());
    EXPECT_THROW(static_cast<void>(child.loadFields(source)), BugError);
}

TEST(RocketComponentCopy, ComponentCast)
{
    std::unique_ptr<RocketComponent> stage = std::make_unique<AxialStage>();
    EXPECT_EQ(QtRocket::componentCast<Rocket>(stage), nullptr);
    EXPECT_NE(stage, nullptr) << "a failed cast keeps the component";
    const std::unique_ptr<AxialStage> typed = QtRocket::componentCast<AxialStage>(std::move(stage));
    EXPECT_NE(typed, nullptr);
}

// ---- Split ----

/// Whether @p part is the split copy number @p i (0-based) of a three-fin set named "Fins" at
/// angle 0.1 with an override mass of 0.3.
::testing::AssertionResult isSplitPart(const RocketComponent::SplitResult& result, std::size_t i,
                                       const RocketComponent& parent)
{
    const RocketComponent* part = result.components.at(i);
    if (&parent.getChild(i) != part)
    {
        return ::testing::AssertionFailure() << "part " << i << " is not child " << i;
    }
    const auto* fin = dynamic_cast<const TestComponent*>(part);
    if (fin == nullptr || result.original == nullptr)
    {
        return ::testing::AssertionFailure() << "not a TestComponent, or no original";
    }
    const RocketComponent& original = *result.original;
    const double expectedAngle = 0.1 + ((static_cast<double>(static_cast<int>(i) * 2) * kPi) / 3);
    if (fin->getInstanceCount() != 1 || fin->getName() != "Fins #" + std::to_string(i + 1) ||
        fin->getAngleOffset() != expectedAngle || std::abs(fin->getOverrideMass() - 0.1) > 1e-15 ||
        fin->getId() == original.getId())
    {
        return ::testing::AssertionFailure()
               << "part " << i << ": " << fin->getName() << ", " << fin->getInstanceCount()
               << " instances, angle " << fin->getAngleOffset() << ", mass "
               << fin->getOverrideMass();
    }
    return ::testing::AssertionSuccess();
}

TEST(RocketComponentSplit, SplitsIntoSingleInstances)
{
    Rocket         rocket;
    AxialStage&    stage = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&      body  = stage.addChild(std::make_unique<BodyTube>(0.3, 0.025));
    TestComponent& fins  = body.addChild(TestComponent::make(0.05));
    body.addChild(TestComponent::make(0.01));
    fins.setName("Fins");
    fins.setInstances({Coordinate{}, Coordinate{}, Coordinate{}}, {0, 0, 0});
    fins.setAngleOffset(0.1);
    fins.setMass(0.2);
    fins.setMassOverridden(true);
    fins.setOverrideMass(0.3);
    rocket.enableEvents();

    const RocketComponent::SplitResult result = fins.splitInstances();
    ASSERT_EQ(result.components.size(), 3U);
    ASSERT_EQ(result.original.get(), &fins);
    EXPECT_EQ(fins.getParent(), nullptr);
    EXPECT_EQ(body.getChildCount(), 4U);
    EXPECT_TRUE(isSplitPart(result, 0, body));
    EXPECT_TRUE(isSplitPart(result, 1, body));
    EXPECT_TRUE(isSplitPart(result, 2, body));
    EXPECT_FALSE(rocket.isFrozen());
}

TEST(RocketComponentSplit, ASingleInstanceIsLeftAlone)
{
    Rocket      rocket;
    AxialStage& stage  = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&   single = stage.addChild(std::make_unique<BodyTube>(0.3, 0.025));
    rocket.enableEvents();

    const RocketComponent::SplitResult same = single.splitInstances(false);
    EXPECT_EQ(same.components, std::vector<RocketComponent*>{&single});
    EXPECT_EQ(same.original, nullptr);
    EXPECT_EQ(single.getParent(), &stage);
}

/// An exception a change listener throws at the final thaw leaves the split done: the copies are
/// in the tree, the rocket is thawed, and the original, which the lost result owned, is gone
/// (splitInstances() documents it; OpenRocket leaves the original detached, alive through the
/// caller's reference).
TEST(RocketComponentSplit, AThrowingListenerLeavesTheSplitDoneWithoutTheOriginal)
{
    Rocket         rocket;
    AxialStage&    stage = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&      body  = stage.addChild(std::make_unique<BodyTube>(0.3, 0.025));
    TestComponent& fins  = body.addChild(TestComponent::make(0.05));
    fins.setName("Fins");
    fins.setInstances({Coordinate{}, Coordinate{}, Coordinate{}}, {0, 0, 0});
    rocket.enableEvents();
    const Uuid originalId = fins.getId();

    const FailingListener listener(rocket, body, true);
    EXPECT_THROW(static_cast<void>(fins.splitInstances()), std::runtime_error);

    // One event, the thaw's, with the three copies in the body; `fins` no longer exists.
    EXPECT_EQ(listener.childCounts, std::vector<std::size_t>{3});
    EXPECT_FALSE(rocket.isFrozen());
    EXPECT_EQ(rocket.findComponent(originalId), nullptr);
    ASSERT_EQ(body.getChildCount(), 3U);
    EXPECT_EQ(body.getChild(0).getName(), "Fins #1");
    EXPECT_EQ(body.getChild(1).getName(), "Fins #2");
    EXPECT_EQ(body.getChild(2).getName(), "Fins #3");
    EXPECT_EQ(body.getChild(0).getInstanceCount(), 1);
    EXPECT_EQ(body.getChild(2).getInstanceCount(), 1);
}

/// Without the freeze, the listener already hears of the original leaving: when it throws
/// there, removeChild() puts the original back and nothing is split.
TEST(RocketComponentSplit, AThrowingListenerWithoutFreezeLeavesTheTreeAsItWas)
{
    Rocket         rocket;
    AxialStage&    stage = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&      body  = stage.addChild(std::make_unique<BodyTube>(0.3, 0.025));
    TestComponent& fins  = body.addChild(TestComponent::make(0.05));
    fins.setName("Fins");
    fins.setInstances({Coordinate{}, Coordinate{}, Coordinate{}}, {0, 0, 0});
    rocket.enableEvents();

    const FailingListener listener(rocket, body, false);
    EXPECT_THROW(static_cast<void>(fins.splitInstances(false)), std::runtime_error);

    // Heard: the original leaving, and its return.
    EXPECT_EQ(listener.childCounts, (std::vector<std::size_t>{0, 1}));
    ASSERT_EQ(body.getChildCount(), 1U);
    EXPECT_EQ(&body.getChild(0), &fins);
    EXPECT_EQ(fins.getParent(), &body);
    EXPECT_EQ(fins.getName(), "Fins");
    EXPECT_EQ(fins.getInstanceCount(), 3);
}

// ---- Structure and debug ----

TEST(RocketComponentDebug, DebugStrings)
{
    TestComponent  root;
    TestComponent& child = root.addChild(TestComponent::make());
    child.setName("child");
    const std::string text = root.toDebugString();
    EXPECT_TRUE(text.starts_with("MassComponent@")) << text;
    EXPECT_NE(text.find("[\"Mass Component\"; MassComponent@"), std::string::npos) << text;
    EXPECT_NE(text.find("[\"child\"]]"), std::string::npos) << text;

    const std::string detail = root.toDebugDetail();
    EXPECT_NE(detail.find("At Component: Mass Component, of class: MassComponent"),
              std::string::npos);
    EXPECT_NE(detail.find("via: AFTER"), std::string::npos);
}

TEST(RocketComponentDebug, DebugTree)
{
    Rocket      rocket;
    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&   body  = stage.addChild(std::make_unique<BodyTube>(0.3, 0.025));
    body.setName("Body");
    TestComponent& fins = body.addChild(TestComponent::make(0.05));
    fins.setInstances({Coordinate{}, Coordinate{}}, {0, 1});
    rocket.enableEvents();

    const std::string tree = rocket.toDebugTree();
    EXPECT_NE(tree.find("Rocket (x1)"), std::string::npos) << tree;
    EXPECT_NE(tree.find("....Stage (# 0)"), std::string::npos) << tree;
    EXPECT_NE(tree.find("........Body (x1)"), std::string::npos) << tree;
    EXPECT_NE(tree.find("(cluster: 2-test )"), std::string::npos) << tree;
    EXPECT_NE(tree.find("[ 2/ 2]"), std::string::npos) << tree;
    EXPECT_NE(tree.find("via: AFTER"), std::string::npos) << tree;
}

TEST(RocketComponentDebug, DebugNumbersRoundHalfUpAsJava)
{
    // Java's %f rounds the decimal digits half-up: %5.3f of 0.0625 is 0.063 (std::format's
    // half-even gives 0.062), %4.1f of 0.25 is " 0.3" and %.4f of 0.03125 is 0.0313.
    Rocket      rocket;
    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&   body  = stage.addChild(std::make_unique<BodyTube>(0.0625, 0.025));
    body.setName("Body");
    TestComponent& part = body.addChild(
        std::make_unique<TestComponent>(ComponentKind::MASS_COMPONENT, AxialMethod::TOP, 0.01));
    rocket.enableEvents();
    part.setAxialOffset(0.25);

    std::string bodyLine;
    body.toDebugTreeNode(bodyLine, "");
    EXPECT_NE(bodyLine.find("|  0.063; "), std::string::npos) << bodyLine;
    std::string stageLine;
    stage.toDebugTreeNode(stageLine, "");
    EXPECT_NE(stageLine.find("|  0.063; "), std::string::npos) << stageLine;
    EXPECT_NE(stageLine.find("len: 0.0625 )(offset:  0.0  via: AFTER )"), std::string::npos)
        << stageLine;
    std::string partLine;
    part.toDebugTreeNode(partLine, "");
    EXPECT_NE(partLine.find("(offset:  0.3  via: TOP )"), std::string::npos) << partLine;

    TestComponent detached{ComponentKind::MASS_COMPONENT, AxialMethod::TOP, 0.03125};
    detached.setAxialOffset(0.03125);
    const std::string detail = detached.toDebugDetail();
    EXPECT_NE(detail.find("position: 0.031250    at offset: 0.0313 via: TOP"), std::string::npos)
        << detail;
    EXPECT_NE(detail.find("length: 0.0313"), std::string::npos) << detail;
}

TEST(RocketComponentDebug, RingHelpers)
{
    EXPECT_DOUBLE_EQ(TestComponent::ringMass(0.02, 0.01, 0.1, 1000),
                     kPi * (0.0004 - 0.0001) * 0.1 * 1000);
    EXPECT_EQ(TestComponent::ringMass(0.01, 0.02, 0.1, 1000), 0.0) << "negative area is 0";
    EXPECT_DOUBLE_EQ(TestComponent::ringVolume(0.02, 0.01, 0.1), kPi * 0.0003 * 0.1);
    const Coordinate cg = TestComponent::ringCG(0.02, 0.01, 0.1, 0.3, 1000);
    EXPECT_DOUBLE_EQ(cg.x, 0.2);
    EXPECT_DOUBLE_EQ(cg.weight, kPi * 0.0003 * 0.2 * 1000);
    EXPECT_DOUBLE_EQ(TestComponent::ringLongitudinalUnitInertia(0.02, 0.01, 0.1),
                     ((3 * (0.0001 + 0.0004)) + 0.01) / 12);
    EXPECT_DOUBLE_EQ(TestComponent::ringRotationalUnitInertia(0.02, 0.01), (0.0001 + 0.0004) / 2);

    std::vector<Coordinate> bounds;
    TestComponent::addBoundingBox(bounds, 0.1, 0.4, 0.02);
    ASSERT_EQ(bounds.size(), 2U);
    EXPECT_TRUE(bounds[0].exactlyEquals(Coordinate{0.1, -0.02, -0.02}));
    EXPECT_TRUE(bounds[1].exactlyEquals(Coordinate{0.4, 0.02, 0.02}));
    TestComponent::addBound(bounds, 0.2, 0.03);
    ASSERT_EQ(bounds.size(), 6U);
    EXPECT_TRUE(bounds[2].exactlyEquals(Coordinate{0.2, -0.03, -0.03}));
    EXPECT_TRUE(bounds[3].exactlyEquals(Coordinate{0.2, 0.03, -0.03}));
    EXPECT_TRUE(bounds[4].exactlyEquals(Coordinate{0.2, 0.03, 0.03}));
    EXPECT_TRUE(bounds[5].exactlyEquals(Coordinate{0.2, -0.03, 0.03}));
}

// ---- ComponentKind ----

TEST(ComponentKind, XmlNamesAsOpenRocketsFiles)
{
    EXPECT_EQ(QtRocket::xmlName(ComponentKind::ROCKET), "rocket");
    EXPECT_EQ(QtRocket::xmlName(ComponentKind::AXIAL_STAGE), "stage");
    EXPECT_EQ(QtRocket::xmlName(ComponentKind::PARALLEL_STAGE), "parallelstage");
    EXPECT_EQ(QtRocket::xmlName(ComponentKind::POD_SET), "podset");
    EXPECT_EQ(QtRocket::xmlName(ComponentKind::TRAPEZOID_FIN_SET), "trapezoidfinset");
    EXPECT_EQ(QtRocket::xmlName(ComponentKind::ENGINE_BLOCK), "engineblock");
    EXPECT_EQ(QtRocket::xmlName(ComponentKind::STREAMER), "streamer");
}

TEST(ComponentKind, XmlNamesReadBack)
{
    EXPECT_TRUE(std::ranges::all_of(QtRocket::kAllComponentKinds, [](ComponentKind kind) {
        return QtRocket::componentKindFromXmlName(QtRocket::xmlName(kind)) == kind;
    }));
    EXPECT_EQ(QtRocket::componentKindFromXmlName("boosterset"), ComponentKind::PARALLEL_STAGE);
    EXPECT_EQ(QtRocket::componentKindFromXmlName("sleeve"), std::nullopt);
    EXPECT_EQ(QtRocket::componentKindFromXmlName("BodyTube"), std::nullopt);
}

TEST(ComponentKind, NamesAndDisplayKeys)
{
    EXPECT_EQ(QtRocket::kAllComponentKinds.size(), 22U);
    EXPECT_EQ(QtRocket::componentKindName(ComponentKind::TUBE_FIN_SET), "TUBE_FIN_SET");
    EXPECT_EQ(QtRocket::className(ComponentKind::PARALLEL_STAGE), "ParallelStage");
    EXPECT_EQ(QtRocket::displayKey(ComponentKind::ELLIPTICAL_FIN_SET),
              "EllipticalFinSet.Ellipticalfinset");
    EXPECT_EQ(QtRocket::displayKey(ComponentKind::LAUNCH_LUG), "LaunchLug.Launchlug");
    EXPECT_EQ(QtRocket::displayName(ComponentKind::PARALLEL_STAGE), "Booster Set");
    EXPECT_EQ(QtRocket::displayName(ComponentKind::TRAPEZOID_FIN_SET), "Trapezoidal Fin Set");
}

TEST(ComponentKind, EveryKindIsAssemblyExternalOrInternal)
{
    const auto& all = QtRocket::kAllComponentKinds;
    EXPECT_EQ(std::ranges::count_if(all, QtRocket::isAssembly), 4);
    EXPECT_EQ(std::ranges::count_if(all, QtRocket::isExternal), 9);
    EXPECT_EQ(std::ranges::count_if(all, QtRocket::isInternal), 9);
    // Every kind is exactly one of them.
    EXPECT_TRUE(std::ranges::all_of(all, [](ComponentKind kind) {
        return (QtRocket::isAssembly(kind) ? 1 : 0) + (QtRocket::isExternal(kind) ? 1 : 0) +
                   (QtRocket::isInternal(kind) ? 1 : 0) ==
               1;
    }));
}

TEST(ComponentKind, CategoriesFollowTheJavaHierarchy)
{
    EXPECT_TRUE(QtRocket::isStage(ComponentKind::PARALLEL_STAGE));
    EXPECT_FALSE(QtRocket::isStage(ComponentKind::POD_SET));
    EXPECT_TRUE(QtRocket::isBodyComponent(ComponentKind::NOSE_CONE));
    EXPECT_FALSE(QtRocket::isBodyComponent(ComponentKind::TUBE_FIN_SET));
    EXPECT_FALSE(QtRocket::isFinSet(ComponentKind::TUBE_FIN_SET));
    EXPECT_TRUE(QtRocket::isFinSet(ComponentKind::FREEFORM_FIN_SET));
    EXPECT_TRUE(QtRocket::isRingComponent(ComponentKind::BULKHEAD));
    EXPECT_TRUE(QtRocket::isMassObject(ComponentKind::STREAMER));
    EXPECT_TRUE(QtRocket::isRecoveryDevice(ComponentKind::PARACHUTE));
    EXPECT_FALSE(QtRocket::isRecoveryDevice(ComponentKind::SHOCK_CORD));
}

TEST(ComponentKind, ClassChainsWalkTheJavaSuperclasses)
{
    using Chain = std::vector<std::string_view>;
    const std::vector<std::pair<ComponentKind, Chain>> expected{
        {ComponentKind::ROCKET, {"Rocket", "ComponentAssembly", "RocketComponent"}},
        {ComponentKind::PARALLEL_STAGE,
         {"ParallelStage", "AxialStage", "ComponentAssembly", "RocketComponent"}},
        {ComponentKind::BODY_TUBE,
         {"BodyTube", "SymmetricComponent", "BodyComponent", "ExternalComponent",
          "RocketComponent"}},
        {ComponentKind::NOSE_CONE,
         {"NoseCone", "Transition", "SymmetricComponent", "BodyComponent", "ExternalComponent",
          "RocketComponent"}},
        {ComponentKind::ELLIPTICAL_FIN_SET,
         {"EllipticalFinSet", "FinSet", "ExternalComponent", "RocketComponent"}},
        {ComponentKind::LAUNCH_LUG, {"LaunchLug", "Tube", "ExternalComponent", "RocketComponent"}},
        {ComponentKind::RAIL_BUTTON, {"RailButton", "ExternalComponent", "RocketComponent"}},
        {ComponentKind::TUBE_COUPLER,
         {"TubeCoupler", "ThicknessRingComponent", "RingComponent", "StructuralComponent",
          "InternalComponent", "RocketComponent"}},
        {ComponentKind::BULKHEAD,
         {"Bulkhead", "RadiusRingComponent", "RingComponent", "StructuralComponent",
          "InternalComponent", "RocketComponent"}},
        {ComponentKind::SHOCK_CORD,
         {"ShockCord", "MassObject", "InternalComponent", "RocketComponent"}},
        {ComponentKind::STREAMER,
         {"Streamer", "RecoveryDevice", "MassObject", "InternalComponent", "RocketComponent"}},
    };
    for (const auto& [kind, names] : expected)
    {
        EXPECT_TRUE(std::ranges::equal(QtRocket::componentClassChain(kind), names))
            << QtRocket::componentKindName(kind);
    }
}

TEST(ComponentKind, EveryClassChainRunsFromTheClassToRocketComponent)
{
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        const std::span<const std::string_view> chain = QtRocket::componentClassChain(kind);
        ASSERT_GE(chain.size(), 3U) << QtRocket::componentKindName(kind);
        EXPECT_EQ(chain.front(), QtRocket::className(kind));
        EXPECT_EQ(chain.back(), "RocketComponent");
    }
}

TEST(ComponentKind, ClassChainsFeedThePerClassPreferences)
{
    // The default colour and line style tables are keyed on superclasses: the walk must reach
    // them.
    const QtRocket::InMemoryPreferences prefs;
    EXPECT_EQ(prefs.getDefaultColor(QtRocket::componentClassChain(ComponentKind::NOSE_CONE)),
              Color(0, 0, 240))
        << "BodyComponent";
    EXPECT_EQ(prefs.getDefaultColor(QtRocket::componentClassChain(ComponentKind::STREAMER)),
              Color(255, 0, 0))
        << "RecoveryDevice before MassObject";
    EXPECT_EQ(prefs.getDefaultColor(QtRocket::componentClassChain(ComponentKind::BULKHEAD)),
              Color(170, 0, 100))
        << "InternalComponent";
    EXPECT_EQ(prefs.getDefaultLineStyle(QtRocket::componentClassChain(ComponentKind::PARACHUTE)),
              LineStyle::DASHED)
        << "MassObject";
    EXPECT_EQ(prefs.getDefaultLineStyle(QtRocket::componentClassChain(ComponentKind::BODY_TUBE)),
              LineStyle::SOLID)
        << "RocketComponent";
}

// ---- A component as the source of a message (logging/Message.h) ----

TEST(RocketComponent, IsAMessageSourceByItsIdAndName)
{
    BodyTube tube;
    tube.setName("Payload bay");
    const QtRocket::MessageSource source = QtRocket::MessageSource::of(tube);
    EXPECT_EQ(source.id, tube.getId());
    EXPECT_EQ(source.name, "Payload bay");

    // Through a reference to the base class as well: the id and the name are the component's.
    const RocketComponent& component = tube;
    EXPECT_TRUE(QtRocket::MessageSource::of(component) == source);

    // A warning prints the name the component had when the source was made, and stays the
    // warning of that component when it is renamed (equality is by id).
    QtRocket::Warning::Other warning = QtRocket::Warning::kOpenAirframeForward;
    warning.setSources(QtRocket::MessageSources{source});
    tube.setName("Booster tube");
    EXPECT_EQ(warning.toString(), R"(Open forward airframe (diameter > 0):  "Payload bay")");
    EXPECT_TRUE(warning.sources().front() == QtRocket::MessageSource::of(tube));

    // Another component, even one of the same name, is another source.
    BodyTube other;
    other.setName("Payload bay");
    EXPECT_FALSE(QtRocket::MessageSource::of(other) == source);
}

}  // namespace
