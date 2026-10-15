#include "QtRocket/util/FileIo.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
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
    EXPECT_FALSE(QtRocket::writeFile(dir.resolve("none/file.bin"), {}).has_value());
}

// writeFile() hands the stream its bytes in pieces of 1 MiB: data of several pieces, with a piece
// that is not whole, comes back as it was; so does a text, byte for byte, a NUL included.
TEST(FileIo, WritesDataOfSeveralPiecesAndTextAsTheyAre)
{
    const TempDir          dir;
    std::vector<std::byte> data((std::size_t{3} << 20U) + 17);
    std::uint32_t          state = 7;
    for (std::byte& b : data)
    {
        state = (state * 1664525U) + 1013904223U;
        b     = static_cast<std::byte>(state >> 24U);
    }
    ASSERT_TRUE(QtRocket::writeFile(dir.resolve("big.bin"), data));
    EXPECT_EQ(QtRocket::readFile(dir.resolve("big.bin")).value_or(std::vector<std::byte>{}), data);

    std::string text = "a\r\nb\n";
    text.push_back('\0');
    text += "c\xFF";
    ASSERT_TRUE(QtRocket::writeTextFile(dir.resolve("text.txt"), text));
    EXPECT_EQ(QtRocket::readTextFile(dir.resolve("text.txt")).value_or(""), text);
    ASSERT_TRUE(QtRocket::writeTextFile(dir.resolve("text.txt"), "shorter"));
    EXPECT_EQ(QtRocket::readTextFile(dir.resolve("text.txt")).value_or(""), "shorter");
}

/// The names of the entries of @p directory, sorted.
[[nodiscard]] std::vector<std::string> namesIn(const std::filesystem::path& directory)
{
    std::vector<std::string> names;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(directory))
    {
        names.push_back(QtRocket::pathToUtf8(entry.path().filename()));
    }
    std::ranges::sort(names);
    return names;
}

// replaceFile() replaces a file with new data and makes a missing one; nothing else is left in
// the directory.
TEST(FileIo, ReplaceFileReplacesOrMakesTheFile)
{
    const TempDir               dir;
    const std::filesystem::path file = dir.write("design.ork", "the old design, which is longer");
    ASSERT_TRUE(QtRocket::replaceFile(file, QtRocket::stringToBytes("new")));
    EXPECT_EQ(QtRocket::readTextFile(file).value_or(""), "new");
    ASSERT_TRUE(QtRocket::replaceFile(dir.resolve("other.ork"), QtRocket::stringToBytes("other")));
    EXPECT_EQ(QtRocket::readTextFile(dir.resolve("other.ork")).value_or(""), "other");
    EXPECT_EQ(namesIn(dir.path()), (std::vector<std::string>{"design.ork", "other.ork"}));
}

// A target that cannot be replaced (a directory) stays as it was, with what it holds, and the
// temporary file is removed again.
TEST(FileIo, AFailedReplaceLeavesTheTargetAndNoTemporaryFile)
{
    const TempDir               dir;
    const std::filesystem::path target = dir.resolve("target");
    std::ignore                        = dir.write("target/inside.txt", "inside");
    const QtRocket::Result<void> replaced =
        QtRocket::replaceFile(target, QtRocket::stringToBytes("new"));
    ASSERT_FALSE(replaced.has_value());
    EXPECT_EQ(replaced.error().code, ErrorCode::IO);
    EXPECT_TRUE(replaced.error().message.starts_with("cannot replace '" +
                                                     QtRocket::pathToUtf8(target) + "': "))
        << replaced.error().message;
    EXPECT_TRUE(std::filesystem::is_directory(target));
    EXPECT_EQ(QtRocket::readTextFile(target / "inside.txt").value_or(""), "inside");
    EXPECT_EQ(namesIn(dir.path()), std::vector<std::string>{"target"});
}

// A directory that does not exist takes no temporary file: the failure names it, and the data
// of replaceFileWith() are not asked for (Java makes the temporary file before it writes).
TEST(FileIo, ReplacingAFileInAMissingDirectoryFailsBeforeTheDataAreAsked)
{
    const TempDir                dir;
    const std::filesystem::path  missing = dir.resolve("none");
    const QtRocket::Result<void> replaced =
        QtRocket::replaceFile(missing / "file.ork", QtRocket::stringToBytes("x"));
    ASSERT_FALSE(replaced.has_value());
    EXPECT_EQ(replaced.error().code, ErrorCode::IO);
    EXPECT_EQ(replaced.error().message,
              "cannot create a temporary file in '" + QtRocket::pathToUtf8(missing) + "'");

    bool                         asked = false;
    const QtRocket::Result<void> with  = QtRocket::replaceFileWith(
        missing / "file.ork", [&asked] -> QtRocket::Result<std::vector<std::byte>> {
            asked = true;
            return std::vector<std::byte>{};
        });
    EXPECT_FALSE(with.has_value());
    EXPECT_FALSE(asked);
    EXPECT_FALSE(std::filesystem::exists(missing));
}

/// replaceFileWith() of @p file with data that fail ("no data"): the Error, and the names of the
/// files of the directory of @p file when the data were asked for, joined by spaces.
[[nodiscard]] std::pair<QtRocket::Error, std::string> failingReplace(
    const std::filesystem::path& file)
{
    std::string                  seen;
    const QtRocket::Result<void> replaced =
        QtRocket::replaceFileWith(file, [&seen, &file] -> QtRocket::Result<std::vector<std::byte>> {
            for (const std::string& name : namesIn(file.parent_path()))
            {
                seen += name + " ";
            }
            return QtRocket::fail(ErrorCode::NOT_FOUND, "no data");
        });
    return {replaced.has_value() ? QtRocket::Error{} : replaced.error(), seen};
}

