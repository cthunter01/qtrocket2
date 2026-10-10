#include "QtRocket/file/openrocket/OverrideSetter.h"

#include <cmath>
#include <optional>
#include <string_view>
#include <utility>

#include "QtRocket/file/openrocket/LoaderWarnings.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

OverrideSetter::OverrideSetter(SetFunction set, EnabledFunction enable)
  : m_set(std::move(set)), m_enable(std::move(enable))
{
    QTROCKET_ASSERT(m_set != nullptr && m_enable != nullptr);
}

Result<void> OverrideSetter::set(RocketComponent& component, std::string_view  value,
                                 const Attributes& /*attributes*/, WarningSet& warnings,
                                 const DocumentLoadingContext& /*context*/) const
{
    // Java: Double.parseDouble(s), and whatever it gives is stored.
    const std::optional<double> number = Strings::javaParseDouble(value);
    if (!number.has_value())
    {
        warnings.add(Warning::kFileInvalidParameter);
        return {};
    }
    // Not OpenRocket's: a NaN or an infinity is refused (decision L3), in QtRocket's own words.
    if (!std::isfinite(*number))
    {
        warnings.add(LoaderWarnings::kOverrideNotFinite);
        return {};
    }
    m_set(component, *number);
    m_enable(component, true);
    return {};
}

}  // namespace QtRocket
