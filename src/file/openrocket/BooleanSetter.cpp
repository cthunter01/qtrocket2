#include "QtRocket/file/openrocket/BooleanSetter.h"

#include <string_view>
#include <utility>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

BooleanSetter::BooleanSetter(SetFunction set) : m_set(std::move(set))
{
    QTROCKET_ASSERT(m_set != nullptr);
}

Result<void> BooleanSetter::set(RocketComponent& component, std::string_view  value,
                                const Attributes& /*attributes*/, WarningSet& warnings,
                                const DocumentLoadingContext& /*context*/) const
{
    const std::string_view trimmed = Strings::trim(value);
    if (Strings::javaEqualsIgnoreCase(trimmed, "true"))
    {
        m_set(component, true);
    }
    else if (Strings::javaEqualsIgnoreCase(trimmed, "false"))
    {
        m_set(component, false);
    }
    else
    {
        warnings.add(Warning::kFileInvalidParameter);
    }
    return {};
}

}  // namespace QtRocket
