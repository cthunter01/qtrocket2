#include "QtRocket/simulation/extension/example/AirStartProvider.h"

#include <memory>
#include <string>

#include "QtRocket/simulation/extension/AbstractSimulationExtensionProvider.h"
#include "QtRocket/simulation/extension/example/AirStart.h"

namespace QtRocket
{

AirStartProvider::AirStartProvider()
  : AbstractSimulationExtensionProvider(std::string(AirStart::kId),
                                        [] { return std::make_unique<AirStart>(); },
                                        {"Launch conditions", "Air-start"})
{
}

}  // namespace QtRocket
