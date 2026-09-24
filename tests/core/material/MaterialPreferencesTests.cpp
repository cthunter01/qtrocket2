#include "QtRocket/material/MaterialPreferences.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/util/BugError.h"

namespace
{

namespace Keys = QtRocket::PreferenceKeys;
using QtRocket::addBuiltinMaterials;
using QtRocket::addUserMaterial;
using QtRocket::addUserMaterials;
using QtRocket::BugError;
using QtRocket::getDefaultComponentMaterial;
using QtRocket::getUserMaterials;
using QtRocket::InMemoryPreferences;
using QtRocket::loadDefaultComponentMaterials;
using QtRocket::Material;
using QtRocket::MaterialGroup;
using QtRocket::MaterialStorage;
using QtRocket::Preferences;
using QtRocket::removeUserMaterial;
using QtRocket::setDefaultComponentMaterial;
using QtRocket::storeUserMaterialChanges;
using Type = Material::Type;

// Component class chains as the rocket group will build them: the class and its ancestors up to
// RocketComponent.
constexpr std::array<std::string_view, 5> kBodyTubeChain{
    "BodyTube", "SymmetricComponent", "BodyComponent", "ExternalComponent", "RocketComponent"};
constexpr std::array<std::string_view, 4> kTrapezoidFinSetChain{
    "TrapezoidFinSet", "FinSet", "ExternalComponent", "RocketComponent"};
constexpr std::array<std::string_view, 4> kNoseConeChain{"NoseCone", "Transition",
                                                         "SymmetricComponent", "RocketComponent"};
constexpr std::array<std::string_view, 3> kShockCordChain{"ShockCord", "MassObject",
                                                          "RocketComponent"};

/// The preferences and a material storage holding OpenRocket's built-in materials, as
/// Databases always does.
class MaterialPreferencesTest : public ::testing::Test
{
protected:
    MaterialPreferencesTest() { addBuiltinMaterials(m_storage); }

    /// The built-in material of @p type named @p name, or a placeholder no expectation matches.
    [[nodiscard]] Material builtin(Type type, std::string_view name) const
    {
        return m_storage.findMaterial(type, name)
            .value_or(Material::newMaterial(Type::CUSTOM, "<not found>", 0, true));
    }

    /// The stored user materials, by key.
    [[nodiscard]] std::optional<std::string> userMaterialEntry(std::string_view key) const
    {
        return m_prefs.getInNode(Keys::kUserMaterialsNode, key);
    }

    /// The names of getUserMaterials(), in its order.
    [[nodiscard]] std::vector<std::string> userMaterialNames() const
    {
        const std::vector<Material> materials = getUserMaterials(m_prefs, m_storage);
        std::vector<std::string>    names;
        names.reserve(materials.size());
        for (const Material& material : materials)
        {
            names.push_back(material.getName());
        }
        return names;
    }

