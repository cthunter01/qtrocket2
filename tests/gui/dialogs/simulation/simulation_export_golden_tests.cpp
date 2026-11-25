// Golden test of the simulation dialog's "Export data" tab (SimulationExportPanel, plan 11.6 E9):
// the file the tab writes, driven as a user drives it (the rows ticked, a stage chosen, the
// options and units set, the file question answered), against
// - OpenRocket's own exports of the flight data stored in its examples (tests/data/csv, the
//   fixtures of the core's csv_export_golden_tests.cpp, written by CsvExportDumper: every
//   variable of a branch in its default unit, or the last unit of its group, with the dialog's
//   options or six other sets), byte for byte; the rows are the first branch's variables, which
//   are the variables of every stored branch in the same order, so the tab writes what
//   OpenRocket's dialog writes for each of the eleven fixtures;
// - the core's CsvExport of the same choices for the first simulation of each of the sixteen
//   examples, with three sets of options, ticks, units and stages.
// The examples are loaded as the core's golden test loads them (GoldenExamples.h: as loaded),
// so that the simulations' statuses are those of the fixtures.

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QComboBox>
#include <QString>
#include <QWidget>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/CsvExport.h"
#include "QtRocket/file/GzipStream.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/dialogs/simulation/SimulationExportPanel.h"
#include "QtRocket/gui/util/CsvOptionValues.h"
#include "QtRocket/gui/util/FileNames.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/CsvExportPanel.h"
#include "QtRocket/gui/widgets/CsvOptionPanel.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"
#include "TestPaths.h"
#include "TestTempDir.h"
#include "dialogs/PreferencesPageTranscript.h"
#include "goldens/GoldenExamples.h"

