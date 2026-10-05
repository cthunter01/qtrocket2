#include "QtRocket/simulation/FlightData.h"

#include <array>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/util/BugError.h"
#include "simulation/SimulationTestSupport.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BugError;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightEvent;
using QtRocket::Rocket;
using QtRocket::Warning;
using QtRocket::WarningSet;
using QtRocket::Test::bugText;
using QtRocket::Test::EventTestRocket;
using QtRocket::Test::expectSame;
using Id   = QtRocket::FlightDataTypeId;
using Type = QtRocket::FlightEvent::Type;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

static_assert(!std::is_copy_constructible_v<FlightData>);
static_assert(!std::is_copy_assignable_v<FlightData>);
static_assert(std::is_nothrow_move_constructible_v<FlightData>);
static_assert(std::is_nothrow_move_assignable_v<FlightData>);

[[nodiscard]] const FlightDataType& type(Id id)
{
    return FlightDataType::builtin(id);
}

/// The ten summary values of @p data, in the order of FlightData's constructor.
[[nodiscard]] std::vector<double> summary(const FlightData& data)
{
    return {data.getMaxAltitude(),       data.getMaxVelocity(),       data.getMaxAcceleration(),
            data.getMaxMachNumber(),     data.getTimeToApogee(),      data.getFlightTime(),
            data.getGroundHitVelocity(), data.getLaunchRodVelocity(), data.getDeploymentVelocity(),
            data.getOptimumDelay()};
}

/// A branch named @p name with the types @p ids and no rows.
[[nodiscard]] std::shared_ptr<FlightDataBranch> branchOf(const std::string&  name,
                                                         std::span<const Id> ids)
{
    std::vector<const FlightDataType*> types;
    for (const Id id : ids)
    {
        types.push_back(&type(id));
    }
    return std::make_shared<FlightDataBranch>(name, types);
}

/// Java's createFlightDataBranch(): a branch named @p name with the one type @p id and a row
/// for each of @p values.
[[nodiscard]] std::shared_ptr<FlightDataBranch> createFlightDataBranch(
    const std::string& name, Id id, std::span<const double> values)
{
    const std::array<Id, 1>           ids{id};
    std::shared_ptr<FlightDataBranch> branch = branchOf(name, ids);
    for (const double value : values)
    {
        branch->addPoint();
        branch->setValue(type(id), value);
    }
    return branch;
}

/// The branch of the Java probe (MiscProbe.java, fullBranch()): eight rows of time, altitude,
/// velocity, acceleration and Mach number.
[[nodiscard]] std::shared_ptr<FlightDataBranch> fullBranch()
{
    const std::array<Id, 5>           ids{Id::TYPE_TIME, Id::TYPE_ALTITUDE, Id::TYPE_VELOCITY_TOTAL,
                                          Id::TYPE_ACCELERATION_TOTAL, Id::TYPE_MACH_NUMBER};
    std::shared_ptr<FlightDataBranch> branch = branchOf("Full", ids);
    const std::array<double, 8>       time{0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5};
    const std::array<double, 8>       altitude{0.0, 3.0, 11.0, 18.0, 18.0, 12.0, 5.0, 0.0};
    const std::array<double, 8>       velocity{0.0, 12.0, 20.0, 9.0, 1.0, 13.0, 15.0, 4.0};
    const std::array<double, 8>       acceleration{5.0, 30.0, 12.0, 9.5, 40.0, 60.0, 3.0, 80.0};
    for (std::size_t i = 0; i < time.size(); i++)
    {
        branch->addPoint();
        branch->setValue(type(Id::TYPE_TIME), time.at(i));
        branch->setValue(type(Id::TYPE_ALTITUDE), altitude.at(i));
        branch->setValue(type(Id::TYPE_VELOCITY_TOTAL), velocity.at(i));
        branch->setValue(type(Id::TYPE_ACCELERATION_TOTAL), acceleration.at(i));
        branch->setValue(type(Id::TYPE_MACH_NUMBER), velocity.at(i) / 320.0);
    }
    return branch;
}

/// fullBranch() with the events of the probe's "full" case.
[[nodiscard]] std::shared_ptr<FlightDataBranch> fullBranchWithEvents(const EventTestRocket& r)
{
    std::shared_ptr<FlightDataBranch> full = fullBranch();
    full->addEvent({Type::LAUNCHROD, 0.125});
    full->addEvent({Type::BURNOUT, 0.75, r.sustainerMount, r.state});
    full->addEvent({Type::BURNOUT, 1.25, r.sustainerMount, r.state});
    full->addEvent({Type::RECOVERY_DEVICE_DEPLOYMENT, 2.75, r.chute});
    full->addEvent({Type::RECOVERY_DEVICE_DEPLOYMENT, 2.25, r.chute});
    full->addEvent({Type::LAUNCHROD, 0.375});
    full->addEvent({Type::GROUND_HIT, 3.5});
    full->setTimeToOptimumAltitude(1.75);
    return full;
}

