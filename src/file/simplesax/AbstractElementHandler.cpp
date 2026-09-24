#include "QtRocket/file/simplesax/AbstractElementHandler.h"

#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

Result<void> AbstractElementHandler::closeElement(std::string_view  element,
                                                  const Attributes& attributes,
                                                  std::string_view content, WarningSet& warnings)
{
    if (!Strings::trim(content).empty())
    {
        warnings.add("Unknown text in element '" + std::string(element) + "', ignoring.");
    }
    if (!attributes.empty())
    {
        warnings.add("Unknown attributes in element '" + std::string(element) + "', ignoring.");
    }
    return {};
}

Result<void> AbstractElementHandler::endHandler(std::string_view /*element*/,
                                                const Attributes& /*attributes*/,
                                                std::string_view /*content*/,
                                                WarningSet& /*warnings*/)
{
    // No-op
    return {};
}

double AbstractElementHandler::parseDouble(std::string_view text, WarningSet& warnings,
                                           const Warning& warning)
{
    const std::optional<double> value = Strings::javaParseDouble(text);
    if (!value.has_value())
    {
        warnings.add(warning);
        return std::numeric_limits<double>::quiet_NaN();
    }
    return *value;
}

}  // namespace QtRocket
