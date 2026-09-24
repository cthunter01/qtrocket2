#include "QtRocket/motor/Manufacturer.h"

#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/motor/Motor.h"

namespace
{

using QtRocket::Manufacturer;
using QtRocket::Motor;

// ---- Ported from ManufacturerTest.java ----

TEST(Manufacturer, Existing)
{
    const Manufacturer& m1 = Manufacturer::getManufacturer("aerotech");
    const Manufacturer& m2 = Manufacturer::getManufacturer("a ");
    const Manufacturer& m3 = Manufacturer::getManufacturer("-isp-");
    const Manufacturer& m4 = Manufacturer::getManufacturer("at/rcs");
    const Manufacturer& m5 = Manufacturer::getManufacturer("e");

    EXPECT_EQ(&m1, &m2);
    EXPECT_EQ(&m1, &m3);
    EXPECT_EQ(&m1, &m4);
    EXPECT_NE(&m1, &m5);
}

TEST(Manufacturer, Contrail)
{
    const Manufacturer& c1 = Manufacturer::getManufacturer("Contrail");

    // Used in rsp files.
    const Manufacturer& c2 = Manufacturer::getManufacturer("Contrail_Rockets");

    EXPECT_EQ(&c1, &c2);
    EXPECT_EQ(c1.getDisplayName(), "Contrail Rockets");
}

TEST(Manufacturer, New)
{
    const Manufacturer& m1 = Manufacturer::getManufacturer("Unknown");
    const Manufacturer& m2 = Manufacturer::getManufacturer(" Unknown/ ");
    const Manufacturer& m3 = Manufacturer::getManufacturer("Unknown/a");

    EXPECT_EQ(m1.getDisplayName(), "Unknown");
    EXPECT_EQ(m2.getDisplayName(), "Unknown");
    EXPECT_EQ(&m1, &m2);

    EXPECT_EQ(m3.getDisplayName(), "Unknown/a");
    EXPECT_NE(&m1, &m3);
}

TEST(Manufacturer, SimpleName)
{
    const Manufacturer& m1 = Manufacturer::getManufacturer("cs");
    const Manufacturer& m2 = Manufacturer::getManufacturer("Cesaroni Technology");
    const Manufacturer& m3 = Manufacturer::getManufacturer("Cesaroni Technology Inc");
    const Manufacturer& m4 = Manufacturer::getManufacturer("Cesaroni Technology Inc.");

    EXPECT_EQ(m1.getDisplayName(), "Cesaroni Technology Inc.");
    EXPECT_EQ(m1.toString(), "Cesaroni Technology Inc.");
    EXPECT_EQ(m1.getSimpleName(), "Cesaroni Technology");

    EXPECT_EQ(&m1, &m2);
    EXPECT_EQ(&m1, &m3);
    EXPECT_EQ(&m1, &m4);
}

TEST(Manufacturer, Matches)
{
    const Manufacturer& m1 = Manufacturer::getManufacturer("aerotech");

    EXPECT_TRUE(m1.matches("a"));
    EXPECT_TRUE(m1.matches("a/"));
    EXPECT_TRUE(m1.matches("a/rcs"));
    EXPECT_TRUE(m1.matches("a/rms"));
    EXPECT_TRUE(m1.matches("aerotech  ...-/%#_!"));
    EXPECT_TRUE(m1.matches(" .isp/"));

    EXPECT_FALSE(m1.matches("aero/tech"));
    EXPECT_FALSE(m1.matches("aero.tech"));
    EXPECT_FALSE(m1.matches("aero_tech"));
    EXPECT_FALSE(m1.matches("aero tech"));
}

// ---- Lookups pinned by running OpenRocket's Manufacturer.getManufacturer ----

/// A name and the manufacturer OpenRocket finds for it.
struct Lookup
{
    Lookup(std::string_view nameValue, std::string_view display, std::string_view simple,
           Motor::Type motorType)
      : name(nameValue), displayName(display), simpleName(simple), type(motorType)
    {
    }

