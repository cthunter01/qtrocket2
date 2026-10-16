#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DecalImage;
class OpenRocketDocument;
class Preferences;

/// Saves a document as a design file: the public entry of the file subsystem for saving
/// (OpenRocket's file/GeneralRocketSaver), the counterpart of GeneralRocketLoader.
///
/// WHAT IS WRITTEN. For storage options of the type OPENROCKET, and of the type WAVEFRONT_OBJ,
/// for which OpenRocket writes the same, the file is an .ork archive, a zip archive (ZipWriter:
/// deflated at level 9, as Java's ZipOutputStream.setLevel(9)), never a plain or a gzip
/// document. Its entries, in this order:
/// 1. "rocket.ork": the design, OpenRocketSaver::save() with the options (with the type
///    OPENROCKET for WAVEFRONT_OBJ), its warnings and errors those of the save;
/// 2. "preview.png": the options' preview image, when they hold one that is not empty;
/// 3. the decal images the components show: of every component of the rocket, the rocket
///    included, the image of the decal of its appearance and that of its inside appearance
///    (an InsideColorComponent's), each image once, in the order of their names
///    (Strings::javaCompareTo(); Java: a TreeSet of the images, which compare by name), each
///    under the name rocket.ork gives it (OrkWriter::decalImageName(): its name unless that is
///    not well-formed UTF-8 or holds what a design file cannot hold, which decalImageRenamed()
///    then warns of), with the bytes DecalImage::getBytes() gives. An image the user chose to
///    leave out (DecalImage::isIgnored()) is not written and is no longer left out afterwards
///    (setIgnored(false)): the flag is cleared when the image's turn comes, as in Java, so it
///    stays cleared when a later image then fails the save;
/// 4. "thrustcurves/<digest>.rse": every thrust-curve motor of the design, written as a
///    RockSim engine file (RockSimMotorWriter, UTF-8, with OrkWriter::sanitizeText(): what XML
///    cannot hold in its texts is left out with OpenRocketSaver::kDroppedCharacters, decision E3
///    of tier 10), so that the file carries the motors with it. A motor is of the design when a
///    component that is a motor mount (MotorMount::isMotorMount(), a mount that is no longer one
///    does not count) has it in one of the flight configurations of the rocket (Rocket::getIds()).
///    Motors are taken in the order of the components (pre-order) and, for each, of the
///    configurations; a motor whose digest is empty is left out, and of two motors of the same
///    digest the first. Every such motor is written, those of the motor database too (the file can
///    then be read where the database lacks them).
/// After a successful save the options hold no preview image any more
/// (StorageOptions::clearPreviewImage()), as in Java: for save(), once the file is in place, so
/// that a save that failed to write the file keeps the image for the next.
///
/// FAILURES, for which there is no archive and no file is touched: an Error of
/// OpenRocketSaver::save(); the Error of a decal image whose bytes cannot be read
/// (decalNotFound(), ErrorCode::NOT_FOUND, for a missing source: the image is then
/// getMissingDecal(), which a caller can set to be ignored, isIgnored(), and save again, as
/// OpenRocket's frame does), or that the document does not know (a decal naming no image of
/// OpenRocketDocument::findDecalImage(): decalNotFound() with the name); ZipWriter's refusal of
/// two entries of one name ("duplicate entry: <name>", such as a decal image named
/// "rocket.ork", or two decal images whose names rocket.ork writes as one) or of a name it cannot
/// write, and its failures of an archive beyond 2 GiB; memory that runs out (kOutOfMemory,
/// ErrorCode::IO: nothing a document holds makes a save throw); and for storage options of the
/// types ROCKSIM and RASAERO, unsupportedFileType() (ErrorCode::UNSUPPORTED_FORMAT): QtRocket
/// writes neither format yet.
///
/// SAVING TO A FILE (save()): a temporary file is made in the directory of the file first
/// (FileIo replaceFileWith()), then the archive is made and written to it, and the temporary
/// file is renamed to the file, which it replaces. A failure of any step leaves the file as it
/// was and removes the temporary file; a directory that cannot take the file fails before the
/// document is written (Java's order: the temporary file is made first). The new file has the
/// permissions the file had (FileIo replaceFile()). A file marked read-only (no write
/// permission for anyone, the read-only attribute on Windows) is replaced as OpenRocket
/// replaces it, and so stays read-only; a save that replaced one has the save warning
/// fileWasReadOnly() (the user's answer of 2026-10-16), and a save that fails has its Error
/// alone.
///
/// The caller (the command line, later the GUI) sets the document's file and its saved state
/// after a successful save (OpenRocketDocument::setFile(), setSaved(true)), as OpenRocket's
/// BasicFrame does: the saver does not.
///
/// The warnings and the errors of the last save are getWarnings() and getErrors(), and its missing
/// decal image getMissingDecal(); all three are cleared when a save begins, a save() that fails
/// before the design is written included (Java: the warnings when the design begins to be
/// written; the missing image is the exception's). They are what a caller shows the user (the
/// command line saves nothing; the GUI will). QtRocket's own warnings of the container are
/// decalImageRenamed(), fileWasReadOnly() and those of the loader's bounds below.
///
/// THE LOADER'S BOUNDS (decision O12 of tier 10, and the user's answer of 2026-10-16: a save
/// warning for every bound of QtRocket's loader that the saved design exceeds). The file is
/// written whatever they say. The bounds of the text of rocket.ork are OpenRocketSaver's, whose
/// warnings this save has too; those of the archive are checked here, as the loader meets them:
/// - an archive longer than GeneralRocketLoader::kMaxFileBytes, which the loader does not open:
///   fileBeyondLoaderBound();
/// - decal images and embedded thrust curves longer than Attachment::kMaxAttachmentBytes, the
///   most a ZipFileAttachment of the loader reads of an entry (a decal image is read when it is
///   shown or saved again, an embedded thrust curve when the motor database lacks its motor):
///   attachmentsBeyondLoaderBound(), one warning for all of them. Every embedded thrust curve
///   counts, also that of a motor the database has, which a reader elsewhere may lack.
/// The other bounds of the archive and its reader cannot be exceeded by a file this saver
/// writes: an archive beyond 2 GiB, an entry name longer than 65535 bytes, a backslash or a
/// NUL character in one (ZipWriter refuses to write each of them, and the save fails, writing
/// nothing); the 2 GiB of a gzip stream (no gzip stream is written); the rules by which the
/// loader finds the document (rocket.ork is the first entry, and its text starts with the XML
/// declaration and <openrocket within its first 300 bytes); the rule that a design read from a
/// plain file reads only the decal images and thrust curves in its directory (DesignFiles: the
/// loader reads the entries of an archive whatever their names); the preview image (the loader
/// does not read it); and ZipFileMotorLoader::kMaxNesting (the motor files of the database,
/// which a design load does not read).
///
/// Deviations from OpenRocket:
/// - The archive is built in memory and returned (saveToBytes()) or written whole; Java streams
///   it into the file. Each entry is compressed when it is added (ZipWriter), so a design with
///   many large decal images that compress well needs the memory of the compressed archive and of
///   one image, and memory that runs out anyway is an Error (kOutOfMemory; Java: an
///   OutOfMemoryError). (Each image is read through its attachment, an archive's from the start
///   of that archive, as in Java.) Java's SavingProgress callback is not ported: there is no
///   stream to watch (estimateFileSize() is, for a caller that wants to show what is coming).
/// - ROCKSIM and RASAERO: Java writes them with RockSimSaver and RASAeroSaver (Milestone 8:
///   // HOOK(rocksim), // HOOK(rasaero)); estimateFileSize() is 0 for them.
/// - Nothing is written when the save fails: Java has written rocket.ork (and the decals
///   before a missing one) into the temporary file, which it then deletes.
/// - The entries carry the local time of the save or, for files that are to have the same
///   headers whenever they are made, the time of Options::entryTime (Java: always the moment
///   of the save).
/// - ZipWriter refuses entry names Java writes (a backslash, a NUL character, more than 65535
///   bytes; see ZipWriter). A decal image is stored under OrkWriter::decalImageName(), its name
///   with a backslash as a slash, without what XML cannot hold and as well-formed UTF-8, in the
///   archive and in rocket.ork alike (Java: the name as it is in both, which for such a name
///   makes a file in which the decal is lost, or that no reader reads).
/// - The texts of an embedded RockSim engine file go through OrkWriter::sanitizeText() (Java
///   writes them as RockSimMotorWriter escapes them).
/// - A decal that names an image the document does not know fails as a missing image (Java's
///   decal holds its image, so there is no such decal).
/// - fileBeyondLoaderBound(), attachmentsBeyondLoaderBound(), decalImageRenamed() and
///   fileWasReadOnly(): warnings of QtRocket's (decision O12 of tier 10; the reviews of tier
///   10a; the user's answers of 2026-10-16). Java has none of them.
/// - No "<file>-bak" is made while the file is replaced (see FileIo replaceFile()), and the
///   temporary file is "QtRocket-<16 hexadecimal digits>.tmp" (Java: "ORSave<digits>.tmp"). The
///   new file has the permissions of the old one, so a read-only file that is replaced stays
///   read-only (Java: the permissions of a new file, so it becomes writable).
class GeneralRocketSaver final
{
public:
    /// What a saver does beyond what Java's does.
    struct Options
    {
        /// The time every entry of the archive carries (ZipWriter::setEntryTime()), for files
        /// that are to be the same whenever they are made; nullopt (the default) for the local
        /// time of the save, as Java.
        std::optional<ZipWriter::EntryTime> entryTime;
    };

