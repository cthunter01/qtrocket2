#pragma once

// The re-saves of the golden harness (tests/data/goldens/<input>/resave/rocket.ork: OpenRocket's
// own save of each input, plain XML of file version 1.11) as the golden tests use them. Test-only.
//
// - The loads of the thirteen re-saved test rockets against the seven motors of TestRockets
//   (testRocketMotors(), loadResave(), testRocketInputs(), and loadTestRocketBytes() for a save
//   of one), the values that OpenRocket's own reload of them changes (kReloadChanges), the
//   comparison of such a loaded design with its goldens and with the settings of its re-save
//   (compareLoadedResave()) and of its simulations with the golden simulation documents
//   (compareSimulations(), with compareOptions() for a simulation's options):
//   resave_golden_tests.cpp, whose top comment says what each is, ork_resave_diff_golden_tests.cpp
//   and ork_roundtrip_golden_tests.cpp (the same comparisons after a round trip through an
//   archive).
// - The comparison of a QtRocket save with a re-save, text against text, line by line
//   (compareOrkTexts(), with rocketElementOf()): ork_resave_diff_golden_tests.cpp. The only
//   normalisation is the creator attribute of the root element (QtRocket writes its own); a line
//   that differs only in numbers that read as the same double on both sides is counted apart
//   (the user's decision U10 of 2026-10-08: JDK 17's longer forms of a double are the same
//   number), and every other difference is reported, a carriage return or a changed order of
//   elements included.
// - The re-saves the manifest lists under "resaves" (tools/openrocket-goldens/README.md): those of
//   the design files of tests/data/ork (the legacy files, kLegacyKind) and of the saver-coverage
//   documents the harness builds (kCoverageKind), and the re-saves WITH simulation data of two
//   examples (loadManifestResaves()); with what their comparisons need: the ids a loader drew
//   numbered (withDrawnIdsNumbered()), the lines added and removed between two texts
//   (lineChanges(), and placedLineChanges() with the place of each) and the entries that differ
//   between two archives (archiveChanges()):
//   ork_resave_diff_golden_tests.cpp and tests/core/file/openrocket/legacy_ork_roundtrip_tests.cpp.

#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>
#include <pugixml.hpp>

#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/Error.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenDesign.h"
#include "goldens/GoldenSettings.h"