// replaceFileWith() asks for the data once the temporary file exists in the target's directory,
// "QtRocket-<16 hexadecimal digits>.tmp"; a failure of the data is passed on, the temporary
// file removed and the target left as it was.
TEST(FileIo, ReplaceFileWithAsksForTheDataWhenTheTemporaryFileExists)
{
    const TempDir               dir;
    const std::filesystem::path file = dir.write("design.ork", "old");
    const auto [error, seen]         = failingReplace(file);
    EXPECT_EQ(error.code, ErrorCode::NOT_FOUND);
    EXPECT_EQ(error.message, "no data");
    EXPECT_EQ(seen.size(), 41U) << seen;
    EXPECT_TRUE(seen.starts_with("QtRocket-")) << seen;
    EXPECT_EQ(seen.find_first_not_of("0123456789abcdef", 9), 25U) << seen;
    EXPECT_EQ(seen.substr(25), ".tmp design.ork ");
    EXPECT_EQ(QtRocket::readTextFile(file).value_or(""), "old");
    EXPECT_EQ(namesIn(dir.path()), std::vector<std::string>{"design.ork"});

    ASSERT_TRUE(QtRocket::replaceFileWith(file, [] {
        return QtRocket::Result<std::vector<std::byte>>(QtRocket::stringToBytes("new"));
    }));
    EXPECT_EQ(QtRocket::readTextFile(file).value_or(""), "new");
}

// A file named without a directory is replaced in the current directory.
TEST(FileIo, ReplaceFileOfANameAloneWritesInTheCurrentDirectory)
{
    const TempDir                               dir;
    const QtRocket::Test::CurrentDirectoryGuard current(dir.path());
    ASSERT_TRUE(QtRocket::replaceFile("plain.ork", QtRocket::stringToBytes("plain")));
    EXPECT_EQ(QtRocket::readTextFile(dir.resolve("plain.ork")).value_or(""), "plain");
    EXPECT_EQ(namesIn(dir.path()), std::vector<std::string>{"plain.ork"});
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

// The opposite of pathToUtf8(): the text is UTF-8 on every platform.
TEST(FileIo, PathFromUtf8ReadsTheTextAsUtf8OnEveryPlatform)
{
    const std::string           text = "dir/r\xC3\xA9sum\xC3\xA9 \xCF\x80.csv";
    const std::filesystem::path path = QtRocket::pathFromUtf8(text);
    EXPECT_EQ(QtRocket::pathToUtf8(path), text);
    EXPECT_EQ(QtRocket::pathToUtf8(path.filename()), "r\xC3\xA9sum\xC3\xA9 \xCF\x80.csv");
    EXPECT_TRUE(QtRocket::pathFromUtf8("").empty());
    EXPECT_EQ(QtRocket::pathFromUtf8("plain.csv"), std::filesystem::path("plain.csv"));
}

// A name out of a file may hold any bytes; none of them makes the conversion throw.
TEST(FileIo, PathFromUtf8ReadsABadSequenceAsTheReplacementCharacter)
{
    EXPECT_EQ(QtRocket::pathToUtf8(QtRocket::pathFromUtf8("a\xFF.csv")), "a\xEF\xBF\xBD.csv");
    EXPECT_EQ(QtRocket::pathToUtf8(QtRocket::pathFromUtf8("cut\xC3")), "cut\xEF\xBF\xBD");
}

/// withoutRedundantSeparators() of @p text as the path spells it, with '/' for the platform's
/// separator. (Not through generic_string(), which may leave out separators by itself.)
[[nodiscard]] std::string spelledAsJava(std::string_view text)
{
    std::string spelled =
        QtRocket::pathToUtf8(QtRocket::withoutRedundantSeparators(std::filesystem::path(text)));
    std::ranges::replace(spelled, '\\', '/');
    return spelled;
}

// Java's File and Path keep no separator twice and none at the end; "." and ".." stay.
TEST(FileIo, WithoutRedundantSeparatorsSpellsAPathAsJavaDoes)
{
    EXPECT_EQ(spelledAsJava("a//b///c.csv"), "a/b/c.csv");
    EXPECT_EQ(spelledAsJava("a/b/"), "a/b");
    EXPECT_EQ(spelledAsJava("a/b//"), "a/b");
    EXPECT_EQ(spelledAsJava("/a//b"), "/a/b");
    EXPECT_EQ(spelledAsJava("a/./b/../c"), "a/./b/../c");
    EXPECT_EQ(spelledAsJava("rocket.ork"), "rocket.ork");
    EXPECT_EQ(spelledAsJava("/"), "/");
    EXPECT_EQ(spelledAsJava(""), "");
}

// A root written with more separators than it needs is the root: Java's Path.of("///") prints
// "/". (Two separators and a name are left out here: on Windows they start a network path.)
TEST(FileIo, WithoutRedundantSeparatorsSpellsARunOfSeparatorsAtTheRootAsOne)
{
    EXPECT_EQ(spelledAsJava("///"), "/");
    EXPECT_EQ(spelledAsJava("////"), "/");
    EXPECT_EQ(spelledAsJava("///a//b/"), "/a/b");
    EXPECT_EQ(spelledAsJava("///a"), "/a");
    EXPECT_EQ(spelledAsJava("/a/"), "/a");
    EXPECT_EQ(spelledAsJava("a"), "a");
    EXPECT_EQ(spelledAsJava("./"), ".");
}

}  // namespace
