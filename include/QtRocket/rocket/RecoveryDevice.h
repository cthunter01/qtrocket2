#pragma once

#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfigurableComponent.h"
#include "QtRocket/rocket/FlightConfigurableParameterSet.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket
{

class ComponentPreset;
class FlightConfigurationId;

/// A device that slows the descent (OpenRocket's RecoveryDevice): parachutes and streamers. It
/// has no aerodynamic effect on the way up (it is inside the rocket); after deployment the
/// simulation uses its drag coefficient (getCD()) and area (getArea()). Its mass is the area
/// times the surface density of its material, plus what a subclass adds.
///
/// The drag coefficient is automatic by default: getCD() then stores getComponentCD(), as Java's
/// getter does, so the field is mutable. When to deploy is a DeploymentConfiguration per flight
/// configuration. A drogue flag is kept for the simulation's warnings.
///
/// A new device is made of the built-in default surface material, "Ripstop nylon", and keeps it
/// as its default material (getDefaultMaterial()), the one a preset without a usable MATERIAL
/// leaves. Deviation: Java's constructor reads both from the application preferences
/// (getDefaultComponentMaterial(RecoveryDevice.class, SURFACE), the RecoveryDevice class chain
/// for a parachute and a streamer alike) and keeps the default in a final field; rocket/ has no
/// access to the preferences (see StructuralComponent), so whoever creates a device for the user
/// applies that material with setDefaultMaterial() and setMaterial(). HOOK(document): Java adds
/// a document material set from a preset to the document's preferences (see
/// StructuralComponent).
///
/// Not ported: the multi-edit config listener overrides (addConfigListener() and friends), by
/// decision (see RocketComponent).
class RecoveryDevice : public MassObject, public virtual FlightConfigurableComponent
{
public:
    /// The device's area, from which its mass and drag follow.
    [[nodiscard]] virtual double getArea() const = 0;

    /// The automatic drag coefficient at @p mach.
    [[nodiscard]] virtual double getComponentCD(double mach) const = 0;

    /// getCD(0).
    [[nodiscard]] double getCD() const;

    /// The drag coefficient: when automatic, getComponentCD(@p mach), stored first.
    [[nodiscard]] double getCD(double mach) const;

    /// Sets the drag coefficient and makes it manual; unless it was manual and equal (within
    /// MathUtil::equals()), clears the preset and fires AERODYNAMIC_CHANGE.
    void setCD(double cd);

    [[nodiscard]] bool isCDAutomatic() const noexcept { return m_cdAutomatic; }

    /// Makes the drag coefficient automatic or not; fires AERODYNAMIC_CHANGE when it changes.
    void setCDAutomatic(bool automatic);

    /// Whether the device is a drogue (deployed before the main).
    [[nodiscard]] bool isDrogue() const noexcept { return m_drogue; }

    /// Sets the drogue flag; fires NONFUNCTIONAL_CHANGE when it changes.
    void setDrogue(bool drogue);

    /// The surface material.
    [[nodiscard]] const Material& getMaterial() const noexcept { return m_material; }

    /// Sets the material; nothing happens when it equals the current one (Material::operator==),
    /// otherwise the preset is cleared and MASS_CHANGE fires.
    /// @throws BugError when @p material is not a SURFACE material (Java:
    ///         IllegalArgumentException "Attempted to set non-surface material").
    void setMaterial(const Material& material);

    /// The surface material a preset without a usable MATERIAL leaves (Java: defaultMaterial).
    [[nodiscard]] const Material& getDefaultMaterial() const noexcept { return m_defaultMaterial; }

    /// Sets the default material (Java sets its final field in the constructor, from the
    /// preferences): fires nothing and leaves the current material and the preset alone. Copies
    /// keep it, as Java's clone keeps the field.
    /// @throws BugError when @p material is not a SURFACE material (Java's cast to
    ///         Material.Surface).
    void setDefaultMaterial(const Material& material);

    /// The materials of the base class followed by the surface material.
    [[nodiscard]] std::vector<Material> getAllMaterials() const override;

    /// When the device deploys, per flight configuration.
    [[nodiscard]] FlightConfigurableParameterSet<DeploymentConfiguration>&
    getDeploymentConfigurations() noexcept
    {
        return m_deploymentConfigurations;
    }
    [[nodiscard]] const FlightConfigurableParameterSet<DeploymentConfiguration>&
    getDeploymentConfigurations() const noexcept
    {
        return m_deploymentConfigurations;
    }

    /// Copies the deployment of @p oldConfigId to @p newConfigId.
    void copyFlightConfiguration(const FlightConfigurationId& oldConfigId,
                                 const FlightConfigurationId& newConfigId) override;

    /// Makes @p fcid use the default deployment.
    void reset(const FlightConfigurationId& fcid) override;

    /// getArea() times the material's surface density.
    [[nodiscard]] double getComponentMass() const override;

protected:
    /// A device of the built-in default surface material (its default material too), with the
    /// default deployment (DeploymentConfiguration{}), an automatic drag coefficient
    /// (kInitialCd, Parachute::kDefaultCd, until computed) and no drogue flag.
    RecoveryDevice();

    /// The base class's values, then the material: the preset's MATERIAL when it has one whose
    /// toString() is longer than 12 characters (Java's String.length(); "NEED a better way to
    /// set preset if field is empty"), else getDefaultMaterial(). Fires AEROMASS_CHANGE.
    /// @throws BugError when that MATERIAL is not a SURFACE material (Java: ClassCastException;
    ///         ComponentPresetFactory refuses such a preset, so only a preset made otherwise has
    ///         one).
    void loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options) override;

    /// The stored drag coefficient of a new device (Java initialises it with Parachute.DEFAULT_CD;
    /// Parachute::kDefaultCd is defined as this value).
    static constexpr double kInitialCd = 0.8;

    /// The drag coefficient (Java: cd); mutable since getCD() refreshes it.
    mutable double m_cd{kInitialCd};
    /// Whether the drag coefficient is automatic (Java: cdAutomatic).
    bool m_cdAutomatic{true};

private:
    bool                                                    m_drogue{false};
    Material                                                m_defaultMaterial;
    Material                                                m_material;
    FlightConfigurableParameterSet<DeploymentConfiguration> m_deploymentConfigurations;
};

}  // namespace QtRocket
