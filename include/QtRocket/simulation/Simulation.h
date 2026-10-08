#pragma once

#include <array>
#include <cstddef>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{

class FlightConfiguration;
class FlightData;
class FlightDataType;
class OpenRocketDocument;
class Preferences;
class Rocket;
class SimulationExtension;
class SimulationListener;
class WarningSet;

/// A simulation of a rocket: its flight configuration, its options, its extensions and the data
/// of its last run (OpenRocket's document/Simulation).
///
/// Placement: OpenRocket keeps Simulation (and PlotAppearance) in its document package. They
/// live in simulation/ here, because SimulationConditions and the simulation engine need the
/// Simulation, and document/ builds on simulation/, never the reverse (MotorClusterState, which
/// OpenRocket keeps in simulation and which lives in mass/ here, is the precedent). The
/// document is only declared here, and a Simulation never calls it.
///
/// The document and changed(): Java's Simulation adds its document as a change listener of
/// itself in its constructors (addChangeListener(document)) and never takes it away. Here it
/// is the document that listens: OpenRocketDocument connects itself to changed() when a
/// simulation enters its list of simulations, and disconnects when the simulation leaves the
/// list. What differs from Java through that:
/// - a simulation made with a document but not yet added to it does not tell the document of
///   its changes (in Java such a change already marks the document as not saved);
/// - a simulation made without a document and then added to one is heard by that document (in
///   Java it never is);
/// - a simulation that was removed from its document is no longer heard (in Java it still is).
///
/// The rocket: the caller owns it. A Simulation refers to its rocket without owning it, so the
/// rocket must outlive the simulation and every copy(), clone() and cloneForUndo() of it. The
/// one exception is duplicateForIndependentSimulation(), whose duplicate owns the private copy
/// of the rocket it was made with (and so do its own copies). setFlightConfigurationId()
/// creates the configuration in the rocket when the rocket lacks it, and
/// duplicateForIndependentSimulation() copies the rocket: both belong to the thread that owns
/// the rocket.
///
/// Preferences: OpenRocket reads the global application preferences and asks its injector for
/// the default simulation options and the RocketDescriptor. Here the preferences are passed in:
/// - a constructor without preferences takes the built-in default options, SimulationOptions(),
///   which are not what OpenRocket gives a new simulation: there the launch conditions of the
///   preferences are applied, and those of an empty preference store include a wind of 2 m/s
///   (see DefaultSimulationOptionFactory). SimulationOptions() is calm.
/// - a constructor with a Preferences& does what Java does with that store: the options are
///   SimulationOptions(preferences) with copyConditionsFrom(DefaultSimulationOptionFactory(
///   preferences).getDefault()).
/// The simulation keeps a pointer to the store (it must outlive the simulation and its copies):
/// it gives the name of the flight configuration that simulate() records (the default
/// configuration name and the motor naming), and the default options of duplicateSimulation().
/// Without a store both use the built-in defaults. Every copy and duplicate of a simulation,
/// duplicateForIndependentSimulation() included, refers to the same store, and so do their
/// options (which write the stepper method they are given to it), as every simulation of
/// OpenRocket reads and writes the one application preferences. The store is safe to use from
/// several threads (see Preferences, "Threads").
///
/// The status: getStatus() works the status out from what it finds (see there); it is not a
/// plain getter. The options may be changed freely (getOptions()): a change emits changed(),
/// and the next getStatus() of a simulation whose data was up to date finds it OUTDATED.
///
/// Change events (Java: ChangeSource; addChangeListener()/removeChangeListener() are
/// changed().connect()/disconnect()): changed() is emitted by setFlightConfigurationId() and
/// setName() when they change something, by syncModId(), setPlotAppearance() and simulate()
/// always, and whenever the options emit theirs (Java: the ConditionListener), except for the
/// options of a copy() or a cloneForUndo(), which Java leaves without that listener.
///
/// Copies, with exactly what Java's Object.clone() shares: a copy(), a clone() and a
/// cloneForUndo() refer to the same rocket and document, share the plot appearances with the
/// original (one map: an appearance set through either shows in both; Java's shallow clone
/// copies the reference to the map) and have copies of the options and clones of the
/// extensions; none of them has the original's connections to changed(). See each for the
/// rest. duplicateSimulation() and duplicateForIndependentSimulation() make a new simulation
/// with a plot appearance map of its own.
///
/// Threads: a Simulation is not thread-safe (Java enforces single-threaded access with a
/// SafetyMutex, which is not ported). Simulations of one design that must run at the same time
/// each run on a duplicateForIndependentSimulation(). What such duplicates still share with
/// the simulation they were made from, and with each other, is the preference store, which is
/// thread-safe: the thread that owns the store may write to it while they run.
///
/// Deviations from OpenRocket:
/// - simulate() returns a Result where Java throws: see there.
/// - simulate() refuses inputs that are not finite before anything runs (validateInputs()),
///   which OpenRocket does not: there such a value ends in a BugException.
/// - The simulated data is held by std::shared_ptr (Java: a reference), because
///   cloneForUndo() and loadFrom() share it; null while there is none. The simulated conditions
///   and the configuration description are null/nullopt while the simulation has not run.
/// - Java's fields simulationEngineClass, simulationStepperClass, aerodynamicCalculatorClass and
///   massCalculatorClass, which nothing ever changes, are not ported; equals() compared them.
/// - The landing dispersion settings (getLandingDispersionSettings() and its setter, and their
///   part of equals() and of the copying methods) are not ported: HOOK(monte-carlo).
/// - hasErrors(branch) throws BugError without simulated data (Java: NullPointerException).
/// - The plot appearances are kept in a std::map, so getPlotAppearances() is ordered by symbol
///   (Java: a HashMap).
/// - Not copyable and not movable: the simulation conditions of a run and the options' change
///   listener point at it. The copying methods return a std::unique_ptr.
class Simulation
{
    /// The key of the constructor the copying methods call through std::make_unique; only
    /// Simulation can make one.
    struct CloneKey
    {
        explicit CloneKey() = default;
    };

public:
    /// The status of a simulation (Java: Simulation.Status), in Java's order.
    enum class Status
    {
        UPTODATE,       ///< up to date
        LOADED,         ///< loaded from a file, status probably up to date
        OUTDATED,       ///< data outdated
        EXTERNAL,       ///< imported external data
        NOT_SIMULATED,  ///< not yet simulated
        CANT_RUN,       ///< cannot be simulated: no motors, or no valid configuration id
        ABORTED,        ///< aborted when last run
    };

