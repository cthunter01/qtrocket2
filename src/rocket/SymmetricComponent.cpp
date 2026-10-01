#include "QtRocket/rocket/SymmetricComponent.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/RingInstanceable.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

namespace
{

using MathUtil::pow2;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// calculateCG(): the CG (relative to the fore end) and volume of a filled conical frustum of
/// length @p l and radii @p r1 (fore) and @p r2 (aft), correct for r1 = r2, r1 = 0 and r2 = 0.
/// Caution: the volume (the weight) is 3 / pi times the true value, which calculateProperties()
/// corrects at the end.
[[nodiscard]] Coordinate calculateCG(double l, double r1, double r2)
{
    const double volume = l * (pow2(r1) + (r1 * r2) + pow2(r2));

    double cg = 0;
    if (volume < MathUtil::kEpsilon)
    {
        cg = l / 2.0;
    }
    else
    {
        cg = (l * (pow2(r1) + (2.0 * r1 * r2) + (3 * pow2(r2)))) /
             (4.0 * (pow2(r1) + (r1 * r2) + pow2(r2)));
    }

    return Coordinate{cg, 0, 0, volume};
}

/// calculateUnitRotMOI(): the unit rotational moment of inertia of a solid frustum, the cylinder
/// included (AIP handbook table 2c-2). Caution: 10/3 times the true value, corrected at the end.
[[nodiscard]] double calculateUnitRotMOI(double r1, double r2)
{
    // check for cylinder special case
    if (std::abs(r1 - r2) < MathUtil::kEpsilon)
    {
        return 10.0 * pow2(r1) / 6.0;
    }

    return (std::pow(r2, 5.0) - std::pow(r1, 5.0)) / (std::pow(r2, 3.0) - std::pow(r1, 3.0));
}

/// calculateLongMOICone(): the longitudinal moment of inertia of a cone of height @p h and
/// radius @p r about its tip. Caution: needs multiplying by pi.
[[nodiscard]] double calculateLongMOICone(double h, double r)
{
    const double m   = pow2(r) * h;
    const double ixx = 3 * m * ((pow2(r) / 20.0) + (pow2(h) / 5.0));

    return ixx;
}

/// calculateLongMOI(): the longitudinal moment of inertia of a solid frustum about its CG, the
/// difference of two cones (the cylinder handled apart); @p cg is calculateCG()'s result.
/// Caution: needs multiplying by pi.
[[nodiscard]] double calculateLongMOI(double l, double r1, double r2, const Coordinate& cg)
{
    // check for cylinder special case
    if (std::abs(r1 - r2) < MathUtil::kEpsilon)
    {
        // compute MOI of cylinder relative to CG of cylinder
        return cg.weight * ((3 * pow2(r1)) + pow2(l)) / 12.0;
    }

    // is the frustum "small end forward" or "small end aft"?
    double shiftCG = cg.x;
    if (r1 > r2)
    {
        std::swap(r1, r2);
        shiftCG = l - cg.x;
    }

    // Find the heights of the two cones. Note that the h1 and h2 being calculated here
    // are NOT the x1 and x2 used in calculateProperties()
    const double h2 = l * r2 / (r2 - r1);
    const double h1 = h2 * r1 / r2;

    const double moi1 = calculateLongMOICone(h1, r1);
    const double moi2 = calculateLongMOICone(h2, r2);

    // compute MOI relative to tip of cones (they share the same tip, of course)
    double moi = moi2 - moi1;

    // use parallel axis theorem to move MOI to be relative to CG of frustum.
    moi = moi - (pow2(h1 + shiftCG) * cg.weight);

    return moi;
}

/// Java's getChildPosition(): the index of @p child in @p parent, -1 when it is not a child.
[[nodiscard]] int childPosition(const RocketComponent& parent, const RocketComponent* child)
{
    const std::optional<std::size_t> position = parent.getChildPosition(child);
    return position ? static_cast<int>(*position) : -1;
}

/// The child @p index (>= 0) of @p parent.
template <class Component>
[[nodiscard]] Component& childAt(Component& parent, int index)
{
    return parent.getChild(static_cast<std::size_t>(index));
}

}  // namespace

SymmetricComponent::SymmetricComponent() : ExternalComponent(AxialMethod::AFTER) { }

// ========================================================================== BodyComponent

