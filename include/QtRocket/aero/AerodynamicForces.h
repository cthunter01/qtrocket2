#pragma once

#include <limits>
#include <string>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"

namespace QtRocket
{

class RocketComponent;

/// The aerodynamic coefficients of a rocket or of one component (OpenRocket's
/// aerodynamics/AerodynamicForces): the CP with CNa, the normal force, pitching moment, side force,
/// yaw moment and roll moment coefficients (the roll split into damping and forcing), the axial
/// and total drag with the total's pressure, base, friction and override parts, the pitch and yaw
/// damping moments, and whether the forces are axisymmetric. Every coefficient starts as NaN.
///
/// The CP and CNa are stored as one weighted coordinate, the moment cpCNa = (x*CNa, y*CNa, z*CNa,
/// CNa): setCP() takes the CP with CNa as its weight, getCP() divides back, and merge() adds the
/// moments, so merging the forces of several components gives their combined CP and CNa.
///
/// The component association: getComponent() is the component the forces belong to (the rocket
/// for the total), or null. With a component, the drag getters account for its CD override:
/// getCD() is 0 when an ancestor overrides it and the override CD when the component itself is
/// overridden (an assembly's stored CD already holds its subtree's overrides), and the pressure,
/// base and friction parts are 0 when the component's CD is overridden, by itself (unless it is an
/// assembly) or by an ancestor; getOverrideCD() is 0 for a component that is neither overridden
/// nor an assembly. The component is a non-owning pointer, valid while it is in its rocket.
///
/// A value type: a copy is Java's clone() (the component pointer and the modification id
/// included). Each setter draws a new modification id when the value changes (exact comparison,
/// so setting NaN always counts as a change), as do zero(), merge() and setAxisymmetric().
class AerodynamicForces
{
public:
    /// Every coefficient NaN, the CP and CNa zero, axisymmetric, no component, the modification
    /// id ModId::invalid().
    AerodynamicForces() = default;

    [[nodiscard]] bool isAxisymmetric() const noexcept { return m_axisymmetric; }
    void               setAxisymmetric(bool isSym);

    /// Associates the forces with @p component (nullptr for none).
    void                                 setComponent(const RocketComponent* component);
    [[nodiscard]] const RocketComponent* getComponent() const noexcept { return m_component; }

    /// Sets the CP and CNa from @p cp, whose weight is CNa: a weight within MathUtil::equals of 0
    /// stores a zero moment. Nothing changes when the new moment equals the stored one
    /// (Coordinate's tolerant operator==).
    void setCP(const Coordinate& cp);

    /// The CP with CNa as its weight; (0, 0, 0, 0) when CNa is (within MathUtil::equals of) 0.
    [[nodiscard]] Coordinate getCP() const noexcept;

    void                 setCN(double cN);
    [[nodiscard]] double getCN() const noexcept { return m_cn; }

    /// The pitching moment coefficient, about the coordinate origin.
    void                 setCm(double cm);
    [[nodiscard]] double getCm() const noexcept { return m_cm; }

    /// The side force coefficient Cy.
    void                 setCside(double cside);
    [[nodiscard]] double getCside() const noexcept { return m_cside; }

    /// The yaw moment coefficient Cn, about the coordinate origin.
    void                 setCyaw(double cyaw);
    [[nodiscard]] double getCyaw() const noexcept { return m_cyaw; }

    /// The roll moment coefficient Cl, about the coordinate origin.
    void                 setCroll(double croll);
    [[nodiscard]] double getCroll() const noexcept { return m_croll; }

    /// The roll damping moment coefficient.
    void                 setCrollDamp(double crollDamp);
    [[nodiscard]] double getCrollDamp() const noexcept { return m_crollDamp; }

    /// The roll forcing moment coefficient.
    void                 setCrollForce(double crollForce);
    [[nodiscard]] double getCrollForce() const noexcept { return m_crollForce; }

    /// The axial drag coefficient CA.
    void                 setCDaxial(double cdaxial);
    [[nodiscard]] double getCDaxial() const noexcept { return m_cdAxial; }

    /// The total drag coefficient, parallel to the airflow.
    void setCD(double cD);
    /// The drag coefficient per instance, with the component's override applied (see the class
    /// comment).
    [[nodiscard]] double getCD() const;

    /// getCD() times the component's instance count.
    /// @throws BugError without a component (Java: NullPointerException).
    [[nodiscard]] double getCDTotal() const;

    /// The pressure drag coefficient (fore pressure).
    void                 setPressureCD(double pressureCD);
    [[nodiscard]] double getPressureCD() const;

