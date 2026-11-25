// The text of the simulations table (Java: swing file/SimulationTableCSVExport's generateCSVData()
// and SimulationPanel's addStatusColumn()). OpenRocket's SimulationTableCSVExportTest is not
// ported (O-M4-11, the critic's C5: its assertions are assertDoesNotThrow and assertNotNull on
// Mockito mocks of JTable and ColumnTableModel, with no expectation to keep); in its place:
// - SimulationTableCsvExportGolden: the CSV export (",", 3 decimals; ";", 2 decimals, exponential)
//   and the Copy of every row of the sixteen examples, as OpenRocket's SimulationPanel writes them
//   (tests/data/goldens/gui/simulation-table, SimulationTablePins.h), and the Copy of a table with
//   a row without data, each line with its own status (OpenRocket bug 25: the pins hold
//   QtRocket's text beside OpenRocket's);
// - SimulationTableCsvExportTest: the header with the hidden columns, the units of now, the rows
//   given, the warnings' delimiter, and the programming errors.

#include "QtRocket/gui/simulation/SimulationTableCsvExport.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "QtRocket/gui/simulation/SimulationTableView.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Strings.h"
#include "simulation/SimulationTablePins.h"
#include "simulation/SimulationTableTestSupport.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

using Column = SimulationColumns::Column;
using Gui::Test::SimulationTableFixture;
using SimulationTableCsvExportTest = Gui::Test::GuiTest;
using SimulationTableCopyGolden    = Gui::Test::GuiTest;

/// The lines of @p text (split at '\n', the last one kept also when empty).
[[nodiscard]] std::vector<std::string> linesOfText(std::string_view text)
{
    std::vector<std::string> lines;
    std::size_t              start = 0;
    while (true)
    {
        const std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos)
        {
            lines.emplace_back(text.substr(start));
            return lines;
        }
        lines.emplace_back(text.substr(start, end - start));
        start = end + 1;
    }
}

/// The names of the statuses of @p model's rows, from the top ("LOADED").
[[nodiscard]] std::vector<std::string> statusNames(const SimulationTableModel& model)
{
    std::vector<std::string> names;
    for (int row = 0; row < model.rowCount(); ++row)
    {
        names.emplace_back(name(model.status(row)));
    }
    return names;
}

/// The table of the example design whose file in data/examples is @p file (null and a failure when
/// it cannot be loaded).
[[nodiscard]] std::unique_ptr<SimulationTableFixture> exampleTable(std::string_view file)
{
    std::unique_ptr<OpenRocketDocument> document = Gui::Test::loadSimulationExample(file);
    return document == nullptr ? nullptr
                               : std::make_unique<SimulationTableFixture>(std::move(document));
}

// ======================================================================= the pinned texts

class SimulationTableCsvExportGolden : public Gui::Test::GuiTest,
                                       public ::testing::WithParamInterface<std::string>
{
protected:
    /// The pinned design of the test's parameter (null and a failure when there is none).
    [[nodiscard]] static const Gui::Test::SimulationTablePinDesign* design()
    {
        const Gui::Test::SimulationTablePins*      table = Gui::Test::simulationTablePinsOrFail();
        const Gui::Test::SimulationTablePinDesign* found =
            table == nullptr ? nullptr : table->findDesign(GetParam());
        if (table != nullptr && found == nullptr)
        {
            ADD_FAILURE() << "no pins of " << GetParam();
        }
        return found;
    }

    /// The design of @p pin, loaded as the probe loaded it, in a table.
    [[nodiscard]] static std::unique_ptr<SimulationTableFixture> tableOf(
        const Gui::Test::SimulationTablePinDesign& pin)
    {
        if (pin.kind == "new")
        {
            return std::make_unique<SimulationTableFixture>(
                OpenRocketDocumentFactory::createNewRocket());
        }
        return exampleTable(pin.file);
    }
};

TEST_P(SimulationTableCsvExportGolden, WritesTheTableAsOpenRocket)
{
    const Gui::Test::SimulationTablePinDesign* pin = design();
    ASSERT_NE(pin, nullptr);
    const std::unique_ptr<SimulationTableFixture> table = tableOf(*pin);
    ASSERT_NE(table, nullptr);
    // Every row, in the table's order (no sort: the document's), as the File menu's export.
    const std::vector<std::shared_ptr<Simulation>> rows = table->model().simulations();
    // The probe writes no text for a design without simulations: here the header alone.
    EXPECT_EQ(rows.empty(), !pin->csv.has_value());
    EXPECT_EQ(SimulationTableCsvExport::tableText(table->model(), rows, ",", 3, false),
              pin->csv.value_or(SimulationTableCsvExport::headerLine(",")));
    EXPECT_EQ(SimulationTableCsvExport::tableText(table->model(), rows, ";", 2, true),
              pin->csvExponential.value_or(SimulationTableCsvExport::headerLine(";")));
}

