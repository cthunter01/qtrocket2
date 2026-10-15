#pragma once

#include <memory>
#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/AppearanceBuilder.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class Attachment;
class DocumentLoadingContext;
class RocketComponent;

/// Reads the <appearance> element of a component: its paint, its shine and its decal, the
/// image on its surface (OpenRocket's file/openrocket/importt/AppearanceHandler). The values
/// are collected in an AppearanceBuilder of the handler's own, which starts from the builder's
/// defaults (paint 187, 187, 187, shine 0.3, no decal), and become the component's appearance
/// when the element ends (RocketComponent::setAppearance(), a NONFUNCTIONAL_CHANGE event). So a
/// second <appearance> of a component replaces the first one whole, and an empty element gives
/// the default appearance, not none. Every component takes one, a stage too.
///
/// The children, as they close:
/// - <paint red="..." green="..." blue="..." alpha="...">: the colour Color::fromXmlAttributes()
///   reads; one it does not read (a missing or wrong value) is passed over without a word, and
///   so are the text and any other attribute of the element.
/// - <shine>: Double.parseDouble of the text. (An Appearance clamps it to 0 to 1.)
/// - <opacityaffectstexture>: Boolean.parseBoolean of the text, which is not trimmed, so
///   " true " is false.
/// - <decal name="..." rotation="..." edgemode="...">: the image. Its attachment is the one the
///   context's attachment factory makes for the name (for a design whose images are files,
///   only for a name inside the design's directory: see the deviations), the image the one the
///   document's decal registry has for that attachment (OpenRocketDocument::getDecalImage()), and
///   the decal of the appearance carries the NAME OF THAT IMAGE, which is the attribute's text for
///   an attachment of an archive and "decals/<file name>", made unique, for a file (see
///   DecalRegistry). The bytes are not read. The rotation is Double.parseDouble, in radians;
///   the edge mode is the constant of Decal::EdgeMode named exactly so ("REPEAT", "MIRROR",
///   "CLAMP", "STICKER"). A second <decal> sets these three anew (both images are registered)
///   and leaves the center, the offset and the scale the first one gave where it gives none.
/// - inside a <decal>: <center>, <offset> and <scale>, each with the attributes x and y
///   (Double.parseDouble). Their text and other attributes are passed over.
/// - anything else, and <center>, <offset> and <scale> outside a <decal>, gives
///   AbstractElementHandler's warnings for its text and its attributes; a <decal> does for its
///   text and for attributes other than the three. An element in any child but a <decal> is
///   "Unknown element <name>, ignoring." (PlainTextHandler).
///
/// A failure ends the load, with ErrorCode::INVALID_ARGUMENT and the message of the exception
/// Java ends it with: a rotation, a shine, an x or a y that is no number (DocumentConfig::
/// parseDouble(): `For input string: "x"`), and an edge mode that is none ("No enum constant
/// info.openrocket.core.appearance.Decal.EdgeMode.<text>").
///
/// Deviations from OpenRocket:
/// - A <decal> without name, rotation or edgemode is ignored with everything in it and one
///   warning of QtRocket's own, LoaderWarnings::kDecalIgnored ("A decal without a name, a
///   rotation or an edge mode was ignored."), and neither an image is registered nor a value
///   set.
///   OpenRocket dies there of a NullPointerException (for a missing rotation or edge mode
///   after it has registered the image). The attributes are looked at in Java's order, so a
///   rotation that is no number fails the load also when the edge mode is missing (and, as in
///   OpenRocket, with the image registered). A missing name is looked at first, so a decal
///   without one is ignored whatever its rotation and edge mode are. That is what OpenRocket
///   dies of when the attachments are files (FileSystemAttachmentFactory: a
///   NullPointerException for the name). When they are the entries of an archive
///   (ZipFileAttachmentFactory, which every archived design is read with) OpenRocket does
///   not die of the missing name: it registers an image without a name and goes on, so that
///   a nameless decal with a rotation and an edge mode loads there (and whoever then lists
///   the document's images dies of the name that is null), and one whose rotation is no
///   number or whose edge mode is none FAILS THE LOAD there, where it is ignored with the
///   warning here, as with files. The ignored decal is read by an IgnoredElementHandler
///   (decision U7 of tier 9), not by the null return with which OpenRocket's handlers ignore
///   an element: nothing in it is looked at, nothing of it is warned of when it closes (its
///   text and its other attributes neither), and the elements around it close with their own
///   attributes and text. (With the null return, which run 9b had here, the component's
///   element closed with those of the <appearance> and warned of them as its own.)
/// - The decal images of a document are bounded: a <decal> whose image would be one more
///   than DocumentConfig::kMaxDecalImages (the registry of the document would have to make a
///   new image for its name, OpenRocketDocument::hasDecalImage()) is ignored, with everything
///   in it and one warning of QtRocket's own, LoaderWarnings::tooManyDecalImages() ("The
///   document has 1000 decal images already; ignoring the decal of a further image."). It is
///   read by an IgnoredElementHandler as the decal without a name is, and like that one
///   whatever its rotation and its edge mode are: one that is no number or none does not
///   fail the load then. A decal of an image the document has already is read as ever, also
///   when the bound is reached. OpenRocket has no bound: its registry, as this one, looks
///   through all its images for every new one, so that the time to read the decals grows
///   with the square of the number of images (see DocumentConfig::kMaxDecalImages).
/// - Which image file a design may read (decision D8 of 2026-10-14, the rule of the lookup
///   tables, DesignFiles::fileOfDesign()): when the images are files (the context's
///   attachment factory reads the file system, AttachmentFactory::readsFileSystem(): a plain or
///   gzip design), a <decal> whose name that rule refuses for the design's directory
///   (DocumentLoadingContext::getDesignDirectory()) is ignored, with everything in it and one
///   warning of QtRocket's own: a name that is not relative ("/datafiles/textures/wood.jpg",
///   as OpenRocket names its built-in textures), one that leaves the directory through "..",
///   every name of a design without a directory (one read from memory without a base
///   directory) with LoaderWarnings::decalFileOutsideDesign() ("The decal image file '<name>'
///   was not read: a design may name such a file only by a relative path inside its own
///   directory; ignoring the decal."), and on Windows only a name with the name of a device in
///   it with LoaderWarnings::decalFileOfDeviceName(). Nothing is asked of the factory and no
///   image is registered; the decal is read by an IgnoredElementHandler as the decal beyond
///   the bound is, whatever its rotation and edge mode are (one that is no number or none does
///   not fail the load then). The rule is asked after the name, the rotation and the edge mode
///   are found there, so a decal that lacks one of them gets kDecalIgnored, and before the
///   bound on the images. The names of an archive's entries are free, as an archive is no
///   file system: the archive of OpenRocket's example "Parallel booster staging" holds its
///   textures as entries "/datafiles/textures/...", which load as in OpenRocket (its plain
///   re-save, where they are files, loses its 17 decals). A name the rule takes is read from
///   the file the rule gives, as a lookup table is: the design's directory with the lexically
///   normal form of the name behind it, the name first spelled as Java's File spells it,
///   without doubled separators and without one at its end (withoutRedundantSeparators(), as
///   FileSystemAttachmentFactory does: "decals/a.png/" reads decals/a.png). The attachment, a
///   FileSystemAttachment, keeps the name as the design writes it. So no ".." behind a
///   symbolic link inside the design's directory climbs out of it ("sub/link/../../x.png" reads
///   x.png of the design's directory, not of the directory above the link's target; the link
///   itself is followed wherever it points). Two spellings of one file ("a.png", "./a.png",
///   "sub/../a.png") are one image here, as the registry finds the image of a file by its path;
///   OpenRocket, which compares the paths as written, makes two. OpenRocket reads any path, a
///   relative one against the current directory of the process when the design has no
///   directory.
/// - A <center>, <offset> or <scale> without x or y is passed over with a warning of
///   QtRocket's own, LoaderWarnings::kDecalPairIgnored (OpenRocket: a NullPointerException); x
///   is looked at before y, so an x that is no number fails the load also when y is missing.
/// - Java removes name, rotation and edgemode from the attribute map of a <decal> when it
///   opens, so that only the others are warned of when it closes. The attributes being const
///   here, the warning is decided on a copy without the three. It differs in two ways once
///   ignored elements have shifted the attributes (DelegatorHandler). When an element ignored
///   inside the <decal> has shifted them and the ones the <decal> closes with are nothing but
///   some of those three names, OpenRocket warns of unknown attributes and QtRocket does not.
///   And when two or more elements are ignored behind the opening of the <decal>, in it
///   (<decal ...><foo><bar/></foo><foo><bar/></foo></decal>, or two <bar/> in one <foo>) or
///   one in it and one behind it in the <appearance>, the decal's OWN map is handed on to an
///   enclosing element: OpenRocket's is the one it emptied, QtRocket's still holds the three.
///   The component's element then closes with "Unknown attributes in element '<component>',
///   ignoring." here and without a warning there, and with six such elements the three
///   reach the parent of the <rocket> element (which the content handler of the whole file
///   would warn of).
/// - The channels of a <paint> are read by Strings::parseInt(), whose digits are ASCII only:
///   a channel written with the decimal digits of another script (the Arabic-Indic four) is
///   one for Integer.parseInt and none here, so that the paint is passed over.
/// - The numbers of an appearance are no input of a simulation, so a NaN and an infinity are
///   taken as OpenRocket takes them (decision U3 does not apply): a rotation of NaN is stored.
/// - A decal holds the name of its image (see Decal); OpenRocket's holds the image.
/// - A context without a document is a BugError when the handler is made (Java: a
///   NullPointerException at the first <decal>).
class AppearanceHandler : public AbstractElementHandler
{
public:
    /// The handler of the <appearance> element of @p component. Both @p component and
    /// @p context, and the document of the context, must outlive the handler.
    /// @throws BugError when @p context has no document
    AppearanceHandler(RocketComponent& component, const DocumentLoadingContext& context);
    /// A temporary context would dangle.
    AppearanceHandler(RocketComponent& component, const DocumentLoadingContext&& context) = delete;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

