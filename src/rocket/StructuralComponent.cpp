#include "QtRocket/rocket/StructuralComponent.h"

#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/InternalComponent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

StructuralComponent::StructuralComponent()
  : m_material(builtinDefaultComponentMaterial(Material::Type::BULK))
{
}

void StructuralComponent::loadFromPreset(const ComponentPreset&   preset,
                                         const PresetLoadOptions& options)
{
    InternalComponent::loadFromPreset(preset, options);

    if (preset.has(ComponentPreset::kMaterial))
    {
        // Java checks the material for null, which a preset value never is here.
        m_material = preset.get(ComponentPreset::kMaterial);
        notifyDocumentMaterial(m_material);
    }
}

void StructuralComponent::setMaterial(const Material& material)
{
    if (material.getType() != Material::Type::BULK)
    {
        bug("Attempted to set non-bulk material " + material.toString());
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

std::vector<Material> StructuralComponent::getAllMaterials() const
{
    std::vector<Material> materials = InternalComponent::getAllMaterials();
    materials.push_back(m_material);
    return materials;
}

}  // namespace QtRocket
