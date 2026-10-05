#pragma once

#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "QtRocket/util/BigDecimal.h"

namespace QtRocket
{

/// The integer types a Config stores: the signed ones (Java has no others), by their width. The
/// character types are no numbers in Java (a Character put into a Config is refused there), and
/// bool is its own type.
template <typename T>
concept ConfigInteger = std::signed_integral<T> && !std::same_as<T, char> &&
                        !std::same_as<T, wchar_t> && (sizeof(T) <= sizeof(std::int64_t));

/// The configuration of a simulation extension (OpenRocket's util/Config): values by key, in the
/// order the keys were first put (Java's LinkedHashMap: putting a key again keeps its place).
///
/// A value keeps the type it was put with, because the .ork writer prints a number with Java's
/// toString of its type and the reader tells them apart again: the Integer 5 and the Double 5.0
/// are different values ("5" and "5.0" in the file), and neither equals the other. The types are
/// the ones Java's Config holds once validateType() and clone() have passed:
/// - Boolean (bool) and String (std::string);
/// - the Number types Java keeps as they are: Byte, Short, Integer, Long (the signed integers of
///   8, 16, 32 and 64 bits), Float and Double;
/// - BigDecimal, which Java turns every other Number into (new BigDecimal(value.toString()): a
///   BigInteger, an AtomicInteger, ...). The .ork reader makes one of a number that is not exactly
///   an int, a long or a double;
/// - List (std::vector<Value>), whose elements are any of these, lists included.
///
/// A Config is a value: a copy is Java's clone(), a deep copy (the lists are copied, as put()
/// copies the list it is given). Java's Config has no equals(), and there is no operator== here
/// either: document/Simulation's configEqual() compares two Configs by their key sets, whatever
/// order the keys were put in, and by Java's equals() of the stored objects. That comparison is
/// sameEntries() (with Value::operator==); comparing two keySet() results with == would not be
/// it, since they are in insertion order.
///
/// The getters with a default return it when the key is absent or holds another type (Java's
/// get(key, def, type)); the getters without one return nullopt there, where Java is given a null
/// default.
///
/// Deviations from OpenRocket:
/// - Java's put() throws for a null (NullPointerException) and for a value, or a list element,
///   of another type (IllegalArgumentException). Here a value of another type does not compile
///   (a Value is made from exactly the types above: no pointer but a `const char*`, no character,
///   no unsigned integer, and nothing that only converts to bool, so a std::vector<bool> element
///   is written `bool{element}`), and only a null `const char*` can be null (a `nullptr`
///   included): that is a BugError with Java's message.
/// - The getters return copies. Java hands out the stored objects, which are immutable except
///   for a list: there a caller could change the Config through the list getList() returned.
/// - keySet() is a copy of the keys, where Java returns an unmodifiable view.
/// - Lookup is linear in the number of keys: an extension has a handful of them.
class Config
{
public:
    class Value;

    /// A list value (Java: an ArrayList of values).
    using List = std::vector<Value>;

    /// One value of a Config, with its Java type (see the class comment). Every constructor is
    /// explicit: Value{5} is an Integer, Value{std::int64_t{5}} a Long, Value{5.0} a Double and
    /// Value{"5"} a String.
    class Value
    {
    public:
        /// The alternatives, in this order: Boolean, Byte, Short, Integer, Long, Float, Double,
        /// BigDecimal, String, List.
        using Variant = std::variant<bool, std::int8_t, std::int16_t, std::int32_t, std::int64_t,
                                     float, double, BigDecimal, std::string, List>;

        /// A Boolean: a bool itself and nothing that merely converts to one. A plain
        /// `Value(bool)` would take every pointer (the pointer-to-bool conversion) and store
        /// true for a `std::string*` or a `const wchar_t*`, where Java's validateType() throws.
        template <std::same_as<bool> T>
        explicit Value(T value) noexcept : m_data(std::in_place_type<bool>, value)
        {
        }

        /// A Byte, Short, Integer or Long, by the width of @p value's type: an int is an
        /// Integer, a std::int64_t a Long (a long is 64 bits wide on Linux and macOS and 32 on
        /// Windows: use the fixed-width types where the Java type matters).
        template <ConfigInteger T>
        explicit Value(T value) noexcept : m_data(fromInteger(value))
        {
        }

        /// A Float.
        explicit Value(float value) noexcept;
        /// A Double.
        explicit Value(double value) noexcept;
        /// A BigDecimal.
        explicit Value(BigDecimal value) noexcept;
        /// A String.
        explicit Value(std::string value) noexcept;
        explicit Value(std::string_view value);
        /// A String.
        /// @throws BugError when @p value is null (Java: NullPointerException "Attempting to add
        ///         null value to Config object")
        explicit Value(const char* value);
        /// A List.
        explicit Value(List value) noexcept;

        /// The value with its type, for std::visit and std::get_if.
        [[nodiscard]] const Variant& variant() const noexcept { return m_data; }

        /// Java's `instanceof Number`: a Byte, Short, Integer, Long, Float, Double or BigDecimal.
        [[nodiscard]] bool isNumber() const;

        /// Java's Number.intValue(), or nullopt when the value is no number: a Long keeps its
        /// low 32 bits, a Float or Double is truncated towards zero and saturates at the int
        /// range, a NaN being 0 (Java's (int) cast), and a BigDecimal gives
        /// BigDecimal::intValue().
        [[nodiscard]] std::optional<std::int32_t> intValue() const;

