#include "QtRocket/file/FileSystemAttachmentFactory.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/document/attachments/FileSystemAttachment.h"
#include "QtRocket/file/AttachmentFactory.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestTempDir.h"

namespace
{

using QtRocket::Attachment;
using QtRocket::AttachmentFactory;
using QtRocket::ErrorCode;
using QtRocket::FileSystemAttachment;
using QtRocket::FileSystemAttachmentFactory;
using QtRocket::Test::TempDir;

/// The file of the attachment @p attachment, which is a FileSystemAttachment.
[[nodiscard]] std::filesystem::path locationOf(const std::shared_ptr<Attachment>& attachment)
{
    const auto* file = dynamic_cast<const FileSystemAttachment*>(attachment.get());
    return file != nullptr ? file->getLocation() : std::filesystem::path{"not a file attachment"};
}

TEST(FileSystemAttachmentFactory, ResolvesARelativeNameAgainstTheBaseDirectory)
{
    const TempDir temp;
    static_cast<void>(temp.write("decals/a.png", "image"));
    const FileSystemAttachmentFactory factory(temp.path());
    EXPECT_EQ(factory.getBaseDirectory(), std::optional<std::filesystem::path>(temp.path()));

    const std::shared_ptr<Attachment> attachment = factory.getAttachment("decals/a.png");
    ASSERT_NE(attachment, nullptr);
    // The attachment keeps the name it was asked by.
    EXPECT_EQ(attachment->getName(), "decals/a.png");
    EXPECT_EQ(locationOf(attachment), temp.path() / "decals" / "a.png");
    EXPECT_EQ(QtRocket::bytesToString(attachment->getBytes().value()), "image");
}

TEST(FileSystemAttachmentFactory, TakesAnAbsoluteNameAsItIs)
{
    const TempDir                     base;
    const TempDir                     elsewhere;
    const std::filesystem::path       file = elsewhere.write("b.png", "other");
    const FileSystemAttachmentFactory factory(base.path());

    const std::string                 name       = QtRocket::pathToUtf8(file);
    const std::shared_ptr<Attachment> attachment = factory.getAttachment(name);
    EXPECT_EQ(attachment->getName(), name);
    EXPECT_EQ(locationOf(attachment), file);
    EXPECT_EQ(QtRocket::bytesToString(attachment->getBytes().value()), "other");
}

TEST(FileSystemAttachmentFactory, ChecksNothingOfTheName)
{
    // As Java's: a name may lead out of the base directory. Who must not follow it checks first.
    const TempDir temp;
    static_cast<void>(temp.write("outside.txt", "outside"));
    static_cast<void>(temp.write("design/inside.txt", "inside"));
    const FileSystemAttachmentFactory factory(temp.path() / "design");

    const std::shared_ptr<Attachment> attachment = factory.getAttachment("../outside.txt");
    EXPECT_EQ(attachment->getName(), "../outside.txt");
    EXPECT_EQ(locationOf(attachment), temp.path() / "design" / ".." / "outside.txt");
    EXPECT_EQ(QtRocket::bytesToString(attachment->getBytes().value()), "outside");
}

// Java's File(parent, child) and File(name) normalise what they are given: separators that are
// doubled or stand at the end are not part of the path. So a decal named "decals/a.png/" is the
// file "decals/a.png", which OpenRocket reads (the probe of the reviewer of tier 9c, lens
// fidelity: out/sniff_rv3.java.out, "plain decal [decals/a.png/]" gives "decals/a.png=5").
// The name of the attachment stays what was asked for.
TEST(FileSystemAttachmentFactory, SpellsTheFileWithoutRedundantSeparators)
{
    const TempDir                     temp;
    const std::filesystem::path       image = temp.write("decals/a.png", "image");
    const FileSystemAttachmentFactory factory(temp.path());

    const std::shared_ptr<Attachment> atTheEnd = factory.getAttachment("decals/a.png/");
    EXPECT_EQ(atTheEnd->getName(), "decals/a.png/");
    EXPECT_EQ(locationOf(atTheEnd), temp.path() / "decals" / "a.png");
    EXPECT_EQ(QtRocket::bytesToString(atTheEnd->getBytes().value()), "image");

    const std::shared_ptr<Attachment> doubled = factory.getAttachment("decals//a.png");
    EXPECT_EQ(doubled->getName(), "decals//a.png");
    EXPECT_EQ(locationOf(doubled), temp.path() / "decals" / "a.png");
    EXPECT_EQ(QtRocket::bytesToString(doubled->getBytes().value()), "image");

    // An absolute name, and a name without a base directory, are spelled the same way.
    const std::string                 absolute     = QtRocket::pathToUtf8(image) + "/";
    const std::shared_ptr<Attachment> absoluteFile = factory.getAttachment(absolute);
    EXPECT_EQ(absoluteFile->getName(), absolute);
    EXPECT_EQ(locationOf(absoluteFile), QtRocket::withoutRedundantSeparators(image));
    EXPECT_EQ(QtRocket::bytesToString(absoluteFile->getBytes().value()), "image");
    const FileSystemAttachmentFactory noBase;
    EXPECT_EQ(locationOf(noBase.getAttachment("decals//a.png/")),
              std::filesystem::path("decals") / "a.png");

    // Nothing else of a name is changed: "." and ".." stay, and so does the case of a letter.
    EXPECT_EQ(locationOf(factory.getAttachment("./decals/../decals/a.png")),
              temp.path() / "." / "decals" / ".." / "decals" / "a.png");
    EXPECT_EQ(QtRocket::bytesToString(
                  factory.getAttachment("./decals/../decals/a.png")->getBytes().value()),
              "image");
}

TEST(FileSystemAttachmentFactory, AMissingFileIsFoundOutWhenItIsRead)
{
    const TempDir                     temp;
    const FileSystemAttachmentFactory factory(temp.path());
    const std::shared_ptr<Attachment> attachment = factory.getAttachment("thrustcurves/abc.rse");
    ASSERT_NE(attachment, nullptr);
    ASSERT_FALSE(attachment->getBytes().has_value());
    EXPECT_EQ(attachment->getBytes().error().code, ErrorCode::NOT_FOUND);
}

TEST(FileSystemAttachmentFactory, ABaseThatIsNoDirectoryFindsNothing)
{
    // Java refuses such a base in the constructor; here it is the file system's state, and the
    // attachment is missing when it is read.
    const TempDir                     temp;
    const std::filesystem::path       file = temp.write("design.ork", "<openrocket/>");
    const FileSystemAttachmentFactory aFile(file);
    EXPECT_EQ(aFile.getAttachment("decals/a.png")->getBytes().error().code, ErrorCode::NOT_FOUND);
    const FileSystemAttachmentFactory nowhere(temp.path() / "no such directory");
    EXPECT_EQ(nowhere.getAttachment("a.png")->getBytes().error().code, ErrorCode::NOT_FOUND);
}

TEST(FileSystemAttachmentFactory, WithoutABaseARelativeNameStaysRelative)
{
    const FileSystemAttachmentFactory factory;
    EXPECT_FALSE(factory.getBaseDirectory().has_value());
    const std::shared_ptr<Attachment> attachment = factory.getAttachment("decals/a.png");
    EXPECT_EQ(attachment->getName(), "decals/a.png");
    EXPECT_EQ(locationOf(attachment), std::filesystem::path("decals") / "a.png");
}

TEST(FileSystemAttachmentFactory, ReadsANameAsUtf8)
{
    const TempDir temp;
    // U+00E9 and U+20AC, written as their UTF-8 bytes.
    const std::string           name = "caf\xC3\xA9 \xE2\x82\xAC.png";
    const std::filesystem::path file =
        temp.path() / std::filesystem::path(std::u8string(name.begin(), name.end()));
    ASSERT_TRUE(QtRocket::writeTextFile(file, "accents").has_value());
    const FileSystemAttachmentFactory factory(temp.path());
    const std::shared_ptr<Attachment> attachment = factory.getAttachment(name);
    EXPECT_EQ(attachment->getName(), name);
    EXPECT_EQ(locationOf(attachment), file);
    EXPECT_EQ(QtRocket::bytesToString(attachment->getBytes().value()), "accents");
}

TEST(FileSystemAttachmentFactory, AMalformedNameMakesAnAttachmentThatIsMissing)
{
    // No name can make the factory fail: a byte that is no UTF-8 reads as U+FFFD in the path,
    // and the attachment keeps the name as it was given.
    const TempDir                     temp;
    const FileSystemAttachmentFactory factory(temp.path());
    const std::string                 name       = "decals/\xFF\xFE.png";
    const std::shared_ptr<Attachment> attachment = factory.getAttachment(name);
    ASSERT_NE(attachment, nullptr);
    EXPECT_EQ(attachment->getName(), name);
    EXPECT_EQ(attachment->getBytes().error().code, ErrorCode::NOT_FOUND);
    EXPECT_EQ(factory.getAttachment("")->getName(), "");
}

TEST(FileSystemAttachmentFactory, MakesTheAttachmentOfAFile)
{
    // getAttachment(File): named after the file's last component.
    const TempDir                     temp;
    const std::filesystem::path       file = temp.write("images/logo.png", "logo");
    const std::shared_ptr<Attachment> attachment =
        FileSystemAttachmentFactory::getFileAttachment(file);
    EXPECT_EQ(attachment->getName(), "logo.png");
    EXPECT_EQ(locationOf(attachment), file);
    EXPECT_EQ(QtRocket::bytesToString(attachment->getBytes().value()), "logo");
}

TEST(FileSystemAttachmentFactory, IsAnAttachmentFactoryThatMakesANewAttachmentEveryTime)
{
    const TempDir temp;
    static_cast<void>(temp.write("a.png", "A"));
    const FileSystemAttachmentFactory factory(temp.path());
    const AttachmentFactory&          base = factory;
    EXPECT_EQ(QtRocket::bytesToString(base.getAttachment("a.png")->getBytes().value()), "A");
    EXPECT_NE(base.getAttachment("a.png"), base.getAttachment("a.png"));
}

}  // namespace
