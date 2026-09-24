#include "QtRocket/preferences/DocumentPreferences.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "QtRocket/util/Color.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// Double.equals(): compares doubleToLongBits, so NaN equals NaN and 0.0 differs from -0.0.
[[nodiscard]] bool javaDoubleEquals(double a, double b) noexcept
{
    if (std::isnan(a) || std::isnan(b))
    {
        return std::isnan(a) && std::isnan(b);
    }
    return a == b && std::signbit(a) == std::signbit(b);
}

}  // namespace

bool DocumentPreferences::DocumentPreference::operator==(const DocumentPreference& other) const
{
    const double* const mine   = std::get_if<double>(&m_value);
    const double* const theirs = std::get_if<double>(&other.m_value);
    if (mine != nullptr && theirs != nullptr)
    {
        return javaDoubleEquals(*mine, *theirs);
    }
    return m_value == other.m_value;
}

std::string_view DocumentPreferences::typeName(Type type) noexcept
{
    switch (type)
    {
        case Type::BOOLEAN:
            return "boolean";
        case Type::INTEGER:
            return "integer";
        case Type::DOUBLE:
            return "double";
        case Type::STRING:
            return "string";
    }
    return "string";
}

DocumentPreferences::DocumentPreferences(const DocumentPreferences& other)
  : m_preferences(other.m_preferences), m_materials(other.m_materials)
{
}

DocumentPreferences& DocumentPreferences::operator=(const DocumentPreferences& other)
{
    if (this != &other)
    {
        m_preferences = other.m_preferences;
        m_materials   = other.m_materials;
    }
    return *this;
}

// ----------------------------------------------------------- ORPreferences accessors

template <class T>
std::optional<T> DocumentPreferences::getAs(std::string_view key) const
{
    const DocumentPreference* const pref = find(key);
    if (pref == nullptr)
    {
        return std::nullopt;
    }
    const T* const value = std::get_if<T>(&pref->value());
    if (value == nullptr)
    {
        return std::nullopt;
    }
    return *value;
}

bool DocumentPreferences::getBoolean(std::string_view key, bool defaultValue) const
{
    return getAs<bool>(key).value_or(defaultValue);
}

void DocumentPreferences::putBoolean(std::string_view key, bool value)
{
    putPreference(key, value);
}

int DocumentPreferences::getInt(std::string_view key, int defaultValue) const
{
    return getAs<int>(key).value_or(defaultValue);
}

void DocumentPreferences::putInt(std::string_view key, int value)
{
    putPreference(key, value);
}

double DocumentPreferences::getDouble(std::string_view key, double defaultValue) const
{
    return getAs<double>(key).value_or(defaultValue);
}

void DocumentPreferences::putDouble(std::string_view key, double value)
{
    putPreference(key, value);
}

std::string DocumentPreferences::getString(std::string_view key,
                                           std::string_view defaultValue) const
{
    return getAs<std::string>(key).value_or(std::string(defaultValue));
}

void DocumentPreferences::putString(std::string_view key, std::string_view value)
{
    putPreference(key, std::string(value));
}

std::optional<Color> DocumentPreferences::getColor(std::string_view key) const
{
    const std::optional<std::string> text = getAs<std::string>(key);
    if (!text.has_value())
    {
        return std::nullopt;
    }
    return parseColor(*text);
}

Color DocumentPreferences::getColor(std::string_view key, const Color& defaultValue) const
{
    return getColor(key).value_or(defaultValue);
}

void DocumentPreferences::putColor(std::string_view key, const std::optional<Color>& value)
{
    if (!value.has_value())
    {
        removePreference(key);
    }
    else
    {
        putPreference(key, stringifyColor(*value));
    }
}

std::optional<Color> DocumentPreferences::parseColor(std::string_view text)
{
    const std::vector<std::string> rgb = Strings::splitJava(text, ',');
    if (rgb.size() != 3)
    {
        return std::nullopt;
    }
    const std::optional<int> red   = Strings::parseInt(Strings::trim(rgb[0]));
    const std::optional<int> green = Strings::parseInt(Strings::trim(rgb[1]));
    const std::optional<int> blue  = Strings::parseInt(Strings::trim(rgb[2]));
    if (!red.has_value() || !green.has_value() || !blue.has_value())
    {
        return std::nullopt;
    }
    return Color(MathUtil::clamp(*red, 0, 255), MathUtil::clamp(*green, 0, 255),
                 MathUtil::clamp(*blue, 0, 255));
}

std::string DocumentPreferences::stringifyColor(const Color& color)
{
    return std::format("{},{},{}", color.red(), color.green(), color.blue());
}

void DocumentPreferences::putPreference(std::string_view key, Value value)
{
    const DocumentPreference candidate(std::move(value));
    const auto               current = m_preferences.find(key);
    if (current != m_preferences.end() && current->second == candidate)
    {
        return;
    }
    m_preferences.insert_or_assign(std::string(key), candidate);
    m_changed.emit();
}

// ------------------------------------------------------------------------ the map

const DocumentPreferences::DocumentPreference* DocumentPreferences::find(
    std::string_view key) const noexcept
{
    const auto it = m_preferences.find(key);
    if (it == m_preferences.end())
    {
        return nullptr;
    }
    return &it->second;
}

bool DocumentPreferences::contains(std::string_view key) const noexcept
{
    return m_preferences.contains(key);
}

void DocumentPreferences::removePreference(std::string_view key)
{
    const auto it = m_preferences.find(key);
    if (it == m_preferences.end())
    {
        return;
    }
    m_preferences.erase(it);
    m_changed.emit();
}

// ---------------------------------------------------------------------- materials

bool DocumentPreferences::addMaterial(std::string_view storableString)
{
    const auto asView = [](const std::string& material) { return std::string_view(material); };
    if (std::ranges::find(m_materials, storableString, asView) != m_materials.end())
    {
        return false;
    }
    m_materials.emplace_back(storableString);
    return true;
}

bool DocumentPreferences::removeMaterial(std::string_view storableString)
{
    const auto asView = [](const std::string& material) { return std::string_view(material); };
    const auto it     = std::ranges::find(m_materials, storableString, asView);
    if (it == m_materials.end())
    {
        return false;
    }
    m_materials.erase(it);
    return true;
}

}  // namespace QtRocket
