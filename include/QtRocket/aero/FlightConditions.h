#pragma once

#include <numbers>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{

class ComponentAssembly;
class FlightConfiguration;

/// The momentary flight conditions of a rocket (OpenRocket's aerodynamics/FlightConditions): the
/// angle of attack and its sine and sinc, the direction of the lateral airflow (theta), the Mach
/// number and its Prandtl-Glauert factor beta, the roll, pitch and yaw rates, the pitch centre,
/// the reference length and area, the nozzle exit area of the motors thrusting into each body
/// wake, and the atmospheric conditions. The aerodynamic calculators read it; the simulation
/// fills one per time step.
///
/// Derived values are cached by the setters, as in Java: setAOA() stores the sine and sinc,
/// setMach() the beta, setRefLength() the area and setRefArea() the length. Every setter fires a
/// change event (changed(), Java's ChangeSource) and draws a new modification id when the value
/// changes; the angles, rates and Mach number count as unchanged within MathUtil::equals'
/// relative 1e-8 (so a setter may leave a slightly different value in place), the reference
/// length and area only when exactly equal.
///
/// Copying: a copy is Java's clone(): every value (the modification id included), a copy of the
/// nozzle areas and of the atmospheric conditions, and no connections to changed(). Copy
/// assignment copies the values and keeps the target's own connections, firing nothing. Moving
/// takes the connections along.
///
/// Deviations from OpenRocket:
/// - Java's addChangeListener() puts a new listener first, so listeners run newest first;
///   changed() runs its slots in connection order.
/// - Java's modification id is null until the first change; here it is ModId::invalid() then.
/// - The atmospheric conditions are held by value: Java keeps the caller's object, so that a later
///   change to it shows through (without an event); here setAtmosphericConditions() copies it, and
///   getAtmosphericConditions() gives the held object, whose direct modification fires no event
///   either (as Java documents).
/// - The thrusting nozzle exit areas are kept in the order they were given (Java: a HashMap, whose
///   order follows the components' random ids), so their total is summed in a fixed order.
/// - setAOA(aoa, sinAOA)'s Java assert (sinAOA within 1e-4 of sin(aoa)) is off at run time in
///   OpenRocket and is not ported.
class FlightConditions
{
public:
    /// The nozzle exit area in m^2 of the motors thrusting into one component assembly's terminal
    /// base wake (Java: an entry of Map<ComponentAssembly, Double>).
    using NozzleExitArea = std::pair<const ComponentAssembly*, double>;

    /// Reference length 1 m (area pi/4), angle of attack, theta and rates 0, Mach 0.3, pitch
    /// centre at the origin, standard atmospheric conditions (Java: new FlightConditions(null)).
    FlightConditions() = default;

    /// As the default constructor, with the reference length of @p config (Java: new
    /// FlightConditions(config)).
    explicit FlightConditions(const FlightConfiguration& config);

    FlightConditions(const FlightConditions& other);
    FlightConditions& operator=(const FlightConditions& other);
    FlightConditions(FlightConditions&& other) noexcept            = default;
    FlightConditions& operator=(FlightConditions&& other) noexcept = default;
    ~FlightConditions()                                            = default;

    /// Java's clone(): a copy (see the class comment).
    [[nodiscard]] FlightConditions clone() const { return *this; }

    /// Sets the reference length from @p config (setRefLength(config.getReferenceLength())).
    void setReference(const FlightConfiguration& config);

    /// Sets the reference length and the area pi (length / 2)^2. Nothing happens when @p length
    /// equals the current length exactly.
    void                 setRefLength(double length);
    [[nodiscard]] double getRefLength() const noexcept { return m_refLength; }

    /// Sets the reference area and the length 2 sqrt(area / pi) (MathUtil::safeSqrt). Nothing
    /// happens when @p area equals the current area exactly.
    void                 setRefArea(double area);
    [[nodiscard]] double getRefArea() const noexcept { return m_refArea; }

    /// Sets the nozzle exit areas of the thrusting motors, per component assembly (the core,
    /// each booster and each pod set have a wake of their own, so a motor cannot reduce an
    /// unrelated base). Zero areas are dropped. Nothing happens when the result equals the
    /// current areas (the same assemblies, by RocketComponent::equals(), with the same areas).
    /// @throws BugError for a null assembly, an assembly given twice, or an area that is negative
    ///         or not finite (Java: IllegalArgumentException; Java's map cannot hold a key
    ///         twice); the current areas are then kept.
    void setThrustingNozzleExitAreas(std::span<const NozzleExitArea> areas);

    /// The nozzle exit area in m^2 of the motors thrusting into @p assembly's wake (an assembly
    /// that equals() it), or 0 when none is.
    [[nodiscard]] double getThrustingNozzleExitArea(const ComponentAssembly& assembly) const;

    /// The nozzle exit areas by wake, in the order they were set (Java: an unmodifiable view of
    /// the map).
    [[nodiscard]] const std::vector<NozzleExitArea>& getThrustingNozzleExitAreas() const noexcept
    {
        return m_thrustingNozzleExitAreas;
    }

    /// The total nozzle exit area in m^2 of all thrusting motors, summed in order (for reporting
    /// and the thrust correction for air pressure; the drag uses the per-assembly areas).
    [[nodiscard]] double getThrustingNozzleExitArea() const noexcept;

    /// Sets the angle of attack, clamped to 0 ... pi, with its sine and sinc: below 0.001 the
    /// sine is the angle itself and the sinc 1, otherwise sin(aoa) and sin(aoa) / aoa.
    void setAOA(double aoa);

