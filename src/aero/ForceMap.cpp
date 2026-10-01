#include "QtRocket/aero/ForceMap.h"

#include <cstddef>
#include <vector>

#include "QtRocket/aero/AerodynamicForces.h"

namespace QtRocket
{

AerodynamicForces& ForceMap::put(const RocketComponent* key, const AerodynamicForces& forces)
{
    const auto found = m_index.find(key);
    if (found != m_index.end())
    {
        AerodynamicForces& stored = m_entries[found->second].second;
        stored                    = forces;
        return stored;
    }
    m_index.emplace(key, m_entries.size());
    return m_entries.emplace_back(key, forces).second;
}

AerodynamicForces* ForceMap::get(const RocketComponent* key) noexcept
{
    const auto found = m_index.find(key);
    return found != m_index.end() ? &m_entries[found->second].second : nullptr;
}

const AerodynamicForces* ForceMap::get(const RocketComponent* key) const noexcept
{
    const auto found = m_index.find(key);
    return found != m_index.end() ? &m_entries[found->second].second : nullptr;
}

bool ForceMap::remove(const RocketComponent* key)
{
    const auto found = m_index.find(key);
    if (found == m_index.end())
    {
        return false;
    }
    const std::size_t position = found->second;
    m_index.erase(found);
    m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(position));
    for (auto& indexed : m_index)
    {
        if (indexed.second > position)
        {
            --indexed.second;
        }
    }
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
