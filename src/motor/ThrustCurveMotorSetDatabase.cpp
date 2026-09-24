#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"

#include <cmath>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The largest difference in diameter or length, in m, that still matches.
constexpr double kDimensionTolerance = 0.005;

/// True when a @p wanted dimension is given (not NaN) and @p actual is more than the tolerance
/// away from it; written as OpenRocket tests it, so that a NaN @p actual never differs.
[[nodiscard]] bool differs(double wanted, double actual) noexcept
{
    return !std::isnan(wanted) && std::abs(wanted - actual) > kDimensionTolerance;
}

/// Whether @p motor of @p set matches the descriptive criteria of findMotors().
[[nodiscard]] bool matchesDescription(const ThrustCurveMotorSet& set, const ThrustCurveMotor& motor,
                                      std::optional<Motor::Type>      type,
                                      std::optional<std::string_view> manufacturer,
                                      std::optional<std::string_view> designation, double diameter,
                                      double length)
{
    if (type.has_value() && *type != set.getType())
    {
        return false;
    }
    if (manufacturer.has_value() && !motor.getManufacturer().matches(*manufacturer))
    {
        return false;
    }
    if (designation.has_value())
    {
        const std::string wanted = Strings::toUpper(*designation);
        if (!Strings::toUpper(motor.getDesignation()).contains(wanted) &&
            !wanted.contains(Strings::toUpper(motor.getCommonName())))
        {
            return false;
        }
    }
    if (differs(diameter, motor.getDiameter()))
    {
        return false;
    }
    return !differs(length, motor.getLength());
}

}  // namespace

std::vector<std::shared_ptr<const Motor>> ThrustCurveMotorSetDatabase::findMotors(
    std::optional<std::string_view> digest, std::optional<Motor::Type> type,
    std::optional<std::string_view> manufacturer, std::optional<std::string_view> designation,
    double diameter, double length) const
{
    const std::vector<std::shared_ptr<const ThrustCurveMotor>> found =
        findThrustCurveMotors(digest, type, manufacturer, designation, diameter, length);
    return {found.begin(), found.end()};
}

std::vector<std::shared_ptr<const ThrustCurveMotor>>
ThrustCurveMotorSetDatabase::findThrustCurveMotors(std::optional<std::string_view> digest,
                                                   std::optional<Motor::Type>      type,
                                                   std::optional<std::string_view> manufacturer,
                                                   std::optional<std::string_view> designation,
                                                   double diameter, double length) const
{
    std::vector<std::shared_ptr<const ThrustCurveMotor>> fullMatches;
    std::vector<std::shared_ptr<const ThrustCurveMotor>> digestMatches;
    std::vector<std::shared_ptr<const ThrustCurveMotor>> descriptionMatches;

    // Apply the filters to every motor; the most restrictive nonempty list is returned, or an
    // empty list when nothing matches at all.
    for (const ThrustCurveMotorSet& set : m_motorSets)
    {
        for (const std::shared_ptr<const ThrustCurveMotor>& motor : set.getMotors())
        {
            // unlike the description, digest must be present in search criteria to get a match
            const bool matchDigest = digest.has_value() && *digest == motor->getDigest();
            const bool matchDescription =
                matchesDescription(set, *motor, type, manufacturer, designation, diameter, length);

            if (matchDigest)
            {
                digestMatches.push_back(motor);
            }
            if (matchDescription)
            {
                descriptionMatches.push_back(motor);
            }
            if (matchDigest && matchDescription)
            {
                fullMatches.push_back(motor);
            }
        }
    }

    if (!fullMatches.empty())
    {
        return fullMatches;
    }
    if (!digestMatches.empty())
    {
        return digestMatches;
    }
    return descriptionMatches;
}

void ThrustCurveMotorSetDatabase::addMotor(std::shared_ptr<const ThrustCurveMotor> motor)
{
    QTROCKET_ASSERT(motor != nullptr);

    // Iterate from last to first, as this is most likely to hit early when loading files
    for (ThrustCurveMotorSet& set : std::views::reverse(m_motorSets))
    {
        if (set.matches(*motor))
        {
            set.addMotor(std::move(motor));
            return;
        }
    }

    ThrustCurveMotorSet newSet;
    newSet.addMotor(std::move(motor));
    m_motorSets.push_back(std::move(newSet));
}

}  // namespace QtRocket
