#include "QtRocket/simulation/SimulationBatchCompletion.h"

#include <cstddef>
#include <optional>

#include <gtest/gtest.h>

#include "simulation/SimulationTestSupport.h"

// SimulationBatchCompletion: which simulations of a batch that runs together are done, and the
// one mark that completes the batch (OpenRocket's SimulationRunDialog.markSimulationComplete()).

namespace
{

using QtRocket::SimulationBatchCompletion;
using QtRocket::Test::bugText;

// Port of SimulationRunDialogTest.reportsCompletionOnlyAfterTheLastWorkerFinishes (swing
// gui/simulation/SimulationRunDialogTest.java).
TEST(SimulationBatchCompletion, ReportsCompletionOnlyAfterTheLastWorkerFinishes)
{
    SimulationBatchCompletion completed(3);

    EXPECT_FALSE(completed.markSimulationComplete(2));
    EXPECT_FALSE(completed.markSimulationComplete(0));
    EXPECT_TRUE(completed.markSimulationComplete(1));
}

// Port of SimulationRunDialogTest.duplicateCompletionDoesNotPublishAnotherBatchCompletion.
TEST(SimulationBatchCompletion, DuplicateCompletionDoesNotPublishAnotherBatchCompletion)
{
    SimulationBatchCompletion completed(1);

    EXPECT_TRUE(completed.markSimulationComplete(0));
    EXPECT_FALSE(completed.markSimulationComplete(0));
}

// What the run dialog reads while the batch runs: which simulations are done, the first that
// is not (whose values it shows), and whether the batch is complete.
TEST(SimulationBatchCompletion, TellsWhichSimulationsAreDoneAndTheFirstThatIsNot)
{
    SimulationBatchCompletion completed(3);
    EXPECT_EQ(completed.size(), 3U);
    EXPECT_FALSE(completed.isBatchComplete());
    EXPECT_EQ(completed.getFirstIncomplete(), std::optional<std::size_t>{0});

    EXPECT_FALSE(completed.markSimulationComplete(0));
    EXPECT_TRUE(completed.isSimulationComplete(0));
    EXPECT_FALSE(completed.isSimulationComplete(1));
    EXPECT_EQ(completed.getFirstIncomplete(), std::optional<std::size_t>{1});

    EXPECT_FALSE(completed.markSimulationComplete(2));
    EXPECT_EQ(completed.getFirstIncomplete(), std::optional<std::size_t>{1});
    EXPECT_FALSE(completed.isBatchComplete());

    EXPECT_TRUE(completed.markSimulationComplete(1));
    EXPECT_TRUE(completed.isBatchComplete());
    EXPECT_EQ(completed.getFirstIncomplete(), std::nullopt);
}

// A batch of none is complete; a simulation that is not one of the batch is a bug (Java: an
// ArrayIndexOutOfBoundsException).
TEST(SimulationBatchCompletion, AnIndexOutsideTheBatchIsABug)
{
    const SimulationBatchCompletion none(0);
    EXPECT_TRUE(none.isBatchComplete());
    EXPECT_EQ(none.getFirstIncomplete(), std::nullopt);

    SimulationBatchCompletion two(2);
    EXPECT_EQ(bugText([&two] { static_cast<void>(two.markSimulationComplete(2)); }),
              "The simulation is not one of the batch");
    EXPECT_EQ(bugText([&two] { static_cast<void>(two.isSimulationComplete(5)); }),
              "The simulation is not one of the batch");
    EXPECT_EQ(two.getFirstIncomplete(), std::optional<std::size_t>{0});
}

}  // namespace
