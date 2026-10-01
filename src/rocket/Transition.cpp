#include "QtRocket/rocket/Transition.h"

#include <memory>
#include <vector>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

Transition::Transition()
{
    m_length = kDefaultRadius * 3;
    setDisplayOrderSide(2);  // Order for displaying the component in the 2D side view
    setDisplayOrderBack(2);  // Order for displaying the component in the 2D back view
}

std::unique_ptr<RocketComponent> Transition::cloneShallow() const
{
    return std::make_unique<Transition>(CopyKey{}, *this);
}

// ================================================================================= length

void Transition::setLength(double length)
{
    if (m_length == length)
    {
        return;
    }
    // Need to clearPreset when length changes.
    clearPreset();
    SymmetricComponent::setLength(length);
}

// ============================================================================ fore radius

double Transition::getForeRadius() const
{
    if (isForeRadiusAutomatic())
    {
        m_foreRadius = getAutoForeRadius();
    }
    return m_foreRadius;
}

double Transition::getAutoForeRadius() const
{
    const SymmetricComponent* c = getPreviousSymmetricComponent();
    if (c != nullptr)
    {
        return c->getFrontAutoRadius();
    }
    return kDefaultRadius;
}

void Transition::setForeRadius(double radius, bool doClamping)
{
    if ((m_foreRadius == radius) && !m_autoForeRadius)
    {
        return;
    }

    m_autoForeRadius = false;
    m_foreRadius     = MathUtil::javaMax(radius, 0);

    if (doClamping && m_thickness > m_foreRadius && m_thickness > m_aftRadius)
    {
        m_thickness = MathUtil::javaMax(m_foreRadius, m_aftRadius);
    }

    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);

    setForeShoulderRadius(getForeShoulderRadius(), doClamping);
}

void Transition::setForeRadius(double radius)
{
    setForeRadius(radius, true);
}

bool Transition::isForeRadiusAutomatic() const
{
    return m_autoForeRadius;
}

void Transition::setForeRadiusAutomatic(bool autoRadius, bool sanityCheck)
{
    // You can only set the auto fore radius if it is possible
    if (sanityCheck)
    {
        autoRadius = autoRadius && canUsePreviousCompAutomatic();
    }

    if (m_autoForeRadius == autoRadius)
    {
        return;
    }

    m_autoForeRadius = autoRadius;

    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void Transition::setForeRadiusAutomatic(bool autoRadius)
{
    setForeRadiusAutomatic(autoRadius, false);
}

double Transition::getAutoForeThickness() const
{
    const SymmetricComponent* c = getPreviousSymmetricComponent();
    if (c != nullptr)
    {
        return c->getThickness();
    }
    return getThickness();
}

// ============================================================================= aft radius

double Transition::getAftRadius() const
{
    if (isAftRadiusAutomatic())
    {
        m_aftRadius = getAutoAftRadius();
    }
    return m_aftRadius;
}

double Transition::getAutoAftRadius() const
{
    const SymmetricComponent* c = getNextSymmetricComponent();
    if (c != nullptr)
    {
        return c->getRearAutoRadius();
    }
    return kDefaultRadius;
}

void Transition::setAftRadius(double radius, bool doClamping)
{
    if ((m_aftRadius == radius) && !m_autoAftRadius)
    {
        return;
    }

    m_autoAftRadius = false;
    m_aftRadius     = MathUtil::javaMax(radius, 0);

    if (doClamping && m_thickness > m_foreRadius && m_thickness > m_aftRadius)
    {
        m_thickness = MathUtil::javaMax(m_foreRadius, m_aftRadius);
    }

    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);

    setAftShoulderRadius(getAftShoulderRadius());
}

void Transition::setAftRadius(double radius)
{
    setAftRadius(radius, true);
}

bool Transition::isAftRadiusAutomatic() const
{
    return m_autoAftRadius;
}

