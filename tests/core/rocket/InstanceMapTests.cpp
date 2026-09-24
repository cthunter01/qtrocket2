#include "QtRocket/rocket/InstanceMap.h"

#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/InstanceContext.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Transformation.h"
#include "rocket/TestComponent.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::InstanceContext;
using QtRocket::InstanceMap;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Transformation;
using QtRocket::Test::TestComponent;

/// Three components of a small tree, for keys.
class InstanceMapTest : public ::testing::Test
{
protected:
    InstanceMapTest()
    {
        m_stage = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_a     = &m_stage->addChild(TestComponent::make(0.1));
        m_b     = &m_stage->addChild(TestComponent::make(0.2));
        m_a->setName("A");
        m_b->setName("B");
    }

    Rocket         m_rocket;
    AxialStage*    m_stage{nullptr};
    TestComponent* m_a{nullptr};
    TestComponent* m_b{nullptr};
};

TEST_F(InstanceMapTest, StartsEmpty)
{
    const InstanceMap map;
    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(map.size(), 0U);
    EXPECT_EQ(map.count(*m_a), 0);
    EXPECT_FALSE(map.containsKey(*m_a));
    EXPECT_TRUE(map.getInstanceContexts(*m_a).empty());
    EXPECT_TRUE(map.keys().empty());
    EXPECT_EQ(map.begin(), map.end());
    EXPECT_EQ(map.toString(), ">> Printing InstanceMap:\n");
}

TEST_F(InstanceMapTest, EmplaceKeepsTheOrderOfFirstInsertion)
{
    InstanceMap map;
    map.emplace(*m_b, 0, Transformation::translation(1, 0, 0));
    map.emplace(*m_a, 0, Transformation::translation(2, 0, 0));
    map.emplace(*m_b, 1, Transformation::translation(3, 0, 0),
                Transformation::translation(1, 1, 1));

    EXPECT_EQ(map.size(), 2U);
    EXPECT_EQ(map.keys(), (std::vector<RocketComponent*>{m_b, m_a}));
    EXPECT_EQ(map.count(*m_b), 2);
    EXPECT_EQ(map.count(*m_a), 1);

    const std::span<const InstanceContext> contexts = map.getInstanceContexts(*m_b);
    ASSERT_EQ(contexts.size(), 2U);
    EXPECT_EQ(contexts[0].component, m_b);
    EXPECT_EQ(contexts[0].instanceNumber, 0);
    EXPECT_EQ(contexts[1].instanceNumber, 1);
    EXPECT_TRUE(contexts[1].getLocation().exactlyEquals(Coordinate{3, 0, 0}));
    EXPECT_TRUE(contexts[0].getParentTransform().isIdentity()) << "the default parent transform";
    EXPECT_EQ(contexts[1].getParentTransform(), Transformation::translation(1, 1, 1));

    // Iteration visits the entries in the same order.
    ASSERT_EQ(std::distance(map.begin(), map.end()), 2);
    EXPECT_EQ(map.begin()->first, m_b);
    EXPECT_EQ(std::next(map.begin())->first, m_a);
}

TEST_F(InstanceMapTest, PutReplacesInPlaceAndAddAppends)
{
    InstanceMap map;
    map.emplace(*m_a, 0, Transformation::kIdentity);
    map.emplace(*m_b, 0, Transformation::kIdentity);

    const InstanceContext replacement{*m_a, 7, Transformation::translation(0, 1, 0)};
    map.put(*m_a, {replacement, replacement});
    EXPECT_EQ(map.keys(), (std::vector<RocketComponent*>{m_a, m_b})) << "put() keeps the place";
    EXPECT_EQ(map.count(*m_a), 2);
    EXPECT_EQ(map.getInstanceContexts(*m_a)[0].instanceNumber, 7);

    map.add(InstanceContext{*m_b, 3, Transformation::kIdentity});
    EXPECT_EQ(map.count(*m_b), 2);
    EXPECT_EQ(map.getInstanceContexts(*m_b)[1].instanceNumber, 3);

    // A new key goes last.
    map.clear();
    map.add(InstanceContext{*m_b, 0, Transformation::kIdentity});
    map.put(*m_a, {});
    EXPECT_EQ(map.keys(), (std::vector<RocketComponent*>{m_b, m_a}));
    EXPECT_TRUE(map.containsKey(*m_a));
    EXPECT_EQ(map.count(*m_a), 0);
}

TEST_F(InstanceMapTest, RemoveAndEraseIfKeepTheOthersInOrder)
{
    InstanceMap map;
    map.emplace(*m_stage, 0, Transformation::kIdentity);
    map.emplace(*m_a, 0, Transformation::kIdentity);
    map.emplace(*m_b, 0, Transformation::kIdentity);

    EXPECT_TRUE(map.remove(*m_a));
    EXPECT_FALSE(map.remove(*m_a));
    EXPECT_EQ(map.keys(), (std::vector<RocketComponent*>{m_stage, m_b}));
    EXPECT_EQ(map.count(*m_b), 1) << "the index follows the removal";

    map.emplace(*m_a, 0, Transformation::kIdentity);
    EXPECT_EQ(map.eraseIf([this](const RocketComponent& c) { return &c != m_a; }), 2U);
    EXPECT_EQ(map.keys(), std::vector<RocketComponent*>{m_a});
    EXPECT_TRUE(map.containsKey(*m_a));
    EXPECT_FALSE(map.containsKey(*m_b));
}

TEST_F(InstanceMapTest, ToStringListsEveryContext)
{
    InstanceMap map;
    map.emplace(*m_a, 0, Transformation::translation(0.5, 0, 0));
    map.emplace(*m_a, 1, Transformation::translation(0.5, 0.25, 0));
    map.emplace(*m_b, 0, Transformation::kIdentity);
    const std::string expected =
        ">> Printing InstanceMap:\n"
        "....[ 0]:[A]\n"
        "........[@ 0][ 0]  " +
        Coordinate{0.5, 0, 0}.toPreciseString() +
        "\n"
        "........[@ 1][ 1]  " +
        Coordinate{0.5, 0.25, 0}.toPreciseString() +
        "\n"
        "....[ 1]:[B]\n"
        "........[@ 0][ 0]  " +
        Coordinate{0, 0, 0}.toPreciseString() + "\n";
    EXPECT_EQ(map.toString(), expected);
}

TEST_F(InstanceMapTest, ContextsCompareByComponentAndTransform)
{
    const InstanceContext a0{*m_a, 0, Transformation::translation(1, 0, 0)};
    const InstanceContext a1{*m_a, 1, Transformation::translation(1, 0, 0),
                             Transformation::translation(5, 0, 0)};
    const InstanceContext a2{*m_a, 0, Transformation::translation(2, 0, 0)};
    const InstanceContext b0{*m_b, 0, Transformation::translation(1, 0, 0)};
    EXPECT_EQ(a0, a1) << "the instance number and parent transform do not take part";
    EXPECT_NE(a0, a2);
    EXPECT_NE(a0, b0);
    EXPECT_EQ(a0.hashCode(), m_a->hashCode());
    EXPECT_EQ(a0.toString(), "Context for A #0");
    EXPECT_EQ(a1.toString(), "Context for A #1");
}

}  // namespace