    /// Status.values(), in declaration order.
    static constexpr std::array<Status, 7> kAllStatuses{
        Status::UPTODATE,      Status::LOADED,   Status::OUTDATED, Status::EXTERNAL,
        Status::NOT_SIMULATED, Status::CANT_RUN, Status::ABORTED};

    /// A new simulation of @p rocket with the built-in default options (see the class comment):
    /// not simulated, without a name, for the rocket's selected flight configuration.
    explicit Simulation(Rocket& rocket);

    /// A new simulation of @p rocket with the default options of @p preferences (see the class
    /// comment). @p preferences must outlive the simulation and its copies.
    Simulation(Rocket& rocket, Preferences& preferences);

    /// A new simulation of @p rocket that belongs to @p document (null: none). The document is
    /// only remembered (getDocument()): it starts to listen to changed() when the simulation is
    /// added to it (see the class comment; Java: addChangeListener(document) here).
    Simulation(OpenRocketDocument* document, Rocket& rocket);
    Simulation(OpenRocketDocument* document, Rocket& rocket, Preferences& preferences);

    /// A simulation as a file describes it (Java: the constructor OpenRocket's loader uses):
    /// with @p status, @p name, @p options (moved in, with their connections; a copy of them
    /// becomes the simulated conditions), @p extensions (the same objects, in order), the
    /// simulated @p data (null: none) and, of @p plotAppearances, those that are not empty. The
    /// flight configuration is the rocket's selected one, whose modification id becomes the
    /// simulated one. @p preferences as in the class comment (null: the built-in defaults).
    Simulation(OpenRocketDocument* document, Rocket& rocket, Status status, std::string name,
               SimulationOptions                                 options,
               std::vector<std::shared_ptr<SimulationExtension>> extensions,
               std::shared_ptr<FlightData>                       data,
               const std::map<std::string, PlotAppearance>&      plotAppearances = {},
               Preferences*                                      preferences     = nullptr);

