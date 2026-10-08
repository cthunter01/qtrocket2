#include "QtRocket/file/ZipFileAttachmentFactory.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/file/ZipFileAttachment.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

ZipFileAttachmentFactory::ZipFileAttachmentFactory(ZipFileAttachment::Archive archive)
  : m_archive(std::move(archive))
{
    if (m_archive == nullptr)
    {
        bug("A ZipFileAttachmentFactory needs an archive");
    }
}

ZipFileAttachmentFactory::ZipFileAttachmentFactory(std::vector<std::byte> archive)
  : m_archive(std::make_shared<const std::vector<std::byte>>(std::move(archive)))
{
}

std::shared_ptr<Attachment> ZipFileAttachmentFactory::getAttachment(std::string_view name) const
{
    return std::make_shared<ZipFileAttachment>(std::string(name), m_archive);
}

}  // namespace QtRocket
