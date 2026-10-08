#pragma once

#include "QtRocket/simulation/extension/AbstractSimulationExtensionProvider.h"

namespace QtRocket
{

/// The provider of the ScriptingExtension (OpenRocket's
/// simulation/extension/impl/ScriptingProvider): its id is ScriptingExtension::kId, and its menu
/// name {"Scripts", "JavaScript"}. The extension it makes never runs a script (see
/// ScriptingExtension).
class ScriptingProvider final : public AbstractSimulationExtensionProvider
{
public:
    ScriptingProvider();
};

}  // namespace QtRocket