// ====================================================================== FlightDataTest.java

/// FlightDataTest.testFlightData
TEST(FlightDataTest, FlightData)
{
    FlightData data;

    const WarningSet& warnings = data.getWarningSet();
    EXPECT_TRUE(warnings.empty());

    EXPECT_EQ(0U, data.getBranchCount());
    expectSame(data.getDeploymentVelocity(), kNaN);
    expectSame(data.getFlightTime(), kNaN);
    expectSame(data.getGroundHitVelocity(), kNaN);
    expectSame(data.getLaunchRodVelocity(), kNaN);
    expectSame(data.getMaxAcceleration(), kNaN);
    expectSame(data.getMaxAltitude(), kNaN);
    expectSame(data.getMaxMachNumber(), kNaN);
    expectSame(data.getTimeToApogee(), kNaN);
}

/// FlightDataTest.testFlightDataFromSummaryData
TEST(FlightDataTest, FlightDataFromSummaryData)
{
    const double deploymentVelocity = 14.8;
    const double flightTime         = 69.1;
    const double groundHitVelocity  = 3.4;
    const double launchRodVelocity  = 17.5;
    const double maxAcceleration    = 156.2;
    const double maxVelocity        = 105.9;
    const double maxAltitude        = 355.1;
    const double maxMachNumber      = 0.31;
    const double timeToApogee       = 7.96;
    const double optimumDelay       = 5.2;

    const FlightData data(maxAltitude, maxVelocity, maxAcceleration, maxMachNumber, timeToApogee,
                          flightTime, groundHitVelocity, launchRodVelocity, deploymentVelocity,
                          optimumDelay);

    const WarningSet& warnings = data.getWarningSet();
    EXPECT_TRUE(warnings.empty());

    EXPECT_EQ(0U, data.getBranchCount());
    EXPECT_EQ(deploymentVelocity, data.getDeploymentVelocity());
    EXPECT_EQ(flightTime, data.getFlightTime());
    EXPECT_EQ(groundHitVelocity, data.getGroundHitVelocity());
    EXPECT_EQ(launchRodVelocity, data.getLaunchRodVelocity());
    EXPECT_EQ(maxAcceleration, data.getMaxAcceleration());
    EXPECT_EQ(maxAltitude, data.getMaxAltitude());
    EXPECT_EQ(maxMachNumber, data.getMaxMachNumber());
    EXPECT_EQ(timeToApogee, data.getTimeToApogee());
    EXPECT_EQ(optimumDelay, data.getOptimumDelay());
}

/// FlightDataTest.testFlightDataFlightDataBranchArray
TEST(FlightDataTest, FlightDataFlightDataBranchArray)
{
    const std::array<Id, 1> timeOnly{Id::TYPE_TIME};
    const FlightData        one{branchOf("Test", timeOnly)};

    EXPECT_TRUE(one.getWarningSet().empty());
    EXPECT_EQ(1U, one.getBranchCount());

    const FlightData two{branchOf("Test 1", timeOnly), branchOf("Test 2", timeOnly)};

    EXPECT_TRUE(two.getWarningSet().empty());
    EXPECT_EQ(2U, two.getBranchCount());
}

/// FlightDataTest.testGetMaxAltitudeCalculated
TEST(FlightDataTest, GetMaxAltitudeCalculated)
{
    const std::array<double, 5> altitudes{10.5, 37.771, 37.5, 5.1, 0.0};
    const FlightData data{createFlightDataBranch("Test Max Alt", Id::TYPE_ALTITUDE, altitudes)};

    EXPECT_EQ(37.771, data.getMaxAltitude());
}

/// FlightDataTest.testGetMaxVelocityCalculated
TEST(FlightDataTest, GetMaxVelocityCalculated)
{
    const std::array<double, 5> velocities{10.5, 23.7, 35.5, 30.1, 0.0};
    const FlightData            data{
        createFlightDataBranch("Test Max Velocity", Id::TYPE_VELOCITY_TOTAL, velocities)};

    EXPECT_EQ(35.5, data.getMaxVelocity());
}

/// FlightDataTest.testGetMaxMachNumberCalculated
TEST(FlightDataTest, GetMaxMachNumberCalculated)
{
    const std::array<double, 5> machs{0.1, 0.2, 0.333, 0.3, 0.1};
    const FlightData data{createFlightDataBranch("Test Max Mach", Id::TYPE_MACH_NUMBER, machs)};

    EXPECT_EQ(0.333, data.getMaxMachNumber());
}

