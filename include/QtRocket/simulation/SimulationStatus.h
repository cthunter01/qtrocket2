#pragma once

#include <any>
#include <chrono>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/simulation/AbstractRkSimulationStepper.h"
#include "QtRocket/simulation/EventQueue.h"
#include "QtRocket/simulation/TumbleDetector.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/MonitorableSet.h"
#include "QtRocket/util/Quaternion.h"
#include "QtRocket/util/WorldCoordinate.h"

namespace QtRocket
{

class FlightConfiguration;
class FlightDataBranch;
class FlightEvent;
class MotorClusterState;
class MotorConfigurationId;
class RecoveryDevice;
class SimulationConditions;
class Warning;
class WarningSet;

/// The dynamic status of a rocket during its flight (OpenRocket's simulation/SimulationStatus):
/// the time, the position (relative to the launch site and in world coordinates), the velocity,
/// the orientation and the rotation velocity, the motor states, the flags (motor ignited,
/// lift-off, launch rod cleared, apogee reached, tumbling, landed), the deployed recovery
/// devices, the queue of the flight events still to handle, the warnings, and the simulation
/// conditions, the flight configuration and the flight data branch it works with. The engine
/// makes one for the sustainer and one for every stage that separates; the steppers advance it
/// and store it to the flight data branch at the end of each step (storeData()).
///
/// A new status (the constructor): at time 0, at the launch position, velocity and site of the
/// conditions, without rotation, and with
/// - the orientation of the launch rod: a roll about z by -theta - (pi / 2 - direction), where
///   theta is the lateral wind direction of new FlightConditions (0) and direction the launch
///   rod direction, then the launch rod angle about y, then pi / 2 - direction about z (each a
///   Quaternion::rotation() multiplied from the left, in that order);
/// - the effective launch rod length: the launch rod length of the conditions, shortened by the
///   distance from the aft end of the aft-most launch lug (LaunchLug, among the components of
///   FlightConfiguration::getActiveComponents(); the x of the first instance of
///   toAbsolute(length)) to the aft end of the rocket (the largest x of
///   FlightConfiguration::getBounds(), at least 0), never below 0. Without a lug the rocket is
///   taken to fly off a tower of the full length; a lug behind the bounds changes nothing;
/// - one motor state for every motor of the configuration (FlightConfiguration::getAllMotors(),
///   in its order: see there for how it differs from Java's hash order);
/// - an empty warning set of its own, and no flight data branch.
///
/// Ownership and sharing: everything Java holds by reference is held by std::shared_ptr, so
/// that the two ways of copying a status share exactly what Java's share:
/// - the copy constructor (Java: SimulationStatus(SimulationStatus), what a Runge-Kutta stepper
///   starts from and what the branch of a separating stage is made with) has a clone() of the
///   conditions, and so clones of the listeners; a clone of the flight configuration with the
///   same stage flags (FlightConfiguration::clone(), then copyStages()); the same flight data
///   branch; the same motor states in a list of its own; a copy of the tumble detector; the
///   same deployed recovery devices in a set of its own; the same events in a queue of its own;
///   a new, empty warning set; the same extra data in a map of its own; and every value,
///   except the maximum altitude and its time, which Java does not copy: they start anew at
///   minus infinity and 0;
/// - clone() (Java: Object.clone(), what a Runge-Kutta stepper makes for each intermediate
///   point of a step) shares everything: the conditions, the configuration, the branch, the
///   list of motor states, the tumble detector, the set of deployed devices, the event queue,
///   the warning set and the extra data map are the same objects, and the values are copied.
///   An event added to the queue of a clone is in the queue of the original.
/// The motor states are shared in both cases: igniting one ignites it for every status that
/// holds it, and for the events that carry it (see MotorClusterState, "Identity").
/// Moving a status hands it over as it is.
///
/// Lifetimes: the status refers to the rocket of its flight configuration, to its components
/// (the motor mounts of the motor states, the sources of the queued events, the deployed
/// recovery devices) and, through the conditions, to the Simulation; all of them must outlive
/// it. The engine keeps the rocket it simulates alive (see FlightData::setSimulatedRocket()).
///
/// Monitoring (Java: Monitorable; the class satisfies the Monitorable concept): the
/// modification id is redrawn exactly where Java redraws it, which each setter says. A
/// simulation listener that changes the status through one of those setters is found out by
/// SimulationListenerHelper, which compares the id around every call. As in Java some changes
/// draw none: the rotation velocity, the separated-stage flag, the extra data, copyProperties(),
/// and what is reached through the status (the event queue, the motor states, the deployed
/// devices, the tumble detector, the configuration). The setters that replace a shared object
/// (the conditions, the configuration, the branch, the warnings) draw a second id (Java:
/// modIDadd), which, as in Java, nothing reads.
///
/// Deviations from OpenRocket:
/// - The constructor and the copy constructor throw BugError for null conditions or a null
///   configuration (Java: NullPointerException), and addWarning() for a warning added without a
///   flight data branch (Java: NullPointerException, after the warning went into the set).
/// - The wall-clock start is a std::chrono::steady_clock time point (Java: System.nanoTime()).
/// - A SIM_WARN event holds a copy of the warning with the same id (see FlightEvent); Java's
///   event holds the object the warning set holds.
/// - The extra data are std::any values (Java: Object). A copy of the status copies the map
///   and with it the values: what the copies must share is put in as a std::shared_ptr (Java:
///   the copies refer to the same objects).
/// - getActiveMotorStates() is an addition for MassCalculator, which cannot see this class.
/// - isAttached() and recordWarnings() are public (Java: private and package-private).
/// - Java's fields maxAlt and maxAltTime are package-visible; here they are reached through
///   their getters and setters only.
/// - There is no copy assignment; the copy constructor is explicit, so that a status is never
///   copied by accident (a copy clones the listeners and the configuration).
class SimulationStatus
{
public:
    /// The clock of the wall-clock start.
    using WallClock = std::chrono::steady_clock;

