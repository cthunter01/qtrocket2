#pragma once

// The state of a design file as GeneralRocketLoader leaves it, in the lines the Java probes of
// tier 9c print for OpenRocket (RecordedProbe.java of part "recorded-files", ExampleProbe.java
// of part "examples"): what the tests that compare whole loaded files with OpenRocket share.
// Test-only.
//
// The state of a file as loaded, before anything is settled, is these lines (designFileState()):
//
//     version=<n>             the file version OpenRocketHandler leaves in the loading context
//     W <text>                the warnings of the load, in order
//     rocket ... decals=[..]  documentLines() of RocketLoaderTestSupport.h: the rocket, the
//                             stages each flight configuration has active, the simulations
//                             with status, branches and extensions, the storage options, the
//                             saved and undo state, the modification ids, the document
//                             materials, the photo settings and the decal images
//     events rocket=<n>       the change events of the rocket during the load
//     | <line>                describeRocket() of ComponentHandlerTestSupport.h: every component
//                             in depth-first order with its class, its name and what it holds,
//                             the selected and every flight configuration with its id, its
//                             stages and its motors
//     name <n> '<name>'       the name each flight configuration shows
//     simulation <n> config=<n>   the place of each simulation's configuration in the rocket
//     motor ...               every motor mount in every flight configuration: its motor
//                             with manufacturer, designation, digest and ejection delay, or
//                             "none"
//     decal '<name>' ...      every decal image and the number of its bytes, or "unreadable"
//       sim[<n>] ...          describeSimulation() of SimulationTestSupport.h: the options, the
//                             extensions with their configuration, the stored summary, the
//                             stored warnings, and every branch with its types, its events and
//                             a digest of every column
//
// The order of the questions is part of a state: an automatic dimension stores what it
// computes. The masses and the length of a design as loaded are therefore asked of a second
// load, on which nothing else was asked before (massAndLength()).

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Error.h"
#include "file/DesignFileEnvironment.h"
#include "file/RocketLoaderTestSupport.h"

