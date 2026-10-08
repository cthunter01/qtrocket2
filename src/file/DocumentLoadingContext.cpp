#include "QtRocket/file/DocumentLoadingContext.h"

#include "QtRocket/file/AttachmentFactory.h"
#include "QtRocket/file/FileSystemAttachmentFactory.h"

namespace QtRocket
{

const AttachmentFactory* DocumentLoadingContext::getAttachmentFactory() const noexcept
{
    if (m_attachmentFactory != nullptr)
    {
        return m_attachmentFactory;
    }
    // Java: new FileSystemAttachmentFactory(), the initial value of the field. It has no state,
    // so one serves every context.
    static const FileSystemAttachmentFactory kDefaultFactory;
    return &kDefaultFactory;
}

}  // namespace QtRocket
