#pragma once

#include <optional>
#include <span>
#include <vector>

namespace QtRocket
{
class CenteringRing;
class FinSet;
class RocketComponent;
}  // namespace QtRocket

/// The calculation behind the fin set panels' "Calculate automatically" (OpenRocket's
/// FinSetConfig.calculateAutoTab(), computeFinTabLength(), isComponentInsideFinSpan() and its
/// SortableRing, swing gui/configdialog/FinSetConfig.java:254-540), free of Qt and of the panel:
/// where the tab of a fin set fits between the centering rings of its parent and how far it reaches
/// into the body. The panel (FinSetConfig::calculateAutoTab()) sets the values it gives through its
/// fields, in OpenRocket's order.
///
/// Deviations from OpenRocket:
/// - computeFinTabLength() returns the tab's offset with its length (Java sets the offset on the
///   DoubleModel it is given, once on every path) and takes no "relativeTo" component (Java's
///   parameter is unused); a list of no rings stands for Java's null list too (the two give the
///   same result).
/// - The rings are sorted as Java's List.sort() sorts fewer than 32 elements (TimSort's run count
///   and binary insertion sort), with OpenRocket's comparator, which truncates the difference of
///   two rings' positions to whole millimetres and so calls rings less than 1 mm apart equal (an
///   order that is not transitive). For 32 rings or more Java merges runs, which this does not:
///   there the order of rings within a millimetre of each other may differ.
namespace QtRocket::Gui::FinTabCalculator
{

/// Where computeFinTabLength() puts the tab.
struct TabPlacement
{
    /// The tab's offset from the front of the root (AxialMethod::TOP, Java: the value given to the
    /// tab shift model).
    double offset = 0.0;
    /// The tab's length, at least 0.
    double length = 0.0;
};

/// What the fin set's parent offers the tab (Java: calculateAutoTab()'s loop over the parent's
/// children): the inner tubes among the parent's direct children that overlap the fin
/// (isComponentInsideFinSpan()) and the centering rings among them.
struct TabSupports
{
    /// The largest outer radius of those inner tubes, 0 without one.
    double maxTubeRadius = 0.0;
    /// The largest outer radius of the centering rings, 0 without one.
    double maxRingRadius = 0.0;
    /// The centering rings larger than maxTubeRadius, in the order of the children.
    std::vector<const CenteringRing*> rings;

    /// Whether the rings place the tab (Java: maxRingRad > maxTubeRad && !rings.isEmpty()):
    /// otherwise the tab runs along the whole root.
    [[nodiscard]] bool ringsPlaceTheTab() const noexcept;
};

/// The tab between the rings @p rings (in any order) for a fin whose root starts
/// @p finPositionFromTop from the top of the rings' parent and is @p finLength long (Java:
/// computeFinTabLength()): the rings are sorted from the top, rings that overlap or touch are
/// merged, and the tab is put between the last two rings that bound the root, along the whole
/// root without rings.
[[nodiscard]] TabPlacement computeFinTabLength(std::span<const CenteringRing* const> rings,
                                               double finPositionFromTop, double finLength);

/// Whether @p component lies within the axial span of @p fins, wholly or in part (Java:
/// isComponentInsideFinSpan(), by their absolute positions).
[[nodiscard]] bool isComponentInsideFinSpan(const FinSet& fins, const RocketComponent& component);

/// The inner tubes and centering rings among the direct children of @p fins' parent (see
/// TabSupports); nothing without a parent.
[[nodiscard]] TabSupports findTabSupports(const FinSet& fins);

/// The tab height that reaches from the root down to the largest inner tube under the fin, of
/// the outer radius @p maxTubeRadius (Java: calculateAutoTab()'s "Compute tab height"): the
/// smaller radius of the parent at the tab's two ends as the tab now lies (MathUtil::min) less
/// @p maxTubeRadius; nullopt when that is negative or NaN, or the parent is no
/// SymmetricComponent.
[[nodiscard]] std::optional<double> tabHeight(const FinSet& fins, double maxTubeRadius);

}  // namespace QtRocket::Gui::FinTabCalculator
