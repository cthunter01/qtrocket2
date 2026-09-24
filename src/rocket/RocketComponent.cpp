#include "QtRocket/rocket/RocketComponent.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
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
#include <typeinfo>
#include <utility>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/Instanceable.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/position/AnglePositionable.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Transformation.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

namespace
{

/// Java's Math.max(value, 0) for the override mass: a NaN stays NaN and -0.0 becomes 0.0.
[[nodiscard]] double javaMaxZero(double value) noexcept
{
    return (value > 0.0 || std::isnan(value)) ? value : 0.0;
}

/// Java's name.matches("^\\s*$"): empty, or only the regex \s characters.
[[nodiscard]] bool isJavaBlank(std::string_view text) noexcept
{
    return std::ranges::all_of(text, [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\x0B' || c == '\f' || c == '\r';
    });
}

/// Axial rotations as angle coordinates (x = rotation about the x axis).
[[nodiscard]] std::vector<Coordinate> axialRotToCoord(const std::vector<double>& angles)
{
    std::vector<Coordinate> coords;
    coords.reserve(angles.size());
    for (const double angle : angles)
    {
        coords.emplace_back(angle, 0.0, 0.0);
    }
    return coords;
}

}  // namespace

// ================================================================================ lifetime

RocketComponent::RocketComponent(AxialMethod axialMethod)
  : m_axialMethod(axialMethod), m_id(Uuid::random())
{
    // The name stays empty: getName() gives getComponentName() (see the class comment).
}

RocketComponent::RocketComponent(const RocketComponent& other)
  : m_length(other.m_length),
    m_axialOffset(other.m_axialOffset),
    m_overrideMass(other.m_overrideMass),
    m_position(other.m_position),
    m_name(other.m_name),
    m_axialMethod(other.m_axialMethod),
    m_massOverridden(other.m_massOverridden),
    m_overrideCGX(other.m_overrideCGX),
    m_overrideCD(other.m_overrideCD),
    m_massOverriddenBy(other.m_massOverriddenBy),
    m_cgOverriddenBy(other.m_cgOverriddenBy),
    m_cdOverriddenBy(other.m_cdOverriddenBy),
    m_presetComponent(other.m_presetComponent),
    m_id(other.m_id),
    m_comment(other.m_comment),
    m_appearance(other.m_appearance),
    m_displayOrderSide(other.m_displayOrderSide),
    m_displayOrderBack(other.m_displayOrderBack),
    m_lineStyle(other.m_lineStyle),
    m_color(other.m_color),
    m_cgOverridden(other.m_cgOverridden),
    m_overrideSubcomponentsMass(other.m_overrideSubcomponentsMass),
    m_overrideSubcomponentsCG(other.m_overrideSubcomponentsCG),
    m_overrideSubcomponentsCD(other.m_overrideSubcomponentsCD),
    m_cdOverridden(other.m_cdOverridden),
    m_ignorePresetClearing(other.m_ignorePresetClearing),
    m_isVisible(other.m_isVisible)
{
    // Not copied: the parent and children (the copy is detached and copyWithOriginalId() adds
    // copied children), the cached locations, and the event bypass (Java's clone() clears it).
}

RocketComponent::~RocketComponent() = default;

// ======================================================================= overridable basics

std::string RocketComponent::getComponentName() const
{
    return std::string{displayName(kind())};
}

bool RocketComponent::isAfter() const
{
    return AxialMethod::AFTER == m_axialMethod;
}

bool RocketComponent::isAxisymmetric() const
{
    return true;
}

std::vector<Material> RocketComponent::getAllMaterials() const
{
    return {};
}

int RocketComponent::getInstanceCount() const
{
    return 1;
}

void RocketComponent::setInstanceCount(int /*count*/)
{
    // Does nothing: this component does not support multiple instances (Java logs a warning).
}

bool RocketComponent::isMotorMount() const
{
    return false;
}

void RocketComponent::componentChanged(const ComponentChangeEvent& /*event*/)
{
    clearCoordinateCaches();
    update();
}

void RocketComponent::clearCoordinateCaches() const noexcept
{
    m_cachedComponentLocations.reset();
    m_cachedComponentAngles.reset();
}

std::string RocketComponent::toString() const
{
    return getName();
}

// ======================================================================= common properties

