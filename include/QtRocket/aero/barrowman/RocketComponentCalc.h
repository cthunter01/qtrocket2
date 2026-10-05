#pragma once

namespace QtRocket
{

class AerodynamicForces;
class FlightConditions;
class RocketComponent;
class Transformation;
class WarningSet;

/// The aerodynamic calculation of one component for the extended Barrowman method (OpenRocket's
/// aerodynamics/barrowman/RocketComponentCalc): its non-axial forces, its friction, pressure and
/// base drag. A calculator holds one per aerodynamic component and assembly (in a
/// ComponentCalcMap), made by CalcFactory::create(), a kind() switch over the component
/// (SymmetricComponentCalc, FinSetCalc, TubeFinSetCalc, LaunchLugCalc, RailButtonCalc,
/// ComponentAssemblyCalc), which copies the geometry it needs at construction.
///
/// It also holds the CP position helpers shared by the fin calculators: the position along the
/// mean aerodynamic chord at subsonic, transonic and supersonic speeds.
///
/// The calculation methods are not const, since an implementation may cache values between
/// calls. Java's null WarningSet is resolved by the calculator, so warnings are a reference.
class RocketComponentCalc
{
public:
    virtual ~RocketComponentCalc() = default;

    RocketComponentCalc(const RocketComponentCalc&)            = delete;
    RocketComponentCalc& operator=(const RocketComponentCalc&) = delete;
    RocketComponentCalc(RocketComponentCalc&&)                 = delete;
    RocketComponentCalc& operator=(RocketComponentCalc&&)      = delete;

    /// Calculates the non-axial forces of the component (normal and side forces, pitch, yaw and
    /// roll moments, and the CP) into @p forces, in local coordinates with the moments about the
    /// local origin; CNa as well when it comes at little extra cost, otherwise NaN. @p transform
    /// is the instance's transformation (from the InstanceMap), which gives its rotation.
    virtual void calculateNonaxialForces(const FlightConditions& conditions,
                                         const Transformation& transform, AerodynamicForces& forces,
                                         WarningSet& warnings) = 0;

    /// The friction drag coefficient of the component, for the coefficient of friction
    /// @p componentCf that the drag calculator worked out.
    [[nodiscard]] virtual double calculateFrictionCD(const FlightConditions& conditions,
                                                     double componentCf, WarningSet& warnings) = 0;

    /// The pressure drag coefficient of the component, without the effect of body
    /// discontinuities and without the base drag (calculateComponentBaseCD()).
    [[nodiscard]] virtual double calculatePressureCD(const FlightConditions& conditions,
                                                     double stagnationCD, double baseCD,
                                                     WarningSet& warnings) = 0;

    /// The base (trailing edge) drag coefficient of the component; 0 unless the component has a
    /// meaningful base drag (fins).
    [[nodiscard]] virtual double calculateComponentBaseCD(const FlightConditions& conditions,
                                                          double baseCD, WarningSet& warnings);

    /// The Reynolds number for the characteristic length @p length: velocity * length /
    /// kinematic viscosity of the atmospheric conditions. (Java's is an instance method that
    /// uses no state; static here.)
    [[nodiscard]] static double calculateReynoldsNumber(double                  length,
                                                        const FlightConditions& conditions);

protected:
    /// The CP position along the MAC at subsonic speeds, as a fraction of the MAC
    /// (SUBSONIC_CP_POS).
    static constexpr double kSubsonicCpPos = 0.25;

    /// Java's constructor takes the component and ignores it; so does this one. A subclass reads
    /// its geometry from it.
    explicit RocketComponentCalc(const RocketComponent& component);

    /// The CP position along the MAC at supersonic speeds, as a fraction of the MAC, for the
    /// aspect ratio times beta @p arBeta: Fleeman's (arBeta - 0.67) / (2 arBeta - 1) from its
    /// validity boundary arBeta = 1 (NACA Report 1307, equation 63) up; kSubsonicCpPos at and
    /// below (0.67 - 0.25) / (1 - 0.5) = 0.84, where the formula equals it; and between the two a
    /// cubic Hermite bridge that matches the value and slope at both ends.
    [[nodiscard]] static double supersonicCPPos(double arBeta) noexcept;

    /// The CP position along the MAC between Mach 0.5 and 2, for @p aspectRatio: kSubsonicCpPos
    /// when the Mach-2 supersonic position is not aft of it; otherwise the fifth-order
    /// interpolation that matches value and slope at Mach 2 when that stays monotone (normalised
    /// endpoint slope over displacement at most 5/3), else the monotone limiting quintic raised to
    /// the power that gives the endpoint slope.
    [[nodiscard]] static double transonicCPPos(double mach, double aspectRatio) noexcept;

private:
    static constexpr double kTransonicCpStartMach = 0.5;
    static constexpr double kTransonicCpEndMach   = 2.0;

    /// The offset in the numerator of the approximate supersonic CP formula
    /// f(x) = (x - offset) / (2x - 1), x = ar * beta. Fleeman rounds the theoretical 2/3 to 0.67;
    /// OpenRocket keeps the rounded value (SUPERSONIC_CP_OFFSET).
    static constexpr double kSupersonicCpOffset = 0.67;

    /// NACA Report 1307, equation (63): the supersonic expression is valid only for ar * beta > 1
    /// (SUPERSONIC_CP_VALIDITY_BOUNDARY).
    static constexpr double kSupersonicCpValidityBoundary = 1.0;

    /// The start of the bridge between the quarter chord and the validity boundary, where the
    /// rounded formula equals kSubsonicCpPos: (offset - subsonicCP) / (1 - 2 subsonicCP) = 0.84
    /// (LOW_AR_CP_BRIDGE_START).
    static constexpr double kLowArCpBridgeStart =
        (kSupersonicCpOffset - kSubsonicCpPos) / (1 - (2 * kSubsonicCpPos));

    /// The largest normalised endpoint slope for which the fifth-order transonic interpolation is
    /// monotone: its quadratic coefficient delta * (10 - 6 * slopeRatio) turns negative above 5/3
    /// (MONOTONIC_QUINTIC_SLOPE_RATIO).
    static constexpr double kMonotonicQuinticSlopeRatio = 5.0 / 3.0;

    /// The source expression (arBeta - 0.67) / (2 arBeta - 1).
    [[nodiscard]] static double sourceSupersonicCPPos(double arBeta) noexcept;

    /// The source expression's derivative with respect to arBeta.
    [[nodiscard]] static double sourceSupersonicCPGradient(double arBeta) noexcept;

    /// The derivative of supersonicCPPos().
    [[nodiscard]] static double supersonicCPGradient(double arBeta) noexcept;
};

}  // namespace QtRocket
