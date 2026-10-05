#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/simulation/extension/SimulationExtension.h"

namespace QtRocket
{

/// Makes simulation extensions by id (OpenRocket's
/// simulation/extension/SimulationExtensionProvider): what the .ork reader asks for the extension a
/// file names, and what the GUI lists in its "add extension" menu.
///
/// Not ported: the discovery of providers. OpenRocket collects every provider through its
/// plugin system (the @Plugin annotation and a Guice multibinding); here the tier that loads
/// .ork files keeps the registry of providers.
///
/// Deviation: getName() returns nullopt where Java returns null.
class SimulationExtensionProvider
{
public:
    virtual ~SimulationExtensionProvider() = default;

    /// The simulation extension ids that this provider supports. The id is used to identify the
    /// extension when storing files. It should follow the conventions of Java package and class
    /// naming.
    [[nodiscard]] virtual std::vector<std::string> getIds() const = 0;

    /// The UI name for the extension @p id. The first values are nested menus, with the last
    /// one the actual entry, for example {"Launch conditions", "Air-start"}.
    ///
    /// If the id does not represent an extension that should be displayed in the UI, this
    /// returns nullopt. For example, if an extension has multiple ids, this must return the
    /// menu name for only one of them.
    [[nodiscard]] virtual std::optional<std::vector<std::string>> getName(
        std::string_view id) const = 0;

    /// A new instance of the simulation extension @p id, with some default configuration.
    [[nodiscard]] virtual std::unique_ptr<SimulationExtension> getInstance(
        std::string_view id) const = 0;

protected:
    SimulationExtensionProvider()                                              = default;
    SimulationExtensionProvider(const SimulationExtensionProvider&)            = default;
    SimulationExtensionProvider& operator=(const SimulationExtensionProvider&) = default;
    SimulationExtensionProvider(SimulationExtensionProvider&&)                 = default;
    SimulationExtensionProvider& operator=(SimulationExtensionProvider&&)      = default;
};

}  // namespace QtRocket