/// FlightDataTest.testGetFlightTime
TEST(FlightDataTest, GetFlightTime)
{
    const std::array<double, 5> times{1.0, 5.0, 15.0, 20.1, 30.2};

    // Flight time is calculated as the last time entry
    const FlightData data{createFlightDataBranch("Test Flight Time", Id::TYPE_TIME, times)};
    EXPECT_EQ(30.2, data.getFlightTime());
}

/// The branch of FlightDataTest.testGetGroundHitVelocity: a flight profile with data logged
/// every second, the first row at 1 s, and an event in every row.
[[nodiscard]] std::shared_ptr<FlightDataBranch> groundHitProfile()
{
    const std::array<double, 21> velocities{// launch to burn out
                                            1.2, 15.0, 31.1, 45.6,
                                            // burn out to apogee
                                            33.6, 21.2, 14.1, 6.8, 0.0,
                                            // apogee to ejection charge
                                            2.4, 4.7, 8.6, 9.23,
                                            // ejection charge to parachute deployment
                                            7.1, 6.2, 6.2, 6.2, 6.2,
                                            // parachute deployment to ground hit
                                            6.2, 0.0, 0.0};
    const std::array<double, 21> altitudes{// launch to burn out
                                           1.2, 16.2, 47.3, 92.9,
                                           // burn out to apogee
                                           126.5, 147.7, 161.8, 168.6, 168.6,
                                           // apogee to ejection charge
                                           166.2, 161.5, 152.9, 143.67,
                                           // ejection charge to parachute deployment
                                           136.57, 113.81, 91.05, 68.29, 45.53,
                                           // parachute deployment to ground hit
                                           22.77, 0.0, 0.0};
    const std::array<Type, 21>   eventTypes{
        // launch to burn out
        Type::LIFTOFF, Type::LAUNCHROD, Type::ALTITUDE, Type::BURNOUT,
        // burn out to apogee
        Type::ALTITUDE, Type::ALTITUDE, Type::ALTITUDE, Type::ALTITUDE, Type::APOGEE,
        // apogee to ejection charge
        Type::ALTITUDE, Type::ALTITUDE, Type::ALTITUDE, Type::EJECTION_CHARGE,
        // ejection charge to parachute deployment
        Type::ALTITUDE, Type::RECOVERY_DEVICE_DEPLOYMENT, Type::ALTITUDE, Type::ALTITUDE,
        Type::ALTITUDE,
        // parachute deployment to ground hit
        Type::GROUND_HIT, Type::ALTITUDE, Type::SIMULATION_END};

    // This flight data branch only needs to record for time, altitude and velocity.
    const std::array<Id, 3> ids{Id::TYPE_TIME, Id::TYPE_ALTITUDE, Id::TYPE_VELOCITY_TOTAL};
    std::shared_ptr<FlightDataBranch> branch = branchOf("Ground Hit Velocities", ids);
    for (std::size_t i = 0; i < velocities.size(); i++)
    {
        branch->addPoint();
        // the data entries are 1 second ahead of the index
        const double time = static_cast<double>(i) + 1.0;
        branch->setValue(type(Id::TYPE_TIME), time);
        branch->setValue(type(Id::TYPE_ALTITUDE), altitudes.at(i));
        branch->setValue(type(Id::TYPE_VELOCITY_TOTAL), velocities.at(i));
        branch->addEvent({eventTypes.at(i), time});
    }
    return branch;
}

/// FlightDataTest.testGetGroundHitVelocity
TEST(FlightDataTest, GetGroundHitVelocity)
{
    const FlightData data{groundHitProfile()};

    EXPECT_EQ(6.2, data.getGroundHitVelocity());
}

/// The other values the same flight gives (not checked in Java's test).
TEST(FlightDataTest, GetGroundHitVelocityProfileGivesTheOtherValuesToo)
{
    const FlightData data{groundHitProfile()};

    EXPECT_EQ(data.getMaxAltitude(), 168.6);
    EXPECT_EQ(data.getMaxVelocity(), 45.6);
    EXPECT_EQ(data.getTimeToApogee(), 8.0) << "the first of the two rows at 168.6 m";
    EXPECT_EQ(data.getFlightTime(), 21.0);
    EXPECT_EQ(data.getLaunchRodVelocity(), 15.0);
    EXPECT_EQ(data.getDeploymentVelocity(), 6.2);
    expectSame(data.getMaxAcceleration(), kNaN);
    expectSame(data.getMaxMachNumber(), kNaN);
    expectSame(data.getOptimumDelay(), kNaN);
}

// =============================================================== calculateInterestingValues()

