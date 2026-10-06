#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "QtRocket/mass/MassCalculator.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/simulation/AbstractRkSimulationStepper.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/WorldCoordinate.h"

namespace QtRocket
{

class AerodynamicCalculator;
class AtmosphericModel;
class GravityModel;
class Rocket;
class Simulation;
class SimulationListener;
class WindModel;

/// The conditions of a simulation (OpenRocket's simulation/SimulationConditions): what does not
/// change during the flight of a rocket, such as the launch rod, the launch site, the
/// atmospheric, gravity and wind models, the aerodynamic calculator, the stepper's limits, the
/// recovery warning thresholds, the random seed and the simulation listeners.
/// SimulationOptions::toSimulationConditions() makes one for a run; the rocket and the flight
/// configuration id come from the Simulation the conditions belong to (setSimulation()).
///
/// Ownership and sharing: the models, the calculator and the listeners are held by
/// std::shared_ptr, because Java shares them by reference: clone() gives conditions that use
/// the same wind model (with its random state), the same atmospheric and gravity models and the
/// same aerodynamic calculator (with its caches), and a clone() of each listener. A simulation
/// status holds its conditions by std::shared_ptr too, and its copy constructor clones them, so
/// that most hooks of a run are called on clones of the listeners (see SimulationListener,
/// "Clones"). The listener list holds the caller's own objects: what is added to it is used as
/// it is, and cloned only when the conditions are. The Simulation is a non-owning pointer: it
/// must outlive the conditions and every clone of them.
///
/// Monitoring (Java: Monitorable; the class satisfies the Monitorable concept): the
/// modification id is redrawn exactly where Java redraws it: by every setter of a plain value
/// (always, except that setLaunchSite(), setLaunchPosition(), setLaunchVelocity() and
/// setGeodeticComputation() do nothing for an equal value) and by setGravityModel(), but not
/// by the setters of the wind model, the atmospheric model, the aerodynamic calculator and the
/// mass calculator, nor by setSimulation() or a change of the listener list. Those four setters
/// draw a second id (Java: modIDadd) when they replace an object, which, as in Java, nothing
/// reads.
///
/// Threads: conditions belong to the one simulation run they were made for. Two runs on two
/// threads use two sets of conditions, which share nothing mutable when they come from two
/// toSimulationConditions() calls (the standard ISA atmospheric model, which every simulation
/// shares, is immutable).
///
/// Deviations from OpenRocket:
/// - The mass calculator: MassCalculator is a class of static functions here, so there is no
///   calculator object to hold. What is left of the field is whether one was set: an optional
///   of the empty class. As in Java nothing reads it.
/// - The atmospheric and gravity models are pointers to const: their calculations are const
///   (see AtmosphericModel and GravityModel).
/// - getRocket(), getMotorConfigurationId() and getFlightConfigurationId() throw BugError
///   without a simulation (Java: NullPointerException), and so does clone() for a null listener
///   in the list.
/// - There is no copy constructor: a copy is made on purpose, by clone(). Moving is allowed.
/// - setGeodeticComputation()'s null check has nothing to check.
class SimulationConditions
{
public:
    /// Conditions with Java's defaults: a launch rod of 1 m, vertical, towards the north (angle
    /// and direction 0), the launch site at latitude 0, longitude 0 and sea level, the launch
    /// position and velocity zero, the SPHERICAL geodetic computation, no models and no
    /// calculators, the recommended time step, maximum time and maximum angle step
    /// (AbstractRkSimulationStepper), the recovery thresholds 20, 3.048, 30.48 and 15.24 m/s, no
    /// listener, the random seed 0, no simulation, and ModId::invalid() as the modification id.
    SimulationConditions() = default;

    SimulationConditions& operator=(const SimulationConditions&)     = delete;
    SimulationConditions(SimulationConditions&&) noexcept            = default;
    SimulationConditions& operator=(SimulationConditions&&) noexcept = default;
    ~SimulationConditions()                                          = default;

    // ---------------------------------------------------------------------- calculators

    /// The aerodynamic calculator, or null while none is set.
    [[nodiscard]] const std::shared_ptr<AerodynamicCalculator>& getAerodynamicCalculator()
        const noexcept
    {
        return m_aerodynamicCalculator;
    }
    /// Sets the aerodynamic calculator (the modification id stays, see the class comment).
    void setAerodynamicCalculator(std::shared_ptr<AerodynamicCalculator> aerodynamicCalculator);

    /// The mass calculator: whether one was set (see the class comment).
    [[nodiscard]] const std::optional<MassCalculator>& getMassCalculator() const noexcept
    {
        return m_massCalculator;
    }
    /// Sets the mass calculator; nullopt for Java's null (the modification id stays).
    void setMassCalculator(std::optional<MassCalculator> massCalculator);

