#include "QtRocket/simulation/extension/impl/JavaCodeProvider.h"

#include <memory>
#include <string>

#include "QtRocket/simulation/extension/AbstractSimulationExtensionProvider.h"
#include "QtRocket/simulation/extension/impl/JavaCode.h"

namespace QtRocket
{

JavaCodeProvider::JavaCodeProvider()
  : AbstractSimulationExtensionProvider(std::string(JavaCode::kId),
                                        [] { return std::make_unique<JavaCode>(); },
                                        {"Scripts", "Java listeners"})
{
}

}  // namespace QtRocket
