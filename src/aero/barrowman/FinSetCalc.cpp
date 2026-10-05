#include "QtRocket/aero/barrowman/FinSetCalc.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/barrowman/Naca1307FinBodyInterference.h"
#include "QtRocket/aero/barrowman/RocketComponentCalc.h"
#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/LinearInterpolator.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/PolyInterpolator.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket
{

namespace
{

constexpr double kInfinity = std::numeric_limits<double>::infinity();

/// Considers the stall angle as 20 degrees (STALL_ANGLE).
constexpr double kStallAngle = 20 * std::numbers::pi / 180;

/// Upper end of the small-angle range where NACA 1307 is used without blending
/// (NACA_LINEAR_ANGLE).
constexpr double kNacaLinearAngle = 10 * std::numbers::pi / 180;

/// The angle of attack that the roll of a fin chord is limited to in the roll damping at low
/// speed, and above which the fin tips count as stalled there: Java's 15 * Math.PI / 180.
constexpr double kRollStallAngle = 15 * std::numbers::pi / 180;

constexpr double kCnaSubsonic   = 0.9;  // CNA_SUBSONIC
constexpr double kCnaSupersonic = 1.5;  // CNA_SUPERSONIC
constexpr double kGamma         = 1.4;  // GAMMA

/// DIVISIONS and DIVISIONS - 1 as the doubles Java widens the ints to in its arithmetic.
constexpr double kDivisionCount = FinSetCalc::kDivisions;
constexpr double kLastDivision  = FinSetCalc::kDivisions - 1;

/// pow(CNA_SUPERSONIC^2 - 1, 1.5) (CNA_SUPERSONIC_B).
[[nodiscard]] double cnaSupersonicB() noexcept
{
    static const double kValue = MathUtil::javaPow(MathUtil::pow2(kCnaSupersonic) - 1, 1.5);
    return kValue;
}

/// The coefficients of the third-order expansion of a fin's supersonic normal force in its angle
/// of attack, tabulated from Mach 1.5 in steps of 0.1 up to (not including) Mach 5 and held at
/// the last value beyond (K1, K2, K3).
struct SupersonicCoefficients
{
    LinearInterpolator k1;
    LinearInterpolator k2;
    LinearInterpolator k3;
};

/// Pre-calculates the values for K1, K2 and K3 (Java's static initialiser).
[[nodiscard]] SupersonicCoefficients makeSupersonicCoefficients()
{
    // Up to Mach 5
    constexpr auto      kCount = static_cast<std::size_t>((5.0 - kCnaSupersonic) * 10);
    std::vector<double> x(kCount);
    std::vector<double> k1(kCount);
    std::vector<double> k2(kCount);
    std::vector<double> k3(kCount);
    for (std::size_t i = 0; i < kCount; i++)
    {
        const double m    = kCnaSupersonic + (static_cast<double>(i) * 0.1);
        const double beta = MathUtil::safeSqrt((m * m) - 1);
        x[i]              = m;
        k1[i]             = 2.0 / beta;
        k2[i] = (((kGamma + 1) * MathUtil::javaPow(m, 4)) - (4 * MathUtil::pow2(beta))) /
                (4 * MathUtil::javaPow(beta, 4));
        k3[i] = (((kGamma + 1) * MathUtil::javaPow(m, 8)) +
                 (((2 * MathUtil::pow2(kGamma)) - (7 * kGamma) - 5) * MathUtil::javaPow(m, 6)) +
                 (10 * (kGamma + 1) * MathUtil::javaPow(m, 4)) + 8) /
                (6 * MathUtil::javaPow(beta, 7));
    }
    return SupersonicCoefficients{.k1 = LinearInterpolator(x, k1),
                                  .k2 = LinearInterpolator(x, k2),
                                  .k3 = LinearInterpolator(x, k3)};
}

/// K1, K2 and K3, built at first use (Java: static fields).
[[nodiscard]] const SupersonicCoefficients& supersonicCoefficients()
{
    static const SupersonicCoefficients kCoefficients = makeSupersonicCoefficients();
    return kCoefficients;
}

/// The transonic interpolation of the single-fin CNa: a polynomial through the values and first
/// derivatives at Mach 0.9 and 1.5 and the second derivative at Mach 0.9 (cnaInterpolator),
/// built at first use.
[[nodiscard]] const PolyInterpolator& cnaInterpolator()
{
    static const PolyInterpolator kInterpolator{
        {kCnaSubsonic, kCnaSupersonic}, {kCnaSubsonic, kCnaSupersonic}, {kCnaSubsonic}};
    return kInterpolator;
}

/// Whether @p component is a trapezoidal fin set whose root and tip chords are equal.
[[nodiscard]] bool isRectangularPlanform(const FinSet& component)
{
    const auto* trapezoidFinSet = dynamic_cast<const TrapezoidFinSet*>(&component);
    return trapezoidFinSet != nullptr &&
           MathUtil::equals(trapezoidFinSet->getRootChord(), trapezoidFinSet->getTipChord());
}

/// The x of the first of @p coordinates (Java: [0].getX()).
/// @throws BugError when there is none (Java: ArrayIndexOutOfBoundsException). A defence only:
///         toRelative() gives one coordinate for each instance location of the parent, and every
///         component of a tree has at least one.
[[nodiscard]] double firstX(const std::vector<Coordinate>& coordinates)
{
    if (coordinates.empty())
    {
        bug("a component without instances has no location");
    }
    return coordinates.front().x;
}

/// Java's Arrays.toString() of @p points: "[(x,y,z), (x,y,z)]".
[[nodiscard]] std::string pointsToString(std::span<const Coordinate> points)
{
    std::string text = "[";
    for (std::size_t i = 0; i < points.size(); i++)
    {
        if (i > 0)
        {
            text += ", ";
        }
        text += points[i].toString();
    }
    text += ']';
    return text;
}

}  // namespace

FinSetCalc::FinSetCalc(const FinSet& component)
  : RocketComponentCalc(component),
    m_thickness(component.getThickness()),
    m_bodyRadius(component.getBodyRadius()),
    m_finCount(component.getFinCount()),
    m_cantAngle(component.getCantAngle()),
    m_span(component.getSpan()),
    m_finArea(component.getPlanformArea()),
    m_crossSection(component.getCrossSection()),
    m_rectangularPlanform(isRectangularPlanform(component))
{
    calculateFinGeometry(component);
    calculateInterferenceFinCount(component);
    m_bodyFinInterference = createBodyFinInterferenceModel(component);
}

// Calculates the non-axial forces produced by each set of fins
// (normal and side forces, pitch, yaw and roll moments, CP position, CNa).
void FinSetCalc::calculateNonaxialForces(const FlightConditions& conditions,
                                         const Transformation& transform, AerodynamicForces& forces,
                                         WarningSet& warnings)
{
    warnings.addAll(m_geometryWarnings);

    if (m_finArea < MathUtil::kEpsilon || m_macSpan < MathUtil::kEpsilon)
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

    // ---- Calculate CNa.

    // One fin without interference (both sub- and supersonic):
    const double cna1 = calculateFinCNa1(conditions);

    // Multiple fins with fin-fin interference
    const double theta = conditions.getTheta();
    const double angle = transform.xRotation();

    // Compute basic CNa without interference effects
    double cna = cna1 * MathUtil::pow2(std::sin(theta - angle));

    // Take into account fin-fin interference effects
    switch (m_interferenceFinCount)
    {
        case 1:
        case 2:
        case 3:
        case 4:
            // No interference effect
            break;

        case 5:
            cna *= 0.948;
            break;

        case 6:
            cna *= 0.913;
            break;

        case 7:
            cna *= 0.854;
            break;

        case 8:
            cna *= 0.81;
            break;

        default:
            // Assume 75% efficiency
            cna *= 0.75;
            warnings.add(Warning::kParallelFins);
            break;
    }

    // Calculate the isolated-fin center of pressure before separating the
    // fin-in-body and body-in-fin loads.
    const double finCp = m_macLead + (calculateCPPos(conditions) * m_macLength);
    double       x     = finCp;

    // Body-fin interference uses the NACA model where its geometric assumptions
    // hold, and retains the previous scalar correction as an explicit fallback.
    const double r   = m_bodyRadius;
    double       tau = r / (m_span + r);
    // Java's guard, kept as a defence: no fin set gets here with such a tau (a NaN or infinite
    // body radius throws at construction, and a NaN, zero or negative span returned above)
    if (std::isnan(tau) || std::isinf(tau))
    {
        tau = 0;
    }
    /*
     * Cant is a wing-incidence case even when the complete planform/CP model is
     * unavailable.  The report selects chart 3 for rectangular supersonic fins
     * above beta*A=2 and equation 19 for the remaining shapes and regimes.
     * Its validity does not improve as body angle of attack approaches stall;
     * the existing post-stall roll reduction below handles that regime instead.
     */
    const double rollInterferenceFactor = Naca1307FinBodyInterference::calculateWingIncidenceFactor(
        tau, conditions.getMach(), m_ar, m_rectangularPlanform);
    const double isolatedCna = cna;
    const double fallbackCna =
        isolatedCna * calculateBodyFinInterferenceFactor(tau, conditions.getMach());
    const double nacaWeight = !m_bodyFinInterference.has_value()
                                  ? 0.0
                                  : calculateNacaApplicabilityWeight(conditions.getAOA());
    // The weight is positive only with a model; the second test says so to clang-tidy.
    if (nacaWeight > 0.0 && m_bodyFinInterference.has_value())
    {
        const double wingLiftCurveSlope = cna1 * conditions.getRefArea() / m_finArea;
        const Naca1307FinBodyInterference::Loads interference =
            m_bodyFinInterference->calculate(conditions.getMach(), wingLiftCurveSlope);
        const double finCna = isolatedCna * interference.finFactor;
        /*
         * In the planar supersonic regime bodyFactor is normalized by the
         * caller's wing lift-curve slope.  Multiplication by isolatedCna restores
         * the report's absolute carryover load, so the cna1 dependence cancels.
         */
        const double bodyCna        = isolatedCna * interference.bodyFactor;
        const double nacaCna        = finCna + bodyCna;
        const double fallbackMoment = fallbackCna * finCp;
        const double nacaMoment     = (finCna * finCp) + (bodyCna * interference.bodyCp);
        cna                         = fallbackCna + (nacaWeight * (nacaCna - fallbackCna));
        if (cna > MathUtil::kEpsilon)
        {
            x = (fallbackMoment + (nacaWeight * (nacaMoment - fallbackMoment))) / cna;
        }
    }
    else
    {
        cna = fallbackCna;
    }

    // OpenRocket leaves two effects out here ("TODO: LOW"): the fin tip Mach cone interference
    // (Barrowman thesis pdf-page 40) and the fin-fin Mach cone effect (MIL-HDBK page 5-25).

    // Calculate roll forces, reduce forcing above stall angle

    // The body-in-fin lift does not act through the canted fin surface.  Cant
    // therefore uses the selected lowercase NACA wing-incidence factor.
    forces.setCrollForce((m_macSpan + r) * cna1 * rollInterferenceFactor * m_cantAngle /
                         conditions.getRefLength());

    if (conditions.getAOA() > kStallAngle)
    {
        forces.setCrollForce(
            forces.getCrollForce() *
            MathUtil::clamp(1 - ((conditions.getAOA() - kStallAngle) / (kStallAngle / 2)), 0, 1));
    }
    forces.setCrollDamp(calculateDampingMoment(conditions));
    forces.setCroll(forces.getCrollForce() - forces.getCrollDamp());

    forces.setCN(cna * MathUtil::min(conditions.getAOA(), kStallAngle));
    forces.setCP(Coordinate{x, 0, 0, cna});
    forces.setCm(forces.getCN() * x / conditions.getRefLength());

    // OpenRocket ("TODO: HIGH"): the actual side force and yaw moment are not computed, because
    // that produces strange results for stable rockets that have two fins in the front part of
    // the fuselage, where the rocket flies at an ever-increasing angle of attack. This may be
    // due to incorrect computation of pitch/yaw damping moments.
    forces.setCside(0);
    forces.setCyaw(0);
}

// Pre-calculates the fin geometry values.
void FinSetCalc::calculateFinGeometry(const FinSet& component)
{
    m_geometryWarnings.clear();

    m_span    = component.getSpan();
    m_finArea = component.getPlanformArea();
    if (m_finArea < MathUtil::kEpsilon)
    {
        m_geometryWarnings.add(Warning::kZeroAreaFin, MessageSources{MessageSource::of(component)});
        m_ar = 0;
    }
    else
    {
        m_ar = 2 * MathUtil::pow2(m_span) / m_finArea;
    }

    checkOutline(component);

    // Calculate the chord lead and trail positions and length.  We do need the points
    // along the root for this
    calculateChords(component.getFinPointsWithRoot());

    // Check and correct any inconsistencies
    correctChords();

    calculateFinProperties(component.getFinFront().y);
}

void FinSetCalc::checkOutline(const FinSet& component)
{
    // Check geometry; don't consider points along fin root for this
    // (doing so will cause spurious jagged fin warnings)
    const std::vector<Coordinate> points = component.getFinPoints();
    bool                          down   = false;
    for (std::size_t i = 1; i < points.size(); i++)
    {
        if ((points[i].y > points[i - 1].y + 0.001) && down)
        {
            m_geometryWarnings.add(Warning::kJaggedEdgedFin,
                                   MessageSources{MessageSource::of(component)});
            break;
        }
        if (points[i].y < points[i - 1].y - 0.001)
        {
            down = true;
        }
    }

    if ((m_bodyRadius > 0) && (m_thickness > m_bodyRadius / 2))
    {
        // Add warnings  (radius/2 == diameter/4)
        m_geometryWarnings.add(Warning::kThickFin, MessageSources{MessageSource::of(component)});
    }
}

void FinSetCalc::calculateChords(std::span<const Coordinate> points)
{
    std::ranges::fill(m_chordLead, kInfinity);
    std::ranges::fill(m_chordTrail, -kInfinity);
    std::ranges::fill(m_chordLength, 0.0);

    for (std::size_t point = 1; point < points.size(); point++)
    {
        const double x1 = points[point - 1].x;
        const double y1 = points[point - 1].y;
        const double x2 = points[point].x;
        const double y2 = points[point].y;

        // Don't use the default EPSILON since it is too small
        // and causes too much numerical instability in the computation of x below
        if (MathUtil::equals(y1, y2, 0.001))
        {
            continue;
        }

        // Java's (int) cast: a NaN gives 0 and an infinity the largest or smallest int (a span
        // of 0), which a plain static_cast leaves undefined
        int i1 = MathUtil::javaIntCast(y1 * 1.0001 / m_span * kLastDivision);
        int i2 = MathUtil::javaIntCast(y2 * 1.0001 / m_span * kLastDivision);
        i1     = MathUtil::clamp(i1, 0, kDivisions - 1);
        i2     = MathUtil::clamp(i2, 0, kDivisions - 1);
        if (i1 > i2)
        {
            std::swap(i1, i2);
        }

        for (int i = i1; i <= i2; i++)
        {
            addChordPoint(static_cast<std::size_t>(i), x1, y1, x2, y2);
        }
    }
}

void FinSetCalc::addChordPoint(std::size_t i, double x1, double y1, double x2, double y2)
{
    const std::span<double> chordLead{m_chordLead};
    const std::span<double> chordTrail{m_chordTrail};
    const std::span<double> chordLength{m_chordLength};

    // Intersection point (x,y)
    // Note that y can be outside the bounds of the line
    // defined by (x1, y1) (x2 y2) so x can similarly be outside
    // the bounds.  If the line is nearly horizontal, it can be
    // 'way outside.  We want to get the whole "strip", so we
    // don't clamp y; however, we do clamp x to avoid numerical
    // instabilities
    const double y = static_cast<double>(i) * m_span / kLastDivision;
    const double x = MathUtil::clamp(((y - y2) / (y1 - y2) * x1) + ((y1 - y) / (y1 - y2) * x2),
                                     MathUtil::javaMin(x1, x2), MathUtil::javaMax(x1, x2));
    // Java's comparisons, kept as they are: a NaN x (from a NaN in the outline) leaves both
    // edges where they were.
    // NOLINTNEXTLINE(readability-use-std-min-max): see above
    if (x < chordLead[i])
    {
        chordLead[i] = x;
    }
    // NOLINTNEXTLINE(readability-use-std-min-max): see above
    if (x > chordTrail[i])
    {
        chordTrail[i] = x;
    }

    // OpenRocket ("TODO: LOW"): if a fin point is exactly on the chord line, it might be
    // counted twice:
    if (y1 < y2)
    {
        chordLength[i] -= x;
    }
    else
    {
        chordLength[i] += x;
    }
}

void FinSetCalc::correctChords()
{
    const std::span<double> chordLead{m_chordLead};
    const std::span<double> chordTrail{m_chordTrail};
    const std::span<double> chordLength{m_chordLength};

    for (std::size_t i = 0; i < chordLead.size(); i++)
    {
        if (std::isinf(chordLead[i]) || std::isinf(chordTrail[i]) || std::isnan(chordLead[i]) ||
            std::isnan(chordTrail[i]))
        {
            chordLead[i]  = 0;
            chordTrail[i] = 0;
        }
        if (chordLength[i] < 0 || std::isnan(chordLength[i]))
        {
            chordLength[i] = 0;
        }
        // NOLINTNEXTLINE(readability-use-std-min-max): Java's comparison, as in addChordPoint()
        if (chordLength[i] > chordTrail[i] - chordLead[i])
        {
            chordLength[i] = chordTrail[i] - chordLead[i];
        }
    }
}

void FinSetCalc::calculateFinProperties(double radius)
{
    const std::span<const double> chordLead{m_chordLead};
    const std::span<const double> chordTrail{m_chordTrail};
    const std::span<const double> chordLength{m_chordLength};

    /* Calculate fin properties:
     *
     * macLength // MAC length
     * macLead   // MAC leading edge position
     * macSpan   // MAC spanwise position
     * ar        // Fin aspect ratio (already set)
     * span      // Fin span (already set)
     */
    m_macLength    = 0;
    m_macLead      = 0;
    m_macSpan      = 0;
    m_cosGamma     = 0;
    m_cosGammaLead = 0;
    m_rollSum      = 0;
    double area    = 0;

    const double dy = m_span / kLastDivision;
    for (std::size_t i = 0; i < chordLead.size(); i++)
    {
        const double length = chordTrail[i] - chordLead[i];
        const double y      = static_cast<double>(i) * dy;

        m_macLength += length * length;
        m_macSpan += y * length;
        m_macLead += chordLead[i] * length;
        area += length;
        m_rollSum += chordLength[i] * MathUtil::pow2(radius + y);

        if (i > 0)
        {
            double dx =
                ((chordTrail[i] + chordLead[i]) / 2) - ((chordTrail[i - 1] + chordLead[i - 1]) / 2);
            double hypot = MathUtil::hypot(dx, dy);
            if (hypot != 0)
            {
                m_cosGamma += dy / hypot;
            }

            dx    = chordLead[i] - chordLead[i - 1];
            hypot = MathUtil::hypot(dx, dy);
            if (hypot != 0)
            {
                m_cosGammaLead += dy / hypot;
            }
        }
    }

    m_macLength *= dy;
    m_macSpan *= dy;
    m_macLead *= dy;
    area *= dy;
    m_rollSum *= dy;
    if (area > MathUtil::kEpsilon)
    {
        m_macLength /= area;
        m_macSpan /= area;
        m_macLead /= area;
    }
    else
    {
        m_macLength = 0;
        m_macSpan   = 0;
        m_macLead   = 0;
    }
    m_cosGamma /= kLastDivision;
    m_cosGammaLead /= kLastDivision;
}

// ---- the body-fin interference and the CNa1 calculation

std::optional<Naca1307FinBodyInterference> FinSetCalc::createBodyFinInterferenceModel(
    const FinSet& finSet) const
{
    const auto* trapezoidFinSet = dynamic_cast<const TrapezoidFinSet*>(&finSet);
    const auto* bodyTube        = dynamic_cast<const BodyTube*>(finSet.getParent());
    if (trapezoidFinSet == nullptr || bodyTube == nullptr)
    {
        return std::nullopt;
    }

    const double finFront = finSet.getAxialFront();
    if (finFront < -MathUtil::kEpsilon)
    {
        return std::nullopt;
    }

    const double                      bodyEnd = calculateCylindricalAfterbodyEnd(finSet, *bodyTube);
    const Naca1307FinBodyInterference model{m_bodyRadius,
                                            m_span,
                                            trapezoidFinSet->getRootChord(),
                                            trapezoidFinSet->getTipChord(),
                                            trapezoidFinSet->getSweep(),
                                            m_ar,
                                            bodyEnd};
    if (!model.isApplicable())
    {
        return std::nullopt;
    }
    return model;
}

double FinSetCalc::calculateCylindricalAfterbodyEnd(const FinSet& finSet, const BodyTube& bodyTube)
{
    const double    finFront       = finSet.getAxialOffset(AxialMethod::ABSOLUTE);
    const double    cylinderRadius = bodyTube.getAftRadius();
    double          bodyEnd = bodyTube.getAxialOffset(AxialMethod::ABSOLUTE) + bodyTube.getLength();
    const BodyTube* currentTube = &bodyTube;

    while (true)
    {
        const SymmetricComponent* candidate = currentTube->getNextSymmetricComponent();
        const auto*               nextTube  = dynamic_cast<const BodyTube*>(candidate);
        if (nextTube == nullptr || nextTube->getParent() != bodyTube.getParent())
        {
            break;
        }

        const double nextStart = nextTube->getAxialOffset(AxialMethod::ABSOLUTE);
        if (!MathUtil::equals(bodyEnd, nextStart) ||
            !MathUtil::equals(nextTube->getForeRadius(), cylinderRadius) ||
            !MathUtil::equals(nextTube->getAftRadius(), cylinderRadius))
        {
            break;
        }

        bodyEnd     = nextStart + nextTube->getLength();
        currentTube = nextTube;
    }

    return bodyEnd - finFront;
}

double FinSetCalc::calculateBodyFinInterferenceFactor(double tau, double mach) noexcept
{
    const double finInBodyFactor = 1 + tau;
    if (mach <= kCnaSubsonic)
    {
        return MathUtil::pow2(finInBodyFactor);
    }
    if (mach >= kCnaSupersonic)
    {
        return finInBodyFactor;
    }

    const double bodyInFinFactor        = tau * finInBodyFactor;
    const double bodyContributionWeight = (kCnaSupersonic - mach) / (kCnaSupersonic - kCnaSubsonic);
    return finInBodyFactor + (bodyContributionWeight * bodyInFinFactor);
}

double FinSetCalc::calculateNacaApplicabilityWeight(double angleOfAttack) noexcept
{
    if (!std::isfinite(angleOfAttack) || angleOfAttack < 0.0 || angleOfAttack >= kStallAngle)
    {
        return 0.0;
    }
    if (angleOfAttack <= kNacaLinearAngle)
    {
        return 1.0;
    }

    const double fraction = (angleOfAttack - kNacaLinearAngle) / (kStallAngle - kNacaLinearAngle);
    const double smoothFraction = fraction * fraction * (3.0 - (2.0 * fraction));
    return 1.0 - smoothFraction;
}

double FinSetCalc::calculateFinCNa1(const FlightConditions& conditions) const
{
    const double mach = conditions.getMach();
    const double ref  = conditions.getRefArea();
    const double alpha =
        MathUtil::min(conditions.getAOA(), std::numbers::pi - conditions.getAOA(), kStallAngle);

    if (m_finArea < MathUtil::kEpsilon || m_span < MathUtil::kEpsilon ||
        m_cosGamma < MathUtil::kEpsilon)
    {
        return 0;
    }

    // Subsonic case
    if (mach <= kCnaSubsonic)
    {
        return 2 * std::numbers::pi * MathUtil::pow2(m_span) /
               (1 + MathUtil::safeSqrt(
                        1 + ((1 - MathUtil::pow2(mach)) *
                             MathUtil::pow2(MathUtil::pow2(m_span) / (m_finArea * m_cosGamma))))) /
               ref;
    }

    const SupersonicCoefficients& k = supersonicCoefficients();

    // Supersonic case
    if (mach >= kCnaSupersonic)
    {
        return m_finArea *
               (k.k1.getValue(mach) + (k.k2.getValue(mach) * alpha) +
                (k.k3.getValue(mach) * MathUtil::pow2(alpha))) /
               ref;
    }

    // Transonic case, interpolate
    const double sq =
        MathUtil::safeSqrt(1 + ((1 - MathUtil::pow2(kCnaSubsonic)) *
                                MathUtil::pow2(m_span * m_span / (m_finArea * m_cosGamma))));
    const double subV = 2 * std::numbers::pi * MathUtil::pow2(m_span) / ref / (1 + sq);
    const double subD =
        2 * kCnaSubsonic * std::numbers::pi * MathUtil::javaPow(m_span, 6) /
        (MathUtil::pow2(m_finArea * m_cosGamma) * ref * sq * MathUtil::pow2(1 + sq));

    const double superV = m_finArea *
                          (k.k1.getValue(kCnaSupersonic) + (k.k2.getValue(kCnaSupersonic) * alpha) +
                           (k.k3.getValue(kCnaSupersonic) * MathUtil::pow2(alpha))) /
                          ref;
    const double superD = -m_finArea / ref * 2 * kCnaSupersonic / cnaSupersonicB();

    return cnaInterpolator().interpolate(mach, {subV, superV, subD, superD, 0});
}

double FinSetCalc::calculateDampingMoment(const FlightConditions& conditions) const
{
    const std::span<const double> chordLength{m_chordLength};
    const double                  rollRate = conditions.getRollRate();

    if (std::abs(rollRate) < 0.1)
    {
        return 0;
    }

    const double mach    = conditions.getMach();
    const double absRate = std::abs(rollRate);

    /*
     * At low speeds and relatively large roll rates (i.e. near apogee) the
     * fin tips rotate well above stall angle.  In this case sum the chords
     * separately.
     */
    if (absRate * (m_bodyRadius + m_span) / conditions.getVelocity() > kRollStallAngle)
    {
        double sum = 0;
        for (std::size_t i = 0; i < chordLength.size(); i++)
        {
            const double dist = m_bodyRadius + (m_span * static_cast<double>(i) / kDivisionCount);
            const double aoa =
                MathUtil::javaMin(absRate * dist / conditions.getVelocity(), kRollStallAngle);
            sum += chordLength[i] * dist * aoa;
        }
        sum = sum * (m_span / kDivisionCount) * 2 * std::numbers::pi / conditions.getBeta() /
              (conditions.getRefArea() * conditions.getRefLength());

        return MathUtil::sign(rollRate) * sum;
    }

    if (mach <= kCnaSubsonic)
    {
        return 2 * std::numbers::pi * rollRate * m_rollSum /
               (conditions.getRefArea() * conditions.getRefLength() * conditions.getVelocity() *
                conditions.getBeta());
    }
    if (mach >= kCnaSupersonic)
    {
        const SupersonicCoefficients& k   = supersonicCoefficients();
        const double                  vel = conditions.getVelocity();
        const double                  k1  = k.k1.getValue(mach);
        const double                  k2  = k.k2.getValue(mach);
        const double                  k3  = k.k3.getValue(mach);

        double sum = 0;

        for (std::size_t i = 0; i < chordLength.size(); i++)
        {
            const double y     = static_cast<double>(i) * m_span / kLastDivision;
            const double angle = rollRate * (m_bodyRadius + y) / vel;

            sum += ((k1 * angle) + (k2 * angle * angle) + (k3 * angle * angle * angle)) *
                   chordLength[i] * (m_bodyRadius + y);
        }

        return sum * m_span / kLastDivision / (conditions.getRefArea() * conditions.getRefLength());
    }

    // Transonic, do linear interpolation
    FlightConditions cond = conditions.clone();
    cond.setMach(kCnaSubsonic - 0.01);
    const double subsonic = calculateDampingMoment(cond);
    cond.setMach(kCnaSupersonic + 0.01);
    const double supersonic = calculateDampingMoment(cond);

    return (subsonic * (kCnaSupersonic - mach) / (kCnaSupersonic - kCnaSubsonic)) +
           (supersonic * (mach - kCnaSubsonic) / (kCnaSupersonic - kCnaSubsonic));
}

// Return the relative position of the CP along the mean aerodynamic chord.
// Below mach 0.5 it is at the quarter chord, above mach 2 calculated using an
// empirical formula, between these two using an interpolation polynomial.
double FinSetCalc::calculateCPPos(const FlightConditions& conditions) const noexcept
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

    // Use the shared shape-preserving interpolation between the two regimes.
    return transonicCPPos(m, m_ar);
}

