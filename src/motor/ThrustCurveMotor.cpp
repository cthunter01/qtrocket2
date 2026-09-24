#include "QtRocket/motor/ThrustCurveMotor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/motor/CaseInfo.h"
#include "QtRocket/motor/DesignationComparator.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Inertia.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// SNAP_DISTANCE and SNAP_TOLERANCE: a pseudo-index fraction this close to 0 or 1 snaps to it.
constexpr double kSnapDistance = 0.0001;

[[nodiscard]] bool isDigit(char c) noexcept
{
    return c >= '0' && c <= '9';
}

/// True when @p text holds one of java.util.regex's line terminators (\n, \r, U+0085, U+2028,
/// U+2029), none of which '.' matches.
[[nodiscard]] bool containsLineTerminator(std::string_view text) noexcept
{
    constexpr std::array<std::string_view, 5> kTerminators{"\n", "\r", "\xC2\x85", "\xE2\x80\xA8",
                                                           "\xE2\x80\xA9"};
    return std::ranges::any_of(
        kTerminators, [text](std::string_view terminator) { return text.contains(terminator); });
}

/// SIMPLIFY_PATTERN.matcher(text).matches(), `^[0-9]*[ -]*([A-Z][0-9]+).*`: group 1, or nullopt.
/// The prefix can only be read greedily (the class letter is neither a digit nor ' ' or '-'), and
/// fewer thrust digits would leave the same rest to '.*', so one pass decides.
[[nodiscard]] std::optional<std::string_view> matchSimplifyPattern(std::string_view text) noexcept
{
    std::size_t position = 0;
    while (position < text.size() && isDigit(text[position]))
    {
        position++;
    }
    while (position < text.size() && (text[position] == ' ' || text[position] == '-'))
    {
        position++;
    }
    if (position >= text.size() || text[position] < 'A' || text[position] > 'Z')
    {
        return std::nullopt;
    }
    const std::size_t start = position;
    position++;
    const std::size_t digitsStart = position;
    while (position < text.size() && isDigit(text[position]))
    {
        position++;
    }
    if (position == digitsStart || containsLineTerminator(text.substr(position)))
    {
        return std::nullopt;
    }
    return text.substr(start, position - start);
}

/// Java's Coordinate.toString(): "(x,y,z)", or "(x,y,z,w=weight)" when weighted, each with
/// String.format's "%.5f" (NaN as "NaN"), as the invalid-CG message shows it.
[[nodiscard]] std::string javaCoordinateString(const Coordinate& c)
{
    if (c.isWeighted())
    {
        return std::format("({},{},{},w={})", Strings::formatFixed(c.x, 5),
                           Strings::formatFixed(c.y, 5), Strings::formatFixed(c.z, 5),
                           Strings::formatFixed(c.weight, 5));
    }
    return std::format("({},{},{})", Strings::formatFixed(c.x, 5), Strings::formatFixed(c.y, 5),
                       Strings::formatFixed(c.z, 5));
}

/// Arrays.toString(double[]): "[1.0, 2.5]".
[[nodiscard]] std::string javaArrayString(std::span<const double> values)
{
    std::string out = "[";
    for (std::size_t i = 0; i < values.size(); i++)
    {
        if (i > 0)
        {
            out += ", ";
        }
        out += Strings::javaDoubleToString(values[i]);
    }
    out += "]";
    return out;
}

/// getIndex(): the index of the last time point at or before @p motorTime (0 when none is).
[[nodiscard]] std::size_t getIndex(std::span<const double> time, double motorTime) noexcept
{
    std::size_t lowerBoundIndex = 0;
    std::size_t upperBoundIndex = 0;
    while (upperBoundIndex < time.size() && motorTime >= time[upperBoundIndex])
    {
        lowerBoundIndex = upperBoundIndex;
        ++upperBoundIndex;
    }
    return lowerBoundIndex;
}

