#include "QtRocket/rocket/RailButton.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The name Java's constructor looks the default material up by.
constexpr std::string_view kDefaultMaterialName = "Delrin";

/// Java's `2.0f / 3`: a float division, widened to double where it is used (so not 2.0 / 3).
constexpr double kTwoThirdsFloat = static_cast<double>(2.0F / 3.0F);
static_assert(kTwoThirdsFloat == 0.6666666865348816);

/// The built-in bulk material named kDefaultMaterialName.
[[nodiscard]] Material builtinDelrin()
{
    for (const BuiltinMaterial& row : builtinMaterials())
    {
        if (row.type == Material::Type::BULK && row.name == kDefaultMaterialName)
        {
            return toMaterial(row);
        }
    }
    bug("no built-in bulk material \"" + std::string(kDefaultMaterialName) + "\"");
}

/// (d / 2)^2. Java writes Math.pow(d / 2, 2), which HotSpot evaluates as the product x * x
/// (the same value MathUtil.pow2() gives), so this is exact on every platform where std::pow
/// may differ in the last bit.
[[nodiscard]] constexpr double radiusSquared(double diameter) noexcept
{
    return MathUtil::pow2(diameter / 2);
}

}  // namespace

const Material& RailButton::defaultRailButtonMaterial()
{
    static const Material kDelrin = builtinDelrin();
    return kDelrin;
}

RailButton::RailButton() : ExternalComponent(AxialMethod::MIDDLE)
{
    // The dimensions and the instance separation are the member initialisers: Java's
    // setBaseHeight(0.002) and setInstanceSeparation(6 outer diameters) store just those values,
    // and their events, like setMaterial()'s, have nobody to go to from a constructor.
    m_material = defaultRailButtonMaterial();
    setDisplayOrderSide(14);  // Order for displaying the component in the 2D side view
    setDisplayOrderBack(11);  // Order for displaying the component in the 2D back view
}

RailButton::RailButton(double od, double ht) : RailButton()
{
    setOuterDiameter(od);
    setTotalHeight(ht);
}

RailButton::RailButton(double od, double id, double ht, double flangeHeight, double baseHeight)
  : ExternalComponent(AxialMethod::MIDDLE),
    m_outerDiameter(od),
    m_innerDiameter(id),
    m_totalHeight(ht),
    m_flangeHeight(flangeHeight),
    // Java's setBaseHeight(): 0 ... getMaxBaseHeight().
    m_baseHeight(MathUtil::javaMin(MathUtil::javaMax(baseHeight, 0), ht - flangeHeight)),
    // Java's setInstanceSeparation(): a separation that equals the initial 0
    // (MathUtil::equals) leaves it.
    m_instanceSeparation(MathUtil::equals(0.0, od * 2) ? 0.0 : od * 2)
{
    m_material = defaultRailButtonMaterial();
    setDisplayOrderSide(14);  // Order for displaying the component in the 2D side view
    setDisplayOrderBack(11);  // Order for displaying the component in the 2D back view
}

std::unique_ptr<RocketComponent> RailButton::cloneShallow() const
{
    return std::make_unique<RailButton>(*this);
}

void RailButton::applyDefaultMaterial(const Preferences&     preferences,
                                      const MaterialStorage& storage)
{
    ExternalComponent::applyDefaultMaterial(preferences, storage);
    const std::optional<Material> delrin =
        storage.findMaterial(Material::Type::BULK, kDefaultMaterialName);
    // Java: a NullPointerException without one, which its databases always hold.
    m_material = delrin.has_value() ? *delrin : defaultRailButtonMaterial();
}

// ============================================================================= dimensions

