#include "QtRocket/unit/UnitGroup.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <functional>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/unit/CaliberUnit.h"
#include "QtRocket/unit/DegreeUnit.h"
#include "QtRocket/unit/FixedPrecisionUnit.h"
#include "QtRocket/unit/FractionalUnit.h"
#include "QtRocket/unit/FrequencyUnit.h"
#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/InchUnit.h"
#include "QtRocket/unit/PercentageOfLengthUnit.h"
#include "QtRocket/unit/TemperatureUnit.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/Value.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Chars.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The UNITS map keys, indexed by UnitGroupId; the two groups the map lacks get their own names.
constexpr std::array<std::string_view, kUnitGroupCount> kUnitGroupNames{
    "NONE",
    "MOTOR_DIMENSIONS",
    "LENGTH",
    "ALL_LENGTHS",
    "DISTANCE",
    "SHAPE_PARAMETER",
    "AREA",
    "STABILITY",
    "SECONDARY_STABILITY",
    "STABILITY_CALIBERS",
    "VELOCITY",
    "WINDSPEED",
    "LATITUDE",
    "LONGITUDE",
    "ACCELERATION",
    "MASS",
    "INERTIA",
    "ANGLE",
    "DENSITY_BULK",
    "DENSITY_SURFACE",
    "DENSITY_LINE",
    "FORCE",
    "IMPULSE",
    "TIME_STEP",
    "SHORT_TIME",
    "FLIGHT_TIME",
    "ROLL",
    "TEMPERATURE",
    "PRESSURE",
    "SHEAR_MODULUS",
    "RELATIVE",
    "ROUGHNESS",
    "COEFFICIENT",
    "FREQUENCY",
    "ENERGY",
    "POWER",
    "MOMENT",
    "MOMENTUM",
    "ANGULAR_MOMENTUM",
    "VOLTAGE",
    "CURRENT",
    "SCALING",
    "STROKE_WIDTH",
};

/// UnitGroup.resetDefaultUnits: the default unit's index per group. SHAPE_PARAMETER, which that
/// method leaves alone, keeps its only unit.
constexpr std::array<int, kUnitGroupCount> kDefaultUnitIndex{
    0,  // NONE
    0,  // MOTOR_DIMENSIONS
    1,  // LENGTH
    2,  // ALL_LENGTHS
    0,  // DISTANCE
    0,  // SHAPE_PARAMETER
    1,  // AREA
    4,  // STABILITY
    5,  // SECONDARY_STABILITY
    0,  // STABILITY_CALIBERS
    0,  // VELOCITY
    0,  // WINDSPEED
    0,  // LATITUDE
    0,  // LONGITUDE
    0,  // ACCELERATION
    0,  // MASS
    1,  // INERTIA
    0,  // ANGLE
    0,  // DENSITY_BULK
    1,  // DENSITY_SURFACE
    1,  // DENSITY_LINE
    0,  // FORCE
    0,  // IMPULSE
    1,  // TIME_STEP
    0,  // SHORT_TIME
    0,  // LONG_TIME
    1,  // ROLL
    1,  // TEMPERATURE
    0,  // PRESSURE
    1,  // SHEAR_MODULUS
    1,  // RELATIVE
    0,  // ROUGHNESS
    0,  // COEFFICIENT
    1,  // FREQUENCY
    0,  // ENERGY
    1,  // POWER
    1,  // MOMENT
    0,  // MOMENTUM
    1,  // ANGULAR_MOMENTUM
    1,  // VOLTAGE
    1,  // CURRENT
    0,  // SCALING
    0,  // STROKE_WIDTH
};

struct SiUnitEntry
{
    std::string_view siUnit;
    UnitGroupId      id;
};

/// UnitGroup.SIUNITS.
constexpr std::array<SiUnitEntry, 20> kSiUnits{{
    {.siUnit = "m", .id = UnitGroupId::ALL_LENGTHS},
    {.siUnit = "m^2", .id = UnitGroupId::AREA},
    {.siUnit = "m/s", .id = UnitGroupId::VELOCITY},
    {.siUnit = "m/s^2", .id = UnitGroupId::ACCELERATION},
    {.siUnit = "kg", .id = UnitGroupId::MASS},
    {.siUnit = "kg m^2", .id = UnitGroupId::INERTIA},
    {.siUnit = "kg/m^3", .id = UnitGroupId::DENSITY_BULK},
    {.siUnit = "N", .id = UnitGroupId::FORCE},
    {.siUnit = "N m", .id = UnitGroupId::MOMENT},
    {.siUnit = "Ns", .id = UnitGroupId::IMPULSE},
    {.siUnit = "s", .id = UnitGroupId::LONG_TIME},
    {.siUnit = "Pa", .id = UnitGroupId::PRESSURE},
    {.siUnit = "V", .id = UnitGroupId::VOLTAGE},
    {.siUnit = "A", .id = UnitGroupId::CURRENT},
    {.siUnit = "J", .id = UnitGroupId::ENERGY},
    {.siUnit = "W", .id = UnitGroupId::POWER},
    {.siUnit = "kg m/s", .id = UnitGroupId::MOMENTUM},
    {.siUnit = "kg m^2/s", .id = UnitGroupId::ANGULAR_MOMENTUM},
    {.siUnit = "Hz", .id = UnitGroupId::FREQUENCY},
    {.siUnit = "K", .id = UnitGroupId::TEMPERATURE},
}};