    // ----------------------------------------------------------- through the simulation

    /// The rocket of the simulation (Simulation::getRocket()).
    /// @throws BugError without a simulation
    [[nodiscard]] Rocket& getRocket() const;

    /// The flight configuration id of the simulation (Java: getMotorConfigurationID(), the same
    /// as getFlightConfigurationID()).
    /// @throws BugError without a simulation
    [[nodiscard]] FlightConfigurationId getMotorConfigurationId() const;

    /// The flight configuration id of the simulation (Simulation::getId()).
    /// @throws BugError without a simulation
    [[nodiscard]] FlightConfigurationId getFlightConfigurationId() const;

    /// Sets the parent simulation (non-owning; null: none). The modification id stays.
    void setSimulation(Simulation* simulation) noexcept { m_simulation = simulation; }

    /// The parent simulation, or null.
    [[nodiscard]] Simulation* getSimulation() const noexcept { return m_simulation; }

    // ----------------------------------------------------------------------- launch rod

    [[nodiscard]] double getLaunchRodLength() const noexcept { return m_launchRodLength; }
    void                 setLaunchRodLength(double launchRodLength) noexcept;

    /// The launch rod angle, rad from vertical.
    [[nodiscard]] double getLaunchRodAngle() const noexcept { return m_launchRodAngle; }
    void                 setLaunchRodAngle(double launchRodAngle) noexcept;

    /// The launch rod direction, rad; 0 = north.
    [[nodiscard]] double getLaunchRodDirection() const noexcept { return m_launchRodDirection; }
    void                 setLaunchRodDirection(double launchRodDirection) noexcept;

    // ---------------------------------------------------------------------- launch site

    /// The launch site (latitude, longitude, altitude).
    [[nodiscard]] const WorldCoordinate& getLaunchSite() const noexcept { return m_launchSite; }
    /// Nothing happens when @p site equals the stored site (WorldCoordinate's tolerant
    /// equality).
    void setLaunchSite(const WorldCoordinate& site) noexcept;

    /// The launch position in simulation coordinates (normally zero; an air start overrides
    /// it).
    [[nodiscard]] const Coordinate& getLaunchPosition() const noexcept { return m_launchPosition; }
    /// Nothing happens when @p launchPosition equals the stored one (Coordinate's tolerant
    /// equality).
    void setLaunchPosition(const Coordinate& launchPosition) noexcept;

    [[nodiscard]] const Coordinate& getLaunchVelocity() const noexcept { return m_launchVelocity; }
    /// Nothing happens when @p launchVelocity equals the stored one.
    void setLaunchVelocity(const Coordinate& launchVelocity) noexcept;

    [[nodiscard]] GeodeticComputationStrategy getGeodeticComputation() const noexcept
    {
        return m_geodeticComputation;
    }
    /// Nothing happens when @p geodeticComputation is the stored strategy.
    void setGeodeticComputation(GeodeticComputationStrategy geodeticComputation) noexcept;

    // --------------------------------------------------------------------------- models

    /// The wind model, or null while none is set. The simulation draws its turbulence from it:
    /// the conditions and their clones share its random state.
    [[nodiscard]] const std::shared_ptr<WindModel>& getWindModel() const noexcept
    {
        return m_windModel;
    }
    /// Sets the wind model (the modification id stays).
    void setWindModel(std::shared_ptr<WindModel> windModel);

    /// The atmospheric model, or null while none is set.
    [[nodiscard]] const std::shared_ptr<const AtmosphericModel>& getAtmosphericModel()
        const noexcept
    {
        return m_atmosphericModel;
    }
    /// Sets the atmospheric model (the modification id stays).
    void setAtmosphericModel(std::shared_ptr<const AtmosphericModel> atmosphericModel);

    /// The gravity model, or null while none is set.
    [[nodiscard]] const std::shared_ptr<const GravityModel>& getGravityModel() const noexcept
    {
        return m_gravityModel;
    }
    /// Sets the gravity model and draws a new modification id (always).
    void setGravityModel(std::shared_ptr<const GravityModel> gravityModel);

    // ------------------------------------------------------------------- stepper limits

    [[nodiscard]] double getTimeStep() const noexcept { return m_timeStep; }
    void                 setTimeStep(double timeStep) noexcept;

    [[nodiscard]] double getMaxSimulationTime() const noexcept { return m_maxSimulationTime; }
    void                 setMaxSimulationTime(double maxSimulationTime) noexcept;

    [[nodiscard]] double getMaximumAngleStep() const noexcept { return m_maximumAngleStep; }
    void                 setMaximumAngleStep(double maximumAngle) noexcept;

    // -------------------------------------------------------------- recovery thresholds

