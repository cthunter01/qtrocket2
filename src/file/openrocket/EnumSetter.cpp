#include "QtRocket/file/openrocket/EnumSetter.h"

#include <string_view>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

Result<void> EnumSetter::set(RocketComponent& component, std::string_view  value,
                             const Attributes& /*attributes*/, WarningSet& warnings,
                             const DocumentLoadingContext& /*context*/) const
{
    if (!m_apply(component, value))
    {
        warnings.add(Warning::kFileInvalidParameter);
    }
    return {};
}

}  // namespace QtRocket
