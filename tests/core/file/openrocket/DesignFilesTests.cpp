#include "QtRocket/file/openrocket/DesignFiles.h"

#include <expected>
#include <filesystem>
#include <optional>

#include <gtest/gtest.h>

#include "QtRocket/file/openrocket/CsvLookupHandler.h"

// DesignFiles holds the rule of which file a design may read (not OpenRocket's), which the
// lookup tables (decision U5 of 2026-10-08), the decal images and the thrust curves of a plain
// design (decision D8 of 2026-10-14) share. Its forms are swept in CsvLookupHandlerTests.cpp,
// lookup_file_containment_tests.cpp, decal_file_containment_tests.cpp and MotorHandlerTests.cpp;
// here are the rule's own answers and that CsvLookupHandler hands it on unchanged.

namespace
{

using QtRocket::CsvLookupHandler;
using QtRocket::DesignFiles;

TEST(DesignFiles, AFileIsTheNormalFormOfItsNameInTheDesignsDirectory)
{
    const std::filesystem::path directory("design");
    EXPECT_EQ(DesignFiles::fileOfDesign(directory, "decals/a.png"), directory / "decals" / "a.png");
    EXPECT_EQ(DesignFiles::fileOfDesign(directory, "decals/sub/../a.png"),
              directory / "decals" / "a.png");
    EXPECT_EQ(DesignFiles::fileOfDesign(directory, "./a.png"), directory / "a.png");
    EXPECT_EQ(DesignFiles::fileOfDesign(std::filesystem::path(), "a.png"),
              std::filesystem::path("a.png"));
}

TEST(DesignFiles, ARefusalNamesItsReason)
{
    const std::filesystem::path directory("design");
    EXPECT_EQ(DesignFiles::fileOfDesign(std::nullopt, "a.png"),
              std::unexpected(DesignFiles::Refusal::NO_DIRECTORY));
    EXPECT_EQ(DesignFiles::fileOfDesign(directory, "/a.png"),
              std::unexpected(DesignFiles::Refusal::NOT_RELATIVE));
    EXPECT_EQ(DesignFiles::fileOfDesign(directory, "decals/../../a.png"),
              std::unexpected(DesignFiles::Refusal::LEAVES_DIRECTORY));
    EXPECT_EQ(DesignFiles::fileOfDesign(directory, ".../a.png"),
              std::unexpected(DesignFiles::Refusal::LEAVES_DIRECTORY));
    EXPECT_EQ(DesignFiles::fileOfDesign(directory, "nul").has_value(),
              !DesignFiles::kRefusesDeviceNames);

    EXPECT_TRUE(DesignFiles::isRelativeName("decals/a.png"));
    EXPECT_FALSE(DesignFiles::isRelativeName("/decals/a.png"));
    EXPECT_TRUE(DesignFiles::isDeviceName("Aux.csv"));
    EXPECT_FALSE(DesignFiles::isDeviceName("auxiliary.csv"));
}

TEST(DesignFiles, TheLookupHandlerHandsTheRuleOn)
{
    const std::filesystem::path directory("design");
    EXPECT_EQ(CsvLookupHandler::fileOfDesign(directory, "a/../b.csv"),
              DesignFiles::fileOfDesign(directory, "a/../b.csv"));
    EXPECT_EQ(CsvLookupHandler::fileOfDesign(directory, "../b.csv"),
              DesignFiles::fileOfDesign(directory, "../b.csv"));
    EXPECT_EQ(CsvLookupHandler::isDeviceName("COM1"), DesignFiles::isDeviceName("COM1"));
    EXPECT_EQ(CsvLookupHandler::kRefusesDeviceNames, DesignFiles::kRefusesDeviceNames);
}

}  // namespace