namespace QtRocket::Test
{

/// "version=<n>": the file version OpenRocketHandler leaves in a loading context of
/// @p environment when it has read the document @p text (it starts at 0); a failure of the
/// read comes first, on a line of its own.
[[nodiscard]] std::string versionLine(const DesignFileEnvironment& environment,
                                      std::string_view             text);

/// "name default '<name>'" and "name <n> '<name>'": the name each flight configuration of
/// the rocket of @p document shows (FlightConfiguration::getName()), the default one first and
/// then in the order of Rocket::getIds(); then "simulation <n> config=<n>" for every simulation
/// of @p document: the place of its configuration in that order, or -1.
[[nodiscard]] std::string nameLines(const OpenRocketDocument& document,
                                    const Preferences&        preferences);

/// "motor config=<n> mount=#<place> <motor>" for every motor mount of @p rocket in every flight
/// configuration: by configuration in the order of Rocket::getIds(), then by mount in the order
/// of the tree (the place is the mount's among the lines of describeRocket(), the rocket being
/// 0). The motor is "<manufacturer>|<designation>|<digest>|delay=<ejection delay>", or "none".
[[nodiscard]] std::string motorLines(const Rocket& rocket);

/// "decal '<name>' bytes=<n>" for every image of the decal registry of @p document, in the
/// order of the names, or "decal '<name>' unreadable" for one whose bytes cannot be read.
[[nodiscard]] std::string decalLines(const OpenRocketDocument& document);

/// describeSimulation() of every simulation of @p document, the columns of the stored branches
/// as digests. It does to each simulation what the loader's next step does
/// (Simulation::syncModId() and getStatus()).
[[nodiscard]] std::string simulationLines(const OpenRocketDocument& document);

/// @p text as its lines, without the line feeds.
[[nodiscard]] std::vector<std::string> linesOf(std::string_view text);

/// "docprefs=<n>", the number of preferences @p document was saved with, and then "docmaterial
/// <text>" for every material of the document in the order and the form a save writes them
/// into <docmaterials> (getDocumentMaterials().allMaterials(), Material::toStorableString()).
[[nodiscard]] std::string documentPreferenceLines(const OpenRocketDocument& document);

/// What a state holds beyond the lines the top of this file lists.
enum class StateDetail
{
    /// Nothing more: the lines of RecordedProbe.
    RECORDED,
    /// documentPreferenceLines() at the end: the lines of ExampleProbe.
    WITH_DOCUMENT_PREFERENCES,
};

/// The lines of a state behind its version, for the load @p loaded of the design whose
/// document is the text @p document: the failure of a load that failed, else the warnings and
/// what the document holds (see the top of this file), every line ended. @p events are the
/// events of the load (countingOptions()), and @p preferences the store of the load, which
/// the names of the flight configurations are formatted with. The default units have to be
/// OpenRocket's (DefaultUnitsGuard): some names hold a value with its unit.
[[nodiscard]] std::string loadedDocumentState(const Result<LoadedDocument>& loaded,
                                              const LoadEvents&             events,
                                              const Preferences&            preferences,
                                              std::string_view              document,
                                              StateDetail detail = StateDetail::RECORDED);

/// The state of the design file @p file as a loader of @p environment loads it, in the lines
/// the top of this file lists; a load that fails gives its failure after the version.
[[nodiscard]] std::vector<std::string> designFileState(DesignFileEnvironment&       environment,
                                                       const std::filesystem::path& file,
                                                       StateDetail detail = StateDetail::RECORDED);

/// A line of a state in its short form, and the lines it stands for.
struct CompactLine
{
    /// The line, or what stands for a run of lines.
    std::string text;
    /// The lines @p text stands for, each ended by a line feed; empty for a line that stands
    /// for itself.
    std::string detail;
};

/// @p state in its short form, in which the long lines of a stored branch are digests:
/// - the line "types=<type> | <type> ..." of a branch is "types=<n> sha256=<digest>", with the
///   number of its types and the SHA-256 of the line (without its indentation);
/// - a run of "col <key>: ..." lines, the digests of the columns of a branch, is one line
///   "cols=<n> sha256=<digest>", with their number and the SHA-256 of the lines as they are,
///   each ended by a line feed.
/// Every other line stands for itself. The indentation is kept.
[[nodiscard]] std::vector<CompactLine> compactState(const std::vector<std::string>& state);

/// The texts of @p lines.
[[nodiscard]] std::vector<std::string> textsOf(const std::vector<CompactLine>& lines);

/// A number of a design, with its name.
struct DesignNumber
{
    std::string name;
    double      value;
};

/// What mass and length @p rocket has, asked in this order: the structure mass and its centre
/// with every stage active (a copy of the default flight configuration), then the launch mass
/// and the place of its centre for a copy of every flight configuration with every stage
/// active, then the length of the first copy.
[[nodiscard]] std::vector<DesignNumber> massAndLength(const Rocket& rocket);

/// Where @p found is not @p expected: "" when they are the same lines, else the first line
/// that differs, numbered from 1, with both texts, and the two numbers of lines.
[[nodiscard]] std::string firstDifference(std::span<const std::string_view> expected,
                                          const std::vector<std::string>&   found);

/// firstDifference() for a state in its short form: the first line whose text differs, and,
/// where that line stands for several, the lines that were found there.
[[nodiscard]] std::string firstDifference(std::span<const std::string_view> expected,
                                          const std::vector<CompactLine>&   found);

/// Whether @p found is @p expected to a relative 1e-9 (and to 1e-15 around zero).
[[nodiscard]] bool isCloseTo(double expected, double found) noexcept;

/// The numbers of @p found that are not the ones @p expected lists ("<name>=<value>", in the
/// same order, each value a double as Java prints it), one line each; "" when all of them are
/// (isCloseTo()).
[[nodiscard]] std::string wrongNumbers(std::span<const std::string_view> expected,
                                       const std::vector<DesignNumber>&  found);

}  // namespace QtRocket::Test
