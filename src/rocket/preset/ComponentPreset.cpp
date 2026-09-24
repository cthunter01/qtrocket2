#include "QtRocket/rocket/preset/ComponentPreset.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/preset/TypedKey.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Md5.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

using Preset = ComponentPreset;

constexpr std::array<AnyTypedKey, 39> kAllKeys{
    Preset::kLegacy,
    Preset::kManufacturer,
    Preset::kPartNo,
    Preset::kDescription,
    Preset::kType,
    Preset::kLength,
    Preset::kHeight,
    Preset::kWidth,
    Preset::kInnerDiameter,
    Preset::kOuterDiameter,
    Preset::kForeShoulderLength,
    Preset::kForeShoulderDiameter,
    Preset::kForeOuterDiameter,
    Preset::kAftShoulderLength,
    Preset::kAftShoulderDiameter,
    Preset::kAftOuterDiameter,
    Preset::kCd,
    Preset::kShape,
    Preset::kMaterial,
    Preset::kFinish,
    Preset::kThickness,
    Preset::kFilled,
    Preset::kMass,
    Preset::kDiameter,
    Preset::kImage,
    Preset::kBaseHeight,
    Preset::kFlangeHeight,
    Preset::kScrewHeight,
    Preset::kScrewMass,
    Preset::kNutMass,
    Preset::kCanopyShape,
    Preset::kSpillDia,
    Preset::kSurfaceArea,
    Preset::kSides,
    Preset::kLineCount,
    Preset::kLineLength,
    Preset::kLineMaterial,
    Preset::kPackedLength,
    Preset::kPackedDiameter,
};

// DO NOT add to this list without redefining "table.column" (OpenRocket's comment).
constexpr std::array<AnyTypedKey, 30> kOrderedKeyList{
    Preset::kLegacy,
    Preset::kManufacturer,
    Preset::kPartNo,
    Preset::kDescription,
    Preset::kOuterDiameter,
    Preset::kForeOuterDiameter,
    Preset::kAftOuterDiameter,
    Preset::kInnerDiameter,
    Preset::kLength,
    Preset::kWidth,
    Preset::kAftShoulderDiameter,
    Preset::kAftShoulderLength,
    Preset::kForeShoulderDiameter,
    Preset::kForeShoulderLength,
    Preset::kBaseHeight,
    Preset::kFlangeHeight,
    Preset::kScrewHeight,
    Preset::kShape,
    Preset::kThickness,
    Preset::kFilled,
    Preset::kDiameter,
    Preset::kSides,
    Preset::kLineCount,
    Preset::kLineLength,
    Preset::kLineMaterial,
    Preset::kMass,
    Preset::kScrewMass,
    Preset::kNutMass,
    Preset::kFinish,
    Preset::kMaterial,
};

// Type.getDisplayedColumns(), per type.
constexpr std::array<AnyTypedKey, 7> kBodyTubeColumns{
    Preset::kLegacy,        Preset::kManufacturer,  Preset::kPartNo, Preset::kDescription,
    Preset::kInnerDiameter, Preset::kOuterDiameter, Preset::kLength};
constexpr std::array<AnyTypedKey, 9>  kNoseConeColumns{Preset::kLegacy,
                                                       Preset::kManufacturer,
                                                       Preset::kPartNo,
                                                       Preset::kDescription,
                                                       Preset::kShape,
                                                       Preset::kAftOuterDiameter,
                                                       Preset::kAftShoulderDiameter,
                                                       Preset::kAftShoulderLength,
                                                       Preset::kLength};
constexpr std::array<AnyTypedKey, 12> kTransitionColumns{Preset::kLegacy,
                                                         Preset::kManufacturer,
                                                         Preset::kPartNo,
                                                         Preset::kDescription,
                                                         Preset::kShape,
                                                         Preset::kForeOuterDiameter,
                                                         Preset::kForeShoulderDiameter,
                                                         Preset::kForeShoulderLength,
                                                         Preset::kAftOuterDiameter,
                                                         Preset::kAftShoulderDiameter,
                                                         Preset::kAftShoulderLength,
                                                         Preset::kLength};
constexpr std::array<AnyTypedKey, 7>  kTubeCouplerColumns{
    Preset::kLegacy,        Preset::kManufacturer,  Preset::kPartNo, Preset::kDescription,
    Preset::kOuterDiameter, Preset::kInnerDiameter, Preset::kLength};
