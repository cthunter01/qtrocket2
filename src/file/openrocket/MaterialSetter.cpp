#include "QtRocket/file/openrocket/MaterialSetter.h"

#include <optional>
#include <string_view>
#include <utility>

#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

constexpr std::string_view kIllegalSpecification = "Illegal material specification, ignoring.";

/// What the element says of the material besides its name.
struct Specification
{
    double                       density{0.0};
    std::optional<double>        shearModulus;
    std::optional<MaterialGroup> group;
};

/// The attribute @p name as Double.parseDouble reads it and as the loader takes it: nullopt for
/// no attribute, for a text that is no number and for a NaN or an infinity (decision L3).
[[nodiscard]] std::optional<double> finiteAttribute(const Setter::Attributes& attributes,
                                                    std::string_view          name)
{
    const std::optional<std::string_view> text = DocumentConfig::attribute(attributes, name);
    return text.has_value() ? DocumentConfig::parseFiniteDouble(*text) : std::nullopt;
}

/// The shear modulus of the element: none without the attribute, and 0 with OpenRocket's
/// warning for an attribute that cannot be read.
[[nodiscard]] std::optional<double> shearModulusOf(const Setter::Attributes& attributes,
                                                   WarningSet&               warnings)
{
    // Parse shear modulus (optional; only use when explicitly provided)
    if (!DocumentConfig::attribute(attributes, "shearModulus").has_value())
    {
        return std::nullopt;
    }
    const std::optional<double> shearModulus = finiteAttribute(attributes, "shearModulus");
    if (!shearModulus.has_value())
    {
        warnings.add("Illegal shear modulus value, using 0.0.");
        return 0.0;
    }
    return shearModulus;
}

/// The material of @p type the element names: its group as the attribute "group" gives it, with
/// OpenRocket's warning for one that names none, and then the material as
/// Databases.findMaterial() finds it in @p materials or makes it.
[[nodiscard]] Material materialOf(const MaterialStorage& materials, Material::Type type,
                                  std::string_view name, Specification specification,
                                  const Setter::Attributes& attributes, WarningSet& warnings)
{
    // Check for material group
    if (const std::optional<std::string_view> group =
            DocumentConfig::attribute(attributes, "group"))
    {
        specification.group = materials.materialGroupFromLegacyDatabaseString(
            *group, type, name, specification.density);
        if (!specification.group.has_value())
        {
            warnings.add("Illegal material group specified, ignoring.");
        }
    }

    if (specification.shearModulus.has_value())
    {
        return materials.findMaterial(type, name, specification.density,
                                      *specification.shearModulus, specification.group);
    }
    return materials.findMaterial(type, name, specification.density, specification.group);
}

}  // namespace

MaterialSetter::MaterialSetter(SetFunction set, Material::Type type)
  : m_set(std::move(set)), m_type(type)
{
    QTROCKET_ASSERT(m_set != nullptr && m_type != Material::Type::CUSTOM);
}

Result<void> MaterialSetter::set(RocketComponent& component, std::string_view value,
                                 const Attributes& attributes, WarningSet& warnings,
                                 const DocumentLoadingContext& context) const
{
    // Check name != ""
    const std::string_view name = Strings::trim(value);
    if (name.empty())
    {
        warnings.add(kIllegalSpecification);
        return {};
    }

    // Parse density. Java stores a NaN and an infinity it reads here; see the class comment.
    Specification               specification;
    const std::optional<double> density = finiteAttribute(attributes, "density");
    if (!density.has_value())
    {
        warnings.add(kIllegalSpecification);
        return {};
    }
    specification.density      = *density;
    specification.shearModulus = shearModulusOf(attributes, warnings);

    // Check type if specified
    const std::optional<std::string_view> type = DocumentConfig::attribute(attributes, "type");
    if (type.has_value() && *type != Strings::toLower(toString(m_type)))
    {
        warnings.add("Illegal material type specified, ignoring.");
        return {};
    }

    if (const MaterialStorage* const application = context.getApplicationMaterials())
    {
        m_set(component,
              materialOf(*application, m_type, name, specification, attributes, warnings));
        return {};
    }
    // An application without materials: nothing is found, so every material is the document's.
    const MaterialStorage none;
    m_set(component, materialOf(none, m_type, name, specification, attributes, warnings));
    return {};
}

}  // namespace QtRocket
