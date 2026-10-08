#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{

class Attachment;

/// The image of a decal: a name and where its bytes come from (OpenRocket's interface
/// appearance/DecalImage and its document implementation DecalRegistry.DecalImageImpl, in one
/// class). The images of a document are made and kept by its DecalRegistry; a rocket/Decal names
/// its image (Decal::getImageName() is getName()), and DecalRegistry::find() gives the image of
/// a name.
///
/// The name: the image's own when it has one (the registry gives one to an image made from a
/// file, and to a copy), else its attachment's. It never changes once the image exists.
///
/// The bytes (getBytes()): from the decal file when one is set, else from the attachment. A decal
/// file is the file an image was read from or exported to for editing; it is asked first, and
/// when it does not exist the failure is decalNotFound() with the file's absolute path (Java:
/// File.getAbsolutePath(), here std::filesystem::absolute()), even if the attachment could
/// supply the bytes. A decal file that exists is read as FileSystemAttachment::readLocation()
/// reads a file, with that function's errors (a directory is NOT_FOUND with its own message,
/// as it is a FileNotFoundException and no DecalNotFoundException in Java). An attachment that
/// reports ErrorCode::NOT_FOUND gives decalNotFound() with the attachment's name; any other
/// Error is passed on unchanged.
///
/// Change notification (Java: ChangeSource): changed() is emitted by fireChangeEvent() and by
/// nothing else, no setter included; OpenRocket fires it after an external editor rewrote the
/// decal file. Every image has its own signal: a copy does not share the listeners.
///
/// Deviations from OpenRocket:
/// - Java's other implementations of the interface (ResourceDecalImage and FileDecalImage of
///   appearance/defaults, the images of the built-in default appearances) are not ported: they
///   belong to the GUI.
/// - The constructors are public (Java: private to the registry), and a null attachment is a
///   BugError there (Java: a NullPointerException at the first use).
/// - getBytes() returns the bytes (Java: an InputStream) and a Result where Java throws:
///   decalNotFound() for DecalNotFoundException, without the image Java's exception carries.
///   Java catches only the DecalNotFoundException of the attachment; here every NOT_FOUND of the
///   attachment is renamed, also the one a FileSystemAttachment gives for a missing file (Java:
///   a FileNotFoundException with the path), which an image reaches only when its decal file
///   was cleared.
/// - exportImage() reads all the bytes before it opens the target; Java does so only for an
///   image with a decal file, and would empty a FileSystemAttachment's own file when asked to
///   export onto it.
/// - fireChangeEvent() takes no source: the slots of changed() take no argument.
/// - copyWithName() is Java's clone() followed by the registry setting the name, in one step.
/// - Not copyable or movable: an image is shared (std::shared_ptr) and heard by its listeners.
class DecalImage
{
public:
    /// An image that takes its name from @p attachment (Java: DecalImageImpl(Attachment)).
    /// @throws BugError when @p attachment is null
    explicit DecalImage(std::shared_ptr<const Attachment> attachment);

    /// An image named @p name (Java: DecalImageImpl(String, Attachment)).
    /// @throws BugError when @p attachment is null
    DecalImage(std::string name, std::shared_ptr<const Attachment> attachment);

    ~DecalImage()                            = default;
    DecalImage(const DecalImage&)            = delete;
    DecalImage& operator=(const DecalImage&) = delete;
    DecalImage(DecalImage&&)                 = delete;
    DecalImage& operator=(DecalImage&&)      = delete;

    /// The image's own name, or else its attachment's.
    [[nodiscard]] const std::string& getName() const noexcept;

    /// All the bytes of the image (see the class comment for where they come from and for the
    /// errors).
    [[nodiscard]] Result<std::vector<std::byte>> getBytes() const;

    /// Writes the bytes of the image to @p file, replacing it. Fails with getBytes()'s Error,
    /// before the file is touched, or with writeFile()'s.
    [[nodiscard]] Result<void> exportImage(const std::filesystem::path& file) const;

    /// The decal file, or nullopt (Java: null).
    [[nodiscard]] const std::optional<std::filesystem::path>& getDecalFile() const noexcept
    {
        return m_decalFile;
    }
    /// Sets the file the bytes are read from; nullopt goes back to the attachment. Emits nothing.
    void setDecalFile(std::optional<std::filesystem::path> file);

    /// Whether the saver leaves this image out of the next archive it writes, clearing the flag
    /// as it does. OpenRocket sets it when the source of the image is missing and the user
    /// declines to look for it.
    [[nodiscard]] bool isIgnored() const noexcept { return m_ignored; }
    void               setIgnored(bool ignored) noexcept { m_ignored = ignored; }

    /// The name.
    [[nodiscard]] const std::string& toString() const noexcept { return getName(); }

    /// Java's compareTo(): by name (String.compareTo, see Strings::javaCompareTo()).
    [[nodiscard]] int compareTo(const DecalImage& other) const noexcept;

    /// A new image named @p name with this image's attachment and decal file; it is not ignored
    /// and has no listeners (Java: clone(), then the registry sets the name).
    [[nodiscard]] std::shared_ptr<DecalImage> copyWithName(std::string name) const;

    /// Emitted by fireChangeEvent().
    [[nodiscard]] Signal<>& changed() noexcept { return m_changed; }
    /// Tells the listeners that the image changed (Java: fireChangeEvent(source)).
    void fireChangeEvent() const { m_changed.emit(); }

private:
    std::shared_ptr<const Attachment>    m_attachment;
    std::optional<std::string>           m_name;
    std::optional<std::filesystem::path> m_decalFile;
    /// Whether this image is ignored when the document is saved.
    bool     m_ignored{false};
    Signal<> m_changed;
};

}  // namespace QtRocket
