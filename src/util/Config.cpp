#include "QtRocket/util/Config.h"

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "QtRocket/util/BigDecimal.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

namespace
{

/// The types Java's Number covers in a Config.
template <typename T>
concept ConfigNumber = ConfigInteger<T> || std::floating_point<T> || std::same_as<T, BigDecimal>;

/// validateType()'s null check, for the one value that can be null here.
[[nodiscard]] std::string nonNullString(const char* value)
{
    if (value == nullptr)
    {
        bug("Attempting to add null value to Config object");
    }
    return {value};
}

/// The (int) cast of a Java long: the low 32 bits.
[[nodiscard]] constexpr std::int32_t lowInt(std::int64_t value) noexcept
{
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(value)));
}

/// Java's Double.equals() and Float.equals(): the same bits once every NaN is the same NaN.
[[nodiscard]] bool javaFloatingEquals(double a, double b) noexcept
{
    return MathUtil::javaDoubleCompare(a, b) == 0;
}

struct IntValueOf
{
    [[nodiscard]] std::optional<std::int32_t> operator()(std::int64_t value) const noexcept
    {
        return lowInt(value);
    }
    [[nodiscard]] std::optional<std::int32_t> operator()(std::int32_t value) const noexcept
    {
        return value;
    }
    [[nodiscard]] std::optional<std::int32_t> operator()(std::int16_t value) const noexcept
    {
        return std::int32_t{value};
    }
    [[nodiscard]] std::optional<std::int32_t> operator()(std::int8_t value) const noexcept
    {
        return std::int32_t{value};
    }
    [[nodiscard]] std::optional<std::int32_t> operator()(float value) const noexcept
    {
        return MathUtil::javaIntCast(static_cast<double>(value));
    }
    [[nodiscard]] std::optional<std::int32_t> operator()(double value) const noexcept
    {
        return MathUtil::javaIntCast(value);
    }
    [[nodiscard]] std::optional<std::int32_t> operator()(const BigDecimal& value) const noexcept
    {
        return value.intValue();
    }
    [[nodiscard]] std::optional<std::int32_t> operator()(bool /*value*/) const noexcept
    {
        return std::nullopt;
    }
    [[nodiscard]] std::optional<std::int32_t> operator()(
        const std::string& /*value*/) const noexcept
    {
        return std::nullopt;
    }
    [[nodiscard]] std::optional<std::int32_t> operator()(
        const Config::List& /*value*/) const noexcept
    {
        return std::nullopt;
    }
};

struct LongValueOf
{
    template <ConfigInteger T>
    [[nodiscard]] std::optional<std::int64_t> operator()(T value) const noexcept
    {
        return std::int64_t{value};
    }
    [[nodiscard]] std::optional<std::int64_t> operator()(float value) const noexcept
    {
        return MathUtil::javaLongCast(static_cast<double>(value));
    }
    [[nodiscard]] std::optional<std::int64_t> operator()(double value) const noexcept
    {
        return MathUtil::javaLongCast(value);
    }
    [[nodiscard]] std::optional<std::int64_t> operator()(const BigDecimal& value) const noexcept
    {
        return value.longValue();
    }
    [[nodiscard]] std::optional<std::int64_t> operator()(bool /*value*/) const noexcept
    {
        return std::nullopt;
    }
    [[nodiscard]] std::optional<std::int64_t> operator()(
        const std::string& /*value*/) const noexcept
    {
        return std::nullopt;
    }
    [[nodiscard]] std::optional<std::int64_t> operator()(
        const Config::List& /*value*/) const noexcept
    {
        return std::nullopt;
    }
};

struct DoubleValueOf
{
    template <ConfigInteger T>
    [[nodiscard]] std::optional<double> operator()(T value) const noexcept
    {
        return static_cast<double>(value);
    }
    [[nodiscard]] std::optional<double> operator()(float value) const noexcept
    {
        return static_cast<double>(value);
    }
    [[nodiscard]] std::optional<double> operator()(double value) const noexcept { return value; }
    [[nodiscard]] std::optional<double> operator()(const BigDecimal& value) const
    {
        return value.doubleValue();
    }
    [[nodiscard]] std::optional<double> operator()(bool /*value*/) const noexcept
    {
        return std::nullopt;
    }
    [[nodiscard]] std::optional<double> operator()(const std::string& /*value*/) const noexcept
    {
        return std::nullopt;
    }
    [[nodiscard]] std::optional<double> operator()(const Config::List& /*value*/) const noexcept
    {
        return std::nullopt;
    }
};

/// Java's equals() of two stored objects of the same type.
struct JavaEquals
{
    [[nodiscard]] bool operator()(float a, float b) const noexcept
    {
        return javaFloatingEquals(static_cast<double>(a), static_cast<double>(b));
    }
    [[nodiscard]] bool operator()(double a, double b) const noexcept
    {
        return javaFloatingEquals(a, b);
    }
    template <typename T>
        requires(!std::floating_point<T>)
    [[nodiscard]] bool operator()(const T& a, const T& b) const
    {
        return a == b;
    }
    /// Objects of different classes are never equal.
    template <typename T, typename U>
        requires(!std::same_as<T, U>)
    [[nodiscard]] bool operator()(const T& /*a*/, const U& /*b*/) const noexcept
    {
        return false;
    }
};

}  // namespace

