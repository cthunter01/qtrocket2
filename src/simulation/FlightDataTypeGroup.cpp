#include "QtRocket/simulation/FlightDataTypeGroup.h"

#include <string_view>

namespace QtRocket
{

namespace
{

struct GroupInfo
{
    std::string_view displayKey;
    std::string_view displayName;
    int              priority;
};

/// FlightDataTypeGroup's constructor arguments per group, with the English message texts.
[[nodiscard]] const GroupInfo& info(FlightDataTypeGroup group) noexcept
{
    static constexpr GroupInfo kTime{
        .displayKey = "FlightDataTypeGroup.GROUP_TIME", .displayName = "Time", .priority = 0};
    static constexpr GroupInfo kPositionAndMotion{
        .displayKey  = "FlightDataTypeGroup.GROUP_POSITION_AND_MOTION",
        .displayName = "Position and Motion",
        .priority    = 10};
    static constexpr GroupInfo kOrientation{.displayKey  = "FlightDataTypeGroup.GROUP_ORIENTATION",
                                            .displayName = "Orientation",
                                            .priority    = 20};
    static constexpr GroupInfo kMassAndInertia{
        .displayKey  = "FlightDataTypeGroup.GROUP_MASS_AND_INERTIA",
        .displayName = "Mass and Inertia",
        .priority    = 30};
    static constexpr GroupInfo kStability{.displayKey  = "FlightDataTypeGroup.GROUP_STABILITY",
                                          .displayName = "Stability",
                                          .priority    = 40};
    static constexpr GroupInfo kThrustAndDrag{
        .displayKey  = "FlightDataTypeGroup.GROUP_THRUST_AND_DRAG",
        .displayName = "Thrust and Drag",
        .priority    = 50};
    static constexpr GroupInfo kCoefficients{.displayKey = "FlightDataTypeGroup.GROUP_COEFFICIENTS",
                                             .displayName = "Coefficients",
                                             .priority    = 60};
    static constexpr GroupInfo kAtmosphericConditions{
        .displayKey  = "FlightDataTypeGroup.GROUP_ATMOSPHERIC_CONDITIONS",
        .displayName = "Atmospheric Conditions",
        .priority    = 70};
    static constexpr GroupInfo kCharacteristicNumbers{
        .displayKey  = "FlightDataTypeGroup.GROUP_CHARACTERISTIC_NUMBERS",
        .displayName = "Characteristic Numbers",
        .priority    = 80};
    static constexpr GroupInfo kReferenceValues{
        .displayKey  = "FlightDataTypeGroup.GROUP_REFERENCE_VALUES",
        .displayName = "Reference Values",
        .priority    = 90};
    static constexpr GroupInfo kSimulationInformation{
        .displayKey  = "FlightDataTypeGroup.GROUP_SIMULATION_INFORMATION",
        .displayName = "Simulation Information",
        .priority    = 100};
    static constexpr GroupInfo kCustom{
        .displayKey = "FlightDataTypeGroup.GROUP_CUSTOM", .displayName = "Custom", .priority = 200};

    switch (group)
    {
        case FlightDataTypeGroup::TIME:
            return kTime;
        case FlightDataTypeGroup::POSITION_AND_MOTION:
            return kPositionAndMotion;
        case FlightDataTypeGroup::ORIENTATION:
            return kOrientation;
        case FlightDataTypeGroup::MASS_AND_INERTIA:
            return kMassAndInertia;
        case FlightDataTypeGroup::STABILITY:
            return kStability;
        case FlightDataTypeGroup::THRUST_AND_DRAG:
            return kThrustAndDrag;
        case FlightDataTypeGroup::COEFFICIENTS:
            return kCoefficients;
        case FlightDataTypeGroup::ATMOSPHERIC_CONDITIONS:
            return kAtmosphericConditions;
        case FlightDataTypeGroup::CHARACTERISTIC_NUMBERS:
            return kCharacteristicNumbers;
        case FlightDataTypeGroup::REFERENCE_VALUES:
            return kReferenceValues;
        case FlightDataTypeGroup::SIMULATION_INFORMATION:
            return kSimulationInformation;
        case FlightDataTypeGroup::CUSTOM:
            return kCustom;
    }
    return kCustom;
}

}  // namespace

std::string_view displayKey(FlightDataTypeGroup group) noexcept
{
    return info(group).displayKey;
}

std::string_view displayName(FlightDataTypeGroup group) noexcept
{
    return info(group).displayName;
}

int priority(FlightDataTypeGroup group) noexcept
{
    return info(group).priority;
}

int compareTo(FlightDataTypeGroup a, FlightDataTypeGroup b) noexcept
{
    return priority(a) - priority(b);
}

}  // namespace QtRocket