    std::string_view name;
    std::string_view displayName;
    std::string_view simpleName;
    Motor::Type      type;
};

void expectLookup(const Lookup& lookup)
{
    const Manufacturer& found = Manufacturer::getManufacturer(lookup.name);
    EXPECT_EQ(found.getDisplayName(), lookup.displayName) << lookup.name;
    EXPECT_EQ(found.getSimpleName(), lookup.simpleName) << lookup.name;
    EXPECT_EQ(found.getMotorType(), lookup.type) << lookup.name;
}

TEST(Manufacturer, LookupsMatchOpenRocket)
{
    const std::vector<Lookup> lookups{
        {"aerotech", "AeroTech", "AeroTech", Motor::Type::UNKNOWN},
        {"a ", "AeroTech", "AeroTech", Motor::Type::UNKNOWN},
        {"-isp-", "AeroTech", "AeroTech", Motor::Type::UNKNOWN},
        {"at/rcs", "AeroTech", "AeroTech", Motor::Type::UNKNOWN},
        {"e", "Estes", "Estes", Motor::Type::SINGLE},
        {"Contrail_Rockets", "Contrail Rockets", "Contrail Rockets", Motor::Type::HYBRID},
        {"cs", "Cesaroni Technology Inc.", "Cesaroni Technology", Motor::Type::RELOAD},
        {"Cesaroni Technology Inc.", "Cesaroni Technology Inc.", "Cesaroni Technology",
         Motor::Type::RELOAD},
        {"PRO38", "Cesaroni Technology Inc.", "Cesaroni Technology", Motor::Type::RELOAD},
        {"ABC", "Cesaroni Technology Inc.", "Cesaroni Technology", Motor::Type::RELOAD},
        {"Public Missiles, Ltd.", "Public Missiles, Ltd.", "Public Missiles", Motor::Type::SINGLE},
        {"sachsen feuerwerks", "WECO Feuerwerk", "WECO Feuerwerk", Motor::Type::SINGLE},
        {"WECO", "WECO Feuerwerk", "WECO Feuerwerk", Motor::Type::SINGLE},
        {"LOC", "LOC/Precision", "LOC/Precision", Motor::Type::UNKNOWN},
        {"K-AT", "Kosdon by AeroTech", "Kosdon by AeroTech", Motor::Type::RELOAD},
        {"kosdon/aerotech", "Kosdon by AeroTech", "Kosdon by AeroTech", Motor::Type::RELOAD},
        {"Kosdon by AeroTech", "Kosdon by AeroTech", "Kosdon by AeroTech", Motor::Type::RELOAD},
        {"Alpha Hybrids Rocketry", "Alpha Hybrid Rocketry LLC", "Alpha Hybrid Rocketry",
         Motor::Type::HYBRID},
        {"p", "Apogee", "Apogee", Motor::Type::SINGLE},
        {"  NewCo  ", "NewCo", "NewCo", Motor::Type::UNKNOWN},
        {"newco", "NewCo", "NewCo", Motor::Type::UNKNOWN},
        // The dotted capital I lower-cases to "i" plus a combining dot, which separates it from
        // "SP", so no alias matches; the Kelvin sign lower-cases to an ASCII "k".
        {"\u0130SP", "\u0130SP", "\u0130SP", Motor::Type::UNKNOWN},
        {"\u212AOS", "Kosdon by AeroTech", "Kosdon by AeroTech", Motor::Type::RELOAD},
    };
    for (const Lookup& lookup : lookups)
    {
        expectLookup(lookup);
    }
}

// ---- The built-in list ----

/// One of OpenRocket's built-in manufacturers and its number of aliases.
struct BuiltIn
{
    BuiltIn(std::string_view display, std::string_view simple, Motor::Type motorType,
            std::size_t aliases)
      : displayName(display), simpleName(simple), type(motorType), aliasCount(aliases)
    {
    }

