#pragma once

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtensionProvider.h"

namespace QtRocket
{

/// The simulation extension providers a program knows: what the .ork reader asks for the
/// extension a file names, and what a GUI builds its "add extension" menu from.
///
/// It has no counterpart in OpenRocket, which asks its injector for the set of providers (a
/// Guice multibinding that its plugin system fills with every class annotated @Plugin:
/// plugin/PluginModule). QtRocket has no global application object, so whoever needs the
/// providers is handed a registry: there is no global instance. bundled() makes the registry of
/// the extensions QtRocket ships.
///
/// The registry owns its providers and keeps them in the order they were added, which is the
/// order getProviders() lists them in and the order create() asks them in.
///
/// Threads: the const members only read, so one registry may be used from several threads at
/// once while none of them adds a provider (the providers of QtRocket's own extensions have no
/// state that a call changes).
///
/// Copying: the providers are owned, so the registry is moved, not copied.
class SimulationExtensionRegistry
{
public:
    /// A registry without providers: create() finds nothing.
    SimulationExtensionRegistry() = default;

    SimulationExtensionRegistry(const SimulationExtensionRegistry&)                = delete;
    SimulationExtensionRegistry& operator=(const SimulationExtensionRegistry&)     = delete;
    SimulationExtensionRegistry(SimulationExtensionRegistry&&) noexcept            = default;
    SimulationExtensionRegistry& operator=(SimulationExtensionRegistry&&) noexcept = default;
    ~SimulationExtensionRegistry()                                                 = default;

    /// The registry of the extensions QtRocket ships, in the order OpenRocket's plugin system
    /// lists the same four: AirStartProvider, RollControlProvider, JavaCodeProvider and
    /// ScriptingProvider. (The last two make extensions that never execute anything: see
    /// JavaCode and ScriptingExtension.)
    [[nodiscard]] static SimulationExtensionRegistry bundled();

    /// Adds @p provider after the ones the registry has.
    /// @throws BugError when @p provider is null
    void add(std::unique_ptr<SimulationExtensionProvider> provider);

    /// The providers, in the order they were added. The pointers are never null and stay valid
    /// while the registry lives (also when it is moved).
    [[nodiscard]] std::vector<const SimulationExtensionProvider*> getProviders() const;

    /// The number of providers.
    [[nodiscard]] std::size_t size() const noexcept { return m_providers.size(); }

    /// Whether there is no provider.
    [[nodiscard]] bool empty() const noexcept { return m_providers.empty(); }

    /// A new extension for the id @p id, or null when no provider knows the id: the instance of
    /// the provider whose getIds() contains @p id. This is the loop of OpenRocket's .ork reader
    /// (importt/SingleSimulationHandler), which asks every provider in turn: when several
    /// providers know the id, each of them is asked for an instance (getInstance(@p id)) and
    /// the instance of the last one is returned. A provider that knows the id and returns null
    /// counts as that: a later null replaces an earlier instance, as in Java.
    ///
    /// The id is compared as it is: the reader replaces "net.sf.openrocket" by
    /// "info.openrocket.core" in the id of an old file before it asks.
    [[nodiscard]] std::unique_ptr<SimulationExtension> create(std::string_view id) const;

private:
    std::vector<std::unique_ptr<SimulationExtensionProvider>> m_providers;
};

}  // namespace QtRocket