/// The position of @p id in the group tables; an id cast from an integer out of the enum's range
/// is a BugError.
[[nodiscard]] std::size_t indexOf(UnitGroupId id)
{
    const auto index = static_cast<std::size_t>(id);
    QTROCKET_ASSERT(index < kUnitGroupCount);
    return index;
}

[[nodiscard]] std::unique_ptr<Unit> general(double multiplier, std::string unit)
{
    return std::make_unique<GeneralUnit>(multiplier, std::move(unit));
}

[[nodiscard]] std::unique_ptr<Unit> fixed(std::string unit, double precision)
{
    return std::make_unique<FixedPrecisionUnit>(std::move(unit), precision);
}

[[nodiscard]] std::unique_ptr<Unit> fixed(std::string unit, double precision, double multiplier)
{
    return std::make_unique<FixedPrecisionUnit>(std::move(unit), precision, multiplier);
}

[[nodiscard]] std::unique_ptr<Unit> fixed(std::string unit, double precision, double multiplier,
                                          bool displayTrailingZeros)
{
    return std::make_unique<FixedPrecisionUnit>(std::move(unit), precision, multiplier,
                                                displayTrailingZeros);
}

[[nodiscard]] std::unique_ptr<Unit> inchesIn64ths()
{
    return std::make_unique<FractionalUnit>(0.0254, "in/64", "in", 64, 1.0 / 16.0, 0.5 / 64.0);
}

[[nodiscard]] std::string dot(std::string_view a, std::string_view b)
{
    return std::format("{}{}{}", a, Chars::kDot, b);
}

[[nodiscard]] std::string squared(std::string_view a)
{
    return std::format("{}{}", a, Chars::kSquared);
}

[[nodiscard]] std::string cubed(std::string_view a)
{
    return std::format("{}{}", a, Chars::kCubed);
}

[[nodiscard]] std::string degree(std::string_view suffix)
{
    return std::format("{}{}", Chars::kDegree, suffix);
}

/// UnitGroup.addStabilityUnits: the placeholders for the caliber and percentage units have no
/// reference length; a StabilityUnitGroup replaces them.
void addStabilityUnits(UnitGroup& group)
{
    group.addUnit(general(0.001, "mm"));
    group.addUnit(general(0.01, "cm"));
    group.addUnit(general(1, "m"));
    group.addUnit(general(0.0254, "in"));
    group.addUnit(std::make_unique<CaliberUnit>());
    group.addUnit(std::make_unique<PercentageOfLengthUnit>());
}

