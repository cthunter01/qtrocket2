#include "QtRocket/material/Material.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// String.split("\\|", 5): at most five fields, the last one keeping any further separators,
/// and trailing empty fields kept (the limit is positive).
[[nodiscard]] std::vector<std::string_view> splitStorable(std::string_view str)
{
    constexpr std::size_t         kLimit = 5;
    std::vector<std::string_view> fields;
    std::size_t                   start = 0;
    while (fields.size() + 1 < kLimit)
    {
        const std::size_t bar = str.find('|', start);
        if (bar == std::string_view::npos)
        {
            break;
        }
        fields.push_back(str.substr(start, bar - start));
        start = bar + 1;
    }
    fields.push_back(str.substr(start));
    return fields;
}

}  // namespace

Material::Material(Type type, std::string name, double density, double inPlaneShearModulus,
                   std::optional<MaterialGroup> group, bool userDefined, bool documentMaterial)
  : m_type(type),
    m_name(std::move(name)),
    m_density(density),
    m_inPlaneShearModulus(inPlaneShearModulus),
    m_group(group.value_or(userDefined ? MaterialGroup::CUSTOM : MaterialGroup::OTHER)),
    m_userDefined(userDefined),
    m_documentMaterial(documentMaterial)
{
}

Material Material::newMaterial(Type type, std::string name, double density,
                               double inPlaneShearModulus, std::optional<MaterialGroup> group,
                               bool userDefined, bool documentMaterial)
{
    return {type,  std::move(name), density,         inPlaneShearModulus,
            group, userDefined,     documentMaterial};
}

Material Material::newMaterial(Type type, std::string name, double density,
                               std::optional<MaterialGroup> group, bool userDefined,
                               bool documentMaterial)
{
    return newMaterial(type, std::move(name), density, 0.0, group, userDefined, documentMaterial);
}

Material Material::newMaterial(Type type, std::string name, double density,
                               std::optional<MaterialGroup> group, bool userDefined)
{
    return newMaterial(type, std::move(name), density, 0.0, group, userDefined, false);
}

Material Material::newMaterial(Type type, std::string name, double density,
                               double inPlaneShearModulus, std::optional<MaterialGroup> group,
                               bool userDefined)
{
    return newMaterial(type, std::move(name), density, inPlaneShearModulus, group, userDefined,
                       false);
}

Material Material::newMaterial(Type type, std::string name, double density,
                               double inPlaneShearModulus, bool userDefined, bool documentMaterial)
{
    return newMaterial(type, std::move(name), density, inPlaneShearModulus, std::nullopt,
                       userDefined, documentMaterial);
}

Material Material::newMaterial(Type type, std::string name, double density, bool userDefined,
                               bool documentMaterial)
{
    return newMaterial(type, std::move(name), density, 0.0, std::nullopt, userDefined,
                       documentMaterial);
}

Material Material::newMaterial(Type type, std::string name, double density, bool userDefined)
{
    return newMaterial(type, std::move(name), density, 0.0, std::nullopt, userDefined, false);
}

std::string Material::getName(const Unit& unit) const
{
    return m_name + " (" + unit.toStringUnit(m_density) + ")";
}

int Material::getGroupPriority() const noexcept
{
    return priority(m_group);
}

std::string Material::toString() const
{
    return getName(unitGroup(unitGroupId(m_type)).getDefaultUnit());
}

void Material::loadFrom(const Material& other)
{
    if (m_type != other.m_type)
    {
        bug("Material type mismatch");
    }
    m_name                = other.m_name;
    m_density             = other.m_density;
    m_inPlaneShearModulus = other.m_inPlaneShearModulus;
    m_group               = other.m_group;
    m_userDefined         = other.m_userDefined;
    m_documentMaterial    = other.m_documentMaterial;
}

bool Material::operator==(const Material& other) const noexcept
{
    return m_type == other.m_type && other.m_name == m_name &&
           MathUtil::equals(other.m_density, m_density) &&
           MathUtil::equals(other.m_inPlaneShearModulus, m_inPlaneShearModulus) &&
           m_group == other.m_group;
}

int Material::compareTo(const Material& other) const noexcept
{
    const int c = Strings::javaCompareTo(m_name, other.m_name);
    if (c != 0)
    {
        return c;
    }
    return MathUtil::javaIntCast((m_density - other.m_density) * 1000);
}

int Material::hashCode() const noexcept
{
    // The three terms are added with Java's wrapping int arithmetic.
    const auto sum =
        static_cast<std::uint32_t>(Strings::javaHashCode(m_name)) +
        static_cast<std::uint32_t>(MathUtil::javaIntCast(m_density * 1000)) +
        static_cast<std::uint32_t>(MathUtil::javaIntCast(m_inPlaneShearModulus * 1e-9));
    return static_cast<int>(sum);
}

