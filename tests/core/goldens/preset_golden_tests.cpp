// The component presets QtRocket reads from its bundled .orc files (data/components), compared
// with what OpenRocket reads from the same files, byte for byte: the preset golden
// tests/data/goldens/presets/presets.json (GoldenPresets.h), which tools/openrocket-goldens'
// PresetDumper wrote with OpenRocket's OpenRocketComponentLoader and ComponentPresetDatabase.
// File by file every preset, its type, manufacturer, part number and digest and every property
// with its value, exactly, and the order of the 5228 presets of the database once every file was
// added; the six nose cones of hasTranscendentalDensity() within a tolerance (decision O-M3-14).
//
// The files are read by OpenRocketComponentLoader one by one (as OpenRocket's
// ComponentPresetDatabaseLoader does) and, for the database, by ComponentPresetDatabaseLoader
// (bundledPresetDatabase()); each test is a process of its own under ctest and reads them once.

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/file/DirectoryIterator.h"
#include "QtRocket/file/preset/ComponentPresetDatabaseLoader.h"
#include "QtRocket/file/preset/OpenRocketComponentLoader.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Sha256.h"
#include "TestPaths.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenPresets.h"
#include "rocket/preset/TestPresetDatabase.h"

namespace
{

using QtRocket::ComponentPresetDatabaseLoader;
using QtRocket::OpenRocketComponentLoader;
using QtRocket::Test::goldenPresetMismatches;
using QtRocket::Test::goldenPresets;
using Json = nlohmann::ordered_json;

/// How many mismatches a test reports before it stops looking.
constexpr std::size_t kReportedMismatches = 20;

/// The bundled directory, data/components.
[[nodiscard]] std::filesystem::path componentsDirectory()
{
    return QtRocket::Test::dataDir() / ComponentPresetDatabaseLoader::kBundledDirectory;
}

/// The preset files of the bundled directory, in the order ComponentPresetDatabaseLoader reads
/// them (DirectoryIterator), relative to it with '/' between their parts.
[[nodiscard]] std::vector<std::string> bundledFileNames()
{
    std::vector<std::string>                   names;
    const QtRocket::DirectoryIterator::Listing listing = QtRocket::DirectoryIterator::listFiles(
        componentsDirectory(), ComponentPresetDatabaseLoader::isPresetFile);
    for (const std::filesystem::path& file : listing.files)
    {
        const std::u8string name =
            file.lexically_relative(componentsDirectory()).generic_u8string();
        names.emplace_back(name.begin(), name.end());
    }
    return names;
}

/// The SHA-256 of @p path, or why it cannot be read.
[[nodiscard]] std::string sha256Of(const std::filesystem::path& path)
{
    const QtRocket::Result<std::vector<std::byte>> bytes = QtRocket::readFile(path);
    return bytes ? QtRocket::sha256Hex(*bytes) : bytes.error().toString();
}

/// The array @p key of the object @p object, or an empty one; a reference, so that the golden's
/// four megabytes are not copied.
[[nodiscard]] const Json& arrayOf(const Json& object, std::string_view key)
{
    static const Json kEmpty = Json::array();
    const auto        found  = object.find(std::string(key));
    return found != object.end() && found->is_array() ? *found : kEmpty;
}

/// The golden, or a test failure and an empty object.
[[nodiscard]] const Json& goldenOrFail()
{
    static const Json             kEmpty = Json::object();
    const QtRocket::Result<Json>& golden = goldenPresets();
    if (!golden)
    {
        ADD_FAILURE() << "cannot read the preset golden: " << golden.error().toString();
        return kEmpty;
    }
    return *golden;
}

/// The golden preset that @p id ("<file>#<index>", an entry of "databaseOrder") names, or null.
[[nodiscard]] const Json* goldenPresetById(const Json& golden, const std::string& id)
{
    const std::size_t hash = id.rfind('#');
    if (hash == std::string::npos)
    {
        return nullptr;
    }
    const std::string file  = id.substr(0, hash);
    const std::size_t index = std::stoul(id.substr(hash + 1));
    for (const Json& entry : arrayOf(golden, "files"))
    {
        if (entry.value("file", "") == file)
        {
            const Json& presets = entry.at("presets");
            return index < presets.size() ? &presets.at(index) : nullptr;
        }
    }
    return nullptr;
}

/// What differs between the presets OpenRocketComponentLoader reads from the bundled file of
/// the golden entry @p entry and the entry's presets; the number of presets goes to @p count.
[[nodiscard]] std::string fileMismatches(const Json& entry, const OpenRocketComponentLoader& loader,
                                         std::size_t& count)
{
    const std::string    file = entry.value("file", "");
    QtRocket::WarningSet warnings;
    const QtRocket::Result<std::vector<OpenRocketComponentLoader::PresetPtr>> presets =
        loader.loadFile(componentsDirectory() / QtRocket::pathFromUtf8(file), warnings);
    if (!presets)
    {
        return std::format("{}: {}\n", file, presets.error().toString());
    }
    std::string mismatches;
    if (!warnings.empty())
    {
        mismatches += std::format("{}: {} warnings\n", file, warnings.size());
    }
    const Json& golden = entry.at("presets");
    if (presets->size() != golden.size() || entry.value("presetCount", 0U) != golden.size())
    {
        mismatches += std::format("{}: {} presets, the golden has {}\n", file, presets->size(),
                                  golden.size());
    }
    for (std::size_t i = 0; i < presets->size() && i < golden.size(); i++)
    {
        const std::string lines = goldenPresetMismatches(*(*presets)[i], golden.at(i));
        if (!lines.empty())
        {
            mismatches += std::format("{}#{}:\n{}", file, i, lines);
        }
    }
    count += presets->size();
    return mismatches;
}

// The manifest lists the golden (its entry "presets": the file, its SHA-256, which a
// regeneration by PresetDumper alone keeps up to date, and the counts), and the golden is the
// harness's: its files are the manifest's preset files.
TEST(PresetGolden, IsTheHarnesssFileForTheManifestsPresetFiles)
{
    const QtRocket::Result<nlohmann::json> manifest =
        QtRocket::Test::loadGoldenJson("manifest.json");
    ASSERT_TRUE(manifest.has_value()) << manifest.error().toString();
    ASSERT_TRUE(manifest->contains("presets"));
    const nlohmann::json& entry = manifest->at("presets");
    EXPECT_EQ(entry.value("json", ""), "presets/presets.json");
    EXPECT_EQ(entry.value("sha256", ""),
              sha256Of(QtRocket::Test::goldensDir() / "presets" / "presets.json"));
    EXPECT_EQ(entry.value("fileCount", 0), 31);
    EXPECT_EQ(entry.value("presetCount", 0), 5231);
    EXPECT_EQ(entry.value("databaseCount", 0), 5228);

    const Json& golden = goldenOrFail();
    EXPECT_EQ(golden.value("schema", ""), "presets");
    EXPECT_EQ(golden.value("schemaVersion", 0), QtRocket::Test::kGoldenSchemaVersion);
    EXPECT_EQ(golden.value("fileCount", 0), 31);
    EXPECT_EQ(golden.value("presetCount", 0), 5231);
    EXPECT_EQ(golden.value("databaseCount", 0), 5228);
    const nlohmann::json& digests = manifest->at("databases").at("presetFileSha256");
    std::size_t           files   = 0;
    for (const Json& file : arrayOf(golden, "files"))
    {
        const std::string name = file.value("file", "");
        ASSERT_TRUE(digests.contains(name)) << name;
        EXPECT_EQ(file.value("sha256", ""), digests.at(name).get<std::string>()) << name;
        files++;
    }
    EXPECT_EQ(files, digests.size());
}

// The golden's files are the bundled files, in the order ComponentPresetDatabaseLoader reads
// them (PresetDumper reads them in DirectoryIterator's order), with the same bytes.
TEST(PresetGolden, IsMadeOfTheBundledFilesInTheOrderOfTheLoader)
{
    const Json&              golden = goldenOrFail();
    std::vector<std::string> files;
    for (const Json& file : arrayOf(golden, "files"))
    {
        files.push_back(file.value("file", ""));
        EXPECT_EQ(sha256Of(componentsDirectory() / QtRocket::pathFromUtf8(files.back())),
                  file.value("sha256", ""))
            << files.back();
    }
    EXPECT_EQ(files, bundledFileNames());
    EXPECT_EQ(files.size(), 31U);
}

// Every bundled file gives OpenRocket's presets: the same number, and each preset with
// OpenRocket's type, manufacturer, part number, digest and properties, in order (5231 in all;
// the database keeps 5228, see below).
TEST(PresetGolden, EveryBundledFileGivesOpenRocketsPresets)
{
    const Json&               golden = goldenOrFail();
    QtRocket::MaterialStorage materials;
    QtRocket::addBuiltinMaterials(materials);
    const OpenRocketComponentLoader loader(materials);
    std::string                     mismatches;
    std::size_t                     count = 0;
    std::size_t                     files = 0;
    for (const Json& entry : arrayOf(golden, "files"))
    {
        mismatches += fileMismatches(entry, loader, count);
        files++;
    }
    EXPECT_EQ(mismatches, "");
    EXPECT_EQ(files, 31U);
    EXPECT_EQ(count, 5231U);
    EXPECT_EQ(golden.value("presetCount", 0U), count);
}

/// The part numbers of the presets of @p database that hasTranscendentalDensity(), in its order.
[[nodiscard]] std::vector<std::string> transcendentalPartNumbers(
    const QtRocket::ComponentPresetDatabase& database)
{
    std::vector<std::string> partNumbers;
    for (const QtRocket::ComponentPresetDatabase::PresetPtr& pointer : database.listAll())
    {
        const QtRocket::ComponentPreset& preset = *pointer;
        if (QtRocket::Test::hasTranscendentalDensity(preset))
        {
            partNumbers.push_back(preset.getPartNo());
        }
    }
    return partNumbers;
}

// The bundled database (ComponentPresetDatabaseLoader over data/components) holds OpenRocket's
// 5228 presets in the order of OpenRocket's: Database.add() sorts by manufacturer and part number
// and keeps presets that compare equal in an order of the search, which the order of the files
// decides (42 groups of equal part numbers, 22 of them across files), and drops three duplicates
// of semroc.orc. Each preset is compared whole, as above.
//
// Not all of their values are compared exactly: the six nose cones of internal/semroc-legacy.orc
// whose density OpenRocket derives from the volume of a HAACK or a POWER shape
// (hasTranscendentalDensity()) have their density compared within a tolerance, and their digests
// not at all.
TEST(PresetGolden, TheBundledDatabaseHoldsThemInOpenRocketsOrder)
{
    const Json&                              golden   = goldenOrFail();
    const Json&                              order    = arrayOf(golden, "databaseOrder");
    const QtRocket::ComponentPresetDatabase& database = QtRocket::Test::bundledPresetDatabase();
    ASSERT_EQ(database.size(), order.size());
    EXPECT_EQ(database.size(), 5228U);
    std::string mismatches;
    std::size_t reported = 0;
    for (std::size_t i = 0; i < database.size() && reported < kReportedMismatches; i++)
    {
        const std::string id     = order.at(i).get<std::string>();
        const Json* const preset = goldenPresetById(golden, id);
        const std::string lines  = preset == nullptr
                                       ? std::format("{} is no preset of the golden\n", id)
                                       : goldenPresetMismatches(*database.listAll()[i], *preset);
        if (!lines.empty())
        {
            mismatches += std::format("database[{}] = {}:\n{}", i, id, lines);
            reported++;
        }
    }
    EXPECT_EQ(mismatches, "");

    EXPECT_EQ(transcendentalPartNumbers(database),
              (std::vector<std::string>{"BC-22595", "BC-523", "BC-836", "BNC-55AC", "BNC-55ACP",
                                        "BNC-70HAC"}));
}

/// The golden entry of the preset @p partNo of the bundled file @p file, or null.
[[nodiscard]] const Json* goldenPresetOf(const Json& golden, const std::string& file,
                                         const std::string& partNo)
{
    for (const Json& entry : arrayOf(golden, "files"))
    {
        if (entry.value("file", "") == file)
        {
            for (const Json& preset : arrayOf(entry, "presets"))
            {
                if (preset.value("partNo", "") == partNo)
                {
                    return &preset;
                }
            }
        }
    }
    return nullptr;
}

/// The preset @p partNo of the bundled file @p file, read by OpenRocketComponentLoader, or null.
[[nodiscard]] OpenRocketComponentLoader::PresetPtr bundledPresetOf(const std::string& file,
                                                                   const std::string& partNo)
{
    QtRocket::MaterialStorage materials;
    QtRocket::addBuiltinMaterials(materials);
    QtRocket::WarningSet                                                      warnings;
    const QtRocket::Result<std::vector<OpenRocketComponentLoader::PresetPtr>> presets =
        OpenRocketComponentLoader(materials).loadFile(
            componentsDirectory() / QtRocket::pathFromUtf8(file), warnings);
    if (presets)
    {
        for (const OpenRocketComponentLoader::PresetPtr& preset : *presets)
        {
            const QtRocket::ComponentPreset& candidate = *preset;
            if (candidate.getPartNo() == partNo)
            {
                return preset;
            }
        }
    }
    return nullptr;
}

/// @p golden with the value at the JSON pointer @p pointer replaced by @p value.
[[nodiscard]] Json changed(const Json& golden, const std::string& pointer, const Json& value)
{
    Json copy                         = golden;
    copy[Json::json_pointer(pointer)] = value;
    return copy;
}

/// @p golden without its property @p key.
[[nodiscard]] Json withoutProperty(const Json& golden, const std::string& key)
{
    Json copy = golden;
    copy["properties"].erase(key);
    return copy;
}

/// @p golden with its property @p key moved to the front.
[[nodiscard]] Json withPropertyFirst(const Json& golden, const std::string& key)
{
    Json properties = Json::object();
    properties[key] = golden.at("properties").at(key);
    for (const auto& [name, value] : golden.at("properties").items())
    {
        properties[name] = value;
    }
    Json copy          = golden;
    copy["properties"] = properties;
    return copy;
}

// The comparison of the tests above sees a difference in anything it compares, and lets the
// six presets of hasTranscendentalDensity() differ in their densities within the tolerance and
// in their digests.
TEST(PresetGolden, TheComparisonSeesEveryDifference)
{
    const Json&                                golden = goldenOrFail();
    const OpenRocketComponentLoader::PresetPtr tube = bundledPresetOf("database/BMS.ORC", "T2-34");
    const Json* const expected = goldenPresetOf(golden, "database/BMS.ORC", "T2-34");
    ASSERT_NE(tube, nullptr);
    ASSERT_NE(expected, nullptr);
    EXPECT_EQ(goldenPresetMismatches(*tube, *expected), "");

    EXPECT_EQ(goldenPresetMismatches(*tube, changed(*expected, "/partNo", "T2-35")),
              "BODY_TUBE 'T2-34': partNo: expected \"T2-35\", found \"T2-34\"\n");
    EXPECT_NE(goldenPresetMismatches(
                  *tube, changed(*expected, "/digest", "00000000000000000000000000000000")),
              "");
    EXPECT_NE(goldenPresetMismatches(*tube, changed(*expected, "/manufacturerDisplayName", "BMS")),
              "");
    // A double is compared bit for bit.
    const double length = expected->at("properties").at("Length").get<double>();
    EXPECT_EQ(goldenPresetMismatches(
                  *tube, changed(*expected, "/properties/Length", std::nextafter(length, 1.0))),
              "BODY_TUBE 'T2-34': Length: expected 0.8636, found 0.8635999999999999\n");
    EXPECT_EQ(
        goldenPresetMismatches(*tube, changed(*expected, "/properties/Material/density", 894.5)),
        "BODY_TUBE 'T2-34': Material.density: expected 894.5, found 894.4\n");
    EXPECT_NE(
        goldenPresetMismatches(*tube, changed(*expected, "/properties/Material/group", "Metals")),
        "");
    // An integer is no double.
    EXPECT_NE(goldenPresetMismatches(*tube, changed(*expected, "/properties/Length", 1)), "");
    // The same keys in the same order.
    EXPECT_NE(goldenPresetMismatches(*tube, withoutProperty(*expected, "Thickness")), "");
    EXPECT_NE(goldenPresetMismatches(*tube, withPropertyFirst(*expected, "Thickness")), "");

    // A HAACK nose cone: its density within the tolerance, its digest not at all.
    const OpenRocketComponentLoader::PresetPtr cone =
        bundledPresetOf("internal/semroc-legacy.orc", "BC-22595");
    const Json* const coneExpected =
        goldenPresetOf(golden, "internal/semroc-legacy.orc", "BC-22595");
    ASSERT_NE(cone, nullptr);
    ASSERT_NE(coneExpected, nullptr);
    ASSERT_TRUE(QtRocket::Test::hasTranscendentalDensity(*cone));
    const double density =
        coneExpected->at("properties").at("Material").at("density").get<double>();
    const Json close =
        changed(changed(*coneExpected, "/properties/Material/density", density * (1.0 + 1e-13)),
                "/digest", "00000000000000000000000000000000");
    EXPECT_EQ(goldenPresetMismatches(*cone, close), "");
    EXPECT_NE(goldenPresetMismatches(*cone, changed(*coneExpected, "/properties/Material/density",
                                                    density * (1.0 + 1e-8))),
              "");
}

}  // namespace