/// The static initialiser of UnitGroup.java, one group at a time. Units may not use HTML tags;
/// the scaling value "X" is obtained by "one of this unit is X of SI units".
void addUnits(UnitGroup& group, UnitGroupId id)
{
    constexpr double kPi = std::numbers::pi;
    switch (id)
    {
        case UnitGroupId::NONE:
            group.addUnit(Unit::noUnit().clone());
            return;

        case UnitGroupId::ENERGY:
            group.addUnit(general(1, "J"));
            group.addUnit(general(1.0e-7, "erg"));
            group.addUnit(general(1.055, "BTU"));
            group.addUnit(general(4.184, "cal"));
            group.addUnit(general(1.3558179483314, dot("ft", "lbf")));
            return;

        case UnitGroupId::POWER:
            group.addUnit(general(1.0e-3, "mW"));
            group.addUnit(general(1, "W"));
            group.addUnit(general(1.0e3, "kW"));
            group.addUnit(general(1.0e-7, "ergs"));
            group.addUnit(general(745.699872, "hp"));
            return;

        case UnitGroupId::MOMENT:
            group.addUnit(general(0.01, dot("N", "cm")));
            group.addUnit(general(1, dot("N", "m")));
            group.addUnit(general(0.112984829, dot("lbf", "in")));
            group.addUnit(general(1.35581795, dot("lbf", "ft")));
            return;

        case UnitGroupId::MOMENTUM:
            group.addUnit(general(1, dot("kg", "m/s")));
            return;

        case UnitGroupId::ANGULAR_MOMENTUM:
            group.addUnit(general(0.0001, dot("kg", squared("cm") + "/s")));
            group.addUnit(general(1, dot("kg", squared("m") + "/s")));
            group.addUnit(general(1, dot("N", dot("m", "s"))));
            group.addUnit(general(1.82899783e-5, dot("oz", squared("in") + "/s")));
            group.addUnit(general(0.000292639653, dot("lb", squared("in") + "/s")));
            group.addUnit(general(0.0421401101, dot("lb", squared("ft") + "/s")));
            group.addUnit(general(1.35581795, dot("lbf", dot("ft", "s"))));
            return;

        case UnitGroupId::VOLTAGE:
            group.addUnit(general(1.0e-3, "mV"));
            group.addUnit(general(1, "V"));
            return;

        case UnitGroupId::CURRENT:
            group.addUnit(general(1.0e-3, "mA"));
            group.addUnit(general(1, "A"));
            return;

        case UnitGroupId::LENGTH:
            group.addUnit(general(0.001, "mm"));
            group.addUnit(general(0.01, "cm"));
            group.addUnit(general(1, "m"));
            group.addUnit(std::make_unique<InchUnit>(0.0254, "in", 1));
            group.addUnit(inchesIn64ths());
            group.addUnit(general(0.3048, "ft"));
            return;

        case UnitGroupId::MOTOR_DIMENSIONS:
            group.addUnit(general(0.001, "mm"));
            group.addUnit(general(0.01, "cm"));
            group.addUnit(general(1, "m"));
            group.addUnit(general(0.0254, "in"));
            return;

        case UnitGroupId::DISTANCE:
            group.addUnit(general(1, "m"));
            group.addUnit(general(1000, "km"));
            group.addUnit(general(0.3048, "ft"));
            group.addUnit(general(0.9144, "yd"));
            group.addUnit(general(1609.344, "mi"));
            group.addUnit(general(1852, "nmi"));
            return;

        case UnitGroupId::ALL_LENGTHS:
            group.addUnit(general(0.001, "mm"));
            group.addUnit(general(0.01, "cm"));
            group.addUnit(general(1, "m"));
            group.addUnit(general(1000, "km"));
            group.addUnit(general(0.0254, "in"));
            group.addUnit(inchesIn64ths());
            group.addUnit(general(0.3048, "ft"));
            group.addUnit(general(0.9144, "yd"));
            group.addUnit(general(1609.344, "mi"));
            group.addUnit(general(1852, "nmi"));
            return;

        case UnitGroupId::AREA:
            group.addUnit(general(MathUtil::pow2(0.001), squared("mm")));
            group.addUnit(general(MathUtil::pow2(0.01), squared("cm")));
            group.addUnit(general(1, squared("m")));
            group.addUnit(general(MathUtil::pow2(0.0254), squared("in")));
            group.addUnit(general(MathUtil::pow2(0.3048), squared("ft")));
            return;

        case UnitGroupId::SHAPE_PARAMETER:
            group.addUnit(std::make_unique<GeneralUnit>(1, std::string(Chars::kZwsp), 1, 10, 0.1));
            return;

        case UnitGroupId::STABILITY:
        case UnitGroupId::SECONDARY_STABILITY:
            addStabilityUnits(group);
            return;

        case UnitGroupId::STABILITY_CALIBERS:
            group.addUnit(general(1, "cal"));
            return;

        case UnitGroupId::VELOCITY:
        case UnitGroupId::WINDSPEED:
            group.addUnit(general(1, "m/s"));
            group.addUnit(general(1 / 3.6, "km/h"));
            group.addUnit(general(0.3048, "ft/s"));
            group.addUnit(general(0.44704, "mph"));
            group.addUnit(general(0.51444445, "kt"));
            return;

        case UnitGroupId::LATITUDE:
            // CompassRose.lbl.north / south are "N" / "S" in OpenRocket's English messages.
            group.addUnit(fixed(degree(" N"), 10E-6, 1, false));
            group.addUnit(fixed(degree(" S"), 10E-6, -1, false));
            return;

        case UnitGroupId::LONGITUDE:
            group.addUnit(fixed(degree(" E"), 10E-6, 1, false));
            group.addUnit(fixed(degree(" W"), 10E-6, -1, false));
            return;

        case UnitGroupId::ACCELERATION:
            group.addUnit(general(1, squared("m/s")));
            group.addUnit(general(0.3048, squared("ft/s")));
            group.addUnit(general(9.80665, "G"));
            return;

        case UnitGroupId::MASS:
            group.addUnit(general(0.001, "g"));
            group.addUnit(general(1, "kg"));
            group.addUnit(general(0.0283495231, "oz"));
            group.addUnit(general(0.45359237, "lb"));
            return;

        case UnitGroupId::INERTIA:
            group.addUnit(general(0.0001, dot("kg", squared("cm"))));
            group.addUnit(general(1, dot("kg", squared("m"))));
            group.addUnit(general(1.82899783e-5, dot("oz", squared("in"))));
            group.addUnit(general(0.000292639653, dot("lb", squared("in"))));
            group.addUnit(general(0.0421401101, dot("lb", squared("ft"))));
            group.addUnit(general(1.35581795, dot("lbf", dot("ft", squared("s")))));
            return;

        case UnitGroupId::ANGLE:
            group.addUnit(std::make_unique<DegreeUnit>());
            group.addUnit(fixed("rad", 0.01));
            group.addUnit(general(1.0 / 3437.74677078, "arcmin"));
            return;

        case UnitGroupId::DENSITY_BULK:
            group.addUnit(general(1000, cubed("g/cm")));
            group.addUnit(general(1000999, cubed("kg/cm")));
            group.addUnit(general(1000, cubed("kg/dm")));
            group.addUnit(general(1, cubed("kg/m")));
            group.addUnit(general(1729.99404, cubed("oz/in")));
            group.addUnit(general(16.0184634, cubed("lb/ft")));
            return;

        case UnitGroupId::DENSITY_SURFACE:
            group.addUnit(general(10, squared("g/cm")));
            group.addUnit(general(0.001, squared("g/m")));
            group.addUnit(general(10000, squared("kg/cm")));
            group.addUnit(general(100, squared("kg/dm")));
            group.addUnit(general(1, squared("kg/m")));
            group.addUnit(general(43.9418487, squared("oz/in")));
            group.addUnit(general(0.305151727, squared("oz/ft")));
            group.addUnit(general(4.88242764, squared("lb/ft")));
            return;

        case UnitGroupId::DENSITY_LINE:
            group.addUnit(general(0.1, "g/cm"));
            group.addUnit(general(0.001, "g/m"));
            group.addUnit(general(100, "kg/cm"));
            group.addUnit(general(10, "kg/dm"));
            group.addUnit(general(1, "kg/m"));
            group.addUnit(general(0.0930102465, "oz/ft"));
            return;

        case UnitGroupId::FORCE:
            group.addUnit(general(1, "N"));
            group.addUnit(general(4.44822162, "lbf"));
            group.addUnit(general(9.80665, "kgf"));
            return;

        case UnitGroupId::IMPULSE:
            group.addUnit(general(1, "Ns"));
            group.addUnit(general(4.44822162, dot("lbf", "s")));
            return;

        case UnitGroupId::TIME_STEP:
            group.addUnit(fixed("ms", 1, 0.001));
            group.addUnit(fixed("s", 0.01));
            return;

        case UnitGroupId::SHORT_TIME:
            group.addUnit(general(1, "s"));
            return;

        case UnitGroupId::LONG_TIME:
            group.addUnit(general(1, "s"));
            group.addUnit(general(60, "min"));
            return;

        case UnitGroupId::ROLL:
            group.addUnit(general(1, "rad/s"));
            group.addUnit(general(kPi / 180, degree("/s")));
            group.addUnit(general(2 * kPi, "r/s"));
            group.addUnit(general(2 * kPi, "Hz"));
            group.addUnit(general(2 * kPi / 60, "rpm"));
            return;

        case UnitGroupId::TEMPERATURE:
            group.addUnit(fixed("K", 0.01));
            group.addUnit(std::make_unique<TemperatureUnit>(1, 273.15, 0.01, degree("C")));
            group.addUnit(std::make_unique<TemperatureUnit>(5.0 / 9.0, 459.67, 0.01, degree("F")));
            return;

        case UnitGroupId::PRESSURE:
            group.addUnit(fixed("mbar", 0.01, 1.0e2));
            group.addUnit(fixed("bar", 0.001, 1.0e5));
            group.addUnit(fixed("atm", 0.001, 1.01325e5));
            group.addUnit(fixed("mmHg", 0.01, 101325.0 / 760.0));
            group.addUnit(fixed("inHg", 0.01, 3386.389));
            group.addUnit(fixed("psi", 0.01, 6894.75729));
            group.addUnit(general(1, "Pa"));
            return;

        case UnitGroupId::SHEAR_MODULUS:
            group.addUnit(general(1, "Pa"));
            group.addUnit(general(1.0e9, "GPa"));
            group.addUnit(general(6.89475729e6, "ksi"));
            return;

        case UnitGroupId::RELATIVE:
            group.addUnit(fixed(std::string(Chars::kZwsp), 0.01, 1.0));
            group.addUnit(general(0.01, "%"));
            group.addUnit(fixed(std::string(Chars::kPermille), 1, 0.001));
            return;

        case UnitGroupId::ROUGHNESS:
            group.addUnit(general(0.000001, std::format("{}m", Chars::kMicro)));
            group.addUnit(general(0.0000254, "mil"));
            group.addUnit(general(0.0254, "in"));
            group.addUnit(general(1, "m"));
            return;

        case UnitGroupId::COEFFICIENT:
            group.addUnit(fixed(std::string(Chars::kZwsp), 0.001));  // zero-width space
            return;

        case UnitGroupId::SCALING:
            group.addUnit(fixed(std::string(Chars::kZwsp), 0.1));  // zero-width space
            return;

        case UnitGroupId::STROKE_WIDTH:
            group.addUnit(general(1, "mm"));
            group.addUnit(general(0.1, std::format("{}m", Chars::kMicro)));
            // OpenRocket keeps `new GeneralUnit(25.4, "in")` commented out here.
            group.addUnit(general(0.0254, "mil"));
            return;

        case UnitGroupId::FREQUENCY:
            // This is not used by OpenRocket, and not extensively tested.
            group.addUnit(std::make_unique<FrequencyUnit>(0.001, "mHz"));
            group.addUnit(std::make_unique<FrequencyUnit>(1, "Hz"));
            group.addUnit(std::make_unique<FrequencyUnit>(1000, "kHz"));
            return;
    }
    QTROCKET_UNREACHABLE();
}