    /// The time after leaving the launch rod before flight event warnings are recorded, s
    /// (WARNINGS_WAIT).
    static constexpr double kWarningsWait = 0.25;

    /// When the z velocity decreases to this proportion of the maximum z velocity, most flight
    /// event warnings are no longer recorded (WARNINGS_VEL).
    static constexpr double kWarningsVel = 0.2;

    /// A status at the start of a flight (see the class comment).
    /// @throws BugError when @p configuration or @p simulationConditions is null
    SimulationStatus(std::shared_ptr<FlightConfiguration>  configuration,
                     std::shared_ptr<SimulationConditions> simulationConditions);

    /// Java's copy constructor (see the class comment, "Ownership and sharing"). When used for
    /// the branch of a separating stage, a new FlightDataBranch has to be given to the copy.
    /// @throws BugError when @p orig has no conditions
    // Explicit: a copy clones the listeners and the configuration, so it has to be asked for.
    // NOLINTNEXTLINE(cppcoreguidelines-explicit-constructor,misc-explicit-constructor): see above
    explicit SimulationStatus(const SimulationStatus& orig);

    SimulationStatus& operator=(const SimulationStatus&)     = delete;
    SimulationStatus(SimulationStatus&&) noexcept            = default;
    SimulationStatus& operator=(SimulationStatus&&) noexcept = default;
    ~SimulationStatus()                                      = default;

    /// Java's clone(): a status that shares everything with this one and has copies of its
    /// values (see the class comment), for the intermediate points of a step.
    [[nodiscard]] SimulationStatus clone() const;

    /// Copies the kinematics (position, world position, velocity, orientation, rotation
    /// velocity) and the flags motor ignited, lift-off, launch rod cleared and apogee reached
    /// of @p orig. The modification id stays.
    void copyProperties(const SimulationStatus& orig) noexcept;

    // ----------------------------------------------------------------------------- time

    /// Sets the simulation time, s, and draws a new modification id.
    void                 setSimulationTime(double time) noexcept;
    [[nodiscard]] double getSimulationTime() const noexcept { return m_time; }

    // -------------------------------------------------------- configuration and motors

