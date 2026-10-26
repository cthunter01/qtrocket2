#include "QtRocket/motor/chooser/MotorSimilarityCache.h"

#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorCorrelation.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"

namespace
{

using QtRocket::Coordinate;
using QtRocket::Motor;
using QtRocket::MotorSimilarityCache;
using QtRocket::ThrustCurveMotor;

using MotorPtr = std::shared_ptr<const Motor>;

MotorPtr motor(std::vector<double> thrust)
{
    QtRocket::Result<ThrustCurveMotor> built =
        ThrustCurveMotor::Builder()
            .setLength(0.07)
            .setTimePoints({0, 1, 2})
            .setThrustPoints(std::move(thrust))
            .setCGPoints({Coordinate(0.01, 0, 0, 0.01), Coordinate(0.01, 0, 0, 0.01),
                          Coordinate(0.01, 0, 0, 0.01)})
            .build();
    EXPECT_TRUE(built.has_value());
    return built.has_value() ? std::make_shared<const ThrustCurveMotor>(std::move(*built))
                             : nullptr;
}

/// The value is MotorCorrelation's, kept once for both orders of a pair.
TEST(MotorSimilarityCache, KeepsOneValueForBothOrdersOfAPair)
{
    const MotorPtr       a = motor({0, 10, 0});
    const MotorPtr       b = motor({0, 6, 3});
    MotorSimilarityCache cache;
    const double         value = cache.similarity(a, b);
    EXPECT_EQ(value, QtRocket::MotorCorrelation::similarity(*a, *b));
    EXPECT_EQ(cache.size(), 1U);
    EXPECT_EQ(cache.similarity(b, a), value);
    EXPECT_EQ(cache.size(), 1U);
    EXPECT_EQ(cache.similarity(a, a), 1.0);
    EXPECT_EQ(cache.size(), 2U);
    cache.clear();
    EXPECT_EQ(cache.size(), 0U);
}

/// An entry keeps its motors, so that their addresses can name no other motors.
TEST(MotorSimilarityCache, KeepsTheMotorsOfAPair)
{
    MotorSimilarityCache       cache;
    std::weak_ptr<const Motor> watched;
    {
        const MotorPtr a = motor({0, 10, 0});
        watched          = a;
        static_cast<void>(cache.similarity(a, motor({0, 5, 0})));
    }
    EXPECT_FALSE(watched.expired());
    cache.clear();
    EXPECT_TRUE(watched.expired());
}

TEST(MotorSimilarityCache, RefusesANullMotor)
{
    MotorSimilarityCache cache;
    EXPECT_THROW(static_cast<void>(cache.similarity(nullptr, motor({0, 1, 0}))),
                 QtRocket::BugError);
}

}  // namespace
