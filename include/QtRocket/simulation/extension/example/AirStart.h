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

class SimulationConditions;

/// Starts the simulation in the air (OpenRocket's simulation/extension/example/AirStart): when
/// the simulation starts, the rocket is put at the launch altitude above the launch site, with
/// the launch velocity along its axis as the launch rod points.
///
/// The configuration (the keys of the Config, with the defaults a getter returns while the key
/// is absent or holds another type): "launchAltitude" (100 m) and "launchVelocity" (50 m/s).
///
/// initialize() adds a listener whose one hook, at the start of the simulation, sets the
/// position of the rocket to (0, 0, launchAltitude) and its velocity to (0, 0, launchVelocity)
/// turned by the orientation of the rocket. Since that changes the status, the simulation gets
/// the warning Warning::kListenersAffected (a listener that is not a system listener affected
/// it).
///
/// The name holds the configuration: "Air-start (<altitude>, <velocity>)" while the launch
/// velocity is above 0.01 m/s, else "Air-start (<altitude>)" (the texts of
/// SimulationExtension.airstart.name.altvel and .alt), each value as the default unit of its
/// unit group prints it with its unit (UnitGroup::toStringUnit()): "Air-start (100 m, 50 m/s)".
///
/// Deviations from OpenRocket:
/// - The listener takes the two settings when initialize() makes it; Java's listener is an
///   inner class that reads them from the extension when the simulation starts.
/// - The constructor is public (Java: implicit, for the provider's injector).
/// - getInputNumbers() lists the two numbers of the configuration, so that
///   Simulation::simulate() refuses a run in which one of them is a NaN or an infinity
///   (Simulation::validateInputs(), QtRocket's own). OpenRocket ends in a BugException for a
///   launch altitude that is no number and for any such launch velocity, and in a
///   SimulationException ("Simulation values exceeded limits") for an infinite launch altitude.
///   Its reader of .ork files cannot give a setting such a value, except an infinity for an
///   integer too large for a double.
class AirStart final : public AbstractSimulationExtension
{
public:
    /// OpenRocket's class name: the id a .ork file names the extension by.
    static constexpr std::string_view kId =
        "info.openrocket.core.simulation.extension.example.AirStart";

    /// An extension with an empty configuration: every getter returns its default.
    AirStart();

    /// Always true: the listener changes the simulated status only.
    [[nodiscard]] bool isMonteCarloSafe() const override;

    /// Adds the air start listener to @p conditions.
    void initialize(SimulationConditions& conditions) override;

    /// The name with the altitude and the velocity (see the class comment).
    [[nodiscard]] std::string getName() const override;

    /// "Start simulation with a configurable altitude and velocity".
    [[nodiscard]] std::optional<std::string> getDescription() const override;

    /// The two numbers the listener takes: "the 'launchAltitude' of the simulation extension
    /// 'Air-start'" and "the 'launchVelocity' ..." (see the class comment, "Deviations").
    [[nodiscard]] std::vector<InputNumber> getInputNumbers() const override;

    [[nodiscard]] std::unique_ptr<SimulationExtension> clone() const override;

    /// The altitude above the launch site at which the simulation starts, m.
    [[nodiscard]] double getLaunchAltitude() const;
    void                 setLaunchAltitude(double launchAltitude);

    /// The speed along the rocket's axis at which the simulation starts, m/s. Each setter stores
    /// its value in the configuration and emits changed() (always).
    [[nodiscard]] double getLaunchVelocity() const;
    void                 setLaunchVelocity(double launchVelocity);
};

}  // namespace QtRocket
