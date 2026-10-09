// OpenRocket's sixteen example designs (data/examples), loaded as the files they are by
// GeneralRocketLoader in the environment of the golden harness (goldens/GoldenExamples.h), and
// held to what OpenRocket makes of them.
//
// 1. ExampleFilesTest, OpenRocket's own test of its examples
//    (core/src/test/java/info/openrocket/core/file/openrocket/ExampleFilesTest.java). One test
//    per file, on the freshly loaded document, statement for statement in Java's order: an
//    automatic dimension stores what it computes, so what a design answers depends on what it
//    was asked before, and this test is the one that asks as OpenRocket's does. Its three
//    tables are Java's, without their entries for "Base drag hack (short-wide).ork", which is
//    no example of OpenRocket's any more. Beside each of Java's literals, which have nine
//    decimals and a tolerance of 1e-6, stands what OpenRocket computes at that statement at
//    full precision (the section "junit" of ExampleOrkFiles.h), held to a relative 1e-9.
//
//    NOT COMPARED: the maximum altitude of the 54 flights. Java flies each in the turbulence
//    its design's wind has (mostly a standard deviation of 0.2 m/s around 2 m/s), drawn from
//    java.util.Random with seed 0, and accepts 0.5 m around its literal. QtRocket's turbulence
//    is another sequence for the same seed (PinkNoise draws from another generator, a decision
//    of tier 3), and with another realisation OpenRocket's own altitude is more than 0.5 m
//    from the literal in 35 of the 54 flights (flown with the seeds 0 to 31; by up to 16 m of
//    617 m: SeedProbe of the probes of tier 9c, part "examples"). QtRocket's altitudes over
//    the same seeds lie as OpenRocket's do: beyond 0.5 m in 35 of the 54 flights and at
//    0.500 m in one more, by up to 16 m (the first measurement at the end of this file). The
//    flights are compared with the calm goldens of the examples instead.
//
//    What does not depend on the realisation is asserted here: every flight runs, none has an
//    error, and its warnings are OpenRocket's, by priority and one by one (the numbers a
//    warning names apart). One warning of one flight does depend on it, in OpenRocket too:
//    of that one it is asserted that the flight has it exactly when it deploys its parachute
//    above the limit of the warning, not that it has none (kDeploymentsAtTheLimit).
//
// 2. OpenRocketCoreTest (core/src/test/java/info/openrocket/core/startup): its twelve methods
//    that load or simulate an example or ask the motor database.
//
// 3. What those tests do not look at. The state of every example as loaded is OpenRocket's,
//    line for line (the lines of DesignFileState.h, here in their short form): the file
//    version, no warning, every component with what it holds, the flight configurations with
//    their ids, names and motors, the 65 motors with their digests, the 54 simulations with
//    status, options, extensions, stored summaries, warnings, branches, events and a digest of
//    every stored column, the decal images with their bytes, the document materials in the
//    order a save writes them, the photo settings, the storage options and the undo state.
//
// 4. The same files without component presets, which is how the product loads them until it
//    reads preset files: the six warnings OpenRocket gives then, and the state it has then.
//
// 5. OpenRocket's re-saves of the examples (resave/rocket.ork of their goldens) load without a
//    warning into the rocket of the original.
//
// The expectations of 3 to 5 are OpenRocket's own answers, printed by the Java probe
// ExampleProbe of the probes of tier 9c, part "examples", and put into ExampleOrkFiles.h by
// its script. With the six presets of the fixture OpenRocket gives, line for line, what it
// gives with its whole preset database (5228 presets), with which its own test and the golden
// harness load.
//
// Each test is a process of its own under ctest, and the bundled motor database is read once
// per process: but for the port of ExampleFilesTest, which is one test per file as in Java, a
// test here goes through all sixteen files.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/BarrowmanCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/mass/MassCalculator.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"
#include "file/DesignFileEnvironment.h"
#include "file/ExampleMotors.h"
#include "file/RocketLoaderTestSupport.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/ConditionsTestSupport.h"
#include "file/openrocket/DesignFileState.h"
#include "file/openrocket/ExampleOrkFiles.h"
#include "file/openrocket/FlightDataTestSupport.h"
#include "goldens/GoldenExamples.h"
#include "motor/TestMotorDatabase.h"
#include "simulation/SimulationRunSupport.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::AerodynamicForces;
using QtRocket::BarrowmanCalculator;
using QtRocket::BoundingBox;
using QtRocket::Coordinate;
using QtRocket::FlightConditions;
using QtRocket::FlightConfiguration;
using QtRocket::FlightData;
using QtRocket::LoadedDocument;
using QtRocket::MassCalculator;
using QtRocket::MessagePriority;
using QtRocket::Motor;
using QtRocket::MotorConfiguration;
using QtRocket::MotorMount;
using QtRocket::NoseCone;
using QtRocket::OpenRocketDocument;
using QtRocket::Preferences;
using QtRocket::Result;
using QtRocket::RigidBody;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Simulation;
using QtRocket::SimulationOptions;
using QtRocket::Warning;
using QtRocket::WarningSet;
using QtRocket::Test::ascii;
using QtRocket::Test::CompactLine;
using QtRocket::Test::compactState;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::DesignFileEnvironment;
using QtRocket::Test::DesignNumber;
using QtRocket::Test::ExampleMotor;
using QtRocket::Test::ExampleMotorMatch;
using QtRocket::Test::ExampleOrkFile;
using QtRocket::Test::ExampleSource;
using QtRocket::Test::GoldenExample;
using QtRocket::Test::goldenExampleEnvironment;
using QtRocket::Test::goldenExamples;
using QtRocket::Test::isCloseTo;
using QtRocket::Test::junitEquals;
using QtRocket::Test::kExampleMotors;
using QtRocket::Test::kExampleOrkFiles;
using QtRocket::Test::LoadEvents;
using QtRocket::Test::loadGoldenExample;
using QtRocket::Test::StateDetail;

using Presets = DesignFileEnvironment::Presets;
using Lines   = std::vector<std::string>;

/// The row of ExampleOrkFiles.h for the example whose file is named @p file, or null.
[[nodiscard]] const ExampleOrkFile* tableOf(std::string_view file)
{
    // Through a span: an iterator of an array is a pointer with some standard libraries.
    const std::span<const ExampleOrkFile> rows(kExampleOrkFiles);
    const auto found = std::ranges::find(rows, file, &ExampleOrkFile::file);
    return found == rows.end() ? nullptr : &*found;
}

// ====================================================================== 1. ExampleFilesTest

/// ExampleFilesTest.NO_MOTORS.
constexpr std::string_view kNoMotors = "[No motors]";
/// ExampleFilesTest.SIMULATION_RANDOM_SEED.
constexpr int kSimulationRandomSeed = 0;
/// ExampleFilesTest.AERODYNAMIC_MACHES.
constexpr std::array<double, 6> kAerodynamicMaches{0.3, 0.8, 1.0, 1.2, 2.0, 3.0};
/// ExampleFilesTest.MASS_EPSILON, POSITION_EPSILON and COEFFICIENT_EPSILON: "Exact design
/// calculations use tight engineering-unit tolerances: one milligram and one micrometer."
constexpr double kMassEpsilon        = 1.0e-6;
constexpr double kPositionEpsilon    = 1.0e-6;
constexpr double kCoefficientEpsilon = 1.0e-6;

/// ExampleFilesTest.WarningCounts: the warnings of a set by priority.
struct WarningCounts
{
    int informative{0};
    int normal{0};
    int critical{0};

    [[nodiscard]] bool operator==(const WarningCounts&) const = default;

    /// WarningCounts.toString().
    [[nodiscard]] std::string toString() const
    {
        return std::format("{{informative={}, normal={}, critical={}}}", informative, normal,
                           critical);
    }

    /// "<informative>,<normal>,<critical>", as the probe prints the counts.
    [[nodiscard]] std::string toProbeText() const
    {
        return std::format("{},{},{}", informative, normal, critical);
    }
};

/// WarningCounts.MISSING: the counts of a simulation without a warning set.
constexpr WarningCounts kMissingWarnings{.informative = -1, .normal = -1, .critical = -1};

/// ExampleFilesTest.ExpectedWarnings, with its Builder (one type here: the calls fill it).
struct ExpectedWarnings
{
    WarningCounts                                     open;
    std::map<std::string, WarningCounts, std::less<>> simulations;

    ExpectedWarnings& openWarnings(int informative, int normal, int critical)
    {
        open = {.informative = informative, .normal = normal, .critical = critical};
        return *this;
    }

    ExpectedWarnings& simulationWarnings(std::string_view simulationName, int informative,
                                         int normal, int critical)
    {
        simulations.insert_or_assign(
            std::string(simulationName),
            WarningCounts{.informative = informative, .normal = normal, .critical = critical});
        return *this;
    }
};

/// ExampleFilesTest.FlightConfigurationSnapshot: the name of a flight configuration and the
/// lines of its active motors, sorted.
struct FlightConfigurationSnapshot
{
    std::string name;
    Lines       motors;
};

/// ExampleFilesTest.ExpectedFlightConfigurations, with its Builder.
struct ExpectedFlightConfigurations
{
    std::vector<FlightConfigurationSnapshot> configurations;

    ExpectedFlightConfigurations& configuration(std::string_view configurationName,
                                                std::initializer_list<std::string_view> motors = {})
    {
        Lines motorList(motors.begin(), motors.end());
        std::ranges::sort(motorList);
        configurations.push_back(
            {.name = std::string(configurationName), .motors = std::move(motorList)});
        return *this;
    }
};

/// ExampleFilesTest.CoordinateSnapshot.
struct CoordinateSnapshot
{
    double x{0.0};
    double y{0.0};
    double z{0.0};

    [[nodiscard]] static CoordinateSnapshot from(const Coordinate& coordinate)
    {
        return {.x = coordinate.x, .y = coordinate.y, .z = coordinate.z};
    }
};

/// ExampleFilesTest.BoundsSnapshot.
struct BoundsSnapshot
{
    CoordinateSnapshot min;
    CoordinateSnapshot max;
};

/// ExampleFilesTest.AerodynamicMetrics: the centre of pressure and the drag coefficient at one
/// Mach number.
struct AerodynamicMetrics
{
    double cpX{0.0};
    double cd{0.0};
};

/// ExampleFilesTest.DesignMetrics: "the fixed, motorless properties of the complete example
/// rocket".
struct DesignMetrics
{
    double                               dryMass{0.0};
    CoordinateSnapshot                   dryCg;
    double                               length{0.0};
    BoundsSnapshot                       bounds;
    std::map<double, AerodynamicMetrics> aerodynamics;
};

/// ExampleFilesTest.SimulationMetrics.
struct SimulationMetrics
{
    double             launchMass{0.0};
    CoordinateSnapshot launchCg;
    double             maxAltitude{0.0};
};

/// ExampleFilesTest.ExpectedMetrics, with its Builder.
struct ExpectedMetrics
{
    DesignMetrics                                         metrics;
    std::map<std::string, SimulationMetrics, std::less<>> simulations;

    ExpectedMetrics& design(double dryMass, double dryCgX, double dryCgY, double dryCgZ,
                            double length, double minX, double minY, double minZ, double maxX,
                            double maxY, double maxZ)
    {
        metrics.dryMass = dryMass;
        metrics.dryCg   = {.x = dryCgX, .y = dryCgY, .z = dryCgZ};
        metrics.length  = length;
        metrics.bounds  = {.min = {.x = minX, .y = minY, .z = minZ},
                           .max = {.x = maxX, .y = maxY, .z = maxZ}};
        return *this;
    }

    ExpectedMetrics& aerodynamics(double mach, double cpX, double cd)
    {
        metrics.aerodynamics.insert_or_assign(mach, AerodynamicMetrics{.cpX = cpX, .cd = cd});
        return *this;
    }

    ExpectedMetrics& simulation(std::string_view name, double launchMass, double launchCgX,
                                double launchCgY, double launchCgZ, double maxAltitude)
    {
        simulations.insert_or_assign(
            std::string(name),
            SimulationMetrics{.launchMass  = launchMass,
                              .launchCg    = {.x = launchCgX, .y = launchCgY, .z = launchCgZ},
                              .maxAltitude = maxAltitude});
        return *this;
    }
};

/// ExampleFilesTest.EXPECTATIONS: the warnings of the load and of every flight, by priority.
[[nodiscard]] std::map<std::string, ExpectedWarnings, std::less<>> allExpectations()
{
    std::map<std::string, ExpectedWarnings, std::less<>> table;
    table.emplace("A simple model rocket.ork",
                  ExpectedWarnings()
                      .openWarnings(0, 0, 0)
                      .simulationWarnings("Simulation 1", 0, 0, 0)
                      .simulationWarnings("Simulation 2", 0, 0, 0)
                      .simulationWarnings("Simulation 3 - too short delay", 0, 1, 0)
                      .simulationWarnings("Simulation 4", 0, 0, 0)
                      .simulationWarnings("Simulation 5", 0, 0, 0));
    table.emplace("Two stage high power rocket.ork",
                  ExpectedWarnings()
                      .openWarnings(0, 0, 0)
                      .simulationWarnings("Simulation 1", 1, 0, 0)
                      .simulationWarnings("Simulation 2", 1, 0, 0));
    table.emplace("Three stage low power rocket.ork",
                  ExpectedWarnings()
                      .openWarnings(0, 0, 0)
                      .simulationWarnings("Simulation 1", 0, 0, 0)
                      .simulationWarnings("Simulation 2", 0, 0, 0)
                      .simulationWarnings("Simulation 3", 0, 0, 0));
    table.emplace(
        "ARC payload rocket.ork",
        ExpectedWarnings().openWarnings(0, 0, 0).simulationWarnings("Simulation 1", 0, 0, 0));
    table.emplace(
        "Tube fin rocket.ork",
        ExpectedWarnings().openWarnings(0, 0, 0).simulationWarnings("Simulation 1", 0, 0, 0));
    table.emplace("Deployable payload.ork",
                  ExpectedWarnings()
                      .openWarnings(0, 0, 0)
                      .simulationWarnings("Simulation 1", 0, 0, 0)
                      .simulationWarnings("Simulation 2", 0, 0, 0)
                      .simulationWarnings("Simulation 3 - too short delay", 0, 2, 0)
                      .simulationWarnings("Simulation 4", 0, 0, 0)
                      .simulationWarnings("Simulation 5", 0, 0, 0));
    table.emplace("Airstart timing.ork", ExpectedWarnings()
                                             .openWarnings(0, 0, 0)
                                             .simulationWarnings("Simulation 1", 0, 1, 0)
                                             .simulationWarnings("Simulation 2", 0, 1, 0)
                                             .simulationWarnings("Simulation 3", 0, 1, 0)
                                             .simulationWarnings("Simulation 4", 0, 1, 0)
                                             .simulationWarnings("Simulation 5", 0, 1, 0));
    table.emplace("Chute release.ork", ExpectedWarnings()
                                           .openWarnings(0, 0, 0)
                                           .simulationWarnings("Simulation 2", 0, 0, 0)
                                           .simulationWarnings("Simulation 3", 0, 0, 0));
    table.emplace("Dual parachute deployment.ork",
                  ExpectedWarnings()
                      .openWarnings(0, 0, 0)
                      .simulationWarnings("Simulation 1", 0, 1, 0)
                      .simulationWarnings("Simulation 2", 0, 1, 0)
                      .simulationWarnings("Simulation 3", 0, 2, 0)
                      .simulationWarnings("Simulation 4", 0, 1, 0)
                      .simulationWarnings("Simulation 5", 0, 1, 0)
                      .simulationWarnings("Simulation 6", 0, 1, 0));
    table.emplace("Clustered motors.ork",
                  ExpectedWarnings()
                      .openWarnings(0, 0, 0)
                      .simulationWarnings("Simulation 1", 0, 0, 0)
                      .simulationWarnings("Simulation 2", 0, 0, 0)
                      .simulationWarnings("Simulation 3 - too short delay", 0, 1, 0)
                      .simulationWarnings("Simulation 4", 0, 0, 0)
                      .simulationWarnings("Simulation 5", 0, 0, 0));
    table.emplace("Parallel booster staging.ork", ExpectedWarnings()
                                                      .openWarnings(0, 0, 0)
                                                      .simulationWarnings("Simulation 1", 0, 0, 0)
                                                      .simulationWarnings("Simulation 2", 0, 0, 0));
    table.emplace("Pods--airframes and winglets.ork",
                  ExpectedWarnings()
                      .openWarnings(0, 0, 0)
                      .simulationWarnings("Simulation 1", 0, 0, 0)
                      .simulationWarnings("Simulation 2", 0, 0, 0)
                      .simulationWarnings("Simulation 3", 0, 0, 0)
                      .simulationWarnings("Simulation 4", 0, 0, 0)
                      .simulationWarnings("Simulation 5", 0, 0, 0));
    table.emplace(
        "Pods--powered with recovery deployment.ork",
        ExpectedWarnings().openWarnings(0, 0, 0).simulationWarnings("Simulation 1", 0, 0, 0));
    table.emplace("Simulation extensions.ork",
                  ExpectedWarnings()
                      .openWarnings(0, 0, 0)
                      .simulationWarnings("Active roll control", 0, 0, 0)
                      .simulationWarnings("No controlling", 0, 0, 0)
                      .simulationWarnings("Roll control + air-start", 1, 0, 0));
    table.emplace("Simulation scripting.ork",
                  ExpectedWarnings()
                      .openWarnings(0, 0, 0)
                      .simulationWarnings("Active roll control", 0, 0, 0)
                      .simulationWarnings("No controlling", 0, 0, 0)
                      .simulationWarnings("Roll control + air-start", 0, 0, 0));
    table.emplace("3D printable nose cone and fins.ork",
                  ExpectedWarnings()
                      .openWarnings(0, 0, 0)
                      .simulationWarnings("Simulation 1", 0, 0, 0)
                      .simulationWarnings("Simulation 2", 0, 0, 0)
                      .simulationWarnings("Simulation 3 - too short delay", 0, 1, 0)
                      .simulationWarnings("Simulation 4", 0, 0, 0)
                      .simulationWarnings("Simulation 5", 0, 0, 0));
    return table;
}

