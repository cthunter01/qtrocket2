#include "QtRocket/rocket/FinSet.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket
{

namespace
{

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// Whether the profile of @p parent is curved: a transition or nose cone that is not conical.
[[nodiscard]] bool isCurved(const RocketComponent& parent)
{
    const auto* transition = dynamic_cast<const Transition*>(&parent);
    return transition != nullptr && transition->getShapeType() != TransitionShape::CONICAL;
}

}  // namespace

// ======================================================================== FinCrossSection

std::string_view finCrossSectionName(FinCrossSection crossSection) noexcept
{
    switch (crossSection)
    {
        case FinCrossSection::SQUARE:
            return "SQUARE";
        case FinCrossSection::ROUNDED:
            return "ROUNDED";
        case FinCrossSection::AIRFOIL:
            return "AIRFOIL";
    }
    return "SQUARE";
}

std::string_view orkName(FinCrossSection crossSection) noexcept
{
    switch (crossSection)
    {
        case FinCrossSection::SQUARE:
            return "square";
        case FinCrossSection::ROUNDED:
            return "rounded";
        case FinCrossSection::AIRFOIL:
            return "airfoil";
    }
    return "square";
}

std::optional<FinCrossSection> finCrossSectionFromOrkName(std::string_view text)
{
    for (const FinCrossSection crossSection : kAllFinCrossSections)
    {
        if (Strings::orkEnumNameMatches(text, finCrossSectionName(crossSection)))
        {
            return crossSection;
        }
    }
    return std::nullopt;
}

std::string_view displayKey(FinCrossSection crossSection) noexcept
{
    switch (crossSection)
    {
        case FinCrossSection::SQUARE:
            return "FinSet.CrossSection.SQUARE";
        case FinCrossSection::ROUNDED:
            return "FinSet.CrossSection.ROUNDED";
        case FinCrossSection::AIRFOIL:
            return "FinSet.CrossSection.AIRFOIL";
    }
    return "FinSet.CrossSection.SQUARE";
}

std::string_view displayName(FinCrossSection crossSection) noexcept
{
    switch (crossSection)
    {
        case FinCrossSection::SQUARE:
            return "Square";
        case FinCrossSection::ROUNDED:
            return "Rounded";
        case FinCrossSection::AIRFOIL:
            return "Airfoil";
    }
    return "Square";
}

// ================================================================================= FinSet

FinSet::FinSet() : ExternalComponent(AxialMethod::BOTTOM), m_filletMaterial(defaultMaterial())
{
    // Drawn in front of the body components in the 2D side and back views.
    setDisplayOrderSide(4);
    setDisplayOrderBack(4);
}

bool FinSet::isAfter() const
{
    return false;
}

// ------------------------------------------------------------------------------- the fins

bool FinSet::assignFinCount(int n)
{
    if (m_finCount == n)
    {
        return false;
    }
    m_finCount             = std::clamp(n, 1, 8);
    m_finRotationIncrement = Transformation::rotateX(2 * std::numbers::pi / m_finCount);
    return true;
}

void FinSet::setFinCount(int n)
{
    if (!assignFinCount(n))
    {
        return;
    }
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

double FinSet::getBaseRotation() const
{
    return getAngleOffset();
}

void FinSet::setBaseRotation(double r)
{
    setAngleOffset(r);
}

void FinSet::setCantAngle(double newCantRadians)
{
    const double clampedCant = MathUtil::clamp(newCantRadians, -kMaxCantRadians, kMaxCantRadians);
    if (MathUtil::equals(clampedCant, m_cantRadians))
    {
        return;
    }
    m_cantRadians = clampedCant;

    fireComponentChangeEvent(ComponentChangeEvent::kAerodynamicChange);
}

Transformation FinSet::getCantRotation() const
{
    if (!m_cantRotation)
    {
        if (MathUtil::equals(m_cantRadians, 0))
        {
            m_cantRotation = Transformation::kIdentity;
        }
        else
        {
            m_cantRotation = Transformation::rotateY(m_cantRadians);
        }
    }
    return *m_cantRotation;
}

void FinSet::setThickness(double r)
{
    if (m_thickness == r)
    {
        return;
    }
    m_thickness = MathUtil::javaMax(r, 0);
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void FinSet::setCrossSection(CrossSection cs)
{
    if (m_crossSection == cs)
    {
        return;
    }
    m_crossSection = cs;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

// ---------------------------------------------------------------------------- the fin tab

void FinSet::setTabHeight(double newTabHeight, bool validateTabHeight)
{
    if (MathUtil::equals(m_tabHeight, MathUtil::max(newTabHeight, 0)))
    {
        return;
    }

    m_tabHeight = newTabHeight;
    if (validateTabHeight)
    {
        const double maxTabHeight = getMaxTabHeight();
        m_tabHeight               = MathUtil::javaMin(m_tabHeight, maxTabHeight);
    }

    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void FinSet::setTabLength(double lengthRequest, bool updatePosition)
{
    if (MathUtil::equals(m_tabLength, MathUtil::max(lengthRequest, 0)))
    {
        return;
    }

    m_tabLength = lengthRequest;

    if (updatePosition)
    {
        updateTabPosition();
    }

    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void FinSet::updateTabPosition()
{
    m_tabPosition = QtRocket::getAsPosition(m_tabOffsetMethod, m_tabOffset, m_tabLength, m_length);
}

void FinSet::setTabOffset(double offsetRequest)
{
    m_tabOffset = offsetRequest;
    updateTabPosition();

    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

double FinSet::getTabOffset(AxialMethod method) const
{
    return QtRocket::getAsOffset(method, m_tabPosition, m_tabLength, m_length);
}

double FinSet::getTabOffset() const
{
    return getTabOffset(m_tabOffsetMethod);
}

double FinSet::getTabPosition(AxialMethod method) const
{
    return QtRocket::getAsPosition(method, m_tabOffset, m_tabLength, m_length);
}

void FinSet::setTabOffsetMethod(AxialMethod newPositionMethod)
{
    // The method is the lens through which the tab's position is seen; the position stays.
    m_tabOffsetMethod = newPositionMethod;
    m_tabOffset = QtRocket::getAsOffset(m_tabOffsetMethod, m_tabPosition, m_tabLength, m_length);

    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

void FinSet::validateFinTabPosition()
{
    // check front bounds (Java: if (tabPosition < 0) tabPosition = 0)
    m_tabPosition = std::max(m_tabPosition, 0.0);

    // check tail bounds (Java: if (length < tabPosition) tabPosition = length)
    m_tabPosition = std::min(m_tabPosition, m_length);
}

void FinSet::validateFinTabLength()
{
    const double xTabBack = getTabTrailingEdge();
    if (m_length < xTabBack)
    {
        m_tabLength -= (xTabBack - m_length);
    }

    m_tabLength = MathUtil::javaMax(0, m_tabLength);
}

double FinSet::getMaxTabHeight() const
{
    const std::optional<double> radiusFront    = getParentFrontRadius();
    const std::optional<double> radiusTrailing = getParentTrailingRadius();
    if (radiusFront && radiusTrailing)
    {
        return MathUtil::min(*radiusFront, *radiusTrailing);
    }
    return std::numeric_limits<double>::max();
}

std::optional<double> FinSet::getParentFrontRadius(const RocketComponent* parent) const
{
    if (const auto* sym = dynamic_cast<const SymmetricComponent*>(parent))
    {
        const Coordinate finFront = getFinFront();

        // The parent's radius at the tab's reference point.
        const double xLead = getTabFrontEdge();

        return sym->getRadius(finFront.x + xLead);
    }
    return std::nullopt;
}

std::optional<double> FinSet::getParentFrontRadius() const
{
    return getParentFrontRadius(m_parent);
}

std::optional<double> FinSet::getParentTrailingRadius(const RocketComponent* parent) const
{
    if (const auto* sym = dynamic_cast<const SymmetricComponent*>(parent))
    {
        const Coordinate finFront = getFinFront();

        // The parent's radius at the tab's reference point.
        const double xTrail = getTabTrailingEdge();

        return sym->getRadius(finFront.x + xTrail);
    }
    return std::nullopt;
}

std::optional<double> FinSet::getParentTrailingRadius() const
{
    return getParentTrailingRadius(m_parent);
}

bool FinSet::isTabTrivial() const
{
    return kMinimumTabArea > (getTabLength() * getTabHeight());
}

bool FinSet::hasTab() const
{
    return (getTabHeight() > 0) && (getTabLength() > 0);
}

bool FinSet::isTabBeyondFin() const
{
    if (!hasTab())
    {
        return false;
    }

    const double xTabFront = getTabFrontEdge();
    const double xTabTrail = getTabTrailingEdge();

    // The tab is measured from the front of the fin, so the fin ends at its length.
    const double xFinEnd = getLength();

    return (xTabFront > xFinEnd && xTabTrail > xFinEnd) || (xTabTrail < 0 && xTabFront < 0);
}

// -------------------------------------------------------------------------------- fillets

void FinSet::setFilletMaterial(const Material& mat)
{
    if (mat.getType() != Material::Type::BULK)
    {
        bug("ExternalComponent requires a bulk material type=" +
            std::string(::QtRocket::toString(mat.getType())));
    }

    if (m_filletMaterial == mat)
    {
        return;
    }
    m_filletMaterial = mat;
    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void FinSet::setFilletRadius(double r)
{
    if (MathUtil::equals(m_filletRadius, r))
    {
        return;
    }
    m_filletRadius = r;
    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void FinSet::applyDefaultMaterial(const Preferences& preferences, const MaterialStorage& storage)
{
    ExternalComponent::applyDefaultMaterial(preferences, storage);
    m_filletMaterial = getDefaultComponentMaterial(preferences, componentClassChain(kind()),
                                                   Material::Type::BULK, storage);

    // Java's constructor assigns the materials while the cached mass values are still NaN. Here
    // a mass getter may have run since the fin set was made, and no event follows to clear what
    // it cached.
    m_singlePlanformArea = kNaN;
    m_centerOfMass       = Coordinate::kNaN;
    m_totalVolume        = kNaN;
}

Coordinate FinSet::calculateFilletCrossSection(double filletRadius, double bodyRadius)
{
    const double hypotenuse    = filletRadius + bodyRadius;
    const double innerArcAngle = std::asin(filletRadius / hypotenuse);
    const double outerArcAngle = std::acos(filletRadius / hypotenuse);

    const double triangleArea = std::tan(outerArcAngle) * filletRadius * filletRadius / 2;
    double crossSectionArea   = triangleArea - (outerArcAngle * filletRadius * filletRadius / 2) -
                                (innerArcAngle * bodyRadius * bodyRadius / 2);

    if (std::isnan(crossSectionArea))
    {
        crossSectionArea = 0.0;
    }
    else
    {
        // each fin has a fillet on each side
        crossSectionArea *= 2;
    }

    // heuristic, relative to the body centre
    const double yCentroid = bodyRadius + (filletRadius / 5);

    return Coordinate{0, yCentroid, 0, crossSectionArea};
}

// How the volume of the fillet is found. It assumes a circular concave fillet tangent to the fin
// and the body tube.
//
// 1. Form a triangle with vertices at the body's centre, the tangent point between the fillet
//    and the fin, and the centre of the fillet radius.
// 2. The line between the centre of the body and the centre of the fillet radius passes through
//    the tangent point between the fillet and the body.
// 3. Find the area of the triangle, then subtract the portion of the body and of the fillet that
//    is in that triangle (angle / 2 pi * pi r^2 = angle / 2 * r^2).
// 4. Multiply the remaining area by the length.
// 5. Return twice that, since there is a fillet on each side of the fin.
Coordinate FinSet::calculateFilletVolumeCentroid() const
{
    const auto* sym = dynamic_cast<const SymmetricComponent*>(m_parent);
    if ((m_filletRadius == 0) || (sym == nullptr))
    {
        return Coordinate::kZero;
    }
    // The root points, of which there is always at least one. (Java computes them a second time
    // only to return zero when there are none.)
    const std::vector<Coordinate> mountPoints = getRootPoints();

    Coordinate filletVolumeCentroid = Coordinate::kZero;

    Coordinate prev = mountPoints.front();
    for (const Coordinate& cur : std::span{mountPoints}.subspan(1))
    {
        // cross section at mid-segment (the x is in the fin frame, as in OpenRocket)
        const double     xAvg       = (prev.x + cur.x) / 2;
        const double     bodyRadius = sym->getRadius(xAvg);
        const Coordinate segmentCrossSection =
            calculateFilletCrossSection(m_filletRadius, bodyRadius).setX(xAvg);

        const double segmentLength = distance(Point2D{prev.x, prev.y}, Point2D{cur.x, cur.y});
        const double segmentVolume = segmentLength * segmentCrossSection.weight;

        const Coordinate segmentCentroid = segmentCrossSection.setWeight(segmentVolume);

        filletVolumeCentroid = filletVolumeCentroid.average(segmentCentroid);

        prev = cur;
    }

    if (m_finCount == 1)
    {
        const Transformation rotation = Transformation::rotateX(getAngleOffset());
        return rotation.transform(filletVolumeCentroid);
    }
    return filletVolumeCentroid.setY(0.0);
}

// ------------------------------------------------------------------------ the parent body

const SymmetricComponent* FinSet::symmetricParent() const
{
    if (m_parent == nullptr)
    {
        return nullptr;
    }
    const auto* sym = dynamic_cast<const SymmetricComponent*>(m_parent);
    if (sym == nullptr)
    {
        bug("the parent of the fin set " + getName() +
            " is not a symmetric component: " + m_parent->getName());
    }
    return sym;
}

double FinSet::getBodyRadius() const
{
    return getFinFront().y;
}

Coordinate FinSet::getFinFront() const
{
    const double                    xFinFront = getAxialFront();
    const SymmetricComponent* const parent    = symmetricParent();
    if (parent == nullptr)
    {
        return Coordinate{0, 0};
    }
    const double yFinFront = parent->getRadius(xFinFront);
    return Coordinate{xFinFront, yFinFront};
}

bool FinSet::isRootStraight() const
{
    if (const auto* transition = dynamic_cast<const Transition*>(m_parent))
    {
        return transition->getShapeType() == TransitionShape::CONICAL;
    }

    // by default, assume a flat base
    return true;
}

// --------------------------------------------------------------------------- the outlines

std::vector<Coordinate> FinSet::translatePoints(std::span<const Coordinate> points, double xDelta,
                                                double yDelta)
{
    std::vector<Coordinate> returnPoints;
    returnPoints.reserve(points.size());
    for (const Coordinate& point : points)
    {
        returnPoints.emplace_back(point.x + xDelta, point.y + yDelta);
    }
    return returnPoints;
}

std::vector<Coordinate> FinSet::translatePoints(std::span<const Coordinate> points,
                                                const Coordinate&           delta)
{
    std::vector<Coordinate> returnPoints;
    returnPoints.reserve(points.size());
    for (const Coordinate& point : points)
    {
        returnPoints.push_back(point.add(delta));
    }
    return returnPoints;
}

std::vector<Coordinate> FinSet::reverse(std::span<const Coordinate> source)
{
    return {source.rbegin(), source.rend()};
}

std::vector<Coordinate> FinSet::combineCurves(std::span<const Coordinate> c1,
                                              std::span<const Coordinate> c2)
{
    std::vector<Coordinate> combined;
    combined.reserve(c1.size() + c2.size());

    // the first curve forwards, the second one backwards directly after it
    combined.insert(combined.end(), c1.begin(), c1.end());
    combined.insert(combined.end(), c2.rbegin(), c2.rend());

    return combined;
}

std::vector<Coordinate> FinSet::getRootPoints(int maximumBodyDivisionCount) const
{
    if (m_parent == nullptr)
    {
        return {Coordinate::kZero};
    }

    const Coordinate finLead = getFinFront();
    const double     xFinEnd = finLead.x + getLength();

    return getMountPoints(finLead.x, xFinEnd, -finLead.x, -finLead.y, maximumBodyDivisionCount);
}

std::vector<Coordinate> FinSet::getRootPoints() const
{
    return getRootPoints(kMaxRootDivisions);
}

std::vector<Coordinate> FinSet::getMountPoints() const
{
    if (m_parent == nullptr)
    {
        return {};
    }

    return getMountPoints(0.0, m_parent->getLength(), 0, 0);
}

std::vector<Coordinate> FinSet::getMountPoints(double xStart, double xEnd, double xOffset,
                                               double yOffset, int maximumBodyDivisionCount) const
{
    const SymmetricComponent* const parent = symmetricParent();
    if (parent == nullptr)
    {
        return {Coordinate::kZero};
    }

    // for a simple body, one increment is perfectly accurate.
    int          divisionCount  = 1;
    const double intervalLength = xEnd - xStart;

    // for anything more complicated, increase the count:
    if (!MathUtil::equals(getCantAngle(), 0) || isCurved(*parent))
    {
        // the maximum precision to enforce when calculating the areas of fins (especially on
        // curved parent bodies)
        const double xWidth = 0.0025;  // width (in metres) of each individual iteration
        divisionCount       = MathUtil::javaIntCast(std::ceil(intervalLength / xWidth));

        // When creating body curves, don't create more than this many divisions; only relevant
        // on very large components.
        divisionCount = std::min(maximumBodyDivisionCount, divisionCount);
    }

    // Recalculate the x step increment, now with the (rounded) division count.
    const double xIncrement = intervalLength / divisionCount;

    // Create the points: step through the radius of the parent. (Java's int divisionCount + 1
    // wraps for Integer.MAX_VALUE, and no point is made then.)
    const std::int64_t pointCount =
        divisionCount == std::numeric_limits<int>::max() ? 0 : std::int64_t{divisionCount} + 1;
    double                  xCurr = xStart;
    std::vector<Coordinate> points;
    for (std::int64_t index = 0; index < pointCount; index++)
    {
        double yCurr = parent->getRadius(xCurr);

        // Account for the fin cant angle
        const double dy = getFinCantYOffset(xStart, xEnd, xCurr);
        yCurr += dy;

        points.emplace_back(xCurr, yCurr);

        xCurr += xIncrement;
    }
    if (points.empty())
    {
        // Java: an IndexOutOfBoundsException below (a canted or curved root of negative length).
        bug("the fin set " + getName() + " has no root points");
    }

    // If the front fin point is outside the parent's bounds, and the last point is still within
    // the parent's bounds, an extra root point is needed at the front of the parent; the same
    // goes for the last point, the other way round. This makes fins on transitions and nose
    // cones come out right (OpenRocket issue #1021).
    const double parentLength = parent->getLength();
    // Front fin point is outside the parent's bounds and last point is beyond the parent's fore
    // end
    if (xStart < 0 && xEnd > 0)
    {
        insertPointByX(points, 0, points.front().y);
    }
    // End fin point is beyond the parent's aft and first point is still before the parent's aft
    // end
    if (xEnd > parentLength && xStart < parentLength)
    {
        const double x = parentLength;
        const double y = points.back().y;
        insertPointByX(points, x, y);
    }

    // correct last point, if beyond a rounding error from body's end.
    if (std::abs(points.back().x - parentLength) < MathUtil::kEpsilon)
    {
        points.back() = points.back().setX(parentLength);
    }

    // translate the points if needed
    if ((std::abs(xOffset) + std::abs(yOffset)) > MathUtil::kEpsilon)
    {
        points = translatePoints(points, xOffset, yOffset);
    }

    return points;
}

std::vector<Coordinate> FinSet::getMountPoints(double xStart, double xEnd, double xOffset,
                                               double yOffset) const
{
    return getMountPoints(xStart, xEnd, xOffset, yOffset, kMaxRootDivisions);
}

void FinSet::insertPointByX(std::vector<Coordinate>& points, double x, double y)
{
    for (const Coordinate& point : points)
    {
        if (std::abs(point.x - x) < MathUtil::kEpsilon)
        {
            return;
        }
    }

    std::size_t insertIndex = 0;
    while (insertIndex < points.size() && points[insertIndex].x < x)
    {
        insertIndex++;
    }

    points.insert(std::next(points.begin(), static_cast<std::ptrdiff_t>(insertIndex)),
                  Coordinate{x, y});
}

double FinSet::getFinCantYOffset(double xStart, double xEnd, double xCurr) const
{
    const SymmetricComponent* const parent = symmetricParent();
    QTROCKET_ASSERT(parent != nullptr);
    const double cantAngle    = getCantAngle();
    const double parentLength = parent->getLength();

    if (MathUtil::equals(cantAngle, 0) || (xStart > parentLength && xEnd > parentLength) ||
        (xStart < 0 && xEnd < 0))
    {
        return 0;
    }

    // Limit the x position to the parent's bounds
    double x = MathUtil::javaMax(xCurr, 0);
    x        = MathUtil::javaMin(x, parentLength);

    // Determine the center of rotation
    const double xCenter = (xStart + xEnd) / 2;

    // Calculate the new x position after rotation
    const double dx       = x - xCenter;  // Distance to the center of rotation
    const double xCurrRot = xCenter + (dx * std::abs(std::cos(cantAngle)));

    // Extend the root of the fin to touch the surface of the parent
    const double radiusRot = parent->getRadius(xCurrRot);
    const double dz        = std::abs(std::sin(cantAngle)) * dx;

    double dy = 0;
    if (!(dz >= radiusRot))
    {
        // Simplification of r^2 = (r - dy)^2 + dz^2, given that dz < r: the cross-section of the
        // body is a circle, and a tangent line from its top reaches a distance dz. (Java's
        // Math.pow(x, 2) is x * x.)
        dy = radiusRot - std::sqrt(MathUtil::pow2(radiusRot) - MathUtil::pow2(dz));
    }

    return -dy;
}

void FinSet::alignEndsWithRoot(std::vector<Coordinate>& finPoints) const
{
    // Set the start and end fin points the same as the root points (necessary for canted fins)
    const std::vector<Coordinate> rootPoints = getRootPoints();
    if (rootPoints.size() > 1)
    {
        QTROCKET_ASSERT(!finPoints.empty());
        finPoints.front() = finPoints.front().setX(rootPoints.front().x).setY(rootPoints.front().y);
        finPoints.back()  = finPoints.back().setX(rootPoints.back().x).setY(rootPoints.back().y);
    }
}

std::vector<Coordinate> FinSet::getFinPointsWithRoot() const
{
    return combineCurves(getFinPoints(), getRootPoints());
}

std::vector<Coordinate> FinSet::getFinPointsWithLowResRoot() const
{
    return combineCurves(getFinPoints(), getRootPoints(kMaxRootDivisionsLowRes));
}

std::vector<Coordinate> FinSet::getTabPoints() const
{
    const double xTabFront = getTabFrontEdge();
    const double xTabTrail = getTabTrailingEdge();

    const Coordinate finFront = getFinFront();

    const SymmetricComponent* const body = symmetricParent();

    // The heights are limited to the body radius at each end.
    double yTabFront  = kNaN;
    double yTabTrail  = kNaN;
    double yTabBottom = kNaN;
    if (body != nullptr)
    {
        yTabFront  = body->getRadius(finFront.x + xTabFront) - finFront.y;
        yTabTrail  = body->getRadius(finFront.x + xTabTrail) - finFront.y;
        yTabBottom = MathUtil::min(yTabFront, yTabTrail) - m_tabHeight;
    }

    return {Coordinate{xTabFront, yTabFront}, Coordinate{xTabFront, yTabBottom},
            Coordinate{xTabTrail, yTabBottom}, Coordinate{xTabTrail, yTabTrail}};
}

std::vector<Coordinate> FinSet::generateTabPointsWithRoot(std::vector<Coordinate> rootPoints) const
{
    const std::vector<Coordinate> tabPoints = getTabPoints();

    rootPoints.insert(rootPoints.begin(), Coordinate{tabPoints.front().x, tabPoints.front().y});

    return combineCurves(tabPoints, rootPoints);
}

std::vector<Coordinate> FinSet::getTabPointsWithRoot() const
{
    if (!hasTab())
    {
        return {};
    }

    const double xTabFront = getTabFrontEdge();
    const double xTabTrail = getTabTrailingEdge();

    std::vector<Coordinate> rootPoints;
    for (const Coordinate& point : getRootPoints())
    {
        if (point.x > xTabFront && point.x < xTabTrail)
        {
            rootPoints.push_back(point);
        }
    }

    return generateTabPointsWithRoot(std::move(rootPoints));
}

std::vector<Coordinate> FinSet::getTabPointsWithRootLowRes() const
{
    if (MathUtil::equals(getTabHeight(), 0) || MathUtil::equals(getTabLength(), 0))
    {
        return {};
    }

    const double xTabFront = getTabFrontEdge();
    const double xTabTrail = getTabTrailingEdge();

    std::vector<Coordinate> rootPoints;
    for (const Coordinate& point : getRootPoints(kMaxRootDivisionsLowRes))
    {
        if (point.x > xTabFront && point.x < xTabTrail)
        {
            rootPoints.push_back(point);
        }
    }

    return generateTabPointsWithRoot(std::move(rootPoints));
}

std::vector<Coordinate> FinSet::generateContinuousFinAndTabShape() const
{
    if (!hasTab() || isTabBeyondFin())
    {
        return getFinPointsWithRoot();
    }

    const std::vector<Coordinate> finPoints  = getFinPoints();
    const std::vector<Coordinate> rootPoints = getRootPoints();
    const std::vector<Coordinate> tabPoints  = getTabPoints();
    QTROCKET_ASSERT(!finPoints.empty());

    const double finStart = finPoints.front().x;

    std::vector<Coordinate> uniformPoints = finPoints;

    bool tabAdded = false;
    for (const Coordinate& rootPoint : rootPoints)
    {
        // Until the tab has been added, the root points behind the tab go in as they come; the
        // first one that is not behind it brings the tab in first.
        if (!tabAdded)
        {
            if (rootPoint.x > tabPoints.back().x)
            {
                uniformPoints.push_back(rootPoint);
            }
            else
            {
                uniformPoints.insert(uniformPoints.end(), tabPoints.rbegin(), tabPoints.rend());
                tabAdded = true;
            }
        }
        // Once the tab is added, the remaining root points that lie before the tab follow.
        if (tabAdded && rootPoint.x < tabPoints.front().x)
        {
            uniformPoints.push_back(rootPoint);
        }
    }

    // Make sure the shape is closed in case the tab is before the fin.
    if (tabPoints.front().x < finStart)
    {
        uniformPoints.push_back(finPoints.front());
    }

    return uniformPoints;
}

// ------------------------------------------------------------------------- areas and mass

Coordinate FinSet::calculateCurveIntegral(std::span<const Coordinate> points)
{
    Coordinate centroidSum{0};

    if (points.empty())
    {
        return centroidSum;
    }

    Coordinate prev = points.front();
    for (const Coordinate& cur : points.subspan(1))
    {
        // calculate marginal area (a point below the x axis gives a negative one, as in
        // OpenRocket)
        const double deltaX        = cur.x - prev.x;
        const double yAvg          = (cur.y + prev.y) * 0.5;
        const double areaIncrement = deltaX * yAvg;
        if (MathUtil::equals(0, areaIncrement))
        {
            prev = cur;
            // zero area increment: ignore and continue
            continue;
        }

        // calculate centroid increment (Java's Math.pow(y, 2) is y * y)
        const double common = 1 / (3 * (cur.y + prev.y));
        const double xCtr =
            common * ((prev.x * ((2 * prev.y) + cur.y)) + (cur.x * ((2 * cur.y) + prev.y)));
        const double yCtr =
            common * ((cur.y * prev.y) + MathUtil::pow2(cur.y) + MathUtil::pow2(prev.y));

        const Coordinate centroidIncrement{xCtr, yCtr, 0, areaIncrement};
        centroidSum = centroidSum.average(centroidIncrement);

        prev = cur;
    }

    // Negative weight => make positive. OpenRocket notes that this is not a correct solution,
    // but that at least nothing fails.
    if (centroidSum.weight < 0)
    {
        centroidSum =
            Coordinate{centroidSum.x, -centroidSum.y, centroidSum.z, std::abs(centroidSum.weight)};
    }

    return centroidSum;
}

std::vector<Coordinate> FinSet::translateToCenterline(std::span<const Coordinate> fromRoot) const
{
    const Coordinate finRoot = getFinFront();

    // locate relative to fin/body centerline
    return translatePoints(fromRoot, 0.0, finRoot.y);
}

Coordinate FinSet::calculateTabCentroid() const
{
    if (dynamic_cast<const SymmetricComponent*>(m_parent) == nullptr || isTabTrivial())
    {
        // no parent, or not a body:
        return Coordinate::kZero;
    }
    // relative to the fin
    const double xTabFrontFin = getTabFrontEdge();
    const double xTabTrailFin = getTabTrailingEdge();

    const Coordinate finFront      = getFinFront();
    const double     xFinFrontBody = finFront.x;
    const double     xTabFrontBody = xFinFrontBody + xTabFrontFin;
    const double     xTabTrailBody = xFinFrontBody + xTabTrailFin;

    // the body points, relative to the fin front and the centerline
    const std::vector<Coordinate> upperCurve =
        getMountPoints(xTabFrontBody, xTabTrailBody, -xFinFrontBody, 0);
    const std::vector<Coordinate> lowerCurve = translateToCenterline(getTabPointsWithRoot());
    const std::vector<Coordinate> tabPoints  = combineCurves(upperCurve, lowerCurve);

    return calculateCurveIntegral(tabPoints);
}

Coordinate FinSet::calculateSinglePlanformCentroid() const
{
    const Coordinate finLead   = getFinFront();
    const double     xFinTrail = finLead.x + getLength();

    const std::vector<Coordinate> upperCurve = translatePoints(getFinPoints(), 0, finLead.y);
    const std::vector<Coordinate> lowerCurve = getMountPoints(finLead.x, xFinTrail, -finLead.x, 0);
    const std::vector<Coordinate> totalCurve = combineCurves(upperCurve, lowerCurve);

    return calculateCurveIntegral(totalCurve);
}

void FinSet::calculateCM() const
{
    const Coordinate wettedCentroid = calculateSinglePlanformCentroid();
    m_singlePlanformArea            = wettedCentroid.weight;
    const double wettedVolume =
        wettedCentroid.weight * m_thickness * relativeVolume(m_crossSection);
    const double     finBulkMass = wettedVolume * m_material.getDensity();
    const Coordinate wettedCM    = wettedCentroid.setWeight(finBulkMass);

    const Coordinate tabCentroid = calculateTabCentroid();
    const double     tabVolume   = tabCentroid.weight * m_thickness;
    const double     tabMass     = tabVolume * m_material.getDensity();
    const Coordinate tabCM       = tabCentroid.setWeight(tabMass);

    const Coordinate filletCentroid = calculateFilletVolumeCentroid();
    const double     filletVolume   = filletCentroid.weight;
    const double     filletMass     = filletVolume * m_filletMaterial.getDensity();
    const Coordinate filletCM       = filletCentroid.setWeight(filletMass);

    m_totalVolume = (wettedVolume + tabVolume + filletVolume) * m_finCount;

    const double     eachFinMass = finBulkMass + tabMass + filletMass;
    const Coordinate eachFinCenterOfMass =
        wettedCM.average(tabCM).average(filletCM).setWeight(eachFinMass);

    // ^^ per fin
    // vv per component

    // The y coordinate: a single fin is rotated around the parent; several fins average out to
    // zero.
    if (m_finCount == 1)
    {
        m_centerOfMass = m_baseRotation.transform(eachFinCenterOfMass);
    }
    else
    {
        m_centerOfMass = eachFinCenterOfMass.setY(0.0).setWeight(eachFinMass * m_finCount);
    }
}

double FinSet::getPlanformArea() const
{
    if (std::isnan(m_singlePlanformArea))
    {
        calculateCM();
    }
    return m_singlePlanformArea;
}

double FinSet::getComponentMass() const
{
    if (m_centerOfMass.isNaN())
    {
        calculateCM();
    }
    return m_centerOfMass.weight;
}

double FinSet::getComponentVolume() const
{
    if (std::isnan(m_totalVolume))
    {
        calculateCM();
    }

    return m_totalVolume;
}

Coordinate FinSet::getComponentCG() const
{
    if (m_centerOfMass.isNaN())
    {
        calculateCM();
    }

    return m_centerOfMass;
}

// The longitudinal unit inertia is approximated as follows:
//
// 1. Approximate a fin with a rectangular thin plate.
// 2. The unit inertia of one fin is taken as the average of the unit inertias through its centre
//    perpendicular to the plane (Izz / M) and through its centre parallel to the plane (Iyy / M).
// 3. If there are several fins, the inertia is shifted to the centre of the fin set with the
//    parallel axis theorem.
double FinSet::getLongitudinalUnitInertia() const
{
    if (std::isnan(m_singlePlanformArea))
    {
        calculateCM();
    }

    // Approximate fin with a rectangular fin; w2 and h2 are squares of the fin width and height
    const double w  = getLength();
    const double h  = getSpan();
    double       w2 = 0;
    double       h2 = 0;

    // If either h or w is 0, we punt and treat the fin as square
    if (MathUtil::equals(h * w, 0))
    {
        w2 = m_singlePlanformArea;
        h2 = m_singlePlanformArea;
    }
    else
    {
        w2 = w * m_singlePlanformArea / h;
        h2 = h * m_singlePlanformArea / w;
    }

    // Iyy = h * w^3 / 12, so Iyy/M = w^2 / 12
    // Izz = h * w * (h^2 + w^2) / 12, so Izz/M = (h^2 + w^2) / 12
    // (Iyy / M + Izz / M) / 2 = (h^2 + 2 * w^2)/24
    const double inertia = (h2 + (2 * w2)) / 24;

    if (m_finCount == 1)
    {
        return inertia;
    }

    // Move the axis to the centre of the fin set. The parallel axis theorem applies to Izz but
    // not to Iyy (the displacement to the new axis is along y); as the inertia is the average of
    // the two, the theorem's term is weighted by 1/2.
    return inertia + (MathUtil::pow2((MathUtil::safeSqrt(h2) / 2) + getBodyRadius()) / 2);
}

// The rotational unit inertia is approximated as follows:
//
// 1. Approximate the fin with a rectangular thin plate.
// 2. Calculate the unit rotational inertia (Ixx / M) of the rectangle about the centre of the
//    approximated fin.
// 3. If there are several fins, shift the inertia axis to the centre of the fin set.
double FinSet::getRotationalUnitInertia() const
{
    if (std::isnan(m_singlePlanformArea))
    {
        calculateCM();
    }

    // Approximate fin with a rectangular fin; h2 is square of fin height
    const double w  = getLength();
    const double h  = getSpan();
    double       h2 = 0;

    // If either h or w is 0, punt and treat it as a square fin
    if (MathUtil::equals(w * h, 0))
    {
        h2 = m_singlePlanformArea;
    }
    else
    {
        h2 = h * m_singlePlanformArea / w;
    }

    // Ixx = w * h^3 / 12, so Ixx / M = h^2 / 12
    const double inertia = h2 / 12;

    if (m_finCount == 1)
    {
        return inertia;
    }

    // Move axis to center of FinSet using Parallel Axis Theorem
    return inertia + MathUtil::pow2((MathUtil::safeSqrt(h2) / 2) + getBodyRadius());
}

// --------------------------------------------------------------------------------- bounds

BoundingBox FinSet::getInstanceBoundingBox() const
{
    BoundingBox singleFinBounds;

    const std::vector<Coordinate> finPoints = getFinPoints();
    singleFinBounds.update(finPoints);

    singleFinBounds.update(Coordinate{0, 0, -m_thickness / 2});
    singleFinBounds.update(Coordinate{0, 0, m_thickness / 2});

    return singleFinBounds;
}

std::vector<Coordinate> FinSet::getComponentBounds() const
{
    std::vector<Coordinate> bounds;
    bounds.reserve(2);

    // The bounds of this component in its own frame.
    double xMin = std::numeric_limits<double>::max();
    // Java's Double.MIN_VALUE, the smallest positive double.
    double xMax = std::numeric_limits<double>::denorm_min();
    double rMax = 0.0;

    for (const Coordinate& point : getFinPoints())
    {
        // Java compares and assigns; std::min and std::max keep the current value for a NaN too.
        xMin = std::min(xMin, point.x);
        xMax = std::max(xMax, point.x);
        rMax = std::max(rMax, MathUtil::hypot(point.y, point.z));
    }

    const std::vector<Coordinate> locations = getComponentLocations();
    QTROCKET_ASSERT(!locations.empty());
    xMax += locations.front().x;

    if (const auto* sym = dynamic_cast<const SymmetricComponent*>(m_parent))
    {
        rMax += sym->getRadius(0);
    }

    addBoundingBox(bounds, xMin, xMax, rMax);
    return bounds;
}

// ------------------------------------------------------------------------ RocketComponent

void FinSet::componentChanged(const ComponentChangeEvent& event)
{
    if (event.isAerodynamicChange() || event.isMassChange())
    {
        m_singlePlanformArea = kNaN;
        m_centerOfMass       = Coordinate::kNaN;
        m_totalVolume        = kNaN;
        m_cantRotation.reset();
    }

    ExternalComponent::componentChanged(event);
}

bool FinSet::allowsChildren() const
{
    return false;
}

bool FinSet::isCompatible(ComponentKind /*kind*/) const
{
    return false;
}

std::vector<std::unique_ptr<RocketComponent>> FinSet::copyFrom(const RocketComponent& source)
{
    const auto* src = dynamic_cast<const FinSet*>(&source);
    if (src == nullptr)
    {
        bug("FinSet::copyFrom(): the source is not a FinSet");
    }
    // Java assigns its fields first; assigning them once the base classes have succeeded leaves
    // this component unchanged when they throw (they neither read nor reset these fields).
    std::vector<std::unique_ptr<RocketComponent>> previous = ExternalComponent::copyFrom(source);
    m_finCount                                             = src->m_finCount;
    m_finRotationIncrement                                 = src->m_finRotationIncrement;
    m_firstFinOffsetRadians                                = src->m_firstFinOffsetRadians;
    m_cantRadians                                          = src->m_cantRadians;
    m_cantRotation                                         = src->m_cantRotation;
    m_thickness                                            = src->m_thickness;
    m_crossSection                                         = src->m_crossSection;
    m_tabHeight                                            = src->m_tabHeight;
    m_tabLength                                            = src->m_tabLength;
    m_tabOffsetMethod                                      = src->m_tabOffsetMethod;
    m_tabPosition                                          = src->m_tabPosition;
    return previous;
}

// ---------------------------------------------------------------------- AxialPositionable

void FinSet::setAxialMethod(AxialMethod newAxialMethod)
{
    RocketComponent::setAxialMethod(newAxialMethod);
}

double FinSet::getAxialOffset() const
{
    return RocketComponent::getAxialOffset();
}

void FinSet::setAxialOffset(double newOffset)
{
    RocketComponent::setAxialOffset(newOffset);
}

// ---------------------------------------------------------------------- AnglePositionable

double FinSet::getAngleOffset() const
{
    return m_firstFinOffsetRadians;
}

void FinSet::setAngleOffset(double angle)
{
    const double reducedAngle = MathUtil::reducePi(angle);
    if (MathUtil::equals(reducedAngle, m_firstFinOffsetRadians))
    {
        return;
    }
    m_firstFinOffsetRadians = reducedAngle;

    if (MathUtil::equals(m_firstFinOffsetRadians, 0))
    {
        m_baseRotation = Transformation::kIdentity;
    }
    else
    {
        m_baseRotation = Transformation::rotateX(m_firstFinOffsetRadians);
    }

    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

AngleMethod FinSet::getAngleMethod() const
{
    return m_angleMethod;
}

void FinSet::setAngleMethod(AngleMethod newMethod)
{
    m_angleMethod = newMethod;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

// --------------------------------------------------------------------- RadiusPositionable

double FinSet::getBoundingRadius() const
{
    return 0.0;
}

double FinSet::getRadiusOffset() const
{
    return RocketComponent::getRadiusOffset();
}

void FinSet::setRadiusOffset(double /*radius*/)
{
    // Does nothing: fins are inherently set to RadiusMethod::SURFACE at 0.0.
}

RadiusMethod FinSet::getRadiusMethod() const
{
    return RadiusMethod::SURFACE;
}

void FinSet::setRadiusMethod(RadiusMethod /*method*/)
{
    // Does nothing: fins are inherently set to RadiusMethod::SURFACE at 0.0.
}

void FinSet::setRadius(RadiusMethod /*method*/, double /*radius*/)
{
    // Does nothing: fins are inherently set to RadiusMethod::SURFACE at 0.0.
}

// ------------------------------------------------------------------------------ instances

int FinSet::getInstanceCount() const
{
    return getFinCount();
}

void FinSet::setInstanceCount(int newCount)
{
    setFinCount(newCount);
}

std::vector<Coordinate> FinSet::getInstanceLocations() const
{
    return RocketComponent::getInstanceLocations();
}

std::vector<Coordinate> FinSet::getInstanceOffsets() const
{
    const double bodyRadius = getBodyRadius();

    // already includes the base rotation
    const std::vector<double> angles = getInstanceAngles();

    const Transformation localCantRotation =
        Transformation{m_length / 2, 0, 0}
            .applyTransformation(getCantRotation())
            .applyTransformation(Transformation{-m_length / 2, 0, 0});

    std::vector<Coordinate> toReturn;
    toReturn.reserve(angles.size());
    for (const double angle : angles)
    {
        const Coordinate raw{0, bodyRadius, 0};
        const Coordinate canted  = localCantRotation.transform(raw);
        const Coordinate rotated = Transformation::rotateX(angle).transform(canted);
        toReturn.push_back(rotated);
    }

    return toReturn;
}

std::string FinSet::getPatternName() const
{
    return std::to_string(getInstanceCount()) + "-fin-ring";
}

double FinSet::getInstanceAngleIncrement() const
{
    return 2 * std::numbers::pi / getFinCount();
}

std::vector<double> FinSet::getInstanceAngles() const
{
    const double angleIncrementRadians = getInstanceAngleIncrement();

    std::vector<double> result;
    result.reserve(static_cast<std::size_t>(getFinCount()));
    for (int finNumber = 0; finNumber < getFinCount(); ++finNumber)
    {
        result.push_back(
            MathUtil::reduce2Pi(m_firstFinOffsetRadians + (angleIncrementRadians * finNumber)));
    }

    return result;
}

RocketComponent::SplitResult FinSet::splitFins(bool freezeRocket)
{
    return splitInstances(freezeRocket);
}

// ---------------------------------------------------------------------------------- debug

std::string FinSet::getPointDescr(std::span<const Coordinate> points, std::string_view name,
                                  std::string_view indent)
{
    std::string buf;

    std::format_to(std::back_inserter(buf), "{}    >> {}: {} points\n", indent, name,
                   points.size());
    int index = 0;
    for (const Coordinate& c : points)
    {
        // Java: indent + "      ....[%2d] (%6.4g, %6.4g)\n"
        std::format_to(std::back_inserter(buf), "{}      ....[{:2}] ({:>6}, {:>6})\n", indent,
                       index, Strings::formatGeneral(c.x, 4), Strings::formatGeneral(c.y, 4));
        index++;
    }
    return buf;
}

std::string FinSet::toDebugDetail(std::source_location where) const
{
    std::string buf = ExternalComponent::toDebugDetail(where);

    buf += getPointDescr(getFinPoints(), "Fin Points", "");

    if (m_parent != nullptr)
    {
        buf += getPointDescr(getMountPoints(0, m_parent->getLength(), 0, 0), "Body Points", "");
        buf += getPointDescr(getRootPoints(), "Root Points", "");
    }

    if (!isTabTrivial())
    {
        // Java: "    TabLength: %6.4f TabHeight: %6.4f @ %6.4f    via: %s\n"
        std::format_to(std::back_inserter(buf),
                       "    TabLength: {:>6} TabHeight: {:>6} @ {:>6}    via: {}\n",
                       Strings::formatFixed(m_tabLength, 4), Strings::formatFixed(m_tabHeight, 4),
                       Strings::formatFixed(m_tabPosition, 4), displayName(m_tabOffsetMethod));
        buf += getPointDescr(getTabPointsWithRoot(), "Tab Points", "");
    }
    return buf;
}

}  // namespace QtRocket