void RocketComponent::setAppearance(std::optional<Appearance> appearance)
{
    m_appearance = std::move(appearance);
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

void RocketComponent::setColor(std::optional<Color> color)
{
    if (m_color == color)
    {
        return;
    }
    m_color = color;
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

void RocketComponent::setLineStyle(std::optional<LineStyle> style)
{
    if (m_lineStyle == style)
    {
        return;
    }
    m_lineStyle = style;
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

std::string RocketComponent::getName() const
{
    return m_name.empty() ? getComponentName() : m_name;
}

void RocketComponent::setName(std::string_view name)
{
    if (getName() == name)
    {
        return;
    }
    if (isJavaBlank(name))
    {
        m_name.clear();  // getComponentName()
    }
    else
    {
        m_name = name;
    }

    if (dynamic_cast<const AxialStage*>(this) != nullptr)
    {
        fireComponentChangeEvent(ComponentChangeEvent::kTreeChange);
    }
    else
    {
        fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    }
}

void RocketComponent::setComment(std::string_view comment)
{
    if (m_comment == comment)
    {
        return;
    }
    m_comment = comment;
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

Result<void> RocketComponent::setId(std::string_view newId)
{
    const auto parsed = Uuid::parse(newId);
    if (!parsed)
    {
        return std::unexpected(parsed.error());
    }
    m_id = *parsed;
    return {};
}

std::string RocketComponent::getDebugName() const
{
    return getName() + "/" + m_id.toString().substr(0, 8);
}

void RocketComponent::clearPreset()
{
    if (m_presetComponent == nullptr || m_ignorePresetClearing)
    {
        return;
    }
    m_presetComponent = nullptr;
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

void RocketComponent::setVisible(bool value)
{
    m_isVisible = value;
    fireComponentChangeEvent(ComponentChangeEvent::kGraphicChange);
}

// =============================================================================== overrides

double RocketComponent::getOverrideMass() const
{
    if (!isMassOverridden())
    {
        m_overrideMass = getComponentMass();
    }
    return m_overrideMass;
}

void RocketComponent::setOverrideMass(double m)
{
    if (MathUtil::equals(m, m_overrideMass))
    {
        return;
    }
    m_overrideMass = javaMaxZero(m);
    if (m_massOverridden)
    {
        fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
    }
}

void RocketComponent::setMassOverridden(bool overridden)
{
    if (m_massOverridden == overridden)
    {
        return;
    }
    m_massOverridden = overridden;

    // Without the override, the override mass follows the component mass.
    if (!m_massOverridden)
    {
        m_overrideMass = getComponentMass();
    }

    updateChildrenMassOverriddenBy();
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

Coordinate RocketComponent::getOverrideCG() const
{
    if (!isCGOverridden())
    {
        m_overrideCGX = getComponentCG().x;
    }
    return getComponentCG().setX(m_overrideCGX);
}

double RocketComponent::getOverrideCGX() const
{
    if (!isCGOverridden())
    {
        m_overrideCGX = getComponentCG().x;
    }
    return m_overrideCGX;
}

void RocketComponent::setOverrideCGX(double x)
{
    if (MathUtil::equals(m_overrideCGX, x))
    {
        return;
    }
    m_overrideCGX = x;
    if (isCGOverridden())
    {
        fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
    }
    else
    {
        fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    }
}

void RocketComponent::setCGOverridden(bool overridden)
{
    if (m_cgOverridden == overridden)
    {
        return;
    }
    m_cgOverridden = overridden;

    // Without the override, the override CG follows the component CG.
    if (!m_cgOverridden)
    {
        m_overrideCGX = getComponentCG().x;
    }

    updateChildrenCGOverriddenBy();
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void RocketComponent::setOverrideCD(double x)
{
    if (MathUtil::equals(m_overrideCD, x))
    {
        return;
    }
    m_overrideCD = x;

    if (isCDOverridden())
    {
        if (isSubcomponentsOverriddenCD())
        {
            overrideSubcomponentsCD(true);
        }
        fireComponentChangeEvent(ComponentChangeEvent::kAerodynamicChange);
    }
    else
    {
        fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    }
}

void RocketComponent::setCDOverridden(bool overridden)
{
    if (m_cdOverridden == overridden)
    {
        return;
    }
    m_cdOverridden = overridden;
    updateChildrenCDOverriddenBy();

    // Overriding our own CD with the subcomponents flag set overrides every descendant, and
    // clearing it clears them.
    if (isSubcomponentsOverriddenCD())
    {
        overrideSubcomponentsCD(overridden);
    }

    // Deferred to aero/: without the override Java refreshes m_overrideCD from getComponentCD(0,
    // 0, defaultMach, 0) here (see the class comment).

    fireComponentChangeEvent(ComponentChangeEvent::kAerodynamicChange);
}

bool RocketComponent::isCDOverriddenByAncestor() const
{
    return (nullptr != m_parent) &&
           (m_parent->isCDOverriddenByAncestor() ||
            (m_parent->isCDOverridden() && m_parent->isSubcomponentsOverriddenCD()));
}

bool RocketComponent::isSubcomponentsOverriddenMass() const
{
    return m_overrideSubcomponentsMass;
}

void RocketComponent::setSubcomponentsOverridden(bool override)
{
    setSubcomponentsOverriddenMass(override);
    setSubcomponentsOverriddenCG(override);
    setSubcomponentsOverriddenCD(override);
}

void RocketComponent::setSubcomponentsOverriddenMass(bool override)
{
    if (m_overrideSubcomponentsMass == override)
    {
        return;
    }
    m_overrideSubcomponentsMass = override;

    updateChildrenMassOverriddenBy();

    fireComponentChangeEvent(ComponentChangeEvent::kMassChange |
                             ComponentChangeEvent::kTreeChangeChildren);
}

bool RocketComponent::isSubcomponentsOverriddenCG() const
{
    return m_overrideSubcomponentsCG;
}

void RocketComponent::setSubcomponentsOverriddenCG(bool override)
{
    if (m_overrideSubcomponentsCG == override)
    {
        return;
    }
    m_overrideSubcomponentsCG = override;

    updateChildrenCGOverriddenBy();

    fireComponentChangeEvent(ComponentChangeEvent::kMassChange |
                             ComponentChangeEvent::kTreeChangeChildren);
}

bool RocketComponent::isSubcomponentsOverriddenCD() const
{
    return m_overrideSubcomponentsCD;
}

void RocketComponent::setSubcomponentsOverriddenCD(bool override)
{
    if (m_overrideSubcomponentsCD == override)
    {
        return;
    }
    m_overrideSubcomponentsCD = override;

    updateChildrenCDOverriddenBy();

    overrideSubcomponentsCD(override);

    fireComponentChangeEvent(ComponentChangeEvent::kAerodynamicChange |
                             ComponentChangeEvent::kTreeChangeChildren);
}

void RocketComponent::overrideSubcomponentsCD(bool override)
{
    // Setting: every descendant becomes overridden by an ancestor. Clearing: every descendant
    // stops being overridden, except below a descendant that overrides its own subcomponents,
    // from which the override continues. (In Java too this walk changes no field: whether a
    // component is overridden by an ancestor is computed from the ancestors' flags.)
    for (const auto& child : m_children)
    {
        if (child->isCDOverriddenByAncestor() != override)
        {
            if (!override && child->isCDOverridden() && child->isSubcomponentsOverriddenCD())
            {
                child->overrideSubcomponentsCD(true);
            }
            else
            {
                child->overrideSubcomponentsCD(override);
            }
        }
    }
}

bool RocketComponent::isOverrideSubcomponentsEnabled() const
{
    return isCGOverridden() || isMassOverridden() || isCDOverridden();
}

// OpenRocket's walk: once a descendant that overrides its subcomponents has been met, every
// later descendant in tree order gets it as the overrider, its siblings' subtrees included; kept
// as it is.
void RocketComponent::updateChildrenMassOverriddenBy()
{
    RocketComponent* overriddenBy =
        (m_massOverridden && m_overrideSubcomponentsMass) ? this : nullptr;
    for (RocketComponent* c : getAllChildren())
    {
        c->m_massOverriddenBy = overriddenBy;
        // In case one of the children has its subcomponents overridden.
        if (overriddenBy == nullptr)
        {
            overriddenBy = (c->m_massOverridden && c->m_overrideSubcomponentsMass) ? c : nullptr;
        }
    }
}

void RocketComponent::updateChildrenCGOverriddenBy()
{
    RocketComponent* overriddenBy = (m_cgOverridden && m_overrideSubcomponentsCG) ? this : nullptr;
    for (RocketComponent* c : getAllChildren())
    {
        c->m_cgOverriddenBy = overriddenBy;
        if (overriddenBy == nullptr)
        {
            overriddenBy = (c->m_cgOverridden && c->m_overrideSubcomponentsCG) ? c : nullptr;
        }
    }
}

void RocketComponent::updateChildrenCDOverriddenBy()
{
    RocketComponent* overriddenBy = (m_cdOverridden && m_overrideSubcomponentsCD) ? this : nullptr;
    for (RocketComponent* c : getAllChildren())
    {
        c->m_cdOverriddenBy = overriddenBy;
        if (overriddenBy == nullptr)
        {
            overriddenBy = (c->m_cdOverridden && c->m_overrideSubcomponentsCD) ? c : nullptr;
        }
    }
}

// ================================================================================ position

double RocketComponent::getLength() const
{
    return m_length;
}

void RocketComponent::setAxialMethod(AxialMethod newAxialMethod)
{
    if (newAxialMethod == m_axialMethod)
    {
        return;
    }
    // Changes how the position is described, not the position itself.
    m_axialMethod = newAxialMethod;
    m_axialOffset = getAxialOffset(newAxialMethod);
}

double RocketComponent::getAxialOffset(AxialMethod asMethod) const
{
    double parentLength = 0;
    if (m_parent != nullptr && dynamic_cast<const Rocket*>(m_parent) == nullptr)
    {
        parentLength = m_parent->getLength();
    }

    if (AxialMethod::ABSOLUTE == asMethod)
    {
        return getComponentLocations().front().x;
    }
    return QtRocket::getAsOffset(asMethod, m_position.x, getLength(), parentLength);
}

double RocketComponent::getAxialOffset() const
{
    return m_axialOffset;
}

void RocketComponent::setAxialOffset(double newOffset)
{
    setAxialOffset(m_axialMethod, newOffset);
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void RocketComponent::setAxialOffset(AxialMethod requestedMethod, double requestedOffset)
{
    double newX = std::numeric_limits<double>::quiet_NaN();

    if (nullptr == m_parent)
    {
        // A best-effort approximation, corrected later in the initialisation.
        newX = requestedOffset;
    }
    else if (AxialMethod::ABSOLUTE == requestedMethod)
    {
        newX = requestedOffset - m_parent->getComponentLocations().front().x;
    }
    else if (isAfter())
    {
        setAfter();
        return;
    }
    else
    {
        newX = QtRocket::getAsPosition(requestedMethod, requestedOffset, getLength(),
                                       m_parent->getLength());
    }

    // Snap to zero below this threshold.
    constexpr double kEpsilon = 0.000001;
    if (kEpsilon > std::abs(newX))
    {
        newX = 0.0;
    }
    else if (std::isnan(newX))
    {
        bug("setAxialOffset is broken -- attempted to update as NaN: " + toDebugDetail());
    }

    m_axialMethod = requestedMethod;
    m_axialOffset = requestedOffset;
    m_position    = m_position.setX(newX);
}

void RocketComponent::update()
{
    setAxialOffset(m_axialMethod, m_axialOffset);
}

void RocketComponent::updateBounds()
{
    // Nothing: subclasses that track adjacent placements override it.
}

double RocketComponent::getRadiusOffset() const
{
    return 0;
}

double RocketComponent::getRadiusOffset(RadiusMethod method) const
{
    const double radius = getRadius(getRadiusMethod(), m_parent, this, getRadiusOffset());
    return QtRocket::getAsOffset(method, m_parent, this, radius);
}

RadiusMethod RocketComponent::getRadiusMethod() const
{
    return RadiusMethod::COAXIAL;
}

double RocketComponent::getAngleOffset() const
{
    return 0;
}

void RocketComponent::setAfter()
{
    if (nullptr == m_parent)
    {
        // Probably an initialisation order issue; ignored.
        return;
    }

    m_axialMethod = AxialMethod::AFTER;
    m_axialOffset = 0.0;

    // The first component of its parent is placed at the top of the parent.
    const std::optional<std::size_t> thisIndex = m_parent->getChildPosition(this);
    if (!thisIndex)
    {
        return;  // Java: getChildPosition() == -1 falls through both branches
    }
    if (0 == *thisIndex)
    {
        m_position = m_position.setX(0.0);
        return;
    }

    // HOOK(rocket-config): the activeness test is getRocket().getSelectedConfiguration()
    // .isComponentActive() in Java; a tree that is not in a Rocket counts every component as
    // active (Java's getRocket() would throw).
    const Rocket* rocket   = findRocket();
    const auto    isActive = [rocket](const RocketComponent& component) {
        return rocket == nullptr || rocket->isComponentActiveInSelectedConfiguration(component);
    };

    std::size_t            idx                = *thisIndex - 1;
    const RocketComponent* referenceComponent = &m_parent->getChild(idx);
    while (!isActive(*referenceComponent) && idx > 0)
    {
        --idx;
        referenceComponent = &m_parent->getChild(idx);
    }

    // When no earlier component is active, this one becomes the new reference point.
    if (!isActive(*referenceComponent))
    {
        m_position = m_position.setX(0.0);
        return;
    }

    const double refLength = referenceComponent->getLength();
    const double refRelX   = referenceComponent->getPosition().x;

    m_position = m_position.setX(refRelX + refLength);
}

// =============================================================================== instances

std::vector<Coordinate> RocketComponent::getInstanceLocations() const
{
    const Coordinate              center  = m_position;
    const std::vector<Coordinate> offsets = getInstanceOffsets();

    std::vector<Coordinate> locations;
    locations.reserve(offsets.size());
    for (const Coordinate& offset : offsets)
    {
        locations.push_back(center.add(offset));
    }
    return locations;
}

std::vector<Coordinate> RocketComponent::getInstanceOffsets() const
{
    return {Coordinate::kZero};
}

std::vector<double> RocketComponent::getInstanceAngles() const
{
    return std::vector<double>(static_cast<std::size_t>(std::max(getInstanceCount(), 0)), 0.0);
}

std::vector<Coordinate> RocketComponent::getComponentLocations() const
{
    if (m_cachedComponentLocations)
    {
        return *m_cachedComponentLocations;
    }

    std::vector<Coordinate> computedLocations;
    if (m_parent == nullptr)
    {
        // An improperly initialised component, or the root Rocket.
        computedLocations = getInstanceOffsets();
    }
    else
    {
        const std::vector<Coordinate> parentPositions   = m_parent->getComponentLocations();
        const std::size_t             parentCount       = parentPositions.size();
        const std::vector<Coordinate> instanceLocations = getInstanceLocations();
        const std::size_t             instanceCount     = instanceLocations.size();
        // The parent rotations are applied too.
        const std::vector<Coordinate> parentRotations = m_parent->getComponentAngles();

        if (parentCount == 1 && instanceCount == 1)
        {
            // The usual case.
            const Transformation rotation =
                Transformation::rotation(parentRotations.front(), m_position);
            computedLocations = {
                parentPositions.front().add(rotation.transform(instanceLocations.front()))};
        }
        else
        {
            computedLocations.resize(instanceCount * parentCount);
            for (std::size_t pi = 0; pi < parentCount; ++pi)
            {
                const Transformation rotation =
                    Transformation::rotation(parentRotations.at(pi), m_position);
                for (std::size_t ii = 0; ii < instanceCount; ++ii)
                {
                    computedLocations.at(pi + (parentCount * ii)) =
                        parentPositions.at(pi).add(rotation.transform(instanceLocations.at(ii)));
                }
            }
        }
    }

    m_cachedComponentLocations = computedLocations;
    return computedLocations;
}

std::vector<Coordinate> RocketComponent::getComponentAngles() const
{
    if (m_cachedComponentAngles)
    {
        return *m_cachedComponentAngles;
    }

    std::vector<Coordinate> computedAngles;
    if (m_parent == nullptr)
    {
        // An improperly initialised component, or the root Rocket.
        computedAngles = axialRotToCoord(getInstanceAngles());
    }
    else
    {
        const std::vector<Coordinate> parentAngles   = m_parent->getComponentAngles();
        const std::size_t             parentCount    = parentAngles.size();
        const std::vector<Coordinate> instanceAngles = axialRotToCoord(getInstanceAngles());
        const std::size_t             instanceCount  = instanceAngles.size();

        if (parentCount == 1 && instanceCount == 1)
        {
            computedAngles = {parentAngles.front().add(instanceAngles.front())};
        }
        else
        {
            computedAngles.resize(instanceCount * parentCount);
            for (std::size_t pi = 0; pi < parentCount; ++pi)
            {
                for (std::size_t ii = 0; ii < instanceCount; ++ii)
                {
                    computedAngles.at(pi + (parentCount * ii)) =
                        parentAngles.at(pi).add(instanceAngles.at(ii));
                }
            }
        }
    }

    m_cachedComponentAngles = computedAngles;
    return computedAngles;
}

std::vector<Coordinate> RocketComponent::toAbsolute(const Coordinate& c) const
{
    std::vector<Coordinate> positions = getComponentLocations();
    for (Coordinate& position : positions)
    {
        position = position.add(c);
    }
    return positions;
}

std::vector<Coordinate> RocketComponent::toRelative(const Coordinate&      c,
                                                    const RocketComponent& dest) const
{
    const std::vector<Coordinate> destLocs  = dest.getComponentLocations();
    const Coordinate              sourceLoc = getComponentLocations().front();

    std::vector<Coordinate> result;
    result.reserve(destLocs.size());
    for (const Coordinate& destLoc : destLocs)
    {
        result.push_back(sourceLoc.add(c).sub(destLoc));
    }
    return result;
}

// ============================================================================ mass and CG

double RocketComponent::getMass() const
{
    if (m_massOverridden)
    {
        return m_overrideMass;
    }
    return getComponentMass();
}

double RocketComponent::getSectionMass() const
{
    double massSubtotal = getMass();
    if (m_massOverridden && m_overrideSubcomponentsMass)
    {
        return massSubtotal;
    }
    for (const auto& child : m_children)
    {
        massSubtotal += child->getSectionMass();
    }
    return massSubtotal;
}

Coordinate RocketComponent::getCG() const
{
    if (m_cgOverridden)
    {
        return getOverrideCG().setWeight(getMass());
    }
    if (m_massOverridden)
    {
        return getComponentCG().setWeight(getMass());
    }
    return getComponentCG();
}

double RocketComponent::getLongitudinalInertia() const
{
    return getLongitudinalUnitInertia() * getMass();
}

double RocketComponent::getRotationalInertia() const
{
    return getRotationalUnitInertia() * getMass();
}

// ==================================================================================== tree

void RocketComponent::insertChild(std::unique_ptr<RocketComponent> component, std::size_t index,
                                  StageTracking tracking)
{
    if (component == nullptr)
    {
        bug("addChild() of a null component");
    }
    if (component->m_parent != nullptr)
    {
        bug("component " + component->getComponentName() + " is already in a tree");
    }
    // No loops in the tree [A -> X -> Y -> B, B.addChild(A)].
    if (&getRoot() == component.get())
    {
        const std::string message = "Component " + component->getComponentName() +
                                    " is a parent of " + getComponentName() +
                                    ", attempting to create cycle in tree.";
        // The component owns this one: destroying it now would destroy this component while
        // its member function runs, so it is let go (the program is stopping anyway).
        [[maybe_unused]] const RocketComponent* const leaked = component.release();
        bug(message);
    }
    if (!isCompatible(*component))
    {
        bug("Component: " + component->getComponentName() +
            " not currently compatible with component: " + getComponentName());
    }
    QTROCKET_ASSERT(index <= m_children.size());

    RocketComponent& added = *component;
    m_children.insert(m_children.begin() + static_cast<std::ptrdiff_t>(index),
                      std::move(component));
    added.m_parent = this;

    added.m_massOverriddenBy =
        (m_massOverridden && m_overrideSubcomponentsMass) ? this : m_massOverriddenBy;
    added.m_cgOverriddenBy =
        (m_cgOverridden && m_overrideSubcomponentsCG) ? this : m_cgOverriddenBy;
    added.m_cdOverriddenBy =
        (m_cdOverridden && m_overrideSubcomponentsCD) ? this : m_cdOverriddenBy;
    // Change the descendants' overrider only when the added component got one, so that an
    // overrider inside the added subtree is not lost.
    added.forEach(
        [&added](RocketComponent& child) {
            if (added.m_massOverriddenBy != nullptr)
            {
                child.m_massOverriddenBy = added.m_massOverriddenBy;
            }
            if (added.m_cgOverriddenBy != nullptr)
            {
                child.m_cgOverriddenBy = added.m_cgOverriddenBy;
            }
            if (added.m_cdOverriddenBy != nullptr)
            {
                child.m_cdOverriddenBy = added.m_cdOverriddenBy;
            }
        },
        false);

    if (tracking == StageTracking::TRACK)
    {
        if (auto* stage = dynamic_cast<AxialStage*>(&added))
        {
            // Java's getRocket() throws outside a Rocket; the stage map is skipped instead.
            if (Rocket* rocket = findRocket())
            {
                rocket->trackStage(*stage);
            }
        }
    }

    checkComponentStructure();
    added.checkComponentStructure();

    fireAddRemoveEvent(added);
}

std::unique_ptr<RocketComponent> RocketComponent::removeChild(const RocketComponent* component,
                                                              StageTracking          tracking)
{
    const std::optional<std::size_t> index = getChildPosition(component);
    if (!index)
    {
        return nullptr;
    }
    component->checkComponentStructure();

    std::unique_ptr<RocketComponent> removed = std::move(m_children[*index]);
    m_children.erase(m_children.begin() + static_cast<std::ptrdiff_t>(*index));
    removed->m_parent = nullptr;

    // Clear an overrider only when it is this component or this component's overrider, so that
    // an overrider inside the removed subtree is kept.
    removed->forEach([this](RocketComponent& c) {
        if (c.m_massOverriddenBy == this || c.m_massOverriddenBy == m_massOverriddenBy)
        {
            c.m_massOverriddenBy = nullptr;
        }
        if (c.m_cgOverriddenBy == this || c.m_cgOverriddenBy == m_cgOverriddenBy)
        {
            c.m_cgOverriddenBy = nullptr;
        }
        if (c.m_cdOverriddenBy == this || c.m_cdOverriddenBy == m_cdOverriddenBy)
        {
            c.m_cdOverriddenBy = nullptr;
        }
    });

    if (tracking == StageTracking::TRACK)
    {
        if (Rocket* rocket = findRocket())
        {
            if (const auto* stage = dynamic_cast<const AxialStage*>(removed.get()))
            {
                rocket->forgetStage(*stage);
            }
            // The removed component's sub-stages too.
            for (const AxialStage* stage : removed->getSubStages())
            {
                rocket->forgetStage(*stage);
            }
        }
    }

    checkComponentStructure();
    removed->checkComponentStructure();

    fireAddRemoveEvent(*removed);
    updateBounds();

    return removed;
}

void RocketComponent::moveChild(const RocketComponent* component, std::size_t index)
{
    const std::optional<std::size_t> oldIndex = getChildPosition(component);
    if (!oldIndex)
    {
        return;
    }
    // Java removes the child before it finds the index out of range, losing the child; the
    // index is checked first here.
    if (index >= m_children.size())
    {
        bug("child index out of range");
    }
    std::unique_ptr<RocketComponent> moved = std::move(m_children[*oldIndex]);
    m_children.erase(m_children.begin() + static_cast<std::ptrdiff_t>(*oldIndex));
    const RocketComponent& movedRef = *moved;
    m_children.insert(m_children.begin() + static_cast<std::ptrdiff_t>(index), std::move(moved));

    checkComponentStructure();
    movedRef.checkComponentStructure();

    updateBounds();
    fireAddRemoveEvent(movedRef);
}

void RocketComponent::fireAddRemoveEvent(const RocketComponent& component)
{
    int type = ComponentChangeEvent::kTreeChange;
    component.forEach([&type](const RocketComponent& c) {
        if (c.isAerodynamic())
        {
            type |= ComponentChangeEvent::kAerodynamicChange;
        }
        if (c.isMassive())
        {
            type |= ComponentChangeEvent::kMassChange;
        }
    });
    fireComponentChangeEvent(type);
}

RocketComponent& RocketComponent::getChild(std::size_t n)
{
    if (n >= m_children.size())
    {
        bug("child index out of range");
    }
    return *m_children[n];
}

const RocketComponent& RocketComponent::getChild(std::size_t n) const
{
    if (n >= m_children.size())
    {
        bug("child index out of range");
    }
    return *m_children[n];
}

std::vector<RocketComponent*> RocketComponent::getChildren()
{
    std::vector<RocketComponent*> children;
    children.reserve(m_children.size());
    for (const auto& child : m_children)
    {
        children.push_back(child.get());
    }
    return children;
}

std::vector<const RocketComponent*> RocketComponent::getChildren() const
{
    std::vector<const RocketComponent*> children;
    children.reserve(m_children.size());
    for (const auto& child : m_children)
    {
        children.push_back(child.get());
    }
    return children;
}

std::vector<RocketComponent*> RocketComponent::getAllChildren()
{
    std::vector<RocketComponent*> all;
    forEach([&all](RocketComponent& c) { all.push_back(&c); }, false);
    return all;
}

std::vector<const RocketComponent*> RocketComponent::getAllChildren() const
{
    std::vector<const RocketComponent*> all;
    forEach([&all](const RocketComponent& c) { all.push_back(&c); }, false);
    return all;
}

bool RocketComponent::containsChild(const RocketComponent* component) const
{
    const std::vector<const RocketComponent*> all = getAllChildren();
    return std::ranges::find(all, component) != all.end();
}

std::optional<std::size_t> RocketComponent::getChildPosition(
    const RocketComponent* child) const noexcept
{
    for (std::size_t i = 0; i < m_children.size(); ++i)
    {
        if (m_children[i].get() == child)
        {
            return i;
        }
    }
    return std::nullopt;
}

std::vector<RocketComponent*> RocketComponent::getParents()
{
    std::vector<RocketComponent*> result;
    // NOLINTNEXTLINE(misc-const-correctness): the ancestors are handed out as non-const
    for (RocketComponent* current = m_parent; current != nullptr; current = current->m_parent)
    {
        result.push_back(current);
    }
    return result;
}

std::vector<const RocketComponent*> RocketComponent::getParents() const
{
    std::vector<const RocketComponent*> result;
    for (const RocketComponent* current = m_parent; current != nullptr; current = current->m_parent)
    {
        result.push_back(current);
    }
    return result;
}

bool RocketComponent::listContainsParent(std::span<const RocketComponent* const> components,
                                         const RocketComponent&                  component)
{
    for (const RocketComponent* c = component.m_parent; c != nullptr; c = c->m_parent)
    {
        if (std::ranges::find(components, c) != components.end())
        {
            return true;
        }
    }
    return false;
}

bool RocketComponent::checkAllClassesEqual(std::span<const RocketComponent* const> components) const
{
    return std::ranges::all_of(components, [this](const RocketComponent* c) {
        return c != nullptr && typeid(*c) == typeid(*this);
    });
}

bool RocketComponent::isAncestor(const RocketComponent& testComponent) const noexcept
{
    for (const RocketComponent* current = testComponent.m_parent; current != nullptr;
         current                        = current->m_parent)
    {
        if (this == current)
        {
            return true;
        }
    }
    return false;
}

RocketComponent& RocketComponent::getRoot() noexcept
{
    RocketComponent* root = this;
    while (root->m_parent != nullptr)
    {
        root = root->m_parent;
    }
    return *root;
}

const RocketComponent& RocketComponent::getRoot() const noexcept
{
    const RocketComponent* root = this;
    while (root->m_parent != nullptr)
    {
        root = root->m_parent;
    }
    return *root;
}

Rocket& RocketComponent::getRocket()
{
    Rocket* rocket = findRocket();
    if (rocket == nullptr)
    {
        bug("getRocket() called with root component " + getRoot().getComponentName());
    }
    return *rocket;
}

const Rocket& RocketComponent::getRocket() const
{
    const Rocket* rocket = findRocket();
    if (rocket == nullptr)
    {
        bug("getRocket() called with root component " + getRoot().getComponentName());
    }
    return *rocket;
}

Rocket* RocketComponent::findRocket() noexcept
{
    return dynamic_cast<Rocket*>(&getRoot());
}

const Rocket* RocketComponent::findRocket() const noexcept
{
    return dynamic_cast<const Rocket*>(&getRoot());
}

AxialStage& RocketComponent::getStage()
{
    AxialStage* stage = findStage();
    if (stage == nullptr)
    {
        bug("getStage() called on hierarchy without an AxialStage.");
    }
    return *stage;
}

const AxialStage& RocketComponent::getStage() const
{
    const AxialStage* stage = findStage();
    if (stage == nullptr)
    {
        bug("getStage() called on hierarchy without an AxialStage.");
    }
    return *stage;
}

AxialStage* RocketComponent::findStage() noexcept
{
    for (RocketComponent* current = this; current != nullptr; current = current->m_parent)
    {
        if (auto* stage = dynamic_cast<AxialStage*>(current))
        {
            return stage;
        }
    }
    return nullptr;
}

const AxialStage* RocketComponent::findStage() const noexcept
{
    for (const RocketComponent* current = this; current != nullptr; current = current->m_parent)
    {
        if (const auto* stage = dynamic_cast<const AxialStage*>(current))
        {
            return stage;
        }
    }
    return nullptr;
}

std::vector<AxialStage*> RocketComponent::getSubStages()
{
    std::vector<AxialStage*> result;
    forEach(
        [&result](RocketComponent& c) {
            if (auto* stage = dynamic_cast<AxialStage*>(&c))
            {
                result.push_back(stage);
            }
        },
        false);
    return result;
}

ComponentAssembly& RocketComponent::getAssembly()
{
    ComponentAssembly* assembly = findAssembly();
    if (assembly == nullptr)
    {
        bug("getAssembly() called on hierarchy without a ComponentAssembly.");
    }
    return *assembly;
}

const ComponentAssembly& RocketComponent::getAssembly() const
{
    const ComponentAssembly* assembly = findAssembly();
    if (assembly == nullptr)
    {
        bug("getAssembly() called on hierarchy without a ComponentAssembly.");
    }
    return *assembly;
}

ComponentAssembly* RocketComponent::findAssembly() noexcept
{
    for (RocketComponent* current = this; current != nullptr; current = current->m_parent)
    {
        if (auto* assembly = dynamic_cast<ComponentAssembly*>(current))
        {
            return assembly;
        }
    }
    return nullptr;
}

const ComponentAssembly* RocketComponent::findAssembly() const noexcept
{
    for (const RocketComponent* current = this; current != nullptr; current = current->m_parent)
    {
        if (const auto* assembly = dynamic_cast<const ComponentAssembly*>(current))
        {
            return assembly;
        }
    }
    return nullptr;
}

std::vector<ComponentAssembly*> RocketComponent::getAllChildAssemblies()
{
    std::vector<ComponentAssembly*> result;
    forEach(
        [&result](RocketComponent& c) {
            if (auto* assembly = dynamic_cast<ComponentAssembly*>(&c))
            {
                result.push_back(assembly);
            }
        },
        false);
    return result;
}

std::vector<ComponentAssembly*> RocketComponent::getDirectChildAssemblies()
{
    std::vector<ComponentAssembly*> result;
    for (const auto& child : m_children)
    {
        if (auto* assembly = dynamic_cast<ComponentAssembly*>(child.get()))
        {
            result.push_back(assembly);
        }
    }
    return result;
}

std::vector<AxialStage*> RocketComponent::getAllChildStages()
{
    return getSubStages();
}

std::vector<AxialStage*> RocketComponent::getTopLevelChildStages()
{
    std::vector<AxialStage*> result;
    const auto addTopLevelStages = [&result](const auto& self, RocketComponent& parent) -> void {
        for (const auto& child : parent.m_children)
        {
            if (auto* stage = dynamic_cast<AxialStage*>(child.get()))
            {
                result.push_back(stage);
            }
            else
            {
                self(self, *child);
            }
        }
    };
    addTopLevelStages(addTopLevelStages, *this);
    return result;
}

std::vector<RocketComponent*> RocketComponent::getParentAssemblies()
{
    std::vector<RocketComponent*> result;
    for (RocketComponent* current = m_parent; current != nullptr; current = current->m_parent)
    {
        if (dynamic_cast<ComponentAssembly*>(current) != nullptr)
        {
            result.push_back(current);
        }
    }
    return result;
}

int RocketComponent::getStageNumber() const
{
    // AxialStage overrides it.
    return getStage().getStageNumber();
}

RocketComponent* RocketComponent::findComponent(const Uuid& idToFind) noexcept
{
    if (m_id == idToFind)
    {
        return this;
    }
    for (const auto& child : m_children)
    {
        if (RocketComponent* found = child->findComponent(idToFind))
        {
            return found;
        }
    }
    return nullptr;
}

const RocketComponent* RocketComponent::findComponent(const Uuid& idToFind) const noexcept
{
    if (m_id == idToFind)
    {
        return this;
    }
    for (const auto& child : m_children)
    {
        if (const RocketComponent* found = std::as_const(*child).findComponent(idToFind))
        {
            return found;
        }
    }
    return nullptr;
}

RocketComponent* RocketComponent::getNextComponent() noexcept
{
    if (!m_children.empty())
    {
        return m_children.front().get();
    }

    const RocketComponent* current    = this;
    RocketComponent*       nextParent = m_parent;
    while (nextParent != nullptr)
    {
        const std::optional<std::size_t> pos = nextParent->getChildPosition(current);
        if (pos && *pos + 1 < nextParent->m_children.size())
        {
            return nextParent->m_children[*pos + 1].get();
        }
        current    = nextParent;
        nextParent = current->m_parent;
    }
    return nullptr;
}

RocketComponent* RocketComponent::getPreviousComponent()
{
    if (m_parent == nullptr)
    {
        return nullptr;
    }
    const std::optional<std::size_t> pos = m_parent->getChildPosition(this);
    if (!pos)
    {
        bug("Inconsistent internal state: " + toDebugName() +
            " is not among its parent's children");
    }
    if (*pos == 0)
    {
        return m_parent;
    }
    RocketComponent* c = m_parent->m_children[*pos - 1].get();
    while (!c->m_children.empty())
    {
        c = c->m_children.back().get();
    }
    return c;
}

RocketComponent::SplitResult RocketComponent::splitInstances(bool freezeRocket)
{
    Rocket&          rocket = getRocket();
    RocketComponent* parent = m_parent;
    if (parent == nullptr)
    {
        bug("splitInstances() of the rocket itself");
    }
    const std::optional<std::size_t> position = parent->getChildPosition(this);
    if (!position)
    {
        bug("splitInstances(): the component is not among its parent's children");
    }
    const std::size_t index       = *position;
    const int         count       = getInstanceCount();
    const double      angleOffset = getAngleOffset();

    SplitResult result;

    if (freezeRocket)
    {
        rocket.freeze();
    }
    try
    {
        if (count > 1)
        {
            // Take this component out of the tree; it is handed back to the caller.
            result.original = parent->removeChild(index, StageTracking::TRACK);
            for (int i = 0; i < count; i++)
            {
                std::unique_ptr<RocketComponent> copy = copyWithNewIds();
                copy->setInstanceCount(1);
                if (auto* anglePositionable = dynamic_cast<AnglePositionable*>(copy.get()))
                {
                    anglePositionable->setAngleOffset(
                        angleOffset + ((static_cast<double>(i * 2) * std::numbers::pi) / count));
                }
                copy->setName(copy->getName() + " #" + std::to_string(i + 1));
                copy->setOverrideMass(getOverrideMass() / count);
                result.components.push_back(&parent->addChild(
                    std::move(copy), index + static_cast<std::size_t>(i), StageTracking::TRACK));
            }
        }
        else
        {
            result.components.push_back(this);
        }
    }
    catch (...)
    {
        if (freezeRocket)
        {
            rocket.thaw();
        }
        throw;
    }
    if (freezeRocket)
    {
        rocket.thaw();
    }

    // As in Java, from this component: nothing reaches the rocket once it has been taken out.
    fireComponentChangeEvent(ComponentChangeEvent::kTreeChange);

    return result;
}

// =============================================================================== iteration

RocketComponent::BasicRange<RocketComponent> RocketComponent::subtree(bool includeSelf)
{
    return BasicRange<RocketComponent>{*this, includeSelf};
}

RocketComponent::BasicRange<const RocketComponent> RocketComponent::subtree(bool includeSelf) const
{
    return BasicRange<const RocketComponent>{*this, includeSelf};
}

template <class Component>
RocketComponent::BasicIterator<Component>::BasicIterator(Component& start, bool includeSelf)
{
    if (const Rocket* rocket = start.findRocket())
    {
        m_rocket    = rocket;
        m_treeModId = rocket->getTreeModId();
    }
    if (includeSelf)
    {
        m_current = &start;
    }
    else if (!start.m_children.empty())
    {
        m_stack.emplace_back(&start, std::size_t{1});
        m_current = start.m_children.front().get();
    }
}

template <class Component>
void RocketComponent::BasicIterator<Component>::checkTree() const
{
    if (m_rocket != nullptr && m_rocket->getTreeModId() != m_treeModId)
    {
        bug("Rocket modified while being iterated");
    }
}

template <class Component>
auto RocketComponent::BasicIterator<Component>::operator*() const -> reference
{
    checkTree();
    QTROCKET_ASSERT(m_current != nullptr);
    return *m_current;
}

template <class Component>
auto RocketComponent::BasicIterator<Component>::operator->() const -> pointer
{
    checkTree();
    QTROCKET_ASSERT(m_current != nullptr);
    return m_current;
}

template <class Component>
auto RocketComponent::BasicIterator<Component>::operator++() -> BasicIterator&
{
    checkTree();
    QTROCKET_ASSERT(m_current != nullptr);
    if (!m_current->m_children.empty())
    {
        // Descend into the current component's children.
        m_stack.emplace_back(m_current, std::size_t{1});
        m_current = m_current->m_children.front().get();
        return *this;
    }
    while (!m_stack.empty())
    {
        auto& [parent, next] = m_stack.back();
        if (next < parent->m_children.size())
        {
            m_current = parent->m_children[next].get();
            ++next;
            return *this;
        }
        m_stack.pop_back();
    }
    m_current = nullptr;
    return *this;
}

template <class Component>
auto RocketComponent::BasicIterator<Component>::operator++(int) -> BasicIterator
{
    BasicIterator previous = *this;
    ++*this;
    return previous;
}

template class RocketComponent::BasicIterator<RocketComponent>;
template class RocketComponent::BasicIterator<const RocketComponent>;

// ================================================================================= events

ComponentChangeSignal::Connection RocketComponent::addComponentChangeListener(
    ComponentChangeSignal::Slot slot)
{
    return getRocket().addComponentChangeListener(std::move(slot));
}

bool RocketComponent::removeComponentChangeListener(
    const ComponentChangeSignal::Connection& connection)
{
    ComponentChangeSignal::Connection handle = connection;
    return handle.disconnect();
}

ComponentChangeSignal::Connection RocketComponent::addChangeListener(
    ComponentChangeSignal::Slot slot)
{
    return addComponentChangeListener(std::move(slot));
}

bool RocketComponent::removeChangeListener(const ComponentChangeSignal::Connection& connection)
{
    return removeComponentChangeListener(connection);
}

void RocketComponent::fireComponentChangeEvent(int type)
{
    fireComponentChangeEvent(ComponentChangeEvent{this, type});
}

void RocketComponent::fireComponentChangeEvent(const ComponentChangeEvent& event)
{
    if (m_parent == nullptr || m_bypassComponentChangeEvent)
    {
        // A detached tree has no listeners.
        return;
    }
    getRoot().fireComponentChangeEvent(event);
}

// ================================================================================= copying

std::unique_ptr<RocketComponent> RocketComponent::copySubtree(
    std::vector<std::pair<const RocketComponent*, RocketComponent*>>& copies) const
{
    std::unique_ptr<RocketComponent> clone = cloneShallow();
    QTROCKET_ASSERT(clone != nullptr);
    const RocketComponent& cloneRef = *clone;
    // cloneShallow() must copy the concrete class.
    QTROCKET_ASSERT(typeid(cloneRef) == typeid(*this));
    QTROCKET_ASSERT(clone->m_parent == nullptr && clone->m_children.empty());
    clone->m_id = m_id;
    copies.emplace_back(this, clone.get());

    for (const auto& child : m_children)
    {
        std::unique_ptr<RocketComponent> childCopy = child->copySubtree(copies);
        childCopy->m_parent                        = clone.get();
        // Not addChild(): that would fire events.
        clone->m_children.push_back(std::move(childCopy));
    }
    return clone;
}

void RocketComponent::remapOverriddenBy(
    const std::vector<std::pair<const RocketComponent*, RocketComponent*>>& copies)
{
    const auto remap = [&copies](RocketComponent* original) -> RocketComponent* {
        if (original == nullptr)
        {
            return nullptr;
        }
        const auto it =
            std::ranges::find(copies, original, [](const auto& entry) { return entry.first; });
        return it != copies.end() ? it->second : nullptr;
    };
    forEach([&remap](RocketComponent& c) {
        c.m_massOverriddenBy = remap(c.m_massOverriddenBy);
        c.m_cgOverriddenBy   = remap(c.m_cgOverriddenBy);
        c.m_cdOverriddenBy   = remap(c.m_cdOverriddenBy);
    });
}

std::unique_ptr<RocketComponent> RocketComponent::copyWithOriginalId() const
{
    std::vector<std::pair<const RocketComponent*, RocketComponent*>> copies;
    std::unique_ptr<RocketComponent>                                 clone = copySubtree(copies);
    clone->remapOverriddenBy(copies);

    checkComponentStructure();
    clone->checkComponentStructure();
    return clone;
}

std::unique_ptr<RocketComponent> RocketComponent::copyWithNewIds() const
{
    std::unique_ptr<RocketComponent> clone = copyWithOriginalId();
    clone->forEach([](RocketComponent& c) { c.newId(); });
    return clone;
}

std::vector<std::unique_ptr<RocketComponent>> RocketComponent::copyFrom(
    const RocketComponent& source)
{
    if (m_parent != nullptr)
    {
        bug("copyFrom called for non-root component, parent=" + m_parent->toDebugString() +
            ", this=" + toDebugString());
    }

    // The previous children, kept alive by the caller until the event has been delivered.
    std::vector<std::unique_ptr<RocketComponent>> previous = std::move(m_children);
    m_children.clear();
    for (const auto& child : previous)
    {
        child->m_parent = nullptr;
    }

    // Copy the source's children, with the source itself standing for this component in the
    // overriddenBy pointers.
    std::vector<std::pair<const RocketComponent*, RocketComponent*>> copies;
    copies.emplace_back(&source, this);
    for (const auto& child : source.m_children)
    {
        std::unique_ptr<RocketComponent> copy = child->copySubtree(copies);
        copy->m_parent                        = this;
        m_children.push_back(std::move(copy));
    }
    for (const auto& child : m_children)
    {
        child->remapOverriddenBy(copies);
    }

    checkComponentStructure();
    source.checkComponentStructure();

    // The fields OpenRocket's copyFrom() copies (the name resolved, as Java copies the stored
    // default name of the source's class).
    m_length                     = source.m_length;
    m_axialMethod                = source.m_axialMethod;
    m_position                   = source.m_position;
    m_color                      = source.m_color;
    m_lineStyle                  = source.m_lineStyle;
    m_overrideMass               = source.m_overrideMass;
    m_massOverridden             = source.m_massOverridden;
    m_overrideCGX                = source.m_overrideCGX;
    m_cgOverridden               = source.m_cgOverridden;
    m_overrideSubcomponentsMass  = source.m_overrideSubcomponentsMass;
    m_overrideSubcomponentsCG    = source.m_overrideSubcomponentsCG;
    m_overrideSubcomponentsCD    = source.m_overrideSubcomponentsCD;
    m_name                       = source.getName();
    m_comment                    = source.m_comment;
    m_id                         = source.m_id;
    m_displayOrderSide           = source.m_displayOrderSide;
    m_displayOrderBack           = source.m_displayOrderBack;
    m_bypassComponentChangeEvent = false;
    clearCoordinateCaches();
    auto*       inside       = dynamic_cast<InsideColorComponent*>(this);
    const auto* sourceInside = dynamic_cast<const InsideColorComponent*>(&source);
    if (inside != nullptr && sourceInside != nullptr)
    {
        inside->setInsideColorComponentHandler(sourceInside->getInsideColorComponentHandler());
    }

    return previous;
}

// ================================================================================ identity

bool RocketComponent::equals(const RocketComponent& other) const noexcept
{
    return this == &other || (typeid(*this) == typeid(other) && m_id == other.m_id);
}

void RocketComponent::checkComponentStructure() const
{
    if (m_parent != nullptr && !m_parent->getChildPosition(this))
    {
        bug("Inconsistent component structure detected, parent does not contain this component "
            "as a child, parent=" +
            m_parent->toDebugString() + " this=" + toDebugString());
    }
    for (const auto& child : m_children)
    {
        if (child->m_parent != this)
        {
            bug("Inconsistent component structure detected, child does not have this component "
                "as the parent, this=" +
                toDebugString() + " child=" + child->toDebugString() + " child.parent=" +
                (child->m_parent == nullptr ? std::string{"null"}
                                            : child->m_parent->toDebugString()));
        }
    }
}

// =================================================================================== debug

std::string RocketComponent::toDebugString() const
{
    std::string text =
        std::format("{}@{}[\"{}\"", className(kind()), static_cast<const void*>(this), getName());
    for (const auto& child : m_children)
    {
        text += "; ";
        text += child->toDebugString();
    }
    text += ']';
    return text;
}

std::string RocketComponent::toDebugName() const
{
    return getName() + "<" + std::string{className(kind())} + ">(" + m_id.toString().substr(0, 8) +
           ")";
}

std::string RocketComponent::toDebugDetail(std::source_location where) const
{
    std::string buf;
    std::format_to(std::back_inserter(buf), " >> Dumping Detailed Information from: {}\n",
                   where.function_name());
    std::format_to(std::back_inserter(buf), "      At Component: {}, of class: {} \n", getName(),
                   className(kind()));
    std::format_to(std::back_inserter(buf), "      position: {:.6f}    at offset: {:.4f} via: {}\n",
                   m_position.x, m_axialOffset, axialMethodName(m_axialMethod));
    std::format_to(std::back_inserter(buf), "      length: {:.4f}\n", getLength());
    return buf;
}

std::string RocketComponent::toDebugTree() const
{
    std::string buffer;
    buffer +=
        "\n   ====== ====== ====== ====== ====== ====== ====== ====== ====== ====== ====== "
        "======\n";
    buffer +=
        "     [Name]                               [Length]          [Rel Pos]            "
        "    [Abs Pos]  \n";
    toDebugTreeHelper(buffer, "");
    buffer +=
        "\n   ====== ====== ====== ====== ====== ====== ====== ====== ====== ====== ====== "
        "======\n";
    return buffer;
}

void RocketComponent::toDebugTreeHelper(std::string& buffer, const std::string& indent) const
{
    toDebugTreeNode(buffer, indent);
    for (const auto& child : m_children)
    {
        child->toDebugTreeHelper(buffer, indent + "....");
    }
}

void RocketComponent::toDebugTreeNode(std::string& buffer, const std::string& indent) const
{
    const std::string prefix = std::format("{}{} (x{})", indent, getName(), getInstanceCount());

    if (1 == getInstanceCount())
    {
        // Un-instanced components (the usual case). Java prints the offset (a double) and the
        // location through %24s.
        std::format_to(std::back_inserter(buffer), "{:<40}|  {:5.3f}; {:>24}; {:>24}; ", prefix,
                       getLength(), Strings::javaDoubleToString(m_axialOffset),
                       getComponentLocations().front().toString());
        std::format_to(std::back_inserter(buffer), "(offset: {:4.1f}  via: {} )\n",
                       getAxialOffset(), axialMethodName(m_axialMethod));
    }
    else if (const auto* instanceable = dynamic_cast<const Instanceable*>(this))
    {
        // Instanced components: motor clusters, booster stage clusters.
        std::format_to(std::back_inserter(buffer), "{:<40} (cluster: {} )", prefix,
                       instanceable->getPatternName());
        std::format_to(std::back_inserter(buffer), "(offset: {:4.1f}  via: {} )\n",
                       getAxialOffset(), axialMethodName(m_axialMethod));

        const std::vector<Coordinate> locations = getComponentLocations();
        for (int instanceNumber = 0; instanceNumber < getInstanceCount(); instanceNumber++)
        {
            const std::string instancePrefix =
                std::format("{}    [{:2}/{:2}]", indent, instanceNumber + 1, getInstanceCount());
            const auto index = static_cast<std::size_t>(instanceNumber);
            std::format_to(std::back_inserter(buffer), "{:<40}|  {:5.3f}; {:>24}; {:>24};\n",
                           instancePrefix, getLength(), Strings::javaDoubleToString(m_axialOffset),
                           index < locations.size() ? locations[index].toString() : std::string{});
        }
    }
    else
    {
        bug("This is a developer error! If you implement an instanced class, please subclass the "
            "Instanceable interface.");
    }

    // Deferred to rocket-config: an active motor mount appends toDebugMountNode() here.
}

// ================================================================== helpers for subclasses

void RocketComponent::addBoundingBox(std::vector<Coordinate>& bounds, double xMin, double xMax,
                                     double r)
{
    bounds.emplace_back(xMin, -r, -r);
    bounds.emplace_back(xMax, r, r);
}

void RocketComponent::addBound(std::vector<Coordinate>& bounds, double x, double r)
{
    bounds.emplace_back(x, -r, -r);
    bounds.emplace_back(x, r, -r);
    bounds.emplace_back(x, r, r);
    bounds.emplace_back(x, -r, r);
}

Coordinate RocketComponent::ringCG(double outerRadius, double innerRadius, double x1, double x2,
                                   double density)
{
    return Coordinate{(x1 + x2) / 2, 0, 0, ringMass(outerRadius, innerRadius, x2 - x1, density)};
}

double RocketComponent::ringVolume(double outerRadius, double innerRadius, double length)
{
    return ringMass(outerRadius, innerRadius, length, 1.0);
}

double RocketComponent::ringMass(double outerRadius, double innerRadius, double length,
                                 double density)
{
    return std::numbers::pi *
           javaMaxZero(MathUtil::pow2(outerRadius) - MathUtil::pow2(innerRadius)) * length *
           density;
}

double RocketComponent::ringLongitudinalUnitInertia(double outerRadius, double innerRadius,
                                                    double length)
{
    // About the center of mass: 1/12 * (3 * (r1^2 + r2^2) + h^2)
    return ((3 * (MathUtil::pow2(innerRadius) + MathUtil::pow2(outerRadius))) +
            MathUtil::pow2(length)) /
           12;
}

double RocketComponent::ringRotationalUnitInertia(double outerRadius, double innerRadius)
{
    // 1/2 * (r1^2 + r2^2)
    return (MathUtil::pow2(innerRadius) + MathUtil::pow2(outerRadius)) / 2;
}

}  // namespace QtRocket
