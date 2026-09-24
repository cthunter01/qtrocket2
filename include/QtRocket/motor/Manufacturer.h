#pragma once

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/motor/Motor.h"

namespace QtRocket
{

class ManufacturerRegistry;

/// A motor manufacturer (OpenRocket's Manufacturer). Manufacturers live in a process-wide
/// registry and are compared by identity: getManufacturer() returns the same object for every
/// name that denotes it, so `&a == &b` is OpenRocket's `a == b`. The registry starts with
/// OpenRocket's 22 built-in manufacturers and their aliases; a name that matches none of them
/// registers a new manufacturer, which lives, like the built-in ones, until the program ends:
/// the registry is never destroyed, so a manufacturer stays valid even while other statics are
/// destroyed at exit.
///
/// Names are matched by their search string (see searchString()): "AT/RCS", "at-rcs" and
/// " At rcs " all denote AeroTech, while "aero tech" does not.
class Manufacturer
{
    /// Only the registry can make one (it is a friend and can name this type).
    struct PassKey
    {
        explicit PassKey() = default;
    };
    friend class ManufacturerRegistry;

public:
    /// The manufacturer called @p name: the registered one whose display name, simple name or
    /// alias gives the same search string, or else a new one whose display and simple names are
    /// @p name trimmed (Java's String.trim), of type UNKNOWN, registered under that search
    /// string. Thread-safe.
    [[nodiscard]] static const Manufacturer& getManufacturer(std::string_view name);

    /// Constructs a manufacturer; only the registry can, since PassKey is private.
    Manufacturer(PassKey /*key*/, std::string displayName, std::string simpleName,
                 Motor::Type motorType, const std::vector<std::string>& alternateNames);

    Manufacturer(const Manufacturer&)            = delete;
    Manufacturer(Manufacturer&&)                 = delete;
    Manufacturer& operator=(const Manufacturer&) = delete;
    Manufacturer& operator=(Manufacturer&&)      = delete;
    ~Manufacturer()                              = default;

    /// The name to show to the user, e.g. "Cesaroni Technology Inc.".
    [[nodiscard]] const std::string& getDisplayName() const noexcept { return m_displayName; }

    /// The simple name, e.g. "Cesaroni Technology": what .ork files store for compatibility.
    [[nodiscard]] const std::string& getSimpleName() const noexcept { return m_simpleName; }

    /// Every name of the manufacturer: the display and simple names and every alias (such as
    /// "A" for AeroTech), as given. OpenRocket's set is unordered; this one is sorted.
    [[nodiscard]] const std::set<std::string>& getAllNames() const noexcept { return m_allNames; }

    /// The search strings of getAllNames(), under which the registry finds the manufacturer.
    [[nodiscard]] const std::set<std::string>& getSearchNames() const noexcept
    {
        return m_searchNames;
    }

    /// The motor type the manufacturer makes when it makes only one, otherwise UNKNOWN.
    [[nodiscard]] Motor::Type getMotorType() const noexcept { return m_motorType; }

    /// True when @p name denotes this manufacturer: its search string is one of
    /// getSearchNames(). (OpenRocket's matches(null) is false; a view cannot be null.)
    [[nodiscard]] bool matches(std::string_view name) const;

    /// The display name (Manufacturer.toString()).
    [[nodiscard]] const std::string& toString() const noexcept { return m_displayName; }

    /// The search string of @p name (generateSearchString): lower-cased as Java's
    /// String.toLowerCase does in a non-Turkish default locale (ASCII letters, and the Kelvin
    /// sign and the dotted capital I, which become "k" and "i" plus a combining dot), every run
    /// of characters other than ASCII letters and digits replaced by one space, and trimmed.
    [[nodiscard]] static std::string searchString(std::string_view name);

private:
    std::string           m_displayName;
    std::string           m_simpleName;
    std::set<std::string> m_allNames;
    std::set<std::string> m_searchNames;
    Motor::Type           m_motorType;
};

}  // namespace QtRocket
