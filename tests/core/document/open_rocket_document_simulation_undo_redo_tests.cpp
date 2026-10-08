#include <memory>
#include <optional>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "document/DocumentTestSupport.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationRunSupport.h"

// OpenRocketDocumentSimulationUndoRedoTest.java, its seven cases with their expectations, and
// one more for the contract the fifth names.
//
// Differences in the set-up, none of which a Java assertion depends on:
// - Java's `new Simulation(document, rocket)` makes the document a listener of the simulation at
//   once; here the document listens from addSimulation() on. Every case adds its simulations
//   before the first step it checks.
// - The simulations have the random seed 0 (AlphaDocument::newSimulation()); Java's have the
//   seeds their options drew. With the test preferences there is no wind for a seed to shape.
// - The document's undo and removal calls take a reference, and getSimulation() gives a
//   std::shared_ptr; an index is a std::size_t and "not found" is nullopt (Java: -1).

namespace
{

using QtRocket::FlightData;
using QtRocket::OpenRocketDocument;
using QtRocket::Simulation;
using QtRocket::Test::AlphaDocument;
using QtRocket::Test::simulateOrFail;
using QtRocket::Test::testFcid;

/// OpenRocketDocumentSimulationUndoRedoTest.EPSILON
constexpr double kEpsilon = 1e-6;

/// The fixture: Java's `Rocket rocket = TestRockets.makeEstesAlphaIII(); OpenRocketDocument
/// document = new OpenRocketDocument(rocket);` and createSimulation().
class OpenRocketDocumentSimulationUndoRedoTest : public ::testing::Test
{
protected:
    [[nodiscard]] OpenRocketDocument& document() const noexcept { return m_alpha.document(); }

