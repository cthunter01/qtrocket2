#include "QtRocket/document/DecalImage.h"

#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/document/attachments/FileSystemAttachment.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

DecalImage::DecalImage(std::shared_ptr<const Attachment> attachment)
  : m_attachment(std::move(attachment))
{
    QTROCKET_ASSERT(m_attachment != nullptr);
}

DecalImage::DecalImage(std::string name, std::shared_ptr<const Attachment> attachment)
  : m_attachment(std::move(attachment)), m_name(std::move(name))
{
    QTROCKET_ASSERT(m_attachment != nullptr);
}

const std::string& DecalImage::getName() const noexcept
{
    return m_name.has_value() ? *m_name : m_attachment->getName();
}

Result<std::vector<std::byte>> DecalImage::getBytes() const
{
    // First check if the decal is located on the file system
    if (m_decalFile.has_value())
    {
        std::error_code ignored;
        if (!std::filesystem::exists(*m_decalFile, ignored))
        {
            return decalNotFound(pathToUtf8(absolutePath(*m_decalFile)));
        }
        return FileSystemAttachment::readLocation(*m_decalFile);
    }
    Result<std::vector<std::byte>> bytes = m_attachment->getBytes();
    if (!bytes.has_value() && bytes.error().code == ErrorCode::NOT_FOUND)
    {
        return decalNotFound(m_attachment->getName());
    }
    return bytes;
}

Result<void> DecalImage::exportImage(const std::filesystem::path& file) const
{
    const Result<std::vector<std::byte>> bytes = getBytes();
    if (!bytes.has_value())
    {
        return std::unexpected(bytes.error());
    }
    return writeFile(file, *bytes);
}

void DecalImage::setDecalFile(std::optional<std::filesystem::path> file)
{
    m_decalFile = std::move(file);
}

int DecalImage::compareTo(const DecalImage& other) const noexcept
{
    return Strings::javaCompareTo(getName(), other.getName());
}

std::shared_ptr<DecalImage> DecalImage::copyWithName(std::string name) const
{
    std::shared_ptr<DecalImage> copy = std::make_shared<DecalImage>(std::move(name), m_attachment);
    copy->m_decalFile                = m_decalFile;
    return copy;
}

}  // namespace QtRocket
