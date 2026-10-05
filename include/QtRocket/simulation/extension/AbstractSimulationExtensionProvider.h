#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtensionProvider.h"

namespace QtRocket
{

/// An abstract implementation of a SimulationExtensionProvider for one extension (OpenRocket's
/// simulation/extension/AbstractSimulationExtensionProvider). Java's constructor takes the class
/// of the extension and its menu name: the id is the class's canonical name, and getInstance()
/// asks the Guice injector for an instance of the class.
///
/// Here the constructor takes the id (for a port of an OpenRocket extension, OpenRocket's class
/// name: see AbstractSimulationExtension), a factory function in place of the injector, and the
/// menu name. As in Java, the class is meant to be derived from (the constructor is protected),
/// one provider class per extension:
///
///     class AirStartProvider final : public AbstractSimulationExtensionProvider
///     {
///     public:
///         AirStartProvider()
///           : AbstractSimulationExtensionProvider(
///                 "info.openrocket.core.simulation.extension.example.AirStart",
///                 [] { return std::make_unique<AirStart>(); }, {"Launch conditions", "Air-start"})
///         {
///         }
///     };
///
/// Java's class is abstract. Here every member has a default, so the class is kept from being
/// used on its own by its protected constructors: only a subclass can make or copy one (a copy
/// made of the base part alone would lose the subclass's overrides), and there is no assignment.
///
/// Not ported: the injected Translator (the protected field trans).
class AbstractSimulationExtensionProvider : public SimulationExtensionProvider
{
public:
    /// Makes a new extension with its default configuration; never null.
    using Factory = std::function<std::unique_ptr<SimulationExtension>()>;

    ~AbstractSimulationExtensionProvider() override = default;

    AbstractSimulationExtensionProvider& operator=(const AbstractSimulationExtensionProvider&) =
        delete;
    AbstractSimulationExtensionProvider& operator=(AbstractSimulationExtensionProvider&&) = delete;

    /// By default, the one id given to the constructor.
    [[nodiscard]] std::vector<std::string> getIds() const override;

    /// By default, the name given to the constructor for the first id that getIds() returns,
    /// and nullopt for any other id.
    /// @throws BugError when a subclass's getIds() returns no id (Java:
    ///         IndexOutOfBoundsException)
    [[nodiscard]] std::optional<std::vector<std::string>> getName(
        std::string_view id) const override;

    /// By default, a new extension from the factory, whatever @p id is (as in Java, which
    /// instantiates the extension class without looking at the id).
    /// @throws BugError when the factory returns null
    [[nodiscard]] std::unique_ptr<SimulationExtension> getInstance(
        std::string_view id) const override;

protected:
    /// Sole constructor: the id of the extension, the function that makes one, and the name
    /// getName() returns (the menus first, the entry last).
    /// @throws BugError when @p factory is empty
    AbstractSimulationExtensionProvider(std::string extensionId, Factory factory,
                                        std::vector<std::string> name);

    /// For a subclass's own copy and move (see the class comment).
    AbstractSimulationExtensionProvider(const AbstractSimulationExtensionProvider&) = default;
    AbstractSimulationExtensionProvider(AbstractSimulationExtensionProvider&&)      = default;

    /// The id of the extension (Java: the protected field extensionClass, as its name).
    [[nodiscard]] const std::string& getExtensionId() const noexcept { return m_extensionId; }

private:
    std::string              m_extensionId;
    Factory                  m_factory;
    std::vector<std::string> m_name;
};

}  // namespace QtRocket
