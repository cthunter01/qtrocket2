#include "QtRocket/file/openrocket/RadiusPositionSetter.h"

#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/rocket/position/RadiusPositionable.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// OpenRocket's warning for an offset it cannot read, with its four blanks.
[[nodiscard]] std::string invalidValue(std::string_view value, const RocketComponent& component)
{
    return std::format("Warning: invalid value radius position. value={}    class: {}", value,
                       DocumentConfig::javaClassName(component));
}

}  // namespace

Result<void> RadiusPositionSetter::set(RocketComponent& component, std::string_view value,
                                       const Attributes& attributes, WarningSet& warnings,
                                       const DocumentLoadingContext& /*context*/) const
{
    // findEnum(attributes.get("method"), RadiusMethod.class), and SURFACE for null.
    const std::optional<std::string_view> name = DocumentConfig::attribute(attributes, "method");
    const RadiusMethod method = (name.has_value() ? radiusMethodFromOrkName(*name) : std::nullopt)
                                    .value_or(RadiusMethod::SURFACE);

    const std::optional<double> offset = Strings::javaParseDouble(value);
    if (!offset.has_value())
    {
        warnings.add(invalidValue(value, component));
        return {};
    }

    auto* const positionable = dynamic_cast<RadiusPositionable*>(&component);
    if (positionable == nullptr)
    {
        warnings.add(std::format("Warning: radiusPositionable is not valid for this class: {}",
                                 DocumentConfig::javaClassName(component)));
        return {};
    }

    // Java stores a NaN and an infinity; they are not applied here (decision L3).
    if (!std::isfinite(*offset))
    {
        warnings.add(invalidValue(value, component));
        return {};
    }

    positionable->setRadiusMethod(method);
    positionable->setRadiusOffset(*offset);
    return {};
}

}  // namespace QtRocket
