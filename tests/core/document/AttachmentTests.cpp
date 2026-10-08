#include "QtRocket/document/Attachment.h"

#include <array>
#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "document/TestAttachments.h"
#include "simulation/SimulationOptionsSupport.h"

// OpenRocket has no test of Attachment. The Java values are those of the probe DecalProbe.java,
// sections G and H (probes/tier9a-document-d2).

namespace
{

using QtRocket::Attachment;
using QtRocket::Error;
using QtRocket::ErrorCode;
using QtRocket::Result;
using QtRocket::Test::ChangeCounter;
using QtRocket::Test::FailingAttachment;
using QtRocket::Test::MemoryAttachment;

// An attachment is shared, never copied, and only its subclasses can be made.
static_assert(std::is_abstract_v<Attachment>);
static_assert(std::has_virtual_destructor_v<Attachment>);
static_assert(!std::is_copy_constructible_v<MemoryAttachment>);
static_assert(!std::is_move_constructible_v<MemoryAttachment>);
static_assert(!std::is_copy_assignable_v<MemoryAttachment>);
static_assert(!std::is_move_assignable_v<MemoryAttachment>);

/// The names the Java probe compared: 'a' 'b' 'B' '' 'ab' e-acute U+1F600 U+FFEE.
constexpr std::array<const char*, 8> kNames{
    "a", "b", "B", "", "ab", "\xC3\xA9", "\xF0\x9F\x98\x80", "\xEF\xBF\xAE"};

/// compareTo() of an attachment of every name with an attachment of every name, row by row.
[[nodiscard]] std::vector<int> compareMatrix()
{
    std::vector<int> matrix;
    for (const char* const left : kNames)
    {
        for (const char* const right : kNames)
        {
            matrix.push_back(
                MemoryAttachment(left, "x").compareTo(FailingAttachment(std::string(right))));
        }
    }
    return matrix;
}

TEST(Attachment, HasTheNameItWasGiven)
{
    // Java: toString=the name getName=the name
    const MemoryAttachment attachment("the name", "bytes");
    EXPECT_EQ(attachment.getName(), "the name");
    EXPECT_EQ(attachment.toString(), "the name");

    // Any text is a name: an archive entry with a leading slash, nothing at all.
    EXPECT_EQ(MemoryAttachment("/datafiles/textures/balsa.jpg", "").getName(),
              "/datafiles/textures/balsa.jpg");
    EXPECT_EQ(MemoryAttachment("", "").getName(), "");
}

TEST(Attachment, IsOrderedByNameAsJavaStringsAre)
{
    // Java's compareTo(), String.compareTo of the names: the difference of the first differing
    // UTF-16 code units, else of the lengths. U+1F600 is a surrogate pair (D83D DE00) and so
    // sorts before U+FFEE, where the UTF-8 bytes would order it after.
    const std::vector<int> expected{
        0,     -1,    31,    1, -1,    -136,  -55260, -65421,  // 'a'
        1,     0,     32,    1, 1,     -135,  -55259, -65420,  // 'b'
        -31,   -32,   0,     1, -31,   -167,  -55291, -65452,  // 'B'
        -1,    -1,    -1,    0, -2,    -1,    -2,     -1,      // ''
        1,     -1,    31,    2, 0,     -136,  -55260, -65421,  // 'ab'
        136,   135,   167,   1, 136,   0,     -55124, -65285,  // e-acute
        55260, 55259, 55291, 2, 55260, 55124, 0,      -10161,  // U+1F600
        65421, 65420, 65452, 1, 65421, 65285, 10161,  0};      // U+FFEE
    EXPECT_EQ(compareMatrix(), expected);
}

TEST(Attachment, TheOrderLooksAtTheNameOnly)
{
    // "considers only the name": two attachments of one name are equal in the order whatever
    // their kind and their bytes.
    const MemoryAttachment  memory("decals/a.png", "one");
    const MemoryAttachment  other("decals/a.png", "two");
    const FailingAttachment failing("decals/a.png");
    EXPECT_EQ(memory.compareTo(other), 0);
    EXPECT_EQ(memory.compareTo(failing), 0);
    EXPECT_EQ(failing.compareTo(memory), 0);
    EXPECT_EQ(memory.compareTo(memory), 0);
}

TEST(Attachment, GetBytesIsTheSubclasss)
{
    const std::unique_ptr<Attachment> memory =
        std::make_unique<MemoryAttachment>("decals/a.png", "MEM");
    const Result<std::vector<std::byte>> bytes = memory->getBytes();
    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(QtRocket::bytesToString(*bytes), "MEM");

    const std::unique_ptr<Attachment> failing = std::make_unique<FailingAttachment>(
        "decals/b.png", ErrorCode::PARSE, "the archive is damaged");
    const Result<std::vector<std::byte>> none = failing->getBytes();
    ASSERT_FALSE(none.has_value());
    EXPECT_EQ(none.error().code, ErrorCode::PARSE);
    EXPECT_EQ(none.error().message, "the archive is damaged");
}

TEST(Attachment, FireChangeEventReachesTheListeners)
{
    // Java: "fireChangeEvent heard=1" (AbstractChangeSource).
    MemoryAttachment    attachment("the name", "");
    const ChangeCounter heard(attachment.changed());
    EXPECT_EQ(heard.count(), 0);
    attachment.fireChangeEvent();
    EXPECT_EQ(heard.count(), 1);
    attachment.fireChangeEvent();
    EXPECT_EQ(heard.count(), 2);

    // Reading is no change.
    EXPECT_TRUE(attachment.getBytes().has_value());
    EXPECT_EQ(heard.count(), 2);
}

TEST(Attachment, AConstAttachmentCanFire)
{
    // The decal images hold their attachment as const; whoever made it can still listen.
    MemoryAttachment    attachment("the name", "");
    const ChangeCounter heard(attachment.changed());
    const Attachment&   view = attachment;
    view.fireChangeEvent();
    EXPECT_EQ(heard.count(), 1);
}

TEST(Attachment, DecalNotFoundIsJavasMessage)
{
    // Java, DecalNotFoundException.getMessage(): "Could not find decal source file
    // 'decals/gone.png'. <br> <br>Would you like to look for this file?"
    const std::unexpected<Error> failure = QtRocket::decalNotFound("decals/gone.png");
    EXPECT_EQ(failure.error().code, ErrorCode::NOT_FOUND);
    EXPECT_EQ(failure.error().message,
              "Could not find decal source file 'decals/gone.png'. <br> "
              "<br>Would you like to look for this file?");

    // The name goes in as it is: MessageFormat does not read an argument as a pattern.
    EXPECT_EQ(QtRocket::decalNotFound("a {0} 'b'").error().message,
              "Could not find decal source file 'a {0} 'b''. <br> <br>Would you like to look for "
              "this file?");
    EXPECT_EQ(QtRocket::decalNotFound("").error().message,
              "Could not find decal source file ''. <br> <br>Would you like to look for this "
              "file?");

    // It converts to the failure of any Result.
    const Result<std::vector<std::byte>> bytes = QtRocket::decalNotFound("x");
    ASSERT_FALSE(bytes.has_value());
    EXPECT_EQ(bytes.error().code, ErrorCode::NOT_FOUND);
    const Result<void> nothing = QtRocket::decalNotFound("x");
    EXPECT_FALSE(nothing.has_value());
}

}  // namespace
