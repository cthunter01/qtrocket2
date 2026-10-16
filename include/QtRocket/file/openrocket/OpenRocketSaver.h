#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class OpenRocketDocument;
class Preferences;
class Rocket;

/// Writes a document as the text of an OpenRocket design file: the XML document rocket.ork,
/// which GeneralRocketSaver packs into an .ork archive (OpenRocket's
/// file/openrocket/OpenRocketSaver). The text is built in memory (OrkWriter), with '\n' line
/// ends only and UTF-8, and is meant to be written in binary mode, so that the file has the
/// same bytes on every platform.
///
/// What save() writes, in Java's order:
/// - the declaration `<?xml version='1.0' encoding='utf-8'?>` (single quotes, as Java);
/// - `<openrocket version="1.11" creator="QtRocket <version>">` (creatorString(); Java writes
///   "OpenRocket <version>"). The version is calculateNecessaryFileVersion(), which is always
///   kFileVersion, the newest version the loader knows (DocumentConfig::kSupportedVersions);
/// - the rocket, recursively (Java's saveComponent()): for each component the lines its saver
///   gives (ComponentSaver; Java's getElements(): the open tag, the parameter lines, the close
///   tag, or nothing at all, as for a stage its kind's saver does not expect); with
///   children, after the parameters a blank line and <subcomponents>, the children with a blank
///   line BETWEEN two of them, </subcomponents>; then the close tag. A blank line follows the
///   rocket;
/// - <datatypes> with one <type source="customexpression"> per custom expression (name, symbol,
///   unit with unittype="auto", expression), followed by a blank line, when the document has
///   custom expressions;
/// - <simulations>, the simulations with a blank line between two of them, each written by
///   SimulationSaver (Java's saveSimulation(); their data only when the options save it);
/// - <photostudio> with PhotoStudioSaver's lines, written also when there are none;
/// - <docprefs>: each document preference as `<pref key="k" type="boolean|integer|double|string">`
///   (OrkWriter::entry() with explicit number types), then, when the document has materials,
///   <docmaterials> with one <material> per material, Material::toStorableString(), in the
///   order of MaterialStorage::allMaterials() (Java's Database: by name with String.compareTo,
///   then by density, across the three types).
///
/// After the text is complete save() adds QtRocket's save warnings (decision U7 of tier 9: the
/// texts are here, but for SimulationSaver::lookupRowsNotSaved(), which SimulationSaver adds while
/// it writes): kDroppedCharacters once when OrkWriter::escape() dropped any character,
/// dispersionAttributesLeftOut() once when SimulationSaver left out attributes of landing
/// dispersion settings that the loader could not read back (SimulationSaver::attributesLeftOut()
/// of every simulation), and a warning for each of the bounds of QtRocket's loader that the saved
/// design exceeds, so that QtRocket would not read it back in full (decision O12 of tier 10, and
/// the user's answer of 2026-10-16: a save warning for every bound the design exceeds). The file
/// is written all the same.
///
/// THE LOADER'S BOUNDS. The bounds of the text are checked here, those of the archive by
/// GeneralRocketSaver. Each is counted as the loader counts it, in the order in which it reads
/// the file, and by itself: as though the loader had no other bound. (Where a design exceeds
/// several, what the loader refuses under one can keep it from reaching another, so that a
/// warning may name a bound under which the loader then refuses nothing.) Each warning names
/// the count, the bound and what the loader does; documentBeyondLoaderBound() is added for the
/// text, the others are beyondLoaderBounds() of the design:
/// - the text longer than GeneralRocketLoader::kMaxDocumentBytes, which the loader does not
///   open: documentBeyondLoaderBound();
/// - more than DocumentConfig::kMaxComponents components below the rocket
///   (DocumentConfig::componentCount()): componentsBeyondLoaderBound();
/// - a component more than ComponentHandler::kMaxDepth levels below the rocket, a stage being at
///   level 1: nestingBeyondLoaderBound(), with the level of the deepest;
/// - more than DocumentConfig::kMaxAssemblies stages, booster sets and pod sets
///   (DocumentConfig::assemblyCount()): assembliesBeyondLoaderBound();
/// - an instance count (<instancecount>) or the line count of a parachute (<linecount>) above the
///   maximum of the setter the loader applies to it, which refuses the count (IntSetter::maximum()
///   of DocumentConfig::findSetter(): the saver asks the loader's table, so that the two
///   agree). That is DocumentConfig::kMaxCount for the instance counts of a launch lug, a rail
///   button, a centering ring, a bulkhead, a pod set and a booster set and for the line count of
///   a parachute (a fin set clamps its count to 8 itself, and an inner tube sets its count
///   through its cluster configuration): countsBeyondLoaderBound(), with how many and the
///   largest;
/// - more component instances over all flight configurations than DocumentConfig::kMaxInstances
///   at any moment of the load (DocumentConfig::instanceLoad()): the loader makes the flight
///   configurations of the rocket first, then attaches each component, in the order of the file,
///   with the instance count its constructor gives it (2 for a pod set and a booster set, 3 for a
///   fin set) and sets the component's own count afterwards, and makes the flight
///   configurations the simulations name and the rocket lacks last. So a component whose count
///   is below the one it is made with can take the rocket beyond the bound while it is read,
///   although the saved rocket is within it: instancesBeyondLoaderBound(), with the most
///   instances the load comes to;
/// - more flight configurations than DocumentConfig::kMaxFlightConfigurations: those of the
///   rocket, the default one not counted (Rocket::getFlightConfigurationCount()), and those the
///   simulations name and the rocket lacks (a configuration that was removed from the rocket),
///   which the loader makes (SingleSimulationHandler): flightConfigurationsBeyondLoaderBound();
/// - more than DocumentConfig::kMaxSimulations simulations: simulationsBeyondLoaderBound();
/// - a multi-level wind model of more than DocumentConfig::kMaxWindLevels levels that the loader
///   takes (WindHandler passes over a level whose numbers are not finite or lie outside the ranges
///   of a simulation, and does not count it): windLevelsBeyondLoaderBound(), with the most levels
///   of one model;
/// - more than DocumentConfig::kMaxDecalImages decal images: the names rocket.ork gives the
///   images of the decals (OrkWriter::decalImageName() of decalImageNames()), each the name of an
///   entry of the archive, which the loader registers one image each (AppearanceHandler):
///   decalImagesBeyondLoaderBound();
/// - flight data with more than DocumentConfig::kMaxWarnings stored warnings (FlightDataHandler):
///   the warnings of FlightData::getWarningSet() as WarningHandler reads each back
///   (WarningHandler::makeWarning() of its type name, its number and its description without
///   what a design file cannot hold, with its line ends as the XML parser hands them on and
///   trimmed; with its id, priority and sources), of which two that are equal are one, as the
///   loader's set holds them, and but one equal to LoaderWarnings::tooManyStoredWarnings(),
///   which the capacity of that set does not count: storedWarningsBeyondLoaderBound(), with
///   the most of one flight;
/// - a list in the configuration of a simulation extension nested more than
///   ConfigHandler::kMaxListDepth deep, a list directly in the configuration being 1 deep:
///   listNestingBeyondLoaderBound(), with the deepest.
/// Not checked:
/// - the warnings of the load itself (DocumentConfig::kMaxWarnings, the capacity of the set
///   GeneralRocketLoader returns): it bounds what a load reports, not what it reads, so a design
///   beyond it loads in full; and which warnings a load gives is the loader's answer to every
///   element, which the saver cannot count without loading the file;
/// - the XML scanner's limits (XmlScanner::kMaxNameLength UTF-16 units of an element or
///   attribute name, XmlScanner::kMaxAttributes attributes of an element) and its names: the
///   saver writes the names of its own vocabulary, a dozen attributes to an element at most,
///   but in one place, the landing dispersion settings, whose attributes are named by the design.
///   There SimulationSaver writes no attribute the loader could not read back (a name that
///   XmlScanner::isPlainAttributeName() refuses, an attribute beyond kMaxAttributes of one
///   element) and save() warns of what it left out (dispersionAttributesLeftOut()): the loader
///   would refuse the whole file, which is never written (decision 3 of tier 10);
/// - the file version (always one the loader knows, see kFileVersion) and the document type
///   declaration the loader refuses (the saver writes none);
/// - ConfigHandler::kMaxListDepth for a <pref> of the document preferences, which the loader
///   reads with a ConfigHandler too: a document preference holds no list
///   (DocumentPreferences::Value), so the saver writes none;
/// - the CsvMachAoALookup::kMaxFileBytes of a lookup table's file and the rule that the file
///   lies in the design's directory: the saver writes the rows of a table (SimulationSaver), which
///   the loader takes before any file, and a table whose rows it cannot read, its file beyond
///   that bound included, already has the save warning SimulationSaver::lookupRowsNotSaved();
/// - what the loader does not bound: the rows of stored flight data and their events, the rows
///   of a lookup table, the deployment and separation configurations of a component, and the
///   other lists of a design (custom expressions, document materials and preferences, plot
///   appearances);
/// - ranges and finite values (decisions U9 and L3 of the loader, SimulationInputRanges), which
///   the loader asks of each value by itself: they are rules of the values, not bounds of the
///   design.
///
/// OpenRocket's file/RocketSaver, the abstract base class of the savers, is not ported as a
/// class: its save(dest, doc, warnings, errors) with the document's default storage options is
/// the save() overload without options, and its estimateFileSize() is estimateFileSize() here.
///
/// Deviations from OpenRocket:
/// - save() returns the text, or an Error, and never throws (Java: an IOException of the stream,
///   a BugException or a RuntimeException of a saver). A storage option of another file type
///   than OPENROCKET is ErrorCode::UNSUPPORTED_FORMAT (unsupportedFileType()): Java's
///   OpenRocketSaver writes its format whatever the options say, and GeneralRocketSaver hands it
///   the WAVEFRONT_OBJ options too, so GeneralRocketSaver passes OPENROCKET for that type.
/// - Every text and attribute value goes through OrkWriter::escape() (decision E3 of tier 10),
///   also those Java writes unescaped: the custom expressions, the <pref> keys, the photo-studio
///   values and the <docmaterials> strings (see OrkWriter for what differs from Java's bytes).
/// - The document preferences are written sorted by key (DocumentPreferences::preferences(), a
///   std::map; Java iterates a HashMap, whose order is unspecified).
/// - The save warnings above are QtRocket's own; Java gives none.
/// - Attributes of landing dispersion settings that the loader could not read back are left out
///   (see SimulationSaver; Java writes the attributes of its own MonteCarloSettings).
/// - The creator is QtRocket's.
/// - estimateFileSize() counts in 64 bits (Java multiplies ints, which wrap beyond about 21
///   million data points).
/// - The ErrorSet is never written to, as in Java (no OpenRocket saver adds an error).
class OpenRocketSaver final
{
public:
    OpenRocketSaver() = delete;