TEST_P(SimulationTableCsvExportGolden, CopiesEveryRowAsOpenRocket)
{
    const Gui::Test::SimulationTablePinDesign* pin = design();
    ASSERT_NE(pin, nullptr);
    const std::unique_ptr<SimulationTableFixture> table = tableOf(*pin);
    ASSERT_NE(table, nullptr);
    // Every row of the examples has data, so OpenRocket pairs the statuses rightly here; without
    // simulations, the status column's header and the others'.
    EXPECT_EQ(SimulationTableCsvExport::copyText(table->model(), table->model().simulations()),
              pin->copy.value_or("Status\t" + SimulationTableCsvExport::headerLine("\t")));
}

INSTANTIATE_TEST_SUITE_P(Designs, SimulationTableCsvExportGolden,
                         ::testing::ValuesIn(Gui::Test::simulationTablePinnedExampleDesigns()),
                         Gui::Test::simulationTablePinTestName);

// The pins' pairing: "A simple model rocket" with a new simulation (no data) inserted as the second
// row and the fourth row out of date. OpenRocket's Copy leaves the row without data out and gives
// every later line the status of the row before it (bug 25); QtRocket's gives each line its own.
TEST_F(SimulationTableCopyGolden, PairsEachLineWithItsOwnStatus)
{
    const Gui::Test::SimulationTablePins* pins = Gui::Test::simulationTablePinsOrFail();
    ASSERT_NE(pins, nullptr);
    const Gui::Test::SimulationTablePinPairing* pairing = pins->findPairing("copy_status_pairing");
    ASSERT_NE(pairing, nullptr);
    std::unique_ptr<OpenRocketDocument> document = Gui::Test::loadSimulationExample(pairing->file);
    ASSERT_NE(document, nullptr);

    // The probe's step: new Simulation(doc, doc.getRocket()) named getNextSimulationName() at
    // row 1, then row 3's launch rod 0.5 m longer.
    const auto added = std::make_shared<Simulation>(document.get(), document->getRocket());
    added->setName(document->getNextSimulationName());
    document->addSimulation(added, 1);
    SimulationOptions& options = document->getSimulation(3)->getOptions();
    options.setLaunchRodLength(options.getLaunchRodLength() + 0.5);

    const SimulationTableFixture table(std::move(document));
    ASSERT_EQ(pairing->steps.size(), 1U);
    EXPECT_EQ(statusNames(table.view().tableModel()), pairing->steps.front().statuses);

    const std::string copy = SimulationTableCsvExport::copyText(
        table.view().tableModel(), table.view().tableModel().simulations());
    EXPECT_EQ(copy, pairing->copy);
    EXPECT_NE(copy, pairing->openRocketCopy);
}

// ================================================================== the text's own rules

TEST_F(SimulationTableCsvExportTest, TheHeaderHasEveryColumnButTheStatusShownOrHidden)
{
    // OpenRocket's model columns 1 to 13: the column chooser hides view columns only, so the
    // hidden stepper column is there (critic C5's header test).
    EXPECT_EQ(SimulationTableCsvExport::headerLine(","),
              "Warnings,Name,Configuration,Sim. Stepper,Velocity off rod (m/s),Apogee (m),"
              "Velocity at deployment (m/s),Optimum delay (s),Max. velocity (m/s),"
              "Max. acceleration (m/s\xC2\xB2),Time to apogee (s),Flight time (s),"
              "Ground hit velocity (m/s)");

    const std::unique_ptr<SimulationTableFixture> table = exampleTable("A simple model rocket.ork");
    ASSERT_NE(table, nullptr);
    table->view().setColumnHidden(SimulationColumns::indexOf(Column::APOGEE), true);
    table->view().setColumnHidden(SimulationColumns::indexOf(Column::STATUS), true);
    const std::string text = SimulationTableCsvExport::tableText(
        table->model(), table->model().simulations(), ",", 3, false);
    EXPECT_EQ(linesOfText(text).front(), SimulationTableCsvExport::headerLine(","));
    EXPECT_EQ(SimulationTableCsvExport::kColumns.size(), 13U);
    EXPECT_EQ(SimulationTableCsvExport::kColumns.front(), Column::WARNINGS);
}

TEST_F(SimulationTableCsvExportTest, WritesTheUnitsOfNow)
{
    const std::unique_ptr<SimulationTableFixture> table = exampleTable("A simple model rocket.ork");
    ASSERT_NE(table, nullptr);
    const std::shared_ptr<Simulation>       first = table->model().simulation(0);
    const QtRocket::Test::DefaultUnitsGuard units;
    // The table read its cells in metres; the text takes the units of when it is made, its
    // header and its values alike (Java reads both then).
    ASSERT_TRUE(unitGroup(UnitGroupId::DISTANCE).setDefaultUnit("ft"));
    const std::string text =
        SimulationTableCsvExport::tableText(table->model(), {first}, ",", 3, false);
    const std::vector<std::string> lines = linesOfText(text);
    ASSERT_EQ(lines.size(), 2U);
    EXPECT_NE(lines.front().find("Apogee (ft)"), std::string::npos);
    const std::optional<double> apogee = SimulationColumns::summaryValue(Column::APOGEE, *first);
    ASSERT_TRUE(apogee.has_value());
    const std::string feet = Strings::doubleToString(
        unitGroup(UnitGroupId::DISTANCE).getDefaultUnit().toUnit(apogee.value_or(0.0)), 3, false);
    EXPECT_NE(lines.back().find("," + feet + ","), std::string::npos) << lines.back();
}

