#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QString>

#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/util/Error.h"

namespace QtRocket::Gui
{

/// The user's preferences, kept in an INI file through QSettings (OpenRocket: the store part of
/// SwingPreferences, over java.util.prefs). Everything typed is the core's Preferences; this
/// class is the store under it, and the store of every GUI-only key (GuiPreferenceKeys.h).
///
/// The file: QSettings' INI format on every operating system (never the Windows registry or a
/// macOS property list), at the place its owner names. The application opens the user's file
/// with openDefault(), the one way to it (QSettings' place for the organisation and the
/// application "QtRocket": ~/.config/QtRocket/QtRocket.ini on Linux); a test passes a file of
/// its own. The file is written when QSettings syncs: in the event loop of the thread that made
/// the store, soon after a change, on sync(), and when the store is destroyed. A file that does
/// not exist is an empty store, and reading never creates it.
///
/// Store model: a node is a QSettings group (its path relative to the file's top), its keys are
/// the group's keys and its child nodes the group's subgroups, so the file reads like the
/// java.util.prefs tree, key for key: the keys of the core are under the root (in the INI
/// file's [General] section), the nodes "units", "userMaterials", "windows", ... are sections.
/// The answers are those of InMemoryPreferences for the same calls:
/// - keys() and childrenNames() are sorted (byte order of their UTF-8 text, as std::string).
/// - Every value round-trips, the empty one included: a value that is not UTF-8 is stored as
///   QSettings' byte array. Every key and node name that is UTF-8 text round-trips, the empty
///   key included: QSettings takes '/' and '\\' in a name as a group separator and drops an
///   empty key, so the names are escaped in the file: '%' as "%25", '/' as "%2F", '\\' as "%5C"
///   and the empty name as "%". (QSettings escapes other characters of its own, which the file
///   shows as %XX or %UXXXX.) A name that is not UTF-8 is a programming error the store does
///   not check: its bytes that are not UTF-8 become U+FFFD.
/// - A key and a child node may have the same name, as in java.util.prefs (QSettings::remove()
///   of the one would remove the other too; this store puts the other back).
/// - A node made by getNode() exists until it is reset, even while it holds nothing; an INI
///   file has no empty sections, so such a node is not in the file and is gone when the file is
///   opened again (java.util.prefs keeps an empty node).
/// - Node objects are never destroyed before the store: a reference from getNode() or a pointer
///   from findNode() stays valid across reset() and restore() too (it then refers to the node of
///   that path again, empty until something is stored there).
/// What the user wrote into the file by hand is read as QSettings reads it: an unquoted value
/// with commas is a list, which reads back joined with ","; other types read as their text; a
/// section whose name is no node name (the empty name, or one with '/') is no child node; and a
/// name with a '%' that starts none of the escapes above is not shown (no name of the store is
/// stored so).
///
/// Deviations from SwingPreferences:
/// - No write on read: SwingPreferences' getString(), getBoolean(), getInt() and getDouble()
///   store the default when the key is absent (and flush the node); here an absent key gives the
///   default and stays absent, as in InMemoryPreferences, so the file holds only what was set.
/// - Every put of SwingPreferences also stores the key "OpenRocketVersion" (storeVersion(), for a
///   migration of the preferences nothing reads yet); this store keeps no version key.
/// - The store is not OpenRocket's: java.util.prefs' node "OpenRocket" is not read or imported
///   (OpenRocket's PreferencesImporter, which imports an exported XML file, is not ported).
/// - On Windows QSettings compares the keys of an INI file without regard to case, so two keys
///   that differ only in case would be one there; no two keys of OpenRocket or QtRocket do.
///
/// Threads: every operation takes one mutex for the whole store (one QSettings object holds the
/// group state of every node), and so does QSettings' event handler, in which QSettings writes
/// the file after a change (its deferred sync). So the store may be read and written from
/// several threads at once, as Preferences requires (Preferences, "Threads"); the store itself
/// belongs to the thread that made it, which must run an event loop for the deferred sync (or
/// call sync()), and is destroyed there.
class SettingsPreferences final : public Preferences
{
    /// The QSettings object and the mutex all nodes of a store share (SettingsPreferences.cpp).
    struct Store;

    /// Passkey of the child-node constructor, which is public for std::make_unique only.
    struct ChildKey
    {
        explicit ChildKey() = default;
    };

public:
    /// The store over the INI file @p fileName (a path in Qt's form, QtStrings::fromPath()). The
    /// file is read when the store is made; a file that does not exist is an empty store.
    explicit SettingsPreferences(const QString& fileName);
    /// A child node of a store (not for use outside this class: ChildKey is private).
    SettingsPreferences(ChildKey key, Store& store, QString group);
    ~SettingsPreferences() override;

    SettingsPreferences(const SettingsPreferences&)            = delete;
    SettingsPreferences& operator=(const SettingsPreferences&) = delete;
    SettingsPreferences(SettingsPreferences&&)                 = delete;
    SettingsPreferences& operator=(SettingsPreferences&&)      = delete;

