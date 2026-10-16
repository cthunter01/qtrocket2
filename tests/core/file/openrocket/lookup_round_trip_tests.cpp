// The save halves of OpenRocket's SimulationLookupCopyTest (core/src/test/.../simulation) and the
// core half of Swing's AerodynamicLookupTruncationTest (swing/src/test/.../gui/simulation): the
// lookup tables of a simulation saved by OpenRocketSaver and loaded back from the text, as the
// Java tests' roundTrip() saves them into a stream and reads them with OpenRocketLoader from
// memory, which is a design without a directory: the CSV file the options name (which does not
// exist) is never read, and the rows the file embeds are the table. The halves without a file
// are SimulationOptionsTests.cpp's (SimulationLookupCopy), the loading of such an element by
// itself CsvLookupHandlerTests.cpp's (SimulationLookupCopyLoad). What the Java tests do not
// state, the saved elements and the warnings of the reload, is what OpenRocket gives for the
// same tests (probe SaverPortsProbe of run 10b): the preference store of OpenRocket's tests has
// the time step 0, which a reload refuses with a warning.

#include <array>
#include <filesystem>
#include <format>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/lookup/CsvMachAoALookup.h"
#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/openrocket/OpenRocketSaver.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "file/RocketLoaderTestSupport.h"
#include "simulation/SimulationRunSupport.h"

namespace
{

using QtRocket::MachAoALookup;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketDocumentFactory;
using QtRocket::Preferences;
using QtRocket::Result;
using QtRocket::Simulation;
using QtRocket::SimulationOptions;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::LoaderEnvironment;

using Lines = std::vector<std::string>;

/// SimulationLookupCopyTest.CSV, also AerodynamicLookupTruncationTest's file: a CSV file that
/// does not exist.
constexpr std::string_view kMissingCsv = "missing-beta-test-lookup.csv";

/// The warning of a reload of a simulation whose time step is 0 (OpenRocket's test preferences).
constexpr std::string_view kIllegalTimeStep = "Illegal time step defined, ignoring.";

/// The value columns of a drag table.
[[nodiscard]] std::span<const std::string> dragColumns()
{
    static const std::array<std::string, 1> kColumns{"cd"};
    return kColumns;
}

/// The value columns of a stability table.
[[nodiscard]] std::span<const std::string> stabilityColumns()
{
    static const std::array<std::string, 3> kColumns{"cn", "cm", "cp"};
    return kColumns;
}

/// The table of the CSV @p rows with the value columns @p columns (Java:
/// CsvMachAoALookup.parse(rows, columns, ',')), which must parse.
[[nodiscard]] std::shared_ptr<const MachAoALookup> lookupTableOf(
    const Lines& rows, std::span<const std::string> columns)
{
    Result<MachAoALookup> table = QtRocket::CsvMachAoALookup::parse(rows, columns, ',');
    EXPECT_TRUE(table.has_value()) << table.error().toString();
    if (!table.has_value())
    {
        return nullptr;
    }
    return std::make_shared<const MachAoALookup>(std::move(*table));
}

/// SimulationLookupCopyTest.options(value): new options of @p preferences (Java: new
/// SimulationOptions(), of the application's preferences) with a drag and a stability table
/// whose Cd and Cp are @p value everywhere, parsed from rows that are kept, of kMissingCsv.
/// @p value is the text Java's string concatenation gives the double ("2.0").
[[nodiscard]] SimulationOptions lookupCopyOptions(Preferences& preferences, std::string_view value)
{
    SimulationOptions options(preferences);
    const Lines       drag{"Mach,Cd", std::format("0,{}", value), std::format("1,{}", value)};
    const Lines       stability{"Mach,Cn,Cm,Cp", std::format("0,1,1,{}", value),
                                std::format("1,1,1,{}", value)};
    options.setDragLookup(QtRocket::pathFromUtf8(kMissingCsv), lookupTableOf(drag, dragColumns()),
                          drag);
    options.setStabilityLookup(QtRocket::pathFromUtf8(kMissingCsv),
                               lookupTableOf(stability, stabilityColumns()), stability);
    return options;
}

/// SimulationLookupCopyTest.assertLookups()
void expectCopiedLookups(const SimulationOptions& options, double value)
{
    ASSERT_TRUE(options.hasDragLookup());
    ASSERT_TRUE(options.hasStabilityLookup());
    EXPECT_EQ(options.getDragLookupTable()->interpolate(0.5, 0, "cd"), value);
    EXPECT_EQ(options.getStabilityLookupTable()->interpolate(0.5, 0, "cp"), value);
}

/// The file of kMissingCsv as the options keep it, made absolute against the current directory.
[[nodiscard]] std::filesystem::path missingCsvPath()
{
    return QtRocket::absolutePath(QtRocket::pathFromUtf8(kMissingCsv));
}

/// A saved document loaded back, with the environment of the load and the texts of the save.
struct LookupRoundTrip
{
    /// Declared first, destroyed last: the reloaded simulations keep its preference store.
    std::unique_ptr<LoaderEnvironment>  environment = std::make_unique<LoaderEnvironment>();
    std::unique_ptr<OpenRocketDocument> document;
    /// The text OpenRocketSaver wrote.
    std::string saved;
    /// The warnings of the reload, as text.
    Lines warnings;

