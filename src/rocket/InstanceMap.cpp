#include "QtRocket/rocket/InstanceMap.h"

#include <cstddef>
#include <format>
#include <iterator>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/rocket/InstanceContext.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket
{

int InstanceMap::count(const RocketComponent& key) const noexcept
{
    const auto it = m_index.find(&key);
    return it != m_index.end() ? static_cast<int>(m_entries[it->second].second.size()) : 0;
}

InstanceMap::Entry& InstanceMap::entryFor(RocketComponent& key)
{
    const auto it = m_index.find(&key);
    if (it != m_index.end())
    {
        return m_entries[it->second];
    }
    m_index.emplace(&key, m_entries.size());
    return m_entries.emplace_back(&key, std::vector<InstanceContext>{});
}

void InstanceMap::emplace(RocketComponent& component, int number, const Transformation& transform,
                          const Transformation& parentTransform)
{
    entryFor(component).second.emplace_back(component, number, transform, parentTransform);
}

void InstanceMap::add(const InstanceContext& context)
{
    entryFor(*context.component).second.push_back(context);
}

void InstanceMap::put(RocketComponent& key, std::vector<InstanceContext> contexts)
{
    entryFor(key).second = std::move(contexts);
}

std::span<const InstanceContext> InstanceMap::getInstanceContexts(
    const RocketComponent& key) const noexcept
{
    const auto it = m_index.find(&key);
    if (it == m_index.end())
    {
        return {};
    }
    return m_entries[it->second].second;
}

bool InstanceMap::remove(const RocketComponent& key)
{
    return eraseIf([&key](const RocketComponent& component) { return &component == &key; }) > 0;
}

void InstanceMap::clear() noexcept
{
    m_entries.clear();
    m_index.clear();
}

std::vector<RocketComponent*> InstanceMap::keys() const
{
    std::vector<RocketComponent*> result;
    result.reserve(m_entries.size());
    for (const Entry& entry : m_entries)
    {
        result.push_back(entry.first);
    }
    return result;
}

void InstanceMap::rebuildIndex()
{
    m_index.clear();
    for (std::size_t i = 0; i < m_entries.size(); ++i)
    {
        m_index.emplace(m_entries[i].first, i);
    }
}

std::string InstanceMap::toString() const
{
    std::string buffer     = ">> Printing InstanceMap:\n";
    int         outerIndex = 0;
    for (const auto& [key, contexts] : m_entries)
    {
        // Java's "% 2d": a space for the sign of a non-negative number, at least two wide.
        std::format_to(std::back_inserter(buffer), "....[{: 2}]:[{}]\n", outerIndex,
                       key->getName());
        outerIndex++;

        int innerIndex = 0;
        for (const InstanceContext& context : contexts)
        {
            std::format_to(std::back_inserter(buffer), "........[@{: 2}][{: 2}]  {}\n", innerIndex,
                           context.instanceNumber, context.getLocation().toPreciseString());
            innerIndex++;
        }
    }
    return buffer;
}

}  // namespace QtRocket
