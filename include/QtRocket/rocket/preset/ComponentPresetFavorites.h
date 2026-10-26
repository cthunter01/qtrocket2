#pragma once

#include <functional>
#include <set>
#include <string>

#include "QtRocket/rocket/preset/ComponentPresetType.h"

namespace QtRocket
{

class ComponentPreset;
class Preferences;

/// The favourite component presets: the ones the preset combo box of a component's edit dialog
/// offers, ticked in the parts library (OpenRocket: SwingPreferences.setComponentFavorite() and
/// getComponentFavorites(), SwingPreferences.java:1011-1033, the abstract methods of
/// ApplicationPreferences that only the Swing GUI implements, and
/// ComponentPresetDao.setFavorite()), as free functions on a Preferences: ComponentPreset sits
/// above preferences/, so Preferences cannot name it (as material/MaterialPreferences.h does for
/// materials).
///
/// The store: the node "favoritePresets" (PreferenceKeys::kFavoritePresetsNode) has a child node
/// per preset type, named by its constant name ("BODY_TUBE", componentPresetTypeName()), and
/// that node holds a favourite's ComponentPreset::preferenceKey() ("Estes|BT-20": the
/// manufacturer's display name, '|', the part number) with the value "true". The key is not the
/// preset's digest, so every preset of a manufacturer and part number shares one favourite (the
/// two lengths of a tube, say). A key may contain '/' ("LOC/Precision|LT-1.52"): QtRocket's stores
/// keep it (SettingsPreferences escapes it in the file).
///
/// No change signal: ComponentPresetDao.setFavorite() also fires the database's add event, which
/// OpenRocket's preset combo listens to; QtRocket's GUI reads the favourites again after the parts
/// library closes and on the rocket's events instead (plan decision O-M3-12).
///
/// Deviation: java.util.prefs refuses a key over 80 characters or with the character U+0000
/// (IllegalArgumentException from setComponentFavorite(), which reaches OpenRocket's error
/// dialog from the parts library and stores nothing; removing such a key works). No bundled
/// preset has such a key (the longest has 56 characters); a user's preset file may, and
/// QtRocket's stores keep it, so it becomes a favourite like any other.
namespace ComponentPresetFavorites
{

/// The keys of the favourites of one type, sorted; a std::less<> set, so that a std::string_view
/// can be looked up without a copy (Java: a HashSet<String>).
using FavoriteKeys = std::set<std::string, std::less<>>;

/// SwingPreferences.getComponentFavorites(type): every key of the node
/// "favoritePresets/<TYPE>", whatever its value (Java collects the node's keys(), so a key stored
/// as "false" by hand counts too). None when the node does not exist; the node is not created
/// (Java's node() makes it).
[[nodiscard]] FavoriteKeys getFavorites(const Preferences& preferences, ComponentPresetType type);

/// Whether @p preset is a favourite of @p type: getFavorites(preferences, type) holds its
/// preferenceKey(), as OpenRocket's parts library tests it (ComponentPresetTableColumn.Favorite).
/// Creates nothing.
[[nodiscard]] bool isFavorite(const Preferences& preferences, const ComponentPreset& preset,
                              ComponentPresetType type);

/// SwingPreferences.setComponentFavorite(preset, type, favorite): stores "true" under
/// @p preset's preferenceKey() in the node "favoritePresets/<TYPE>" (both nodes created as
/// needed), or removes the key when @p favorite is false (nothing happens when it is absent; an
/// emptied node stays). @p type names the node; OpenRocket passes the parts library's type, which
/// is @p preset's own type for every preset it lists.
void setFavorite(Preferences& preferences, const ComponentPreset& preset, ComponentPresetType type,
                 bool favorite);

}  // namespace ComponentPresetFavorites

}  // namespace QtRocket
