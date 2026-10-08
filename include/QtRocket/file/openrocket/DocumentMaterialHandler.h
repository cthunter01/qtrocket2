#pragma once

#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class OpenRocketDocument;

/// Reads the <docmaterials> element of a document's preferences: the materials that belong to
/// the document, each a <material> child whose text is the material as
/// Material::toStorableString() writes it, "BULK|My wood|680.0|4.0E8|Woods" (OpenRocket's
/// file/openrocket/importt/DocumentMaterialHandler).
///
/// A <material> that closes is read with Material::fromStorableString() as a user-defined
/// material (the older forms without a shear modulus or without a group included; the group
/// "ThreadsLines" of old files is looked up in the application's materials of the context,
/// when it has some), marked as a document material and added to the document's materials
/// (OpenRocketDocument::getDocumentMaterials()), which do not take a material they have
/// already. Its attributes are not looked at. A text that is no material ends the load, with
/// ErrorCode::INVALID_ARGUMENT and Java's message, "Illegal material string: <text>".
///
/// Any other child is ignored with everything in it and WITHOUT a warning. As after every
/// ignored element, DelegatorHandler then closes the enclosing elements with the attributes
/// and the text of the element below them: <docmaterials> with those of the ignored child,
/// <docprefs> with those of <docmaterials>, and so on to the root.
///
/// The materials a file lists here come on top of the ones its components were given, which
/// the document registers as the components get them (Rocket::documentMaterialSet()).
///
/// Deviations from OpenRocket:
/// - A material whose density is a NaN or an infinity is not added, and one whose shear
///   modulus is has it read as 0; each gives Warning::kFileInvalidParameter (OpenRocket stores
///   both). It is the rule of MaterialSetter for a component's material, taken over: a density
///   is an input of a simulation as soon as a component gets the material, and a NaN, which
///   equals nothing, is never found among the materials again, so every line of a file that
///   holds one would add one more.
/// - The materials go into the document's MaterialStorage (Java: the three databases of its
///   DocumentPreferences).
/// - The constructor takes the loading context (Java: the document), for the document and the
///   application's materials (Java: the global Databases). Without a document in the context
///   it is a BugError.
class DocumentMaterialHandler final : public AbstractElementHandler
{
public:
    /// The handler of the <docmaterials> element of the document of @p context. The context
    /// and what it points at must outlive the handler.
    /// @throws BugError when @p context has no document
    explicit DocumentMaterialHandler(const DocumentLoadingContext& context);
    /// A temporary context would dangle.
    explicit DocumentMaterialHandler(const DocumentLoadingContext&& context) = delete;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

private:
    const DocumentLoadingContext* m_context;
    OpenRocketDocument*           m_document;
};

}  // namespace QtRocket
