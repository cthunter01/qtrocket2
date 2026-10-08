#include "QtRocket/rocket/FreeformFinSet.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

namespace
{

/// The end of convertFinSet(): gives @p freeform, which carries @p finset's name, the freeform
/// component name in place of @p finset's at the start of that name, and @p finset's appearance.
void adoptNameAndAppearance(const FinSet& finset, FreeformFinSet& freeform)
{
    // Set name
    const std::string componentTypeName = finset.getComponentName();
    const std::string name              = freeform.getName();

    if (name.starts_with(componentTypeName))
    {
        freeform.setName(freeform.getComponentName() + name.substr(componentTypeName.size()));
    }

    freeform.setAppearance(finset.getAppearance());
}

}  // namespace

FreeformFinSet::FreeformFinSet()
  : m_points{Coordinate::kZero, Coordinate{0.025, 0.05}, Coordinate{0.075, 0.05},
             Coordinate{0.05, 0}}
{
    m_length = 0.05;
}

std::unique_ptr<RocketComponent> FreeformFinSet::cloneShallow() const
{
    return std::make_unique<FreeformFinSet>(*this);
}

FreeformFinSet::ConversionResult FreeformFinSet::convertFinSet(FinSet& finset, bool freezeRocket)
{
    Rocket* const rocket = freezeRocket ? dynamic_cast<Rocket*>(&finset.getRoot()) : nullptr;
    if (rocket != nullptr)
    {
        rocket->freeze();
    }

    ConversionResult       result;
    RocketComponent* const parent   = finset.getParent();
    std::size_t            position = 0;
    // Made before anything changes (Java makes it once the fin set is out of the tree; the
    // constructor has no side effects), so that the handler below can tell whether the tree has
    // taken it: addChild() empties the pointer only when it does.
    auto freeform = std::make_unique<FreeformFinSet>();
    try
    {
        // Get fin set position and remove fin set
        if (parent != nullptr)
        {
            const std::optional<std::size_t> found = parent->getChildPosition(&finset);
            if (!found)
            {
                bug("convertFinSet(): the fin set is not among its parent's children");
            }
            position        = *found;
            result.original = parent->removeChild(position);
        }

        // Create the freeform fin set
        const std::vector<Coordinate> finPoints = finset.getFinPoints();
        freeform->setPoints(finPoints);
        freeform->setAxialOffset(finset.getAxialMethod(), finset.getAxialOffset());

        // Copy component attributes (the new fin set has no children to hand back)
        static_cast<void>(freeform->copyFrom(finset));

        adoptNameAndAppearance(finset, *freeform);

        // Add freeform fin set to parent
        if (parent != nullptr)
        {
            result.freeform = &parent->addChild(std::move(freeform), position);
        }
        else
        {
            result.freeform = freeform.get();
            result.detached = std::move(freeform);
        }
    }
    catch (...)
    {
        // Deviation: Java leaves the fin set it took out detached, a live object its caller still
        // holds. Here the result owns it and is lost with the exception, so it goes back to its
        // place unless the new fin set already has it.
        if (parent != nullptr && result.original != nullptr && freeform != nullptr)
        {
            parent->addChild(std::move(result.original), position);
        }
        if (rocket != nullptr)
        {
            rocket->thaw();
        }
        throw;
    }
    if (rocket != nullptr)
    {
        rocket->thaw();
    }

    return result;
}

Result<void> FreeformFinSet::addPoint(std::size_t index, Point2D location)
{
    // (Not index + 1 > size: that wraps for the largest index, which is what a -1 becomes.)
    if (index < 1 || index >= m_points.size())
    {
        return fail(ErrorCode::INVALID_ARGUMENT,
                    "Cannot add new point before the first or after the last point");
    }

    // the new point is created at the given location
    m_points.insert(std::next(m_points.begin(), static_cast<std::ptrdiff_t>(index)),
                    Coordinate{location.x, location.y});

    // adding a point within the segment affects neither mass nor aerodynamics
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    return {};
}

Result<void> FreeformFinSet::removePoint(std::size_t index)
{
    // The point list is never empty (see setPoints()), so size() - 1 is the last index.
    if (index == 0 || index == m_points.size() - 1)
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "cannot remove first or last point");
    }
    if (index >= m_points.size())
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "index out of range");
    }

    // copy the old list in case the operation fails
    std::vector<Coordinate> copy = m_points;

    m_points.erase(std::next(m_points.begin(), static_cast<std::ptrdiff_t>(index)));
    if (intersects())
    {
        // if error, rollback.
        m_points = std::move(copy);
    }

    fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange);
    return {};
}

