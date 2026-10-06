#pragma once

#include <deque>
#include <memory>
#include <optional>

#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/simulation/SimulationEngine.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/SimulationStepper.h"

namespace QtRocket
{

class Coordinate;
class FlightData;
class FlightEvent;
class RecoveryDevice;
class Rocket;
class SimulationConditions;

/// The event-driven simulation engine (OpenRocket's simulation/BasicEventSimulationEngine): it
/// runs a flight from the launch to the landing of every stage. It keeps a queue of flight
/// events per branch (in the simulation status), handles the events that are due, lets the
/// stepper of the current flight phase take one step at most up to the next event, and adds the
/// events the new state calls for (lift-off, launch rod clearance, apogee, tumbling, ground
/// hit). A stage that separates gets a simulation status and a flight data branch of its own,
/// which is simulated after the branch it separated from.
///
/// One run, simulate():
/// - the flight stepper is the options' choice (SimulationStepperMethod): RK4 or RK6;
/// - the rocket of the conditions is copied (Rocket::copyRocketWithOriginalId()), the copy
///   selects the simulated flight configuration (which settles the positions that depend on
///   the configuration), and the simulated configuration is a clone of the original one bound
///   to the copy, with the original's stage activeness. Nothing of the caller's rocket is
///   changed, and it is read again only by the nested run described below, which copies it
///   once more;
/// - the first status and its branch (named after the topmost active stage, "(null)" without
///   one) are made, and the sanity checks run in this order: no active stage and no motors
///   abort the simulation (a SIM_ABORT event each); no recovery device, or a drogue without a
///   main parachute, add a warning;
/// - the LAUNCH event is queued, the listeners' startSimulation() is called, and the branches
///   are simulated, last separated first (a stack), until none is left. Around every branch the
///   listeners' startSimulationBranch() and endSimulationBranch() are called; a branch without
///   data adds Warning::kEmptyBranch; then endSimulation() is called;
/// - whatever happens, the flight data take their summary values at the end
///   (FlightData::calculateInterestingValues()).
///
/// A SimulationException (from a listener, or from a computation that went wrong) ends the run:
/// the listeners' endSimulation() gets it, an EXCEPTION event with its message goes to the
/// branch that was being simulated, then endSimulationBranch() gets it, and it leaves
/// simulate(). The flight data hold what was computed up to then.
///
/// The optimum altitude: when a recovery device deploys before apogee, the engine runs the
/// whole flight once more in a nested engine, on a clone of the conditions with the system
/// listeners only plus the OptimumCoastListener (which refuses every deployment and ends the
/// run at apogee), and takes the maximum altitude and the time to apogee of that run as the
/// branch's optimum altitude and the time to it.
///
/// Ownership: the engine holds the rocket copy by std::shared_ptr and hands it to the flight
/// data on every way out of simulate() (FlightData::setSimulatedRocket()), so that the sources
/// of the events and the mounts of the motor states stay valid for as long as the flight data
/// live. The simulation statuses, their flight configurations and the conditions (with the
/// listener clones) die with the engine. The conditions refer to their Simulation and, through
/// it, to the caller's rocket: both must outlive simulate().
///
/// Threads: an engine has no shared or static state. Two engines may run at the same time when
/// their conditions belong to two simulations of two rocket copies
/// (Simulation::duplicateForIndependentSimulation()).
///
/// Deviations from OpenRocket:
/// - The LAUNCH event's source is the rocket copy the engine simulates (Java: the caller's
///   rocket, the only event of a run that points there): the copy has the same id and the same
///   stage number, and it is the one the flight data keep alive.
/// - The motors that one event ignites are queued in the order of the status's motor states,
///   the order of the motor mounts in the component tree (Java: the hash order of the
///   configuration's motor map). The ignition events of two mounts of one stage at the same
///   time compare equal, so they leave the queue, and reach the flight data branch, in an order
///   that follows from the order in which they were queued: it can differ from OpenRocket's,
///   whose own order changes with the component ids.
/// - An engine that is used again starts afresh. Java keeps the branches that an earlier run of
///   the same engine left unsimulated (after an exception) and would simulate them at the end
///   of the next run; nothing in OpenRocket uses an engine twice.
/// - BugError where Java throws a NullPointerException or a ClassCastException: conditions
///   without a simulation; a status without a flight data branch or without a warning set; an
///   IGNITION, BURNOUT or EJECTION_CHARGE event without a motor state; an IGNITION event whose
///   source is not a motor mount or whose motor is not a ThrustCurveMotor; a STAGE_SEPARATION
///   event without a source; a RECOVERY_DEVICE_DEPLOYMENT event whose source is not a recovery
///   device (Java passes a null device to the listeners first).
/// - The nested optimum-coast run: Java catches every exception that is not a
///   SimulationException there, logs it and returns null, which its caller then dereferences
///   (a NullPointerException leaves the outer run). Here a BugError of the nested run passes
///   through as it is.
/// - Java's finally block around a branch (endSimulationBranch() with a null exception) also
///   runs on the way out of an exception that is not a SimulationException; so it does here,
///   on the way out of a BugError.
/// - Logging is not ported.
class BasicEventSimulationEngine final : public SimulationEngine
{
public:
    /// The thrust must be below this value, in N, for the transition to tumbling
    /// (THRUST_TUMBLE_CONDITION; an arbitrary value, as Java notes).
    static constexpr double kThrustTumbleCondition = 0.01;

