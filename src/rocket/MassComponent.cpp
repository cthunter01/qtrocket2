#include "QtRocket/rocket/MassComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <optional>
#include <string_view>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

using Type = MassComponent::MassComponentType;

struct TypeInfo
{
    Type             type;
    std::string_view name;
    std::string_view orkName;
    std::string_view displayKey;
    std::string_view displayName;
};

// Translation keys and English titles from OpenRocket's messages.properties.
constexpr std::array<TypeInfo, MassComponent::kAllMassComponentTypes.size()> kTypes{{
    {.type        = Type::MASSCOMPONENT,
     .name        = "MASSCOMPONENT",
     .orkName     = "masscomponent",
     .displayKey  = "MassComponent.MassComponent",
     .displayName = "Mass Component"},
    {.type        = Type::ALTIMETER,
     .name        = "ALTIMETER",
     .orkName     = "altimeter",
     .displayKey  = "MassComponent.Altimeter",
     .displayName = "Altimeter"},
    {.type        = Type::FLIGHTCOMPUTER,
     .name        = "FLIGHTCOMPUTER",
     .orkName     = "flightcomputer",
     .displayKey  = "MassComponent.FlightComputer",
     .displayName = "Flight Computer"},
    {.type        = Type::DEPLOYMENTCHARGE,
     .name        = "DEPLOYMENTCHARGE",
     .orkName     = "deploymentcharge",
     .displayKey  = "MassComponent.DeploymentCharge",
     .displayName = "Deployment Charge"},
    {.type        = Type::TRACKER,
     .name        = "TRACKER",
     .orkName     = "tracker",
     .displayKey  = "MassComponent.Tracker",
     .displayName = "Tracker"},
    {.type        = Type::PAYLOAD,
     .name        = "PAYLOAD",
     .orkName     = "payload",
     .displayKey  = "MassComponent.Payload",
     .displayName = "Payload"},
    {.type        = Type::RECOVERYHARDWARE,
     .name        = "RECOVERYHARDWARE",
     .orkName     = "recoveryhardware",
     .displayKey  = "MassComponent.RecoveryHardware",
     .displayName = "Recovery Hardware"},
    {.type        = Type::BATTERY,
     .name        = "BATTERY",
     .orkName     = "battery",
     .displayKey  = "MassComponent.Battery",
     .displayName = "Battery"},
}};

[[nodiscard]] constexpr const TypeInfo& info(Type type) noexcept
{
    for (const TypeInfo& row : kTypes)
    {
        if (row.type == type)
        {
            return row;
        }
    }
    return kTypes.front();  // not reached: every type has a row (checked below)
}

static_assert(std::ranges::all_of(MassComponent::kAllMassComponentTypes,
                                  [](Type type) { return info(type).type == type; }));

}  // namespace

MassComponent::MassComponent()
{
    setDisplayOrderSide(13);  // Order for displaying the component in the 2D side view
    setDisplayOrderBack(10);  // Order for displaying the component in the 2D back view
}

MassComponent::MassComponent(double length, double radius, double mass)
  : MassObject(length, radius), m_mass(mass)
{
    setDisplayOrderSide(13);  // Order for displaying the component in the 2D side view
    setDisplayOrderBack(10);  // Order for displaying the component in the 2D back view
}

std::unique_ptr<RocketComponent> MassComponent::cloneShallow() const
{
    return std::make_unique<MassComponent>(*this);
}

double MassComponent::getComponentMass() const
{
    return m_mass;
}

void MassComponent::setComponentMass(double mass)
{
    mass = MathUtil::javaMax(mass, 0.0);
    if (MathUtil::equals(m_mass, mass))
    {
        return;
    }
    m_mass = mass;
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

double MassComponent::getDensity() const
{
    double d = getComponentMass() / getVolume();
    if (std::isnan(d))
    {
        d = 0;
    }
    return d;
}

void MassComponent::setDensity(double density)
{
    double m = density * getVolume();
    m        = MathUtil::clamp(m, 0, 1000000);
    if (std::isnan(m))
    {
        m = 0;
    }
    setComponentMass(m);
}

double MassComponent::getVolume() const
{
    // Java evaluates the radius before the length.
    const double radius = getRadius();
    return std::numbers::pi * MathUtil::pow2(radius) * getLength();
}

void MassComponent::setMassComponentType(MassComponentType type)
{
    if (m_massComponentType == type)
    {
        return;
    }
    m_massComponentType = type;
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

bool MassComponent::allowsChildren() const
{
    return true;
}

bool MassComponent::isCompatible(ComponentKind kind) const
{
    return isInternal(kind);
}

std::string_view massComponentTypeName(MassComponent::MassComponentType type) noexcept
{
    return info(type).name;
}

std::string_view orkName(MassComponent::MassComponentType type) noexcept
{
    return info(type).orkName;
}

std::optional<MassComponent::MassComponentType> massComponentTypeFromOrkName(std::string_view text)
{
    for (const TypeInfo& row : kTypes)
    {
        if (Strings::orkEnumNameMatches(text, row.name))
        {
            return row.type;
        }
    }
    return std::nullopt;
}

std::string_view displayKey(MassComponent::MassComponentType type) noexcept
{
    return info(type).displayKey;
}

std::string_view displayName(MassComponent::MassComponentType type) noexcept
{
    return info(type).displayName;
}

}  // namespace QtRocket
