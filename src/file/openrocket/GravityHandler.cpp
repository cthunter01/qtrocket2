#include "QtRocket/file/openrocket/GravityHandler.h"

#include <cmath>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

GravityHandler::GravityHandler(std::optional<std::string> model) : m_model(std::move(model)) { }

Result<ElementHandler*> GravityHandler::openElement(std::string_view /*element*/,
                                                    const Attributes& /*attributes*/,
                                                    WarningSet& /*warnings*/)
{
    return &PlainTextHandler::instance();
}

Result<void> GravityHandler::closeElement(std::string_view element, const Attributes& attributes,
                                          std::string_view content, WarningSet& warnings)
{
    if (element != "value")
    {
        return AbstractElementHandler::closeElement(element, attributes, content, warnings);
    }

    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    double           d    = Strings::javaParseDouble(content).value_or(kNaN);
    // Not OpenRocket's, which tests for a NaN only: an infinity is refused too (decision U3).
    if (!std::isfinite(d))
    {
        warnings.add("Illegal gravity value specified, ignoring.");
        d = kNaN;
    }
    m_constantValue = d;
    return {};
}

void GravityHandler::storeSettings(SimulationOptions& options, WarningSet& warnings) const
{
    if (m_model == "wgs")
    {
        options.setGravityModelType(GravityModelType::WGS);
    }
    else if (m_model == "constant")
    {
        options.setGravityModelType(GravityModelType::CONSTANT);
        if (!std::isnan(m_constantValue))
        {
            options.setConstantGravity(m_constantValue);
        }
    }
    else
    {
        options.setGravityModelType(GravityModelType::WGS);
        warnings.add(std::format("Unknown gravity model type '{}', using WGS.",
                                 m_model.has_value() ? std::string_view(*m_model) : "null"));
    }
}

}  // namespace QtRocket
