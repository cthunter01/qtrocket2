#include "QtRocket/file/openrocket/StringSetter.h"

#include <string_view>
#include <utility>

#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

StringSetter::StringSetter(SetFunction set)
{
    QTROCKET_ASSERT(set != nullptr);
    m_set = [set = std::move(set)](RocketComponent& component, std::string_view value,
                                   WarningSet& /*warnings*/) { return set(component, value); };
}

StringSetter::StringSetter(WarningFunction set) : m_set(std::move(set))
{
    QTROCKET_ASSERT(m_set != nullptr);
}

Result<void> StringSetter::set(RocketComponent& component, std::string_view  value,
                               const Attributes& /*attributes*/, WarningSet& warnings,
                               const DocumentLoadingContext& /*context*/) const
{
    return m_set(component, value, warnings);
}

}  // namespace QtRocket
