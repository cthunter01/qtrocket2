#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "QtRocket/simulation/RunSample.h"

namespace QtRocket
{

class FlightConfiguration;

/// The progress of a running simulation as OpenRocket's run dialog estimates it, and the values
/// the dialog shows while the simulation runs: a port of the progress logic of its
/// InteractiveSimulationWorker (swing gui/simulation/SimulationRunDialog.java:307-487), without
/// Swing. The estimator does on the owning thread what Java's process() and simulationDone() do
/// on the event thread; the samples it is fed come from a RunSampleListener of the run (Java's
/// SimulationProgressListener), taken from it on the owning thread.
///
/// The estimate runs through three stages (Java's simulationStage, -2, -1 and 0), each mapping
/// one value of the last sample linearly onto a fraction of the flight (MathUtil::map):
/// - BOOST (-2), while the simulation time is below the burn-out estimate: the time, from 0 to
///   the estimate, onto 0 ... kBurnoutProgress;
/// - COAST (-1), from the first sample at or after the burn-out estimate (which fixes the
///   burn-out velocity: its vertical velocity, at least 0.1 m/s) while the vertical velocity is
///   not negative: the velocity, from the burn-out velocity to 0, onto kBurnoutProgress ...
///   kApogeeProgress;
/// - LANDING (0), from the first sample with a negative vertical velocity (which fixes the
///   apogee altitude: the maximum altitude so far, at least 1 m): the altitude, from the apogee
///   altitude to 0, onto kApogeeProgress ... 1.
/// A stage is left for good; one sample can pass from BOOST through COAST to LANDING. The
/// progress shown is the fraction in percent, rounded half up and narrowed as Java's
/// (int)(100 * p + 0.5) (MathUtil::javaIntCast), never less than before and clamped to 0 ... 100.
///
/// The maximum altitude and velocity follow the sustainer only, and every sample of a batch:
/// a sample of the active sustainer that rises (a positive vertical velocity) or is higher than
/// the maximum altitude makes its altitude the maximum altitude (also a lower one: "the last
/// apogee found is the real one", as Java's comment says) and raises the maximum velocity to
/// its velocity (Java's Math.max, so a NaN velocity makes it NaN).
///
/// Deviation (a bug found while porting): in COAST a sample whose vertical velocity is NaN, which
/// is neither negative nor not, falls through in Java to the landing estimate with an apogee
/// altitude of 0, whose singular range makes MathUtil.map throw on the event thread; here it
/// leaves the progress as it was (the sample is still the last one shown). The simulation's
/// NaN check stops a run before a step with a NaN velocity ends, so no run of the engine gets
/// there.
///
/// Not thread-safe: one thread (the owning thread) feeds it and reads it.
class SimulationProgressEstimator
{
public:
    /// The fraction of the flight at the burn-out estimate (Java: BURNOUT_PROGRESS).
    static constexpr double kBurnoutProgress = 0.4;
    /// The fraction of the flight at apogee (Java: APOGEE_PROGRESS).
    static constexpr double kApogeeProgress = 0.7;
    /// The least burn-out estimate (Java: Math.max(launchBurn + otherBurn, 0.1)), in s.
    static constexpr double kMinimumBurnoutTime = 0.1;
    /// The least burn-out velocity (Java: MathUtil.max(vz, 0.1)), in m/s.
    static constexpr double kMinimumBurnoutVelocity = 0.1;
    /// The least apogee altitude (Java: MathUtil.max(maxAltitude, 1)), in m.
    static constexpr double kMinimumApogeeAltitude = 1.0;

    /// The stage of the estimate (Java's simulationStage; its comment speaks of "0 ... n" for
    /// the landing of several stages, which it never implemented).
    enum class Stage
    {
        BOOST   = -2,  ///< the time from 0 to the burn-out estimate
        COAST   = -1,  ///< the vertical velocity from the burn-out velocity to 0
        LANDING = 0,   ///< the altitude from the apogee altitude to 0
    };

    /// An estimator for a run of a simulation of @p configuration, the simulation's active flight
    /// configuration (its duplicate's, or the simulation's own when the run is prepared: Java
    /// reads the document's simulation as it makes the worker), with the burn-out estimate
    /// estimateBurnoutTime(@p configuration). Keeps no reference to @p configuration.
    /// @throws BugError when an active motor configuration of @p configuration has no motor
    explicit SimulationProgressEstimator(const FlightConfiguration& configuration);

    /// An estimator with the burn-out estimate @p burnoutTimeEstimate (in s), at least
    /// kMinimumBurnoutTime as estimateBurnoutTime() gives it (Java's Math.max: a NaN stays NaN,
    /// and keeps BOOST from ever applying). The floor keeps every range process() maps from
    /// being singular, so process() never throws.
    explicit SimulationProgressEstimator(double burnoutTimeEstimate) noexcept;

