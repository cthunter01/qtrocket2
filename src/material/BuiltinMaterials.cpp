#include "QtRocket/material/BuiltinMaterials.h"

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/material/MaterialStorage.h"

namespace QtRocket
{

namespace
{

using Type = Material::Type;

[[nodiscard]] constexpr BuiltinMaterial row(Type type, std::string_view name, double density,
                                            double inPlaneShearModulus, MaterialGroup group)
{
    return {.type                = type,
            .name                = name,
            .density             = density,
            .inPlaneShearModulus = inPlaneShearModulus,
            .group               = group};
}

/// The static initialiser of Databases.java, row for row. Shear modulus (G) values are based on
/// standard engineering data (MatWeb, USDA Wood Handbook, lookpolymers), in pascals; where only
/// Young's modulus E and Poisson's ratio v are known, G = E / (2 * (1 + v)).
constexpr std::array<BuiltinMaterial, 82> kBuiltinMaterials{{
    // Plastics
    row(Type::BULK, "Acrylic", 1190, 1.7e9, MaterialGroup::PLASTICS),
    row(Type::BULK, "Delrin", 1420, 0.946e9, MaterialGroup::PLASTICS),
    row(Type::BULK, "Nylon", 1150, 1.15e9, MaterialGroup::FIBERS),  // Nylon 6/6
    row(Type::BULK, "Polycarbonate (Lexan)", 1200, 0.786e9, MaterialGroup::PLASTICS),
    row(Type::BULK, "Polystyrene", 1050, 1.23e9, MaterialGroup::PLASTICS),
    row(Type::BULK, "PVC", 1390, 2.28e9, MaterialGroup::PLASTICS),

    // 3D Printing Plastics (Assumed 100% infill isotropic approximation)
    row(Type::BULK, "PLA - 100% infill", 1250, 2.4e9, MaterialGroup::PLASTICS),
    row(Type::BULK, "PETG - 100% infill", 1250, 0.8e9, MaterialGroup::PLASTICS),
    row(Type::BULK, "ABS - 100% infill", 1050, 0.875e9, MaterialGroup::PLASTICS),
    row(Type::BULK, "ASA - 100% infill", 1050, 0.8e9, MaterialGroup::PLASTICS),

    // Metals
    row(Type::BULK, "Aluminum", 2700, 26.0e9, MaterialGroup::METALS),  // 6061-T6
    row(Type::BULK, "Brass", 8600, 38.9e9, MaterialGroup::METALS),
    row(Type::BULK, "Steel", 7850, 79.7e9, MaterialGroup::METALS),
    row(Type::BULK, "Titanium", 4500, 43.0e9, MaterialGroup::METALS),

    // Woods (Values are approximate G_LT / In-plane shear)
    row(Type::BULK, "Balsa", 170, 0.23e9, MaterialGroup::WOODS),
    row(Type::BULK, "Basswood", 500, 0.331e9, MaterialGroup::WOODS),
    row(Type::BULK, "Birch", 670, 0.70e9, MaterialGroup::WOODS),
    row(Type::BULK, "Cork", 240, 0.01e9, MaterialGroup::WOODS),
    row(Type::BULK, "Maple", 755, 0.71e9, MaterialGroup::WOODS),
    row(Type::BULK, "Pine", 530, 0.71e9, MaterialGroup::WOODS),
    row(Type::BULK, "Plywood (birch)", 630, 0.613e9, MaterialGroup::WOODS),
    row(Type::BULK, "Spruce", 450, 1.1e9, MaterialGroup::WOODS),

    // Composites
    row(Type::BULK, "Carbon fiber", 1780, 4.14e9, MaterialGroup::COMPOSITES),  // Quasi-isotropic
    row(Type::BULK, "Fiberglass", 1850, 4.14e9, MaterialGroup::COMPOSITES),
    row(Type::BULK, "Kraft phenolic", 950, 1.78e9, MaterialGroup::COMPOSITES),  // Paper phenolic
    row(Type::BULK, "Blue tube", 1300, 0, MaterialGroup::COMPOSITES),
    row(Type::BULK, "Quantum tubing", 1050, 0, MaterialGroup::PLASTICS),

    // Paper/Foams (Low shear modulus, often negligible, but non-zero values provided where
    // applicable)
    row(Type::BULK, "Cardboard", 680, 0.4e9, MaterialGroup::PAPER),  // Solid paperboard
    row(Type::BULK, "Paper (office)", 820, 0.0, MaterialGroup::PAPER),
    row(Type::BULK, "Depron (XPS)", 40, 0.0027e9, MaterialGroup::FOAMS),
    row(Type::BULK, "Styrofoam (generic EPS)", 20, 0.002e9, MaterialGroup::FOAMS),
    row(Type::BULK, "Styrofoam \"Blue foam\" (XPS)", 32, 0.0028e9, MaterialGroup::FOAMS),

    // Surface Materials (Films/Fabrics - Shear Modulus generally not applicable for thin flexible
    // membranes in this model)
    row(Type::SURFACE, "Ripstop nylon", 0.067, 0.0, MaterialGroup::FABRICS),
    row(Type::SURFACE, "Mylar", 0.021, 0.0, MaterialGroup::PLASTICS),
    row(Type::SURFACE, "Polyethylene (thin)", 0.015, 0.0, MaterialGroup::PLASTICS),
    row(Type::SURFACE, "Polyethylene (heavy)", 0.040, 0.0, MaterialGroup::PLASTICS),
    row(Type::SURFACE, "Silk", 0.060, 0.0, MaterialGroup::FABRICS),
    row(Type::SURFACE, "Paper (office)", 0.080, 0.0, MaterialGroup::PAPER),
    row(Type::SURFACE, "Cellophane", 0.018, 0.0, MaterialGroup::PLASTICS),
    row(Type::SURFACE, "Cr\xC3\xAApe paper", 0.025, 0.0, MaterialGroup::PAPER),  // "Crêpe paper"

    // Line Materials (1D tension members - Shear Modulus N/A)
    row(Type::LINE, "Thread (heavy-duty)", 0.0003, 0.0, MaterialGroup::OTHER),
    row(Type::LINE, "Elastic cord (round 2 mm, 1/16 in)", 0.0018, 0.0, MaterialGroup::ELASTICS),
    row(Type::LINE, "Elastic cord (flat 6 mm, 1/4 in)", 0.0043, 0.0, MaterialGroup::ELASTICS),
    row(Type::LINE, "Elastic cord (flat 12 mm, 1/2 in)", 0.008, 0.0, MaterialGroup::ELASTICS),
    row(Type::LINE, "Elastic cord (flat 19 mm, 3/4 in)", 0.0012, 0.0, MaterialGroup::ELASTICS),
    row(Type::LINE, "Elastic cord (flat 25 mm, 1 in)", 0.0016, 0.0, MaterialGroup::ELASTICS),
    row(Type::LINE, "Braided nylon (2 mm, 1/16 in)", 0.001, 0.0, MaterialGroup::NYLONS),
    row(Type::LINE, "Braided nylon (3 mm, 1/8 in)", 0.0035, 0.0, MaterialGroup::NYLONS),
    row(Type::LINE, "Tubular nylon (11 mm, 7/16 in)", 0.013, 0.0, MaterialGroup::NYLONS),
    row(Type::LINE, "Tubular nylon (14 mm, 9/16 in)", 0.016, 0.0, MaterialGroup::NYLONS),
    row(Type::LINE, "Tubular nylon (25 mm, 1 in)", 0.029, 0.0, MaterialGroup::NYLONS),
    row(Type::LINE, "Kevlar thread 138  (0.4 mm, 1/64 in)", 0.00014808, 0.0,
        MaterialGroup::KEVLARS),
    row(Type::LINE, "Kevlar thread 207  (0.5 mm, 1/64 in)", 0.00023622, 0.0,
        MaterialGroup::KEVLARS),
    row(Type::LINE, "Kevlar thread 346  (0.7 mm, 1/32 in)", 0.00047243, 0.0,
        MaterialGroup::KEVLARS),
    row(Type::LINE, "Kevlar thread 415  (0.8 mm, 1/32 in)", 0.00055117, 0.0,
        MaterialGroup::KEVLARS),
    row(Type::LINE, "Kevlar thread 800 (1.1 mm, 3/64 in)", 0.00099211, 0.0, MaterialGroup::KEVLARS),
    row(Type::LINE, "Kevlar 12-strand (3.2 mm, 1/8 in)", 0.00967306, 0.0, MaterialGroup::KEVLARS),
    row(Type::LINE, "Kevlar 12-strand (4.8 mm, 3/16 in)", 0.01785797, 0.0, MaterialGroup::KEVLARS),
    row(Type::LINE, "Kevlar 12-strand (6.4 mm, 1/4 in)", 0.02976328, 0.0, MaterialGroup::KEVLARS),
    row(Type::LINE, "Kevlar 12-strand (7.9 mm, 5/16 in)", 0.04464491, 0.0, MaterialGroup::KEVLARS),
    row(Type::LINE, "Kevlar 12-strand (10 mm, 3/8 in)", 0.05952655, 0.0, MaterialGroup::KEVLARS),
    row(Type::LINE, "Kevlar 12-strand (11 mm, 7/16 in)", 0.07440819, 0.0, MaterialGroup::KEVLARS),
    row(Type::LINE, "Kevlar 12-strand (13 mm, 1/2 in)", 0.11607678, 0.0, MaterialGroup::KEVLARS),
    row(Type::LINE, "Kevlar 12-strand (14 mm, 9/16 in)", 0.20834293, 0.0, MaterialGroup::KEVLARS),
    row(Type::LINE, "Kevlar 12-strand (16 mm, 5/8 in)", 0.28721562, 0.0, MaterialGroup::KEVLARS),
    row(Type::LINE, "Kevlar 12-strand (19 mm, 3/4 in)", 0.3497185, 0.0, MaterialGroup::KEVLARS),
    row(Type::LINE, "Kevlar 12-strand (25 mm, 1 in)", 0.45686629, 0.0, MaterialGroup::KEVLARS),
    row(Type::LINE, "Nylon flat webbing md. (10 mm, 3/8 in)", 0.00951444, 0.0,
        MaterialGroup::NYLONS),
    row(Type::LINE, "Nylon flat webbing md. (13 mm, 1/2  in)", 0.01334208, 0.0,
        MaterialGroup::NYLONS),
    row(Type::LINE, "Nylon flat webbing md. (16 mm, 5/8 in)", 0.01618548, 0.0,
        MaterialGroup::NYLONS),
    row(Type::LINE, "Nylon flat webbing lg. (14 mm, 9/16 in)", 0.02723097, 0.0,
        MaterialGroup::NYLONS),
    row(Type::LINE, "Nylon flat webbing lg. (25 mm, 1 in)", 0.03969816, 0.0, MaterialGroup::NYLONS),
    row(Type::LINE, "Paraline small IIIA (6.4 mm, 1.4 in)", 0.00371829, 0.0, MaterialGroup::OTHER),
    row(Type::LINE, "Elastic rubber band (flat 3.2 mm, 1/8 in)", 0.00297638, 0.0,
        MaterialGroup::ELASTICS),
    row(Type::LINE, "Elastic rubber band (flat 6.4 mm, 1/4 in)", 0.00613107, 0.0,
        MaterialGroup::ELASTICS),
    row(Type::LINE, "Elastic braided cord (flat 3.2 mm, 1/8 in)", 0.00106, 0.0,
        MaterialGroup::ELASTICS),
    row(Type::LINE, "Elastic braided cord (flat 4 mm, 5/32 in)", 0.002, 0.0,
        MaterialGroup::ELASTICS),
    row(Type::LINE, "Elastic braided cord (flat 6.4 mm, 1/4 in)", 0.00254, 0.0,
        MaterialGroup::ELASTICS),
    row(Type::LINE, "Elastic braided cord (round 2 mm, 1/16 in)", 0.0035, 0.0,
        MaterialGroup::ELASTICS),
    row(Type::LINE, "Elastic braided cord (round 2.5 mm, 3/32 in)", 0.0038, 0.0,
        MaterialGroup::ELASTICS),
    row(Type::LINE, "Elastic braided cord (flat 10 mm, 3/8 in)", 0.00381, 0.0,
        MaterialGroup::ELASTICS),
    row(Type::LINE, "Elastic braided cord (flat 13 mm, 1/2 in)", 0.00551172, 0.0,
        MaterialGroup::ELASTICS),
}};

}  // namespace

std::span<const BuiltinMaterial> builtinMaterials() noexcept
{
    return kBuiltinMaterials;
}

Material toMaterial(const BuiltinMaterial& row)
{
    return Material::newMaterial(row.type, std::string(row.name), row.density,
                                 row.inPlaneShearModulus, row.group, false);
}

std::size_t addBuiltinMaterials(MaterialStorage& storage)
{
    std::size_t added = 0;
    for (const BuiltinMaterial& row : kBuiltinMaterials)
    {
        if (storage.addMaterial(toMaterial(row)))
        {
            added++;
        }
    }
    return added;
}

}  // namespace QtRocket