/// ExampleFilesTest.EXPECTED_FLIGHT_CONFIGURATIONS: the flight configurations in their order,
/// each with its name and its motors, "<mount> -> <motor> x<count>".
[[nodiscard]] std::map<std::string, ExpectedFlightConfigurations, std::less<>>
allExpectedFlightConfigurations()
{
    std::map<std::string, ExpectedFlightConfigurations, std::less<>> table;
    table.emplace("3D printable nose cone and fins.ork",
                  ExpectedFlightConfigurations()
                      .configuration(kNoMotors)
                      .configuration("[A8-3]", {"Inner Tube -> A8-3 x1"})
                      .configuration("[B6-4]", {"Inner Tube -> B6-4 x1"})
                      .configuration("[C6-3]", {"Inner Tube -> C6-3 x1"})
                      .configuration("[C6-5]", {"Inner Tube -> C6-5 x1"})
                      .configuration("[C6-7]", {"Inner Tube -> C6-7 x1"}));
    table.emplace("A simple model rocket.ork",
                  ExpectedFlightConfigurations()
                      .configuration(kNoMotors)
                      .configuration("[A8-3]", {"Inner Tube -> A8-3 x1"})
                      .configuration("[B4-4]", {"Inner Tube -> B4-4 x1"})
                      .configuration("[C6-3]", {"Inner Tube -> C6-3 x1"})
                      .configuration("[C6-5]", {"Inner Tube -> C6-5 x1"})
                      .configuration("[C6-7]", {"Inner Tube -> C6-7 x1"}));
    table.emplace("ARC payload rocket.ork",
                  ExpectedFlightConfigurations().configuration(kNoMotors).configuration(
                      "[None; F50-9]", {"Inner Tube -> F50-9 x1"}));
    table.emplace("Airstart timing.ork",
                  ExpectedFlightConfigurations()
                      .configuration(kNoMotors)
                      .configuration("[3\u00D7 I211-P, K550-P]",
                                     {"38mm airstart -> I211-P x3", "54mm center -> K550-P x1"})
                      .configuration("Airstart @2s",
                                     {"38mm airstart -> I211-P x3", "54mm center -> K550-P x1"})
                      .configuration("Airstart @1s",
                                     {"38mm airstart -> I211-P x3", "54mm center -> K550-P x1"})
                      .configuration("airstart @4s",
                                     {"38mm airstart -> I211-P x3", "54mm center -> K550-P x1"})
                      .configuration("airstart @6s",
                                     {"38mm airstart -> I211-P x3", "54mm center -> K550-P x1"}));
    table.emplace("Chute release.ork", ExpectedFlightConfigurations()
                                           .configuration(kNoMotors)
                                           .configuration("[G40-7]", {"Inner Tube -> G40-7 x1"})
                                           .configuration("[G80-10]", {"Inner Tube -> G80-10 x1"}));
    table.emplace("Clustered motors.ork",
                  ExpectedFlightConfigurations()
                      .configuration(kNoMotors)
                      .configuration("[4\u00D7 A8-3]", {"Clustered Inner Tube -> A8-3 x4"})
                      .configuration("[4\u00D7 B4-4]", {"Clustered Inner Tube -> B4-4 x4"})
                      .configuration("[4\u00D7 C6-3]", {"Clustered Inner Tube -> C6-3 x4"})
                      .configuration("[4\u00D7 C6-5]", {"Clustered Inner Tube -> C6-5 x4"})
                      .configuration("[4\u00D7 C6-7]", {"Clustered Inner Tube -> C6-7 x4"}));
    table.emplace("Deployable payload.ork",
                  ExpectedFlightConfigurations()
                      .configuration(kNoMotors)
                      .configuration("[None; A8-3]", {"Inner Tube -> A8-3 x1"})
                      .configuration("[None; B4-4]", {"Inner Tube -> B4-4 x1"})
                      .configuration("[None; C6-3]", {"Inner Tube -> C6-3 x1"})
                      .configuration("[None; C6-5]", {"Inner Tube -> C6-5 x1"})
                      .configuration("[None; C6-7]", {"Inner Tube -> C6-7 x1"}));
    table.emplace("Dual parachute deployment.ork",
                  ExpectedFlightConfigurations()
                      .configuration(kNoMotors)
                      .configuration("[H669-P]", {"Inner Tube -> H669-P x1"})
                      .configuration("[H242-P]", {"Inner Tube -> H242-P x1"})
                      .configuration("[J570-P]", {"Inner Tube -> J570-P x1"})
                      .configuration("[H999-P]", {"Inner Tube -> H999-P x1"})
                      .configuration("[I1299-P]", {"Inner Tube -> I1299-P x1"})
                      .configuration("[G64-P]", {"Inner Tube -> G64-P x1"}));
    table.emplace("Parallel booster staging.ork",
                  ExpectedFlightConfigurations()
                      .configuration(kNoMotors)
                      .configuration("[I115-10; 2\u00D7 E12-0]",
                                     {"Body Tube -> I115-10 x1", "Booster Motor Tube -> E12-0 x2"})
                      .configuration("[I115-10; None]", {"Body Tube -> I115-10 x1"}));
    table.emplace("Pods--airframes and winglets.ork",
                  ExpectedFlightConfigurations()
                      .configuration(kNoMotors)
                      .configuration("[A8-3]", {"18mm Motor mount -> A8-3 x1"})
                      .configuration("[B6-4]", {"18mm Motor mount -> B6-4 x1"})
                      .configuration("[C6-5]", {"18mm Motor mount -> C6-5 x1"})
                      .configuration("[C12-6]", {"18mm Motor mount -> C12-6 x1"})
                      .configuration("[D16-6]", {"18mm Motor mount -> D16-6 x1"}));
    table.emplace(
        "Pods--powered with recovery deployment.ork",
        ExpectedFlightConfigurations().configuration(kNoMotors).configuration(
            "[2\u00D7 A10-3, A8-P]", {"Inner Tube -> A10-3 x2", "Inner Tube -> A8-P x1"}));
    table.emplace("Simulation extensions.ork",
                  ExpectedFlightConfigurations().configuration(kNoMotors).configuration(
                      "[L540-P]", {"Inner Tube -> L540-P x1"}));
    table.emplace("Simulation scripting.ork",
                  ExpectedFlightConfigurations().configuration(kNoMotors).configuration(
                      "[L540-P]", {"Inner Tube -> L540-P x1"}));
    table.emplace(
        "Three stage low power rocket.ork",
        ExpectedFlightConfigurations()
            .configuration(kNoMotors)
            .configuration("[A8-5; B6-0; B6-0]", {"Inner Tube -> A8-5 x1", "Inner Tube -> B6-0 x1",
                                                  "Inner Tube -> B6-0 x1"})
            .configuration("[C6-5; B6-0; B6-0]", {"Inner Tube -> B6-0 x1", "Inner Tube -> B6-0 x1",
                                                  "Inner Tube -> C6-5 x1"})
            .configuration("[C6-7; C6-0; C6-0]", {"Inner Tube -> C6-0 x1", "Inner Tube -> C6-0 x1",
                                                  "Inner Tube -> C6-7 x1"}));
    table.emplace("Tube fin rocket.ork",
                  ExpectedFlightConfigurations().configuration(kNoMotors).configuration(
                      "[D12-7]", {"Body tube -> D12-7 x1"}));
    table.emplace("Two stage high power rocket.ork",
                  ExpectedFlightConfigurations()
                      .configuration(kNoMotors)
                      .configuration("[H148R-0; H148R-0]", {"Booster motor mount -> H148-0 x1",
                                                            "Sustainer Motor Mount -> H148-0 x1"})
                      .configuration("[I59WN-P; I357T-14]", {"Booster motor mount -> I357-14 x1",
                                                             "Sustainer Motor Mount -> I59-P x1"}));
    return table;
}

