#include "QtRocket/material/MaterialDatabase.h"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/util/Signal.h"

namespace
{

using QtRocket::addBuiltinMaterials;
using QtRocket::Material;
using QtRocket::MaterialDatabase;
using QtRocket::MaterialGroup;
using QtRocket::MaterialStorage;
using Type = Material::Type;

Material bulk(const std::string& name, double density,
              std::optional<MaterialGroup> group = std::nullopt)
{
    return Material::newMaterial(Type::BULK, name, density, group, false, false);
}

// ---- Ported from MaterialDatabaseTest.java (Databases with the built-in materials) ----

class MaterialDatabaseTest : public ::testing::Test
{
protected:
    MaterialDatabaseTest() { addBuiltinMaterials(databases); }

    MaterialStorage databases;
};

TEST_F(MaterialDatabaseTest, DatabasesInitialization)
{
    EXPECT_FALSE(databases.bulkMaterials().empty());
    EXPECT_FALSE(databases.surfaceMaterials().empty());
    EXPECT_FALSE(databases.lineMaterials().empty());
}

/// Verify the exact number of built-in materials so accidental removals or duplicate additions
/// cannot silently reduce or expand the choices presented to users.
TEST_F(MaterialDatabaseTest, DefaultMaterialCounts)
{
    EXPECT_EQ(databases.bulkMaterials().size(), 32U);
    EXPECT_EQ(databases.surfaceMaterials().size(), 8U);
    EXPECT_EQ(databases.lineMaterials().size(), 42U);

    const std::size_t totalMaterialCount = databases.bulkMaterials().size() +
                                           databases.surfaceMaterials().size() +
                                           databases.lineMaterials().size();
    EXPECT_EQ(totalMaterialCount, 82U);
    EXPECT_EQ(databases.totalMaterialCount(), 82U);
}

TEST_F(MaterialDatabaseTest, FindMaterialByTypeAndName)
{
    const std::optional<Material> aluminum = databases.findMaterial(Type::BULK, "Aluminum");
    ASSERT_TRUE(aluminum.has_value());
    EXPECT_EQ(aluminum->getName(), "Aluminum");
    EXPECT_EQ(aluminum->getType(), Type::BULK);
    EXPECT_NEAR(aluminum->getDensity(), 2700, 0.001);
}

TEST_F(MaterialDatabaseTest, FindMaterialByTypeNameAndDensity)
{
    const Material customMaterial =
        databases.findMaterial(Type::BULK, "CustomMaterial", 1000, MaterialGroup::PLASTICS);
    EXPECT_EQ(customMaterial.getName(), "CustomMaterial");
    EXPECT_EQ(customMaterial.getType(), Type::BULK);
    EXPECT_NEAR(customMaterial.getDensity(), 1000, 0.001);
    EXPECT_EQ(customMaterial.getGroup(), MaterialGroup::PLASTICS);
    EXPECT_TRUE(customMaterial.isUserDefined());
}

TEST_F(MaterialDatabaseTest, GetDatabase)
{
    EXPECT_EQ(&databases.database(Type::BULK), &databases.bulkMaterials());
    EXPECT_EQ(&databases.database(Type::SURFACE), &databases.surfaceMaterials());
    EXPECT_EQ(&databases.database(Type::LINE), &databases.lineMaterials());
    const MaterialStorage& constDatabases = databases;
    EXPECT_EQ(&constDatabases.database(Type::LINE), &databases.lineMaterials());
}

TEST_F(MaterialDatabaseTest, GetDatabaseInvalidType)
{
    // Java: getDatabase(null) throws a NullPointerException; the type that has no database here
    // is CUSTOM (OpenRocket: IllegalArgumentException).
    EXPECT_THROW(static_cast<void>(databases.database(Type::CUSTOM)), std::invalid_argument);
    const MaterialStorage& constDatabases = databases;
    EXPECT_THROW(static_cast<void>(constDatabases.database(Type::CUSTOM)), std::invalid_argument);
}

// ---- QtRocket additions: the Database<T> container itself ----

TEST(MaterialDatabase, StartsEmpty)
{
    const MaterialDatabase db;
    EXPECT_TRUE(db.empty());
    EXPECT_EQ(db.size(), 0U);
    EXPECT_EQ(db.begin(), db.end());
    EXPECT_EQ(db.indexOf(bulk("x", 1.0)), -1);
    EXPECT_FALSE(db.contains(bulk("x", 1.0)));
    EXPECT_THROW(static_cast<void>(db.get(0)), std::out_of_range);
}

TEST(MaterialDatabase, KeepsMaterialsInNaturalOrder)
{
    MaterialDatabase db;
    EXPECT_TRUE(db.add(bulk("Delrin", 1420, MaterialGroup::PLASTICS)));
    EXPECT_TRUE(db.add(bulk("Aluminum", 2700, MaterialGroup::METALS)));
    EXPECT_TRUE(db.add(bulk("Balsa", 170, MaterialGroup::WOODS)));
    ASSERT_EQ(db.size(), 3U);
    EXPECT_EQ(db.get(0).getName(), "Aluminum");
    EXPECT_EQ(db.get(1).getName(), "Balsa");
    EXPECT_EQ(db.get(2).getName(), "Delrin");
    EXPECT_THROW(static_cast<void>(db.get(3)), std::out_of_range);

    std::vector<std::string> names;
    for (const Material& m : db)
    {
        names.push_back(m.getName());
    }
    EXPECT_EQ(names, (std::vector<std::string>{"Aluminum", "Balsa", "Delrin"}));

    // The same name sorts by density (Material.compareTo).
    EXPECT_TRUE(db.add(bulk("Balsa", 120)));
    EXPECT_TRUE(db.add(bulk("Balsa", 200)));
    ASSERT_EQ(db.size(), 5U);
    EXPECT_EQ(db.get(1).getDensity(), 120);
    EXPECT_EQ(db.get(2).getDensity(), 170);
    EXPECT_EQ(db.get(3).getDensity(), 200);
    EXPECT_EQ(db.indexOf(bulk("Balsa", 170, MaterialGroup::WOODS)), 2);
    EXPECT_EQ(db.indexOf(bulk("Balsa", 170)), -1);  // the group takes part in equals
    EXPECT_TRUE(db.contains(bulk("Delrin", 1420, MaterialGroup::PLASTICS)));
    EXPECT_FALSE(db.contains(bulk("Delrin", 1421, MaterialGroup::PLASTICS)));

    // Java's String order: upper case before lower case, byte by byte.
    EXPECT_TRUE(db.add(bulk("aluminum", 1)));
    EXPECT_TRUE(db.add(bulk("Zinc", 7140)));
    EXPECT_EQ(db.get(5).getName(), "Zinc");
    EXPECT_EQ(db.get(6).getName(), "aluminum");
}

TEST(MaterialDatabase, RejectsAnEqualMaterial)
{
    MaterialDatabase db;
    EXPECT_TRUE(db.add(bulk("Balsa", 170, MaterialGroup::WOODS)));
    EXPECT_FALSE(db.add(bulk("Balsa", 170, MaterialGroup::WOODS)));
    // equals ignores the user-defined and document flags.
    EXPECT_FALSE(
        db.add(Material::newMaterial(Type::BULK, "Balsa", 170, MaterialGroup::WOODS, true, true)));
    // ... and tolerates a density within MathUtil::equals.
    EXPECT_FALSE(db.add(bulk("Balsa", 170 * (1 + 1e-10), MaterialGroup::WOODS)));
    EXPECT_EQ(db.size(), 1U);

    // Densities that compare equal (within 0.001) but are not equal are both kept, and the
    // newcomer goes before the one it compared equal to (Collections.binarySearch's index).
    EXPECT_TRUE(db.add(bulk("Balsa", 170.0004, MaterialGroup::WOODS)));
    ASSERT_EQ(db.size(), 2U);
    EXPECT_EQ(db.get(0).getDensity(), 170.0004);
    EXPECT_EQ(db.get(1).getDensity(), 170);

    // A material of another type with the same name and density is a different material and is
    // inserted before the one found by the binary search.
    EXPECT_TRUE(db.add(
        Material::newMaterial(Type::SURFACE, "Balsa", 170, MaterialGroup::WOODS, false, false)));
    ASSERT_EQ(db.size(), 3U);
    EXPECT_EQ(db.get(0).getType(), Type::SURFACE);
    EXPECT_EQ(db.get(0).getDensity(), 170);
    EXPECT_EQ(db.get(1).getDensity(), 170.0004);
}

TEST(MaterialDatabase, RemoveTakesTheFirstEqualMaterial)
{
    MaterialDatabase db;
    db.add(bulk("Aluminum", 2700, MaterialGroup::METALS));
    db.add(bulk("Balsa", 170, MaterialGroup::WOODS));
    db.add(bulk("Delrin", 1420, MaterialGroup::PLASTICS));
    EXPECT_FALSE(db.remove(bulk("Balsa", 170)));  // the group differs
    EXPECT_EQ(db.size(), 3U);
    EXPECT_TRUE(db.remove(bulk("Balsa", 170, MaterialGroup::WOODS)));
    EXPECT_EQ(db.size(), 2U);
    EXPECT_FALSE(db.contains(bulk("Balsa", 170, MaterialGroup::WOODS)));
    EXPECT_EQ(db.get(0).getName(), "Aluminum");
    EXPECT_EQ(db.get(1).getName(), "Delrin");
    EXPECT_FALSE(db.remove(bulk("Balsa", 170, MaterialGroup::WOODS)));
    EXPECT_TRUE(db.remove(bulk("Aluminum", 2700, MaterialGroup::METALS)));
    EXPECT_TRUE(db.remove(bulk("Delrin", 1420, MaterialGroup::PLASTICS)));
    EXPECT_TRUE(db.empty());
}

TEST(MaterialDatabase, SignalsAdditionsAndRemovals)
{
    MaterialDatabase         db;
    std::vector<std::string> added;
    std::vector<std::string> removed;
    const MaterialDatabase*  source = nullptr;
    const auto               addedConnection =
        db.materialAdded.connect([&](const Material& m, const MaterialDatabase& from) {
            added.push_back(m.getName());
            source = &from;
        });
    const auto removedConnection =
        db.materialRemoved.connect([&](const Material& m, const MaterialDatabase& from) {
            removed.push_back(m.getName());
            source = &from;
        });

    db.add(bulk("Balsa", 170));
    EXPECT_EQ(added, (std::vector<std::string>{"Balsa"}));
    EXPECT_EQ(source, &db);
    db.add(bulk("Balsa", 170));  // rejected: no signal
    EXPECT_EQ(added.size(), 1U);
    db.add(bulk("Aluminum", 2700));
    EXPECT_EQ(added, (std::vector<std::string>{"Balsa", "Aluminum"}));
    EXPECT_TRUE(removed.empty());

    db.remove(bulk("Zinc", 1));  // nothing removed: no signal
    EXPECT_TRUE(removed.empty());
    db.remove(bulk("Balsa", 170));
    EXPECT_EQ(removed, (std::vector<std::string>{"Balsa"}));
    EXPECT_EQ(source, &db);

    // A disconnected listener hears nothing more.
    EXPECT_TRUE(db.materialAdded.disconnect(addedConnection));
    db.add(bulk("Steel", 7850));
    EXPECT_EQ(added.size(), 2U);
    EXPECT_TRUE(db.materialRemoved.disconnect(removedConnection));
    db.remove(bulk("Steel", 7850));
    EXPECT_EQ(removed.size(), 1U);
}

TEST(MaterialDatabase, AddAllAndMoves)
{
    MaterialDatabase first;
    first.add(bulk("Balsa", 170));
    first.add(bulk("Aluminum", 2700));
    MaterialDatabase second;
    second.add(bulk("Aluminum", 2700));
    second.add(bulk("Delrin", 1420));
    EXPECT_TRUE(second.addAll(first));  // Balsa is new, Aluminum is not
    ASSERT_EQ(second.size(), 3U);
    EXPECT_EQ(second.get(0).getName(), "Aluminum");
    EXPECT_EQ(second.get(1).getName(), "Balsa");
    EXPECT_EQ(second.get(2).getName(), "Delrin");
    EXPECT_FALSE(second.addAll(first));  // nothing new
    EXPECT_FALSE(second.addAll(MaterialDatabase()));
    EXPECT_EQ(first.size(), 2U);  // the source is untouched

    const MaterialDatabase moved = std::move(second);
    EXPECT_EQ(moved.size(), 3U);
    EXPECT_EQ(moved.get(2).getName(), "Delrin");
}

}  // namespace
