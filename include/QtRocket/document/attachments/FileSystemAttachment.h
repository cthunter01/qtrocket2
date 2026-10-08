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
class FileSystemAttachment final : public Attachment
{
public:
    /// An attachment named @p name whose bytes are the file @p location. Nothing is checked
    /// here: the file need not exist.
    FileSystemAttachment(std::string name, std::filesystem::path location);

    /// Where the file is.
    [[nodiscard]] const std::filesystem::path& getLocation() const noexcept { return m_location; }

    /// The bytes of the file, as readLocation(getLocation()).
    [[nodiscard]] Result<std::vector<std::byte>> getBytes() const override;

    /// Reads the file @p file as an attachment's source: ErrorCode::NOT_FOUND when it does not
    /// exist, is a directory or cannot be opened, ErrorCode::IO when reading it fails (see the
    /// class comment). A decal image reads its decal file with it.
    [[nodiscard]] static Result<std::vector<std::byte>> readLocation(
        const std::filesystem::path& file);

private:
    std::filesystem::path m_location;
};

}  // namespace QtRocket
