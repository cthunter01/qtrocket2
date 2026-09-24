#include "QtRocket/material/Material.h"

#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Error.h"

namespace
{

using QtRocket::addBuiltinMaterials;
using QtRocket::displayKey;
using QtRocket::displayName;
using QtRocket::ErrorCode;
using QtRocket::GeneralUnit;
using QtRocket::Material;
using QtRocket::MaterialGroup;
using QtRocket::MaterialStorage;
using QtRocket::materialTypeFromString;
using QtRocket::Result;
using QtRocket::UnitGroup;
using QtRocket::unitGroupId;
using QtRocket::UnitGroupId;
using Type = Material::Type;

constexpr double kEpsilon = 1e-6;
constexpr double kNaN     = std::numeric_limits<double>::quiet_NaN();

// ---- Ported from MaterialTest.java ----

TEST(Material, Material)
{
    const std::string name    = "Test Material";
    const Type        type    = Type::BULK;
    const double      density = 1.0;

    // Test user-defined material
    Material m = Material::newMaterial(type, name, density, true);
    EXPECT_EQ(name, m.getName());
    EXPECT_EQ(type, m.getType());
    EXPECT_NEAR(density, m.getDensity(), kEpsilon);
    EXPECT_TRUE(m.isUserDefined());
    EXPECT_FALSE(m.isDocumentMaterial());
    EXPECT_EQ(MaterialGroup::CUSTOM, m.getGroup());

    // Test non-user-defined material
    m = Material::newMaterial(type, name, density, false);
    EXPECT_FALSE(m.isUserDefined());
    EXPECT_FALSE(m.isDocumentMaterial());
    EXPECT_EQ(MaterialGroup::OTHER, m.getGroup());

    // Test defined material group
    m = Material::newMaterial(type, name, density, MaterialGroup::WOODS, false);
    EXPECT_FALSE(m.isUserDefined());
    EXPECT_FALSE(m.isDocumentMaterial());
    EXPECT_EQ(MaterialGroup::WOODS, m.getGroup());

    // Test storable string
    const std::string storable = m.toStorableString();
    EXPECT_EQ("BULK|Test Material|1.0|0.0|Woods", storable);
}

// testDocumentDatabase and testLoadFromPreset assign materials to a BodyTube of a document, which
// the rocket model will bring; the material side of them is kept here with a MaterialStorage in
// the role of the document's material database.
TEST(Material, DocumentDatabase)
{
    MaterialStorage document;
    EXPECT_EQ(document.totalMaterialCount(), 0U);

    // Create a material
    std::string    name    = "Tube Material";
    const Type     type    = Type::BULK;
    double         density = 314;
    const Material m       = Material::newMaterial(type, name, density, true, true);

    // Check document material database
    EXPECT_EQ(document.totalMaterialCount(), 0U);

    // Assign material to body tube
    EXPECT_TRUE(document.addMaterial(m));
    EXPECT_EQ(document.totalMaterialCount(), 1U);
    EXPECT_EQ(name, m.getName());
    EXPECT_EQ(type, m.getType());
    EXPECT_NEAR(density, m.getDensity(), kEpsilon);
    EXPECT_TRUE(m.isUserDefined());
    EXPECT_TRUE(m.isDocumentMaterial());
    EXPECT_EQ(MaterialGroup::CUSTOM, m.getGroup());

    // Assign a new material
    name              = "New Material";
    density           = 271;
    const Material m2 = Material::newMaterial(type, name, density, true, true);
    EXPECT_TRUE(document.addMaterial(m2));
    EXPECT_EQ(document.totalMaterialCount(), 2U);
    EXPECT_EQ(name, m2.getName());
    EXPECT_EQ(type, m2.getType());
    EXPECT_NEAR(density, m2.getDensity(), kEpsilon);
    EXPECT_TRUE(m2.isUserDefined());
    EXPECT_TRUE(m2.isDocumentMaterial());
    EXPECT_EQ(MaterialGroup::CUSTOM, m2.getGroup());

    // Remove a material
    EXPECT_TRUE(document.removeMaterial(m));
    EXPECT_EQ(document.totalMaterialCount(), 1U);
}

TEST(Material, LoadFromPreset)
{
    const Material preset = Material::newMaterial(Type::BULK, "Preset Material", 980, true, true);
    EXPECT_EQ("Preset Material", preset.getName());
    EXPECT_EQ(Type::BULK, preset.getType());
    EXPECT_NEAR(980, preset.getDensity(), kEpsilon);
    EXPECT_TRUE(preset.isUserDefined());
    EXPECT_TRUE(preset.isDocumentMaterial());
    EXPECT_EQ(MaterialGroup::CUSTOM, preset.getGroup());
    EXPECT_EQ(preset.toStorableString(), "BULK|Preset Material|980.0|0.0|Custom");
}

// ---- QtRocket additions ----

TEST(Material, FactoriesAndTheirDefaults)
{
    const Material full =
        Material::newMaterial(Type::LINE, "Full", 0.01, 2.5e9, MaterialGroup::NYLONS, true, true);
    EXPECT_EQ(full.getType(), Type::LINE);
    EXPECT_EQ(full.getName(), "Full");
    EXPECT_EQ(full.getDensity(), 0.01);
    EXPECT_EQ(full.getInPlaneShearModulus(), 2.5e9);
    EXPECT_EQ(full.getGroup(), MaterialGroup::NYLONS);
    EXPECT_TRUE(full.isUserDefined());
    EXPECT_TRUE(full.isDocumentMaterial());

    const Material noShear =
        Material::newMaterial(Type::SURFACE, "x", 0.1, MaterialGroup::FABRICS, false, true);
    EXPECT_EQ(noShear.getInPlaneShearModulus(), 0.0);
    EXPECT_EQ(noShear.getGroup(), MaterialGroup::FABRICS);
    EXPECT_FALSE(noShear.isUserDefined());
    EXPECT_TRUE(noShear.isDocumentMaterial());

    const Material noDocument =
        Material::newMaterial(Type::BULK, "x", 1.0, MaterialGroup::METALS, true);
    EXPECT_EQ(noDocument.getInPlaneShearModulus(), 0.0);
    EXPECT_FALSE(noDocument.isDocumentMaterial());
    EXPECT_EQ(noDocument.getGroup(), MaterialGroup::METALS);

    const Material shearNoDocument =
        Material::newMaterial(Type::BULK, "x", 1.0, 3.0e9, MaterialGroup::METALS, false);
    EXPECT_EQ(shearNoDocument.getInPlaneShearModulus(), 3.0e9);
    EXPECT_FALSE(shearNoDocument.isDocumentMaterial());

    const Material shearNoGroup = Material::newMaterial(Type::BULK, "x", 1.0, 3.0e9, false, true);
    EXPECT_EQ(shearNoGroup.getInPlaneShearModulus(), 3.0e9);
    EXPECT_EQ(shearNoGroup.getGroup(), MaterialGroup::OTHER);
    EXPECT_TRUE(shearNoGroup.isDocumentMaterial());
    EXPECT_EQ(Material::newMaterial(Type::BULK, "x", 1.0, 3.0e9, true, false).getGroup(),
              MaterialGroup::CUSTOM);

    const Material noGroup = Material::newMaterial(Type::BULK, "x", 1.0, true, true);
    EXPECT_EQ(noGroup.getGroup(), MaterialGroup::CUSTOM);
    EXPECT_EQ(noGroup.getInPlaneShearModulus(), 0.0);
    EXPECT_TRUE(noGroup.isDocumentMaterial());

    const Material minimal = Material::newMaterial(Type::CUSTOM, "x", 1.0, false);
    EXPECT_EQ(minimal.getType(), Type::CUSTOM);
    EXPECT_EQ(minimal.getGroup(), MaterialGroup::OTHER);
    EXPECT_FALSE(minimal.isDocumentMaterial());

    // The constructor with an explicit group keeps it whatever the user-defined flag.
    const Material constructed(Type::BULK, "x", 1.0, 0.0, MaterialGroup::WOODS, true, false);
    EXPECT_EQ(constructed.getGroup(), MaterialGroup::WOODS);
    const Material userCustom(Type::BULK, "x", 1.0, 0.0, MaterialGroup::CUSTOM, false, false);
    EXPECT_EQ(userCustom.getGroup(), MaterialGroup::CUSTOM);
    EXPECT_FALSE(userCustom.isUserDefined());
}

TEST(Material, TypeHelpers)
{
    EXPECT_EQ(QtRocket::toString(Type::BULK), "BULK");
    EXPECT_EQ(QtRocket::toString(Type::SURFACE), "SURFACE");
    EXPECT_EQ(QtRocket::toString(Type::LINE), "LINE");
    EXPECT_EQ(QtRocket::toString(Type::CUSTOM), "CUSTOM");
    for (const Type type : Material::kAllTypes)
    {
        EXPECT_EQ(materialTypeFromString(QtRocket::toString(type)), type);
    }
    EXPECT_EQ(materialTypeFromString("bulk"), std::nullopt);
    EXPECT_EQ(materialTypeFromString("Bulk"), std::nullopt);
    EXPECT_EQ(materialTypeFromString(""), std::nullopt);
    EXPECT_EQ(materialTypeFromString("BULK "), std::nullopt);

    EXPECT_EQ(displayName(Type::BULK), "Bulk");
    EXPECT_EQ(displayName(Type::SURFACE), "Surface");
    EXPECT_EQ(displayName(Type::LINE), "Line");
    EXPECT_EQ(displayName(Type::CUSTOM), "Custom");
    EXPECT_EQ(displayKey(Type::BULK), "Databases.materials.types.Bulk");
    EXPECT_EQ(displayKey(Type::LINE), "Databases.materials.types.Line");

    EXPECT_EQ(unitGroupId(Type::BULK), UnitGroupId::DENSITY_BULK);
    EXPECT_EQ(unitGroupId(Type::CUSTOM), UnitGroupId::DENSITY_BULK);
    EXPECT_EQ(unitGroupId(Type::SURFACE), UnitGroupId::DENSITY_SURFACE);
    EXPECT_EQ(unitGroupId(Type::LINE), UnitGroupId::DENSITY_LINE);
}

TEST(Material, NamesWithDensity)
{
    UnitGroup::resetDefaultUnits();
    const Material aluminum =
        Material::newMaterial(Type::BULK, "Aluminum", 2700, 26.0e9, MaterialGroup::METALS, false);
    EXPECT_EQ(aluminum.getName(), "Aluminum");
    EXPECT_EQ(aluminum.toString(), "Aluminum (2.7 g/cm³)");
    EXPECT_EQ(aluminum.getName(GeneralUnit(1, "kg/m³")), "Aluminum (2700 kg/m³)");
    EXPECT_EQ(aluminum.getName(GeneralUnit(16.0184634, "lb/ft³")), "Aluminum (169 lb/ft³)");
    const Material ripstop =
        Material::newMaterial(Type::SURFACE, "Ripstop nylon", 0.067, MaterialGroup::FABRICS, false);
    EXPECT_EQ(ripstop.toString(), "Ripstop nylon (67 g/m²)");
    const Material cord = Material::newMaterial(Type::LINE, "Elastic cord (round 2 mm, 1/16 in)",
                                                0.0018, MaterialGroup::ELASTICS, false);
    EXPECT_EQ(cord.toString(), "Elastic cord (round 2 mm, 1/16 in) (1.8 g/m)");
    const Material odd = Material::newMaterial(Type::CUSTOM, "Odd", 1500, true);
    EXPECT_EQ(odd.toString(), "Odd (1.5 g/cm³)");  // CUSTOM uses the bulk density units
    EXPECT_EQ(Material::newMaterial(Type::BULK, "None", kNaN, true).toString(), "None (N/A)");

    UnitGroup::setDefaultImperialUnits();
    EXPECT_EQ(aluminum.toString(), "Aluminum (1.56 oz/in³)");
    UnitGroup::resetDefaultUnits();
}

TEST(Material, EqualsIgnoresTheFlagsAndToleratesDensity)
{
    const Material a =
        Material::newMaterial(Type::BULK, "Balsa", 170, 0.23e9, MaterialGroup::WOODS, false, false);
    const Material sameFlagsDiffer =
        Material::newMaterial(Type::BULK, "Balsa", 170, 0.23e9, MaterialGroup::WOODS, true, true);
    const Material withinTolerance = Material::newMaterial(
        Type::BULK, "Balsa", 170 * (1 + 1e-10), 0.23e9 * (1 - 1e-10), MaterialGroup::WOODS, false);
    const Material otherDensity =
        Material::newMaterial(Type::BULK, "Balsa", 171, 0.23e9, MaterialGroup::WOODS, false);
    const Material otherShear =
        Material::newMaterial(Type::BULK, "Balsa", 170, 0.24e9, MaterialGroup::WOODS, false);
    const Material otherGroup =
        Material::newMaterial(Type::BULK, "Balsa", 170, 0.23e9, MaterialGroup::OTHER, false);
    const Material otherName =
        Material::newMaterial(Type::BULK, "balsa", 170, 0.23e9, MaterialGroup::WOODS, false);
    const Material otherType =
        Material::newMaterial(Type::SURFACE, "Balsa", 170, 0.23e9, MaterialGroup::WOODS, false);

    EXPECT_TRUE(a == a);
    EXPECT_TRUE(a == sameFlagsDiffer);
    EXPECT_TRUE(a == withinTolerance);
    EXPECT_FALSE(a == otherDensity);
    EXPECT_FALSE(a == otherShear);
    EXPECT_FALSE(a == otherGroup);
    EXPECT_FALSE(a == otherName);
    EXPECT_FALSE(a == otherType);
    EXPECT_TRUE(a != otherType);
    // NaN densities never compare equal (MathUtil.equals).
    const Material nan = Material::newMaterial(Type::BULK, "x", kNaN, true);
    EXPECT_FALSE(nan == nan);
}

TEST(Material, CompareToOrdersByNameThenDensity)
{
    const Material a1 = Material::newMaterial(Type::BULK, "a", 1.0, true);
    EXPECT_EQ(a1.compareTo(a1), 0);
    EXPECT_EQ(a1.compareTo(Material::newMaterial(Type::BULK, "a", 1.0005, true)), 0);
    EXPECT_EQ(a1.compareTo(Material::newMaterial(Type::BULK, "a", 1.0004, true)), 0);
    EXPECT_LT(a1.compareTo(Material::newMaterial(Type::BULK, "a", 1.002, true)), 0);
    EXPECT_GT(Material::newMaterial(Type::BULK, "a", 1.002, true).compareTo(a1), 0);
    EXPECT_EQ(Material::newMaterial(Type::BULK, "a", 1.002, true).compareTo(a1), 2);  // Java: 2
    EXPECT_EQ(a1.compareTo(Material::newMaterial(Type::BULK, "a", 1.002, true)), -2);
    // The name decides first, as Java's String.compareTo (upper case before lower case).
    EXPECT_GT(Material::newMaterial(Type::BULK, "Balsa", 1, true)
                  .compareTo(Material::newMaterial(Type::BULK, "Aluminum", 2, true)),
              0);
    EXPECT_GT(a1.compareTo(Material::newMaterial(Type::BULK, "B", 1.0, true)), 0);
    EXPECT_LT(Material::newMaterial(Type::BULK, "Cra", 1.0, true)
                  .compareTo(Material::newMaterial(Type::BULK, "Crêpe", 1.0, true)),
              0);
    // The type, group and flags do not take part.
    EXPECT_EQ(a1.compareTo(
                  Material::newMaterial(Type::LINE, "a", 1.0, MaterialGroup::NYLONS, false, true)),
              0);
    // A huge difference saturates as Java's (int) cast; NaN gives 0.
    EXPECT_EQ(Material::newMaterial(Type::BULK, "a", 1e300, true).compareTo(a1), 2147483647);
    EXPECT_EQ(a1.compareTo(Material::newMaterial(Type::BULK, "a", 1e300, true)), -2147483648);
    EXPECT_EQ(Material::newMaterial(Type::BULK, "a", kNaN, true).compareTo(a1), 0);
}

TEST(Material, HashCodeIsJavas)
{
    // Pinned on JDK 17: name.hashCode() + (int)(density * 1000) + (int)(shear * 1e-9).
    const Material aluminum =
        Material::newMaterial(Type::BULK, "Aluminum", 2700, 26.0e9, MaterialGroup::METALS, false);
    EXPECT_EQ(aluminum.hashCode(), 2135883802);
    EXPECT_EQ(Material::newMaterial(Type::BULK, "Test Material", 1.0, true).hashCode(), 2119408861);
    EXPECT_EQ(Material::newMaterial(Type::SURFACE, "Crêpe paper", 0.025, true).hashCode(),
              -714553611);
    EXPECT_EQ(
        Material::newMaterial(Type::LINE, "Kevlar thread 138  (0.4 mm, 1/64 in)", 0.00014808, true)
            .hashCode(),
        -1700022585);
    EXPECT_EQ(Material::newMaterial(Type::BULK, "Steel", 7850, 79.7e9, true, false).hashCode(),
              88058378);  // wraps around
    EXPECT_EQ(Material::newMaterial(Type::BULK, "Aluminum", -2700, 26.0e9, true, false).hashCode(),
              2130483802);
    EXPECT_EQ(Material::newMaterial(Type::BULK, "Aluminum", 1e300, true).hashCode(), -14299873);
    EXPECT_EQ(Material::newMaterial(Type::BULK, "Aluminum", kNaN, true).hashCode(), 2133183776);
    EXPECT_EQ(Material::newMaterial(Type::BULK, "", 0.0, true).hashCode(), 0);
    EXPECT_EQ(Material::newMaterial(Type::BULK, "a|b", 0.0, true).hashCode(), 97159);
    EXPECT_EQ(Material::newMaterial(Type::BULK, "🚀 rocket", 0.0, true).hashCode(), -1391342671);
    // The flags, type and group do not take part; std::hash follows hashCode.
    EXPECT_EQ(Material::newMaterial(Type::LINE, "Aluminum", 2700, 26.0e9, MaterialGroup::WOODS,
                                    true, true)
                  .hashCode(),
              aluminum.hashCode());
    EXPECT_EQ(std::hash<Material>{}(aluminum),
              static_cast<std::size_t>(static_cast<unsigned int>(2135883802)));
}

TEST(Material, StorableStringFormat)
{
    EXPECT_EQ(Material::newMaterial(Type::LINE, "Elastic cord (round 2 mm, 1/16 in)", 0.0018,
                                    MaterialGroup::ELASTICS, false)
                  .toStorableString(),
              "LINE|Elastic cord (round 2 mm, 1/16 in)|0.0018|0.0|Elastics");
    EXPECT_EQ(
        Material::newMaterial(Type::BULK, "Aluminum", 2700, 26.0e9, MaterialGroup::METALS, false)
            .toStorableString(),
        "BULK|Aluminum|2700.0|2.6E10|Metals");
    EXPECT_EQ(
        Material::newMaterial(Type::BULK, "Delrin", 1420, 0.946e9, MaterialGroup::PLASTICS, false)
            .toStorableString(),
        "BULK|Delrin|1420.0|9.46E8|Plastics");
    EXPECT_EQ(Material::newMaterial(Type::LINE, "Kevlar thread 138  (0.4 mm, 1/64 in)", 0.00014808,
                                    MaterialGroup::KEVLARS, false)
                  .toStorableString(),
              "LINE|Kevlar thread 138  (0.4 mm, 1/64 in)|1.4808E-4|0.0|Kevlars");
    EXPECT_EQ(
        Material::newMaterial(Type::SURFACE, "Paper (office)", 0.080, MaterialGroup::PAPER, false)
            .toStorableString(),
        "SURFACE|Paper (office)|0.08|0.0|PaperProducts");
    EXPECT_EQ(Material::newMaterial(Type::BULK, "Foam", 0.0027e9, 1.0 / 3.0, true, true)
                  .toStorableString(),
              "BULK|Foam|2700000.0|0.3333333333333333|Custom");
    EXPECT_EQ(Material::newMaterial(Type::CUSTOM, "Odd", 1.0e-5, false).toStorableString(),
              "CUSTOM|Odd|1.0E-5|0.0|Other");
    // A '|' in the name would break the format: it becomes a space.
    EXPECT_EQ(Material::newMaterial(Type::BULK, "a|b||c", 1.0, true).toStorableString(),
              "BULK|a b  c|1.0|0.0|Custom");
    EXPECT_EQ(Material::newMaterial(Type::BULK, "", 0.0, true).toStorableString(),
              "BULK||0.0|0.0|Custom");
    EXPECT_EQ(Material::newMaterial(Type::BULK, "x", kNaN, true).toStorableString(),
              "BULK|x|NaN|0.0|Custom");
}

TEST(Material, FromStorableStringParsesEveryFormat)
{
    // The current five-field form round-trips.
    const Material         original = Material::newMaterial(Type::LINE, "Cord", 0.0018, 1.5e9,
                                                            MaterialGroup::ELASTICS, true, true);
    const Result<Material> parsed = Material::fromStorableString(original.toStorableString(), true);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_TRUE(*parsed == original);
    EXPECT_EQ(parsed->getType(), Type::LINE);
    EXPECT_EQ(parsed->getName(), "Cord");
    EXPECT_EQ(parsed->getDensity(), 0.0018);
    EXPECT_EQ(parsed->getInPlaneShearModulus(), 1.5e9);
    EXPECT_EQ(parsed->getGroup(), MaterialGroup::ELASTICS);
    EXPECT_TRUE(parsed->isUserDefined());
    EXPECT_FALSE(parsed->isDocumentMaterial());  // never set by the parser
    EXPECT_FALSE(
        Material::fromStorableString("BULK|Aluminum|2700.0|2.6E10|Metals", false)->isUserDefined());

    // The pre-shear-modulus four-field form: the fourth field is the group.
    const Result<Material> old = Material::fromStorableString("BULK|Balsa|170.0|Woods", false);
    ASSERT_TRUE(old.has_value());
    EXPECT_EQ(old->getInPlaneShearModulus(), 0.0);
    EXPECT_EQ(old->getGroup(), MaterialGroup::WOODS);
    EXPECT_EQ(old->getDensity(), 170.0);

    // The three-field form: no group, so CUSTOM for a user material and OTHER otherwise.
    EXPECT_EQ(Material::fromStorableString("BULK|Balsa|170", true)->getGroup(),
              MaterialGroup::CUSTOM);
    EXPECT_EQ(Material::fromStorableString("BULK|Balsa|170", false)->getGroup(),
              MaterialGroup::OTHER);
    EXPECT_EQ(Material::fromStorableString("SURFACE|Silk|6e-2", false)->getDensity(), 0.06);

    // Four fields with a numeric fourth: a shear modulus and no group.
    const Result<Material> shearOnly = Material::fromStorableString("BULK|Balsa|170|2.3E8", false);
    ASSERT_TRUE(shearOnly.has_value());
    EXPECT_EQ(shearOnly->getInPlaneShearModulus(), 2.3e8);
    EXPECT_EQ(shearOnly->getGroup(), MaterialGroup::OTHER);

    // An unknown group is dropped, not an error (OpenRocket logs it).
    EXPECT_EQ(Material::fromStorableString("BULK|Balsa|170|0.0|Nonsense", true)->getGroup(),
              MaterialGroup::CUSTOM);
    EXPECT_EQ(Material::fromStorableString("BULK|Balsa|170|Nonsense", false)->getGroup(),
              MaterialGroup::OTHER);
    EXPECT_EQ(Material::fromStorableString("BULK|Balsa|170|0.0|", false)->getGroup(),
              MaterialGroup::OTHER);
    // A sixth field stays inside the group field (split with a limit of 5).
    EXPECT_EQ(Material::fromStorableString("BULK|Balsa|170|0.0|Woods|extra", false)->getGroup(),
              MaterialGroup::OTHER);
    // The name keeps its spaces and may be empty.
    EXPECT_EQ(Material::fromStorableString("BULK| spaced name |1|0|Woods", false)->getName(),
              " spaced name ");
    EXPECT_EQ(Material::fromStorableString("BULK||1|0|Woods", false)->getName(), "");
    // Java's Double.parseDouble trims the number.
    EXPECT_EQ(Material::fromStorableString("BULK|x| 170 |Woods", false)->getDensity(), 170.0);

    // The legacy group name resolves through a storage; without one it is OTHER.
    EXPECT_EQ(Material::fromStorableString(
                  "LINE|Braided nylon (2 mm, 1/16 in)|0.001|0.0|ThreadsLines", true)
                  ->getGroup(),
              MaterialGroup::OTHER);
    EXPECT_EQ(
        Material::fromStorableString("LINE|Braided nylon (2 mm, 1/16 in)|0.001|ThreadsLines", true)
            ->getGroup(),
        MaterialGroup::OTHER);
    MaterialStorage storage;
    addBuiltinMaterials(storage);
    EXPECT_EQ(Material::fromStorableString(
                  "LINE|Braided nylon (2 mm, 1/16 in)|0.001|0.0|ThreadsLines", true, storage)
                  ->getGroup(),
              MaterialGroup::NYLONS);
    EXPECT_EQ(Material::fromStorableString("LINE|braided NYLON (2 mm, 1/16 in)|0.001|ThreadsLines",
                                           true, storage)
                  ->getGroup(),
              MaterialGroup::NYLONS);
    EXPECT_EQ(Material::fromStorableString("LINE|Nothing|0.001|0.0|ThreadsLines", true, storage)
                  ->getGroup(),
              MaterialGroup::OTHER);
    EXPECT_EQ(
        Material::fromStorableString("BULK|Aluminum|2700|0.0|Metals", false, storage)->getGroup(),
        MaterialGroup::METALS);
    EXPECT_EQ(
        Material::fromStorableString("BULK|Aluminum|2700|0.0|Nonsense", false, storage)->getGroup(),
        MaterialGroup::OTHER);
}

TEST(Material, FromStorableStringRejectsMalformedStrings)
{
    for (const std::string_view bad :
         {"", "BULK", "BULK|x", "|x|1", "WOOD|x|1.0", "bulk|x|1.0", "BULK|x|abc", "BULK|x|",
          "BULK|x|1.0.0|Woods", "CUSTOM|x|1.0", "CUSTOM|x|1.0|0.0|Custom", "BULK||"})
    {
        const Result<Material> result = Material::fromStorableString(bad, true);
        ASSERT_FALSE(result.has_value()) << bad;
        EXPECT_EQ(result.error().code, ErrorCode::PARSE) << bad;
        EXPECT_EQ(result.error().message, "Illegal material string: " + std::string(bad)) << bad;
    }
    MaterialStorage storage;
    EXPECT_FALSE(Material::fromStorableString("BULK|x", true, storage).has_value());
}

TEST(Material, LoadFromCopiesEverythingOfTheSameType)
{
    Material       target = Material::newMaterial(Type::BULK, "Old", 1.0, true);
    const Material source = Material::newMaterial(Type::BULK, "New", 2.0, 3.0e9,
                                                  MaterialGroup::COMPOSITES, false, true);
    target.loadFrom(source);
    EXPECT_TRUE(target == source);
    EXPECT_EQ(target.getName(), "New");
    EXPECT_EQ(target.getDensity(), 2.0);
    EXPECT_EQ(target.getInPlaneShearModulus(), 3.0e9);
    EXPECT_EQ(target.getGroup(), MaterialGroup::COMPOSITES);
    EXPECT_FALSE(target.isUserDefined());
    EXPECT_TRUE(target.isDocumentMaterial());
    EXPECT_EQ(target.getType(), Type::BULK);

    const Material line = Material::newMaterial(Type::LINE, "Line", 0.1, true);
    EXPECT_THROW(target.loadFrom(line), std::invalid_argument);
    EXPECT_EQ(target.getName(), "New");  // untouched
    EXPECT_NO_THROW(target.loadFrom(target));
}

TEST(Material, DocumentFlagIsMutable)
{
    Material m = Material::newMaterial(Type::BULK, "x", 1.0, true);
    EXPECT_FALSE(m.isDocumentMaterial());
    m.setDocumentMaterial(true);
    EXPECT_TRUE(m.isDocumentMaterial());
    m.setDocumentMaterial(false);
    EXPECT_FALSE(m.isDocumentMaterial());
    EXPECT_EQ(m.getGroupPriority(), 1000);
    EXPECT_EQ(Material::newMaterial(Type::BULK, "x", 1.0, MaterialGroup::METALS, false)
                  .getGroupPriority(),
              0);
}

}  // namespace
