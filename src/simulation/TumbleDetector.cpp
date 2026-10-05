#include "QtRocket/simulation/TumbleDetector.h"

#include <cmath>
#include <numbers>

#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

bool TumbleDetector::update(double time, bool guideCleared, double aoa, double airSpeed,
                            double airDensity, double naturalFrequency) noexcept
{
    const double dt = std::isnan(m_lastTime) ? 0.0 : time - m_lastTime;
    m_lastTime      = time;

    const double dynamicPressure = 0.5 * airDensity * airSpeed * airSpeed;

    // Observability gate.  A rocket still on the launch guide is held on course and
    // cannot tumble, yet a crosswind across a stationary rocket puts the angle of
    // attack near 90 degrees, so the angle carries no information until the rocket
    // is free.  Below the dynamic pressure floor the airflow direction is likewise
    // meaningless.  Hold rather than reset, so that a spell of negligible airflow --
    // passing through apogee, for instance -- does not discard evidence accumulated
    // before it.
    if (dt <= 0 || !guideCleared || std::isnan(aoa) || std::isnan(dynamicPressure) ||
        dynamicPressure < kMinDynamicPressure)
    {
        return isTumbling();
    }

    const double alpha = 1.0 - std::exp(-dt / timeConstant(naturalFrequency));
    m_filteredAoa += alpha * (aoa - m_filteredAoa);

    return isTumbling();
}

double TumbleDetector::timeConstant(double naturalFrequency) noexcept
{
    if (std::isnan(naturalFrequency) || naturalFrequency <= 0)
    {
        // Statically unstable, or not yet computable: there is no restoring
        // oscillation to wait out, so respond at the shortest sensible scale.
        return kMinTimeConstant;
    }
    return MathUtil::clamp(kDwellPeriods * 2 * std::numbers::pi / naturalFrequency,
                           kMinTimeConstant, kMaxTimeConstant);
}

}  // namespace QtRocket
