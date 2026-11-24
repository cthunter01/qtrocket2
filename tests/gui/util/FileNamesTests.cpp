// The file names of OpenRocket's file questions (FileHelper): the forced extension, Java's
// absolute path and the CSV questions' name filters.

#include "QtRocket/gui/util/FileNames.h"

#include <filesystem>

#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/util/FileIo.h"
#include "TestTempDir.h"

namespace QtRocket::Gui
{

namespace
{

using FileNamesTest = Test::GuiTest;

/// FileHelper.forceExtension(@p file, "ork").
[[nodiscard]] std::filesystem::path forceOrk(const std::filesystem::path& file)
{
    return FileNames::forceExtension(file, QStringLiteral("ork"));
}

/// FileHelper.forceExtension(@p file, "csv").
[[nodiscard]] std::filesystem::path forceCsv(const std::filesystem::path& file)
{
    return FileNames::forceExtension(file, QStringLiteral("csv"));
}

}  // namespace

TEST_F(FileNamesTest, ForcesTheExtensionAsFileHelperDoes)
{
    // Moved here with the function from DocumentFileOperationsTests (the design files' "ork").
    // FileHelper.forceExtension(file, "ork"), the Java probe's FORCE lines (/d is the directory).
    const QtRocket::Test::TempDir directory;
    const std::filesystem::path&  d = directory.path();
    EXPECT_EQ(forceOrk(d / "x"), d / "x.ork");
    EXPECT_EQ(forceOrk(d / "x.ork"), d / "x.ork");
    EXPECT_EQ(forceOrk(d / "x.ORK"), d / "x.ORK");
    EXPECT_EQ(forceOrk(d / "x.Ork"), d / "x.Ork");
    EXPECT_EQ(forceOrk(d / "x.ork.gz"), d / "x.ork.gz.ork");
    EXPECT_EQ(forceOrk(d / "x.ORK.GZ"), d / "x.ORK.GZ.ork");
    EXPECT_EQ(forceOrk(d / "x.v2"), d / "x.v2.ork");
    EXPECT_EQ(forceOrk(d / ".ork"), d / ".ork.ork");
    EXPECT_EQ(forceOrk(d / "x."), d / "x..ork");
    EXPECT_EQ(forceOrk(d / "x..ork"), d / "x..ork");
    EXPECT_EQ(forceOrk(d / ".hidden"), d / ".hidden.ork");
    EXPECT_EQ(forceOrk(d / "x.rkt"), d / "x.rkt.ork");
    EXPECT_EQ(forceOrk(d / "x.orkk"), d / "x.orkk.ork");
    EXPECT_EQ(forceOrk(d / "x .ork"), d / "x .ork");
    EXPECT_EQ(forceOrk(d / "a.b.c"), d / "a.b.c.ork");
    // Java lowercases the extension in Locale.ENGLISH: the Kelvin sign is a 'k'; the dotted
    // capital I is not an 'o'.
    const std::filesystem::path kelvin = d / QtStrings::toPath(QStringLiteral(u"x.or\u212A"));
    EXPECT_EQ(forceOrk(kelvin), kelvin);
    EXPECT_EQ(forceOrk(d / QtStrings::toPath(QStringLiteral(u"x.\u0130RK"))),
              d / QtStrings::toPath(QStringLiteral(u"x.\u0130RK.ork")));
    EXPECT_EQ(forceOrk(d / QtStrings::toPath(QStringLiteral(u"\u00E9t\u00E9.ork"))),
              d / QtStrings::toPath(QStringLiteral(u"\u00E9t\u00E9.ork")));
    // A dot in the directory is no extension; an extension given with its dot is added as it is.
    EXPECT_EQ(forceOrk(d / "d.dir" / "x"), d / "d.dir" / "x.ork");
    EXPECT_EQ(FileNames::forceExtension(d / "x", QStringLiteral(".ork")), d / "x.ork");
    // A relative file: unchanged with the extension, else made absolute as Java's
    // File.getAbsolutePath() makes it.
    EXPECT_EQ(forceOrk("x.ork"), std::filesystem::path("x.ork"));
    EXPECT_EQ(forceOrk("x"), std::filesystem::current_path() / "x.ork");
}

TEST_F(FileNamesTest, ForcesTheCsvExtension)
{
    // Moved here with the function from FreeformFinSetConfigTests (the fin points' export), in a
    // directory of an absolute path: FreeformFinSetConfig's own copy kept a relative path
    // relative, where Java's (and now this one) makes it absolute (the test above).
    // FileHelper.forceExtension(): unless the name ends with ".csv" (ignoring case).
    const QtRocket::Test::TempDir directory;
    const std::filesystem::path   dir = directory.path() / "dir";
    EXPECT_EQ(forceCsv(dir / "fin"), dir / "fin.csv");
    EXPECT_EQ(forceCsv(dir / "fin.CSV"), dir / "fin.CSV");
    EXPECT_EQ(forceCsv(dir / "a.b.csv"), dir / "a.b.csv");
    EXPECT_EQ(forceCsv(dir / "fin.txt"), dir / "fin.txt.csv");
    // A name that starts with its only dot has no extension.
    EXPECT_EQ(forceCsv(dir / ".csv"), dir / ".csv.csv");
    // The forced name of a relative file, made absolute.
    EXPECT_EQ(forceCsv(pathFromUtf8("dir/fin")),
              (std::filesystem::current_path() / pathFromUtf8("dir/fin.csv")).make_preferred());
    EXPECT_EQ(forceCsv(pathFromUtf8("dir/fin.csv")), pathFromUtf8("dir/fin.csv"));
}

TEST_F(FileNamesTest, MakesAFileAbsoluteAsJavasFileDoes)
{
    // File.getAbsolutePath(): resolved against the current directory, nothing of the name
    // changed (no dots or spaces dropped), with the platform's separators.
    EXPECT_EQ(FileNames::absoluteFile(pathFromUtf8("designs/x .ork.")),
              (std::filesystem::current_path() / pathFromUtf8("designs/x .ork.")).make_preferred());
    const QtRocket::Test::TempDir directory;
    const std::filesystem::path   absolute = directory.path() / "x.ork";
    EXPECT_EQ(FileNames::absoluteFile(absolute), std::filesystem::path(absolute).make_preferred());
}

TEST_F(FileNamesTest, TheCsvQuestionsFiltersAreFileHelpersAndSwingsAcceptAllFilter)
{
    // FileHelper.CSV_FILTER and Swing's FileChooser.acceptAllFileFilterText, as Qt name filters.
    EXPECT_EQ(FileNames::csvFilter(), QStringLiteral("Comma Separated Files (*.csv)"));
    EXPECT_EQ(FileNames::allFilesFilter(), QStringLiteral("All Files (*)"));
}

}  // namespace QtRocket::Gui
