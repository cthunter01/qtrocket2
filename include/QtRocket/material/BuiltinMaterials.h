#pragma once

#include <cstddef>
#include <span>
#include <string_view>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"

namespace QtRocket
{

class MaterialStorage;

/// One row of OpenRocket's built-in material table (the static initialiser of Databases.java).
struct BuiltinMaterial
{
    Material::Type   type;
    std::string_view name;
    double           density;              ///< kg/m^3, kg/m^2 or kg/m by type
    double           inPlaneShearModulus;  ///< Pa; 0 where OpenRocket gives none
    MaterialGroup    group;
};

/// The built-in materials in Databases.java's order: 32 bulk, 8 surface and 42 line materials.
/// The names are the English message texts, which are the table's own names.
[[nodiscard]] std::span<const BuiltinMaterial> builtinMaterials() noexcept;

/// The material of a row: not user-defined, not a document material.
[[nodiscard]] Material toMaterial(const BuiltinMaterial& row);

/// Databases' static initialiser without the user materials and the preference listener: adds
/// every built-in material to the database of its type. Returns the number added (82 into an
/// empty storage; a material already present is not added twice).
std::size_t addBuiltinMaterials(MaterialStorage& storage);

}  // namespace QtRocket