// The expectations of this section are the values of the Java probe (MiscProbe.java,
// "FlightData"), in the order {max altitude, max velocity, max acceleration, max Mach number,
// time to apogee, flight time, ground hit velocity, launch rod velocity, deployment velocity,
// optimum delay}. The computation only compares, subtracts, multiplies and divides.

TEST(FlightDataValues, AFullBranch)
{
    const EventTestRocket r;
    const FlightData      data{fullBranchWithEvents(r)};
    // The first of two rows at the maximum altitude; the maximum acceleration before the
    // earlier deployment (2.25 s); the launch rod and deployment velocities of the last event
    // of each type; the optimum delay after the last burnout.
    expectSame(summary(data), {18.0, 20.0, 40.0, 0.0625, 1.5, 3.5, 4.0, 9.0, 7.0, 0.5});
}

TEST(FlightDataValues, EventsOutsideTheData)
{
    const EventTestRocket             r;
    std::shared_ptr<FlightDataBranch> outside = fullBranch();
    outside->addEvent({Type::GROUND_HIT, 3.75});
    outside->addEvent({Type::RECOVERY_DEVICE_DEPLOYMENT, -0.5, r.chute});
    outside->addEvent({Type::LAUNCHROD, 3.5 + 5e-9});
    const FlightData data{std::move(outside)};
    // A deployment before the first row leaves no row for the maximum acceleration: 0. A time
    // within EPSILON after the last row is extrapolated; beyond that, and before the first row,
    // the velocity is NaN.
    expectSame(summary(data),
               {18.0, 20.0, 0.0, 0.0625, 1.5, 3.5, kNaN, 3.9999998900000016, kNaN, kNaN});
}

TEST(FlightDataValues, WithoutEvents)
{
    const FlightData data{fullBranch()};
    expectSame(summary(data), {18.0, 20.0, 80.0, 0.0625, 1.5, 3.5, kNaN, kNaN, kNaN, kNaN});
}

TEST(FlightDataValues, OnlyTheFirstBranchCounts)
{
    std::shared_ptr<FlightDataBranch> second = fullBranch();
    second->addEvent({Type::GROUND_HIT, 1.0});
    const FlightData data{fullBranch(), std::move(second)};
    expectSame(summary(data), {18.0, 20.0, 80.0, 0.0625, 1.5, 3.5, kNaN, kNaN, kNaN, kNaN});
}

TEST(FlightDataValues, WithoutAnAltitudeColumnTheRestIsNotComputed)
{
    const std::array<Id, 3>           ids{Id::TYPE_TIME, Id::TYPE_VELOCITY_TOTAL,
                                          Id::TYPE_ACCELERATION_TOTAL};
    std::shared_ptr<FlightDataBranch> noAltitude = branchOf("NoAlt", ids);
    for (int i = 0; i < 3; i++)
    {
        noAltitude->addPoint();
        noAltitude->setValue(type(Id::TYPE_TIME), i);
        noAltitude->setValue(type(Id::TYPE_VELOCITY_TOTAL), 2.0 * i);
        noAltitude->setValue(type(Id::TYPE_ACCELERATION_TOTAL), 7.0);
    }
    noAltitude->addEvent({Type::GROUND_HIT, 1.5});
    noAltitude->addEvent({Type::BURNOUT, 0.5});
    noAltitude->setTimeToOptimumAltitude(2.0);
    const FlightData data{std::move(noAltitude)};
    // No ground hit velocity, no optimum delay and no maximum acceleration, although the
    // branch could give them.
    expectSame(summary(data), {kNaN, 4.0, kNaN, kNaN, kNaN, 2.0, kNaN, kNaN, kNaN, kNaN});
}

TEST(FlightDataValues, WithoutATimeColumn)
{
    const std::array<double, 1> altitude{3.0};
    const FlightData            data{createFlightDataBranch("NoTime", Id::TYPE_ALTITUDE, altitude)};
    expectSame(summary(data), {3.0, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN});
}

/// The probe's "sparse" branch: time and altitude only, the maximum altitude met within
/// EPSILON one row early.
[[nodiscard]] std::shared_ptr<FlightDataBranch> sparseBranch()
{
    const std::array<Id, 2>           ids{Id::TYPE_TIME, Id::TYPE_ALTITUDE};
    std::shared_ptr<FlightDataBranch> sparse = branchOf("Sparse", ids);
    const std::array<double, 4>       altitude{1.0, 5.0, 5.00000001, 2.0};
    for (std::size_t i = 0; i < altitude.size(); i++)
    {
        sparse->addPoint();
        sparse->setValue(type(Id::TYPE_TIME), 10.0 + static_cast<double>(i));
        sparse->setValue(type(Id::TYPE_ALTITUDE), altitude.at(i));
    }
    sparse->addEvent({Type::GROUND_HIT, 11.0});
    sparse->addEvent({Type::LAUNCHROD, 10.5});
    return sparse;
}

