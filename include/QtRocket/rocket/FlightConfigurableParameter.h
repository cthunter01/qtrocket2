#pragma once

#include <concepts>

#include "QtRocket/rocket/FlightConfigurationId.h"

namespace QtRocket
{

/// A parameter object that can differ per flight configuration, the element type of a
/// FlightConfigurableParameterSet (OpenRocket's FlightConfigurableParameter<E>): stage
/// separation, recovery deployment, motor and flight configurations.
///
/// Java's generic interface becomes a concept, since the parameters are value types held by the
/// set (a virtual base would give each of them a vtable for no caller that needs one):
/// - `e.clone()` returns an exact copy of @p e (used when a whole set is copied);
/// - `e.copy(fcid)` returns a copy that belongs to the configuration @p fcid (used by
///   copyFlightConfiguration(); a parameter that does not store its id ignores it);
/// - `e.update()` refreshes derived state after the set changed;
/// - `a == b` is the Java class's equals(), which FlightConfigurableParameterSet uses in
///   setDefault(), isDefault(E), getId(E) and toDebug(). It must reproduce that equals() exactly,
///   which is not always every field: FlightConfiguration compares its id only,
///   MotorConfiguration its MotorConfigurationId, and StageSeparationConfiguration ignores the
///   separation altitude.
/// Both copies return E by value, so E must be move constructible.
template <class E>
concept FlightConfigurableParameter =
    std::move_constructible<E> && std::equality_comparable<E> &&
    requires(const E& constValue, E& value, const FlightConfigurationId& fcid) {
        { constValue.clone() } -> std::convertible_to<E>;
        { constValue.copy(fcid) } -> std::convertible_to<E>;
        value.update();
    };

}  // namespace QtRocket
