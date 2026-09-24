#include "QtRocket/material/MaterialDatabase.h"

#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <vector>

#include "QtRocket/material/Material.h"

namespace QtRocket
{

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
    materialAdded.emit(m_list[static_cast<std::size_t>(index)], *this);
    return true;
}

bool MaterialDatabase::addAll(const MaterialDatabase& other)
{
    bool modified = false;
    for (const Material& material : other)
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
        if (*it == material)
        {
            const Material removed = *it;
            m_list.erase(it);
            materialRemoved.emit(removed, *this);
            return true;
        }
    }
    return false;
}

bool MaterialDatabase::contains(const Material& material) const noexcept
{
    return indexOf(material) >= 0;
}

int MaterialDatabase::indexOf(const Material& material) const noexcept
{
    for (std::size_t i = 0; i < m_list.size(); i++)
    {
        if (m_list[i] == material)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

const Material& MaterialDatabase::get(std::size_t index) const
{
    if (index >= m_list.size())
    {
        throw std::out_of_range("material index out of range");
    }
    return m_list[index];
}

}  // namespace QtRocket
