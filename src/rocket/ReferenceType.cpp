#include "QtRocket/rocket/ReferenceType.h"

#include <optional>
#include <string_view>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/RadialParent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The fore and aft radii of a symmetric component.
struct SymmetricRadii
{
    double fore;
    double aft;
};

/// @p component's radii when it is a symmetric component (Java: instanceof SymmetricComponent,
/// getForeRadius(), getAftRadius()); see the HOOK in the header.
[[nodiscard]] std::optional<SymmetricRadii> symmetricRadii(const RocketComponent& component)
{
    if (!isBodyComponent(component.kind()))
    {
        return std::nullopt;
    }
    const auto* radial = dynamic_cast<const RadialParent*>(&component);
    if (radial == nullptr)
    {
        return std::nullopt;
    }
    return SymmetricRadii{.fore = radial->getOuterRadius(-1.0),
                          .aft  = radial->getOuterRadius(radial->getLength())};
}

}  // namespace

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
            for (const RocketComponent* c : config.getActiveComponents())
            {
                if (const std::optional<SymmetricRadii> radii = symmetricRadii(*c))
                {
                    if (radii->fore >= 0.0005)
                    {
                        return radii->fore * 2;
                    }
                    if (radii->aft >= 0.0005)
                    {
                        return radii->aft * 2;
                    }
                }
            }
            return Rocket::kDefaultReferenceLength;
        case ReferenceType::MAXIMUM:
        {
            double r = 0;
            for (const RocketComponent* c : config.getActiveComponents())
            {
                if (const std::optional<SymmetricRadii> radii = symmetricRadii(*c))
                {
                    r = MathUtil::javaMax(r, radii->fore);
                    r = MathUtil::javaMax(r, radii->aft);
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
