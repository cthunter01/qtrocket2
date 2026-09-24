#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace QtRocket
{

/// When a motor ignites (OpenRocket's IgnitionEvent), in declaration order.
///
/// OpenRocket's isActivationEvent(FlightConfiguration, FlightEvent, RocketComponent) needs the
/// rocket model and the simulation's flight events, which live in higher layers; the simulation
/// ports it next to its event handling, with these rules: AUTOMATIC acts as LAUNCH for a motor in
/// the launch stage and as EJECTION_CHARGE otherwise; LAUNCH fires on a LAUNCH event;
/// EJECTION_CHARGE and BURNOUT fire on that event when it comes from the stage directly below
/// the motor's own stage (the event source's stage's upper stage is the motor's stage); NEVER
/// never fires.
enum class IgnitionEvent
{
    AUTOMATIC,        ///< automatic (launch or ejection charge)
    LAUNCH,           ///< at launch
    EJECTION_CHARGE,  ///< at the first ejection charge of the previous stage
    BURNOUT,          ///< at the first burnout of the previous stage
    NEVER,            ///< never
};

/// IgnitionEvent.values(), in declaration order.
inline constexpr std::array<IgnitionEvent, 5> kAllIgnitionEvents{
    IgnitionEvent::AUTOMATIC, IgnitionEvent::LAUNCH, IgnitionEvent::EJECTION_CHARGE,
    IgnitionEvent::BURNOUT, IgnitionEvent::NEVER};

/// The event's name (the public `name` field and getName()), the enum constant's name:
/// "AUTOMATIC", "LAUNCH", "EJECTION_CHARGE", "BURNOUT" or "NEVER".
[[nodiscard]] std::string_view name(IgnitionEvent event) noexcept;

/// The translation key of the display name, "MotorMount.IgnitionEvent." + name().
[[nodiscard]] std::string_view displayKey(IgnitionEvent event) noexcept;

/// The English display name (IgnitionEvent.toString() with OpenRocket's English messages), e.g.
/// "First ejection charge of previous stage".
[[nodiscard]] std::string_view displayName(IgnitionEvent event) noexcept;

/// The translation key of the short name the motor configuration table shows,
/// "MotorMount.IgnitionEvent.short." + name().
[[nodiscard]] std::string_view shortDisplayKey(IgnitionEvent event) noexcept;

/// The English short name, e.g. "Ejection charge".
[[nodiscard]] std::string_view shortDisplayName(IgnitionEvent event) noexcept;

/// The name the event has in .ork files (<ignitionevent>): name() in lower case without
/// underscores, e.g. "ejectioncharge".
[[nodiscard]] std::string_view orkName(IgnitionEvent event) noexcept;

/// IgnitionEvent.equals(String): true when @p content is exactly orkName(event).
[[nodiscard]] bool matchesOrkName(IgnitionEvent event, std::string_view content) noexcept;

/// The first event (in declaration order) whose orkName() is @p content, as the .ork loader looks
/// it up; nullopt when there is none (OpenRocket then warns and ignores the element).
[[nodiscard]] std::optional<IgnitionEvent> ignitionEventFromOrkName(
    std::string_view content) noexcept;

}  // namespace QtRocket