    /// The options of the first simulation of the reloaded document; options of the built-in
    /// defaults, and a test failure, when there is none.
    [[nodiscard]] const SimulationOptions& options() const
    {
        static const SimulationOptions kNone;
        if (document == nullptr || document->getSimulationCount() == 0)
        {
            ADD_FAILURE() << "the reloaded document has no simulation";
            return kNone;
        }
        return document->getSimulation(0)->getOptions();
    }
};

/// The roundTrip() of the Java tests after the simulation is in @p document: the document saved
/// by OpenRocketSaver with new storage options, and the text loaded back without a directory, in
/// an environment of its own (Java: a new DocumentLoadingContext whose motor finder finds none).
[[nodiscard]] LookupRoundTrip saveAndLoadLookups(OpenRocketDocument& document)
{
    LookupRoundTrip                     trip;
    const QtRocket::InMemoryPreferences preferences;
    QtRocket::WarningSet                saveWarnings;
    QtRocket::ErrorSet                  errors;
    const Result<std::string>           text = QtRocket::OpenRocketSaver::save(
        document, QtRocket::StorageOptions{}, preferences, saveWarnings, errors);
    if (!text.has_value())
    {
        ADD_FAILURE() << text.error().toString();
        return trip;
    }
    EXPECT_TRUE(saveWarnings.empty());
    EXPECT_TRUE(errors.empty());
    trip.saved = *text;
    Result<QtRocket::LoadedDocument> loaded =
        QtRocket::GeneralRocketLoader(trip.environment->context())
            .load(QtRocket::stringToBytes(trip.saved));
    if (!loaded.has_value())
    {
        ADD_FAILURE() << loaded.error().toString();
        return trip;
    }
    for (const QtRocket::Warning& warning : loaded->warnings)
    {
        trip.warnings.push_back(warning.toString());
    }
    trip.document = std::move(loaded->document);
    return trip;
}

/// SimulationLookupCopyTest.roundTrip(@p options): a new simulation of @p preferences (Java: new
/// Simulation(document, rocket), of the application's preferences) of a new empty design whose
/// conditions are copied from @p options, saved and loaded back.
[[nodiscard]] LookupRoundTrip lookupCopyRoundTrip(Preferences&             preferences,
                                                  const SimulationOptions& options)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createEmptyRocket();
    const auto simulation =
        std::make_shared<Simulation>(document.get(), document->getRocket(), preferences);
    simulation->getOptions().copyConditionsFrom(options);
    document->addSimulation(simulation);
    return saveAndLoadLookups(*document);
}

/// The lines of the lookup elements (<draglookup> and <stabilitylookup>, with their rows) of the
/// .ork text @p text, as written.
[[nodiscard]] Lines lookupLinesOf(std::string_view text)
{
    Lines lines;
    bool  inside = false;
    for (std::string& line : QtRocket::Strings::split(text, '\n'))
    {
        inside = inside || line.contains("<draglookup") || line.contains("<stabilitylookup");
        const bool closes = line.contains("</draglookup>") || line.contains("</stabilitylookup>");
        if (inside)
        {
            lines.push_back(std::move(line));
        }
        inside = inside && !closes;
    }
    return lines;
}

