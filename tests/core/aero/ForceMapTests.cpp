#include "QtRocket/aero/ForceMap.h"

#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/StabilityForceBreakdown.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "rocket/TestComponent.h"

namespace
{

using QtRocket::AerodynamicForces;
using QtRocket::ForceMap;
using QtRocket::RocketComponent;
using QtRocket::StabilityForceBreakdown;
using QtRocket::Test::TestComponent;

using Keys = std::vector<const RocketComponent*>;

/// Forces whose CN is @p cn.
[[nodiscard]] AerodynamicForces forcesWithCN(double cn)
{
    AerodynamicForces forces;
    forces.setCN(cn);
    return forces;
}

TEST(ForceMap, KeepsTheOrderOfInsertion)
{
    const TestComponent a;
    const TestComponent b;
    const TestComponent c;
    ForceMap            map;
    EXPECT_TRUE(map.empty());

    map.put(&c, forcesWithCN(3));
    map.put(&a, forcesWithCN(1));
    map.put(&b, forcesWithCN(2));
    EXPECT_EQ(map.size(), 3U);
    EXPECT_EQ(map.keys(), (Keys{&c, &a, &b}));

    // Putting an existing key replaces its forces in place (LinkedHashMap.put()).
    AerodynamicForces& stored = map.put(&a, forcesWithCN(10));
    EXPECT_EQ(stored.getCN(), 10);
    EXPECT_EQ(map.keys(), (Keys{&c, &a, &b}));
    EXPECT_EQ(map.get(&a)->getCN(), 10);

    std::vector<double> cns;
    for (const auto& [key, forces] : map)
    {
        cns.push_back(forces.getCN());
    }
    EXPECT_EQ(cns, (std::vector<double>{3, 10, 2}));
}

TEST(ForceMap, GetGivesNullForAMissingKeyAndAllowsANullKey)
{
    const TestComponent a;
    ForceMap            map;
    EXPECT_EQ(map.get(&a), nullptr);
    EXPECT_FALSE(map.containsKey(&a));

    map.put(nullptr, forcesWithCN(7));
    EXPECT_TRUE(map.containsKey(nullptr));
    EXPECT_EQ(map.get(nullptr)->getCN(), 7);

    const ForceMap& constMap = map;
    EXPECT_EQ(constMap.get(nullptr)->getCN(), 7);
    EXPECT_EQ(constMap.get(&a), nullptr);
}

TEST(ForceMap, ForcesCanBeModifiedInPlace)
{
    const TestComponent a;
    ForceMap            map;
    map.put(&a, forcesWithCN(1));
    map.get(&a)->setCm(0.5);
    for (auto& entry : map)
    {
        entry.second.setCD(0.25);
    }
    EXPECT_EQ(map.get(&a)->getCm(), 0.5);
    EXPECT_EQ(map.get(&a)->getCD(), 0.25);
}

TEST(ForceMap, RemoveKeepsTheOthersInOrder)
{
    const TestComponent a;
    const TestComponent b;
    const TestComponent c;
    ForceMap            map;
    map.put(&a, forcesWithCN(1));
    map.put(&b, forcesWithCN(2));
    map.put(&c, forcesWithCN(3));

    EXPECT_TRUE(map.remove(&a));
    EXPECT_FALSE(map.remove(&a));
    EXPECT_EQ(map.keys(), (Keys{&b, &c}));
    EXPECT_EQ(map.get(&c)->getCN(), 3);
    EXPECT_EQ(map.get(&b)->getCN(), 2);

    map.put(&a, forcesWithCN(4));
    EXPECT_EQ(map.keys(), (Keys{&b, &c, &a}));

    map.clear();
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.get(&b), nullptr);
}

TEST(StabilityForceBreakdown, HoldsTheTwoMaps)
{
    const TestComponent part;
    const TestComponent assembly;
    ForceMap            components;
    components.put(&part, forcesWithCN(1));
    ForceMap assemblies;
    assemblies.put(&assembly, forcesWithCN(2));

    StabilityForceBreakdown breakdown{components, assemblies};
    EXPECT_EQ(breakdown.getComponentForces().keys(), Keys{&part});
    EXPECT_EQ(breakdown.getAssemblyForces().get(&assembly)->getCN(), 2);

    breakdown.getAssemblyForces().get(&assembly)->setCD(0.5);
    const StabilityForceBreakdown& constBreakdown = breakdown;
    EXPECT_EQ(constBreakdown.getAssemblyForces().get(&assembly)->getCD(), 0.5);
    EXPECT_EQ(constBreakdown.getComponentForces().size(), 1U);
}

}  // namespace
