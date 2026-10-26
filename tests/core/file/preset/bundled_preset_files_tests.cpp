// The component preset files QtRocket ships (data/components): OpenRocket's, byte for byte. The
// 16 files of database/ are the orc/ directory of the openrocket-database submodule at 1512874
// (Apache-2.0, with its LICENSE and README.md, which OpenRocket's build copies beside them); the 15
// of internal/ are OpenRocket's core/src/main/resources/datafiles/components/internal (GPL).
// .gitattributes keeps git from changing their line ends.

#include <array>
#include <cstddef>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/cli/DataDirectory.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Sha256.h"
#include "TestPaths.h"
#include "goldens/GoldenData.h"

namespace
{

struct BundledFile
{
    std::string_view path;  ///< relative to data/components
    std::size_t      size;
    std::string_view sha256;
};

// sha256sum of the files OpenRocket 5f164fd0e ships (the .orc files' digests are those of
// tests/data/goldens/manifest.json, which the harness read from the same files).
// clang-format off
constexpr auto kBundledFiles = std::to_array<BundledFile>({
    {.path = "database/BMS.ORC", .size = 165545, .sha256 = "5e256d2817d94260f7eb26dd65c5c2a66420542e02f41a11e3dc20b83de97a03"},
    {.path = "database/LICENSE", .size = 11357, .sha256 = "b40930bbcf80744c86c46a12bc9da056641d722716c378f5659b9e555ef833e1"},
    {.path = "database/README.md", .size = 11620, .sha256 = "eeebd7dde5a92da6a22bd44d4e3221869e6fd734430ddf7c5cf23025d3bf28bd"},
    {.path = "database/ROCKETARIUM.ORC", .size = 78873, .sha256 = "5ca1af198d418a5c2f8c0680c63ff381f6725854bddfb8ebcd6f0b20b4784082"},
    {.path = "database/apogee.orc", .size = 5280, .sha256 = "70e956f390a83cade8fbd07a81922e14ab898f218f205187fcdc5d370563b363"},
    {.path = "database/bluetube.orc", .size = 31847, .sha256 = "e3152289a440d282e2f781cca38e5a1e08bf33892818828c41266b7204bf72e9"},
    {.path = "database/competition_chutes.orc", .size = 35490, .sha256 = "b775834c65881b026cb374c2635113a516a5c748278cd65ddf029f1992a14446"},
    {.path = "database/estes_classic.orc", .size = 285453, .sha256 = "0a258b36dcb3a0bb28c5e44ea20c562fabe2da3e66a8cea6f185fa5362df676a"},
    {.path = "database/estes_ps2.orc", .size = 20607, .sha256 = "cc3be6d8b80c0378238c8ee2a30eb97eefaa06db61e5eb81f944b76ae7989f24"},
    {.path = "database/generic_materials.orc", .size = 55244, .sha256 = "5efe55beca89c94a1e4d78f032f3c596d2a113acf31cc89505afb8f6918da318"},
    {.path = "database/giantleaprocketry.orc", .size = 54800, .sha256 = "f30c5b42427bfc0b96cf44d599dfd29d4db575aac8754bd9be86de541d955e27"},
    {.path = "database/loc_precision.orc", .size = 91929, .sha256 = "92c7c5670daf83892c6917c29ceef566c1006672cea659abd71e9a0b1b62f480"},
    {.path = "database/madcow.orc", .size = 151881, .sha256 = "1499c88a130fc6cc23fd09eae4d0170ba37b0ad63fde6ea34738dbadf4b6f8b5"},
    {.path = "database/mpc.orc", .size = 35863, .sha256 = "abf2302fa592b4470ec833d72f9c999e2c900ee7f8ca7cd6b11a81a6bfcd4970"},
    {.path = "database/publicmissiles.orc", .size = 91739, .sha256 = "ea99d895b1df952657c63cf2bf91ca47c874c976d72dd6080d650090df129215"},
    {.path = "database/quest.orc", .size = 60251, .sha256 = "b2375f7fd4c1fc2225aa720829eceee2c7a1deb10ec393211842ac658847b2e4"},
    {.path = "database/semroc.orc", .size = 978252, .sha256 = "ac99356bd151d226ce6b9e8d2753705261e8160eaabe9609e6512e4464316c75"},
    {.path = "database/top_flight.orc", .size = 29525, .sha256 = "bc96d460c6109eebd695f808628d23ed51e119dc993e6d0cc0f0f0e4a3386456"},
    {.path = "internal/Estes-legacy.orc", .size = 65298, .sha256 = "78d4839ac36c24013005a2c58770559e073492d10e2f7b41aa9f9ae72bb26c6e"},
    {.path = "internal/Front_Range_Rocket_Recovery.orc", .size = 21653, .sha256 = "e2a9e44f154dd61efcc64b4172ae86f2a07e6521d4d405e5764aa0ba894d3f2b"},
    {.path = "internal/Fruity_Chutes_Enhanced.orc", .size = 51928, .sha256 = "fbec776851fca0d7b3ebcbc357d82315c056329bb29dbefeb7d47998248a90f3"},
    {.path = "internal/LocPrecision-legacy.orc", .size = 72781, .sha256 = "e71e3235593b135d3a419423c96f11897f33bb561f7579c128d4452c5c3cbac6"},
    {.path = "internal/Quest-legacy.orc", .size = 55801, .sha256 = "4f2692b83372426dcc0eb6918eb19698914a53d93216f2821f03112dcd12ab1d"},
    {.path = "internal/RailButton_Database.orc", .size = 6384, .sha256 = "be77c9f763dc9c102e059c5af410071fcc5f1fde09282889f0964af4935161f5"},
    {.path = "internal/Rocketman.orc", .size = 82698, .sha256 = "d3bb335edd4564c1f744e9240abbe47503d959863d6999d9f55d5682d5ebc107"},
    {.path = "internal/Spherachutes_Parachutes.orc", .size = 34879, .sha256 = "5e292f8e4473df000416b9459709716337ac96dd5068e0b770942aeaed8a154e"},
    {.path = "internal/b2_Rocketry_Parachutes..orc", .size = 16283, .sha256 = "d3b3c2c9f8a2ce4f0f3e476dd0653037e7fee4da196b27bf098123e4e884e3da"},
    {.path = "internal/bluetube-legacy.orc", .size = 9997, .sha256 = "c212e93fbe76d73dc36e5e6a97634fa7c62eb0e9d8acdfeb3d6e1a6510bc7328"},
    {.path = "internal/bms-legacy.orc", .size = 87837, .sha256 = "0848b4f924436e83a1d143ef0406a41c23e4a23ce228a0c2f40a536eaafedcf4"},
    {.path = "internal/fliskits-legacy.orc", .size = 90359, .sha256 = "e5a71d64f4471bea37a92ac1d958fd6ab684e3780efe4255e9017704cf829e00"},
    {.path = "internal/giantleaprocketry-legacy.orc", .size = 3627, .sha256 = "01b46c2795c01dcc7c1c6bb6c99cab04c305e317b8cf763e18a39f6e0c2de2f8"},
    {.path = "internal/publicmissiles-legacy.orc", .size = 70313, .sha256 = "18947fc27cab3f4954f9652a27319ba538eb2f670d225ffa74dd75fd9016c95e"},
    {.path = "internal/semroc-legacy.orc", .size = 535954, .sha256 = "316dd98dcd4ffceb7ea36569266cb37567723dedc2689eec2eb18a5b661fcd7a"},
});
// clang-format on

/// The files under @p root, relative to it with '/' between their parts.
[[nodiscard]] std::set<std::string> filesUnder(const std::filesystem::path& root)
{
    std::set<std::string> files;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::recursive_directory_iterator(root))
    {
        if (entry.is_regular_file())
        {
            const std::u8string name = entry.path().lexically_relative(root).generic_u8string();
            files.emplace(name.begin(), name.end());
        }
    }
    return files;
}