void FreeformFinSet::setPoints(std::span<const Coordinate> newPoints, bool validateFinTab)
{
    if (newPoints.empty())
    {
        // Java: an IndexOutOfBoundsException.
        bug("FreeformFinSet::setPoints(): no points");
    }
    // The first point is taken to be at the origin; if it isn't, the outline is moved there.
    const Coordinate        delta = newPoints.front().multiply(-1);
    std::vector<Coordinate> points =
        kIgnoreSmallerThan < delta.length2()
            ? translatePoints(newPoints, delta)
            : std::vector<Coordinate>(newPoints.begin(), newPoints.end());

    for (Coordinate& point : points)
    {
        // As in OpenRocket, both limits start from the point as it was: when x and y are both
        // too large, the limited y replaces the limited x.
        const Coordinate p = point;
        if (p.x > kSnapLargerThan)
        {
            point = p.setX(kSnapLargerThan);
        }
        if (p.y > kSnapLargerThan)
        {
            point = p.setY(kSnapLargerThan);
        }
    }

    // copy the old points, in case validation fails
    std::vector<Coordinate> pointsCopy = m_points;
    const double            lengthCopy = m_length;

    m_points = std::move(points);

    update(validateFinTab);

    if (intersects())
    {
        // on error, reset to the old points
        m_points = std::move(pointsCopy);
        m_length = lengthCopy;
    }

    fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange);
}

Result<void> FreeformFinSet::setPoint(std::size_t index, double xRequest, double yRequest)
{
    if (getParent() == nullptr)
    {
        return {};
    }

    if (index >= m_points.size())
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "index out of range");
    }

    // if the new x,y would cause a fin larger than our max-size, limit the new request:
    double xAccept = xRequest;
    double yAccept = yRequest;
    if (0 == index)
    {
        const Coordinate& cl        = m_points.back();
        const double      newLength = cl.x - xRequest;
        if (newLength > kSnapLargerThan)
        {
            xAccept = kSnapLargerThan - cl.x;
        }
    }
    else
    {
        xAccept = std::min(xAccept, kSnapLargerThan);
        yAccept = std::min(yAccept, kSnapLargerThan);
    }

    const Coordinate revertPoint = m_points[index];

    m_points[index] = Coordinate{xAccept, yAccept};

    if (kIgnoreSmallerThan > std::abs(revertPoint.x - xAccept) &&
        kIgnoreSmallerThan > std::abs(revertPoint.y - yAccept))
    {
        // no-op. ignore
        return {};
    }

    if ((m_points.size() - 1) == index)
    {
        clampLastPoint(xAccept - revertPoint.x);
    }

    update();

    if (intersects())
    {
        // intersection found: put the point back and give up (Java logs it)
        m_points[index] = revertPoint;
        return {};
    }

    fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange);
    return {};
}

void FreeformFinSet::movePoints(double deltaX, double deltaY)
{
    // zero-out 0th index -- it's the local origin and is always (0,0)
    m_points.front() = Coordinate::kZero;

    for (Coordinate& point : std::span{m_points}.subspan(1))
    {
        point = point.add(deltaX, deltaY, 0.0);
    }
}

std::vector<Coordinate> FreeformFinSet::getFinPoints() const
{
    std::vector<Coordinate> finPoints = m_points;

    alignEndsWithRoot(finPoints);

    return finPoints;
}

double FreeformFinSet::getSpan() const
{
    double highest = 0;
    for (const Coordinate& c : m_points)
    {
        highest = std::max(highest, c.y);
    }

    return highest - MathUtil::javaMin(m_points.back().y, 0);
}

void FreeformFinSet::update()
{
    update(true);
}

void FreeformFinSet::update(bool validateFinTab)
{
    const double oldLength = m_length;
    m_length               = m_points.back().x - m_points.front().x;
    updateAxialPosition();

    if (getParent() != nullptr)
    {
        clampFirstPoint();

        for (std::size_t i = 1; i + 1 < m_points.size(); i++)
        {
            clampInteriorPoint(i);
        }

        clampLastPoint();

        if (oldLength != m_length && validateFinTab)
        {
            validateFinTabLength();
        }
    }
}

