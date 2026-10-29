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
/// Removed images: removeDecal() takes an image out of the list, but the registry keeps it, apart
/// from the registered ones, for as long as it lives, and its name stays taken. A rocket/Decal
/// holds the name of its image, and a name that was once handed out may still stand in the
/// rocket, or come back into it with an undo (the appearances a removal clears are an ordinary
/// undo step), so a name must go on meaning the image it was given to:
/// - find() gives a removed image too; getDecalList(), size(), empty() and isRegistered() tell of
///   the registered ones only.
/// - makeUniqueName() and makeUniqueImage() treat the name of a removed image as taken, so a
///   number that a removed copy had is not given out again.
/// - getDecalImage() registers a removed image again when it is asked for the same source: the
///   file that is its decal file, or, for an attachment that is no file, its name. Another file
///   of the same file name gets a new image under a new name ("a.png" of another directory
///   becomes "decals/a (1).png" while "decals/a.png" is a removed image).
///
/// Images of another document (importImage(), for components carried here from another
/// document: OpenRocketDocument::importDecalImage()). Java's registry has no such operation: a
/// pasted component holds the other document's image object, which its saver writes although
/// the registry does not list it (see the deviations). Here a component names its image, so the
/// image has to come into this registry, and the components are made to name it as it is
/// called here. The image this registry uses for an imported image @p image, of name N, is the
/// first of:
/// 1. the image named N here, registered or removed, when it has the same source as @p image
///    (DecalImage::hasSameSource()): it is the same image, such as the original of a copy taken
///    from this document, or an earlier import of the same image;
/// 2. any other image here of the same source, under whatever name (the first registered one
///    in name order, else the first removed one): an earlier import that had to be renamed, the
///    original of an image that comes back from another document under another name (a round
///    trip ends with this registry's own image, not a numbered copy of it), and also a copy
///    that makeUniqueImage() made, which has its original's source until it is given a decal
///    file of its own (see the deviations);
/// 3. the image named N here when the bytes of both can be read and are equal: the same
///    picture, such as one of OpenRocket's built-in textures that every example design stores
///    under its name ("/datafiles/textures/balsa.jpg"). Shared rather than copied, so that
///    designs exchanging components do not collect numbered copies of one picture; an image
///    whose bytes cannot be read is not the same as any other;
/// 4. a copy of @p image (DecalImage::copyWithName()), registered under N when no image here,
///    registered or removed, has that name, else under makeUniqueName(N), OpenRocket's rule for
///    a name that is taken (the rule makeUniqueImage() follows: "decals/a.png" beside another
///    "decals/a.png" becomes "decals/a (1).png", "/datafiles/textures/balsa.jpg" becomes
///    "decals//datafiles/textures/balsa.jpg"). A name that is free is kept as it is, without
///    the prefix makeUniqueName() would give it: it is the name the other document's file
///    holds the image under.
/// A removed image that rule 1, 2 or 3 picks is registered again: a component is to use it. The
/// copy shares the attachment and so the bytes of @p image (an archive's bytes stay with the
/// attachments that read them), and never the image object: the images of two registries
/// differ in their flags, their decal files and their listeners.
///
/// Deviations from OpenRocket:
/// - Java's registry forgets a removed image, while the components that an undo brings back
///   still hold the object, so the texture and its bytes come back with the undo and Java's
///   saver, which collects the images from the components, writes them. The name is then free:
///   asked for an attachment of that name, or for the same file, Java makes a second object
///   under the same name, which the saver's set of images, sorted by name, collapses again.
///   Here a name means one image (see "Removed images"): the image comes back from find(), is
///   registered again for the same source, and no other image gets its name or, for a copy, its
///   number. Removed images are never dropped, so the names they hold stay taken for the life of
///   the registry.
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
/// - makeUniqueName() is public (Java: private) and find(), isRegistered(), size() and empty()
///   are additions: a rocket/Decal holds the name of its image, not the image.
/// - hasDecalImage() is an addition, for a caller that bounds the images of a document (the
///   .ork loader): whether getDecalImage() would make a new image.
/// - removeDecal() is public (Java: package-private, called by the document), and so is
///   registerAgain(), an addition, for the undo of imported images (OpenRocketDocument).
/// - importImage() is an addition (see "Images of another document"). In Java a component
///   pasted from another document keeps that document's image object: the image is never
///   registered here, so the decal list and the appearance editor do not show it, and when an
///   image of this document has the same name (two designs that both name
///   "/datafiles/textures/balsa.jpg", one with other bytes), the saver's set of images, sorted
///   by name, writes one of the two and the components of the other show it after the next load
///   (probe DecalTransferProbe of Milestone 2: the pasted fin sets come back with the target's
///   bytes).
/// - importImage() takes images of the same source for one image (rules 1 and 2), also two
///   images that another document keeps apart: an image and a copy that makeUniqueImage() made
///   of it, while the copy has no decal file of its own (OpenRocket's decal editor gives it one
///   right away, EditDecalHelper.launchEditor(); the two share theirs only when that export
///   failed, and QtRocket has no decal editor yet). Components that show the two
///   then come into this registry with one image, the first in name order, and the decals of
///   the other are renamed to it; the bytes are the same. Java's pasted components keep the two
///   image objects, which its saver writes under their two names.
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

    /// The image of @p attachment, registered now when it is new, or again when it is a removed
    /// image of the same source (see the class comment).
    /// @throws BugError when @p attachment is null
    [[nodiscard]] std::shared_ptr<DecalImage> getDecalImage(
        const std::shared_ptr<const Attachment>& attachment);

    /// Whether getDecalImage(@p attachment) would give an image there is already, a registered
    /// one or a removed one of the same source that it would register again, and so make no
    /// new one. Changes nothing.
    /// @throws BugError when @p attachment is null
    [[nodiscard]] bool hasDecalImage(const std::shared_ptr<const Attachment>& attachment) const;

    /// An image like @p original under a name no registered or removed image has: a copy with
    /// the same attachment and decal file, registered as makeUniqueName(original's name). When
    /// that is the name @p original has (it is neither registered nor removed here and collides
    /// with nothing), @p original itself is returned and nothing is registered. Null gives null.
    /// As in Java, an image registered under a name without the "decals/" prefix gets a copy
    /// with the prefix first ("plain.png" gives "decals/plain.png", then "decals/plain (1).png").
    [[nodiscard]] std::shared_ptr<DecalImage> makeUniqueImage(
        const std::shared_ptr<DecalImage>& original);

    /// @p name with the "decals/" prefix, renamed when it collides with the name of a registered
    /// or a removed image (see the class comment). Registers nothing.
    [[nodiscard]] std::string makeUniqueName(std::string_view name) const;

    /// The registered images, sorted by name (Strings::javaCompareTo()). A removed image is not
    /// among them.
    [[nodiscard]] std::vector<std::shared_ptr<DecalImage>> getDecalList() const;

    /// Takes the image registered under the name of @p decal out of the list; true when there
    /// was one. As in Java the name decides, not the object, and null gives false. The image is
    /// kept as a removed one (see the class comment): its name stays taken and find() still
    /// gives it.
    bool removeDecal(const DecalImage* decal);

    /// The image that has exactly the name @p name, registered or removed, or null: the image a
    /// rocket/Decal of that name means.
    [[nodiscard]] std::shared_ptr<DecalImage> find(std::string_view name) const;

    /// Whether an image is registered under exactly @p name; false for a removed one.
    [[nodiscard]] bool isRegistered(std::string_view name) const;

    /// What importImage() did with an image of another registry.
    struct Import
    {
        /// The image of this registry that the decals of the imported image are to name from
        /// now on (by its name, which may differ from the imported image's): one this registry
        /// had, or the copy importImage() registered.
        std::shared_ptr<DecalImage> image;
        /// Whether importImage() registered it: a new copy, or an image that removeDecal() had
        /// taken out, registered again. False when it was registered already.
        bool registered{false};
    };

    /// The image of this registry for @p image, an image of another registry or a copy of
    /// one, which components that come into this registry's document show (see "Images of
    /// another document" in the class comment): an image this registry has, registered again
    /// when it was removed, or a copy of @p image registered now. Reads the bytes of both
    /// images when this registry has an image of @p image's name and another source.
    [[nodiscard]] Import importImage(const DecalImage& image);

    /// How many images importImage() of each of @p images, one after the other in their order,
    /// would register (a copy, or a removed image registered again): by how much size() would
    /// grow. Changes nothing; reads the bytes of images as importImage() does. An addition, for
    /// a caller that bounds the images of a document (ComponentCopy::countNewDecalImages()).
    [[nodiscard]] std::size_t countImports(
        const std::vector<std::shared_ptr<const DecalImage>>& images) const;

    /// Registers the removed image named @p name again and returns it (what getDecalImage()
    /// does for a removed image of the same source); null when no removed image has that
    /// name, also when a registered one has it.
    std::shared_ptr<DecalImage> registerAgain(std::string_view name);

    /// The number of registered images.
    [[nodiscard]] std::size_t size() const noexcept { return m_registeredDecals.size(); }
    [[nodiscard]] bool        empty() const noexcept { return m_registeredDecals.empty(); }

private:
    /// Images by name; the key is the image's getName().
    using Images = std::map<std::string, std::shared_ptr<DecalImage>, std::less<>>;

    /// The image of @p images whose decal file is @p file, or null (Java: findDecalForFile()).
    [[nodiscard]] static std::shared_ptr<DecalImage> findDecalForFile(
        const Images& images, const std::filesystem::path& file);

    /// The image of the same source as @p image (DecalImage::hasSameSource()): the first
    /// registered one in name order, else the first removed one, else null.
    [[nodiscard]] std::shared_ptr<DecalImage> findSameSource(const DecalImage& image) const;

    /// importImage()'s result for @p image, an image of this registry: registered again when it
    /// was removed.
    [[nodiscard]] Import reuse(const std::shared_ptr<DecalImage>& image);

    /// The registered images.
    Images m_registeredDecals;
    /// The images removeDecal() took out of the list. A name is in one of the two maps at most.
    Images m_detachedDecals;
};

}  // namespace QtRocket
