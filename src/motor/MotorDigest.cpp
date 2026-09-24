#include "QtRocket/motor/MotorDigest.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Md5.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

[[nodiscard]] std::string_view typeName(MotorDigest::DataType type) noexcept
{
    switch (type)
    {
        case MotorDigest::DataType::TIME_ARRAY:
            return "TIME_ARRAY";
        case MotorDigest::DataType::MASS_SPECIFIC:
            return "MASS_SPECIFIC";
        case MotorDigest::DataType::MASS_PER_TIME:
            return "MASS_PER_TIME";
        case MotorDigest::DataType::CG_SPECIFIC:
            return "CG_SPECIFIC";
        case MotorDigest::DataType::CG_PER_TIME:
            return "CG_PER_TIME";
        case MotorDigest::DataType::FORCE_PER_TIME:
            return "FORCE_PER_TIME";
    }
    return "?";
}

/// v + signum(v) * EPSILON: moves a value a little away from zero (NaN and zeros unchanged).
[[nodiscard]] double next(double v) noexcept
{
    return v + (MathUtil::signum(v) * MotorDigest::kEpsilon);
}

/// `(int) Math.round(next(next(v) * multiplier))`: Math.round gives a long (saturating at its
/// range, 0 for NaN) and the (int) cast keeps its low 32 bits.
[[nodiscard]] std::int32_t roundValue(double v, int multiplier) noexcept
{
    v = next(v);
    v *= multiplier;
    v = next(v);
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(MathUtil::javaRound(v)));
}

/// The x of each CG point and the weight (mass) of each.
struct CgArrays
{
    std::vector<double> cgx;
    std::vector<double> mass;
};

[[nodiscard]] CgArrays splitCgPoints(const ThrustCurveMotor& motor)
{
    CgArrays arrays;
    arrays.cgx.reserve(motor.getCGPoints().size());
    arrays.mass.reserve(motor.getCGPoints().size());
    for (const Coordinate& cg : motor.getCGPoints())
    {
        arrays.cgx.push_back(cg.x);
        arrays.mass.push_back(cg.weight);
    }
    return arrays;
}

}  // namespace

int MotorDigest::getOrder(DataType type) noexcept
{
    switch (type)
    {
        case DataType::TIME_ARRAY:
            return 0;
        case DataType::MASS_SPECIFIC:
            return 1;
        case DataType::MASS_PER_TIME:
            return 2;
        case DataType::CG_SPECIFIC:
            return 3;
        case DataType::CG_PER_TIME:
            return 4;
        case DataType::FORCE_PER_TIME:
            return 5;
    }
    return -1;
}

int MotorDigest::getMultiplier(DataType type) noexcept
{
    switch (type)
    {
        case DataType::MASS_SPECIFIC:
        case DataType::MASS_PER_TIME:
            return 10000;  // 0.1 g
        case DataType::TIME_ARRAY:
        case DataType::CG_SPECIFIC:
        case DataType::CG_PER_TIME:
        case DataType::FORCE_PER_TIME:
            return 1000;  // ms, mm, mN
    }
    return 1;
}

void MotorDigest::update(DataType type, std::span<const double> values)
{
    const int                 multiplier = getMultiplier(type);
    std::vector<std::int32_t> intValues;
    intValues.reserve(values.size());
    for (const double v : values)
    {
        intValues.push_back(roundValue(v, multiplier));
    }
    updateRounded(type, intValues);
}

void MotorDigest::update(DataType type, std::initializer_list<double> values)
{
    update(type, std::span<const double>(values.begin(), values.size()));
}

void MotorDigest::updateRounded(DataType type, std::span<const std::int32_t> values)
{
    // Check for correct order
    if (m_lastOrder >= getOrder(type))
    {
        bug(std::format("Called with type={} order={} while lastOrder={}", typeName(type),
                        getOrder(type), m_lastOrder));
    }
    m_lastOrder = getOrder(type);

    // Digest the type
    updateInt(getOrder(type));

    // Digest the data length (a Java array length, an int)
    updateInt(static_cast<std::int32_t>(values.size()));

    // Digest the values
    for (const std::int32_t v : values)
    {
        updateInt(v);
    }
}

