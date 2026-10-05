#include "QtRocket/aero/BarrowmanDragCalculator.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <vector>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/DragCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/aero/barrowman/RocketComponentCalc.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/InstanceMap.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/PolyInterpolator.h"

namespace QtRocket
{

namespace
{

using MathUtil::pow2;

/// 17 degrees in radians, as Java writes it (17 * Math.PI / 180).
constexpr double kSeventeenDegrees = 17 * std::numbers::pi / 180;

/// The axial drag multiplier below 17 degrees (axialDragPoly1): 1 at 0 and 1.3 at 17 degrees,
/// both with zero slope.
[[nodiscard]] const std::vector<double>& axialDragPoly1()
{
    static const std::vector<double> kPoly =
        PolyInterpolator{{0, kSeventeenDegrees}, {0, kSeventeenDegrees}}.interpolator(
            {1, 1.3, 0, 0});
    return kPoly;
}

/// The axial drag multiplier from 17 to 90 degrees (axialDragPoly2): 1.3 at 17 and 0 at 90
/// degrees, zero slope at both and zero curvature at 90.
[[nodiscard]] const std::vector<double>& axialDragPoly2()
{
    static const std::vector<double> kPoly =
        PolyInterpolator{{kSeventeenDegrees, std::numbers::pi / 2},
                         {kSeventeenDegrees, std::numbers::pi / 2},
                         {std::numbers::pi / 2}}
            .interpolator({1.3, 0, 0, 0, 0});
    return kPoly;
}

/// @p component as a SymmetricComponent, or nullptr (Java: instanceof SymmetricComponent).
[[nodiscard]] const SymmetricComponent* asSymmetric(const RocketComponent& component) noexcept
{
    return dynamic_cast<const SymmetricComponent*>(&component);
}

/// Whether the CD of @p c is overridden, by itself or by an ancestor: such a component has no
/// friction, pressure or base drag.
[[nodiscard]] bool isOverridden(const RocketComponent& c)
{
    return c.isCDOverridden() || c.isCDOverriddenByAncestor();
}

/// The entry of @p c in @p forceMap, or nullptr without a map or an entry (Java:
/// `forceMap != null && forceMap.get(c) != null`).
[[nodiscard]] AerodynamicForces* entryOf(ForceMap* forceMap, const RocketComponent& c) noexcept
{
    return forceMap != nullptr ? forceMap->get(&c) : nullptr;
}

/// calculateReynoldsNumber(): the Reynolds number of the rocket's aerodynamic length.
[[nodiscard]] double calculateReynoldsNumber(const FlightConfiguration& configuration,
                                             const FlightConditions&    conditions)
{
    return conditions.getVelocity() * configuration.getLengthAerodynamic() /
           conditions.getAtmosphericConditions().getKinematicViscosity();
}

/// The compressibility corrections of the skin friction coefficient: the subsonic c1 and the
/// supersonic c2.
struct Compressibility
{
    double c1{1.0};
    double c2{1.0};
};

/// The corrections of a perfect finish: none up to a Reynolds number of 1e6, in full from 3e6,
/// in proportion between them.
[[nodiscard]] Compressibility perfectFinishCompressibility(double mach, double re) noexcept
{
    Compressibility c;
    if ((mach < 1.1) && (re > 1.0e6))
    {
        if (re < 3.0e6)
        {
            c.c1 = 1 - (0.1 * pow2(mach) * (re - 1.0e6) / 2.0e6);
        }
        else
        {
            c.c1 = 1 - (0.1 * pow2(mach));
        }
    }
    if ((mach > 0.9) && (re > 1.0e6))
    {
        if (re < 3.0e6)
        {
            c.c2 = 1 + (((1.0 / MathUtil::javaPow(1 + (0.045 * pow2(mach)), 0.25)) - 1) *
                        (re - 1.0e6) / 2.0e6);
        }
        else
        {
            c.c2 = 1.0 / MathUtil::javaPow(1 + (0.045 * pow2(mach)), 0.25);
        }
    }
    return c;
}

/// The corrections of a turbulent boundary layer.
[[nodiscard]] Compressibility turbulentCompressibility(double mach) noexcept
{
    Compressibility c;
    if (mach < 1.1)
    {
        c.c1 = 1 - (0.1 * pow2(mach));
    }
    if (mach > 0.9)
    {
        c.c2 = 1 / MathUtil::javaPow(1 + (0.15 * pow2(mach)), 0.58);
    }
    return c;
}

/// The roughness-limited skin friction coefficients by Finish, each worked out when the first
/// component with that finish asks for it (Java: the array roughnessLimited, NaN until then).
class RoughnessLimited
{
public:
    RoughnessLimited(const FlightConfiguration& configuration, double roughnessCorrection)
      : m_configuration(&configuration), m_roughnessCorrection(roughnessCorrection)
    {
        m_values.fill(std::numeric_limits<double>::quiet_NaN());
    }