/// The process-wide groups, built once on first use without going through unitGroup(), which
/// would re-enter the initialisation.
struct Registry
{
    std::array<std::unique_ptr<UnitGroup>, kUnitGroupCount> groups;

    Registry()
    {
        for (const UnitGroupId id : kAllUnitGroupIds)
        {
            const std::size_t i = indexOf(id);
            groups.at(i)        = std::make_unique<UnitGroup>();
            addUnits(*groups.at(i), id);
            groups.at(i)->setDefaultUnit(kDefaultUnitIndex.at(i));
        }
    }
};

[[nodiscard]] Registry& registry()
{
    static Registry s_registry;
    return s_registry;
}

void setDefault(UnitGroupId id, std::string_view name)
{
    const bool found = unitGroup(id).setDefaultUnit(name);
    QTROCKET_ASSERT(found);
}

/// Java's "\\W" (ASCII word characters only): keeps letters, digits and underscores.
[[nodiscard]] std::string wordCharacters(std::string_view text)
{
    std::string out;
    for (const char c : text)
    {
        const bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        const bool digit  = c >= '0' && c <= '9';
        if (letter || digit || c == '_')
        {
            out += c;
        }
    }
    return out;
}

/// Java's regex \s: [ \t\n\x0B\f\r].
[[nodiscard]] bool isRegexSpace(char c) noexcept
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\x0B' || c == '\f' || c == '\r';
}

