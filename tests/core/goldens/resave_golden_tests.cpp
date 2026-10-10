// Golden tests of the re-saved test rockets: OpenRocket's save of each of the thirteen rockets
// of TestRockets.java (tests/data/goldens/testrocket-<name>/resave/rocket.ork, written by
// tools/openrocket-goldens: plain XML of file version 1.11), loaded by the .ork loader
// (GeneralRocketLoader) and compared with the goldens of the rocket that was saved.
//
// The rocket built in code is compared with those goldens by test_rockets_golden_tests.cpp
// (geometry.json), tests/core/mass/MassCalculatorTests.cpp (mass.json) and aero_golden_tests.cpp
// (aero.json). Here the same numbers are reached by another road: OpenRocket's saver wrote the
// Java rocket, and QtRocket's loader reads it. A setter of the loader that puts a value of one
// of these designs in the wrong place shows as a difference from the rocket of the makers.
//
// The load. The motors of the makers are synthetic (their digests are "digest A8 test" and so
// on): against the bundled motor database the loader would give the mounts real motors of the
// same designations, with other digests, masses and flights. So the motor database of these
// loads holds exactly the seven motors of TestRockets, added in the order A8, A10, B4, C6, D21,
// M1350, G77 (testRocketMotors()), and the loader finds each motor by its digest. Everything
// else is the environment of the other loaded designs (DesignFileEnvironment: OpenRocket's test
// preferences, its materials, the bundled extensions), without component presets: no test
// rocket names one. Every file is loaded in an environment of its own.
//
// Compared, for each of the thirteen files (ResaveGolden):
// - the load gives no warning;
// - the design, settled as the harness settles a design before it dumps it (see
//   AutomaticDimensions.h; no test rocket needs a pass), against geometry.json, mass.json and
//   aero.json with compareLoadedDesign() of GoldenDesign.h, as the example designs are compared:
//   every component in the order of the tree with its id (the file has the ids of the Java
//   rocket, which no rebuilt rocket has), every flight configuration with its id (those of
//   makeMultiStageEventTestRocket() and makeClusterPods() included, which the makers draw at
//   random and the comparison of the rebuilt rockets has to leave out), the masses, and the
//   aerodynamics;
// - the deployment, separation and ignition settings against the elements of the file itself
//   (compareSavedSettings() of GoldenSettings.h);
// - the simulations (ResaveGoldenSimulations): the fifty the harness gave the documents, one per
//   flight configuration and the three variants, each with its name, its flight configuration,
//   no extension, no flight data, and the options the harness flew it with, which are the
//   "options" of its golden simulation document, value for value (the re-save was written after
//   the harness had made the options reproducible). Their statuses are those of OpenRocket's
//   own reload: NOT_SIMULATED for the 36 whose configuration has a motor, CANT_RUN for the 14
//   without one.
//
// THE FOURTEEN EXCEPTIONS. OpenRocket's own reload of these files does not give back the rocket
// of the makers in fourteen values of geometry.json, in three of the designs (kReloadChanges;
// measured with OpenRocket's loader against the same seven motors and the harness's dumpers:
// the probe "resaves" of tier 9d, ResaveProbe.java and out/reload-diff.txt). They are
// properties of the pair of OpenRocket's saver and loader, not of this port, and each is pinned
// with both numbers: the test changes a copy of the golden document to what OpenRocket's reload
// gives, after checking that the committed document holds the maker's number, and the
// comparison with the changed document has to report nothing. There are three causes:
// - A fin tab without a height is not saved (FinSetSaver writes a tab only when it has a height
//   and a length). The fin sets of Big Blue and of the booster of the multi-stage event rocket
//   were given their length when the tab already stood at the front of the fin, and neither
//   FreeformFinSet.setPoints() nor the constructor of TrapezoidFinSet moves a tab; read as an
//   offset from the middle of the fin, that position is -0.05 m and +0.005 m. The loader
//   recomputes the position from the offset it has, the default 0 (FinSetPointHandler calls
//   updateTabPosition() and the <rootchord> setter is TrapezoidFinSet.setRootChord(), which
//   does too), so the reloaded tab sits in the middle: five values each (the offset and the x
//   of the four tab points).
// - The saver writes the points of a freeform fin set as getFinPoints() gives them, whose last
//   point is the last root point, computed in the frame of the body: 0.15000000000000002 for
//   the 0.15 the maker gave Big Blue's fins. The reloaded fin set is that long, and its
//   rotational inertia follows by one unit in the last place: three values, each within the
//   tolerance of the comparison either way (as are six values of mass.json that follow the
//   inertia; they are no exceptions: mass.json is compared as it is).
// - The pod body of makeEstesAlphaIIIWithPods() is a BodyTube(length, 0): the constructor
//   leaves the default wall of 2 mm on a tube of radius 0. The file has both, and every setter
//   keeps a wall within its radius, so the loaded tube has no wall: one value.
// ResaveGoldenExceptions shows that the exceptions are needed and are all there are: against
// the committed documents the comparison reports the eleven values beyond its tolerance, one
// line each, and nothing else.
//
// Tolerances (plan section 6.4), as for the rockets of the makers: geometry, mass and
// aerodynamic coefficients relative 1e-9; positions, CGs and CPs absolute 1e-9 m; ids, names,
// the numbers of the file and the options of the simulations exactly (one option is computed
// and not stored, the launch rod direction of a launch into a multi-level wind: relative
// 1e-12).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <format>
#include <limits>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <pugixml.hpp>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/AutomaticDimensions.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenDesign.h"
#include "goldens/GoldenGeometry.h"
#include "goldens/GoldenMismatches.h"
#include "goldens/GoldenSettings.h"
#include "rocket/TestRockets.h"

