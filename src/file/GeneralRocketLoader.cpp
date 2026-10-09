#include "QtRocket/file/GeneralRocketLoader.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/AttachmentFactory.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/FileSystemAttachmentFactory.h"
#include "QtRocket/file/GzipStream.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/ZipFileAttachment.h"
#include "QtRocket/file/ZipFileAttachmentFactory.h"
#include "QtRocket/file/ZipInputStream.h"
#include "QtRocket/file/openrocket/OpenRocketLoader.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"

namespace QtRocket
{

namespace
{

/// The bytes of a file, or of a document, that are looked at to find out what it is
/// (READ_BYTES).
constexpr std::size_t kReadBytes = 300;
/// Fewer bytes than this are no file and no document.
constexpr std::size_t kMinimumBytes = 10;

constexpr std::string_view kOpenRocketSignature = "<openrocket";
constexpr std::string_view kRockSimSignature    = "<RockSimDoc";
constexpr std::string_view kRasAeroSignature    = "<RASAeroDoc";

/// The characters Java's `.` does not match: the line terminators, as UTF-8.
constexpr std::array<std::string_view, 5> kLineTerminators{"\n", "\r", "\xC2\x85", "\xE2\x80\xA8",
                                                           "\xE2\x80\xA9"};

[[nodiscard]] std::unexpected<Error> unsupported()
{
    return fail(ErrorCode::UNSUPPORTED_FORMAT, "Unsupported or corrupt file.");
}

/// Java's failure for an IOException or an IllegalArgumentException that leaves
/// load(InputStream, String), with the code of @p cause.
[[nodiscard]] std::unexpected<Error> streamFailure(const Error& cause)
{
    return fail(cause.code, "Exception loading stream: " + cause.message);
}

/// The failure for a document of more than @p maxBytes bytes.
[[nodiscard]] std::unexpected<Error> exceedsBound(std::size_t maxBytes)
{
    return fail(
        ErrorCode::IO,
        std::format("Exception loading stream: Input exceeds maximum size of {} bytes", maxBytes));
}

/// Whether @p name is matched by Java's `.*<extension>$` as String.matches() applies it, the
/// ASCII letters of the extension in either case: it ends in @p extension (given in lower
/// case), and no line terminator stands before that.
[[nodiscard]] bool endsIn(std::string_view name, std::string_view extension) noexcept
{
    if (name.size() < extension.size())
    {
        return false;
    }
    const std::string_view stem       = name.substr(0, name.size() - extension.size());
    const std::string_view tail       = name.substr(stem.size());
    const auto             sameLetter = [](char found, char lower) noexcept {
        return found == lower || (found >= 'A' && found <= 'Z' && found - 'A' + 'a' == lower);
    };
    if (!std::ranges::equal(tail, extension, sameLetter))
    {
        return false;
    }
    return std::ranges::none_of(kLineTerminators, [stem](std::string_view terminator) {
        return stem.contains(terminator);
    });
}

/// Whether @p bytes start with @p signature.
[[nodiscard]] bool startsWith(std::span<const std::byte> bytes, std::string_view signature) noexcept
{
    return bytes.size() >= signature.size() &&
           std::ranges::equal(bytes.first(signature.size()), signature,
                              [](std::byte b, char c) { return b == static_cast<std::byte>(c); });
}

/// Whether "<openrocket" is in @p start, as Java's loop finds it: a byte that does not go on
/// the match starts the search over and is not itself tried as a first byte.
[[nodiscard]] bool hasOpenRocketSignature(std::span<const std::byte> start) noexcept
{
    std::size_t match = 0;
    for (const std::byte b : start)
    {
        if (b == static_cast<std::byte>(kOpenRocketSignature.at(match)))
        {
            match++;
            if (match == kOpenRocketSignature.size())
            {
                return true;
            }
        }
        else
        {
            match = 0;
        }
    }
    return false;
}

/// The attachments of a document that is no archive: the files of @p directory, or files
/// named against the current directory when there is none (setAttachmentFactory()).
[[nodiscard]] std::unique_ptr<const AttachmentFactory> fileAttachments(
    const std::optional<std::filesystem::path>& directory)
{
    if (directory.has_value())
    {
        return std::make_unique<const FileSystemAttachmentFactory>(*directory);
    }
    return std::make_unique<const FileSystemAttachmentFactory>();
}

/// The document a gzip stream holds: what could be unpacked of it, with the damage that ended
/// it; or the failure for a stream of more than @p maxBytes bytes.
[[nodiscard]] Result<InflatedPrefix> documentOfGzip(std::span<const std::byte> stream,
                                                    std::size_t                maxBytes)
{
    InflatedPrefix document = gzipInflatePrefix(stream, maxBytes);
    // Everything but damage (the bound, a stream that could not be set up) ends the load here.
    if (document.failure.has_value() && document.failure->code != ErrorCode::PARSE)
    {
        return streamFailure(*document.failure);
    }
    return document;
}

/// Moves @p zip to the entry that is the document: the first one when its name says so, as in
/// Java, else the first later one named *.ork. Fails when the archive has none or cannot be
/// read that far.
[[nodiscard]] Result<ZipInputStream::Entry> documentEntry(ZipInputStream& zip)
{
    Result<std::optional<ZipInputStream::Entry>> entry = zip.nextEntry();
    if (!entry)
    {
        return streamFailure(entry.error());
    }
    if (!entry->has_value())
    {
        return unsupported();
    }
    const std::string_view first = (*entry)->name;
    if (endsIn(first, ".ork") || endsIn(first, ".rkt") || endsIn(first, ".cdx1"))
    {
        return std::move(**entry);
    }
    // Not OpenRocket's, which returns the empty rocket here: the *.ork entry is the document.
    while (true)
    {
        entry = zip.nextEntry();
        if (!entry)
        {
            return streamFailure(entry.error());
        }
        if (!entry->has_value())
        {
            return unsupported();
        }
        if (endsIn((*entry)->name, ".ork"))
        {
            return std::move(**entry);
        }
    }
}

/// The document a zip archive holds, as documentOfGzip() gives a gzip stream's.
[[nodiscard]] Result<InflatedPrefix> documentOfArchive(std::span<const std::byte> archive,
                                                       std::size_t                maxBytes)
{
    ZipInputStream                      zip(archive);
    const Result<ZipInputStream::Entry> entry = documentEntry(zip);
    if (!entry)
    {
        return std::unexpected(entry.error());
    }
    // What the header says of the size is enough to refuse the entry, before anything is read.
    if (std::cmp_greater(entry->size, maxBytes))
    {
        return exceedsBound(maxBytes);
    }
    InflatedPrefix document;
    if (Result<void> read = zip.readEntryInto(document.bytes, maxBytes); !read)
    {
        if (read.error().code != ErrorCode::PARSE)
        {
            return streamFailure(read.error());
        }
        document.failure = std::move(read.error());
    }
    return document;
}

/// Reads @p document, which is OpenRocket's by its first bytes, into the document of
/// @p context (loadUsing() with the OpenRocket loader). @p damage is what ended the stream the
/// document came from, when it did not simply end.
[[nodiscard]] Result<void> loadOpenRocketDocument(DocumentLoadingContext&     context,
                                                  std::span<const std::byte>  document,
                                                  const std::optional<Error>& damage,
                                                  WarningSet&                 warnings)
{
    // A deflate stream whose data ends too early is, to Java's XML parser, a document that
    // ends there: the parser takes the EOFException of the stream for the end of its input.
    // So what could be unpacked is read as the document, and is malformed unless it happens
    // to be complete. Any other damage is the IOException that the parser meets when it has
    // read the part before it (AbstractRocketLoader.load()); here the stream was unpacked
    // first, so nothing of the document is read.
    if (damage.has_value() && damage->message != kUnexpectedEndOfZlibStream)
    {
        return fail(damage->code, "I/O error: " + damage->message);
    }
    if (Result<void> loaded = OpenRocketLoader::load(context, document, warnings); !loaded)
    {
        const ErrorCode code = loaded.error().code;
        if (code == ErrorCode::PARSE || code == ErrorCode::IO ||
            code == ErrorCode::UNSUPPORTED_FORMAT)
        {
            // The loader's own failures: Java's RocketLoadException, which passes as it is.
            return loaded;
        }
        // A handler's or a setter's (Java: the IllegalArgumentException).
        return streamFailure(loaded.error());
    }

    // Check for custom materials that need to be added to the document material database
    context.getOpenRocketDocument()->reloadDocumentMaterials();
    return {};
}

/// Finds out what @p document is and loads it into the document of @p context (loadRocket()).
[[nodiscard]] Result<void> loadRocket(DocumentLoadingContext&     context,
                                      std::span<const std::byte>  document,
                                      const std::optional<Error>& damage, WarningSet& warnings)
{
    // Java reads the first bytes of the stream here: a stream that fails before it has given
    // them fails the load with its exception.
    if (damage.has_value() && document.size() < kReadBytes)
    {
        return streamFailure(*damage);
    }
    const std::span<const std::byte> start = document.first(std::min(document.size(), kReadBytes));
    if (start.size() < kMinimumBytes)
    {
        return unsupported();
    }

    // Check for OpenRocket
    if (hasOpenRocketSignature(start))
    {
        return loadOpenRocketDocument(context, document, damage, warnings);
    }

    // Check for RockSim
    if (startsWith(start, kRockSimSignature))
    {
        // HOOK(rocksim): the RockSim loader.
        return fail(ErrorCode::UNSUPPORTED_FORMAT, "RockSim design files are not supported.");
    }

    // Check for RASAero
    if (startsWith(start, kRasAeroSignature))
    {
        // HOOK(rasaero): the RASAero loader.
        return fail(ErrorCode::UNSUPPORTED_FORMAT, "RASAero design files are not supported.");
    }
    return unsupported();
}

/// What the handlers ask of the environment whenever a file has the element for it.
void requireEnvironment(const DocumentLoadingContext& environment)
{
    if (environment.getMotorFinder() == nullptr)
    {
        bug("The environment of the rocket loader has no motor finder");
    }
    if (environment.getPreferences() == nullptr)
    {
        bug("The environment of the rocket loader has no preference store");
    }
}

}  // namespace

GeneralRocketLoader::GeneralRocketLoader(const DocumentLoadingContext& environment)
  : GeneralRocketLoader(environment, Options{})
{
}

GeneralRocketLoader::GeneralRocketLoader(DocumentLoadingContext environment, Options options)
  : m_environment(std::move(environment)), m_options(std::move(options))
{
    requireEnvironment(m_environment);
}

Result<LoadedDocument> GeneralRocketLoader::load(const std::filesystem::path& file) const
{
    // The file as Java's File spells it, which is what its message names.
    const std::filesystem::path    spelled = withoutRedundantSeparators(file);
    Result<std::vector<std::byte>> bytes   = readFile(spelled, m_options.maxFileBytes);
    if (!bytes)
    {
        return fail(ErrorCode::IO, std::format("Exception loading file: {} , {}",
                                               pathToUtf8(spelled), bytes.error().message));
    }
    // Java: baseFile.getParentFile(), which is null for a file named without a directory.
    const std::filesystem::path directory = spelled.parent_path();
    if (directory.empty())
    {
        return loadBytes(std::move(*bytes), std::nullopt, std::filesystem::path("."));
    }
    return loadBytes(std::move(*bytes), directory, directory);
}

Result<LoadedDocument> GeneralRocketLoader::load(
    std::vector<std::byte> bytes, const std::optional<std::filesystem::path>& baseDirectory) const
{
    return loadBytes(std::move(bytes), baseDirectory, baseDirectory);
}

Result<LoadedDocument> GeneralRocketLoader::loadBytes(
    std::vector<std::byte> bytes, const std::optional<std::filesystem::path>& attachmentDirectory,
    std::optional<std::filesystem::path> designDirectory) const
{
    // Java: the document is a field of the loader, made before anything is read, with the
    // events of its rocket enabled.
    LoadedDocument         loaded{.document = OpenRocketDocumentFactory::createEmptyRocket(),
                                  .warnings = {}};
    DocumentLoadingContext context = m_environment;
    context.setOpenRocketDocument(loaded.document.get());
    context.setFileVersion(0);
    context.setDesignDirectory(std::move(designDirectory));

    // loadStep1()
    if (bytes.size() < kMinimumBytes)
    {
        return unsupported();
    }
    std::unique_ptr<const AttachmentFactory> attachments;
    // What a stream gave, and the document, which is that or the bytes themselves.
    InflatedPrefix             unpacked;
    std::span<const std::byte> document;
    // The bytes of an archive, which its attachments keep.
    ZipFileAttachment::Archive archive;

    if (looksLikeGzip(bytes))
    {
        attachments                     = fileAttachments(attachmentDirectory);
        Result<InflatedPrefix> inflated = documentOfGzip(bytes, m_options.maxDocumentBytes);
        if (!inflated)
        {
            return std::unexpected(std::move(inflated.error()));
        }
        unpacked = std::move(*inflated);
        document = unpacked.bytes;
    }
    else if (startsWith(bytes, "PK"))
    {
        archive     = std::make_shared<const std::vector<std::byte>>(std::move(bytes));
        attachments = std::make_unique<const ZipFileAttachmentFactory>(archive);
        Result<InflatedPrefix> entry = documentOfArchive(*archive, m_options.maxDocumentBytes);
        if (!entry)
        {
            return std::unexpected(std::move(entry.error()));
        }
        unpacked = std::move(*entry);
        document = unpacked.bytes;
    }
    else
    {
        if (bytes.size() > m_options.maxDocumentBytes)
        {
            return exceedsBound(m_options.maxDocumentBytes);
        }
        attachments = fileAttachments(attachmentDirectory);
        document    = bytes;
    }
    context.setAttachmentFactory(attachments.get());
    if (m_options.beforeReading)
    {
        m_options.beforeReading(context);
    }

    if (Result<void> read = loadRocket(context, document, unpacked.failure, loaded.warnings); !read)
    {
        return std::unexpected(std::move(read.error()));
    }

    // Java: load(InputStream, String) ends with this. The events were on all along; the call
    // updates the rocket.
    loaded.document->getRocket().enableEvents();
    return loaded;
}

}  // namespace QtRocket