    /// A <decal> ends: nothing more. The element itself ends: the component gets the
    /// appearance (setAppearance()).
    [[nodiscard]] Result<void> endHandler(std::string_view element, const Attributes& attributes,
                                          std::string_view content, WarningSet& warnings) override;

protected:
    /// Gives the component the appearance the element holds (Java: setAppearance()): here its
    /// outside appearance.
    virtual void setAppearance();

    /// The component the element belongs to.
    [[nodiscard]] RocketComponent& component() const noexcept { return *m_component; }

    /// The appearance of the values read so far.
    [[nodiscard]] Appearance builtAppearance() const { return m_builder.getAppearance(); }

private:
    // openElement() and closeElement() in parts.

    /// A <decal> opens: its image, rotation and edge mode; false, with the warning for it,
    /// when it lacks one of the three, names an image file the design may not read, or would
    /// give the document too many images, and is to be ignored.
    [[nodiscard]] Result<bool> openDecal(const Attributes& attributes, WarningSet& warnings);
    /// The attachment of the image a <decal> names as @p name: the attachment factory's for an
    /// entry of an archive, whatever its name; for a file, the FileSystemAttachment of the file
    /// DesignFiles::fileOfDesign() gives for the name, or null with the warning for the
    /// refusal (decision D8 of 2026-10-14; see the class comment).
    [[nodiscard]] std::shared_ptr<const Attachment> readableImage(std::string_view name,
                                                                  WarningSet&      warnings) const;
    /// A <paint> closes.
    void closePaint(const Attributes& attributes);
    /// A <shine> closes.
    [[nodiscard]] Result<void> closeShine(std::string_view content);
    /// A <center>, <offset> or <scale> of a decal closes: @p set is the builder's setter of
    /// the pair.
    [[nodiscard]] Result<void> closePair(const Attributes& attributes, WarningSet& warnings,
                                         void (AppearanceBuilder::*set)(double, double));

    RocketComponent*              m_component;
    const DocumentLoadingContext* m_context;
    AppearanceBuilder             m_builder;
    bool                          m_isInDecal{false};
    /// Whether the element that closes next at this handler is a <decal> that was ignored.
    bool m_isIgnoringDecal{false};
};

}  // namespace QtRocket
