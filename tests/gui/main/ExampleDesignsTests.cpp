#include "QtRocket/gui/main/ExampleDesigns.h"

#include <filesystem>
#include <optional>
#include <string_view>
#include <system_error>
#include <vector>

#include <QChar>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "TestPaths.h"
#include "TestTempDir.h"

// The example designs of a directory and the order of their menu. The names, the sorted order
// and the menus are those of OpenRocket's ExampleDesignFile and ExampleDesignFileAction in the
// Java probe RecentExamplesProbe (probes/m2r3-P4/java: out-default.txt for the shipped examples,
// out-custom.txt and out-empty.txt for the probe's own directories, the "EXAMPLES" lines), with
// the deliberate differences marked where a test expects them.

namespace QtRocket::Gui
{

namespace
{

using ExampleDesignsTest = Gui::Test::GuiTest;

/// The separator in a menu transcript, as the probe prints it.
[[nodiscard]] QString separator()
{
    return QStringLiteral("----");
}

/// The names of @p designs' designs in their order.
[[nodiscard]] QStringList designNames(const ExampleDesigns& designs)
{
    QStringList names;
    for (const ExampleDesigns::Design& design : designs.designs())
    {
        names.append(design.name);
    }
    return names;
}

/// The menu of @p designs as the probe prints it: each design's name, "----" for a separator.
[[nodiscard]] QStringList menuTranscript(const ExampleDesigns& designs)
{
    QStringList transcript;
    for (const std::optional<ExampleDesigns::Design>& entry : designs.menuEntries())
    {
        transcript.append(entry.has_value() ? entry->name : separator());
    }
    return transcript;
}

/// Writes the files of the probe's own directory (out-custom.txt) into @p directory: design
/// files of every spelling, files of other kinds and a directory named like a design.
void writeProbeDirectory(const QtRocket::Test::TempDir& directory)
{
    for (const std::string_view name :
         {"Tube fin rocket.ork", "Zeta.ork", "alpha.ork", "Upper.ORK", "notes.txt",
          "Airstart timing.ork", "simulation SCRIPTING.ork", "3D printable nose cone and fins.ork",
          ".ork", "Mixed.oRk", "x.ork.gz", "dir.ork/inside.txt"})
    {
        static_cast<void>(directory.write(name, "x\n"));
    }
}

TEST_F(ExampleDesignsTest, TheShippedExamplesAreSortedByName)
{
    // "EXAMPLES sorted" of out-default.txt (String.compareTo(): "ARC" after "A simple").
    const ExampleDesigns designs(QtRocket::Test::dataDir() / "examples");
    EXPECT_EQ(
        designNames(designs),
        (QStringList{
            QStringLiteral("3D printable nose cone and fins"),
            QStringLiteral("A simple model rocket"), QStringLiteral("ARC payload rocket"),
            QStringLiteral("Airstart timing"), QStringLiteral("Chute release"),
            QStringLiteral("Clustered motors"), QStringLiteral("Deployable payload"),
            QStringLiteral("Dual parachute deployment"), QStringLiteral("Parallel booster staging"),
            QStringLiteral("Pods--airframes and winglets"),
            QStringLiteral("Pods--powered with recovery deployment"),
            QStringLiteral("Simulation extensions"), QStringLiteral("Simulation scripting"),
            QStringLiteral("Three stage low power rocket"), QStringLiteral("Tube fin rocket"),
            QStringLiteral("Two stage high power rocket")}));
    const ExampleDesigns::Design& first = designs.designs().front();
    EXPECT_EQ(first.file,
              QtRocket::Test::dataDir() / "examples" / "3D printable nose cone and fins.ork");
}

TEST_F(ExampleDesignsTest, TheShippedExamplesAreInOpenRocketsOrderIgnoringCase)
{
    // "EXAMPLES menu" of out-default.txt, with decision U4: the order list's "3D Printable Nose
    // Cone and Fins" takes "3D printable nose cone and fins" after "Tube fin rocket"; OpenRocket
    // puts it last, after a separator of its own ("... 'Simulation scripting' ---- '3D printable
    // nose cone and fins'").
    const ExampleDesigns designs(QtRocket::Test::dataDir() / "examples");
    EXPECT_EQ(
        menuTranscript(designs),
        (QStringList{
            QStringLiteral("A simple model rocket"), QStringLiteral("Two stage high power rocket"),
            QStringLiteral("Three stage low power rocket"), QStringLiteral("ARC payload rocket"),
            QStringLiteral("Tube fin rocket"), QStringLiteral("3D printable nose cone and fins"),
            separator(), QStringLiteral("Deployable payload"), QStringLiteral("Airstart timing"),
            QStringLiteral("Chute release"), QStringLiteral("Dual parachute deployment"),
            QStringLiteral("Clustered motors"), QStringLiteral("Parallel booster staging"),
            QStringLiteral("Pods--airframes and winglets"),
            QStringLiteral("Pods--powered with recovery deployment"), separator(),
            QStringLiteral("Simulation extensions"), QStringLiteral("Simulation scripting")}));
}

TEST_F(ExampleDesignsTest, ADirectoryOfItsOwnIsListedAndOrderedAsOpenRocketDoes)
{
    // out-custom.txt: "EXAMPLES sorted: '' '3D printable nose cone and fins' 'Airstart timing'
    // 'Mixed' 'Tube fin rocket' 'Upper' 'Zeta' 'alpha' 'dir' 'simulation SCRIPTING'", without
    // 'dir', the directory "dir.ork", which OpenRocket lists from a directory (QtRocket takes the
    // regular files, as OpenRocket's jar holds).
    const QtRocket::Test::TempDir directory;
    writeProbeDirectory(directory);
    const ExampleDesigns designs(directory.path());
    EXPECT_EQ(designNames(designs),
              (QStringList{QString(), QStringLiteral("3D printable nose cone and fins"),
                           QStringLiteral("Airstart timing"), QStringLiteral("Mixed"),
                           QStringLiteral("Tube fin rocket"), QStringLiteral("Upper"),
                           QStringLiteral("Zeta"), QStringLiteral("alpha"),
                           QStringLiteral("simulation SCRIPTING")}));
    EXPECT_EQ(designs.designs().at(3),
              (ExampleDesigns::Design{.name = QStringLiteral("Mixed"),
                                      .file = directory.resolve("Mixed.oRk")}));

    // "EXAMPLES menu: 'Tube fin rocket' ---- 'Airstart timing' ---- ---- '' '3D printable nose
    // cone and fins' 'Mixed' 'Upper' 'Zeta' 'alpha' 'dir' 'simulation SCRIPTING'", with decision
    // U4 (the names of the order list match ignoring case: "3D printable nose cone and fins" and
    // "simulation SCRIPTING" take their places) and without 'dir'.
    EXPECT_EQ(
        menuTranscript(designs),
        (QStringList{
            QStringLiteral("Tube fin rocket"), QStringLiteral("3D printable nose cone and fins"),
            separator(), QStringLiteral("Airstart timing"), separator(),
            QStringLiteral("simulation SCRIPTING"), separator(), QString(), QStringLiteral("Mixed"),
            QStringLiteral("Upper"), QStringLiteral("Zeta"), QStringLiteral("alpha")}));
}

TEST_F(ExampleDesignsTest, ANameOfTheOrderListTakesItsExactDesignFirst)
{
    // Decision U4 changes nothing where OpenRocket's exact match finds a design: "SIMULATION
    // SCRIPTING" sorts before "Simulation scripting", which still takes the place of the order
    // list's "Simulation scripting"; the other one is not named by it.
    const QtRocket::Test::TempDir directory;
    const std::filesystem::path   exact = directory.write("Simulation scripting.ork", "x\n");
    const std::filesystem::path   upper = directory.write("SIMULATION SCRIPTING.ork", "x\n");
    std::error_code               error;
    if (std::filesystem::equivalent(exact, upper, error))
    {
        GTEST_SKIP() << "the file system does not tell the two names apart";
    }
    const ExampleDesigns designs(directory.path());
    EXPECT_EQ(menuTranscript(designs),
              (QStringList{separator(), separator(), QStringLiteral("Simulation scripting"),
                           separator(), QStringLiteral("SIMULATION SCRIPTING")}));
}

TEST_F(ExampleDesignsTest, WithoutDesignsTheMenuHasOnlyItsSeparators)
{
    // out-empty.txt: "EXAMPLES sorted:" (none), "EXAMPLES menu: ---- ----"; a directory that is
    // missing has none either (Java: listFiles() gives null).
    const QtRocket::Test::TempDir directory;
    const ExampleDesigns          empty(directory.path());
    EXPECT_TRUE(empty.designs().empty());
    EXPECT_EQ(menuTranscript(empty), (QStringList{separator(), separator()}));
    const ExampleDesigns missing(directory.resolve("missing"));
    EXPECT_TRUE(missing.designs().empty());
    EXPECT_EQ(menuTranscript(missing), (QStringList{separator(), separator()}));
}

TEST_F(ExampleDesignsTest, ADesignFileNameEndsWithOrkInAnyCase)
{
    // ExampleDesignFile.PATTERN ".*\.[oO][rR][kK]$" with String.matches().
    EXPECT_TRUE(ExampleDesigns::isDesignFileName(QStringLiteral("x.ork")));
    EXPECT_TRUE(ExampleDesigns::isDesignFileName(QStringLiteral("x.ORK")));
    EXPECT_TRUE(ExampleDesigns::isDesignFileName(QStringLiteral("x.oRk")));
    EXPECT_TRUE(ExampleDesigns::isDesignFileName(QStringLiteral(".ork")));
    EXPECT_TRUE(ExampleDesigns::isDesignFileName(QStringLiteral("a.b.ork")));
    EXPECT_TRUE(ExampleDesigns::isDesignFileName(QStringLiteral(u"\u00E9t\u00E9.ork")));
    EXPECT_FALSE(ExampleDesigns::isDesignFileName(QStringLiteral("x.ork.gz")));
    EXPECT_FALSE(ExampleDesigns::isDesignFileName(QStringLiteral("x.txt")));
    EXPECT_FALSE(ExampleDesigns::isDesignFileName(QStringLiteral("ork")));
    EXPECT_FALSE(ExampleDesigns::isDesignFileName(QStringLiteral("xork")));
    EXPECT_FALSE(ExampleDesigns::isDesignFileName(QString()));
    // Only these ASCII letters: the Kelvin sign is no 'K' for the class [kK].
    EXPECT_FALSE(ExampleDesigns::isDesignFileName(QStringLiteral(u"x.or\u212A")));
    // '.' takes no line terminator.
    EXPECT_FALSE(ExampleDesigns::isDesignFileName(QStringLiteral("a\nb.ork")));
    EXPECT_FALSE(ExampleDesigns::isDesignFileName(QStringLiteral("a\rb.ork")));
    EXPECT_FALSE(ExampleDesigns::isDesignFileName(QStringLiteral(u"a\u0085b.ork")));
    EXPECT_FALSE(ExampleDesigns::isDesignFileName(QStringLiteral(u"a\u2028b.ork")));
    EXPECT_FALSE(ExampleDesigns::isDesignFileName(QStringLiteral(u"a\u2029b.ork")));
    EXPECT_TRUE(ExampleDesigns::isDesignFileName(QStringLiteral("a\tb.ork")));

    // The name: all but the last four characters.
    EXPECT_EQ(ExampleDesigns::designName(QStringLiteral("Tube fin rocket.ork")),
              QStringLiteral("Tube fin rocket"));
    EXPECT_EQ(ExampleDesigns::designName(QStringLiteral("Upper.ORK")), QStringLiteral("Upper"));
    EXPECT_EQ(ExampleDesigns::designName(QStringLiteral(".ork")), QString());
}

TEST_F(ExampleDesignsTest, TheOrderListIsOpenRocketsWithItsSeparators)
{
    // ExampleDesignFileAction.exampleFileOrder (null: a separator).
    EXPECT_EQ(ExampleDesigns::kMenuOrder.size(), 18U);
    EXPECT_EQ(ExampleDesigns::kMenuOrder.at(5), "3D Printable Nose Cone and Fins");
    EXPECT_TRUE(ExampleDesigns::kMenuOrder.at(6).empty());
    EXPECT_TRUE(ExampleDesigns::kMenuOrder.at(15).empty());
    EXPECT_EQ(ExampleDesigns::kMenuOrder.back(), "Simulation scripting");
}

}  // namespace

}  // namespace QtRocket::Gui