constexpr std::array<AnyTypedKey, 6> kBulkHeadColumns{Preset::kLegacy,        Preset::kManufacturer,
                                                      Preset::kPartNo,        Preset::kDescription,
                                                      Preset::kOuterDiameter, Preset::kLength};
// CENTERING_RING, ENGINE_BLOCK and LAUNCH_LUG show the body tube's columns.
constexpr std::array<AnyTypedKey, 14> kRailButtonColumns{
    Preset::kLegacy,        Preset::kManufacturer,
    Preset::kPartNo,        Preset::kDescription,
    Preset::kBaseHeight,    Preset::kFlangeHeight,
    Preset::kScrewHeight,   Preset::kHeight,
    Preset::kInnerDiameter, Preset::kOuterDiameter,
    Preset::kMass,          Preset::kScrewMass,
    Preset::kNutMass,       Preset::kCd};
constexpr std::array<AnyTypedKey, 8> kStreamerColumns{
    Preset::kLegacy, Preset::kManufacturer, Preset::kPartNo,    Preset::kDescription,
    Preset::kLength, Preset::kWidth,        Preset::kThickness, Preset::kMaterial};
constexpr std::array<AnyTypedKey, 16> kParachuteColumns{
    Preset::kLegacy,       Preset::kManufacturer, Preset::kPartNo,         Preset::kDescription,
    Preset::kCanopyShape,  Preset::kDiameter,     Preset::kSpillDia,       Preset::kSurfaceArea,
    Preset::kMaterial,     Preset::kSides,        Preset::kLineCount,      Preset::kLineLength,
    Preset::kLineMaterial, Preset::kCd,           Preset::kPackedDiameter, Preset::kPackedLength};

/// The bytes a java.io.DataOutputStream writes, for computeDigest().
class DataOutput
{
public:
    /// writeBytes(String): the low byte of each UTF-16 code unit of the String @p text stands
    /// for (so every character above U+00FF loses its high byte, as in Java).
    void writeBytes(std::string_view text)
    {
        for (const char16_t unit : Strings::toUtf16(text))
        {
            m_bytes.push_back(static_cast<std::byte>(unit & 0xFFU));
        }
    }

    /// writeBoolean: one byte, 1 or 0.
    void writeBoolean(bool value) { m_bytes.push_back(value ? std::byte{1} : std::byte{0}); }

    /// writeInt: four bytes, big-endian.
    void writeInt(int value) { writeBigEndian(static_cast<std::uint32_t>(value), 4); }

    /// writeDouble: the eight bytes of Double.doubleToLongBits, big-endian (every NaN written as
    /// the canonical 0x7ff8000000000000).
    void writeDouble(double value)
    {
        constexpr std::uint64_t kCanonicalNaN = 0x7ff8000000000000ULL;
        writeBigEndian(std::isnan(value) ? kCanonicalNaN : std::bit_cast<std::uint64_t>(value), 8);
    }

    [[nodiscard]] std::span<const std::byte> bytes() const noexcept { return m_bytes; }

private:
    void writeBigEndian(std::uint64_t value, int byteCount)
    {
        for (int shift = (byteCount - 1) * 8; shift >= 0; shift -= 8)
        {
            m_bytes.push_back(
                static_cast<std::byte>((value >> static_cast<unsigned>(shift)) & 0xFFU));
        }
    }

    std::vector<std::byte> m_bytes;
};

/// Writes a property's value the way computeDigest() does.
struct DigestWriter
{
    DataOutput* out;

    void operator()(bool value) const { out->writeBoolean(value); }
    // Java's computeDigest has no branch for Integer: only the key's name is written.
    void operator()(int /*value*/) const { }
    void operator()(double value) const { out->writeDouble(value); }
    void operator()(const std::string& value) const { out->writeBytes(value); }
    void operator()(const ManufacturerRef& value) const
    {
        out->writeBytes(value.get().getSimpleName());
    }
    void operator()(ComponentPresetType value) const
    {
        out->writeBytes(componentPresetTypeName(value));
    }
    // "this is ugly to use the ordinal but what else?" (OpenRocket)
    void operator()(TransitionShape value) const { out->writeInt(static_cast<int>(value)); }
    void operator()(const Material& value) const { out->writeDouble(value.getDensity()); }
    void operator()(Finish value) const { out->writeBytes(finishName(value)); }
    // Nor for byte[]: the image counts by its name only.
    void operator()(const std::vector<std::byte>& /*value*/) const { }
};

}  // namespace

