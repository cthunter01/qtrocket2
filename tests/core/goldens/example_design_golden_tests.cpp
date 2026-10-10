// Design golden tests of OpenRocket's example designs: the sixteen files of data/examples, loaded
// by the .ork loader (GeneralRocketLoader, through the fixture of GoldenExamples.h) and compared
// with what OpenRocket computes for the designs it loaded from the same files, in
// tests/data/goldens/example-<name>/geometry.json, mass.json and aero.json
// (tools/openrocket-goldens; the formats are in that tool's README.md).
//
// Every example is loaded twice, each time into a new document, and each load is compared in
// full (ExampleDesignGolden):
// - from the file OpenRocket ships (a zip archive of file version 1.10 or 1.11), and
// - from OpenRocket's re-save of the loaded design (resave/rocket.ork of its goldens: plain XML
//   of file version 1.11, with every element OpenRocket's saver writes).
// OpenRocket itself reproduces every golden document from either file, so both loads have to
// give the same design, by two different documents. The environment of the loads is the
// harness's (goldenExampleEnvironment(): the bundled motor database, the six component presets
// the examples refer to, OpenRocket's test preferences with motor names without designations),
// and the design is settled as the harness settles it before anything is asked of it
// (loadSettledGoldenExample(); AutomaticDimensions.h says why). There is one test per source,
// which goes through the sixteen examples and reports every mismatch under the name of its
// example: ctest runs every test in a process of its own, and each process that loads a design
// reads the motor database first.
//
// Compared (compareLoadedDesign() of GoldenDesign.h, which the tests of other loaded designs
// share; the comparisons of the components, the configurations, the masses and the
// aerodynamics are those of the thirteen test rockets, see test_rockets_golden_tests.cpp,
// tests/core/mass/MassCalculatorTests.cpp and aero_golden_tests.cpp):
// - geometry.json: the name of the rocket and its selected configuration; the warnings of the
//   load (none in any of the sixteen); every component in the order of the tree with its id
//   (the 327 ids of the files) and with everything the golden entry holds: class, name,
//   placement, mass properties with and without the overrides, the overrides and the component
//   whose override covers it, bounds, instances, and every entry of its "details". The
//   examples bring what no test rocket has: a streamer, an elliptical fin set, a tube fin set
//   and rail buttons; components whose mass is overridden by an ancestor's and one that
//   overrides its subcomponents'; a fixed angle method; clusters of three and four in a ring;
//   rough and unfinished surfaces. Then every flight configuration with its id, its name (four
//   are the user's own, and two are stored names in brackets), its stages, its motors (twenty
//   plugged ones, two that ignite at the burnout of the stage below, four that ignite after a
//   delay), its reference values, lengths and bounds, and every instance of every active
//   component.
// - mass.json: the locations of the components and, per configuration, the four rigid bodies
//   and the CM analysis.
// - aero.json: per configuration the geometry warnings, the 25 points with their force
//   analyses, and the worst CPs.
// - The deployment, separation and ignition settings and the component presets, which no golden
//   document records, are compared with the elements of the re-save (compareSavedSettings() of
//   GoldenSettings.h): 31 <deployevent>, 6 <deploymentconfiguration>, 10 <separationevent>, 4
//   <separationconfiguration>, 22 <motormount>, 65 <motor>, 65 <ignitionconfiguration> and 6
//   <preset> in the sixteen files. The six presets are of two nose cones, two body tubes and
//   two parachutes; geometry.json shows only for the nose cones whether a preset was loaded.
//
// Nothing is skipped silently: the numbers of what was compared have to be those of the files
// of each example, the examples are the "example" inputs of the manifest, and the totals over
// the sixteen are pinned for either source. Nor is the comparison vacuous where it is new:
// ExampleDesignGoldenMutations changes one golden value, or one element of a re-save, at a time
// and expects the line that reports it, for the values that only the examples bring and for
// what only a loaded design is compared in (ids, load warnings, settings, presets), and it
// holds the tolerances of the comparison of mass.json, which the examples meet through
// compareLoadedDesign(), by a value just inside and one just outside each.
//
// Without component presets (ExampleDesignGoldenWithoutPresets), which is how the product loads
// a design before it reads preset files (Milestone 3), the same comparison of the original
// files has to find exactly what OpenRocket's own harness writes then (the tier 9 scouts ran it
// with an empty preset database: probe "golden-tests", out/nopresets.diff): six load warnings
// in three examples, and 0 instead of 0.0015748 m for the thickness of the fore shoulder of two
// nose cones, which their presets alone supply. Everything else is equal: every other value of
// geometry.json and all of mass.json and aero.json. These are the only exemptions of this file.
// The settings are compared there too, with the same numbers of elements; of the six <preset>
// it is held that no component has a preset then (SavedPresets::NOT_FOUND).
//
// Tolerances (plan section 6.4), as for the test rockets: geometry, mass and aerodynamic
// coefficients relative 1e-9; positions, CGs and CPs absolute 1e-9 m; ids, names, warnings and
// the numbers and texts of the re-save exactly.

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <pugixml.hpp>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Error.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenDesign.h"
#include "goldens/GoldenExamples.h"
#include "goldens/GoldenGeometry.h"
#include "goldens/GoldenSettings.h"

