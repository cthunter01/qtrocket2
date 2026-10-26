#include "QtRocket/rocket/preset/ComponentPresetFavorites.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "rocket/preset/ExamplePresets.h"

// OpenRocket has no JUnit test of the favourites. The stored nodes, keys and values below are
// what OpenRocket's own SwingPreferences writes for the same calls (setComponentFavorite()
// through ComponentPresetDao.setFavorite(), getComponentFavorites()): PrefsProbe.java of the M3
// run 1 probes of part C3 (section "favourites"), run against the compiled core and swing of
// commit 5f164fd0e (JDK 17, java.util.prefs' FileSystemPreferences).

namespace
{

namespace Keys = QtRocket::PreferenceKeys;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetDatabase;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::InMemoryPreferences;
using QtRocket::Manufacturer;
using QtRocket::Preferences;
using QtRocket::TypedPropertyMap;
namespace Favorites = QtRocket::ComponentPresetFavorites;
using QtRocket::ComponentPresetFavorites::FavoriteKeys;
using PresetPtr = ComponentPresetDatabase::PresetPtr;
using TextList  = std::vector<std::string>;

/// A body tube 24.7 mm by 23.7 mm of @p length (the probe's tube()).
[[nodiscard]] PresetPtr bodyTube(std::string_view manufacturer, const std::string& partNo,
                                 double length)
{
    TypedPropertyMap props;
    props.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer(manufacturer));
    props.put(ComponentPreset::kPartNo, partNo);
    props.put(ComponentPreset::kType, ComponentPresetType::BODY_TUBE);
    props.put(ComponentPreset::kLength, length);
    props.put(ComponentPreset::kOuterDiameter, 0.0247);
    props.put(ComponentPreset::kInnerDiameter, 0.0237);
    const QtRocket::MaterialStorage materials;
    return std::make_shared<const ComponentPreset>(
        ComponentPresetFactory::create(props, materials).value());
}

/// The example designs' Estes "BT-50, 30352" body tube.
[[nodiscard]] PresetPtr estesBt50()
{
    const ComponentPresetDatabase database = QtRocket::Test::makeExamplePresetDatabase();
    return database.find("Estes", "BT-50, 30352").at(0);
}

/// The contents of the node @p path, each "key=value", or {"absent"}.
[[nodiscard]] TextList nodeContents(const Preferences& preferences, std::string_view path)
{
    const Preferences* const node = preferences.findNode(path);
    if (node == nullptr)
    {
        return {"absent"};
    }
    TextList contents;
    for (const std::string& key : node->keys())
    {
        contents.push_back(key + "=" + node->get(key).value_or("<none>"));
    }
    return contents;
}

TEST(ComponentPresetFavorites, TheNodeIsOpenRockets)
{
    EXPECT_EQ(Keys::kFavoritePresetsNode, "favoritePresets");
}

// The probe's "setFavorite bt20": the key under the type's node, the value "true".
TEST(ComponentPresetFavorites, AFavoriteIsItsPreferenceKeyInTheNodeOfItsType)
{
    const PresetPtr bt50 = estesBt50();
    ASSERT_EQ(bt50->preferenceKey(), "Estes|BT-50, 30352");

    InMemoryPreferences prefs;
    Favorites::setFavorite(prefs, *bt50, ComponentPresetType::BODY_TUBE, true);
    EXPECT_EQ(nodeContents(prefs, "favoritePresets/BODY_TUBE"),
              TextList{"Estes|BT-50, 30352=true"});
    EXPECT_EQ(prefs.getNode("favoritePresets").childrenNames(), TextList{"BODY_TUBE"});
    EXPECT_EQ(prefs.keys(), TextList{});
    EXPECT_EQ(Favorites::getFavorites(prefs, ComponentPresetType::BODY_TUBE),
              FavoriteKeys{"Estes|BT-50, 30352"});
    EXPECT_TRUE(Favorites::isFavorite(prefs, *bt50, ComponentPresetType::BODY_TUBE));

    // Again: nothing changes.
    Favorites::setFavorite(prefs, *bt50, ComponentPresetType::BODY_TUBE, true);
    EXPECT_EQ(nodeContents(prefs, "favoritePresets/BODY_TUBE"),
              TextList{"Estes|BT-50, 30352=true"});
}

// The probe's "bt20 off" and "bt20 off again".
TEST(ComponentPresetFavorites, AnUnsetFavoriteIsRemoved)
{
    const PresetPtr     bt50 = estesBt50();
    InMemoryPreferences prefs;
    Favorites::setFavorite(prefs, *bt50, ComponentPresetType::BODY_TUBE, true);
    Favorites::setFavorite(prefs, *bt50, ComponentPresetType::BODY_TUBE, false);
    EXPECT_EQ(nodeContents(prefs, "favoritePresets/BODY_TUBE"), TextList{});
    EXPECT_FALSE(Favorites::isFavorite(prefs, *bt50, ComponentPresetType::BODY_TUBE));
    EXPECT_EQ(Favorites::getFavorites(prefs, ComponentPresetType::BODY_TUBE), FavoriteKeys{});

    Favorites::setFavorite(prefs, *bt50, ComponentPresetType::BODY_TUBE, false);
    EXPECT_EQ(nodeContents(prefs, "favoritePresets/BODY_TUBE"), TextList{});
}

