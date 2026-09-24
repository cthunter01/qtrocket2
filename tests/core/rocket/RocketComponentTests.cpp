#include "QtRocket/rocket/RocketComponent.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Uuid.h"
#include "rocket/TestComponent.h"

namespace
{

using QtRocket::Appearance;
using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BugError;
using QtRocket::Color;
using QtRocket::ComponentAssembly;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::LineStyle;
using QtRocket::RadiusMethod;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Uuid;
using QtRocket::Test::TestComponent;
using StageTracking = RocketComponent::StageTracking;

constexpr double kEpsilon = QtRocket::MathUtil::kEpsilon;
constexpr double kPi      = std::numbers::pi;

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

::testing::AssertionResult repositions(TestComponent& fins, const AxialPositionTestCase& cur)
{
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

/// The shape of TestRockets.makeEstesAlphaIII() that RocketTest's positioning tests use: a stage
/// holding a nose cone (0.07 m) and a body tube (0.2 m, radius 0.012 m) that carries a fin set
/// (root chord 0.05 m) at the bottom, whose first fin sits on the body's surface.
class EstesTreeTest : public ::testing::Test
{
protected:
    EstesTreeTest()
    {
        m_stage = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_nose  = &m_stage->addChild(TestComponent::make(0.07, ComponentKind::NOSE_CONE));
        m_body  = &m_stage->addChild(TestComponent::make(0.2, ComponentKind::BODY_TUBE));
        m_body->setOuterRadius(0.012);
        m_fins = &m_body->addChild(std::make_unique<TestComponent>(ComponentKind::TRAPEZOID_FIN_SET,
                                                                   AxialMethod::BOTTOM, 0.05));
        m_fins->setInstances({Coordinate{0.0, 0.012, 0.0}}, {0.0});
        m_rocket.enableEvents();
    }

    Rocket         m_rocket;
    AxialStage*    m_stage{nullptr};
    TestComponent* m_nose{nullptr};
    TestComponent* m_body{nullptr};
    TestComponent* m_fins{nullptr};
};

// ---- Ported from RocketTest.java (the parts that need no concrete component) ----

TEST_F(EstesTreeTest, ComponentLocations)
{
    EXPECT_TRUE(coordinatesNear(m_nose->getComponentLocations().at(0), Coordinate{0, 0, 0}));
    EXPECT_TRUE(coordinatesNear(m_body->getComponentLocations().at(0), Coordinate{0.07, 0, 0}));
    EXPECT_EQ(m_fins->getComponentLocations().at(0), (Coordinate{0.22, 0.012, 0}));
}

TEST_F(EstesTreeTest, ChangeAxialMethod)
{
    // Verify the construction.
    EXPECT_NEAR(0.20, m_body->getLength(), kEpsilon) << "incorrect body length";
    EXPECT_NEAR(0.05, m_fins->getLength(), kEpsilon) << "incorrect fin length";
    EXPECT_EQ(m_fins->getComponentLocations().at(0), (Coordinate{0.22, 0.012, 0}));

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
        EXPECT_TRUE(repositions(*m_fins, allTestCases[caseIndex])) << "Test Case # " << caseIndex;
    }
}

TEST_F(EstesTreeTest, ComponentLocationCacheInvalidatesOnMove)
{
    // Warm the caches before moving the fin set, so that the checks cover invalidation too.
    const Coordinate initialAbsolute = m_fins->getComponentLocations().at(0);
    const Coordinate initialRelative = m_fins->toRelative(Coordinate::kNul, *m_body).at(0);

    m_fins->setAxialMethod(AxialMethod::TOP);
    m_fins->setAxialOffset(0.16);

    const Coordinate movedAbsolute = m_fins->getComponentLocations().at(0);
    const Coordinate movedRelative = m_fins->toRelative(Coordinate::kNul, *m_body).at(0);

    EXPECT_NE(initialAbsolute, movedAbsolute)
        << "Absolute component location cache was not invalidated";
    EXPECT_EQ((Coordinate{0.23, 0.012, 0}), movedAbsolute)
        << "Absolute component location is incorrect";

    EXPECT_NE(initialRelative, movedRelative)
        << "Relative component location cache was not invalidated";
    EXPECT_EQ((Coordinate{0.16, 0.012, 0}), movedRelative)
        << "Relative component location is incorrect";
}

TEST_F(EstesTreeTest, RemoveReadjustLocation)
{
    EXPECT_NEAR(m_body->getComponentLocations().at(0).x, 0.07, kEpsilon);

    // Removing the nose cone moves the body tube up.
    const std::unique_ptr<RocketComponent> nose = m_stage->removeChild(0);
    ASSERT_EQ(nose.get(), m_nose);

    EXPECT_NEAR(m_body->getComponentLocations().at(0).x, 0.0, kEpsilon);
}

// ---- Ported from UUIDSearchTest.java ----

TEST_F(EstesTreeTest, UuidSearch)
{
    // Searching for the nose cone by its id finds it.
    const Uuid noseConeId = m_nose->getId();
    EXPECT_EQ(m_nose, m_rocket.findComponent(noseConeId)) << "UUID search didn't find NoseCone";

    // Once removed, it is not found any more (Java: REMOVED).
    const std::unique_ptr<RocketComponent> removed =
        m_stage->removeChild(m_nose, StageTracking::SKIP);
    ASSERT_NE(removed, nullptr) << "failed to remove NoseCone";
    EXPECT_EQ(m_rocket.findComponent(noseConeId), nullptr);

    // The nil id is not found.
    EXPECT_EQ(m_rocket.findComponent(Uuid{0U, 0U}), nullptr);
}

// ---- Positions ----

TEST_F(EstesTreeTest, EveryAxialMethodPositionsAsItsArithmetic)
{
    m_fins->setAxialOffset(AxialMethod::TOP, 0.03);
    EXPECT_DOUBLE_EQ(m_fins->getPosition().x, 0.03);
    m_fins->setAxialOffset(AxialMethod::MIDDLE, 0.03);
    EXPECT_DOUBLE_EQ(m_fins->getPosition().x, 0.03 + ((0.2 - 0.05) / 2));
    m_fins->setAxialOffset(AxialMethod::BOTTOM, 0.03);
    EXPECT_DOUBLE_EQ(m_fins->getPosition().x, 0.03 + (0.2 - 0.05));
    // ABSOLUTE is from the rocket's tip: the body starts at 0.07.
    m_fins->setAxialOffset(AxialMethod::ABSOLUTE, 0.25);
    EXPECT_DOUBLE_EQ(m_fins->getPosition().x, 0.25 - 0.07);
    EXPECT_EQ(m_fins->getAxialMethod(), AxialMethod::ABSOLUTE);
    EXPECT_EQ(m_fins->getAxialOffset(), 0.25);
    // Asking for another method's offset changes nothing.
    EXPECT_DOUBLE_EQ(m_fins->getAxialOffset(AxialMethod::TOP), 0.18);
    EXPECT_DOUBLE_EQ(m_fins->getAxialOffset(AxialMethod::BOTTOM), 0.18 + (0.05 - 0.2));
    EXPECT_EQ(m_fins->getAxialMethod(), AxialMethod::ABSOLUTE);
    EXPECT_DOUBLE_EQ(m_fins->getAxialFront(), 0.18);
}

TEST_F(EstesTreeTest, AfterFromAnotherMethodUsesTheParentLengthUntilTheNextUpdate)
{
    // Java tests isAfter() on the current method: a component switched to AFTER through the
    // two-argument setter is first placed parent length + offset, then setAfter() on the next
    // update.
    m_fins->setAxialOffset(AxialMethod::AFTER, 0.01);
    EXPECT_DOUBLE_EQ(m_fins->getPosition().x, 0.21);
    m_fins->setAxialOffset(0.01);             // fires, which updates every component
    EXPECT_EQ(m_fins->getPosition().x, 0.0);  // the first child of the body
    EXPECT_EQ(m_fins->getAxialOffset(), 0.0);
}

TEST_F(EstesTreeTest, SmallPositionsSnapToZeroAndNaNIsABug)
{
    m_fins->setAxialOffset(AxialMethod::TOP, 5e-7);
    EXPECT_EQ(m_fins->getPosition().x, 0.0);
    EXPECT_EQ(m_fins->getAxialOffset(), 5e-7);
    m_fins->setAxialOffset(AxialMethod::TOP, 2e-6);
    EXPECT_EQ(m_fins->getPosition().x, 2e-6);
    EXPECT_THROW(m_fins->setAxialOffset(AxialMethod::TOP, std::numeric_limits<double>::quiet_NaN()),
                 BugError);
}

TEST_F(EstesTreeTest, AfterSkipsEmptyStages)
{
    // A second stage after an empty one: the empty stage is inactive, so the reference point
    // restarts at 0.
    AxialStage&    empty = m_rocket.addChild(std::make_unique<AxialStage>());
    AxialStage&    third = m_rocket.addChild(std::make_unique<AxialStage>());
    TestComponent& tube  = third.addChild(TestComponent::make(0.3));
    static_cast<void>(tube);
    // The third stage follows the first (0.27 long): the empty one in between is skipped.
    EXPECT_DOUBLE_EQ(empty.getPosition().x, 0.27);
    EXPECT_DOUBLE_EQ(third.getPosition().x, 0.27);

    // With the first stage removed as well, nothing active precedes it.
    const std::unique_ptr<RocketComponent> first = m_rocket.removeChild(0);
    EXPECT_EQ(third.getPosition().x, 0.0);
}

TEST(RocketComponentPosition, DetachedComponentTakesTheOffsetAsPosition)
{
    TestComponent lonely{ComponentKind::BODY_TUBE, AxialMethod::TOP, 0.1};
    lonely.setAxialOffset(AxialMethod::MIDDLE, 0.4);
    EXPECT_EQ(lonely.getPosition().x, 0.4);
    lonely.setAfter();  // nothing without a parent
    EXPECT_EQ(lonely.getAxialMethod(), AxialMethod::MIDDLE);
}

TEST(RocketComponentPosition, RadiusOffsetInAnotherMethod)
{
    TestComponent body{ComponentKind::BODY_TUBE};
    body.setOuterRadius(0.05);
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
        m_body            = &stage.addChild(TestComponent::make(0.3));
        m_pod             = &m_body->addChild(
            std::make_unique<TestComponent>(ComponentKind::POD_SET, AxialMethod::TOP));
        m_pod->setAxialOffset(AxialMethod::TOP, 0.1);
        m_pod->setInstances({Coordinate{0, 0.1, 0}, Coordinate{0, -0.1, 0}}, {0.0, kPi});
        m_leaf = &m_pod->addChild(
            std::make_unique<TestComponent>(ComponentKind::BODY_TUBE, AxialMethod::TOP));
        m_leaf->setAxialOffset(AxialMethod::TOP, 0.05);
        m_leaf->setInstances({Coordinate{0, 0.01, 0}, Coordinate{0, 0, 0.02}}, {0.0, 0.0});
        m_rocket.enableEvents();
    }

