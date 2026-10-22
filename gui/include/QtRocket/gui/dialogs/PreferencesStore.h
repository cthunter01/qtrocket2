#pragma once

#include <memory>

namespace QtRocket
{
class InMemoryPreferences;
class Preferences;
}  // namespace QtRocket

/// What the preferences dialog does with the store under the Preferences it edits, which the
/// Preferences interface does not offer: a snapshot of the whole store, its restoring, and the
/// reset of every preference. OpenRocket's dialog exports the java.util.prefs node to a file and
/// imports it again (PreferencesExporter, PreferencesImporter) and resets it with
/// SwingPreferences.clearPreferences(); here the store is the application's SettingsPreferences
/// or an InMemoryPreferences (the tests' store), and each of these functions does what that store
/// offers for it.
///
/// Deviation: restore() puts back exactly the snapshot, so a key or node made after the snapshot
/// is removed; java.util.prefs' importPreferences() merges the file into the store and keeps them
/// (probe: a key and a node added after the export are still there after the import).
namespace QtRocket::Gui::PreferencesStore
{

/// Everything @p store holds now, keys and child nodes.
/// @throws BugError for a store that is neither a SettingsPreferences nor an InMemoryPreferences
[[nodiscard]] std::unique_ptr<InMemoryPreferences> snapshot(const Preferences& store);

/// Replaces every key and child node of @p store with those of @p snapshot.
/// @throws BugError for a store of another kind (see snapshot())
void restore(Preferences& store, const InMemoryPreferences& snapshot);

/// SwingPreferences.clearPreferences() ("Reset all preferences"): every key and node of @p store
/// removed, then the process-wide default units reset (UnitGroup::resetDefaultUnits()) and
/// stored again (Preferences::storeDefaultUnits()). Changes the default units: GUI thread only.
/// @throws BugError for a store of another kind (see snapshot())
void clearPreferences(Preferences& store);

}  // namespace QtRocket::Gui::PreferencesStore
