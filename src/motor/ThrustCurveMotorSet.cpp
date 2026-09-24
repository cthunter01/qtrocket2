#include "QtRocket/motor/ThrustCurveMotorSet.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <utility>

#include "QtRocket/motor/DesignationComparator.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// getFormattedDescription(): the description with whitespace runs collapsed, trimmed.
[[nodiscard]] std::string formattedDescription(const Motor& motor)
{
    return Strings::collapseWhitespace(motor.getDescription());
}

/// ThrustCurveMotorComparator: the order motors of a set are displayed in (negative when @p a
/// comes first).
[[nodiscard]] int compareForDisplay(const ThrustCurveMotor& a, const ThrustCurveMotor& b)
{
    // 1. Designation
    if (a.getDesignation() != b.getDesignation())
    {
        return Strings::javaCompareTo(a.getDesignation(), b.getDesignation());
    }

    // 2. Number of data points (more is better)
    if (a.getSampleSize() != b.getSampleSize())
    {
        return b.getSampleSize() > a.getSampleSize() ? 1 : -1;
    }

    // 3. Comment length (longer is better), in UTF-16 code units as Java's String.length()
    const std::size_t lengthA = Strings::javaLength(a.getDescription());
    const std::size_t lengthB = Strings::javaLength(b.getDescription());
    if (lengthA == lengthB)
    {
        return 0;
    }
    return lengthB > lengthA ? 1 : -1;
}

}  // namespace

void ThrustCurveMotorSet::addMotor(std::shared_ptr<const ThrustCurveMotor> motor)
{
    QTROCKET_ASSERT(motor != nullptr);

    checkFirstInsertion(*motor);
    if (!matches(*motor))
    {
        bug(std::format(
            "Motor does not match the set: manufacturer={} designation={} "
            "diameter={} length={} set_size={} motor={}",
            m_manufacturer->toString(), m_designation, Strings::javaDoubleToString(m_diameter),
            Strings::javaDoubleToString(m_length), m_motors.size(), motor->toString()));
    }
    updateType(*motor);
    addStandardDelays(*motor);
    if (!checkMotorOverwrite(motor))
    {
        m_motors.push_back(std::move(motor));
        // Java's List.sort is a stable merge sort.
        std::ranges::stable_sort(
            m_motors, [](const auto& a, const auto& b) { return compareForDisplay(*a, *b) < 0; });
    }
}

bool ThrustCurveMotorSet::checkMotorOverwrite(const std::shared_ptr<const ThrustCurveMotor>& motor)
{
    const std::string& digest = motor->getDigest();
    for (std::shared_ptr<const ThrustCurveMotor>& present : m_motors)
    {
        // isMotorPresent(): OpenRocket keeps each motor's digest in an identity map, which always
        // holds the motor's own digest.
        if (digest == present->getDigest() && motor->getDesignation() == present->getDesignation())
        {
            // Match found, check which one to keep (or both) based on comment
            const std::string newComment = formattedDescription(*motor);
            const std::string oldComment = formattedDescription(*present);
            if (newComment.empty() || newComment == oldComment)
            {
                return true;
            }
            if (oldComment.empty())
            {
                present = motor;
                return true;
            }
            // else continue search and add both
        }
    }
    return false;
}

void ThrustCurveMotorSet::addStandardDelays(const ThrustCurveMotor& motor)
{
    for (const double delay : motor.getStandardDelays())
    {
        // Math.rint, half to even; List.contains compares as Double.equals (NaN equals NaN,
        // -0.0 differs from 0.0), which is Double.compare giving 0.
        const double d       = std::nearbyint(delay);
        const bool   present = std::ranges::any_of(
            m_delays, [d](double x) { return MathUtil::javaDoubleCompare(x, d) == 0; });
        if (!present)
        {
            m_delays.push_back(d);
        }
    }
    std::ranges::stable_sort(
        m_delays, [](double a, double b) { return MathUtil::javaDoubleCompare(a, b) < 0; });
}

void ThrustCurveMotorSet::updateType(const ThrustCurveMotor& motor)
{
    // Update the type if now known
    if (m_type == Motor::Type::UNKNOWN)
    {
        m_type = motor.getMotorType();
        // Add "Plugged" option if hybrid
        if (m_type == Motor::Type::HYBRID)
        {
            const bool present = std::ranges::any_of(m_delays, [](double x) {
                return MathUtil::javaDoubleCompare(x, Motor::kPluggedDelay) == 0;
            });
            if (!present)
            {
                m_delays.push_back(Motor::kPluggedDelay);
            }
        }
    }
}

void ThrustCurveMotorSet::checkFirstInsertion(const ThrustCurveMotor& motor)
{
    if (m_motors.empty())
    {
        m_manufacturer = &motor.getManufacturer();
        m_designation  = motor.getDesignation();
        m_commonName   = motor.getCommonName();
        m_diameter     = motor.getDiameter();
        m_length       = motor.getLength();
        m_totalImpulse = MathUtil::javaRound(motor.getTotalImpulseEstimate());
        m_caseInfo     = motor.getCaseInfo();
        m_available    = motor.isAvailable();
    }
}

bool ThrustCurveMotorSet::matches(const ThrustCurveMotor& motor) const
{
    if (m_motors.empty())
    {
        return true;
    }

    if (m_manufacturer != &motor.getManufacturer())
    {
        return false;
    }

    if (!MathUtil::equals(m_diameter, motor.getDiameter()))
    {
        return false;
    }

    if (!MathUtil::equals(m_length, motor.getLength()))
    {
        return false;
    }

    if (m_type != Motor::Type::UNKNOWN && motor.getMotorType() != Motor::Type::UNKNOWN &&
        m_type != motor.getMotorType())
    {
        return false;
    }

    if (!Strings::javaEqualsIgnoreCase(m_designation, motor.getDesignation()))
    {
        return false;
    }

    if (!Strings::javaEqualsIgnoreCase(m_commonName, motor.getCommonName()))
    {
        return false;
    }

    return Strings::javaEqualsIgnoreCase(m_caseInfo, motor.getCaseInfo());
}

std::string ThrustCurveMotorSet::toString() const
{
    const bool empty = m_manufacturer == nullptr;
    return std::format("ThrustCurveMotorSet[{} {}, type={}, count={}]",
                       empty ? std::string("null") : m_manufacturer->toString(),
                       empty ? std::string("null") : m_designation, name(m_type), m_motors.size());
}

int ThrustCurveMotorSet::compareTo(const ThrustCurveMotorSet& other) const
{
    QTROCKET_ASSERT(m_manufacturer != nullptr && other.m_manufacturer != nullptr);

    // 1. Manufacturer
    int value = Strings::javaPrimaryCollatorCompare(m_manufacturer->getDisplayName(),
                                                    other.m_manufacturer->getDisplayName());
    if (value != 0)
    {
        return value;
    }

    // 2. Designation
    value = DesignationComparator::compare(m_designation, other.m_designation);
    if (value != 0)
    {
        return value;
    }

    // 3. Diameter
    value = MathUtil::javaDoubleCompare(m_diameter, other.m_diameter);
    if (value != 0)
    {
        return value;
    }

    // 4. Length
    return MathUtil::javaDoubleCompare(m_length, other.m_length);
}

}  // namespace QtRocket
