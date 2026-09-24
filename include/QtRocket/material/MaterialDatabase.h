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
/// invalidates.
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
    MaterialDatabase(MaterialDatabase&&)                 = default;
    MaterialDatabase& operator=(MaterialDatabase&&)      = default;

    /// Database.add: inserts @p material at its sorted position (Collections.binarySearch, then
    /// the found index or the insertion point) unless an equal one is present; true when added.
    /// Emits materialAdded after the insertion.
    bool add(const Material& material);
    /// Adds every material of @p other (AbstractCollection.addAll); true when any was added.
    bool addAll(const MaterialDatabase& other);
    /// Removes the first material equal to @p material (AbstractCollection.remove through the
    /// iterator); true when one was removed. Emits materialRemoved after the removal.
    bool remove(const Material& material);

    [[nodiscard]] bool contains(const Material& material) const noexcept;
    /// The index of the first material equal to @p material, or -1.
    [[nodiscard]] int indexOf(const Material& material) const noexcept;
    /// @throws std::out_of_range when @p index is out of range (Java: IndexOutOfBoundsException)
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