    /// The copy the copying methods start from (cloneForUndo()'s); only Simulation can call it.
    Simulation(CloneKey key, const Simulation& other);

    Simulation(const Simulation&)            = delete;
    Simulation& operator=(const Simulation&) = delete;
    Simulation(Simulation&&)                 = delete;
    Simulation& operator=(Simulation&&)      = delete;
    ~Simulation();

    // ------------------------------------------------------------------- configuration

    /// The flight configuration of this simulation's id in the rocket (the rocket's default
    /// configuration for an id the rocket does not have).
    [[nodiscard]] FlightConfiguration&       getActiveConfiguration();
    [[nodiscard]] const FlightConfiguration& getActiveConfiguration() const;

    /// The rocket of the simulation.
    [[nodiscard]] Rocket&       getRocket() noexcept { return *m_rocket; }
    [[nodiscard]] const Rocket& getRocket() const noexcept { return *m_rocket; }

    /// The document the simulation belongs to, or null.
    [[nodiscard]] OpenRocketDocument* getDocument() const noexcept { return m_document; }

    /// The preferences given to the constructor, or null.
    [[nodiscard]] Preferences* getPreferences() const noexcept { return m_preferences; }

    /// The id of the flight configuration that is simulated (the error id until one is set).
    [[nodiscard]] const FlightConfigurationId& getFlightConfigurationId() const noexcept
    {
        return m_configId;
    }
    /// getFlightConfigurationId().
    [[nodiscard]] const FlightConfigurationId& getId() const noexcept { return m_configId; }

    /// Sets the flight configuration id. A configuration that the rocket does not have yet is
    /// created in it first. Emits changed() when the id changes.
    /// @throws BugError for the error id (Java: IllegalArgumentException)
    void setFlightConfigurationId(const FlightConfigurationId& fcid);

    // ------------------------------------------------------------- options, extensions

    /// Applies the conditions of @p options to this simulation's options
    /// (SimulationOptions::copyConditionsFrom()).
    void copySimulationOptionsFrom(const SimulationOptions& options);

    /// The options of the simulation. They may be changed freely; the status follows (see the
    /// class comment).
    [[nodiscard]] SimulationOptions&       getOptions() noexcept { return m_options; }
    [[nodiscard]] const SimulationOptions& getOptions() const noexcept { return m_options; }

    /// The simulation extensions: the list itself, which the caller changes (Java hands out its
    /// list). simulate() initialises them in the order of the list.
    [[nodiscard]] std::vector<std::shared_ptr<SimulationExtension>>&
    getSimulationExtensions() noexcept
    {
        return m_simulationExtensions;
    }
    [[nodiscard]] const std::vector<std::shared_ptr<SimulationExtension>>& getSimulationExtensions()
        const noexcept
    {
        return m_simulationExtensions;
    }

    /// Replaces the extensions by @p extensions: the same objects, in order (they are then
    /// shared with whoever holds them, as in Java). Given this simulation's own list, the list
    /// ends up empty, as in Java (it is cleared before it is read).
    void copyExtensionsFrom(const std::vector<std::shared_ptr<SimulationExtension>>& extensions);

    // ----------------------------------------------------------------------------- name

    /// The user-defined name of the simulation.
    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    /// Sets the name; emits changed() when it changes.
    void setName(std::string_view name);