    /// What turns a version "major.minor" into an integer (Java: FILE_VERSION_DIVISOR): the
    /// loader's, so that the two cannot disagree.
    static constexpr int kFileVersionDivisor = DocumentConfig::kFileVersionDivisor;

    /// The file version the saver writes: 111, "1.11" (Java: FILE_VERSION_DIVISOR + 11). It is
    /// DocumentConfig::kSupportedVersions.back() (checked at compile time).
    static constexpr int kFileVersion = kFileVersionDivisor + 11;

    /// Estimated storage per part of a document, hand-estimated by OpenRocket from saved files
    /// (Java: BYTES_PER_COMPONENT_COMPRESSED and the like).
    static constexpr std::int64_t kBytesPerComponentCompressed  = 80;
    static constexpr std::int64_t kBytesPerSimulationCompressed = 100;
    static constexpr std::int64_t kBytesPerDatapointCompressed  = 100;

    /// The save warning for a save in which OrkWriter::escape() dropped characters (decision E3
    /// of tier 10).
    static constexpr std::string_view kDroppedCharacters =
        "Characters that a design file cannot hold (control characters other than tab, line feed "
        "and carriage return, U+FFFE, U+FFFF) were left out of the saved texts.";

    /// The save warning for @p count attributes of landing dispersion settings that the saver
    /// left out (SimulationSaver::attributesLeftOut(); QtRocket's text): "<count> attributes of
    /// landing dispersion settings were left out of the saved design: a design file holds at
    /// most <kMaxAttributes> attributes to an element, each named by an XML 1.0 name without a
    /// colon of at most <kMaxNameLength> characters, other than "xmlns"."
    [[nodiscard]] static std::string dispersionAttributesLeftOut(std::size_t count);

