#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialDatabase.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{

/// The three material databases, one per non-custom type, with OpenRocket's lookups on them
/// (Databases.java's static databases and methods, and DocumentPreferences' copies of them for a
/// document's own materials). An instance starts empty: addBuiltinMaterials() fills it with
/// OpenRocket's built-in materials, and the preferences layer adds the user's.
///
/// OpenRocket's MaterialStorage is the DatabaseListener that writes user-defined materials to
/// the preferences as they are added and removed; that part is the userMaterialAdded and
/// userMaterialRemoved signals here (added fires only for user-defined materials, removed for
/// every removal, as there), which the preferences layer connects to.
///
/// Not thread-safe; the GUI thread owns it.
class MaterialStorage
{
public:
    MaterialStorage();
    ~MaterialStorage()                                 = default;
    MaterialStorage(const MaterialStorage&)            = delete;
    MaterialStorage& operator=(const MaterialStorage&) = delete;
    MaterialStorage(MaterialStorage&&)                 = delete;
    MaterialStorage& operator=(MaterialStorage&&)      = delete;

    /// Databases.getDatabase: the database of @p type.
    /// @throws std::invalid_argument for CUSTOM, which has none (OpenRocket:
    /// IllegalArgumentException)
    [[nodiscard]] MaterialDatabase&       database(Material::Type type);
    [[nodiscard]] const MaterialDatabase& database(Material::Type type) const;
    [[nodiscard]] MaterialDatabase&       bulkMaterials() noexcept { return m_bulk; }
    [[nodiscard]] const MaterialDatabase& bulkMaterials() const noexcept { return m_bulk; }
    [[nodiscard]] MaterialDatabase&       surfaceMaterials() noexcept { return m_surface; }
    [[nodiscard]] const MaterialDatabase& surfaceMaterials() const noexcept { return m_surface; }
    [[nodiscard]] MaterialDatabase&       lineMaterials() noexcept { return m_line; }
    [[nodiscard]] const MaterialDatabase& lineMaterials() const noexcept { return m_line; }

    /// Databases.findMaterial(type, name, density, shearModulus, group): a copy of the first
    /// material of @p type whose name matches @p name ignoring ASCII case and whose density and
    /// shear modulus match within MathUtil::equals (the group is not compared, for files older
    /// than OpenRocket 24.12), or a new user-defined document material with the given fields
    /// when there is none. This is how the .ork loader resolves a component's material.
    /// @throws std::invalid_argument for the CUSTOM type
    [[nodiscard]] Material findMaterial(Material::Type type, std::string_view name, double density,
                                        double                       inPlaneShearModulus,
                                        std::optional<MaterialGroup> group) const;
    /// Databases.findMaterial(type, name, density, group): as above, matching by name and density
    /// only (the older lookup); the fallback material has shear modulus 0.
    /// @throws std::invalid_argument for the CUSTOM type
    [[nodiscard]] Material findMaterial(Material::Type type, std::string_view name, double density,
                                        std::optional<MaterialGroup> group) const;
    /// findMaterial(type, name, density, nullopt).
    /// @throws std::invalid_argument for the CUSTOM type
    [[nodiscard]] Material findMaterial(Material::Type type, std::string_view name,
                                        double density) const;
    /// Databases.findMaterial(type, name): a copy of the first material of @p type whose name
    /// matches ignoring ASCII case, or nullopt (OpenRocket: null).
    /// @throws std::invalid_argument for the CUSTOM type
    [[nodiscard]] std::optional<Material> findMaterial(Material::Type   type,
                                                       std::string_view name) const;

    /// Databases.addMaterial: adds to the database of the material's type; true when added.
    /// @throws std::invalid_argument for a CUSTOM material
    bool addMaterial(const Material& material);
    /// Databases.removeMaterial; true when a material was removed.
    /// @throws std::invalid_argument for a CUSTOM material
    bool removeMaterial(const Material& material);

    /// DocumentPreferences.getAllMaterials: a new database holding every material of the three
    /// (changes to it do not affect them).
    [[nodiscard]] MaterialDatabase allMaterials() const;
    /// DocumentPreferences.getMaterialCount. @throws std::invalid_argument for CUSTOM
    [[nodiscard]] std::size_t materialCount(Material::Type type) const;
    /// DocumentPreferences.getTotalMaterialCount.
    [[nodiscard]] std::size_t totalMaterialCount() const noexcept;

    /// MaterialGroup.loadFromDatabaseStringWithBackwardCompatibility: "ThreadsLines", the
    /// pre-24.12 name of what became ELASTICS, KEVLARS and NYLONS, is resolved by looking the
    /// material up in the database of @p type by name (ignoring ASCII case) and density
    /// (MathUtil::equals): the material's group when it is one of those three, OTHER otherwise
    /// (also for the CUSTOM type, whose lookup OpenRocket lets fail). Any other name goes through
    /// materialGroupFromDatabaseString(): nullopt when unknown.
    [[nodiscard]] std::optional<MaterialGroup> materialGroupFromLegacyDatabaseString(
        std::string_view groupString, Material::Type type, std::string_view materialName,
        double density) const;

    /// A user-defined material was added to one of the databases (the preferences store it).
    Signal<const Material&> userMaterialAdded;
    /// A material was removed from one of the databases (the preferences forget it).
    Signal<const Material&> userMaterialRemoved;

private:
    void listenTo(MaterialDatabase& database);

    MaterialDatabase m_bulk;
    MaterialDatabase m_surface;
    MaterialDatabase m_line;
};

}  // namespace QtRocket