    Rocket         m_rocket;
    TestComponent* m_body{nullptr};
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
    TestComponent& body  = stage.addChild(TestComponent::make());
    TestComponent& inner = body.addChild(TestComponent::make(0.0, ComponentKind::INNER_TUBE));

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

TEST(RocketComponentTree, StagesBelowAComponent)
{
    Rocket         rocket;
    AxialStage&    core    = rocket.addChild(std::make_unique<AxialStage>());
    TestComponent& body    = core.addChild(TestComponent::make());
    AxialStage&    booster = body.addChild(std::make_unique<AxialStage>());
    booster.addChild(TestComponent::make());
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
    auto           range = root.subtree();
    auto           it    = range.begin();
    EXPECT_EQ(&*it, &root);
    EXPECT_EQ(it->getParent(), nullptr);
    auto previous = it++;
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
    stage.addChild(TestComponent::make());
    rocket.enableEvents();

    auto it = rocket.subtree().begin();
    ++it;
    stage.addChild(TestComponent::make());  // a tree change
    EXPECT_THROW(++it, BugError);
}

// ---- Properties ----

TEST(RocketComponentProperties, DefaultValues)
{
    const TestComponent component;
    EXPECT_EQ(component.getName(), "Body Tube");
    EXPECT_EQ(component.toString(), "Body Tube");
    EXPECT_EQ(component.getComponentName(), "Body Tube");
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
    EXPECT_EQ(component.getName(), "Body Tube");
    component.setName("x");
    component.setName("");
    EXPECT_EQ(component.getName(), "Body Tube");
    // Other characters are kept.
    component.setName("\xC2\xA0");
    EXPECT_EQ(component.getName(), "\xC2\xA0");

    const TestComponent nose{ComponentKind::NOSE_CONE};
    EXPECT_EQ(nose.getName(), "Nose Cone");
    EXPECT_EQ(Rocket{}.getName(), "Rocket");
    EXPECT_EQ(AxialStage{}.getName(), "Stage");
}

TEST(RocketComponentProperties, Ids)
{
    TestComponent component;
    const Uuid    id{0x123e4567e89b12d3ULL, 0xa456426614174000ULL};
    component.setId(id);
    EXPECT_EQ(component.getId(), id);
    EXPECT_EQ(component.getDebugName(), "Body Tube/123e4567");
    EXPECT_EQ(component.toDebugName(), "Body Tube<BodyTube>(123e4567)");

    ASSERT_TRUE(component.setId("00000000-0000-0000-0000-000000000001").has_value());
    EXPECT_EQ(component.getId(), (Uuid{0U, 1U}));
    const auto failed = component.setId("not a uuid");
    EXPECT_FALSE(failed.has_value());
    EXPECT_EQ(component.getId(), (Uuid{0U, 1U}));
    EXPECT_EQ(component.hashCode(), (Uuid{0U, 1U}).hashCode());
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

/// A rocket with one stage and one body tube, events enabled, recording the event types.
class PropertyEventsTest : public ::testing::Test
{
protected:
    PropertyEventsTest()
    {
        m_stage = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_body  = &m_stage->addChild(TestComponent::make(0.1));
        m_rocket.enableEvents();
        m_connection = m_rocket.addComponentChangeListener(
            [this](const ComponentChangeEvent& e) { m_types.push_back(e.getType()); });
    }

    Rocket                                  m_rocket;
    AxialStage*                             m_stage{nullptr};
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
    Rocket         rocket;
    AxialStage&    stage = rocket.addChild(std::make_unique<AxialStage>());
    TestComponent& body  = stage.addChild(TestComponent::make(0.1));
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
    TestComponent  root{ComponentKind::BODY_TUBE, AxialMethod::TOP, 0.3};
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

    TestComponent source{ComponentKind::BODY_TUBE, AxialMethod::MIDDLE, 0.4};
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
    TestComponent& body  = stage.addChild(TestComponent::make(0.3));
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
    Rocket         rocket;
    AxialStage&    stage  = rocket.addChild(std::make_unique<AxialStage>());
    TestComponent& single = stage.addChild(TestComponent::make(0.3));
    rocket.enableEvents();

    const RocketComponent::SplitResult same = single.splitInstances(false);
    EXPECT_EQ(same.components, std::vector<RocketComponent*>{&single});
    EXPECT_EQ(same.original, nullptr);
    EXPECT_EQ(single.getParent(), &stage);
}

// ---- Structure and debug ----

TEST(RocketComponentDebug, DebugStrings)
{
    TestComponent  root;
    TestComponent& child = root.addChild(TestComponent::make());
    child.setName("child");
    const std::string text = root.toDebugString();
    EXPECT_TRUE(text.starts_with("BodyTube@")) << text;
    EXPECT_NE(text.find("[\"Body Tube\"; BodyTube@"), std::string::npos) << text;
    EXPECT_NE(text.find("[\"child\"]]"), std::string::npos) << text;

    const std::string detail = root.toDebugDetail();
    EXPECT_NE(detail.find("At Component: Body Tube, of class: BodyTube"), std::string::npos);
    EXPECT_NE(detail.find("via: AFTER"), std::string::npos);
}

TEST(RocketComponentDebug, DebugTree)
{
    Rocket         rocket;
    AxialStage&    stage = rocket.addChild(std::make_unique<AxialStage>());
    TestComponent& body  = stage.addChild(TestComponent::make(0.3));
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

}  // namespace
