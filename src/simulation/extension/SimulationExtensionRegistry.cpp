#include "QtRocket/simulation/extension/SimulationExtensionRegistry.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtensionProvider.h"
#include "QtRocket/simulation/extension/example/AirStartProvider.h"
#include "QtRocket/simulation/extension/example/RollControlProvider.h"
#include "QtRocket/simulation/extension/impl/JavaCodeProvider.h"
#include "QtRocket/simulation/extension/impl/ScriptingProvider.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

SimulationExtensionRegistry SimulationExtensionRegistry::bundled()
{
    SimulationExtensionRegistry registry;
    registry.add(std::make_unique<AirStartProvider>());
    registry.add(std::make_unique<RollControlProvider>());
    registry.add(std::make_unique<JavaCodeProvider>());
    registry.add(std::make_unique<ScriptingProvider>());
    return registry;
}

void SimulationExtensionRegistry::add(std::unique_ptr<SimulationExtensionProvider> provider)
{
    if (provider == nullptr)
    {
        bug("A simulation extension registry cannot hold a null provider");
    }
    m_providers.push_back(std::move(provider));
}

std::vector<const SimulationExtensionProvider*> SimulationExtensionRegistry::getProviders() const
{
    std::vector<const SimulationExtensionProvider*> providers;
    providers.reserve(m_providers.size());
    for (const std::unique_ptr<SimulationExtensionProvider>& provider : m_providers)
    {
        providers.push_back(provider.get());
    }
    return providers;
}

std::unique_ptr<SimulationExtension> SimulationExtensionRegistry::create(std::string_view id) const
{
    std::unique_ptr<SimulationExtension> extension;
    for (const std::unique_ptr<SimulationExtensionProvider>& p : m_providers)
    {
        const std::vector<std::string> ids = p->getIds();
        if (std::ranges::any_of(ids, [id](const std::string& known) { return known == id; }))
        {
            extension = p->getInstance(id);
        }
    }
    return extension;
}

}  // namespace QtRocket
