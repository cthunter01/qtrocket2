#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedKey.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/unit/UnitGroup.h"

namespace QtRocket
{

class Manufacturer;

/// A preset component (OpenRocket's ComponentPreset): a manufacturer's part as a typed property
/// map, with the MD5 digest of its properties that .ork files store to recognise it again.
///
/// Presets are made by ComponentPresetFactory::create(), which validates the properties,
/// completes the derived ones and computes the digest; afterwards a preset is immutable. The
/// properties are read through the typed keys below (Java's static TypedKey fields, named
/// kCamelCase here: LENGTH is kLength), whose names are OpenRocket's ("Length", "PartNo", ...).
///
/// The accessors that return a reference into the preset (getDigest(), get(), getProperties())
/// return a copy when called on a temporary preset. That copy of the properties is a temporary
/// map in turn, whose pointing accessors (get(), getValue(), entries()) do not compile: bind it
/// to a variable first. A preset reached through operator-> of a temporary Result is not a
/// temporary to them: keep the Result while using such a reference.
///
/// Not ported: Java's serialization (writeObject/readObject with MaterialSerializationProxy);
/// the .orc loaders (Milestone 3) rebuild presets through the factory instead.
class ComponentPreset
{
public:
    using Type = ComponentPresetType;

    // ---- the keys (Java: ComponentPreset.LEGACY, ...), in Java's declaration order

    static constexpr TypedKey<bool>                kLegacy{"Legacy"};
    static constexpr TypedKey<ManufacturerRef>     kManufacturer{"Manufacturer"};
    static constexpr TypedKey<std::string>         kPartNo{"PartNo"};
    static constexpr TypedKey<std::string>         kDescription{"Description"};
    static constexpr TypedKey<ComponentPresetType> kType{"Type"};
    static constexpr TypedKey<double>              kLength{"Length", UnitGroupId::LENGTH};
    static constexpr TypedKey<double>              kHeight{"Height", UnitGroupId::LENGTH};
    static constexpr TypedKey<double>              kWidth{"Width", UnitGroupId::LENGTH};
    static constexpr TypedKey<double> kInnerDiameter{"InnerDiameter", UnitGroupId::LENGTH};
    static constexpr TypedKey<double> kOuterDiameter{"OuterDiameter", UnitGroupId::LENGTH};
    static constexpr TypedKey<double> kForeShoulderLength{"ForeShoulderLength",
                                                          UnitGroupId::LENGTH};
    static constexpr TypedKey<double> kForeShoulderDiameter{"ForeShoulderDiameter",
                                                            UnitGroupId::LENGTH};
    static constexpr TypedKey<double> kForeOuterDiameter{"ForeOuterDiameter", UnitGroupId::LENGTH};
    static constexpr TypedKey<double> kAftShoulderLength{"AftShoulderLength", UnitGroupId::LENGTH};
    static constexpr TypedKey<double> kAftShoulderDiameter{"AftShoulderDiameter",
                                                           UnitGroupId::LENGTH};
    static constexpr TypedKey<double> kAftOuterDiameter{"AftOuterDiameter", UnitGroupId::LENGTH};
    static constexpr TypedKey<double> kCd{"DragCoefficient", UnitGroupId::COEFFICIENT};
    static constexpr TypedKey<TransitionShape>        kShape{"Shape"};
    static constexpr TypedKey<Material>               kMaterial{"Material"};
    static constexpr TypedKey<Finish>                 kFinish{"Finish"};
    static constexpr TypedKey<double>                 kThickness{"Thickness", UnitGroupId::LENGTH};
    static constexpr TypedKey<bool>                   kFilled{"Filled"};
    static constexpr TypedKey<double>                 kMass{"Mass", UnitGroupId::MASS};
    static constexpr TypedKey<double>                 kDiameter{"Diameter", UnitGroupId::LENGTH};
    static constexpr TypedKey<std::vector<std::byte>> kImage{"Image"};

    // rail button specific
    static constexpr TypedKey<double> kBaseHeight{"BaseHeight", UnitGroupId::LENGTH};
    static constexpr TypedKey<double> kFlangeHeight{"FlangeHeight", UnitGroupId::LENGTH};
    static constexpr TypedKey<double> kScrewHeight{"ScrewHeight", UnitGroupId::LENGTH};
    static constexpr TypedKey<double> kScrewMass{"ScrewMass", UnitGroupId::MASS};
    static constexpr TypedKey<double> kNutMass{"NutMass", UnitGroupId::MASS};

    // parachute specific (the manufacturer, part number, description, diameter, canopy
    // material and mass are the keys above). The canopy shape is a Transition.Shape in
    // OpenRocket too, and the surface area is in the length unit group, as there.
    static constexpr TypedKey<TransitionShape> kCanopyShape{"CanopyShape"};
    static constexpr TypedKey<double>          kSpillDia{"SpillDia", UnitGroupId::LENGTH};
    static constexpr TypedKey<double>          kSurfaceArea{"SurfaceArea", UnitGroupId::LENGTH};
    static constexpr TypedKey<int>             kSides{"Sides"};
    static constexpr TypedKey<int>             kLineCount{"LineCount"};
    static constexpr TypedKey<double>          kLineLength{"LineLength", UnitGroupId::LENGTH};
    static constexpr TypedKey<Material>        kLineMaterial{"LineMaterial"};
    static constexpr TypedKey<double>          kPackedLength{"PackedLength", UnitGroupId::LENGTH};
    static constexpr TypedKey<double> kPackedDiameter{"PackedDiameter", UnitGroupId::LENGTH};

    /// Every key above, in declaration order.
    [[nodiscard]] static std::span<const AnyTypedKey> allKeys() noexcept;

