#include "QtRocket/aero/barrowman/RocketComponentCalc.h"

#include <numbers>

#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

RocketComponentCalc::RocketComponentCalc(const RocketComponent& /*component*/) { }

double RocketComponentCalc::calculateComponentBaseCD(const FlightConditions& /*conditions*/,
                                                     double /*baseCD*/, WarningSet& /*warnings*/)
{
    return 0;
}

double RocketComponentCalc::calculateReynoldsNumber(double                  length,
                                                    const FlightConditions& conditions)
{
    return conditions.getVelocity() * length /
           conditions.getAtmosphericConditions().getKinematicViscosity();
}

double RocketComponentCalc::supersonicCPPos(double arBeta) noexcept
{
    if (arBeta <= kLowArCpBridgeStart)
    {
        return kSubsonicCpPos;
    }
    if (arBeta >= kSupersonicCpValidityBoundary)
    {
        return sourceSupersonicCPPos(arBeta);
    }

    const double width          = kSupersonicCpValidityBoundary - kLowArCpBridgeStart;
    const double t              = (arBeta - kLowArCpBridgeStart) / width;
    const double t2             = t * t;
    const double t3             = t2 * t;
    const double sourcePosition = sourceSupersonicCPPos(kSupersonicCpValidityBoundary);
    const double sourceGradient = sourceSupersonicCPGradient(kSupersonicCpValidityBoundary);

    // Cubic Hermite basis. The quarter-chord endpoint has zero gradient.
    const double startBasis       = (2 * t3) - (3 * t2) + 1;
    const double endBasis         = (-2 * t3) + (3 * t2);
    const double endGradientBasis = t3 - t2;
    return (startBasis * kSubsonicCpPos) + (endBasis * sourcePosition) +
           (endGradientBasis * width * sourceGradient);
}

double RocketComponentCalc::transonicCPPos(double mach, double aspectRatio) noexcept
{
    const double machRange = kTransonicCpEndMach - kTransonicCpStartMach;
    const double t         = (mach - kTransonicCpStartMach) / machRange;
    const double arBetaAtMach2 =
        aspectRatio * std::numbers::sqrt3;  // Math.sqrt(3), the same double
    const double endpointPosition = supersonicCPPos(arBetaAtMach2);
    const double delta            = endpointPosition - kSubsonicCpPos;

    if (delta <= 0)
    {
        return kSubsonicCpPos;
    }

    // At Mach 2, d(ar*beta)/dM = 2*ar/sqrt(3). Multiplying by the 1.5-wide normalized Mach
    // interval reduces the normalized endpoint slope to arBeta*d(CP)/d(arBeta).
    const double normalizedEndpointSlope = arBetaAtMach2 * supersonicCPGradient(arBetaAtMach2);
    const double slopeRatio              = normalizedEndpointSlope / delta;

    if (slopeRatio <= kMonotonicQuinticSlopeRatio)
    {
        const double t2 = t * t;
        const double t3 = t2 * t;
        const double t4 = t3 * t;
        const double t5 = t4 * t;

        return kSubsonicCpPos + (((10 * delta) - (6 * normalizedEndpointSlope)) * t2) +
               (((-20 * delta) + (14 * normalizedEndpointSlope)) * t3) +
               (((15 * delta) - (11 * normalizedEndpointSlope)) * t4) +
               (((3 * normalizedEndpointSlope) - (4 * delta)) * t5);
    }

    // At slopeRatio=5/3 the normalized limiting quintic is B(t)=(10/3)t^3-(10/3)t^4+t^5. Raising
    // B to slopeRatio/(5/3) preserves monotonicity and gives exactly the requested endpoint slope.
    const double t2              = t * t;
    const double t3              = t2 * t;
    const double limitingQuintic = t3 * ((10.0 / 3.0) - ((10.0 / 3.0) * t) + t2);
    return kSubsonicCpPos +
           (delta * MathUtil::javaPow(limitingQuintic, slopeRatio / kMonotonicQuinticSlopeRatio));
}

double RocketComponentCalc::sourceSupersonicCPPos(double arBeta) noexcept
{
    return (arBeta - kSupersonicCpOffset) / ((2 * arBeta) - 1);
}

double RocketComponentCalc::sourceSupersonicCPGradient(double arBeta) noexcept
{
    const double denominator = (2 * arBeta) - 1;
    return ((2 * kSupersonicCpOffset) - 1) / (denominator * denominator);
}

double RocketComponentCalc::supersonicCPGradient(double arBeta) noexcept
{
    if (arBeta <= kLowArCpBridgeStart)
    {
        return 0;
    }
    if (arBeta >= kSupersonicCpValidityBoundary)
    {
        return sourceSupersonicCPGradient(arBeta);
    }

    const double width          = kSupersonicCpValidityBoundary - kLowArCpBridgeStart;
    const double t              = (arBeta - kLowArCpBridgeStart) / width;
    const double t2             = t * t;
    const double sourcePosition = sourceSupersonicCPPos(kSupersonicCpValidityBoundary);
    const double sourceGradient = sourceSupersonicCPGradient(kSupersonicCpValidityBoundary);

    const double startBasisGradient       = (6 * t2) - (6 * t);
    const double endBasisGradient         = -startBasisGradient;
    const double endGradientBasisGradient = (3 * t2) - (2 * t);
    return (((startBasisGradient * kSubsonicCpPos) + (endBasisGradient * sourcePosition)) / width) +
           (endGradientBasisGradient * sourceGradient);
}

}  // namespace QtRocket