void MotorDigest::updateInt(std::int32_t value) noexcept
{
    const auto                     bits = static_cast<std::uint32_t>(value);
    const std::array<std::byte, 4> bytes{static_cast<std::byte>((bits >> 24U) & 0xFFU),
                                         static_cast<std::byte>((bits >> 16U) & 0xFFU),
                                         static_cast<std::byte>((bits >> 8U) & 0xFFU),
                                         static_cast<std::byte>(bits & 0xFFU)};
    m_digest.update(bytes);
}

std::string MotorDigest::getDigest()
{
    if (m_used)
    {
        bug("MotorDigest already used");
    }
    m_used                   = true;
    const Md5::Digest result = m_digest.finish();
    return Strings::hexString(result);
}

std::string MotorDigest::digestMotor(const ThrustCurveMotor& motor)
{
    // Create the motor digest from data available in RASP files
    MotorDigest motorDigest;
    motorDigest.update(DataType::TIME_ARRAY, motor.getTimePoints());

    const CgArrays cg = splitCgPoints(motor);
    motorDigest.update(DataType::MASS_PER_TIME, cg.mass);
    motorDigest.update(DataType::CG_PER_TIME, cg.cgx);
    motorDigest.update(DataType::FORCE_PER_TIME, motor.getThrustPoints());
    return motorDigest.getDigest();
}

bool MotorDigest::isDigestCompatible(const Motor& motor, std::string_view digest)
{
    if (digest == motor.getDigest())
    {
        return true;
    }

    const auto* thrustCurveMotor = dynamic_cast<const ThrustCurveMotor*>(&motor);
    if (thrustCurveMotor == nullptr)
    {
        return false;
    }

    const std::vector<double>& timePoints   = thrustCurveMotor->getTimePoints();
    const std::vector<double>& thrustPoints = thrustCurveMotor->getThrustPoints();
    const CgArrays             cg           = splitCgPoints(*thrustCurveMotor);
    const double               launchMass   = thrustCurveMotor->getLaunchMass();
    const double               burnoutMass  = thrustCurveMotor->getBurnoutMass();

    // RASP and automatically calculated RSE mass use endpoint masses.
    MotorDigest massSpecific;
    massSpecific.update(DataType::TIME_ARRAY, timePoints);
    massSpecific.update(DataType::MASS_SPECIFIC, {launchMass, burnoutMass});
    massSpecific.update(DataType::FORCE_PER_TIME, thrustPoints);
    if (digest == massSpecific.getDigest())
    {
        return true;
    }

    // Some RSE files provide CG samples while asking the loader to calculate mass.
    MotorDigest massSpecificAndCg;
    massSpecificAndCg.update(DataType::TIME_ARRAY, timePoints);
    massSpecificAndCg.update(DataType::MASS_SPECIFIC, {launchMass, burnoutMass});
    massSpecificAndCg.update(DataType::CG_PER_TIME, cg.cgx);
    massSpecificAndCg.update(DataType::FORCE_PER_TIME, thrustPoints);
    if (digest == massSpecificAndCg.getDigest())
    {
        return true;
    }

    // Thrust-only digests remain stable if mass and CG modeling changes.
    MotorDigest thrustOnly;
    thrustOnly.update(DataType::TIME_ARRAY, timePoints);
    thrustOnly.update(DataType::FORCE_PER_TIME, thrustPoints);
    if (digest == thrustOnly.getDigest())
    {
        return true;
    }

    MotorDigest massPerTime;
    massPerTime.update(DataType::TIME_ARRAY, timePoints);
    massPerTime.update(DataType::MASS_PER_TIME, cg.mass);
    massPerTime.update(DataType::FORCE_PER_TIME, thrustPoints);
    if (digest == massPerTime.getDigest())
    {
        return true;
    }

    MotorDigest cgPerTime;
    cgPerTime.update(DataType::TIME_ARRAY, timePoints);
    cgPerTime.update(DataType::CG_PER_TIME, cg.cgx);
    cgPerTime.update(DataType::FORCE_PER_TIME, thrustPoints);
    return digest == cgPerTime.getDigest();
}

std::string MotorDigest::digestComment(std::string_view comment)
{
    const std::string                normalized = Strings::collapseWhitespace(comment);
    const std::span<const std::byte> bytes      = std::as_bytes(std::span(normalized));
    return Strings::hexString(md5(bytes));
}

}  // namespace QtRocket
