#include "QtRocket/file/openrocket/PhotoStudioHandler.h"

#include <algorithm>
#include <array>
#include <expected>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

namespace
{

/// The value of the colour attribute @p key (safeParseColor()): nullopt when the element has
/// no such attribute, and a failure when its text is no int.
[[nodiscard]] Result<std::optional<int>> safeParseColor(
    const ElementHandler::Attributes& attributes, std::string_view key)
{
    const std::optional<std::string_view> value = DocumentConfig::attribute(attributes, key);
    if (!value.has_value())
    {
        return std::nullopt;
    }
    const Result<int> parsed = DocumentConfig::parseInt(value);
    if (!parsed)
    {
        return std::unexpected(parsed.error());
    }
    return *parsed;
}

}  // namespace

PhotoStudioHandler::PhotoStudioHandler(std::map<std::string, std::string>& settings) noexcept
  : m_settings(&settings)
{
}

Result<ElementHandler*> PhotoStudioHandler::openElement(std::string_view /*element*/,
                                                        const Attributes& /*attributes*/,
                                                        WarningSet& /*warnings*/)
{
    return &PlainTextHandler::instance();
}

Result<void> PhotoStudioHandler::closeElement(std::string_view  element,
                                              const Attributes& attributes,
                                              std::string_view content, WarningSet& warnings)
{
    if (std::ranges::find(kTextSettings, element) != kTextSettings.end())
    {
        m_settings->insert_or_assign(std::string(element), std::string(content));
        return {};
    }
    if (std::ranges::find(kColorSettings, element) != kColorSettings.end())
    {
        Result<std::optional<std::string>> color = getColor(attributes);
        if (!color)
        {
            return std::unexpected(color.error());
        }
        if (color->has_value())
        {
            m_settings->insert_or_assign(std::string(element), std::move(**color));
        }
        return {};
    }

    return AbstractElementHandler::closeElement(element, attributes, content, warnings);
}

Result<std::optional<std::string>> PhotoStudioHandler::getColor(const Attributes& attributes)
{
    const Result<std::optional<int>> red = safeParseColor(attributes, "red");
    if (!red)
    {
        return std::unexpected(red.error());
    }
    const Result<std::optional<int>> green = safeParseColor(attributes, "green");
    if (!green)
    {
        return std::unexpected(green.error());
    }
    const Result<std::optional<int>> blue = safeParseColor(attributes, "blue");
    if (!blue)
    {
        return std::unexpected(blue.error());
    }
    if (!red->has_value() || !green->has_value() || !blue->has_value())
    {
        // Java logs "Missing color component in PhotoStudioHandler element: <element>".
        return std::nullopt;
    }
    int alpha = 255;  // set default
    // add a test if "alpha" was added to the XML / backwards compatibility
    if (const std::optional<std::string_view> a = DocumentConfig::attribute(attributes, "alpha"))
    {
        // "alpha" string was present so load the value
        const Result<int> parsed = DocumentConfig::parseInt(a);
        if (!parsed)
        {
            return std::unexpected(parsed.error());
        }
        alpha = *parsed;
    }
    return std::format("{} {} {} {}", red->value_or(0), green->value_or(0), blue->value_or(0),
                       alpha);
}

}  // namespace QtRocket
