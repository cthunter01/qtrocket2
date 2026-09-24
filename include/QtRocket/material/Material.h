#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class MaterialStorage;
class Unit;

/// A material with a name and a density (OpenRocket's Material). The density's meaning depends
/// on the type: kg/m^3 for BULK (and CUSTOM), kg/m^2 for SURFACE, kg/m for LINE. The in-plane
/// shear modulus (Pa) serves the fin flutter analysis.
///
/// OpenRocket declares four subclasses, Material.Bulk, Surface, Line and Custom, whose only
/// difference is getType() (Surface.toStorableString just calls the base method): equals(),
/// loadFrom() and the storable string all compare or write the type. One class with a Type field
/// therefore preserves every behavioural difference, and Materials are plain values here, copied
/// rather than shared; the type is fixed at construction.
class Material
{
public:
    enum class Type
    {
        BULK,
        SURFACE,
        LINE,
        CUSTOM,
    };

    /// Material.Type.values().
    static constexpr std::array<Type, 4> kAllTypes{Type::BULK, Type::SURFACE, Type::LINE,
                                                   Type::CUSTOM};

    /// The full constructor (the private Material(...) of OpenRocket plus the type). A missing
    /// @p group becomes CUSTOM for a user-defined material and OTHER otherwise
    /// (getEquivalentGroup). The name is used as is, without translation.
    Material(Type type, std::string name, double density, double inPlaneShearModulus,
             std::optional<MaterialGroup> group, bool userDefined, bool documentMaterial);

    // ---- Material.newMaterial: OpenRocket's seven factories, same parameters and defaults ----

    [[nodiscard]] static Material newMaterial(Type type, std::string name, double density,
                                              double                       inPlaneShearModulus,
                                              std::optional<MaterialGroup> group, bool userDefined,
                                              bool documentMaterial);
    /// Shear modulus 0.
    [[nodiscard]] static Material newMaterial(Type type, std::string name, double density,
                                              std::optional<MaterialGroup> group, bool userDefined,
                                              bool documentMaterial);
    /// Shear modulus 0, not a document material.
    [[nodiscard]] static Material newMaterial(Type type, std::string name, double density,
                                              std::optional<MaterialGroup> group, bool userDefined);
    /// Not a document material.
    [[nodiscard]] static Material newMaterial(Type type, std::string name, double density,
                                              double                       inPlaneShearModulus,
                                              std::optional<MaterialGroup> group, bool userDefined);
    /// No group.
    [[nodiscard]] static Material newMaterial(Type type, std::string name, double density,
                                              double inPlaneShearModulus, bool userDefined,
                                              bool documentMaterial);
    /// Shear modulus 0, no group.
    [[nodiscard]] static Material newMaterial(Type type, std::string name, double density,
                                              bool userDefined, bool documentMaterial);
    /// Shear modulus 0, no group, not a document material.
    [[nodiscard]] static Material newMaterial(Type type, std::string name, double density,
                                              bool userDefined);

    [[nodiscard]] Type               getType() const noexcept { return m_type; }
    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    /// The name followed by the density in @p unit: "Aluminum (2.7 g/cm³)".
    [[nodiscard]] std::string getName(const Unit& unit) const;
    [[nodiscard]] double      getDensity() const noexcept { return m_density; }
    /// The in-plane shear modulus G, in Pa.
    [[nodiscard]] double getInPlaneShearModulus() const noexcept { return m_inPlaneShearModulus; }
    [[nodiscard]] MaterialGroup getGroup() const noexcept { return m_group; }
    /// The group's priority (OpenRocket returns Integer.MAX_VALUE for a null group, which the
    /// constructor never leaves).
    [[nodiscard]] int getGroupPriority() const noexcept;
    /// True for a material the user defined (or a file supplied), false for a built-in one.
    [[nodiscard]] bool isUserDefined() const noexcept { return m_userDefined; }
    /// True for a material stored in the document rather than the application preferences.
    [[nodiscard]] bool isDocumentMaterial() const noexcept { return m_documentMaterial; }
    void               setDocumentMaterial(bool documentMaterial) noexcept
    {
        m_documentMaterial = documentMaterial;
    }

