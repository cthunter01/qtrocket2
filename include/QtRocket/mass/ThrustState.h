#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace QtRocket
{

/// The state of a motor cluster during a simulation (OpenRocket's simulation/ThrustState), in
/// Java's declaration order. A motor goes ARMED -> THRUSTING -> DELAYING -> SPENT (a plugged
/// motor skips DELAYING); MotorClusterState drives the transitions.
///
/// Java's enum carries a name, a description, the next state and a sequence number (SPENT 10,
/// every other state one less than its successor's); here they are the constexpr functions below.
/// It lives in mass/ with MotorClusterState (see there).
enum class ThrustState
{
    SPENT,      ///< "Spent": finished producing thrust
    DELAYING,   ///< "Delaying": after burnout, before the ejection charge
    THRUSTING,  ///< "Thrusting": producing thrust
    ARMED,      ///< "Armed": not yet lit
};

/// ThrustState.values(), in declaration order.
inline constexpr std::array<ThrustState, 4> kAllThrustStates{
    ThrustState::SPENT, ThrustState::DELAYING, ThrustState::THRUSTING, ThrustState::ARMED};

/// The short name (getName() and toString()): "Spent", "Delaying", "Thrusting" or "Armed".
[[nodiscard]] constexpr std::string_view name(ThrustState state) noexcept
{
    switch (state)
    {
        case ThrustState::SPENT:
            return "Spent";
        case ThrustState::DELAYING:
            return "Delaying";
        case ThrustState::THRUSTING:
            return "Thrusting";
        case ThrustState::ARMED:
            break;
    }
    return "Armed";
}

/// The name of the enum constant (Java: name()): "SPENT", "DELAYING", "THRUSTING" or "ARMED".
[[nodiscard]] constexpr std::string_view enumName(ThrustState state) noexcept
{
    switch (state)
    {
        case ThrustState::SPENT:
            return "SPENT";
        case ThrustState::DELAYING:
            return "DELAYING";
        case ThrustState::THRUSTING:
            return "THRUSTING";
        case ThrustState::ARMED:
            break;
    }
    return "ARMED";
}

/// The state whose enumName() is @p name, compared exactly (ThrustState.valueOf()); nullopt for
/// an unknown name, where Java throws IllegalArgumentException.
[[nodiscard]] constexpr std::optional<ThrustState> thrustStateFromEnumName(
    std::string_view name) noexcept
{
    for (const ThrustState state : kAllThrustStates)
    {
        if (enumName(state) == name)
        {
            return state;
        }
    }
    return std::nullopt;
}

/// The long description (getDescription()), Java's text verbatim (DELAYING's starts with a
/// space).
[[nodiscard]] constexpr std::string_view description(ThrustState state) noexcept
{
    switch (state)
    {
        case ThrustState::SPENT:
            return "Finished Producing thrust.";
        case ThrustState::DELAYING:
            return " After Burnout, but before ejection";
        case ThrustState::THRUSTING:
            return "Currently Producing thrust";
        case ThrustState::ARMED:
            break;
    }
    return "Armed, but not yet lit.";
}

/// The state that follows @p state (getNext()); nullopt after SPENT (Java: null).
[[nodiscard]] constexpr std::optional<ThrustState> nextState(ThrustState state) noexcept
{
    switch (state)
    {
        case ThrustState::SPENT:
            return std::nullopt;
        case ThrustState::DELAYING:
            return ThrustState::SPENT;
        case ThrustState::THRUSTING:
            return ThrustState::DELAYING;
        case ThrustState::ARMED:
            break;
    }
    return ThrustState::THRUSTING;
}

/// The place of @p state in the sequence (getSequenceNumber()): SPENT 10 (Java's arbitrary
/// SEQUENCE_NUMBER_END), each earlier state one less than the state after it, so ARMED is 7.
[[nodiscard]] constexpr int sequenceNumber(ThrustState state) noexcept
{
    constexpr int                    kSequenceNumberEnd = 10;
    const std::optional<ThrustState> following          = nextState(state);
    if (!following)
    {
        return kSequenceNumberEnd;
    }
    return -1 + sequenceNumber(*following);
}

/// Whether @p state comes after @p other in the sequence (isAfter()).
[[nodiscard]] constexpr bool isAfter(ThrustState state, ThrustState other) noexcept
{
    return sequenceNumber(state) > sequenceNumber(other);
}

/// Whether @p state comes before @p other in the sequence (isBefore()).
[[nodiscard]] constexpr bool isBefore(ThrustState state, ThrustState other) noexcept
{
    return sequenceNumber(state) < sequenceNumber(other);
}

/// Whether the motor produces thrust in @p state: THRUSTING only (isThrusting()).
[[nodiscard]] constexpr bool isThrusting(ThrustState state) noexcept
{
    return state == ThrustState::THRUSTING;
}

/// Whether a motor in @p state has its state, CG and thrust updated by the simulation:
/// THRUSTING and DELAYING (needsSimulation()).
[[nodiscard]] constexpr bool needsSimulation(ThrustState state) noexcept
{
    return state == ThrustState::THRUSTING || state == ThrustState::DELAYING;
}

}  // namespace QtRocket
