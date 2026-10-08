#include "QtRocket/document/attachments/FileSystemAttachment.h"

#include <cstddef>
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

FileSystemAttachment::FileSystemAttachment(std::string name, std::filesystem::path location)
  : Attachment(std::move(name)), m_location(std::move(location))
{
}

Result<std::vector<std::byte>> FileSystemAttachment::getBytes() const
{
    return readLocation(m_location);
}

Result<std::vector<std::byte>> FileSystemAttachment::readLocation(const std::filesystem::path& file)
{
    // What Java's FileInputStream refuses to open is "not found"; readFile() reports all of it,
    // and a failure while reading, as ErrorCode::IO. A directory is asked for first, because
    // libstdc++ opens one.
    std::error_code ignored;
    if (std::filesystem::is_directory(file, ignored))
    {
        return fail(ErrorCode::NOT_FOUND,
                    std::format("cannot read '{}': is a directory", pathToUtf8(file)));
    }
    if (!std::ifstream(file, std::ios::binary))
    {
        return fail(ErrorCode::NOT_FOUND,
                    std::format("cannot open '{}' for reading", pathToUtf8(file)));
    }
    return readFile(file);
}

}  // namespace QtRocket
