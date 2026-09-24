#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "QtRocket/motor/Motor.h"

namespace QtRocket
{

class Manufacturer;
class ThrustCurveMotor;

/// The thrust curves of one motor (OpenRocket's ThrustCurveMotorSet): motors of the same
/// manufacturer, diameter, length, designation, common name and case, typically the curves
/// different sources measured. The first motor added fixes those properties; the type is the
/// first one other than UNKNOWN, and the delays are the union of every motor's standard delays.
///
/// Before the first motor is added, getManufacturer() is nullptr (OpenRocket: null) and the
/// names are empty (null there).
class ThrustCurveMotorSet
{
public:
    /// Adds @p motor unless an equivalent one is present (addMotor()): a motor with the same
    /// digest and designation is dropped when its description, whitespace collapsed, is empty or
    /// the same as the present one's, and replaces the present one when that one's is empty;
    /// otherwise both are kept. The motors stay sorted by designation (Java's String.compareTo),
    /// then more data points first, then longer description first (a replacement takes the place
    /// of the motor it replaces without re-sorting, as in OpenRocket). The delays gain the
    /// motor's standard delays rounded half to even, each once, in Double.compare order; a
    /// hybrid type (from the first typed motor) adds Motor::kPluggedDelay.
    /// @throws BugError when @p motor is null or does not match() the set (OpenRocket:
    ///         IllegalArgumentException); callers check match() first.
    void addMotor(std::shared_ptr<const ThrustCurveMotor> motor);

    /// Whether @p motor belongs in this set (matches()): always for an empty set; otherwise the
    /// same Manufacturer object, diameter and length equal within MathUtil's tolerance, types
    /// that agree unless either is UNKNOWN, and designation, common name and case info equal
    /// ignoring case (Java's equalsIgnoreCase).
    [[nodiscard]] bool matches(const ThrustCurveMotor& motor) const;

    /// The motors in display order (getMotors()).
    [[nodiscard]] const std::vector<std::shared_ptr<const ThrustCurveMotor>>& getMotors()
        const noexcept
    {
        return m_motors;
    }

    [[nodiscard]] std::size_t getMotorCount() const noexcept { return m_motors.size(); }

    /// The union of the standard delays of the motors, sorted.
    [[nodiscard]] const std::vector<double>& getDelays() const noexcept { return m_delays; }

    /// The manufacturer, or nullptr while the set is empty.
    [[nodiscard]] const Manufacturer* getManufacturer() const noexcept { return m_manufacturer; }

    [[nodiscard]] const std::string& getCommonName() const noexcept { return m_commonName; }
    [[nodiscard]] const std::string& getDesignation() const noexcept { return m_designation; }

    /// The diameter, or -1 while the set is empty.
    [[nodiscard]] double getDiameter() const noexcept { return m_diameter; }

    /// The length, or -1 while the set is empty.
    [[nodiscard]] double getLength() const noexcept { return m_length; }

    /// The first type other than UNKNOWN among the motors added, or UNKNOWN.
    [[nodiscard]] Motor::Type getType() const noexcept { return m_type; }

    /// The first motor's total impulse estimate rounded as Java's Math.round does.
    [[nodiscard]] std::int64_t getTotalImpulse() const noexcept { return m_totalImpulse; }

    [[nodiscard]] const std::string& getCaseInfo() const noexcept { return m_caseInfo; }

    /// The first motor's availability.
    [[nodiscard]] bool isAvailable() const noexcept { return m_available; }

    /// "ThrustCurveMotorSet[<manufacturer> <designation>, type=<type name>, count=<n>]", with
    /// "null null" for the manufacturer and designation of an empty set, as in OpenRocket.
    [[nodiscard]] std::string toString() const;

    /// The order sets are listed in (compareTo()): manufacturer display name (collated), then
    /// designation (DesignationComparator), then diameter and length (Double.compare).
    /// @throws BugError when either set is empty (OpenRocket: NullPointerException).
    [[nodiscard]] int compareTo(const ThrustCurveMotorSet& other) const;

private:
    [[nodiscard]] bool checkMotorOverwrite(const std::shared_ptr<const ThrustCurveMotor>& motor);
    void               addStandardDelays(const ThrustCurveMotor& motor);
    void               updateType(const ThrustCurveMotor& motor);
    void               checkFirstInsertion(const ThrustCurveMotor& motor);

    std::vector<std::shared_ptr<const ThrustCurveMotor>> m_motors;
    std::vector<double>                                  m_delays;

    const Manufacturer* m_manufacturer{nullptr};
    std::string         m_commonName;
    std::string         m_designation;
    double              m_diameter{-1};
    double              m_length{-1};
    std::int64_t        m_totalImpulse{0};
    Motor::Type         m_type{Motor::Type::UNKNOWN};
    std::string         m_caseInfo;
    bool                m_available{true};
};

}  // namespace QtRocket
