#include "QtRocket/file/openrocket/AtmosphereHandler.h"

#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

}  // namespace

AtmosphereHandler::AtmosphereHandler(std::optional<std::string> model) : m_model(std::move(model))
{
}

Result<ElementHandler*> AtmosphereHandler::openElement(std::string_view /*element*/,
                                                       const Attributes& /*attributes*/,
                                                       WarningSet& /*warnings*/)
{
    return &PlainTextHandler::instance();
}

Result<void> AtmosphereHandler::closeElement(std::string_view element, const Attributes& attributes,
                                             std::string_view content, WarningSet& warnings)
{
    const double d = Strings::javaParseDouble(content).value_or(kNaN);

    // Where OpenRocket tests for a NaN this tests for a value that is not finite: an infinite
    // temperature or pressure is refused too (decision U3).
    if (element == "basetemperature")
    {
        if (!std::isfinite(d))
        {
            warnings.add("Illegal base temperature specified, ignoring.");
        }
        m_temperature = std::isfinite(d) ? d : kNaN;
    }
    else if (element == "basepressure")
    {
        if (!std::isfinite(d))
        {
            warnings.add("Illegal base pressure specified, ignoring.");
        }
        // Prevent zero or negative pressures.
        m_pressure = std::isfinite(d) ? MathUtil::javaMax(d, 0.001) : kNaN;
    }
    else if (element == "baserelativehumidity")
    {
        if (std::isnan(d) || d < 0 || d > 1)
        {
            warnings.add("Illegal base humidity specified, ignoring");
        }
        else
        {
            m_humidity = d;
        }
    }
    else
    {
        return AbstractElementHandler::closeElement(element, attributes, content, warnings);
    }
    return {};
}

void AtmosphereHandler::storeSettings(SimulationOptions& options, WarningSet& warnings) const
{
    if (!std::isnan(m_pressure))
    {
        options.setLaunchPressure(m_pressure);
    }
    if (!std::isnan(m_temperature))
    {
        options.setLaunchTemperature(m_temperature);
    }
    if (!std::isnan(m_humidity))
    {
        options.setLaunchRelativeHumidity(m_humidity);
    }

    if (m_model == "isa")
    {
        options.setIsaAtmosphere(true);
    }
    else if (m_model == "extendedisa")
    {
        options.setIsaAtmosphere(false);
    }
    else
    {
        options.setIsaAtmosphere(true);
        warnings.add("Unknown atmospheric model, using ISA.");
    }
}

}  // namespace QtRocket
