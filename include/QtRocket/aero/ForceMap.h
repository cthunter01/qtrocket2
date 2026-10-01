#pragma once

#include <cstddef>
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
/// forces are held by value. References and pointers to them stay valid until the next put() of a
/// new key.
class ForceMap
{
public:
    using Entry         = std::pair<const RocketComponent*, AerodynamicForces>;
    using Iterator      = std::vector<Entry>::iterator;
    using ConstIterator = std::vector<Entry>::const_iterator;

    /// Stores @p forces under @p key: an existing key keeps its place and gets the new forces
    /// (Java: LinkedHashMap.put()), a new key goes last. Returns the stored forces.
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
    std::vector<Entry>                                      m_entries;
    std::unordered_map<const RocketComponent*, std::size_t> m_index;
};

}  // namespace QtRocket
