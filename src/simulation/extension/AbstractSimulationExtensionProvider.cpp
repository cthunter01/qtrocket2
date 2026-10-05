#include "QtRocket/simulation/extension/AbstractSimulationExtensionProvider.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

AbstractSimulationExtensionProvider::AbstractSimulationExtensionProvider(
    std::string extensionId, Factory factory, std::vector<std::string> name)
  : m_extensionId(std::move(extensionId)), m_factory(std::move(factory)), m_name(std::move(name))
{
    QTROCKET_ASSERT(m_factory != nullptr);
}

std::vector<std::string> AbstractSimulationExtensionProvider::getIds() const
{
    return {m_extensionId};
}

std::optional<std::vector<std::string>> AbstractSimulationExtensionProvider::getName(
    std::string_view id) const
{
    // getIds() is virtual: a subclass with several ids names only its first one.
    const std::vector<std::string> ids = getIds();
    QTROCKET_ASSERT(!ids.empty());
    if (id == ids[0])
    {
        return m_name;
    }
    return std::nullopt;
}

std::unique_ptr<SimulationExtension> AbstractSimulationExtensionProvider::getInstance(
    std::string_view /*id*/) const
{
    std::unique_ptr<SimulationExtension> extension = m_factory();
    QTROCKET_ASSERT(extension != nullptr);
    return extension;
}

}  // namespace QtRocket
