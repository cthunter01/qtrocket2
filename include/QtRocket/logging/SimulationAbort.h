#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/logging/Message.h"

namespace QtRocket
{

/// Why a simulation stopped early (OpenRocket: logging/SimulationAbort). It travels as the data
/// of a SIM_ABORT flight event, not in a WarningSet. Equality is Message's: two aborts are equal
/// whatever their causes, as in Java, which does not override equals() here.
class SimulationAbort final : public Message
{
public:
    /// The possible causes (Java: SimulationAbort.Cause).
    enum class Cause
    {
        NO_ACTIVE_STAGES,        ///< no active stages in the simulation
        NO_MOTORS_DEFINED,       ///< no motors defined in the configuration
        NO_CONFIGURED_IGNITION,  ///< motors defined, but none configured to fire at liftoff
        NO_MOTORS_FIRED,         ///< no motor ignited
        NO_LIFTOFF,              ///< motors ignited, but the rocket did not lift off
        NO_CP,                   ///< the active components' centre of pressure cannot be computed
        ACTIVE_LENGTH_ZERO,      ///< the active components have a total length of 0
        ACTIVE_MASS_ZERO,        ///< the active components have a total mass of 0
        TUMBLE_UNDER_THRUST,     ///< a stage is tumbling under thrust
        DEPLOY_UNDER_THRUST,     ///< a recovery device deployed while a motor is still burning
    };

    /// Every cause, in declaration order (Java: Cause.values()).
    static constexpr std::array<Cause, 10> kAllCauses{
        Cause::NO_ACTIVE_STAGES,   Cause::NO_MOTORS_DEFINED, Cause::NO_CONFIGURED_IGNITION,
        Cause::NO_MOTORS_FIRED,    Cause::NO_LIFTOFF,        Cause::NO_CP,
        Cause::ACTIVE_LENGTH_ZERO, Cause::ACTIVE_MASS_ZERO,  Cause::TUMBLE_UNDER_THRUST,
        Cause::DEPLOY_UNDER_THRUST};

    explicit SimulationAbort(Cause cause);

    [[nodiscard]] Cause cause() const noexcept { return m_cause; }

    /// The cause's text (Java: cause.toString()).
    [[nodiscard]] std::string messageDescription() const override;
    [[nodiscard]] bool        replaceBy(const Message& /*other*/) const override { return false; }
    [[nodiscard]] std::unique_ptr<Message> clone() const override;
    [[nodiscard]] std::string_view typeName() const noexcept override { return "SimulationAbort"; }

private:
    Cause m_cause;
};

/// The user-visible text of a cause, with OpenRocket's English wording (Java: Cause.toString();
/// named causeText() because Message::toString() would hide a free toString() inside every
/// Message subclass). NO_LIFTOFF keeps OpenRocket's `<html>...<br>...</html>` markup.
[[nodiscard]] std::string_view causeText(SimulationAbort::Cause cause) noexcept;

/// The constant's name, e.g. "NO_ACTIVE_STAGES" (Java: Cause.name()). .ork files store it lower
/// case without underscores ("noactivestages"); that is the file loader's business.
[[nodiscard]] std::string_view causeName(SimulationAbort::Cause cause) noexcept;

/// The cause with that name, or nullopt (Java: Cause.valueOf()).
[[nodiscard]] std::optional<SimulationAbort::Cause> causeFromName(std::string_view name) noexcept;

}  // namespace QtRocket
