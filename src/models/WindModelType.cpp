#include "QtRocket/models/WindModelType.h"

#include <optional>
#include <string_view>

#include "QtRocket/util/Strings.h"

namespace QtRocket
{

std::string_view toStringValue(WindModelType type) noexcept
{
    switch (type)
    {
        case WindModelType::AVERAGE:
            return "Average";
        case WindModelType::MULTI_LEVEL:
            return "MultiLevel";
    }
    return "Average";  // not reached: the switch covers every type
}

std::string_view windModelTypeName(WindModelType type) noexcept
{
    switch (type)
    {
        case WindModelType::AVERAGE:
            return "AVERAGE";
        case WindModelType::MULTI_LEVEL:
            return "MULTI_LEVEL";
    }
    return "AVERAGE";  // not reached
}

std::string_view orkName(WindModelType type) noexcept
{
    switch (type)
    {
        case WindModelType::AVERAGE:
            return "average";
        case WindModelType::MULTI_LEVEL:
            return "multilevel";
    }
    return "average";  // not reached
}

std::optional<WindModelType> windModelTypeFromString(std::string_view text) noexcept
{
    for (const WindModelType type : kAllWindModelTypes)
    {
        if (Strings::javaEqualsIgnoreCase(toStringValue(type), text))
        {
            return type;
        }
    }
    return std::nullopt;
}

}  // namespace QtRocket