namespace QtRocket::Gui
{

namespace
{

using SimulationExportGolden = Gui::Test::GuiTest;
using QtRocket::Test::GoldenExample;

/// Which variables the tab ticks.
enum class Ticks
{
    ALL,         ///< Select all
    EVEN_ROWS,   ///< the first row, the third, ...
    FIRST_FIVE,  ///< the first five rows
};

/// The unit of its group each variable is written in.
enum class Units
{
    DEFAULT,  ///< the group's default unit, the one the tab starts with
    SECOND,   ///< the group's second unit (the first when it has one only)
    LAST,     ///< the group's last unit
};

/// One export of a stored flight: what the user chooses in the tab, and the same for the core.
struct ExportChoice
{
    /// The index of the stage chosen (the branch written); past the last stage: the last.
    std::size_t        stage{0};
    Ticks              ticks{Ticks::ALL};
    Units              units{Units::DEFAULT};
    CsvOptionValues    options;
    CsvExport::Options coreOptions;
};

/// One fixture of tests/data/csv: the branch of a simulation of an example, as the core's golden
/// test lists it (CsvExportDumper's FIXTURES and VARIANTS).
struct CsvFixture
{
    std::string  file;
    std::string  example;
    std::size_t  simulation{0};
    ExportChoice choice;
};

/// The tab's values of the options @p separator, ... (the comment options in the order of the
/// tab's check boxes: simulation, field and event comments).
[[nodiscard]] CsvOptionValues tabOptions(const QString& separator, int decimalPlaces,
                                         bool exponentialNotation, const QString& comment,
                                         bool simulationComments, bool fieldComments,
                                         bool eventComments)
{
    return {.fieldSeparator      = separator,
            .decimalPlaces       = decimalPlaces,
            .exponentialNotation = exponentialNotation,
            .commentCharacter    = comment,
            .commentOptions      = {simulationComments, fieldComments, eventComments}};
}

/// The core's options of the same.
[[nodiscard]] CsvExport::Options coreOptions(std::string_view separator, int decimalPlaces,
                                             bool exponentialNotation, std::string_view comment,
                                             bool simulationComments, bool fieldComments,
                                             bool eventComments)
{
    return {.fieldSeparator      = std::string(separator),
            .decimalPlaces       = decimalPlaces,
            .exponentialNotation = exponentialNotation,
            .commentStarter      = std::string(comment),
            .simulationComments  = simulationComments,
            .fieldComments       = fieldComments,
            .eventComments       = eventComments};
}

/// Every variable of the branch @p stage in @p units with the dialog's options (the fixtures'
/// default) or @p options.
[[nodiscard]] ExportChoice allVariables(std::size_t stage = 0, Units units = Units::DEFAULT,
                                        CsvOptionValues options = tabOptions(QStringLiteral(","), 3,
                                                                             true,
                                                                             QStringLiteral("#"),
                                                                             true, true, true))
{
    return {.stage       = stage,
            .ticks       = Ticks::ALL,
            .units       = units,
            .options     = std::move(options),
            .coreOptions = {}};
}

/// The eleven fixtures, as the core's golden test lists them.
[[nodiscard]] std::vector<CsvFixture> fixtures()
{
    const std::string tubeFin = "Tube fin rocket.ork";
    return {
        {.file    = "tube-fin-rocket_sim00_branch0.csv.gz",
         .example = tubeFin,
         .choice  = allVariables()},
        {.file    = "tube-fin-rocket_sim00_branch0_variant1.csv.gz",
         .example = tubeFin,
         .choice  = allVariables(
             0, Units::DEFAULT,
             tabOptions(QStringLiteral(";"), 6, false, QStringLiteral("%"), true, false, false))},
        {.file    = "tube-fin-rocket_sim00_branch0_variant2.csv.gz",
         .example = tubeFin,
         .choice  = allVariables(
             0, Units::DEFAULT,
             tabOptions(QStringLiteral(" "), 0, true, QStringLiteral(";"), false, true, true))},
        {.file    = "tube-fin-rocket_sim00_branch0_variant3.csv.gz",
         .example = tubeFin,
         .choice  = allVariables(
             0, Units::DEFAULT,
             tabOptions(QStringLiteral("\t"), 15, true, QStringLiteral("#"), false, false, true))},
        {.file    = "tube-fin-rocket_sim00_branch0_variant4.csv.gz",
         .example = tubeFin,
         .choice  = allVariables(
             0, Units::DEFAULT,
             tabOptions(QStringLiteral(","), 3, true, QStringLiteral("//"), true, true, false))},
        {.file    = "tube-fin-rocket_sim00_branch0_variant5.csv.gz",
         .example = tubeFin,
         .choice  = allVariables(
             0, Units::LAST,
             tabOptions(QStringLiteral("|"), 2, false, QStringLiteral("!!"), true, true, true))},
        {.file    = "tube-fin-rocket_sim00_branch0_variant6.csv.gz",
         .example = tubeFin,
         .choice  = allVariables(
             0, Units::DEFAULT,
             tabOptions(QStringLiteral("<->"), 0, false, QString(), true, true, true))},
        {.file    = "two-stage-high-power-rocket_sim00_branch1.csv.gz",
         .example = "Two stage high power rocket.ork",
         .choice  = allVariables(1)},
        {.file       = "dual-parachute-deployment_sim02_branch0.csv.gz",
         .example    = "Dual parachute deployment.ork",
         .simulation = 2,
         .choice     = allVariables()},
        {.file       = "simulation-extensions_sim02_branch0.csv.gz",
         .example    = "Simulation extensions.ork",
         .simulation = 2,
         .choice     = allVariables()},
        {.file    = "three-stage-low-power-rocket_sim00_branch2.csv.gz",
         .example = "Three stage low power rocket.ork",
         .choice  = allVariables(2)},
    };
}

/// The three choices each example's first simulation is exported with: the dialog's options with
/// every variable; every other variable in the last units of their groups, other options, the
/// last stage; the first five in their groups' second units, a tab between them, the first stage.
[[nodiscard]] std::vector<ExportChoice> exampleChoices()
{
    return {
        {.stage   = 0,
         .ticks   = Ticks::ALL,
         .units   = Units::DEFAULT,
         .options = tabOptions(QStringLiteral(","), 3, true, QStringLiteral("#"), true, true, true),
         .coreOptions = coreOptions(",", 3, true, "#", true, true, true)},
        {.stage = 99,
         .ticks = Ticks::EVEN_ROWS,
         .units = Units::LAST,
         .options =
             tabOptions(QStringLiteral(";"), 6, false, QStringLiteral("%"), true, false, true),
         .coreOptions = coreOptions(";", 6, false, "%", true, false, true)},
        {.stage = 0,
         .ticks = Ticks::FIRST_FIVE,
         .units = Units::SECOND,
         .options =
             tabOptions(QStringLiteral("\t"), 0, true, QStringLiteral("//"), false, true, false),
         .coreOptions = coreOptions("\t", 0, true, "//", false, true, false)},
    };
}

/// Whether the row @p row is ticked by @p ticks.
[[nodiscard]] bool isTicked(Ticks ticks, std::size_t row)
{
    switch (ticks)
    {
        case Ticks::ALL:
            return true;
        case Ticks::EVEN_ROWS:
            return row % 2 == 0;
        case Ticks::FIRST_FIVE:
            return row < 5;
    }
    return false;
}

/// The unit of @p group that @p units names.
[[nodiscard]] const Unit& unitOf(const UnitGroup& group, Units units)
{
    switch (units)
    {
        case Units::DEFAULT:
            return group.getDefaultUnit();
        case Units::SECOND:
            return group.getUnit(group.getUnitCount() > 1 ? 1 : 0);
        case Units::LAST:
            return group.getUnit(group.getUnitCount() - 1);
    }
    return group.getDefaultUnit();
}

/// The index of the stage @p choice writes of a flight of @p branchCount branches.
[[nodiscard]] std::size_t stageOf(const ExportChoice& choice, std::size_t branchCount)
{
    return choice.stage < branchCount ? choice.stage : branchCount - 1;
}

/// The text of the file @p file; empty, with a test failure, when it cannot be read.
[[nodiscard]] std::string writtenText(const std::filesystem::path& file)
{
    const Result<std::vector<std::byte>> bytes = readFile(file);
    if (!bytes.has_value())
    {
        ADD_FAILURE() << bytes.error().toString();
        return {};
    }
    return bytesToString(*bytes);
}

/// The text of the fixture @p file of tests/data/csv, gunzipped; empty, with a test failure,
/// when it cannot be read.
[[nodiscard]] std::string fixtureText(const std::string& file)
{
    const Result<std::vector<std::byte>> compressed =
        readFile(QtRocket::Test::testDataDir() / "csv" / file);
    if (!compressed.has_value())
    {
        ADD_FAILURE() << compressed.error().toString();
        return {};
    }
    const Result<std::vector<std::byte>> text = gzipInflate(*compressed);
    if (!text.has_value())
    {
        ADD_FAILURE() << text.error().toString();
        return {};
    }
    return bytesToString(*text);
}

/// Where @p actual first differs from @p expected: the line and both lines' starts; empty when
/// they are equal.
[[nodiscard]] std::string firstDifference(const std::string& expected, const std::string& actual)
{
    if (expected == actual)
    {
        return {};
    }
    std::istringstream expectedLines(expected);
    std::istringstream actualLines(actual);
    std::string        expectedLine;
    std::string        actualLine;
    for (std::size_t line = 1;; ++line)
    {
        const bool moreExpected = static_cast<bool>(std::getline(expectedLines, expectedLine));
        const bool moreActual   = static_cast<bool>(std::getline(actualLines, actualLine));
        if (!moreExpected || !moreActual || expectedLine != actualLine)
        {
            return "line " + std::to_string(line) + " (sizes " + std::to_string(expected.size()) +
                   " and " + std::to_string(actual.size()) +
                   "):\n  expected: " + expectedLine.substr(0, 200) +
                   "\n  written:  " + actualLine.substr(0, 200);
        }
    }
}

/// What the tab writes of @p simulation when the user makes @p choice and answers the file
/// question; empty, with a test failure, when it writes nothing.
[[nodiscard]] std::string tabExport(const std::shared_ptr<Simulation>& simulation,
                                    const ExportChoice&                choice)
{
    Gui::Test::TestGuiServices    services;
    QWidget                       holder;
    const QtRocket::Test::TempDir directory;
    auto* const panel = makeChild<SimulationExportPanel>(&holder, services.services(), simulation);
    CsvExportPanel& rows = panel->exportPanel();
    for (std::size_t row = 0; row < rows.rowCount(); ++row)
    {
        rows.setSelected(row, isTicked(choice.ticks, row));
        rows.setSelectedUnit(row, unitOf(panel->types().at(row)->getUnitGroup(), choice.units));
    }
    if (panel->stageSelection() != nullptr)
    {
        Gui::Test::chooseItem(
            *panel->stageSelection(),
            static_cast<int>(
                stageOf(choice, static_cast<std::size_t>(panel->stageSelection()->count()))));
    }
    panel->csvOptions().setValues(choice.options);
    services.prompts().answerSaveFile(
        UserPrompts::SaveFileAnswer{.file               = directory.resolve("export"),
                                    .directory          = directory.path(),
                                    .saveSimulationData = std::nullopt,
                                    .selectedNameFilter = FileNames::csvFilter(),
                                    .csvOptions         = std::nullopt});
    if (!panel->doExport())
    {
        ADD_FAILURE() << "the tab wrote nothing";
        return {};
    }
    return writtenText(directory.resolve("export.csv"));
}

/// What the core's CsvExport writes of @p simulation for @p choice, the variables those of its
/// first branch.
[[nodiscard]] std::string coreExport(Simulation& simulation, const ExportChoice& choice)
{
    const FlightData&                        data  = *simulation.getSimulatedData();
    const std::vector<const FlightDataType*> types = data.getBranch(0).getTypes();
    std::vector<const FlightDataType*>       fields;
    std::vector<const Unit*>                 units;
    for (std::size_t row = 0; row < types.size(); ++row)
    {
        if (isTicked(choice.ticks, row))
        {
            fields.push_back(types.at(row));
            units.push_back(&unitOf(types.at(row)->getUnitGroup(), choice.units));
        }
    }
    std::ostringstream out;
    const Result<void> written = CsvExport::exportCsv(
        out, simulation, data.getBranch(stageOf(choice, data.getBranchCount())), fields, units,
        choice.coreOptions);
    if (!written.has_value())
    {
        ADD_FAILURE() << written.error().toString();
        return {};
    }
    return out.str();
}

/// The document of the example @p name, as loaded; null, with a test failure, when it cannot be
/// loaded.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> loadExample(std::string_view name)
{
    const GoldenExample* const example = QtRocket::Test::findGoldenExample(name);
    if (example == nullptr)
    {
        ADD_FAILURE() << "no example " << name;
        return nullptr;
    }
    Result<LoadedDocument> loaded = QtRocket::Test::loadGoldenExample(*example);
    if (!loaded.has_value())
    {
        ADD_FAILURE() << loaded.error().toString();
        return nullptr;
    }
    return std::move(loaded->document);
}

/// Compares what the tab writes for @p fixture with the fixture.
void compareFixture(const CsvFixture& fixture)
{
    SCOPED_TRACE(fixture.file);
    const std::unique_ptr<OpenRocketDocument> document = loadExample(fixture.example);
    if (document == nullptr)
    {
        return;
    }
    const std::string difference =
        firstDifference(fixtureText(fixture.file),
                        tabExport(document->getSimulation(fixture.simulation), fixture.choice));
    EXPECT_TRUE(difference.empty()) << difference;
}

/// Compares what the tab writes of the first simulation of @p example with the core's export,
/// for each of exampleChoices().
void compareExample(const GoldenExample& example)
{
    SCOPED_TRACE(example.file);
    const std::unique_ptr<OpenRocketDocument> document = loadExample(example.file);
    if (document == nullptr)
    {
        return;
    }
    const std::shared_ptr<Simulation> simulation = document->getSimulation(0);
    ASSERT_TRUE(simulation->hasSimulationData());
    std::size_t index = 0;
    for (const ExportChoice& choice : exampleChoices())
    {
        SCOPED_TRACE("choice " + std::to_string(index++));
        const std::string difference =
            firstDifference(coreExport(*simulation, choice), tabExport(simulation, choice));
        EXPECT_TRUE(difference.empty()) << difference;
    }
}

// OpenRocket's exports of the flight data stored in its examples: the tab writes each fixture's
// bytes.
TEST_F(SimulationExportGolden, TheTabWritesEveryFixtureAsOpenRocketWroteIt)
{
    for (const CsvFixture& fixture : fixtures())
    {
        compareFixture(fixture);
    }
}

// The first simulation of each example, exported through the tab with three choices, as the core
// exports it.
TEST_F(SimulationExportGolden, TheTabWritesTheFirstFlightOfEachExampleAsTheCoreExportsIt)
{
    ASSERT_EQ(QtRocket::Test::goldenExamples().size(), 16U);
    for (const GoldenExample& example : QtRocket::Test::goldenExamples())
    {
        compareExample(example);
    }
}

}  // namespace

}  // namespace QtRocket::Gui