    /// The save warning for a document text of @p bytes bytes, beyond
    /// GeneralRocketLoader::kMaxDocumentBytes: "The saved design is <bytes> bytes long; QtRocket
    /// loads a design of at most <kMaxDocumentBytes> bytes and will not open this file."
    [[nodiscard]] static std::string documentBeyondLoaderBound(std::size_t bytes);

    /// The save warning for a rocket of @p components components (DocumentConfig::
    /// componentCount(), the rocket not counted), beyond DocumentConfig::kMaxComponents: "The
    /// saved rocket has <components> components; QtRocket loads at most <kMaxComponents> and will
    /// ignore the ones beyond when it opens this file."
    [[nodiscard]] static std::string componentsBeyondLoaderBound(std::size_t components);

    /// The save warning for a rocket whose deepest component stands @p depth levels below it (a
    /// stage at 1), beyond ComponentHandler::kMaxDepth: "The saved rocket has components <depth>
    /// levels below the rocket; QtRocket loads components at most <kMaxDepth> levels deep and
    /// will ignore the deeper ones, with everything in them, when it opens this file."
    [[nodiscard]] static std::string nestingBeyondLoaderBound(std::size_t depth);

    /// The save warning for a rocket of @p assemblies stages, booster sets and pod sets, beyond
    /// DocumentConfig::kMaxAssemblies: "The saved rocket has <assemblies> stages, booster sets
    /// and pod sets; QtRocket loads at most <kMaxAssemblies> and will ignore the ones beyond,
    /// with everything in them, when it opens this file."
    [[nodiscard]] static std::string assembliesBeyondLoaderBound(std::size_t assemblies);