/// Whether @p text holds a character Java's "." does not match: \n, \r, U+0085 (NEL), U+2028
/// (line separator) or U+2029 (paragraph separator), the last three as UTF-8.
[[nodiscard]] bool holdsLineTerminator(std::string_view text) noexcept
{
    constexpr std::array<std::string_view, 5> kTerminators{"\n", "\r", "\xC2\x85", "\xE2\x80\xA8",
                                                           "\xE2\x80\xA9"};
    return std::ranges::any_of(
        kTerminators, [text](std::string_view terminator) { return text.contains(terminator); });
}

/// The character class [0-9.,-] of UnitGroup.STRING_PATTERN.
[[nodiscard]] bool isNumberCharacter(char c) noexcept
{
    return (c >= '0' && c <= '9') || c == '.' || c == ',' || c == '-';
}

}  // namespace

// ---- UnitGroup ----

void UnitGroup::setDefaultMetricUnits()
{
    setDefault(UnitGroupId::LENGTH, "cm");
    setDefault(UnitGroupId::MOTOR_DIMENSIONS, "mm");
    setDefault(UnitGroupId::DISTANCE, "m");
    setDefault(UnitGroupId::AREA, squared("cm"));
    setDefault(UnitGroupId::STABILITY, "cal");
    setDefault(UnitGroupId::SECONDARY_STABILITY, "%");
    setDefault(UnitGroupId::VELOCITY, "m/s");
    setDefault(UnitGroupId::ACCELERATION, squared("m/s"));
    setDefault(UnitGroupId::MASS, "g");
    setDefault(UnitGroupId::INERTIA, dot("kg", squared("m")));
    setDefault(UnitGroupId::ANGULAR_MOMENTUM, dot("kg", squared("m") + "/s"));
    setDefault(UnitGroupId::MOMENT, dot("N", "m"));
    setDefault(UnitGroupId::ANGLE, Chars::kDegree);
    setDefault(UnitGroupId::DENSITY_BULK, cubed("g/cm"));
    setDefault(UnitGroupId::DENSITY_SURFACE, squared("g/m"));
    setDefault(UnitGroupId::DENSITY_LINE, "g/m");
    setDefault(UnitGroupId::FORCE, "N");
    setDefault(UnitGroupId::IMPULSE, "Ns");
    setDefault(UnitGroupId::TIME_STEP, "s");
    setDefault(UnitGroupId::LONG_TIME, "s");
    setDefault(UnitGroupId::ROLL, "r/s");
    setDefault(UnitGroupId::TEMPERATURE, degree("C"));
    setDefault(UnitGroupId::WINDSPEED, "m/s");
    setDefault(UnitGroupId::LATITUDE, degree(" N"));
    setDefault(UnitGroupId::LONGITUDE, degree(" E"));
    setDefault(UnitGroupId::PRESSURE, "mbar");
    setDefault(UnitGroupId::SHEAR_MODULUS, "GPa");
    setDefault(UnitGroupId::RELATIVE, "%");
    setDefault(UnitGroupId::ROUGHNESS, std::format("{}m", Chars::kMicro));
    setDefault(UnitGroupId::STROKE_WIDTH, "mm");
}

