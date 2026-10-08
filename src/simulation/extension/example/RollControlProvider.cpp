#include "QtRocket/simulation/extension/example/RollControlProvider.h"

#include <memory>
#include <string>

#include "QtRocket/simulation/extension/AbstractSimulationExtensionProvider.h"
#include "QtRocket/simulation/extension/example/RollControl.h"

namespace QtRocket
{

RollControlProvider::RollControlProvider()
  : AbstractSimulationExtensionProvider(std::string(RollControl::kId),
                                        [] { return std::make_unique<RollControl>(); },
                                        {"Control Enhancements", "Roll Control"})
{
}

}  // namespace QtRocket