    InMemoryPreferences m_prefs;
    MaterialStorage     m_storage;
};

// ---------------------------------------------------------- default component materials

TEST_F(MaterialPreferencesTest, DefaultMaterialsFallBackOnTheBuiltIns)
{
    const Material bulk =
        getDefaultComponentMaterial(m_prefs, kBodyTubeChain, Type::BULK, m_storage);
    EXPECT_EQ(bulk.getName(), "Cardboard");
    EXPECT_EQ(bulk, builtin(Type::BULK, "Cardboard"));
    EXPECT_FALSE(bulk.isUserDefined());
    EXPECT_EQ(bulk.toStorableString(), "BULK|Cardboard|680.0|4.0E8|PaperProducts");

    const Material surface =
        getDefaultComponentMaterial(m_prefs, kBodyTubeChain, Type::SURFACE, m_storage);
    EXPECT_EQ(surface.getName(), "Ripstop nylon");
    EXPECT_EQ(surface.getType(), Type::SURFACE);

    const Material line =
        getDefaultComponentMaterial(m_prefs, kShockCordChain, Type::LINE, m_storage);
    EXPECT_EQ(line.getName(), "Elastic cord (round 2 mm, 1/16 in)");
    EXPECT_EQ(line.getGroup(), MaterialGroup::ELASTICS);
    EXPECT_EQ(QtRocket::kDefaultLineMaterialName, line.getName());
}

TEST_F(MaterialPreferencesTest, DefaultMaterialIsTheNearestStoredOne)
{
    const Material balsa = builtin(Type::BULK, "Balsa");
    setDefaultComponentMaterial(m_prefs, "BodyComponent", balsa);
    EXPECT_EQ(m_prefs.getInNode(Keys::kComponentMaterialsNode, "BodyComponent"),
              "BULK|Balsa|170.0|2.3E8|Woods");
    EXPECT_EQ(getDefaultComponentMaterial(m_prefs, kBodyTubeChain, Type::BULK, m_storage), balsa);
    // Another class chain does not see it.
    EXPECT_EQ(getDefaultComponentMaterial(m_prefs, kTrapezoidFinSetChain, Type::BULK, m_storage)
                  .getName(),
              "Cardboard");

    // A nearer class wins; the stored material is parsed as not user-defined.
    const Material mine =
        Material::newMaterial(Type::BULK, "Mine", 1234.5, 1.0e9, MaterialGroup::CUSTOM, true);
    setDefaultComponentMaterial(m_prefs, "BodyTube", mine);
    const Material found =
        getDefaultComponentMaterial(m_prefs, kBodyTubeChain, Type::BULK, m_storage);
    EXPECT_EQ(found, mine);
    EXPECT_FALSE(found.isUserDefined());
    EXPECT_FALSE(found.isDocumentMaterial());

    // nullopt removes the entry, and the ancestor's shows again.
    setDefaultComponentMaterial(m_prefs, "BodyTube", std::nullopt);
    EXPECT_EQ(m_prefs.getInNode(Keys::kComponentMaterialsNode, "BodyTube"), std::nullopt);
    EXPECT_EQ(getDefaultComponentMaterial(m_prefs, kBodyTubeChain, Type::BULK, m_storage), balsa);
}

TEST_F(MaterialPreferencesTest, DefaultMaterialOfTheWrongTypeGivesTheFallback)
{
    // The nearest stored material decides: when its type differs, Java returns the built-in
    // default without looking further along the chain.
    setDefaultComponentMaterial(m_prefs, "BodyComponent", builtin(Type::BULK, "Balsa"));
    setDefaultComponentMaterial(m_prefs, "BodyTube", builtin(Type::SURFACE, "Paper (office)"));
    EXPECT_EQ(getDefaultComponentMaterial(m_prefs, kBodyTubeChain, Type::BULK, m_storage).getName(),
              "Cardboard");
    EXPECT_EQ(
        getDefaultComponentMaterial(m_prefs, kBodyTubeChain, Type::SURFACE, m_storage).getName(),
        "Paper (office)");
    EXPECT_EQ(getDefaultComponentMaterial(m_prefs, kBodyTubeChain, Type::LINE, m_storage).getName(),
              "Elastic cord (round 2 mm, 1/16 in)");
}

TEST_F(MaterialPreferencesTest, UnparsableDefaultMaterialGivesTheFallback)
{
    for (const std::string_view text :
         {"", "BULK|Balsa", "WOOD|Balsa|170.0", "BULK|Balsa|heavy", "CUSTOM|Odd|1.0|0.0|Custom"})
    {
        m_prefs.setDefaultComponentMaterialString("BodyTube", text);
        EXPECT_EQ(
            getDefaultComponentMaterial(m_prefs, kBodyTubeChain, Type::BULK, m_storage).getName(),
            "Cardboard")
            << text;
    }
}

TEST_F(MaterialPreferencesTest, DefaultMaterialResolvesTheLegacyThreadsLinesGroup)
{
    // Before OpenRocket 24.12 the Kevlar, nylon and elastic lines shared the group ThreadsLines;
    // the storage finds the material's present group.
    m_prefs.setDefaultComponentMaterialString(
        "ShockCord", "LINE|Kevlar thread 800 (1.1 mm, 3/64 in)|9.9211E-4|ThreadsLines");
    const Material kevlar =
        getDefaultComponentMaterial(m_prefs, kShockCordChain, Type::LINE, m_storage);
    EXPECT_EQ(kevlar.getGroup(), MaterialGroup::KEVLARS);
    EXPECT_EQ(kevlar, builtin(Type::LINE, "Kevlar thread 800 (1.1 mm, 3/64 in)"));
    m_prefs.setDefaultComponentMaterialString("ShockCord", "LINE|Twine|0.001|ThreadsLines");
    EXPECT_EQ(
        getDefaultComponentMaterial(m_prefs, kShockCordChain, Type::LINE, m_storage).getGroup(),
        MaterialGroup::OTHER);
}

TEST_F(MaterialPreferencesTest, DefaultMaterialOfCustomTypeIsABug)
{
    // Java: IllegalArgumentException("Unknown material type: CUSTOM").
    EXPECT_THROW(static_cast<void>(
                     getDefaultComponentMaterial(m_prefs, kBodyTubeChain, Type::CUSTOM, m_storage)),
                 BugError);
}

TEST(MaterialPreferences, DefaultMaterialNeedsTheBuiltInMaterials)
{
    const InMemoryPreferences prefs;
    const MaterialStorage     empty;
    EXPECT_THROW(
        static_cast<void>(getDefaultComponentMaterial(prefs, kBodyTubeChain, Type::BULK, empty)),
        BugError);
}

TEST_F(MaterialPreferencesTest, LoadDefaultComponentMaterials)
{
    loadDefaultComponentMaterials(m_prefs, m_storage);
    EXPECT_EQ(m_prefs.getInNode(Keys::kComponentMaterialsNode, "FinSet"),
              "BULK|Balsa|170.0|2.3E8|Woods");
    EXPECT_EQ(m_prefs.getInNode(Keys::kComponentMaterialsNode, "NoseCone"),
              "BULK|Polystyrene|1050.0|1.23E9|Plastics");
    EXPECT_EQ(getDefaultComponentMaterial(m_prefs, kTrapezoidFinSetChain, Type::BULK, m_storage)
                  .getName(),
              "Balsa");
    EXPECT_EQ(getDefaultComponentMaterial(m_prefs, kNoseConeChain, Type::BULK, m_storage).getName(),
              "Polystyrene");
    EXPECT_EQ(getDefaultComponentMaterial(m_prefs, kBodyTubeChain, Type::BULK, m_storage).getName(),
              "Cardboard");

    // Without the built-ins, Databases.findMaterial's null removes the entries.
    const MaterialStorage empty;
    loadDefaultComponentMaterials(m_prefs, empty);
    EXPECT_EQ(m_prefs.getInNode(Keys::kComponentMaterialsNode, "FinSet"), std::nullopt);
    EXPECT_EQ(m_prefs.getInNode(Keys::kComponentMaterialsNode, "NoseCone"), std::nullopt);
}

// ------------------------------------------------------------------- user materials

TEST_F(MaterialPreferencesTest, UserMaterialsAreStoredUnderTheFirstFreeKey)
{
    EXPECT_TRUE(getUserMaterials(m_prefs, m_storage).empty());
    EXPECT_EQ(m_prefs.findNode(Keys::kUserMaterialsNode), nullptr);  // reading creates nothing

    const Material mine = Material::newMaterial(Type::BULK, "Mine", 2.0, true);
    const Material cloth =
        Material::newMaterial(Type::SURFACE, "Cloth", 0.1, 0.0, MaterialGroup::FABRICS, true);
    addUserMaterial(m_prefs, mine, m_storage);
    addUserMaterial(m_prefs, cloth, m_storage);
    EXPECT_EQ(userMaterialEntry("material0"), "BULK|Mine|2.0|0.0|Custom");
    EXPECT_EQ(userMaterialEntry("material1"), "SURFACE|Cloth|0.1|0.0|Fabrics");

    // An equal material is not stored twice, whatever its flags (the group counts, and a
    // material that is not user-defined gets OTHER unless given one).
    addUserMaterial(m_prefs,
                    Material::newMaterial(Type::BULK, "Mine", 2.0, MaterialGroup::CUSTOM, false),
                    m_storage);
    ASSERT_NE(m_prefs.findNode(Keys::kUserMaterialsNode), nullptr);
    EXPECT_EQ(m_prefs.findNode(Keys::kUserMaterialsNode)->keys().size(), 2U);

    // A freed key is reused.
    removeUserMaterial(m_prefs, mine, m_storage);
    EXPECT_EQ(userMaterialEntry("material0"), std::nullopt);
    const Material cord = Material::newMaterial(Type::LINE, "Cord", 0.01, true);
    addUserMaterial(m_prefs, cord, m_storage);
    EXPECT_EQ(userMaterialEntry("material0"), "LINE|Cord|0.01|0.0|Custom");
    EXPECT_EQ(userMaterialNames(), (std::vector<std::string>{"Cord", "Cloth"}));
}

TEST_F(MaterialPreferencesTest, GetUserMaterialsParsesUserDefinedMaterialsInKeyOrder)
{
    Preferences& node = m_prefs.getNode(Keys::kUserMaterialsNode);
    node.put("material2", "LINE|Cord|0.01|0.0|Nylons");
    node.put("material0", "BULK|Mine|2.0|0.0|Custom");
    node.put("material1", "not a material");        // skipped (Java logs it)
    node.put("custom", "SURFACE|Foil|0.2|Metals");  // any key is read, the older form too
    EXPECT_EQ(userMaterialNames(), (std::vector<std::string>{"Foil", "Mine", "Cord"}));

    const std::vector<Material> materials = getUserMaterials(m_prefs, m_storage);
    ASSERT_EQ(materials.size(), 3U);
    EXPECT_TRUE(materials[0].isUserDefined());
    EXPECT_EQ(materials[0].getGroup(), MaterialGroup::METALS);
    EXPECT_EQ(materials[2].getGroup(), MaterialGroup::NYLONS);
    EXPECT_FALSE(materials[1].isDocumentMaterial());
}

TEST_F(MaterialPreferencesTest, GetUserMaterialsDropsDuplicatesAsAHashSetDoes)
{
    Preferences& node = m_prefs.getNode(Keys::kUserMaterialsNode);
    node.put("material0", "BULK|Mine|1.0|0.0|Custom");
    node.put("material1", "BULK|Mine|1.0|0.0|Custom");  // equal, same hash: dropped
    // Equal within MathUtil::equals but hashed apart ((int)(density * 1000) is 999, not 1000):
    // Java's HashSet keeps both.
    node.put("material2", "BULK|Mine|0.9999999999|0.0|Custom");
    const std::vector<Material> materials = getUserMaterials(m_prefs, m_storage);
    ASSERT_EQ(materials.size(), 2U);
    EXPECT_EQ(materials[0], materials[1]);
    EXPECT_NE(materials[0].hashCode(), materials[1].hashCode());
}

TEST_F(MaterialPreferencesTest, AddUserMaterialChecksLikeHashSetContains)
{
    addUserMaterial(m_prefs, Material::newMaterial(Type::BULK, "Mine", 1.0, true), m_storage);
    // Equal to it within MathUtil::equals but hashed apart: HashSet.contains misses it, so Java
    // stores it as well.
    const Material close = Material::newMaterial(Type::BULK, "Mine", 0.9999999999, true);
    addUserMaterial(m_prefs, close, m_storage);
    EXPECT_EQ(userMaterialEntry("material1"), close.toStorableString());
    // Now each has an equal twin with the same hash in the set.
    addUserMaterial(m_prefs, Material::newMaterial(Type::BULK, "Mine", 1.0, true), m_storage);
    addUserMaterial(m_prefs, close, m_storage);
    const Preferences* const node = m_prefs.findNode(Keys::kUserMaterialsNode);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->keys().size(), 2U);
}