void FreeformFinSet::clampFirstPoint()
{
    const SymmetricComponent* const body = symmetricParent();
    QTROCKET_ASSERT(body != nullptr);

    const Coordinate finFront  = getFinFront();
    const double     xFinFront = finFront.x;  // x of fin start, body-frame
    const double     yFinFront = finFront.y;  // y of fin start, body-frame

    const Coordinate p0 = m_points.front();

    if (Coordinate::kZero != p0)
    {
        // The fin cannot start behind its end.
        const double xTrail = m_points.back().x;
        const double xDelta = std::min(p0.x, xTrail);
        const double yDelta = body->getRadius(xFinFront + xDelta) - yFinFront;

        movePoints(-xDelta, -yDelta);

        if (AxialMethod::TOP == getAxialMethod())
        {
            m_axialOffset = m_axialOffset + xDelta;
            m_position    = m_position.add(xDelta, 0, 0);
        }
        else if (AxialMethod::MIDDLE == getAxialMethod())
        {
            m_axialOffset = m_axialOffset + (xDelta / 2);
        }
    }

    m_length = m_points.back().x;
}

void FreeformFinSet::clampInteriorPoint(std::size_t index)
{
    const SymmetricComponent* const sym = symmetricParent();
    QTROCKET_ASSERT(sym != nullptr);

    const Coordinate finFront  = getFinFront();
    const double     xFinFront = finFront.x;  // x of fin start, body-frame
    const double     yFinFront = finFront.y;  // y of fin start, body-frame

    const double xBodyFront = -xFinFront;
    const double xBodyBack  = xBodyFront + sym->getLength();

    const double xPrior = m_points[index].x;
    const double yPrior = m_points[index].y;

    if ((xBodyFront <= xPrior) && (xPrior <= xBodyBack))
    {
        const double yBody = sym->getRadius(xPrior + xFinFront) - yFinFront;

        // ensure that an interior point is outside of its mounting body:
        if (yBody > yPrior)
        {
            m_points[index] = m_points[index].setY(yBody);
        }
    }
}

void FreeformFinSet::clampLastPoint(double xDelta)
{
    const SymmetricComponent* const body = symmetricParent();
    QTROCKET_ASSERT(body != nullptr);

    const Coordinate finFront  = getFinFront();
    const double     xFinStart = finFront.x;  // x of fin start, body-frame
    const double     yFinStart = finFront.y;  // y of fin start, body-frame

    const Coordinate last = m_points.back();

    const double yBody  = body->getRadius(xFinStart + last.x) - yFinStart;
    const double yDelta = yBody - last.y;
    if (kIgnoreSmallerThan < std::abs(yDelta))
    {
        // i.e. if it is close enough above OR is inside the body. In either case, snap it to the
        // body: set the y value to *exactly* match the parent body.
        m_points.back() = Coordinate{last.x, yBody};
    }

    if (kIgnoreSmallerThan < std::abs(xDelta))
    {
        m_length = m_points.back().x;
        if (AxialMethod::MIDDLE == getAxialMethod())
        {
            m_axialOffset = m_axialOffset + (xDelta / 2);
        }
        else if (AxialMethod::BOTTOM == getAxialMethod())
        {
            m_axialOffset = m_axialOffset + xDelta;
        }
    }
}

bool FreeformFinSet::intersects() const
{
    for (std::size_t index = 0; index + 1 < m_points.size(); ++index)
    {
        if (intersects(index))
        {
            return true;
        }
    }
    return false;
}

bool FreeformFinSet::intersects(std::size_t targetIndex) const
{
    // (Java logs an error for a segment that does not exist; intersects() asks for none.)
    QTROCKET_ASSERT(targetIndex + 1 < m_points.size());

    const Point2D pt1{m_points[targetIndex].x, m_points[targetIndex].y};
    const Point2D pt2{m_points[targetIndex + 1].x, m_points[targetIndex + 1].y};

    for (std::size_t comparisonIndex = targetIndex + 1; comparisonIndex + 1 < m_points.size();
         ++comparisonIndex)
    {
        if (comparisonIndex - targetIndex < 2)
        {
            // a line segment will trivially not intersect with itself, nor can adjacent line
            // segments intersect with each other, because they share a common endpoint.
            continue;
        }
        const Point2D pc1{m_points[comparisonIndex].x, m_points[comparisonIndex].y};
        const Point2D pc2{m_points[comparisonIndex + 1].x, m_points[comparisonIndex + 1].y};

        // special case for when the first and last points are co-located.
        if ((0 == targetIndex) && (m_points.size() == comparisonIndex + 2) &&
            (kIgnoreSmallerThan > distance(pt1, pc2)))
        {
            continue;
        }

        if (segmentsIntersect(pc1, pc2, pt1, pt2))
        {
            return true;
        }
    }
    return false;
}

}  // namespace QtRocket
