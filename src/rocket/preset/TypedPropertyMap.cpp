#include "QtRocket/rocket/preset/TypedPropertyMap.h"

#include <cstddef>
#include <format>
#include <iterator>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/TypedKey.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The visitor of toString(const TypedValue&): String.valueOf for each value type.
struct ValueFormatter
{
    std::string operator()(bool value) const { return value ? "true" : "false"; }
    std::string operator()(int value) const { return std::format("{}", value); }
    std::string operator()(double value) const { return Strings::javaDoubleToString(value); }
    std::string operator()(const std::string& value) const { return value; }
    std::string operator()(const ManufacturerRef& value) const { return value.get().toString(); }
    std::string operator()(ComponentPresetType value) const
    {
        return std::string(componentPresetTypeName(value));
    }
    std::string operator()(TransitionShape value) const { return std::string(displayName(value)); }
    std::string operator()(const Material& value) const { return value.toString(); }
    std::string operator()(Finish value) const { return QtRocket::toString(value); }
    std::string operator()(const std::vector<std::byte>& value) const
    {
        return std::format("byte[{}]", value.size());
    }
};

}  // namespace

std::string toString(const TypedValue& value)
{
    return std::visit(ValueFormatter{}, value);
}

bool TypedPropertyMap::remove(const AnyTypedKey& key)
{
    return std::erase_if(m_entries, [&key](const Entry& entry) { return entry.key == key; }) > 0;
}

void TypedPropertyMap::putAll(const TypedPropertyMap& other)
{
    if (&other == this)
    {
        return;
    }
    for (const Entry& entry : other.m_entries)
    {
        putValue(entry.key, entry.value);
    }
}

std::vector<AnyTypedKey> TypedPropertyMap::keySet() const
{
    std::vector<AnyTypedKey> keys;
    keys.reserve(m_entries.size());
    for (const Entry& entry : m_entries)
    {
        keys.push_back(entry.key);
    }
    return keys;
}

std::string TypedPropertyMap::toString() const
{
    std::string out = "TypedPropertyMap: { ";
    for (const Entry& entry : m_entries)
    {
        std::format_to(std::back_inserter(out), "{} => {}", entry.key.toString(),
                       QtRocket::toString(entry.value));
    }
    out += "}";
    return out;
}

const TypedPropertyMap::Entry* TypedPropertyMap::find(const AnyTypedKey& key) const noexcept
{
    for (const Entry& entry : m_entries)
    {
        if (entry.key == key)
        {
            return &entry;
        }
    }
    return nullptr;
}

void TypedPropertyMap::putValue(const AnyTypedKey& key, TypedValue value)
{
    if (value.index() != key.getTypeIndex())
    {
        bug(std::format("{} cannot hold the value {}", key.toString(), QtRocket::toString(value)));
    }
    for (Entry& entry : m_entries)
    {
        if (entry.key == key)
        {
            entry.value = std::move(value);
            return;
        }
    }
    m_entries.push_back(Entry{.key = key, .value = std::move(value)});
}

}  // namespace QtRocket
