#pragma once

#include <string>

#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

/// A mass with its centre and a diagonal moment of inertia (OpenRocket's masscalc/RigidBody): the
/// centre of mass is a Coordinate whose weight is the mass (kg), and Ixx (about the rocket's
/// axis, the rotational inertia), Iyy and Izz (the longitudinal inertias) are taken about the
/// centre of mass unless rebase() moved the reference point.
///
/// An immutable value: every operation returns a new body. Java's public final fields cm, Ixx,
/// Iyy and Izz are read through the getters.
///
/// Error policy: a negative inertia is a BugError, as Java's BugException ("attempted to
/// initialize an InertiaMatrix with a negative inertia value"); the calculations never produce
/// one from valid input. A NaN passes, as in Java.
///
/// Not ported: hashCode(), a constant 1 in Java (equality is tolerant, so nothing better is
/// consistent with it): a RigidBody is a computed value, never a key.
class RigidBody
{
public:
    /// EMPTY: no mass at the origin, no inertia.
    static const RigidBody kEmpty;

    /// A body at @p cm (weight = mass) with the axial inertia @p axialInertia (Ixx) and the
    /// longitudinal inertia @p longitudinalInertia for both Iyy and Izz.
    /// @throws BugError when an inertia is negative.
    RigidBody(const Coordinate& cm, double axialInertia, double longitudinalInertia);

    /// A body at @p cm (weight = mass) with the inertias @p ixx, @p iyy and @p izz.
    /// @throws BugError when an inertia is negative.
    RigidBody(const Coordinate& cm, double ixx, double iyy, double izz);

    /// The two bodies combined: the centre of mass is the mass-weighted average of the two
    /// (Coordinate::average), and each inertia the sum of the two after rebase() to that point
    /// (the parallel axis theorem).
    [[nodiscard]] RigidBody add(const RigidBody& that) const;

    /// A copy with the mass and all three inertias multiplied by the non-negative @p factor: for
    /// a fixed geometry the inertia scales with the mass, which keeps the body consistent under
    /// rebase(), whose parallel-axis term uses the mass.
    [[nodiscard]] RigidBody scaleMass(double factor) const;

    /// The centre of mass, its weight the mass.
    [[nodiscard]] constexpr const Coordinate& getCenterOfMass() const noexcept { return m_cm; }
    /// The same as getCenterOfMass().
    [[nodiscard]] constexpr const Coordinate& getCM() const noexcept { return m_cm; }

    [[nodiscard]] constexpr double getIxx() const noexcept { return m_ixx; }
    [[nodiscard]] constexpr double getIyy() const noexcept { return m_iyy; }
    [[nodiscard]] constexpr double getIzz() const noexcept { return m_izz; }

    /// Iyy.
    [[nodiscard]] constexpr double getLongitudinalInertia() const noexcept { return m_iyy; }
    /// The mass, the weight of the centre of mass.
    [[nodiscard]] constexpr double getMass() const noexcept { return m_cm.weight; }
    /// Ixx.
    [[nodiscard]] constexpr double getRotationalInertia() const noexcept { return m_ixx; }

    /// Whether this body equals kEmpty (operator==, so within MathUtil::kEpsilon).
    [[nodiscard]] bool isEmpty() const noexcept;

    /// Java's equals(): the centres of mass are equal (Coordinate::operator==, mass included)
    /// and each inertia agrees within MathUtil::kEpsilon.
    [[nodiscard]] bool operator==(const RigidBody& other) const noexcept;

    /// The inertias moved from the centre of mass to @p newLocation by the parallel axis theorem,
    /// I + m d^2 with d the distance from the axis through the new point: Ixx + m (dy^2 + dz^2),
    /// Iyy + m (dx^2 + dz^2), Izz + m (dx^2 + dy^2). The new body's centre is @p newLocation as
    /// given, weight included (Java keeps it too, so the result usually has no mass).
    [[nodiscard]] RigidBody rebase(const Coordinate& newLocation) const;

    /// rebase() to the centre of mass moved by @p delta (Coordinate::add, which adds the weights
    /// too): a simplified parallel axis translation that keeps the inertia diagonal.
    [[nodiscard]] RigidBody translateInertia(const Coordinate& delta) const;

    /// toCMString() + " // " + toMOIString().
    [[nodiscard]] std::string toString() const;

    /// "CoM: <mass>g @[<x>,<y>,<z>]" with eight decimals each, as Java's String.format("%.8f")
    /// in an English locale (Strings::formatFixed; NaN, Infinity and -0.00000000 included).
    [[nodiscard]] std::string toCMString() const;

    /// "MOI: [ <Ixx>, <Iyy>, <Izz>]" with eight decimals each.
    [[nodiscard]] std::string toMOIString() const;

private:
    struct Unchecked
    { };

    /// For kEmpty: constant initialisation without the check (the values are valid).
    constexpr RigidBody(Unchecked /*tag*/, const Coordinate& cm, double ixx, double iyy,
                        double izz) noexcept
      : m_cm(cm), m_ixx(ixx), m_iyy(iyy), m_izz(izz)
    {
    }

    Coordinate m_cm;
    double     m_ixx;
    double     m_iyy;
    double     m_izz;
};

inline constexpr RigidBody RigidBody::kEmpty{Unchecked{}, Coordinate::kZero, 0.0, 0.0, 0.0};

}  // namespace QtRocket
