#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"

#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/BugError.h"

// OpenRocket has no JUnit test of ComponentPresetDatabase, ComponentPresetDao or Database. Every
// expectation below (the index a preset is added at, the lists, what find() gives, the digests)
// is what OpenRocket's own classes print for the same presets: PresetDatabaseProbe.java of the
// tier 9a probes, run against the compiled core of commit 5f164fd0e (JDK 17).

namespace
{

using QtRocket::BugError;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetDatabase;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::Manufacturer;
using QtRocket::TypedPropertyMap;

using PresetPtr = ComponentPresetDatabase::PresetPtr;
using Lines     = std::vector<std::string>;

/// The preset the factory makes of @p props; a refusal throws std::bad_expected_access, which
/// fails the test.
[[nodiscard]] PresetPtr make(const TypedPropertyMap& props)
{
    const QtRocket::MaterialStorage materials;
    return std::make_shared<const ComponentPreset>(
        ComponentPresetFactory::create(props, materials).value());
}

/// The probe's tube(): a body tube 24.7 mm by 23.7 mm of @p length, with the LEGACY flag when
/// @p legacy says so (the flag is not part of the digest).
[[nodiscard]] PresetPtr tube(std::string_view manufacturer, const std::string& partNo,
                             double length, std::optional<bool> legacy = std::nullopt)
{
    TypedPropertyMap props;
    if (legacy.has_value())
    {
        props.put(ComponentPreset::kLegacy, *legacy);
    }
    props.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer(manufacturer));
    props.put(ComponentPreset::kPartNo, partNo);
    props.put(ComponentPreset::kType, ComponentPresetType::BODY_TUBE);
    props.put(ComponentPreset::kLength, length);
    props.put(ComponentPreset::kOuterDiameter, 0.0247);
    props.put(ComponentPreset::kInnerDiameter, 0.0237);
    return make(props);
}

/// The probe's chute(): a parachute of @p diameter with six lines of that length.
[[nodiscard]] PresetPtr chute(std::string_view manufacturer, const std::string& partNo,
                              double diameter)
{
    TypedPropertyMap props;
    props.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer(manufacturer));
    props.put(ComponentPreset::kPartNo, partNo);
    props.put(ComponentPreset::kType, ComponentPresetType::PARACHUTE);
    props.put(ComponentPreset::kDiameter, diameter);
    props.put(ComponentPreset::kLineCount, 6);
    props.put(ComponentPreset::kLineLength, diameter);
    return make(props);
}

/// The probe's coupler(): a tube coupler 23.7 mm by 22.7 mm of @p length.
[[nodiscard]] PresetPtr coupler(std::string_view manufacturer, const std::string& partNo,
                                double length)
{
    TypedPropertyMap props;
    props.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer(manufacturer));
    props.put(ComponentPreset::kPartNo, partNo);
    props.put(ComponentPreset::kType, ComponentPresetType::TUBE_COUPLER);
    props.put(ComponentPreset::kLength, length);
    props.put(ComponentPreset::kOuterDiameter, 0.0237);
    props.put(ComponentPreset::kInnerDiameter, 0.0227);
    return make(props);
}

/// One preset as the probe prints it, without the length: "Estes | BT-50 | BODY_TUBE | <digest>".
[[nodiscard]] std::string line(const ComponentPreset& preset)
{
    return std::format("{} | {} | {} | {}", preset.getManufacturer().getSimpleName(),
                       preset.getPartNo(), componentPresetTypeName(preset.getType()),
                       preset.getDigest());
}

[[nodiscard]] Lines lines(std::span<const PresetPtr> presets)
{
    Lines result;
    result.reserve(presets.size());
    for (const PresetPtr& preset : presets)
    {
        result.push_back(line(*preset));
    }
    return result;
}

/// add() of @p preset: the index the object has in the list afterwards, or -1 when add()
/// refused it (the probe's "-> true, index 3" and "-> false").
[[nodiscard]] int addAt(ComponentPresetDatabase& database, const PresetPtr& preset)
{
    if (!database.add(preset))
    {
        return -1;
    }
    const std::vector<PresetPtr>& all = database.listAll();
    for (std::size_t i = 0; i < all.size(); i++)
    {
        if (all[i] == preset)
        {
            return static_cast<int>(i);
        }
    }
    return -2;  // added, but not in the list: never
}

