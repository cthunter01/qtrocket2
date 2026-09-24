#include "QtRocket/material/BuiltinMaterials.h"

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/material/MaterialStorage.h"

namespace
{

using QtRocket::addBuiltinMaterials;
using QtRocket::BuiltinMaterial;
using QtRocket::builtinMaterials;
using QtRocket::Material;
using QtRocket::MaterialGroup;
using QtRocket::MaterialStorage;
using QtRocket::toMaterial;
using Type = Material::Type;

const BuiltinMaterial* find(Type type, std::string_view name)
{
    for (const BuiltinMaterial& row : builtinMaterials())
    {
        if (row.type == type && row.name == name)
        {
            return &row;
        }
    }
    return nullptr;
}

TEST(BuiltinMaterials, CountsPerType)
{
    const std::span<const BuiltinMaterial> rows = builtinMaterials();
    EXPECT_EQ(rows.size(), 82U);
    std::map<Type, std::size_t> counts;
    for (const BuiltinMaterial& row : rows)
    {
        counts[row.type]++;
    }
    EXPECT_EQ(counts[Type::BULK], 32U);
    EXPECT_EQ(counts[Type::SURFACE], 8U);
    EXPECT_EQ(counts[Type::LINE], 42U);
    EXPECT_EQ(counts[Type::CUSTOM], 0U);
}

TEST(BuiltinMaterials, NamesAreUniquePerType)
{
    std::set<std::pair<Type, std::string_view>> seen;
    for (const BuiltinMaterial& row : builtinMaterials())
    {
        EXPECT_TRUE(seen.insert({row.type, row.name}).second)
            << "duplicate " << std::string(row.name);
        EXPECT_FALSE(row.name.empty());
        EXPECT_GT(row.density, 0.0);
        EXPECT_GE(row.inPlaneShearModulus, 0.0);
    }
    // The one name shared across types: bulk paper and surface paper.
    EXPECT_NE(find(Type::BULK, "Paper (office)"), nullptr);
    EXPECT_NE(find(Type::SURFACE, "Paper (office)"), nullptr);
}

TEST(BuiltinMaterials, SpotValuesMatchDatabasesJava)
{
    const std::span<const BuiltinMaterial> rows = builtinMaterials();
    EXPECT_EQ(rows.front().name, "Acrylic");
    EXPECT_EQ(rows.front().density, 1190);
    EXPECT_EQ(rows.front().inPlaneShearModulus, 1.7e9);
    EXPECT_EQ(rows.front().group, MaterialGroup::PLASTICS);
    EXPECT_EQ(rows.back().name, "Elastic braided cord (flat 13 mm, 1/2 in)");
    EXPECT_EQ(rows.back().type, Type::LINE);
    EXPECT_EQ(rows.back().density, 0.00551172);
    EXPECT_EQ(rows.back().group, MaterialGroup::ELASTICS);

    const BuiltinMaterial* aluminum = find(Type::BULK, "Aluminum");
    ASSERT_NE(aluminum, nullptr);
    EXPECT_EQ(aluminum->density, 2700);
    EXPECT_EQ(aluminum->inPlaneShearModulus, 26.0e9);
    EXPECT_EQ(aluminum->group, MaterialGroup::METALS);

    const BuiltinMaterial* nylon = find(Type::BULK, "Nylon");
    ASSERT_NE(nylon, nullptr);
    EXPECT_EQ(nylon->density, 1150);
    EXPECT_EQ(nylon->inPlaneShearModulus, 1.15e9);
    EXPECT_EQ(nylon->group, MaterialGroup::FIBERS);  // Nylon 6/6 is filed under fibers

    const BuiltinMaterial* quantum = find(Type::BULK, "Quantum tubing");
    ASSERT_NE(quantum, nullptr);
    EXPECT_EQ(quantum->density, 1050);
    EXPECT_EQ(quantum->inPlaneShearModulus, 0);
    EXPECT_EQ(quantum->group, MaterialGroup::PLASTICS);

    const BuiltinMaterial* blueFoam = find(Type::BULK, "Styrofoam \"Blue foam\" (XPS)");
    ASSERT_NE(blueFoam, nullptr);
    EXPECT_EQ(blueFoam->density, 32);
    EXPECT_EQ(blueFoam->inPlaneShearModulus, 0.0028e9);
    EXPECT_EQ(blueFoam->group, MaterialGroup::FOAMS);

    const BuiltinMaterial* cardboard = find(Type::BULK, "Cardboard");
    ASSERT_NE(cardboard, nullptr);
    EXPECT_EQ(cardboard->density, 680);
    EXPECT_EQ(cardboard->inPlaneShearModulus, 0.4e9);
    EXPECT_EQ(cardboard->group, MaterialGroup::PAPER);

    const BuiltinMaterial* crepe = find(Type::SURFACE, "Cr\xC3\xAApe paper");
    ASSERT_NE(crepe, nullptr);
    EXPECT_EQ(crepe->name, "Crêpe paper");
    EXPECT_EQ(crepe->density, 0.025);
    EXPECT_EQ(crepe->inPlaneShearModulus, 0.0);
    EXPECT_EQ(crepe->group, MaterialGroup::PAPER);

    const BuiltinMaterial* ripstop = find(Type::SURFACE, "Ripstop nylon");
    ASSERT_NE(ripstop, nullptr);
    EXPECT_EQ(ripstop->density, 0.067);
    EXPECT_EQ(ripstop->group, MaterialGroup::FABRICS);

    const BuiltinMaterial* kevlar = find(Type::LINE, "Kevlar 12-strand (25 mm, 1 in)");
    ASSERT_NE(kevlar, nullptr);
    EXPECT_EQ(kevlar->density, 0.45686629);
    EXPECT_EQ(kevlar->group, MaterialGroup::KEVLARS);
    EXPECT_NE(find(Type::LINE, "Kevlar thread 138  (0.4 mm, 1/64 in)"), nullptr);  // two spaces
    EXPECT_NE(find(Type::LINE, "Nylon flat webbing md. (13 mm, 1/2  in)"), nullptr);
    EXPECT_NE(find(Type::LINE, "Paraline small IIIA (6.4 mm, 1.4 in)"), nullptr);

    const BuiltinMaterial* thread = find(Type::LINE, "Thread (heavy-duty)");
    ASSERT_NE(thread, nullptr);
    EXPECT_EQ(thread->density, 0.0003);
    EXPECT_EQ(thread->group, MaterialGroup::OTHER);

    const BuiltinMaterial* elastic = find(Type::LINE, "Elastic cord (round 2 mm, 1/16 in)");
    ASSERT_NE(elastic, nullptr);
    EXPECT_EQ(elastic->density, 0.0018);
    EXPECT_EQ(elastic->group, MaterialGroup::ELASTICS);

    EXPECT_EQ(find(Type::BULK, "Kevlar thread 138  (0.4 mm, 1/64 in)"), nullptr);
    EXPECT_EQ(find(Type::BULK, "aluminum"), nullptr);  // the table is case-sensitive

    // Surface and line materials carry no shear modulus.
    for (const BuiltinMaterial& row : builtinMaterials())
    {
        if (row.type != Type::BULK)
        {
            EXPECT_EQ(row.inPlaneShearModulus, 0.0) << std::string(row.name);
        }
    }
}

TEST(BuiltinMaterials, GroupsPerType)
{
    std::map<MaterialGroup, int> bulkGroups;
    std::map<MaterialGroup, int> lineGroups;
    for (const BuiltinMaterial& row : builtinMaterials())
    {
        if (row.type == Type::BULK)
        {
            bulkGroups[row.group]++;
        }
        else if (row.type == Type::LINE)
        {
            lineGroups[row.group]++;
        }
        EXPECT_NE(row.group, MaterialGroup::CUSTOM) << std::string(row.name);
    }
    EXPECT_EQ(bulkGroups[MaterialGroup::PLASTICS], 10);
    EXPECT_EQ(bulkGroups[MaterialGroup::METALS], 4);
    EXPECT_EQ(bulkGroups[MaterialGroup::WOODS], 8);
    EXPECT_EQ(bulkGroups[MaterialGroup::COMPOSITES], 4);
    EXPECT_EQ(bulkGroups[MaterialGroup::PAPER], 2);
    EXPECT_EQ(bulkGroups[MaterialGroup::FOAMS], 3);
    EXPECT_EQ(bulkGroups[MaterialGroup::FIBERS], 1);
    EXPECT_EQ(lineGroups[MaterialGroup::OTHER], 2);
    EXPECT_EQ(lineGroups[MaterialGroup::ELASTICS], 14);
    EXPECT_EQ(lineGroups[MaterialGroup::NYLONS], 10);
    EXPECT_EQ(lineGroups[MaterialGroup::KEVLARS], 16);
}

TEST(BuiltinMaterials, ToMaterialIsASystemMaterial)
{
    const BuiltinMaterial* aluminum = find(Type::BULK, "Aluminum");
    ASSERT_NE(aluminum, nullptr);
    const Material material = toMaterial(*aluminum);
    EXPECT_EQ(material.getType(), Type::BULK);
    EXPECT_EQ(material.getName(), "Aluminum");
    EXPECT_EQ(material.getDensity(), 2700);
    EXPECT_EQ(material.getInPlaneShearModulus(), 26.0e9);
    EXPECT_EQ(material.getGroup(), MaterialGroup::METALS);
    EXPECT_FALSE(material.isUserDefined());
    EXPECT_FALSE(material.isDocumentMaterial());
    EXPECT_EQ(material.toStorableString(), "BULK|Aluminum|2700.0|2.6E10|Metals");

    const Material crepe = toMaterial(*find(Type::SURFACE, "Cr\xC3\xAApe paper"));
    EXPECT_EQ(crepe.toStorableString(), "SURFACE|Crêpe paper|0.025|0.0|PaperProducts");
    const Material kevlar = toMaterial(*find(Type::LINE, "Kevlar thread 138  (0.4 mm, 1/64 in)"));
    EXPECT_EQ(kevlar.toStorableString(),
              "LINE|Kevlar thread 138  (0.4 mm, 1/64 in)|1.4808E-4|0.0|Kevlars");
}

TEST(BuiltinMaterials, FillsAStorage)
{
    MaterialStorage storage;
    EXPECT_EQ(addBuiltinMaterials(storage), 82U);
    EXPECT_EQ(storage.bulkMaterials().size(), 32U);
    EXPECT_EQ(storage.surfaceMaterials().size(), 8U);
    EXPECT_EQ(storage.lineMaterials().size(), 42U);
    EXPECT_EQ(storage.totalMaterialCount(), 82U);
    // Every row is found again, and the databases are sorted by name.
    for (const BuiltinMaterial& row : builtinMaterials())
    {
        EXPECT_TRUE(storage.database(row.type).contains(toMaterial(row))) << std::string(row.name);
    }
    EXPECT_EQ(storage.bulkMaterials().get(0).getName(), "ABS - 100% infill");
    EXPECT_EQ(storage.bulkMaterials().get(1).getName(), "ASA - 100% infill");
    EXPECT_EQ(storage.bulkMaterials().get(2).getName(), "Acrylic");
    EXPECT_EQ(storage.bulkMaterials().get(31).getName(), "Titanium");
    EXPECT_EQ(storage.surfaceMaterials().get(0).getName(), "Cellophane");
    EXPECT_EQ(storage.lineMaterials().get(0).getName(), "Braided nylon (2 mm, 1/16 in)");
    EXPECT_EQ(storage.lineMaterials().get(41).getName(), "Tubular nylon (25 mm, 1 in)");
    // Built-in materials are not user materials: the preferences never hear of them.
    int userAdded = 0;
    storage.userMaterialAdded.connect([&userAdded](const Material&) { userAdded++; });
    MaterialStorage listened;
    listened.userMaterialAdded.connect([&userAdded](const Material&) { userAdded++; });
    addBuiltinMaterials(listened);
    EXPECT_EQ(userAdded, 0);
    // Adding them again adds nothing.
    EXPECT_EQ(addBuiltinMaterials(storage), 0U);
    EXPECT_EQ(storage.totalMaterialCount(), 82U);
}

}  // namespace
