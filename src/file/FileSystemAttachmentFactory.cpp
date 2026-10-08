#include "QtRocket/file/FileSystemAttachmentFactory.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/document/attachments/FileSystemAttachment.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

FileSystemAttachmentFactory::FileSystemAttachmentFactory(std::filesystem::path baseDirectory)
  : m_baseDirectory(std::move(baseDirectory))
{
}

std::shared_ptr<Attachment> FileSystemAttachmentFactory::getFileAttachment(
    const std::filesystem::path& file)
{
    return std::make_shared<FileSystemAttachment>(pathToUtf8(file.filename()), file);
}

std::shared_ptr<Attachment> FileSystemAttachmentFactory::getAttachment(std::string_view name) const
{
    // Through char8_t, so that the name is read as UTF-8 on every platform; made valid first,
    // because the conversion to the platform's path may refuse a malformed sequence.
    const std::string     valid = Strings::toValidUtf8(name);
    std::filesystem::path file(std::u8string(valid.begin(), valid.end()));
    if (!file.is_absolute() && m_baseDirectory.has_value())
    {
        // Java's File(parent, child) appends the child whatever it starts with. On Windows a
        // name such as "/datafiles/x.jpg" is not absolute (it names no drive), and operator/
        // alone would let its root replace the base directory's.
        file = *m_baseDirectory / file.relative_path();
    }
    return std::make_shared<FileSystemAttachment>(std::string(name), std::move(file));
}

}  // namespace QtRocket
