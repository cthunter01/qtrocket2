#pragma once

#include <array>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace QtRocket
{

class Preferences;

/// A rocket motor: its identity (type, code, designation, common name, digest), its dimensions,
/// and its thrust, mass and centre of mass over the time since ignition. Ported from OpenRocket's
/// Motor interface; ThrustCurveMotor is the only implementation, and the motor database hands
/// motors out as std::shared_ptr<const ThrustCurveMotor>, since OpenRocket shares one immutable
/// instance between the database and every configuration that uses it.
///
/// Masses are in kg, lengths and CG positions in m (measured from the motor's top), times in s,
/// thrust in N and unit inertias in m^2.
class Motor
{
public:
    /// The kinds of rocket motor (OpenRocket's Motor.Type), in declaration order.
    enum class Type
    {
        SINGLE,   ///< "Single-use": single-use solid propellant motor
        RELOAD,   ///< "Reloadable": reloadable solid propellant motor
        HYBRID,   ///< "Hybrid": hybrid rocket motor engine
        UNKNOWN,  ///< "Unknown": unknown motor type
    };

    /// Motor.Type.values(), in declaration order.
    static constexpr std::array<Type, 4> kAllTypes{Type::SINGLE, Type::RELOAD, Type::HYBRID,
                                                   Type::UNKNOWN};

    /// The pseudo-time of a motor with no propellant left to account for (PSEUDO_TIME_EMPTY).
    static constexpr double kPseudoTimeEmpty = std::numeric_limits<double>::quiet_NaN();
    /// The pseudo-time of a motor at launch (PSEUDO_TIME_LAUNCH).
    static constexpr double kPseudoTimeLaunch = 0.0;
    /// The pseudo-time of a burnt-out motor (PSEUDO_TIME_BURNOUT = Double.MAX_VALUE).
    static constexpr double kPseudoTimeBurnout = std::numeric_limits<double>::max();

    /// The ejection delay of a "plugged" motor, one without an ejection charge (PLUGGED_DELAY =
    /// Double.POSITIVE_INFINITY).
    static constexpr double kPluggedDelay = std::numeric_limits<double>::infinity();

    /// The share of the maximum thrust below which the motor counts as off when the burn time and
    /// the average thrust are estimated: NFPA 1125's "official" burn time is the time the motor
    /// produces more than 5% of its maximum thrust (MARGINAL_THRUST).
    static constexpr double kMarginalThrust = 0.05;

    virtual ~Motor() = default;

    /// The motor type. For a ThrustCurveMotor from the database, the type of its
    /// ThrustCurveMotorSet is usually the one to show.
    [[nodiscard]] virtual Type getMotorType() const = 0;

    /// The motor code (for database motors, the designation as thrustcurve.org spells it).
    [[nodiscard]] virtual const std::string& getCode() const = 0;

    /// The common name, e.g. "H128".
    [[nodiscard]] virtual const std::string& getCommonName() const = 0;

    /// The common name followed by "-" and the delay (see ThrustCurveMotor::getDelayString).
    [[nodiscard]] virtual std::string getCommonName(double delay) const = 0;

    /// The designation, e.g. "H128W".
    [[nodiscard]] virtual const std::string& getDesignation() const = 0;

    /// The designation followed by "-" and the delay (see ThrustCurveMotor::getDelayString).
    [[nodiscard]] virtual std::string getDesignation(double delay) const = 0;

    /// The designation when @p preferences choose the designation for the motor name column
    /// (Preferences::getMotorNameColumn()), otherwise the common name. OpenRocket reads the
    /// application preferences itself; here the caller passes them.
    [[nodiscard]] const std::string& getMotorName(const Preferences& preferences) const;

    /// As getMotorName(preferences), with the delay appended.
    [[nodiscard]] std::string getMotorName(const Preferences& preferences, double delay) const;

    /// Extra description, such as comments on the source of the thrust curve; may contain line
    /// breaks.
    [[nodiscard]] virtual const std::string& getDescription() const = 0;

    /// The maximum diameter of the motor.
    [[nodiscard]] virtual double getDiameter() const = 0;

    /// The characteristic length of the motor: typically from its bottom to the end of the
    /// maximum-diameter part, ignoring smaller ejection charge compartments.
    [[nodiscard]] virtual double getLength() const = 0;

    /// The digest identifying the motor's functional data (see MotorDigest).
    [[nodiscard]] virtual const std::string& getDigest() const = 0;

    [[nodiscard]] virtual double getLaunchCGx() const   = 0;
    [[nodiscard]] virtual double getBurnoutCGx() const  = 0;
    [[nodiscard]] virtual double getLaunchMass() const  = 0;
    [[nodiscard]] virtual double getBurnoutMass() const = 0;

    /// An estimate of the burn time, or NaN when none is available.
    [[nodiscard]] virtual double getBurnTimeEstimate() const = 0;

    /// An estimate of the average thrust, or NaN when none is available.
    [[nodiscard]] virtual double getAverageThrustEstimate() const = 0;

    /// An estimate of the maximum thrust, or NaN when none is available.
    [[nodiscard]] virtual double getMaxThrustEstimate() const = 0;

    /// An estimate of the total impulse, or NaN when none is available.
    [[nodiscard]] virtual double getTotalImpulseEstimate() const = 0;

    /// The time of the last thrust sample.
    [[nodiscard]] virtual double getBurnTime() const = 0;

    /// The thrust @p motorTime seconds after ignition.
    [[nodiscard]] virtual double getThrust(double motorTime) const = 0;

    /// The total mass @p motorTime seconds after ignition.
    [[nodiscard]] virtual double getTotalMass(double motorTime) const = 0;

    /// The propellant mass left @p motorTime seconds after ignition.
    [[nodiscard]] virtual double getPropellantMass(double motorTime) const = 0;

    /// The position of the centre of mass @p motorTime seconds after ignition.
    [[nodiscard]] virtual double getCMx(double motorTime) const = 0;

    [[nodiscard]] virtual double getUnitIxx() const = 0;
    [[nodiscard]] virtual double getUnitIyy() const = 0;
    [[nodiscard]] virtual double getUnitIzz() const = 0;

protected:
    Motor()                        = default;
    Motor(const Motor&)            = default;
    Motor(Motor&&)                 = default;
    Motor& operator=(const Motor&) = default;
    Motor& operator=(Motor&&)      = default;
};

/// The short name of a motor type (Type.getName() and Type.toString()): "Single-use",
/// "Reloadable", "Hybrid" or "Unknown".
[[nodiscard]] std::string_view name(Motor::Type type) noexcept;

/// The long description of a motor type (Type.getDescription()), e.g. "Hybrid rocket motor
/// engine".
[[nodiscard]] std::string_view description(Motor::Type type) noexcept;

/// The name a motor type has in .ork files, the enum constant's name in lower case: "single",
/// "reload", "hybrid" or "unknown" (RocketComponentSaver writes no <type> for UNKNOWN).
[[nodiscard]] std::string_view orkName(Motor::Type type) noexcept;

/// The motor type whose orkName() is @p name, compared exactly as MotorHandler does; nullopt for
/// an unknown name (OpenRocket then warns and ignores the element).
[[nodiscard]] std::optional<Motor::Type> motorTypeFromOrkName(std::string_view name) noexcept;

/// The name of the enum constant (Type.name()): "SINGLE", "RELOAD", "HYBRID" or "UNKNOWN". This
/// is how logging's Warning::MissingMotor stores the type, since logging/ sits below motor/.
[[nodiscard]] std::string_view enumName(Motor::Type type) noexcept;

/// The motor type whose enumName() is @p name, compared exactly (Type.valueOf()); nullopt for an
/// unknown name, where OpenRocket throws IllegalArgumentException.
[[nodiscard]] std::optional<Motor::Type> motorTypeFromEnumName(std::string_view name) noexcept;

}  // namespace QtRocket