    /// A design without a drogue: warn when a recovery device deploys above this speed, m/s.
    [[nodiscard]] double getRecoverySpeedWarning() const noexcept { return m_recoverySpeedWarning; }
    void                 setRecoverySpeedWarning(double recoverySpeedWarning) noexcept;

    /// A design with a drogue: warn when the drogue deploys below this speed at apogee, m/s.
    [[nodiscard]] double getDrogueLowSpeedWarning() const noexcept
    {
        return m_drogueLowSpeedWarning;
    }
    void setDrogueLowSpeedWarning(double drogueLowSpeedWarning) noexcept;

    /// A design with a drogue: warn when the main deploys above this speed, m/s.
    [[nodiscard]] double getRecoveryDrogueMainHighSpeedWarning() const noexcept
    {
        return m_recoveryDrogueMainHighSpeedWarning;
    }
    void setRecoveryDrogueMainHighSpeedWarning(double recoveryDrogueMainHighSpeedWarning) noexcept;

    /// A design with a drogue: warn when the main deploys below this speed, m/s.
    [[nodiscard]] double getRecoveryDrogueMainLowSpeedWarning() const noexcept
    {
        return m_recoveryDrogueMainLowSpeedWarning;
    }
    void setRecoveryDrogueMainLowSpeedWarning(double recoveryDrogueMainLowSpeedWarning) noexcept;

    // ------------------------------------------------------------------------- the rest

    /// The seed of the simulation's random sources (a 32-bit int, as in Java).
    [[nodiscard]] int getRandomSeed() const noexcept { return m_randomSeed; }
    void              setRandomSeed(int randomSeed) noexcept;

    /// The simulation listeners: the list itself, which the caller changes (Java hands out its
    /// list). The hooks are called in the order of the list. Changing the list draws no
    /// modification id.
    [[nodiscard]] std::vector<std::shared_ptr<SimulationListener>>&
    getSimulationListenerList() noexcept
    {
        return m_simulationListeners;
    }
    [[nodiscard]] const std::vector<std::shared_ptr<SimulationListener>>&
    getSimulationListenerList() const noexcept
    {
        return m_simulationListeners;
    }

    /// The modification id (Java: getModID()); see the class comment.
    [[nodiscard]] ModId getModId() const noexcept { return m_modId; }
    /// getModId() (the Monitorable concept).
    [[nodiscard]] ModId modId() const noexcept { return m_modId; }

    /// Java's clone(): conditions with the same values, the same simulation, the same models
    /// and calculators (shared, see the class comment), the same modification id, and a list
    /// with a clone() of every listener, in order.
    /// @throws BugError when the list holds a null listener
    [[nodiscard]] SimulationConditions clone() const;

private:
    /// The member-wise copy clone() starts from (Java: super.clone()).
    SimulationConditions(const SimulationConditions&) = default;

    Simulation* m_simulation{nullptr};  // The parent simulation

    double m_launchRodLength{1};

    /** Launch rod angle >= 0, radians from vertical */
    double m_launchRodAngle{0};

    /** Launch rod direction, 0 = north */
    double m_launchRodDirection{0};

    // Launch site location (lat, lon, alt)
    WorldCoordinate m_launchSite{0, 0, 0};

    // Launch location in simulation coordinates (normally always 0, air-start would override
    // this)
    Coordinate m_launchPosition{Coordinate::kNul};

    Coordinate m_launchVelocity{Coordinate::kNul};

    GeodeticComputationStrategy m_geodeticComputation{GeodeticComputationStrategy::SPHERICAL};

    std::shared_ptr<WindModel>              m_windModel;
    std::shared_ptr<const AtmosphericModel> m_atmosphericModel;
    std::shared_ptr<const GravityModel>     m_gravityModel;

    std::shared_ptr<AerodynamicCalculator> m_aerodynamicCalculator;
    std::optional<MassCalculator>          m_massCalculator;

    double m_timeStep{AbstractRkSimulationStepper::kRecommendedTimeStep};
    double m_maxSimulationTime{AbstractRkSimulationStepper::kRecommendedMaxTime};
    double m_maximumAngleStep{AbstractRkSimulationStepper::kRecommendedAngleStep};

    double m_recoverySpeedWarning{20.0};
    double m_drogueLowSpeedWarning{3.048};
    double m_recoveryDrogueMainHighSpeedWarning{30.48};
    double m_recoveryDrogueMainLowSpeedWarning{15.24};

    std::vector<std::shared_ptr<SimulationListener>> m_simulationListeners;

    int m_randomSeed{0};

    ModId m_modId{ModId::invalid()};
    /// Java's modIDadd: drawn when a model or calculator is replaced, and never read.
    ModId m_modIdAdd{ModId::invalid()};
};

}  // namespace QtRocket