std::string_view componentPresetTypeName(ComponentPresetType type) noexcept
{
    switch (type)
    {
        case ComponentPresetType::BODY_TUBE:
            return "BODY_TUBE";
        case ComponentPresetType::NOSE_CONE:
            return "NOSE_CONE";
        case ComponentPresetType::TRANSITION:
            return "TRANSITION";
        case ComponentPresetType::TUBE_COUPLER:
            return "TUBE_COUPLER";
        case ComponentPresetType::BULK_HEAD:
            return "BULK_HEAD";
        case ComponentPresetType::CENTERING_RING:
            return "CENTERING_RING";
        case ComponentPresetType::ENGINE_BLOCK:
            return "ENGINE_BLOCK";
        case ComponentPresetType::LAUNCH_LUG:
            return "LAUNCH_LUG";
        case ComponentPresetType::RAIL_BUTTON:
            return "RAIL_BUTTON";
        case ComponentPresetType::STREAMER:
            return "STREAMER";
        case ComponentPresetType::PARACHUTE:
            return "PARACHUTE";
    }
    return "BODY_TUBE";
}

std::optional<ComponentPresetType> componentPresetTypeFromName(std::string_view name) noexcept
{
    for (const ComponentPresetType type : kAllComponentPresetTypes)
    {
        if (componentPresetTypeName(type) == name)
        {
            return type;
        }
    }
    return std::nullopt;
}

ComponentKind componentKind(ComponentPresetType type) noexcept
{
    switch (type)
    {
        case ComponentPresetType::BODY_TUBE:
            return ComponentKind::BODY_TUBE;
        case ComponentPresetType::NOSE_CONE:
            return ComponentKind::NOSE_CONE;
        case ComponentPresetType::TRANSITION:
            return ComponentKind::TRANSITION;
        case ComponentPresetType::TUBE_COUPLER:
            return ComponentKind::TUBE_COUPLER;
        case ComponentPresetType::BULK_HEAD:
            return ComponentKind::BULKHEAD;
        case ComponentPresetType::CENTERING_RING:
            return ComponentKind::CENTERING_RING;
        case ComponentPresetType::ENGINE_BLOCK:
            return ComponentKind::ENGINE_BLOCK;
        case ComponentPresetType::LAUNCH_LUG:
            return ComponentKind::LAUNCH_LUG;
        case ComponentPresetType::RAIL_BUTTON:
            return ComponentKind::RAIL_BUTTON;
        case ComponentPresetType::STREAMER:
            return ComponentKind::STREAMER;
        case ComponentPresetType::PARACHUTE:
            return ComponentKind::PARACHUTE;
    }
    return ComponentKind::BODY_TUBE;
}

std::optional<ComponentPresetType> presetTypeOf(ComponentKind kind) noexcept
{
    switch (kind)
    {
        case ComponentKind::BODY_TUBE:
        case ComponentKind::INNER_TUBE:
        case ComponentKind::TUBE_FIN_SET:
            return ComponentPresetType::BODY_TUBE;
        case ComponentKind::NOSE_CONE:
            return ComponentPresetType::NOSE_CONE;
        case ComponentKind::TRANSITION:
            return ComponentPresetType::TRANSITION;
        case ComponentKind::TUBE_COUPLER:
            return ComponentPresetType::TUBE_COUPLER;
        case ComponentKind::BULKHEAD:
            return ComponentPresetType::BULK_HEAD;
        case ComponentKind::CENTERING_RING:
            return ComponentPresetType::CENTERING_RING;
        case ComponentKind::ENGINE_BLOCK:
            return ComponentPresetType::ENGINE_BLOCK;
        case ComponentKind::LAUNCH_LUG:
            return ComponentPresetType::LAUNCH_LUG;
        case ComponentKind::RAIL_BUTTON:
            return ComponentPresetType::RAIL_BUTTON;
        case ComponentKind::STREAMER:
            return ComponentPresetType::STREAMER;
        case ComponentKind::PARACHUTE:
            return ComponentPresetType::PARACHUTE;
        case ComponentKind::ROCKET:
        case ComponentKind::AXIAL_STAGE:
        case ComponentKind::PARALLEL_STAGE:
        case ComponentKind::POD_SET:
        case ComponentKind::TRAPEZOID_FIN_SET:
        case ComponentKind::ELLIPTICAL_FIN_SET:
        case ComponentKind::FREEFORM_FIN_SET:
        case ComponentKind::MASS_COMPONENT:
        case ComponentKind::SHOCK_CORD:
            return std::nullopt;
    }
    return std::nullopt;
}

