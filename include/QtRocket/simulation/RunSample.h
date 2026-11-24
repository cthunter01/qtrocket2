#pragma once

namespace QtRocket
{

class SimulationStatus;

/// What OpenRocket's run dialog reads of the simulation statuses its progress listener
/// publishes while a simulation runs (swing gui/simulation/SimulationRunDialog.java): the
/// simulation time, the altitude and the vertical velocity it shows (updateProgress(), :252-261)
/// and estimates the progress from (InteractiveSimulationWorker.process(), :367-422), and
/// whether the sustainer is in the flight (process() takes its maxima only from such statuses).
/// An addition: RunSampleListener makes these samples, SimulationProgressEstimator reads them.
///
/// Deviation: Java publishes a copy of the whole status (new SimulationStatus(status), which
/// also clones the simulation conditions and every listener) and hands it to the event thread;
/// a sample keeps the four values the dialog reads, so it can be copied cheaply and handed from
/// the simulation's thread to another (it shares nothing with the run).
struct RunSample
{
    /// SimulationStatus::getSimulationTime(), in s.
    double simulationTime{0.0};
    /// The z coordinate of SimulationStatus::getRocketPosition(), in m: the altitude above the
    /// launch site.
    double altitude{0.0};
    /// The z coordinate of SimulationStatus::getRocketVelocity(), in m/s.
    double verticalVelocity{0.0};
    /// Whether stage 0, the sustainer, is active in the status's flight configuration
    /// (getConfiguration().isStageActive(0)): false in the branch of a stage that separated.
    bool sustainerActive{false};

    /// The sample of @p status, as Java's run dialog reads its copy of it.
    [[nodiscard]] static RunSample of(const SimulationStatus& status);

    /// Member by member (a NaN differs from everything, as with ==).
    [[nodiscard]] bool operator==(const RunSample& other) const noexcept = default;
};

}  // namespace QtRocket
