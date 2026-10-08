#pragma once

#include <functional>
#include <string_view>

#include "QtRocket/file/openrocket/Setter.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class RocketComponent;

/// Sets a material of a component from `<material type="bulk" density="680.0"
/// shearModulus="4.0E8" group="PaperProducts">Cardboard</material>` (OpenRocket's
/// file/openrocket/importt/MaterialSetter). The text is the material's name; the setter knows
/// the type its component takes (BULK for a body, a fin or a ring and for a fin's fillet,
/// SURFACE for a parachute or streamer, LINE for a shock cord and a parachute's lines).
///
/// set() does, in this order:
/// - The name is the text trimmed (String.trim()). An empty name: the warning "Illegal material
///   specification, ignoring.", and nothing is set.
/// - The density is the attribute "density" as Double.parseDouble reads it. No such attribute,
///   or a text that is no number: the same warning, and nothing is set.
/// - The shear modulus is the attribute "shearModulus" (spelled so), when the element has it. A
///   text that is no number adds the warning "Illegal shear modulus value, using 0.0." and the
///   shear modulus is 0.
/// - The attribute "type", when the element has it, must be the setter's type as an .ork file
///   writes it ("bulk", "surface", "line"), compared as it is. Anything else: the warning
///   "Illegal material type specified, ignoring.", and nothing is set.
/// - The group is the attribute "group", when the element has it, as
///   MaterialStorage::materialGroupFromLegacyDatabaseString() reads it (the name a file stores,
///   "PaperProducts"; "ThreadsLines" of older files is looked up). A text that names no group
///   adds the warning "Illegal material group specified, ignoring." and the setter goes on
///   without a group.
/// - The material is then looked up in the application's materials (Java: Databases; here
///   DocumentLoadingContext::getApplicationMaterials()) as MaterialStorage::findMaterial()
///   does: by name and density when the element has no shear modulus, by name, density and
///   shear modulus when it has one. The group is not compared. A material that is found is
///   taken as the application has it (its own shear modulus and group). One that is not found
///   becomes a new material of the document, user-defined, with the element's name, density,
///   shear modulus (0 when it has none) and group (CUSTOM when it has none).
/// - Last, the material is set on the component. The component's setter does nothing for a
///   material equal to the one it has; for a material of the document it tells the rocket, whose
///   document adds it to its own materials (Rocket::documentMaterialSet()), where Java's setter
///   registers it with the document's preferences. A fin's fillet and a shock cord do not: Java
///   has no registration there.
///
/// Other attributes are not looked at, and the attributes' names are compared exactly
/// ("shearmodulus" is another attribute).
///
/// So a shear modulus that cannot be read, or one that is not the application's for that
/// material, makes a material of the document out of a name the application knows: "Cardboard"
/// with shearModulus="xx" is a user-defined Cardboard with a shear modulus of 0.
///
/// The component's setter also drops the component's preset link, unless
/// RocketComponent::setIgnorePresetClearing(true) is in force, as it is while the handler of a
/// component's parameters works (see ComponentPresetSetter).
///
/// Deviations from OpenRocket:
/// - A density that is a NaN or an infinity is refused like a text that is no number, and a
///   shear modulus that is one is read as 0 with the warning of a text that is no number (the
///   rule of decision L3 of the loader for what a simulation computes with). OpenRocket stores
///   both: density="NaN" gives a component whose mass is a NaN.
/// - Without application materials (a null pointer in the context) every material is one of
///   the document; Java always has its Databases.
/// - Java removes the attributes it reads from the map; the attributes are const here, and
///   nothing reads them after the setter.
class MaterialSetter final : public Setter
{
public:
    /// What sets the material on the component (Java: the setter method).
    using SetFunction = std::function<void(RocketComponent&, const Material&)>;

    /// A setter of a material of @p type through @p set.
    /// @throws BugError when @p set is empty or @p type is CUSTOM, which no component takes.
    MaterialSetter(SetFunction set, Material::Type type);

    [[nodiscard]] Result<void> set(RocketComponent& component, std::string_view value,
                                   const Attributes& attributes, WarningSet& warnings,
                                   const DocumentLoadingContext& context) const override;

private:
    SetFunction    m_set;
    Material::Type m_type;
};

}  // namespace QtRocket