// ---- the digests of the probe's presets

constexpr std::string_view kApogeeAp1 = "d2e730e62baf1cdee64211ee7ea94c8c";
constexpr std::string_view kEstesBt20 = "e15d4e5e9624f484d2422b4403adab3c";
constexpr std::string_view kEstesBt5  = "6be439ebc3de70535b2fa805f6d51521";
constexpr std::string_view kEstesBt50 = "ef6fe044c8ae731946110053dde7c301";  // length 0.4572
constexpr std::string_view kEstesPk8  = "afbad848fbf4ade50af97459bfb35037";
constexpr std::string_view kEstesTc50 = "144583f724ed4ebfda1bba26ea558e49";
constexpr std::string_view kQuestQ1   = "d789d76ae1ab14d9b082d0d6c3fdd10c";

/// The ten distinct presets of the probe's part A, added in its order.
void addDistinct(ComponentPresetDatabase& database)
{
    database.add(tube("Quest", "Q-1", 0.1));
    database.add(tube("Estes", "BT-50", 0.4572, false));
    database.add(tube("Apogee", "AP-1", 0.1));
    database.add(tube("Estes", "BT-20", 0.1));
    database.add(chute("Estes", "PK-8", 0.2032));
    database.add(tube("Estes", "bt-5", 0.1));
    database.add(tube("Estes", "BT-5", 0.1));
    database.add(tube("Public Missiles, Ltd.", "PML-1", 0.1));
    database.add(tube("LOC/Precision", "LOC-1", 0.1));
    database.add(coupler("Estes", "TC-50", 0.05));
}

/// The list after addDistinct().
[[nodiscard]] Lines distinctLines()
{
    return {"Apogee | AP-1 | BODY_TUBE | d2e730e62baf1cdee64211ee7ea94c8c",
            "Estes | BT-20 | BODY_TUBE | e15d4e5e9624f484d2422b4403adab3c",
            "Estes | BT-5 | BODY_TUBE | 6be439ebc3de70535b2fa805f6d51521",
            "Estes | BT-50 | BODY_TUBE | ef6fe044c8ae731946110053dde7c301",
            "Estes | PK-8 | PARACHUTE | afbad848fbf4ade50af97459bfb35037",
            "Estes | TC-50 | TUBE_COUPLER | 144583f724ed4ebfda1bba26ea558e49",
            "Estes | bt-5 | BODY_TUBE | 51181091d6f25f6aceb70dcd458d9b4f",
            "LOC/Precision | LOC-1 | BODY_TUBE | c1f4e2c2f5d87b117e003c1523ca2949",
            "Public Missiles | PML-1 | BODY_TUBE | 8d0c0457eb2ad33da54c6be4a3ee2010",
            "Quest | Q-1 | BODY_TUBE | d789d76ae1ab14d9b082d0d6c3fdd10c"};
}

/// addDistinct() and then the probe's five presets that compare equal to "Estes BT-50": three
/// other lengths (0.2, 0.3, 0.1), the 0.2 one again and a parachute with that part number.
void addAllOfPartA(ComponentPresetDatabase& database)
{
    addDistinct(database);
    database.add(tube("Estes", "BT-50", 0.2, false));
    database.add(tube("Estes", "BT-50", 0.3, false));
    database.add(tube("Estes", "BT-50", 0.1, false));
    database.add(tube("Estes", "BT-50", 0.2, false));
    database.add(chute("Estes", "BT-50", 0.3));
}