void UnitGroup::setDefaultImperialUnits()
{
    setDefault(UnitGroupId::LENGTH, "in");
    setDefault(UnitGroupId::MOTOR_DIMENSIONS, "in");
    setDefault(UnitGroupId::DISTANCE, "ft");
    setDefault(UnitGroupId::AREA, squared("in"));
    setDefault(UnitGroupId::STABILITY, "cal");
    setDefault(UnitGroupId::SECONDARY_STABILITY, "%");
    setDefault(UnitGroupId::VELOCITY, "ft/s");
    setDefault(UnitGroupId::ACCELERATION, squared("ft/s"));
    setDefault(UnitGroupId::MASS, "oz");
    setDefault(UnitGroupId::INERTIA, dot("lb", squared("ft")));
    setDefault(UnitGroupId::ANGULAR_MOMENTUM, dot("lb", squared("ft") + "/s"));
    setDefault(UnitGroupId::MOMENT, dot("lbf", "in"));
    setDefault(UnitGroupId::ANGLE, Chars::kDegree);
    setDefault(UnitGroupId::DENSITY_BULK, cubed("oz/in"));
    setDefault(UnitGroupId::DENSITY_SURFACE, squared("oz/ft"));
    setDefault(UnitGroupId::DENSITY_LINE, "oz/ft");
    setDefault(UnitGroupId::FORCE, "N");
    setDefault(UnitGroupId::IMPULSE, "Ns");
    setDefault(UnitGroupId::TIME_STEP, "s");
    setDefault(UnitGroupId::LONG_TIME, "s");
    setDefault(UnitGroupId::ROLL, "r/s");
    setDefault(UnitGroupId::TEMPERATURE, degree("F"));
    setDefault(UnitGroupId::WINDSPEED, "mph");
    setDefault(UnitGroupId::LATITUDE, degree(" N"));
    setDefault(UnitGroupId::LONGITUDE, degree(" E"));
    setDefault(UnitGroupId::PRESSURE, "mbar");
    setDefault(UnitGroupId::SHEAR_MODULUS, "ksi");
    setDefault(UnitGroupId::RELATIVE, "%");
    setDefault(UnitGroupId::ROUGHNESS, "mil");
    setDefault(UnitGroupId::STROKE_WIDTH, "mil");
}

void UnitGroup::resetDefaultUnits()
{
    for (const UnitGroupId id : kAllUnitGroupIds)
    {
        unitGroup(id).setDefaultUnit(kDefaultUnitIndex.at(indexOf(id)));
    }
}

std::unique_ptr<UnitGroup::StabilityUnitGroup> UnitGroup::stabilityUnits(double reference)
{
    return std::make_unique<StabilityUnitGroup>(unitGroup(UnitGroupId::STABILITY), reference);
}

std::unique_ptr<UnitGroup::StabilityUnitGroup> UnitGroup::secondaryStabilityUnits(double reference)
{
    return std::make_unique<StabilityUnitGroup>(unitGroup(UnitGroupId::SECONDARY_STABILITY),
                                                reference);
}

std::unique_ptr<UnitGroup::StabilityUnitGroup> UnitGroup::stabilityUnits(
    const std::function<double()>& referenceLengthProvider)
{
    return std::make_unique<StabilityUnitGroup>(unitGroup(UnitGroupId::STABILITY),
                                                referenceLengthProvider);
}

