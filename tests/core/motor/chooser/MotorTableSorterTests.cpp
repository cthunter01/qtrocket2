#include "QtRocket/motor/chooser/MotorTableSorter.h"

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorColumns.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorDatabaseModel.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "unit/DefaultUnitsGuard.h"

// The JDK's DefaultRowSorter has no JUnit test in OpenRocket; the keys of the chooser's header
// clicks are pinned in motor_chooser_golden_tests.cpp (clickName, clickNameAgain, ...).

namespace
{

using QtRocket::Coordinate;
using QtRocket::InMemoryPreferences;
using QtRocket::Manufacturer;
using QtRocket::MotorTableSorter;
using QtRocket::ThrustCurveMotor;
using QtRocket::ThrustCurveMotorColumns;
using QtRocket::ThrustCurveMotorDatabaseModel;
using QtRocket::ThrustCurveMotorSet;
using QtRocket::Test::DefaultUnitsGuard;

using enum ThrustCurveMotorColumns;
using Key  = MotorTableSorter::SortKey;
using Keys = std::vector<Key>;
using Rows = std::vector<std::size_t>;

constexpr auto kAscending  = MotorTableSorter::SortOrder::ASCENDING;
constexpr auto kDescending = MotorTableSorter::SortOrder::DESCENDING;

/// A sort key.
Key key(ThrustCurveMotorColumns column, MotorTableSorter::SortOrder order)
{
    return {.column = column, .sortOrder = order};
}

ThrustCurveMotorSet setOf(const std::string& manufacturer, std::string designation, double diameter)
{
    QtRocket::Result<ThrustCurveMotor> built =
        ThrustCurveMotor::Builder()
            .setManufacturer(Manufacturer::getManufacturer(manufacturer))
            .setDesignation(std::move(designation))
            .setDiameter(diameter)
            .setLength(0.07)
            .setTimePoints({0, 1})
            .setThrustPoints({1, 0})
            .setCGPoints({Coordinate(0.01, 0, 0, 0.01), Coordinate(0.01, 0, 0, 0.01)})
            .build();
    EXPECT_TRUE(built.has_value());
    ThrustCurveMotorSet set;
    if (built.has_value())
    {
        set.addMotor(std::make_shared<const ThrustCurveMotor>(std::move(*built)));
    }
    return set;
}

TEST(MotorTableSorter, StartsWithTheChoosersKeys)
{
    const MotorTableSorter sorter;
    EXPECT_EQ(sorter.getSortKeys(), (Keys{key(DIAMETER, kAscending), key(TOTAL_IMPULSE, kAscending),
                                          key(MANUFACTURER, kAscending)}));
}

/// The primary key toggles; another column becomes the primary key, ascending, its old key
/// removed; the keys are cut to three.
TEST(MotorTableSorter, TogglesAsAHeaderClickDoes)
{
    MotorTableSorter sorter;
    sorter.toggleSortOrder(NAME);
    EXPECT_EQ(sorter.getSortKeys(), (Keys{key(NAME, kAscending), key(DIAMETER, kAscending),
                                          key(TOTAL_IMPULSE, kAscending)}));
    sorter.toggleSortOrder(NAME);
    EXPECT_EQ(sorter.getSortKeys().front(), key(NAME, kDescending));
    sorter.toggleSortOrder(NAME);
    EXPECT_EQ(sorter.getSortKeys().front(), key(NAME, kAscending));
    sorter.toggleSortOrder(NAME);
    sorter.toggleSortOrder(TOTAL_IMPULSE);
    EXPECT_EQ(sorter.getSortKeys(), (Keys{key(TOTAL_IMPULSE, kAscending), key(NAME, kDescending),
                                          key(DIAMETER, kAscending)}));
}

TEST(MotorTableSorter, TakesAllTheKeysItIsGiven)
{
    MotorTableSorter sorter;
    const Keys four{key(LENGTH, kDescending), key(CASEINFO, kAscending), key(NAME, kAscending),
                    key(MANUFACTURER, kAscending)};
    sorter.setSortKeys(four);
    EXPECT_EQ(sorter.getSortKeys(), four);
    sorter.toggleSortOrder(LENGTH);
    EXPECT_EQ(sorter.getSortKeys().size(), 3U);
    EXPECT_EQ(sorter.getSortKeys().front(), key(LENGTH, kAscending));
    sorter.setSortKeys({});
    sorter.toggleSortOrder(CASEINFO);
    EXPECT_EQ(sorter.getSortKeys(), (Keys{key(CASEINFO, kAscending)}));
}

/// The keys in turn, a descending key reversed, then the model's order.
TEST(MotorTableSorter, SortsByTheKeysThenByTheModelOrder)
{
    const DefaultUnitsGuard             guard;
    const InMemoryPreferences           preferences;
    const ThrustCurveMotorSet           a = setOf("Estes", "C6", 0.018);
    const ThrustCurveMotorSet           b = setOf("AeroTech", "C6", 0.018);
    const ThrustCurveMotorSet           c = setOf("Estes", "A8", 0.018);
    const ThrustCurveMotorSet           d = setOf("Estes", "A8", 0.024);
    const ThrustCurveMotorDatabaseModel model({&a, &b, &c, &d});
    MotorTableSorter                    sorter;
    const std::array<std::size_t, 4>    all{3, 2, 1, 0};
    // Diameter, total impulse (all 1 Ns... the same), manufacturer: AeroTech first, then model
    // order among the Estes of 18 mm.
    EXPECT_EQ(sorter.sort(all, model, preferences), (Rows{1, 0, 2, 3}));
    sorter.setSortKeys(std::array{key(NAME, kDescending)});
    EXPECT_EQ(sorter.sort(all, model, preferences), (Rows{0, 1, 2, 3}));
    EXPECT_EQ(sorter.compare(2, 2, model, preferences), 0);
    EXPECT_LT(sorter.compare(2, 3, model, preferences), 0);
    EXPECT_THROW(static_cast<void>(sorter.compare(0, 4, model, preferences)), QtRocket::BugError);
}

}  // namespace
