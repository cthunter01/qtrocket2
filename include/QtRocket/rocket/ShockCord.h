#pragma once

#include <memory>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket
{

/// A shock cord (OpenRocket's ShockCord): a cord of a line material, packed into a small
/// cylinder; its mass is the material's linear density times the cord length. The cord length
/// is automatic by default: three times the rocket's length (the stored length outside a
/// rocket), and getCordLength() stores it, as Java's getter does.
///
/// A new cord is 0.4 m of the built-in default line material, "Elastic cord (round 2 mm,
/// 1/16 in)" (see StructuralComponent for why the preferences are not consulted). It holds no
/// children.
///
/// Not ported: the multi-edit config listeners (see RocketComponent).
class ShockCord : public MassObject
{
public:
    using RocketComponent::isCompatible;

    /// The automatic cord length per rocket length (AUTO_CORD_LENGTH_RATIO).
    static constexpr double kAutoCordLengthRatio = 3.0;

    /// A 0.4 m cord with an automatic length, of the built-in default line material.
    ShockCord();

    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::SHOCK_CORD; }

    /// The cord material.
    [[nodiscard]] const Material& getMaterial() const noexcept { return m_material; }

    /// Sets the cord material; fires MASS_CHANGE unless it equals the current one
    /// (Material::operator==). It does not clear the preset, as in Java.
    /// @throws BugError when @p material is not a LINE material (Java: BugException
    ///         "Attempting to set non-linear material.").
    void setMaterial(const Material& material);

    /// The materials of the base class followed by the cord material.
    [[nodiscard]] std::vector<Material> getAllMaterials() const override;

    /// The cord length; when automatic, kAutoCordLengthRatio times the rocket's length (the
    /// stored length when the root is not a Rocket), stored first.
    [[nodiscard]] double getCordLength() const;

    /// Sets the cord length (negative values and NaN become 0, MathUtil::max()) and makes it
    /// manual; fires MASS_CHANGE unless it was manual and equal (within MathUtil::equals()).
    void setCordLength(double length);

    [[nodiscard]] bool isCordLengthAutomatic() const noexcept { return m_cordLengthAutomatic; }

    /// Makes the cord length automatic (storing the automatic length) or not; fires MASS_CHANGE
    /// when it changes.
    void setCordLengthAutomatic(bool automatic);

    /// The material's density times getCordLength().
    [[nodiscard]] double getComponentMass() const override;

    /// Always false.
    [[nodiscard]] bool allowsChildren() const override;

    /// Always false.
    [[nodiscard]] bool isCompatible(ComponentKind kind) const override;

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;

private:
    /// kAutoCordLengthRatio times the rocket's length, or the stored length outside a rocket.
    [[nodiscard]] double getAutoCordLength() const;

    Material       m_material;
    mutable double m_cordLength{0.4};
    bool           m_cordLengthAutomatic{true};
};

}  // namespace QtRocket
