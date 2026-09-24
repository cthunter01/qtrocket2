#include "QtRocket/motor/Motor.h"

#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/preferences/Preferences.h"

namespace QtRocket
{

const std::string& Motor::getMotorName(const Preferences& preferences) const
{
    const bool useDesignation = preferences.getMotorNameColumn();
    return useDesignation ? getDesignation() : getCommonName();
}

std::string Motor::getMotorName(const Preferences& preferences, double delay) const
{
    const bool useDesignation = preferences.getMotorNameColumn();
    return useDesignation ? getDesignation(delay) : getCommonName(delay);
}

std::string_view name(Motor::Type type) noexcept
{
    switch (type)
    {
        case Motor::Type::SINGLE:
            return "Single-use";
        case Motor::Type::RELOAD:
            return "Reloadable";
        case Motor::Type::HYBRID:
            return "Hybrid";
        case Motor::Type::UNKNOWN:
            return "Unknown";
    }
    return "Unknown";
}

std::string_view description(Motor::Type type) noexcept
{
    switch (type)
    {
        case Motor::Type::SINGLE:
            return "Single-use solid propellant motor";
        case Motor::Type::RELOAD:
            return "Reloadable solid propellant motor";
        case Motor::Type::HYBRID:
            return "Hybrid rocket motor engine";
        case Motor::Type::UNKNOWN:
            return "Unknown motor type";
    }
    return "Unknown motor type";
}

std::string_view orkName(Motor::Type type) noexcept
{
    switch (type)
    {
        case Motor::Type::SINGLE:
            return "single";
        case Motor::Type::RELOAD:
            return "reload";
        case Motor::Type::HYBRID:
            return "hybrid";
        case Motor::Type::UNKNOWN:
            return "unknown";
    }
    return "unknown";
}

std::optional<Motor::Type> motorTypeFromOrkName(std::string_view name) noexcept
{
    for (const Motor::Type type : Motor::kAllTypes)
    {
        if (orkName(type) == name)
        {
            return type;
        }
    }
    return std::nullopt;
}

std::string_view enumName(Motor::Type type) noexcept
{
    switch (type)
    {
        case Motor::Type::SINGLE:
            return "SINGLE";
        case Motor::Type::RELOAD:
            return "RELOAD";
        case Motor::Type::HYBRID:
            return "HYBRID";
        case Motor::Type::UNKNOWN:
            return "UNKNOWN";
    }
    return "UNKNOWN";
}

std::optional<Motor::Type> motorTypeFromEnumName(std::string_view name) noexcept
{
    for (const Motor::Type type : Motor::kAllTypes)
    {
        if (enumName(type) == name)
        {
            return type;
        }
    }
    return std::nullopt;
}

}  // namespace QtRocket