    /// The name of the branch of a rocket without an active stage
    /// (BasicEventSimulationEngine.nullBranchName).
    static constexpr const char* kNullBranchName = "(null)";

    /// The message of the exception for a NaN in the simulation status
    /// (BasicEventSimulationEngine.error.NaNResult).
    static constexpr const char* kNaNResult =
        "Simulation resulted in not-a-number (NaN) value, please report a bug.";

    BasicEventSimulationEngine();
    BasicEventSimulationEngine(const BasicEventSimulationEngine&)            = delete;
    BasicEventSimulationEngine& operator=(const BasicEventSimulationEngine&) = delete;
    BasicEventSimulationEngine(BasicEventSimulationEngine&&)                 = delete;
    BasicEventSimulationEngine& operator=(BasicEventSimulationEngine&&)      = delete;
    ~BasicEventSimulationEngine() override;

    /// Simulates the flight (see the class comment).
    /// @throws SimulationException from a simulation listener or from a failed computation
    /// @throws BugError when @p simulationConditions is null or has no simulation
    void simulate(const std::shared_ptr<SimulationConditions>& simulationConditions) override;

    /// The flight data of the run: the object simulate() fills, also when simulate() ended in
    /// an exception; null before the first simulate().
    [[nodiscard]] const std::shared_ptr<FlightData>& getFlightData() const noexcept override
    {
        return m_flightData;
    }

private:
    /// The status that is being simulated (Java: currentStatus).
    /// @throws BugError when there is none
    [[nodiscard]] SimulationStatus& status();

    /// The body of Java's try block in simulate(): the set-up, the sanity checks and the loop
    /// over the branches.
    void runSimulation(const std::shared_ptr<SimulationConditions>& simulationConditions);

    /// Makes the first status with its branch and runs the sanity checks on design and
    /// configuration.
    void setUpFirstStatus(const std::shared_ptr<SimulationConditions>& simulationConditions);

    /// Simulates the next branch of the stack, with the listener calls around it.
    void simulateBranch(const SimulationConditions& simulationConditions);

    /// Java's finally block of simulate().
    void finishFlightData();

    /// Simulates the current branch from its status to its end.
    void simulateLoop(const SimulationConditions& simulationConditions);

    /// The stepper a branch starts with: on the ground the ground stepper, tumbling the tumble
    /// stepper, with a recovery device deployed the landing stepper, otherwise the flight
    /// stepper.
    void selectStepper();

    /// Makes @p stepper the current stepper and hands it the current status (Java:
    /// currentStepper = stepper; currentStatus = currentStepper.initialize(currentStatus)).
    void switchStepper(SimulationStepper& stepper);

