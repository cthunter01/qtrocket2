#include "QtRocket/rocket/ReferenceType.h"

#include <optional>
#include <string_view>

#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/util/MathUtil.h"
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

double getReferenceLength(ReferenceType type, const FlightConfiguration& config)
{
    switch (type)
    {
        case ReferenceType::NOSECONE:
            // Each radius is read when Java reads it: reading an automatic radius refreshes it,
            // and the aft radius is read only when the fore radius is too small.
            for (const RocketComponent* c : config.getActiveComponents())
            {
                if (const auto* s = dynamic_cast<const SymmetricComponent*>(c))
                {
                    if (s->getForeRadius() >= 0.0005)
                    {
                        return s->getForeRadius() * 2;
                    }
                    if (s->getAftRadius() >= 0.0005)
                    {
                        return s->getAftRadius() * 2;
                    }
                }
            }
            return Rocket::kDefaultReferenceLength;
        case ReferenceType::MAXIMUM:
        {
            double r = 0;
            for (const RocketComponent* c : config.getActiveComponents())
            {
                if (const auto* s = dynamic_cast<const SymmetricComponent*>(c))
                {
                    r = MathUtil::javaMax(r, s->getForeRadius());
                    r = MathUtil::javaMax(r, s->getAftRadius());
                }
            }
            r *= 2;
            if (r < 0.001)
            {
                r = Rocket::kDefaultReferenceLength;
            }
            return r;
        }
        case ReferenceType::CUSTOM:
            return config.getRocket().getCustomReferenceLength();
    }
    return Rocket::kDefaultReferenceLength;
}

}  // namespace QtRocket
