#include "QtRocket/motor/IgnitionEvent.h"

#include <optional>
#include <string_view>

namespace QtRocket
{

namespace
{

/// The names, keys and English messages of one event.
struct EventInfo
{
    std::string_view name;
    std::string_view displayKey;
    std::string_view displayName;
    std::string_view shortDisplayKey;
    std::string_view shortDisplayName;
    std::string_view orkName;
};

[[nodiscard]] const EventInfo& info(IgnitionEvent event) noexcept
{
    static constexpr EventInfo kAutomatic{
        .name             = "AUTOMATIC",
        .displayKey       = "MotorMount.IgnitionEvent.AUTOMATIC",
        .displayName      = "Automatic (launch or ejection charge)",
        .shortDisplayKey  = "MotorMount.IgnitionEvent.short.AUTOMATIC",
        .shortDisplayName = "Automatic",
        .orkName          = "automatic"};
    static constexpr EventInfo kLaunch{.name             = "LAUNCH",
                                       .displayKey       = "MotorMount.IgnitionEvent.LAUNCH",
                                       .displayName      = "Launch",
                                       .shortDisplayKey  = "MotorMount.IgnitionEvent.short.LAUNCH",
                                       .shortDisplayName = "Launch",
                                       .orkName          = "launch"};
    static constexpr EventInfo kEjectionCharge{
        .name             = "EJECTION_CHARGE",
        .displayKey       = "MotorMount.IgnitionEvent.EJECTION_CHARGE",
        .displayName      = "First ejection charge of previous stage",
        .shortDisplayKey  = "MotorMount.IgnitionEvent.short.EJECTION_CHARGE",
        .shortDisplayName = "Ejection charge",
        .orkName          = "ejectioncharge"};
    static constexpr EventInfo kBurnout{.name            = "BURNOUT",
                                        .displayKey      = "MotorMount.IgnitionEvent.BURNOUT",
                                        .displayName     = "First burnout of previous stage",
                                        .shortDisplayKey = "MotorMount.IgnitionEvent.short.BURNOUT",
                                        .shortDisplayName = "Burnout",
                                        .orkName          = "burnout"};
    static constexpr EventInfo kNever{.name             = "NEVER",
                                      .displayKey       = "MotorMount.IgnitionEvent.NEVER",
                                      .displayName      = "Never",
                                      .shortDisplayKey  = "MotorMount.IgnitionEvent.short.NEVER",
                                      .shortDisplayName = "Never",
                                      .orkName          = "never"};

    switch (event)
    {
        case IgnitionEvent::AUTOMATIC:
            return kAutomatic;
        case IgnitionEvent::LAUNCH:
            return kLaunch;
        case IgnitionEvent::EJECTION_CHARGE:
            return kEjectionCharge;
        case IgnitionEvent::BURNOUT:
            return kBurnout;
        case IgnitionEvent::NEVER:
            return kNever;
    }
    return kNever;
}

}  // namespace

std::string_view name(IgnitionEvent event) noexcept
{
    return info(event).name;
}

std::string_view displayKey(IgnitionEvent event) noexcept
{
    return info(event).displayKey;
}

std::string_view displayName(IgnitionEvent event) noexcept
{
    return info(event).displayName;
}

std::string_view shortDisplayKey(IgnitionEvent event) noexcept
{
    return info(event).shortDisplayKey;
}

std::string_view shortDisplayName(IgnitionEvent event) noexcept
{
    return info(event).shortDisplayName;
}

std::string_view orkName(IgnitionEvent event) noexcept
{
    return info(event).orkName;
}

bool matchesOrkName(IgnitionEvent event, std::string_view content) noexcept
{
    return orkName(event) == content;
}

std::optional<IgnitionEvent> ignitionEventFromOrkName(std::string_view content) noexcept
{
    for (const IgnitionEvent event : kAllIgnitionEvents)
    {
        if (matchesOrkName(event, content))
        {
            return event;
        }
    }
    return std::nullopt;
}

}  // namespace QtRocket
