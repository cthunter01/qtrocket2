#pragma once

#include <memory>
#include <numbers>
#include <vector>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

/// A set of trapezoidal fins (OpenRocket's TrapezoidFinSet): the root and tip chords are parallel
/// to the rocket's axis, and the leading and trailing edges may be slanted.
///
///            sweep   tip chord
///            |    |___________
///            |   /            |
///            |  /             |
///            | /              |  height
///             /               |
///  __________/________________|_____________
///                 length
///               == root chord
///
/// The root chord is the component's length. The outline has four points, or three when the tip
/// chord is 0.1 mm or less (a triangle); a root chord below 0.1 mm is drawn as 0.1 mm.
///
/// Kept from OpenRocket: setFinShape() and setSweep() store their values as given (no clamping,
/// and setFinShape() does not move the tab), while setRootChord(), setTipChord() and setHeight()
/// turn a negative value into 0.
///
/// Deviation from OpenRocket: the multi-edit config listeners are not ported (see
/// RocketComponent).
class TrapezoidFinSet : public FinSet
{
public:
    /// The largest sweep angle setSweepAngle() accepts either way, 89 degrees (MAX_SWEEP_ANGLE).
    static constexpr double kMaxSweepAngle = 89 * std::numbers::pi / 180.0;

    /// Three fins: root and tip chord 0.05 m, sweep 0.025 m, height 0.03 m.
    TrapezoidFinSet();

    /// A set of @p fins fins (limited to 1 ... 8) of the given dimensions, stored as given.
    TrapezoidFinSet(int fins, double rootChord, double tipChord, double sweep, double height);

    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::TRAPEZOID_FIN_SET;
    }

    /// Sets all the dimensions and the thickness at once, as given, and fires AEROMASS_CHANGE,
    /// unless every one of them is exactly the current value.
    void setFinShape(double rootChord, double tipChord, double sweep, double height,
                     double thickness);

    /// The root chord: the component's length.
    [[nodiscard]] double getRootChord() const noexcept { return m_length; }

    /// Sets the root chord (negative values become 0, Java's Math.max), recomputes the tab
    /// position and fires AEROMASS_CHANGE, unless it is exactly the current chord.
    void setRootChord(double r);

    [[nodiscard]] double getTipChord() const noexcept { return m_tipChord; }

    /// Sets the tip chord (negative values become 0) and fires AEROMASS_CHANGE, unless it is
    /// exactly the current chord.
    void setTipChord(double r);

    /// The sweep length: how far aft of the root's leading edge the tip's leading edge is.
    [[nodiscard]] double getSweep() const noexcept { return m_sweep; }

    /// Sets the sweep length and fires AEROMASS_CHANGE, unless it is exactly the current one.
    void setSweep(double r);

    /// The sweep angle, atan2(sweep, height); it is not stored. With no height: pi / 2 for a
    /// positive sweep, -pi / 2 for a negative one, else 0.
    [[nodiscard]] double getSweepAngle() const;

    /// Sets the sweep length that gives the sweep angle @p r (limited to +-kMaxSweepAngle) at
    /// the current height; nothing happens when that length is NaN or infinite.
    void setSweepAngle(double r);

    [[nodiscard]] double getHeight() const noexcept { return m_height; }

    /// Sets the height (negative values become 0) and fires AEROMASS_CHANGE, unless it is
    /// exactly the current height.
    void setHeight(double r);

    /// (0, 0), (sweep, height), (sweep + tip chord, height) unless the tip chord is 0.1 mm or
    /// less, and (max(root chord, 0.1 mm), 0); the first and the last point lie on the ends of
    /// the root when it has more than one point.
    [[nodiscard]] std::vector<Coordinate> getFinPoints() const override;

    /// The height.
    [[nodiscard]] double getSpan() const override;

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;

private:
    // The root chord is the length.
    double m_tipChord{0};
    double m_height{0};
    double m_sweep{0};
};

}  // namespace QtRocket
