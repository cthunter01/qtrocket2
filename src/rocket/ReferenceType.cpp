#include "QtRocket/rocket/ReferenceType.h"

#include <optional>
#include <string_view>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/RadialParent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// A symmetric component's fore and aft radii (Java: instanceof SymmetricComponent,
/// getForeRadius(), getAftRadius()), each read when asked, as Java reads them: reading an
/// automatic radius refreshes it, and NOSECONE reads the aft radius only when the fore radius is
/// too small.
class SymmetricRadii
{
public:
    /// @p component's radii when it is a SymmetricComponent, or a stand-in for one (see
    /// standInOf()); nullopt for any other component.
    [[nodiscard]] static std::optional<SymmetricRadii> of(const RocketComponent& component)
    {
        if (const auto* symmetric = dynamic_cast<const SymmetricComponent*>(&component))
        {
            return SymmetricRadii{symmetric, nullptr};
        }
        if (const RadialParent* standIn = standInOf(component))
        {
            return SymmetricRadii{nullptr, standIn};
        }
        return std::nullopt;
    }

    [[nodiscard]] double fore() const
    {
        return m_symmetric != nullptr ? m_symmetric->getForeRadius()
                                      : m_standIn->getOuterRadius(-1.0);
    }

    [[nodiscard]] double aft() const
    {
        return m_symmetric != nullptr ? m_symmetric->getAftRadius()
                                      : m_standIn->getOuterRadius(m_standIn->getLength());
    }

private:
    SymmetricRadii(const SymmetricComponent* symmetric, const RadialParent* standIn)
      : m_symmetric(symmetric), m_standIn(standIn)
    {
    }

    /// A component of a body kind that is a RadialParent but not a SymmetricComponent: no class
    /// of the library is one, but the test fixtures' TestBodyComponent is, whose fore radius is
    /// getOuterRadius(-1) and aft radius getOuterRadius(getLength()).
    /// HOOK(test-fixtures): remove once TestRockets and the other fixtures build their bodies from
    /// the real components.
    [[nodiscard]] static const RadialParent* standInOf(const RocketComponent& component)
    {
        if (!isBodyComponent(component.kind()))
        {
            return nullptr;
        }
        return dynamic_cast<const RadialParent*>(&component);
    }

    const SymmetricComponent* m_symmetric;
    const RadialParent*       m_standIn;
};

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
                if (const std::optional<SymmetricRadii> s = SymmetricRadii::of(*c))
                {
                    if (s->fore() >= 0.0005)
                    {
                        return s->fore() * 2;
                    }
                    if (s->aft() >= 0.0005)
                    {
                        return s->aft() * 2;
                    }
                }
            }
            return Rocket::kDefaultReferenceLength;
        case ReferenceType::MAXIMUM:
        {
            double r = 0;
            for (const RocketComponent* c : config.getActiveComponents())
            {
                if (const std::optional<SymmetricRadii> s = SymmetricRadii::of(*c))
                {
                    r = MathUtil::javaMax(r, s->fore());
                    r = MathUtil::javaMax(r, s->aft());
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