    [[nodiscard]] double of(Finish finish)
    {
        double& value = m_values.at(static_cast<std::size_t>(finish));
        if (std::isnan(value))
        {
            value = 0.032 *
                    MathUtil::javaPow(
                        roughnessSize(finish) / m_configuration->getLengthAerodynamic(), 0.2) *
                    m_roughnessCorrection;
        }
        return value;
    }

private:
    const FlightConfiguration*              m_configuration;
    double                                  m_roughnessCorrection;
    std::array<double, kAllFinishes.size()> m_values{};
};

/// The skin friction coefficient of a component: the rocket's @p cf or the roughness-limited
/// coefficient @p roughnessLimited of the component's finish.
[[nodiscard]] double componentFrictionCoefficient(bool perfectFinish, double re, double cf,
                                                  double roughnessLimited) noexcept
{
    if (perfectFinish)
    {
        if ((re > 1.0e6) && (roughnessLimited > cf))
        {
            return roughnessLimited;
        }
        return cf;
    }
    return MathUtil::javaMax(cf, roughnessLimited);
}

/// The extent of the body that the active symmetric components form (Java: the locals minX, maxX
/// and maxR of calculateFrictionCD()).
struct BodyExtent
{
    double maxR{0};
    double minX{std::numeric_limits<double>::max()};
    double maxX{0};

