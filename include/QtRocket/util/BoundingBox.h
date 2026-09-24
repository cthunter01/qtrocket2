#pragma once

#include <array>
#include <iosfwd>
#include <span>
#include <string>
#include <vector>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket
{

class Transformation;

/// The axis-aligned box around a set of coordinates, kept as its minimum and maximum corners,
/// ported from OpenRocket's BoundingBox. A new box is empty: its minimum corner is at +max
/// double and its maximum corner at -max double on every axis, so that the first update() sets
/// both. update() only ever grows the box, and a NaN component never changes it (no comparison
/// with NaN holds).
///
/// The corners keep the weight they were constructed with (update() changes only x, y and z,
/// and a new box's corners are unweighted), and that weight takes part in operator== and in
/// span() (Coordinate::sub keeps the maximum corner's weight), exactly as in OpenRocket.
///
/// transform() is not exact: it boxes the two transformed corners, not the transformed box, so
/// a rotated box can be too small. OpenRocket uses it for the UI only; keep it out of aerodynamic,
/// mass and simulation calculations. transform() an empty box only after checking isEmpty(): its
/// +/-max corners are transformed like any point, so even the identity turns an empty box into
/// the box of the whole space (-max to +max), and a rotation overflows them to infinity
/// (OpenRocket's FlightConfiguration.calculateBounds checks isEmpty() first for that reason).
///
/// Deviations from OpenRocket:
/// - min and max are public fields there; here they are read through min() and max().
/// - clone() is not ported (a copy is a clone), update(CoordinateIF[]) and update(Collection)
///   are one update(std::span<const Coordinate>), and toRectangle() returns a Rectangle2D
///   (Geometry2D.h), the stand-in for java.awt.geom.Rectangle2D.
/// - toString() rounds differently; see there.
class BoundingBox
{
public:
    /// An empty box.
    constexpr BoundingBox() noexcept = default;

    /// The box with the given corners, kept as given (weights included, and whether or not
    /// @p minimum is below @p maximum).
    constexpr BoundingBox(const Coordinate& minimum, const Coordinate& maximum) noexcept
      : m_min{minimum}, m_max{maximum}
    {
    }

    /// Makes the box empty again.
    constexpr void clear() noexcept
    {
        m_min = kEmptyMin;
        m_max = kEmptyMax;
    }

    /// The corner with the smallest x, y and z.
    [[nodiscard]] constexpr const Coordinate& min() const noexcept { return m_min; }

    /// The corner with the largest x, y and z.
    [[nodiscard]] constexpr const Coordinate& max() const noexcept { return m_max; }

    /// True when the minimum corner lies beyond the maximum on any axis (so for a new box).
    [[nodiscard]] constexpr bool isEmpty() const noexcept
    {
        return (m_min.x > m_max.x) || (m_min.y > m_max.y) || (m_min.z > m_max.z);
    }

    /// A new box around the two transformed corners of this one (see the class comment: not
    /// exact under a rotation).
    [[nodiscard]] BoundingBox transform(const Transformation& transformation) const noexcept;

    /// Grows the box to include the point (value, value, value). Returns this box.
    constexpr BoundingBox& update(double value) noexcept
    {
        updateXMin(value);
        updateYMin(value);
        updateZMin(value);

        updateXMax(value);
        updateYMax(value);
        updateZMax(value);
        return *this;
    }

    /// Grows the box to include @p c (its weight is ignored). Returns this box.
    constexpr BoundingBox& update(const Coordinate& c) noexcept
    {
        updateXMin(c.x);
        updateYMin(c.y);
        updateZMin(c.z);

        updateXMax(c.x);
        updateYMax(c.y);
        updateZMax(c.z);
        return *this;
    }

    /// Grows the box in x and y to include @p rect (from (x, y) to (x + width, y + height), as
    /// Rectangle2D's getMinX/getMaxX and getMinY/getMaxY); z is untouched. Returns this box.
    constexpr BoundingBox& update(const Rectangle2D& rect) noexcept
    {
        updateXMin(rect.x);
        updateYMin(rect.y);
        updateXMax(rect.x + rect.width);
        updateYMax(rect.y + rect.height);
        return *this;
    }

    /// Grows the box to include every coordinate of @p list. Returns this box.
    constexpr BoundingBox& update(std::span<const Coordinate> list) noexcept
    {
        for (const Coordinate& c : list)
        {
            update(c);
        }
        return *this;
    }

    /// Grows the box to include @p other; an empty @p other changes nothing. Returns this box.
    constexpr BoundingBox& update(const BoundingBox& other) noexcept
    {
        if (other.isEmpty())
        {
            return *this;
        }
        updateXMin(other.m_min.x);
        updateYMin(other.m_min.y);
        updateZMin(other.m_min.z);

        updateXMax(other.m_max.x);
        updateYMax(other.m_max.y);
        updateZMax(other.m_max.z);
        return *this;
    }

    /// max() - min(): the extent along each axis, with the maximum corner's weight.
    [[nodiscard]] constexpr Coordinate span() const noexcept { return m_max.sub(m_min); }

    /// {min(), max()}.
    [[nodiscard]] constexpr std::array<Coordinate, 2> toArray() const noexcept
    {
        return {m_min, m_max};
    }

    /// {max(), min()}: the maximum corner first, as OpenRocket's toCollection() lists them.
    [[nodiscard]] std::vector<Coordinate> toCollection() const;

    /// The x-y footprint: (min x, min y) with width() and height().
    [[nodiscard]] constexpr Rectangle2D toRectangle() const noexcept
    {
        return Rectangle2D{
            .x = m_min.x, .y = m_min.y, .width = m_max.x - m_min.x, .height = m_max.y - m_min.y};
    }

    /// The extent along x (Java: getWidth).
    [[nodiscard]] constexpr double width() const noexcept { return m_max.x - m_min.x; }

    /// The extent along y (Java: getHeight).
    [[nodiscard]] constexpr double height() const noexcept { return m_max.y - m_min.y; }

    /// "[( x, y, z) < ( x, y, z)]" with the corners in Java's %g: six significant digits,
    /// trailing zeros kept, decimal notation from 1e-4 up to 1e6 ("0.000100000", "1.00000",
    /// "123456") and otherwise "d.ddddde+XX"; NaN, Infinity and -Infinity as Java spells them.
    /// Deviation: the digits come from std::format, which rounds the exact binary value half to
    /// even where Java's Formatter rounds the shortest round-trip decimal half up (see
    /// Coordinate::toString()). For people and test output only; nothing written to a file goes
    /// through this.
    [[nodiscard]] std::string toString() const;

    /// OpenRocket's equality: both corners equal as Coordinates (tolerant, weight included). As
    /// in Java's other.min.equals(this.min), the relative tolerance is taken against this box's
    /// corners, so a == b and b == a can differ right at the tolerance edge.
    [[nodiscard]] bool operator==(const BoundingBox& other) const noexcept;

private:
    static constexpr Coordinate kEmptyMin = Coordinate::kMax.setWeight(0.0);
    static constexpr Coordinate kEmptyMax = Coordinate::kMin.setWeight(0.0);

    constexpr void updateXMin(double value) noexcept
    {
        if (m_min.x > value)
        {
            m_min = m_min.setX(value);
        }
    }

    constexpr void updateYMin(double value) noexcept
    {
        if (m_min.y > value)
        {
            m_min = m_min.setY(value);
        }
    }

    constexpr void updateZMin(double value) noexcept
    {
        if (m_min.z > value)
        {
            m_min = m_min.setZ(value);
        }
    }

    constexpr void updateXMax(double value) noexcept
    {
        if (m_max.x < value)
        {
            m_max = m_max.setX(value);
        }
    }

    constexpr void updateYMax(double value) noexcept
    {
        if (m_max.y < value)
        {
            m_max = m_max.setY(value);
        }
    }

    constexpr void updateZMax(double value) noexcept
    {
        if (m_max.z < value)
        {
            m_max = m_max.setZ(value);
        }
    }

    Coordinate m_min{kEmptyMin};
    Coordinate m_max{kEmptyMax};
};

/// Writes toString(), so that GoogleTest prints a readable value when EXPECT_EQ on two boxes
/// fails.
std::ostream& operator<<(std::ostream& os, const BoundingBox& box);

}  // namespace QtRocket
