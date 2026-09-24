#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "QtRocket/rocket/preset/TypedKey.h"

namespace QtRocket
{

/// A map from TypedKeys to values of the keys' types (OpenRocket's TypedPropertyMap, a
/// LinkedHashMap underneath): the entries keep the order in which their keys were first put, a
/// put() of a key already present replaces the value in place, and keys are equal when their
/// names and value types are (AnyTypedKey::operator==).
///
/// get() gives a pointer to the value, nullptr for an absent key (Java's null). A value is never
/// null here: Java's put(key, null) has no counterpart. containsValue(), values() and entrySet()
/// are replaced by entries(); clone() is the copy constructor.
class TypedPropertyMap
{
public:
    /// One key and its value, which always has the key's type.
    struct Entry
    {
        AnyTypedKey key;
        TypedValue  value;
    };

    /// The number of entries (size()).
    [[nodiscard]] std::size_t size() const noexcept { return m_entries.size(); }

    /// True when there are no entries (isEmpty()).
    [[nodiscard]] bool isEmpty() const noexcept { return m_entries.empty(); }

    /// True when @p key has a value (containsKey()).
    [[nodiscard]] bool containsKey(const AnyTypedKey& key) const noexcept
    {
        return find(key) != nullptr;
    }

    /// The value of @p key, or nullptr when it has none (get()). The pointer stays valid until
    /// the key is put again, removed or the map changes size.
    template <TypedValueType T>
    [[nodiscard]] const T* get(const TypedKey<T>& key) const noexcept
    {
        const Entry* entry = find(key);
        return entry == nullptr ? nullptr : std::get_if<T>(&entry->value);
    }

    /// Sets the value of @p key (put()): replaced in place when the key is present, appended
    /// otherwise.
    template <TypedValueType T>
    void put(const TypedKey<T>& key, std::type_identity_t<T> value)
    {
        putValue(key, TypedValue(std::in_place_type<T>, std::move(value)));
    }

    /// The value of @p key whatever its type, or nullptr (get() through a TypedKey<?>, as the
    /// preset table reads its columns).
    [[nodiscard]] const TypedValue* getValue(const AnyTypedKey& key) const noexcept
    {
        const Entry* entry = find(key);
        return entry == nullptr ? nullptr : &entry->value;
    }

    /// put() through a key whose type is known at run time only (the .orc column parsers put
    /// through raw TypedKeys): replaced in place when the key is present, appended otherwise.
    /// @throws BugError when @p value does not hold the key's type (Java would store it and fail
    ///         with a ClassCastException on the first get()).
    void putValue(const AnyTypedKey& key, TypedValue value);

    /// Removes the entry of @p key; true when there was one (remove()).
    bool remove(const AnyTypedKey& key);

    /// Puts every entry of @p other, in its order (putAll()).
    void putAll(const TypedPropertyMap& other);

    /// Removes every entry (clear()).
    void clear() noexcept { m_entries.clear(); }

    /// The keys, in insertion order (keySet()).
    [[nodiscard]] std::vector<AnyTypedKey> keySet() const;

    /// The entries, in insertion order (entrySet()).
    [[nodiscard]] std::span<const Entry> entries() const noexcept { return m_entries; }

    /// "TypedPropertyMap: { " followed by "<key> => <value>" for each entry, with no separator
    /// between entries, and "}" (toString()). Each value is written by toString(const
    /// TypedValue&).
    [[nodiscard]] std::string toString() const;

private:
    /// The entry of @p key, or nullptr.
    [[nodiscard]] const Entry* find(const AnyTypedKey& key) const noexcept;

    std::vector<Entry> m_entries;
};

/// A value as Java's String.valueOf writes it: "true"/"false", an int in decimal, a double as
/// Double.toString (Strings::javaDoubleToString), a string as is, a manufacturer as its display
/// name, a preset type as its name ("BODY_TUBE"), a shape as its English name, a material as
/// Material::toString() and a finish as toString(Finish). Deviation: Java writes a byte array as
/// "[B@" and an identity hash, which differs from run to run; this writes "byte[<size>]".
[[nodiscard]] std::string toString(const TypedValue& value);

}  // namespace QtRocket
