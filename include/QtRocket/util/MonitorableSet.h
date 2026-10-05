#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include "QtRocket/util/ModId.h"

namespace QtRocket
{

/// A set whose changes can be watched through a modification id (OpenRocket's
/// util/MonitorableSet, a java.util.HashSet that implements Monitorable; the set satisfies the
/// Monitorable concept). The simulation status keeps the deployed recovery devices in one.
///
/// modId() is redrawn exactly where Java redraws it: in add(), remove() and clear(), each whether
/// or not the set changes (add() of an element that is there, remove() of one that is not), and
/// in addAll(), once for the call and once more for every element, since Java's addAll() draws an
/// id and then add()s each element.
///
/// Deviations from OpenRocket:
/// - The elements are kept and iterated in the order they were added. Java's HashSet iterates in
///   hash order, which for components follows their random ids: whoever sums over the set (the
///   landing stepper adds the drag areas of the deployed devices) may differ from Java, and from
///   one Java run to the next, in the last bits.
/// - Java's modification id is null until the first change; here it is ModId::invalid() then.
/// - Not ported: removeAll(), retainAll() and the iterator's remove() (remove() takes an element
///   out).
///
/// @p E must be equality comparable and cheap to copy (the simulation stores pointers); the
/// lookups are linear, which suits the handful of elements a simulation has.
template <class E>
class MonitorableSet
{
public:
    /// Iterates the elements in the order they were added. Invalidated by every change.
    using ConstIterator = std::vector<E>::const_iterator;

    /// Adds @p element unless the set has it. True when the set grew.
    bool add(const E& element)
    {
        m_modId = ModId{};
        if (contains(element))
        {
            return false;
        }
        m_elements.push_back(element);
        return true;
    }

    /// add() of every element of @p other, in its order. True when the set grew.
    bool addAll(const MonitorableSet& other)
    {
        m_modId = ModId{};
        // A copy, so that a set can be added to itself (Java: nothing changes then).
        const std::vector<E> elements = other.m_elements;
        bool                 modified = false;
        for (const E& element : elements)
        {
            if (add(element))
            {
                modified = true;
            }
        }
        return modified;
    }

    /// Removes every element.
    void clear() noexcept
    {
        m_modId = ModId{};
        m_elements.clear();
    }

    /// Takes @p element out of the set. True when it was there.
    bool remove(const E& element)
    {
        m_modId       = ModId{};
        const auto it = std::ranges::find(m_elements, element);
        if (it == m_elements.end())
        {
            return false;
        }
        m_elements.erase(it);
        return true;
    }

    [[nodiscard]] bool contains(const E& element) const
    {
        return std::ranges::find(m_elements, element) != m_elements.end();
    }

    [[nodiscard]] std::size_t size() const noexcept { return m_elements.size(); }
    /// Java: isEmpty().
    [[nodiscard]] bool empty() const noexcept { return m_elements.empty(); }

    [[nodiscard]] ConstIterator begin() const noexcept { return m_elements.cbegin(); }
    [[nodiscard]] ConstIterator end() const noexcept { return m_elements.cend(); }

    /// The modification id (Java: getModID()); see the class comment.
    [[nodiscard]] ModId modId() const noexcept { return m_modId; }

    /// Set equality: the same elements in any order (Java: AbstractSet.equals()).
    [[nodiscard]] bool operator==(const MonitorableSet& other) const
    {
        return size() == other.size() &&
               std::ranges::all_of(other.m_elements,
                                   [this](const E& element) { return contains(element); });
    }

private:
    std::vector<E> m_elements;
    ModId          m_modId{ModId::invalid()};
};

}  // namespace QtRocket