    /// OpenRocketDocumentSimulationUndoRedoTest.createSimulation()
    [[nodiscard]] std::shared_ptr<Simulation> createSimulation(std::string_view name) const
    {
        return m_alpha.newSimulation(name, testFcid(0));
    }

private:
    AlphaDocument m_alpha;
};

// OpenRocketDocumentSimulationUndoRedoTest.undoRedoSimulationDeletion_restoresSimulationAndResults
TEST_F(OpenRocketDocumentSimulationUndoRedoTest,
       UndoRedoSimulationDeletionRestoresSimulationAndResults)
{
    const std::shared_ptr<Simulation> simulation = createSimulation("Delete me");
    document().addSimulation(simulation);

    simulateOrFail(*simulation);
    const std::shared_ptr<FlightData> dataBefore = simulation->getSimulatedData();
    ASSERT_NE(dataBefore, nullptr);
    EXPECT_GT(dataBefore->getMaxAltitude(), 0);

    document().clearUndo();

    document().addUndoPosition("Delete simulation");
    document().removeSimulation(*simulation);
    EXPECT_EQ(0U, document().getSimulationCount());

    document().undo();
    ASSERT_EQ(1U, document().getSimulationCount());

    const std::shared_ptr<Simulation> restored = document().getSimulation(0);
    EXPECT_EQ("Delete me", restored->getName());
    EXPECT_EQ(testFcid(0), restored->getFlightConfigurationId());

    const std::shared_ptr<FlightData> restoredData = restored->getSimulatedData();
    ASSERT_NE(restoredData, nullptr) << "Undoing simulation deletion should restore results";
    EXPECT_NEAR(dataBefore->getMaxAltitude(), restoredData->getMaxAltitude(), 0.001);
    EXPECT_EQ(Simulation::Status::UPTODATE, restored->getStatus());

    EXPECT_TRUE(document().isRedoAvailable());
    document().redo();
    EXPECT_EQ(0U, document().getSimulationCount());
    EXPECT_FALSE(document().isRedoAvailable());
}

// OpenRocketDocumentSimulationUndoRedoTest
//     .undoRedoSimulationSettingChanges_restoresSettingsAndKeepsResults
TEST_F(OpenRocketDocumentSimulationUndoRedoTest,
       UndoRedoSimulationSettingChangesRestoresSettingsAndKeepsResults)
{
    const std::shared_ptr<Simulation> simulation = createSimulation("Original");
    document().addSimulation(simulation);

    simulateOrFail(*simulation);
    const std::shared_ptr<FlightData> dataBefore = simulation->getSimulatedData();
    ASSERT_NE(dataBefore, nullptr);

    const double rodLengthBefore   = simulation->getOptions().getLaunchRodLength();
    const double maxAltitudeBefore = dataBefore->getMaxAltitude();

    document().clearUndo();

    document().addUndoPosition("Edit simulation");
    simulation->setName("Modified");
    simulation->getOptions().setLaunchRodLength(rodLengthBefore + 0.5);
    EXPECT_EQ(Simulation::Status::OUTDATED, simulation->getStatus());

    document().undo();
    EXPECT_EQ("Original", simulation->getName());
    EXPECT_NEAR(rodLengthBefore, simulation->getOptions().getLaunchRodLength(), kEpsilon);
    ASSERT_NE(simulation->getSimulatedData(), nullptr);
    EXPECT_NEAR(maxAltitudeBefore, simulation->getSimulatedData()->getMaxAltitude(), 0.001);
    EXPECT_EQ(Simulation::Status::UPTODATE, simulation->getStatus());

    document().redo();
    EXPECT_EQ("Modified", simulation->getName());
    EXPECT_NEAR(rodLengthBefore + 0.5, simulation->getOptions().getLaunchRodLength(), kEpsilon);
    ASSERT_NE(simulation->getSimulatedData(), nullptr);
    EXPECT_NEAR(maxAltitudeBefore, simulation->getSimulatedData()->getMaxAltitude(), 0.001);
    EXPECT_EQ(Simulation::Status::OUTDATED, simulation->getStatus());
}

// OpenRocketDocumentSimulationUndoRedoTest.redoNotClearedBySimulationRunWhileInUndoHistory
TEST_F(OpenRocketDocumentSimulationUndoRedoTest, RedoNotClearedBySimulationRunWhileInUndoHistory)
{
    const std::shared_ptr<Simulation> simulation = createSimulation("Original");
    document().addSimulation(simulation);
    document().clearUndo();

    document().addUndoPosition("Rename simulation");
    simulation->setName("Renamed");
    document().undo();

    EXPECT_EQ("Original", simulation->getName());
    EXPECT_TRUE(document().isRedoAvailable());

    simulateOrFail(*simulation);

    EXPECT_TRUE(document().isRedoAvailable())
        << "Running a simulation should not clear redo history";
    document().redo();
    EXPECT_EQ("Renamed", simulation->getName());
}

// OpenRocketDocumentSimulationUndoRedoTest
//     .redoClearedByNewSimulationSettingChangeWhileInUndoHistory
TEST_F(OpenRocketDocumentSimulationUndoRedoTest,
       RedoClearedByNewSimulationSettingChangeWhileInUndoHistory)
{
    const std::shared_ptr<Simulation> simulation = createSimulation("Original");
    document().addSimulation(simulation);
    document().clearUndo();

    document().addUndoPosition("Rename simulation");
    simulation->setName("Renamed");
    document().undo();

    EXPECT_TRUE(document().isRedoAvailable());

    simulation->setName("Different change");
    EXPECT_FALSE(document().isRedoAvailable()) << "A new simulation edit should clear redo history";
}

// OpenRocketDocumentSimulationUndoRedoTest
//     .getSimulationIndexAndRemoveSimulationUseIdentityNotEquals
TEST_F(OpenRocketDocumentSimulationUndoRedoTest,
       GetSimulationIndexAndRemoveSimulationUseIdentityNotEquals)
{
    // As in Java, the two are made alike and still not equal: the wind models of their options
    // drew seeds of their own. The next case has two simulations that are equal.
    const std::shared_ptr<Simulation> first  = createSimulation("Same");
    const std::shared_ptr<Simulation> second = createSimulation("Same");

    document().addSimulation(first);
    document().addSimulation(second);

    EXPECT_EQ(0U, document().getSimulationIndex(*first));
    EXPECT_EQ(1U, document().getSimulationIndex(*second));

    document().removeSimulation(*second);
    ASSERT_EQ(1U, document().getSimulationCount());
    EXPECT_EQ(first, document().getSimulation(0));
    EXPECT_EQ(std::nullopt, document().getSimulationIndex(*second));
}

// The same contract with a simulation and its copy(), which are equal in Java too. OpenRocket
// itself searches with equals() (ArrayList.indexOf() and remove(Object)): it finds the original
// at index 0 when asked for the copy, and removes the original when told to remove the copy.
TEST_F(OpenRocketDocumentSimulationUndoRedoTest, ACopyOfASimulationIsNotFoundInItsPlace)
{
    const std::shared_ptr<Simulation> original = createSimulation("Same");
    const std::shared_ptr<Simulation> copy     = original->copy();
    ASSERT_TRUE(*original == *copy);

    document().addSimulation(original);
    EXPECT_EQ(0U, document().getSimulationIndex(*original));
    EXPECT_EQ(std::nullopt, document().getSimulationIndex(*copy));

    // Removing a simulation that is not in the list removes nothing.
    document().removeSimulation(*copy);
    ASSERT_EQ(1U, document().getSimulationCount());
    EXPECT_EQ(original, document().getSimulation(0));

    document().addSimulation(copy);
    EXPECT_EQ(0U, document().getSimulationIndex(*original));
    EXPECT_EQ(1U, document().getSimulationIndex(*copy));

    document().removeSimulation(*copy);
    ASSERT_EQ(1U, document().getSimulationCount());
    EXPECT_EQ(original, document().getSimulation(0));
    EXPECT_EQ(std::nullopt, document().getSimulationIndex(*copy));
}

// OpenRocketDocumentSimulationUndoRedoTest
//     .undoRedoDeletionWithMultipleSimulations_restoresOrderAndResults
TEST_F(OpenRocketDocumentSimulationUndoRedoTest,
       UndoRedoDeletionWithMultipleSimulationsRestoresOrderAndResults)
{
    const std::shared_ptr<Simulation> first  = createSimulation("First");
    const std::shared_ptr<Simulation> second = createSimulation("Second");
    document().addSimulation(first);
    document().addSimulation(second);

    simulateOrFail(*first);
    simulateOrFail(*second);
    EXPECT_NE(first->getSimulatedData(), nullptr);
    EXPECT_NE(second->getSimulatedData(), nullptr);

    document().clearUndo();

    document().addUndoPosition("Delete first simulation");
    document().removeSimulation(*first);
    ASSERT_EQ(1U, document().getSimulationCount());
    EXPECT_EQ("Second", document().getSimulation(0)->getName());

    document().undo();
    ASSERT_EQ(2U, document().getSimulationCount());
    EXPECT_EQ("First", document().getSimulation(0)->getName());
    EXPECT_EQ("Second", document().getSimulation(1)->getName());
    EXPECT_NE(document().getSimulation(0)->getSimulatedData(), nullptr);
    EXPECT_NE(document().getSimulation(1)->getSimulatedData(), nullptr);

    document().redo();
    ASSERT_EQ(1U, document().getSimulationCount());
    EXPECT_EQ("Second", document().getSimulation(0)->getName());
}

// OpenRocketDocumentSimulationUndoRedoTest
//     .undoRedoSimulationSettingChange_onlyAffectsEditedSimulation
TEST_F(OpenRocketDocumentSimulationUndoRedoTest,
       UndoRedoSimulationSettingChangeOnlyAffectsEditedSimulation)
{
    const std::shared_ptr<Simulation> first  = createSimulation("First");
    const std::shared_ptr<Simulation> second = createSimulation("Second");
    document().addSimulation(first);
    document().addSimulation(second);
    document().clearUndo();

    document().addUndoPosition("Edit second simulation");
    second->setName("Second (edited)");

    document().undo();
    EXPECT_EQ("First", first->getName());
    EXPECT_EQ("Second", second->getName());

    document().redo();
    EXPECT_EQ("First", first->getName());
    EXPECT_EQ("Second (edited)", second->getName());
}

}  // namespace