    /// Sets the flight configuration (the modification id stays).
    /// @throws BugError when @p configuration is null
    void setConfiguration(std::shared_ptr<FlightConfiguration> configuration);

    /// The flight configuration: the stages that still fly in this branch.
    [[nodiscard]] FlightConfiguration& getConfiguration() noexcept { return *m_configuration; }
    [[nodiscard]] const FlightConfiguration& getConfiguration() const noexcept
    {
        return *m_configuration;
    }
    /// getConfiguration().
    [[nodiscard]] FlightConfiguration& getFlightConfiguration() noexcept
    {
        return *m_configuration;
    }
    [[nodiscard]] const FlightConfiguration& getFlightConfiguration() const noexcept
    {
        return *m_configuration;
    }
    /// The flight configuration as the status holds it (an addition, for who needs to share
    /// it).
    [[nodiscard]] const std::shared_ptr<FlightConfiguration>& getConfigurationPointer()
        const noexcept
    {
        return m_configuration;
    }

    /// Every motor state, in the order of the configuration's motors when the status was made.
    [[nodiscard]] const std::vector<std::shared_ptr<MotorClusterState>>& getMotors() const noexcept
    {
        return *m_motorStateList;
    }

    /// The motor states whose mount is in an active stage of the configuration, in the order of
    /// getMotors(); a new list.
    [[nodiscard]] std::vector<std::shared_ptr<MotorClusterState>> getActiveMotors() const;

    /// getActiveMotors() as the in-flight entry points of MassCalculator take the motor states
    /// (an addition: Java passes the status to MassCalculator.calculateMotor(status) and
    /// calculate(type, status), which read getConfiguration(), getSimulationTime() and
    /// getActiveMotors()). The pointers are valid while the states live.
    [[nodiscard]] std::vector<const MotorClusterState*> getActiveMotorStates() const;

    /// Does nothing and returns false, as in Java (an unfinished method there).
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static): Java's instance method
    [[nodiscard]] bool moveBurntOutMotor(const MotorConfigurationId& motor) const noexcept;

    // --------------------------------------------------------------------- flight data

    /// Sets the flight data branch the status is stored to (the modification id stays).
    void setFlightDataBranch(std::shared_ptr<FlightDataBranch> flightDataBranch);

    /// The flight data branch, or null while none is set.
    [[nodiscard]] const std::shared_ptr<FlightDataBranch>& getFlightDataBranch() const noexcept
    {
        return m_flightDataBranch;
    }

    /// Stores the status to the flight data branch: a new point with the time, the altitude
    /// (z), the altitude above sea level, the position x and y, the latitude and longitude in
    /// degrees, the lateral distance and its direction (0 = north), the lateral, vertical and
    /// total velocity, the orientation (the angle theta above the horizon and the azimuth phi
    /// of the rocket's axis) and the computation time since the wall-clock start. Also raises
    /// the maximum z velocity to the current one.
    /// @throws BugError without a flight data branch (Java: NullPointerException), or when the
    ///         branch is immutable
    void storeData();

    // ----------------------------------------------------------------------- kinematics

    /// Sets the rocket position relative to the launch site (at t = 0 s it is (0, 0, 0)) and
    /// draws a new modification id.
    void                            setRocketPosition(const Coordinate& position) noexcept;
    [[nodiscard]] const Coordinate& getRocketPosition() const noexcept { return m_position; }

    /// Sets the rocket position in world coordinates (with the launch site's altitude,
    /// longitude and latitude) and draws a new modification id.
    void                                 setRocketWorldPosition(const WorldCoordinate& wc) noexcept;
    [[nodiscard]] const WorldCoordinate& getRocketWorldPosition() const noexcept
    {
        return m_worldPosition;
    }

    /// Sets the rocket velocity and draws a new modification id.
    void                            setRocketVelocity(const Coordinate& velocity) noexcept;
    [[nodiscard]] const Coordinate& getRocketVelocity() const noexcept { return m_velocity; }

    [[nodiscard]] const Quaternion& getRocketOrientationQuaternion() const noexcept
    {
        return m_orientation;
    }
    /// Sets the orientation and draws a new modification id.
    void setRocketOrientationQuaternion(const Quaternion& orientation) noexcept;

