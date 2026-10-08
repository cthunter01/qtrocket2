#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/file/AttachmentFactory.h"

namespace QtRocket
{

/// The attachments of a design that is a plain (or gzipped) file: each is a
/// FileSystemAttachment, a file next to the design (OpenRocket's
/// file/FileSystemAttachmentFactory). The base directory is the directory of the design; a
/// factory without one resolves relative names against the current directory, as Java's File
/// does.
///
/// getAttachment(name) takes an absolute name as it is and resolves a relative one against the
/// base directory, exactly as Java: nothing is checked, so "../x.png" and "/etc/x" name files
/// outside the base directory (a decal may be anywhere; who must not follow a name out of the
/// directory checks the name first, as the motor handler checks a digest). The attachment keeps
/// the name it was asked by, whatever file it resolves to.
///
/// Deviations from OpenRocket:
/// - Java's constructor refuses a base that is not a directory (IllegalArgumentException "Base
///   file for FileSystemAttachmentFactory is not a directory"). Here the base is not looked at:
///   whether a directory exists is the file system's state, not the caller's mistake, and an
///   attachment below a base that is no directory is simply not found when it is read.
/// - @p name is UTF-8; a malformed byte reads as U+FFFD (a Java String has no such byte), so
///   that no name can make the conversion to a path fail.
/// - getAttachment(File) is getFileAttachment(), because a string literal converts to a path
///   and to a string view alike.
class FileSystemAttachmentFactory final : public AttachmentFactory
{
public:
    /// A factory without a base directory.
    FileSystemAttachmentFactory() = default;

    /// A factory for the files of @p baseDirectory.
    explicit FileSystemAttachmentFactory(std::filesystem::path baseDirectory);

    /// The base directory, when there is one.
    [[nodiscard]] const std::optional<std::filesystem::path>& getBaseDirectory() const noexcept
    {
        return m_baseDirectory;
    }

    /// The attachment of the file @p file, named after the file's last component
    /// (getAttachment(File): what choosing an image from disk makes).
    [[nodiscard]] static std::shared_ptr<Attachment> getFileAttachment(
        const std::filesystem::path& file);

    /// A FileSystemAttachment named @p name whose file is @p name itself when that is an
    /// absolute path, and @p name below the base directory otherwise.
    [[nodiscard]] std::shared_ptr<Attachment> getAttachment(std::string_view name) const override;

private:
    std::optional<std::filesystem::path> m_baseDirectory;
};

}  // namespace QtRocket
