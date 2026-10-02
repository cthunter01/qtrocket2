#pragma once

#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>
#include <vector>

#include "QtRocket/aero/AerodynamicForces.h"

namespace QtRocket
{

class RocketComponent;

/// The aerodynamic forces of a set of components, in the order the components were added
/// (OpenRocket's Map<RocketComponent, AerodynamicForces>, a LinkedHashMap wherever the
/// calculators build one): the force analysis of AerodynamicCalculator::getForceAnalysis() and the
/// per-component and per-assembly maps of StabilityForceBreakdown and DragCalculator.
///
/// Order matters: the simulation sums CNa-weighted terms over a force analysis, so iterating in
/// insertion order (the active instances' tree order) keeps the result reproducible.
///
/// Keys compare by identity: OpenRocket compares components with equals() (the same class and
/// id), which within one component tree is the same thing. A key is a non-owning pointer, valid
/// while its component is in its tree; a null key is allowed (Java's HashMap takes one too). The
/// forces are held by value, one list node per entry as in Java's LinkedHashMap: references,
/// pointers and iterators to an entry stay valid until that key is removed or the map is cleared,
/// assigned to or destroyed (put() of another key never moves an entry; a moved-to map takes the
/// entries over, and the moved-from map is empty). The key cannot be changed through an entry
/// (Java's Map.Entry has no setKey()), the forces can.
class ForceMap
{
public:
    using Entry         = std::pair<const RocketComponent* const, AerodynamicForces>;
    using Iterator      = std::list<Entry>::iterator;
    using ConstIterator = std::list<Entry>::const_iterator;

    ForceMap() = default;

    /// A copy of the entries, in order.
    ForceMap(const ForceMap& other);
    /// Replaces the entries by a copy of @p other's (unchanged when the copy fails).
    ForceMap& operator=(const ForceMap& other);
    ForceMap(ForceMap&& other) noexcept;
    ForceMap& operator=(ForceMap&& other) noexcept;
    ~ForceMap() = default;

    /// Stores @p forces under @p key: an existing key keeps its place and gets the new forces
    /// (Java: LinkedHashMap.put()), a new key goes last. Returns the stored forces. Nothing
    /// changes when it throws (std::bad_alloc).
    AerodynamicForces& put(const RocketComponent* key, const AerodynamicForces& forces);

    /// The forces of @p key, or nullptr when it is not in the map (Java: get() returning null).
    [[nodiscard]] AerodynamicForces*       get(const RocketComponent* key) noexcept;
    [[nodiscard]] const AerodynamicForces* get(const RocketComponent* key) const noexcept;

    /// Whether @p key is in the map.
    [[nodiscard]] bool containsKey(const RocketComponent* key) const noexcept
    {
        return m_index.contains(key);
    }

    /// Removes @p key and its forces; false when it was not in the map. The others keep their
    /// order.
    bool remove(const RocketComponent* key);

    /// The number of components.
    [[nodiscard]] std::size_t size() const noexcept { return m_entries.size(); }

    /// Whether there is no component.
    [[nodiscard]] bool empty() const noexcept { return m_entries.empty(); }

    /// Removes everything.
    void clear() noexcept;

    /// The components, in order (Java: keySet()).
    [[nodiscard]] std::vector<const RocketComponent*> keys() const;

    /// The entries, in order (Java: entrySet()); the forces may be modified through them.
    [[nodiscard]] Iterator      begin() noexcept { return m_entries.begin(); }
    [[nodiscard]] Iterator      end() noexcept { return m_entries.end(); }
    [[nodiscard]] ConstIterator begin() const noexcept { return m_entries.begin(); }
    [[nodiscard]] ConstIterator end() const noexcept { return m_entries.end(); }

private:
    std::list<Entry>                                     m_entries;
    std::unordered_map<const RocketComponent*, Iterator> m_index;
};

}  // namespace QtRocket