    /// The base drag coefficient.
    void                 setBaseCD(double baseCD);
    [[nodiscard]] double getBaseCD() const;

    /// The friction drag coefficient.
    void                 setFrictionCD(double frictionCD);
    [[nodiscard]] double getFrictionCD() const;

    /// The drag coefficient from CD overrides.
    void                 setOverrideCD(double overrideCD);
    [[nodiscard]] double getOverrideCD() const;

    void                 setPitchDampingMoment(double pitchDampingMoment);
    [[nodiscard]] double getPitchDampingMoment() const noexcept { return m_pitchDampingMoment; }

    void                 setYawDampingMoment(double yawDampingMoment);
    [[nodiscard]] double getYawDampingMoment() const noexcept { return m_yawDampingMoment; }

    /// Clears the component and sets CN, Cm, Cside, Cyaw, Croll, CrollDamp, CrollForce, CDaxial,
    /// CD and the damping moments to NaN; the pressure, base, friction and override CDs and the
    /// axisymmetric flag are left as they are, as in Java. Deviation: Java then calls
    /// setCP(null), which throws a NullPointerException (no caller in OpenRocket reaches it);
    /// here the CP and CNa become NaN.
    void reset();

    /// Sets every coefficient to 0 (CP and CNa to Coordinate::kNul, the pressure, base, friction
    /// and override CDs excepted, as in Java) and the forces axisymmetric; the component stays.
    /// Returns *this.
    AerodynamicForces& zero() &;

    /// zero() on a temporary, returned by value: Java's `new AerodynamicForces().zero()` idiom is
    /// `AerodynamicForces{}.zero()`, and a reference can never be bound to the dead temporary.
    [[nodiscard]] AerodynamicForces zero() &&;

    /// Adds @p other's CP moment (cpCNa, weights summed), CN, Cm, Cside, Cyaw, Croll, CrollDamp and
    /// CrollForce to these; the drag, damping moments, component and axisymmetric flag stay.
    /// Returns *this.
    AerodynamicForces& merge(const AerodynamicForces& other) &;

    /// merge() on a temporary, returned by value (as zero() &&).
    [[nodiscard]] AerodynamicForces merge(const AerodynamicForces& other) &&;

    /// The id of the current state (Monitorable).
    [[nodiscard]] ModId modId() const noexcept { return m_modId; }

    /// Java's equals(): CN, Cm, Cside, Cyaw, Croll, CrollDamp, CrollForce, CDaxial, CD, the
    /// pressure, base and friction CDs and the damping moments, all through their getters (the
    /// override logic included), within MathUtil::equals (so never for a NaN), and the CPs equal
    /// (Coordinate's tolerant operator==). The override CD, component, axisymmetric flag and
    /// modification id do not count. An object always equals itself.
    [[nodiscard]] bool operator==(const AerodynamicForces& other) const;

    /// Java's hashCode(): (int)(1000 * (getCD() + getCDaxial() + getCP().weight)) plus the CP's
    /// hash code, in Java's int arithmetic.
    [[nodiscard]] int hashCode() const;

    /// Java's toString(): "AerodynamicForces[" then "component:<name>," with a component,
    /// "cp:<Coordinate::toString()>," (Java's %.5f), and "CN:", "Cm:", "Cside:", "Cyaw:",
    /// "Croll:", "CDaxial:", "CD:" with each value that is not NaN (Java's Double.toString), the
    /// last comma dropped, and "]".
    [[nodiscard]] std::string toString() const;

private:
    static constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

    /// Whether the component's CD is overridden by itself (unless it is an assembly) or by an
    /// ancestor: the pressure, base and friction CDs are then 0.
    [[nodiscard]] bool dragPartsOverridden() const;

    /// Sets @p field to @p value and draws a new id, unless they are exactly equal.
    void set(double& field, double value);

    const RocketComponent* m_component{nullptr};
    Coordinate             m_cpCNa{Coordinate::kZero};
    double                 m_cn{kNaN};
    double                 m_cm{kNaN};
    double                 m_cside{kNaN};
    double                 m_cyaw{kNaN};
    double                 m_croll{kNaN};
    double                 m_crollDamp{kNaN};
    double                 m_crollForce{kNaN};
    double                 m_cdAxial{kNaN};
    double                 m_cd{kNaN};
    double                 m_pressureCD{kNaN};
    double                 m_baseCD{kNaN};
    double                 m_frictionCD{kNaN};
    double                 m_overrideCD{kNaN};
    double                 m_pitchDampingMoment{kNaN};
    double                 m_yawDampingMoment{kNaN};
    ModId                  m_modId{ModId::invalid()};
    bool                   m_axisymmetric{true};
};

}  // namespace QtRocket