/// ExampleFilesTest.EXPECTED_METRICS: the design metrics (structure mass, its centre, the
/// length, the bounds), the centre of pressure and the drag coefficient at six Mach numbers,
/// and of every simulation the launch mass, its centre and the maximum altitude (the last is
/// Java's for its own turbulence and is not compared, see the top of this file).
[[nodiscard]] std::map<std::string, ExpectedMetrics, std::less<>> allExpectedMetrics()
{
    std::map<std::string, ExpectedMetrics, std::less<>> table;
    // NOLINTBEGIN(modernize-use-std-numbers): Java's literals; some lie near a constant of
    // <numbers>
    table.emplace(
        "3D printable nose cone and fins.ork",
        ExpectedMetrics()
            .design(0.057419604, 0.214561205, 0.000218314, 0.000066745, 0.354000000, 0.000000000,
                    -0.022063625, -0.037215320, 0.354000000, 0.042395200, 0.036215320)
            .aerodynamics(0.3, 0.269160482, 0.772230385)
            .aerodynamics(0.8, 0.272833141, 0.938378897)
            .aerodynamics(1.0, 0.275089490, 1.024030296)
            .aerodynamics(1.2, 0.275963518, 0.992291810)
            .aerodynamics(2.0, 0.254288319, 0.780300301)
            .aerodynamics(3.0, 0.229694915, 0.634933624)
            .simulation("Simulation 1", 0.073769604, 0.233852135, 0.000169928, 0.000051952,
                        38.527531869)
            .simulation("Simulation 2", 0.073019604, 0.233156282, 0.000171673, 0.000052486,
                        116.924671859)
            .simulation("Simulation 3 - too short delay", 0.080519604, 0.239531474, 0.000155683,
                        0.000047597, 241.992437712)
            .simulation("Simulation 4", 0.080519604, 0.239531474, 0.000155683, 0.000047597,
                        270.290597708)
            .simulation("Simulation 5", 0.080519604, 0.239531474, 0.000155683, 0.000047597,
                        271.042340423));
    table.emplace(
        "A simple model rocket.ork",
        ExpectedMetrics()
            .design(0.048159131, 0.205520726, 0.000140925, 0.000048525, 0.425400000, 0.000000000,
                    -0.022116025, -0.037306080, 0.425400000, 0.042500000, 0.036306080)
            .aerodynamics(0.3, 0.330558027, 0.637648221)
            .aerodynamics(0.8, 0.334770851, 0.807991801)
            .aerodynamics(1.0, 0.337095157, 0.900091037)
            .aerodynamics(1.2, 0.337695939, 0.876988207)
            .aerodynamics(2.0, 0.309035047, 0.689469038)
            .aerodynamics(3.0, 0.277388013, 0.567382662)
            .simulation("Simulation 1", 0.064509131, 0.247968766, 0.000105208, 0.000036226,
                        50.461283726)
            .simulation("Simulation 2", 0.067059131, 0.252723221, 0.000101207, 0.000034848,
                        134.804540725)
            .simulation("Simulation 3 - too short delay", 0.071259131, 0.259812312, 0.000095242,
                        0.000032794, 277.739071145)
            .simulation("Simulation 4", 0.071259131, 0.259812312, 0.000095242, 0.000032794,
                        315.464455424)
            .simulation("Simulation 5", 0.071259131, 0.259812312, 0.000095242, 0.000032794,
                        318.480495989));
    table.emplace(
        "ARC payload rocket.ork",
        ExpectedMetrics()
            .design(0.267512145, 0.509532877, 0.000000000, 0.000000000, 1.066800000, 0.000000000,
                    -0.043076988, -0.073111532, 1.066800000, 0.083555900, 0.071611532)
            .aerodynamics(0.3, 0.831719401, 0.665475612)
            .aerodynamics(0.8, 0.849325364, 0.752012332)
            .aerodynamics(1.0, 0.856741756, 0.798856979)
            .aerodynamics(1.2, 0.834119436, 0.776327322)
            .aerodynamics(2.0, 0.610801694, 0.597702893)
            .aerodynamics(3.0, 0.496521928, 0.488251307)
            .simulation("Simulation 1", 0.357312145, 0.637271014, 0.000000000, 0.000000000,
                        450.039668874));
    table.emplace(
        "Airstart timing.ork",
        ExpectedMetrics()
            .design(9.859292436, 0.920262936, 0.000000000, 0.000000000, 2.566202786, 0.000000000,
                    -0.110976238, -0.190716483, 2.566202786, 0.219354400, 0.189216483)
            .aerodynamics(0.3, 1.719587673, 0.360224115)
            .aerodynamics(0.8, 1.747074534, 0.423734710)
            .aerodynamics(1.0, 1.747337261, 0.458894343)
            .aerodynamics(1.2, 1.746385457, 0.437416393)
            .aerodynamics(2.0, 1.450921861, 0.270142506)
            .aerodynamics(3.0, 1.189405316, 0.184524395)
            .simulation("Simulation 1", 12.745756436, 1.255733620, 0.000000000, 0.000000000,
                        1312.626508948)
            .simulation("Simulation 2", 12.745756436, 1.255733620, 0.000000000, 0.000000000,
                        1283.961263427)
            .simulation("Simulation 3", 12.745756436, 1.255733620, 0.000000000, 0.000000000,
                        1286.934653122)
            .simulation("Simulation 4", 12.745756436, 1.255733620, 0.000000000, 0.000000000,
                        1292.324182636)
            .simulation("Simulation 5", 12.745756436, 1.255733620, 0.000000000, 0.000000000,
                        1256.775679343));
    table.emplace(
        "Chute release.ork",
        ExpectedMetrics()
            .design(0.910751564, 0.421265938, -0.000111763, 0.000000000, 1.102146640, 0.000000000,
                    -0.057561223, -0.097317713, 1.102146640, 0.110998000, 0.094936463)
            .aerodynamics(0.3, 0.859854217, 0.538751642)
            .aerodynamics(0.8, 0.870619450, 0.707244775)
            .aerodynamics(1.0, 0.876820330, 0.799563822)
            .aerodynamics(1.2, 0.878818332, 0.781063297)
            .aerodynamics(2.0, 0.808642509, 0.612155438)
            .aerodynamics(3.0, 0.729260003, 0.503503435)
            .simulation("Simulation 2", 1.033751564, 0.488052792, -0.000098465, 0.000000000,
                        306.635548402)
            .simulation("Simulation 3", 1.015651564, 0.479239876, -0.000100220, 0.000000000,
                        510.162252899));
    table.emplace(
        "Clustered motors.ork",
        ExpectedMetrics()
            .design(0.169955760, 0.350398815, 0.000161170, 0.000028419, 0.705000000, 0.000000000,
                    -0.045049038, -0.076527223, 0.705000000, 0.087500000, 0.075027223)
            .aerodynamics(0.3, 0.549481999, 0.628446332)
            .aerodynamics(0.8, 0.563084510, 0.781219011)
            .aerodynamics(1.0, 0.570408159, 0.893819226)
            .aerodynamics(1.2, 0.556761674, 0.925946816)
            .aerodynamics(2.0, 0.398683533, 0.877818412)
            .aerodynamics(3.0, 0.321419999, 0.789779196)
            .simulation("Simulation 1", 0.235355760, 0.440042329, 0.000116385, 0.000020522,
                        57.697089973)
            .simulation("Simulation 2", 0.245555760, 0.449719025, 0.000111550, 0.000019669,
                        141.651988796)
            .simulation("Simulation 3 - too short delay", 0.262355760, 0.464016863, 0.000104407,
                        0.000018410, 277.916172643)
            .simulation("Simulation 4", 0.262355760, 0.464016863, 0.000104407, 0.000018410,
                        305.142914961)
            .simulation("Simulation 5", 0.262355760, 0.464016863, 0.000104407, 0.000018410,
                        305.704810293));
    table.emplace(
        "Deployable payload.ork",
        ExpectedMetrics()
            .design(0.065362828, 0.230802713, 0.000103833, 0.000035753, 0.506600000, 0.000000000,
                    -0.022116025, -0.037306080, 0.506600000, 0.042500000, 0.036306080)
            .aerodynamics(0.3, 0.409771450, 0.671180597)
            .aerodynamics(0.8, 0.415263987, 0.839663147)
            .aerodynamics(1.0, 0.419181545, 0.929531734)
            .aerodynamics(1.2, 0.417128350, 0.903859958)
            .aerodynamics(2.0, 0.362252931, 0.709141659)
            .aerodynamics(3.0, 0.308693031, 0.580297513)
            .simulation("Simulation 1", 0.081712828, 0.279584352, 0.000083057, 0.000028599,
                        31.720105774)
            .simulation("Simulation 2", 0.084262828, 0.285486004, 0.000080544, 0.000027733,
                        95.211002105)
            .simulation("Simulation 3 - too short delay", 0.088462828, 0.294464677, 0.000076720,
                        0.000026417, 230.443880143)
            .simulation("Simulation 4", 0.088462828, 0.294464677, 0.000076720, 0.000026417,
                        261.088187032)
            .simulation("Simulation 5", 0.088462828, 0.294464677, 0.000076720, 0.000026417,
                        262.319051721));
    table.emplace(
        "Dual parachute deployment.ork",
        ExpectedMetrics()
            .design(1.360777109, 0.885775814, -0.000056928, 0.000000000, 1.477010000, 0.000000000,
                    -0.053283363, -0.091108391, 1.477010000, 0.104521000, 0.089927291)
            .aerodynamics(0.3, 1.233658682, 0.817700126)
            .aerodynamics(0.8, 1.242077600, 0.899589201)
            .aerodynamics(1.0, 1.251703409, 0.938357388)
            .aerodynamics(1.2, 1.262490750, 0.905412252)
            .aerodynamics(2.0, 1.191623817, 0.702838307)
            .aerodynamics(3.0, 1.075151017, 0.569778819)
            .simulation("Simulation 1", 1.612777109, 0.962082593, -0.000047270, 0.000000000,
                        592.018467244)
            .simulation("Simulation 2", 1.629577109, 0.966738611, -0.000046783, 0.000000000,
                        660.314539436)
            .simulation("Simulation 3", 2.246921109, 1.025063116, -0.000033929, 0.000000000,
                        2219.003994812)
            .simulation("Simulation 4", 1.691777109, 0.978182861, -0.000045063, 0.000000000,
                        895.933154796)
            .simulation("Simulation 5", 1.782777109, 0.993667955, -0.000042763, 0.000000000,
                        1156.476267928)
            .simulation("Simulation 6", 1.511977109, 0.933373665, -0.000050422, 0.000000000,
                        225.919093339));
    table.emplace(
        "Parallel booster staging.ork",
        ExpectedMetrics()
            .design(0.608323060, 0.563665061, 0.000000000, -0.000091181, 1.143000000, 0.000000000,
                    -0.095594428, -0.058461467, 1.143000000, 0.095594428, 0.058461467)
            .aerodynamics(0.3, 1.000159223, 1.621998911)
            .aerodynamics(0.8, 1.007210684, 1.949674986)
            .aerodynamics(1.0, 1.012177226, 2.237113748)
            .aerodynamics(1.2, 1.010776934, 2.313743861)
            .aerodynamics(2.0, 0.935690274, 2.057921971)
            .aerodynamics(3.0, 0.859280655, 1.882448987)
            .simulation("Simulation 1", 1.308123060, 0.839750466, 0.000000000, -0.000042402,
                        1117.700336058)
            .simulation("Simulation 2", 1.188323060, 0.813006991, 0.000000000, -0.000046677,
                        846.737497406));
    table.emplace(
        "Pods--airframes and winglets.ork",
        ExpectedMetrics()
            .design(0.058709249, 0.212237045, 0.000265925, -0.000000000, 0.441325000, 0.000000000,
                    -0.028911746, -0.081368900, 0.441325000, 0.049758600, 0.081368900)
            .aerodynamics(0.3, 0.346316323, 0.821718035)
            .aerodynamics(0.8, 0.351490139, 1.007636150)
            .aerodynamics(1.0, 0.355990018, 1.113618245)
            .aerodynamics(1.2, 0.357955574, 1.084838004)
            .aerodynamics(2.0, 0.338258107, 0.850365291)
            .aerodynamics(3.0, 0.315263350, 0.709725705)
            .simulation("Simulation 1", 0.075059249, 0.255898027, 0.000207999, -0.000000000,
                        29.826989419)
            .simulation("Simulation 2", 0.074309249, 0.254315684, 0.000210098, -0.000000000,
                        92.350436331)
            .simulation("Simulation 3", 0.081809249, 0.268833540, 0.000190837, -0.000000000,
                        196.236148374)
            .simulation("Simulation 4", 0.082409249, 0.268644179, 0.000189448, -0.000000000,
                        205.261032084)
            .simulation("Simulation 5", 0.084409249, 0.271954973, 0.000184959, -0.000000000,
                        243.625610375));
    table.emplace(
        "Pods--powered with recovery deployment.ork",
        ExpectedMetrics()
            .design(0.065375956, 0.054377603, 0.000000000, 0.000075460, 0.342310000, -0.142535000,
                    -0.071838936, -0.050096536, 0.199775000, 0.071838936, 0.050096536)
            .aerodynamics(0.3, 0.139542077, 2.424203830)
            .aerodynamics(0.8, 0.144256136, 2.802372537)
            .aerodynamics(1.0, 0.148211933, 3.034594111)
            .aerodynamics(1.2, 0.148165315, 3.041411948)
            .aerodynamics(2.0, 0.116403821, 2.766620740)
            .aerodynamics(3.0, 0.095927996, 2.557657227)
            .simulation("Simulation 1", 0.098725956, 0.088755626, 0.000000000, 0.000049969,
                        90.808973921));
    table.emplace(
        "Simulation extensions.ork",
        ExpectedMetrics()
            .design(7.003644847, 1.359884995, -0.000000000, 0.000000000, 2.705000000, 0.000000000,
                    -0.217426743, -0.170000000, 2.705000000, 0.224246213, 0.255000000)
            .aerodynamics(0.3, 1.829863039, 0.567308624)
            .aerodynamics(0.8, 1.807889809, 0.688743234)
            .aerodynamics(1.0, 1.795690477, 0.754337716)
            .aerodynamics(1.2, 1.872173847, 0.729775546)
            .aerodynamics(2.0, 1.977280718, 0.545401992)
            .aerodynamics(3.0, 1.825413004, 0.433534502)
            .simulation("Active roll control", 12.659644847, 1.774095862, -0.000000000, 0.000000000,
                        2409.556289273)
            .simulation("No controlling", 12.659644847, 1.774095862, -0.000000000, 0.000000000,
                        2409.788229823)
            .simulation("Roll control + air-start", 12.659644847, 1.774095862, -0.000000000,
                        0.000000000, 2883.098817217));
    table.emplace(
        "Simulation scripting.ork",
        ExpectedMetrics()
            .design(7.003641463, 1.359885249, -0.000000000, 0.000000000, 2.705000000, 0.000000000,
                    -0.217426743, -0.170000000, 2.705000000, 0.224246213, 0.255000000)
            .aerodynamics(0.3, 1.829858157, 0.567308164)
            .aerodynamics(0.8, 1.807880862, 0.688742800)
            .aerodynamics(1.0, 1.795676203, 0.754337312)
            .aerodynamics(1.2, 1.872162141, 0.729775178)
            .aerodynamics(2.0, 1.977282801, 0.545401722)
            .aerodynamics(3.0, 1.825413770, 0.433534324)
            .simulation("Active roll control", 12.659641463, 1.774096113, -0.000000000, 0.000000000,
                        2410.145768488)
            .simulation("No controlling", 12.659641463, 1.774096113, -0.000000000, 0.000000000,
                        2410.145768488)
            .simulation("Roll control + air-start", 12.659641463, 1.774096113, -0.000000000,
                        0.000000000, 2410.249916711));
    table.emplace(
        "Three stage low power rocket.ork",
        ExpectedMetrics()
            .design(0.079890089, 0.315866459, 0.000169905, 0.000058503, 0.560000000, 0.000000000,
                    -0.027116025, -0.045966334, 0.560000000, 0.052500000, 0.044966334)
            .aerodynamics(0.3, 0.433827752, 1.190211918)
            .aerodynamics(0.8, 0.436751396, 1.638487224)
            .aerodynamics(1.0, 0.440596280, 1.883371749)
            .aerodynamics(1.2, 0.443931326, 1.860449296)
            .aerodynamics(2.0, 0.438041610, 1.638295249)
            .aerodynamics(3.0, 0.422432711, 1.444875685)
            .simulation("Simulation 1", 0.127440089, 0.363962391, 0.000106510, 0.000036674,
                        263.632015842)
            .simulation("Simulation 2", 0.134190089, 0.364165490, 0.000101153, 0.000034830,
                        469.098287617)
            .simulation("Simulation 3", 0.149190089, 0.376314539, 0.000090983, 0.000031328,
                        617.061091167));
    table.emplace(
        "Tube fin rocket.ork",
        ExpectedMetrics()
            .design(0.033801428, 0.332328366, -0.000066012, 0.000000000, 0.577088000, 0.000000000,
                    -0.027666716, -0.024790400, 0.577088000, 0.027666716, 0.024790400)
            .aerodynamics(0.3, 0.496639644, 1.784827589)
            .aerodynamics(0.8, 0.496639644, 1.913825697)
            .aerodynamics(1.0, 0.496639644, 1.989120709)
            .aerodynamics(1.2, 0.496639644, 1.970916577)
            .aerodynamics(2.0, 0.496639644, 1.783745978)
            .aerodynamics(3.0, 0.497943848, 1.634371956)
            .simulation("Simulation 1", 0.076401428, 0.452827035, -0.000029205, 0.000000000,
                        281.866439856));
    table.emplace(
        "Two stage high power rocket.ork",
        ExpectedMetrics()
            .design(1.956061854, 1.226799029, -0.000094066, 0.000000000, 2.051050000, 0.000000000,
                    -0.109249038, -0.187724885, 2.051050000, 0.215900000, 0.186224885)
            .aerodynamics(0.3, 1.558244177, 0.896841955)
            .aerodynamics(0.8, 1.575521369, 1.016227137)
            .aerodynamics(1.0, 1.590011846, 1.083939204)
            .aerodynamics(1.2, 1.586656106, 1.071367976)
            .aerodynamics(2.0, 1.428040616, 0.913285239)
            .aerodynamics(3.0, 1.296000437, 0.803539806)
            .simulation("Simulation 1", 2.574301854, 1.330247457, -0.000071475, 0.000000000,
                        667.083070858)
            .simulation("Simulation 2", 2.792501854, 1.330015190, -0.000065890, 0.000000000,
                        1371.496561284));
    // NOLINTEND(modernize-use-std-numbers)
    return table;
}

/// The names the entries of @p table have.
template <class Value>
[[nodiscard]] std::set<std::string> keysOf(const std::map<std::string, Value, std::less<>>& table)
{
    std::set<std::string> keys;
    for (const auto& [key, value] : table)
    {
        keys.insert(key);
    }
    return keys;
}

/// ExampleFilesTest.countRelevantWarnings(). (A warning always has a priority here; Java
/// counts one without as normal.)
[[nodiscard]] WarningCounts countRelevantWarnings(const WarningSet& warnings)
{
    WarningCounts counts;
    for (const Warning& warning : warnings)
    {
        switch (warning.priority())
        {
            case MessagePriority::LOW:
                counts.informative++;
                break;
            case MessagePriority::NORMAL:
                counts.normal++;
                break;
            case MessagePriority::HIGH:
                counts.critical++;
                break;
        }
    }
    return counts;
}

/// ExampleFilesTest.formatWarnings().
[[nodiscard]] std::string formatWarnings(const WarningSet& warnings)
{
    std::string text;
    for (const Warning& warning : warnings)
    {
        text += std::format("- {}: {}\n", exportLabel(warning.priority()), warning.toString());
    }
    return text;
}

/// ExampleFilesTest.describeActiveMotors(): "<mount> -> <motor> x<count>" for every motor of
/// @p activeMotors, sorted.
[[nodiscard]] Lines describeActiveMotors(const std::vector<MotorConfiguration>& activeMotors,
                                         const Preferences&                     preferences)
{
    Lines motors;
    for (const MotorConfiguration& motorConfiguration : activeMotors)
    {
        const MotorMount& mount     = motorConfiguration.getMount();
        const auto* const component = dynamic_cast<const RocketComponent*>(&mount);
        motors.push_back(std::format("{} -> {} x{}",
                                     component == nullptr ? "<no component>" : component->getName(),
                                     motorConfiguration.toMotorName(preferences),
                                     mount.getMotorCountIncludingAssemblyCopies()));
    }
    std::ranges::sort(motors);
    return motors;
}

/// ExampleFilesTest.describeFlightConfigurations(): every flight configuration of @p rocket,
/// the default one first, as Java's iteration of Rocket.getFlightConfigurations() gives them.
[[nodiscard]] std::vector<FlightConfigurationSnapshot> describeFlightConfigurations(
    const Rocket& rocket, const Preferences& preferences)
{
    std::vector<FlightConfigurationSnapshot> configurations;
    for (const FlightConfiguration& configuration : rocket.getFlightConfigurations().values())
    {
        configurations.push_back(
            {.name   = configuration.getName(preferences),
             .motors = describeActiveMotors(configuration.getActiveMotors(), preferences)});
    }
    return configurations;
}

/// ExampleFilesTest.formatFlightConfigurations().
[[nodiscard]] std::string formatFlightConfigurations(
    const std::vector<FlightConfigurationSnapshot>& configurations)
{
    std::string text;
    for (const FlightConfigurationSnapshot& configuration : configurations)
    {
        text += std::format("- {}\n", configuration.name);
        if (configuration.motors.empty())
        {
            text += "  <no active motors>\n";
            continue;
        }
        for (const std::string& motor : configuration.motors)
        {
            text += std::format("  {}\n", motor);
        }
    }
    return text;
}

