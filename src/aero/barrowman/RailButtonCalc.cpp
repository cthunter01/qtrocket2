#include "QtRocket/aero/barrowman/RailButtonCalc.h"

#include <array>
#include <cstddef>
#include <utility>
#include <vector>

#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/barrowman/RocketComponentCalc.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

namespace
{

// values transcribed from Gowen and Perkins, "Drag of Circular Cylinders for a Wide Range of
// Reynolds Numbers and Mach Numbers", NACA Technical Note 2960, Figure 7
constexpr std::array<double, 12> kCdDomain{0.0, 0.2, 0.3, 0.4, 0.5, 0.6,
                                           0.7, 1.0, 1.6, 2.0, 2.8, 100.0};
constexpr std::array<double, 12> kCdRange{1.2, 1.22, 1.25, 1.3,  1.4,  1.5,
                                          1.6, 2.1,  1.5,  1.45, 1.33, 1.33};

}  // namespace

RailButtonCalc::RailButtonCalc(const RailButton& button)
  : RocketComponentCalc(button), m_button(&button)
{
}

double RailButtonCalc::calculateFrictionCD(const FlightConditions& /*conditions*/,
                                           double /*componentCf*/, WarningSet& /*warnings*/)
{
    // very small relative surface area, and slick
    return 0.0;
}

void RailButtonCalc::calculateNonaxialForces(const FlightConditions& /*conditions*/,
                                             const Transformation& /*transform*/,
                                             AerodynamicForces& /*forces*/,
                                             WarningSet& /*warnings*/)
{
    // Nothing to be done
}

double RailButtonCalc::calculatePressureCD(const FlightConditions& conditions, double stagnationCD,
                                           double /*baseCD*/, WarningSet& /*warnings*/)
{
    // grab relevant button params
    const std::vector<Coordinate> instanceOffsets = m_button->getInstanceOffsets();

    // compute button reference area
    const double buttonHt  = m_button->getTotalHeight();
    const double outerArea = buttonHt * m_button->getOuterDiameter();
    const double notchArea =
        (m_button->getOuterDiameter() - m_button->getInnerDiameter()) * m_button->getInnerHeight();
    const double refArea = outerArea - notchArea;

    // accumulate Cd contribution from each rail button. If velocity is 0 just set CDmul to a
    // value previously competed for velocity MathUtil.EPSILON and skip the loop to avoid
    // division by 0
    double cdMul = 0.0;
    if (conditions.getMach() > MathUtil::kEpsilon)
    {
        const int instanceCount = m_button->getInstanceCount();
        // Java indexes the offsets up to the instance count (an array index error when there
        // are fewer).
        QTROCKET_ASSERT(std::cmp_less_equal(instanceCount, instanceOffsets.size()));
        for (int i = 0; i < instanceCount; i++)
        {
            // add to CDmul
            cdMul += effectiveInstanceCD(conditions, instanceOffsets[static_cast<std::size_t>(i)],
                                         buttonHt);
        }

        // since we'll be multiplying by the instance count up in the drag calculator, we want
        // to return the mean CD instead of the total
        cdMul /= instanceCount;
    }
    else
    {
        // value at velocity of MathUtil.EPSILON
        cdMul = 8.786395072609939E-4;
    }

    return cdMul * stagnationCD * refArea / conditions.getRefArea();
}

double RailButtonCalc::effectiveInstanceCD(const FlightConditions& conditions,
                                           const Coordinate& instanceOffset, double buttonHt) const
{
    // compute boundary layer height at button location. I can't find a good reference for the
    // formula, e.g. https://aerospaceengineeringblog.com/boundary-layers/ simply says it's the
    // "scientific consensus".
    const std::vector<Coordinate> absolute = m_button->toAbsolute(instanceOffset);
    if (absolute.empty())
    {
        bug("a rail button has no absolute location");  // Java: an array index error
    }
    const double x   = absolute[0].x;                           // location of button
    const double rex = calculateReynoldsNumber(x, conditions);  // Reynolds number of location
    const double del = 0.37 * x / MathUtil::javaPow(rex, 0.2);  // Boundary layer thickness

    // compute mean airspeed over button
    // this assumes airspeed changes linearly through boundary layer and that all parts of the
    // railbutton contribute equally to Cd, neither of which is true but both are plenty close
    // enough for our purposes
    double mach = 0;
    if (buttonHt > del)
    {
        // Case 1: button extends beyond boundary layer
        // Mean velocity is 1/2 rocket velocity up to limit of boundary layer, full velocity
        // after that
        mach = (buttonHt - (0.5 * del)) * conditions.getMach() / buttonHt;
    }
    else
    {
        // Case 2: button is entirely within boundary layer
        mach = MathUtil::map(buttonHt / 2.0, 0, del, 0, conditions.getMach());
    }

    // look up Cd as function of speed. It's pretty constant as a function of Reynolds number
    // when slow, so we can just use a function of Mach number
    const double cd = MathUtil::interpolate(kCdDomain, kCdRange, mach);

    // Since later drag force calculations don't consider boundary layer, compute "effective Cd"
    // based on rocket velocity
    return cd * MathUtil::pow2(mach) / MathUtil::pow2(conditions.getMach());
}

}  // namespace QtRocket
