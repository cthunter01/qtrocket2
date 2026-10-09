#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// Loads a design file: the public entry of the file subsystem (OpenRocket's
/// file/GeneralRocketLoader). It finds out what a file is, unpacks a gzip stream or a zip
/// archive, chooses where the attachments of the design come from and hands the document to
/// OpenRocketLoader. A load gives a new OpenRocketDocument and the warnings of the load
/// (LoadedDocument), or the Error that says why there is none.
///
/// THE ENVIRONMENT. The loader is made with a DocumentLoadingContext in which the caller has
/// set what it has (Java takes all of it from the application's global objects):
/// - a motor finder (required): a DatabaseMotorFinder over the motor database, used during a
///   load only;
/// - a preference store (required): the simulations of every document this loader returns
///   keep it and go on reading and writing it, so IT MUST OUTLIVE THOSE DOCUMENTS. It also
///   gives the default material of a component whose element names none;
/// - the application's materials, the component presets and the registry of simulation
///   extensions (each may be missing; DocumentLoadingContext says what a load makes of that),
///   used during a load only: a component holds a copy of its material and shares its preset,
///   and an extension does not refer to the registry that made it.
/// The loader keeps a copy of the context; for every load it copies that again and sets the
/// document, the file version, the attachment factory and the design's directory itself
/// (whatever the caller set for these four is not used). A document that was returned needs
/// neither the loader nor anything of the environment but the preference store, nor the file
/// it came from: the bytes of an archive are shared by its attachments.
///
/// WHAT A FILE IS, as OpenRocket decides it, byte for byte (loadStep1() and loadRocket()):
/// - fewer than 10 bytes: "Unsupported or corrupt file.";
/// - the bytes 1f 8b first: a gzip stream, which is unpacked;
/// - the bytes "PK" first: a zip archive, read from its local headers as
///   java.util.zip.ZipInputStream reads it (not from its central directory). An archive
///   without a first entry is "Unsupported or corrupt file.". When the name of the first
///   entry ends in ".ork", ".rkt" or ".cdx1", the ASCII letters in either case, that entry is
///   the document. Otherwise (not OpenRocket's, see the deviations) the first later entry
///   whose name ends in ".ork" is, and an archive without one is "Unsupported or corrupt
///   file.". "Ends in" is Java's `.*\.[oO][rR][kK]$` on the whole name: what stands before the
///   extension must not have a line terminator in it;
/// - anything else is the document itself.
/// The document, unpacked, is then looked at again: fewer than 10 bytes are "Unsupported or
/// corrupt file."; "<openrocket" anywhere in its first 300 bytes makes it an OpenRocket
/// document (found by Java's matcher, which starts over at a byte that does not fit without
/// trying that byte as a first one, so "<<openrocket" is not found); "<RockSimDoc" or
/// "<RASAeroDoc" as its first 11 bytes make it a document of that program, which QtRocket
/// does not read; everything else is "Unsupported or corrupt file.". So a document whose root
/// element starts after byte 300, one in UTF-16 and one with a prefixed root
/// (<or:openrocket>) are not recognised, a gzip stream in a gzip stream or in an archive is
/// not unpacked twice, and a document whose root is <openrocketx> is read (and gives "Unknown
/// element openrocketx, ignoring." and the empty rocket).
///
/// THE ATTACHMENTS (decal images, embedded thrust curves): from an archive they are its
/// entries, found by their exact name, a leading slash included (ZipFileAttachmentFactory),
/// also when the archive was given as bytes. For a plain or gzip document they are files
/// (FileSystemAttachmentFactory): beside the design for load(file), in the base directory of
/// load(bytes), and resolved against the current directory of the process when there is
/// neither. Nothing is read before an attachment is asked for its bytes.
///
/// AFTER THE DOCUMENT IS READ, in OpenRocket's order: the steps of OpenRocketLoader (stage
/// activeness, the simulations' modification ids, the storage options, documentLoaded() of
/// the extensions, the undo history cleared), then the document's materials are collected
/// from the components (OpenRocketDocument::reloadDocumentMaterials()), then the rocket is
/// updated (Rocket::enableEvents(): the events were on all along). The document that is
/// returned
/// - has no file (OpenRocketDocument::getFile()) and is not saved (isSaved()), unless the
///   file changed nothing at all (no <rocket>, or nothing in it): whoever opened the file
///   calls setFile() and setSaved(true), as OpenRocket's file actions do;
/// - has an undo history of one state, the loaded one, with nothing to undo or redo;
/// - has the storage options of a design file: OPENROCKET, not explicitly set, and saving the
///   simulated data when a simulation came with stored data.
///
/// FAILURES. A failed load returns no document. The texts are OpenRocket's:
/// - "Exception loading file: <file> , <reason>", ErrorCode::IO: the file cannot be opened or
///   read, or is larger than the file bound. The reason is QtRocket's own text (Java: the
///   operating system's);
/// - "Unsupported or corrupt file.", ErrorCode::UNSUPPORTED_FORMAT: see above;
/// - "Malformed XML in input.", ErrorCode::PARSE: the document is not well-formed XML;
/// - "I/O error: <name>", ErrorCode::IO: the document names an encoding that is not read;
/// - "Exception loading stream: <message>", ErrorCode::INVALID_ARGUMENT: a handler or setter
///   refused a value that fails the load in OpenRocket too, <message> being the message of
///   Java's exception ("Invalid UUID string: x", "Attempted to set the configuration to an
///   error id. Not Allowed!"). The message can be empty, and the text is then the prefix
///   alone;
/// - a gzip stream or an archive that is damaged, ErrorCode::PARSE, as OpenRocket meets the
///   damage while it reads the stream:
///   - before 300 bytes of the document could be unpacked, or on the way to a later entry of
///     an archive: "Exception loading stream: <reason>" ("Unexpected end of ZLIB input
///     stream", "invalid entry CRC (expected 0x... but got 0x...)", "encrypted ZIP entry not
///     supported" and the other texts of ZipInputStream and gzipInflatePrefix());
///   - further on, in a stream whose first 300 bytes are not a document's: "Unsupported or
///     corrupt file.", the damage unseen;
///   - further on in an OpenRocket document, when the data of a deflate stream ends too early
///     (kUnexpectedEndOfZlibStream): what could be unpacked is read as the document, because
///     Java's XML parser takes the end of such a stream for the end of its input. It is
///     "Malformed XML in input." then, unless the document is complete all the same (a gzip
///     stream that lacks only its last bytes, the check sum), and then it loads;
///   - any other damage further on in an OpenRocket document (a wrong check sum or size, a
///     stored entry that is cut short): "I/O error: <reason>".
/// - a document beyond the document bound, ErrorCode::IO: "Exception loading stream: Input
///   exceeds maximum size of <n> bytes";
/// - a RockSim or RASAero document, ErrorCode::UNSUPPORTED_FORMAT, with a text that names the
///   program;
/// - a document type declaration with declarations of its own, ErrorCode::UNSUPPORTED_FORMAT
///   with XmlScanner's text (OpenRocket reads such a document).
/// What a failed load leaves changed outside the document it discards: a stepper method that
/// a <simulationsteppermethod> element named is written to the preference store when the
/// element is read, as in OpenRocket; the flight data types a stored branch named stay in
/// FlightDataType's registry of the process; and the motor finder and the attachments were
/// asked. Nothing else is touched.
///
/// BOUNDS (not OpenRocket's, which has none): a design file may have kMaxFileBytes bytes and
/// the unpacked document kMaxDocumentBytes. An archive entry whose header says that it holds
/// more than the document bound is refused before anything is unpacked; a gzip stream, and an
/// entry that does not say or that holds more than it says, is given up at the bound, where
/// the unpacking stops. No memory is taken for a number a file merely states. An attachment
/// keeps Java's limit (Attachment::kMaxAttachmentBytes).
///
/// Deviations from OpenRocket:
/// - An object made with the environment and asked any number of times, each load with a new
///   document (Java: one loader object per file, which holds the document and the warnings;
///   the document is the Result's, with the warnings, in a LoadedDocument). Java's fileName
///   argument, which the loader of this format does not read, is gone.
/// - The zip archive whose first entry is not a design: OpenRocket returns the empty rocket
///   without any message (a later save would then write an empty design over the file). Here
///   the first later entry named *.ork is the document (an archive OpenRocket 13.05 wrote has
///   "decals/" first), and an archive without one is "Unsupported or corrupt file.".
/// - An archive given as bytes has its attachments (Java: a NullPointerException for a zip
///   read from a stream without a file).
/// - A gzip stream or an archive entry is unpacked whole before the document is looked at
///   (Java reads a stream through buffers). Three things follow for a damaged stream, which
///   fails in both: with damage that gives "I/O error: <reason>" nothing of the document is
///   read, where Java's handlers have read the part before the damage and may fail first; a
///   gzip stream with a wrong check sum or length behind its data, which Java meets after
///   the document however short that is ("I/O error: Corrupt GZIP trailer"), is "Exception
///   loading stream: <reason>" here for a document of less than 300 bytes, and the reason is
///   QtRocket's own text; and a stored entry with a wrong check sum, which Java refuses
///   together with its last block of up to 8192 bytes ("Exception loading stream: invalid
///   entry CRC ..." for a design of that size), is judged by the rule of the 300 bytes here.
///   Only the first member of a gzip stream of several is read.
/// - RockSim and RASAero documents are not read (not part of this milestone).
/// - The bounds above.
/// - What the environment must hold is checked when the loader is made: a context without a
///   motor finder or without a preference store is a BugError (Java: its globals are there).
/// - There is no catch-all: nothing here turns a BugError or another exception into an Error.
///   No file can make the loader throw one; an environment that breaks its contract can.
/// - RocketLoader, AbstractRocketLoader and RocketLoadException are not ported as classes
///   (see OpenRocketLoader): the failure of a load is the Error of the Result.
class GeneralRocketLoader final
{
public:
    /// The most bytes a design document may have once it is unpacked: 256 MiB.
    static constexpr std::size_t kMaxDocumentBytes = std::size_t{256} * 1024 * 1024;

