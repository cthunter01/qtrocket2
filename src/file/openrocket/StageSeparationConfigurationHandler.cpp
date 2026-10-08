#include "QtRocket/file/openrocket/StageSeparationConfigurationHandler.h"

#include <optional>
#include <string_view>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/FlightConfigurableParameterSet.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

StageSeparationConfigurationHandler::StageSeparationConfigurationHandler(
    AxialStage& stage, const DocumentLoadingContext& /*context*/) noexcept
  : m_stage(&stage)
{
}

StageSeparationConfiguration StageSeparationConfigurationHandler::getConfiguration(
    const StageSeparationConfiguration& def) const
{
    StageSeparationConfiguration config = def.clone();
    if (m_event.has_value())
    {
        config.setSeparationEvent(*m_event);
    }
    // Java: !Double.isNaN() of a field that starts as NaN, so that an infinity is stored.
    if (m_altitude.has_value())
    {
        config.setSeparationAltitude(*m_altitude);
    }
    if (m_delay.has_value())
    {
        config.setSeparationDelay(*m_delay);
    }
    return config;
}

Result<ElementHandler*> StageSeparationConfigurationHandler::openElement(
    std::string_view /*element*/, const Attributes& /*attributes*/, WarningSet& /*warnings*/)
{
    return &PlainTextHandler::instance();
}

Result<void> StageSeparationConfigurationHandler::closeElement(std::string_view  element,
                                                               const Attributes& attributes,
                                                               std::string_view  content,
                                                               WarningSet&       warnings)
{
    content = Strings::trim(content);

    if (element == "separationevent")
    {
        m_event = separationEventFromOrkName(content);
        if (!m_event.has_value())
        {
            warnings.add(Warning::kFileInvalidParameter);
        }
        return {};
    }
    if (element == "separationaltitude")
    {
        // Java: Double.parseDouble, a NaN and an infinity included.
        m_altitude = DocumentConfig::parseFiniteDouble(content);
        if (!m_altitude.has_value())
        {
            warnings.add(Warning::kFileInvalidParameter);
        }
        return {};
    }
    if (element == "separationdelay")
    {
        // Java: Double.parseDouble, a NaN and an infinity included.
        m_delay = DocumentConfig::parseFiniteDouble(content);
        if (!m_delay.has_value())
        {
            warnings.add(Warning::kFileInvalidParameter);
        }
        return {};
    }
    return AbstractElementHandler::closeElement(element, attributes, content, warnings);
}

Result<void> StageSeparationConfigurationHandler::endHandler(std::string_view /*element*/,
                                                             const Attributes& attributes,
                                                             std::string_view /*content*/,
                                                             WarningSet& warnings)
{
    const FlightConfigurationId fcid = DocumentConfig::configurationId(attributes);
    if (!fcid.isValid())
    {
        // Not OpenRocket's, where no id read from a file is the error id: see the class comment.
        warnings.add(Warning::kFileInvalidParameter);
        return {};
    }
    FlightConfigurableParameterSet<StageSeparationConfiguration>& configurations =
        m_stage->getSeparationConfigurations();

    // copy and update to the file-read values
    const StageSeparationConfiguration config = getConfiguration(configurations.get(fcid));
    if (fcid.isDefaultId())
    {
        // Java's map takes the new value under the key that equals the default id. (set() would
        // do nothing, and setDefault() nothing for a value that compares equal.)
        configurations.getDefault() = config;
        return {};
    }
    configurations.set(fcid, config);
    return {};
}

}  // namespace QtRocket
