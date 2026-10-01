#pragma once

#include <memory>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"

namespace QtRocket
{

class ComponentPreset;

/// A component with a well-defined physical appearance that affects the aerodynamics
/// (OpenRocket's ExternalComponent): body components, fin sets, launch lugs, rail buttons and
/// tube fin sets. It has a bulk material, held by value (see the plan's decision: a document
/// material that the user edits is re-applied to every component whose material equals it), and
/// its mass is the material's density times getComponentVolume(). Its surface finish is a
/// rocket/Finish (Java's nested ExternalComponent.Finish), NORMAL by default.
///
/// Deviations from OpenRocket:
/// - A new component's material is the built-in bulk default, "Cardboard" (defaultMaterial()).
///   Java's constructor asks the global application preferences
///   (getDefaultComponentMaterial(getClass(), BULK)), which there is no global for here; with
///   OpenRocket's test preferences that is Cardboard too, so the golden data and the ported tests
///   agree. The GUI applies the user's per-class default (material/MaterialPreferences.h) to a
///   component it creates.
/// - setMaterial() and loadFromPreset() do not register a document material with the document's
///   preferences: rocket/ cannot see the document (see the HOOK in ExternalComponent.cpp).
/// - setMaterial() of a material that is not BULK throws BugError (Java:
///   IllegalArgumentException).
/// - The multi-edit config listeners are not ported (see RocketComponent).
class ExternalComponent : public RocketComponent
{
public:
    /// The material a new external component starts with: the built-in bulk "Cardboard"
    /// (ApplicationPreferences' DEFAULT_BULK_MATERIAL, kDefaultBulkMaterialName).
    [[nodiscard]] static const Material& defaultMaterial();

    /// The volume of the component's material, from which its mass is computed.
    [[nodiscard]] virtual double getComponentVolume() const = 0;

    /// The material's density times getComponentVolume().
    [[nodiscard]] double getComponentMass() const override;

    /// True: an external component has an aerodynamic effect.
    [[nodiscard]] bool isAerodynamic() const override;

    /// True: an external component has mass.
    [[nodiscard]] bool isMassive() const override;

    [[nodiscard]] const Material& getMaterial() const noexcept { return m_material; }

    /// Sets the material; when it differs (Material's ==), clears the preset and fires
    /// MASS_CHANGE.
    /// @throws BugError when @p mat is not a BULK material (Java: IllegalArgumentException).
    void setMaterial(const Material& mat);

    /// The materials of the base class (none) followed by this component's material.
    [[nodiscard]] std::vector<Material> getAllMaterials() const override;

    [[nodiscard]] Finish getFinish() const noexcept { return m_finish; }

    /// Sets the surface finish; fires AERODYNAMIC_CHANGE | GRAPHIC_CHANGE when it changes (it
    /// keeps the preset).
    void setFinish(Finish finish);

protected:
    /// A component positioned by @p relativePosition, with defaultMaterial() and NORMAL finish.
    explicit ExternalComponent(AxialMethod relativePosition);

    /// Loads the base properties, then the preset's FINISH (through setFinish()) and MATERIAL
    /// (stored directly: no event, the preset is kept).
    void loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options) override;

    /// Copies the finish and the material of @p source, which must be an ExternalComponent, then
    /// RocketComponent's fields.
    /// @throws BugError when @p source is not an ExternalComponent (Java: ClassCastException),
    ///         and as RocketComponent::copyFrom().
    std::vector<std::unique_ptr<RocketComponent>> copyFrom(const RocketComponent& source) override;

    /// The material (Java: protected field material).
    Material m_material;
    /// The surface finish (Java: protected field finish).
    Finish m_finish{Finish::NORMAL};
};

}  // namespace QtRocket