/// The five "Estes BT-50" presets of addAllOfPartA(), in list order.
[[nodiscard]] Lines fiveBt50Lines()
{
    return {"Estes | BT-50 | BODY_TUBE | b0a2d33363880e351298646f61f371d4",   // length 0.3
            "Estes | BT-50 | BODY_TUBE | 45bdf8915529a6957e84e58eedf28613",   // length 0.2
            "Estes | BT-50 | BODY_TUBE | f7e11a2ea0ed52c32fc5b4ecf2947ea6",   // length 0.1
            "Estes | BT-50 | PARACHUTE | cde748b26f286e156edbe0b6a501f3ce",   // the parachute
            "Estes | BT-50 | BODY_TUBE | ef6fe044c8ae731946110053dde7c301"};  // length 0.4572
}

/// find() of the probe's part A database, as lines.
[[nodiscard]] Lines found(std::string_view manufacturer, std::string_view partNo)
{
    ComponentPresetDatabase database;
    addAllOfPartA(database);
    return lines(database.find(manufacturer, partNo));
}

// ============================================================================ the test presets

TEST(ComponentPresetDatabase, TheTestPresetsAreTheProbes)
{
    // The same properties give the same digests as in Java, so the lists below hold the presets
    // the probe added.
    EXPECT_EQ(tube("Apogee", "AP-1", 0.1)->getDigest(), kApogeeAp1);
    EXPECT_EQ(tube("Estes", "BT-20", 0.1)->getDigest(), kEstesBt20);
    EXPECT_EQ(tube("Estes", "BT-5", 0.1)->getDigest(), kEstesBt5);
    EXPECT_EQ(tube("Estes", "BT-50", 0.4572, false)->getDigest(), kEstesBt50);
    EXPECT_EQ(chute("Estes", "PK-8", 0.2032)->getDigest(), kEstesPk8);
    EXPECT_EQ(coupler("Estes", "TC-50", 0.05)->getDigest(), kEstesTc50);
    EXPECT_EQ(tube("Quest", "Q-1", 0.1)->getDigest(), kQuestQ1);
}

// ======================================================================================= add()

TEST(ComponentPresetDatabase, ANewDatabaseIsEmpty)
{
    const ComponentPresetDatabase database;
    EXPECT_TRUE(database.empty());
    EXPECT_EQ(database.size(), 0U);
    EXPECT_TRUE(database.listAll().empty());
    EXPECT_TRUE(database.find("Estes", "BT-50").empty());
    EXPECT_TRUE(database.listForType(ComponentPresetType::BODY_TUBE).empty());
}

TEST(ComponentPresetDatabase, AddKeepsTheOrderOfManufacturerAndPartNumber)
{
    // The index each preset is added at: the insertion point of the binary search. The order is
    // String.compareTo's, by UTF-16 code unit: "BT-5" before "BT-50", upper case before lower
    // case ("TC-50" before "bt-5"), and the manufacturers by simple name ("Public Missiles").
    ComponentPresetDatabase database;
    EXPECT_EQ(addAt(database, tube("Quest", "Q-1", 0.1)), 0);
    EXPECT_EQ(addAt(database, tube("Estes", "BT-50", 0.4572, false)), 0);
    EXPECT_EQ(addAt(database, tube("Apogee", "AP-1", 0.1)), 0);
    EXPECT_EQ(addAt(database, tube("Estes", "BT-20", 0.1)), 1);
    EXPECT_EQ(addAt(database, chute("Estes", "PK-8", 0.2032)), 3);
    EXPECT_EQ(addAt(database, tube("Estes", "bt-5", 0.1)), 4);
    EXPECT_EQ(addAt(database, tube("Estes", "BT-5", 0.1)), 2);
    EXPECT_EQ(addAt(database, tube("Public Missiles, Ltd.", "PML-1", 0.1)), 6);
    EXPECT_EQ(addAt(database, tube("LOC/Precision", "LOC-1", 0.1)), 6);
    EXPECT_EQ(addAt(database, coupler("Estes", "TC-50", 0.05)), 5);

    EXPECT_EQ(lines(database.listAll()), distinctLines());
    EXPECT_EQ(database.size(), 10U);
    EXPECT_FALSE(database.empty());
}

