// Tests of the helpers of cli/SimulateCommand.h. QtRocket's own; the command itself is run in
// CliTests.cpp.

#include "QtRocket/cli/SimulateCommand.h"

#include <filesystem>
#include <functional>
#include <initializer_list>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/cli/CliArguments.h"
#include "QtRocket/cli/CliOutput.h"
#include "QtRocket/cli/ExitCode.h"
#include "QtRocket/file/CsvExport.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestTempDir.h"
#include "rocket/TestRockets.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::CliArguments;
using QtRocket::CliOutput;
using QtRocket::CsvExport;
using QtRocket::ExitCode;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::FlightEvent;
using QtRocket::MultiLevelPinkNoiseWindModel;
using QtRocket::pathToUtf8;
using QtRocket::SimulateCommand;
using QtRocket::Simulation;
using QtRocket::SimulationOptions;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::TempDir;
using QtRocket::Test::TestEstesAlphaIII;

/// The options of a simulation with a turbulent wind: an average wind of 4 m/s with a standard
/// deviation of 1.5 m/s, and two levels of the multi-level wind with deviations of their own.
[[nodiscard]] std::unique_ptr<SimulationOptions> windyOptions()
{
    auto options = std::make_unique<SimulationOptions>();
    options->getAverageWindModel().setAverage(4.0);
    options->getAverageWindModel().setStandardDeviation(1.5);
    MultiLevelPinkNoiseWindModel& levels = options->getMultiLevelWindModel();
    levels.clearLevels();
    EXPECT_TRUE(levels.addWindLevel(0.0, 3.0, 0.5, 0.75));
    EXPECT_TRUE(levels.addWindLevel(500.0, 8.0, 1.0, 2.0));
    return options;
}

/// The seeds of eight runs of applyOptions() of a command line without --seed on @p options.
[[nodiscard]] std::set<int> seedsWithoutSeedOption(SimulationOptions& options)
{
    const CliArguments arguments{};
    std::set<int>      seeds;
    for (int i = 0; i < 8; ++i)
    {
        SimulateCommand::applyOptions(arguments, options);
        seeds.insert(options.getRandomSeed());
    }
    return seeds;
}

/// A simulation of @p rocket with the flight data @p data, as a file describes one.
[[nodiscard]] std::unique_ptr<Simulation> loadedSimulation(QtRocket::Rocket&           rocket,
                                                           std::shared_ptr<FlightData> data)
{
    return std::make_unique<Simulation>(
        nullptr, rocket, Simulation::Status::LOADED, "Loaded", SimulationOptions(),
        std::vector<std::shared_ptr<QtRocket::SimulationExtension>>{}, std::move(data));
}

TEST(SimulateCommand, SeveralSimulationsNumberTheirCsvFilesBeforeTheExtension)
{
    EXPECT_EQ(SimulateCommand::numberedCsvPath("out.csv", 2), std::filesystem::path("out-2.csv"));
    EXPECT_EQ(
        SimulateCommand::numberedCsvPath(std::filesystem::path("dir") / "flight.data.csv", 12),
        std::filesystem::path("dir") / "flight.data-12.csv");
    EXPECT_EQ(SimulateCommand::numberedCsvPath("out", 3), std::filesystem::path("out-3"));
}

// The wall clock of 'Computation time' would make two runs with the same seed differ.
TEST(SimulateCommand, TheCsvHasEveryColumnButTheComputationTime)
{
    const FlightDataType& time     = FlightDataType::builtin(FlightDataTypeId::TYPE_TIME);
    const FlightDataType& altitude = FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE);
    const FlightDataType& computation =
        FlightDataType::builtin(FlightDataTypeId::TYPE_COMPUTATION_TIME);
    const FlightDataBranch                   branch("Sustainer", {time, computation, altitude});
    const std::vector<const FlightDataType*> expected{&time, &altitude};
    EXPECT_EQ(SimulateCommand::csvFields(branch), expected);
}

// --calm: no turbulence on any wind model, the average winds as they were (SimulateCommand.h).
TEST(SimulateCommand, CalmSetsEveryStandardDeviationToZeroAndKeepsTheWinds)
{
    const std::unique_ptr<SimulationOptions> options = windyOptions();
    CliArguments                             arguments;
    arguments.calm = true;
    SimulateCommand::applyOptions(arguments, *options);
    EXPECT_EQ(options->getAverageWindModel().getStandardDeviation(), 0.0);
    EXPECT_EQ(options->getAverageWindModel().getAverage(), 4.0);
    const std::vector<MultiLevelPinkNoiseWindModel::LevelWindModel*> levels =
        options->getMultiLevelWindModel().getLevels();
    ASSERT_EQ(levels.size(), 2U);
    EXPECT_EQ(levels[0]->getStandardDeviation(), 0.0);
    EXPECT_EQ(levels[0]->getSpeed(), 3.0);
    EXPECT_EQ(levels[1]->getStandardDeviation(), 0.0);
    EXPECT_EQ(levels[1]->getSpeed(), 8.0);

    // Without --calm nothing of the wind changes.
    const std::unique_ptr<SimulationOptions> windy = windyOptions();
    SimulateCommand::applyOptions(CliArguments{}, *windy);
    EXPECT_EQ(windy->getAverageWindModel().getStandardDeviation(), 1.5);
    EXPECT_EQ(windy->getMultiLevelWindModel().getLevels()[1]->getStandardDeviation(), 2.0);
}

