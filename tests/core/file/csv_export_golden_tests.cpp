// Golden test of CsvExport: the flight data stored in OpenRocket's example designs, exported by
// QtRocket and by OpenRocket, compared byte for byte.
//
// The fixtures (tests/data/csv/*.csv.gz) are what OpenRocket's CSVExport.exportCSV() writes for
// a branch of a simulation of an example as OpenRocket loads it (never simulated again, so the
// numbers are the file's and the export alone decides the text), written by
// tools/openrocket-goldens/src/main/java/info/qtrocket/goldens/CsvExportDumper.java (see the
// README there and tests/data/csv/README.md): every field of the branch in the order of its
// getTypes(), each in the default unit of its group, with the options of the export dialog or
// one of four other sets of options. Here the example is loaded as the golden harness loads it
// (goldenExampleEnvironment(), loadGoldenExample(): as loaded, not settled; the stored data does
// not depend on either), the same branch exported with the same options, and the text compared
// with the fixture's, gunzipped.
//
// Between them the nine fixtures hold: the layouts of the comments (simulation, field and event
// comments each on and off, the line of the comment starter alone), the separators ",", ";",
// " " and a tab, the comment starters "#", "%", ";" and "//", 0, 3, 6 and 15 decimals with and
// without the exponential notation, a branch of a booster and the third branch of a three-stage
// flight, the warnings of a simulation (with sources), a SIM_WARN event, the zero-width space of
// a dimensionless unit, the column of a simulation extension, and the order of events at the
// same time whose sources are known by their ids only (the events of a loaded flight are
// detached, see CsvExport).

