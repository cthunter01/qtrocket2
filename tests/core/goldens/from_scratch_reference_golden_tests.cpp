// The reference of the GUI's from-scratch scenario (tests/data/goldens/gui/from-scratch/, written
// by tools/openrocket-gui-reference: OpenRocket's own GUI code runs scenario.json headless and
// saves the design; see GoldenGuiReference.h). The GUI's exit test of Milestone 2 drives the same
// scenario through QtRocket's GUI and compares with the reference; this file checks, without a
// GUI, that the reference stays usable:
//
// - the scenario is well formed, and state.json is the record of exactly this scenario (the
//   SHA-256 of scenario.json, a record per step of the same kind), made with the OpenRocket of
//   the other goldens; every record's tree digest is the digest of its tree;
// - QtRocket loads the saved design (rocket.ork) without warnings, into the state OpenRocket's
//   own load gives (designFileState() of DesignFileState.h, line for line, the component ids of
//   the file included), with OpenRocket's masses, centres of mass and length;
// - the reloaded rocket is the one the GUI saved (the tree of the last record, describeRocket()
//   without ids), but for what OpenRocket's own save and reload changes (kSaveAndReloadChanges).
//   The exit test compares a reload of QtRocket's save with that record in the same way.

#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Sha256.h"
#include "QtRocket/util/Uuid.h"
#include "file/DesignFileEnvironment.h"
#include "file/RocketLoaderTestSupport.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/DesignFileState.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenGuiReference.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::GeneralRocketLoader;
using QtRocket::LoadedDocument;
using QtRocket::Result;
using QtRocket::ThrustCurveMotorSetDatabase;
using QtRocket::Uuid;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::DesignFileEnvironment;
using QtRocket::Test::GuiReference;
using QtRocket::Test::GuiScenario;
using QtRocket::Test::GuiStepKind;
using QtRocket::Test::guiStepName;

using Presets = DesignFileEnvironment::Presets;

[[nodiscard]] std::filesystem::path scenarioFile()
{
    return QtRocket::Test::guiFromScratchDir() / "scenario.json";
}

[[nodiscard]] std::filesystem::path referenceFile()
{
    return QtRocket::Test::guiFromScratchDir() / "rocket.ork";
}

/// The contents of @p file; a test failure and "" when it cannot be read.
[[nodiscard]] std::string textOf(const std::filesystem::path& file)
{
    const Result<std::string> text = QtRocket::readTextFile(file);
    if (!text)
    {
        ADD_FAILURE() << text.error().toString();
        return {};
    }
    return *text;
}

[[nodiscard]] Result<GuiScenario> loadScenario()
{
    return QtRocket::Test::parseGuiScenario(textOf(scenarioFile()));
}

[[nodiscard]] Result<GuiReference> loadReference()
{
    return QtRocket::Test::parseGuiReference(
        textOf(QtRocket::Test::guiFromScratchDir() / "state.json"));
}

/// The environment of the loads, the one in which the tool loaded the saved design: OpenRocket's
/// GUI after its start-up (SwingStartup), without component presets and motors (QtRocket's
/// start-up of Milestone 2 has none either, and the design names none), with the default
/// component materials the start-up stores (loadDefaultComponentMaterials(): Balsa for fin sets,
/// Polystyrene for nose cones). A new component of the loader starts with them, so a <material>
/// that names the same one changes nothing and fires no event: the events of the load count it.
class GuiEnvironment
{
public:
    GuiEnvironment()
    {
        QtRocket::addBuiltinMaterials(m_materials);
        QtRocket::loadDefaultComponentMaterials(m_environment.preferences(), m_materials);
    }

    [[nodiscard]] DesignFileEnvironment& get() noexcept { return m_environment; }

private:
    ThrustCurveMotorSetDatabase m_motors;
    QtRocket::MaterialStorage   m_materials;
    DesignFileEnvironment       m_environment{Presets::NONE, m_motors};
};

/// A line of a tree that OpenRocket's own save and reload of the reference changes: in the line
/// that starts with @p line, @p saved becomes @p reloaded.
struct LineChange
{
    std::string_view line;
    std::string_view saved;
    std::string_view reloaded;
};

