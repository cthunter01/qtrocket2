#include "QtRocket/rocket/Parachute.h"

#include <memory>
#include <numbers>
#include <optional>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/TypedKey.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The value of @p key in @p preset when it has one greater than 0 (Java: preset.has(key) &&
/// preset.get(key) > 0), else nullopt.
template <class T>
[[nodiscard]] std::optional<T> positiveValue(const ComponentPreset& preset, const TypedKey<T>& key)
{
    if (preset.has(key) && preset.get(key) > 0)
    {
        return preset.get(key);
    }
    return std::nullopt;
}

}  // namespace

Parachute::Parachute()
  : m_defaultLineMaterial(builtinDefaultComponentMaterial(Material::Type::LINE)),
    m_lineMaterial(m_defaultLineMaterial)
{
    setDisplayOrderSide(11);  // Order for displaying the component in the 2D side view
    setDisplayOrderBack(9);   // Order for displaying the component in the 2D back view
}

std::unique_ptr<RocketComponent> Parachute::cloneShallow() const
{
    return std::make_unique<Parachute>(*this);
}

void Parachute::setDiameter(double d)
{
    if (MathUtil::equals(m_diameter, d))
    {
        return;
    }
    m_diameter = d;
    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

double Parachute::getArea() const
{
    return std::numbers::pi * MathUtil::pow2(m_diameter / 2);
}

void Parachute::setArea(double area)
{
    if (MathUtil::equals(getArea(), area))
    {
        return;
    }
    m_diameter = MathUtil::safeSqrt(area / std::numbers::pi) * 2;
    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

double Parachute::getComponentCD(double /*mach*/) const
{
    return m_cd;  // OpenRocket: "TODO: HIGH: Better parachute CD estimate?"
}

void Parachute::setLineCount(int n)
{
    if (m_lineCount == n)
    {
        return;
    }
    m_lineCount = n;
    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

double Parachute::getLineLength() const
{
    if (m_lineLengthAutomatic)
    {
        m_lineLength = getAutoLineLength();
    }
    return m_lineLength;
}

void Parachute::setLineLength(double length)
{
    if (MathUtil::equals(m_lineLength, length) && !m_lineLengthAutomatic)
    {
        return;
    }
    m_lineLength          = length;
    m_lineLengthAutomatic = false;
    if (getLineCount() != 0)
    {
        fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
        clearPreset();
    }
    else
    {
        fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    }
}

double Parachute::getAutoLineLength() const
{
    return m_diameter * kAutoLineLengthRatio;
}

void Parachute::setLineLengthAutomatic(bool automatic)
{
    if (m_lineLengthAutomatic == automatic)
    {
        return;
    }
    m_lineLengthAutomatic = automatic;
    if (m_lineLengthAutomatic)
    {
        m_lineLength = getAutoLineLength();
    }
    if (getLineCount() != 0)
    {
        fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
    }
    else
    {
        fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    }
}

void Parachute::setLineMaterial(const Material& material)
{
    if (material.getType() != Material::Type::LINE)
    {
        bug("Attempted to set non-line material " + material.toString());
    }
    if (material == m_lineMaterial)
    {
        return;
    }
    m_lineMaterial = material;
    // HOOK(document): a document material goes to the document's preferences (see
    // StructuralComponent).
    if (getLineCount() != 0)
    {
        clearPreset();
        fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
    }
    else
    {
        fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    }
}

void Parachute::setDefaultLineMaterial(const Material& material)
{
    if (material.getType() != Material::Type::LINE)
    {
        bug("Attempted to set non-line default material " + material.toString());
    }
    m_defaultLineMaterial = material;
}

std::vector<Material> Parachute::getAllMaterials() const
{
    std::vector<Material> materials = RecoveryDevice::getAllMaterials();
    materials.push_back(m_lineMaterial);
    return materials;
}

double Parachute::getComponentMass() const
{
    return RecoveryDevice::getComponentMass() +
           (getLineCount() * getLineLength() * getLineMaterial().getDensity());
}

bool Parachute::allowsChildren() const
{
    return false;
}

bool Parachute::isCompatible(ComponentKind /*kind*/) const
{
    return false;
}

void Parachute::loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options)
{
    RecoveryDevice::loadFromPreset(preset, options);

    const bool allowAutoRadius = options.allowAutoRadius.value_or(true);

    // Substitute the preset's parachute values for the component's.
    // The description:
    if (preset.has(ComponentPreset::kDescription) &&
        !preset.get(ComponentPreset::kDescription).empty())
    {
        m_name = preset.get(ComponentPreset::kDescription);
    }
    else
    {
        m_name.clear();  // getComponentName() (see RocketComponent)
    }
    // The diameter:
    m_diameter = positiveValue(preset, ComponentPreset::kDiameter).value_or(kDefaultDiameter);
    // The drag coefficient:
    const std::optional<double> cd = positiveValue(preset, ComponentPreset::kCd);
    m_cdAutomatic                  = !cd.has_value();
    m_cd                           = cd.value_or(kDefaultCd);
    // The line count:
    m_lineCount = positiveValue(preset, ComponentPreset::kLineCount).value_or(kDefaultLineCount);
    // The line length (manual either way):
    m_lineLength = positiveValue(preset, ComponentPreset::kLineLength).value_or(kDefaultLineLength);
    m_lineLengthAutomatic = false;
    // The line material ("NEED a better way to set preset if field is empty"):
    if (preset.has(ComponentPreset::kLineMaterial) &&
        Strings::javaLength(preset.get(ComponentPreset::kLineMaterial).toString()) > 12)
    {
        const Material& material = preset.get(ComponentPreset::kLineMaterial);
        // Java's field takes any material; ComponentPresetFactory refuses a preset whose line
        // material is not a LINE one, so this guards setLineMaterial()'s invariant.
        if (material.getType() != Material::Type::LINE)
        {
            bug("Attempted to load non-line material " + material.toString());
        }
        m_lineMaterial = material;
        // HOOK(document): a document material goes to the document's preferences (see
        // StructuralComponent).
    }
    else
    {
        m_lineMaterial = m_defaultLineMaterial;
    }

    // The packed length:
    if (const std::optional<double> length = positiveValue(preset, ComponentPreset::kPackedLength))
    {
        setLength(*length);
    }
    // The packed diameter:
    if (const std::optional<double> diameter =
            positiveValue(preset, ComponentPreset::kPackedDiameter))
    {
        setRadius(*diameter / 2);
    }
    // Fit the packed diameter into the parent's inner diameter:
    if (preset.has(ComponentPreset::kPackedLength) && (getLength() > 0) &&
        preset.has(ComponentPreset::kPackedDiameter) && (getRadius() > 0) && allowAutoRadius)
    {
        setRadiusAutomatic(true);
    }

    // The mass override:
    const std::optional<double> mass = positiveValue(preset, ComponentPreset::kMass);
    m_overrideMass                   = mass.value_or(0);
    m_massOverridden                 = mass.has_value();
}

}  // namespace QtRocket
