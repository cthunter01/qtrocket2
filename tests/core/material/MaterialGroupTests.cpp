#include "QtRocket/material/MaterialGroup.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"

namespace
{

using QtRocket::addBuiltinMaterials;
using QtRocket::databaseString;
using QtRocket::displayKey;
using QtRocket::displayName;
using QtRocket::isUserDefined;
using QtRocket::kAllMaterialGroups;
using QtRocket::Material;
using QtRocket::MaterialGroup;
using QtRocket::materialGroupFromDatabaseString;
using QtRocket::MaterialStorage;
using QtRocket::priority;

/// The built-in material database that MaterialGroupTest.java resolves "ThreadsLines" against.
class MaterialGroupTest : public ::testing::Test
{
protected:
    MaterialGroupTest() { addBuiltinMaterials(m_storage); }

    MaterialStorage m_storage;
};

/// The material @p found holds, or a placeholder no expectation matches, so that a failed lookup
/// fails the expectations that follow (instead of an unchecked access).
Material orNotFound(const std::optional<Material>& found)
{
    return found.value_or(Material::newMaterial(Material::Type::CUSTOM, "<not found>", 0, true));
}

/// A group's names and flags agree with each other.
void expectGroupConsistent(MaterialGroup group)
{
    EXPECT_EQ(materialGroupFromDatabaseString(databaseString(group)), group);
    EXPECT_EQ(displayKey(group), "MaterialGroup." + std::string(databaseString(group)));
    EXPECT_FALSE(displayName(group).empty());
    EXPECT_EQ(isUserDefined(group), group == MaterialGroup::CUSTOM);
}

void expectEveryGroupConsistent()
{
    for (const MaterialGroup group : kAllMaterialGroups)
    {
        expectGroupConsistent(group);
    }
}

/// The groups' priorities, in the enum order.
std::vector<int> priorities()
{
    std::vector<int> result;
    result.reserve(kAllMaterialGroups.size());
    for (const MaterialGroup group : kAllMaterialGroups)
    {
        result.push_back(priority(group));
    }
    return result;
}

/// The groups' distinct database strings.
std::set<std::string_view> databaseStrings()
{
    std::set<std::string_view> result;
    for (const MaterialGroup group : kAllMaterialGroups)
    {
        result.insert(databaseString(group));
    }
    return result;
}

// ---- Ported from MaterialGroupTest.java ----

TEST_F(MaterialGroupTest, LoadFromDatabaseStringNormalGroups)
{
    // Test that normal groups still work
    EXPECT_EQ(materialGroupFromDatabaseString("Metals"), MaterialGroup::METALS);
    EXPECT_EQ(materialGroupFromDatabaseString("Woods"), MaterialGroup::WOODS);
    EXPECT_EQ(materialGroupFromDatabaseString("Plastics"), MaterialGroup::PLASTICS);
    EXPECT_EQ(materialGroupFromDatabaseString("Elastics"), MaterialGroup::ELASTICS);
    EXPECT_EQ(materialGroupFromDatabaseString("Kevlars"), MaterialGroup::KEVLARS);
    EXPECT_EQ(materialGroupFromDatabaseString("Nylons"), MaterialGroup::NYLONS);
    EXPECT_EQ(materialGroupFromDatabaseString("Other"), MaterialGroup::OTHER);
    EXPECT_EQ(materialGroupFromDatabaseString("Custom"), MaterialGroup::CUSTOM);
}

TEST_F(MaterialGroupTest, LoadFromDatabaseStringInvalidGroup)
{
    // Test that invalid groups throw exception (nullopt here)
    EXPECT_EQ(materialGroupFromDatabaseString("InvalidGroup"), std::nullopt);
}

TEST_F(MaterialGroupTest, BackwardCompatibilityThreadsLinesToElastics)
{
    // Find an elastic material from the database and use its actual name
    const std::optional<Material> elasticMaterialFound =
        m_storage.findMaterial(Material::Type::LINE, "Elastic cord (round 2 mm, 1/16 in)");
    ASSERT_TRUE(elasticMaterialFound.has_value()) << "Elastic material should be found in database";
    const Material elasticMaterial = orNotFound(elasticMaterialFound);
    EXPECT_EQ(elasticMaterial.getGroup(), MaterialGroup::ELASTICS)
        << "Material should be in ELASTICS group";

    const std::optional<MaterialGroup> group = m_storage.materialGroupFromLegacyDatabaseString(
        "ThreadsLines", elasticMaterial.getType(), elasticMaterial.getName(),
        elasticMaterial.getDensity());
    EXPECT_EQ(group, MaterialGroup::ELASTICS);
}

TEST_F(MaterialGroupTest, BackwardCompatibilityThreadsLinesToKevlars)
{
    const std::optional<Material> kevlarMaterialFound =
        m_storage.findMaterial(Material::Type::LINE, "Kevlar thread 138  (0.4 mm, 1/64 in)");
    ASSERT_TRUE(kevlarMaterialFound.has_value()) << "Kevlar material should be found in database";
    const Material kevlarMaterial = orNotFound(kevlarMaterialFound);
    EXPECT_EQ(kevlarMaterial.getGroup(), MaterialGroup::KEVLARS)
        << "Material should be in KEVLARS group";

    const std::optional<MaterialGroup> group = m_storage.materialGroupFromLegacyDatabaseString(
        "ThreadsLines", kevlarMaterial.getType(), kevlarMaterial.getName(),
        kevlarMaterial.getDensity());
    EXPECT_EQ(group, MaterialGroup::KEVLARS);
}

TEST_F(MaterialGroupTest, BackwardCompatibilityThreadsLinesToNylons)
{
    const std::optional<Material> nylonMaterialFound =
        m_storage.findMaterial(Material::Type::LINE, "Braided nylon (2 mm, 1/16 in)");
    ASSERT_TRUE(nylonMaterialFound.has_value()) << "Nylon material should be found in database";
    const Material nylonMaterial = orNotFound(nylonMaterialFound);
    EXPECT_EQ(nylonMaterial.getGroup(), MaterialGroup::NYLONS)
        << "Material should be in NYLONS group";

    const std::optional<MaterialGroup> group = m_storage.materialGroupFromLegacyDatabaseString(
        "ThreadsLines", nylonMaterial.getType(), nylonMaterial.getName(),
        nylonMaterial.getDensity());
    EXPECT_EQ(group, MaterialGroup::NYLONS);
}

TEST_F(MaterialGroupTest, BackwardCompatibilityThreadsLinesToOther)
{
    // When material is not found in ELASTICS, KEVLARS, or NYLONS
    const std::optional<MaterialGroup> group = m_storage.materialGroupFromLegacyDatabaseString(
        "ThreadsLines", Material::Type::LINE, "NonExistentMaterial", 0.001);
    EXPECT_EQ(group, MaterialGroup::OTHER);
}

TEST_F(MaterialGroupTest, BackwardCompatibilityThreadsLinesToOtherForNonLineMaterial)
{
    // When material type is not LINE (ThreadsLines was only for LINE materials)
    const std::optional<MaterialGroup> group = m_storage.materialGroupFromLegacyDatabaseString(
        "ThreadsLines", Material::Type::BULK, "Aluminum", 2700);
    EXPECT_EQ(group, MaterialGroup::OTHER);
}

TEST_F(MaterialGroupTest, BackwardCompatibilityNormalGroup)
{
    // Test that non-ThreadsLines groups work normally
    const std::optional<MaterialGroup> group = m_storage.materialGroupFromLegacyDatabaseString(
        "Metals", Material::Type::BULK, "Aluminum", 2700);
    EXPECT_EQ(group, MaterialGroup::METALS);
}

// testLoadFromDatabaseStringWithBackwardCompatibilityNullGroup has no counterpart: a
// std::string_view cannot be null. An empty name is simply unknown.
TEST_F(MaterialGroupTest, BackwardCompatibilityEmptyGroupIsUnknown)
{
    EXPECT_EQ(
        m_storage.materialGroupFromLegacyDatabaseString("", Material::Type::BULK, "Aluminum", 2700),
        std::nullopt);
    EXPECT_EQ(materialGroupFromDatabaseString(""), std::nullopt);
}

TEST_F(MaterialGroupTest, BackwardCompatibilityCaseInsensitive)
{
    // Test that material name matching is case-insensitive
    const std::optional<Material> elasticMaterialFound =
        m_storage.findMaterial(Material::Type::LINE, "Elastic cord (round 2 mm, 1/16 in)");
    ASSERT_TRUE(elasticMaterialFound.has_value());
    const Material elasticMaterial = orNotFound(elasticMaterialFound);
    EXPECT_EQ(elasticMaterial.getGroup(), MaterialGroup::ELASTICS);

    // Use uppercase version of the name
    const std::optional<MaterialGroup> group = m_storage.materialGroupFromLegacyDatabaseString(
        "ThreadsLines", elasticMaterial.getType(), "ELASTIC CORD (ROUND 2 MM, 1/16 IN)",
        elasticMaterial.getDensity());
    EXPECT_EQ(group, MaterialGroup::ELASTICS);
}

TEST_F(MaterialGroupTest, BackwardCompatibilityDensityTolerance)
{
    // Test that density matching uses MathUtil.equals (with tolerance)
    const std::optional<Material> elasticMaterialFound =
        m_storage.findMaterial(Material::Type::LINE, "Elastic cord (round 2 mm, 1/16 in)");
    ASSERT_TRUE(elasticMaterialFound.has_value());
    const Material elasticMaterial = orNotFound(elasticMaterialFound);
    EXPECT_EQ(elasticMaterial.getGroup(), MaterialGroup::ELASTICS);

    // Using a slightly different density that should still match
    const std::optional<MaterialGroup> group = m_storage.materialGroupFromLegacyDatabaseString(
        "ThreadsLines", elasticMaterial.getType(), elasticMaterial.getName(),
        elasticMaterial.getDensity() + 1e-10);
    EXPECT_EQ(group, MaterialGroup::OTHER);  // We currently don't handle tolerance
    // MathUtil.equals is relative (1e-8 of 0.0018 is 1.8e-11), so this one does match:
    EXPECT_EQ(m_storage.materialGroupFromLegacyDatabaseString(
                  "ThreadsLines", elasticMaterial.getType(), elasticMaterial.getName(),
                  elasticMaterial.getDensity() + 1e-12),
              MaterialGroup::ELASTICS);
}

// ---- QtRocket additions ----

TEST(MaterialGroup, AllGroupsInPriorityOrder)
{
    ASSERT_EQ(kAllMaterialGroups.size(), 13U);
    EXPECT_EQ(kAllMaterialGroups[0], MaterialGroup::METALS);
    EXPECT_EQ(kAllMaterialGroups[12], MaterialGroup::CUSTOM);
    // Strictly increasing in the enum order, so also distinct.
    const std::vector<int> ordered = priorities();
    EXPECT_EQ(std::ranges::adjacent_find(ordered, std::greater_equal<>()), ordered.end());
    EXPECT_EQ(std::set<int>(ordered.begin(), ordered.end()).size(), 13U);
    EXPECT_EQ(databaseStrings().size(), 13U);
    expectEveryGroupConsistent();
}

TEST(MaterialGroup, PrioritiesNamesAndDatabaseStrings)
{
    EXPECT_EQ(priority(MaterialGroup::METALS), 0);
    EXPECT_EQ(priority(MaterialGroup::WOODS), 10);
    EXPECT_EQ(priority(MaterialGroup::PLASTICS), 20);
    EXPECT_EQ(priority(MaterialGroup::FABRICS), 30);
    EXPECT_EQ(priority(MaterialGroup::PAPER), 40);
    EXPECT_EQ(priority(MaterialGroup::FOAMS), 50);
    EXPECT_EQ(priority(MaterialGroup::COMPOSITES), 60);
    EXPECT_EQ(priority(MaterialGroup::FIBERS), 70);
    EXPECT_EQ(priority(MaterialGroup::ELASTICS), 80);
    EXPECT_EQ(priority(MaterialGroup::KEVLARS), 90);
    EXPECT_EQ(priority(MaterialGroup::NYLONS), 100);
    EXPECT_EQ(priority(MaterialGroup::OTHER), 110);
    EXPECT_EQ(priority(MaterialGroup::CUSTOM), 1000);

    EXPECT_EQ(databaseString(MaterialGroup::PAPER), "PaperProducts");
    EXPECT_EQ(displayName(MaterialGroup::PAPER), "Paper Products");
    EXPECT_EQ(displayKey(MaterialGroup::PAPER), "MaterialGroup.PaperProducts");
    EXPECT_EQ(databaseString(MaterialGroup::METALS), "Metals");
    EXPECT_EQ(displayName(MaterialGroup::METALS), "Metals");
    EXPECT_EQ(databaseString(MaterialGroup::CUSTOM), "Custom");
    EXPECT_EQ(databaseString(MaterialGroup::OTHER), "Other");
    EXPECT_EQ(databaseString(MaterialGroup::ELASTICS), "Elastics");
    EXPECT_EQ(databaseString(MaterialGroup::KEVLARS), "Kevlars");
    EXPECT_EQ(databaseString(MaterialGroup::NYLONS), "Nylons");
    EXPECT_EQ(databaseString(MaterialGroup::COMPOSITES), "Composites");
    EXPECT_EQ(databaseString(MaterialGroup::FIBERS), "Fibers");
    EXPECT_EQ(databaseString(MaterialGroup::FOAMS), "Foams");
    EXPECT_EQ(databaseString(MaterialGroup::FABRICS), "Fabrics");
    EXPECT_EQ(databaseString(MaterialGroup::WOODS), "Woods");
    EXPECT_EQ(databaseString(MaterialGroup::PLASTICS), "Plastics");
}

TEST(MaterialGroup, DatabaseStringLookupIsExact)
{
    EXPECT_EQ(materialGroupFromDatabaseString("metals"), std::nullopt);
    EXPECT_EQ(materialGroupFromDatabaseString("Metals "), std::nullopt);
    EXPECT_EQ(materialGroupFromDatabaseString("Paper Products"), std::nullopt);
    EXPECT_EQ(materialGroupFromDatabaseString("PaperProducts"), MaterialGroup::PAPER);
    // The pre-24.12 name needs the material database (MaterialStorage) to resolve.
    EXPECT_EQ(materialGroupFromDatabaseString("ThreadsLines"), std::nullopt);
}

TEST(MaterialGroup, ComparesByPriority)
{
    EXPECT_LT(MaterialGroup::METALS, MaterialGroup::WOODS);
    EXPECT_LT(MaterialGroup::OTHER, MaterialGroup::CUSTOM);
    EXPECT_EQ(MaterialGroup::NYLONS, MaterialGroup::NYLONS);
    EXPECT_NE(MaterialGroup::NYLONS, MaterialGroup::KEVLARS);
}

TEST_F(MaterialGroupTest, LegacyLookupForTheCustomTypeGivesOther)
{
    // OpenRocket's getDatabase(CUSTOM) would throw; here the lookup finds nothing.
    EXPECT_EQ(m_storage.materialGroupFromLegacyDatabaseString(
                  "ThreadsLines", Material::Type::CUSTOM, "Braided nylon (2 mm, 1/16 in)", 0.001),
              MaterialGroup::OTHER);
    EXPECT_EQ(
        m_storage.materialGroupFromLegacyDatabaseString("Woods", Material::Type::CUSTOM, "x", 1.0),
        MaterialGroup::WOODS);
    EXPECT_EQ(
        m_storage.materialGroupFromLegacyDatabaseString("Nonsense", Material::Type::LINE, "x", 1.0),
        std::nullopt);
    // A line material that is not in one of the three replacement groups gives OTHER too.
    EXPECT_EQ(m_storage.materialGroupFromLegacyDatabaseString("ThreadsLines", Material::Type::LINE,
                                                              "Thread (heavy-duty)", 0.0003),
              MaterialGroup::OTHER);
    // An empty storage finds nothing.
    const MaterialStorage empty;
    EXPECT_EQ(empty.materialGroupFromLegacyDatabaseString("ThreadsLines", Material::Type::LINE,
                                                          "Braided nylon (2 mm, 1/16 in)", 0.001),
              MaterialGroup::OTHER);
}

}  // namespace
