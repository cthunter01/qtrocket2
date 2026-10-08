#include "QtRocket/aero/lookup/CsvMachAoALookup.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestTempDir.h"

namespace
{

using QtRocket::ErrorCode;
using QtRocket::MachAoALookup;
using QtRocket::Result;
using QtRocket::Test::TempDir;
namespace CsvMachAoALookup = QtRocket::CsvMachAoALookup;

/// The drag columns (List.of("cd")).
[[nodiscard]] std::vector<std::string> cdColumns()
{
    return {"cd"};
}

/// The stability columns (List.of("cn", "cm", "cp")).
[[nodiscard]] std::vector<std::string> stabilityColumns()
{
    return {"cn", "cm", "cp"};
}

constexpr std::string_view kSeparatorHint =
    "Make sure the column is included and you are using the correct field separator.";

/// CsvMachAoALookup::parse() of @p text split into lines.
[[nodiscard]] Result<MachAoALookup> parseText(std::string_view                text,
                                              const std::vector<std::string>& columns = cdColumns(),
                                              char                            separator = ',')
{
    return CsvMachAoALookup::parse(CsvMachAoALookup::splitLines(text), columns, separator);
}

TEST(CsvMachAoALookup, SplitLinesAsReadAllLines)
{
    using Lines = std::vector<std::string>;
    EXPECT_EQ(CsvMachAoALookup::splitLines(""), Lines{});
    EXPECT_EQ(CsvMachAoALookup::splitLines("a"), Lines{"a"});
    EXPECT_EQ(CsvMachAoALookup::splitLines("a\n"), Lines{"a"});
    EXPECT_EQ(CsvMachAoALookup::splitLines("a\r\nb\rc\n\nd"), (Lines{"a", "b", "c", "", "d"}));
    EXPECT_EQ(CsvMachAoALookup::splitLines("\n"), Lines{""});
    EXPECT_EQ(CsvMachAoALookup::splitLines("a\r"), Lines{"a"});
}

TEST(CsvMachAoALookup, SkipsCommentsAndBlankLinesAndTrimsFields)
{
    const auto table = parseText(
        "# a drag table\n\n   \n  Mach , C_D  \n# comment\n 0 , 0.2 \n"
        "\t1,\t0.6\t\n");
    ASSERT_TRUE(table.has_value()) << table.error().toString();
    EXPECT_FALSE(table->hasAoA());
    EXPECT_EQ(table->interpolate(0.5, 0, "cd"), 0.4);
}

TEST(CsvMachAoALookup, ReadsWindowsAndOldMacLineEndings)
{
    const TempDir dir;
    const auto    crlf = CsvMachAoALookup::fromCsv(
        dir.write("crlf.csv", "mach,cd\r\n0,0.2\r\n1,0.6\r\n"), cdColumns());
    ASSERT_TRUE(crlf.has_value()) << crlf.error().toString();
    EXPECT_EQ(crlf->interpolate(1, 0, "cd"), 0.6);

    const auto cr =
        CsvMachAoALookup::fromCsv(dir.write("cr.csv", "mach,cd\r0,0.2\r1,0.6"), cdColumns());
    ASSERT_TRUE(cr.has_value()) << cr.error().toString();
    EXPECT_EQ(cr->interpolate(1, 0, "cd"), 0.6);
}

TEST(CsvMachAoALookup, AngleOfAttackColumnGivesAnglesToEveryRow)
{
    const auto table = parseText(
        "MACH,Angle Of Attack,CN,CM,CP,comment\n"
        "0,0,1,2,3,x\n0,10,2,3,4,y\n1,0,3,4,5,z\n1,10,4,5,6,w\n",
        stabilityColumns());
    ASSERT_TRUE(table.has_value()) << table.error().toString();
    EXPECT_TRUE(table->hasAoA());
    EXPECT_EQ(table->getMaxAoA(), 10);
    EXPECT_EQ(table->interpolate(0.5, 5, "cn"), 2.5);
    EXPECT_EQ(table->interpolate(0.5, 5, "cp"), 4.5);
}

TEST(CsvMachAoALookup, FirstOfTwoEqualHeaderNamesWins)
{
    const auto table = parseText("mach,cd,C_D\n0,0.1,0.9\n1,0.3,0.9\n");
    ASSERT_TRUE(table.has_value()) << table.error().toString();
    EXPECT_EQ(table->interpolate(1, 0, "cd"), 0.3);
}

TEST(CsvMachAoALookup, CustomSeparatorAndJavaNumberSyntax)
{
    const auto table = parseText("mach|cd\n+0|1e-1\n0x1p0|.5d\n", cdColumns(), '|');
    ASSERT_TRUE(table.has_value()) << table.error().toString();
    EXPECT_EQ(table->getMaxMach(), 1.0);
    EXPECT_EQ(table->interpolate(1, 0, "cd"), 0.5);
    EXPECT_EQ(table->interpolate(0, 0, "cd"), 0.1);
}

TEST(CsvMachAoALookup, HeaderErrorsAreJavasMessages)
{
    const auto noMach = parseText("speed,cd\n0,1\n");
    ASSERT_FALSE(noMach.has_value());
    EXPECT_EQ(noMach.error().code, ErrorCode::PARSE);
    EXPECT_EQ(noMach.error().message,
              "Lookup table header must contain a 'mach' column. " + std::string{kSeparatorHint});

    // Java reports the normalized name of the missing column.
    const auto noCm = parseText("mach,cn,cp\n0,1,2\n", {"CN", "C_M", "CP"});
    ASSERT_FALSE(noCm.has_value());
    EXPECT_EQ(noCm.error().message,
              "Lookup table header missing required column 'cm'. " + std::string{kSeparatorHint});

    // The wrong separator leaves one header name.
    const auto wrongSeparator = parseText("mach;cd\n0;1\n");
    ASSERT_FALSE(wrongSeparator.has_value());
    EXPECT_EQ(wrongSeparator.error().message,
              "Lookup table header must contain a 'mach' column. " + std::string{kSeparatorHint});
}

TEST(CsvMachAoALookup, RowErrorsAreJavasMessages)
{
    const auto shortRow = parseText("mach,cd\n0,1\n1\n");
    ASSERT_FALSE(shortRow.has_value());
    EXPECT_EQ(shortRow.error().code, ErrorCode::PARSE);
    EXPECT_EQ(shortRow.error().message, "Row missing value for column 'cd'");

    const auto badNumber = parseText("mach,cd\n0,abc\n");
    ASSERT_FALSE(badNumber.has_value());
    EXPECT_EQ(badNumber.error().message, "Illegal numeric value 'abc' in column 'cd'");

    const auto emptyField = parseText("mach,cd\n,1\n");
    ASSERT_FALSE(emptyField.has_value());
    EXPECT_EQ(emptyField.error().message, "Illegal numeric value '' in column 'mach'");

    const auto badAngle = parseText("mach,aoa,cd\n0,ten,1\n");
    ASSERT_FALSE(badAngle.has_value());
    EXPECT_EQ(badAngle.error().message, "Illegal numeric value 'ten' in column 'aoa'");
}

TEST(CsvMachAoALookup, EmptyTablesAreErrors)
{
    const auto nothing = parseText("# only a comment\n\n");
    ASSERT_FALSE(nothing.has_value());
    EXPECT_EQ(nothing.error().code, ErrorCode::PARSE);
    EXPECT_EQ(nothing.error().message, "Lookup table is missing a header row");

    const auto headerOnly = parseText("mach,cd\n");
    ASSERT_FALSE(headerOnly.has_value());
    EXPECT_EQ(headerOnly.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(headerOnly.error().message, "No lookup data added");

    const auto noColumns = parseText("mach,cd\n0,1\n", {});
    ASSERT_FALSE(noColumns.has_value());
    EXPECT_EQ(noColumns.error().message, "At least one value column is required");
}

TEST(CsvMachAoALookup, RequiringTheAngleColumnFailsAtTheFirstRow)
{
    // The angle is never a value, so the builder misses it (Java throws from addData()).
    const auto table = parseText("mach,aoa,cd\n0,0,1\n0,x,2\n", {"cd", "Angle of attack"});
    ASSERT_FALSE(table.has_value());
    EXPECT_EQ(table.error().message, "Value for column 'aoa' missing");
}

TEST(CsvMachAoALookup, UnreadableFilesAreIoErrors)
{
    const TempDir dir;
    const auto    missing = CsvMachAoALookup::fromCsv(dir.resolve("none.csv"), cdColumns());
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code, ErrorCode::IO);
    EXPECT_EQ(missing.error().message,
              "Failed to read lookup table from " + QtRocket::pathToUtf8(dir.resolve("none.csv")));

    const auto invalidUtf8 =
        CsvMachAoALookup::fromCsv(dir.write("latin1.csv", "mach,cd\n0,1 \xE9\n"), cdColumns());
    ASSERT_FALSE(invalidUtf8.has_value());
    EXPECT_EQ(invalidUtf8.error().code, ErrorCode::IO);
}

TEST(CsvMachAoALookup, ADirectoryIsAnIoError)
{
    // Java: UncheckedIOException "Failed to read lookup table from <path>" (the cause an
    // IOException "Is a directory"); never an exception or a parse error here, on any platform.
    const TempDir dir;
    const auto    table = CsvMachAoALookup::fromCsv(dir.path(), cdColumns());
    ASSERT_FALSE(table.has_value());
    EXPECT_EQ(table.error().code, ErrorCode::IO);
    EXPECT_EQ(table.error().message,
              "Failed to read lookup table from " + QtRocket::pathToUtf8(dir.path()));
}

TEST(CsvMachAoALookup, TheIoErrorNamesThePathInUtf8)
{
    const TempDir               dir;
    const std::filesystem::path missing = dir.resolve(std::filesystem::path{u8"tabl\u00E9.csv"});
    const auto                  table   = CsvMachAoALookup::fromCsv(missing, cdColumns());
    ASSERT_FALSE(table.has_value());
    EXPECT_EQ(table.error().code, ErrorCode::IO);
    EXPECT_TRUE(table.error().message.ends_with("tabl\xC3\xA9.csv")) << table.error().message;
}

// Not OpenRocket's, which reads a file of any size: a design file names the CSV file of its
// lookup tables, so the read is bounded.
TEST(CsvMachAoALookup, AFileBeyondTheLimitIsAnIoError)
{
    const TempDir               dir;
    const std::string           text = "mach,cd\n0,0.2\n1,0.6\n";
    const std::filesystem::path file = dir.write("table.csv", text);

    const auto atTheLimit = CsvMachAoALookup::fromCsv(file, cdColumns(), ',', text.size());
    ASSERT_TRUE(atTheLimit.has_value());
    EXPECT_EQ(atTheLimit->interpolate(0.5, 0, "cd"), 0.4);

    const auto beyond = CsvMachAoALookup::fromCsv(file, cdColumns(), ',', text.size() - 1);
    ASSERT_FALSE(beyond.has_value());
    EXPECT_EQ(beyond.error().code, ErrorCode::IO);
    EXPECT_EQ(beyond.error().message,
              "Failed to read lookup table from " + QtRocket::pathToUtf8(file));

    // Without a limit of the caller's it is 32 MiB.
    EXPECT_EQ(CsvMachAoALookup::kMaxFileBytes, 33554432U);
    EXPECT_TRUE(CsvMachAoALookup::fromCsv(file, cdColumns()).has_value());
}

/// Whether this machine has the device @p device (a POSIX system).
[[nodiscard]] bool hasDevice(const std::filesystem::path& device)
{
    std::error_code error;
    return std::filesystem::is_character_file(device, error);
}

/// The failure of reading the table of @p path: "IO: <message>".
[[nodiscard]] std::string failureOf(const std::filesystem::path& path)
{
    const auto table = CsvMachAoALookup::fromCsv(path, cdColumns());
    return table.has_value()
               ? "a table"
               : std::string(toString(table.error().code)) + ": " + table.error().message;
}

// Not OpenRocket's, which opens whatever the path names: a device is not opened, since reading
// one may never end (/dev/zero) and opening one may block. (OpenRocket reads /dev/null as an
// empty file, "Lookup table is missing a header row".)
TEST(CsvMachAoALookup, ADeviceIsNotRead)
{
    if (!hasDevice("/dev/zero") || !hasDevice("/dev/null"))
    {
        GTEST_SKIP() << "no /dev/zero and /dev/null here";
    }
    EXPECT_EQ(failureOf("/dev/zero"), "IO: Failed to read lookup table from /dev/zero");
    EXPECT_EQ(failureOf("/dev/null"), "IO: Failed to read lookup table from /dev/null");
}

}  // namespace
