#include "QtRocket/file/openrocket/AnglePositionSetter.h"

#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AnglePositionable.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// OpenRocket's warning for an angle it cannot read, with its runs of two blanks.
[[nodiscard]] std::string invalidAngle(std::string_view value, const RocketComponent& component)
{
    return std::format("Warning: invalid angle position. value={}  (degrees)  class: {}", value,
                       DocumentConfig::javaClassName(component));
}

}  // namespace

Result<void> AnglePositionSetter::set(RocketComponent& component, std::string_view value,
                                      const Attributes& attributes, WarningSet& warnings,
                                      const DocumentLoadingContext& /*context*/) const
{
    // findEnum(attributes.get("method"), AngleMethod.class), and RELATIVE for null.
    const std::optional<std::string_view> name = DocumentConfig::attribute(attributes, "method");
    const AngleMethod method = (name.has_value() ? angleMethodFromOrkName(*name) : std::nullopt)
                                   .value_or(AngleMethod::RELATIVE);

    const std::optional<double> degrees = Strings::javaParseDouble(value);
    if (!degrees.has_value())
    {
        warnings.add(invalidAngle(value, component));
        return {};
    }
    const double angle = MathUtil::javaToRadians(*degrees);

    auto* const positionable = dynamic_cast<AnglePositionable*>(&component);
    if (positionable == nullptr)
    {
        warnings.add(
            std::format("Warning: info.openrocket.core.file.openrocket.importt.AnglePositionSetter "
                        "is not valid for class: {}",
                        DocumentConfig::javaClassName(component)));
        return {};
    }

    // Java stores a NaN and an infinity; they are not applied here (decision L3).
    if (!std::isfinite(angle))
    {
        warnings.add(invalidAngle(value, component));
        return {};
    }

    positionable->setAngleMethod(method);
    positionable->setAngleOffset(angle);
    return {};
}

}  // namespace QtRocket