    /// The save warning for an archive of @p bytes bytes, beyond
    /// GeneralRocketLoader::kMaxFileBytes (QtRocket's text, decision O12 of tier 10): "The saved
    /// file is <bytes> bytes long; QtRocket loads a file of at most <kMaxFileBytes> bytes and
    /// will not open this one."
    [[nodiscard]] static std::string fileBeyondLoaderBound(std::size_t bytes);

    /// The save warning for @p count entries of the archive that the loader reads as attachments
    /// (decal images and embedded thrust curves) and that are longer than
    /// Attachment::kMaxAttachmentBytes, the longest of them @p longest bytes long (QtRocket's
    /// text): "Attachments of the saved file (decal images or thrust curves) are longer than
    /// QtRocket reads: <count> of more than <kMaxAttachmentBytes> bytes, the longest <longest>
    /// bytes long. QtRocket will not read them when it opens this file."
    [[nodiscard]] static std::string attachmentsBeyondLoaderBound(std::size_t count,
                                                                  std::size_t longest);

    /// The save warning for a save that replaced @p file, a file marked read-only (QtRocket's
    /// text, the user's answer of 2026-10-16): "The file '<file>' was read-only; QtRocket
    /// replaced it and left it read-only." (@p file as pathToUtf8() spells it).
    [[nodiscard]] static std::string fileWasReadOnly(const std::filesystem::path& file);

