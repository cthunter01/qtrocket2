#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/lookup/CsvMachAoALookup.h"
#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "document/TestDocuments.h"

// Bug 29 (see SimulationOptions::operator==): what the simulation dialog's Cancel undoes after
// an edit of an option that OpenRocket's equality ignores. The scenario is the sim-config
// scouting's CancelUndoProbe.java: the user renames the stage, opens the dialog (its undo
// position "Edit S"), changes one option and cancels, which undoes. OpenRocket printed:
// - "geodetic only: earlier rename of the stage after Cancel REVERTED (the earlier rename is
//   lost); geodetic Spherical approximation -> Spherical approximation; ...; redo available
//   true"
// - "launch into wind only: earlier rename of the stage after Cancel REVERTED ...; into wind
//   true -> true; redo available true"
// - "time step (counted by equals): earlier rename of the stage after Cancel kept; ...; redo
//   available true"
// With the fix every edit is counted, so Cancel undoes the edit and keeps the rename. Here the
// dialog's undo is the document's undo() itself (the dialog of run 3 calls it).

namespace
{

using QtRocket::GeodeticComputationStrategy;
using QtRocket::InMemoryPreferences;
using QtRocket::MachAoALookup;
using QtRocket::OpenRocketDocument;
using QtRocket::Result;
using QtRocket::Simulation;
using QtRocket::SimulationOptions;

/// The document of the probe, TestRockets.makeTestRocket_v100() with a simulation "S" and a
/// clean undo history.
class SimulationDialogCancel : public ::testing::Test
{
protected:
    SimulationDialogCancel()
    {
        m_simulation->setName("S");
        m_document->addSimulation(m_simulation);
        m_document->clearUndo();
        m_stageName = stageName();
    }

    /// The probe's steps: the user's earlier edit (the stage renamed), the dialog's undo
    /// position, the edit of the options by @p edit, and Cancel's undo.
    void editAndCancel(void (*edit)(SimulationOptions&))
    {
        m_document->addUndoPosition("Modify Nose cone");
        m_document->getRocket().getChild(0).setName(m_stageName + " renamed");
        m_document->addUndoPosition("Edit S");
        edit(options());
        ASSERT_TRUE(m_document->isUndoAvailable());
        m_document->undo();
    }

    /// The name of the stage, which an undo replaces.
    [[nodiscard]] std::string stageName() const
    {
        return m_document->getRocket().getChild(0).getName();
    }

    /// Whether the rename the user made before the dialog opened is still there.
    [[nodiscard]] bool renameKept() const { return stageName() == m_stageName + " renamed"; }

    /// The options of the document's simulation (undo loads into the same object).
    [[nodiscard]] SimulationOptions& options() const
    {
        return m_document->getSimulation(0)->getOptions();
    }

    [[nodiscard]] OpenRocketDocument& document() const noexcept { return *m_document; }

private:
    InMemoryPreferences                 m_preferences;
    std::unique_ptr<OpenRocketDocument> m_document{QtRocket::Test::makeTestRocketV100()};
    std::shared_ptr<Simulation>         m_simulation{
        std::make_shared<Simulation>(m_document.get(), m_document->getRocket(), m_preferences)};
    std::string m_stageName;
};

void computeOnTheEllipsoid(SimulationOptions& options)
{
    options.setGeodeticComputation(GeodeticComputationStrategy::WGS84);
}

void toggleLaunchIntoWind(SimulationOptions& options)
{
    options.setLaunchIntoWind(!options.getLaunchIntoWind());
}

void toggleIsa(SimulationOptions& options)
{
    options.setIsaAtmosphere(!options.isIsaAtmosphere());
}

/// A drag table whose coefficient is 0.5 everywhere.
[[nodiscard]] std::shared_ptr<const MachAoALookup> dragTable()
{
    const std::vector<std::string>   rows{"Mach,Cd", "0,0.5", "1,0.5"};
    const std::array<std::string, 1> columns{"cd"};
    Result<MachAoALookup>            table = QtRocket::CsvMachAoALookup::parse(rows, columns, ',');
    EXPECT_TRUE(table.has_value());
    if (!table.has_value())
    {
        return nullptr;
    }
    return std::make_shared<const MachAoALookup>(std::move(*table));
}

void nameTheDragTableFile(SimulationOptions& options)
{
    // The table the options have, now read from a file of another name.
    options.setDragLookup(std::filesystem::path("drag.csv"), options.getDragLookupTable(),
                          options.getDragLookupCsvRows());
}

void changeTheTimeStep(SimulationOptions& options)
{
    options.setTimeStep(0.02);
}

TEST_F(SimulationDialogCancel, AGeodeticEditIsUndoneAndTheEarlierRenameKept)
{
    editAndCancel(&computeOnTheEllipsoid);
    EXPECT_TRUE(renameKept()) << "OpenRocket reverts the rename";
    EXPECT_EQ(options().getGeodeticComputation(), GeodeticComputationStrategy::SPHERICAL);
    EXPECT_TRUE(document().isRedoAvailable());
}

TEST_F(SimulationDialogCancel, ALaunchIntoWindEditIsUndoneAndTheEarlierRenameKept)
{
    const bool intoWind = options().getLaunchIntoWind();
    editAndCancel(&toggleLaunchIntoWind);
    EXPECT_TRUE(renameKept()) << "OpenRocket reverts the rename";
    EXPECT_EQ(options().getLaunchIntoWind(), intoWind);
    EXPECT_TRUE(document().isRedoAvailable());
}

TEST_F(SimulationDialogCancel, AnIsaEditIsUndoneAndTheEarlierRenameKept)
{
    const bool isa = options().isIsaAtmosphere();
    editAndCancel(&toggleIsa);
    EXPECT_TRUE(renameKept()) << "OpenRocket reverts the rename";
    EXPECT_EQ(options().isIsaAtmosphere(), isa);
    EXPECT_TRUE(document().isRedoAvailable());
}

TEST_F(SimulationDialogCancel, ALookupPathEditIsUndoneAndTheEarlierRenameKept)
{
    options().setDragLookup(std::filesystem::path("table.csv"), dragTable());
    document().clearUndo();
    const std::filesystem::path path = options().getDragLookupCsvPath().value_or("");
    editAndCancel(&nameTheDragTableFile);
    EXPECT_TRUE(renameKept()) << "OpenRocket reverts the rename";
    EXPECT_EQ(options().getDragLookupCsvPath(), path);
    EXPECT_TRUE(document().isRedoAvailable());
}

TEST_F(SimulationDialogCancel, AnEditOpenRocketComparesIsUndoneAsThere)
{
    editAndCancel(&changeTheTimeStep);
    EXPECT_TRUE(renameKept());
    EXPECT_EQ(options().getTimeStep(), 0.05);
    EXPECT_TRUE(document().isRedoAvailable());
}

}  // namespace