TEST_F(SimulationTableCsvExportTest, WritesTheRowsGivenInTheirOrderAndOnlyThoseWithData)
{
    const std::unique_ptr<SimulationTableFixture> table = exampleTable("A simple model rocket.ork");
    ASSERT_NE(table, nullptr);
    const SimulationTableModel&       model  = table->model();
    const std::shared_ptr<Simulation> second = model.simulation(1);
    const std::shared_ptr<Simulation> fourth = model.simulation(3);
    // A simulation without data has no line (and no status in the Copy).
    const auto added =
        std::make_shared<Simulation>(&table->document(), table->document().getRocket());
    added->setName("Without data");
    table->document().addSimulation(added);
    ASSERT_EQ(model.rowOf(*added), 5);

    const std::string all =
        SimulationTableCsvExport::tableText(model, model.simulations(), ",", 3, false);
    const std::vector<std::string> allLines = linesOfText(all);
    ASSERT_EQ(allLines.size(), 6U);

    const std::string some =
        SimulationTableCsvExport::tableText(model, {fourth, added, second}, ",", 3, false);
    EXPECT_EQ(linesOfText(some),
              (std::vector<std::string>{allLines.at(0), allLines.at(4), allLines.at(2)}));
    EXPECT_EQ(SimulationTableCsvExport::rowLine(model, *second, ",", 3, false).value_or(""),
              allLines.at(2));
    EXPECT_FALSE(SimulationTableCsvExport::rowLine(model, *added, ",", 3, false).has_value());

    const std::vector<std::string> copied =
        linesOfText(SimulationTableCsvExport::copyText(model, {fourth, added, second}));
    ASSERT_EQ(copied.size(), 3U);
    EXPECT_EQ(copied.at(1).substr(0, copied.at(1).find('\t')), "Loaded from file");
    EXPECT_NE(copied.at(1).find("\tSimulation 4\t"), std::string::npos);
    EXPECT_NE(copied.at(2).find("\tSimulation 2\t"), std::string::npos);
    // None: the header alone.
    EXPECT_EQ(SimulationTableCsvExport::tableText(model, {}, ";", 3, false),
              SimulationTableCsvExport::headerLine(";"));
}

TEST_F(SimulationTableCsvExportTest, JoinsTheWarningsByTheirDelimiter)
{
    const std::unique_ptr<SimulationTableFixture> table =
        exampleTable("Dual parachute deployment.ork");
    ASSERT_NE(table, nullptr);
    const std::shared_ptr<Simulation> simulation = table->model().simulation(2);
    const WarningSet*                 warnings   = simulation->getSimulatedWarnings();
    ASSERT_NE(warnings, nullptr);
    ASSERT_EQ(warnings->size(), 2U);
    auto              it     = warnings->begin();
    const std::string first  = it->toString();
    const std::string second = (++it)->toString();

    // " | " between two warnings, " ; " when the separator is "|" itself; the whole escaped.
    EXPECT_EQ(SimulationTableCsvExport::warningsText(*simulation, ","),
              Strings::escapeCsv(first + " | " + second));
    EXPECT_EQ(SimulationTableCsvExport::warningsText(*simulation, "|"),
              Strings::escapeCsv(first + " ; " + second));
    const std::optional<std::string> line =
        SimulationTableCsvExport::rowLine(table->model(), *simulation, "|", 3, false);
    ASSERT_TRUE(line.has_value());
    EXPECT_EQ(line.value_or("").substr(0, line.value_or("").find("|Simulation")),
              Strings::escapeCsv(first + " ; " + second));

    // Nothing without data (without warnings: the goldens' empty first fields).
    const Simulation fresh(&table->document(), table->document().getRocket());
    EXPECT_EQ(SimulationTableCsvExport::warningsText(fresh, ","), "");
}

TEST_F(SimulationTableCsvExportTest, RefusesASimulationThatIsNoRow)
{
    const SimulationTableFixture      table(OpenRocketDocumentFactory::createNewRocket());
    const std::shared_ptr<Simulation> stranger =
        std::make_shared<Simulation>(&table.document(), table.document().getRocket());
    EXPECT_THROW(static_cast<void>(SimulationTableCsvExport::tableText(table.view().tableModel(),
                                                                       {stranger}, ",", 3, false)),
                 BugError);
    EXPECT_THROW(
        static_cast<void>(SimulationTableCsvExport::copyText(table.view().tableModel(), {nullptr})),
        BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