    /// The Error of a save with storage options of @p fileType (ROCKSIM or RASAERO): "QtRocket
    /// cannot save a design in the <name> format yet." (QtRocket's text).
    [[nodiscard]] static std::string unsupportedFileType(StorageOptions::FileType fileType);

    /// The save warning for the decal image @p name, which is stored as @p stored
    /// (OrkWriter::decalImageName(); QtRocket's text): "The decal image "<name>" is saved as
    /// "<stored>": a design file cannot hold its name as it is." (@p name as well-formed UTF-8).
    [[nodiscard]] static std::string decalImageRenamed(std::string_view name,
                                                       std::string_view stored);

    /// The Error of a save that ran out of memory (QtRocket's text).
    static constexpr std::string_view kOutOfMemory =
        "There was not enough memory to save the design.";

    /// A saver that writes with @p preferences, the application's, which decide whether a flight
    /// configuration's name is its own (OpenRocketSaver::save()). They must outlive the saver.
    explicit GeneralRocketSaver(const Preferences& preferences);
    /// The same with @p options.
    GeneralRocketSaver(const Preferences& preferences, Options options);

    /// Saves @p document to @p file with the document's default storage options (Java:
    /// save(File, document)): see the class comment. The options' preview image is cleared
    /// after a successful save.
    [[nodiscard]] Result<void> save(const std::filesystem::path& file,
                                    OpenRocketDocument&          document);