/// getIndexFraction(): how far @p motorTime lies from time[index] towards time[index + 1], snapped
/// to 0 or 1 within kSnapDistance; 0 at the last point.
[[nodiscard]] double getIndexFraction(std::span<const double> time, double motorTime,
                                      std::size_t index) noexcept
{
    const std::size_t lowerBoundIndex = index;
    const std::size_t upperBoundIndex = index + 1;

    // we are already at the end of the time array.
    if (upperBoundIndex == time.size())
    {
        return 0.0;
    }

    const double lowerBoundTime = time[lowerBoundIndex];
    const double upperBoundTime = time[upperBoundIndex];
    const double timeFraction   = motorTime - lowerBoundTime;
    const double indexFraction  = timeFraction / (upperBoundTime - lowerBoundTime);

    if (kSnapDistance > indexFraction)
    {
        // round down to previous index
        return 0.0;
    }
    if ((1 - kSnapDistance) < indexFraction)
    {
        // round up to next index
        return 1.0;
    }
    // general case
    return indexFraction;
}

/// interpolateAtIndex(): the value at @p pseudoIndex, linearly between the neighbouring samples, or
/// the sample itself when the fraction is within kSnapDistance of 0 or 1. A NaN index gives NaN,
/// (int) NaN being 0 in Java.
[[nodiscard]] double interpolateAtIndex(std::span<const double> values, double pseudoIndex)
{
    const int lowerIndex = MathUtil::javaIntCast(pseudoIndex);
    const int upperIndex = lowerIndex + 1;

    const double lowerFrac = pseudoIndex - static_cast<double>(lowerIndex);
    const double upperFrac = 1 - lowerFrac;

    const auto lower = static_cast<std::size_t>(lowerIndex);
    const auto upper = static_cast<std::size_t>(upperIndex);
    if (kSnapDistance > lowerFrac)
    {
        // index ~= int ... therefore:
        return values[lower];
    }
    QTROCKET_ASSERT(upper < values.size());
    if (kSnapDistance > upperFrac)
    {
        return values[upper];
    }

    const double lowerValue = values[lower];
    const double upperValue = values[upper];

    // return simple linear inverse interpolation
    return (lowerValue * upperFrac) + (upperValue * lowerFrac);
}

/// The first problem with the shape of the curve, as build() checks it: the array lengths, the
/// number of points, strictly increasing times and a start at time zero.
[[nodiscard]] std::optional<std::string> checkTimes(std::span<const double> time,
                                                    std::span<const double> thrust,
                                                    std::size_t             cgCount)
{
    if (time.size() != thrust.size() || time.size() != cgCount)
    {
        return std::format("Array lengths do not match, time:{} thrust:{} cg:{}", time.size(),
                           thrust.size(), cgCount);
    }
    if (time.size() < 2)
    {
        return std::format("Too short thrust-curve, length={}", time.size());
    }
    for (std::size_t i = 0; i + 1 < time.size(); i++)
    {
        if (time[i + 1] <= time[i])
        {
            return std::format(
                "Two thrust values for single time point, "
                "time[{}]={}, thrust={}; time[{}]={}, "
                "thrust={}",
                i, Strings::javaDoubleToString(time[i]), Strings::javaDoubleToString(thrust[i]),
                i + 1, Strings::javaDoubleToString(time[i + 1]),
                Strings::javaDoubleToString(thrust[i + 1]));
        }
    }
    if (!MathUtil::equals(time[0], 0))
    {
        return "Curve starts at time " + Strings::javaDoubleToString(time[0]);
    }
    return std::nullopt;
}

/// The first thrust that is negative, NaN or above the maximum, as build() reports it.
[[nodiscard]] std::optional<std::string> checkThrust(std::span<const double> thrust)
{
    // A curve that does not start or end at zero thrust is accepted, as in OpenRocket: many
    // thrustcurve.org files have one or the other, and it matters less to the simulation than the
    // normal variation between motors.
    for (const double t : thrust)
    {
        if (t < 0)
        {
            return "Negative thrust.";
        }
        if (t > ThrustCurveMotor::kMaxThrust || std::isnan(t))
        {
            return "Invalid thrust " + Strings::javaDoubleToString(t);
        }
    }
    return std::nullopt;
}