std::span<const AnyTypedKey> ComponentPreset::allKeys() noexcept
{
    return kAllKeys;
}

std::span<const AnyTypedKey> ComponentPreset::orderedKeyList() noexcept
{
    return kOrderedKeyList;
}

std::span<const AnyTypedKey> ComponentPreset::displayedColumns(Type type) noexcept
{
    switch (type)
    {
        case Type::BODY_TUBE:
        case Type::CENTERING_RING:
        case Type::ENGINE_BLOCK:
        case Type::LAUNCH_LUG:
            return kBodyTubeColumns;
        case Type::NOSE_CONE:
            return kNoseConeColumns;
        case Type::TRANSITION:
            return kTransitionColumns;
        case Type::TUBE_COUPLER:
            return kTubeCouplerColumns;
        case Type::BULK_HEAD:
            return kBulkHeadColumns;
        case Type::RAIL_BUTTON:
            return kRailButtonColumns;
        case Type::STREAMER:
            return kStreamerColumns;
        case Type::PARACHUTE:
            return kParachuteColumns;
    }
    return kBodyTubeColumns;
}

std::optional<bool> ComponentPreset::getLegacy() const
{
    const bool* legacy = m_properties.get(kLegacy);
    if (legacy == nullptr)
    {
        return std::nullopt;
    }
    return *legacy;
}

int ComponentPreset::compareTo(const ComponentPreset& other) const
{
    const int manuCompare = Strings::javaCompareTo(getManufacturer().getSimpleName(),
                                                   other.getManufacturer().getSimpleName());
    if (manuCompare != 0)
    {
        return manuCompare;
    }
    return Strings::javaCompareTo(getPartNo(), other.getPartNo());
}

std::string ComponentPreset::preferenceKey() const
{
    return getManufacturer().toString() + "|" + getPartNo();
}

std::string ComponentPreset::toOrkElement() const
{
    return std::format(R"(<preset type="{}" manufacturer="{}" partno="{}" digest="{}"/>)",
                       componentPresetTypeName(getType()),
                       Strings::escapeXml(getManufacturer().getSimpleName()),
                       Strings::escapeXml(getPartNo()), m_digest);
}

bool ComponentPreset::matches(std::string_view manufacturer, std::string_view partNo) const
{
    return getManufacturer().matches(manufacturer) && getPartNo() == partNo;
}

int ComponentPreset::hashCode() const noexcept
{
    return Strings::javaHashCode(m_digest);
}

void ComponentPreset::computeDigest()
{
    std::vector<const TypedPropertyMap::Entry*> entries;
    entries.reserve(m_properties.size());
    for (const TypedPropertyMap::Entry& entry : m_properties.entries())
    {
        entries.push_back(&entry);
    }
    // keys.sort(by getName().compareTo); the names are distinct, so the order is total.
    std::ranges::sort(entries,
                      [](const TypedPropertyMap::Entry* a, const TypedPropertyMap::Entry* b) {
                          return Strings::javaCompareTo(a->key.getName(), b->key.getName()) < 0;
                      });

    DataOutput out;
    for (const TypedPropertyMap::Entry* entry : entries)
    {
        if (entry->key == kLegacy)
        {
            continue;
        }
        out.writeBytes(entry->key.getName());
        std::visit(DigestWriter{.out = &out}, entry->value);
    }
    const Md5::Digest digest = md5(out.bytes());
    m_digest                 = Strings::hexString(digest);
}

void ComponentPreset::missingKey(const AnyTypedKey& key) const
{
    bug(std::format("Preset did not contain key {} {}", key.toString(), m_properties.toString()));
}

}  // namespace QtRocket
