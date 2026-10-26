#pragma once

#include <cstddef>
#include <memory>
#include <string>

namespace QtRocket
{

class Preferences;
class ThrustCurveMotor;

/// An item of the motor chooser's "Select thrust curve:" list (OpenRocket's swing MotorHolder,
/// gui/dialogs/motor/thrustcurve/MotorHolder.java): a thrust curve and its position in the list,
/// which picks its colour (the plot colour of that index) in the list and in the thrust curve plot.
/// ThrustCurveMotorSelection::getCurveItems() makes them.
class MotorHolder
{
public:
    /// @throws BugError when @p motor is null.
    MotorHolder(std::shared_ptr<const ThrustCurveMotor> motor, std::size_t index);

    [[nodiscard]] const std::shared_ptr<const ThrustCurveMotor>& getMotor() const noexcept
    {
        return m_motor;
    }

    /// The position in the list (getIndex()).
    [[nodiscard]] std::size_t getIndex() const noexcept { return m_index; }

    /// The item's text (toString()): the motor's name as the name column shows it, its designation
    /// or its common name (Motor::getMotorName(), Preferences::getMotorNameColumn()).
    [[nodiscard]] std::string toString(const Preferences& preferences) const;

    /// equals(): the same motor (by identity, as OpenRocket's motors have no equals()); the index
    /// is not compared.
    [[nodiscard]] bool operator==(const MotorHolder& other) const noexcept
    {
        return m_motor == other.m_motor;
    }

private:
    std::shared_ptr<const ThrustCurveMotor> m_motor;
    std::size_t                             m_index;
};

}  // namespace QtRocket
