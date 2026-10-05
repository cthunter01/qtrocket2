#pragma once

#include <optional>
#include <string>
#include <vector>

#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{

class FlightDataType;
class OpenRocketDocument;
class Simulation;
class WarningSet;

/// An abstract implementation of a SimulationExtension (OpenRocket's
/// simulation/extension/AbstractSimulationExtension): it keeps the configuration in a Config,
/// gives every optional method its default, and is a change source (Java: AbstractChangeSource;
/// addChangeListener()/removeChangeListener() are changed().connect()/disconnect()). A subclass
/// implements initialize() and clone(), and usually getName().
///
/// The id: Java's getId() is the canonical name of the extension's class, found by reflection,
/// and a .ork file names the extension by it, for example
/// "info.openrocket.core.simulation.extension.example.AirStart". C++ has no such reflection, so
/// the id is a constructor argument. A port of an OpenRocket extension must pass OpenRocket's
/// class name, or files written by either program would not find the extension in the other.
///
/// Deviations from OpenRocket:
/// - clone() stays abstract. Java clones with Object.clone(), which copies whatever the subclass
///   is; here the copy constructor copies this class's part (the id, the name and the
///   configuration), and a subclass writes
///   `return std::make_unique<MyExtension>(*this);`.
/// - A copy has no connections to changed(). In Java the copy made by Object.clone() shares the
///   original's listener list (the list is a final field that the shallow copy does not
///   duplicate), so a listener of either hears both; nothing in OpenRocket relies on that.
/// - The injected Translator (the protected field trans) is not ported: QtRocket has no
///   translation layer, and a subclass writes its texts in English.
/// - Only the protected fireChangeEvent() is ported, not AbstractChangeSource's public
///   fireChangeEvent(Object source): changed() carries no source.
class AbstractSimulationExtension : public SimulationExtension
{
public:
    ~AbstractSimulationExtension() override = default;

    AbstractSimulationExtension& operator=(const AbstractSimulationExtension&) = delete;
    AbstractSimulationExtension(AbstractSimulationExtension&&)                 = delete;
    AbstractSimulationExtension& operator=(AbstractSimulationExtension&&)      = delete;

    /// The id given to the constructor (see the class comment).
    [[nodiscard]] std::string getId() const override;

    /// By default, the name given to the constructor.
    [[nodiscard]] std::string getName() const override;

    /// By default, nullopt.
    [[nodiscard]] std::optional<std::string> getDescription() const override;

    /// By default, an empty list.
    [[nodiscard]] std::vector<const FlightDataType*> getFlightDataTypes() const override;

    /// By default, does nothing.
    void documentLoaded(OpenRocketDocument& document, Simulation& simulation,
                        WarningSet& warnings) override;

    /// A copy of the configuration.
    [[nodiscard]] Config getConfig() const override;

    /// Replaces the configuration with a copy of @p config and emits changed() (always).
    void setConfig(const Config& config) override;

    /// Emitted when the configuration changes (ChangeSource).
    [[nodiscard]] Signal<>& changed() noexcept { return m_changed; }

protected:
    /// An extension with the id @p id whose name is the last part of the id, as Java's
    /// no-argument constructor names the extension after its class (getSimpleName()): the id
    /// "info.openrocket.core.simulation.extension.example.AirStart" gives "AirStart". A
    /// subclass that uses this constructor should override getName().
    explicit AbstractSimulationExtension(std::string id);

    /// An extension with the id @p id and the fixed name @p name.
    AbstractSimulationExtension(std::string id, std::string name);

    /// Copies the id, the name and the configuration, not the connections to changed() (see
    /// the class comment); for a subclass's clone().
    AbstractSimulationExtension(const AbstractSimulationExtension& other);

    /// Emits changed() (Java's fireChangeEvent()): a subclass calls it from every setter that
    /// changes its configuration.
    void fireChangeEvent() const { m_changed.emit(); }

    /// The configuration (Java's protected field config), which a subclass reads and writes
    /// directly.
    Config m_config;

private:
    std::string m_id;
    std::string m_name;
    Signal<>    m_changed;
};

}  // namespace QtRocket
