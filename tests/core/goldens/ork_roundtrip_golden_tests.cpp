// The loaded state of OpenRocket's designs after a round trip through an .ork archive: a design
// is loaded, saved by GeneralRocketSaver into an archive in memory with the document's default
// storage options (what a user's Save does: rocket.ork with the simulation data, the decal
// images, a RockSim engine file of every thrust-curve motor), and the archive is loaded back from
// memory without a directory (GeneralRocketLoader::load(bytes); decision O5 of tier 10: a round
// trip goes through the archive, never through plain XML, which keeps no decal image). The reload
// is compared field by field with what OpenRocket's own load of the design gives, so that a value
// that the saver and the loader get wrong in the same way, which no comparison of the saved text
// can see, shows here.
//
// ork_resave_diff_golden_tests.cpp compares the saves with OpenRocket's texts (its re-saves, its
// saves with data, its archives) and the flights of a reloaded archive bit for bit; that is not
// repeated here. What this file adds:
//
// - The sixteen examples, each loaded fresh from the ORIGINAL archive with the six presets
//   (OrkRoundTripGolden.TheExamplesComeBackInOpenRocketsState). The archive is the first save
//   of the loaded design: a save stores the automatic dimensions its getters compute, so the
//   saves of one loaded document can differ (kDualParachuteSaves of ork_resave_diff), and a
//   first save writes what the design file held. The state of the reload (designBytesState() of
//   DesignFileState.h: the warnings of the load, the document with its flight configurations,
//   simulations, storage options, undo state, materials, photo settings and decal images, the
//   change events of the load, every component with all it holds, the names of the
//   configurations, every motor, every decal image with its size, every simulation with its
//   options, extensions, summary values, stored warnings and the events and a digest of every
//   column of every branch, and the document materials) is OpenRocket's state of the original
//   archive as loaded (ExampleOrkFiles.h), line for line, with one known difference
//   (kRoundTripChanges): the file version. And the reload has OpenRocket's masses, centres of
//   mass and length of the original as loaded (the rows' numbers, asked of a design on which
//   nothing else was asked: see roundTripTheExamples()) and, after settling, as settled (the
//   settled numbers).
//
//   The sixteen stored warnings of a high-speed deployment (in six examples) come back with their
//   speed because the loader reads the type the saver writes, RecoveryHighSpeedDeployment, as
//   well as HighSpeedDeployment (the user's decision E2 of 2026-10-14). OpenRocket's own round
//   trip reads them as warnings of the type Other without the speed: its reload differs from its
//   state of the original in those sixteen warnings and the sixteen events that hold them (the
//   probe FixedPointProbe of the scouts of tier 10, roundtrip-goldens).
//
// - The three examples that name a component preset, loaded without presets
//   (OrkRoundTripGolden.TheExamplesWithoutPresetsKeepTheirSixReferences): each component keeps
//   the reference its <preset> makes (the user's decision U8 of 2026-10-08), the saver writes it
//   back, and the reload is OpenRocket's state of the original without presets, the six "No
//   matching ComponentPreset" warnings included, with the same known difference; and the reload
//   keeps the six references with their four attributes, as the re-saves write them
//   (compareSavedSettings() of GoldenSettings.h with SavedPresets::NOT_FOUND).
//
// - The thirteen test rockets, each loaded from its re-save against the seven motors of
//   TestRockets (OrkRoundTripGolden.TheTestRocketsComeBackAsTheRocketsOfTheGoldens). Their
//   motors are synthetic, so the archive is loaded back against the same seven motors, never
//   against the bundled database, which would give the mounts real motors of the same names
//   (decision Q4 of run 10b). The reload is the rocket of the goldens but for the fourteen values
//   OpenRocket's own reload changes (kReloadChanges): geometry, masses and aerodynamics
//   (compareLoadedResave() of GoldenResaves.h, as ResaveGolden compares the reload of the
//   re-save), the deployment, separation and ignition settings of the re-save, and the fifty
//   simulations with their names, configurations, statuses and options (compareSimulations()).
//   A save of the reload is the first save, entry for entry and byte for byte (the entries'
//   names, their order and their uncompressed bytes; decision O7 of tier 10: never the bytes of
//   the archives, which carry the time of the save).