namespace
{

using nlohmann::json;
using QtRocket::LoadedDocument;
using QtRocket::Preferences;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::Test::compareLoadedDesign;
using QtRocket::Test::compareLoadedGeometry;
using QtRocket::Test::compareSavedSettings;
using QtRocket::Test::DesignComparison;
using QtRocket::Test::DesignCounts;
using QtRocket::Test::DesignFileEnvironment;
using QtRocket::Test::DesignGoldens;
using QtRocket::Test::ExampleSource;
using QtRocket::Test::findGoldenExample;
using QtRocket::Test::GoldenExample;
using QtRocket::Test::goldenExampleEnvironment;
using QtRocket::Test::goldenExamples;
using QtRocket::Test::loadDesignGoldens;
using QtRocket::Test::loadGoldenResave;
using QtRocket::Test::loadSettledGoldenExample;
using QtRocket::Test::SavedPresets;
using QtRocket::Test::savedSettingsCounts;
using QtRocket::Test::SettingsComparison;
using QtRocket::Test::SettingsCounts;

using Presets = DesignFileEnvironment::Presets;

// ============================================================================ the comparison

/// An example loaded and compared with its goldens: what the files hold, and what the
/// comparisons compared and found.
struct ExampleRun
{
    std::string        problem;   ///< why there is no comparison (a file, the load), else ""
    DesignCounts       golden;    ///< what geometry.json, mass.json and aero.json hold
    DesignComparison   design;    ///< the comparison with them
    SettingsCounts     saved;     ///< the settings elements of the re-save
    SettingsComparison settings;  ///< the comparison with them
};

/// The preference store of the loads of the examples, with which the names of the flight
/// configurations and of the motors are made (motors by their common names).
[[nodiscard]] const Preferences& preferencesOf(Presets presets)
{
    return goldenExampleEnvironment(presets).preferences();
}

/// What a design that was loaded with the presets @p presets is to hold of the presets its file
/// names: each of them, or none.
[[nodiscard]] SavedPresets savedPresetsOf(Presets presets)
{
    return presets == Presets::NONE ? SavedPresets::NOT_FOUND : SavedPresets::LOADED;
}

/// Loads @p example from @p source with the presets @p presets, settles it, and compares it with
/// the design documents @p goldens and with the settings and the presets of its re-save.
[[nodiscard]] ExampleRun compareExample(const GoldenExample& example, ExampleSource source,
                                        Presets presets, const DesignGoldens& goldens)
{
    ExampleRun run;
    run.golden                          = goldenCounts(goldens);
    const Result<LoadedDocument> loaded = loadSettledGoldenExample(example, source, presets);
    if (!loaded)
    {
        run.problem = loaded.error().toString();
        return run;
    }
    const auto resave = loadGoldenResave(example.name);
    if (!resave)
    {
        run.problem = resave.error().toString();
        return run;
    }
    Rocket& rocket = loaded->document->getRocket();
    run.design     = compareLoadedDesign(rocket, loaded->warnings, goldens, preferencesOf(presets));
    run.saved      = savedSettingsCounts(**resave);
    run.settings =
        compareSavedSettings(rocket, goldens.geometry, **resave, savedPresetsOf(presets));
    return run;
}

/// compareExample() with the six presets of the examples and the committed goldens.
[[nodiscard]] ExampleRun runExample(const GoldenExample& example, ExampleSource source)
{
    const Result<DesignGoldens> goldens = loadDesignGoldens(example.name);
    if (!goldens)
    {
        ExampleRun run;
        run.problem = goldens.error().toString();
        return run;
    }
    return compareExample(example, source, Presets::EXAMPLES, *goldens);
}

// ===================================================================================== tests

/// What the design documents of the sixteen examples hold together (the Python survey of the
/// probes of tier 9d, part "example-design": out/survey.txt). The 1608 instances are those of
/// the 1321 entries of the instance maps of the 66 configurations.
[[nodiscard]] DesignCounts designCountsOfTheExamples()
{
    return DesignCounts{.components         = 327,
                        .ids                = 327,
                        .details            = 4854,
                        .configurations     = 66,
                        .motors             = 65,
                        .instanceEntries    = 1321,
                        .instances          = 1608,
                        .loadWarnings       = 0,
                        .massConfigurations = 66,
                        .aero               = {.configurations = 66,
                                               .points         = 1650,
                                               .components     = 15475,
                                               .worstCPs       = 462,
                                               .warnings       = 396}};
}

/// What the sixteen re-saves hold together of the settings (out/resave-survey.txt of the same
/// probes): 25 default deployments and 6 of single configurations, 6 default separations and 4
/// of single configurations, 22 motor mounts with 65 motors, and the six presets of kLostPresets.
[[nodiscard]] SettingsCounts settingsCountsOfTheExamples()
{
    return SettingsCounts{.deployEvents             = 31,
                          .deploymentConfigurations = 6,
                          .separationEvents         = 10,
                          .separationConfigurations = 4,
                          .motorMounts              = 22,
                          .motors                   = 65,
                          .ignitionConfigurations   = 65,
                          .presets                  = 6};
}

/// What comparing the examples, loaded from one source, with their goldens gave.
struct ComparedExamples
{
    int            examples{0};  ///< the "example" inputs of the manifest
    DesignCounts   golden;       ///< what their design documents hold
    DesignCounts   compared;     ///< what was compared
    SettingsCounts saved;        ///< the settings elements of their re-saves
    SettingsCounts settings;     ///< those that were compared
    /// By example: why there is no comparison, the mismatches, and what was not compared in
    /// full. Empty when every example is the design of its goldens.
    std::string problems;
};

/// Adds the comparison @p run of the input @p name to @p compared.
void addRun(ComparedExamples& compared, std::string_view name, const ExampleRun& run)
{
    compared.golden += run.golden;
    compared.compared += run.design.compared;
    compared.saved += run.saved;
    compared.settings += run.settings.compared;
    if (!run.problem.empty() || !run.design.report.empty() || !run.settings.report.empty())
    {
        compared.problems +=
            std::format("{}: {}\n{}{}", name, run.problem, run.design.report, run.settings.report);
    }
    if (run.design.compared != run.golden)
    {
        compared.problems += std::format("{}: compared {}; the files hold {}\n", name,
                                         toText(run.design.compared), toText(run.golden));
    }
    if (run.settings.compared != run.saved)
    {
        compared.problems += std::format("{}: compared {}; the re-save holds {}\n", name,
                                         toText(run.settings.compared), toText(run.saved));
    }
}

/// Compares every "example" input of the manifest, loaded from @p source, with its goldens and
/// sums what the files hold and what was compared. An input that the fixture does not list, one
/// that cannot be loaded, every mismatch and an input that was not compared in full are
/// problems.
[[nodiscard]] ComparedExamples compareExamples(ExampleSource source)
{
    ComparedExamples compared;
    const auto       manifest = QtRocket::Test::loadGoldenManifest();
    if (!manifest)
    {
        compared.problems = manifest.error().toString();
        return compared;
    }
    for (const QtRocket::Test::GoldenInput& input : manifest->inputs)
    {
        if (input.kind != "example")
        {
            continue;
        }
        compared.examples++;
        const GoldenExample* example = findGoldenExample(input.name);
        if (example == nullptr)
        {
            compared.problems +=
                std::format("{}: not among the examples of the fixture\n", input.name);
            continue;
        }
        addRun(compared, input.name, runExample(*example, source));
    }
    return compared;
}

// The sixteen examples, loaded from the files OpenRocket ships, are the designs of the goldens:
// 16 inputs, 327 components with their ids, 66 configurations with 65 motors and 1608
// instances, 66 mass calculations, 1650 points of the aerodynamics, and the 31 deployments, 10
// separations, 65 ignitions and 6 presets of the re-saves. Everything the files hold is
// compared for every example (its line says so when not), and the totals are those of the
// sixteen directories.
TEST(ExampleDesignGolden, TheOriginalFilesGiveTheDesignsOfTheGoldens)
{
    const ComparedExamples compared = compareExamples(ExampleSource::ORIGINAL);
    EXPECT_EQ(compared.problems, "");
    EXPECT_EQ(compared.examples, 16);
    EXPECT_EQ(goldenExamples().size(), 16U);
    EXPECT_EQ(compared.compared, compared.golden) << "what was compared, and what the files hold";
    EXPECT_EQ(compared.golden, designCountsOfTheExamples());
    EXPECT_EQ(compared.settings, compared.saved) << "the settings compared, and those of the files";
    EXPECT_EQ(compared.saved, settingsCountsOfTheExamples());
}

// And so are the sixteen designs loaded from OpenRocket's re-saves: other documents (plain XML
// of file version 1.11, every element the saver writes, no stored flight, no decal image), the
// same designs.
TEST(ExampleDesignGolden, TheResavesGiveTheDesignsOfTheGoldens)
{
    const ComparedExamples compared = compareExamples(ExampleSource::RESAVE);
    EXPECT_EQ(compared.problems, "");
    EXPECT_EQ(compared.examples, 16);
    EXPECT_EQ(compared.compared, compared.golden) << "what was compared, and what the files hold";
    EXPECT_EQ(compared.golden, designCountsOfTheExamples());
    EXPECT_EQ(compared.settings, compared.saved) << "the settings compared, and those of the files";
    EXPECT_EQ(compared.saved, settingsCountsOfTheExamples());
}

// =========================================================================== without presets

/// A <preset> element of an example whose preset an empty preset database does not hold: the
/// golden input, the name of the component and the part of Estes it names, in the order of the
/// elements in the files.
struct LostPreset
{
    std::string_view input;
    std::string_view component;
    std::string_view part;
};

constexpr std::array<LostPreset, 6> kLostPresets{{
    {.input     = "example-3d-printable-nose-cone-and-fins",
     .component = "Paper Body Tube -- DO NOT PRINT",
     .part      = "BT-50FE, 30359"},
    {.input     = "example-deployable-payload",
     .component = "Parachute, plastic, preassembled, 10 in., PN 2262",
     .part      = "PK-10, 2262"},
    {.input     = "example-deployable-payload",
     .component = "Parachute kit, plastic, 8 in., PN 2260",
     .part      = "PK-8, 2260"},
    {.input     = "example-pods-powered-with-recovery-deployment",
     .component = "Nose Cone",
     .part      = "PNC-50YR, 72604"},
    {.input     = "example-pods-powered-with-recovery-deployment",
     .component = "Body Tube",
     .part      = "BT-50, 30352"},
    {.input     = "example-pods-powered-with-recovery-deployment",
     .component = "Nose Cone",
     .part      = "PNC-20, 072606"},
}};

/// A value of geometry.json that a preset alone supplies: the golden input, the path of the
/// component, the entry of its "details", and the value with the preset and without it.
struct LostValue
{
    std::string_view input;
    std::string_view path;
    std::string_view detail;
    double           withPreset;
    double           withoutPreset;
};

/// The thickness of the fore shoulder of the two nose cones of "Pods--powered with recovery
/// deployment.ork": the element of a nose cone stores no fore shoulder (it has none, its
/// length is 0), so the thickness is the preset's or 0.
constexpr std::array<LostValue, 2> kLostValues{{
    {.input         = "example-pods-powered-with-recovery-deployment",
     .path          = "/0/0",
     .detail        = "foreShoulderThickness",
     .withPreset    = 0.0015748,
     .withoutPreset = 0.0},
    {.input         = "example-pods-powered-with-recovery-deployment",
     .path          = "/0/1/0/0",
     .detail        = "foreShoulderThickness",
     .withPreset    = 0.0015748,
     .withoutPreset = 0.0},
}};

/// The warning of OpenRocket's loader for @p lost as the harness writes a warning
/// (ComponentPresetSetter: Warning.fromString(), so of the class Other with the normal priority
/// and without sources).
[[nodiscard]] json lostPresetWarning(const LostPreset& lost)
{
    const std::string text =
        std::format("No matching ComponentPreset for component {} found matching Estes {}",
                    lost.component, lost.part);
    return json{{"type", "Other"},
                {"priority", "NORMAL"},
                {"description", text},
                {"text", text},
                {"sources", json::array()}};
}

/// What changeToWithoutPresets() changed.
struct PresetChanges
{
    int         warnings{0};  ///< the load warnings added
    int         values{0};    ///< the values replaced
    std::string problems;     ///< a value that is not in the document as kLostValues has it
};

/// Makes of @p geometry, the committed geometry.json of the input @p input, the document that
/// OpenRocket's harness writes when it loads the design with an empty preset database: the
/// warnings of kLostPresets as its "loadWarnings" and the values of kLostValues. Adds what it
/// changed to @p changes.
void changeToWithoutPresets(std::string_view input, json& geometry, PresetChanges& changes)
{
    for (const LostPreset& lost : kLostPresets)
    {
        if (lost.input == input)
        {
            geometry.at("loadWarnings").push_back(lostPresetWarning(lost));
            changes.warnings++;
        }
    }
    for (const LostValue& lost : kLostValues)
    {
        if (lost.input != input)
        {
            continue;
        }
        for (json& component : geometry.at("components"))
        {
            if (component.at("path").get<std::string>() != lost.path)
            {
                continue;
            }
            json& value = component.at("details").at(std::string{lost.detail});
            if (value.get<double>() != lost.withPreset)
            {
                changes.problems += std::format("{} {}: the golden {} is {}\n", input, lost.path,
                                                lost.detail, value.dump());
            }
            value = lost.withoutPreset;
            changes.values++;
        }
    }
}

/// What the examples give without component presets.
struct WithoutPresets
{
    PresetChanges changes;  ///< what the documents of OpenRocket differ in then
    /// The comparison with those documents and with the re-saves, as for the two sources with
    /// the presets: what the files hold, what was compared, and by example why there is no
    /// comparison, the mismatches, and what was not compared in full.
    ComparedExamples compared;
};

/// Loads every example from its original file with an empty preset database and compares it
/// with the design documents OpenRocket writes then (changeToWithoutPresets()) and with the
/// settings of its re-save, of whose presets the design then holds none.
[[nodiscard]] WithoutPresets examplesWithoutPresets()
{
    WithoutPresets result;
    for (const GoldenExample& example : goldenExamples())
    {
        result.compared.examples++;
        Result<DesignGoldens> goldens = loadDesignGoldens(example.name);
        if (!goldens)
        {
            result.compared.problems +=
                std::format("{}: {}\n", example.name, goldens.error().toString());
            continue;
        }
        changeToWithoutPresets(example.name, goldens->geometry, result.changes);
        addRun(result.compared, example.name,
               compareExample(example, ExampleSource::ORIGINAL, Presets::NONE, *goldens));
    }
    return result;
}

// With an empty preset database the sixteen examples give the design documents of the goldens
// but for the six warnings of the loads and the two values that their presets alone supply:
// what OpenRocket itself gives then. (The six components keep what their elements store; that
// state is compared with OpenRocket's line for line in
// tests/core/file/openrocket/example_files_tests.cpp, as loaded and unsettled.) As with the
// presets, everything the files hold is compared for every example and the totals are pinned;
// the settings are those of the re-saves, and none of the six components has a preset.
TEST(ExampleDesignGoldenWithoutPresets, OnlyTheSixWarningsAndTheTwoShoulderThicknessesDiffer)
{
    DesignCounts expected = designCountsOfTheExamples();
    expected.loadWarnings = 6;

    const WithoutPresets result = examplesWithoutPresets();
    EXPECT_EQ(result.compared.problems, "");
    EXPECT_EQ(result.changes.problems, "");
    EXPECT_EQ(result.compared.examples, 16);
    EXPECT_EQ(result.changes.warnings, 6);
    EXPECT_EQ(result.changes.values, 2);
    EXPECT_EQ(result.compared.compared, result.compared.golden)
        << "what was compared, and what the files hold";
    EXPECT_EQ(result.compared.golden, expected);
    EXPECT_EQ(result.compared.settings, result.compared.saved)
        << "the settings compared, and those of the files";
    EXPECT_EQ(result.compared.saved, settingsCountsOfTheExamples());
}

// ================================================================================= mutations

// The comparison is not vacuous where it is new: a golden value changed in a copy of a
// geometry.json is reported, in one line that names it (more for a change of the structure).
// The values are those the examples bring to the comparison and those only a loaded design is
// compared in. (That a number is compared within its tolerance is the business of the tests of
// the test rockets and of GoldenMismatches.)

/// A loaded and settled example with the warnings of its load and its design documents. The
/// subjects are loaded from the re-saves, the smaller documents: it is the comparison that is
/// under test here, not the load.
struct Subject
{
    LoadedDocument loaded;
    DesignGoldens  goldens;
};

/// The subjects of the mutations, by the name of their golden input: each is loaded once.
using Subjects = std::map<std::string, Subject, std::less<>>;

/// The subject of the golden input @p input, loaded on first use; null, with the reason in
/// @p problem, when it cannot be had.
[[nodiscard]] const Subject* subjectOf(Subjects& subjects, std::string_view input,
                                       std::string& problem)
{
    if (const auto known = subjects.find(input); known != subjects.end())
    {
        return &known->second;
    }
    const GoldenExample* example = findGoldenExample(input);
    if (example == nullptr)
    {
        problem = "no such example";
        return nullptr;
    }
    Result<DesignGoldens>  goldens = loadDesignGoldens(input);
    Result<LoadedDocument> loaded  = loadSettledGoldenExample(*example, ExampleSource::RESAVE);
    if (!goldens || !loaded)
    {
        problem = !goldens ? goldens.error().toString() : loaded.error().toString();
        return nullptr;
    }
    const auto added = subjects.emplace(
        std::string{input}, Subject{.loaded = std::move(*loaded), .goldens = std::move(*goldens)});
    return &added.first->second;
}

/// The report of the comparison of @p subject with the geometry.json document @p geometry.
[[nodiscard]] std::string geometryReport(const Subject& subject, const json& geometry)
{
    return compareLoadedGeometry(subject.loaded.document->getRocket(), subject.loaded.warnings,
                                 geometry, preferencesOf(Presets::EXAMPLES))
        .report;
}

/// The object of a geometry.json document that a mutation changes: the document itself, or the
/// first component, or the first motor of a configuration, whose value at the JSON pointer
/// `where` is `is`.
struct Target
{
    enum class Kind
    {
        DOCUMENT,
        COMPONENT,
        MOTOR,
    };
    Kind             kind{Kind::DOCUMENT};
    std::string_view where;
    json             is;
};

[[nodiscard]] Target theDocument()
{
    return {};
}

[[nodiscard]] Target theComponentWith(std::string_view where, json is)
{
    return {.kind = Target::Kind::COMPONENT, .where = where, .is = std::move(is)};
}

[[nodiscard]] Target theMotorWith(std::string_view where, json is)
{
    return {.kind = Target::Kind::MOTOR, .where = where, .is = std::move(is)};
}

/// Whether the value of @p object at the JSON pointer @p where is @p is.
[[nodiscard]] bool has(const json& object, std::string_view where, const json& is)
{
    const json::json_pointer pointer{std::string{where}};
    return object.contains(pointer) && object.at(pointer) == is;
}

/// The JSON pointer of the first motor of a configuration of @p geometry that @p target
/// describes; nullopt when there is none.
[[nodiscard]] std::optional<std::string> motorPointer(const json& geometry, const Target& target)
{
    const json& configurations = geometry.at("configurations");
    for (std::size_t c = 0; c < configurations.size(); c++)
    {
        const json& motors = configurations.at(c).at("motors");
        for (std::size_t m = 0; m < motors.size(); m++)
        {
            if (has(motors.at(m), target.where, target.is))
            {
                return std::format("/configurations/{}/motors/{}", c, m);
            }
        }
    }
    return std::nullopt;
}

/// The JSON pointer of the object of @p geometry that @p target describes ("" for the
/// document); nullopt when the document has no such object.
[[nodiscard]] std::optional<std::string> pointerOf(const json& geometry, const Target& target)
{
    if (target.kind == Target::Kind::DOCUMENT)
    {
        return std::string{};
    }
    if (target.kind == Target::Kind::MOTOR)
    {
        return motorPointer(geometry, target);
    }
    const json& components = geometry.at("components");
    for (std::size_t i = 0; i < components.size(); i++)
    {
        if (has(components.at(i), target.where, target.is))
        {
            return std::format("/components/{}", i);
        }
    }
    return std::nullopt;
}

/// The operations of a JSON patch (RFC 6902) a mutation is made of, each on a path in its
/// target: a value that is there replaced, a value added (a new key, or behind the last
/// element of a list for "/-"), a value removed, and a value moved to another place.
[[nodiscard]] json replaced(std::string_view path, json value)
{
    return json{{"op", "replace"}, {"path", path}, {"value", std::move(value)}};
}

[[nodiscard]] json added(std::string_view path, json value)
{
    return json{{"op", "add"}, {"path", path}, {"value", std::move(value)}};
}

[[nodiscard]] json removed(std::string_view path)
{
    return json{{"op", "remove"}, {"path", path}};
}

[[nodiscard]] json moved(std::string_view from, std::string_view path)
{
    return json{{"op", "move"}, {"from", from}, {"path", path}};
}

/// A change to the geometry.json of an example that the comparison has to report.
struct GeometryMutation
{
    std::string_view name;      ///< what is changed
    std::string_view input;     ///< the golden input whose document is changed
    Target           target;    ///< the object of the document that is changed
    json             change;    ///< the operation on it (replaced(), added(), removed(), moved())
    std::string_view line;      ///< what the line of the report that names the change starts with
    int              lines{1};  ///< the number of lines the change is reported in
};

/// @p geometry after @p mutation; nullopt when the document has no target for it. (An
/// operation that does not fit the document throws, which fails the test.)
[[nodiscard]] std::optional<json> changed(const json& geometry, const GeometryMutation& mutation)
{
    const std::optional<std::string> target = pointerOf(geometry, mutation.target);
    if (!target)
    {
        return std::nullopt;
    }
    json operation = mutation.change;
    for (const char* key : {"path", "from"})
    {
        if (operation.contains(key))
        {
            operation.at(key) = *target + operation.at(key).get<std::string>();
        }
    }
    return geometry.patch(json::array({operation}));
}

constexpr std::string_view kPrinted   = "example-3d-printable-nose-cone-and-fins";
constexpr std::string_view kAirstart  = "example-airstart-timing";
constexpr std::string_view kClustered = "example-clustered-motors";
constexpr std::string_view kDual      = "example-dual-parachute-deployment";
constexpr std::string_view kPods      = "example-pods-powered-with-recovery-deployment";
constexpr std::string_view kTubeFins  = "example-tube-fin-rocket";
constexpr std::string_view kTwoStage  = "example-two-stage-high-power-rocket";

/// The id of the default flight configuration of every rocket.
constexpr std::string_view kDefaultConfigurationId = "ffffffff-f4f2-f1f0-0000-00000000162c";
/// An id that no component and no flight configuration has.
constexpr const char* kAnotherId = "00000000-0000-4000-8000-000000000000";

/// The changes to what only a loaded design is compared in: the values the file records once,
/// the ids, the order of the components and the warnings of the load.
[[nodiscard]] std::vector<GeometryMutation> loadedDesignMutations()
{
    return {
        {.name   = "RocketName",
         .input  = kPods,
         .target = theDocument(),
         .change = replaced("/rocketName", "Another rocket"),
         .line   = R"(  rocketName: expected "Another rocket", got "Pods With Recovery")"},
        {.name   = "SelectedConfiguration",
         .input  = kPods,
         .target = theDocument(),
         .change = replaced("/selectedConfiguration", kDefaultConfigurationId),
         .line   = "  selectedConfiguration: index: expected 0, got 1",
         .lines  = 2},
        {.name   = "ALoadWarning",
         .input  = kPods,
         .target = theDocument(),
         .change = added("/loadWarnings/-", lostPresetWarning(kLostPresets.at(3))),
         .line   = "  loadWarnings: number: expected 1, got 0"},
        {.name   = "AnId",
         .input  = kPods,
         .target = theComponentWith("/path", "/0/1/0"),
         .change = replaced("/id", kAnotherId),
         .line   = R"(  /0/1/0 "Pod Set": id: expected "00000000-0000-4000-8000-000000000000")"},
        {.name   = "TheOrderOfTwoComponents",
         .input  = kPods,
         .target = theDocument(),
         .change = moved("/components/2", "/components/3"),
         .line   = R"(  component 2: path: expected "/0/1", got "/0/0")",
         .lines  = 4},
        {.name   = "AComponentLess",
         .input  = kPods,
         .target = theDocument(),
         .change = removed("/components/14"),
         .line   = "  components: number: expected 14, got 15"},
        {.name   = "AConfigurationLess",
         .input  = kPods,
         .target = theDocument(),
         .change = removed("/configurations/1"),
         .line   = "  configurations: number: expected 1, got 2",
         .lines  = 2},
        {.name   = "AFileFieldNothingCompares",
         .input  = kPods,
         .target = theDocument(),
         .change = added("/generator", "another dumper"),
         .line   = "  generator: not compared"},
    };
}

/// The changes to the details of the three classes of components that only the examples have.
[[nodiscard]] std::vector<GeometryMutation> newClassMutations()
{
    return {
        {.name   = "StreamerStripLength",
         .input  = kPods,
         .target = theComponentWith("/type", "Streamer"),
         .change = replaced("/details/stripLength", 9.0),
         .line   = "  details.stripLength: expected 9, got "},
        {.name   = "StreamerStripWidth",
         .input  = kPods,
         .target = theComponentWith("/type", "Streamer"),
         .change = replaced("/details/stripWidth", 9.0),
         .line   = "  details.stripWidth: expected 9, got "},
        {.name   = "StreamerDragCoefficient",
         .input  = kPods,
         .target = theComponentWith("/type", "Streamer"),
         .change = replaced("/details/cd", 9.0),
         .line   = "  details.cd: expected 9, got "},
        {.name   = "StreamerMaterial",
         .input  = kPods,
         .target = theComponentWith("/type", "Streamer"),
         .change = replaced("/details/material/density", 9.0),
         .line   = "  details.material.density: expected 9, got "},
        {.name   = "EllipticalFinSetHeight",
         .input  = kClustered,
         .target = theComponentWith("/type", "EllipticalFinSet"),
         .change = replaced("/details/height", 9.0),
         .line   = "  details.height: expected 9, got "},
        {.name   = "TubeFinSetOuterRadius",
         .input  = kTubeFins,
         .target = theComponentWith("/type", "TubeFinSet"),
         .change = replaced("/details/outerRadius", 9.0),
         .line   = "  details.outerRadius: expected 9, got "},
        {.name   = "TubeFinSetInnerRadius",
         .input  = kTubeFins,
         .target = theComponentWith("/type", "TubeFinSet"),
         .change = replaced("/details/innerRadius", 9.0),
         .line   = "  details.innerRadius: expected 9, got "},
        {.name   = "TubeFinSetThickness",
         .input  = kTubeFins,
         .target = theComponentWith("/type", "TubeFinSet"),
         .change = replaced("/details/thickness", 9.0),
         .line   = "  details.thickness: expected 9, got "},
        {.name   = "TubeFinSetBodyRadius",
         .input  = kTubeFins,
         .target = theComponentWith("/type", "TubeFinSet"),
         .change = replaced("/details/bodyRadius", 9.0),
         .line   = "  details.bodyRadius: expected 9, got "},
        {.name   = "TubeFinSetFinCount",
         .input  = kTubeFins,
         .target = theComponentWith("/type", "TubeFinSet"),
         .change = replaced("/details/finCount", 9),
         .line   = "  details.finCount: expected 9, got 6"},
        {.name   = "TubeFinSetBaseRotation",
         .input  = kTubeFins,
         .target = theComponentWith("/type", "TubeFinSet"),
         .change = replaced("/details/baseRotation", 9.0),
         .line   = "  details.baseRotation: expected 9, got "},
    };
}

/// The changes to the values that the examples are the first to have other than by default.
[[nodiscard]] std::vector<GeometryMutation> newValueMutations()
{
    return {
        {.name   = "TheComponentWhoseOverrideCoversTheMass",
         .input  = kDual,
         .target = theComponentWith("/overrides/massOverriddenBy", "/0"),
         .change = replaced("/overrides/massOverriddenBy", "/"),
         .line   = R"(  massOverriddenBy: expected "/", got "/0")"},
        {.name   = "SubcomponentsOverriddenMass",
         .input  = kDual,
         .target = theComponentWith("/overrides/subcomponentsOverriddenMass", true),
         .change = replaced("/overrides/subcomponentsOverriddenMass", false),
         .line   = "  subcomponentsOverriddenMass: expected false, got true"},
        {.name   = "AFixedAngleMethod",
         .input  = kTubeFins,
         .target = theComponentWith("/details/angleMethod", "FIXED"),
         .change = replaced("/details/angleMethod", "RELATIVE"),
         .line   = R"(  details.angleMethod: expected "RELATIVE", got "FIXED")"},
        {.name   = "AClusterOfThree",
         .input  = kAirstart,
         .target = theComponentWith("/details/clusterConfiguration", "3-ring"),
         .change = replaced("/details/clusterConfiguration", "single"),
         .line   = R"(  details.clusterConfiguration: expected "single", got "3-ring")"},
        {.name   = "AClusterOfFour",
         .input  = kClustered,
         .target = theComponentWith("/details/clusterConfiguration", "4-ring"),
         .change = replaced("/details/clusterConfiguration", "single"),
         .line   = R"(  details.clusterConfiguration: expected "single", got "4-ring")"},
        {.name   = "ARoughFinish",
         .input  = kPrinted,
         .target = theComponentWith("/details/finish", "ROUGH"),
         .change = replaced("/details/finish", "NORMAL"),
         .line   = R"(  details.finish: expected "NORMAL", got "ROUGH")"},
        {.name   = "AnUnfinishedSurface",
         .input  = kPrinted,
         .target = theComponentWith("/details/finish", "UNFINISHED"),
         .change = replaced("/details/finish", "NORMAL"),
         .line   = R"(  details.finish: expected "NORMAL", got "UNFINISHED")"},
        {.name   = "IgnitionAtBurnout",
         .input  = kTwoStage,
         .target = theMotorWith("/ignitionEvent", "BURNOUT"),
         .change = replaced("/ignitionEvent", "AUTOMATIC"),
         .line   = R"(  motor in /0/5/3/2 ignitionEvent: expected "AUTOMATIC", got "BURNOUT")"},
        {.name   = "AnIgnitionDelay",
         .input  = kAirstart,
         .target = theMotorWith("/ignitionDelay", 2.0),
         .change = replaced("/ignitionDelay", 2.5),
         .line   = "  motor in /0/3/5 ignitionDelay: expected 2.5, got 2 "},
        {.name   = "APluggedMotor",
         .input  = kAirstart,
         .target = theMotorWith("/ejectionDelay", "Infinity"),
         .change = replaced("/ejectionDelay", 5.0),
         .line   = "  motor in /0/3/4 ejectionDelay: expected 5, got inf "},
        {.name   = "ANameOfTheUsers",
         .input  = kAirstart,
         .target = theDocument(),
         .change = replaced("/configurations/2/name", "Airstart @3s"),
         .line   = R"(  name: expected "Airstart @3s", got "Airstart @2s")"},
    };
}

/// The changes that add a field nothing compares, to every kind of object of the document that
/// the comparison of the components and the configurations reads.
[[nodiscard]] std::vector<GeometryMutation> newFieldMutations()
{
    return {
        {.name   = "AComponentField",
         .input  = kPods,
         .target = theComponentWith("/type", "Streamer"),
         .change = added("/volume", 1.0),
         .line   = "  volume: not compared"},
        {.name   = "AnOverrideField",
         .input  = kPods,
         .target = theComponentWith("/type", "Streamer"),
         .change = added("/overrides/overrideLength", 1.0),
         .line   = "  overrides.overrideLength: not compared"},
        {.name   = "AConfigurationField",
         .input  = kPods,
         .target = theDocument(),
         .change = added("/configurations/1/stability", 1.0),
         .line   = "  stability: not compared"},
        {.name   = "ABoundingBoxField",
         .input  = kPods,
         .target = theDocument(),
         .change = added("/configurations/1/boundingBox/centre", 1.0),
         .line   = "  boundingBox.centre: not compared"},
        {.name   = "AMotorField",
         .input  = kPods,
         .target = theMotorWith("/mount", "/0/1/2"),
         .change = added("/thrust", 1.0),
         .line   = "  motor in /0/1/2.thrust: not compared"},
        {.name   = "AnInstanceEntryField",
         .input  = kPods,
         .target = theDocument(),
         .change = added("/configurations/1/instances/1/count", 1),
         .line   = "  instances of /0.count: not compared"},
        {.name   = "AnInstanceField",
         .input  = kPods,
         .target = theDocument(),
         .change = added("/configurations/1/instances/1/instances/0/scale", 1.0),
         .line   = "  instance 0 of /0.scale: not compared"},
    };
}

/// What is wrong with the report of the comparison of the example of @p mutation with its
/// changed geometry.json; "" when the report has the lines expected.
[[nodiscard]] std::string problemOf(const GeometryMutation& mutation, Subjects& subjects)
{
    std::string          problem;
    const Subject* const subject = subjectOf(subjects, mutation.input, problem);
    if (subject == nullptr)
    {
        return std::format("{}: {}\n", mutation.name, problem);
    }
    const std::optional<json> geometry = changed(subject->goldens.geometry, mutation);
    if (!geometry || *geometry == subject->goldens.geometry)
    {
        return std::format("{}: nothing to change in {}\n", mutation.name, mutation.input);
    }
    const std::string report = geometryReport(*subject, *geometry);
    if (!report.contains(std::format("\n{}", mutation.line)) ||
        std::ranges::count(report, '\n') != mutation.lines + 1)
    {
        return std::format("{}: the report is\n{}", mutation.name, report);
    }
    return "";
}

/// "<n> changes", and behind it every mutation of @p mutations that is not reported as
/// expected, with the report it got.
[[nodiscard]] std::string reported(const std::vector<GeometryMutation>& mutations,
                                   Subjects&                            subjects)
{
    std::string problems;
    for (const GeometryMutation& mutation : mutations)
    {
        problems += problemOf(mutation, subjects);
    }
    return std::format("{} changes{}{}", mutations.size(), problems.empty() ? "" : "\n", problems);
}

/// The reports of the comparisons of the subjects of the mutations with their documents as
/// they are, which have to be empty.
[[nodiscard]] std::string reportsOfTheUnchangedDocuments(Subjects& subjects)
{
    std::string reports;
    for (const std::string_view input :
         {kPrinted, kAirstart, kClustered, kDual, kPods, kTubeFins, kTwoStage})
    {
        std::string          problem;
        const Subject* const subject = subjectOf(subjects, input, problem);
        reports +=
            subject == nullptr ? problem : geometryReport(*subject, subject->goldens.geometry);
    }
    return reports;
}

// One test, so that each of the seven examples the changes are made to is loaded once: the
// documents as they are match, and then every change is reported: to what only a loaded design
// is compared in, to a detail of the classes only the examples have, to a value only the
// examples have, and a field that nothing compares.
TEST(ExampleDesignGoldenMutations, AChangedValueOfAGeometryDocumentIsReported)
{
    Subjects subjects;
    EXPECT_EQ(reportsOfTheUnchangedDocuments(subjects), "");
    EXPECT_EQ(reported(loadedDesignMutations(), subjects), "8 changes");
    EXPECT_EQ(reported(newClassMutations(), subjects), "11 changes");
    EXPECT_EQ(reported(newValueMutations(), subjects), "11 changes");
    EXPECT_EQ(reported(newFieldMutations(), subjects), "7 changes");
}

// ------------------------------------------------------------------- mass and aerodynamics

/// The report of the comparison of the tube fin example, the smallest of the sixteen, with its
/// three design documents after @p change has been made to them (nullptr: as they are).
[[nodiscard]] std::string designReportAfter(void (*change)(DesignGoldens& goldens))
{
    const GoldenExample*  example = findGoldenExample(kTubeFins);
    Result<DesignGoldens> goldens = loadDesignGoldens(kTubeFins);
    if (example == nullptr || !goldens)
    {
        return "no golden data of the tube fin example";
    }
    if (change != nullptr)
    {
        change(*goldens);
    }
    const Result<LoadedDocument> loaded = loadSettledGoldenExample(*example, ExampleSource::RESAVE);
    if (!loaded)
    {
        return loaded.error().toString();
    }
    return compareLoadedDesign(loaded->document->getRocket(), loaded->warnings, *goldens,
                               preferencesOf(Presets::EXAMPLES))
        .report;
}

/// Gives the launch mass of the configuration with the motor another value.
void changeALaunchMass(DesignGoldens& goldens)
{
    goldens.mass.at("configurations").at(1).at("launch").at("mass") = 9.0;
}

/// Gives the stall angle of the aerodynamics another value.
void changeTheStallAngle(DesignGoldens& goldens)
{
    goldens.aero.at("stallAngle") = 9.0;
}

/// Moves four values that the comparison of mass.json reads, each by a part of its tolerance
/// (GoldenDesign.h: relative 1e-9, with an absolute 1e-15 for a value that is zero; 1e-12 m for
/// a location):
/// - the launch mass, a value of a rigid body, and the mass of the nose cone in the CM analysis,
///   each by @p relative of itself;
/// - the z of the launch CM, which is zero but for the rounding of a sum (3.6e-21 m in the
///   document), to @p zero;
/// - the x of the location of the body tube by @p location (in geometry.json: the locations the
///   mass calculations rest on, which the comparison of geometry.json itself holds to 1e-9 m).
/// The configuration is the one with the motor, "[D12-7]".
void moveTheMassValues(DesignGoldens& goldens, double relative, double zero, double location)
{
    json& configuration                       = goldens.mass.at("configurations").at(1);
    json& launchMass                          = configuration.at("launch").at("mass");
    launchMass                                = launchMass.get<double>() * (1 + relative);
    json& noseMass                            = configuration.at("cmAnalysis").at(1).at("eachMass");
    noseMass                                  = noseMass.get<double>() * (1 + relative);
    configuration.at("launch").at("cm").at(2) = zero;
    json& x = goldens.geometry.at("components").at(3).at("componentLocations").at(0).at(0);
    x       = x.get<double>() + location;
}

/// Each of the four by half its tolerance.
void moveWithinTheMassTolerances(DesignGoldens& goldens)
{
    moveTheMassValues(goldens, 5e-10, 5e-16, 5e-13);
}

/// Each of the four by twice its tolerance.
void moveBeyondTheMassTolerances(DesignGoldens& goldens)
{
    moveTheMassValues(goldens, 2e-9, 2e-15, 2e-12);
}

/// How often @p part is in @p text.
[[nodiscard]] int occurrences(std::string_view text, std::string_view part)
{
    int         count = 0;
    std::size_t at    = text.find(part);
    while (at != std::string_view::npos)
    {
        count++;
        at = text.find(part, at + part.size());
    }
    return count;
}

// The comparison of a loaded design goes on to its masses and its aerodynamics: a changed value
// of mass.json or of aero.json is in its report (what GoogleTest was told about the masses is
// under "mass.json:"). What is compared of the two documents is the business of the tests of
// the test rockets, and so is how closely aero.json is (GoldenMismatches). How closely
// mass.json is compared is held here, where the examples meet it: four values moved by half
// their tolerance are still the design's, and moved by twice their tolerance each of them is
// reported, in an entry of its own, and nothing of the other two documents is.
TEST(ExampleDesignGoldenMutations, AChangedValueOfTheMassOrTheAerodynamicsIsReported)
{
    EXPECT_EQ(designReportAfter(nullptr), "");

    const std::string mass = designReportAfter(changeALaunchMass);
    EXPECT_TRUE(mass.starts_with("mass.json:\n")) << mass;
    EXPECT_TRUE(mass.contains("[D12-7] launch mass")) << mass;
    EXPECT_FALSE(mass.contains("stallAngle")) << mass;

    const std::string aero = designReportAfter(changeTheStallAngle);
    EXPECT_TRUE(aero.starts_with("file:\n  stallAngle: expected 9, got ")) << aero;
    EXPECT_FALSE(aero.contains("mass.json")) << aero;

    EXPECT_EQ(designReportAfter(moveWithinTheMassTolerances), "");

    const std::string beyond = designReportAfter(moveBeyondTheMassTolerances);
    EXPECT_EQ(occurrences(beyond, "mass.json:\n"), 4) << beyond;
    EXPECT_TRUE(beyond.contains("[D12-7] launch mass")) << beyond;
    EXPECT_TRUE(beyond.contains("[D12-7] analysis Nose cone eachMass")) << beyond;
    EXPECT_TRUE(beyond.contains("[D12-7] launch cm z")) << beyond;
    EXPECT_TRUE(beyond.contains("/0/1 #0")) << beyond;
    EXPECT_FALSE(beyond.contains(": expected ")) << "geometry.json or aero.json: " << beyond;
}

// ---------------------------------------------------------------------------------- settings

// Nor is the comparison of the settings: an element of the re-save of "Two stage high power
// rocket.ork" changed in a copy of the document is reported. The design has recovery devices
// with deployments of their own for both configurations, a booster with separations of its
// own, and two motor mounts. Its first configuration has the id cf7a0b8c-.... The presets are
// changed in the re-save of "Pods--powered with recovery deployment.ork", which has three.

/// What a mutation does to an element of the re-save.
enum class Edit
{
    TEXT,              ///< gives it another text
    REMOVE,            ///< removes it
    CONFIGURATION_ID,  ///< gives it another configid
    ATTRIBUTE,         ///< gives its attribute `attribute` another value
    ADD_CHILD,         ///< gives it a new child
};

/// A change to the re-save of an example that the comparison of the settings has to report: to
/// the first element `element` whose parent is `parent`.
struct SettingsMutation
{
    std::string_view name;  ///< what is changed
    std::string_view element;
    std::string_view parent;
    Edit             edit{Edit::TEXT};
    const char*      attribute{""};  ///< Edit::ATTRIBUTE: the attribute that is changed
    /// The new text, the new configid, the new value of the attribute, the name of the new child.
    const char* value{""};
    std::string line;      ///< what a line of the report starts with
    int         lines{1};  ///< the number of lines the change is reported in
};

/// Makes the change of @p mutation to @p resave. Returns whether there was something to change.
/// (The document is changed through the handles of its nodes, which pugixml hands out for a
/// constant document too.)
[[nodiscard]] bool apply(const SettingsMutation& mutation, const pugi::xml_document& resave)
{
    pugi::xml_node element = resave.find_node([&mutation](const pugi::xml_node& node) {
        return std::string_view{node.name()} == mutation.element &&
               std::string_view{node.parent().name()} == mutation.parent;
    });
    if (element.empty())
    {
        return false;
    }
    switch (mutation.edit)
    {
        case Edit::TEXT:
            return std::string_view{element.child_value()} != mutation.value &&
                   element.text().set(mutation.value);
        case Edit::REMOVE:
            return element.parent().remove_child(element);
        case Edit::CONFIGURATION_ID:
            return element.attribute("configid").set_value(mutation.value);
        case Edit::ATTRIBUTE:
            return element.attribute(mutation.attribute).set_value(mutation.value);
        case Edit::ADD_CHILD:
            return !element.append_child(mutation.value).empty();
    }
    return false;
}

/// The line of the report about the element @p element of the first flight configuration of the
/// two-stage example, which says @p rest of it.
[[nodiscard]] std::string ofTheFirstConfiguration(std::string_view element, std::string_view rest)
{
    return std::format(R"(  <{} configid="cf7a0b8c-19e9-4112-912f-505bc8e01067">{})", element,
                       rest);
}

/// The changes to the deployments and the separations.
[[nodiscard]] std::vector<SettingsMutation> recoveryMutations()
{
    return {
        {.name    = "TheDefaultDeployEvent",
         .element = "deployevent",
         .parent  = "parachute",
         .value   = "never",
         .line    = R"(  default <deployevent>: expected "never", got "ejection")"},
        {.name    = "TheDefaultDeployAltitude",
         .element = "deployaltitude",
         .parent  = "parachute",
         .value   = "123.5",
         .line    = "  default <deployaltitude>: expected 123.5, got 200 "},
        {.name    = "TheDefaultDeployDelay",
         .element = "deploydelay",
         .parent  = "parachute",
         .value   = "1.5",
         .line    = "  default <deploydelay>: expected 1.5, got 0 "},
        {.name    = "ADefaultDeployEventLess",
         .element = "deployevent",
         .parent  = "parachute",
         .edit    = Edit::REMOVE,
         .line    = "  default <deployevent>: number: expected 1, got 0",
         .lines   = 2},
        {.name    = "TheDeployEventOfAConfiguration",
         .element = "deployevent",
         .parent  = "deploymentconfiguration",
         .value   = "never",
         .line    = ofTheFirstConfiguration("deploymentconfiguration",
                                            R"( <deployevent>: expected "never", got "altitude")")},
        {.name    = "TheDeployAltitudeOfAConfiguration",
         .element = "deployaltitude",
         .parent  = "deploymentconfiguration",
         .value   = "123.5",
         .line    = ofTheFirstConfiguration("deploymentconfiguration",
                                            " <deployaltitude>: expected 123.5, got 152.4 ")},
        // The numbers of the file are compared exactly: the double next to 152.4 is another one.
        {.name    = "TheDeployAltitudeOfAConfigurationByOneUnitInTheLastPlace",
         .element = "deployaltitude",
         .parent  = "deploymentconfiguration",
         .value   = "152.40000000000003",
         .line =
             ofTheFirstConfiguration("deploymentconfiguration",
                                     " <deployaltitude>: expected 152.40000000000003, got 152.4 ")},
        {.name    = "ADeploymentConfigurationLess",
         .element = "deploymentconfiguration",
         .parent  = "parachute",
         .edit    = Edit::REMOVE,
         .line    = "  <deploymentconfiguration>: number: expected 1, got 2"},
        {.name    = "ADeploymentOfAConfigurationTheRocketLacks",
         .element = "deploymentconfiguration",
         .parent  = "parachute",
         .edit    = Edit::CONFIGURATION_ID,
         .value   = kAnotherId,
         .line = R"(  <deploymentconfiguration configid="00000000-0000-4000-8000-000000000000">: )"
                 "no flight configuration of the rocket has this id"},
        {.name    = "TheDefaultSeparationEvent",
         .element = "separationevent",
         .parent  = "stage",
         .value   = "never",
         .line    = R"(  default <separationevent>: expected "never", got "ejection")"},
        {.name    = "TheDefaultSeparationDelay",
         .element = "separationdelay",
         .parent  = "stage",
         .value   = "1.5",
         .line    = "  default <separationdelay>: expected 1.5, got 0 "},
        {.name    = "TheSeparationEventOfAConfiguration",
         .element = "separationevent",
         .parent  = "separationconfiguration",
         .value   = "never",
         .line = ofTheFirstConfiguration("separationconfiguration",
                                         R"( <separationevent>: expected "never", got "burnout")")},
        {.name    = "TheSeparationAltitudeOfAConfiguration",
         .element = "separationaltitude",
         .parent  = "separationconfiguration",
         .value   = "123.5",
         .line    = ofTheFirstConfiguration("separationconfiguration",
                                            " <separationaltitude>: expected 123.5, got 200 ")},
        {.name    = "ASeparationConfigurationLess",
         .element = "separationconfiguration",
         .parent  = "stage",
         .edit    = Edit::REMOVE,
         .line    = "  <separationconfiguration>: number: expected 1, got 2"},
    };
}

/// The changes to the motor mounts.
[[nodiscard]] std::vector<SettingsMutation> ignitionMutations()
{
    return {
        {.name    = "TheDefaultIgnitionEvent",
         .element = "ignitionevent",
         .parent  = "motormount",
         .value   = "never",
         .line    = R"(  <motormount> <ignitionevent>: expected "never", got "automatic")"},
        {.name    = "TheDefaultIgnitionDelay",
         .element = "ignitiondelay",
         .parent  = "motormount",
         .value   = "1.5",
         .line    = "  <motormount> <ignitiondelay>: expected 1.5, got 0 "},
        {.name    = "TheOverhang",
         .element = "overhang",
         .parent  = "motormount",
         .value   = "0.5",
         .line    = "  <motormount> <overhang>: expected 0.5, got "},
        {.name    = "TheIgnitionEventOfAConfiguration",
         .element = "ignitionevent",
         .parent  = "ignitionconfiguration",
         .value   = "never",
         .line = ofTheFirstConfiguration("ignitionconfiguration",
                                         R"( <ignitionevent>: expected "never", got "burnout")")},
        {.name    = "TheIgnitionDelayOfAConfiguration",
         .element = "ignitiondelay",
         .parent  = "ignitionconfiguration",
         .value   = "1.5",
         .line    = ofTheFirstConfiguration("ignitionconfiguration",
                                            " <ignitiondelay>: expected 1.5, got 0 ")},
        {.name    = "AnIgnitionConfigurationLess",
         .element = "ignitionconfiguration",
         .parent  = "motormount",
         .edit    = Edit::REMOVE,
         .line    = ofTheFirstConfiguration("ignitionconfiguration", ": not in the re-save")},
        {.name    = "AnIgnitionConfigurationMore",
         .element = "motormount",
         .parent  = "innertube",
         .edit    = Edit::ADD_CHILD,
         .value   = "ignitionconfiguration",
         .line    = "  <ignitionconfiguration>: number: expected 3, got 2"},
        {.name    = "TheEjectionDelayOfAMotor",
         .element = "delay",
         .parent  = "motor",
         .value   = "1.5",
         .line    = ofTheFirstConfiguration("motor", " <delay>: expected 1.5, got 0 ")},
        {.name    = "APluggedMotor",
         .element = "delay",
         .parent  = "motor",
         .value   = "none",
         .line    = ofTheFirstConfiguration("motor", " <delay>: expected inf, got 0 ")},
        {.name    = "TheDesignationOfAMotor",
         .element = "designation",
         .parent  = "motor",
         .value   = "Z9",
         .line = ofTheFirstConfiguration("motor", R"( <designation>: expected "Z9", got "H148R")")},
        {.name    = "TheDigestOfAMotor",
         .element = "digest",
         .parent  = "motor",
         .value   = "00",
         .line    = ofTheFirstConfiguration("motor", R"( <digest>: expected "00", got ")")},
        {.name    = "AMotorLess",
         .element = "motor",
         .parent  = "motormount",
         .edit    = Edit::REMOVE,
         .line    = ofTheFirstConfiguration("motor", ": not in the re-save")},
        {.name    = "AMotorMore",
         .element = "motormount",
         .parent  = "innertube",
         .edit    = Edit::ADD_CHILD,
         .value   = "motor",
         .line    = "  <motor>: number: expected 3, got 2"},
        {.name    = "AMotorMountLess",
         .element = "motormount",
         .parent  = "innertube",
         .edit    = Edit::REMOVE,
         .line    = "  <motormount>: number: expected 0, got 1"},
    };
}

/// The changes to the presets: to the first <preset> of the pods example, that of its first
/// nose cone (PNC-50YR of Estes), and to its streamer, which has none.
[[nodiscard]] std::vector<SettingsMutation> presetMutations()
{
    return {
        {.name      = "TheTypeOfAPreset",
         .element   = "preset",
         .parent    = "nosecone",
         .edit      = Edit::ATTRIBUTE,
         .attribute = "type",
         .value     = "TRANSITION",
         .line      = R"(  <preset> type: expected "TRANSITION", got "NOSE_CONE")"},
        {.name      = "TheManufacturerOfAPreset",
         .element   = "preset",
         .parent    = "nosecone",
         .edit      = Edit::ATTRIBUTE,
         .attribute = "manufacturer",
         .value     = "Quest",
         .line      = R"(  <preset> manufacturer: expected "Quest", got "Estes")"},
        {.name      = "ThePartNumberOfAPreset",
         .element   = "preset",
         .parent    = "nosecone",
         .edit      = Edit::ATTRIBUTE,
         .attribute = "partno",
         .value     = "PNC-50, 0",
         .line      = R"(  <preset> partno: expected "PNC-50, 0", got "PNC-50YR, 72604")"},
        {.name      = "TheDigestOfAPreset",
         .element   = "preset",
         .parent    = "nosecone",
         .edit      = Edit::ATTRIBUTE,
         .attribute = "digest",
         .value     = "00",
         .line = R"(  <preset> digest: expected "00", got "18363dec5642206a109389ef48efcb32")"},
        {.name    = "APresetLess",
         .element = "preset",
         .parent  = "nosecone",
         .edit    = Edit::REMOVE,
         .line    = "  <preset>: number: expected 0, got 1"},
        {.name    = "APresetMore",
         .element = "streamer",
         .parent  = "subcomponents",
         .edit    = Edit::ADD_CHILD,
         .value   = "preset",
         .line    = "  <preset>: number: expected 1, got 0"},
    };
}

/// An example as loaded from its re-save, with its geometry.json and the re-save.
struct SettingsSubject
{
    std::string                         problem;  ///< why there is none, else ""
    LoadedDocument                      loaded;
    json                                geometry;
    std::unique_ptr<pugi::xml_document> resave;
};

/// Loads the example of the golden input @p input from its re-save with the presets @p presets
/// (as loaded: no setting is an automatic dimension).
[[nodiscard]] SettingsSubject settingsSubject(std::string_view input,
                                              Presets          presets = Presets::EXAMPLES)
{
    SettingsSubject      subject;
    const GoldenExample* example  = findGoldenExample(input);
    auto                 geometry = QtRocket::Test::loadGoldenGeometry(input);
    auto                 resave   = loadGoldenResave(input);
    if (example == nullptr || !geometry || !resave)
    {
        subject.problem = std::format("no golden data of {}", input);
        return subject;
    }
    Result<LoadedDocument> loaded =
        QtRocket::Test::loadGoldenExample(*example, ExampleSource::RESAVE, presets);
    if (!loaded)
    {
        subject.problem = loaded.error().toString();
        return subject;
    }
    subject.loaded   = std::move(*loaded);
    subject.geometry = std::move(*geometry);
    subject.resave   = std::move(*resave);
    return subject;
}

/// What the comparison of the settings of @p subject gives against @p resave, the design held
/// to have what @p presets says of the presets the file names.
[[nodiscard]] SettingsComparison settingsOf(const SettingsSubject&    subject,
                                            const pugi::xml_document& resave,
                                            SavedPresets presets = SavedPresets::LOADED)
{
    return compareSavedSettings(subject.loaded.document->getRocket(), subject.geometry, resave,
                                presets);
}

/// What is wrong with the report of the comparison of the settings of @p subject after
/// @p mutation changed a copy of its re-save; "" when the report has the lines expected.
[[nodiscard]] std::string problemOf(const SettingsMutation& mutation,
                                    const SettingsSubject&  subject)
{
    pugi::xml_document resave;
    resave.reset(*subject.resave);
    if (!apply(mutation, resave))
    {
        return std::format("{}: nothing to change\n", mutation.name);
    }
    const std::string report = settingsOf(subject, resave).report;
    if (!report.contains(std::format("\n{}", mutation.line)) ||
        std::ranges::count(report, '\n') != mutation.lines + 1)
    {
        return std::format("{}: the report is\n{}", mutation.name, report);
    }
    return "";
}

/// "<n> changes", and behind it every mutation of @p mutations that is not reported as
/// expected, with the report it got.
[[nodiscard]] std::string reported(const std::vector<SettingsMutation>& mutations,
                                   const SettingsSubject&               subject)
{
    std::string problems;
    for (const SettingsMutation& mutation : mutations)
    {
        problems += problemOf(mutation, subject);
    }
    return std::format("{} changes{}{}", mutations.size(), problems.empty() ? "" : "\n", problems);
}

/// What the comparison of the settings of a subject gives against its re-save, or against a
/// copy of it that a mutation changed: the report, and whether the elements compared are all
/// the file holds.
struct SettingsVerdict
{
    std::string report;
    bool        everyElementCompared{false};
};

[[nodiscard]] SettingsVerdict settingsVerdict(const SettingsSubject&  subject,
                                              const SettingsMutation* mutation = nullptr)
{
    pugi::xml_document resave;
    resave.reset(*subject.resave);
    if (mutation != nullptr && !apply(*mutation, resave))
    {
        return {.report = "nothing to change", .everyElementCompared = false};
    }
    const SettingsComparison comparison = settingsOf(subject, resave);
    return {.report               = comparison.report,
            .everyElementCompared = comparison.compared == savedSettingsCounts(resave)};
}

/// An element where the comparison does not look: a deployment in a body tube, which has none.
[[nodiscard]] SettingsMutation aDeployEventInABodyTube()
{
    return {.name    = "ADeployEventInABodyTube",
            .element = "bodytube",
            .parent  = "subcomponents",
            .edit    = Edit::ADD_CHILD,
            .value   = "deployevent",
            .line    = ""};
}

// One test, so that the example is loaded once: the re-save as it is matches, with every
// element of it compared; then every change is reported. An element where no component has it
// is in no report: it shows in the numbers, which is why the tests hold the elements compared
// against those of the whole file.
TEST(ExampleDesignGoldenMutations, AChangedElementOfAResaveIsReported)
{
    const SettingsSubject subject = settingsSubject(kTwoStage);
    ASSERT_EQ(subject.problem, "");

    const SettingsVerdict asItIs = settingsVerdict(subject);
    EXPECT_EQ(asItIs.report, "");
    EXPECT_TRUE(asItIs.everyElementCompared);

    EXPECT_EQ(reported(recoveryMutations(), subject), "14 changes");
    EXPECT_EQ(reported(ignitionMutations(), subject), "14 changes");

    const SettingsMutation stray      = aDeployEventInABodyTube();
    const SettingsVerdict  withAStray = settingsVerdict(subject, &stray);
    EXPECT_EQ(withAStray.report, "");
    EXPECT_FALSE(withAStray.everyElementCompared);
}

/// What the comparison reports of a component that has a preset when the design is held to
/// have been loaded without the presets.
constexpr std::string_view kAPresetWhereNoneIsExpected =
    "\n  preset of the component, loaded without presets: number: expected 0, got 1\n";
/// What it reports of a component without a preset whose element has one.
constexpr std::string_view kNoPresetWhereOneIsExpected =
    "\n  <preset>: number: expected 1, got 0\n";

// The presets, in the re-save of "Pods--powered with recovery deployment.ork": two nose cones
// and a body tube, each made from a preset of Estes. The re-save as it is matches the design
// that was loaded with the presets, every element compared; then every change to a <preset> is
// reported: its type, its manufacturer, its part number, its digest, a <preset> less and one
// more. And the comparison tells a design that holds the presets from one that was loaded
// without them: either is the design of the file only when it is held against what it was
// loaded with, and against the other each of the three components is reported.
TEST(ExampleDesignGoldenMutations, AChangedPresetOfAResaveIsReported)
{
    const SettingsSubject subject = settingsSubject(kPods);
    ASSERT_EQ(subject.problem, "");

    const SettingsComparison asItIs = settingsOf(subject, *subject.resave);
    EXPECT_EQ(asItIs.report, "");
    EXPECT_EQ(asItIs.compared.presets, 3);
    EXPECT_EQ(asItIs.compared, savedSettingsCounts(*subject.resave));

    EXPECT_EQ(reported(presetMutations(), subject), "6 changes");

    const SettingsComparison asIfWithout =
        settingsOf(subject, *subject.resave, SavedPresets::NOT_FOUND);
    EXPECT_EQ(occurrences(asIfWithout.report, kAPresetWhereNoneIsExpected), 3)
        << asIfWithout.report;
    EXPECT_EQ(std::ranges::count(asIfWithout.report, '\n'), 6) << asIfWithout.report;
    EXPECT_EQ(asIfWithout.compared.presets, 0);

    const SettingsSubject without = settingsSubject(kPods, Presets::NONE);
    ASSERT_EQ(without.problem, "");

    const SettingsComparison held = settingsOf(without, *without.resave, SavedPresets::NOT_FOUND);
    EXPECT_EQ(held.report, "");
    EXPECT_EQ(held.compared.presets, 3);
    EXPECT_EQ(held.compared, savedSettingsCounts(*without.resave));

    const SettingsComparison asIfLoaded = settingsOf(without, *without.resave);
    EXPECT_EQ(occurrences(asIfLoaded.report, kNoPresetWhereOneIsExpected), 3) << asIfLoaded.report;
    EXPECT_EQ(std::ranges::count(asIfLoaded.report, '\n'), 6) << asIfLoaded.report;
    EXPECT_EQ(asIfLoaded.compared.presets, 0);
}

}  // namespace
