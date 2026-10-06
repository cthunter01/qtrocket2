#include "QtRocket/preferences/InMemoryPreferences.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/util/BugError.h"

namespace QtRocket
{

// The other node stays locked while its values and its children (each under its own lock) are
// copied, so that the snapshot is one state of the node: the lock lives until the delegated
// constructor has returned.
InMemoryPreferences::InMemoryPreferences(const InMemoryPreferences& other)
  : InMemoryPreferences(other, std::scoped_lock(other.m_mutex))
{
}

InMemoryPreferences::InMemoryPreferences(const InMemoryPreferences& other,
                                         const std::scoped_lock<std::mutex>& /*lock*/)
  : m_values(other.m_values), m_children(copyChildren(other.m_children))
{
}

InMemoryPreferences& InMemoryPreferences::operator=(const InMemoryPreferences& other)
{
    if (this != &other)
    {
        // The snapshot is taken first and on its own: the two nodes are never locked together
        // (one may be a child of the other).
        Values   values;
        Children children;
        {
            const std::scoped_lock lock(other.m_mutex);
            values   = other.m_values;
            children = copyChildren(other.m_children);
        }
        const std::scoped_lock lock(m_mutex);
        m_values   = std::move(values);
        m_children = std::move(children);
    }
    return *this;
}

InMemoryPreferences::Children InMemoryPreferences::copyChildren(const Children& children)
{
    Children copy;
    for (const auto& [name, child] : children)
    {
        copy.emplace(name, std::make_unique<InMemoryPreferences>(*child));
    }
    return copy;
}

InMemoryPreferences::Contents InMemoryPreferences::contents() const
{
    const std::scoped_lock lock(m_mutex);
    Contents               result;
    result.values = m_values;
    result.children.reserve(m_children.size());
    for (const auto& [name, child] : m_children)
    {
        result.children.emplace_back(name, child.get());
    }
    return result;
}

std::optional<std::string> InMemoryPreferences::get(std::string_view key) const
{
    const std::scoped_lock lock(m_mutex);
    const auto             it = m_values.find(key);
    if (it == m_values.end())
    {
        return std::nullopt;
    }
    return it->second;
}

void InMemoryPreferences::put(std::string_view key, std::string_view value)
{
    // The strings are made before the lock is taken.
    std::string            storedKey(key);
    std::string            storedValue(value);
    const std::scoped_lock lock(m_mutex);
    m_values.insert_or_assign(std::move(storedKey), std::move(storedValue));
}

void InMemoryPreferences::remove(std::string_view key)
{
    const std::scoped_lock lock(m_mutex);
    const auto             it = m_values.find(key);
    if (it != m_values.end())
    {
        m_values.erase(it);
    }
}

void InMemoryPreferences::clear()
{
    const std::scoped_lock lock(m_mutex);
    m_values.clear();
}

std::vector<std::string> InMemoryPreferences::keys() const
{
    const std::scoped_lock   lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_values.size());
    for (const auto& [key, value] : m_values)
    {
        names.push_back(key);
    }
    return names;
}

std::vector<std::string> InMemoryPreferences::childrenNames() const
{
    const std::scoped_lock   lock(m_mutex);
    std::vector<std::string> names;
    names.reserve(m_children.size());
    for (const auto& [name, child] : m_children)
    {
        names.push_back(name);
    }
    return names;
}

InMemoryPreferences& InMemoryPreferences::getNode(std::string_view name)
{
    // java.util.prefs: a relative path, one node name per '/'-separated segment.
    const std::size_t      slash = name.find('/');
    const std::string_view head  = name.substr(0, slash);
    QTROCKET_ASSERT(!head.empty());
    InMemoryPreferences* child = nullptr;
    {
        const std::scoped_lock lock(m_mutex);
        auto                   it = m_children.find(head);
        if (it == m_children.end())
        {
            it = m_children.emplace(std::string(head), std::make_unique<InMemoryPreferences>())
                     .first;
        }
        child = it->second.get();
    }
    if (slash == std::string_view::npos)
    {
        return *child;
    }
    return child->getNode(name.substr(slash + 1));
}

const InMemoryPreferences* InMemoryPreferences::findNode(std::string_view name) const noexcept
{
    const std::size_t          slash = name.find('/');
    const std::string_view     head  = name.substr(0, slash);
    const InMemoryPreferences* child = nullptr;
    {
        const std::scoped_lock lock(m_mutex);
        const auto             it = m_children.find(head);
        if (it == m_children.end())
        {
            return nullptr;
        }
        child = it->second.get();
    }
    if (slash == std::string_view::npos)
    {
        return child;
    }
    return child->findNode(name.substr(slash + 1));
}

void InMemoryPreferences::reset()
{
    // The children are destroyed after the lock is released.
    Children removed;
    {
        const std::scoped_lock lock(m_mutex);
        m_values.clear();
        removed.swap(m_children);
    }
}

bool InMemoryPreferences::empty() const noexcept
{
    const std::scoped_lock lock(m_mutex);
    return m_values.empty() && m_children.empty();
}

bool InMemoryPreferences::operator==(const InMemoryPreferences& other) const
{
    if (this == &other)
    {
        return true;
    }
    // Compared through what each node held at one moment: no two nodes are locked together.
    const Contents mine   = contents();
    const Contents theirs = other.contents();
    if (mine.values != theirs.values || mine.children.size() != theirs.children.size())
    {
        return false;
    }
    // Both lists are sorted by name.
    return std::ranges::equal(
        mine.children, theirs.children, [](const auto& child, const auto& otherChild) {
            return child.first == otherChild.first && *child.second == *otherChild.second;
        });
}

}  // namespace QtRocket