    /// The user's preferences file: QSettings' INI file for the organisation and the application
    /// "QtRocket" in the user's scope (QSettings::setPath() moves it, as the GUI tests do).
    [[nodiscard]] static QString defaultFileName();
    /// The store over defaultFileName(): how the application opens the user's preferences.
    [[nodiscard]] static std::unique_ptr<SettingsPreferences> openDefault();

    [[nodiscard]] std::optional<std::string> get(std::string_view key) const override;
    void put(std::string_view key, std::string_view value) override;
    void remove(std::string_view key) override;
    void clear() override;
    [[nodiscard]] std::vector<std::string>   keys() const override;
    [[nodiscard]] std::vector<std::string>   childrenNames() const override;
    [[nodiscard]] SettingsPreferences&       getNode(std::string_view name) override;
    [[nodiscard]] const SettingsPreferences* findNode(
        std::string_view name) const noexcept override;

    /// Everything this node holds now, keys and child nodes recursively, as one state of the
    /// store (taken under its lock): for the preferences dialog's Cancel and for comparisons.
    [[nodiscard]] std::unique_ptr<InMemoryPreferences> snapshot() const;
    /// Replaces this node's keys and child nodes with those of @p snapshot (any store; usually a
    /// snapshot() taken before), recursively: afterwards snapshot() equals @p snapshot. As
    /// InMemoryPreferences' assignment, but the node objects stay (see the class comment).
    void restore(const Preferences& snapshot);
    /// Removes every key and every child node of this node, recursively: back to the factory
    /// state (InMemoryPreferences::reset()). On the root, the whole file is emptied.
    void reset();
    /// SwingPreferences.clearPreferences() ('Reset all preferences'): reset() of the whole store,
    /// then the process-wide default units are reset (UnitGroup::resetDefaultUnits()) and stored
    /// again (storeDefaultUnits()). Changes the default units, so for the GUI thread only.
    void clearPreferences();

    /// Writes every change to the file now (QSettings::sync()). An IO error when the file cannot
    /// be written (QSettings::AccessError).
    [[nodiscard]] Result<void> sync();
    /// Whether the file could be read and written so far: an IO error when it could not be
    /// written (QSettings::AccessError), a PARSE error when it is not an INI file QSettings
    /// reads whole (QSettings::FormatError: what could be read is kept, and the next write
    /// replaces the file with it).
    [[nodiscard]] Result<void> status() const;
    /// The INI file (in Qt's form).
    [[nodiscard]] QString fileName() const;

    /// The default component colours of the current theme (ThemeColors::defaultComponentColors()
    /// of the application palette's theme), as SwingPreferences fills DEFAULT_COLORS from the UI
    /// theme. On another thread than the application's, the theme it found there last (the
    /// palette belongs to the GUI thread); the light theme before it was asked there.
    [[nodiscard]] ComponentDefaults defaultComponentColors() const noexcept override;

private:
    using Children = std::map<std::string, std::unique_ptr<SettingsPreferences>, std::less<>>;

    /// The child node object @p name (an unescaped name), made when it does not exist yet. The
    /// caller holds the lock.
    [[nodiscard]] SettingsPreferences& childLocked(std::string_view name) const;
    /// Whether this node exists: made by getNode() since its last reset, or a group of the file.
    [[nodiscard]] bool existsLocked() const;
    /// The names of the child nodes that exist, sorted. The caller holds the lock.
    [[nodiscard]] std::vector<std::string> childrenNamesLocked() const;
    /// The node at @p name (a '/'-separated path) or nullptr, never made. The caller holds the
    /// lock.
    [[nodiscard]] const SettingsPreferences* findNodeLocked(std::string_view name) const;
    /// get(), put(), remove() with the lock held.
    [[nodiscard]] std::optional<std::string> getLocked(std::string_view key) const;
    void putLocked(std::string_view key, std::string_view value);
    void removeLocked(std::string_view key);
    /// reset() with the lock held.
    void resetLocked();
    /// Forgets that the child nodes were made by getNode(), recursively (after a reset).
    void forgetChildrenLocked();
    /// Copies this node's keys and child nodes into @p target. The caller holds the lock.
    void snapshotLocked(InMemoryPreferences& target) const;
    /// Stores the keys and child nodes of @p source here (which is empty). The caller holds the
    /// lock.
    void restoreLocked(const InMemoryPreferences& source);

    /// The store of the root (null in a child node), declared first: the child nodes go first.
    std::unique_ptr<Store> m_ownStore;
    /// The store all nodes share.
    Store* m_store;
    /// This node's group, the escaped names of the path from the root joined by '/' (empty for
    /// the root).
    QString m_group;
    /// Made by getNode() since the last reset of this node or of an ancestor (guarded by the
    /// store's mutex). The root always exists.
    bool m_made;
    /// The child node objects, by unescaped name (guarded by the store's mutex): every node
    /// asked for so far, whether it exists now or not.
    mutable Children m_children;
};

}  // namespace QtRocket::Gui
