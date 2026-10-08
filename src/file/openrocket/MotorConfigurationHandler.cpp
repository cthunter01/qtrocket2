#include "QtRocket/file/openrocket/MotorConfigurationHandler.h"

#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

MotorConfigurationHandler::MotorConfigurationHandler(
    Rocket& rocket, const DocumentLoadingContext& /*context*/) noexcept
  : m_rocket(&rocket)
{
}

Result<ElementHandler*> MotorConfigurationHandler::openElement(std::string_view element,
                                                               const Attributes& /*attributes*/,
                                                               WarningSet& warnings)
{
    if ((m_inNameElement && element == "name") || !(element == "name" || element == "stage"))
    {
        warnings.add(Warning::kFileInvalidParameter);
        return nullptr;
    }
    if (element == "name")
    {
        m_inNameElement = true;
    }

    return &PlainTextHandler::instance();
}

Result<void> MotorConfigurationHandler::closeElement(std::string_view  element,
                                                     const Attributes& attributes,
                                                     std::string_view  content,
                                                     WarningSet& /*warnings*/)
{
    if (element == "name")
    {
        m_name = std::string(content);
    }
    else if (element == "stage")
    {
        // Java: Integer.parseInt(attributes.get("number")), whose NumberFormatException ends
        // the load.
        const Result<int> stageNumber =
            DocumentConfig::parseInt(DocumentConfig::attribute(attributes, "number"));
        if (!stageNumber)
        {
            return std::unexpected(stageNumber.error());
        }
        // Boolean.parseBoolean: no attribute is false.
        const bool isActive = Strings::javaEqualsIgnoreCase(
            DocumentConfig::attribute(attributes, "active").value_or(""), "true");
        m_hasActiveStage = m_hasActiveStage || isActive;
        m_stageActiveness.insert_or_assign(*stageNumber, isActive);
    }
    return {};
}

Result<void> MotorConfigurationHandler::endHandler(std::string_view  element,
                                                   const Attributes& attributes,
                                                   std::string_view content, WarningSet& warnings)
{
    const FlightConfigurationId fcid = DocumentConfig::configurationId(attributes);
    if (!fcid.isValid())
    {
        warnings.add(Warning::kFileInvalidParameter);
        return {};
    }

    m_rocket->createFlightConfiguration(fcid);

    if (m_name.has_value() && !Strings::trim(*m_name).empty())
    {
        m_rocket->getFlightConfiguration(fcid).setName(*m_name);
    }

    // Ensure there's always at least one stage active
    if (!m_hasActiveStage && !m_stageActiveness.empty())
    {
        m_stageActiveness.insert_or_assign(0, true);
    }

    for (const auto& [stageNumber, isActive] : m_stageActiveness)
    {
        m_rocket->getFlightConfiguration(fcid).preloadStageActiveness(stageNumber, isActive);
    }

    if (DocumentConfig::attribute(attributes, "default") == std::string_view("true"))
    {
        m_rocket->setSelectedConfiguration(fcid);
    }

    // Java removes the two attributes it has read from the map, which stays const here.
    Attributes others = attributes;
    others.erase("configid");
    others.erase("default");
    return AbstractElementHandler::closeElement(element, others, content, warnings);
}

}  // namespace QtRocket
