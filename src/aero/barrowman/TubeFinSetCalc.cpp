#include "QtRocket/aero/barrowman/TubeFinSetCalc.h"

#include <cmath>
#include <numbers>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/barrowman/TubeCalc.h"
#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

TubeFinSetCalc::TubeFinSetCalc(const TubeFinSet& tubes)
  : TubeCalc(tubes),
    m_bodyRadius(tubes.getBodyRadius()),
    m_chord(tubes.getLength()),
    m_outerRadius(tubes.getOuterRadius())
{
    const MessageSources source{MessageSource{tubes.getId(), tubes.getName()}};
    if (tubes.getFinCount() == 1)
    {
        m_geometryWarnings.add(Warning::kTubeIsolated, source);
    }
    else if (tubes.getTubeSeparation() > MathUtil::kEpsilon)
    {
        m_geometryWarnings.add(Warning::kTubeSeparation, source);
    }
    else if (tubes.getTubeSeparation() < -MathUtil::kEpsilon)
    {
        m_geometryWarnings.add(Warning::kTubeOverlap, source);
    }

    const double innerRadius = tubes.getInnerRadius();

    // precompute geometry. This will be the geometry of a single tube, since the calculator
    // iterates across them. Doesn't consider interference between them; that should only be
    // relevant for fins that are either separated or overlapping.

    // aspect ratio.
    m_ar = 2 * innerRadius / m_chord;

    // Some trigonometry...
    // We need a triangle with the following three sides:
    // d is from the center of the body tube to a tangent point on the tube fin
    // outerRadius is from the center of the tube fin to the tangent point. Note that d and
    //     outerRadius are at right angles
    // bodyRadius + outerRadius is from the center of the body tube to the center of the tube
    //     fin. This is the hypotenuse of the right triangle.

    // Find length of d
    const double d =
        std::sqrt(MathUtil::pow2(m_bodyRadius + m_outerRadius) - MathUtil::pow2(m_outerRadius));

    // Area of diamond formed by mirroring triangle on its hypotenuse (same area as rectangle
    // formed by d and outerarea, but it *isn't* that rectangle)
    const double a = d * m_outerRadius;

    // angle between outerRadius and bodyRadius+outerRadius
    const double theta1 = std::acos(m_outerRadius / (m_outerRadius + m_bodyRadius));

    // area of arc from tube fin, doubled to get both halves of diamond
    const double a1 = MathUtil::pow2(m_outerRadius) * theta1;

    // angle between bodyRadius+outerRadius and d
    const double theta2 = (std::numbers::pi / 2.0) - theta1;

    // area of arc from body tube. Doubled so we have area to remove from diamond
    const double a2 = MathUtil::pow2(m_bodyRadius) * theta2;

    // area of interstice for one tube fin
    m_intersticeArea = a - a1 - a2;

    // wetted area for friction drag calculation. We don't consider the inner surface of the
    // tube; that affects the pressure drop through the tube and so (indirecctly) affects the
    // pressure drag.

    // Area of the outer surface of a tube, not including portion masked by interstice
    const double outerArea = m_chord * 2.0 * (std::numbers::pi - theta1) * m_outerRadius;

    // Surface area of the portion of the body tube masked by the tube fin. We'll subtract it
    // from the tube fin area rather than go in and change the body tube surface area
    // calculation. If tube fin and body tube roughness aren't the same this will result in an
    // inaccuracy.
    const double maskedArea = m_chord * 2.0 * theta2 * m_bodyRadius;

    m_wettedArea = outerArea - maskedArea;

    // Precompute most of CNa. Equation comes from Ribner, "The ring airfoil in nonaxial flow",
    // Journal of the Aeronautical Sciences 14(9) pp 529-530 (1947) equation (5). As stated in
    // techdoc.pdf, it's normalized by (1/2) rho v^2 (see section 3.1.1)
    const double arprime = 2 * m_ar / std::numbers::pi;
    m_cnaconst =
        2 * (arprime / (1 + arprime)) * std::numbers::pi * std::numbers::pi * innerRadius * m_chord;
}

void TubeFinSetCalc::calculateNonaxialForces(const FlightConditions& conditions,
                                             const Transformation& /*transform*/,
                                             AerodynamicForces& forces, WarningSet& warnings)
{
    warnings.addAll(m_geometryWarnings);

    if (m_outerRadius < 0.001)
    {
        forces.setCm(0);
        forces.setCN(0);
        forces.setCP(Coordinate::kZero);
        forces.setCroll(0);
        forces.setCrollDamp(0);
        forces.setCrollForce(0);
        forces.setCside(0);
        forces.setCyaw(0);
        return;
    }

    // Calculate CNa
    const double cna = m_cnaconst / conditions.getRefArea();

    // Calculate CP position
    const double x = calculateCPPos(conditions) * m_chord;

    // Roll forces
    // This isn't really tested, since the cant angle is required to be 0.
    forces.setCrollForce((m_bodyRadius + m_outerRadius) * cna * kCantAngle /
                         conditions.getRefLength());

    if (conditions.getAOA() > kStallAngle)
    {
        forces.setCrollForce(
            forces.getCrollForce() *
            MathUtil::clamp(1 - ((conditions.getAOA() - kStallAngle) / (kStallAngle / 2)), 0, 1));
    }

    forces.setCrollDamp((m_bodyRadius + m_outerRadius) * conditions.getRollRate() /
                        conditions.getVelocity() * cna / conditions.getRefLength());

    forces.setCroll(forces.getCrollForce() - forces.getCrollDamp());

    forces.setCN(cna * MathUtil::min(conditions.getAOA(), kStallAngle));
    forces.setCP(Coordinate{x, 0, 0, cna});
    forces.setCm(forces.getCN() * x / conditions.getRefLength());

    // OpenRocket computes no side force and no yaw moment here, and says why: doing so
    // produces strange results for stable rockets that have two fins in the front part of the
    // fuselage, where the rocket flies at an ever-increasing angle of attack, which may be due
    // to incorrect computation of the pitch and yaw damping moments.
    forces.setCside(0);
    forces.setCyaw(0);
}

double TubeFinSetCalc::calculateCPPos(const FlightConditions& conditions) const noexcept
{
    const double m = conditions.getMach();
    if (m <= 0.5)
    {
        // At subsonic speeds CP at quarter chord
        return kSubsonicCpPos;
    }
    if (m >= 2)
    {
        // At supersonic speeds use empirical formula
        return supersonicCPPos(m_ar * conditions.getBeta());
    }

    // Use the same shape-preserving interpolation as conventional fins.
    return transonicCPPos(m, m_ar);
}

double TubeFinSetCalc::calculateFrictionCD(const FlightConditions& conditions, double componentCf,
                                           WarningSet& /*warnings*/)
{
    return componentCf * m_wettedArea / conditions.getRefArea();
}

double TubeFinSetCalc::calculatePressureCD(const FlightConditions& conditions, double stagnationCD,
                                           double baseCD, WarningSet& warnings)
{
    warnings.addAll(m_geometryWarnings);

    return TubeCalc::calculatePressureCD(conditions, stagnationCD, baseCD, warnings) +
           ((stagnationCD + baseCD) * m_intersticeArea / conditions.getRefArea());
}

}  // namespace QtRocket