/// What the save and the reload of the scenario's design change, in OpenRocket as in QtRocket:
/// - the inner tube, a motor mount, saves its default ignition (<ignitionevent> of <motormount>),
///   and the loader sets it with MotorConfiguration.setIgnitionEvent(), which marks it an
///   override (MotorMountHandler.java:101-115, MotorConfiguration.java:198-205).
constexpr std::array kSaveAndReloadChanges{
    LineChange{.line     = "      InnerTube 'Inner Tube' ",
               .saved    = " ign=AUTOMATIC:0.0:false",
               .reloaded = " ign=AUTOMATIC:0.0:true"},
};

/// @p tree with the changes of kSaveAndReloadChanges made, each exactly once; a test failure for
/// a change whose line is not there.
[[nodiscard]] std::vector<std::string> reloadedTree(std::vector<std::string> tree)
{
    for (const LineChange& change : kSaveAndReloadChanges)
    {
        std::size_t made = 0;
        for (std::string& line : tree)
        {
            const std::size_t at = line.find(change.saved);
            if (line.starts_with(change.line) && at != std::string::npos)
            {
                line.replace(at, change.saved.size(), change.reloaded);
                made++;
            }
        }
        EXPECT_EQ(made, 1U) << "the saved tree has no line " << change.line << "..."
                            << change.saved;
    }
    return tree;
}

/// The kinds of the scenario's steps by name.
[[nodiscard]] std::vector<std::string_view> scenarioStepNames(const GuiScenario& scenario)
{
    std::vector<std::string_view> names;
    names.reserve(scenario.steps.size());
    for (const QtRocket::Test::GuiScenarioStep& step : scenario.steps)
    {
        names.push_back(guiStepName(step.kind));
    }
    return names;
}

/// The kinds of the records by name.
[[nodiscard]] std::vector<std::string_view> recordStepNames(const GuiReference& reference)
{
    std::vector<std::string_view> names;
    names.reserve(reference.steps.size());
    for (const QtRocket::Test::GuiStepRecord& record : reference.steps)
    {
        names.push_back(guiStepName(record.kind));
    }
    return names;
}

/// The records whose tree digest is not the digest of their tree, one line each.
[[nodiscard]] std::vector<std::string> wrongDigests(const GuiReference& reference)
{
    std::vector<std::string> wrong;
    for (const QtRocket::Test::GuiStepRecord& record : reference.steps)
    {
        if (record.tree && QtRocket::Test::guiTreeSha256(*record.tree) != record.treeSha256)
        {
            wrong.push_back(std::format("record {}", record.index));
        }
    }
    return wrong;
}

/// The records whose index is not their place in the list.
[[nodiscard]] std::vector<std::size_t> misplacedRecords(const GuiReference& reference)
{
    std::vector<std::size_t> misplaced;
    for (std::size_t at = 0; at < reference.steps.size(); at++)
    {
        if (reference.steps[at].index != at)
        {
            misplaced.push_back(at);
        }
    }
    return misplaced;
}

/// The lines of @p state that are warnings ("W <text>").
[[nodiscard]] std::vector<std::string> warningLinesOf(const std::vector<std::string>& state)
{
    std::vector<std::string> warnings;
    for (const std::string& line : state)
    {
        if (line.starts_with("W "))
        {
            warnings.push_back(line);
        }
    }
    return warnings;
}

// ================================================================================== the files

TEST(FromScratchReferenceGolden, TheScenarioIsWellFormed)
{
    const Result<GuiScenario> scenario = loadScenario();
    ASSERT_TRUE(scenario.has_value()) << scenario.error().toString();
    ASSERT_FALSE(scenario->steps.empty());
    EXPECT_EQ(scenario->steps.front().kind, GuiStepKind::NEW);
    // It ends with the save: the file chooser of Save as, after the design information dialog.
    EXPECT_EQ(scenario->steps.back().kind, GuiStepKind::CHOOSE_FILE);
    EXPECT_FALSE(scenario->title.empty());
}

TEST(FromScratchReferenceGolden, TheReferenceIsTheRecordOfThisScenario)
{
    const Result<GuiScenario>  scenario  = loadScenario();
    const Result<GuiReference> reference = loadReference();
    ASSERT_TRUE(scenario.has_value()) << scenario.error().toString();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    const std::vector<std::byte> bytes = QtRocket::stringToBytes(textOf(scenarioFile()));
    EXPECT_EQ(reference->scenarioFile, "scenario.json");
    EXPECT_EQ(reference->scenarioSha256, QtRocket::sha256Hex(bytes))
        << "scenario.json changed after the reference was made: run "
           "tools/openrocket-gui-reference/generate.sh";
    EXPECT_EQ(reference->scenarioTitle, scenario->title);
    EXPECT_EQ(reference->scenarioSteps, scenario->steps.size());
    EXPECT_EQ(recordStepNames(*reference), scenarioStepNames(*scenario));
    EXPECT_EQ(misplacedRecords(*reference), std::vector<std::size_t>{});
}

