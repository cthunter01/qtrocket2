#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace QtRocket
{

/// Which simulations of a batch that runs together are done, and whether the batch is: the
/// completion bookkeeping of OpenRocket's run dialog (swing gui/simulation/
/// SimulationRunDialog.java: the simulationDone array, the static helper
/// markSimulationComplete() :263-285, and the loop of updateProgress() :219-231 that finds the
/// simulation whose values the dialog shows), without Swing. A class over the flags in place of
/// Java's static helper over a boolean[] that the dialog owns.
///
/// Java's run dialog marks a simulation done when its worker is done (simulationDone(), also
/// after a failure or a cancel, :428-440) and fires one SimulationChangeEvent when the mark is the
/// one that completes the batch: "A single batch-completion signal prevents the simulation table
/// from sorting while other workers are still publishing their result values." QtRocket's
/// workers publish nothing to the table (each run is adopted on the owning thread as it ends),
/// but the run dialog keeps Java's single event per batch, so the table refreshes and sorts once,
/// after the last simulation.
///
/// Not thread-safe: the owning thread's.
class SimulationBatchCompletion
{
public:
    /// A batch of @p count simulations, none of them done (Java: new boolean[n]).
    explicit SimulationBatchCompletion(std::size_t count);

    /// Marks simulation @p index done, and tells whether this completed the batch: true only for
    /// the call that marks the last simulation that was not done, false for every other call (also
    /// for a simulation that was done already, which stays done). Java:
    /// markSimulationComplete(completed, index).
    /// @throws BugError when @p index is not below size() (Java: an
    ///         ArrayIndexOutOfBoundsException)
    [[nodiscard]] bool markSimulationComplete(std::size_t index);

    /// Whether simulation @p index is done.
    /// @throws BugError when @p index is not below size()
    [[nodiscard]] bool isSimulationComplete(std::size_t index) const;

    /// Whether every simulation of the batch is done (also for a batch of none).
    [[nodiscard]] bool isBatchComplete() const noexcept;

    /// The first simulation that is not done, whose values Java's run dialog shows
    /// (updateProgress()); nullopt when the batch is complete (Java's dialog then closes).
    [[nodiscard]] std::optional<std::size_t> getFirstIncomplete() const noexcept;

    /// The number of simulations of the batch.
    [[nodiscard]] std::size_t size() const noexcept { return m_completed.size(); }

private:
    std::vector<bool> m_completed;
};

}  // namespace QtRocket
