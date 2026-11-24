#include "QtRocket/simulation/RunSample.h"

// Tests of the sample of a running simulation that the run dialog's progress listener publishes
// (an addition: Java publishes a copy of the whole status, of which SimulationRunDialog.java
// reads these four values).

#include <memory>

#include <gtest/gtest.h>

#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/util/Coordinate.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationStatusSupport.h"

namespace
{

using QtRocket::Coordinate;
using QtRocket::FlightConfiguration;
using QtRocket::RunSample;
using QtRocket::SimulationConditions;
using QtRocket::SimulationStatus;
using QtRocket::Test::statusConfiguration;
using QtRocket::Test::TestBeta;
using QtRocket::Test::testFcid;
using QtRocket::Test::TestStatus;

TEST(RunSample, OfAStatusIsItsTimeAltitudeVerticalVelocityAndSustainer)
{
    TestStatus fixture;
    fixture.status.setSimulationTime(3.5);
    fixture.status.setRocketPosition(Coordinate(1.0, 2.0, 120.25));
    fixture.status.setRocketVelocity(Coordinate(4.0, 5.0, -6.5));

    const RunSample sample = RunSample::of(fixture.status);
    EXPECT_EQ(sample.simulationTime, 3.5);
    EXPECT_EQ(sample.altitude, 120.25);
    EXPECT_EQ(sample.verticalVelocity, -6.5);
    EXPECT_TRUE(sample.sustainerActive);
    EXPECT_EQ(sample, (RunSample{.simulationTime   = 3.5,
                                 .altitude         = 120.25,
                                 .verticalVelocity = -6.5,
                                 .sustainerActive  = true}));
}

// The status of a booster's branch flies the booster alone (its configuration has only the
// booster's stage active, as the engine sets it when the stage separates).
TEST(RunSample, TheSustainerIsNotActiveInTheBranchOfAStageThatSeparated)
{
    TestBeta                                   beta;
    const std::shared_ptr<FlightConfiguration> booster =
        statusConfiguration(*beta.rocket, testFcid(1));
    booster->setOnlyStage(1);
    const SimulationStatus status(booster, std::make_shared<SimulationConditions>());
    EXPECT_FALSE(RunSample::of(status).sustainerActive);

    const std::shared_ptr<FlightConfiguration> both =
        statusConfiguration(*beta.rocket, testFcid(1));
    const SimulationStatus whole(both, std::make_shared<SimulationConditions>());
    EXPECT_TRUE(RunSample::of(whole).sustainerActive);
}

TEST(RunSample, SamplesAreEqualMemberByMember)
{
    const RunSample sample{
        .simulationTime = 1.0, .altitude = 2.0, .verticalVelocity = 3.0, .sustainerActive = true};
    RunSample other = sample;
    EXPECT_EQ(other, sample);
    other.sustainerActive = false;
    EXPECT_NE(other, sample);
    other                  = sample;
    other.verticalVelocity = -3.0;
    EXPECT_NE(other, sample);
    // Zero-initialised by default.
    EXPECT_EQ(RunSample{}, (RunSample{.simulationTime   = 0.0,
                                      .altitude         = 0.0,
                                      .verticalVelocity = 0.0,
                                      .sustainerActive  = false}));
}

}  // namespace
