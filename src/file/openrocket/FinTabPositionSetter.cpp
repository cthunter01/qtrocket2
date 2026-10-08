#include "QtRocket/file/openrocket/FinTabPositionSetter.h"

#include <format>
#include <optional>
#include <string_view>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

namespace
{

/// @p component as the fin set the setter is for.
[[nodiscard]] FinSet& finSetOf(RocketComponent& component)
{
    auto* const finSet = dynamic_cast<FinSet*>(&component);
    if (finSet == nullptr)
    {
        // Java: throw new IllegalStateException("FinTabPositionSetter called for component " + c)
        bug(std::format("FinTabPositionSetter called for component {}, a {}", component.getName(),
                        className(component.kind())));
    }
    return *finSet;
}

/// The value of relativeto="" with the names of old files made the current ones (Java:
/// "translate from old enum names to current enum names").
[[nodiscard]] std::string_view translated(std::string_view relative) noexcept
{
    if (relative.contains("front"))
    {
        return "top";
    }
    if (relative.contains("center"))
    {
        return "middle";
    }
    if (relative.contains("end"))
    {
        return "bottom";
    }
    return relative;
}

}  // namespace

FinTabPositionSetter::FinTabPositionSetter()
  : m_offset(
        [](RocketComponent& component, double offset) { finSetOf(component).setTabOffset(offset); })
{
}

Result<void> FinTabPositionSetter::set(RocketComponent& component, std::string_view value,
                                       const Attributes& attributes, WarningSet& warnings,
                                       const DocumentLoadingContext& context) const
{
    FinSet& finSet = finSetOf(component);

    const std::optional<std::string_view> attribute =
        DocumentConfig::attribute(attributes, "relativeto");
    if (!attribute.has_value())
    {
        warnings.add("Required attribute 'relativeto' not found for fin tab position.");
        return {};
    }

    const std::string_view           relative = translated(*attribute);
    const std::optional<AxialMethod> position = axialMethodFromOrkName(relative);
    if (!position.has_value())
    {
        warnings.add(std::format("Illegal attribute value '{}' encountered.", relative));
        return {};
    }

    finSet.setTabOffsetMethod(*position);
    return m_offset.set(component, value, attributes, warnings, context);
}

}  // namespace QtRocket
