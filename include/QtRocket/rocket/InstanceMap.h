#pragma once

#include <concepts>
#include <cstddef>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "QtRocket/rocket/InstanceContext.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket
{

class RocketComponent;

/// The instances of a set of components (OpenRocket's InstanceMap): for each component, the
/// InstanceContexts of its instances, rocket-wide. FlightConfiguration keeps two, the active
/// instances and the extra instances drawn although inactive.
///
/// Order: the entries iterate in the order their components were first added, and each
/// component's contexts in the order they were added. FlightConfiguration fills its maps with a
/// pre-order walk of the rocket, so the keys come in tree order (the rocket first) and a
/// component's contexts in the order of its parents' instances, then its own (for a component
/// with n instances under a parent with m, the context of parent instance i and own instance j
/// is at i * n + j). Deviation: OpenRocket's map is a ConcurrentHashMap, whose iteration order
/// follows the components' hash codes (their random ids) and so differs between runs.
///
/// Keys compare by identity. OpenRocket compares them with equals() (the same class and id),
/// which within one component tree is the same thing. The keys are non-owning pointers, valid
/// while their components are in the tree the map was made from.
class InstanceMap
{
public:
    /// A component and the contexts of its instances.
    using Entry         = std::pair<RocketComponent*, std::vector<InstanceContext>>;
    using ConstIterator = std::vector<Entry>::const_iterator;

    /// The number of contexts of @p key, 0 when it is not in the map.
    [[nodiscard]] int count(const RocketComponent& key) const noexcept;

    /// Adds the context of instance @p number of @p component (a new key goes last).
    void emplace(RocketComponent& component, int number, const Transformation& transform,
                 const Transformation& parentTransform = Transformation::kIdentity);

    /// Adds @p context to its component's contexts (a new key goes last).
    void add(const InstanceContext& context);

    /// Replaces the contexts of @p key with @p contexts, keeping its place, or adds @p key last
    /// (Java: HashMap.put()).
    void put(RocketComponent& key, std::vector<InstanceContext> contexts);

    /// The contexts of @p key; empty when it is not in the map (Java: null).
    [[nodiscard]] std::span<const InstanceContext> getInstanceContexts(
        const RocketComponent& key) const noexcept;

    /// Whether @p key is in the map.
    [[nodiscard]] bool containsKey(const RocketComponent& key) const noexcept
    {
        return m_index.contains(&key);
    }

    /// Removes @p key and its contexts; false when it was not in the map.
    bool remove(const RocketComponent& key);

    /// Removes every key for which @p pred holds; returns how many were removed. The others keep
    /// their order.
    template <std::predicate<const RocketComponent&> Pred>
    std::size_t eraseIf(Pred pred)
    {
        const std::size_t before = m_entries.size();
        std::erase_if(m_entries, [&pred](const Entry& entry) { return pred(*entry.first); });
        rebuildIndex();
        return before - m_entries.size();
    }

    /// The number of components.
    [[nodiscard]] std::size_t size() const noexcept { return m_entries.size(); }

    /// Whether there is no component.
    [[nodiscard]] bool isEmpty() const noexcept { return m_entries.empty(); }

    /// Removes everything.
    void clear() noexcept;

    /// The components, in order.
    [[nodiscard]] std::vector<RocketComponent*> keys() const;

    /// The entries, in order (Java: entrySet()).
    [[nodiscard]] ConstIterator begin() const noexcept { return m_entries.begin(); }
    [[nodiscard]] ConstIterator end() const noexcept { return m_entries.end(); }

    /// ">> Printing InstanceMap:" and, for every entry, "....[ i]:[name]" followed by one
    /// "........[@ j][ n]  <location>" line per context (the location's toPreciseString()).
    [[nodiscard]] std::string toString() const;

private:
    /// The entry of @p key, created last when it is new.
    Entry& entryFor(RocketComponent& key);

    /// Recomputes m_index from m_entries.
    void rebuildIndex();

    std::vector<Entry>                                      m_entries;
    std::unordered_map<const RocketComponent*, std::size_t> m_index;
};

}  // namespace QtRocket
