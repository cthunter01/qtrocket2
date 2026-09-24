#include "QtRocket/rocket/position/AngleMethod.h"

#include <numbers>
#include <optional>
#include <string_view>

#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

double getAngle(AngleMethod method, const RocketComponent*       parentComponent,
                const RocketComponent* /*thisComponent*/, double requestedOffset)
{
    switch (method)
    {
        case AngleMethod::RELATIVE:
            QTROCKET_ASSERT(parentComponent != nullptr);
            return parentComponent->getAngleOffset() + requestedOffset;
        case AngleMethod::FIXED:
            return 0.00;
        case AngleMethod::MIRROR_XY:
        {
            QTROCKET_ASSERT(parentComponent != nullptr);
            double combinedAngle =
                MathUtil::reduce2Pi(parentComponent->getAngleOffset() + requestedOffset);
            if (std::numbers::pi > combinedAngle)
            {
                combinedAngle = -(combinedAngle - std::numbers::pi);
            }
            return combinedAngle;
        }
    }
    return 0.0;
}

std::string_view angleMethodName(AngleMethod method) noexcept
{
    switch (method)
    {
        case AngleMethod::RELATIVE:
            return "RELATIVE";
        case AngleMethod::FIXED:
            return "FIXED";
        case AngleMethod::MIRROR_XY:
            return "MIRROR_XY";
    }
    return "RELATIVE";
}

std::string_view orkName(AngleMethod method) noexcept
{
    switch (method)
    {
        case AngleMethod::RELATIVE:
            return "relative";
        case AngleMethod::FIXED:
            return "fixed";
        case AngleMethod::MIRROR_XY:
            return "mirror_xy";
    }
    return "relative";
}

std::optional<AngleMethod> angleMethodFromOrkName(std::string_view text)
{
    for (const AngleMethod method : kAllAngleMethods)
    {
        if (Strings::orkEnumNameMatches(text, angleMethodName(method)))
        {
            return method;
        }
    }
    return std::nullopt;
}

std::string_view displayKey(AngleMethod method) noexcept
{
    switch (method)
    {
        case AngleMethod::RELATIVE:
            return "RocketComponent.Position.Method.Angle.RELATIVE";
        case AngleMethod::FIXED:
            return "RocketComponent.Position.Method.Angle.FIXED";
        case AngleMethod::MIRROR_XY:
            return "RocketComponent.Position.Method.Angle.MIRROR_XY";
    }
    return "RocketComponent.Position.Method.Angle.RELATIVE";
}

std::string_view displayName(AngleMethod method) noexcept
{
    switch (method)
    {
        case AngleMethod::RELATIVE:
            return "Relative to the parent component";
        case AngleMethod::FIXED:
            return "Angle is fixed.";
        case AngleMethod::MIRROR_XY:
            return "Mirror relative to the rocket's x-y plane";
    }
    return "Relative to the parent component";
}

}  // namespace QtRocket