namespace
{

using nlohmann::json;
using QtRocket::GeneralRocketLoader;
using QtRocket::LoadedDocument;
using QtRocket::MultiLevelPinkNoiseWindModel;
using QtRocket::PinkNoiseWindModel;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::Simulation;
using QtRocket::SimulationOptions;
using QtRocket::ThrustCurveMotor;
using QtRocket::ThrustCurveMotorSetDatabase;
using QtRocket::WindModelType;
using QtRocket::Test::compareLoadedDesign;
using QtRocket::Test::compareLoadedGeometry;
using QtRocket::Test::compareSavedSettings;
using QtRocket::Test::DesignComparison;
using QtRocket::Test::DesignCounts;
using QtRocket::Test::DesignFileEnvironment;
using QtRocket::Test::DesignGoldens;
using QtRocket::Test::GoldenInput;
using QtRocket::Test::GoldenManifest;
using QtRocket::Test::loadDesignGoldens;
using QtRocket::Test::loadGoldenJson;
using QtRocket::Test::loadGoldenManifest;
using QtRocket::Test::loadGoldenResave;
using QtRocket::Test::savedSettingsCounts;
using QtRocket::Test::SettingsComparison;
using QtRocket::Test::SettingsCounts;
using QtRocket::Test::settleAutomaticDimensions;

using Presets = DesignFileEnvironment::Presets;

/// What the manifest calls the inputs that are rockets of TestRockets.java.
constexpr std::string_view kTestRocketKind = "testrocket";

// ================================================================================= the loads

/// The motor database of the loads: exactly the seven motors of TestRockets, added in this
/// order. Made once per test process; the motors are found by their digests.
[[nodiscard]] const ThrustCurveMotorSetDatabase& testRocketMotors()
{
    static const ThrustCurveMotorSetDatabase kMotors = [] {
        ThrustCurveMotorSetDatabase database;
        for (const std::shared_ptr<const ThrustCurveMotor>& motor :
             {QtRocket::Test::motorA8(), QtRocket::Test::motorA10(), QtRocket::Test::motorB4(),
              QtRocket::Test::motorC6(), QtRocket::Test::motorD21(), QtRocket::Test::motorM1350(),
              QtRocket::Test::motorG77()})
        {
            database.addMotor(motor);
        }
        return database;
    }();
    return kMotors;
}

/// A re-save loaded into a document, with the environment of the load.
struct LoadedResave
{
    /// The simulations of the document keep the preference store of the environment: it is
    /// declared first, and so destroyed last.
    std::unique_ptr<DesignFileEnvironment> environment;
    LoadedDocument                         loaded;
};

/// Loads the re-save of @p input against the seven motors of TestRockets, in an environment of
/// its own: the document as loaded, with the warnings of the load.
[[nodiscard]] Result<LoadedResave> loadResave(const GoldenInput& input)
{
    auto environment = std::make_unique<DesignFileEnvironment>(Presets::NONE, testRocketMotors());
    const GeneralRocketLoader loader(environment->context());
    Result<LoadedDocument>    loaded = loader.load(QtRocket::Test::goldensDir() / input.resave);
    if (!loaded)
    {
        return std::unexpected(loaded.error());
    }
    return LoadedResave{.environment = std::move(environment), .loaded = std::move(*loaded)};
}

/// The inputs of the manifest that are rockets of TestRockets.java, in its order; the reason
/// in @p problem when the manifest cannot be read.
[[nodiscard]] std::vector<GoldenInput> testRocketInputs(std::string& problem)
{
    std::vector<GoldenInput>     inputs;
    const Result<GoldenManifest> manifest = loadGoldenManifest();
    if (!manifest)
    {
        problem = manifest.error().toString();
        return inputs;
    }
    for (const GoldenInput& input : manifest->inputs)
    {
        if (input.kind == kTestRocketKind)
        {
            inputs.push_back(input);
        }
    }
    return inputs;
}

// ============================================================================ the exceptions

/// A value of geometry.json that OpenRocket's own reload of a re-save gives otherwise than the
/// rocket of the maker has it (see the top of this file for the three causes).
struct ReloadChange
{
    std::string_view input;     ///< the golden input
    std::string_view path;      ///< the component
    std::string_view value;     ///< the JSON pointer of the value in the entry of the component
    std::string_view field;     ///< what the report of the comparison calls the value
    double           built;     ///< the committed geometry.json: the rocket of TestRockets.java
    double           reloaded;  ///< OpenRocket's reload of its own save of that rocket
    /// Whether the two numbers are further apart than the tolerance of the comparison, so that
    /// the comparison tells them apart.
    bool beyondTolerance;
};

constexpr std::string_view kBigBlue    = "testrocket-big-blue";
constexpr std::string_view kPods       = "testrocket-estes-alpha-iii-with-pods";
constexpr std::string_view kMultiStage = "testrocket-multi-stage-event-test-rocket";

/// The fourteen, as the probe ResaveProbe.java measured them (out/reload-diff.txt: "golden" and
/// "reloaded"). Nothing else of the thirteen geometry.json and aero.json documents differs in
/// OpenRocket's reload.
constexpr std::array<ReloadChange, 14> kReloadChanges{{
    // Big Blue, "Freeform Fin Set": the tab in the middle of the fin, not at its front.
    {.input           = kBigBlue,
     .path            = "/0/1/0",
     .value           = "/details/tabOffset",
     .field           = "details.tabOffset",
     .built           = -0.049999999999999996,
     .reloaded        = 0.0,
     .beyondTolerance = true},
    {.input           = kBigBlue,
     .path            = "/0/1/0",
     .value           = "/details/tabPoints/0/0",
     .field           = "details.tabPoints[0].x",
     .built           = 0.0,
     .reloaded        = 0.05000000000000001,
     .beyondTolerance = true},
    {.input           = kBigBlue,
     .path            = "/0/1/0",
     .value           = "/details/tabPoints/1/0",
     .field           = "details.tabPoints[1].x",
     .built           = 0.0,
     .reloaded        = 0.05000000000000001,
     .beyondTolerance = true},
    {.input           = kBigBlue,
     .path            = "/0/1/0",
     .value           = "/details/tabPoints/2/0",
     .field           = "details.tabPoints[2].x",
     .built           = 0.05,
     .reloaded        = 0.1,
     .beyondTolerance = true},
    {.input           = kBigBlue,
     .path            = "/0/1/0",
     .value           = "/details/tabPoints/3/0",
     .field           = "details.tabPoints[3].x",
     .built           = 0.05,
     .reloaded        = 0.1,
     .beyondTolerance = true},
    // ... and its length, the x of the last fin point as the saver wrote it, with the inertia
    // that follows.
    {.input           = kBigBlue,
     .path            = "/0/1/0",
     .value           = "/length",
     .field           = "length",
     .built           = 0.15,
     .reloaded        = 0.15000000000000002,
     .beyondTolerance = false},
    {.input           = kBigBlue,
     .path            = "/0/1/0",
     .value           = "/rotationalInertia",
     .field           = "rotationalInertia",
     .built           = 0.000559701886215444,
     .reloaded        = 0.0005597018862154438,
     .beyondTolerance = false},
    {.input           = kBigBlue,
     .path            = "/0/1/0",
     .value           = "/rotationalUnitInertia",
     .field           = "rotationalUnitInertia",
     .built           = 0.005586716956087414,
     .reloaded        = 0.005586716956087413,
     .beyondTolerance = false},
    // The Alpha III with pods, "Pod Body": no wall on a tube without a radius.
    {.input           = kPods,
     .path            = "/0/1/4/0",
     .value           = "/details/thickness",
     .field           = "details.thickness",
     .built           = 0.002,
     .reloaded        = 0.0,
     .beyondTolerance = true},
    // The multi-stage event rocket, "Center Booster Fin Set": the tab in the middle of the
    // fin, which is shorter than the tab.
    {.input           = kMultiStage,
     .path            = "/1/0/0",
     .value           = "/details/tabOffset",
     .field           = "details.tabOffset",
     .built           = 0.005000000000000001,
     .reloaded        = 0.0,
     .beyondTolerance = true},
    {.input           = kMultiStage,
     .path            = "/1/0/0",
     .value           = "/details/tabPoints/0/0",
     .field           = "details.tabPoints[0].x",
     .built           = 0.0,
     .reloaded        = -0.005000000000000001,
     .beyondTolerance = true},
    {.input           = kMultiStage,
     .path            = "/1/0/0",
     .value           = "/details/tabPoints/1/0",
     .field           = "details.tabPoints[1].x",
     .built           = 0.0,
     .reloaded        = -0.005000000000000001,
     .beyondTolerance = true},
    {.input           = kMultiStage,
     .path            = "/1/0/0",
     .value           = "/details/tabPoints/2/0",
     .field           = "details.tabPoints[2].x",
     .built           = 0.05,
     .reloaded        = 0.045,
     .beyondTolerance = true},
    {.input           = kMultiStage,
     .path            = "/1/0/0",
     .value           = "/details/tabPoints/3/0",
     .field           = "details.tabPoints[3].x",
     .built           = 0.05,
     .reloaded        = 0.045,
     .beyondTolerance = true},
}};

/// What changeToTheReload() changed.
struct ReloadChanges
{
    int         values{0};  ///< the values replaced
    std::string problems;   ///< a value that is not in the document as kReloadChanges has it
};

/// The entry of the component at @p path in the document @p geometry, for changing it; null
/// when the document has none.
[[nodiscard]] json* componentEntry(json& geometry, std::string_view path)
{
    for (json& component : geometry.at("components"))
    {
        if (component.at("path").get<std::string>() == path)
        {
            return &component;
        }
    }
    return nullptr;
}

/// Makes of @p geometry, the committed geometry.json of the input @p input, the document that
/// OpenRocket's harness writes for its own reload of the re-save: the values of kReloadChanges.
/// Adds what it changed to @p changes; a value that the committed document does not hold as
/// the table says is a problem, and is left as it is.
void changeToTheReload(std::string_view input, json& geometry, ReloadChanges& changes)
{
    for (const ReloadChange& change : kReloadChanges)
    {
        if (change.input != input)
        {
            continue;
        }
        json* const              component = componentEntry(geometry, change.path);
        const json::json_pointer pointer{std::string{change.value}};
        if (component == nullptr || !component->contains(pointer) ||
            !component->at(pointer).is_number() ||
            component->at(pointer).get<double>() != change.built)
        {
            changes.problems += std::format("{} {} {}: the golden value is not {}\n", input,
                                            change.path, change.value, change.built);
            continue;
        }
        component->at(pointer) = change.reloaded;
        changes.values++;
    }
}

// ============================================================================ the comparison

/// A re-save loaded and compared with the goldens of its input: what the files hold, and what
/// the comparisons compared and found.
struct ResaveRun
{
    std::string        problem;            ///< why there is no comparison, else ""
    int                loadWarnings{0};    ///< the warnings of the load
    int                settlingPasses{0};  ///< the passes that settled something
    DesignCounts       golden;             ///< what geometry.json, mass.json and aero.json hold
    DesignComparison   design;             ///< the comparison with them
    SettingsCounts     saved;              ///< the settings elements of the re-save
    SettingsComparison settings;           ///< the comparison with them
};

/// Loads the re-save of @p input, settles the design and compares it with the design documents
/// @p goldens and with the settings of the file.
[[nodiscard]] ResaveRun compareResave(const GoldenInput& input, const DesignGoldens& goldens)
{
    ResaveRun run;
    run.golden                        = goldenCounts(goldens);
    const Result<LoadedResave> resave = loadResave(input);
    const auto                 file   = loadGoldenResave(input.name);
    if (!resave || !file)
    {
        run.problem = !resave ? resave.error().toString() : file.error().toString();
        return run;
    }
    Rocket& rocket             = resave->loaded.document->getRocket();
    run.loadWarnings           = static_cast<int>(resave->loaded.warnings.size());
    const Result<int> settling = settleAutomaticDimensions(rocket);
    if (!settling)
    {
        run.problem = settling.error().toString();
        return run;
    }
    run.settlingPasses = *settling;
    run.design         = compareLoadedDesign(rocket, resave->loaded.warnings, goldens,
                                             resave->environment->preferences());
    run.saved          = savedSettingsCounts(**file);
    run.settings       = compareSavedSettings(rocket, goldens.geometry, **file);
    return run;
}

/// What comparing the thirteen re-saves with the goldens gave.
struct ComparedResaves
{
    int            inputs{0};          ///< the "testrocket" inputs of the manifest
    ReloadChanges  changes;            ///< what the documents of OpenRocket's reload differ in
    int            loadWarnings{0};    ///< the warnings of the thirteen loads
    int            settlingPasses{0};  ///< the settling passes that changed something
    DesignCounts   golden;             ///< what the design documents hold
    DesignCounts   compared;           ///< what was compared
    SettingsCounts saved;              ///< the settings elements of the re-saves
    SettingsCounts settings;           ///< those that were compared
    /// By input: why there is no comparison, the mismatches, and what was not compared in full.
    /// Empty when every reloaded design is the rocket of its goldens.
    std::string problems;
};

/// Adds the comparison @p run of the input @p name to @p compared.
void addRun(ComparedResaves& compared, std::string_view name, const ResaveRun& run)
{
    compared.loadWarnings += run.loadWarnings;
    compared.settlingPasses += run.settlingPasses;
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

/// Loads the re-save of every test rocket of the manifest and compares it with the design
/// documents of its input, changed to what OpenRocket's own reload gives (kReloadChanges).
[[nodiscard]] ComparedResaves compareResaves()
{
    ComparedResaves compared;
    for (const GoldenInput& input : testRocketInputs(compared.problems))
    {
        compared.inputs++;
        Result<DesignGoldens> goldens = loadDesignGoldens(input.name);
        if (!goldens)
        {
            compared.problems += std::format("{}: {}\n", input.name, goldens.error().toString());
            continue;
        }
        changeToTheReload(input.name, goldens->geometry, compared.changes);
        addRun(compared, input.name, compareResave(input, *goldens));
    }
    return compared;
}

/// What the design documents of the thirteen test rockets hold together (the Python survey of
/// the probes of tier 9d, part "resaves": out/survey.txt). The 151 components and the 47
/// configurations are those TestRocketsGoldenCoverage pins for the rockets of the makers, and
/// the aerodynamics those of AeroGoldenCoverage.
[[nodiscard]] DesignCounts designCountsOfTheTestRockets()
{
    return DesignCounts{.components         = 151,
                        .ids                = 151,
                        .details            = 2323,
                        .configurations     = 47,
                        .motors             = 48,
                        .instanceEntries    = 571,
                        .instances          = 831,
                        .loadWarnings       = 0,
                        .massConfigurations = 47,
                        .aero               = {.configurations = 47,
                                               .points         = 1175,
                                               .components     = 9400,
                                               .worstCPs       = 329,
                                               .warnings       = 294}};
}

/// The settings elements of the thirteen re-saves, as TestRocketsGoldenSettings pins them for
/// the rockets of the makers.
[[nodiscard]] SettingsCounts settingsCountsOfTheTestRockets()
{
    return SettingsCounts{.deployEvents             = 9,
                          .deploymentConfigurations = 0,
                          .separationEvents         = 8,
                          .separationConfigurations = 1,
                          .motorMounts              = 16,
                          .motors                   = 48,
                          .ignitionConfigurations   = 48};
}

// The thirteen re-saves load without a warning, and each reloaded design is the rocket of its
// goldens but for the fourteen values that OpenRocket's own reload changes: 151 components with
// the ids of the files, 47 configurations with theirs, 47 mass calculations, 1175 points of the
// aerodynamics, and the settings elements of the files. Everything the documents hold is
// compared for every input (its line says so when not).
TEST(ResaveGolden, TheReloadedTestRocketsAreTheRocketsOfTheGoldens)
{
    const ComparedResaves compared = compareResaves();
    EXPECT_EQ(compared.problems, "");
    EXPECT_EQ(compared.inputs, 13);
    EXPECT_EQ(compared.loadWarnings, 0);
    EXPECT_EQ(compared.settlingPasses, 0) << "no test rocket has a stale automatic dimension";
    EXPECT_EQ(compared.changes.problems, "");
    EXPECT_EQ(compared.changes.values, 14);
    EXPECT_EQ(compared.compared, compared.golden) << "what was compared, and what the files hold";
    EXPECT_EQ(compared.golden, designCountsOfTheTestRockets());
    EXPECT_EQ(compared.settings, compared.saved) << "the settings compared, and those of the files";
    EXPECT_EQ(compared.saved, settingsCountsOfTheTestRockets());
}

// =============================================================================== simulations

/// A number as the golden documents hold it: a JSON number, or the name of a value JSON has
/// none for (Json.java of the harness).
[[nodiscard]] json goldenForm(double value)
{
    if (std::isnan(value))
    {
        return "NaN";
    }
    if (std::isinf(value))
    {
        return value > 0 ? "Infinity" : "-Infinity";
    }
    return value;
}

/// The average wind model @p wind as SimulationDumper.options() writes it.
[[nodiscard]] json averageWindDocument(const PinkNoiseWindModel& wind)
{
    return json{{"average", goldenForm(wind.getAverage())},
                {"standardDeviation", goldenForm(wind.getStandardDeviation())},
                {"turbulenceIntensity", goldenForm(wind.getTurbulenceIntensity())},
                {"direction", goldenForm(wind.getDirection())}};
}

/// The multi-level wind model @p wind as SimulationDumper.options() writes it.
[[nodiscard]] json multiLevelWindDocument(const MultiLevelPinkNoiseWindModel& wind)
{
    json levels = json::array();
    for (const MultiLevelPinkNoiseWindModel::LevelWindModel* level : wind.getLevels())
    {
        levels.push_back(json{{"altitude", goldenForm(level->getAltitude())},
                              {"speed", goldenForm(level->getSpeed())},
                              {"direction", goldenForm(level->getDirection())},
                              {"standardDeviation", goldenForm(level->getStandardDeviation())}});
    }
    const bool msl =
        wind.getAltitudeReference() == MultiLevelPinkNoiseWindModel::AltitudeReference::MSL;
    return json{{"altitudeReference", msl ? "MSL" : "AGL"}, {"levels", std::move(levels)}};
}

/// The options @p options as the golden harness writes the options of a simulation
/// (SimulationDumper.options(): the getters, the enums by their names).
[[nodiscard]] json optionsDocument(const SimulationOptions& options)
{
    return json{
        {"launchRodLength", goldenForm(options.getLaunchRodLength())},
        {"launchIntoWind", options.getLaunchIntoWind()},
        {"launchRodAngle", goldenForm(options.getLaunchRodAngle())},
        {"launchRodDirection", goldenForm(options.getLaunchRodDirection())},
        {"windModelType", windModelTypeName(options.getWindModelType())},
        {"averageWind", averageWindDocument(options.getAverageWindModel())},
        {"multiLevelWind", multiLevelWindDocument(options.getMultiLevelWindModel())},
        {"launchAltitude", goldenForm(options.getLaunchAltitude())},
        {"launchLatitude", goldenForm(options.getLaunchLatitude())},
        {"launchLongitude", goldenForm(options.getLaunchLongitude())},
        {"geodeticComputation", name(options.getGeodeticComputation())},
        {"isaAtmosphere", options.isIsaAtmosphere()},
        {"launchTemperature", goldenForm(options.getLaunchTemperature())},
        {"launchPressure", goldenForm(options.getLaunchPressure())},
        {"launchRelativeHumidity", goldenForm(options.getLaunchRelativeHumidity())},
        {"timeStep", goldenForm(options.getTimeStep())},
        {"maxSimulationTime", goldenForm(options.getMaxSimulationTime())},
        {"maximumStepAngle", goldenForm(options.getMaximumStepAngle())},
        {"randomSeed", options.getRandomSeed()},
        {"randomSeedFixed", options.isRandomSeedFixed()},
        {"gravityModelType", gravityModelTypeName(options.getGravityModelType())},
        {"constantGravity", goldenForm(options.getConstantGravity())},
        {"stepperMethod", simulationStepperMethodName(options.getSimulationStepperMethodChoice())},
        {"recoverySpeedWarning", goldenForm(options.getRecoverySpeedWarning())},
        {"drogueLowSpeedWarning", goldenForm(options.getDrogueLowSpeedWarning())},
        {"recoveryDrogueMainHighSpeedWarning",
         goldenForm(options.getRecoveryDrogueMainHighSpeedWarning())},
        {"recoveryDrogueMainLowSpeedWarning",
         goldenForm(options.getRecoveryDrogueMainLowSpeedWarning())},
        {"hasDragLookup", options.hasDragLookup()},
        {"hasStabilityLookup", options.hasStabilityLookup()},
    };
}

/// The relative tolerance of the one option that is computed with mathematical functions and
/// not stored: the launch rod direction of a launch into a multi-level wind, the direction of
/// the model's wind at the launch site (an arc tangent of a sine and a cosine).
constexpr double kComputedDirectionRelative = 1e-12;

/// The value of the golden options whose JSON pointer is this one.
constexpr std::string_view kLaunchRodDirection = "/launchRodDirection";

/// Whether the option @p actual is the golden @p expected: the same value, or, for a number
/// that is @p computed, one within kComputedDirectionRelative of it.
[[nodiscard]] bool isTheOption(const json& expected, const json& actual, bool computed)
{
    if (computed && expected.is_number() && actual.is_number())
    {
        const double golden = expected.get<double>();
        return std::abs(actual.get<double>() - golden) <=
               kComputedDirectionRelative * std::abs(golden);
    }
    return expected == actual;
}

/// What the comparison of the options of a simulation compared and found.
struct OptionsComparison
{
    int         values{0};  ///< the values of the golden options
    std::string report;     ///< the mismatches, one line each
};

/// Compares @p options with the golden "options" @p expected of a simulation document, value
/// for value: every value of the golden object, however deep, has to be there and equal (they
/// are settings: the numbers exactly), and @p options must give no value beyond them.
[[nodiscard]] OptionsComparison compareOptions(const json&              expected,
                                               const SimulationOptions& options)
{
    OptionsComparison result;
    const json        golden = expected.flatten();
    const json        actual = optionsDocument(options).flatten();
    const bool        computedDirection =
        options.getLaunchIntoWind() && options.getWindModelType() == WindModelType::MULTI_LEVEL;
    for (const auto& [pointer, value] : golden.items())
    {
        result.values++;
        if (!actual.contains(pointer))
        {
            result.report += std::format("  options{}: not compared\n", pointer);
        }
        else if (!isTheOption(value, actual.at(pointer),
                              computedDirection && pointer == kLaunchRodDirection))
        {
            result.report += std::format("  options{}: expected {}, got {}\n", pointer,
                                         value.dump(), actual.at(pointer).dump());
        }
    }
    for (const auto& [pointer, value] : actual.items())
    {
        if (!golden.contains(pointer))
        {
            result.report += std::format("  options{}: not in the golden options\n", pointer);
        }
    }
    return result;
}

/// What the simulations of the re-saves are, and what comparing them with the golden simulation
/// documents gave.
struct ComparedSimulations
{
    int inputs{0};
    int simulations{0};    ///< the simulations of the loaded documents
    int notSimulated{0};   ///< those whose status is NOT_SIMULATED
    int cannotRun{0};      ///< those whose status is CANT_RUN
    int withoutMotors{0};  ///< those whose flight configuration has no motor, by geometry.json
    int optionValues{0};   ///< the values of the golden options that were compared
    /// By simulation: what differs from its golden document. Empty when every simulation is
    /// the one the harness saved.
    std::string problems;
};

/// The status OpenRocket's reload gives a simulation of a re-save: none has flight data, so it
/// cannot be run when its flight configuration has no motor, and is not simulated yet otherwise.
[[nodiscard]] Simulation::Status statusAfterTheLoad(bool hasMotors)
{
    return hasMotors ? Simulation::Status::NOT_SIMULATED : Simulation::Status::CANT_RUN;
}

/// Whether the flight configuration the golden simulation document @p golden names (by its
/// index among the configurations of @p geometry, with its id) has a motor; @p problem says
/// why when the two documents do not agree on the configuration.
[[nodiscard]] bool goldenConfigurationHasMotors(const json& golden, const json& geometry,
                                                std::string& problem)
{
    const json&       named          = golden.at("flightConfiguration");
    const json&       configurations = geometry.at("configurations");
    const std::size_t index          = named.at("index").get<std::size_t>();
    if (index >= configurations.size() || configurations.at(index).at("id") != named.at("id"))
    {
        problem += "  flightConfiguration: not a configuration of geometry.json\n";
        return false;
    }
    return configurations.at(index).at("hasMotors").get<bool>();
}

/// Compares the loaded @p simulation with its golden document @p golden: the name, the flight
/// configuration, the status after the load, no extension and no flight data, and the options.
/// Adds what it found to @p compared, under @p what.
void compareSimulation(ComparedSimulations& compared, const std::string& what, const json& golden,
                       const json& geometry, Simulation& simulation)
{
    std::string       report;
    const bool        hasMotors = goldenConfigurationHasMotors(golden, geometry, report);
    const auto        status    = simulation.getStatus();
    const std::string id        = simulation.getFlightConfigurationId().toString();
    compared.simulations++;
    compared.withoutMotors += hasMotors ? 0 : 1;
    compared.notSimulated += status == Simulation::Status::NOT_SIMULATED ? 1 : 0;
    compared.cannotRun += status == Simulation::Status::CANT_RUN ? 1 : 0;
    if (simulation.getName() != golden.at("name").get<std::string>())
    {
        report += std::format("  name: expected {}, got \"{}\"\n", golden.at("name").dump(),
                              simulation.getName());
    }
    if (id != golden.at("flightConfiguration").at("id").get<std::string>())
    {
        report += std::format("  flightConfiguration.id: expected {}, got \"{}\"\n",
                              golden.at("flightConfiguration").at("id").dump(), id);
    }
    if (status != statusAfterTheLoad(hasMotors))
    {
        report += std::format("  status: expected {}, got {}\n",
                              name(statusAfterTheLoad(hasMotors)), name(status));
    }
    if (simulation.getSimulationExtensions().size() != golden.at("extensions").size() ||
        simulation.hasSimulationData())
    {
        report += "  extensions or flight data: the file has neither\n";
    }
    const OptionsComparison options = compareOptions(golden.at("options"), simulation.getOptions());
    compared.optionValues += options.values;
    report += options.report;
    if (!report.empty())
    {
        compared.problems += std::format("{}:\n{}", what, report);
    }
}

/// Compares the simulations of the document of @p resave, as loaded, with the simulation
/// documents the manifest lists for @p input, in their order (that of the harness's document).
void compareSimulations(ComparedSimulations& compared, const GoldenInput& input,
                        const LoadedResave& resave, const json& geometry)
{
    const std::vector<std::shared_ptr<Simulation>> simulations =
        resave.loaded.document->getSimulations();
    if (simulations.size() != input.simulations.size())
    {
        compared.problems += std::format("{}: {} simulations, the manifest lists {}\n", input.name,
                                         simulations.size(), input.simulations.size());
        return;
    }
    for (std::size_t i = 0; i < simulations.size(); i++)
    {
        const std::string  what   = std::format("{} simulation {}", input.name, i);
        const Result<json> golden = loadGoldenJson(input.simulations.at(i).json);
        if (!golden)
        {
            compared.problems += std::format("{}: {}\n", what, golden.error().toString());
            continue;
        }
        compareSimulation(compared, what, *golden, geometry, *simulations.at(i));
    }
}

/// Loads the re-save of every test rocket and compares its simulations, as loaded, with the
/// golden simulation documents of its input.
[[nodiscard]] ComparedSimulations compareSimulationsOfTheResaves()
{
    ComparedSimulations compared;
    for (const GoldenInput& input : testRocketInputs(compared.problems))
    {
        compared.inputs++;
        const Result<LoadedResave> resave   = loadResave(input);
        const Result<const json*>  geometry = QtRocket::Test::goldenGeometry(input.name);
        if (!resave || !geometry)
        {
            compared.problems +=
                std::format("{}: {}\n", input.name,
                            !resave ? resave.error().toString() : geometry.error().toString());
            continue;
        }
        compareSimulations(compared, input, *resave, **geometry);
    }
    return compared;
}

// The documents of the thirteen re-saves hold the fifty simulations the harness flew: each
// with the name, the flight configuration and the options of its golden simulation document
// (1808 values: 32 of every simulation and 4 of each of its wind levels, one level but for the
// three of the flight into a multi-level wind; among the fifty are the RK6 stepper, the WGS84
// geodetic computation and that wind). OpenRocket's own reload leaves 36 of them NOT_SIMULATED
// and the 14 of a configuration without motors CANT_RUN, and so does this one.
TEST(ResaveGoldenSimulations, TheSimulationsAreThoseTheHarnessSaved)
{
    const ComparedSimulations compared = compareSimulationsOfTheResaves();
    EXPECT_EQ(compared.problems, "");
    EXPECT_EQ(compared.inputs, 13);
    EXPECT_EQ(compared.simulations, 50);
    EXPECT_EQ(compared.notSimulated, 36);
    EXPECT_EQ(compared.cannotRun, 14);
    EXPECT_EQ(compared.withoutMotors, 14);
    EXPECT_EQ(compared.optionValues, 1808);
}

// The comparison of the options is not vacuous: a value of the golden options changed in a copy
// is reported, in the line that names it, and so is an option that only one side has.

/// The golden input whose simulations the changes are made to: the Alpha III has the three
/// variants among its nine simulations (6: the RK6 stepper, 7: the WGS84 geodetic computation,
/// 8: the multi-level wind).
constexpr std::string_view kAlphaIII = "testrocket-estes-alpha-iii";

/// A change to the golden options of a simulation of the Alpha III that the comparison of the
/// options has to report (or, with no line, has to let pass).
struct OptionsMutation
{
    std::string_view name;        ///< what is changed
    std::size_t      simulation;  ///< the index of the simulation
    json             change;      ///< the operation of a JSON patch (RFC 6902) on its "options"
    std::string_view line;        ///< what the first line of the report starts with
    int              lines;       ///< the number of lines of the report
};

/// What GoogleTest prints for the mutation of a test that failed: its name.
// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const OptionsMutation& mutation, std::ostream* out)
{
    *out << mutation.name;
}

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

/// A wind level of the golden options.
[[nodiscard]] json aWindLevel()
{
    return json{
        {"altitude", 100.0}, {"speed", 4.0}, {"direction", 2.2}, {"standardDeviation", 0.0}};
}

/// The changes. The launch rod direction is pi / 2 in every simulation of the Alpha III; the
/// number next to it is one unit in the last place away, 1.5707963268 three parts in 10^11.
[[nodiscard]] std::vector<OptionsMutation> optionsMutations()
{
    return {
        {.name       = "TheTimeStep",
         .simulation = 4,
         .change     = replaced("/timeStep", 0.06),
         .line       = "  options/timeStep: expected 0.06, got 0.05",
         .lines      = 1},
        {.name       = "TheRandomSeed",
         .simulation = 4,
         .change     = replaced("/randomSeed", 1),
         .line       = "  options/randomSeed: expected 1, got 0",
         .lines      = 1},
        {.name       = "AFlag",
         .simulation = 4,
         .change     = replaced("/launchIntoWind", false),
         .line       = "  options/launchIntoWind: expected false, got true",
         .lines      = 1},
        {.name       = "TheStepperMethod",
         .simulation = 6,
         .change     = replaced("/stepperMethod", "RK4"),
         .line       = R"(  options/stepperMethod: expected "RK4", got "RK6")",
         .lines      = 1},
        {.name       = "TheGeodeticComputation",
         .simulation = 7,
         .change     = replaced("/geodeticComputation", "SPHERICAL"),
         .line       = R"(  options/geodeticComputation: expected "SPHERICAL", got "WGS84")",
         .lines      = 1},
        {.name       = "TheWindModelType",
         .simulation = 8,
         .change     = replaced("/windModelType", "AVERAGE"),
         .line       = R"(  options/windModelType: expected "AVERAGE", got "MULTI_LEVEL")",
         .lines      = 1},
        {.name       = "TheSpeedOfAWindLevel",
         .simulation = 8,
         .change     = replaced("/multiLevelWind/levels/1/speed", 5.0),
         .line       = "  options/multiLevelWind/levels/1/speed: expected 5.0, got 4.0",
         .lines      = 1},
        {.name       = "TheAltitudeReference",
         .simulation = 8,
         .change     = replaced("/multiLevelWind/altitudeReference", "AGL"),
         .line       = R"(  options/multiLevelWind/altitudeReference: expected "AGL", got "MSL")",
         .lines      = 1},
        {.name       = "AWindLevelMore",
         .simulation = 4,
         .change     = added("/multiLevelWind/levels/-", aWindLevel()),
         .line       = "  options/multiLevelWind/levels/1/altitude: not compared",
         .lines      = 4},
        {.name       = "AWindLevelLess",
         .simulation = 8,
         .change     = removed("/multiLevelWind/levels/2"),
         .line       = "  options/multiLevelWind/levels/2/altitude: not in the golden options",
         .lines      = 4},
        {.name       = "AnOptionNothingCompares",
         .simulation = 0,
         .change     = added("/launchRailLength", 1.0),
         .line       = "  options/launchRailLength: not compared",
         .lines      = 1},
        {.name       = "AnOptionLess",
         .simulation = 0,
         .change     = removed("/randomSeedFixed"),
         .line       = "  options/randomSeedFixed: not in the golden options",
         .lines      = 1},
        // A stored number is compared exactly ...
        {.name       = "AStoredDirectionByOneUnitInTheLastPlace",
         .simulation = 4,
         .change     = replaced("/launchRodDirection", 1.5707963267948968),
         .line =
             "  options/launchRodDirection: expected 1.5707963267948968, got 1.5707963267948966",
         .lines = 1},
        // ... and the computed one, the direction into a multi-level wind, within its tolerance.
        {.name       = "TheComputedDirectionByOneUnitInTheLastPlace",
         .simulation = 8,
         .change     = replaced("/launchRodDirection", 1.5707963267948968),
         .line       = "",
         .lines      = 0},
        {.name       = "TheComputedDirectionBeyondItsTolerance",
         .simulation = 8,
         .change     = replaced("/launchRodDirection", 1.5707963268),
         .line       = "  options/launchRodDirection: expected 1.5707963268, got 1.57079632679",
         .lines      = 1},
    };
}

/// The report of comparing the options of the simulation of index @p simulation of the reloaded
/// Alpha III with its golden options after @p change; a line that starts with "no " when there
/// is nothing to compare.
[[nodiscard]] std::string optionsReportAfter(std::size_t simulation, const json& change)
{
    const Result<GoldenManifest> manifest = loadGoldenManifest();
    const GoldenInput* const     input    = manifest ? manifest->find(kAlphaIII) : nullptr;
    if (input == nullptr || simulation >= input->simulations.size())
    {
        return "no such golden simulation";
    }
    const Result<json>         golden = loadGoldenJson(input->simulations.at(simulation).json);
    const Result<LoadedResave> resave = loadResave(*input);
    if (!golden || !resave || simulation >= resave->loaded.document->getSimulationCount())
    {
        return "no golden document or no such simulation in the re-save";
    }
    const json options = golden->at("options").patch(json::array({change}));
    return compareOptions(options, resave->loaded.document->getSimulation(simulation)->getOptions())
        .report;
}

/// One changed golden option.
class ResaveGoldenOptionsMutation : public ::testing::TestWithParam<OptionsMutation>
{ };

TEST_P(ResaveGoldenOptionsMutation, IsReportedInItsLine)
{
    const OptionsMutation& mutation = GetParam();
    const std::string      report   = optionsReportAfter(mutation.simulation, mutation.change);
    EXPECT_TRUE(report.starts_with(mutation.line)) << report;
    EXPECT_EQ(std::ranges::count(report, '\n'), mutation.lines) << report;
}

/// The test name of @p info's mutation: what it changes.
[[nodiscard]] std::string optionsMutationTestName(
    const ::testing::TestParamInfo<OptionsMutation>& info)
{
    return std::string{info.param.name};
}

INSTANTIATE_TEST_SUITE_P(Changes, ResaveGoldenOptionsMutation,
                         ::testing::ValuesIn(optionsMutations()), optionsMutationTestName);

// ===================================================================== the exceptions, needed

/// The start of the line with which the comparison of geometry.json reports @p change when the
/// document is the committed one: the name of the value and the number of the maker's rocket,
/// up to the number found.
[[nodiscard]] std::string reportedLine(const ReloadChange& change)
{
    return std::format("\n  {}: expected {}, got ", change.field, change.built);
}

/// What is wrong with @p report, the comparison of the reloaded design of @p input with its
/// committed geometry.json: it has to hold one line for each value of kReloadChanges of the
/// input that is beyond the tolerance, under the one line that names their component (the
/// values of a design are all of one component), and nothing else. Adds the number of those
/// values to @p reported.
[[nodiscard]] std::string reportProblems(std::string_view input, const std::string& report,
                                         int& reported)
{
    std::string problems;
    int         expected = 0;
    for (const ReloadChange& change : kReloadChanges)
    {
        if (change.input != input || !change.beyondTolerance)
        {
            continue;
        }
        expected++;
        if (!report.contains(reportedLine(change)))
        {
            problems += std::format("{} {} {}: not reported\n", input, change.path, change.field);
        }
    }
    reported += expected;
    if (std::ranges::count(report, '\n') != (expected == 0 ? 0 : expected + 1))
    {
        problems += std::format("{}: the report is not that of {} values of one component:\n{}",
                                input, expected, report);
    }
    return problems;
}

/// What comparing the reloaded designs with the committed geometry.json documents, as they
/// are, gave.
struct WithoutTheChanges
{
    int         inputs{0};
    int         reported{0};  ///< the values of kReloadChanges the comparison has to report
    std::string problems;     ///< a value that is not reported, a line beyond the values
};

/// Compares the reloaded and settled design of every test rocket with its committed
/// geometry.json and checks the report against kReloadChanges (reportProblems()).
[[nodiscard]] WithoutTheChanges compareWithTheCommittedGeometry()
{
    WithoutTheChanges result;
    for (const GoldenInput& input : testRocketInputs(result.problems))
    {
        result.inputs++;
        const Result<const json*>  geometry = QtRocket::Test::goldenGeometry(input.name);
        const Result<LoadedResave> resave   = loadResave(input);
        if (!geometry || !resave)
        {
            result.problems += std::format("{}: no geometry.json or no re-save\n", input.name);
            continue;
        }
        Rocket& rocket = resave->loaded.document->getRocket();
        if (!settleAutomaticDimensions(rocket).has_value())
        {
            result.problems += std::format("{}: does not settle\n", input.name);
            continue;
        }
        const DesignComparison compared = compareLoadedGeometry(
            rocket, resave->loaded.warnings, **geometry, resave->environment->preferences());
        result.problems += reportProblems(input.name, compared.report, result.reported);
    }
    return result;
}

// The exceptions are needed, and they are all there are: against the committed geometry.json
// documents the reloaded designs differ in the eleven values of the table that are beyond the
// tolerance of the comparison, each in its line with the number of the maker's rocket, and in
// nothing else (ten of the thirteen designs in nothing at all).
TEST(ResaveGoldenExceptions, TheCommittedGeometryDiffersInTheElevenValuesBeyondTheTolerance)
{
    const WithoutTheChanges result = compareWithTheCommittedGeometry();
    EXPECT_EQ(result.problems, "");
    EXPECT_EQ(result.inputs, 13);
    EXPECT_EQ(result.reported, 11);
}

/// What is wrong with kReloadChanges as a table: a value whose two numbers are the same, or
/// whose beyondTolerance is not what the relative tolerance of the comparison says of them.
/// Counts the values by design in @p bigBlue, @p pods and @p multiStage.
[[nodiscard]] std::string tableProblems(int& bigBlue, int& pods, int& multiStage)
{
    std::string problems;
    for (const ReloadChange& change : kReloadChanges)
    {
        bigBlue += change.input == kBigBlue ? 1 : 0;
        pods += change.input == kPods ? 1 : 0;
        multiStage += change.input == kMultiStage ? 1 : 0;
        const double scale = std::max(std::abs(change.built), std::abs(change.reloaded));
        const bool   beyond =
            std::abs(change.built - change.reloaded) > QtRocket::Test::kGoldenRelative * scale;
        if (change.built == change.reloaded || beyond != change.beyondTolerance)
        {
            problems += std::format("{} {} {}\n", change.input, change.path, change.value);
        }
    }
    return problems;
}

// The table: eight values of Big Blue's fins, one of the pod body, five of the booster's fins;
// the three that are within the tolerance are the length of Big Blue's fins and the two
// inertias that follow it, one unit in the last place each.
TEST(ResaveGoldenExceptions, TheTableHoldsFourteenValuesOfThreeDesigns)
{
    int bigBlue    = 0;
    int pods       = 0;
    int multiStage = 0;
    EXPECT_EQ(tableProblems(bigBlue, pods, multiStage), "");
    EXPECT_EQ(bigBlue, 8);
    EXPECT_EQ(pods, 1);
    EXPECT_EQ(multiStage, 5);
    EXPECT_EQ(kReloadChanges.size(), 14U);
}

/// The length of the component at @p path of the reloaded design of the input @p name; NaN
/// when there is no such design or component.
[[nodiscard]] double reloadedLength(std::string_view name, std::string_view path)
{
    const Result<GoldenManifest> manifest = loadGoldenManifest();
    const GoldenInput* const     input    = manifest ? manifest->find(name) : nullptr;
    if (input == nullptr)
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const Result<LoadedResave> resave = loadResave(*input);
    if (!resave)
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const QtRocket::RocketComponent* const component =
        QtRocket::Test::componentAtGoldenPath(resave->loaded.document->getRocket(), path);
    return component != nullptr ? component->getLength() : std::numeric_limits<double>::quiet_NaN();
}

// One of the three values within the tolerance can be told apart all the same: the length of
// Big Blue's fins is the x of their last point as the file has it, a number that was read and
// not computed, and it is the saver's 0.15000000000000002, not the maker's 0.15.
TEST(ResaveGoldenExceptions, TheFinsOfBigBlueAreAsLongAsTheFileSays)
{
    EXPECT_EQ(reloadedLength(kBigBlue, "/0/1/0"), 0.15000000000000002);
    EXPECT_NE(0.15000000000000002, 0.15);
}

// ============================================================== the ids of the configurations

/// An id that no flight configuration has.
constexpr const char* kAnotherId = "00000000-0000-4000-8000-000000000000";

/// What comparing the configuration ids of the designs whose makers draw one at random gave.
struct RandomIds
{
    int makers{0};  ///< the makers that draw the id of a configuration at random
    /// A design that is not that of its geometry.json, and a changed id that the comparison
    /// does not report.
    std::string problems;
};

/// The report of the comparison of the reloaded and settled design of @p input with @p geometry.
[[nodiscard]] std::string geometryReport(const GoldenInput& input, const json& geometry)
{
    const Result<LoadedResave> resave = loadResave(input);
    if (!resave)
    {
        return resave.error().toString();
    }
    Rocket& rocket = resave->loaded.document->getRocket();
    if (!settleAutomaticDimensions(rocket).has_value())
    {
        return "the design does not settle";
    }
    return compareLoadedGeometry(rocket, resave->loaded.warnings, geometry,
                                 resave->environment->preferences())
        .report;
}

/// For every maker that draws the id of its flight configuration at random: the reloaded design
/// has the ids of geometry.json (the report is empty), and it is the ids that are compared (the
/// id of the last configuration replaced by another one is reported).
[[nodiscard]] RandomIds compareTheRandomIds()
{
    RandomIds                    result;
    const Result<GoldenManifest> manifest = loadGoldenManifest();
    for (const QtRocket::Test::TestRocketMaker& maker : QtRocket::Test::testRocketMakers())
    {
        if (!maker.randomConfigurationId)
        {
            continue;
        }
        result.makers++;
        const GoldenInput* const input   = manifest ? manifest->find(maker.input) : nullptr;
        Result<DesignGoldens>    goldens = loadDesignGoldens(maker.input);
        if (input == nullptr || !goldens)
        {
            result.problems += std::format("{}: no goldens\n", maker.input);
            continue;
        }
        ReloadChanges changes;
        changeToTheReload(maker.input, goldens->geometry, changes);
        result.problems += geometryReport(*input, goldens->geometry);

        goldens->geometry.at("configurations").back().at("id") = kAnotherId;
        const std::string report = geometryReport(*input, goldens->geometry);
        if (!report.contains(std::format(R"(id: expected "{}", got ")", kAnotherId)))
        {
            result.problems += std::format("{}: another configuration id is not reported:\n{}",
                                           maker.input, report);
        }
    }
    return result;
}

// makeMultiStageEventTestRocket() and makeClusterPods() give their flight configuration a new
// random id, which the comparison of the rebuilt rockets leaves out. The file has the id
// OpenRocket drew, the golden documents have it too, and here it is compared.
TEST(ResaveGoldenConfigurationIds, TheIdsTheMakersDrawAtRandomAreThoseOfTheFiles)
{
    const RandomIds result = compareTheRandomIds();
    EXPECT_EQ(result.problems, "");
    EXPECT_EQ(result.makers, 2);
}

}  // namespace