        /// Java's Number.longValue(), or nullopt when the value is no number: a Float or Double
        /// is truncated and saturates at the long range, a NaN being 0, and a BigDecimal gives
        /// BigDecimal::longValue().
        [[nodiscard]] std::optional<std::int64_t> longValue() const;

        /// Java's Number.doubleValue(), or nullopt when the value is no number: a Long becomes
        /// the nearest double, and a BigDecimal gives BigDecimal::doubleValue().
        [[nodiscard]] std::optional<double> doubleValue() const;

        /// Java's equals() of the stored objects: the same type and the same value, so the
        /// Integer 5 equals neither the Long 5 nor the Double 5.0. A Float or Double compares as
        /// Double.equals does (a NaN equals a NaN, 0.0 does not equal -0.0), a BigDecimal with
        /// its scale (1.0 does not equal 1.00), and lists element by element.
        [[nodiscard]] bool operator==(const Value& other) const;

    private:
        template <ConfigInteger T>
        [[nodiscard]] static Variant fromInteger(T value) noexcept
        {
            if constexpr (sizeof(T) == sizeof(std::int8_t))
            {
                return Variant{std::in_place_type<std::int8_t>, value};
            }
            else if constexpr (sizeof(T) == sizeof(std::int16_t))
            {
                return Variant{std::in_place_type<std::int16_t>, value};
            }
            else if constexpr (sizeof(T) == sizeof(std::int32_t))
            {
                return Variant{std::in_place_type<std::int32_t>, value};
            }
            else
            {
                return Variant{std::in_place_type<std::int64_t>, value};
            }
        }

        Variant m_data;
    };

    /// Java's put(String, Object): stores @p value under @p key, replacing the value the key has
    /// and keeping its place in the key order, or adding the key at the end.
    void put(std::string_view key, Value value);

    /// Java's typed overloads put(String, String), put(String, Number), put(String, Boolean) and
    /// put(String, List<?>): put(key, Value(value)) for every type a Value is made from, so
    /// put("n", 5) stores an Integer and put("x", 5.0) a Double.
    template <typename T>
        requires(!std::same_as<std::remove_cvref_t<T>, Value>) &&
                (!std::is_array_v<std::remove_reference_t<T>>) && std::constructible_from<Value, T>
    void put(std::string_view key, T&& value)
    {
        put(key, Value(std::forward<T>(value)));
    }

    /// put(key, Value(value)) for a C string, such as a string literal: a String.
    /// @throws BugError when @p value is null (see Value)
    void put(std::string_view key, const char* value);

    /// The value of @p key, or nullopt when there is none (Java: get(key, null)).
    [[nodiscard]] std::optional<Value> get(std::string_view key) const;
    /// The value of @p key, or @p def when there is none.
    [[nodiscard]] Value get(std::string_view key, const Value& def) const;

    /// The Boolean of @p key, or nullopt / @p def when the key is absent or no Boolean.
    [[nodiscard]] std::optional<bool> getBoolean(std::string_view key) const;
    [[nodiscard]] bool                getBoolean(std::string_view key, bool def) const;

    /// Value::intValue() of the number of @p key, or nullopt / @p def when the key is absent or
    /// no number (a String "12" is not a number).
    [[nodiscard]] std::optional<std::int32_t> getInt(std::string_view key) const;
    [[nodiscard]] std::int32_t                getInt(std::string_view key, std::int32_t def) const;

    /// Value::longValue() of the number of @p key, or nullopt / @p def.
    [[nodiscard]] std::optional<std::int64_t> getLong(std::string_view key) const;
    [[nodiscard]] std::int64_t                getLong(std::string_view key, std::int64_t def) const;

    /// Value::doubleValue() of the number of @p key, or nullopt / @p def.
    [[nodiscard]] std::optional<double> getDouble(std::string_view key) const;
    [[nodiscard]] double                getDouble(std::string_view key, double def) const;

    /// The String of @p key, or nullopt / @p def when the key is absent or no String.
    [[nodiscard]] std::optional<std::string> getString(std::string_view key) const;
    [[nodiscard]] std::string getString(std::string_view key, std::string_view def) const;

    /// The List of @p key, or nullopt / @p def when the key is absent or no List.
    [[nodiscard]] std::optional<List> getList(std::string_view key) const;
    [[nodiscard]] List                getList(std::string_view key, const List& def) const;

    /// Whether @p key has a value.
    [[nodiscard]] bool containsKey(std::string_view key) const noexcept;

    /// The keys, in the order they were first put.
    [[nodiscard]] std::vector<std::string> keySet() const;

    /// Whether @p other holds the same keys with equal values (Value::operator==), in whatever
    /// order they were put: the comparison of document/Simulation's configEqual() in Java,
    /// `a.keySet().equals(b.keySet())` on Sets and then Objects.equals() of each key's values.
    /// Not in Java's Config, which leaves the comparison to that caller.
    [[nodiscard]] bool sameEntries(const Config& other) const;

private:
    /// The value stored under @p key, or null.
    [[nodiscard]] const Value* find(std::string_view key) const noexcept;

    /// The entries in insertion order.
    std::vector<std::pair<std::string, Value>> m_entries;
};

}  // namespace QtRocket
