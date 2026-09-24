#include "QtRocket/logging/Warning.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/MessagePriority.h"

namespace QtRocket
{

namespace
{

// OpenRocket's English texts (core/src/main/resources/l10n/messages.properties).
constexpr std::string_view kLargeAoaText          = "Large angle of attack encountered";
constexpr std::string_view kRecoveryHighSpeedText = "Recovery device deployment at high speed";
constexpr std::string_view kMainHighSpeedText     = "Main parachute deployment at high speed";
constexpr std::string_view kMainLowSpeedText      = "Main parachute deployment at low speed";
constexpr std::string_view kDrogueLowSpeedText    = "Drogue deployment at low speed at apogee";
constexpr std::string_view kDrogueNoMainText = "Drogue configured but no main parachute present";
constexpr std::string_view kEventAfterLandingText = "Flight Event occurred after landing: ";

/// Java's "0.00E0" DecimalFormat: two decimals, the exponent without sign or padding ("1.23E6").
std::string formatExponential(double value)
{
    const std::string text     = std::format("{:.2e}", value);  // "1.23e+06", "-1.23e-07"
    const std::size_t at       = text.find('e');
    std::string_view  exponent = std::string_view{text}.substr(at + 1);
    const bool        negative = exponent.starts_with('-');
    exponent.remove_prefix(1);  // the sign
    while (exponent.size() > 1 && exponent.starts_with('0'))
    {
        exponent.remove_prefix(1);
    }
    return std::format("{}E{}{}", std::string_view{text}.substr(0, at), negative ? "-" : "",
                       exponent);
}

/// Java's Unit.roundForDecimalFormat(): three significant digits and at most three decimals.
double roundForDecimalFormat(double value)
{
    const double sign = value < 0.0 ? -1.0 : 1.0;  // Math.signum; the caller excludes 0
    double       val  = std::abs(value);
    double       mul  = 1.0;
    while (val < 100.0 && mul < 1000.0)
    {
        mul *= 10.0;
        val *= 10.0;
    }
    return (std::rint(val) / mul) * sign;
}

/// Java's "0.0##" DecimalFormat: one to three decimals.
std::string formatDecimal(double value)
{
    std::string text = std::format("{:.3f}", value);
    while (text.ends_with('0') && !text.ends_with(".0"))
    {
        text.pop_back();
    }
    return text;
}

/// Java's DecimalFormat output for an infinity: the infinity symbol with the sign in front.
std::string formatInfinity(double value)
{
    return value < 0.0 ? "-∞" : "∞";
}

/// Java's Unit.toString() for a unit with multiplier 1: exponential above a million, whole
/// numbers from 100 up, "0" at or below 0.0005, three significant digits in between.
std::string formatGeneral(double value)
{
    if (std::isinf(value))
    {
        return formatInfinity(value);
    }
    if (std::abs(value) > 1.0e6)
    {
        return formatExponential(value);
    }
    if (std::abs(value) >= 100.0)
    {
        return std::format("{:.0f}", value);
    }
    if (std::abs(value) <= 0.0005)
    {
        return "0";
    }
    const double rounded = roundForDecimalFormat(value);
    if (std::abs(rounded - std::floor(rounded)) < 0.0001)
    {
        return std::format("{:.0f}", rounded);
    }
    return formatDecimal(rounded);
}

/// UNITS_VELOCITY.toStringUnit() with the default unit m/s: "38.3 m/s"; "N/A" for NaN.
/// TODO(units): format through UnitGroup
std::string formatVelocity(double value)
{
    if (std::isnan(value))
    {
        return "N/A";
    }
    return formatGeneral(value) + " m/s";
}

/// UNITS_ANGLE.toStringUnit() with the default unit degrees: DegreeUnit formats with "0.#" and
/// no space before the sign, so 0.3 rad gives "17.2°".
/// TODO(units): format through UnitGroup
std::string formatDegrees(double radians)
{
    const double degrees = radians / (std::numbers::pi / 180.0);
    if (std::isinf(degrees))
    {
        return formatInfinity(degrees) + "°";
    }
    std::string text = std::format("{:.1f}", degrees);
    if (text.ends_with(".0"))
    {
        text.resize(text.size() - 2);
    }
    text += "°";
    return text;
}

/// The text of the four deployment-speed warnings: the speed in brackets unless it is NaN.
std::string speedDescription(std::string_view text, double speed)
{
    if (std::isnan(speed))
    {
        return std::string{text};
    }
    return std::format("{} ({})", text, formatVelocity(speed));
}

/// Java's Double.doubleToLongBits() equality: NaN equals NaN, and -0.0 differs from 0.0.
bool sameDoubleBits(double lhs, double rhs) noexcept
{
    if (std::isnan(lhs) && std::isnan(rhs))
    {
        return true;
    }
    return std::bit_cast<std::uint64_t>(lhs) == std::bit_cast<std::uint64_t>(rhs);
}

}  // namespace

Warning::Other Warning::fromString(std::string text, MessagePriority priority)
{
    return Other{std::move(text), priority};
}

Warning::Other Warning::fromString(std::string text)
{
    return fromString(std::move(text), MessagePriority::NORMAL);
}

// ---- LargeAOA ---------------------------------------------------------------------------------

Warning::LargeAOA::LargeAOA(double aoa) : m_aoa(aoa)
{
    setPriority(MessagePriority::LOW);
}

std::string Warning::LargeAOA::messageDescription() const
{
    if (std::isnan(m_aoa))
    {
        return std::format("{}.", kLargeAoaText);
    }
    return std::format("{} ({})", kLargeAoaText, formatDegrees(m_aoa));
}

bool Warning::LargeAOA::replaceBy(const Message& other) const
{
    const auto* o = dynamic_cast<const LargeAOA*>(&other);
    if (o == nullptr)
    {
        return false;
    }
    if (std::isnan(m_aoa))  // an unknown angle is replaced by any known one
    {
        return true;
    }
    return o->m_aoa > m_aoa;
}

void Warning::LargeAOA::replaceContents(const Message& other)
{
    const auto* o = dynamic_cast<const LargeAOA*>(&other);
    if (o == nullptr)
    {
        throw std::invalid_argument("LargeAOA::replaceContents needs a LargeAOA");
    }
    m_aoa = o->m_aoa;
}

std::unique_ptr<Message> Warning::LargeAOA::clone() const
{
    return std::make_unique<LargeAOA>(*this);
}

// ---- Deployment-speed warnings ----------------------------------------------------------------

Warning::RecoveryHighSpeedDeployment::RecoveryHighSpeedDeployment(double         speed,
                                                                  MessageSources chutes)
  : m_speed(speed)
{
    setSources(std::move(chutes));
    setPriority(MessagePriority::NORMAL);
}

std::string Warning::RecoveryHighSpeedDeployment::messageDescription() const
{
    return speedDescription(kRecoveryHighSpeedText, m_speed);
}

std::unique_ptr<Message> Warning::RecoveryHighSpeedDeployment::clone() const
{
    return std::make_unique<RecoveryHighSpeedDeployment>(*this);
}

Warning::HighSpeedMainDeployment::HighSpeedMainDeployment(double speed, MessageSources chutes)
  : m_speed(speed)
{
    setSources(std::move(chutes));
    setPriority(MessagePriority::NORMAL);
}

std::string Warning::HighSpeedMainDeployment::messageDescription() const
{
    return speedDescription(kMainHighSpeedText, m_speed);
}

std::unique_ptr<Message> Warning::HighSpeedMainDeployment::clone() const
{
    return std::make_unique<HighSpeedMainDeployment>(*this);
}

Warning::LowSpeedMainDeployment::LowSpeedMainDeployment(double speed, MessageSources chutes)
  : m_speed(speed)
{
    setSources(std::move(chutes));
    setPriority(MessagePriority::NORMAL);
}

std::string Warning::LowSpeedMainDeployment::messageDescription() const
{
    return speedDescription(kMainLowSpeedText, m_speed);
}

std::unique_ptr<Message> Warning::LowSpeedMainDeployment::clone() const
{
    return std::make_unique<LowSpeedMainDeployment>(*this);
}

Warning::LowSpeedDrogueDeployment::LowSpeedDrogueDeployment(double speed, MessageSources chutes)
  : m_speed(speed)
{
    setSources(std::move(chutes));
    setPriority(MessagePriority::NORMAL);
}

std::string Warning::LowSpeedDrogueDeployment::messageDescription() const
{
    return speedDescription(kDrogueLowSpeedText, m_speed);
}

std::unique_ptr<Message> Warning::LowSpeedDrogueDeployment::clone() const
{
    return std::make_unique<LowSpeedDrogueDeployment>(*this);
}

// ---- RecoveryDrogueWithoutMain ----------------------------------------------------------------

Warning::RecoveryDrogueWithoutMain::RecoveryDrogueWithoutMain()
{
    setPriority(MessagePriority::HIGH);
}

std::string Warning::RecoveryDrogueWithoutMain::messageDescription() const
{
    return std::string{kDrogueNoMainText};
}

std::unique_ptr<Message> Warning::RecoveryDrogueWithoutMain::clone() const
{
    return std::make_unique<RecoveryDrogueWithoutMain>(*this);
}

// ---- EventAfterLanding ------------------------------------------------------------------------

Warning::EventAfterLanding::EventAfterLanding(std::optional<std::string> eventType)
  : m_eventType(std::move(eventType))
{
    setPriority(MessagePriority::HIGH);
}

std::string Warning::EventAfterLanding::messageDescription() const
{
    if (m_eventType.has_value())
    {
        return std::format("{}{}", kEventAfterLandingText, *m_eventType);
    }
    return std::string{kEventAfterLandingText};
}

std::unique_ptr<Message> Warning::EventAfterLanding::clone() const
{
    return std::make_unique<EventAfterLanding>(*this);
}

bool Warning::EventAfterLanding::equals(const Message& other) const
{
    const auto* o = dynamic_cast<const EventAfterLanding*>(&other);
    return o != nullptr && id() == o->id();
}

// ---- MissingMotor -----------------------------------------------------------------------------

Warning::MissingMotor::MissingMotor()
  : m_diameter(std::numeric_limits<double>::quiet_NaN()),
    m_length(std::numeric_limits<double>::quiet_NaN()),
    m_delay(std::numeric_limits<double>::quiet_NaN())
{
    setPriority(MessagePriority::HIGH);
}

std::string Warning::MissingMotor::messageDescription() const
{
    std::string text =
        std::format("No motor with designation '{}'", m_designation.value_or("null"));
    if (m_manufacturer.has_value())
    {
        text += std::format(" for manufacturer '{}'", *m_manufacturer);
    }
    text += " found.";
    return text;
}

std::unique_ptr<Message> Warning::MissingMotor::clone() const
{
    return std::make_unique<MissingMotor>(*this);
}

bool Warning::MissingMotor::equals(const Message& other) const
{
    const auto* o = dynamic_cast<const MissingMotor*>(&other);
    return o != nullptr && Message::equals(other) && sameDoubleBits(m_delay, o->m_delay) &&
           m_designation == o->m_designation && sameDoubleBits(m_diameter, o->m_diameter) &&
           m_digest == o->m_digest && sameDoubleBits(m_length, o->m_length) &&
           m_manufacturer == o->m_manufacturer && m_type == o->m_type;
}

// ---- Other ------------------------------------------------------------------------------------

Warning::Other::Other(std::string description, MessagePriority priority)
  : m_description(std::move(description))
{
    setPriority(priority);
}

std::unique_ptr<Message> Warning::Other::clone() const
{
    return std::make_unique<Other>(*this);
}

bool Warning::Other::equals(const Message& other) const
{
    const auto* o = dynamic_cast<const Other*>(&other);
    return o != nullptr && Message::equals(other) && m_description == o->m_description;
}

// ---- The fixed-text warnings ------------------------------------------------------------------

// These are the static instances OpenRocket has. Their construction allocates a std::string and
// an id; an allocation failure that early is fatal whatever we do.
// NOLINTBEGIN(bugprone-throwing-static-initialization)
const Warning::Other Warning::kDiameterDiscontinuity{"Discontinuity in rocket body diameter",
                                                     MessagePriority::LOW};
const Warning::Other Warning::kOpenAirframeForward{"Open forward airframe (diameter > 0)",
                                                   MessagePriority::LOW};
const Warning::Other Warning::kAirframeGap{"Gap in rocket airframe", MessagePriority::LOW};
const Warning::Other Warning::kAirframeOverlap{"Overlap in airframe components",
                                               MessagePriority::LOW};
const Warning::Other Warning::kPodsetForward{"In-line podset forward of parent airframe component",
                                             MessagePriority::LOW};
const Warning::Other Warning::kPodsetOverlap{"In-line podset overlaps parent airframe component",
                                             MessagePriority::LOW};
const Warning::Other Warning::kThickFin{"Thick fins may not simulate accurately",
                                        MessagePriority::LOW};
const Warning::Other Warning::kJaggedEdgedFin{"Jagged-edged fin predictions may be inaccurate",
                                              MessagePriority::LOW};
const Warning::Other Warning::kZeroAreaFin{"Fins with zero area will not affect aerodynamics",
                                           MessagePriority::LOW};
const Warning::Other Warning::kListenersAffected{"Listeners modified the flight simulation",
                                                 MessagePriority::LOW};
const Warning::Other Warning::kNoRecoveryDevice{"No recovery device defined in the simulation.",
                                                MessagePriority::HIGH};
const Warning::Other Warning::kFileInvalidParameter{"Invalid parameter encountered, ignoring.",
                                                    MessagePriority::NORMAL};
const Warning::Other Warning::kParallelFins{"Too many parallel fins", MessagePriority::LOW};
const Warning::Other Warning::kSupersonic{
    "Body calculations may not be entirely accurate at supersonic speeds.",
    MessagePriority::NORMAL};
const Warning::Other Warning::kRecoveryLaunchRod{
    "Recovery device deployed while on the launch guide.", MessagePriority::HIGH};
const Warning::Other Warning::kTumbleUnderThrust{"Stage began to tumble under thrust.",
                                                 MessagePriority::HIGH};
const Warning::Other Warning::kZeroVolumeBody{"Zero-volume bodies may not simulate accurately",
                                              MessagePriority::LOW};
const Warning::Other Warning::kTubeIsolated{"Isolated tube fins may not simulate accurately",
                                            MessagePriority::LOW};
const Warning::Other Warning::kTubeSeparation{"Space between tube fins may not simulate accurately",
                                              MessagePriority::LOW};
const Warning::Other Warning::kTubeOverlap{"Overlapping tube fins may not simulate accurately",
                                           MessagePriority::LOW};
const Warning::Other Warning::kObjZeroThickness{
    "Zero-thickness component can cause issues for 3D printing", MessagePriority::LOW};
const Warning::Other Warning::kSeparationOrder{"Stages separated in an unreasonable order",
                                               MessagePriority::NORMAL};
const Warning::Other Warning::kEarlySeparation{"Stages separated before clearing launch rod/rail",
                                               MessagePriority::HIGH};
const Warning::Other Warning::kEmptyBranch{"Simulation branch contains no data",
                                           MessagePriority::HIGH};
// NOLINTEND(bugprone-throwing-static-initialization)

}  // namespace QtRocket