    [[nodiscard]] const Coordinate& getRocketRotationVelocity() const noexcept
    {
        return m_rotationVelocity;
    }
    /// Sets the rotation velocity. As in Java, the modification id stays.
    void setRocketRotationVelocity(const Coordinate& rotation) noexcept;

    /// Sets the effective launch rod length and draws a new modification id.
    void                 setEffectiveLaunchRodLength(double effectiveLaunchRodLength) noexcept;
    [[nodiscard]] double getEffectiveLaunchRodLength() const noexcept
    {
        return m_effectiveLaunchRodLength;
    }

    /// Sets the wall-clock time at which the simulation was started and draws a new
    /// modification id.
    void setSimulationStartWallTime(WallClock::time_point simulationStartWallTime) noexcept;
    [[nodiscard]] WallClock::time_point getSimulationStartWallTime() const noexcept
    {
        return m_simulationStartWallTime;
    }

    // ---------------------------------------------------------------------------- flags

    /// Each flag setter draws a new modification id (always).
    void setMotorIgnited(bool motorIgnited) noexcept;
    /// Whether a motor has ignited.
    [[nodiscard]] bool isMotorIgnited() const noexcept { return m_motorIgnited; }

    void setLiftoff(bool liftoff) noexcept;
    /// Whether the rocket has risen from the ground.
    [[nodiscard]] bool isLiftoff() const noexcept { return m_liftoff; }

    /// Sets the flag; clearing the rod (true) also starts the wait before warnings are recorded:
    /// recordWarnings() is false until kWarningsWait after the current simulation time.
    void setLaunchRodCleared(bool launchRod) noexcept;
    /// Whether the launch rod has been cleared.
    [[nodiscard]] bool isLaunchRodCleared() const noexcept { return m_launchRodCleared; }

    void setApogeeReached(bool apogeeReached) noexcept;
    /// Whether apogee has been detected.
    [[nodiscard]] bool isApogeeReached() const noexcept { return m_apogeeReached; }

    void setTumbling(bool tumbling) noexcept;
    /// Whether the rocket is tumbling.
    [[nodiscard]] bool isTumbling() const noexcept { return m_tumbling; }

    void setLanded(bool landed) noexcept;
    /// Whether the rocket has landed.
    [[nodiscard]] bool isLanded() const noexcept { return m_landed; }

    /// The maximum altitude so far; minus infinity in a new status and in a copy.
    [[nodiscard]] double getMaxAlt() const noexcept { return m_maxAlt; }
    /// Sets the maximum altitude and draws a new modification id.
    void setMaxAlt(double maxAlt) noexcept;

    /// The time of the maximum altitude; 0 in a new status and in a copy.
    [[nodiscard]] double getMaxAltTime() const noexcept { return m_maxAltTime; }
    /// Sets the time of the maximum altitude and draws a new modification id.
    void setMaxAltTime(double maxAltTime) noexcept;

    /// The maximum z velocity so far in the flight (raised by storeData()); minus infinity in a
    /// new status.
    [[nodiscard]] double getMaxZVelocity() const noexcept { return m_maxZVelocity; }

    /// The time from which recordWarnings() may be true: the recommended maximum simulation
    /// time until the launch rod is cleared (an addition: Java reads the field).
    [[nodiscard]] double getStartWarningsTime() const noexcept { return m_startWarningsTime; }

    /// Whether this branch simulates a stage that has been separated and left behind, rather
    /// than the rocket that flew on.
    [[nodiscard]] bool isSeparatedStage() const noexcept { return m_separatedStage; }
    /// Marks this branch as simulating a separated stage. The modification id stays.
    void setSeparatedStage(bool separatedStage) noexcept { m_separatedStage = separatedStage; }

    /// The deployed recovery devices: the set itself, to which the engine adds.
    [[nodiscard]] MonitorableSet<const RecoveryDevice*>& getDeployedRecoveryDevices() noexcept
    {
        return *m_deployedRecoveryDevices;
    }
    [[nodiscard]] const MonitorableSet<const RecoveryDevice*>& getDeployedRecoveryDevices()
        const noexcept
    {
        return *m_deployedRecoveryDevices;
    }

