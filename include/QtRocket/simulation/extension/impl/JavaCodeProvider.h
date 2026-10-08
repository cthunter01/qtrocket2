#pragma once

#include "QtRocket/simulation/extension/AbstractSimulationExtensionProvider.h"

namespace QtRocket
{

/// The provider of the JavaCode extension (OpenRocket's
/// simulation/extension/impl/JavaCodeProvider): its id is JavaCode::kId, and its menu name
/// {"Scripts", "Java listeners"}. The extension it makes never runs any code (see JavaCode).
class JavaCodeProvider final : public AbstractSimulationExtensionProvider
{
public:
    JavaCodeProvider();
};

}  // namespace QtRocket