#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/GeneralRocketSaver.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Error.h"
#include "file/DesignFileEnvironment.h"
#include "file/openrocket/DesignFileState.h"
#include "file/openrocket/ExampleOrkFiles.h"
#include "goldens/AutomaticDimensions.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenDesign.h"
#include "goldens/GoldenExamples.h"
#include "goldens/GoldenGeometry.h"
#include "goldens/GoldenResaves.h"
#include "goldens/GoldenSettings.h"

namespace
{

using QtRocket::GeneralRocketSaver;
using QtRocket::LoadedDocument;
using QtRocket::OpenRocketDocument;
using QtRocket::Preferences;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::Test::addResaveRun;
using QtRocket::Test::archiveChanges;
using QtRocket::Test::AsLoaded;
using QtRocket::Test::changeToTheReload;
using QtRocket::Test::CompactLine;
using QtRocket::Test::compactState;
using QtRocket::Test::ComparedResaves;
using QtRocket::Test::ComparedSimulations;
using QtRocket::Test::compareLoadedResave;
using QtRocket::Test::compareSavedSettings;
using QtRocket::Test::compareSimulations;
using QtRocket::Test::designBytesState;
using QtRocket::Test::designCountsOfTheTestRockets;
using QtRocket::Test::DesignFileEnvironment;
using QtRocket::Test::DesignGoldens;
using QtRocket::Test::ExampleOrkFile;
using QtRocket::Test::ExampleSource;
using QtRocket::Test::firstDifference;
using QtRocket::Test::GoldenExample;
using QtRocket::Test::goldenExampleEnvironment;
using QtRocket::Test::goldenExamples;
using QtRocket::Test::goldenGeometry;
using QtRocket::Test::GoldenInput;
using QtRocket::Test::kExampleOrkFiles;
using QtRocket::Test::loadDesignGoldens;
using QtRocket::Test::LoadedResave;
using QtRocket::Test::loadGoldenExample;
using QtRocket::Test::loadGoldenExampleFromBytes;
using QtRocket::Test::loadGoldenResave;
using QtRocket::Test::loadResave;
using QtRocket::Test::loadTestRocketBytes;
using QtRocket::Test::massAndLength;
using QtRocket::Test::SavedPresets;
using QtRocket::Test::SettingsComparison;
using QtRocket::Test::settingsCountsOfTheTestRockets;
using QtRocket::Test::settleAutomaticDimensions;
using QtRocket::Test::StateDetail;
using QtRocket::Test::testRocketInputs;
using QtRocket::Test::wrongNumbers;

using Presets = DesignFileEnvironment::Presets;

// ================================================================================== the saves

/// The .ork archive of @p document, saved with its default storage options by a GeneralRocketSaver
/// with @p preferences, as a user's Save writes the file. Empty, with what went wrong in
/// @p problems, when the save fails or warns (no design of these tests gives a save warning).
[[nodiscard]] std::vector<std::byte> archiveOf(std::string_view name, OpenRocketDocument& document,
                                               const Preferences& preferences,
                                               std::string&       problems)
{
    GeneralRocketSaver             saver(preferences);
    Result<std::vector<std::byte>> bytes =
        saver.saveToBytes(document, document.getDefaultStorageOptions());
    if (!bytes || !saver.getWarnings().empty())
    {
        problems += std::format("{}: the save {}\n", name,
                                bytes ? std::format("gave {} warnings", saver.getWarnings().size())
                                      : "failed: " + bytes.error().toString());
        return {};
    }
    return std::move(*bytes);
}

/// The lines that differ between @p expected and @p found, two states of the same number of
/// lines, each as "<expected> -> <found>", one per line; for states of another number of lines,
/// the numbers and the first line that differs (firstDifference() of DesignFileState.h).
[[nodiscard]] std::string stateChanges(std::span<const std::string_view> expected,
                                       const std::vector<CompactLine>&   found)
{
    if (expected.size() != found.size())
    {
        return std::format("{} lines -> {} lines: {}\n", expected.size(), found.size(),
                           firstDifference(expected, found));
    }
    std::string changes;
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        if (expected[i] != found[i].text)
        {
            changes += std::format("{} -> {}\n", expected[i], found[i].text);
        }
    }
    return changes;
}