void Transition::setAftRadiusAutomatic(bool autoRadius, bool sanityCheck)
{
    // You can only set the auto aft radius if it is possible
    if (sanityCheck)
    {
        autoRadius = autoRadius && canUseNextCompAutomatic();
    }

    if (m_autoAftRadius == autoRadius)
    {
        return;
    }

    m_autoAftRadius = autoRadius;

    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void Transition::setAftRadiusAutomatic(bool autoRadius)
{
    setAftRadiusAutomatic(autoRadius, false);
}

double Transition::getAutoAftThickness() const
{
    const SymmetricComponent* c = getNextSymmetricComponent();
    if (c != nullptr)
    {
        return c->getThickness();
    }
    return getThickness();
}

// ======================================================================= radius automatics

double Transition::getFrontAutoRadius() const
{
    if (isAftRadiusAutomatic())
    {
        return -1;
    }
    return getAftRadius();
}

double Transition::getRearAutoRadius() const
{
    if (isForeRadiusAutomatic())
    {
        return -1;
    }
    return getForeRadius();
}

bool Transition::usesPreviousCompAutomatic() const
{
    return isForeRadiusAutomatic();
}

bool Transition::usesNextCompAutomatic() const
{
    return isAftRadiusAutomatic();
}

bool Transition::canUsePreviousCompAutomatic() const
{
    const SymmetricComponent* referenceComp = getPreviousSymmetricComponent();
    if (referenceComp == nullptr)
    {
        return false;
    }
    return !referenceComp->usesNextCompAutomatic();
}

bool Transition::canUseNextCompAutomatic() const
{
    const SymmetricComponent* referenceComp = getNextSymmetricComponent();
    if (referenceComp == nullptr)
    {
        return false;
    }
    return !referenceComp->usesPreviousCompAutomatic();
}

// ================================================================================== shape

void Transition::setShapeType(TransitionShape type)
{
    if (m_type == type)
    {
        return;
    }
    m_type           = type;
    m_clipped        = isClippable(type);
    m_shapeParameter = defaultParameter(type);

    // Need to clearPreset when shape type changes.
    clearPreset();

    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void Transition::setShapeParameter(double n)
{
    if (m_shapeParameter == n)
    {
        return;
    }
    m_shapeParameter = MathUtil::clamp(n, minParameter(m_type), maxParameter(m_type));

    // Need to clearPreset when shape parameter changes.
    clearPreset();

    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

bool Transition::isClipped() const
{
    if (!isClippable(m_type))
    {
        return false;
    }
    return m_clipped;
}

void Transition::setClipped(bool c)
{
    if (m_clipped == c)
    {
        return;
    }
    m_clipped = c;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

bool Transition::isClippedEnabled() const noexcept
{
    return isClippable(m_type);
}

double Transition::getShapeParameterMin() const noexcept
{
    return minParameter(m_type);
}

double Transition::getShapeParameterMax() const noexcept
{
    return maxParameter(m_type);
}

// ============================================================================== shoulders

void Transition::setForeShoulderRadius(double foreShoulderRadius, bool doClamping)
{
    if (doClamping)
    {
        foreShoulderRadius = MathUtil::javaMin(foreShoulderRadius, getForeRadius());
    }

    if (MathUtil::equals(m_foreShoulderRadius, foreShoulderRadius))
    {
        return;
    }

    m_foreShoulderRadius = foreShoulderRadius;

    if (doClamping)
    {
        m_foreShoulderThickness = MathUtil::javaMin(m_foreShoulderRadius, m_foreShoulderThickness);
    }

    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void Transition::setForeShoulderRadius(double foreShoulderRadius)
{
    setForeShoulderRadius(foreShoulderRadius, true);
}

void Transition::setForeShoulderThickness(double foreShoulderThickness)
{
    if (MathUtil::equals(m_foreShoulderThickness, foreShoulderThickness))
    {
        return;
    }
    m_foreShoulderThickness = foreShoulderThickness;
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void Transition::setForeShoulderLength(double foreShoulderLength)
{
    if (MathUtil::equals(m_foreShoulderLength, foreShoulderLength))
    {
        return;
    }

    if (MathUtil::equals(foreShoulderLength, 0))
    {
        m_foreShoulderLength = 0;
        setForeShoulderRadius(0);
        setForeShoulderThickness(0);
        setForeShoulderCapped(false);
    }
    else if (MathUtil::equals(getForeShoulderLength(), 0))
    {
        const double wallThickness = getAutoForeThickness();
        const double radius        = getAutoForeRadius();
        m_foreShoulderLength       = foreShoulderLength;

        // Only update radius and thickness if those variables were zero beforehand
        if (MathUtil::equals(getForeShoulderRadius(), 0))
        {
            setForeShoulderRadius(radius - wallThickness);
        }
        if (MathUtil::equals(getForeShoulderThickness(), 0))
        {
            setForeShoulderThickness(wallThickness);
        }

        setForeShoulderCapped(true);
    }
    else
    {
        m_foreShoulderLength = foreShoulderLength;
    }
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void Transition::setForeShoulderCapped(bool capped)
{
    if (m_foreShoulderCapped == capped)
    {
        return;
    }
    m_foreShoulderCapped = capped;
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void Transition::setAftShoulderRadius(double aftShoulderRadius, bool doClamping)
{
    if (doClamping)
    {
        aftShoulderRadius = MathUtil::javaMin(aftShoulderRadius, getAftRadius());
    }

    if (MathUtil::equals(m_aftShoulderRadius, aftShoulderRadius))
    {
        return;
    }

    m_aftShoulderRadius = aftShoulderRadius;

    if (doClamping)
    {
        m_aftShoulderThickness = MathUtil::javaMin(m_aftShoulderRadius, m_aftShoulderThickness);
    }

    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void Transition::setAftShoulderRadius(double aftShoulderRadius)
{
    setAftShoulderRadius(aftShoulderRadius, true);
}

void Transition::setAftShoulderThickness(double aftShoulderThickness)
{
    if (MathUtil::equals(m_aftShoulderThickness, aftShoulderThickness))
    {
        return;
    }
    m_aftShoulderThickness = aftShoulderThickness;
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void Transition::setAftShoulderLength(double aftShoulderLength)
{
    if (MathUtil::equals(m_aftShoulderLength, aftShoulderLength))
    {
        return;
    }

    if (MathUtil::equals(aftShoulderLength, 0))
    {
        m_aftShoulderLength = 0;
        setAftShoulderRadius(0);
        setAftShoulderThickness(0);
        setAftShoulderCapped(false);
    }
    else if (MathUtil::equals(getAftShoulderLength(), 0))
    {
        const double wallThickness = getAutoAftThickness();
        const double radius        = getAutoAftRadius();
        m_aftShoulderLength        = aftShoulderLength;

        // Only update radius and thickness if those variables were zero beforehand
        if (MathUtil::equals(getAftShoulderRadius(), 0))
        {
            setAftShoulderRadius(radius - wallThickness);
        }
        if (MathUtil::equals(getAftShoulderThickness(), 0))
        {
            setAftShoulderThickness(wallThickness);
        }

        setAftShoulderCapped(true);
    }
    else
    {
        m_aftShoulderLength = aftShoulderLength;
    }
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void Transition::setAftShoulderCapped(bool capped)
{
    if (m_aftShoulderCapped == capped)
    {
        return;
    }
    m_aftShoulderCapped = capped;
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

// ============================================================================ the profile

double Transition::getRadius(double x) const
{
    // Java reads only the radius it returns for a point outside the transition (reading an
    // automatic radius refreshes its stored value).
    if (x < 0)
    {
        return getForeRadius();
    }
    if (x >= m_length)
    {
        return getAftRadius();
    }
    // Fore before aft, as Java evaluates them (both may refresh automatic radii; the order of
    // function arguments is unspecified in C++).
    const double r1 = getForeRadius();
    const double r2 = getAftRadius();
    return getTransitionRadius(m_type, x, r1, r2, m_length, m_shapeParameter, isClipped(),
                               m_clipLength);
}

double Transition::getInnerRadius(double x) const
{
    return MathUtil::javaMax(getRadius(x) - m_thickness, 0);
}

std::vector<Coordinate> Transition::getComponentBounds() const
{
    std::vector<Coordinate> bounds = SymmetricComponent::getComponentBounds();
    if (m_foreShoulderLength > kMinFeature)
    {
        addBound(bounds, -m_foreShoulderLength, m_foreShoulderRadius);
    }
    if (m_aftShoulderLength > kMinFeature)
    {
        addBound(bounds, getLength() + m_aftShoulderLength, m_aftShoulderRadius);
    }
    return bounds;
}

void Transition::calculateProperties() const
{
    SymmetricComponent::calculateProperties();

    // only adjust properties if there is in fact at least one shoulder
    const bool hasShoulder =
        (getForeShoulderLength() > kMinFeature) || (getAftShoulderLength() > kMinFeature);
    if (!hasShoulder)
    {
        return;
    }

    // we'll work with volumes and not masses because density is uniform and it'll save some
    // multiplications and divisions

    // accumulate data for later calculation of properties with shoulders added
    const double     transVolume  = m_volume;
    double           transLongMOI = m_longitudinalUnitInertia * transVolume;
    const double     transRotMOI  = m_rotationalUnitInertia * transVolume;
    const Coordinate transCG      = m_cg.value_or(Coordinate{});  // set by the integration
    const double     density      = getMaterial().getDensity();

    double     foreCapVolume  = 0.0;
    Coordinate foreCapCG      = Coordinate::kZero;
    double     foreCapLongMOI = 0.0;
    double     foreCapRotMOI  = 0.0;
    if (isForeShoulderCapped())
    {
        const double ir =
            MathUtil::javaMax(getForeShoulderRadius() - getForeShoulderThickness(), 0);

        foreCapCG = ringCG(ir, 0, -getForeShoulderLength(),
                           getForeShoulderThickness() - getForeShoulderLength(), density);

        foreCapVolume = ringVolume(ir, 0, getForeShoulderThickness());

        foreCapLongMOI =
            ringLongitudinalUnitInertia(ir, 0, getForeShoulderThickness()) * foreCapVolume;

        foreCapRotMOI += ringRotationalUnitInertia(ir, 0.0) * foreCapVolume;
    }

    double     foreShoulderVolume  = 0.0;
    Coordinate foreShoulderCG      = Coordinate::kZero;
    double     foreShoulderLongMOI = 0.0;
    double     foreShoulderRotMOI  = 0.0;
    if (getForeShoulderLength() > kMinFeature)
    {
        const double outer = getForeShoulderRadius();
        const double ir =
            MathUtil::javaMax(getForeShoulderRadius() - getForeShoulderThickness(), 0);

        foreShoulderCG = ringCG(getForeShoulderRadius(), ir, -getForeShoulderLength(), 0, density);

        foreShoulderVolume = ringVolume(outer, ir, getForeShoulderLength());

        foreShoulderLongMOI =
            ringLongitudinalUnitInertia(outer, ir, getForeShoulderLength()) * foreShoulderVolume;

        foreShoulderRotMOI = ringRotationalUnitInertia(outer, ir) * foreShoulderVolume;
    }

    double     aftShoulderVolume  = 0.0;
    Coordinate aftShoulderCG      = Coordinate::kZero;
    double     aftShoulderLongMOI = 0.0;
    double     aftShoulderRotMOI  = 0.0;
    if (getAftShoulderLength() > kMinFeature)
    {
        const double outer = getAftShoulderRadius();
        const double ir = MathUtil::javaMax(getAftShoulderRadius() - getAftShoulderThickness(), 0);

        aftShoulderCG = ringCG(getAftShoulderRadius(), ir, getLength(),
                               getLength() + getAftShoulderLength(), density);

        aftShoulderVolume = ringVolume(outer, ir, getAftShoulderLength());

        aftShoulderLongMOI =
            ringLongitudinalUnitInertia(outer, ir, getAftShoulderLength()) * aftShoulderVolume;

        aftShoulderRotMOI = ringRotationalUnitInertia(outer, ir) * aftShoulderVolume;
    }

    double     aftCapVolume  = 0.0;
    Coordinate aftCapCG      = Coordinate::kZero;
    double     aftCapLongMOI = 0.0;
    double     aftCapRotMOI  = 0.0;
    if (isAftShoulderCapped())
    {
        const double ir = MathUtil::javaMax(getAftShoulderRadius() - getAftShoulderThickness(), 0);

        aftCapCG = ringCG(ir, 0, getLength() + getAftShoulderLength() - getAftShoulderThickness(),
                          getLength() + getAftShoulderLength(), density);

        aftCapVolume = ringVolume(ir, 0, getAftShoulderThickness());

        // OpenRocket takes the fore shoulder's thickness for the aft cap's length here.
        aftCapLongMOI =
            ringLongitudinalUnitInertia(ir, 0, getForeShoulderThickness()) * aftCapVolume;

        aftCapRotMOI = ringRotationalUnitInertia(ir, 0.0) * aftCapVolume;
    }

    // Combine results
    m_volume = foreCapVolume + foreShoulderVolume + transVolume + aftShoulderVolume + aftCapVolume;

    const double cgx = (foreCapCG.x * foreCapCG.weight) +
                       (foreShoulderCG.x * foreShoulderCG.weight) + (transCG.x * transCG.weight) +
                       (aftShoulderCG.x * aftShoulderCG.weight) + (aftCapCG.x * aftCapCG.weight);

    const double mass = foreCapCG.weight + foreShoulderCG.weight + transCG.weight +
                        aftShoulderCG.weight + aftCapCG.weight;

    // If the mass is 0, so are moments of inertia
    if (mass < MathUtil::kEpsilon)
    {
        m_cg                      = Coordinate{0, 0, 0, 0};
        m_longitudinalUnitInertia = 0.0;
        m_rotationalUnitInertia   = 0.0;

        return;
    }

    const Coordinate cg{cgx / mass, 0, 0, mass};
    m_cg = cg;

    // need to use parallel axis theorem to move longitudinal MOI to CG of component
    foreCapLongMOI += MathUtil::pow2(cg.x - foreCapCG.x) * foreCapVolume;
    foreShoulderLongMOI += MathUtil::pow2(cg.x - foreShoulderCG.x) * foreShoulderVolume;
    transLongMOI += MathUtil::pow2(cg.x - transCG.x) * transVolume;
    aftShoulderLongMOI += MathUtil::pow2(cg.x - aftShoulderCG.x) * aftShoulderVolume;
    aftCapLongMOI += MathUtil::pow2(cg.x - aftCapCG.x) * aftCapVolume;

    const double longMOI =
        foreCapLongMOI + foreShoulderLongMOI + transLongMOI + aftShoulderLongMOI + aftCapLongMOI;
    m_longitudinalUnitInertia = longMOI / m_volume;

    const double rotMOI =
        foreCapRotMOI + foreShoulderRotMOI + transRotMOI + aftShoulderRotMOI + aftCapRotMOI;
    m_rotationalUnitInertia = rotMOI / m_volume;
}

void Transition::componentChanged(const ComponentChangeEvent& event)
{
    SymmetricComponent::componentChanged(event);
    m_clipLength.reset();
}

bool Transition::isCompatible(ComponentKind kind) const
{
    if (isInternal(kind))
    {
        return true;
    }
    return kind == ComponentKind::FREEFORM_FIN_SET;
}

void Transition::loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options)
{
    SymmetricComponent::loadFromPreset(preset, options);

    bool presetFilled = false;
    if (preset.has(ComponentPreset::kFilled))
    {
        presetFilled = preset.get(ComponentPreset::kFilled);
    }

    if (preset.has(ComponentPreset::kThickness))
    {
        m_foreShoulderThickness = m_thickness;
        m_aftShoulderThickness  = m_thickness;
    }

    if (preset.has(ComponentPreset::kShape))
    {
        const TransitionShape s = preset.get(ComponentPreset::kShape);
        setShapeType(s);
        setClipped(isClippable(s));
        setShapeParameter(defaultParameter(s));
    }
    if (preset.has(ComponentPreset::kAftOuterDiameter))
    {
        const double outerDiameter = preset.get(ComponentPreset::kAftOuterDiameter);
        setAftRadiusAutomatic(false);
        setAftRadius(outerDiameter / 2.0);
    }
    if (preset.has(ComponentPreset::kAftShoulderLength))
    {
        setAftShoulderLength(preset.get(ComponentPreset::kAftShoulderLength));
    }
    if (preset.has(ComponentPreset::kAftShoulderDiameter))
    {
        const double d = preset.get(ComponentPreset::kAftShoulderDiameter);
        setAftShoulderRadius(d / 2.0);
        if (presetFilled)
        {
            setAftShoulderThickness(d / 2.0);
        }
    }
    if (preset.has(ComponentPreset::kForeOuterDiameter))
    {
        const double outerDiameter = preset.get(ComponentPreset::kForeOuterDiameter);
        setForeRadiusAutomatic(false);
        setForeRadius(outerDiameter / 2.0);
    }
    if (preset.has(ComponentPreset::kForeShoulderLength))
    {
        setForeShoulderLength(preset.get(ComponentPreset::kForeShoulderLength));
    }
    if (preset.has(ComponentPreset::kForeShoulderDiameter))
    {
        const double d = preset.get(ComponentPreset::kForeShoulderDiameter);
        setForeShoulderRadius(d / 2.0);
        if (presetFilled)
        {
            setForeShoulderThickness(d / 2.0);
        }
    }

    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

}  // namespace QtRocket
