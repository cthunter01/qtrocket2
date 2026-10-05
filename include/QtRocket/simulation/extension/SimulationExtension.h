#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "QtRocket/util/Config.h"

namespace QtRocket
{

class FlightDataType;
class OpenRocketDocument;
class Simulation;
class SimulationConditions;
class WarningSet;

/// Something a user attaches to a simulation to change how it runs (OpenRocket's
/// simulation/extension/SimulationExtension): before a run, initialize() may modify the
/// simulation conditions or add simulation listeners to them. A .ork file stores an extension as
/// its id and its configuration (a Config).
///
/// Ownership: a simulation holds its extensions through std::shared_ptr, because OpenRocket
/// shares them by reference: document/Simulation's copyExtensionsFrom() and its constructor put
/// the very same extension objects into a second list, getSimulationExtensions() hands out the
/// simulation's own list, and the equality of two simulations first asks whether two extensions
/// are the same object. clone() and SimulationExtensionProvider::getInstance(), which make a
/// new extension, return a std::unique_ptr, which converts to the shared pointer of a list.
///
/// SimulationConditions, Simulation and OpenRocketDocument are only declared here; later tiers
/// define them. initialize() may throw a SimulationException (Java declares it), the one
/// exception family that crosses the simulation loop.
///
/// Deviation: getDescription() returns nullopt where Java returns null.
class SimulationExtension
{
public:
    virtual ~SimulationExtension() = default;

    /// Whether this extension is safe to execute repeatedly and concurrently as part of a Monte
    /// Carlo analysis. A safe extension must confine mutations to the copied simulation and must
    /// not perform external side effects such as file, console, network, or user-interface
    /// output. Extensions default to unsafe, so custom code must explicitly opt in after
    /// verifying those guarantees.
    [[nodiscard]] virtual bool isMonteCarloSafe() const { return false; }

    /// The id that identifies this extension when it is stored to a file.
    [[nodiscard]] virtual std::string getId() const = 0;

    /// A short description of this extension to be shown in the UI. The name may contain
    /// elements from the extension's configuration, for example "Air start (150m)".
    [[nodiscard]] virtual std::string getName() const = 0;

    /// A longer description text for this extension, which may be shown in the UI as extra
    /// information about it, or nullopt if not available.
    [[nodiscard]] virtual std::optional<std::string> getDescription() const = 0;

    /// Called once for each simulation this extension is attached to when loading a document.
    /// This may perform necessary changes to the document at load time; @p warnings are the
    /// document loading warnings.
    virtual void documentLoaded(OpenRocketDocument& document, Simulation& simulation,
                                WarningSet& warnings) = 0;

    /// Initializes this extension for running within a simulation. Called before running a
    /// simulation; it can either modify the simulation conditions or add simulation listeners
    /// to them.
    /// @throws SimulationException when the extension cannot run
    virtual void initialize(SimulationConditions& conditions) = 0;

    /// The flight data types this extension creates: only new types, not existing types that
    /// the extension adds to the flight data. (A FlightDataType is a process-wide object, so
    /// the pointers are never null and never dangle.)
    [[nodiscard]] virtual std::vector<const FlightDataType*> getFlightDataTypes() const = 0;

    /// A copy of this extension, with all configuration deep-copied.
    [[nodiscard]] virtual std::unique_ptr<SimulationExtension> clone() const = 0;

    /// The current configuration of this extension. The extension may keep its configuration in
    /// a Config, or create one when asked.
    [[nodiscard]] virtual Config getConfig() const = 0;

    /// Sets this extension's configuration. The extension should load all its configuration
    /// from @p config.
    virtual void setConfig(const Config& config) = 0;

protected:
    SimulationExtension()                                      = default;
    SimulationExtension(const SimulationExtension&)            = default;
    SimulationExtension& operator=(const SimulationExtension&) = default;
    SimulationExtension(SimulationExtension&&)                 = default;
    SimulationExtension& operator=(SimulationExtension&&)      = default;
};

}  // namespace QtRocket