// =============================================================================== the examples

/// The row of ExampleOrkFiles.h of the example whose file is named @p file, or null.
[[nodiscard]] const ExampleOrkFile* exampleRowOf(std::string_view file)
{
    for (const ExampleOrkFile& row : kExampleOrkFiles)
    {
        if (row.file == file)
        {
            return &row;
        }
    }
    return nullptr;
}

/// The known difference of a round trip: the line of the file version of a state
/// (versionLine() of DesignFileState.h) before and after. QtRocket saves every design as file
/// version 1.11 (OpenRocketSaver::calculateNecessaryFileVersion() is a constant, as OpenRocket's
/// is), so an example of version 1.10, fourteen of the sixteen, comes back as one of 1.11; the
/// two examples of version 1.11 come back unchanged. OpenRocket's round trip changes the version
/// as well (the probe FixedPointProbe of the scouts of tier 10). Nothing the loader does depends
/// on the difference for these files: the state is the same in every other line.
constexpr std::string_view kVersion110       = "version=110";
constexpr std::string_view kRoundTripChanges = "version=110 -> version=111\n";

/// The changes a round trip is to make to the state @p state as loaded: kRoundTripChanges for a
/// file of version 1.10, nothing for one of 1.11.
[[nodiscard]] std::string_view expectedChanges(std::span<const std::string_view> state)
{
    return !state.empty() && state.front() == kVersion110 ? kRoundTripChanges : "";
}

/// What the round trips of the examples compared and found.
struct ExampleRoundTrips
{
    int         designs{0};           ///< the examples saved and loaded back
    int         withData{0};          ///< those whose default storage options save the data
    std::size_t stateLines{0};        ///< the lines of their states, in the short form
    int         newVersions{0};       ///< the states whose version changed (kRoundTripChanges)
    std::size_t numbers{0};           ///< the numbers compared, as loaded and as settled
    std::size_t presetReferences{0};  ///< the references to presets compared (without presets)
    std::string problems;             ///< what differs, by example; empty when nothing does
};

/// Compares the state of the archive @p archive of @p example, loaded back in the environment of
/// @p presets (designBytesState()), with @p expected, OpenRocket's state of the original as
/// loaded, but for expectedChanges(). @p asLoaded is handed the reload before its state is
/// described (designBytesState()). Counts into @p trips.
void compareTheState(const GoldenExample& example, std::span<const std::string_view> expected,
                     std::vector<std::byte> archive, Presets presets, const AsLoaded& asLoaded,
                     ExampleRoundTrips& trips)
{
    const std::vector<CompactLine> state =
        compactState(designBytesState(goldenExampleEnvironment(presets), std::move(archive),
                                      StateDetail::WITH_DOCUMENT_PREFERENCES, asLoaded));
    trips.stateLines += state.size();
    const std::string changes = stateChanges(expected, state);
    trips.newVersions += changes == kRoundTripChanges ? 1 : 0;
    if (changes != expectedChanges(expected))
    {
        trips.problems += std::format("{}: the state of the reload differs from OpenRocket's\n{}",
                                      example.name, changes);
    }
}

/// Compares the masses, centres of mass and length of @p rocket, the design of a reload of the
/// archive of the example of the row @p row as loaded, asked first (massAndLength() of
/// DesignFileState.h), with OpenRocket's numbers of the original as loaded, then, with the
/// automatic dimensions settled as the golden harness settles them, with its settled numbers.
/// (The number of the settling passes is not compared: asking the numbers has already brought
/// the one stale design, the dual parachute deployment, where the one pass of OpenRocket's
/// settling brings it.) Counts into @p trips.
void compareTheNumbers(const GoldenExample& example, const ExampleOrkFile& row,
                       const Rocket& rocket, ExampleRoundTrips& trips)
{
    const std::string numbers = wrongNumbers(row.numbers, massAndLength(rocket));
    const Result<int> passes  = settleAutomaticDimensions(rocket);
    const std::string settled = passes ? wrongNumbers(row.settled, massAndLength(rocket))
                                       : passes.error().toString() + "\n";
    trips.numbers += row.numbers.size() + row.settled.size();
    if (!numbers.empty() || !settled.empty())
    {
        trips.problems +=
            std::format("{}: the numbers of the reload as loaded\n{}and as settled\n{}",
                        example.name, numbers, settled);
    }
}