/// The first CG point with a NaN, outside the motor or with a negative mass, as build() reports it.
[[nodiscard]] std::optional<std::string> checkCg(std::span<const Coordinate> cg,
                                                 std::span<const double> time, double length)
{
    for (const Coordinate& c : cg)
    {
        if (c.isNaN())
        {
            return "Invalid CG " + javaCoordinateString(c);
        }
        if (c.x < 0)
        {
            return std::format("Invalid CG position {}: CG is below the start of the motor.",
                               Strings::formatFixed(c.x, 6));
        }
        if (c.x > length)
        {
            return std::format("Invalid CG position: {}: CG is above the end of the motor.",
                               Strings::formatFixed(c.x, 6));
        }
        if (c.weight < 0)
        {
            // Arrays.asList(cg).indexOf(c): the first point equal to this one (within the tolerance
            // of Coordinate's equality), so possibly an earlier one.
            const auto first =
                std::ranges::find_if(cg, [&c](const Coordinate& other) { return other == c; });
            const auto index = static_cast<std::size_t>(std::distance(cg.begin(), first));
            return std::format("Negative mass {}at time={}", Strings::javaDoubleToString(c.weight),
                               Strings::javaDoubleToString(time[index]));
        }
    }
    return std::nullopt;
}

/// The estimates computeStatistics() derives from a curve.
struct Statistics
{
    double maxThrust{0};
    double burnTimeEstimate{0};
    double averageThrust{0};
    double totalImpulse{0};
};

/// The time the thrust first reaches @p thrustLimit, interpolated between the samples around it
/// (their midpoint when their thrusts are equal).
[[nodiscard]] double burnStartTime(std::span<const double> time, std::span<const double> thrust,
                                   double maxThrust, double thrustLimit)
{
    if (thrust[0] >= thrustLimit)
    {
        return time[0];
    }
    std::size_t startPos = 1;
    while (startPos < thrust.size() && thrust[startPos] < thrustLimit)
    {
        startPos++;
    }
    if (startPos >= thrust.size())
    {
        bug(std::format("Could not compute burn start time, maxThrust={} limit={} thrust={}",
                        Strings::javaDoubleToString(maxThrust),
                        Strings::javaDoubleToString(thrustLimit), javaArrayString(thrust)));
    }
    if (MathUtil::equals(thrust[startPos - 1], thrust[startPos]))
    {
        // For safety
        return (time[startPos - 1] + time[startPos]) / 2;
    }
    return MathUtil::map(thrustLimit, thrust[startPos - 1], thrust[startPos], time[startPos - 1],
                         time[startPos]);
}

/// The time the thrust last falls below @p thrustLimit, interpolated as burnStartTime() does.
[[nodiscard]] double burnEndTime(std::span<const double> time, std::span<const double> thrust,
                                 double maxThrust, double thrustLimit)
{
    if (thrust.back() >= thrustLimit)
    {
        return time.back();
    }
    // OpenRocket counts endPos down from size - 2 and marks a failed search with -1; here the
    // search runs over the indices below size - 1 from the top.
    std::size_t endPos = thrust.size() - 1;
    bool        found  = false;
    while (endPos > 0 && !found)
    {
        endPos--;
        found = thrust[endPos] >= thrustLimit;
    }
    if (!found)
    {
        bug(std::format("Could not compute burn end time, maxThrust={} limit={} thrust={}",
                        Strings::javaDoubleToString(maxThrust),
                        Strings::javaDoubleToString(thrustLimit), javaArrayString(thrust)));
    }
    if (MathUtil::equals(thrust[endPos], thrust[endPos + 1]))
    {
        // For safety
        return (time[endPos] + time[endPos + 1]) / 2;
    }
    return MathUtil::map(thrustLimit, thrust[endPos], thrust[endPos + 1], time[endPos],
                         time[endPos + 1]);
}