TEST(ComponentPresetDatabase, AddRefusesAPresetWhoseDigestIsInTheList)
{
    ComponentPresetDatabase database;
    addDistinct(database);
    const PresetPtr held = database.listAll()[3];
    ASSERT_EQ(held->getDigest(), kEstesBt50);

    // The same object, an equal preset, and two that differ in the LEGACY flag only, which the
    // digest leaves out.
    EXPECT_FALSE(database.add(held));
    EXPECT_FALSE(database.add(tube("Estes", "BT-50", 0.4572, false)));
    EXPECT_FALSE(database.add(tube("Estes", "BT-50", 0.4572, true)));
    EXPECT_FALSE(database.add(tube("Estes", "BT-50", 0.4572)));

    EXPECT_EQ(lines(database.listAll()), distinctLines());
    EXPECT_EQ(database.listAll()[3], held);
}

TEST(ComponentPresetDatabase, PresetsThatCompareEqualButDifferAreAllKept)
{
    // Same manufacturer and part number, another digest: added at the index the search found, in
    // front of the preset found there. The order among them follows from the search (0.3, 0.2,
    // 0.1, the parachute, 0.4572), not from the order of addition.
    ComponentPresetDatabase database;
    addDistinct(database);
    EXPECT_EQ(addAt(database, tube("Estes", "BT-50", 0.2, false)), 3);
    EXPECT_EQ(addAt(database, tube("Estes", "BT-50", 0.3, false)), 3);
    EXPECT_EQ(addAt(database, tube("Estes", "BT-50", 0.1, false)), 5);
    EXPECT_EQ(addAt(database, tube("Estes", "BT-50", 0.2, false)), -1);
    // The type is not part of the order: a parachute with a tube's part number sorts among them.
    EXPECT_EQ(addAt(database, chute("Estes", "BT-50", 0.3)), 6);

    const Lines expected{"Apogee | AP-1 | BODY_TUBE | d2e730e62baf1cdee64211ee7ea94c8c",
                         "Estes | BT-20 | BODY_TUBE | e15d4e5e9624f484d2422b4403adab3c",
                         "Estes | BT-5 | BODY_TUBE | 6be439ebc3de70535b2fa805f6d51521",
                         "Estes | BT-50 | BODY_TUBE | b0a2d33363880e351298646f61f371d4",
                         "Estes | BT-50 | BODY_TUBE | 45bdf8915529a6957e84e58eedf28613",
                         "Estes | BT-50 | BODY_TUBE | f7e11a2ea0ed52c32fc5b4ecf2947ea6",
                         "Estes | BT-50 | PARACHUTE | cde748b26f286e156edbe0b6a501f3ce",
                         "Estes | BT-50 | BODY_TUBE | ef6fe044c8ae731946110053dde7c301",
                         "Estes | PK-8 | PARACHUTE | afbad848fbf4ade50af97459bfb35037",
                         "Estes | TC-50 | TUBE_COUPLER | 144583f724ed4ebfda1bba26ea558e49",
                         "Estes | bt-5 | BODY_TUBE | 51181091d6f25f6aceb70dcd458d9b4f",
                         "LOC/Precision | LOC-1 | BODY_TUBE | c1f4e2c2f5d87b117e003c1523ca2949",
                         "Public Missiles | PML-1 | BODY_TUBE | 8d0c0457eb2ad33da54c6be4a3ee2010",
                         "Quest | Q-1 | BODY_TUBE | d789d76ae1ab14d9b082d0d6c3fdd10c"};
    EXPECT_EQ(lines(database.listAll()), expected);
    EXPECT_EQ(database.size(), 14U);
}

TEST(ComponentPresetDatabase, ANullPresetIsABug)
{
    ComponentPresetDatabase database;
    EXPECT_THROW(database.add(nullptr), BugError);
    EXPECT_THROW(database.insert(nullptr), BugError);
    const std::vector<PresetPtr> withNull{tube("Estes", "BT-20", 0.1), nullptr,
                                          tube("Estes", "BT-5", 0.1)};
    EXPECT_THROW(database.addAll(withNull), BugError);
    // The presets before the null one have been added.
    EXPECT_EQ(lines(database.listAll()),
              Lines{"Estes | BT-20 | BODY_TUBE | e15d4e5e9624f484d2422b4403adab3c"});
}

