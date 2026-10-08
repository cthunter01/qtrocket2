#include "QtRocket/file/openrocket/DocumentMaterialHandler.h"

#include <cmath>
#include <string_view>
#include <utility>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

DocumentMaterialHandler::DocumentMaterialHandler(const DocumentLoadingContext& context)
  : m_context(&context), m_document(context.getOpenRocketDocument())
{
    if (m_document == nullptr)
    {
        bug("The loading context of a document material handler has no document");
    }
}

Result<ElementHandler*> DocumentMaterialHandler::openElement(std::string_view element,
                                                             const Attributes& /*attributes*/,
                                                             WarningSet& /*warnings*/)
{
    if (element == "material")
    {
        return &PlainTextHandler::instance();
    }
    return nullptr;
}

Result<void> DocumentMaterialHandler::closeElement(std::string_view element,
                                                   const Attributes& /*attributes*/,
                                                   std::string_view content, WarningSet& warnings)
{
    if (element != "material")
    {
        return {};
    }
    // Material.fromStorableString(content, true), which asks the application's materials for
    // the group of a "ThreadsLines" material.
    const MaterialStorage* const application = m_context->getApplicationMaterials();
    Result<Material> material = application != nullptr
                                    ? Material::fromStorableString(content, true, *application)
                                    : Material::fromStorableString(content, true);
    if (!material)
    {
        // Java: an IllegalArgumentException, which ends the load.
        return fail(ErrorCode::INVALID_ARGUMENT, std::move(material.error().message));
    }
    // Not OpenRocket's, which stores both: see the class comment.
    if (!std::isfinite(material->getDensity()))
    {
        warnings.add(Warning::kFileInvalidParameter);
        return {};
    }
    if (!std::isfinite(material->getInPlaneShearModulus()))
    {
        warnings.add(Warning::kFileInvalidParameter);
        material = Material(material->getType(), material->getName(), material->getDensity(), 0.0,
                            material->getGroup(), material->isUserDefined(), false);
    }
    material->setDocumentMaterial(true);
    m_document->getDocumentMaterials().addMaterial(*material);
    return {};
}

}  // namespace QtRocket
