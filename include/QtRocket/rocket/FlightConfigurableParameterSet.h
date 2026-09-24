#pragma once

#include <algorithm>
#include <cstddef>
#include <format>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/rocket/FlightConfigurableParameter.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

/// A parameter that can differ per flight configuration (OpenRocket's
/// FlightConfigurableParameterSet<E>): a default value, kept under
/// FlightConfigurationId::defaultValueId(), and overrides for specific configurations.
///
/// The entries keep their insertion order (Java: a LinkedHashMap), the default first. Each value
/// lives in its own heap cell, so a reference returned by get() or getDefault() stays valid
/// until that entry is removed or replaced (set() on its id, setDefault(), reset()), as a Java
/// reference would keep pointing at the object; isDefault(fcid) compares those cells by
/// identity, as Java compares the objects. A value is never absent (Java's null is not a
/// value): the default always exists.
///
/// Deviations from OpenRocket:
/// - set(fcid, null) is remove(fcid); set() stores the value it is given (moved in), as Java
///   stores the reference.
/// - Removing the default entry (Java: set(DEFAULT_VALUE_FCID, null) or reset(DEFAULT_VALUE_FCID),
///   which leaves the set without a default and fails later) does nothing.
/// - get(int) throws BugError instead of an ArrayIndexOutOfBoundsException, and getId() gives
///   nullopt for Java's null.
/// - toArray() (typed on FlightConfiguration) is not ported; iterate values() instead.
///
/// Copying a set clones every value with E::clone() (Java's copy constructor); moving keeps the
/// cells.
template <FlightConfigurableParameter E>
class FlightConfigurableParameterSet
{
public:
    /// A set with @p defaultValue and no overrides.
    explicit FlightConfigurableParameterSet(E defaultValue)
    {
        m_entries.emplace_back(FlightConfigurationId::defaultValueId(),
                               std::make_unique<E>(std::move(defaultValue)));
    }

    /// A copy whose values are clones of @p other's (Java's copy constructor).
    FlightConfigurableParameterSet(const FlightConfigurableParameterSet& other)
    {
        m_entries.reserve(other.m_entries.size());
        for (const auto& [fcid, value] : other.m_entries)
        {
            m_entries.emplace_back(fcid, std::make_unique<E>(value->clone()));
        }
    }

    FlightConfigurableParameterSet& operator=(const FlightConfigurableParameterSet& other)
    {
        if (this != &other)
        {
            FlightConfigurableParameterSet copy{other};
            m_entries = std::move(copy.m_entries);
        }
        return *this;
    }

    FlightConfigurableParameterSet(FlightConfigurableParameterSet&&) noexcept            = default;
    FlightConfigurableParameterSet& operator=(FlightConfigurableParameterSet&&) noexcept = default;
    ~FlightConfigurableParameterSet()                                                    = default;

    /// The default value, used by every configuration without an override.
    [[nodiscard]] E&       getDefault() noexcept { return *m_entries.front().second; }
    [[nodiscard]] const E& getDefault() const noexcept { return *m_entries.front().second; }

    /// Replaces the default value, unless it is equal to the current one (isDefault(value)).
    void setDefault(E nextDefaultValue)
    {
        if (isDefault(nextDefaultValue))
        {
            return;
        }
        m_entries.front().second = std::make_unique<E>(std::move(nextDefaultValue));
    }

    /// Whether @p fcid has an entry (an override, or the default id itself).
    [[nodiscard]] bool containsId(const FlightConfigurationId& fcid) const noexcept
    {
        return find(fcid) != m_entries.end();
    }

    /// The number of overrides, the default not counted.
    [[nodiscard]] std::size_t size() const noexcept { return m_entries.size() - 1; }

    /// The id of the first entry (the default first) whose value equals @p testValue, or nullopt.
    [[nodiscard]] std::optional<FlightConfigurationId> getId(const E& testValue) const
    {
        for (const auto& [fcid, value] : m_entries)
        {
            if (testValue == *value)
            {
                return fcid;
            }
        }
        return std::nullopt;
    }

    /// The value of the override at @p index in getIds() order.
    /// @throws BugError when @p index is negative or not below size() (Java:
    ///         ArrayIndexOutOfBoundsException / IndexOutOfBoundsException).
    [[nodiscard]] E&       get(int index) { return *m_entries.at(entryIndex(index)).second; }
    [[nodiscard]] const E& get(int index) const { return *m_entries.at(entryIndex(index)).second; }

    /// The value for @p fcid: its override, else the default.
    [[nodiscard]] E& get(const FlightConfigurationId& fcid)
    {
        const auto it = find(fcid);
        return it != m_entries.end() ? *it->second : getDefault();
    }
    [[nodiscard]] const E& get(const FlightConfigurationId& fcid) const
    {
        const auto it = find(fcid);
        return it != m_entries.end() ? *it->second : getDefault();
    }

    /// The ids with an override, in insertion order (the default id is left out).
    [[nodiscard]] std::vector<FlightConfigurationId> getIds() const
    {
        std::vector<FlightConfigurationId> ids;
        ids.reserve(m_entries.size());
        for (const auto& entry : m_entries)
        {
            ids.push_back(entry.first);
        }
        // Java removes the first id equal to the default one: the default entry.
        if (const auto it = std::ranges::find(ids, FlightConfigurationId::defaultValueId());
            it != ids.end())
        {
            ids.erase(it);
        }
        return ids;
    }

