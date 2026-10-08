#include "QtRocket/file/openrocket/ColorSetter.h"

#include <optional>
#include <string_view>
#include <utility>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

ColorSetter::ColorSetter(SetFunction set) : m_set(std::move(set))
{
    QTROCKET_ASSERT(m_set != nullptr);
}

Result<void> ColorSetter::set(RocketComponent& component, std::string_view value,
                              const Attributes& attributes, WarningSet& warnings,
                              const DocumentLoadingContext& /*context*/) const
{
    // ORColor.fromXMLAttributes(attributes)
    const std::optional<Color> color =
        Color::fromXmlAttributes(DocumentConfig::attribute(attributes, "red"),
                                 DocumentConfig::attribute(attributes, "green"),
                                 DocumentConfig::attribute(attributes, "blue"),
                                 DocumentConfig::attribute(attributes, "alpha"));
    if (!color.has_value())
    {
        warnings.add(Warning::kFileInvalidParameter);
        return {};
    }

    m_set(component, *color);

    if (!Strings::trim(value).empty())
    {
        warnings.add(Warning::kFileInvalidParameter);
    }
    return {};
}

}  // namespace QtRocket