    // --------------------------------------------------------------------------- status

    /// Works out the status and returns it. In this order:
    /// - a status whose data is up to date (isStatusUpToDate()) becomes OUTDATED when the
    ///   modification id of the simulated flight configuration (a clone of the rocket's
    ///   configuration of getId(), as in Java) is not the simulated one, or the options are
    ///   not equal to the simulated conditions (SimulationOptions::operator==; never equal
    ///   without simulated conditions);
    /// - with the error id as the configuration id the status becomes CANT_RUN and is
    ///   returned;
    /// - a configuration without motors makes it CANT_RUN;
    /// - simulated data with a SIM_ABORT event (hasErrors()) makes it ABORTED.
    /// The status found is stored: it is the starting point of the next call.
    Status getStatus();

    /// The status as it is stored, without working it out anew (an addition: Java reads the
    /// field).
    [[nodiscard]] Status getStoredStatus() const noexcept { return m_status; }

    /// getDescription(getStatus(), *this): a longer, more user-friendly description of the
    /// status.
    [[nodiscard]] std::string getStatusDescription();

    /// Whether a branch of the simulated data has a SIM_ABORT event; false without data.
    [[nodiscard]] bool hasErrors() const;

    /// Whether branch @p branch of the simulated data has a SIM_ABORT event.
    /// @throws BugError without simulated data, or for a branch the data does not have
    [[nodiscard]] bool hasErrors(std::size_t branch) const;

    /// Whether @p status says that the data of a simulation is up to date: UPTODATE, LOADED or
    /// EXTERNAL.
    [[nodiscard]] static constexpr bool isStatusUpToDate(Status status) noexcept
    {
        return status == Status::UPTODATE || status == Status::LOADED || status == Status::EXTERNAL;
    }

    /// Takes the modification id of the flight configuration as the simulated one, and emits
    /// changed() (Java: syncModID()).
    void syncModId();

    // ------------------------------------------------------------------------- simulate

    /// Whether every number a run reads is finite. Fails with ErrorCode::INVALID_ARGUMENT for
    /// a NaN or an infinity, with a text that names the first such value, for example "Cannot
    /// simulate: the launch rod length is not finite (NaN)." simulate() calls this before
    /// anything runs; a caller may ask beforehand. The validation is QtRocket's own: OpenRocket
    /// has none (see simulate(), "Inputs that are not finite").
    ///
    /// Checked, in this order: every number SimulationOptions::toSimulationConditions() reads
    /// from the options (the steppers and the engine get nothing else of them but choices, such
    /// as the stepper method, and the random seed), and then every delay and altitude the engine
    /// reads from the flight configuration that is simulated:
    /// - the launch rod length and the launch rod angle;
    /// - the launch latitude, longitude and altitude;
    /// - of the wind model in use (SimulationOptions::getWindModelType()): the speed, the
    ///   standard deviation and the direction of the average wind (its turbulence intensity is
    ///   the quotient of the first two), or the altitude, the speed, the direction and the
    ///   standard deviation of every level of the multi-level wind;
    /// - the launch rod direction as the run uses it (SimulationOptions::
    ///   getLaunchRodDirection(): the wind direction when launching into the wind);
    /// - without the ISA atmosphere: the launch temperature, pressure and relative humidity;
    /// - with the constant gravity model: the constant gravity;
    /// - of the stability lookup table and of the drag lookup table, when the options have one:
    ///   every Mach number, angle of attack and coefficient of the table
    ///   (MachAoALookup::findNonFinite(); which rows a flight interpolates in is not known
    ///   beforehand, and a table read from CSV can hold "NaN");
    /// - the time step, the maximum simulation time and the maximum step angle;
    /// - the four recovery speed warning thresholds;
    /// - of the motor of every active motor mount that has one in the configuration: the
    ///   ignition delay, and the ejection delay, for which Motor::kPluggedDelay (+infinity: a
    ///   plugged motor, without an ejection charge) is a valid value as well;
    /// - of every active recovery device: the deployment delay and the deployment altitude;
    /// - of every active stage but stage 0, which has nothing above it to separate from: the
    ///   separation delay and the separation altitude.
    /// Not checked: what the run does not read (the wind model that is not in use, the launch
    /// temperature, pressure and humidity under the ISA atmosphere, the constant gravity under
    /// the WGS model, the stored rod direction of a launch into the wind, the components of
    /// stages that are not active, the motors and the settings of other flight configurations),
    /// the dimensions and masses of the design and the thrust curves of its motors, and whether
    /// a finite value is a sensible one: a finite value that the atmospheric model refuses is
    /// toSimulationConditions()'s error, and one that is merely absurd is flown as it is (a
    /// wind of 1e300 m/s overflows on the way and still ends in a BugError).
    [[nodiscard]] Result<void> validateInputs() const;

