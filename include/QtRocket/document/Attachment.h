#pragma once

#include <cstddef>
#include <expected>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{

/// Something a document refers to by name and whose bytes live elsewhere: a decal image or an
/// embedded thrust curve, in a file next to the design or in the archive the design was loaded
/// from (OpenRocket's document/Attachment). The subclasses say where the bytes are:
/// FileSystemAttachment here, and in file/ the one that reads an archive.
///
/// Reading (Java: getBytes() returns an InputStream and throws): getBytes() returns all the
/// bytes or an Error. ErrorCode::NOT_FOUND means that the source does not exist or cannot be
/// opened, Java's DecalNotFoundException and FileNotFoundException, which the motor loader takes
/// silently and for which a decal image reports decalNotFound(); any other code means that the
/// source exists and could not be read (Java: another IOException). Since all the bytes are
/// returned at once, the attachments of this port take kMaxAttachmentBytes at most unless they
/// are made with another limit, so that no file can make a reader hold more.
///
/// Order: by name, as Java's compareTo() (String.compareTo, see Strings::javaCompareTo()).
///
/// Change notification (Java: AbstractChangeSource, with a public fireChangeEvent()): changed()
/// is emitted by fireChangeEvent() and by nothing else; OpenRocket itself never fires or hears
/// an attachment.
///
/// An attachment is not copied or moved: it is shared (std::shared_ptr) by the decal images made
/// from it.
class Attachment
{
public:
    /// The most bytes an attachment may have: 32 MiB (Java: ZipFileAttachment's
    /// MAX_ATTACHMENT_BYTES, which there bounds the entries of an archive only).
    static constexpr std::size_t kMaxAttachmentBytes = std::size_t{32} * 1024 * 1024;

    /// An attachment named @p name: the name the document refers to it by, such as
    /// "decals/logo.png".
    explicit Attachment(std::string name);
    virtual ~Attachment() = default;

    Attachment(const Attachment&)            = delete;
    Attachment& operator=(const Attachment&) = delete;
    Attachment(Attachment&&)                 = delete;
    Attachment& operator=(Attachment&&)      = delete;

    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }

    /// All the bytes of the attachment, or why they could not be read (see the class comment for
    /// the error codes).
    [[nodiscard]] virtual Result<std::vector<std::byte>> getBytes() const = 0;

    /// Java's compareTo(): negative, zero or positive as this name sorts before, with or after
    /// @p other's.
    [[nodiscard]] int compareTo(const Attachment& other) const noexcept;

    /// The name.
    [[nodiscard]] const std::string& toString() const noexcept { return m_name; }

    /// Emitted by fireChangeEvent().
    [[nodiscard]] Signal<>& changed() noexcept { return m_changed; }
    /// Tells the listeners that the attachment changed.
    void fireChangeEvent() const { m_changed.emit(); }

private:
    std::string m_name;
    Signal<>    m_changed;
};

/// The failure for a decal image or an attachment whose source is missing (Java: a
/// DecalNotFoundException, whose message is the text of "ExportDecalDialog.source.exception" with
/// the name or the path): ErrorCode::NOT_FOUND with the message
/// "Could not find decal source file '<source>'. <br> <br>Would you like to look for this file?".
/// @p source is the name of the attachment or the absolute path of the file. Deviation: Java's
/// exception also carries the decal image (getDecal()); here the caller has the image it asked.
[[nodiscard]] std::unexpected<Error> decalNotFound(
    std::string_view source, std::source_location where = std::source_location::current());

}  // namespace QtRocket
