#pragma once

#include <memory>
#include <string_view>

#include "QtRocket/document/Attachment.h"

namespace QtRocket
{

/// Makes the attachments a file being loaded refers to by name: its decal images and its
/// embedded thrust curves (OpenRocket's file/AttachmentFactory). The loader of a design chooses
/// the factory by where the design comes from: a ZipFileAttachmentFactory for an archive (a
/// zipped .ork), a FileSystemAttachmentFactory over the design's directory for a plain or
/// gzipped file.
///
/// A factory only names the attachment: nothing is read before Attachment::getBytes(), and an
/// attachment that does not exist is made like any other and fails there with
/// ErrorCode::NOT_FOUND.
class AttachmentFactory
{
public:
    virtual ~AttachmentFactory() = default;

    /// The attachment named @p name (getAttachment()); never null. Every call makes a new
    /// object, as in Java: the decal registry is what gives two references to one name one
    /// image.
    [[nodiscard]] virtual std::shared_ptr<Attachment> getAttachment(
        std::string_view name) const = 0;

protected:
    AttachmentFactory()                                    = default;
    AttachmentFactory(const AttachmentFactory&)            = default;
    AttachmentFactory(AttachmentFactory&&)                 = default;
    AttachmentFactory& operator=(const AttachmentFactory&) = default;
    AttachmentFactory& operator=(AttachmentFactory&&)      = default;
};

}  // namespace QtRocket