namespace QtRocket::Test
{

// ================================================================================= the loads

/// What the manifest calls the inputs that are rockets of TestRockets.java.
inline constexpr std::string_view kTestRocketKind = "testrocket";

/// The motor database of the loads: exactly the seven motors of TestRockets, added in this
/// order. Made once per test process; the motors are found by their digests.
[[nodiscard]] const ThrustCurveMotorSetDatabase& testRocketMotors();

/// A re-save loaded into a document, with the environment of the load.
struct LoadedResave
{
    /// The simulations of the document keep the preference store of the environment: it is
    /// declared first, and so destroyed last.
    std::unique_ptr<DesignFileEnvironment> environment;
    LoadedDocument                         loaded;
};

/// What a test does to the preference store of a load before the file is read.
using PreparePreferences = void (*)(Preferences& preferences);

/// Loads the re-save of @p input against the seven motors of TestRockets, in an environment of
/// its own: the document as loaded, with the warnings of the load. @p prepare, when given,
/// changes the preference store of the environment first (OpenRocket's test preferences).
[[nodiscard]] Result<LoadedResave> loadResave(const GoldenInput& input,
                                              PreparePreferences prepare = nullptr);

/// Loads the design @p bytes hold, the contents of a design file (QtRocket's archive of a loaded
/// re-save, say), from memory and without a directory (GeneralRocketLoader::load(bytes)), as
/// loadResave() loads a re-save: against the seven motors of TestRockets, in an environment of
/// its own, with @p prepare.
[[nodiscard]] Result<LoadedResave> loadTestRocketBytes(std::vector<std::byte> bytes,
                                                       PreparePreferences     prepare = nullptr);

/// The inputs of the manifest that are rockets of TestRockets.java, in its order; the reason
/// in @p problem when the manifest cannot be read.
[[nodiscard]] std::vector<GoldenInput> testRocketInputs(std::string& problem);

// ============================================================================ the exceptions

/// A value of geometry.json that OpenRocket's own reload of a re-save gives otherwise than the
/// rocket of the maker has it (see resave_golden_tests.cpp for the three causes).
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

inline constexpr std::string_view kBigBlue    = "testrocket-big-blue";
inline constexpr std::string_view kPods       = "testrocket-estes-alpha-iii-with-pods";
inline constexpr std::string_view kMultiStage = "testrocket-multi-stage-event-test-rocket";

/// The fourteen, as the probe ResaveProbe.java measured them (out/reload-diff.txt: "golden" and
/// "reloaded"). Nothing else of the thirteen geometry.json and aero.json documents differs in
/// OpenRocket's reload.
inline constexpr std::array<ReloadChange, 14> kReloadChanges{{
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

// ==================================================== the comparison of a loaded test rocket

/// What changeToTheReload() changed.
struct ReloadChanges
{
    int         values{0};  ///< the values replaced
    std::string problems;   ///< a value that is not in the document as kReloadChanges has it
};

/// Makes of @p geometry, the committed geometry.json of the input @p input, the document that
/// OpenRocket's harness writes for its own reload of the re-save: the values of kReloadChanges.
/// Adds what it changed to @p changes; a value that the committed document does not hold as
/// the table says is a problem, and is left as it is.
void changeToTheReload(std::string_view input, nlohmann::json& geometry, ReloadChanges& changes);

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

/// Settles the design of @p resave, a re-saved test rocket as loaded (from its re-save, or
/// from a save of that), and compares it with the design documents @p goldens and with the
/// settings elements of @p file, the re-save of its input (compareLoadedDesign() of
/// GoldenDesign.h, compareSavedSettings() of GoldenSettings.h).
[[nodiscard]] ResaveRun compareLoadedResave(const LoadedResave&       resave,
                                            const DesignGoldens&      goldens,
                                            const pugi::xml_document& file);

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
void addResaveRun(ComparedResaves& compared, std::string_view name, const ResaveRun& run);

/// What the design documents of the thirteen test rockets hold together (the Python survey of
/// the probes of tier 9d, part "resaves": out/survey.txt). The 151 components and the 47
/// configurations are those TestRocketsGoldenCoverage pins for the rockets of the makers, and
/// the aerodynamics those of AeroGoldenCoverage.
[[nodiscard]] DesignCounts designCountsOfTheTestRockets();

/// The settings elements of the thirteen re-saves, as TestRocketsGoldenSettings pins them for
/// the rockets of the makers. No test rocket is made from a component preset.
[[nodiscard]] SettingsCounts settingsCountsOfTheTestRockets();

/// What the simulations of the re-saves are, and what comparing them with the golden simulation
/// documents gave.
struct ComparedSimulations
{
    int inputs{0};
    int simulations{0};    ///< the simulations of the loaded documents
    int notSimulated{0};   ///< those whose status is NOT_SIMULATED
    int cannotRun{0};      ///< those whose status is CANT_RUN
    int withoutMotors{0};  ///< those whose flight configuration has no motor, by geometry.json
    int intoTheWind{0};    ///< those that launch into the wind
    int optionValues{0};   ///< the values of the golden options that were compared
    int rodDirections{0};  ///< the stored launch rod directions compared with those of the files
    /// By simulation: what differs from its golden document. Empty when every simulation is
    /// the one the harness saved.
    std::string problems;
};

/// Compares the simulations of the document of @p resave, as loaded, with the simulation
/// documents the manifest lists for @p input, in their order (that of the harness's document,
/// and of @p file, the re-save itself).
void compareSimulations(ComparedSimulations& compared, const GoldenInput& input,
                        const LoadedResave& resave, const nlohmann::json& geometry,
                        const pugi::xml_document& file);

// =============================================================================== the options

/// What the comparison of the options of a simulation compared and found.
struct OptionsComparison
{
    int         values{0};  ///< the values of the golden options
    std::string report;     ///< the mismatches, one line each
};

/// Compares @p options with the golden "options" @p expected of a simulation document, value
/// for value: every value of the golden object, however deep, has to be there and equal (they
/// are settings: the numbers exactly), and @p options must give no value beyond them.
[[nodiscard]] OptionsComparison compareOptions(const nlohmann::json&    expected,
                                               const SimulationOptions& options);

// ===================================================================== the text of a re-save

/// What comparing an .ork text of QtRocket with one of OpenRocket gave (compareOrkTexts()).
struct OrkTextComparison
{
    /// The lines compared: those of the longer text.
    std::size_t lines{0};
    /// The lines that differ in nothing but numbers that read as the same double on both sides
    /// ("0.001" and "1.0E-3"): the same text by value, not reported.
    std::size_t byValue{0};
    /// The lines that differ otherwise, each reported in differences.
    std::size_t differing{0};
    /// The first kMaxReportedLines of the differing lines, each as "line N:" with the line of
    /// each side, then how many more there are; empty when no line differs.
    std::string differences;
};

/// How many differing lines OrkTextComparison::differences shows.
inline constexpr std::size_t kMaxReportedLines = 20;

/// Compares the .ork text @p ours (QtRocket's) with @p theirs (OpenRocket's), line by line (a
/// line ends at '\n'; a '\r' stays in its line and so differs): the value of the creator
/// attribute of the root element set aside on both sides (withoutCreator()), every other byte
/// compared, but for numbers of the same value (see OrkTextComparison). Lines are compared at
/// the same position, so an element that moved differs where it was and where it is. A text that
/// ends with a line feed where the other does not is one differing line more ("the end:").
[[nodiscard]] OrkTextComparison compareOrkTexts(std::string_view ours, std::string_view theirs);

/// The lines of @p text, each without its '\n' (a text that ends with '\n' has no empty line
/// after it).
[[nodiscard]] std::vector<std::string_view> lineViewsOf(std::string_view text);

/// The lines that differ between @p before and @p after, two texts of the same number of lines
/// (lineViewsOf()), each as "<line of before> -> <line of after>" without the indentation (the
/// spaces in front), one per line; for texts of another number of lines "<n> lines -> <m>
/// lines" instead. Empty when the lines are the same.
[[nodiscard]] std::string changedLines(std::string_view before, std::string_view after);

/// @p text with the value of its first creator attribute replaced by "CREATOR".
[[nodiscard]] std::string withoutCreator(std::string_view text);

/// The <rocket> element of the .ork text @p text: its lines from "  <rocket>" to "  </rocket>",
/// both included, each with its '\n'; empty when the text has none.
[[nodiscard]] std::string rocketElementOf(std::string_view text);

/// Whether @p ours and @p theirs, two lines, differ only in numbers that read as the same double
/// (Strings::javaParseDouble(), bit for bit): the same text around the numbers, the same count
/// of numbers, and at least one number written otherwise. A number starts where no ASCII letter,
/// digit or '.' precedes it, with an optional '-', and is digits with an optional fraction and
/// exponent ("E", an optional '-', digits): the digits inside a word (an id) are none.
[[nodiscard]] bool sameByValue(std::string_view ours, std::string_view theirs);

/// @p text with every UUID that @p original does not spell out (knownIds() of
/// ComponentHandlerTestSupport.h) replaced by "drawn-id-<n>", numbered from 1 in the order of
/// their first occurrence in @p text. A loader draws a random id wherever the file stores none
/// (every component of the formats 1.0 to 1.8, which store no component id; a flight
/// configuration the file does not name, such as the one a simulation with an empty <configid>
/// gets; and simulations, events and warnings), and a save writes it: two programs, or two
/// loads, draw other ids, which this makes the same wherever they stand in the same places. An
/// id of the file stays as it is.
[[nodiscard]] std::string withDrawnIdsNumbered(std::string_view text, std::string_view original);

/// The lines that are removed from @p before and added to @p after, a longest common
/// subsequence of their lines apart, in the order of the texts: "- <line>" for a line of
/// @p before alone and "+ <line>" for one of @p after alone, each without its indentation (the
/// spaces in front) and ended by '\n'. A line that changed is a "-" line and a "+" line. Empty
/// when the texts have the same lines. Texts too long to compare so (more than 25 million pairs
/// of differing lines in the middle) give "<n> lines -> <m> lines".
[[nodiscard]] std::string lineChanges(std::string_view before, std::string_view after);

/// lineChanges() of two texts given as their lines.
[[nodiscard]] std::string lineChanges(std::span<const std::string_view> before,
                                      std::span<const std::string_view> after);

/// lineChanges() with the place of every line: "- <n>: <line>" for the line n of @p before
/// and "+ <n>: <line>" for the line n of @p after (numbered from 1), each line as it is, its
/// indentation included. A line that moved is a change where it was and where it is, and a
/// line indented otherwise is a change.
[[nodiscard]] std::string placedLineChanges(std::string_view before, std::string_view after);

/// What differs between the .ork archives @p first and @p second of one design: the names of
/// their entries in their order, and the uncompressed bytes of each (an entry that differs is
/// shown by changedLines() of its texts); empty when nothing does. Counts the entries of @p first
/// into @p entries. (Decision O7 of tier 10: never the bytes of the archives themselves, which
/// carry the time of the save and the deflate stream of the zlib at hand.)
[[nodiscard]] std::string archiveChanges(std::span<const std::byte> first,
                                         std::span<const std::byte> second, std::size_t& entries);

// =============================== the re-saves of the legacy files and the coverage documents

/// What the manifest's "resaves" calls the re-saves of the design files of tests/data/ork.
inline constexpr std::string_view kLegacyKind = "legacy";
/// ... and those of the saver-coverage documents the harness builds (CoverageDocuments.java).
inline constexpr std::string_view kCoverageKind = "coverage";

/// A re-save of "resaves.inputs" of the manifest. File names are relative to goldensDir().
struct ResaveInput
{
    std::string name;  ///< "legacy-v1-6-apocd", "coverage-kitchen-sink"
    std::string kind;  ///< kLegacyKind or kCoverageKind
    /// The file of the repository a legacy re-save was made from ("tests/data/ork/<file>"), or
    /// the method of the harness that builds a coverage document.
    std::string source;
    /// The SHA-256 of the legacy file, as 64 hexadecimal digits; empty for a coverage document.
    std::string sourceSha256;
    /// OpenRocket's re-save: OpenRocketSaver's XML of the design as loaded (as built), without
    /// simulation data for a legacy file, with it where saveSimulationData says so.
    std::string resave;
    /// Whether the legacy file is an archive whose first entry is not the document, which the
    /// harness gave OpenRocket packed again with the document first (OpenRocket reads the first
    /// entry only; QtRocket takes the first *.ork entry, decision T3).
    bool documentEntryMovedFirst{false};
    /// Whether the re-save of a coverage document holds its simulation data (always false for a
    /// legacy file).
    bool saveSimulationData{false};
    /// The entries of OpenRocket's .ork archive of a legacy file as loaded (GeneralRocketSaver,
    /// with the simulation data), in their order; empty when it could not be saved.
    std::vector<std::string> archiveEntries;
    /// Why OpenRocket could not save that archive: "DecalNotFoundException: <the decal's name>";
    /// empty when it could.
    std::string archiveError;
};

/// A re-save WITH simulation data of an example ("resaves.withData"): OpenRocketSaver's XML of
/// the first save of a load of the example, gzip-compressed.
struct ResaveWithData
{
    std::string input;   ///< the example's golden input
    std::string resave;  ///< the file, relative to goldensDir()
};

/// "resaves" of the manifest.
struct ManifestResaves
{
    /// The digests of the component presets of the environment of the legacy files: the six of
    /// the example designs (makeExamplePresetDatabase()), sorted.
    std::vector<std::string>    legacyPresetDigests;
    std::vector<ResaveWithData> withData;
    std::vector<ResaveInput>    inputs;
};

/// Interprets "resaves" of the parsed manifest @p manifest (ErrorCode::PARSE when it is missing,
/// a required field is missing or has the wrong type, or a kind is neither kLegacyKind nor
/// kCoverageKind).
[[nodiscard]] Result<ManifestResaves> parseManifestResaves(const nlohmann::json& manifest);

/// "resaves" of goldensDir()/manifest.json, read once per test process.
[[nodiscard]] const Result<ManifestResaves>& manifestResaves();

/// The re-saves of the kind @p kind, in the manifest's order; the reason in @p problem when the
/// manifest cannot be read.
[[nodiscard]] std::vector<ResaveInput> resaveInputsOf(std::string_view kind, std::string& problem);

/// The re-save with simulation data of the example input @p input, or null.
[[nodiscard]] const ResaveWithData* resaveWithDataOf(std::string_view input);

/// The file a legacy re-save was made from: its "source" in the repository.
[[nodiscard]] std::filesystem::path legacyFileOf(const ResaveInput& input);

}  // namespace QtRocket::Test