// ==================================================================================== insert()

TEST(ComponentPresetDatabase, InsertAppendsWhateverTheListHolds)
{
    ComponentPresetDatabase database;
    const PresetPtr         q1   = tube("Quest", "Q-1", 0.1);
    const PresetPtr         bt20 = tube("Estes", "BT-20", 0.1);
    database.insert(q1);
    database.insert(bt20);
    database.insert(q1);

    // Not sorted, and the same preset twice.
    const Lines expected{"Quest | Q-1 | BODY_TUBE | d789d76ae1ab14d9b082d0d6c3fdd10c",
                         "Estes | BT-20 | BODY_TUBE | e15d4e5e9624f484d2422b4403adab3c",
                         "Quest | Q-1 | BODY_TUBE | d789d76ae1ab14d9b082d0d6c3fdd10c"};
    EXPECT_EQ(lines(database.listAll()), expected);
    EXPECT_EQ(database.listAll()[0], q1);
    EXPECT_EQ(database.listAll()[1], bt20);
    EXPECT_EQ(database.listAll()[2], q1);
}

TEST(ComponentPresetDatabase, AddSearchesAnUnsortedListAsItIs)
{
    // After insert() the list need not be sorted; add() still runs Java's binary search over it,
    // so where a preset lands (and whether a duplicate is noticed) follows from that search.
    ComponentPresetDatabase database;
    const PresetPtr         q1 = tube("Quest", "Q-1", 0.1);
    database.insert(q1);
    database.insert(tube("Estes", "BT-20", 0.1));
    database.insert(q1);

    EXPECT_EQ(addAt(database, tube("Estes", "BT-50", 0.4572, false)), 2);
    EXPECT_EQ(addAt(database, tube("Apogee", "AP-1", 0.1)), 0);
    EXPECT_EQ(addAt(database, tube("Quest", "Q-1", 0.1)), -1);
    EXPECT_EQ(addAt(database, tube("Estes", "BT-20", 0.1)), -1);
    EXPECT_EQ(addAt(database, tube("Quest", "Z-9", 0.1)), 5);
    EXPECT_EQ(addAt(database, tube("Estes", "BT-20", 0.1)), -1);

    const Lines expected{"Apogee | AP-1 | BODY_TUBE | d2e730e62baf1cdee64211ee7ea94c8c",
                         "Quest | Q-1 | BODY_TUBE | d789d76ae1ab14d9b082d0d6c3fdd10c",
                         "Estes | BT-20 | BODY_TUBE | e15d4e5e9624f484d2422b4403adab3c",
                         "Estes | BT-50 | BODY_TUBE | ef6fe044c8ae731946110053dde7c301",
                         "Quest | Q-1 | BODY_TUBE | d789d76ae1ab14d9b082d0d6c3fdd10c",
                         "Quest | Z-9 | BODY_TUBE | b7d917f15f3ac352a5bc010cfb0c223b"};
    EXPECT_EQ(lines(database.listAll()), expected);
}

// ==================================================================================== addAll()

TEST(ComponentPresetDatabase, AddAllAddsEachAndTellsWhetherAnyWasNew)
{
    ComponentPresetDatabase      database;
    const std::vector<PresetPtr> batch{tube("Quest", "Q-1", 0.1),
                                       tube("Estes", "BT-50", 0.4572, false),
                                       tube("Estes", "BT-50", 0.4572, true),  // a duplicate
                                       tube("Apogee", "AP-1", 0.1)};
    EXPECT_TRUE(database.addAll(batch));
    const Lines three{"Apogee | AP-1 | BODY_TUBE | d2e730e62baf1cdee64211ee7ea94c8c",
                      "Estes | BT-50 | BODY_TUBE | ef6fe044c8ae731946110053dde7c301",
                      "Quest | Q-1 | BODY_TUBE | d789d76ae1ab14d9b082d0d6c3fdd10c"};
    EXPECT_EQ(lines(database.listAll()), three);
    // The first of two equal presets is the one kept.
    EXPECT_EQ(database.listAll()[1], batch[1]);

    EXPECT_FALSE(database.addAll(batch));
    EXPECT_FALSE(database.addAll(std::span<const PresetPtr>{}));
    EXPECT_EQ(lines(database.listAll()), three);

    const std::vector<PresetPtr> second{tube("Estes", "BT-50", 0.4572, false),
                                        tube("Estes", "BT-20", 0.1)};
    EXPECT_TRUE(database.addAll(second));
    const Lines four{"Apogee | AP-1 | BODY_TUBE | d2e730e62baf1cdee64211ee7ea94c8c",
                     "Estes | BT-20 | BODY_TUBE | e15d4e5e9624f484d2422b4403adab3c",
                     "Estes | BT-50 | BODY_TUBE | ef6fe044c8ae731946110053dde7c301",
                     "Quest | Q-1 | BODY_TUBE | d789d76ae1ab14d9b082d0d6c3fdd10c"};
    EXPECT_EQ(lines(database.listAll()), four);
}