/// compareTheNumbers() of a load of its own of the archive @p archive.
void compareTheNumbersOfALoad(const GoldenExample& example, const ExampleOrkFile& row,
                              std::vector<std::byte> archive, ExampleRoundTrips& trips)
{
    const Result<LoadedDocument> loaded = loadGoldenExampleFromBytes(std::move(archive));
    if (!loaded)
    {
        trips.problems += std::format("{}: {}\n", example.name, loaded.error().toString());
        return;
    }
    compareTheNumbers(example, row, loaded->document->getRocket(), trips);
}

/// Loads @p example fresh from the original archive in the environment of @p presets and saves it
/// as an archive (archiveOf()); the archive, or nothing with the problem in @p trips.
[[nodiscard]] std::vector<std::byte> archiveOfTheExample(const GoldenExample& example,
                                                         Presets presets, ExampleRoundTrips& trips)
{
    const Result<LoadedDocument> loaded =
        loadGoldenExample(example, ExampleSource::ORIGINAL, presets);
    if (!loaded)
    {
        trips.problems += std::format("{}: {}\n", example.name, loaded.error().toString());
        return {};
    }
    trips.designs++;
    trips.withData += loaded->document->getDefaultStorageOptions().getSaveSimulationData() ? 1 : 0;
    return archiveOf(example.name, *loaded->document,
                     goldenExampleEnvironment(presets).preferences(), trips.problems);
}

/// Saves each of the sixteen examples to an archive and compares its reload with OpenRocket's
/// state and numbers of the original (compareTheState(), compareTheNumbers()).
[[nodiscard]] ExampleRoundTrips roundTripTheExamples()
{
    ExampleRoundTrips trips;
    for (const GoldenExample& example : goldenExamples())
    {
        const ExampleOrkFile* const row = exampleRowOf(example.file);
        std::vector<std::byte> archive  = archiveOfTheExample(example, Presets::EXAMPLES, trips);
        if (row == nullptr || archive.empty())
        {
            trips.problems +=
                std::format("{}: no row in ExampleOrkFiles.h or no archive\n", example.name);
            continue;
        }
        // What is asked of a design as loaded is part of its state (an automatic dimension
        // stores what it computes), and a copy of a design is no design as loaded (a copy of a
        // rocket brings automatic dimensions up to date, in OpenRocket too). But a design whose
        // automatic dimensions are all up to date as loaded, which OpenRocket's settling does
        // not change (fifteen of the sixteen), stores what it holds already: its numbers are
        // asked of the reload itself before the state is described, which saves a load (the
        // comparison of the state would show a change). The dual parachute deployment, stale as
        // loaded, gets a load of its own.
        if (row->settlingPasses == 0)
        {
            compareTheState(
                example, row->state, std::move(archive), Presets::EXAMPLES,
                [&example, row, &trips](const LoadedDocument& reload) {
                    compareTheNumbers(example, *row, reload.document->getRocket(), trips);
                },
                trips);
            continue;
        }
        compareTheState(example, row->state, archive, Presets::EXAMPLES, nullptr, trips);
        compareTheNumbersOfALoad(example, *row, std::move(archive), trips);
    }
    return trips;
}

// Each of the sixteen examples comes back from its archive in the state OpenRocket's load of the
// original gives (see the top of this file): 65 motors, 23 decal images, 54 simulations with
// their 69 branches of stored flight data (all sixteen save their data by default), 20 stored
// warnings and 112 document materials among the lines, every one of them as OpenRocket has it,
// but for the file version of the fourteen examples of version 1.10. The masses, centres of mass
// and length, as loaded and as settled, are OpenRocket's too.
TEST(OrkRoundTripGolden, TheExamplesComeBackInOpenRocketsState)
{
    const ExampleRoundTrips trips = roundTripTheExamples();
    EXPECT_EQ(trips.problems, "");
    EXPECT_EQ(trips.designs, 16);
    EXPECT_EQ(trips.withData, 16);
    EXPECT_EQ(trips.newVersions, 14);
    EXPECT_EQ(trips.stateLines, 2352U);
    EXPECT_EQ(trips.numbers, 360U);
}