    /// Stores @p nextValue as the override of @p fcid (replacing any, in its place), then
    /// update()s every value. Setting the default id does nothing: use setDefault().
    void set(const FlightConfigurationId& fcid, E nextValue)
    {
        if (fcid.isDefaultId())
        {
            return;
        }
        auto cell = std::make_unique<E>(std::move(nextValue));
        if (const auto it = find(fcid); it != m_entries.end())
        {
            it->second = std::move(cell);
        }
        else
        {
            m_entries.emplace_back(fcid, std::move(cell));
        }
        update();
    }

    /// Removes the override of @p fcid, if any, then update()s every value (Java: set(fcid,
    /// null)). The default entry is never removed.
    void remove(const FlightConfigurationId& fcid)
    {
        if (!fcid.isDefaultId())
        {
            if (const auto it = find(fcid); it != m_entries.end())
            {
                m_entries.erase(it);
            }
        }
        update();
    }

    /// Whether @p testValue equals the default value (Java: Utils.equals(getDefault(), value)).
    [[nodiscard]] bool isDefault(const E& testValue) const { return getDefault() == testValue; }

    /// Whether the entry of @p fcid is the default's very cell: true for the default id, false
    /// for an id without an entry (Java compares getDefault() with map.get(fcid), which is null
    /// then) and for any override.
    [[nodiscard]] bool isDefault(const FlightConfigurationId& fcid) const noexcept
    {
        const auto it = find(fcid);
        return it != m_entries.end() && it->second.get() == m_entries.front().second.get();
    }

    /// Makes @p fcid use the default value again; the error id is ignored.
    void reset(const FlightConfigurationId& fcid)
    {
        if (fcid.isValid())
        {
            remove(fcid);
        }
    }

    /// Removes every override.
    void reset() { m_entries.erase(std::next(m_entries.begin()), m_entries.end()); }

    /// Stores a copy (E::copy(newConfigId)) of the value for @p oldConfigId, override or default,
    /// as the override of @p newConfigId, update()s every value, and returns @p newConfigId.
    FlightConfigurationId copyFlightConfiguration(const FlightConfigurationId& oldConfigId,
                                                  const FlightConfigurationId& newConfigId)
    {
        E newValue = get(oldConfigId).copy(newConfigId);
        set(newConfigId, std::move(newValue));
        update();
        return newConfigId;
    }

    /// Calls update() on every value, the default first.
    void update()
    {
        for (auto& entry : m_entries)
        {
            entry.second->update();
        }
    }

    /// Every value in insertion order, the default first (Java: the Iterable).
    [[nodiscard]] auto values()
    {
        return m_entries | std::views::transform([](auto& entry) -> E& { return *entry.second; });
    }
    [[nodiscard]] auto values() const
    {
        return m_entries |
               std::views::transform([](const auto& entry) -> const E& { return *entry.second; });
    }

    /// A multi-line dump: "====== Dumping ConfigurationSet<Name> (n configurations)" and one
    /// "    [shortkey    ]: value" line per override, the key starred when the value equals the
    /// default. Name is E::kTypeName when E declares it (Java: the class's simple name), and the
    /// value is E::toString() when E has it.
    [[nodiscard]] std::string toDebug() const
    {
        std::string      buf;
        std::string_view typeName = "Parameter";
        if constexpr (requires { E::kTypeName; })
        {
            typeName = E::kTypeName;
        }
        std::format_to(std::back_inserter(buf),
                       "====== Dumping ConfigurationSet<{}> ({} configurations)\n", typeName,
                       size());
        for (const FlightConfigurationId& loopFcid : getIds())
        {
            std::string shortKey = loopFcid.toShortKey();
            const E&    inst     = get(loopFcid);
            if (isDefault(inst))
            {
                shortKey.insert(0, 1, '*');
                shortKey.push_back('*');
            }
            std::string text = "?";
            if constexpr (requires(const E& value) { value.toString(); })
            {
                text = inst.toString();
            }
            std::format_to(std::back_inserter(buf), "    [{:<12}]: {}\n", shortKey, text);
        }
        return buf;
    }

private:
    using Entry   = std::pair<FlightConfigurationId, std::unique_ptr<E>>;
    using Entries = std::vector<Entry>;

    [[nodiscard]] Entries::iterator find(const FlightConfigurationId& fcid) noexcept
    {
        return std::ranges::find(m_entries, fcid, &Entry::first);
    }
    [[nodiscard]] Entries::const_iterator find(const FlightConfigurationId& fcid) const noexcept
    {
        return std::ranges::find(m_entries, fcid, &Entry::first);
    }

    /// The entry of the override at @p index (the default is entry 0).
    [[nodiscard]] std::size_t entryIndex(int index) const
    {
        if (index < 0 || static_cast<std::size_t>(index) >= size())
        {
            bug(std::format("configurable parameter index {} out of range ({} overrides)", index,
                            size()));
        }
        return static_cast<std::size_t>(index) + 1;
    }

    Entries m_entries;
};

}  // namespace QtRocket