double FinSetCalc::calculateFrictionCD(const FlightConditions& conditions, double componentCf,
                                       WarningSet& /*warnings*/)
{
    // a fin with 0 area contributes no drag
    if (m_finArea < MathUtil::kEpsilon || m_macLength < MathUtil::kEpsilon)
    {
        return 0.0;
    }

    return componentCf * (1 + (2 * m_thickness / m_macLength)) * 2 * m_finArea /
           conditions.getRefArea();
}

double FinSetCalc::calculatePressureCD(const FlightConditions& conditions, double stagnationCD,
                                       double /*baseCD*/, WarningSet& /*warnings*/)
{
    // a fin with 0 area contributes no drag
    if (m_finArea < MathUtil::kEpsilon)
    {
        return 0.0;
    }

    const double mach = conditions.getMach();
    double       cd   = 0;

    // Pressure fore-drag
    if (m_crossSection == FinSet::CrossSection::AIRFOIL ||
        m_crossSection == FinSet::CrossSection::ROUNDED)
    {
        // Round leading edge
        if (mach < 0.9)
        {
            cd = MathUtil::javaPow(1 - MathUtil::pow2(mach), -0.417) - 1;
        }
        else if (mach < 1)
        {
            cd = 1 - (1.785 * (mach - 0.9));
        }
        else
        {
            cd = 1.214 - (0.502 / MathUtil::pow2(mach)) +
                 (0.1095 / MathUtil::pow2(MathUtil::pow2(mach)));
        }
    }
    else if (m_crossSection == FinSet::CrossSection::SQUARE)
    {
        cd = stagnationCD;
    }
    else
    {
        // Java: UnsupportedOperationException; no fourth cross-section exists
        bug(std::format("Unsupported fin profile: {}", static_cast<int>(m_crossSection)));
    }

    // Slanted leading edge
    cd *= MathUtil::pow2(m_cosGammaLead);

    // Scale to correct reference area
    cd *= m_span * m_thickness / conditions.getRefArea();

    return cd;
}

