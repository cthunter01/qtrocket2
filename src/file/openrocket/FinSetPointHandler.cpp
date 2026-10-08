#include "QtRocket/file/openrocket/FinSetPointHandler.h"

#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

namespace
{

constexpr std::string_view kIllegalFinPoints = "Illegal fin points specification, ignoring.";

/// The coordinate the attribute @p name of a point gives, or nullopt when there is no such
/// attribute or it is no finite number.
[[nodiscard]] std::optional<double> coordinate(const ElementHandler::Attributes& attributes,
                                               std::string_view                  name)
{
    const std::optional<std::string_view> text = DocumentConfig::attribute(attributes, name);
    if (!text.has_value())
    {
        return std::nullopt;
    }
    // Java: Double.parseDouble, a NaN and an infinity included.
    return DocumentConfig::parseFiniteDouble(*text);
}

}  // namespace

FinSetPointHandler::FinSetPointHandler(FreeformFinSet& finset,
                                       const DocumentLoadingContext& /*context*/) noexcept
  : m_finset(&finset)
{
}

Result<ElementHandler*> FinSetPointHandler::openElement(std::string_view /*element*/,
                                                        const Attributes& /*attributes*/,
                                                        WarningSet& /*warnings*/)
{
    return &PlainTextHandler::instance();
}

Result<void> FinSetPointHandler::closeElement(std::string_view  element,
                                              const Attributes& attributes,
                                              std::string_view content, WarningSet& warnings)
{
    const std::optional<double> x = coordinate(attributes, "x");
    const std::optional<double> y = coordinate(attributes, "y");
    if (!x.has_value() || !y.has_value())
    {
        warnings.add(Warning::fromString(std::string(kIllegalFinPoints)));
        return {};
    }
    m_coordinates.emplace_back(*x, *y);

    // Java removes the two attributes it has read from the map, which stays const here.
    Attributes others = attributes;
    others.erase("x");
    others.erase("y");
    return AbstractElementHandler::closeElement(element, others, content, warnings);
}

Result<void> FinSetPointHandler::endHandler(std::string_view /*element*/,
                                            const Attributes& /*attributes*/,
                                            std::string_view /*content*/, WarningSet& warnings)
{
    if (m_coordinates.empty())
    {
        // Java: an IndexOutOfBoundsException out of setPoints(), which ends the load.
        warnings.add(Warning::fromString(std::string(kIllegalFinPoints)));
        return {};
    }
    m_finset->setPoints(m_coordinates);
    // Update the tab position. This is because the tab position relies on the finset length,
    // but because the <tabposition> tag comes before the <finpoints> tag in the .ork file, the
    // tab position will be set first, using the default finset length, not the intended finset
    // length that we extract in this part. So we update the tab position here to cope for the
    // wrongly calculated tab position earlier.
    m_finset->updateTabPosition();
    return {};
}

}  // namespace QtRocket
