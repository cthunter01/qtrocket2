#include "QtRocket/file/openrocket/AxialPositionSetter.h"

#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/AxialPositionable.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The method the attribute @p name names (Java: findEnum(attributes.get(name),
/// AxialMethod.class)); nullopt when the element has no such attribute or it names none.
[[nodiscard]] std::optional<AxialMethod> methodOf(const Setter::Attributes& attributes,
                                                  std::string_view          name)
{
    const std::optional<std::string_view> text = DocumentConfig::attribute(attributes, name);
    return text.has_value() ? axialMethodFromOrkName(*text) : std::nullopt;
}

/// OpenRocket's warning for an offset it cannot read, "radius" and the four blanks included.
[[nodiscard]] std::string invalidValue(std::string_view value, const RocketComponent& component)
{
    return std::format("Warning: invalid value radius position. value={}    class: {}", value,
                       DocumentConfig::javaClassName(component));
}

}  // namespace

Result<void> AxialPositionSetter::set(RocketComponent& component, std::string_view value,
                                      const Attributes& attributes, WarningSet& warnings,
                                      const DocumentLoadingContext& /*context*/) const
{
    // first check preferred attribute name:
    std::optional<AxialMethod> method = methodOf(attributes, "method");
    // fall-back to old name
    if (!method.has_value())
    {
        method = methodOf(attributes, "type");
    }
    if (!method.has_value())
    {
        warnings.add(Warning::kFileInvalidParameter);
        return {};
    }

    const std::optional<double> offset = Strings::javaParseDouble(value);
    if (!offset.has_value())
    {
        warnings.add(invalidValue(value, component));
        return {};
    }

    auto* const positionable = dynamic_cast<AxialPositionable*>(&component);
    if (positionable == nullptr)
    {
        warnings.add(
            std::format("Warning: info.openrocket.core.file.openrocket.importt.AxialPositionSetter "
                        "is not valid for class: {}",
                        DocumentConfig::javaClassName(component)));
        return {};
    }

    // Java sets whatever it read: the method, and then a NaN is a BugException out of
    // setAxialOffset() and an infinity is stored. Neither is applied here (decisions L3, L4).
    if (!std::isfinite(*offset))
    {
        warnings.add(invalidValue(value, component));
        return {};
    }

    positionable->setAxialMethod(*method);
    positionable->setAxialOffset(*offset);
    return {};
}

}  // namespace QtRocket
