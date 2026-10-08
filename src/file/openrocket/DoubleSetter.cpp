#include "QtRocket/file/openrocket/DoubleSetter.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// What DoubleSetter.set() makes of a text before it reads the number.
struct Parts
{
    /// The text that is compared with the special word (Java: special).
    std::string special;
    /// The text of the number (Java: data).
    std::string data;
    /// Whether the text was a word, the separator and more (Java: args.length > 1).
    bool severalParts{false};
};

/// @p trimmed as the word and the number of a setter without a separator, or split at
/// @p separator as String.split() splits.
[[nodiscard]] Parts splitValue(std::string_view trimmed, std::optional<char> separator)
{
    Parts parts{.special = std::string(trimmed), .data = std::string(trimmed)};
    if (!separator.has_value())
    {
        return parts;
    }
    const std::vector<std::string> args = Strings::splitJava(trimmed, *separator);
    if (args.size() <= 1)
    {
        return parts;
    }
    parts.severalParts = true;
    parts.special      = args.front();
    // String.join(separator, the parts behind the first).
    parts.data.clear();
    for (std::size_t i = 1; i < args.size(); ++i)
    {
        if (i > 1)
        {
            parts.data.push_back(*separator);
        }
        parts.data.append(args[i]);
    }
    return parts;
}

/// Warning.FILE_INVALID_PARAMETER + " data: '" + data + "' - " + c.getName().
[[nodiscard]] std::string invalidParameter(std::string_view data, const RocketComponent& component)
{
    return std::format("{} data: '{}' - {}", Warning::kFileInvalidParameter.toString(), data,
                       component.getName());
}

}  // namespace

DoubleSetter::DoubleSetter(SetFunction set) : m_set(std::move(set))
{
    QTROCKET_ASSERT(m_set != nullptr);
}

DoubleSetter::DoubleSetter(SetFunction set, double multiplier)
  : m_set(std::move(set)), m_multiplier(multiplier)
{
    QTROCKET_ASSERT(m_set != nullptr);
}

DoubleSetter::DoubleSetter(SetFunction set, std::string special, SpecialFunction specialSet)
  : m_set(std::move(set)), m_special(std::move(special)), m_specialSet(std::move(specialSet))
{
    QTROCKET_ASSERT(m_set != nullptr && m_specialSet != nullptr);
}

DoubleSetter::DoubleSetter(SetFunction set, std::string special, char separator,
                           SpecialFunction specialSet)
  : m_set(std::move(set)),
    m_special(std::move(special)),
    m_specialSet(std::move(specialSet)),
    m_separator(separator)
{
    QTROCKET_ASSERT(m_set != nullptr && m_specialSet != nullptr);
}

Result<void> DoubleSetter::set(RocketComponent& component, std::string_view  value,
                               const Attributes& /*attributes*/, WarningSet& warnings,
                               const DocumentLoadingContext& /*context*/) const
{
    const Parts parts = splitValue(Strings::trim(value), m_separator);
    const bool  isSpecial =
        m_special.has_value() && Strings::javaEqualsIgnoreCase(parts.special, *m_special);

    // Normal case
    if (!isSpecial || parts.severalParts)
    {
        const std::optional<double> number = Strings::javaParseDouble(parts.data);
        if (!number.has_value())
        {
            // Java: the NumberFormatException is caught, and the special case is still looked at.
            warnings.add(invalidParameter(parts.data, component));
        }
        else if (!std::isfinite(*number) || !std::isfinite(*number * m_multiplier))
        {
            // Check for valid double. Java returns here, so "auto NaN" sets no flag either. Java
            // checks the number it read; the product is checked here too (decision L3), which
            // matters for no multiplier of the table: pi / 180 makes a number smaller.
            warnings.add(invalidParameter(parts.data, component));
            return {};
        }
        else
        {
            m_set(component, *number * m_multiplier);
        }
    }

    // Check for special case
    if (isSpecial)
    {
        m_specialSet(component, true);
    }
    return {};
}

}  // namespace QtRocket