TEST_F(MaterialPreferencesTest, RemoveUserMaterialRemovesEveryEqualEntry)
{
    Preferences& node = m_prefs.getNode(Keys::kUserMaterialsNode);
    node.put("material0", "BULK|Mine|2.0|0.0|Custom");
    node.put("material1", "BULK|Other|3.0|0.0|Custom");
    node.put("material2", "BULK|MINE|2.0|0.0|Custom");  // names compare exactly: kept
    node.put("material3", "BULK|Mine|2.0|0.0|Custom");
    node.put("material4", "garbage");
    // Material.equals: the flags do not count, the group does.
    removeUserMaterial(m_prefs, Material::newMaterial(Type::BULK, "Mine", 2.0, false), m_storage);
    EXPECT_EQ(node.keys().size(), 5U);  // group OTHER
    removeUserMaterial(m_prefs,
                       Material::newMaterial(Type::BULK, "Mine", 2.0, MaterialGroup::CUSTOM, false),
                       m_storage);
    EXPECT_EQ(node.keys(), (std::vector<std::string>{"material1", "material2", "material4"}));
    // Removing what is not there changes nothing.
    removeUserMaterial(m_prefs, Material::newMaterial(Type::LINE, "Mine", 2.0, true), m_storage);
    EXPECT_EQ(node.keys().size(), 3U);
}

