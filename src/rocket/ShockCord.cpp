#include "QtRocket/rocket/ShockCord.h"

#include <memory>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

ShockCord::ShockCord() : m_material(builtinDefaultComponentMaterial(Material::Type::LINE))
{
    setDisplayOrderSide(12);  // Order for displaying the component in the 2D side view
    setDisplayOrderBack(7);   // Order for displaying the component in the 2D back view
}

std::unique_ptr<RocketComponent> ShockCord::cloneShallow() const
{
    return std::make_unique<ShockCord>(*this);
}

void ShockCord::setMaterial(const Material& material)
{
    if (material.getType() != Material::Type::LINE)
    {
        bug("Attempting to set non-linear material.");
    }
    if (m_material == material)
    {
        return;
    }
    m_material = material;
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

std::vector<Material> ShockCord::getAllMaterials() const
{
    std::vector<Material> materials = MassObject::getAllMaterials();
    materials.push_back(m_material);
    return materials;
}

double ShockCord::getCordLength() const
{
    if (m_cordLengthAutomatic)
    {
        m_cordLength = getAutoCordLength();
    }
    return m_cordLength;
}

void ShockCord::setCordLength(double length)
{
    length = MathUtil::max(length, 0);
    if (MathUtil::equals(length, m_cordLength) && !m_cordLengthAutomatic)
    {
        return;
    }
    m_cordLength          = length;
    m_cordLengthAutomatic = false;
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

double ShockCord::getAutoCordLength() const
{
    const Rocket* rocket = findRocket();
    if (rocket == nullptr)
    {
        return m_cordLength;
    }
    return rocket->getLength() * kAutoCordLengthRatio;
}

void ShockCord::setCordLengthAutomatic(bool automatic)
{
    if (m_cordLengthAutomatic == automatic)
    {
        return;
    }
    m_cordLengthAutomatic = automatic;
    if (m_cordLengthAutomatic)
    {
        m_cordLength = getAutoCordLength();
    }
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

double ShockCord::getComponentMass() const
{
    return m_material.getDensity() * getCordLength();
}

bool ShockCord::allowsChildren() const
{
    return false;
}

bool ShockCord::isCompatible(ComponentKind /*kind*/) const
{
    return false;
}

}  // namespace QtRocket
