// A simulation run twice with the same random seed must produce the same flight: the two cases
// of OpenRocket's WindModelSeedReproducibilityTest
// (core/src/test/java/info/openrocket/core/simulation/WindModelSeedReproducibilityTest.java) that
// run a simulation. (The three cases that sample the wind models and the options directly are in
// SimulationOptionsTests.cpp.)
//
// The seed reaches the integrator (the Runge-Kutta steppers build their random source from the
// conditions' seed) and, through SimulationOptions::toSimulationConditions(), the wind model the
// run uses, so a flight with wind turbulence is reproducible from the seed.
//
// The simulations run under the preferences of OpenRocket's test set-up (SimulationRunSupport.h).
// The turbulence itself is not OpenRocket's (PinkNoise draws from another generator than Java's),
// so the flights are not the ones the Java test sees; what is asserted is the same.

#include <format>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/Error.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationRunSupport.h"

namespace
{

using QtRocket::FlightData;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::MultiLevelPinkNoiseWindModel;
using QtRocket::Result;
using QtRocket::Simulation;
using QtRocket::SimulationOptions;
using QtRocket::WindModelType;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::junitEquals;
using QtRocket::Test::simulatedData;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;

/// WindModelSeedReproducibilityTest.SEED
constexpr int kSeed = 12345;
/// WindModelSeedReproducibilityTest.RUNS
constexpr int kRuns = 5;

/// WindModelSeedReproducibilityTest.TURBULENCE: turbulence has to be non-zero, or there is no
/// randomness to reproduce.
constexpr double kTurbulence = 0.2;

/// WindModelSeedReproducibilityTest.WIND_SPEED
constexpr double kWindSpeed = 5.0;

/// WindModelSeedReproducibilityTest.LAUNCH_GUIDE_LENGTH: long enough that the rocket leaves the
/// guide fast enough to fly. On a 1 m guide this airframe departs in this wind for a good
/// fraction of seeds, and the flight ends a metre off the pad -- which is reproducible too, but
/// would leave the test asserting the repeatability of an abort rather than of a flight.
constexpr double kLaunchGuideLength = 3.0;

/// WindModelSeedReproducibilityTest.MIN_MEANINGFUL_APOGEE: still-air apogee is around 133 m; a
/// flight well below this one did not fly.
constexpr double kMinMeaningfulApogee = 100.0;

/// WindModelSeedReproducibilityTest.SAME_FLIGHT_TOLERANCE: tolerance on "the same flight", in
/// metres. (Java allows for runs of one seed that differ by around 1.2e-4 m across its platforms
/// and JVM execution modes; here two runs of one seed in one process are the same flight to the
/// last bit, which SimulationThreading checks. The Java tolerance is kept.)
constexpr double kSameFlightTolerance = 5.0e-4;

/// Adds a level to @p model and expects that to succeed.
void addLevel(MultiLevelPinkNoiseWindModel& model, double altitude, double speed, double direction,
              double standardDeviation)
{
    const Result<void> added = model.addWindLevel(altitude, speed, direction, standardDeviation);
    EXPECT_TRUE(added.has_value()) << (added.has_value() ? "" : added.error().message);
}

/// WindModelSeedReproducibilityTest.apogee(type, seed): the apogee of a flight of the Estes
/// Alpha III in turbulent wind of the model @p type, with the random seed @p seed; NaN, and a
/// failure, when the simulation fails.
[[nodiscard]] double apogee(WindModelType type, int seed)
{
    const TestEstesAlphaIII alpha;
    JavaTestPreferences     preferences;
    Simulation              sim(*alpha.rocket, preferences.store);
    sim.setFlightConfigurationId(testFcid(0));

    SimulationOptions& options = sim.getOptions();
    options.setIsaAtmosphere(true);
    options.setTimeStep(0.05);
    options.setLaunchRodLength(kLaunchGuideLength);
    options.setWindModelType(type);

    if (type == WindModelType::AVERAGE)
    {
        options.setWindSpeedAverage(kWindSpeed);
        options.setWindTurbulenceIntensity(kTurbulence);
    }
    else
    {
        MultiLevelPinkNoiseWindModel& model = options.getMultiLevelWindModel();
        model.clearLevels();
        addLevel(model, 0, kWindSpeed, 0, kWindSpeed * kTurbulence);
        addLevel(model, 200, kWindSpeed * 1.5, 0, kWindSpeed * kTurbulence);
    }

    options.setRandomSeed(seed);
    const Result<void> result = sim.simulate();
    if (!result.has_value())
    {
        ADD_FAILURE() << result.error().message;
        return std::numeric_limits<double>::quiet_NaN();
    }

    const FlightData& data = simulatedData(sim);
    if (data.getBranchCount() == 0)
    {
        ADD_FAILURE() << "the simulation has no branch";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return data.getBranch(0).getMaximum(FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE));
}

/// The runs 1 ... RUNS - 1 of WindModelSeedReproducibilityTest.assertReproducible() whose apogee
/// is not @p reference within SAME_FLIGHT_TOLERANCE.
[[nodiscard]] std::vector<std::string> runsThatDiffer(WindModelType type, double reference)
{
    std::vector<std::string> differing;
    for (int i = 1; i < kRuns; i++)
    {
        const ::testing::AssertionResult same =
            junitEquals(reference, apogee(type, kSeed), kSameFlightTolerance);
        if (!same)
        {
            differing.push_back(std::format("run {} with seed {} must reproduce the first run: {}",
                                            i, kSeed, same.message()));
        }
    }
    return differing;
}

/// WindModelSeedReproducibilityTest.assertReproducible()
void assertReproducible(WindModelType type)
{
    const double reference = apogee(type, kSeed);

    // Guard the fixture, not the fix: an aborted flight is reproducible as readily as a real
    // one, so without this the test could keep passing while measuring nothing but how
    // repeatably the rocket falls over.
    ASSERT_TRUE(reference > kMinMeaningfulApogee)
        << "fixture must produce a real flight, but apogee was " << reference << " m";

    EXPECT_EQ(runsThatDiffer(type, reference), std::vector<std::string>{});
}

// WindModelSeedReproducibilityTest.testAverageWindModelIsReproducibleFromSeed
TEST(WindModelSeedReproducibilityTest, AverageWindModelIsReproducibleFromSeed)
{
    assertReproducible(WindModelType::AVERAGE);
}

// WindModelSeedReproducibilityTest.testMultiLevelWindModelIsReproducibleFromSeed
TEST(WindModelSeedReproducibilityTest, MultiLevelWindModelIsReproducibleFromSeed)
{
    assertReproducible(WindModelType::MULTI_LEVEL);
}

}  // namespace
