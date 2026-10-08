#include "QtRocket/file/openrocket/SimulationPlotAppearanceHandler.h"

#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The attribute @p name of an element, or none (Java: attributes.get(name), null when absent).
[[nodiscard]] std::optional<std::string_view> attribute(
    const ElementHandler::Attributes& attributes, std::string_view name)
{
    const auto found = attributes.find(name);
    if (found == attributes.end())
    {
        return std::nullopt;
    }
    return std::string_view{found->second};
}

}  // namespace

Result<ElementHandler*> SimulationPlotAppearanceHandler::openElement(
    std::string_view element, const Attributes& /*attributes*/, WarningSet& warnings)
{
    if (element == "series")
    {
        return &PlainTextHandler::instance();
    }
    warnings.add("Unknown element '" + std::string(element) + "', ignoring.");
    return nullptr;
}

Result<void> SimulationPlotAppearanceHandler::closeElement(std::string_view  element,
                                                           const Attributes& attributes,
                                                           std::string_view /*content*/,
                                                           WarningSet& warnings)
{
    if (element != "series")
    {
        return {};
    }

    const std::optional<std::string_view> symbol = attribute(attributes, "symbol");
    if (!symbol.has_value() || Strings::isEmpty(*symbol))
    {
        warnings.add("Plot appearance series missing symbol, ignoring.");
        return {};
    }

    // The exact lookup of the .ork format (lineStyleFromString() also takes "DASHED").
    const std::optional<LineStyle> lineStyle = DocumentConfig::findEnum(
        attribute(attributes, "linestyle"), kAllLineStyles, &lineStyleName);
    const std::optional<Color> color =
        Color::fromXmlAttributes(attribute(attributes, "red"), attribute(attributes, "green"),
                                 attribute(attributes, "blue"), attribute(attributes, "alpha"));

    const PlotAppearance appearance(color, lineStyle);
    if (!appearance.isEmpty())
    {
        m_plotAppearances.insert_or_assign(std::string(*symbol), appearance);
    }
    return {};
}

}  // namespace QtRocket
