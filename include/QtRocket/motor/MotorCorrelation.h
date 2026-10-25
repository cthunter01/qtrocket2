#pragma once

namespace QtRocket
{

class Motor;

/// How alike the thrust curves of two motors are (OpenRocket's utils/MotorCorrelation). The motor
/// chooser's "Hide very similar thrust curves" hides every other curve of the selected motor set
/// whose similarity() with the selected one is not below 0.95
/// (ThrustCurveMotorSelectionPanel.MOTOR_SIMILARITY_THRESHOLD, part of the chooser's curve choice).
///
/// Both functions sample the curves as OpenRocket does: Motor::getThrust() at the times 0, 0.01,
/// 0.02, ... below 1000 s, the time accumulated by repeated addition of 0.01 (100,001 samples of
/// each motor), and the sums formed in Java's order. Since only + - * / are involved, the results
/// equal OpenRocket's to the last bit for motors whose thrust does (every ThrustCurveMotor:
/// tests/core/motor/MotorCorrelationTests.cpp pins the 322 ordered pairs of curves of OpenRocket's
/// motor database). Both are symmetric, also bit for bit: similarity(a, b) == similarity(b, a).
///
/// The cost is that of the 200,002 getThrust() calls, each a search of the curve's time points:
/// some 30 ms per pair in a debug build (OpenRocket: 1.4 ms), so a caller that asks for the same
/// pair again (the chooser at every selection) keeps the results; thanks to the symmetry one
/// result serves both orders.
namespace MotorCorrelation
{

/// A measure of how similar @p motor1 and @p motor2 are, between 0.0 and 1.0 (1.0 for identical
/// thrust curves): the minimum of their crossCorrelation(), the ratio of the smaller to the larger
/// of their average thrust estimates and of their total impulse estimates, and twice that ratio of
/// their burn time estimates. A ratio is 1.0 when the larger value is zero (within
/// MathUtil::kEpsilon, MathUtil::equals()). The minimum is Java's Math.min, so a NaN estimate
/// makes the similarity NaN (which is not below any threshold).
/// @throws BugError as crossCorrelation() does.
[[nodiscard]] double similarity(const Motor& motor1, const Motor& motor2);

/// The scaled cross-correlation of the thrust curves of @p motor1 and @p motor2: the sum of the
/// products of their thrusts at the sample times, divided by the larger of the two sums of
/// squares, a value between 0.0 and 1.0 (inclusive) that is closer to 1.0 the more alike the
/// curves are. 1.0 when the larger sum of squares is zero (within MathUtil::kEpsilon): two curves
/// without thrust correlate fully. A NaN thrust makes the result NaN.
/// @throws BugError when a thrust is negative ("Negative thrust, t1=<thrust1> t2=<thrust2>", the
///         thrusts as Java's Double.toString writes them, at the first sample time with one).
[[nodiscard]] double crossCorrelation(const Motor& motor1, const Motor& motor2);

}  // namespace MotorCorrelation

}  // namespace QtRocket
