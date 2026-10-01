#include "QtRocket/mass/ThrustState.h"

#include <optional>

#include <gtest/gtest.h>

namespace
{

using QtRocket::kAllThrustStates;
using QtRocket::ThrustState;

TEST(ThrustState, DeclarationOrderIsJavas)
{
    ASSERT_EQ(kAllThrustStates.size(), 4U);
    EXPECT_EQ(kAllThrustStates[0], ThrustState::SPENT);
    EXPECT_EQ(kAllThrustStates[1], ThrustState::DELAYING);
    EXPECT_EQ(kAllThrustStates[2], ThrustState::THRUSTING);
    EXPECT_EQ(kAllThrustStates[3], ThrustState::ARMED);
}

TEST(ThrustState, NamesAndDescriptions)
{
    EXPECT_EQ(name(ThrustState::SPENT), "Spent");
    EXPECT_EQ(name(ThrustState::DELAYING), "Delaying");
    EXPECT_EQ(name(ThrustState::THRUSTING), "Thrusting");
    EXPECT_EQ(name(ThrustState::ARMED), "Armed");

    EXPECT_EQ(description(ThrustState::SPENT), "Finished Producing thrust.");
    EXPECT_EQ(description(ThrustState::DELAYING), " After Burnout, but before ejection");
    EXPECT_EQ(description(ThrustState::THRUSTING), "Currently Producing thrust");
    EXPECT_EQ(description(ThrustState::ARMED), "Armed, but not yet lit.");
}

TEST(ThrustState, NextFollowsTheBurnSequence)
{
    EXPECT_EQ(nextState(ThrustState::ARMED), std::optional{ThrustState::THRUSTING});
    EXPECT_EQ(nextState(ThrustState::THRUSTING), std::optional{ThrustState::DELAYING});
    EXPECT_EQ(nextState(ThrustState::DELAYING), std::optional{ThrustState::SPENT});
    EXPECT_EQ(nextState(ThrustState::SPENT), std::nullopt);
}

TEST(ThrustState, SequenceNumbersCountDownFromTheEnd)
{
    static_assert(sequenceNumber(ThrustState::SPENT) == 10);
    EXPECT_EQ(sequenceNumber(ThrustState::SPENT), 10);
    EXPECT_EQ(sequenceNumber(ThrustState::DELAYING), 9);
    EXPECT_EQ(sequenceNumber(ThrustState::THRUSTING), 8);
    EXPECT_EQ(sequenceNumber(ThrustState::ARMED), 7);
}

TEST(ThrustState, BeforeAndAfter)
{
    EXPECT_TRUE(isBefore(ThrustState::ARMED, ThrustState::THRUSTING));
    EXPECT_TRUE(isBefore(ThrustState::ARMED, ThrustState::SPENT));
    EXPECT_FALSE(isBefore(ThrustState::SPENT, ThrustState::DELAYING));
    EXPECT_FALSE(isBefore(ThrustState::DELAYING, ThrustState::DELAYING));

    EXPECT_TRUE(isAfter(ThrustState::SPENT, ThrustState::DELAYING));
    EXPECT_TRUE(isAfter(ThrustState::THRUSTING, ThrustState::ARMED));
    EXPECT_FALSE(isAfter(ThrustState::ARMED, ThrustState::THRUSTING));
    EXPECT_FALSE(isAfter(ThrustState::THRUSTING, ThrustState::THRUSTING));
}

TEST(ThrustState, ThrustingAndSimulationFlags)
{
    EXPECT_FALSE(isThrusting(ThrustState::ARMED));
    EXPECT_TRUE(isThrusting(ThrustState::THRUSTING));
    EXPECT_FALSE(isThrusting(ThrustState::DELAYING));
    EXPECT_FALSE(isThrusting(ThrustState::SPENT));

    EXPECT_FALSE(needsSimulation(ThrustState::ARMED));
    EXPECT_TRUE(needsSimulation(ThrustState::THRUSTING));
    EXPECT_TRUE(needsSimulation(ThrustState::DELAYING));
    EXPECT_FALSE(needsSimulation(ThrustState::SPENT));
}

}  // namespace