    /// getName() with the density in the default unit of the type's unit group.
    [[nodiscard]] std::string toString() const;

    /// Copies every field of @p other into this material.
    /// @throws BugError when the types differ (OpenRocket: IllegalArgumentException)
    void loadFrom(const Material& other);

    /// Material.equals: the same type, name, density and shear modulus (MathUtil::equals) and
    /// group; whether the material is user-defined or a document material does not count.
    [[nodiscard]] bool operator==(const Material& other) const noexcept;

    /// Material.compareTo: by name (String.compareTo, Strings::javaCompareTo), then by density,
    /// the difference times 1000 truncated to an int as Java does, so densities within 0.001
    /// compare equal. Negative, zero or positive; MaterialDatabase orders by it.
    [[nodiscard]] int compareTo(const Material& other) const noexcept;

    /// Material.hashCode: name.hashCode() + (int)(density * 1000) + (int)(shear * 1e-9) in
    /// Java's wrapping int arithmetic, with Java's String.hashCode of the name. There is no
    /// std::hash<Material>: equality allows a tolerance (and is not transitive), so equal
    /// materials can hash apart and an unordered container of them would break its contract.
    [[nodiscard]] int hashCode() const noexcept;

    /// The preference and .ork document form, "TYPE|name|density|shearModulus|GroupString"
    /// ("BULK|Test Material|1.0|0.0|Woods"): the type's enumerator name, the name with every '|'
    /// replaced by a space, and the two doubles as Java writes them (Strings::javaDoubleToString).
    [[nodiscard]] std::string toStorableString() const;

    /// Material.fromStorableString: parses toStorableString()'s form and the older
    /// "TYPE|name|density|GroupString" and "TYPE|name|density" forms (a fourth field that is not
    /// a number is the group). An unknown group leaves the group unset (OpenRocket logs and goes
    /// on), so the material gets CUSTOM or OTHER; the legacy group "ThreadsLines" resolves to
    /// OTHER without a MaterialStorage to look the material up in. The result is not a document
    /// material. The numbers are read as Double.parseDouble reads them
    /// (Strings::javaParseDouble: "1e999" is an infinity, "1.5d" is 1.5, "Inf" is not a number),
    /// which also decides whether a fourth field is the shear modulus or the group. Fails
    /// (ErrorCode::PARSE, OpenRocket's IllegalArgumentException) for fewer than three fields, an
    /// unknown type, a CUSTOM type or an unparsable density.
    [[nodiscard]] static Result<Material> fromStorableString(std::string_view str,
                                                             bool             userDefined);
    /// As above, resolving "ThreadsLines" through
    /// MaterialStorage::materialGroupFromLegacyDatabaseString.
    [[nodiscard]] static Result<Material> fromStorableString(std::string_view str, bool userDefined,
                                                             const MaterialStorage& storage);

private:
    [[nodiscard]] static Result<Material> fromStorableString(std::string_view str, bool userDefined,
                                                             const MaterialStorage* storage);

    Type          m_type;
    std::string   m_name;
    double        m_density;
    double        m_inPlaneShearModulus;
    MaterialGroup m_group;
    bool          m_userDefined;
    bool          m_documentMaterial;
};

/// Material.Type.name(): "BULK", "SURFACE", "LINE", "CUSTOM", the spelling of the storable
/// string (the .ork <material type=""> attribute is its lower-case form).
[[nodiscard]] std::string_view toString(Material::Type type) noexcept;

/// Material.Type.valueOf: the type named exactly @p name, or nullopt.
[[nodiscard]] std::optional<Material::Type> materialTypeFromString(std::string_view name) noexcept;

/// The translation key of the type's display name, e.g. "Databases.materials.types.Bulk".
[[nodiscard]] std::string_view displayKey(Material::Type type) noexcept;

/// The English display name: "Bulk", "Surface", "Line", "Custom".
[[nodiscard]] std::string_view displayName(Material::Type type) noexcept;

/// Material.Type.getUnitGroup: the density unit group of the type (DENSITY_BULK for BULK and
/// CUSTOM, DENSITY_SURFACE, DENSITY_LINE).
[[nodiscard]] UnitGroupId unitGroupId(Material::Type type) noexcept;

}  // namespace QtRocket
