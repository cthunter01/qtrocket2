#include "QtRocket/util/FileIo.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Error.h"
#include "TestTempDir.h"

namespace
{

using QtRocket::ErrorCode;
using QtRocket::Test::TempDir;

TEST(FileIo, WritesAndReadsBytesAsTheyAre)
{
    const TempDir                dir;
    const std::filesystem::path  file = dir.resolve("data.bin");
    const std::vector<std::byte> data{std::byte{0}, std::byte{'\r'}, std::byte{'\n'},
                                      std::byte{0xFF}, std::byte{'a'}};
    ASSERT_TRUE(QtRocket::writeFile(file, data));

    const auto read = QtRocket::readFile(file);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(*read, data);

    const auto text = QtRocket::readTextFile(file);
    ASSERT_TRUE(text.has_value());
    EXPECT_EQ(*text, std::string("\0\r\n\xFF"
                                 "a",
                                 5));
    EXPECT_EQ(QtRocket::stringToBytes(*text), data);
    EXPECT_EQ(QtRocket::bytesToString(data), *text);
}

TEST(FileIo, AnEmptyFileReadsAsNothing)
{
    const TempDir dir;
    const auto    text = QtRocket::readTextFile(dir.write("empty.txt", ""));
    ASSERT_TRUE(text.has_value());
    EXPECT_TRUE(text->empty());
}

TEST(FileIo, AMissingFileIsAnIoError)
{
    const TempDir               dir;
    const std::filesystem::path missing = dir.resolve("none.txt");
    const auto                  read    = QtRocket::readFile(missing);
    ASSERT_FALSE(read.has_value());
    EXPECT_EQ(read.error().code, ErrorCode::IO);
    EXPECT_EQ(read.error().message,
              "cannot open '" + QtRocket::pathToUtf8(missing) + "' for reading");
}

TEST(FileIo, ADirectoryIsAnIoErrorOnEveryPlatform)
{
    // libstdc++ throws from the read, libc++ reads a directory as empty and MSVC fails to open it:
    // readFile() refuses it before any of that.
    const TempDir dir;
    const auto    read = QtRocket::readFile(dir.path());
    ASSERT_FALSE(read.has_value());
    EXPECT_EQ(read.error().code, ErrorCode::IO);
    EXPECT_EQ(read.error().message,
              "cannot read '" + QtRocket::pathToUtf8(dir.path()) + "': is a directory");

    const auto text = QtRocket::readTextFile(dir.path());
    ASSERT_FALSE(text.has_value());
    EXPECT_EQ(text.error().code, ErrorCode::IO);
}

TEST(FileIo, WritingIntoAMissingDirectoryIsAnIoError)
{
    const TempDir dir;
    const auto    written = QtRocket::writeTextFile(dir.resolve("none/file.txt"), "text");
    ASSERT_FALSE(written.has_value());
    EXPECT_EQ(written.error().code, ErrorCode::IO);
}

TEST(FileIo, PathToUtf8IsTheUtf8FormOnEveryPlatform)
{
    // u8"..." builds the path from UTF-8 everywhere; path::string() would give the Windows ANSI
    // code page's bytes (or throw) there.
    const std::filesystem::path path{u8"dir/résumé π.csv"};
    EXPECT_EQ(QtRocket::pathToUtf8(path.filename()), "r\xC3\xA9sum\xC3\xA9 \xCF\x80.csv");
    EXPECT_EQ(QtRocket::pathToUtf8(std::filesystem::path{}), "");
}

}  // namespace
