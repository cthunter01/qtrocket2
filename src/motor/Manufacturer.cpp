#include "QtRocket/motor/Manufacturer.h"

#include <format>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/motor/Motor.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

/// The process-wide manufacturer list (OpenRocket's static ManufacturerList): every
/// manufacturer ever returned, found by each of its search strings. Manufacturers are never
/// removed, so the references handed out stay valid until the program ends.
class ManufacturerRegistry
{
public:
    [[nodiscard]] static ManufacturerRegistry& instance()
    {
        static ManufacturerRegistry s_registry;
        return s_registry;
    }

    [[nodiscard]] const Manufacturer& get(std::string_view name)
    {
        const std::string      search = Manufacturer::searchString(name);
        const std::scoped_lock lock(m_mutex);
        if (const auto found = m_bySearchName.find(search); found != m_bySearchName.end())
        {
            return *found->second;
        }
        const std::string trimmed(Strings::trim(name));
        return add(trimmed, trimmed, Motor::Type::UNKNOWN, {});
    }

    ManufacturerRegistry(const ManufacturerRegistry&)            = delete;
    ManufacturerRegistry(ManufacturerRegistry&&)                 = delete;
    ManufacturerRegistry& operator=(const ManufacturerRegistry&) = delete;
    ManufacturerRegistry& operator=(ManufacturerRegistry&&)      = delete;
    ~ManufacturerRegistry()                                      = default;

private:
    /// The built-in manufacturers, exactly as Manufacturer's static initializer registers them.
    ManufacturerRegistry()
    {
        using Type = Motor::Type;

        // AeroTech has many name combinations...
        std::vector<std::string> names;
        for (const std::string_view s : {"A", "AT", "AERO", "AEROT", "AEROTECH"})
        {
            names.emplace_back(s);
            names.push_back(std::format("{}-RMS", s));
            names.push_back(std::format("{}-RCS", s));
            names.push_back(std::format("RCS-{}", s));
            names.push_back(std::format("{}-APOGEE", s));
        }
        names.emplace_back("ISP");

        // Aerotech has single-use, reload and hybrid motors
        add("AeroTech", "AeroTech", Type::UNKNOWN, names);

        add("Alpha Hybrid Rocketry LLC", "Alpha Hybrid Rocketry", Type::HYBRID,
            {"AHR", "ALPHA", "ALPHA HYBRID", "ALPHA HYBRIDS", "ALPHA HYBRIDS ROCKETRY"});

        add("Animal Motor Works", "Animal Motor Works", Type::RELOAD, {"AMW", "AW", "ANIMAL"});

        add("Apogee", "Apogee", Type::SINGLE, {"AP", "APOG", "P"});

        add("Cesaroni Technology Inc.", "Cesaroni Technology", Type::RELOAD,
            {"CES", "CESARONI", "CESARONI TECHNOLOGY INCORPORATED", "CTI", "CS", "CSR", "PRO38",
             "ABC"});

        add("Contrail Rockets", "Contrail Rockets", Type::HYBRID,
            {"CR", "CONTR", "CONTRAIL", "CONTRAIL ROCKET"});

        add("Estes", "Estes", Type::SINGLE, {"E", "ES"});

        // Ellis Mountain has both single-use and reload motors
        add("Ellis Mountain", "Ellis Mountain", Type::UNKNOWN,
            {"EM", "ELLIS", "ELLIS MOUNTAIN ROCKET", "ELLIS MOUNTAIN ROCKETS"});

        add("Gorilla Rocket Motors", "Gorilla Rocket Motors", Type::RELOAD,
            {"GR", "GORILLA", "GORILLA ROCKET", "GORILLA ROCKETS", "GORILLA MOTOR",
             "GORILLA MOTORS", "GORILLA ROCKET MOTOR"});

        add("HyperTEK", "HyperTEK", Type::HYBRID, {"H", "HT", "HYPER"});

        add("Kosdon by AeroTech", "Kosdon by AeroTech", Type::RELOAD,
            {"K", "KBA", "K-AT", "KOS", "KOSDON", "KOSDON/AT", "KOSDON/AEROTECH"});

        add("LOC/Precision", "LOC/Precision", Type::UNKNOWN, {"LOC"});

        add("Loki Research", "Loki Research", Type::RELOAD, {"LOKI", "LR"});

        add("Public Missiles, Ltd.", "Public Missiles", Type::SINGLE,
            {"PM", "PML", "PUBLIC MISSILES LIMITED"});

        add("Propulsion Polymers", "Propulsion Polymers", Type::HYBRID,
            {"PP", "PROP", "PROPULSION"});

        add("Quest", "Quest", Type::SINGLE, {"Q", "QU"});

        add("RATT Works", "RATT Works", Type::HYBRID, {"RATT", "RT", "RTW"});

        add("Roadrunner Rocketry", "Roadrunner Rocketry", Type::SINGLE, {"RR", "ROADRUNNER"});

        add("Rocketvision", "Rocketvision", Type::SINGLE, {"RV", "ROCKET VISION"});

        add("Sky Ripper Systems", "Sky Ripper Systems", Type::HYBRID,
            {"SR", "SRS", "SKYR", "SKYRIPPER", "SKY RIPPER", "SKYRIPPER SYSTEMS"});

        add("West Coast Hybrids", "West Coast Hybrids", Type::HYBRID,
            {"WCH", "WCR", "WEST COAST", "WEST COAST HYBRID"});

        // German WECO Feuerwerk, previously Sachsen Feuerwerk
        add("WECO Feuerwerk", "WECO Feuerwerk", Type::SINGLE,
            {"WECO", "WECO FEUERWERKS", "SF", "SACHSEN", "SACHSEN FEUERWERK",
             "SACHSEN FEUERWERKS"});
    }