/// ExampleFilesTest.findNoMotorsConfiguration(): the flight configuration of @p rocket that is
/// named "[No motors]", or null (Java: an IllegalStateException).
[[nodiscard]] const FlightConfiguration* findNoMotorsConfiguration(const Rocket&      rocket,
                                                                   const Preferences& preferences)
{
    for (const FlightConfiguration& configuration : rocket.getFlightConfigurations().values())
    {
        if (configuration.getName(preferences) == kNoMotors)
        {
            return &configuration;
        }
    }
    return nullptr;
}

/// ExampleFilesTest.calculateAerodynamicMetrics(): "CP and CD at fixed sea-level,
/// zero-angle-of-attack conditions across the subsonic, transonic, and supersonic regimes. The
/// complete motorless rocket is used so motor selection cannot change the aerodynamic
/// regression baseline."
[[nodiscard]] std::map<double, AerodynamicMetrics> calculateAerodynamicMetrics(
    const FlightConfiguration& configuration)
{
    std::map<double, AerodynamicMetrics> metrics;
    BarrowmanCalculator                  calculator;
    for (const double mach : kAerodynamicMaches)
    {
        FlightConditions conditions(configuration);
        conditions.setMach(mach);
        conditions.setAOA(0.0);
        WarningSet              warnings;
        const Coordinate        cp = calculator.getCP(configuration, conditions, &warnings);
        const AerodynamicForces forces =
            calculator.getAerodynamicForces(configuration, conditions, &warnings);
        metrics.insert_or_assign(mach, AerodynamicMetrics{.cpX = cp.x, .cd = forces.getCD()});
    }
    return metrics;
}

/// ExampleFilesTest.describeDesignMetrics(), for the configuration @p noMotors of the rocket
/// (findNoMotorsConfiguration()): the structure, the bounds and the aerodynamics of a copy of
/// it with every stage active, computed in this order, and then the length, which Java asks
/// last, as an argument of the constructor of its result.
[[nodiscard]] DesignMetrics describeDesignMetrics(const FlightConfiguration& noMotors)
{
    FlightConfiguration configuration = noMotors.clone();
    configuration.setAllStages();

    const RigidBody   structure = MassCalculator::calculateStructure(configuration);
    const BoundingBox bounds    = configuration.getBoundingBox();
    DesignMetrics     metrics;
    metrics.aerodynamics = calculateAerodynamicMetrics(configuration);
    metrics.dryMass      = structure.getMass();
    metrics.dryCg        = CoordinateSnapshot::from(structure.getCM());
    metrics.length       = configuration.getLength();
    metrics.bounds       = {.min = CoordinateSnapshot::from(bounds.min()),
                            .max = CoordinateSnapshot::from(bounds.max())};
    return metrics;
}

/// ExampleFilesTest.describePreflightMetrics(): "launch properties before a simulation can
/// change any flight state". The maximum altitude is not known yet (Java: NaN).
[[nodiscard]] SimulationMetrics describePreflightMetrics(Simulation& simulation)
{
    const RigidBody launch = MassCalculator::calculateLaunch(simulation.getActiveConfiguration());
    return {.launchMass  = launch.getMass(),
            .launchCg    = CoordinateSnapshot::from(launch.getCM()),
            .maxAltitude = std::numeric_limits<double>::quiet_NaN()};
}

/// ExampleFilesTest.pinSimulationRandomness(): "A fixed stepper seed and wind-model seed make
/// altitude a property of the saved design." The seed is Java's 0, but for the measurements at
/// the end of this file.
void pinSimulationRandomness(Simulation& simulation, int seed = kSimulationRandomSeed)
{
    SimulationOptions& options = simulation.getOptions();
    options.setRandomSeed(seed);
    options.setRandomSeedFixed(true);
    options.getWindModel().setSeed(seed);
}

/// What the loop of the Java test finds of one simulation.
struct SimulationRecord
{
    std::string name;
    /// The launch mass and its centre before the run, and the maximum altitude of the run.
    SimulationMetrics metrics;
    /// The warnings of the run by priority, each as "<priority> <text>", and the type of each
    /// (Warning::typeName()).
    WarningCounts warnings;
    Lines         warningTexts;
    Lines         warningTypes;
    /// The speed at which the recovery device of the first branch was deployed, as the flight
    /// data has it (FlightData::getDeploymentVelocity(): the speed of the stored rows at the
    /// time of the deployment).
    double deploymentVelocity{std::numeric_limits<double>::quiet_NaN()};
    /// The speed above which the flight warns of a deployment
    /// (SimulationOptions::getRecoverySpeedWarning(): 20 m/s unless the design says otherwise).
    double recoverySpeedWarning{std::numeric_limits<double>::quiet_NaN()};
    /// The speed each warning of a deployment at high speed names
    /// (Warning::RecoveryHighSpeedDeployment::speed()), in the order of the warnings.
    std::vector<double> highSpeedDeployments;
    /// Why the run failed (Java: the exception simulate() throws), when it did.
    std::optional<std::string> failure;
    /// Simulation::hasErrors() after the run.
    bool hasErrors{false};
};

/// The body of the loop of the Java test for @p simulation, without its assertions: the launch
/// mass and its centre before anything else, then the seeds, then the flight, and what the
/// flight left.
[[nodiscard]] SimulationRecord flySimulation(Simulation& simulation,
                                             int         seed = kSimulationRandomSeed)
{
    SimulationRecord record;
    record.name    = simulation.getName();
    record.metrics = describePreflightMetrics(simulation);
    pinSimulationRandomness(simulation, seed);
    // "Warning expectations must not depend on the random seed generated when the simulation
    // options are constructed."
    simulation.getOptions().setRandomSeed(seed);
    simulation.getOptions().setRandomSeedFixed(true);
    const Result<void> flight = simulation.simulate();
    if (!flight)
    {
        record.failure = flight.error().toString();
        return record;
    }

    const WarningSet* const warnings = simulation.getSimulatedWarnings();
    record.warnings = warnings == nullptr ? kMissingWarnings : countRelevantWarnings(*warnings);
    const std::shared_ptr<FlightData>& data = simulation.getSimulatedData();
    if (data == nullptr)
    {
        record.failure = "no simulated data";
        return record;
    }
    record.metrics.maxAltitude  = data->getMaxAltitude();
    record.deploymentVelocity   = data->getDeploymentVelocity();
    record.recoverySpeedWarning = simulation.getOptions().getRecoverySpeedWarning();
    record.hasErrors            = simulation.hasErrors();
    if (warnings != nullptr)
    {
        for (const Warning& warning : *warnings)
        {
            record.warningTexts.push_back(
                std::format("{} {}", exportLabel(warning.priority()), warning.toString()));
            record.warningTypes.emplace_back(warning.typeName());
            const auto* const atHighSpeed =
                dynamic_cast<const Warning::RecoveryHighSpeedDeployment*>(&warning);
            if (atHighSpeed != nullptr)
            {
                record.highSpeedDeployments.push_back(atHighSpeed->speed());
            }
        }
    }
    return record;
}

/// The loop of the Java test over the simulations of @p document, in their order. (Java's
/// test ends at the first flight that throws; here every simulation is flown, and the failure
/// is in its record.)
[[nodiscard]] std::vector<SimulationRecord> flySimulations(const OpenRocketDocument& document,
                                                           int seed = kSimulationRandomSeed)
{
    std::vector<SimulationRecord> records;
    for (const std::shared_ptr<Simulation>& simulation : document.getSimulations())
    {
        records.push_back(flySimulation(*simulation, seed));
    }
    return records;
}

/// A simulation whose flight deploys its recovery device at a speed around the 20 m/s above
/// which a deployment is warned of (Warning::RecoveryHighSpeedDeployment), so that whether the
/// flight has that warning depends on the realisation of the turbulence.
struct DeploymentAtTheLimit
{
    std::string_view file;
    std::string_view simulation;
};

/// The one such simulation of the examples. Java's table expects no warning for it, which
/// holds for the flight OpenRocket flies with seed 0: flown with the seeds 0 to 31, OpenRocket
/// deploys at 16.3 to 20.9 m/s and warns with the seeds 17 and 29 (SeedProbe of the probes of
/// tier 9c, part "examples"). QtRocket's turbulence is another sequence, and not the same in
/// every standard library either (see PinkNoise): with glibc and the seeds 0 to 31 it deploys
/// at 16.3 to 20.8 m/s here and warns with seed 1 (the measurements at the end of this file).
/// No other flight of the examples is near the limit for its spread: "Three stage low power
/// rocket.ork" Simulation 2 deploys at 17.4 to 19.3 m/s and "Parallel booster staging.ork"
/// Simulation 2 at 15.7 to 19.1 m/s in OpenRocket's 32 flights (17.3 to 19.1 and 15.3 to
/// 18.6 m/s in QtRocket's), without a warning in any, and the flights that deploy closer to
/// 20 m/s do so within 0.05 m/s whatever the seed (19.0 to 19.6 m/s).
///
/// What is asserted of such a simulation in place of Java's "no such warning" is what holds
/// whatever the realisation: see deploymentContradictions().
constexpr std::array<DeploymentAtTheLimit, 1> kDeploymentsAtTheLimit{
    {{.file = "Three stage low power rocket.ork", .simulation = "Simulation 3"}}};

/// The warning type whose presence can depend on the realisation of the turbulence.
constexpr std::string_view kHighSpeedDeployment = "RecoveryHighSpeedDeployment";

/// Whether the simulation @p simulation of the example @p fileName is one of
/// kDeploymentsAtTheLimit.
[[nodiscard]] bool isAtTheLimit(std::string_view fileName, std::string_view simulation)
{
    return std::ranges::any_of(kDeploymentsAtTheLimit, [&](const DeploymentAtTheLimit& limit) {
        return limit.file == fileName && limit.simulation == simulation;
    });
}

/// How far the deployment speed of the flight data (the speed of the stored rows at the time
/// of the deployment event) may lie from the speed the flight compared with its limit when it
/// deployed, in m/s. The flight handles an event up to a millisecond after its time (the
/// shortest step towards an event), in which a falling rocket changes its speed by 0.01 m/s;
/// in the one flight of the second measurement at the end of this file that has the warning,
/// the two speeds are 20.7595 and 20.7636 m/s.
constexpr double kDeploymentSpeedSlack = 0.05;

/// What is asserted of a simulation of kDeploymentsAtTheLimit about its warnings of a
/// deployment at high speed, in place of Java's "none", and holds whatever the realisation of
/// the turbulence: the flight deploys its recovery device, and it has such a warning exactly
/// when it deployed faster than the limit of its options. What of @p record contradicts that:
/// a warning that names a speed that is not above the limit, a warning of a flight whose data
/// has the deployment below the limit, and no warning of a flight whose data has it above the
/// limit or has none (kDeploymentSpeedSlack apart). Empty when nothing does, and for a flight
/// that failed, which is reported as that.
[[nodiscard]] Lines deploymentContradictions(std::string_view        fileName,
                                             const SimulationRecord& record)
{
    Lines problems;
    if (record.failure.has_value())
    {
        return problems;
    }
    const std::string context = std::format("{} ({})", fileName, record.name);
    const double      limit   = record.recoverySpeedWarning;
    for (const double speed : record.highSpeedDeployments)
    {
        // Not "speed <= limit": a speed that is no number is not above the limit either.
        if (!(speed > limit))
        {
            problems.push_back(
                std::format("Warning of a deployment at {} m/s, which is not above the limit of "
                            "{} m/s, for {}",
                            speed, limit, context));
        }
    }
    const bool warned = !record.highSpeedDeployments.empty();
    if (warned && !(record.deploymentVelocity > limit - kDeploymentSpeedSlack))
    {
        problems.push_back(
            std::format("Warning of a deployment at high speed, but the flight data has the "
                        "deployment at {} m/s, below the limit of {} m/s, for {}",
                        record.deploymentVelocity, limit, context));
    }
    if (!warned && !(record.deploymentVelocity < limit + kDeploymentSpeedSlack))
    {
        problems.push_back(
            std::format("No warning of a deployment at high speed, but the flight data has the "
                        "deployment at {} m/s, not below the limit of {} m/s, for {}",
                        record.deploymentVelocity, limit, context));
    }
    return problems;
}

/// deploymentContradictions() of every simulation of kDeploymentsAtTheLimit among @p records,
/// the simulations of the example @p fileName; empty when there is none.
[[nodiscard]] Lines deploymentFailures(std::string_view                     fileName,
                                       const std::vector<SimulationRecord>& records)
{
    Lines failures;
    for (const SimulationRecord& record : records)
    {
        if (isAtTheLimit(fileName, record.name))
        {
            const Lines problems = deploymentContradictions(fileName, record);
            failures.insert(failures.end(), problems.begin(), problems.end());
        }
    }
    return failures;
}

/// @p record without its warnings of a deployment at high speed.
[[nodiscard]] SimulationRecord withoutHighSpeedDeployments(const SimulationRecord& record)
{
    SimulationRecord kept = record;
    kept.warningTexts.clear();
    kept.warningTypes.clear();
    for (std::size_t i = 0; i < record.warningTypes.size(); i++)
    {
        if (record.warningTypes[i] == kHighSpeedDeployment)
        {
            // Such a warning has the normal priority.
            kept.warnings.normal--;
            continue;
        }
        kept.warningTexts.push_back(record.warningTexts[i]);
        kept.warningTypes.push_back(record.warningTypes[i]);
    }
    return kept;
}

/// @p records as the assertions look at them: what Java's test finds of every simulation of
/// the example @p fileName, but for the simulations of kDeploymentsAtTheLimit, whose warnings
/// of a deployment at high speed are left out of the warnings that are counted and compared
/// (the speeds they name stay in the record). Java expects none of them there, so what is
/// asserted of such a simulation is everything else: that its flight runs and has no error,
/// its launch mass and its centre, that it has no other warning, and that it has this one
/// exactly when it deploys above the limit (deploymentFailures()).
[[nodiscard]] std::vector<SimulationRecord> withoutWarningsOfTheRealisation(
    std::string_view fileName, const std::vector<SimulationRecord>& records)
{
    std::vector<SimulationRecord> asserted;
    asserted.reserve(records.size());
    for (const SimulationRecord& record : records)
    {
        asserted.push_back(isAtTheLimit(fileName, record.name) ? withoutHighSpeedDeployments(record)
                                                               : record);
    }
    return asserted;
}

/// The names of the simulations of @p records.
[[nodiscard]] std::set<std::string> namesOf(const std::vector<SimulationRecord>& records)
{
    std::set<std::string> names;
    for (const SimulationRecord& record : records)
    {
        names.insert(record.name);
    }
    return names;
}

/// JUnit's assertEquals(expected, actual, delta, message): adds @p message, with the two
/// values, to @p failures when they are further apart than @p delta.
void checkEquals(Lines& failures, double expected, double actual, double delta,
                 std::string_view message)
{
    const ::testing::AssertionResult result = junitEquals(expected, actual, delta);
    if (!result)
    {
        failures.push_back(std::format("{}: {}", message, result.message()));
    }
}

/// ExampleFilesTest.assertCoordinateEquals().
void checkCoordinateEquals(Lines& failures, const CoordinateSnapshot& expected,
                           const CoordinateSnapshot& actual, std::string_view message)
{
    checkEquals(failures, expected.x, actual.x, kPositionEpsilon, std::format("{} (x)", message));
    checkEquals(failures, expected.y, actual.y, kPositionEpsilon, std::format("{} (y)", message));
    checkEquals(failures, expected.z, actual.z, kPositionEpsilon, std::format("{} (z)", message));
}

/// The part of ExampleFilesTest.assertDesignMetrics() about the Mach numbers: both sets of
/// metrics are for the six of the test, and the centre of pressure and the drag coefficient
/// at each are Java's.
void checkAerodynamics(Lines& failures, std::string_view fileName,
                       const std::map<double, AerodynamicMetrics>& expected,
                       const std::map<double, AerodynamicMetrics>& actual)
{
    if (expected.size() != kAerodynamicMaches.size() || actual.size() != expected.size())
    {
        failures.push_back(std::format("Aerodynamic Mach baseline count changed for {}", fileName));
        return;
    }
    for (const double mach : kAerodynamicMaches)
    {
        const auto expectedAtMach = expected.find(mach);
        const auto actualAtMach   = actual.find(mach);
        if (expectedAtMach == expected.end() || actualAtMach == actual.end())
        {
            failures.push_back(std::format("Aerodynamic Mach baseline changed for {}", fileName));
            continue;
        }
        const std::string context = std::format("{} at Mach {:.1f}", fileName, mach);
        checkEquals(failures, expectedAtMach->second.cpX, actualAtMach->second.cpX,
                    kPositionEpsilon, std::format("CP changed for {}", context));
        checkEquals(failures, expectedAtMach->second.cd, actualAtMach->second.cd,
                    kCoefficientEpsilon, std::format("CD changed for {}", context));
    }
}

