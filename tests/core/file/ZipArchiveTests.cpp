#include "QtRocket/file/ZipArchive.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"

namespace
{

using QtRocket::ZipArchive;
using QtRocket::ZipWriter;

// Deterministic, incompressible-enough filler (a small linear congruential generator; no <random>
// so that clang-tidy does not object to a constant seed).
std::vector<std::byte> randomBytes(std::size_t count)
{
    std::vector<std::byte> bytes(count);
    std::uint32_t          state = 12345;
    for (auto& b : bytes)
    {
        state = (state * 1664525U) + 1013904223U;
        b     = static_cast<std::byte>(state >> 24);
    }
    return bytes;
}

TEST(ZipArchive, RoundTripsEntriesThroughWriter)
{
    const auto text   = QtRocket::stringToBytes("<openrocket version=\"1.11\"/>\n");
    const auto binary = randomBytes(100'000);

    ZipWriter writer;
    writer.add("rocket.ork", text);
    writer.add("thrustcurves/abc.rse", binary);
    writer.add("empty.txt", {});
    const auto bytes = writer.finish();
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    EXPECT_TRUE(ZipArchive::looksLikeZip(*bytes));

    const auto archive = ZipArchive::fromBytes(*bytes);
    ASSERT_TRUE(archive.has_value()) << archive.error().toString();
    ASSERT_EQ(archive->entries().size(), 3U);
    EXPECT_EQ(archive->entries()[0].name, "rocket.ork");
    EXPECT_TRUE(archive->contains("thrustcurves/abc.rse"));
    EXPECT_FALSE(archive->contains("missing"));
    ASSERT_NE(archive->find("rocket.ork"), nullptr);
    EXPECT_EQ(*archive->find("rocket.ork"), text);
    ASSERT_NE(archive->find("thrustcurves/abc.rse"), nullptr);
    EXPECT_EQ(*archive->find("thrustcurves/abc.rse"), binary);
    ASSERT_NE(archive->find("empty.txt"), nullptr);
    EXPECT_TRUE(archive->find("empty.txt")->empty());
}

TEST(ZipArchive, ReadsOpenRocketMotorZip)
{
    const auto bytes = QtRocket::readFile(QtRocket::Test::testDataDir() / "motors" / "test.zip");
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    const auto archive = ZipArchive::fromBytes(*bytes);
    ASSERT_TRUE(archive.has_value()) << archive.error().toString();
    EXPECT_FALSE(archive->entries().empty());
    for (const auto& entry : archive->entries())
    {
        EXPECT_FALSE(entry.name.empty());
        EXPECT_FALSE(entry.data.empty()) << entry.name;
    }
}

TEST(ZipArchive, ReadsExampleDesign)
{
    const auto bytes =
        QtRocket::readFile(QtRocket::Test::dataDir() / "examples" / "A simple model rocket.ork");
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    ASSERT_TRUE(ZipArchive::looksLikeZip(*bytes));
    const auto archive = ZipArchive::fromBytes(*bytes);
    ASSERT_TRUE(archive.has_value()) << archive.error().toString();
    ASSERT_NE(archive->find("rocket.ork"), nullptr);
    const std::string xml = QtRocket::bytesToString(*archive->find("rocket.ork"));
    EXPECT_NE(xml.find("<openrocket"), std::string::npos);
}

TEST(ZipArchive, RejectsGarbage)
{
    const auto garbage = randomBytes(1000);
    EXPECT_FALSE(ZipArchive::looksLikeZip(garbage));
    const auto archive = ZipArchive::fromBytes(garbage);
    ASSERT_FALSE(archive.has_value());
    EXPECT_EQ(archive.error().code, QtRocket::ErrorCode::PARSE);
    EXPECT_FALSE(ZipArchive::fromBytes({}).has_value());
}

}  // namespace
