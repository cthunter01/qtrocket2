#include "QtRocket/file/openrocket/IgnoredElementHandler.h"

#include <string_view>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

IgnoredElementHandler& IgnoredElementHandler::instance() noexcept
{
    static IgnoredElementHandler s_instance;
    return s_instance;
}

Result<ElementHandler*> IgnoredElementHandler::openElement(std::string_view /*element*/,
                                                           const Attributes& /*attributes*/,
                                                           WarningSet& /*warnings*/)
{
    // Never null: an element that is ignored that way leaves its attributes and text to the
    // elements around it (see the class comment).
    return this;
}

Result<void> IgnoredElementHandler::closeElement(std::string_view /*element*/,
                                                 const Attributes& /*attributes*/,
                                                 std::string_view /*content*/,
                                                 WarningSet& /*warnings*/)
{
    return {};
}

Result<void> IgnoredElementHandler::endHandler(std::string_view /*element*/,
                                               const Attributes& /*attributes*/,
                                               std::string_view /*content*/,
                                               WarningSet& /*warnings*/)
{
    return {};
}

}  // namespace QtRocket