/// ExampleFilesTest.assertDesignMetrics(): what of @p actual is not @p expected within Java's
/// tolerances, with Java's messages; empty when everything is.
[[nodiscard]] Lines designFailures(std::string_view fileName, const DesignMetrics& expected,
                                   const DesignMetrics& actual)
{
    Lines failures;
    checkEquals(failures, expected.dryMass, actual.dryMass, kMassEpsilon,
                std::format("Dry mass changed for {}", fileName));
    checkCoordinateEquals(failures, expected.dryCg, actual.dryCg,
                          std::format("Dry CG changed for {}", fileName));
    checkEquals(failures, expected.length, actual.length, kPositionEpsilon,
                std::format("Physical length changed for {}", fileName));
    checkCoordinateEquals(failures, expected.bounds.min, actual.bounds.min,
                          std::format("Bounding box changed for {} (minimum)", fileName));
    checkCoordinateEquals(failures, expected.bounds.max, actual.bounds.max,
                          std::format("Bounding box changed for {} (maximum)", fileName));
    checkAerodynamics(failures, fileName, expected.aerodynamics, actual.aerodynamics);
    return failures;
}

/// The assertions of the loop of the Java test for the simulation of @p record: its flight
/// ran, its launch mass and the centre of it are Java's (assertSimulationMetrics()), its
/// warnings are Java's by priority, and it has no error.
///
/// Java also asserts the maximum altitude here, within 0.5 m of the literal that
/// @p expectedMetrics holds for the simulation. That assertion is NOT made: the altitude is
/// that of a flight in turbulence drawn from java.util.Random (see the top of this file).
void checkSimulation(Lines& failures, std::string_view fileName, const ExpectedWarnings& expected,
                     const ExpectedMetrics& expectedMetrics, const SimulationRecord& record)
{
    if (record.failure.has_value())
    {
        failures.push_back(std::format("Simulation failed for {} ({}): {}", fileName, record.name,
                                       *record.failure));
        return;
    }
    const std::string context = std::format("{} simulation '{}'", fileName, record.name);
    if (const auto metrics = expectedMetrics.simulations.find(record.name);
        metrics == expectedMetrics.simulations.end())
    {
        failures.push_back(std::format("Missing expected metrics for {}.", context));
    }
    else
    {
        checkEquals(failures, metrics->second.launchMass, record.metrics.launchMass, kMassEpsilon,
                    std::format("Launch mass changed for {}", context));
        checkCoordinateEquals(failures, metrics->second.launchCg, record.metrics.launchCg,
                              std::format("Launch CG changed for {}", context));
    }
    if (const auto warnings = expected.simulations.find(record.name);
        warnings == expected.simulations.end())
    {
        failures.push_back(std::format("Missing expected warnings configuration for {}.", context));
    }
    else if (warnings->second != record.warnings)
    {
        failures.push_back(std::format("Warnings when simulating {} ({}) (expected={}, actual={})",
                                       fileName, record.name, warnings->second.toString(),
                                       record.warnings.toString()));
        failures.insert(failures.end(), record.warningTexts.begin(), record.warningTexts.end());
    }
    if (record.hasErrors)
    {
        failures.push_back(std::format("Simulation aborted for {} ({})", fileName, record.name));
    }
}

/// checkSimulation() of every record of @p records: empty when every simulation is as Java
/// expects it.
[[nodiscard]] Lines simulationFailures(std::string_view fileName, const ExpectedWarnings& expected,
                                       const ExpectedMetrics&               expectedMetrics,
                                       const std::vector<SimulationRecord>& records)
{
    Lines failures;
    for (const SimulationRecord& record : records)
    {
        checkSimulation(failures, fileName, expected, expectedMetrics, record);
    }
    return failures;
}

// ------------------------------------------- the same statements against OpenRocket's values

/// "<name>.x=<x>", "<name>.y=<y>" and "<name>.z=<z>", added to @p lines.
void addCoordinate(Lines& lines, std::string_view name, const CoordinateSnapshot& coordinate)
{
    const auto number = [](double value) { return QtRocket::Strings::javaDoubleToString(value); };
    lines.push_back(std::format("{}.x={}", name, number(coordinate.x)));
    lines.push_back(std::format("{}.y={}", name, number(coordinate.y)));
    lines.push_back(std::format("{}.z={}", name, number(coordinate.z)));
}

/// What the test found of the simulation number @p index, in the lines of the probe.
void addSimulation(Lines& lines, std::size_t index, const SimulationRecord& record)
{
    const auto number = [](double value) { return QtRocket::Strings::javaDoubleToString(value); };
    const std::string sim = std::format("sim[{}]", index);
    lines.push_back(ascii(std::format("{} '{}'", sim, QtRocket::Test::onOneLine(record.name))));
    lines.push_back(std::format("{}.launch.mass={}", sim, number(record.metrics.launchMass)));
    addCoordinate(lines, sim + ".launch.cm", record.metrics.launchCg);
    if (record.failure.has_value())
    {
        lines.push_back(std::format("{} THROWN {}", sim, *record.failure));
        return;
    }
    lines.push_back(std::format("{}.maxAltitude={}", sim, number(record.metrics.maxAltitude)));
    lines.push_back(std::format("{}.warnings={}", sim, record.warnings.toProbeText()));
    lines.push_back(
        std::format("{}.errors={}", sim, QtRocket::Test::javaBoolean(record.hasErrors)));
    for (const std::string& warning : record.warningTexts)
    {
        lines.push_back(ascii(std::format("{} W {}", sim, QtRocket::Test::onOneLine(warning))));
    }
}

/// What the test found, in the lines of the section "junit" of ExampleOrkFiles.h, which the
/// Java probe prints at the same statements: "open=<counts>", "configuration '<name>' |
/// <motor> | ...", "design.<what>=<value>", "aero[<mach>].cp.x=<value>" and ".cd", and of
/// every simulation "sim[<n>] '<name>'", "sim[<n>].launch.<what>=<value>",
/// "sim[<n>].maxAltitude=<value>", "sim[<n>].warnings=<counts>", "sim[<n>].errors=<flag>" and
/// "sim[<n>] W <priority> <text>" for each of its warnings.
[[nodiscard]] Lines junitLines(const WarningCounts&                            openWarnings,
                               const std::vector<FlightConfigurationSnapshot>& configurations,
                               const DesignMetrics&                            design,
                               const std::vector<SimulationRecord>&            records)
{
    const auto number = [](double value) { return QtRocket::Strings::javaDoubleToString(value); };
    Lines      lines{"open=" + openWarnings.toProbeText()};
    for (const FlightConfigurationSnapshot& configuration : configurations)
    {
        std::string line =
            std::format("configuration '{}'", QtRocket::Test::onOneLine(configuration.name));
        for (const std::string& motor : configuration.motors)
        {
            line += " | " + motor;
        }
        lines.push_back(ascii(line));
    }
    lines.push_back("design.mass=" + number(design.dryMass));
    addCoordinate(lines, "design.cm", design.dryCg);
    lines.push_back("design.length=" + number(design.length));
    addCoordinate(lines, "design.min", design.bounds.min);
    addCoordinate(lines, "design.max", design.bounds.max);
    for (const auto& [mach, metrics] : design.aerodynamics)
    {
        lines.push_back(std::format("aero[{}].cp.x={}", number(mach), number(metrics.cpX)));
        lines.push_back(std::format("aero[{}].cd={}", number(mach), number(metrics.cd)));
    }
    for (std::size_t index = 0; index < records.size(); index++)
    {
        addSimulation(lines, index, records[index]);
    }
    return lines;
}

/// How a line of the section "junit" is compared.
enum class JunitLine
{
    /// A text: the same characters.
    TEXT,
    /// "<name>=<number>": the same name, and the number to a relative 1e-9 (isCloseTo()).
    NUMBER,
    /// The maximum altitude of a flight: the same name. The number is that of a flight in
    /// another turbulence than OpenRocket's and is not compared (see the top of this file).
    ALTITUDE,
    /// A warning of a flight: the same characters but for the numbers in it (the speed of a
    /// deployment is that of the flight).
    WARNING,
};

/// How the line @p line of the section "junit" is compared.
[[nodiscard]] JunitLine kindOf(std::string_view line)
{
    if (line.contains(".maxAltitude="))
    {
        return JunitLine::ALTITUDE;
    }
    if (line.starts_with("design.") || line.starts_with("aero[") || line.contains(".launch."))
    {
        return JunitLine::NUMBER;
    }
    if (line.starts_with("sim[") && line.contains("] W "))
    {
        return JunitLine::WARNING;
    }
    return JunitLine::TEXT;
}

/// @p text with '#' for every number in it: a digit and the digits and points that follow it.
[[nodiscard]] std::string withoutNumbers(std::string_view text)
{
    const auto  isDigit = [](char c) { return c >= '0' && c <= '9'; };
    std::string result;
    std::size_t at = 0;
    while (at < text.size())
    {
        if (!isDigit(text[at]))
        {
            result += text[at++];
            continue;
        }
        result += '#';
        while (at < text.size() && (isDigit(text[at]) || text[at] == '.'))
        {
            at++;
        }
    }
    return result;
}

/// Whether the line @p found of the test is what the probe's line @p expected asks for
/// (JunitLine).
[[nodiscard]] bool isJunitLine(std::string_view expected, std::string_view found)
{
    const std::size_t equals = expected.find('=');
    switch (kindOf(expected))
    {
        case JunitLine::NUMBER:
        {
            const std::optional<double> wanted =
                QtRocket::Strings::javaParseDouble(expected.substr(equals + 1));
            const std::optional<double> given =
                found.size() > equals ? QtRocket::Strings::javaParseDouble(found.substr(equals + 1))
                                      : std::nullopt;
            return found.substr(0, equals + 1) == expected.substr(0, equals + 1) &&
                   wanted.has_value() && given.has_value() && isCloseTo(*wanted, *given);
        }
        case JunitLine::ALTITUDE:
            return found.substr(0, equals + 1) == expected.substr(0, equals + 1);
        case JunitLine::WARNING:
            return withoutNumbers(found) == withoutNumbers(expected);
        case JunitLine::TEXT:
            break;
    }
    return found == expected;
}

/// The lines of @p found that are not what the probe's lines @p expected ask for, each with
/// the probe's line; "" when all are.
[[nodiscard]] std::string wrongJunitLines(std::span<const std::string_view> expected,
                                          const Lines&                      found)
{
    std::string report;
    if (expected.size() != found.size())
    {
        report = std::format("{} lines expected, {} found\n", expected.size(), found.size());
    }
    for (std::size_t i = 0; i < std::min(expected.size(), found.size()); i++)
    {
        if (!isJunitLine(expected[i], found[i]))
        {
            report += std::format("expected {}\nfound    {}\n", expected[i], found[i]);
        }
    }
    return report;
}

class ExampleFilesTest : public ::testing::TestWithParam<GoldenExample>
{
private:
    /// A warning of a flight names a speed in the default units.
    DefaultUnitsGuard m_units;
};

// ExampleFilesTest.exampleFileMatchesExpectedValues(), statement for statement, without the
// maximum altitudes (see the top of this file), and with OpenRocket's value beside each of
// its literals.
//
// It is also what three methods of OpenRocketCoreTest ask for: testSimulateSimpleModelRocket,
// testSimulateTwoStageRocket and testSimulateAllExampleFiles (every simulation of every example
// runs without an exception, has no error afterwards and has flight data). Java flies those
// with a seed of the moment; here they are the flights of this test, with seed 0.
TEST_P(ExampleFilesTest, ExampleFileMatchesExpectedValues)
{
    const GoldenExample& example      = GetParam();
    const std::string&   fileName     = example.file;
    const auto           warningTable = allExpectations();
    const auto           expected     = warningTable.find(fileName);
    ASSERT_NE(expected, warningTable.end())
        << "Missing expected warnings configuration for example file: " << fileName;
    const auto configurationTable           = allExpectedFlightConfigurations();
    const auto expectedFlightConfigurations = configurationTable.find(fileName);
    ASSERT_NE(expectedFlightConfigurations, configurationTable.end())
        << "Missing expected flight configuration data for example file: " << fileName;
    const auto metricsTable    = allExpectedMetrics();
    const auto expectedMetrics = metricsTable.find(fileName);
    ASSERT_NE(expectedMetrics, metricsTable.end())
        << "Missing expected design and simulation metrics for example file: " << fileName;
    const ExampleOrkFile* const openRocket = tableOf(fileName);
    ASSERT_NE(openRocket, nullptr) << "ExampleOrkFiles.h has no " << fileName;

    const Result<LoadedDocument> loaded = loadGoldenExample(example);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    const OpenRocketDocument& doc         = *loaded->document;
    const Rocket&             rocket      = doc.getRocket();
    const Preferences&        preferences = goldenExampleEnvironment().preferences();

    const WarningCounts openWarnings = countRelevantWarnings(loaded->warnings);
    EXPECT_EQ(openWarnings.toString(), expected->second.open.toString())
        << "Warnings when opening " << fileName << ":\n"
        << formatWarnings(loaded->warnings);

    const std::vector<FlightConfigurationSnapshot> actualFlightConfigurations =
        describeFlightConfigurations(rocket, preferences);
    EXPECT_EQ(formatFlightConfigurations(actualFlightConfigurations),
              formatFlightConfigurations(expectedFlightConfigurations->second.configurations))
        << "Flight configurations when opening " << fileName << " did not match expected values.";

    const FlightConfiguration* const noMotors = findNoMotorsConfiguration(rocket, preferences);
    ASSERT_NE(noMotors, nullptr) << "Example rocket is missing its " << kNoMotors
                                 << " flight configuration";
    const DesignMetrics actualDesignMetrics = describeDesignMetrics(*noMotors);
    EXPECT_EQ(designFailures(fileName, expectedMetrics->second.metrics, actualDesignMetrics),
              Lines{});

    // Of the one warning that depends on the realisation of the turbulence
    // (kDeploymentsAtTheLimit) it is not asserted that the flight has none, as Java's table
    // has it, but that the flight has it exactly when it deploys above the limit.
    const std::vector<SimulationRecord> records =
        withoutWarningsOfTheRealisation(fileName, flySimulations(doc));
    EXPECT_EQ(simulationFailures(fileName, expected->second, expectedMetrics->second, records),
              Lines{});
    EXPECT_EQ(deploymentFailures(fileName, records), Lines{});
    EXPECT_EQ(keysOf(expectedMetrics->second.simulations), namesOf(records))
        << "Saved simulations for " << fileName << " did not match the metrics baseline";

    // What OpenRocket itself computes at these statements (commit 5f164fd0e), at full
    // precision.
    EXPECT_EQ(
        wrongJunitLines(openRocket->junit, junitLines(openWarnings, actualFlightConfigurations,
                                                      actualDesignMetrics, records)),
        "");
}

INSTANTIATE_TEST_SUITE_P(Examples, ExampleFilesTest, ::testing::ValuesIn(goldenExamples()),
                         QtRocket::Test::goldenExampleTestName);

/// The names of the files of the examples.
[[nodiscard]] std::set<std::string> exampleFileNames()
{
    std::set<std::string> names;
    for (const GoldenExample& example : goldenExamples())
    {
        names.insert(example.file);
    }
    return names;
}

