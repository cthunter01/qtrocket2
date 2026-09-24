#include "QtRocket/logging/SimulationAbort.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/logging/Message.h"

namespace QtRocket
{

SimulationAbort::SimulationAbort(Cause cause) : m_cause(cause) { }

std::string SimulationAbort::messageDescription() const
{
    return std::string{causeText(m_cause)};
}

std::unique_ptr<Message> SimulationAbort::clone() const
{
    return std::make_unique<SimulationAbort>(*this);
}

std::string_view causeText(SimulationAbort::Cause cause) noexcept
{
    using Cause = SimulationAbort::Cause;
    switch (cause)
    {
        case Cause::NO_ACTIVE_STAGES:
            return "No active stages";
        case Cause::NO_MOTORS_DEFINED:
            return "No motors defined in the simulation";
        case Cause::NO_CONFIGURED_IGNITION:
            return "No motors configured to ignite at liftoff";
        case Cause::NO_MOTORS_FIRED:
            return "No motors ignited";
        case Cause::NO_LIFTOFF:
            return "<html>Motor burnout without liftoff. <br>Use more (powerful) motors, or "
                   "decrease the rocket mass.</html>";
        case Cause::NO_CP:
            return "Can't calculate Center of Pressure";
        case Cause::ACTIVE_LENGTH_ZERO:
            return "Active airframe has length 0";
        case Cause::ACTIVE_MASS_ZERO:
            return "Total mass of active stages is 0";
        case Cause::TUMBLE_UNDER_THRUST:
            return "Stage began to tumble under thrust.";
        case Cause::DEPLOY_UNDER_THRUST:
            return "Recovery system deployed while still under thrust";
    }
    return "";
}

std::string_view causeName(SimulationAbort::Cause cause) noexcept
{
    using Cause = SimulationAbort::Cause;
    switch (cause)
    {
        case Cause::NO_ACTIVE_STAGES:
            return "NO_ACTIVE_STAGES";
        case Cause::NO_MOTORS_DEFINED:
            return "NO_MOTORS_DEFINED";
        case Cause::NO_CONFIGURED_IGNITION:
            return "NO_CONFIGURED_IGNITION";
        case Cause::NO_MOTORS_FIRED:
            return "NO_MOTORS_FIRED";
        case Cause::NO_LIFTOFF:
            return "NO_LIFTOFF";
        case Cause::NO_CP:
            return "NO_CP";
        case Cause::ACTIVE_LENGTH_ZERO:
            return "ACTIVE_LENGTH_ZERO";
        case Cause::ACTIVE_MASS_ZERO:
            return "ACTIVE_MASS_ZERO";
        case Cause::TUMBLE_UNDER_THRUST:
            return "TUMBLE_UNDER_THRUST";
        case Cause::DEPLOY_UNDER_THRUST:
            return "DEPLOY_UNDER_THRUST";
    }
    return "";
}

std::optional<SimulationAbort::Cause> causeFromName(std::string_view name) noexcept
{
    const auto* const it = std::ranges::find_if(
        SimulationAbort::kAllCauses,
        [name](SimulationAbort::Cause cause) { return causeName(cause) == name; });
    if (it == SimulationAbort::kAllCauses.end())
    {
        return std::nullopt;
    }
    return *it;
}

}  // namespace QtRocket