    /// Simulates the flight (Java: simulate(SimulationListener...)).
    ///
    /// The simulated data is dropped first. Then, in Java's order: a simulation whose status is
    /// EXTERNAL cannot be simulated; the inputs are validated (validateInputs(), a step
    /// OpenRocket does not have); the options are turned into simulation conditions
    /// (SimulationOptions::toSimulationConditions()) that belong to this simulation; every
    /// extension is initialised with them (SimulationExtension::initialize()), in order;
    /// @p additionalListeners are appended to the conditions' listener list, in order (the very
    /// objects: see SimulationListener, "Clones", for what they see of the run); and the engine
    /// runs.
    ///
    /// Whatever happens after the data was dropped, also on a failure (Java: the finally block):
    /// the simulated conditions become a copy of the options, the configuration description
    /// the name of the flight configuration, the simulated modification id that of the flight
    /// configuration, the simulated data what the engine produced (null when it did not get to
    /// run), the status UPTODATE, and changed() is emitted. So a failed run leaves the
    /// simulation UPTODATE with the data it got to, as in Java; getStatus() makes that ABORTED
    /// when the data holds a SIM_ABORT event.
    ///
    /// Deviation from OpenRocket: Java throws; here simulate() is the boundary at which the
    /// SimulationException family is caught, and the failure is the Error of the Result:
    /// - ErrorCode::CANCELLED for a SimulationCancelledException (an InterruptListener whose
    ///   stop was requested), with the exception's message;
    /// - ErrorCode::SIMULATION_ABORTED for any other SimulationException (an imported
    ///   simulation, an extension that cannot run, a listener's exception, a calculation that
    ///   went wrong), with the exception's message;
    /// - toSimulationConditions()'s Error when the atmospheric model refuses the launch
    ///   conditions (Java: an IllegalArgumentException that leaves simulate());
    /// - ErrorCode::INVALID_ARGUMENT when an input is not finite, with validateInputs()'s text,
    ///   which names the value. Nothing has run then: neither an extension nor the engine was
    ///   asked, and the bookkeeping above is that of an imported simulation (no simulated data,
    ///   the status UPTODATE, changed() emitted).
    /// A simulation that aborts (a SIM_ABORT event) is not a failure: simulate() succeeds and
    /// getStatus() is ABORTED. A BugError passes through, after the bookkeeping above.
    ///
    /// Inputs that are not finite: OpenRocket's simulate() does not validate the options or the
    /// design, and a NaN or an infinity among them surfaces where the computation first meets
    /// it, mostly as a BugException, some of them in the middle of the flight. A BugError
    /// means a defect of the program, and these values come from outside it, so simulate()
    /// refuses them before anything runs (see validateInputs() for the list). The .ork reader
    /// does its part earlier: it does not apply a NaN or an infinity that a file holds for a
    /// simulation option or for a delay or altitude of the design (it adds a warning, and the
    /// value stays what it was), so that a loaded simulation passes the validation; the plugged
    /// ejection delay ("none" in a file) is a value like any other there.
    [[nodiscard]] Result<void> simulate(
        std::span<const std::shared_ptr<SimulationListener>> additionalListeners = {});

