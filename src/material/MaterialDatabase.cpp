#include "QtRocket/material/MaterialDatabase.h"

#include <cstddef>
#include <format>
#include <iterator>
#include <utility>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

MaterialDatabase::MaterialDatabase(MaterialDatabase&& other) noexcept
  : m_list(std::exchange(other.m_list, {}))
{
}

bool MaterialDatabase::add(const Material& material)
{
    // Collections.binarySearch: the index of an element comparing equal, or -(insertion point + 1)
    int  low   = 0;
    int  high  = static_cast<int>(m_list.size()) - 1;
    int  index = -1;
    bool found = false;
    while (low <= high)
    {
        const int mid = static_cast<int>(static_cast<unsigned int>(low + high) >> 1U);
        const int cmp = m_list[static_cast<std::size_t>(mid)].compareTo(material);
        if (cmp < 0)
        {
            low = mid + 1;
        }
        else if (cmp > 0)
        {
            high = mid - 1;
        }
        else
        {
            index = mid;
            found = true;
            break;
        }
    }
    if (found)
    {
        // List might contain the element
        if (contains(material))
        {
            return false;
        }
    }
    else
    {
        index = low;
    }
    const auto position = std::next(m_list.begin(), index);
    m_list.insert(position, material);
    // Java passes the element object, which a listener cannot invalidate; a reference into the
    // list (or @p material, which may alias it) would dangle once a slot changes the database.
    const Material added = m_list[static_cast<std::size_t>(index)];
    materialAdded.emit(added, *this);
    return true;
}

bool MaterialDatabase::addAll(const MaterialDatabase& other)
{
    bool modified = false;
    // A snapshot: a slot may change either database while the materials are added.
    const std::vector<Material> materials(other.begin(), other.end());
    for (const Material& material : materials)
    {
        if (add(material))
        {
            modified = true;
        }
    }
    return modified;
}

bool MaterialDatabase::remove(const Material& material)
{
    for (auto it = m_list.begin(); it != m_list.end(); ++it)
    {
        if (material == *it)
        {
            const Material removed = std::move(*it);
            m_list.erase(it);
            materialRemoved.emit(removed, *this);
            return true;
        }
    }
    return false;
}

void MaterialDatabase::clear()
{
    while (!m_list.empty())
    {
        const Material removed = std::move(m_list.front());
        m_list.erase(m_list.begin());
        materialRemoved.emit(removed, *this);
    }
}

bool MaterialDatabase::contains(const Material& material) const noexcept
{
    return indexOf(material) >= 0;
}

int MaterialDatabase::indexOf(const Material& material) const noexcept
{
    for (std::size_t i = 0; i < m_list.size(); i++)
    {
        if (material == m_list[i])
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

const Material& MaterialDatabase::get(std::size_t index) const
{
    // OpenRocket: List.get's IndexOutOfBoundsException
    if (index >= m_list.size())
    {
        bug(std::format("material index out of range: {}", index));
    }
    return m_list[index];
}

}  // namespace QtRocket