    /// The save warning for @p counts counts of instances or of parachute lines above
    /// DocumentConfig::kMaxCount, the largest @p largest: "Counts of instances or of parachute
    /// lines in the saved rocket are above what QtRocket loads: <counts> above <kMaxCount>, the
    /// largest <largest>. QtRocket refuses such a count when it opens this file, and the
    /// component keeps the count it is made with."
    [[nodiscard]] static std::string countsBeyondLoaderBound(std::size_t counts, int largest);

    /// The save warning for a load that comes to @p instances component instances over all
    /// flight configurations, beyond DocumentConfig::kMaxInstances: "The saved rocket comes to
    /// <instances> component instances over its flight configurations while it is read;
    /// QtRocket loads at most <kMaxInstances> and will ignore the components, counts, cluster
    /// layouts and flight configurations that would go beyond, with the motors and simulations
    /// of those configurations, when it opens this file."
    [[nodiscard]] static std::string instancesBeyondLoaderBound(std::uint64_t instances);

    /// The save warning for @p configurations flight configurations, beyond
    /// DocumentConfig::kMaxFlightConfigurations: "The saved design has <configurations> flight
    /// configurations; QtRocket loads at most <kMaxFlightConfigurations> and will ignore the
    /// ones beyond, with their motors and simulations, when it opens this file."
    [[nodiscard]] static std::string flightConfigurationsBeyondLoaderBound(
        std::size_t configurations);

    /// The save warning for @p simulations simulations, beyond DocumentConfig::kMaxSimulations:
    /// "The saved design has <simulations> simulations; QtRocket loads at most <kMaxSimulations>
    /// and will ignore the ones beyond when it opens this file."
    [[nodiscard]] static std::string simulationsBeyondLoaderBound(std::size_t simulations);

    /// The save warning for a wind model of @p levels levels, beyond
    /// DocumentConfig::kMaxWindLevels: "A simulation of the saved design has a wind model of
    /// <levels> levels; QtRocket loads at most <kMaxWindLevels> levels of a wind model and will
    /// ignore the ones beyond when it opens this file."
    [[nodiscard]] static std::string windLevelsBeyondLoaderBound(std::size_t levels);

    /// The save warning for @p images decal images, beyond DocumentConfig::kMaxDecalImages: "The
    /// saved rocket shows <images> decal images; QtRocket loads at most <kMaxDecalImages> and
    /// will ignore the decals of the images beyond when it opens this file."
    [[nodiscard]] static std::string decalImagesBeyondLoaderBound(std::size_t images);