/// The lookup elements OpenRocket writes for SimulationLookupCopyTest.options(@p value) (the
/// probe; its file name made absolute against its working directory as here).
[[nodiscard]] Lines javaLookupLines(std::string_view value)
{
    const std::string file = QtRocket::Strings::escapeXml(QtRocket::pathToUtf8(missingCsvPath()));
    return {
        std::format(R"(        <draglookup file="{}">)", file),
        "          <row>Mach,Cd</row>",
        std::format("          <row>0,{}</row>", value),
        std::format("          <row>1,{}</row>", value),
        "        </draglookup>",
        std::format(R"(        <stabilitylookup file="{}">)", file),
        "          <row>Mach,Cn,Cm,Cp</row>",
        std::format("          <row>0,1,1,{}</row>", value),
        std::format("          <row>1,1,1,{}</row>", value),
        "        </stabilitylookup>",
    };
}

// SimulationLookupCopyTest.testCopiedLookupsSurviveSaveAndReloadWithoutExternalFiles: lookups
// copied from other options are saved with their rows and read back from them, with the path
// kept, although the file does not exist.
TEST(SimulationLookupCopyRoundTrip, CopiedLookupsSurviveSaveAndReloadWithoutExternalFiles)
{
    JavaTestPreferences     preferences;
    const SimulationOptions source = lookupCopyOptions(preferences.store, "2.0");
    SimulationOptions       target(preferences.store);

    target.copyConditionsFrom(source);

    const LookupRoundTrip trip = lookupCopyRoundTrip(preferences.store, target);
    expectCopiedLookups(trip.options(), 2);
    EXPECT_EQ(source.getDragLookupCsvRows(), target.getDragLookupCsvRows());
    EXPECT_EQ(source.getStabilityLookupCsvRows(), target.getStabilityLookupCsvRows());

    // What OpenRocket writes and reads back (SaverPortsProbe).
    EXPECT_EQ(lookupLinesOf(trip.saved), javaLookupLines("2.0"));
    EXPECT_EQ(trip.warnings, Lines{std::string(kIllegalTimeStep)});
    EXPECT_EQ(trip.options().getDragLookupCsvRows(), source.getDragLookupCsvRows());
    EXPECT_EQ(trip.options().getStabilityLookupCsvRows(), source.getStabilityLookupCsvRows());
    EXPECT_EQ(trip.options().getDragLookupCsvPath(), missingCsvPath());
    EXPECT_EQ(trip.options().getStabilityLookupCsvPath(), missingCsvPath());
    std::error_code noFile;
    EXPECT_FALSE(std::filesystem::exists(missingCsvPath(), noFile));
}

// SimulationLookupCopyTest.testCopiedLookupsReplaceStaleDestinationRows: options whose lookups
// were replaced by a copy save the rows of the copy, not their old ones.
TEST(SimulationLookupCopyRoundTrip, CopiedLookupsReplaceStaleDestinationRows)
{
    JavaTestPreferences preferences;
    SimulationOptions   target = lookupCopyOptions(preferences.store, "1.0");

    target.copyConditionsFrom(lookupCopyOptions(preferences.store, "2.0"));

    const LookupRoundTrip trip = lookupCopyRoundTrip(preferences.store, target);
    expectCopiedLookups(trip.options(), 2);

    // What OpenRocket writes and reads back (SaverPortsProbe).
    EXPECT_EQ(lookupLinesOf(trip.saved), javaLookupLines("2.0"));
    EXPECT_EQ(trip.warnings, Lines{std::string(kIllegalTimeStep)});
    EXPECT_EQ(trip.options().getDragLookupCsvRows(), (Lines{"Mach,Cd", "0,2.0", "1,2.0"}));
    EXPECT_EQ(trip.options().getStabilityLookupCsvRows(),
              (Lines{"Mach,Cn,Cm,Cp", "0,1,1,2.0", "1,1,1,2.0"}));
}

/// The rows of AerodynamicLookupTruncationTest's table after the test's edit: the header, 40
/// rows of the Mach numbers 0 to 39 whose Cd (@p drag) or Cp (otherwise) is the Mach number plus
/// one, and the comment "# edited" that the edit in the dialog appends (42 rows).
[[nodiscard]] Lines longTableRows(bool drag)
{
    Lines rows{drag ? "Mach,Cd" : "Mach,Cn,Cm,Cp"};
    for (int i = 0; i < 40; i++)
    {
        rows.push_back(drag ? std::format("{},{}", i, i + 1) : std::format("{},1,1,{}", i, i + 1));
    }
    rows.emplace_back("# edited");
    return rows;
}