    /// Registers a new manufacturer under each of its search strings. A search string that is
    /// already taken is a bug in the built-in list (OpenRocket: IllegalStateException).
    const Manufacturer& add(std::string displayName, std::string simpleName, Motor::Type type,
                            const std::vector<std::string>& alternateNames)
    {
        auto manufacturer =
            std::make_unique<const Manufacturer>(Manufacturer::PassKey{}, std::move(displayName),
                                                 std::move(simpleName), type, alternateNames);
        for (const std::string& search : manufacturer->getSearchNames())
        {
            const auto [previous, inserted] = m_bySearchName.emplace(search, manufacturer.get());
            if (!inserted)
            {
                bug(std::format("Manufacturer name clash between manufacturers {} and {} name {}",
                                previous->second->toString(), manufacturer->toString(), search));
            }
        }
        m_manufacturers.push_back(std::move(manufacturer));
        return *m_manufacturers.back();
    }

    std::mutex                                              m_mutex;
    std::vector<std::unique_ptr<const Manufacturer>>        m_manufacturers;
    std::map<std::string, const Manufacturer*, std::less<>> m_bySearchName;
};

Manufacturer::Manufacturer(PassKey /*key*/, std::string displayName, std::string simpleName,
                           Motor::Type motorType, const std::vector<std::string>& alternateNames)
  : m_displayName(std::move(displayName)),
    m_simpleName(std::move(simpleName)),
    m_motorType(motorType)
{
    m_allNames.insert(m_displayName);
    m_allNames.insert(m_simpleName);
    m_searchNames.insert(searchString(m_displayName));
    m_searchNames.insert(searchString(m_simpleName));
    for (const std::string& name : alternateNames)
    {
        m_allNames.insert(name);
        m_searchNames.insert(searchString(name));
    }
}

const Manufacturer& Manufacturer::getManufacturer(std::string_view name)
{
    return ManufacturerRegistry::instance().get(name);
}

bool Manufacturer::matches(std::string_view name) const
{
    return m_searchNames.contains(searchString(name));
}

std::string Manufacturer::searchString(std::string_view name)
{
    std::string out;
    out.reserve(name.size());
    // A run of other characters becomes one space; runs at either end are trimmed away.
    bool       separatorPending = false;
    const auto append           = [&](char c) {
        if (separatorPending && !out.empty())
        {
            out.push_back(' ');
        }
        separatorPending = false;
        out.push_back(c);
    };
    for (const char32_t codePoint : Strings::toCodePoints(name))
    {
        if ((codePoint >= U'0' && codePoint <= U'9') || (codePoint >= U'a' && codePoint <= U'z'))
        {
            append(static_cast<char>(codePoint));
        }
        else if (codePoint >= U'A' && codePoint <= U'Z')
        {
            append(static_cast<char>(codePoint - U'A' + U'a'));
        }
        else if (codePoint == U'\u212A')
        {
            // KELVIN SIGN: its lower case is the ASCII "k".
            append('k');
        }
        else if (codePoint == U'\u0130')
        {
            // LATIN CAPITAL LETTER I WITH DOT ABOVE: "i" plus U+0307, a separator.
            append('i');
            separatorPending = true;
        }
        else
        {
            separatorPending = true;
        }
    }
    return out;
}

}  // namespace QtRocket
