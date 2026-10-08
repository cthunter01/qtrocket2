#include "QtRocket/util/FileIo.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
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

// ---- the bounded read ---------------------------------------------------------------------------

/// What readFile(@p file, @p maxBytes) gives: "bytes <text>", or "<code>: <message>".
[[nodiscard]] std::string readBounded(const std::filesystem::path& file, std::size_t maxBytes)
{
    const auto read = QtRocket::readFile(file, maxBytes);
    if (read.has_value())
    {
        return "bytes " + QtRocket::bytesToString(*read);
    }
    return std::string(QtRocket::toString(read.error().code)) + ": " + read.error().message;
}

TEST(FileIo, ABoundedReadTakesAFileUpToItsLimit)
{
    const TempDir               dir;
    const std::filesystem::path file = dir.write("five.txt", "12345");
    EXPECT_EQ(readBounded(file, 100), "bytes 12345");
    // The limit itself is allowed.
    EXPECT_EQ(readBounded(file, 5), "bytes 12345");
    // Java: FileUtils.readBytes(InputStream, int), "Input exceeds maximum size of 4 bytes".
    EXPECT_EQ(readBounded(file, 4), "IO: Input exceeds maximum size of 4 bytes");
    EXPECT_EQ(readBounded(file, 0), "IO: Input exceeds maximum size of 0 bytes");
    EXPECT_EQ(readBounded(dir.write("empty.txt", ""), 0), "bytes ");
}

TEST(FileIo, ABoundedReadFailsAsAnUnboundedOneForWhatIsNoFile)
{
    const TempDir               dir;
    const std::filesystem::path missing = dir.resolve("none.txt");
    EXPECT_EQ(readBounded(missing, 100),
              "IO: cannot open '" + QtRocket::pathToUtf8(missing) + "' for reading");
    EXPECT_EQ(readBounded(dir.path(), 100),
              "IO: cannot read '" + QtRocket::pathToUtf8(dir.path()) + "': is a directory");
}

/// A file under @p dir that says it holds a terabyte and holds nothing (a sparse file), or
/// 64 MiB where the file system does not make one; an empty path when neither can be made.
[[nodiscard]] std::filesystem::path makeHugeFile(const TempDir& dir)
{
    const std::filesystem::path file = dir.write("huge.bin", "x");
    std::error_code             error;
    std::filesystem::resize_file(file, std::uintmax_t{1} << 40U, error);
    if (error)
    {
        std::filesystem::resize_file(file, std::uintmax_t{64} * 1024 * 1024, error);
    }
    return error ? std::filesystem::path() : file;
}

// Hostile input: a file that says it is huge is refused by what it says, before a byte is read
// or reserved. A terabyte that is nothing but a size would otherwise end in std::bad_alloc.
TEST(FileIo, ABoundedReadRefusesAHugeFileBeforeItReadsIt)
{
    const TempDir               dir;
    const std::filesystem::path file = makeHugeFile(dir);
    ASSERT_FALSE(file.empty());
    EXPECT_EQ(readBounded(file, 1024), "IO: Input exceeds maximum size of 1024 bytes");
}

/// Whether this machine has the device @p device (a POSIX system).
[[nodiscard]] bool hasDevice(const std::filesystem::path& device)
{
    std::error_code error;
    return std::filesystem::is_character_file(device, error);
}

// Hostile input: a source that has no size and no end. The read stops one byte beyond the
// limit.
TEST(FileIo, ABoundedReadGivesUpOnASourceWithoutEnd)
{
    if (!hasDevice("/dev/zero"))
    {
        GTEST_SKIP() << "no /dev/zero here";
    }
    EXPECT_EQ(readBounded("/dev/zero", 4096), "IO: Input exceeds maximum size of 4096 bytes");
    EXPECT_EQ(readBounded("/dev/zero", 0), "IO: Input exceeds maximum size of 0 bytes");
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

// Java's File.getAbsolutePath(): the current directory in front of a relative path, without a
// look at the file system; an absolute path as it is; the current directory for an empty path.
TEST(FileIo, AbsolutePathResolvesAgainstTheCurrentDirectory)
{
    const std::filesystem::path cwd = std::filesystem::current_path();

    EXPECT_EQ(QtRocket::absolutePath("no-such-directory/rocket.ork"),
              cwd / "no-such-directory" / "rocket.ork");
    EXPECT_EQ(QtRocket::absolutePath(cwd / "a" / "rocket.ork"), cwd / "a" / "rocket.ork");
    EXPECT_TRUE(QtRocket::absolutePath("rocket.ork").is_absolute());
    // std::filesystem::absolute("") is an error on some platforms.
    EXPECT_EQ(QtRocket::absolutePath(std::filesystem::path{}), cwd);
}

}  // namespace
