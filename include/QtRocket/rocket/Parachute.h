#pragma once

#include <memory>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket
{

class ComponentPreset;

/// A parachute (OpenRocket's Parachute): a round canopy of a diameter and a surface material,
/// with shroud lines of a line material. Its mass is the canopy's plus line count * line length
/// * the line material's density. The line length is automatic by default: kAutoLineLengthRatio
/// times the diameter, stored by getLineLength() as Java's getter does. The automatic drag
/// coefficient is the stored one (kDefaultCd unless a preset or setCD() changed it; OpenRocket
/// has no better estimate).
///
/// A new parachute has a diameter of 0.3 m and six 0.3 m lines (automatic, so 0.45 m) of the
/// built-in default line material, "Elastic cord (round 2 mm, 1/16 in)", which it keeps as its
/// default line material (getDefaultLineMaterial()), the one a preset without a usable
/// LINE_MATERIAL leaves. Deviation: Java's constructor reads both from the application
/// preferences (getDefaultComponentMaterial(Parachute.class, LINE), the Parachute class chain)
/// and keeps the default in a final field; rocket/ has no access to the preferences (see
/// StructuralComponent), so whoever creates a parachute for the user applies that material with
/// setDefaultLineMaterial() and setLineMaterial() (and the canopy's as RecoveryDevice says). It
/// holds no children.
///
/// Presets: loadFromPreset() is Java's two overloads in one; PresetLoadOptions::allowAutoRadius
/// is Java's params[0] (true when not given), which the .ork loader passes as false.
///
/// Not ported: the multi-edit config listeners (see RocketComponent).
class Parachute : public RecoveryDevice
{
public:
    using RocketComponent::isCompatible;

    /// The drag coefficient of a new parachute (DEFAULT_CD; a new streamer starts from it too).
    static constexpr double kDefaultCd = kInitialCd;
    /// The diameter of a new parachute, and of one loaded from a preset without a diameter.
    static constexpr double kDefaultDiameter = 0.3;
    /// The line count of a new parachute, and of one loaded from a preset without a line count.
    static constexpr int kDefaultLineCount = 6;
    /// The line length of a new parachute, and of one loaded from a preset without a line
    /// length.
    static constexpr double kDefaultLineLength = 0.3;
    /// The automatic line length per diameter (AUTO_LINE_LENGTH_RATIO).
    static constexpr double kAutoLineLengthRatio = 1.5;

    /// A parachute of diameter 0.3 m with six lines of automatic length.
    Parachute();

    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::PARACHUTE; }

    [[nodiscard]] double getDiameter() const noexcept { return m_diameter; }

    /// Sets the diameter; unless it equals (within MathUtil::equals()) the current one, clears
    /// the preset and fires AEROMASS_CHANGE.
    void setDiameter(double d);

    /// The canopy area: pi (diameter / 2)^2.
    [[nodiscard]] double getArea() const override;

    /// Sets the diameter that gives @p area (2 sqrt(area / pi), MathUtil::safeSqrt()); unless the
    /// area equals (within MathUtil::equals()) the current one, clears the preset and fires
    /// AEROMASS_CHANGE.
    void setArea(double area);

    /// The stored drag coefficient (OpenRocket has no better parachute estimate).
    [[nodiscard]] double getComponentCD(double mach) const override;

    [[nodiscard]] int getLineCount() const noexcept { return m_lineCount; }

    /// Sets the number of shroud lines; when it changes, clears the preset and fires
    /// MASS_CHANGE.
    void setLineCount(int n);

    /// The line length; when automatic, kAutoLineLengthRatio times the diameter, stored first.
    [[nodiscard]] double getLineLength() const;

    /// Sets the line length and makes it manual; unless it was manual and equal (within
    /// MathUtil::equals()), fires MASS_CHANGE and then clears the preset when there are lines,
    /// else fires NONFUNCTIONAL_CHANGE (and keeps the preset).
    void setLineLength(double length);

    [[nodiscard]] bool isLineLengthAutomatic() const noexcept { return m_lineLengthAutomatic; }

    /// Makes the line length automatic (storing the automatic length) or not; when it changes,
    /// fires MASS_CHANGE when there are lines, else NONFUNCTIONAL_CHANGE.
    void setLineLengthAutomatic(bool automatic);

    /// The shroud line material.
    [[nodiscard]] const Material& getLineMaterial() const noexcept { return m_lineMaterial; }

    /// Sets the line material; unless it equals the current one (Material::operator==), clears
    /// the preset and fires MASS_CHANGE when there are lines, else fires NONFUNCTIONAL_CHANGE.
    /// A document material is announced first, with lines or without (see RecoveryDevice), as
    /// is the LINE_MATERIAL a preset gives the parachute, after the preset's canopy material.
    /// @throws BugError when @p material is not a LINE material (Java: IllegalArgumentException
    ///         "Attempted to set non-line material").
    void setLineMaterial(const Material& material);

    /// The line material a preset without a usable LINE_MATERIAL leaves (Java:
    /// DEFAULT_LINE_MATERIAL).
    [[nodiscard]] const Material& getDefaultLineMaterial() const noexcept
    {
        return m_defaultLineMaterial;
    }

    /// Sets the default line material (Java sets its final field in the constructor, from the
    /// preferences): fires nothing and leaves the current line material and the preset alone.
    /// Copies keep it, as Java's clone keeps the field.
    /// @throws BugError when @p material is not a LINE material.
    void setDefaultLineMaterial(const Material& material);

    /// The materials of the base class followed by the line material.
    [[nodiscard]] std::vector<Material> getAllMaterials() const override;

    /// The canopy's mass plus line count * getLineLength() * the line material's density.
    [[nodiscard]] double getComponentMass() const override;

    /// Always false.
    [[nodiscard]] bool allowsChildren() const override;

    /// Always false.
    [[nodiscard]] bool isCompatible(ComponentKind kind) const override;

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;

    /// Substitutes the preset's parachute values for the component's (Java's loadFromPreset(
    /// preset, params...)), after RecoveryDevice's: the name from a non-empty DESCRIPTION (else
    /// the component name); the DIAMETER, a CD (making the drag coefficient manual; without one
    /// it becomes automatic and kDefaultCd), the LINE_COUNT and the LINE_LENGTH (the line length
    /// becomes manual either way) when positive, else the defaults; the LINE_MATERIAL when its
    /// toString() is longer than 12 characters, else getDefaultLineMaterial(); a positive
    /// PACKED_LENGTH through setLength() and a positive PACKED_DIAMETER through setRadius()
    /// (both fire); an automatic radius when the preset has both packed dimensions, the length
    /// and radius are positive and @p options allow it (allowAutoRadius, true when not given);
    /// and a positive MASS as the override mass (mass overridden), else no mass override (the
    /// override mass 0). Like Java, it stores the mass override directly, without updating the
    /// children's overriddenBy pointers (a parachute has no children).
    /// @throws BugError when that LINE_MATERIAL is not a LINE material (Java stores any material;
    ///         ComponentPresetFactory refuses such a preset, so only a preset made otherwise has
    ///         one).
    void loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options) override;

private:
    /// kAutoLineLengthRatio times the diameter.
    [[nodiscard]] double getAutoLineLength() const;

    double         m_diameter{kDefaultDiameter};
    Material       m_defaultLineMaterial;
    Material       m_lineMaterial;
    int            m_lineCount{kDefaultLineCount};
    mutable double m_lineLength{kDefaultLineLength};
    bool           m_lineLengthAutomatic{true};
};

}  // namespace QtRocket