    /// Saves @p document to @p file with @p options (Java: save(File, document, options)).
    [[nodiscard]] Result<void> save(const std::filesystem::path& file, OpenRocketDocument& document,
                                    StorageOptions& options);

    /// The bytes of the file save() would write: the archive (see the class comment), or the
    /// Error of the save. The document is not const for the reasons of OpenRocketSaver::save();
    /// the options lose their preview image after a successful save; a decal image that was
    /// left out is no longer.
    [[nodiscard]] Result<std::vector<std::byte>> saveToBytes(OpenRocketDocument& document,
                                                             StorageOptions&     options);

    /// The size the file is estimated to have (Java: estimateFileSize()):
    /// OpenRocketSaver::estimateFileSize() for the types OPENROCKET and WAVEFRONT_OBJ, 0 for
    /// ROCKSIM and RASAERO (Java: the length of their text; HOOK(rocksim), HOOK(rasaero)).
    [[nodiscard]] static std::int64_t estimateFileSize(const OpenRocketDocument& document,
                                                       const StorageOptions&     options);

    /// The warnings of the last save.
    [[nodiscard]] const WarningSet& getWarnings() const noexcept { return m_warnings; }
    /// The errors of the last save (Java: no saver of the format adds one).
    [[nodiscard]] const ErrorSet& getErrors() const noexcept { return m_errors; }

    /// The decal image whose source was missing in the last save, which then failed with
    /// decalNotFound() (Java: DecalNotFoundException.getDecal()); null otherwise.
    [[nodiscard]] const std::shared_ptr<DecalImage>& getMissingDecal() const noexcept
    {
        return m_missingDecal;
    }

private:
    /// Clears the warnings, the errors and the missing decal image of the last save.
    void clearLastSave();

    /// The archive of @p document with @p options (see the class comment), or the Error of the
    /// save, running out of memory included; the options are left as they are.
    [[nodiscard]] Result<std::vector<std::byte>> archive(OpenRocketDocument&   document,
                                                         const StorageOptions& options);

    /// archive() but for the memory: the entries in their order.
    [[nodiscard]] Result<std::vector<std::byte>> buildArchive(OpenRocketDocument&   document,
                                                              const StorageOptions& options);

    /// The attachments of an archive that are longer than the loader reads
    /// (attachmentsBeyondLoaderBound()).
    struct LargeAttachments
    {
        /// How many there are.
        std::size_t count{0};
        /// The bytes of the longest.
        std::size_t longest{0};

        /// Counts an attachment of @p bytes bytes when it is longer than
        /// Attachment::kMaxAttachmentBytes.
        void add(std::size_t bytes) noexcept;
    };

    /// Adds the decal images the components of @p document show to @p archive (step 3 of the
    /// class comment), each to @p large.
    [[nodiscard]] Result<void> addDecals(OpenRocketDocument& document, ZipWriter& archive,
                                         LargeAttachments& large);

    /// Adds the thrust curves of the motors of @p document to @p archive (step 4 of the class
    /// comment), each to @p large.
    void addThrustCurves(const OpenRocketDocument& document, ZipWriter& archive,
                         LargeAttachments& large);

    const Preferences*          m_preferences;
    Options                     m_options;
    WarningSet                  m_warnings;
    ErrorSet                    m_errors;
    std::shared_ptr<DecalImage> m_missingDecal;
};

}  // namespace QtRocket