// --seed fixes the seed; without it a seed that is not fixed is drawn anew, as OpenRocket's run
// dialog draws it (SimulationRunDialog.java:122), and one the design fixes is kept. --time-step
// sets the time step, and only then.
TEST(SimulateCommand, TheSeedIsTheOneGivenElseANewOneUnlessTheDesignFixesIt)
{
    SimulationOptions given;
    given.setRandomSeed(5);
    CliArguments arguments;
    arguments.seed     = -7;
    arguments.timeStep = 0.01;
    SimulateCommand::applyOptions(arguments, given);
    EXPECT_EQ(given.getRandomSeed(), -7);
    EXPECT_TRUE(given.isRandomSeedFixed());
    EXPECT_EQ(given.getTimeStep(), 0.01);

    SimulationOptions fixed;
    fixed.setRandomSeed(1234);
    fixed.setRandomSeedFixed(true);
    const double timeStep = fixed.getTimeStep();
    EXPECT_EQ(seedsWithoutSeedOption(fixed), std::set<int>{1234});
    EXPECT_TRUE(fixed.isRandomSeedFixed());
    EXPECT_EQ(fixed.getTimeStep(), timeStep);

    // Eight draws of 32 random bits are not all the same (but for one chance in 2^224).
    SimulationOptions drawn;
    EXPECT_GT(seedsWithoutSeedOption(drawn).size(), 1U);
    EXPECT_FALSE(drawn.isRandomSeedFixed());
}

// A flight without a branch, or whose first branch has no column to write, writes no file and
// counts as a failed simulation (OpenRocket's export panel refuses such data).
TEST(SimulateCommand, AFlightWithNothingToWriteWritesNoCsvAndFails)
{
    const TestEstesAlphaIII           alpha;
    const TempDir                     directory;
    const std::filesystem::path       csv = directory.resolve("flight.csv");
    const std::unique_ptr<Simulation> withoutBranch =
        loadedSimulation(*alpha.rocket, std::make_shared<FlightData>());
    CliOutput output;
    EXPECT_EQ(
        SimulateCommand::writeCsv(csv, *withoutBranch, *withoutBranch->getSimulatedData(), output),
        ExitCode::SIMULATION);
    EXPECT_EQ(output.err(), "QtRocket_cli: simulation 'Loaded' has no flight data to write to '" +
                                pathToUtf8(csv) + "'\n");
    EXPECT_TRUE(output.out().empty());
    EXPECT_FALSE(std::filesystem::exists(csv));

    const auto clock = std::make_shared<FlightDataBranch>(
        "Clock", std::initializer_list<std::reference_wrapper<const FlightDataType>>{
                     FlightDataType::builtin(FlightDataTypeId::TYPE_COMPUTATION_TIME)});
    const std::unique_ptr<Simulation> clockOnly = loadedSimulation(
        *alpha.rocket, std::make_shared<FlightData>(
                           std::initializer_list<std::shared_ptr<FlightDataBranch>>{clock}));
    CliOutput clockOutput;
    EXPECT_EQ(
        SimulateCommand::writeCsv(csv, *clockOnly, *clockOnly->getSimulatedData(), clockOutput),
        ExitCode::SIMULATION);
    EXPECT_FALSE(std::filesystem::exists(csv));
}

// A branch without a TIME column is written as CsvExport writes it, every event first (the
// layout CsvExportTests pins on OpenRocket's), not refused.
TEST(SimulateCommand, ABranchWithoutATimeColumnIsWritten)
{
    const DefaultUnitsGuard     units;
    const TestEstesAlphaIII     alpha;
    const TempDir               directory;
    const std::filesystem::path csv      = directory.resolve("flight.csv");
    const FlightDataType&       altitude = FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE);
    const auto                  branch   = std::make_shared<FlightDataBranch>(
        "Timeless", std::initializer_list<std::reference_wrapper<const FlightDataType>>{altitude});
    branch->addPoint();
    branch->setValue(altitude, 10.0);
    branch->addPoint();
    branch->setValue(altitude, 20.0);
    branch->addEvent(FlightEvent(FlightEvent::Type::APOGEE, 3.0));
    const std::unique_ptr<Simulation> simulation = loadedSimulation(
        *alpha.rocket, std::make_shared<FlightData>(
                           std::initializer_list<std::shared_ptr<FlightDataBranch>>{branch}));
    CliOutput output;
    EXPECT_EQ(SimulateCommand::writeCsv(csv, *simulation, *simulation->getSimulatedData(), output),
              ExitCode::OK);
    EXPECT_EQ(output.out(), "  CSV: " + pathToUtf8(csv) + "\n");
    EXPECT_TRUE(output.err().empty()) << output.err();

    const std::vector<const FlightDataType*> fields{&altitude};
    const std::vector<const QtRocket::Unit*> fieldUnits = CsvExport::defaultUnits(fields);
    std::ostringstream                       expected;
    ASSERT_TRUE(CsvExport::exportCsv(expected, *simulation, *branch, fields, fieldUnits, {}));
    const QtRocket::Result<std::string> written = QtRocket::readTextFile(csv);
    EXPECT_EQ(written, expected.str());
    EXPECT_NE(
        expected.str().find("# Altitude (m)\n# Event APOGEE occurred at t=3 seconds\n10\n20\n"),
        std::string::npos)
        << expected.str();
}

}  // namespace
