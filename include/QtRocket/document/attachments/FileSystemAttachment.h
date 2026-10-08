#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// An attachment that is a file on disk (OpenRocket's document/attachments/
/// FileSystemAttachment): the name the document refers to it by, and where the file is. The
/// decal registry treats it apart from every other attachment: it finds the image of such an
/// attachment by the file, not by the name (DecalRegistry::getDecalImage()).
///
/// Deviations from OpenRocket:
/// - getBytes() reads the whole file at once; Java opens a FileInputStream that the caller reads.
///   What Java's constructor of that stream refuses with a FileNotFoundException (a file that does
///   not exist, a directory, a file that may not be opened) is ErrorCode::NOT_FOUND here, and a
///   failure while reading (Java: an IOException in the caller) is ErrorCode::IO. The message is
///   QtRocket's, with the path in UTF-8, not the operating system's text Java gives.
/// - The file may take getMaxAttachmentBytes() bytes, by default the 32 MiB an entry of an
///   archive may take (Attachment::kMaxAttachmentBytes). Java has no limit for a file, since it
///   hands out a stream; here the name of the file can come from a design ("thrustcurves/
///   <digest>.rse" next to a plain .ork, an absolute decal name), and reading it whole must not
///   exhaust the memory. A larger file fails with ErrorCode::IO and the two texts
///   ZipFileAttachment has: "Attachment '<name>' exceeds the maximum size of <max> bytes" when
///   the file says so itself, before anything is read, and "Input exceeds maximum size of <max>
///   bytes" when more bytes come than it said (readLocation(), which knows no name, gives the
///   second text in both cases).
/// - Only a regular file is read. Whatever else the path names (a device such as /dev/zero, a
///   pipe, a socket: Java opens them, and blocks on or never finishes with some) fails with
///   ErrorCode::IO and "cannot read '<path>': not a regular file", without being opened. A link
///   to a regular file is one.
class FileSystemAttachment final : public Attachment
{
public:
    /// An attachment named @p name whose bytes are the file @p location, which may take
    /// @p maxAttachmentBytes bytes. Nothing is checked here: the file need not exist.
    FileSystemAttachment(std::string name, std::filesystem::path location,
                         std::size_t maxAttachmentBytes = kMaxAttachmentBytes);

    /// Where the file is.
    [[nodiscard]] const std::filesystem::path& getLocation() const noexcept { return m_location; }

    /// The most bytes getBytes() returns.
    [[nodiscard]] std::size_t getMaxAttachmentBytes() const noexcept
    {
        return m_maxAttachmentBytes;
    }

    /// The bytes of the file, as readLocation(getLocation(), getMaxAttachmentBytes()), but for
    /// the text of the failure for a file that says it is too large (see the class comment).
    [[nodiscard]] Result<std::vector<std::byte>> getBytes() const override;

    /// Reads the file @p file as an attachment's source: ErrorCode::NOT_FOUND when it does not
    /// exist, is a directory or cannot be opened; ErrorCode::IO when it is no regular file, when
    /// it holds more than @p maxBytes bytes ("Input exceeds maximum size of <maxBytes> bytes")
    /// or when reading it fails (see the class comment). A decal image reads its decal file with
    /// it.
    [[nodiscard]] static Result<std::vector<std::byte>> readLocation(
        const std::filesystem::path& file, std::size_t maxBytes = kMaxAttachmentBytes);

private:
    std::filesystem::path m_location;
    std::size_t           m_maxAttachmentBytes;
};

}  // namespace QtRocket
