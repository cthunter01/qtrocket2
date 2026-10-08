#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace QtRocket
{

class Attachment;
class DecalImage;

/// The decal images of a document, by name (OpenRocket's document/DecalRegistry). A document has
/// one; the loader and the GUI ask it for the image of an attachment, and every image it hands
/// out is shared with it (std::shared_ptr), so an image stays valid for whoever holds it after
/// it was removed.
///
/// The image of an attachment (getDecalImage()), Java's two cases:
/// - A FileSystemAttachment is found by its file: the image that has that file as its decal
///   file, whatever the two are called. A new file gets an image named
///   makeUniqueName(the file's name) with the file as its decal file.
/// - Any other attachment is found by its name, and a new name gets an image that takes its
///   name from the attachment. The first attachment of a name keeps supplying the bytes.
///
/// Unique names (makeUniqueName()) follow Java's pattern "(.*?)( \((\d+)\)+)?\.(\w*)", matched
/// against the whole name: a base, an optional " (number)" and an extension. The name gets the
/// prefix "decals/" unless it has it. When a registered name has the same base and extension,
/// the result is base + " (n)." + extension with the smallest n from 1 that no such name has. The
/// quirks of the pattern are kept, since they decide the names in the files OpenRocket and
/// QtRocket write:
/// - The number of the name asked for is dropped: "a (7).png" beside "a.png" becomes
///   "decals/a (1).png". Leading zeros count as the number ("(007)" is 7), several closing
///   parentheses are accepted ("a (1)).png"), and the space before the parenthesis is required.
/// - A name does not match when it has no dot, when a character after its last dot is not an
///   ASCII letter, digit or underscore, or when a line terminator (\n, \r, U+0085, U+2028,
///   U+2029) comes before that dot. Such a name has neither base nor extension: it collides only
///   with itself and with the registered names of an empty base and extension, and what it then
///   becomes is " (n)." without the "decals/" prefix (a second "noext" is " (1).", and once
///   " (1)." is registered every name without a dot becomes " (2).", " (3)." and so on).
/// - A name that begins with a slash keeps it behind the prefix: "decals//textures/a.png".
///
/// Deviations from OpenRocket:
/// - The number is written with plain digits. Java formats it with MessageFormat, which groups
///   the digits of the default locale: the thousandth copy is "a (1,000).png", a name the
///   pattern no longer reads as numbered, so that the next copy gets the same name and replaces
///   it in the registry. Here the copies go on as "a (1000).png", "a (1001).png".
/// - A registered name whose number does not fit an int ("a (2147483648).png") still makes the
///   name asked for a copy, but its number is not counted; Java's Integer.parseInt throws a
///   NumberFormatException out of the registry. No int can collide with such a number.
/// - When several images have the decal file asked for (a copy made by makeUniqueImage() shares
///   its original's), the first in name order is found; Java finds the one its HashMap happens
///   to list first.
/// - Files are compared as std::filesystem::path compares them, after dropping a separator at
///   the end as Java's File does ("x/a.png" and "x//a.png" are the same file, "x/./a.png" is
///   another one, as in Java). Java's File.equals() ignores case on Windows; this does not.
/// - getDecalList() returns the images in a vector sorted by name (Java: a TreeSet over
///   DecalImage.compareTo()).
/// - makeUniqueName() is public (Java: private) and find(), size() and empty() are additions:
///   a rocket/Decal holds the name of its image, not the image.
/// - removeDecal() is public (Java: package-private, called by the document).
/// - A null attachment is a BugError (Java: NullPointerException).
/// - Not copyable or movable: a copy would share the images of another document.
class DecalRegistry
{
public:
    DecalRegistry()                                = default;
    ~DecalRegistry()                               = default;
    DecalRegistry(const DecalRegistry&)            = delete;
    DecalRegistry& operator=(const DecalRegistry&) = delete;
    DecalRegistry(DecalRegistry&&)                 = delete;
    DecalRegistry& operator=(DecalRegistry&&)      = delete;

    /// The image of @p attachment, registered now when it is new (see the class comment).
    /// @throws BugError when @p attachment is null
    [[nodiscard]] std::shared_ptr<DecalImage> getDecalImage(
        const std::shared_ptr<const Attachment>& attachment);

    /// An image like @p original under a name no registered image has: a copy with the same
    /// attachment and decal file, registered as makeUniqueName(original's name). When that is
    /// the name @p original has (it is not registered here and collides with nothing),
    /// @p original itself is returned and nothing is registered. Null gives null. As in Java, an
    /// image registered under a name without the "decals/" prefix gets a copy with the prefix
    /// first ("plain.png" gives "decals/plain.png", then "decals/plain (1).png").
    [[nodiscard]] std::shared_ptr<DecalImage> makeUniqueImage(
        const std::shared_ptr<DecalImage>& original);

    /// @p name with the "decals/" prefix, renamed when it collides with a registered name (see
    /// the class comment). Registers nothing.
    [[nodiscard]] std::string makeUniqueName(std::string_view name) const;

    /// The registered images, sorted by name (Strings::javaCompareTo()).
    [[nodiscard]] std::vector<std::shared_ptr<DecalImage>> getDecalList() const;

    /// Removes the image registered under the name of @p decal; true when there was one. As in
    /// Java the name decides, not the object, and null gives false.
    bool removeDecal(const DecalImage* decal);

    /// The image registered under exactly @p name, or null.
    [[nodiscard]] std::shared_ptr<DecalImage> find(std::string_view name) const;

    /// The number of registered images.
    [[nodiscard]] std::size_t size() const noexcept { return m_registeredDecals.size(); }
    [[nodiscard]] bool        empty() const noexcept { return m_registeredDecals.empty(); }

private:
    /// The image whose decal file is @p file, or null (Java: findDecalForFile()).
    [[nodiscard]] std::shared_ptr<DecalImage> findDecalForFile(
        const std::filesystem::path& file) const;

    /// The images by name; the key is the image's getName().
    std::map<std::string, std::shared_ptr<DecalImage>, std::less<>> m_registeredDecals;
};

}  // namespace QtRocket
