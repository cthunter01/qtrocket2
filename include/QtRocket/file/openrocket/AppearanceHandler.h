#pragma once

#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/AppearanceBuilder.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

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
///   context's attachment factory makes for the name, the image the one the document's decal
///   registry has for that attachment (OpenRocketDocument::getDecalImage()), and the decal of
///   the appearance carries the NAME OF THAT IMAGE, which is the attribute's text for an
///   attachment of an archive and "decals/<file name>", made unique, for a file (see
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
/// - A <decal> without name, rotation or edgemode is ignored with everything in it and
///   Warning::kFileInvalidParameter, and neither an image is registered nor a value set.
///   OpenRocket dies there of a NullPointerException (for a missing rotation or edge mode
///   after it has registered the image). The attributes are looked at in Java's order, so a
///   rotation that is no number fails the load also when the edge mode is missing (and, as in
///   OpenRocket, with the image registered). As after every ignored element, DelegatorHandler
///   then closes the enclosing elements with the attributes and the text of the element one
///   level below: the <appearance> with the decal's, which nobody looks at, and the
///   component's element with those of the <appearance>, so that an attribute or a text of
///   the <appearance> element, which is otherwise passed over, is then warned of as the
///   component's.
/// - A <center>, <offset> or <scale> without x or y is passed over with
///   Warning::kFileInvalidParameter (OpenRocket: a NullPointerException); x is looked at before
///   y, so an x that is no number fails the load also when y is missing.
/// - Java removes name, rotation and edgemode from the attribute map of a <decal> when it
///   opens, so that only the others are warned of when it closes. The attributes being const
///   here, the warning is decided on a copy without the three. It differs when an element
///   ignored inside the <decal> has shifted the attributes (DelegatorHandler) and the ones the
///   <decal> closes with are nothing but some of those three names: OpenRocket then warns of
///   unknown attributes and QtRocket does not.
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

    /// A <decal> opens: its image, rotation and edge mode; false when it lacks one of the
    /// three and is to be ignored.
    [[nodiscard]] Result<bool> openDecal(const Attributes& attributes, WarningSet& warnings);
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
};

}  // namespace QtRocket
