#pragma once

#include "QtRocket/simulation/extension/AbstractSimulationExtensionProvider.h"

namespace QtRocket
{

/// The provider of the AirStart extension (OpenRocket's
/// simulation/extension/example/AirStartProvider): its id is AirStart::kId, and its menu name
/// {"Launch conditions", "Air-start"}.
class AirStartProvider final : public AbstractSimulationExtensionProvider
{
public:
    AirStartProvider();
};

}  // namespace QtRocket
