#include "QtRocket/aero/BarrowmanDragCalculator.h"

#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

double BarrowmanDragCalculator::calculateStagnationCD(double mach) noexcept
{
    double pressure = 0;
    if (mach <= 1)
    {
        pressure = 1 + (MathUtil::pow2(mach) / 4) + (MathUtil::pow2(MathUtil::pow2(mach)) / 40);
    }
    else
    {
        pressure = 1.84 - (0.76 / MathUtil::pow2(mach)) +
                   (0.166 / MathUtil::pow2(MathUtil::pow2(mach))) +
                   (0.035 / MathUtil::pow2(mach * mach * mach));
    }
    return 0.85 * pressure;
}

double BarrowmanDragCalculator::calculateBaseCD(double mach) noexcept
{
    if (mach <= 1)
    {
        return 0.12 + (0.13 * mach * mach);
    }
    return 0.25 / mach;
}

}  // namespace QtRocket