void SymmetricComponent::setLength(double length)
{
    if (m_length == length)
    {
        return;
    }
    m_length = MathUtil::javaMax(length, 0);
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

bool SymmetricComponent::allowsChildren() const
{
    return true;
}

double SymmetricComponent::getInnerRadius() const
{
    // BodyComponent's field InnerRadius, which OpenRocket never assigns.
    return 0.0;
}

// ============================================================================ the profile

// The fore radius is read before the aft one in a statement of its own, as Java evaluates them:
// both may refresh automatic radii, and C++ leaves the order of function arguments unspecified.

double SymmetricComponent::getMaxRadius() const
{
    const double fore = getForeRadius();
    return MathUtil::max(fore, getAftRadius());
}

double SymmetricComponent::getThickness() const
{
    if (m_filled)
    {
        const double fore = getForeRadius();
        return MathUtil::javaMax(fore, getAftRadius());
    }
    return m_thickness;
}

void SymmetricComponent::setThickness(double thickness, bool doClamping)
{
    if ((m_thickness == thickness) && !m_filled)
    {
        return;
    }
    m_thickness = doClamping ? MathUtil::clamp(thickness, 0, getMaxRadius()) : thickness;
    m_filled    = false;
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
    clearPreset();
}

void SymmetricComponent::setThickness(double thickness)
{
    setThickness(thickness, true);
}

void SymmetricComponent::setFilled(bool filled)
{
    if (m_filled == filled)
    {
        return;
    }
    m_filled = filled;
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
    clearPreset();
}

bool SymmetricComponent::isAfter() const
{
    return true;
}

// ================================================================================= bounds

BoundingBox SymmetricComponent::getInstanceBoundingBox() const
{
    BoundingBox instanceBounds;

    instanceBounds.update(Coordinate{getLength(), 0, 0});

    const double fore = getForeRadius();  // before the aft radius (see getMaxRadius())
    const double r    = MathUtil::javaMax(fore, getAftRadius());
    instanceBounds.update(Coordinate{0, r, r});
    instanceBounds.update(Coordinate{0, -r, -r});

    return instanceBounds;
}

std::vector<Coordinate> SymmetricComponent::getComponentBounds() const
{
    std::vector<Coordinate> list;
    list.reserve(24);
    for (int n = 0; n <= 5; n++)
    {
        const double x = n * getLength() / 5;
        const double r = getRadius(x);
        addBound(list, x, r);
    }
    return list;
}

// ===================================================================== integrated values

void SymmetricComponent::loadFromPreset(const ComponentPreset&   preset,
                                        const PresetLoadOptions& options)
{
    ExternalComponent::loadFromPreset(preset, options);

    if (preset.has(ComponentPreset::kThickness))
    {
        m_thickness = preset.get(ComponentPreset::kThickness);
        m_filled    = false;
    }
    if (preset.has(ComponentPreset::kFilled))
    {
        m_filled = preset.get(ComponentPreset::kFilled);
    }
}

double SymmetricComponent::getComponentVolume() const
{
    if (std::isnan(m_volume))
    {
        calculateProperties();
    }
    return m_volume;
}

double SymmetricComponent::getFullVolume() const
{
    if (std::isnan(m_fullVolume))
    {
        calculateProperties();
    }
    return m_fullVolume;
}

double SymmetricComponent::getComponentWetArea() const
{
    if (std::isnan(m_wetArea))
    {
        calculateProperties();
    }
    return m_wetArea;
}

double SymmetricComponent::getComponentPlanformArea() const
{
    if (std::isnan(m_planArea))
    {
        calculateProperties();
    }
    return m_planArea;
}

double SymmetricComponent::getComponentPlanformCenter() const
{
    if (std::isnan(m_planCenter))
    {
        calculateProperties();
    }
    return m_planCenter;
}

Coordinate SymmetricComponent::getComponentCG() const
{
    return getSymmetricComponentCG();
}

Coordinate SymmetricComponent::getSymmetricComponentCG() const
{
    if (!m_cg)
    {
        calculateProperties();
    }
    // calculateProperties() always sets it.
    return m_cg.value_or(Coordinate{});
}

double SymmetricComponent::getLongitudinalUnitInertia() const
{
    if (std::isnan(m_longitudinalUnitInertia))
    {
        calculateProperties();
    }
    return m_longitudinalUnitInertia;
}

double SymmetricComponent::getRotationalUnitInertia() const
{
    if (std::isnan(m_rotationalUnitInertia))
    {
        calculateProperties();
    }
    return m_rotationalUnitInertia;
}

void SymmetricComponent::calculateProperties() const
{
    m_wetArea                 = 0;
    m_planArea                = 0;
    m_planCenter              = 0;
    m_fullVolume              = 0;
    m_volume                  = 0;
    m_longitudinalUnitInertia = 0;
    m_rotationalUnitInertia   = 0;
    m_cg                      = Coordinate{};

    double cgx = 0;

    // Check length > 0
    if (getLength() < MathUtil::kEpsilon)
    {
        return;
    }

    // Integrate for volume, CG, wetted area, planform area, and moments of inertia
    for (int n = 0; n < kDivisions; n++)
    {
        // x1 and x2 are the bounds on this division, hyp is the length of the hypotenuse from r1
        // to r2, height is the y-axis height of the component if not filled, r1o and r2o are the
        // outer radii, r1i and r2i are the inner radii.

        // get x bounds and length for this division
        const double x1 = n * getLength() / kDivisions;
        const double x2 = (n + 1) * getLength() / kDivisions;
        const double l  = x2 - x1;

        // get outer and inner radii
        const double r1o = getRadius(x1);
        const double r2o = getRadius(x2);

        // use thickness and angle of outer wall to get height of ring
        const double hyp    = MathUtil::hypot(r2o - r1o, l);
        const double height = m_thickness * hyp / l;

        // get inner radii.
        double r1i = 0;
        double r2i = 0;
        if (!m_filled)
        {
            // Tiny inaccuracy is introduced on a division where one end is closed and other is
            // open.
            r1i = MathUtil::max(r1o - height, 0);
            r2i = MathUtil::max(r2o - height, 0);
        }

        // find volume and CG of (possibly hollow) frustum
        const Coordinate fullCG  = calculateCG(l, r1o, r2o);
        const Coordinate innerCG = calculateCG(l, r1i, r2i);

        const double dFullV = fullCG.weight;
        const double dV     = fullCG.weight - innerCG.weight;
        // Some discontinuous profiles contain slices with no material. Their CG is irrelevant,
        // but it must remain finite so that 0 * dCG does not become NaN.
        const double dCG =
            dV == 0.0 ? l / 2.0 : ((fullCG.x * fullCG.weight) - (innerCG.x * innerCG.weight)) / dV;

        // First moment, used later for CG calculation
        const double dCGx = dV * (x1 + dCG);

        // rotational moment of inertia
        const double ixxo = calculateUnitRotMOI(r1o, r2o);
        const double ixxi = calculateUnitRotMOI(r1i, r2i);
        const double ixx  = (ixxo * fullCG.weight) - (ixxi * innerCG.weight);

        // longitudinal moment of inertia -- axis through CG of division
        double iyy = calculateLongMOI(l, r1o, r2o, fullCG) - calculateLongMOI(l, r1i, r2i, innerCG);

        // move to axis through forward end of component
        iyy += dV * pow2(x1 + dCG);

        // Add to the volume-related components
        m_volume += dV;
        m_fullVolume += dFullV;
        cgx += dCGx;
        m_rotationalUnitInertia += ixx;
        m_longitudinalUnitInertia += iyy;

        // Wetted area ( * PI at the end)
        m_wetArea += (r1o + r2o) * std::sqrt(pow2(r1o - r2o) + pow2(l));

        // Planform area & moment
        const double dA = l * (r1o + r2o);
        m_planArea += dA;
        const double planMoment = (dA * x1) + (2.0 * pow2(l) * ((r1o / 6.0) + (r2o / 3.0)));
        m_planCenter += planMoment;
    }

    if (m_planArea > 0)
    {
        m_planCenter /= m_planArea;
    }

    // get unit moments of inertia
    m_rotationalUnitInertia /= m_volume;
    m_longitudinalUnitInertia /= m_volume;

    // Correct for deferred constant factors
    m_volume *= std::numbers::pi / 3.0;
    m_fullVolume *= std::numbers::pi / 3.0;
    cgx *= std::numbers::pi / 3.0;
    m_wetArea *= std::numbers::pi;
    m_rotationalUnitInertia *= 3.0 / 10.0;

    Coordinate cg;
    if (m_volume < 0.0000000001)
    {
        // 0.1 mm^3
        m_volume = 0;
        cg       = Coordinate{getLength() / 2, 0, 0, 0};
    }
    else
    {
        // the mass of this shape is the material density * volume. it cannot come from
        // getComponentMass() since that includes the shoulders
        cg = Coordinate{cgx / m_volume, 0, 0, getMaterial().getDensity() * m_volume};
    }
    m_cg = cg;

    // a component so small it has no volume can't contribute to moment of inertia
    if (MathUtil::equals(m_volume, 0))
    {
        m_rotationalUnitInertia   = 0;
        m_longitudinalUnitInertia = 0;
        return;
    }

    // Shift longitudinal inertia to CG
    m_longitudinalUnitInertia = m_longitudinalUnitInertia - pow2(cg.x);
}

void SymmetricComponent::componentChanged(const ComponentChangeEvent& event)
{
    ExternalComponent::componentChanged(event);
    if (event.isAerodynamicChange() || event.isMassChange())
    {
        m_wetArea                 = kNaN;
        m_planArea                = kNaN;
        m_planCenter              = kNaN;
        m_volume                  = kNaN;
        m_fullVolume              = kNaN;
        m_longitudinalUnitInertia = kNaN;
        m_rotationalUnitInertia   = kNaN;
        m_cg.reset();
    }
}

// ============================================================================= neighbours

template <class Self>
SymmetricComponent::SymmetricPointer<Self> SymmetricComponent::previousSymmetricOf(Self& self)
{
    using Symmetric = std::remove_pointer_t<SymmetricPointer<Self>>;
    using Component =
        std::conditional_t<std::is_const_v<Self>, const RocketComponent, RocketComponent>;

    Component* parent = self.getParent();
    if ((nullptr == parent) || (nullptr == parent->getParent()))
    {
        return nullptr;
    }

    // might be: (a) Rocket -- for Centerline/Axial stages
    // (b) BodyTube -- for Parallel Stages & PodSets
    Component* grandParent = parent->getParent();

    int searchParentIndex =
        childPosition(*grandParent, parent);  // position of component w/in parent
    int searchSiblingIndex =
        childPosition(*parent, &self) - 1;  // guess at index of previous component

    while (0 <= searchParentIndex)
    {
        Component& searchParent = childAt(*grandParent, searchParentIndex);

        if (dynamic_cast<const ComponentAssembly*>(&searchParent) != nullptr)
        {
            while (0 <= searchSiblingIndex)
            {
                Component& searchSibling = childAt(searchParent, searchSiblingIndex);
                auto*      symmetric     = dynamic_cast<Symmetric*>(&searchSibling);
                if ((symmetric != nullptr) && self.isInline(searchSibling))
                {
                    return previousFromAssemblies<Symmetric>(symmetric, symmetric, 0);
                }
                --searchSiblingIndex;
            }
        }

        // Look forward to the previous stage
        --searchParentIndex;

        if (0 <= searchParentIndex)
        {
            searchSiblingIndex =
                static_cast<int>(childAt(*grandParent, searchParentIndex).getChildCount()) - 1;
        }
    }

    // one last thing -- I could be the child of a ComponentAssembly, and in line with
    // the SymmetricComponent that is my grandParent
    auto* symmetricGrandParent = dynamic_cast<Symmetric*>(grandParent);
    if ((symmetricGrandParent != nullptr) && self.isInline(*grandParent))
    {
        // If the grandparent is actually before me, then this is the previous component
        if ((parent->getAxialOffset(AxialMethod::TOP) + self.getAxialOffset(AxialMethod::TOP)) > 0)
        {
            return symmetricGrandParent;
        }
        // If not, then search for the component before the grandparent
        // NOTE: will be incorrect if the ComponentAssembly is even further to the front than the
        // previous component of the grandparent. But that would be really bad rocket design...
        return previousSymmetricOf(*symmetricGrandParent);
    }

    return nullptr;
}

template <class Symmetric>
Symmetric* SymmetricComponent::previousFromAssemblies(Symmetric* parent, Symmetric* previous,
                                                      double flushOffset)
{
    if (previous == nullptr)
    {
        return parent;
    }
    if (parent == nullptr)
    {
        return previous;
    }

    double     maxRadius         = previous->isAftRadiusAutomatic() ? 0 : previous->getAftRadius();
    Symmetric* previousComponent = previous;
    for (auto* assembly : parent->getDirectChildAssemblies())
    {
        if (assembly->getChildCount() == 0)
        {
            continue;
        }
        // Check if the component assembly's last child is a symmetric component that is:
        // - inline with the parent
        // - flush with the end of the parent
        // - larger in aft radius than the parent
        // in that case, this component assembly is the new previousComponent.
        auto& lastChild          = assembly->getChild(assembly->getChildCount() - 1);
        auto* lastSymmetricChild = dynamic_cast<Symmetric*>(&lastChild);
        if (!((lastSymmetricChild != nullptr) && parent->isInline(lastChild)))
        {
            continue;
        }
        // How much the last child is flush with the parent
        const double flushDeviation = flushOffset + assembly->getAxialOffset(AxialMethod::BOTTOM);

        // If the last symmetric child from the assembly if flush with the end of the parent and
        // larger than the current previous component, then this is the new previous component
        if (MathUtil::equals(flushDeviation, 0) && !lastSymmetricChild->isAftRadiusAutomatic() &&
            lastSymmetricChild->getAftRadius() > maxRadius)
        {
            previousComponent = lastSymmetricChild;
            maxRadius         = previousComponent->getAftRadius();
        }
        // It could be that there is a child component assembly that is flush with the end of the
        // parent or larger. Recursively check assembly's children
        previousComponent = previousFromAssemblies<Symmetric>(lastSymmetricChild, previousComponent,
                                                              flushDeviation);
        if ((previousComponent != nullptr) && !previousComponent->isAftRadiusAutomatic())
        {
            maxRadius = previousComponent->getAftRadius();
        }
    }

    return previousComponent;
}

template <class Self>
SymmetricComponent::SymmetricPointer<Self> SymmetricComponent::nextSymmetricOf(Self& self)
{
    using Symmetric = std::remove_pointer_t<SymmetricPointer<Self>>;
    using Component =
        std::conditional_t<std::is_const_v<Self>, const RocketComponent, RocketComponent>;

    Component* parent = self.getParent();
    if ((nullptr == parent) || (nullptr == parent->getParent()))
    {
        return nullptr;
    }

    // might be: (a) Rocket -- for centerline stages
    // (b) BodyTube -- for Parallel Stages
    Component* grandParent = parent->getParent();

    // note: this is not guaranteed to _contain_ a stage... but that we're _searching_ for one.
    int searchParentIndex  = childPosition(*grandParent, parent);
    int searchSiblingIndex = childPosition(*parent, &self) + 1;

    while (searchParentIndex < static_cast<int>(grandParent->getChildCount()))
    {
        Component& searchParent = childAt(*grandParent, searchParentIndex);

        if (dynamic_cast<const ComponentAssembly*>(&searchParent) != nullptr)
        {
            while (searchSiblingIndex < static_cast<int>(searchParent.getChildCount()))
            {
                Component& searchSibling = childAt(searchParent, searchSiblingIndex);
                auto*      symmetric     = dynamic_cast<Symmetric*>(&searchSibling);
                if ((symmetric != nullptr) && self.isInline(searchSibling))
                {
                    return nextFromAssemblies<Symmetric>(symmetric, symmetric, 0);
                }
                ++searchSiblingIndex;
            }
        }

        // Look aft to the next stage
        ++searchParentIndex;
        searchSiblingIndex = 0;
    }

    // One last thing -- I could have a child that is a ComponentAssembly that is in line
    // with me
    return nextInOwnAssemblies(self, *parent);
}

template <class Self>
SymmetricComponent::SymmetricPointer<Self> SymmetricComponent::nextInOwnAssemblies(
    Self& self, const RocketComponent& parent)
{
    using Symmetric = std::remove_pointer_t<SymmetricPointer<Self>>;

    for (auto* child : self.getChildren())
    {
        if (dynamic_cast<const ComponentAssembly*>(child) == nullptr)
        {
            continue;
        }
        for (auto* grandchild : child->getChildren())
        {
            auto* symmetricGrandchild = dynamic_cast<Symmetric*>(grandchild);
            if ((symmetricGrandchild != nullptr) && self.isInline(*grandchild))
            {
                // If the grandparent is actually after me, then this is the next component
                if ((parent.getAxialOffset(AxialMethod::BOTTOM) +
                     self.getAxialOffset(AxialMethod::BOTTOM)) < 0)
                {
                    return symmetricGrandchild;
                }
                // If not, then search for the component after the grandparent
                // NOTE: will be incorrect if the ComponentAssembly is even further to the back
                // than the next component of the grandparent. But that would be really bad
                // rocket design...
                return nextSymmetricOf(*symmetricGrandchild);
            }
        }
    }

    return nullptr;
}

template <class Symmetric>
Symmetric* SymmetricComponent::nextFromAssemblies(Symmetric* parent, Symmetric* next,
                                                  double flushOffset)
{
    if (next == nullptr)
    {
        return parent;
    }
    if (parent == nullptr)
    {
        return next;
    }

    double     maxRadius     = next->isForeRadiusAutomatic() ? 0 : next->getForeRadius();
    Symmetric* nextComponent = next;
    for (auto* assembly : parent->getDirectChildAssemblies())
    {
        if (assembly->getChildCount() == 0)
        {
            continue;
        }
        // Check if the component assembly's first child is a symmetric component that is:
        // - inline with the parent
        // - flush with the front of the parent
        // - larger in fore radius than the parent
        // in that case, this component assembly is the new nextComponent.
        auto& firstChild          = assembly->getChild(0);
        auto* firstSymmetricChild = dynamic_cast<Symmetric*>(&firstChild);
        if (!((firstSymmetricChild != nullptr) && parent->isInline(firstChild)))
        {
            continue;
        }
        // How much the first child is flush with the parent
        const double flushDeviation = flushOffset + assembly->getAxialOffset(AxialMethod::TOP);

        // If the first symmetric child from the assembly if flush with the front of the parent
        // and larger than the current next component, then this is the new next component
        if (MathUtil::equals(flushDeviation, 0) && !firstSymmetricChild->isForeRadiusAutomatic() &&
            firstSymmetricChild->getForeRadius() > maxRadius)
        {
            nextComponent = firstSymmetricChild;
            maxRadius     = nextComponent->getForeRadius();
        }
        // It could be that there is a child component assembly that is flush with the front of
        // the parent or larger. Recursively check assembly's children
        nextComponent =
            nextFromAssemblies<Symmetric>(firstSymmetricChild, nextComponent, flushDeviation);
        if ((nextComponent != nullptr) && !nextComponent->isForeRadiusAutomatic())
        {
            maxRadius = nextComponent->getForeRadius();
        }
    }

    return nextComponent;
}

const SymmetricComponent* SymmetricComponent::getPreviousSymmetricComponent() const
{
    return previousSymmetricOf(*this);
}

SymmetricComponent* SymmetricComponent::getPreviousSymmetricComponent()
{
    return previousSymmetricOf(*this);
}

const SymmetricComponent* SymmetricComponent::getNextSymmetricComponent() const
{
    return nextSymmetricOf(*this);
}

SymmetricComponent* SymmetricComponent::getNextSymmetricComponent()
{
    return nextSymmetricOf(*this);
}

bool SymmetricComponent::isInline(const RocketComponent& candidate) const
{
    // if we share a parent, we are in line
    if (m_parent == candidate.getParent())
    {
        return true;
    }

    // if both of our parents are either not ring instanceable, or have a radial offset of 0 from
    // their centerline, we are in line.
    if ((dynamic_cast<const RingInstanceable*>(m_parent) != nullptr) &&
        !MathUtil::equals(::QtRocket::getRadius(m_parent->getRadiusMethod(), m_parent->getParent(),
                                                this, m_parent->getRadiusOffset()),
                          0))
    {
        return false;
    }

    const RocketComponent* candidateParent = candidate.getParent();
    if (dynamic_cast<const RingInstanceable*>(candidateParent) != nullptr)
    {
        // We need to check if the grandparent of the candidate is a body tube and if the outer
        // radius is automatic. If so, then this would cause an infinite loop when checking the
        // radius of the radiusMethod.
        if (candidateParent->getParent() == this &&
            (isAftRadiusAutomatic() || isForeRadiusAutomatic()))
        {
            return false;
        }
        return MathUtil::equals(
            ::QtRocket::getRadius(candidateParent->getRadiusMethod(), candidateParent->getParent(),
                                  &candidate, candidateParent->getRadiusOffset()),
            0);
    }

    return true;
}

}  // namespace QtRocket
