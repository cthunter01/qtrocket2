#include "QtRocket/file/ZipFileAttachment.h"

#include <cstddef>
#include <expected>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/file/ZipInputStream.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

ZipFileAttachment::ZipFileAttachment(std::string name, Archive archive,
                                     std::size_t maxAttachmentBytes)
  : Attachment(std::move(name)),
    m_archive(std::move(archive)),
    m_maxAttachmentBytes(maxAttachmentBytes)
{
    if (m_archive == nullptr)
    {
        bug("A ZipFileAttachment needs an archive");
    }
}

Result<std::vector<std::byte>> ZipFileAttachment::getBytes() const
{
    const std::string& name = getName();
    ZipInputStream     zip(*m_archive);
    while (true)
    {
        Result<std::optional<ZipInputStream::Entry>> entry = zip.nextEntry();
        if (!entry)
        {
            return std::unexpected(std::move(entry.error()));
        }
        if (!entry->has_value())
        {
            return decalNotFound(name);
        }
        if ((*entry)->name == name)
        {
            if (std::cmp_greater((*entry)->size, m_maxAttachmentBytes))
            {
                return fail(ErrorCode::IO,
                            std::format("Attachment '{}' exceeds the maximum size of {} bytes",
                                        name, m_maxAttachmentBytes));
            }
            return zip.readEntry(m_maxAttachmentBytes);
        }
    }
}

}  // namespace QtRocket
