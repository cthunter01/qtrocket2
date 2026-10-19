// The reference of the GUI's from-scratch scenario (tests/data/goldens/gui/from-scratch/, written
// by tools/openrocket-gui-reference: OpenRocket's own GUI code runs scenario.json headless and
// saves the design; see GoldenGuiReference.h). The GUI's exit test of Milestone 2 drives the same
// scenario through QtRocket's GUI and compares with the reference; this file checks, without a
// GUI, that the reference stays usable:
//
// - the scenario is well formed, and state.json is the record of exactly this scenario (the
//   SHA-256 of scenario.json, a record per step of the same kind), made with the OpenRocket of
//   the other goldens; every record's tree digest is the digest of its tree, and every step that
//   sets a field changed the tree (a value the field already had would check nothing);
// - the component paths the exit test resolves (GoldenGuiReference.h) name the components of the
//   records in QtRocket's load, and a later child of a name by its occurrence;
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
#include <memory>
#include <optional>
#include <set>
#include <span>
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
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
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

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::ErrorCode;
using QtRocket::GeneralRocketLoader;
using QtRocket::LoadedDocument;
using QtRocket::Parachute;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::ShockCord;
using QtRocket::ThrustCurveMotorSetDatabase;
using QtRocket::Uuid;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::DesignFileEnvironment;
using QtRocket::Test::GuiComponentPath;
using QtRocket::Test::guiComponentPathOf;
using QtRocket::Test::guiComponentPathText;
using QtRocket::Test::GuiReference;
using QtRocket::Test::GuiScenario;
using QtRocket::Test::GuiStepKind;
using QtRocket::Test::guiStepName;
using QtRocket::Test::GuiStepRecord;
using QtRocket::Test::resolveGuiComponentPath;

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

/// The field steps (type, choose, check, editPoint, insertPoint) whose record has the tree digest
/// of the record before, by index.
[[nodiscard]] std::vector<std::size_t> fieldStepsThatChangeNothing(const GuiReference& reference)
{
    std::vector<std::size_t> unchanged;
    for (std::size_t at = 1; at < reference.steps.size(); at++)
    {
        const GuiStepRecord& record = reference.steps[at];
        const bool field = record.kind == GuiStepKind::TYPE || record.kind == GuiStepKind::CHOOSE ||
                           record.kind == GuiStepKind::CHECK ||
                           record.kind == GuiStepKind::EDIT_POINT ||
                           record.kind == GuiStepKind::INSERT_POINT;
        if (field && record.treeSha256 == reference.steps[at - 1].treeSha256)
        {
            unchanged.push_back(record.index);
        }
    }
    return unchanged;
}

/// The component paths of the records: the selections, and the components of the open dialogs.
[[nodiscard]] std::vector<GuiComponentPath> recordedPaths(const GuiReference& reference)
{
    std::vector<GuiComponentPath> paths;
    for (const GuiStepRecord& record : reference.steps)
    {
        paths.insert(paths.end(), record.selection.begin(), record.selection.end());
        if (record.windowComponent)
        {
            paths.push_back(*record.windowComponent);
        }
    }
    return paths;
}

/// What resolving @p paths in a rocket gave: how many named a component, and those whose
/// component has another path (as text).
struct PathRoundTrips
{
    std::size_t              resolved{0};
    std::vector<std::string> wrong;
};

/// Resolves each of @p paths in @p rocket and takes the path of the component it names; the paths
/// that name none (a component of an earlier tree) are skipped.
[[nodiscard]] PathRoundTrips roundTrips(const Rocket&                     rocket,
                                        std::span<const GuiComponentPath> paths)
{
    PathRoundTrips trips;
    for (const GuiComponentPath& path : paths)
    {
        const Result<const RocketComponent*> component = resolveGuiComponentPath(rocket, path);
        if (!component)
        {
            continue;
        }
        trips.resolved++;
        if (guiComponentPathOf(**component) != path)
        {
            trips.wrong.push_back(
                std::format("{} -> {}", guiComponentPathText(path),
                            guiComponentPathText(guiComponentPathOf(**component))));
        }
    }
    return trips;
}

