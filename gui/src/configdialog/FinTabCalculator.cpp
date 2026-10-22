#include "QtRocket/gui/configdialog/FinTabCalculator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <optional>
#include <span>
#include <vector>

#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::Gui::FinTabCalculator
{

namespace
{

/// A ring as computeFinTabLength() sorts it: the ring and its position from the top of its parent.
struct PlacedRing
{
    const CenteringRing* ring;
    double               positionFromTop;
};

/// OpenRocket's comparator of two rings: (int) (1000.0d * (a's top - b's top)), of which only the
/// sign counts. Java's (int) truncates towards zero (and makes 0 of a NaN), so rings less than
/// 1 mm apart compare equal.
[[nodiscard]] int compareRings(const PlacedRing& a, const PlacedRing& b) noexcept
{
    const double difference = 1000.0 * (a.positionFromTop - b.positionFromTop);
    if (std::isnan(difference) || std::abs(difference) < 1.0)
    {
        return 0;
    }
    return difference < 0 ? -1 : 1;
}

/// The length of the run at the start of @p rings, made ascending (Java's
/// TimSort.countRunAndMakeAscending() over the whole list): a strictly descending run is reversed.
[[nodiscard]] std::size_t countRunAndMakeAscending(std::vector<PlacedRing>& rings)
{
    const std::size_t hi    = rings.size();
    std::size_t       runHi = 1;
    if (runHi == hi)
    {
        return 1;
    }
    if (compareRings(rings[runHi++], rings[0]) < 0)
    {
        // Descending
        while (runHi < hi && compareRings(rings[runHi], rings[runHi - 1]) < 0)
        {
            ++runHi;
        }
        std::reverse(rings.begin(), std::next(rings.begin(), static_cast<std::ptrdiff_t>(runHi)));
    }
    else
    {
        // Ascending
        while (runHi < hi && compareRings(rings[runHi], rings[runHi - 1]) >= 0)
        {
            ++runHi;
        }
    }
    return runHi;
}

/// Sorts @p rings as Java's List.sort() sorts fewer than 32 elements with OpenRocket's comparator
/// (TimSort.sort(): the first run, then TimSort.binarySort() of the rest); see the namespace
/// comment for more.
void sortAsJava(std::vector<PlacedRing>& rings)
{
    if (rings.size() < 2)
    {
        return;
    }
    std::size_t start = countRunAndMakeAscending(rings);
    for (; start < rings.size(); ++start)
    {
        const PlacedRing pivot = rings[start];
        std::size_t      left  = 0;
        std::size_t      right = start;
        while (left < right)
        {
            const std::size_t mid = (left + right) / 2;
            if (compareRings(pivot, rings[mid]) < 0)
            {
                right = mid;
            }
            else
            {
                left = mid + 1;
            }
        }
        for (std::size_t i = start; i > left; --i)
        {
            rings[i] = rings[i - 1];
        }
        rings[left] = pivot;
    }
}

/// A centering ring, or several that overlap or touch merged into one (OpenRocket's
/// FinSetConfig.SortableRing).
class SortableRing
{
public:
    explicit SortableRing(const PlacedRing& placed)
      : m_thickness(placed.ring->getLength()), m_positionFromTop(placed.positionFromTop)
    {
    }

    /// Merges an adjacent ring.
    void merge(const PlacedRing& adjacent)
    {
        const double v = adjacent.positionFromTop;
        if (m_positionFromTop < v)
        {
            m_thickness = v + adjacent.ring->getLength() - m_positionFromTop;
        }
        else
        {
            const double tmp  = m_positionFromTop + m_thickness;
            m_positionFromTop = v;
            m_thickness       = tmp - v;
        }
    }

    /// The position of the bottom edge of the ring, from the top of the parent.
    [[nodiscard]] double bottomSidePositionFromTop() const noexcept
    {
        return m_positionFromTop + m_thickness;
    }

    /// The position of the top edge of the ring, from the top of the parent.
    [[nodiscard]] double positionFromTop() const noexcept { return m_positionFromTop; }

private:
    /// The length of the ring (more commonly called the thickness).
    double m_thickness;
    /// The position of the ring from the top of the parent.
    double m_positionFromTop;
};

/// The rings sorted from the top, merged where they overlap or touch.
[[nodiscard]] std::vector<SortableRing> sortedRings(std::span<const CenteringRing* const> rings)
{
    std::vector<PlacedRing> placed;
    placed.reserve(rings.size());
    for (const CenteringRing* ring : rings)
    {
        placed.push_back({.ring = ring, .positionFromTop = ring->getAxialOffset(AxialMethod::TOP)});
    }
    // Sort rings from top of parent to bottom
    sortAsJava(placed);

    std::vector<SortableRing> positionsFromTop;
    for (const PlacedRing& centeringRing : placed)
    {
        // Handle centering rings that overlap or are adjacent by synthetically merging them into
        // one virtual ring.
        if (!positionsFromTop.empty() &&
            positionsFromTop.back().bottomSidePositionFromTop() >= centeringRing.positionFromTop)
        {
            positionsFromTop.back().merge(centeringRing);
        }
        else
        {
            positionsFromTop.emplace_back(centeringRing);
        }
    }
    return positionsFromTop;
}

/// The rings the tab lies between (Java: top and bottom).
struct BoundingRings
{
    const SortableRing* top    = nullptr;
    const SortableRing* bottom = nullptr;
};

/// Picks the last two rings that meet the criteria (Java's loop over positionsFromTop).
[[nodiscard]] BoundingRings boundingRings(const std::vector<SortableRing>& positionsFromTop,
                                          double finPositionFromTop, double finLength)
{
    BoundingRings bounds;
    for (const SortableRing& sortableRing : positionsFromTop)
    {
        if (bounds.top == nullptr)
        {
            bounds.top = &sortableRing;
        }
        else if (sortableRing.bottomSidePositionFromTop() <= finPositionFromTop)
        {
            bounds.top    = &sortableRing;
            bounds.bottom = nullptr;
        }
        else if (bounds.top->bottomSidePositionFromTop() <= finPositionFromTop)
        {
            if (bounds.bottom == nullptr)
            {
                // If the current ring is in the upper half of the root chord, make it the top
                // ring
                if (sortableRing.bottomSidePositionFromTop() <
                    finPositionFromTop + (finLength / 2.0))
                {
                    bounds.top = &sortableRing;
                }
                else
                {
                    bounds.bottom = &sortableRing;
                }
            }
            // Is the ring even with or above the end of the root chord? If so, make the existing
            // bottom the top ring, and the current ring the bottom
            else if (sortableRing.positionFromTop() <= finPositionFromTop + finLength)
            {
                bounds.top    = bounds.bottom;
                bounds.bottom = &sortableRing;
            }
        }
        else if (bounds.bottom == nullptr)
        {
            bounds.bottom = &sortableRing;
        }
    }
    return bounds;
}

/// The tab with a top ring and no bottom ring.
[[nodiscard]] TabPlacement belowTopRing(const SortableRing& top, double finPositionFromTop,
                                        double finLength)
{
    // If there is no bottom ring and the top ring's bottom edge is within the span of the root
    // chord, then set the position of the fin tab starting at the bottom side of the top ring.
    if (top.bottomSidePositionFromTop() >= finPositionFromTop)
    {
        return {.offset = top.bottomSidePositionFromTop() - finPositionFromTop,
                .length = finPositionFromTop + finLength - top.bottomSidePositionFromTop()};
    }
    const double diffLen = top.positionFromTop() - finPositionFromTop;
    if (diffLen < 0)
    {
        // Otherwise the top ring is outside the span of the root chord so set the tab length to
        // be the entire root chord.
        return {.offset = 0.0, .length = finLength};
    }
    // Otherwise there is one ring within the span. Return the length from the start of the fin
    // to the top side of the ring.
    return {.offset = 0.0, .length = diffLen};
}

/// The tab between a top and a bottom ring.
[[nodiscard]] TabPlacement betweenRings(const SortableRing& top, const SortableRing& bottom,
                                        double finPositionFromTop, double finLength)
{
    // If the bottom edge of the top centering ring is above the start of the fin's root chord,
    // then make the fin tab align with the start of the root chord.
    if (top.bottomSidePositionFromTop() < finPositionFromTop)
    {
        const double lenToBottomRing = bottom.positionFromTop() - finPositionFromTop;
        // If the bottom ring lies farther back (down) than the trailing edge of the fin, then the
        // tab should only be as long as the fin.
        return {.offset = 0.0, .length = lenToBottomRing > finLength ? finLength : lenToBottomRing};
    }
    const double offset = top.bottomSidePositionFromTop() - finPositionFromTop;
    // The bottom ring is beyond the trailing edge of the fin.
    if (bottom.positionFromTop() > finLength + finPositionFromTop)
    {
        return {.offset = offset,
                .length = finLength + finPositionFromTop - top.bottomSidePositionFromTop()};
    }
    // The rings are within the span of the root chord.  Place the tab between them.
    return {.offset = offset, .length = bottom.positionFromTop() - top.bottomSidePositionFromTop()};
}

}  // namespace

bool TabSupports::ringsPlaceTheTab() const noexcept
{
    return maxRingRadius > maxTubeRadius && !rings.empty();
}

TabPlacement computeFinTabLength(std::span<const CenteringRing* const> rings,
                                 double finPositionFromTop, double finLength)
{
    // Fin tabs will be computed between the last two rings that meet the criteria, represented
    // by top and bottom here.
    const std::vector<SortableRing> positionsFromTop = sortedRings(rings);
    const BoundingRings bounds = boundingRings(positionsFromTop, finPositionFromTop, finLength);

    TabPlacement result;
    // Edge case where there are no centering rings or for some odd reason top and bottom are
    // identical.
    if (bounds.top == nullptr || bounds.top == bounds.bottom)
    {
        result = {.offset = 0.0, .length = finLength};
    }
    else if (bounds.bottom == nullptr)
    {
        result = belowTopRing(*bounds.top, finPositionFromTop, finLength);
    }
    else
    {
        result = betweenRings(*bounds.top, *bounds.bottom, finPositionFromTop, finLength);
    }
    if (result.length < 0)
    {
        result.length = 0.0;
    }
    return result;
}

bool isComponentInsideFinSpan(const FinSet& fins, const RocketComponent& component)
{
    const double finXMin  = fins.getAxialOffset(AxialMethod::ABSOLUTE);
    const double finXMax  = finXMin + fins.getLength();
    const double compXMin = component.getAxialOffset(AxialMethod::ABSOLUTE);
    const double compXMax = compXMin + component.getLength();
    return (compXMin >= finXMin && compXMin < finXMax) ||
           (compXMax > finXMin && compXMax <= finXMax) ||
           (compXMin <= finXMin && compXMax >= finXMax);
}

TabSupports findTabSupports(const FinSet& fins)
{
    TabSupports            supports;
    const RocketComponent* parent = fins.getParent();
    if (parent == nullptr)
    {
        return supports;
    }
    std::vector<const CenteringRing*> rings;
    for (const RocketComponent* child : parent->getChildren())
    {
        if (const auto* tube = dynamic_cast<const InnerTube*>(child))
        {
            if (!isComponentInsideFinSpan(fins, *tube))
            {
                continue;
            }
            // NOLINTNEXTLINE(readability-use-std-min-max): Java's test, NaN included
            if (tube->getOuterRadius() > supports.maxTubeRadius)
            {
                supports.maxTubeRadius = tube->getOuterRadius();
            }
        }
        else if (const auto* ring = dynamic_cast<const CenteringRing*>(child))
        {
            // NOLINTNEXTLINE(readability-use-std-min-max): Java's test, NaN included
            if (ring->getOuterRadius() > supports.maxRingRadius)
            {
                supports.maxRingRadius = ring->getOuterRadius();
            }
            rings.push_back(ring);
        }
    }
    // Remove rings that are smaller than the maximum inner tube radius
    for (const CenteringRing* ring : rings)
    {
        // Java removes a ring when its radius <= the tube's (a NaN radius stays).
        const bool smaller = ring->getOuterRadius() <= supports.maxTubeRadius;
        if (!smaller)
        {
            supports.rings.push_back(ring);
        }
    }
    return supports;
}

std::optional<double> tabHeight(const FinSet& fins, double maxTubeRadius)
{
    const auto* parent = dynamic_cast<const SymmetricComponent*>(fins.getParent());
    if (parent == nullptr)
    {
        return std::nullopt;
    }
    const Coordinate finFront = fins.getFinFront();
    const double     finStart = finFront.x + fins.getTabFrontEdge();
    const double     finEnd   = finFront.x + fins.getTabTrailingEdge();
    const double     parentMinRadius =
        MathUtil::min(parent->getRadius(finStart), parent->getRadius(finEnd));
    const double height = parentMinRadius - maxTubeRadius;
    // A negative (or NaN) height is not set.
    if (height >= 0)
    {
        return height;
    }
    return std::nullopt;
}

}  // namespace QtRocket::Gui::FinTabCalculator
