#pragma once

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "QtRocket/rocket/preset/ComponentPresetType.h"

namespace QtRocket
{

class ComponentPreset;

/// The component presets an application knows (OpenRocket's ComponentPresetDatabase, a
/// Database<ComponentPreset> behind the ComponentPresetDao interface): the list the .ork loader
/// looks a <preset> element up in (find()) and the preset chooser lists by type.
///
/// The presets are held as std::shared_ptr<const ComponentPreset>, and a component based on a
/// preset co-owns it (RocketComponent::loadPreset(std::shared_ptr<const ComponentPreset>)): a
/// preset lives as long as the database or any component that uses it, also a copy of the
/// component in an undo snapshot or in the rocket copy a FlightData keeps. A copy of a database
/// shares its presets.
///
/// The list is kept the way Java keeps it. add() is Database.add(): a binary search by
/// ComponentPreset::compareTo() (the manufacturer's simple name, then the part number), the new
/// preset going to the index found or to the insertion point, so that presets added only with
/// add() are sorted, and a preset whose digest the list already holds is refused when the search
/// finds a preset that compares equal. Presets that compare equal but differ (two lengths of one
/// part number) are all kept, in an order that follows from the search, not from the order of
/// addition. insert() is ComponentPresetDao.insert(): it appends, whatever the list holds; add()
/// afterwards searches the list as it is, as Java does.
///
/// Deviations from OpenRocket:
/// - A null preset is a bug (BugError) in add(), addAll() and insert(); Java's add() dies of a
///   NullPointerException (or stores the null in an empty list), and its insert() stores it.
/// - listAll() gives the list read-only; Java hands out its own mutable list.
/// - listForType(null) and find(null, ...) give an empty list in Java; a ComponentPresetType and
///   a std::string_view cannot be null.
///
/// Not ported, until the preset chooser of Milestone 3 needs them: the favourites
/// (listForType(type, favorite) and setFavorite(), which read and write the application
/// preferences), the database listeners (DatabaseListener, Database.fireAddEvent() and
/// fireRemoveEvent()), and what else Database and AbstractSet offer (get(index), indexOf(),
/// contains(), remove(), clear() and removal through the iterator). Reading .orc preset files
/// is Milestone 3 too: until then only tests fill a database.
///
/// Not thread-safe: the thread that owns the database changes it; a loader only reads it.
class ComponentPresetDatabase
{
public:
    using PresetPtr = std::shared_ptr<const ComponentPreset>;

    /// Database.add(): adds @p preset at its place in the order (see the class comment); false,
    /// and nothing added, when the binary search finds a preset that compares equal and the list
    /// holds a preset with the same digest (ComponentPreset::operator==).
    /// @throws BugError when @p preset is null.
    bool add(PresetPtr preset);

    /// AbstractCollection.addAll(): add() of every preset of @p presets, in order; true when any
    /// was added. @p presets may be this database's own list.
    /// @throws BugError when a preset is null (the presets before it have been added).
    bool addAll(std::span<const PresetPtr> presets);

    /// ComponentPresetDao.insert(): appends @p preset to the list, without looking at the order
    /// or for duplicates.
    /// @throws BugError when @p preset is null.
    void insert(PresetPtr preset);

    /// Every preset, in the list's order (ComponentPresetDao.listAll()). The reference is valid
    /// until the database changes.
    [[nodiscard]] const std::vector<PresetPtr>& listAll() const noexcept { return m_list; }

    /// The presets of @p type, in the list's order (listForType(type)).
    [[nodiscard]] std::vector<PresetPtr> listForType(ComponentPresetType type) const;

    /// The presets whose type is one of @p types, in the list's order, each once
    /// (listForTypes(types)); none for no type.
    [[nodiscard]] std::vector<PresetPtr> listForTypes(
        std::span<const ComponentPresetType> types) const;
    [[nodiscard]] std::vector<PresetPtr> listForTypes(
        std::initializer_list<ComponentPresetType> types) const;

    /// The presets that ComponentPreset::matches(@p manufacturer, @p partNo) holds for, in the
    /// list's order (find()): @p manufacturer is any name of the preset's manufacturer, compared
    /// as Manufacturer::matches() does ("ESTES" and "es" find Estes), and @p partNo is the
    /// preset's part number exactly. The .ork loader then takes the first one whose digest is
    /// the file's.
    [[nodiscard]] std::vector<PresetPtr> find(std::string_view manufacturer,
                                              std::string_view partNo) const;

    /// The number of presets (Database.size()).
    [[nodiscard]] std::size_t size() const noexcept { return m_list.size(); }
    [[nodiscard]] bool        empty() const noexcept { return m_list.empty(); }

private:
    /// What Collections.binarySearch(list, key) returns: the index of a preset comparing equal
    /// to @p key (found), or the insertion point.
    struct SearchResult
    {
        std::size_t index{0};
        bool        found{false};
    };

    [[nodiscard]] SearchResult binarySearch(const ComponentPreset& key) const;

    /// ArrayList.contains(): whether a preset of the list equals @p preset (the same digest).
    [[nodiscard]] bool contains(const ComponentPreset& preset) const noexcept;

    std::vector<PresetPtr> m_list;
};

}  // namespace QtRocket
