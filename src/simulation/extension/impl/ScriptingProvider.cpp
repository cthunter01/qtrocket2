#include "QtRocket/simulation/extension/impl/ScriptingProvider.h"

#include <memory>
#include <string>

#include "QtRocket/simulation/extension/AbstractSimulationExtensionProvider.h"
#include "QtRocket/simulation/extension/impl/ScriptingExtension.h"

namespace QtRocket
{

ScriptingProvider::ScriptingProvider()
  : AbstractSimulationExtensionProvider(std::string(ScriptingExtension::kId),
                                        [] { return std::make_unique<ScriptingExtension>(); },
                                        {"Scripts", "JavaScript"})
{
}

}  // namespace QtRocket
