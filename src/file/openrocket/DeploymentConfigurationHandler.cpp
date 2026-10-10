#include "QtRocket/file/openrocket/DeploymentConfigurationHandler.h"

#include <cmath>
#include <optional>
#include <string_view>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/LoaderWarnings.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfigurableParameterSet.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The number of the element @p element whose text is @p content, or none with a warning:
/// OpenRocket's for a text that is no number (Java: parseDouble(content, warnings,
/// Warning.FILE_INVALID_PARAMETER)), and QtRocket's own for a NaN or an infinity, which
/// OpenRocket passes over or stores (decision L3).
[[nodiscard]] std::optional<double> finiteNumberOf(std::string_view element,
                                                   std::string_view content, WarningSet& warnings)
{
    const std::optional<double> number = Strings::javaParseDouble(content);
    if (!number.has_value())
    {
        warnings.add(Warning::kFileInvalidParameter);
        return std::nullopt;
    }
    if (!std::isfinite(*number))
    {
        warnings.add(LoaderWarnings::numberNotFinite(element));
        return std::nullopt;
    }
    return number;
}

}  // namespace

DeploymentConfigurationHandler::DeploymentConfigurationHandler(
    RecoveryDevice& component, const DocumentLoadingContext& /*context*/) noexcept
  : m_recoveryDevice(&component)
{
}

DeploymentConfiguration DeploymentConfigurationHandler::getConfiguration(
    const DeploymentConfiguration& def) const
{
    DeploymentConfiguration config = def.clone();
    if (m_event.has_value())
    {
        config.setDeployEvent(*m_event);
    }
    // Java: !Double.isNaN() of a field that starts as NaN, so that an infinity is stored.
    if (m_delay.has_value())
    {
        config.setDeployDelay(*m_delay);
    }
    if (m_altitude.has_value())
    {
        config.setDeployAltitude(*m_altitude);
    }
    return config;
}

Result<ElementHandler*> DeploymentConfigurationHandler::openElement(
    std::string_view /*element*/, const Attributes& /*attributes*/, WarningSet& /*warnings*/)
{
    return &PlainTextHandler::instance();
}

Result<void> DeploymentConfigurationHandler::closeElement(std::string_view  element,
                                                          const Attributes& attributes,
                                                          std::string_view  content,
                                                          WarningSet&       warnings)
{
    content = Strings::trim(content);

    if (element == "deployevent")
    {
        m_event = deployEventFromOrkName(content);
        if (!m_event.has_value())
        {
            warnings.add(Warning::kFileInvalidParameter);
        }
        return {};
    }
    if (element == "deploydelay")
    {
        // Java: Double.parseDouble, a NaN and an infinity included.
        m_delay = finiteNumberOf(element, content, warnings);
        return {};
    }
    if (element == "deployaltitude")
    {
        // Java: Double.parseDouble, a NaN and an infinity included.
        m_altitude = finiteNumberOf(element, content, warnings);
        return {};
    }
    return AbstractElementHandler::closeElement(element, attributes, content, warnings);
}

Result<void> DeploymentConfigurationHandler::endHandler(std::string_view /*element*/,
                                                        const Attributes& attributes,
                                                        std::string_view /*content*/,
                                                        WarningSet& warnings)
{
    const FlightConfigurationId configId = DocumentConfig::configurationId(attributes);
    if (!configId.isValid())
    {
        // Not OpenRocket's, where no id read from a file is the error id: see the class comment.
        warnings.add(LoaderWarnings::kDeploymentConfigurationOfErrorId);
        return {};
    }
    FlightConfigurableParameterSet<DeploymentConfiguration>& configurations =
        m_recoveryDevice->getDeploymentConfigurations();
    const DeploymentConfiguration config = getConfiguration(configurations.getDefault());
    if (configId.isDefaultId())
    {
        // Java's map takes the new value under the key that equals the default id. (set() would
        // do nothing, and setDefault() nothing for a value that compares equal.)
        configurations.getDefault() = config;
        return {};
    }
    configurations.set(configId, config);
    return {};
}

}  // namespace QtRocket
