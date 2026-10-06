#pragma once

#include <limits>
#include <numbers>

namespace QtRocket
{

/// Decides whether the rocket is tumbling, from how long its angle of attack has persisted
/// rather than from its instantaneous value (OpenRocket's simulation/TumbleDetector).
///
/// What distinguishes a tumbling rocket from a gust is not the size of the angle of attack at
/// one instant but whether the rocket recovers. A statically stable rocket returns to alignment
/// within about a pitch period; a tumbling rocket does not. The detector therefore low-pass
/// filters the angle of attack with a time constant taken from the rocket's own pitch natural
/// frequency, and declares tumbling only once the filtered value is sustained. No aerodynamic
/// coefficient enters the decision, and no flight-phase gating is needed in either direction:
/// the launch guide departure transient does not survive the filter, and descent tumbling does.
///
/// The angle is measured against the air-relative velocity rather than the ground-relative
/// trajectory: a stable rocket aligns itself with the air, so in a crosswind its axis is offset
/// from its ground track by atan(wind / airspeed) purely as a matter of geometry, an offset
/// that reaches tens of degrees at the low speeds just after launch guide departure, which is
/// where the detector must not fire.
///
/// The filter: with dt the time since the previous update(), and tau the time constant,
///   filteredAOA += (1 - exp(-dt / tau)) * (aoa - filteredAOA),
/// tau being kDwellPeriods pitch periods (2 pi / naturalFrequency each) held within
/// kMinTimeConstant ... kMaxTimeConstant, and kMinTimeConstant for a rocket without a natural
/// frequency (NaN, zero or negative: statically unstable, or not yet computable).
///
/// A copy carries the filter state (Java: the copy constructor), so that the branch of a stage
/// that separates inherits what its parent accumulated.
///
/// Deviation: the constants are public (Java: private), for the tests.
class TumbleDetector
{
public:
    /// The filtered angle of attack above which the rocket counts as tumbling, rad: 60 degrees,
    /// written as in Java (TUMBLE_THRESHOLD). Normal flight sits below the stall angle, while a
    /// rocket whose axis is uncorrelated with the airflow averages 90 degrees; the threshold
    /// sits in the gap between the two.
    static constexpr double kTumbleThreshold = 60 * std::numbers::pi / 180;

    /// The dynamic pressure below which the airflow direction carries no usable information,
    /// Pa; at sea level roughly 1.3 m/s (MIN_DYNAMIC_PRESSURE).
    static constexpr double kMinDynamicPressure = 1.0;

    /// The filter time constant in pitch natural periods (DWELL_PERIODS).
    static constexpr double kDwellPeriods = 2.0;

    /// The limits of the filter time constant, s (MIN_TIME_CONSTANT, MAX_TIME_CONSTANT).
    static constexpr double kMinTimeConstant = 0.05;
    static constexpr double kMaxTimeConstant = 2.0;

    /// Advances the detector by one simulation step and reports whether the rocket should be
    /// considered to be tumbling (isTumbling() after the step).
    ///
    /// The step is remembered (@p time becomes the time of the last update) but the filter is
    /// held, not reset, when the angle carries no information: on the first update and whenever
    /// time has not advanced, while the rocket is on the launch guide (held on course, yet a
    /// crosswind across a stationary rocket puts the angle of attack near 90 degrees), for a
    /// NaN angle of attack or dynamic pressure, and below kMinDynamicPressure (0.5 *
    /// @p airDensity * @p airSpeed^2), so that a spell of negligible airflow, such as passing
    /// through apogee, does not discard the evidence accumulated before it.
    ///
    /// @param time             the current simulation time, s
    /// @param guideCleared     whether the rocket has left the launch guide, and so is free to
    ///                         rotate at all
    /// @param aoa              the angle of attack, rad
    /// @param airSpeed         the speed relative to the air, m/s
    /// @param airDensity       the ambient air density, kg/m^3
    /// @param naturalFrequency the pitch natural frequency in rad/s, or NaN when the rocket is
    ///                         statically unstable and so has no restoring dynamics to wait out
    bool update(double time, bool guideCleared, double aoa, double airSpeed, double airDensity,
                double naturalFrequency) noexcept;

    /// Whether the filtered angle of attack exceeds kTumbleThreshold.
    [[nodiscard]] bool isTumbling() const noexcept { return m_filteredAoa > kTumbleThreshold; }

    /// The filtered angle of attack, rad; 0 until the first step that counts.
    [[nodiscard]] double getFilteredAOA() const noexcept { return m_filteredAoa; }

private:
    /// The filter time constant in s for a pitch natural frequency in rad/s (see the class
    /// comment).
    [[nodiscard]] static double timeConstant(double naturalFrequency) noexcept;

    double m_filteredAoa{0.0};
    double m_lastTime{std::numeric_limits<double>::quiet_NaN()};
};

}  // namespace QtRocket