TEST(ComponentPresetDatabase, AddAllOfTheDatabasesOwnList)
{
    // A sorted list finds each of its presets again and adds nothing.
    ComponentPresetDatabase sorted;
    addDistinct(sorted);
    EXPECT_FALSE(sorted.addAll(sorted.listAll()));
    EXPECT_EQ(lines(sorted.listAll()), distinctLines());

    // An unsorted one can miss a preset and add it a second time; the list handed in is read as
    // it was when the call began (Java would throw a ConcurrentModificationException).
    ComponentPresetDatabase unsorted;
    unsorted.insert(tube("Quest", "Q-1", 0.1));
    unsorted.insert(tube("Estes", "BT-20", 0.1));
    unsorted.insert(tube("Apogee", "AP-1", 0.1));
    EXPECT_TRUE(unsorted.addAll(unsorted.listAll()));
    const Lines expected{"Apogee | AP-1 | BODY_TUBE | d2e730e62baf1cdee64211ee7ea94c8c",
                         "Quest | Q-1 | BODY_TUBE | d789d76ae1ab14d9b082d0d6c3fdd10c",
                         "Estes | BT-20 | BODY_TUBE | e15d4e5e9624f484d2422b4403adab3c",
                         "Apogee | AP-1 | BODY_TUBE | d2e730e62baf1cdee64211ee7ea94c8c",
                         "Quest | Q-1 | BODY_TUBE | d789d76ae1ab14d9b082d0d6c3fdd10c"};
    EXPECT_EQ(lines(unsorted.listAll()), expected);
}

// ====================================================================================== find()

TEST(ComponentPresetDatabase, FindGivesEveryPresetOfThePartNumberInListOrder)
{
    EXPECT_EQ(found("Estes", "BT-50"), fiveBt50Lines());
    EXPECT_EQ(found("Estes", "BT-5"),
              Lines{"Estes | BT-5 | BODY_TUBE | 6be439ebc3de70535b2fa805f6d51521"});
    EXPECT_EQ(found("Estes", "bt-5"),
              Lines{"Estes | bt-5 | BODY_TUBE | 51181091d6f25f6aceb70dcd458d9b4f"});
    EXPECT_EQ(found("Estes", "PK-8"),
              Lines{"Estes | PK-8 | PARACHUTE | afbad848fbf4ade50af97459bfb35037"});
    EXPECT_EQ(found("Quest", "Q-1"),
              Lines{"Quest | Q-1 | BODY_TUBE | d789d76ae1ab14d9b082d0d6c3fdd10c"});
}

