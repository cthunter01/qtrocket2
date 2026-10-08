#pragma once

#include "QtRocket/simulation/extension/AbstractSimulationExtensionProvider.h"

namespace QtRocket
{

/// The provider of the RollControl extension (OpenRocket's
/// simulation/extension/example/RollControlProvider): its id is RollControl::kId, and its menu
/// name {"Control Enhancements", "Roll Control"}.
class RollControlProvider final : public AbstractSimulationExtensionProvider
{
public:
    RollControlProvider();
};

}  // namespace QtRocket
