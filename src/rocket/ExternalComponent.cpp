#include "QtRocket/rocket/ExternalComponent.h"

#include <memory>
#include <string>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

const Material& ExternalComponent::defaultMaterial()
{
    static const Material kDefault = builtinDefaultComponentMaterial(Material::Type::BULK);
    return kDefault;
}

ExternalComponent::ExternalComponent(AxialMethod relativePosition)
  : RocketComponent(relativePosition), m_material(defaultMaterial())
{
}

double ExternalComponent::getComponentMass() const
{
    return m_material.getDensity() * getComponentVolume();
}

bool ExternalComponent::isAerodynamic() const
{
    return true;
}

bool ExternalComponent::isMassive() const
{
    return true;
}

void ExternalComponent::setMaterial(const Material& mat)
{
    if (mat.getType() != Material::Type::BULK)
    {
        bug("ExternalComponent requires a bulk material type=" +
            std::string(::QtRocket::toString(mat.getType())));
    }

    if (m_material == mat)
    {
        return;
    }
    m_material = mat;
    // HOOK(document): Java adds a document material to the document's preferences here
    // (rocket.getDocument().getDocumentPreferences().addMaterial(mat)) when the root is a Rocket
    // that belongs to a document; rocket/ cannot reach OpenRocketDocument.
    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void ExternalComponent::applyDefaultMaterial(const Preferences&     preferences,
                                             const MaterialStorage& storage)
{
    m_material = getDefaultComponentMaterial(preferences, componentClassChain(kind()),
                                             Material::Type::BULK, storage);
}

std::vector<Material> ExternalComponent::getAllMaterials() const
{
    std::vector<Material> materials = RocketComponent::getAllMaterials();
    materials.push_back(m_material);
    return materials;
}

void ExternalComponent::setFinish(Finish finish)
{
    if (m_finish == finish)
    {
        return;
    }
    m_finish = finish;
    fireComponentChangeEvent(ComponentChangeEvent::kAerodynamicChange |
                             ComponentChangeEvent::kGraphicChange);
}

void ExternalComponent::loadFromPreset(const ComponentPreset&   preset,
                                       const PresetLoadOptions& options)
{
    RocketComponent::loadFromPreset(preset, options);

    if (preset.has(ComponentPreset::kFinish))
    {
        setFinish(preset.get(ComponentPreset::kFinish));
    }

    if (preset.has(ComponentPreset::kMaterial))
    {
        m_material = preset.get(ComponentPreset::kMaterial);
        // HOOK(document): Java registers a document material with the document here too (see
        // setMaterial()).
    }
}

std::vector<std::unique_ptr<RocketComponent>> ExternalComponent::copyFrom(
    const RocketComponent& source)
{
    const auto* src = dynamic_cast<const ExternalComponent*>(&source);
    if (src == nullptr)
    {
        bug("ExternalComponent::copyFrom(): the source is not an ExternalComponent");
    }
    // Java assigns the two fields first; assigning them once RocketComponent::copyFrom() has
    // succeeded leaves this component unchanged when it throws (RocketComponent's part neither
    // reads nor resets them).
    std::vector<std::unique_ptr<RocketComponent>> previous = RocketComponent::copyFrom(source);
    m_finish                                               = src->m_finish;
    m_material                                             = src->m_material;
    return previous;
}

}  // namespace QtRocket