#include <algorithm>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/CsvExport.h"
#include "QtRocket/file/GzipStream.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"
#include "goldens/GoldenExamples.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Test
{
namespace
{

/// One fixture: the branch of a simulation of an example, exported with options.
struct CsvFixture
{
    /// The name of the file in tests/data/csv.
    std::string file;
    /// The name of the example in data/examples.
    std::string example;
    /// The index of the simulation in the document.
    std::size_t simulation{0};
    /// The index of the branch in the simulated data.
    std::size_t branch{0};
    /// The options of the export.
    CsvExport::Options options{};
};

/// The options of CsvExportDumper's VARIANTS, in its order.
[[nodiscard]] CsvExport::Options variant(std::string_view separator, int decimalPlaces,
                                         bool exponentialNotation, std::string_view comment,
                                         bool simulationComments, bool fieldComments,
                                         bool eventComments)
{
    return CsvExport::Options{.fieldSeparator      = std::string(separator),
                              .decimalPlaces       = decimalPlaces,
                              .exponentialNotation = exponentialNotation,
                              .commentStarter      = std::string(comment),
                              .simulationComments  = simulationComments,
                              .fieldComments       = fieldComments,
                              .eventComments       = eventComments};
}

/// The fixtures, as CsvExportDumper's FIXTURES and VARIANTS make them.
[[nodiscard]] std::vector<CsvFixture> fixtures()
{
    const std::string tubeFin = "Tube fin rocket.ork";
    return {
        {.file = "tube-fin-rocket_sim00_branch0.csv.gz", .example = tubeFin},
        {.file    = "tube-fin-rocket_sim00_branch0_variant1.csv.gz",
         .example = tubeFin,
         .options = variant(";", 6, false, "%", true, false, false)},
        {.file    = "tube-fin-rocket_sim00_branch0_variant2.csv.gz",
         .example = tubeFin,
         .options = variant(" ", 0, true, ";", false, true, true)},
        {.file    = "tube-fin-rocket_sim00_branch0_variant3.csv.gz",
         .example = tubeFin,
         .options = variant("\t", 15, true, "#", false, false, true)},
        {.file    = "tube-fin-rocket_sim00_branch0_variant4.csv.gz",
         .example = tubeFin,
         .options = variant(",", 3, true, "//", true, true, false)},
        {.file    = "two-stage-high-power-rocket_sim00_branch1.csv.gz",
         .example = "Two stage high power rocket.ork",
         .branch  = 1},
        {.file       = "dual-parachute-deployment_sim02_branch0.csv.gz",
         .example    = "Dual parachute deployment.ork",
         .simulation = 2},
        {.file       = "simulation-extensions_sim02_branch0.csv.gz",
         .example    = "Simulation extensions.ork",
         .simulation = 2},
        {.file    = "three-stage-low-power-rocket_sim00_branch2.csv.gz",
         .example = "Three stage low power rocket.ork",
         .branch  = 2},
    };
}

[[nodiscard]] std::filesystem::path csvDir()
{
    return testDataDir() / "csv";
}

/// The text of a fixture, gunzipped.
[[nodiscard]] Result<std::string> fixtureText(const std::string& file)
{
    const Result<std::vector<std::byte>> compressed = readFile(csvDir() / file);
    if (!compressed)
    {
        return std::unexpected(compressed.error());
    }
    const Result<std::vector<std::byte>> text = gzipInflate(*compressed);
    if (!text)
    {
        return std::unexpected(text.error());
    }
    return bytesToString(*text);
}

/// Where @p actual first differs from @p expected: the line number and both lines (at most 200
/// characters of each); empty when they are equal.
[[nodiscard]] std::string firstDifference(std::string_view expected, std::string_view actual)
{
    if (expected == actual)
    {
        return {};
    }
    std::size_t at   = 0;
    std::size_t line = 1;
    while (at < expected.size() && at < actual.size() && expected[at] == actual[at])
    {
        if (expected[at] == '\n')
        {
            line++;
        }
        at++;
    }
    const auto lineAround = [at](std::string_view text) {
        const std::size_t newline = at == 0 ? std::string_view::npos : text.rfind('\n', at - 1);
        const std::size_t start   = newline == std::string_view::npos ? 0 : newline + 1;
        const std::size_t end     = std::min(text.find('\n', start), text.size());
        return std::string(text.substr(start, std::min<std::size_t>(end - start, 200)));
    };
    return std::format("line {} (sizes {} and {}):\n  OpenRocket: {}\n  QtRocket:   {}", line,
                       expected.size(), actual.size(), lineAround(expected), lineAround(actual));
}

/// Exports the branch of @p fixture from @p loaded and compares it with the fixture.
void compareFixture(const LoadedDocument& loaded, const CsvFixture& fixture)
{
    SCOPED_TRACE(fixture.file);
    const Result<std::string> expected = fixtureText(fixture.file);
    ASSERT_TRUE(expected.has_value()) << expected.error().toString();

    const OpenRocketDocument& document = *loaded.document;
    ASSERT_LT(fixture.simulation, document.getSimulationCount());
    const std::shared_ptr<Simulation>  simulation = document.getSimulation(fixture.simulation);
    const std::shared_ptr<FlightData>& data       = simulation->getSimulatedData();
    ASSERT_NE(data, nullptr);
    ASSERT_LT(fixture.branch, data->getBranchCount());
    const FlightDataBranch&                  branch = data->getBranch(fixture.branch);
    const std::vector<const FlightDataType*> fields = branch.getTypes();
    const std::vector<const Unit*>           units  = CsvExport::defaultUnits(fields);

    std::ostringstream out;
    const Result<void> exported =
        CsvExport::exportCsv(out, *simulation, branch, fields, units, fixture.options);
    ASSERT_TRUE(exported.has_value()) << exported.error().toString();
    const std::string difference = firstDifference(*expected, out.str());
    EXPECT_TRUE(difference.empty()) << difference;
}

/// Loads each example of @p all once and compares its fixtures.
void compareFixtures(const std::vector<CsvFixture>& all)
{
    std::set<std::string> examples;
    for (const CsvFixture& fixture : all)
    {
        examples.insert(fixture.example);
    }
    for (const std::string& name : examples)
    {
        SCOPED_TRACE(name);
        const GoldenExample* example = findGoldenExample(name);
        ASSERT_NE(example, nullptr);
        const Result<LoadedDocument> loaded = loadGoldenExample(*example);
        ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
        for (const CsvFixture& fixture : all)
        {
            if (fixture.example == name)
            {
                compareFixture(*loaded, fixture);
            }
        }
    }
}

/// The names of the files in tests/data/csv that end with ".csv.gz".
[[nodiscard]] std::set<std::string> fixtureFiles()
{
    std::set<std::string> files;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(csvDir()))
    {
        const std::string name = pathToUtf8(entry.path().filename());
        if (name.ends_with(".csv.gz"))
        {
            files.insert(name);
        }
    }
    return files;
}

[[nodiscard]] std::set<std::string> listedFiles()
{
    std::set<std::string> files;
    for (const CsvFixture& fixture : fixtures())
    {
        files.insert(fixture.file);
    }
    return files;
}

// The flight data stored in the examples, exported as OpenRocket exports it: the bytes of every
// fixture. The warnings and the angles in them are formatted with the default units.
TEST(CsvExportGolden, StoredBranchesOfTheExamplesAreExportedAsOpenRocketExportsThem)
{
    const DefaultUnitsGuard units;
    compareFixtures(fixtures());
}

// Every fixture of tests/data/csv is compared, and every fixture compared is there.
TEST(CsvExportGolden, EveryFixtureIsCompared)
{
    EXPECT_EQ(fixtureFiles(), listedFiles());
    EXPECT_EQ(listedFiles().size(), 9U);
}

}  // namespace
}  // namespace QtRocket::Test
