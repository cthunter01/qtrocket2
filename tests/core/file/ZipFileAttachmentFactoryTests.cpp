#include "QtRocket/file/ZipFileAttachmentFactory.h"

#include <cstddef>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/file/AttachmentFactory.h"
#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/file/ZipFileAttachment.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"

namespace
{

using QtRocket::Attachment;
using QtRocket::AttachmentFactory;
using QtRocket::BugError;
using QtRocket::ErrorCode;
using QtRocket::ZipFileAttachment;
using QtRocket::ZipFileAttachmentFactory;

[[nodiscard]] std::vector<std::byte> twoEntries()
{
    QtRocket::ZipWriter writer;
    writer.add("rocket.ork", QtRocket::stringToBytes("<openrocket/>"));
    writer.add("decals/a.png", QtRocket::stringToBytes("A"));
    return writer.finish().value();
}

TEST(ZipFileAttachmentFactory, MakesAttachmentsOfItsArchive)
{
    const ZipFileAttachmentFactory    factory(twoEntries());
    const std::shared_ptr<Attachment> attachment = factory.getAttachment("decals/a.png");
    ASSERT_NE(attachment, nullptr);
    EXPECT_EQ(attachment->getName(), "decals/a.png");
    EXPECT_EQ(QtRocket::bytesToString(attachment->getBytes().value()), "A");

    // The attachment is a ZipFileAttachment with the default limit.
    const auto* zip = dynamic_cast<const ZipFileAttachment*>(attachment.get());
    ASSERT_NE(zip, nullptr);
    EXPECT_EQ(zip->getMaxAttachmentBytes(), ZipFileAttachment::kMaxAttachmentBytes);
}

TEST(ZipFileAttachmentFactory, AnAttachmentIsMadeWhetherItsEntryExistsOrNot)
{
    const ZipFileAttachmentFactory    factory(twoEntries());
    const std::shared_ptr<Attachment> missing = factory.getAttachment("thrustcurves/abc.rse");
    ASSERT_NE(missing, nullptr);
    EXPECT_EQ(missing->getName(), "thrustcurves/abc.rse");
    ASSERT_FALSE(missing->getBytes().has_value());
    EXPECT_EQ(missing->getBytes().error().code, ErrorCode::NOT_FOUND);
}

TEST(ZipFileAttachmentFactory, EveryCallMakesANewAttachment)
{
    const ZipFileAttachmentFactory factory(twoEntries());
    EXPECT_NE(factory.getAttachment("decals/a.png"), factory.getAttachment("decals/a.png"));
}

TEST(ZipFileAttachmentFactory, TheAttachmentsKeepTheArchiveAlive)
{
    std::shared_ptr<Attachment> attachment;
    {
        const ZipFileAttachment::Archive archive =
            std::make_shared<const std::vector<std::byte>>(twoEntries());
        const ZipFileAttachmentFactory factory(archive);
        EXPECT_EQ(factory.getArchive(), archive);
        attachment = factory.getAttachment("rocket.ork");
        EXPECT_EQ(archive.use_count(), 3);
    }
    EXPECT_EQ(QtRocket::bytesToString(attachment->getBytes().value()), "<openrocket/>");
}

TEST(ZipFileAttachmentFactory, IsAnAttachmentFactory)
{
    const ZipFileAttachmentFactory factory(twoEntries());
    const AttachmentFactory&       base = factory;
    EXPECT_EQ(QtRocket::bytesToString(base.getAttachment("decals/a.png")->getBytes().value()), "A");
}

TEST(ZipFileAttachmentFactory, ANullArchiveIsABug)
{
    EXPECT_THROW(ZipFileAttachmentFactory(ZipFileAttachment::Archive{}), BugError);
}

}  // namespace
