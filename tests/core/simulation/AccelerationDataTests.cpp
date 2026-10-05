#include "QtRocket/simulation/AccelerationData.h"

#include <limits>
#include <optional>
#include <type_traits>

#include <gtest/gtest.h>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Quaternion.h"
#include "simulation/SimulationTestSupport.h"

namespace
{

using QtRocket::AccelerationData;
using QtRocket::BugError;
using QtRocket::Coordinate;
using QtRocket::Quaternion;
using QtRocket::Test::bugText;

static_assert(std::is_copy_constructible_v<AccelerationData>);
static_assert(std::is_copy_assignable_v<AccelerationData>);

// The values of the Java probe (probes/events-data-impl/MiscProbe.java, "AccelerationData").
// The rotations are exact unit quaternions, and rotate() and invRotate() only add and multiply,
// so the results are exact on every platform.

/// A rotation about the y axis: w = 0.6, y = 0.8.
constexpr Quaternion kRotation{0.6, 0.0, 0.8, 0.0};
/// The rotation by a third of a turn about (1, 1, 1): it takes x to y, y to z and z to x.
constexpr Quaternion kCycle{0.5, 0.5, 0.5, 0.5};
constexpr Coordinate kLinear{1.5, -2.25, 3.125, 0.5};
constexpr Coordinate kRotational{-0.75, 4.5, 0.0625};

/// Expects every component of @p actual to be exactly that of @p expected.
void expectExactly(const Coordinate& actual, const Coordinate& expected)
{
    EXPECT_EQ(actual.x, expected.x);
    EXPECT_EQ(actual.y, expected.y);
    EXPECT_EQ(actual.z, expected.z);
    EXPECT_EQ(actual.weight, expected.weight);
}

TEST(AccelerationData, GivenInRocketCoordinatesItRotatesIntoWorldCoordinates)
{
    const AccelerationData data(kLinear, kRotational, std::nullopt, std::nullopt, kRotation);

    expectExactly(data.getLinearAccelerationRC(), kLinear);
    expectExactly(data.getRotationalAccelerationRC(), kRotational);
    expectExactly(data.getLinearAccelerationWC(),
                  Coordinate{2.58, -2.25, -2.3150000000000004, 0.5});
    expectExactly(data.getRotationalAccelerationWC(),
                  Coordinate{0.27000000000000013, 4.5, 0.7025, 0.0});
    // Asking again gives the same values.
    expectExactly(data.getLinearAccelerationWC(),
                  Coordinate{2.58, -2.25, -2.3150000000000004, 0.5});
    expectExactly(data.getLinearAccelerationRC(), kLinear);
}

TEST(AccelerationData, GivenInWorldCoordinatesItRotatesBackIntoRocketCoordinates)
{
    const AccelerationData data(std::nullopt, std::nullopt, kLinear, kRotational, kRotation);

    // NOLINTNEXTLINE(modernize-use-std-numbers): Java's result, not an approximation of a constant
    const Coordinate linearRC{-3.4200000000000004, -2.25, 0.5649999999999997, 0.5};
    expectExactly(data.getLinearAccelerationRC(), linearRC);
    expectExactly(data.getRotationalAccelerationRC(),
                  Coordinate{0.1500000000000002, 4.5, -0.7375, 0.0});
    expectExactly(data.getLinearAccelerationWC(), kLinear);
    expectExactly(data.getRotationalAccelerationWC(), kRotational);
}

TEST(AccelerationData, TheTwoAccelerationsMayComeInDifferentFrames)
{
    const AccelerationData data(kLinear, std::nullopt, std::nullopt, kRotational, kRotation);

    expectExactly(data.getLinearAccelerationRC(), kLinear);
    expectExactly(data.getLinearAccelerationWC(),
                  Coordinate{2.58, -2.25, -2.3150000000000004, 0.5});
    expectExactly(data.getRotationalAccelerationWC(), kRotational);
    expectExactly(data.getRotationalAccelerationRC(),
                  Coordinate{0.1500000000000002, 4.5, -0.7375, 0.0});
}

TEST(AccelerationData, WhatIsGivenInBothFramesIsNotConverted)
{
    // The world values are not the rotation of the rocket values: each getter returns what it
    // was given.
    const AccelerationData data(kLinear, kRotational, kRotational, kLinear, kRotation);

    expectExactly(data.getLinearAccelerationRC(), kLinear);
    expectExactly(data.getRotationalAccelerationRC(), kRotational);
    expectExactly(data.getLinearAccelerationWC(), kRotational);
    expectExactly(data.getRotationalAccelerationWC(), kLinear);
}

TEST(AccelerationData, RotatesFromRocketToWorldCoordinates)
{
    const AccelerationData data(Coordinate{1, 2, 3}, Coordinate{4, 5, 6}, std::nullopt,
                                std::nullopt, kCycle);
    expectExactly(data.getLinearAccelerationWC(), Coordinate{3.0, 1.0, 2.0, 0.0});
    expectExactly(data.getRotationalAccelerationWC(), Coordinate{6.0, 4.0, 5.0, 0.0});

    // And back.
    const AccelerationData back(std::nullopt, std::nullopt, Coordinate{3, 1, 2},
                                Coordinate{6, 4, 5}, kCycle);
    expectExactly(back.getLinearAccelerationRC(), Coordinate{1.0, 2.0, 3.0, 0.0});
    expectExactly(back.getRotationalAccelerationRC(), Coordinate{4.0, 5.0, 6.0, 0.0});
}

TEST(AccelerationData, KeepsTheRotation)
{
    const AccelerationData data(kLinear, kRotational, std::nullopt, std::nullopt, kRotation);
    EXPECT_EQ(data.getRotation(), kRotation);
    EXPECT_EQ(data.getRotation().toString(),
              "Quaternion[0.600000,0.000000,0.800000,0.000000,norm=1.000000]");
}

TEST(AccelerationData, AnAccelerationGivenInNoFrameIsABug)
{
    // Java: IllegalArgumentException with this message (two spaces after the colon).
    EXPECT_EQ(bugText([] {
                  const AccelerationData data(std::nullopt, kRotational, std::nullopt, kRotational,
                                              kCycle);
              }),
              "Parameter is null:  linearAccelerationRC=null linearAccelerationWC=null "
              "rotationalAccelerationRC=(-0.75000,4.50000,0.06250) "
              "rotationalAccelerationWC=(-0.75000,4.50000,0.06250) "
              "rotation=Quaternion[0.500000,0.500000,0.500000,0.500000,norm=1.000000]");
    EXPECT_EQ(bugText([] {
                  const AccelerationData data(kLinear, std::nullopt, kLinear, std::nullopt, kCycle);
              }),
              "Parameter is null:  linearAccelerationRC=(1.50000,-2.25000,3.12500,w=0.50000) "
              "linearAccelerationWC=(1.50000,-2.25000,3.12500,w=0.50000) "
              "rotationalAccelerationRC=null rotationalAccelerationWC=null "
              "rotation=Quaternion[0.500000,0.500000,0.500000,0.500000,norm=1.000000]");
    EXPECT_EQ(bugText([] {
                  const AccelerationData data(std::nullopt, std::nullopt, std::nullopt,
                                              std::nullopt, kCycle);
              }),
              "Parameter is null:  linearAccelerationRC=null linearAccelerationWC=null "
              "rotationalAccelerationRC=null rotationalAccelerationWC=null "
              "rotation=Quaternion[0.500000,0.500000,0.500000,0.500000,norm=1.000000]");
    EXPECT_THROW(static_cast<void>(AccelerationData(std::nullopt, kRotational, std::nullopt,
                                                    std::nullopt, kCycle)),
                 BugError);
    EXPECT_THROW(static_cast<void>(
                     AccelerationData(kLinear, std::nullopt, std::nullopt, std::nullopt, kCycle)),
                 BugError);
}

TEST(AccelerationData, EqualsComparesTheRocketCoordinates)
{
    const AccelerationData  data(kLinear, kRotational, std::nullopt, std::nullopt, kRotation);
    const AccelerationData& self = data;
    EXPECT_TRUE(data == self);
    EXPECT_TRUE(data ==
                AccelerationData(kLinear, kRotational, std::nullopt, std::nullopt, kRotation));
    EXPECT_TRUE(data == AccelerationData(kLinear, kRotational, std::nullopt, std::nullopt, kCycle))
        << "the rotation takes part only through the conversion";
    EXPECT_TRUE(data == AccelerationData(std::nullopt, std::nullopt, data.getLinearAccelerationWC(),
                                         data.getRotationalAccelerationWC(), kRotation))
        << "the same accelerations given in world coordinates";
    EXPECT_FALSE(data ==
                 AccelerationData(std::nullopt, std::nullopt, kLinear, kRotational, kRotation));
    EXPECT_FALSE(data ==
                 AccelerationData(kRotational, kRotational, std::nullopt, std::nullopt, kRotation));
    EXPECT_FALSE(data == AccelerationData(kLinear, kLinear, std::nullopt, std::nullopt, kRotation));
}

TEST(AccelerationData, EqualsIsCoordinatesTolerantEquality)
{
    const AccelerationData data(kLinear, kRotational, std::nullopt, std::nullopt, kRotation);
    EXPECT_TRUE(data == AccelerationData(Coordinate{1.5 * (1 + 1e-9), -2.25, 3.125, 0.5},
                                         kRotational, std::nullopt, std::nullopt, kRotation));
    EXPECT_FALSE(data == AccelerationData(Coordinate{1.5, -2.25, 3.125, 0.75}, kRotational,
                                          std::nullopt, std::nullopt, kRotation))
        << "the weight takes part";
}

TEST(AccelerationData, DataWithANaNEqualsOnlyItself)
{
    const AccelerationData  nan(Coordinate::kNaN, kRotational, std::nullopt, std::nullopt,
                                kRotation);
    const AccelerationData& self = nan;
    EXPECT_TRUE(nan == self) << "the same object";
    EXPECT_FALSE(nan == AccelerationData(Coordinate::kNaN, kRotational, std::nullopt, std::nullopt,
                                         kRotation));
    const AccelerationData copy(nan);
    EXPECT_FALSE(nan == copy);
}

TEST(AccelerationData, ACopyKeepsWhatWasConverted)
{
    const AccelerationData data(kLinear, kRotational, std::nullopt, std::nullopt, kRotation);
    const Coordinate       world = data.getLinearAccelerationWC();
    const AccelerationData copy(data);
    expectExactly(copy.getLinearAccelerationWC(), world);
    expectExactly(copy.getRotationalAccelerationWC(), data.getRotationalAccelerationWC());
    EXPECT_TRUE(copy == data);
}

TEST(AccelerationData, InfiniteAndNaNValuesPassThrough)
{
    const double           inf = std::numeric_limits<double>::infinity();
    const AccelerationData data(Coordinate{inf, 0, 0}, Coordinate{0, 0, 0}, std::nullopt,
                                std::nullopt, Quaternion{});
    EXPECT_EQ(data.getLinearAccelerationRC().x, inf);
    // The identity rotation multiplies the infinity by zero in the other components.
    EXPECT_TRUE(data.getLinearAccelerationWC().isNaN());
    expectExactly(data.getRotationalAccelerationWC(), Coordinate{0, 0, 0});
}

}  // namespace
