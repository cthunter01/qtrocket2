#include "QtRocket/file/simplesax/NullElementHandler.h"

#include <string>
#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

NullElementHandler& NullElementHandler::instance() noexcept
{
    static NullElementHandler s_instance;
    return s_instance;
}

Result<ElementHandler*> NullElementHandler::openElement(std::string_view element,
                                                        const Attributes& /*attributes*/,
                                                        WarningSet& warnings)
{
    warnings.add("Unknown element " + std::string(element) + ", ignoring.");
    return nullptr;
}

Result<void> NullElementHandler::closeElement(std::string_view element,
                                              const Attributes& /*attributes*/,
                                              std::string_view content, WarningSet& warnings)
{
    return AbstractElementHandler::closeElement(element, Attributes{}, content, warnings);
}

}  // namespace QtRocket