std::unique_ptr<UnitGroup::StabilityUnitGroup> UnitGroup::secondaryStabilityUnits(
    const std::function<double()>& referenceLengthProvider)
{
    return std::make_unique<StabilityUnitGroup>(unitGroup(UnitGroupId::SECONDARY_STABILITY),
                                                referenceLengthProvider);
}

void UnitGroup::addUnit(std::unique_ptr<Unit> unit)
{
    QTROCKET_ASSERT(unit != nullptr);
    m_units.push_back(std::move(unit));
}

int UnitGroup::getUnitCount() const
{
    return static_cast<int>(m_units.size());
}

const Unit& UnitGroup::getDefaultUnit() const
{
    // OpenRocket: List.get's IndexOutOfBoundsException for a group without units
    QTROCKET_ASSERT(std::cmp_less(m_defaultUnit, m_units.size()));
    return *m_units[static_cast<std::size_t>(m_defaultUnit)];
}

void UnitGroup::setDefaultUnit(int n)
{
    if (n < 0 || std::cmp_greater_equal(n, m_units.size()))
    {
        bug(std::format("index out of range: {}", n));
    }
    m_defaultUnit = n;
}

bool UnitGroup::setDefaultUnit(std::string_view name)
{
    for (std::size_t i = 0; i < m_units.size(); i++)
    {
        if (m_units[i]->getUnit() == name)
        {
            setDefaultUnit(static_cast<int>(i));
            return true;
        }
    }
    return false;
}

const Unit& UnitGroup::getSIUnit() const
{
    for (const auto& u : m_units)
    {
        if (u->getMultiplier() == 1)
        {
            return *u;
        }
    }
    return unitGroup(UnitGroupId::NONE).getDefaultUnit();
}

const Unit* UnitGroup::findApproximate(std::string_view str) const
{
    const std::string wanted = wordCharacters(str);
    for (const auto& u : m_units)
    {
        if (Strings::equalsIgnoreAsciiCase(wanted, wordCharacters(u->getUnit())))
        {
            return u.get();
        }
    }
    return nullptr;
}

const Unit* UnitGroup::getUnit(std::string_view name) const
{
    for (const auto& unit : m_units)
    {
        if (unit->getUnit() == name)
        {
            return unit.get();
        }
    }
    return nullptr;
}

const Unit& UnitGroup::getUnit(int n) const
{
    // OpenRocket: List.get's IndexOutOfBoundsException
    if (n < 0 || std::cmp_greater_equal(n, m_units.size()))
    {
        bug(std::format("unit index out of range: {}", n));
    }
    return *m_units[static_cast<std::size_t>(n)];
}

