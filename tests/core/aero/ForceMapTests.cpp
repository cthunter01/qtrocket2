#include "QtRocket/aero/ForceMap.h"

#include <array>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>
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

// Java's Map.Entry has no setKey(): the key cannot be reassigned through an entry, which would
// leave the index pointing at the wrong entry. The forces can be modified (through a mutable map).
template <class EntryRef>
concept KeyAssignable = requires(EntryRef entry, const RocketComponent* key) { entry.first = key; };
template <class EntryRef>
concept ForcesAssignable =
    requires(EntryRef entry, const AerodynamicForces& forces) { entry.second = forces; };

static_assert(std::is_same_v<std::iter_reference_t<ForceMap::Iterator>, ForceMap::Entry&>);
static_assert(
    std::is_same_v<std::iter_reference_t<ForceMap::ConstIterator>, const ForceMap::Entry&>);
static_assert(!KeyAssignable<std::iter_reference_t<ForceMap::Iterator>>);
static_assert(!KeyAssignable<std::iter_reference_t<ForceMap::ConstIterator>>);
static_assert(ForcesAssignable<std::iter_reference_t<ForceMap::Iterator>>);
static_assert(!ForcesAssignable<std::iter_reference_t<ForceMap::ConstIterator>>);
// The concepts do detect an assignable key: the vector-backed entry type this replaced had one.
static_assert(KeyAssignable<std::pair<const RocketComponent*, AerodynamicForces>&>);

TEST(ForceMap, ReferencesStayValidWhenOtherKeysArePut)
{
    // One node per entry, as in Java's LinkedHashMap: putting more keys never moves an entry.
    const std::array<TestComponent, 32> components;
    ForceMap                            map;
    AerodynamicForces&                  first = map.put(components.data(), forcesWithCN(1));
    for (std::size_t i = 1; i < components.size(); ++i)
    {
        map.put(&components.at(i), forcesWithCN(static_cast<double>(i)));
    }
    EXPECT_EQ(&first, map.get(components.data()));
    first.setCm(0.5);
    EXPECT_EQ(map.get(components.data())->getCm(), 0.5);

    // Removing another key leaves it in place as well.
    EXPECT_TRUE(map.remove(&components[1]));
    EXPECT_EQ(&first, map.get(components.data()));
    EXPECT_EQ(map.size(), components.size() - 1);
    EXPECT_EQ(map.get(&components[2])->getCN(), 2);
}

TEST(ForceMap, ACopyIsIndependentAndIndexesItsOwnEntries)
{
    const TestComponent a;
    const TestComponent b;
    ForceMap            original;
    original.put(&a, forcesWithCN(1));
    original.put(&b, forcesWithCN(2));

    ForceMap copy{original};
    EXPECT_EQ(copy.keys(), (Keys{&a, &b}));
    EXPECT_NE(copy.get(&a), original.get(&a));  // the copy's own entry, not the original's
    copy.get(&a)->setCN(10);
    EXPECT_EQ(original.get(&a)->getCN(), 1);
    EXPECT_EQ(copy.get(&a)->getCN(), 10);

    ForceMap assigned;
    assigned.put(&b, forcesWithCN(5));
    assigned = original;
    EXPECT_EQ(assigned.keys(), (Keys{&a, &b}));
    EXPECT_NE(assigned.get(&b), original.get(&b));
    EXPECT_EQ(assigned.get(&b)->getCN(), 2);
    EXPECT_TRUE(assigned.remove(&a));
    EXPECT_EQ(original.keys(), (Keys{&a, &b}));

    const ForceMap& self = assigned;
    assigned             = self;
    EXPECT_EQ(assigned.keys(), Keys{&b});
}

TEST(ForceMap, MovingTakesTheEntriesAlongAndEmptiesTheSource)
{
    const TestComponent a;
    const TestComponent b;
    ForceMap            source;
    AerodynamicForces&  forcesOfA = source.put(&a, forcesWithCN(1));

    ForceMap moved{std::move(source)};
    EXPECT_EQ(moved.get(&a), &forcesOfA);  // the entry itself moved along
    EXPECT_EQ(moved.keys(), Keys{&a});
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move): moved from on purpose
    EXPECT_TRUE(source.empty());
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.Move): the index was emptied along with the list
    EXPECT_EQ(source.get(&a), nullptr);
    source.put(&b, forcesWithCN(2));  // the moved-from map is usable again
    EXPECT_EQ(source.keys(), Keys{&b});

    ForceMap target;
    target.put(&b, forcesWithCN(3));
    target = std::move(moved);
    EXPECT_EQ(target.get(&a), &forcesOfA);
    EXPECT_EQ(target.get(&b), nullptr);
    EXPECT_EQ(target.keys(), Keys{&a});
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move): moved from on purpose
    EXPECT_TRUE(moved.empty());
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.Move): the index was emptied along with the list
    EXPECT_EQ(moved.get(&a), nullptr);
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
