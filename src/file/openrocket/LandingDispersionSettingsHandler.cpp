#include "QtRocket/file/openrocket/LandingDispersionSettingsHandler.h"

#include <string>
#include <string_view>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

// HOOK(monte-carlo): Java parses the attributes runs and seed here (Integer.valueOf of the
// trimmed text) and warns when one is missing or no integer.
LandingDispersionSettingsHandler::LandingDispersionSettingsHandler(const Attributes& attributes)
  : m_settings(attributes)
{
}

Result<ElementHandler*> LandingDispersionSettingsHandler::openElement(
    std::string_view element, const Attributes& /*attributes*/, WarningSet& warnings)
{
    if (element == "uncertainty")
    {
        return &PlainTextHandler::instance();
    }
    warnings.add("Unknown landing-dispersion element '" + std::string(element) + "', ignoring.");
    return nullptr;
}

Result<void> LandingDispersionSettingsHandler::closeElement(std::string_view  element,
                                                            const Attributes& attributes,
                                                            std::string_view /*content*/,
                                                            WarningSet& /*warnings*/)
{
    if (element != "uncertainty")
    {
        return {};
    }

    // HOOK(monte-carlo): Java reads parameter and distribution with DocumentConfig.findEnum()
    // and spread with DocumentConfig.stringToDouble(), makes an UncertaintySpec of them (which
    // refuses a spread that is not finite or negative, and a distribution that does not fit
    // the parameter), keeps one uncertainty per parameter and none for a spread of 0, with a
    // warning for each thing it refuses; and when the element ends it builds the
    // MonteCarloSettings, which refuses a number of runs outside 2 to 100000.
    m_settings.addUncertainty(attributes);
    return {};
}

}  // namespace QtRocket