    /// ORDERED_KEY_LIST: the 30 keys the preset tables offer as columns, in column order: every
    /// key but kType, kHeight, kCd, kImage, kCanopyShape, kSpillDia, kSurfaceArea, kPackedLength
    /// and kPackedDiameter. A translation key "table.column.<name>" exists for each.
    [[nodiscard]] static std::span<const AnyTypedKey> orderedKeyList() noexcept;

    /// Type.getDisplayedColumns(): the columns the preset chooser shows for presets of @p type.
    [[nodiscard]] static std::span<const AnyTypedKey> displayedColumns(Type type) noexcept;

    // ---- properties

    /// The LEGACY flag (getLegacy()): whether the preset comes from the legacy database; nullopt
    /// when the preset does not say (Java: null).
    [[nodiscard]] std::optional<bool> getLegacy() const;

    /// The preset type (getType()).
    [[nodiscard]] Type getType() const { return get(kType); }

    /// The manufacturer (getManufacturer()).
    [[nodiscard]] const Manufacturer& getManufacturer() const { return get(kManufacturer).get(); }

    /// The part number (getPartNo()), a copy as Java's String, so that it outlives a temporary
    /// preset (ComponentPresetFactory::create(...)->getPartNo()).
    [[nodiscard]] std::string getPartNo() const { return get(kPartNo); }

    /// The digest: 32 lowercase hexadecimal digits (getDigest()). The reference lives as long as
    /// the preset; a temporary preset gives a copy.
    [[nodiscard]] const std::string& getDigest() const& noexcept { return m_digest; }
    [[nodiscard]] std::string        getDigest() const&& { return m_digest; }

    /// True when @p key has a value (has()).
    [[nodiscard]] bool has(const AnyTypedKey& key) const noexcept
    {
        return m_properties.containsKey(key);
    }

    /// The value of @p key (get()). The reference lives as long as the preset; a temporary
    /// preset gives a copy.
    /// @throws BugError "Preset did not contain key <key> <properties>" when it has none, as
    ///         OpenRocket's BugException.
    template <TypedValueType T>
    [[nodiscard]] const T& get(const TypedKey<T>& key) const&
    {
        const T* value = m_properties.get(key);
        if (value == nullptr)
        {
            missingKey(key);
        }
        return *value;
    }
    template <TypedValueType T>
    [[nodiscard]] T get(const TypedKey<T>& key) const&&
    {
        return get(key);
    }

    /// The properties, in the order they were put. The reference lives as long as the preset; a
    /// temporary preset gives a copy (a temporary map, see TypedPropertyMap).
    [[nodiscard]] const TypedPropertyMap& getProperties() const& noexcept { return m_properties; }
    [[nodiscard]] TypedPropertyMap        getProperties() const&& { return m_properties; }

    // ---- identity

    /// ComponentPreset.compareTo: by the manufacturers' simple names, then by part number (both
    /// String.compareTo, Strings::javaCompareTo). Negative, zero or positive.
    [[nodiscard]] int compareTo(const ComponentPreset& other) const;

    /// The part number (toString()), a copy as getPartNo().
    [[nodiscard]] std::string toString() const { return getPartNo(); }

    /// "<manufacturer>|<part number>" with the manufacturer's display name (preferenceKey()),
    /// under which the preferences keep a favourite preset.
    [[nodiscard]] std::string preferenceKey() const;

    /// The element the .ork saver writes for a component based on this preset
    /// (RocketComponentSaver): <preset type="BODY_TUBE" manufacturer="Estes" partno="BT-20"
    /// digest="..."/>, with the manufacturer's simple name and the part number escaped as
    /// TextUtil.escapeXML does.
    [[nodiscard]] std::string toOrkElement() const;

    /// The test ComponentPresetDao.find(manufacturer, partNo) applies, which the .ork loader
    /// looks presets up with: @p manufacturer names this preset's manufacturer
    /// (Manufacturer::matches) and the part number equals @p partNo.
    [[nodiscard]] bool matches(std::string_view manufacturer, std::string_view partNo) const;

    /// Presets are equal when their digests are (equals()).
    [[nodiscard]] bool operator==(const ComponentPreset& other) const noexcept
    {
        return m_digest == other.m_digest;
    }

    /// The digest's String.hashCode (hashCode()).
    [[nodiscard]] int hashCode() const noexcept;

    /// The factory's put() (private) of a value that would convert silently does not compile,
    /// as in TypedPropertyMap::put().
    template <TypedValueType T, class U>
        requires RefusedTypedValue<T, U>
    void put(const TypedKey<T>& /*key*/, U&& /*value*/) = delete;

private:
    friend class ComponentPresetFactory;

    ComponentPreset() = default;

    /// Adds @p other's properties (the factory's putAll()).
    void putAll(const TypedPropertyMap& other) { m_properties.putAll(other); }

    /// Sets a property (the factory's put()).
    template <TypedValueType T>
    void put(const TypedKey<T>& key, std::type_identity_t<T> value)
    {
        m_properties.put(key, std::move(value));
    }

    /// computeDigest(): the MD5 of the properties other than LEGACY, sorted by key name, each
    /// written as DataOutputStream writes it: the name's bytes, then for a double its IEEE bits
    /// (Double.doubleToLongBits, big-endian), for a string, a manufacturer (its simple name), a
    /// finish or a preset type (their names) the low byte of each UTF-16 code unit
    /// (writeBytes), for a boolean one byte, for a material its density as a double, for a
    /// shape its ordinal as a big-endian int, and for an int or the image nothing (Java has no
    /// branch for Integer or byte[], so only the name counts).
    void computeDigest();

    [[noreturn]] void missingKey(const AnyTypedKey& key) const;

    TypedPropertyMap m_properties;
    std::string      m_digest;
};

}  // namespace QtRocket
