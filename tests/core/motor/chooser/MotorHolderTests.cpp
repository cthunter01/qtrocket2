#include "QtRocket/motor/chooser/MotorHolder.h"

#include <memory>
#include <utility>

#include <gtest/gtest.h>

#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"

// OpenRocket has no JUnit test of MotorHolder.

namespace
{

using QtRocket::Coordinate;
using QtRocket::InMemoryPreferences;
using QtRocket::MotorHolder;
using QtRocket::ThrustCurveMotor;

using MotorPtr = std::shared_ptr<const ThrustCurveMotor>;

MotorPtr namedMotor()
{
    QtRocket::Result<ThrustCurveMotor> built =
        ThrustCurveMotor::Builder()
            .setDesignation("H128W")
            .setCommonName("H128")
            .setLength(0.07)
            .setTimePoints({0, 1})
            .setThrustPoints({1, 0})
            .setCGPoints({Coordinate(0.01, 0, 0, 0.01), Coordinate(0.01, 0, 0, 0.01)})
            .build();
    EXPECT_TRUE(built.has_value());
    return built.has_value() ? std::make_shared<const ThrustCurveMotor>(std::move(*built))
                             : nullptr;
}

/// The text is the name the name column shows: the designation by default, else the common name.
TEST(MotorHolder, ShowsTheMotorsNameAsTheNameColumnSays)
{
    InMemoryPreferences preferences;
    const MotorHolder   holder(namedMotor(), 3);
    EXPECT_EQ(holder.getIndex(), 3U);
    EXPECT_EQ(holder.toString(preferences), "H128W");
    preferences.setMotorNameColumn(false);
    EXPECT_EQ(holder.toString(preferences), "H128");
}

/// Two holders are equal when they hold the same motor, whatever their index.
TEST(MotorHolder, ComparesTheMotorOnly)
{
    const MotorPtr motor = namedMotor();
    EXPECT_EQ(MotorHolder(motor, 0), MotorHolder(motor, 1));
    EXPECT_NE(MotorHolder(motor, 0), MotorHolder(namedMotor(), 0));
}

TEST(MotorHolder, RefusesANullMotor)
{
    EXPECT_THROW(MotorHolder(nullptr, 0), QtRocket::BugError);
}

}  // namespace
