// The preset databases of the tests (TestPresetDatabase.h): what the small one holds, which the
// tests that use it may rely on. (The bundled one is the preset golden's subject:
// preset_golden_tests.cpp.)

#include <format>
#include <set>
#include <string>

#include <gtest/gtest.h>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "rocket/preset/TestPresetDatabase.h"

namespace
{

using QtRocket::ComponentPresetDatabase;
using QtRocket::ComponentPresetType;

/// The number of presets of each type in @p database: "<TYPE> <n>; " for every type.
[[nodiscard]] std::string typeCounts(const ComponentPresetDatabase& database)
{
    std::string counts;
    for (const ComponentPresetType type : QtRocket::kAllComponentPresetTypes)
    {
        counts += std::format("{} {}; ", QtRocket::componentPresetTypeName(type),
                              database.listForType(type).size());
    }
    return counts;
}

/// The simple names of the manufacturers of the presets of @p database.
[[nodiscard]] std::set<std::string> manufacturersOf(const ComponentPresetDatabase& database)
{
    std::set<std::string> manufacturers;
    for (const ComponentPresetDatabase::PresetPtr& preset : database.listAll())
    {
        manufacturers.insert(preset->getManufacturer().getSimpleName());
    }
    return manufacturers;
}

/// The part numbers of the legacy presets of @p database.
[[nodiscard]] std::set<std::string> legacyPartNumbersOf(const ComponentPresetDatabase& database)
{
    std::set<std::string> legacy;
    for (const ComponentPresetDatabase::PresetPtr& preset : database.listAll())
    {
        const bool* const isLegacy =
            preset->getProperties().get(QtRocket::ComponentPreset::kLegacy);
        if (isLegacy != nullptr && *isLegacy)
        {
            legacy.insert(preset->getPartNo());
        }
    }
    return legacy;
}

TEST(TestPresetDatabase, TheSmallDatabaseHoldsEveryTypeOfFourManufacturers)
{
    const ComponentPresetDatabase database = QtRocket::Test::smallPresetDatabase();
    EXPECT_EQ(database.size(), 43U);
    EXPECT_EQ(typeCounts(database),
              "BODY_TUBE 12; NOSE_CONE 6; TRANSITION 2; TUBE_COUPLER 4; BULK_HEAD 2; "
              "CENTERING_RING 8; ENGINE_BLOCK 2; LAUNCH_LUG 1; RAIL_BUTTON 1; STREAMER 2; "
              "PARACHUTE 3; ");
    EXPECT_EQ(manufacturersOf(database),
              (std::set<std::string>{"Estes", "Quest", "Rail-Buttons.com", "Top Flight Recovery"}));
    EXPECT_EQ(legacyPartNumbersOf(database), (std::set<std::string>{"BH-1 ", "ST-1"}));
    // Held in Database.add()'s order: by manufacturer, then by part number.
    EXPECT_EQ(database.listAll().front()->getManufacturer().getSimpleName(), "Estes");
    EXPECT_EQ(database.listAll().back()->getManufacturer().getSimpleName(), "Top Flight Recovery");
}

TEST(TestPresetDatabase, TheSmallDatabaseIsANewDatabaseEachTime)
{
    ComponentPresetDatabase first = QtRocket::Test::smallPresetDatabase();
    first.insert(first.listAll().front());
    EXPECT_EQ(first.size(), 44U);
    const ComponentPresetDatabase second = QtRocket::Test::smallPresetDatabase();
    EXPECT_EQ(second.size(), 43U);
    // The presets are shared.
    EXPECT_EQ(second.listAll().front(), first.listAll().front());
}

}  // namespace