TEST(FlightDataValues, WithoutVelocityAndAccelerationColumns)
{
    const FlightData data{sparseBranch()};
    // The time to apogee is that of the first row equal to the maximum within EPSILON.
    expectSame(summary(data), {5.00000001, kNaN, kNaN, kNaN, 11.0, 13.0, kNaN, kNaN, kNaN, kNaN});
}

TEST(FlightDataValues, OnlyNegativeAccelerationsGiveZero)
{
    const std::array<Id, 3> ids{Id::TYPE_TIME, Id::TYPE_ALTITUDE, Id::TYPE_ACCELERATION_TOTAL};
    std::shared_ptr<FlightDataBranch> negative = branchOf("Negative", ids);
    for (int i = 0; i < 3; i++)
    {
        negative->addPoint();
        negative->setValue(type(Id::TYPE_TIME), i);
        negative->setValue(type(Id::TYPE_ACCELERATION_TOTAL), -1.0 - i);
    }
    const FlightData data{std::move(negative)};
    // The altitude was never set: its maximum is NaN, and no row equals NaN.
    expectSame(summary(data), {kNaN, kNaN, 0.0, kNaN, kNaN, 2.0, kNaN, kNaN, kNaN, kNaN});
}

TEST(FlightDataValues, NaNAccelerationsAreSkippedAndTheDeploymentEndsTheSearch)
{
    const EventTestRocket   r;
    const std::array<Id, 3> ids{Id::TYPE_TIME, Id::TYPE_ALTITUDE, Id::TYPE_ACCELERATION_TOTAL};
    std::shared_ptr<FlightDataBranch> nanAcc = branchOf("NaNAcc", ids);
    const std::array<double, 5>       acceleration{kNaN, 4.0, kNaN, 9.0, 2.0};
    for (std::size_t i = 0; i < acceleration.size(); i++)
    {
        nanAcc->addPoint();
        nanAcc->setValue(type(Id::TYPE_TIME), static_cast<double>(i));
        nanAcc->setValue(type(Id::TYPE_ALTITUDE), 10.0 - static_cast<double>(i));
        nanAcc->setValue(type(Id::TYPE_ACCELERATION_TOTAL), acceleration.at(i));
    }
    nanAcc->addEvent({Type::RECOVERY_DEVICE_DEPLOYMENT, 3.0, r.chute});
    const FlightData data{std::move(nanAcc)};
    // 9.0 is in the row at the deployment time, which no longer counts.
    expectSame(summary(data), {10.0, kNaN, 4.0, kNaN, 0.0, 4.0, kNaN, kNaN, kNaN, kNaN});
}

TEST(FlightDataValues, ABranchWithoutRows)
{
    const std::array<Id, 3> ids{Id::TYPE_TIME, Id::TYPE_ALTITUDE, Id::TYPE_ACCELERATION_TOTAL};
    const FlightData        data{branchOf("NoRows", ids)};
    expectSame(summary(data), {kNaN, kNaN, 0.0, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN});
}

TEST(FlightDataValues, TheEmptyBranch)
{
    const FlightData data{std::make_shared<FlightDataBranch>()};
    expectSame(summary(data), {kNaN, kNaN, 0.0, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN});
}

