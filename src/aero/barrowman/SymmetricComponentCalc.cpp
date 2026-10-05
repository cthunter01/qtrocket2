#include "QtRocket/aero/barrowman/SymmetricComponentCalc.h"

#include <cmath>
#include <concepts>
#include <format>
#include <numbers>
#include <optional>
#include <vector>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/BarrowmanDragCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/barrowman/RocketComponentCalc.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/LinearInterpolator.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/PolyInterpolator.h"

namespace QtRocket
{

namespace
{

/// Java's `for (double m = from; m < to; m += step)`: the Mach numbers accumulate the rounding
/// of the step exactly as Java's do, since they are the keys of the interpolators. (A while
/// loop: a floating-point for-loop counter is what the loop-counter checks reject.)
template <std::invocable<double> Body>
void forEachMach(double from, double to, double step, Body body)
{
    double m = from;
    while (m < to)
    {
        body(m);
        m += step;
    }
}

// Experimental values of pressure drag for different nose cone shapes with a fineness ratio of
// 3. The data is taken from 'Collection of Zero-Lift Drag Data on Bodies of Revolution from
// Free-Flight Investigations', NASA TR-R-100, NTRS 19630004995, page 16. This data is
// extrapolated for other fineness ratios.
//
// Format: {Mach numbers}, {Cd values}. Java's static interpolators are function-local statics
// here, built on first use.

[[nodiscard]] const LinearInterpolator& ellipsoidInterpolator()
{
    static const LinearInterpolator kInterpolator{
        {1.2, 1.25, 1.3, 1.4, 1.6, 2.0, 2.4},
        {0.110, 0.128, 0.140, 0.148, 0.152, 0.159, 0.162 /* constant */}};
    return kInterpolator;
}

[[nodiscard]] const LinearInterpolator& x14Interpolator()
{
    static const LinearInterpolator kInterpolator{
        {1.2, 1.3, 1.4, 1.6, 1.8, 2.2, 2.6, 3.0, 3.6},
        {0.140, 0.156, 0.169, 0.192, 0.206, 0.227, 0.241, 0.249, 0.252}};
    return kInterpolator;
}

[[nodiscard]] const LinearInterpolator& x12Interpolator()
{
    static const LinearInterpolator kInterpolator{
        {0.925, 0.95, 1.0, 1.05, 1.1, 1.2, 1.3, 1.7, 2.0},
        {0, 0.014, 0.050, 0.060, 0.059, 0.081, 0.084, 0.085, 0.078}};
    return kInterpolator;
}

[[nodiscard]] const LinearInterpolator& x34Interpolator()
{
    static const LinearInterpolator kInterpolator{
        {0.8, 0.9, 1.0, 1.06, 1.2, 1.4, 1.6, 2.0, 2.8, 3.4},
        {0, 0.015, 0.078, 0.121, 0.110, 0.098, 0.090, 0.084, 0.078, 0.074}};
    return kInterpolator;
}

[[nodiscard]] const LinearInterpolator& vonKarmanInterpolator()
{
    static const LinearInterpolator kInterpolator{
        {0.9, 0.95, 1.0, 1.05, 1.1, 1.2, 1.4, 1.6, 2.0, 3.0},
        {0, 0.010, 0.027, 0.055, 0.070, 0.081, 0.095, 0.097, 0.091, 0.083}};
    return kInterpolator;
}

[[nodiscard]] const LinearInterpolator& lvHaackInterpolator()
{
    static const LinearInterpolator kInterpolator{
        {0.9, 0.95, 1.0, 1.05, 1.1, 1.2, 1.4, 1.6, 2.0},
        {0, 0.010, 0.024, 0.066, 0.084, 0.100, 0.114, 0.117, 0.113}};
    return kInterpolator;
}

[[nodiscard]] const LinearInterpolator& parabolicInterpolator()
{
    static const LinearInterpolator kInterpolator{
        {0.95, 0.975, 1.0, 1.05, 1.1, 1.2, 1.4, 1.7},
        {0, 0.016, 0.041, 0.092, 0.109, 0.119, 0.113, 0.108}};
    return kInterpolator;
}

[[nodiscard]] const LinearInterpolator& parabolic12Interpolator()
{
    static const LinearInterpolator kInterpolator{
        {0.8, 0.9, 0.95, 1.0, 1.05, 1.1, 1.3, 1.5, 1.8},
        {0, 0.016, 0.042, 0.100, 0.126, 0.125, 0.100, 0.090, 0.088}};
    return kInterpolator;
}

[[nodiscard]] const LinearInterpolator& parabolic34Interpolator()
{
    static const LinearInterpolator kInterpolator{
        {0.9, 0.95, 1.0, 1.05, 1.1, 1.2, 1.4, 1.7},
        {0, 0.023, 0.073, 0.098, 0.107, 0.106, 0.089, 0.082}};
    return kInterpolator;
}

/// The stagnation pressure drag from Mach 0 to 3 in steps of 0.05: the drag of a blunt body.
/// Java asks BarrowmanCalculator.calculateStagnationCD(), which forwards to
/// BarrowmanDragCalculator's; that one is called directly here.
[[nodiscard]] const LinearInterpolator& bluntInterpolator()
{
    static const LinearInterpolator kInterpolator = [] {
        LinearInterpolator blunt;
        forEachMach(0, 3, 0.05, [&blunt](double m) {
            blunt.addPoint(m, BarrowmanDragCalculator::calculateStagnationCD(m));
        });
        return blunt;
    }();
    return kInterpolator;
}

/// The polynomial through a value and a slope at Mach 1 and at Mach 1.3
/// (conicalPolyInterpolator).
[[nodiscard]] const PolyInterpolator& conicalPolyInterpolator()
{
    static const PolyInterpolator kInterpolator{{1.0, 1.3}, {1.0, 1.3}};
    return kInterpolator;
}

/// The pressure drag over the Mach number of an ogive of shape parameter @p param (0: a cone)
/// whose half-angle at the tip has the sine @p sinphi (calculateOgiveNoseInterpolator()): from
/// Mach 1 to 1.3 a polynomial, above it the direct formula, both scaled by the shape parameter
/// multiplier.
[[nodiscard]] LinearInterpolator calculateOgiveNoseInterpolator(double param, double sinphi)
{
    LinearInterpolator interpolator;

    // In the range M = 1 ... 1.3 use polynomial approximation
    const double cdMach1  = sinphi;
    const double cdMach13 = (2.1 * MathUtil::pow2(sinphi)) + (0.6019 * sinphi);

    const std::vector<double> poly = conicalPolyInterpolator().interpolator(
        {cdMach1, cdMach13, 4 / (AtmosphericConditions::kGamma + 1) * (1 - (0.5 * cdMach1)),
         -1.1341 * sinphi});

    // Shape parameter multiplier
    const double mul = (0.72 * MathUtil::pow2(param - 0.5)) + 0.82;

    forEachMach(1, 1.3001, 0.02, [&interpolator, &poly, mul](double m) {
        interpolator.addPoint(m, mul * PolyInterpolator::eval(m, poly));
    });

    // Above M = 1.3 use direct formula
    forEachMach(1.32, 4, 0.02, [&interpolator, mul, sinphi](double m) {
        interpolator.addPoint(m, mul * ((2.1 * MathUtil::pow2(sinphi)) +
                                        (0.5 * sinphi / MathUtil::safeSqrt((m * m) - 1))));
    });

    return interpolator;
}

/// The cone of fineness ratio @p fineness, which the power and parabolic series end in.
[[nodiscard]] LinearInterpolator coneOfFineness(double fineness)
{
    return calculateOgiveNoseInterpolator(
        0, 1 / MathUtil::safeSqrt(1 + (4 * MathUtil::pow2(fineness))));
}

/// The fineness-ratio-3 drag of a shape: one interpolator, or for a parameterised shape the two
/// that bound its parameter and the position 0 <= p <= 1 of the parameter between them. Empty
/// for the shapes whose interpolator is computed directly (Java: int1, int2 and p).
struct ShapeTables
{
    std::optional<LinearInterpolator> int1;
    std::optional<LinearInterpolator> int2;
    double                            p{0};
};

[[nodiscard]] ShapeTables powerTables(double param, double fineness)
{
    if (param <= 0.25)
    {
        return {.int1 = bluntInterpolator(), .int2 = x14Interpolator(), .p = param * 4};
    }
    if (param <= 0.5)
    {
        return {.int1 = x14Interpolator(), .int2 = x12Interpolator(), .p = (param - 0.25) * 4};
    }
    if (param <= 0.75)
    {
        return {.int1 = x12Interpolator(), .int2 = x34Interpolator(), .p = (param - 0.5) * 4};
    }
    return {.int1 = x34Interpolator(), .int2 = coneOfFineness(fineness), .p = (param - 0.75) * 4};
}

[[nodiscard]] ShapeTables parabolicTables(double param, double fineness)
{
    if (param <= 0.5)
    {
        return {
            .int1 = coneOfFineness(fineness), .int2 = parabolic12Interpolator(), .p = param * 2};
    }
    if (param <= 0.75)
    {
        return {.int1 = parabolic12Interpolator(),
                .int2 = parabolic34Interpolator(),
                .p    = (param - 0.5) * 4};
    }
    return {.int1 = parabolic34Interpolator(),
            .int2 = parabolicInterpolator(),
            .p    = (param - 0.75) * 4};
}

/// The interpolation between the two bounding interpolators of a parameterised shape, at the
/// Mach numbers of both.
[[nodiscard]] LinearInterpolator blend(const LinearInterpolator& int1,
                                       const LinearInterpolator& int2, double p)
{
    LinearInterpolator int3;
    for (const double m : int1.xPoints())
    {
        int3.addPoint(m, (p * int2.getValue(m)) + ((1 - p) * int1.getValue(m)));
    }
    for (const double m : int2.xPoints())
    {
        int3.addPoint(m, (p * int2.getValue(m)) + ((1 - p) * int1.getValue(m)));
    }
    return int3;
}

/// The fineness-ratio-3 drag @p int1 extrapolated to @p fineness, added to @p interpolator.
void extrapolateToFineness(LinearInterpolator& interpolator, const LinearInterpolator& int1,
                           double fineness)
{
    const double log4 = std::log(fineness + 1) / std::log(4.0);
    for (const double m : int1.xPoints())
    {
        const double stag = bluntInterpolator().getValue(m);
        interpolator.addPoint(m, stag * MathUtil::javaPow(int1.getValue(m) / stag, log4));
    }
}

/// Continues @p interpolator from its first Mach number down to Mach 0 in the form
/// Cd = a M^b + Cd(M = 0), when its first value is not (almost) zero.
void addSubsonicRegion(LinearInterpolator& interpolator, double sinphi)
{
    const std::vector<double> xPoints = interpolator.xPoints();
    if (xPoints.empty())
    {
        bug("the pressure drag interpolator has no points");  // Java: an array index error
    }
    const double min      = xPoints[0];
    const double minValue = interpolator.getValue(min);
    if (minValue < 0.001)
    {
        // No interpolation necessary
        return;
    }

    const double cdMach0  = 0.8 * MathUtil::pow2(sinphi);
    const double minDeriv = (interpolator.getValue(min + 0.01) - minValue) / 0.01;

    // These should not occur, but might cause havoc for the interpolation
    if ((cdMach0 >= minValue - 0.01) || (minDeriv <= 0.01))
    {
        return;
    }

    // Cd = a*M^b + cdMach0
    const double b = min * minDeriv / (minValue - cdMach0);
    const double a = (minValue - cdMach0) / MathUtil::javaPow(min, b);

    forEachMach(0, min, 0.05, [&interpolator, a, b, cdMach0](double m) {
        interpolator.addPoint(m, (a * MathUtil::javaPow(m, b)) + cdMach0);
    });
}

}  // namespace

SymmetricComponentCalc::SymmetricComponentCalc(const SymmetricComponent& component)
  : RocketComponentCalc(component), m_length(component.getLength())
{
    // Java reads the fore radius before the aft radius (an automatic radius is refreshed when
    // it is read).
    const double componentForeRadius = component.getForeRadius();
    const double componentAftRadius  = component.getAftRadius();
    if (m_length > 0)
    {
        m_foreRadius = componentForeRadius;
        m_aftRadius  = componentAftRadius;
    }
    else
    {
        // If length is zero, the component is a disk, i.e. a zero-length tube, so match the fore
        // and aft diameter
        const double componentMaxR = MathUtil::javaMax(componentForeRadius, componentAftRadius);
        m_foreRadius               = componentMaxR;
        m_aftRadius                = componentMaxR;
    }

    m_fineness       = m_length / (2 * std::abs(m_aftRadius - m_foreRadius));
    m_fullVolume     = component.getFullVolume();
    m_planformArea   = component.getComponentPlanformArea();
    m_planformCenter = component.getComponentPlanformCenter();

    m_wetArea = component.getComponentWetArea();

    if (dynamic_cast<const BodyTube*>(&component) != nullptr)
    {
        // no shape, parameter 0, frontal area 0, sinphi 0: the members' initial values
    }
    else if (const auto* transition = dynamic_cast<const Transition*>(&component))
    {
        m_shape       = transition->getShapeType();
        m_param       = transition->getShapeParameter();
        m_frontalArea = std::abs(std::numbers::pi *
                                 ((m_foreRadius * m_foreRadius) - (m_aftRadius * m_aftRadius)));

        const double r = component.getRadius(0.99 * m_length);
        if (m_shape == TransitionShape::OGIVE && m_param == 1.0)
        {
            m_sinphi = 0;  // special case: tangent ogive
        }
        else
        {
            m_sinphi = (m_aftRadius - r) / MathUtil::hypot(m_aftRadius - r, 0.01 * m_length);
        }
    }
    else
    {
        bug(std::format("Unknown component type {}", component.getComponentName()));
    }
}

void SymmetricComponentCalc::calculateNonaxialForces(const FlightConditions& conditions,
                                                     const Transformation& /*transform*/,
                                                     AerodynamicForces& forces,
                                                     WarningSet&        warnings)
{
    // Pre-calculate and store the results
    if (std::isnan(m_cnaCache))
    {
        const double r0 = m_foreRadius;
        const double r1 = m_aftRadius;

        if (MathUtil::equals(r0, r1))
        {
            m_isTube   = true;
            m_cnaCache = 0;
        }
        else
        {
            m_isTube = false;

            const double a0 = std::numbers::pi * MathUtil::pow2(r0);
            const double a1 = std::numbers::pi * MathUtil::pow2(r1);

            // This calculation of CNa is based on slender body theory, which at first glance
            // should not be appropriate particularly for boattails which one would intuitively
            // expect would have less effect on stability than predicted here. However, replacing
            // this code with the wind tunnel based data from Moore, F. G. and L. Y. Moore,
            // "Improved Aerodynamics for Configurations With Boattails", Journal of Spacecraft
            // and Rockets 45(2), March-April 2008 made almost no difference in CP calculations.
            // A short boattail added to the "simple model rocket" example shows a 1 mm
            // difference between this code and that result, at the cost of a substantial
            // increase in code complexity.
            m_cnaCache = 2 * (a1 - a0);
            m_cpCache  = ((m_length * a1) - m_fullVolume) / (a1 - a0);
        }
    }

    Coordinate cp;

    // If fore == aft, only body lift is encountered
    if (m_isTube)
    {
        cp = getLiftCP(conditions);
    }
    else
    {
        cp = Coordinate{m_cpCache, 0, 0,
                        m_cnaCache * conditions.getSincAOA() / conditions.getRefArea()}
                 .average(getLiftCP(conditions));
    }

    forces.setCP(cp);
    forces.setCN(forces.getCP().weight * conditions.getAOA());
    forces.setCm(forces.getCN() * cp.x / conditions.getRefLength());
    forces.setCroll(0);
    forces.setCrollDamp(0);
    forces.setCrollForce(0);
    forces.setCside(0);
    forces.setCyaw(0);

    // Add warning on supersonic flight
    if (conditions.getMach() > 1.1)
    {
        warnings.add(Warning::kSupersonic);
    }
}

Coordinate SymmetricComponentCalc::getLiftCP(const FlightConditions& conditions) const noexcept
{
    // Without this extra multiplier the rocket may become unstable at apogee when turning
    // around, and begin oscillating horizontally. During the flight of the rocket this has no
    // effect. It is effective only when AOA > 45 deg and the velocity is less than 15 m/s.
    //
    // OpenRocket marks it as something to revisit: this causes an anomaly to the flight results
    // with the CP jumping at apogee. It is kept, since the flight results depend on it.
    double mul = 1;
    if ((conditions.getMach() < 0.05) && (conditions.getAOA() > std::numbers::pi / 4))
    {
        mul = MathUtil::pow2(conditions.getMach() / 0.05);
    }

    return Coordinate{m_planformCenter, 0, 0,
                      mul * kBodyLiftK * m_planformArea / conditions.getRefArea() *
                          conditions.getSinAOA() * conditions.getSincAOA()};  // sin(aoa)^2 / aoa
}

double SymmetricComponentCalc::calculateFrictionCD(const FlightConditions& conditions,
                                                   double componentCf, WarningSet& /*warnings*/)
{
    return componentCf * m_wetArea / conditions.getRefArea();
}

double SymmetricComponentCalc::calculatePressureCD(const FlightConditions& conditions,
                                                   double stagnationCD, double baseCD,
                                                   WarningSet& /*warnings*/)
{
    // Check for simple cases first
    if (MathUtil::equals(m_foreRadius, m_aftRadius))
    {
        return 0;
    }

    if (m_length < 0.001)
    {
        if (m_foreRadius < m_aftRadius)
        {
            return stagnationCD * m_frontalArea / conditions.getRefArea();
        }
        return baseCD * m_frontalArea / conditions.getRefArea();
    }

    // Boattail drag computed directly from base drag
    if (m_aftRadius < m_foreRadius)
    {
        if (m_fineness >= 3)
        {
            return 0;
        }
        const double cd = baseCD * m_frontalArea / conditions.getRefArea();
        if (m_fineness <= 1)
        {
            return cd;
        }
        return cd * (3 - m_fineness) / 2;
    }

    // All nose cones and shoulders from pre-calculated and interpolating
    if (!m_interpolator.has_value())
    {
        m_interpolator = calculateNoseInterpolator();
    }

    return m_interpolator->getValue(conditions.getMach()) * m_frontalArea / conditions.getRefArea();
}

// First, the transonic/supersonic region is computed. For conical and ogive shapes this is
// calculated directly. For other shapes, the values for fineness-ratio 3 transitions are taken
// from the experimental values stored above (for parameterized shapes the values are
// interpolated between the parameter values). These are then extrapolated to the current
// fineness ratio.
//
// Finally, if the first data points in the interpolator are not zero, the subsonic region is
// interpolated in the form Cd = a*M^b + Cd(M=0).
LinearInterpolator SymmetricComponentCalc::calculateNoseInterpolator() const
{
    if (!m_shape.has_value())
    {
        // Java: a NullPointerException on the null shape of a body tube.
        bug("a body tube has no pressure drag interpolator");
    }
    const TransitionShape shape = *m_shape;

    LinearInterpolator interpolator;
    ShapeTables        tables;

    // Take into account nose cone shape. Conical and ogive generate the interpolator directly.
    // Others store a interpolator for fineness ratio 3 into int1, or for parameterized shapes
    // store the bounding fineness ratio 3 interpolators into int1 and int2 and set 0 <= p <= 1
    // according to the bounds.
    switch (shape)
    {
        case TransitionShape::CONICAL:
            interpolator = calculateOgiveNoseInterpolator(0, m_sinphi);  // param==0 -> conical
            break;

        case TransitionShape::OGIVE:
            interpolator = calculateOgiveNoseInterpolator(m_param, m_sinphi);
            break;

        case TransitionShape::ELLIPSOID:
            tables.int1 = ellipsoidInterpolator();
            break;

        case TransitionShape::POWER:
            tables = powerTables(m_param, m_fineness);
            break;

        case TransitionShape::PARABOLIC:
            tables = parabolicTables(m_param, m_fineness);
            break;

        case TransitionShape::HAACK:
            tables.int1 = vonKarmanInterpolator();
            tables.int2 = lvHaackInterpolator();
            tables.p    = m_param * 3;
            break;

        default:
            bug(std::format("Unknown transition shape: {}", static_cast<int>(shape)));
    }

    if (tables.p < 0 || tables.p > 1.00001)
    {
        bug(std::format("Inconsistent parameter value p={} shape={}", tables.p,
                        transitionShapeName(shape)));
    }

    // Check for parameterized shape and interpolate if necessary
    if (tables.int2.has_value())
    {
        // Java: int1 is never null when int2 is set.
        QTROCKET_ASSERT(tables.int1.has_value());
        tables.int1 = blend(*tables.int1, *tables.int2, tables.p);
    }

    // Extrapolate for fineness ratio if necessary
    if (tables.int1.has_value())
    {
        extrapolateToFineness(interpolator, *tables.int1, m_fineness);
    }

    // Now the transonic/supersonic region is ok. We still need to interpolate the subsonic
    // region, if the values are non-zero.
    addSubsonicRegion(interpolator, m_sinphi);

    return interpolator;
}

}  // namespace QtRocket