    /// The most bytes a design file may have, the attachments of an archive included: 1 GiB.
    static constexpr std::size_t kMaxFileBytes = std::size_t{1024} * 1024 * 1024;

    /// What a load may take and who watches it. An application leaves this alone; the tests
    /// of the bounds lower them, so that they need no file of 256 MiB.
    struct Options
    {
        /// The most bytes of the unpacked document.
        std::size_t maxDocumentBytes{kMaxDocumentBytes};
        /// The most bytes of the file load(file) reads.
        std::size_t maxFileBytes{kMaxFileBytes};
        /// Called with the context of a load when the loader has set it up (the new, empty
        /// document, the attachment factory, the design's directory) and has unpacked the
        /// file, before it looks at the document and reads it: for who wants to hear the
        /// events of a load (Java's document is a field of the loader from its construction)
        /// or to see what the handlers are given. It is not called for a file that is refused
        /// before that (too short, a stream that cannot be unpacked, beyond a bound). The
        /// listeners it connects stay with the document.
        std::function<void(const DocumentLoadingContext&)> beforeReading;
    };

    /// A loader that loads with @p environment (see the class comment), of which it keeps a
    /// copy.
    /// @throws BugError when @p environment has no motor finder or no preference store
    explicit GeneralRocketLoader(const DocumentLoadingContext& environment);

