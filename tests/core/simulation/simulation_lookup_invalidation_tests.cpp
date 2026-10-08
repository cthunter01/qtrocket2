// OpenRocket's SimulationLookupInvalidationTest (core/src/test/java/info/openrocket/core/document/
// SimulationLookupInvalidationTest.java): a change of a lookup table of the options makes the
// results of a loaded simulation outdated. The Java test is in the document package, where
// OpenRocket keeps Simulation; it needs no document.
//
// Every test is parameterised over the two lookups as in Java (@ValueSource(booleans = {true,
// false})): true is the drag table, false the stability table.
//
// One difference in the wording: Java hands its SimulationOptions object to the simulation and
// goes on changing that same object. Here the options are moved into the simulation, so the
// tests change simulation.getOptions(), which is Java's object.

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/lookup/CsvMachAoALookup.h"
#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/Error.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::MachAoALookup;
using QtRocket::Result;
using QtRocket::Simulation;
using QtRocket::SimulationExtension;
using QtRocket::SimulationOptions;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;

using Status = Simulation::Status;

/// The table of the CSV @p rows with the value columns @p columns, which must parse (Java:
/// CsvMachAoALookup.parse(rows, columns, ',')).
[[nodiscard]] std::shared_ptr<const MachAoALookup> parseTable(const std::vector<std::string>& rows,
                                                              std::span<const std::string> columns)
{
    Result<MachAoALookup> table = QtRocket::CsvMachAoALookup::parse(rows, columns, ',');
    EXPECT_TRUE(table.has_value()) << (table.has_value() ? "" : table.error().message);
    if (!table.has_value())
    {
        return nullptr;
    }
    return std::make_shared<const MachAoALookup>(std::move(*table));
}

/// SimulationLookupInvalidationTest.setLookup(options, drag, value): a drag table whose
/// coefficient is @p value, or a stability table whose CP is @p value. @p value is the text
/// Java's string concatenation gives the double ("1.0").
void setLookup(SimulationOptions& options, bool drag, const std::string& value)
{
    if (drag)
    {
        static const std::array<std::string, 1> kColumns{"cd"};
        options.setDragLookup(std::nullopt, parseTable({"Mach,Cd", "0," + value}, kColumns));
    }
    else
    {
        static const std::array<std::string, 3> kColumns{"cn", "cm", "cp"};
        options.setStabilityLookup(std::nullopt,
                                   parseTable({"Mach,Cn,Cm,Cp", "0,1,1," + value}, kColumns));
    }
}

class SimulationLookupInvalidationTest : public ::testing::TestWithParam<bool>
{
protected:
    /// SimulationLookupInvalidationTest.loadedSimulation(options): a simulation of the Estes
    /// Alpha III in TEST_FCID_0, as a file would describe it, with the status LOADED.
    [[nodiscard]] Simulation& loadedSimulation(SimulationOptions options)
    {
        m_alpha.rocket->setSelectedConfiguration(testFcid(0));
        m_simulation = std::make_unique<Simulation>(
            nullptr, *m_alpha.rocket, Status::LOADED, "Test", std::move(options),
            std::vector<std::shared_ptr<SimulationExtension>>{}, nullptr);
        EXPECT_EQ(Status::LOADED, m_simulation->getStatus());
        return *m_simulation;
    }

private:
    TestEstesAlphaIII           m_alpha;
    std::unique_ptr<Simulation> m_simulation;
};

// SimulationLookupInvalidationTest.testAddingLookupInvalidatesResults
TEST_P(SimulationLookupInvalidationTest, AddingLookupInvalidatesResults)
{
    const bool  drag       = GetParam();
    Simulation& simulation = loadedSimulation(SimulationOptions());
    setLookup(simulation.getOptions(), drag, "1.0");
    EXPECT_EQ(Status::OUTDATED, simulation.getStatus());
}

// SimulationLookupInvalidationTest.testReplacingLookupInvalidatesResults
TEST_P(SimulationLookupInvalidationTest, ReplacingLookupInvalidatesResults)
{
    const bool        drag = GetParam();
    SimulationOptions options;
    setLookup(options, drag, "1.0");
    Simulation& simulation = loadedSimulation(std::move(options));

    setLookup(simulation.getOptions(), drag, "2.0");

    EXPECT_EQ(Status::OUTDATED, simulation.getStatus());
}

// SimulationLookupInvalidationTest.testClearingLookupInvalidatesResults
TEST_P(SimulationLookupInvalidationTest, ClearingLookupInvalidatesResults)
{
    const bool        drag = GetParam();
    SimulationOptions options;
    setLookup(options, drag, "1.0");
    Simulation& simulation = loadedSimulation(std::move(options));

    if (drag)
    {
        simulation.getOptions().clearDragLookup();
    }
    else
    {
        simulation.getOptions().clearStabilityLookup();
    }

    EXPECT_EQ(Status::OUTDATED, simulation.getStatus());
}

// SimulationLookupInvalidationTest.testUnchangedLookupKeepsResultsCurrent
TEST_P(SimulationLookupInvalidationTest, UnchangedLookupKeepsResultsCurrent)
{
    const bool        drag = GetParam();
    SimulationOptions options;
    setLookup(options, drag, "1.0");
    Simulation& simulation = loadedSimulation(std::move(options));

    // Java: assertEquals(options, options.clone())
    const SimulationOptions clone(simulation.getOptions());
    EXPECT_TRUE(simulation.getOptions() == clone);
    EXPECT_EQ(Status::LOADED, simulation.getStatus());
}

INSTANTIATE_TEST_SUITE_P(DragAndStability, SimulationLookupInvalidationTest, ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& paramInfo) {
                             return std::string(paramInfo.param ? "Drag" : "Stability");
                         });

}  // namespace
