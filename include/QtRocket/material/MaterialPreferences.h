#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{

class MaterialStorage;

// The Material-typed methods of OpenRocket's ApplicationPreferences and SwingPreferences, as free
// functions on a Preferences: material/ sits above preferences/ (plan 3.1), so Preferences keeps
// only their string halves (getDefaultComponentMaterialString() and friends) and these add the
// Material parsing, the type check and the built-in fallbacks.
//
// OpenRocket reaches its material databases through the global Databases class, both for the
// fallbacks and, inside Material.fromStorableString, to resolve the pre-24.12 group
// "ThreadsLines" (MaterialStorage::materialGroupFromLegacyDatabaseString). Here the databases are
// a MaterialStorage the caller passes; it must hold the built-in materials (addBuiltinMaterials())
// as Databases always does.

/// The built-in materials ApplicationPreferences falls back on (StaticFieldHolder.DEFAULT_*):
/// the names Databases.findMaterial looks up by type.
inline constexpr std::string_view kDefaultLineMaterialName = "Elastic cord (round 2 mm, 1/16 in)";
inline constexpr std::string_view kDefaultSurfaceMaterialName = "Ripstop nylon";
inline constexpr std::string_view kDefaultBulkMaterialName    = "Cardboard";

/// ApplicationPreferences.getDefaultComponentMaterial(Class, Material.Type): the first material
/// stored along @p classChain in the "componentMaterials" node
/// (Preferences::getDefaultComponentMaterialString()), parsed as a material that is not
/// user-defined (Material::fromStorableString(text, false, storage)), when it parses and has
/// @p type. Otherwise, including when the nearest stored material has another type or does not
/// parse (Java ignores the IllegalArgumentException and looks no further along the chain), the
/// built-in default of @p type found in @p storage by name (Databases.findMaterial): LINE
/// kDefaultLineMaterialName, SURFACE kDefaultSurfaceMaterialName, BULK kDefaultBulkMaterialName.
///
/// Java takes the component's Class and walks its superclasses; @p classChain is that walk, as
/// for the other per-class lookups of Preferences. Deviation: Java resolves the three fallbacks
/// once, at their first use (StaticFieldHolder); here every call looks them up in @p storage.
/// @throws BugError for the CUSTOM type (Java: IllegalArgumentException "Unknown material
///         type"), and when @p storage lacks the fallback material, which Java's Databases always
///         holds (a storage without the built-in materials is a programming error).
[[nodiscard]] Material getDefaultComponentMaterial(const Preferences&     preferences,
                                                   ComponentClassChain    classChain,
                                                   Material::Type         type,
                                                   const MaterialStorage& storage);

/// ApplicationPreferences.setDefaultComponentMaterial(Class, Material): stores
/// Material::toStorableString() under @p className (the component class's simple name) in the
/// "componentMaterials" node, or removes the entry for nullopt (Java: a null material).
void setDefaultComponentMaterial(Preferences& preferences, std::string_view className,
                                 const std::optional<Material>& material);

/// SwingPreferences.loadDefaultComponentMaterials(), which OpenRocket runs at every start: makes
/// the built-in "Balsa" the default material of FinSet and "Polystyrene" that of NoseCone (both
/// BULK, found in @p storage with Databases.findMaterial; a missing one removes the entry, as
/// Java's null does).
void loadDefaultComponentMaterials(Preferences& preferences, const MaterialStorage& storage);

// ---- the user material store (SwingPreferences) ----
//
// The user-defined materials live in the "userMaterials" node (PreferenceKeys::kUserMaterialsNode),
// one Material::toStorableString() per key "material0", "material1", ...; the key carries no
// meaning and is not used when loading.

/// SwingPreferences.getUserMaterials(): every stored material that parses, as a user-defined
/// material (Material::fromStorableString(text, true, storage)), in key order; strings that do
/// not parse are skipped (Java logs them). Java collects them in a HashSet, so a material equal to
/// an earlier one (Material::operator==) with the same Material::hashCode() is dropped; that
/// keeps two materials that are equal but hash differently, exactly as Java's set does. Does not
/// create the node.
[[nodiscard]] std::vector<Material> getUserMaterials(const Preferences&     preferences,
                                                     const MaterialStorage& storage);

/// SwingPreferences.addUserMaterial(): nothing when getUserMaterials() already holds a material
/// equal to @p material (with the same hash code, as HashSet.contains); otherwise stores its
/// storable string under the first key "material<i>" (i = 0, 1, ...) that holds nothing.
void addUserMaterial(Preferences& preferences, const Material& material,
                     const MaterialStorage& storage);

/// SwingPreferences.removeUserMaterial(): removes every stored material that parses to a material
/// equal to @p material (Material::operator==); strings that do not parse stay.
void removeUserMaterial(Preferences& preferences, const Material& material,
                        const MaterialStorage& storage);

/// The part of Databases' static initialiser after the built-in materials: adds every material of
/// getUserMaterials() to the database of its type in @p storage. Returns the number added (a
/// material equal to one already there is not added again).
std::size_t addUserMaterials(MaterialStorage& storage, const Preferences& preferences);

/// The listener Databases attaches last (OpenRocket's material/MaterialStorage class): from now
/// on a user-defined material added to a database of @p storage is stored with addUserMaterial()
/// and any material removed from one is forgotten with removeUserMaterial(), both in
/// @p preferences. Connect after addUserMaterials(), as Databases does. The connections last as
/// long as the returned handles; @p preferences must outlive them.
[[nodiscard]] std::array<Signal<const Material&>::ScopedConnection, 2> storeUserMaterialChanges(
    MaterialStorage& storage, Preferences& preferences);

}  // namespace QtRocket