/// computeStatistics(): the maximum thrust, the burn time between the points where the thrust
/// crosses Motor::kMarginalThrust of the maximum, the total impulse (trapezoidal) and the average
/// thrust over the burn time.
[[nodiscard]] Statistics computeCurveStatistics(std::span<const double> time,
                                                std::span<const double> thrust)
{
    Statistics statistics;

    // Maximum thrust
    for (const double t : thrust)
    {
        statistics.maxThrust = std::max(statistics.maxThrust, t);
    }

    // Burn start and end time
    const double thrustLimit = statistics.maxThrust * Motor::kMarginalThrust;
    const double burnStart   = burnStartTime(time, thrust, statistics.maxThrust, thrustLimit);
    const double burnEnd     = burnEndTime(time, thrust, statistics.maxThrust, thrustLimit);

    // Burn time
    statistics.burnTimeEstimate = std::max(burnEnd - burnStart, 0.0);

    // Total impulse and average thrust
    double averageThrust = 0;
    for (std::size_t impulsePos = 0; impulsePos + 1 < time.size(); impulsePos++)
    {
        const double t0 = time[impulsePos];
        const double t1 = time[impulsePos + 1];
        const double f0 = thrust[impulsePos];
        const double f1 = thrust[impulsePos + 1];

        statistics.totalImpulse += (t1 - t0) * (f0 + f1) / 2;

        if (t0 < burnStart && t1 > burnStart)
        {
            const double fStart = MathUtil::map(burnStart, t0, t1, f0, f1);
            averageThrust += (fStart + f1) / 2 * (t1 - burnStart);
        }
        else if (t0 >= burnStart && t1 <= burnEnd)
        {
            averageThrust += (f0 + f1) / 2 * (t1 - t0);
        }
        else if (t0 < burnEnd && t1 > burnEnd)
        {
            const double fEnd = MathUtil::map(burnEnd, t0, t1, f0, f1);
            averageThrust += (f0 + fEnd) / 2 * (burnEnd - t0);
        }
    }

    statistics.averageThrust =
        statistics.burnTimeEstimate > 0 ? averageThrust / statistics.burnTimeEstimate : 0;
    return statistics;
}

}  // namespace

ThrustCurveMotor::ThrustCurveMotor() : m_manufacturer(&Manufacturer::getManufacturer("Unknown")) { }