/// The components of @p rocket (the rocket included) whose path does not resolve to them, by
/// name.
[[nodiscard]] std::vector<std::string> componentsNotNamedByTheirPath(const Rocket& rocket)
{
    std::vector<const RocketComponent*> components = rocket.getAllChildren();
    components.push_back(&rocket);
    std::vector<std::string> wrong;
    for (const RocketComponent* component : components)
    {
        const GuiComponentPath               path     = guiComponentPathOf(*component);
        const Result<const RocketComponent*> resolved = resolveGuiComponentPath(rocket, path);
        if (!resolved || *resolved != component)
        {
            wrong.push_back(
                std::format("{} ({})", component->getName(), guiComponentPathText(path)));
        }
    }
    return wrong;
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

TEST(FromScratchReferenceGolden, EveryFieldStepChangesTheTree)
{
    // A step that gives a field the value it already has would also pass a binding that does
    // nothing; the tool refuses such a scenario, and this guards the committed record.
    const Result<GuiReference> reference = loadReference();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    EXPECT_EQ(fieldStepsThatChangeNothing(*reference), std::vector<std::size_t>{});
}

// ======================================================================== the component paths

TEST(FromScratchReferenceGolden, TheRecordedPathsNameTheirComponentsInTheReload)
{
    const Result<GuiReference> reference = loadReference();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    ASSERT_FALSE(reference->steps.empty());
    GuiEnvironment               environment;
    const DefaultUnitsGuard      units;
    const GeneralRocketLoader    loader(environment.get().context());
    const Result<LoadedDocument> loaded = loader.load(referenceFile());
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    const Rocket& rocket = loaded->document->getRocket();

    // After the save the selection is the rocket (Save as selects it), the empty path.
    const std::vector<GuiComponentPath>& last = reference->steps.back().selection;
    EXPECT_EQ(last, std::vector<GuiComponentPath>{GuiComponentPath{}});
    const PathRoundTrips lastTrips = roundTrips(rocket, last);
    EXPECT_EQ(lastTrips.resolved, last.size());
    EXPECT_EQ(lastTrips.wrong, std::vector<std::string>{});

    // The selections and dialog components of every record that are still in the saved tree
    // (not the trapezoidal fin set before its conversion, nor the copy before its renaming).
    const std::vector<GuiComponentPath> paths = recordedPaths(*reference);
    const PathRoundTrips                trips = roundTrips(rocket, paths);
    EXPECT_GT(trips.resolved, paths.size() / 2);
    EXPECT_EQ(trips.wrong, std::vector<std::string>{});
    EXPECT_EQ(componentsNotNamedByTheirPath(rocket), std::vector<std::string>{});
}

/// The error code of @p result, nullopt when it has a value.
[[nodiscard]] std::optional<ErrorCode> errorCodeOf(const Result<const RocketComponent*>& result)
{
    return result ? std::nullopt : std::optional<ErrorCode>{result.error().code};
}

TEST(FromScratchReferenceGolden, AnOccurrenceNamesALaterChildOfTheSameName)
{
    // Children of one name, as the scenario's paste makes them: two parachutes, and a shock cord
    // of that name between them (the occurrence counts names, whatever the class).
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    stage.setName("Sustainer");
    auto& tube = stage.addChild(std::make_unique<BodyTube>());
    tube.setName("Body Tube");
    auto& first = tube.addChild(std::make_unique<Parachute>());
    first.setName("Parachute");
    tube.addChild(std::make_unique<ShockCord>()).setName("Parachute");
    auto& copy = tube.addChild(std::make_unique<Parachute>());
    copy.setName("Parachute");

    const GuiComponentPath third{{.name = "Sustainer", .occurrence = 1},
                                 {.name = "Body Tube", .occurrence = 1},
                                 {.name = "Parachute", .occurrence = 3}};
    EXPECT_EQ(resolveGuiComponentPath(rocket, third).value_or(nullptr), &copy);
    EXPECT_EQ(guiComponentPathOf(copy), third);
    EXPECT_EQ(guiComponentPathText(third), "Sustainer/Body Tube/Parachute#3");

    // An element without an occurrence is the first child of its name.
    const GuiComponentPath plain{
        {.name = "Sustainer"}, {.name = "Body Tube"}, {.name = "Parachute"}};
    EXPECT_EQ(resolveGuiComponentPath(rocket, plain).value_or(nullptr), &first);
    EXPECT_EQ(guiComponentPathOf(first), plain);
    EXPECT_EQ(guiComponentPathText(plain), "Sustainer/Body Tube/Parachute");

    // No fourth child of the name, no child of another name; the rocket is the empty path.
    GuiComponentPath fourth  = third;
    fourth.back().occurrence = 4;
    EXPECT_EQ(errorCodeOf(resolveGuiComponentPath(rocket, fourth)), ErrorCode::NOT_FOUND);
    const GuiComponentPath missing{{.name = "Sustainer"}, {.name = "Nose cone"}};
    EXPECT_EQ(errorCodeOf(resolveGuiComponentPath(rocket, missing)), ErrorCode::NOT_FOUND);
    EXPECT_EQ(resolveGuiComponentPath(rocket, {}).value_or(nullptr), &rocket);
    EXPECT_EQ(guiComponentPathOf(rocket), GuiComponentPath{});
    EXPECT_EQ(guiComponentPathText({}), "(rocket)");
    EXPECT_EQ(componentsNotNamedByTheirPath(rocket), std::vector<std::string>{});
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