/// The SHA-256 of the file @p path, or the reason it cannot be read.
[[nodiscard]] std::string sha256Of(const std::filesystem::path& path)
{
    const QtRocket::Result<std::vector<std::byte>> bytes = QtRocket::readFile(path);
    return bytes ? QtRocket::sha256Hex(*bytes) : bytes.error().toString();
}

[[nodiscard]] std::filesystem::path componentsDirectory()
{
    return QtRocket::Test::dataDir() / "components";
}

TEST(BundledPresetFiles, AreOpenRocketsFilesByteForByte)
{
    std::set<std::string> expected;
    for (const BundledFile& file : kBundledFiles)
    {
        expected.emplace(file.path);
        const std::filesystem::path path =
            componentsDirectory() / QtRocket::pathFromUtf8(file.path);
        EXPECT_EQ(std::filesystem::file_size(path), file.size) << file.path;
        EXPECT_EQ(sha256Of(path), file.sha256) << file.path;
    }
    EXPECT_EQ(filesUnder(componentsDirectory()), expected);
}

TEST(BundledPresetFiles, AreTheFilesOfTheGoldensManifest)
{
    const QtRocket::Result<nlohmann::json> manifest =
        QtRocket::Test::loadGoldenJson("manifest.json");
    ASSERT_TRUE(manifest) << manifest.error().toString();
    const nlohmann::json& digests     = manifest->at("databases").at("presetFileSha256");
    std::size_t           presetFiles = 0;
    for (const BundledFile& file : kBundledFiles)
    {
        if (file.path.ends_with(".orc") || file.path.ends_with(".ORC"))
        {
            presetFiles++;
            ASSERT_TRUE(digests.contains(std::string(file.path))) << file.path;
            EXPECT_EQ(digests.at(std::string(file.path)).get<std::string>(), file.sha256)
                << file.path;
        }
    }
    EXPECT_EQ(presetFiles, 31U);
    EXPECT_EQ(digests.size(), presetFiles);
    EXPECT_EQ(manifest->at("openrocket").at("presetDatabaseCommit").get<std::string>(),
              "1512874ace50917e25ccf7d7df1f616b5f39a8b5");
}

TEST(BundledPresetFiles, AreCopiedIntoTheBuildTree)
{
    // The build tree has the installed layout (share/QtRocket beside bin/), which the programs
    // read the components from: the same bytes.
    const std::filesystem::path copies = QtRocket::DataDirectory::dataDirectory("") / "components";
    for (const BundledFile& file : kBundledFiles)
    {
        const std::filesystem::path                    relative = QtRocket::pathFromUtf8(file.path);
        const QtRocket::Result<std::vector<std::byte>> copy = QtRocket::readFile(copies / relative);
        ASSERT_TRUE(copy) << copy.error().toString();
        EXPECT_TRUE(
            *copy ==
            QtRocket::readFile(componentsDirectory() / relative).value_or(std::vector<std::byte>{}))
            << file.path;
    }
}

}  // namespace