TEST_F(MaterialPreferencesTest, AddUserMaterialsFillsTheStorage)
{
    Preferences& node = m_prefs.getNode(Keys::kUserMaterialsNode);
    node.put("material0", "BULK|Mine|2.0|0.0|Custom");
    node.put("material1", "LINE|Cord|0.01|0.0|Nylons");
    node.put("material2", "BULK|Balsa|170.0|2.3E8|Woods");  // equal to a built-in: not added
    const std::size_t before = m_storage.totalMaterialCount();
    EXPECT_EQ(addUserMaterials(m_storage, m_prefs), 2U);
    EXPECT_EQ(m_storage.totalMaterialCount(), before + 2);
    const std::optional<Material> mine = m_storage.findMaterial(Type::BULK, "Mine");
    ASSERT_TRUE(mine.has_value());
    EXPECT_TRUE(mine.value_or(builtin(Type::BULK, "Balsa")).isUserDefined());
    EXPECT_TRUE(m_storage.findMaterial(Type::LINE, "Cord").has_value());
    EXPECT_EQ(addUserMaterials(m_storage, m_prefs), 0U);  // all there already
}

TEST_F(MaterialPreferencesTest, StoreUserMaterialChangesFollowsTheStorage)
{
    {
        const auto     connections = storeUserMaterialChanges(m_storage, m_prefs);
        const Material mine        = Material::newMaterial(Type::BULK, "Mine", 2.0, true);
        m_storage.addMaterial(mine);
        EXPECT_EQ(userMaterialEntry("material0"), "BULK|Mine|2.0|0.0|Custom");
        // A material that is not user-defined is not stored.
        m_storage.addMaterial(
            Material::newMaterial(Type::BULK, "System", 1.0, MaterialGroup::OTHER, false));
        EXPECT_EQ(userMaterialNames(), (std::vector<std::string>{"Mine"}));
        m_storage.addMaterial(Material::newMaterial(Type::LINE, "Cord", 0.01, true));
        EXPECT_EQ(userMaterialNames(), (std::vector<std::string>{"Mine", "Cord"}));

        m_storage.removeMaterial(mine);
        EXPECT_EQ(userMaterialNames(), (std::vector<std::string>{"Cord"}));
    }
    // Once the connections are gone the preferences stay as they are.
    m_storage.addMaterial(Material::newMaterial(Type::BULK, "Later", 5.0, true));
    m_storage.removeMaterial(Material::newMaterial(Type::LINE, "Cord", 0.01, true));
    EXPECT_EQ(userMaterialNames(), (std::vector<std::string>{"Cord"}));
}

}  // namespace
