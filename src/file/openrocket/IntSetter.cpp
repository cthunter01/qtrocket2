#include "QtRocket/file/openrocket/IntSetter.h"

#include <optional>
#include <string_view>
#include <utility>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

IntSetter::IntSetter(SetFunction set) : m_set(std::move(set))
{
    QTROCKET_ASSERT(m_set != nullptr);
}

IntSetter::IntSetter(SetFunction set, int maximum) : m_set(std::move(set)), m_maximum(maximum)
{
    QTROCKET_ASSERT(m_set != nullptr);
}

Result<void> IntSetter::set(RocketComponent& component, std::string_view  value,
                            const Attributes& /*attributes*/, WarningSet& warnings,
                            const DocumentLoadingContext& /*context*/) const
{
    // Integer.parseInt(s): nothing is trimmed.
    const std::optional<int> number = Strings::parseInt(value);
    if (!number.has_value() || (m_maximum.has_value() && *number > *m_maximum))
    {
        warnings.add(Warning::kFileInvalidParameter);
        return {};
    }
    m_set(component, *number);
    return {};
}

}  // namespace QtRocket
