#include "QtRocket/rocket/RecoveryDevice.h"

#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

RecoveryDevice::RecoveryDevice()
  : m_defaultMaterial(builtinDefaultComponentMaterial(Material::Type::SURFACE)),
    m_material(m_defaultMaterial),
    m_deploymentConfigurations(DeploymentConfiguration{})
{
}

double RecoveryDevice::getCD() const
{
    return getCD(0);
}

double RecoveryDevice::getCD(double mach) const
{
    if (m_cdAutomatic)
    {
        m_cd = getComponentCD(mach);
    }
    return m_cd;
}

void RecoveryDevice::setCD(double cd)
{
    if (MathUtil::equals(m_cd, cd) && !isCDAutomatic())
    {
        return;
    }
    m_cd          = cd;
    m_cdAutomatic = false;
    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kAerodynamicChange);
}

void RecoveryDevice::setCDAutomatic(bool automatic)
{
    if (m_cdAutomatic == automatic)
    {
        return;
    }
    m_cdAutomatic = automatic;
    fireComponentChangeEvent(ComponentChangeEvent::kAerodynamicChange);
}

void RecoveryDevice::setDrogue(bool drogue)
{
    if (m_drogue == drogue)
    {
        return;
    }
    m_drogue = drogue;
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

void RecoveryDevice::setMaterial(const Material& material)
{
    if (material.getType() != Material::Type::SURFACE)
    {
        bug("Attempted to set non-surface material " + material.toString());
    }
    if (material == m_material)
    {
        return;
    }
    m_material = material;
    notifyDocumentMaterial(m_material);
    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void RecoveryDevice::setDefaultMaterial(const Material& material)
{
    if (material.getType() != Material::Type::SURFACE)
    {
        bug("Attempted to set non-surface default material " + material.toString());
    }
    m_defaultMaterial = material;
}

std::vector<Material> RecoveryDevice::getAllMaterials() const
{
    std::vector<Material> materials = MassObject::getAllMaterials();
    materials.push_back(m_material);
    return materials;
}

void RecoveryDevice::copyFlightConfiguration(const FlightConfigurationId& oldConfigId,
                                             const FlightConfigurationId& newConfigId)
{
    m_deploymentConfigurations.copyFlightConfiguration(oldConfigId, newConfigId);
}

void RecoveryDevice::reset(const FlightConfigurationId& fcid)
{
    m_deploymentConfigurations.reset(fcid);
}

double RecoveryDevice::getComponentMass() const
{
    return getArea() * getMaterial().getDensity();
}

void RecoveryDevice::loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options)
{
    MassObject::loadFromPreset(preset, options);

    // Set the preset's material ("NEED a better way to set preset if field is empty").
    if (preset.has(ComponentPreset::kMaterial))
    {
        const Material& material = preset.get(ComponentPreset::kMaterial);
        // Java: String.length() of Material.toString(), "name (density unit)".
        if (Strings::javaLength(material.toString()) > 12)
        {
            if (material.getType() != Material::Type::SURFACE)
            {
                // Java: ClassCastException from the cast to Material.Surface.
                bug("Attempted to load non-surface material " + material.toString());
            }
            m_material = material;
            notifyDocumentMaterial(m_material);
        }
        else
        {
            m_material = m_defaultMaterial;
        }
    }
    else
    {
        m_material = m_defaultMaterial;
    }

    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

}  // namespace QtRocket