    /// The same with the listeners written in place: simulate({listener}).
    [[nodiscard]] Result<void> simulate(
        std::initializer_list<std::shared_ptr<SimulationListener>> additionalListeners);

    /// simulate() with an InterruptListener for @p stopToken after @p additionalListeners (an
    /// addition: Java's callers add InterruptListener.INSTANCE themselves, which watches the
    /// thread's interrupt flag): a stop requested through the token's source ends the run with
    /// ErrorCode::CANCELLED after the step that is being taken.
    [[nodiscard]] Result<void> simulate(
        const std::stop_token&                               stopToken,
        std::span<const std::shared_ptr<SimulationListener>> additionalListeners = {});

    // ------------------------------------------------------------ what was simulated

    /// The conditions used in the previous simulation, or null if the simulation has not been
    /// run.
    [[nodiscard]] SimulationOptions* getSimulatedConditions() noexcept
    {
        return m_simulatedConditions.has_value() ? &*m_simulatedConditions : nullptr;
    }
    [[nodiscard]] const SimulationOptions* getSimulatedConditions() const noexcept
    {
        return m_simulatedConditions.has_value() ? &*m_simulatedConditions : nullptr;
    }

    /// The warnings of the previous simulation: the warning set of the simulated data; null
    /// without data.
    [[nodiscard]] WarningSet*       getSimulatedWarnings() noexcept;
    [[nodiscard]] const WarningSet* getSimulatedWarnings() const noexcept;

    /// The description of the flight configuration of the previous simulation (its name, with
    /// the motors substituted); nullopt if the simulation has not been run.
    [[nodiscard]] const std::optional<std::string>& getSimulatedConfigurationDescription()
        const noexcept
    {
        return m_simulatedConfigurationDescription;
    }

    /// The flight data of the previous simulation, or null.
    [[nodiscard]] const std::shared_ptr<FlightData>& getSimulatedData() const noexcept
    {
        return m_simulatedData;
    }

    /// The modification id of the flight configuration when it was simulated; ModId::invalid()
    /// when it was not (an addition: Java reads the field).
    [[nodiscard]] ModId getSimulatedConfigurationModId() const noexcept
    {
        return m_simulatedConfigurationModId;
    }

    /// Whether the simulation has plottable flight data: data with at least one branch.
    [[nodiscard]] bool hasSimulationData() const noexcept;

    /// Whether the simulation has summary flight data: any data.
    [[nodiscard]] bool hasSummaryData() const noexcept { return m_simulatedData != nullptr; }

    // -------------------------------------------------------------------------- copying

    /// A copy for cut, copy and paste: the same rocket, document, name, configuration id and
    /// plot appearances (shared, see the class comment), copies of the options and clones of
    /// the extensions, and nothing simulated: the status NOT_SIMULATED, no simulated
    /// conditions, description or data. As in Java, a change of the copy's options does not
    /// emit the copy's changed().
    [[nodiscard]] std::unique_ptr<Simulation> copy() const;

    /// clone(true).
    [[nodiscard]] std::unique_ptr<Simulation> clone() const;

    /// A copy with everything: as copy(), plus the status, the simulated conditions (a copy),
    /// the description, the simulated modification id and, with @p includeSimulatedData, a
    /// clone of the simulated data (FlightData::clone(); null otherwise). A change of the
    /// clone's options emits the clone's changed().
    [[nodiscard]] std::unique_ptr<Simulation> clone(bool includeSimulatedData) const;

    /// A copy for the undo history of a document: as clone(), but the simulated data is shared
    /// with this simulation, not cloned, and, as in Java, a change of the copy's options does
    /// not emit the copy's changed(). (Package-private in Java, for OpenRocketDocument.)
    [[nodiscard]] std::unique_ptr<Simulation> cloneForUndo() const;

