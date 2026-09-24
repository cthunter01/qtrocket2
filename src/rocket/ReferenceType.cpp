#include "QtRocket/rocket/ReferenceType.h"

#include <optional>
#include <string_view>

#include "QtRocket/util/Strings.h"

namespace QtRocket
{

std::string_view referenceTypeName(ReferenceType type) noexcept
{
    switch (type)
    {
        case ReferenceType::NOSECONE:
            return "NOSECONE";
        case ReferenceType::MAXIMUM:
            return "MAXIMUM";
        case ReferenceType::CUSTOM:
            return "CUSTOM";
    }
    return "MAXIMUM";
}

std::string_view orkName(ReferenceType type) noexcept
{
    switch (type)
    {
        case ReferenceType::NOSECONE:
            return "nosecone";
        case ReferenceType::MAXIMUM:
            return "maximum";
        case ReferenceType::CUSTOM:
            return "custom";
    }
    return "maximum";
}

std::optional<ReferenceType> referenceTypeFromOrkName(std::string_view text)
{
    for (const ReferenceType type : kAllReferenceTypes)
    {
        if (Strings::orkEnumNameMatches(text, referenceTypeName(type)))
        {
            return type;
        }
    }
    return std::nullopt;
}

}  // namespace QtRocket