    /// The kinematic tumble detector of this branch, which carries its filter state across the
    /// steps.
    [[nodiscard]] TumbleDetector&       getTumbleDetector() noexcept { return *m_tumbleDetector; }
    [[nodiscard]] const TumbleDetector& getTumbleDetector() const noexcept
    {
        return *m_tumbleDetector;
    }

    // ------------------------------------------------------------------------- warnings

    /// Sets the warning set the warnings go to; null: none (addWarning() then makes a new one).
    /// The modification id stays.
    void setWarnings(std::shared_ptr<WarningSet> warnings);

    /// Adds @p warning to the warning set. When the set did not have a warning of that kind,
    /// a SIM_WARN event at the current simulation time, without a source, goes to the flight
    /// data branch; its data is the warning as the set stored it (FlightEvent::warningData()).
    /// A warning the set already has (which it may replace by @p warning, see MessageSet) adds
    /// no event.
    /// @throws BugError when an event is to be added and there is no flight data branch
    void addWarning(const Warning& warning);

    /// addWarning() for every warning of @p warnings, in order.
    void addWarnings(const WarningSet& warnings);

    /// The warning set, or null.
    [[nodiscard]] const std::shared_ptr<WarningSet>& getWarnings() const noexcept
    {
        return m_warnings;
    }

    /// Whether (most) flight event warnings are currently being saved: not before the launch
    /// rod is cleared, not until kWarningsWait after that, and not once the z velocity is below
    /// 20 % of the maximum z velocity so far.
    [[nodiscard]] bool recordWarnings() const noexcept;

    // --------------------------------------------------------------------------- events

    /// The queue of the flight events still to handle.
    [[nodiscard]] EventQueue&       getEventQueue() noexcept { return *m_eventQueue; }
    [[nodiscard]] const EventQueue& getEventQueue() const noexcept { return *m_eventQueue; }

    /// Adds @p event to the event queue unless a listener aborts adding it
    /// (SimulationListenerHelper::fireAddFlightEvent()).
    /// @throws SimulationException from a listener
    void addEvent(const FlightEvent& event);

    /// Aborts the current simulation branch: adds a SIM_ABORT event for @p cause at the current
    /// simulation time (addEvent(), so a listener may drop it).
    /// @throws SimulationException from a listener
    void abortSimulation(SimulationAbort::Cause cause);

    /// Removes from the event queue all the events that came from components which are no
    /// longer attached (isAttached()), without a new modification id of the queue
    /// (EventQueue::Iterator::remove()).
    void removeUnattachedEvents();

    /// Whether @p event came from a component that is still attached to the stages of this
    /// branch: an event without a source (pointer), from a component without a parent (the
    /// rocket), or from a component in an active stage of the configuration.
    [[nodiscard]] bool isAttached(const FlightEvent& event) const;

    // ----------------------------------------------------------------------- conditions

    /// Sets the simulation conditions (the modification id stays).
    void setSimulationConditions(std::shared_ptr<SimulationConditions> simulationConditions);

    /// The simulation conditions (null only after setSimulationConditions(nullptr)).
    [[nodiscard]] const std::shared_ptr<SimulationConditions>& getSimulationConditions()
        const noexcept
    {
        return m_simulationConditions;
    }

    // ----------------------------------------------------------------------- extra data

    /// Stores extra data for use by simulation listeners under @p key; see the class comment
    /// for what the copies of a status share.
    void putExtraData(const std::string& key, std::any value);

    /// The extra data stored under @p key, or null if nothing has been set for the key. The
    /// map is initially empty. The pointer is valid until the key is set again.
    [[nodiscard]] std::any*       getExtraData(const std::string& key);
    [[nodiscard]] const std::any* getExtraData(const std::string& key) const;

    // ---------------------------------------------------------------------------- debug

