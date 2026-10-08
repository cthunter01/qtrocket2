#include "QtRocket/file/DatabaseMotorFinder.h"

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorDatabase.h"
#include "QtRocket/motor/MotorDigest.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// A criterion as the warning keeps it: a copy of the text, or none.
[[nodiscard]] std::optional<std::string> copyOf(std::optional<std::string_view> text)
{
    if (!text.has_value())
    {
        return std::nullopt;
    }
    return std::string(*text);
}

/// The type as the warning keeps it: the name of the enum constant, or none.
[[nodiscard]] std::optional<std::string> nameOf(std::optional<Motor::Type> type)
{
    if (!type.has_value())
    {
        return std::nullopt;
    }
    return std::string(enumName(*type));
}

}  // namespace

DatabaseMotorFinder::DatabaseMotorFinder(const MotorDatabase& database) noexcept
  : m_database(&database)
{
}

std::shared_ptr<const Motor> DatabaseMotorFinder::handleMissingMotor(
    std::optional<Motor::Type> type, std::optional<std::string_view> manufacturer,
    std::string_view designation, double diameter, double length,
    std::optional<std::string_view> digest, WarningSet& warnings) const
{
    Warning::MissingMotor mmw;
    mmw.setDesignation(std::string(designation));
    mmw.setDigest(copyOf(digest));
    mmw.setDiameter(diameter);
    mmw.setLength(length);
    mmw.setManufacturer(copyOf(manufacturer));
    mmw.setType(nameOf(type));
    warnings.add(mmw);
    return nullptr;
}

std::shared_ptr<const Motor> DatabaseMotorFinder::findMotor(
    std::optional<Motor::Type> type, std::optional<std::string_view> manufacturer,
    std::optional<std::string_view> designation, double diameter, double length,
    std::optional<std::string_view> digest, WarningSet& warnings) const
{
    if (!designation.has_value())
    {
        warnings.add(Warning::fromString("No motor specified, ignoring."));
        return nullptr;
    }

    const std::vector<std::shared_ptr<const Motor>> motors =
        m_database->findMotors(digest, type, manufacturer, designation, diameter, length);

    // No motors
    if (motors.empty())
    {
        return handleMissingMotor(type, manufacturer, *designation, diameter, length, digest,
                                  warnings);
    }

    // One motor
    if (motors.size() == 1)
    {
        return motors.front();
    }

    // Multiple motors, check digest for which one to use
    if (digest.has_value())
    {
        // Prefer a motor with a compatible digest (historical digests included).
        for (const std::shared_ptr<const Motor>& m : motors)
        {
            if (MotorDigest::isDigestCompatible(*m, *digest))
            {
                return m;
            }
        }

        // Fall back to an exact designation match if possible (e.g. prefer "B6" over "B6-0").
        for (const std::shared_ptr<const Motor>& m : motors)
        {
            if (Strings::javaEqualsIgnoreCase(m->getDesignation(), *designation))
            {
                return m;
            }
        }
    }
    else
    {
        std::string str = std::format("Multiple motors with designation '{}'", *designation);
        if (manufacturer.has_value())
        {
            str += std::format(" for manufacturer '{}'", *manufacturer);
        }
        str += " found, one chosen arbitrarily.";
        warnings.add(str);
    }
    return motors.front();
}

}  // namespace QtRocket
