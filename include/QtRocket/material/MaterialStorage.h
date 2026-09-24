#pragma once

#include <array>
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
/// OpenRocket's built-in materials, and addUserMaterials() (MaterialPreferences.h) adds the
/// user's from the preferences.
///
/// OpenRocket's MaterialStorage is the DatabaseListener that writes user-defined materials to
/// the preferences as they are added and removed; that part is the userMaterialAdded and
/// userMaterialRemoved signals here (added fires only for user-defined materials, removed for
/// every removal, as there), which storeUserMaterialChanges() (MaterialPreferences.h) connects
/// to the preferences.
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
    /// @throws BugError for CUSTOM, which has none (OpenRocket:
    /// IllegalArgumentException)
    ///
    /// The databases are handed out by reference for adding and removing; the storage listens to
    /// them for as long as it lives. Moving a database out (MaterialDatabase's move constructor)
    /// takes its materials only and leaves the storage's database empty and still listened to.
    [[nodiscard]] MaterialDatabase&       database(Material::Type type);
    [[nodiscard]] const MaterialDatabase& database(Material::Type type) const;
    [[nodiscard]] MaterialDatabase&       bulkMaterials() noexcept { return m_bulk; }
    [[nodiscard]] const MaterialDatabase& bulkMaterials() const noexcept { return m_bulk; }
    [[nodiscard]] MaterialDatabase&       surfaceMaterials() noexcept { return m_surface; }
    [[nodiscard]] const MaterialDatabase& surfaceMaterials() const noexcept { return m_surface; }
    [[nodiscard]] MaterialDatabase&       lineMaterials() noexcept { return m_line; }
    [[nodiscard]] const MaterialDatabase& lineMaterials() const noexcept { return m_line; }

    /// Databases.findMaterial(type, baseName, density, shearModulus, group): the name is first
    /// translated (translatedMaterialName(): "PLA" becomes "PLA - 100% infill", " aluminum"
    /// becomes "Aluminum"); then a copy of the first material of @p type whose name equals it
    /// ignoring case (String.equalsIgnoreCase, Strings::javaEqualsIgnoreCase) and whose density
    /// and shear modulus match within MathUtil::equals (the group is not compared, for files
    /// older than OpenRocket 24.12), or else a new user-defined document material with the
    /// translated name and the given fields. This is how the .ork loader resolves a component's
    /// material.
    /// @throws BugError for the CUSTOM type
    [[nodiscard]] Material findMaterial(Material::Type type, std::string_view baseName,
                                        double density, double inPlaneShearModulus,
                                        std::optional<MaterialGroup> group) const;
    /// Databases.findMaterial(type, baseName, density, group): as above, matching by name and
    /// density only (the older lookup); the fallback material has shear modulus 0.
    /// @throws BugError for the CUSTOM type
    [[nodiscard]] Material findMaterial(Material::Type type, std::string_view baseName,
                                        double density, std::optional<MaterialGroup> group) const;
    /// findMaterial(type, baseName, density, nullopt).
    /// @throws BugError for the CUSTOM type
    [[nodiscard]] Material findMaterial(Material::Type type, std::string_view baseName,
                                        double density) const;
    /// Databases.findMaterial(type, baseName): a copy of the first material of @p type whose
    /// name equals the translated name ignoring case, as above, or nullopt (OpenRocket: null).
    /// @throws BugError for the CUSTOM type
    [[nodiscard]] std::optional<Material> findMaterial(Material::Type   type,
                                                       std::string_view baseName) const;

    /// Databases.addMaterial: adds to the database of the material's type; true when added.
    /// @throws BugError for a CUSTOM material
    bool addMaterial(const Material& material);
    /// Databases.removeMaterial; true when a material was removed.
    /// @throws BugError for a CUSTOM material
    bool removeMaterial(const Material& material);

    /// DocumentPreferences.getAllMaterials: a new database holding every material of the three
    /// (changes to it do not affect them).
    [[nodiscard]] MaterialDatabase allMaterials() const;
    /// DocumentPreferences.getMaterialCount. @throws BugError for CUSTOM
    [[nodiscard]] std::size_t materialCount(Material::Type type) const;
    /// DocumentPreferences.getTotalMaterialCount.
    [[nodiscard]] std::size_t totalMaterialCount() const noexcept;

    /// MaterialGroup.loadFromDatabaseStringWithBackwardCompatibility: "ThreadsLines", the
    /// pre-24.12 name of what became ELASTICS, KEVLARS and NYLONS, is resolved by looking the
    /// material up in the database of @p type by name (String.equalsIgnoreCase, untranslated)
    /// and density
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
    using Connection = Signal<const Material&, const MaterialDatabase&>::ScopedConnection;

    /// Forwards a user-defined material added to @p database to userMaterialAdded.
    [[nodiscard]] Connection forwardAdded(MaterialDatabase& database);
    /// Forwards a material removed from @p database to userMaterialRemoved.
    [[nodiscard]] Connection forwardRemoved(MaterialDatabase& database);

    MaterialDatabase m_bulk;
    MaterialDatabase m_surface;
    MaterialDatabase m_line;
    /// The storage's listeners on the three databases, cut when the storage is destroyed.
    std::array<Connection, 6> m_connections;
};

}  // namespace QtRocket
