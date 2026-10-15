// Tests of cli/CliOutput.h. QtRocket's own.

#include "QtRocket/cli/CliOutput.h"

#include <filesystem>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/FileIo.h"
#include "TestTempDir.h"
#include "cli/CliRunSupport.h"

namespace
{

using QtRocket::CliOutput;
using QtRocket::WarningSet;
using QtRocket::Test::CFile;
using QtRocket::Test::CFileMode;
using QtRocket::Test::openCFile;
using QtRocket::Test::TempDir;

TEST(CliOutput, WithoutSinksItKeepsWhatWasWritten)
{
    CliOutput output;
    output.outLine("apogee: {} m", 50.5);
    output.writeOut("x");
    output.errLine("warning: {}", "text");
    EXPECT_EQ(output.out(), "apogee: 50.5 m\nx");
    EXPECT_EQ(output.err(), "warning: text\n");
}

TEST(CliOutput, WithSinksItHandsEveryTextOnAndKeepsNothing)
{
    std::string out;
    std::string err;
    CliOutput   output([&out](std::string_view text) { out += text; },
                       [&err](std::string_view text) { err += text; });
    output.outLine("{}", 1);
    output.errLine("{}", 2);
    EXPECT_EQ(out, "1\n");
    EXPECT_EQ(err, "2\n");
    EXPECT_TRUE(output.out().empty());
    EXPECT_TRUE(output.err().empty());
}

// The executable's output: each text printed at once, its bytes as they are ('\n' alone; the
// streams are binary here, as the executable's are on Windows), and nothing kept.
TEST(CliOutput, WithCStreamsItPrintsEveryTextAndKeepsNothing)
{
    const TempDir               directory;
    const std::filesystem::path outFile = directory.resolve("out.txt");
    const std::filesystem::path errFile = directory.resolve("err.txt");
    {
        const CFile out = openCFile(outFile, CFileMode::WRITE);
        const CFile err = openCFile(errFile, CFileMode::WRITE);
        ASSERT_NE(out, nullptr);
        ASSERT_NE(err, nullptr);
        CliOutput output(out.get(), err.get());
        output.outLine("apogee: {} m", 50.5);
        output.writeOut("x");
        output.errLine("warning: {}", "text");
        EXPECT_FALSE(output.outFailed());
        EXPECT_TRUE(output.out().empty());
        EXPECT_TRUE(output.err().empty());
    }
    EXPECT_EQ(QtRocket::readTextFile(outFile), "apogee: 50.5 m\nx");
    EXPECT_EQ(QtRocket::readTextFile(errFile), "warning: text\n");
}

// A standard output that cannot be written (here a stream opened for reading, whose every write
// fails at once) makes nothing throw: the failure is remembered, and the standard error still
// works.
TEST(CliOutput, WithCStreamsAnOutputThatCannotBeWrittenIsRemembered)
{
    const TempDir               directory;
    const std::filesystem::path readOnly = directory.write("read-only.txt", "");
    const std::filesystem::path errFile  = directory.resolve("err.txt");
    {
        const CFile out = openCFile(readOnly, CFileMode::READ);
        const CFile err = openCFile(errFile, CFileMode::WRITE);
        ASSERT_NE(out, nullptr);
        ASSERT_NE(err, nullptr);
        CliOutput output(out.get(), err.get());
        output.outLine("first");
        EXPECT_TRUE(output.outFailed());
        output.outLine("second");
        output.errLine("still written");
        EXPECT_TRUE(output.outFailed());
    }
    EXPECT_EQ(QtRocket::readTextFile(readOnly), "");
    EXPECT_EQ(QtRocket::readTextFile(errFile), "still written\n");

    // Neither the outputs that keep the text nor those with sinks ever fail so.
    CliOutput kept;
    kept.outLine("x");
    EXPECT_FALSE(kept.outFailed());
}

#ifdef __linux__
// A full disk: the text fits into the stream's buffer, and the flush fails.
TEST(CliOutput, WithCStreamsAFullDiskIsRemembered)
{
    const CFile full = openCFile("/dev/full", CFileMode::WRITE);
    ASSERT_NE(full, nullptr);
    const TempDir directory;
    const CFile   err = openCFile(directory.resolve("err.txt"), CFileMode::WRITE);
    ASSERT_NE(err, nullptr);
    CliOutput output(full.get(), err.get());
    output.outLine("a short line");
    EXPECT_TRUE(output.outFailed());
}
#endif

TEST(CliOutput, WarningsGoToTheStandardErrorOneALine)
{
    WarningSet warnings;
    warnings.add(std::string_view("First."));
    warnings.add(std::string_view("Second."));
    CliOutput output;
    output.warningLines(warnings);
    EXPECT_EQ(output.err(), "warning: First.\nwarning: Second.\n");
    EXPECT_TRUE(output.out().empty());
}

}  // namespace