TEST(ComponentPresetDatabase, FindTakesAnyNameOfTheManufacturer)
{
    // What an .ork file stores is the simple name, but any spelling Manufacturer::matches()
    // accepts finds the presets: another case, surrounding blanks, an alias, the display name.
    EXPECT_EQ(found("ESTES", "BT-50"), fiveBt50Lines());
    EXPECT_EQ(found("estes", "BT-50"), fiveBt50Lines());
    EXPECT_EQ(found(" Estes ", "BT-50"), fiveBt50Lines());
    EXPECT_EQ(found("ES", "BT-50"), fiveBt50Lines());
    EXPECT_EQ(found("E", "BT-50"), fiveBt50Lines());

    const Lines pml{"Public Missiles | PML-1 | BODY_TUBE | 8d0c0457eb2ad33da54c6be4a3ee2010"};
    EXPECT_EQ(found("Public Missiles, Ltd.", "PML-1"), pml);
    EXPECT_EQ(found("Public Missiles", "PML-1"), pml);
    EXPECT_EQ(found("PML", "PML-1"), pml);
    EXPECT_EQ(found("public-missiles", "PML-1"), pml);

    const Lines loc{"LOC/Precision | LOC-1 | BODY_TUBE | c1f4e2c2f5d87b117e003c1523ca2949"};
    EXPECT_EQ(found("LOC", "LOC-1"), loc);
    EXPECT_EQ(found("loc precision", "LOC-1"), loc);

    EXPECT_EQ(found("Q", "Q-1"),
              Lines{"Quest | Q-1 | BODY_TUBE | d789d76ae1ab14d9b082d0d6c3fdd10c"});
}

TEST(ComponentPresetDatabase, FindComparesThePartNumberExactly)
{
    EXPECT_TRUE(found("Estes", "bt-50").empty());
    EXPECT_TRUE(found("Estes", "BT-50 ").empty());
    EXPECT_TRUE(found("Estes", "").empty());
    // Another manufacturer, an unknown one, none.
    EXPECT_TRUE(found("Quest", "BT-50").empty());
    EXPECT_TRUE(found("Nobody Rockets", "BT-50").empty());
    EXPECT_TRUE(found("", "BT-50").empty());
}

TEST(ComponentPresetDatabase, FindGivesTheDatabasesOwnPresets)
{
    ComponentPresetDatabase database;
    addDistinct(database);
    const std::vector<PresetPtr> presets = database.find("ESTES", "BT-50");
    ASSERT_EQ(presets.size(), 1U);
    EXPECT_EQ(presets[0], database.listAll()[3]);
    EXPECT_EQ(presets[0]->getDigest(), kEstesBt50);
}

// ============================================================================= the type lists

TEST(ComponentPresetDatabase, ListForTypeKeepsTheListOrder)
{
    ComponentPresetDatabase database;
    addAllOfPartA(database);

    const Lines tubes{"Apogee | AP-1 | BODY_TUBE | d2e730e62baf1cdee64211ee7ea94c8c",
                      "Estes | BT-20 | BODY_TUBE | e15d4e5e9624f484d2422b4403adab3c",
                      "Estes | BT-5 | BODY_TUBE | 6be439ebc3de70535b2fa805f6d51521",
                      "Estes | BT-50 | BODY_TUBE | b0a2d33363880e351298646f61f371d4",
                      "Estes | BT-50 | BODY_TUBE | 45bdf8915529a6957e84e58eedf28613",
                      "Estes | BT-50 | BODY_TUBE | f7e11a2ea0ed52c32fc5b4ecf2947ea6",
                      "Estes | BT-50 | BODY_TUBE | ef6fe044c8ae731946110053dde7c301",
                      "Estes | bt-5 | BODY_TUBE | 51181091d6f25f6aceb70dcd458d9b4f",
                      "LOC/Precision | LOC-1 | BODY_TUBE | c1f4e2c2f5d87b117e003c1523ca2949",
                      "Public Missiles | PML-1 | BODY_TUBE | 8d0c0457eb2ad33da54c6be4a3ee2010",
                      "Quest | Q-1 | BODY_TUBE | d789d76ae1ab14d9b082d0d6c3fdd10c"};
    EXPECT_EQ(lines(database.listForType(ComponentPresetType::BODY_TUBE)), tubes);

    const Lines chutes{"Estes | BT-50 | PARACHUTE | cde748b26f286e156edbe0b6a501f3ce",
                       "Estes | PK-8 | PARACHUTE | afbad848fbf4ade50af97459bfb35037"};
    EXPECT_EQ(lines(database.listForType(ComponentPresetType::PARACHUTE)), chutes);

    EXPECT_EQ(lines(database.listForType(ComponentPresetType::TUBE_COUPLER)),
              Lines{"Estes | TC-50 | TUBE_COUPLER | 144583f724ed4ebfda1bba26ea558e49"});
    EXPECT_TRUE(database.listForType(ComponentPresetType::NOSE_CONE).empty());
}