    /// The save warning for flight data of @p warnings stored warnings, beyond
    /// DocumentConfig::kMaxWarnings: "A simulation of the saved design stores <warnings>
    /// warnings with its flight; QtRocket loads at most <kMaxWarnings> warnings of a flight and
    /// will drop the ones beyond when it opens this file."
    [[nodiscard]] static std::string storedWarningsBeyondLoaderBound(std::size_t warnings);

    /// The save warning for lists nested @p depth deep in an extension's configuration, beyond
    /// ConfigHandler::kMaxListDepth: "A simulation extension of the saved design has lists
    /// nested <depth> deep in its configuration; QtRocket loads lists nested at most
    /// <kMaxListDepth> deep and will ignore the deeper ones when it opens this file."
    [[nodiscard]] static std::string listNestingBeyondLoaderBound(std::size_t depth);

    /// The save warnings for the bounds of the loader that @p document exceeds, in the order of
    /// the class comment, but for the length of the text (documentBeyondLoaderBound()), which
    /// save() adds itself: what save() adds to its warnings for the design. Empty for a design
    /// the loader reads in full.
    [[nodiscard]] static std::vector<std::string> beyondLoaderBounds(
        const OpenRocketDocument& document);

    /// The names of the decal images the components of @p rocket show, the rocket included: of
    /// the decal of each component's appearance and of its inside appearance (an
    /// InsideColorComponent's), each name once, sorted with Strings::javaCompareTo() (Java: a
    /// TreeSet of the images, which compare by name). GeneralRocketSaver writes these images.
    [[nodiscard]] static std::vector<std::string> decalImageNames(const Rocket& rocket);

    /// The Error of save() for storage options of @p fileType, which is not OPENROCKET:
    /// "OpenRocketSaver writes the OpenRocket format; the storage options ask for ROCKSIM."
    [[nodiscard]] static std::string unsupportedFileType(StorageOptions::FileType fileType);

    /// Writes @p document with @p options (Java: save(OutputStream, document, options, warnings,
    /// errors)): the text of the file, or ErrorCode::UNSUPPORTED_FORMAT when @p options are of
    /// another file type than OPENROCKET (nothing is written then). @p preferences are the
    /// application's, which decide whether a flight configuration's name is its own
    /// (FlightConfiguration::isNameOverridden()). The save warnings are added to @p warnings.
    /// The document is not const: the status of a simulation is worked out while it is written
    /// (Simulation::getStatus(), as Java), and the getters of automatic dimensions store what
    /// they compute, as Java's do; nothing else changes, and the document is never settled
    /// before it is written.
    [[nodiscard]] static Result<std::string> save(OpenRocketDocument&   document,
                                                  const StorageOptions& options,
                                                  const Preferences&    preferences,
                                                  WarningSet& warnings, ErrorSet& errors);

    /// save() with the document's default storage options (Java: RocketSaver.save(dest, doc,
    /// warnings, errors)).
    [[nodiscard]] static Result<std::string> save(OpenRocketDocument& document,
                                                  const Preferences&  preferences,
                                                  WarningSet& warnings, ErrorSet& errors);

    /// Java's estimateFileSize(): 80 bytes per component (the rocket included), 100 per
    /// simulation and, when @p options save the simulation data, 100 per data point (the rows
    /// of every branch of every simulation that has flight data). Decals are not estimated, as
    /// in Java.
    [[nodiscard]] static std::int64_t estimateFileSize(const OpenRocketDocument& document,
                                                       const StorageOptions&     options);

    /// The file version needed to store everything @p document holds (Java:
    /// calculateNecessaryFileVersion(), whose test accessor is
    /// testAccessor_calculateNecessaryFileVersion()): always kFileVersion, as in Java.
    [[nodiscard]] static int calculateNecessaryFileVersion(const OpenRocketDocument& document,
                                                           const StorageOptions&     options);

    /// The text of a file version: (version / kFileVersionDivisor) + "." + (version %
    /// kFileVersionDivisor), "1.11" for 111.
    [[nodiscard]] static std::string fileVersionText(int fileVersion);
};

}  // namespace QtRocket
