#include "QtRocket/gui/binding/MaterialBinding.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/Binding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialDatabase.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The main component's material of @p source when the binding is made.
/// @throws BugError when @p selection is null or the material is not of @p type
[[nodiscard]] Material firstValue(const std::shared_ptr<ComponentSelection>& selection,
                                  const MaterialSource& source, Material::Type type)
{
    if (selection == nullptr)
    {
        bug("A binding of a property needs a component selection");
    }
    Material material = source.getValue(*selection);
    if (material.getType() != type)
    {
        bug("The main component's material is not of the material binding's type");
    }
    return material;
}

/// Whether @p a and @p b are the same in every field (Material's == allows a tolerance in the
/// numbers and ignores whether a material is user-defined, both of which the combo box shows).
[[nodiscard]] bool identical(const Material& a, const Material& b) noexcept
{
    return a.getType() == b.getType() && a.getName() == b.getName() &&
           a.getDensity() == b.getDensity() &&
           a.getInPlaneShearModulus() == b.getInPlaneShearModulus() &&
           a.getGroup() == b.getGroup() && a.isUserDefined() == b.isUserDefined() &&
           a.isDocumentMaterial() == b.isDocumentMaterial();
}

}  // namespace

MaterialBinding::MaterialBinding(std::shared_ptr<ComponentSelection> selection,
                                 MaterialSource source, Material::Type type,
                                 MaterialStorage&    applicationMaterials,
                                 OpenRocketDocument& document)
  : Binding(std::move(selection)),
    m_source(std::move(source)),
    m_type(type),
    m_applicationDatabase(&applicationMaterials.database(type)),
    m_documentDatabase(&document.getDocumentMaterials().database(type)),
    m_value(firstValue(getSelection(), m_source, type))
{
    if (!m_source.isWritable())
    {
        bug("A material combo box needs a setter (Java: MaterialModel requires set<Name>)");
    }
    if (&document.getRocket() != &getSelection()->getRocket())
    {
        bug("The material binding's document is not the document of the selection's rocket");
    }
    m_enabled                = m_source.isEnabled(*getSelection());
    m_materials              = readMaterials();
    const auto changed       = [this](const Material& /*material*/,
                                      const MaterialDatabase& /*database*/) { databaseChanged(); };
    m_databaseConnections[0] = m_applicationDatabase->materialAdded.connect(changed);
    m_databaseConnections[1] = m_applicationDatabase->materialRemoved.connect(changed);
    m_databaseConnections[2] = m_documentDatabase->materialAdded.connect(changed);
    m_databaseConnections[3] = m_documentDatabase->materialRemoved.connect(changed);
}

MaterialBinding::~MaterialBinding() = default;

std::vector<Material> MaterialBinding::readMaterials() const
{
    std::vector<Material> materials(m_applicationDatabase->begin(), m_applicationDatabase->end());
    materials.insert(materials.end(), m_documentDatabase->begin(), m_documentDatabase->end());
    return materials;
}

void MaterialBinding::databaseChanged()
{
    if (!isValid())
    {
        return;  // the document's database may be gone with the rocket
    }
    m_materials = readMaterials();
    fireChanged();
}

bool MaterialBinding::readState()
{
    const ComponentSelection& selection = *getSelection();
    Material                  value     = m_source.getValue(selection);
    if (value.getType() != m_type)
    {
        bug("The main component's material is not of the material binding's type");
    }
    const bool enabled = m_source.isEnabled(selection);
    const bool changed = !identical(value, m_value) || enabled != m_enabled;
    m_value            = std::move(value);
    m_enabled          = enabled;
    return changed;
}

bool MaterialBinding::select(const Material& material)
{
    if (material.getType() != m_type)
    {
        bug("A material of another type was given to a material binding");
    }
    if (!isWidgetWriteAllowed())
    {
        return false;
    }
    // The selection's event (or its own refresh) updates the cache and tells the listeners, one
    // of which may drop the last owner of this binding.
    const std::shared_ptr<Binding> keepAlive = weak_from_this().lock();
    m_source.set(*getSelection(), material);
    return true;
}

int MaterialBinding::size() const noexcept
{
    return static_cast<int>(m_materials.size());
}

const Material* MaterialBinding::materialAt(int index) const noexcept
{
    if (index < 0 || index >= size())
    {
        return nullptr;
    }
    return &m_materials[static_cast<std::size_t>(index)];
}

int MaterialBinding::indexOf(const Material& material) const noexcept
{
    const auto found = std::ranges::find(m_materials, material);
    if (found == m_materials.end())
    {
        return -1;
    }
    return static_cast<int>(std::distance(m_materials.begin(), found));
}

std::vector<MaterialBinding::GroupItems> MaterialBinding::groups() const
{
    // Java: a TreeMap by group priority (MaterialGroup's declaration order) of the groups that
    // have materials, each group's list sorted with Material.compareTo() (List.sort: stable).
    std::vector<GroupItems> groups;
    for (const MaterialGroup group : kAllMaterialGroups)
    {
        GroupItems items{.group = group, .indices = {}};
        for (int index = 0; index < size(); ++index)
        {
            if (m_materials[static_cast<std::size_t>(index)].getGroup() == group)
            {
                items.indices.push_back(index);
            }
        }
        if (items.indices.empty())
        {
            continue;
        }
        std::ranges::stable_sort(items.indices, [this](int a, int b) {
            return m_materials[static_cast<std::size_t>(a)].compareTo(
                       m_materials[static_cast<std::size_t>(b)]) < 0;
        });
        groups.push_back(std::move(items));
    }
    return groups;
}

std::string MaterialBinding::displayString(const Material& material)
{
    std::string text = material.toString();
    if (material.isUserDefined())
    {
        text.insert(0, "(ud) ");
    }
    return text;
}

}  // namespace QtRocket::Gui