/// What the reload of AerodynamicLookupTruncationTest's options gives.
struct LongTableReload
{
    /// Whether the reloaded options have the table.
    bool hasTable{false};
    /// getMaxMach() of the table.
    double maxMach{std::numeric_limits<double>::quiet_NaN()};
    /// The table's Cd (drag) or Cp at Mach 39 and no angle of attack.
    double at39{std::numeric_limits<double>::quiet_NaN()};
    /// The CSV rows of the reloaded options.
    std::optional<Lines> rows;
    /// The rows of the saved element, as written, and the warnings of the reload.
    Lines savedRows;
    Lines warnings;
};

/// AerodynamicLookupTruncationTest.testEditingLongTablePreservesFinalRows(@p drag) after the
/// dialog: options of @p preferences (Java: new SimulationOptions()) with the drag (@p drag) or
/// stability table of longTableRows() of kMissingCsv, in a simulation "Lookup test" of a new
/// empty design that has not been simulated, saved and loaded back (the test's roundTrip()).
[[nodiscard]] LongTableReload reloadLongTable(Preferences& preferences, bool drag)
{
    SimulationOptions options(preferences);
    const Lines       rows = longTableRows(drag);
    if (drag)
    {
        options.setDragLookup(QtRocket::pathFromUtf8(kMissingCsv),
                              lookupTableOf(rows, dragColumns()), rows);
    }
    else
    {
        options.setStabilityLookup(QtRocket::pathFromUtf8(kMissingCsv),
                                   lookupTableOf(rows, stabilityColumns()), rows);
    }
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createEmptyRocket();
    document->addSimulation(std::make_shared<Simulation>(
        document.get(), document->getRocket(), Simulation::Status::NOT_SIMULATED, "Lookup test",
        std::move(options), std::vector<std::shared_ptr<QtRocket::SimulationExtension>>{}, nullptr,
        std::map<std::string, QtRocket::PlotAppearance>{}, &preferences));
    const LookupRoundTrip                      trip   = saveAndLoadLookups(*document);
    const SimulationOptions&                   loaded = trip.options();
    const std::shared_ptr<const MachAoALookup> table =
        drag ? loaded.getDragLookupTable() : loaded.getStabilityLookupTable();

    LongTableReload reload;
    reload.hasTable = table != nullptr;
    if (table != nullptr)
    {
        reload.maxMach = table->getMaxMach();
        reload.at39    = table->interpolate(39, 0, drag ? "cd" : "cp");
    }
    reload.rows = drag ? loaded.getDragLookupCsvRows() : loaded.getStabilityLookupCsvRows();
    for (const std::string& line : lookupLinesOf(trip.saved))
    {
        if (line.contains("<row>"))
        {
            reload.savedRows.push_back(line);
        }
    }
    reload.warnings = trip.warnings;
    return reload;
}

class LookupTruncationRoundTrip : public ::testing::TestWithParam<bool>
{ };

// Port of the core half of AerodynamicLookupTruncationTest.testEditingLongTablePreservesFinalRows
// (Swing; parameterized by the table, drag or stability): a table of 42 rows, longer than the
// dialog's preview, is saved with all its rows and read back whole. The dialog that edits it is
// Milestone 4's; the rows here are those it leaves. What OpenRocket writes for the same options
// is every row, the comment included (SaverPortsProbe).
TEST_P(LookupTruncationRoundTrip, TestEditingLongTablePreservesFinalRows)
{
    JavaTestPreferences   preferences;
    const LongTableReload loaded = reloadLongTable(preferences.store, GetParam());
    ASSERT_TRUE(loaded.hasTable);
    EXPECT_EQ(loaded.maxMach, 39);
    EXPECT_EQ(loaded.at39, 40);
    ASSERT_TRUE(loaded.rows.has_value());
    EXPECT_EQ(loaded.rows.value_or(Lines{}).size(), 42U);

    EXPECT_EQ(loaded.rows, longTableRows(GetParam()));
    ASSERT_EQ(loaded.savedRows.size(), 42U);
    EXPECT_EQ(loaded.savedRows.back(), "          <row># edited</row>");
    EXPECT_EQ(loaded.warnings, Lines{std::string(kIllegalTimeStep)});
}

INSTANTIATE_TEST_SUITE_P(DragAndStability, LookupTruncationRoundTrip, ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& paramInfo) {
                             return std::string(paramInfo.param ? "Drag" : "Stability");
                         });

}  // namespace
