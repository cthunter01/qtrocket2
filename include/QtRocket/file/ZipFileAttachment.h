#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// An attachment that is an entry of the archive a design was loaded from: a decal image
/// ("decals/logo.png") or an embedded thrust curve ("thrustcurves/<digest>.rse") of an .ork
/// container (OpenRocket's document/attachments/ZipFileAttachment). It lives in file/, not in
/// document/, because it reads the archive (ZipInputStream).
///
/// getBytes() walks the archive from its start, entry by entry, as Java's ZipInputStream does,
/// and returns the contents of the first entry whose name equals the attachment's name exactly:
/// byte for byte, the case and a leading slash included ("/datafiles/textures/x.jpg" and
/// "datafiles/textures/x.jpg" are different names, and one of OpenRocket's examples stores the
/// former). An entry's contents may take getMaxAttachmentBytes() bytes:
/// - an entry whose local header declares more fails with ErrorCode::IO and "Attachment '<name>'
///   exceeds the maximum size of <max> bytes", before anything is read;
/// - an entry that does not declare its size (see ZipInputStream::Entry::size) and inflates to
///   more fails with ErrorCode::IO and "Input exceeds maximum size of <max> bytes".
/// An archive without such an entry fails with decalNotFound(name) (ErrorCode::NOT_FOUND; Java:
/// DecalNotFoundException). Every entry before the one asked for is read through and checked on
/// the way, as in Java, so a corrupt or cut-off archive fails with ZipInputStream's error
/// (ErrorCode::PARSE) for it. An entry whose name ends with '/', a directory, is found like any
/// other, with its empty contents.
///
/// Deviations from OpenRocket:
/// - Java keeps the archive's URL and opens it again for every getBytes(). Here the attachment
///   holds the archive's bytes, shared with every other attachment of the same load
///   (std::shared_ptr), so a document does not depend on its source file once it is loaded: the
///   file may be replaced, moved or written over, and a design loaded from memory has its
///   attachments too.
/// - A null archive is a BugError in the constructor (Java: a NullPointerException at the first
///   getBytes()).
/// - The limit is a std::size_t (Java: an int, a negative one refused at the first read).
class ZipFileAttachment final : public Attachment
{
public:
    /// The most bytes an attachment may have: 32 MiB (MAX_ATTACHMENT_BYTES).
    static constexpr std::size_t kMaxAttachmentBytes = std::size_t{32} * 1024 * 1024;

    /// The bytes of an archive, shared by the attachments read from it.
    using Archive = std::shared_ptr<const std::vector<std::byte>>;

    /// The attachment named @p name of @p archive, whose contents may take
    /// @p maxAttachmentBytes bytes. Nothing is read here: the entry need not exist.
    /// @throws BugError when @p archive is null
    ZipFileAttachment(std::string name, Archive archive,
                      std::size_t maxAttachmentBytes = kMaxAttachmentBytes);

    /// The most bytes getBytes() returns.
    [[nodiscard]] std::size_t getMaxAttachmentBytes() const noexcept
    {
        return m_maxAttachmentBytes;
    }

    /// The contents of the entry, or why they could not be read (see the class comment).
    [[nodiscard]] Result<std::vector<std::byte>> getBytes() const override;

private:
    Archive     m_archive;
    std::size_t m_maxAttachmentBytes;
};

}  // namespace QtRocket
