#pragma once

#include <cstddef>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{

/// A set of materials kept in their natural order (OpenRocket's Database<Material>): no two equal
/// materials (Material::operator==), sorted by Material::compareTo, reachable by index, and
/// announcing every addition and removal (DatabaseListener) through two signals. Materials are
/// stored by value; get() and the iterators give references that an add() or remove()
/// invalidates. A slot receives its own copy of the material, so it may change the database.
///
/// A Java Database is an object with an identity whose listeners belong to it. Here the signals
/// (and so the connections) stay with their object: the move constructor takes only the
/// materials, leaving the source empty and still connected, and there is no move assignment, so
/// a database that others listen to cannot be replaced wholesale; clear() it instead.
///
/// Not ported from AbstractSet: removeAll, retainAll, containsAll, removal through the iterator,
/// and the set's equals and hashCode.
///
/// Not thread-safe: the owning thread (in practice the GUI thread) does everything.
class MaterialDatabase
{
public:
    using ConstIterator = std::vector<Material>::const_iterator;

    MaterialDatabase()                                   = default;
    ~MaterialDatabase()                                  = default;
    MaterialDatabase(const MaterialDatabase&)            = delete;
    MaterialDatabase& operator=(const MaterialDatabase&) = delete;
    /// Takes the materials of @p other, which is left empty; no signal is emitted and neither
    /// database's connections move.
    MaterialDatabase(MaterialDatabase&& other) noexcept;
    MaterialDatabase& operator=(MaterialDatabase&&) = delete;

    /// Database.add: inserts @p material at its sorted position (Collections.binarySearch, then
    /// the found index or the insertion point) unless an equal one is present; true when added.
    /// Emits materialAdded after the insertion.
    bool add(const Material& material);
    /// Adds every material of @p other (AbstractCollection.addAll) as it was when the call began,
    /// so that a slot may change either database; true when any was added.
    bool addAll(const MaterialDatabase& other);
    /// Removes the first material that @p material equals (AbstractCollection.remove through the
    /// iterator: @p material == element, whose tolerance is relative to the element); true when
    /// one was removed. Emits materialRemoved after the removal.
    bool remove(const Material& material);
    /// Removes every material from the first on, emitting materialRemoved after each removal
    /// (AbstractCollection.clear through the iterator).
    void clear();

    /// Whether @p material equals one of the materials (as indexOf()).
    [[nodiscard]] bool contains(const Material& material) const noexcept;
    /// The index of the first material that @p material equals (ArrayList.indexOf:
    /// @p material == element), or -1.
    [[nodiscard]] int indexOf(const Material& material) const noexcept;
    /// @throws BugError when @p index is out of range (Java: IndexOutOfBoundsException)
    [[nodiscard]] const Material& get(std::size_t index) const;
    [[nodiscard]] std::size_t     size() const noexcept { return m_list.size(); }
    [[nodiscard]] bool            empty() const noexcept { return m_list.empty(); }

    [[nodiscard]] ConstIterator begin() const noexcept { return m_list.begin(); }
    [[nodiscard]] ConstIterator end() const noexcept { return m_list.end(); }

    /// DatabaseListener.elementAdded(element, source).
    Signal<const Material&, const MaterialDatabase&> materialAdded;
    /// DatabaseListener.elementRemoved(element, source).
    Signal<const Material&, const MaterialDatabase&> materialRemoved;

private:
    std::vector<Material> m_list;
};

}  // namespace QtRocket