/// Compares @p reload, the reload of the archive of @p example loaded without presets, with the
/// settings elements of OpenRocket's re-save of the example, the references to presets included
/// (compareSavedSettings() with SavedPresets::NOT_FOUND), which asks nothing that a state holds
/// of an automatic dimension. Counts into @p trips.
void compareTheReferences(const GoldenExample& example, const LoadedDocument& reload,
                          ExampleRoundTrips& trips)
{
    const Result<const nlohmann::json*> geometry = goldenGeometry(example.name);
    const auto                          resave   = loadGoldenResave(example.name);
    if (!geometry || !resave)
    {
        trips.problems += std::format("{}: no geometry.json or no re-save\n", example.name);
        return;
    }
    const SettingsComparison settings = compareSavedSettings(
        reload.document->getRocket(), **geometry, **resave, SavedPresets::NOT_FOUND);
    trips.presetReferences += static_cast<std::size_t>(settings.compared.presets);
    if (!settings.report.empty())
    {
        trips.problems +=
            std::format("{}: the settings of the reload\n{}", example.name, settings.report);
    }
}

/// Saves each example that needs its presets (a row of ExampleOrkFiles.h with a state without
/// presets), loaded without them, to an archive and compares its reload with the references of
/// the re-save (compareTheReferences()) and with OpenRocket's state without presets.
[[nodiscard]] ExampleRoundTrips roundTripTheExamplesWithoutPresets()
{
    ExampleRoundTrips trips;
    for (const GoldenExample& example : goldenExamples())
    {
        const ExampleOrkFile* const row = exampleRowOf(example.file);
        if (row == nullptr || row->stateWithoutPresets.empty())
        {
            continue;
        }
        std::vector<std::byte> archive = archiveOfTheExample(example, Presets::NONE, trips);
        if (archive.empty())
        {
            continue;
        }
        compareTheState(
            example, row->stateWithoutPresets, std::move(archive), Presets::NONE,
            [&example, &trips](const LoadedDocument& reload) {
                compareTheReferences(example, reload, trips);
            },
            trips);
    }
    return trips;
}

// The three examples that name a component preset, loaded without presets, come back in
// OpenRocket's state without presets, the six warnings of the references it cannot resolve
// included, but for the file version; and the six references survive the round trip, each with
// the type, manufacturer, part number and digest of its <preset>.
TEST(OrkRoundTripGolden, TheExamplesWithoutPresetsKeepTheirSixReferences)
{
    const ExampleRoundTrips trips = roundTripTheExamplesWithoutPresets();
    EXPECT_EQ(trips.problems, "");
    EXPECT_EQ(trips.designs, 3);
    EXPECT_EQ(trips.newVersions, 2);
    EXPECT_EQ(trips.presetReferences, 6U);
    EXPECT_EQ(trips.stateLines, 474U);
}

// ========================================================================= the test rockets

/// What the round trips of the test rockets compared and found.
struct TestRocketRoundTrips
{
    int                 inputs{0};    ///< the test rockets of the manifest
    std::size_t         entries{0};   ///< the entries of their archives
    ComparedResaves     designs;      ///< the reloads against the goldens and the re-saves
    ComparedSimulations simulations;  ///< their simulations against the golden simulations
    std::string         problems;     ///< what differs beyond those, by input
};