std::string Material::toStorableString() const
{
    std::string name = m_name;
    for (char& c : name)
    {
        if (c == '|')
        {
            c = ' ';
        }
    }
    return std::format("{}|{}|{}|{}|{}", QtRocket::toString(m_type), name,
                       Strings::javaDoubleToString(m_density),
                       Strings::javaDoubleToString(m_inPlaneShearModulus), databaseString(m_group));
}

Result<Material> Material::fromStorableString(std::string_view str, bool userDefined)
{
    return fromStorableString(str, userDefined, nullptr);
}

Result<Material> Material::fromStorableString(std::string_view str, bool userDefined,
                                              const MaterialStorage& storage)
{
    return fromStorableString(str, userDefined, &storage);
}

Result<Material> Material::fromStorableString(std::string_view str, bool userDefined,
                                              const MaterialStorage* storage)
{
    const std::vector<std::string_view> split = splitStorable(str);
    if (split.size() < 3)
    {
        return fail(ErrorCode::PARSE, std::format("Illegal material string: {}", str));
    }

    const std::optional<Type> type = materialTypeFromString(split[0]);
    if (!type)
    {
        return fail(ErrorCode::PARSE, std::format("Illegal material string: {}", str));
    }

    const std::string_view name = split[1];

    const std::optional<double> density = Strings::javaParseDouble(split[2]);
    if (!density)
    {
        return fail(ErrorCode::PARSE, std::format("Illegal material string: {}", str));
    }

    // The legacy group name is resolved through the storage when there is one; without one,
    // "ThreadsLines" gives OTHER as a lookup that finds nothing does. An unknown group is left
    // unset, as OpenRocket logs it and goes on.
    const auto group = [&](std::string_view groupString) -> std::optional<MaterialGroup> {
        if (storage != nullptr)
        {
            return storage->materialGroupFromLegacyDatabaseString(groupString, *type, name,
                                                                  *density);
        }
        if (groupString == "ThreadsLines")
        {
            return MaterialGroup::OTHER;
        }
        return materialGroupFromDatabaseString(groupString);
    };

    double                       inPlaneShearModulus = 0.0;
    std::optional<MaterialGroup> materialGroup;

    // Handle backward compatibility: old format has 4 fields (type|name|density|group)
    // new format has 5 fields (type|name|density|inPlaneShearModulus|group)
    if (split.size() >= 4)
    {
        if (const std::optional<double> shear = Strings::javaParseDouble(split[3]))
        {
            inPlaneShearModulus = *shear;
            if (split.size() == 5)
            {
                materialGroup = group(split[4]);
            }
        }
        else
        {
            // 4th field is not a number, so it must be the group (old format)
            materialGroup = group(split[3]);
        }
    }

    switch (*type)
    {
        case Type::BULK:
        case Type::SURFACE:
        case Type::LINE:
            return Material(*type, std::string(name), *density, inPlaneShearModulus, materialGroup,
                            userDefined, false);
        case Type::CUSTOM:
            break;
    }
    return fail(ErrorCode::PARSE, std::format("Illegal material string: {}", str));
}

std::string_view toString(Material::Type type) noexcept
{
    switch (type)
    {
        case Material::Type::BULK:
            return "BULK";
        case Material::Type::SURFACE:
            return "SURFACE";
        case Material::Type::LINE:
            return "LINE";
        case Material::Type::CUSTOM:
            return "CUSTOM";
    }
    return "BULK";
}

std::optional<Material::Type> materialTypeFromString(std::string_view name) noexcept
{
    for (const Material::Type type : Material::kAllTypes)
    {
        if (toString(type) == name)
        {
            return type;
        }
    }
    return std::nullopt;
}

std::string_view displayKey(Material::Type type) noexcept
{
    switch (type)
    {
        case Material::Type::BULK:
            return "Databases.materials.types.Bulk";
        case Material::Type::SURFACE:
            return "Databases.materials.types.Surface";
        case Material::Type::LINE:
            return "Databases.materials.types.Line";
        case Material::Type::CUSTOM:
            return "Databases.materials.types.Custom";
    }
    return "Databases.materials.types.Bulk";
}

std::string_view displayName(Material::Type type) noexcept
{
    switch (type)
    {
        case Material::Type::BULK:
            return "Bulk";
        case Material::Type::SURFACE:
            return "Surface";
        case Material::Type::LINE:
            return "Line";
        case Material::Type::CUSTOM:
            return "Custom";
    }
    return "Bulk";
}

UnitGroupId unitGroupId(Material::Type type) noexcept
{
    switch (type)
    {
        case Material::Type::BULK:
        case Material::Type::CUSTOM:
            return UnitGroupId::DENSITY_BULK;
        case Material::Type::SURFACE:
            return UnitGroupId::DENSITY_SURFACE;
        case Material::Type::LINE:
            return UnitGroupId::DENSITY_LINE;
    }
    return UnitGroupId::DENSITY_BULK;
}

}  // namespace QtRocket
