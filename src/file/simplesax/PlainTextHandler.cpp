#include "QtRocket/file/simplesax/PlainTextHandler.h"

#include <string>
#include <string_view>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

PlainTextHandler& PlainTextHandler::instance() noexcept
{
    static PlainTextHandler s_instance;
    return s_instance;
}

Result<ElementHandler*> PlainTextHandler::openElement(std::string_view element,
                                                      const Attributes& /*attributes*/,
                                                      WarningSet& warnings)
{
    warnings.add("Unknown element " + std::string(element) + ", ignoring.");
    return nullptr;
}

Result<void> PlainTextHandler::closeElement(std::string_view /*element*/,
                                            const Attributes& /*attributes*/,
                                            std::string_view /*content*/, WarningSet& /*warnings*/)
{
    // Warning from openElement is sufficient.
    return {};
}

}  // namespace QtRocket