// ---- Builder ----

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setCaseInfo(std::string value)
{
    m_motor.m_caseInfo = std::move(value);
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setCGPoints(std::vector<Coordinate> cg)
{
    m_motor.m_cg = std::move(cg);
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setDescription(std::string description)
{
    m_motor.m_description = std::move(description);
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setCode(std::string code)
{
    m_motor.m_code = std::move(code);
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setCommonName(std::string name)
{
    m_motor.m_commonName = std::move(name);
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setDesignation(std::string designation)
{
    m_motor.m_designation = std::move(designation);
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setDiameter(double value)
{
    m_motor.m_diameter = value;
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setDigest(std::string digest)
{
    m_motor.m_digest = std::move(digest);
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setInitialMass(double value)
{
    m_motor.m_initialMass = value;
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setLength(double value)
{
    m_motor.m_length = value;
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setManufacturer(
    const Manufacturer& manufacturer)
{
    m_motor.m_manufacturer = &manufacturer;
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setMotorType(Motor::Type type)
{
    m_motor.m_type = type;
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setPropellantInfo(std::string value)
{
    m_motor.m_propellantInfo = std::move(value);
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setTcMotorId(std::string value)
{
    m_motor.m_tcMotorId = std::move(value);
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setInfoUrl(std::string value)
{
    m_motor.m_infoUrl = std::move(value);
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setDataFiles(std::optional<int> value)
{
    m_motor.m_dataFiles = value;
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setUpdatedOn(std::string value)
{
    m_motor.m_updatedOn = std::move(value);
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setDataSource(std::string value)
{
    m_motor.m_dataSource = std::move(value);
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setSparky(bool value)
{
    m_motor.m_sparky = value;
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setStandardDelays(std::vector<double> delays)
{
    m_motor.m_delays = std::move(delays);
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setThrustPoints(std::vector<double> thrust)
{
    m_motor.m_thrust = std::move(thrust);
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setTimePoints(std::vector<double> time)
{
    m_motor.m_time = std::move(time);
    return *this;
}

ThrustCurveMotor::Builder& ThrustCurveMotor::Builder::setAvailability(bool available)
{
    m_motor.m_available = available;
    return *this;
}

std::string ThrustCurveMotor::Builder::simplifyDesignation(std::string_view designation)
{
    const std::string_view str = Strings::trim(designation);
    if (const std::optional<std::string_view> group = matchSimplifyPattern(str))
    {
        return std::string(*group);
    }
    // str.replaceAll("\\s", ""): Java's \s is space, \t, \n, \x0B, \f and \r
    std::string out;
    out.reserve(str.size());
    for (const char c : str)
    {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\x0B' && c != '\f' && c != '\r')
        {
            out.push_back(c);
        }
    }
    return out;
}

Result<ThrustCurveMotor> ThrustCurveMotor::Builder::build() const
{
    ThrustCurveMotor motor = m_motor;

    // Check argument validity, in OpenRocket's order
    std::optional<std::string> error = checkTimes(motor.m_time, motor.m_thrust, motor.m_cg.size());
    if (!error.has_value())
    {
        error = checkThrust(motor.m_thrust);
    }
    if (!error.has_value())
    {
        error = checkCg(motor.m_cg, motor.m_time, motor.m_length);
    }
    if (!error.has_value() && motor.m_type != Type::SINGLE && motor.m_type != Type::RELOAD &&
        motor.m_type != Type::HYBRID && motor.m_type != Type::UNKNOWN)
    {
        error = std::format("Illegal motor type={}", static_cast<int>(motor.m_type));
    }
    if (error.has_value())
    {
        return fail(ErrorCode::INVALID_ARGUMENT, std::move(*error));
    }

    motor.m_unitRotationalInertia = Inertia::filledCylinderRotational(motor.m_diameter / 2);
    motor.m_unitLongitudinalInertia =
        Inertia::filledCylinderLongitudinal(motor.m_diameter / 2, motor.m_length);

    // Without a designation (a thrust curve read from a file), the motor code stands in.
    if (motor.m_designation.empty())
    {
        motor.m_designation = motor.m_code;
    }

    // Normalize the common name: one of the standard designation form ("B6-0", "B6W", "H128W") is
    // reduced to its letter and digits ("B6", "H128"); others ("RCS 18/20") stay as they are.
    if (!motor.m_commonName.empty())
    {
        if (const std::optional<std::string_view> group = matchSimplifyPattern(motor.m_commonName))
        {
            motor.m_commonName = std::string(*group);
        }
    }
    if (motor.m_commonName.empty())
    {
        motor.m_commonName = simplifyDesignation(motor.m_designation);
    }

    motor.computeStatistics();

    return motor;
}
// ---- ThrustCurveMotor ----

std::optional<CaseInfo> ThrustCurveMotor::getCaseInfoEnum() const noexcept
{
    return parseCaseInfo(m_caseInfo);
}

std::span<const CaseInfo> ThrustCurveMotor::getCompatibleCases() const noexcept
{
    const std::optional<CaseInfo> myCase = getCaseInfoEnum();
    if (!myCase.has_value())
    {
        return {};
    }
    return compatibleCases(*myCase);
}

double ThrustCurveMotor::getPropellantMass() const noexcept
{
    return getLaunchMass() - getBurnoutMass();
}

double ThrustCurveMotor::getPseudoIndex(double motorTime) const
{
    if (m_time.empty() || 0 > motorTime)
    {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const std::size_t lowerIndex = getIndex(m_time, motorTime);
    const double      fraction   = getIndexFraction(m_time, motorTime, lowerIndex);
    return static_cast<double>(lowerIndex) + fraction;
}

double ThrustCurveMotor::getThrust(double motorTime) const
{
    const double pseudoIndex = getPseudoIndex(motorTime);
    return interpolateAtIndex(m_thrust, pseudoIndex);
}

double ThrustCurveMotor::getCMx(double motorTime) const
{
    const double pseudoIndex = getPseudoIndex(motorTime);
    return interpolateCenterOfMassAtIndex(pseudoIndex).x;
}

double ThrustCurveMotor::getTime(double motorTime) const
{
    const double pseudoIndex = getPseudoIndex(motorTime);
    return interpolateAtIndex(m_time, pseudoIndex);
}

double ThrustCurveMotor::getTotalMass(double motorTime) const
{
    const double pseudoIndex = getPseudoIndex(motorTime);
    return interpolateCenterOfMassAtIndex(pseudoIndex).weight;
}

double ThrustCurveMotor::getPropellantMass(double motorTime) const
{
    const double pseudoIndex = getPseudoIndex(motorTime);
    const double totalMass   = interpolateCenterOfMassAtIndex(pseudoIndex).weight;
    return totalMass - getBurnoutMass();
}

Coordinate ThrustCurveMotor::interpolateCenterOfMassAtIndex(double pseudoIndex) const
{
    // Java's pseudoIndex % 1 is fmod: the fraction, with the sign of the index (NaN for NaN).
    const double upperFrac  = std::fmod(pseudoIndex, 1.0);
    const double lowerFrac  = 1 - upperFrac;
    const int    lowerIndex = MathUtil::javaIntCast(pseudoIndex);
    const int    upperIndex = lowerIndex + 1;

    const auto lower = static_cast<std::size_t>(lowerIndex);
    const auto upper = static_cast<std::size_t>(upperIndex);

    // if the pseudo index is close to an integer (OpenRocket tests the same fraction twice, so the
    // second branch only catches rounding in 1 - (1 - upperFrac)):
    if (kSnapDistance > (1 - lowerFrac))
    {
        return m_cg[lower];
    }
    QTROCKET_ASSERT(upper < m_cg.size());
    if (kSnapDistance > upperFrac)
    {
        return m_cg[upper];
    }

    // return simple linear interpolation
    const Coordinate lowerValue = m_cg[lower].multiply(lowerFrac);
    const Coordinate upperValue = m_cg[upper].multiply(upperFrac);

    return lowerValue.add(upperValue);
}

void ThrustCurveMotor::computeStatistics()
{
    const Statistics statistics = computeCurveStatistics(m_time, m_thrust);
    m_maxThrust                 = statistics.maxThrust;
    m_burnTimeEstimate          = statistics.burnTimeEstimate;
    m_averageThrust             = statistics.averageThrust;
    m_totalImpulse              = statistics.totalImpulse;
}
std::string ThrustCurveMotor::getCommonName(double delay) const
{
    return m_commonName + "-" + getDelayString(delay);
}

std::string ThrustCurveMotor::getDesignation(double delay) const
{
    return m_designation + "-" + getDelayString(delay);
}

int ThrustCurveMotor::compareTo(const ThrustCurveMotor& other) const
{
    // 1. Manufacturer
    int value = Strings::javaPrimaryCollatorCompare(m_manufacturer->getDisplayName(),
                                                    other.m_manufacturer->getDisplayName());
    if (value != 0)
    {
        return value;
    }

    // 2. Designation
    value = DesignationComparator::compare(getDesignation(), other.getDesignation());
    if (value != 0)
    {
        return value;
    }

    // 3. Diameter
    value = MathUtil::javaIntCast((getDiameter() - other.getDiameter()) * 1000000);
    if (value != 0)
    {
        return value;
    }

    // 4. Length
    return MathUtil::javaIntCast((getLength() - other.getLength()) * 1000000);
}

std::string ThrustCurveMotor::toString() const
{
    return std::format("ThrustCurveMotor[{} {}, digest={}]", m_manufacturer->getDisplayName(),
                       m_designation, m_digest);
}

std::string ThrustCurveMotor::getDelayString(double delay, std::string_view plugged)
{
    if (delay == kPluggedDelay)
    {
        return std::string(plugged);
    }
    // Math.rint rounds half to even, as std::nearbyint does in the default rounding mode.
    delay = std::nearbyint(delay * 10) / 10;
    if (MathUtil::equals(delay, std::nearbyint(delay)))
    {
        return std::format("{}", MathUtil::javaIntCast(delay));
    }
    return Strings::javaDoubleToString(delay);
}

}  // namespace QtRocket
