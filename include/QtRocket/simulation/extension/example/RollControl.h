#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/simulation/extension/AbstractSimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"

namespace QtRocket
{

class FlightDataType;
class SimulationConditions;

/// An example extension that applies a PI controller to the cant of a fin set, to control the
/// roll rate of the rocket (OpenRocket's simulation/extension/example/RollControl). One note,
/// from OpenRocket: making aerodynamic changes during the simulation makes the simulation run
/// extremely slowly.
///
/// The configuration (the keys of the Config, which a .ork file stores, with the defaults a
/// getter returns while the key is absent or holds another type):
/// - "controlFinName" ("CONTROL"): the name of the fin set to turn;
/// - "startTime" (0.5 s): the controller is idle before this simulation time;
/// - "setPoint" (0 rad/s): the desired roll rate;
/// - "finRate" (10 degrees per second, in rad/s): the fastest the fins turn;
/// - "maxFinAngle" (15 degrees, in rad): the largest fin angle;
/// - "KP" (0.007) and "KI" (0.2): the gains of the controller.
///
/// The run: initialize() adds a listener to the conditions.
/// - When the simulation starts, it looks for the first fin set of that name among the active
///   components of the simulated rocket (the engine's private copy: the caller's rocket is never
///   touched) and remembers its cant; without such a fin set the simulation ends with the
///   SimulationException "A fin set with name '<name>' was not found".
/// - It takes the roll rate from every calculation of the flight conditions.
/// - After every step from the start time on: error = setPoint - rollRate; the integral of the
///   error grows by error * (the time since the step before); the wanted fin angle is
///   KP * error + KI * integral; the fin angle moves towards it by at most finRate * (that
///   time), is limited to +-maxFinAngle, is set as the cant of the fin set and written to the
///   flight data column finCantType() of the row the step began with. The fin angle starts at
///   0, not at the cant the fin set has, so the first controlled step turns a canted fin set
///   back towards 0.
/// - When the simulation ends, the fin set gets the cant it had at the start.
/// As in Java the listener is cloned with the status of the simulation (see SimulationListener,
/// "Clones"), and every clone points at the same fin set: after a stage separation the listener
/// of the sustainer's branch and the one of the dropped stage's branch each go on with the
/// controller state of the separation and turn that one fin set, whichever stage it is on, and
/// each branch gets the column. The nested simulation that finds the optimum coast time runs
/// without the listener (the engine drops the listeners that are not system listeners there).
///
/// Deviations from OpenRocket:
/// - getFlightDataTypes() returns the type once. Java keeps the types in a static list to which
///   every constructed RollControl adds the type again, so its list grows with every extension
///   ever made in the process; the one reader of the list (the document) makes a set of it.
/// - The listener takes the seven settings when initialize() makes it; Java's listener is an
///   inner class that reads them from the extension at every step. They cannot change during
///   a run of Simulation::simulate(), and a listener that outlives its extension stays valid.
/// - Java prints "Attempting to set angle ... clamping." to the standard error stream whenever
///   the fin angle is limited; that is not ported.
/// - The constructor is public (Java: package-private, for the provider's injector).
class RollControl final : public AbstractSimulationExtension
{
public:
    /// OpenRocket's class name: the id a .ork file names the extension by.
    static constexpr std::string_view kId =
        "info.openrocket.core.simulation.extension.example.RollControl";

    /// The name and the symbol of the flight data type of the fin cant. The symbol is a small
    /// alpha (U+03B1, here as its two UTF-8 bytes) followed by "fc".
    static constexpr std::string_view kFinCantTypeName = "Control fin cant";
    static constexpr std::string_view kFinCantTypeSymbol =
        "\xCE\xB1"
        "fc";

    /// An extension with an empty configuration: every getter returns its default.
    RollControl();

    /// The flight data type the fin cant is saved as (Java: FIN_CANT_TYPE): the type
    /// FlightDataType::getType() gave for kFinCantTypeName, kFinCantTypeSymbol and the angle
    /// unit group when it was first asked, which the constructor does (Java: when the class is
    /// loaded). The same object from then on, also when a later getType() with that symbol
    /// replaced it in the registry.
    [[nodiscard]] static const FlightDataType& finCantType();

    /// Adds the roll control listener to @p conditions.
    void initialize(SimulationConditions& conditions) override;

    /// "Roll Control".
    [[nodiscard]] std::string getName() const override;

    /// OpenRocket's description (it says "PID"; the controller has no derivative term).
    [[nodiscard]] std::optional<std::string> getDescription() const override;

    /// finCantType(), once (see the class comment).
    [[nodiscard]] std::vector<const FlightDataType*> getFlightDataTypes() const override;

    [[nodiscard]] std::unique_ptr<SimulationExtension> clone() const override;

    /// The name of the fin set to turn.
    [[nodiscard]] std::string getControlFinName() const;
    void                      setControlFinName(std::string_view name);

    /// The simulation time from which the controller acts, s.
    [[nodiscard]] double getStartTime() const;
    void                 setStartTime(double startTime);

    /// The desired roll rate, rad/s.
    [[nodiscard]] double getSetPoint() const;
    void                 setSetPoint(double rollRate);

    /// The maximum turn rate of the control fins, rad/s.
    [[nodiscard]] double getFinRate() const;
    void                 setFinRate(double finRate);

    /// The maximum angle of the control fins, rad.
    [[nodiscard]] double getMaxFinAngle() const;
    void                 setMaxFinAngle(double maxFin);

    /// The proportional gain.
    [[nodiscard]] double getKP() const;
    void                 setKP(double kp);

    /// The integral gain. Each setter stores its value in the configuration and emits changed()
    /// (always).
    [[nodiscard]] double getKI() const;
    void                 setKI(double ki);
};

}  // namespace QtRocket