/// How many configurations, motor lines and simulations the three tables of the Java test
/// hold: "<configurations> configurations, <motor lines> motor lines, <simulations with
/// warning counts> and <simulations with metrics> simulations, <Mach numbers> Mach numbers".
[[nodiscard]] std::string sizesOfTheTables()
{
    std::size_t configurations = 0;
    std::size_t motors         = 0;
    for (const auto& [file, expected] : allExpectedFlightConfigurations())
    {
        configurations += expected.configurations.size();
        for (const FlightConfigurationSnapshot& configuration : expected.configurations)
        {
            motors += configuration.motors.size();
        }
    }
    std::size_t counted = 0;
    for (const auto& [file, expected] : allExpectations())
    {
        counted += expected.simulations.size();
    }
    std::size_t measured = 0;
    std::size_t machs    = 0;
    for (const auto& [file, expected] : allExpectedMetrics())
    {
        measured += expected.simulations.size();
        machs += expected.metrics.aerodynamics.size();
    }
    return std::format("{} configurations, {} motor lines, {} and {} simulations, {} Mach numbers",
                       configurations, motors, counted, measured, machs);
}

// The three tables are for the sixteen examples and no other file (Java's own have a
// seventeenth entry each, for a file that is no example any more), and hold what Java's hold
// for them: 66 flight configurations with 65 motor lines, 54 simulations, six Mach numbers per
// file.
TEST(ExampleFilesTestTables, AreForTheSixteenExamples)
{
    EXPECT_EQ(exampleFileNames().size(), 16U);
    EXPECT_EQ(keysOf(allExpectations()), exampleFileNames());
    EXPECT_EQ(keysOf(allExpectedFlightConfigurations()), exampleFileNames());
    EXPECT_EQ(keysOf(allExpectedMetrics()), exampleFileNames());
    EXPECT_EQ(sizesOfTheTables(),
              "66 configurations, 65 motor lines, 54 and 54 simulations, 96 Mach numbers");
}

// ==================================================================== 2. OpenRocketCoreTest

/// The example whose file is named @p file, loaded from @p source in the environment of
/// @p presets; an Error when there is no such example.
[[nodiscard]] Result<LoadedDocument> loadExampleNamed(
    std::string_view file, ExampleSource source = ExampleSource::ORIGINAL,
    Presets presets = Presets::EXAMPLES)
{
    const GoldenExample* const example = QtRocket::Test::findGoldenExample(file);
    if (example == nullptr)
    {
        return QtRocket::fail(QtRocket::ErrorCode::NOT_FOUND, std::format("no example {}", file));
    }
    return loadGoldenExample(*example, source, presets);
}

/// What is wrong with the active motors of @p config for OpenRocketCoreTest: a motor
/// configuration without a motor, or a motor without a designation; empty when nothing is.
[[nodiscard]] Lines motorsWithoutDesignation(const FlightConfiguration& config)
{
    Lines problems;
    for (const MotorConfiguration& motorConfiguration : config.getActiveMotors())
    {
        const std::shared_ptr<const Motor>& motor = motorConfiguration.getMotor();
        if (motor == nullptr)
        {
            problems.emplace_back("Motor should not be null");
        }
        else if (motor->getDesignation().empty())
        {
            problems.emplace_back("Motor designation should not be empty");
        }
    }
    return problems;
}

// The methods of OpenRocketCoreTest that ask the motor database and load an example without
// flying it: testMotorDatabaseIsPopulated, testMotorDatabaseContainsKnownMotor,
// testLoadSimpleModelRocket, testLoadedRocketHasMotors, testLoadedRocketComponentsAreComplete,
// testRocketHasMultipleFlightConfigurations, testLoadTwoStageRocket, testLoadThreeStageRocket
// and testLoadClusteredMotorsRocket, in one test (one process, one reading of the database).
// Its three methods that fly a simulation are covered by the flights of ExampleFilesTest
// above. Its other methods are about the bindings of OpenRocket's global application object,
// which QtRocket does not have.
TEST(OpenRocketCoreTest, FindsItsMotorsAndLoadsTheExamples)
{
    // testMotorDatabaseIsPopulated, testMotorDatabaseContainsKnownMotor
    const QtRocket::ThrustCurveMotorSetDatabase& database = QtRocket::Test::bundledMotorDatabase();
    EXPECT_FALSE(database.getMotorSets().empty())
        << "Motor database should contain motors after loading";
    EXPECT_FALSE(database
                     .findMotors(std::nullopt, std::nullopt, "Estes", "B6",
                                 std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::quiet_NaN())
                     .empty())
        << "Motor database should contain Estes B6 motors";

    // testLoadSimpleModelRocket
    const Result<LoadedDocument> simple = loadExampleNamed("A simple model rocket.ork");
    ASSERT_TRUE(simple.has_value()) << simple.error().toString();
    const Rocket& rocket = simple->document->getRocket();
    EXPECT_GT(rocket.getChildCount(), 0U) << "Rocket should have child components";
    // testLoadedRocketHasMotors: "A simple model rocket.ork should have motor configurations"
    const FlightConfiguration& config = rocket.getSelectedConfiguration();
    EXPECT_TRUE(config.hasMotors()) << "Flight configuration should have motors assigned";
    EXPECT_FALSE(config.getActiveMotors().empty())
        << "There should be active motors in the configuration";
    EXPECT_EQ(motorsWithoutDesignation(config), Lines{});
    // testLoadedRocketComponentsAreComplete (a component always has a name here; Java checks
    // each for null)
    EXPECT_FALSE(rocket.getAllChildren().empty()) << "Rocket should have components";
    // testRocketHasMultipleFlightConfigurations
    EXPECT_GT(rocket.getFlightConfigurationCount(), 0)
        << "Rocket should have at least one flight configuration";

    // testLoadTwoStageRocket
    const Result<LoadedDocument> twoStages = loadExampleNamed("Two stage high power rocket.ork");
    ASSERT_TRUE(twoStages.has_value()) << twoStages.error().toString();
    EXPECT_GE(twoStages->document->getRocket().getChildCount(), 2U)
        << "Two-stage rocket should have at least 2 stage components";

    // testLoadThreeStageRocket
    const Result<LoadedDocument> threeStages = loadExampleNamed("Three stage low power rocket.ork");
    ASSERT_TRUE(threeStages.has_value()) << threeStages.error().toString();
    EXPECT_GE(threeStages->document->getRocket().getChildCount(), 3U)
        << "Three-stage rocket should have at least 3 stage components";

    // testLoadClusteredMotorsRocket
    const Result<LoadedDocument> clustered = loadExampleNamed("Clustered motors.ork");
    ASSERT_TRUE(clustered.has_value()) << clustered.error().toString();
    EXPECT_TRUE(clustered->document->getRocket().getSelectedConfiguration().hasMotors())
        << "Clustered motors rocket should have motors";
}

// ================================================================== 3. the state as loaded

/// The document @p document without what its root element holds: the start tag of
/// <openrocket>, with what stands before it, and an end tag. OpenRocketHandler takes the file
/// version from that tag alone, and reading this instead of the whole document saves a second
/// reading of the stored flights.
[[nodiscard]] std::string rootElementOf(std::string_view document)
{
    constexpr std::string_view kRoot = "<openrocket";
    const std::size_t          start = document.find(kRoot);
    const std::size_t end = start == std::string_view::npos ? start : document.find('>', start);
    if (end == std::string_view::npos)
    {
        return std::string(document);
    }
    return std::string(document.substr(0, end + 1)) + "</openrocket>";
}

/// The state of @p example as loaded from @p source in the environment of @p presets: the
/// lines of DesignFileState.h with the document's preferences and materials at the end
/// (StateDetail::WITH_DOCUMENT_PREFERENCES), as the Java probe ExampleProbe prints them.
[[nodiscard]] Lines stateLines(const GoldenExample& example, ExampleSource source, Presets presets)
{
    const DefaultUnitsGuard      units;
    const DesignFileEnvironment& environment = goldenExampleEnvironment(presets);
    const std::string document = QtRocket::Test::documentOfDesignFile(example.path(source));
    const std::string version  = QtRocket::Test::versionLine(environment, rootElementOf(document));
    const std::shared_ptr<LoadEvents> events = std::make_shared<LoadEvents>();
    const Result<LoadedDocument>      loaded =
        loadGoldenExample(example, source, presets, QtRocket::Test::countingOptions(events));
    return QtRocket::Test::linesOf(version + QtRocket::Test::loadedDocumentState(
                                                 loaded, *events,
                                                 goldenExampleEnvironment(presets).preferences(),
                                                 document, StateDetail::WITH_DOCUMENT_PREFERENCES));
}

/// An example and its state as loaded, in the short form.
struct ExampleState
{
    std::string              file;
    std::vector<CompactLine> lines;
};

/// The state of every example as loaded from @p source in the environment of @p presets; with
/// @p only, of the examples whose files it names and of no other.
[[nodiscard]] std::vector<ExampleState> statesOfTheExamples(ExampleSource source, Presets presets,
                                                            const Lines* only = nullptr)
{
    std::vector<ExampleState> states;
    for (const GoldenExample& example : goldenExamples())
    {
        // (std::ranges::find: not every standard library the project builds with has
        // std::ranges::contains.)
        if (only == nullptr || std::ranges::find(*only, example.file) != only->end())
        {
            states.push_back({.file  = example.file,
                              .lines = compactState(stateLines(example, source, presets))});
        }
    }
    return states;
}

/// The lines of ExampleOrkFiles.h for the state of @p row with component presets, or, with
/// @p withoutPresets, without them (which is the same state for a design that names none).
[[nodiscard]] std::span<const std::string_view> stateOf(const ExampleOrkFile& row,
                                                        bool                  withoutPresets)
{
    return withoutPresets && !row.stateWithoutPresets.empty() ? row.stateWithoutPresets : row.state;
}

/// The examples of @p states whose state is not OpenRocket's (ExampleOrkFiles.h), each with the
/// first line that differs; "" when every state is.
[[nodiscard]] std::string wrongStates(const std::vector<ExampleState>& states, bool withoutPresets)
{
    std::string report;
    for (const ExampleState& state : states)
    {
        const ExampleOrkFile* const row = tableOf(state.file);
        const std::string           difference =
            row == nullptr
                ? std::string("not in ExampleOrkFiles.h")
                : QtRocket::Test::firstDifference(stateOf(*row, withoutPresets), state.lines);
        if (!difference.empty())
        {
            report += std::format("{}: {}\n", state.file, difference);
        }
    }
    return report;
}

/// The number that follows @p key in @p line (its decimal digits); 0 when the key is not
/// there.
[[nodiscard]] int numberAfter(std::string_view line, std::string_view key)
{
    const std::size_t at = line.find(key);
    if (at == std::string_view::npos)
    {
        return 0;
    }
    int value = 0;
    for (std::size_t i = at + key.size(); i < line.size() && line[i] >= '0' && line[i] <= '9'; i++)
    {
        value = (value * 10) + (line[i] - '0');
    }
    return value;
}

/// The names between the brackets of "ext=[<name> <name>]" at the end of @p line: how many
/// there are.
[[nodiscard]] int extensionsOf(std::string_view line)
{
    constexpr std::string_view kMark = " ext=[";
    const std::size_t          at    = line.find(kMark);
    if (at == std::string_view::npos || line.substr(at + kMark.size()).starts_with(']'))
    {
        return 0;
    }
    return 1 + static_cast<int>(std::ranges::count(line.substr(at + kMark.size()), ' '));
}

/// What the examples hold altogether, counted in the lines of their states as loaded.
struct Totals
{
    int files{0};
    int version110{0};
    int version111{0};
    int loadWarnings{0};
    int components{0};
    int configurations{0};
    int simulations{0};
    int loaded{0};
    int branches{0};
    int rows{0};
    int events{0};
    int storedWarnings{0};
    int extensions{0};
    int simulationsWithExtensions{0};
    int motors{0};
    int mountsWithoutMotor{0};
    int decalImages{0};
    int unreadableDecalImages{0};
    int documentMaterials{0};

    /// Counts the line @p line of a state behind its indentation: the lines of the document
    /// and of the rocket.
    void countDesign(std::string_view line)
    {
        version110 += line == "version=110" ? 1 : 0;
        version111 += line == "version=111" ? 1 : 0;
        loadWarnings += line.starts_with("W ") ? 1 : 0;
        if (line.starts_with("rocket '"))
        {
            components += numberAfter(line, " components=");
            configurations += numberAfter(line, " configurations=");
        }
        if (line.starts_with("motor config="))
        {
            (line.ends_with(" none") ? mountsWithoutMotor : motors)++;
        }
        if (line.starts_with("decal '"))
        {
            decalImages++;
            unreadableDecalImages += line.ends_with(" unreadable") ? 1 : 0;
        }
        documentMaterials += line.starts_with("docmaterial ") ? 1 : 0;
    }

    /// Counts the line @p line of a state behind its indentation: the lines of the simulations
    /// and of their stored flights.
    void countSimulations(std::string_view line)
    {
        if (line.starts_with("sim '"))
        {
            simulations++;
            loaded += line.contains(" status=LOADED ") ? 1 : 0;
            branches += numberAfter(line, " branches=");
            extensions += extensionsOf(line);
            simulationsWithExtensions += extensionsOf(line) > 0 ? 1 : 0;
        }
        rows += line.starts_with("branch[") ? numberAfter(line, " rows=") : 0;
        events += line.starts_with("event ") ? 1 : 0;
        storedWarnings += line.starts_with("fw[") ? 1 : 0;
    }

    [[nodiscard]] std::string toText() const
    {
        return std::format(
            "{} files, {} of version 1.10 and {} of 1.11, {} warnings; {} components, {} "
            "configurations, {} motors and {} mounts without one; {} simulations, {} loaded, {} "
            "with {} extensions; {} branches, {} rows, {} events, {} stored warnings; {} decal "
            "images, {} unreadable; {} document materials",
            files, version110, version111, loadWarnings, components, configurations, motors,
            mountsWithoutMotor, simulations, loaded, simulationsWithExtensions, extensions,
            branches, rows, events, storedWarnings, decalImages, unreadableDecalImages,
            documentMaterials);
    }
};

/// @p line without the blanks it starts with.
[[nodiscard]] std::string_view unindented(std::string_view line)
{
    const std::size_t first = line.find_first_not_of(' ');
    return first == std::string_view::npos ? std::string_view() : line.substr(first);
}

/// What the examples of @p states hold altogether.
[[nodiscard]] Totals totalsOf(const std::vector<ExampleState>& states)
{
    Totals totals;
    for (const ExampleState& state : states)
    {
        totals.files++;
        for (const CompactLine& line : state.lines)
        {
            totals.countDesign(unindented(line.text));
            totals.countSimulations(unindented(line.text));
        }
    }
    return totals;
}

/// The lines of @p state that start with @p start, without it.
[[nodiscard]] Lines linesStartingWith(const ExampleState& state, std::string_view start)
{
    Lines lines;
    for (const CompactLine& line : state.lines)
    {
        if (line.text.starts_with(start))
        {
            lines.push_back(line.text.substr(start.size()));
        }
    }
    return lines;
}

/// The names of the decal images of @p states, each once.
[[nodiscard]] std::set<std::string> decalNames(const std::vector<ExampleState>& states)
{
    std::set<std::string> names;
    for (const ExampleState& state : states)
    {
        for (const std::string& decal : linesStartingWith(state, "decal '"))
        {
            names.insert(decal.substr(0, decal.find('\'')));
        }
    }
    return names;
}

/// The "decal '" lines of the state of the example @p file among @p states, without their
/// first word: "<name>' bytes=<n>" for every decal image of the document.
[[nodiscard]] Lines decalLinesOf(const std::vector<ExampleState>& states, std::string_view file)
{
    const auto found = std::ranges::find(states, file, &ExampleState::file);
    return found == states.end() ? Lines{"no such example"} : linesStartingWith(*found, "decal '");
}

/// A motor a mount holds: what a "motor ..." line of a state says of it.
struct LoadedMotor
{
    std::string manufacturer;
    std::string designation;
    std::string digest;
};

