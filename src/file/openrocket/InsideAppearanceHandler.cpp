#include "QtRocket/file/openrocket/InsideAppearanceHandler.h"

#include <string_view>

#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/AppearanceHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/InsideColorComponentHandler.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

InsideAppearanceHandler::InsideAppearanceHandler(RocketComponent&              component,
                                                 const DocumentLoadingContext& context)
  : AppearanceHandler(component, context)
{
}

Result<void> InsideAppearanceHandler::closeElement(std::string_view  element,
                                                   const Attributes& attributes,
                                                   std::string_view content, WarningSet& warnings)
{
    // TODO: delete 'edgesSameAsInside' when backward compatibility with
    // 22.02.beta.01-22.02.beta.05 is not needed anymore
    if (element == "edgessameasinside" || element == "edgesSameAsInside")
    {
        // Boolean.parseBoolean(content)
        const bool edgesSameAsInside = Strings::javaEqualsIgnoreCase(content, "true");
        if (auto* const inside = dynamic_cast<InsideColorComponent*>(&component()))
        {
            inside->getInsideColorComponentHandler().setEdgesSameAsInside(edgesSameAsInside);
        }
        return {};
    }
    // TODO: delete 'insideSameAsOutside' when backward compatibility with
    // 22.02.beta.01-22.02.beta.05 is not needed anymore
    if (element == "insidesameasoutside" || element == "insideSameAsOutside")
    {
        const bool insideSameAsOutside = Strings::javaEqualsIgnoreCase(content, "true");
        if (auto* const inside = dynamic_cast<InsideColorComponent*>(&component()))
        {
            inside->getInsideColorComponentHandler().setSeparateInsideOutside(insideSameAsOutside);
        }
        return {};
    }

    return AppearanceHandler::closeElement(element, attributes, content, warnings);
}

void InsideAppearanceHandler::setAppearance()
{
    if (auto* const inside = dynamic_cast<InsideColorComponent*>(&component()))
    {
        inside->getInsideColorComponentHandler().setInsideAppearance(builtAppearance());
    }
}

}  // namespace QtRocket
