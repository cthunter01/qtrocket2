#pragma once

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "QtRocket/rocket/preset/ComponentPresetFavorites.h"
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
/// The favourites: listForType(type, favorites) is Java's listForType(type, true), with the
/// favourites passed in where Java reads them from the application preferences; Java's
/// setFavorite(), which stores a favourite in the preferences and fires the database's add
/// event, is ComponentPresetFavorites::setFavorite() (ComponentPresetFavorites.h), with no event.
///
/// Not ported: the database listeners (DatabaseListener, Database.fireAddEvent() and
/// fireRemoveEvent()), which only OpenRocket's preset combo box uses, to see a favourite change
/// (QtRocket's GUI reads the favourites again instead: plan decision O-M3-12), and what else
/// Database and AbstractSet offer (get(index), indexOf(), contains(), remove(), clear() and
/// removal through the iterator). The .orc reader (file/preset/) fills a database from files.
///
/// The duplicate test: Java's add() runs ArrayList.contains(), a walk over the whole list, each
/// time the binary search finds a preset that compares equal, so that a file of many presets
/// sharing one manufacturer and part number (a user's .orc) loads in quadratic time. Here the
/// database also keeps the digests of its list in a hash index (updated by add() and insert(),
/// the only ways in), and contains() asks the index: the same answer in constant time, and the
/// same order. What stays linear per add() is the insertion into the vector, which ArrayList.add(
/// index, element) shares (a copy of the presets after the index).
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

    /// The presets of @p type, in the list's order (listForType(type), and listForType(type,
    /// false)).
    [[nodiscard]] std::vector<PresetPtr> listForType(ComponentPresetType type) const;

    /// The favourite presets of @p type, in the list's order (listForType(type, true)): those
    /// whose ComponentPreset::preferenceKey() is one of @p favorites, which the caller takes from
    /// ComponentPresetFavorites::getFavorites(preferences, type) (Java reads
    /// Application.getPreferences().getComponentFavorites(type) itself). Every preset of a
    /// favourite key is listed, so two presets of one part number both are.
    [[nodiscard]] std::vector<PresetPtr> listForType(
        ComponentPresetType type, const ComponentPresetFavorites::FavoriteKeys& favorites) const;

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

    /// Puts @p preset (not null) at @p index of the list and its digest into the index.
    void insertIndexed(std::size_t index, PresetPtr preset);

    /// ArrayList.contains(): whether a preset of the list equals @p preset (the same digest),
    /// through m_digests.
    [[nodiscard]] bool contains(const ComponentPreset& preset) const;

    std::vector<PresetPtr> m_list;
    /// The digest of every preset of m_list, once per preset (insert() may hold one twice): a
    /// multiset, so that a removal, which nothing ports yet, would drop one entry of a digest.
    std::unordered_multiset<std::string> m_digests;
};

}  // namespace QtRocket