/// The motors of the "motor config=<n> mount=#<n> <manufacturer>|<designation>|<digest>|
/// delay=<delay>" lines of @p state; a mount without a motor is left out.
[[nodiscard]] std::vector<LoadedMotor> motorsOf(const ExampleState& state)
{
    std::vector<LoadedMotor> motors;
    for (const std::string& line : linesStartingWith(state, "motor config="))
    {
        const Lines parts = QtRocket::Strings::split(line.substr(line.rfind(' ') + 1), '|');
        if (parts.size() == 4)
        {
            const std::size_t name = line.find(' ', line.find(" mount=#") + 1) + 1;
            motors.push_back({.manufacturer = line.substr(name, line.find('|') - name),
                              .designation  = parts[1],
                              .digest       = parts[2]});
        }
    }
    return motors;
}

/// The reference of ExampleMotors.h for the motor @p motor of the example @p file, or null.
[[nodiscard]] const ExampleMotor* referenceOf(std::string_view file, const LoadedMotor& motor)
{
    const std::span<const ExampleMotor> references(kExampleMotors);
    const auto found = std::ranges::find_if(references, [&](const ExampleMotor& reference) {
        return reference.file == file && reference.manufacturer == motor.manufacturer &&
               reference.designation == motor.designation;
    });
    return found == references.end() ? nullptr : &*found;
}

/// How the motors of @p states stand to the references of ExampleMotors.h, which hold what
/// OpenRocket makes of every <motor> of the examples: "<n> motors: <n> with the digest of the
/// file, <n> with a digest of an older form of the same curve, <n> taken by their designation;
/// <n> references used", and a line for every motor that has no reference or not the digest of
/// its reference.
[[nodiscard]] std::string motorMatches(const std::vector<ExampleState>& states)
{
    std::map<ExampleMotorMatch, int> counts;
    std::set<const ExampleMotor*>    used;
    std::string                      problems;
    int                              motors = 0;
    for (const ExampleState& state : states)
    {
        for (const LoadedMotor& motor : motorsOf(state))
        {
            motors++;
            const ExampleMotor* const reference = referenceOf(state.file, motor);
            if (reference == nullptr || reference->found != motor.digest)
            {
                problems += std::format("\n{}: {} {} has the digest {}", state.file,
                                        motor.manufacturer, motor.designation, motor.digest);
                continue;
            }
            counts[reference->match]++;
            used.insert(reference);
        }
    }
    return std::format(
               "{} motors: {} with the digest of the file, {} with a digest of an older "
               "form of the same curve, {} taken by their designation; {} references used",
               motors, counts[ExampleMotorMatch::EXACT], counts[ExampleMotorMatch::COMPATIBLE],
               counts[ExampleMotorMatch::DESCRIPTION], used.size()) +
           problems;
}

/// The examples that, freshly loaded from @p source with nothing asked before, have other
/// masses, centres of mass or another length than OpenRocket finds in the original as loaded
/// (the section "numbers" of ExampleOrkFiles.h; in OpenRocket the re-save has the numbers of
/// the original, digit for digit), each with the numbers that differ; "" when none has.
[[nodiscard]] std::string examplesWithOtherMasses(ExampleSource source)
{
    std::string report;
    for (const GoldenExample& example : goldenExamples())
    {
        const ExampleOrkFile* const  row    = tableOf(example.file);
        const Result<LoadedDocument> loaded = loadGoldenExample(example, source);
        if (row == nullptr || !loaded)
        {
            report += std::format(
                "{}: {}\n", example.file,
                row == nullptr ? "not in ExampleOrkFiles.h" : loaded.error().toString());
            continue;
        }
        const std::string wrong = QtRocket::Test::wrongNumbers(
            row->numbers, QtRocket::Test::massAndLength(loaded->document->getRocket()));
        if (!wrong.empty())
        {
            report += std::format("{}:\n{}", example.file, wrong);
        }
    }
    return report;
}

// The state of each of the sixteen examples as loaded is OpenRocket's, line for line (see the
// top of this file and DesignFileState.h), and altogether they hold what OpenRocket counts in
// them. One test for all files: the bundled motor database is read once per process.
//
// The motors: each of the 65 <motor> elements gets the motor OpenRocket gives it (its digest
// is in the state, and is the one ExampleMotors.h has for the reference). Seven of them get
// another thrust curve than the one the design was saved with, silently: the database has no
// curve of the file's digest, and the motor is taken by its designation, as OpenRocket takes
// it (decision D10).
TEST(ExampleFiles, LoadIntoOpenRocketsState)
{
    const std::vector<ExampleState> states =
        statesOfTheExamples(ExampleSource::ORIGINAL, Presets::EXAMPLES);
    ASSERT_EQ(states.size(), 16U);
    EXPECT_EQ(wrongStates(states, false), "");
    EXPECT_EQ(totalsOf(states).toText(),
              "16 files, 14 of version 1.10 and 2 of 1.11, 0 warnings; 327 components, 50 "
              "configurations, 65 motors and 1 mounts without one; 54 simulations, 54 loaded, 4 "
              "with 6 extensions; 69 branches, 36962 rows, 735 events, 20 stored warnings; 23 "
              "decal images, 0 unreadable; 112 document materials");
    EXPECT_EQ(decalNames(states).size(), 17U);
    // Six images are entries of their archive whose names start with a slash, as the design
    // names them (OpenRocket's own textures): found by exactly that name, and read.
    EXPECT_EQ(decalLinesOf(states, "Parallel booster staging.ork"),
              (Lines{"/datafiles/textures/balsa.jpg' bytes=54032",
                     "/datafiles/textures/chute.jpg' bytes=6686",
                     "/datafiles/textures/hardboard.jpg' bytes=12599",
                     "/datafiles/textures/spiral-wound-alpha.png' bytes=2044",
                     "/datafiles/textures/wadding.png' bytes=12495",
                     "/datafiles/textures/wood.jpg' bytes=43305"}));
    EXPECT_EQ(motorMatches(states),
              "65 motors: 6 with the digest of the file, 52 with a digest "
              "of an older form of the same curve, 7 taken by their "
              "designation; 41 references used");
    // The masses, the centres of mass and the length of each example, asked first of a load
    // of its own (an automatic dimension stores what it computes, so the state above, which
    // asks for much else, is no place to ask for them).
    EXPECT_EQ(examplesWithOtherMasses(ExampleSource::ORIGINAL), "");
}

/// The number named @p name among @p numbers; NaN when there is none.
[[nodiscard]] double numberNamed(const std::vector<DesignNumber>& numbers, std::string_view name)
{
    const auto found = std::ranges::find(numbers, name, &DesignNumber::name);
    return found == numbers.end() ? std::numeric_limits<double>::quiet_NaN() : found->value;
}

// What a freshly loaded design answers depends on what it was asked before: an automatic
// dimension stores what it computes. "Dual parachute deployment.ork" is the one example in
// which that shows in the masses. Asked first, the centre of mass of the structure is at
// 0.8857758 m, which is what ExampleFilesTest pins (0.885775814), and the launch centre of the
// first flight configuration, asked next, at 0.9629167 m. With the automatic dimensions
// settled, as the golden harness settles them before it dumps anything, they are at
// 0.8784465 m (the value of the goldens) and 0.9620826 m. (ExampleFilesTest finds 0.962082593
// for that launch centre in the unsettled design: it asks for the bounds and the aerodynamics
// in between.) For the other fifteen examples the two states have the same masses and
// lengths, in OpenRocket too; GoldenExamples.LoadFromBothSourcesAndSettleIntoTheStateOfTheGoldens
// holds the settled state of all sixteen, and that settling changes nothing in the fifteen.
TEST(ExampleFiles, TheDualParachuteDesignIsStaleAsLoaded)
{
    const ExampleOrkFile* const row = tableOf("Dual parachute deployment.ork");
    ASSERT_NE(row, nullptr);

    // As loaded, and nothing asked before.
    const Result<LoadedDocument> loaded = loadExampleNamed(row->file);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    const std::vector<DesignNumber> asLoaded =
        QtRocket::Test::massAndLength(loaded->document->getRocket());
    EXPECT_EQ(QtRocket::Test::wrongNumbers(row->numbers, asLoaded), "");
    EXPECT_NEAR(numberNamed(asLoaded, "structure.cm.x"), 0.8857758142371108, 1e-9);
    EXPECT_NEAR(numberNamed(asLoaded, "structure.cm.x"), 0.885775814, kPositionEpsilon);
    EXPECT_NEAR(numberNamed(asLoaded, "launch[0].cm.x"), 0.9629167212690506, 1e-9);

    // Settled first, on a document of its own.
    const Result<LoadedDocument> second = loadExampleNamed(row->file);
    ASSERT_TRUE(second.has_value()) << second.error().toString();
    const Result<int> passes = QtRocket::Test::settleGoldenExample(*second);
    ASSERT_TRUE(passes.has_value()) << passes.error().toString();
    EXPECT_EQ(*passes, 1);
    EXPECT_EQ(row->settlingPasses, 1);
    const std::vector<DesignNumber> settled =
        QtRocket::Test::massAndLength(second->document->getRocket());
    EXPECT_EQ(QtRocket::Test::wrongNumbers(row->settled, settled), "");
    EXPECT_NEAR(numberNamed(settled, "structure.cm.x"), 0.8784464812860964, 1e-9);
    EXPECT_NEAR(numberNamed(settled, "launch[0].cm.x"), 0.9620825931703152, 1e-9);
}

// ============================================================== 4. without component presets

/// The warnings of the loads of @p states, by file: the texts of the "W " lines of a state
/// that has any.
[[nodiscard]] std::map<std::string, Lines> loadWarningsOf(const std::vector<ExampleState>& states)
{
    std::map<std::string, Lines> warnings;
    for (const ExampleState& state : states)
    {
        Lines texts = linesStartingWith(state, "W ");
        if (!texts.empty())
        {
            warnings.insert_or_assign(state.file, std::move(texts));
        }
    }
    return warnings;
}

/// What OpenRocket says of the six <preset> elements of the examples when its preset database
/// does not hold their presets (WarnProbe.txt of the tier 9 scouts): "No matching
/// ComponentPreset for component <name> found matching <manufacturer> <part number>", by file.
[[nodiscard]] std::map<std::string, Lines> presetWarnings()
{
    const auto warning = [](std::string_view component, std::string_view part) {
        return std::format("No matching ComponentPreset for component {} found matching Estes {}",
                           component, part);
    };
    return {
        {"3D printable nose cone and fins.ork",
         {warning("Paper Body Tube -- DO NOT PRINT", "BT-50FE, 30359")}},
        {"Deployable payload.ork",
         {warning("Parachute, plastic, preassembled, 10 in., PN 2262", "PK-10, 2262"),
          warning("Parachute kit, plastic, 8 in., PN 2260", "PK-8, 2260")}},
        {"Pods--powered with recovery deployment.ork",
         {warning("Nose Cone", "PNC-50YR, 72604"), warning("Body Tube", "BT-50, 30352"),
          warning("Nose Cone", "PNC-20, 072606")}},
    };
}

/// The examples whose state without component presets is another than with them, according
/// to OpenRocket (ExampleOrkFiles.h).
[[nodiscard]] Lines examplesThatNeedTheirPresets()
{
    Lines files;
    for (const ExampleOrkFile& row : kExampleOrkFiles)
    {
        if (!row.stateWithoutPresets.empty())
        {
            files.emplace_back(row.file);
        }
    }
    return files;
}

/// The examples whose document has a <preset> element (a tag that starts with "<preset").
[[nodiscard]] Lines examplesThatNameAPreset()
{
    Lines files;
    for (const GoldenExample& example : goldenExamples())
    {
        if (QtRocket::Test::documentOfDesignFile(example.original).contains("<preset"))
        {
            files.push_back(example.file);
        }
    }
    return files;
}

/// What loading the examples without component presets gives, but for the examples whose
/// files @p except names: "<n> loads, <n> warnings", and behind it a line for every load that
/// fails and for every warning of a load.
[[nodiscard]] std::string loadsWithoutPresets(const Lines& except)
{
    int         loads    = 0;
    int         warnings = 0;
    std::string problems;
    for (const GoldenExample& example : goldenExamples())
    {
        if (std::ranges::find(except, example.file) != except.end())
        {
            continue;
        }
        const Result<LoadedDocument> loaded =
            loadGoldenExample(example, ExampleSource::ORIGINAL, Presets::NONE);
        if (!loaded)
        {
            problems += std::format("\n{}: {}", example.file, loaded.error().toString());
            continue;
        }
        loads++;
        for (const Warning& warning : loaded->warnings)
        {
            warnings++;
            problems += std::format("\n{}: {}", example.file, warning.toString());
        }
    }
    return std::format("{} loads, {} warnings{}", loads, warnings, problems);
}

// Without component presets (the product has none before it reads preset files, Milestone 3)
// the sixteen examples load with exactly six warnings, OpenRocket's for a preset its database
// does not hold: one, two and three in the three designs that name presets. The state of each
// of the three is the one OpenRocket has then: the six components keep what their elements
// store and have no preset, the document materials stand in another order (a preset registers
// its material before the element's own), and two nose cones lose the one value their preset
// alone supplied (see the next test).
//
// The other thirteen examples are loaded without presets too, and give no warning. Their
// state is not compared a second time: their documents have no <preset> element, which is
// checked, so no load of them asks the preset database for anything, and what it holds cannot
// change what ExampleFiles.LoadIntoOpenRocketsState finds (OpenRocket's state of them is the
// same line for line without presets: ExampleOrkFiles.h has no other).
TEST(ExampleFilesWithoutPresets, GiveOpenRocketsSixWarningsAndItsState)
{
    const Lines withPresets{"3D printable nose cone and fins.ork", "Deployable payload.ork",
                            "Pods--powered with recovery deployment.ork"};
    EXPECT_EQ(examplesThatNameAPreset(), withPresets);
    EXPECT_EQ(examplesThatNeedTheirPresets(), withPresets);

    const std::vector<ExampleState> states =
        statesOfTheExamples(ExampleSource::ORIGINAL, Presets::NONE, &withPresets);
    ASSERT_EQ(states.size(), 3U);
    EXPECT_EQ(wrongStates(states, true), "");
    EXPECT_EQ(loadWarningsOf(states), presetWarnings());
    EXPECT_EQ(totalsOf(states).loadWarnings, 6);

    EXPECT_EQ(loadsWithoutPresets(withPresets), "13 loads, 0 warnings");
}

/// The nose cones of @p rocket in the order of the tree: "<name>: preset <part number>, fore
/// shoulder thickness <thickness>" each, "no preset" for one without.
[[nodiscard]] Lines noseConesOf(const Rocket& rocket)
{
    Lines lines;
    rocket.forEach([&lines](const RocketComponent& component) {
        const auto* const cone = dynamic_cast<const NoseCone*>(&component);
        if (cone != nullptr)
        {
            lines.push_back(std::format(
                "{}: {}, fore shoulder thickness {}", cone->getName(),
                cone->getPresetComponent() == nullptr
                    ? std::string("no preset")
                    : "preset " + cone->getPresetComponent()->getPartNo(),
                QtRocket::Strings::javaDoubleToString(cone->getForeShoulderThickness())));
        }
    });
    return lines;
}

// The two values of the geometry of the examples that change without presets: the fore
// shoulder thickness of the two nose cones of "Pods--powered with recovery deployment.ork". A
// nose cone's element stores no fore shoulder (it has none: its length is 0), so the
// thickness is the preset's, 0.0015748 m, or 0 without it. No mass, centre, bound or
// aerodynamic value depends on it.
TEST(ExampleFilesWithoutPresets, TwoNoseConesLoseTheThicknessOfTheirForeShoulders)
{
    constexpr std::string_view   kFile = "Pods--powered with recovery deployment.ork";
    const Result<LoadedDocument> with  = loadExampleNamed(kFile);
    ASSERT_TRUE(with.has_value()) << with.error().toString();
    EXPECT_EQ(noseConesOf(with->document->getRocket()),
              (Lines{"Nose Cone: preset PNC-50YR, 72604, fore shoulder thickness 0.0015748",
                     "Nose Cone: preset PNC-20, 072606, fore shoulder thickness 0.0015748"}));

    const Result<LoadedDocument> without =
        loadExampleNamed(kFile, ExampleSource::ORIGINAL, Presets::NONE);
    ASSERT_TRUE(without.has_value()) << without.error().toString();
    EXPECT_EQ(noseConesOf(without->document->getRocket()),
              (Lines{"Nose Cone: no preset, fore shoulder thickness 0.0",
                     "Nose Cone: no preset, fore shoulder thickness 0.0"}));
}

