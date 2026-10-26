#include "QtRocket/motor/chooser/ThrustCurveMotorDatabaseModel.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorColumns.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"

// OpenRocket has no JUnit test of ThrustCurveMotorDatabaseModel.

namespace
{

using QtRocket::Coordinate;
using QtRocket::InMemoryPreferences;
using QtRocket::Manufacturer;
using QtRocket::ThrustCurveMotor;
using QtRocket::ThrustCurveMotorColumns;
using QtRocket::ThrustCurveMotorDatabaseModel;
using QtRocket::ThrustCurveMotorSet;

ThrustCurveMotorSet setOf(std::string designation)
{
    QtRocket::Result<ThrustCurveMotor> built =
        ThrustCurveMotor::Builder()
            .setManufacturer(Manufacturer::getManufacturer("Estes"))
            .setDesignation(std::move(designation))
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

TEST(ThrustCurveMotorDatabaseModel, ListsTheSetsInOrder)
{
    const ThrustCurveMotorSet     a = setOf("A8");
    const ThrustCurveMotorSet     b = setOf("B6");
    ThrustCurveMotorDatabaseModel model({&b, &a});
    const InMemoryPreferences     preferences;
    EXPECT_EQ(model.getRowCount(), 2U);
    EXPECT_EQ(ThrustCurveMotorDatabaseModel::getColumnCount(), 6U);
    EXPECT_EQ(&model.getMotorSet(0), &b);
    EXPECT_EQ(model.getValueText(1, ThrustCurveMotorColumns::NAME, preferences), "A8");
    EXPECT_EQ(model.getValueText(0, ThrustCurveMotorColumns::MANUFACTURER, preferences), "Estes");
    model.setDatabase({&a});
    EXPECT_EQ(model.getRowCount(), 1U);
    EXPECT_EQ(model.getMotorSets().front(), &a);
}

/// getIndex() finds the set itself, not an equal one.
TEST(ThrustCurveMotorDatabaseModel, FindsASetByIdentity)
{
    const ThrustCurveMotorSet           a    = setOf("A8");
    const ThrustCurveMotorSet           twin = setOf("A8");
    const ThrustCurveMotorDatabaseModel model({&a, &a});
    EXPECT_EQ(model.getIndex(a), std::optional<std::size_t>(0));
    EXPECT_EQ(model.getIndex(twin), std::nullopt);
}

TEST(ThrustCurveMotorDatabaseModel, RefusesANullSetAndARowBeyondTheEnd)
{
    const ThrustCurveMotorSet a = setOf("A8");
    EXPECT_THROW(ThrustCurveMotorDatabaseModel({&a, nullptr}), QtRocket::BugError);
    const ThrustCurveMotorDatabaseModel model({&a});
    EXPECT_THROW(static_cast<void>(model.getMotorSet(1)), QtRocket::BugError);
}

}  // namespace
