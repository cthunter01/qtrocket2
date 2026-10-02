#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string_view>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket
{

/// A generic component with a given mass and an approximate shape (OpenRocket's MassComponent):
/// payload, altimeter, battery, ... The mass is set directly (setComponentMass()) or through a
/// density over the packed cylinder (setDensity()). The type only says what the component
/// represents; it does not affect any calculation. It accepts internal components as children.
///
/// Not ported: the multi-edit config listeners (see RocketComponent).
class MassComponent : public MassObject
{
public:
    using RocketComponent::isCompatible;

    /// What the mass component represents (Java: MassComponentType), in OpenRocket's order.
    enum class MassComponentType
    {
        MASSCOMPONENT,
        ALTIMETER,
        FLIGHTCOMPUTER,
        DEPLOYMENTCHARGE,
        TRACKER,
        PAYLOAD,
        RECOVERYHARDWARE,
        BATTERY,
    };

    /// Every type, in declaration order (MassComponentType.values()).
    static constexpr std::array<MassComponentType, 8> kAllMassComponentTypes{
        MassComponentType::MASSCOMPONENT,    MassComponentType::ALTIMETER,
        MassComponentType::FLIGHTCOMPUTER,   MassComponentType::DEPLOYMENTCHARGE,
        MassComponentType::TRACKER,          MassComponentType::PAYLOAD,
        MassComponentType::RECOVERYHARDWARE, MassComponentType::BATTERY};

    /// A component of mass 0, 25 mm long with a radius of 12.5 mm, of type MASSCOMPONENT,
    /// positioned TOP.
    MassComponent();

    /// A component of @p length, @p radius and @p mass (stored as given), of type
    /// MASSCOMPONENT, positioned TOP.
    MassComponent(double length, double radius, double mass);

    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::MASS_COMPONENT;
    }

    /// The mass.
    [[nodiscard]] double getComponentMass() const override;

    /// Sets the mass (negative values become 0); fires MASS_CHANGE unless it equals (within
    /// MathUtil::equals()) the current one.
    void setComponentMass(double mass);

    /// The mass divided by the packed volume (pi r^2 length); 0 when that is NaN (0 / 0).
    [[nodiscard]] double getDensity() const;

    /// Sets the mass to @p density times the packed volume, clamped to 0 ... 1e6 (NaN gives 0),
    /// through setComponentMass().
    void setDensity(double density);

    [[nodiscard]] MassComponentType getMassComponentType() const noexcept
    {
        return m_massComponentType;
    }

    /// Sets the type; fires NONFUNCTIONAL_CHANGE when it changes.
    void setMassComponentType(MassComponentType type);

    /// Always true.
    [[nodiscard]] bool allowsChildren() const override;

    /// Accepts every internal component.
    [[nodiscard]] bool isCompatible(ComponentKind kind) const override;

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;

private:
    /// pi r^2 length, the radius read first.
    [[nodiscard]] double getVolume() const;

    double            m_mass{0};
    MassComponentType m_massComponentType{MassComponentType::MASSCOMPONENT};
};

/// The constant's name, e.g. "FLIGHTCOMPUTER" (Java: name()).
[[nodiscard]] std::string_view massComponentTypeName(
    MassComponent::MassComponentType type) noexcept;

/// The .ork spelling the mass component saver writes in <masscomponenttype>: the lower-cased name
/// ("flightcomputer").
[[nodiscard]] std::string_view orkName(MassComponent::MassComponentType type) noexcept;

/// The type @p text names, matched as DocumentConfig.findEnum() does (Strings::
/// orkEnumNameMatches()); nullopt for anything else.
[[nodiscard]] std::optional<MassComponent::MassComponentType> massComponentTypeFromOrkName(
    std::string_view text);

/// The translation key of the type's title, e.g. "MassComponent.FlightComputer".
[[nodiscard]] std::string_view displayKey(MassComponent::MassComponentType type) noexcept;

/// The English title (Java: toString()), e.g. "Flight Computer".
[[nodiscard]] std::string_view displayName(MassComponent::MassComponentType type) noexcept;

}  // namespace QtRocket
