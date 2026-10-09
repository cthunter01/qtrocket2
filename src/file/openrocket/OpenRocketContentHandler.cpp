#include "QtRocket/file/openrocket/OpenRocketContentHandler.h"

#include <memory>
#include <string>
#include <string_view>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/ComponentParameterHandler.h"
#include "QtRocket/file/openrocket/DatatypeHandler.h"
#include "QtRocket/file/openrocket/DocumentPreferencesHandler.h"
#include "QtRocket/file/openrocket/PhotoStudioHandler.h"
#include "QtRocket/file/openrocket/SimulationsHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

OpenRocketContentHandler::OpenRocketContentHandler(const DocumentLoadingContext& context)
  : m_context(&context), m_document(context.getOpenRocketDocument())
{
    if (m_document == nullptr)
    {
        bug("The loading context of the content handler has no document");
    }
}

Result<ElementHandler*> OpenRocketContentHandler::openElement(std::string_view element,
                                                              const Attributes& /*attributes*/,
                                                              WarningSet& warnings)
{
    if (element == "rocket")
    {
        if (m_rocketDefined)
        {
            warnings.add(Warning::fromString(
                "Multiple rocket designs within one document, ignoring later ones."));
            return nullptr;
        }
        m_rocketDefined = true;
        m_childHandler =
            std::make_unique<ComponentParameterHandler>(m_document->getRocket(), *m_context);
        return m_childHandler.get();
    }

    if (element == "datatypes")
    {
        if (m_datatypesDefined)
        {
            warnings.add(Warning::fromString("Multiple datatype blocks. Ignoring later ones."));
            return nullptr;
        }
        m_datatypesDefined = true;
        m_childHandler     = std::make_unique<DatatypeHandler>(*m_context);
        return m_childHandler.get();
    }

    if (element == "simulations")
    {
        if (m_simulationsDefined)
        {
            warnings.add(Warning::fromString(
                "Multiple simulation definitions within one document, ignoring later ones."));
            return nullptr;
        }
        m_simulationsDefined = true;
        m_childHandler       = std::make_unique<SimulationsHandler>(*m_context);
        return m_childHandler.get();
    }

    if (element == "photostudio")
    {
        m_childHandler = std::make_unique<PhotoStudioHandler>(m_document->getPhotoSettings());
        return m_childHandler.get();
    }

    if (element == "docprefs")
    {
        m_childHandler = std::make_unique<DocumentPreferencesHandler>(*m_context);
        return m_childHandler.get();
    }

    warnings.add(Warning::fromString("Unknown element " + std::string(element) + ", ignoring."));

    return nullptr;
}

}  // namespace QtRocket