    void add(const SymmetricComponent& s)
    {
        const double componentMinX = s.getAxialOffset(AxialMethod::ABSOLUTE);
        minX                       = MathUtil::javaMin(minX, componentMinX);

        const double componentMaxX = componentMinX + s.getLength();
        maxX                       = MathUtil::javaMax(maxX, componentMaxX);

        const double componentMaxR = MathUtil::javaMax(s.getForeRadius(), s.getAftRadius());
        maxR                       = MathUtil::javaMax(maxR, componentMaxR);
    }
};

/// The stagnation drag of one instance of @p s on the ring by which its fore radius exceeds the
/// aft radius of the active symmetric component ahead of it; nullopt when it does not.
[[nodiscard]] std::optional<double> calculateStagnationDiskCD(
    const FlightConfiguration& configuration, const FlightConditions& conditions,
    const SymmetricComponent& s, double stagnation)
{
    double       foreRadius = s.getForeRadius();
    const double aftRadius  = s.getAftRadius();
    if (s.getLength() == 0)
    {
        foreRadius = MathUtil::javaMax(foreRadius, aftRadius);
    }
    double                          radius        = 0;
    const SymmetricComponent* const prevComponent = s.getPreviousSymmetricComponent();
    if (prevComponent != nullptr && configuration.isComponentActive(*prevComponent))
    {
        radius = prevComponent->getAftRadius();
    }

    if (radius < foreRadius)
    {
        const double area = std::numbers::pi * (pow2(foreRadius) - pow2(radius));
        return stagnation * area / conditions.getRefArea();
    }
    return std::nullopt;
}

/// isTerminalSymmetricComponent(): whether @p component ends at an exposed base in the active
/// flight configuration rather than at an internal radius discontinuity.
[[nodiscard]] bool isTerminalSymmetricComponent(const FlightConfiguration& configuration,
                                                const SymmetricComponent&  component)
{
    const SymmetricComponent* const nextComponent = component.getNextSymmetricComponent();
    return nextComponent == nullptr || !configuration.isComponentActive(*nextComponent);
}

/// calculateSymmetricComponentBaseArea(): the exposed aft-facing area of one instance of
/// @p component.
[[nodiscard]] double calculateSymmetricComponentBaseArea(const FlightConfiguration& configuration,
                                                         const SymmetricComponent&  component)
{
    const double foreRadius = component.getForeRadius();
    double       aftRadius  = component.getAftRadius();
    if (component.getLength() == 0)
    {
        aftRadius = MathUtil::javaMax(foreRadius, aftRadius);
    }

    const SymmetricComponent* const nextComponent = component.getNextSymmetricComponent();
    double                          nextRadius    = 0;
    if (nextComponent != nullptr && configuration.isComponentActive(*nextComponent))
    {
        nextRadius = nextComponent->getForeRadius();
    }

    if (nextRadius >= aftRadius)
    {
        return 0;
    }
    return std::numbers::pi * (pow2(aftRadius) - pow2(nextRadius));
}

/// The base drag of one instance of the symmetric component @p s, of which @p instanceCount are
/// active; nullopt when it has no exposed aft-facing area.
[[nodiscard]] std::optional<double> calculateSymmetricComponentBaseCD(
    const FlightConfiguration& configuration, const FlightConditions& conditions,
    const SymmetricComponent& s, int instanceCount, double base)
{
    const double area = calculateSymmetricComponentBaseArea(configuration, s);
    if (!(area > 0))
    {
        return std::nullopt;
    }

    // A component assembly represents one independent core, booster, or pod wake. Clamp nozzle
    // area against that wake only, so excess nozzle area cannot reduce base drag on another
    // body.  Internal diameter steps are outside the terminal wake and remain unchanged.
    double areaScale = 1;
    if (isTerminalSymmetricComponent(configuration, s))
    {
        const double totalComponentArea = instanceCount * area;
        const double nozzleExitArea     = conditions.getThrustingNozzleExitArea(s.getAssembly());
        const double excludedNozzleArea = MathUtil::javaMin(nozzleExitArea, totalComponentArea);
        areaScale = (totalComponentArea - excludedNozzleArea) / totalComponentArea;
    }
    return base * area * areaScale / conditions.getRefArea();
}

/// calculateOverrideCD(): the override drag of the rocket; each overridden assembly's into
/// @p assemblyForces and each overridden component's into @p componentForces, when given.
[[nodiscard]] double calculateOverrideCD(const FlightConfiguration& configuration,
                                         ForceMap* componentForces, ForceMap* assemblyForces)
{
    double total = 0;
    for (const auto& [c, contexts] : configuration.getActiveInstances())
    {
        const int  instanceCount = static_cast<int>(contexts.size());
        const bool assembly      = isAssembly(c->kind());

        if (!c->isAerodynamic() && !assembly)
        {
            continue;
        }

        if (c->isCDOverridden() && !c->isCDOverriddenByAncestor())
        {
            const double             cd        = instanceCount * c->getOverrideCD();
            ForceMap* const          targetMap = assembly ? assemblyForces : componentForces;
            AerodynamicForces* const entry     = entryOf(targetMap, *c);
            if (entry != nullptr)
            {
                entry->setOverrideCD(cd);
            }
            total += cd;
        }
    }

    return total;
}

}  // namespace

std::unique_ptr<DragCalculator> BarrowmanDragCalculator::newInstance() const
{
    return std::make_unique<BarrowmanDragCalculator>();
}

void BarrowmanDragCalculator::calculateDrag(const FlightConfiguration& configuration,
                                            const FlightConditions&    conditions,
                                            ForceMap* componentForces, ForceMap* assemblyForces,
                                            AerodynamicForces& totalForces, WarningSet& warnings)
{
    // Java's private calculations each call ensureCalcMap() again, which does nothing then.
    m_calcMap.ensureBuilt(configuration);

    const double frictionCD =
        calculateFrictionCD(configuration, conditions, componentForces, warnings);
    const double pressureCD =
        calculatePressureCD(configuration, conditions, componentForces, warnings);
    const double baseCD     = calculateBaseCD(configuration, conditions, componentForces, warnings);
    const double overrideCD = calculateOverrideCD(configuration, componentForces, assemblyForces);

    totalForces.setFrictionCD(frictionCD);
    totalForces.setPressureCD(pressureCD);
    totalForces.setBaseCD(baseCD);
    totalForces.setOverrideCD(overrideCD);
    totalForces.setCD(frictionCD + pressureCD + baseCD + overrideCD);
    totalForces.setCDaxial(toAxialDrag(conditions, totalForces.getCD()));
}

double BarrowmanDragCalculator::toAxialDrag(const FlightConditions& conditions, double cd) const
{
    double aoa = MathUtil::clamp(conditions.getAOA(), 0, std::numbers::pi);
    double mul = 0;

    if (aoa > std::numbers::pi / 2)
    {
        aoa = std::numbers::pi - aoa;
    }
    if (aoa < kSeventeenDegrees)
    {
        mul = PolyInterpolator::eval(aoa, axialDragPoly1());
    }
    else
    {
        mul = PolyInterpolator::eval(aoa, axialDragPoly2());
    }

    if (conditions.getAOA() < std::numbers::pi / 2)
    {
        return mul * cd;
    }
    return -mul * cd;
}

void BarrowmanDragCalculator::voidAerodynamicCache()
{
    m_calcMap.clear();
}

RocketComponentCalc& BarrowmanDragCalculator::calcOf(const RocketComponent& component)
{
    RocketComponentCalc* const calc = m_calcMap.get(component);
    if (calc == nullptr)
    {
        // Java: calcMap.get(c) is null, a NullPointerException
        bug(std::format("component {} has no calculation", component.getName()));
    }
    return *calc;
}

double BarrowmanDragCalculator::calculateFrictionCD(const FlightConfiguration& configuration,
                                                    const FlightConditions&    conditions,
                                                    ForceMap* forceMap, WarningSet& warnings)
{
    const double mach          = conditions.getMach();
    const double re            = calculateReynoldsNumber(configuration, conditions);
    const bool   perfectFinish = configuration.getRocket().isPerfectFinish();
    const double cf            = calculateFrictionCoefficient(perfectFinish, mach, re);

    double     otherFrictionCD = 0;
    double     bodyFrictionCD  = 0;
    BodyExtent body;

    RoughnessLimited roughnessLimited{configuration, calculateRoughnessCorrection(mach)};

    for (const auto& [c, contexts] : configuration.getActiveInstances())
    {
        if (!c->isAerodynamic() || isOverridden(*c))
        {
            continue;
        }

        const auto* const external = dynamic_cast<const ExternalComponent*>(c);
        QTROCKET_ASSERT(external != nullptr);  // Java: a ClassCastException
        const double componentCf = componentFrictionCoefficient(
            perfectFinish, re, cf, roughnessLimited.of(external->getFinish()));

        const double componentFrictionCD =
            calcOf(*c).calculateFrictionCD(conditions, componentCf, warnings);
        const int instanceCount = static_cast<int>(contexts.size());

        if (const SymmetricComponent* const s = asSymmetric(*c))
        {
            bodyFrictionCD += instanceCount * componentFrictionCD;
            body.add(*s);
        }
        else
        {
            otherFrictionCD += instanceCount * componentFrictionCD;
        }

        if (AerodynamicForces* const entry = entryOf(forceMap, *c))
        {
            entry->setFrictionCD(componentFrictionCD);
        }
    }

    const double bodyLength = body.maxX - body.minX + 0.0001;
    const double correction = calculateBodyFrictionCorrection(bodyLength, body.maxR);

    if (forceMap != nullptr)
    {
        for (auto& [key, forces] : *forceMap)
        {
            if (key != nullptr && asSymmetric(*key) != nullptr)
            {
                forces.setFrictionCD(forces.getFrictionCD() * correction);
            }
        }
    }

    return otherFrictionCD + (correction * bodyFrictionCD);
}

double BarrowmanDragCalculator::calculateBodyFrictionCorrection(double bodyLength,
                                                                double maxRadius) noexcept
{
    const double bodyDiameter  = 2 * maxRadius;
    const double finenessRatio = bodyLength / bodyDiameter;
    return 1 + (1.0 / (2 * finenessRatio));
}

double BarrowmanDragCalculator::calculateFrictionCoefficient(bool perfectFinish, double mach,
                                                             double reynolds) noexcept
{
    double          cf = 0;
    Compressibility c;

    if (perfectFinish)
    {
        if (reynolds < 1.0e4)
        {
            cf = 1.33e-2;
        }
        else if (reynolds < 5.39e5)
        {
            cf = 1.328 / MathUtil::safeSqrt(reynolds);
        }
        else
        {
            cf = (1.0 / pow2((1.50 * std::log(reynolds)) - 5.6)) - (1700 / reynolds);
        }
        c = perfectFinishCompressibility(mach, reynolds);
    }
    else
    {
        if (reynolds < 1.0e4)
        {
            cf = 1.48e-2;
        }
        else
        {
            cf = 1.0 / pow2((1.50 * std::log(reynolds)) - 5.6);
        }
        c = turbulentCompressibility(mach);
    }

    // The same in both of Java's branches.
    if (mach < 0.9)
    {
        cf *= c.c1;
    }
    else if (mach < 1.1)
    {
        cf *= (c.c2 * (mach - 0.9) / 0.2) + (c.c1 * (1.1 - mach) / 0.2);
    }
    else
    {
        cf *= c.c2;
    }

    return cf;
}

double BarrowmanDragCalculator::calculateRoughnessCorrection(double mach) noexcept
{
    if (mach < 0.9)
    {
        return 1 - (0.1 * pow2(mach));
    }
    if (mach > 1.1)
    {
        return 1 / (1 + (0.18 * pow2(mach)));
    }
    const double c1 = 1 - (0.1 * pow2(0.9));
    const double c2 = 1.0 / (1 + (0.18 * pow2(1.1)));
    return (c2 * (mach - 0.9) / 0.2) + (c1 * (1.1 - mach) / 0.2);
}

double BarrowmanDragCalculator::calculatePressureCD(const FlightConfiguration& configuration,
                                                    const FlightConditions&    conditions,
                                                    ForceMap* forceMap, WarningSet& warnings)
{
    const double stagnation = calculateStagnationCD(conditions.getMach());
    const double base       = calculateBaseCD(conditions.getMach());

    double total = 0;
    for (const auto& [c, contexts] : configuration.getActiveInstances())
    {
        if (!c->isAerodynamic() || isOverridden(*c))
        {
            continue;
        }

        const int instanceCount = static_cast<int>(contexts.size());

        const double cd = calcOf(*c).calculatePressureCD(conditions, stagnation, base, warnings);

        AerodynamicForces* const entry = entryOf(forceMap, *c);
        if (entry != nullptr)
        {
            entry->setPressureCD(cd);
        }

        total += cd * instanceCount;

        const SymmetricComponent* const s = asSymmetric(*c);
        if (s == nullptr)
        {
            continue;
        }
        if (const std::optional<double> diskCd =
                calculateStagnationDiskCD(configuration, conditions, *s, stagnation))
        {
            total += instanceCount * *diskCd;

            if (entry != nullptr)
            {
                entry->setPressureCD(entry->getPressureCD() + *diskCd);
            }
        }
    }

    return total;
}

double BarrowmanDragCalculator::calculateBaseCD(const FlightConfiguration& configuration,
                                                const FlightConditions&    conditions,
                                                ForceMap* forceMap, WarningSet& warnings)
{
    const double base  = calculateBaseCD(conditions.getMach());
    double       total = 0;

    for (const auto& [c, contexts] : configuration.getActiveInstances())
    {
        if (isOverridden(*c))
        {
            continue;
        }

        const int instanceCount = static_cast<int>(contexts.size());

        // Base drag for symmetric components (body tubes, nose cones, transitions)
        if (const SymmetricComponent* const s = asSymmetric(*c))
        {
            if (const std::optional<double> cd = calculateSymmetricComponentBaseCD(
                    configuration, conditions, *s, instanceCount, base))
            {
                total += instanceCount * *cd;
                if (AerodynamicForces* const entry = entryOf(forceMap, *c))
                {
                    entry->setBaseCD(*cd);
                }
            }
        }
        else if (c->isAerodynamic())
        {
            // Base drag for non-symmetric components (fins, etc.)
            const double cd = calcOf(*c).calculateComponentBaseCD(conditions, base, warnings);
            if (cd > 0)
            {
                total += cd * instanceCount;
                if (AerodynamicForces* const entry = entryOf(forceMap, *c))
                {
                    entry->setBaseCD(cd);
                }
            }
        }
    }

    return total;
}

double BarrowmanDragCalculator::calculateStagnationCD(double mach) noexcept
{
    double pressure = 0;
    if (mach <= 1)
    {
        pressure = 1 + (pow2(mach) / 4) + (pow2(pow2(mach)) / 40);
    }
    else
    {
        pressure = 1.84 - (0.76 / pow2(mach)) + (0.166 / pow2(pow2(mach))) +
                   (0.035 / pow2(mach * mach * mach));
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
