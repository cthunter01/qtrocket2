#pragma once

#include <array>
#include <string_view>

namespace QtRocket
{

/// The groups flight data types are sorted into (OpenRocket's FlightDataTypeGroup instances), in
/// Java's declaration order, which is also their priority order. Java compares and equates groups
/// by priority alone; as the priorities are distinct, the enum's own comparison operators give the
/// same order and equality, and compareTo() gives Java's exact result.
enum class FlightDataTypeGroup
{
    TIME,                    ///< priority 0
    POSITION_AND_MOTION,     ///< priority 10
    ORIENTATION,             ///< priority 20
    MASS_AND_INERTIA,        ///< priority 30
    STABILITY,               ///< priority 40
    THRUST_AND_DRAG,         ///< priority 50
    COEFFICIENTS,            ///< priority 60
    ATMOSPHERIC_CONDITIONS,  ///< priority 70
    CHARACTERISTIC_NUMBERS,  ///< priority 80
    REFERENCE_VALUES,        ///< priority 90
    SIMULATION_INFORMATION,  ///< priority 100
    CUSTOM,                  ///< priority 200: custom expressions, extensions, unknown types
};

/// FlightDataTypeGroup.ALL_GROUPS, in declaration (priority) order.
inline constexpr std::array<FlightDataTypeGroup, 12> kAllFlightDataTypeGroups{
    FlightDataTypeGroup::TIME,
    FlightDataTypeGroup::POSITION_AND_MOTION,
    FlightDataTypeGroup::ORIENTATION,
    FlightDataTypeGroup::MASS_AND_INERTIA,
    FlightDataTypeGroup::STABILITY,
    FlightDataTypeGroup::THRUST_AND_DRAG,
    FlightDataTypeGroup::COEFFICIENTS,
    FlightDataTypeGroup::ATMOSPHERIC_CONDITIONS,
    FlightDataTypeGroup::CHARACTERISTIC_NUMBERS,
    FlightDataTypeGroup::REFERENCE_VALUES,
    FlightDataTypeGroup::SIMULATION_INFORMATION,
    FlightDataTypeGroup::CUSTOM,
};

/// The translation key of the group's name, e.g. "FlightDataTypeGroup.GROUP_POSITION_AND_MOTION".
[[nodiscard]] std::string_view displayKey(FlightDataTypeGroup group) noexcept;

/// The English name (FlightDataTypeGroup.getName() and toString() with OpenRocket's English
/// messages), e.g. "Position and Motion".
[[nodiscard]] std::string_view displayName(FlightDataTypeGroup group) noexcept;

/// The sort priority: lower sorts first.
[[nodiscard]] int priority(FlightDataTypeGroup group) noexcept;

/// FlightDataTypeGroup.compareTo: the difference of the priorities, negative when @p a sorts
/// before @p b.
[[nodiscard]] int compareTo(FlightDataTypeGroup a, FlightDataTypeGroup b) noexcept;

}  // namespace QtRocket