    /// The burn-out estimate of a flight of @p configuration (SimulationRunDialog.java:335-349):
    /// the longest Motor::getBurnTimeEstimate() of the active motors that ignite at LAUNCH
    /// (MathUtil::max, which skips a NaN) plus the sum of those of the other active motors (any
    /// other ignition event, AUTOMATIC included), at least kMinimumBurnoutTime (Java's Math.max:
    /// a NaN sum stays NaN). One motor configuration counts once, whatever its mount's cluster
    /// or instance count. The sum goes in the order of getActiveMotors() (Java's follows a
    /// HashMap of the motor configuration ids, so three or more other motors may sum to a
    /// different last bit there).
    /// @throws BugError when an active motor configuration of @p configuration has no motor
    [[nodiscard]] static double estimateBurnoutTime(const FlightConfiguration& configuration);

    /// Takes in @p samples, the samples published since the last call, in order (Java:
    /// process(chunks)): the maxima from each of them, then the stage and the progress from the
    /// last one, which becomes getLastSample(). Nothing for no samples (Java's process() is
    /// never called without chunks).
    void process(std::span<const RunSample> samples);

    /// The run has ended, however it ended (Java: simulationDone(), which a cancelled or failed
    /// run reaches too): the progress becomes 100. The rest stays as it is, and further samples
    /// are still taken in.
    void finish();

    /// The progress in percent, 0 ... 100 (Java: the worker's progress property, the run
    /// dialog's progress bar is their integer mean); 0 before the first sample.
    [[nodiscard]] int getProgress() const noexcept { return m_progress; }

    /// The stage of the estimate; BOOST before the first sample.
    [[nodiscard]] Stage getStage() const noexcept { return m_stage; }

    /// The burn-out estimate, in s.
    [[nodiscard]] double getBurnoutTimeEstimate() const noexcept { return m_burnoutTimeEstimate; }

    /// The vertical velocity at the burn-out estimate (at least kMinimumBurnoutVelocity), in m/s;
    /// 0 before COAST (Java's field starts at 0).
    [[nodiscard]] double getBurnoutVelocity() const noexcept { return m_burnoutVelocity; }

    /// The altitude of the apogee (at least kMinimumApogeeAltitude), in m; 0 before LANDING
    /// (Java's field starts at 0).
    [[nodiscard]] double getApogeeAltitude() const noexcept { return m_apogeeAltitude; }

    /// The maximum altitude of the sustainer so far (see the class comment), in m; 0 before the
    /// first sample (Java: simulationMaxAltitude[index]).
    [[nodiscard]] double getMaxAltitude() const noexcept { return m_maxAltitude; }

    /// The maximum vertical velocity of the sustainer so far, in m/s; 0 before the first sample
    /// (Java: simulationMaxVelocity[index]).
    [[nodiscard]] double getMaxVelocity() const noexcept { return m_maxVelocity; }

    /// The last sample taken in (Java: simulationStatuses[index]); nullopt before the first.
    [[nodiscard]] const std::optional<RunSample>& getLastSample() const noexcept
    {
        return m_lastSample;
    }

    /// The value the run dialog shows after "Simulation time:" (SimuRunDlg.lbl.Simutime): the
    /// last sample's time in the default unit of the long times
    /// (UnitGroupId::LONG_TIME's toStringUnit(), "12.3 s"); "" before the first sample
    /// (SimulationRunDialog.java:244-253).
    [[nodiscard]] std::string getTimeText() const;

    /// The value the run dialog shows after "Altitude:" (SimuRunDlg.lbl.Altitude): the last
    /// sample's altitude and the maximum altitude in the default distance unit, "120 m (max.
    /// 150 m)"; "" before the first sample (SimulationRunDialog.java:255-257). " (max. " and
    /// ")" are literals of Java's that its translations do not cover.
    [[nodiscard]] std::string getAltitudeText() const;

    /// The value the run dialog shows after "Velocity:" (SimuRunDlg.lbl.Velocity): the last
    /// sample's vertical velocity and the maximum velocity in the default velocity unit, "-3.2
    /// m/s (max. 85.1 m/s)"; "" before the first sample (SimulationRunDialog.java:259-261).
    [[nodiscard]] std::string getVelocityText() const;

private:
    /// Java's setSimulationProgress(p): the new progress from the fraction @p fraction.
    void setProgress(double fraction) noexcept;

    double                   m_burnoutTimeEstimate;
    double                   m_burnoutVelocity{0.0};
    double                   m_apogeeAltitude{0.0};
    double                   m_maxAltitude{0.0};
    double                   m_maxVelocity{0.0};
    Stage                    m_stage{Stage::BOOST};
    int                      m_progress{0};
    std::optional<RunSample> m_lastSample;
};

/// The constant's name, e.g. "COAST".
[[nodiscard]] std::string_view name(SimulationProgressEstimator::Stage stage) noexcept;

}  // namespace QtRocket
