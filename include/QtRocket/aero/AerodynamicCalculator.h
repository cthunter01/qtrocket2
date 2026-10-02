#pragma once

#include <memory>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"

namespace QtRocket
{

class FlightConditions;
class FlightConfiguration;
class RocketComponent;
class WarningSet;

/// Aerodynamic calculations on a rocket (OpenRocket's aerodynamics/AerodynamicCalculator):
/// the CP, the total forces, the per-component force analysis, the worst CP over the lateral wind
/// directions, the stall angle and the geometry checks. BarrowmanCalculator is the implementation;
/// AbstractAerodynamicCalculator holds the shared logic.
///
/// As in Java, @p warnings may be null (the warnings are then dropped). The calculations are not
/// const: an implementation caches per-component data, voided when the rocket's aerodynamic or
/// tree modification id changes. A calculator belongs to one thread at a time; a simulation takes
/// its own newInstance().
///
/// Monitorable: modId() (Java's getModID()).
class AerodynamicCalculator
{
public:
    virtual ~AerodynamicCalculator() = default;

    AerodynamicCalculator& operator=(const AerodynamicCalculator&) = delete;
    AerodynamicCalculator& operator=(AerodynamicCalculator&&)      = delete;

    /// The largest stall angle in radians.
    [[nodiscard]] virtual double getStallAngle() const = 0;

    /// The CP of @p configuration in @p conditions, in absolute coordinates, with CNa as its
    /// weight.
    [[nodiscard]] virtual Coordinate getCP(const FlightConfiguration& configuration,
                                           const FlightConditions&    conditions,
                                           WarningSet*                warnings) = 0;

    /// The aerodynamic forces acting on the rocket.
    [[nodiscard]] virtual AerodynamicForces getAerodynamicForces(
        const FlightConfiguration& configuration, const FlightConditions& conditions,
        WarningSet* warnings) = 0;

    /// The aerodynamic forces per component, in the order of the active instances; the rocket's
    /// entry holds the total forces.
    [[nodiscard]] virtual ForceMap getForceAnalysis(const FlightConfiguration& configuration,
                                                    const FlightConditions&    conditions,
                                                    WarningSet*                warnings) = 0;

    /// The worst (foremost) CP over every lateral wind direction; the theta that gives it is set
    /// in @p conditions. Not [[nodiscard]]: a caller may want only that side effect (OpenRocket's
    /// component analysis calls it to set the worst theta and drops the CP).
    virtual Coordinate getWorstCP(const FlightConfiguration& configuration,
                                  FlightConditions& conditions, WarningSet* warnings) = 0;

    /// A new, independent calculator of the same type (Java: newInstance()).
    [[nodiscard]] virtual std::unique_ptr<AerodynamicCalculator> newInstance() const = 0;

    /// Checks the component tree under @p component for geometric problems and adds the
    /// warnings to @p warnings (when not null).
    virtual void checkGeometry(const FlightConfiguration& configuration,
                               const RocketComponent& component, WarningSet* warnings) = 0;

    /// The id of the calculator's state (Monitorable; Java's getModID()).
    [[nodiscard]] virtual ModId modId() const = 0;

protected:
    AerodynamicCalculator()                             = default;
    AerodynamicCalculator(const AerodynamicCalculator&) = default;
    AerodynamicCalculator(AerodynamicCalculator&&)      = default;
};

}  // namespace QtRocket
