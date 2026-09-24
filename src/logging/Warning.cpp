#include "QtRocket/logging/Warning.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

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

/// The text of the four deployment-speed warnings: the speed in brackets, in the user's default
/// velocity unit (UnitGroup.UNITS_VELOCITY.toStringUnit), unless it is NaN.
std::string speedDescription(std::string_view text, double speed)
{
    if (std::isnan(speed))
    {
        return std::string{text};
    }
    return std::format("{} ({})", text, unitGroup(UnitGroupId::VELOCITY).toStringUnit(speed));
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
    // UnitGroup.UNITS_ANGLE.toStringUnit: the user's default angle unit ("17.2°" in degrees).
    return std::format("{} ({})", kLargeAoaText, unitGroup(UnitGroupId::ANGLE).toStringUnit(m_aoa));
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
        bug("LargeAOA::replaceContents needs a LargeAOA");
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
