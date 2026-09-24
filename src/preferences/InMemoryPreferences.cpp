#include "QtRocket/preferences/InMemoryPreferences.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/util/BugError.h"

namespace QtRocket
{

InMemoryPreferences::InMemoryPreferences(const InMemoryPreferences& other)
  : m_values(other.m_values), m_children(copyChildren(other.m_children))
{
}

InMemoryPreferences& InMemoryPreferences::operator=(const InMemoryPreferences& other)
{
    if (this != &other)
    {
        m_values   = other.m_values;
        m_children = copyChildren(other.m_children);
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

std::optional<std::string> InMemoryPreferences::get(std::string_view key) const
{
    const auto it = m_values.find(key);
    if (it == m_values.end())
    {
        return std::nullopt;
    }
    return it->second;
}

void InMemoryPreferences::put(std::string_view key, std::string_view value)
{
    m_values.insert_or_assign(std::string(key), std::string(value));
}

void InMemoryPreferences::remove(std::string_view key)
{
    const auto it = m_values.find(key);
    if (it != m_values.end())
    {
        m_values.erase(it);
    }
}

void InMemoryPreferences::clear()
{
    m_values.clear();
}

std::vector<std::string> InMemoryPreferences::keys() const
{
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
    auto it = m_children.find(head);
    if (it == m_children.end())
    {
        it = m_children.emplace(std::string(head), std::make_unique<InMemoryPreferences>()).first;
    }
    InMemoryPreferences& child = *it->second;
    if (slash == std::string_view::npos)
    {
        return child;
    }
    return child.getNode(name.substr(slash + 1));
}

const InMemoryPreferences* InMemoryPreferences::findNode(std::string_view name) const noexcept
{
    const std::size_t      slash = name.find('/');
    const std::string_view head  = name.substr(0, slash);
    const auto             it    = m_children.find(head);
    if (it == m_children.end())
    {
        return nullptr;
    }
    const InMemoryPreferences& child = *it->second;
    if (slash == std::string_view::npos)
    {
        return &child;
    }
    return child.findNode(name.substr(slash + 1));
}

void InMemoryPreferences::reset()
{
    m_values.clear();
    m_children.clear();
}

bool InMemoryPreferences::empty() const noexcept
{
    return m_values.empty() && m_children.empty();
}

bool InMemoryPreferences::operator==(const InMemoryPreferences& other) const
{
    if (m_values != other.m_values || m_children.size() != other.m_children.size())
    {
        return false;
    }
    return std::ranges::all_of(m_children, [&other](const auto& entry) {
        const auto otherChild = other.m_children.find(entry.first);
        return otherChild != other.m_children.end() && *entry.second == *otherChild->second;
    });
}

}  // namespace QtRocket