// The probe's "bt20 as coupler type": the type passed, not the preset's, names the node.
TEST(ComponentPresetFavorites, TheTypeGivenNamesTheNode)
{
    const PresetPtr     bt50 = estesBt50();
    InMemoryPreferences prefs;
    Favorites::setFavorite(prefs, *bt50, ComponentPresetType::TUBE_COUPLER, true);
    EXPECT_EQ(nodeContents(prefs, "favoritePresets/TUBE_COUPLER"),
              TextList{"Estes|BT-50, 30352=true"});
    EXPECT_EQ(nodeContents(prefs, "favoritePresets/BODY_TUBE"), TextList{"absent"});
    EXPECT_TRUE(Favorites::isFavorite(prefs, *bt50, ComponentPresetType::TUBE_COUPLER));
    EXPECT_FALSE(Favorites::isFavorite(prefs, *bt50, ComponentPresetType::BODY_TUBE));
    EXPECT_EQ(Favorites::getFavorites(prefs, ComponentPresetType::BODY_TUBE), FavoriteKeys{});
}

// The probe's keys written by hand: every key of the node is a favourite.
TEST(ComponentPresetFavorites, EveryKeyIsAFavoriteWhateverItsValue)
{
    InMemoryPreferences prefs;
    Preferences&        node = prefs.getNode("favoritePresets/BODY_TUBE");
    node.put("Cesaroni Technology Inc.|Pro-38 tube", "false");
    node.put("Estes|BT-50, 30352", "nonsense");
    EXPECT_EQ(Favorites::getFavorites(prefs, ComponentPresetType::BODY_TUBE),
              (FavoriteKeys{"Cesaroni Technology Inc.|Pro-38 tube", "Estes|BT-50, 30352"}));
    EXPECT_TRUE(Favorites::isFavorite(prefs, *estesBt50(), ComponentPresetType::BODY_TUBE));
}

TEST(ComponentPresetFavorites, ReadingCreatesNothing)
{
    const PresetPtr           bt50 = estesBt50();
    const InMemoryPreferences prefs;
    EXPECT_EQ(Favorites::getFavorites(prefs, ComponentPresetType::BODY_TUBE), FavoriteKeys{});
    EXPECT_FALSE(Favorites::isFavorite(prefs, *bt50, ComponentPresetType::BODY_TUBE));
    EXPECT_TRUE(prefs.empty());

    // A favourites node without the type's node creates nothing either.
    InMemoryPreferences other;
    Favorites::setFavorite(other, *bt50, ComponentPresetType::NOSE_CONE, true);
    EXPECT_EQ(Favorites::getFavorites(other, ComponentPresetType::PARACHUTE), FavoriteKeys{});
    EXPECT_EQ(other.getNode("favoritePresets").childrenNames(), TextList{"NOSE_CONE"});
}

// The probe's keys: the manufacturer's display name ("cs" is Cesaroni Technology Inc.), and a
// '/' that stays part of the key.
TEST(ComponentPresetFavorites, TheKeyIsTheDisplayNameAndThePartNumber)
{
    const PresetPtr cesaroni = bodyTube("cs", "Pro-38 tube", 0.3);
    const PresetPtr loc      = bodyTube("LOC/Precision", "LT-1.52", 0.762);
    EXPECT_EQ(cesaroni->preferenceKey(), "Cesaroni Technology Inc.|Pro-38 tube");
    EXPECT_EQ(loc->preferenceKey(), "LOC/Precision|LT-1.52");

    InMemoryPreferences prefs;
    Favorites::setFavorite(prefs, *cesaroni, ComponentPresetType::BODY_TUBE, true);
    Favorites::setFavorite(prefs, *loc, ComponentPresetType::BODY_TUBE, true);
    EXPECT_EQ(
        nodeContents(prefs, "favoritePresets/BODY_TUBE"),
        (TextList{"Cesaroni Technology Inc.|Pro-38 tube=true", "LOC/Precision|LT-1.52=true"}));
    EXPECT_EQ(prefs.getNode("favoritePresets/BODY_TUBE").childrenNames(), TextList{});
}

// The key is no digest: the presets of one manufacturer and part number share a favourite.
TEST(ComponentPresetFavorites, PresetsOfOnePartNumberShareTheirFavorite)
{
    const PresetPtr long20  = bodyTube("Estes", "BT-20", 0.4572);
    const PresetPtr short20 = bodyTube("Estes", "BT-20", 0.2286);
    ASSERT_NE(long20->getDigest(), short20->getDigest());

    InMemoryPreferences prefs;
    Favorites::setFavorite(prefs, *long20, ComponentPresetType::BODY_TUBE, true);
    EXPECT_TRUE(Favorites::isFavorite(prefs, *short20, ComponentPresetType::BODY_TUBE));
    Favorites::setFavorite(prefs, *short20, ComponentPresetType::BODY_TUBE, false);
    EXPECT_FALSE(Favorites::isFavorite(prefs, *long20, ComponentPresetType::BODY_TUBE));
}

// Deviation (see the header): java.util.prefs refuses a key over 80 characters (the probe:
// IllegalArgumentException "Key too long: Estes|XXX...XY" for 81, stored for 80); QtRocket's
// stores keep it.
TEST(ComponentPresetFavorites, AKeyLongerThanJavaAllowsIsStored)
{
    constexpr std::size_t kJavaMaxKeyLength = 80;
    const std::string     partNo(kJavaMaxKeyLength + 1 - std::string_view("Estes|").size(), 'X');
    const PresetPtr       tube = bodyTube("Estes", partNo, 0.1);
    ASSERT_EQ(tube->preferenceKey().size(), kJavaMaxKeyLength + 1);

    InMemoryPreferences prefs;
    Favorites::setFavorite(prefs, *tube, ComponentPresetType::BODY_TUBE, true);
    EXPECT_TRUE(Favorites::isFavorite(prefs, *tube, ComponentPresetType::BODY_TUBE));
    Favorites::setFavorite(prefs, *tube, ComponentPresetType::BODY_TUBE, false);
    EXPECT_FALSE(Favorites::isFavorite(prefs, *tube, ComponentPresetType::BODY_TUBE));
}

}  // namespace