// ========================================================================== 5. the re-saves

/// What the lines of the state of an example say of its rocket: the components with what they
/// hold, the flight configurations, the names they show and the motors (the lines that start
/// with "| ", "rocket ", "config ", "name " and "motor "), and for every simulation its name,
/// its status and its extensions (its "sim " line without the number of branches) and its
/// flight configuration (its "simulation " line).
template <class Line>
[[nodiscard]] Lines designLinesOf(std::span<const Line> state)
{
    constexpr std::array<std::string_view, 7> kStarts{"| ",     "rocket ", "config ",    "name ",
                                                      "motor ", "sim '",   "simulation "};
    Lines                                     lines;
    for (const Line& stateLine : state)
    {
        const std::string_view line(stateLine);
        if (std::ranges::none_of(
                kStarts, [line](std::string_view start) { return line.starts_with(start); }))
        {
            continue;
        }
        // The stored branches are not the design's.
        constexpr std::string_view kBranches = " branches=";
        const std::size_t          at        = line.find(kBranches);
        if (line.starts_with("sim '") && at != std::string_view::npos)
        {
            const std::size_t end = line.find(' ', at + 1);
            lines.push_back(std::string(line.substr(0, at)) +
                            std::string(end == std::string_view::npos ? "" : line.substr(end)));
            continue;
        }
        lines.emplace_back(line);
    }
    return lines;
}

/// @p line with the name a re-save gives a decal image whose entry lies outside "decals/":
/// OpenRocket's saver writes every image as "decals/<file name>", so a texture
/// "/datafiles/textures/<file name>" of the original is "decals/<file name>" in the re-save.
[[nodiscard]] std::string withResavedDecalNames(std::string line)
{
    return QtRocket::Test::replaceAll(std::move(line), "'/datafiles/textures/", "'decals/");
}

/// The re-saves of @p states whose design is not the one OpenRocket loads from the original
/// (designLinesOf() of the state of ExampleOrkFiles.h, with the decal names of a re-save),
/// each with the first line that differs; "" when every one is.
[[nodiscard]] std::string resavesWithAnotherDesign(const std::vector<ExampleState>& states)
{
    std::string report;
    for (const ExampleState& state : states)
    {
        const ExampleOrkFile* const row = tableOf(state.file);
        if (row == nullptr)
        {
            report += std::format("{}: not in ExampleOrkFiles.h\n", state.file);
            continue;
        }
        Lines original = designLinesOf(row->state);
        for (std::string& line : original)
        {
            line = withResavedDecalNames(std::move(line));
        }
        const Lines texts = QtRocket::Test::textsOf(state.lines);
        const Lines found = designLinesOf(std::span<const std::string>(texts));
        const std::vector<std::string_view> expected(original.begin(), original.end());
        const std::string difference = QtRocket::Test::firstDifference(expected, found);
        if (!difference.empty())
        {
            report += std::format("{}: {}\n", state.file, difference);
        }
    }
    return report;
}

// OpenRocket's re-save of each example (resave/rocket.ork of its goldens: the design as
// OpenRocket loaded it, saved before anything was settled, as plain XML of file version 1.11
// without the stored flights) loads without a warning, and holds the design of the original:
// the same components with the same values, the stale automatic ones included, the same flight
// configurations with the same ids, names and motors, and the same simulations by name,
// status, extensions and flight configuration. Only the decal images differ: a re-save is
// the document alone, so none of its 23 images can be read, and the six textures of the
// parallel boosters have the names OpenRocket's saver gives them. Freshly loaded, a re-save
// has the masses, the centres of mass and the length of the original as loaded.
//
// (The comparison of the re-saves with their goldens, and of a design QtRocket saves with a
// re-save, are not made here.)
TEST(ExampleFilesResaves, LoadWithoutAWarningIntoTheDesignOfTheOriginal)
{
    const std::vector<ExampleState> states =
        statesOfTheExamples(ExampleSource::RESAVE, Presets::EXAMPLES);
    ASSERT_EQ(states.size(), 16U);
    EXPECT_EQ(resavesWithAnotherDesign(states), "");
    EXPECT_EQ(totalsOf(states).toText(),
              "16 files, 0 of version 1.10 and 16 of 1.11, 0 warnings; 327 components, 50 "
              "configurations, 65 motors and 1 mounts without one; 54 simulations, 54 loaded, 4 "
              "with 6 extensions; 0 branches, 0 rows, 0 events, 20 stored warnings; 23 decal "
              "images, 23 unreadable; 112 document materials");
    EXPECT_EQ(examplesWithOtherMasses(ExampleSource::RESAVE), "");
}

// ============================================================ for a comparison with the probe

/// Prints, for every example loaded from @p source in the environment of @p presets, what the
/// Java probe ExampleProbe prints first: "=== <name>", the lines of the state as loaded, not
/// shortened, "#numbers" and the masses, centres of mass and the length of a second load.
void printStates(ExampleSource source, Presets presets)
{
    for (const GoldenExample& example : goldenExamples())
    {
        std::cout << "=== " << (source == ExampleSource::ORIGINAL ? example.file : example.name)
                  << "\n";
        for (const std::string& line : stateLines(example, source, presets))
        {
            std::cout << line << "\n";
        }
        std::cout << "#numbers\n";
        const Result<LoadedDocument> loaded = loadGoldenExample(example, source, presets);
        if (loaded)
        {
            for (const DesignNumber& number :
                 QtRocket::Test::massAndLength(loaded->document->getRocket()))
            {
                std::cout << number.name << "="
                          << QtRocket::Strings::javaDoubleToString(number.value) << "\n";
            }
        }
    }
}

// The states as the Java probe prints them, for a comparison with its output (the probes of
// tier 9c, part "examples": scripts/compare.sh).
TEST(ExampleFilesTable, DISABLED_PrintsTheStates)
{
    printStates(ExampleSource::ORIGINAL, Presets::EXAMPLES);
}

TEST(ExampleFilesTable, DISABLED_PrintsTheStatesWithoutPresets)
{
    printStates(ExampleSource::ORIGINAL, Presets::NONE);
}

TEST(ExampleFilesTable, DISABLED_PrintsTheStatesOfTheResaves)
{
    printStates(ExampleSource::RESAVE, Presets::EXAMPLES);
}

// ------------------------------------------------------------------------- a measurement

/// How many seeds the measurement below flies every simulation with: 0 to 31, as SeedProbe
/// flies OpenRocket's.
constexpr int kMeasuredSeeds = 32;

/// The warnings OpenRocket's flight of the simulation number @p index of @p row gave, as the
/// section "junit" has them ("<priority> <text>"), without the numbers in them.
[[nodiscard]] Lines javaWarnings(const ExampleOrkFile& row, std::size_t index)
{
    const std::string start = std::format("sim[{}] W ", index);
    Lines             warnings;
    for (const std::string_view line : row.junit)
    {
        if (line.starts_with(start))
        {
            warnings.push_back(withoutNumbers(line.substr(start.size())));
        }
    }
    return warnings;
}

/// The warnings of @p record, in ASCII and without the numbers in them, as javaWarnings() has
/// OpenRocket's.
[[nodiscard]] Lines maskedWarnings(const SimulationRecord& record)
{
    Lines warnings;
    for (const std::string& warning : record.warningTexts)
    {
        warnings.push_back(withoutNumbers(ascii(QtRocket::Test::onOneLine(warning))));
    }
    return warnings;
}

/// What the measurement keeps of one simulation over all seeds.
struct SeedSpread
{
    std::string name;
    /// The lowest and the highest maximum altitude, and the slowest and the fastest deployment.
    double lowest{std::numeric_limits<double>::infinity()};
    double highest{-std::numeric_limits<double>::infinity()};
    double slowest{std::numeric_limits<double>::infinity()};
    double fastest{-std::numeric_limits<double>::infinity()};
    /// The seeds with which the flight failed, had an error or had other warnings than
    /// OpenRocket's flight with seed 0, each with what it had.
    std::string deviations;
};

/// What the flight of @p record has that OpenRocket's flight with seed 0 of the simulation
/// number @p index of @p row has not: its failure, its errors, or its warnings when they are
/// others (the numbers in them apart); "" when it is as OpenRocket's.
[[nodiscard]] std::string deviationOf(const SimulationRecord& record, const ExampleOrkFile& row,
                                      std::size_t index)
{
    if (!record.failure.has_value() && !record.hasErrors &&
        maskedWarnings(record) == javaWarnings(row, index))
    {
        return "";
    }
    std::string deviation =
        std::format("{}{}{} warnings:", record.failure.value_or(""),
                    record.hasErrors ? " has errors, " : "", record.warningTexts.size());
    for (const std::string& warning : record.warningTexts)
    {
        deviation += std::format(" [{}]", warning);
    }
    return deviation;
}

/// Flies the simulations of @p example with the seed @p seed on a freshly loaded document and
/// adds to @p spreads what they give.
void measureSeed(const GoldenExample& example, const ExampleOrkFile& row, int seed,
                 std::vector<SeedSpread>& spreads)
{
    const Result<LoadedDocument> loaded = loadGoldenExample(example);
    if (!loaded)
    {
        return;
    }
    const std::vector<SimulationRecord> records = flySimulations(*loaded->document, seed);
    spreads.resize(records.size());
    for (std::size_t index = 0; index < records.size(); index++)
    {
        const SimulationRecord& record = records[index];
        SeedSpread&             spread = spreads[index];
        spread.name                    = record.name;
        spread.lowest                  = std::min(spread.lowest, record.metrics.maxAltitude);
        spread.highest                 = std::max(spread.highest, record.metrics.maxAltitude);
        spread.slowest                 = std::min(spread.slowest, record.deploymentVelocity);
        spread.fastest                 = std::max(spread.fastest, record.deploymentVelocity);
        const std::string deviation    = deviationOf(record, row, index);
        if (!deviation.empty())
        {
            spread.deviations += std::format(" {} ({})", seed, deviation);
        }
    }
}

/// Java's literal for the maximum altitude of the simulation @p simulation of the example
/// @p file (ExampleFilesTest.EXPECTED_METRICS); NaN when it has none.
[[nodiscard]] double javaAltitude(std::string_view file, std::string_view simulation)
{
    const auto metricsTable = allExpectedMetrics();
    const auto expected     = metricsTable.find(file);
    if (expected == metricsTable.end())
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const auto metrics = expected->second.simulations.find(simulation);
    return metrics == expected->second.simulations.end() ? std::numeric_limits<double>::quiet_NaN()
                                                         : metrics->second.maxAltitude;
}

/// Prints what measureSeed() found of @p example over the seeds: per simulation the lowest and
/// the highest maximum altitude, Java's literal for its own flight and how far the two are
/// from it at most, the slowest and the fastest deployment, and the seeds that deviate.
void printSeedSpread(const GoldenExample& example, const std::vector<SeedSpread>& spreads)
{
    for (const SeedSpread& spread : spreads)
    {
        const double literal = javaAltitude(example.file, spread.name);
        std::cout << std::format(
            "{} | {} | altitude {:.3f} to {:.3f} | Java {:.3f} | off by at most {:.3f} | "
            "deployment {:.3f} to {:.3f} | deviating seeds:{}\n",
            example.file, spread.name, spread.lowest, spread.highest, literal,
            std::max(std::abs(spread.lowest - literal), std::abs(spread.highest - literal)),
            spread.slowest, spread.fastest,
            spread.deviations.empty() ? " none" : spread.deviations);
    }
}

// A measurement, not a test (it takes twenty minutes in a debug build): every simulation of every
// example flown with 32 seeds, each on a freshly loaded document, as the Java probe SeedProbe
// flies OpenRocket's. It prints what depends on the realisation of the turbulence and what
// does not: the maximum altitudes, which ExampleFilesTest pins to 0.5 m and this file does not
// compare; the speeds of the deployments, which decide a warning at 20 m/s; and the seeds with
// which a flight fails, has an error or has other warnings than OpenRocket's flight (the
// numbers in the warnings apart), which the test above asserts for seed 0.
TEST(ExampleFilesTable, DISABLED_PrintsWhatOfTheFlightsDependsOnTheSeed)
{
    const DefaultUnitsGuard units;
    for (const GoldenExample& example : goldenExamples())
    {
        const ExampleOrkFile* const row = tableOf(example.file);
        ASSERT_NE(row, nullptr);
        std::vector<SeedSpread> spreads;
        for (int seed = 0; seed < kMeasuredSeeds; seed++)
        {
            measureSeed(example, *row, seed, spreads);
        }
        printSeedSpread(example, spreads);
    }
}

/// "warned at <speed> m/s" for every warning of a deployment at high speed of @p record, each
/// speed a double as Java prints it; "no warning" when it has none.
[[nodiscard]] std::string highSpeedWarningsOf(const SimulationRecord& record)
{
    std::string text;
    for (const double speed : record.highSpeedDeployments)
    {
        text += std::format("{}warned at {} m/s", text.empty() ? "" : ", ",
                            QtRocket::Strings::javaDoubleToString(speed));
    }
    return text.empty() ? "no warning" : text;
}

/// Prints one line for the flight @p record of the simulation @p limit with the seed @p seed:
/// the deployment speed of its flight data, the limit of its options, its warnings of a
/// deployment at high speed, and whether the three agree as the test asserts it
/// (deploymentContradictions()).
void printDeploymentAtTheLimit(const DeploymentAtTheLimit& limit, int seed,
                               const SimulationRecord& record)
{
    const Lines contradictions = deploymentContradictions(limit.file, record);
    std::string agreement      = contradictions.empty() ? "consistent" : "CONTRADICTION:";
    for (const std::string& contradiction : contradictions)
    {
        agreement += std::format(" [{}]", contradiction);
    }
    std::cout << std::format("{} | {} | seed {} | deployment {} m/s | limit {} m/s | {} | {}\n",
                             limit.file, limit.simulation, seed,
                             QtRocket::Strings::javaDoubleToString(record.deploymentVelocity),
                             QtRocket::Strings::javaDoubleToString(record.recoverySpeedWarning),
                             highSpeedWarningsOf(record), agreement);
}

/// Flies the simulations of the example of @p limit with every seed of the measurement, each
/// time all of them in their order on a freshly loaded document, as the test flies them, and
/// prints the flights of the simulation of @p limit (printDeploymentAtTheLimit()).
void measureDeploymentsAtTheLimit(const DeploymentAtTheLimit& limit)
{
    const GoldenExample* const example = QtRocket::Test::findGoldenExample(limit.file);
    if (example == nullptr)
    {
        std::cout << "no example " << limit.file << "\n";
        return;
    }
    for (int seed = 0; seed < kMeasuredSeeds; seed++)
    {
        const Result<LoadedDocument> loaded = loadGoldenExample(*example);
        if (!loaded)
        {
            std::cout << loaded.error().toString() << "\n";
            continue;
        }
        for (const SimulationRecord& record : flySimulations(*loaded->document, seed))
        {
            if (record.name == limit.simulation)
            {
                printDeploymentAtTheLimit(limit, seed, record);
            }
        }
    }
}

// A measurement, not a test (it takes five minutes in a debug build): the simulations of
// kDeploymentsAtTheLimit flown with the same 32 seeds, and of every flight what the test
// asserts of such a simulation in place of Java's "no warning of a deployment at high speed":
// the deployment speed of the flight data, the limit, the speed a warning names, and whether
// the flight has the warning exactly when it deploys above the limit.
TEST(ExampleFilesTable, DISABLED_PrintsTheDeploymentsAtTheLimit)
{
    const DefaultUnitsGuard units;
    for (const DeploymentAtTheLimit& limit : kDeploymentsAtTheLimit)
    {
        measureDeploymentsAtTheLimit(limit);
    }
}

}  // namespace
