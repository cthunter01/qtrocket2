#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/Motor.h"

namespace QtRocket
{

/// Finds the motor for one a design file names (OpenRocket's file/MotorFinder). The loader of a
/// file asks the finder of its DocumentLoadingContext for every <motor> element; the
/// DatabaseMotorFinder answers from the motor database.
class MotorFinder
{
public:
    virtual ~MotorFinder() = default;

    /// The motor to put into the design for the one a file describes (findMotor()), or null for
    /// no motor. A criterion that is nullopt (Java: null) or NaN is to be ignored in the search.
    /// The finder adds to @p warnings what the user should know about the choice (a motor that
    /// was not found, one chosen of several).
    [[nodiscard]] virtual std::shared_ptr<const Motor> findMotor(
        std::optional<Motor::Type> type, std::optional<std::string_view> manufacturer,
        std::optional<std::string_view> designation, double diameter, double length,
        std::optional<std::string_view> digest, WarningSet& warnings) const = 0;

protected:
    MotorFinder()                              = default;
    MotorFinder(const MotorFinder&)            = default;
    MotorFinder(MotorFinder&&)                 = default;
    MotorFinder& operator=(const MotorFinder&) = default;
    MotorFinder& operator=(MotorFinder&&)      = default;
};

}  // namespace QtRocket
