#include "QtRocket/document/attachments/FileSystemAttachment.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"

namespace QtRocket
{

FileSystemAttachment::FileSystemAttachment(std::string name, std::filesystem::path location,
                                           std::size_t maxAttachmentBytes)
  : Attachment(std::move(name)),
    m_location(std::move(location)),
    m_maxAttachmentBytes(maxAttachmentBytes)
{
}

Result<std::vector<std::byte>> FileSystemAttachment::getBytes() const
{
    // ZipFileAttachment's text for an entry that declares more than the limit; a path that has
    // no size (it does not exist, or is no regular file) is readLocation()'s to refuse.
    std::error_code      noSize;
    const std::uintmax_t size = std::filesystem::file_size(m_location, noSize);
    if (!noSize && std::cmp_greater(size, m_maxAttachmentBytes))
    {
        return fail(ErrorCode::IO,
                    std::format("Attachment '{}' exceeds the maximum size of {} bytes", getName(),
                                m_maxAttachmentBytes));
    }
    return readLocation(m_location, m_maxAttachmentBytes);
}

Result<std::vector<std::byte>> FileSystemAttachment::readLocation(const std::filesystem::path& file,
                                                                  std::size_t maxBytes)
{
    // What Java's FileInputStream refuses to open is "not found"; readFile() reports all of it,
    // and a failure while reading, as ErrorCode::IO. A directory is asked for first, because
    // libstdc++ opens one.
    std::error_code                    ignored;
    const std::filesystem::file_status status = std::filesystem::status(file, ignored);
    if (std::filesystem::is_directory(status))
    {
        return fail(ErrorCode::NOT_FOUND,
                    std::format("cannot read '{}': is a directory", pathToUtf8(file)));
    }
    // Not OpenRocket's (see the class comment): a device or a pipe is not opened at all, since
    // opening one may block and reading one may never end.
    // (A file whose kind cannot be told is left to the attempt to open it.)
    if (std::filesystem::exists(status) && !std::filesystem::is_regular_file(status) &&
        status.type() != std::filesystem::file_type::unknown)
    {
        return fail(ErrorCode::IO,
                    std::format("cannot read '{}': not a regular file", pathToUtf8(file)));
    }
    if (!std::ifstream(file, std::ios::binary))
    {
        return fail(ErrorCode::NOT_FOUND,
                    std::format("cannot open '{}' for reading", pathToUtf8(file)));
    }
    return readFile(file, maxBytes);
}

}  // namespace QtRocket