    /// The same with @p options.
    GeneralRocketLoader(DocumentLoadingContext environment, Options options);

    /// Loads the design file @p file (load()). Its attachments are the file's entries when it
    /// is an archive and the files of its directory otherwise, and the design's directory of
    /// the context is that directory.
    [[nodiscard]] Result<LoadedDocument> load(const std::filesystem::path& file) const;

    /// Loads the design that @p bytes, the contents of a design file, hold (load(InputStream,
    /// String)). Its attachments are the entries of @p bytes when they are an archive, and
    /// otherwise the files of @p baseDirectory, or files named against the current directory
    /// when there is none; @p baseDirectory is also the design's directory of the context.
    /// The bytes are taken over: an archive is kept by the attachments of the document.
    [[nodiscard]] Result<LoadedDocument> load(
        std::vector<std::byte>                      bytes,
        const std::optional<std::filesystem::path>& baseDirectory = std::nullopt) const;

private:
    /// Loads the design @p bytes hold: the attachments of a document that is no archive are
    /// the files of @p attachmentDirectory, and @p designDirectory goes into the context.
    [[nodiscard]] Result<LoadedDocument> loadBytes(
        std::vector<std::byte>                      bytes,
        const std::optional<std::filesystem::path>& attachmentDirectory,
        std::optional<std::filesystem::path>        designDirectory) const;

    DocumentLoadingContext m_environment;
    Options                m_options;
};

}  // namespace QtRocket
