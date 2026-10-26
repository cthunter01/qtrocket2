#include "QtRocket/motor/MotorCorrelation.h"

#include <format>

#include "QtRocket/motor/Motor.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::MotorCorrelation
{

namespace
{

/// The last sample time is below this (seconds).
constexpr double kSampleEnd = 1000;

/// The step between two sample times (seconds), added to the time at every sample.
constexpr double kSampleStep = 0.01;

/// diff(): the ratio of the smaller to the larger of @p a and @p b, 1.0 when the larger is zero.
[[nodiscard]] double diff(double a, double b) noexcept
{
    const double min = MathUtil::javaMin(a, b);
    const double max = MathUtil::javaMax(a, b);

    if (MathUtil::equals(max, 0))
    {
        return 1.0;
    }
    return min / max;
}

}  // namespace

double similarity(const Motor& motor1, const Motor& motor2)
{
    using MathUtil::javaMin;

    double d = crossCorrelation(motor1, motor2);

    d = javaMin(d, diff(motor1.getAverageThrustEstimate(), motor2.getAverageThrustEstimate()));
    d = javaMin(d, 2 * diff(motor1.getBurnTimeEstimate(), motor2.getBurnTimeEstimate()));
    d = javaMin(d, diff(motor1.getTotalImpulseEstimate(), motor2.getTotalImpulseEstimate()));

    return d;
}

double crossCorrelation(const Motor& motor1, const Motor& motor2)
{
    double auto1 = 0;
    double auto2 = 0;
    double cross = 0;
    // OpenRocket's for (t = 0; t < 1000; t += 0.01): the time is accumulated, not computed from a
    // sample count, so that the sample times (and through them the thrusts) are Java's.
    double t = 0;
    while (t < kSampleEnd)
    {
        const double thrust1 = motor1.getThrust(t);
        const double thrust2 = motor2.getThrust(t);

        if (thrust1 < 0 || thrust2 < 0)
        {
            bug(std::format("Negative thrust, t1={} t2={}", Strings::javaDoubleToString(thrust1),
                            Strings::javaDoubleToString(thrust2)));
        }

        auto1 += thrust1 * thrust1;
        auto2 += thrust2 * thrust2;
        cross += thrust1 * thrust2;

        t += kSampleStep;
    }

    const double autoCorrelation = MathUtil::javaMax(auto1, auto2);

    if (MathUtil::equals(autoCorrelation, 0))
    {
        return 1.0;
    }

    return cross / autoCorrelation;
}

}  // namespace QtRocket::MotorCorrelation
