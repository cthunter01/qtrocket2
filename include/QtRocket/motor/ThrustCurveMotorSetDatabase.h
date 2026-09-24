#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorDatabase.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"

namespace QtRocket
{

class ThrustCurveMotor;

/// The motor database: ThrustCurveMotorSets, each grouping the curves of one motor, in the order
/// they were created (OpenRocket's ThrustCurveMotorSetDatabase). It is filled once while loading
/// and only read afterwards, so a loaded database can be shared between threads.
class ThrustCurveMotorSetDatabase final : public MotorDatabase
{
public:
    /// findThrustCurveMotors() as the MotorDatabase interface returns it.
    [[nodiscard]] std::vector<std::shared_ptr<const Motor>> findMotors(
        std::optional<std::string_view> digest, std::optional<Motor::Type> type,
        std::optional<std::string_view> manufacturer, std::optional<std::string_view> designation,
        double diameter, double length) const override;

    /// The motors matching the criteria (OpenRocket's findMotors()), the sets and each set's
    /// motors in order. A motor matches the digest when @p digest is given and equal to its own.
    /// It matches the description when, for every criterion given (a nullopt or NaN one is
    /// ignored), its set's type is @p type; its manufacturer matches() @p manufacturer; its
    /// designation contains @p designation or @p designation contains its common name, both
    /// upper-cased (ASCII letters only; Java's toUpperCase also folds other letters); and its
    /// diameter and length are each within 5 mm of the one given. The result is the motors that
    /// match both if there are any, else those that match the digest if there are any, else those
    /// that match the description.
    [[nodiscard]] std::vector<std::shared_ptr<const ThrustCurveMotor>> findThrustCurveMotors(
        std::optional<std::string_view> digest, std::optional<Motor::Type> type,
        std::optional<std::string_view> manufacturer, std::optional<std::string_view> designation,
        double diameter, double length) const;

    /// Every motor set (getMotorSets()).
    [[nodiscard]] const std::vector<ThrustCurveMotorSet>& getMotorSets() const noexcept
    {
        return m_motorSets;
    }

    /// Adds @p motor to the last-created set that matches() it (searching from the last, since
    /// motors of one file tend to arrive together), or to a new set at the end.
    /// @throws BugError when @p motor is null.
    void addMotor(std::shared_ptr<const ThrustCurveMotor> motor);

private:
    std::vector<ThrustCurveMotorSet> m_motorSets;
};

}  // namespace QtRocket