    std::string_view displayName;
    std::string_view simpleName;
    Motor::Type      type;
    std::size_t      aliasCount;
};

/// Every name of @p manufacturer, aliases included, leads back to it.
void expectAllNamesLeadBack(const Manufacturer& manufacturer)
{
    for (const std::string& name : manufacturer.getAllNames())
    {
        EXPECT_EQ(&Manufacturer::getManufacturer(name), &manufacturer) << name;
        EXPECT_TRUE(manufacturer.matches(name)) << name;
    }
}

/// Checks a built-in manufacturer and returns it.
const Manufacturer& expectBuiltIn(const BuiltIn& builtIn)
{
    const Manufacturer& byDisplay = Manufacturer::getManufacturer(builtIn.displayName);
    const Manufacturer& bySimple  = Manufacturer::getManufacturer(builtIn.simpleName);
    EXPECT_EQ(&byDisplay, &bySimple) << builtIn.displayName;
    EXPECT_EQ(byDisplay.getDisplayName(), builtIn.displayName);
    EXPECT_EQ(byDisplay.getSimpleName(), builtIn.simpleName);
    EXPECT_EQ(byDisplay.getMotorType(), builtIn.type) << builtIn.displayName;
    expectAllNamesLeadBack(byDisplay);
    // The display and simple names plus the aliases (display = simple counts once).
    const std::size_t ownNames = builtIn.displayName == builtIn.simpleName ? 1 : 2;
    EXPECT_EQ(byDisplay.getAllNames().size(), ownNames + builtIn.aliasCount) << builtIn.displayName;
    return byDisplay;
}

TEST(Manufacturer, BuiltInManufacturers)
{
    const std::vector<BuiltIn> builtIns{
        {"AeroTech", "AeroTech", Motor::Type::UNKNOWN, 26},
        {"Alpha Hybrid Rocketry LLC", "Alpha Hybrid Rocketry", Motor::Type::HYBRID, 5},
        {"Animal Motor Works", "Animal Motor Works", Motor::Type::RELOAD, 3},
        {"Apogee", "Apogee", Motor::Type::SINGLE, 3},
        {"Cesaroni Technology Inc.", "Cesaroni Technology", Motor::Type::RELOAD, 8},
        {"Contrail Rockets", "Contrail Rockets", Motor::Type::HYBRID, 4},
        {"Estes", "Estes", Motor::Type::SINGLE, 2},
        {"Ellis Mountain", "Ellis Mountain", Motor::Type::UNKNOWN, 4},
        {"Gorilla Rocket Motors", "Gorilla Rocket Motors", Motor::Type::RELOAD, 7},
        {"HyperTEK", "HyperTEK", Motor::Type::HYBRID, 3},
        {"Kosdon by AeroTech", "Kosdon by AeroTech", Motor::Type::RELOAD, 7},
        {"LOC/Precision", "LOC/Precision", Motor::Type::UNKNOWN, 1},
        {"Loki Research", "Loki Research", Motor::Type::RELOAD, 2},
        {"Public Missiles, Ltd.", "Public Missiles", Motor::Type::SINGLE, 3},
        {"Propulsion Polymers", "Propulsion Polymers", Motor::Type::HYBRID, 3},
        {"Quest", "Quest", Motor::Type::SINGLE, 2},
        {"RATT Works", "RATT Works", Motor::Type::HYBRID, 3},
        {"Roadrunner Rocketry", "Roadrunner Rocketry", Motor::Type::SINGLE, 2},
        {"Rocketvision", "Rocketvision", Motor::Type::SINGLE, 2},
        {"Sky Ripper Systems", "Sky Ripper Systems", Motor::Type::HYBRID, 6},
        {"West Coast Hybrids", "West Coast Hybrids", Motor::Type::HYBRID, 4},
        {"WECO Feuerwerk", "WECO Feuerwerk", Motor::Type::SINGLE, 6},
    };
    std::set<const Manufacturer*> distinct;
    for (const BuiltIn& builtIn : builtIns)
    {
        distinct.insert(&expectBuiltIn(builtIn));
    }
    EXPECT_EQ(distinct.size(), builtIns.size());
}

TEST(Manufacturer, AeroTechAliases)
{
    const Manufacturer& aeroTech = Manufacturer::getManufacturer("AeroTech");
    for (const std::string_view prefix : {"A", "AT", "AERO", "AEROT", "AEROTECH"})
    {
        const std::string base(prefix);
        for (const std::string& alias :
             {base, base + "-RMS", base + "-RCS", "RCS-" + base, base + "-APOGEE"})
        {
            EXPECT_TRUE(aeroTech.getAllNames().contains(alias)) << alias;
            EXPECT_EQ(&Manufacturer::getManufacturer(alias), &aeroTech) << alias;
        }
    }
    EXPECT_TRUE(aeroTech.getAllNames().contains("ISP"));
}

TEST(Manufacturer, SearchString)
{
    EXPECT_EQ(Manufacturer::searchString("Public Missiles, Ltd."), "public missiles ltd");
    EXPECT_EQ(Manufacturer::searchString("  --AT/RCS--  "), "at rcs");
    EXPECT_EQ(Manufacturer::searchString("KOSDON/AEROTECH"), "kosdon aerotech");
    EXPECT_EQ(Manufacturer::searchString(""), "");
    EXPECT_EQ(Manufacturer::searchString("./-"), "");
    // Non-ASCII letters are separators, as Java's [^a-zA-Z0-9]+ makes them.
    EXPECT_EQ(Manufacturer::searchString("M\u00FCller Raketen"), "m ller raketen");
    EXPECT_EQ(Manufacturer::searchString("\u212A"), "k");
    EXPECT_EQ(Manufacturer::searchString("\u0130X"), "i x");
    EXPECT_EQ(Manufacturer::searchString("12 Motors"), "12 motors");
}

TEST(Manufacturer, SearchNamesAreTheSearchStringsOfAllNames)
{
    const Manufacturer& estes = Manufacturer::getManufacturer("Estes");
    EXPECT_EQ(estes.getAllNames(), (std::set<std::string>{"E", "ES", "Estes"}));
    EXPECT_EQ(estes.getSearchNames(), (std::set<std::string>{"e", "es", "estes"}));
    EXPECT_FALSE(estes.matches(""));
    EXPECT_FALSE(estes.matches("est"));
}

TEST(Manufacturer, NewManufacturersAreRegisteredOnce)
{
    const Manufacturer& first = Manufacturer::getManufacturer("  Zeta Motor Works!  ");
    EXPECT_EQ(first.getDisplayName(), "Zeta Motor Works!");
    EXPECT_EQ(first.getSimpleName(), "Zeta Motor Works!");
    EXPECT_EQ(first.getMotorType(), Motor::Type::UNKNOWN);
    EXPECT_EQ(first.getAllNames(), (std::set<std::string>{"Zeta Motor Works!"}));
    EXPECT_EQ(first.getSearchNames(), (std::set<std::string>{"zeta motor works"}));
    EXPECT_EQ(&Manufacturer::getManufacturer("ZETA-MOTOR-WORKS"), &first);
    EXPECT_TRUE(first.matches("zeta motor works"));
    EXPECT_FALSE(first.matches("zetamotorworks"));
}

TEST(Manufacturer, EmptyNameIsAManufacturerToo)
{
    const Manufacturer& empty = Manufacturer::getManufacturer("");
    EXPECT_EQ(empty.getDisplayName(), "");
    // "--" has the same (empty) search string.
    EXPECT_EQ(&Manufacturer::getManufacturer("--"), &empty);
}

TEST(Manufacturer, ConcurrentLookupsAgree)
{
    constexpr int                    kThreads = 8;
    std::vector<const Manufacturer*> results(kThreads, nullptr);
    {
        std::vector<std::jthread> threads;
        threads.reserve(kThreads);
        for (int i = 0; i < kThreads; i++)
        {
            threads.emplace_back([&results, i] {
                results.at(static_cast<std::size_t>(i)) =
                    &Manufacturer::getManufacturer("Concurrent Rocketry");
            });
        }
    }
    for (const Manufacturer* result : results)
    {
        EXPECT_EQ(result, results.front());
    }
}

}  // namespace