/// Loads the re-save of the test rocket @p input, saves it to an archive and loads that back
/// against the seven motors; saves the reload again and compares the two archives
/// (archiveChanges()), then the reload with the goldens and the re-save (compareLoadedResave(),
/// compareSimulations()). Counts into @p trips.
void roundTripTheTestRocket(const GoldenInput& input, TestRocketRoundTrips& trips)
{
    Result<DesignGoldens>      goldens = loadDesignGoldens(input.name);
    const Result<LoadedResave> resave  = loadResave(input);
    const auto                 file    = loadGoldenResave(input.name);
    if (!goldens || !resave || !file)
    {
        trips.problems += std::format("{}: no goldens, no load or no re-save\n", input.name);
        return;
    }
    const std::vector<std::byte> first = archiveOf(
        input.name, *resave->loaded.document, resave->environment->preferences(), trips.problems);
    const Result<LoadedResave> reload = loadTestRocketBytes(first);
    if (first.empty() || !reload)
    {
        trips.problems += std::format("{}: no archive, or no reload of it\n", input.name);
        return;
    }
    const std::vector<std::byte> second = archiveOf(
        input.name, *reload->loaded.document, reload->environment->preferences(), trips.problems);
    const std::string changes = archiveChanges(first, second, trips.entries);
    if (!changes.empty())
    {
        trips.problems += std::format("{}: the save of the reload is not the first save\n{}",
                                      input.name, changes);
    }
    changeToTheReload(input.name, goldens->geometry, trips.designs.changes);
    addResaveRun(trips.designs, input.name, compareLoadedResave(*reload, *goldens, **file));
    compareSimulations(trips.simulations, input, *reload, goldens->geometry, **file);
}

/// roundTripTheTestRocket() of each test rocket of the manifest.
[[nodiscard]] TestRocketRoundTrips roundTripTheTestRockets()
{
    TestRocketRoundTrips trips;
    for (const GoldenInput& input : testRocketInputs(trips.problems))
    {
        trips.inputs++;
        trips.designs.inputs++;
        trips.simulations.inputs++;
        roundTripTheTestRocket(input, trips);
    }
    return trips;
}

// The thirteen test rockets come back from their archives as the rockets of their goldens, but
// for the fourteen values OpenRocket's own reload of the re-saves changes (kReloadChanges): the
// reloads give no warning and have no stale automatic dimension, and the comparison holds what
// ResaveGolden holds for the reloads of the re-saves (151 components with their ids, 47 flight
// configurations, the masses, 1175 points of the aerodynamics, the settings elements of the
// re-saves) and what ResaveGoldenSimulations holds for their fifty simulations (36 not simulated,
// 14 that cannot run without a motor, 1808 values of their options, the fifty stored directions
// of the launch rod). Saved again, each reload gives the first archive, all 39 entries: the
// thirteen rocket.ork and a RockSim engine file of each of the 26 motors of their mounts, one per
// rocket and digest, named by the synthetic digest ("thrustcurves/digest A8 test.rse").
TEST(OrkRoundTripGolden, TheTestRocketsComeBackAsTheRocketsOfTheGoldens)
{
    const TestRocketRoundTrips trips = roundTripTheTestRockets();
    EXPECT_EQ(trips.problems, "");
    EXPECT_EQ(trips.inputs, 13);
    EXPECT_EQ(trips.entries, 39U);

    const ComparedResaves& designs = trips.designs;
    EXPECT_EQ(designs.problems, "");
    EXPECT_EQ(designs.loadWarnings, 0);
    EXPECT_EQ(designs.settlingPasses, 0);
    EXPECT_EQ(designs.changes.problems, "");
    EXPECT_EQ(designs.changes.values, 14);
    EXPECT_EQ(designs.compared, designs.golden);
    EXPECT_EQ(designs.golden, designCountsOfTheTestRockets());
    EXPECT_EQ(designs.settings, designs.saved);
    EXPECT_EQ(designs.saved, settingsCountsOfTheTestRockets());

    const ComparedSimulations& simulations = trips.simulations;
    EXPECT_EQ(simulations.problems, "");
    EXPECT_EQ(simulations.simulations, 50);
    EXPECT_EQ(simulations.notSimulated, 36);
    EXPECT_EQ(simulations.cannotRun, 14);
    EXPECT_EQ(simulations.withoutMotors, 14);
    EXPECT_EQ(simulations.optionValues, 1808);
    EXPECT_EQ(simulations.intoTheWind, 50);
    EXPECT_EQ(simulations.rodDirections, 50);
}

}  // namespace
