#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/unit/UnitGroup.h"

namespace QtRocket
{

class Manufacturer;

/// ComponentPreset.Type, defined in ComponentPreset.h; declared here so that preset values can
/// hold one.
enum class ComponentPresetType;

/// A preset's reference to its manufacturer. Manufacturers live in their registry for the whole
/// program (Manufacturer.h), so a preset holds a reference, never a copy, and never a null one.
using ManufacturerRef = std::reference_wrapper<const Manufacturer>;

/// Every value type a preset property can have: Java's Boolean, Integer, Double, String,
/// Manufacturer, ComponentPreset.Type, Transition.Shape, Material, ExternalComponent.Finish and
/// byte[] (the image), one alternative each.
using TypedValue =
    std::variant<bool, int, double, std::string, ManufacturerRef, ComponentPresetType,
                 TransitionShape, Material, Finish, std::vector<std::byte>>;

namespace Detail
{

/// The index of @p T among the alternatives of TypedValue, or their count when it is none.
template <class T, class... Alternatives>
consteval std::size_t typedValueIndex(std::type_identity<std::variant<Alternatives...>> /*tag*/)
{
    constexpr std::array<bool, sizeof...(Alternatives)> kMatches{
        std::is_same_v<T, Alternatives>...};
    for (std::size_t i = 0; i < kMatches.size(); i++)
    {
        if (kMatches.at(i))
        {
            return i;
        }
    }
    return kMatches.size();
}

}  // namespace Detail

/// One of the value types of TypedValue.
template <class T>
concept TypedValueType =
    Detail::typedValueIndex<T>(std::type_identity<TypedValue>{}) < std::variant_size_v<TypedValue>;

/// The identity of a TypedKey whatever its value type: its name, the value type (as an index
/// into TypedValue) and the unit group of a quantity. TypedPropertyMap stores keys as this,
/// and key lists (ComponentPreset's orderedKeyList() and displayedColumns()) are lists of it.
///
/// Equality is Java's TypedKey.equals: the same name and the same value type (the unit group
/// does not count). Java's hashCode() mixes in Class.hashCode(), an identity hash that differs
/// from run to run, so it has no counterpart.
class AnyTypedKey
{
public:
    /// The name, e.g. "OuterDiameter" (getName()): what the preset digest writes and the .orc
    /// files use.
    [[nodiscard]] constexpr std::string_view getName() const noexcept { return m_name; }

    /// The unit group of a quantity (getUnitGroup()), nullopt for the other keys.
    [[nodiscard]] constexpr std::optional<UnitGroupId> getUnitGroup() const noexcept
    {
        return m_unitGroup;
    }

    /// The index of the value type in TypedValue (Java: getType()).
    [[nodiscard]] constexpr std::size_t getTypeIndex() const noexcept { return m_typeIndex; }

    /// True when the value type is @p T.
    template <TypedValueType T>
    [[nodiscard]] constexpr bool hasType() const noexcept
    {
        return m_typeIndex == Detail::typedValueIndex<T>(std::type_identity<TypedValue>{});
    }

    /// "TypedKey [name=<name>]" (toString()).
    [[nodiscard]] std::string toString() const
    {
        return "TypedKey [name=" + std::string(m_name) + "]";
    }

    [[nodiscard]] constexpr bool operator==(const AnyTypedKey& other) const noexcept
    {
        return m_name == other.m_name && m_typeIndex == other.m_typeIndex;
    }

protected:
    constexpr AnyTypedKey(std::string_view name, std::size_t typeIndex,
                          std::optional<UnitGroupId> unitGroup) noexcept
      : m_name(name), m_typeIndex(typeIndex), m_unitGroup(unitGroup)
    {
    }

private:
    std::string_view           m_name;
    std::size_t                m_typeIndex;
    std::optional<UnitGroupId> m_unitGroup;
};

/// A key of a TypedPropertyMap whose values have type @p T (OpenRocket's TypedKey<T>): the map's
/// get() and put() take and give a T for it, so a value of the wrong type does not compile.
///
/// Keys are process-wide constants (ComponentPreset's kLength and the rest) built at compile
/// time: the constructor is consteval, so the name, a view, always refers to a string literal
/// and a key can be declared constexpr without any initialisation order issue.
template <TypedValueType T>
class TypedKey : public AnyTypedKey
{
public:
    using ValueType = T;

    /// A key named @p name, of the unit group @p unitGroup when it is a quantity.
    consteval explicit TypedKey(std::string_view           name,
                                std::optional<UnitGroupId> unitGroup = std::nullopt) noexcept
      : AnyTypedKey(name, Detail::typedValueIndex<T>(std::type_identity<TypedValue>{}), unitGroup)
    {
    }
};

}  // namespace QtRocket