    /// Sets the angle of attack with a sine the caller already knows (both clamped, the angle to
    /// 0 ... pi and the sine to 0 ... 1); the sinc is sinAOA / aoa, or 1 below 0.001 rad.
    void setAOA(double aoa, double sinAOA);

    [[nodiscard]] double getAOA() const noexcept { return m_aoa; }
    [[nodiscard]] double getSinAOA() const noexcept { return m_sinAOA; }
    /// sin(aoa) / aoa, which is 1 at an angle of attack of zero.
    [[nodiscard]] double getSincAOA() const noexcept { return m_sincAOA; }

    /// Sets the direction of the lateral airflow.
    void                 setTheta(double theta);
    [[nodiscard]] double getTheta() const noexcept { return m_theta; }

    /// Sets the Mach number, raised to at least 0 (Java's Math.max: NaN stays NaN), and its beta.
    void                 setMach(double mach);
    [[nodiscard]] double getMach() const noexcept { return m_mach; }

    /// The velocity in m/s: the Mach number times the speed of sound of the atmospheric
    /// conditions.
    [[nodiscard]] double getVelocity() const noexcept;

    /// Sets the Mach number from @p velocity in m/s and the current speed of sound.
    void setVelocity(double velocity);

    /// The Prandtl-Glauert factor: sqrt(1 - M^2) below Mach 1, sqrt(M^2 - 1) from it, but at
    /// least 0.25 (MathUtil::max, so a NaN Mach number gives 0.25).
    [[nodiscard]] double getBeta() const noexcept { return m_beta; }

    [[nodiscard]] double getRollRate() const noexcept { return m_rollRate; }
    void                 setRollRate(double rate);

    [[nodiscard]] double getPitchRate() const noexcept { return m_pitchRate; }
    void                 setPitchRate(double pitchRate);

    [[nodiscard]] double getYawRate() const noexcept { return m_yawRate; }
    void                 setYawRate(double yawRate);

    [[nodiscard]] const Coordinate& getPitchCenter() const noexcept { return m_pitchCenter; }
    /// Sets the pitch centre; nothing happens when it equals the current one (Coordinate's
    /// tolerant operator==).
    void setPitchCenter(const Coordinate& pitchCenter);

    /// The atmospheric conditions. Modifying them through this reference fires no change event.
    [[nodiscard]] AtmosphericConditions& getAtmosphericConditions() noexcept
    {
        return m_atmosphericConditions;
    }
    [[nodiscard]] const AtmosphericConditions& getAtmosphericConditions() const noexcept
    {
        return m_atmosphericConditions;
    }

    /// Sets (copies) the atmospheric conditions; nothing happens when they equal the current ones
    /// (AtmosphericConditions::operator==, which includes the humidity).
    void setAtmosphericConditions(const AtmosphericConditions& conditions);

    /// The id of the current state (Monitorable): a new one on every change event.
    [[nodiscard]] ModId modId() const noexcept { return m_modId; }

    /// Emitted after every change (Java's ChangeSource; see the class comment for the order).
    [[nodiscard]] Signal<>& changed() noexcept { return m_changed; }

    /// Java's toString(): "FlightConditions[aoa=<deg>°,theta=<deg>°,mach=<M>,
    /// thrustingNozzleExitArea=<m^2>,rollRate=<r>,pitchRate=<r>,yawRate=<r>,refLength=<m>,
    /// pitchCenter=<Coordinate>,atmosphericConditions=<AtmosphericConditions>]" with 2, 2, 3, 6,
    /// 2, 2, 2 and 3 decimals (Java's %.2f, ... in an English locale, Strings::formatFixed).
    [[nodiscard]] std::string toString() const;

    /// Java's equals(): the reference length, angle of attack, theta, Mach number and rates
    /// within MathUtil::equals (never for a NaN), the same nozzle areas, the pitch centres and
    /// atmospheric conditions equal (both tolerant). The reference area, sine, sinc and beta
    /// follow from these; the modification id does not count. An object always equals itself.
    [[nodiscard]] bool operator==(const FlightConditions& other) const noexcept;

    /// Java's hashCode(): 31 * (int)(1000 * (refLength + aoa + theta + mach + rollRate +
    /// pitchRate + yawRate)) plus the nozzle areas' HashMap.hashCode() (the sum over the entries
    /// of the assembly's hashCode() XOR Double.hashCode(area)), in Java's int arithmetic.
    [[nodiscard]] int hashCode() const noexcept;

private:
    /// The smallest beta (MIN_BETA).
    static constexpr double kMinBeta = 0.25;

    /// calculateBeta(): sqrt(|1 - M^2|), at least kMinBeta.
    [[nodiscard]] static double calculateBeta(double mach) noexcept;

    /// Draws a new modification id and emits changed().
    void fireChangeEvent();

    /// Copies every value of @p other (not the connections).
    void copyValuesFrom(const FlightConditions& other);

    double                      m_refLength{1.0};
    double                      m_refArea{std::numbers::pi * 0.25};
    std::vector<NozzleExitArea> m_thrustingNozzleExitAreas;
    double                      m_aoa{0};
    double                      m_sinAOA{0};
    double                      m_sincAOA{1.0};
    double                      m_theta{0};
    double                      m_mach{0.3};
    double                      m_beta{calculateBeta(0.3)};
    double                      m_rollRate{0};
    double                      m_pitchRate{0};
    double                      m_yawRate{0};
    Coordinate                  m_pitchCenter{Coordinate::kNul};
    AtmosphericConditions       m_atmosphericConditions;
    ModId                       m_modId{ModId::invalid()};
    Signal<>                    m_changed;
};

}  // namespace QtRocket