void RailButton::setBaseHeight(double newBaseHeight)
{
    m_baseHeight = MathUtil::javaMax(newBaseHeight, 0);
    m_baseHeight = MathUtil::javaMin(m_baseHeight, getMaxBaseHeight());
    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void RailButton::setFlangeHeight(double newFlangeHeight)
{
    m_flangeHeight = MathUtil::javaMax(newFlangeHeight, 0);
    m_flangeHeight = MathUtil::javaMin(m_flangeHeight, getMaxFlangeHeight());
    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void RailButton::setTotalHeight(double newHeight)
{
    m_totalHeight = MathUtil::javaMax(newHeight, getMinTotalHeight());

    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void RailButton::setScrewHeight(double height)
{
    m_screwHeight = MathUtil::javaMax(height, 0);
    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void RailButton::setInnerDiameter(double newId)
{
    m_innerDiameter = MathUtil::javaMin(newId, m_outerDiameter);
    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void RailButton::setOuterDiameter(double newOd)
{
    m_outerDiameter = newOd;
    setInnerDiameter(m_innerDiameter);

    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

// =============================================================================== position

double RailButton::getAngleOffset() const
{
    return m_angleOffsetRad;
}

AngleMethod RailButton::getAngleMethod() const
{
    return AngleMethod::RELATIVE;
}

void RailButton::setAngleMethod(AngleMethod /*newMethod*/)
{
    // Does nothing, as in Java.
}

void RailButton::setAngleOffset(double angle)
{
    const double clampedRad = MathUtil::clamp(angle, -std::numbers::pi, std::numbers::pi);

    if (MathUtil::equals(m_angleOffsetRad, clampedRad))
    {
        return;
    }
    m_angleOffsetRad = clampedRad;
    fireComponentChangeEvent(ComponentChangeEvent::kAerodynamicChange);
}

void RailButton::setAxialMethod(AxialMethod newMethod)
{
    RocketComponent::setAxialMethod(newMethod);
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

double RailButton::getAxialOffset() const
{
    return RocketComponent::getAxialOffset();
}

void RailButton::setAxialOffset(double newOffset)
{
    RocketComponent::setAxialOffset(newOffset);
}

// ============================================================================== instances

BoundingBox RailButton::getInstanceBoundingBox() const
{
    BoundingBox instanceBounds;

    instanceBounds.update(Coordinate{0, m_totalHeight + m_screwHeight, 0});
    instanceBounds.update(Coordinate{0, -m_totalHeight - m_screwHeight, 0});

    const double r = getOuterDiameter() / 2;
    instanceBounds.update(Coordinate{r, 0, r});
    instanceBounds.update(Coordinate{-r, 0, -r});

    return instanceBounds;
}

std::vector<Coordinate> RailButton::getInstanceLocations() const
{
    return RocketComponent::getInstanceLocations();
}

std::vector<Coordinate> RailButton::getInstanceOffsets() const
{
    const double yOffset = std::cos(m_angleOffsetRad) * m_radialDistance;
    const double zOffset = std::sin(m_angleOffsetRad) * m_radialDistance;

    std::vector<Coordinate> toReturn;
    toReturn.reserve(static_cast<std::size_t>(std::max(getInstanceCount(), 0)));
    for (int index = 0; index < getInstanceCount(); index++)
    {
        toReturn.emplace_back(index * m_instanceSeparation, yOffset, zOffset);
    }

    return toReturn;
}

void RailButton::componentChanged(const ComponentChangeEvent& event)
{
    ExternalComponent::componentChanged(event);

    double parentRadius = 0;
    for (const RocketComponent* body = getParent(); body != nullptr; body = body->getParent())
    {
        if (const auto* tube = dynamic_cast<const BodyTube*>(body))
        {
            parentRadius = tube->getOuterRadius();
            break;
        }
    }

    m_radialDistance = parentRadius;
    clearCoordinateCaches();
}

double RailButton::getInstanceSeparation() const
{
    return m_instanceSeparation;
}

void RailButton::setInstanceSeparation(double separation)
{
    if (MathUtil::equals(m_instanceSeparation, separation))
    {
        return;
    }
    m_instanceSeparation = separation;
    fireComponentChangeEvent(ComponentChangeEvent::kAerodynamicChange);
}

void RailButton::setInstanceCount(int newCount)
{
    if (newCount == m_instanceCount || newCount <= 0)
    {
        return;
    }
    m_instanceCount = newCount;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

int RailButton::getInstanceCount() const
{
    return m_instanceCount;
}

std::string RailButton::getPatternName() const
{
    return std::to_string(getInstanceCount()) + "-Line";
}

// ========================================================================== mass and bounds

double RailButton::getComponentVolume() const
{
    const double volOuter    = std::numbers::pi * radiusSquared(m_outerDiameter) * m_flangeHeight;
    const double volInner    = std::numbers::pi * radiusSquared(m_innerDiameter) * getInnerHeight();
    const double volStandoff = std::numbers::pi * radiusSquared(m_outerDiameter) * m_baseHeight;
    const double volScrew =
        kTwoThirdsFloat * std::numbers::pi * radiusSquared(m_outerDiameter) * m_screwHeight;
    const double volInstance = volOuter + volInner + volStandoff + volScrew;
    return volInstance * getInstanceCount();
}

std::vector<Coordinate> RailButton::getComponentBounds() const
{
    const double            r = m_outerDiameter / 2.0;
    std::vector<Coordinate> set;
    set.reserve(8);
    set.emplace_back(r, m_totalHeight, r);
    set.emplace_back(r, m_totalHeight, -r);
    set.emplace_back(r, 0, r);
    set.emplace_back(r, 0, -r);
    set.emplace_back(-r, 0, r);
    set.emplace_back(-r, 0, -r);
    set.emplace_back(-r, m_totalHeight, r);
    set.emplace_back(-r, m_totalHeight, -r);
    return set;
}

Coordinate RailButton::getComponentCG() const
{
    // Math.PI and density are assumed constant through calculation, and thus may be factored
    // out.
    const double massBase   = radiusSquared(m_outerDiameter) * m_baseHeight;
    const double massInner  = radiusSquared(m_innerDiameter) * getInnerHeight();
    const double massFlange = radiusSquared(m_outerDiameter) * m_flangeHeight;
    const double massScrew  = kTwoThirdsFloat * radiusSquared(m_outerDiameter) * m_screwHeight;
    const double totalMass  = massFlange + massInner + massBase + massScrew;
    const double baseCM     = m_baseHeight / 2;
    const double innerCM    = m_baseHeight + (getInnerHeight() / 2);
    const double flangeCM   = m_totalHeight - (getFlangeHeight() / 2);
    const double screwCM    = m_totalHeight + ((4 * m_screwHeight) / (3 * std::numbers::pi));
    const double heightCM = ((massBase * baseCM) + (massInner * innerCM) + (massFlange * flangeCM) +
                             (massScrew * screwCM)) /
                            totalMass;
    const auto*  symmetricParent = dynamic_cast<const SymmetricComponent*>(getParent());
    const double parentRadius =
        symmetricParent != nullptr ? symmetricParent->getRadius(getAxialOffset()) : 0;

    if (heightCM > m_totalHeight + m_screwHeight)
    {
        bug(" bug found while computing the CG of a RailButton: " + getName() +
            "\n height of CG: " + Strings::javaDoubleToString(heightCM));
    }

    const double cmX = (m_instanceSeparation * (m_instanceCount - 1)) / 2;
    const double cmY = std::cos(m_angleOffsetRad) * (parentRadius + heightCM);
    const double cmZ = std::sin(m_angleOffsetRad) * (parentRadius + heightCM);

    return Coordinate{cmX, cmY, cmZ, getComponentMass()};
}

double RailButton::getLongitudinalUnitInertia() const
{
    return 0.0;
}

double RailButton::getRotationalUnitInertia() const
{
    return 0.0;
}

bool RailButton::allowsChildren() const
{
    return false;
}

bool RailButton::isCompatible(ComponentKind /*kind*/) const
{
    // Allow nothing to be attached to a rail button
    return false;
}

// ================================================================================= preset

void RailButton::loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options)
{
    ExternalComponent::loadFromPreset(preset, options);

    if (preset.has(ComponentPreset::kOuterDiameter))
    {
        m_outerDiameter = preset.get(ComponentPreset::kOuterDiameter);
    }
    if (preset.has(ComponentPreset::kInnerDiameter))
    {
        m_innerDiameter = preset.get(ComponentPreset::kInnerDiameter);
    }
    if (preset.has(ComponentPreset::kHeight))
    {
        m_totalHeight = preset.get(ComponentPreset::kHeight);
    }
    if (preset.has(ComponentPreset::kFlangeHeight))
    {
        m_flangeHeight = preset.get(ComponentPreset::kFlangeHeight);
    }
    if (preset.has(ComponentPreset::kBaseHeight))
    {
        m_baseHeight = preset.get(ComponentPreset::kBaseHeight);
    }
    if (preset.has(ComponentPreset::kScrewHeight))
    {
        m_screwHeight = preset.get(ComponentPreset::kScrewHeight);
    }
    if (preset.has(ComponentPreset::kCd) && preset.get(ComponentPreset::kCd) > 0)
    {
        setCDOverridden(true);
        setOverrideCD(preset.get(ComponentPreset::kCd));
    }

    double totalMass      = 0;
    bool   massOverridden = false;
    if (preset.has(ComponentPreset::kMass))
    {
        massOverridden = true;
        totalMass += preset.get(ComponentPreset::kMass);
    }
    if (preset.has(ComponentPreset::kScrewMass))
    {
        massOverridden = true;
        totalMass += preset.get(ComponentPreset::kScrewMass);
    }
    if (preset.has(ComponentPreset::kNutMass))
    {
        massOverridden = true;
        totalMass += preset.get(ComponentPreset::kNutMass);
    }
    if (massOverridden)
    {
        setMassOverridden(true);
        setOverrideMass(totalMass);
    }

    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

}  // namespace QtRocket