double FinSetCalc::calculateComponentBaseCD(const FlightConditions& conditions, double baseCD,
                                            WarningSet& /*warnings*/)
{
    // a fin with 0 area contributes no drag
    if (m_finArea < MathUtil::kEpsilon)
    {
        return 0.0;
    }

    double cd = 0;

    // Trailing edge drag
    if (m_crossSection == FinSet::CrossSection::SQUARE)
    {
        cd = baseCD;
    }
    else if (m_crossSection == FinSet::CrossSection::ROUNDED)
    {
        cd = baseCD / 2;
    }
    // Airfoil assumed to have zero base drag

    // Scale to correct reference area
    cd *= m_span * m_thickness / conditions.getRefArea();

    return cd;
}

void FinSetCalc::calculateInterferenceFinCount(const FinSet& component)
{
    const RocketComponent* parent = component.getParent();
    if (parent == nullptr)
    {
        bug("fin set without parent component");  // Java: IllegalStateException
    }

    const double lead  = firstX(component.toRelative(Coordinate::kNul, *parent));
    const double trail = firstX(component.toRelative(Coordinate{component.getLength()}, *parent));

    /*
     * The counting fails if the fin root chord is very small, in that case assume
     * no other fin interference than this fin set.
     */
    if (trail - lead < 0.007)
    {
        m_interferenceFinCount = m_finCount;
    }
    else
    {
        m_interferenceFinCount = 0;
        for (const RocketComponent* c : parent->getChildren())
        {
            const auto* finSet = dynamic_cast<const FinSet*>(c);
            if (finSet == nullptr)
            {
                continue;
            }
            const double finLead  = firstX(c->toRelative(Coordinate::kNul, *parent));
            const double finTrail = firstX(c->toRelative(Coordinate{c->getLength()}, *parent));

            // Compute overlap of the fins

            if ((finLead < trail - 0.005) && (finTrail > lead + 0.005))
            {
                m_interferenceFinCount += finSet->getFinCount();
            }
        }
    }
    if (m_interferenceFinCount < component.getFinCount())
    {
        bug(std::format("Counted {} parallel fins, when component itself has {}, fin points={}",
                        m_interferenceFinCount, component.getFinCount(),
                        pointsToString(component.getFinPoints())));
    }
}

}  // namespace QtRocket
