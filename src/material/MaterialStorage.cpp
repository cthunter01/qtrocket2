#include "QtRocket/material/MaterialStorage.h"

#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialDatabase.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

MaterialStorage::MaterialStorage()
{
    // Databases adds its storage listener to the line, surface and bulk databases.
    listenTo(m_line);
    listenTo(m_surface);
    listenTo(m_bulk);
}

void MaterialStorage::listenTo(MaterialDatabase& database)
{
    database.materialAdded.connect(
        [this](const Material& material, const MaterialDatabase& /*source*/) {
            if (material.isUserDefined())
            {
                userMaterialAdded.emit(material);
            }
        });
    database.materialRemoved.connect(
        [this](const Material& material, const MaterialDatabase& /*source*/) {
            userMaterialRemoved.emit(material);
        });
}

MaterialDatabase& MaterialStorage::database(Material::Type type)
{
    switch (type)
    {
        case Material::Type::BULK:
            return m_bulk;
        case Material::Type::SURFACE:
            return m_surface;
        case Material::Type::LINE:
            return m_line;
        case Material::Type::CUSTOM:
            break;
    }
    bug(std::format("Illegal material type: {}", toString(type)));
}

const MaterialDatabase& MaterialStorage::database(Material::Type type) const
{
    switch (type)
    {
        case Material::Type::BULK:
            return m_bulk;
        case Material::Type::SURFACE:
            return m_surface;
        case Material::Type::LINE:
            return m_line;
        case Material::Type::CUSTOM:
            break;
    }
    bug(std::format("Illegal material type: {}", toString(type)));
}

Material MaterialStorage::findMaterial(Material::Type type, std::string_view name, double density,
                                       double                       inPlaneShearModulus,
                                       std::optional<MaterialGroup> group) const
{
    const MaterialDatabase& db = database(type);
    for (const Material& m : db)
    {
        // Material group comparison is omitted to keep compatibility with files pre OR 24.12
        if (Strings::equalsIgnoreAsciiCase(m.getName(), name) &&
            MathUtil::equals(m.getDensity(), density) &&
            MathUtil::equals(m.getInPlaneShearModulus(), inPlaneShearModulus))
        {
            return m;
        }
    }
    return Material::newMaterial(type, std::string(name), density, inPlaneShearModulus, group, true,
                                 true);
}

Material MaterialStorage::findMaterial(Material::Type type, std::string_view name, double density,
                                       std::optional<MaterialGroup> group) const
{
    const MaterialDatabase& db = database(type);
    for (const Material& m : db)
    {
        // Backward-compatible lookup: match by name and density only.
        if (Strings::equalsIgnoreAsciiCase(m.getName(), name) &&
            MathUtil::equals(m.getDensity(), density))
        {
            return m;
        }
    }
    return Material::newMaterial(type, std::string(name), density, 0.0, group, true, true);
}

Material MaterialStorage::findMaterial(Material::Type type, std::string_view name,
                                       double density) const
{
    return findMaterial(type, name, density, std::nullopt);
}

std::optional<Material> MaterialStorage::findMaterial(Material::Type   type,
                                                      std::string_view name) const
{
    const MaterialDatabase& db = database(type);
    for (const Material& m : db)
    {
        if (Strings::equalsIgnoreAsciiCase(m.getName(), name))
        {
            return m;
        }
    }
    return std::nullopt;
}

bool MaterialStorage::addMaterial(const Material& material)
{
    return database(material.getType()).add(material);
}

bool MaterialStorage::removeMaterial(const Material& material)
{
    return database(material.getType()).remove(material);
}

MaterialDatabase MaterialStorage::allMaterials() const
{
    MaterialDatabase all;
    all.addAll(m_bulk);
    all.addAll(m_surface);
    all.addAll(m_line);
    return all;
}

std::size_t MaterialStorage::materialCount(Material::Type type) const
{
    return database(type).size();
}

std::size_t MaterialStorage::totalMaterialCount() const noexcept
{
    return m_bulk.size() + m_surface.size() + m_line.size();
}

std::optional<MaterialGroup> MaterialStorage::materialGroupFromLegacyDatabaseString(
    std::string_view groupString, Material::Type type, std::string_view materialName,
    double density) const
{
    // Handle backward compatibility for "ThreadsLines"
    if (groupString == "ThreadsLines")
    {
        if (type != Material::Type::CUSTOM)
        {
            // Search the database for the material by name and density
            for (const Material& m : database(type))
            {
                if (Strings::equalsIgnoreAsciiCase(m.getName(), materialName) &&
                    MathUtil::equals(m.getDensity(), density))
                {
                    const MaterialGroup foundGroup = m.getGroup();
                    // Check if the material belongs to one of the groups that replaced ThreadsLines
                    if (foundGroup == MaterialGroup::ELASTICS ||
                        foundGroup == MaterialGroup::KEVLARS || foundGroup == MaterialGroup::NYLONS)
                    {
                        return foundGroup;
                    }
                }
            }
        }
        // Material not found in ELASTICS, KEVLARS, or NYLONS, return OTHER
        return MaterialGroup::OTHER;
    }
    // For all other groups, use the standard loading method
    return materialGroupFromDatabaseString(groupString);
}

}  // namespace QtRocket
