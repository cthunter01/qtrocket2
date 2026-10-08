#include "QtRocket/file/openrocket/IgnitionConfigurationHandler.h"

#include <format>
#include <optional>
#include <string_view>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

IgnitionConfigurationHandler::IgnitionConfigurationHandler(
    const DocumentLoadingContext& /*context*/) noexcept
{
}

Result<ElementHandler*> IgnitionConfigurationHandler::openElement(std::string_view /*element*/,
                                                                  const Attributes& /*attributes*/,
                                                                  WarningSet& /*warnings*/)
{
    return &PlainTextHandler::instance();
}

Result<void> IgnitionConfigurationHandler::closeElement(std::string_view  element,
                                                        const Attributes& attributes,
                                                        std::string_view  content,
                                                        WarningSet&       warnings)
{
    content = Strings::trim(content);

    if (element == "ignitionevent")
    {
        if (const std::optional<IgnitionEvent> event = ignitionEventFromOrkName(content))
        {
            m_ignitionEvent = event;
        }
        // As in Java, where the field keeps the event of an earlier element: the warning is
        // for an element that leaves the handler without any event.
        if (!m_ignitionEvent.has_value())
        {
            warnings.add(Warning::fromString(
                std::format("Unknown ignition event type '{}', ignoring.", content)));
        }
        return {};
    }
    if (element == "ignitiondelay")
    {
        // Java: Double.parseDouble, a NaN and an infinity included.
        if (const std::optional<double> delay = DocumentConfig::parseFiniteDouble(content))
        {
            m_ignitionDelay = delay;
        }
        else
        {
            warnings.add(Warning::fromString("Illegal ignition delay specified, ignoring."));
        }
        return {};
    }
    return AbstractElementHandler::closeElement(element, attributes, content, warnings);
}

}  // namespace QtRocket