Config::Value::Value(bool value) noexcept : m_data(value) { }

Config::Value::Value(float value) noexcept : m_data(value) { }

Config::Value::Value(double value) noexcept : m_data(value) { }

Config::Value::Value(BigDecimal value) noexcept : m_data(std::move(value)) { }

Config::Value::Value(std::string value) noexcept : m_data(std::move(value)) { }

Config::Value::Value(std::string_view value) : m_data(std::string(value)) { }

Config::Value::Value(const char* value) : m_data(nonNullString(value)) { }

Config::Value::Value(List value) noexcept : m_data(std::move(value)) { }

bool Config::Value::isNumber() const
{
    return std::visit([]<typename T>(const T& /*value*/) { return ConfigNumber<T>; }, m_data);
}

std::optional<std::int32_t> Config::Value::intValue() const
{
    return std::visit(IntValueOf{}, m_data);
}

std::optional<std::int64_t> Config::Value::longValue() const
{
    return std::visit(LongValueOf{}, m_data);
}

std::optional<double> Config::Value::doubleValue() const
{
    return std::visit(DoubleValueOf{}, m_data);
}

bool Config::Value::operator==(const Value& other) const
{
    return std::visit(JavaEquals{}, m_data, other.m_data);
}

void Config::put(std::string_view key, Value value)
{
    const auto entry = std::ranges::find(m_entries, key, &std::pair<std::string, Value>::first);
    if (entry != m_entries.end())
    {
        entry->second = std::move(value);
        return;
    }
    m_entries.emplace_back(std::string(key), std::move(value));
}

void Config::put(std::string_view key, const char* value)
{
    put(key, Value(value));
}

const Config::Value* Config::find(std::string_view key) const noexcept
{
    const auto entry = std::ranges::find(m_entries, key, &std::pair<std::string, Value>::first);
    return entry != m_entries.end() ? &entry->second : nullptr;
}

std::optional<Config::Value> Config::get(std::string_view key) const
{
    const Value* const value = find(key);
    return value != nullptr ? std::optional<Value>(*value) : std::nullopt;
}

Config::Value Config::get(std::string_view key, const Value& def) const
{
    const Value* const value = find(key);
    return value != nullptr ? *value : def;
}

std::optional<bool> Config::getBoolean(std::string_view key) const
{
    const Value* const value = find(key);
    if (value == nullptr)
    {
        return std::nullopt;
    }
    const bool* const stored = std::get_if<bool>(&value->variant());
    return stored != nullptr ? std::optional<bool>(*stored) : std::nullopt;
}

bool Config::getBoolean(std::string_view key, bool def) const
{
    return getBoolean(key).value_or(def);
}

std::optional<std::int32_t> Config::getInt(std::string_view key) const
{
    const Value* const value = find(key);
    return value != nullptr ? value->intValue() : std::nullopt;
}

std::int32_t Config::getInt(std::string_view key, std::int32_t def) const
{
    return getInt(key).value_or(def);
}

std::optional<std::int64_t> Config::getLong(std::string_view key) const
{
    const Value* const value = find(key);
    return value != nullptr ? value->longValue() : std::nullopt;
}

std::int64_t Config::getLong(std::string_view key, std::int64_t def) const
{
    return getLong(key).value_or(def);
}

std::optional<double> Config::getDouble(std::string_view key) const
{
    const Value* const value = find(key);
    return value != nullptr ? value->doubleValue() : std::nullopt;
}

double Config::getDouble(std::string_view key, double def) const
{
    return getDouble(key).value_or(def);
}

std::optional<std::string> Config::getString(std::string_view key) const
{
    const Value* const value = find(key);
    if (value == nullptr)
    {
        return std::nullopt;
    }
    const std::string* const stored = std::get_if<std::string>(&value->variant());
    return stored != nullptr ? std::optional<std::string>(*stored) : std::nullopt;
}

std::string Config::getString(std::string_view key, std::string_view def) const
{
    if (std::optional<std::string> value = getString(key); value.has_value())
    {
        return std::move(*value);
    }
    return std::string(def);
}

std::optional<Config::List> Config::getList(std::string_view key) const
{
    const Value* const value = find(key);
    if (value == nullptr)
    {
        return std::nullopt;
    }
    const List* const stored = std::get_if<List>(&value->variant());
    return stored != nullptr ? std::optional<List>(*stored) : std::nullopt;
}

Config::List Config::getList(std::string_view key, const List& def) const
{
    if (std::optional<List> value = getList(key); value.has_value())
    {
        return std::move(*value);
    }
    return def;
}

bool Config::containsKey(std::string_view key) const noexcept
{
    return find(key) != nullptr;
}

std::vector<std::string> Config::keySet() const
{
    std::vector<std::string> keys;
    keys.reserve(m_entries.size());
    for (const auto& [key, value] : m_entries)
    {
        keys.push_back(key);
    }
    return keys;
}

}  // namespace QtRocket