    /// The pre-step hook, the step (at most to the next event) and the post-step hook.
    void takeStep();

    /// What follows a step: the ALTITUDE event, the maximum altitude, lift-off or ground hit,
    /// the launch rod, apogee, tumbling and the end of the simulation.
    void addStepEvents(double oldAlt, const Coordinate& origin, const Coordinate& originVelocity,
                       double previousSimulationTime);

    /// The lift-off test before lift-off (the rocket does not sink into the ground) and the
    /// ground hit test after it. Returns the position relative to the start location.
    [[nodiscard]] Coordinate checkLiftoffAndGroundHit(const Coordinate& origin,
                                                      const Coordinate& originVelocity);

    /// The tumble detector and, otherwise, the large angle of attack warning.
    void checkTumbling();

    /// Handles the events of the queue that are due. False when the branch has ended.
    [[nodiscard]] bool handleEvents(const SimulationConditions& simulationConditions);

    /// Handles one event; sets @p ret to false when it ends the branch.
    void handleEvent(const FlightEvent& event, bool& ret);

    /// Queues an IGNITION event for every active motor @p event ignites.
    void queueIgnitions(const FlightEvent& event);

    /// Queues a STAGE_SEPARATION event for every active stage @p event separates.
    void queueSeparations(const FlightEvent& event);

    /// Queues a RECOVERY_DEVICE_DEPLOYMENT event for every recovery device @p event deploys.
    void queueDeployments(const FlightEvent& event);

    /// Java's switch over the event type. False for Java's `continue` (the rest of the
    /// handling of this event is skipped).
    [[nodiscard]] bool dispatchEvent(const FlightEvent& event, bool& ret);

    [[nodiscard]] bool handleIgnition(const FlightEvent& event);
    void               handleBurnout(const FlightEvent& event);
    void               handleEjectionCharge(const FlightEvent& event);
    void               handleStageSeparation(const FlightEvent& event);
    void               handleApogee(const FlightEvent& event);
    void               handleRecoveryDeviceDeployment(const FlightEvent& event);
    void               handleGroundHit(const FlightEvent& event);
    void               handleTumble(const FlightEvent& event);

    /// The speed warnings of a recovery device that deploys.
    void addDeploymentSpeedWarnings(const RecoveryDevice& deployingDevice);

    /// The next flight event to handle, taken out of the queue, or nullopt if no more events
    /// should be handled. Jumps the simulation time forward to the event while no motor has
    /// ignited.
    [[nodiscard]] std::optional<FlightEvent> nextEvent();

    /// Checks that the active stages of @p currentStatus can be simulated: when a branch
    /// starts, and whenever a stage separates.
    static void checkGeometry(SimulationStatus& currentStatus);

    /// @throws SimulationCalculationException when the current status holds a NaN
    void checkNaN();

    /// Runs the nested simulation that finds the optimum altitude (see the class comment) and
    /// returns its flight data.
    [[nodiscard]] std::shared_ptr<FlightData> computeCoastTime();

    /// The rocket that is simulated; declared first, so that it is destroyed after the
    /// statuses, whose configurations refer to it.
    std::shared_ptr<Rocket> m_simulationRocket;

    std::shared_ptr<FlightData> m_flightData;

    /// One stepper per flight phase, kept for the whole run (Java: the fields): the RK4 stepper
    /// until simulate() takes the options' choice, the landing, the tumble and the ground
    /// stepper.
    std::unique_ptr<SimulationStepper> m_flightStepper;
    std::unique_ptr<SimulationStepper> m_landingStepper;
    std::unique_ptr<SimulationStepper> m_tumbleStepper;
    std::unique_ptr<SimulationStepper> m_groundStepper;

    SimulationStepper* m_currentStepper{nullptr};

    std::optional<SimulationStatus> m_currentStatus;

    FlightConfigurationId m_fcid{FlightConfigurationId::errorId()};

    /// The simulation branches still to simulate: a stack whose top is the front (Java: an
    /// ArrayDeque used with push(), peek() and pop()).
    std::deque<SimulationStatus> m_toSimulate;
};

}  // namespace QtRocket
