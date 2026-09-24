#include "QtRocket/material/MaterialStorage.h"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialDatabase.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/util/Signal.h"

namespace
{

using QtRocket::addBuiltinMaterials;
using QtRocket::Material;
using QtRocket::MaterialDatabase;
using QtRocket::MaterialGroup;
using QtRocket::MaterialStorage;
using Type = Material::Type;

class MaterialStorageTest : public ::testing::Test
{
protected:
    MaterialStorageTest() { addBuiltinMaterials(storage); }

    MaterialStorage storage;
};

TEST(MaterialStorage, StartsEmpty)
{
    const MaterialStorage empty;
    EXPECT_EQ(empty.totalMaterialCount(), 0U);
    EXPECT_TRUE(empty.bulkMaterials().empty());
    EXPECT_TRUE(empty.surfaceMaterials().empty());
    EXPECT_TRUE(empty.lineMaterials().empty());
    EXPECT_EQ(empty.materialCount(Type::BULK), 0U);
    EXPECT_EQ(empty.findMaterial(Type::BULK, "Aluminum"), std::nullopt);
    EXPECT_TRUE(empty.allMaterials().empty());
    EXPECT_THROW(static_cast<void>(empty.materialCount(Type::CUSTOM)), std::invalid_argument);
}

TEST_F(MaterialStorageTest, DatabasesByType)
{
    EXPECT_EQ(&storage.database(Type::BULK), &storage.bulkMaterials());
    EXPECT_EQ(&storage.database(Type::SURFACE), &storage.surfaceMaterials());
    EXPECT_EQ(&storage.database(Type::LINE), &storage.lineMaterials());
    EXPECT_THROW(static_cast<void>(storage.database(Type::CUSTOM)), std::invalid_argument);
    EXPECT_EQ(storage.materialCount(Type::BULK), 32U);
    EXPECT_EQ(storage.materialCount(Type::SURFACE), 8U);
    EXPECT_EQ(storage.materialCount(Type::LINE), 42U);
    EXPECT_EQ(storage.totalMaterialCount(), 82U);
}

TEST_F(MaterialStorageTest, FindByNameIgnoresAsciiCase)
{
    const std::optional<Material> aluminum = storage.findMaterial(Type::BULK, "aluminum");
    ASSERT_TRUE(aluminum.has_value());
    EXPECT_EQ(aluminum->getName(), "Aluminum");
    EXPECT_EQ(aluminum->getDensity(), 2700);
    EXPECT_EQ(aluminum->getInPlaneShearModulus(), 26.0e9);
    EXPECT_EQ(aluminum->getGroup(), MaterialGroup::METALS);
    EXPECT_FALSE(aluminum->isUserDefined());
    EXPECT_FALSE(aluminum->isDocumentMaterial());
    EXPECT_TRUE(storage.findMaterial(Type::SURFACE, "RIPSTOP NYLON").has_value());
    EXPECT_TRUE(
        storage.findMaterial(Type::LINE, "kevlar thread 138  (0.4 mm, 1/64 in)").has_value());
    // The type is part of the lookup, and the name is not translated or trimmed.
    EXPECT_EQ(storage.findMaterial(Type::SURFACE, "Aluminum"), std::nullopt);
    EXPECT_EQ(storage.findMaterial(Type::BULK, " Aluminum"), std::nullopt);
    EXPECT_EQ(storage.findMaterial(Type::BULK, "Unobtainium"), std::nullopt);
    EXPECT_THROW(static_cast<void>(storage.findMaterial(Type::CUSTOM, "Aluminum")),
                 std::invalid_argument);
    // The result is a copy: changing it does not change the database.
    std::optional<Material> copy = storage.findMaterial(Type::BULK, "Aluminum");
    copy->setDocumentMaterial(true);
    EXPECT_FALSE(storage.findMaterial(Type::BULK, "Aluminum")->isDocumentMaterial());
}

TEST_F(MaterialStorageTest, FindByNameAndDensityOrFallBackToACustomMaterial)
{
    // The .ork loader's lookup: name (any ASCII case) and density within MathUtil::equals.
    const Material found = storage.findMaterial(Type::BULK, "ALUMINUM", 2700 + 1e-6, std::nullopt);
    EXPECT_EQ(found.getName(), "Aluminum");
    EXPECT_EQ(found.getDensity(), 2700);
    EXPECT_EQ(found.getInPlaneShearModulus(), 26.0e9);  // the database's value, not 0
    EXPECT_EQ(found.getGroup(), MaterialGroup::METALS);
    EXPECT_FALSE(found.isUserDefined());
    EXPECT_FALSE(found.isDocumentMaterial());
    // The group given is not compared (files older than 24.12 have none or other groups).
    EXPECT_EQ(storage.findMaterial(Type::BULK, "Aluminum", 2700, MaterialGroup::WOODS).getGroup(),
              MaterialGroup::METALS);
    EXPECT_EQ(storage.findMaterial(Type::BULK, "Aluminum", 2700).getGroup(), MaterialGroup::METALS);

    // Another density is another material: a new user-defined document material.
    const Material other = storage.findMaterial(Type::BULK, "Aluminum", 2710, std::nullopt);
    EXPECT_EQ(other.getName(), "Aluminum");
    EXPECT_EQ(other.getDensity(), 2710);
    EXPECT_EQ(other.getInPlaneShearModulus(), 0.0);
    EXPECT_EQ(other.getGroup(), MaterialGroup::CUSTOM);
    EXPECT_TRUE(other.isUserDefined());
    EXPECT_TRUE(other.isDocumentMaterial());
    EXPECT_EQ(storage.totalMaterialCount(), 82U);  // the fallback is not stored

    const Material unknown =
        storage.findMaterial(Type::LINE, "Unknownium", 0.0042, MaterialGroup::NYLONS);
    EXPECT_EQ(unknown.getType(), Type::LINE);
    EXPECT_EQ(unknown.getName(), "Unknownium");
    EXPECT_EQ(unknown.getDensity(), 0.0042);
    EXPECT_EQ(unknown.getGroup(), MaterialGroup::NYLONS);
    EXPECT_TRUE(unknown.isUserDefined());
    EXPECT_TRUE(unknown.isDocumentMaterial());
    EXPECT_EQ(storage.findMaterial(Type::LINE, "Unknownium", 0.0042).getGroup(),
              MaterialGroup::CUSTOM);
    EXPECT_THROW(static_cast<void>(storage.findMaterial(Type::CUSTOM, "x", 1.0)),
                 std::invalid_argument);
}

TEST_F(MaterialStorageTest, FindWithShearModulus)
{
    // The newer lookup compares the shear modulus as well.
    const Material aluminum =
        storage.findMaterial(Type::BULK, "Aluminum", 2700, 26.0e9, MaterialGroup::METALS);
    EXPECT_FALSE(aluminum.isUserDefined());
    EXPECT_EQ(aluminum.getInPlaneShearModulus(), 26.0e9);
    const Material within =
        storage.findMaterial(Type::BULK, "aluminum", 2700, 26.0e9 * (1 + 1e-9), std::nullopt);
    EXPECT_FALSE(within.isUserDefined());

    const Material differentShear =
        storage.findMaterial(Type::BULK, "Aluminum", 2700, 0.0, std::nullopt);
    EXPECT_TRUE(differentShear.isUserDefined());
    EXPECT_TRUE(differentShear.isDocumentMaterial());
    EXPECT_EQ(differentShear.getInPlaneShearModulus(), 0.0);
    EXPECT_EQ(differentShear.getGroup(), MaterialGroup::CUSTOM);
    const Material withGroup =
        storage.findMaterial(Type::BULK, "Aluminum", 2700, 25.0e9, MaterialGroup::METALS);
    EXPECT_TRUE(withGroup.isUserDefined());
    EXPECT_EQ(withGroup.getInPlaneShearModulus(), 25.0e9);
    EXPECT_EQ(withGroup.getGroup(), MaterialGroup::METALS);
    // Zero shear moduli match zero.
    EXPECT_FALSE(storage.findMaterial(Type::LINE, "Thread (heavy-duty)", 0.0003, 0.0, std::nullopt)
                     .isUserDefined());
    EXPECT_THROW(static_cast<void>(storage.findMaterial(Type::CUSTOM, "x", 1.0, 0.0, std::nullopt)),
                 std::invalid_argument);
}

TEST_F(MaterialStorageTest, AddAndRemoveMaterials)
{
    const Material user = Material::newMaterial(Type::BULK, "Unobtainium", 42, true, false);
    EXPECT_TRUE(storage.addMaterial(user));
    EXPECT_EQ(storage.bulkMaterials().size(), 33U);
    EXPECT_TRUE(storage.bulkMaterials().contains(user));
    EXPECT_FALSE(storage.addMaterial(user));  // already there
    EXPECT_EQ(storage.findMaterial(Type::BULK, "unobtainium")->getDensity(), 42);
    EXPECT_FALSE(storage.findMaterial(Type::BULK, "Unobtainium", 42).isDocumentMaterial());

    EXPECT_TRUE(storage.removeMaterial(user));
    EXPECT_EQ(storage.bulkMaterials().size(), 32U);
    EXPECT_FALSE(storage.removeMaterial(user));
    EXPECT_EQ(storage.findMaterial(Type::BULK, "Unobtainium"), std::nullopt);

    const Material line = Material::newMaterial(Type::LINE, "String", 0.001, true, true);
    EXPECT_TRUE(storage.addMaterial(line));
    EXPECT_EQ(storage.lineMaterials().size(), 43U);
    EXPECT_EQ(storage.bulkMaterials().size(), 32U);
    EXPECT_TRUE(storage.removeMaterial(line));

    const Material custom = Material::newMaterial(Type::CUSTOM, "Odd", 1.0, true);
    EXPECT_THROW(static_cast<void>(storage.addMaterial(custom)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(storage.removeMaterial(custom)), std::invalid_argument);
    EXPECT_EQ(storage.totalMaterialCount(), 82U);
}

TEST_F(MaterialStorageTest, SignalsUserMaterialsToThePreferences)
{
    std::vector<std::string> added;
    std::vector<std::string> removed;
    storage.userMaterialAdded.connect(
        [&added](const Material& m) { added.push_back(m.getName()); });
    storage.userMaterialRemoved.connect(
        [&removed](const Material& m) { removed.push_back(m.getName()); });

    // A system material (not user-defined) is added silently, a user-defined one is announced.
    storage.addMaterial(
        Material::newMaterial(Type::BULK, "System", 1.0, MaterialGroup::OTHER, false));
    EXPECT_TRUE(added.empty());
    storage.addMaterial(Material::newMaterial(Type::BULK, "Mine", 2.0, true));
    storage.addMaterial(Material::newMaterial(Type::SURFACE, "Cloth", 0.1, true, true));
    storage.addMaterial(
        Material::newMaterial(Type::LINE, "Cord", 0.01, MaterialGroup::NYLONS, true));
    EXPECT_EQ(added, (std::vector<std::string>{"Mine", "Cloth", "Cord"}));
    storage.addMaterial(Material::newMaterial(Type::BULK, "Mine", 2.0, true));  // rejected
    EXPECT_EQ(added.size(), 3U);

    // Every removal is announced, user-defined or not.
    storage.removeMaterial(
        Material::newMaterial(Type::BULK, "System", 1.0, MaterialGroup::OTHER, false));
    storage.removeMaterial(
        Material::newMaterial(Type::LINE, "Cord", 0.01, MaterialGroup::NYLONS, true));
    storage.removeMaterial(
        Material::newMaterial(Type::LINE, "Cord", 0.01, MaterialGroup::NYLONS, true));
    EXPECT_EQ(removed, (std::vector<std::string>{"System", "Cord"}));
    // Going through the database directly reaches the same listeners.
    storage.surfaceMaterials().remove(
        Material::newMaterial(Type::SURFACE, "Cloth", 0.1, true, true));
    EXPECT_EQ(removed.size(), 3U);
    storage.bulkMaterials().add(Material::newMaterial(Type::BULK, "Direct", 3.0, true));
    EXPECT_EQ(added.back(), "Direct");
}

TEST_F(MaterialStorageTest, AllMaterialsIsAnIndependentSortedCopy)
{
    MaterialDatabase all = storage.allMaterials();
    EXPECT_EQ(all.size(), 82U);
    for (std::size_t i = 1; i < all.size(); i++)
    {
        EXPECT_LE(all.get(i - 1).compareTo(all.get(i)), 0) << i;
    }
    EXPECT_EQ(all.get(0).getName(), "ABS - 100% infill");
    EXPECT_TRUE(all.contains(*storage.findMaterial(Type::SURFACE, "Mylar")));
    EXPECT_TRUE(all.add(Material::newMaterial(Type::BULK, "Extra", 1.0, true)));
    EXPECT_EQ(all.size(), 83U);
    EXPECT_EQ(storage.totalMaterialCount(), 82U);

    MaterialStorage small;
    small.addMaterial(Material::newMaterial(Type::LINE, "b", 1.0, true));
    small.addMaterial(Material::newMaterial(Type::BULK, "a", 1.0, true));
    small.addMaterial(Material::newMaterial(Type::SURFACE, "a", 1.0, true));
    const MaterialDatabase three = small.allMaterials();
    ASSERT_EQ(three.size(), 3U);
    // Bulk is copied first; the surface "a" compares equal to it and is inserted before it.
    EXPECT_EQ(three.get(0).getType(), Type::SURFACE);
    EXPECT_EQ(three.get(1).getType(), Type::BULK);
    EXPECT_EQ(three.get(2).getName(), "b");
}

}  // namespace
