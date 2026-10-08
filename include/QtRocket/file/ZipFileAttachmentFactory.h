#pragma once

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/file/AttachmentFactory.h"
#include "QtRocket/file/ZipFileAttachment.h"

namespace QtRocket
{

/// The attachments of a design that was loaded from an archive: each is a ZipFileAttachment of
/// that archive, found by its exact entry name when it is read (OpenRocket's
/// file/ZipFileAttachmentFactory).
///
/// Deviation from OpenRocket: Java keeps the archive's URL; here the factory holds the
/// archive's bytes and shares them with the attachments it makes, which keep them alive (see
/// ZipFileAttachment). A null archive is a BugError (Java: a NullPointerException when an
/// attachment is read).
class ZipFileAttachmentFactory final : public AttachmentFactory
{
public:
    /// A factory over @p archive, the bytes of the whole archive.
    /// @throws BugError when @p archive is null
    explicit ZipFileAttachmentFactory(ZipFileAttachment::Archive archive);

    /// A factory over the archive whose bytes are @p archive, which it takes over.
    explicit ZipFileAttachmentFactory(std::vector<std::byte> archive);

    /// The archive's bytes.
    [[nodiscard]] const ZipFileAttachment::Archive& getArchive() const noexcept
    {
        return m_archive;
    }

    /// A ZipFileAttachment named @p name with the default size limit
    /// (ZipFileAttachment::kMaxAttachmentBytes).
    [[nodiscard]] std::shared_ptr<Attachment> getAttachment(std::string_view name) const override;

private:
    ZipFileAttachment::Archive m_archive;
};

}  // namespace QtRocket
