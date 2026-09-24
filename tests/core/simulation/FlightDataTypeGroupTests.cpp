#include "QtRocket/simulation/FlightDataTypeGroup.h"

#include <algorithm>
#include <cstddef>
#include <set>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace
{

using QtRocket::compareTo;
using QtRocket::displayKey;
using QtRocket::displayName;
using QtRocket::FlightDataTypeGroup;
using QtRocket::kAllFlightDataTypeGroups;
using QtRocket::priority;

TEST(FlightDataTypeGroup, AllGroupsInJavaOrder)
{
    // FlightDataTypeGroup.ALL_GROUPS: the declaration order, CUSTOM last.
    ASSERT_EQ(kAllFlightDataTypeGroups.size(), 12U);
    for (std::size_t i = 0; i < kAllFlightDataTypeGroups.size(); i++)
    {
        EXPECT_EQ(static_cast<std::size_t>(kAllFlightDataTypeGroups.at(i)), i);
    }
    EXPECT_EQ(kAllFlightDataTypeGroups.front(), FlightDataTypeGroup::TIME);
    EXPECT_EQ(kAllFlightDataTypeGroups.back(), FlightDataTypeGroup::CUSTOM);
}

TEST(FlightDataTypeGroup, Priorities)
{
    EXPECT_EQ(priority(FlightDataTypeGroup::TIME), 0);
    EXPECT_EQ(priority(FlightDataTypeGroup::POSITION_AND_MOTION), 10);
    EXPECT_EQ(priority(FlightDataTypeGroup::ORIENTATION), 20);
    EXPECT_EQ(priority(FlightDataTypeGroup::MASS_AND_INERTIA), 30);
    EXPECT_EQ(priority(FlightDataTypeGroup::STABILITY), 40);
    EXPECT_EQ(priority(FlightDataTypeGroup::THRUST_AND_DRAG), 50);
    EXPECT_EQ(priority(FlightDataTypeGroup::COEFFICIENTS), 60);
    EXPECT_EQ(priority(FlightDataTypeGroup::ATMOSPHERIC_CONDITIONS), 70);
    EXPECT_EQ(priority(FlightDataTypeGroup::CHARACTERISTIC_NUMBERS), 80);
    EXPECT_EQ(priority(FlightDataTypeGroup::REFERENCE_VALUES), 90);
    EXPECT_EQ(priority(FlightDataTypeGroup::SIMULATION_INFORMATION), 100);
    EXPECT_EQ(priority(FlightDataTypeGroup::CUSTOM), 200);
}

TEST(FlightDataTypeGroup, EnglishNames)
{
    EXPECT_EQ(displayName(FlightDataTypeGroup::TIME), "Time");
    EXPECT_EQ(displayName(FlightDataTypeGroup::POSITION_AND_MOTION), "Position and Motion");
    EXPECT_EQ(displayName(FlightDataTypeGroup::ORIENTATION), "Orientation");
    EXPECT_EQ(displayName(FlightDataTypeGroup::MASS_AND_INERTIA), "Mass and Inertia");
    EXPECT_EQ(displayName(FlightDataTypeGroup::STABILITY), "Stability");
    EXPECT_EQ(displayName(FlightDataTypeGroup::THRUST_AND_DRAG), "Thrust and Drag");
    EXPECT_EQ(displayName(FlightDataTypeGroup::COEFFICIENTS), "Coefficients");
    EXPECT_EQ(displayName(FlightDataTypeGroup::ATMOSPHERIC_CONDITIONS), "Atmospheric Conditions");
    EXPECT_EQ(displayName(FlightDataTypeGroup::CHARACTERISTIC_NUMBERS), "Characteristic Numbers");
    EXPECT_EQ(displayName(FlightDataTypeGroup::REFERENCE_VALUES), "Reference Values");
    EXPECT_EQ(displayName(FlightDataTypeGroup::SIMULATION_INFORMATION), "Simulation Information");
    EXPECT_EQ(displayName(FlightDataTypeGroup::CUSTOM), "Custom");
}

TEST(FlightDataTypeGroup, DisplayKeys)
{
    EXPECT_EQ(displayKey(FlightDataTypeGroup::TIME), "FlightDataTypeGroup.GROUP_TIME");
    EXPECT_EQ(displayKey(FlightDataTypeGroup::POSITION_AND_MOTION),
              "FlightDataTypeGroup.GROUP_POSITION_AND_MOTION");
    EXPECT_EQ(displayKey(FlightDataTypeGroup::ORIENTATION),
              "FlightDataTypeGroup.GROUP_ORIENTATION");
    EXPECT_EQ(displayKey(FlightDataTypeGroup::MASS_AND_INERTIA),
              "FlightDataTypeGroup.GROUP_MASS_AND_INERTIA");
    EXPECT_EQ(displayKey(FlightDataTypeGroup::STABILITY), "FlightDataTypeGroup.GROUP_STABILITY");
    EXPECT_EQ(displayKey(FlightDataTypeGroup::THRUST_AND_DRAG),
              "FlightDataTypeGroup.GROUP_THRUST_AND_DRAG");
    EXPECT_EQ(displayKey(FlightDataTypeGroup::COEFFICIENTS),
              "FlightDataTypeGroup.GROUP_COEFFICIENTS");
    EXPECT_EQ(displayKey(FlightDataTypeGroup::ATMOSPHERIC_CONDITIONS),
              "FlightDataTypeGroup.GROUP_ATMOSPHERIC_CONDITIONS");
    EXPECT_EQ(displayKey(FlightDataTypeGroup::CHARACTERISTIC_NUMBERS),
              "FlightDataTypeGroup.GROUP_CHARACTERISTIC_NUMBERS");
    EXPECT_EQ(displayKey(FlightDataTypeGroup::REFERENCE_VALUES),
              "FlightDataTypeGroup.GROUP_REFERENCE_VALUES");
    EXPECT_EQ(displayKey(FlightDataTypeGroup::SIMULATION_INFORMATION),
              "FlightDataTypeGroup.GROUP_SIMULATION_INFORMATION");
    EXPECT_EQ(displayKey(FlightDataTypeGroup::CUSTOM), "FlightDataTypeGroup.GROUP_CUSTOM");
}

TEST(FlightDataTypeGroup, NamesAndKeysAreDistinct)
{
    std::set<std::string_view> names;
    std::set<std::string_view> keys;
    for (const FlightDataTypeGroup group : kAllFlightDataTypeGroups)
    {
        EXPECT_TRUE(names.insert(displayName(group)).second) << displayName(group);
        EXPECT_TRUE(keys.insert(displayKey(group)).second) << displayKey(group);
        EXPECT_EQ(displayKey(group).substr(0, 26), "FlightDataTypeGroup.GROUP_");
    }
}

/// compareTo(@p a, @p b) is Java's, and agrees with the enum's own operators.
void expectComparison(FlightDataTypeGroup a, FlightDataTypeGroup b)
{
    EXPECT_EQ(compareTo(a, b), priority(a) - priority(b));
    EXPECT_EQ(compareTo(a, b), -compareTo(b, a));
    // The enum's own order is the priority order, and equality is Java's equals().
    EXPECT_EQ(a < b, compareTo(a, b) < 0);
    EXPECT_EQ(a == b, compareTo(a, b) == 0);
}

TEST(FlightDataTypeGroup, CompareToIsThePriorityDifference)
{
    // Java: this.priority - o.priority.
    EXPECT_EQ(compareTo(FlightDataTypeGroup::TIME, FlightDataTypeGroup::POSITION_AND_MOTION), -10);
    EXPECT_EQ(compareTo(FlightDataTypeGroup::CUSTOM, FlightDataTypeGroup::TIME), 200);
    EXPECT_EQ(compareTo(FlightDataTypeGroup::CUSTOM, FlightDataTypeGroup::SIMULATION_INFORMATION),
              100);
    for (const FlightDataTypeGroup a : kAllFlightDataTypeGroups)
    {
        EXPECT_EQ(compareTo(a, a), 0);
        for (const FlightDataTypeGroup b : kAllFlightDataTypeGroups)
        {
            expectComparison(a, b);
        }
    }
    EXPECT_TRUE(std::ranges::is_sorted(kAllFlightDataTypeGroups,
                                       [](auto a, auto b) { return compareTo(a, b) < 0; }));
}

}  // namespace