int UnitGroup::getUnitIndex(const Unit& u) const
{
    for (std::size_t i = 0; i < m_units.size(); i++)
    {
        if (m_units[i]->equals(u))
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool UnitGroup::contains(const Unit& u) const
{
    return getUnitIndex(u) >= 0;
}

std::vector<const Unit*> UnitGroup::getUnits() const
{
    std::vector<const Unit*> units;
    units.reserve(m_units.size());
    for (const auto& u : m_units)
    {
        units.push_back(u.get());
    }
    return units;
}

double UnitGroup::fromUnit(double value) const
{
    return getDefaultUnit().fromUnit(value);
}

std::string UnitGroup::toString(double value) const
{
    return getDefaultUnit().toString(value);
}

std::string UnitGroup::toStringUnit(double value) const
{
    return getDefaultUnit().toStringUnit(value);
}

Value UnitGroup::toValue(double value) const
{
    return getDefaultUnit().toValue(value);
}

std::string UnitGroup::toString() const
{
    return "UnitGroup:" + getSIUnit().getUnit();
}

Result<double> UnitGroup::fromString(std::string_view str) const
{
    // STRING_PATTERN "^\s*([0-9.,-]+)(.*?)$", matched against the whole string: leading
    // whitespace, a run of number characters, then the rest, which may not hold a line
    // terminator (Java's "." matches none of \n, \r, U+0085, U+2028 and U+2029).
    std::size_t i = 0;
    while (i < str.size() && isRegexSpace(str[i]))
    {
        i++;
    }
    const std::size_t numberStart = i;
    while (i < str.size() && isNumberCharacter(str[i]))
    {
        i++;
    }
    const std::string_view number = str.substr(numberStart, i - numberStart);
    const std::string_view rest   = str.substr(i);
    if (number.empty() || holdsLineTerminator(rest))
    {
        return fail(ErrorCode::PARSE, "string did not match required pattern");
    }

    const std::optional<double> parsed = Strings::convertToDouble(number);
    if (!parsed)
    {
        return fail(ErrorCode::PARSE, std::format("not a number: \"{}\"", number));
    }
    const std::string_view unit = Strings::trim(rest);

    if (unit.empty())
    {
        return getDefaultUnit().fromUnit(*parsed);
    }
    for (const auto& u : m_units)
    {
        if (Strings::equalsIgnoreAsciiCase(unit, u->getUnit()))
        {
            return u->fromUnit(*parsed);
        }
    }
    return fail(ErrorCode::PARSE, std::format("unknown unit {}", unit));
}

bool UnitGroup::equals(const UnitGroup& other) const
{
    if (m_units.size() != other.m_units.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < m_units.size(); i++)
    {
        if (!m_units[i]->equals(*other.m_units[i]))
        {
            return false;
        }
    }
    return true;
}

std::size_t UnitGroup::hash() const
{
    std::size_t code = 0;
    for (const auto& u : m_units)
    {
        code = code + u->hash();
    }
    return code;
}

// ---- StabilityUnitGroup ----

UnitGroup::StabilityUnitGroup::StabilityUnitGroup(UnitGroup& stabilityUnit, double reference)
  : StabilityUnitGroup(stabilityUnit, std::make_unique<CaliberUnit>(reference),
                       std::make_unique<PercentageOfLengthUnit>(reference))
{
}

UnitGroup::StabilityUnitGroup::StabilityUnitGroup(
    UnitGroup& stabilityUnit, const std::function<double()>& referenceLengthProvider)
  : StabilityUnitGroup(stabilityUnit, std::make_unique<CaliberUnit>(referenceLengthProvider),
                       std::make_unique<PercentageOfLengthUnit>(referenceLengthProvider))
{
}

UnitGroup::StabilityUnitGroup::StabilityUnitGroup(
    UnitGroup& stabilityUnit, std::unique_ptr<CaliberUnit> caliberUnit,
    std::unique_ptr<PercentageOfLengthUnit> percentageOfLengthUnit)
  : m_stabilityUnit(&stabilityUnit)
{
    for (const auto& unit : stabilityUnit.m_units)
    {
        if (caliberUnit != nullptr && dynamic_cast<const CaliberUnit*>(unit.get()) != nullptr)
        {
            m_units.push_back(std::move(caliberUnit));
        }
        else if (percentageOfLengthUnit != nullptr &&
                 dynamic_cast<const PercentageOfLengthUnit*>(unit.get()) != nullptr)
        {
            m_percentageOfLengthUnit = percentageOfLengthUnit.get();
            m_units.push_back(std::move(percentageOfLengthUnit));
        }
        else
        {
            m_units.push_back(unit->clone());
        }
    }
    if (m_percentageOfLengthUnit == nullptr)
    {
        m_detachedPercentageOfLengthUnit = std::move(percentageOfLengthUnit);
        m_percentageOfLengthUnit         = m_detachedPercentageOfLengthUnit.get();
    }
    m_defaultUnit = stabilityUnit.m_defaultUnit;
}

void UnitGroup::StabilityUnitGroup::setDefaultUnit(int n)
{
    UnitGroup::setDefaultUnit(n);
    m_stabilityUnit->setDefaultUnit(n);
}

std::string UnitGroup::StabilityUnitGroup::toString() const
{
    return "StabilityUnitGroup:" + getSIUnit().getUnit();
}

// ---- FixedUnitGroup ----

FixedUnitGroup::FixedUnitGroup(std::string unitString) : m_unitString(std::move(unitString))
{
    addUnit(general(1, m_unitString));
}

bool FixedUnitGroup::contains(const Unit& /*u*/) const
{
    return true;
}

std::string FixedUnitGroup::toString() const
{
    return "FixedUnitGroup:" + getSIUnit().getUnit();
}

// ---- registry ----

UnitGroup& unitGroup(UnitGroupId id)
{
    return *registry().groups.at(indexOf(id));
}

std::string_view unitGroupName(UnitGroupId id)
{
    return kUnitGroupNames.at(indexOf(id));
}

std::optional<UnitGroupId> unitGroupFromName(std::string_view name) noexcept
{
    for (const UnitGroupId id : kAllUnitGroupIds)
    {
        if (unitGroupName(id) == name)
        {
            return id;
        }
    }
    return std::nullopt;
}

std::optional<UnitGroupId> unitGroupFromSiUnit(std::string_view siUnit) noexcept
{
    for (const SiUnitEntry& entry : kSiUnits)
    {
        if (entry.siUnit == siUnit)
        {
            return entry.id;
        }
    }
    return std::nullopt;
}

}  // namespace QtRocket
