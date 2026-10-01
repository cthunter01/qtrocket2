#include "QtRocket/aero/ForceMap.h"

#include <iterator>
#include <utility>
#include <vector>

#include "QtRocket/aero/AerodynamicForces.h"

namespace QtRocket
{

ForceMap::ForceMap(const ForceMap& other)
{
    m_index.reserve(other.m_index.size());
    for (const Entry& entry : other.m_entries)
    {
        put(entry.first, entry.second);
    }
}

ForceMap& ForceMap::operator=(const ForceMap& other)
{
    if (this != &other)
    {
        ForceMap copy{other};
        *this = std::move(copy);
    }
    return *this;
}

// The list's nodes move along with it, so the iterators in the index stay valid (they then refer
// to this map's list); the source is cleared so that its list and index agree.
ForceMap::ForceMap(ForceMap&& other) noexcept
  : m_entries(std::move(other.m_entries)), m_index(std::move(other.m_index))
{
    other.clear();
}

ForceMap& ForceMap::operator=(ForceMap&& other) noexcept
{
    if (this != &other)
    {
        m_entries = std::move(other.m_entries);
        m_index   = std::move(other.m_index);
        other.clear();
    }
    return *this;
}

AerodynamicForces& ForceMap::put(const RocketComponent* key, const AerodynamicForces& forces)
{
    const auto found = m_index.find(key);
    if (found != m_index.end())
    {
        AerodynamicForces& stored = found->second->second;
        stored                    = forces;
        return stored;
    }
    // The entry first, then the index: should the index throw, the entry is taken back, so the
    // two never disagree.
    m_entries.emplace_back(key, forces);
    try
    {
        m_index.emplace(key, std::prev(m_entries.end()));
    }
    catch (...)
    {
        m_entries.pop_back();
        throw;
    }
    return m_entries.back().second;
}

AerodynamicForces* ForceMap::get(const RocketComponent* key) noexcept
{
    const auto found = m_index.find(key);
    return found != m_index.end() ? &found->second->second : nullptr;
}

const AerodynamicForces* ForceMap::get(const RocketComponent* key) const noexcept
{
    const auto found = m_index.find(key);
    return found != m_index.end() ? &found->second->second : nullptr;
}

bool ForceMap::remove(const RocketComponent* key)
{
    const auto found = m_index.find(key);
    if (found == m_index.end())
    {
        return false;
    }
    m_entries.erase(found->second);
    m_index.erase(found);
    return true;
}

void ForceMap::clear() noexcept
{
    m_entries.clear();
    m_index.clear();
}

std::vector<const RocketComponent*> ForceMap::keys() const
{
    std::vector<const RocketComponent*> result;
    result.reserve(m_entries.size());
    for (const Entry& entry : m_entries)
    {
        result.push_back(entry.first);
    }
    return result;
}

}  // namespace QtRocket
