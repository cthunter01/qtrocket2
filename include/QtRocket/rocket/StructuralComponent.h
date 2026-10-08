#pragma once

#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/InternalComponent.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket
{

class ComponentPreset;

/// An internal component made of a bulk material (OpenRocket's StructuralComponent): the base of
/// the ring components (inner tubes, couplers, centering rings, bulkheads, engine blocks).
///
/// The material is held by value (see Material). A new component is made of the built-in default
/// bulk material, "Cardboard" (builtinDefaultComponentMaterial()). Deviation: OpenRocket's
/// constructor takes the default material of the component's class from the application
/// preferences; rocket/ has no access to them, so whoever creates a component for the user
/// applies getDefaultComponentMaterial() (MaterialPreferences.h) with setMaterial().
///
/// Document materials: when the material set (by setMaterial() or a preset) is a document
/// material and the rocket belongs to a document, Java adds it to the document's preferences
/// at once (DocumentPreferences.addMaterial()). document/ sits above rocket/, so the component
/// tells its rocket instead (RocketComponent::notifyDocumentMaterial(), at the place of Java's
/// call), and the rocket emits Rocket::documentMaterialSet(), which the document listens to.
class StructuralComponent : public InternalComponent
{
public:
    /// The material.
    [[nodiscard]] const Material& getMaterial() const noexcept { return m_material; }

    /// Sets the material: nothing happens when it equals the current one (Material::operator==),
    /// otherwise a document material is announced (see the class comment), the preset is
    /// cleared and MASS_CHANGE fires.
    /// @throws BugError when @p material is not a BULK material (Java:
    ///         IllegalArgumentException "Attempted to set non-bulk material").
    void setMaterial(const Material& material);

    /// The materials of the base class followed by this component's material.
    [[nodiscard]] std::vector<Material> getAllMaterials() const override;

protected:
    /// A component of the built-in default bulk material, positioned BOTTOM.
    StructuralComponent();

    /// The base class's values, then the preset's MATERIAL when it has one (stored directly,
    /// without an event: the frozen rocket fires loadPreset()'s events; a document material is
    /// announced whatever the component's material was).
    void loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options) override;

private:
    Material m_material;
};

}  // namespace QtRocket
