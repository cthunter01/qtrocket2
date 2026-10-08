#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

bool ComponentPresetDatabase::add(PresetPtr preset)
{
    if (preset == nullptr)
    {
        bug("ComponentPresetDatabase::add(): the preset is null");
    }

    const SearchResult search = binarySearch(*preset);
    // List might contain the element (Java looks through the whole list, not only at the
    // presets that compare equal).
    if (search.found && contains(*preset))
    {
        return false;
    }
    // The found index, or the insertion point: a preset that compares equal to the found one
    // goes in front of it.
    m_list.insert(std::next(m_list.begin(), static_cast<std::ptrdiff_t>(search.index)),
                  std::move(preset));
    // Java fires the add event here; the database listeners are not ported (see the class
    // comment).
    return true;
}

bool ComponentPresetDatabase::addAll(std::span<const PresetPtr> presets)
{
    // A snapshot: @p presets may be this database's own list, which add() changes.
    const std::vector<PresetPtr> snapshot(presets.begin(), presets.end());
    bool                         modified = false;
    for (const PresetPtr& preset : snapshot)
    {
        if (add(preset))
        {
            modified = true;
        }
    }
    return modified;
}

void ComponentPresetDatabase::insert(PresetPtr preset)
{
    if (preset == nullptr)
    {
        bug("ComponentPresetDatabase::insert(): the preset is null");
    }
    m_list.push_back(std::move(preset));
}

std::vector<ComponentPresetDatabase::PresetPtr> ComponentPresetDatabase::listForType(
    ComponentPresetType type) const
{
    std::vector<PresetPtr> result;
    for (const PresetPtr& preset : m_list)
    {
        if (preset->getType() == type)
        {
            result.push_back(preset);
        }
    }
    return result;
}

std::vector<ComponentPresetDatabase::PresetPtr> ComponentPresetDatabase::listForTypes(
    std::span<const ComponentPresetType> types) const
{
    // Java answers one type with listForType() and no type with an empty list; the loop gives
    // both.
    std::vector<PresetPtr> result;
    for (const PresetPtr& preset : m_list)
    {
        if (std::ranges::find(types, preset->getType()) != types.end())
        {
            result.push_back(preset);
        }
    }
    return result;
}

std::vector<ComponentPresetDatabase::PresetPtr> ComponentPresetDatabase::listForTypes(
    std::initializer_list<ComponentPresetType> types) const
{
    return listForTypes(std::span<const ComponentPresetType>{types.begin(), types.size()});
}

std::vector<ComponentPresetDatabase::PresetPtr> ComponentPresetDatabase::find(
    std::string_view manufacturer, std::string_view partNo) const
{
    std::vector<PresetPtr> presets;
    for (const PresetPtr& preset : m_list)
    {
        if (preset->matches(manufacturer, partNo))
        {
            presets.push_back(preset);
        }
    }
    return presets;
}

ComponentPresetDatabase::SearchResult ComponentPresetDatabase::binarySearch(
    const ComponentPreset& key) const
{
    // Collections.binarySearch() of an ArrayList (indexedBinarySearch), which also runs on a list
    // that insert() has left unsorted: low and high are one past Java's, so that they stay
    // unsigned.
    std::size_t low  = 0;
    std::size_t high = m_list.size();
    while (low < high)
    {
        const std::size_t mid = low + ((high - 1 - low) / 2);
        const int         cmp = m_list[mid]->compareTo(key);
        if (cmp < 0)
        {
            low = mid + 1;
        }
        else if (cmp > 0)
        {
            high = mid;
        }
        else
        {
            return SearchResult{.index = mid, .found = true};
        }
    }
    return SearchResult{.index = low, .found = false};
}

bool ComponentPresetDatabase::contains(const ComponentPreset& preset) const noexcept
{
    return std::ranges::any_of(m_list,
                               [&preset](const PresetPtr& held) { return *held == preset; });
}

}  // namespace QtRocket
