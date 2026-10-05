#pragma once

#include <optional>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Quaternion.h"

namespace QtRocket
{

/// The linear and the rotational acceleration of the rocket at one instant (OpenRocket's
/// simulation/AccelerationData), each known in rocket coordinates (RC), in world coordinates
/// (WC) or in both, with the rotation from rocket to world coordinates. A stepper gives the
/// accelerations in the frame it computed them in; the other frame is converted on first use,
/// through the rotation (rotate() from rocket to world coordinates, invRotate() back), and
/// kept.
///
/// Laziness: the getters cache the converted value in the object, as Java's do, so the cache
/// members are mutable; an AccelerationData is therefore not safe to read from two threads at
/// once.
///
/// Deviations from OpenRocket:
/// - A Coordinate that Java passes as null is std::nullopt. Giving neither frame of an
///   acceleration is a programming error: BugError with Java's message (Java:
///   IllegalArgumentException), in which the coordinates and the rotation are printed by this
///   port's toString() (see Quaternion::toString() for how it differs from Java's).
/// - The rotation cannot be null.
/// - hashCode() is not ported (nothing hashes acceleration data).
class AccelerationData
{
public:
    /// @param linearAccelerationRC      the linear acceleration in rocket coordinates, or nullopt
    /// @param rotationalAccelerationRC  the rotational acceleration in rocket coordinates, or
    ///                                  nullopt
    /// @param linearAccelerationWC      the linear acceleration in world coordinates, or nullopt
    /// @param rotationalAccelerationWC  the rotational acceleration in world coordinates, or
    ///                                  nullopt
    /// @param rotation                  the rotation from rocket coordinates to world coordinates
    /// @throws BugError when both linear accelerations, or both rotational accelerations, are
    ///         nullopt
    AccelerationData(const std::optional<Coordinate>& linearAccelerationRC,
                     const std::optional<Coordinate>& rotationalAccelerationRC,
                     const std::optional<Coordinate>& linearAccelerationWC,
                     const std::optional<Coordinate>& rotationalAccelerationWC,
                     const Quaternion&                rotation);

    /// The linear acceleration in rocket coordinates: the one given, else the world one rotated
    /// back (invRotate()), computed once.
    [[nodiscard]] Coordinate getLinearAccelerationRC() const;

    /// The rotational acceleration in rocket coordinates: the one given, else the world one
    /// rotated back (invRotate()), computed once.
    [[nodiscard]] Coordinate getRotationalAccelerationRC() const;

    /// The linear acceleration in world coordinates: the one given, else the rocket one rotated
    /// (rotate()), computed once.
    [[nodiscard]] Coordinate getLinearAccelerationWC() const;

    /// The rotational acceleration in world coordinates: the one given, else the rocket one
    /// rotated (rotate()), computed once.
    [[nodiscard]] Coordinate getRotationalAccelerationWC() const;

    /// The rotation from rocket coordinates to world coordinates.
    [[nodiscard]] const Quaternion& getRotation() const noexcept { return m_rotation; }

    /// Java's equals(): the same object, or equal linear and rotational accelerations in rocket
    /// coordinates (Coordinate's tolerant operator==, which is never true of a NaN: data with a
    /// NaN in it equals itself only as the same object). The world coordinates and the rotation
    /// take part only through the conversion.
    [[nodiscard]] bool operator==(const AccelerationData& other) const;

private:
    mutable std::optional<Coordinate> m_linearAccelerationRC;
    mutable std::optional<Coordinate> m_rotationalAccelerationRC;
    mutable std::optional<Coordinate> m_linearAccelerationWC;
    mutable std::optional<Coordinate> m_rotationalAccelerationWC;
    /// Rotates from rocket coordinates to world coordinates.
    Quaternion m_rotation;
};

}  // namespace QtRocket
