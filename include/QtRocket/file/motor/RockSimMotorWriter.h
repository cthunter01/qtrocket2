#pragma once

#include <string>

namespace QtRocket
{

class ThrustCurveMotor;

/// Writes a ThrustCurveMotor as a RockSim engine file, .rse (OpenRocket's RockSimMotorWriter), as
/// .ork files embed the motors they use (thrustcurves/<digest>.rse). RockSimMotorLoader reads the
/// result back: one <engine> with the manufacturer's simple name, the designation, the type, the
/// diameter and length in mm, the launch and propellant masses in g, the delays (a plugged delay
/// as 1000), explicit mass and CG (auto-calc-mass="0", auto-calc-cg="0"), the description as
/// <comments> when there is one, and an <eng-data> per point with its time, thrust, mass (g) and
/// CG (mm). Numbers are written with Strings::doubleToString (TextUtil.doubleToString), so the
/// text matches OpenRocket's character for character: three decimals, or four significant digits
/// in exponential notation below 0.001 and from 10000 (a 12345.678 N thrust is written "1.235e4",
/// a 20.5 kg motor "2.05e4" g). What is read back therefore matches the motor to that precision,
/// and its digest (times in ms, masses in 0.1 g, CGs in mm, thrusts in mN) equals
/// MotorDigest::digestMotor() of the motor when every value stays below 10000, except at exact
/// rounding boundaries.
class RockSimMotorWriter
{
public:
    /// The complete .rse document for @p motor (write(), an instance method in OpenRocket; the
    /// writer has no state, so here it is static).
    [[nodiscard]] static std::string write(const ThrustCurveMotor& motor);
};

}  // namespace QtRocket