    /// The modification id (Java: getModID()); see the class comment.
    [[nodiscard]] ModId getModId() const noexcept { return m_modId; }
    /// getModId() (the Monitorable concept).
    [[nodiscard]] ModId modId() const noexcept { return m_modId; }

    /// One line per queued event, in the queue's array order: "      [t:<type> @<time>" with
    /// the type's display name and the time as Java prints a double, "  src:<name>" for an
    /// event with a source, "  data:<class>" for one with data (Java's simple class name:
    /// MotorClusterState, the warning's type name, SimulationAbort, Pair or String), and "]".
    [[nodiscard]] std::string toEventDebug() const;

    /// "MotorState list:" and one line "          [<MotorClusterState::toDescription()>]" per
    /// motor state.
    [[nodiscard]] std::string toMotorsDebug() const;

private:
    /// The key of the constructor that makes the shallow copy of clone().
    struct ShallowKey
    {
        explicit ShallowKey() = default;
    };

    SimulationStatus(ShallowKey key, const SimulationStatus& orig);

    /// Makes one motor state per motor of the configuration (Java: populateMotors()).
    void populateMotors();

    /// The effective launch rod length of the constructor (see the class comment).
    [[nodiscard]] double calculateEffectiveLaunchRodLength() const;

    /// Raises the maximum z velocity to @p zVel, with a new modification id when it rises.
    void setMaxZVelocity(double zVel) noexcept;

    std::shared_ptr<SimulationConditions> m_simulationConditions;
    std::shared_ptr<FlightConfiguration>  m_configuration;
    std::shared_ptr<FlightDataBranch>     m_flightDataBranch;

    double m_time{0};

    Coordinate      m_position;
    WorldCoordinate m_worldPosition{0, 0, 0};
    Coordinate      m_velocity;

    Quaternion m_orientation;
    Coordinate m_rotationVelocity;

    double m_maxZVelocity{-std::numeric_limits<double>::infinity()};
    double m_startWarningsTime{AbstractRkSimulationStepper::kRecommendedMaxTime};

    /** Kinematic tumble detector, carrying its filter state across steps. */
    std::shared_ptr<TumbleDetector> m_tumbleDetector{std::make_shared<TumbleDetector>()};

    /** Whether this branch simulates a stage that has been separated and left behind. */
    bool m_separatedStage{false};

    double m_effectiveLaunchRodLength{0};

    // Set of all motors
    std::shared_ptr<std::vector<std::shared_ptr<MotorClusterState>>> m_motorStateList{
        std::make_shared<std::vector<std::shared_ptr<MotorClusterState>>>()};

    /** Wall-clock time when the simulation was started. */
    WallClock::time_point m_simulationStartWallTime;

    /** Set to true when a motor has ignited. */
    bool m_motorIgnited{false};

    /** Set to true when the rocket has risen from the ground. */
    bool m_liftoff{false};

    /** Set to true when the launch rod has been cleared. */
    bool m_launchRodCleared{false};

    /** Set to true when apogee has been detected. */
    bool m_apogeeReached{false};

    /** Set to true to indicate the rocket is tumbling. */
    bool m_tumbling{false};

    /** Set to true to indicate rocket has landed */
    bool m_landed{false};

    /** Contains a list of deployed recovery devices. */
    std::shared_ptr<MonitorableSet<const RecoveryDevice*>> m_deployedRecoveryDevices{
        std::make_shared<MonitorableSet<const RecoveryDevice*>>()};

    /** The flight event queue */
    std::shared_ptr<EventQueue> m_eventQueue{std::make_shared<EventQueue>()};

    std::shared_ptr<WarningSet> m_warnings;

    /** Available for special purposes by the listeners. */
    std::shared_ptr<std::map<std::string, std::any>> m_extraData{
        std::make_shared<std::map<std::string, std::any>>()};

    double m_maxAlt{-std::numeric_limits<double>::infinity()};
    double m_maxAltTime{0};

    ModId m_modId{ModId::invalid()};
    /// Java's modIDadd: drawn when a shared object is replaced, and never read.
    ModId m_modIdAdd{ModId::invalid()};
};

}  // namespace QtRocket