TEST(FromScratchReferenceGolden, TheReferenceComesFromTheOpenRocketOfTheGoldens)
{
    const Result<GuiReference> reference = loadReference();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    const auto manifest = QtRocket::Test::loadGoldenManifest();
    ASSERT_TRUE(manifest.has_value()) << manifest.error().toString();
    EXPECT_EQ(reference->openRocketCommit, manifest->openrocketCommit);
    EXPECT_EQ(reference->openRocketVersion, manifest->openrocketVersion);
    EXPECT_FALSE(reference->dirty);
    EXPECT_TRUE(reference->javaVersion.starts_with("17."));
    // GeneralRocketSaver's archive held the document alone: no decal, no motor file, no preview.
    EXPECT_EQ(reference->referenceFile, "rocket.ork");
    EXPECT_EQ(reference->savedAs, "rocket.ork");
    EXPECT_EQ(reference->archiveEntries, std::vector<std::string>{"rocket.ork"});
}

TEST(FromScratchReferenceGolden, EveryTreeDigestIsTheDigestOfItsTree)
{
    const Result<GuiReference> reference = loadReference();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    ASSERT_FALSE(reference->steps.empty());
    // The last record, after the save, has its tree; the digests of the others are made the same
    // way.
    EXPECT_TRUE(reference->steps.back().tree.has_value());
    EXPECT_TRUE(reference->steps.back().saved);
    EXPECT_EQ(wrongDigests(*reference), std::vector<std::string>{});
}

// ================================================================================== the design

TEST(FromScratchReferenceGolden, TheSavedDesignLoadsWithoutWarnings)
{
    GuiEnvironment               environment;
    const DefaultUnitsGuard      units;
    const GeneralRocketLoader    loader(environment.get().context());
    const Result<LoadedDocument> loaded = loader.load(referenceFile());
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    EXPECT_EQ(QtRocket::Test::warningLines(loaded->warnings), "");
}

TEST(FromScratchReferenceGolden, TheSavedDesignLoadsIntoOpenRocketsState)
{
    const Result<GuiReference> reference = loadReference();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    GuiEnvironment                 environment;
    const std::vector<std::string> state = QtRocket::Test::designFileState(
        environment.get(), referenceFile(), QtRocket::Test::StateDetail::WITH_DOCUMENT_PREFERENCES);
    const std::vector<std::string_view> expected(reference->loadedLines.begin(),
                                                 reference->loadedLines.end());
    EXPECT_EQ(QtRocket::Test::firstDifference(expected, state), "");
    // OpenRocket's load gave no warning either.
    EXPECT_EQ(warningLinesOf(reference->loadedLines), std::vector<std::string>{});
}

TEST(FromScratchReferenceGolden, TheSavedDesignHasOpenRocketsMassesAndLength)
{
    const Result<GuiReference> reference = loadReference();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    GuiEnvironment               environment;
    const DefaultUnitsGuard      units;
    const GeneralRocketLoader    loader(environment.get().context());
    const Result<LoadedDocument> loaded = loader.load(referenceFile());
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    const std::vector<std::string_view> expected(reference->loadedNumbers.begin(),
                                                 reference->loadedNumbers.end());
    EXPECT_EQ(QtRocket::Test::wrongNumbers(
                  expected, QtRocket::Test::massAndLength(loaded->document->getRocket())),
              "");
}

TEST(FromScratchReferenceGolden, TheReloadIsTheSavedRocketButForWhatTheReloadChanges)
{
    const Result<GuiReference> reference = loadReference();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    ASSERT_FALSE(reference->steps.empty());
    ASSERT_TRUE(reference->steps.back().tree.has_value());
    GuiEnvironment               environment;
    const DefaultUnitsGuard      units;
    const GeneralRocketLoader    loader(environment.get().context());
    const Result<LoadedDocument> loaded = loader.load(referenceFile());
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    // describeRocket() with no known id, as the records print the tree.
    EXPECT_EQ(QtRocket::Test::describeRocket(loaded->document->getRocket(),
                                             environment.get().preferences(), std::set<Uuid>{}),
              reloadedTree(reference->steps.back().tree.value_or(std::vector<std::string>{})));
}

}  // namespace
