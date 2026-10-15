// Tests of the helpers of cli/SimulateCommand.h. QtRocket's own; the command itself is run in
// CliTests.cpp.

#include "QtRocket/cli/SimulateCommand.h"

#include <filesystem>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"

namespace
{

using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::SimulateCommand;

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

}  // namespace