TEST(FlightDataValues, RecalculatingKeepsWhatIsNotRecomputed)
{
    const std::array<double, 1> altitude{3.0};
    FlightData                  noTime(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    noTime.addBranch(createFlightDataBranch("NoTime", Id::TYPE_ALTITUDE, altitude));
    noTime.calculateInterestingValues();
    // The early return: the three velocities and the optimum delay keep their values.
    expectSame(summary(noTime), {3.0, kNaN, kNaN, kNaN, kNaN, kNaN, 7.0, 8.0, 9.0, 10.0});

    FlightData sparse(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    sparse.addBranch(sparseBranch());
    sparse.calculateInterestingValues();
    // The deployment velocity keeps its value: the branch has no such event.
    expectSame(summary(sparse), {5.00000001, kNaN, kNaN, kNaN, 11.0, 13.0, kNaN, kNaN, 9.0, kNaN});

    FlightData none(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    none.calculateInterestingValues();
    expectSame(summary(none), {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0});
}

TEST(FlightDataValues, AreNotRecomputedWhenTheBranchChanges)
{
    const std::shared_ptr<FlightDataBranch> branch = fullBranch();
    FlightData                              data{branch};
    branch->addPoint();
    branch->setValue(type(Id::TYPE_TIME), 4.0);
    branch->setValue(type(Id::TYPE_ALTITUDE), 100.0);
    EXPECT_EQ(data.getMaxAltitude(), 18.0);
    data.calculateInterestingValues();
    EXPECT_EQ(data.getMaxAltitude(), 100.0);
    EXPECT_EQ(data.getFlightTime(), 4.0);
    EXPECT_EQ(data.getTimeToApogee(), 4.0);
}

// ============================================================================ the branches

TEST(FlightDataBranches, AreSharedWithWhoeverAddedThem)
{
    const std::shared_ptr<FlightDataBranch> first  = fullBranch();
    const std::shared_ptr<FlightDataBranch> second = fullBranch();
    FlightData                              data;
    data.addBranch(first);
    data.addBranch(second);

    ASSERT_EQ(data.getBranchCount(), 2U);
    EXPECT_EQ(&data.getBranch(0), first.get());
    EXPECT_EQ(&data.getBranch(1), second.get());
    EXPECT_EQ(&std::as_const(data).getBranch(1), second.get());
    ASSERT_EQ(data.getBranches().size(), 2U);
    EXPECT_EQ(data.getBranches().at(0), first);
    EXPECT_EQ(data.getBranches().at(1), second);
    expectSame(data.getMaxAltitude(), kNaN);  // addBranch() computes nothing

    // The status of the simulation goes on writing the branch the data holds.
    first->addEvent({Type::LAUNCH, 0.0});
    EXPECT_EQ(data.getBranch(0).getEvents().size(), 1U);
}

TEST(FlightDataBranches, GetStageNrFindsTheVeryBranch)
{
    // Java: MiscProbe.java, "stage nr" (-1 is nullopt here).
    const std::shared_ptr<FlightDataBranch> first  = fullBranch();
    const std::shared_ptr<FlightDataBranch> second = fullBranch();
    const FlightData                        data{first, second};

    EXPECT_EQ(data.getStageNr(*first), 0U);
    EXPECT_EQ(data.getStageNr(*second), 1U);
    const FlightDataBranch other = first->clone();
    EXPECT_EQ(data.getStageNr(other), std::nullopt) << "an equal branch is another branch";
    EXPECT_EQ(data.getStageNr(data.clone().getBranch(0)), std::nullopt);
}

TEST(FlightDataBranches, AnIndexOutOfRangeAndANullBranchAreBugs)
{
    FlightData data{fullBranch()};
    EXPECT_EQ(bugText([&data] { static_cast<void>(data.getBranch(1)); }),
              "Index 1 out of bounds for length 1");
    EXPECT_THROW(static_cast<void>(std::as_const(data).getBranch(7)), BugError);
    EXPECT_THROW(data.addBranch(nullptr), BugError);
    EXPECT_THROW(static_cast<void>(FlightData{std::shared_ptr<FlightDataBranch>{}}), BugError);
    EXPECT_EQ(data.getBranchCount(), 1U);

    FlightData empty;
    EXPECT_THROW(static_cast<void>(empty.getBranch(0)), BugError);
}

// ============================================================================ immute()

TEST(FlightDataImmutability, ImmuteFreezesTheWarningsAndTheBranches)
{
    // Java: MiscProbe.java, "immute".
    const std::shared_ptr<FlightDataBranch> branch = fullBranch();
    FlightData                              data{branch};
    EXPECT_TRUE(data.isMutable());
    EXPECT_TRUE(data.getWarningSet().add(Warning::kListenersAffected));

    data.immute();
    EXPECT_FALSE(data.isMutable());
    EXPECT_FALSE(branch->isMutable());
    EXPECT_FALSE(data.getWarningSet().isMutable());
    EXPECT_THROW(data.getWarningSet().add(Warning::kNoRecoveryDevice), BugError);
    EXPECT_THROW(data.addBranch(fullBranch()), BugError);
    EXPECT_THROW(branch->addPoint(), BugError);
    EXPECT_EQ(data.getBranchCount(), 1U);
    EXPECT_NO_THROW(data.immute()) << "again";
}

TEST(FlightDataImmutability, CalculateInterestingValuesDoesNotCheck)
{
    FlightData data(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    data.addBranch(fullBranch());
    data.immute();
    EXPECT_NO_THROW(data.calculateInterestingValues());
    EXPECT_EQ(data.getMaxAltitude(), 18.0);
}

TEST(FlightDataImmutability, NanDataIsImmutableAndEmpty)
{
    // Java: FlightData.NaN_DATA.
    const FlightData& nan = FlightData::nanData();
    EXPECT_FALSE(nan.isMutable());
    EXPECT_EQ(nan.getBranchCount(), 0U);
    EXPECT_TRUE(nan.getWarningSet().empty());
    EXPECT_FALSE(nan.getWarningSet().isMutable());
    expectSame(summary(nan), {kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN, kNaN});
    EXPECT_EQ(&FlightData::nanData(), &nan) << "one object";
    EXPECT_EQ(nan.getSimulatedRocket(), nullptr);
}

// ============================================================================ clone()

TEST(FlightDataClone, CopiesEverythingButTheOptimumDelay)
{
    // Java: MiscProbe.java, "full clone".
    const EventTestRocket r;
    FlightData            data{fullBranchWithEvents(r)};
    data.getWarningSet().add(Warning::kListenersAffected);
    data.getWarningSet().add(Warning::LargeAOA(0.5));
    const FlightData clone = data.clone();

    expectSame(summary(data), {18.0, 20.0, 40.0, 0.0625, 1.5, 3.5, 4.0, 9.0, 7.0, 0.5});
    expectSame(summary(clone), {18.0, 20.0, 40.0, 0.0625, 1.5, 3.5, 4.0, 9.0, 7.0, kNaN});
    EXPECT_TRUE(clone.isMutable());
    ASSERT_EQ(clone.getBranchCount(), 1U);
    EXPECT_NE(&clone.getBranch(0), &data.getBranch(0)) << "the branches are cloned";
    EXPECT_EQ(clone.getBranch(0).getLength(), 8U);
    EXPECT_EQ(clone.getBranch(0).getEvents().size(), 7U);
    EXPECT_EQ(clone.getBranch(0).getTimeToOptimumAltitude(), 1.75);
    EXPECT_EQ(clone.getBranch(0).getOptimumDelay(), 0.5)
        << "the branch still knows it; only the summary value is not copied";

    ASSERT_EQ(clone.getWarningSet().size(), 2U);
    EXPECT_TRUE(clone.getWarningSet() == data.getWarningSet());
    EXPECT_EQ(clone.getWarningSet().begin()->id(), data.getWarningSet().begin()->id())
        << "the warnings keep their ids";
}

TEST(FlightDataClone, OfImmutableDataIsMutableAllTheWayDown)
{
    FlightData data{fullBranch()};
    data.getWarningSet().add(Warning::kListenersAffected);
    data.immute();
    FlightData clone = data.clone();

    EXPECT_TRUE(clone.isMutable());
    EXPECT_TRUE(clone.getBranch(0).isMutable());
    EXPECT_TRUE(clone.getWarningSet().isMutable());
    EXPECT_NO_THROW(clone.addBranch(fullBranch()));
    EXPECT_NO_THROW(clone.getBranch(0).addPoint());
    EXPECT_TRUE(clone.getWarningSet().add(Warning::kNoRecoveryDevice));
    EXPECT_EQ(data.getBranchCount(), 1U);
    EXPECT_EQ(data.getBranch(0).getLength(), 8U);
    EXPECT_EQ(data.getWarningSet().size(), 1U);
}

TEST(FlightDataClone, OfSummaryData)
{
    // Java: MiscProbe.java, "summary clone".
    const FlightData data(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    const FlightData clone = data.clone();
    expectSame(summary(clone), {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, kNaN});
    EXPECT_EQ(clone.getBranchCount(), 0U);
}

// ============================================================================ moving

TEST(FlightDataMove, TakesEverythingAlong)
{
    const EventTestRocket r;
    FlightData            data{fullBranchWithEvents(r)};
    data.getWarningSet().add(Warning::kListenersAffected);
    data.immute();
    const FlightDataBranch* branch = &data.getBranch(0);

    FlightData moved(std::move(data));
    EXPECT_FALSE(moved.isMutable());
    EXPECT_EQ(&moved.getBranch(0), branch);
    EXPECT_EQ(moved.getWarningSet().size(), 1U);
    expectSame(summary(moved), {18.0, 20.0, 40.0, 0.0625, 1.5, 3.5, 4.0, 9.0, 7.0, 0.5});

    FlightData assigned;
    assigned = std::move(moved);
    EXPECT_FALSE(assigned.isMutable());
    EXPECT_EQ(&assigned.getBranch(0), branch);
    expectSame(summary(assigned), {18.0, 20.0, 40.0, 0.0625, 1.5, 3.5, 4.0, 9.0, 7.0, 0.5});
}

// ==================================================================== the warning of an event

TEST(FlightDataWarnings, FindWarningGivesTheSetsWarningOfAnEvent)
{
    FlightData  data;
    WarningSet& warnings = data.getWarningSet();
    ASSERT_TRUE(warnings.add(Warning::LargeAOA(0.3)));
    const Warning* stored = warnings.find(Warning::LargeAOA(0.3));
    ASSERT_NE(stored, nullptr);

    // What the simulation does when it adds a warning: the event carries a copy of the set's.
    const FlightEvent event{Type::SIM_WARN, 1.0, nullptr, FlightEvent::warningData(*stored)};
    EXPECT_EQ(data.findWarning(event), stored);

    // A worse angle of attack later in the flight replaces the contents of the set's warning;
    // the event's copy still shows the first one.
    EXPECT_FALSE(warnings.add(Warning::LargeAOA(0.6)));
    const auto* inEvent = dynamic_cast<const Warning::LargeAOA*>(event.getWarning().get());
    const auto* inSet   = dynamic_cast<const Warning::LargeAOA*>(data.findWarning(event));
    ASSERT_NE(inEvent, nullptr);
    ASSERT_NE(inSet, nullptr);
    EXPECT_EQ(inEvent->aoa(), 0.3);
    EXPECT_EQ(inSet->aoa(), 0.6) << "Java's event shows this one: it holds the set's object";
}

TEST(FlightDataWarnings, FindWarningOfOtherEventsIsNull)
{
    FlightData data;
    data.getWarningSet().add(Warning::kListenersAffected);

    EXPECT_EQ(data.findWarning(FlightEvent{Type::LAUNCH, 0.0}), nullptr) << "no warning";
    const FlightEvent stranger{Type::SIM_WARN, 1.0, nullptr,
                               FlightEvent::warningData(Warning::kNoRecoveryDevice)};
    EXPECT_EQ(data.findWarning(stranger), nullptr) << "a warning the set does not have";

    // A clone has the warnings with the same ids, so the events of its branches find them.
    const Warning* stored = data.getWarningSet().find(Warning::kListenersAffected);
    ASSERT_NE(stored, nullptr);
    const FlightEvent event{Type::SIM_WARN, 1.0, nullptr, FlightEvent::warningData(*stored)};
    const FlightData  clone = data.clone();
    ASSERT_NE(clone.findWarning(event), nullptr);
    EXPECT_NE(clone.findWarning(event), stored);
    EXPECT_EQ(clone.findWarning(event)->id(), stored->id());
}

// ==================================================================== the simulated rocket

TEST(FlightDataSimulatedRocket, IsKeptAliveByTheDataAndItsClones)
{
    auto                        rocket = std::make_shared<Rocket>();
    const std::weak_ptr<Rocket> weak   = rocket;

    std::optional<FlightData> data{std::in_place};
    EXPECT_EQ(data->getSimulatedRocket(), nullptr);
    data->setSimulatedRocket(rocket);
    EXPECT_EQ(data->getSimulatedRocket(), rocket);
    rocket.reset();
    EXPECT_FALSE(weak.expired()) << "the data co-owns the rocket";

    std::optional<FlightData> clone{data->clone()};
    EXPECT_EQ(clone->getSimulatedRocket(), data->getSimulatedRocket());
    data.reset();
    EXPECT_FALSE(weak.expired()) << "and so does its clone";

    std::optional<FlightData> moved = std::move(clone);
    EXPECT_FALSE(weak.expired()) << "a move takes it along";
    moved.reset();
    EXPECT_TRUE(weak.expired());
}

TEST(FlightDataSimulatedRocket, CanBeSetOnImmutableDataAndDropped)
{
    FlightData data;
    data.immute();
    const auto rocket = std::make_shared<Rocket>();
    EXPECT_NO_THROW(data.setSimulatedRocket(rocket));
    EXPECT_EQ(data.getSimulatedRocket(), rocket);
    data.setSimulatedRocket(nullptr);
    EXPECT_EQ(data.getSimulatedRocket(), nullptr);
    EXPECT_EQ(rocket.use_count(), 1);
}

TEST(FlightDataSimulatedRocket, OutlivesTheEventsThatPointIntoIt)
{
    // The events of a finished simulation point into the simulated rocket; the data that holds
    // them holds the rocket.
    std::optional<FlightData> data;
    {
        auto        rocket = std::make_shared<Rocket>();
        AxialStage& stage  = rocket->addChild(std::make_unique<AxialStage>());
        stage.setName("Stage");
        const std::vector<const FlightDataType*> types{&type(Id::TYPE_TIME)};
        const auto branch = std::make_shared<FlightDataBranch>("Flight", &stage, types);
        branch->addEvent({Type::STAGE_SEPARATION, 1.0, &stage});
        data.emplace(std::initializer_list<std::shared_ptr<FlightDataBranch>>{branch});
        data->setSimulatedRocket(std::move(rocket));
    }
    const std::vector<FlightEvent> events = data->getBranch(0).getEvents();
    ASSERT_EQ(events.size(), 1U);
    ASSERT_NE(events.front().getSource(), nullptr);
    EXPECT_EQ(events.front().getSource()->getName(), "Stage");
    EXPECT_EQ(events.front().toString(),
              "FlightEvent[type=STAGE_SEPARATION,time=1.0,source=Stage,data=null]");
}

}  // namespace
