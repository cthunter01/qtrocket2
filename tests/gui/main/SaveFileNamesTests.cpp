#include "QtRocket/gui/main/SaveFileNames.h"

#include <filesystem>
#include <optional>

#include <QChar>
#include <QDir>
#include <QString>
#include <QtTypes>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "TestTempDir.h"

// SaveFileChooser's rules about names (getFilenameInput(), setSelectedFile(), approveSelection())
// and FileUtils.getIllegalFilenameChar(), against the Java probe of DesignFileSaveAsFileChooser
// with the default directory <work>/default (VerifyChooserProbe, handoff/m2-probes/m2r3-verify-P).

namespace QtRocket::Gui
{

namespace
{

using SaveFileNamesTest = Gui::Test::GuiTest;

/// @p name with the platform's separators.
[[nodiscard]] QString native(const QString& name)
{
    return QDir::toNativeSeparators(name);
}

/// @p text without its first separator of the platform (String.replaceFirst(File.separator)).
[[nodiscard]] QString withoutFirstSeparator(QString text)
{
    const qsizetype at = text.indexOf(QDir::separator());
    if (at >= 0)
    {
        text.remove(at, 1);
    }
    return text;
}

TEST_F(SaveFileNamesTest, TheIllegalCharactersAreFileUtilssInTheirOrder)
{
    EXPECT_EQ(SaveFileNames::illegalCharacter(QStringLiteral("ab.ork")), std::nullopt);
    EXPECT_EQ(SaveFileNames::illegalCharacter(QString()), std::nullopt);
    EXPECT_EQ(SaveFileNames::illegalCharacter(QStringLiteral("a:b.ork")), QChar(u':'));
    EXPECT_EQ(SaveFileNames::illegalCharacter(QStringLiteral("x|y.ork")), QChar(u'|'));
    EXPECT_EQ(SaveFileNames::illegalCharacter(QStringLiteral("a\\b.ork")), QChar(u'\\'));
    EXPECT_EQ(SaveFileNames::illegalCharacter(QStringLiteral("q?.ork")), QChar(u'?'));
    EXPECT_EQ(SaveFileNames::illegalCharacter(QStringLiteral("s*\"<>.ork")), QChar(u'*'));
    EXPECT_EQ(SaveFileNames::illegalCharacter(QStringLiteral("t\"<>.ork")), QChar(u'"'));
    EXPECT_EQ(SaveFileNames::illegalCharacter(QStringLiteral("u><.ork")), QChar(u'<'));
    EXPECT_EQ(SaveFileNames::illegalCharacter(QStringLiteral("v>.ork")), QChar(u'>'));
    // ILLEGAL_CHARS' order, not the name's: '/' before ':'.
    EXPECT_EQ(SaveFileNames::illegalCharacter(QStringLiteral("a:b/c")), QChar(u'/'));
}

TEST_F(SaveFileNamesTest, TheNameOfAFileIsItsPathRelativeToTheChoosersDirectory)
{
    const QtRocket::Test::TempDir work;
    const std::filesystem::path   defaultDirectory = work.resolve("default");
    // <work>/default/x.ork: "x.ork", proposed.
    EXPECT_EQ(SaveFileNames::filenameInput(defaultDirectory / "x.ork", defaultDirectory),
              QStringLiteral("x.ork"));
    EXPECT_TRUE(SaveFileNames::isProposable(defaultDirectory / "x.ork", defaultDirectory));
    // <work>/default/sub/z.ork: "sub/z.ork", not proposed.
    EXPECT_EQ(SaveFileNames::filenameInput(defaultDirectory / "sub" / "z.ork", defaultDirectory),
              native(QStringLiteral("sub/z.ork")));
    EXPECT_FALSE(SaveFileNames::isProposable(defaultDirectory / "sub" / "z.ork", defaultDirectory));
    // <work>/default/a:b.ork: "a:b.ork", not proposed.
    EXPECT_EQ(SaveFileNames::filenameInput(defaultDirectory / "a:b.ork", defaultDirectory),
              QStringLiteral("a:b.ork"));
    EXPECT_FALSE(SaveFileNames::isProposable(defaultDirectory / "a:b.ork", defaultDirectory));
    // <work>/other/y.ork: its whole path (canonical, as it does not start with the directory's)
    // without the first separator, not proposed.
    const std::filesystem::path other = work.resolve("other") / "y.ork";
    EXPECT_EQ(SaveFileNames::filenameInput(other, defaultDirectory),
              withoutFirstSeparator(
                  SaveFileNames::absolutePathText(std::filesystem::weakly_canonical(other))));
    EXPECT_FALSE(SaveFileNames::isProposable(other, defaultDirectory));
    // <work>/defaultX/s.ork: "X" + "/s.ork" without its separator, "Xs.ork": proposed.
    const std::filesystem::path sibling = work.resolve("defaultX") / "s.ork";
    EXPECT_EQ(SaveFileNames::filenameInput(sibling, defaultDirectory), QStringLiteral("Xs.ork"));
    EXPECT_TRUE(SaveFileNames::isProposable(sibling, defaultDirectory));
    // Spaces and control characters at the ends of the name go (String.trim()), others stay.
    EXPECT_EQ(SaveFileNames::filenameInput(defaultDirectory / "x.ork \t", defaultDirectory),
              QStringLiteral("x.ork"));
    EXPECT_EQ(
        SaveFileNames::filenameInput(
            defaultDirectory / QtStrings::toPath(QStringLiteral(u"x.ork\u00A0")), defaultDirectory),
        QStringLiteral(u"x.ork\u00A0"));
}

TEST_F(SaveFileNamesTest, ADirectoryIsTakenAsJavasFileHasIt)
{
    const QtRocket::Test::TempDir work;
    const std::filesystem::path   defaultDirectory = work.resolve("default");
    // A separator at the end of the directory, or repeated, is none (java.io.File's
    // normalisation): <work>/default/sub/z.ork stays "sub/z.ork".
    EXPECT_EQ(SaveFileNames::absolutePathText(defaultDirectory / ""),
              SaveFileNames::absolutePathText(defaultDirectory));
    EXPECT_EQ(
        SaveFileNames::filenameInput(defaultDirectory / "sub" / "z.ork", defaultDirectory / ""),
        native(QStringLiteral("sub/z.ork")));
    const QString text = SaveFileNames::absolutePathText(defaultDirectory);
    EXPECT_EQ(SaveFileNames::absolutePathText(QtStrings::toPath(
                  text + QDir::separator() + QDir::separator() + QStringLiteral("x"))),
              text + QDir::separator() + QStringLiteral("x"));
    // A root keeps its separator.
    const std::filesystem::path root = defaultDirectory.root_path();
    EXPECT_EQ(SaveFileNames::absolutePathText(root), native(QtStrings::fromPath(root)));
    // A relative path is resolved against the current directory.
    EXPECT_EQ(SaveFileNames::absolutePathText("x.ork"),
              SaveFileNames::absolutePathText(std::filesystem::current_path() / "x.ork"));
}

TEST_F(SaveFileNamesTest, ADirectoryOfAnotherSpellingIsComparedCanonically)
{
    // "Windows may normalize the chooser directory (case or 8.3 short names)": a file whose
    // path does not start with the directory's is compared by their canonical paths.
    const QtRocket::Test::TempDir work;
    static_cast<void>(work.write("default/sub/kept.txt", "x"));
    const std::filesystem::path defaultDirectory = work.resolve("default");
    EXPECT_EQ(
        SaveFileNames::filenameInput(defaultDirectory / "x.ork", defaultDirectory / "sub" / ".."),
        QStringLiteral("x.ork"));
}

TEST_F(SaveFileNamesTest, TheChoosersDirectoryWithoutOneIsTheHomeDirectory)
{
    EXPECT_EQ(SaveFileNames::chooserDirectory({}), QtStrings::toPath(QDir::homePath()));
    const QtRocket::Test::TempDir work;
    EXPECT_EQ(SaveFileNames::chooserDirectory(work.path()), work.path());
}

}  // namespace

}  // namespace QtRocket::Gui
