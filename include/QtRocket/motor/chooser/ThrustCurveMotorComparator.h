#pragma once

#include <cstdint>
#include <memory>

namespace QtRocket
{

class ThrustCurveMotor;

/// Orders the thrust curves of a motor set by how good they look, the best first (OpenRocket's
/// swing ThrustCurveMotorComparator, gui/dialogs/motor/thrustcurve/): the motor chooser's curve
/// list is sorted with it, and with no preferred curve its first curve is the one the chooser
/// selects (ThrustCurveMotorSelection). "10 chars of comments correspond to one thrust point, max
/// ten points."
///
/// Java's int arithmetic cannot overflow here for a curve that fits in memory; the goodness is
/// computed in 64 bits, and compare() gives the same answers.
class ThrustCurveMotorComparator
{
public:
    /// calculateGoodness(): ten times the number of time points plus the length of the
    /// description in UTF-16 code units (String.length(), Strings::javaLength()), at most 100.
    [[nodiscard]] static std::int64_t calculateGoodness(const ThrustCurveMotor& motor);

    /// compare(o1, o2): calculateGoodness(o2) - calculateGoodness(o1), negative when @p o1 is the
    /// better curve (narrowed to the int range).
    [[nodiscard]] static int compare(const ThrustCurveMotor& o1, const ThrustCurveMotor& o2);

    /// compare(*a, *b) < 0, for a std::ranges::stable_sort (Java's List.sort is stable: curves of
    /// equal goodness keep their order).
    [[nodiscard]] bool operator()(const std::shared_ptr<const ThrustCurveMotor>& a,
                                  const std::shared_ptr<const ThrustCurveMotor>& b) const;
};

}  // namespace QtRocket