TEST(ComponentPresetDatabase, ListForTypesGivesEachPresetOnceInListOrder)
{
    ComponentPresetDatabase database;
    addAllOfPartA(database);

    const Lines chutes{"Estes | BT-50 | PARACHUTE | cde748b26f286e156edbe0b6a501f3ce",
                       "Estes | PK-8 | PARACHUTE | afbad848fbf4ade50af97459bfb35037"};
    const Lines chutesAndCouplers{
        "Estes | BT-50 | PARACHUTE | cde748b26f286e156edbe0b6a501f3ce",
        "Estes | PK-8 | PARACHUTE | afbad848fbf4ade50af97459bfb35037",
        "Estes | TC-50 | TUBE_COUPLER | 144583f724ed4ebfda1bba26ea558e49"};

    // No type: nothing. One type: listForType().
    EXPECT_TRUE(database.listForTypes({}).empty());
    EXPECT_TRUE(database.listForTypes(std::span<const ComponentPresetType>{}).empty());
    EXPECT_EQ(lines(database.listForTypes({ComponentPresetType::PARACHUTE})), chutes);

    // Several: the order is the list's, whatever the order of the types, and a type given
    // twice gives its presets once.
    EXPECT_EQ(lines(database.listForTypes(
                  {ComponentPresetType::PARACHUTE, ComponentPresetType::TUBE_COUPLER})),
              chutesAndCouplers);
    EXPECT_EQ(lines(database.listForTypes(
                  {ComponentPresetType::TUBE_COUPLER, ComponentPresetType::PARACHUTE})),
              chutesAndCouplers);
    EXPECT_EQ(lines(database.listForTypes(
                  {ComponentPresetType::PARACHUTE, ComponentPresetType::PARACHUTE})),
              chutes);
    EXPECT_TRUE(
        database.listForTypes({ComponentPresetType::NOSE_CONE, ComponentPresetType::TRANSITION})
            .empty());

    // The same through a span (Java's listForTypes(List)).
    const std::vector<ComponentPresetType> types{ComponentPresetType::TUBE_COUPLER,
                                                 ComponentPresetType::PARACHUTE};
    EXPECT_EQ(lines(database.listForTypes(types)), chutesAndCouplers);
}

// =================================================================================== ownership

TEST(ComponentPresetDatabase, APresetOutlivesTheDatabaseWhileSomethingHoldsIt)
{
    PresetPtr                            kept;
    std::weak_ptr<const ComponentPreset> dropped;
    {
        ComponentPresetDatabase database;
        addDistinct(database);
        kept    = database.find("Estes", "BT-50").at(0);
        dropped = database.find("Quest", "Q-1").at(0);
        EXPECT_FALSE(dropped.expired());
    }
    // The database is gone: the preset nobody else held went with it, the other one is intact.
    EXPECT_TRUE(dropped.expired());
    ASSERT_NE(kept, nullptr);
    EXPECT_EQ(kept->getDigest(), kEstesBt50);
    EXPECT_EQ(kept->get(ComponentPreset::kLength), 0.4572);
}

TEST(ComponentPresetDatabase, ACopyOfADatabaseSharesThePresets)
{
    ComponentPresetDatabase database;
    addDistinct(database);
    ComponentPresetDatabase copy = database;
    EXPECT_EQ(copy.listAll(), database.listAll());

    // The lists are separate; the presets are the same objects.
    EXPECT_TRUE(copy.add(tube("Quest", "Z-9", 0.1)));
    EXPECT_EQ(copy.size(), 11U);
    EXPECT_EQ(database.size(), 10U);
    EXPECT_EQ(copy.listAll()[0], database.listAll()[0]);
}

}  // namespace