    /// Loads @p simulation into this simulation: its name, configuration id, description and
    /// simulated modification id; the conditions of its options (copyConditionsFrom(), which
    /// emits changed() when they differ); its simulated conditions (none, a copy, or their
    /// conditions when this simulation has some already); its extensions (the same objects);
    /// its status and its simulated data (shared). When the loaded status says that the data
    /// is up to date and the configuration id is valid, the simulated modification id becomes
    /// that of this simulation's flight configuration. The plot appearances stay, as in Java.
    void loadFrom(const Simulation& simulation);

    /// A new simulation of @p newRocket in non-simulated state with this simulation's document,
    /// preferences, name, configuration id (as it is: the configuration is not created in
    /// @p newRocket), conditions (copyConditionsFrom() onto the new simulation's default
    /// options), configuration description, clones of the extensions and copies of the plot
    /// appearances. @p newRocket must outlive the duplicate.
    [[nodiscard]] std::unique_ptr<Simulation> duplicateSimulation(Rocket& newRocket) const;

    /// A deep copy of this simulation for a run of its own: a not yet simulated simulation of a
    /// copy of the rocket (Rocket::copyRocketWithOriginalId()), which the duplicate owns,
    /// without a document, with the name, the configuration id, a full copy of the options and
    /// clones of the extensions. It may run on another thread without touching this simulation
    /// or its rocket; making it reads this simulation and its rocket, so it belongs to their
    /// thread. The one thing the duplicate shares with this simulation is the preference store
    /// (Java: the application preferences): simulate() reads the naming of the flight
    /// configuration from it on the thread it runs on, and a stepper method set in the
    /// duplicate's options is written to it. The store is thread-safe (see Preferences,
    /// "Threads"), so it may be written while duplicates run. The plot appearances are not
    /// copied, as in Java.
    [[nodiscard]] std::unique_ptr<Simulation> duplicateForIndependentSimulation() const;

    // ---------------------------------------------------------------------- change events

    /// Emitted when the simulation changes (ChangeSource; see the class comment).
    [[nodiscard]] Signal<>& changed() noexcept { return m_changed; }

    // ------------------------------------------------------------------------- equality

    /// Java's equals(): the same simulation, or the same name, the same configuration id, equal
    /// options (SimulationOptions::operator==, so never for two independently made simulations,
    /// whose wind models differ in their seeds), equal plot appearances, and extension lists of
    /// the same length whose extensions are pairwise the same object or have the same id and the
    /// same configuration entries (Config::sameEntries()). The status and everything simulated
    /// do not count.
    [[nodiscard]] bool operator==(const Simulation& other) const;

    /// Java's hashCode(): always 0.
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static): Java's instance method
    [[nodiscard]] int hashCode() const noexcept { return 0; }

    // ----------------------------------------------------------------- plot appearances

    /// The plot appearance stored for @p type (a copy), or nullopt: also for a type whose
    /// symbol is empty or blank.
    [[nodiscard]] std::optional<PlotAppearance> getPlotAppearance(const FlightDataType& type) const;

    /// Stores @p appearance for @p type, by the type's symbol; nullopt or an empty appearance
    /// removes the stored one. Emits changed() (always), unless the type's symbol is empty or
    /// blank: nothing happens then.
    void setPlotAppearance(const FlightDataType&                type,
                           const std::optional<PlotAppearance>& appearance);

    /// A copy of the plot appearances, by symbol.
    [[nodiscard]] std::map<std::string, PlotAppearance> getPlotAppearances() const;

protected:
    /// Emits changed().
    void fireChangeEvent() const { m_changed.emit(); }

