#pragma once

#include <memory>
#include <vector>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

/// A set of elliptical fins (OpenRocket's EllipticalFinSet): each fin is half an ellipse whose
/// one axis is the root chord (the component's length) and whose other half-axis is the height.
/// The outline is a polygon of kPoints points at equal steps of the ellipse's parameter, from
/// the leading edge of the root (0, 0) over the tip to the trailing edge (length, 0).
///
/// Kept from OpenRocket: setHeight() and setLength() store their values as given (negative values
/// too), and a root chord below 0.1 mm is drawn as 0.1 mm.
///
/// Deviation from OpenRocket: the multi-edit config listeners are not ported (see
/// RocketComponent).
class EllipticalFinSet : public FinSet
{
public:
    /// The number of points of the outline (POINTS).
    static constexpr int kPoints = 31;

    /// Three fins with a root chord of 0.05 m and a height of 0.05 m.
    EllipticalFinSet();

    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::ELLIPTICAL_FIN_SET;
    }

    /// Point i is ((cos(a) + 1) / 2 * length, sin(a) * height) with a = pi (30 - i) / 30, the
    /// first and the last point exactly (0, 0) and (length, 0); the two lie on the ends of the
    /// root when it has more than one point.
    [[nodiscard]] std::vector<Coordinate> getFinPoints() const override;

    /// The height.
    [[nodiscard]] double getSpan() const override;

    [[nodiscard]] double getHeight() const noexcept { return m_height; }

    /// Sets the height and fires AEROMASS_CHANGE, unless it equals the current height
    /// (MathUtil::equals).
    void setHeight(double height);

    /// Sets the root chord, shortens a tab that would end behind the fin
    /// (validateFinTabLength()) and fires AEROMASS_CHANGE, unless it equals the current length
    /// (MathUtil::equals).
    void setLength(double length);

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;

private:
    double m_height{0.05};
};

}  // namespace QtRocket
