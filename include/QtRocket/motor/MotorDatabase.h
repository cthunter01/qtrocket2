#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "QtRocket/motor/Motor.h"

namespace QtRocket
{

/// A searchable collection of motors (OpenRocket's MotorDatabase). The motors are immutable and
/// shared: every search returns the database's own instances.
class MotorDatabase
{
public:
    virtual ~MotorDatabase() = default;

    /// Every motor matching the criteria. A criterion that is nullopt (OpenRocket: null) or a NaN
    /// @p diameter or @p length is ignored; see ThrustCurveMotorSetDatabase for how the criteria
    /// combine.
    [[nodiscard]] virtual std::vector<std::shared_ptr<const Motor>> findMotors(
        std::optional<std::string_view> digest, std::optional<Motor::Type> type,
        std::optional<std::string_view> manufacturer, std::optional<std::string_view> designation,
        double diameter, double length) const = 0;

protected:
    MotorDatabase()                                = default;
    MotorDatabase(const MotorDatabase&)            = default;
    MotorDatabase(MotorDatabase&&)                 = default;
    MotorDatabase& operator=(const MotorDatabase&) = default;
    MotorDatabase& operator=(MotorDatabase&&)      = default;
};

}  // namespace QtRocket
