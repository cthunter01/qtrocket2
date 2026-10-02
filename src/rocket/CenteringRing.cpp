#include "QtRocket/rocket/CenteringRing.h"

#include <memory>
#include <vector>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/RadiusRingComponent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

namespace
{

/// The x of the first coordinate of @p coordinates (Java: toRelative(...)[0].getX()).
[[nodiscard]] double firstX(const std::vector<Coordinate>& coordinates)
{
    // Java: ArrayIndexOutOfBoundsException; every component has at least one location.
    QTROCKET_ASSERT(!coordinates.empty());
    return coordinates.front().x;
}

}  // namespace

CenteringRing::CenteringRing()
{
    setOuterRadiusAutomatic(true);
    setInnerRadiusAutomatic(true);
    setLength(0.002);
    setDisplayOrderSide(7);  // Order for displaying the component in the 2D side view
    setDisplayOrderBack(5);  // Order for displaying the component in the 2D back view
}

std::unique_ptr<RocketComponent> CenteringRing::cloneShallow() const
{
    return std::make_unique<CenteringRing>(*this);
}

double CenteringRing::getInnerRadius() const
{
    // The sibling inner radius automation.
    if (isInnerRadiusAutomatic())
    {
        m_innerRadius = 0;
        // The ring has no parent when it is detached from a rocket.
        if (m_parent != nullptr)
        {
            const RocketComponent& parent = *m_parent;
            for (const RocketComponent* sibling : parent.getChildren())
            {
                // Only inner tubes are considered for the automatic inner radius (for now).
                const auto* tube = dynamic_cast<const InnerTube*>(sibling);
                if (tube == nullptr)  // excludes this ring
                {
                    continue;
                }

                const double pos1 = firstX(toRelative(Coordinate::kNul, *sibling));
                const double pos2 = firstX(toRelative(Coordinate{getLength()}, *sibling));
                if (pos2 < 0 || pos1 > sibling->getLength())
                {
                    continue;
                }

                m_innerRadius = MathUtil::javaMax(m_innerRadius, tube->getOuterRadius());
            }
            m_innerRadius = MathUtil::javaMin(m_innerRadius, getOuterRadius());
        }
    }

    return RadiusRingComponent::getInnerRadius();
}

bool CenteringRing::allowsChildren() const
{
    return false;
}

bool CenteringRing::isCompatible(ComponentKind /*kind*/) const
{
    return false;
}

}  // namespace QtRocket
