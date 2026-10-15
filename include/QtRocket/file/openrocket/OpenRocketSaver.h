#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class OpenRocketDocument;
class Preferences;

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
/// it writes): kDroppedCharacters once when OrkWriter::escape() dropped any character, and
/// a warning for each of QtRocket's own load bounds the saved design exceeds, so that QtRocket
/// would not read it back in full (decision O12 of tier 10): the text beyond
/// GeneralRocketLoader::kMaxDocumentBytes (documentBeyondLoaderBound()), a rocket of more than
/// DocumentConfig::kMaxComponents components (componentsBeyondLoaderBound()). The file is
/// written all the same.
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

    /// The save warning for a document text of @p bytes bytes, beyond
    /// GeneralRocketLoader::kMaxDocumentBytes: "The saved design is <bytes> bytes long; QtRocket
    /// loads a design of at most <kMaxDocumentBytes> bytes and will not open this file."
    [[nodiscard]] static std::string documentBeyondLoaderBound(std::size_t bytes);

    /// The save warning for a rocket of @p components components (DocumentConfig::
    /// componentCount(), the rocket not counted), beyond DocumentConfig::kMaxComponents: "The
    /// saved rocket has <components> components; QtRocket loads at most <kMaxComponents> and will
    /// ignore the ones beyond when it opens this file."
    [[nodiscard]] static std::string componentsBeyondLoaderBound(std::size_t components);

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