private:
    /// What the constructors of a new simulation share: the flight configuration id of the
    /// rocket's selected configuration, and the options' change listener.
    void initialize();

    /// Makes a change of the options emit changed() (Java: the ConditionListener).
    void connectConditionListener();

    /// The part of simulate() that Java's try block holds: everything up to and including the
    /// engine's run. @p flightData receives what the engine produced.
    [[nodiscard]] Result<void> runSimulation(
        std::span<const std::shared_ptr<SimulationListener>> additionalListeners,
        std::shared_ptr<FlightData>&                         flightData);

    /// Java's finally block of simulate(): records what was simulated (see simulate()).
    void recordSimulation(std::shared_ptr<FlightData> flightData);

    /// The preferences the configuration description is made with: those given to the
    /// constructor, or built-in defaults.
    [[nodiscard]] std::string describeConfiguration() const;

    /// Java's simulationExtensionsEqual().
    [[nodiscard]] static bool simulationExtensionsEqual(
        const std::vector<std::shared_ptr<SimulationExtension>>& a,
        const std::vector<std::shared_ptr<SimulationExtension>>& b);

    /// Java's setPlotAppearancesInternal(): replaces the plot appearances by the ones of
    /// @p appearances that are not empty.
    void setPlotAppearancesInternal(const std::map<std::string, PlotAppearance>& appearances,
                                    bool                                         notify);

    OpenRocketDocument* m_document{nullptr};
    Rocket*             m_rocket;
    /// The rocket when this simulation owns it (duplicateForIndependentSimulation()), else
    /// null.
    std::shared_ptr<Rocket> m_ownedRocket;
    Preferences*            m_preferences{nullptr};
    FlightConfigurationId   m_configId{FlightConfigurationId::errorId()};

    std::string m_name;

    Status m_status;

    /// The change signal; declared before the options, whose slot emits it.
    Signal<> m_changed;

    /** The conditions to use */
    SimulationOptions m_options;

    // HOOK(monte-carlo): Java keeps the optional landing dispersion settings (MonteCarloSettings)
    // here, with a getter, a setter that fires a change, their part of equals() and of copy(),
    // clone(), cloneForUndo(), loadFrom(), duplicateSimulation() and
    // duplicateForIndependentSimulation().

    std::vector<std::shared_ptr<SimulationExtension>> m_simulationExtensions;

    /** The conditions actually used in the previous simulation, or nullopt */
    std::optional<SimulationOptions> m_simulatedConditions;
    std::optional<std::string>       m_simulatedConfigurationDescription;
    std::shared_ptr<FlightData>      m_simulatedData;
    ModId                            m_simulatedConfigurationModId{ModId::invalid()};
    /// Optional per-series plot overrides, stored by FlightDataType symbol for stable
    /// persistence. Shared with the copies Java makes with Object.clone() (see the class
    /// comment).
    std::shared_ptr<std::map<std::string, PlotAppearance>> m_plotAppearances{
        std::make_shared<std::map<std::string, PlotAppearance>>()};
};

/// The constant's name, e.g. "NOT_SIMULATED" (Java: Status.name()).
[[nodiscard]] std::string_view name(Simulation::Status status) noexcept;

/// The English name of the status (Java: Status.toString(), the text of
/// Simulation.Status.<name>), e.g. "Not Simulated Yet". ABORTED keeps OpenRocket's markup,
/// "<i><b>ABORTED</b></i>".
[[nodiscard]] std::string_view displayName(Simulation::Status status) noexcept;

/// The English description of the status (the text of Simulation.Status.Description.<name>),
/// with OpenRocket's markup, e.g. "<i>Up to date</i>".
[[nodiscard]] std::string_view description(Simulation::Status status) noexcept;

/// Java's Status.getDescription(sim): a longer, more user-friendly description. It is
/// description(@p status), unless @p simulation's status (getStatus(), which is worked out
/// here) is ABORTED and its data has SIM_ABORT events: then, for every branch that has one,
/// description(@p status) followed by "<i>: ", the text of the first abort of the branch and
/// "</i><br>" (more than one branch can abort).
[[nodiscard]] std::string getDescription(Simulation::Status status, Simulation& simulation);

}  // namespace QtRocket
