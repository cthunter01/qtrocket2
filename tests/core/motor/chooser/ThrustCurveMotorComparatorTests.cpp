#include "QtRocket/motor/chooser/ThrustCurveMotorComparator.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"

// OpenRocket has no JUnit test of ThrustCurveMotorComparator.

namespace
{

using QtRocket::Coordinate;
using QtRocket::ThrustCurveMotor;
using QtRocket::ThrustCurveMotorComparator;

using MotorPtr = std::shared_ptr<const ThrustCurveMotor>;

/// A motor of @p points time points and the description @p description.
MotorPtr motor(std::size_t points, std::string description)
{
    std::vector<double>     time;
    std::vector<double>     thrust;
    std::vector<Coordinate> cg;
    for (std::size_t i = 0; i < points; i++)
    {
        time.push_back(static_cast<double>(i));
        thrust.push_back(i + 1 < points ? 1.0 : 0.0);
        cg.emplace_back(0.01, 0, 0, 0.01);
    }
    QtRocket::Result<ThrustCurveMotor> built = ThrustCurveMotor::Builder()
                                                   .setLength(0.07)
                                                   .setTimePoints(std::move(time))
                                                   .setThrustPoints(std::move(thrust))
                                                   .setCGPoints(std::move(cg))
                                                   .setDescription(std::move(description))
                                                   .build();
    EXPECT_TRUE(built.has_value());
    return built.has_value() ? std::make_shared<const ThrustCurveMotor>(std::move(*built))
                             : nullptr;
}

/// Ten per time point, plus the description's UTF-16 length up to 100.
TEST(ThrustCurveMotorComparator, CountsPointsAndDescription)
{
    EXPECT_EQ(ThrustCurveMotorComparator::calculateGoodness(*motor(2, "")), 20);
    EXPECT_EQ(ThrustCurveMotorComparator::calculateGoodness(*motor(5, "abc")), 53);
    EXPECT_EQ(ThrustCurveMotorComparator::calculateGoodness(*motor(3, std::string(150, 'x'))), 130);
    // U+00E9 is one UTF-16 unit, U+1F680 two.
    EXPECT_EQ(ThrustCurveMotorComparator::calculateGoodness(*motor(2, "\xC3\xA9")), 21);
    EXPECT_EQ(ThrustCurveMotorComparator::calculateGoodness(*motor(2, "\xF0\x9F\x9A\x80")), 22);
}

/// The better curve first: compare() is goodness(o2) - goodness(o1).
TEST(ThrustCurveMotorComparator, PutsTheBetterCurveFirst)
{
    const MotorPtr many = motor(10, "");
    const MotorPtr few  = motor(3, "a description");
    EXPECT_EQ(ThrustCurveMotorComparator::compare(*many, *few), 43 - 100);
    EXPECT_EQ(ThrustCurveMotorComparator::compare(*few, *many), 100 - 43);
    EXPECT_EQ(ThrustCurveMotorComparator::compare(*few, *few), 0);
    EXPECT_TRUE(ThrustCurveMotorComparator{}(many, few));
    EXPECT_FALSE(ThrustCurveMotorComparator{}(few, many));
}

/// A stable sort keeps curves of equal goodness in their order, as Java's List.sort does.
TEST(ThrustCurveMotorComparator, SortsStably)
{
    const MotorPtr        first  = motor(2, "ab");
    const MotorPtr        best   = motor(4, "");
    const MotorPtr        second = motor(2, "cd");
    std::vector<MotorPtr> motors{first, best, second};
    std::ranges::stable_sort(motors, ThrustCurveMotorComparator{});
    EXPECT_EQ(motors, (std::vector<MotorPtr>{best, first, second}));
}

TEST(ThrustCurveMotorComparator, RefusesANullMotor)
{
    EXPECT_THROW(static_cast<void>(ThrustCurveMotorComparator{}(nullptr, motor(2, ""))),
                 QtRocket::BugError);
}

}  // namespace
