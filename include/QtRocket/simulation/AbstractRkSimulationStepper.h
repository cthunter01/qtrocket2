#pragma once

#include <numbers>

namespace QtRocket
{

/// The base of the Runge-Kutta simulation steppers (OpenRocket's
/// simulation/AbstractRKSimulationStepper). For now it holds only its public constants, which
/// the simulation conditions and the simulation options read (Java reads them through
/// RK4SimulationStepper, which inherits them).
// HOOK(steppers): part B completes this class
class AbstractRkSimulationStepper
{
public:
    /// A recommended reasonably accurate time step, s (RECOMMENDED_TIME_STEP).
    static constexpr double kRecommendedTimeStep = 0.05;

    /// A recommended reasonable maximum simulation time, s (RECOMMENDED_MAX_TIME).
    static constexpr double kRecommendedMaxTime = 1200;

    /// A recommended maximum angle step value, rad: 3 degrees, written as in Java
    /// (RECOMMENDED_ANGLE_STEP).
    static constexpr double kRecommendedAngleStep = 3 * std::numbers::pi / 180;

    /// A random amount that is added to pitch and yaw coefficients, plus or minus
    /// (PITCH_YAW_RANDOM).
    static constexpr double kPitchYawRandom = 0.0005;
};

}  // namespace QtRocket
